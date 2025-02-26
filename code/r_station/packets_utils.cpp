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

#include "packets_utils.h"
#include "../base/config.h"
#include "../base/flags.h"
#include "../base/encr.h"
#include "../base/models_list.h"
#include "../base/hardware_radio.h"
#include "../base/radio_utils.h"
#include "../base/commands.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/radio_tx.h"
#include "../base/ctrl_interfaces.h"

#include "radio_links_sik.h"
#include "processor_rx_video.h"
#include "shared_vars.h"
#include "shared_vars_state.h"
#include "timers.h"
#include "test_link_params.h"
#include "ruby_rt_station.h"

u8 s_RadioRawPacket[MAX_PACKET_TOTAL_SIZE];

u32 s_StreamsTxPacketIndex[MAX_RADIO_STREAMS];
u16 s_StreamsLastTxTime[MAX_RADIO_STREAMS];

u32 s_TimeLastLogAlarmNoInterfacesCanSend = 0;

int s_LastSetAtherosCardsDatarates[MAX_RADIO_INTERFACES];

bool s_bFirstTimeLogTxAssignment = true;

void packet_utils_init()
{
   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      s_StreamsTxPacketIndex[i] = 0;
      s_StreamsLastTxTime[i] = 0;
   }
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_LastSetAtherosCardsDatarates[i] = 5000;
}


void _computeBestTXCardsForEachLocalRadioLink(int* pIndexCardsForRadioLinks)
{
   if ( NULL == pIndexCardsForRadioLinks )
      return;

   int iBestRXQualityForRadioLink[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      iBestRXQualityForRadioLink[i] = -1000000;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      pIndexCardsForRadioLinks[i] = -1;

   int iCountRadioLinks = g_SM_RadioStats.countLocalRadioLinks;
   if ( iCountRadioLinks < 1 )
      iCountRadioLinks = 1;
   for( int iRadioLink = 0; iRadioLink < iCountRadioLinks; iRadioLink++ )
   {
      int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iRadioLink].matchingVehicleRadioLinkId;

      // Radio link is downlink only or a relay link ? Controller can't send data on it (uplink)

      if ( NULL != g_pCurrentModel )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
       
      if ( NULL != g_pCurrentModel )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;
      
      int iMinPrefferedIndex = 10000;
      int iPrefferedCardIndex = -1;

      // Iterate all radio interfaces assigned to this local radio link

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
            break;
         }

         int iRadioLinkForCard = g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId;
         if ( (iRadioLinkForCard < 0) || (iRadioLinkForCard != iRadioLink) )
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

         int iRadioLinkForCard = g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId;
         if ( (iRadioLinkForCard < 0) || (iRadioLinkForCard != iRadioLink) )
            continue;

         if ( -1 == pIndexCardsForRadioLinks[iRadioLink] )
         {
            pIndexCardsForRadioLinks[iRadioLink] = i;
            iBestRXQualityForRadioLink[iRadioLink] = g_SM_RadioStats.radio_interfaces[i].rxRelativeQuality;
         }
         else if ( g_SM_RadioStats.radio_interfaces[i].rxRelativeQuality > iBestRXQualityForRadioLink[iRadioLink] )
         {
            pIndexCardsForRadioLinks[iRadioLink] = i;
            iBestRXQualityForRadioLink[iRadioLink] = g_SM_RadioStats.radio_interfaces[i].rxRelativeQuality;
         }
      }
      if ( s_bFirstTimeLogTxAssignment )
      {
         if ( -1 == pIndexCardsForRadioLinks[iRadioLink] )
            log_softerror_and_alarm("No Tx card was assigned to local radio link %d.", iRadioLink+1);
         else
            log_line("Assigned radio card %d as best Tx card for local radio link %d.", pIndexCardsForRadioLinks[iRadioLink]+1, iRadioLink+1);
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
      if ( getDataRatesBPS()[i] == iDataRate )
      {
         iCurrentIndex = i;
         break;
      }
   }

   // Do not decrease below 2 mbps
   for( int i=0; i<iLevelsDown; i++ )
   {
      if ( (iCurrentIndex > 0) )
      if ( getRealDataRateFromRadioDataRate(getDataRatesBPS()[iCurrentIndex], 0) > DEFAULT_RADIO_DATARATE_LOWEST)
         iCurrentIndex--;
   }

   if ( iCurrentIndex >= 0 )
      iDataRate = getDataRatesBPS()[iCurrentIndex];
   return iDataRate;
}

