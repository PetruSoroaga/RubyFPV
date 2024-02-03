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
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopacketsqueue.h"

#include "shared_vars.h"
#include "processor_rx_video.h"
#include "rx_video_output.h"
#include "video_link_adaptive.h"
#include "video_link_keyframe.h"
#include "packets_utils.h"
#include "links_utils.h"
#include "timers.h"

typedef struct
{
   unsigned int fec_decode_missing_packets_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int fec_decode_fec_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_data_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_fec_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int missing_packets_count;
}
type_fec_info;


type_fec_info s_FECInfo;

bool ProcessorRxVideo::m_sbFECInitialized = false;
int ProcessorRxVideo::m_siInstancesCount = 0;
FILE* ProcessorRxVideo::m_fdLogFile = NULL;

void ProcessorRxVideo::oneTimeInit()
{
   m_siInstancesCount = 0;
   m_sbFECInitialized = false;
   m_fdLogFile = NULL;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      g_pVideoProcessorRxList[i] = NULL;
   log_line("[VideoRx] Did one time initialization.");
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
#define log log_line

ProcessorRxVideo::ProcessorRxVideo(u32 uVehicleId, u32 uVideoStreamIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siInstancesCount;
   m_siInstancesCount++;

   log("[VideoRx] Created new instance (number %d of %d) for VID: %u, stream %u", m_iInstanceIndex+1, m_siInstancesCount, uVehicleId, uVideoStreamIndex);
   m_uVehicleId = uVehicleId;
   m_uVideoStreamIndex = uVideoStreamIndex;
   m_uLastTimeRequestedRetransmission = 0;
   m_uRequestRetransmissionUniqueId = 0;
   m_TimeLastVideoStatsUpdate = 0;
   m_TimeLastHistoryStatsUpdate = 0;
   m_TimeLastRetransmissionsStatsUpdate = 0;
   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
      m_pRXBlocksStack[i] = NULL;
}

ProcessorRxVideo::~ProcessorRxVideo()
{
   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   {
      if ( NULL == m_pRXBlocksStack[i] )
         continue;

      for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
      {
         if ( NULL != m_pRXBlocksStack[i]->packetsInfo[k].pData )
            free(m_pRXBlocksStack[i]->packetsInfo[k].pData);
      }
      free(m_pRXBlocksStack[i]);
   }
   log("[VideoRx] Video processor deleted for VID %u, video stream %u", m_uVehicleId, m_uVideoStreamIndex);

   m_siInstancesCount--;

   if ( 0 == m_siInstancesCount )
   {
      if ( m_fdLogFile != NULL )
         fclose(m_fdLogFile);
      m_fdLogFile = NULL;
   }
}

bool ProcessorRxVideo::init()
{
   if ( m_bInitialized )
      return true;
   m_bInitialized = true;

   m_fdLogFile = NULL;
   //m_fdLogFile = fopen(LOG_FILE_VIDEO, "a+");

   log("[VideoRx] Initialize video processor Rx instance number %d, for VID %u, video stream index %u", m_iInstanceIndex+1, m_uVehicleId, m_uVideoStreamIndex);

   if ( ! m_sbFECInitialized )
   {
      fec_init();
      m_sbFECInitialized = true;
   }

   m_iRXBlocksStackTopIndex = -1;
   m_iRXMaxBlocksToBuffer = 0;

   resetRetransmissionsStats();
   
   video_link_adaptive_init(m_uVehicleId);
   video_link_keyframe_init(m_uVehicleId);

   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log("[VideoRx] Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   
   memset((u8*)&m_SM_VideoDecodeStats, 0, sizeof(shared_mem_video_stream_stats));

   m_SM_VideoDecodeStats.uVehicleId = m_uVehicleId;
   m_SM_VideoDecodeStats.uVideoStreamIndex = m_uVideoStreamIndex;
   m_SM_VideoDecodeStats.isRecording = 0;
   m_SM_VideoDecodeStats.frames_type = RADIO_FLAGS_FRAME_TYPE_DATA;
   m_SM_VideoDecodeStats.width = m_SM_VideoDecodeStats.height = 0;
   m_SM_VideoDecodeStats.total_DiscardedBuffers = 0;
   m_SM_VideoDecodeStats.total_DiscardedLostPackets = 0;
   m_SM_VideoDecodeStats.totalEncodingSwitches = 0;
   m_SM_VideoDecodeStats.uEncodingExtraFlags2 = 0;

   Model* pModel = findModelWithId(m_uVehicleId, 120);
   if ( NULL != pModel )
   {
      log("[VideoRx] Using current model for initialization. Model SW version: %d.%d (b %d)",
         (pModel->sw_version >>8) & 0xFF,
         pModel->sw_version & 0xFF,
         (pModel->sw_version)>>16);

      m_SM_VideoDecodeStats.video_stream_and_type = 0;
      m_SM_VideoDecodeStats.video_link_profile = pModel->video_params.user_selected_video_link_profile | (pModel->video_params.user_selected_video_link_profile<<4);
      m_SM_VideoDecodeStats.encoding_extra_flags = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].encoding_extra_flags;
      log_line("[VideoRx] Set default video encoding extra flags based on local model and user selected video profile. Flags: [%s]", str_format_video_encoding_flags(m_SM_VideoDecodeStats.encoding_extra_flags));
      m_SM_VideoDecodeStats.data_packets_per_block = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].block_packets;
      m_SM_VideoDecodeStats.fec_packets_per_block = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].block_fecs;
      m_SM_VideoDecodeStats.video_data_length = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].packet_length;
      m_SM_VideoDecodeStats.fec_time = 0;
      m_SM_VideoDecodeStats.fps = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].fps;
      m_SM_VideoDecodeStats.keyframe_ms = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].keyframe_ms;
      m_SM_VideoDecodeStats.width = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].width;
      m_SM_VideoDecodeStats.height = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].height;
   }
   else
   {
      if ( g_bSearching )
         log_softerror_and_alarm("[VideoRx] Started video rx in search mode!");
      else
         log_softerror_and_alarm("[VideoRx] Started RX Video with no model configured or present! Using default settings.");

      m_SM_VideoDecodeStats.video_stream_and_type = 0;
      m_SM_VideoDecodeStats.video_link_profile = VIDEO_PROFILE_BEST_PERF | (VIDEO_PROFILE_BEST_PERF<<4);
      m_SM_VideoDecodeStats.encoding_extra_flags = ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO | ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
      m_SM_VideoDecodeStats.encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
      log_line("[VideoRx] Set default video encoding extra flags with no model present. Flags: [%s]", str_format_video_encoding_flags(m_SM_VideoDecodeStats.encoding_extra_flags));
      m_SM_VideoDecodeStats.data_packets_per_block = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
      m_SM_VideoDecodeStats.fec_packets_per_block = DEFAULT_VIDEO_BLOCK_FECS_HP;
      m_SM_VideoDecodeStats.video_data_length = DEFAULT_VIDEO_DATA_LENGTH_HP;
      m_SM_VideoDecodeStats.fps = 30;
      m_SM_VideoDecodeStats.keyframe_ms = 0;
      m_SM_VideoDecodeStats.width = 1280;
      m_SM_VideoDecodeStats.height = 720;   
   }

   if ( m_SM_VideoDecodeStats.encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS )
   {
      log_line("[VideoRx] Set retransmissions flag in video stats as it's enabled on current vehicle.");
      m_SM_VideoDecodeStats.isRetransmissionsOn = 1;
   }
   else
   {
      log_line("[VideoRx] Unset retransmissions flag in video stats as it's disabled on current vehicle.");
      m_SM_VideoDecodeStats.isRetransmissionsOn = 0;    
   }

   memset((u8*)&m_SM_VideoDecodeStatsHistory, 0, sizeof(shared_mem_video_stream_stats_history));
   m_SM_VideoDecodeStatsHistory.uVehicleId = m_uVehicleId;
   m_SM_VideoDecodeStatsHistory.uVideoStreamIndex = m_uVideoStreamIndex;
   m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;
   m_SM_RetransmissionsStats.uGraphRefreshIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;
   log("[VideoRx] Using graphs slice interval of %d miliseconds.", m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs);

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   {
      m_pRXBlocksStack[i] = (type_received_block_info*)malloc(sizeof(type_received_block_info));
      for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
         m_pRXBlocksStack[i]->packetsInfo[k].pData = (u8*)malloc(MAX_PACKET_PAYLOAD+1);
   }
   log("[VideoRx] Allocated %u Mb for rx video caching (%d max blocks in buffers)", (u32)MAX_RXTX_BLOCKS_BUFFER*(u32)MAX_TOTAL_PACKETS_IN_BLOCK*(u32)MAX_PACKET_PAYLOAD/(u32)1000/(u32)1000, MAX_RXTX_BLOCKS_BUFFER);
   
   resetReceiveState();
   resetOutputState();
  
   log("[VideoRx] Initial video stream state: %d x %d, %d fps, %d ms kf, %d video data size",
       m_SM_VideoDecodeStats.width, m_SM_VideoDecodeStats.height, m_SM_VideoDecodeStats.fps,
       m_SM_VideoDecodeStats.keyframe_ms, m_SM_VideoDecodeStats.video_data_length);
   log("[VideoRx] Initialize video processor complete.");
   log("[VideoRx] ====================================");
   return true;
}

bool ProcessorRxVideo::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[VideoRx] Uninitialize video processor Rx instance number %d for VID %u, video stream index %d", m_iInstanceIndex+1, m_uVehicleId, m_uVideoStreamIndex);
   
   m_bInitialized = false;
   return true;
}

