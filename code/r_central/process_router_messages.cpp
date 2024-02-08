/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../base/alarms.h"
#include "../base/hw_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_preferences.h"
#include "../common/string_utils.h"
#include "menu/menu.h"
#include "menu/menu_diagnose_radio_link.h"
#include "process_router_messages.h"
#include <pthread.h>
#include "shared_vars.h"
#include "timers.h"
#include "pairing.h"
#include "warnings.h"
#include "ui_alarms.h"
#include "popup_log.h"
#include "link_watch.h"
#include "osd.h"
#include "osd_common.h"
#include "notifications.h"
#include "events.h"
#include "colors.h"
#include "ruby_central.h"
#include "ui_alarms.h"

#define MAX_ROUTER_MESSAGES 80

pthread_t s_pThreadIPC;
pthread_mutex_t s_pThreadIPCMutex;
bool s_bThreadInitOk = false;
u8 s_pMessagesFromRouter[MAX_ROUTER_MESSAGES][MAX_PACKET_TOTAL_SIZE];
int s_MessagesFromRouterSize[MAX_ROUTER_MESSAGES];
int s_iCountMessagesFromRouter = 0;

int s_fIPCToRouter = -1;
int s_fIPCFromRouter = -1;
bool s_bIPCRouterHasReadErrors = false;

u8 s_BufferPipeFromRouter[MAX_PACKET_TOTAL_SIZE];
u8 s_BufferTmpOutputRouterMessage[MAX_PACKET_TOTAL_SIZE];
int s_BufferTmpOutputRouterMessagePos = 0;

char s_BufferVehicleLogFileData[2048];
int s_BufferVehicleLogFilePos = 0;

void * _router_ipc_thread_func(void *ignored_argument);

void start_pipes_to_router()
{
   log_line("[Router COMM] Start pipes to router.");

   s_bThreadInitOk = false;
   s_bIPCRouterHasReadErrors = false;

   if ( -1 == s_fIPCToRouter )
   {
      s_fIPCToRouter = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_CENTRAL_TO_ROUTER);
      if ( s_fIPCToRouter < 0 )
         return;
   }
   else
      log_line("[Router COMM] Already has IPC write endpoint to router.");

   if ( -1 == s_fIPCFromRouter )
   {
      s_fIPCFromRouter = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_CENTRAL);
      if ( s_fIPCFromRouter < 0 )
         return;
   }
   else
      log_line("[Router COMM] Already has IPC read endpoint from router.");

   s_BufferTmpOutputRouterMessagePos = 0;
   
   s_iCountMessagesFromRouter = 0;
   if ( 0 != pthread_mutex_init(&s_pThreadIPCMutex, NULL) )
      log_softerror_and_alarm("[Router COMM] Failed to init mutex for router IPC");
   else if ( 0 != pthread_create(&s_pThreadIPC, NULL, &_router_ipc_thread_func, NULL) )
   {
      log_softerror_and_alarm("[Router COMM] Failed to create thread for router IPC");
      pthread_mutex_destroy(&s_pThreadIPCMutex);
   }
   else
   {
      s_bThreadInitOk = true;
      log_line("[Router COMM] Created IPC receiving thread.");
   }
}

void stop_pipes_to_router()
{
   log_line("[Router COMM] Stopping IPC to/from router.");
   
   if ( s_bThreadInitOk )
   {
      pthread_cancel(s_pThreadIPC);
      pthread_mutex_destroy(&s_pThreadIPCMutex);
      log_line("[Router COMM] Stopped IPC receiving thread and mutex.");
   }
   s_bThreadInitOk = false;

   ruby_close_ipc_channel(s_fIPCFromRouter);
   ruby_close_ipc_channel(s_fIPCToRouter);
   s_fIPCToRouter = -1;
   s_fIPCFromRouter = -1;
   s_bIPCRouterHasReadErrors = false;
   log_line("[Router COMM] Stopped IPC to/from router.");
}

void send_model_changed_message_to_router(u32 uChangeType, u8 uExtraParam)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY | (uChangeType<<8) | (uExtraParam <<16); 
   PH.total_length = sizeof(t_packet_header);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   send_packet_to_router(buffer, PH.total_length);
}

void send_control_message_to_router(u8 packet_type, u32 extraParam)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, packet_type, STREAM_ID_DATA);
   PH.vehicle_id_src = extraParam;
   PH.vehicle_id_dest = extraParam;
   PH.total_length = sizeof(t_packet_header);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   send_packet_to_router(buffer, PH.total_length);
}

void send_control_message_to_router_and_data(u8 packet_type, u8* pData, int nDataLength)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, packet_type, STREAM_ID_DATA);
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + nDataLength;

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   if ( nDataLength > 0 )
      memcpy(&(buffer[sizeof(t_packet_header)]), pData, nDataLength);

   send_packet_to_router(buffer, PH.total_length);
}

void send_packet_to_router(u8* pPacket, int nLength)
{
   if ( (NULL == pPacket) || (nLength <= 0) )
      return;

   if ( -1 == s_fIPCToRouter )
   {
      log_softerror_and_alarm("[Router COMM] No IPC to router to send message to.");
      return; 
   }
   ruby_ipc_channel_send_message(s_fIPCToRouter, pPacket, nLength);
}

t_structure_vehicle_info* _get_runtime_info_for_packet(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*) pPacketBuffer;
   
   // Searching ?
   if ( g_bSearching )
   {
      g_SearchVehicleRuntimeInfo.uVehicleId = pPH->vehicle_id_src;
      g_SearchVehicleRuntimeInfo.pModel = findModelWithId(pPH->vehicle_id_src, 40);
      return &g_SearchVehicleRuntimeInfo;
   }

   // First time pairing?
   if ( (NULL == g_pCurrentModel) || ( ! g_bFirstModelPairingDone ) )
   {
      if ( NULL != g_pCurrentModel )
      {
         g_VehiclesRuntimeInfo[0].uVehicleId = g_pCurrentModel->vehicle_id;
         g_VehiclesRuntimeInfo[0].pModel = g_pCurrentModel;
      }
      return &(g_VehiclesRuntimeInfo[0]);
   }

   // Find it in the expected vehicles list

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == pPH->vehicle_id_src )
         return &(g_VehiclesRuntimeInfo[i]);
   }

   // Unexpected vehicle

   g_UnexpectedVehicleRuntimeInfo.uVehicleId = pPH->vehicle_id_src;
   g_UnexpectedVehicleRuntimeInfo.pModel = findModelWithId(pPH->vehicle_id_src, 41);
   return &(g_UnexpectedVehicleRuntimeInfo);
}

