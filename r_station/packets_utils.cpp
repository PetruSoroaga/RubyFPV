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

#include "packets_utils.h"
#include "../base/config.h"
#include "../base/flags.h"
#include "../base/encr.h"
#include "../base/hardware_radio.h"
#include "../base/launchers.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/ctrl_interfaces.h"

#include "processor_rx_video.h"
#include "shared_vars.h"
#include "timers.h"

static u8 s_RadioRawPackets[MAX_RADIO_STREAMS][MAX_PACKET_TOTAL_SIZE];

static u32 s_StreamsPacketIndex[MAX_RADIO_STREAMS];
static u16 s_StreamsLastTxTime[MAX_RADIO_STREAMS];
static bool s_bAnyPacketsSentToRadio = false;

static u32 s_TimeLastLogAlarmNoInterfacesCanSend = 0;

int s_LastSetAtherosCardsDatarates[MAX_RADIO_INTERFACES];


void _computeBestTXCardsForEachRadioLink(int* pIndexCardsForRadioLinks)
{
   int iBestRXQualityForRadioLink[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      iBestRXQualityForRadioLink[i] = -1000000;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo )
          continue;

      u32 cardFlags = controllerGetCardFlags(pNICInfo->szMAC);

      bool bCanSend = false;
      if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
         bCanSend = true;

      if ( ! bCanSend )
         continue;

      int iRadioLink = g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId;
      if ( iRadioLink < 0 )
         continue;

      if ( NULL == g_pCurrentModel )
      {
         pIndexCardsForRadioLinks[0] = i;
         break;
      }

      if ( pNICInfo->currentFrequency != g_pCurrentModel->radioLinksParams.link_frequency[iRadioLink] )
         continue;

      // Radio link is downlink only ? Controller can't send data on it (uplink)

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
        

      if ( -1 == pIndexCardsForRadioLinks[iRadioLink] )
      {
         pIndexCardsForRadioLinks[iRadioLink] = i;
         iBestRXQualityForRadioLink[iRadioLink] = g_Local_RadioStats.radio_interfaces[i].rxRelativeQuality;
      }
      else if ( g_Local_RadioStats.radio_interfaces[i].rxRelativeQuality > iBestRXQualityForRadioLink[iRadioLink] )
      {
         pIndexCardsForRadioLinks[iRadioLink] = i;
         iBestRXQualityForRadioLink[iRadioLink] = g_Local_RadioStats.radio_interfaces[i].rxRelativeQuality;
      }
   }
}

int _compute_packet_datarate(int iRadioLink, int iRadioInterface)
{
   int nRateTx = DEFAULT_DATARATE_DATA;

   radio_hw_info_t* pNICInfo = hardware_get_radio_info(iRadioInterface);
   if ( NULL == pNICInfo )
      nRateTx = DEFAULT_DATARATE_DATA;
   else
   {
      int nRate = controllerGetCardDataRate(pNICInfo->szMAC); // Returns 0 if radio link datarate must be used (no custom datarate set for this radio card);
      if ( nRate != 0 )
         nRateTx = nRate;
      else if ( NULL == g_pCurrentModel )
         nRateTx = DEFAULT_DATARATE_DATA;
      else
      {
         nRateTx = g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][1];

         int nVideoProfile = process_data_rx_video_get_current_received_video_profile();
         if ( nVideoProfile >= 0 && nVideoProfile < MAX_VIDEO_LINK_PROFILES )
         if ( nVideoProfile != g_pCurrentModel->video_params.user_selected_video_link_profile )
         {
            int nRate = g_pCurrentModel->video_link_profiles[nVideoProfile].radio_datarate_data;
            if ( nRate != 0 )
            if ( getDataRateRealMbRate(nRate) < getDataRateRealMbRate(nRateTx) )
               nRateTx = nRate;
         }

         bool bUseLowest = false;
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA )
            bUseLowest = true;
         if ( g_bIsVehicleLinkToControllerLost )
            bUseLowest = true;
         
         if ( bUseLowest )
         {
            if ( nRateTx > 0 )
               nRateTx = getDataRates()[0];
            else
               nRateTx = -1;
         }
      }
   }
   return nRateTx;
}

