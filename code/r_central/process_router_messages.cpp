/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "../base/alarms.h"
#include "../base/hw_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/ctrl_interfaces.h"
#include "../common/string_utils.h"
#include "process_router_messages.h"
#include "shared_vars.h"
#include "timers.h"
#include "pairing.h"
#include "warnings.h"
#include "ui_alarms.h"
#include "popup_log.h"
#include "osd.h"
#include "osd_common.h"
#include "notifications.h"
#include "events.h"
#include "colors.h"
#include "ui_alarms.h"

int s_fIPCToRouter = -1;
int s_fIPCFromRouter = -1;

u8 s_BufferPipeFromRouter[MAX_PACKET_TOTAL_SIZE];
u8 s_BufferTmpOutputRouterMessage[MAX_PACKET_TOTAL_SIZE];
int s_BufferTmpOutputRouterMessagePos = 0;

u8 s_BufferLastCommandReply[MAX_PACKET_TOTAL_SIZE];

u32 s_uLastCommandResponseQueried = MAX_U32;
u32 s_uLastCommandResponseRetryCounterQueried = MAX_U32;

static char s_BufferVehicleLogFileData[2048];
static int s_BufferVehicleLogFilePos = 0;

void start_pipes_to_router()
{
   log_line("[Router COMM] Start pipes to router.");

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
  
   s_uLastCommandResponseQueried = MAX_U32;
   s_uLastCommandResponseRetryCounterQueried = MAX_U32;
   s_BufferTmpOutputRouterMessagePos = 0;
   memset( s_BufferLastCommandReply, 0, MAX_PACKET_TOTAL_SIZE);
}

void stop_pipes_to_router()
{
   log_line("[Router COMM] Stop IPC to/from router.");
   
   ruby_close_ipc_channel(s_fIPCFromRouter);
   ruby_close_ipc_channel(s_fIPCToRouter);
   s_fIPCToRouter = -1;
   s_fIPCFromRouter = -1;
}

void send_model_changed_message_to_router(u32 uChangeType)
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type =  PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED;
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY | (uChangeType<<8); 
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);
   PH.extra_flags = 0;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);
}

void send_control_message_to_router(u8 packet_type, u32 extraParam)
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type =  packet_type;
   PH.vehicle_id_src = extraParam;
   PH.vehicle_id_dest = extraParam;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);
   PH.extra_flags = 0;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);
}

void send_control_message_to_router_and_data(u8 packet_type, u8* pData, int nDataLength)
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type =  packet_type;
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = 0;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + nDataLength;
   PH.extra_flags = 0;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   if ( nDataLength > 0 )
      memcpy(&(buffer[sizeof(t_packet_header)]), pData, nDataLength);

   packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);
}

void send_packet_to_router(u8* pPacket, int nLength)
{
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
   
   if ( g_bSearching )
   {
      g_SearchVehicleRuntimeInfo.uVehicleId = pPH->vehicle_id_src;
      return &g_SearchVehicleRuntimeInfo;
   }

   if ( (NULL == g_pCurrentModel) || (pPH->vehicle_id_src == g_pCurrentModel->vehicle_id) || (! g_bFirstModelPairingDone) )
      return &(g_VehiclesRuntimeInfo[0]);

   if ( (NULL != g_pCurrentModel) )
   if ( 0 == g_VehiclesRuntimeInfo[0].uVehicleId || MAX_U32 == g_VehiclesRuntimeInfo[0].uVehicleId )
   {
      g_VehiclesRuntimeInfo[0].uVehicleId = pPH->vehicle_id_src;
      return &(g_VehiclesRuntimeInfo[0]);
   }

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (g_VehiclesRuntimeInfo[i].uVehicleId == 0) || (g_VehiclesRuntimeInfo[i].uVehicleId == MAX_U32) )
      {
         g_VehiclesRuntimeInfo[i].uVehicleId = pPH->vehicle_id_src;
         return &(g_VehiclesRuntimeInfo[i]);
      }
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == pPH->vehicle_id_src )
      {
         return &(g_VehiclesRuntimeInfo[i]);
      }
   }
   return &(g_VehiclesRuntimeInfo[MAX_CONCURENT_VEHICLES-1]);
}