void _process_received_ruby_telemetry_extended(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*) pPacketBuffer;
   t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
   if ( NULL == pRuntimeInfo )
   {
      log_softerror_and_alarm("Process received Ruby telemetry from router: Failed to find vehicle runtime info for vehicle %u. Ignoring this telemetry packet.", pPH->vehicle_id_src);
      return;
   }

   int iTelemetryVersion = 0;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v1) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      iTelemetryVersion = 1;
   else if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v2) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      iTelemetryVersion = 2;         
   else if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      iTelemetryVersion = 3;

   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3)) )
      iTelemetryVersion = 3;
   if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)) )
      iTelemetryVersion = 3;
      
   if ( iTelemetryVersion == 0 )
   {
      log_softerror_and_alarm("Received unknown ruby telemetry version from vehicle id %u", pPH->vehicle_id_src);
      return;
   }

   bool bFirstTimeGotTelemetry = false;
   if ( ! pRuntimeInfo->bGotRubyTelemetryInfo )
   {
      bFirstTimeGotTelemetry = true;
      int iIndexRuntime = -1;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( &(g_VehiclesRuntimeInfo[i]) == pRuntimeInfo )
         {
            iIndexRuntime = i;
            break;
         }
      }
      pRuntimeInfo->bGotRubyTelemetryInfo = true;
      if ( g_bSearching )
         log_line("Start receiving Ruby telemetry (version %d) from router for vehicle id %u, runtime index: search", iTelemetryVersion, pRuntimeInfo->uVehicleId);
      else
         log_line("Start receiving Ruby telemetry (version %d) from router for vehicle id %u, runtime index: %d.", iTelemetryVersion, pRuntimeInfo->uVehicleId, iIndexRuntime);
      log_current_runtime_vehicles_info();
   }

   pRuntimeInfo->uTimeLastRecvRubyTelemetry = g_TimeNow;
   pRuntimeInfo->uTimeLastRecvRubyTelemetryExtended = g_TimeNow;
   pRuntimeInfo->uTimeLastRecvAnyRubyTelemetry = g_TimeNow;
   pRuntimeInfo->tmp_iCountRubyTelemetryPacketsFull++;

   // v1 ruby telemetry
   if ( 1 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v1* pPHRTE = (t_packet_header_ruby_telemetry_extended_v1*)(pPacketBuffer+sizeof(t_packet_header));
   
      radio_populate_ruby_telemetry_v3_from_ruby_telemetry_v1(&(pRuntimeInfo->headerRubyTelemetryExtended), (t_packet_header_ruby_telemetry_extended_v1*)(pPacketBuffer+sizeof(t_packet_header)));
      int dx = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v1);
      int totalLength = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v1);
   
      if ( pPHRTE->flags & FLAG_RUBY_TELEMETRY_HAS_EXTENDED_INFO )
      {
         t_packet_header_ruby_telemetry_extended_extra_info* pPHRTExtraInfo = (t_packet_header_ruby_telemetry_extended_extra_info*)(pPacketBuffer + dx);
         dx += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         totalLength += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         
         if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfo )
            log_line("Start receiving Ruby telemetry extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         pRuntimeInfo->bGotRubyTelemetryExtraInfo = true;
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfo), pPHRTExtraInfo, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
      }

      if ( pPHRTE->extraSize > 0 )
      if ( pPHRTE->extraSize == sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) )
      if ( pPH->total_length == (totalLength + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
          if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions )
            log_line("Start receiving Ruby telemetry retransmissions extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions = true;
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfoRetransmissions), pPacketBuffer + dx, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      }
   }
   // v2 ruby telemetry
   else if ( 2 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v2* pPHRTE = (t_packet_header_ruby_telemetry_extended_v2*)(pPacketBuffer+sizeof(t_packet_header));
   
      radio_populate_ruby_telemetry_v3_from_ruby_telemetry_v2(&(pRuntimeInfo->headerRubyTelemetryExtended), (t_packet_header_ruby_telemetry_extended_v2*)(pPacketBuffer+sizeof(t_packet_header)));
      
      int dx = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v2);
      int totalLength = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v2);
   
      if ( pPHRTE->flags & FLAG_RUBY_TELEMETRY_HAS_EXTENDED_INFO )
      {
         t_packet_header_ruby_telemetry_extended_extra_info* pPHRTExtraInfo = (t_packet_header_ruby_telemetry_extended_extra_info*)(pPacketBuffer + dx);
         dx += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         totalLength += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         
         if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfo )
         {
            pRuntimeInfo->bGotRubyTelemetryExtraInfo = true;
            log_line("Start receiving Ruby telemetry extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
            log_current_runtime_vehicles_info();
         }
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfo), pPHRTExtraInfo, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
      }

      if ( pPHRTE->extraSize > 0 )
      if ( pPHRTE->extraSize == sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) )
      if ( pPH->total_length == (totalLength + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
          if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions )
            log_line("Start receiving Ruby telemetry retransmissions extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions = true;
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfoRetransmissions), pPacketBuffer + dx, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      }
   }
   // v3 ruby telemetry
   else if ( 3 == iTelemetryVersion )
   {
      t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pPacketBuffer+sizeof(t_packet_header));

      memcpy(&(pRuntimeInfo->headerRubyTelemetryExtended), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_ruby_telemetry_extended_v3) );

      int dx = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v3);
      int totalLength = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v3);
   
      if ( pPHRTE->flags & FLAG_RUBY_TELEMETRY_HAS_EXTENDED_INFO )
      {
         t_packet_header_ruby_telemetry_extended_extra_info* pPHRTExtraInfo = (t_packet_header_ruby_telemetry_extended_extra_info*)(pPacketBuffer + dx);
         dx += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         totalLength += sizeof(t_packet_header_ruby_telemetry_extended_extra_info);
         
         if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfo )
         {
            pRuntimeInfo->bGotRubyTelemetryExtraInfo = true;
            log_line("Start receiving Ruby telemetry extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
            log_current_runtime_vehicles_info();
         }
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfo), pPHRTExtraInfo, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
      }

      if ( pPHRTE->extraSize > 0 )
      if ( pPHRTE->extraSize == sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) )
      if ( pPH->total_length == (totalLength + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
          if ( ! pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions )
            log_line("Start receiving Ruby telemetry retransmissions extra info from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         pRuntimeInfo->bGotRubyTelemetryExtraInfoRetransmissions = true;
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtraInfoRetransmissions), pPacketBuffer + dx, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
      }
   }

   if ( bFirstTimeGotTelemetry )
   {
      log_line("Received telemetry has camera flag set? %s",
          (pRuntimeInfo->headerRubyTelemetryExtended.flags & FLAG_RUBY_TELEMETRY_VEHICLE_HAS_CAMERA)?"yes":"no");
      onEventPairingStartReceivingData();
   }
   
   if ( g_bSearching )
   {
      char szFreq1[64];
      char szFreq2[64];
      char szFreq3[64];
      strcpy(szFreq1, str_format_frequency(pRuntimeInfo->headerRubyTelemetryExtended.uRadioFrequenciesKhz[0]) );
      strcpy(szFreq2, str_format_frequency(pRuntimeInfo->headerRubyTelemetryExtended.uRadioFrequenciesKhz[1]) );
      strcpy(szFreq3, str_format_frequency(pRuntimeInfo->headerRubyTelemetryExtended.uRadioFrequenciesKhz[2]) );
      
      static u32 s_uTimeLastLogTelemetrySearch = 0;
      if ( g_TimeNow > s_uTimeLastLogTelemetrySearch + 2000 )
      {
         s_uTimeLastLogTelemetrySearch = g_TimeNow;
         log_line("Received a Ruby telemetry packet while searching: vehicle ID: %u, radio links (%d): %s, %s, %s",
            pRuntimeInfo->headerRubyTelemetryExtended.vehicle_id, pRuntimeInfo->headerRubyTelemetryExtended.radio_links_count,
            szFreq1, szFreq2, szFreq3 );
      }
   }
}

