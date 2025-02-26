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

#include "../base/base.h"
#include "../base/encr.h"
#include "../base/config_hw.h"
#include "../base/hw_procs.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "radio_rx.h"
#include "radiolink.h"
#include "radio_duplicate_det.h"
#include <poll.h>

int s_iRadioRxInitialized = 0;
int s_iRadioRxSingalStop = 0;
int s_iRadioRxDevMode = 0;
int s_iRadioRxLoopTimeoutInterval = 60;
int s_iRadioRxMarkedForQuit = 0;
int s_iCurrentRxThreadPriority = -1;
int s_iPendingRxThreadPriority = -1;

t_radio_rx_state s_RadioRxState;
pthread_t s_pThreadRadioRx;

shared_mem_radio_stats* s_pSMRadioStats = NULL;
int s_iSearchMode = 0;
u32 s_uRadioRxTimeNow = 0;
u32 s_uRadioRxMaxTimeRead = 0;
u32 s_uRadioRxLastTimeQueue = 0;
u32 s_uRadioRxLastReceivedPacket =0;

u32 s_uRadioRxLoopTimeMin = 10000;
u32 s_uRadioRxLoopTimeMax = 0;
u32 s_uRadioRxLoopTimeAvg = 2;
u32 s_uRadioRxLoopTimeSpikesAvg = 10;
u32 s_uRadioRxLoopSpikesCount = 0;
#define MAX_SPIKES_TO_LOG 10
u32 s_uRadioRxLoopLastSpikesTimes[MAX_SPIKES_TO_LOG];
u32 s_uRadioRxLoopLastSpikesRxPackets[MAX_SPIKES_TO_LOG];
u32 s_uRadioRxLoopLastSpikesIndex = 0;

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
u8* s_pPacketsCounterOutputHighPriority = NULL;
u8* s_pPacketsCounterOutputData = NULL;
u8* s_pPacketsCounterOutputMissing = NULL;
u8* s_pPacketsCounterOutputMissingMaxGap = NULL;
u8* s_pRxAirGapTracking = NULL;

extern pthread_mutex_t s_pMutexRadioSyncRxTxThreads;
extern int s_iMutexRadioSyncRxTxThreadsInitialized;

volatile int s_bHasPendingOperation = 0;
volatile int s_bCanDoOperations = 0;

extern u32 s_uLastRadioPingSentTime;
extern u8 s_uLastRadioPingId;

t_radio_rx_state_vehicle* _radio_rx_get_stats_structure_for_vehicle(u32 uVehicleId)
{
   t_radio_rx_state_vehicle* pStatsVehicle = NULL;

   //----------------------------------------------
   // Begin: Compute index of stats to use

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
   {
      pStatsVehicle = &(s_RadioRxState.vehicles[MAX_CONCURENT_VEHICLES-1]);
      log_softerror_and_alarm("[RadioRx] No more room in vehicles runtime info structure for new vehicle VID %u. Reuse last index: %d", uVehicleId, MAX_CONCURENT_VEHICLES-1);
      char szTmp[256];
      szTmp[0] = 0;
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         char szT[32];
         sprintf(szT, "%u", s_RadioRxState.vehicles[i].uVehicleId);
         if ( 0 != i )
            strcat(szTmp, ", ");
         strcat(szTmp, szT);
      }
      log_softerror_and_alarm("[RadioRx] Current vehicles in vehicles runtime info: [%s]", szTmp);  
   }

   pStatsVehicle->uVehicleId = uVehicleId;

   // End: Compute index of stats to use
   //----------------------------------------------
   return pStatsVehicle;
}

// returns the number of missing packets detected on the radio link
int _radio_rx_update_local_stats_on_new_radio_packet(int iInterface, int iIsShortPacket, u32 uVehicleId, u8* pPacket, int iLength, int iDataIsOk)
{
   if ( (NULL == pPacket) || ( iLength <= 2 ) )
      return 0;

   int nReturnLost = 0;

   t_radio_rx_state_vehicle* pStatsVehicle = _radio_rx_get_stats_structure_for_vehicle(uVehicleId);

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
      
      // Check for lost packets
      if ( pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] > 0 )
      if ( pPH->radio_link_packet_index > pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface]+1 )
      {
         u32 lost = pPH->radio_link_packet_index - (pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] + 1);
         pStatsVehicle->uTotalRxPacketsLost += lost;
         pStatsVehicle->uTmpRxPacketsLost += lost;
         nReturnLost = lost;
      }

      // Check for restarted on the other end
      //if ( pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] > 10 )
      //if ( pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface] < 0xFFFF-10 ) // pPH->radio_link_packet_index is 16 bits
      //if ( pPH->radio_link_packet_index < pStatsVehicle->uLastRxRadioLinkPacketIndex[iInterface]-10 )
      //    radio_dup_detection_set_vehicle_restarted_flag(pPH->vehicle_id_src);

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
   s_iRadioRxReadTimeInterval.tv_usec = 50000; // 50 milisec timeout

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



