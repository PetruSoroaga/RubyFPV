/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
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

#include "video_tx_buffers.h"
#include "shared_vars.h"
#include "timers.h"
#include "packets_utils.h"
#include "../common/string_utils.h"
#include "../radio/fec.h"
#include "adaptive_video.h"
#include "processor_tx_video.h"
#include "processor_relay.h"

#define MAX_PACKETS_TO_SEND_IN_ONE_SLICE 40

typedef struct
{
   unsigned int fec_decode_missing_packets_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int fec_decode_fec_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_data_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_fec_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int missing_packets_count;
} type_fec_info;
type_fec_info s_FECDecodeInfo;

int VideoTxPacketsBuffer::m_siVideoBuffersInstancesCount = 0;

u32 s_uTimeTotalFecTimeMicroSec = 0;
u32 s_uTimeFecMicroPerSec = 0;
u32 s_uLastTimeFecCalculation = 0;

VideoTxPacketsBuffer::VideoTxPacketsBuffer(int iVideoStreamIndex, int iCameraIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siVideoBuffersInstancesCount;
   m_siVideoBuffersInstancesCount++;

   m_iVideoStreamIndex = iVideoStreamIndex;
   m_iCameraIndex = iCameraIndex;

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
   {
      m_VideoPackets[i][k].pRawData = NULL;
      m_VideoPackets[i][k].pPH = NULL;
      m_VideoPackets[i][k].pPHVF = NULL;
      m_VideoPackets[i][k].pVideoData = NULL;
   }
   m_uCurrentH264FrameId = 0;
   m_iCurrentBufferIndexToSend = 0;
   m_iCurrentBufferPacketIndexToSend = 0;
   m_iNextBufferIndexToFill = 0;
   m_iNextBufferPacketIndexToFill = 0;
   m_iCountReadyToSend = 0;

   m_uNextVideoBlockIndexToGenerate = 0;
   m_uNextVideoBlockPacketIndexToGenerate = 0;
   m_uRadioStreamPacketIndex = 0;
   m_iVideoStreamInfoIndex = 0;

   memset(&m_PacketHeaderVideo, 0, sizeof(t_packet_header_video_full_98));
   memset(&m_PacketHeaderVideo, 0, sizeof(t_packet_header_video_full_98));

   m_ParserInputH264.init();
   m_uTempBufferNALPresenceFlags = 0;
}

VideoTxPacketsBuffer::~VideoTxPacketsBuffer()
{
   uninit();

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
   {
      if ( NULL != m_VideoPackets[i][k].pRawData )
         free(m_VideoPackets[i][k].pRawData);

      m_VideoPackets[i][k].pRawData = NULL;
      m_VideoPackets[i][k].pPH = NULL;
      m_VideoPackets[i][k].pPHVF = NULL;
      m_VideoPackets[i][k].pVideoData = NULL;
   }

   m_siVideoBuffersInstancesCount--;
}

bool VideoTxPacketsBuffer::init(Model* pModel)
{
   if ( m_bInitialized )
      return true;
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[VideoTXBuffer] Invalid model on init.");
      return false;
   }
   log_line("[VideoTXBuffer] Initialize video Tx buffer instance number %d.", m_iInstanceIndex+1);

   m_uNextVideoBlockIndexToGenerate = 0;
   m_uNextVideoBlockPacketIndexToGenerate = 0;
   updateVideoHeader(pModel);
   
   m_iTempVideoBufferFilledBytes = 0;
   m_iNextBufferIndexToFill = 0;
   m_iNextBufferPacketIndexToFill = 0;
   m_iCurrentBufferIndexToSend = 0;
   m_iCurrentBufferPacketIndexToSend = 0;
   m_iCountReadyToSend = 0;
   m_bInitialized = true;
   log_line("[VideoTXBuffer] Initialized video Tx buffer instance number %d.", m_iInstanceIndex+1);
   return true;
}

bool VideoTxPacketsBuffer::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[VideoTXBuffer] Uninitialize video Tx buffer instance number %d.", m_iInstanceIndex+1);
   
   m_bInitialized = false;
   return true;
}

void VideoTxPacketsBuffer::discardBuffer()
{
   m_uNextVideoBlockIndexToGenerate = 0;
   m_uNextVideoBlockPacketIndexToGenerate = 0;
   
   m_iTempVideoBufferFilledBytes = 0;
   m_iNextBufferIndexToFill = 0;
   m_iNextBufferPacketIndexToFill = 0;
   m_iCurrentBufferIndexToSend = 0;
   m_iCurrentBufferPacketIndexToSend = 0;
   m_iCountReadyToSend = 0;
   
   log_softerror_and_alarm("[VideoTXBuffer] Discarded buffer");
}