int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength)
{
   if ( ! s_bAnyPacketsSentToRadio )
   {
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         s_StreamsPacketIndex[i] = 0;
         s_StreamsLastTxTime[i] = 0;
      }
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         s_LastSetAtherosCardsDatarates[i] = 5000;
   }

   // Figure out the best radio interface to use for Tx for each radio link;

   int iTXInterfaceIndexForRadioLinks[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      iTXInterfaceIndexForRadioLinks[i] = -1;

   _computeBestTXCardsForEachRadioLink( &iTXInterfaceIndexForRadioLinks[0] );

   int be = 0;
   if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
   if ( hpp() )
      be = 1;

   //int iHasCommandPacket = 0;
   //u32 uCommandId = MAX_U32;
   //u32 uCommandResendCount = 0;
   int iTotalPackets = 0;
   bool bPacketSent = false;
   u32 uStreamId = STREAM_ID_DATA;

   //int iCountRetransmissionsPackets = 0;

   // Set packets indexes and chained flag, if multiple packets are found in the input buffer

   u8* pData = pPacketData;
   int nLength = nPacketLength;
   u32 uCountChainedPackets = 0;
   u32 uStartPacketIndex = s_StreamsPacketIndex[uStreamId];
   while ( nLength > 0 )
   {
      iTotalPackets++;
      t_packet_header* pPH = (t_packet_header*)pData;

      //if ( pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2)
      //   iCountRetransmissionsPackets++;

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
      {
         t_packet_header_command* pCom = (t_packet_header_command*)(pData + sizeof(t_packet_header));
         g_uTimeLastCommandRequestSent = g_TimeNow;
         g_uLastCommandRequestIdSent = pCom->command_counter;
         g_uLastCommandRequestIdRetrySent = pCom->command_resend_counter;
         //iHasCommandPacket++;
         //uCommandId = pCom->command_counter;
         //uCommandResendCount = pCom->command_resend_counter;

      }
      pPH->stream_packet_idx = (((u32)uStreamId)<<28) | (uStartPacketIndex & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
      if ( s_bReceivedInvalidRadioPackets )
      {
         pPH->extra_flags = 0;
         pPH->vehicle_id_src = 0;
      }
      nLength -= pPH->total_length;
      pData += pPH->total_length;
      uStartPacketIndex++;
      uCountChainedPackets++;
   }

   // Send the final composed packet to each radio link

   for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      // Send update packets only on a single radio interface, not on multiple radio links
      if ( g_bUpdateInProgress && bPacketSent )
         break;

      int iRadioInterfaceIndex = iTXInterfaceIndexForRadioLinks[iLink];
      if ( iRadioInterfaceIndex < 0 )
      {
         if ( g_TimeNow > s_TimeLastLogAlarmNoInterfacesCanSend + 500 )
         {
            s_TimeLastLogAlarmNoInterfacesCanSend = g_TimeNow;
            log_softerror_and_alarm("No radio interfaces on controller can send data on radio link %d", iLink+1);
         }
         continue;
      }
      g_Local_RadioStats.radio_links[iLink].lastTxInterfaceIndex = iRadioInterfaceIndex;

      radio_stats_set_tx_card_for_radio_link(&g_Local_RadioStats, iLink, iRadioInterfaceIndex);

      u32 microT = get_current_timestamp_micros();

      if ( NULL != g_pCurrentModel )
      {
         u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[iLink];
         if ( ! (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER) )
         {
            radioFlags &= ~(RADIO_FLAGS_STBC | RADIO_FLAGS_LDPC | RADIO_FLAGS_SHORT_GI | RADIO_FLAGS_HT40);
            radioFlags |= RADIO_FLAGS_HT20;
         }
         radio_set_frames_flags(radioFlags);
      }
      else
         radio_set_frames_flags(DEFAULT_RADIO_FRAMES_FLAGS);

      int nRateTx = _compute_packet_datarate(iLink, iRadioInterfaceIndex);
      radio_set_out_datarate(nRateTx);

      int totalLength = radio_build_packet(s_RadioRawPackets[uStreamId], pPacketData, nPacketLength, RADIO_PORT_ROUTER_UPLINK, be, 0, NULL);
      if ( radio_write_packet(iRadioInterfaceIndex, s_RadioRawPackets[uStreamId], totalLength) )
      {
         radio_stats_update_on_packet_sent_on_radio_interface(&g_Local_RadioStats, g_TimeNow, iRadioInterfaceIndex, iLink, (int)uStreamId, nPacketLength, (int)uCountChainedPackets);
         radio_stats_update_on_packet_sent_on_radio_link(&g_Local_RadioStats, g_TimeNow, iRadioInterfaceIndex, iLink, (int)uStreamId, nPacketLength, (int)uCountChainedPackets);

         s_StreamsLastTxTime[uStreamId] = get_current_timestamp_micros() - microT;
         bPacketSent = true;
         //if ( iHasCommandPacket > 0 )
         //   log_line("Sent %d commands (cid: %u, retry: %u) to v. using r. link %d, on r. int. %d (%s, %s), freq %d Mhz, total concat. pack.: %d, %d bytes", iHasCommandPacket, uCommandId, uCommandResendCount, iLink+1, iRadioInterfaceIndex+1, pNICInfo->szUSBPort, pNICInfo->szDriver, pNICInfo->currentFrequency, iTotalPackets, nPacketLength);
         hardware_sleep_micros(500);
      }
      else
         log_softerror_and_alarm("Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   }


   if ( bPacketSent )
   {
      radio_stats_update_on_packet_sent_for_radio_stream(&g_Local_RadioStats, g_TimeNow, (int)uStreamId, nPacketLength);

      s_StreamsPacketIndex[uStreamId] += uCountChainedPackets;

      s_bAnyPacketsSentToRadio = true;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastRadioTxTime = g_TimeNow;
   }
   else
   {
      log_softerror_and_alarm("Packet not sent! No radio interface could send it.");
      log_softerror_and_alarm("Current model links frequencies: 1: %d Mhz, 2: %d Mhz, 3: %d Mhz", g_pCurrentModel->radioLinksParams.link_frequency[0], g_pCurrentModel->radioLinksParams.link_frequency[1], g_pCurrentModel->radioLinksParams.link_frequency[2] );
      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      {
         int nicIndex = iTXInterfaceIndexForRadioLinks[i];
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(nicIndex);
         if ( NULL == pNICInfo )
         {
            log_softerror_and_alarm("Can't get NIC info for radio interface %d", nicIndex+1);
            continue;
         }
         log_softerror_and_alarm("Current radio interface used for TX on radio link %d: %d, freq: %d Mhz", i+1, nicIndex+1, pNICInfo->currentFrequency);
      }
   }
   return 0;
}

int get_controller_radio_link_stats_size()
{
   #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
   int len = sizeof(u8) + sizeof(u32) + 3 * sizeof(u8);
   len += sizeof(u8) * CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES * g_PD_ControllerLinkStats.radio_interfaces_count;
   len += sizeof(u8) * CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES * 5 * g_PD_ControllerLinkStats.video_streams_count;
   return len;
   #endif

   return 0;
}

void add_controller_radio_link_stats_to_buffer(u8* pDestBuffer)
{
   #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
   
   *pDestBuffer = g_PD_ControllerLinkStats.flagsAndVersion;
   pDestBuffer++;
   memcpy(pDestBuffer, &g_PD_ControllerLinkStats.lastUpdateTime, sizeof(u32));
   pDestBuffer += sizeof(u32);
   *pDestBuffer = CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES;
   pDestBuffer++;
   *pDestBuffer = g_PD_ControllerLinkStats.radio_interfaces_count;
   pDestBuffer++;
   *pDestBuffer = g_PD_ControllerLinkStats.video_streams_count;
   pDestBuffer++;

   for( int i=0; i<g_PD_ControllerLinkStats.radio_interfaces_count; i++ )
   {
      memcpy(pDestBuffer, &(g_PD_ControllerLinkStats.radio_interfaces_rx_quality[i][0]), CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES);
      pDestBuffer += CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES;
   }

   for( int i=0; i<g_PD_ControllerLinkStats.video_streams_count; i++ )
   {
      memcpy(pDestBuffer, &(g_PD_ControllerLinkStats.radio_streams_rx_quality[i][0]), CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES);
      pDestBuffer += CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES;
      memcpy(pDestBuffer, &(g_PD_ControllerLinkStats.video_streams_blocks_clean[i][0]), CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES);
      pDestBuffer += CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES;
      memcpy(pDestBuffer, &(g_PD_ControllerLinkStats.video_streams_blocks_reconstructed[i][0]), CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES);
      pDestBuffer += CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES;
      memcpy(pDestBuffer, &(g_PD_ControllerLinkStats.video_streams_blocks_max_ec_packets_used[i][0]), CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES);
      pDestBuffer += CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES;
      memcpy(pDestBuffer, &(g_PD_ControllerLinkStats.video_streams_requested_retransmission_packets[i][0]), CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES);
      pDestBuffer += CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES;
   }
   #endif
}
