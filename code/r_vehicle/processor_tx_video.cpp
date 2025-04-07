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
#include "../base/hardware.h"
#include "../base/models.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/fec.h"
#include "../base/camera_utils.h"
#include "../base/parser_h264.h"
#include "../common/string_utils.h"
#include "shared_vars.h"
#include "timers.h"

#include "processor_tx_video.h"
#include "processor_relay.h"
#include "packets_utils.h"
#include "../utils/utils_vehicle.h"
#include "adaptive_video.h"
#include "video_source_csi.h"
#include "video_source_majestic.h"
#include <semaphore.h>

int ProcessorTxVideo::m_siInstancesCount = 0;
u32 s_lCountBytesVideoIn = 0; 


ProcessorTxVideo::ProcessorTxVideo(int iVideoStreamIndex, int iCameraIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siInstancesCount;
   m_siInstancesCount++;

   m_iVideoStreamIndex = iVideoStreamIndex;
   m_iCameraIndex = iCameraIndex;

   for( int i=0; i<MAX_VIDEO_BITRATE_HISTORY_VALUES; i++ )
   {
      m_BitrateHistorySamples[i].uTimeStampTaken = 0;
      m_BitrateHistorySamples[i].uTotalBitrateBPS = 0;
      m_BitrateHistorySamples[i].uVideoBitrateBPS = 0;
   }
   
   m_uVideoBitrateKbAverageSum = 0;
   m_uTotalVideoBitrateKbAverageSum = 0;
   m_uVideoBitrateAverage = 0;
   m_uTotalVideoBitrateAverage = 0;
   m_uIntervalMSComputeVideoBitrateSample = 50; // Must compute video bitrate averages quite often as they are used for auto adustments of video bitrate and quantization
   m_uTimeLastVideoBitrateSampleTaken = 0;
   m_iVideoBitrateSampleIndex = 0;
}

ProcessorTxVideo::~ProcessorTxVideo()
{
   uninit();
   m_siInstancesCount--;
}

bool ProcessorTxVideo::init()
{
   if ( m_bInitialized )
      return true;

   log_line("[VideoTX] Initialize video processor Tx instance number %d.", m_iInstanceIndex+1);

   // To fix
   /*
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      log_line("[VideoTx] Set initial majestic caputure bitrate as %u bps", g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate);
   }
   */
   m_bInitialized = true;

   return true;
}

bool ProcessorTxVideo::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[VideoTX] Uninitialize video processor Tx instance number %d.", m_iInstanceIndex+1);
   
   m_bInitialized = false;
   return true;
}


// Returns bps
u32 ProcessorTxVideo::getCurrentVideoBitrate()
{
   return m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uVideoBitrateBPS;
}

// Returns bps
u32 ProcessorTxVideo::getCurrentVideoBitrateAverage()
{
   return m_uVideoBitrateAverage;
}

// Returns bps
u32 ProcessorTxVideo::getCurrentVideoBitrateAverageLastMs(u32 uMilisec)
{
   u32 uSumKb = 0;
   u32 uCount = 0;
   int iIndex = m_iVideoBitrateSampleIndex;

   u32 uTime0 = m_BitrateHistorySamples[iIndex].uTimeStampTaken;
   
   while ( (uCount < MAX_VIDEO_BITRATE_HISTORY_VALUES) && (m_BitrateHistorySamples[iIndex].uTimeStampTaken >= (uTime0 - uMilisec)) )
   {
      uSumKb += m_BitrateHistorySamples[iIndex].uVideoBitrateBPS/1000;
      uCount++;
      iIndex--;
      if ( iIndex < 0 )
        iIndex = MAX_VIDEO_BITRATE_HISTORY_VALUES-1;
   }
   if ( uCount > 0 )
      return 1000*(uSumKb/uCount);
   return 0;
}

// Returns bps
u32 ProcessorTxVideo::getCurrentTotalVideoBitrate()
{
   return m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uTotalBitrateBPS;
}

