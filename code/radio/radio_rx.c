/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and/or binary forms, with or without
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
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../base/base.h"
#include "../base/encr.h"
#include "../base/config_hw.h"
#include "../base/hw_procs.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "radio_rx.h"
#include "radiolink.h"
#include "radio_duplicate_det.h"

int s_iRadioRxInitialized = 0;
int s_iRadioRxSingalStop = 0;
int s_iRadioRxDevMode = 0;
int s_iRadioRxLoopTimeoutInterval = 15;
int s_iRadioRxMarkedForQuit = 0;
int s_iDefaultRxThreadPriority = -1;
int s_iCustomRxThreadPriority = DEFAULT_PRIORITY_THREAD_RADIO_RX;
int s_iLastSetCustomRxThreadPriority = DEFAULT_PRIORITY_THREAD_RADIO_RX;

t_radio_rx_state s_RadioRxState;
pthread_t s_pThreadRadioRx;

shared_mem_radio_stats* s_pSMRadioStats = NULL;
shared_mem_radio_stats_interfaces_rx_graph* s_pSMRadioRxGraphs = NULL;
int s_iSearchMode = 0;
u32 s_uRadioRxTimeNow = 0;
u32 s_uRadioRxLastTimeRead = 0;
u32 s_uRadioRxLastTimeQueue = 0;

int s_iRadioRxPausedInterfaces[MAX_RADIO_INTERFACES];
int s_iRadioRxAllInterfacesPaused = 0;

fd_set s_RadioRxReadSet;
fd_set s_RadioRxExceptionSet;
int s_iRadioRxCountFDs = 0;
int s_iRadioRxMaxFD = 0;
struct timeval s_iRadioRxReadTimeInterval;

u8 s_tmpLastProcessedRadioRxPacket[MAX_PACKET_TOTAL_SIZE];

u32 s_uLastRxShortPacketsVehicleIds[MAX_RADIO_INTERFACES];

// Pointers to array of int-s (max radio cards, for each card)
u8* s_pPacketsCounterOutputVideo = NULL;
u8* s_pPacketsCounterOutputECVideo = NULL;
u8* s_pPacketsCounterOutputRetrVideo = NULL;
u8* s_pPacketsCounterOutputData = NULL;
u8* s_pPacketsCounterOutputMissing = NULL;
u8* s_pPacketsCounterOutputMissingMaxGap = NULL;

extern pthread_mutex_t s_pMutexRadioSyncRxTxThreads;
extern int s_iMutexRadioSyncRxTxThreadsInitialized;

volatile int s_bHasPendingOperation = 0;
volatile int s_bCanDoOperations = 0;

extern u32 s_uLastRadioPingSentTime;
extern u8 s_uLastRadioPingId;

// returns the number of missing packets detected on the radio link

int _radio_rx_update_local_stats_on_new_radio_packet(int iInterface, int iIsShortPacket, u32 uVehicleId, u8* pPacket, int iLength, int iDataIsOk)
{
   if ( (NULL == pPacket) || ( iLength <= 2 ) )
      return 0;

   int nReturnLost = 0;

   //----------------------------------------------
   // Begin: Compute index of stats to use

   t_radio_rx_state_vehicle* pStatsVehicle = NULL;
   if ( (s_RadioRxState.vehicles[0].uVehicleId == 0) || (s_RadioRxState.vehicles[0].uVehicleId == uVehicleId)  )
      pStatsVehicle = &(s_RadioRxState.vehicles[0]);
   
   if ( NULL == pStatsVehicle )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( s_RadioRxState.vehicles[i].uVehicleId == uVehicleId )
         {
            pStatsVehicle = &(s_RadioRxState.vehicles[i]);
            break;
         }
      }
   }

   if ( NULL == pStatsVehicle )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( s_RadioRxState.vehicles[i].uVehicleId == 0 )
         {
            pStatsVehicle = &(s_RadioRxState.vehicles[i]);
            break;
         }
      }
   }

   if ( NULL == pStatsVehicle )
      pStatsVehicle = &(s_RadioRxState.vehicles[MAX_CONCURENT_VEHICLES-1]);

   // End: Compute index of stats to use
   //----------------------------------------------

   pStatsVehicle->uVehicleId = uVehicleId;

   if ( iDataIsOk )
   {
      pStatsVehicle->uTotalRxPackets++;
      pStatsVehicle->uTmpRxPackets++;
   }
   else
   {
      pStatsVehicle->uTotalRxPacketsBad++;
      pStatsVehicle->uTmpRxPacketsBad++;
   }

   if ( iIsShortPacket )
   {
      t_packet_header_short* pPHS = (t_packet_header_short*)pPacket;
      u32 uNext = ((pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface]+1) & 0xFF);
      if ( pPHS->packet_id != uNext )
      {
         u32 lost = pPHS->packet_id - uNext;
         if ( pPHS->packet_id < uNext )
            lost = pPHS->packet_id + 255 - uNext;
         pStatsVehicle->uTotalRxPacketsLost += lost;
         pStatsVehicle->uTmpRxPacketsLost += lost;
         nReturnLost = lost;
      }

      pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] = pPHS->packet_id;
   }
   else
   {
      t_packet_header* pPH = (t_packet_header*)pPacket;
      if ( pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] > 0 )
      if ( pPH->radio_link_packet_index > pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface]+1 )
      {
         u32 lost = pPH->radio_link_packet_index - (pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] + 1);
         pStatsVehicle->uTotalRxPacketsLost += lost;
         pStatsVehicle->uTmpRxPacketsLost += lost;
         nReturnLost = lost;
      }

      if ( pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] > 0 )
      if ( pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] < 0xFA00 )
      if ( pPH->radio_link_packet_index > 0x0500 )
      if ( pPH->radio_link_packet_index < pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] )
          radio_dup_detection_set_vehicle_restarted_flag(pPH->vehicle_id_src);

      pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] = pPH->radio_link_packet_index;
   }
   return nReturnLost;
}

