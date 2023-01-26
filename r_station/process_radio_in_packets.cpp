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

#include "process_radio_in_packets.h"
#include "processor_rx_audio.h"
#include "processor_rx_video.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/launchers.h"
#include "../base/ctrl_interfaces.h"
#include "../base/encr.h"
#include "../base/commands.h"
#include "../base/ruby_ipc.h"
#include "../base/camera_utils.h"
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"
#include "links_utils.h"
#include "shared_vars.h"
#include "timers.h"

#define MAX_PACKETS_IN_ID_HISTORY 6

fd_set s_ReadSetRXRadio;
u32 s_uRadioRxReadTimeoutCount = 0;
u32 s_StreamsMaxReceivedPacketIndex[MAX_RADIO_STREAMS];

u32 s_uTotalBadPacketsReceived = 0;

static u32 s_TimeLastLogWrongRxPacket = 0;
static u32 s_TimeLastDisplaySearchRubyTelemetry = 0;
static u32 s_TimeLastReceivedModelSettings = 0;
static u32 s_LastReceivedAlarmCount = MAX_U32;

static bool s_bReceivedAnyValidPacket = false;

static u32 s_TimeLastLogAlarmStreamPacketsVariation = 0;


u32 s_uParseRadioInNALStartSequence = 0xFFFFFFFF;
u32 s_uParseRadioInNALCurrentSlices = 0;
u32 s_uParseRadioInLastNALTag = 0xFFFFFFFF;
u32 s_uParseRadioInNALTotalParsedBytes = 0;
u32 s_uLastRadioInVideoFrameTime = 0;

void init_radio_rx_structures()
{
   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      s_StreamsMaxReceivedPacketIndex[i] = MAX_U32;
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
      int freqNew = (int)freq;
      log_line("Received extra packet info to change frequency on radio link %d, new frequency: %d Mhz", nLink+1, freqNew);
      bool bChanged = false;
      if ( g_pCurrentModel->radioLinksParams.link_frequency[nLink] != freqNew )
         bChanged = true;

      if ( ! bChanged )
      {
         log_line("Same frequency as current one. Ignoring info from vehicle.");
         return;
      }
      int freqOld = g_pCurrentModel->radioLinksParams.link_frequency[nLink];
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNIC = hardware_get_radio_info(i);
         if ( NULL == pNIC || (0 == pNIC->isEnabled) )
            continue;
         if ( 0 == hardware_radioindex_supports_frequency(i, freqNew) )
            continue;

         u32 flags = controllerGetCardFlags(pNIC->szMAC);
         if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pNIC->szMAC) )
            continue;

         if ( pNIC->currentFrequency == freqOld )
         {
            launch_set_frequency(NULL, i, freqNew);
            g_Local_RadioStats.radio_interfaces[i].currentFrequency = freqNew;
            radio_stats_set_card_current_frequency(&g_Local_RadioStats, i, freqNew);
         }
      }
      hardware_save_radio_info();

      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));

      g_pCurrentModel->radioLinksParams.link_frequency[nLink] = freqNew;         
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, true);
      saveModel(g_pCurrentModel);
      log_line("Notifying all other components of the link frequency change.");

      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
      PH.packet_type = PACKET_TYPE_LOCAL_LINK_FREQUENCY_CHANGED;
      PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
      PH.vehicle_id_dest = 0;
      PH.total_headers_length = sizeof(t_packet_header);
      PH.total_length = sizeof(t_packet_header) + 2*sizeof(int);
      PH.extra_flags = 0;
      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      int* pI = (int*)((&buffer[0])+sizeof(t_packet_header));
      *pI = nLink;
      pI++;
      *pI = freqNew;
      packet_compute_crc(buffer, PH.total_length);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length);
      ruby_ipc_channel_send_message(g_fIPCToTelemetry, buffer, PH.total_length);
      log_line("Done notifying all other components about the link frequency change.");
   }
}

