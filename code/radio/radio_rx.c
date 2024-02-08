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

#include "../base/base.h"
#include "../base/encr.h"
#include "../base/enc.h"
#include <pthread.h>
#include <sodium.h>
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "radio_rx.h"
#include "radiolink.h"
#include "radio_duplicate_det.h"

int s_iRadioRxInitialized = 0;
int s_iRadioRxSingalStop = 0;
int s_iRadioRxLoopTimeoutInterval = 15;
int s_iRadioRxMarkedForQuit = 0;

t_radio_rx_state s_RadioRxState;

pthread_t s_pThreadRadioRx;
pthread_mutex_t s_pThreadRadioRxMutex;
shared_mem_radio_stats* s_pSMRadioStats = NULL;
shared_mem_radio_stats_interfaces_rx_graph* s_pSMRadioRxGraphs = NULL;
static int s_iSodiumInited = 0;
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
   if ( (s_RadioRxState.vehicles[0].uVehicleId == 0) || (s_RadioRxState.vehicles[0].uVehicleId == uVehicleId)  )
      iStatsIndex = 0;

   if ( -1 == iStatsIndex )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( s_RadioRxState.vehicles[i].uVehicleId == uVehicleId )
         {
            iStatsIndex = i;
            break;
         }
      }

      if ( iStatsIndex == -1 )
      {
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            if ( s_RadioRxState.vehicles[i].uVehicleId == 0 )
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

   s_RadioRxState.vehicles[iStatsIndex].uVehicleId = uVehicleId;
   if ( 0 == s_RadioRxState.vehicles[iStatsIndex].uTotalRxPackets )
   {
      s_RadioRxState.uTimeLastStatsUpdate = s_uRadioRxTimeNow;
      s_RadioRxState.uTimeLastMinute = s_uRadioRxTimeNow;
   }

   if ( iDataIsOk )
   {
      s_RadioRxState.vehicles[iStatsIndex].uTotalRxPackets++;
      s_RadioRxState.vehicles[iStatsIndex].uTmpRxPackets++;
   }
   else
   {
      s_RadioRxState.vehicles[iStatsIndex].uTotalRxPacketsBad++;
      s_RadioRxState.vehicles[iStatsIndex].uTmpRxPacketsBad++;
   }

   if ( s_RadioRxState.uAcceptedFirmwareType == MODEL_FIRMWARE_TYPE_OPENIPC )
      return;

   if ( iIsShortPacket )
   {
      t_packet_header_short* pPHS = (t_packet_header_short*)pPacket;
      u32 uNext = ((s_RadioRxState.vehicles[iStatsIndex].uLastRxRadioLinkPacketIndex[iInterface]+1) & 0xFF);
      if ( pPHS->packet_id != uNext )
      {
         u32 lost = pPHS->packet_id - uNext;
         if ( pPHS->packet_id < uNext )
            lost = pPHS->packet_id + 255 - uNext;
         s_RadioRxState.vehicles[iStatsIndex].uTotalRxPacketsLost += lost;
         s_RadioRxState.vehicles[iStatsIndex].uTmpRxPacketsLost += lost;
      }

      s_RadioRxState.vehicles[iStatsIndex].uLastRxRadioLinkPacketIndex[iInterface] = pPHS->packet_id;
   }
   else
   {
      t_packet_header* pPH = (t_packet_header*)pPacket;
      if ( s_RadioRxState.vehicles[iStatsIndex].uLastRxRadioLinkPacketIndex[iInterface] > 0 )
      if ( pPH->radio_link_packet_index > s_RadioRxState.vehicles[iStatsIndex].uLastRxRadioLinkPacketIndex[iInterface]+1 )
      {
         u32 lost = pPH->radio_link_packet_index - (s_RadioRxState.vehicles[iStatsIndex].uLastRxRadioLinkPacketIndex[iInterface] + 1);
         s_RadioRxState.vehicles[iStatsIndex].uTotalRxPacketsLost += lost;
         s_RadioRxState.vehicles[iStatsIndex].uTmpRxPacketsLost += lost;
      }

      if ( s_RadioRxState.vehicles[iStatsIndex].uLastRxRadioLinkPacketIndex[iInterface] > 0 )
      if ( s_RadioRxState.vehicles[iStatsIndex].uLastRxRadioLinkPacketIndex[iInterface] < 0xFA00 )
      if ( pPH->radio_link_packet_index > 0x0500 )
      if ( pPH->radio_link_packet_index < s_RadioRxState.vehicles[iStatsIndex].uLastRxRadioLinkPacketIndex[iInterface] )
          radio_dup_detection_set_vehicle_restarted_flag(pPH->vehicle_id_src);

      s_RadioRxState.vehicles[iStatsIndex].uLastRxRadioLinkPacketIndex[iInterface] = pPH->radio_link_packet_index;
   }
}

