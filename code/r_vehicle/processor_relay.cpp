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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models_list.h"
#include "../base/radio_utils.h"
#include "../base/ruby_ipc.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "../common/relay_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radio_rx.h"
#include "processor_relay.h"
#include "ruby_rt_vehicle.h"
#include "shared_vars.h"
#include "timers.h"
#include "video_link_auto_keyframe.h"

bool s_bHasEverReceivedDataFromRelayedVehicle = false;
bool s_uLastReceivedRelayedVehicleID = MAX_U32;

u8 s_RadioRawPacketRelayed[MAX_PACKET_TOTAL_SIZE];

// It's a pointer to: type_uplink_rx_info_stats s_UplinkInfoRxStats[MAX_RADIO_INTERFACES];
type_uplink_rx_info_stats* s_pRelayRxInfoStats = NULL;

u8 s_uLastLocalRadioLinkUsedForPingToRelayedVehicle = 0;

u32 s_uLastTimeReceivedRubyTelemetryFromRelayedVehicle = 0;

u32 relay_get_time_last_received_ruby_telemetry_from_relayed_vehicle()
{
   return s_uLastTimeReceivedRubyTelemetryFromRelayedVehicle;
}

void _process_received_ruby_telemetry_from_relayed_vehicle(u8* pPacket, int iLenght)
{
   s_uLastTimeReceivedRubyTelemetryFromRelayedVehicle = g_TimeNow;
}

void relay_init_and_set_rx_info_stats(type_uplink_rx_info_stats* pUplinkStats)
{
   s_pRelayRxInfoStats = pUplinkStats;
   s_bHasEverReceivedDataFromRelayedVehicle = false;
   s_uLastReceivedRelayedVehicleID = MAX_U32;
}


void relay_process_received_single_radio_packet_from_controller_to_relayed_vehicle(int iRadioInterfaceIndex, u8* pBufferData, int iBufferLength)
{
   t_packet_header* pPH = (t_packet_header*)pBufferData;
   //log_line("[Relay] Relay from CID %u to VID %u, type: %s",
   //   pPH->vehicle_id_src, pPH->vehicle_id_dest, str_get_packet_type(pPH->packet_type));

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
      s_uLastLocalRadioLinkUsedForPingToRelayedVehicle = (u8) g_pCurrentModel->radioInterfacesParams.interface_link_id[iRadioInterfaceIndex];

   relay_send_single_packet_to_relayed_vehicle(pBufferData, iBufferLength);
}