void _radio_rx_update_fd_sets()
{
   FD_ZERO(&s_RadioRxReadSet);
   FD_ZERO(&s_RadioRxExceptionSet);

   s_iRadioRxCountFDs = 0;
   s_iRadioRxMaxFD = 0;
   s_iRadioRxReadTimeInterval.tv_sec = 0;
   s_iRadioRxReadTimeInterval.tv_usec = 20000; // 20 milisec timeout

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
         continue;
      if ( s_RadioRxState.iRadioInterfacesBroken[i] )
         continue;
      if ( s_iRadioRxPausedInterfaces[i] )
         continue;

      FD_SET(pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, &s_RadioRxReadSet);
      FD_SET(pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, &s_RadioRxExceptionSet);

      s_iRadioRxCountFDs++;
      if ( pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd > s_iRadioRxMaxFD )
         s_iRadioRxMaxFD = pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd;
   }
   s_iRadioRxMaxFD++;
}

void _radio_rx_add_packet_to_rx_queue(u8* pPacket, int iLength, int iRadioInterface)
{
   if ( (NULL == pPacket) || (iLength <= 0) || s_iRadioRxMarkedForQuit )
      return;

   t_radio_rx_state_packets_queue* pQueue = &s_RadioRxState.queue_reg_priority;
   t_packet_header* pPH = (t_packet_header*)pPacket;
   if ( radio_packet_type_is_high_priority( pPH->packet_type ) )
      pQueue = &s_RadioRxState.queue_high_priority;

   // Add the packet to the queue
   pQueue->uPacketsRxInterface[pQueue->iCurrentRxPacketIndex] = iRadioInterface;
   pQueue->uPacketsAreShort[pQueue->iCurrentRxPacketIndex] = 0;
   pQueue->iPacketsLengths[pQueue->iCurrentRxPacketIndex] = iLength;
   memcpy(pQueue->pPacketsBuffers[pQueue->iCurrentRxPacketIndex], pPacket, iLength);
   
   u32 uTimeStartTryLock = get_current_timestamp_ms();
   int iLock = -1;
   while ( (iLock != 0 ) && (get_current_timestamp_ms() <= uTimeStartTryLock+5) )
   {
      iLock = pthread_mutex_trylock(&(pQueue->mutex));
      hardware_sleep_micros(100);
   }

   if ( iLock != 0 )
      log_softerror_and_alarm("[RadioRxThread] Doing unguarded rx buffers manipulation");
   
   // No more room? Discard it
   if ( pQueue->iCurrentRxPacketsInQueue >= MAX_RX_PACKETS_QUEUE )
   {
      if ( 0 == iLock )
         pthread_mutex_unlock(&(pQueue->mutex));
      s_uRadioRxLastTimeQueue += get_current_timestamp_ms() - s_uRadioRxTimeNow;
      return;
   }

   pQueue->iCurrentRxPacketsInQueue++;   
   pQueue->iCurrentRxPacketIndex++;
   if ( pQueue->iCurrentRxPacketIndex >= MAX_RX_PACKETS_QUEUE )
      pQueue->iCurrentRxPacketIndex = 0;

   if ( pQueue->iCurrentRxPacketsInQueue > pQueue->iStatsMaxPacketsInQueueLastMinute )
      pQueue->iStatsMaxPacketsInQueueLastMinute = pQueue->iCurrentRxPacketsInQueue;
   if ( pQueue->iCurrentRxPacketsInQueue > pQueue->iStatsMaxPacketsInQueue )
      pQueue->iStatsMaxPacketsInQueue = pQueue->iCurrentRxPacketsInQueue;

   if ( NULL != pQueue->pSemaphore )
         sem_post(pQueue->pSemaphore);

   if ( 0 == iLock )
      pthread_mutex_unlock(&(pQueue->mutex));

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


int _radio_rx_process_serial_short_packet(int iInterfaceIndex, u8* pPacketBuffer, int iPacketLength)
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
         if ( (uCRC & 0x00FFFFFF) == (pPH->uCRC & 0x00FFFFFF) )
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

int _radio_rx_parse_received_serial_radio_data(int iInterfaceIndex)
{
   static u8 s_uBuffersSerialMessages[MAX_RADIO_INTERFACES][512];
   static int s_uBuffersSerialMessagesReadPos[MAX_RADIO_INTERFACES];
   static int s_bInitializedBuffersSerialMessages = 0;

   if ( ! s_bInitializedBuffersSerialMessages )
   {
      s_bInitializedBuffersSerialMessages = 1;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         s_uBuffersSerialMessagesReadPos[i] = 0;
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
   {
      log_softerror_and_alarm("[RadioRxThread] Tried to process a received short radio packet on radio interface (%d) that is not opened for read.", iInterfaceIndex+1);
      return -1;
   }
   if ( ! hardware_radio_index_is_serial_radio(iInterfaceIndex) )
   {
      log_softerror_and_alarm("[RadioRxThread] Tried to process a received short radio packet on radio interface (%d) that is not a serial radio.", iInterfaceIndex+1);
      return 0;
   }

   int iMaxRead = 512 - s_uBuffersSerialMessagesReadPos[iInterfaceIndex];

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   int iRead = read(pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, &(s_uBuffersSerialMessages[iInterfaceIndex][s_uBuffersSerialMessagesReadPos[iInterfaceIndex]]), iMaxRead);

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   if ( iRead < 0 )
   {
      log_softerror_and_alarm("[RadioRxThread] Failed to read received short radio packet on serial radio interface (%d).", iInterfaceIndex+1);
      return -1;
   }

   s_uBuffersSerialMessagesReadPos[iInterfaceIndex] += iRead;
   int iBufferLength = s_uBuffersSerialMessagesReadPos[iInterfaceIndex];
   
   // Received at least the full header?
   if ( iBufferLength < (int)sizeof(t_packet_header_short) )
      return 0;

   s_uRadioRxTimeNow = get_current_timestamp_ms();

   int iPacketPos = -1;
   do
   {
      u8* pData = (u8*)&(s_uBuffersSerialMessages[iInterfaceIndex][0]);
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
               s_uBuffersSerialMessages[iInterfaceIndex][i] = s_uBuffersSerialMessages[iInterfaceIndex][i+iBytesToDiscard];
            s_uBuffersSerialMessagesReadPos[iInterfaceIndex] -= iBytesToDiscard;

            _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 1, s_uLastRxShortPacketsVehicleIds[iInterfaceIndex], s_uBuffersSerialMessages[iInterfaceIndex], iBytesToDiscard, 0);
            radio_stats_set_bad_data_on_current_rx_interval(s_pSMRadioStats, NULL, iInterfaceIndex);
         }
         return 0;
      }

      if ( iPacketPos > 0 )
      {
         _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 1, s_uLastRxShortPacketsVehicleIds[iInterfaceIndex], s_uBuffersSerialMessages[iInterfaceIndex], iPacketPos, 0);
         radio_stats_set_bad_data_on_current_rx_interval(s_pSMRadioStats, NULL, iInterfaceIndex);
      }
      t_packet_header_short* pPHS = (t_packet_header_short*)(pData+iPacketPos);
      int iShortTotalPacketSize = (int)(pPHS->data_length + sizeof(t_packet_header_short));
      
      _radio_rx_process_serial_short_packet(iInterfaceIndex, pData+iPacketPos, iShortTotalPacketSize);

      iShortTotalPacketSize += iPacketPos;
      if ( iShortTotalPacketSize > iBufferLength )
         iShortTotalPacketSize = iBufferLength;
      for( int i=0; i<iBufferLength - iShortTotalPacketSize; i++ )
         s_uBuffersSerialMessages[iInterfaceIndex][i] = s_uBuffersSerialMessages[iInterfaceIndex][i+iShortTotalPacketSize];
      s_uBuffersSerialMessagesReadPos[iInterfaceIndex] -= iShortTotalPacketSize;
      iBufferLength -= iShortTotalPacketSize;
   } while ( (iPacketPos >= 0) && (iBufferLength >= (int)sizeof(t_packet_header_short)) );
   return 0;
}

