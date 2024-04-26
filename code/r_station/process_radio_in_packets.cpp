/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
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
#include "links_utils.h"
#include "test_link_params.h"
#include "shared_vars.h"
#include "timers.h"

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

   s_ParserH264RadioInput.init(camera_get_active_camera_h264_slices(g_pCurrentModel));
}

void _process_extra_data_from_packet(u8 dataType, u8 dataSize, u8* pExtraData)
{
   log_line("Processing received extra data in packet : extra data type: %d, extra data size: %d", (int)dataType, (int)dataSize);
   if ( dataType == EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK1 || dataType == EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK2 || dataType == EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK3 )
   if ( dataSize == 6 )
   {
      int nLink = 0;
      if ( dataType == EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK2 )
         nLink = 1;
      if ( dataType == EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK3 )
         nLink = 2;
      u32 freq = 0;
      memcpy((u8*)&freq, pExtraData, sizeof(u32));
      u32 freqNew = freq;
      log_line("Received extra packet info to change frequency on radio link %d, new frequency: %s", nLink+1, str_format_frequency(freqNew));
      bool bChanged = false;
      if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[nLink] != freqNew )
         bChanged = true;

      if ( ! bChanged )
      {
         log_line("Same frequency as current one. Ignoring info from vehicle.");
         return;
      }
      u32 freqOld = g_pCurrentModel->radioLinksParams.link_frequency_khz[nLink];
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo || (0 == pRadioHWInfo->isEnabled) )
            continue;
         if ( 0 == hardware_radioindex_supports_frequency(i, freqNew) )
            continue;

         u32 flags = controllerGetCardFlags(pRadioHWInfo->szMAC);
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
            continue;

         if ( pRadioHWInfo->uCurrentFrequencyKhz == freqOld )
         {
            Preferences* pP = get_Preferences();
            radio_utils_set_interface_frequency(g_pCurrentModel, i, nLink, freqNew, g_pProcessStats, pP->iDebugWiFiChangeDelay);
            g_SM_RadioStats.radio_interfaces[i].uCurrentFrequencyKhz = freqNew;
            radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, freqNew);
         }
      }
      hardware_save_radio_info();

      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_SM_RadioStats, sizeof(shared_mem_radio_stats));

      g_pCurrentModel->radioLinksParams.link_frequency_khz[nLink] = freqNew;
      saveControllerModel(g_pCurrentModel);
      log_line("Notifying all other components of the link frequency change.");

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED, STREAM_ID_DATA);
      PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
      PH.vehicle_id_dest = 0;
      PH.total_length = sizeof(t_packet_header) + 2*sizeof(u32);
   
      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      u32* pI = (u32*)((&buffer[0])+sizeof(t_packet_header));
      *pI = (u32)nLink;
      pI++;
      *pI = freqNew;

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length);
      ruby_ipc_channel_send_message(g_fIPCToTelemetry, buffer, PH.total_length);
      log_line("Done notifying all other components about the link frequency change.");
   }
}

