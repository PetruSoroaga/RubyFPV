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

/*
t_packet_header s_CurrentPH;
t_packet_header_video_full_77 s_CurrentPHVF;

bool s_bPauseVideoPacketsTX = false;
bool s_bPendingEncodingSwitch = false;

u32 sTimeLastFecTimeCalculation = 0;
u32 sTimeTotalFecTimeMicroSec = 0;

ParserH264 s_ParserH264CameraOutput;
ParserH264 s_ParserH264RadioOutput;

u32 s_lCountBytesSend = 0;
u32 s_lCountBytesVideoIn = 0; 
int s_CurrentMaxBlocksInBuffers = MAX_RXTX_BLOCKS_BUFFER;

int s_currentReadBufferIndex = 0;
int s_currentReadBlockPacketIndex = 0;

int s_iCurrentBufferIndexToSend = 0;
int s_iCurrentBlockPacketIndexToSend = 0;

int s_LastSentBlockBufferIndex = -1;

u32 s_TimeLastTelemetryInfoUpdate = 0;
u32 s_TimeLastSemaphoreCheck = 0;
u32 s_TimeLastEncodingSchemeLog = 0;
u32 s_LastEncodingChangeAtVideoBlockIndex = 0;
u32 s_uCountEncodingChanges = 0;

int s_iRetransmissionsDuplicationIndex = 0;
u32 s_uLastReceivedRetransmissionRequestUniqueId = 0;
u32 s_uInjectFaultsCountPacketsDeclined = 0;

#define MAX_HISTORY_RETRANSMISSION_INFO 200

typedef struct
{
   u32 video_block_index;
   u8 video_packet_index;
   u32 uReceiveTime;
   u8 uRepeatCount;
}
type_retransmissions_history_info;

type_retransmissions_history_info s_listRetransmissionsSegmentsInfo[MAX_HISTORY_RETRANSMISSION_INFO];
int s_iCountRetransmissionsSegmentsInfo = 0;
u32 s_listLastRetransmissionsRequestsTimes[MAX_HISTORY_RETRANSMISSION_INFO];
int s_iCountLastRetransmissionsRequestsTimes = 0;
*/

int ProcessorTxVideo::m_siInstancesCount = 0;
u32 s_lCountBytesVideoIn = 0; 

bool process_data_tx_is_on_iframe()
{
   //return s_ParserH264CameraOutput.IsInsideIFrame();
   return false;
}


