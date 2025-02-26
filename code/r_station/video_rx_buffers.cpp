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
         m_VideoBlocks[i].packets[k].pVideoData = NULL;
         m_VideoBlocks[i].packets[k].pPH = NULL;
         m_VideoBlocks[i].packets[k].pPHVS = NULL;
         m_VideoBlocks[i].packets[k].pPHVSImp = NULL;
      }
   }
   m_bBuffersAreEmpty = true;
   m_uMaxVideoBlockIndexPresentInBuffer = 0;
   m_uMaxVideoBlockPacketIndexPresentInBuffer = 0;
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
      m_VideoBlocks[i].packets[k].pVideoData = NULL;
      m_VideoBlocks[i].packets[k].pPH = NULL;
      m_VideoBlocks[i].packets[k].pPHVS = NULL;
      m_VideoBlocks[i].packets[k].pPHVSImp = NULL;
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
   _empty_buffers("init", NULL, NULL);
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
   _empty_buffers(szReason, NULL, NULL);
}

bool VideoRxPacketsBuffer::_check_allocate_video_block_in_buffer(int iBufferIndex)
{
   if ( (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) )
      return false;

   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockDataPackets + m_VideoBlocks[iBufferIndex].iBlockECPackets; i++ )
   {
      if ( NULL != m_VideoBlocks[iBufferIndex].packets[i].pRawData )
         continue;

      u8* pRawData = (u8*)malloc(MAX_PACKET_TOTAL_SIZE);
      if ( NULL == pRawData )
      {
         log_error_and_alarm("[VideoRXBuffer] Failed to allocate packet, buffer index: %d, packet index %d", iBufferIndex, i);
         return false;
      }
      m_VideoBlocks[iBufferIndex].packets[i].pRawData = pRawData;
      m_VideoBlocks[iBufferIndex].packets[i].pVideoData = pRawData + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment);
      m_VideoBlocks[iBufferIndex].packets[i].pPH = (t_packet_header*)pRawData;
      m_VideoBlocks[iBufferIndex].packets[i].pPHVS = (t_packet_header_video_segment*)(pRawData + sizeof(t_packet_header));
      m_VideoBlocks[iBufferIndex].packets[i].pPHVSImp = (t_packet_header_video_segment_important*)(pRawData + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment));
      _empty_block_buffer_packet_index(iBufferIndex, i);
   }
   return true;
}

void VideoRxPacketsBuffer::_empty_block_buffer_packet_index(int iBufferIndex, int iPacketIndex)
{
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].uReceivedTime = 0;
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].uRequestedTime = 0;
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

