#include "../base/base.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/launchers.h"
#include "../base/ruby_ipc.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"

#include "ruby_rt_vehicle.h"
#include "packets_utils.h"
#include "processor_tx_video.h"
#include "process_received_ruby_messages.h"
#include "video_link_auto_keyframe.h"
#include "video_link_stats_overwrites.h"
#include "launchers_vehicle.h"
#include "relay_rx.h"
#include "relay_tx.h"
#include "shared_vars.h"
#include "timers.h"

extern t_packet_queue s_QueueRadioPacketsOut;

fd_set s_ReadSetRXRadio;

bool s_bRCLinkDetected = false;

u32 s_uLastCommandChangeDataRateTime = 0;
u32 s_uLastCommandChangeDataRateCounter = MAX_U32;

u32 s_uTotalBadPacketsReceived = 0;

static u32 s_uRadioRxTimeoutAlarmCounter = 0;
int s_LastAtherosCardsDatarates[MAX_RADIO_INTERFACES];

u32 s_StreamsMaxReceivedPacketIndex[MAX_RADIO_STREAMS];

void init_radio_in_packets_state()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_LastAtherosCardsDatarates[i] = 5000;

   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      s_StreamsMaxReceivedPacketIndex[i] = MAX_U32;
}

void _try_decode_controller_links_stats_from_packet(u8* pPacketData, int packetLength)
{
   t_packet_header* pPH = (t_packet_header*)pPacketData;

   bool bHasControllerData = false;
   u8* pData = NULL;
   //int dataLength = 0;

   if ( ((pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY ) && (pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK) )
   if ( pPH->total_length > sizeof(t_packet_header) + 2*sizeof(u8) )
   {
      bHasControllerData = true;
      pData = pPacketData + sizeof(t_packet_header) + 2*sizeof(u8);
   }
   if ( ((pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO ) && (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS) )
   {
      pData = pPacketData + sizeof(t_packet_header);
      u8 countR = *(pData+1);
      if ( pPH->total_length > sizeof(t_packet_header) + 2*sizeof(u8) + countR * (sizeof(u32) + 2*sizeof(u8)) )
      {
         bHasControllerData = true;
         pData = pPacketData + sizeof(t_packet_header) + 2*sizeof(u8) + countR * (sizeof(u32) + 2*sizeof(u8));
      }
   }
   if ( ((pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO ) && (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2) )
   {
      pData = pPacketData + sizeof(t_packet_header);
      pData += sizeof(u32); // Skip: Retransmission request unique id
      u8 countR = *(pData+1);
      if ( pPH->total_length > sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8) + countR * (sizeof(u32) + 2*sizeof(u8)) )
      {
         bHasControllerData = true;
         pData = pPacketData + sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8) + countR * (sizeof(u32) + 2*sizeof(u8));
      }
   }
   if ( (! bHasControllerData ) || NULL == pData )
      return;

   g_SM_VideoLinkStats.timeLastReceivedControllerLinkInfo = g_TimeNow;

   //u8 flagsAndVersion = *pData;
   pData++;
   u32 timestamp = 0;
   memcpy(&timestamp, pData, sizeof(u32));
   pData += sizeof(u32);

   if ( g_PD_LastRecvControllerLinksStats.lastUpdateTime == timestamp )
   {
      //log_line("Discarded duplicate controller links info.");
      return;
   }

   g_PD_LastRecvControllerLinksStats.lastUpdateTime = timestamp;
   u8 srcSlicesCount = *pData;
   pData++;
   g_PD_LastRecvControllerLinksStats.radio_interfaces_count = *pData;
   pData++;
   g_PD_LastRecvControllerLinksStats.video_streams_count = *pData;
   pData++;
   for( int i=0; i<g_PD_LastRecvControllerLinksStats.radio_interfaces_count; i++ )
   {
      memcpy(&(g_PD_LastRecvControllerLinksStats.radio_interfaces_rx_quality[i][0]), pData, srcSlicesCount);
      pData += srcSlicesCount;
   }
   for( int i=0; i<g_PD_LastRecvControllerLinksStats.video_streams_count; i++ )
   {
      memcpy(&(g_PD_LastRecvControllerLinksStats.radio_streams_rx_quality[i][0]), pData, srcSlicesCount);
      pData += srcSlicesCount;
      memcpy(&(g_PD_LastRecvControllerLinksStats.video_streams_blocks_clean[i][0]), pData, srcSlicesCount);
      pData += srcSlicesCount;
      memcpy(&(g_PD_LastRecvControllerLinksStats.video_streams_blocks_reconstructed[i][0]), pData, srcSlicesCount);
      pData += srcSlicesCount;
      memcpy(&(g_PD_LastRecvControllerLinksStats.video_streams_blocks_max_ec_packets_used[i][0]), pData, srcSlicesCount);
      pData += srcSlicesCount;
      memcpy(&(g_PD_LastRecvControllerLinksStats.video_streams_requested_retransmission_packets[i][0]), pData, srcSlicesCount);
      pData += srcSlicesCount;
   }

   // Update dev video links graphs stats using controller received video links info

   u32 receivedTimeInterval = CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES * CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL_MS;
   int sliceDestIndex = 0;

   for( u32 sampleTimestamp = VIDEO_LINK_STATS_REFRESH_INTERVAL_MS/2; sampleTimestamp <= receivedTimeInterval; sampleTimestamp += VIDEO_LINK_STATS_REFRESH_INTERVAL_MS )
   {
      u32 sliceSrcIndex = sampleTimestamp/CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL_MS;
      if ( sliceSrcIndex >= CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES )
         break;

      for( int i=0; i<g_PD_LastRecvControllerLinksStats.radio_interfaces_count; i++ )
         g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[i][sliceDestIndex] = g_PD_LastRecvControllerLinksStats.radio_interfaces_rx_quality[i][sliceSrcIndex];

      for( int i=0; i<g_PD_LastRecvControllerLinksStats.video_streams_count; i++ )
      {
         g_SM_VideoLinkGraphs.controller_received_radio_streams_rx_quality[i][sliceDestIndex] = g_PD_LastRecvControllerLinksStats.radio_streams_rx_quality[i][sliceSrcIndex];
         g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[i][sliceDestIndex] = g_PD_LastRecvControllerLinksStats.video_streams_blocks_clean[i][sliceSrcIndex];
         g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[i][sliceDestIndex] = g_PD_LastRecvControllerLinksStats.video_streams_blocks_reconstructed[i][sliceSrcIndex];
         g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[i][sliceDestIndex] = g_PD_LastRecvControllerLinksStats.video_streams_blocks_max_ec_packets_used[i][sliceSrcIndex];
         g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[i][sliceDestIndex] = g_PD_LastRecvControllerLinksStats.video_streams_requested_retransmission_packets[i][sliceSrcIndex];
      }
      sliceDestIndex++;

      // Update only the last 3 intervals
      if ( sliceDestIndex > 3 || sliceSrcIndex > 3 )
         break;
   }
}

void _check_update_atheros_datarates(u32 linkIndex, int datarateVideo)
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      if ( (pRadioInfo->typeAndDriver & 0xFF) != RADIO_TYPE_ATHEROS )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != (int)linkIndex )
         continue;
      if ( datarateVideo == s_LastAtherosCardsDatarates[i] )
         continue;

      bool bWasOpenedForWrite = false;
      bool bWasOpenedForRead = false;
      log_line("Vehicle has Atheros cards. Setting the datarates for it.");
      if ( pRadioInfo->openedForWrite )
      {
         bWasOpenedForWrite = true;
         radio_close_interface_for_write(i);
      }
      if ( pRadioInfo->openedForRead )
      {
         bWasOpenedForRead = true;
         radio_close_interface_for_read(i);
      }
      g_TimeNow = get_current_timestamp_ms();
      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      launch_set_datarate_atheros(g_pCurrentModel, i, datarateVideo);
      s_LastAtherosCardsDatarates[i] = datarateVideo;

      g_TimeNow = get_current_timestamp_ms();
      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      if ( bWasOpenedForRead )
         radio_open_interface_for_read(i, RADIO_PORT_ROUTER_UPLINK);

      if ( bWasOpenedForWrite )
         radio_open_interface_for_write(i);

      g_TimeNow = get_current_timestamp_ms();
      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }
   }
}


