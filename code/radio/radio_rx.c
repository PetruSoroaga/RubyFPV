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
#include <pthread.h>
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "radio_rx.h"
#include "radiolink.h"
#include "radio_duplicate_det.h"

int s_iRadioRxInitialized = 0;
int s_iRadioRxSingalStop = 0;
int s_iRadioRxLoopTimeoutInterval = 15;
int s_iRadioRxMarkedForQuit = 0;

t_radio_rx_buffers s_RadioRxBuffers;

pthread_t s_pThreadRadioRx;
pthread_mutex_t s_pThreadRadioRxMutex;
shared_mem_radio_stats* s_pSMRadioStats = NULL;
shared_mem_radio_stats_interfaces_rx_graph* s_pSMRadioRxGraphs = NULL;
int s_iSearchMode = 0;
u32 s_uRadioRxTimeNow = 0;
u32 s_uRadioRxLastTimeRead = 0;
u32 s_uRadioRxLastTimeQueue = 0;

int s_iRadioRxPausedInterfaces[MAX_RADIO_INTERFACES];
int s_iRadioRxAllInterfacesPaused = 0;

fd_set s_RadioRxReadSet;

u8 s_tmpLastProcessedRadioRxPacket[MAX_PACKET_TOTAL_SIZE];

u32 s_uLastRxShortPacketsVehicleIds[MAX_RADIO_INTERFACES];

extern pthread_mutex_t s_pMutexRadioSyncRxTxThreads;
extern int s_iMutexRadioSyncRxTxThreadsInitialized;

void _radio_rx_update_local_stats_on_new_radio_packet(int iInterface, int iIsShortPacket, u32 uVehicleId, u8* pPacket, int iLength, int iDataIsOk)
{
   if ( (NULL == pPacket) || ( iLength <= 2 ) )
      return;

   //----------------------------------------------
   // Begin: Compute index of stats to use

   int iStatsIndex = -1;
   if ( (s_RadioRxBuffers.uVehiclesIds[0] == 0) || (s_RadioRxBuffers.uVehiclesIds[0] == uVehicleId)  )
      iStatsIndex = 0;

   if ( -1 == iStatsIndex )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( s_RadioRxBuffers.uVehiclesIds[i] == uVehicleId )
         {
            iStatsIndex = i;
            break;
         }
      }

      if ( iStatsIndex == -1 )
      {
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            if ( s_RadioRxBuffers.uVehiclesIds[i] == 0 )
            {
               iStatsIndex = i;
               break;
            }
         }
      }
   }

   if ( iStatsIndex == -1 )
      iStatsIndex = MAX_CONCURENT_VEHICLES-1;

   // End: Compute index of stats to use
   //----------------------------------------------

   s_RadioRxBuffers.uVehiclesIds[iStatsIndex] = uVehicleId;
   if ( 0 == s_RadioRxBuffers.uTotalRxPackets[iStatsIndex] )
   {
      s_RadioRxBuffers.uTimeLastStatsUpdate = s_uRadioRxTimeNow;
      s_RadioRxBuffers.uTimeLastMinute = s_uRadioRxTimeNow;
   }

   if ( iDataIsOk )
   {
      s_RadioRxBuffers.uTotalRxPackets[iStatsIndex]++;
      s_RadioRxBuffers.uTmpRxPackets[iStatsIndex]++;
   }
   else
   {
      s_RadioRxBuffers.uTotalRxPacketsBad[iStatsIndex]++;
      s_RadioRxBuffers.uTmpRxPacketsBad[iStatsIndex]++;
   }

   if ( iIsShortPacket )
   {
      t_packet_header_short* pPHS = (t_packet_header_short*)pPacket;
      u32 uNext = ((s_RadioRxBuffers.uLastRxRadioLinkPacketIndex[iStatsIndex][iInterface]+1) & 0xFF);
      if ( pPHS->packet_id != uNext )
      {
         u32 lost = pPHS->packet_id - uNext;
         if ( pPHS->packet_id < uNext )
            lost = pPHS->packet_id + 255 - uNext;
         s_RadioRxBuffers.uTotalRxPacketsLost[iStatsIndex] += lost;
         s_RadioRxBuffers.uTmpRxPacketsLost[iStatsIndex] += lost;
      }

      s_RadioRxBuffers.uLastRxRadioLinkPacketIndex[iStatsIndex][iInterface] = pPHS->packet_id;
   }
   else
   {
      t_packet_header* pPH = (t_packet_header*)pPacket;
      if ( s_RadioRxBuffers.uLastRxRadioLinkPacketIndex[iStatsIndex][iInterface] > 0 )
      if ( pPH->radio_link_packet_index > s_RadioRxBuffers.uLastRxRadioLinkPacketIndex[iStatsIndex][iInterface]+1 )
      {
         u32 lost = pPH->radio_link_packet_index - (s_RadioRxBuffers.uLastRxRadioLinkPacketIndex[iStatsIndex][iInterface] + 1);
         s_RadioRxBuffers.uTotalRxPacketsLost[iStatsIndex] += lost;
         s_RadioRxBuffers.uTmpRxPacketsLost[iStatsIndex] += lost;
      }

      if ( s_RadioRxBuffers.uLastRxRadioLinkPacketIndex[iStatsIndex][iInterface] > 0 )
      if ( s_RadioRxBuffers.uLastRxRadioLinkPacketIndex[iStatsIndex][iInterface] < 0xFA00 )
      if ( pPH->radio_link_packet_index > 0x0500 )
      if ( pPH->radio_link_packet_index < s_RadioRxBuffers.uLastRxRadioLinkPacketIndex[iStatsIndex][iInterface] )
          radio_dup_detection_set_vehicle_restarted_flag(pPH->vehicle_id_src);

      s_RadioRxBuffers.uLastRxRadioLinkPacketIndex[iStatsIndex][iInterface] = pPH->radio_link_packet_index;
   }
}