int _process_received_message_from_router(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*) pPacketBuffer;

   //log_line("[Router COMM] Received message component: %d, type %d from router, length: %d", (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE), pPH->packet_type, pPH->total_length);

   if ( g_bSearching )
   if ( (pPH->packet_type != PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) &&
        (pPH->packet_type != PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY) ) 
      return 0;

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);

      // v1 ruby telemetry
      if ( pPH->total_headers_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v1) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
         t_packet_header_ruby_telemetry_extended_v1* pPHRTE = (t_packet_header_ruby_telemetry_extended_v1*)(pPacketBuffer+sizeof(t_packet_header));
      
         if ( g_bSearching )
         {
            char szFreq1[64];
            char szFreq2[64];
            char szFreq3[64];
            strcpy(szFreq1, str_format_frequency(pPHRTE->radio_frequencies[0] & 0x7FFF) );
            strcpy(szFreq2, str_format_frequency(pPHRTE->radio_frequencies[1] & 0x7FFF) );
            strcpy(szFreq3, str_format_frequency(pPHRTE->radio_frequencies[2] & 0x7FFF) );
            
            log_line("Received a Ruby telemetry packet while searching: vehicle ID: %u, radio links (%d): %s, %s, %s",
               pPHRTE->vehicle_id, pPHRTE->radio_links_count,
               szFreq1, szFreq2, szFreq3 );
         }
         if ( ! pRuntimeInfo->bGotRubyTelemetryInfo )
         {
            int iIndexRuntime = -1;
            for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
            {
               if ( &(g_VehiclesRuntimeInfo[i]) == pRuntimeInfo )
               {
                  iIndexRuntime = i;
                  break;
               }
            }

            log_line("Start receiving Ruby telemetry from router for vehicle id %u, runtime index: %d.", pRuntimeInfo->uVehicleId, iIndexRuntime);
         }

         pRuntimeInfo->bGotRubyTelemetryInfo = true;
         pRuntimeInfo->uTimeLastRecvRubyTelemetry = g_TimeNow;
         radio_populate_ruby_telemetry_v2_from_ruby_telemetry_v1(&(pRuntimeInfo->headerRubyTelemetryExtended), (t_packet_header_ruby_telemetry_extended_v1*)(pPacketBuffer+sizeof(t_packet_header)));
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
      if ( pPH->total_headers_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v2) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
         t_packet_header_ruby_telemetry_extended_v2* pPHRTE = (t_packet_header_ruby_telemetry_extended_v2*)(pPacketBuffer+sizeof(t_packet_header));
      
         if ( g_bSearching )
         {
            char szFreq1[64];
            char szFreq2[64];
            char szFreq3[64];
            strcpy(szFreq1, str_format_frequency(pPHRTE->uRadioFrequencies[0]) );
            strcpy(szFreq2, str_format_frequency(pPHRTE->uRadioFrequencies[1]) );
            strcpy(szFreq3, str_format_frequency(pPHRTE->uRadioFrequencies[2]) );
            
            log_line("Received a Ruby telemetry packet while searching: vehicle ID: %u, radio links (%d): %s, %s, %s",
               pPHRTE->vehicle_id, pPHRTE->radio_links_count,
               szFreq1, szFreq2, szFreq3 );
         }
         if ( ! pRuntimeInfo->bGotRubyTelemetryInfo )
         {
            int iIndexRuntime = -1;
            for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
            {
               if ( &(g_VehiclesRuntimeInfo[i]) == pRuntimeInfo )
               {
                  iIndexRuntime = i;
                  break;
               }
            }

            log_line("Start receiving Ruby telemetry from router for vehicle id %u, runtime index: %d.", pRuntimeInfo->uVehicleId, iIndexRuntime);
         }

         pRuntimeInfo->bGotRubyTelemetryInfo = true;
         pRuntimeInfo->uTimeLastRecvRubyTelemetry = g_TimeNow;
         memcpy(&(pRuntimeInfo->headerRubyTelemetryExtended), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_ruby_telemetry_extended_v2) );

         int dx = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v2);
         int totalLength = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v2);
      
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

      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_FC_TELEMETRY )
   if ( (! osd_is_debug()) && (!g_bSearching) )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);

      if ( ! pRuntimeInfo->bGotFCTelemetry )
         log_line("Start receiving FC telemetry from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
      pRuntimeInfo->bGotFCTelemetry = true;
      pRuntimeInfo->uTimeLastRecvFCTelemetry = g_TimeNow;
      memcpy(&(pRuntimeInfo->headerFCTelemetry), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_fc_telemetry) );
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_FC_TELEMETRY_EXTENDED )
   if ( (! osd_is_debug()) && (!g_bSearching) )
   {
      t_structure_vehicle_info* pRuntimeInfo = _get_runtime_info_for_packet(pPacketBuffer);

      if ( ! pRuntimeInfo->bGotFCTelemetry )
         log_line("Start receiving FC telemetry from router for vehicle id %u.", pRuntimeInfo->uVehicleId);
      pRuntimeInfo->bGotFCTelemetry = true;
      pRuntimeInfo->bGotFCTelemetryExtra = true;
      pRuntimeInfo->uTimeLastRecvFCTelemetry = g_TimeNow;
      memcpy(&(pRuntimeInfo->headerFCTelemetry), pPacketBuffer+sizeof(t_packet_header), sizeof(t_packet_header_fc_telemetry) );
      memcpy(&(pRuntimeInfo->headerFCTelemetryExtra), pPacketBuffer+sizeof(t_packet_header) + sizeof(t_packet_header_fc_telemetry), sizeof(t_packet_header_fc_extra) );
      return 0;    
   }

   if ( pPH->packet_type == pPH->packet_type == PACKET_TYPE_FC_RC_CHANNELS )
   if ( !g_bSearching )
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
      warnings_add(szBuff);

      Popup* p = new Popup(szBuff, 0.5, 0.5, 0.6, 10.0);
      p->setCentered();
      p->setFont(g_idFontOSD);
      p->setIconId(g_idIconWarning, get_Color_IconWarning());
      popups_add_topmost(p);

      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS )
   {
      u8 countCards = pPacketBuffer[sizeof(t_packet_header)];
      if ( pPH->total_length != sizeof(t_packet_header) + sizeof(u8) + countCards * sizeof(shared_mem_radio_stats_radio_interface) )
         return 0;

      memcpy((u8*)g_SM_VehicleRxStats, (u8*)(pPacketBuffer + sizeof(t_packet_header) + sizeof(u8)), countCards*sizeof(shared_mem_radio_stats_radio_interface));
      g_bGotStatsVehicleRxCards = true;
      return 0;
   }


   if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD )
   {
      if ( g_bOSDPluginsNeedTelemetryStreams )
      {
         t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
         t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)(pPacketBuffer + sizeof(t_packet_header));
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
   {
      t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pPacketBuffer + sizeof(t_packet_header));
      
      memcpy( s_BufferLastCommandReply, pPacketBuffer, pPH->total_length );
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

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED )
   {
      warnings_add_radio_reinitialized();
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_ALARM )
   {
      u32 uAlarms = 0;
      u32 uFlags = 0;
      u32 uAlarmsCount = 0;
      memcpy(&uAlarms, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
      memcpy(&uFlags, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
      memcpy(&uAlarmsCount, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
      char szBuff[256];
      alarms_to_string(uAlarms, szBuff);

      // Alarm generated by the controller ?
      if ( pPH->vehicle_id_src == 0 )
      {
         log_line("Received local alarms (alarms: %s, repeat count: %d)", szBuff, (int)uAlarmsCount);
         alarms_add_from_local(uAlarms, uFlags, 1);
      }
      else
      {
         log_line("Received vehicle alarms (alarms: %s, repeat count: %d)", szBuff, (int)uAlarmsCount);

         if ( uAlarms & ALARM_ID_LINK_TO_CONTROLLER_LOST )
            g_bIsVehicleLinkToControllerLost = true;
         if ( uAlarms & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
            g_bIsVehicleLinkToControllerLost = false;

         if ( (uAlarms & ALARM_ID_LINK_TO_CONTROLLER_LOST) || (uAlarms & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED) )
         {
            Preferences* pP = get_Preferences();
            if ( ! (pP->uEnabledAlarms & ALARM_ID_LINK_TO_CONTROLLER_LOST) )
            {
               log_line("This alarm (controller link lost/recovered) is disabled. Do not show it in UI.");
               return 0;
            }

            if ( uAlarms & ALARM_ID_LINK_TO_CONTROLLER_LOST )
               warnings_add_link_to_controller_lost();
            else if ( uAlarms & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
               warnings_add_link_to_controller_recovered();
         }
         else
            alarms_add_from_vehicle(uAlarms, uFlags, uAlarmsCount);
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_MODEL_SETTINGS )
   {
      log_line("Received message from router to reload model due to newly received model settings from vehicle.");
      if ( NULL == g_pCurrentModel )
         return 0;

      int length = pPH->total_length - pPH->total_headers_length;
      if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
      {
         u8 size = *(((u8*)pPH) + pPH->total_length-1);
         length -= size;
      }
      u8* pBufferData = pPacketBuffer + sizeof(t_packet_header);
      log_line("Received message with all model settings (compressed). Model file size (compressed): %d", length);

      hw_execute_bash_command("rm -rf tmp/model.mdl", NULL);
      FILE* fd = fopen("tmp/last_recv_model.tar", "wb");
      if ( NULL != fd )
      {
         fwrite(pBufferData, 1, length, fd);
         fclose(fd);
         fd = NULL;
         hw_execute_bash_command("tar -zxf tmp/last_recv_model.tar", NULL);
      }

      u8 buffer[5048];
      length = 0;
      fd = fopen("tmp/model.mdl", "rb");
      if ( NULL != fd )
      {
         length = fread(buffer, 1, 5000, fd);
         fclose(fd);
         onEventReceivedModelSettings(buffer, length, true);
         hw_execute_bash_command("rm -rf tmp/model.mdl", NULL);
      }
      else
         log_error_and_alarm("Failed to load received model file from decompressed file: tmp/model.mdl");
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_RECEIVED_VEHICLE_LOG_SEGMENT )
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

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_LINK_FREQUENCY_CHANGED )
   {
      u32* pI = (u32*)(pPacketBuffer+sizeof(t_packet_header));
      u32 uLinkId = *pI;
      pI++;
      u32 uNewFreq = *pI;
      log_line("Received new model link frequency from router (link %u new frequency: %s). Updating local model copy.", uLinkId+1, str_format_frequency(uNewFreq));
      if ( (int)uLinkId < g_pCurrentModel->radioLinksParams.links_count )
      {
         g_pCurrentModel->radioLinksParams.link_frequency[uLinkId] = uNewFreq;
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkId )
               g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i] = uNewFreq;
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
      memcpy(&(g_pCurrentModel->relay_params), pPacketBuffer + sizeof(t_packet_header), sizeof(type_relay_parameters));
      memcpy(&(g_pCurrentModel->radioInterfacesParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters));
      memcpy(&(g_pCurrentModel->radioLinksParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters));
      
      warnings_add("Radio configuration has changed on the vehicle. Updating controller radio configuration.", g_idIconRadio);
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

   int iCountMessagesProcessed = 0;
   while(true)
   {
      u8* pResult = ruby_ipc_try_read_message(s_fIPCFromRouter, timeoutReadPipeMicroseconds, s_BufferTmpOutputRouterMessage, &s_BufferTmpOutputRouterMessagePos, s_BufferPipeFromRouter);
      if ( NULL == pResult )
      {
         if ( iCountMessagesProcessed > 10 )
            log_softerror_and_alarm("Processing too many messages from router.");
         return iCountMessagesProcessed;
      }
      iCountMessagesProcessed++;
      t_packet_header* pPH = (t_packet_header*) &s_BufferPipeFromRouter[0];
      if ( ! packet_check_crc(s_BufferPipeFromRouter, pPH->total_length) )
          log_softerror_and_alarm("[Router COMM] Received invalid message (invalid CRC) from router. Ignoring it.");
      else
          _process_received_message_from_router(s_BufferPipeFromRouter);

      u32 uTimeNow = get_current_timestamp_ms();
      if ( uTimeNow >= uTimeStart + uMaxMiliseconds )
      {
         if ( iCountMessagesProcessed > 10 )
            log_softerror_and_alarm("Processing too many messages from router.");
         return iCountMessagesProcessed;
      }
      timeoutReadPipeMicroseconds = 1000 * (uTimeStart + uMaxMiliseconds - uTimeNow);
   }
   return iCountMessagesProcessed;
}

u8* get_last_command_reply_from_router()
{
   if ( 0 == s_BufferLastCommandReply[0] && 0 == s_BufferLastCommandReply[1] )
   if ( 0 == s_BufferLastCommandReply[2] && 0 == s_BufferLastCommandReply[3] )
   if ( 0 == s_BufferLastCommandReply[4] && 0 == s_BufferLastCommandReply[5] )
      return NULL;

   t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)((&s_BufferLastCommandReply[0]) + sizeof(t_packet_header));
   if ( pPHCR->origin_command_counter == s_uLastCommandResponseQueried &&
        pPHCR->origin_command_resend_counter == s_uLastCommandResponseRetryCounterQueried )
      return NULL;


   s_uLastCommandResponseQueried = pPHCR->origin_command_counter;
   s_uLastCommandResponseRetryCounterQueried = pPHCR->origin_command_resend_counter;

   return s_BufferLastCommandReply;
}