void _process_received_radio_packet(int iRadioInterface, u8* pData, int length )
{
   t_packet_header* pPH = (t_packet_header*)pData;

   #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
   if ( (((pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY ) && (pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK)) ||
        (((pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO ) && (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS)) ||
        (((pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO ) && (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2)) )
      _try_decode_controller_links_stats_from_packet(pData, length);
   #endif
     
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
   {
      process_received_ruby_message(pData);
      return;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   {
      if ( pPH->packet_type == PACKET_TYPE_COMMAND )
      {
         t_packet_header_command* pPHC = (t_packet_header_command*)(pData + sizeof(t_packet_header));
         if ( pPHC->command_type == COMMAND_ID_UPLOAD_SW_TO_VEHICLE || pPHC->command_type == COMMAND_ID_UPLOAD_SW_TO_VEHICLE63 )
            g_uTimeLastCommandSowftwareUpload = g_TimeNow;

         if ( pPHC->command_type == COMMAND_ID_SET_RADIO_LINK_FLAGS )
         if ( pPHC->command_counter != s_uLastCommandChangeDataRateCounter || (g_TimeNow > s_uLastCommandChangeDataRateCounter + 4000) )
         {
            s_uLastCommandChangeDataRateTime = g_TimeNow;
            s_uLastCommandChangeDataRateCounter = pPHC->command_counter;
            g_TimeLastSetRadioFlagsCommandReceived = g_TimeNow;
            u32* pInfo = (u32*)(pData + sizeof(t_packet_header)+sizeof(t_packet_header_command));
            u32 linkIndex = *pInfo;
            pInfo++;
            u32 linkFlags = *pInfo;
            pInfo++;
            int* piInfo = (int*)pInfo;
            int datarateVideo = *piInfo;
            piInfo++;
            int datarateData = *piInfo;
            log_line("Received Set Radio Links Flags command. Link %d, Link flags: %u, Datarate %d/%d", linkIndex+1, linkFlags, datarateVideo, datarateData);
            if ( datarateVideo != g_pCurrentModel->radioLinksParams.link_datarates[linkIndex][0] ) 
            {
               _check_update_atheros_datarates(linkIndex, datarateVideo);
            }
         }
      }
      //log_line("Received a command: packet type: %d, module: %d, length: %d", pPH->packet_type, pPH->packet_flags & PACKET_FLAGS_MASK_MODULE, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, pData, length);
      return;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
   {
      //log_line("Received an uplink telemetry packet: packet type: %d, module: %d, length: %d", pPH->packet_type, pPH->packet_flags & PACKET_FLAGS_MASK_MODULE, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, pData, length);
      return;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RC )
   {
      ruby_ipc_channel_send_message(s_fIPCRouterToRC, pData, length);
      return;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   {
      if ( pPH->packet_type == PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL )
      {
         if ( pPH->total_length != sizeof(t_packet_header) + sizeof(u32) )
            return;

         u32 uAdaptiveLevel = 0;
         memcpy( &uAdaptiveLevel, pData + sizeof(t_packet_header), sizeof(u32));

         t_packet_header PH;
         PH.packet_flags = PACKET_COMPONENT_VIDEO;
         PH.packet_type =  PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK;
         PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
         PH.vehicle_id_dest = pPH->vehicle_id_src;
         PH.total_headers_length = sizeof(t_packet_header);
         PH.total_length = sizeof(t_packet_header) + sizeof(u32);
         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet+sizeof(t_packet_header), &uAdaptiveLevel, sizeof(u32));
         packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);

         int iAdaptiveLevel = uAdaptiveLevel;
         int iLevelsHQ = 1 + g_pCurrentModel->get_video_link_profile_max_level_shifts(g_pCurrentModel->video_params.user_selected_video_link_profile);
         int iLevelsMQ = 1 + g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_MQ);
         int iLevelsLQ = 1 + g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_LQ);

         int iTargetProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
         int iTargetShiftLevel = 0;

         if ( iAdaptiveLevel < iLevelsHQ )
         {
            iTargetProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
            iTargetShiftLevel = iAdaptiveLevel;
         }
         else if ( iAdaptiveLevel < iLevelsHQ + iLevelsMQ )
         {
            iTargetProfile = VIDEO_PROFILE_MQ;
            iTargetShiftLevel = iAdaptiveLevel - iLevelsHQ;    
         }
         else if ( iAdaptiveLevel < iLevelsHQ + iLevelsMQ + iLevelsLQ )
         {
            iTargetProfile = VIDEO_PROFILE_LQ;
            iTargetShiftLevel = iAdaptiveLevel - iLevelsHQ - iLevelsMQ;    
         }
         
         if ( iTargetProfile == g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
         if ( iTargetShiftLevel == g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel )
         {
            return;
         }
         video_stats_overwrites_switch_to_profile_and_level(iTargetProfile, iTargetShiftLevel);
         return;
      }
      
      if ( pPH->packet_type == PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE )
      {
         if ( pPH->total_length != sizeof(t_packet_header) + sizeof(u32) )
            return;
         // Discard requests if we are on fixed keyframe
         if ( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe > 0 )
            return;
         
         u32 uNewKeyframeValue = 0;
         memcpy( &uNewKeyframeValue, pData + sizeof(t_packet_header), sizeof(u32));

         if ( g_SM_VideoLinkStats.overwrites.uCurrentKeyframe != uNewKeyframeValue )
            log_line("[KeyFrame] Recv request from controller for keyframe: %u", uNewKeyframeValue);
         else
            log_line("[KeyFrame] Recv again request from controller for keyframe: %u", uNewKeyframeValue);          
         
         t_packet_header PH;
         PH.packet_flags = PACKET_COMPONENT_VIDEO;
         PH.packet_type =  PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK;
         PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
         PH.vehicle_id_dest = pPH->vehicle_id_src;
         PH.total_headers_length = sizeof(t_packet_header);
         PH.total_length = sizeof(t_packet_header) + sizeof(u32);
         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet+sizeof(t_packet_header), &uNewKeyframeValue, sizeof(u32));
         packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);

         video_link_auto_keyframe_set_controller_requested_value((int)uNewKeyframeValue);
         return;
      }

      if ( g_pCurrentModel->hasCamera() )
         process_data_tx_video_command(iRadioInterface, pData);
   }
}

// returns the packet length if it was reconstructed

int _handle_received_packet_error(int iInterfaceIndex, u8* pData, int nDataLength, int* pCRCOk)
{
   s_uTotalBadPacketsReceived++;

   int iRadioError = get_last_processing_error_code();

   // Try reconstruction
   if ( iRadioError == RADIO_PROCESSING_ERROR_CODE_INVALID_CRC_RECEIVED )
   {
      t_packet_header* pPH = (t_packet_header*)pData;
      pPH->extra_flags = 0;
      pPH->vehicle_id_src = 0;
      int nPacketLength = packet_process_and_check(iInterfaceIndex, pData, nDataLength, pCRCOk);

      if ( nPacketLength > 0 )
      {
         if ( g_TimeNow > g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket + 10000 )
         {
            char szBuff[256];
            alarms_to_string(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED, szBuff);
            log_line("Reconstructed invalid radio packet received. Sending alarm to controller. Alarms: %s, repeat count: %d", szBuff, (int)s_uTotalBadPacketsReceived);
            send_alarm_to_controller(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED, 0, 1);
            g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket = g_TimeNow;
            s_uTotalBadPacketsReceived = 0;
         }
         return nPacketLength;
      }
   }

   if ( iRadioError == RADIO_PROCESSING_ERROR_CODE_INVALID_CRC_RECEIVED )
   if ( g_TimeNow > g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket + 10000 )
   {
      char szBuff[256];
      alarms_to_string(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET, szBuff);
      log_line("Invalid radio packet received (bad CRC) (error: %d). Sending alarm to controller. Alarms: %s, repeat count: %d", iRadioError, szBuff, (int)s_uTotalBadPacketsReceived);
      send_alarm_to_controller(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET, 0, 1);
      s_uTotalBadPacketsReceived = 0;
   }

   if ( g_TimeNow > g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket + 10000 )
   {
      g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket = g_TimeNow;
      log_softerror_and_alarm("Received %d invalid packet(s). Error: %d", s_uTotalBadPacketsReceived, iRadioError );
      s_uTotalBadPacketsReceived = 0;
   } 
   return 0;
}

void _mark_link_from_controller_present()
{
   g_bHadEverLinkToController = true;
   g_TimeLastReceivedRadioPacketFromController = g_TimeNow;
   
   if ( ! g_bHasLinkToController )
   {
      g_bHasLinkToController = true;
      if ( g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.layout] & OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM )
         send_alarm_to_controller(ALARM_ID_LINK_TO_CONTROLLER_RECOVERED, 0, 5);
   }
}