void _radio_rx_add_packet_to_rx_queue(u8* pPacket, int iLength, int iRadioInterface)
{
   if ( (NULL == pPacket) || (iLength <= 0) || s_iRadioRxMarkedForQuit )
      return;

   s_RadioRxBuffers.iPacketsRxInterface[s_RadioRxBuffers.iCurrentRxPacketIndex] = iRadioInterface;
   s_RadioRxBuffers.iPacketsAreShort[s_RadioRxBuffers.iCurrentRxPacketIndex] = 0;
   s_RadioRxBuffers.iPacketsLengths[s_RadioRxBuffers.iCurrentRxPacketIndex] = iLength;
   memcpy(s_RadioRxBuffers.pPacketsBuffers[s_RadioRxBuffers.iCurrentRxPacketIndex], pPacket, iLength);
   
   int iPacketsInQueue = 0;
   if ( s_RadioRxBuffers.iCurrentRxPacketIndex > s_RadioRxBuffers.iCurrentRxPacketToConsume )
      iPacketsInQueue = s_RadioRxBuffers.iCurrentRxPacketIndex - s_RadioRxBuffers.iCurrentRxPacketToConsume;
   else if ( s_RadioRxBuffers.iCurrentRxPacketIndex < s_RadioRxBuffers.iCurrentRxPacketToConsume )
      iPacketsInQueue = MAX_RX_PACKETS_QUEUE - s_RadioRxBuffers.iCurrentRxPacketToConsume + s_RadioRxBuffers.iCurrentRxPacketIndex;
   if ( iPacketsInQueue > s_RadioRxBuffers.iMaxPacketsInQueueLastMinute )
      s_RadioRxBuffers.iMaxPacketsInQueueLastMinute = iPacketsInQueue;
   if ( iPacketsInQueue > s_RadioRxBuffers.iMaxPacketsInQueue )
      s_RadioRxBuffers.iMaxPacketsInQueue = iPacketsInQueue;

   u32 uTimeStartTryLock = get_current_timestamp_ms();
   int iLock = -1;
   while ( (iLock != 0 ) && (get_current_timestamp_ms() <= uTimeStartTryLock+5) )
   {
      iLock = pthread_mutex_trylock(&s_pThreadRadioRxMutex);
      hardware_sleep_micros(100);
   }

   if ( iLock != 0 )
      log_softerror_and_alarm("[RadioRxThread] Doing unguarded rx buffers manipulation");
   
   if ( s_RadioRxBuffers.iCurrentRxPacketToConsume == -1 )
      s_RadioRxBuffers.iCurrentRxPacketToConsume = s_RadioRxBuffers.iCurrentRxPacketIndex;

   s_RadioRxBuffers.iCurrentRxPacketIndex++;
   if ( s_RadioRxBuffers.iCurrentRxPacketIndex >= MAX_RX_PACKETS_QUEUE )
      s_RadioRxBuffers.iCurrentRxPacketIndex = 0;

   if ( s_RadioRxBuffers.iCurrentRxPacketIndex == s_RadioRxBuffers.iCurrentRxPacketToConsume )
   {
      // No more room. Must skip few pending processing packets
      log_softerror_and_alarm("[RadioRxThread] No more room in rx buffers. Discarding some unprocessed messages. Max messages in queue: %d, last 10 sec: %d.", s_RadioRxBuffers.iMaxPacketsInQueue, s_RadioRxBuffers.iMaxPacketsInQueueLastMinute);
      if ( iLock != 0 )
      {
         iLock = pthread_mutex_lock(&s_pThreadRadioRxMutex);
      }
      for( int i=0; i<3; i++ )
      {
         s_RadioRxBuffers.iCurrentRxPacketToConsume++;
         if ( s_RadioRxBuffers.iCurrentRxPacketToConsume >= MAX_RX_PACKETS_QUEUE )
            s_RadioRxBuffers.iCurrentRxPacketToConsume = 0;
      }
   }
   if ( 0 == iLock )
      pthread_mutex_unlock(&s_pThreadRadioRxMutex);

   s_uRadioRxLastTimeQueue += get_current_timestamp_ms() - s_uRadioRxTimeNow;
}

void _radio_rx_check_add_packet_to_rx_queue(u8* pPacket, int iLength, int iRadioInterfaceIndex)
{   
   if ( radio_dup_detection_is_duplicate(iRadioInterfaceIndex, pPacket, iLength, s_uRadioRxTimeNow) )
      return;

   if ( NULL != s_pSMRadioStats )
     radio_stats_update_on_unique_packet_received(s_pSMRadioStats, s_pSMRadioRxGraphs, s_uRadioRxTimeNow, iRadioInterfaceIndex, pPacket, iLength);

   _radio_rx_add_packet_to_rx_queue(pPacket, iLength, iRadioInterfaceIndex);
}