u8* _radio_rx_wait_get_queue_packet(t_radio_rx_state_packets_queue* pQueue, int iHighPriorityQueue, u32 uTimeoutMicroSec, int* pLength, int* pIsShortPacket, int* pRadioInterfaceIndex)
{
   int iRes = -1;

   if ( 0 == uTimeoutMicroSec )
   {
      iRes = sem_trywait(pQueue->pSemaphoreRead);
   }
   else
   {
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_nsec += 1000LL*(long long)uTimeoutMicroSec;
      iRes = sem_timedwait(pQueue->pSemaphoreRead, &ts);
   }
   if ( 0 != iRes )
      return NULL;

   int iIndexToCopy = 0;

   iIndexToCopy = pQueue->iCurrentPacketIndexToConsume;
   if ( (iIndexToCopy < 0) || (iIndexToCopy >= pQueue->iQueueSize) )
      return NULL;
   if ( (pQueue->iPacketsLengths[iIndexToCopy] <= 0) || (pQueue->iPacketsLengths[iIndexToCopy] > MAX_PACKET_TOTAL_SIZE) )
      return NULL;
   if ( NULL == pQueue->pPacketsBuffers[iIndexToCopy] )
      return NULL;

   if ( NULL != pLength )
      *pLength = pQueue->iPacketsLengths[iIndexToCopy];
   if ( NULL != pIsShortPacket )
      *pIsShortPacket = pQueue->uPacketsAreShort[iIndexToCopy];
   if ( NULL != pRadioInterfaceIndex )
      *pRadioInterfaceIndex = pQueue->uPacketsRxInterface[iIndexToCopy];

   memcpy(s_tmpLastProcessedRadioRxPacket, pQueue->pPacketsBuffers[iIndexToCopy], pQueue->iPacketsLengths[iIndexToCopy]);

   pQueue->iCurrentPacketIndexToConsume++;
   if ( pQueue->iCurrentPacketIndexToConsume >= pQueue->iQueueSize )
      pQueue->iCurrentPacketIndexToConsume = 0;

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

   return _radio_rx_wait_get_queue_packet(&(s_RadioRxState.queue_high_priority), 1, uTimeoutMicroSec, pLength, pIsShortPacket, pRadioInterfaceIndex);
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

   return _radio_rx_wait_get_queue_packet(&(s_RadioRxState.queue_reg_priority), 0, uTimeoutMicroSec, pLength, pIsShortPacket, pRadioInterfaceIndex);
}

void _radio_rx_add_packet_to_rx_queue(u8* pPacket, int iLength, int iRadioInterface)
{
   if ( (NULL == pPacket) || (iLength <= 0) || s_iRadioRxMarkedForQuit )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   u8 uPacketFlags = pPH->packet_flags;
   u8 uPacketType = pPH->packet_type;

   t_radio_rx_state_packets_queue* pQueue = &s_RadioRxState.queue_reg_priority;
   if ( radio_packet_type_is_high_priority(uPacketFlags, uPacketType) )
      pQueue = &s_RadioRxState.queue_high_priority;

   // No more room? Discard it
   if ( ((pQueue->iCurrentPacketIndexToWrite+1) % pQueue->iQueueSize) == (pQueue->iCurrentPacketIndexToConsume % pQueue->iQueueSize) )
   {
      //s_uRadioRxLastTimeQueue += get_current_timestamp_ms() - s_uRadioRxTimeNow;
      return;
   }

   // Add the packet to the queue
   pQueue->uPacketsRxInterface[pQueue->iCurrentPacketIndexToWrite] = iRadioInterface;
   pQueue->uPacketsAreShort[pQueue->iCurrentPacketIndexToWrite] = 0;
   pQueue->iPacketsLengths[pQueue->iCurrentPacketIndexToWrite] = iLength;
   memcpy(pQueue->pPacketsBuffers[pQueue->iCurrentPacketIndexToWrite], pPacket, iLength);
      
   pQueue->iCurrentPacketIndexToWrite++;
   if ( pQueue->iCurrentPacketIndexToWrite >= pQueue->iQueueSize )
      pQueue->iCurrentPacketIndexToWrite = 0;

   int iCountPackets = pQueue->iCurrentPacketIndexToWrite - pQueue->iCurrentPacketIndexToConsume;
   if ( pQueue->iCurrentPacketIndexToWrite < pQueue->iCurrentPacketIndexToConsume )
      iCountPackets = pQueue->iCurrentPacketIndexToWrite + (pQueue->iQueueSize - pQueue->iCurrentPacketIndexToConsume);

   if ( iCountPackets > pQueue->iStatsMaxPacketsInQueueLastMinute )
      pQueue->iStatsMaxPacketsInQueueLastMinute = iCountPackets;
   if ( iCountPackets > pQueue->iStatsMaxPacketsInQueue )
      pQueue->iStatsMaxPacketsInQueue = iCountPackets;

   if ( NULL != pQueue->pSemaphoreWrite )
   {
      if ( 0 != sem_post(pQueue->pSemaphoreWrite) )
         log_softerror_and_alarm("Failed to set semaphore for packet ready.");
   }
   //s_uRadioRxLastTimeQueue += get_current_timestamp_ms() - s_uRadioRxTimeNow;
}

