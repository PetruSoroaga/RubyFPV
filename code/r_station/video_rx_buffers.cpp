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

#include "video_rx_buffers.h"
#include "shared_vars.h"
#include "timers.h"
#include "packets_utils.h"
#include "../radio/fec.h"

int VideoRxPacketsBuffer::m_siVideoBuffersInstancesCount = 0;

typedef struct
{
   unsigned int fec_decode_missing_packets_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int fec_decode_fec_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_data_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_fec_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int missing_packets_count;
} type_fec_info;
type_fec_info s_FECRxInfo;

VideoRxPacketsBuffer::VideoRxPacketsBuffer(int iVideoStreamIndex, int iCameraIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siVideoBuffersInstancesCount;
   m_siVideoBuffersInstancesCount++;

   m_iVideoStreamIndex = iVideoStreamIndex;
   m_iCameraIndex = iCameraIndex;

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   {
      _empty_block_buffer_index(i);
      for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
      {
         m_VideoBlocks[i].packets[k].pRawData = NULL;
         m_VideoBlocks[i].packets[k].pPH = NULL;
         m_VideoBlocks[i].packets[k].pPHVF = NULL;
         m_VideoBlocks[i].packets[k].pVideoData = NULL;
      }
   }
   m_uMaxVideoBlockIndexInBuffer = 0;
   m_bEndOfFirstIFrameDetected = false;
   m_uMaxVideoBlockIndexReceived = 0;
   m_uMaxVideoBlockPacketIndexReceived = 0;
}

VideoRxPacketsBuffer::~VideoRxPacketsBuffer()
{
   uninit();

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
   {
      if ( NULL != m_VideoBlocks[i].packets[k].pRawData )
         free(m_VideoBlocks[i].packets[k].pRawData);

      m_VideoBlocks[i].packets[k].pRawData = NULL;
      m_VideoBlocks[i].packets[k].pPH = NULL;
      m_VideoBlocks[i].packets[k].pPHVF = NULL;
      m_VideoBlocks[i].packets[k].pVideoData = NULL;
   }

   m_siVideoBuffersInstancesCount--;
}

bool VideoRxPacketsBuffer::init(Model* pModel)
{
   if ( m_bInitialized )
      return true;

   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[VideoRXBuffer] Invalid model on init.");
      return false;
   }
   log_line("[VideoRXBuffer] Initialize video Rx buffer instance number %d.", m_iInstanceIndex+1);
   _empty_buffers("init");
   m_bInitialized = true;
   log_line("[VideoRXBuffer] Initialized video Tx buffer instance number %d.", m_iInstanceIndex+1);
   return true;
}

bool VideoRxPacketsBuffer::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[VideoRXBuffer] Uninitialize video Tx buffer instance number %d.", m_iInstanceIndex+1);
   
   m_bInitialized = false;
   return true;
}

void VideoRxPacketsBuffer::emptyBuffers(const char* szReason)
{
   _empty_buffers(szReason);
}

bool VideoRxPacketsBuffer::_check_allocate_video_block_in_buffer(int iBufferIndex)
{
   if ( (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) )
      return false;

   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockDataPackets + m_VideoBlocks[iBufferIndex].iBlockECPackets; i++ )
   {
      if ( NULL != m_VideoBlocks[iBufferIndex].packets[i].pRawData )
         continue;

      m_VideoBlocks[iBufferIndex].packets[i].pRawData = (u8*)malloc(MAX_PACKET_TOTAL_SIZE);
      if ( NULL == m_VideoBlocks[iBufferIndex].packets[i].pRawData )
      {
         log_error_and_alarm("[VideoRXBuffer] Failed to allocate packet, buffer index: %d, packet index %d", iBufferIndex, i);
         return false;
      }
      m_VideoBlocks[iBufferIndex].packets[i].pPH = (t_packet_header*)&(m_VideoBlocks[iBufferIndex].packets[i].pRawData[0]);
      m_VideoBlocks[iBufferIndex].packets[i].pPHVF = (t_packet_header_video_full_98*)&(m_VideoBlocks[iBufferIndex].packets[i].pRawData[sizeof(t_packet_header)]);
      m_VideoBlocks[iBufferIndex].packets[i].pVideoData = &(m_VideoBlocks[iBufferIndex].packets[i].pRawData[sizeof(t_packet_header) + sizeof(t_packet_header_video_full_98)]);
      _empty_block_buffer_packet_index(iBufferIndex, i);
   }
   return true;
}

