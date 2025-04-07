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
   m_iTopBufferIndex = 0;
   m_iBottomBufferIndexToOutput = 0;
   m_iBottomPacketIndexToOutput = 0;
   m_iCountBlocksPresent = 0;
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
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].bReconstructed = false;
   m_VideoBlocks[iBufferIndex].packets[iPacketIndex].bOutputed = false;
}

void VideoRxPacketsBuffer::_empty_block_buffer_index(int iBufferIndex)
{
   m_VideoBlocks[iBufferIndex].uVideoBlockIndex = 0;
   m_VideoBlocks[iBufferIndex].bEmpty = true;
   m_VideoBlocks[iBufferIndex].uReceivedTime = 0;
   m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex = -1;
   m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex = -1;
   m_VideoBlocks[iBufferIndex].iEndOfFrameDetectedAtPacketIndex = -1;
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
      log_softerror_and_alarm("%s (no additional data)", szLog);
   else
   {
      log_softerror_and_alarm("%s (recv video packet [%u/%u] (retransmitted: %s), frame index: %u, stream id/packet index: %u, radio index: %u)",
         szLog, pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex,
         (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED)?"yes":"no",
         pPHVS->uH264FrameIndex,
         (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_INDEX) >> PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX,
         pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX, pPH->radio_link_packet_index);
   
      log_softerror_and_alarm("[VRXBuffers] Top video block index: %d, top data packet received: [%u/%d], top max data/ec packet received: [%u/%d]",
         m_iTopBufferIndex,
         m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex, m_VideoBlocks[m_iTopBufferIndex].iMaxReceivedDataPacketIndex,
         m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex, m_VideoBlocks[m_iTopBufferIndex].iMaxReceivedDataOrECPacketIndex);
      log_softerror_and_alarm("[VRXBuffers] Bottom block buffer index: %d, packet: [%u/%d]",
         m_iBottomBufferIndexToOutput, m_VideoBlocks[m_iBottomBufferIndexToOutput].uVideoBlockIndex, m_iBottomPacketIndexToOutput);
      log_softerror_and_alarm("[VRXBuffers] Bottom block buffer index: %d, received packets: %d/%d",
         m_iBottomBufferIndexToOutput, m_VideoBlocks[m_iBottomBufferIndexToOutput].iRecvDataPackets, m_VideoBlocks[m_iBottomBufferIndexToOutput].iRecvECPackets);
   }

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
      _empty_block_buffer_index(i);

   g_SMControllerRTInfo.uOutputedVideoPacketsSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex]++;
   if ( g_TimeNow > g_TimeLastVideoParametersOrProfileChanged + 3000 )
   if ( g_TimeNow > g_TimeStart + 5000 )
      g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks++;

   m_iTopBufferIndex = 0;
   m_iBottomBufferIndexToOutput = 0;
   m_iBottomPacketIndexToOutput = 0;
   m_iCountBlocksPresent = 0;
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
   // Find a good PH, PHVF and video-debug-info (if any) in the block to reuse it in reconstruction

   /*
   log_line("DBG doing ec for block %u, recv data/ec: %d/%d, scheme: %d/%d, max recv index: %d, eof: %d",
      m_VideoBlocks[iBufferIndex].uVideoBlockIndex,
      m_VideoBlocks[iBufferIndex].iRecvDataPackets, m_VideoBlocks[iBufferIndex].iRecvECPackets,
      m_VideoBlocks[iBufferIndex].iBlockDataPackets, m_VideoBlocks[iBufferIndex].iBlockECPackets,
      m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex,
      m_VideoBlocks[iBufferIndex].iEndOfFrameDetectedAtPacketIndex);
   */
   m_ECRxInfo.missing_packets_count = 0;
   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockDataPackets; i++ )
   {
      m_ECRxInfo.p_decode_data_packets_pointers[i] = m_VideoBlocks[iBufferIndex].packets[i].pVideoData;

      if ( m_VideoBlocks[iBufferIndex].packets[i].bEmpty )
      {
         m_ECRxInfo.decode_missing_packets_indexes[m_ECRxInfo.missing_packets_count] = i;
         m_ECRxInfo.missing_packets_count++;
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

   // Add the needed FEC packets to the list
   int pos = 0;
   int iECDelta = m_VideoBlocks[iBufferIndex].iBlockDataPackets;
   for( int i=0; i<m_VideoBlocks[iBufferIndex].iBlockECPackets; i++ )
   {
      if ( ! m_VideoBlocks[iBufferIndex].packets[i+iECDelta].bEmpty )
      {
         m_ECRxInfo.p_decode_ec_packets_pointers[pos] = m_VideoBlocks[iBufferIndex].packets[i+iECDelta].pVideoData;
         m_ECRxInfo.decode_ec_packets_indexes[pos] = i;
         pos++;
         if ( pos == (int)(m_ECRxInfo.missing_packets_count) )
            break;
      }
   }

   int iRes = fec_decode(m_VideoBlocks[iBufferIndex].iBlockDataSize, m_ECRxInfo.p_decode_data_packets_pointers, m_VideoBlocks[iBufferIndex].iBlockDataPackets, m_ECRxInfo.p_decode_ec_packets_pointers, m_ECRxInfo.decode_ec_packets_indexes, m_ECRxInfo.decode_missing_packets_indexes, m_ECRxInfo.missing_packets_count);
   if ( iRes < 0 )
   {
      log_softerror_and_alarm("[VideoRXBuffer] Failed to decode video block [%u], type %d/%d/%d bytes; max data recv index: %d, max data/ec received index: %d, eoframe-index: %d; recv: %d/%d packets, missing count: %d",
        m_VideoBlocks[iBufferIndex].uVideoBlockIndex,
        m_VideoBlocks[iBufferIndex].iBlockDataPackets, m_VideoBlocks[iBufferIndex].iBlockECPackets,
        m_VideoBlocks[iBufferIndex].iBlockDataSize,
        m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex,
        m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex,
        m_VideoBlocks[iBufferIndex].iEndOfFrameDetectedAtPacketIndex,
        m_VideoBlocks[iBufferIndex].iRecvDataPackets, m_VideoBlocks[iBufferIndex].iRecvECPackets,
        m_ECRxInfo.missing_packets_count);
   }

   // Mark all data packets reconstructed as received, set the right info in them (packet header info and video packet header info)
   for( int i=0; i<(int)(m_ECRxInfo.missing_packets_count); i++ )
   {
      int iPacketIndexToFix = m_ECRxInfo.decode_missing_packets_indexes[i];
      m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].bEmpty = false;
      m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].bReconstructed = true;
      m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].bOutputed = false;
      m_VideoBlocks[iBufferIndex].packets[iPacketIndexToFix].uReceivedTime = g_TimeNow;
      m_VideoBlocks[iBufferIndex].iRecvDataPackets++;
      if ( iPacketIndexToFix > m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex )
         m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex = iPacketIndexToFix;
      if ( iPacketIndexToFix > m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex )
         m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex = iPacketIndexToFix;


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