void ProcessorRxVideo::resetReceiveState()
{
   log("[VideoRx] Start: Reset video RX state and buffers (discarded buffers count: %d)", m_SM_VideoDecodeStats.total_DiscardedBuffers);
   
   m_iRXBlocksStackTopIndex = -1;

   m_LastReceivedVideoPacketInfo.receive_time = 0;
   m_LastReceivedVideoPacketInfo.video_block_index = MAX_U32;
   m_LastReceivedVideoPacketInfo.video_block_packet_index = MAX_U32;
   m_LastReceivedVideoPacketInfo.stream_packet_idx = MAX_U32;

   m_uLastBlockReceivedAckKeyframeInterval = MAX_U32;
   m_uLastBlockReceivedAdaptiveVideoInterval = MAX_U32;
   m_uLastBlockReceivedSetVideoBitrate = MAX_U32;
   m_uLastBlockReceivedEncodingExtraFlags2 = MAX_U32;

   resetRetransmissionsStats();
   
   // Compute how many blocks to buffer

   m_iMilisecondsMaxRetransmissionWindow = ((m_SM_VideoDecodeStats.encoding_extra_flags & 0xFF00) >> 8) * 5;
   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   m_uTimeIntervalMsForRequestingRetransmissions = 10;

   log("[VideoRx] Need buffers to cache %d miliseconds of video", m_iMilisecondsMaxRetransmissionWindow);
   log("[VideoRx] Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);

   u32 videoBitrate = DEFAULT_VIDEO_BITRATE;
   Model* pModel = findModelWithId(m_uVehicleId, 121);
   if ( NULL != pModel )
      videoBitrate = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps;

   log("[VideoRx] Curent video bitrate: %u bps; current data/block: %d, ec/block: %d, video data length: %d", videoBitrate, m_SM_VideoDecodeStats.data_packets_per_block, m_SM_VideoDecodeStats.fec_packets_per_block, m_SM_VideoDecodeStats.video_data_length);
   log("[VideoRx] Curent blocks/sec: about %d blocks", videoBitrate/m_SM_VideoDecodeStats.data_packets_per_block/m_SM_VideoDecodeStats.video_data_length/8);
   
   float miliPerBlock = (float)((u32)(1000 * 8 * (u32)m_SM_VideoDecodeStats.video_data_length * (u32)m_SM_VideoDecodeStats.data_packets_per_block)) / (float)videoBitrate;

   u32 bytesToBuffer = (((float)videoBitrate / 1000.0) * m_iMilisecondsMaxRetransmissionWindow)/8.0;
   log("[VideoRx] Need buffers to cache %u bits of video (%u bytes of video), one video block is %d bytes and lasts %.1f miliseconds",
      bytesToBuffer*8, bytesToBuffer, m_SM_VideoDecodeStats.data_packets_per_block * m_SM_VideoDecodeStats.video_data_length,
      miliPerBlock);
   
   if ( (0 != bytesToBuffer) && (0 != m_SM_VideoDecodeStats.video_data_length) && (0 != m_SM_VideoDecodeStats.data_packets_per_block) )
      m_iRXMaxBlocksToBuffer = 2 + bytesToBuffer/m_SM_VideoDecodeStats.video_data_length/m_SM_VideoDecodeStats.data_packets_per_block;
   else
      m_iRXMaxBlocksToBuffer = 5;

   // Add extra time for last retransmissions (50 milisec)
   if ( miliPerBlock > 0.0001 )
      m_iRXMaxBlocksToBuffer += 50.0/miliPerBlock;

   m_iRXMaxBlocksToBuffer *= 2.0;

   if ( m_iRXMaxBlocksToBuffer >= MAX_RXTX_BLOCKS_BUFFER )
   {
      m_iRXMaxBlocksToBuffer = MAX_RXTX_BLOCKS_BUFFER-1;
      log("[VideoRx] Capped max video Rx cache to %d blocks", m_iRXMaxBlocksToBuffer);
   }

   log("[VideoRx] Computed result: Will cache a maximum of %d video blocks, for a total of %d miliseconds of video (max retransmission window is %d ms); one block stores %.1f miliseconds of video",
       m_iRXMaxBlocksToBuffer, (int)(miliPerBlock * (float)m_iRXMaxBlocksToBuffer), m_iMilisecondsMaxRetransmissionWindow, miliPerBlock);
   
   resetReceiveBuffers();

   // Reset received packets statistics

   m_SM_VideoDecodeStats.maxBlocksAllowedInBuffers = m_iRXMaxBlocksToBuffer;
   m_SM_VideoDecodeStats.currentPacketsInBuffers = 0;
   m_SM_VideoDecodeStats.maxPacketsInBuffers = 10;
   m_SM_VideoDecodeStats.total_DiscardedSegments = 0;

   
   m_SM_VideoDecodeStatsHistory.totalCurrentlyMissingPackets = 0;
   for( int i=0; i<MAX_HISTORY_VIDEO_INTERVALS; i++ )
   {
      m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[i] = 0;
      m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[i] = 0;
      m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[i] = 0;
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[i] = 0;
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[i] = 0;
      m_SM_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[i] = 0;
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksBadPerPeriod[i] = 0;
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[i] = 0;
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksRetrasmitedPerPeriod[i] = 0;
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[i] = 0;
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[i] = 0;
      m_SM_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[i] = 0;
   }

   //s_TimeLastHistoryStatsUpdate = g_TimeNow;

   m_uLastHardEncodingsChangeVideoBlockIndex = MAX_U32;
   m_uLastVideoResolutionChangeVideoBlockIndex = MAX_U32;
   m_uEncodingsChangeCount = 0;
   m_uLastReceivedVideoLinkProfile = 0;

   m_uTimeLastVideoStreamChanged = g_TimeNow;

   log("[VideoRx] End: Reset video RX state and buffers (total discarded buffers: %d)", m_SM_VideoDecodeStats.total_DiscardedBuffers);
   log("--------------------------------------------------------");
   log("");
}

void ProcessorRxVideo::resetOutputState()
{
   m_uLastOutputVideoBlockTime = 0;
   m_uLastOutputVideoBlockIndex = MAX_U32;
}

void ProcessorRxVideo::resetReceiveBuffers()
{
   for( int i=0; i<=m_iRXMaxBlocksToBuffer; i++ )
   {
      m_pRXBlocksStack[i]->data_packets = MAX_TOTAL_PACKETS_IN_BLOCK;
      m_pRXBlocksStack[i]->fec_packets = 0;
      resetReceiveBuffersBlock(i);
   }

   m_iRXBlocksStackTopIndex = -1;

   m_SM_VideoDecodeStatsHistory.totalCurrentlyMissingPackets = 0;
   m_SM_VideoDecodeStats.currentPacketsInBuffers = 0;
   m_SM_VideoDecodeStats.maxPacketsInBuffers = 10;
   m_SM_VideoDecodeStats.total_DiscardedBuffers++;
}

void ProcessorRxVideo::resetReceiveBuffersBlock(int rx_buffer_block_index)
{
   for( int k=0; k<m_pRXBlocksStack[rx_buffer_block_index]->data_packets + m_pRXBlocksStack[rx_buffer_block_index]->fec_packets; k++ )
   {
      m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[k].state = RX_PACKET_STATE_EMPTY;
      m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[k].uRetrySentCount = 0;
      m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[k].uTimeFirstRetrySent = 0;
      m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[k].uTimeLastRetrySent = 0;
      m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[k].packet_length = 0;
   }

   m_pRXBlocksStack[rx_buffer_block_index]->video_block_index = MAX_U32;
   m_pRXBlocksStack[rx_buffer_block_index]->packet_length = 0;
   m_pRXBlocksStack[rx_buffer_block_index]->data_packets = 0;
   m_pRXBlocksStack[rx_buffer_block_index]->fec_packets = 0;
   m_pRXBlocksStack[rx_buffer_block_index]->received_data_packets = 0;
   m_pRXBlocksStack[rx_buffer_block_index]->received_fec_packets = 0;
   m_pRXBlocksStack[rx_buffer_block_index]->totalPacketsRequested = 0;
   m_pRXBlocksStack[rx_buffer_block_index]->uTimeFirstPacketReceived = MAX_U32;
   m_pRXBlocksStack[rx_buffer_block_index]->uTimeFirstRetrySent = 0;
   m_pRXBlocksStack[rx_buffer_block_index]->uTimeLastRetrySent = 0;
   m_pRXBlocksStack[rx_buffer_block_index]->uTimeLastUpdated = 0;
}

void ProcessorRxVideo::resetState()
{
   resetReceiveState();
   resetOutputState();
}

void ProcessorRxVideo::resetRetransmissionsStats()
{
   memset((u8*)&m_SM_RetransmissionsStats, 0, sizeof(shared_mem_controller_retransmissions_stats));
   log("[VideoRx] VID %u, video stream %u: Reseting retransmissions stats...", m_uVehicleId, m_uVideoStreamIndex);
   
   m_SM_RetransmissionsStats.uVehicleId = m_uVehicleId;
   m_SM_RetransmissionsStats.uVideoStreamIndex = m_uVideoStreamIndex;
   m_SM_RetransmissionsStats.uGraphRefreshIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;
   m_SM_RetransmissionsStats.totalRequestedRetransmissions = 0;
   m_SM_RetransmissionsStats.totalRequestedRetransmissionsLast500Ms = 0;

   m_SM_RetransmissionsStats.totalReceivedRetransmissions = 0;
   m_SM_RetransmissionsStats.totalReceivedRetransmissionsLast500Ms = 0;

   m_SM_RetransmissionsStats.totalRequestedVideoPackets = 0;
   m_SM_RetransmissionsStats.totalReceivedVideoPackets = 0;

   m_SM_RetransmissionsStats.iCountActiveRetransmissions = 0;
   
   for( int i=0; i<MAX_HISTORY_VIDEO_INTERVALS; i++ )
   {
      m_SM_RetransmissionsStats.history[i].uCountRequestedRetransmissions = 0;
      m_SM_RetransmissionsStats.history[i].uCountAcknowledgedRetransmissions = 0;
      m_SM_RetransmissionsStats.history[i].uCountDroppedRetransmissions = 0;
      m_SM_RetransmissionsStats.history[i].uCountRequestedPacketsForRetransmission = 0;
      m_SM_RetransmissionsStats.history[i].uCountReRequestedPacketsForRetransmission = 0;
      m_SM_RetransmissionsStats.history[i].uCountReceivedRetransmissionPackets = 0;
      m_SM_RetransmissionsStats.history[i].uCountReceivedRetransmissionPacketsDuplicate = 0;
      m_SM_RetransmissionsStats.history[i].uCountReceivedRetransmissionPacketsDropped = 0;

      m_SM_RetransmissionsStats.history[i].uMinRetransmissionRoundtripTime = 0;
      m_SM_RetransmissionsStats.history[i].uMaxRetransmissionRoundtripTime = 0;
      m_SM_RetransmissionsStats.history[i].uAverageRetransmissionRoundtripTime = 0;
   }

   m_SM_VideoDecodeStats.total_DiscardedLostPackets = 0;
   m_SM_VideoDecodeStats.total_DiscardedSegments = 0;
   m_SM_VideoDecodeStats.total_DiscardedBuffers = 0;

   log("[VideoRx] VID %u, video stream %u: Reseting retransmissions stats complete", m_uVehicleId, m_uVideoStreamIndex);
}

void ProcessorRxVideo::onControllerSettingsChanged()
{
   log("[VideoRx] VID %u, video stream %u: Controller params changed. Reinitializing RX video state...", m_uVehicleId, m_uVideoStreamIndex);

   u32 tmp1 = m_uLastHardEncodingsChangeVideoBlockIndex;
   u32 tmp2 = m_uLastVideoResolutionChangeVideoBlockIndex;

   m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;
   m_SM_RetransmissionsStats.uGraphRefreshIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;
   log("[VideoRx] Using graphs slice interval of %d miliseconds.", m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs);

   m_uRetryRetransmissionAfterTimeoutMiliseconds = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log("[VideoRx]: Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", m_uRetryRetransmissionAfterTimeoutMiliseconds, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   
   resetReceiveState();
   resetOutputState();

   m_uLastHardEncodingsChangeVideoBlockIndex = tmp1;
   m_uLastVideoResolutionChangeVideoBlockIndex = tmp2;
}
      
void ProcessorRxVideo::logCurrentRxBuffers(bool bIncludeRetransmissions)
{
   char szBuff[256];
   sprintf(szBuff, "DBG: last output video block index: %u, (last recv: [%u/%d]) stack_ptr: %d ", m_uLastOutputVideoBlockIndex, m_LastReceivedVideoPacketInfo.video_block_index, m_LastReceivedVideoPacketInfo.video_block_packet_index, m_iRXBlocksStackTopIndex);
   for( int i=0; i<=m_iRXBlocksStackTopIndex; i++)
   {
      if ( i > 3 )
      {
         strcat(szBuff,"...");
         break;
      }
      if ( i > 0 )
         strcat(szBuff, ", ");
      char szTmp[32];
      sprintf(szTmp, "[%u: ", m_pRXBlocksStack[i]->video_block_index);
      strcat(szBuff, szTmp);
      for( int k=0; k<m_pRXBlocksStack[i]->data_packets + m_pRXBlocksStack[i]->fec_packets; k++ )
      {
         if ( m_pRXBlocksStack[i]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
            sprintf(szTmp,"%d", k);
         else
            sprintf(szTmp, "x");
         strcat(szBuff, szTmp);
      }
      strcat(szBuff, "]");
   }
   log_line(szBuff);

   if ( bIncludeRetransmissions )
   {
      char szTmp[32];
      sprintf(szBuff, "DBG: first block retransmission requests: block %u = [", m_pRXBlocksStack[0]->video_block_index);
      for( int k=0; k<m_pRXBlocksStack[0]->data_packets + m_pRXBlocksStack[0]->fec_packets; k++ )
      {
         if ( 0 != k )
            strcat(szBuff, ", ");
         if ( m_pRXBlocksStack[0]->packetsInfo[k].uTimeFirstRetrySent == 0 ||
              m_pRXBlocksStack[0]->packetsInfo[k].uTimeLastRetrySent == 0 )
            strcat(szBuff, "(!)");

         sprintf(szTmp,"%d", m_pRXBlocksStack[0]->packetsInfo[k].uRetrySentCount);
         strcat(szBuff, szTmp);

         if ( m_pRXBlocksStack[0]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
            strcat(szBuff, "(r)");
      }
      strcat(szBuff, "]");
      log_line(szBuff);
   }
}

void ProcessorRxVideo::checkAndDiscardBlocksTooOld()
{
   if ( m_iRXBlocksStackTopIndex < 0 )
      return;

   if ( m_pRXBlocksStack[0]->uTimeLastUpdated >= g_TimeNow - m_iMilisecondsMaxRetransmissionWindow*1.5 )
      return;

   if ( m_pRXBlocksStack[m_iRXBlocksStackTopIndex]->uTimeLastUpdated < g_TimeNow - m_iMilisecondsMaxRetransmissionWindow*1.5 )
   {
      updateHistoryStatsDiscaredAllStack();
      resetReceiveBuffers();
      resetOutputState();
      return;
   }


   // Find first block that is too old (from top to bottom)

   int iStackIndex = m_iRXBlocksStackTopIndex;
   while ( iStackIndex >= 0 )
   {
      if ( m_pRXBlocksStack[iStackIndex]->uTimeFirstPacketReceived != MAX_U32 )
      if ( m_pRXBlocksStack[iStackIndex]->uTimeFirstPacketReceived < g_TimeNow - (u32)m_iMilisecondsMaxRetransmissionWindow )
      {
         break;
      }
      iStackIndex--;
   }

   // Discard blocks, from 0 to (index, inclusive) (to first that is not too old) (they are not past the retransmission window)
   // Block at iStackIndex is the first that is too old
   if ( iStackIndex >= 0 )
   {
      updateHistoryStatsDiscaredStackSegment(iStackIndex+1);
      pushIncompleteBlocksOut(iStackIndex+1, true);
   }
}

void ProcessorRxVideo::sendPacketToOutput(int rx_buffer_block_index, int block_packet_index)
{
   u32 video_block_index = m_pRXBlocksStack[rx_buffer_block_index]->video_block_index;

   if ( MAX_U32 == video_block_index || 0 == m_pRXBlocksStack[rx_buffer_block_index]->data_packets )
      return;

   if ( m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[block_packet_index].state != RX_PACKET_STATE_RECEIVED )
   {
      m_SM_VideoDecodeStats.total_DiscardedLostPackets++;
      return;
   }

   if ( g_bDebug )
      return;

   u8* pBuffer = m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[block_packet_index].pData;
   int length = m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[block_packet_index].packet_length;

   processor_rx_video_forward_video_data(m_uVehicleId, m_SM_VideoDecodeStats.width, m_SM_VideoDecodeStats.height, pBuffer, length);
}

void ProcessorRxVideo::pushIncompleteBlocksOut(int countToPush, bool bTooOld)
{
   // Discard blocks, do not output them (unless blocks are good and not too old)

   if ( countToPush <= 0 )
      return;

   #ifdef PROFILE_RX
   u32 uTimeStart = get_current_timestamp_ms();
   #endif

   m_uLastOutputVideoBlockTime = g_TimeNow;

   if ( m_uLastOutputVideoBlockIndex != MAX_U32 )
      m_uLastOutputVideoBlockIndex += (u32) countToPush;
   else
      m_uLastOutputVideoBlockIndex = m_pRXBlocksStack[0]->video_block_index + (u32) countToPush;

   bool bFullDiscard = false;
   if ( countToPush >= m_iRXBlocksStackTopIndex + 1 )
   {
      countToPush = m_iRXBlocksStackTopIndex+1;
      bFullDiscard = true;
      //if ( bTooOld )
      //   _rx_video_log_line("Discarding full Rx stack [0-%d] (too old, newest data in discarded segment was at %02d:%02d.%03d)", s_RXBlocksStackTopIndex, s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated % 1000);
      //else
      //   _rx_video_log_line("Discarding full Rx stack [0-%d] (to make room for newer blocks, discarded blocks indexes [%u-%u], latest received block index: %u", s_RXBlocksStackTopIndex, s_pRXBlocksStack[0]->video_block_index, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->video_block_index, s_LastReceivedVideoPacketInfo.video_block_index);

      m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0] = 0;
   }
   else
   {
      //if ( bReasonTooOld )
      //   _rx_video_log_line("Discarding Rx stack segment [0-%d] of total [0-%d] (too old, newest data in discarded segment was at %02d:%02d.%03d)", countToPush-1, s_RXBlocksStackTopIndex, s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated % 1000);
      //else
      //   _rx_video_log_line("Discarding Rx stack segment [0-%d] (to make room for newer blocks, discarded blocks indexes [%u-%u], latest received block index: %u", countToPush-1, s_pRXBlocksStack[0]->video_block_index, s_pRXBlocksStack[countToPush-1]->video_block_index, s_LastReceivedVideoPacketInfo.video_block_index);
   }

   for( int i=0; i<countToPush; i++ )
   {
      m_SM_VideoDecodeStats.currentPacketsInBuffers -= m_pRXBlocksStack[i]->received_data_packets;
      m_SM_VideoDecodeStats.currentPacketsInBuffers -= m_pRXBlocksStack[i]->received_fec_packets;
      if ( m_SM_VideoDecodeStats.currentPacketsInBuffers < 0 )
         m_SM_VideoDecodeStats.currentPacketsInBuffers = 0;

      if ( m_pRXBlocksStack[i]->received_data_packets + m_pRXBlocksStack[i]->received_fec_packets >= m_pRXBlocksStack[i]->data_packets )
      if ( m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0] > 0 )
         m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0]--;

      if ( bTooOld )
      {
         m_SM_VideoDecodeStats.total_DiscardedLostPackets += m_pRXBlocksStack[i]->data_packets - m_pRXBlocksStack[i]->received_data_packets;
         resetReceiveBuffersBlock(i);
         continue;
      }

      // Do reconstruction if we have enough data for doing it;
      
      if ( m_pRXBlocksStack[i]->received_data_packets >= m_pRXBlocksStack[i]->data_packets )
      {
         int iIndex = getVehicleRuntimeIndex(m_uVehicleId);
         if ( -1 != iIndex )
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uIntervalsOuputCleanVideoPackets[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uCurrentIntervalIndex]++;  
      }

      if ( (m_pRXBlocksStack[i]->received_data_packets < m_pRXBlocksStack[i]->data_packets) &&
           (m_pRXBlocksStack[i]->received_data_packets + m_pRXBlocksStack[i]->received_fec_packets >= m_pRXBlocksStack[i]->data_packets) )
      {
         reconstructBlock(i);
         int iIndex = getVehicleRuntimeIndex(m_uVehicleId);
         if ( -1 != iIndex )
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uIntervalsOuputRecontructedVideoPackets[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uCurrentIntervalIndex]++;  
      }

      if ( m_pRXBlocksStack[i]->received_data_packets >= m_pRXBlocksStack[i]->data_packets )
      {
         for( int k=0; k<m_pRXBlocksStack[i]->data_packets; k++ )
            sendPacketToOutput(i, k);
      }
      else
         m_SM_VideoDecodeStats.total_DiscardedLostPackets += m_pRXBlocksStack[i]->data_packets - m_pRXBlocksStack[i]->received_data_packets;

      resetReceiveBuffersBlock(i);
   }
         
   if ( bFullDiscard )
   {
      resetReceiveBuffersBlock(0);
      m_iRXBlocksStackTopIndex = -1;
   }
   else
   {
      // Rotate the outputed blocks (first countToPush blocks are moved to the end of the circular buffer, the others are sifhted to the begining of the buffer )
      type_received_block_info* tmpBlocksStack[MAX_RXTX_BLOCKS_BUFFER];
      memcpy((u8*)&(tmpBlocksStack[0]), (u8*)&(m_pRXBlocksStack[0]), (m_iRXBlocksStackTopIndex+1)*sizeof(type_received_block_info*));
      
      // Move blocks to the begining
      for( int i=countToPush; i<=m_iRXBlocksStackTopIndex; i++ )
         m_pRXBlocksStack[i-countToPush] = tmpBlocksStack[i];

      // Move the first N to the end of circular buffer
      for( int i=0; i<countToPush; i++ )
         m_pRXBlocksStack[m_iRXBlocksStackTopIndex-countToPush+i+1] = tmpBlocksStack[i];
      
      m_iRXBlocksStackTopIndex -= countToPush;
   }
   m_SM_VideoDecodeStats.total_DiscardedSegments++;
   
   #ifdef PROFILE_RX
   u32 dTime1 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime1 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Pushing incomplete video blocks out (%d blocks)took too long: %u ms.",  countToPush, dTime1);
   #endif
}

void ProcessorRxVideo::pushFirstBlockOut()
{
   #ifdef PROFILE_RX
   u32 uTimeStart = get_current_timestamp_ms();
   #endif

   // If we have all the data packets or
   // If no recontruction is possible, just output valid data

   int countRetransmittedPackets = 0;
   for( int i=0; i<m_pRXBlocksStack[0]->data_packets; i++ )
      if ( m_pRXBlocksStack[0]->packetsInfo[i].uRetrySentCount > 0 )
      if ( m_pRXBlocksStack[0]->packetsInfo[i].state == RX_PACKET_STATE_RECEIVED )
         countRetransmittedPackets++;

   updateHistoryStatsBlockOutputed(0, 0 != countRetransmittedPackets);

   m_SM_VideoDecodeStats.currentPacketsInBuffers -= m_pRXBlocksStack[0]->received_data_packets;
   m_SM_VideoDecodeStats.currentPacketsInBuffers -= m_pRXBlocksStack[0]->received_fec_packets;
   if ( m_SM_VideoDecodeStats.currentPacketsInBuffers < 0 )
      m_SM_VideoDecodeStats.currentPacketsInBuffers = 0;

   // Do reconstruction (we have enough data for doing it)

   if ( m_pRXBlocksStack[0]->received_data_packets < m_pRXBlocksStack[0]->data_packets )
   {
      
      if ( m_pRXBlocksStack[0]->received_data_packets + m_pRXBlocksStack[0]->received_fec_packets >= m_pRXBlocksStack[0]->data_packets )
      {
         int iIndex = getVehicleRuntimeIndex(m_uVehicleId);
         if ( -1 != iIndex )
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uIntervalsOuputRecontructedVideoPackets[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uCurrentIntervalIndex]++;  

         reconstructBlock(0);
         //_log_current_buffer();
      }
      //else
      //   _rx_video_log_line("Can't reconstruct block %u, has only %d packets of minimum %d required.", s_pRXBlocksStack[0]->video_block_index, s_pRXBlocksStack[0]->received_data_packets + s_pRXBlocksStack[0]->received_fec_packets, s_pRXBlocksStack[0]->data_packets);
   }
   else
   {
      if ( g_PD_ControllerLinkStats.tmp_video_streams_blocks_clean[0] < 254 )
         g_PD_ControllerLinkStats.tmp_video_streams_blocks_clean[0]++;
      else
         g_PD_ControllerLinkStats.tmp_video_streams_blocks_clean[0] = 254;

      int iIndex = getVehicleRuntimeIndex(m_uVehicleId);
      if ( -1 != iIndex )
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uIntervalsOuputCleanVideoPackets[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uCurrentIntervalIndex]++;
   }

   #ifdef PROFILE_RX
   u32 dTime1 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime1 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Pushing first video block out (history update and reconstruction) took too long: %u ms.", dTime1);
   #endif

   // Output the block

   if ( m_pRXBlocksStack[0]->received_data_packets + m_pRXBlocksStack[0]->received_fec_packets >= m_pRXBlocksStack[0]->data_packets )
   if ( m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0] > 0 )
      m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0]--;

   for( int i=0; i<m_pRXBlocksStack[0]->data_packets; i++ )
      sendPacketToOutput(0, i);

   m_uLastOutputVideoBlockTime = g_TimeNow;
   m_uLastOutputVideoBlockIndex = m_pRXBlocksStack[0]->video_block_index;

   resetReceiveBuffersBlock(0);

   // Shift the rx blocks buffers by one block
   if ( m_iRXBlocksStackTopIndex > 0 )
   {
      type_received_block_info* pFirstBlock = m_pRXBlocksStack[0];
      for( int i=0; i<m_iRXBlocksStackTopIndex; i++ )
         m_pRXBlocksStack[i] = m_pRXBlocksStack[i+1];
      m_pRXBlocksStack[m_iRXBlocksStackTopIndex] = pFirstBlock;
   }
   if ( m_iRXBlocksStackTopIndex >= 0 )
      m_iRXBlocksStackTopIndex--;

   #ifdef PROFILE_RX
   u32 dTime2 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime2 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Pushing first video block out (to player) took too long: %u ms.", dTime2);
   #endif
}