void VideoRxPacketsBuffer::_empty_block_buffer_packet_index(int iBufferIndex, int iPacketIndex)
{
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].uReceivedTime = 0;
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].uRequestedTime = 0;
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].bEndOfFirstIFrameDetected = m_bEndOfFirstIFrameDetected;
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].bEmpty = true;
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].bOutputed = false;
}

void VideoRxPacketsBuffer::_empty_block_buffer_index(int iBufferIndex)
{
   m_VideoBlocks[iBufferIndex].uReceivedTime = 0;
   m_VideoBlocks[iBufferIndex].uVideoBlockIndex = 0;
   m_VideoBlocks[iBufferIndex].iBlockDataSize = 0;
   m_VideoBlocks[iBufferIndex].iBlockDataPackets = 0;
   m_VideoBlocks[iBufferIndex].iBlockECPackets = 0;
   m_VideoBlocks[iBufferIndex].iRecvDataPackets = 0;
   m_VideoBlocks[iBufferIndex].iRecvECPackets = 0;
   m_VideoBlocks[iBufferIndex].iReconstructedECUsed = 0;
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
   {
      _empty_block_buffer_packet_index(iBufferIndex, k);
   }
}

void VideoRxPacketsBuffer::_empty_buffers(const char* szReason)
{
   m_bEndOfFirstIFrameDetected = false;

   if ( NULL == szReason )
      log_line("Empty buffers (no reason)");
   else
      log_line("Empty buffers (%s)", szReason);
   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
      _empty_block_buffer_index(i);

   g_SMControllerRTInfo.uOutputedVideoPacketsSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex]++;

   m_iBufferIndexFirstReceivedBlock = -1;
   m_iBufferIndexFirstReceivedPacketIndex = -1;
   m_uMaxVideoBlockIndexInBuffer = 0;
   m_uMaxVideoBlockIndexReceived = 0;
   m_uMaxVideoBlockPacketIndexReceived = 0;
}

