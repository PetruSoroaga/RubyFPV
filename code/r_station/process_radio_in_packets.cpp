/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "process_radio_in_packets.h"
#include "processor_rx_audio.h"
#include "processor_rx_video.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/radio_utils.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../base/encr.h"
#include "../base/models_list.h"
#include "../base/commands.h"
#include "../base/ruby_ipc.h"
#include "../base/parser_h264.h"
#include "../base/camera_utils.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../common/models_connect_frequencies.h"
#include "../common/relay_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radio_duplicate_det.h"
#include "../radio/radio_tx.h"
#include "ruby_rt_station.h"
#include "relay_rx.h"
#include "test_link_params.h"
#include "shared_vars.h"
#include "timers.h"
#include "process_video_packets.h"
#include "adaptive_video.h"

#define MAX_PACKETS_IN_ID_HISTORY 6

fd_set s_ReadSetRXRadio;
u32 s_uRadioRxReadTimeoutCount = 0;

u32 s_uTotalBadPacketsReceived = 0;

u32 s_TimeLastLoggedSearchingRubyTelemetry = 0;
u32 s_TimeLastLoggedSearchingRubyTelemetryVehicleId = 0;

ParserH264 s_ParserH264RadioInput;

#define MAX_ALARMS_HISTORY 50

u32 s_uLastReceivedAlarmsIndexes[MAX_ALARMS_HISTORY];
u32 s_uTimeLastReceivedAlarm = 0;

void init_radio_rx_structures()
{
   for( int i=0; i<MAX_ALARMS_HISTORY; i++ )
      s_uLastReceivedAlarmsIndexes[i] = MAX_U32;

   s_ParserH264RadioInput.init();
}