u32 ProcessorRxVideo::getLastTimeVideoStreamChanged()
{
   return m_uTimeLastVideoStreamChanged;
}

int ProcessorRxVideo::getCurrentlyReceivedVideoProfile()
{
   if ( 0 == m_SM_VideoDecodeStats.width )
      return -1;
   return (m_SM_VideoDecodeStats.video_link_profile & 0x0F);
}

int ProcessorRxVideo::getCurrentlyReceivedVideoFPS()
{
   if ( 0 == m_SM_VideoDecodeStats.width )
      return -1;
   return m_SM_VideoDecodeStats.fps;
}

int ProcessorRxVideo::getCurrentlyReceivedVideoKeyframe()
{
   if ( 0 == m_SM_VideoDecodeStats.width )
      return -1;
   return m_SM_VideoDecodeStats.keyframe_ms;
}

int ProcessorRxVideo::getCurrentlyMissingVideoPackets()
{
   int iCount = 0;
   for( int i=0; i<m_iRXBlocksStackTopIndex; i++ )
   {
      iCount += m_pRXBlocksStack[i]->data_packets + m_pRXBlocksStack[i]->fec_packets - (m_pRXBlocksStack[i]->received_data_packets + m_pRXBlocksStack[i]->received_fec_packets);
   }
   return iCount;
}

int ProcessorRxVideo::getVideoWidth()
{
   return m_SM_VideoDecodeStats.width;
}

int ProcessorRxVideo::getVideoHeight()
{
   return m_SM_VideoDecodeStats.height;
}

int ProcessorRxVideo::getVideoFPS()
{
   return m_SM_VideoDecodeStats.fps;
}

shared_mem_video_stream_stats* ProcessorRxVideo::getVideoDecodeStats()
{
   return &m_SM_VideoDecodeStats;
}

shared_mem_video_stream_stats_history* ProcessorRxVideo::getVideoDecodeStatsHistory()
{
   return &m_SM_VideoDecodeStatsHistory;
}

shared_mem_controller_retransmissions_stats* ProcessorRxVideo::getControllerRetransmissionsStats()
{
   return &m_SM_RetransmissionsStats;
}


void ProcessorRxVideo::updateHistoryStats(u32 uTimeNow)
{
   if ( uTimeNow < m_TimeLastHistoryStatsUpdate + m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs )
      return;

   m_TimeLastHistoryStatsUpdate = uTimeNow;

   m_SM_VideoDecodeStatsHistory.totalCurrentlyMissingPackets = 0;
   for( int i=0; i<m_iRXBlocksStackTopIndex; i++ )
   {
      int c = m_pRXBlocksStack[i]->data_packets + m_pRXBlocksStack[i]->fec_packets - (m_pRXBlocksStack[i]->received_data_packets + m_pRXBlocksStack[i]->received_fec_packets);
      m_SM_VideoDecodeStatsHistory.totalCurrentlyMissingPackets += c;
   }
   m_SM_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[0] = m_SM_VideoDecodeStatsHistory.totalCurrentlyMissingPackets;

   m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0] = 0;
   for( int i=0; i<m_iRXBlocksStackTopIndex; i++ )
   {
      if ( m_pRXBlocksStack[i]->data_packets > 0 )
      if ( m_pRXBlocksStack[i]->received_data_packets + m_pRXBlocksStack[i]->received_fec_packets >= m_pRXBlocksStack[i]->data_packets )
      if ( m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0] < 255 )
         m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0]++;
   }

   for( int i=MAX_HISTORY_VIDEO_INTERVALS-1; i>0; i-- )
   {
      m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[i] = m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[i-1];
      m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[i] = m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[i-1];
      m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[i] = m_SM_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[i-1];
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[i] = m_SM_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[i-1];
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[i] = m_SM_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[i-1];
      m_SM_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[i] = m_SM_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[i-1];
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksBadPerPeriod[i] = m_SM_VideoDecodeStatsHistory.outputHistoryBlocksBadPerPeriod[i-1];
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[i] = m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[i-1];
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksRetrasmitedPerPeriod[i] = m_SM_VideoDecodeStatsHistory.outputHistoryBlocksRetrasmitedPerPeriod[i-1];
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[i] = m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[i-1];
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[i] = m_SM_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[i-1];
      m_SM_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[i] = m_SM_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[i-1];
   }

   m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[0] = 0;
   m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[0] = 0;
   m_SM_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[0] = 0;
   m_SM_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[0] = 0;
   m_SM_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[0] = 0;
   m_SM_VideoDecodeStatsHistory.outputHistoryBlocksBadPerPeriod[0] = 0;
   m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[0] = 0;
   m_SM_VideoDecodeStatsHistory.outputHistoryBlocksRetrasmitedPerPeriod[0] = 0;
   m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[0] = 0;
   m_SM_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[0] = 0;
   m_SM_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[0] = 0;
}


void ProcessorRxVideo::periodicLoop(u32 uTimeNow)
{
   if ( (m_SM_VideoDecodeStats.encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS) )
   if ( relay_controller_must_display_video_from(g_pCurrentModel, m_uVehicleId) )
   {
      if ( m_LastReceivedVideoPacketInfo.video_block_index != MAX_U32 )
         checkAndRequestMissingPackets();
   }

   // Can we output the first few blocks?

   int maxBlocksToOutputIfAvailable = MAX_BLOCKS_TO_OUTPUT_IF_AVAILABLE;
   do
   {
      if ( m_iRXBlocksStackTopIndex < 0 )
         break;
      if ( m_pRXBlocksStack[0]->data_packets == 0 )
         break;
      if ( m_pRXBlocksStack[0]->received_data_packets + m_pRXBlocksStack[0]->received_fec_packets < m_pRXBlocksStack[0]->data_packets )
         break;

      pushFirstBlockOut();
      maxBlocksToOutputIfAvailable--;
   }
   while ( maxBlocksToOutputIfAvailable > 0 );

   checkAndDiscardBlocksTooOld();

   if ( uTimeNow >= m_TimeLastVideoStatsUpdate+50 )
   {
      m_TimeLastVideoStatsUpdate = uTimeNow;

      m_SM_VideoDecodeStats.frames_type = radio_get_received_frames_type();
      m_SM_VideoDecodeStats.iCurrentRxTxSyncType = g_pCurrentModel->rxtx_sync_type;

      bool bDisableRetransmissionOnLinkLost = false;
      if ( g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds != 0 )
      if ( uTimeNow > g_uTimeLastReceivedResponseToAMessage + g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds )
         bDisableRetransmissionOnLinkLost = true;

      if ( bDisableRetransmissionOnLinkLost )
      {
         if ( m_SM_VideoDecodeStats.isRetransmissionsOn )
         {
            log_line("[VideoRx] Unset retransmissions flag in video stats as link to vehicle is lost for %u ms", uTimeNow - g_uTimeLastReceivedResponseToAMessage);
            m_SM_VideoDecodeStats.isRetransmissionsOn = 0;
         }
      }
      else if ( m_SM_VideoDecodeStats.encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS )
      {
         if ( 0 == m_SM_VideoDecodeStats.isRetransmissionsOn )
         {
            log_line("[VideoRx] Set retransmissions flag in video stats as it's enabled on received video profile and link to vehicle is not lost (last link from vehicle %u ms ago)", uTimeNow - g_uTimeLastReceivedResponseToAMessage);
            m_SM_VideoDecodeStats.isRetransmissionsOn = 1;
         }
      }
      else
      {
         if ( 1 == m_SM_VideoDecodeStats.isRetransmissionsOn )
         {
            log_line("[VideoRx] Unset retransmissions flag in video stats au it's disabled on received video profile.");
            m_SM_VideoDecodeStats.isRetransmissionsOn = 0;
         }
      }
   }

   updateHistoryStats(uTimeNow);

   if ( uTimeNow > m_TimeLastRetransmissionsStatsUpdate + m_SM_RetransmissionsStats.uGraphRefreshIntervalMs )
   {
      m_TimeLastRetransmissionsStatsUpdate = uTimeNow;
      updateRetransmissionsHistoryStats(uTimeNow);
   }
}

// Returns 1 if a video block has just finished and the flag "Can TX" is set

