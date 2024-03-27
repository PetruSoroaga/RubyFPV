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

#include "packets_utils.h"
#include "../base/base.h"
#include "../base/flags.h"
#include "../base/encr.h"
#include "../base/commands.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "ruby_rt_vehicle.h"
#include "shared_vars.h"
#include "timers.h"
#include "processor_tx_video.h"
#include "video_link_stats_overwrites.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radio_tx.h"

u8 s_RadioRawPacket[MAX_PACKET_TOTAL_SIZE];

u32 s_StreamsTxPacketIndex[MAX_RADIO_STREAMS];
u32 s_LastPacketsSumTxTime[MAX_RADIO_INTERFACES];
u32 s_LastVideoPacketsSumTxTime[MAX_RADIO_INTERFACES];

int s_LastTxDataRates[MAX_RADIO_INTERFACES][2];
u32 s_uTimeStartTxDataRateDivergence = 0;

bool s_bSentAnyPacket = false;

u32 s_VehicleLogSegmentIndex = 0;


typedef struct
{
   u32 uIndex; // monotonicaly increasing
   u32 uId;
   u32 uFlags1;
   u32 uFlags2;
   u32 uRepeatCount;
   u32 uStartTime;
} __attribute__((packed)) t_alarm_info;

#define MAX_ALARMS_QUEUE 20

t_alarm_info s_AlarmsQueue[MAX_ALARMS_QUEUE];
int s_AlarmsPendingInQueue = 0;
u32 s_uAlarmsIndex = 0;
u32 s_uTimeLastAlarmSentToRouter = 0;


// Returns the actual datarate used last time for data or video

int get_last_tx_used_datarate(int iInterface, int iType)
{
   return s_LastTxDataRates[iInterface][iType];
}

// Returns the actual datarate total mbps used last time for video

int get_last_tx_video_datarate_mbps()
{
   u32 nMaxRate = 0;
   u32 nMinRate = MAX_U32;

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;
      if ( !( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
         continue;
      if ( s_LastTxDataRates[i][0] == 0 )
         continue;
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioInfo) || (! pRadioInfo->isHighCapacityInterface) )
         continue;
        
      if ( getRealDataRateFromRadioDataRate(s_LastTxDataRates[i][0]) > nMaxRate )
         nMaxRate = getRealDataRateFromRadioDataRate(s_LastTxDataRates[i][0]);
      if ( getRealDataRateFromRadioDataRate(s_LastTxDataRates[i][0]) < nMinRate )
         nMinRate = getRealDataRateFromRadioDataRate(s_LastTxDataRates[i][0]);
   }
   if ( nMinRate == MAX_U32 )
      return DEFAULT_RADIO_DATARATE_VIDEO;
   return nMinRate/1000/1000;
}