// return number of packets parsed, -1 if the interface is now invalid or broken

int _radio_rx_parse_received_wifi_radio_data(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;

   int iReturn = 0;
   int iDataIsOk = 1;
   int iBufferLength = 0;
   u8* pBuffer = NULL;
   int iCountParsed = 0;

   for( int iCountReads=0; iCountReads<3; iCountReads++ )
   {
      iBufferLength = 0;
      pBuffer = radio_process_wlan_data_in(iInterfaceIndex, &iBufferLength);
      if ( NULL == pBuffer )
         break;

      u8* pData = pBuffer;
      int iLength = iBufferLength;

      if ( iBufferLength <= 0 )
      {
         log_softerror_and_alarm("[RadioRxThread] Rx cap returned an empty buffer (%d length) on radio interface index %d.", iBufferLength, iInterfaceIndex+1);
         iDataIsOk = 0;
         iLength = 0;
         iReturn = -1;
         break;
      }

      iCountParsed++;

      int iCountPackets = 0;
      int iIsVideoData = 0;

      while ( (iLength > 0) && (NULL != pData) )
      { 
         t_packet_header* pPH = (t_packet_header*)pData;

         if ( s_iRadioRxDevMode )
         if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
         {
            s_uLastRadioPingSentTime = get_current_timestamp_ms();
            s_uLastRadioPingId = *(pData +sizeof(t_packet_header));
         }

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
            iIsVideoData = 1;

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
         {
            t_packet_header_video_full_98* pPHVF = (t_packet_header_video_full_98*) (pData+sizeof(t_packet_header));    
            
            if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
            {
               if ( NULL != s_pPacketsCounterOutputRetrVideo )
                  s_pPacketsCounterOutputRetrVideo[iInterfaceIndex] = s_pPacketsCounterOutputRetrVideo[iInterfaceIndex]+1;
            }
            else
            {
               if ( pPHVF->uCurrentBlockPacketIndex >= pPHVF->uCurrentBlockDataPackets )
               {
                  if ( NULL != s_pPacketsCounterOutputECVideo )
                     s_pPacketsCounterOutputECVideo[iInterfaceIndex] = s_pPacketsCounterOutputECVideo[iInterfaceIndex]+1;
               }
               else
               {
                  if ( NULL != s_pPacketsCounterOutputVideo )
                     s_pPacketsCounterOutputVideo[iInterfaceIndex] = s_pPacketsCounterOutputVideo[iInterfaceIndex]+1;
               }
            }
         }
         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_VIDEO )
         if ( NULL != s_pPacketsCounterOutputData )
            s_pPacketsCounterOutputData[iInterfaceIndex] = s_pPacketsCounterOutputData[iInterfaceIndex]+1;

         // To fix
         /*
         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
         {
            t_packet_header_video_full_77* pPHVF = (t_packet_header_video_full_77*) (pData+sizeof(t_packet_header));    
            if ( ! (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
            if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
            if ( pPHVF->video_block_packet_index < pPHVF->block_packets)
            {
               u8* pExtraData = pData + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_77) + pPHVF->video_data_length;
               u32* pExtraDataU32 = (u32*)pExtraData;
               pExtraDataU32[4] = get_current_timestamp_ms();
            }
         }
         */
         iCountPackets++;
         int bCRCOk = 0;
         int iPacketLength = packet_process_and_check(iInterfaceIndex, pData, iLength, &bCRCOk);

         if ( iPacketLength <= 0 )
         {
            iDataIsOk = 0;
            s_RadioRxState.iRadioInterfacesRxBadPackets[iInterfaceIndex] = get_last_processing_error_code();
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
    
      // To fix, remove
      //t_packet_header* pPH2 = (t_packet_header*)pBuffer;
      //if ( (pPH2->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
      //if ( NULL != s_pVideoPacketsCounterOutput )
      //   continue;

      s_uRadioRxTimeNow = get_current_timestamp_ms();

      t_packet_header* pPH = (t_packet_header*)pBuffer;
      u32 uVehicleId = pPH->vehicle_id_src;

      int nLost = _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 0, uVehicleId, pBuffer, iBufferLength, iDataIsOk);
      if ( nLost > 0 )
      {
         if ( NULL != s_pPacketsCounterOutputMissing)
            s_pPacketsCounterOutputMissing[iInterfaceIndex] = s_pPacketsCounterOutputMissing[iInterfaceIndex]+1;
         if ( NULL != s_pPacketsCounterOutputMissingMaxGap )
         if ( nLost > s_pPacketsCounterOutputMissingMaxGap[iInterfaceIndex] )
            s_pPacketsCounterOutputMissingMaxGap[iInterfaceIndex] = (u8)nLost;
      }
      if ( NULL != s_pSMRadioStats )
         radio_stats_update_on_new_radio_packet_received(s_pSMRadioStats, s_pSMRadioRxGraphs, s_uRadioRxTimeNow, iInterfaceIndex, pBuffer, iBufferLength, 0, iIsVideoData, iDataIsOk);
   }

   if ( iReturn < 0 )
      return iReturn;

   return iCountParsed;
}