void relay_process_received_radio_packet_from_relayed_vehicle(int iRadioLink, int iRadioInterfaceIndex, u8* pBufferData, int iBufferLength)
{
   //log_line("Received packet from relayed vehicle on radio interface %d, %d bytes", iRadioInterfaceIndex+1, iBufferLength);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioRxTime = g_TimeNow;

   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
      return;

   t_packet_header* pPH = (t_packet_header*)pBufferData;

   if ( (pPH->vehicle_id_src == 0) || (pPH->vehicle_id_src == MAX_U32) ||
        (g_pCurrentModel->relay_params.uRelayedVehicleId == 0 ) ||
        (g_pCurrentModel->relay_params.uRelayedVehicleId == MAX_U32) ||
        (pPH->vehicle_id_src != g_pCurrentModel->relay_params.uRelayedVehicleId) )
   {
      static bool s_bFirstTimeReceivedDataFromWrongRelayedVID = true;
      if ( s_bFirstTimeReceivedDataFromWrongRelayedVID )
      {
         log_line("[Relay] Started to receive data for first time from wrong relayed VID (%u). Expected relayed VID: %u",
            pPH->vehicle_id_src, g_pCurrentModel->relay_params.uRelayedVehicleId);
         s_bFirstTimeReceivedDataFromWrongRelayedVID = false;
      }
      return;
   }

   if ( ! s_bHasEverReceivedDataFromRelayedVehicle )
   {
      log_line("[Relay] Started to receive valid data from relayed VID: %u, current relay flags: %s, current relay mode: %s",
       pPH->vehicle_id_src, str_format_relay_flags(g_pCurrentModel->relay_params.uRelayCapabilitiesFlags), str_format_relay_mode(g_pCurrentModel->relay_params.uCurrentRelayMode));
      s_bHasEverReceivedDataFromRelayedVehicle = true;
      s_uLastReceivedRelayedVehicleID = pPH->vehicle_id_src;
   }

   // Do not relay video packets if relay mode is not one where remote video through this vehicle is needed

   if ( ((pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO) ||
        ((pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_AUDIO) )
   if ( (pPH->packet_type == PACKET_TYPE_VIDEO_DATA_FULL) ||
        (pPH->packet_type == PACKET_TYPE_AUDIO_SEGMENT) )
   if ( ! relay_vehicle_must_forward_video_from_relayed_vehicle(g_pCurrentModel, pPH->vehicle_id_src) )
      return;

   type_uplink_rx_info_stats* pRxInfoStats = NULL;
   if ( NULL != s_pRelayRxInfoStats )
      pRxInfoStats = (type_uplink_rx_info_stats*)(((u32*)s_pRelayRxInfoStats) + iRadioInterfaceIndex * sizeof(type_uplink_rx_info_stats));

   u8* pData = pBufferData;
   int nLength = iBufferLength;
   int iCountReceivedPackets = 0;
   bool bIsFullComposedPacketOkToForward = true;
   bool bPacketContainsDataToForward = false;
   
   while ( nLength > 0 )
   {
      t_packet_header* pPH = (t_packet_header*)pData;

      int bCRCOk = 0;
      int iPacketLength = packet_process_and_check(iRadioInterfaceIndex, pData, nLength, &bCRCOk);

      if ( iPacketLength <= 0 )
         return;

      if ( pPH->vehicle_id_src != g_pCurrentModel->relay_params.uRelayedVehicleId )
      {
         pData += pPH->total_length;
         nLength -= pPH->total_length;
         
         if ( (NULL != pRxInfoStats) && (g_TimeNow > pRxInfoStats->timeLastLogWrongRxPacket + 2000) )
         {
            pRxInfoStats->timeLastLogWrongRxPacket = g_TimeNow;
            log_softerror_and_alarm("[Relaying] Received radio packet on the relay link from a different vehicle than the relayed vehicle (received VID: %u, current main VID: %u, relayed VID: %u)", pPH->vehicle_id_src, g_pCurrentModel->vehicle_id, g_pCurrentModel->relay_params.uRelayedVehicleId );
         }
         
         bIsFullComposedPacketOkToForward = false;
         continue;
      }

      iCountReceivedPackets++;
  
      if ( (pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST) ||
           (pPH->packet_type ==  PACKET_TYPE_RUBY_PAIRING_CONFIRMATION) )
      {
         bPacketContainsDataToForward = true;
         log_line("Will relay from relayed vehicle to controller the pairing confirmation message.");
      }
      if ( (pPH->packet_type == PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK) ||
           (pPH->packet_type == PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK) )
         bPacketContainsDataToForward = true;

      if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO )
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
      if ( relay_current_vehicle_must_send_relayed_video_feeds() )
         bPacketContainsDataToForward = true;
   
      if ( g_pCurrentModel->relay_params.uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_TELEMETRY )
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
         bPacketContainsDataToForward = true;

      // Ruby telemetry and FC telemetry is always forwarded on the relay link
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
      if ( (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_SHORT) ||
           (pPH->packet_type == PACKET_TYPE_FC_TELEMETRY) ||
           (pPH->packet_type == PACKET_TYPE_FC_TELEMETRY_EXTENDED) )
      {
         if ( (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_SHORT) )
            _process_received_ruby_telemetry_from_relayed_vehicle((u8*)&pPH, pPH->total_length);
         bPacketContainsDataToForward = true;
      }

      if ( (pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY) )
      {
         bPacketContainsDataToForward = true;
         if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
            memcpy(pData+sizeof(t_packet_header)+2*sizeof(u8)+sizeof(u32), &s_uLastLocalRadioLinkUsedForPingToRelayedVehicle, sizeof(u8));
      }
      pData += pPH->total_length;
      nLength -= pPH->total_length;
   }

   if ( (! bIsFullComposedPacketOkToForward) || (! bPacketContainsDataToForward) )
      return;

   // Forward the full composed packet to the controller
   relay_send_packet_to_controller(pBufferData, iBufferLength);
}


void relay_on_relay_params_changed()
{
   if ( NULL == g_pCurrentModel )
      return;

   log_line("[Relay] Processing notification that relay link id was updated by user command...");
   
   if ( g_pCurrentModel->relay_params.uRelayedVehicleId != s_uLastReceivedRelayedVehicleID )
   {
      s_bHasEverReceivedDataFromRelayedVehicle = false;
      s_uLastReceivedRelayedVehicleID = MAX_U32;
   }

   radio_rx_stop_rx_thread();
   close_radio_interfaces();

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   int iRelayRadioLink = g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId;
   if ( iRelayRadioLink < 0 || iRelayRadioLink >= g_pCurrentModel->radioLinksParams.links_count )
   {
      // Relaying is disabled. Set all radio links and interfaces as active

      log_line("[Relay] Relaying is disabled. Set all radio links and interfaces as usable for regular radio links.");

      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
         g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      }
   }
   else
   {
      // Relaying is enabled on a radio link.

      log_line("[Relay] Relaying is enabled on radio link %d.", iRelayRadioLink+1);
      g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRelayRadioLink] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
      
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != iRelayRadioLink )
         {
            g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
            continue;
         }
         log_line("[Relay] Mark radio interface %d as used for relaying.", i+1);
         g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
      }

      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      }
      g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRelayRadioLink] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
   }

   saveCurrentModel();

   configure_radio_interfaces_for_current_model(g_pCurrentModel, g_pProcessStats);
   
   open_radio_interfaces();

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   radio_rx_start_rx_thread(&g_SM_RadioStats, NULL, 0);
   
   log_line("[Relay] Done processing notification that relay parameters where updated by user command. Notify all local components about new radio config.");
   
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY | (MODEL_CHANGED_GENERIC<<8);
   PH.total_length = sizeof(t_packet_header);

   ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);
         
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void relay_on_relay_mode_changed(u8 uOldMode, u8 uNewMode)
{
   log_line("[Relay] New relay mode: %d, %s", uNewMode, str_format_relay_mode(uNewMode));

   if ( uOldMode & RELAY_MODE_REMOTE )
   if ( ! (uNewMode & RELAY_MODE_REMOTE) )
      video_link_auto_keyframe_set_controller_requested_value(0, DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL);

   g_iShowVideoKeyframesAfterRelaySwitch = 6;
}