void VideoRxPacketsBuffer::_empty_buffers(const char* szReason, t_packet_header* pPH, t_packet_header_video_segment* pPHVS)
{
   resetFrameEndDetectedFlag();
  
   char szLog[256];
   if ( NULL == szReason )
      strcpy(szLog, "[VRXBuffers] Empty buffers (no reason)");
   else
      sprintf(szLog, "[VRXBuffers] Empty buffers (%s)", szReason);

   if ( (NULL == pPH) || (NULL == pPHVS) )
      log_line("%s (no additional data)", szLog);
   else
   {
      log_line("%s (recv video packet [%u/%u] (retransmitted: %s), frame index: %u, stream id/packet index: %u, radio index: %u)",
         szLog, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
         (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED)?"yes":"no",
         pPHVS->uH264FrameIndex,
         (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_INDEX) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX,
         pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, pPH->radio_link_packet_index);
   
      log_line("[VRXBuffers] Max video block packet received: [%u/%u]", m_uMaxVideoBlockIndexReceived, m_uMaxVideoBlockPacketIndexReceived);
      log_line("[VRXBuffers] First received block buffer index: %d, packet index: %d",
         m_iBufferIndexFirstReceivedBlock, m_iBufferIndexFirstReceivedPacketIndex );
      if ( m_iBufferIndexFirstReceivedBlock != -1 )
      {
         if ( -1 != m_iBufferIndexFirstReceivedPacketIndex )
            log_line("[VRXBuffers] buffer[%d][%d] is empty? %s, 0x%X",
               m_iBufferIndexFirstReceivedBlock, m_iBufferIndexFirstReceivedPacketIndex,
               m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].packets[m_iBufferIndexFirstReceivedPacketIndex].bEmpty?"yes":"no",
               m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].packets[m_iBufferIndexFirstReceivedPacketIndex].pPH);
         int iBufferIndex = m_iBufferIndexFirstReceivedBlock;
         log_line("[VRXBuffers] First recv video block index: %u, received data/ec in this block: %d,%d, time now, recv: %u - %u = %u",
            m_VideoBlocks[iBufferIndex].uVideoBlockIndex,
            m_VideoBlocks[iBufferIndex].iRecvDataPackets,
            m_VideoBlocks[iBufferIndex].iRecvECPackets,
            g_TimeNow, m_VideoBlocks[iBufferIndex].uReceivedTime,
            g_TimeNow - m_VideoBlocks[iBufferIndex].uReceivedTime);

         if ( NULL != m_VideoBlocks[iBufferIndex].packets[0].pPH )
         log_line("[VRXBuffers] First recv video block: %u = [%u/%u] (retr: %s), frame index: %u, stream id/packet index: %u, radio index: %u",
            m_VideoBlocks[iBufferIndex].uVideoBlockIndex,
            m_VideoBlocks[iBufferIndex].packets[0].pPHVS->uCurrentBlockIndex,
            m_VideoBlocks[iBufferIndex].packets[0].pPHVS->uCurrentBlockPacketIndex,
            (m_VideoBlocks[iBufferIndex].packets[0].pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED)?"yes":"no",
            m_VideoBlocks[iBufferIndex].packets[0].pPHVS->uH264FrameIndex,
            (m_VideoBlocks[iBufferIndex].packets[0].pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_INDEX) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX,
            m_VideoBlocks[iBufferIndex].packets[0].pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, m_VideoBlocks[iBufferIndex].packets[0].pPH->radio_link_packet_index );
         
         
         iBufferIndex = m_iBufferIndexFirstReceivedBlock+1;
         if ( iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER )
            iBufferIndex = 0;
         log_line("[VRXBuffers] Second recv video block index: %u, received data/ec in this block: %d,%d, time now, recv: %u - %u = %u",
            m_VideoBlocks[iBufferIndex].uVideoBlockIndex,
            m_VideoBlocks[iBufferIndex].iRecvDataPackets,
            m_VideoBlocks[iBufferIndex].iRecvECPackets, 
            g_TimeNow,
            m_VideoBlocks[iBufferIndex].uReceivedTime,
            g_TimeNow - m_VideoBlocks[iBufferIndex].uReceivedTime);
         if ( NULL != m_VideoBlocks[iBufferIndex].packets[0].pPH )
         log_line("[VRXBuffers] Second recv video block: %u = [%u/%u] (retr: %s), frame index: %u, stream id/packet index: %u, radio index: %u",
            m_VideoBlocks[iBufferIndex].uVideoBlockIndex,
            m_VideoBlocks[iBufferIndex].packets[0].pPHVS->uCurrentBlockIndex,
            m_VideoBlocks[iBufferIndex].packets[0].pPHVS->uCurrentBlockPacketIndex,
            (m_VideoBlocks[iBufferIndex].packets[0].pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED)?"yes":"no",
            m_VideoBlocks[iBufferIndex].packets[0].pPHVS->uH264FrameIndex,
            (m_VideoBlocks[iBufferIndex].packets[0].pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_INDEX) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX,
            m_VideoBlocks[iBufferIndex].packets[0].pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, m_VideoBlocks[iBufferIndex].packets[0].pPH->radio_link_packet_index );
      }
   }

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
      _empty_block_buffer_index(i);

   m_bBuffersAreEmpty = true;

   g_SMControllerRTInfo.uOutputedVideoPacketsSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex]++;
   if ( g_TimeNow > g_TimeLastVideoParametersOrProfileChanged + 3000 )
   if ( g_TimeNow > g_TimeStart + 5000 )
      g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks++;

   m_iBufferIndexFirstReceivedBlock = -1;
   m_iBufferIndexFirstReceivedPacketIndex = -1;

   m_uMaxVideoBlockIndexPresentInBuffer = 0;
   m_uMaxVideoBlockPacketIndexPresentInBuffer = 0;
   m_uMaxVideoBlockIndexReceived = 0;
   m_uMaxVideoBlockPacketIndexReceived = 0;

   log_line("[VRXBuffers] Done emptying buffers.");
}