void _radio_rx_update_stats(u32 uTimeNow)
{
   static int s_iCounterRadioRxStatsUpdate = 0;
   static int s_iCounterRadioRxStatsUpdate2 = 0;

   if ( uTimeNow >= s_RadioRxState.uTimeLastStatsUpdate + 1000 )
   {
      s_RadioRxState.uTimeLastStatsUpdate = uTimeNow;
      s_iCounterRadioRxStatsUpdate++;
      int iAnyRxPackets = 0;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( s_RadioRxState.vehicles[i].uVehicleId == 0 )
            continue;

         if ( (s_RadioRxState.vehicles[i].uTmpRxPackets > 0) || (s_RadioRxState.vehicles[i].uTmpRxPacketsBad > 0) )
            iAnyRxPackets = 1;
         if ( s_RadioRxState.vehicles[i].uTotalRxPackets > 0 )
         {
            if ( s_RadioRxState.vehicles[i].uTmpRxPackets > s_RadioRxState.vehicles[i].iMaxRxPacketsPerSec )
               s_RadioRxState.vehicles[i].iMaxRxPacketsPerSec = s_RadioRxState.vehicles[i].uTmpRxPackets;
            if ( s_RadioRxState.vehicles[i].uTmpRxPackets < s_RadioRxState.vehicles[i].iMinRxPacketsPerSec )
               s_RadioRxState.vehicles[i].iMinRxPacketsPerSec = s_RadioRxState.vehicles[i].uTmpRxPackets;
         }

         /*
         if ( (s_iCounterRadioRxStatsUpdate % 10) == 0 )
         {
            log_line("[RadioRxThread] Received packets from VID %u: %u/sec (min: %d/sec, max: %d/sec)", 
               s_RadioRxState.vehicles[i].uVehicleId, s_RadioRxState.vehicles[i].uTmpRxPackets,
               s_RadioRxState.vehicles[i].iMinRxPacketsPerSec, s_RadioRxState.vehicles[i].iMaxRxPacketsPerSec);
            log_line("[RadioRxThread] Total recv packets from VID: %u: %u, bad: %u, lost: %u",
               s_RadioRxState.vehicles[i].uVehicleId, s_RadioRxState.vehicles[i].uTotalRxPackets, s_RadioRxState.vehicles[i].uTotalRxPacketsBad, s_RadioRxState.vehicles[i].uTotalRxPacketsLost );
         }
         */
         
         s_RadioRxState.vehicles[i].uTmpRxPackets = 0;
         s_RadioRxState.vehicles[i].uTmpRxPacketsBad = 0;
         s_RadioRxState.vehicles[i].uTmpRxPacketsLost = 0;
      }

      if ( (s_iCounterRadioRxStatsUpdate % 10) == 0 )
      if ( 0 == iAnyRxPackets )
         log_line("[RadioRxThread] No packets received in the last seconds");
   }
   if ( uTimeNow >= s_RadioRxState.uTimeLastMinuteStatsUpdate + 1000 * 5 )
   {
      s_RadioRxState.uTimeLastMinuteStatsUpdate = uTimeNow;
      s_iCounterRadioRxStatsUpdate2++;
      log_line("[RadioRxThread] Max packets in queues (high/reg prio): %d/%d. Max packets in queue in last 10 sec: %d/%d",
         s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueue,
         s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueue,
         s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueueLastMinute,
         s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueueLastMinute);
      s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueueLastMinute = 0;
      s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueueLastMinute = 0;

      radio_duplicate_detection_log_info();

      if ( (s_iCounterRadioRxStatsUpdate2 % 10) == 0 )
      {
         log_line("[RadioRxThread] Reset max stats.");
         s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueue = 0;
         s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueue = 0;
      }
   }
}