int _process_received_ruby_message(int iRuntimeIndex, int iInterfaceIndex, u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   
   u8 uPacketType = pPH->packet_type;
   int iTotalLength = pPH->total_length;
   u32 uVehicleIdSrc = pPH->vehicle_id_src;
   u32 uVehicleIdDest = pPH->vehicle_id_dest;
   
   if ( uPacketType == PACKET_TYPE_TEST_RADIO_LINK )
   {
      test_link_process_received_message(iInterfaceIndex, pPacketBuffer);
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_DEBUG_VEHICLE_RT_INFO )
   {
      if ( iTotalLength == sizeof(t_packet_header) + sizeof(vehicle_runtime_info) )
      {
         memcpy((u8*)&g_SMVehicleRTInfo, pPacketBuffer + sizeof(t_packet_header), sizeof(vehicle_runtime_info));
         if ( g_SMControllerRTInfo.iCurrentIndex > 100 && g_SMControllerRTInfo.iCurrentIndex < 150 )
         {
            if ( 0 == g_SMControllerRTInfo.iDeltaIndexFromVehicle )
               g_SMControllerRTInfo.iDeltaIndexFromVehicle = g_SMVehicleRTInfo.iCurrentIndex - g_SMControllerRTInfo.iCurrentIndex;
            else
            {
               if ( g_SMVehicleRTInfo.iCurrentIndex > g_SMControllerRTInfo.iCurrentIndex)
               if ( g_SMVehicleRTInfo.iCurrentIndex - g_SMControllerRTInfo.iCurrentIndex > g_SMControllerRTInfo.iDeltaIndexFromVehicle )
                  g_SMControllerRTInfo.iDeltaIndexFromVehicle = g_SMVehicleRTInfo.iCurrentIndex - g_SMControllerRTInfo.iCurrentIndex;
               if ( g_SMVehicleRTInfo.iCurrentIndex < g_SMControllerRTInfo.iCurrentIndex )
               if ( g_SMVehicleRTInfo.iCurrentIndex - g_SMControllerRTInfo.iCurrentIndex < g_SMControllerRTInfo.iDeltaIndexFromVehicle )
                  g_SMControllerRTInfo.iDeltaIndexFromVehicle = g_SMVehicleRTInfo.iCurrentIndex - g_SMControllerRTInfo.iCurrentIndex;
            }
         }  
      }
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED )
   {
      log_line("Received current radio configuration from vehicle uid %u, packet size: %d bytes.", uVehicleIdSrc, iTotalLength);
      if ( iTotalLength != (sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters)) )
      {
         log_softerror_and_alarm("Received current radio configuration: invalid packet size. Ignoring.");
         return 0;
      }
      if ( NULL == g_pCurrentModel )
         return 0;
      bool bRadioConfigChanged = false;
      if ( g_pCurrentModel->uVehicleId == uVehicleIdSrc )
      {
         type_radio_interfaces_parameters radioInt;
         type_radio_links_parameters radioLinks;
         memcpy(&radioInt, pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters));
         memcpy(&radioLinks, pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters));
      
         bRadioConfigChanged = IsModelRadioConfigChanged(&(g_pCurrentModel->radioLinksParams), &(g_pCurrentModel->radioInterfacesParams),
                  &radioLinks, &radioInt);

         if ( bRadioConfigChanged )
            log_line("Received from vehicle the current radio config for current model and radio links count or radio interfaces count has changed on the vehicle side.");
      }
      memcpy(&(g_pCurrentModel->relay_params), pPacketBuffer + sizeof(t_packet_header), sizeof(type_relay_parameters));
      memcpy(&(g_pCurrentModel->radioInterfacesParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters));
      memcpy(&(g_pCurrentModel->radioLinksParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters));
      g_pCurrentModel->updateRadioInterfacesRadioFlagsFromRadioLinksFlags();
      saveControllerModel(g_pCurrentModel);

      pPH->packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      if ( bRadioConfigChanged )
         reasign_radio_links(false);
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION )
   {      
      u32 uResendCount = 0;
      if ( iTotalLength >= (int)(sizeof(t_packet_header) + sizeof(u32)) )
         memcpy(&uResendCount, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));

      log_line("Received pairing confirmation from vehicle (received vehicle resend counter: %u). VID: %u, CID: %u", uResendCount, uVehicleIdSrc, uVehicleIdDest);
      
      if ( -1 == iRuntimeIndex )
      {
         log_softerror_and_alarm("Received pairing confirmation from unknown VID %u. Currently known runtime vehicles:", uVehicleIdSrc);
         logCurrentVehiclesRuntimeInfo();
         return 0;
      }

      if ( ! g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsPairingDone )
      {
         ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, iTotalLength);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

         send_alarm_to_central(ALARM_ID_CONTROLLER_PAIRING_COMPLETED, uVehicleIdSrc, 0);
         g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsPairingDone = true;
      }
      return 0;
   }

   if ( (uPacketType == PACKET_TYPE_SIK_CONFIG) || (uPacketType == PACKET_TYPE_OTA_UPDATE_STATUS) )
   {
      if ( -1 != g_fIPCToCentral )
         ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, iTotalLength);
      else
         log_softerror_and_alarm("Received SiK config message but there is not channel to central to notify it.");

      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_RADIO_REINITIALIZED )
   {
      log_line("Received message that vehicle radio interfaces where reinitialized.");
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED, STREAM_ID_DATA);
      PH.total_length = sizeof(t_packet_header);
      radio_packet_compute_crc((u8*)&PH, PH.total_length);
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)&PH, PH.total_length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_ALARM )
   {
      u32 uAlarmIndex = 0;
      u32 uAlarm = 0;
      u32 uFlags1 = 0;
      u32 uFlags2 = 0;
      char szBuff[256];
      if ( iTotalLength == ((int)(sizeof(t_packet_header) + 3 * sizeof(u32))) )
      {
         memcpy(&uAlarm, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy(&uFlags1, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         memcpy(&uFlags2, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
         alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);
         log_line("Received alarm from vehicle (old format). Alarm: %s, alarm index: %u", szBuff, uAlarmIndex);
      }
      // New format, version 7.5
      else if ( iTotalLength == ((int)(sizeof(t_packet_header) + 4 * sizeof(u32))) )
      {
         memcpy(&uAlarmIndex, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy(&uAlarm, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         memcpy(&uFlags1, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
         memcpy(&uFlags2, pPacketBuffer + sizeof(t_packet_header) + 3*sizeof(u32), sizeof(u32));
         alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);
         log_line("Received alarm from vehicle. Alarm: %s, alarm index: %u", szBuff, uAlarmIndex);
      
         bool bDuplicateAlarm = false;
         if ( g_TimeNow > s_uTimeLastReceivedAlarm + 5000 )
             bDuplicateAlarm = false;
         else
         {
            for( int i=0; i<MAX_ALARMS_HISTORY; i++ )
            {
               if ( s_uLastReceivedAlarmsIndexes[i] == uAlarmIndex )
               {
                  bDuplicateAlarm = true;
                  break;
               }
            }  
         }
         if ( bDuplicateAlarm )
         {
            log_line("Duplicate alarm. Ignoring it.");
            return 0;
         }
         
         for( int i=MAX_ALARMS_HISTORY-1; i>0; i-- )
            s_uLastReceivedAlarmsIndexes[i] = s_uLastReceivedAlarmsIndexes[i-1];
         s_uLastReceivedAlarmsIndexes[0] = uAlarmIndex;
      }
      else
      {
         log_softerror_and_alarm("Received invalid alarm from vehicle. Received %d bytes, expected %d bytes", iTotalLength, (int)sizeof(t_packet_header) + 4 * (int)sizeof(u32));
         return 0;
      }

      s_uTimeLastReceivedAlarm = g_TimeNow;
      
      if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_LOST )
      {
         if ( iRuntimeIndex != -1 )
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleLinkToControllerLostAlarm = true;
         log_line("Received alarm that vehicle lost the connection from controller.");
      }
      if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
      {
         if ( iRuntimeIndex != -1 )
            g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleLinkToControllerLostAlarm = false;
         log_line("Received alarm that vehicle recovered the connection from controller.");
      }
      
      log_line("Sending the alarm to central...");
      ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, iTotalLength);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      log_line("Sent alarm to central");
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
   if ( iTotalLength >= (int)(sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32)) )
   {
      u8 uPingId = 0;
      u32 uVehicleLocalTimeMs = 0;
      u8 uOriginalLocalRadioLinkId = 0;
      u8 uReplyVehicleLocalRadioLinkId = 0;

      memcpy(&uPingId, pPacketBuffer + sizeof(t_packet_header), sizeof(u8));
      memcpy(&uVehicleLocalTimeMs, pPacketBuffer + sizeof(t_packet_header)+sizeof(u8), sizeof(u32));
      memcpy(&uOriginalLocalRadioLinkId, pPacketBuffer + sizeof(t_packet_header)+sizeof(u8)+sizeof(u32), sizeof(u8));
      if ( pPH->total_length > sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32) )
         memcpy(&uReplyVehicleLocalRadioLinkId, pPacketBuffer + sizeof(t_packet_header)+2*sizeof(u8) + sizeof(u32), sizeof(u8));
      int iIndex = getVehicleRuntimeIndex(pPH->vehicle_id_src);
      if ( iIndex >= 0 )
      {
         if ( uPingId == g_State.vehiclesRuntimeInfo[iIndex].uLastPingIdSentToVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] )
         {
            g_TimeNow = get_current_timestamp_ms();
            u32 uRoundtripMilis = g_TimeNow - g_State.vehiclesRuntimeInfo[iIndex].uTimeLastPingSentToVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId];
            controller_rt_info_update_ack_rt_time(&g_SMControllerRTInfo, pPH->vehicle_id_src, g_SM_RadioStats.radio_interfaces[iInterfaceIndex].assignedLocalRadioLinkId, uRoundtripMilis);
            if ( uPingId != g_State.vehiclesRuntimeInfo[iIndex].uLastPingIdReceivedFromVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] )
            {
               g_State.vehiclesRuntimeInfo[iIndex].uLastPingIdReceivedFromVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] = uPingId;
               g_State.vehiclesRuntimeInfo[iIndex].uTimeLastPingReplyReceivedFromVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] = g_TimeNow;
               g_State.vehiclesRuntimeInfo[iIndex].uPingRoundtripTimeOnLocalRadioLinks[uOriginalLocalRadioLinkId] = uRoundtripMilis;
               if ( uRoundtripMilis < g_State.vehiclesRuntimeInfo[iIndex].uMinimumPingTimeMilisec )
               {
                  g_State.vehiclesRuntimeInfo[iIndex].uMinimumPingTimeMilisec = uRoundtripMilis;
                  adjustLinkClockDeltasForVehicleRuntimeIndex(iIndex, uRoundtripMilis, uVehicleLocalTimeMs);
               }
            }
         }
      }
      else
         log_softerror_and_alarm("Received ping reply from unknown VID: %u", pPH->vehicle_id_src);
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_MODEL_SETTINGS )
   {
      if ( -1 != g_fIPCToCentral )
         ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, iTotalLength);
      else
         log_softerror_and_alarm("Received message with current compressed model settings (%d bytes) but there is not channel to central to notify it.", iTotalLength - sizeof(t_packet_header));

      return 0;
   }

   if ( uPacketType == PACKET_TYPE_RUBY_LOG_FILE_SEGMENT )
   if ( iTotalLength >= (int)sizeof(t_packet_header) )
   {
      t_packet_header_file_segment* pPHFS = (t_packet_header_file_segment*)(pPacketBuffer + sizeof(t_packet_header));
      log_line("Received a log file segment: file id: %d, segment size: %d, segment index: %d", pPHFS->file_id, (int)pPHFS->segment_size, (int)pPHFS->segment_index);
      if ( pPHFS->file_id == FILE_ID_VEHICLE_LOG )
      {
         char szFile[128];
         snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), LOG_FILE_VEHICLE, g_pCurrentModel->getShortName());      
         char szPath[256];
         strcpy(szPath, FOLDER_LOGS);
         strcat(szPath, szFile);

         FILE* fd = fopen(szPath, "ab");
         if ( NULL != fd )
         {
            u8* pData = pPacketBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_file_segment);
            fwrite(pData, 1, pPHFS->segment_size, fd);
            fclose(fd);
         }
         // Forward the message as a local control message to ruby_central
         pPH->packet_flags &= (~PACKET_FLAGS_MASK_MODULE);
         pPH->packet_flags |= PACKET_COMPONENT_LOCAL_CONTROL;
         pPH->packet_type = PACKET_TYPE_LOCAL_CONTROL_RECEIVED_VEHICLE_LOG_SEGMENT;
         
         ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, pPH->total_length);

         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         return 0;
      }
      return 0;
   }

   if ( uPacketType == PACKET_TYPE_NEGOCIATE_RADIO_LINKS )
   if ( iTotalLength > (int)sizeof(t_packet_header) + (int)sizeof(u8) )
   {
      // uCommand is second byte after header
      u8 uCommand = pPacketBuffer[sizeof(t_packet_header) + sizeof(u8)];
      if ( NEGOCIATE_RADIO_STEP_DATA_RATE == uCommand )
      {
         radio_stats_reset_interfaces_rx_info(&g_SM_RadioStats);
      }
      ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, iTotalLength);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return 0;
   }
   return 0;
}