void VideoTxPacketsBuffer::_checkAllocatePacket(int iBufferIndex, int iPacketIndex)
{
   if ( (iBufferIndex < 0) || (iBufferIndex >= MAX_RXTX_BLOCKS_BUFFER) || (iPacketIndex < 0) || (iPacketIndex >= MAX_TOTAL_PACKETS_IN_BLOCK) )
      return;
   if ( NULL != m_VideoPackets[iBufferIndex][iPacketIndex].pRawData )
      return;

   m_VideoPackets[iBufferIndex][iPacketIndex].pRawData = (u8*)malloc(MAX_PACKET_TOTAL_SIZE);
   if ( NULL == m_VideoPackets[iBufferIndex][iPacketIndex].pRawData )
   {
      log_error_and_alarm("Failed to allocate video buffer at index: [%d/%d]", iPacketIndex, iBufferIndex);
      return;
   }
   m_VideoPackets[iBufferIndex][iPacketIndex].pPH = (t_packet_header*)&(m_VideoPackets[iBufferIndex][iPacketIndex].pRawData[0]);
   m_VideoPackets[iBufferIndex][iPacketIndex].pPHVF = (t_packet_header_video_full_98*)&(m_VideoPackets[iBufferIndex][iPacketIndex].pRawData[sizeof(t_packet_header)]);
   m_VideoPackets[iBufferIndex][iPacketIndex].pVideoData = &(m_VideoPackets[iBufferIndex][iPacketIndex].pRawData[sizeof(t_packet_header) + sizeof(t_packet_header_video_full_98)]);
}