int ProcessorRxVideo::handleReceivedVideoPacket(int interfaceNb, u8* pBuffer, int length)
{
   #ifdef PROFILE_RX
   u32 uTimeStart = get_current_timestamp_ms();
   int iStackIndexStart = m_iRXBlocksStackTopIndex;
   #endif

   checkAndDiscardBlocksTooOld();

   t_packet_header* pPH = (t_packet_header*) pBuffer;
   
   Model* pModel = findModelWithId(pPH->vehicle_id_src, 122);
   if ( NULL == pModel )
   {
      static u32 s_uLastAlarmVideoUnkownTime = 0;
      static u32 s_uTimeLastAlarmVideoUnkownVehicleId = 0;
      if ( (s_uLastAlarmVideoUnkownTime == 0 ) || (pPH->vehicle_id_src != s_uTimeLastAlarmVideoUnkownVehicleId) )
      if ( (s_uLastAlarmVideoUnkownTime == 0 ) || (g_TimeNow > s_uLastAlarmVideoUnkownTime+10000) )
      {
         s_uLastAlarmVideoUnkownTime = g_TimeNow;
         s_uTimeLastAlarmVideoUnkownVehicleId = pPH->vehicle_id_src;
         send_alarm_to_central(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_UNKNOWN_VIDEO, pPH->vehicle_id_src);
      }
      return 0;
   }

   u32 video_block_index = 0;
   u8  video_block_packet_index = 0;
   u8  block_packets;
   u8  block_fecs;

   t_packet_header_video_full_77* pPHVFNew = (t_packet_header_video_full_77*) (pBuffer+sizeof(t_packet_header));    
   video_block_index = pPHVFNew->video_block_index;
   video_block_packet_index = pPHVFNew->video_block_packet_index;
   block_packets = pPHVFNew->block_packets;
   block_fecs = pPHVFNew->block_fecs;

   #ifdef PROFILE_RX
   u32 dTime1 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime1 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Discarding old blocks took too long: %u ms. Stack top before/after: %d/%d", video_block_index , video_block_packet_index, interfaceNb, length, dTime1, iStackIndexStart, m_iRXBlocksStackTopIndex);
   #endif

   int packetsGap = preProcessReceivedVideoPacket(interfaceNb, pBuffer, length);

   #ifdef PROFILE_RX
   u32 dTime2 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime2 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Preprocessing packet took too long: %u ms. Stack top before/after: %d/%d", video_block_index , video_block_packet_index, interfaceNb, length, dTime2, iStackIndexStart, m_iRXBlocksStackTopIndex);
   #endif

   if ( packetsGap < 0 )
   {
      return 0;
   }

   int nReturnCanTx = 0;

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_CAN_START_TX )
   if ( !( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
   if ( video_block_packet_index >= block_packets + block_fecs - 1 )
      nReturnCanTx = 1;

   int added_to_rx_buffer_index = -1;

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
      added_to_rx_buffer_index = processRetransmittedVideoPacket(pBuffer, length);
   else
      added_to_rx_buffer_index = processReceivedVideoPacket(pBuffer, length);

   if ( added_to_rx_buffer_index != 1 )
   {
      if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
      {
         if ( m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[0] < 254 )
            m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[0]++;
      }
      else
      {
         if ( m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[0] < 254 )
            m_SM_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[0]++;
      }
   }

   #ifdef PROFILE_RX
   u32 dTime3 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime3 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Adding packet to buffers took too long: %u ms. Stack top before/after: %d/%d", video_block_index , video_block_packet_index, interfaceNb, length, dTime3, iStackIndexStart, m_iRXBlocksStackTopIndex);
   #endif

   if ( -1 == added_to_rx_buffer_index )
      return nReturnCanTx;

   // Can we output the first few blocks?
   int maxBlocksToOutputIfAvailable = MAX_BLOCKS_TO_OUTPUT_IF_AVAILABLE;
   do
   {
      if ( m_iRXBlocksStackTopIndex < 0 )
         break;
      if ( m_pRXBlocksStack[0]->data_packets == 0 )
         break;
      if ( m_pRXBlocksStack[0]->received_data_packets + m_pRXBlocksStack[0]->received_fec_packets < m_pRXBlocksStack[0]->data_packets )
         break;

      pushFirstBlockOut();
      nReturnCanTx = 1;
      maxBlocksToOutputIfAvailable--;
   }
   while ( maxBlocksToOutputIfAvailable > 0 );

   #ifdef PROFILE_RX
   u32 dTime4 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime4 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Sending video blocks to player took too long: %u ms. Stack top before/after: %d/%d", video_block_index , video_block_packet_index, interfaceNb, length, dTime4, iStackIndexStart, m_iRXBlocksStackTopIndex);
   #endif

   // Output it anyway if not using bidirectional video and we are past the first block

   bool bOutputBocksAsIs = false;
   if ( !( m_SM_VideoDecodeStats.encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS ) )
      bOutputBocksAsIs = true;
   if ( g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds != 0 )
   if ( g_TimeNow > g_uTimeLastReceivedResponseToAMessage + g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds )
      bOutputBocksAsIs = true;

   if ( bOutputBocksAsIs )
   if ( m_iRXBlocksStackTopIndex > 0 )
   {
      pushFirstBlockOut();
      nReturnCanTx = 1;

      #ifdef PROFILE_RX
      u32 dTime5 = get_current_timestamp_ms() - uTimeStart;
      if ( dTime5 >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Force pushing video blocks to player took too long: %u ms. Stack top before/after: %d/%d", video_block_index , video_block_packet_index, interfaceNb, length, dTime5, iStackIndexStart, m_iRXBlocksStackTopIndex);
      #endif

      return nReturnCanTx;
   }

   #ifdef PROFILE_RX
   u32 dTime6 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime6 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Total processing took too long: %u ms. Stack top before/after: %d/%d", video_block_index , video_block_packet_index, interfaceNb, length, dTime6, iStackIndexStart, m_iRXBlocksStackTopIndex);
   #endif

   return nReturnCanTx;
}


type_retransmission_stats s_RetransmissionStats;

u32 s_TimeLastHistoryStatsUpdate = 0;
u32 s_TimeLastControllerRestransmissionsStatsUpdate = 0;


u32 s_LastOutputVideoBlockTime = 0;
u32 s_LastOutputVideoBlockIndex = MAX_U32;

u32 s_LastHardEncodingsChangeVideoBlockIndex = MAX_U32;
u32 s_LastVideoResolutionChangeVideoBlockIndex = MAX_U32;
u32 s_uEncodingsChangeCount = 0;
u8  s_uLastReceivedVideoLinkProfile = 0;
u32 s_LastTimeRequestedRetransmissions = 0;
u32 s_uTimeIntervalMsForRequestingRetransmissions = 10;

u32 s_RetransmissionRetryTimeout = 40; // miliseconds
int s_iMilisecondsMaxRetransmissionWindow = 50;


extern t_packet_queue s_QueueRadioPackets;


void ProcessorRxVideo::updateRetransmissionsHistoryStats(u32 uTimeNow)
{
   if ( ! m_bInitialized )
      return;

   // Compute total retransmissions requests and responses in last 500 milisec

   m_TimeLastRetransmissionsStatsUpdate = uTimeNow;
      
   m_SM_RetransmissionsStats.totalRequestedRetransmissionsLast500Ms = 0;
   m_SM_RetransmissionsStats.totalReceivedRetransmissionsLast500Ms = 0;
   u32 dTime = 0;
   int iIndex = 0;
   while ( (dTime < 500) && (iIndex < MAX_HISTORY_VIDEO_INTERVALS) )
   {
      m_SM_RetransmissionsStats.totalRequestedRetransmissionsLast500Ms += m_SM_RetransmissionsStats.history[iIndex].uCountRequestedRetransmissions;
      m_SM_RetransmissionsStats.totalReceivedRetransmissionsLast500Ms += m_SM_RetransmissionsStats.history[iIndex].uCountAcknowledgedRetransmissions;
      dTime += m_SM_RetransmissionsStats.uGraphRefreshIntervalMs;
      iIndex++;
   }

   if ( m_SM_RetransmissionsStats.history[0].uCountReceivedRetransmissionPackets > 0 )
      m_SM_RetransmissionsStats.history[0].uAverageRetransmissionRoundtripTime /= m_SM_RetransmissionsStats.history[0].uCountReceivedRetransmissionPackets;

   // Remove retransmissions that are too old (older than retransmission window) from the list
   // Start from zero index (oldest) and remove all that are too old

   if ( m_SM_RetransmissionsStats.iCountActiveRetransmissions < 0 )
      m_SM_RetransmissionsStats.iCountActiveRetransmissions = 0;
   if ( m_SM_RetransmissionsStats.iCountActiveRetransmissions > MAX_HISTORY_STACK_RETRANSMISSION_INFO )
      m_SM_RetransmissionsStats.iCountActiveRetransmissions = MAX_HISTORY_STACK_RETRANSMISSION_INFO;
   int iRemoveToIndex = 0;
   int iRemoveIncompleteCount = 0;
   while ( ( iRemoveToIndex < m_SM_RetransmissionsStats.iCountActiveRetransmissions) &&
           ( m_SM_RetransmissionsStats.listActiveRetransmissions[iRemoveToIndex].uRequestTime < uTimeNow - s_iMilisecondsMaxRetransmissionWindow ) )
   {
      if ( m_SM_RetransmissionsStats.listActiveRetransmissions[iRemoveToIndex].uReceivedPackets !=
           m_SM_RetransmissionsStats.listActiveRetransmissions[iRemoveToIndex].uRequestedPackets )
         iRemoveIncompleteCount++;
      iRemoveToIndex++;
   }
   if ( m_SM_RetransmissionsStats.history[0].uCountDroppedRetransmissions < 254 - iRemoveIncompleteCount )
      m_SM_RetransmissionsStats.history[0].uCountDroppedRetransmissions += iRemoveIncompleteCount;
   else
      m_SM_RetransmissionsStats.history[0].uCountDroppedRetransmissions = 254;

   if ( (iRemoveToIndex > 0) && (iRemoveToIndex >= m_SM_RetransmissionsStats.iCountActiveRetransmissions) )
      m_SM_RetransmissionsStats.iCountActiveRetransmissions = 0;
   if ( (iRemoveToIndex > 0) && (iRemoveToIndex < m_SM_RetransmissionsStats.iCountActiveRetransmissions) )
   {
      for( int i=0; i<m_SM_RetransmissionsStats.iCountActiveRetransmissions - iRemoveToIndex; i++ )
      {
         memcpy((u8*)&(m_SM_RetransmissionsStats.listActiveRetransmissions[i]),
                (u8*)&(m_SM_RetransmissionsStats.listActiveRetransmissions[i+iRemoveToIndex]),
                sizeof(controller_single_retransmission_state)); 
      }
      m_SM_RetransmissionsStats.iCountActiveRetransmissions -= iRemoveToIndex;
   }

   for( int i=MAX_HISTORY_VIDEO_INTERVALS-1; i>0; i-- )
   {
      m_SM_RetransmissionsStats.history[i].uCountRequestedRetransmissions = m_SM_RetransmissionsStats.history[i-1].uCountRequestedRetransmissions;
      m_SM_RetransmissionsStats.history[i].uCountAcknowledgedRetransmissions = m_SM_RetransmissionsStats.history[i-1].uCountAcknowledgedRetransmissions;
      m_SM_RetransmissionsStats.history[i].uCountCompletedRetransmissions = m_SM_RetransmissionsStats.history[i-1].uCountCompletedRetransmissions;
      m_SM_RetransmissionsStats.history[i].uCountDroppedRetransmissions = m_SM_RetransmissionsStats.history[i-1].uCountDroppedRetransmissions;

      m_SM_RetransmissionsStats.history[i].uMinRetransmissionRoundtripTime = m_SM_RetransmissionsStats.history[i-1].uMinRetransmissionRoundtripTime;
      m_SM_RetransmissionsStats.history[i].uMaxRetransmissionRoundtripTime = m_SM_RetransmissionsStats.history[i-1].uMaxRetransmissionRoundtripTime;
      m_SM_RetransmissionsStats.history[i].uAverageRetransmissionRoundtripTime = m_SM_RetransmissionsStats.history[i-1].uAverageRetransmissionRoundtripTime;
      
      m_SM_RetransmissionsStats.history[i].uCountRequestedPacketsForRetransmission = m_SM_RetransmissionsStats.history[i-1].uCountRequestedPacketsForRetransmission;
      m_SM_RetransmissionsStats.history[i].uCountReRequestedPacketsForRetransmission = m_SM_RetransmissionsStats.history[i-1].uCountReRequestedPacketsForRetransmission;
      m_SM_RetransmissionsStats.history[i].uCountReceivedRetransmissionPackets = m_SM_RetransmissionsStats.history[i-1].uCountReceivedRetransmissionPackets;
      m_SM_RetransmissionsStats.history[i].uCountReceivedRetransmissionPacketsDuplicate = m_SM_RetransmissionsStats.history[i-1].uCountReceivedRetransmissionPacketsDuplicate;
      m_SM_RetransmissionsStats.history[i].uCountReceivedRetransmissionPacketsDropped = m_SM_RetransmissionsStats.history[i-1].uCountReceivedRetransmissionPacketsDropped;
   }

   m_SM_RetransmissionsStats.history[0].uCountRequestedRetransmissions = 0;
   m_SM_RetransmissionsStats.history[0].uCountAcknowledgedRetransmissions = 0;
   m_SM_RetransmissionsStats.history[0].uCountCompletedRetransmissions = 0;
   m_SM_RetransmissionsStats.history[0].uCountDroppedRetransmissions = 0;

   m_SM_RetransmissionsStats.history[0].uCountRequestedPacketsForRetransmission = 0;
   m_SM_RetransmissionsStats.history[0].uCountReRequestedPacketsForRetransmission = 0;
   m_SM_RetransmissionsStats.history[0].uCountReceivedRetransmissionPackets = 0;
   m_SM_RetransmissionsStats.history[0].uCountReceivedRetransmissionPacketsDuplicate = 0;
   m_SM_RetransmissionsStats.history[0].uCountReceivedRetransmissionPacketsDropped = 0;
}

void ProcessorRxVideo::updateHistoryStatsDiscaredAllStack()
{
   if ( m_iRXBlocksStackTopIndex < 0 )
      return;
   /*
   log("");
   log("-------------------------------------------------------------------");
   _rx_video_log_line("No. %d Discared full stack [0-%d] (%u-%u), last updated: (%02d:%02d.%03d - %02d:%02d.%03d) length: %d ms", s_VDStatsCache.total_DiscardedSegments, s_RXBlocksStackTopIndex, s_pRXBlocksStack[0]->video_block_index, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->video_block_index, s_pRXBlocksStack[0]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[0]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[0]->uTimeLastUpdated%1000, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated%1000, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated-s_pRXBlocksStack[0]->uTimeLastUpdated);
   _rx_video_log_line("  * Last output video block: %u at %02d:%02d.%03d (%d ms ago)", s_LastOutputVideoBlockIndex, s_LastOutputVideoBlockTime/1000/60, (s_LastOutputVideoBlockTime/1000)%60, s_LastOutputVideoBlockTime%1000, g_TimeNow - s_LastOutputVideoBlockTime);
   _rx_video_log_line("  * Last received video block: %u at %02d:%02d.%03d (%d ms ago)", s_LastReceivedVideoPacketInfo.video_block_index, s_LastReceivedVideoPacketInfo.receive_time/1000/60, (s_LastReceivedVideoPacketInfo.receive_time/1000)%60, s_LastReceivedVideoPacketInfo.receive_time%1000, g_TimeNow - s_LastReceivedVideoPacketInfo.receive_time);
   _rx_video_log_line("  * Last updated: from %d ms to %d ms ago", g_TimeNow - s_pRXBlocksStack[0]->uTimeLastUpdated, g_TimeNow - s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated);
   _rx_video_log_line("-------------------------------------------------------------------");
   */

   u32 timeMin = m_pRXBlocksStack[0]->uTimeLastUpdated;
   u32 timeMax = m_pRXBlocksStack[m_iRXBlocksStackTopIndex]->uTimeLastUpdated;

   int indexMin = (g_TimeNow - timeMin)/m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs;
   int indexMax = (g_TimeNow - timeMax)/m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs;
   if ( indexMin < 0 ) indexMin = 0;
   if ( indexMin >= MAX_HISTORY_VIDEO_INTERVALS ) indexMin = MAX_HISTORY_VIDEO_INTERVALS-1;
   if ( indexMax < 0 ) indexMax = 0;
   if ( indexMax >= MAX_HISTORY_VIDEO_INTERVALS ) indexMax = MAX_HISTORY_VIDEO_INTERVALS-1;

   for( int i=indexMin; i<=indexMax; i++ )
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[i]++;

   m_SM_VideoDecodeStatsHistory.totalCurrentlyMissingPackets = 0;
}

void ProcessorRxVideo::updateHistoryStatsDiscaredStackSegment(int countDiscardedBlocks)
{
   if ( countDiscardedBlocks <= 0 )
      return;

   /*
   _rx_video_log_line("");
   _rx_video_log_line("-------------------------------------------------------------------");
   _rx_video_log_line("No. %d Discared stack segment [0-%d] (%u-%u), last updated: (%02d:%02d.%03d - %02d:%02d.%03d) length: %d ms", s_VDStatsCache.total_DiscardedSegments, countDiscardedBlocks-1, s_pRXBlocksStack[0]->video_block_index, s_pRXBlocksStack[countDiscardedBlocks-1]->video_block_index, s_pRXBlocksStack[0]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[0]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[0]->uTimeLastUpdated%1000, s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated%1000, s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated-s_pRXBlocksStack[0]->uTimeLastUpdated);
   _rx_video_log_line("  * Stack top is at %d: %u (max allowed stack is %d blocks), received %d ms ago, retransmission timeout is: %d ms", s_RXBlocksStackTopIndex, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->video_block_index, s_VDStatsCache.maxBlocksAllowedInBuffers, g_TimeNow - s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated, s_RetransmissionRetryTimeout);
   _rx_video_log_line("  * Last output video block: %u at %02d:%02d.%03d (%d ms ago)", s_LastOutputVideoBlockIndex, s_LastOutputVideoBlockTime/1000/60, (s_LastOutputVideoBlockTime/1000)%60, s_LastOutputVideoBlockTime%1000, g_TimeNow - s_LastOutputVideoBlockTime);
   _rx_video_log_line("  * Last received video block: %u at %02d:%02d.%03d (%d ms ago)", s_LastReceivedVideoPacketInfo.video_block_index, s_LastReceivedVideoPacketInfo.receive_time/1000/60, (s_LastReceivedVideoPacketInfo.receive_time/1000)%60, s_LastReceivedVideoPacketInfo.receive_time%1000, g_TimeNow - s_LastReceivedVideoPacketInfo.receive_time);
   _rx_video_log_line("  * Last updated: from %d ms to %d ms ago", g_TimeNow - s_pRXBlocksStack[0]->uTimeLastUpdated, g_TimeNow - s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated);
   */

   // Detect the time interval we are discarding

   u32 timeMin = m_pRXBlocksStack[0]->uTimeLastUpdated;
   u32 timeMax = m_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated;

   int indexMin = (g_TimeNow - timeMin)/m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs;
   int indexMax = (g_TimeNow - timeMax)/m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs;
   if ( indexMin < 0 ) indexMin = 0;
   if ( indexMin >= MAX_HISTORY_VIDEO_INTERVALS ) indexMin = MAX_HISTORY_VIDEO_INTERVALS-1;
   if ( indexMax < 0 ) indexMax = 0;
   if ( indexMax >= MAX_HISTORY_VIDEO_INTERVALS ) indexMax = MAX_HISTORY_VIDEO_INTERVALS-1;

   for( int i=indexMin; i<=indexMax; i++ )
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[i]++;

   for( int i=0; i<countDiscardedBlocks; i++ )
   {
      if ( m_pRXBlocksStack[i]->received_data_packets + m_pRXBlocksStack[i]->received_fec_packets < m_pRXBlocksStack[i]->data_packets )
      {
         //_rx_video_log_line("  * Unrecoverable block %d: %u [received: %d/%d], last updated time: %02d:%02d.%03d (%d ms ago)", i, s_pRXBlocksStack[i]->video_block_index, s_pRXBlocksStack[i]->received_data_packets, s_pRXBlocksStack[i]->received_fec_packets, s_pRXBlocksStack[i]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[i]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[i]->uTimeLastUpdated%1000, g_TimeNow - s_pRXBlocksStack[i]->uTimeLastUpdated);
         for( int k=0; k<m_pRXBlocksStack[i]->data_packets+m_pRXBlocksStack[i]->fec_packets; k++ )
            if ( m_pRXBlocksStack[i]->packetsInfo[k].state != RX_PACKET_STATE_RECEIVED )
            {
               //if ( m_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount > 0 )
               //   _rx_video_log_line("      - missing packet %d: retry count: %d, first retry: %d ms ago, last retry: %d ms ago", k, s_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount, g_TimeNow - s_pRXBlocksStack[i]->packetsInfo[k].uTimeFirstRetrySent, g_TimeNow - s_pRXBlocksStack[i]->packetsInfo[k].uTimeLastRetrySent);
               //else
               //   _rx_video_log_line("      - missing packet %d: retry count: 0", k);
            }

         u32 time = m_pRXBlocksStack[i]->uTimeLastUpdated;
         int index = (g_TimeNow - time)/m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs;
         if ( index < 0 )
            index = 0;
         if ( index >= MAX_HISTORY_VIDEO_INTERVALS )
            index = MAX_HISTORY_VIDEO_INTERVALS-1;
         m_SM_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[index]++;
      }
      else
      {
         if ( m_pRXBlocksStack[i]->received_data_packets >= m_pRXBlocksStack[i]->data_packets )
            m_SM_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[0]++;
         else
            m_SM_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[0]++;
         //_rx_video_log_line("  * Usable block %d: %u [received %d/%d], last updated time: %02d:%02d.%03d (%d ms ago)", i, s_pRXBlocksStack[i]->video_block_index, s_pRXBlocksStack[i]->received_data_packets, s_pRXBlocksStack[i]->received_fec_packets, s_pRXBlocksStack[i]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[i]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[i]->uTimeLastUpdated%1000, g_TimeNow - s_pRXBlocksStack[i]->uTimeLastUpdated);
      }
   }

   if ( countDiscardedBlocks <= m_iRXBlocksStackTopIndex )
   {
      if ( m_pRXBlocksStack[countDiscardedBlocks]->received_data_packets + m_pRXBlocksStack[countDiscardedBlocks]->received_fec_packets < m_pRXBlocksStack[countDiscardedBlocks]->data_packets )
      {
         //_rx_video_log_line("  * Next block in the RX buffer %d: %u [received: %d/%d], last updated time: %02d:%02d.%03d (%d ms ago)", countDiscardedBlocks, s_pRXBlocksStack[countDiscardedBlocks]->video_block_index, s_pRXBlocksStack[countDiscardedBlocks]->received_data_packets, s_pRXBlocksStack[countDiscardedBlocks]->received_fec_packets, s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated%1000, g_TimeNow - s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated);
         for( int k=0; k<m_pRXBlocksStack[countDiscardedBlocks]->data_packets+m_pRXBlocksStack[countDiscardedBlocks]->fec_packets; k++ )
            if ( m_pRXBlocksStack[countDiscardedBlocks]->packetsInfo[k].state != RX_PACKET_STATE_RECEIVED )
            {
               //if ( m_pRXBlocksStack[countDiscardedBlocks]->packetsInfo[k].uRetrySentCount > 0 )
               //   _rx_video_log_line("      - missing packet %d: retry count: %d, first retry: %d ms ago, last retry: %d ms ago", k, s_pRXBlocksStack[countDiscardedBlocks]->packetsInfo[k].uRetrySentCount, g_TimeNow - s_pRXBlocksStack[countDiscardedBlocks]->packetsInfo[k].uTimeFirstRetrySent, g_TimeNow - s_pRXBlocksStack[countDiscardedBlocks]->packetsInfo[k].uTimeLastRetrySent);
               //else
               //   _rx_video_log_line("      - missing packet %d: retry count: 0", k);
            }
      }
      //else
      //   _rx_video_log_line("  * Next block in the RX buffer %d: %u [received %d/%d], last updated time: %02d:%02d.%03d (%d ms ago)", countDiscardedBlocks, s_pRXBlocksStack[countDiscardedBlocks]->video_block_index, s_pRXBlocksStack[countDiscardedBlocks]->received_data_packets, s_pRXBlocksStack[countDiscardedBlocks]->received_fec_packets, s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated%1000, g_TimeNow - s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated);
   }
   //else
   //   _rx_video_log_line("  * Nothing else left in the RX buffers after this.");

   //_rx_video_log_line("-------------------------------------------------------------------");
}

void ProcessorRxVideo::updateHistoryStatsBlockOutputed(int rx_buffer_block_index, bool hasRetransmittedPackets)
{
   u32 video_block_index = m_pRXBlocksStack[rx_buffer_block_index]->video_block_index;
   if ( MAX_U32 == video_block_index )
      return;

   if ( hasRetransmittedPackets )
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksRetrasmitedPerPeriod[0]++;


   if ( m_pRXBlocksStack[rx_buffer_block_index]->received_data_packets >= m_pRXBlocksStack[rx_buffer_block_index]->data_packets )
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[0]++;
   else if ( m_pRXBlocksStack[rx_buffer_block_index]->received_data_packets + m_pRXBlocksStack[rx_buffer_block_index]->received_fec_packets >= m_pRXBlocksStack[rx_buffer_block_index]->data_packets )
   {
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[0]++;
      int ecUsed = m_pRXBlocksStack[rx_buffer_block_index]->data_packets - m_pRXBlocksStack[rx_buffer_block_index]->received_data_packets;
      if ( ecUsed > m_SM_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[0] )
         m_SM_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[0] = ecUsed;
   }
   else if ( m_pRXBlocksStack[rx_buffer_block_index]->received_data_packets + m_pRXBlocksStack[rx_buffer_block_index]->received_fec_packets > 0 )
   {
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksBadPerPeriod[0]++;
      //log_line("Bad block out");
   }
   if ( video_block_index > m_uLastOutputVideoBlockIndex+1 )
   {
      u32 diff = video_block_index - m_uLastOutputVideoBlockIndex;
      if ( diff + m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[0] > 255 )
         m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[0] = 255;
      else
         m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[0] += diff;
   }
}

void ProcessorRxVideo::reconstructBlock(int rx_buffer_block_index)
{

   if ( g_PD_ControllerLinkStats.tmp_video_streams_blocks_reconstructed[0] < 254 )
      g_PD_ControllerLinkStats.tmp_video_streams_blocks_reconstructed[0]++;
   else
      g_PD_ControllerLinkStats.tmp_video_streams_blocks_reconstructed[0] = 254;

   // Add existing data packets, mark and count the ones that are missing

   s_FECInfo.missing_packets_count = 0;
   for( int i=0; i<m_pRXBlocksStack[rx_buffer_block_index]->data_packets; i++ )
   {
      s_FECInfo.fec_decode_data_packets_pointers[i] = m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[i].pData;
      if ( m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[i].state != RX_PACKET_STATE_RECEIVED )
      {
         s_FECInfo.fec_decode_missing_packets_indexes[s_FECInfo.missing_packets_count] = i;
         s_FECInfo.missing_packets_count++;
         //s_VDStatsCache.total_BadOrLostPackets++;
      }
   }

   if ( s_FECInfo.missing_packets_count > g_PD_ControllerLinkStats.tmp_video_streams_blocks_max_ec_packets_used[0] )
   {
      // missing packets in a block can't be larger than 8 bits (config values for max EC/Data pachets)
      g_PD_ControllerLinkStats.tmp_video_streams_blocks_max_ec_packets_used[0] = s_FECInfo.missing_packets_count;
   }

   // Add the needed FEC packets to the list
   unsigned int pos = 0;
   for( int i=0; i<m_pRXBlocksStack[rx_buffer_block_index]->fec_packets; i++ )
   {
      if ( m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[i+m_pRXBlocksStack[rx_buffer_block_index]->data_packets].state == RX_PACKET_STATE_RECEIVED)
      {
         s_FECInfo.fec_decode_fec_packets_pointers[pos] = m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[i+m_pRXBlocksStack[rx_buffer_block_index]->data_packets].pData;
         s_FECInfo.fec_decode_fec_indexes[pos] = i;
         pos++;
         if ( pos == s_FECInfo.missing_packets_count )
            break;
      }
   }

   fec_decode(m_pRXBlocksStack[rx_buffer_block_index]->packet_length, s_FECInfo.fec_decode_data_packets_pointers, m_pRXBlocksStack[rx_buffer_block_index]->data_packets, s_FECInfo.fec_decode_fec_packets_pointers, s_FECInfo.fec_decode_fec_indexes, s_FECInfo.fec_decode_missing_packets_indexes, s_FECInfo.missing_packets_count );
         
   // Mark all data packets reconstructed as received, set the right data in them
   for( u32 i=0; i<s_FECInfo.missing_packets_count; i++ )
   {
      m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[s_FECInfo.fec_decode_missing_packets_indexes[i]].state = RX_PACKET_STATE_RECEIVED;
      m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[s_FECInfo.fec_decode_missing_packets_indexes[i]].packet_length = m_pRXBlocksStack[rx_buffer_block_index]->packet_length;
      m_pRXBlocksStack[rx_buffer_block_index]->received_data_packets++;

      if ( m_SM_VideoDecodeStats.currentPacketsInBuffers > m_SM_VideoDecodeStats.maxPacketsInBuffers )
         m_SM_VideoDecodeStats.maxPacketsInBuffers = m_SM_VideoDecodeStats.currentPacketsInBuffers;
   }
  //_rx_video_log_line("Reconstructed block %u, had %d missing packets", s_pRXBlocksStack[rx_buffer_block_index]->video_block_index, s_FECInfo.missing_packets_count);

}

void ProcessorRxVideo::discardRetransmissionsRequestsTooOld()
{
   if ( m_iRXBlocksStackTopIndex < 0 )
   {
      m_SM_RetransmissionsStats.iCountActiveRetransmissions = 0;
      return;
   }

   if ( m_pRXBlocksStack[0]->video_block_index == MAX_U32 )
   {
      m_SM_RetransmissionsStats.iCountActiveRetransmissions = 0;
      return;
   }

   for( int i=0; i<m_SM_RetransmissionsStats.iCountActiveRetransmissions; i++ )
   {
      bool bRemoveThis = false;

      if ( m_SM_RetransmissionsStats.listActiveRetransmissions[i].uRequestedPackets == 0 )
         bRemoveThis = true;
      if ( m_SM_RetransmissionsStats.listActiveRetransmissions[i].uRequestTime < g_TimeNow - s_iMilisecondsMaxRetransmissionWindow )
         bRemoveThis = true;

      bool bAllPacketsInvalid = true;
      bool bAllPacketsReceived = true;
      if ( ! bRemoveThis )
      for( int k=0; k<m_SM_RetransmissionsStats.listActiveRetransmissions[i].uRequestedPackets; k++ )
      {
         if ( m_SM_RetransmissionsStats.listActiveRetransmissions[i].uRequestedVideoBlockIndex[k] >= m_pRXBlocksStack[0]->video_block_index )
            bAllPacketsInvalid = false;
         if ( m_SM_RetransmissionsStats.listActiveRetransmissions[i].uReceivedPacketCount[k] == 0 )
         {
            bAllPacketsReceived = false;
            break;
         }
      }

      if ( bAllPacketsInvalid || bAllPacketsReceived )
         bRemoveThis = true;
   
      if ( ! bRemoveThis )
        continue;
 
      for( int k=i; k<m_SM_RetransmissionsStats.iCountActiveRetransmissions-1; k++ )
      {
         memcpy((u8*)&(m_SM_RetransmissionsStats.listActiveRetransmissions[k]),
                (u8*)&(m_SM_RetransmissionsStats.listActiveRetransmissions[k+1]),
                sizeof(controller_single_retransmission_state)); 
      }
      m_SM_RetransmissionsStats.iCountActiveRetransmissions--;
      m_SM_RetransmissionsStats.listActiveRetransmissions[m_SM_RetransmissionsStats.iCountActiveRetransmissions].uRetransmissionId = 0;
      m_SM_RetransmissionsStats.listActiveRetransmissions[m_SM_RetransmissionsStats.iCountActiveRetransmissions].uRequestTime = 0;
      m_SM_RetransmissionsStats.listActiveRetransmissions[m_SM_RetransmissionsStats.iCountActiveRetransmissions].uRequestedPackets = 0;
      m_SM_RetransmissionsStats.listActiveRetransmissions[m_SM_RetransmissionsStats.iCountActiveRetransmissions].uReceivedPackets = 0;
   }
}

void ProcessorRxVideo::checkAndRequestMissingPackets()
{
   Model* pModel = findModelWithId(m_uVehicleId, 123);

   if ( g_bSearching || (NULL == pModel) || g_bUpdateInProgress )
      return;
   if ( pModel->is_spectator )
      return;

   if ( g_TimeNow < m_uLastTimeRequestedRetransmission + m_uTimeIntervalMsForRequestingRetransmissions )
      return;

   if ( m_uTimeIntervalMsForRequestingRetransmissions < 20 )
      m_uTimeIntervalMsForRequestingRetransmissions++;
   if ( m_uLastTimeRequestedRetransmission < g_TimeNow-400 )
      m_uTimeIntervalMsForRequestingRetransmissions = 10;

   if ( ! (pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS) )
      return;
   if ( -1 == m_iRXBlocksStackTopIndex )
      return;

   // If link is lost, do not request retransmissions

   if ( m_LastReceivedVideoPacketInfo.receive_time != 0 )
   {
      int iDelta = 1000;
      if ( 0 != g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds )
         iDelta = g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds;
      if ( g_TimeNow > m_LastReceivedVideoPacketInfo.receive_time + iDelta )
         return;
   }

   // Remove pending active retransmissions that are for video blocks already outputed or discarded
   discardRetransmissionsRequestsTooOld();

   bool bUseNewVersion = false;
   if ( ((pModel->sw_version>>8) & 0xFF) > 6 )
      bUseNewVersion = true;
   if ( ((pModel->sw_version>>8) & 0xFF) == 6 )
   if ( ((pModel->sw_version & 0xFF) == 9) || ((pModel->sw_version & 0xFF) >= 90 ) )
      bUseNewVersion = true;
   
   u8 buffer[1200];
   u8* pBuffer = NULL;
   if ( bUseNewVersion )
      pBuffer = &buffer[6];
   else
      pBuffer = &buffer[2];

   // First bytes in the buffer:
   //   u32: retransmission request unique id
   //   u8: video link id
   //   u8: number of packets requested
   //   (u32+u8+u8)*n = each video block index and video packet index requested + repeat count
   //   optional: serialized minimized t_packet_data_controller_link_stats - link stats (video and radio)



   // Request missing packets from the first block in stack up to the last one (including the last one)
   // Max requested packets count is limited to 200, due to max radio packet size

   int iBlockStartIndex = 0;
   int iBlockEndIndex = m_iRXBlocksStackTopIndex;

   // If not aggressive video retransmissions, request missing packets only up to the last 2 received blocks;
   if ( ! (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_RETRANSMISSIONS_FAST) )
      iBlockEndIndex = m_iRXBlocksStackTopIndex-3;

   int totalCountRequested = 0;
   int totalCountRequestedNew = 0;
   int totalCountReRequested = 0;

   for( int i=iBlockStartIndex; i<=iBlockEndIndex; i++ )
   {
      // Request maximum 30 packets at each retransmission request, in order to allow the vehicle some time to send them back
      if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
         break;

      // Skip empty blocks or blocks which can be reconstructed
      if ( m_pRXBlocksStack[i]->data_packets == 0 )
         continue;
      if ( m_pRXBlocksStack[i]->received_data_packets + m_pRXBlocksStack[i]->received_fec_packets >= m_pRXBlocksStack[i]->data_packets )
         continue;
      
      int countToRequestForBlock = 0;

      // For last block, request only missing packets up untill the last received data packet in the block
      if ( i == m_iRXBlocksStackTopIndex )
      {
         bool bCheckAndRequestFromTopBlock = false;

         if ( m_LastReceivedVideoPacketInfo.receive_time < g_TimeNow - 20 )
         if ( (m_pRXBlocksStack[i]->received_data_packets > 0) || (m_pRXBlocksStack[i]->received_fec_packets > 0) )
            bCheckAndRequestFromTopBlock = true;

         if ( 0 != g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs )
         if ( 0 != m_pRXBlocksStack[i]->uTimeLastUpdated )
         if ( m_pRXBlocksStack[i]->uTimeLastUpdated < g_TimeNow - g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs )
            bCheckAndRequestFromTopBlock = true;

         if ( bCheckAndRequestFromTopBlock )
         {
            // Request missing packets from the start of the block

            int iMaxBlockPacketIndexReceived = -1;
            for( int k=m_pRXBlocksStack[i]->data_packets + m_pRXBlocksStack[i]->fec_packets-1; k>=0; k-- )
            {
               if ( m_pRXBlocksStack[i]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
               {
                  iMaxBlockPacketIndexReceived = k;
                  break;
               }
            }

            if ( iMaxBlockPacketIndexReceived >= m_pRXBlocksStack[i]->fec_packets )
            if ( m_pRXBlocksStack[i]->received_data_packets + m_pRXBlocksStack[i]->received_fec_packets < iMaxBlockPacketIndexReceived - m_pRXBlocksStack[i]->fec_packets )
            {
               countToRequestForBlock = m_pRXBlocksStack[i]->data_packets - m_pRXBlocksStack[i]->received_data_packets - m_pRXBlocksStack[i]->received_fec_packets;
               if ( countToRequestForBlock > iMaxBlockPacketIndexReceived )
                 countToRequestForBlock = iMaxBlockPacketIndexReceived;
            }
         }
         //if ( countToRequestForBlock > 0 )
         //   log_line("DEBUG request %d packets for top block, index %d, recv data: %d of %d, recv fec: %d of %d",
         //       countToRequestForBlock, i, m_pRXBlocksStack[i]->received_data_packets, m_pRXBlocksStack[i]->data_packets, m_pRXBlocksStack[i]->received_fec_packets, m_pRXBlocksStack[i]->fec_packets);
      }
      else
         countToRequestForBlock = m_pRXBlocksStack[i]->data_packets - m_pRXBlocksStack[i]->received_data_packets - m_pRXBlocksStack[i]->received_fec_packets;

      if ( countToRequestForBlock <= 0 )
         continue;

      countToRequestForBlock -= m_pRXBlocksStack[i]->totalPacketsRequested;

      // First, re-request the packets we already requested once.
      // Then, request additional packets if needed (not enough requested for possible reconstruction)
      // Then, request some EC packets (half the original EC rate) proportional to missing packets count
      
      if ( m_pRXBlocksStack[i]->totalPacketsRequested > 0 )
      {
         for( int k=0; k<m_pRXBlocksStack[i]->data_packets; k++ )
         {
            if ( m_pRXBlocksStack[i]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
               continue;
            if ( m_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount == 0 )
               continue;
            if ( m_pRXBlocksStack[i]->packetsInfo[k].uTimeLastRetrySent >= g_TimeNow-m_uRetryRetransmissionAfterTimeoutMiliseconds )
               continue;

            m_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount++;
            m_pRXBlocksStack[i]->uTimeLastRetrySent = g_TimeNow;
            m_pRXBlocksStack[i]->packetsInfo[k].uTimeLastRetrySent = g_TimeNow;

            // Decrease interval of future retransmissions requests for this packet
            u32 dt = 5 * m_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount;
            if ( dt > m_uRetryRetransmissionAfterTimeoutMiliseconds-10 )
               dt = m_uRetryRetransmissionAfterTimeoutMiliseconds-10;
            m_pRXBlocksStack[i]->packetsInfo[k].uTimeLastRetrySent -= dt;

            memcpy(pBuffer, &(m_pRXBlocksStack[i]->video_block_index), sizeof(u32));
            pBuffer += sizeof(u32);
            *pBuffer = (u8)k;
            pBuffer++;
            *pBuffer = (u8)(m_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount);
            pBuffer++;
            totalCountRequested++;
            totalCountReRequested++;

            if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
               break;
         }
      }

      if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
         break;

      // Request additional packets from the block if not enough for possible reconstruction

      if ( m_pRXBlocksStack[i]->data_packets - m_pRXBlocksStack[i]->received_data_packets - m_pRXBlocksStack[i]->received_fec_packets - m_pRXBlocksStack[i]->totalPacketsRequested > 0 )
      {
         for( int k=0; k<m_pRXBlocksStack[i]->data_packets; k++ )
         {
            if ( m_pRXBlocksStack[i]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
               continue;
            if ( m_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount != 0 )
               continue;

            m_pRXBlocksStack[i]->packetsInfo[k].uTimeFirstRetrySent = g_TimeNow;
            m_pRXBlocksStack[i]->packetsInfo[k].uTimeLastRetrySent = g_TimeNow;
            m_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount = 1;
            totalCountRequestedNew++;
            m_pRXBlocksStack[i]->totalPacketsRequested++;

            if ( 0 == m_pRXBlocksStack[i]->uTimeFirstRetrySent )
               m_pRXBlocksStack[i]->uTimeFirstRetrySent = g_TimeNow;
            m_pRXBlocksStack[i]->uTimeLastRetrySent = g_TimeNow;

            memcpy(pBuffer, &(m_pRXBlocksStack[i]->video_block_index), sizeof(u32));
            pBuffer += sizeof(u32);
            *pBuffer = (u8)k;
            pBuffer++;
            *pBuffer = (u8)(m_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount);
            pBuffer++;

            totalCountRequested++;
            countToRequestForBlock--;
            if ( countToRequestForBlock <= 0 )
               break;

            if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
               break;
         }
      }

      if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
         break;
   }

   // No new video packets for a long time? Request next one

   if ( 0 != g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs )
   if ( m_LastReceivedVideoPacketInfo.video_block_index != MAX_U32 )
   if ( m_LastReceivedVideoPacketInfo.receive_time != 0 )
   if ( m_LastReceivedVideoPacketInfo.receive_time + g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs <= g_TimeNow )
   {
      u32 videoBlock = m_LastReceivedVideoPacketInfo.video_block_index;
      u32 videoPacket = m_LastReceivedVideoPacketInfo.video_block_packet_index;
      videoPacket++;
      if ( videoPacket >= (u32)m_SM_VideoDecodeStats.data_packets_per_block )
      {
         videoPacket = 0;
         videoBlock++;
      }
      memcpy(pBuffer, &videoBlock, sizeof(u32));
      pBuffer += sizeof(u32);
      *pBuffer = (u8)videoPacket;
      pBuffer++;
      *pBuffer = 1;
      pBuffer++;

      totalCountRequested++;
      totalCountRequestedNew++;
   }

   if ( 0 == totalCountRequested )      
      return;

   m_uLastTimeRequestedRetransmission = g_TimeNow;

   if ( bUseNewVersion )
   {
      m_uRequestRetransmissionUniqueId++;
      memcpy((u8*)&(buffer[0]), (u8*)&m_uRequestRetransmissionUniqueId, sizeof(u32));
      buffer[4] = 0; // video stream id
      buffer[5] = (u8) totalCountRequested;
   }
   else
   {
      buffer[0] = 0; // video stream id
      buffer[1] = (u8) totalCountRequested;
   }
   
   // Log info
   /*
   char szBuff[1024];
   sprintf(szBuff, "DEBUG requested %d packets for retransmission (last output video block: %u, first video block in stack: %u, stack top: %d): ",
     totalCountRequested, m_uLastOutputVideoBlockIndex, m_pRXBlocksStack[0]->video_block_index, m_iRXBlocksStackTopIndex );
   u8* pTmp = &buffer[6];
   for( int i=0; i<totalCountRequested; i++ )
   {
      u32 uVideoBlock = 0;
      memcpy((u8*)&uVideoBlock, pTmp, sizeof(u32));
      char szTmp[32];
      sprintf(szTmp, "%u/%dx%d, ", uVideoBlock, (int) *(pTmp+4), (int) *(pTmp+5));
      strcat(szBuff, szTmp);
      pTmp += 6 * sizeof(u8);
   }
   log_line(szBuff);
   */
   int iIndex = getVehicleRuntimeIndex(m_uVehicleId);
   if ( -1 != iIndex )
   {
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uIntervalsRequestedRetransmissions[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uCurrentIntervalIndex] += totalCountRequestedNew;
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uIntervalsRetriedRetransmissions[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uCurrentIntervalIndex] += totalCountReRequested;
   }

   m_SM_RetransmissionsStats.history[0].uCountRequestedRetransmissions++;
   m_SM_RetransmissionsStats.history[0].uCountRequestedPacketsForRetransmission += totalCountRequested;
   m_SM_RetransmissionsStats.history[0].uCountReRequestedPacketsForRetransmission += totalCountReRequested;

   m_SM_RetransmissionsStats.totalRequestedRetransmissions++;
   m_SM_RetransmissionsStats.totalRequestedVideoPackets += totalCountRequestedNew;

   // Store info about this retransmission request

   if ( m_SM_RetransmissionsStats.iCountActiveRetransmissions < MAX_HISTORY_STACK_RETRANSMISSION_INFO )
   {
      int k = m_SM_RetransmissionsStats.iCountActiveRetransmissions;
      m_SM_RetransmissionsStats.listActiveRetransmissions[k].uRetransmissionId = m_uRequestRetransmissionUniqueId;
      m_SM_RetransmissionsStats.listActiveRetransmissions[k].uRequestTime = g_TimeNow; 
      m_SM_RetransmissionsStats.listActiveRetransmissions[k].uRequestedPackets = totalCountRequested; 
      m_SM_RetransmissionsStats.listActiveRetransmissions[k].uReceivedPackets = 0;

      u8* pTmp = &(buffer[2]);
      if ( bUseNewVersion )
         pTmp = &(buffer[6]);

      for( int i=0; i<totalCountRequested; i++ )
      {
         memcpy((u8*)&(m_SM_RetransmissionsStats.listActiveRetransmissions[k].uRequestedVideoBlockIndex[i]), pTmp, sizeof(u32));
         pTmp += 4;
         m_SM_RetransmissionsStats.listActiveRetransmissions[k].uRequestedVideoBlockPacketIndex[i] = *pTmp;
         pTmp++;
         m_SM_RetransmissionsStats.listActiveRetransmissions[k].uRequestedVideoPacketRetryCount[i] = *pTmp;
         pTmp++;
         m_SM_RetransmissionsStats.listActiveRetransmissions[k].uReceivedPacketCount[i] = 0;
         m_SM_RetransmissionsStats.listActiveRetransmissions[k].uReceivedPacketTime[i] = 0;
      }

      m_SM_RetransmissionsStats.listActiveRetransmissions[k].uMinResponseTime = 0;
      m_SM_RetransmissionsStats.listActiveRetransmissions[k].uMaxResponseTime = 0;

      m_SM_RetransmissionsStats.iCountActiveRetransmissions++;
   }

   if ( g_PD_ControllerLinkStats.tmp_video_streams_requested_retransmission_packets[0] + totalCountRequested < 254 )
      g_PD_ControllerLinkStats.tmp_video_streams_requested_retransmission_packets[0] += totalCountRequested;
   else
      g_PD_ControllerLinkStats.tmp_video_streams_requested_retransmission_packets[0] = 254;

   int bufferLength = 0;

   if ( bUseNewVersion )
      bufferLength = sizeof(u32) + 2 + totalCountRequested*(sizeof(u32)+sizeof(u8)+sizeof(u8));
   else
      bufferLength = 2 + totalCountRequested*(sizeof(u32)+sizeof(u8)+sizeof(u8));
   
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS, STREAM_ID_DATA);
   if ( bUseNewVersion )
      PH.packet_type = PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2;
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = m_uVehicleId;
   if ( m_uVehicleId == 0 || m_uVehicleId == MAX_U32 )
   {
      PH.vehicle_id_dest = pModel->vehicle_id;
      log_softerror_and_alarm("[VideoRx] Tried to request retransmissions before having received a video packet.");
   }
   PH.total_length = sizeof(t_packet_header) + bufferLength;

   #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO  
   if ( NULL != pModel )
   if ( pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS )
   if ( pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO )
   if ( g_TimeNow > g_TimeLastControllerLinkStatsSent + CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL_MS/2 )
      PH.total_length += get_controller_radio_link_stats_size();
   #endif

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet + sizeof(t_packet_header), buffer, bufferLength);

   #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
   if ( NULL != pModel && (pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS) )
   if ( g_TimeNow > g_TimeLastControllerLinkStatsSent + CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL_MS/2 )
   {
      add_controller_radio_link_stats_to_buffer(packet + sizeof(t_packet_header)+bufferLength);
      g_TimeLastControllerLinkStatsSent = g_TimeNow;
   }
   #endif
   
   packets_queue_add_packet(&s_QueueRadioPackets, packet);

   // If there are retried retransmissions, send the request twice
   if ( totalCountReRequested > 5 )
      packets_queue_add_packet(&s_QueueRadioPackets, packet);
}

void ProcessorRxVideo::addPacketToReceivedBlocksBuffers(u8* pBuffer, int length, int rx_buffer_block_index, bool bWasRetransmitted)
{
   t_packet_header* pPH = (t_packet_header*)pBuffer;

   Model* pModel = findModelWithId(pPH->vehicle_id_src, 124);
   if ( NULL == pModel )
      return;

   if ( rx_buffer_block_index > m_iRXBlocksStackTopIndex )
      m_iRXBlocksStackTopIndex = rx_buffer_block_index;

   u32 video_block_index = 0;
   u8 video_block_packet_index = 0;
   u16 video_data_length = 0;
   int iLastAckKeyframeInterval = 0;
   int iLastAckVideoLevelShift = 0;
   u32 uLastSetVideoBitrate = 0;
   u32 uEncodingExtraFlags2 = 0;
   
   t_packet_header_video_full_77* pPHVFNew = (t_packet_header_video_full_77*) (pBuffer+sizeof(t_packet_header));    
   video_block_index = pPHVFNew->video_block_index;
   video_block_packet_index = pPHVFNew->video_block_packet_index;
   video_data_length = pPHVFNew->video_data_length;

   iLastAckKeyframeInterval = pPHVFNew->uLastAckKeyframeInterval;
   iLastAckVideoLevelShift = pPHVFNew->uLastAckLevelShift;
   uLastSetVideoBitrate = pPHVFNew->uLastSetVideoBitrate;
   uEncodingExtraFlags2 = pPHVFNew->encoding_extra_flags2;


   if ( m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[video_block_packet_index].state == RX_PACKET_STATE_RECEIVED )
      return;

   // Begin - Check for last acknowledged values

   if ( ! bWasRetransmitted )
   if ( (m_uLastBlockReceivedEncodingExtraFlags2 == MAX_U32) ||
        (video_block_index > m_uLastBlockReceivedEncodingExtraFlags2) )
   {
      m_uLastBlockReceivedEncodingExtraFlags2 = video_block_index;
      m_SM_VideoDecodeStats.uEncodingExtraFlags2 = uEncodingExtraFlags2;
   }

   if ( ! bWasRetransmitted )
   if ( (m_uLastBlockReceivedSetVideoBitrate == MAX_U32) ||
        (video_block_index > m_uLastBlockReceivedSetVideoBitrate) )
   {
      m_uLastBlockReceivedSetVideoBitrate = video_block_index;
      m_SM_VideoDecodeStats.uLastSetVideoBitrate = uLastSetVideoBitrate;
      int iIndex = getVehicleRuntimeIndex(m_uVehicleId);
      if ( -1 != iIndex )
      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uLastSetVideoBitrate != uLastSetVideoBitrate )
      {
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uLastSetVideoBitrate = uLastSetVideoBitrate;
         log_line("Received video info (video block index: %u) from VID %u (%u) that video bitrate was set to %u bps", video_block_index, pPH->vehicle_id_src, m_uVehicleId, (uLastSetVideoBitrate & 0x7FFFFFFF));
      }
   }

   if ( ! bWasRetransmitted )
   if ( (m_uLastBlockReceivedAckKeyframeInterval == MAX_U32) ||
        (video_block_index > m_uLastBlockReceivedAckKeyframeInterval) )
   {
      m_uLastBlockReceivedAckKeyframeInterval = video_block_index;
      m_SM_VideoDecodeStats.iLastAckKeyframeInterval = iLastAckKeyframeInterval;

      int iIndex = getVehicleRuntimeIndex(m_uVehicleId);
      if ( -1 != iIndex )
      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedKeyFrameMs != iLastAckKeyframeInterval )
      {
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedKeyFrameMs = iLastAckKeyframeInterval;
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedKeyFrameMsRetryCount = 0;
         log_line("Received video ACK (video block index: %u) from VID %u (%u) for setting keyframe to %d ms", video_block_index, pPH->vehicle_id_src, m_uVehicleId, iLastAckKeyframeInterval);
      }
   }

   if ( ! bWasRetransmitted )
   if ( (m_uLastBlockReceivedAdaptiveVideoInterval == MAX_U32) ||
        (video_block_index > m_uLastBlockReceivedAdaptiveVideoInterval) )
   {
      m_uLastBlockReceivedAdaptiveVideoInterval = video_block_index;
      m_SM_VideoDecodeStats.iLastAckVideoLevelShift = iLastAckVideoLevelShift;

      int iIndex = getVehicleRuntimeIndex(m_uVehicleId);
      if ( -1 != iIndex )
      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedLevelShift != iLastAckVideoLevelShift )
      {
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedLevelShift = iLastAckVideoLevelShift;
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastAckLevelShift = g_TimeNow;
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedLevelShiftRetryCount = 0;
         log_line("Received video ACK (video block index: %u) from VID %u (%u) for setting video level shift to %d", video_block_index, pPH->vehicle_id_src, m_uVehicleId, iLastAckVideoLevelShift);
      }
   }

   // End - Check for last acknowledged values


   if ( m_pRXBlocksStack[rx_buffer_block_index]->uTimeFirstPacketReceived == MAX_U32 )
      m_pRXBlocksStack[rx_buffer_block_index]->uTimeFirstPacketReceived = g_TimeNow;


   t_packet_header_video_full_77* pPHVNew = (t_packet_header_video_full_77*) (pBuffer+sizeof(t_packet_header));
   m_pRXBlocksStack[rx_buffer_block_index]->video_block_index = pPHVNew->video_block_index;
   m_pRXBlocksStack[rx_buffer_block_index]->packet_length = pPHVNew->video_data_length;
   m_pRXBlocksStack[rx_buffer_block_index]->data_packets = pPHVNew->block_packets;
   m_pRXBlocksStack[rx_buffer_block_index]->fec_packets = pPHVNew->block_fecs;
   m_pRXBlocksStack[rx_buffer_block_index]->uTimeLastUpdated = g_TimeNow;
   m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[pPHVNew->video_block_packet_index].state = RX_PACKET_STATE_RECEIVED;
   m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[pPHVNew->video_block_packet_index].packet_length = pPHVNew->video_data_length;

   if ( video_data_length < 100 || video_data_length > MAX_PACKET_TOTAL_SIZE )
      log_softerror_and_alarm("Invalid video data size to copy (%d bytes)", video_data_length);
   else
   {
      memcpy(m_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[video_block_packet_index].pData, pBuffer+sizeof(t_packet_header)+sizeof(t_packet_header_video_full_77), video_data_length);
   }

   if ( video_block_packet_index < m_pRXBlocksStack[rx_buffer_block_index]->data_packets )
      m_pRXBlocksStack[rx_buffer_block_index]->received_data_packets++;
   else
      m_pRXBlocksStack[rx_buffer_block_index]->received_fec_packets++;


   m_SM_VideoDecodeStats.currentPacketsInBuffers++;
   if ( m_SM_VideoDecodeStats.currentPacketsInBuffers > m_SM_VideoDecodeStats.maxPacketsInBuffers )
      m_SM_VideoDecodeStats.maxPacketsInBuffers = m_SM_VideoDecodeStats.currentPacketsInBuffers;
}