void _process_received_full_radio_packet(int iInterfaceIndex)
{
   int bufferLength = 0;
   u8* pBuffer = NULL;
   int bCRCOk = 0;

   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iInterfaceIndex);

   #ifdef PROFILE_RX
   u32 timeNow = get_current_timestamp_ms();
   #endif

   pBuffer = radio_process_wlan_data_in(iInterfaceIndex, &bufferLength);

   #ifdef PROFILE_RX
   u32 dTime1 = get_current_timestamp_ms() - timeNow;
   if ( dTime1 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Reading received radio packet from radio interface %d took too long: %d ms.", iInterfaceIndex+1, (int)dTime1);
   #endif

   if ( pBuffer == NULL )
   {
      // Can be null if the read timed out
      if ( radio_get_last_read_error_code() == RADIO_READ_ERROR_TIMEDOUT )
      {
         log_softerror_and_alarm("Rx ppcap read timedout reading a packet. Sending alarm to controller.");
         s_uRadioRxTimeoutAlarmCounter++;
         send_alarm_to_controller(ALARM_ID_VEHICLE_RX_TIMEOUT, 1+(u32)iInterfaceIndex, s_uRadioRxTimeoutAlarmCounter);
         return;
      }
      if ( radio_interfaces_broken() )
      {
         log_line("Radio interfaces have broken up. Reinitaliziging everything.");
         reinit_radio_interfaces();
      }
      return;
   }

   int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[iInterfaceIndex];

   bool bIsRelayTrafic = false;
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId > 0 )
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId-1 == iRadioLinkId )
      bIsRelayTrafic = true;

   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      bIsRelayTrafic = true;

   if ( bIsRelayTrafic )
   {
      // Process packets received on relay links separately
      relay_process_received_radio_packet_from_relayed_vehicle(iRadioLinkId, iInterfaceIndex, pBuffer, bufferLength);
      return;
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

   u32 gap = g_TimeNow - g_TimeLastReceivedRadioPacketFromController;
   if ( gap > 254 )
      gap = 254;
   if ( g_SM_VideoLinkGraphs.vehicleRXMaxTimeGap[0] == 255 || gap > g_SM_VideoLinkGraphs.vehicleRXMaxTimeGap[0] )
      g_SM_VideoLinkGraphs.vehicleRXMaxTimeGap[0] = gap;

   if ( 0 == g_TimeFirstReceivedRadioPacketFromController )
      g_TimeFirstReceivedRadioPacketFromController = g_TimeNow;
   
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioRxTime = g_TimeNow;

   u8* pData = pBuffer;
   int nLength = bufferLength;
   int iCountPackets = 0;
   while ( nLength > 0 )
   {
      #ifdef PROFILE_RX
      u32 uTime2 = get_current_timestamp_ms();
      #endif

      t_packet_header* pPH = (t_packet_header*)pData;
      bCRCOk = false;
      int nPacketLength = packet_process_and_check(iInterfaceIndex, pData, nLength, &bCRCOk);

      #ifdef PROFILE_RX
      u32 dTime2 = get_current_timestamp_ms() - uTime2;
      if ( dTime2 >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Processing received single radio packet (type: %d, length: %d/%d bytes) from radio interface %d took too long: %d ms.", pPH->packet_type, pPH->total_length, nPacketLength, iInterfaceIndex+1, (int)dTime2);
      #endif

      int nRes = radio_stats_update_on_packet_received(&g_SM_RadioStats, g_TimeNow, iInterfaceIndex, pData, nPacketLength, (int)bCRCOk);
      if ( nRes != 1 )
      {
         pData += pPH->total_length;
         nLength -= pPH->total_length;
         continue;
      }
      g_UplinkInfoRxStats[iInterfaceIndex].lastReceivedDBM = pRadioInfo->monitor_interface_read.radioInfo.nDbm;
      g_UplinkInfoRxStats[iInterfaceIndex].lastReceivedDataRate = pRadioInfo->monitor_interface_read.radioInfo.nRate;

      if ( nPacketLength <= 0 )
      {
         nPacketLength = _handle_received_packet_error(iInterfaceIndex, pData, nLength, &bCRCOk);
         if ( nPacketLength <= 0 )
         {
            return;
         }
      }

      iCountPackets++;

      // Process received packet from controller to relayed vehicle separately

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId > 0) )
      if ( g_pCurrentModel->relay_params.uRelayVehicleId == pPH->vehicle_id_dest )
      {
         _mark_link_from_controller_present();
  
         relay_process_received_single_radio_packet_from_controller_to_relayed_vehicle(iInterfaceIndex, pData, pPH->total_length);

         pData += pPH->total_length;
         nLength -= pPH->total_length;
         continue;
      }

      if ( NULL != g_pCurrentModel && pPH->vehicle_id_dest != g_pCurrentModel->vehicle_id )
      {
         if ( g_TimeNow > g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket + 2000 )
         {
            g_UplinkInfoRxStats[iInterfaceIndex].timeLastLogWrongRxPacket = g_TimeNow;
            log_softerror_and_alarm("Received packet targeted to a different vehicle. Current vehicle UID: %u, received targeted vehicle UID: %u", g_pCurrentModel->vehicle_id, pPH->vehicle_id_dest);
         }
         return;
      }

      _mark_link_from_controller_present();
      
      u32 uStreamId = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      if ( uStreamId < 0 || uStreamId >= MAX_RADIO_STREAMS )
         uStreamId = 0;

      if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST )
      {
         for( int i=0; i<MAX_RADIO_STREAMS; i++ )
            s_StreamsMaxReceivedPacketIndex[i] = MAX_U32;
      }

      if ( (s_StreamsMaxReceivedPacketIndex[uStreamId] == MAX_U32) || ((pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) > s_StreamsMaxReceivedPacketIndex[uStreamId]) )
         s_StreamsMaxReceivedPacketIndex[uStreamId] = (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);

      // For data streams, discard packets that are older than the newest packet received on that stream

      if ( uStreamId < STREAM_ID_VIDEO_1 )
      if ( (s_StreamsMaxReceivedPacketIndex[uStreamId] != MAX_U32) && (s_StreamsMaxReceivedPacketIndex[uStreamId] > 10) )
      if ( (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) < s_StreamsMaxReceivedPacketIndex[uStreamId]-10 )
      {
         pData += pPH->total_length;
         nLength -= pPH->total_length;
         continue;   
      }
      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RC )
      {
         if ( ! s_bRCLinkDetected )
         {
            char szBuff[64];
            snprintf(szBuff, sizeof(szBuff), "touch %s", FILE_TMP_RC_DETECTED);
            hw_execute_bash_command(szBuff, NULL);
         }
         s_bRCLinkDetected = true;
      }

      _process_received_radio_packet(iInterfaceIndex, pData, pPH->total_length);
      pData += pPH->total_length;
      nLength -= pPH->total_length;
   }

   #ifdef PROFILE_RX
   u32 dTimeLast = get_current_timestamp_ms() - timeNow;
   if ( dTimeLast >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Reading and processing received radio packet (%d bytes, %d subpackets) from radio interface %d took too long: %d ms.", bufferLength, iCountPackets, iInterfaceIndex+1, (int)dTimeLast);
   #endif
}