int _radio_rx_process_sik_short_packet(int iInterfaceIndex, u8* pPacketBuffer, int iPacketLength)
{
   static u8 s_uLastRxShortPacketsIds[MAX_RADIO_INTERFACES];
   static u8 s_uBuffersFullMessages[MAX_RADIO_INTERFACES][MAX_PACKET_TOTAL_SIZE*2];
   static int s_uBuffersFullMessagesReadPos[MAX_RADIO_INTERFACES];
   static int s_bInitializedBuffersFullMessages = 0;

   if ( ! s_bInitializedBuffersFullMessages )
   {
      s_bInitializedBuffersFullMessages = 1;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         s_uBuffersFullMessagesReadPos[i] = 0;
         s_uLastRxShortPacketsIds[i] = 0xFF;
         s_uLastRxShortPacketsVehicleIds[i] = 0;
      }
   }

   if ( (NULL == pPacketBuffer) || (iPacketLength < sizeof(t_packet_header_short)) )
      return -1;
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex > hardware_get_radio_interfaces_count()) )
      return -1;

   t_packet_header_short* pPHS = (t_packet_header_short*)pPacketBuffer;

   // If it's the start of a full packet, reset rx buffer for this interface
   if ( pPHS->start_header == SHORT_PACKET_START_BYTE_START_PACKET )
   {
     s_uBuffersFullMessagesReadPos[iInterfaceIndex] = 0;
     if ( pPHS->data_length >= sizeof(t_packet_header) - sizeof(u32) )
     {
        t_packet_header* pPH = (t_packet_header*)(pPacketBuffer + sizeof(t_packet_header_short));
        s_uLastRxShortPacketsVehicleIds[iInterfaceIndex] = pPH->vehicle_id_src;
     }
   }

   // Update radio interfaces rx stats

   _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 1, s_uLastRxShortPacketsVehicleIds[iInterfaceIndex], pPacketBuffer, iPacketLength, 1);
   if ( NULL != s_pSMRadioStats )
      radio_stats_update_on_new_radio_packet_received(s_pSMRadioStats, s_pSMRadioRxGraphs, s_uRadioRxTimeNow, iInterfaceIndex, pPacketBuffer, iPacketLength, 1, 0, 1);
   
   // If there are missing packets, reset rx buffer for this interface
   
   u32 uNext = (((u32)(s_uLastRxShortPacketsIds[iInterfaceIndex])) + 1) & 0xFF;
   if ( uNext != pPHS->packet_id )
   {
      s_uBuffersFullMessagesReadPos[iInterfaceIndex] = 0;
   }
   s_uLastRxShortPacketsIds[iInterfaceIndex] = pPHS->packet_id;
   // Add the content of the packet to the buffer

   memcpy(&s_uBuffersFullMessages[iInterfaceIndex][s_uBuffersFullMessagesReadPos[iInterfaceIndex]], pPacketBuffer + sizeof(t_packet_header_short), pPHS->data_length);
   s_uBuffersFullMessagesReadPos[iInterfaceIndex] += pPHS->data_length;

   // Do we have a full valid radio packet?

   if ( s_uBuffersFullMessagesReadPos[iInterfaceIndex] >= sizeof(t_packet_header) )
   {
      t_packet_header* pPH = (t_packet_header*) s_uBuffersFullMessages[iInterfaceIndex];
      if ( (pPH->total_length >= sizeof(t_packet_header)) && (s_uBuffersFullMessagesReadPos[iInterfaceIndex] >= pPH->total_length) )
      {
         u32 uCRC = base_compute_crc32(&s_uBuffersFullMessages[iInterfaceIndex][sizeof(u32)], pPH->total_length - sizeof(u32));
         if ( uCRC == pPH->crc )
         {
            s_uBuffersFullMessagesReadPos[iInterfaceIndex] = 0;
            _radio_rx_check_add_packet_to_rx_queue(s_uBuffersFullMessages[iInterfaceIndex], pPH->total_length, iInterfaceIndex);
         }
      }
   }

   // Too much data? Then reset the buffer and wait for the start of a new full packet.
   if ( s_uBuffersFullMessagesReadPos[iInterfaceIndex] >= MAX_PACKET_TOTAL_SIZE*2 - 255 )
     s_uBuffersFullMessagesReadPos[iInterfaceIndex] = 0;
   return 1;
}

// return 0 on success, -1 if the interface is now invalid or broken