void VideoRxPacketsBuffer::_check_do_ec_for_video_block(int iBufferIndex)
{
   if ( (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) )
      return;

   if ( 0 == m_VideoBlocks[iBufferIndex].iBlockECPackets )
      return;

   if ( m_VideoBlocks[iBufferIndex].iRecvDataPackets >= m_VideoBlocks[iBufferIndex].iBlockDataPackets )
      return;

   if ( m_VideoBlocks[iBufferIndex].iRecvDataPackets + m_VideoBlocks[iBufferIndex].iRecvECPackets < m_VideoBlocks[iBufferIndex].iBlockDataPackets )
      return;

   m_VideoBlocks[iBufferIndex].iReconstructedECUsed = m_VideoBlocks[iBufferIndex].iBlockDataPackets - m_VideoBlocks[iBufferIndex].iRecvDataPackets;

   //log_line("DEBUG do EC for block %u, buffer index %d, missing packets: %d, block data size: %d", m_VideoBlocks[iBufferIndex].uVideoBlockIndex, iBufferIndex, m_VideoBlocks[iBufferIndex].iReconstructedECUsed, m_VideoBlocks[iBufferIndex].iBlockDataSize);

   t_packet_header* pPHGood = NULL;
   t_packet_header_video_full_98* pPHVFGood = NULL;
   t_packet_header_video_full_98_debug_info* pPHVFDebugInfoGood = NULL;

   // Find a good PH, PHVF and if there is (or not) video debug info in the block
   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockDataPackets; i++ )
   {
      if ( m_VideoBlocks[iBufferIndex].packets[i].bEmpty )
         continue;
      pPHGood = m_VideoBlocks[iBufferIndex].packets[i].pPH;
      pPHVFGood = m_VideoBlocks[iBufferIndex].packets[i].pPHVF;
      if ( pPHVFGood->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
         pPHVFDebugInfoGood = (t_packet_header_video_full_98_debug_info*) m_VideoBlocks[iBufferIndex].packets[i].pVideoData;
      break;
   }

   // Add existing data packets, mark and count the ones that are missing

   s_FECRxInfo.missing_packets_count = 0;
   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockDataPackets; i++ )
   {
      if ( m_VideoBlocks[iBufferIndex].packets[i].bEmpty )
      {
         u8* pVideoSource = m_VideoBlocks[iBufferIndex].packets[i].pVideoData;
         if ( NULL != pPHVFDebugInfoGood )
            pVideoSource += sizeof(t_packet_header_video_full_98_debug_info);
         s_FECRxInfo.fec_decode_data_packets_pointers[i] = pVideoSource;
         s_FECRxInfo.fec_decode_missing_packets_indexes[s_FECRxInfo.missing_packets_count] = i;
         s_FECRxInfo.missing_packets_count++;
         //log_line("DEBUG add packt index %d as missing", i);
      }
      else
      {
         t_packet_header_video_full_98* pPHVF = m_VideoBlocks[iBufferIndex].packets[i].pPHVF;
         t_packet_header_video_full_98_debug_info* pPHVFDebugInfo = (t_packet_header_video_full_98_debug_info*) m_VideoBlocks[iBufferIndex].packets[i].pVideoData;

         u8* pVideoSource = m_VideoBlocks[iBufferIndex].packets[i].pVideoData;
         if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
            pVideoSource += sizeof(t_packet_header_video_full_98_debug_info);

         s_FECRxInfo.fec_decode_data_packets_pointers[i] = pVideoSource;

         u16 uVideoSize = 0;
         memcpy(&uVideoSize, pVideoSource, sizeof(u16));
         u32 crc = base_compute_crc32(pVideoSource, pPHVF->uCurrentBlockPacketSize);
         //log_line("DEBUG add (%s) index %d to EC, video: %X size: %d-%d bytes, CRC %u = %u %s",
         //   m_VideoBlocks[iBufferIndex].packets[i].bEmpty?"miss":"pkg",
         //   i, pVideoSource, uVideoSize, pPHVF->uCurrentBlockPacketSize,
         //   pPHVFDebugInfo->uVideoCRC, crc, (pPHVFDebugInfo->uVideoCRC == crc)?"equal":"different");
      }
   }

   // To fix
   /*
   if ( s_FECInfo.missing_packets_count > g_PD_ControllerLinkStats.tmp_video_streams_blocks_max_ec_packets_used[0] )
   {
      // missing packets in a block can't be larger than 8 bits (config values for max EC/Data pachets)
      g_PD_ControllerLinkStats.tmp_video_streams_blocks_max_ec_packets_used[0] = s_FECInfo.missing_packets_count;
   }
   */

   // Add the needed FEC packets to the list
   unsigned int pos = 0;
   int iECDelta = m_VideoBlocks[iBufferIndex].iBlockDataPackets;
   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockECPackets; i++ )
   {
      if ( ! m_VideoBlocks[iBufferIndex].packets[i+iECDelta].bEmpty )
      {
         t_packet_header_video_full_98* pPHVF = m_VideoBlocks[iBufferIndex].packets[i+iECDelta].pPHVF;
         t_packet_header_video_full_98_debug_info* pPHVFDebugInfo = (t_packet_header_video_full_98_debug_info*) m_VideoBlocks[iBufferIndex].packets[i+iECDelta].pVideoData;
         u8* pVideoSource = m_VideoBlocks[iBufferIndex].packets[i+iECDelta].pVideoData;
         if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
            pVideoSource += sizeof(t_packet_header_video_full_98_debug_info);
         s_FECRxInfo.fec_decode_fec_packets_pointers[pos] = pVideoSource;
         s_FECRxInfo.fec_decode_fec_indexes[pos] = i;

         u32 crc = base_compute_crc32(pVideoSource, m_VideoBlocks[iBufferIndex].packets[i+iECDelta].pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_video_full_98) - sizeof(t_packet_header_video_full_98_debug_info));
         //log_line("DEBUG added EC packet index %d (%d total bytes) for decoding. CRC: %u = %u ? %s",
         //      i + iECDelta, m_VideoBlocks[iBufferIndex].packets[i+iECDelta].pPH->total_length,
         //      pPHVFDebugInfo->uVideoCRC, crc, (pPHVFDebugInfo->uVideoCRC == crc)?"equal":"different");

         pos++;
         if ( pos == s_FECRxInfo.missing_packets_count )
            break;
      }
   }

   //log_line("DEBUG do actual EC for block size %d...", m_VideoBlocks[iBufferIndex].iBlockDataSize );

   fec_decode(m_VideoBlocks[iBufferIndex].iBlockDataSize, s_FECRxInfo.fec_decode_data_packets_pointers, m_VideoBlocks[iBufferIndex].iBlockDataPackets, s_FECRxInfo.fec_decode_fec_packets_pointers, s_FECRxInfo.fec_decode_fec_indexes, s_FECRxInfo.fec_decode_missing_packets_indexes, s_FECRxInfo.missing_packets_count );
   //log_line("DEBUG done EC decoding for block size %d, missing packets: %d", m_VideoBlocks[iBufferIndex].iBlockDataSize, s_FECRxInfo.missing_packets_count);
   
   // Mark all data packets reconstructed as received, set the right info in them (video header info)
   for( u32 i=0; i<s_FECRxInfo.missing_packets_count; i++ )
   {
      int iPacketIndex = s_FECRxInfo.fec_decode_missing_packets_indexes[i];
      //log_line("DEBUG recover packet index %d", iPacketIndex);
      m_VideoBlocks[iBufferIndex].packets[iPacketIndex].bEmpty = false;
      m_VideoBlocks[iBufferIndex].packets[iPacketIndex].bOutputed = false;
      m_VideoBlocks[iBufferIndex].packets[iPacketIndex].bEndOfFirstIFrameDetected = m_bEndOfFirstIFrameDetected;
      m_VideoBlocks[iBufferIndex].packets[iPacketIndex].uReceivedTime = g_TimeNow;
      m_VideoBlocks[iBufferIndex].iRecvDataPackets++;

      t_packet_header* pPH = m_VideoBlocks[iBufferIndex].packets[iPacketIndex].pPH;
      t_packet_header_video_full_98* pPHVF = m_VideoBlocks[iBufferIndex].packets[iPacketIndex].pPHVF;
      t_packet_header_video_full_98_debug_info* pPHVFDebugInfo = (t_packet_header_video_full_98_debug_info*) m_VideoBlocks[iBufferIndex].packets[iPacketIndex].pVideoData;
      if ( NULL != pPHGood )
         memcpy(pPH, pPHGood, sizeof(t_packet_header));
      if ( NULL != pPHVFGood )
         memcpy(pPHVF, pPHVFGood, sizeof(t_packet_header_video_full_98));
      if ( NULL != pPHVFDebugInfoGood )
         memcpy(pPHVFDebugInfo, pPHVFDebugInfoGood, sizeof(t_packet_header_video_full_98_debug_info));

      pPHVF->uCurrentBlockPacketIndex = iPacketIndex;

      u8* pVideoSource = m_VideoBlocks[iBufferIndex].packets[iPacketIndex].pVideoData;
      if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
         pVideoSource += sizeof(t_packet_header_video_full_98_debug_info);

      u16 uVideoDataSize = 0;
      memcpy(&uVideoDataSize, pVideoSource, sizeof(u16));
      u32 crc = base_compute_crc32(pVideoSource, pPHVF->uCurrentBlockPacketSize);
      //log_line("DEBUG recovered data packet index %d [%u/%u] (%d total bytes, %d video data), CRC %u",
      //   iPacketIndex, pPHVF->uCurrentBlockIndex, pPHVF->uCurrentBlockPacketIndex,
      //   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].pPH->total_length,
      //   uVideoDataSize,
      //   crc);
      
      // To fix
      //if ( m_SM_VideoDecodeStats.currentPacketsInBuffers > m_SM_VideoDecodeStats.maxPacketsInBuffers )
      //   m_SM_VideoDecodeStats.maxPacketsInBuffers = m_SM_VideoDecodeStats.currentPacketsInBuffers;
   }
}