void _process_received_short_radio_packet(int iInterfaceIndex, u8* pPacket, int iMaxSize)
{
   if ( NULL == pPacket )
      return;
   t_packet_header_short* pPHS = (t_packet_header_short*)pPacket;

   if ( pPHS->total_length > iMaxSize )
      return;

   if ( pPHS->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )   
   if ( pPHS->total_length == (int)sizeof(t_packet_header_short)+2 )
   {
      u8 pingId = *(pPacket+sizeof(t_packet_header_short));
      u8 radioLinkId = *(pPacket+sizeof(t_packet_header_short)+1);
      u32 timeNow = get_current_timestamp_micros();

      t_packet_header_short PHS;
      PHS.packet_type = PACKET_TYPE_RUBY_PING_CLOCK_REPLY;
      PHS.packet_index = radio_get_next_short_packet_index();
      PHS.stream_packet_idx = (((u32)STREAM_ID_DATA+1)<<PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) | (get_stream_next_packet_index(STREAM_ID_DATA+1) & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
      PHS.vehicle_id_src = pPHS->vehicle_id_dest;
      PHS.vehicle_id_dest = pPHS->vehicle_id_src;
      PHS.total_length = sizeof(t_packet_header_short) + 2*sizeof(u8) + sizeof(u32);
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PHS, sizeof(t_packet_header_short));
      memcpy(packet+sizeof(t_packet_header_short), &pingId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header_short)+sizeof(u8), &timeNow, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header_short)+sizeof(u32)+sizeof(u8), &radioLinkId, sizeof(u8));
   
      if ( radio_write_sik_packet(iInterfaceIndex, packet, (int)PHS.total_length) > 0 )
         log_line("Sent SiK ping reply, packet id: %u", PHS.stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
      else
         log_softerror_and_alarm("Failed to write ping reply to SiK radio.");
   }

   radio_stats_update_on_short_packet_received(&g_SM_RadioStats, g_TimeNow, iInterfaceIndex, pPacket, (int)pPHS->total_length);
}