void _process_received_single_packet_while_searching(int interfaceIndex, u8* pData, int length)
{
   t_packet_header* pPH = (t_packet_header*)pData;
      
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   {
      static u32 s_uTimeLastNotifVideoOnSearchToCentral = 0;
      if ( g_TimeNow >= s_uTimeLastNotifVideoOnSearchToCentral + 50 )
      {
         s_uTimeLastNotifVideoOnSearchToCentral = g_TimeNow;
         // Notify central
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROLL_VIDEO_DETECTED_ON_SEARCH, STREAM_ID_DATA);
         PH.vehicle_id_src = g_uSearchFrequency;
         PH.vehicle_id_dest = 0;
         PH.total_length = sizeof(t_packet_header);

         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         radio_packet_compute_crc(packet, PH.total_length);

         ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
      return;
   }

   // Ruby telemetry is always sent to central and rx telemetry (to populate Ruby telemetry shared object)
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
   {
      // v3,v4 ruby telemetry
      bool bIsV3 = false;
      bool bIsV4 = false;

      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3)))
         bIsV3 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)))
         bIsV3 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)))
         bIsV3 = true;
      if ( bIsV3 )
      {
         t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pData + sizeof(t_packet_header));
         if ( (pPHRTE->rubyVersion >> 4) > 10 )
            bIsV3 = false;
         if ( (pPHRTE->rubyVersion >> 4) == 10 )
         if ( (pPHRTE->rubyVersion & 0x0F) > 4 )
            bIsV3 = false;
      }

      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v4)))
         bIsV4 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v4) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)))
         bIsV4 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v4) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)))
         bIsV4 = true;
      if ( bIsV4 )
      {
         t_packet_header_ruby_telemetry_extended_v4* pPHRTE = (t_packet_header_ruby_telemetry_extended_v4*)(pData + sizeof(t_packet_header));
         if ( (pPHRTE->rubyVersion >> 4) < 10 )
            bIsV4 = false;
         if ( (pPHRTE->rubyVersion >> 4) == 10 )
         if ( (pPHRTE->rubyVersion & 0x0F) < 4 )
            bIsV4 = false;
      }

      log_line("Received telemetry while searching. V3=%s V4=%s", bIsV3?"yes":"no", bIsV4?"yes":"no");
      if ( (!bIsV3) && (!bIsV4) )
      {
         t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pData + sizeof(t_packet_header)); 
         log_softerror_and_alarm("Received unknown telemetry version while searching (on %d Mhz). Version: %d.%d, Size: %d bytes (%d+%d)",
            g_uSearchFrequency/1000, pPHRTE->rubyVersion >> 4, pPHRTE->rubyVersion & 0x0F, pPH->total_length, sizeof(t_packet_header), sizeof(t_packet_header_ruby_telemetry_extended_v3));
      }
      if ( bIsV3 )
      {
         t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pData + sizeof(t_packet_header));
         u8 vMaj = (pPHRTE->rubyVersion) >> 4;
         u8 vMin = (pPHRTE->rubyVersion) & 0x0F;
         if ( (g_TimeNow >= s_TimeLastLoggedSearchingRubyTelemetry + 2000) || (s_TimeLastLoggedSearchingRubyTelemetryVehicleId != pPH->vehicle_id_src) )
         {
            s_TimeLastLoggedSearchingRubyTelemetry = g_TimeNow;
            s_TimeLastLoggedSearchingRubyTelemetryVehicleId = pPH->vehicle_id_src;
            char szFreq1[64];
            char szFreq2[64];
            char szFreq3[64];
            strcpy(szFreq1, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[0]));
            strcpy(szFreq2, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[1]));
            strcpy(szFreq3, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[2]));
            log_line("Received a Ruby telemetry packet (version 3) while searching: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
             pPHRTE->uVehicleId, vMaj, vMin, pPHRTE->radio_links_count, 
             szFreq1, szFreq2, szFreq3 );
         }
         if ( -1 != g_fIPCToCentral )
            ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
      if ( bIsV4 )
      {
         t_packet_header_ruby_telemetry_extended_v4* pPHRTE = (t_packet_header_ruby_telemetry_extended_v4*)(pData + sizeof(t_packet_header));
         u8 vMaj = pPHRTE->rubyVersion;
         u8 vMin = pPHRTE->rubyVersion;
         vMaj = vMaj >> 4;
         vMin = vMin & 0x0F;
         if ( (g_TimeNow >= s_TimeLastLoggedSearchingRubyTelemetry + 2000) || (s_TimeLastLoggedSearchingRubyTelemetryVehicleId != pPH->vehicle_id_src) )
         {
            s_TimeLastLoggedSearchingRubyTelemetry = g_TimeNow;
            s_TimeLastLoggedSearchingRubyTelemetryVehicleId = pPH->vehicle_id_src;
            char szFreq1[64];
            char szFreq2[64];
            char szFreq3[64];
            strcpy(szFreq1, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[0]));
            strcpy(szFreq2, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[1]));
            strcpy(szFreq3, str_format_frequency(pPHRTE->uRadioFrequenciesKhz[2]));
            log_line("Received a Ruby telemetry packet (version 4) while searching: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
             pPHRTE->uVehicleId, vMaj, vMin, pPHRTE->radio_links_count, 
             szFreq1, szFreq2, szFreq3 );
         }
         if ( -1 != g_fIPCToCentral )
            ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_SHORT )
   {
      if ( (g_TimeNow >= s_TimeLastLoggedSearchingRubyTelemetry + 2000) || (s_TimeLastLoggedSearchingRubyTelemetryVehicleId != pPH->vehicle_id_src) )
      {
         s_TimeLastLoggedSearchingRubyTelemetry = g_TimeNow;
         s_TimeLastLoggedSearchingRubyTelemetryVehicleId = pPH->vehicle_id_src;
         log_line("Received a Ruby telemetry short packet while searching: vehicle ID: %u", pPH->vehicle_id_src );
      }
      if ( -1 != g_fIPCToCentral )
         ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }   
}


