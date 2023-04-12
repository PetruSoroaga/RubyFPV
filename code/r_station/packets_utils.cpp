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

static u8 s_RadioRawPacket[MAX_PACKET_TOTAL_SIZE];

static u32 s_StreamsPacketIndex[MAX_RADIO_STREAMS];
static u16 s_StreamsLastTxTime[MAX_RADIO_STREAMS];
static bool s_bAnyPacketsSentToRadio = false;

static u32 s_TimeLastLogAlarmNoInterfacesCanSend = 0;

int s_LastSetAtherosCardsDatarates[MAX_RADIO_INTERFACES];

static bool s_bFirstTimeLogTxAssignment = true;

u32 get_stream_next_packet_index(int iStreamId)
{
   u32 uVal = s_StreamsPacketIndex[iStreamId];
   s_StreamsPacketIndex[iStreamId]++;
   return uVal;
}


void _computeBestTXCardsForEachRadioLink(int* pIndexCardsForRadioLinks)
{
   if ( NULL == pIndexCardsForRadioLinks )
      return;

   int iBestRXQualityForRadioLink[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      iBestRXQualityForRadioLink[i] = -1000000;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      pIndexCardsForRadioLinks[i] = -1;

   int iCountRadioLinks = g_Local_RadioStats.countRadioLinks;
   if ( iCountRadioLinks < 1 )
      iCountRadioLinks = 1;
   for( int iRadioLink = 0; iRadioLink < iCountRadioLinks; iRadioLink++ )
   {
      // Radio link is downlink only ? Controller can't send data on it (uplink)

      if ( NULL != g_pCurrentModel )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
       
      int iMinPrefferedIndex = 10000;
      int iPrefferedCardIndex = -1;

      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
             continue;

         u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);

         if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) ||
              ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX)) ||
              ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
            continue;

         if ( ! pRadioHWInfo->isTxCapable )
            continue;

         if ( NULL == g_pCurrentModel )
         {
            pIndexCardsForRadioLinks[iRadioLink] = i;
            return;
         }

         int iRadioLinkCard = g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId;
         if ( (iRadioLinkCard < 0) || (iRadioLinkCard != iRadioLink) )
            continue;

         int iCardTxIndex = controllerIsCardTXPreferred(pRadioHWInfo->szMAC);
         if ( iCardTxIndex > 0 && iCardTxIndex < iMinPrefferedIndex )
         {
            iMinPrefferedIndex = iCardTxIndex;
            iPrefferedCardIndex = i;
         }
      }
      if ( iPrefferedCardIndex >= 0 )
      {
         pIndexCardsForRadioLinks[iRadioLink] = iPrefferedCardIndex;
         if ( s_bFirstTimeLogTxAssignment )
            log_line("Assigned preffered Tx card %d as Tx card for radio link %d.", iPrefferedCardIndex+1, iRadioLink+1);
         continue;
      }
   
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
             continue;

         u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);

         if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) ||
              ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX)) ||
              ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
            continue;

         if ( ! pRadioHWInfo->isTxCapable )
            continue;

         int iRadioLinkCard = g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId;
         if ( (iRadioLinkCard < 0) || (iRadioLinkCard != iRadioLink) )
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
      if ( s_bFirstTimeLogTxAssignment )
      {
         if ( -1 == pIndexCardsForRadioLinks[iRadioLink] )
            log_softerror_and_alarm("No Tx card was assigned to radio link %d.", iRadioLink+1);
         else
            log_line("Assigned radio card %d as best Tx card for radio link %d.", pIndexCardsForRadioLinks[iRadioLink]+1, iRadioLink+1);
      }
   }
   s_bFirstTimeLogTxAssignment = false;
}

int _get_lower_datarate_value(int iDataRate, int iLevelsDown )
{
   if ( iDataRate < 0 )
   {
      for( int i=0; i<iLevelsDown; i++ )
      {
         if ( iDataRate < -1 )
            iDataRate++;
      }

      // Lowest rate is MCS-0, about 6mbps
      return iDataRate;
   }

   int iCurrentIndex = -1;
   for( int i=0; i<getDataRatesCount(); i++ )
   {
      if ( getDataRates()[i] == iDataRate )
      {
         iCurrentIndex = i;
         break;
      }
   }

   // Do not decrease below 2 mbps
   for( int i=0; i<iLevelsDown; i++ )
   {
      if ( (iCurrentIndex > 0) )
      if ( getRealDataRateFromRadioDataRate(getDataRates()[iCurrentIndex]) > 2000000)
         iCurrentIndex--;
   }

   if ( iCurrentIndex >= 0 )
      iDataRate = getDataRates()[iCurrentIndex];
   return iDataRate;
}