void _process_received_short_radio_data(int iInterfaceIndex)
{
   static u8 s_uBuffersShortMessages[MAX_RADIO_INTERFACES][513];
   static int s_uBufferShortMessagesReadPos[MAX_RADIO_INTERFACES];
   static bool s_bInitializedBuffersShortMessages = false;

   if ( ! s_bInitializedBuffersShortMessages )
   {
      s_bInitializedBuffersShortMessages = true;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         s_uBufferShortMessagesReadPos[i] = 0;
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
      return;

   if ( ! hardware_radio_index_is_sik_radio(iInterfaceIndex) )
      return;
         
   int iMaxRead = 512 - s_uBufferShortMessagesReadPos[iInterfaceIndex];
   int iRead = read(pRadioHWInfo->monitor_interface_read.selectable_fd, &(s_uBuffersShortMessages[iInterfaceIndex][s_uBufferShortMessagesReadPos[iInterfaceIndex]]), iMaxRead);
   if ( iRead <= 0 )
      return;

   s_uBufferShortMessagesReadPos[iInterfaceIndex] += iRead;
   int iBufferLength = s_uBufferShortMessagesReadPos[iInterfaceIndex];
   
   // Received at least the full header?

   if ( iBufferLength < (int)sizeof(t_packet_header_short) )
      return;

   int iPacketPos = -1;
   do
   {
      u8* pData = (u8*)&(s_uBuffersShortMessages[iInterfaceIndex][0]);
      iPacketPos = radio_buffer_is_valid_short_packet(pData, iBufferLength);

      // No (more) valid packet found in the buffer?
      if ( iPacketPos < 0 )
      {
         if ( iBufferLength > 255 )
         {
            // Keep the last 255 bytes in the buffer
            log_line("[RadioRX] Discarded data too old (%d bytes in the buffer), no valid messages in it.", iBufferLength);
            for( int i=0; i<(iBufferLength-255); i++ )
               s_uBuffersShortMessages[iInterfaceIndex][i] = s_uBuffersShortMessages[iInterfaceIndex][i+255];
            s_uBufferShortMessagesReadPos[iInterfaceIndex] -= 255;
         }
         return;
      }
      _process_received_short_radio_packet(iInterfaceIndex, pData + iPacketPos, iBufferLength-iPacketPos);
   
      t_packet_header_short* pPHS = (t_packet_header_short*)(pData+iPacketPos);

      int delta = (iPacketPos+(int)(pPHS->total_length));
      if ( delta > iBufferLength )
         delta = iBufferLength;
      for( int i=0; i<iBufferLength - delta; i++ )
         s_uBuffersShortMessages[iInterfaceIndex][i] = s_uBuffersShortMessages[iInterfaceIndex][i+delta];
      s_uBufferShortMessagesReadPos[iInterfaceIndex] -= delta;
      iBufferLength -= delta;
   } while ( iPacketPos >= 0 );
}

void process_received_radio_packets()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
         continue;
      if( 0 != FD_ISSET(pRadioHWInfo->monitor_interface_read.selectable_fd, &s_ReadSetRXRadio) )
      {
         if ( hardware_radio_index_is_sik_radio(i) )
            _process_received_short_radio_data(i);
         else
            _process_received_full_radio_packet(i);
      }
   }
}