void VideoRxPacketsBuffer::_add_video_packet_to_buffer(int iBufferIndex, u8* pPacket, int iPacketLength)
{
   if ( (NULL == pPacket) || (iPacketLength < (int)(sizeof(t_packet_header)+sizeof(t_packet_header_video_segment) + sizeof(t_packet_header_video_segment_important))) || (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) )
      return;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*)(pPacket + sizeof(t_packet_header));
   t_packet_header_video_segment_important* pPHVSImp = (t_packet_header_video_segment_important*)(pPacket + sizeof(t_packet_header) + sizeof(t_packet_header_video_segment));

   if ( NULL != m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].pRawData )
   if ( ! m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bEmpty )
      return;

   if ( NULL != m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].pRawData )
   if ( m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bOutputed )
      return;

   // Set basic video block info before check allocate block in buffer as it needs block info about packets   
   m_VideoBlocks[iBufferIndex].uVideoBlockIndex = pPHVS->uCurrentBlockIndex;
   m_VideoBlocks[iBufferIndex].uReceivedTime = g_TimeNow;
   if ( pPHVS->uCurrentBlockPacketIndex > m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex )
   {
      if ( (pPHVS->uCurrentBlockDataPackets != m_VideoBlocks[iBufferIndex].iBlockDataPackets) ||
           (pPHVS->uCurrentBlockECPackets != m_VideoBlocks[iBufferIndex].iBlockECPackets) )
      {
         for(u8 u=0; u<pPHVS->uCurrentBlockPacketIndex; u++)
         {
            if ( ! m_VideoBlocks[iBufferIndex].packets[u].bEmpty )
            {
               m_VideoBlocks[iBufferIndex].packets[u].pPHVS->uCurrentBlockDataPackets = pPHVS->uCurrentBlockDataPackets;
               m_VideoBlocks[iBufferIndex].packets[u].pPHVS->uCurrentBlockECPackets = pPHVS->uCurrentBlockECPackets;
            }
         }
      }
      m_VideoBlocks[iBufferIndex].iBlockDataPackets = pPHVS->uCurrentBlockDataPackets;
      m_VideoBlocks[iBufferIndex].iBlockECPackets = pPHVS->uCurrentBlockECPackets;
      m_VideoBlocks[iBufferIndex].iBlockDataSize = pPHVS->uCurrentBlockPacketSize;
   }
   if ( ! _check_allocate_video_block_in_buffer(iBufferIndex) )
      return;

   if ( m_VideoBlocks[m_iTopBufferIndex].bEmpty )
      log_line("[VRXBuffers] Start adding video packets to empty buffer. Adding [%u/%u] at buffer index %d",
         pPHVS->uCurrentBlockIndex, pPHVS->uCurrentBlockPacketIndex, m_iTopBufferIndex);
   
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

   m_VideoBlocks[iBufferIndex].bEmpty = false;
   if ( pPHVS->uCurrentBlockPacketIndex < pPHVS->uCurrentBlockDataPackets )
   {
      m_VideoBlocks[iBufferIndex].iRecvDataPackets++;
      if ( (int)pPHVS->uCurrentBlockPacketIndex > m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex )
         m_VideoBlocks[iBufferIndex].iMaxReceivedDataPacketIndex = (int)pPHVS->uCurrentBlockPacketIndex;
      if ( (int)pPHVS->uCurrentBlockPacketIndex > m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex )
         m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex = (int)pPHVS->uCurrentBlockPacketIndex;
   }
   else
   {
      m_VideoBlocks[iBufferIndex].iRecvECPackets++;
      if ( (int)pPHVS->uCurrentBlockPacketIndex > m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex )
         m_VideoBlocks[iBufferIndex].iMaxReceivedDataOrECPacketIndex = (int)pPHVS->uCurrentBlockPacketIndex;
   }

   if ( ! (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
   if ( pPHVS->uCurrentBlockPacketIndex < pPHVS->uCurrentBlockDataPackets )
   if ( (pPHVSImp->uFrameAndNALFlags & VIDEO_PACKET_FLAGS_IS_END_OF_TRANSMISSION_FRAME) ||
        (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_END) )
   {
      m_bFrameEndDetected = true;
      m_uFrameEndDetectedTime = g_TimeNow;
      m_VideoBlocks[iBufferIndex].iEndOfFrameDetectedAtPacketIndex = ((int)pPHVS->uCurrentBlockPacketIndex) + (int)(pPHVSImp->uFrameAndNALFlags & 0x03);
   }
   if ( pPHVS->uCurrentBlockPacketIndex >= pPHVS->uCurrentBlockDataPackets )
   if ( (m_VideoBlocks[iBufferIndex].iEndOfFrameDetectedAtPacketIndex != -1) ||
        (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_END) )
   {
      m_bFrameEndDetected = true;
      m_uFrameEndDetectedTime = g_TimeNow;
   }
   if ( pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_END )
   if ( (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_I) || (pPHVS->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_NAL_P) )
   {
      m_bFrameEndDetected = true;
      m_uFrameEndDetectedTime = g_TimeNow;    
   }

   m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].uReceivedTime = g_TimeNow;
   m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bEmpty = false;
   m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bReconstructed = false;
   m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].bOutputed = false;
   
   memcpy(m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].pRawData, pPacket, iPacketLength);
   
   // Set remaining empty space to 0 as EC uses the good video data packets too.
   if ( pPHVS->uCurrentBlockPacketIndex < pPHVS->uCurrentBlockDataPackets )
   {
      u8* pVideoSource = m_VideoBlocks[iBufferIndex].packets[pPHVS->uCurrentBlockPacketIndex].pVideoData;
      pVideoSource += sizeof(t_packet_header_video_segment_important);
      pVideoSource += pPHVSImp->uVideoDataLength;
      int iSizeToZero = MAX_PACKET_TOTAL_SIZE - sizeof(t_packet_header) - sizeof(t_packet_header_video_segment) - sizeof(t_packet_header_video_segment_important) - pPHVSImp->uVideoDataLength;
      if ( iSizeToZero > 0 )
         memset(pVideoSource, 0, iSizeToZero);
   }
   _check_do_ec_for_video_block(iBufferIndex);
}
      
