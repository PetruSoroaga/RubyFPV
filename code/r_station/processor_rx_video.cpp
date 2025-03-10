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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../radio/fec.h" 

#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/shared_mem.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../common/string_utils.h"
#include "../common/relay_utils.h"
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopacketsqueue.h"

#include "shared_vars.h"
#include "shared_vars_state.h"
#include "processor_rx_video.h"
#include "rx_video_output.h"
#include "packets_utils.h"
#include "video_rx_buffers.h"
#include "timers.h"
#include "ruby_rt_station.h"
#include "test_link_params.h"

extern t_packet_queue s_QueueRadioPacketsHighPrio;

int ProcessorRxVideo::m_siInstancesCount = 0;
FILE* ProcessorRxVideo::m_fdLogFile = NULL;

void ProcessorRxVideo::oneTimeInit()
{
   m_siInstancesCount = 0;
   m_fdLogFile = NULL;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      g_pVideoProcessorRxList[i] = NULL;
   log_line("[ProcessorRxVideo] Did one time initialization.");
}

ProcessorRxVideo* ProcessorRxVideo::getVideoProcessorForVehicleId(u32 uVehicleId, u32 uVideoStreamIndex)
{
   if ( (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return NULL;

   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL != g_pVideoProcessorRxList[i] )
      if ( g_pVideoProcessorRxList[i]->m_uVehicleId == uVehicleId )
      if ( g_pVideoProcessorRxList[i]->m_uVideoStreamIndex == uVideoStreamIndex )
         return g_pVideoProcessorRxList[i];
   }
   return NULL;
}

/*
void ProcessorRxVideo::log(const char* format, ...)
{
   va_list args;
   va_start(args, format);

   log_line(format, args);

   if ( m_fdLogFile <= 0 )
      return;

   char szTime[64];
   sprintf(szTime,"%d:%02d:%02d.%03d", (int)(g_TimeNow/1000/60/60), (int)(g_TimeNow/1000/60)%60, (int)((g_TimeNow/1000)%60), (int)(g_TimeNow%1000));
 
   fprintf(m_fdLogFile, "%s: ", szTime);
   vfprintf(m_fdLogFile, format, args);
   fprintf(m_fdLogFile, "\n");
   fflush(m_fdLogFile);
}
*/

ProcessorRxVideo::ProcessorRxVideo(u32 uVehicleId, u8 uVideoStreamIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siInstancesCount;
   m_siInstancesCount++;

   log_line("[ProcessorRxVideo] Created new instance (number %d of %d) for VID: %u, stream %u", m_iInstanceIndex+1, m_siInstancesCount, uVehicleId, uVideoStreamIndex);
   m_uVehicleId = uVehicleId;
   m_uVideoStreamIndex = uVideoStreamIndex;
   m_uLastTimeRequestedRetransmission = 0;
   m_uRequestRetransmissionUniqueId = 0;
   m_TimeLastHistoryStatsUpdate = 0;
   m_TimeLastRetransmissionsStatsUpdate = 0;
   m_uLatestVideoPacketReceiveTime = 0;

   m_uLastVideoBlockIndexResolutionChange = 0;
   m_uLastVideoBlockPacketIndexResolutionChange = 0;

   m_bPaused = false;

   m_pVideoRxBuffer = new VideoRxPacketsBuffer(uVideoStreamIndex, 0);
   Model* pModel = findModelWithId(uVehicleId, 201);
   if ( NULL == pModel )
      log_softerror_and_alarm("[ProcessorRxVideo] Can't find model for VID %u", uVehicleId);
   else
      m_pVideoRxBuffer->init(pModel);

   // Add it to the video decode stats shared mem list

   m_iIndexVideoDecodeStats = -1;
   for(int i=0; i<MAX_VIDEO_PROCESSORS; i++)
   {
      if ( g_SM_VideoDecodeStats.video_streams[i].uVehicleId == 0 )
      {
         m_iIndexVideoDecodeStats = i;
         break;
      }
      if ( g_SM_VideoDecodeStats.video_streams[i].uVehicleId == uVehicleId )
      {
         m_iIndexVideoDecodeStats = i;
         break;
      }
   }
   if ( -1 != m_iIndexVideoDecodeStats )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uVehicleId = uVehicleId;
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uVideoStreamIndex = uVideoStreamIndex;
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth = 0;
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight = 0;
   }
}