int try_receive_radio_packets(u32 uMaxMicroSeconds)
{
   int maxfd = 0;
   struct timeval timeRXRadio;

   FD_ZERO(&s_ReadSetRXRadio);
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
         continue;
      FD_SET(pRadioHWInfo->monitor_interface_read.selectable_fd, &s_ReadSetRXRadio);
      if ( pRadioHWInfo->monitor_interface_read.selectable_fd > maxfd )
         maxfd = pRadioHWInfo->monitor_interface_read.selectable_fd;
   }

   // Check for exceptions first

   timeRXRadio.tv_sec = 0;
   timeRXRadio.tv_usec = 10; // 0.01 miliseconds timeout

   int nResult = select(maxfd+1, NULL, NULL, &s_ReadSetRXRadio, &timeRXRadio);
   if ( nResult < 0 || nResult > 0 )
   {
      hardware_sleep_ms(50);
      if ( g_bQuit )
         return 0;
      log_line("Radio interfaces have broken up. Exception on radio handle. Reinitalizing everything.");
      reinit_radio_interfaces();
      return 0;
   }

   FD_ZERO(&s_ReadSetRXRadio);
   maxfd = 0;
   timeRXRadio.tv_sec = 0;
   timeRXRadio.tv_usec = uMaxMicroSeconds;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
         continue;
      FD_SET(pRadioHWInfo->monitor_interface_read.selectable_fd, &s_ReadSetRXRadio);
      if ( pRadioHWInfo->monitor_interface_read.selectable_fd > maxfd )
         maxfd = pRadioHWInfo->monitor_interface_read.selectable_fd;
   }

   // Check for read ready

   nResult = select(maxfd+1, &s_ReadSetRXRadio, NULL, NULL, &timeRXRadio);
   if ( nResult < 0 )
   {
      log_line("Radio interfaces have broken up. Exception on radio handle. Reinitalizing everything.");
      reinit_radio_interfaces();
      return 0;
   }


   if ( nResult == 0 || ((NULL != g_pProcessStats) && (0 != g_pProcessStats->lastRadioRxTime) && (g_pProcessStats->lastRadioRxTime < g_TimeNow + 3000)) )
   //if ( g_pCurrentModel->clock_sync_type != CLOCK_SYNC_TYPE_NONE )
   {
      if ( g_TimeLastReceivedRadioPacketFromController < g_TimeNow - 3000 )
      if ( g_bHasLinkToController )
      {
         g_bHasLinkToController = false;
         if ( g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.layout] & OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM )
            send_alarm_to_controller(ALARM_ID_LINK_TO_CONTROLLER_LOST, 0, 5);
      }
   }
   //if ( nResult != 0 )
   //   log_line("Try read radio, count: %d, maxfx: %d, result: %d", s_iInterfacesRXCount, maxfd, nResult);
   return nResult;
}