void _radio_rx_add_packet_to_rx_queue(u8* pPacket, int iLength, int iRadioInterface)
{
   if ( (NULL == pPacket) || (iLength <= 0) || s_iRadioRxMarkedForQuit )
      return;

   // Add the packet to the queue
   s_RadioRxState.iPacketsRxInterface[s_RadioRxState.iCurrentRxPacketIndex] = iRadioInterface;
   s_RadioRxState.iPacketsAreShort[s_RadioRxState.iCurrentRxPacketIndex] = 0;
   s_RadioRxState.iPacketsLengths[s_RadioRxState.iCurrentRxPacketIndex] = iLength;
   memcpy(s_RadioRxState.pPacketsBuffers[s_RadioRxState.iCurrentRxPacketIndex], pPacket, iLength);
   

   int iPacketsInQueue = 0;
   if ( s_RadioRxState.iCurrentRxPacketIndex > s_RadioRxState.iCurrentRxPacketToConsume )
      iPacketsInQueue = s_RadioRxState.iCurrentRxPacketIndex - s_RadioRxState.iCurrentRxPacketToConsume;
   else if ( s_RadioRxState.iCurrentRxPacketIndex < s_RadioRxState.iCurrentRxPacketToConsume )
      iPacketsInQueue = MAX_RX_PACKETS_QUEUE - s_RadioRxState.iCurrentRxPacketToConsume + s_RadioRxState.iCurrentRxPacketIndex;
   
   if ( iPacketsInQueue > s_RadioRxState.iMaxPacketsInQueueLastMinute )
      s_RadioRxState.iMaxPacketsInQueueLastMinute = iPacketsInQueue;
   if ( iPacketsInQueue > s_RadioRxState.iMaxPacketsInQueue )
      s_RadioRxState.iMaxPacketsInQueue = iPacketsInQueue;

   u32 uTimeStartTryLock = get_current_timestamp_ms();
   int iLock = -1;
   while ( (iLock != 0 ) && (get_current_timestamp_ms() <= uTimeStartTryLock+5) )
   {
      iLock = pthread_mutex_trylock(&s_pThreadRadioRxMutex);
      hardware_sleep_micros(100);
   }

   if ( iLock != 0 )
      log_softerror_and_alarm("[RadioRxThread] Doing unguarded rx buffers manipulation");
   
   s_RadioRxState.iCurrentRxPacketIndex++;
   if ( s_RadioRxState.iCurrentRxPacketIndex >= MAX_RX_PACKETS_QUEUE )
      s_RadioRxState.iCurrentRxPacketIndex = 0;

   if ( s_RadioRxState.iCurrentRxPacketIndex == s_RadioRxState.iCurrentRxPacketToConsume )
   {
      // No more room. Must skip few pending processing packets
      log_softerror_and_alarm("[RadioRxThread] No more room in rx buffers. Discarding some unprocessed messages. Max messages in queue: %d, last 10 sec: %d.", s_RadioRxState.iMaxPacketsInQueue, s_RadioRxState.iMaxPacketsInQueueLastMinute);
      if ( iLock != 0 )
      {
         iLock = pthread_mutex_lock(&s_pThreadRadioRxMutex);
      }
      for( int i=0; i<3; i++ )
      {
         s_RadioRxState.iCurrentRxPacketToConsume++;
         if ( s_RadioRxState.iCurrentRxPacketToConsume >= MAX_RX_PACKETS_QUEUE )
            s_RadioRxState.iCurrentRxPacketToConsume = 0;
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

   int iRead = read(pRadioHWInfo->monitor_interface_read.selectable_fd, &(s_uBuffersSerialMessages[iInterfaceIndex][s_uBuffersSerialMessagesReadPos[iInterfaceIndex]]), iMaxRead);

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


// returns 0 if it should be discarded
// returns positive number: how many packets have been skipped - 1
int _radio_rx_parse_received_packet_wfbohd(int iInterfaceIndex, u8* pBuffer, int iBufferLength)
{
   if ( (NULL == pBuffer) || (iBufferLength < sizeof(type_wfb_block_header)) )
      return 0;

   // Allocate a vehicle stats index to use for WfbOpenIPC
   int iStatsIndex = -1;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( s_RadioRxState.vehicles[i].uVehicleId == 1 )
      {
         iStatsIndex = i;
         break;
      }
   }

   if ( iStatsIndex == -1 )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( s_RadioRxState.vehicles[i].uVehicleId == 0 )
         {
            s_RadioRxState.vehicles[i].uVehicleId = 1;
            iStatsIndex = i;
            break;
         }
      }
   }

   if ( -1 == iStatsIndex )
      return 0;

   if ( (*pBuffer) == PACKET_TYPE_WFB_KEY )
   {
      if ( iBufferLength != sizeof(type_wfb_session_header) + sizeof(type_wfb_session_data) + crypto_box_MACBYTES )
         return 0;
      type_wfb_session_data sessionData;
      if ( 0 != crypto_box_open_easy((uint8_t*)&sessionData,  pBuffer + sizeof(type_wfb_session_header),
                                sizeof(type_wfb_session_data) + crypto_box_MACBYTES,
                                ((type_wfb_session_header*)pBuffer)->sessionNonce,
                                s_RadioRxState.vehicles[iStatsIndex].tx_publickey, s_RadioRxState.vehicles[iStatsIndex].rx_secretkey) )
      {
         log_line("[RadioRx] Failed to initialize encryption for OpenIPC camera.");
         wfbohd_set_wrong_key_flag();
         return 0;
      }
      if ( 0 != memcmp(s_RadioRxState.vehicles[iStatsIndex].session_key, sessionData.sessionKey, sizeof(s_RadioRxState.vehicles[iStatsIndex].session_key)) )
      {
         memcpy(s_RadioRxState.vehicles[iStatsIndex].session_key, sessionData.sessionKey, sizeof(s_RadioRxState.vehicles[iStatsIndex].session_key));

         if ( NULL == s_RadioRxState.vehicles[iStatsIndex].pFEC )
            s_RadioRxState.vehicles[iStatsIndex].pFEC = zfec_new(sessionData.uK, sessionData.uN);
         else if ( (sessionData.uK != s_RadioRxState.vehicles[iStatsIndex].pFEC->k) || (sessionData.uN != s_RadioRxState.vehicles[iStatsIndex].pFEC->n) )
         {
            zfec_free(s_RadioRxState.vehicles[iStatsIndex].pFEC);
            s_RadioRxState.vehicles[iStatsIndex].pFEC = zfec_new(sessionData.uK, sessionData.uN); 
         }
         log_line("[RadioRx] Created new session for OpenIPC camera (%d/%d)", (int)sessionData.uK, (int)sessionData.uN);
      }
      return 0; 
   }

   if ( NULL == s_RadioRxState.vehicles[iStatsIndex].pFEC )
      return 0;
   if ( s_RadioRxState.vehicles[iStatsIndex].pFEC->k > s_RadioRxState.vehicles[iStatsIndex].pFEC->n )
      return 0;

   if ( (*pBuffer) != PACKET_TYPE_WFB_DATA )
      return 0;

   if ( iBufferLength < sizeof(type_wfb_block_header) + sizeof(type_wfb_packet_header) )
      return 0;

   uint8_t decrypted[2000];
   long long unsigned int decrypted_len;
   type_wfb_block_header *pBlockHeader = (type_wfb_block_header*)pBuffer;

   if ( 0 != crypto_aead_chacha20poly1305_decrypt(decrypted, &decrypted_len, NULL,
                                          pBuffer + sizeof(type_wfb_block_header), iBufferLength - sizeof(type_wfb_block_header),
                                          pBuffer,
                                          sizeof(type_wfb_block_header),
                                          (uint8_t*)(&(pBlockHeader->uDataNonce)), s_RadioRxState.vehicles[iStatsIndex].session_key) )
   {
      static u32 sl_uLastTimeErrorDecodingOpenIPCVideo = 0;
      if ( s_uRadioRxTimeNow > sl_uLastTimeErrorDecodingOpenIPCVideo+2000 )
      {
         sl_uLastTimeErrorDecodingOpenIPCVideo = s_uRadioRxTimeNow;
         log_line("[RadioRx] Failed to decode video packet from OpenIPC camera.");
      }
      wfbohd_set_wrong_key_flag();
      return 0;
   }
   if ( decrypted_len < sizeof(type_wfb_block_header) + sizeof(type_wfb_packet_header) )
      return 0;

   u32 uFreqKhz = 0;
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL != pRadioHWInfo )
      uFreqKhz = pRadioHWInfo->uCurrentFrequencyKhz;
 
   u8* pFakeVideoPacket = wfbohd_radio_on_received_video_packet(uFreqKhz, s_RadioRxState.vehicles[iStatsIndex].pFEC, pBlockHeader, decrypted, decrypted_len);

   if ( NULL != pFakeVideoPacket )
   {
      t_packet_header* pPH = (t_packet_header*)pFakeVideoPacket;
      t_packet_header_video_full_77* pPHVF = (t_packet_header_video_full_77*) (pFakeVideoPacket+sizeof(t_packet_header));    
      if ( NULL != s_pSMRadioStats )
         radio_stats_update_on_unique_packet_received(s_pSMRadioStats, s_pSMRadioRxGraphs, s_uRadioRxTimeNow, iInterfaceIndex, pFakeVideoPacket, pPH->total_length + pPHVF->video_data_length);
      _radio_rx_add_packet_to_rx_queue(pFakeVideoPacket, pPH->total_length, iInterfaceIndex);
   }

   u8* pTmpPacket = (u8*) radiopackets_wfbohd_generate_fake_ruby_telemetry_header(s_uRadioRxTimeNow, uFreqKhz);
   if ( NULL != pTmpPacket )
   {
      u8 packet[2000];
      int iLen = radiopackets_wfbohd_generate_fake_full_ruby_telemetry_packet(uFreqKhz, pTmpPacket, packet);
      if ( iLen == (sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v3)) )
      {
         if ( NULL != s_pSMRadioStats )
            radio_stats_update_on_unique_packet_received(s_pSMRadioStats, s_pSMRadioRxGraphs, s_uRadioRxTimeNow, iInterfaceIndex, packet, iLen);

         _radio_rx_add_packet_to_rx_queue(packet, iLen, iInterfaceIndex);
      }
   }

   u32 u = be64toh(pBlockHeader->uDataNonce);
   u32 uBlockIndex = u >> 8;
   u32 uFragmentIndex = u & 0xFF;

   static u32 s_uLastBlockReceived = 0;
   static u32 s_uLastFragmentReceived = 0;

   u32 uSkipped = 1;
   if ( uBlockIndex > s_uLastBlockReceived )
   {
      uSkipped += s_RadioRxState.vehicles[iStatsIndex].pFEC->n * (uBlockIndex - s_uLastBlockReceived - 1);
   }
   else if ( (uBlockIndex == s_uLastBlockReceived) && (uFragmentIndex > s_uLastFragmentReceived) )
   {
      uSkipped += uFragmentIndex - s_uLastFragmentReceived - 1;
   }

   s_uLastBlockReceived = uBlockIndex;
   s_uLastFragmentReceived = uFragmentIndex;
   return uSkipped;
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
         s_RadioRxState.iRadioInterfacesRxTimeouts[iInterfaceIndex]++;
      }
      else
      {
         log_softerror_and_alarm("[RadioRxThread] Rx pcap returned a NULL packet on radio interface %d.", iInterfaceIndex+1);
         if ( (s_RadioRxState.uAcceptedFirmwareType == MODEL_FIRMWARE_TYPE_OPENIPC) )
            iReturn = 0;
         else
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
   int iCountSkippedPackets = 0;

   if ( (s_RadioRxState.uAcceptedFirmwareType == MODEL_FIRMWARE_TYPE_OPENIPC) )
   {
      if ( iBufferLength <= 0 )
         return 0;
      int iRes = _radio_rx_parse_received_packet_wfbohd(iInterfaceIndex, pBuffer, iBufferLength);
      if ( iRes > 0 )
      {
         iIsVideoData = 1;
         iCountPackets++;
         iCountSkippedPackets = iRes - 1;
      }
   }
   else
   {
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
   }

   s_uRadioRxTimeNow = get_current_timestamp_ms();

   u32 uVehicleId = 0;
   if ( s_RadioRxState.uAcceptedFirmwareType == MODEL_FIRMWARE_TYPE_OPENIPC )
   {
      u32 uFreqKhz = 0;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
      if ( NULL != pRadioHWInfo )
         uFreqKhz = pRadioHWInfo->uCurrentFrequencyKhz;

      uVehicleId = wfbohd_radio_generate_vehicle_id(uFreqKhz);
   }
   else if ( NULL != pBuffer )
   {
      t_packet_header* pPH = (t_packet_header*)pBuffer;
      uVehicleId = pPH->vehicle_id_src;
   }

   _radio_rx_update_local_stats_on_new_radio_packet(iInterfaceIndex, 0, uVehicleId, pBuffer, iBufferLength, iDataIsOk);
   if ( NULL != s_pSMRadioStats )
   {
      if ( (s_RadioRxState.uAcceptedFirmwareType == MODEL_FIRMWARE_TYPE_OPENIPC) )
         radio_stats_update_on_new_wfbohd_radio_packet_received(s_pSMRadioStats, s_pSMRadioRxGraphs, s_uRadioRxTimeNow, iInterfaceIndex, pBuffer, iBufferLength, iIsVideoData, iDataIsOk, iCountSkippedPackets);
      else
         radio_stats_update_on_new_radio_packet_received(s_pSMRadioStats, s_pSMRadioRxGraphs, s_uRadioRxTimeNow, iInterfaceIndex, pBuffer, iBufferLength, 0, iIsVideoData, iDataIsOk);
   }
   return iReturn;
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

         if ( (s_iCounterRadioRxStatsUpdate % 10) == 0 )
         {
            log_line("[RadioRxThread] Received packets from VID %u: %u/sec (min: %d/sec, max: %d/sec)", 
               s_RadioRxState.vehicles[i].uVehicleId, s_RadioRxState.vehicles[i].uTmpRxPackets,
               s_RadioRxState.vehicles[i].iMinRxPacketsPerSec, s_RadioRxState.vehicles[i].iMaxRxPacketsPerSec);
            log_line("[RadioRxThread] Total recv packets from VID: %u: %u, bad: %u, lost: %u",
               s_RadioRxState.vehicles[i].uVehicleId, s_RadioRxState.vehicles[i].uTotalRxPackets, s_RadioRxState.vehicles[i].uTotalRxPacketsBad, s_RadioRxState.vehicles[i].uTotalRxPacketsLost );
         }

         s_RadioRxState.vehicles[i].uTmpRxPackets = 0;
         s_RadioRxState.vehicles[i].uTmpRxPacketsBad = 0;
         s_RadioRxState.vehicles[i].uTmpRxPacketsLost = 0;
      }

      if ( (s_iCounterRadioRxStatsUpdate % 10) == 0 )
      if ( 0 == iAnyRxPackets )
         log_line("[RadioRxThread] No packets received in the last seconds");
   }
   if ( uTimeNow >= s_RadioRxState.uTimeLastMinute + 1000 * 5 )
   {
      s_RadioRxState.uTimeLastMinute = uTimeNow;
      s_iCounterRadioRxStatsUpdate2++;
      log_line("[RadioRxThread] Max packets in queue: %d. Max packets in queue in last 10 sec: %d.",
         s_RadioRxState.iMaxPacketsInQueue, s_RadioRxState.iMaxPacketsInQueueLastMinute);
      s_RadioRxState.iMaxPacketsInQueueLastMinute = 0;

      radio_duplicate_detection_log_info();

      if ( (s_iCounterRadioRxStatsUpdate2 % 10) == 0 )
      {
         log_line("[RadioRxThread] Reset max stats.");
         s_RadioRxState.iMaxPacketsInQueue = 0;
      }
   }
}