static void * _thread_radio_rx(void *argument)
{
   log_line("[RadioRxThread] Started.");

   if ( s_iCustomRxThreadPriority > 0 )
   {
      int iRet = hw_increase_current_thread_priority("[RadioRxThread]", s_iCustomRxThreadPriority);
      if ( -1 == s_iDefaultRxThreadPriority )
         s_iDefaultRxThreadPriority = iRet;
   }
   else if ( s_iDefaultRxThreadPriority != -1 )
      s_iDefaultRxThreadPriority = hw_increase_current_thread_priority("[RadioRxThread]", s_iDefaultRxThreadPriority);

   log_line("[RadioRxThread] Initialized State. Waiting for rx messages...");

   int* piQuit = (int*) argument;
   int iLoopCounter = 0;
   int iLoopOkCounter = 0;
   u32 uTimeLastLoopCheck = get_current_timestamp_ms();
   u32 uTimeLastRead = 0;
   u32 uTime = 0;

   while ( 1 )
   {
      iLoopCounter++;
      if ( (NULL != piQuit) && (*piQuit != 0 ) )
      {
         log_line("[RadioRxThread] Signaled to stop.");
         break;
      }
      if ( s_iRadioRxMarkedForQuit )
      {
         if ( iLoopCounter )
            log_line("[RadioRxThread] Rx marked for quit. Do nothing.");
         iLoopCounter = -1;
         hardware_sleep_ms(10);
         continue;
      }

      if ( s_bHasPendingOperation )
      {
         s_bCanDoOperations = 1;
         hardware_sleep_ms(1);
         continue;
      }
      
      s_bCanDoOperations = 0;
      
      uTime = get_current_timestamp_ms();
      if ( uTime - uTimeLastLoopCheck > 3 )
         iLoopOkCounter = 0;
      else
         iLoopOkCounter++;

      if ( uTime - uTimeLastLoopCheck > s_iRadioRxLoopTimeoutInterval )
      {
         log_softerror_and_alarm("[RadioRxThread] Radio Rx loop took too long (%d ms, read: %u microsec, queue: %u microsec).", uTime - uTimeLastLoopCheck, s_uRadioRxLastTimeRead, s_uRadioRxLastTimeQueue);
         if ( uTime - uTimeLastLoopCheck > s_RadioRxState.uMaxLoopTime )
            s_RadioRxState.uMaxLoopTime = uTime - uTimeLastLoopCheck;
      }
      uTimeLastLoopCheck = uTime;

      // Loop is executed every 50 ms max. So update stats every 500 ms max

      if ( 0 == (iLoopCounter % 10) )
      {
         _radio_rx_update_stats(uTime);
         if ( s_iLastSetCustomRxThreadPriority != s_iCustomRxThreadPriority )
         {
            log_line("[RadioRxThread] New thread priority must be set, from %d to %d.", s_iLastSetCustomRxThreadPriority, s_iCustomRxThreadPriority);
            s_iLastSetCustomRxThreadPriority = s_iCustomRxThreadPriority;

            if ( s_iCustomRxThreadPriority > 0 )
               hw_increase_current_thread_priority("[RadioRxThread]", s_iCustomRxThreadPriority);
            else if ( s_iDefaultRxThreadPriority != -1 )
               hw_increase_current_thread_priority("[RadioRxThread]", s_iDefaultRxThreadPriority);
         }
      }

      _radio_rx_update_fd_sets();

      if ( s_iRadioRxCountFDs <= 0 )
      {
         hardware_sleep_ms(10);
         continue;
      }

      uTimeLastRead = uTime;

      int nResult = select(s_iRadioRxMaxFD, &s_RadioRxReadSet, NULL, NULL, &s_iRadioRxReadTimeInterval);

      s_uRadioRxLastTimeRead += get_current_timestamp_micros() - uTimeLastRead;
      s_uRadioRxLastTimeQueue = 0;

      if ( nResult < 0 )
      {
         log_line("[RadioRxThread] Radio interfaces have broken up. Exception on select read radio handle.");
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
            if ( (NULL != pRadioHWInfo) && pRadioHWInfo->openedForRead )
               s_RadioRxState.iRadioInterfacesBroken[i] = 1;
         }
      }

      if ( nResult <= 0 )
         continue;

      // Received data, process it
      int iMaxedInterface = -1;
      int iParsedPackets[MAX_RADIO_INTERFACES];
      memset(iParsedPackets, 0, sizeof(int)*MAX_RADIO_INTERFACES);

      // Repeat reading while we have max reads on at leas one interface
      do
      {
         iMaxedInterface = -1;
         for(int iInterfaceIndex=0; iInterfaceIndex<hardware_get_radio_interfaces_count(); iInterfaceIndex++)
         {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
            if( (NULL == pRadioHWInfo) || (s_iRadioRxPausedInterfaces[iInterfaceIndex]) || (0 == FD_ISSET(pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, &s_RadioRxReadSet)) )
               continue;

            if ( hardware_radio_index_is_serial_radio(iInterfaceIndex) )
            {
               int iResult = _radio_rx_parse_received_serial_radio_data(iInterfaceIndex);
               if ( iResult < 0 )
               {
                  s_RadioRxState.iRadioInterfacesBroken[iInterfaceIndex] = 1;
                  continue;
               }
            }
            else
            {
               iParsedPackets[iInterfaceIndex] = _radio_rx_parse_received_wifi_radio_data(iInterfaceIndex);
               if ( (iParsedPackets[iInterfaceIndex] < 0) || ( radio_get_last_read_error_code() == RADIO_READ_ERROR_INTERFACE_BROKEN ) )
               {
                  log_line("[RadioRx] Mark interface %d as broken", iInterfaceIndex+1);
                  s_RadioRxState.iRadioInterfacesBroken[iInterfaceIndex] = 1;
                  continue;
               }
               if ( iParsedPackets[iInterfaceIndex] > 2 )
                  iMaxedInterface = iInterfaceIndex;
            }
         }
      } while (iMaxedInterface != -1);
   }

   log_line("[RadioRxThread] Stopped.");
   return NULL;
}