// Returns true if the packet has the highest video block/packet index received (in order)
bool VideoRxPacketsBuffer::checkAddVideoPacket(u8* pPacket, int iPacketLength)
{
   if ( (NULL == pPacket) || (iPacketLength <= (int)(sizeof(t_packet_header)+sizeof(t_packet_header_video_segment) + sizeof(t_packet_header_video_segment_important))) )
      return false;

   t_packet_header* pPH = (t_packet_header*)pPacket;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*)(pPacket + sizeof(t_packet_header));
  
   // Empty buffers?
   if ( m_VideoBlocks[m_iTopBufferIndex].bEmpty )
   {
      // Wait for start of a block
      if ( 0 != pPHVS->uCurrentBlockPacketIndex )
         return false;
      m_iCountBlocksPresent = 1;
      _add_video_packet_to_buffer(m_iTopBufferIndex, pPacket, iPacketLength);
      return true;
   }

   // Top block is not empty
   // Non-empty buffers from now on

   // On the current top block?
   if ( m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex == pPHVS->uCurrentBlockIndex )
   {
      _add_video_packet_to_buffer(m_iTopBufferIndex, pPacket, iPacketLength);
      if ( pPHVS->uCurrentBlockPacketIndex >= m_VideoBlocks[m_iTopBufferIndex].iMaxReceivedDataOrECPacketIndex )
         return true;
      return false;
   }

   // Stream restarted?
   if ( m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex > 200 )
   if ( pPHVS->uCurrentBlockIndex < m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex - 200 )
   {
      _empty_buffers("stream restarted", pPH, pPHVS);
      // Wait for start of a block
      if ( pPHVS->uCurrentBlockPacketIndex != 0 )
         return false;
      m_iCountBlocksPresent = 1;
      _add_video_packet_to_buffer(m_iTopBufferIndex, pPacket, iPacketLength);
      return true;
   }

   // Compute difference from top

   // Old block packet (older than top block)?

   if ( pPHVS->uCurrentBlockIndex < m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex )
   {
      if ( pPHVS->uCurrentBlockIndex < m_VideoBlocks[m_iBottomBufferIndexToOutput].uVideoBlockIndex )
         return false;

      u32 uDiffBlocks = m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex - pPHVS->uCurrentBlockIndex;
      if ( uDiffBlocks >= MAX_RXTX_BLOCKS_BUFFER - 1 )
         return false;

      int iBufferIndex = m_iTopBufferIndex - (int) uDiffBlocks;
      if ( iBufferIndex < 0 )
         iBufferIndex += MAX_RXTX_BLOCKS_BUFFER;
      _add_video_packet_to_buffer(iBufferIndex, pPacket, iPacketLength);
      return false;
   }

   // Future block packet

   u32 uDiffBlocks = pPHVS->uCurrentBlockIndex - m_VideoBlocks[m_iBottomBufferIndexToOutput].uVideoBlockIndex;
   if ( uDiffBlocks >= MAX_RXTX_BLOCKS_BUFFER - 1 )
   {
      _empty_buffers("too much gap in received blocks", pPH, pPHVS);

      if ( pPHVS->uCurrentBlockPacketIndex != 0 )
         return false;
      m_iCountBlocksPresent = 1;
      _add_video_packet_to_buffer(m_iTopBufferIndex, pPacket, iPacketLength);
      return true;
   }

   uDiffBlocks = pPHVS->uCurrentBlockIndex - m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex;

   for(u32 uBlk=m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex+1; uBlk<=pPHVS->uCurrentBlockIndex; uBlk++)
   {
      m_iTopBufferIndex++;
      if ( m_iTopBufferIndex >= MAX_RXTX_BLOCKS_BUFFER )
         m_iTopBufferIndex = 0;

      if ( m_iTopBufferIndex == m_iBottomBufferIndexToOutput )
      {
         _empty_buffers("too old data in stream, inconsistent.", pPH, pPHVS);
         // Wait for start of a block
         if ( pPHVS->uCurrentBlockPacketIndex != 0 )
            return false;
         m_iCountBlocksPresent = 1;
         _add_video_packet_to_buffer(m_iTopBufferIndex, pPacket, iPacketLength);
         return true;
      }
      if ( ! _check_allocate_video_block_in_buffer(m_iTopBufferIndex) )
         return false;
      m_iCountBlocksPresent++;
      _empty_block_buffer_index(m_iTopBufferIndex);
      m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex = uBlk;
      m_VideoBlocks[m_iTopBufferIndex].bEmpty = false;
      m_VideoBlocks[m_iTopBufferIndex].uReceivedTime = g_TimeNow;
      m_VideoBlocks[m_iTopBufferIndex].iBlockDataSize = pPHVS->uCurrentBlockPacketSize;
      m_VideoBlocks[m_iTopBufferIndex].iBlockDataPackets = pPHVS->uCurrentBlockDataPackets;
      m_VideoBlocks[m_iTopBufferIndex].iBlockECPackets = pPHVS->uCurrentBlockECPackets;
      if ( m_iTopBufferIndex == m_iBottomBufferIndexToOutput )
      {
         m_iBottomPacketIndexToOutput = 0;
         m_iBottomBufferIndexToOutput++;
         if ( m_iBottomBufferIndexToOutput >= MAX_RXTX_BLOCKS_BUFFER )
            m_iBottomBufferIndexToOutput = 0;
         m_iCountBlocksPresent--;
      }
   }
   _add_video_packet_to_buffer(m_iTopBufferIndex, pPacket, iPacketLength);
   return true;
}