int _compute_packet_datarate(bool bIsVideoPacket, bool bIsRetransmited, int iVehicleRadioLinkId, int iRadioInterface)
{
   int nRateTx = DEFAULT_RADIO_DATARATE_VIDEO;
   int nRateTxVideo = video_stats_overwrites_get_current_radio_datarate_video(iVehicleRadioLinkId, iRadioInterface);
         
   if ( bIsVideoPacket )
   {
      nRateTx = nRateTxVideo;

      //if ( bIsRetransmited )
      //   nRateTx = video_stats_overwrites_get_next_level_down_radio_datarate_video(iVehicleRadioLinkId, iRadioInterface);
      
      // To fix : OpenIPC uses now user set video profile radio datarate or radio link data rate
      #ifdef HW_PLATFORM_OPENIPC_CAMERA
      
      nRateTx = g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iVehicleRadioLinkId];
      if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_datarate_video_bps[iRadioInterface] )
         nRateTx = g_pCurrentModel->radioInterfacesParams.interface_datarate_video_bps[iRadioInterface];

      int nRateUserVideoProfile = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps;
      if ( 0 != nRateUserVideoProfile )
      if ( getRealDataRateFromRadioDataRate(nRateUserVideoProfile) < getRealDataRateFromRadioDataRate(nRateTx) )
         nRateTx = nRateUserVideoProfile;

      #endif

      if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_datarate_video_bps[iRadioInterface] )
      if ( getRealDataRateFromRadioDataRate(g_pCurrentModel->radioInterfacesParams.interface_datarate_video_bps[iRadioInterface]) < getRealDataRateFromRadioDataRate(nRateTx) )
         nRateTx = g_pCurrentModel->radioInterfacesParams.interface_datarate_video_bps[iRadioInterface];

      if ( 0 == nRateTx )
         nRateTx = DEFAULT_RADIO_DATARATE_VIDEO;

      if ( bIsRetransmited )
         return nRateTx;


      if ( 0 == s_LastTxDataRates[iRadioInterface][0] )
      {
         s_LastTxDataRates[iRadioInterface][0] = nRateTx;
         s_uTimeStartTxDataRateDivergence = 0;
      }
      
      if ( nRateTx == s_LastTxDataRates[iRadioInterface][0] )
         return nRateTx;

      if ( nRateTx > s_LastTxDataRates[iRadioInterface][0] )
      {
         s_LastTxDataRates[iRadioInterface][0] = nRateTx;
         s_uTimeStartTxDataRateDivergence = 0;
         return nRateTx;
      }
     
      // Now, the tx data rate should be lower than the video computed rate data rate
      // Start a timer, do not switch lower righaway

      if ( 0 == s_uTimeStartTxDataRateDivergence )
      {
         s_uTimeStartTxDataRateDivergence = g_TimeNow;
         nRateTx = s_LastTxDataRates[iRadioInterface][0];
      }
      else
      {
         if ( g_TimeNow < s_uTimeStartTxDataRateDivergence + DEFAULT_LOWER_VIDEO_RADIO_DATARATE_AFTER_MS )
            nRateTx = s_LastTxDataRates[iRadioInterface][0];
         else
         {
            s_uTimeStartTxDataRateDivergence = 0;
            s_LastTxDataRates[iRadioInterface][0] = nRateTx;
         }           
      }

      s_LastTxDataRates[iRadioInterface][0] = nRateTx;

      return nRateTx;
   }

   // Data packet

   switch ( g_pCurrentModel->radioLinksParams.uDownlinkDataDataRateType[iVehicleRadioLinkId] )
   {
      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_FIXED:
         nRateTx = g_pCurrentModel->radioLinksParams.link_datarate_data_bps[iVehicleRadioLinkId];
         break;

      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO:
         {
         nRateTx = g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iVehicleRadioLinkId];
         if ( 0 != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps )
         if ( getRealDataRateFromRadioDataRate(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps) < getRealDataRateFromRadioDataRate(nRateTx) )
            nRateTx = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps;
         if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile >= 0 && g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile < MAX_VIDEO_LINK_PROFILES )
         if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != g_pCurrentModel->video_params.user_selected_video_link_profile )
         {
            int nRate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].radio_datarate_video_bps;
            if ( nRate != 0 )
            if ( getRealDataRateFromRadioDataRate(nRate) < getRealDataRateFromRadioDataRate(nRateTx) )
                  nRateTx = nRate;
         }

         nRateTxVideo = video_stats_overwrites_get_current_radio_datarate_video(iVehicleRadioLinkId, iRadioInterface);
         if ( nRateTxVideo != 0 )
         if ( getRealDataRateFromRadioDataRate(nRateTxVideo) < getRealDataRateFromRadioDataRate(nRateTx) )
            nRateTx = nRateTxVideo;
         }
         break;

      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST:
      default:
         if ( g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iVehicleRadioLinkId] > 0 )
            nRateTx = DEFAULT_RADIO_DATARATE_LOWEST;
         else
            nRateTx = -1;
         break;
   }  
   
   s_LastTxDataRates[iRadioInterface][1] = nRateTx;
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
   int iAirRate = g_pCurrentModel->radioLinksParams.link_datarate_data_bps[iVehicleRadioLinkId] /8;
   if ( hardware_radio_index_is_sik_radio(iRadioInterfaceIndex) )
      iAirRate = hardware_radio_sik_get_air_baudrate_in_bytes(iRadioInterfaceIndex);

   s_LastTxDataRates[iRadioInterfaceIndex][1] = g_pCurrentModel->radioInterfacesParams.interface_datarate_data_bps[iRadioInterfaceIndex];
  
   bool bPacketsSent = true;

   u8* pData = pPacketData;
   int nLength = nPacketLength;
   while ( nLength > 0 )
   {
      t_packet_header* pPH = (t_packet_header*)pData;
      if ( ! radio_can_send_packet_on_slow_link(iLocalRadioLinkId, pPH->packet_type, 0, g_TimeNow) )
      {
         nLength -= pPH->total_length;
         pData += pPH->total_length;
         continue;
      }

      if ( pPH->total_length > 100 )
      {
         nLength -= pPH->total_length;
         pData += pPH->total_length;
         continue;
      }

      if ( iAirRate > 0 )
      if ( g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec >= (DEFAULT_RADIO_SERIAL_MAX_TX_LOAD * (u32)iAirRate) / 100 )
      {
         static u32 sl_uLastTimeInterfaceTxOverloaded = 0;
         if ( g_TimeNow > sl_uLastTimeInterfaceTxOverloaded + 20000 )
         {
            sl_uLastTimeInterfaceTxOverloaded = g_TimeNow;
            log_line("Radio interface %d is tx overloaded: sending %d bytes/sec and air data rate is %d bytes/sec", iRadioInterfaceIndex+1, (int)g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec, iAirRate);
            send_alarm_to_controller(ALARM_ID_RADIO_LINK_DATA_OVERLOAD, (g_SM_RadioStats.radio_interfaces[iRadioInterfaceIndex].txBytesPerSec & 0xFFFFFF) | (((u32)iRadioInterfaceIndex)<<24),(u32)iAirRate,0);
         }

         nLength -= pPH->total_length;
         pData += pPH->total_length;
         continue;
      }

      if ( (iLocalRadioLinkId < 0) || (iLocalRadioLinkId >= MAX_RADIO_INTERFACES) )
         iLocalRadioLinkId = 0;
      u16 uRadioLinkPacketIndex = radio_get_next_radio_link_packet_index(iLocalRadioLinkId);
      pPH->radio_link_packet_index = uRadioLinkPacketIndex;

      if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
         radio_packet_compute_crc((u8*)pPH, sizeof(t_packet_header));
      else
         radio_packet_compute_crc((u8*)pPH, pPH->total_length);

      if ( pRadioHWInfo->openedForWrite )
      {
         u32 microT1 = get_current_timestamp_micros();

         int iWriteResult = radio_tx_send_serial_radio_packet(iRadioInterfaceIndex, (u8*)pPH, pPH->total_length);
         if ( iWriteResult > 0 )
         {
            u32 microT2 = get_current_timestamp_micros();
            if ( microT2 > microT1 )
               s_LastPacketsSumTxTime[iRadioInterfaceIndex] += microT2 - microT1;
            
            int iTotalSent = pPH->total_length;
            if ( 0 < g_pCurrentModel->radioLinksParams.iSiKPacketSize )
               iTotalSent += sizeof(t_packet_header_short) * (int) (pPH->total_length / g_pCurrentModel->radioLinksParams.iSiKPacketSize);
            
            u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
            radio_stats_update_on_packet_sent_on_radio_interface(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, iTotalSent);
            radio_stats_update_on_packet_sent_on_radio_link(&g_SM_RadioStats, g_TimeNow, iLocalRadioLinkId, (int)uStreamId, pPH->total_length, 1);
         }
         else
         {
            bPacketsSent = false;
            log_softerror_and_alarm("Failed to write to serial radio interface %d.", iRadioInterfaceIndex+1);
            if ( iWriteResult == -2 )
            {
               if ( hardware_radio_index_is_sik_radio(iRadioInterfaceIndex) )
                  flag_reinit_sik_interface(iRadioInterfaceIndex);
               nLength = 0;
               break;
            }
         }
      }
      else
      {
         bPacketsSent = false;
         log_softerror_and_alarm("Radio serial interface %d is not opened for write. Can't send packet on it.", iRadioInterfaceIndex+1);
      }
      nLength -= pPH->total_length;
      pData += pPH->total_length;
   }

   return bPacketsSent;
}