// Returns bps
u32 ProcessorTxVideo::getCurrentTotalVideoBitrateAverage()
{
   return m_uTotalVideoBitrateAverage;
}

// Returns bps
u32 ProcessorTxVideo::getCurrentTotalVideoBitrateAverageLastMs(u32 uMilisec)
{
   // To fix
   return  0;
   u32 uSumKb = 0;
   u32 uCount = 0;
   int iIndex = m_iVideoBitrateSampleIndex;
   
   u32 uTime0 = m_BitrateHistorySamples[iIndex].uTimeStampTaken;
   
   while ( (uCount < MAX_VIDEO_BITRATE_HISTORY_VALUES) && (m_BitrateHistorySamples[iIndex].uTimeStampTaken >= (uTime0 - uMilisec)) )
   {
      uSumKb += m_BitrateHistorySamples[iIndex].uTotalBitrateBPS/1000;
      uCount++;
      iIndex--;
      if ( iIndex < 0 )
        iIndex = MAX_VIDEO_BITRATE_HISTORY_VALUES-1;
   }

   if ( uCount > 0 )
      return 1000*(uSumKb/uCount);
   return 0;
}

void ProcessorTxVideo::periodicLoop()
{
   if ( g_TimeNow < (m_uTimeLastVideoBitrateSampleTaken + m_uIntervalMSComputeVideoBitrateSample) )
      return;

   u32 uDeltaTime = g_TimeNow - m_uTimeLastVideoBitrateSampleTaken;
   m_uTimeLastVideoBitrateSampleTaken = g_TimeNow;

   m_iVideoBitrateSampleIndex++;
   if ( m_iVideoBitrateSampleIndex >= MAX_VIDEO_BITRATE_HISTORY_VALUES )
      m_iVideoBitrateSampleIndex = 0;

   m_uVideoBitrateKbAverageSum -= m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uVideoBitrateBPS/1000;
   m_uTotalVideoBitrateKbAverageSum -= m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uTotalBitrateBPS/1000;
  
   m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uVideoBitrateBPS = ((s_lCountBytesVideoIn * 8) / uDeltaTime) * 1000;
   //To fix
   //m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uTotalBitrateBPS = ((s_lCountBytesSend * 8) / uDeltaTime) * 1000;
   m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uTimeStampTaken = g_TimeNow;

   m_uVideoBitrateKbAverageSum += m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uVideoBitrateBPS/1000;
   m_uTotalVideoBitrateKbAverageSum += m_BitrateHistorySamples[m_iVideoBitrateSampleIndex].uTotalBitrateBPS/1000;
  
   m_uVideoBitrateAverage = 1000*(m_uVideoBitrateKbAverageSum/MAX_VIDEO_BITRATE_HISTORY_VALUES);
   m_uTotalVideoBitrateAverage = 1000*(m_uTotalVideoBitrateKbAverageSum/MAX_VIDEO_BITRATE_HISTORY_VALUES);
   
   s_lCountBytesVideoIn = 0;
   // To fix
   //s_lCountBytesSend = 0;
}