int _process_received_ruby_message(int iInterfaceIndex, u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_TEST_RADIO_LINK )
   {
      test_link_process_received_message(iInterfaceIndex, pPacketBuffer);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED )
   {
      log_line("Received current radio configuration from vehicle uid %u, packet size: %d bytes.", pPH->vehicle_id_src, pPH->total_length);
      if ( pPH->total_length != (sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters)) )
      {
         log_softerror_and_alarm("Received current radio configuration: invalid packet size. Ignoring.");
         return 0;
      }
      if ( NULL == g_pCurrentModel )
         return 0;
      if ( g_pCurrentModel->uVehicleId == pPH->vehicle_id_src )
      {
         type_radio_interfaces_parameters radioInt;
         type_radio_links_parameters radioLinks;
         memcpy(&radioInt, pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters));
         memcpy(&radioLinks, pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters));
      
         bool bRadioConfigChanged = IsModelRadioConfigChanged(&(g_pCurrentModel->radioLinksParams), &(g_pCurrentModel->radioInterfacesParams),
                  &radioLinks, &radioInt);

         if ( bRadioConfigChanged )
            log_line("Received from vehicle the current radio config for current model and radio links count or radio interfaces count has changed on the vehicle side.");
      }
      memcpy(&(g_pCurrentModel->relay_params), pPacketBuffer + sizeof(t_packet_header), sizeof(type_relay_parameters));
      memcpy(&(g_pCurrentModel->radioInterfacesParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters), sizeof(type_radio_interfaces_parameters));
      memcpy(&(g_pCurrentModel->radioLinksParams), pPacketBuffer + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), sizeof(type_radio_links_parameters));
      saveControllerModel(g_pCurrentModel);

      pPH->packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION )
   {
      g_uTimeLastReceivedResponseToAMessage = g_TimeNow;
      radio_stats_set_received_response_from_vehicle_now(&g_SM_RadioStats, g_TimeNow);

      u32 uResendCount = 0;
      if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(u32) )
         memcpy(&uResendCount, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));

      log_line("Received pairing confirmation from vehicle (received vehicle resend counter: %u). VID: %u, CID: %u", uResendCount, pPH->vehicle_id_src, pPH->vehicle_id_dest);
      
      int iRuntimeIndex = getVehicleRuntimeIndex(pPH->vehicle_id_src);
      if ( -1 == iRuntimeIndex )
      {
         log_softerror_and_alarm("Received pairing confirmation from unknown VID %u. Currently known runtime vehicles:", pPH->vehicle_id_src);
         logCurrentVehiclesRuntimeInfo();
         return 0;
      }

      if ( ! g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsPairingDone )
      {
         ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, pPH->total_length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

         send_alarm_to_central(ALARM_ID_CONTROLLER_PAIRING_COMPLETED, pPH->vehicle_id_src, 0);
         g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsPairingDone = true;
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_SIK_CONFIG )
   {
      if ( -1 != g_fIPCToCentral )
         ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length);
      else
         log_softerror_and_alarm("Received SiK config message but there is not channel to central to notify it.");

      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_RADIO_REINITIALIZED )
   {
      log_line("Received message that vehicle radio interfaces where reinitialized.");
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED, STREAM_ID_DATA);
      PH.total_length = sizeof(t_packet_header);
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)&PH, PH.total_length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_ALARM )
   {
      u32 uAlarmIndex = 0;
      u32 uAlarm = 0;
      u32 uFlags1 = 0;
      u32 uFlags2 = 0;
      char szBuff[256];
      if ( pPH->total_length == (int)(sizeof(t_packet_header) + 3 * sizeof(u32)) )
      {
         memcpy(&uAlarm, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy(&uFlags1, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         memcpy(&uFlags2, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
         alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);
         log_line("Received alarm from vehicle (old format). Alarm: %s, alarm index: %u", szBuff, uAlarmIndex);
      }
      // New format, version 7.5
      else if ( pPH->total_length == (int)(sizeof(t_packet_header) + 4 * sizeof(u32)) )
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
         log_softerror_and_alarm("Received invalid alarm from vehicle. Received %d bytes, expected %d bytes", pPH->total_length, (int)sizeof(t_packet_header) + 4 * (int)sizeof(u32));
         return 0;
      }

      s_uTimeLastReceivedAlarm = g_TimeNow;
      
      if ( uAlarm & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET )
         s_bReceivedInvalidRadioPackets = true;

      if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_LOST )
      {
         g_bIsVehicleLinkToControllerLost = true;
         log_line("Received alarm that vehicle lost the connection from controller.");
      }
      if ( uAlarm & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
      {
         g_bIsVehicleLinkToControllerLost = false;
         log_line("Received alarm that vehicle recovered the connection from controller.");
      }
      
      log_line("Send alarm to central");
      ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, pPH->total_length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      log_line("Sent alarm to central");
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
   if ( pPH->total_length >= sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32) )
   {
      g_uTimeLastReceivedResponseToAMessage = g_TimeNow;
      radio_stats_set_received_response_from_vehicle_now(&g_SM_RadioStats, g_TimeNow);
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
         for( int k=0; k<MAX_RUNTIME_INFO_PINGS_HISTORY; k++ )
         {
            if ( uPingId == g_State.vehiclesRuntimeInfo[iIndex].uLastPingIdSentToVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId][k] )
            if ( uPingId != g_State.vehiclesRuntimeInfo[iIndex].uLastPingIdReceivedFromVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] )
            {
               u32 uRoundtripMicros = get_current_timestamp_micros() - g_State.vehiclesRuntimeInfo[iIndex].uTimeLastPingSentToVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId][k];
               u32 uRoundtripMilis = uRoundtripMicros / 1000;
               g_State.vehiclesRuntimeInfo[iIndex].uLastPingIdReceivedFromVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] = uPingId;
               g_State.vehiclesRuntimeInfo[iIndex].uTimeLastPingReplyReceivedFromVehicleOnLocalRadioLinks[uOriginalLocalRadioLinkId] = g_TimeNow;

               g_State.vehiclesRuntimeInfo[iIndex].uPingRoundtripTimeOnLocalRadioLinks[uOriginalLocalRadioLinkId] = uRoundtripMicros;
               g_SM_RouterVehiclesRuntimeInfo.uPingRoundtripTimeVehiclesOnLocalRadioLinks[iIndex][uOriginalLocalRadioLinkId] = uRoundtripMicros;
               addLinkRTTimeToRuntimeInfoIndex(iIndex, (int)uOriginalLocalRadioLinkId, uRoundtripMilis, uVehicleLocalTimeMs);

               if ( NULL != g_pProcessStats )
                  g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
               break;
            }
         }
      }
      else
         log_softerror_and_alarm("Received ping reply from unknown VID: %u", pPH->vehicle_id_src);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_MODEL_SETTINGS )
   {
      if ( -1 != g_fIPCToCentral )
         ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length);
      else
         log_softerror_and_alarm("Received message with current compressed model settings (%d bytes) but there is not channel to central to notify it.", pPH->total_length - sizeof(t_packet_header));

      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_LOG_FILE_SEGMENT )
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
   return 0;
}