// Returns true if the packet has the highest video block/packet index received (in order)
bool VideoRxPacketsBuffer::_add_video_packet_to_buffer(int iBufferIndex, u8* pPacket, int iPacketLength)
{
   if ( (NULL == pPacket) || (iPacketLength < (int)(sizeof(t_packet_header)+sizeof(t_packet_header_video_full_98))) || (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) )
      return false;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   t_packet_header_video_full_98* pPHVF = (t_packet_header_video_full_98*)(pPacket + sizeof(t_packet_header));
   
   m_VideoBlocks[iBufferIndex].uVideoBlockIndex = pPHVF->uCurrentBlockIndex;
   m_VideoBlocks[iBufferIndex].iBlockDataSize = pPHVF->uCurrentBlockPacketSize;
   m_VideoBlocks[iBufferIndex].iBlockDataPackets = pPHVF->uCurrentBlockDataPackets;
   m_VideoBlocks[iBufferIndex].iBlockECPackets = pPHVF->uCurrentBlockECPackets;
   m_VideoBlocks[iBufferIndex].uReceivedTime = g_TimeNow;

   if ( ! _check_allocate_video_block_in_buffer(iBufferIndex) )
      return false;

   /*
   if ( pPHVF->uCurrentBlockPacketIndex == (iBufferIndex%5) )
   {
      log_line("DEBUG test");
      m_VideoBlocks[iBufferIndex].packets[iBufferIndex%5].bEndOfFirstIFrameDetected = false;
      m_VideoBlocks[iBufferIndex].packets[iBufferIndex%5].bEmpty = true;
      m_VideoBlocks[iBufferIndex].packets[iBufferIndex%5].bOutputed = false;
      m_VideoBlocks[iBufferIndex].packets[iBufferIndex%5].uReceivedTime = 0;
      memset(m_VideoBlocks[iBufferIndex].packets[iBufferIndex%5].pRawData, 0, MAX_PACKET_TOTAL_SIZE);
      //m_VideoBlocks[iBufferIndex].iRecvDataPackets--;
      return;
   }
   */

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
   {
      //log_line("DEBUG vrx add retr on buf index %d: current state empty: %d, outputed: %d",
      //    iBufferIndex,
      //    m_VideoBlocks[iBufferIndex].packets[pPHVF->uCurrentBlockPacketIndex].bEmpty,
      //    m_VideoBlocks[iBufferIndex].packets[pPHVF->uCurrentBlockPacketIndex].bOutputed );
   }
   
   if ( ! m_VideoBlocks[iBufferIndex].packets[pPHVF->uCurrentBlockPacketIndex].bEmpty )
      return false;
   if ( m_VideoBlocks[iBufferIndex].packets[pPHVF->uCurrentBlockPacketIndex].bOutputed )
      return false;

   if ( pPHVF->uCurrentBlockIndex > m_uMaxVideoBlockIndexInBuffer )
      m_uMaxVideoBlockIndexInBuffer = pPHVF->uCurrentBlockIndex;
   
   //log_line("DEBUG adding packet [%u/%u/%u-%d] to buffer index %d-%d, first in buffer: %d/%d",
   //  pPHVF->uCurrentBlockIndex, pPHVF->uCurrentBlockPacketIndex,
   //  pPH->total_length, iPacketLength, iBufferIndex, pPHVF->uCurrentBlockPacketIndex, m_iBufferIndexFirstReceivedBlock, m_iBufferIndexFirstReceivedPacketIndex);

   m_VideoBlocks[iBufferIndex].packets[pPHVF->uCurrentBlockPacketIndex].bEndOfFirstIFrameDetected = m_bEndOfFirstIFrameDetected;
   if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_END_FRAME )
   if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_IFRAME )
      m_bEndOfFirstIFrameDetected = true;

   // Begin - Update Runtime Stats

   g_SMControllerRTInfo.uSliceUpdateTime[g_SMControllerRTInfo.iCurrentIndex] = g_TimeNow;
   if ( pPHVF->uCurrentBlockPacketIndex < pPHVF->uCurrentBlockDataPackets )
      g_SMControllerRTInfo.uRecvVideoDataPackets[g_SMControllerRTInfo.iCurrentIndex]++;
   else
      g_SMControllerRTInfo.uRecvVideoECPackets[g_SMControllerRTInfo.iCurrentIndex]++;

   if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_END_FRAME )
   {
      if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_IFRAME )
         g_SMControllerRTInfo.uRecvEndOfFrame[g_SMControllerRTInfo.iCurrentIndex] = 2;
      else
         g_SMControllerRTInfo.uRecvEndOfFrame[g_SMControllerRTInfo.iCurrentIndex] = 1;
   }
   else if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_IFRAME )
      g_SMControllerRTInfo.uRecvEndOfFrame[g_SMControllerRTInfo.iCurrentIndex] = 3;
  
   // End - Update Runtime Stats

   m_VideoBlocks[iBufferIndex].packets[pPHVF->uCurrentBlockPacketIndex].bEmpty = false;
   m_VideoBlocks[iBufferIndex].packets[pPHVF->uCurrentBlockPacketIndex].bOutputed = false;
   
   memcpy(m_VideoBlocks[iBufferIndex].packets[pPHVF->uCurrentBlockPacketIndex].pRawData, pPacket, iPacketLength);
   
   // Set remaining empty space to 0 as EC uses the good video data packets too.
   
   t_packet_header_video_full_98_debug_info* pPHVFDebugInfo = (t_packet_header_video_full_98_debug_info*)m_VideoBlocks[iBufferIndex].packets[pPHVF->uCurrentBlockPacketIndex].pVideoData;
   u8* pVideoSource = m_VideoBlocks[iBufferIndex].packets[pPHVF->uCurrentBlockPacketIndex].pVideoData;
   if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
      pVideoSource += sizeof(t_packet_header_video_full_98_debug_info);

   u16 uVideoDataSize = 0;
   memcpy(&uVideoDataSize, pVideoSource, sizeof(u16));
   if ( uVideoDataSize < pPHVF->uCurrentBlockPacketSize - sizeof(u16) )
   {
      //log_line("DEBUG fill %d empty bytes on %X", (int)pPHVF->uCurrentBlockPacketSize - sizeof(u16) - (int)uVideoDataSize, pVideoSource);
      memset(pVideoSource + sizeof(u16) + uVideoDataSize, 0, (int)pPHVF->uCurrentBlockPacketSize - sizeof(u16) - (int)uVideoDataSize );
   }
   u32 crc = base_compute_crc32(pVideoSource, pPHVF->uCurrentBlockPacketSize);
   //log_line("DEBUG video source size: %d (expected: %d - 2), CRC %u = %u %s",
   //   uVideoDataSize, pPHVF->uCurrentBlockPacketSize,
   //   pPHVFDebugInfo->uVideoCRC, crc, (pPHVFDebugInfo->uVideoCRC == crc)?"equal":"different");

   if ( pPHVF->uCurrentBlockPacketIndex < pPHVF->uCurrentBlockDataPackets )
      m_VideoBlocks[iBufferIndex].iRecvDataPackets++;
   else
      m_VideoBlocks[iBufferIndex].iRecvECPackets++;

   if ( pPHVF->uCurrentBlockIndex > m_uMaxVideoBlockIndexReceived )
   {
      m_uMaxVideoBlockIndexReceived = pPHVF->uCurrentBlockIndex;
      m_uMaxVideoBlockPacketIndexReceived = pPHVF->uCurrentBlockPacketIndex;
      return true;
   }
   if ( pPHVF->uCurrentBlockIndex == m_uMaxVideoBlockIndexReceived )
   if ( pPHVF->uCurrentBlockPacketIndex > m_uMaxVideoBlockPacketIndexReceived )
   {
      m_uMaxVideoBlockIndexReceived = pPHVF->uCurrentBlockIndex;
      m_uMaxVideoBlockPacketIndexReceived = pPHVF->uCurrentBlockPacketIndex;
      return true;    
   }
   return false;
}
      