void VideoRxPacketsBuffer::_check_do_ec_for_video_block(int iBufferIndex)
{
   if ( (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) )
      return;

   if ( m_VideoBlocks[iBufferIndex].iRecvDataPackets >= m_VideoBlocks[iBufferIndex].iBlockDataPackets )
      return;

   if ( 0 == m_VideoBlocks[iBufferIndex].iBlockECPackets )
      return;

   if ( m_VideoBlocks[iBufferIndex].iRecvDataPackets + m_VideoBlocks[iBufferIndex].iRecvECPackets < m_VideoBlocks[iBufferIndex].iBlockDataPackets )
      return;

   m_VideoBlocks[iBufferIndex].iReconstructedECUsed = m_VideoBlocks[iBufferIndex].iBlockDataPackets - m_VideoBlocks[iBufferIndex].iRecvDataPackets;

   t_packet_header* pPHGood = NULL;
   t_packet_header_video_segment* pPHVSGood = NULL;
   int iPacketIndexGood = -1;

   // Add existing data packets, mark and count the ones that are missing
   // Find a good PH, PHVF and video-debug-info (if any) in the block

   s_FECRxInfo.missing_packets_count = 0;
   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockDataPackets; i++ )
   {
      s_FECRxInfo.fec_decode_data_packets_pointers[i] = m_VideoBlocks[iBufferIndex].packets[i].pVideoData;
      if ( m_VideoBlocks[iBufferIndex].packets[i].bEmpty )
      {
         s_FECRxInfo.fec_decode_missing_packets_indexes[s_FECRxInfo.missing_packets_count] = i;
         s_FECRxInfo.missing_packets_count++;
      }
      else
      {
         pPHGood = m_VideoBlocks[iBufferIndex].packets[i].pPH;
         pPHVSGood = m_VideoBlocks[iBufferIndex].packets[i].pPHVS;
         iPacketIndexGood = i;
      }
   }

   if ( -1 == iPacketIndexGood )
      return;

   // To fix
   /*
   if ( s_FECInfo.missing_packets_count > g_PD_ControllerLinkStats.tmp_video_streams_blocks_max_ec_packets_used[0] )
   {
      // missing packets in a block can't be larger than 8 bits (config values for max EC/Data pachets)
      g_PD_ControllerLinkStats.tmp_video_streams_blocks_max_ec_packets_used[0] = s_FECInfo.missing_packets_count;
   }
   */

   // Add the needed FEC packets to the list
   int pos = 0;
   int iECDelta = m_VideoBlocks[iBufferIndex].iBlockDataPackets;
   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockECPackets; i++ )
   {
      if ( ! m_VideoBlocks[iBufferIndex].packets[i+iECDelta].bEmpty )
      {
         s_FECRxInfo.fec_decode_fec_packets_pointers[pos] = m_VideoBlocks[iBufferIndex].packets[i+iECDelta].pVideoData;
         s_FECRxInfo.fec_decode_fec_indexes[pos] = i;
         pos++;
         if ( pos == (int)(s_FECRxInfo.missing_packets_count) )
            break;
      }
   }

   fec_decode(m_VideoBlocks[iBufferIndex].iBlockDataSize, s_FECRxInfo.fec_decode_data_packets_pointers, m_VideoBlocks[iBufferIndex].iBlockDataPackets, s_FECRxInfo.fec_decode_fec_packets_pointers, s_FECRxInfo.fec_decode_fec_indexes, s_FECRxInfo.fec_decode_missing_packets_indexes, s_FECRxInfo.missing_packets_count);
   
   // Mark all data packets reconstructed as received, set the right info in them (packet header info and video packet header info)
   for( int i=0; i<(int)(s_FECRxInfo.missing_packets_count); i++ )
   {
      int iPacketIndexToFix = s_FECRxInfo.fec_decode_missing_packets_indexes[i];
      m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].bEmpty = false;
      m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].bOutputed = false;
      m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].uReceivedTime = g_TimeNow;
      m_VideoBlocks[iBufferIndex].iRecvDataPackets++;

      t_packet_header* pPHToFix = m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].pPH;
      t_packet_header_video_segment* pPHVSToFix = m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].pPHVS;
      memcpy(pPHToFix, pPHGood, sizeof(t_packet_header));
      memcpy(pPHVSToFix, pPHVSGood, sizeof(t_packet_header_video_segment));
      
      pPHToFix->total_length = sizeof(t_packet_header) + sizeof(t_packet_header_video_segment) + pPHVSGood->uCurrentBlockPacketSize;
      pPHToFix->packet_flags &= ~PACKET_FLAGS_BIT_RETRANSMITED;
      pPHToFix->stream_packet_idx = (pPHGood->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_INDEX) | (((pPHGood->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX) + (iPacketIndexToFix-iPacketIndexGood)) & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
      pPHToFix->radio_link_packet_index = pPHGood->radio_link_packet_index + (iPacketIndexToFix-iPacketIndexGood);

      pPHVSToFix->uCurrentBlockPacketIndex = iPacketIndexToFix;
      pPHVSToFix->uStreamInfoFlags = 0;
      pPHVSToFix->uStreamInfo = 0;
   }
}