int compute_packet_uplink_datarate(int iVehicleRadioLink, int iRadioInterface, type_radio_links_parameters* pRadioLinksParams, u8* pPacketData)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterface);
   if ( (NULL == pRadioHWInfo) || (NULL == pRadioLinksParams) )
      return DEFAULT_RADIO_DATARATE_DATA;
   
   if ( (pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS) ||
        (pRadioHWInfo->iRadioType == RADIO_TYPE_RALINK) )
      return DEFAULT_RADIO_DATARATE_LOWEST;

   int nRateTx = DEFAULT_RADIO_DATARATE_DATA;
   if ( NULL == g_pCurrentModel )
      return nRateTx;

   bool bUsesHT40 = false;
   if ( g_pCurrentModel->radioLinksParams.link_radio_flags[iVehicleRadioLink] & RADIO_FLAG_HT40_VEHICLE )
      bUsesHT40 = true;

   int nVideoProfileAdaptive = -1;
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(g_pCurrentModel->uVehicleId);
   if ( NULL != pRuntimeInfo )
      nVideoProfileAdaptive = pRuntimeInfo->uPendingVideoProfileToSet;
   if ( (nVideoProfileAdaptive < 0) || (nVideoProfileAdaptive >= VIDEO_PROFILE_LAST) )
      nVideoProfileAdaptive = -1;

   switch ( pRadioLinksParams->uUplinkDataDataRateType[iVehicleRadioLink] )
   {
      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_FIXED:
         nRateTx = pRadioLinksParams->uplink_datarate_data_bps[iVehicleRadioLink];
         break;

      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO:
         nRateTx = pRadioLinksParams->link_datarate_video_bps[iVehicleRadioLink];
         if ( NULL != g_pCurrentModel )
         {
            // User set fixed datarate for this user profile?
            if ( 0 != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps )
            if ( getRealDataRateFromRadioDataRate(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps, bUsesHT40) < getRealDataRateFromRadioDataRate(nRateTx, bUsesHT40) )
               nRateTx = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps;
            
            // If no adaptive video profile is active, use video MQ profile for data uplink  
            // An adaptive video profile is active?
            bool bUpdatedWithAdaptive = false;
            if ( (nVideoProfileAdaptive >= 0) && (nVideoProfileAdaptive < MAX_VIDEO_LINK_PROFILES) )
            if ( nVideoProfileAdaptive != g_pCurrentModel->video_params.user_selected_video_link_profile )
            {
               // Fixed datarate for this adaptive profile?
               int nRate = g_pCurrentModel->video_link_profiles[nVideoProfileAdaptive].radio_datarate_video_bps;
               if ( nRate != 0 )
               if ( getRealDataRateFromRadioDataRate(nRate, bUsesHT40) < getRealDataRateFromRadioDataRate(nRateTx, bUsesHT40) )
               {
                  nRateTx = nRate;
                  bUpdatedWithAdaptive = true;
               }
               // Use computed auto datarate for this adaptive video profile
               if ( nVideoProfileAdaptive == VIDEO_PROFILE_MQ )
               {
                  nRate = utils_get_video_profile_mq_radio_datarate(g_pCurrentModel);
                  if ( getRealDataRateFromRadioDataRate(nRate, bUsesHT40) < getRealDataRateFromRadioDataRate(nRateTx, bUsesHT40) )
                  {
                     nRateTx = nRate;
                     bUpdatedWithAdaptive = true;
                  }
               }
               if ( nVideoProfileAdaptive == VIDEO_PROFILE_LQ )
               {
                  nRate = utils_get_video_profile_lq_radio_datarate(g_pCurrentModel);
                  if ( getRealDataRateFromRadioDataRate(nRate, bUsesHT40) < getRealDataRateFromRadioDataRate(nRateTx, bUsesHT40) )
                  {
                     nRateTx = nRate;
                     bUpdatedWithAdaptive = true;
                  }
               }

               if ( ! bUpdatedWithAdaptive )
               {
                  nRate = utils_get_video_profile_mq_radio_datarate(g_pCurrentModel);
                  if ( getRealDataRateFromRadioDataRate(nRate, bUsesHT40) < getRealDataRateFromRadioDataRate(nRateTx, bUsesHT40) )
                  {
                     nRateTx = nRate;
                     bUpdatedWithAdaptive = true;
                  }                
               }
            }
         }
         break;

      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST:
      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO:
      default:
         if ( pRadioLinksParams->link_datarate_video_bps[iVehicleRadioLink] > 0 )
            nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         else
            nRateTx = -1;
         break;
   }  

   bool bUseLowest = false;
   if ( NULL != g_pCurrentModel )
   {
      int iRuntimeIndex = getVehicleRuntimeIndex(g_pCurrentModel->uVehicleId);
      if ( -1 != iRuntimeIndex )
      {
         if ( g_State.vehiclesRuntimeInfo[iRuntimeIndex].bIsVehicleLinkToControllerLostAlarm )
            bUseLowest = true;
         if ( g_State.vehiclesRuntimeInfo[iRuntimeIndex].uLastTimeReceivedAckFromVehicle > g_TimeNow + 1000 )
            bUseLowest = true;
      }
   }

   if ( NULL != pPacketData )
   {
      t_packet_header* pPH = (t_packet_header*)pPacketData;
      // Use lowest datarate for these packets.
      if ( (pPH->packet_type == PACKET_TYPE_NEGOCIATE_RADIO_LINKS) ||
           (pPH->packet_type == PACKET_TYPE_COMMAND) ||
           (pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST) )
         bUseLowest = true;
   }

   if ( bUseLowest )
   {
      if ( nRateTx > 0 )
         nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
      else
         nRateTx = -1;
   }

   return nRateTx;
}