// Returns index in the receive stack for the block where we added the packet, or -1 if it was discarded

int ProcessorRxVideo::processRetransmittedVideoPacket(u8* pBuffer, int length)
{
   t_packet_header* pPH = (t_packet_header*)pBuffer;
   Model* pModel = findModelWithId(pPH->vehicle_id_src, 125);
   if ( NULL == pModel )
      return -1;

   if ( -1 == m_iRXBlocksStackTopIndex )
      return -1;

   u32 video_block_index = 0;
   u8 video_block_packet_index = 0;
   
   t_packet_header_video_full_77* pPHVFNew = (t_packet_header_video_full_77*) (pBuffer+sizeof(t_packet_header));    
   video_block_index = pPHVFNew->video_block_index;
   video_block_packet_index = pPHVFNew->video_block_packet_index;

   if ( video_block_index < m_pRXBlocksStack[0]->video_block_index )
      return -1;
   
   if ( video_block_index > m_pRXBlocksStack[m_iRXBlocksStackTopIndex]->video_block_index )
      return -1;

   int dest_stack_index = (video_block_index-m_pRXBlocksStack[0]->video_block_index);
   if ( (dest_stack_index < 0) || (dest_stack_index >= m_iRXMaxBlocksToBuffer) )
      return -1;

   if ( m_pRXBlocksStack[dest_stack_index]->packetsInfo[video_block_packet_index].state == RX_PACKET_STATE_RECEIVED )
      return -1;

   addPacketToReceivedBlocksBuffers(pBuffer, length, dest_stack_index, true);

   return dest_stack_index;
}