// Returns true if the packet has the highest video block/packet index received (in order)
bool VideoRxPacketsBuffer::checkAddVideoPacket(u8* pPacket, int iPacketLength)
{
   if ( (NULL == pPacket) || (iPacketLength <= (int)(sizeof(t_packet_header)+sizeof(t_packet_header_video_full_98))) )
      return false;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   t_packet_header_video_full_98* pPHVF = (t_packet_header_video_full_98*)(pPacket + sizeof(t_packet_header));
   int iVideoDataSize = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_video_full_98);
   u8* pVideoSource = pPacket + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_98);
   if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
   {
      iVideoDataSize -= sizeof(t_packet_header_video_full_98_debug_info);
      pVideoSource += sizeof(t_packet_header_video_full_98_debug_info);
   }
   u16 uVideoSize = 0;
   memcpy(&uVideoSize, pVideoSource, sizeof(u16));
   
   if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
   {
      t_packet_header_video_full_98_debug_info* pPHVFDebugInfo = (t_packet_header_video_full_98_debug_info*)(pPacket + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_98));
      u32 uVideoCRC = base_compute_crc32(pVideoSource, pPHVF->uCurrentBlockPacketSize);
      //log_line("DEBUG recv packet [%u/%u], %d video data (%d + 2), CRC %u = %u %s",
      //  pPHVF->uCurrentBlockIndex, pPHVF->uCurrentBlockPacketIndex,
      //  iVideoDataSize, uVideoSize, pPHVFDebugInfo->uVideoCRC, uVideoCRC, (pPHVFDebugInfo->uVideoCRC == uVideoCRC)?"equal":"different");
   }
   /*
   static u32 s_uTmpDebugLastBlock = 0;
   static u32 s_uTmpDebugLastPacket =0;
   
   u16 uTmp = 0;
   memcpy(&uTmp, pPacket + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_98), sizeof(u16));

   int iMissing = 0;
   if ( pPHVF->uCurrentBlockIndex == s_uTmpDebugLastBlock )
   if ( pPHVF->uCurrentBlockPacketIndex > s_uTmpDebugLastPacket+1 )
      iMissing = pPHVF->uCurrentBlockPacketIndex - s_uTmpDebugLastPacket - 1;
   if ( pPHVF->uCurrentBlockIndex > s_uTmpDebugLastBlock+1 )
      iMissing = 1;
   if ( pPHVF->uCurrentBlockIndex == s_uTmpDebugLastBlock+1 )
   if ( (pPHVF->uCurrentBlockPacketIndex != 0) || (s_uTmpDebugLastPacket != (u32)(pPHVF->uCurrentBlockDataPackets + pPHVF->uCurrentBlockECPackets -1) ) )
      iMissing = 1;

   //if ( iMissing )
   //   log_line("DEBUG 2recv video [%u/%u] %s%d %d/%d bytes", pPHVF->uCurrentBlockIndex, pPHVF->uCurrentBlockPacketIndex,
   //      iMissing?"***":"   ", iMissing, pPHVF->uCurrentBlockPacketSize, pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_video_full_98));
   
   s_uTmpDebugLastBlock = pPHVF->uCurrentBlockIndex;
   s_uTmpDebugLastPacket = pPHVF->uCurrentBlockPacketIndex;
   */


   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
   {
      //if ( -1 == m_iBufferIndexFirstReceivedBlock )
      //   log_line("DEBUG vrx check add retransmitted packet, buffer start index: %d", m_iBufferIndexFirstReceivedBlock);
      //else
      //   log_line("DEBUG vrx check add retransmitted packet, buffer start index: %d (%u)", m_iBufferIndexFirstReceivedBlock, m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex);
   }
   // Empty buffers? (never received anything?)

   if ( -1 == m_iBufferIndexFirstReceivedBlock )
   {
      // Add packet if it's the first packet from the block. Discard if not first from block
      if ( 0 != pPHVF->uCurrentBlockPacketIndex )
        return false;
  
      m_iBufferIndexFirstReceivedBlock = 0;
      m_iBufferIndexFirstReceivedPacketIndex = 0;
      return _add_video_packet_to_buffer(m_iBufferIndexFirstReceivedBlock, pPacket, iPacketLength);
   }

   // Non-empty buffer from now on

   // Too old video block?
   
   if ( pPHVF->uCurrentBlockIndex < m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
   {
      // Discard it and detect restart of vehicle
      if ( pPHVF->uCurrentBlockIndex + 20 < m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
         _empty_buffers("received video block too old, vehicle was restarted");
      return false;
   }

   // Find delta from start of buffer (first received video block in buffer)
   u32 uDeltaVideoBlocks = pPHVF->uCurrentBlockIndex - m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex;

   // No more room? Discard all buffers as the time difference is too big since first video block in buffer
   if ( uDeltaVideoBlocks >= MAX_RXTX_BLOCKS_BUFFER-5 )
   {
      _empty_buffers("no more room");
      if ( 0 != pPHVF->uCurrentBlockPacketIndex )
         return false;

      m_iBufferIndexFirstReceivedBlock = 0;
      m_iBufferIndexFirstReceivedPacketIndex = 0;
      return _add_video_packet_to_buffer(m_iBufferIndexFirstReceivedBlock, pPacket, iPacketLength);
   }

   // Set all video blocks indexes from first video block in buffer to this one

   int iTargetBufferIndex = m_iBufferIndexFirstReceivedBlock+1;
   if ( iTargetBufferIndex >= MAX_RXTX_BLOCKS_BUFFER )
      iTargetBufferIndex = 0;
   u32 uVideoBlockIndex = m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex+1;
   for( int i=0; i<(int)uDeltaVideoBlocks; i++ )
   {
      m_VideoBlocks[iTargetBufferIndex].uVideoBlockIndex = uVideoBlockIndex;
      m_VideoBlocks[iTargetBufferIndex].iBlockDataSize = pPHVF->uCurrentBlockPacketSize;
      m_VideoBlocks[iTargetBufferIndex].iBlockDataPackets = pPHVF->uCurrentBlockDataPackets;
      m_VideoBlocks[iTargetBufferIndex].iBlockECPackets = pPHVF->uCurrentBlockECPackets;

      if ( ! _check_allocate_video_block_in_buffer(iTargetBufferIndex) )
         return false;
      uVideoBlockIndex++;
      iTargetBufferIndex++;
      if ( iTargetBufferIndex >= MAX_RXTX_BLOCKS_BUFFER )
         iTargetBufferIndex = 0;
   }

   iTargetBufferIndex = m_iBufferIndexFirstReceivedBlock + uDeltaVideoBlocks;
   if ( iTargetBufferIndex >= MAX_RXTX_BLOCKS_BUFFER )
      iTargetBufferIndex -= MAX_RXTX_BLOCKS_BUFFER;

   bool bReturn = _add_video_packet_to_buffer(iTargetBufferIndex, pPacket, iPacketLength);

   if ( pPHVF->uCurrentBlockECPackets > 0 )
   if ( pPHVF->uCurrentBlockPacketIndex >= pPHVF->uCurrentBlockDataPackets )
      _check_do_ec_for_video_block(iTargetBufferIndex);

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
      _check_do_ec_for_video_block(iTargetBufferIndex);
   return bReturn;
}

u32 VideoRxPacketsBuffer::getMaxReceivedVideoBlockIndex()
{
   return m_uMaxVideoBlockIndexInBuffer;
}

bool VideoRxPacketsBuffer::hasFirstVideoPacketInBuffer()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return false;
   return ! m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].packets[m_iBufferIndexFirstReceivedPacketIndex].bEmpty;
}