void relay_on_relay_flags_changed(u32 uNewFlags)
{
   log_line("[Relay] Relay flags changed to: %u, %s", uNewFlags, str_format_relay_flags(uNewFlags));
}

void relay_on_relayed_vehicle_id_changed(u32 uNewVehicleId)
{
   log_line("[Relay] Set relayed VID changed to: %u (Did received data from previous relayed VID %u? %s)",
    uNewVehicleId, s_uLastReceivedRelayedVehicleID,
    (s_bHasEverReceivedDataFromRelayedVehicle?"Yes":"No") );

   s_bHasEverReceivedDataFromRelayedVehicle = false;
}

void relay_send_packet_to_controller(u8* pBufferData, int iBufferLength)
{
   if ( iBufferLength <= 0 )
   {
      log_softerror_and_alarm("[Relay] Tried to send an empty radio packet (%d bytes) from relayed vehicle to controller.", iBufferLength);
      return;
   }

   u8* pData = pBufferData;
   int nLength = iBufferLength;
   u32 uCountChainedPackets = 0;
   u32 uStreamId = 0;
   u32 uSourceVehicleId = 0;
   bool bContainsPairConfirmation = false;

   while ( nLength > 0 )
   {
      t_packet_header* pPH = (t_packet_header*)pData;
      if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION )
         bContainsPairConfirmation = true;
      uSourceVehicleId = pPH->vehicle_id_src;
      uStreamId = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      if ( uStreamId < 0 || uStreamId >= MAX_RADIO_STREAMS )
         uStreamId = 0;

      nLength -= pPH->total_length;
      pData += pPH->total_length;
      uCountChainedPackets++;
   }

   if ( uSourceVehicleId != g_pCurrentModel->relay_params.uRelayedVehicleId )
   {
      return;
   } 
   // Send packet on all radio links to controller (that are not set as relay links)

   bool bPacketSent = false;

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iRadioLinkId )
         continue;
        
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;

      if ( !(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      int iRadioInterfaceIndex = -1;
      for( int k=0; k<g_pCurrentModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[k] == iRadioLinkId )
         {
            iRadioInterfaceIndex = k;
            break;
         }
      }
      if ( iRadioInterfaceIndex < 0 )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( ! pRadioHWInfo->openedForWrite )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      
      int nRateTx = g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iRadioLinkId];
      radio_set_out_datarate(nRateTx);

      u32 radioFlags = g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[iRadioInterfaceIndex];
      if ( ! (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE) )
      {
         radioFlags &= ~(RADIO_FLAGS_STBC | RADIO_FLAGS_LDPC | RADIO_FLAGS_SHORT_GI | RADIO_FLAGS_HT40);
         radioFlags |= RADIO_FLAGS_HT20;
      }
      radio_set_frames_flags(radioFlags);

      int totalLength = radio_build_new_raw_packet(iRadioLinkId, s_RadioRawPacketRelayed, pBufferData, iBufferLength, RADIO_PORT_ROUTER_DOWNLINK, 0, 0, NULL);

      if ( (totalLength >0) && radio_write_raw_packet(iRadioInterfaceIndex, s_RadioRawPacketRelayed, totalLength) )
      {           
         bPacketSent = true;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxPackets++;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxBytes += iBufferLength;
      }
      else
         log_softerror_and_alarm("[RelayTX] Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   }

   if ( ! bPacketSent )
   {
      log_softerror_and_alarm("[RelayTX] Packet not sent! No radio interface could send it.");
      if ( bContainsPairConfirmation )
         log_softerror_and_alarm("[RelayTX] Contained a pairing confirmation that was not relayed back.");
      return;
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioTxTime = g_TimeNow;

   if ( bContainsPairConfirmation )
     log_line("[RelayTX] Relayed back pairing confirmation from relayed vehicle VID %u to controller %u",
        uSourceVehicleId, g_uControllerId);
   //log_line("[RelayTX] Sent relayed packet to controller, stream %u, %d bytes, relayed VID: %u, current VID: %u", uStreamId, iBufferLength, uSourceVehicleId, g_pCurrentModel->vehicle_id);
}

void relay_send_single_packet_to_relayed_vehicle(u8* pBufferData, int iBufferLength)
{
   if ( (NULL == pBufferData) || (iBufferLength <= 0) )
   {
      log_softerror_and_alarm("[Relay] Tried to send an empty radio packet to relayed vehicle");
      return;
   }
   t_packet_header* pPH = (t_packet_header*)pBufferData;

   u32 uRelayedVehicleId = pPH->vehicle_id_dest;
   u32 uStreamId = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   if ( uStreamId < 0 || uStreamId >= MAX_RADIO_STREAMS )
      uStreamId = 0;

   if ( uRelayedVehicleId != g_pCurrentModel->relay_params.uRelayedVehicleId )
   {
      return;
   }

   // Send packet on all radio links to relayed vehicle

   bool bPacketSent = false;

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY) )
         continue;

      if ( !(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      int iRadioInterfaceIndex = -1;
      for( int k=0; k<g_pCurrentModel->radioInterfacesParams.interfaces_count; k++ )
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[k] == iRadioLinkId )
         {
            iRadioInterfaceIndex = k;
            break;
         }
      if ( iRadioInterfaceIndex < 0 )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( ! pRadioHWInfo->openedForWrite )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      
      int nRateTx = g_pCurrentModel->radioLinksParams.link_datarate_data_bps[iRadioLinkId];
      radio_set_out_datarate(nRateTx);

      u32 radioFlags = g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[iRadioInterfaceIndex];
      if ( ! (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE) )
      {
         radioFlags &= ~(RADIO_FLAGS_STBC | RADIO_FLAGS_LDPC | RADIO_FLAGS_SHORT_GI | RADIO_FLAGS_HT40);
         radioFlags |= RADIO_FLAGS_HT20;
      }
      radio_set_frames_flags(radioFlags);

      int totalLength = radio_build_new_raw_packet(iRadioLinkId, s_RadioRawPacketRelayed, pBufferData, iBufferLength, RADIO_PORT_ROUTER_UPLINK, 0, 0, NULL);

      if ( (totalLength>0) && radio_write_raw_packet(iRadioInterfaceIndex, s_RadioRawPacketRelayed, totalLength) )
      {           
         bPacketSent = true;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxPackets++;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxBytes += iBufferLength;
      }
      else
         log_softerror_and_alarm("[RelayTX] Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   }

   if ( ! bPacketSent )
   {
      log_softerror_and_alarm("[RelayTX] Packet not sent to relayed vehicle! No radio interface could send it.");
      return;
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
}

bool relay_current_vehicle_must_send_own_video_feeds()
{
   static bool sl_bCurrentRelayModeMustSentOwnVideoFeed = false;
   bool bMustSendOwnVideo = false;

   if ( (NULL == g_pCurrentModel) || g_bVideoPaused )
   {
      if ( sl_bCurrentRelayModeMustSentOwnVideoFeed )
      {
         sl_bCurrentRelayModeMustSentOwnVideoFeed = false;
         log_line("Flag to tell if this vehicle should send it's own video feeds changed to: false");
      }
      return false;
   }
   
   bMustSendOwnVideo = relay_vehicle_must_forward_own_video(g_pCurrentModel);

   if ( bMustSendOwnVideo != sl_bCurrentRelayModeMustSentOwnVideoFeed )
   {
      sl_bCurrentRelayModeMustSentOwnVideoFeed = bMustSendOwnVideo;
      log_line("Flag to tell if this vehicle should send it's own video feeds changed to: %s", bMustSendOwnVideo?"true":"false");
   }
   return bMustSendOwnVideo;
}

bool relay_current_vehicle_must_send_relayed_video_feeds()
{
   return relay_vehicle_must_forward_relayed_video(g_pCurrentModel);
}