// Returns index in the receive stack for the block where we added the packet, or -1 if it was discarded

int ProcessorRxVideo::processReceivedVideoPacket(u8* pBuffer, int length)
{
   t_packet_header* pPH = (t_packet_header*)pBuffer;
   Model* pModel = findModelWithId(pPH->vehicle_id_src, 126);
   if ( NULL == pModel )
      return -1;
   u32 video_block_index = 0;
   u8 video_block_packet_index = 0;
   
   u8  block_packets = 0;
   u8  block_fecs = 0;

   t_packet_header_video_full_77* pPHVFNew = (t_packet_header_video_full_77*) (pBuffer+sizeof(t_packet_header));    
   video_block_index = pPHVFNew->video_block_index;
   video_block_packet_index = pPHVFNew->video_block_packet_index;
   block_packets = pPHVFNew->block_packets;
   block_fecs = pPHVFNew->block_fecs;

   //log_line("DEBUG [%u/%u], top: %d, [0] = [%u, %d/%d]",
   //   video_block_index, video_block_packet_index, m_iRXBlocksStackTopIndex, 
   //   m_pRXBlocksStack[0]->video_block_index, m_pRXBlocksStack[0]->received_data_packets,
   //   m_pRXBlocksStack[0]->received_fec_packets);

   // Find a position in blocks buffer where to put it

   // Empty buffers?

   if ( m_iRXBlocksStackTopIndex == -1 )
   {
      // Never used?
      if ( m_uLastOutputVideoBlockIndex == MAX_U32 )
      {
         // If we are past recoverable point in a block just discard it and wait for a new block
         if ( video_block_packet_index > block_fecs )
         {
            return -1;
         }
         log("[VideoRx] Started new buffers at[%u/%d]", video_block_index, video_block_packet_index);
         m_iRXBlocksStackTopIndex = 0;
         addPacketToReceivedBlocksBuffers(pBuffer, length, 0, false);
         return 0;
      }
   }

   // Discard received packets from blocks prior to the first block in stack or to the last output block

   if ( m_uLastOutputVideoBlockIndex != MAX_U32 )
   if ( video_block_index <= m_uLastOutputVideoBlockIndex )
      return -1;

   if ( m_iRXBlocksStackTopIndex >= 0 )
   if ( video_block_index < m_pRXBlocksStack[0]->video_block_index )
      return -1;


   // Find position for this block in the receive stack

   u32 stackIndex = 0;
   if ( m_iRXBlocksStackTopIndex >= 0 && video_block_index >= m_pRXBlocksStack[0]->video_block_index )
      stackIndex = video_block_index - m_pRXBlocksStack[0]->video_block_index;
   else if ( m_uLastOutputVideoBlockIndex != MAX_U32 )
      stackIndex = video_block_index - m_uLastOutputVideoBlockIndex-1;
   
   if ( stackIndex >= (u32)m_iRXMaxBlocksToBuffer )
   {
      int overflow = stackIndex - m_iRXMaxBlocksToBuffer+1;
      if ( (overflow > m_iRXMaxBlocksToBuffer*2/3) || ((stackIndex - (u32)m_iRXBlocksStackTopIndex) > (u32)m_iRXMaxBlocksToBuffer*2/3) )
      {
         updateHistoryStatsDiscaredAllStack();
         resetReceiveBuffers();
         resetOutputState();
      }
      else
      {
         // Discard few more blocks if they also have missing packets and can't be reconstructed right now; to avoid multiple consecutive discards events
         int iLookAhead = 2 + m_iRXMaxBlocksToBuffer/10;
         while ( (overflow < m_iRXBlocksStackTopIndex) && (iLookAhead > 0) )
         {
            if ( m_pRXBlocksStack[overflow]->received_data_packets + m_pRXBlocksStack[overflow]->received_fec_packets >= m_pRXBlocksStack[overflow]->data_packets )
               break;
            overflow++;
            iLookAhead--;
         }
        
         updateHistoryStatsDiscaredStackSegment(overflow);
         pushIncompleteBlocksOut(overflow, false);
      }

      // Did we discarded everything?
      if ( -1 == m_iRXBlocksStackTopIndex )
      {
         // If we are past recoverable point in a block just discard it and wait for a new block
         if ( video_block_packet_index > block_fecs )
            return -1;

         m_iRXBlocksStackTopIndex = 0;
         addPacketToReceivedBlocksBuffers(pBuffer, length, 0, false);
         return 0;
      }
      stackIndex -= overflow;
   }

   // Add the packet to the buffer
   addPacketToReceivedBlocksBuffers(pBuffer, length, stackIndex, false);
   
   // Add info about any missing blocks in the stack: video block indexes, data scheme, last update time for any skipped blocks
   for( u32 i=0; i<stackIndex; i++ )
      if ( 0 == m_pRXBlocksStack[i]->uTimeLastUpdated )
      {
         m_pRXBlocksStack[i]->uTimeLastUpdated = g_TimeNow;
         m_pRXBlocksStack[i]->data_packets = block_packets;
         m_pRXBlocksStack[i]->fec_packets = block_fecs;
         m_pRXBlocksStack[i]->video_block_index = video_block_index - stackIndex + i;
      }
   return stackIndex;
}