void _log_encoding_scheme()
{
 /*
   if ( g_TimeNow > s_TimeLastEncodingSchemeLog + 5000 )
   {
      s_TimeLastEncodingSchemeLog = g_TimeNow;
      char szScheme[64];
      char szVideoStream[64];
      strcpy(szScheme, "[Unknown]");
      strcpy(szVideoStream, "[Unknown]");

      // lower 4 bits: current video profile
      // higher 4 bits: user selected video profile

      strcpy(szScheme, str_get_video_profile_name(s_CurrentPHVF.video_link_profile & 0x0F));
      
      sprintf(szVideoStream, "[%d]", (s_CurrentPHVF.video_link_profile>>4) & 0x0F );
      u32 uValueDup = (s_CurrentPHVF.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT) >> 16;

      log_line("New current video profile used: %s (video stream Id: %s), extra params: 0x%08X, (for video res %d x %d, %d FPS, %d ms keyframe):",
         szScheme, szVideoStream, s_CurrentPHVF.uProfileEncodingFlags, s_CurrentPHVF.video_width, s_CurrentPHVF.video_height, s_CurrentPHVF.video_fps, s_CurrentPHVF.video_keyframe_interval_ms );
      log_line("New encoding scheme used: %d/%d block data/ECs, video data length: %d, R-data/R-retr: %d/%d",
         s_CurrentPHVF.block_packets, s_CurrentPHVF.block_fecs, s_CurrentPHVF.video_data_length,
         (uValueDup & 0x0F), ((uValueDup >> 4) & 0x0F) );
      log_line("Encoding change (%u times) active starting with stream packet: %u, video block index: %u, video packet index: %u", s_uCountEncodingChanges,
         (s_CurrentPH.stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX), s_CurrentPHVF.video_block_index, s_CurrentPHVF.video_block_packet_index);
      log_line("Encodings add debug info in video packets: %s", ((s_CurrentPHVF.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS)?"yes":"no"));
      return;
   }

   return;
   log_line("New encoding scheme: [%u/%u], %d/%d, %d/%d/%d", s_CurrentPHVF.video_block_index, s_CurrentPHVF.video_block_packet_index,
      (s_CurrentPHVF.video_link_profile>>4) & 0x0F, s_CurrentPHVF.video_link_profile & 0x0F,
         s_CurrentPHVF.block_packets, s_CurrentPHVF.block_fecs, s_CurrentPHVF.video_data_length);
*/
}


bool process_data_tx_video_command(int iRadioInterface, u8* pPacketBuffer)
{
   if ( ! g_bReceivedPairingRequest )
      return false;

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS )
   {
      static u32 s_uLastRecvRetransmissionId = 0;
      u8 uCount = pPacketBuffer[sizeof(t_packet_header) + sizeof(u32) + sizeof(u8)];
      u32 uRetrId = 0;
      memcpy(&uRetrId, &pPacketBuffer[sizeof(t_packet_header)], sizeof(u32));
      
      log_line("[AdaptiveVideo] Received retr request id %u from controller for %d packets", uRetrId, (int)uCount);
      
      u8* pDataPackets = pPacketBuffer + sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8);
      for( int i=0; i<(int)uCount; i++ )
      {
         u32 uBlockId = 0;
         int iPacketIndex = 0;
         memcpy(&uBlockId, pDataPackets, sizeof(u32));
         pDataPackets += sizeof(u32);
         iPacketIndex = (int) *pDataPackets;
         pDataPackets++;
         g_pVideoTxBuffers->resendVideoPacket(uRetrId, uBlockId, iPacketIndex);
         if ( (s_uLastRecvRetransmissionId != uRetrId) && (uCount < 4) )
            g_pVideoTxBuffers->resendVideoPacket(uRetrId, uBlockId, iPacketIndex);
      }
      s_uLastRecvRetransmissionId = uRetrId;
   }

   if ( pPH->packet_type == PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL )
   {
      if ( pPH->total_length < sizeof(t_packet_header) + 2*sizeof(u8) )
         return true;
      
      u32 uRequestId = 0;
      u8 uVideoProfile = 0;
      u8 uVideoStreamIndex = 0;
      memcpy( &uRequestId, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
      memcpy( &uVideoProfile, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32), sizeof(u8));
      memcpy( &uVideoStreamIndex, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32) + sizeof(u8), sizeof(u8));
   
      log_line("[AdaptiveVideo] Received req id %u from CID %u to switch video level to: %d (%s)",
          uRequestId, pPH->vehicle_id_src, uVideoProfile, str_get_video_profile_name(uVideoProfile));

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK, STREAM_ID_VIDEO_1);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = pPH->vehicle_id_src;
      PH.total_length = sizeof(t_packet_header) + sizeof(u32) + sizeof(u8);
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &uRequestId, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header) + sizeof(u32), &uVideoProfile, sizeof(u8));
      if ( radio_packet_type_is_high_priority(PH.packet_flags, PH.packet_type) )
         send_packet_to_radio_interfaces(packet, PH.total_length, -1);
      else
         packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);

      adaptive_video_set_last_profile_requested_by_controller((int)uVideoProfile);
        
      //int iTargetProfile = g_pCurrentModel->get_video_profile_from_total_levels_shift(iAdaptiveLevel);
      //int iTargetProfileShiftLevel = g_pCurrentModel->get_video_profile_level_shift_from_total_levels_shift(iAdaptiveLevel);
      