void _radio_rx_check_add_packet_to_rx_queue(u8* pPacket, int iLength, int iRadioInterfaceIndex)
{
   if ( radio_dup_detection_is_duplicate_on_stream(iRadioInterfaceIndex, pPacket, iLength, s_uRadioRxTimeNow) )
      return;

   if ( NULL != s_pSMRadioStats )
     radio_stats_update_on_unique_packet_received(s_pSMRadioStats, s_uRadioRxTimeNow, iRadioInterfaceIndex, pPacket, iLength);

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
      radio_stats_update_on_new_radio_packet_received(s_pSMRadioStats, s_uRadioRxTimeNow, iInterfaceIndex, pPacketBuffer, iPacketLength, 1, 1);
   
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
            radio_stats_set_bad_data_on_current_rx_interval(s_pSMRadioStats, iInterfaceIndex);
         }
         return 0;
      }

      if ( iPacketPos > 0 )
      {
         _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 1, s_uLastRxShortPacketsVehicleIds[iInterfaceIndex], s_uBuffersSerialMessages[iInterfaceIndex], iPacketPos, 0);
         radio_stats_set_bad_data_on_current_rx_interval(s_pSMRadioStats, iInterfaceIndex);
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

int _radio_rx_parse_received_wifi_radio_data(int iInterfaceIndex, int iMaxReads)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;

   int iReturn = 0;
   int iDataIsOk = 1;
   int iBufferLength = 0;
   u8* pPacketBuffer = NULL;
   int iCountParsed = 0;

   //static int sdebugCountParser = 0;
   //sdebugCountParser++;

   for( int iCountReads=0; iCountReads<iMaxReads; iCountReads++ )
   {
      iBufferLength = 0;
      pPacketBuffer = radio_process_wlan_data_in(iInterfaceIndex, &iBufferLength);
      if ( NULL == pPacketBuffer )
         break;

      if ( iBufferLength <= 0 )
      {
         log_softerror_and_alarm("[RadioRxThread] Rx cap returned an empty buffer (%d length) on radio interface index %d.", iBufferLength, iInterfaceIndex+1);
         iDataIsOk = 0;
         iReturn = -1;
         break;
      }

      iCountParsed++;
      iDataIsOk = 1;
      t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
      u8 uPacketFlags = pPH->packet_flags;
      u8 uPacketType = pPH->packet_type;
      u32 uVehicleId = pPH->vehicle_id_src;
   
      if ( s_iRadioRxDevMode )
      if ( uPacketType == PACKET_TYPE_RUBY_PING_CLOCK )
      {
         s_uLastRadioPingSentTime = get_current_timestamp_ms();
         s_uLastRadioPingId = *(pPacketBuffer +sizeof(t_packet_header));
      }

      if ( radio_packet_type_is_high_priority(uPacketFlags, uPacketType) )
      {
         if ( NULL != s_pPacketsCounterOutputHighPriority )
            s_pPacketsCounterOutputHighPriority[iInterfaceIndex]++;
      }
      else
      {      
         if ( NULL != s_pPacketsCounterOutputData )
            s_pPacketsCounterOutputData[iInterfaceIndex]++;
      }
      // To fix
      /*
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
      {
         t_packet_header_video_full_77* pPHVF = (t_packet_header_video_full_77*) (pPacketBuffer+sizeof(t_packet_header));    
         if ( ! (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
         if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
         if ( pPHVF->video_block_packet_index < pPHVF->block_packets)
         {
            u8* pExtraData = pPacketBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_77) + pPHVF->video_data_length;
            u32* pExtraDataU32 = (u32*)pExtraData;
            pExtraDataU32[4] = get_current_timestamp_ms();
         }
      }
      */
      int bCRCOk = 0;
      int iPacketLength = packet_process_and_check(iInterfaceIndex, pPacketBuffer, iBufferLength, &bCRCOk);

      if ( iPacketLength <= 0 )
      {
         log_softerror_and_alarm("[RadioRxThread] Process and check packet of %d bytes failed, error: %d", iBufferLength, get_last_processing_error_code());
         iDataIsOk = 0;
         s_RadioRxState.iRadioInterfacesRxBadPackets[iInterfaceIndex] = get_last_processing_error_code();
         continue;
      }

      if ( ! bCRCOk )
      {
         log_softerror_and_alarm("[RadioRxThread] Received broken packet (wrong CRC) on radio interface %d. Packet size: %d bytes, type: %s",
            iInterfaceIndex+1, pPH->total_length, str_get_packet_type(pPH->packet_type));
         iDataIsOk = 0;
         continue;
      }

      _radio_rx_check_add_packet_to_rx_queue(pPacketBuffer, iPacketLength, iInterfaceIndex);
    
      if ( NULL != s_pRxAirGapTracking )
      {
         s_uRadioRxTimeNow = get_current_timestamp_ms();
         u32 uGap = s_uRadioRxTimeNow - s_uRadioRxLastReceivedPacket;
         if ( uGap > 255 )
            uGap = 255;
         if ( uGap > *s_pRxAirGapTracking )
            *s_pRxAirGapTracking = uGap;
         s_uRadioRxLastReceivedPacket = s_uRadioRxTimeNow;
      }

      int nLost = _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 0, uVehicleId, pPacketBuffer, iBufferLength, iDataIsOk);
      if ( nLost > 0 )
      {
         if ( NULL != s_pPacketsCounterOutputMissing)
            s_pPacketsCounterOutputMissing[iInterfaceIndex]++;
         if ( NULL != s_pPacketsCounterOutputMissingMaxGap )
         if ( nLost > s_pPacketsCounterOutputMissingMaxGap[iInterfaceIndex] )
            s_pPacketsCounterOutputMissingMaxGap[iInterfaceIndex] = (u8)nLost;
      }
      if ( NULL != s_pSMRadioStats )
         radio_stats_update_on_new_radio_packet_received(s_pSMRadioStats, s_uRadioRxTimeNow, iInterfaceIndex, pPacketBuffer, iBufferLength, 0, iDataIsOk);
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
   if ( uTimeNow >= s_RadioRxState.uTimeLastMinuteStatsUpdate + 1000 * 20 )
   {
      s_RadioRxState.uTimeLastMinuteStatsUpdate = uTimeNow;
      s_iCounterRadioRxStatsUpdate2++;

      radio_duplicate_detection_log_info();

      log_line("[RadioRxThread] Max packets in queues (high/reg prio): %d/%d. Max packets in queue in last 10 sec: %d/%d",
         s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueue,
         s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueue,
         s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueueLastMinute,
         s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueueLastMinute);
      s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueueLastMinute = 0;
      s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueueLastMinute = 0;

      int iCountPacketsHigh = s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite - s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume;
      if ( s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite < s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume )
         iCountPacketsHigh = s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite + (s_RadioRxState.queue_high_priority.iQueueSize - s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume);

      int iCountPacketsReg = s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite - s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume;
      if ( s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite < s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume )
         iCountPacketsReg = s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite + (s_RadioRxState.queue_reg_priority.iQueueSize - s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume);

      log_line("[RadioRxThread] Packets in queues now pending consumption (high/reg prio): %d/%d",
         iCountPacketsHigh, iCountPacketsReg);

      if ( (s_iCounterRadioRxStatsUpdate2 % 10) == 0 )
      {
         log_line("[RadioRxThread] Reset max stats.");
         s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueue = 0;
         s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueue = 0;
      }

      //log_line("[RadioRxThread] Min/Max/Avg/Spikes-Avg (+count) radio rx loop times: %u / %u / %u / %u (%u times) ms", s_uRadioRxLoopTimeMin, s_uRadioRxLoopTimeMax, s_uRadioRxLoopTimeAvg, s_uRadioRxLoopTimeSpikesAvg, s_uRadioRxLoopSpikesCount);
      /*
      char szLog[256];
      szLog[0] = 0;
      int iIndex = s_uRadioRxLoopLastSpikesIndex;
      for ( int i=0; i<MAX_SPIKES_TO_LOG; i++ )
      {
         if ( i != 0 )
            strcat(szLog, ", ");
         char szTmp[32];
         iIndex--;
         if ( iIndex < 0 )
            iIndex = MAX_SPIKES_TO_LOG-1;
         sprintf(szTmp, "(-%d)=(%d pkts, %u ms)", i+1, s_uRadioRxLoopLastSpikesRxPackets[iIndex], s_uRadioRxLoopLastSpikesTimes[iIndex]);
         strcat(szLog, szTmp);
      }
      log_line("[RadioRxThread] Last spikes: [%s]", szLog);
      */
   }
}