ProcessorRxVideo::~ProcessorRxVideo()
{
   
   log_line("[ProcessorRxVideo] Video processor deleted for VID %u, video stream %u", m_uVehicleId, m_uVideoStreamIndex);

   m_siInstancesCount--;

   if ( 0 == m_siInstancesCount )
   {
      if ( m_fdLogFile != NULL )
         fclose(m_fdLogFile);
      m_fdLogFile = NULL;
   }

   // Remove this processor from video decode stats list
   if ( m_iIndexVideoDecodeStats != -1 )
   {
      memset(&(g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats]), 0, sizeof(shared_mem_video_stream_stats));
      for( int i=m_iIndexVideoDecodeStats; i<MAX_VIDEO_PROCESSORS-1; i++ )
         memcpy(&(g_SM_VideoDecodeStats.video_streams[i]), &(g_SM_VideoDecodeStats.video_streams[i+1]), sizeof(shared_mem_video_stream_stats));
      memset(&(g_SM_VideoDecodeStats.video_streams[MAX_VIDEO_PROCESSORS-1]), 0, sizeof(shared_mem_video_stream_stats));
   }
   m_iIndexVideoDecodeStats = -1;
}

bool ProcessorRxVideo::init()
{
   if ( m_bInitialized )
      return true;
   m_bInitialized = true;

   m_fdLogFile = NULL;

   log_line("[ProcessorRxVideo] Initialize video processor Rx instance number %d, for VID %u, video stream index %u", m_iInstanceIndex+1, m_uVehicleId, m_uVideoStreamIndex);

   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log_line("[ProcessorRxVideo] Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
      
   resetReceiveState();
   resetOutputState();
  
   log_line("[ProcessorRxVideo] Initialize video processor complete.");
   log_line("[ProcessorRxVideo] ====================================");
   return true;
}

bool ProcessorRxVideo::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[ProcessorRxVideo] Uninitialize video processor Rx instance number %d for VID %u, video stream index %d", m_iInstanceIndex+1, m_uVehicleId, m_uVideoStreamIndex);
   
   m_bInitialized = false;
   return true;
}