int _radio_rx_parse_received_sik_radio_data(int iInterfaceIndex)
{
   static u8 s_uBuffersSiKMessages[MAX_RADIO_INTERFACES][512];
   static int s_uBuffersSiKMessagesReadPos[MAX_RADIO_INTERFACES];
   static int s_bInitializedBuffersSiKMessages = 0;

   if ( ! s_bInitializedBuffersSiKMessages )
   {
      s_bInitializedBuffersSiKMessages = 1;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         s_uBuffersSiKMessagesReadPos[i] = 0;
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
   {
      log_softerror_and_alarm("[RadioRxThread] Tried to process a received short radio packet on radio interface (%d) that is not opened for read.", iInterfaceIndex+1);
      return -1;
   }
   if ( ! hardware_radio_index_is_sik_radio(iInterfaceIndex) )
   {
      log_softerror_and_alarm("[RadioRxThread] Tried to process a received short radio packet on radio interface (%d) that is not a SiK radio.", iInterfaceIndex+1);
      return 0;
   }

   int iMaxRead = 512 - s_uBuffersSiKMessagesReadPos[iInterfaceIndex];

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   int iRead = read(pRadioHWInfo->monitor_interface_read.selectable_fd, &(s_uBuffersSiKMessages[iInterfaceIndex][s_uBuffersSiKMessagesReadPos[iInterfaceIndex]]), iMaxRead);

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   if ( iRead < 0 )
   {
      log_softerror_and_alarm("[RadioRxThread] Failed to read received short radio packet on SiK radio interface (%d).", iInterfaceIndex+1);
      return -1;
   }

   s_uBuffersSiKMessagesReadPos[iInterfaceIndex] += iRead;
   int iBufferLength = s_uBuffersSiKMessagesReadPos[iInterfaceIndex];
   
   // Received at least the full header?
   if ( iBufferLength < (int)sizeof(t_packet_header_short) )
      return 0;

   s_uRadioRxTimeNow = get_current_timestamp_ms();

   int iPacketPos = -1;
   do
   {
      u8* pData = (u8*)&(s_uBuffersSiKMessages[iInterfaceIndex][0]);
      iPacketPos = -1;
      for( int i=0; i<iBufferLength-sizeof(t_packet_header_short); i++ )
      {
         if ( radio_buffer_is_valid_short_packet(pData+i, iBufferLength-i) )
         {
            iPacketPos = i;
            break;
         }
      }
      // No valid packet found in the buffer?
      if ( iPacketPos < 0 )
      {
         if ( iBufferLength >= 400 )
         {
            int iBytesToDiscard = iBufferLength - 256;
            // Keep only the last 255 bytes in the buffer
            for( int i=0; i<(iBufferLength-iBytesToDiscard); i++ )
               s_uBuffersSiKMessages[iInterfaceIndex][i] = s_uBuffersSiKMessages[iInterfaceIndex][i+iBytesToDiscard];
            s_uBuffersSiKMessagesReadPos[iInterfaceIndex] -= iBytesToDiscard;

            _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 1, s_uLastRxShortPacketsVehicleIds[iInterfaceIndex], s_uBuffersSiKMessages[iInterfaceIndex], iBytesToDiscard, 0);
            radio_stats_set_bad_data_on_current_rx_interval(s_pSMRadioStats, NULL, iInterfaceIndex);
         }
         return 0;
      }

      if ( iPacketPos > 0 )
      {
         _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 1, s_uLastRxShortPacketsVehicleIds[iInterfaceIndex], s_uBuffersSiKMessages[iInterfaceIndex], iPacketPos, 0);
         radio_stats_set_bad_data_on_current_rx_interval(s_pSMRadioStats, NULL, iInterfaceIndex);
      }
      t_packet_header_short* pPHS = (t_packet_header_short*)(pData+iPacketPos);
      int iShortTotalPacketSize = (int)(pPHS->data_length + sizeof(t_packet_header_short));
      
      _radio_rx_process_sik_short_packet(iInterfaceIndex, pData+iPacketPos, iShortTotalPacketSize);

      iShortTotalPacketSize += iPacketPos;
      if ( iShortTotalPacketSize > iBufferLength )
         iShortTotalPacketSize = iBufferLength;
      for( int i=0; i<iBufferLength - iShortTotalPacketSize; i++ )
         s_uBuffersSiKMessages[iInterfaceIndex][i] = s_uBuffersSiKMessages[iInterfaceIndex][i+iShortTotalPacketSize];
      s_uBuffersSiKMessagesReadPos[iInterfaceIndex] -= iShortTotalPacketSize;
      iBufferLength -= iShortTotalPacketSize;
   } while ( (iPacketPos >= 0) && (iBufferLength >= (int)sizeof(t_packet_header_short)) );
   return 0;
}

// return 0 on success, -1 if the interface is now invalid or broken

int _radio_rx_parse_received_wifi_radio_data(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;

   int iBufferLength = 0;
   u8* pBuffer = radio_process_wlan_data_in(iInterfaceIndex, &iBufferLength);

   u8* pData = pBuffer;
   int iLength = iBufferLength;

   int iReturn = 0;
   int iDataIsOk = 1;

   if ( pBuffer == NULL ) 
   {
      iDataIsOk = 0;
      iLength = 0;
      // Can be zero if the read timedout
      if ( radio_get_last_read_error_code() == RADIO_READ_ERROR_TIMEDOUT )
      {
         log_softerror_and_alarm("[RadioRxThread] Rx ppcap read timedout reading a packet on radio interface %d.", iInterfaceIndex+1);
         s_RadioRxBuffers.iRadioInterfacesRxTimeouts[iInterfaceIndex]++;
      }
      else
      {
         log_softerror_and_alarm("[RadioRxThread] Rx pcap returned a NULL packet on radio interface %d.", iInterfaceIndex+1);
         iReturn = -1;
      }
   }

   if ( iBufferLength <= 0 )
   {
      log_softerror_and_alarm("[RadioRxThread] Rx cap returned an empty buffer (%d length) on radio interface index %d.", iBufferLength, iInterfaceIndex+1);
      iDataIsOk = 0;
      iLength = 0;
      iReturn = -1;
   }

   int iCountPackets = 0;
   int iIsVideoData = 0;

   while ( (iLength > 0) && (NULL != pData) )
   { 
      t_packet_header* pPH = (t_packet_header*)pData;

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
         iIsVideoData = 1;

      iCountPackets++;
      int bCRCOk = 0;
      int iPacketLength = packet_process_and_check(iInterfaceIndex, pData, iLength, &bCRCOk);

      if ( iPacketLength <= 0 )
      {
         iDataIsOk = 0;
         s_RadioRxBuffers.iRadioInterfacesRxBadPackets[iInterfaceIndex] = get_last_processing_error_code();
         break;
      }

      if ( ! bCRCOk )
      {
         log_softerror_and_alarm("[RadioRxThread] Received broken packet (wrong CRC) on radio interface %d. Packet size: %d bytes, type: %s",
            iInterfaceIndex+1, pPH->total_length, str_get_packet_type(pPH->packet_type));
         iDataIsOk = 0;
         pData += pPH->total_length;
         iLength -= pPH->total_length; 
         continue;
      }

      if ( (iPacketLength != pPH->total_length) || (pPH->total_length >= MAX_PACKET_TOTAL_SIZE) )
      {
         log_softerror_and_alarm("[RadioRxThread] Received broken packet (computed size: %d). Packet size: %d bytes, type: %s", iPacketLength, pPH->total_length, str_get_packet_type(pPH->packet_type));
         iDataIsOk = 0;
         pData += pPH->total_length;
         iLength -= pPH->total_length; 
         continue;
      }
      _radio_rx_check_add_packet_to_rx_queue(pData, pPH->total_length, iInterfaceIndex);

      pData += pPH->total_length;
      iLength -= pPH->total_length;    
   }

   s_uRadioRxTimeNow = get_current_timestamp_ms();

   u32 uVehicleId = 0;
   if ( NULL != pBuffer )
   {
      t_packet_header* pPH = (t_packet_header*)pBuffer;
      uVehicleId = pPH->vehicle_id_src;
   }

   _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 0, uVehicleId, pBuffer, iBufferLength, iDataIsOk);
   if ( NULL != s_pSMRadioStats )
      radio_stats_update_on_new_radio_packet_received(s_pSMRadioStats, s_pSMRadioRxGraphs, s_uRadioRxTimeNow, iInterfaceIndex, pBuffer, iBufferLength, 0, iIsVideoData, iDataIsOk);

   return iReturn;
}