int radio_rx_start_rx_thread(shared_mem_radio_stats* pSMRadioStats, shared_mem_radio_stats_interfaces_rx_graph* pSMRadioRxGraphs, int iSearchMode, u32 uAcceptedFirmwareType)
{
   if ( s_iRadioRxInitialized )
      return 1;

   _radio_rx_update_fd_sets();

   s_pSMRadioStats = pSMRadioStats;
   s_pSMRadioRxGraphs = pSMRadioRxGraphs;
   s_iSearchMode = iSearchMode;
   s_iRadioRxSingalStop = 0;
   s_RadioRxState.uAcceptedFirmwareType = uAcceptedFirmwareType;
   radio_rx_reset_interfaces_broken_state();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_iRadioRxPausedInterfaces[i] = 0;

   s_iRadioRxAllInterfacesPaused = 0;

   for( int i=0; i<MAX_RX_PACKETS_QUEUE; i++ )
   {
      s_RadioRxState.queue_high_priority.iPacketsLengths[i] = 0;
      s_RadioRxState.queue_high_priority.uPacketsAreShort[i] = 0;
      s_RadioRxState.queue_high_priority.uPacketsRxInterface[i] = 0;
      s_RadioRxState.queue_high_priority.pPacketsBuffers[i] = (u8*) malloc(MAX_PACKET_TOTAL_SIZE);
      if ( NULL == s_RadioRxState.queue_high_priority.pPacketsBuffers[i] )
      {
         log_error_and_alarm("[RadioRx] Failed to allocate rx packets buffers!");
         return 0;
      }
      s_RadioRxState.queue_reg_priority.iPacketsLengths[i] = 0;
      s_RadioRxState.queue_reg_priority.uPacketsAreShort[i] = 0;
      s_RadioRxState.queue_reg_priority.uPacketsRxInterface[i] = 0;
      s_RadioRxState.queue_reg_priority.pPacketsBuffers[i] = (u8*) malloc(MAX_PACKET_TOTAL_SIZE);
      if ( NULL == s_RadioRxState.queue_reg_priority.pPacketsBuffers[i] )
      {
         log_error_and_alarm("[RadioRx] Failed to allocate rx packets buffers!");
         return 0;
      }
   }

   log_line("[RadioRx] Allocated %u bytes for %d rx packets.", 2*MAX_RX_PACKETS_QUEUE * MAX_PACKET_TOTAL_SIZE, MAX_RX_PACKETS_QUEUE);

   s_RadioRxState.queue_high_priority.iCurrentRxPacketToConsume = 0;
   s_RadioRxState.queue_high_priority.iCurrentRxPacketIndex = 0;
   s_RadioRxState.queue_high_priority.iCurrentRxPacketsInQueue = 0;
   s_RadioRxState.queue_reg_priority.iCurrentRxPacketToConsume = 0;
   s_RadioRxState.queue_reg_priority.iCurrentRxPacketIndex = 0;
   s_RadioRxState.queue_reg_priority.iCurrentRxPacketsInQueue = 0;
   
   s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueue = 0;
   s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueueLastMinute = 0;
   s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueue = 0;
   s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueueLastMinute = 0;

   s_RadioRxState.uTimeLastStatsUpdate = get_current_timestamp_ms();
   s_RadioRxState.uTimeLastMinuteStatsUpdate = get_current_timestamp_ms();
   
   s_RadioRxState.queue_high_priority.pSemaphore = sem_open(RUBY_SEM_RX_RADIO_HIGH_PRIORITY, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == s_RadioRxState.queue_high_priority.pSemaphore )
   {
      log_error_and_alarm("[RadioRx] Failed to create semaphore: %s", RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
      return 0;
   }
   s_RadioRxState.queue_reg_priority.pSemaphore = sem_open(RUBY_SEM_RX_RADIO_REG_PRIORITY, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == s_RadioRxState.queue_reg_priority.pSemaphore )
   {
      log_error_and_alarm("[RadioRx] Failed to create semaphore: %s", RUBY_SEM_RX_RADIO_REG_PRIORITY);
      return 0;
   }

   if ( 0 != pthread_mutex_init(&(s_RadioRxState.queue_high_priority.mutex), NULL) )
   {
      log_error_and_alarm("[RadioRx] Failed to init high priority mutex.");
      return 0;
   }
   if ( 0 != pthread_mutex_init(&(s_RadioRxState.queue_reg_priority.mutex), NULL) )
   {
      log_error_and_alarm("[RadioRx] Failed to init reg priority mutex.");
      return 0;
   }

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      s_RadioRxState.vehicles[i].uVehicleId = 0;
      s_RadioRxState.vehicles[i].uDetectedFirmwareType = s_RadioRxState.uAcceptedFirmwareType;
      s_RadioRxState.vehicles[i].uTotalRxPackets = 0;
      s_RadioRxState.vehicles[i].uTotalRxPacketsBad = 0;
      s_RadioRxState.vehicles[i].uTotalRxPacketsLost = 0;
      s_RadioRxState.vehicles[i].uTmpRxPackets = 0;
      s_RadioRxState.vehicles[i].uTmpRxPacketsBad = 0;
      s_RadioRxState.vehicles[i].uTmpRxPacketsLost = 0;
      s_RadioRxState.vehicles[i].iMaxRxPacketsPerSec = 0;
      s_RadioRxState.vehicles[i].iMinRxPacketsPerSec = 1000000;

      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
         s_RadioRxState.vehicles[i].uLastRxRadioLinkPacketIndex[k] = 0;
   }

   s_RadioRxState.uMaxLoopTime = 0;

   if ( 0 != pthread_create(&s_pThreadRadioRx, NULL, &_thread_radio_rx, (void*)&s_iRadioRxSingalStop) )
   {
      log_error_and_alarm("[RadioRx] Failed to create thread for radio rx.");
      return 0;
   }

   s_iRadioRxInitialized = 1;
   log_line("[RadioRx] Started radio rx thread, accepted firmware types: %s.", str_format_firmware_type(s_RadioRxState.uAcceptedFirmwareType));
   return 1;
}