void _check_update_first_pairing_done_if_needed(int iInterfaceIndex, u8* pPacketData)
{
   if ( g_bFirstModelPairingDone || g_bSearching || (NULL == pPacketData) )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacketData;
   
   u32 uStreamPacketIndex = pPH->stream_packet_idx;
   u32 uVehicleIdSrc = pPH->vehicle_id_src;
   
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   u32 uCurrentFrequencyKhz = 0;
   if ( NULL != pRadioHWInfo )
   {
      uCurrentFrequencyKhz = pRadioHWInfo->uCurrentFrequencyKhz;
      log_line("Received first radio packet (packet index %u) (from VID %u) on radio interface %d, freq: %s, and first pairing was not done. Do first pairing now.",
       uStreamPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, uVehicleIdSrc, iInterfaceIndex+1, str_format_frequency(uCurrentFrequencyKhz));
   }
   else
      log_line("Received first radio packet (packet index %u) (from VID %u) on radio interface %d and first pairing was not done. Do first pairing now.", uStreamPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, uVehicleIdSrc, iInterfaceIndex+1);

   log_line("Current router local model VID: %u, ptr: %X, models current model: VID: %u, ptr: %X",
      g_pCurrentModel->uVehicleId, g_pCurrentModel,
      getCurrentModel()->uVehicleId, getCurrentModel());
   g_bFirstModelPairingDone = true;
   g_pCurrentModel->uVehicleId = uVehicleIdSrc;
   g_pCurrentModel->b_mustSyncFromVehicle = true;
   g_pCurrentModel->is_spectator = false;
   deleteAllModels();
   addNewModel();
   replaceModel(0, g_pCurrentModel);
   saveControllerModel(g_pCurrentModel);
   logControllerModels();
   set_model_main_connect_frequency(g_pCurrentModel->uVehicleId, uCurrentFrequencyKhz);
   ruby_set_is_first_pairing_done();

   resetVehicleRuntimeInfo(0);
   g_State.vehiclesRuntimeInfo[0].uVehicleId = uVehicleIdSrc;
   
   // Notify central
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_FIRST_PAIRING_DONE, STREAM_ID_DATA);
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   radio_packet_compute_crc(packet, PH.total_length);
   if ( ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length) )
      log_line("Sent notification to central that first parining was done.");
   else
      log_softerror_and_alarm("Failed to send notification to central that first parining was done.");
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}