bool _send_packet_to_wifi_radio_interface(int iLocalRadioLinkId, int iRadioInterfaceIndex, u8* pPacketData, int nPacketLength, bool bHasVideoPacket, bool bIsRetransmited)
{
   if ( (NULL == pPacketData) || (nPacketLength <= 0) || (NULL == g_pCurrentModel) )
      return false;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return false;

   int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId;
   if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
      return false;
   
   int nRateTx = _compute_packet_datarate(bHasVideoPacket, bIsRetransmited, iVehicleRadioLinkId, iRadioInterfaceIndex);
   
   static int nLastRateTxVideo = 0;
   if ( bHasVideoPacket && (nLastRateTxVideo != nRateTx) )
   {
      nLastRateTxVideo = nRateTx;
      //log_line("DBG Switched video data rate to %d (retransmitted: %s)", nRateTx, bIsRetransmited?"yes":"no");
   }

   radio_set_out_datarate(nRateTx);

   u32 radioFlags = g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[iRadioInterfaceIndex];
   radio_set_frames_flags(radioFlags);

   int be = 0;
   if ( g_pCurrentModel->enc_flags != MODEL_ENC_FLAGS_NONE )
   if ( hpp() )
   {
      if ( bHasVideoPacket )
      if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_VIDEO) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
         be = 1;
      if ( ! bHasVideoPacket )
      {
         if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_BEACON) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
            be = 1;
         if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
            be = 1;
      }
   }

   int totalLength = 0;
   if ( s_iPendingFrequencyChangeLinkId >= 0 && s_uPendingFrequencyChangeTo > 100 && s_uTimeFrequencyChangeRequest != 0 && g_TimeNow > s_uTimeFrequencyChangeRequest && s_uTimeFrequencyChangeRequest >= g_TimeNow - VEHICLE_SWITCH_FREQUENCY_AFTER_MS )
   {
      u8 extraData[6];
      memcpy(&extraData[0], (u8*)&s_uPendingFrequencyChangeTo, sizeof(u32));
      extraData[4] = EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK1;
      if ( s_iPendingFrequencyChangeLinkId == 1 )
         extraData[4] = EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK2;
      if ( s_iPendingFrequencyChangeLinkId == 2 )
         extraData[4] = EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK3;
      extraData[5] = 6;
      //log_line("Sending extra data: %d %d, %d, %d, %d, %d", extraData[5], extraData[4], extraData[3], extraData[2], extraData[1], extraData[0]);
      totalLength = radio_build_new_raw_packet(iLocalRadioLinkId, s_RadioRawPacket, pPacketData, nPacketLength, RADIO_PORT_ROUTER_DOWNLINK, be, 6, &extraData[0]);
   }
   else
      totalLength = radio_build_new_raw_packet(iLocalRadioLinkId, s_RadioRawPacket, pPacketData, nPacketLength, RADIO_PORT_ROUTER_DOWNLINK, be, 0, NULL);

   u32 microT1 = get_current_timestamp_micros();

   if ( radio_write_raw_packet(iRadioInterfaceIndex, s_RadioRawPacket, totalLength) )
   {       
      u32 microT2 = get_current_timestamp_micros();
      if ( microT2 > microT1 )
      {
         s_LastPacketsSumTxTime[iRadioInterfaceIndex] += microT2 - microT1;
         if ( bHasVideoPacket )
            s_LastVideoPacketsSumTxTime[iRadioInterfaceIndex] += microT2 - microT1;
      }
      radio_stats_update_on_packet_sent_on_radio_interface(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, nPacketLength);
      radio_stats_set_tx_radio_datarate_for_packet(&g_SM_RadioStats, iRadioInterfaceIndex, iLocalRadioLinkId, nRateTx, bHasVideoPacket?1:0);

      int iCountChainedPackets[MAX_RADIO_STREAMS];
      int iTotalBytesOnEachStream[MAX_RADIO_STREAMS];
      memset(iCountChainedPackets, 0, MAX_RADIO_STREAMS*sizeof(int));
      memset(iTotalBytesOnEachStream, 0, MAX_RADIO_STREAMS*sizeof(int));

      u8* pData = pPacketData;
      int nLength = nPacketLength;
      while ( nLength > 0 )
      {
         t_packet_header* pPH = (t_packet_header*)pData;
         u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;

         iCountChainedPackets[uStreamId]++;
         iTotalBytesOnEachStream[uStreamId] += pPH->total_length;

         nLength -= pPH->total_length;
         pData += pPH->total_length;
      }

      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         if ( 0 == iCountChainedPackets[i] )
            continue;
         radio_stats_update_on_packet_sent_on_radio_link(&g_SM_RadioStats, g_TimeNow, iLocalRadioLinkId, i, iTotalBytesOnEachStream[i], iCountChainedPackets[i]);
      }
      return true;
   }

   log_softerror_and_alarm("Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   return false;
}