void ProcessorRxVideo::resetReceiveState()
{
   log_line("[ProcessorRxVideo] Start: Reset video RX state and buffers");
   
   m_InfoLastReceivedVideoPacket.receive_time = 0;
   m_InfoLastReceivedVideoPacket.video_block_index = MAX_U32;
   m_InfoLastReceivedVideoPacket.video_block_packet_index = MAX_U32;
   m_InfoLastReceivedVideoPacket.stream_packet_idx = MAX_U32;
   m_uLatestVideoPacketReceiveTime = 0;
   
   m_uLastBlockReceivedAckKeyframeInterval = MAX_U32;
   m_uLastBlockReceivedAdaptiveVideoInterval = MAX_U32;
   m_uLastBlockReceivedSetVideoBitrate = MAX_U32;
   m_uLastBlockReceivedEncodingExtraFlags2 = MAX_U32;

   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   m_uTimeIntervalMsForRequestingRetransmissions = 10;

   log_line("[ProcessorRxVideo] Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   
   if ( NULL != m_pVideoRxBuffer )
      m_pVideoRxBuffer->emptyBuffers("Reset receiver state on controller settings changed.");

   m_uTimeLastVideoStreamChanged = g_TimeNow;

   log_line("[ProcessorRxVideo] End: Reset video RX state and buffers");
   log_line("--------------------------------------------------------");
   log_line("");
}

void ProcessorRxVideo::resetOutputState()
{
   log_line("[ProcessorRxVideo] Reset output state.");
   m_uLastOutputVideoBlockTime = 0;
   m_uLastOutputVideoBlockIndex = MAX_U32;
   m_uLastOutputVideoBlockPacketIndex = MAX_U32;
   m_uLastOutputVideoBlockDataPackets = 5555;
}

void ProcessorRxVideo::resetStateOnVehicleRestart()
{
   log_line("[ProcessorRxVideo] VID %d, video stream %u: Reset state, full, due to vehicle restart.", m_uVehicleId, m_uVideoStreamIndex);
   resetReceiveState();
   resetOutputState();
   m_uRequestRetransmissionUniqueId = 0;
   m_uLastVideoBlockIndexResolutionChange = 0;
   m_uLastVideoBlockPacketIndexResolutionChange = 0;
}

void ProcessorRxVideo::discardRetransmissionsInfo()
{
   m_pVideoRxBuffer->emptyBuffers("No new video past retransmission window");
   resetOutputState();
   m_uLastTimeRequestedRetransmission = g_TimeNow;

   //if ( g_TimeNow > g_TimeLastVideoParametersOrProfileChanged + 3000 )
   //if ( g_TimeNow > g_TimeStart + 5000 )
   //   g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks++;
}

void ProcessorRxVideo::onControllerSettingsChanged()
{
   log_line("[ProcessorRxVideo] VID %u, video stream %u: Controller params changed. Reinitializing RX video state...", m_uVehicleId, m_uVideoStreamIndex);

   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log_line("[ProcessorRxVideo]: Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   
   resetReceiveState();
   resetOutputState();
}

void ProcessorRxVideo::pauseProcessing()
{
   m_bPaused = true;
   log_line("[ProcessorRxVideo] VID %u, video stream %u: paused processing.", m_uVehicleId, m_uVideoStreamIndex);
}

void ProcessorRxVideo::resumeProcessing()
{
   m_bPaused = false;
   log_line("[ProcessorRxVideo] VID %u, video stream %u: resumed processing.", m_uVehicleId, m_uVideoStreamIndex);
}      

void ProcessorRxVideo::checkAndDiscardBlocksTooOld()
{
   if ( (NULL == m_pVideoRxBuffer) || (0 == m_pVideoRxBuffer->getBlocksCountInBuffer()) )
      return;

   // Discard blocks that are too old, past retransmission window
   u32 uCutoffTime = g_TimeNow - m_iMilisecondsMaxRetransmissionWindow;
   int iCountSkipped = m_pVideoRxBuffer->discardOldBlocks(uCutoffTime);

   if ( iCountSkipped > 0 )
   {
      g_SMControllerRTInfo.uOutputedVideoPacketsSkippedBlocks[g_SMControllerRTInfo.iCurrentIndex] += iCountSkipped;
      if ( g_TimeNow > g_TimeLastVideoParametersOrProfileChanged + 3000 )
      if ( g_TimeNow > g_TimeStart + 5000 )
         g_SMControllerRTInfo.uTotalCountOutputSkippedBlocks++;
   }
}


u32 ProcessorRxVideo::getLastTimeVideoStreamChanged()
{
   return m_uTimeLastVideoStreamChanged;
}

u32 ProcessorRxVideo::getLastestVideoPacketReceiveTime()
{
   return m_uLatestVideoPacketReceiveTime;
}

int ProcessorRxVideo::getVideoWidth()
{
   int iVideoWidth = 0;
   if ( m_iIndexVideoDecodeStats != -1 )
   {
      if ( (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth) && (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight) )
         iVideoWidth = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth;
   }
   else
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
         iVideoWidth = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].width;
   }
   return iVideoWidth;
}

int ProcessorRxVideo::getVideoHeight()
{
   int iVideoHeight = 0;
   if ( m_iIndexVideoDecodeStats != -1 )
   {
      if ( (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth) && (0 != g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight) )
         iVideoHeight = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight;
   }
   else
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
         iVideoHeight = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].height;
   }
   return iVideoHeight;
}

int ProcessorRxVideo::getVideoFPS()
{
   int iFPS = 0;
   if ( -1 != m_iIndexVideoDecodeStats )
      iFPS = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoFPS;
   if ( 0 == iFPS )
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
         iFPS = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].fps;
   }
   return iFPS;
}

int ProcessorRxVideo::getVideoType()
{
   int iVideoType = 0;
   if ( (-1 != m_iIndexVideoDecodeStats ) &&
        (0 != ((g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uVideoStreamIndexAndType >> 4) & 0x0F) ) )
      iVideoType = (g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uVideoStreamIndexAndType >> 4) & 0x0F;
   else
   {
      Model* pModel = findModelWithId(m_uVehicleId, 177);
      if ( NULL != pModel )
      {
         iVideoType = VIDEO_TYPE_H264;
         if ( pModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
            iVideoType = VIDEO_TYPE_H265;
      }
   }
   return iVideoType;
}

int ProcessorRxVideo::periodicLoop(u32 uTimeNow, bool bForceSyncNow)
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   Model* pModel = findModelWithId(m_uVehicleId, 155);
   if ( (NULL == pModel) || (NULL == pRuntimeInfo) )
      return -1;
     
   return checkAndRequestMissingPackets(bForceSyncNow);
}