bool VideoRxPacketsBuffer::hasIncompleteBlocks()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return false;

   if ( m_uMaxVideoBlockIndexInBuffer > m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
      return true;
   return false;
}

int VideoRxPacketsBuffer::getBlocksCountInBuffer()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return 0;

   if ( m_uMaxVideoBlockIndexInBuffer == m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
      return 1;

   int iCountBlocks = 1;
   int iBlockIndex = m_iBufferIndexFirstReceivedBlock;
   while ( m_VideoBlocks[iBlockIndex].uVideoBlockIndex != m_uMaxVideoBlockIndexInBuffer )
   {
      iBlockIndex++;
      if ( iBlockIndex >= MAX_RXTX_BLOCKS_BUFFER )
         iBlockIndex = 0;
      if ( iBlockIndex == m_iBufferIndexFirstReceivedBlock )
         break;

      iCountBlocks++;
   }
   return iCountBlocks;
}

type_rx_video_packet_info* VideoRxPacketsBuffer::getFirstVideoPacketInBuffer()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return NULL;
   return &(m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].packets[m_iBufferIndexFirstReceivedPacketIndex]);
}

type_rx_video_block_info* VideoRxPacketsBuffer::getFirstVideoBlockInBuffer()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return NULL;
   return &(m_VideoBlocks[m_iBufferIndexFirstReceivedBlock]);
}