ProcessorTxVideo::ProcessorTxVideo(int iVideoStreamIndex, int iCameraIndex)
:m_bInitialized(false)
{
   m_iInstanceIndex = m_siInstancesCount;
   m_siInstancesCount++;

   m_iVideoStreamIndex = iVideoStreamIndex;
   m_iCameraIndex = iCameraIndex;

   //m_iLastRequestedAdaptiveVideoLevelFromController = 0;
   //m_uInitialSetCaptureVideoBitrate = 0;
   //m_uLastSetCaptureVideoBitrate = 0;

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
      setLastSetCaptureVideoBitrate(g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate, true, 0);
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


void ProcessorTxVideo::setLastRequestedAdaptiveVideoLevelFromController(int iLevel)
{
   m_iLastRequestedAdaptiveVideoLevelFromController = iLevel;
}

void ProcessorTxVideo::setLastSetCaptureVideoBitrate(u32 uBitrate, bool bInitialValue, int iSource)
{
   m_uLastSetCaptureVideoBitrate = uBitrate;
   if ( bInitialValue || (0 == m_uInitialSetCaptureVideoBitrate) )
      m_uInitialSetCaptureVideoBitrate = uBitrate;

   m_uLastSetCaptureVideoBitrate |= VIDEO_BITRATE_FLAG_ADJUSTED;
   if ( bInitialValue || ((m_uLastSetCaptureVideoBitrate & VIDEO_BITRATE_FIELD_MASK) == m_uInitialSetCaptureVideoBitrate) )
      m_uLastSetCaptureVideoBitrate &= ~VIDEO_BITRATE_FLAG_ADJUSTED;
}

int ProcessorTxVideo::getLastRequestedAdaptiveVideoLevelFromController()
{
   return m_iLastRequestedAdaptiveVideoLevelFromController;
}

u32 ProcessorTxVideo::getLastSetCaptureVideoBitrate()
{
   return m_uLastSetCaptureVideoBitrate;
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

// Returns true if it changed

bool _check_update_video_link_profile_data()
{
 /*
   bool bChanged = false;

   if ( s_CurrentPHVF.video_link_profile != g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
   {
      s_CurrentPHVF.video_link_profile = g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile;
      bChanged = true;
   }
   if ( s_CurrentPHVF.video_width != g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].width )
   {
      s_CurrentPHVF.video_width = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].width;
      bChanged = true;
   }
   if ( s_CurrentPHVF.video_height != g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].height )
   {
      s_CurrentPHVF.video_height = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].height;
      bChanged = true;
   }
   if ( s_CurrentPHVF.video_fps != g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].fps )
   {
      s_CurrentPHVF.video_fps = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].fps;
      bChanged = true;
   }
   
   if ( 0 != g_SM_VideoLinkStats.overwrites.currentDataBlocks )
   {
      if ( s_CurrentPHVF.block_packets != g_SM_VideoLinkStats.overwrites.currentDataBlocks )
      {
         s_CurrentPHVF.block_packets = g_SM_VideoLinkStats.overwrites.currentDataBlocks;
         bChanged = true;
      }
      if ( s_CurrentPHVF.block_fecs != g_SM_VideoLinkStats.overwrites.currentECBlocks )
      {
         s_CurrentPHVF.block_fecs = g_SM_VideoLinkStats.overwrites.currentECBlocks;
         bChanged = true;
      }
   }
   else
   {
      if ( s_CurrentPHVF.block_packets != g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_packets )
      {
         s_CurrentPHVF.block_packets = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_packets;
         bChanged = true;
      }
      if ( s_CurrentPHVF.block_fecs != g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs )
      {
         s_CurrentPHVF.block_fecs = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs;
         bChanged = true;
      }
   }

   if ( s_CurrentPHVF.video_data_length != g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].video_data_length )
   {
      s_CurrentPHVF.video_data_length = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].video_data_length;
      bChanged = true;
   }

   if ( g_bDeveloperMode != ((s_CurrentPHVF.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS)?true:false) )
   {
      if ( g_bDeveloperMode )
         s_CurrentPHVF.uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS;
      else
         s_CurrentPHVF.uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS;
      bChanged = true;
   }

   if ( bChanged )
   {
      s_uCountEncodingChanges++;
      s_LastEncodingChangeAtVideoBlockIndex = s_CurrentPHVF.video_block_index;
   }
   return bChanged;
   */
 return false;
}

// returns true if packet should not be sent

bool _inject_recoverable_faults(int bufferIndex, u32 streamPacketIndex, int packetIndex, bool isRetransmitted)
{
 /*
   static int s_iInjectFaultCount = 0;

   if ( isRetransmitted )
      return false;

   if ( (rand()%200) < 1 )
      s_iInjectFaultCount++;

   if ( (rand()%3000) < 1 )
      s_iInjectFaultCount+=3;

   if ( s_iInjectFaultCount > 0 )
   {
      s_iInjectFaultCount--;
      return true;
   }
   */
   return false;
}

// returns true if packet should not be sent

bool _inject_faults(int bufferIndex, u32 streamPacketIndex, int packetIndex, bool isRetransmitted)
{
 /*
   static u32 s_uIFLastTimeInject = 0;
   static u32 s_uIFLastTimeInjectDuration = 0;
   static bool s_bIFLastTimeInjectAllowRetransmissions = false;
   //static u32 s_uIFLastPacketToInject = 0;
   //static u32 s_uIFLastPacketToInjectDelta = 0;

   if ( ! isRetransmitted )
   if ( g_TimeNow > s_uIFLastTimeInject + s_uIFLastTimeInjectDuration )
   {
      log_line("Stopped injecting fault at video packet [%u/%d]", s_BlocksTxBuffers[bufferIndex].video_block_index, packetIndex);
      int iShortInject = ((rand()%5) == 2);
      if ( iShortInject )
      {
         s_uIFLastTimeInject = g_TimeNow + 1000+(rand()%3000);
         s_uIFLastTimeInjectDuration = 10 + (rand()%30);
         s_bIFLastTimeInjectAllowRetransmissions = (rand()%2)?true:false;
      }
      else
      {
         s_uIFLastTimeInject = g_TimeNow + 4000+(rand()%3000);
         s_uIFLastTimeInjectDuration = 20 + (rand()%200);
         //s_bIFLastTimeInjectAllowRetransmissions = (rand()%3)?false:true;
         s_bIFLastTimeInjectAllowRetransmissions = false;
      }
      s_uInjectFaultsCountPacketsDeclined = 0;
      log_line("Generated new %s inject faults segments, starting %d ms from now, %d ms long, allow retransmissions: %s;",
         (iShortInject?"short":"long"), s_uIFLastTimeInject - g_TimeNow, s_uIFLastTimeInjectDuration, s_bIFLastTimeInjectAllowRetransmissions?"yes":"no");
   }

   if ( g_TimeNow >= s_uIFLastTimeInject && g_TimeNow <= s_uIFLastTimeInject + s_uIFLastTimeInjectDuration )
   {
      if ( ! isRetransmitted )
      {
         if ( 0 == s_uInjectFaultsCountPacketsDeclined )
            log_line("Started inject faults period for %d ms, starting at video packet [%u/%d], allow retransmissions: %s;",
               s_uIFLastTimeInjectDuration, s_BlocksTxBuffers[bufferIndex].video_block_index, packetIndex, s_bIFLastTimeInjectAllowRetransmissions?"yes":"no");
         
         s_uInjectFaultsCountPacketsDeclined++;
         return true;
      }
      if ( s_bIFLastTimeInjectAllowRetransmissions )
         return false;
      return true;
   }
*/
   /*
   if ( 0 )
   if ( (0 != s_uIFLastTimeInject) && (streamPacketIndex > s_uIFLastTimeInject + s_uIFLastPacketToInjectDelta) )
   {
      s_uIFLastPacketToInject = streamPacketIndex + 20 + (rand()%50);
      s_uIFLastPacketToInjectDelta = 1 + (rand()%10);
   }

   if ( 0 )
   if ( (0 != s_uIFLastPacketToInject) && (streamPacketIndex >= s_uIFLastPacketToInject && streamPacketIndex < s_uIFLastPacketToInject + s_uIFLastPacketToInjectDelta) )
      return true;
   return false;
   */


   return false;

   //if ( (rand()%500) < 10 )
   //    return true;


   //if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 500 ) == 200 )
   //   return true;
   //return false;

   //if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 1000 ) > 200 )
   //if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 1000 ) < 250 )
   //if ( packetIndex == 0 || packetIndex == 1 || packetIndex == 2 || packetIndex != s_BlocksTxBuffers[bufferIndex].block_packets+1 )
   //   return true;

   //if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 500 ) > 100 )
   //{
   //   if ( packetIndex == 4 || packetIndex == 7 || packetIndex == 8 )
   //         return false;
   //   return true;
   //}

/*
   if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 1000 ) == 100 ||
        ( s_BlocksTxBuffers[bufferIndex].video_block_index % 1000 ) == 101 )
   if ( packetIndex == 0 || packetIndex == 1 || packetIndex == 2 || packetIndex == 3 )   
         return true;
   if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 500 ) > 250 )
   if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 500 ) < 450 )
   if ( packetIndex == 0 || packetIndex == 1 || packetIndex == 2 || packetIndex == s_BlocksTxBuffers[bufferIndex].block_packets+1 )
   if ( ! isRetransmitted )
   {
      static int sss_i = 0;
      sss_i++;
      return true;
   }

   //if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 1000 ) > 950 )
   //if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 1000 ) < 953 )
   //if ( packetIndex == 0 || packetIndex == 1 || packetIndex == s_BlocksTxBuffers[bufferIndex].block_packets+1 )
   //if ( ! isRetransmitted )
   //   return true;

   //if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 1000 ) > 800 )
   //if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 1000 ) < 950 )
   //if ( packetIndex == 0 || packetIndex == 1 || packetIndex == 2 || packetIndex == s_BlocksTxBuffers[bufferIndex].block_packets+1 )
   //   return true;

      /*
      if ( ( s_BlocksTxBuffers[bufferIndex].video_block_index % 900 ) > 200 )
      {
         if ( sssbi != s_BlocksTxBuffers[bufferIndex].video_block_index )
         {
            sssbi = s_BlocksTxBuffers[bufferIndex].video_block_index;
            for( int i=0; i<10; i++ )
               ssspi[i] = 0;
            ssspi[0] = 1;
            for( int kk=0; kk<2; kk++ )
            {
                int i = rand()%10;
                if ( i > 9 ) i = 9;
                ssspi[i] = 1;
            }
         }
         if ( ssspi[packetIndex] == 1 )
         if ( ! isRetransmitted )
           return;
      }
      */

   return false;
}

// Sends a packet to radio

void _send_packet(int bufferIndex, int packetIndex, bool isRetransmitted, bool isDuplicationPacket, bool isLastBlockToSend)
{
 /*
   if ( s_BlocksTxBuffers[bufferIndex].packetsInfo[packetIndex].flags != PACKET_FLAG_SENT &&
        s_BlocksTxBuffers[bufferIndex].packetsInfo[packetIndex].flags != PACKET_FLAG_READ )
      return;

   s_BlocksTxBuffers[bufferIndex].packetsInfo[packetIndex].flags = PACKET_FLAG_SENT;

   u8* pPacketData = s_BlocksTxBuffers[bufferIndex].packetsInfo[packetIndex].pRawData;
   t_packet_header* pPH = (t_packet_header*)(pPacketData);
   t_packet_header_video_full_77* pPHVF = (t_packet_header_video_full_77*)(pPacketData + sizeof(t_packet_header));

   if ( isRetransmitted )
      pPHVF->uLastRecvVideoRetransmissionId = s_uLastReceivedRetransmissionRequestUniqueId;
   else
   {
      pPHVF->uLastRecvVideoRetransmissionId = 0;
      if ( pPHVF->uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
      {
         u8* pExtraData = pPacketData + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_77) + pPHVF->video_data_length;
         u32* pExtraDataU32 = (u32*)pExtraData;
         pExtraDataU32[2] = get_current_timestamp_ms();
      }
   }

   pPH->stream_packet_idx = s_CurrentPH.stream_packet_idx;
   s_CurrentPH.stream_packet_idx++;
   s_CurrentPH.stream_packet_idx &= PACKET_FLAGS_MASK_STREAM_PACKET_IDX;
   s_CurrentPH.stream_packet_idx |= (STREAM_ID_VIDEO_1) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;

   pPHVF->uLastSetVideoBitrate = g_pProcessorTxVideo->getLastSetCaptureVideoBitrate();
   if ( isRetransmitted )
   {
      pPH->packet_flags = PACKET_COMPONENT_VIDEO | PACKET_FLAGS_BIT_RETRANSMITED;
      pPHVF->uLastAckKeyframeInterval = 0;
      pPHVF->uLastAckLevelShift = 0;
   }
   else
   {
      pPH->packet_flags = PACKET_COMPONENT_VIDEO;
      pPH->packet_flags &= (~PACKET_FLAGS_BIT_CAN_START_TX);

      if ( isLastBlockToSend && (packetIndex == s_BlocksTxBuffers[bufferIndex].block_packets+s_BlocksTxBuffers[bufferIndex].block_fecs-1) )
         pPH->packet_flags |= PACKET_FLAGS_BIT_CAN_START_TX;

      if ( g_pCurrentModel->rxtx_sync_type != RXTX_SYNC_TYPE_ADV )
         pPH->packet_flags |= PACKET_FLAGS_BIT_CAN_START_TX;

      if ( NULL != g_pProcessorTxVideo )
      {
         pPHVF->uLastAckKeyframeInterval = (u16) g_SM_VideoLinkStats.overwrites.uCurrentControllerRequestedKeyframeMs;
         pPHVF->uLastAckLevelShift = (u8) g_pProcessorTxVideo->getLastRequestedAdaptiveVideoLevelFromController();
      }
   }
   
   pPH->packet_flags |= PACKET_FLAGS_BIT_HEADERS_ONLY_CRC;

   static u8 uLastVideoProfile = 0;
   if ( uLastVideoProfile != (pPHVF->video_link_profile & 0x0F) )
   {
      uLastVideoProfile = (pPHVF->video_link_profile & 0x0F);
   }

   // If video paused or sending only relayed remote video, do not send it

   if ( s_bPauseVideoPacketsTX || (! relay_current_vehicle_must_send_own_video_feeds()) )
   {
      return;
   }

   if ( g_bDeveloperMode )
   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_VIDEO_FAULTS )
   if ( _inject_faults(bufferIndex, pPH->stream_packet_idx, packetIndex, isRetransmitted) )
      return;

   if ( g_bDeveloperMode )
   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_RECOVERABLE_VIDEO_FAULTS )
   if ( _inject_recoverable_faults(bufferIndex, pPH->stream_packet_idx, packetIndex, isRetransmitted) )
      return;

   if ( ((s_CurrentPHVF.video_stream_and_type >> 4) & 0x0F) == VIDEO_TYPE_H264 )
   if ( (! isRetransmitted) && (! isDuplicationPacket) )
   if ( packetIndex < s_BlocksTxBuffers[bufferIndex].block_packets )
   if ( NULL != g_pCurrentModel )
   if ( (g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDLayout] & OSD_FLAG_SHOW_STATS_VIDEO_KEYFRAMES_INFO) ||
        (g_iDebugShowKeyFramesAfterRelaySwitch > 0) )
   {
      u8* pVideoData = pPacketData + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_77);
      
      bool bStartOfFrameDetected = s_ParserH264RadioOutput.parseData(pVideoData, pPHVF->video_data_length, g_TimeNow);
      if ( bStartOfFrameDetected )
      {         
         u32 uLastFrameDuration = s_ParserH264RadioOutput.getTimeDurationOfLastCompleteFrame();
         if ( uLastFrameDuration > 127 )
            uLastFrameDuration = 127;
         if ( uLastFrameDuration < 1 )
            uLastFrameDuration = 1;

         u32 uLastFrameSize = s_ParserH264RadioOutput.getSizeOfLastCompleteFrameInBytes();
         uLastFrameSize /= 1000; // transform to kbytes

         if ( uLastFrameSize > 127 )
            uLastFrameSize = 127; // kbytes

         g_VideoInfoStatsRadioOut.uLastFrameIndex = (g_VideoInfoStatsRadioOut.uLastFrameIndex+1) % MAX_FRAMES_SAMPLES;
         g_VideoInfoStatsRadioOut.uFramesDuration[g_VideoInfoStatsRadioOut.uLastFrameIndex] = uLastFrameDuration;
         g_VideoInfoStatsRadioOut.uFramesTypesAndSizes[g_VideoInfoStatsRadioOut.uLastFrameIndex] = (g_VideoInfoStatsRadioOut.uFramesTypesAndSizes[g_VideoInfoStatsRadioOut.uLastFrameIndex] & 0x80) | ((u8)uLastFrameSize);
          
         u32 uNextIndex = (g_VideoInfoStatsRadioOut.uLastFrameIndex+1) % MAX_FRAMES_SAMPLES;
         
         if ( s_ParserH264RadioOutput.IsInsideIFrame() )
            g_VideoInfoStatsRadioOut.uFramesTypesAndSizes[uNextIndex] |= (1<<7);
         else
            g_VideoInfoStatsRadioOut.uFramesTypesAndSizes[uNextIndex] &= 0x7F;
      
         g_VideoInfoStatsRadioOut.uDetectedKeyframeIntervalMs = s_ParserH264RadioOutput.getCurrentlyDetectedKeyframeIntervalMs();
         g_VideoInfoStatsRadioOut.uDetectedFPS = s_ParserH264RadioOutput.getDetectedFPS();
         g_VideoInfoStatsRadioOut.uDetectedSlices = (u32) s_ParserH264RadioOutput.getDetectedSlices();

         if ( g_iDebugShowKeyFramesAfterRelaySwitch > 0 )
         if ( s_ParserH264RadioOutput.IsInsideIFrame() )
         {
            log_line("[Debug] Transmitting keyframe after relay switch.");
            g_iDebugShowKeyFramesAfterRelaySwitch--;
         }
      }
   }
   send_packet_to_radio_interfaces(pPacketData, pPH->total_length, -1);

   s_lCountBytesSend += pPH->total_length;
   s_lCountBytesSend += 14; // radio headers
   */
}

// Returns the number of packets ready to be sent

int process_data_tx_video_has_packets_ready_to_send()
{
 /*
   int countReadyToSend = 0;

   int iBufferIndex = s_iCurrentBufferIndexToSend;
   int iPacketIndex = s_iCurrentBlockPacketIndexToSend;

   while ( true )
   {
      if ( ! ( s_BlocksTxBuffers[iBufferIndex].packetsInfo[iPacketIndex].flags & PACKET_FLAG_READ ) )
         break;
      countReadyToSend++;

      iPacketIndex++;
      if ( iPacketIndex >= s_BlocksTxBuffers[iBufferIndex].block_packets )
      {
         iPacketIndex = 0;
         iBufferIndex++;
         if ( iBufferIndex >= s_CurrentMaxBlocksInBuffers )
            iBufferIndex = 0;
      }
   }
   return countReadyToSend;
   */
 return 0;
}

// How many packets backwards to send (from the available ones)
// Returns the number of packets sent

int process_data_tx_video_send_packets_ready_to_send(int howMany)
{
 /*
   if ( howMany <= 0 )
      return 0;

   int countSent = 0;
   
   u32 uMicroTime = 0;
   if ( howMany > 5 )
      uMicroTime = get_current_timestamp_micros();

   for( int i=0; i<howMany; i++ )
   {
      if ( ! ( s_BlocksTxBuffers[s_iCurrentBufferIndexToSend].packetsInfo[s_iCurrentBlockPacketIndexToSend].flags & PACKET_FLAG_READ ) )
         break;

      _send_packet(s_iCurrentBufferIndexToSend, s_iCurrentBlockPacketIndexToSend, false, false, true);
      countSent++;

      // Send ec packet if any

      u32 uECSpreadHigh = (s_CurrentPHVF.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT)?1:0;
      u32 uECSpreadLow = (s_CurrentPHVF.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT)?1:0;
      int iECSpread = (int)(uECSpreadLow | (uECSpreadHigh<<1));

      if ( 0 == iECSpread )
      {
         if ( s_iCurrentBlockPacketIndexToSend == (s_BlocksTxBuffers[s_iCurrentBufferIndexToSend].block_packets - 1) )
         {
            for( int k=0; k<s_BlocksTxBuffers[s_iCurrentBufferIndexToSend].block_fecs; k++ )
               _send_packet(s_iCurrentBufferIndexToSend, s_BlocksTxBuffers[s_iCurrentBufferIndexToSend].block_packets + k, false, false, true);
         }
      }
      else if ( s_BlocksTxBuffers[s_iCurrentBufferIndexToSend].block_fecs > 0 )
      {
         int iECPacketsPerSlice = s_BlocksTxBuffers[s_iCurrentBufferIndexToSend].block_fecs/(iECSpread+1);
         if ( iECPacketsPerSlice < 1 )
            iECPacketsPerSlice = 1;

         int iDeltaBlocks = s_iCurrentBlockPacketIndexToSend/iECPacketsPerSlice;
         if ( iDeltaBlocks > iECSpread )
            iDeltaBlocks = iECSpread;

         int iPrevBlockToSend = s_iCurrentBufferIndexToSend - iDeltaBlocks - 1;
         if ( iPrevBlockToSend < 0 )
            iPrevBlockToSend += s_CurrentMaxBlocksInBuffers;

         if ( s_iCurrentBlockPacketIndexToSend < s_BlocksTxBuffers[iPrevBlockToSend].block_fecs )
         if ( s_BlocksTxBuffers[iPrevBlockToSend].packetsInfo[s_BlocksTxBuffers[iPrevBlockToSend].block_packets + s_iCurrentBlockPacketIndexToSend].flags & PACKET_FLAG_READ )
            _send_packet(iPrevBlockToSend, s_BlocksTxBuffers[iPrevBlockToSend].block_packets + s_iCurrentBlockPacketIndexToSend, false, false, true);
         
      }
      s_iCurrentBlockPacketIndexToSend++;
      if ( s_iCurrentBlockPacketIndexToSend >= s_BlocksTxBuffers[s_iCurrentBufferIndexToSend].block_packets )
      {
         s_iCurrentBlockPacketIndexToSend = 0;
         s_iCurrentBufferIndexToSend++;
         if ( s_iCurrentBufferIndexToSend >= s_CurrentMaxBlocksInBuffers )
            s_iCurrentBufferIndexToSend = 0;
      }
   }

   static int sl_iCountSuccessiveOverloads = 0;
   if ( howMany > 5 )
   {
      uMicroTime = get_current_timestamp_micros() - uMicroTime;
      u32 uVideoPacketsPerSec = g_pProcessorTxVideo->getCurrentTotalVideoBitrateAverage() / 1200 / 8;
      if ( uVideoPacketsPerSec < 10 )
         uVideoPacketsPerSec = 10;
      if ( uMicroTime/howMany > 500000/uVideoPacketsPerSec )
      {
         sl_iCountSuccessiveOverloads++;
         
      }
      else
      {
         if ( sl_iCountSuccessiveOverloads > 3 )
            sl_iCountSuccessiveOverloads -= 3;
      }
   }
   else if ( sl_iCountSuccessiveOverloads > 0 )
         sl_iCountSuccessiveOverloads--;
   return countSent;
   */
 return 0;
}


// Returns true if a block is complete

bool _onNewCompletePacketReadFromInput()
{
 /*
   u32 uTimeDiff = g_TimeNow - g_TimeLastVideoPacketIn;
   g_TimeLastVideoPacketIn = g_TimeNow;
   
   // If video is paused or if transmitting only remote relay video, then skip it
   if ( s_bPauseVideoPacketsTX || (! relay_current_vehicle_must_send_own_video_feeds()) )
   {
      s_LastSentBlockBufferIndex = -1;
      s_currentReadBufferIndex = 0;
      s_currentReadBlockPacketIndex = 0;
      s_iCurrentBufferIndexToSend = 0;
      s_iCurrentBlockPacketIndexToSend = 0;

      s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].currentReadPosition = 0;
      _reset_tx_buffers();
      return false;
   }

   if ( 0xFF == g_PHVehicleTxStats.historyVideoPacketsGapMax[0] )
      g_PHVehicleTxStats.historyVideoPacketsGapMax[0] = uTimeDiff;
   if ( uTimeDiff > g_PHVehicleTxStats.historyVideoPacketsGapMax[0] )
      g_PHVehicleTxStats.historyVideoPacketsGapMax[0] = uTimeDiff;

   g_PHVehicleTxStats.tmp_uVideoIntervalsCount++;
   g_PHVehicleTxStats.tmp_uVideoIntervalsSum += uTimeDiff;
   
   int iMaxPackets = s_CurrentPHVF.block_fecs + s_CurrentPHVF.block_packets;
   if ( s_CurrentPHVF.block_packets > s_CurrentPHVF.block_fecs )
      iMaxPackets = s_CurrentPHVF.block_packets*2;
   if ( iMaxPackets > s_iCurrentMaxTxPacketsInAVideoBlock )
   {
      log_line("[VideoTx] Must increase block packets buffer from %d to %d", s_iCurrentMaxTxPacketsInAVideoBlock, iMaxPackets );

      for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
      {
         s_BlocksTxBuffers[i].iAllocatedPackets = iMaxPackets;
         for( int k=s_iCurrentMaxTxPacketsInAVideoBlock; k<s_BlocksTxBuffers[i].iAllocatedPackets; k++ )
         {
            s_BlocksTxBuffers[i].packetsInfo[k].pRawData = (u8*) malloc(MAX_PACKET_TOTAL_SIZE);
            if ( NULL == s_BlocksTxBuffers[i].packetsInfo[k].pRawData )
            {
               log_error_and_alarm("[VideoTx] Failed to alocate memory for buffers.");
               s_BlocksTxBuffers[i].iAllocatedPackets = k;
               break;
            }
         }
      }
      s_iCurrentMaxTxPacketsInAVideoBlock = iMaxPackets;
   }

   // Save the new packet in the tx buffers

   s_BlocksTxBuffers[s_currentReadBufferIndex].video_block_index = s_CurrentPHVF.video_block_index;
   s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].flags = PACKET_FLAG_READ;
   s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].uTimestamp = g_TimeNow;

   s_CurrentPHVF.video_link_profile = g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile;
   s_CurrentPHVF.uProfileEncodingFlags = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].uProfileEncodingFlags;

   s_CurrentPHVF.uLastRecvVideoRetransmissionId = 0;
   s_CurrentPHVF.uVideoStatusFlags2 &= 0xFFFFFF00;
   s_CurrentPHVF.uVideoStatusFlags2 |= g_SM_VideoLinkStats.overwrites.currentH264QUantization & 0xFF;
     
   if ( s_ParserH264CameraOutput.IsInsideIFrame() )
      s_CurrentPHVF.uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_IFRAME;
   else
      s_CurrentPHVF.uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_IS_IFRAME;
   

   s_CurrentPHVF.uVideoStatusFlags2 &= ~VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE;
   if ( g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile] != 0 )
      s_CurrentPHVF.uVideoStatusFlags2 |= VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE;

   if ( g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs < 255*250 )
      s_CurrentPHVF.video_keyframe_interval_ms = g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs;
   else
      s_CurrentPHVF.video_keyframe_interval_ms = 255*250;

   u8* pPacketData = s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].pRawData;
   t_packet_header* pPH = (t_packet_header*)(pPacketData);
   t_packet_header_video_full_77* pPHVF = (t_packet_header_video_full_77*)(((u8*)(pPacketData)) + sizeof(t_packet_header));
   memcpy(pPH, &s_CurrentPH, sizeof(t_packet_header));
   memcpy(pPHVF, &s_CurrentPHVF, sizeof(t_packet_header_video_full_77));

   if ( s_CurrentPHVF.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
   {
      static u32 s_uDebugLastAddedPacketTimestamp = 0;
      u8* pExtraData = pPacketData + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_77) + pPHVF->video_data_length;
      u32* pExtraDataU32 = (u32*)pExtraData;
      pExtraDataU32[1] = get_current_timestamp_ms();
      pExtraDataU32[0] = pExtraDataU32[1] - s_uDebugLastAddedPacketTimestamp;
      s_uDebugLastAddedPacketTimestamp = pExtraDataU32[1];
   }
   // Go to next packet in the buffer

   s_currentReadBlockPacketIndex++;
   s_CurrentPHVF.video_block_packet_index++;

   // Still on the data packets ?
   if ( s_currentReadBlockPacketIndex < s_CurrentPHVF.block_packets )
   {
      s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].flags = PACKET_FLAG_EMPTY;
      s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].uTimestamp = 0;
      s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].currentReadPosition = 0;
      return false;
   }      

   // Generate and add EC packets if EC is enabled

   if ( s_CurrentPHVF.block_fecs > 0 )
   {
      for( int i=0; i<s_CurrentPHVF.block_packets; i++ )
         p_fec_data_packets[i] = ((u8*)s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[i].pRawData) + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_77);

      for( int i=0; i<s_CurrentPHVF.block_fecs; i++ )
         p_fec_data_fecs[i] = ((u8*)s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_CurrentPHVF.block_packets+i].pRawData) + sizeof(t_packet_header) + sizeof(t_packet_header_video_full_77);

      u32 tTemp = get_current_timestamp_micros();
      fec_encode(s_BlocksTxBuffers[s_currentReadBufferIndex].video_data_length, p_fec_data_packets, s_CurrentPHVF.block_packets, p_fec_data_fecs, s_CurrentPHVF.block_fecs);
      tTemp = get_current_timestamp_micros() - tTemp;
      sTimeTotalFecTimeMicroSec += tTemp;

      for( int i=0; i<s_CurrentPHVF.block_fecs; i++ )
      {
         s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].flags = PACKET_FLAG_READ;
         s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].uTimestamp = g_TimeNow;
         s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].currentReadPosition = 0;

         t_packet_header* pHeader = (t_packet_header*)(s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].pRawData);
         t_packet_header_video_full_77* pVideo = (t_packet_header_video_full_77*)(((u8*)(pHeader)) + sizeof(t_packet_header));
         memcpy(pHeader, &s_CurrentPH, sizeof(t_packet_header));
         memcpy(pVideo, &s_CurrentPHVF, sizeof(t_packet_header_video_full_77));

         s_currentReadBlockPacketIndex++;
         s_CurrentPHVF.video_block_packet_index++;
      }
   }

   if ( s_bPendingEncodingSwitch )
   {
      //log_line("Applying pending encodings change (starting at video block index %u):", s_CurrentPHVF.video_block_index);
      s_bPendingEncodingSwitch = false;

      if ( _check_update_video_link_profile_data() )
         _log_encoding_scheme();

      s_CurrentPH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      s_CurrentPH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_video_full_77) + s_CurrentPHVF.video_data_length;
      if ( s_CurrentPHVF.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS )
         s_CurrentPH.total_length += 6 * sizeof(u32);
   }

   // We read to the end of a video block
   // Move to next video block in the tx queue

   s_CurrentPHVF.video_block_index++;
   s_CurrentPHVF.video_block_packet_index = 0;

   s_currentReadBufferIndex++;
   s_currentReadBlockPacketIndex = 0;

   if ( s_currentReadBufferIndex >= s_CurrentMaxBlocksInBuffers )
      s_currentReadBufferIndex = 0;

   if ( s_currentReadBufferIndex == s_iCurrentBufferIndexToSend )
   {
      s_iCurrentBlockPacketIndexToSend = 0;
      s_iCurrentBufferIndexToSend++;
      if ( s_iCurrentBufferIndexToSend >= s_CurrentMaxBlocksInBuffers )
         s_iCurrentBufferIndexToSend = 0;
   }

   // Reset info on the next video block to send

   s_BlocksTxBuffers[s_currentReadBufferIndex].video_block_index = s_CurrentPHVF.video_block_index;
   for( int i=0; i<MAX_TOTAL_PACKETS_IN_BLOCK; i++ )
   {
      s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[i].flags = PACKET_FLAG_EMPTY;
      s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[i].uTimestamp = 0;
      s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[i].currentReadPosition = 0;
   }

   s_BlocksTxBuffers[s_currentReadBufferIndex].video_data_length = s_CurrentPHVF.video_data_length;
   s_BlocksTxBuffers[s_currentReadBufferIndex].block_packets = s_CurrentPHVF.block_packets;
   s_BlocksTxBuffers[s_currentReadBufferIndex].block_fecs = s_CurrentPHVF.block_fecs;

   // Reset info on the first packet of next video block to send
   s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[0].currentReadPosition = 0;

   pPacketData = s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[0].pRawData;
   pPH = (t_packet_header*)pPacketData;
   pPHVF = (t_packet_header_video_full_77*)(pPacketData + sizeof(t_packet_header));
   memcpy(pPH, &s_CurrentPH, sizeof(t_packet_header));
   memcpy(pPHVF, &s_CurrentPHVF, sizeof(t_packet_header_video_full_77));
*/
   return true;
}

bool process_data_tx_video_init()
{
 /*
   log_line("[VideoTx] Allocating tx buffers (%d blocks, max %d packets/block)...", MAX_RXTX_BLOCKS_BUFFER, MAX_TOTAL_PACKETS_IN_BLOCK);
   s_iCurrentMaxTxPacketsInAVideoBlock = g_pCurrentModel->get_current_max_video_packets_for_all_profiles();

   log_line("[VideoTx] Current model max packets needed in a block (data+ec): %d", s_iCurrentMaxTxPacketsInAVideoBlock);
   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   {
      s_BlocksTxBuffers[i].iAllocatedPackets = s_iCurrentMaxTxPacketsInAVideoBlock;
      for( int k=0; k<s_BlocksTxBuffers[i].iAllocatedPackets; k++ )
      {
         s_BlocksTxBuffers[i].packetsInfo[k].pRawData = (u8*) malloc(MAX_PACKET_TOTAL_SIZE);
         if ( NULL == s_BlocksTxBuffers[i].packetsInfo[k].pRawData )
         {
            log_error_and_alarm("[VideoTx] Failed to alocate memory for buffers.");
            s_BlocksTxBuffers[i].iAllocatedPackets = k;
            return false;
         }
      }
   }
   log_line("[VideoTx] Allocated tx buffers (%d blocks, max %d packets/block).", MAX_RXTX_BLOCKS_BUFFER, s_iCurrentMaxTxPacketsInAVideoBlock);

   s_ParserH264CameraOutput.init();
   s_ParserH264RadioOutput.init();


   s_uCountEncodingChanges = 0;
   _reset_tx_buffers();

   log_line("[VideoTx] Allocated %u Mb for video Tx buffers (%d blocks)", (u32)MAX_RXTX_BLOCKS_BUFFER * (u32)s_iCurrentMaxTxPacketsInAVideoBlock * (u32) MAX_PACKET_TOTAL_SIZE / 1000 / 1000, MAX_RXTX_BLOCKS_BUFFER );

   radio_packet_init(&s_CurrentPH, PACKET_COMPONENT_VIDEO | PACKET_FLAGS_BIT_HEADERS_ONLY_CRC, PACKET_TYPE_VIDEO_DATA_FULL, STREAM_ID_VIDEO_1);

   s_CurrentPH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   s_CurrentPH.vehicle_id_dest = 0;

   _check_update_video_link_profile_data();

   s_currentReadBufferIndex = 0;
   s_currentReadBlockPacketIndex = 0;
   s_iCurrentBufferIndexToSend = 0;
   s_iCurrentBlockPacketIndexToSend = 0;

   s_BlocksTxBuffers[0].video_block_index = 0;
   s_BlocksTxBuffers[0].video_data_length = s_CurrentPHVF.video_data_length;
   s_BlocksTxBuffers[0].block_packets = s_CurrentPHVF.block_packets;
   s_BlocksTxBuffers[0].block_fecs = s_CurrentPHVF.block_fecs;

   s_BlocksTxBuffers[0].packetsInfo[0].flags = PACKET_FLAG_EMPTY;
   s_BlocksTxBuffers[0].packetsInfo[0].uTimestamp = 0;
   s_BlocksTxBuffers[0].packetsInfo[0].currentReadPosition = 0;

   t_packet_header* pHeader = (t_packet_header*)(s_BlocksTxBuffers[0].packetsInfo[0].pRawData);
   t_packet_header_video_full_77* pVideo = (t_packet_header_video_full_77*)(((u8*)(pHeader)) + sizeof(t_packet_header));
   memcpy(pHeader, &s_CurrentPH, sizeof(t_packet_header));
   memcpy(pVideo, &s_CurrentPHVF, sizeof(t_packet_header_video_full_77));

   process_data_tx_video_reset_retransmissions_history_info();

   for( int i=0; i<MAX_TOTAL_PACKETS_IN_BLOCK; i++ )
   {
      s_BlocksTxBuffers[0].packetsInfo[i].flags = PACKET_FLAG_EMPTY;
      s_BlocksTxBuffers[0].packetsInfo[i].uTimestamp = 0;
   }
   s_LastSentBlockBufferIndex = -1;


   u32 videoBitrate = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps;
   float miliPerBlock = (float)((u32)(1000 * 8 * (u32)g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].video_data_length * (u32)g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_packets)) / (float)videoBitrate;
   u32 uMaxWindowMS = 500;
   u32 bytesToBuffer = (((float)videoBitrate / 1000.0) * uMaxWindowMS)/8.0;
   log_line("[VideoTx] Need buffers to cache %u bits of video (%u bytes of video for %u ms of video)", bytesToBuffer*8, bytesToBuffer, uMaxWindowMS);
   u32 uMaxBlocksToBuffer = 2 + bytesToBuffer/g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].video_data_length/g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_packets;
   
   // Add extra time for last retransmissions (50 milisec)
   if ( miliPerBlock > 0.0001 )
      uMaxBlocksToBuffer += 50.0/miliPerBlock;
   
   uMaxBlocksToBuffer *= 1.2;

   if ( uMaxBlocksToBuffer < 5 )
      uMaxBlocksToBuffer = 5;
   if ( uMaxBlocksToBuffer >= MAX_RXTX_BLOCKS_BUFFER )
      uMaxBlocksToBuffer = MAX_RXTX_BLOCKS_BUFFER-1;

   log_line("[VideoTx] Computed result: Will cache a maximum of %u video blocks, for a total of %d miliseconds of video (max retransmission window is %u ms); one block stores %.1f miliseconds of video",
       uMaxBlocksToBuffer, (int)((float)miliPerBlock * (float)uMaxBlocksToBuffer), uMaxWindowMS, miliPerBlock);

   s_CurrentMaxBlocksInBuffers = MAX_RXTX_BLOCKS_BUFFER;

   g_TimeLastVideoPacketIn = get_current_timestamp_ms();
   _log_encoding_scheme();
*/
   return true;
}

bool process_data_tx_video_uninit()
{
   return true;
}

void _process_command_resend_packets(u8* pPacketBuffer, u8 packetType)
{   
 /*
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   u8* pData = pPacketBuffer + sizeof(t_packet_header);

   if ( packetType == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2 )
   {
      memcpy((u8*)&s_uLastReceivedRetransmissionRequestUniqueId, pData, sizeof(u32));
      pData += sizeof(u32); // Skip: Retransmission request unique Id
   }

   //u8 videoStreamId = *pData; //Skip video stream Id
   pData++;
   u8 countSegmentsRequested = *pData;
   pData++;

   u32 requested_video_block_index = MAX_U32;
   u8  requested_video_packet_index = 0;
   u8  requested_retry_count = 0;
   
   
   {
      char szBuff[256];
      char szTmp[32];
      szBuff[0] = 0;
      u8* pTmp = pData;
      for( u8 c=0; c<countSegmentsRequested; c++ )
      {
         memcpy(&requested_video_block_index, pTmp, sizeof(u32));
         pTmp += sizeof(u32);
         requested_video_packet_index = *pTmp;
         pTmp++;
         requested_retry_count = *pTmp;
         pTmp++;

         if ( 0 != szBuff[0] )
            strcat(szBuff, ", ");
         sprintf(szTmp, "[%u/%d] x %d", requested_video_block_index, requested_video_packet_index, requested_retry_count);
         strcat(szBuff, szTmp);
         if ( c > 4 )
            break;
      }
   }

   g_PHTE_Retransmissions.totalReceivedRetransmissionsRequestsUnique++;

   if ( s_iCountLastRetransmissionsRequestsTimes < MAX_HISTORY_RETRANSMISSION_INFO )
   {
      s_listLastRetransmissionsRequestsTimes[s_iCountLastRetransmissionsRequestsTimes] = g_TimeNow;
      s_iCountLastRetransmissionsRequestsTimes++;
   }

   if ( g_SM_VideoLinkGraphs.tmp_vehileReceivedRetransmissionsRequestsCount < 255 )
      g_SM_VideoLinkGraphs.tmp_vehileReceivedRetransmissionsRequestsCount++;
   if ( g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPackets + countSegmentsRequested <= 255 )
      g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPackets += countSegmentsRequested;
   else
      g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPackets = 255;

   for( u8 c=0; c<countSegmentsRequested; c++ )
   {
      memcpy(&requested_video_block_index, pData, sizeof(u32));
      pData += sizeof(u32);
      requested_video_packet_index = *pData;
      pData++;
      requested_retry_count = *pData;
      pData++;


      int iDuplicateSegmentIndex = -1;
      for( int i=0; i<s_iCountRetransmissionsSegmentsInfo; i++ )
         if ( (s_listRetransmissionsSegmentsInfo[i].video_block_index == requested_video_block_index) &&
              (s_listRetransmissionsSegmentsInfo[i].video_packet_index == requested_video_packet_index) )
         {
            iDuplicateSegmentIndex = i;
            break;
         }

      if ( -1 != iDuplicateSegmentIndex )
      {
         s_listRetransmissionsSegmentsInfo[iDuplicateSegmentIndex].uRepeatCount++;
         g_PHTE_Retransmissions.totalReceivedRetransmissionsRequestsSegmentsRetried++;
      }
      else if ( s_iCountRetransmissionsSegmentsInfo < MAX_HISTORY_RETRANSMISSION_INFO )
      {
         s_listRetransmissionsSegmentsInfo[s_iCountRetransmissionsSegmentsInfo].video_block_index = requested_video_block_index;
         s_listRetransmissionsSegmentsInfo[s_iCountRetransmissionsSegmentsInfo].video_packet_index = requested_video_packet_index;
         s_listRetransmissionsSegmentsInfo[s_iCountRetransmissionsSegmentsInfo].uRepeatCount = 0;
         s_listRetransmissionsSegmentsInfo[s_iCountRetransmissionsSegmentsInfo].uReceiveTime = g_TimeNow;
         g_PHTE_Retransmissions.totalReceivedRetransmissionsRequestsSegmentsUnique++;
      }
      
      if ( requested_retry_count > 1 )
      {
         if ( g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPacketsRetried + (requested_retry_count-1) <= 255 )
            g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPacketsRetried += (requested_retry_count-1);
         else
            g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPacketsRetried = 255;
      }
      if ( requested_video_block_index > s_CurrentPHVF.video_block_index )
         continue;

      int diff = s_CurrentPHVF.video_block_index-requested_video_block_index;
      if ( diff >= s_CurrentMaxBlocksInBuffers )
         continue;

      int bufferIndex = s_currentReadBufferIndex - diff;
      if ( bufferIndex < 0 )
         bufferIndex += s_CurrentMaxBlocksInBuffers;

      if ( s_BlocksTxBuffers[bufferIndex].video_block_index != requested_video_block_index )
         continue;

      if ( requested_video_packet_index >= s_BlocksTxBuffers[bufferIndex].block_packets + s_BlocksTxBuffers[bufferIndex].block_fecs )
         continue;

      if ( s_BlocksTxBuffers[bufferIndex].packetsInfo[requested_video_packet_index].flags != PACKET_FLAG_READ &&
           s_BlocksTxBuffers[bufferIndex].packetsInfo[requested_video_packet_index].flags != PACKET_FLAG_SENT )
         continue;

      //log_line("Resending packet [%u/%d]", requested_video_block_index, requested_video_packet_index);

      _send_packet(bufferIndex, (int)requested_video_packet_index, true, false, false);

      s_iRetransmissionsDuplicationIndex++;
      bool bDoDuplicate = false;
      u32 uValue = 0xFF;
      if ( (NULL != g_pCurrentModel) && ((s_CurrentPHVF.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT) != VIDEO_PROFILE_ENCODING_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO) )
         uValue = (s_CurrentPHVF.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT) >> 16;

      // Manual duplication percent or auto ? 0xF0 - auto
      if ( (uValue >> 4) != 0x0F )
      {
         int percent = (int)((uValue & 0xFF) >> 4); // from 0 to 10, 0x0F for auto
         if ( percent <= 10 )
         {
            int freq = 0;
            if ( percent != 0 )
               freq = (10/percent);
            if ( percent < 7 )
               freq++;
            if ( percent >= 9 )
               freq = 0;
            //log_line("percent: %d, freq: %d, current: %d", percent, freq, s_iRetransmissionsDuplicationIndex);
            if ( percent > 0 && s_iRetransmissionsDuplicationIndex > freq )
               bDoDuplicate = true;
         }
      }

      if ( (uValue >> 4) == 0x0F )
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
      if ( g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel > 1 )
         bDoDuplicate = true;

      if ( bDoDuplicate )
      {
         s_iRetransmissionsDuplicationIndex = 0;
         _send_packet(bufferIndex, (int)requested_video_packet_index, true, false, false);
      }
   }

   if ( pPH->total_length > sizeof(t_packet_header) + 2*sizeof(u8) + countSegmentsRequested*(sizeof(u32) + 2*sizeof(u8)) )
   {
      // Extract controller radio & video links stats from pData pointer
      //u8 flagsAndVersion = *pData;
      pData++;
   }
   */
}

extern bool bDebugNoVideoOutput;

bool process_data_tx_video_command(int iRadioInterface, u8* pPacketBuffer)
{
/*
   if ( bDebugNoVideoOutput )
      return true;
     
   if ( ! (s_CurrentPHVF.uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS) )
      return false;

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS || pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2 )
      _process_command_resend_packets(pPacketBuffer, pPH->packet_type );
*/

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
      
      //if ( NULL != g_pProcessorTxVideo )
      //   g_pProcessorTxVideo->setLastRequestedAdaptiveVideoLevelFromController(iAdaptiveLevel);
      
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
 /*
   if ( g_TimeNow >= sTimeLastFecTimeCalculation + 500)
   {
      sTimeLastFecTimeCalculation = g_TimeNow;
      s_CurrentPHVF.fec_time = 2*sTimeTotalFecTimeMicroSec;
      sTimeTotalFecTimeMicroSec = 0;

      // Update retransmission statistics for the last 5 secs
      // Discard all the info older than 5 secs

      g_PHTE_Retransmissions.totalReceivedRetransmissionsRequestsSegmentsUniqueLast5Sec = s_iCountRetransmissionsSegmentsInfo;
      g_PHTE_Retransmissions.totalReceivedRetransmissionsRequestsSegmentsRetriedLast5Sec = 0;

      for( int i=s_iCountRetransmissionsSegmentsInfo-1; i>=0; i-- )
      {
         if ( (s_listRetransmissionsSegmentsInfo[i].uReceiveTime == 0) || (s_listRetransmissionsSegmentsInfo[i].uReceiveTime+500 < g_TimeNow) )
         {
            g_PHTE_Retransmissions.totalReceivedRetransmissionsRequestsSegmentsUniqueLast5Sec = s_iCountRetransmissionsSegmentsInfo - i;
             
            // Move segments down

            for( int k=0; k<s_iCountRetransmissionsSegmentsInfo-i-1; k++ )
               memcpy((u8*)&(s_listRetransmissionsSegmentsInfo[k]), (u8*)&(s_listRetransmissionsSegmentsInfo[k+i+1]), sizeof(type_retransmissions_history_info));
            s_iCountRetransmissionsSegmentsInfo = s_iCountRetransmissionsSegmentsInfo-i-1;
            break;
         }
         if ( s_listRetransmissionsSegmentsInfo[i].uRepeatCount > 0 )
            g_PHTE_Retransmissions.totalReceivedRetransmissionsRequestsSegmentsRetriedLast5Sec++;
      }

      g_PHTE_Retransmissions.totalReceivedRetransmissionsRequestsUniqueLast5Sec = s_iCountLastRetransmissionsRequestsTimes;
         
      for( int i=s_iCountLastRetransmissionsRequestsTimes-1; i>=0; i-- )
      {
         if ( (s_listLastRetransmissionsRequestsTimes[i] == 0) || (s_listLastRetransmissionsRequestsTimes[i] + 500 < g_TimeNow) )
         {
            for( int k=0; k<s_iCountLastRetransmissionsRequestsTimes-i-1; k++ )
               s_listLastRetransmissionsRequestsTimes[k] = s_listLastRetransmissionsRequestsTimes[k+i+1];
            s_iCountLastRetransmissionsRequestsTimes = s_iCountLastRetransmissionsRequestsTimes - i - 1;
            g_PHTE_Retransmissions.totalReceivedRetransmissionsRequestsUniqueLast5Sec = s_iCountLastRetransmissionsRequestsTimes;
            break;
         }
      }
   }
*/
   g_pProcessorTxVideo->periodicLoop();
   return true;
}

u8* process_data_tx_video_get_current_buffer_to_read_pointer()
{
   //return ((u8*)s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].pRawData)+sizeof(t_packet_header)+sizeof(t_packet_header_video_full_77) + s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].currentReadPosition;
return NULL;
}

int process_data_tx_video_get_current_buffer_to_read_size()
{
   //return s_BlocksTxBuffers[s_currentReadBufferIndex].video_data_length - s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].currentReadPosition;
return 0;
}

void _parse_camera_source_h264_data(u8* pData, int iDataSize)
{
 /*
   if ( (NULL == g_pCurrentModel) || (NULL == pData) || (iDataSize <= 0) )
      return;
   if ( ! g_pCurrentModel->hasCamera() )
      return;

   bool bStartOfFrameDetected = s_ParserH264CameraOutput.parseData(pData, iDataSize, g_TimeNow);
   if ( ! bStartOfFrameDetected )
      return;

   if ( g_iDebugShowKeyFramesAfterRelaySwitch > 0 )
   if ( s_ParserH264CameraOutput.IsInsideIFrame() )
   {
      log_line("[Debug] Start reading video keyframe from camera after relay switch.");
      g_iDebugShowKeyFramesAfterRelaySwitch--;
   }

   u32 uLastFrameDuration = s_ParserH264CameraOutput.getTimeDurationOfLastCompleteFrame();
   if ( uLastFrameDuration > 127 )
      uLastFrameDuration = 127;
   if ( uLastFrameDuration < 1 )
      uLastFrameDuration = 1;

   u32 uLastFrameSize = s_ParserH264CameraOutput.getSizeOfLastCompleteFrameInBytes();
   uLastFrameSize /= 1000; // transform to kbytes

   if ( uLastFrameSize > 127 )
      uLastFrameSize = 127; // kbytes

   g_VideoInfoStatsCameraOutput.uLastFrameIndex = (g_VideoInfoStatsCameraOutput.uLastFrameIndex+1) % MAX_FRAMES_SAMPLES;
   g_VideoInfoStatsCameraOutput.uFramesDuration[g_VideoInfoStatsCameraOutput.uLastFrameIndex] = uLastFrameDuration;
   g_VideoInfoStatsCameraOutput.uFramesTypesAndSizes[g_VideoInfoStatsCameraOutput.uLastFrameIndex] = (g_VideoInfoStatsCameraOutput.uFramesTypesAndSizes[g_VideoInfoStatsCameraOutput.uLastFrameIndex] & 0x80) | ((u8)uLastFrameSize);
 
   u32 uNextIndex = (g_VideoInfoStatsCameraOutput.uLastFrameIndex+1) % MAX_FRAMES_SAMPLES;
  
   if ( s_ParserH264CameraOutput.IsInsideIFrame() )
      g_VideoInfoStatsCameraOutput.uFramesTypesAndSizes[uNextIndex] |= (1<<7);
   else
      g_VideoInfoStatsCameraOutput.uFramesTypesAndSizes[uNextIndex] &= 0x7F;

   g_VideoInfoStatsCameraOutput.uDetectedKeyframeIntervalMs = s_ParserH264CameraOutput.getCurrentlyDetectedKeyframeIntervalMs();

   if ( s_ParserH264CameraOutput.IsInsideIFrame() )
      return;

   g_VideoInfoStatsCameraOutput.uDetectedFPS = s_ParserH264CameraOutput.getDetectedFPS();
   g_VideoInfoStatsCameraOutput.uDetectedSlices = (u32) s_ParserH264CameraOutput.getDetectedSlices();
   

   // We are on P frames now. Check if we need to set a different keyframe interval
   // and if we are far enough from last I-frame to do the change

   if ( g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs == g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs )
      return;
   if ( (g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs == 0) || (g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs == 0) )
      return;

   bool bUpdateKeyframeIntervalNow = false;
   u32 uTimeStartOfLastIFrame = s_ParserH264CameraOutput.getStartTimeOfLastIFrame();

   if ( g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs > g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs )
   if ( ((g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs < 500 ) && (g_TimeNow >= uTimeStartOfLastIFrame + g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs/2)) || 
        ((g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs >= 500 ) && (g_TimeNow >= uTimeStartOfLastIFrame + (g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs*4)/5)) )
      bUpdateKeyframeIntervalNow = true;
   if ( g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs < g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs )
   if ( ((g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs < 500 ) && (g_TimeNow >= uTimeStartOfLastIFrame + g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs*3/4)) || 
        ((g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs >= 500 ) && (g_TimeNow >= uTimeStartOfLastIFrame + g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs*4/5)) )
      bUpdateKeyframeIntervalNow = true;

   if ( ! bUpdateKeyframeIntervalNow )
      return;

   // Set highest bit to mark keyframe changed 
   g_VideoInfoStatsCameraOutput.uFramesDuration[g_VideoInfoStatsCameraOutput.uLastFrameIndex] |= 0x80;
   
   g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs = g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs;
   
   // Send the actual keyframe change to video source/capture
   
   int iCurrentFPS = 30;
   if ( NULL != g_pCurrentModel )
     iCurrentFPS = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].fps;

   int iKeyFrameCountValue = (iCurrentFPS * g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs) / 1000; 
   u32 uKeyframeCount = iKeyFrameCountValue;

   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
   {
      video_source_csi_send_control_message(RASPIVID_COMMAND_ID_KEYFRAME, (u16)uKeyframeCount, 0);
   }
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      float fGOP = 1.0;
      fGOP = ((float)g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs)/1000.0;
      hardware_camera_maj_set_keyframe(fGOP);                
   }
   */
}

// Returns true if a complete block was read
bool process_data_tx_video_on_new_data(u8* pData, int iDataSize)
{
   s_lCountBytesVideoIn += iDataSize;
 /*
   if ( (NULL == pData) || (iDataSize <= 0) )
      return false;

   bool bCompleteBlock = false;

   // Always parse the input video stream as we might need to do keyframe adjustment
   if ( NULL != g_pCurrentModel )
     _parse_camera_source_h264_data(pData, iDataSize);

   s_lCountBytesVideoIn += iDataSize;

   while ( iDataSize > 0 )
   {
      int iBytesLeftInCurrentVideoPacket =  process_data_tx_video_get_current_buffer_to_read_size();
      u8* pVideoPacket = process_data_tx_video_get_current_buffer_to_read_pointer();

      int iBytesToCopy = iDataSize;
      if ( iBytesToCopy > iBytesLeftInCurrentVideoPacket )
         iBytesToCopy = iBytesLeftInCurrentVideoPacket;

      memcpy(pVideoPacket, pData, iBytesToCopy);
      s_BlocksTxBuffers[s_currentReadBufferIndex].packetsInfo[s_currentReadBlockPacketIndex].currentReadPosition += iBytesToCopy;

      iDataSize -= iBytesToCopy;
      pData += iBytesToCopy;

      // We have a new complete video packet
      if ( iBytesToCopy == iBytesLeftInCurrentVideoPacket )
      {
         bCompleteBlock |= _onNewCompletePacketReadFromInput();
      }
   }
   return bCompleteBlock;
   */
 return false;
}

void process_data_tx_video_signal_model_changed()
{
   //log_line("TXVideo: Received model changed notification");
   //s_bPendingEncodingSwitch = true;
}

void process_data_tx_video_pause_tx()
{
   //s_bPauseVideoPacketsTX = true;
}

void process_data_tx_video_resume_tx()
{
   //s_bPauseVideoPacketsTX = false;
}

void process_data_tx_video_reset_retransmissions_history_info()
{
 /*
   memset((u8*)&g_PHTE_Retransmissions, 0, sizeof(g_PHTE_Retransmissions));

   memset((u8*)&s_listRetransmissionsSegmentsInfo, 0, sizeof(type_retransmissions_history_info) * MAX_HISTORY_RETRANSMISSION_INFO);
   s_iCountRetransmissionsSegmentsInfo = 0;
   s_iCountLastRetransmissionsRequestsTimes = 0;
   */
}