// Sends a radio packet to all posible radio interfaces or just to a single radio link

int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength, int iSingleRadioLink)
{
   if ( ! s_bSentAnyPacket )
   {
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         s_StreamsTxPacketIndex[i] = 0;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         s_LastPacketsSumTxTime[i] = 0;
         s_LastTxDataRates[i][0] = 0;
         s_LastTxDataRates[i][1] = 0;
      }
   }

   if ( nPacketLength <= 0 )
      return -1;

   // Set packets indexes and tx times if multiple packets are found in the input buffer

   bool bHasVideoPacket = false;
   bool bIsRetransmited = false;
   bool bHasPingReplyPacket = false;
   bool bHasLowCapacityLinkOnlyPackets = false;
   bool bHasCommandParamsZipResponse = false;
   bool bHasZipParamsPacket = false;

   int iHasCommandPacket = 0;
   
   int iZipParamsPacketSize = 0;
   u32 uZipParamsUniqueIndex = 0;
   u8  uZipParamsFlags = 0;
   u8  uZipParamsSegmentIndex = 0;
   u8  uZipParamsTotalSegments = 0;
   u8  uPingReplySendOnLocalRadioLinkId = 0xFF;
   u8  uFirstPacketType = 0;
   u32 uDestVehicleId = 0;

   int iCountTotalChainedPackets = 0;
   int iCountChainedPackets[MAX_RADIO_STREAMS];
   int iTotalBytesOnEachStream[MAX_RADIO_STREAMS];
   memset(iCountChainedPackets, 0, MAX_RADIO_STREAMS*sizeof(int));
   memset(iTotalBytesOnEachStream, 0, MAX_RADIO_STREAMS*sizeof(int));

   bool bHasPingPacket = false;
   int iPingLinkId = -1;

   t_packet_header* pPHZipParams = NULL;
   t_packet_header_command_response* pPHCRZipParams = NULL;

   u8* pData = pPacketData;
   int nLength = nPacketLength;
   
   while ( nLength > 0 )
   {
      t_packet_header* pPH = (t_packet_header*)pData;
   
      if ( pPH->packet_flags_extended & PACKET_FLAGS_EXTENDED_BIT_SEND_ON_LOW_CAPACITY_LINK_ONLY )
         bHasLowCapacityLinkOnlyPackets = true;

      if ( 0 == uFirstPacketType )
         uFirstPacketType = pPH->packet_type;

      if ( pPH->packet_type == PACKET_TYPE_TEST_RADIO_LINK )
         iSingleRadioLink = pData[sizeof(t_packet_header)];

      //uPacketTypes[uCountChainedPackets] = pPH->packet_type;
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
      {
         iHasCommandPacket++;
         t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pData + sizeof(t_packet_header));
         if ( pPHCR->origin_command_type == COMMAND_ID_GET_ALL_PARAMS_ZIP )
         {
            bHasCommandParamsZipResponse = true;
            pPHZipParams = pPH;
            pPHCRZipParams = pPHCR;
         }
      }

      if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
         bIsRetransmited = true;

      if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
      {
         bHasPingReplyPacket = true;
         memcpy((u8*)&uPingReplySendOnLocalRadioLinkId, pData + sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32), sizeof(u8));
      }

      if ( pPH->packet_type == PACKET_TYPE_RUBY_MODEL_SETTINGS )
      {
         bHasZipParamsPacket = true;

         memcpy((u8*)&uZipParamsUniqueIndex, pData + sizeof(t_packet_header) + sizeof(u32), sizeof(u32));
         memcpy(&uZipParamsFlags, pData + sizeof(t_packet_header) + 2*sizeof(u32), sizeof(u8));

         if ( 0 == uZipParamsFlags )
            iZipParamsPacketSize = pPH->total_length - sizeof(t_packet_header) - 2 * sizeof(u32) - sizeof(u8);
         else
         {
            uZipParamsSegmentIndex = *(pData + sizeof(t_packet_header)+2*sizeof(u32) + sizeof(u8));
            uZipParamsTotalSegments = *(pData + sizeof(t_packet_header)+2*sizeof(u32) + 2*sizeof(u8));
            iZipParamsPacketSize = (int)(*(pData + sizeof(t_packet_header)+2*sizeof(u32) + 3*sizeof(u8)));
         }
      }

      uDestVehicleId = pPH->vehicle_id_dest;
      
      u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      if ( pPH->packet_type != PACKET_TYPE_RUBY_PING_CLOCK )
      if ( pPH->packet_type != PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
         s_StreamsTxPacketIndex[uStreamId]++;
      pPH->stream_packet_idx = (((u32)uStreamId)<<PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) | (s_StreamsTxPacketIndex[uStreamId] & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);

      if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
      {
         u8 uReplyRadioLinkId = 0;
         memcpy( &uReplyRadioLinkId, pData + sizeof(t_packet_header)+2*sizeof(u8)+sizeof(32), sizeof(u8));
         iPingLinkId = (int)uReplyRadioLinkId;
         bHasPingPacket = true;
      }

      iCountChainedPackets[uStreamId]++;
      iTotalBytesOnEachStream[uStreamId] += pPH->total_length;
      iCountTotalChainedPackets++;

      if ( uStreamId >= STREAM_ID_VIDEO_1 )
         bHasVideoPacket = true;

      nLength -= pPH->total_length;
      pData += pPH->total_length;
   }

   // Send packet on all radio links that can send this packet or just to the single radio interface that user wants
   // Exception: Ping reply packet is sent only on the associated radio link for this ping

   bool bPacketSent = false;

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iRadioLinkId].matchingVehicleRadioLinkId;
      int iRadioInterfaceIndex = -1;
      for( int k=0; k<g_pCurrentModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[k] == iVehicleRadioLinkId )
         {
            iRadioInterfaceIndex = k;
            break;
         }
      }
      if ( iRadioInterfaceIndex < 0 )
         continue;

      if ( (-1 != iSingleRadioLink) && (iRadioLinkId != iSingleRadioLink) )
         continue;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( bHasPingReplyPacket && (uPingReplySendOnLocalRadioLinkId != 0xFF) )
      if ( iRadioLinkId != (int) uPingReplySendOnLocalRadioLinkId )
         continue;

      // Do not send regular packets to controller using relay links
      if ( (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY) ||
           (g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iVehicleRadioLinkId) )
         continue;

      // Send Ping reply packets only to the assigned radio link
      if ( bHasPingPacket && (1 == iCountTotalChainedPackets) )
      if ( iRadioLinkId != iPingLinkId )
         continue;

      if ( !(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( bHasVideoPacket )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
         continue;

      if ( ! bHasVideoPacket )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( ! pRadioHWInfo->openedForWrite )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      if ( bHasVideoPacket && (!(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO)) )
         continue;
      if ( (!bHasVideoPacket) && (!(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
         continue;
      
      if ( hardware_radio_index_is_serial_radio(iRadioInterfaceIndex) )
      {
         if ( (! bHasVideoPacket) && (!bIsRetransmited) )
         if ( _send_packet_to_serial_radio_interface(iRadioLinkId, iRadioInterfaceIndex, pPacketData, nPacketLength) )
         {
            bPacketSent = true;
            if ( bHasZipParamsPacket )
            {
               if ( 0 == uZipParamsFlags )
                  log_line("Sent radio packet: zip params packet (%d bytes) (transfer id %u) as single packet to SiK radio interface %d", iZipParamsPacketSize, uZipParamsUniqueIndex, iRadioInterfaceIndex+1 );  
               else
                  log_line("Sent radio packet: zip params packet (%d bytes) (transfer id %u) as small packets (%u of %u) to SiK radio interface %d", iZipParamsPacketSize, uZipParamsUniqueIndex, uZipParamsSegmentIndex, uZipParamsTotalSegments, iRadioInterfaceIndex+1 );  
            }
         }
      }
      else
      {
         if ( bHasLowCapacityLinkOnlyPackets )
            continue;
         if ( _send_packet_to_wifi_radio_interface(iRadioLinkId, iRadioInterfaceIndex, pPacketData, nPacketLength, bHasVideoPacket, bIsRetransmited) )
         {
            bPacketSent = true;
            if ( bHasCommandParamsZipResponse )
            {
               if ( pPHZipParams->total_length > sizeof(t_packet_header) + 200 )
                  log_line("Sent radio packet: model zip params command response (%d bytes) as single zip file to radio interface %d, command index: %d, retry: %d",
                     (int)(pPHZipParams->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response)), 
                     iRadioInterfaceIndex+1,
                     (int)pPHCRZipParams->origin_command_counter, (int)pPHCRZipParams->origin_command_resend_counter );
               else
                  log_line("Sent radio packet: model zip params command response (%d bytes) as small segment (%d of %d, unique id %d) zip file to radio interface %d, command index: %d, retry: %d",
                     (int)(pPHZipParams->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response) - 4*sizeof(u8)),
                     1+(int)(*(((u8*)pPHZipParams) + sizeof(t_packet_header) + sizeof(t_packet_header_command_response) + sizeof(u8))),
                     (int)(*(((u8*)pPHZipParams) + sizeof(t_packet_header) + sizeof(t_packet_header_command_response) + 2*sizeof(u8))),
                     (int)(*(((u8*)pPHZipParams) + sizeof(t_packet_header) + sizeof(t_packet_header_command_response))),
                     iRadioInterfaceIndex+1,
                     (int)pPHCRZipParams->origin_command_counter, (int)pPHCRZipParams->origin_command_resend_counter );
            }
            if ( bHasZipParamsPacket )
            {
               if ( 0 == uZipParamsFlags )
                  log_line("Sent radio packet: zip params packet (%d bytes) (transfer id %u) as single packet to radio interface %d", iZipParamsPacketSize, uZipParamsUniqueIndex, iRadioInterfaceIndex+1 );
               else
                  log_line("Sent radio packet: zip params packet (%d bytes) (transfer id %u) as small packets (%u of %u) to radio interface %d", iZipParamsPacketSize, uZipParamsUniqueIndex, uZipParamsSegmentIndex, uZipParamsTotalSegments, iRadioInterfaceIndex+1 );
            }
         }
      }
   }

   if ( ! bPacketSent )
   {
      if ( bHasLowCapacityLinkOnlyPackets )
         return 0;

      log_softerror_and_alarm("Packet not sent! No radio interface could send it (%s, type: %d, %s, %d bytes, %d chained packets). %d radio links. %s",
         bHasVideoPacket?"video packet":"data packet",
         (int)uFirstPacketType, str_get_packet_type((int)uFirstPacketType), nPacketLength,
         (int) iCountChainedPackets[STREAM_ID_DATA], g_pCurrentModel->radioLinksParams.links_count,
         bHasLowCapacityLinkOnlyPackets?"Had low capacity link flag":"No link specific flags (low/high capacity)");
      if ( bHasPingReplyPacket )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[uPingReplySendOnLocalRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
            log_softerror_and_alarm("Packet contained a ping for relay radio link %d", ((int)uPingReplySendOnLocalRadioLinkId)+1);
         else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[uPingReplySendOnLocalRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            log_softerror_and_alarm("Packet contained a ping for disabled radio link %d", ((int)uPingReplySendOnLocalRadioLinkId)+1);
         else
            log_softerror_and_alarm("Packet contained a ping for radio link %d", ((int)uPingReplySendOnLocalRadioLinkId)+1);
      }
      else
         log_softerror_and_alarm("Packet did not contained a ping packet.");
      return 0;
   }

   // Packet sent. Update stats and info

   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      if ( 0 == iCountChainedPackets[i] )
         continue;
      radio_stats_update_on_packet_sent_for_radio_stream(&g_SM_RadioStats, g_TimeNow, uDestVehicleId, i, iTotalBytesOnEachStream[i]);
   }

   g_PHVehicleTxStats.tmp_uAverageTxCount++;

   if ( g_PHVehicleTxStats.historyTxPackets[0] < 255 )
      g_PHVehicleTxStats.historyTxPackets[0]++;

   u32 uTxGap = g_TimeNow - g_TimeLastTxPacket;
   g_TimeLastTxPacket = g_TimeNow;

   if ( uTxGap > 254 )
      uTxGap = 254;

   g_PHVehicleTxStats.tmp_uAverageTxSum += uTxGap;

   if ( 0xFF == g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0] )
      g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0] = uTxGap;
   if ( 0xFF == g_PHVehicleTxStats.historyTxGapMinMiliseconds[0] )
      g_PHVehicleTxStats.historyTxGapMinMiliseconds[0] = uTxGap;


   if ( uTxGap > g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0] )
      g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0] = uTxGap;
   if ( uTxGap < g_PHVehicleTxStats.historyTxGapMinMiliseconds[0] )
      g_PHVehicleTxStats.historyTxGapMinMiliseconds[0] = uTxGap;


   static u32 sl_TimeLastPacketsSumTxTimeComputation = 0;

   if ( g_TimeNow >= sl_TimeLastPacketsSumTxTimeComputation + 200 )
   {
      u32 uDeltaTime = g_TimeNow - sl_TimeLastPacketsSumTxTimeComputation;
      sl_TimeLastPacketsSumTxTimeComputation = g_TimeNow;
      u32 uTotalTxTimePerSec = 0;
      u32 uTotalVideoTxTimePerSec = 0;
      
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         g_InterfacesTxMiliSecTimePerSecond[i] = s_LastPacketsSumTxTime[i] / uDeltaTime;
         s_LastPacketsSumTxTime[i] = 0;
         g_InterfacesVideoTxMiliSecTimePerSecond[i] = s_LastVideoPacketsSumTxTime[i] / uDeltaTime;
         s_LastVideoPacketsSumTxTime[i] = 0;
         uTotalTxTimePerSec += g_InterfacesTxMiliSecTimePerSecond[i];
         uTotalVideoTxTimePerSec += g_InterfacesVideoTxMiliSecTimePerSecond[i];
      }

      if ( uTotalTxTimePerSec > g_uTotalRadioTxTimePerSec )
         g_uTotalRadioTxTimePerSec = uTotalTxTimePerSec;
      else
         g_uTotalRadioTxTimePerSec = g_uTotalRadioTxTimePerSec/3 + uTotalTxTimePerSec*2/3;

      if ( uTotalVideoTxTimePerSec > g_uTotalVideoRadioTxTimePerSec )
         g_uTotalVideoRadioTxTimePerSec = uTotalVideoTxTimePerSec;
      else
         g_uTotalVideoRadioTxTimePerSec = g_uTotalVideoRadioTxTimePerSec/3 + uTotalVideoTxTimePerSec*2/3;
   }

   if ( bHasVideoPacket )
   {
      s_countTXVideoPacketsOutTemp++;
   }
   else
   {
      s_countTXDataPacketsOutTemp += iCountChainedPackets[STREAM_ID_DATA];
      s_countTXCompactedPacketsOutTemp++;
   }

   s_bSentAnyPacket = true;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
   //log_line("Sent packet %d, on link %d", s_StreamsTxPacketIndex[uStreamId], uStreamId);
   

   #ifdef LOG_RAW_TELEMETRY
   t_packet_header* pPH = (t_packet_header*) pPacketData;
   if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD )
   {
      t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)(pPacketData + sizeof(t_packet_header));
      log_line("[Raw_Telem] Send raw telemetry packet to radio interfaces, index %u, %d bytes", pPHTR->telem_segment_index, pPH->total_length);
   }
   #endif

   return 0;
}

