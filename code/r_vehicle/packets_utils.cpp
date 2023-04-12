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
#include "../base/base.h"
#include "../base/flags.h"
#include "../base/encr.h"
#include "../base/commands.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "shared_vars.h"
#include "timers.h"
#include "processor_tx_video.h"
#include "video_link_stats_overwrites.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"

extern t_packet_queue s_QueueRadioPacketsOut;

static u8 s_RadioRawPacket[MAX_PACKET_TOTAL_SIZE];

static u32 s_StreamsCurrentPacketIndex[MAX_RADIO_STREAMS];
static u16 s_LastPacketTxTime[MAX_RADIO_INTERFACES];
static u16 s_LastPacketsSumTxTime[MAX_RADIO_INTERFACES];
static u32 s_TimeLastPacketsSumTxTimeComputation = 0;

static int s_LastTxDataRates[MAX_RADIO_INTERFACES][2];
static u32 s_uLastTimeTxDataRateDivergence = 0;

static bool s_bSentAnyPacket = false;

static u32 s_VehicleLogSegmentIndex = 0;


u32 s_uLastAlarms = 0;
u32 s_uLastAlarmsFlags = 0;
u32 s_uLastAlarmsRepeatCount = 0;
u32 s_uLastAlarmsTime = 0;
u32 s_uLastAlarmsCount = 0;

u32 get_stream_next_packet_index(int iStreamId)
{
   u32 uVal = s_StreamsCurrentPacketIndex[iStreamId];
   s_StreamsCurrentPacketIndex[iStreamId]++;
   return uVal;
}

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
      return DEFAULT_RADIO_DATARATE;
   return nMinRate/1000/1000;
}

int _compute_packet_datarate(bool bIsVideoPacket, bool bIsRetransmited, int iRadioLink, int iRadioInterface)
{
   int nRateTx = DEFAULT_RADIO_DATARATE;

   if ( ! bIsVideoPacket )
   {
      nRateTx = g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][1];
      int nRateTxVideo = video_stats_overwrites_get_current_radio_datarate_video(iRadioLink, iRadioInterface);
      if ( getRealDataRateFromRadioDataRate(nRateTxVideo) < getRealDataRateFromRadioDataRate(nRateTx) )
         nRateTx = nRateTxVideo;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA )
      {
         if ( nRateTx > 0 )
            nRateTx = getDataRates()[0];
         else
            nRateTx = -1;
      }
      if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_datarates[iRadioInterface][1] )
         nRateTx = g_pCurrentModel->radioInterfacesParams.interface_datarates[iRadioInterface][1];
      if ( 0 == nRateTx )
         nRateTx = DEFAULT_RADIO_DATARATE;

      s_LastTxDataRates[iRadioInterface][1] = nRateTx;

      return nRateTx;
   }

   nRateTx = video_stats_overwrites_get_current_radio_datarate_video(iRadioLink, iRadioInterface);

   if ( bIsRetransmited )
      nRateTx = video_stats_overwrites_get_next_level_down_radio_datarate_video(iRadioLink, iRadioInterface);
   
   if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_datarates[iRadioInterface][0] )
      nRateTx = g_pCurrentModel->radioInterfacesParams.interface_datarates[iRadioInterface][0];

   if ( 0 == nRateTx )
      nRateTx = DEFAULT_RADIO_DATARATE;

   if ( bIsRetransmited )
      return nRateTx;


   if ( 0 == s_LastTxDataRates[iRadioInterface][0] )
   {
      s_LastTxDataRates[iRadioInterface][0] = nRateTx;
      s_uLastTimeTxDataRateDivergence = 0;
   }
   
   if ( nRateTx == s_LastTxDataRates[iRadioInterface][0] )
      return nRateTx;

   if ( nRateTx > s_LastTxDataRates[iRadioInterface][0] )
   {
      s_LastTxDataRates[iRadioInterface][0] = nRateTx;
      s_uLastTimeTxDataRateDivergence = 0;
      return nRateTx;
   }

  
   if ( 0 == s_uLastTimeTxDataRateDivergence )
   {
      s_uLastTimeTxDataRateDivergence = g_TimeNow;
      nRateTx = s_LastTxDataRates[iRadioInterface][0];
   }
   else
   {
      if ( g_TimeNow < s_uLastTimeTxDataRateDivergence + 200 )
         nRateTx = s_LastTxDataRates[iRadioInterface][0];
      else
      {
         s_uLastTimeTxDataRateDivergence = 0;
         s_LastTxDataRates[iRadioInterface][0] = nRateTx;
      }           
   }

   s_LastTxDataRates[iRadioInterface][0] = nRateTx;

   return nRateTx;
}