void radio_rx_stop_rx_thread()
{
   if ( ! s_iRadioRxInitialized )
      return;

   log_line("[RadioRx] Signaled radio rx thread to stop.");
   s_iRadioRxSingalStop = 1;
   s_iRadioRxInitialized = 0;

   pthread_mutex_lock(&(s_RadioRxState.queue_high_priority.mutex));
   pthread_mutex_unlock(&(s_RadioRxState.queue_high_priority.mutex));

   pthread_mutex_lock(&(s_RadioRxState.queue_reg_priority.mutex));
   pthread_mutex_unlock(&(s_RadioRxState.queue_reg_priority.mutex));

   pthread_cancel(s_pThreadRadioRx);

   pthread_mutex_destroy(&(s_RadioRxState.queue_high_priority.mutex));
   pthread_mutex_destroy(&(s_RadioRxState.queue_reg_priority.mutex));

   if ( NULL != s_RadioRxState.queue_high_priority.pSemaphore )
      sem_close(s_RadioRxState.queue_high_priority.pSemaphore);
   if ( NULL != s_RadioRxState.queue_reg_priority.pSemaphore )
      sem_close(s_RadioRxState.queue_reg_priority.pSemaphore);
   s_RadioRxState.queue_high_priority.pSemaphore = NULL;
   s_RadioRxState.queue_reg_priority.pSemaphore = NULL;
}

void radio_rx_set_custom_thread_priority(int iPriority)
{
   s_iCustomRxThreadPriority = iPriority;
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

void radio_rx_pause_interface(int iInterfaceIndex, const char* szReason)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   if ( s_iRadioRxInitialized )
   {
      s_bHasPendingOperation = 1;
      while ( ! s_bCanDoOperations )
         hardware_sleep_ms(1);

      pthread_mutex_lock(&(s_RadioRxState.queue_high_priority.mutex));
      pthread_mutex_lock(&(s_RadioRxState.queue_reg_priority.mutex));
      s_iRadioRxPausedInterfaces[iInterfaceIndex]++;
      _radio_rx_check_update_all_paused_flag();
      pthread_mutex_unlock(&(s_RadioRxState.queue_reg_priority.mutex));
      pthread_mutex_unlock(&(s_RadioRxState.queue_high_priority.mutex));

      _radio_rx_update_fd_sets();
      s_bHasPendingOperation = 0;
      s_bCanDoOperations = 0;
   }
   else
   {
      s_iRadioRxPausedInterfaces[iInterfaceIndex] = 1;
      _radio_rx_check_update_all_paused_flag();
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);

   char szRadioName[64];
   strcpy(szRadioName, "N/A");
   if ( NULL != pRadioHWInfo )
      strncpy(szRadioName, pRadioHWInfo->szName, sizeof(szRadioName)/sizeof(szRadioName[0]));

   char szBuff[128];
   szBuff[0] = 0;
   if ( NULL != szReason )
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), " (reason: %s)", szReason);

   log_line("[RadioRx] Pause Rx on radio interface %d, [%s] (+%d)%s", iInterfaceIndex+1, szRadioName, s_iRadioRxPausedInterfaces[iInterfaceIndex], szBuff);
}

void radio_rx_resume_interface(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   if ( s_iRadioRxInitialized )
   {
      s_bHasPendingOperation = 1;
      while ( ! s_bCanDoOperations )
         hardware_sleep_ms(1);

      pthread_mutex_lock(&(s_RadioRxState.queue_high_priority.mutex));
      pthread_mutex_lock(&(s_RadioRxState.queue_reg_priority.mutex));
      if ( s_iRadioRxPausedInterfaces[iInterfaceIndex] > 0 )
      {
         s_iRadioRxPausedInterfaces[iInterfaceIndex]--;
         if ( s_iRadioRxPausedInterfaces[iInterfaceIndex] == 0 )
            s_iRadioRxAllInterfacesPaused = 0;
      }
      pthread_mutex_unlock(&(s_RadioRxState.queue_reg_priority.mutex));
      pthread_mutex_unlock(&(s_RadioRxState.queue_high_priority.mutex));
      _radio_rx_update_fd_sets();
      s_bHasPendingOperation = 0;
      s_bCanDoOperations = 0;
   }
   else
   {
      s_iRadioRxPausedInterfaces[iInterfaceIndex] = 0;
      s_iRadioRxAllInterfacesPaused = 0;
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL != pRadioHWInfo )
      log_line("[RadioRx] Resumed Rx on radio interface %d, [%s] (+%d)", iInterfaceIndex+1, pRadioHWInfo->szName, s_iRadioRxPausedInterfaces[iInterfaceIndex]);
   else
      log_line("[RadioRx] Resumed Rx on radio interface %d, [%s] (+%d)", iInterfaceIndex+1, "N/A", s_iRadioRxPausedInterfaces[iInterfaceIndex]);
}

void radio_rx_mark_quit()
{
   s_iRadioRxMarkedForQuit = 1;
}

void radio_rx_set_dev_mode()
{
   s_iRadioRxDevMode = 1;
   log_line("[RadioRx] Set dev mode");
}

// Pointers to array of int-s (max radio cards, for each card)
void radio_rx_set_packet_counter_output(u8* pCounterOutputVideo, u8* pCounterOutputECVideo, u8* pCounterOutputRetrVideo, u8* pCounterOutputData, u8* pCounterMissingPackets, u8* pCounterMissingPacketsMaxGap)
{
   s_pPacketsCounterOutputVideo = pCounterOutputVideo;
   s_pPacketsCounterOutputECVideo = pCounterOutputECVideo;
   s_pPacketsCounterOutputRetrVideo = pCounterOutputRetrVideo;
   s_pPacketsCounterOutputData = pCounterOutputData;
   s_pPacketsCounterOutputMissing = pCounterMissingPackets;
   s_pPacketsCounterOutputMissingMaxGap = pCounterMissingPacketsMaxGap;
}

int radio_rx_detect_firmware_type_from_packet(u8* pPacketBuffer, int nPacketLength)
{
   if ( (NULL == pPacketBuffer) || (nPacketLength < 4) )
      return 0;

   if ( nPacketLength >= sizeof(t_packet_header) )
   {
      t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
      if ( pPH->total_length > nPacketLength )
      {
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_HAS_ENCRYPTION )
         {
            int dx = sizeof(t_packet_header) - sizeof(u32) - sizeof(u32);
            int l = nPacketLength-dx;
            dpp(pPacketBuffer + dx, l);
         }
         u32 uCRC = 0;
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
            uCRC = base_compute_crc32(pPacketBuffer+sizeof(u32), sizeof(t_packet_header)-sizeof(u32));
         else
            uCRC = base_compute_crc32(pPacketBuffer+sizeof(u32), pPH->total_length-sizeof(u32));

         if ( (uCRC & 0x00FFFFFF) == (pPH->uCRC & 0x00FFFFFF) )
            return MODEL_FIRMWARE_TYPE_RUBY;
      }
   }

   return 0;
}