void _process_received_model_settings(u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
      return;
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   t_structure_vehicle_info* pRuntimeInfo = get_vehicle_runtime_info_for_vehicle_id(pPH->vehicle_id_src);
   if ( (NULL == pRuntimeInfo) || (NULL == pRuntimeInfo->pModel) )
      return;
      
   u8* pData = pPacketBuffer + sizeof(t_packet_header);
   int iDataSize = (int)pPH->total_length - sizeof(t_packet_header);
   if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
   {
      u8 size = *(((u8*)pPH) + pPH->total_length-1);
      iDataSize -= size;
   }

   if ( (pRuntimeInfo->pModel->sw_version >> 16) > 79 ) // v 7.7
   {
      pData += 2*sizeof(u32) + sizeof(u8);
      iDataSize -= 2*sizeof(u32) - sizeof(u8);
      log_line("Received model settings packet: vehicle is new mode (7.7)");
   }
   else
      log_line("Received model settings packet: vehicle is in legacy mode (less than 7.7)");

   // Received all settings in a single segment?
   if ( iDataSize > 255 )
   {
      if ( (pRuntimeInfo->pModel->sw_version >> 16) > 79 )
      {
         u32 uTransferId = 0;
         memcpy((u8*)&uTransferId, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         log_line("Received model settings from router in a single packet with full zip model settings from vehicle %u (%d bytes). Transfer id: %u", pPH->vehicle_id_src, iDataSize, uTransferId);
      }
      else
         log_line("Received legacy model settings from router in a single packet with full zip model settings from vehicle %u (%d bytes).", pPH->vehicle_id_src, iDataSize);
      handle_commands_on_full_model_settings_received(pPH->vehicle_id_src, pData, iDataSize);
      return;
   }

   if ( (pRuntimeInfo->pModel->sw_version >> 16) < 79 ) // v 7.7
   {
      u8* pData = pPacketBuffer + sizeof(t_packet_header);
      int iSegmentId = (int)(*pData);
      int iSegment = (int)(*(pData+1));
      int iTotalSegments = *(pData+2);
      int iSegmentSize = *(pData+3);
      log_line("Received message from router (%d bytes, from vehicle id %u) with segment model settings from vehicle (segment id %d, %d of %d, %d bytes).", iDataSize, pPH->vehicle_id_src, iSegmentId, iSegment+1, iTotalSegments, iSegmentSize);
      
      if ( iSegment >= 0 && iSegment < 20 )
      if ( iTotalSegments > 0 && iTotalSegments < 20 )
      if ( iSegmentSize <= iDataSize )
      {
         memcpy( &(pRuntimeInfo->pSegmentsModelSettingsDownload[iSegment][0]), pData+4, iSegmentSize);
         pRuntimeInfo->uSegmentsModelSettingsIds[iSegment] = iSegmentId;
         pRuntimeInfo->uSegmentsModelSettingsSize[iSegment] = iSegmentSize;
         pRuntimeInfo->uSegmentsModelSettingsCount = iTotalSegments;
         log_line("Stored segment %d, %d bytes", iSegment+1, iSegmentSize);
         
         bool bHasAll = true;
         int iTotalSize = 0;
         u8 bufferAll[4096];

         iSegmentId = pRuntimeInfo->uSegmentsModelSettingsIds[0];
            
         for( int i=0; i<iTotalSegments; i++ )
         {
            if ( (int)pRuntimeInfo->uSegmentsModelSettingsIds[i] != iSegmentId )
            {
               bHasAll = false;
               break;
            }
            if ( pRuntimeInfo->uSegmentsModelSettingsSize[i] == 0 )
            {
               bHasAll = false;
               break;
            }
            memcpy(bufferAll + iTotalSize, &(pRuntimeInfo->pSegmentsModelSettingsDownload[i][0]), (int) pRuntimeInfo->uSegmentsModelSettingsSize[i]);
            iTotalSize += (int) pRuntimeInfo->uSegmentsModelSettingsSize[i];
         }
         if ( bHasAll )
         {
            log_line("Got all model settings segments. Total size: %d bytes", iTotalSize);
            handle_commands_on_full_model_settings_received(pPH->vehicle_id_src, bufferAll, iTotalSize);
         }
      }
      return;
   }

   u32 uTransferId = 0;
   u8 uTransferFlags = 0;
   memcpy((u8*)&uTransferId, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
   memcpy((u8*)&uTransferFlags, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32) + sizeof(u8), sizeof(u8));

   int iSegmentIndex = (int)(*pData);
   int iTotalSegments = (int)(*(pData+1));
   int iSegmentSize = (int)(*(pData+2));
   log_line("Received message from router (%d bytes, from vehicle id %u) with model settings segment (%d of %d, %d bytes) for transfer id %u.",
       iDataSize, pPH->vehicle_id_src, iSegmentIndex+1, iTotalSegments, iSegmentSize, uTransferId);

   if ( iSegmentIndex >= 0 && iSegmentIndex < 20 )
   if ( iTotalSegments > 0 && iTotalSegments < 20 )
   if ( iSegmentSize <= iDataSize )
   {
      memcpy( &(pRuntimeInfo->pSegmentsModelSettingsDownload[iSegmentIndex][0]), pData+3, iSegmentSize);
      pRuntimeInfo->uSegmentsModelSettingsIds[iSegmentIndex] = uTransferId;
      pRuntimeInfo->uSegmentsModelSettingsSize[iSegmentIndex] = iSegmentSize;
      pRuntimeInfo->uSegmentsModelSettingsCount = iTotalSegments;
      log_line("Stored segment %d, %d bytes", iSegmentIndex+1, iSegmentSize);
      
      bool bHasAll = true;
      int iTotalSize = 0;
      
      u8 bufferAll[4096];
      char szBufferReceived[256];
      szBufferReceived[0] = 0;

      for( int i=0; i<iTotalSegments; i++ )
      {
         if ( pRuntimeInfo->uSegmentsModelSettingsIds[i] != uTransferId )
         {
            bHasAll = false;
            continue;
         }
         if ( pRuntimeInfo->uSegmentsModelSettingsSize[i] == 0 )
         {
            bHasAll = false;
            continue;
         }
         char szTmp[32];
         sprintf(szTmp, " %d", i+1);
         strcat(szBufferReceived, szTmp);
         memcpy(bufferAll + iTotalSize, &(pRuntimeInfo->pSegmentsModelSettingsDownload[i][0]), (int) pRuntimeInfo->uSegmentsModelSettingsSize[i]);
         iTotalSize += (int) pRuntimeInfo->uSegmentsModelSettingsSize[i];
      }
      if ( bHasAll )
      {
         log_line("Got all model settings segments. Total size: %d bytes. Process it.", iTotalSize);
         handle_commands_on_full_model_settings_received(pPH->vehicle_id_src, bufferAll, iTotalSize);
      }
      else
         log_line("Still has not received all small segments. Received segments so far: [%s]", szBufferReceived);
   }
}

int _process_received_message_from_router(u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
      return -1;
   t_packet_header* pPH = (t_packet_header*) pPacketBuffer;
   
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_LOCAL_CONTROL )
   if ( (0 == pPH->vehicle_id_src) || (MAX_U32 == pPH->vehicle_id_src) )
      log_softerror_and_alarm("Received invalid radio packet: Invalid source vehicle id: %u (vehicle id dest: %u, packet type: %s)",
         pPH->vehicle_id_src, pPH->vehicle_id_dest, str_get_packet_type(pPH->packet_type));
   
   // Do not process all telemetry packets while searching. Only required ones.

   if ( g_bSearching )
   if ( (pPH->packet_type != PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) &&
        (pPH->packet_type != PACKET_TYPE_RUBY_TELEMETRY_SHORT) &&
        (pPH->packet_type != PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY) ) 
      return 0;


   if ( pPH->packet_type == PACKET_TYPE_FIRST_PAIRING_DONE )
   {
      log_line("Received notification from router that first pairing was done.");
      log_line("Current local model VID %u, ptr: %X (before updating local copy)", g_pCurrentModel->vehicle_id, g_pCurrentModel);
      loadAllModels();
      g_pCurrentModel = getCurrentModel();
      g_pCurrentModel->b_mustSyncFromVehicle = true;
      ruby_set_active_model_id(g_pCurrentModel->vehicle_id);
      g_bFirstModelPairingDone = true;
      g_bSyncModelSettingsOnLinkRecover = true;
      log_line("Updated current local model VID %u, ptr: %X", g_pCurrentModel->vehicle_id, g_pCurrentModel);
      g_VehiclesRuntimeInfo[0].uVehicleId = g_pCurrentModel->vehicle_id;
      g_VehiclesRuntimeInfo[0].pModel = g_pCurrentModel;
      log_line("Updated runtime info index 0");
      warnings_add(0, "First pairing complete.");
      onModelAdded(g_pCurrentModel->vehicle_id);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY )
   {
      log_line("Received message that router is ready, in working state.");
      
      pairing_on_router_ready();
      notification_remove_start_pairing();
      g_bIsRouterReady = true;
      g_RouterIsReadyTimestamp = g_TimeNow;
      return 0;
   }


   if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION )
   {
      u32 uResendCount = 0;
      if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(u32) )
         memcpy(&uResendCount, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));

      log_line("Received pairing confirmation from router (received vehicle resend counter: %u). VID: %u, CID: %u", uResendCount, pPH->vehicle_id_src, pPH->vehicle_id_dest);
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
      if ( NULL == pRuntimeInfo )
         log_softerror_and_alarm("Failed to create a vehicle runtime info for vehicle id %u", pPH->vehicle_id_src);
      else
      {
         pRuntimeInfo->bPairedConfirmed = true;
         log_line("Pairing confirmed for vehicle VID %u", pRuntimeInfo->uVehicleId);
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_SWITCH_FAVORIVE_VEHICLE )
   {
      log_line("Received message from router that favorite vehicle was switched to VID: %u", pPH->vehicle_id_dest);
      g_bSwitchingFavoriteVehicle = false;

      Model* pTmp = findModelWithId(pPH->vehicle_id_dest, 42);
      if ( NULL == pTmp )
      {
         warnings_add(0, "Failed to switch favorite vehicle");
         return 0;
      }
      char szBuff[256];
      sprintf(szBuff, "Switched to vehicle: %s", pTmp->getLongName());
      warnings_replace("Switching to favorite", szBuff);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_TEST_RADIO_LINK )
   {

      int iRadioLinkId = pPacketBuffer[sizeof(t_packet_header)];
      int iCmdId = pPacketBuffer[sizeof(t_packet_header)+1];
      int iMsgLen = pPH->total_length - sizeof(t_packet_header)-2*sizeof(u8);
      log_line("Processing received test link message type %s from router, %d data bytes for radio link %d",
      str_get_packet_test_link_command(iCmdId), iMsgLen, iRadioLinkId+1);

      if ( iCmdId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_STATUS )
      {
         char szBuff[256];
         strcpy(szBuff, (char*)(pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u8)));
         warnings_add_configuring_radio_link_line(szBuff);
      }

      if ( iCmdId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END )
      {
         bool bSucceeded = false;
         if ( pPacketBuffer[sizeof(t_packet_header)+2*sizeof(u8)] )
            bSucceeded = true;
         log_line("Radio link params update succeeded? %s", bSucceeded?"Yes":"No");

         warnings_remove_configuring_radio_link(bSucceeded);
         link_reset_reconfiguring_radiolink();

         if ( bSucceeded )
         {
            reloadCurrentModel();
            g_pCurrentModel = getCurrentModel();
         }
         else
         {
            Popup* p = new Popup("Failed to set the new parameters.",0.25,0.44, 0.5, 4);
            p->setIconId(g_idIconError, get_Color_IconError());
            popups_add_topmost(p);
         }
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_SIK_CONFIG )
   {
       u8 uCommandId = *(pPacketBuffer + sizeof(t_packet_header) + sizeof(u8));
       if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
          log_line("Received from router local response to SiK config command %d", (int)uCommandId);
       else
          log_line("Received from router vehicle response to SiK config command %d", (int)uCommandId);
       MenuDiagnoseRadioLink* pMenu = (MenuDiagnoseRadioLink*) menu_get_menu_by_id(MENU_ID_DIAGNOSE_RADIO_LINK);
       if ( NULL != pMenu )
       {
          u8* pBuffer = pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u8);
          if ( 0 == uCommandId )
          {
             if ( pPH->vehicle_id_src == g_uControllerId )
                pMenu->onReceivedControllerData(pBuffer, pPH->total_length - sizeof(t_packet_header) - 2 * sizeof(u8));
             else
                pMenu->onReceivedVehicleData(pBuffer, pPH->total_length - sizeof(t_packet_header) - 2 * sizeof(u8));
          }
       }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_SWITCH_RADIO_LINK )
   {
      int iLink = (int)pPH->vehicle_id_src;
      int iSucceeded = (int) pPH->vehicle_id_dest;
      u32 uFreqKhz = 0;
      if ( NULL != g_pCurrentModel )
         uFreqKhz = g_pCurrentModel->radioLinksParams.link_frequency_khz[iLink];
      g_bSwitchingRadioLink = false;

      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)&g_SM_RadioStats, (u8*)g_pSM_RadioStats, sizeof(shared_mem_radio_stats));

      log_line("Received response from router to switch to vehicle radio link %d: succeeded: %d", iLink+1, iSucceeded);
      warnings_remove_switching_radio_link(iLink, uFreqKhz, (bool) iSucceeded);
      menu_refresh_all_menus();
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_SHORT )
   {
      if ( g_bSearching )
         log_line("Received a short Ruby telemetry packet while searching, from vehicle id: %u", pPH->vehicle_id_src);

      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
      if ( pPH->total_length != (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_ruby_telemetry_short) )
      {
         log_softerror_and_alarm("Received invalid short telemetry packet from vehicle id %u. Received invalid size: %d bytes, expected %d bytes",
            pPH->vehicle_id_src, pPH->total_length, sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_short) );
         return 0;
      }
      
      t_packet_header_ruby_telemetry_short* pPHRTS = (t_packet_header_ruby_telemetry_short*)(pPacketBuffer+sizeof(t_packet_header));
      
      if ( ! pRuntimeInfo->bGotFCTelemetry )
      {
         pRuntimeInfo->bGotFCTelemetry = true;
         log_line("Start receiving short FC telemetry from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         log_current_runtime_vehicles_info();
      }
      if ( (! pRuntimeInfo->bGotRubyTelemetryInfo) || ( 0 == pRuntimeInfo->uTimeLastRecvRubyTelemetryShort ) )
      {
         pRuntimeInfo->bGotRubyTelemetryInfo = true;
         log_line("Start receiving short Ruby telemetry from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         log_current_runtime_vehicles_info();
         onEventPairingStartReceivingData();
      }
      pRuntimeInfo->bGotFCTelemetryShort = true;
      pRuntimeInfo->bGotRubyTelemetryInfo = true;
      pRuntimeInfo->bGotRubyTelemetryInfoShort = true;
      pRuntimeInfo->uTimeLastRecvFCTelemetry = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvFCTelemetryShort = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvRubyTelemetry = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvRubyTelemetryShort = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvAnyRubyTelemetry = g_TimeNow;

      pRuntimeInfo->tmp_iCountRubyTelemetryPacketsShort++;
      pRuntimeInfo->tmp_iCountFCTelemetryPacketsShort++;

      t_packet_header_fc_telemetry PHFCT;
      if ( pRuntimeInfo->bGotFCTelemetry )
         memcpy((u8*)&PHFCT, &(pRuntimeInfo->headerFCTelemetry), sizeof(t_packet_header_fc_telemetry));
      else
         memset((u8*)&PHFCT, 0, sizeof(t_packet_header_fc_telemetry));

      PHFCT.flight_mode = pPHRTS->flight_mode;
      PHFCT.throttle = pPHRTS->throttle;
      PHFCT.voltage = pPHRTS->voltage; // 1/1000 volts
      PHFCT.current = pPHRTS->current; // 1/1000 amps
      PHFCT.altitude = pPHRTS->altitude; // 1/100 meters  -1000 m
      PHFCT.altitude_abs = pPHRTS->altitude_abs; // 1/100 meters -1000 m
      PHFCT.distance = pPHRTS->distance; // 1/100 meters
      PHFCT.heading = pPHRTS->heading;
      
      PHFCT.vspeed = pPHRTS->vspeed; // 1/100 meters -1000 m
      PHFCT.aspeed = pPHRTS->aspeed; // airspeed (1/100 meters - 1000 m)
      PHFCT.hspeed = pPHRTS->hspeed; // 1/100 meters -1000 m
      
      memcpy(&(pRuntimeInfo->headerFCTelemetry), (u8*)&PHFCT, sizeof(t_packet_header_fc_telemetry) );
      
      t_packet_header_ruby_telemetry_extended_v3 PHRTE;
      if ( pRuntimeInfo->bGotRubyTelemetryInfo )
         memcpy((u8*)&PHRTE, &(pRuntimeInfo->headerRubyTelemetryExtended), sizeof(t_packet_header_ruby_telemetry_extended_v3));
      else
         memset((u8*)&PHRTE, 0, sizeof(t_packet_header_ruby_telemetry_extended_v3));

      PHRTE.vehicle_id = pPH->vehicle_id_src;
      PHRTE.version = pPHRTS->version;
      PHRTE.radio_links_count = pPHRTS->radio_links_count;
      if ( PHRTE.radio_links_count > 3 )
         PHRTE.radio_links_count = 3;
      for( int i=0; i<PHRTE.radio_links_count; i++ )
         PHRTE.uRadioFrequenciesKhz[i] = pPHRTS->uRadioFrequenciesKhz[i];

      memcpy(&(pRuntimeInfo->headerRubyTelemetryExtended), (u8*)&PHRTE, sizeof(t_packet_header_ruby_telemetry_extended_v3) );
      
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
   {
      _process_received_ruby_telemetry_extended(pPacketBuffer);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_FC_TELEMETRY )
   if ( pPH->total_length == (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry) )
   if ( (! osd_is_debug()) && (!g_bSearching) )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);

      if ( ! pRuntimeInfo->bGotFCTelemetry )
      {
         pRuntimeInfo->bGotFCTelemetry = true;
         log_line("Start receiving FC telemetry from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         log_current_runtime_vehicles_info();
      }
      pRuntimeInfo->uTimeLastRecvFCTelemetry = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvFCTelemetryFull = g_TimeNow;
      pRuntimeInfo->tmp_iCountFCTelemetryPacketsFull++;
      memcpy(&(pRuntimeInfo->headerFCTelemetry), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_fc_telemetry) );
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_FC_TELEMETRY_EXTENDED )
   if ( pPH->total_length == (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry) + (u16)sizeof(t_packet_header_fc_extra) )
   if ( (! osd_is_debug()) && (!g_bSearching) )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);

      if ( ! pRuntimeInfo->bGotFCTelemetry )
      {
         pRuntimeInfo->bGotFCTelemetry = true;
         log_line("Start receiving FC telemetry extended from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
         log_current_runtime_vehicles_info();
      }
      pRuntimeInfo->uTimeLastRecvFCTelemetry = g_TimeNow;
      pRuntimeInfo->uTimeLastRecvFCTelemetryFull = g_TimeNow;
      pRuntimeInfo->tmp_iCountFCTelemetryPacketsFull++;      
      memcpy(&(pRuntimeInfo->headerFCTelemetry), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_fc_telemetry) );
      
      if ( pRuntimeInfo->headerFCTelemetry.flags & FC_TELE_FLAGS_HAS_MESSAGE )
      {
         memcpy(&(pRuntimeInfo->headerFCTelemetryExtra), pPacketBuffer+sizeof(t_packet_header) + sizeof(t_packet_header_fc_telemetry), sizeof(t_packet_header_fc_extra) );
         pRuntimeInfo->bGotFCTelemetryExtra = true;

         if ( (0 == strncmp(pRuntimeInfo->szLastMessageFromFC, (const char*)(pRuntimeInfo->headerFCTelemetryExtra.text), FC_MESSAGE_MAX_LENGTH-1)) &&
              (g_TimeNow < pRuntimeInfo->uTimeLastMessageFromFC+5000) )
            return 0;

         strcpy(pRuntimeInfo->szLastMessageFromFC, (const char*)(pRuntimeInfo->headerFCTelemetryExtra.text));
         char szBuff[512];
         if ( (pRuntimeInfo->szLastMessageFromFC[0] == 'R') && (pRuntimeInfo->szLastMessageFromFC[1] == ':') )
         {
            strcpy(szBuff, "Ruby: ");
            strcat(szBuff, &(pRuntimeInfo->szLastMessageFromFC[0]));
         }
         else
         {
            strcpy(szBuff, "FC: ");
            strcat(szBuff, pRuntimeInfo->szLastMessageFromFC);
         }
         if ( pRuntimeInfo->pModel != NULL )
         if ( ! (pRuntimeInfo->pModel->osd_params.osd_preferences[osd_get_current_layout_index()] & OSD_PREFERENCES_BIT_FLAG_DONT_SHOW_FC_MESSAGES) )
         if ( ! (pRuntimeInfo->pModel->telemetry_params.flags & TELEMETRY_FLAGS_DONT_SHOW_FC_MESSAGES) )
            warnings_add(pPH->vehicle_id_src, szBuff, g_idIconInfo, get_Color_IconNormal(), 8);
         pRuntimeInfo->uTimeLastMessageFromFC = g_TimeNow;
      }
      return 0;    
   }

   if ( pPH->packet_type == PACKET_TYPE_FC_RC_CHANNELS )
   if ( pPH->total_length == (u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_fc_rc_channels) )
   if ( (!g_bSearching) && g_bFirstModelPairingDone )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);

      memcpy(&(pRuntimeInfo->headerFCTelemetryRCChannels), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_fc_rc_channels) );
      return 0;
   }


   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS )
   {
      if ( pPH->total_length != sizeof(t_packet_header) + 2*sizeof(shared_mem_video_info_stats) )
         return 0;
      memcpy((u8*)&g_VideoInfoStatsFromVehicle, (u8*)(pPacketBuffer + sizeof(t_packet_header)), sizeof(shared_mem_video_info_stats));
      memcpy((u8*)&g_VideoInfoStatsFromVehicleRadioOut, (u8*)(pPacketBuffer + sizeof(t_packet_header) + sizeof(shared_mem_video_info_stats)), sizeof(shared_mem_video_info_stats));
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY )
   {
      if ( pPH->total_length != sizeof(t_packet_header) + sizeof(shared_mem_dev_video_bitrate_history) )
         return 0;
      memcpy((u8*)&g_SM_DevVideoBitrateHistory, (u8*)(pPacketBuffer + sizeof(t_packet_header)), sizeof(shared_mem_dev_video_bitrate_history));
      g_bGotStatsVideoBitrate = true;
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY )
   {
      if ( pPH->total_length != sizeof(t_packet_header) + sizeof(t_packet_header_vehicle_tx_history) )
         return 0;
      memcpy((u8*)&g_PHVehicleTxHistory, (u8*)(pPacketBuffer + sizeof(t_packet_header)), sizeof(t_packet_header_vehicle_tx_history));
      g_bGotStatsVehicleTx = true;
      return 0;
   }


   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY )
   {
      if ( pPH->total_length != sizeof(t_packet_header) + sizeof(u32) + sizeof(shared_mem_radio_stats_interface_rx_hist) )
         return 0;

      u32 uInt = 0;
      memcpy((u8*)&uInt, (u8*)(pPacketBuffer + sizeof(t_packet_header)), sizeof(u32));
      memcpy((u8*)&(g_SM_HistoryRxStatsVehicle.interfaces_history[uInt]), (u8*)(pPacketBuffer + sizeof(t_packet_header) + sizeof(u32)), sizeof(shared_mem_radio_stats_interface_rx_hist));
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLLER_RADIO_INTERFACE_FAILED_TO_INITIALIZE )
   {
      int iRadioInterface = pPH->vehicle_id_dest;
      char szBuff[256];
      
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(iRadioInterface);
      if ( NULL == pNICInfo )
         sprintf(szBuff, "Radio Interface %d failed to initialize!", iRadioInterface+1);
      else
      {
         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
         if ( NULL != pCardInfo )
            sprintf(szBuff, "Radio Interface %d (%s) failed to initialize!", iRadioInterface+1, str_get_radio_card_model_string(pCardInfo->cardModel));
         else
            sprintf(szBuff, "Radio Interface %d (%s) failed to initialize!", iRadioInterface+1, "Unknown Type");
      }
      warnings_add(pPH->vehicle_id_src, szBuff);

      Popup* p = new Popup(szBuff, 0.5, 0.5, 0.6, 10.0);
      p->setCentered();
      p->setFont(g_idFontOSD);
      p->setIconId(g_idIconWarning, get_Color_IconWarning());
      popups_add_topmost(p);

      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);
      if ( NULL == pRuntimeInfo )
         return 0;

      u8 countCards = pPacketBuffer[sizeof(t_packet_header)];

      // Can have either number of cards * rx stats for each card
      // Or index of card and rx stats for that card

      // Full rx stats for all cards in a single packet

      if ( (countCards != 0 ) && (pPH->total_length == (sizeof(t_packet_header) + sizeof(u8) + countCards * sizeof(shared_mem_radio_stats_radio_interface)) ) )
      {
         memcpy((u8*)&(pRuntimeInfo->SMVehicleRxStats[0]), (u8*)(pPacketBuffer + sizeof(t_packet_header) + sizeof(u8)), countCards*sizeof(shared_mem_radio_stats_radio_interface));
         pRuntimeInfo->uTimeLastRecvVehicleRxStats = g_TimeNow;
         pRuntimeInfo->bGotStatsVehicleRxCards = true;
      }

      // Rx stats for a single card
      if ( (NULL != g_pCurrentModel) && (countCards < g_pCurrentModel->radioInterfacesParams.interfaces_count) && (pPH->total_length == (sizeof(t_packet_header) + sizeof(u8) + sizeof(shared_mem_radio_stats_radio_interface_compact)) ) )
      if ( (NULL != pRuntimeInfo) && (pRuntimeInfo->uTimeLastRecvVehicleRxStats < g_TimeNow-1000 ) )
      if ( pPH->total_length == (sizeof(t_packet_header) + sizeof(u8) + sizeof(shared_mem_radio_stats_radio_interface_compact)) )
      {
         shared_mem_radio_stats_radio_interface_compact statsCompact;
         memcpy((u8*)&statsCompact, (u8*)(pPacketBuffer + sizeof(t_packet_header) + sizeof(u8)), sizeof(shared_mem_radio_stats_radio_interface_compact));
         
         pRuntimeInfo->SMVehicleRxStats[countCards].lastDbm = statsCompact.lastDbm;
         pRuntimeInfo->SMVehicleRxStats[countCards].lastDbmVideo = statsCompact.lastDbmVideo;
         pRuntimeInfo->SMVehicleRxStats[countCards].lastDbmData = statsCompact.lastDbmData;
         pRuntimeInfo->SMVehicleRxStats[countCards].lastRecvDataRate = statsCompact.lastRecvDataRate;
         pRuntimeInfo->SMVehicleRxStats[countCards].lastRecvDataRateVideo = statsCompact.lastRecvDataRateVideo;
         pRuntimeInfo->SMVehicleRxStats[countCards].lastRecvDataRateData = statsCompact.lastRecvDataRateData;

         pRuntimeInfo->SMVehicleRxStats[countCards].totalRxBytes = statsCompact.totalRxBytes;
         pRuntimeInfo->SMVehicleRxStats[countCards].totalTxBytes = statsCompact.totalTxBytes;
         pRuntimeInfo->SMVehicleRxStats[countCards].rxBytesPerSec = statsCompact.rxBytesPerSec;
         pRuntimeInfo->SMVehicleRxStats[countCards].txBytesPerSec = statsCompact.txBytesPerSec;
         pRuntimeInfo->SMVehicleRxStats[countCards].totalRxPackets = statsCompact.totalRxPackets;
         pRuntimeInfo->SMVehicleRxStats[countCards].totalRxPacketsBad = statsCompact.totalRxPacketsBad;
         pRuntimeInfo->SMVehicleRxStats[countCards].totalRxPacketsLost = statsCompact.totalRxPacketsLost;
         pRuntimeInfo->SMVehicleRxStats[countCards].totalTxPackets = statsCompact.totalTxPackets;
         pRuntimeInfo->SMVehicleRxStats[countCards].rxPacketsPerSec = statsCompact.rxPacketsPerSec;
         pRuntimeInfo->SMVehicleRxStats[countCards].txPacketsPerSec = statsCompact.txPacketsPerSec;
         pRuntimeInfo->SMVehicleRxStats[countCards].timeLastRxPacket = statsCompact.timeLastRxPacket;
         pRuntimeInfo->SMVehicleRxStats[countCards].timeLastTxPacket = statsCompact.timeLastTxPacket;

         pRuntimeInfo->SMVehicleRxStats[countCards].rxQuality = statsCompact.rxQuality;
         pRuntimeInfo->SMVehicleRxStats[countCards].rxRelativeQuality = statsCompact.rxRelativeQuality;

         memcpy(pRuntimeInfo->SMVehicleRxStats[countCards].hist_rxPacketsCount, statsCompact.hist_rxPacketsCount, MAX_HISTORY_RADIO_STATS_RECV_SLICES * sizeof(u8));
         memcpy(pRuntimeInfo->SMVehicleRxStats[countCards].hist_rxPacketsLostCount, statsCompact.hist_rxPacketsLostCount, MAX_HISTORY_RADIO_STATS_RECV_SLICES * sizeof(u8));
         memcpy(pRuntimeInfo->SMVehicleRxStats[countCards].hist_rxGapMiliseconds, statsCompact.hist_rxGapMiliseconds, MAX_HISTORY_RADIO_STATS_RECV_SLICES * sizeof(u8));
         memset(pRuntimeInfo->SMVehicleRxStats[countCards].hist_rxPacketsBadCount, 0, MAX_HISTORY_RADIO_STATS_RECV_SLICES*sizeof(u8));
         
         pRuntimeInfo->bGotStatsVehicleRxCards = true;      
      }
      return 0;
   }


   if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD )
   {
      if ( g_bOSDPluginsNeedTelemetryStreams )
      {
         t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
         int len = pPH->total_length - sizeof(t_packet_header)-sizeof(t_packet_header_telemetry_raw);
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
         {
            u8 size = *(((u8*)pPH) + pPH->total_length-1);
            len -= size;
         }
         u8* pTelemetryData = pPacketBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw);

         for( int i=0; i<g_iPluginsOSDCount; i++ )
         {
            if ( NULL == g_pPluginsOSD[i]->pFunctionRequestTelemetryStreams ||
                 NULL == g_pPluginsOSD[i]->pFunctionOnTelemetryStreamData ||
                 NULL == g_pCurrentModel )
               continue;

            int iRes = (*(g_pPluginsOSD[i]->pFunctionRequestTelemetryStreams))();
            if ( iRes )
               (*(g_pPluginsOSD[i]->pFunctionOnTelemetryStreamData))(pTelemetryData, len, g_pCurrentModel->telemetry_params.fc_telemetry_type);
         }
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_COMMAND_RESPONSE )
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   if ( g_bFirstModelPairingDone )
   {
      handle_commands_on_response_received(pPacketBuffer, pPH->total_length);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED )
   {
      warnings_add_radio_reinitialized();
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_ALARM )
   {
      u32 uAlarmIndex = 0;
      u32 uAlarm = 0;
      u32 uFlags1 = 0;
      u32 uFlags2 = 0;

      // Old format ?
      if ( pPH->total_length == (int)(sizeof(t_packet_header) + 3 * sizeof(u32)) )
      {
         memcpy(&uAlarm, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy(&uFlags1, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         memcpy(&uFlags2, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
      }

      // New format, version 7.5
      else if ( pPH->total_length == (int)(sizeof(t_packet_header) + 4 * sizeof(u32)) )
      {
         memcpy(&uAlarmIndex, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy(&uAlarm, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         memcpy(&uFlags1, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
         memcpy(&uFlags2, pPacketBuffer + sizeof(t_packet_header) + 3*sizeof(u32), sizeof(u32));
      }
      else
      {
         log_softerror_and_alarm("Received invalid alarm from router. Received %d bytes, expected %d bytes", pPH->total_length, (int)sizeof(t_packet_header) + 4 * (int)sizeof(u32));
         return 0;
      }

      char szBuff[256];
      alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);

      // Alarm generated by the controller ?
      bool bLocalAlarm = false;
      if ( (pPH->vehicle_id_src == 0) || (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         bLocalAlarm = true;
      if ( bLocalAlarm )
      {
         log_line("Received local alarm: %s, alarm index: %u", szBuff, uAlarmIndex);
         alarms_add_from_local(uAlarm, uFlags1, uFlags2);
      }
      else
      {
         log_line("Received vehicle alarm: %s, alarm index: %u", szBuff, uAlarmIndex);

         if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_LOST )
            g_bIsVehicleLinkToControllerLost = true;
         if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
            g_bIsVehicleLinkToControllerLost = false;

         if ( (uAlarm & ALARM_ID_LINK_TO_CONTROLLER_LOST) || (uAlarm & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED) )
         {
            Preferences* pP = get_Preferences();
            if ( ! (pP->uEnabledAlarms & ALARM_ID_LINK_TO_CONTROLLER_LOST) )
            {
               log_line("This alarm (controller link lost/recovered) is disabled. Do not show it in UI.");
               return 0;
            }

            if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_LOST )
               warnings_add_link_to_controller_lost(pPH->vehicle_id_src);
            else if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
               warnings_add_link_to_controller_recovered(pPH->vehicle_id_src);
         }
         else
            alarms_add_from_vehicle(pPH->vehicle_id_src, uAlarm, uFlags1, uFlags2);
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_MODEL_SETTINGS )
   if ( g_bFirstModelPairingDone )
   {
      int iDataSize = (int)pPH->total_length - sizeof(t_packet_header);
      if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
      {
         u8 size = *(((u8*)pPH) + pPH->total_length-1);
         iDataSize -= size;
      }

      t_structure_vehicle_info* pRuntimeInfo = get_vehicle_runtime_info_for_vehicle_id(pPH->vehicle_id_src);
      if ( NULL == pRuntimeInfo )
      {
         log_error_and_alarm("Received model settings packet (%d bytes) but there is no runtime info for VID %u. Ignoring it.",
            iDataSize, pPH->vehicle_id_src);
         return 0;
      }
      if ( NULL == pRuntimeInfo->pModel )
      {
         log_error_and_alarm("Received model settings packet (%d bytes) but there is no model in runtime info for VID %u. Ignoring it.",
            iDataSize, pPH->vehicle_id_src);
         return 0;
      }

      _process_received_model_settings(pPacketBuffer);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_RECEIVED_VEHICLE_LOG_SEGMENT )
   if ( g_bFirstModelPairingDone )
   {
      log_line("Received a vehicle live log file segment");

      if ( NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) )
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG )
      {
         t_packet_header_file_segment* pPHFS = (t_packet_header_file_segment*) (pPacketBuffer + sizeof(t_packet_header));
         int length = pPHFS->segment_size;
         u8* pData = pPacketBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_file_segment);
         memcpy((&(s_BufferVehicleLogFileData[0])) + s_BufferVehicleLogFilePos, pData, length);
         s_BufferVehicleLogFilePos += length;
         int parsePos = 0;
         while ( parsePos < s_BufferVehicleLogFilePos )
         {
            if ( s_BufferVehicleLogFileData[parsePos] == 10 || s_BufferVehicleLogFileData[parsePos] == 13 )
            {
                s_BufferVehicleLogFileData[parsePos] = 0;
                if ( 0 < parsePos )
                   popup_log_add_vehicle_entry(s_BufferVehicleLogFileData);

                for( int k=parsePos+1; k<s_BufferVehicleLogFilePos; k++ )
                    s_BufferVehicleLogFileData[k-parsePos-1] = s_BufferVehicleLogFileData[k];
                s_BufferVehicleLogFilePos -= parsePos+1;
                if ( s_BufferVehicleLogFilePos < 0 )
                   s_BufferVehicleLogFilePos = 0;
                parsePos = 0;
             }
            parsePos++;
         }
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED )
   {
      u32* pI = (u32*)(pPacketBuffer+sizeof(t_packet_header));
      u32 uLinkId = *pI;
      pI++;
      u32 uNewFreq = *pI;
      log_line("Received new model link frequency from router (link %u new frequency: %s). Updating local model copy.", uLinkId+1, str_format_frequency(uNewFreq));
      if ( (int)uLinkId < g_pCurrentModel->radioLinksParams.links_count )
      {
         g_pCurrentModel->radioLinksParams.link_frequency_khz[uLinkId] = uNewFreq;
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkId )
               g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i] = uNewFreq;
         }
         notification_add_frequency_changed((int)uLinkId, uNewFreq);
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED )
   {
      log_line("Received current radio configuration from vehicle uid %u, packet size: %d bytes.", pPH->vehicle_id_src, pPH->total_length);
      if ( NULL == g_pCurrentModel )
         return 0;
      if ( pPH->total_length != (sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters)) )
      {
         log_softerror_and_alarm("Received current radio configuration: invalid packet size. Ignoring.");
         return 0;
      }
      bool bChanged = false;
      if ( 0 != memcmp(&(g_pCurrentModel->relay_params), pPacketBuffer + sizeof(t_packet_header), sizeof(type_relay_parameters)) )
         bChanged = true;
      if ( 0 != memcmp(&(g_pCurrentModel->radioInterfacesParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters)) )
         bChanged = true;
      if ( 0 != memcmp(&(g_pCurrentModel->radioLinksParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters)) )
         bChanged = true;
     
      if ( bChanged )
      {
         log_line("Radio configuration has changed on the vehicle.");
         memcpy(&(g_pCurrentModel->relay_params), pPacketBuffer + sizeof(t_packet_header), sizeof(type_relay_parameters));
         memcpy(&(g_pCurrentModel->radioInterfacesParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters));
         memcpy(&(g_pCurrentModel->radioLinksParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters));
         
         warnings_add(pPH->vehicle_id_src, "Radio configuration has changed on the vehicle. Updating controller radio configuration.", g_idIconRadio);
         hardware_load_radio_info();
      }
      else
         log_line("Received new radio configuration from vehicle is the same as old one, nothing to do, ignoring it.");
   }
   return 0;
}