u32 VideoRxPacketsBuffer::getBufferBottomReceivedVideoBlockIndex()
{
   return m_VideoBlocks[m_iBottomBufferIndexToOutput].uVideoBlockIndex;
}

u32 VideoRxPacketsBuffer::getBufferTopReceivedVideoBlockIndex()
{
   return m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex;
}

int VideoRxPacketsBuffer::getTopBufferMaxReceivedVideoBlockPacketIndex()
{
   return m_VideoBlocks[m_iTopBufferIndex].iMaxReceivedDataOrECPacketIndex;
}

int VideoRxPacketsBuffer::getBlocksCountInBuffer()
{
   if ( m_VideoBlocks[m_iTopBufferIndex].bEmpty )
      return 0;

   return m_iCountBlocksPresent;
   /*
   if ( m_iTopBufferIndex == m_iBottomBufferIndexToOutput )
      return 1;
   if ( m_iTopBufferIndex > m_iBottomBufferIndexToOutput )
      return m_iTopBufferIndex - m_iBottomBufferIndexToOutput + 1;

   return m_iTopBufferIndex + (MAX_RXTX_BLOCKS_BUFFER - m_iBottomBufferIndexToOutput) + 1;
   */
}

type_rx_video_block_info* VideoRxPacketsBuffer::getVideoBlockInBuffer(int iStartPosition)
{
   int iIndex = m_iBottomBufferIndexToOutput + iStartPosition;
   if ( iIndex >= MAX_RXTX_BLOCKS_BUFFER )
      iIndex -= MAX_RXTX_BLOCKS_BUFFER;
   return &(m_VideoBlocks[iIndex]);
}