// Return -1 if discarded

int ProcessorRxVideo::preProcessRetransmittedVideoPacket(int interfaceNb, u8* pBuffer, int length)
{
   t_packet_header* pPH = (t_packet_header*) pBuffer;
   
   Model* pModel = findModelWithId(pPH->vehicle_id_src, 127);
   if ( NULL == pModel )
      return -1;
   
   u32 video_block_index = 0;
   u8 video_block_packet_index = 0;
   u32 uIdRetransmissionRequest = 0;

   t_packet_header_video_full_77* pPHVFNew = (t_packet_header_video_full_77*) (pBuffer+sizeof(t_packet_header));    
   video_block_index = pPHVFNew->video_block_index;
   video_block_packet_index = pPHVFNew->video_block_packet_index;
   uIdRetransmissionRequest = pPHVFNew->uLastRecvVideoRetransmissionId;

   g_uTimeLastReceivedResponseToAMessage = g_TimeNow;
 
   int iRetransmissionIndex = -1;
   controller_single_retransmission_state* pRetransmissionInfo = NULL;
   
   for( int i=0; i<m_SM_RetransmissionsStats.iCountActiveRetransmissions; i++ )
   {
      if ( m_SM_RetransmissionsStats.listActiveRetransmissions[i].uRetransmissionId == uIdRetransmissionRequest )
      {
         iRetransmissionIndex = i;
         pRetransmissionInfo = &(m_SM_RetransmissionsStats.listActiveRetransmissions[i]);
         break;
      }
   }

   // Found the retransmission info in the list
   if ( iRetransmissionIndex != -1 )
   {
       u32 dTime = (g_TimeNow - pRetransmissionInfo->uRequestTime);
       if ( (dTime < pRetransmissionInfo->uMinResponseTime) || pRetransmissionInfo->uMinResponseTime == 0 )
          pRetransmissionInfo->uMinResponseTime = dTime;
       if ( dTime > pRetransmissionInfo->uMaxResponseTime )
          pRetransmissionInfo->uMaxResponseTime = dTime;

       if ( dTime < m_SM_RetransmissionsStats.history[0].uMinRetransmissionRoundtripTime ||
            m_SM_RetransmissionsStats.history[0].uMinRetransmissionRoundtripTime == 0 )
          m_SM_RetransmissionsStats.history[0].uMinRetransmissionRoundtripTime = dTime;
       if ( (dTime < 255) && dTime > m_SM_RetransmissionsStats.history[0].uMaxRetransmissionRoundtripTime )
          m_SM_RetransmissionsStats.history[0].uMaxRetransmissionRoundtripTime = dTime;

       // Find the video packet info in the list for this retransmission

       for( int i=0; i<pRetransmissionInfo->uRequestedPackets; i++ )
       {
          if ( pRetransmissionInfo->uRequestedVideoBlockIndex[i] == video_block_index )
          if ( pRetransmissionInfo->uRequestedVideoBlockPacketIndex[i] == video_block_packet_index )
          {
             // Received the first ever response for this retransmission 
             if ( 0 == pRetransmissionInfo->uReceivedPackets )
             {
                m_SM_RetransmissionsStats.history[0].uCountAcknowledgedRetransmissions++;
                m_SM_RetransmissionsStats.totalReceivedRetransmissions++;
             }

             // Received this segment for the first time
             if ( 0 == pRetransmissionInfo->uReceivedPacketCount[i] )
             {
                m_SM_RetransmissionsStats.listActiveRetransmissions[iRetransmissionIndex].uReceivedPacketTime[i] = g_TimeNow;
                m_SM_RetransmissionsStats.history[0].uAverageRetransmissionRoundtripTime += dTime;
                m_SM_RetransmissionsStats.history[0].uCountReceivedRetransmissionPackets++;
                m_SM_RetransmissionsStats.totalReceivedVideoPackets++;
                pRetransmissionInfo->uReceivedPackets++;
             }
             else
                m_SM_RetransmissionsStats.history[0].uCountReceivedRetransmissionPacketsDuplicate++;
                
             pRetransmissionInfo->uReceivedPacketCount[i]++;
             break;
          }
       }

       if ( pRetransmissionInfo->uRequestedPackets == pRetransmissionInfo->uReceivedPackets )
       {
          m_SM_RetransmissionsStats.history[0].uCountCompletedRetransmissions++;
          // Remove it from the retransmissions list
          for( int i=iRetransmissionIndex; i<m_SM_RetransmissionsStats.iCountActiveRetransmissions-1; i++ )
             memcpy( (u8*)&(m_SM_RetransmissionsStats.listActiveRetransmissions[i]), 
                    (u8*)&(m_SM_RetransmissionsStats.listActiveRetransmissions[i+1]),
                    sizeof(controller_single_retransmission_state) );
          m_SM_RetransmissionsStats.iCountActiveRetransmissions--;
       }
   }
   
   if ( -1 == m_iRXBlocksStackTopIndex )
      return -1;

   if ( m_uLastOutputVideoBlockIndex != MAX_U32 )
   if ( video_block_index <= m_uLastOutputVideoBlockIndex )
      return -1;

   int stackIndex = video_block_index - m_pRXBlocksStack[0]->video_block_index;
   if ( (stackIndex < 0) || (stackIndex >= m_iRXMaxBlocksToBuffer) )
      return -1;

   if ( m_pRXBlocksStack[stackIndex]->packetsInfo[video_block_packet_index].state == RX_PACKET_STATE_RECEIVED )
      return -1;

   return 0;
}