// Returns number of messages received

int try_read_messages_from_router(u32 uMaxMiliseconds)
{
   int timeoutReadPipeMicroseconds = 1000*uMaxMiliseconds;
   u32 uTimeStart = get_current_timestamp_ms();
   if ( -1 == s_fIPCFromRouter )
   {
       hardware_sleep_ms(uMaxMiliseconds/2+1);
       return 0;
   }

   if ( s_bIPCRouterHasReadErrors )
   {
      log_line("Too many read errors on pipe from router. Reinit pipes.");
      stop_pipes_to_router();
      start_pipes_to_router();
   }

   u8 uTmpMsg[MAX_PACKET_TOTAL_SIZE];

   int iCountMessagesProcessed = 0;
   while(true)
   {
      u8* pResult = NULL;
      if ( s_bThreadInitOk )
      {
         pthread_mutex_lock(&s_pThreadIPCMutex);
         int iTmpCount = s_iCountMessagesFromRouter;
         if ( 0 == iTmpCount )
         {
            pthread_mutex_unlock(&s_pThreadIPCMutex);
            pResult = NULL;
            hardware_sleep_ms(uMaxMiliseconds/4+1);
         }
         else
         {
            memcpy(&(uTmpMsg[0]), &(s_pMessagesFromRouter[0][0]), s_MessagesFromRouterSize[0]);
            pResult = &uTmpMsg[0];
            s_iCountMessagesFromRouter--;
            for( int i=0; i<s_iCountMessagesFromRouter; i++ )
            {
               s_MessagesFromRouterSize[i] = s_MessagesFromRouterSize[i+1];
               memcpy(&(s_pMessagesFromRouter[i][0]), &(s_pMessagesFromRouter[i+1][0]), s_MessagesFromRouterSize[i]);
            }
            pthread_mutex_unlock(&s_pThreadIPCMutex);
            hardware_sleep_micros(500);
         }        
      }
      else
         pResult = ruby_ipc_try_read_message(s_fIPCFromRouter, timeoutReadPipeMicroseconds, s_BufferTmpOutputRouterMessage, &s_BufferTmpOutputRouterMessagePos, s_BufferPipeFromRouter);
      
      if ( NULL != pResult )
      {
         iCountMessagesProcessed++;
         t_packet_header* pPH = (t_packet_header*) pResult;
         if ( ! radio_packet_check_crc(pResult, pPH->total_length) )
             log_softerror_and_alarm("[Router COMM] Received invalid message (invalid CRC) from router. Ignoring it.");
         else
             _process_received_message_from_router(pResult);

         if ( iCountMessagesProcessed > 15 )
         {
            log_softerror_and_alarm("Processing too many messages from router.");
            return iCountMessagesProcessed;
         }
      }

      u32 uTimeNow = get_current_timestamp_ms();
      if ( uTimeNow >= uTimeStart + uMaxMiliseconds )
      {
         return iCountMessagesProcessed;
      }
      timeoutReadPipeMicroseconds = 1000 * (uTimeStart + uMaxMiliseconds - uTimeNow);
   }
   return iCountMessagesProcessed;
}