static void * _thread_radio_rx(void *argument)
{
   log_line("[RadioRxThread] Started.");

   pthread_t this_thread = pthread_self();
   struct sched_param params;
   int policy = 0;
   int ret = 0;

   ret = pthread_getschedparam(this_thread, &policy, &params);
   if ( ret != 0 )
     log_softerror_and_alarm("[RadioRxThread] Failed to get schedule param");
   log_line("[RadioRxThread] Current thread policy/priority: %d/%d", policy, params.sched_priority);

   params.sched_priority = DEFAULT_PRORITY_THREAD_RADIO_RX;
   ret = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
   if ( ret != 0 )
      log_softerror_and_alarm("[RadioRxThread] Failed to set thread schedule class, error: %d, %s", errno, strerror(errno));

   ret = pthread_getschedparam(this_thread, &policy, &params);
   if ( ret != 0 )
     log_softerror_and_alarm("[RadioRxThread] Failed to get schedule param");
   log_line("[RadioRxThread] Current new thread policy/priority: %d/%d", policy, params.sched_priority);

   log_line("[RadioRxThread] Initialized State. Waiting for rx messages...");

   int* piQuit = (int*) argument;
   struct timeval timeRXRadio;
   
   int maxfd = -1;
   int iCountFD = 0;
   int iLoopCounter = 0;
   u32 uTimeLastLoopCheck = get_current_timestamp_ms();
   
   while ( 1 )
   {
      hardware_sleep_micros(500);

      u32 uTime = get_current_timestamp_ms();
      if ( uTime - uTimeLastLoopCheck > s_iRadioRxLoopTimeoutInterval )
      {
         log_softerror_and_alarm("[RadioRxThread] Radio Rx loop took too long (%d ms, read: %u microsec, queue: %u microsec).", uTime - uTimeLastLoopCheck, s_uRadioRxLastTimeRead, s_uRadioRxLastTimeQueue);
         if ( uTime - uTimeLastLoopCheck > s_RadioRxState.uMaxLoopTime )
            s_RadioRxState.uMaxLoopTime = uTime - uTimeLastLoopCheck;
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
         if ( s_RadioRxState.iRadioInterfacesBroken[i] )
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
               s_RadioRxState.iRadioInterfacesBroken[i] = 1;
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
      timeRXRadio.tv_usec = 2000; // 2 miliseconds timeout

      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (NULL == pRadioHWInfo) || (! pRadioHWInfo->openedForRead) )
            continue;
         if ( s_RadioRxState.iRadioInterfacesBroken[i] )
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
            s_RadioRxState.iRadioInterfacesBroken[i] = 1;
         continue;
      }

      // Received data, process it

      for(int iInterfaceIndex=0; iInterfaceIndex<hardware_get_radio_interfaces_count(); iInterfaceIndex++)
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
         if( (NULL == pRadioHWInfo) || (s_iRadioRxPausedInterfaces[iInterfaceIndex]) || (0 == FD_ISSET(pRadioHWInfo->monitor_interface_read.selectable_fd, &s_RadioRxReadSet)) )
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
            int iResult = _radio_rx_parse_received_wifi_radio_data(iInterfaceIndex);
            if ( iResult < 0 )
            {
               log_line("[RadioRx] Mark interface %d as broken", iInterfaceIndex+1);
               s_RadioRxState.iRadioInterfacesBroken[iInterfaceIndex] = 1;
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

int radio_rx_start_rx_thread(shared_mem_radio_stats* pSMRadioStats, shared_mem_radio_stats_interfaces_rx_graph* pSMRadioRxGraphs, int iSearchMode, u32 uAcceptedFirmwareType)
{
   if ( s_iRadioRxInitialized )
      return 1;

   s_pSMRadioStats = pSMRadioStats;
   s_pSMRadioRxGraphs = pSMRadioRxGraphs;
   s_iSearchMode = iSearchMode;
   s_iRadioRxSingalStop = 0;
   s_RadioRxState.uAcceptedFirmwareType = uAcceptedFirmwareType;
   
   if ( s_RadioRxState.uAcceptedFirmwareType == MODEL_FIRMWARE_TYPE_OPENIPC )
   {
      if ( ! s_iSodiumInited )
      {
         if ( sodium_init() < 0 )
            log_softerror_and_alarm("[RadioRx] Can't init sodium library.");
         else
            s_iSodiumInited = 1;
      }
      if ( 1 != load_openipc_keys(FILE_DEFAULT_OPENIPC_KEYS) )
         log_softerror_and_alarm("[RadioRx] Can't load openIpc keys from file [%s].", FILE_DEFAULT_OPENIPC_KEYS);
   }

   radio_rx_reset_interfaces_broken_state();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_iRadioRxPausedInterfaces[i] = 0;

   s_iRadioRxAllInterfacesPaused = 0;

   for( int i=0; i<MAX_RX_PACKETS_QUEUE; i++ )
   {
      s_RadioRxState.iPacketsLengths[i] = 0;
      s_RadioRxState.iPacketsAreShort[i] = 0;
      s_RadioRxState.pPacketsBuffers[i] = (u8*) malloc(MAX_PACKET_TOTAL_SIZE);
      if ( NULL == s_RadioRxState.pPacketsBuffers[i] )
      {
         log_error_and_alarm("[RadioRx] Failed to allocate rx packets buffers!");
         return 0;
      }
   }

   log_line("[RadioRx] Allocated %u bytes for %d rx packets.", MAX_RX_PACKETS_QUEUE * MAX_PACKET_TOTAL_SIZE, MAX_RX_PACKETS_QUEUE);

   s_RadioRxState.iCurrentRxPacketToConsume = 0;
   s_RadioRxState.iCurrentRxPacketIndex = 0;

   s_RadioRxState.uTimeLastStatsUpdate = get_current_timestamp_ms();
   
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

      if ( s_RadioRxState.vehicles[i].uDetectedFirmwareType & MODEL_FIRMWARE_TYPE_OPENIPC )
      {
         if ( NULL != get_openipc_key1() )
            memcpy( s_RadioRxState.vehicles[i].rx_secretkey, get_openipc_key1(), crypto_box_SECRETKEYBYTES);
         if ( NULL != get_openipc_key2() )
            memcpy( s_RadioRxState.vehicles[i].tx_publickey, get_openipc_key2(), crypto_box_SECRETKEYBYTES);
         memset( s_RadioRxState.vehicles[i].session_key, 0, sizeof(s_RadioRxState.vehicles[i].session_key)); 
         s_RadioRxState.vehicles[i].pFEC = NULL;
      }
   }
   s_RadioRxState.uTimeLastMinute = get_current_timestamp_ms();
   s_RadioRxState.iMaxPacketsInQueue = 0;
   s_RadioRxState.iMaxPacketsInQueueLastMinute = 0;

   
   s_RadioRxState.uMaxLoopTime = 0;

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

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);

   if ( ! s_iRadioRxInitialized )
   {
      s_iRadioRxPausedInterfaces[iInterfaceIndex] = 1;
      _radio_rx_check_update_all_paused_flag();
   }
   else
   {
      pthread_mutex_lock(&s_pThreadRadioRxMutex);
      s_iRadioRxPausedInterfaces[iInterfaceIndex]++;
      _radio_rx_check_update_all_paused_flag();
      pthread_mutex_unlock(&s_pThreadRadioRxMutex);    
   }

   if ( NULL != pRadioHWInfo )
      log_line("[RadioRx] Pause Rx on radio interface %d, [%s] (+%d)", iInterfaceIndex+1, pRadioHWInfo->szName, s_iRadioRxPausedInterfaces[iInterfaceIndex]);
   else
      log_line("[RadioRx] Pause Rx on radio interface %d, [%s] (+%d)", iInterfaceIndex+1, "N/A", s_iRadioRxPausedInterfaces[iInterfaceIndex]);
}