// To fix 
/*         if ( iTargetProfile == g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
    if ( iTargetProfileShiftLevel == g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel )
      {
         return;
      }
      video_stats_overwrites_switch_to_profile_and_level(iAdaptiveLevel, iTargetProfile, iTargetProfileShiftLevel);
*/
      return true;
   } 

   if ( pPH->packet_type == PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE )
   {
      if ( pPH->total_length < sizeof(t_packet_header) + sizeof(u32) )
         return true;
      if ( pPH->total_length > sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8) )
         return true;

      // Discard requests if we are on fixed keyframe
      //if ( !(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME) )
      //   return true;
      
      u8 uRequestId = 0;
      u32 uNewKeyframeValueMs = 0;
      u8 uVideoStreamIndex = 0;
      memcpy( &uRequestId, pPacketBuffer + sizeof(t_packet_header), sizeof(u8));
      memcpy( &uNewKeyframeValueMs, pPacketBuffer + sizeof(t_packet_header) + sizeof(u8), sizeof(u32));
      if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(u32) + sizeof(u8) )
         memcpy( &uVideoStreamIndex, pPacketBuffer + sizeof(t_packet_header) + sizeof(u32) + sizeof(u8), sizeof(u8));

      log_line("[AdaptiveVideo] Received req id %u from CID %u to switch video keyframe to: %d ms",
          uRequestId, pPH->vehicle_id_src, uNewKeyframeValueMs);

// To fix 
/*         if ( g_SM_VideoLinkStats.overwrites.uCurrentControllerRequestedKeyframeMs != uNewKeyframeValueMs )
         log_line("[KeyFrame] Recv request from controller for keyframe: %u ms (previous requested was: %u ms)", uNewKeyframeValueMs, g_SM_VideoLinkStats.overwrites.uCurrentControllerRequestedKeyframeMs);
      else
         log_line("[KeyFrame] Recv again request from controller for keyframe: %u ms", uNewKeyframeValueMs);          
*/       
      // If video is not sent from this vehicle to controller, then we must reply back with acknowledgment
      if ( ! relay_current_vehicle_must_send_own_video_feeds() )
      {
         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK, STREAM_ID_DATA);
         PH.packet_flags = PACKET_COMPONENT_VIDEO;
         PH.packet_type =  PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK;
         PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
         PH.vehicle_id_dest = pPH->vehicle_id_src;
         PH.total_length = sizeof(t_packet_header) + sizeof(u32) + sizeof(u8);
         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet+sizeof(t_packet_header), &uRequestId, sizeof(u8));
         memcpy(packet+sizeof(t_packet_header) + sizeof(u8), &uNewKeyframeValueMs, sizeof(u32));
         packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
      }

      adaptive_video_set_last_kf_requested_by_controller(uNewKeyframeValueMs);
      return true;
   }

   return false;
}

bool process_data_tx_video_loop()
{
   g_pProcessorTxVideo->periodicLoop();
   return true;
}

void process_data_tx_video_on_new_data(u8* pData, int iDataSize)
{
   s_lCountBytesVideoIn += iDataSize;
}