// Returns true if the packet has the highest video block/packet index received (in order)
bool VideoRxPacketsBuffer::_add_video_packet_to_buffer(int iBufferIndex, u8* pPacket, int iPacketLength)
{
   if ( (NULL == pPacket) || (iPacketLength < (int)(sizeof(t_packet_header)+sizeof(t_packet_header_video_segment) + sizeof(t_packet_header_video_segment_important))) || (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) )
      return false;

   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*)(pPacket + sizeof(t_packet_header));
   t_packet_header_video_segment_important* pPHVSImp = (t_packet_header_video_segment_important*)(pPacket + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment));


   // Set basic video block info before check allocate block in buffer as it needs block info about packets   
   m_VideoBlocks[iBufferIndex].uVideoBlockIndex = pPHVS->uCurrentBlockIndex;
   m_VideoBlocks[iBufferIndex].iBlockDataSize = pPHVS->uCurrentBlockPacketSize;
   m_VideoBlocks[iBufferIndex].iBlockDataPackets = pPHVS->uCurrentBlockDataPackets;
   m_VideoBlocks[iBufferIndex].iBlockECPackets = pPHVS->uCurrentBlockECPackets;
   m_VideoBlocks[iBufferIndex].uReceivedTime = g_TimeNow;

   if ( ! _check_allocate_video_block_in_buffer(iBufferIndex) )
      return false;
   
   if ( ! m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bEmpty )
      return false;
   if ( m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bOutputed )
      return false;

   if ( pPHVS->uCurrentBlockIndex > m_uMaxVideoBlockIndexPresentInBuffer )
   {
      m_uMaxVideoBlockIndexPresentInBuffer = pPHVS->uCurrentBlockIndex;
      m_uMaxVideoBlockPacketIndexPresentInBuffer = pPHVS->uCurrentBlockPacketIndex;
   }
   else if ( (pPHVS->uCurrentBlockIndex == m_uMaxVideoBlockIndexPresentInBuffer) && (pPHVS->uCurrentBlockPacketIndex > m_uMaxVideoBlockPacketIndexPresentInBuffer) )
      m_uMaxVideoBlockPacketIndexPresentInBuffer = pPHVS->uCurrentBlockPacketIndex;

   if ( m_bBuffersAreEmpty )
      log_line("[VRXBuffers] Start adding video packets to empty buffer. Adding [%u/%u] at buffer index %d, max received video block index so far: %u",
         pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex, iBufferIndex, m_uMaxVideoBlockIndexReceived);
   
   m_bBuffersAreEmpty = false;
   // Begin - Update Runtime Stats

   g_SMControllerRTInfo.uSliceUpdateTime[g_SMControllerRTInfo.iCurrentIndex] = g_TimeNow;
   if ( pPHVS->uCurrentBlockPacketIndex < pPHVS->uCurrentBlockDataPackets )
      g_SMControllerRTInfo.uRecvVideoDataPackets[g_SMControllerRTInfo.iCurrentIndex]++;
   else
      g_SMControllerRTInfo.uRecvVideoECPackets[g_SMControllerRTInfo.iCurrentIndex]++;

   // To fix
   //if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_IFRAME )
   //   g_SMControllerRTInfo.uRecvFramesInfo[g_SMControllerRTInfo.iCurrentIndex] |= 0b01;
   //else
   //   g_SMControllerRTInfo.uRecvFramesInfo[g_SMControllerRTInfo.iCurrentIndex] |= 0b10;

   // To fix
   /*
   if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_END_FRAME )
   {
      if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_IFRAME )
         g_SMControllerRTInfo.uRecvFramesInfo[g_SMControllerRTInfo.iCurrentIndex] |= 0b0100;
      else
         g_SMControllerRTInfo.uRecvFramesInfo[g_SMControllerRTInfo.iCurrentIndex] |= 0b1000;
   }
   */
   // End - Update Runtime Stats

   m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bEmpty = false;
   m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bOutputed = false;
   
   memcpy(m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].pRawData, pPacket, iPacketLength);
   
   // Set remaining empty space to 0 as EC uses the good video data packets too.
   
   u8* pVideoSource = m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].pVideoData;
   pVideoSource += sizeof(t_packet_header_video_segment_important);
   pVideoSource += pPHVSImp->uVideoDataLength;
   int iSizeToZero = MAX_PACKET_TOTAL_SIZE - sizeof(t_packet_header) - sizeof(t_packet_header_video_segment) - sizeof(t_packet_header_video_segment_important) - pPHVSImp->uVideoDataLength;
   if ( iSizeToZero > 0 )
      memset(pVideoSource, 0, iSizeToZero);

   if ( pPHVS->uCurrentBlockPacketIndex < pPHVS->uCurrentBlockDataPackets )
      m_VideoBlocks[iBufferIndex].iRecvDataPackets++;
   else
      m_VideoBlocks[iBufferIndex].iRecvECPackets++;

   m_bFrameEndDetected = false;

   if ( (pPHVS->uCurrentBlockIndex > m_uMaxVideoBlockIndexReceived) || 
        ((pPHVS->uCurrentBlockIndex == m_uMaxVideoBlockIndexReceived) &&
         (pPHVS->uCurrentBlockPacketIndex > m_uMaxVideoBlockPacketIndexReceived)) )
   {
      m_uMaxVideoBlockIndexReceived = pPHVS->uCurrentBlockIndex;
      m_uMaxVideoBlockPacketIndexReceived = pPHVS->uCurrentBlockPacketIndex;

      if ( pPHVSImp->uFrameAndNALFlags & VIDEO_PACKET_FLAGS_IS_END_OF_TRANSMISSION_FRAME )
      {
         m_bFrameEndDetected = true;
         m_uFrameEndDetectedTime = g_TimeNow;
      }
      return true;
   }

   return false;
}
      