int _compute_packet_datarate(int iRadioLink, int iRadioInterface)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterface);
   if ( NULL == pRadioHWInfo )
      return DEFAULT_RADIO_DATARATE;
   
   int nRateTx = controllerGetCardDataRate(pRadioHWInfo->szMAC); // Returns 0 if radio link datarate must be used (no custom datarate set for this radio card);
   if ( nRateTx != 0 )
      return nRateTx;

   nRateTx = DEFAULT_RADIO_DATARATE;
   if ( NULL == g_pCurrentModel )
      return nRateTx;
  
   nRateTx = g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][1];

   int nVideoProfile = process_data_rx_video_get_current_received_video_profile();
   if ( nVideoProfile >= 0 && nVideoProfile < MAX_VIDEO_LINK_PROFILES )
   if ( nVideoProfile != g_pCurrentModel->video_params.user_selected_video_link_profile )
   {
      int nRate = g_pCurrentModel->video_link_profiles[nVideoProfile].radio_datarate_data;
      if ( nRate != 0 )
      {
         if ( getRealDataRateFromRadioDataRate(nRate) < getRealDataRateFromRadioDataRate(nRateTx) )
            nRateTx = nRate;
      }
      else // Auto
      {
         if ( nVideoProfile == VIDEO_PROFILE_MQ )
         {
            if ( nRateTx > 0 )
            if ( getRealDataRateFromRadioDataRate(12) < getRealDataRateFromRadioDataRate(nRateTx) )
               nRateTx = 12;

            if ( nRateTx < 0 )
            if ( getRealDataRateFromRadioDataRate(-2) < getRealDataRateFromRadioDataRate(nRateTx) )
               nRateTx = -2;
         }
         if ( nVideoProfile == VIDEO_PROFILE_LQ )
         {
            if ( nRateTx > 0 )
            if ( getRealDataRateFromRadioDataRate(6) < getRealDataRateFromRadioDataRate(nRateTx) )
               nRateTx = 6;

            if ( nRateTx < 0 )
            if ( getRealDataRateFromRadioDataRate(-1) < getRealDataRateFromRadioDataRate(nRateTx) )
               nRateTx = -1;
         }
      }
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
   
   // Set packets indexes and chained counters, if multiple packets are found in the input buffer

   int iCountChainedPackets[MAX_RADIO_STREAMS];
   int iTotalBytesOnEachStream[MAX_RADIO_STREAMS];
   memset(iCountChainedPackets, 0, MAX_RADIO_STREAMS*sizeof(int));
   memset(iTotalBytesOnEachStream, 0, MAX_RADIO_STREAMS*sizeof(int));

   u8* pData = pPacketData;
   int nLength = nPacketLength;
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
      u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      pPH->stream_packet_idx = (((u32)uStreamId)<<PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) | (s_StreamsPacketIndex[uStreamId] & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
      s_StreamsPacketIndex[uStreamId]++;
      iCountChainedPackets[uStreamId]++;
      iTotalBytesOnEachStream[uStreamId] += pPH->total_length;
      if ( s_bReceivedInvalidRadioPackets )
      {
         pPH->extra_flags = 0;
         pPH->vehicle_id_src = 0;
      }
      nLength -= pPH->total_length;
      pData += pPH->total_length;
   }

   // Send the final composed packet to each radio link

   for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      // Do not try to send packets on the vehicle radio link that is assigned for relaying, if any
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;
        
      // Send update packets only on a single radio interface, not on multiple radio links
      if ( g_bUpdateInProgress && bPacketSent )
         break;

      int iRadioInterfaceIndex = iTXInterfaceIndexForRadioLinks[iLink];
      if ( iRadioInterfaceIndex < 0 )
      {
         if ( g_TimeNow > s_TimeLastLogAlarmNoInterfacesCanSend + 20000 )
         {
            s_TimeLastLogAlarmNoInterfacesCanSend = g_TimeNow;
            log_softerror_and_alarm("No radio interfaces on controller can send data on radio link %d", iLink+1);
         }
         continue;
      }

      g_Local_RadioStats.radio_links[iLink].lastTxInterfaceIndex = iRadioInterfaceIndex;

      radio_stats_set_tx_card_for_radio_link(&g_Local_RadioStats, iLink, iRadioInterfaceIndex);

      u32 microT = get_current_timestamp_micros();

      if ( hardware_radio_index_is_sik_radio(iRadioInterfaceIndex) )
      {
         pData = pPacketData;
         nLength = nPacketLength;
         
         while ( nLength > 0 )
         {
            t_packet_header* pPH = (t_packet_header*)pData;
            if ( ! radio_can_send_packet_on_slow_link(iLink, pPH->packet_type, 1, g_TimeNow) )
            {
               nLength -= pPH->total_length;
               pData += pPH->total_length;
               continue;
            }
            int iOutLen = radio_buffer_embed_packet_to_short_packet(pPH, s_RadioRawPacket, MAX_PACKET_TOTAL_SIZE);
            if ( iOutLen < (int)sizeof(t_packet_header_short) )
            {
               nLength -= pPH->total_length;
               pData += pPH->total_length;
               continue;
            }
            u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      
            if ( radio_write_sik_packet(iRadioInterfaceIndex, s_RadioRawPacket, iOutLen) > 0 )
            {
               radio_stats_update_on_packet_sent_on_radio_interface(&g_Local_RadioStats, g_TimeNow, iRadioInterfaceIndex, iOutLen);
               radio_stats_update_on_packet_sent_on_radio_link(&g_Local_RadioStats, g_TimeNow, iLink, (int)uStreamId, iOutLen, 1);
               bPacketSent = true;
               log_line("Sent embeded sik packet, type: %d, %d bytes, %d ms", pPH->packet_type, iOutLen, (get_current_timestamp_micros() - microT)/1000);
            }   
            else
               log_softerror_and_alarm("Failed to write to SiK radio interface %d.", iRadioInterfaceIndex+1);
            nLength -= pPH->total_length;
            pData += pPH->total_length;
         }
         continue;
      }

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

      int totalLength = radio_build_packet(s_RadioRawPacket, pPacketData, nPacketLength, RADIO_PORT_ROUTER_UPLINK, be, 0, NULL);
      if ( radio_write_packet(iRadioInterfaceIndex, s_RadioRawPacket, totalLength) )
      {
         radio_stats_update_on_packet_sent_on_radio_interface(&g_Local_RadioStats, g_TimeNow, iRadioInterfaceIndex, nPacketLength);
         
         for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         {
            if ( 0 == iCountChainedPackets[i] )
               continue;
            radio_stats_update_on_packet_sent_on_radio_link(&g_Local_RadioStats, g_TimeNow, iLink, i, iTotalBytesOnEachStream[i], iCountChainedPackets[i]);
            s_StreamsLastTxTime[i] = get_current_timestamp_micros() - microT;
         }
         bPacketSent = true;
         hardware_sleep_micros(500);
      }
      else
         log_softerror_and_alarm("Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   }


   if ( bPacketSent )
   {
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         if ( 0 == iCountChainedPackets[i] )
            continue;
         radio_stats_update_on_packet_sent_for_radio_stream(&g_Local_RadioStats, g_TimeNow, i, iTotalBytesOnEachStream[i]);
      }
      s_bAnyPacketsSentToRadio = true;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastRadioTxTime = g_TimeNow;
   }
   else
   {
      log_softerror_and_alarm("Packet not sent! No radio interface could send it.");
      char szFreq1[64];
      char szFreq2[64];
      char szFreq3[64];
      strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[0]));
      strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[1]));
      strcpy(szFreq3, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[2]));

      log_softerror_and_alarm("Current model links frequencies: 1: %s, 2: %s, 3: %s", szFreq1, szFreq2, szFreq3 );
      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      {
         int nicIndex = iTXInterfaceIndexForRadioLinks[i];
         if ( nicIndex < 0 || nicIndex > hardware_get_radio_interfaces_count() )
         {
            log_softerror_and_alarm("No radio interfaces assigned for Tx to radio link %d.", i+1);
            continue;          
         }
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(nicIndex);
         if ( NULL == pRadioHWInfo )
         {
            log_softerror_and_alarm("Can't get NIC info for radio interface %d", nicIndex+1);
            continue;
         }
         log_softerror_and_alarm("Current radio interface used for TX on radio link %d: %d, freq: %s", i+1, nicIndex+1, str_format_frequency(pRadioHWInfo->uCurrentFrequency));
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


int get_controller_radio_interface_index_for_radio_link(int iRadioLink)
{
   if ( (iRadioLink < 0) || (iRadioLink >= g_Local_RadioStats.countRadioLinks) )
      return -1;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
          continue;

      u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);

      if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) ||
           ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX)) ||
           ( !(cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
         continue;

      if ( ! pRadioHWInfo->isTxCapable )
         continue;

      int iRadioLinkCard = g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId;
      if ( (iRadioLinkCard < 0) || (iRadioLinkCard != iRadioLink) )
         continue;
      return i;
   }
   return -1;
}