// Returns the gap from the last received packet, or -1 if discarded
// The gap is 0 (return value) for retransmitted packets that are not ignored

int ProcessorRxVideo::preProcessReceivedVideoPacket(int interfaceNb, u8* pBuffer, int length)
{
   t_packet_header* pPH = (t_packet_header*) pBuffer;
   
   Model* pModel = findModelWithId(pPH->vehicle_id_src, 128);
   if ( NULL == pModel )
      return -1;

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
   {
      return preProcessRetransmittedVideoPacket(interfaceNb, pBuffer, length );
   }

   u32 video_block_index = 0;
   u8 video_block_packet_index = 0;
   u16 video_data_length = 0;
   u8  block_packets = 0;
   u8  block_fecs = 0;

   t_packet_header_video_full_77* pPHVFNew = (t_packet_header_video_full_77*) (pBuffer+sizeof(t_packet_header));    
   video_block_index = pPHVFNew->video_block_index;
   video_block_packet_index = pPHVFNew->video_block_packet_index;
   video_data_length = pPHVFNew->video_data_length;
   block_packets = pPHVFNew->block_packets;
   block_fecs = pPHVFNew->block_fecs;

   u32 prevRecvVideoBlockIndex = m_LastReceivedVideoPacketInfo.video_block_index;
   u32 prevRecvVideoBlockPacketIndex = m_LastReceivedVideoPacketInfo.video_block_packet_index;

   m_LastReceivedVideoPacketInfo.receive_time = g_TimeNow;
   m_LastReceivedVideoPacketInfo.stream_packet_idx = (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
   m_LastReceivedVideoPacketInfo.video_block_index = video_block_index;
   m_LastReceivedVideoPacketInfo.video_block_packet_index = video_block_packet_index;

   if ( m_uLastOutputVideoBlockIndex != MAX_U32 )
   {
      if ( m_LastReceivedVideoPacketInfo.video_block_index <= m_uLastOutputVideoBlockIndex )
         return -1;
   }

   if ( -1 != m_iRXBlocksStackTopIndex )
   {
      if ( video_block_index < m_pRXBlocksStack[0]->video_block_index )
         return -1;
      
      int stackIndex = video_block_index - m_pRXBlocksStack[0]->video_block_index;
      
      if ( (stackIndex >= 0) && (stackIndex < m_iRXMaxBlocksToBuffer) )
      if ( m_pRXBlocksStack[stackIndex]->packetsInfo[video_block_packet_index].state == RX_PACKET_STATE_RECEIVED )
         return -1;
   }
   
   // Regular video packets that will be processed further

   // Compute gap (in packets) between this and last video packet received
   // Used only for history reporting purposes

   int gap = 0;
   if ( video_block_index == prevRecvVideoBlockIndex )
   if ( video_block_packet_index > prevRecvVideoBlockPacketIndex )
      gap = video_block_packet_index - prevRecvVideoBlockPacketIndex-1;

   if ( video_block_index > prevRecvVideoBlockIndex )
   {
       gap = block_packets + block_fecs - prevRecvVideoBlockPacketIndex - 1;
       gap += video_block_packet_index;

       gap += (block_packets + block_fecs) * (video_block_index - prevRecvVideoBlockIndex - 1);
   }
   if ( gap > m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[0] )
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[0] = (gap>255) ? 255:gap;

   // Check video resolution change

   int width = 0;
   int height = 0;
   u8 video_link_profile = 0;
   u32 encoding_extra_flags = 0;
   int keyframe_ms = 0;
   u8 video_stream_and_type = 0;
   u8 video_fps = 0;
   u32 fec_time = 0;

   width = pPHVFNew->video_width;
   height = pPHVFNew->video_height;
   video_link_profile = pPHVFNew->video_link_profile;
   encoding_extra_flags = pPHVFNew->encoding_extra_flags;
   keyframe_ms = pPHVFNew->video_keyframe_interval_ms;
   video_stream_and_type = pPHVFNew->video_stream_and_type;
   video_fps = pPHVFNew->video_fps;
   fec_time = pPHVFNew->fec_time;

   if ( m_SM_VideoDecodeStats.width != 0 && (m_SM_VideoDecodeStats.width != width || m_SM_VideoDecodeStats.height != height) )
   if ( (m_uLastVideoResolutionChangeVideoBlockIndex == MAX_U32) || (video_block_index > m_uLastVideoResolutionChangeVideoBlockIndex) )
   {
      m_uTimeLastVideoStreamChanged = g_TimeNow;
      m_uLastVideoResolutionChangeVideoBlockIndex = video_block_index;
      log("[VideoRx] Video resolution changed at video block index %u (From %d x %d to %d x %d). Signal reload of the video player.",
         video_block_index, m_SM_VideoDecodeStats.width, m_SM_VideoDecodeStats.height, width, height); 
   }

   // Check if encodings changed (ignore flags related to tx only or irelevant: retransmission window, duplication percent)

   if ( (m_SM_VideoDecodeStats.video_link_profile & 0x0F) != (video_link_profile & 0x0F) ||
        (m_SM_VideoDecodeStats.encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) != (encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) ||
        m_SM_VideoDecodeStats.data_packets_per_block != block_packets ||
        m_SM_VideoDecodeStats.fec_packets_per_block != block_fecs ||
        m_SM_VideoDecodeStats.video_data_length != video_data_length
      )
   if ( (m_uLastHardEncodingsChangeVideoBlockIndex == MAX_U32) || (video_block_index > m_uLastHardEncodingsChangeVideoBlockIndex) )
   {
      m_uEncodingsChangeCount++;
      m_uLastHardEncodingsChangeVideoBlockIndex = video_block_index;

      
      bool bOnlyECschemeChanged = false;
      if ( (block_packets != m_SM_VideoDecodeStats.data_packets_per_block) || (block_fecs != m_SM_VideoDecodeStats.fec_packets_per_block) )
      if ( (m_SM_VideoDecodeStats.video_link_profile & 0x0F) == (video_link_profile & 0x0F) &&
           (m_SM_VideoDecodeStats.encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) == (encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) &&
           (m_SM_VideoDecodeStats.video_data_length == video_data_length)
         )
         bOnlyECschemeChanged = true;

      static u32 s_TimeLastEncodingsChangedLog = 0;
      if ( bOnlyECschemeChanged )
      {
          log("[VideoRx] Detected (only) error correction scheme change (%u times) on video at block idx %u. Data/EC Old: %d/%d, New: %d/%d", m_uEncodingsChangeCount, video_block_index, m_SM_VideoDecodeStats.data_packets_per_block, m_SM_VideoDecodeStats.fec_packets_per_block, block_packets, block_fecs);
      }
      else if ( g_TimeNow > s_TimeLastEncodingsChangedLog + 5000 )
      {
         s_TimeLastEncodingsChangedLog = g_TimeNow;
         m_uTimeLastVideoStreamChanged = g_TimeNow;
         log_line("[VideoRx] Detected video encodings change (%u times) on the received stream (at video block index %u):", m_uEncodingsChangeCount, video_block_index);
         if ( ( m_SM_VideoDecodeStats.video_link_profile & 0x0F) != (video_link_profile & 0x0F) )
            log_line("Video link profile (from [user/current] to [user/current]: [%s/%s] -> [%s/%s]", str_get_video_profile_name((m_SM_VideoDecodeStats.video_link_profile >> 4) & 0x0F), str_get_video_profile_name(m_SM_VideoDecodeStats.video_link_profile & 0x0F), str_get_video_profile_name((video_link_profile >> 4) & 0x0F), str_get_video_profile_name(video_link_profile & 0x0F));
         if ( m_SM_VideoDecodeStats.data_packets_per_block != block_packets )
            log_line("data packets (from/to): %d -> %d", m_SM_VideoDecodeStats.data_packets_per_block, block_packets);
         if ( m_SM_VideoDecodeStats.fec_packets_per_block != block_fecs )
            log_line("ec packets (from/to): %d -> %d", m_SM_VideoDecodeStats.fec_packets_per_block, block_fecs);
         if ( m_SM_VideoDecodeStats.video_data_length != video_data_length )
            log_line("video data length (from/to): %d -> %d", m_SM_VideoDecodeStats.video_data_length, video_data_length);
         if ( (m_SM_VideoDecodeStats.encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) != (encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) )
            log_line("Encoding extra flags (from/to): %u -> %u", m_SM_VideoDecodeStats.encoding_extra_flags, encoding_extra_flags);
         log_line("keyframe (from/to): %d ms -> %d ms", m_SM_VideoDecodeStats.keyframe_ms, keyframe_ms);
      }
      else
      {
         //log_line("New encodings: [%u/0], %d/%d/%d", pPHVF->video_block_index, pPHVF->block_packets, pPHVF->block_fecs, pPHVF->video_data_length);
      }
      // Max retransmission window changed on user selected profile?

      bool bReinitDueToWindowChange = false;
      if ( (video_link_profile & 0x0F) == pModel->video_params.user_selected_video_link_profile )
      if ( m_uLastReceivedVideoLinkProfile == pModel->video_params.user_selected_video_link_profile )
      if ( (encoding_extra_flags & 0xFF00) != (m_SM_VideoDecodeStats.encoding_extra_flags & 0xFF00) )
         bReinitDueToWindowChange = true;

      if ( (m_SM_VideoDecodeStats.encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) != (encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) )
      {
         log_line("[VideoRx] Encoding extra flags changed from/to: %u -> %u.", m_SM_VideoDecodeStats.encoding_extra_flags, encoding_extra_flags);
         log_line("[VideoRx] Encoding extra flags old: [%s]", str_format_video_encoding_flags(m_SM_VideoDecodeStats.encoding_extra_flags));
         log_line("[VideoRx] Encoding extra flags new: [%s]", str_format_video_encoding_flags(encoding_extra_flags));
      }   
      m_SM_VideoDecodeStats.video_link_profile = video_link_profile;
      m_SM_VideoDecodeStats.encoding_extra_flags = encoding_extra_flags;
      m_SM_VideoDecodeStats.data_packets_per_block = block_packets;
      m_SM_VideoDecodeStats.fec_packets_per_block = block_fecs;
      m_SM_VideoDecodeStats.video_data_length = video_data_length;

      if ( bReinitDueToWindowChange )
      {
         log("[VideoRx] Retransmission window (milisec) has changed (to %d ms). Reinitializing rx state...", ((encoding_extra_flags & 0xFF00)>>8)*5);
         resetReceiveState();
         resetOutputState();
         gap = 0;
         m_uLastHardEncodingsChangeVideoBlockIndex = video_block_index;
         m_uLastVideoResolutionChangeVideoBlockIndex = video_block_index;      
      }
   }

   m_uLastReceivedVideoLinkProfile = (video_link_profile & 0x0F);

   if ( (m_SM_VideoDecodeStats.encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) != (encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) )
   {
      log_line("[VideoRx] Received video encoding extra flags changed from/to: %u -> %u.", m_SM_VideoDecodeStats.encoding_extra_flags, encoding_extra_flags);
      log_line("[VideoRx] Video encoding extra flags old: [%s]", str_format_video_encoding_flags(m_SM_VideoDecodeStats.encoding_extra_flags));
      log_line("[VideoRx] Video encoding extra flags new: [%s]", str_format_video_encoding_flags(encoding_extra_flags));
   } 
   m_SM_VideoDecodeStats.encoding_extra_flags = encoding_extra_flags;

   m_SM_VideoDecodeStats.video_stream_and_type = video_stream_and_type;
   m_SM_VideoDecodeStats.keyframe_ms = keyframe_ms;
   m_SM_VideoDecodeStats.fps = video_fps;
   m_SM_VideoDecodeStats.width = width;
   m_SM_VideoDecodeStats.height = height;
   m_SM_VideoDecodeStats.fec_time = fec_time;
   
   // Set initial stream level shift

   int iIndex = getVehicleRuntimeIndex(m_uVehicleId);
   if ( -1 != iIndex )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift == -1 )
         video_link_adaptive_set_intial_video_adjustment_level(m_uVehicleId, m_uLastReceivedVideoLinkProfile, block_packets, block_fecs);

      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedKeyFrameMs <= 0 )
         video_link_keyframe_set_intial_received_level(m_uVehicleId, m_SM_VideoDecodeStats.keyframe_ms);
   }

   return gap;
}