type_rx_video_packet_info* VideoRxPacketsBuffer::getFirstPacketInBuffer(type_rx_video_block_info** ppOutputBlock)
{
   if ( NULL != ppOutputBlock )
      *ppOutputBlock = NULL;

   if ( m_VideoBlocks[m_iTopBufferIndex].bEmpty )
      return NULL;

   if ( m_iBottomBufferIndexToOutput == m_iTopBufferIndex )
   if ( m_iBottomPacketIndexToOutput > m_VideoBlocks[m_iTopBufferIndex].iMaxReceivedDataOrECPacketIndex )
      return NULL;

   if ( NULL != ppOutputBlock )
      *ppOutputBlock = &(m_VideoBlocks[m_iBottomBufferIndexToOutput]);
   
   return &(m_VideoBlocks[m_iBottomBufferIndexToOutput].packets[m_iBottomPacketIndexToOutput]);
}

void VideoRxPacketsBuffer::goToNextPacketInBuffer()
{
   // If we are already at the top packet, do nothing
   if ( m_VideoBlocks[m_iTopBufferIndex].bEmpty )
      return;

   if ( m_iBottomBufferIndexToOutput == m_iTopBufferIndex )
   if ( m_iBottomPacketIndexToOutput >= m_VideoBlocks[m_iTopBufferIndex].iMaxReceivedDataPacketIndex )
      return;

   // Will adavance by +1 the bottom rx packet index in buffer (either empty or outputed already)

   if ( m_VideoBlocks[m_iBottomBufferIndexToOutput].packets[m_iBottomPacketIndexToOutput].bEmpty ||
       (!m_VideoBlocks[m_iBottomBufferIndexToOutput].packets[m_iBottomPacketIndexToOutput].bOutputed) )
   {
      g_SMControllerRTInfo.uOutputedVideoPacketsSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex]++;
      if ( g_TimeNow > g_TimeLastVideoParametersOrProfileChanged + 3000 )
      if ( g_TimeNow > g_TimeStart + 5000 )
         g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks++;
   }
   
   m_iBottomPacketIndexToOutput++;

   if ( m_iBottomPacketIndexToOutput >= m_VideoBlocks[m_iBottomBufferIndexToOutput].iBlockDataPackets )
   {
      m_iBottomPacketIndexToOutput = 0;
      m_iBottomBufferIndexToOutput++;
      if ( m_iBottomBufferIndexToOutput >= MAX_RXTX_BLOCKS_BUFFER )
         m_iBottomBufferIndexToOutput = 0;
      m_iCountBlocksPresent--;
   }
}