type_rx_video_block_info* VideoRxPacketsBuffer::getVideoBlockInBuffer(int iStartPosition)
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return NULL;
   int iIndex = m_iBufferIndexFirstReceivedBlock + iStartPosition;
   if ( iIndex >= MAX_RXTX_BLOCKS_BUFFER )
      iIndex -= MAX_RXTX_BLOCKS_BUFFER;
   return &(m_VideoBlocks[iIndex]);
}


void VideoRxPacketsBuffer::advanceStartPosition()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return;

   u32 uFirstVideoBlockIndexInBuffer = m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex;
   m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].packets[m_iBufferIndexFirstReceivedPacketIndex].bOutputed = true;
   m_iBufferIndexFirstReceivedPacketIndex++;
   
   // Skip over the EC packets or empty blocks (iBlockDataPackets is 0)
   if ( m_iBufferIndexFirstReceivedPacketIndex >= m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iBlockDataPackets )
   {
      _empty_block_buffer_index(m_iBufferIndexFirstReceivedBlock);
      m_iBufferIndexFirstReceivedBlock++;
      m_iBufferIndexFirstReceivedPacketIndex = 0;
      if ( m_iBufferIndexFirstReceivedBlock >= MAX_RXTX_BLOCKS_BUFFER )
         m_iBufferIndexFirstReceivedBlock = 0;

      m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex = uFirstVideoBlockIndexInBuffer+1;
   }
}