void _process_received_single_packet_while_searching(int interfaceIndex, u8* pData, int length)
{
   t_packet_header* pPH = (t_packet_header*)pData;
      
   // Ruby telemetry is always sent to central and rx telemetry (to populate Ruby telemetry shared object)
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
   {
      // v1 ruby telemetry
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v1) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
         t_packet_header_ruby_telemetry_extended_v1* pPHRTE = (t_packet_header_ruby_telemetry_extended_v1*)(pData + sizeof(t_packet_header));
         u8 vMaj = pPHRTE->version;
         u8 vMin = pPHRTE->version;
         vMaj = vMaj >> 4;
         vMin = vMin & 0x0F;
         if ( (g_TimeNow >= s_TimeLastLoggedSearchingRubyTelemetry + 2000) || (s_TimeLastLoggedSearchingRubyTelemetryVehicleId != pPH->vehicle_id_src) )
         {
            s_TimeLastLoggedSearchingRubyTelemetry = g_TimeNow;
            s_TimeLastLoggedSearchingRubyTelemetryVehicleId = pPH->vehicle_id_src;
            char szFreq1[64];
            char szFreq2[64];
            char szFreq3[64];
            strcpy(szFreq1, str_format_frequency(pPHRTE->radio_frequencies[0] & 0x7FFF));
            strcpy(szFreq2, str_format_frequency(pPHRTE->radio_frequencies[1] & 0x7FFF));
            strcpy(szFreq3, str_format_frequency(pPHRTE->radio_frequencies[2] & 0x7FFF));
            log_line("Received a Ruby telemetry packet (version 1) while searching: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
             pPHRTE->uVehicleId, vMaj, vMin, pPHRTE->radio_links_count, 
             szFreq1, szFreq2, szFreq3 );
         }
         if ( -1 != g_fIPCToCentral )
            ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
      // v2 ruby telemetry
      else if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v2) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
      {
         t_packet_header_ruby_telemetry_extended_v2* pPHRTE = (t_packet_header_ruby_telemetry_extended_v2*)(pData + sizeof(t_packet_header));
         u8 vMaj = pPHRTE->version;
         u8 vMin = pPHRTE->version;
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
            log_line("Received a Ruby telemetry packet (version 2) while searching: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
             pPHRTE->uVehicleId, vMaj, vMin, pPHRTE->radio_links_count, 
             szFreq1, szFreq2, szFreq3 );
         }
         if ( -1 != g_fIPCToCentral )
            ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
      // v3 ruby telemetry
      bool bIsV3 = false;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3)))
         bIsV3 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info)))
         bIsV3 = true;
      if ( pPH->total_length == ((u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended_v3) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info) + (u16)sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)))
         bIsV3 = true;
      if ( bIsV3 )
      {
         t_packet_header_ruby_telemetry_extended_v3* pPHRTE = (t_packet_header_ruby_telemetry_extended_v3*)(pData + sizeof(t_packet_header));
         u8 vMaj = pPHRTE->version;
         u8 vMin = pPHRTE->version;
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
            log_line("Received a Ruby telemetry packet (version 3) while searching: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
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

void _parse_single_packet_h264_data(u8* pPacketData, bool bIsRelayed)
{
   if ( NULL == pPacketData )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacketData;
   t_packet_header_video_full_77* pPHVF = (t_packet_header_video_full_77*) (pPacketData+sizeof(t_packet_header));
   int iVideoDataLength = pPHVF->video_data_length;    
   u8* pData = pPacketData + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_77);

   bool bStartOfFrameDetected = s_ParserH264RadioInput.parseData(pData, iVideoDataLength, g_TimeNow);
   if ( ! bStartOfFrameDetected )
      return;
   
   if ( g_iDebugShowKeyFramesAfterRelaySwitch > 0 )
   if ( s_ParserH264RadioInput.IsInsideIFrame() )
   {
      log_line("[Debug] Received video keyframe from VID %u after relay switch.", pPH->vehicle_id_src);
      g_iDebugShowKeyFramesAfterRelaySwitch--;
   }

   u32 uLastFrameDuration = s_ParserH264RadioInput.getTimeDurationOfLastCompleteFrame();
   if ( uLastFrameDuration > 127 )
      uLastFrameDuration = 127;
   if ( uLastFrameDuration < 1 )
      uLastFrameDuration = 1;

   u32 uLastFrameSize = s_ParserH264RadioInput.getSizeOfLastCompleteFrame();
   uLastFrameSize /= 1000; // transform to kbytes

   if ( uLastFrameSize > 127 )
      uLastFrameSize = 127; // kbytes

   g_SM_VideoInfoStatsRadioIn.uLastIndex = (g_SM_VideoInfoStatsRadioIn.uLastIndex+1) % MAX_FRAMES_SAMPLES;
   g_SM_VideoInfoStatsRadioIn.uFramesDuration[g_SM_VideoInfoStatsRadioIn.uLastIndex] = uLastFrameDuration;
   g_SM_VideoInfoStatsRadioIn.uFramesTypesAndSizes[g_SM_VideoInfoStatsRadioIn.uLastIndex] = (g_SM_VideoInfoStatsRadioIn.uFramesTypesAndSizes[g_SM_VideoInfoStatsRadioIn.uLastIndex] & 0x80) | ((u8)uLastFrameSize);
 
   u32 uNextIndex = (g_SM_VideoInfoStatsRadioIn.uLastIndex+1) % MAX_FRAMES_SAMPLES;
  
   if ( s_ParserH264RadioInput.IsInsideIFrame() )
      g_SM_VideoInfoStatsRadioIn.uFramesTypesAndSizes[uNextIndex] |= (1<<7);
   else
      g_SM_VideoInfoStatsRadioIn.uFramesTypesAndSizes[uNextIndex] &= 0x7F;

   g_SM_VideoInfoStatsRadioIn.uKeyframeIntervalMs = s_ParserH264RadioInput.getCurrentlyDetectedKeyframeIntervalMs();
   g_SM_VideoInfoStatsRadioIn.uDetectedFPS = s_ParserH264RadioInput.getDetectedFPS();
   g_SM_VideoInfoStatsRadioIn.uDetectedSlices = (u32) s_ParserH264RadioInput.getDetectedSlices();
}


void _check_update_first_pairing_done_if_needed(int iInterfaceIndex, t_packet_header *pPH)
{
   if ( g_bFirstModelPairingDone || g_bSearching || (NULL == pPH) )
      return;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   u32 uCurrentFrequencyKhz = 0;
   if ( NULL != pRadioHWInfo )
   {
      uCurrentFrequencyKhz = pRadioHWInfo->uCurrentFrequencyKhz;
      log_line("Received first radio packet (packet index %u) (from VID %u) on radio interface %d, freq: %s, and first pairing was not done. Do first pairing now.",
       pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, pPH->vehicle_id_src, iInterfaceIndex+1, str_format_frequency(uCurrentFrequencyKhz));
   }
   else
      log_line("Received first radio packet (packet index %u) (from VID %u) on radio interface %d and first pairing was not done. Do first pairing now.", pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, pPH->vehicle_id_src, iInterfaceIndex+1);

   log_line("Current router local model VID: %u, ptr: %X, models current model: VID: %u, ptr: %X",
      g_pCurrentModel->uVehicleId, g_pCurrentModel,
      getCurrentModel()->uVehicleId, getCurrentModel());
   g_bFirstModelPairingDone = true;
   g_pCurrentModel->uVehicleId = pPH->vehicle_id_src;
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
   g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[0] = pPH->vehicle_id_src;
   g_State.vehiclesRuntimeInfo[0].uVehicleId = pPH->vehicle_id_src;
   
   // Notify central
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_FIRST_PAIRING_DONE, STREAM_ID_DATA);
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   
   if ( ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length) )
      log_line("Sent notification to central that first parining was done.");
   else
      log_softerror_and_alarm("Failed to send notification to central that first parining was done.");
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}