void _radio_rx_update_stats(u32 uTimeNow)
{
   static int s_iCounterRadioRxStatsUpdate = 0;
   static int s_iCounterRadioRxStatsUpdate2 = 0;

   if ( uTimeNow >= s_RadioRxBuffers.uTimeLastStatsUpdate + 1000 )
   {
      s_RadioRxBuffers.uTimeLastStatsUpdate = uTimeNow;
      s_iCounterRadioRxStatsUpdate++;
      int iAnyRxPackets = 0;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( s_RadioRxBuffers.uVehiclesIds[i] == 0 )
            continue;

         if ( (s_RadioRxBuffers.uTmpRxPackets[i] > 0) || (s_RadioRxBuffers.uTmpRxPacketsBad[i] > 0) )
            iAnyRxPackets = 1;
         if ( s_RadioRxBuffers.uTotalRxPackets[i] > 0 )
         {
            if ( s_RadioRxBuffers.uTmpRxPackets[i] > s_RadioRxBuffers.iMaxRxPacketsPerSec[i] )
               s_RadioRxBuffers.iMaxRxPacketsPerSec[i] = s_RadioRxBuffers.uTmpRxPackets[i];
            if ( s_RadioRxBuffers.uTmpRxPackets[i] < s_RadioRxBuffers.iMinRxPacketsPerSec[i] )
               s_RadioRxBuffers.iMinRxPacketsPerSec[i] = s_RadioRxBuffers.uTmpRxPackets[i];
         }

         if ( (s_iCounterRadioRxStatsUpdate % 10) == 0 )
         {
            log_line("[RadioRxThread] Received packets from VID %u: %u/sec (min: %d/sec, max: %d/sec)", 
               s_RadioRxBuffers.uVehiclesIds[i], s_RadioRxBuffers.uTmpRxPackets[i],
               s_RadioRxBuffers.iMinRxPacketsPerSec[i], s_RadioRxBuffers.iMaxRxPacketsPerSec[i]);
            log_line("[RadioRxThread] Total recv packets from VID: %u: %u, bad: %u, lost: %u",
               s_RadioRxBuffers.uVehiclesIds[i], s_RadioRxBuffers.uTotalRxPackets[i], s_RadioRxBuffers.uTotalRxPacketsBad[i], s_RadioRxBuffers.uTotalRxPacketsLost[i] );
         }

         s_RadioRxBuffers.uTmpRxPackets[i] = 0;
         s_RadioRxBuffers.uTmpRxPacketsBad[i] = 0;
         s_RadioRxBuffers.uTmpRxPacketsLost[i] = 0;
      }

      if ( (s_iCounterRadioRxStatsUpdate % 10) == 0 )
      if ( 0 == iAnyRxPackets )
         log_line("[RadioRxThread] No packets received in the last seconds");
   }
   if ( uTimeNow >= s_RadioRxBuffers.uTimeLastMinute + 1000 * 5 )
   {
      s_RadioRxBuffers.uTimeLastMinute = uTimeNow;
      s_iCounterRadioRxStatsUpdate2++;
      log_line("[RadioRxThread] Max packets in queue: %d. Max packets in queue in last 10 sec: %d.",
         s_RadioRxBuffers.iMaxPacketsInQueue, s_RadioRxBuffers.iMaxPacketsInQueueLastMinute);
      s_RadioRxBuffers.iMaxPacketsInQueueLastMinute = 0;

      radio_duplicate_detection_log_info();

      if ( (s_iCounterRadioRxStatsUpdate2 % 10) == 0 )
      {
         log_line("[RadioRxThread] Reset max stats.");
         s_RadioRxBuffers.iMaxPacketsInQueue = 0;
      }
   }
}