void VideoRxPacketsBuffer::advanceStartPositionToVideoBlock(u32 uVideoBlockIndex)
{
   //log_line("DEBUG skip buffers from current video block index %u to %u", m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex, uVideoBlockIndex);
   if ( uVideoBlockIndex > m_uMaxVideoBlockIndexInBuffer )
      uVideoBlockIndex = m_uMaxVideoBlockIndexInBuffer;

   // Skip until the desired block or first that can be outputed
   while ( m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex != uVideoBlockIndex )
   {
      if ( m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iRecvDataPackets + m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iRecvECPackets >= m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iBlockDataPackets )
         break;

      //log_line("DEBUG discard video block %u, received packets: %d/%d", m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex,
      //   m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iRecvDataPackets,
      //   m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iRecvECPackets);

      g_SMControllerRTInfo.uOutputedVideoPacketsSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex]++;
      
      // Discard first block in buffer
      _empty_block_buffer_index(m_iBufferIndexFirstReceivedBlock);
      m_iBufferIndexFirstReceivedPacketIndex = 0;
      m_iBufferIndexFirstReceivedBlock++;
      if ( m_iBufferIndexFirstReceivedBlock >= MAX_RXTX_BLOCKS_BUFFER )
         m_iBufferIndexFirstReceivedBlock = 0;

      if ( 0 == m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
         break;
   }
}