void * _router_ipc_thread_func(void *ignored_argument)
{
   while ( true )
   {
      u8* pResult = NULL;
      if ( -1 != s_fIPCFromRouter )
         pResult = ruby_ipc_try_read_message(s_fIPCFromRouter, 9000, s_BufferTmpOutputRouterMessage, &s_BufferTmpOutputRouterMessagePos, s_BufferPipeFromRouter);
      if ( NULL == pResult )
      {
         hardware_sleep_ms(10);
         if ( ruby_ipc_get_read_continous_error_count() > 100 )
         {
            log_line("Too many read errors on pipe from router. Flag read error.");
            s_bIPCRouterHasReadErrors = true;
         }
         continue;
      }
      pthread_mutex_lock(&s_pThreadIPCMutex);
      if ( s_iCountMessagesFromRouter >= MAX_ROUTER_MESSAGES )
      {
         log_softerror_and_alarm("[Router COMM] The local router message queue is full. Ignoring message received from router.");
      }
      else
      {
         t_packet_header* pPH = (t_packet_header*)pResult;
         s_MessagesFromRouterSize[s_iCountMessagesFromRouter] = pPH->total_length;
         if ( (pPH->total_length >= MAX_PACKET_TOTAL_SIZE) || (pPH->total_length < sizeof(t_packet_header)) )
            log_softerror_and_alarm("[Router COMM] Received message from router too big or small (%d bytes). Ignoring it.", (int)pPH->total_length);
         else
         {
            memcpy(&(s_pMessagesFromRouter[s_iCountMessagesFromRouter][0]), pResult, s_MessagesFromRouterSize[s_iCountMessagesFromRouter]);
            s_iCountMessagesFromRouter++;
         }
      }
      pthread_mutex_unlock(&s_pThreadIPCMutex);
   }
   log_line("[Router COMM] Stopped IPC receiving thread.");
}