int radio_rx_any_interface_broken()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( s_RadioRxState.iRadioInterfacesBroken[i] )
          return i+1;

   return 0;
}

int radio_rx_any_interface_bad_packets()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( s_RadioRxState.iRadioInterfacesRxBadPackets[i] )
          return i+1;

   return 0;
}

int radio_rx_get_interface_bad_packets_error_and_reset(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;
   int iError = s_RadioRxState.iRadioInterfacesRxBadPackets[iInterfaceIndex];
   s_RadioRxState.iRadioInterfacesRxBadPackets[iInterfaceIndex] = 0;
   return iError;
}

int radio_rx_any_rx_timeouts()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( s_RadioRxState.iRadioInterfacesRxTimeouts[i] > 0 )
          return i+1;
   return 0;
}

int radio_rx_get_timeout_count_and_reset(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;
   int iCount = s_RadioRxState.iRadioInterfacesRxTimeouts[iInterfaceIndex];
   s_RadioRxState.iRadioInterfacesRxTimeouts[iInterfaceIndex] = 0;
   return iCount;
}

void radio_rx_reset_interfaces_broken_state()
{
   log_line("[RadioRx] Reset broken state for all interface. Mark all interfaces as not broken.");
   
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      s_RadioRxState.iRadioInterfacesBroken[i] = 0;
      s_RadioRxState.iRadioInterfacesRxTimeouts[i] = 0;
      s_RadioRxState.iRadioInterfacesRxBadPackets[i] = 0;
   }
}

u32 radio_rx_get_and_reset_max_loop_time()
{
   u32 u = s_RadioRxState.uMaxLoopTime;
   s_RadioRxState.uMaxLoopTime = 0;
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


u8* _radio_rx_wait_get_queue_packet(t_radio_rx_state_packets_queue* pQueue, u32 uTimeoutMicroSec, int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex)
{
   int iRes = -1;

   if ( 0 == uTimeoutMicroSec )
   {
      iRes = sem_trywait(pQueue->pSemaphore);
   }
   else
   {
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_nsec += 1000*uTimeoutMicroSec;
      iRes = sem_timedwait(pQueue->pSemaphore, &ts);
   }
   if ( 0 != iRes )
      return NULL;

   int iIndexToCopy = 0;
   pthread_mutex_lock(&(pQueue->mutex));

   iIndexToCopy = pQueue->iCurrentRxPacketToConsume;
   if ( NULL != pLength )
      *pLength = pQueue->iPacketsLengths[pQueue->iCurrentRxPacketToConsume];
   if ( NULL != pIsShortPacket )
      *pIsShortPacket = pQueue->uPacketsAreShort[pQueue->iCurrentRxPacketToConsume];
   if ( NULL != pRadioInterfaceIndex )
      *pRadioInterfaceIndex = pQueue->uPacketsRxInterface[pQueue->iCurrentRxPacketToConsume];

   // Defer copy after mutex unlock
   //memcpy(s_tmpLastProcessedRadioRxPacket, pQueue->pPacketsBuffers[pQueue->iCurrentRxPacketToConsume], pQueue->iPacketsLengths[pQueue->iCurrentRxPacketToConsume]);

   pQueue->iCurrentRxPacketToConsume++;
   if ( pQueue->iCurrentRxPacketToConsume >= MAX_RX_PACKETS_QUEUE )
      pQueue->iCurrentRxPacketToConsume = 0;
   pQueue->iCurrentRxPacketsInQueue--;
   pthread_mutex_unlock(&(pQueue->mutex));

   memcpy(s_tmpLastProcessedRadioRxPacket, pQueue->pPacketsBuffers[iIndexToCopy], pQueue->iPacketsLengths[iIndexToCopy]);

   return s_tmpLastProcessedRadioRxPacket;
}

u8* radio_rx_wait_get_next_received_high_prio_packet(u32 uTimeoutMicroSec, int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex)
{
   if ( NULL != pLength )
      *pLength = 0;
   if ( NULL != pIsShortPacket )
      *pIsShortPacket = 0;
   if ( NULL != pRadioInterfaceIndex )
      *pRadioInterfaceIndex = 0;
   if ( 0 == s_iRadioRxInitialized )
      return NULL;

   if ( NULL == s_RadioRxState.queue_high_priority.pSemaphore )
   if ( s_RadioRxState.queue_high_priority.iCurrentRxPacketIndex == s_RadioRxState.queue_high_priority.iCurrentRxPacketToConsume )
      return 0;

   return _radio_rx_wait_get_queue_packet(&(s_RadioRxState.queue_high_priority), uTimeoutMicroSec, pLength, pIsShortPacket, pRadioInterfaceIndex);
}

u8* radio_rx_wait_get_next_received_reg_prio_packet(u32 uTimeoutMicroSec, int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex)
{
   if ( NULL != pLength )
      *pLength = 0;
   if ( NULL != pIsShortPacket )
      *pIsShortPacket = 0;
   if ( NULL != pRadioInterfaceIndex )
      *pRadioInterfaceIndex = 0;
   if ( 0 == s_iRadioRxInitialized )
      return NULL;

   if ( NULL == s_RadioRxState.queue_reg_priority.pSemaphore )
   if ( s_RadioRxState.queue_reg_priority.iCurrentRxPacketIndex == s_RadioRxState.queue_reg_priority.iCurrentRxPacketToConsume )
      return 0;

   return _radio_rx_wait_get_queue_packet(&(s_RadioRxState.queue_reg_priority), uTimeoutMicroSec, pLength, pIsShortPacket, pRadioInterfaceIndex);
}