void radio_rx_resume_interface(int iInterfaceIndex)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);

   if ( ! s_iRadioRxInitialized )
   {
      s_iRadioRxPausedInterfaces[iInterfaceIndex] = 0;
      s_iRadioRxAllInterfacesPaused = 0;
   }
   else
   {
      pthread_mutex_lock(&s_pThreadRadioRxMutex);
      s_iRadioRxPausedInterfaces[iInterfaceIndex]--;
      s_iRadioRxAllInterfacesPaused = 0;
      pthread_mutex_unlock(&s_pThreadRadioRxMutex);
   }

   if ( NULL != pRadioHWInfo )
      log_line("[RadioRx] Resumed Rx on radio interface %d, [%s] (+%d)", iInterfaceIndex+1, pRadioHWInfo->szName, s_iRadioRxPausedInterfaces[iInterfaceIndex]);
   else
      log_line("[RadioRx] Resumed Rx on radio interface %d, [%s] (+%d)", iInterfaceIndex+1, "N/A", s_iRadioRxPausedInterfaces[iInterfaceIndex]);
}

void radio_rx_mark_quit()
{
   s_iRadioRxMarkedForQuit = 1;
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

   if ( nPacketLength >= sizeof(type_wfb_block_header) )
   if ( ((*pPacketBuffer) == PACKET_TYPE_WFB_DATA) || ((*pPacketBuffer) == PACKET_TYPE_WFB_KEY) )
   {
      return MODEL_FIRMWARE_TYPE_OPENIPC;
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

t_radio_rx_state* radio_rx_get_state()
{
   return &s_RadioRxState;
}

int radio_rx_has_packets_to_consume()
{
   if ( 0 == s_iRadioRxInitialized )
      return 0;
   if ( s_RadioRxState.iCurrentRxPacketIndex == s_RadioRxState.iCurrentRxPacketToConsume )
      return 0;

   int iCount = 0;
   int iBottom = 0;
   int iTop = 0;
   //pthread_mutex_lock(&s_pThreadRadioRxMutex);
   iBottom = s_RadioRxState.iCurrentRxPacketToConsume;
   iTop = s_RadioRxState.iCurrentRxPacketIndex;
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
   if ( s_RadioRxState.iCurrentRxPacketIndex == s_RadioRxState.iCurrentRxPacketToConsume )
      return NULL;

   pthread_mutex_lock(&s_pThreadRadioRxMutex);

   if ( NULL != pLength )
      *pLength = s_RadioRxState.iPacketsLengths[s_RadioRxState.iCurrentRxPacketToConsume];
   if ( NULL != pIsShortPacket )
      *pIsShortPacket = s_RadioRxState.iPacketsAreShort[s_RadioRxState.iCurrentRxPacketToConsume];
   if ( NULL != pRadioInterfaceIndex )
      *pRadioInterfaceIndex = s_RadioRxState.iPacketsRxInterface[s_RadioRxState.iCurrentRxPacketToConsume];

   memcpy( s_tmpLastProcessedRadioRxPacket, s_RadioRxState.pPacketsBuffers[s_RadioRxState.iCurrentRxPacketToConsume], s_RadioRxState.iPacketsLengths[s_RadioRxState.iCurrentRxPacketToConsume]);

   s_RadioRxState.iCurrentRxPacketToConsume++;
   if ( s_RadioRxState.iCurrentRxPacketToConsume >= MAX_RX_PACKETS_QUEUE )
      s_RadioRxState.iCurrentRxPacketToConsume = 0;

   pthread_mutex_unlock(&s_pThreadRadioRxMutex);

   return s_tmpLastProcessedRadioRxPacket;
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