void send_packet_vehicle_log(u8* pBuffer, int length)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_LOG_FILE_SEGMENT, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = PH.vehicle_id_src;
   PH.total_length = sizeof(t_packet_header) + sizeof(t_packet_header_file_segment) + (u16)length;

   t_packet_header_file_segment PHFS;
   PHFS.file_id = FILE_ID_VEHICLE_LOG;
   PHFS.segment_index = s_VehicleLogSegmentIndex++;
   PHFS.segment_size = (u32) length;
   PHFS.total_segments = MAX_U32;

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet + sizeof(t_packet_header), (u8*)&PHFS, sizeof(t_packet_header_file_segment));
   memcpy(packet + sizeof(t_packet_header) + sizeof(t_packet_header_file_segment), pBuffer, length);

   packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
}

void _send_alarm_packet_to_router(u32 uAlarmIndex, u32 uAlarm, u32 uFlags1, u32 uFlags2, u32 uRepeatCount)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_ALARM, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + 4*sizeof(u32);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &uAlarmIndex, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &uAlarm, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+2*sizeof(u32), &uFlags1, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+3*sizeof(u32), &uFlags2, sizeof(u32));
   packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);

   char szBuff[128];
   alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);
   log_line("Sent alarm packet to router: %s, alarm index: %u, repeat count: %u", szBuff, uAlarmIndex, uRepeatCount);

   s_uTimeLastAlarmSentToRouter = g_TimeNow;
}