// Returns 1 if end of a video block was reached
// Returns -1 if the packet is not for this vehicle or was not processed
int _process_received_video_data_packet(int iInterfaceIndex, u8* pPacket, int iPacketLength)
{
   t_packet_header* pPH = (t_packet_header*)pPacket;
   u32 uVehicleId = pPH->vehicle_id_src;
   Model* pModel = findModelWithId(uVehicleId, 111);
   bool bIsRelayedPacket = relay_controller_is_vehicle_id_relayed_vehicle(g_pCurrentModel, uVehicleId);
   
   if ( NULL == pModel )
      return -1;

   // Did we received a new video stream? Add info for it.

   t_packet_header_video_full_77* pPHVF = (t_packet_header_video_full_77*) (pPacket+sizeof(t_packet_header));
   u8 uVideoStreamIndexAndType = pPHVF->video_stream_and_type;
   u8 uVideoStreamIndex = (uVideoStreamIndexAndType & 0x0F);
   u8 uVideoStreamType = ((uVideoStreamIndexAndType & 0xF0) >> 4);

   int iRuntimeIndex = -1;
   int iFirstFreeSlot = -1;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL == g_pVideoProcessorRxList[i] )
      {
         iFirstFreeSlot = i;
         break;
      }
      if ( g_pVideoProcessorRxList[i]->m_uVehicleId == uVehicleId )
      if ( g_pVideoProcessorRxList[i]->m_uVideoStreamIndex == (uVideoStreamIndexAndType & 0x0F) )
      {
         iRuntimeIndex = i;
         break;
      }
   }

   // New video stream
   if ( -1 == iRuntimeIndex )
   {
      if ( -1 == iFirstFreeSlot )
      {
         log_softerror_and_alarm("No more free slots to create new video rx processor, for VID: %u, video stream index %d", uVehicleId, (uVideoStreamIndexAndType & 0x0F));
         return -1;
      }
      u32 uVideoBlockIndex = pPHVF->video_block_index;
      log_line("Start receiving new video stream: type: %d, profile: %d, %s, res: %dx%d, fps: %d, keyframe: %d, scheme: %d/%d, video data size: %d",
         uVideoStreamType, pPHVF->video_link_profile & 0x0F, str_get_video_profile_name(pPHVF->video_link_profile & 0x0F),
         pPHVF->video_width, pPHVF->video_height, pPHVF->video_fps,
         pPHVF->video_keyframe_interval_ms,
         pPHVF->block_packets, pPHVF->block_fecs, pPHVF->video_data_length);
      log_line("Create new video Rx processor for VID %u, firmware type: %s, video stream %d, video stream type: %d, at radio packet index: %u, video block index: %u",
         uVehicleId, str_format_firmware_type(pModel->getVehicleFirmwareType()),
         (int)uVideoStreamIndex, (int)uVideoStreamType, pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, uVideoBlockIndex);
         g_pVideoProcessorRxList[iFirstFreeSlot] = new ProcessorRxVideo(uVehicleId, (u32)(uVideoStreamIndexAndType & 0x0F));
      
      g_pVideoProcessorRxList[iFirstFreeSlot]->init();
      iRuntimeIndex = iFirstFreeSlot;
   }

   if ( ! ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
   if ( pPHVF->video_block_packet_index < pPHVF->block_packets )
   if ( uVideoStreamType == VIDEO_TYPE_H264 )
   if ( pModel->osd_params.osd_flags[pModel->osd_params.layout] & OSD_FLAG_SHOW_STATS_VIDEO_KEYFRAMES_INFO )
   if ( get_ControllerSettings()->iShowVideoStreamInfoCompactType == 0 )
      _parse_single_packet_h264_data(pPacket, bIsRelayedPacket);

   if ( ! (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
   if ( pPHVF->encoding_extra_flags2 & ENCODING_EXTRA_FLAGS2_HAS_DEBUG_TIMESTAMPS )
   if ( pPHVF->video_block_packet_index < pPHVF->block_packets)
   {
      u8* pExtraData = pPacket + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_77) + pPHVF->video_data_length;
      u32* pExtraDataU32 = (u32*)pExtraData;
      pExtraDataU32[5] = get_current_timestamp_ms();
   }

   int nRet = 0;

   if ( bIsRelayedPacket )
   {
      if ( relay_controller_must_display_remote_video(g_pCurrentModel) )
         nRet = g_pVideoProcessorRxList[iRuntimeIndex]->handleReceivedVideoPacket(iInterfaceIndex, pPacket, pPH->total_length);
   }
   else
   {
      if ( relay_controller_must_display_main_video(g_pCurrentModel) )
         nRet = g_pVideoProcessorRxList[iRuntimeIndex]->handleReceivedVideoPacket(iInterfaceIndex, pPacket, pPH->total_length);
   }
   return nRet;
}

// Returns 1 if end of a video block was reached
// Returns -1 if the packet is not for this vehicle or was not processed

int process_received_single_radio_packet(int interfaceIndex, u8* pData, int length)
{
   t_packet_header* pPH = (t_packet_header*)pData;
     
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioRxTime = g_TimeNow;


   #ifdef PROFILE_RX
   u32 timeStart = get_current_timestamp_ms();
   #endif

   _check_update_first_pairing_done_if_needed(interfaceIndex, pPH);

   // Searching ?
   
   if ( g_bSearching )
   {
      _process_received_single_packet_while_searching(interfaceIndex, pData, length);
      return 0;
   }
   
   if ( (0 == pPH->vehicle_id_src) || (MAX_U32 == pPH->vehicle_id_src) )
      log_softerror_and_alarm("Received invalid radio packet: Invalid source vehicle id: %u (vehicle id dest: %u, packet type: %s)",
         pPH->vehicle_id_src, pPH->vehicle_id_dest, str_get_packet_type(pPH->packet_type));

   u32 uVehicleId = pPH->vehicle_id_src;
   u32 uStreamId = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   if ( uStreamId >= MAX_RADIO_STREAMS )
      uStreamId = 0;

   bool bNewVehicleId = true;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( 0 == g_State.vehiclesRuntimeInfo[i].uVehicleId )
         break;
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
      {
         bNewVehicleId = false;
         break;
      }
   }

   if ( bNewVehicleId )
   {
      log_line("Start receiving radio packets from new VID: %u", pPH->vehicle_id_src);
      int iCount = 0;
      int iFirstFree = -1;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_State.vehiclesRuntimeInfo[i].uVehicleId != 0 )
            iCount++;

         if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == 0 )
         if ( -1 == iFirstFree )
            iFirstFree = i;
      }

      logCurrentVehiclesRuntimeInfo();

      if ( (iCount >= MAX_CONCURENT_VEHICLES) || (-1 == iFirstFree) )
         log_softerror_and_alarm("No more room to store the new VID: %u", pPH->vehicle_id_src);
      else
      {
         resetVehicleRuntimeInfo(iFirstFree);
         g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[iFirstFree] = uVehicleId;
         g_State.vehiclesRuntimeInfo[iFirstFree].uVehicleId = uVehicleId;
      }
   }

   // Detect vehicle restart (stream packets indexes are starting again from zero or low value )

   if ( radio_dup_detection_is_vehicle_restarted(uVehicleId) )
   {
      log_line("Vehicle restarted (received stream packet index %u for stream %d, on interface index %d; Max received stream packet index was at %u for stream %d",
       (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), uStreamId, interfaceIndex+1, radio_dup_detection_get_max_received_packet_index_for_stream(uVehicleId, uStreamId), uStreamId );
      g_pProcessStats->alarmFlags = PROCESS_ALARM_RADIO_STREAM_RESTARTED;
      g_pProcessStats->alarmTime = g_TimeNow;

      int iRuntimeIndex = getVehicleRuntimeIndex(uVehicleId);
      if ( -1 != iRuntimeIndex )
      {
         // Reset pairing info so that pairing is done again with this vehicle
         resetPairingStateForVehicleRuntimeInfo(iRuntimeIndex);
      }
      for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      {
         if ( NULL == g_pVideoProcessorRxList[i] )
            break;
         if ( g_pVideoProcessorRxList[i]->m_uVehicleId == uVehicleId )
         {
            g_pVideoProcessorRxList[i]->resetState();
            break;
         }
      }
      radio_dup_detection_reset_vehicle_restarted_flag(uVehicleId);
   }
   
   // For data streams, discard packets that are too older than the newest packet received on that stream for that vehicle
   
   u32 uMaxStreamPacketIndex = radio_dup_detection_get_max_received_packet_index_for_stream(uVehicleId, uStreamId);
   
   if ( (uStreamId < STREAM_ID_VIDEO_1) && (uMaxStreamPacketIndex > 50) )
   if ( (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) < uMaxStreamPacketIndex-50 )
   {
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
      {
         t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pData + sizeof(t_packet_header));

         if ( pPHCR->origin_command_type == COMMAND_ID_GET_ALL_PARAMS_ZIP )
         {
            log_line("Discarding received command response for get zip model settings, command counter %d, command retry counter %d, on radio interface %d. Discard because this packet index is %u and last received packet index for this data stream (stream %d) is %u",
                pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter,
                interfaceIndex+1,
                (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), uStreamId, uMaxStreamPacketIndex);
         }
      }
      return 0;
   }

   bool bIsRelayedPacket = false;

   // Received packet from different vehicle ?

   if ( (NULL != g_pCurrentModel) && (pPH->vehicle_id_src != g_pCurrentModel->uVehicleId) )
   {
      // Relayed packet?
      if ( !g_bSearching )
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( (g_pCurrentModel->relay_params.uRelayedVehicleId != 0) && (g_pCurrentModel->relay_params.uRelayedVehicleId != MAX_U32) )
      if ( pPH->vehicle_id_src == g_pCurrentModel->relay_params.uRelayedVehicleId )
         bIsRelayedPacket = true;

      /*
      if ( bIsRelayedPacket )
      {
         relay_rx_process_single_received_packet( interfaceIndex, pData, length);

         #ifdef PROFILE_RX
         u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
         if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
            log_softerror_and_alarm("[Profile-Rx] Processing ruby message from single relayed radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTimeEnd);
         #endif

         return 0;
      }
      */

      // Discard all packets except telemetry comming from wrong models, if the first pairing was done

      if ( !bIsRelayedPacket )
      {
         // Ruby telemetry is always sent to central, so that wrong vehicle or relayed vehicles can be detected
         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
         if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED || 
              pPH->packet_type == PACKET_TYPE_FC_TELEMETRY ||
              pPH->packet_type == PACKET_TYPE_FC_TELEMETRY_EXTENDED )
         {
            if ( -1 != g_fIPCToCentral )
               ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
            if ( NULL != g_pProcessStats )
               g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         }
         #ifdef PROFILE_RX
         u32 dTime1 = get_current_timestamp_ms() - timeStart;
         if ( dTime1 >= PROFILE_RX_MAX_TIME )
            log_softerror_and_alarm("[Profile-Rx] Processing single radio packet (type: %d len: %d bytes) from different vehicle, from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTime1);
         #endif
         
         return 0;
      }
   }

   if ( g_bDebugIsPacketsHistoryGraphOn && (!g_bDebugIsPacketsHistoryGraphPaused) )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);

      int cRetr = 0;
      int cVideo = 0;
      int cTelem = 0;
      int cRC = 0;
      int cPing = 0;
      int cOther = 0;
      if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
         cRetr = 1;

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
         cVideo = 1;
      else if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
         cTelem = 1;
      else if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RC )
         cRC = 1;
      else if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
      {
         if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
            cPing = 1;
         else
            cOther = 1;
      }
      else
         cOther = 1;
      add_detailed_history_rx_packets(g_pDebug_SM_RouterPacketsStatsHistory, g_TimeNow % 1000, cVideo, cRetr, cTelem, cRC, cPing, cOther, pRadioHWInfo->monitor_interface_read.radioInfo.nDataRateBPSMCS);
      #ifdef PROFILE_RX
      u32 dTimeDbg = get_current_timestamp_ms() - timeStart;
      if ( dTimeDbg >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Adding debug stats for single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTimeDbg);
      #endif
   }

   //log_line("Recv packet: index: %u, interfaceIndex: %d,  stream id: %u, len: %d", (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), interfaceIndex, uStreamId, pPH->total_length);

   if ( ! bIsRelayedPacket )
   if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
   {
      u8* pEnd = pData + pPH->total_length;
      pEnd--;
      u8 size = *pEnd;
      pEnd--;
      u8 type = *pEnd;
      pEnd -= size-2;
      _process_extra_data_from_packet(type, size, pEnd);

      #ifdef PROFILE_RX
      u32 dTimeEx = get_current_timestamp_ms() - timeStart;
      if ( dTimeEx >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing extra info from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTimeEx);
      #endif

   }

   //if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_VIDEO )
   //   log_line("Received packet from vehicle: module: %d, type: %d, length: %d", (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE), pPH->packet_type, pPH->total_length);

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
   {
      if ( bIsRelayedPacket )
      {
         if ( (pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY) ||
              (pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION) )
            _process_received_ruby_message(interfaceIndex, pData);
         return 0;
      }
      _process_received_ruby_message(interfaceIndex, pData);

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing ruby message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   {
      if ( pPH->packet_type != PACKET_TYPE_COMMAND_RESPONSE )
         return 0;

      t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pData + sizeof(t_packet_header));
      g_uTimeLastReceivedResponseToAMessage = g_TimeNow;
      radio_stats_set_received_response_from_vehicle_now(&g_SM_RadioStats, g_TimeNow);

      if ( pPHCR->origin_command_type == COMMAND_ID_GET_ALL_PARAMS_ZIP )
      {
         log_line("[Router] Received command response for get zip model settings, command counter %d, command retry counter %d, %d extra bytes, packet index: %u, on radio interface %d.",
            pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter,
            pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response),
            pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX,
            interfaceIndex+1);
      }
      
      if ( bIsRelayedPacket )
         return 0;

      ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      
      type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(pPH->vehicle_id_src);
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

               if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
                  continue;

               int nRadioLinkId = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;
               if ( nRadioLinkId < 0 || nRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
                  continue;
               if ( nRadioLinkId != (int)g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand )
                  continue;

               if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[nRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
                  continue;

               if ( NULL != pRadioHWInfo )
               if ( ((pRadioHWInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS) ||
                    ((pRadioHWInfo->typeAndDriver & 0xFF) == RADIO_TYPE_RALINK) )
               {
                  int nRateTx = controllerGetCardDataRate(pRadioHWInfo->szMAC); // Returns 0 if radio link datarate must be used (no custom datarate set for this radio card);
                  if ( 0 == nRateTx )
                     nRateTx = g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand;
                  update_atheros_card_datarate(g_pCurrentModel, i, nRateTx, g_pProcessStats);
                  g_TimeNow = get_current_timestamp_ms();
               }
            }
         }
      }

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing command message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   {
      if ( ! bIsRelayedPacket )
      if ( ! g_bSearching )
      if ( g_bFirstModelPairingDone )
      if ( -1 != g_fIPCToTelemetry )
      {
         #ifdef LOG_RAW_TELEMETRY
         if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD )
         {
            t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)(pData + sizeof(t_packet_header));
            log_line("[Raw_Telem] Received raw telem packet from vehicle, index %u, %d / %d bytes",
               pPHTR->telem_segment_index, pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_telemetry_raw), pPH->total_length);
         }
         #endif
         ruby_ipc_channel_send_message(g_fIPCToTelemetry, pData, length);
      }
      bool bSendToCentral = false;
      bool bSendRelayedTelemetry = false;

      // Ruby telemetry and FC telemetry from relayed vehicle is always sent to central to detect the relayed vehicle
      if ( bIsRelayedPacket )
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
      if ( (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_SHORT) ||
           (pPH->packet_type == PACKET_TYPE_FC_TELEMETRY) ||
           (pPH->packet_type == PACKET_TYPE_FC_TELEMETRY_EXTENDED) )
          bSendToCentral = true;

      if ( bIsRelayedPacket )
      if ( relay_controller_must_forward_telemetry_from(g_pCurrentModel, pPH->vehicle_id_src) )
         bSendRelayedTelemetry = true;

      if ( (! bIsRelayedPacket) || bSendRelayedTelemetry )
      if ( (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_SHORT) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) ||
           (pPH->packet_type == PACKET_TYPE_FC_TELEMETRY) ||
           (pPH->packet_type == PACKET_TYPE_FC_TELEMETRY_EXTENDED) ||
           (pPH->packet_type == PACKET_TYPE_FC_RC_CHANNELS) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY ) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS ) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS) ||
           (pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY) )
         bSendToCentral = true;

      if ( bSendToCentral )
      {
         ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
        
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

         #ifdef PROFILE_RX
         u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
         if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
            log_softerror_and_alarm("[Profile-Rx] Processing telemetry message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTimeEnd);
         #endif
         return 0;
      }

      if ( bIsRelayedPacket )
         return 0;

      if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS )
      if ( NULL != g_pSM_VideoLinkStats )
      if ( pPH->total_length == sizeof(t_packet_header) + sizeof(shared_mem_video_link_stats_and_overwrites) )
         memcpy(g_pSM_VideoLinkStats, pData+sizeof(t_packet_header), sizeof(shared_mem_video_link_stats_and_overwrites) );

      if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_GRAPHS )
      if ( NULL != g_pSM_VideoLinkGraphs )
      if ( pPH->total_length == sizeof(t_packet_header) + sizeof(shared_mem_video_link_graphs) )
         memcpy(g_pSM_VideoLinkGraphs, pData+sizeof(t_packet_header), sizeof(shared_mem_video_link_graphs) );

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing telemetry message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RC )
   {
      if ( bIsRelayedPacket )
         return 0;

      if ( -1 != g_fIPCToRC )
         ruby_ipc_channel_send_message(g_fIPCToRC, pData, length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing RC message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   {
      if ( g_bSearching )
         return 0;

      if ( pPH->packet_type == PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK )
      {
         u32 uLevel = 0;
         memcpy((u8*)&uLevel, pData + sizeof(t_packet_header), sizeof(u32));
         
         if ( uLevel == MAX_U32 )
            return 0;

         bool bMatched = false;
         int iIndex = getVehicleRuntimeIndex(uVehicleId);
         if ( -1 != iIndex )
         {
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedLevelShift = (int) uLevel;
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastAckLevelShift = g_TimeNow;
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedLevelShiftRetryCount = 0;
            bMatched = true;
            log_line("Received ACK (packet index: %u) from VID %u for setting adaptive video level to %u", pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, pPH->vehicle_id_src, uLevel);
         }
         if ( ! bMatched )
            log_softerror_and_alarm("Received ACK from unknown vehicle (uid: %u) for setting adaptive video level to %u", uVehicleId, uLevel);
         //send_alarm_to_central(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_RECEIVED_DEPRECATED_MESSAGE, PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK);
         return 0;
      }

      if ( pPH->packet_type == PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK )
      {
         u32 uKeyframeMs = 0;
         memcpy((u8*)&uKeyframeMs, pData + sizeof(t_packet_header), sizeof(u32));
         bool bMatched = false;
         int iIndex = getVehicleRuntimeIndex(uVehicleId);
         if ( -1 != iIndex )
         {
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedKeyFrameMs = (int) uKeyframeMs;
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedKeyFrameMsRetryCount = 0;
            bMatched = true;
            log_line("Received ACK (packet index: %u) from VID %u for setting keyframe to %u ms", pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, pPH->vehicle_id_src, uKeyframeMs);
         }
         if ( ! bMatched )
            log_softerror_and_alarm("Received ACK from unknown vehicle (uid: %u) for setting keyframe to %u ms", uVehicleId, uKeyframeMs);
         
         //send_alarm_to_central(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_RECEIVED_DEPRECATED_MESSAGE, PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK);
         return 0;
      }

      if ( pPH->packet_type != PACKET_TYPE_VIDEO_DATA_FULL )
      {
         //log_line("Received unknown video packet type.");
         return 0;
      }

      int nRet = _process_received_video_data_packet(interfaceIndex, pData, pPH->total_length);

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing video message from single radio packet (type: %d len: %d/%d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, length, interfaceIndex+1, (int)dTimeEnd);
      #endif

      return nRet;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_AUDIO )
   {
      if ( bIsRelayedPacket || g_bSearching )
         return 0;

      if ( pPH->packet_type != PACKET_TYPE_AUDIO_SEGMENT )
      {
         //log_line("Received unknown video packet type.");
         return 0;
      }
      process_received_audio_packet(pData);

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing audio message from single radio packet (type: %d len: %d/%d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, length, interfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   #ifdef PROFILE_RX
   u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
   if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Processing other type of message from single radio packet (type: %d len: %d/%d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, length, interfaceIndex+1, (int)dTimeEnd);
   #endif

   return 0;
} 