int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength)
{
   if ( ! s_bSentAnyPacket )
   {
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         s_StreamsCurrentPacketIndex[i] = 0;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         s_LastPacketTxTime[i] = 0;
         s_LastPacketsSumTxTime[i] = 0;
         s_LastTxDataRates[i][0] = 0;
         s_LastTxDataRates[i][1] = 0;
      }
   }

   // Set packets indexes and tx times if multiple packets are found in the input buffer

   bool bHasVideoPacket = false;
   bool bIsRetransmited = false;
   bool bHasPingPacket = false;
   int iHasCommandPacket = 0;
   u8 uPingRadioLinkId = 0xFF;
   u8 uFirstPacketType = 0;

   int iCountChainedPackets[MAX_RADIO_STREAMS];
   int iTotalBytesOnEachStream[MAX_RADIO_STREAMS];
   memset(iCountChainedPackets, 0, MAX_RADIO_STREAMS*sizeof(int));
   memset(iTotalBytesOnEachStream, 0, MAX_RADIO_STREAMS*sizeof(int));

   u8* pData = pPacketData;
   int nLength = nPacketLength;
   
   while ( nLength > 0 )
   {
      t_packet_header* pPH = (t_packet_header*)pData;

      if ( 0 == uFirstPacketType )
         uFirstPacketType = pPH->packet_type;
      //uPacketTypes[uCountChainedPackets] = pPH->packet_type;
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
      {
         iHasCommandPacket++;
         //t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pData + sizeof(t_packet_header));
      }

      if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
         bIsRetransmited = true;

      if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
      {
         bHasPingPacket = true;
         memcpy((u8*)&uPingRadioLinkId, pData + sizeof(t_packet_header) + sizeof(u8) + sizeof(u32), sizeof(u8));
      }

      u32 uStreamId = (pPH->stream_packet_idx) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      pPH->stream_packet_idx = (((u32)uStreamId)<<PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX) | (s_StreamsCurrentPacketIndex[uStreamId] & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
      s_StreamsCurrentPacketIndex[uStreamId]++;
      iCountChainedPackets[uStreamId]++;
      iTotalBytesOnEachStream[uStreamId] += pPH->total_length;
      
      if ( uStreamId >= STREAM_ID_VIDEO_1 )
         bHasVideoPacket = true;

      nLength -= pPH->total_length;
      pData += pPH->total_length;
   }

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

   // Send packet on all radio links that can send this packet
   // Exception: Ping reply packet is sent only on the associated radio link for this ping

   int nRateTx = 0;

   bool bPacketSent = false;

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( bHasPingPacket && (uPingRadioLinkId != 0xFF) )
      if ( iRadioLinkId != (int) uPingRadioLinkId )
         continue;

      // Do not send regular packets to controller using relay links
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;

      if ( !(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( bHasVideoPacket )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
         continue;

      if ( ! bHasVideoPacket )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
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
      if ( bHasVideoPacket && (!(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO)) )
         continue;
      if ( (!bHasVideoPacket) && (!(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
         continue;
      
      u32 microT = get_current_timestamp_micros();

      // Send radio packet over SiK radios as individual packets

      if ( hardware_radio_index_is_sik_radio(iRadioInterfaceIndex) )
      {
         pData = pPacketData;
         nLength = nPacketLength;
         s_LastTxDataRates[iRadioInterfaceIndex][1] = g_pCurrentModel->radioInterfacesParams.interface_datarates[iRadioInterfaceIndex][1];
         while ( nLength > 0 )
         {
            t_packet_header* pPH = (t_packet_header*)pData;
            if ( ! radio_can_send_packet_on_slow_link(iRadioLinkId, pPH->packet_type, 0, g_TimeNow) )
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
               s_LastPacketTxTime[iRadioInterfaceIndex] = get_current_timestamp_micros() - microT;
               s_LastPacketsSumTxTime[iRadioInterfaceIndex] += s_LastPacketTxTime[iRadioInterfaceIndex];
               
               radio_stats_update_on_packet_sent_on_radio_interface(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, iOutLen);
               radio_stats_update_on_packet_sent_on_radio_link(&g_SM_RadioStats, g_TimeNow, iRadioLinkId, (int)uStreamId, iOutLen, 1);
               bPacketSent = true;
            }   
            else
               log_softerror_and_alarm("Failed to write to SiK radio interface %d.", iRadioInterfaceIndex+1);
            nLength -= pPH->total_length;
            pData += pPH->total_length;
         }
         continue;
      }
      
      nRateTx = _compute_packet_datarate(bHasVideoPacket, bIsRetransmited, iRadioLinkId, iRadioInterfaceIndex);
      radio_set_out_datarate(nRateTx);

      u32 radioFlags = g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[iRadioInterfaceIndex];
      if ( ! (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE) )
      {
         radioFlags &= ~(RADIO_FLAGS_STBC | RADIO_FLAGS_LDPC | RADIO_FLAGS_SHORT_GI | RADIO_FLAGS_HT40);
         radioFlags |= RADIO_FLAGS_HT20;
      }
      radio_set_frames_flags(radioFlags);

      int totalLength = 0;
      if ( s_iPendingFrequencyChangeLinkId >= 0 && s_uPendingFrequencyChangeTo > 100 && s_uTimeFrequencyChangeRequest != 0 && g_TimeNow > s_uTimeFrequencyChangeRequest && s_uTimeFrequencyChangeRequest >= g_TimeNow - VEHICLE_SWITCH_FREQUENCY_AFTER_MS )
      {
         u8 extraData[32];
         memcpy(&extraData[0], (u8*)&s_uPendingFrequencyChangeTo, sizeof(u32));
         extraData[4] = EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK1;
         if ( s_iPendingFrequencyChangeLinkId == 1 )
            extraData[4] = EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK2;
         if ( s_iPendingFrequencyChangeLinkId == 2 )
            extraData[4] = EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK3;
         extraData[5] = 6;
         //log_line("Sending extra data: %d %d, %d, %d, %d, %d", extraData[5], extraData[4], extraData[3], extraData[2], extraData[1], extraData[0]);
         totalLength = radio_build_packet(s_RadioRawPacket, pPacketData, nPacketLength, RADIO_PORT_ROUTER_DOWNLINK, be, 6, &extraData[0]);
      }
      else
         totalLength = radio_build_packet(s_RadioRawPacket, pPacketData, nPacketLength, RADIO_PORT_ROUTER_DOWNLINK, be, 0, NULL);

      if ( radio_write_packet(iRadioInterfaceIndex, s_RadioRawPacket, totalLength) )
      {       
         s_LastPacketTxTime[iRadioInterfaceIndex] = get_current_timestamp_micros() - microT;
         s_LastPacketsSumTxTime[iRadioInterfaceIndex] += s_LastPacketTxTime[iRadioInterfaceIndex];
         
         radio_stats_update_on_packet_sent_on_radio_interface(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, nPacketLength);

         for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         {
            if ( 0 == iCountChainedPackets[i] )
               continue;
            radio_stats_update_on_packet_sent_on_radio_link(&g_SM_RadioStats, g_TimeNow, iRadioLinkId, i, iTotalBytesOnEachStream[i], iCountChainedPackets[i]);
         }
         bPacketSent = true;
      }
      else
         log_softerror_and_alarm("Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   }

   if ( ! bPacketSent )
   {
      log_softerror_and_alarm("Packet not sent! No radio interface could send it (%s, type: %d, %d bytes, %d chained packets). %d radio links.",
         bHasVideoPacket?"video packet":"data packet", (int)uFirstPacketType, nPacketLength, (int) iCountChainedPackets[STREAM_ID_DATA], g_pCurrentModel->radioLinksParams.links_count);
      if ( bHasPingPacket )
         log_softerror_and_alarm("Packet contained a ping for radio link %d", ((int)uPingRadioLinkId)+1);
      else
         log_softerror_and_alarm("Packet did not contained a ping packet.");
      return 0;
   }

   // Packet sent. Update stats and info

   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      if ( 0 == iCountChainedPackets[i] )
         continue;
      radio_stats_update_on_packet_sent_for_radio_stream(&g_SM_RadioStats, g_TimeNow, i, iTotalBytesOnEachStream[i]);
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


   if ( bHasVideoPacket && (g_TimeNow >= s_TimeLastPacketsSumTxTimeComputation + 200) )
   {
      s_TimeLastPacketsSumTxTimeComputation = g_TimeNow;
      u32 uTotalTxTime = 0;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         g_TXMiliSecTimePerSecond[i] = s_LastPacketsSumTxTime[i]*5/1000;
         uTotalTxTime += g_TXMiliSecTimePerSecond[i];
         s_LastPacketsSumTxTime[i] = 0;
      }
      if ( 0 == g_uTotalRadioTxTimePerSec )
         g_uTotalRadioTxTimePerSec = uTotalTxTime;
      else
         g_uTotalRadioTxTimePerSec = g_uTotalRadioTxTimePerSec/3 + uTotalTxTime*2/3;      
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
   //log_line("Sent packet %d, on link %d", s_StreamsCurrentPacketIndex[uStreamId], uStreamId);
   
   return 0;
}

void send_packet_vehicle_log(u8* pBuffer, int length)
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_RUBY;
   PH.packet_type =  PACKET_TYPE_RUBY_LOG_FILE_SEGMENT;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = PH.vehicle_id_src;
   PH.total_headers_length = sizeof(t_packet_header) + sizeof(t_packet_header_file_segment);
   PH.total_length = PH.total_headers_length + (u16)length;

   t_packet_header_file_segment PHFS;
   PHFS.file_id = FILE_ID_VEHICLE_LOG;
   PHFS.segment_index = s_VehicleLogSegmentIndex++;
   PHFS.segment_size = (u32) length;
   PHFS.total_segments = MAX_U32;

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), (u8*)&PHFS, sizeof(t_packet_header_file_segment));
   memcpy(packet+PH.total_headers_length, pBuffer, length);

   packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);
}


void send_alarm_to_controller(u32 uAlarms, u32 uFlags, u32 uRepeatCount)
{
   if ( (uAlarms == s_uLastAlarms) && (g_TimeNow < s_uLastAlarmsTime + 500) )
      return;
   s_uLastAlarms = uAlarms;
   s_uLastAlarmsFlags = uFlags;
   s_uLastAlarmsRepeatCount = uRepeatCount;
   s_uLastAlarmsTime = g_TimeNow;
   s_uLastAlarmsCount++;
}