static void * _thread_radio_rx(void *argument)
{
   log_line("[RadioRxThread] Started.");

   pthread_t thId = pthread_self();
   pthread_attr_t thAttr;
   int policy = 0;
   int max_prio_for_policy = 0;

   pthread_attr_init(&thAttr);
   pthread_attr_getschedpolicy(&thAttr, &policy);
   max_prio_for_policy = sched_get_priority_max(policy);

   pthread_setschedprio(thId, max_prio_for_policy);
   pthread_attr_destroy(&thAttr);

   log_line("[RadioRxThread] Initialized State. Waiting for rx messages...");

   int* piQuit = (int*) argument;
   struct timeval timeRXRadio;
   
   int maxfd = -1;
   int iCountFD = 0;
   int iLoopCounter = 0;
   u32 uTimeLastLoopCheck = get_current_timestamp_ms();
   
   while ( 1 )
   {
      hardware_sleep_micros(200);

      u32 uTime = get_current_timestamp_ms();
      if ( uTime - uTimeLastLoopCheck > s_iRadioRxLoopTimeoutInterval )
      {
         log_softerror_and_alarm("[RadioRxThread] Radio Rx loop took too long (%d ms, read: %u microsec, queue: %u microsec).", uTime - uTimeLastLoopCheck, s_uRadioRxLastTimeRead, s_uRadioRxLastTimeQueue);
         if ( uTime - uTimeLastLoopCheck > s_RadioRxBuffers.uMaxLoopTime )
            s_RadioRxBuffers.uMaxLoopTime = uTime - uTimeLastLoopCheck;
      }
      uTimeLastLoopCheck = uTime;

      if ( s_iRadioRxAllInterfacesPaused )
      {
         log_line("[RadioRxThread] All interfaces paused. Pause Rx.");
         hardware_sleep_ms(400);
         uTimeLastLoopCheck = get_current_timestamp_ms();
         continue;
      }
      if ( s_iRadioRxMarkedForQuit )
      {
         log_line("[RadioRxThread] Rx marked for quit. Do nothing.");
         hardware_sleep_ms(20);
         uTimeLastLoopCheck = get_current_timestamp_ms();
         continue;
      }

      if ( (NULL != piQuit) && (*piQuit != 0 ) )
      {
         log_line("[RadioRxThread] Signaled to stop.");
         break;
      }

      iLoopCounter++;
         
      if ( 0 == (iLoopCounter % 20) )
      {
         _radio_rx_update_stats(uTime);
      }

      // Build exceptions FD_SET

      maxfd = -1;
      iCountFD = 0;
      FD_ZERO(&s_RadioRxReadSet);
      for(int i=0; i<hardware_get_radio_interfaces_count(); i++)
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (NULL == pRadioHWInfo) || ( !pRadioHWInfo->openedForRead) )
            continue;
         if ( s_RadioRxBuffers.iRadioInterfacesBroken[i] )
            continue;
         if ( s_iRadioRxPausedInterfaces[i] )
            continue;
         iCountFD++;
         FD_SET(pRadioHWInfo->monitor_interface_read.selectable_fd, &s_RadioRxReadSet);
         if ( pRadioHWInfo->monitor_interface_read.selectable_fd > maxfd )
            maxfd = pRadioHWInfo->monitor_interface_read.selectable_fd;
      }

      // Check for exceptions first

      s_uRadioRxLastTimeQueue = 0;
      s_uRadioRxLastTimeRead = 0;
      u32 uTimeLastRead = get_current_timestamp_micros();

      timeRXRadio.tv_sec = 0;
      timeRXRadio.tv_usec = 10; // 0.01 miliseconds timeout

      int nResult = 0;
      if ( iCountFD > 0 )
         nResult = select(maxfd+1, NULL, NULL, &s_RadioRxReadSet, &timeRXRadio);

      s_uRadioRxLastTimeRead += get_current_timestamp_micros() - uTimeLastRead;
      
      if ( nResult < 0 || nResult > 0 )
      {
         for(int i=0; i<hardware_get_radio_interfaces_count(); i++)
         {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
            if ( ( NULL != pRadioHWInfo) && (FD_ISSET(pRadioHWInfo->monitor_interface_read.selectable_fd, &s_RadioRxReadSet)) )
            if ( 0 == s_iRadioRxPausedInterfaces[i] )
            if ( pRadioHWInfo->openedForRead )
            {
               log_softerror_and_alarm("[RadioRxThread] Radio interface %d [%s] has an exception.", i+1, pRadioHWInfo->szName);
               s_RadioRxBuffers.iRadioInterfacesBroken[i] = 1;
               //if ( hardware_radio_index_is_sik_radio(i) )
               //   flag_reinit_sik_interface(i);
            }
         }
      }

      // Build read FD_SET

      FD_ZERO(&s_RadioRxReadSet);
      maxfd = 0;
      iCountFD = 0;
      timeRXRadio.tv_sec = 0;
      timeRXRadio.tv_usec = 1000; // 1 miliseconds timeout

      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
            continue;
         if ( s_RadioRxBuffers.iRadioInterfacesBroken[i] )
            continue;
         if ( s_iRadioRxPausedInterfaces[i] )
            continue;

         iCountFD++;
         FD_SET(pRadioHWInfo->monitor_interface_read.selectable_fd, &s_RadioRxReadSet);
         if ( pRadioHWInfo->monitor_interface_read.selectable_fd > maxfd )
            maxfd = pRadioHWInfo->monitor_interface_read.selectable_fd;
      }

      // Check for read ready

      uTimeLastRead = get_current_timestamp_micros();
      
      nResult = 0;
      if ( iCountFD > 0 )
         nResult = select(maxfd+1, &s_RadioRxReadSet, NULL, NULL, &timeRXRadio);

      s_uRadioRxLastTimeRead += get_current_timestamp_micros() - uTimeLastRead;
      s_uRadioRxLastTimeQueue = 0;

      if ( nResult < 0 )
      {
         log_line("[RadioRxThread] Radio interfaces have broken up. Exception on select read radio handle.");
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
            s_RadioRxBuffers.iRadioInterfacesBroken[i] = 1;
         continue;
      }

      // Received data, process it

      for(int iInterfaceIndex=0; iInterfaceIndex<hardware_get_radio_interfaces_count(); iInterfaceIndex++)
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
         if( (NULL == pRadioHWInfo) || (s_iRadioRxPausedInterfaces[iInterfaceIndex]) || (0 == FD_ISSET(pRadioHWInfo->monitor_interface_read.selectable_fd, &s_RadioRxReadSet)) )
            continue;

         if ( hardware_radio_index_is_sik_radio(iInterfaceIndex) )
         {
            int iResult = _radio_rx_parse_received_sik_radio_data(iInterfaceIndex);
            if ( iResult < 0 )
            {
               s_RadioRxBuffers.iRadioInterfacesBroken[iInterfaceIndex] = 1;
               continue;
            }
         }
         else
         {
            int iResult = _radio_rx_parse_received_wifi_radio_data(iInterfaceIndex);
            if ( iResult < 0 )
            {
               s_RadioRxBuffers.iRadioInterfacesBroken[iInterfaceIndex] = 1;
               continue;
            }
         }

         if ( (NULL != piQuit) && (*piQuit != 0 ) )
         {
            log_line("[RadioRxThread] Signaled to stop.");
            break;
         }
      }
   }
   log_line("[RadioRxThread] Stopped.");
   return NULL;
}