void send_alarm_to_controller(u32 uAlarm, u32 uFlags1, u32 uFlags2, u32 uRepeatCount)
{
   char szBuff[128];
   alarms_to_string(uAlarm, uFlags1, uFlags2, szBuff);

   s_uAlarmsIndex++;
   log_line("Sending alarm to controller: %s, alarm index: %u, repeat count: %u", szBuff, s_uAlarmsIndex, uRepeatCount);

   _send_alarm_packet_to_router(s_uAlarmsIndex, uAlarm, uFlags1, uFlags2, uRepeatCount);

   if ( uRepeatCount <= 1 )
      return;

   if ( s_AlarmsPendingInQueue >= MAX_ALARMS_QUEUE )
   {
      log_softerror_and_alarm("Too many alarms in the queue (%d alarms). Can't queue any more alarms.", s_AlarmsPendingInQueue);
      return;
   }
   uRepeatCount--;
   log_line("Queued alarm %s to queue position %d to be sent %d more times", szBuff, s_AlarmsPendingInQueue, uRepeatCount );

   s_AlarmsQueue[s_AlarmsPendingInQueue].uIndex = s_uAlarmsIndex;
   s_AlarmsQueue[s_AlarmsPendingInQueue].uId = uAlarm;
   s_AlarmsQueue[s_AlarmsPendingInQueue].uFlags1 = uFlags1;
   s_AlarmsQueue[s_AlarmsPendingInQueue].uFlags2 = uFlags2;
   s_AlarmsQueue[s_AlarmsPendingInQueue].uRepeatCount = uRepeatCount;
   s_AlarmsQueue[s_AlarmsPendingInQueue].uStartTime = g_TimeNow;
   s_AlarmsPendingInQueue++;
}

void send_pending_alarms_to_controller()
{
   if ( s_AlarmsPendingInQueue == 0 )
      return;

   if ( g_TimeNow < s_uTimeLastAlarmSentToRouter + 150 )
      return;

   if ( s_AlarmsQueue[0].uRepeatCount == 0 )
   {
      for( int i=0; i<s_AlarmsPendingInQueue-1; i++ )
         memcpy(&(s_AlarmsQueue[i]), &(s_AlarmsQueue[i+1]), sizeof(t_alarm_info));
      s_AlarmsPendingInQueue--;
      if ( s_AlarmsPendingInQueue == 0 )
         return;
   }

   _send_alarm_packet_to_router(s_AlarmsQueue[0].uIndex, s_AlarmsQueue[0].uId, s_AlarmsQueue[0].uFlags1, s_AlarmsQueue[0].uFlags2, s_AlarmsQueue[0].uRepeatCount );
   s_AlarmsQueue[0].uRepeatCount--;
}