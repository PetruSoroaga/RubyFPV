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
#include "processor_rx_video_wfbohd.h"
#include "rx_video_output.h"
#include "packets_utils.h"
#include "links_utils.h"
#include "timers.h"

ProcessorRxVideoWFBOHD::ProcessorRxVideoWFBOHD(u32 uVehicleId, u32 uVideoStreamIndex)
:ProcessorRxVideo(uVehicleId, uVideoStreamIndex)
{
   log_line("[VideoRx] Created a video rx processor for an OpenIPC camera.");
}

ProcessorRxVideoWFBOHD::~ProcessorRxVideoWFBOHD()
{
}

bool ProcessorRxVideoWFBOHD::init()
{
   if ( m_bInitialized )
      return true;

   m_bInitialized = true;
   
   memset(&m_SM_VideoDecodeStats, 0, sizeof(shared_mem_video_stream_stats));
   m_SM_VideoDecodeStats.uVehicleId = m_uVehicleId;
   m_SM_VideoDecodeStats.uVideoStreamIndex = m_uVideoStreamIndex;
   m_SM_VideoDecodeStats.frames_type = RADIO_FLAGS_FRAME_TYPE_DATA;
   m_SM_VideoDecodeStats.video_stream_and_type = 0 | (VIDEO_TYPE_OPENIPC<<4);
   m_SM_VideoDecodeStats.video_link_profile = 0;
      
   m_SM_VideoDecodeStats.data_packets_per_block = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
   m_SM_VideoDecodeStats.fec_packets_per_block = DEFAULT_VIDEO_BLOCK_FECS_HP;
   m_SM_VideoDecodeStats.video_data_length = DEFAULT_VIDEO_DATA_LENGTH_HP;
   m_SM_VideoDecodeStats.fps = 30;
   m_SM_VideoDecodeStats.width = 1280;
   m_SM_VideoDecodeStats.height = 720;   
  
   memset((u8*)&m_SM_VideoDecodeStatsHistory, 0, sizeof(shared_mem_video_stream_stats_history));
   m_SM_VideoDecodeStatsHistory.uVehicleId = m_uVehicleId;
   m_SM_VideoDecodeStatsHistory.uVideoStreamIndex = m_uVideoStreamIndex;
   m_SM_VideoDecodeStatsHistory.outputHistoryIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;

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


   memset((u8*)&m_SM_RetransmissionsStats, 0, sizeof(shared_mem_controller_retransmissions_stats));
   m_SM_RetransmissionsStats.uVehicleId = m_uVehicleId;
   m_SM_RetransmissionsStats.uVideoStreamIndex = m_uVideoStreamIndex;
   m_SM_RetransmissionsStats.uGraphRefreshIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;

   return true;
}

bool ProcessorRxVideoWFBOHD::uninit()
{
   if ( ! m_bInitialized )
      return true;

   log_line("[VideoRx] Uninitialize video processor Rx OpenIPC instance number %d for VID %u, video stream index %d", m_iInstanceIndex+1, m_uVehicleId, m_uVideoStreamIndex);
   
   m_bInitialized = false;
   return true;
}

void ProcessorRxVideoWFBOHD::resetState()
{
}


void ProcessorRxVideoWFBOHD::periodicLoop(u32 uTimeNow)
{
   if ( uTimeNow >= m_TimeLastVideoStatsUpdate+50 )
   {
      m_TimeLastVideoStatsUpdate = uTimeNow;

      m_SM_VideoDecodeStats.frames_type = radio_get_received_frames_type();
      m_SM_VideoDecodeStats.iCurrentRxTxSyncType = g_pCurrentModel->rxtx_sync_type;
   }

   updateHistoryStats(uTimeNow);
}

// Returns 1 if a video block has just finished and the flag "Can TX" is set

int ProcessorRxVideoWFBOHD::handleReceivedVideoPacket(int interfaceNb, u8* pBuffer, int length)
{
   //t_packet_header* pPH = (t_packet_header*)pBuffer;
   t_packet_header_video_full_77* pPHVF = (t_packet_header_video_full_77*) (pBuffer+sizeof(t_packet_header));    
  
   if ( pPHVF->video_block_index == m_LastReceivedVideoPacketInfo.video_block_index )
   {
      if ( pPHVF->video_block_packet_index >= pPHVF->block_packets )
         m_SM_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[0]++;
      return 0;
   }

   if ( pPHVF->video_block_index > m_LastReceivedVideoPacketInfo.video_block_index+1 )
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[0]++;
   else
      m_SM_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[0]++;
 
   m_LastReceivedVideoPacketInfo.video_block_index = pPHVF->video_block_index;
   m_LastReceivedVideoPacketInfo.video_block_packet_index = pPHVF->video_block_packet_index;
 

   m_SM_VideoDecodeStats.data_packets_per_block = pPHVF->block_packets;
   m_SM_VideoDecodeStats.fec_packets_per_block = pPHVF->block_fecs;
   m_SM_VideoDecodeStats.video_data_length = pPHVF->video_data_length;

   return 0;
}