void _check_update_bidirectional_link_state(int iRuntimeIndex, u8 uPacketType, u8 uPacketFlags)
{
   bool bPacketIsAck = false;
   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   if ( uPacketType == PACKET_TYPE_COMMAND_RESPONSE )
      bPacketIsAck = true;

   if ( (uPacketType == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION) ||
        (uPacketType == PACKET_TYPE_RUBY_PING_CLOCK_REPLY) ||
        (uPacketType == PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK) ||
        (uPacketType == PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK) ||
        (uPacketType == PACKET_TYPE_NEGOCIATE_RADIO_LINKS) ||
        (uPacketType == PACKET_TYPE_TEST_RADIO_LINK) )
      bPacketIsAck = true;

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   if ( uPacketFlags & PACKET_FLAGS_BIT_RETRANSMITED )
      bPacketIsAck = true;

   if ( bPacketIsAck )
   {
      g_SM_RadioStats.uLastTimeReceivedAckFromAVehicle = g_TimeNow;
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastTimeReceivedAckFromVehicle = g_TimeNow;
      g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleLinkToControllerLostAlarm = false;
   }
}

// Returns 1 if end of a video block was reached
// Returns -1 if the packet is not for this vehicle or was not processed

int process_received_single_radio_packet(int iInterfaceIndex, u8* pData, int iDataLength)
{
   t_packet_header* pPH = (t_packet_header*)pData;
   
   u32 uStreamPacketIndex = pPH->stream_packet_idx;
   u32 uVehicleIdSrc = pPH->vehicle_id_src;
   u32 uVehicleIdDest = pPH->vehicle_id_dest;
   u8 uPacketType = pPH->packet_type;
   int iPacketLength = pPH->total_length;
   u8 uPacketFlags = pPH->packet_flags;

   if ( NULL != g_pProcessStats )
   {
      // Added in radio_rx thread too
      u32 uGap = g_TimeNow - g_pProcessStats->lastRadioRxTime;
      if ( uGap > 255 )
         uGap = 255;
      if ( uGap > g_SMControllerRTInfo.uRxMaxAirgapSlots2[g_SMControllerRTInfo.iCurrentIndex] )
         g_SMControllerRTInfo.uRxMaxAirgapSlots2[g_SMControllerRTInfo.iCurrentIndex] = uGap;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
   }

   #ifdef PROFILE_RX
   u32 timeStart = get_current_timestamp_ms();
   #endif

   _check_update_first_pairing_done_if_needed(iInterfaceIndex, pData);

   // Searching ?
   
   if ( g_bSearching )
   {
      _process_received_single_packet_while_searching(iInterfaceIndex, pData, iDataLength);
      return 0;
   }
   
   if ( (0 == uVehicleIdSrc) || (MAX_U32 == uVehicleIdSrc) )
   {
      log_error_and_alarm("Received invalid radio packet: Invalid source vehicle id: %u (vehicle id dest: %u, packet type: %s, %d bytes, %d total bytes, component: %d)",
         uVehicleIdSrc, uVehicleIdDest, str_get_packet_type(uPacketType), iDataLength, pPH->total_length, pPH->packet_flags & PACKET_FLAGS_MASK_MODULE);
      return -1;
   }

   u32 uStreamId = (uStreamPacketIndex)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   if ( uStreamId >= MAX_RADIO_STREAMS )
      uStreamId = 0;

   bool bNewVehicleId = true;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( 0 == g_State.vehiclesRuntimeInfo[i].uVehicleId )
         break;
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == uVehicleIdSrc )
      {
         bNewVehicleId = false;
         break;
      }
   }

   if ( bNewVehicleId )
   {
      log_line("Start receiving radio packets from new VID: %u", uVehicleIdSrc);
      int iCountUsed = 0;
      int iFirstFree = -1;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_State.vehiclesRuntimeInfo[i].uVehicleId != 0 )
            iCountUsed++;

         if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == 0 )
         if ( -1 == iFirstFree )
            iFirstFree = i;
      }

      logCurrentVehiclesRuntimeInfo();

      if ( (iCountUsed >= MAX_CONCURENT_VEHICLES) || (-1 == iFirstFree) )
      {
         log_softerror_and_alarm("Radio in: No more room in vehicles runtime info structure for new vehicle VID %u", uVehicleIdSrc);
         char szTmp[256];
         szTmp[0] = 0;
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            char szT[32];
            sprintf(szT, "%u", g_State.vehiclesRuntimeInfo[i].uVehicleId);
            if ( 0 != i )
               strcat(szTmp, ", ");
            strcat(szTmp, szT);
         }
         log_softerror_and_alarm("Radio in: Current vehicles in vehicles runtime info: [%s]", szTmp);                   
      }
      else
      {
         resetVehicleRuntimeInfo(iFirstFree);
         g_State.vehiclesRuntimeInfo[iFirstFree].uVehicleId = uVehicleIdSrc;
         adaptive_video_on_new_vehicle(iFirstFree);
      }
   }

   int iRuntimeIndex = getVehicleRuntimeIndex(uVehicleIdSrc);
   if ( -1 != iRuntimeIndex )
      _check_update_bidirectional_link_state(iRuntimeIndex, uPacketType, uPacketFlags);

   // Detect vehicle restart (stream packets indexes are starting again from zero or low value )

   if ( radio_dup_detection_is_vehicle_restarted(uVehicleIdSrc) )
   {
      log_line("RX thread detected vehicle restart (received stream packet index %u for stream %d, on interface index %d; Max received stream packet index was at %u for stream %d",
       (uStreamPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), uStreamId, iInterfaceIndex+1, radio_dup_detection_get_max_received_packet_index_for_stream(uVehicleIdSrc, uStreamId), uStreamId );
      g_pProcessStats->alarmFlags = PROCESS_ALARM_RADIO_STREAM_RESTARTED;
      g_pProcessStats->alarmTime = g_TimeNow;

      if ( -1 != iRuntimeIndex )
      {
         // Reset pairing info so that pairing is done again with this vehicle
         resetPairingStateForVehicleRuntimeInfo(iRuntimeIndex);
      }
      for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      {
         if ( NULL == g_pVideoProcessorRxList[i] )
            break;
         if ( g_pVideoProcessorRxList[i]->m_uVehicleId == uVehicleIdSrc )
         {
            g_pVideoProcessorRxList[i]->resetStateOnVehicleRestart();
            break;
         }
      }
      radio_dup_detection_reset_vehicle_restarted_flag(uVehicleIdSrc);
      log_line("Done processing RX thread detected vehicle restart.");
   }
   
   // For data streams, discard packets that are too older than the newest packet received on that stream for that vehicle
   
   u32 uMaxStreamPacketIndex = radio_dup_detection_get_max_received_packet_index_for_stream(uVehicleIdSrc, uStreamId);
   
   if ( (uStreamId < STREAM_ID_VIDEO_1) && (uMaxStreamPacketIndex > 100) )
   if ( (uStreamPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) < uMaxStreamPacketIndex-100 )
   {
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
      {
         t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pData + sizeof(t_packet_header));

         if ( pPHCR->origin_command_type == COMMAND_ID_GET_ALL_PARAMS_ZIP )
         {
            log_line("Discarding received command response for get zip model settings, command counter %d, command retry counter %d, on radio interface %d. Discard because this packet index is %u and last received packet index for this data stream (stream %d) is %u",
                pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter,
                iInterfaceIndex+1,
                (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), uStreamId, uMaxStreamPacketIndex);
         }
      }
      return 0;
   }

   bool bIsRelayedPacket = false;

   // Received packet from different vehicle ?

   if ( (NULL != g_pCurrentModel) && (uVehicleIdSrc != g_pCurrentModel->uVehicleId) )
   {
      // Relayed packet?
      if ( !g_bSearching )
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( (g_pCurrentModel->relay_params.uRelayedVehicleId != 0) && (g_pCurrentModel->relay_params.uRelayedVehicleId != MAX_U32) )
      if ( uVehicleIdSrc == g_pCurrentModel->relay_params.uRelayedVehicleId )
         bIsRelayedPacket = true;

      /*
      if ( bIsRelayedPacket )
      {
         relay_rx_process_single_received_packet( iInterfaceIndex, pData, iDataLength);

         #ifdef PROFILE_RX
         u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
         if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
            log_softerror_and_alarm("[Profile-Rx] Processing ruby message from single relayed radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, iInterfaceIndex+1, (int)dTimeEnd);
         #endif

         return 0;
      }
      */

      // Discard all packets except telemetry comming from wrong models, if the first pairing was done

      if ( !bIsRelayedPacket )
      {
         // Ruby telemetry is always sent to central, so that wrong vehicle or relayed vehicles can be detected
         if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
         if ( (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) || 
              (uPacketType == PACKET_TYPE_FC_TELEMETRY) ||
              (uPacketType == PACKET_TYPE_FC_TELEMETRY_EXTENDED) )
         {
            if ( -1 != g_fIPCToCentral )
               ruby_ipc_channel_send_message(g_fIPCToCentral, pData, iDataLength);
            if ( NULL != g_pProcessStats )
               g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         }
         #ifdef PROFILE_RX
         u32 dTime1 = get_current_timestamp_ms() - timeStart;
         if ( dTime1 >= PROFILE_RX_MAX_TIME )
            log_softerror_and_alarm("[Profile-Rx] Processing single radio packet (type: %d len: %d bytes) from different vehicle, from radio interface %d took too long: %d ms.", uPacketType, iPacketLength, iInterfaceIndex+1, (int)dTime1);
         #endif
         
         return 0;
      }
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
   {
      if ( bIsRelayedPacket )
      {
         if ( (uPacketType == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION) ||
              (uPacketType == PACKET_TYPE_RUBY_PING_CLOCK_REPLY) ||
              (uPacketType == PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK) ||
              (uPacketType == PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK) ||
              (uPacketType == PACKET_TYPE_NEGOCIATE_RADIO_LINKS) ||
              (uPacketType == PACKET_TYPE_TEST_RADIO_LINK) )
            _process_received_ruby_message(iRuntimeIndex, iInterfaceIndex, pData);
         return 0;
      }
      _process_received_ruby_message(iRuntimeIndex, iInterfaceIndex, pData);

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing ruby message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", uPacketType, iPacketLength, iInterfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   {
      if ( uPacketType != PACKET_TYPE_COMMAND_RESPONSE )
         return 0;
      if ( iPacketLength < (int)sizeof(t_packet_header) + (int)sizeof(t_packet_header_command_response) )
         return 0;

      t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pData + sizeof(t_packet_header));

      if ( ((pPHCR->origin_command_type & COMMAND_TYPE_MASK) == COMMAND_ID_SET_VIDEO_PARAMS) ||
           ((pPHCR->origin_command_type & COMMAND_TYPE_MASK) == COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES) ||
           ((pPHCR->origin_command_type & COMMAND_TYPE_MASK) == COMMAND_ID_RESET_VIDEO_LINK_PROFILE) ||
           ((pPHCR->origin_command_type & COMMAND_TYPE_MASK) == COMMAND_ID_GET_CORE_PLUGINS_INFO)||
           ((pPHCR->origin_command_type & COMMAND_TYPE_MASK) == COMMAND_ID_GET_ALL_PARAMS_ZIP) )
      {
         log_line("Recv command response. Reset H264 stream detected profile and level for VID %u", pPH->vehicle_id_src);
         shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, pPH->vehicle_id_src); 
         if ( NULL != pSMVideoStreamInfo )
         {
            pSMVideoStreamInfo->uDetectedH264Profile = 0;
            pSMVideoStreamInfo->uDetectedH264ProfileConstrains = 0;
            pSMVideoStreamInfo->uDetectedH264Level = 0;
         }
         g_TimeLastVideoParametersOrProfileChanged = g_TimeNow;
      }

      if ( pPHCR->origin_command_type == COMMAND_ID_GET_ALL_PARAMS_ZIP )
      {
         log_line("[Router] Received command response for get zip model settings, command counter %d, command retry counter %d, %d extra bytes, packet index: %u, on radio interface %d.",
            pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter,
            pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response),
            pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX,
            iInterfaceIndex+1);
      }
      
      if ( bIsRelayedPacket )
         return 0;

      ruby_ipc_channel_send_message(g_fIPCToCentral, pData, iDataLength);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      
      type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(uVehicleIdSrc);
      if ( NULL != pRuntimeInfo )         
      if ( pRuntimeInfo->uLastCommandIdSent != MAX_U32 )
      if ( pRuntimeInfo->uLastCommandIdRetrySent != MAX_U32 )
      if ( pPHCR->origin_command_counter == pRuntimeInfo->uLastCommandIdSent )
      if ( pPHCR->origin_command_resend_counter == pRuntimeInfo->uLastCommandIdRetrySent ) 
      {
         pRuntimeInfo->uLastCommandIdSent = MAX_U32;
         pRuntimeInfo->uLastCommandIdRetrySent = MAX_U32;
         addCommandRTTimeToRuntimeInfo(pRuntimeInfo, g_TimeNow - pRuntimeInfo->uTimeLastCommandIdSent);
      }

      if ( pPHCR->origin_command_type == COMMAND_ID_SET_RADIO_LINK_FLAGS )
      if ( pPHCR->command_response_flags & COMMAND_RESPONSE_FLAGS_OK)
      {
         if ( pPHCR->origin_command_counter != g_uLastInterceptedCommandCounterToSetRadioFlags )
         {
            log_line("Intercepted command response Ok to command sent to set radio flags and radio data datarate on radio link %u to %u bps (%d datarate).", g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand+1, getRealDataRateFromRadioDataRate(g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand, 0), g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand);
            g_uLastInterceptedCommandCounterToSetRadioFlags = pPHCR->origin_command_counter;

            for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
            {
               radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);

               if ( (NULL == pRadioHWInfo) || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
                  continue;

               int nRadioLinkId = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;
               if ( nRadioLinkId < 0 || nRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
                  continue;
               if ( nRadioLinkId != (int)g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand )
                  continue;

               if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[nRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
                  continue;

               if ( (pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS) ||
                    (pRadioHWInfo->iRadioType == RADIO_TYPE_RALINK) )
               {
                  int nRateTx = g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand;
                  update_atheros_card_datarate(g_pCurrentModel, i, nRateTx, g_pProcessStats);
                  g_TimeNow = get_current_timestamp_ms();
               }
            }
         }
      }

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing command message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", uPacketType, iPacketLength, iInterfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   {
      if ( ! bIsRelayedPacket )
      if ( ! g_bSearching )
      if ( g_bFirstModelPairingDone )
      if ( -1 != g_fIPCToTelemetry )
      {
         #ifdef LOG_RAW_TELEMETRY
         if ( uPacketType == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD )
         {
            t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)(pData + sizeof(t_packet_header));
            log_line("[Raw_Telem] Received raw telem packet from vehicle, index %u, %d / %d bytes",
               pPHTR->telem_segment_index, pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_telemetry_raw), pPH->total_length);
         }
         #endif
         if ( (uPacketType != PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS ) &&
              (uPacketType != PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY) &&
              (uPacketType != PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS) )
            ruby_ipc_channel_send_message(g_fIPCToTelemetry, pData, iDataLength);
      }
      bool bSendToCentral = false;
      bool bSendRelayedTelemetry = false;

      // Ruby telemetry and FC telemetry from relayed vehicle is always sent to central to detect the relayed vehicle
      if ( bIsRelayedPacket )
      if ( (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_SHORT) ||
           (uPacketType == PACKET_TYPE_FC_TELEMETRY) ||
           (uPacketType == PACKET_TYPE_FC_TELEMETRY_EXTENDED) ||
           (uPacketType == PACKET_TYPE_TELEMETRY_MSP))
          bSendToCentral = true;

      if ( bIsRelayedPacket )
      if ( relay_controller_must_forward_telemetry_from(g_pCurrentModel, uVehicleIdSrc) )
         bSendRelayedTelemetry = true;

      if ( (! bIsRelayedPacket) || bSendRelayedTelemetry )
      if ( (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_SHORT) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) ||
           (uPacketType == PACKET_TYPE_FC_TELEMETRY) ||
           (uPacketType == PACKET_TYPE_FC_TELEMETRY_EXTENDED) ||
           (uPacketType == PACKET_TYPE_FC_RC_CHANNELS) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY ) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS ) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS) ||
           (uPacketType == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD) ||
           (uPacketType == PACKET_TYPE_DEBUG_INFO) ||
           (uPacketType == PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY) ||
           (uPacketType == PACKET_TYPE_TELEMETRY_MSP))
         bSendToCentral = true;

      if ( bSendToCentral )
      {
         ruby_ipc_channel_send_message(g_fIPCToCentral, pData, iDataLength);
        
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

         #ifdef PROFILE_RX
         u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
         if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
            log_softerror_and_alarm("[Profile-Rx] Processing telemetry message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", uPacketType, iPacketLength, iInterfaceIndex+1, (int)dTimeEnd);
         #endif
         return 0;
      }

      if ( bIsRelayedPacket )
         return 0;

      // To fix
      //if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS )
      //if ( NULL != g_pSM_VideoLinkStats )
      //if ( pPH->total_length == sizeof(t_packet_header) + sizeof(shared_mem_video_link_stats_and_overwrites) )
      //   memcpy(g_pSM_VideoLinkStats, pData+sizeof(t_packet_header), sizeof(shared_mem_video_link_stats_and_overwrites) );

      if ( uPacketType == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_GRAPHS )
      if ( NULL != g_pSM_VideoLinkGraphs )
      if ( iPacketLength == sizeof(t_packet_header) + sizeof(shared_mem_video_link_graphs) )
         memcpy(g_pSM_VideoLinkGraphs, pData+sizeof(t_packet_header), sizeof(shared_mem_video_link_graphs) );

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing telemetry message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", uPacketType, iPacketLength, iInterfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RC )
   {
      if ( bIsRelayedPacket )
         return 0;

      if ( -1 != g_fIPCToRC )
         ruby_ipc_channel_send_message(g_fIPCToRC, pData, iDataLength);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing RC message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", uPacketType, iPacketLength, iInterfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   {
      Model* pModel = findModelWithId(uVehicleIdSrc, 117);
      if ( (NULL == pModel) || (get_sw_version_build(pModel) < 262) )
      {
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            if ( (uVehicleIdSrc == 0) || (g_SM_RadioStats.radio_streams[i][0].uVehicleId == uVehicleIdSrc) || (g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].uVehicleId == uVehicleIdSrc) )
               g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].totalRxBytes = 0;
         }
         return 0;
      }
      int nRet = process_received_video_packet(iInterfaceIndex, pData, iDataLength);
      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing video message from single radio packet (type: %d (%s) len: %d/%d bytes), from radio interface %d took too long: %d ms.", uPacketType, str_get_packet_type(uPacketType), iPacketLength, iDataLength, iInterfaceIndex+1, (int)dTimeEnd);
      #endif
      return nRet;
   }

   if ( (uPacketFlags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_AUDIO )
   {
      if ( bIsRelayedPacket || g_bSearching )
         return 0;

      if ( uPacketType != PACKET_TYPE_AUDIO_SEGMENT )
      {
         //log_line("Received unknown video packet type.");
         return 0;
      }
      process_received_audio_packet(pData);

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing audio message from single radio packet (type: %d len: %d/%d bytes), from radio interface %d took too long: %d ms.", uPacketType, iPacketLength, iDataLength, iInterfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   #ifdef PROFILE_RX
   u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
   if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Processing other type of message from single radio packet (type: %d len: %d/%d bytes), from radio interface %d took too long: %d ms.", uPacketType, iPacketLength, iDataLength, iInterfaceIndex+1, (int)dTimeEnd);
   #endif

   return 0;
} 