int radio_rx_start_rx_thread(shared_mem_radio_stats* pSMRadioStats, shared_mem_radio_stats_interfaces_rx_graph* pSMRadioRxGraphs, int iSearchMode)
{
   if ( s_iRadioRxInitialized )
      return 1;

   s_pSMRadioStats = pSMRadioStats;
   s_pSMRadioRxGraphs = pSMRadioRxGraphs;
   s_iSearchMode = iSearchMode;
   s_iRadioRxSingalStop = 0;

   radio_rx_reset_interfaces_broken_state();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_iRadioRxPausedInterfaces[i] = 0;

   s_iRadioRxAllInterfacesPaused = 0;

   for( int i=0; i<MAX_RX_PACKETS_QUEUE; i++ )
   {
      s_RadioRxBuffers.iPacketsLengths[i] = 0;
      s_RadioRxBuffers.iPacketsAreShort[i] = 0;
      s_RadioRxBuffers.pPacketsBuffers[i] = (u8*) malloc(MAX_PACKET_TOTAL_SIZE);
      if ( NULL == s_RadioRxBuffers.pPacketsBuffers[i] )
      {
         log_error_and_alarm("[RadioRx] Failed to allocate rx packets buffers!");
         return 0;
      }
   }

   log_line("[RadioRx] Allocated %u bytes for %d rx packets.", MAX_RX_PACKETS_QUEUE * MAX_PACKET_TOTAL_SIZE, MAX_RX_PACKETS_QUEUE);

   s_RadioRxBuffers.iCurrentRxPacketToConsume = -1;
   s_RadioRxBuffers.iCurrentRxPacketIndex = 0;

   s_RadioRxBuffers.uTimeLastStatsUpdate = get_current_timestamp_ms();
   
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      s_RadioRxBuffers.uVehiclesIds[i] =0;
      s_RadioRxBuffers.uTotalRxPackets[i] = 0;
      s_RadioRxBuffers.uTotalRxPacketsBad[i] = 0;
      s_RadioRxBuffers.uTotalRxPacketsLost[i] = 0;
      s_RadioRxBuffers.uTmpRxPackets[i] = 0;
      s_RadioRxBuffers.uTmpRxPacketsBad[i] = 0;
      s_RadioRxBuffers.uTmpRxPacketsLost[i] = 0;
      s_RadioRxBuffers.iMaxRxPacketsPerSec[i] = 0;
      s_RadioRxBuffers.iMinRxPacketsPerSec[i] = 1000000;

      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
         s_RadioRxBuffers.uLastRxRadioLinkPacketIndex[i][k] = 0;
   }
   s_RadioRxBuffers.uTimeLastMinute = get_current_timestamp_ms();
   s_RadioRxBuffers.iMaxPacketsInQueue = 0;
   s_RadioRxBuffers.iMaxPacketsInQueueLastMinute = 0;

   
   s_RadioRxBuffers.uMaxLoopTime = 0;

   if ( 0 != pthread_mutex_init(&s_pThreadRadioRxMutex, NULL) )
   {
      log_error_and_alarm("[RadioRx] Failed to init mutex.");
      return 0;
   }

   if ( 0 != pthread_create(&s_pThreadRadioRx, NULL, &_thread_radio_rx, (void*)&s_iRadioRxSingalStop) )
   {
      log_error_and_alarm("[RadioRx] Failed to create thread for radio rx.");
      return 0;
   }

   s_iRadioRxInitialized = 1;
   log_line("[RadioRx] Started radio rx thread.");
   return 1;
}

void radio_rx_stop_rx_thread()
{
   if ( ! s_iRadioRxInitialized )
      return;

   log_line("[RadioRx] Signaled radio rx thread to stop.");
   s_iRadioRxSingalStop = 1;
   s_iRadioRxInitialized = 0;

   pthread_mutex_lock(&s_pThreadRadioRxMutex);
   pthread_mutex_unlock(&s_pThreadRadioRxMutex);

   pthread_cancel(s_pThreadRadioRx);
   pthread_mutex_destroy(&s_pThreadRadioRxMutex);
}


void radio_rx_set_timeout_interval(int iMiliSec)
{
   s_iRadioRxLoopTimeoutInterval = iMiliSec;
   log_line("[RadioRx] Set loop check timeout interval to %d ms.", s_iRadioRxLoopTimeoutInterval);
}

void _radio_rx_check_update_all_paused_flag()
{
   s_iRadioRxAllInterfacesPaused = 1;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( 0 == s_iRadioRxPausedInterfaces[i] )
      {
         s_iRadioRxAllInterfacesPaused = 0;
         break;
      }
   }
}

void radio_rx_pause_interface(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   log_line("[RadioRx] Pause Rx on radio interface %d", iInterfaceIndex+1);

   if ( ! s_iRadioRxInitialized )
   {
      s_iRadioRxPausedInterfaces[iInterfaceIndex] = 1;
      _radio_rx_check_update_all_paused_flag();
      return;
   }

   pthread_mutex_lock(&s_pThreadRadioRxMutex);
   s_iRadioRxPausedInterfaces[iInterfaceIndex] = 1;
   _radio_rx_check_update_all_paused_flag();
   pthread_mutex_unlock(&s_pThreadRadioRxMutex);
}