void VideoTxPacketsBuffer::_fillVideoPacketHeaders(int iBufferIndex, int iPacketIndex, int iVideoSize, u32 uNALPresenceFlags, bool bEndOfTransmissionFrame)
{
   t_packet_header* pCurrentPacketHeader = m_VideoPackets[iBufferIndex][iPacketIndex].pPH;
   memcpy(pCurrentPacketHeader, &m_PacketHeader, sizeof(t_packet_header));

   pCurrentPacketHeader->total_length = sizeof(t_packet_header)+sizeof(t_packet_header_video_full_98);
   pCurrentPacketHeader->total_length += iVideoSize;
   if ( m_PacketHeaderVideo.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
      pCurrentPacketHeader->total_length += sizeof(t_packet_header_video_full_98_debug_info);

   t_packet_header_video_full_98* pCurrentVideoPacketHeader = m_VideoPackets[iBufferIndex][iPacketIndex].pPHVF;
   memcpy(pCurrentVideoPacketHeader, &m_PacketHeaderVideo, sizeof(t_packet_header_video_full_98));
   pCurrentVideoPacketHeader->uH264FrameId = m_uCurrentH264FrameId;
   pCurrentVideoPacketHeader->uH264SliceId = m_uCurrentH264SliceId;
   pCurrentVideoPacketHeader->uCurrentBlockIndex = m_uNextVideoBlockIndexToGenerate;
   pCurrentVideoPacketHeader->uCurrentBlockPacketIndex = m_uNextVideoBlockPacketIndexToGenerate;

   pCurrentVideoPacketHeader->uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_IS_END_OF_TRANSMISSION_FRAME;
   if ( bEndOfTransmissionFrame )
   if ( pCurrentVideoPacketHeader->uCurrentBlockECPackets == 0 )
      pCurrentVideoPacketHeader->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_END_OF_TRANSMISSION_FRAME;
   if ( bEndOfTransmissionFrame )
   if ( ( pCurrentVideoPacketHeader->uCurrentBlockPacketIndex < pCurrentVideoPacketHeader->uCurrentBlockDataPackets) || (pCurrentVideoPacketHeader->uCurrentBlockPacketIndex == pCurrentVideoPacketHeader->uCurrentBlockDataPackets + pCurrentVideoPacketHeader->uCurrentBlockECPackets - 1) )
      pCurrentVideoPacketHeader->uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_END_OF_TRANSMISSION_FRAME;

   pCurrentVideoPacketHeader->uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_MASK_NAL_FLAGS;
   pCurrentVideoPacketHeader->uVideoStatusFlags2 |= uNALPresenceFlags;
   
   //log_line("DEBUG filled video packet [%u/%u], %d bytes, end-of-tr-frame: %d, video-stat-flags: [%s]",
   //   pCurrentVideoPacketHeader->uCurrentBlockIndex,
   //   pCurrentVideoPacketHeader->uCurrentBlockPacketIndex,
   //   iVideoSize, bEndOfTransmissionFrame,
   //   str_format_video_nal_frame_flags_from_status_flags2(pCurrentVideoPacketHeader->uVideoStatusFlags2));
   
   int iVideoProfile = adaptive_video_get_current_active_video_profile();
   pCurrentVideoPacketHeader->uCurrentVideoLinkProfile = iVideoProfile;
   
   m_iVideoStreamInfoIndex++;
   pCurrentVideoPacketHeader->uStreamInfoFlags = (u8)m_iVideoStreamInfoIndex;   
   pCurrentVideoPacketHeader->uStreamInfo = 0;
   u32 uTmp1, uTmp2;

   switch ( m_iVideoStreamInfoIndex )
   {
      case VIDEO_STREAM_INFO_FLAG_NONE:
          pCurrentVideoPacketHeader->uStreamInfo = 0;
          break;

      case VIDEO_STREAM_INFO_FLAG_SIZE:
          uTmp1 = (u32)g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width;
          uTmp2 = (u32)g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height;
          pCurrentVideoPacketHeader->uStreamInfo = (uTmp1 & 0xFFFF) | ((uTmp2 & 0xFFFF)<<16);
          break;

      case VIDEO_STREAM_INFO_FLAG_FPS:
          pCurrentVideoPacketHeader->uStreamInfo = (u32)g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps;
          break;

      case VIDEO_STREAM_INFO_FLAG_FEC_TIME:
          pCurrentVideoPacketHeader->uStreamInfo = s_uTimeFecMicroPerSec;
          break;

      case VIDEO_STREAM_INFO_FLAG_VIDEO_PROFILE_FLAGS:
          pCurrentVideoPacketHeader->uStreamInfo = g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileEncodingFlags;
          break;

      default:
          m_iVideoStreamInfoIndex = 0;
          pCurrentVideoPacketHeader->uStreamInfo = 0;
          break;
   }
}

void VideoTxPacketsBuffer::updateVideoHeader(Model* pModel)
{
   if ( NULL == pModel )
      return;

   radio_packet_init(&m_PacketHeader, PACKET_COMPONENT_VIDEO | PACKET_FLAGS_BIT_HEADERS_ONLY_CRC, PACKET_TYPE_VIDEO_DATA_98, STREAM_ID_VIDEO_1);
   m_PacketHeader.vehicle_id_src = pModel->uVehicleId;
   m_PacketHeader.vehicle_id_dest = 0;

   int iVideoProfile = adaptive_video_get_current_active_video_profile();

   if ( 0 == m_iNextBufferPacketIndexToFill )
   {
      m_PacketHeaderVideo.uCurrentBlockPacketSize = pModel->video_link_profiles[iVideoProfile].video_data_length;
      m_PacketHeaderVideo.uCurrentBlockDataPackets = pModel->video_link_profiles[iVideoProfile].block_packets;
      m_PacketHeaderVideo.uCurrentBlockECPackets = pModel->video_link_profiles[iVideoProfile].block_fecs;
   
      m_uNextBlockPacketSize = m_PacketHeaderVideo.uCurrentBlockPacketSize;
      m_uNextBlockDataPackets = m_PacketHeaderVideo.uCurrentBlockDataPackets;
      m_uNextBlockECPackets = m_PacketHeaderVideo.uCurrentBlockECPackets;

      log_line("[VideoTXBuffer] Current EC scheme to use rightaway: %d/%d (%d bytes)", m_PacketHeaderVideo.uCurrentBlockDataPackets, m_PacketHeaderVideo.uCurrentBlockECPackets, m_PacketHeaderVideo.uCurrentBlockPacketSize);
   }
   else
   {
      m_uNextBlockPacketSize = pModel->video_link_profiles[iVideoProfile].video_data_length;
      m_uNextBlockDataPackets = pModel->video_link_profiles[iVideoProfile].block_packets;
      m_uNextBlockECPackets = pModel->video_link_profiles[iVideoProfile].block_fecs;
      log_line("[VideoTXBuffer] Next EC scheme to use: %d/%d (%d bytes)", m_uNextBlockDataPackets, m_uNextBlockECPackets, m_uNextBlockPacketSize);
   }

   if ( pModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
   {
      m_PacketHeaderVideo.uVideoStreamIndexAndType = 0 | (VIDEO_TYPE_H265<<4);
      log_line("[VideoTxBuffer] Set video header as H265 stream");
   }
   else
   {
      m_PacketHeaderVideo.uVideoStreamIndexAndType = 0 | (VIDEO_TYPE_H264<<4);
      log_line("[VideoTxBuffer] Set video header as H264 stream");
   }
   
   m_PacketHeaderVideo.uCurrentVideoLinkProfile = iVideoProfile;
   m_PacketHeaderVideo.uStreamInfoFlags = 0;
   m_PacketHeaderVideo.uStreamInfo = 0;
   updateCurrentKFValue();

   // Update status flags
   m_PacketHeaderVideo.uVideoStatusFlags2 = 0;
   if ( pModel->bDeveloperMode )
      m_PacketHeaderVideo.uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS;
}

void VideoTxPacketsBuffer::updateCurrentKFValue()
{
   m_PacketHeaderVideo.uCurrentVideoKeyframeIntervalMs = adaptive_video_get_current_kf();
}

void VideoTxPacketsBuffer::fillVideoPacketsFromCSI(u8* pVideoData, int iDataSize, bool bEndOfFrame)
{
   if ( (NULL == pVideoData) || (iDataSize <= 0) )
      return;

   if ( NULL != g_pProcessorTxVideo )
      process_data_tx_video_on_new_data(pVideoData, iDataSize);

   while ( iDataSize > 0 )
   {
      int iSizeLeftToFillInCurrentPacket = m_PacketHeaderVideo.uCurrentBlockPacketSize - m_iTempVideoBufferFilledBytes - sizeof(u16);
      int iParsed = m_ParserInputH264.parseSingleSegmentDataUntilStartOfNextNALOrLimit(pVideoData, iDataSize, iSizeLeftToFillInCurrentPacket);
      
      // End of NAL unit?
      bool bEndNAL = false;
      if ( (iParsed == iDataSize) && bEndOfFrame )
         bEndNAL = true;
      if ( (iParsed < iSizeLeftToFillInCurrentPacket) && (iParsed >= 0) && (pVideoData[iParsed] == 0) && (pVideoData[iParsed+1] == 0) && (pVideoData[iParsed+2] == 0) && (pVideoData[iParsed+3] == 0x01) )
         bEndNAL = true;

      u32 uStart = 0;
      u32 uEnd = 0;
      memcpy(&uStart, pVideoData, sizeof(u32));
      memcpy(&uEnd, &pVideoData[iParsed-4], sizeof(u32));

      // End of P or I frame or frame slice? (otherways it's smaller SPS PPS NALs)
      // Then end a video packet block here
      if ( bEndNAL )
      if ( (m_ParserInputH264.getCurrentSegmentNALType() == 1) || (m_ParserInputH264.getCurrentSegmentNALType() == 5) )
      {
         m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_NAL_END;
         if ( m_ParserInputH264.getCurrentSegmentNALType() == 1 )
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_P_NAL;
         else
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_I_NAL;
         if ( (pVideoData[0] == 0) && (pVideoData[1] == 0) && (pVideoData[2] == 0) && (pVideoData[3] == 0x01) )
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_NAL_START;

         memcpy(&m_TempVideoBuffer[m_iTempVideoBufferFilledBytes], pVideoData, iParsed);
         m_iTempVideoBufferFilledBytes += iParsed;
         _addNewVideoPacket(m_TempVideoBuffer, m_iTempVideoBufferFilledBytes, m_uTempBufferNALPresenceFlags, (iParsed == iDataSize) && bEndOfFrame);
         m_iTempVideoBufferFilledBytes = 0;
         m_uTempBufferNALPresenceFlags = 0;
         pVideoData += iParsed;
         iDataSize -= iParsed;
         continue;
      }

      // It's a SPS,PSP or part of an I/P NAL unit (not end of it)
      // Just fill video packets as needed/possible
      
      u8 *pDataToAdd = pVideoData;
      int iDataToAdd = iParsed;
      while ( iDataToAdd > 0 )
      {
         if ( m_ParserInputH264.getCurrentSegmentNALType() == 1 )
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_P_NAL;
         if ( m_ParserInputH264.getCurrentSegmentNALType() == 5 )
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_I_NAL;
         if ( (m_ParserInputH264.getCurrentSegmentNALType() == 7) ||
              (m_ParserInputH264.getCurrentSegmentNALType() == 8 ) )
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_SPS_NAL;
         if ( (pDataToAdd[0] == 0) && (pDataToAdd[1] == 0) && (pDataToAdd[2] == 0) && (pDataToAdd[3] == 0x01) )
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_NAL_START;

         iSizeLeftToFillInCurrentPacket = m_PacketHeaderVideo.uCurrentBlockPacketSize - m_iTempVideoBufferFilledBytes - sizeof(u16);
    
         if ( iDataToAdd < iSizeLeftToFillInCurrentPacket )
         {
            memcpy(&m_TempVideoBuffer[m_iTempVideoBufferFilledBytes], pDataToAdd, iDataToAdd);
            m_iTempVideoBufferFilledBytes += iDataToAdd;            
            break;
         }

         memcpy(&m_TempVideoBuffer[m_iTempVideoBufferFilledBytes], pDataToAdd, iSizeLeftToFillInCurrentPacket);
         m_iTempVideoBufferFilledBytes += iSizeLeftToFillInCurrentPacket;
         _addNewVideoPacket(m_TempVideoBuffer, m_iTempVideoBufferFilledBytes, m_uTempBufferNALPresenceFlags, false);
         pDataToAdd += iSizeLeftToFillInCurrentPacket;
         iDataToAdd -= iSizeLeftToFillInCurrentPacket;
         m_iTempVideoBufferFilledBytes = 0;
         m_uTempBufferNALPresenceFlags = 0;
      }
      pVideoData += iParsed;
      iDataSize -= iParsed;
   }
}

void VideoTxPacketsBuffer::fillVideoPacketsFromRTSPPacket(u8* pVideoData, int iDataSize, bool bSingle, bool bEnd, u32 uNALType)
{
   if ( (NULL == pVideoData) || (iDataSize <= 0) )
      return;

   if ( NULL != g_pProcessorTxVideo )
      process_data_tx_video_on_new_data(pVideoData, iDataSize);


   int iSizeLeftToFillInCurrentPacket = m_PacketHeaderVideo.uCurrentBlockPacketSize - m_iTempVideoBufferFilledBytes - sizeof(u16);
   
   u32 uStart = 0;
   u32 uEnd = 0;
   memcpy(&uStart, pVideoData, sizeof(u32));
   memcpy(&uEnd, &pVideoData[iDataSize-4], sizeof(u32));

   // Fits in the current remaining packet buffer?
   if ( bEnd )
   if ( iDataSize <= iSizeLeftToFillInCurrentPacket )
   {
      if ( (uNALType == 7) || (uNALType == 8) )
         m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_SPS_NAL;
      else if ( uNALType == 1 )
         m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_P_NAL;
      else
         m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_I_NAL;
      if ( (pVideoData[0] == 0) && (pVideoData[1] == 0) && (pVideoData[2] == 0) && (pVideoData[3] == 0x01) )
         m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_NAL_START;

      if ( (uNALType == 7) || (uNALType == 8) )
      {
         memcpy(&m_TempVideoBuffer[m_iTempVideoBufferFilledBytes], pVideoData, iDataSize);
         m_iTempVideoBufferFilledBytes += iDataSize;            
         return;
      }
      else
      {
         m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_NAL_END;
         if ( (pVideoData[0] == 0) && (pVideoData[1] == 0) && (pVideoData[2] == 0) && (pVideoData[3] == 0x01) )
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_NAL_START;
         if ( uNALType == 1 )
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_P_NAL;
         else
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_I_NAL;

         memcpy(&m_TempVideoBuffer[m_iTempVideoBufferFilledBytes], pVideoData, iDataSize);
         m_iTempVideoBufferFilledBytes += iDataSize;
         _addNewVideoPacket(m_TempVideoBuffer, m_iTempVideoBufferFilledBytes, m_uTempBufferNALPresenceFlags, true);
         m_iTempVideoBufferFilledBytes = 0;
         m_uTempBufferNALPresenceFlags = 0;
         return;
      }
   }
   
   u8 *pDataToAdd = pVideoData;
   int iDataToAdd = iDataSize;
   while ( iDataToAdd > 0 )
   {
      if ( (uNALType == 7) || (uNALType == 8) )
         m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_SPS_NAL;
      else if ( uNALType == 1 )
         m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_P_NAL;
      else
         m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_I_NAL;
      if ( (pDataToAdd[0] == 0) && (pDataToAdd[1] == 0) && (pDataToAdd[2] == 0) && (pDataToAdd[3] == 0x01) )
         m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_NAL_START;

      iSizeLeftToFillInCurrentPacket = m_PacketHeaderVideo.uCurrentBlockPacketSize - m_iTempVideoBufferFilledBytes - sizeof(u16);
      if ( iDataToAdd >= iSizeLeftToFillInCurrentPacket )
      {
         memcpy(&m_TempVideoBuffer[m_iTempVideoBufferFilledBytes], pDataToAdd, iSizeLeftToFillInCurrentPacket);
         m_iTempVideoBufferFilledBytes += iSizeLeftToFillInCurrentPacket;
         _addNewVideoPacket(m_TempVideoBuffer, m_iTempVideoBufferFilledBytes, m_uTempBufferNALPresenceFlags, false);
         pDataToAdd += iSizeLeftToFillInCurrentPacket;
         iDataToAdd -= iSizeLeftToFillInCurrentPacket;
         m_iTempVideoBufferFilledBytes = 0;
         m_uTempBufferNALPresenceFlags = 0;
         continue;
      }
      else
      {
         memcpy(&m_TempVideoBuffer[m_iTempVideoBufferFilledBytes], pDataToAdd, iDataToAdd);
         m_iTempVideoBufferFilledBytes += iDataToAdd;
         if ( bEnd )
         {
            if ( (uNALType == 7) || (uNALType == 8) )
               m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_SPS_NAL;
            else if ( uNALType == 1 )
               m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_P_NAL;
            else
               m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_CONTAINS_I_NAL;
            if ( (pDataToAdd[0] == 0) && (pDataToAdd[1] == 0) && (pDataToAdd[2] == 0) && (pDataToAdd[3] == 0x01) )
               m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_NAL_START;

            if ( (uNALType == 7) || (uNALType == 8) )
               return;
            
            m_uTempBufferNALPresenceFlags |= VIDEO_STATUS_FLAGS2_NAL_END;
            _addNewVideoPacket(m_TempVideoBuffer, m_iTempVideoBufferFilledBytes, m_uTempBufferNALPresenceFlags, true);
            m_iTempVideoBufferFilledBytes = 0;
            m_uTempBufferNALPresenceFlags = 0;
         }
         return;
      }
   }
}


void VideoTxPacketsBuffer::_addNewVideoPacket(u8* pVideoData, int iDataSize, u32 uNALPresenceFlags, bool bEndOfTransmissionFrame)
{
   if ( (! m_bInitialized) || (NULL == pVideoData) || (iDataSize <= 0) || (iDataSize > MAX_PACKET_PAYLOAD) )
      return;

   _checkAllocatePacket(m_iNextBufferIndexToFill, m_iNextBufferPacketIndexToFill);

   // Started a new video block? Set the pending EC scheme and clear the state of the block
   if ( 0 == m_iNextBufferPacketIndexToFill )
   {
      m_PacketHeaderVideo.uCurrentBlockPacketSize = m_uNextBlockPacketSize;
      m_PacketHeaderVideo.uCurrentBlockDataPackets = m_uNextBlockDataPackets;
      m_PacketHeaderVideo.uCurrentBlockECPackets = m_uNextBlockECPackets;
   }

   // Update packet headers
   _fillVideoPacketHeaders(m_iNextBufferIndexToFill, m_iNextBufferPacketIndexToFill, iDataSize + sizeof(u16), uNALPresenceFlags, bEndOfTransmissionFrame);
   //t_packet_header* pCurrentPacketHeader = m_VideoPackets[m_iNextBufferIndexToFill][m_iNextBufferPacketIndexToFill].pPH;
   t_packet_header_video_full_98* pCurrentVideoPacketHeader = m_VideoPackets[m_iNextBufferIndexToFill][m_iNextBufferPacketIndexToFill].pPHVF;
   u8* pVideoDestination = m_VideoPackets[m_iNextBufferIndexToFill][m_iNextBufferPacketIndexToFill].pVideoData;
   if ( m_PacketHeaderVideo.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
      pVideoDestination += sizeof(t_packet_header_video_full_98_debug_info);

   // Copy video data
   u16 uVideoSize = iDataSize;
   memcpy(pVideoDestination, &uVideoSize, sizeof(u16));
   memcpy(pVideoDestination+sizeof(u16), pVideoData, iDataSize);
   
   // Set remaining empty space to 0 as EC uses the good video data packets too.
   if ( iDataSize < (int)pCurrentVideoPacketHeader->uCurrentBlockPacketSize - (int)sizeof(u16) )
   {
      memset(pVideoDestination+sizeof(u16) + iDataSize, 0, pCurrentVideoPacketHeader->uCurrentBlockPacketSize - sizeof(u16) - iDataSize );
   }

   if ( m_PacketHeaderVideo.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
   {
      t_packet_header_video_full_98_debug_info* pPHVFDebugInfo = (t_packet_header_video_full_98_debug_info*)(m_VideoPackets[m_iNextBufferIndexToFill][m_iNextBufferPacketIndexToFill].pVideoData);
      pPHVFDebugInfo->uVideoCRC = base_compute_crc32(pVideoDestination, pCurrentVideoPacketHeader->uCurrentBlockPacketSize);
   }

   // Update state
   m_iNextBufferPacketIndexToFill++;
   m_iCountReadyToSend++;
   m_uNextVideoBlockPacketIndexToGenerate++;

   if ( m_uNextVideoBlockPacketIndexToGenerate >= pCurrentVideoPacketHeader->uCurrentBlockDataPackets )
   if ( pCurrentVideoPacketHeader->uCurrentBlockECPackets > 0 )
   {
      // Compute EC packets
      u8* p_fec_data_packets[MAX_DATA_PACKETS_IN_BLOCK];
      u8* p_fec_data_fecs[MAX_FECS_PACKETS_IN_BLOCK];

      for( int i=0; i<m_PacketHeaderVideo.uCurrentBlockDataPackets; i++ )
      {
         _checkAllocatePacket(m_iNextBufferIndexToFill, i);
         pVideoDestination = m_VideoPackets[m_iNextBufferIndexToFill][i].pVideoData;
         if ( m_PacketHeaderVideo.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
            pVideoDestination += sizeof(t_packet_header_video_full_98_debug_info);
         p_fec_data_packets[i] = pVideoDestination;
      }
      int iECDelta = m_PacketHeaderVideo.uCurrentBlockDataPackets;
      for( int i=0; i<m_PacketHeaderVideo.uCurrentBlockECPackets; i++ )
      {
         _checkAllocatePacket(m_iNextBufferIndexToFill, i+iECDelta);
         pVideoDestination = m_VideoPackets[m_iNextBufferIndexToFill][i+iECDelta].pVideoData;
         if ( m_PacketHeaderVideo.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
            pVideoDestination += sizeof(t_packet_header_video_full_98_debug_info);
         p_fec_data_fecs[i] = pVideoDestination;
      }

      u32 tTemp = get_current_timestamp_micros();
      fec_encode(m_PacketHeaderVideo.uCurrentBlockPacketSize, p_fec_data_packets, m_PacketHeaderVideo.uCurrentBlockDataPackets, p_fec_data_fecs, m_PacketHeaderVideo.uCurrentBlockECPackets);
      tTemp = get_current_timestamp_micros() - tTemp;
      s_uTimeTotalFecTimeMicroSec += tTemp;
      if ( 0 == s_uLastTimeFecCalculation )
      {
         s_uTimeFecMicroPerSec = 0;
         s_uTimeTotalFecTimeMicroSec = 0;
         s_uLastTimeFecCalculation = g_TimeNow;
      }
      else if ( g_TimeNow >= s_uLastTimeFecCalculation + 250 )
      {
         s_uTimeFecMicroPerSec = 4 * s_uTimeTotalFecTimeMicroSec;
         s_uTimeTotalFecTimeMicroSec = 0;
         s_uLastTimeFecCalculation = g_TimeNow;
      }

      for( int i=0; i<m_PacketHeaderVideo.uCurrentBlockECPackets; i++ )
      {
         // Update packet headers
         _fillVideoPacketHeaders(m_iNextBufferIndexToFill, i+iECDelta, m_PacketHeaderVideo.uCurrentBlockPacketSize, uNALPresenceFlags, bEndOfTransmissionFrame);

         //pCurrentPacketHeader = m_VideoPackets[m_iNextBufferIndexToFill][i+iECDelta].pPH;
         pCurrentVideoPacketHeader = m_VideoPackets[m_iNextBufferIndexToFill][i+iECDelta].pPHVF;
         pVideoDestination = m_VideoPackets[m_iNextBufferIndexToFill][i+iECDelta].pVideoData;

         if ( m_PacketHeaderVideo.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
         {
            t_packet_header_video_full_98_debug_info* pPHVFDebugInfo = (t_packet_header_video_full_98_debug_info*)(m_VideoPackets[m_iNextBufferIndexToFill][i+iECDelta].pVideoData);
            pVideoDestination += sizeof(t_packet_header_video_full_98_debug_info);
         
            pPHVFDebugInfo->uVideoCRC = base_compute_crc32(pVideoDestination, pCurrentVideoPacketHeader->uCurrentBlockPacketSize);
         }

         m_iNextBufferPacketIndexToFill++;
         m_iCountReadyToSend++;
         m_uNextVideoBlockPacketIndexToGenerate++;
      }
   }

   if ( m_uNextVideoBlockPacketIndexToGenerate >= pCurrentVideoPacketHeader->uCurrentBlockDataPackets + pCurrentVideoPacketHeader->uCurrentBlockECPackets )
   {
      m_uNextVideoBlockPacketIndexToGenerate = 0;
      m_uNextVideoBlockIndexToGenerate++;
      m_iNextBufferPacketIndexToFill = 0;
      m_iNextBufferIndexToFill++;
      if ( m_iNextBufferIndexToFill >= MAX_RXTX_BLOCKS_BUFFER )
         m_iNextBufferIndexToFill = 0;

      // Buffer is full, discard old packets
      if ( m_iNextBufferIndexToFill == m_iCurrentBufferIndexToSend )
      {
         log_softerror_and_alarm("[VideoTXBuffer] Buffer is full. Discard all blocks. (Packets ready to send: %d)", m_iCountReadyToSend);

         discardBuffer();
         log_softerror_and_alarm("[VideoTXBuffer] Discarded blocks to send. (Packets ready to send now: %d)", m_iCountReadyToSend);
      }
   }
}

void VideoTxPacketsBuffer::_sendPacket(int iBufferIndex, int iPacketIndex, u32 uRetransmissionId)
{
   t_packet_header* pCurrentPacketHeader = m_VideoPackets[iBufferIndex][iPacketIndex].pPH;
   t_packet_header_video_full_98* pCurrentVideoPacketHeader = m_VideoPackets[iBufferIndex][iPacketIndex].pPHVF;

   //t_packet_header_video_full_98* pPHVF = (t_packet_header_video_full_98*) (m_VideoPackets[iBufferIndex][iPacketIndex].pRawData+sizeof(t_packet_header));    
   
   // stream_packet_idx: high 4 bits: stream id (0..15), lower 28 bits: stream packet index
   pCurrentPacketHeader->stream_packet_idx = m_uRadioStreamPacketIndex;
   pCurrentPacketHeader->stream_packet_idx &= PACKET_FLAGS_MASK_STREAM_PACKET_IDX;
   pCurrentPacketHeader->stream_packet_idx |= (STREAM_ID_VIDEO_1) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   m_uRadioStreamPacketIndex++;

   pCurrentPacketHeader->packet_flags = PACKET_COMPONENT_VIDEO;
   pCurrentPacketHeader->packet_flags |= PACKET_FLAGS_BIT_HEADERS_ONLY_CRC;

   if ( 0 == uRetransmissionId )
   {
      pCurrentPacketHeader->packet_flags &= ~PACKET_FLAGS_BIT_RETRANSMITED;
      if ( m_iCountReadyToSend > 0 )
         m_iCountReadyToSend--;

      if ( g_pCurrentModel->bDeveloperMode )
      {
         if ( iPacketIndex < pCurrentVideoPacketHeader->uCurrentBlockDataPackets )
            g_VehicleRuntimeInfo.uSentVideoDataPackets[g_VehicleRuntimeInfo.iCurrentIndex]++;
         else
            g_VehicleRuntimeInfo.uSentVideoECPackets[g_VehicleRuntimeInfo.iCurrentIndex]++;
      }
   }
   else
   {
      pCurrentPacketHeader->packet_flags |= PACKET_FLAGS_BIT_RETRANSMITED;
      pCurrentVideoPacketHeader->uStreamInfoFlags = VIDEO_STREAM_INFO_FLAG_RETRANSMISSION_ID;
      pCurrentVideoPacketHeader->uStreamInfo = uRetransmissionId;
   }

   if ( g_bVideoPaused || (! relay_current_vehicle_must_send_own_video_feeds()) )
      return;

   //t_packet_header_video_full_98_debug_info* pPHVFDebugInfo = (t_packet_header_video_full_98_debug_info*) m_VideoPackets[iBufferIndex][iPacketIndex].pVideoData;
   //u8* pVideoData = m_VideoPackets[iBufferIndex][iPacketIndex].pVideoData;
   //pVideoData += sizeof(t_packet_header_video_full_98_debug_info);
   //u32 crc = base_compute_crc32(pVideoData, pCurrentVideoPacketHeader->uCurrentBlockPacketSize);

   send_packet_to_radio_interfaces((u8*)pCurrentPacketHeader, pCurrentPacketHeader->total_length, -1);
}

int VideoTxPacketsBuffer::hasPendingPacketsToSend()
{
   return m_iCountReadyToSend;
}

int VideoTxPacketsBuffer::sendAvailablePackets(int iMaxCountToSend)
{
   if ( m_iCountReadyToSend <= 0 )
      return 0;

   int iToSend = m_iCountReadyToSend;
   if ( iToSend > MAX_PACKETS_TO_SEND_IN_ONE_SLICE )
      iToSend = MAX_PACKETS_TO_SEND_IN_ONE_SLICE;
   if ( iMaxCountToSend > 0 )
   if ( iToSend > iMaxCountToSend )
      iToSend = iMaxCountToSend;

   int iCountSent = 0;
   for( int i=0; i<iToSend; i++ )
   {
      if ( NULL == m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPH )
      {
         log_softerror_and_alarm("Invalid packet [%d/%d], video next to gen: [%u/%u], ready to send: %d, header: %X", m_iCurrentBufferIndexToSend, m_iCurrentBufferPacketIndexToSend,
         m_uNextVideoBlockIndexToGenerate, m_uNextVideoBlockPacketIndexToGenerate, m_iCountReadyToSend, m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPH);
         continue;
      }
      _sendPacket(m_iCurrentBufferIndexToSend, m_iCurrentBufferPacketIndexToSend, 0);
      iCountSent++;
      t_packet_header_video_full_98* pCurrentVideoPacketHeader = m_VideoPackets[m_iCurrentBufferIndexToSend][m_iCurrentBufferPacketIndexToSend].pPHVF;
      m_iCurrentBufferPacketIndexToSend++;
      if ( m_iCurrentBufferPacketIndexToSend >= pCurrentVideoPacketHeader->uCurrentBlockDataPackets + pCurrentVideoPacketHeader->uCurrentBlockECPackets )
      {
         m_iCurrentBufferPacketIndexToSend = 0;
         m_iCurrentBufferIndexToSend++;
         if ( m_iCurrentBufferIndexToSend >= MAX_RXTX_BLOCKS_BUFFER )
            m_iCurrentBufferIndexToSend = 0;
      }
      if ( m_iCountReadyToSend <= 0 )
         break;

      if ( m_iCurrentBufferIndexToSend == m_iNextBufferIndexToFill )
      if ( m_iCurrentBufferPacketIndexToSend == m_iNextBufferPacketIndexToFill )
         break;
   }
   return iCountSent;
}


void VideoTxPacketsBuffer::resendVideoPacket(u32 uRetransmissionId, u32 uVideoBlockIndex, u32 uVideoBlockPacketIndex)
{
   if ( uVideoBlockIndex > m_uNextVideoBlockIndexToGenerate )
      return;
   if ( uVideoBlockIndex == m_uNextVideoBlockIndexToGenerate )
   if ( uVideoBlockPacketIndex >= m_uNextVideoBlockPacketIndexToGenerate )
      return;
 
   int iDeltaBlocksBack = (int)m_uNextVideoBlockIndexToGenerate - (int)uVideoBlockIndex;
   if ( (iDeltaBlocksBack < 0) || (iDeltaBlocksBack >= MAX_RXTX_BLOCKS_BUFFER) )
      return;

   int iBufferIndex = m_iNextBufferIndexToFill - iDeltaBlocksBack;
   if ( iBufferIndex < 0 )
      iBufferIndex += MAX_RXTX_BLOCKS_BUFFER;

   // Still too old?
   if ( iBufferIndex < 0 )
      return;

   if ( NULL == m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPH )
      return;
   if ( m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVF->uCurrentBlockIndex != uVideoBlockIndex )
      return;
   if ( m_VideoPackets[iBufferIndex][uVideoBlockPacketIndex].pPHVF->uCurrentBlockPacketIndex != uVideoBlockPacketIndex )
      return;

   if ( 0 == uRetransmissionId )
      uRetransmissionId = MAX_U32-1;
   _sendPacket(iBufferIndex, (int)uVideoBlockPacketIndex, uRetransmissionId);
}