int _process_received_ruby_message(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_RUBY_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK )
   {
      u32 uLevel = 0;
      memcpy((u8*)&uLevel, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
      g_ControllerAdaptiveVideoInfo.iLastAcknowledgedLevelShift = (int) uLevel;
      g_ControllerAdaptiveVideoInfo.iLastRequestedLevelShiftRetryCount = 0;
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK )
   {
      u32 uKeyframe = 0;
      memcpy((u8*)&uKeyframe, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
      g_ControllerAdaptiveVideoInfo.iLastAcknowledgedKeyFrame = (int) uKeyframe;
      g_ControllerAdaptiveVideoInfo.iLastRequestedKeyFrameRetryCount = 0;
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION )
   {
      g_uTimeLastReceivedResponseToAMessage = g_TimeNow;

      u32 uResendCount = 0;
      if ( pPH->total_length >= pPH->total_headers_length + sizeof(u32) )
         memcpy(&uResendCount, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));

      log_line("Received pairing confirmation from vehicle (received vehicle resend counter: %u). VID: %u, CID: %u", uResendCount, pPH->vehicle_id_src, pPH->vehicle_id_dest);
      g_bRuntimeControllerPairingCompleted = true;
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_RADIO_REINITIALIZED )
   {
      log_line("Received message that vehicle radio interfaces where reinitialized.");
      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
      PH.packet_type =  PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED;
      PH.total_headers_length = sizeof(t_packet_header);
      PH.total_length = sizeof(t_packet_header);
      packet_compute_crc((u8*)&PH, sizeof(t_packet_header));
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)&PH, PH.total_length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_ALARM )
   {
      u32 uAlarms = 0;
      u32 uFlags = 0;
      u32 uAlarmsCount = 0;
      char szBuff[256];
      memcpy(&uAlarms, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
      memcpy(&uFlags, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
      memcpy(&uAlarmsCount, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u32));
      alarms_to_string(uAlarms, szBuff);
      log_line("Received alarm(s) from vehicle. Alarms: %s, flags: %u, count: %d.", szBuff, uFlags, (int)uAlarmsCount);
      if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET )
         s_bReceivedInvalidRadioPackets = true;

      if ( uAlarms & ALARM_ID_LINK_TO_CONTROLLER_LOST )
      {
         g_bIsVehicleLinkToControllerLost = true;
         log_line("Received alarm that vehicle lost the connection from controller.");
      }
      if ( uAlarms & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
      {
         g_bIsVehicleLinkToControllerLost = false;
         log_line("Received alarm that vehicle recovered the connection from controller.");
      }
      if ( s_LastReceivedAlarmCount != uAlarmsCount )
      {
         s_LastReceivedAlarmCount = uAlarmsCount;
         pPH->packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
         packet_compute_crc(pPacketBuffer, sizeof(t_packet_header)+3*sizeof(u32));
         ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, pPH->total_length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
   {
      g_uLastReceivedPingResponseTime = g_TimeNow;
      g_uTimeLastReceivedResponseToAMessage = g_TimeNow;
         u32 pingId = 0;
         u32 vehicleTime = 0;
         u8 radioLinkId = 0;
         memcpy(&pingId, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy(&vehicleTime, pPacketBuffer + sizeof(t_packet_header)+sizeof(u32), sizeof(u32));
         memcpy(&radioLinkId, pPacketBuffer + sizeof(t_packet_header)+2*sizeof(u32), sizeof(u8));

         if ( pingId == s_uLastPingSentId )
         if ( pingId != s_uLastPingRecvId )
         {
            s_uLastPingRecvId = pingId;
            s_uRoundtripLinkTimeMicroSec = get_current_timestamp_micros() - g_uLastPingSendTimeMicroSec;
            if ( s_uRoundtripLinkTimeMicroSec < s_uRoundtripLinkTimeMicroSecMinimum )
               s_uRoundtripLinkTimeMicroSecMinimum = s_uRoundtripLinkTimeMicroSec;
            //log_line("Received PING clock response (counter: %u), updated PING roundtrip: %.2f ms", pingId, s_uRoundtripLinkTimeMicroSec/1000.0f);
            radio_stats_set_radio_link_rt_delay(&g_Local_RadioStats, radioLinkId, s_uRoundtripLinkTimeMicroSec/1000);
            if ( NULL != g_pProcessStats )
               g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         }
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_MODEL_SETTINGS )
   {
      if ( 0 != s_TimeLastReceivedModelSettings )
      if ( g_TimeNow < s_TimeLastReceivedModelSettings + 5000 )
      {
         log_line("Ignoring received duplicate model settings (less than 5 sec since last one received).");
         return 0;
      }
      s_TimeLastReceivedModelSettings = g_TimeNow;

      if ( -1 != g_fIPCToCentral )
      {
         log_line("Received message with current compressed model settings. Notifying central to handle it.");
         ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length);
      }
      else
         log_softerror_and_alarm("Received message with current compressed model settings but there is not channel to central to notify it.");

      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_LOG_FILE_SEGMENT )
   {
      t_packet_header_file_segment* pPHFS = (t_packet_header_file_segment*)(pPacketBuffer + sizeof(t_packet_header));
      log_line("Received a log file segment: file id: %d, segment size: %d, segment index: %d", pPHFS->file_id, (int)pPHFS->segment_size, (int)pPHFS->segment_index);
      if ( pPHFS->file_id == FILE_ID_VEHICLE_LOG )
      {
         char szFile[128];
         snprintf(szFile, 127, LOG_FILE_VEHICLE, g_pCurrentModel->getShortName());
         FILE* fd = fopen(szFile, "ab");
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
         packet_compute_crc(pPacketBuffer, pPH->total_length);

         ruby_ipc_channel_send_message(g_fIPCToCentral, pPacketBuffer, pPH->total_length);

         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         return 0;
      }
      return 0;
   }
   return 0;
}

// Returns 1 if end of a video block was reached
// Returns -1 if the packet is not for this vehicle or was not processed

int _process_received_single_radio_packet(int interfaceIndex, u32 uStreamId, u8* pData, int length)
{
   t_packet_header* pPH = (t_packet_header*)pData;

   #ifdef PROFILE_RX
   u32 timeStart = get_current_timestamp_ms();
   #endif

   // Searching ?
   if ( g_bSearching )
   {
      // Ruby telemetry is always sent to central and rx telemetry (to populate Ruby telemetry shared object)
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
      if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
      {
         t_packet_header_ruby_telemetry_extended* pPHRTE = (t_packet_header_ruby_telemetry_extended*)(pData + sizeof(t_packet_header));
         u8 vMaj = pPHRTE->version;
         u8 vMin = pPHRTE->version;
         vMaj = vMaj >> 4;
         vMin = vMin & 0x0F;
         if ( g_TimeNow >= s_TimeLastDisplaySearchRubyTelemetry + 1000 )
         {
            s_TimeLastDisplaySearchRubyTelemetry = g_TimeNow;
            log_line("Received a Ruby telemetry packet while searching: vehicle ID: %u, version: %d.%d, radio links: %d Mhz, %d Mhz, %d Mhz", pPHRTE->vehicle_id, vMaj, vMin, pPHRTE->radio_links_count, pPHRTE->radio_frequencies[0], pPHRTE->radio_frequencies[1], pPHRTE->radio_frequencies[2] );
         }
         if ( -1 != g_fIPCToTelemetry )
            ruby_ipc_channel_send_message(g_fIPCToTelemetry, pData, length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
      #ifdef PROFILE_RX
      u32 dTime1 = get_current_timestamp_ms() - timeStart;
      if ( dTime1 >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing single radio packet (type: %d, len: %d bytes) while searcing, from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTime1);
      #endif

      return -1;
   }

   // Received packet from different vehicle ?
   // Then discard every type of packet except telemetry comming from wrong model, if the first pairing was done

   if ( g_bFirstModelPairingDone && (NULL != g_pCurrentModel) && (pPH->vehicle_id_src != g_pCurrentModel->vehicle_id) )
   {
      // Discard everything except telemetry
      // Ruby telemetry is always sent to central, so that wrong vehicle can be detected
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
      if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED || 
           pPH->packet_type == PACKET_TYPE_FC_TELEMETRY ||
           pPH->packet_type == PACKET_TYPE_FC_TELEMETRY_EXTENDED )
      {
         if ( -1 != g_fIPCToTelemetry )
            ruby_ipc_channel_send_message(g_fIPCToTelemetry, pData, length);
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      }
      #ifdef PROFILE_RX
      u32 dTime1 = get_current_timestamp_ms() - timeStart;
      if ( dTime1 >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing single radio packet (type: %d len: %d bytes) from different vehicle, from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTime1);
      #endif
      return -1;
   }

   // First time we received anything?

   if ( ! s_bReceivedAnyValidPacket )
   {
      log_line("Vehicle detected for the first time (on stream id %u, packet index %u). Initialize data and video data structures...", uStreamId, pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
      process_data_rx_video_reset_state();
      init_radio_rx_structures();
   }

   // Detect vehicle restart (stream packets indexes are starting again from zero or low value )

   if ( s_StreamsMaxReceivedPacketIndex[uStreamId] != MAX_U32 )
   {
      if (( s_StreamsMaxReceivedPacketIndex[uStreamId] > 10 ) && 
           ((pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) < s_StreamsMaxReceivedPacketIndex[uStreamId] - 10) )
      if ( g_TimeNow > s_TimeLastLogAlarmStreamPacketsVariation + 5000 )
      {
         s_TimeLastLogAlarmStreamPacketsVariation = g_TimeNow;
         log_line("[Profile-Rx] Received stream %d packet index %u, interface %d, is older than max packet for the stream (%u).", (int)uStreamId, (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), interfaceIndex+1, s_StreamsMaxReceivedPacketIndex[uStreamId]);
      }

      bool bRestarted = false;
      if ( ( s_StreamsMaxReceivedPacketIndex[uStreamId] > 5000 ) && 
           ((pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) < s_StreamsMaxReceivedPacketIndex[uStreamId] - 5000) )
         bRestarted = true;
      
      if ( bRestarted )
      {
         g_pProcessStats->alarmFlags = PROCESS_ALARM_RADIO_STREAM_RESTARTED;
         g_pProcessStats->alarmTime = g_TimeNow;

         log_line("Vehicle restarted (received stream packet index %u for stream %d, on interface index %d, max received stream packet index was at %u for stream %d", (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), uStreamId, interfaceIndex, s_StreamsMaxReceivedPacketIndex[uStreamId], uStreamId );
         process_data_rx_video_reset_state();
         init_radio_rx_structures();
         s_LastReceivedAlarmCount = MAX_U32;
      }

   }
   if ( (s_StreamsMaxReceivedPacketIndex[uStreamId] == MAX_U32) || ((pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) > s_StreamsMaxReceivedPacketIndex[uStreamId]) )
      s_StreamsMaxReceivedPacketIndex[uStreamId] = (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);

   if ( g_bDebugIsPacketsHistoryGraphOn && (!g_bDebugIsPacketsHistoryGraphPaused) )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(interfaceIndex);

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
      add_detailed_history_rx_packets(g_pDebug_SM_RouterPacketsStatsHistory, g_TimeNow % 1000, cVideo, cRetr, cTelem, cRC, cPing, cOther, pNICInfo->monitor_interface_read.radioInfo.nRate/2);
      #ifdef PROFILE_RX
      u32 dTimeDbg = get_current_timestamp_ms() - timeStart;
      if ( dTimeDbg >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Adding debug stats for single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTimeDbg);
      #endif
   }

   //log_line("Recv packet: index: %u, interfaceIndex: %d,  stream id: %u, len: %d", (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), interfaceIndex, uStreamId, pPH->total_length);

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
      _process_received_ruby_message(pData);

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing ruby message from single radio packet (type: %d len: %d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, interfaceIndex+1, (int)dTimeEnd);
      #endif

      return 0;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   {
      ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pData + sizeof(t_packet_header));
      
      if ( pPHCR->origin_command_counter == g_uLastCommandRequestIdSent && g_uLastCommandRequestIdSent != MAX_U32 )
      if ( pPHCR->origin_command_resend_counter == g_uLastCommandRequestIdRetrySent && g_uLastCommandRequestIdRetrySent != MAX_U32 )
      {
         u32 dTime = g_TimeNow - g_uTimeLastCommandRequestSent;
         g_uLastCommandRequestIdSent = MAX_U32;
         g_uLastCommandRequestIdRetrySent = MAX_U32;
         radio_stats_set_commands_rt_delay(&g_Local_RadioStats, dTime);
      }

      if ( pPHCR->origin_command_type == COMMAND_ID_SET_RADIO_LINK_FLAGS )
      if ( pPHCR->command_response_flags & COMMAND_RESPONSE_FLAGS_OK)
      {
         if ( pPHCR->origin_command_counter != g_uLastCommandCounterToSetRadioFlags )
         {
            log_line("Intercepted Ok response to command to set radio data datarate on radio link %u to %d Mbps (%d datarate).", g_uLastRadioLinkIndexSentRadioCommand, getDataRateRealMbRate(g_iLastRadioLinkDataDataRateSentRadioCommand), g_iLastRadioLinkDataDataRateSentRadioCommand);
            g_uLastCommandCounterToSetRadioFlags = pPHCR->origin_command_counter;

            for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
            {
               radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);

               if ( controllerIsCardDisabled(pNICInfo->szMAC) )
                  continue;

               int nRadioLinkId = g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId;
               if ( nRadioLinkId < 0 || nRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
                  continue;
               if ( nRadioLinkId != (int)g_uLastRadioLinkIndexSentRadioCommand )
                  continue;

               if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[nRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
                  continue;

               if ( NULL != pNICInfo && ((pNICInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS) )
               {
                  int nRateTx = 0;
                  nRateTx = controllerGetCardDataRate(pNICInfo->szMAC); // Returns 0 if radio link datarate must be used (no custom datarate set for this radio card);
                  if ( 0 == nRateTx )
                     nRateTx = g_iLastRadioLinkDataDataRateSentRadioCommand;
                  launch_set_datarate_atheros(NULL, i, nRateTx);
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
      if ( -1 != g_fIPCToTelemetry )
         ruby_ipc_channel_send_message(g_fIPCToTelemetry, pData, length);

      if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY )
         ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);

      if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS )
      {
         ruby_ipc_channel_send_message(g_fIPCToCentral, pData, length);
         return 0;
      }
      if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
      {
         memcpy((u8*)&g_PH_RTE, pData + sizeof(t_packet_header), sizeof(t_packet_header_ruby_telemetry_extended));
      }

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
      if ( pPH->packet_type != PACKET_TYPE_VIDEO_DATA_FULL )
      {
         //log_line("Received unknown video packet type.");
         return 0;
      }

      if ( ! g_bSearching )
      if ( ! ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
      if ( NULL != g_pCurrentModel )
      if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_STATS_VIDEO_INFO)
      {
         t_packet_header_video_full* pPHVFTemp = (t_packet_header_video_full*) (pData+sizeof(t_packet_header));
   
         int iFoundNALVideoFrame = 0;
         int iFoundPosition = -1;
         u8* pTmp = pData + sizeof(t_packet_header) + sizeof(t_packet_header_video_full);
         int length = pPHVFTemp->video_packet_length;
         for( int k=0; k<length; k++ )
         {
             s_uParseRadioInNALStartSequence = (s_uParseRadioInNALStartSequence<<8) | (*pTmp);
             s_uParseRadioInNALTotalParsedBytes++;
             pTmp++;

             if ( (s_uParseRadioInNALStartSequence & 0xFFFFFF00) == 0x0100 )
             {
                s_uParseRadioInLastNALTag = s_uParseRadioInNALStartSequence & 0b11111;
                if ( s_uParseRadioInLastNALTag == 1 || s_uParseRadioInLastNALTag == 5 )
                {
                   s_uParseRadioInNALCurrentSlices++;
                   if ( (int)s_uParseRadioInNALCurrentSlices >= camera_get_active_camera_h264_slices(g_pCurrentModel) )
                   {
                      s_uParseRadioInNALCurrentSlices = 0;
                      iFoundNALVideoFrame = s_uParseRadioInLastNALTag;
                      g_VideoInfoStatsRadioIn.uTmpCurrentFrameSize += k;
                      iFoundPosition = k;
                   }
                }
                //if ((s_uParseVideoTag == 0x0121) || (s_uParseVideoTag == 0x0127))
             }
         }

         if ( iFoundNALVideoFrame )
         {
             u32 delta = g_TimeNow - s_uLastRadioInVideoFrameTime;
             if ( delta > 254 )
                delta = 254;
             if ( delta == 0 )
                delta = 1;

             g_VideoInfoStatsRadioIn.uTmpCurrentFrameSize = g_VideoInfoStatsRadioIn.uTmpCurrentFrameSize / 1000;
             if ( g_VideoInfoStatsRadioIn.uTmpCurrentFrameSize > 127 )
                g_VideoInfoStatsRadioIn.uTmpCurrentFrameSize = 127;

             g_VideoInfoStatsRadioIn.uLastIndex = (g_VideoInfoStatsRadioIn.uLastIndex+1) % MAX_FRAMES_SAMPLES;
             g_VideoInfoStatsRadioIn.uFramesTimes[g_VideoInfoStatsRadioIn.uLastIndex] = delta;
             g_VideoInfoStatsRadioIn.uFramesTypesAndSizes[g_VideoInfoStatsRadioIn.uLastIndex] = (g_VideoInfoStatsRadioIn.uFramesTypesAndSizes[g_VideoInfoStatsRadioIn.uLastIndex] & 0x80) | ((u8)g_VideoInfoStatsRadioIn.uTmpCurrentFrameSize);
             
             u32 uNextIndex = (g_VideoInfoStatsRadioIn.uLastIndex+1) % MAX_FRAMES_SAMPLES;
             if ( iFoundNALVideoFrame == 5 )
                g_VideoInfoStatsRadioIn.uFramesTypesAndSizes[uNextIndex] |= (1<<7);
             else
                g_VideoInfoStatsRadioIn.uFramesTypesAndSizes[uNextIndex] &= 0x7F;
         
             g_VideoInfoStatsRadioIn.uTmpKeframeDeltaFrames++;
             if ( iFoundNALVideoFrame == 5 )
             {
                g_VideoInfoStatsRadioIn.uKeyframeInterval = g_VideoInfoStatsRadioIn.uTmpKeframeDeltaFrames;
                g_VideoInfoStatsRadioIn.uTmpKeframeDeltaFrames = 0; 
             }

             s_uLastRadioInVideoFrameTime = g_TimeNow;
             g_VideoInfoStatsRadioIn.uTmpCurrentFrameSize = length-iFoundPosition;
         }
         else
            g_VideoInfoStats.uTmpCurrentFrameSize += length;
      }

      int nRet = process_data_rx_video_on_new_packet(interfaceIndex, pData, length);

      #ifdef PROFILE_RX
      u32 dTimeEnd = get_current_timestamp_ms() - timeStart;
      if ( dTimeEnd >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing video message from single radio packet (type: %d len: %d/%d bytes), from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, length, interfaceIndex+1, (int)dTimeEnd);
      #endif

      return nRet;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_AUDIO )
   {
      if ( pPH->packet_type != PACKET_TYPE_AUDIO_SEGMENT )
      {
         //log_line("Received unknown video packet type.");
         return 0;
      }
      process_audio_packet(pData);

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

// Returns 1 if end of a video block was reached

int _process_received_full_radio_packet(int interfaceIndex)
{
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioRxTime = g_TimeNow;

   int nReturn = 0;
   int bufferLength = 0;
   u8* pBuffer = NULL;
   int bCRCOk = 0;

   #ifdef PROFILE_RX
   u32 timeNow = get_current_timestamp_ms();
   #endif

   pBuffer = radio_process_wlan_data_in(interfaceIndex, &bufferLength);

   #ifdef PROFILE_RX
   u32 dTime1 = get_current_timestamp_ms() - timeNow;
   if ( dTime1 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Reading received radio packet from radio interface %d took too long: %d ms.", interfaceIndex+1, (int)dTime1);
   #endif

   if ( pBuffer == NULL ) 
   {
      // Can be zero if the read timedout
      
      if ( radio_get_last_read_error_code() == RADIO_READ_ERROR_TIMEDOUT )
      {
         log_softerror_and_alarm("Rx ppcap read timedout reading a packet. Send alarm to central.");
         s_uRadioRxReadTimeoutCount++;
         send_alarm_to_central(ALARM_ID_CONTROLLER_RX_TIMEOUT,(u32)interfaceIndex, s_uRadioRxReadTimeoutCount);
         return 0;
      }
      log_softerror_and_alarm("Rx pcap returned a NULL packet.");
      return 0;
   }

   #ifdef PROFILE_RX
   u8 bufferCopy[MAX_PACKET_TOTAL_SIZE];
   if ( bufferLength > MAX_PACKET_TOTAL_SIZE )
   {
      log_softerror_and_alarm("[RX]: Received packet bigger than max packet size allowed: %d bytes, max %d bytes", bufferLength, MAX_PACKET_TOTAL_SIZE);
      bufferLength = MAX_PACKET_TOTAL_SIZE;
   }
   memcpy(bufferCopy, pBuffer, bufferLength);
   pBuffer = bufferCopy;
   #endif

   int iCountPackets = 0;
   u8* pData = pBuffer;
   int nLength = bufferLength;
   while ( nLength > 0 )
   { 
      t_packet_header* pPH = (t_packet_header*)pData;
      iCountPackets++;

      #ifdef PROFILE_RX
      u32 uTime2 = get_current_timestamp_ms();
      #endif
      
      int packetLength = packet_process_and_check(interfaceIndex, pData, nLength, &bCRCOk);

      #ifdef PROFILE_RX
      u32 dTime2 = get_current_timestamp_ms() - uTime2;
      if ( dTime2 >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing received single radio packet (type: %d, length: %d/%d bytes) from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, packetLength, interfaceIndex+1, (int)dTime2);
      #endif

      int nRes = radio_stats_update_on_packet_received(&g_Local_RadioStats, g_TimeNow, interfaceIndex, pData, packetLength, (int)bCRCOk);
      if ( (nRes != 1) && (!g_bSearching) )
      {
         #ifdef PROFILE_RX
         u32 dTime3 = get_current_timestamp_ms() - uTime2;
         if ( dTime3 >= PROFILE_RX_MAX_TIME )
            log_softerror_and_alarm("[Profile-Rx] Updating radio stats for single radio packet (type: %d, lenght: %d/%d bytes) from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, packetLength, interfaceIndex+1, (int)dTime3);
         #endif

         pData += pPH->total_length;
         nLength -= pPH->total_length;
         continue;
      }

      #ifdef PROFILE_RX
      u32 dTime3 = get_current_timestamp_ms() - uTime2;
      if ( dTime3 >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Updating radio stats for single radio packet (type: %d, lenght: %d/%d bytes) from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, packetLength, interfaceIndex+1, (int)dTime3);
      #endif
       
      if ( packetLength <= 0  )
      {
         int iRadioError = get_last_processing_error_code();
         s_uTotalBadPacketsReceived++; 
         if ( ! g_bSearching )
         if ( g_TimeNow > s_TimeLastLogWrongRxPacket + 10000 )
         {
            s_TimeLastLogWrongRxPacket = g_TimeNow;
            log_softerror_and_alarm("Received invalid packet, error code: %d (buffer size: %d, packet size: %d, packet offset: %d, encrypted: %d).", iRadioError, bufferLength, pPH->total_length, bufferLength-nLength, (pPH->packet_flags & PACKET_FLAGS_BIT_HAS_ENCRYPTION)?1:0 );
            if ( iRadioError == RADIO_PROCESSING_ERROR_CODE_INVALID_CRC_RECEIVED )
            {
               log_line("Send alarm to central, invalid radio packet, error: %d", iRadioError);
               send_alarm_to_central(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET, (u32)iRadioError, 1);
            }
         } 
         return 0;
      }

      if ( ! bCRCOk )
      {
         if ( g_TimeNow > s_TimeLastLogWrongRxPacket + 2000 )
         {
            s_TimeLastLogWrongRxPacket = g_TimeNow;
            log_softerror_and_alarm("Received packet broken packet (wrong CRC)");
         } 
         return 0;
      }

      u32 uStreamId = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      if ( uStreamId >= MAX_RADIO_STREAMS )
         uStreamId = 0;

      // For data streams, discard packets that are older than the newest packet received on that stream

      if ( uStreamId < STREAM_ID_VIDEO_1 )
      {
         if ( ! g_bSearching )
         if ( s_StreamsMaxReceivedPacketIndex[uStreamId] != MAX_U32 )
         if ( (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) < s_StreamsMaxReceivedPacketIndex[uStreamId] )
         {
            pData += pPH->total_length;
            nLength -= pPH->total_length;
            continue;   
         }
      }
      
      int nResultPacket = _process_received_single_radio_packet(interfaceIndex, uStreamId, pData, pPH->total_length);
      nReturn = nReturn | nResultPacket;
      if ( nResultPacket >= 0 )
         s_bReceivedAnyValidPacket = true;
      pData += pPH->total_length;
      nLength -= pPH->total_length; 
   }

   #ifdef PROFILE_RX
   u32 dTimeLast = get_current_timestamp_ms() - timeNow;
   if ( dTimeLast >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Reading and processing received radio packet (%d bytes, %d subpackets) from radio interface %d took too long: %d ms.", bufferLength, iCountPackets, interfaceIndex+1, (int)dTimeLast);
   #endif

   return nReturn;
}


// Returns 1 if end of a video block was reached

int process_received_radio_packets()
{
   int nReturn = 0;
   for(int i=0; i<hardware_get_radio_interfaces_count(); i++)
   {
      u32 timeNow = get_current_timestamp_ms();
      
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if( 0 == FD_ISSET(pNICInfo->monitor_interface_read.selectable_fd, &s_ReadSetRXRadio) )
         continue;

      nReturn = nReturn | _process_received_full_radio_packet(i);
      timeNow = get_current_timestamp_ms() - timeNow;
      if ( timeNow > 5 )
      {
         log_softerror_and_alarm("Processing received radio packet on radio interface %d took too long: %d ms.", i+1, (int)timeNow);
         char szBuff[256];
         szBuff[0] = 0;
         for( int i=0; i<g_Local_RadioStats.countRadioLinks; i++ )
         {
            char szTmp[32];
            sprintf(szTmp, "link %d: %d ", i, g_Local_RadioStats.radio_links[i].lastTxInterfaceIndex);
            strcat(szBuff, szTmp);
         }
         log_softerror_and_alarm("Current tx interfaces for each radio link: %s", szBuff);
      }
   }
   return nReturn;
}


int try_receive_radio_packets(u32 uMaxMicroSeconds)
{
   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = uMaxMicroSeconds;

   int maxfd = -1;
   FD_ZERO(&s_ReadSetRXRadio);
   for(int i=0; i<hardware_get_radio_interfaces_count(); i++)
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( pNICInfo->openedForRead )
      {
         FD_SET(pNICInfo->monitor_interface_read.selectable_fd, &s_ReadSetRXRadio);
         if ( pNICInfo->monitor_interface_read.selectable_fd > maxfd )
            maxfd = pNICInfo->monitor_interface_read.selectable_fd;
      }
   }

   int res = select(maxfd+1, &s_ReadSetRXRadio, NULL, NULL, &to);
   return res;
}