bool _send_packet_to_serial_radio_interface(int iLocalRadioLinkId, int iRadioInterfaceIndex, u8* pPacketData, int nPacketLength)
{
   if ( (NULL == pPacketData) || (nPacketLength <= 0) || (NULL == g_pCurrentModel) )
      return false;
    
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return false;

   int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId;
   if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
      return false;
     
   // Do not send packet if the link is overloaded
   int iAirRate = g_pCurrentModel->radioLinksParams.link_datarate_data_bps[iVehicleRadioLinkId]/8;
   if ( hardware_radio_index_is_sik_radio(iRadioInterfaceIndex) )
      iAirRate = hardware_radio_sik_get_air_baudrate_in_bytes(iRadioInterfaceIndex);

   t_packet_header* pPH = (t_packet_header*)pPacketData;
   u8 uPacketType = pPH->packet_type;
   if ( ! radio_can_send_packet_on_slow_link(iLocalRadioLinkId, uPacketType, 1, g_TimeNow) )
      return false;
      
   if ( iAirRate > 0 )
   if ( g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec >= (DEFAULT_RADIO_SERIAL_MAX_TX_LOAD * (u32)iAirRate) / 100 )
   {
      static u32 sl_uLastTimeInterfaceTxOverloaded = 0;
      if ( g_TimeNow > sl_uLastTimeInterfaceTxOverloaded + 20000 )
      {
         sl_uLastTimeInterfaceTxOverloaded = g_TimeNow;
         log_line("Radio interface %d is tx overloaded: sending %d bytes/sec and air data rate is %d bytes/sec", iRadioInterfaceIndex+1, (int)g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec, iAirRate);
         send_alarm_to_central(ALARM_ID_RADIO_LINK_DATA_OVERLOAD, (g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec & 0xFFFFFF) | (((u32)iRadioInterfaceIndex)<<24), (u32)iAirRate);
      }
      return false;
   }

   if ( (iLocalRadioLinkId < 0) || (iLocalRadioLinkId >= MAX_RADIO_INTERFACES) )
      iLocalRadioLinkId = 0;
   u16 uRadioLinkPacketIndex = radio_get_next_radio_link_packet_index(iLocalRadioLinkId);
   pPH->radio_link_packet_index = uRadioLinkPacketIndex;

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
      radio_packet_compute_crc((u8*)pPH, sizeof(t_packet_header));
   else
      radio_packet_compute_crc((u8*)pPH, pPH->total_length);

   if ( 1 != pRadioHWInfo->openedForWrite )
   {
      log_softerror_and_alarm("Radio serial interface %d is not opened for write. Can't send packet on it.", iRadioInterfaceIndex+1);
      return false;
   }

   int iWriteResult = radio_tx_send_serial_radio_packet(iRadioInterfaceIndex, pPacketData, nPacketLength);
   if ( iWriteResult > 0 )
   {
      int iTotalSent = nPacketLength;
      if ( g_pCurrentModel->radioLinksParams.iSiKPacketSize > 0 )
         iTotalSent += sizeof(t_packet_header_short) * (int) (nPacketLength / g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      radio_stats_update_on_packet_sent_on_radio_interface(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, iTotalSent);
      radio_stats_update_on_packet_sent_on_radio_link(&g_SM_RadioStats, g_TimeNow, iLocalRadioLinkId, (int)uStreamId, nPacketLength);
      return true;
   }

   log_softerror_and_alarm("Failed to write to serial radio interface %d.", iRadioInterfaceIndex+1);
   if ( iWriteResult == -2 )
   if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      radio_links_flag_reinit_sik_interface(iRadioInterfaceIndex);
   return false;
}

bool _send_packet_to_wifi_radio_interface(int iLocalRadioLinkId, int iRadioInterfaceIndex, u8* pPacketData, int nPacketLength)
{
   if ( (NULL == pPacketData) || (nPacketLength <= 0) || (NULL == g_pCurrentModel) )
      return false;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return false;

   int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId;
   if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
      return false;
   
   u32 microT = get_current_timestamp_micros();

   int nRateTx = compute_packet_uplink_datarate(iVehicleRadioLinkId, iRadioInterfaceIndex, &(g_pCurrentModel->radioLinksParams), pPacketData);
   radio_set_out_datarate(nRateTx);

   u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[iVehicleRadioLinkId];
   radio_set_frames_flags(radioFlags);

   if ( test_link_is_in_progress() )
      log_line("Test link in progress. Sending radio packet using datarate: %d", nRateTx);
     
   if ( (pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS) ||
        (pRadioHWInfo->iRadioType == RADIO_TYPE_RALINK) )
   {
      //update_atheros_card_datarate(g_pCurrentModel, iRadioInterfaceIndex, nRateTx, g_pProcessStats);
      g_TimeNow = get_current_timestamp_ms();
   }

   int be = 0;
   if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
   if ( hpp() )
      be = 1;

   int totalLength = radio_build_new_raw_ieee_packet(iLocalRadioLinkId, s_RadioRawPacket, pPacketData, nPacketLength, RADIO_PORT_ROUTER_UPLINK, be);
   int iRepeatCount = 0;
   bool bShouldDuplicate = false;

   t_packet_header* pPH = (t_packet_header*)pPacketData;
   if ( (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS) ||
        (pPH->packet_type == PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE) ||
        (pPH->packet_type == PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL) )
      bShouldDuplicate = true;

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   if ( pPH->packet_type == PACKET_TYPE_COMMAND )
   {
      Model* pModel = findModelWithId(pPH->vehicle_id_dest, 7);
      if ( (NULL != pModel) && (pModel->uModelFlags & MODEL_FLAG_PRIORITIZE_UPLINK) )
      {
         t_packet_header_command* pPHC = (t_packet_header_command*)(pPacketData + sizeof(t_packet_header));
         if ( pPHC->command_type == COMMAND_ID_SET_RADIO_LINK_FREQUENCY )
            bShouldDuplicate = true;
      }
   }

   if ( bShouldDuplicate )
      iRepeatCount++;

   if ( radio_write_raw_ieee_packet(iRadioInterfaceIndex, s_RadioRawPacket, totalLength, iRepeatCount) )
   {
      radio_stats_update_on_packet_sent_on_radio_interface(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, nPacketLength);
      radio_stats_set_tx_radio_datarate_for_packet(&g_SM_RadioStats, iRadioInterfaceIndex, iLocalRadioLinkId, nRateTx, 0);
      
      u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      
      radio_stats_update_on_packet_sent_on_radio_link(&g_SM_RadioStats, g_TimeNow, iLocalRadioLinkId, (int)uStreamId, pPH->total_length);
      s_StreamsLastTxTime[uStreamId] = get_current_timestamp_micros() - microT;

      if ( pPH->packet_type == PACKET_TYPE_SIK_CONFIG )
      {
         u8 uVehicleLinkId = *(pPacketData + sizeof(t_packet_header));
         u8 uCommandId = *(pPacketData + sizeof(t_packet_header) + sizeof(u8));
         log_line("Sent radio packet to vehicle to configure SiK vehicle radio link %d, command: %d", (int) uVehicleLinkId+1, (int)uCommandId);
      }
      return true;
   }
   
   log_softerror_and_alarm("Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   return false;
}

// Returns -1 on error, 0 on success
int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength, int iSendToSingleRadioLink, int iRepeatCount, int iTraceSource)
{
   if ( nPacketLength <= 0 )
      return -1;

   // Figure out the best radio interface to use for Tx for each radio link;

   int iTXInterfaceIndexForLocalRadioLinks[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      iTXInterfaceIndexForLocalRadioLinks[i] = -1;

   _computeBestTXCardsForEachLocalRadioLink( &iTXInterfaceIndexForLocalRadioLinks[0] );

   bool bIsPingPacket = false;
   int iPingOnLocalRadioLinkId = -1;
   
   // Set packet index

   t_packet_header* pPH = (t_packet_header*)pPacketData;
   u8 uPacketFlags = pPH->packet_flags;
   u8 uPacketType = pPH->packet_type;
   int iPacketLength = pPH->total_length;
   u32 uDestVehicleId = pPH->vehicle_id_dest;
   u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   {
      u8 uLocalRadioLinkId = 0;
      memcpy( &uLocalRadioLinkId, pPacketData + sizeof(t_packet_header)+sizeof(u8), sizeof(u8));
      iPingOnLocalRadioLinkId = (int)uLocalRadioLinkId;
      bIsPingPacket = true;
   }

   if ( pPH->packet_type == PACKET_TYPE_TEST_RADIO_LINK )
   {
      int iModelRadioLinkIndex = pPacketData[sizeof(t_packet_header)+2];
      int iCmdType = pPacketData[sizeof(t_packet_header)+4];
      for( int iRadioLink = 0; iRadioLink < g_SM_RadioStats.countLocalRadioLinks; iRadioLink++ )
      {
         int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iRadioLink].matchingVehicleRadioLinkId;
         if ( iCmdType != PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START )
         if ( iCmdType != PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END )
         if ( iVehicleRadioLinkId == iModelRadioLinkIndex )
         {
            iSendToSingleRadioLink = iRadioLink;
         }
      }
   }
     
   if ( uPacketType != PACKET_TYPE_RUBY_PING_CLOCK )
   if ( uPacketType != PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
      s_StreamsTxPacketIndex[uStreamId]++;

   pPH->stream_packet_idx = (((u32)uStreamId)<<PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) | (s_StreamsTxPacketIndex[uStreamId] & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
   
   // Send the packet to each local radio link

   bool bPacketSent = false;

   for( int iLocalRadioLinkId=0; iLocalRadioLinkId<g_SM_RadioStats.countLocalRadioLinks; iLocalRadioLinkId++ )
   {
      int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId;
      int iRadioInterfaceIndex = iTXInterfaceIndexForLocalRadioLinks[iLocalRadioLinkId];
      if ( iRadioInterfaceIndex < 0 )
      {
         if ( g_TimeNow > s_TimeLastLogAlarmNoInterfacesCanSend + 20000 )
         {
            s_TimeLastLogAlarmNoInterfacesCanSend = g_TimeNow;
            log_softerror_and_alarm("No radio interfaces on controller can send data on local radio link %d", iLocalRadioLinkId+1);
         }
         continue;
      }

      if ( (-1 != iSendToSingleRadioLink) && (iLocalRadioLinkId != iSendToSingleRadioLink) )
         continue;
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      // Do not try to send packets on the vehicle radio link that is assigned for relaying, if any
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;
        
      // Send update packets only on a single radio interface, not on multiple radio links
      if ( g_bUpdateInProgress && bPacketSent )
         break;

      // Send Ping packets only to the assigned radio link
      if ( bIsPingPacket )
      if ( iLocalRadioLinkId != iPingOnLocalRadioLinkId )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( ! pRadioHWInfo->openedForWrite )
         continue;

      g_SM_RadioStats.radio_links[iLocalRadioLinkId].lastTxInterfaceIndex = iRadioInterfaceIndex;

      radio_stats_set_tx_card_for_radio_link(&g_SM_RadioStats, iLocalRadioLinkId, iRadioInterfaceIndex);

      for( int i=0; i<=iRepeatCount; i++ )
      {
         if ( hardware_radio_index_is_serial_radio(iRadioInterfaceIndex) )
         {
            if ( g_bUpdateInProgress )
               continue;
            bPacketSent |= _send_packet_to_serial_radio_interface(iLocalRadioLinkId, iRadioInterfaceIndex, pPacketData, nPacketLength);
         }
         else
            bPacketSent |= _send_packet_to_wifi_radio_interface(iLocalRadioLinkId, iRadioInterfaceIndex, pPacketData, nPacketLength);
      
         if ( i < iRepeatCount )
            hardware_sleep_micros(400);
      }
   }


   if ( bPacketSent )
   {
      if ( radio_packet_type_is_high_priority(uPacketFlags, uPacketType) )
         g_SMControllerRTInfo.uTxHighPriorityPackets[g_SMControllerRTInfo.iCurrentIndex]++;
      else
         g_SMControllerRTInfo.uTxPackets[g_SMControllerRTInfo.iCurrentIndex]++;
      
      radio_stats_update_on_packet_sent_for_radio_stream(&g_SM_RadioStats, g_TimeNow, uDestVehicleId, uStreamId, uPacketType, iPacketLength);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastRadioTxTime = g_TimeNow;

      #ifdef LOG_RAW_TELEMETRY
      if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_UPLOAD )
      {
         t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)(pPacketData + sizeof(t_packet_header));
         log_line("[Raw_Telem] Send raw telemetry packet to radio interfaces, index %u, %d / %d bytes", pPHTR->telem_segment_index, pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_telemetry_raw), pPH->total_length);
      }
      #endif
      return 0;
   }

   log_softerror_and_alarm("Packet not sent! No radio interface could send it. Packet type: %s", str_get_packet_type(uPacketType));
   char szFreq1[64];
   char szFreq2[64];
   char szFreq3[64];
   char szTmp[256];
   strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[0]));
   strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[1]));
   strcpy(szFreq3, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[2]));

   log_softerror_and_alarm("Current local radio links: %d, current model links frequencies: 1: %s, 2: %s, 3: %s", g_SM_RadioStats.countLocalRadioLinks, szFreq1, szFreq2, szFreq3 );
   for( int i=0; i<g_SM_RadioStats.countLocalRadioLinks; i++ )
   {
      int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId;

      int nicIndex = iTXInterfaceIndexForLocalRadioLinks[i];
      if ( nicIndex < 0 || nicIndex > hardware_get_radio_interfaces_count() )
      {
         log_softerror_and_alarm("No radio interfaces assigned for Tx on local radio link %d.", i+1);
         continue;          
      }
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(nicIndex);
      if ( NULL == pRadioHWInfo )
      {
         log_softerror_and_alarm("Can't get NIC info for radio interface %d", nicIndex+1);
         continue;
      }
      log_softerror_and_alarm("Current radio interface used for TX on local radio link %d, vehicle radio link %d: %d, freq: %s",
         i+1, iVehicleRadioLinkId+1, nicIndex+1, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz));    
      str_get_radio_capabilities_description(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId], szTmp);
      log_softerror_and_alarm("Current vehicle radio link %d capabilities: %s", iVehicleRadioLinkId+1, szTmp);
   }

   return -1;
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


int get_controller_radio_interface_index_for_radio_link(int iLocalRadioLinkId)
{
   if ( (iLocalRadioLinkId < 0) || (iLocalRadioLinkId >= g_SM_RadioStats.countLocalRadioLinks) )
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

      int iRadioLinkCard = g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId;
      if ( (iRadioLinkCard < 0) || (iRadioLinkCard != iLocalRadioLinkId) )
         continue;
      return i;
   }
   return -1;
}