// Returns 1 if a video block has just finished and the flag "Can TX" is set

int ProcessorRxVideo::handleReceivedVideoPacket(int interfaceNb, u8* pBuffer, int length)
{
   if ( m_bPaused )
      return 1;

   t_packet_header* pPH = (t_packet_header*)pBuffer;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pBuffer+sizeof(t_packet_header));    
   controller_runtime_info_vehicle* pRTInfo = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, pPH->vehicle_id_src);
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);

   #ifdef PROFILE_RX
   //u32 uTimeStart = get_current_timestamp_ms();
   //int iStackIndexStart = m_iRXBlocksStackTopIndex;
   #endif  

   #if defined(RUBY_BUILD_HW_PLATFORM_PI)
   if (((pPHVS->uVideoStreamIndexAndType >> 4) & 0x0F) == VIDEO_TYPE_H265 )
   {
      static u32 s_uTimeLastSendVideoUnsuportedAlarmToCentral = 0;
      if ( g_TimeNow > s_uTimeLastSendVideoUnsuportedAlarmToCentral + 20000 )
      {
         s_uTimeLastSendVideoUnsuportedAlarmToCentral = g_TimeNow;
         send_alarm_to_central(ALARM_ID_UNSUPPORTED_VIDEO_TYPE, pPHVS->uVideoStreamIndexAndType, pPH->vehicle_id_src);
      }
   }
   #endif

   // Discard retransmitted packets that:
   // * Are from before latest video stream resolution change;
   // * Are from before a vehicle restart detected;
   // Retransmitted packets are sent on controller request or automatically (ie on a missing ACK)
   
   if ( (NULL == m_pVideoRxBuffer) || (NULL == pRuntimeInfo) || (! pRuntimeInfo->bIsPairingDone) )
      return 0;

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
   if ( (0 != m_uLastVideoBlockIndexResolutionChange) && (0 != m_uLastVideoBlockPacketIndexResolutionChange) )
   {
      bool bDiscard = false;
      if ( pPHVS->uCurrentBlockIndex < m_uLastVideoBlockIndexResolutionChange )
         bDiscard = true;
      if ( pPHVS->uCurrentBlockIndex == m_uLastVideoBlockIndexResolutionChange )
      if ( pPHVS->uCurrentBlockPacketIndex < m_uLastVideoBlockPacketIndexResolutionChange )
         bDiscard = true;
   
      if ( bDiscard )
         return 0;
   }


   bool bNewestOnStream = m_pVideoRxBuffer->checkAddVideoPacket(pBuffer, length);
   if ( bNewestOnStream )
   if ( ! (pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
   {
      m_uLatestVideoPacketReceiveTime = g_TimeNow;
      updateControllerRTInfoAndVideoDecodingStats(pBuffer, length);
   }

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
   {
      if ( NULL != pRTInfo )
      {
         pRTInfo->uCountAckRetransmissions[g_SMControllerRTInfo.iCurrentIndex]++;
         if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_RETRANSMISSION_ID )
         if ( pPHVS->uStreamInfo == m_uRequestRetransmissionUniqueId )
         {
            u32 uDeltaTime = g_TimeNow - m_uLastTimeRequestedRetransmission;
            controller_rt_info_update_ack_rt_time(&g_SMControllerRTInfo, pPH->vehicle_id_src, g_SM_RadioStats.radio_interfaces[interfaceNb].assignedLocalRadioLinkId, uDeltaTime);
         }
      }
   }

   // If one way link, or retransmissions are off, 
   //    or spectator mode, or not paired yet, or test link is in progress
   //    or negociate radio link is in progress,
   //    or vehicle has lost link to controller,
   // skip blocks, if there are more video blocks with gaps in buffer
   Model* pModel = findModelWithId(m_uVehicleId, 170);
   bool bSkipIncompleteBlocks = false;

   if ( g_bSearching || g_bUpdateInProgress || pModel->is_spectator )
      bSkipIncompleteBlocks = true;

   if ( ! bSkipIncompleteBlocks )
   if ( (NULL == pModel) || pModel->isVideoLinkFixedOneWay() || test_link_is_in_progress() || g_bNegociatingRadioLinks || (g_TimeNow < g_uTimeEndedNegiciateRadioLink + 3000) )
      bSkipIncompleteBlocks = true;

   if ( ! bSkipIncompleteBlocks )
   if ( (NULL == pRuntimeInfo) || (!pRuntimeInfo->bIsPairingDone) || (g_TimeNow < pRuntimeInfo->uPairingRequestTime + 100) )
      bSkipIncompleteBlocks = true;

   if ( ! bSkipIncompleteBlocks )
   if ( NULL != pModel )
   if ( ! (pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS) )
      bSkipIncompleteBlocks = true;

   // Output available video packets
   type_rx_video_block_info* pVideoBlock = NULL;
   type_rx_video_packet_info* pVideoPacket = NULL;
   u8* pVideoRawStreamData = NULL;

   pVideoPacket = m_pVideoRxBuffer->getMarkFirstPacketToOutput(&pVideoBlock, bSkipIncompleteBlocks);

   while ( (NULL != pVideoPacket) && (NULL != pVideoBlock) && (NULL != pVideoPacket->pRawData) )
   {
      t_packet_header_video_segment_important* pPHVSImp = pVideoPacket->pPHVSImp;
      pVideoRawStreamData = pVideoPacket->pVideoData;
      pVideoRawStreamData += sizeof(t_packet_header_video_segment_important);

      int iVideoWidth = getVideoWidth();
      int iVideoHeight = getVideoHeight();

      rx_video_output_video_data(m_uVehicleId, (pVideoPacket->pPHVS->uVideoStreamIndexAndType >> 4) & 0x0F , iVideoWidth, iVideoHeight, pVideoRawStreamData, pPHVSImp->uVideoDataLength, pVideoPacket->pPH->total_length);

      g_SMControllerRTInfo.uOutputedVideoPackets[g_SMControllerRTInfo.iCurrentIndex]++;
      if ( pVideoPacket->pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
         g_SMControllerRTInfo.uOutputedVideoPacketsRetransmitted[g_SMControllerRTInfo.iCurrentIndex]++;
      if ( pVideoPacket->bReconstructed )
      {
         if ( pVideoBlock->iReconstructedECUsed > g_SMControllerRTInfo.uOutputedVideoPacketsMaxECUsed[g_SMControllerRTInfo.iCurrentIndex] )
            g_SMControllerRTInfo.uOutputedVideoPacketsMaxECUsed[g_SMControllerRTInfo.iCurrentIndex] = pVideoBlock->iReconstructedECUsed;
         
         if ( pVideoBlock->iReconstructedECUsed == 1 )
            g_SMControllerRTInfo.uOutputedVideoPacketsSingleECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
         else if ( pVideoBlock->iReconstructedECUsed == 2 )
            g_SMControllerRTInfo.uOutputedVideoPacketsTwoECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
         else
            g_SMControllerRTInfo.uOutputedVideoPacketsMultipleECUsed[g_SMControllerRTInfo.iCurrentIndex]++;
         pVideoBlock->iReconstructedECUsed = 0;
      }

      pVideoPacket = m_pVideoRxBuffer->getMarkFirstPacketToOutput(&pVideoBlock, bSkipIncompleteBlocks);
   }

   return 1;
}


void ProcessorRxVideo::updateControllerRTInfoAndVideoDecodingStats(u8* pRadioPacket, int iPacketLength)
{
   if ( (m_iIndexVideoDecodeStats < 0) || (m_iIndexVideoDecodeStats >= MAX_VIDEO_PROCESSORS) )
      return;
   t_packet_header_video_segment* pPHVS = (t_packet_header_video_segment*) (pRadioPacket+sizeof(t_packet_header));    

   if ( g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile != pPHVS->uCurrentVideoLinkProfile )
   {
      // Video profile changed on the received stream
      if ( pPHVS->uCurrentVideoLinkProfile == VIDEO_PROFILE_MQ ||
           pPHVS->uCurrentVideoLinkProfile == VIDEO_PROFILE_LQ ||
           g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile == VIDEO_PROFILE_MQ ||
           g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile == VIDEO_PROFILE_LQ )
      {
         if ( pPHVS->uCurrentVideoLinkProfile > g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile )
            g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_LOWER;
         else
            g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_HIGHER;
      }
      else
         g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_USER_SELECTABLE;
   }

   memcpy( &(g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS), pPHVS, sizeof(t_packet_header_video_segment));

   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_SIZE )
   if ( 0 != pPHVS->uCurrentBlockIndex )
   {
      if ( (g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth != (int)(pPHVS->uStreamInfo & 0xFFFF)) ||
           (g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight != (int)((pPHVS->uStreamInfo >> 16) & 0xFFFF)) )
      {
          m_uLastVideoBlockIndexResolutionChange = pPHVS->uCurrentBlockIndex;
          m_uLastVideoBlockPacketIndexResolutionChange = pPHVS->uCurrentBlockPacketIndex;
      }
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoWidth = pPHVS->uStreamInfo & 0xFFFF;
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoHeight = (pPHVS->uStreamInfo >> 16) & 0xFFFF;
   }
   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_FPS )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].iCurrentVideoFPS = pPHVS->uStreamInfo;
   }
   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_FEC_TIME )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uCurrentFECTimeMicros = pPHVS->uStreamInfo;
   }
   if ( pPHVS->uStreamInfoFlags == VIDEO_STREAM_INFO_FLAG_VIDEO_PROFILE_FLAGS )
   {
      g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].uCurrentVideoProfileEncodingFlags = pPHVS->uStreamInfo;
   }
}