// Returns true if the packet has the highest video block/packet index received (in order)
bool VideoRxPacketsBuffer::checkAddVideoPacket(u8* pPacket, int iPacketLength)
{
   if ( (NULL == pPacket) || (iPacketLength <= (int)(sizeof(t_packet_header)+sizeof(t_packet_header_video_segment) + sizeof(t_packet_header_video_segment_important))) )
      return false;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*)(pPacket + sizeof(t_packet_header));
  
   // Empty buffers? (never received anything?)

   if ( -1 == m_iBufferIndexFirstReceivedBlock )
   {
      // Add packet if it's the first packet from the block. Discard if not first from block
      if ( 0 != pPHVS->uCurrentBlockPacketIndex )
        return false;
  
      m_iBufferIndexFirstReceivedBlock = 0;
      m_iBufferIndexFirstReceivedPacketIndex = 0;
      return _add_video_packet_to_buffer(m_iBufferIndexFirstReceivedBlock, pPacket, iPacketLength);
   }

   // Non-empty buffer from now on

   // Too old video block?
   
   if ( pPHVS->uCurrentBlockIndex < m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
   {
      // Discard it and detect restart of vehicle
      if ( ! (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
      if ( pPHVS->uCurrentBlockIndex + 100 < m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
         _empty_buffers("received video block too old, vehicle was restarted", pPH, pPHVS);
      return false;
   }

   // Find delta from start of buffer (first received video block in buffer)
   u32 uDeltaVideoBlocks = pPHVS->uCurrentBlockIndex - m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex;

   // No more room? Discard all buffers as the time difference is too big since first video block in buffer
   if ( uDeltaVideoBlocks >= MAX_RXTX_BLOCKS_BUFFER-5 )
   {
      _empty_buffers("no more room", pPH, pPHVS);
      if ( 0 != pPHVS->uCurrentBlockPacketIndex )
         return false;

      m_iBufferIndexFirstReceivedBlock = 0;
      m_iBufferIndexFirstReceivedPacketIndex = 0;
      return _add_video_packet_to_buffer(m_iBufferIndexFirstReceivedBlock, pPacket, iPacketLength);
   }

   // Set all video blocks indexes from first video block in buffer to this one

   u32 uVideoBlockIndex = m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex+1;
   int iTargetBufferIndex = m_iBufferIndexFirstReceivedBlock+1;
   if ( iTargetBufferIndex >= MAX_RXTX_BLOCKS_BUFFER )
      iTargetBufferIndex = 0;
   for( int i=0; i<(int)uDeltaVideoBlocks; i++ )
   {
      m_VideoBlocks[iTargetBufferIndex].uVideoBlockIndex = uVideoBlockIndex;
      m_VideoBlocks[iTargetBufferIndex].uReceivedTime = g_TimeNow;
      m_VideoBlocks[iTargetBufferIndex].iBlockDataSize = pPHVS->uCurrentBlockPacketSize;
      m_VideoBlocks[iTargetBufferIndex].iBlockDataPackets = pPHVS->uCurrentBlockDataPackets;
      m_VideoBlocks[iTargetBufferIndex].iBlockECPackets = pPHVS->uCurrentBlockECPackets;

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

   if ( pPHVS->uCurrentBlockECPackets > 0 )
   if ( pPHVS->uCurrentBlockPacketIndex >= pPHVS->uCurrentBlockDataPackets )
      _check_do_ec_for_video_block(iTargetBufferIndex);

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
      _check_do_ec_for_video_block(iTargetBufferIndex);
   return bReturn;
}

u32 VideoRxPacketsBuffer::getMaxReceivedVideoBlockIndex()
{
   return m_uMaxVideoBlockIndexReceived;
}

u32 VideoRxPacketsBuffer::getMaxReceivedVideoBlockPacketIndex()
{
   return m_uMaxVideoBlockPacketIndexReceived;
}

u32 VideoRxPacketsBuffer::getMaxReceivedVideoBlockIndexPresentInBuffer()
{
   return m_uMaxVideoBlockIndexPresentInBuffer;
}

u32 VideoRxPacketsBuffer::getMaxReceivedVideoBlockPacketIndexPresentInBuffer()
{
   return m_uMaxVideoBlockPacketIndexPresentInBuffer;
}


bool VideoRxPacketsBuffer::hasFirstVideoPacketInBuffer()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return false;
   return ! m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].packets[m_iBufferIndexFirstReceivedPacketIndex].bEmpty;
}

u32 VideoRxPacketsBuffer::getFirstVideoBlockIndexPresentInBuffer()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return 0;
   return m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex;
}

bool VideoRxPacketsBuffer::hasIncompleteBlocks()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return false;

   if ( m_uMaxVideoBlockIndexPresentInBuffer > m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
      return true;
   return false;
}

int VideoRxPacketsBuffer::getBlocksCountInBuffer()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return 0;

   if ( m_uMaxVideoBlockIndexPresentInBuffer == m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
      return 1;

   int iCountBlocks = 1;
   int iBlockIndex = m_iBufferIndexFirstReceivedBlock;
   while ( m_VideoBlocks[iBlockIndex].uVideoBlockIndex != m_uMaxVideoBlockIndexPresentInBuffer )
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

type_rx_video_block_info* VideoRxPacketsBuffer::getLastVideoBlockInBuffer()
{
   if ( -1 == m_iBufferIndexFirstReceivedBlock )
      return NULL;
   if ( m_uMaxVideoBlockIndexPresentInBuffer == m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
      return &(m_VideoBlocks[m_iBufferIndexFirstReceivedBlock]);

   int iCountBlocks = 0;
   int iBlockIndex = m_iBufferIndexFirstReceivedBlock;
   while ( m_VideoBlocks[iBlockIndex].uVideoBlockIndex != m_uMaxVideoBlockIndexPresentInBuffer )
   {
      iBlockIndex++;
      if ( iBlockIndex >= MAX_RXTX_BLOCKS_BUFFER )
         iBlockIndex = 0;
      if ( iBlockIndex == m_iBufferIndexFirstReceivedBlock )
         break;

      iCountBlocks++;
      if ( iCountBlocks >= MAX_RXTX_BLOCKS_BUFFER )
         break;
   }
   return &(m_VideoBlocks[iBlockIndex]);
}

type_rx_video_block_info* VideoRxPacketsBuffer::getVideoBlockInBuffer(int iStartPosition)
{
   if ( m_iBufferIndexFirstReceivedBlock < 0 )
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


// Returns the number of incomplete blocks that where skipped
int VideoRxPacketsBuffer::advanceStartPositionToVideoBlock(u32 uVideoBlockIndex)
{
   if ( uVideoBlockIndex > m_uMaxVideoBlockIndexPresentInBuffer )
      uVideoBlockIndex = m_uMaxVideoBlockIndexPresentInBuffer+1;

   if ( m_iBufferIndexFirstReceivedBlock < 0 )
      return 0;

   // Skip until the desired block or first that can be outputed or the end
   int iSkippedBlocks = 0;
   while ( m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex != uVideoBlockIndex )
   {
      // This can be outputed. Stop here
      if ( m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iRecvDataPackets + m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iRecvECPackets >= m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iBlockDataPackets )
         break;

      iSkippedBlocks++;
      
      // Discard first block in buffer
      int iCountBlocks = getBlocksCountInBuffer();
      type_rx_video_block_info* pVideoBlockLast = getVideoBlockInBuffer(iCountBlocks-1);
   
      log_line("[VRXBuffers] Discard first block in buffer: buff index: %d, video block: %u, has %d/%d video/ec packets, recv time: %u. Buffer has %d (of %d max) video blocks in buffer. Newest video block id: %u, has %d/%d recv data/ec packets, recv time: %u, (diff from first: %u ms)",
         m_iBufferIndexFirstReceivedBlock, m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex,
         m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iRecvDataPackets,
         m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].iRecvECPackets,
         m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uReceivedTime,
         iCountBlocks, MAX_RXTX_BLOCKS_BUFFER,
         (NULL != pVideoBlockLast)?(pVideoBlockLast->uVideoBlockIndex): 0,
         (NULL != pVideoBlockLast)?(pVideoBlockLast->iRecvDataPackets): 0,
         (NULL != pVideoBlockLast)?(pVideoBlockLast->iRecvECPackets): 0,
         (NULL != pVideoBlockLast)?(pVideoBlockLast->uReceivedTime): 0,
         (NULL != pVideoBlockLast)?(pVideoBlockLast->uReceivedTime - m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uReceivedTime): 0
         );
      
      _empty_block_buffer_index(m_iBufferIndexFirstReceivedBlock);
      m_iBufferIndexFirstReceivedPacketIndex = 0;
      m_iBufferIndexFirstReceivedBlock++;
      if ( m_iBufferIndexFirstReceivedBlock >= MAX_RXTX_BLOCKS_BUFFER )
         m_iBufferIndexFirstReceivedBlock = 0;

      // Reached end of received blocks. Buffers are completly empty now
      if ( 0 == m_VideoBlocks[m_iBufferIndexFirstReceivedBlock].uVideoBlockIndex )
      {
         m_iBufferIndexFirstReceivedBlock = -1;
         m_iBufferIndexFirstReceivedPacketIndex = -1;
         break;
      }
   }
   
   return iSkippedBlocks;
}

void VideoRxPacketsBuffer::resetFrameEndDetectedFlag()
{
   m_bFrameEndDetected = false;
   m_uFrameEndDetectedTime = 0;
}

bool VideoRxPacketsBuffer::isFrameEndDetected()
{
   return m_bFrameEndDetected;
}

u32 VideoRxPacketsBuffer::getFrameEndDetectionTime()
{
   return m_uFrameEndDetectedTime;
}