int VideoRxPacketsBuffer::discardOldBlocks(u32 uCutOffTime)
{
   if ( m_VideoBlocks[m_iTopBufferIndex].bEmpty )
      return 0;

   int iCountDiscarded = 0;
   while ( (m_iBottomBufferIndexToOutput != m_iTopBufferIndex) && (m_iCountBlocksPresent > 1) )
   {
      if ( m_VideoBlocks[m_iBottomBufferIndexToOutput].uReceivedTime > uCutOffTime )
         break;
      /*
      if ( iCountDiscarded < 3 )
         log_line("DBG discard bottom index: %d, block %u (recv time: %u ms ago, recv packets data,ec: %d,%d, scheme: %d/%d), top index: %d, block %u",
            m_iBottomBufferIndexToOutput, m_VideoBlocks[m_iBottomBufferIndexToOutput].uVideoBlockIndex,
            g_TimeNow - m_VideoBlocks[m_iBottomBufferIndexToOutput].uReceivedTime,
            m_VideoBlocks[m_iBottomBufferIndexToOutput].iRecvDataPackets, m_VideoBlocks[m_iBottomBufferIndexToOutput].iRecvECPackets,
            m_VideoBlocks[m_iBottomBufferIndexToOutput].iBlockDataPackets, m_VideoBlocks[m_iBottomBufferIndexToOutput].iBlockECPackets,
            m_iTopBufferIndex, m_VideoBlocks[m_iTopBufferIndex].uVideoBlockIndex);
      */
      iCountDiscarded++;
      _empty_block_buffer_index(m_iBottomBufferIndexToOutput);
      m_iBottomPacketIndexToOutput = 0;
      m_iBottomBufferIndexToOutput++;
      if ( m_iBottomBufferIndexToOutput >= MAX_RXTX_BLOCKS_BUFFER )
         m_iBottomBufferIndexToOutput = 0;
      m_iCountBlocksPresent--;
   }

   return iCountDiscarded;
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