void radio_rx_resume_interface(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   log_line("[RadioRx] Resumed Rx on radio interface %d", iInterfaceIndex+1);

   if ( ! s_iRadioRxInitialized )
   {
      s_iRadioRxPausedInterfaces[iInterfaceIndex] = 0;
      s_iRadioRxAllInterfacesPaused = 0;
      return;
   }
   
   pthread_mutex_lock(&s_pThreadRadioRxMutex);
   s_iRadioRxPausedInterfaces[iInterfaceIndex] = 0;
   s_iRadioRxAllInterfacesPaused = 0;
   pthread_mutex_unlock(&s_pThreadRadioRxMutex);
}

void radio_rx_mark_quit()
{
   s_iRadioRxMarkedForQuit = 1;
}

int radio_rx_any_interface_broken()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( s_RadioRxBuffers.iRadioInterfacesBroken[i] )
          return i+1;

   return 0;
}

int radio_rx_any_interface_bad_packets()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( s_RadioRxBuffers.iRadioInterfacesRxBadPackets[i] )
          return i+1;

   return 0;
}

int radio_rx_get_interface_bad_packets_error_and_reset(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;
   int iError = s_RadioRxBuffers.iRadioInterfacesRxBadPackets[iInterfaceIndex];
   s_RadioRxBuffers.iRadioInterfacesRxBadPackets[iInterfaceIndex] = 0;
   return iError;
}

int radio_rx_any_rx_timeouts()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( s_RadioRxBuffers.iRadioInterfacesRxTimeouts[i] > 0 )
          return i+1;
   return 0;
}

int radio_rx_get_timeout_count_and_reset(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;
   int iCount = s_RadioRxBuffers.iRadioInterfacesRxTimeouts[iInterfaceIndex];
   s_RadioRxBuffers.iRadioInterfacesRxTimeouts[iInterfaceIndex] = 0;
   return iCount;
}

void radio_rx_reset_interfaces_broken_state()
{
   log_line("[RadioRx] Reset broken state for all interface. Mark all interfaces as not broken.");
   
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      s_RadioRxBuffers.iRadioInterfacesBroken[i] = 0;
      s_RadioRxBuffers.iRadioInterfacesRxTimeouts[i] = 0;
      s_RadioRxBuffers.iRadioInterfacesRxBadPackets[i] = 0;
   }
}

t_radio_rx_buffers* radio_rx_get_state()
{
   return &s_RadioRxBuffers;
}

int radio_rx_has_packets_to_consume()
{
   if ( 0 == s_iRadioRxInitialized )
      return 0;
   if ( -1 == s_RadioRxBuffers.iCurrentRxPacketToConsume )
      return 0;

   int iCount = 0;
   int iBottom = 0;
   int iTop = 0;
   //pthread_mutex_lock(&s_pThreadRadioRxMutex);
   iBottom = s_RadioRxBuffers.iCurrentRxPacketToConsume;
   iTop = s_RadioRxBuffers.iCurrentRxPacketIndex;
   //pthread_mutex_unlock(&s_pThreadRadioRxMutex);

   if ( iBottom < iTop )
      iCount = iTop - iBottom;
   else
      iCount = MAX_RX_PACKETS_QUEUE - iBottom + iTop;
   
   return iCount;
}

u8* radio_rx_get_next_received_packet(int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex)
{
   if ( NULL != pLength )
      *pLength = 0;
   if ( NULL != pIsShortPacket )
      *pIsShortPacket = 0;
   if ( NULL != pRadioInterfaceIndex )
      *pRadioInterfaceIndex = 0;

   if ( 0 == s_iRadioRxInitialized )
      return NULL;
   if ( -1 == s_RadioRxBuffers.iCurrentRxPacketToConsume )
      return NULL;

   pthread_mutex_lock(&s_pThreadRadioRxMutex);

   if ( s_RadioRxBuffers.iCurrentRxPacketToConsume == s_RadioRxBuffers.iCurrentRxPacketIndex )
   {
      pthread_mutex_unlock(&s_pThreadRadioRxMutex);
      return NULL;
   }

   if ( NULL != pLength )
      *pLength = s_RadioRxBuffers.iPacketsLengths[s_RadioRxBuffers.iCurrentRxPacketToConsume];
   if ( NULL != pIsShortPacket )
      *pIsShortPacket = s_RadioRxBuffers.iPacketsAreShort[s_RadioRxBuffers.iCurrentRxPacketToConsume];
   if ( NULL != pRadioInterfaceIndex )
      *pRadioInterfaceIndex = s_RadioRxBuffers.iPacketsRxInterface[s_RadioRxBuffers.iCurrentRxPacketToConsume];

   memcpy( s_tmpLastProcessedRadioRxPacket, s_RadioRxBuffers.pPacketsBuffers[s_RadioRxBuffers.iCurrentRxPacketToConsume], s_RadioRxBuffers.iPacketsLengths[s_RadioRxBuffers.iCurrentRxPacketToConsume]);

   s_RadioRxBuffers.iCurrentRxPacketToConsume++;
   if ( s_RadioRxBuffers.iCurrentRxPacketToConsume >= MAX_RX_PACKETS_QUEUE )
      s_RadioRxBuffers.iCurrentRxPacketToConsume = 0;

   pthread_mutex_unlock(&s_pThreadRadioRxMutex);

   return s_tmpLastProcessedRadioRxPacket;
}

u32 radio_rx_get_and_reset_max_loop_time()
{
   u32 u = s_RadioRxBuffers.uMaxLoopTime;
   s_RadioRxBuffers.uMaxLoopTime = 0;
   return u;
}

u32 radio_rx_get_and_reset_max_loop_time_read()
{
   u32 u = s_uRadioRxLastTimeRead;
   s_uRadioRxLastTimeRead = 0;
   return u;
}

u32 radio_rx_get_and_reset_max_loop_time_queue()
{
   u32 u = s_uRadioRxLastTimeQueue;
   s_uRadioRxLastTimeQueue = 0;
   return u;
}