void * _thread_radio_rx(void *argument)
{
   log_line("[RadioRxThread] Started.");

   if ( s_iPendingRxThreadPriority > 0 )
   if ( s_iPendingRxThreadPriority != s_iCurrentRxThreadPriority )
   {
      hw_increase_current_thread_priority("[RadioRxThread]", s_iPendingRxThreadPriority);
      s_iCurrentRxThreadPriority = s_iPendingRxThreadPriority;
   }

   for( int i=0; i<MAX_SPIKES_TO_LOG; i++ )
   {
      s_uRadioRxLoopLastSpikesTimes[i] = 0;
      s_uRadioRxLoopLastSpikesRxPackets[i] = 0;
   }
   log_line("[RadioRxThread] Initialized State. Waiting for rx messages...");

   int* piQuit = (int*) argument;
   int iPollTimeoutMs = 10;
   int iLoopCounter = 0;
   int iLoopParsedPackets = 0;
   int iLoopErrorsCounter = 0;
   u32 uTimeLastLoopCheck = get_current_timestamp_ms();
   u32 uTimeReadSignaled = 0;
   u32 uTimeNow = 0;

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
         hardware_sleep_ms(5);
         continue;
      }

      if ( s_bHasPendingOperation )
      {
         s_bCanDoOperations = 1;
         hardware_sleep_ms(1);
         continue;
      }
      
      s_bCanDoOperations = 0;
      uTimeNow = s_uRadioRxTimeNow = get_current_timestamp_ms();

      u32 uDeltaTime = uTimeNow - uTimeLastLoopCheck;

      if ( uDeltaTime < s_uRadioRxLoopTimeMin )
         s_uRadioRxLoopTimeMin = uDeltaTime;
      if ( uDeltaTime > s_uRadioRxLoopTimeMax )
         s_uRadioRxLoopTimeMax = uDeltaTime;
      s_uRadioRxLoopTimeAvg = (s_uRadioRxLoopTimeAvg * 99 + uDeltaTime)/100;
      
      if ( (uDeltaTime >= iPollTimeoutMs + 10) || (uTimeNow - uTimeReadSignaled > 5) )
      {
         iLoopErrorsCounter++;
         if ( (iLoopErrorsCounter % 20) == 1 )
            log_softerror_and_alarm("ERROR Rx loop (count %d) took %u ms (bf-read: %u + aft-read: %u)", iLoopErrorsCounter, uDeltaTime, uTimeReadSignaled - uTimeLastLoopCheck, uTimeNow - uTimeReadSignaled);
      }
      else
      {
         if ( iLoopErrorsCounter > 1 )
            log_softerror_and_alarm("ERROR Rx loop (lcount %d) took %u ms (bf-read: %u + aft-read: %u)", iLoopErrorsCounter, uDeltaTime, uTimeReadSignaled - uTimeLastLoopCheck, uTimeNow - uTimeReadSignaled);
         iLoopErrorsCounter = 0;
      }
      uTimeLastLoopCheck = uTimeNow;

      uDeltaTime = uTimeNow - uTimeReadSignaled;
      if ( (uDeltaTime >= 4) && (0 != uTimeReadSignaled) )
      {
         s_uRadioRxLoopSpikesCount++;
         s_uRadioRxLoopTimeSpikesAvg = (s_uRadioRxLoopTimeSpikesAvg*99 + uDeltaTime)/100;

         s_uRadioRxLoopLastSpikesTimes[s_uRadioRxLoopLastSpikesIndex] = uDeltaTime;
         s_uRadioRxLoopLastSpikesRxPackets[s_uRadioRxLoopLastSpikesIndex] = iLoopParsedPackets;
         s_uRadioRxLoopLastSpikesIndex++;
         if ( s_uRadioRxLoopLastSpikesIndex >= MAX_SPIKES_TO_LOG )
            s_uRadioRxLoopLastSpikesIndex = 0;

      }

      // Loop is executed every 10 ms max. So check and update stats about every 250 ms max

      if ( 0 == (iLoopCounter % 20) )
      {
         _radio_rx_update_stats(uTimeNow);
         if ( s_iPendingRxThreadPriority != s_iCurrentRxThreadPriority )
         {
            log_line("[RadioRxThread] New thread priority must be set, from %d to %d.", s_iCurrentRxThreadPriority, s_iPendingRxThreadPriority);
            s_iCurrentRxThreadPriority = s_iPendingRxThreadPriority;

            if ( s_iPendingRxThreadPriority > 0 )
               hw_increase_current_thread_priority("[RadioRxThread]", s_iPendingRxThreadPriority);
            else
               hw_increase_current_thread_priority("[RadioRxThread]", 0);
         }
      }

      iLoopParsedPackets = 0;
      struct pollfd fds[MAX_RADIO_INTERFACES];
      s_iRadioRxCountFDs = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
            continue;
         if ( s_RadioRxState.iRadioInterfacesBroken[i] )
            continue;
         if ( s_iRadioRxPausedInterfaces[i] )
            continue;
         fds[s_iRadioRxCountFDs].fd = pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd;
         fds[s_iRadioRxCountFDs].events = POLLIN;
         s_iRadioRxCountFDs++;
      }

      if ( s_iRadioRxCountFDs <= 0 )
      {
         hardware_sleep_ms(5);
         uTimeReadSignaled = get_current_timestamp_ms();
         continue;
      }

      //int nResult = select(s_iRadioRxMaxFD, &s_RadioRxReadSet, NULL, NULL, &s_iRadioRxReadTimeInterval);
      int nResult = poll(fds, s_iRadioRxCountFDs, iPollTimeoutMs);

      
      uTimeReadSignaled = get_current_timestamp_ms();
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
      {
         hardware_sleep_micros(500);
         continue;
      }

      // Received data, process it
      int iMaxRepeatCount = 3;
      int iMaxedInterface = -1;
      int iParsedPackets[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         iParsedPackets[i] = 1;

      // Repeat reading while we have max reads on at least one interface
      do
      {
         iMaxRepeatCount--;
         iMaxedInterface = -1;
         int kIndex = 0;
         for(int iInterfaceIndex=0; iInterfaceIndex<hardware_get_radio_interfaces_count(); iInterfaceIndex++)
         {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
            if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
               continue;
            if ( s_RadioRxState.iRadioInterfacesBroken[iInterfaceIndex] )
               continue;
            if ( s_iRadioRxPausedInterfaces[iInterfaceIndex] )
               continue;

            //if( 0 == FD_ISSET(pRadioHWInfo->runtimeInterfaceInfoRx.selectable_fd, &s_RadioRxReadSet) )
            //   continue;
            if ( 0 == (fds[kIndex].revents & POLLIN) )
               continue;
            kIndex++;

            if ( hardware_radio_index_is_serial_radio(iInterfaceIndex) )
            {
               int iResult = _radio_rx_parse_received_serial_radio_data(iInterfaceIndex);
               if ( iResult < 0 )
                  s_RadioRxState.iRadioInterfacesBroken[iInterfaceIndex] = 1;
            }
            else if ( iParsedPackets[iInterfaceIndex] > 0 )
            {
               iParsedPackets[iInterfaceIndex] = _radio_rx_parse_received_wifi_radio_data(iInterfaceIndex, 3);
               if ( (iParsedPackets[iInterfaceIndex] < 0) || ( radio_get_last_read_error_code() == RADIO_READ_ERROR_INTERFACE_BROKEN ) )
               {
                  log_line("[RadioRx] Mark interface %d as broken", iInterfaceIndex+1);
                  s_RadioRxState.iRadioInterfacesBroken[iInterfaceIndex] = 1;
                  continue;
               }
               iLoopParsedPackets += iParsedPackets[iInterfaceIndex];
               if ( iParsedPackets[iInterfaceIndex] >= 3 )
                  iMaxedInterface = iInterfaceIndex;
            }
         }
      } while ( (iMaxedInterface != -1) && (iMaxRepeatCount > 0));
   }

   log_line("[RadioRxThread] Stopped.");
   return NULL;
}