int ProcessorRxVideo::checkAndRequestMissingPackets(bool bForceSyncNow)
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(m_uVehicleId);
   Model* pModel = findModelWithId(m_uVehicleId, 179);
   if ( (NULL == pModel) || (NULL == pRuntimeInfo) )
      return -1;
   if ( (NULL == m_pVideoRxBuffer) || (0 == m_pVideoRxBuffer->getBlocksCountInBuffer()) )
      return -1;

   if ( rx_video_out_is_stream_output_disabled() )
      return -1;

   int iVideoProfileNow = g_SM_VideoDecodeStats.video_streams[m_iIndexVideoDecodeStats].PHVS.uCurrentVideoLinkProfile;
   m_iMilisecondsMaxRetransmissionWindow = ((pModel->video_link_profiles[iVideoProfileNow].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK) >> 8) * 5;

   checkAndDiscardBlocksTooOld();

   if ( g_bSearching || g_bUpdateInProgress || m_bPaused || pModel->is_spectator || test_link_is_in_progress() || g_bNegociatingRadioLinks || (g_TimeNow < g_uTimeEndedNegiciateRadioLink + 3000) )
      return -1;
   if ( (! pRuntimeInfo->bIsPairingDone) || (g_TimeNow < pRuntimeInfo->uPairingRequestTime + 100) )
      return -1;
   if ( pModel->isVideoLinkFixedOneWay() || (!(pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS)) )
      return -1;

   // Do not request from models 9.7 or older
   if ( get_sw_version_build(pModel) < 242 )
      return -1;

   // If we haven't received any video yet, don't try retransmissions
   if ( (0 == m_uLatestVideoPacketReceiveTime) || (-1 == m_iIndexVideoDecodeStats) )
      return -1;

   if ( g_TimeNow < g_TimeLastVideoParametersOrProfileChanged + 200 )
      return -1;

   // If too much time since we last received a new video packet, then discard the entire rx buffer
   if ( m_iMilisecondsMaxRetransmissionWindow > 10 )
   if ( g_TimeNow >= m_uLatestVideoPacketReceiveTime + m_iMilisecondsMaxRetransmissionWindow - 10 )
   {
      log_line("[ProcessorRxVideo] Discard old blocks due to no new video packet for %d ms", m_iMilisecondsMaxRetransmissionWindow);
      m_pVideoRxBuffer->emptyBuffers("No new video received since start of retransmission window");
      resetOutputState();
      return -1;
   }

   
   if ( (!bForceSyncNow) && (g_TimeNow < m_uLastTimeRequestedRetransmission + m_uTimeIntervalMsForRequestingRetransmissions) )
      return -1;

   if ( g_TimeNow >= m_uLastTimeRequestedRetransmission + m_iMilisecondsMaxRetransmissionWindow )
      m_uTimeIntervalMsForRequestingRetransmissions = 10;
   else if ( m_uTimeIntervalMsForRequestingRetransmissions < 50 )
      m_uTimeIntervalMsForRequestingRetransmissions += 5;

   // If link is lost, do not request retransmissions
   if ( pRuntimeInfo->bIsVehicleLinkToControllerLostAlarm )
      return -1;

   if ( (!bForceSyncNow) && (0 != g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds) )
   {
      u32 uDelta = (u32)g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds;
      if ( uDelta > 50 )
      if ( uDelta > (u32)m_iMilisecondsMaxRetransmissionWindow )
      if ( g_TimeNow > pRuntimeInfo->uLastTimeReceivedAckFromVehicle + uDelta )
         return -1;
   }


   // Request all missing packets except current block

   //#define PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS 20
   // params after header:
   //   u32: retransmission request id
   //   u8: video stream index
   //   u8: number of video packets requested
   //   (u32+u8)*n = each (video block index + video packet index) requested 

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = m_uVehicleId;
   if ( m_uVehicleId == 0 || m_uVehicleId == MAX_U32 )
   {
      PH.vehicle_id_dest = pModel->uVehicleId;
      log_softerror_and_alarm("[ProcessorRxVideo] Tried to request retransmissions before having received a video packet.");
   }

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   m_uRequestRetransmissionUniqueId++;
   memcpy(packet + sizeof(t_packet_header), (u8*)&m_uRequestRetransmissionUniqueId, sizeof(u32));
   memcpy(packet + sizeof(t_packet_header) + sizeof(u32), (u8*)&m_uVideoStreamIndex, sizeof(u8));
   u8* pDataInfo = packet + sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8);
   u32 uTopVideoBlockIndexInBuffer = 0;
   u32 uTopVideoBlockPacketIndexInBuffer = 0xFF;
   u32 uTopVideoBlockLastRecvTime = 0;
   u32 uLastRequestedVideoBlockIndex = 0;
   int iLastRequestedVideoBlockPacketIndex = 0;

   char szBufferBlocks[128];
   szBufferBlocks[0] = 0;

   int iCountPacketsRequested = 0;
   int iCountBlocks = m_pVideoRxBuffer->getBlocksCountInBuffer();
   for( int i=0; i<iCountBlocks; i++ )
   {
      type_rx_video_block_info* pVideoBlock = m_pVideoRxBuffer->getVideoBlockInBuffer(i);
      if ( NULL == pVideoBlock )
         continue;
      
      if ( i < 3 )
      {
         // To remove to revert
         if ( 0 != szBufferBlocks[0] )
            strcat(szBufferBlocks, ", ");
         char szTmp[128];
         sprintf(szTmp, "[%u: %d/%d pckts]", pVideoBlock->uVideoBlockIndex, pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets);
         strcat(szBufferBlocks, szTmp);
      }
      int iExpectedDataPackets = pVideoBlock->iBlockDataPackets;
      if ( pVideoBlock->iEndOfFramePacketIndex >= 0 )
         iExpectedDataPackets = pVideoBlock->iEndOfFramePacketIndex+1;
      int iCountToRequestFromBlock = iExpectedDataPackets - pVideoBlock->iRecvDataPackets - pVideoBlock->iRecvECPackets;

      // Ignore current last video block if it's still receiving packets now or has not received any packet at all
      if ( i == iCountBlocks-1 )
      if ( iCountToRequestFromBlock > 0 )
      {
         uTopVideoBlockIndexInBuffer = pVideoBlock->uVideoBlockIndex;
         uTopVideoBlockLastRecvTime = pVideoBlock->uReceivedTime;
         bool bRequestData = false;

         // Reached EC stage but not enough EC packets to reconstruct
         if ( pVideoBlock->iRecvECPackets > 0 )
         if ( (pVideoBlock->iRecvDataPackets + pVideoBlock->iBlockECPackets) < iExpectedDataPackets )
         {
             log_line("[AdaptiveVideo] Req top block as it has only %d/%d recv data/ec pckts (expected %d data p), now t: %u",
                pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets, iExpectedDataPackets, g_TimeNow);
             bRequestData = true;
         }

         if ( ! bRequestData )
         if ( pVideoBlock->uReceivedTime < g_TimeNow - DEFAULT_VIDEO_END_FRAME_DETECTION_TIMEOUT )
         if ( pVideoBlock->iRecvDataPackets > 0 )
         {
            log_line("[AdaptiveVideo] Req top block pckts as it has a rx time gap (last recv pckt was %u ms ago, time now: %u)",
              g_TimeNow - pVideoBlock->uReceivedTime, g_TimeNow);
            bRequestData = true;
         }
         if ( get_sw_version_build(pModel) < 275 )
         {
            bRequestData = false;
            iCountToRequestFromBlock = 0;
         }

         if ( ! bRequestData )
            continue;
         log_line("[AdaptiveVideo] Top block (%u) has %d/%d recv data/ec packets, eof: %d, will request %d packets.", pVideoBlock->uVideoBlockIndex, pVideoBlock->iRecvDataPackets, pVideoBlock->iRecvECPackets, pVideoBlock->iEndOfFramePacketIndex, iCountToRequestFromBlock);
      }
      if ( iCountToRequestFromBlock > 0 )
      {
         for( int k=0; k<iExpectedDataPackets; k++ )
         {
            if ( NULL == pVideoBlock->packets[k].pRawData )
               continue;
            if ( ! pVideoBlock->packets[k].bEmpty )
               continue;

            uLastRequestedVideoBlockIndex = pVideoBlock->uVideoBlockIndex;
            iLastRequestedVideoBlockPacketIndex = k;
            memcpy(pDataInfo, &pVideoBlock->uVideoBlockIndex, sizeof(u32));
            pDataInfo += sizeof(u32);
            u8 uPacketIndex = k;
            memcpy(pDataInfo, &uPacketIndex, sizeof(u8));
            pDataInfo += sizeof(u8);

            iCountToRequestFromBlock--;
            iCountPacketsRequested++;
            if ( iCountToRequestFromBlock == 0 )
               break;
            if ( iCountPacketsRequested >= DEFAULT_VIDEO_RETRANS_MAX_PCOUNT )
              break;
         }
      }
      if ( iCountPacketsRequested >= DEFAULT_VIDEO_RETRANS_MAX_PCOUNT )
        break;
   }

   if ( iCountPacketsRequested == 0 )
      return 0;

   u8 uCount = iCountPacketsRequested;
   memcpy(packet + sizeof(t_packet_header) + sizeof(u32) + sizeof(u8), (u8*)&uCount, sizeof(u8));
   PH.total_length = sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8);
   PH.total_length += iCountPacketsRequested*(sizeof(u32) + sizeof(u8)); 
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));

   m_uLastTimeRequestedRetransmission = g_TimeNow;

   controller_runtime_info_vehicle* pRTInfo = controller_rt_info_get_vehicle_info(&g_SMControllerRTInfo, m_uVehicleId);
   if ( NULL != pRTInfo )
   {
      pRTInfo->uCountReqRetransmissions[g_SMControllerRTInfo.iCurrentIndex]++;
      if ( pRTInfo->uCountReqRetrPackets[g_SMControllerRTInfo.iCurrentIndex] + uCount > 255 )
         pRTInfo->uCountReqRetrPackets[g_SMControllerRTInfo.iCurrentIndex] = 255;
      else
         pRTInfo->uCountReqRetrPackets[g_SMControllerRTInfo.iCurrentIndex] += uCount;
   }

   pDataInfo = packet + sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8);
   u32 uFirstReqBlockIndex =0;
   memcpy(&uFirstReqBlockIndex, pDataInfo, sizeof(u32));
   log_line("[AdaptiveVideo] * Requested retr id %u from vehicle for %d packets ([%u/%d]...[%u/%d])",
      m_uRequestRetransmissionUniqueId, iCountPacketsRequested,
      uFirstReqBlockIndex, (int)pDataInfo[sizeof(u32)], uLastRequestedVideoBlockIndex, iLastRequestedVideoBlockPacketIndex);
   
   log_line("[AdaptiveVideo] * Video blocks in buffer: %d (%s), top/max video block index in buffer: [%u/%u] / [%u/%d]",
      iCountBlocks, szBufferBlocks, uTopVideoBlockIndexInBuffer, uTopVideoBlockPacketIndexInBuffer, m_pVideoRxBuffer->getMaxReceivedVideoBlockIndex(), m_pVideoRxBuffer->getMaxReceivedVideoBlockPacketIndex());
   log_line("[AdaptiveVideo] Max Video block packet received: [%u/%d], top video block last recv time: %u ms ago",
      m_pVideoRxBuffer->getMaxReceivedVideoBlockIndex(), m_pVideoRxBuffer->getMaxReceivedVideoBlockPacketIndex(), g_TimeNow - uTopVideoBlockLastRecvTime);
   packets_queue_add_packet(&s_QueueRadioPacketsHighPrio, packet);
   return iCountPacketsRequested;
}

void discardRetransmissionsInfoAndBuffersOnLengthyOp()
{
   log_line("[ProcessorRxVideo] Discard all retransmissions info after a lengthy router operation.");
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL == g_pVideoProcessorRxList[i] )
         break;
      g_pVideoProcessorRxList[i]->discardRetransmissionsInfo();
   }
}