int radio_rx_start_rx_thread(shared_mem_radio_stats* pSMRadioStats, int iSearchMode, u32 uAcceptedFirmwareType)
{
   if ( s_iRadioRxInitialized )
      return 1;

   s_pSMRadioStats = pSMRadioStats;
   s_iSearchMode = iSearchMode;
   s_iRadioRxSingalStop = 0;
   s_RadioRxState.uAcceptedFirmwareType = uAcceptedFirmwareType;
   radio_rx_reset_interfaces_broken_state();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_iRadioRxPausedInterfaces[i] = 0;

   s_iRadioRxAllInterfacesPaused = 0;

   s_RadioRxState.queue_reg_priority.iQueueSize = MAX_RX_PACKETS_QUEUE;
   s_RadioRxState.queue_high_priority.iQueueSize = MAX_RX_PACKETS_QUEUE;

   for( int i=0; i<s_RadioRxState.queue_reg_priority.iQueueSize; i++ )
   {
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
   log_line("[RadioRx] Allocated %u bytes for %d rx packets (reg priority)", s_RadioRxState.queue_reg_priority.iQueueSize * MAX_PACKET_TOTAL_SIZE, s_RadioRxState.queue_reg_priority.iQueueSize);

   for( int i=0; i<s_RadioRxState.queue_high_priority.iQueueSize; i++ )
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
   }
   log_line("[RadioRx] Allocated %u bytes for %d rx packets (high priority)", s_RadioRxState.queue_high_priority.iQueueSize * MAX_PACKET_TOTAL_SIZE, s_RadioRxState.queue_high_priority.iQueueSize);

   s_RadioRxState.queue_high_priority.iCurrentPacketIndexToConsume = 0;
   s_RadioRxState.queue_high_priority.iCurrentPacketIndexToWrite = 0;
   s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToConsume = 0;
   s_RadioRxState.queue_reg_priority.iCurrentPacketIndexToWrite = 0;
   
   s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueue = 0;
   s_RadioRxState.queue_high_priority.iStatsMaxPacketsInQueueLastMinute = 0;
   s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueue = 0;
   s_RadioRxState.queue_reg_priority.iStatsMaxPacketsInQueueLastMinute = 0;

   s_RadioRxState.uTimeLastStatsUpdate = get_current_timestamp_ms();
   s_RadioRxState.uTimeLastMinuteStatsUpdate = get_current_timestamp_ms();
   
   sem_unlink(RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
   sem_unlink(RUBY_SEM_RX_RADIO_REG_PRIORITY);

   s_RadioRxState.queue_high_priority.pSemaphoreWrite = sem_open(RUBY_SEM_RX_RADIO_HIGH_PRIORITY, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == s_RadioRxState.queue_high_priority.pSemaphoreWrite) || (SEM_FAILED == s_RadioRxState.queue_high_priority.pSemaphoreWrite) )
   {
      log_error_and_alarm("[RadioRx] Failed to create write semaphore: %s, try alternative.", RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
      s_RadioRxState.queue_high_priority.pSemaphoreWrite = sem_open(RUBY_SEM_RX_RADIO_HIGH_PRIORITY, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_RadioRxState.queue_high_priority.pSemaphoreWrite) || (SEM_FAILED == s_RadioRxState.queue_high_priority.pSemaphoreWrite) )
      {
         log_error_and_alarm("[RadioRx] Failed to create write semaphore: %s", RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
         return 0;
      }
   }
   s_RadioRxState.queue_high_priority.pSemaphoreRead = sem_open(RUBY_SEM_RX_RADIO_HIGH_PRIORITY, O_RDWR);
   if ( (NULL == s_RadioRxState.queue_high_priority.pSemaphoreRead) || (SEM_FAILED == s_RadioRxState.queue_high_priority.pSemaphoreRead) )
   {
      log_error_and_alarm("[RadioRx] Failed to create read semaphore: %s, try alternative.", RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
      s_RadioRxState.queue_high_priority.pSemaphoreRead = sem_open(RUBY_SEM_RX_RADIO_HIGH_PRIORITY, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_RadioRxState.queue_high_priority.pSemaphoreRead) || (SEM_FAILED == s_RadioRxState.queue_high_priority.pSemaphoreRead) )
      {
         log_error_and_alarm("[RadioRx] Failed to create read semaphore: %s", RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
         return 0;
      }
   }
   int iSemVal = 0;
   if ( 0 == sem_getvalue(s_RadioRxState.queue_high_priority.pSemaphoreRead, &iSemVal) )
      log_line("[RadioRx] High priority queue semaphore initial value: %d", iSemVal);
   else
      log_softerror_and_alarm("[RadioRx] Failed to get high priority queue sem value.");

   s_RadioRxState.queue_reg_priority.pSemaphoreWrite = sem_open(RUBY_SEM_RX_RADIO_REG_PRIORITY, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR, 0);
   if ( (NULL == s_RadioRxState.queue_reg_priority.pSemaphoreWrite) || (SEM_FAILED == s_RadioRxState.queue_reg_priority.pSemaphoreWrite) )
   {
      log_error_and_alarm("[RadioRx] Failed to create write semaphore: %s, try alternative.", RUBY_SEM_RX_RADIO_REG_PRIORITY);
      s_RadioRxState.queue_reg_priority.pSemaphoreWrite = sem_open(RUBY_SEM_RX_RADIO_REG_PRIORITY, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_RadioRxState.queue_reg_priority.pSemaphoreWrite) || (SEM_FAILED == s_RadioRxState.queue_reg_priority.pSemaphoreWrite) )
      {
         log_error_and_alarm("[RadioRx] Failed to create write semaphore: %s", RUBY_SEM_RX_RADIO_REG_PRIORITY);
         return 0;
      }
   }
   s_RadioRxState.queue_reg_priority.pSemaphoreRead = sem_open(RUBY_SEM_RX_RADIO_REG_PRIORITY, O_RDWR);
   if ( (NULL == s_RadioRxState.queue_reg_priority.pSemaphoreRead) || (SEM_FAILED == s_RadioRxState.queue_reg_priority.pSemaphoreRead) )
   {
      log_error_and_alarm("[RadioRx] Failed to create read semaphore: %s, try alternative", RUBY_SEM_RX_RADIO_REG_PRIORITY);
      s_RadioRxState.queue_reg_priority.pSemaphoreRead = sem_open(RUBY_SEM_RX_RADIO_REG_PRIORITY, O_CREAT, S_IWUSR | S_IRUSR, 0); 
      if ( (NULL == s_RadioRxState.queue_reg_priority.pSemaphoreRead) || (SEM_FAILED == s_RadioRxState.queue_reg_priority.pSemaphoreRead) )
      {
         log_error_and_alarm("[RadioRx] Failed to create read semaphore: %s", RUBY_SEM_RX_RADIO_REG_PRIORITY);
         return 0;
      }
   }

   iSemVal = 0;
   if ( 0 == sem_getvalue(s_RadioRxState.queue_reg_priority.pSemaphoreRead, &iSemVal) )
      log_line("[RadioRx] Reg priority queue semaphore initial value: %d", iSemVal);
   else
      log_softerror_and_alarm("[RadioRx] Failed to get reg priority queue sem value.");

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

   pthread_cancel(s_pThreadRadioRx);

   if ( NULL != s_RadioRxState.queue_high_priority.pSemaphoreWrite )
      sem_close(s_RadioRxState.queue_high_priority.pSemaphoreWrite);
   if ( NULL != s_RadioRxState.queue_high_priority.pSemaphoreRead )
      sem_close(s_RadioRxState.queue_high_priority.pSemaphoreRead);
   if ( NULL != s_RadioRxState.queue_reg_priority.pSemaphoreWrite )
      sem_close(s_RadioRxState.queue_reg_priority.pSemaphoreWrite);
   if ( NULL != s_RadioRxState.queue_reg_priority.pSemaphoreRead )
      sem_close(s_RadioRxState.queue_reg_priority.pSemaphoreRead);
   s_RadioRxState.queue_high_priority.pSemaphoreWrite = NULL;
   s_RadioRxState.queue_high_priority.pSemaphoreRead = NULL;
   s_RadioRxState.queue_reg_priority.pSemaphoreWrite = NULL;
   s_RadioRxState.queue_reg_priority.pSemaphoreRead = NULL;

   sem_unlink(RUBY_SEM_RX_RADIO_HIGH_PRIORITY);
   sem_unlink(RUBY_SEM_RX_RADIO_REG_PRIORITY);
}

void radio_rx_set_custom_thread_priority(int iPriority)
{
   s_iPendingRxThreadPriority = iPriority;
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

      s_iRadioRxPausedInterfaces[iInterfaceIndex]++;
      _radio_rx_check_update_all_paused_flag();

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

   log_line("[RadioRx] Pause Rx on radio interface %d, [%s] (paused %d times)%s", iInterfaceIndex+1, szRadioName, s_iRadioRxPausedInterfaces[iInterfaceIndex]+1, szBuff);
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

      if ( s_iRadioRxPausedInterfaces[iInterfaceIndex] > 0 )
      {
         s_iRadioRxPausedInterfaces[iInterfaceIndex]--;
         if ( s_iRadioRxPausedInterfaces[iInterfaceIndex] == 0 )
            s_iRadioRxAllInterfacesPaused = 0;
      }
      s_bHasPendingOperation = 0;
      s_bCanDoOperations = 0;
   }
   else
   {
      s_iRadioRxPausedInterfaces[iInterfaceIndex] = 0;
      s_iRadioRxAllInterfacesPaused = 0;
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( 0 == s_iRadioRxPausedInterfaces[iInterfaceIndex] )
   {
      if ( NULL != pRadioHWInfo )
         log_line("[RadioRx] Resumed Rx on radio interface %d, [%s]", iInterfaceIndex+1, pRadioHWInfo->szName);
      else
         log_line("[RadioRx] Resumed Rx on radio interface %d, [%s]", iInterfaceIndex+1, "N/A");
   }
   else
   {
      if ( NULL != pRadioHWInfo )
         log_line("[RadioRx] Tried resume Rx on radio interface %d, [%s], still paused %d times.", iInterfaceIndex+1, pRadioHWInfo->szName, s_iRadioRxPausedInterfaces[iInterfaceIndex]);
      else
         log_line("[RadioRx] Tried resume Rx on radio interface %d, [%s], still paused %d times", iInterfaceIndex+1, "N/A", s_iRadioRxPausedInterfaces[iInterfaceIndex]);    
   }
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
void radio_rx_set_packet_counter_output(u8* pCounterOutputHighPriority, u8* pCounterOutputData, u8* pCounterMissingPackets, u8* pCounterMissingPacketsMaxGap)
{
   s_pPacketsCounterOutputHighPriority = pCounterOutputHighPriority;
   s_pPacketsCounterOutputData = pCounterOutputData;
   s_pPacketsCounterOutputMissing = pCounterMissingPackets;
   s_pPacketsCounterOutputMissingMaxGap = pCounterMissingPacketsMaxGap;
}

void radio_rx_set_air_gap_track_output(u8* pCounterRxAirgap)
{
   s_pRxAirGapTracking = pCounterRxAirgap;
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
            int dx = sizeof(t_packet_header);
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
   u32 u = s_uRadioRxMaxTimeRead;
   s_uRadioRxMaxTimeRead = 0;
   return u;
}

u32 radio_rx_get_and_reset_max_loop_time_queue()
{
   u32 u = s_uRadioRxLastTimeQueue;
   s_uRadioRxLastTimeQueue = 0;
   return u;
}
