/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
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
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopacketsqueue.h"

#include "shared_vars.h"
#include "processor_rx_video.h"
#include "processor_rx_video_forward.h"
#include "video_link_adaptive.h"
#include "video_link_keyframe.h"
#include "packets_utils.h"
#include "timers.h"

#define MAX_RETRANSMISSION_BUFFER_HISTORY_LENGTH 20
#define MAX_BLOCKS_TO_OUTPUT_IF_AVAILABLE 20

typedef struct
{
   unsigned int fec_decode_missing_packets_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int fec_decode_fec_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_data_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* fec_decode_fec_packets_pointers[MAX_TOTAL_PACKETS_IN_BLOCK];
   unsigned int missing_packets_count;
}
type_fec_info;

typedef struct
{
   u32 uRetransmissionTimeMinim; // in miliseconds
   u32 uRetransmissionTimeAverage;
   u32 uRetransmissionTimeLast;
   u32 retransmissionTimePreviousBuffer[MAX_RETRANSMISSION_BUFFER_HISTORY_LENGTH];
   u32 uRetransmissionTimePreviousIndex;
   u32 uRetransmissionTimePreviousSum;
   u32 uRetransmissionTimePreviousSumCount;
}
type_retransmission_stats;

typedef struct
{
   u32 receive_time;
   u32 stream_packet_idx;
   u32 video_block_index;
   u32 video_block_packet_index;
}
type_last_rx_packet_info;

#define RX_PACKET_STATE_EMPTY 0
#define RX_PACKET_STATE_RECEIVED 1

typedef struct
{
   int state;
   int packet_length;
   u8 uRetrySentCount;
   u32 uTimeFirstRetrySent;
   u32 uTimeLastRetrySent;
   u8* pData;
}
type_received_block_packet_info;

typedef struct
{
   u32 video_block_index; // MAX_U32 if it's empty
   int packet_length;
   int data_packets;
   int fec_packets;
   int received_data_packets;
   int received_fec_packets;

   int totalPacketsRequested;
   u32 uTimeFirstPacketReceived;
   u32 uTimeFirstRetrySent;
   u32 uTimeLastRetrySent;
   u32 uTimeLastUpdated;
   type_received_block_packet_info packetsInfo[MAX_TOTAL_PACKETS_IN_BLOCK];

} type_received_block_info;


type_fec_info s_FECInfo;
type_retransmission_stats s_RetransmissionStats;

shared_mem_video_decode_stats s_VDStatsCache; // local copy to update faster
shared_mem_video_decode_stats* s_pSM_VideoDecodeStats = NULL; // shared mem copy to update slower

shared_mem_video_decode_stats_history* s_pSM_VideoDecodeStatsHistory = NULL; // shared mem copy to update slower
shared_mem_controller_retransmissions_stats* s_pSM_ControllerRetransmissionsStats = NULL;

u32 s_TimeLastEncodingsChangedLog = 0;
u32 s_TimeLastVideoStatsUpdate = 0;
u32 s_TimeLastHistoryStatsUpdate = 0;
u32 s_TimeLastControllerRestransmissionsStatsUpdate = 0;

type_last_rx_packet_info s_LastReceivedVideoPacketInfo;

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

static u32 s_uRequestRetransmissionUniqueId = 0;

// Blocks are stored in order in the stack (based on video block index)

type_received_block_info* s_pRXBlocksStack[MAX_RXTX_BLOCKS_BUFFER];
int s_RXBlocksStackTopIndex = -1;
int s_RXMaxBlocksToBuffer = 0;


extern t_packet_queue s_QueueRadioPackets;


void _rx_video_log_line(const char* format, ...)
{
   if ( g_fdLogFile <= 0 )
      return;

   char szTime[64];
   sprintf(szTime,"%d:%02d:%02d.%03d", (int)(g_TimeNow/1000/60/60), (int)(g_TimeNow/1000/60)%60, (int)((g_TimeNow/1000)%60), (int)(g_TimeNow%1000));
 
   fprintf(g_fdLogFile, "%s: ", szTime);  

   va_list args;
   va_start(args, format);

   vfprintf(g_fdLogFile, format, args);

   /*
   char szBuff[256];
   sprintf(szBuff, " (last recv: [%u/%d]) stack_ptr: %d ", s_LastReceivedVideoPacketInfo.video_block_index, s_LastReceivedVideoPacketInfo.video_block_packet_index, s_RXBlocksStackTopIndex);
   for( int i=0; i<=s_RXBlocksStackTopIndex; i++)
   {
      if ( i > 3 )
      {
         strcat(szBuff,"...");
         break;
      }
      if ( i > 0 )
         strcat(szBuff, ", ");
      char szTmp[32];
      sprintf(szTmp, "[%u: ", s_pRXBlocksStack[i]->video_block_index);
      strcat(szBuff, szTmp);
      for( int k=0; k<s_pRXBlocksStack[i]->data_packets + s_pRXBlocksStack[i]->fec_packets; k++ )
      {
         if ( s_pRXBlocksStack[i]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
            sprintf(szTmp,"%d", k);
         else
            sprintf(szTmp, "x");
         strcat(szBuff, szTmp);
      }
      strcat(szBuff, "]");
   }
   fprintf(g_fdLogFile, szBuff);
   */

   fprintf(g_fdLogFile, "\n");
   fflush(g_fdLogFile);
}

void _log_current_buffer(bool bIncludeRetransmissions)
{
   char szBuff[256];
   sprintf(szBuff, "DEBUG: last output video block index: %u, (last recv: [%u/%d]) stack_ptr: %d ", s_LastOutputVideoBlockIndex, s_LastReceivedVideoPacketInfo.video_block_index, s_LastReceivedVideoPacketInfo.video_block_packet_index, s_RXBlocksStackTopIndex);
   for( int i=0; i<=s_RXBlocksStackTopIndex; i++)
   {
      if ( i > 3 )
      {
         strcat(szBuff,"...");
         break;
      }
      if ( i > 0 )
         strcat(szBuff, ", ");
      char szTmp[32];
      sprintf(szTmp, "[%u: ", s_pRXBlocksStack[i]->video_block_index);
      strcat(szBuff, szTmp);
      for( int k=0; k<s_pRXBlocksStack[i]->data_packets + s_pRXBlocksStack[i]->fec_packets; k++ )
      {
         if ( s_pRXBlocksStack[i]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
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
      sprintf(szBuff, "DEBUG: first block retransmission requests: block %u = [", s_pRXBlocksStack[0]->video_block_index);
      for( int k=0; k<s_pRXBlocksStack[0]->data_packets + s_pRXBlocksStack[0]->fec_packets; k++ )
      {
         if ( 0 != k )
            strcat(szBuff, ", ");
         if ( s_pRXBlocksStack[0]->packetsInfo[k].uTimeFirstRetrySent == 0 ||
              s_pRXBlocksStack[0]->packetsInfo[k].uTimeLastRetrySent == 0 )
            strcat(szBuff, "(!)");

         sprintf(szTmp,"%d", s_pRXBlocksStack[0]->packetsInfo[k].uRetrySentCount);
         strcat(szBuff, szTmp);

         if ( s_pRXBlocksStack[0]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
            strcat(szBuff, "(r)");
      }
      strcat(szBuff, "]");
      log_line(szBuff);
   }
}

void _rx_video_reset_receive_buffer_block(int rx_buffer_block_index)
{
   for( int k=0; k<s_pRXBlocksStack[rx_buffer_block_index]->data_packets + s_pRXBlocksStack[rx_buffer_block_index]->fec_packets; k++ )
   {
      s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[k].state = RX_PACKET_STATE_EMPTY;
      s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[k].uRetrySentCount = 0;
      s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[k].uTimeFirstRetrySent = 0;
      s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[k].uTimeLastRetrySent = 0;
      s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[k].packet_length = 0;
   }
   s_pRXBlocksStack[rx_buffer_block_index]->video_block_index = MAX_U32;
   s_pRXBlocksStack[rx_buffer_block_index]->packet_length = 0;
   s_pRXBlocksStack[rx_buffer_block_index]->data_packets = 0;
   s_pRXBlocksStack[rx_buffer_block_index]->fec_packets = 0;
   s_pRXBlocksStack[rx_buffer_block_index]->received_data_packets = 0;
   s_pRXBlocksStack[rx_buffer_block_index]->received_fec_packets = 0;
   s_pRXBlocksStack[rx_buffer_block_index]->totalPacketsRequested = 0;
   s_pRXBlocksStack[rx_buffer_block_index]->uTimeFirstPacketReceived = MAX_U32;
   s_pRXBlocksStack[rx_buffer_block_index]->uTimeFirstRetrySent = 0;
   s_pRXBlocksStack[rx_buffer_block_index]->uTimeLastRetrySent = 0;
   s_pRXBlocksStack[rx_buffer_block_index]->uTimeLastUpdated = 0;
}

void _rx_video_reset_receive_buffers()
{
   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   {
      s_pRXBlocksStack[i]->data_packets = MAX_TOTAL_PACKETS_IN_BLOCK;
      s_pRXBlocksStack[i]->fec_packets = 0;
      _rx_video_reset_receive_buffer_block(i);
   }

   s_RXBlocksStackTopIndex = -1;

   g_VideoDecodeStatsHistory.totalCurrentlyMissingPackets = 0;
   s_VDStatsCache.currentPacketsInBuffers = 0;
   s_VDStatsCache.maxPacketsInBuffers = 10;
   s_VDStatsCache.total_DiscardedBuffers++;
}

void _rx_video_reset_receive_state()
{
   log_line("Reset video RX state and buffers (count: %d)", s_VDStatsCache.total_DiscardedBuffers);
   _rx_video_log_line("");
   _rx_video_log_line("-----------------------------------");
   _rx_video_log_line("Reset video RX state and buffers (count: %d)...", s_VDStatsCache.total_DiscardedBuffers);

   s_RXBlocksStackTopIndex = -1;

   s_LastOutputVideoBlockTime = 0;
   s_LastOutputVideoBlockIndex = MAX_U32;

   s_LastReceivedVideoPacketInfo.receive_time = 0;
   s_LastReceivedVideoPacketInfo.video_block_index = MAX_U32;
   s_LastReceivedVideoPacketInfo.video_block_packet_index = MAX_U32;
   s_LastReceivedVideoPacketInfo.stream_packet_idx = MAX_U32;

   process_data_rx_video_reset_retransmission_stats();
   
   // Compute how many blocks to buffer

   s_RXMaxBlocksToBuffer = 24;

   s_iMilisecondsMaxRetransmissionWindow = ((s_VDStatsCache.encoding_extra_flags & 0xFF00) >> 8) * 5;
   log_line("Need buffers to cache %d miliseconds of video", s_iMilisecondsMaxRetransmissionWindow);
   s_RetransmissionRetryTimeout = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log_line("Video RX: Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", s_RetransmissionRetryTimeout, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);

   _rx_video_log_line("Need buffers to cache %d miliseconds of video", s_iMilisecondsMaxRetransmissionWindow);
   _rx_video_log_line("Video RX: Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", s_RetransmissionRetryTimeout, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   
   u32 videoBitrate = DEFAULT_VIDEO_BITRATE;
   if ( NULL != g_pCurrentModel )
      videoBitrate = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps;

   log_line("Curent video bitrate: %u bps; current data/block: %d, ec/block: %d, packet length: %d", videoBitrate, s_VDStatsCache.data_packets_per_block, s_VDStatsCache.fec_packets_per_block, s_VDStatsCache.video_packet_length);
   _rx_video_log_line("Curent video bitrate: %u bps; current data packets/block: %d, packet length: %d", videoBitrate, s_VDStatsCache.data_packets_per_block, s_VDStatsCache.video_packet_length);
   log_line("Curent blocks/sec: about %d blocks", videoBitrate/s_VDStatsCache.data_packets_per_block/s_VDStatsCache.video_packet_length/8);
   _rx_video_log_line("Curent blocks/sec: about %d blocks", videoBitrate/s_VDStatsCache.data_packets_per_block/s_VDStatsCache.video_packet_length/8);
   
   float miliPerBlock = (float)((u32)(1000 * 8 * (u32)s_VDStatsCache.video_packet_length * (u32)s_VDStatsCache.data_packets_per_block)) / (float)videoBitrate;

   u32 bytesToBuffer = (((float)videoBitrate / 1000.0) * s_iMilisecondsMaxRetransmissionWindow)/8.0;
   log_line("Need buffers to cache %u bits of video (%u bytes of video)", bytesToBuffer*8, bytesToBuffer);
   _rx_video_log_line("Need buffers to cache %u bits of video (%u bytes of video)", bytesToBuffer*8, bytesToBuffer);

   if ( 0 != bytesToBuffer && 0 != s_VDStatsCache.video_packet_length && 0 != s_VDStatsCache.data_packets_per_block )
      s_RXMaxBlocksToBuffer = 2 + bytesToBuffer/s_VDStatsCache.video_packet_length/s_VDStatsCache.data_packets_per_block;
   else
      s_RXMaxBlocksToBuffer = 5;

   // Add extra time for last retransmissions (50 milisec)
   s_RXMaxBlocksToBuffer += 50/miliPerBlock;

   s_RXMaxBlocksToBuffer *= 2.5;

   log_line("Computed result: need to cache %d video blocks for %d miliseconds of video; one block stores %.1f miliseconds of video", s_RXMaxBlocksToBuffer, s_iMilisecondsMaxRetransmissionWindow, miliPerBlock);
   _rx_video_log_line("Computed result: need to cache %d video blocks for %d miliseconds of video; one block stores %.1f miliseconds of video", s_RXMaxBlocksToBuffer, s_iMilisecondsMaxRetransmissionWindow, miliPerBlock);
   if ( s_RXMaxBlocksToBuffer >= MAX_RXTX_BLOCKS_BUFFER )
   {
      s_RXMaxBlocksToBuffer = MAX_RXTX_BLOCKS_BUFFER-1;
      log_line("Capped video Rx cache to %d blocks", s_RXMaxBlocksToBuffer);
      _rx_video_log_line("Capped video Rx cache to %d blocks", s_RXMaxBlocksToBuffer);
   }
   log_line("Max blocks that can be stored in video RX buffers: %u, for a total of %d ms of video", MAX_RXTX_BLOCKS_BUFFER, (int)(MAX_RXTX_BLOCKS_BUFFER*miliPerBlock));
   _rx_video_log_line("Max blocks that can be stored in video RX buffers: %u, for a total of %d ms of video", MAX_RXTX_BLOCKS_BUFFER, (int)(MAX_RXTX_BLOCKS_BUFFER*miliPerBlock));

   _rx_video_reset_receive_buffers();

   // Reset packets statistics

   s_VDStatsCache.maxBlocksAllowedInBuffers = s_RXMaxBlocksToBuffer;
   s_VDStatsCache.maxPacketsInBuffers = 20;
   s_VDStatsCache.currentPacketsInBuffers = 0;
   s_VDStatsCache.maxPacketsInBuffers = 10;
   s_VDStatsCache.total_DiscardedSegments = 0;

   g_VideoDecodeStatsHistory.totalCurrentlyMissingPackets = 0;
   for( int i=0; i<MAX_HISTORY_VIDEO_INTERVALS; i++ )
   {
      g_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[i] = 0;
      g_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[i] = 0;
      g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[i] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[i] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[i] = 0;
      g_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[i] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksBadPerPeriod[i] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[i] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksRetrasmitedPerPeriod[i] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[i] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[i] = 0;
      g_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[i] = 0;
   }

   memcpy((u8*)s_pSM_VideoDecodeStats, (u8*)(&s_VDStatsCache), sizeof(shared_mem_video_decode_stats));
   memcpy((u8*)s_pSM_VideoDecodeStatsHistory, (u8*)(&g_VideoDecodeStatsHistory), sizeof(shared_mem_video_decode_stats_history));

   s_TimeLastVideoStatsUpdate = g_TimeNow-1000;
   s_TimeLastHistoryStatsUpdate = g_TimeNow;

   s_LastHardEncodingsChangeVideoBlockIndex = MAX_U32;
   s_LastVideoResolutionChangeVideoBlockIndex = MAX_U32;
   s_uEncodingsChangeCount = 0;
   s_uLastReceivedVideoLinkProfile = 0;
   s_uTimeIntervalMsForRequestingRetransmissions = 10;

   _rx_video_log_line("Reset video RX state and buffers done.");
   _rx_video_log_line("--------------------------------------");
   _rx_video_log_line("");
}

void _update_history_discared_all_stack()
{
   if ( s_RXBlocksStackTopIndex < 0 )
      return;
   _rx_video_log_line("");
   _rx_video_log_line("-------------------------------------------------------------------");
   _rx_video_log_line("No. %d Discared full stack [0-%d] (%u-%u), last updated: (%02d:%02d.%03d - %02d:%02d.%03d) length: %d ms", s_VDStatsCache.total_DiscardedSegments, s_RXBlocksStackTopIndex, s_pRXBlocksStack[0]->video_block_index, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->video_block_index, s_pRXBlocksStack[0]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[0]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[0]->uTimeLastUpdated%1000, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated%1000, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated-s_pRXBlocksStack[0]->uTimeLastUpdated);
   _rx_video_log_line("  * Last output video block: %u at %02d:%02d.%03d (%d ms ago)", s_LastOutputVideoBlockIndex, s_LastOutputVideoBlockTime/1000/60, (s_LastOutputVideoBlockTime/1000)%60, s_LastOutputVideoBlockTime%1000, g_TimeNow - s_LastOutputVideoBlockTime);
   _rx_video_log_line("  * Last received video block: %u at %02d:%02d.%03d (%d ms ago)", s_LastReceivedVideoPacketInfo.video_block_index, s_LastReceivedVideoPacketInfo.receive_time/1000/60, (s_LastReceivedVideoPacketInfo.receive_time/1000)%60, s_LastReceivedVideoPacketInfo.receive_time%1000, g_TimeNow - s_LastReceivedVideoPacketInfo.receive_time);
   _rx_video_log_line("  * Last updated: from %d ms to %d ms ago", g_TimeNow - s_pRXBlocksStack[0]->uTimeLastUpdated, g_TimeNow - s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated);
   _rx_video_log_line("-------------------------------------------------------------------");

   u32 timeMin = s_pRXBlocksStack[0]->uTimeLastUpdated;
   u32 timeMax = s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated;

   int indexMin = (g_TimeNow - timeMin)/g_VideoDecodeStatsHistory.outputHistoryIntervalMs;
   int indexMax = (g_TimeNow - timeMax)/g_VideoDecodeStatsHistory.outputHistoryIntervalMs;
   if ( indexMin < 0 ) indexMin = 0;
   if ( indexMin >= MAX_HISTORY_VIDEO_INTERVALS ) indexMin = MAX_HISTORY_VIDEO_INTERVALS-1;
   if ( indexMax < 0 ) indexMax = 0;
   if ( indexMax >= MAX_HISTORY_VIDEO_INTERVALS ) indexMax = MAX_HISTORY_VIDEO_INTERVALS-1;

   for( int i=indexMin; i<=indexMax; i++ )
      g_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[i]++;

   g_VideoDecodeStatsHistory.totalCurrentlyMissingPackets = 0;
}

void _update_history_discared_stack_segment(int countDiscardedBlocks)
{
   if ( countDiscardedBlocks <= 0 )
      return;

   _rx_video_log_line("");
   _rx_video_log_line("-------------------------------------------------------------------");
   _rx_video_log_line("No. %d Discared stack segment [0-%d] (%u-%u), last updated: (%02d:%02d.%03d - %02d:%02d.%03d) length: %d ms", s_VDStatsCache.total_DiscardedSegments, countDiscardedBlocks-1, s_pRXBlocksStack[0]->video_block_index, s_pRXBlocksStack[countDiscardedBlocks-1]->video_block_index, s_pRXBlocksStack[0]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[0]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[0]->uTimeLastUpdated%1000, s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated%1000, s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated-s_pRXBlocksStack[0]->uTimeLastUpdated);
   _rx_video_log_line("  * Stack top is at %d: %u (max allowed stack is %d blocks), received %d ms ago, retransmission timeout is: %d ms", s_RXBlocksStackTopIndex, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->video_block_index, s_VDStatsCache.maxBlocksAllowedInBuffers, g_TimeNow - s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated, s_RetransmissionRetryTimeout);
   _rx_video_log_line("  * Last output video block: %u at %02d:%02d.%03d (%d ms ago)", s_LastOutputVideoBlockIndex, s_LastOutputVideoBlockTime/1000/60, (s_LastOutputVideoBlockTime/1000)%60, s_LastOutputVideoBlockTime%1000, g_TimeNow - s_LastOutputVideoBlockTime);
   _rx_video_log_line("  * Last received video block: %u at %02d:%02d.%03d (%d ms ago)", s_LastReceivedVideoPacketInfo.video_block_index, s_LastReceivedVideoPacketInfo.receive_time/1000/60, (s_LastReceivedVideoPacketInfo.receive_time/1000)%60, s_LastReceivedVideoPacketInfo.receive_time%1000, g_TimeNow - s_LastReceivedVideoPacketInfo.receive_time);
   _rx_video_log_line("  * Last updated: from %d ms to %d ms ago", g_TimeNow - s_pRXBlocksStack[0]->uTimeLastUpdated, g_TimeNow - s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated);

   // Detect the time interval we are discarding

   u32 timeMin = s_pRXBlocksStack[0]->uTimeLastUpdated;
   u32 timeMax = s_pRXBlocksStack[countDiscardedBlocks-1]->uTimeLastUpdated;

   int indexMin = (g_TimeNow - timeMin)/g_VideoDecodeStatsHistory.outputHistoryIntervalMs;
   int indexMax = (g_TimeNow - timeMax)/g_VideoDecodeStatsHistory.outputHistoryIntervalMs;
   if ( indexMin < 0 ) indexMin = 0;
   if ( indexMin >= MAX_HISTORY_VIDEO_INTERVALS ) indexMin = MAX_HISTORY_VIDEO_INTERVALS-1;
   if ( indexMax < 0 ) indexMax = 0;
   if ( indexMax >= MAX_HISTORY_VIDEO_INTERVALS ) indexMax = MAX_HISTORY_VIDEO_INTERVALS-1;

   //for( int i=indexMin; i<=indexMax; i++ )
   //   g_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[i]++;

   for( int i=0; i<countDiscardedBlocks; i++ )
   {
      if ( s_pRXBlocksStack[i]->received_data_packets + s_pRXBlocksStack[i]->received_fec_packets < s_pRXBlocksStack[i]->data_packets )
      {
         //_rx_video_log_line("  * Unrecoverable block %d: %u [received: %d/%d], last updated time: %02d:%02d.%03d (%d ms ago)", i, s_pRXBlocksStack[i]->video_block_index, s_pRXBlocksStack[i]->received_data_packets, s_pRXBlocksStack[i]->received_fec_packets, s_pRXBlocksStack[i]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[i]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[i]->uTimeLastUpdated%1000, g_TimeNow - s_pRXBlocksStack[i]->uTimeLastUpdated);
         for( int k=0; k<s_pRXBlocksStack[i]->data_packets+s_pRXBlocksStack[i]->fec_packets; k++ )
            if ( s_pRXBlocksStack[i]->packetsInfo[k].state != RX_PACKET_STATE_RECEIVED )
            {
               if ( s_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount > 0 )
                  _rx_video_log_line("      - missing packet %d: retry count: %d, first retry: %d ms ago, last retry: %d ms ago", k, s_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount, g_TimeNow - s_pRXBlocksStack[i]->packetsInfo[k].uTimeFirstRetrySent, g_TimeNow - s_pRXBlocksStack[i]->packetsInfo[k].uTimeLastRetrySent);
               else
                  _rx_video_log_line("      - missing packet %d: retry count: 0", k);
            }

         u32 time = s_pRXBlocksStack[i]->uTimeLastUpdated;
         int index = (g_TimeNow - time)/g_VideoDecodeStatsHistory.outputHistoryIntervalMs;
         if ( index < 0 )
            index = 0;
         if ( index >= MAX_HISTORY_VIDEO_INTERVALS )
            index = MAX_HISTORY_VIDEO_INTERVALS-1;
         g_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[index]++;
      }
      else
      {
         if ( s_pRXBlocksStack[i]->received_data_packets >= s_pRXBlocksStack[i]->data_packets )
            g_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[0]++;
         else
            g_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[0]++;
         //_rx_video_log_line("  * Usable block %d: %u [received %d/%d], last updated time: %02d:%02d.%03d (%d ms ago)", i, s_pRXBlocksStack[i]->video_block_index, s_pRXBlocksStack[i]->received_data_packets, s_pRXBlocksStack[i]->received_fec_packets, s_pRXBlocksStack[i]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[i]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[i]->uTimeLastUpdated%1000, g_TimeNow - s_pRXBlocksStack[i]->uTimeLastUpdated);
      }
   }

   if ( countDiscardedBlocks <= s_RXBlocksStackTopIndex )
   {
      if ( s_pRXBlocksStack[countDiscardedBlocks]->received_data_packets + s_pRXBlocksStack[countDiscardedBlocks]->received_fec_packets < s_pRXBlocksStack[countDiscardedBlocks]->data_packets )
      {
         _rx_video_log_line("  * Next block in the RX buffer %d: %u [received: %d/%d], last updated time: %02d:%02d.%03d (%d ms ago)", countDiscardedBlocks, s_pRXBlocksStack[countDiscardedBlocks]->video_block_index, s_pRXBlocksStack[countDiscardedBlocks]->received_data_packets, s_pRXBlocksStack[countDiscardedBlocks]->received_fec_packets, s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated%1000, g_TimeNow - s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated);
         for( int k=0; k<s_pRXBlocksStack[countDiscardedBlocks]->data_packets+s_pRXBlocksStack[countDiscardedBlocks]->fec_packets; k++ )
            if ( s_pRXBlocksStack[countDiscardedBlocks]->packetsInfo[k].state != RX_PACKET_STATE_RECEIVED )
            {
               if ( s_pRXBlocksStack[countDiscardedBlocks]->packetsInfo[k].uRetrySentCount > 0 )
                  _rx_video_log_line("      - missing packet %d: retry count: %d, first retry: %d ms ago, last retry: %d ms ago", k, s_pRXBlocksStack[countDiscardedBlocks]->packetsInfo[k].uRetrySentCount, g_TimeNow - s_pRXBlocksStack[countDiscardedBlocks]->packetsInfo[k].uTimeFirstRetrySent, g_TimeNow - s_pRXBlocksStack[countDiscardedBlocks]->packetsInfo[k].uTimeLastRetrySent);
               else
                  _rx_video_log_line("      - missing packet %d: retry count: 0", k);
            }
      }
      else
      {
         _rx_video_log_line("  * Next block in the RX buffer %d: %u [received %d/%d], last updated time: %02d:%02d.%03d (%d ms ago)", countDiscardedBlocks, s_pRXBlocksStack[countDiscardedBlocks]->video_block_index, s_pRXBlocksStack[countDiscardedBlocks]->received_data_packets, s_pRXBlocksStack[countDiscardedBlocks]->received_fec_packets, s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated%1000, g_TimeNow - s_pRXBlocksStack[countDiscardedBlocks]->uTimeLastUpdated);
      }
   }
   else
      _rx_video_log_line("  * Nothing else left in the RX buffers after this.");

   _rx_video_log_line("-------------------------------------------------------------------");
}

void _update_history_blocks_output(int rx_buffer_block_index, bool hasRetransmittedPackets )
{
   u32 video_block_index = s_pRXBlocksStack[rx_buffer_block_index]->video_block_index;
   if ( MAX_U32 == video_block_index )
      return;

   if ( hasRetransmittedPackets )
      g_VideoDecodeStatsHistory.outputHistoryBlocksRetrasmitedPerPeriod[0]++;


   if ( s_pRXBlocksStack[rx_buffer_block_index]->received_data_packets >= s_pRXBlocksStack[rx_buffer_block_index]->data_packets )
      g_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[0]++;
   else if ( s_pRXBlocksStack[rx_buffer_block_index]->received_data_packets + s_pRXBlocksStack[rx_buffer_block_index]->received_fec_packets >= s_pRXBlocksStack[rx_buffer_block_index]->data_packets )
   {
      g_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[0]++;
      int ecUsed = s_pRXBlocksStack[rx_buffer_block_index]->data_packets - s_pRXBlocksStack[rx_buffer_block_index]->received_data_packets;
      if ( ecUsed > g_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[0] )
         g_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[0] = ecUsed;
   }
   else if ( s_pRXBlocksStack[rx_buffer_block_index]->received_data_packets + s_pRXBlocksStack[rx_buffer_block_index]->received_fec_packets > 0 )
   {
      g_VideoDecodeStatsHistory.outputHistoryBlocksBadPerPeriod[0]++;
      //log_line("Bad block out");
   }
   if ( video_block_index > s_LastOutputVideoBlockIndex+1 )
   {
      u32 diff = video_block_index - s_LastOutputVideoBlockIndex;
      if ( diff + g_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[0] > 255 )
         g_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[0] = 255;
      else
         g_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[0] += diff;
   }
}

void _reconstruct_block(int rx_buffer_block_index)
{

   if ( g_PD_ControllerLinkStats.tmp_video_streams_blocks_reconstructed[0] < 254 )
      g_PD_ControllerLinkStats.tmp_video_streams_blocks_reconstructed[0]++;
   else
      g_PD_ControllerLinkStats.tmp_video_streams_blocks_reconstructed[0] = 254;

   // Add existing data packets, mark and count the ones that are missing

   s_FECInfo.missing_packets_count = 0;
   for( int i=0; i<s_pRXBlocksStack[rx_buffer_block_index]->data_packets; i++ )
   {
      s_FECInfo.fec_decode_data_packets_pointers[i] = s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[i].pData;
      if ( s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[i].state != RX_PACKET_STATE_RECEIVED )
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
   for( int i=0; i<s_pRXBlocksStack[rx_buffer_block_index]->fec_packets; i++ )
   {
      if ( s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[i+s_pRXBlocksStack[rx_buffer_block_index]->data_packets].state == RX_PACKET_STATE_RECEIVED)
      {
         s_FECInfo.fec_decode_fec_packets_pointers[pos] = s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[i+s_pRXBlocksStack[rx_buffer_block_index]->data_packets].pData;
         s_FECInfo.fec_decode_fec_indexes[pos] = i;
         pos++;
         if ( pos == s_FECInfo.missing_packets_count )
            break;
      }
   }

   //if ( pos != s_FECInfo.missing_packets_count )
   //   log_line("DEBUG !!!!!!!!!!!!!!!!!!!!!! INVALID RECONSTRUCTION");

   //log_line("DEBUG recontruct, packet length: %d, scheme data/fec: %d/%d, missing: %d", s_pRXBlocksStack[rx_buffer_block_index]->packet_length, s_pRXBlocksStack[rx_buffer_block_index]->data_packets, s_pRXBlocksStack[rx_buffer_block_index]->fec_packets, s_FECInfo.missing_packets_count );


   fec_decode(s_pRXBlocksStack[rx_buffer_block_index]->packet_length, s_FECInfo.fec_decode_data_packets_pointers, s_pRXBlocksStack[rx_buffer_block_index]->data_packets, s_FECInfo.fec_decode_fec_packets_pointers, s_FECInfo.fec_decode_fec_indexes, s_FECInfo.fec_decode_missing_packets_indexes, s_FECInfo.missing_packets_count );
         
   // Mark all data packets reconstructed as received, set the right data in them
   for( u32 i=0; i<s_FECInfo.missing_packets_count; i++ )
   {
      s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[s_FECInfo.fec_decode_missing_packets_indexes[i]].state = RX_PACKET_STATE_RECEIVED;
      s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[s_FECInfo.fec_decode_missing_packets_indexes[i]].packet_length = s_pRXBlocksStack[rx_buffer_block_index]->packet_length;
      s_pRXBlocksStack[rx_buffer_block_index]->received_data_packets++;

      if ( s_VDStatsCache.currentPacketsInBuffers > s_VDStatsCache.maxPacketsInBuffers )
         s_VDStatsCache.maxPacketsInBuffers = s_VDStatsCache.currentPacketsInBuffers;
   }
  //_rx_video_log_line("Reconstructed block %u, had %d missing packets", s_pRXBlocksStack[rx_buffer_block_index]->video_block_index, s_FECInfo.missing_packets_count);

}

void _send_packet_to_output(int rx_buffer_block_index, int block_packet_index )
{
   u32 video_block_index = s_pRXBlocksStack[rx_buffer_block_index]->video_block_index;

   if ( MAX_U32 == video_block_index || 0 == s_pRXBlocksStack[rx_buffer_block_index]->data_packets )
      return;

   if ( s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[block_packet_index].state != RX_PACKET_STATE_RECEIVED )
   {
      s_VDStatsCache.total_DiscardedLostPackets++;
      return;
   }

   if ( g_bDebug )
      return;

   u8* pBuffer = s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[block_packet_index].pData;
   int length = s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[block_packet_index].packet_length;

   //_rx_video_log_line("Output block %u/%d", s_pRXBlocksStack[rx_buffer_block_index]->video_block_index,block_packet_index);
   if ( length <= 0 || length > 1300 )
   {
      //log_line("DEBUG will send an invalid size packet to output: %d bytes, [%u/%d], last video block output before this one: %u", length, s_pRXBlocksStack[rx_buffer_block_index]->video_block_index,block_packet_index, s_LastOutputVideoBlockIndex);
   }
   processor_rx_video_forward_video_data(pBuffer, length);
}

void shift_blocks_buffer()
{
   if ( s_RXBlocksStackTopIndex < 0 )
      return;

   _rx_video_reset_receive_buffer_block(0);

   type_received_block_info* pFirstBlock = s_pRXBlocksStack[0];
   for( int i=0; i<s_RXBlocksStackTopIndex; i++ )
      s_pRXBlocksStack[i] = s_pRXBlocksStack[i+1];
   s_pRXBlocksStack[s_RXBlocksStackTopIndex] = pFirstBlock;

   s_RXBlocksStackTopIndex--;
}

void _push_first_block_out()
{
   #ifdef PROFILE_RX
   u32 uTimeStart = get_current_timestamp_ms();
   #endif

   // If we have all the data packets or
   // If no recontruction is possible, just output valid data

   int countRetransmittedPackets = 0;
   for( int i=0; i<s_pRXBlocksStack[0]->data_packets; i++ )
      if ( s_pRXBlocksStack[0]->packetsInfo[i].uRetrySentCount > 0 )
      if ( s_pRXBlocksStack[0]->packetsInfo[i].state == RX_PACKET_STATE_RECEIVED )
         countRetransmittedPackets++;

   _update_history_blocks_output(0, 0 != countRetransmittedPackets);

   s_VDStatsCache.currentPacketsInBuffers -= s_pRXBlocksStack[0]->received_data_packets;
   s_VDStatsCache.currentPacketsInBuffers -= s_pRXBlocksStack[0]->received_fec_packets;
   if ( s_VDStatsCache.currentPacketsInBuffers < 0 )
      s_VDStatsCache.currentPacketsInBuffers = 0;

   //log_line("DEBUG Push block out");
   //_log_current_buffer();

   // Do reconstruction (we have enough data for doing it)

   if ( s_pRXBlocksStack[0]->received_data_packets < s_pRXBlocksStack[0]->data_packets )
   {
      if ( s_pRXBlocksStack[0]->received_data_packets + s_pRXBlocksStack[0]->received_fec_packets >= s_pRXBlocksStack[0]->data_packets )
      {
         g_ControllerAdaptiveVideoInfo.uIntervalsOuputRecontructedVideoPackets[g_ControllerAdaptiveVideoInfo.uCurrentIntervalIndex]++;  
         _reconstruct_block(0);
         //log_line("DEBUG Reconstructed block, video block index: %u", s_pRXBlocksStack[0]->video_block_index);
         //_log_current_buffer();
      }
      else
         _rx_video_log_line("Can't reconstruct block %u, has only %d packets of minimum %d required.", s_pRXBlocksStack[0]->video_block_index, s_pRXBlocksStack[0]->received_data_packets + s_pRXBlocksStack[0]->received_fec_packets, s_pRXBlocksStack[0]->data_packets);
   
   }
   else
   {
      if ( g_PD_ControllerLinkStats.tmp_video_streams_blocks_clean[0] < 254 )
         g_PD_ControllerLinkStats.tmp_video_streams_blocks_clean[0]++;
      else
         g_PD_ControllerLinkStats.tmp_video_streams_blocks_clean[0] = 254;

      g_ControllerAdaptiveVideoInfo.uIntervalsOuputCleanVideoPackets[g_ControllerAdaptiveVideoInfo.uCurrentIntervalIndex]++;

   }

   #ifdef PROFILE_RX
   u32 dTime1 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime1 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Pushing first video block out (history update and reconstruction) took too long: %u ms.", dTime1);
   #endif

   // Output the block

   if ( s_pRXBlocksStack[0]->received_data_packets + s_pRXBlocksStack[0]->received_fec_packets >= s_pRXBlocksStack[0]->data_packets )
   if ( g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0] > 0 )
      g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0]--;

   for( int i=0; i<s_pRXBlocksStack[0]->data_packets; i++ )
      _send_packet_to_output(0, i);

   //if ( s_LastOutputVideoBlockIndex != MAX_U32 )
   //if ( s_pRXBlocksStack[0]->video_block_index != s_LastOutputVideoBlockIndex + 1 )
   //   log_line("DEBUG: output block skipped");

   s_LastOutputVideoBlockIndex = s_pRXBlocksStack[0]->video_block_index;
   s_LastOutputVideoBlockTime = g_TimeNow;

   shift_blocks_buffer();

   #ifdef PROFILE_RX
   u32 dTime2 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime2 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Pushing first video block out (to player) took too long: %u ms.", dTime2);
   #endif

}

// Discard blocks, do not output them (unless blocks are good)

void _push_incomplete_blocks_out(int countToPush, bool bReasonTooOld)
{
   if ( countToPush <= 0 )
      return;

   #ifdef PROFILE_RX
   u32 uTimeStart = get_current_timestamp_ms();
   #endif

   //log_line("DEBUG: pushed incomplete blocks out: %d", countToPush);

   s_LastOutputVideoBlockTime = g_TimeNow;

   if ( s_LastOutputVideoBlockIndex != MAX_U32 )
      s_LastOutputVideoBlockIndex += (u32) countToPush;
   else
      s_LastOutputVideoBlockIndex = s_pRXBlocksStack[0]->video_block_index + (u32) countToPush;

   bool bFullDiscard = false;
   if ( countToPush >= s_RXBlocksStackTopIndex + 1 )
   {
      countToPush = s_RXBlocksStackTopIndex+1;
      bFullDiscard = true;
      if ( bReasonTooOld )
         _rx_video_log_line("Discarding full Rx stack [0-%d] (too old, newest data in discarded segment was at %02d:%02d.%03d)", s_RXBlocksStackTopIndex, s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated % 1000);
      else
         _rx_video_log_line("Discarding full Rx stack [0-%d] (to make room for newer blocks, discarded blocks indexes [%u-%u], latest received block index: %u", s_RXBlocksStackTopIndex, s_pRXBlocksStack[0]->video_block_index, s_pRXBlocksStack[s_RXBlocksStackTopIndex]->video_block_index, s_LastReceivedVideoPacketInfo.video_block_index);

      g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0] = 0;
   }
   else
   {
      if ( bReasonTooOld )
         _rx_video_log_line("Discarding Rx stack segment [0-%d] of total [0-%d] (too old, newest data in discarded segment was at %02d:%02d.%03d)", countToPush-1, s_RXBlocksStackTopIndex, s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated/1000/60, (s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated/1000)%60, s_pRXBlocksStack[countToPush-1]->uTimeLastUpdated % 1000);
      else
         _rx_video_log_line("Discarding Rx stack segment [0-%d] (to make room for newer blocks, discarded blocks indexes [%u-%u], latest received block index: %u", countToPush-1, s_pRXBlocksStack[0]->video_block_index, s_pRXBlocksStack[countToPush-1]->video_block_index, s_LastReceivedVideoPacketInfo.video_block_index);
   }

   for( int i=0; i<countToPush; i++ )
   {
      s_VDStatsCache.currentPacketsInBuffers -= s_pRXBlocksStack[i]->received_data_packets;
      s_VDStatsCache.currentPacketsInBuffers -= s_pRXBlocksStack[i]->received_fec_packets;
      if ( s_VDStatsCache.currentPacketsInBuffers < 0 )
         s_VDStatsCache.currentPacketsInBuffers = 0;

      if ( s_pRXBlocksStack[i]->received_data_packets + s_pRXBlocksStack[i]->received_fec_packets >= s_pRXBlocksStack[i]->data_packets )
      if ( g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0] > 0 )
         g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0]--;

      // Do reconstruction if we have enough data for doing it;

      if ( s_pRXBlocksStack[i]->received_data_packets >= s_pRXBlocksStack[i]->data_packets )
         g_ControllerAdaptiveVideoInfo.uIntervalsOuputCleanVideoPackets[g_ControllerAdaptiveVideoInfo.uCurrentIntervalIndex]++;  

      if ( (s_pRXBlocksStack[i]->received_data_packets < s_pRXBlocksStack[i]->data_packets) &&
           (s_pRXBlocksStack[i]->received_data_packets + s_pRXBlocksStack[i]->received_fec_packets >= s_pRXBlocksStack[i]->data_packets) )
      {
         _reconstruct_block(i);
         g_ControllerAdaptiveVideoInfo.uIntervalsOuputRecontructedVideoPackets[g_ControllerAdaptiveVideoInfo.uCurrentIntervalIndex]++;  
      }

      if ( s_pRXBlocksStack[i]->received_data_packets >= s_pRXBlocksStack[i]->data_packets )
      {
         for( int k=0; k<s_pRXBlocksStack[i]->data_packets; k++ )
            _send_packet_to_output(i, k);
      }
      else
         s_VDStatsCache.total_DiscardedLostPackets += s_pRXBlocksStack[i]->data_packets-s_pRXBlocksStack[i]->received_data_packets;

      s_pRXBlocksStack[i]->data_packets = MAX_TOTAL_PACKETS_IN_BLOCK;
      s_pRXBlocksStack[i]->fec_packets = 0;
      _rx_video_reset_receive_buffer_block(i);
   }
         
   type_received_block_info* tmpBlocksStack[MAX_RXTX_BLOCKS_BUFFER];
   memcpy((u8*)&(tmpBlocksStack[0]), (u8*)&(s_pRXBlocksStack[0]), (s_RXBlocksStackTopIndex+1)*sizeof(type_received_block_info*));
   for( int i=0; i<countToPush; i++ )
      s_pRXBlocksStack[s_RXBlocksStackTopIndex-countToPush+1+i] = tmpBlocksStack[i];
   for( int i=countToPush; i<=s_RXBlocksStackTopIndex; i++ )
      s_pRXBlocksStack[i-countToPush] = tmpBlocksStack[i];

   s_RXBlocksStackTopIndex -= countToPush;
   s_VDStatsCache.total_DiscardedSegments++;

   if ( bFullDiscard )
      s_RXBlocksStackTopIndex = -1;
   _rx_video_log_line("Discarded all to video block index %u. Now stack top is at %d, total discarded segments: %d", s_LastOutputVideoBlockIndex, s_RXBlocksStackTopIndex, s_VDStatsCache.total_DiscardedSegments);

   #ifdef PROFILE_RX
   u32 dTime1 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime1 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Pushing incomplete video blocks out (%d blocks)took too long: %u ms.",  countToPush, dTime1);
   #endif
}

void _check_and_request_missing_packets()
{
   if ( g_bSearching || NULL == g_pCurrentModel || g_bUpdateInProgress )
      return;
   if ( g_pCurrentModel->is_spectator )
      return;

   if ( g_TimeNow < s_LastTimeRequestedRetransmissions + s_uTimeIntervalMsForRequestingRetransmissions )
      return;

   if ( s_uTimeIntervalMsForRequestingRetransmissions < 20 )
      s_uTimeIntervalMsForRequestingRetransmissions++;
   if ( s_LastTimeRequestedRetransmissions < g_TimeNow-400 )
      s_uTimeIntervalMsForRequestingRetransmissions = 10;

   // If link is lost, do not request retransmissions

   if ( s_LastReceivedVideoPacketInfo.receive_time != 0 )
   {
      if ( 0 != g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds )
      {
         if ( g_TimeNow > s_LastReceivedVideoPacketInfo.receive_time + g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds )
            return;
      }
      else if ( g_TimeNow > s_LastReceivedVideoPacketInfo.receive_time + 1000 )
            return;
   }

   if ( NULL == g_pCurrentModel || g_pCurrentModel->is_spectator || g_bSearching )
      return;
   if ( ! (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS) )
      return;
   if ( -1 == s_RXBlocksStackTopIndex )
      return;

   bool bUseNewVersion = false;
   if ( ((g_pCurrentModel->sw_version>>8) & 0xFF) > 6 )
      bUseNewVersion = true;
   if ( ((g_pCurrentModel->sw_version>>8) & 0xFF) == 6 )
   if ( ((g_pCurrentModel->sw_version & 0xFF) == 9) || ((g_pCurrentModel->sw_version & 0xFF) >= 90 ) )
      bUseNewVersion = true;

   //log_line("DEBUG vehicle sw: %d.%d", ((g_pCurrentModel->sw_version>>8) & 0xFF), (g_pCurrentModel->sw_version & 0xFF));
   
   u8 buffer[1200];
   u8* pBuffer = NULL;
   if ( bUseNewVersion )
      pBuffer = &buffer[6];
   else
      pBuffer = &buffer[2];

   // Request missing packets from the first block in stack up to the last one (including the last one)
   // Max requested packets count is limited to 200, due to max radio packet size

   int totalCountRequested = 0;
   int totalCountRequestedNew = 0;
   int totalCountReRequested = 0;

   for( int i=0; i<=s_RXBlocksStackTopIndex; i++ )
   {
      // Request maximum 30 packets at each retransmission request, in order to allow the vehicle some time to send them back
      if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
         break;

      if ( s_pRXBlocksStack[i]->data_packets == 0 )
         continue;
      if ( s_pRXBlocksStack[i]->received_data_packets + s_pRXBlocksStack[i]->received_fec_packets >= s_pRXBlocksStack[i]->data_packets )
         continue;
      
      int countToRequestForBlock = 0;

      // For last block, request only missing packets up untill the last received data packet in the block
      if ( i == s_RXBlocksStackTopIndex )
      {
         if ( 0 != g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs )
         if ( s_pRXBlocksStack[i]->uTimeLastUpdated < g_TimeNow - g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs )
         {
            countToRequestForBlock = s_pRXBlocksStack[i]->data_packets;
            for(int k=s_pRXBlocksStack[i]->data_packets-1; k>=0; k-- )
            {
               countToRequestForBlock--;
               if ( s_pRXBlocksStack[i]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
                  break;
            }
            if ( s_pRXBlocksStack[i]->received_data_packets > 0 )
               countToRequestForBlock -= s_pRXBlocksStack[i]->received_data_packets-1;
         }
      }
      else
         countToRequestForBlock = s_pRXBlocksStack[i]->data_packets - s_pRXBlocksStack[i]->received_data_packets - s_pRXBlocksStack[i]->received_fec_packets;

      if ( countToRequestForBlock <= 0 )
         continue;

      // First, re-request the packets we already requested once.
      // Then, request additional packets if needed (not enough requested for possible reconstruction)
      // Then, request some EC packets (half the original EC rate) proportional to missing packets count
      
      if ( s_pRXBlocksStack[i]->totalPacketsRequested > 0 )
      {
         for( int k=0; k<s_pRXBlocksStack[i]->data_packets; k++ )
         {
            if ( s_pRXBlocksStack[i]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
               continue;
            if ( s_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount == 0 )
               continue;
            if ( s_pRXBlocksStack[i]->packetsInfo[k].uTimeLastRetrySent >= g_TimeNow-s_RetransmissionRetryTimeout )
               continue;

            s_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount++;
            s_pRXBlocksStack[i]->uTimeLastRetrySent = g_TimeNow;
            s_pRXBlocksStack[i]->packetsInfo[k].uTimeLastRetrySent = g_TimeNow;

            // Decrease interval of future retransmissions requests for this packet
            u32 dt = 5 * s_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount;
            if ( dt > s_RetransmissionRetryTimeout-10 )
               dt = s_RetransmissionRetryTimeout-10;
            s_pRXBlocksStack[i]->packetsInfo[k].uTimeLastRetrySent -= dt;

            memcpy(pBuffer, &(s_pRXBlocksStack[i]->video_block_index), sizeof(u32));
            pBuffer += sizeof(u32);
            *pBuffer = (u8)k;
            pBuffer++;
            *pBuffer = (u8)(s_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount);
            pBuffer++;
            totalCountRequested++;
            totalCountReRequested++;

            if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
               break;
         }
      }

      if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
         break;

      countToRequestForBlock -= s_pRXBlocksStack[i]->totalPacketsRequested;

      // Request additional packets from the block if not enough for possible reconstruction

      if ( s_pRXBlocksStack[i]->data_packets - s_pRXBlocksStack[i]->received_data_packets - s_pRXBlocksStack[i]->received_fec_packets - s_pRXBlocksStack[i]->totalPacketsRequested > 0 )
      {
         for( int k=0; k<s_pRXBlocksStack[i]->data_packets; k++ )
         {
            if ( s_pRXBlocksStack[i]->packetsInfo[k].state == RX_PACKET_STATE_RECEIVED )
               continue;
            if ( s_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount != 0 )
               continue;

            s_pRXBlocksStack[i]->packetsInfo[k].uTimeFirstRetrySent = g_TimeNow;
            s_pRXBlocksStack[i]->packetsInfo[k].uTimeLastRetrySent = g_TimeNow;
            s_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount = 1;
            totalCountRequestedNew++;
            s_pRXBlocksStack[i]->totalPacketsRequested++;

            if ( 0 == s_pRXBlocksStack[i]->uTimeFirstRetrySent )
               s_pRXBlocksStack[i]->uTimeFirstRetrySent = g_TimeNow;
            s_pRXBlocksStack[i]->uTimeLastRetrySent = g_TimeNow;

            memcpy(pBuffer, &(s_pRXBlocksStack[i]->video_block_index), sizeof(u32));
            pBuffer += sizeof(u32);
            *pBuffer = (u8)k;
            pBuffer++;
            *pBuffer = (u8)(s_pRXBlocksStack[i]->packetsInfo[k].uRetrySentCount);
            pBuffer++;

            totalCountRequested++;
            countToRequestForBlock--;
            if ( countToRequestForBlock == 0 )
               break;

            if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
               break;
         }
      }

      if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
         break;

      // Request half the EC for the missing packets

      int countDataPacketsMissingInBlock = s_pRXBlocksStack[i]->data_packets - s_pRXBlocksStack[i]->received_data_packets;
      int countECPacketsToRequest = 0;
      if ( (s_pRXBlocksStack[i]->fec_packets > 0) && (s_pRXBlocksStack[i]->data_packets > 0) )
         countECPacketsToRequest = (countDataPacketsMissingInBlock * s_pRXBlocksStack[i]->fec_packets)/s_pRXBlocksStack[i]->data_packets/2;
      //log_line("DEBUG request %d EC retransmissions for %d missing data packets", countECPacketsToRequest, countDataPacketsMissingInBlock);
      int dataPackets = s_pRXBlocksStack[i]->data_packets;
      for( int k=0; k<countECPacketsToRequest; k++ )
      {
         if ( s_pRXBlocksStack[i]->packetsInfo[k + dataPackets].state == RX_PACKET_STATE_RECEIVED )
            continue;
         
         s_pRXBlocksStack[i]->packetsInfo[k + dataPackets].uTimeFirstRetrySent = g_TimeNow;
         s_pRXBlocksStack[i]->packetsInfo[k + dataPackets].uTimeLastRetrySent = g_TimeNow;
         s_pRXBlocksStack[i]->packetsInfo[k + dataPackets].uRetrySentCount = 1;
         totalCountRequestedNew++;
         s_pRXBlocksStack[i]->totalPacketsRequested++;

         if ( 0 == s_pRXBlocksStack[i]->uTimeFirstRetrySent )
            s_pRXBlocksStack[i]->uTimeFirstRetrySent = g_TimeNow;
         s_pRXBlocksStack[i]->uTimeLastRetrySent = g_TimeNow;

         memcpy(pBuffer, &(s_pRXBlocksStack[i]->video_block_index), sizeof(u32));
         pBuffer += sizeof(u32);
         *pBuffer = (u8)(k + dataPackets);
         pBuffer++;
         *pBuffer = (u8)(s_pRXBlocksStack[i]->packetsInfo[k+dataPackets].uRetrySentCount);
         pBuffer++;

         totalCountRequested++;

         if ( totalCountRequested >= MAX_RETRANSMISSION_PACKETS_IN_REQUEST-2 )
            break;
      }
   }

   // No new video packets for a long time? Request next one

   if ( 0 != g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs )
   if ( s_LastReceivedVideoPacketInfo.video_block_index != MAX_U32 )
   if ( s_LastReceivedVideoPacketInfo.receive_time != 0 )
   if ( s_LastReceivedVideoPacketInfo.receive_time + g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs <= g_TimeNow )
   {
      u32 videoBlock = s_LastReceivedVideoPacketInfo.video_block_index;
      u32 videoPacket = s_LastReceivedVideoPacketInfo.video_block_packet_index;
      videoPacket++;
      if ( videoPacket >= (u32)s_VDStatsCache.data_packets_per_block )
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
      //log_line("DEBUG: Requested next video packet after a long pause (last received %d ms ago): [%u/%d]", g_TimeNow - s_LastReceivedVideoPacketInfo.receive_time, videoBlock, videoPacket);
   }

   if ( 0 == totalCountRequested )      
      return;

   s_LastTimeRequestedRetransmissions = g_TimeNow;

   if ( bUseNewVersion )
   {
      s_uRequestRetransmissionUniqueId++;
      memcpy((u8*)&(buffer[0]), (u8*)&s_uRequestRetransmissionUniqueId, sizeof(u32));
      buffer[4] = 0; // video stream id
      buffer[5] = (u8) totalCountRequested;
   }
   else
   {
      buffer[0] = 0; // video stream id
      buffer[1] = (u8) totalCountRequested;
   }
   
   g_ControllerAdaptiveVideoInfo.uIntervalsRequestedRetransmissions[g_ControllerAdaptiveVideoInfo.uCurrentIntervalIndex] += totalCountRequestedNew;
   g_ControllerAdaptiveVideoInfo.uIntervalsRetriedRetransmissions[g_ControllerAdaptiveVideoInfo.uCurrentIntervalIndex] += totalCountReRequested;
   //if ( totalCountReRequested > 0 )
   //   log_line("DEBUG video: total req: %d, total req new: %d, total rereq: %d", totalCountRequested, totalCountRequestedNew, totalCountReRequested);
   g_ControllerRetransmissionsStats.history[0].uCountRequestedRetransmissions++;
   g_ControllerRetransmissionsStats.history[0].uCountRequestedPacketsForRetransmission += totalCountRequested;
   g_ControllerRetransmissionsStats.history[0].uCountReRequestedPacketsForRetransmission += totalCountReRequested;

   g_ControllerRetransmissionsStats.totalRequestedRetransmissions++;
   g_ControllerRetransmissionsStats.totalRequestedSegments += totalCountRequestedNew;

   // Store info about this retransmission request

   if ( g_ControllerRetransmissionsStats.iListRetransmissionsCount < MAX_HISTORY_STACK_RETRANSMISSION_INFO )
   {
      int k = g_ControllerRetransmissionsStats.iListRetransmissionsCount;
      g_ControllerRetransmissionsStats.listRetransmissions[k].uRetransmissionId = s_uRequestRetransmissionUniqueId;
      g_ControllerRetransmissionsStats.listRetransmissions[k].uRequestTime = g_TimeNow; 
      g_ControllerRetransmissionsStats.listRetransmissions[k].uRequestedSegments = totalCountRequested; 
      g_ControllerRetransmissionsStats.listRetransmissions[k].uReceivedSegments = 0;

      u8* pTmp = &(buffer[2]);
      if ( bUseNewVersion )
         pTmp = &(buffer[6]);

      for( int i=0; i<totalCountRequested; i++ )
      {
         memcpy((u8*)&(g_ControllerRetransmissionsStats.listRetransmissions[k].uRequestedVideoBlockIndex[i]), pTmp, sizeof(u32));
         pTmp += 4;
         g_ControllerRetransmissionsStats.listRetransmissions[k].uRequestedVideoBlockPacketIndex[i] = *pTmp;
         pTmp++;
         g_ControllerRetransmissionsStats.listRetransmissions[k].uRequestedVideoPacketRetryCount[i] = *pTmp;
         pTmp++;
         g_ControllerRetransmissionsStats.listRetransmissions[k].uReceivedSegmentCount[i] = 0;
         g_ControllerRetransmissionsStats.listRetransmissions[k].uReceivedSegmentTime[i] = 0;
      }

      g_ControllerRetransmissionsStats.listRetransmissions[k].uMinResponseTime = 0;
      g_ControllerRetransmissionsStats.listRetransmissions[k].uMaxResponseTime = 0;

      g_ControllerRetransmissionsStats.iListRetransmissionsCount++;
   }

   if ( g_PD_ControllerLinkStats.tmp_video_streams_requested_retransmission_packets[0] + totalCountRequested < 254 )
      g_PD_ControllerLinkStats.tmp_video_streams_requested_retransmission_packets[0] += totalCountRequested;
   else
      g_PD_ControllerLinkStats.tmp_video_streams_requested_retransmission_packets[0] = 254;

   //_rx_video_log_line("");
   //_rx_video_log_line("");
   //_rx_video_log_line("Requested %d packets", totalCountRequested);
   
   /*
   char szBuff[256];
   szBuff[0] = 0;
   u8* pTmp = &(buffer[6]);
   for( int i=0; i<totalCountRequested; i++ )
   {
      if ( i > 3 )
         break;

      char szTmp[64];
      u32 uTmp = 0;
      memcpy((u8*)&uTmp, pTmp, sizeof(u32));
      pTmp += 4;
      u8 uTmp2 = *pTmp;
      pTmp++;
      sprintf(szTmp, "[%u/%d]x%d", uTmp, uTmp2, *pTmp);
      pTmp++;

      if ( 0 != szBuff[0] )
         strcat(szBuff, ", ");
      strcat(szBuff, szTmp);
   }
   log_line("DEBUG requested id %u, request %d packets (%d new): %s", s_uRequestRetransmissionUniqueId, totalCountRequested, totalCountRequestedNew, szBuff);
   */

   int bufferLength = 0;

   if ( bUseNewVersion )
      bufferLength = sizeof(u32) + 2 + totalCountRequested*(sizeof(u32)+sizeof(u8)+sizeof(u8));
   else
      bufferLength = 2 + totalCountRequested*(sizeof(u32)+sizeof(u8)+sizeof(u8));

   //if ( bUseNewVersion )
   //   log_line("DEBUG requested new version retr id: %u, %d segments", s_uRequestRetransmissionUniqueId, totalCountRequested);
   
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_VIDEO;
   PH.packet_type = PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS;
   if ( bUseNewVersion )
      PH.packet_type = PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2;
   
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + bufferLength;

   #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO  
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS )
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO )
   if ( g_TimeNow > g_TimeLastControllerLinkStatsSent + CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL_MS/2 )
      PH.total_length += get_controller_radio_link_stats_size();
   #endif

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet + sizeof(t_packet_header), buffer, bufferLength);

   #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS) )
   if ( g_TimeNow > g_TimeLastControllerLinkStatsSent + CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL_MS/2 )
   {
      add_controller_radio_link_stats_to_buffer(packet + sizeof(t_packet_header)+bufferLength);
      g_TimeLastControllerLinkStatsSent = g_TimeNow;
   }
   #endif
   
   packets_queue_add_packet(&s_QueueRadioPackets, packet);

   // If there are retried retransmissions, send the request twice
   //if ( totalCountRequestedNew < totalCountRequested )
   //   packets_queue_add_packet(&s_QueueRadioPackets, packet);
}

void _add_packet_to_received_blocks_buffers(u8* pBuffer, int length, int rx_buffer_block_index)
{
   //t_packet_header* pPH = (t_packet_header*)pBuffer;
   t_packet_header_video_full* pPVF = (t_packet_header_video_full*) (pBuffer+sizeof(t_packet_header));

   if ( s_RXBlocksStackTopIndex < rx_buffer_block_index )
      s_RXBlocksStackTopIndex = rx_buffer_block_index;

   if ( s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[pPVF->video_block_packet_index].state == RX_PACKET_STATE_RECEIVED )
   {
      //log_line("DEBUG do not add duplicate packet in buffer");
      return;
   }

   if ( s_pRXBlocksStack[rx_buffer_block_index]->uTimeFirstPacketReceived == MAX_U32 )
      s_pRXBlocksStack[rx_buffer_block_index]->uTimeFirstPacketReceived = g_TimeNow;


   s_pRXBlocksStack[rx_buffer_block_index]->video_block_index = pPVF->video_block_index;
   s_pRXBlocksStack[rx_buffer_block_index]->packet_length = pPVF->video_packet_length;
   s_pRXBlocksStack[rx_buffer_block_index]->data_packets = pPVF->block_packets;
   s_pRXBlocksStack[rx_buffer_block_index]->fec_packets = pPVF->block_fecs;
   s_pRXBlocksStack[rx_buffer_block_index]->uTimeLastUpdated = g_TimeNow;
   s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[pPVF->video_block_packet_index].state = RX_PACKET_STATE_RECEIVED;
   s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[pPVF->video_block_packet_index].packet_length = pPVF->video_packet_length;

   if ( pPVF->video_packet_length < 100 || pPVF->video_packet_length > MAX_PACKET_TOTAL_SIZE )
      log_softerror_and_alarm("Invalid video block size to copy (%d bytes)", pPVF->video_packet_length);
   else
      memcpy(s_pRXBlocksStack[rx_buffer_block_index]->packetsInfo[pPVF->video_block_packet_index].pData, pBuffer+sizeof(t_packet_header)+sizeof(t_packet_header_video_full), pPVF->video_packet_length);

   if ( pPVF->video_block_packet_index < s_pRXBlocksStack[rx_buffer_block_index]->data_packets )
      s_pRXBlocksStack[rx_buffer_block_index]->received_data_packets++;
   else
      s_pRXBlocksStack[rx_buffer_block_index]->received_fec_packets++;


   s_VDStatsCache.currentPacketsInBuffers++;
   if ( s_VDStatsCache.currentPacketsInBuffers > s_VDStatsCache.maxPacketsInBuffers )
      s_VDStatsCache.maxPacketsInBuffers = s_VDStatsCache.currentPacketsInBuffers;
}

// Returns index in the receive stack for the block where we added the packet, or -1 if it was discarded

int _process_retransmitted_video_packet(u8* pBuffer, int length)
{
   t_packet_header_video_full* pPHVF = (t_packet_header_video_full*) (pBuffer+sizeof(t_packet_header));

   //_rx_video_log_line("Processing re-packet %u/%d (stack: %d, last output: %u)", pPHVF->video_block_index, pPHVF->video_block_packet_index, s_RXBlocksStackTopIndex, s_LastOutputVideoBlockIndex);

   if ( -1 == s_RXBlocksStackTopIndex )
   {
      return -1;
   }
   if ( pPHVF->video_block_index < s_pRXBlocksStack[0]->video_block_index )
   {
      return -1;
   }
   if ( pPHVF->video_block_index > s_pRXBlocksStack[s_RXBlocksStackTopIndex]->video_block_index )
   {
      return -1;
   }
   int dest_stack_index = (pPHVF->video_block_index-s_pRXBlocksStack[0]->video_block_index);
   if ( dest_stack_index < 0 || dest_stack_index >= MAX_RXTX_BLOCKS_BUFFER )
   {
      return -1;
   }
   if ( s_pRXBlocksStack[dest_stack_index]->packetsInfo[pPHVF->video_block_packet_index].state == RX_PACKET_STATE_RECEIVED )
   {
      return -1;
   }

   s_RetransmissionStats.uRetransmissionTimeLast = g_TimeNow - s_pRXBlocksStack[dest_stack_index]->packetsInfo[pPHVF->video_block_packet_index].uTimeFirstRetrySent;
   if ( s_RetransmissionStats.uRetransmissionTimeLast < s_RetransmissionStats.uRetransmissionTimeMinim )
      s_RetransmissionStats.uRetransmissionTimeMinim = s_RetransmissionStats.uRetransmissionTimeLast;

   if ( s_RetransmissionStats.retransmissionTimePreviousBuffer[s_RetransmissionStats.uRetransmissionTimePreviousIndex] != MAX_U32 )
      s_RetransmissionStats.uRetransmissionTimePreviousSum -= s_RetransmissionStats.retransmissionTimePreviousBuffer[s_RetransmissionStats.uRetransmissionTimePreviousIndex];
   s_RetransmissionStats.retransmissionTimePreviousBuffer[s_RetransmissionStats.uRetransmissionTimePreviousIndex] = s_RetransmissionStats.uRetransmissionTimeLast;
   s_RetransmissionStats.uRetransmissionTimePreviousSum += s_RetransmissionStats.uRetransmissionTimeLast;
   s_RetransmissionStats.uRetransmissionTimePreviousIndex++;
   if ( s_RetransmissionStats.uRetransmissionTimePreviousIndex >= MAX_RETRANSMISSION_BUFFER_HISTORY_LENGTH )
      s_RetransmissionStats.uRetransmissionTimePreviousIndex = 0;
   if ( s_RetransmissionStats.uRetransmissionTimePreviousSumCount < MAX_RETRANSMISSION_BUFFER_HISTORY_LENGTH )
      s_RetransmissionStats.uRetransmissionTimePreviousSumCount++;
   if ( 0 != s_RetransmissionStats.uRetransmissionTimePreviousSumCount )
      s_RetransmissionStats.uRetransmissionTimeAverage = s_RetransmissionStats.uRetransmissionTimePreviousSum / s_RetransmissionStats.uRetransmissionTimePreviousSumCount;

   //_log_current_buffer();
   //log_line("DEBUG Adding retr packet to buffer at index %d", dest_stack_index);

   _add_packet_to_received_blocks_buffers(pBuffer, length, dest_stack_index);

/////////////////////////////
   /*
   int totalMissing = 0;
      for( int i=0; i<s_RXBlocksStackTopIndex; i++ )
      {
         int c = s_pRXBlocksStack[i]->data_packets + s_pRXBlocksStack[i]->fec_packets - (s_pRXBlocksStack[i]->received_data_packets + s_pRXBlocksStack[i]->received_fec_packets);
         totalMissing += c;
      }
   log_line("DEBUG added a retransmission packet [%u/%d] to buffers. Buffers now have %d blocks, starting at [%u/%d], %d total missing packets.",
        pPHVF->video_block_index, pPHVF->video_block_packet_index,
        s_RXBlocksStackTopIndex+1,
        s_pRXBlocksStack[0]->video_block_index, 0,
        totalMissing );
   */
/////////////////////////////////

   //_log_current_buffer();

   return dest_stack_index;
}

// Returns index in the receive stack for the block where we added the packet, or -1 if it was discarded

int _process_video_packet(u8* pBuffer, int length)
{
   t_packet_header_video_full* pPVF = (t_packet_header_video_full*) (pBuffer+sizeof(t_packet_header));

   //_rx_video_log_line("Processing packet %u/%d (stack: %d, last output: %u)", pPVF->video_block_index, pPVF->video_block_packet_index, s_RXBlocksStackTopIndex, s_LastOutputVideoBlockIndex);

   // Find a position in blocks buffer where to put it

   // Empty buffers?

   if ( s_RXBlocksStackTopIndex == -1 )
   {
      // Never used?
      if ( s_LastOutputVideoBlockIndex == MAX_U32 )
      {
         // If we are past recoverable point in a block just discard it and wait for a new block
         if ( pPVF->video_block_packet_index > pPVF->block_fecs )
         {
            //log_line("DEBUG: wait for start of block (received [%u/%d])", pPVF->video_block_index, (int)pPVF->video_block_packet_index);
            return -1;
         }
         //_rx_video_log_line("Started new buffers at[%u/%d]", pPVF->video_block_index, pPVF->video_block_packet_index);
         log_line("Started new buffers at[%u/%d]", pPVF->video_block_index, pPVF->video_block_packet_index);
         s_RXBlocksStackTopIndex = 0;
         _add_packet_to_received_blocks_buffers(pBuffer, length, 0);
         //_log_current_buffer();
         return 0;
      }
   }

   // Discard received packets from blocks prior to the first block in stack or to the last output block

   if ( s_LastOutputVideoBlockIndex != MAX_U32 )
   if ( pPVF->video_block_index <= s_LastOutputVideoBlockIndex )
      return -1;

   if ( s_RXBlocksStackTopIndex >= 0 )
   if ( pPVF->video_block_index < s_pRXBlocksStack[0]->video_block_index )
      return -1;


   // Find position for this block in the receive stack

   u32 stackIndex = 0;
   if ( s_RXBlocksStackTopIndex >= 0 && pPVF->video_block_index >= s_pRXBlocksStack[0]->video_block_index )
      stackIndex = pPVF->video_block_index - s_pRXBlocksStack[0]->video_block_index;
   else if ( s_LastOutputVideoBlockIndex != MAX_U32 )
      stackIndex = pPVF->video_block_index - s_LastOutputVideoBlockIndex-1;
   
   if ( stackIndex >= (u32)s_RXMaxBlocksToBuffer )
   {
      _rx_video_log_line("Received packet after the end of RX buffers. Discarding a segment to make room...");
      int overflow = stackIndex - s_RXMaxBlocksToBuffer+1;
      if ( (overflow > s_RXMaxBlocksToBuffer*2/3) || ((stackIndex - (u32)s_RXBlocksStackTopIndex) > (u32)s_RXMaxBlocksToBuffer*2/3) )
      {
         _update_history_discared_all_stack();
         _rx_video_reset_receive_buffers();
         s_LastOutputVideoBlockTime = 0;
         s_LastOutputVideoBlockIndex = MAX_U32;
      }
      else
      {
         // Discard few more blocks if they also have missing packets and can't be reconstructed right now; to avoid multiple consecutive discards events
         int iLookAhead = 2 + s_RXMaxBlocksToBuffer/10;
         while ( overflow < s_RXBlocksStackTopIndex && iLookAhead > 0 )
         {
            if ( s_pRXBlocksStack[overflow]->received_data_packets + s_pRXBlocksStack[overflow]->received_fec_packets >= s_pRXBlocksStack[overflow]->data_packets )
               break;
            overflow++;
            iLookAhead--;
         }
        
         _update_history_discared_stack_segment(overflow);
         _push_incomplete_blocks_out(overflow, false);
      }

      // Did we discarded everything?
      if ( -1 == s_RXBlocksStackTopIndex )
      {
         // If we are past recoverable point in a block just discard it and wait for a new block
         if ( pPVF->video_block_packet_index > pPVF->block_fecs )
            return -1;

         _rx_video_log_line("Started new buffers at[%u/%d]", pPVF->video_block_index, pPVF->video_block_packet_index);
         s_RXBlocksStackTopIndex = 0;
         _add_packet_to_received_blocks_buffers(pBuffer, length, 0);
         return 0;
      }
      stackIndex -= overflow;
   }

   // Add the packet to the buffer
   _add_packet_to_received_blocks_buffers(pBuffer, length, stackIndex);
   
   // Add info about any missing blocks in the stack: video block indexes, data scheme, last update time for any skipped blocks
   for( u32 i=0; i<stackIndex; i++ )
      if ( 0 == s_pRXBlocksStack[i]->uTimeLastUpdated )
      {
         s_pRXBlocksStack[i]->uTimeLastUpdated = g_TimeNow;
         s_pRXBlocksStack[i]->data_packets = pPVF->block_packets;
         s_pRXBlocksStack[i]->fec_packets = pPVF->block_fecs;
         s_pRXBlocksStack[i]->video_block_index = pPVF->video_block_index-stackIndex+i;
      }
   return stackIndex;
}

// Return -1 if discarded

int _preprocess_retransmitted_video_packet(int interfaceNb, u8* pBuffer, int length)
{
   //t_packet_header* pPH = (t_packet_header*) pBuffer;
   t_packet_header_video_full* pPHVF = (t_packet_header_video_full*) (pBuffer+sizeof(t_packet_header));

   g_uTimeLastReceivedResponseToAMessage = g_TimeNow;

   u32 uIdRetransmissionRequest = (pPHVF->video_width << 16) | pPHVF->video_height;
   
   int iRetransmissionIndex = -1;
   controller_retransmission_state* pRetransmissionInfo = NULL;
   
   for( int i=0; i<g_ControllerRetransmissionsStats.iListRetransmissionsCount; i++ )
   {
      if ( g_ControllerRetransmissionsStats.listRetransmissions[i].uRetransmissionId == uIdRetransmissionRequest )
      {
         iRetransmissionIndex = i;
         pRetransmissionInfo = &(g_ControllerRetransmissionsStats.listRetransmissions[i]);
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

       if ( dTime < g_ControllerRetransmissionsStats.history[0].uMinRetransmissionRoundtripTime ||
            g_ControllerRetransmissionsStats.history[0].uMinRetransmissionRoundtripTime == 0 )
          g_ControllerRetransmissionsStats.history[0].uMinRetransmissionRoundtripTime = dTime;
       if ( (dTime < 255) && dTime > g_ControllerRetransmissionsStats.history[0].uMaxRetransmissionRoundtripTime )
          g_ControllerRetransmissionsStats.history[0].uMaxRetransmissionRoundtripTime = dTime;

       // Find the video packet info in the list for this retransmission

       for( int i=0; i<pRetransmissionInfo->uRequestedSegments; i++ )
       {
          if ( pRetransmissionInfo->uRequestedVideoBlockIndex[i] == pPHVF->video_block_index )
          if ( pRetransmissionInfo->uRequestedVideoBlockPacketIndex[i] == pPHVF->video_block_packet_index )
          {
             // Received the first ever response for this retransmission 
             if ( 0 == pRetransmissionInfo->uReceivedSegments )
             {
                g_ControllerRetransmissionsStats.history[0].uCountAcknowledgedRetransmissions++;
                g_ControllerRetransmissionsStats.totalReceivedRetransmissions++;
             }

             // Received this segment for the first time
             if ( 0 == pRetransmissionInfo->uReceivedSegmentCount[i] )
             {
                g_ControllerRetransmissionsStats.listRetransmissions[iRetransmissionIndex].uReceivedSegmentTime[i] = g_TimeNow;
                g_ControllerRetransmissionsStats.history[0].uAverageRetransmissionRoundtripTime += dTime;
                g_ControllerRetransmissionsStats.history[0].uCountReceivedRetransmissionPackets++;
                g_ControllerRetransmissionsStats.totalReceivedSegments++;
                pRetransmissionInfo->uReceivedSegments++;
             }
             else
                g_ControllerRetransmissionsStats.history[0].uCountReceivedRetransmissionPacketsDuplicate++;
                
             pRetransmissionInfo->uReceivedSegmentCount[i]++;
             break;
          }
       }

       if ( pRetransmissionInfo->uRequestedSegments == pRetransmissionInfo->uReceivedSegments )
       {
          //log_line("DEBUG received all segments for retransmission id %u", pRetransmissionInfo->uRetransmissionId);
          g_ControllerRetransmissionsStats.history[0].uCountCompletedRetransmissions++;
          // Remove it from the retransmissions list
          for( int i=iRetransmissionIndex; i<g_ControllerRetransmissionsStats.iListRetransmissionsCount-1; i++ )
             memcpy( (u8*)&(g_ControllerRetransmissionsStats.listRetransmissions[i]), 
                    (u8*)&(g_ControllerRetransmissionsStats.listRetransmissions[i+1]),
                    sizeof(controller_retransmission_state) );
          g_ControllerRetransmissionsStats.iListRetransmissionsCount--;
       }
   }
   
   if ( -1 == s_RXBlocksStackTopIndex )
   {
      //log_line("DEBUG Discarded one packet A");
      //_log_current_buffer();
      return -1;
   }

   if ( s_LastOutputVideoBlockIndex != MAX_U32 )
   if ( pPHVF->video_block_index <= s_LastOutputVideoBlockIndex )
   {
      //log_line("DEBUG Discarded one packet B, received too late");
      //_log_current_buffer();
      return -1;
   }

   int stackIndex = pPHVF->video_block_index - s_pRXBlocksStack[0]->video_block_index;
   if ( stackIndex < 0 || stackIndex >= MAX_RXTX_BLOCKS_BUFFER )
   {
      //log_line("DEBUG Discarded one packet C");
      //_log_current_buffer();
      return -1;
   }

   if ( s_pRXBlocksStack[stackIndex]->packetsInfo[pPHVF->video_block_packet_index].state == RX_PACKET_STATE_RECEIVED )
   {
      //log_line("DEBUG Discarded one packet D, received duplicate");
      //_log_current_buffer();
      return -1;
   }

   return 0;
}


// Returns the gap from the last received packet, or -1 if discarded
// The gap is 0 (return value) for retransmitted packets that are not ignored

int _preprocess_video_packet(int interfaceNb, u8* pBuffer, int length)
{
   t_packet_header* pPH = (t_packet_header*) pBuffer;
   t_packet_header_video_full* pPHVF = (t_packet_header_video_full*) (pBuffer+sizeof(t_packet_header));

   //_rx_video_log_line("Recv packet [%u/%d], retr: %d, len: %d/%d", pPVF->video_block_index, pPVF->video_block_packet_index, pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED, pPH->total_length, pPVF->video_packet_length);

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
   {
      return _preprocess_retransmitted_video_packet(interfaceNb, pBuffer, length );
   }

   u32 prevRecvVideoBlockIndex = s_LastReceivedVideoPacketInfo.video_block_index;
   u32 prevRecvVideoBlockPacketIndex = s_LastReceivedVideoPacketInfo.video_block_packet_index;

   s_LastReceivedVideoPacketInfo.receive_time = g_TimeNow;
   s_LastReceivedVideoPacketInfo.stream_packet_idx = (pPH->stream_packet_idx & PACKET_FLAGS_MASK_STREAM_PACKET_IDX);
   s_LastReceivedVideoPacketInfo.video_block_index = pPHVF->video_block_index;
   s_LastReceivedVideoPacketInfo.video_block_packet_index = pPHVF->video_block_packet_index;


   if ( s_LastOutputVideoBlockIndex != MAX_U32 )
   if ( pPHVF->video_block_index <= s_LastOutputVideoBlockIndex )
   {
      //log_line("DEBUG: received packet before last output");
      return -1;
   }
   if ( -1 != s_RXBlocksStackTopIndex )
   {
      if ( pPHVF->video_block_index < s_pRXBlocksStack[0]->video_block_index )
      {
         //log_line("DEBUG: received packet before stack bottom");
         return -1;
      }
      int stackIndex = pPHVF->video_block_index - s_pRXBlocksStack[0]->video_block_index;
      if ( stackIndex >= 0 && stackIndex < MAX_RXTX_BLOCKS_BUFFER )
      if ( s_pRXBlocksStack[stackIndex]->packetsInfo[pPHVF->video_block_packet_index].state == RX_PACKET_STATE_RECEIVED )
      {
         //log_line("DEBUG: received duplicate packet");
         return -1;
      }
   }
   
   // Regular video packets that will be processed further

   // Compute gap (in packets) between this and last video packet received
   // Used only for history reporting purposes

   int gap = 0;
   if ( pPHVF->video_block_index == prevRecvVideoBlockIndex )
   if ( pPHVF->video_block_packet_index > prevRecvVideoBlockPacketIndex )
      gap = pPHVF->video_block_packet_index - prevRecvVideoBlockPacketIndex-1;

   if ( pPHVF->video_block_index > prevRecvVideoBlockIndex )
   {
       gap = pPHVF->block_packets + pPHVF->block_fecs - prevRecvVideoBlockPacketIndex - 1;
       gap += pPHVF->video_block_packet_index;
       gap += (pPHVF->block_packets + pPHVF->block_fecs) * (pPHVF->video_block_index - prevRecvVideoBlockIndex - 1);
   }
   if ( gap > g_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[0] )
      g_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[0] = (gap>255) ? 255:gap;

   // Check video resolution change

   if ( s_VDStatsCache.width != 0 && (s_VDStatsCache.width != pPHVF->video_width || s_VDStatsCache.height != pPHVF->video_height) )
   if ( (s_LastVideoResolutionChangeVideoBlockIndex == MAX_U32) || (pPHVF->video_block_index > s_LastVideoResolutionChangeVideoBlockIndex) )
   {
      s_LastVideoResolutionChangeVideoBlockIndex = pPHVF->video_block_index;
      log_line("Video resolution changed at video block index %u. Signal reload of the video player.", pPHVF->video_block_index);
      _rx_video_log_line("Video resolution changed at video block index %u.", pPHVF->video_block_index);
      sem_t* p = sem_open(SEMAPHORE_RESTART_VIDEO_PLAYER, O_CREAT, S_IWUSR | S_IRUSR, 0);
      sem_post(p);
      sem_close(p);  
   }

   // Check if encodings changed (ignore flags related to tx only or irelevant: retransmission window, duplication percent)
   //log_line("DEBUG: Current video profile: %d, ec: %d/%d", (pPHVF->video_link_profile & 0x0F), pPHVF->block_packets, pPHVF->block_fecs);

   if ( (s_VDStatsCache.video_link_profile & 0x0F) != (pPHVF->video_link_profile & 0x0F) ||
        (s_VDStatsCache.encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) != (pPHVF->encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) ||
        s_VDStatsCache.data_packets_per_block != pPHVF->block_packets ||
        s_VDStatsCache.fec_packets_per_block != pPHVF->block_fecs ||
        s_VDStatsCache.video_packet_length != pPHVF->video_packet_length
      )
   if ( (s_LastHardEncodingsChangeVideoBlockIndex == MAX_U32) || (pPHVF->video_block_index > s_LastHardEncodingsChangeVideoBlockIndex) )
   {
      s_uEncodingsChangeCount++;
      s_LastHardEncodingsChangeVideoBlockIndex = pPHVF->video_block_index;

      
      bool bOnlyECschemeChanged = false;
      if ( (pPHVF->block_packets != s_VDStatsCache.data_packets_per_block) || (pPHVF->block_fecs != s_VDStatsCache.fec_packets_per_block) )
      if ( (s_VDStatsCache.video_link_profile & 0x0F) == (pPHVF->video_link_profile & 0x0F) &&
           (s_VDStatsCache.encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) == (pPHVF->encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT | ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE | 0x00FF0000))) &&
           (s_VDStatsCache.video_packet_length == pPHVF->video_packet_length)
         )
         bOnlyECschemeChanged = true;

      if ( bOnlyECschemeChanged )
      {
          log_line("EC only changed (%u times) on video at block idx %u. Data/EC Old: %d/%d, New: %d/%d", s_uEncodingsChangeCount, pPHVF->video_block_index, s_VDStatsCache.data_packets_per_block, s_VDStatsCache.fec_packets_per_block, pPHVF->block_packets, pPHVF->block_fecs);
          _rx_video_log_line("Encodings EC scheme only changed on the received video stream at video block index %u. Data/EC Old: %d/%d, New: %d/%d", pPHVF->video_block_index, s_VDStatsCache.data_packets_per_block, s_VDStatsCache.fec_packets_per_block, pPHVF->block_packets, pPHVF->block_fecs);
      }
      else if ( g_TimeNow > s_TimeLastEncodingsChangedLog + 5000 )
      {
         s_TimeLastEncodingsChangedLog = g_TimeNow;

         int kf = pPHVF->video_keyframe;
         if ( pPHVF->video_keyframe > 200 )
            kf = (pPHVF->video_keyframe-200)*20;
         log_line("Encodings changed (%u times) on the received stream (at video block index %u):", s_uEncodingsChangeCount, pPHVF->video_block_index);
         log_line("Video link profile (from [user/current] to [user/current]: [%s/%s] -> [%s/%s]", str_get_video_profile_name((s_VDStatsCache.video_link_profile >> 4) & 0x0F), str_get_video_profile_name(s_VDStatsCache.video_link_profile & 0x0F), str_get_video_profile_name((pPHVF->video_link_profile >> 4) & 0x0F), str_get_video_profile_name(pPHVF->video_link_profile & 0x0F));
         log_line("Extra param (from/to): %d -> %d", s_VDStatsCache.encoding_extra_flags, pPHVF->encoding_extra_flags);
         log_line("data packets (from/to): %d -> %d", s_VDStatsCache.data_packets_per_block, pPHVF->block_packets);
         log_line("ec packets (from/to): %d -> %d", s_VDStatsCache.fec_packets_per_block, pPHVF->block_fecs);
         log_line("keyframe (from/to): %d -> %d", s_VDStatsCache.keyframe, kf);
         log_line("packet length (from/to): %d -> %d", s_VDStatsCache.video_packet_length, pPHVF->video_packet_length);
         log_dword_bits("New encoding extra flags: ", pPHVF->encoding_extra_flags);
         
         _rx_video_log_line("Encodings changed on the received stream (at video block index %u):", pPHVF->video_block_index);
         _rx_video_log_line("Video link profile (from [user/current] to [user/current]: [%s/%s] -> [%s/%s]", str_get_video_profile_name((s_VDStatsCache.video_link_profile >> 4) & 0x0F), str_get_video_profile_name(s_VDStatsCache.video_link_profile & 0x0F), str_get_video_profile_name((pPHVF->video_link_profile >> 4) & 0x0F), str_get_video_profile_name(pPHVF->video_link_profile & 0x0F));
         _rx_video_log_line("Extra param (from/to): %d -> %d", s_VDStatsCache.encoding_extra_flags, pPHVF->encoding_extra_flags);
         _rx_video_log_line("data packets (from/to): %d -> %d", s_VDStatsCache.data_packets_per_block, pPHVF->block_packets);
         _rx_video_log_line("ec packets (from/to): %d -> %d", s_VDStatsCache.fec_packets_per_block, pPHVF->block_fecs);
         _rx_video_log_line("packet length (from/to): %d -> %d", s_VDStatsCache.video_packet_length, pPHVF->video_packet_length);
      }
      else
      {
         //log_line("New encodings: [%u/0], %d/%d/%d", pPHVF->video_block_index, pPHVF->block_packets, pPHVF->block_fecs, pPHVF->video_packet_length);
      }
      // Max retransmission window changed on user selected profile?

      bool bReinitDueToWindowChange = false;
      if ( (pPHVF->video_link_profile & 0x0F) == g_pCurrentModel->video_params.user_selected_video_link_profile )
      if ( s_uLastReceivedVideoLinkProfile == g_pCurrentModel->video_params.user_selected_video_link_profile )
      if ( (pPHVF->encoding_extra_flags & 0xFF00) != (s_VDStatsCache.encoding_extra_flags & 0xFF00) )
         bReinitDueToWindowChange = true;

      s_VDStatsCache.video_link_profile = pPHVF->video_link_profile;
      s_VDStatsCache.encoding_extra_flags = pPHVF->encoding_extra_flags;
      s_VDStatsCache.data_packets_per_block = pPHVF->block_packets;
      s_VDStatsCache.fec_packets_per_block = pPHVF->block_fecs;
      s_VDStatsCache.video_packet_length = pPHVF->video_packet_length;

      if ( bReinitDueToWindowChange )
      {
         log_line("Retransmission window (milisec) has changed (to %d ms). Reinitializing rx state...", ((pPHVF->encoding_extra_flags & 0xFF00)>>8)*5);
         _rx_video_log_line("Retransmission window (milisec) has changed (to %d ms). Reinitializing rx state...", ((pPHVF->encoding_extra_flags & 0xFF00)>>8)*5);
         _rx_video_reset_receive_state();
         gap = 0;
         s_LastHardEncodingsChangeVideoBlockIndex = pPHVF->video_block_index;
         s_LastVideoResolutionChangeVideoBlockIndex = pPHVF->video_block_index;      
      }
   }

   s_uLastReceivedVideoLinkProfile = (pPHVF->video_link_profile & 0x0F);
   s_VDStatsCache.encoding_extra_flags = pPHVF->encoding_extra_flags;
   s_VDStatsCache.video_type = pPHVF->video_type;
   s_VDStatsCache.fps = pPHVF->video_fps;
   s_VDStatsCache.keyframe = pPHVF->video_keyframe;
   if ( pPHVF->video_keyframe > 200 )
      s_VDStatsCache.keyframe = (pPHVF->video_keyframe-200)*20;

   s_VDStatsCache.width = pPHVF->video_width;
   s_VDStatsCache.height = pPHVF->video_height;
   s_VDStatsCache.fec_time = pPHVF->fec_time;

   // Set initial stream level shift

   if ( g_ControllerAdaptiveVideoInfo.iLastRequestedLevelShift == -1 )
   {
      video_link_adaptive_set_intial_video_adjustment_level(s_uLastReceivedVideoLinkProfile, pPHVF->block_packets, pPHVF->block_fecs);
   }

   if ( g_ControllerAdaptiveVideoInfo.iLastRequestedKeyFrame <= 0 )
   {
      video_link_keyframe_set_intial_received_level(s_VDStatsCache.keyframe);
   }

   return gap;
}

void _check_and_discard_too_old_blocks()
{
   if ( -1 == s_RXBlocksStackTopIndex )
      return;

   if ( s_pRXBlocksStack[0]->uTimeLastUpdated >= g_TimeNow - s_iMilisecondsMaxRetransmissionWindow*1.5 )
      return;

   if ( s_pRXBlocksStack[s_RXBlocksStackTopIndex]->uTimeLastUpdated < g_TimeNow - s_iMilisecondsMaxRetransmissionWindow*1.5 )
   {
      _update_history_discared_all_stack();
      _rx_video_reset_receive_buffers();
      s_LastOutputVideoBlockTime = 0;
      s_LastOutputVideoBlockIndex = MAX_U32;
      //log_line("DEBUG discarded buffer");
      return;
   }


   // Discard blocks, from 0 index to newest, if they are past the retransmission window
   // Find first block that is too old (from top to bottom)

   int iStackIndex = s_RXBlocksStackTopIndex;
   //u32 uTimeTooOld = 0;

   while ( iStackIndex >= 0 )
   {
      if ( s_pRXBlocksStack[iStackIndex]->uTimeFirstPacketReceived != MAX_U32 )
      if ( s_pRXBlocksStack[iStackIndex]->uTimeFirstPacketReceived < g_TimeNow - (u32)s_iMilisecondsMaxRetransmissionWindow )
      {
         //uTimeTooOld = s_pRXBlocksStack[iStackIndex]->uTimeFirstPacketReceived;
         break;
      }
      iStackIndex--;
   }

   if ( iStackIndex >= 0 )
   {
      //log_line("DEBUG discard few blocks (%d) due to too old, (%d ms retransmission window), receive time: %s", iStackIndex+1, s_iMilisecondsMaxRetransmissionWindow, str_format_time(uTimeTooOld));
      _update_history_discared_stack_segment(iStackIndex+1);
      _push_incomplete_blocks_out(iStackIndex+1, false);
   }
}

void rx_video_periodic_checks()
{
   if ( g_TimeNow >= s_TimeLastHistoryStatsUpdate + g_VideoDecodeStatsHistory.outputHistoryIntervalMs )
   {
      s_TimeLastHistoryStatsUpdate = g_TimeNow;

      g_VideoDecodeStatsHistory.totalCurrentlyMissingPackets = 0;
      for( int i=0; i<s_RXBlocksStackTopIndex; i++ )
      {
         int c = s_pRXBlocksStack[i]->data_packets + s_pRXBlocksStack[i]->fec_packets - (s_pRXBlocksStack[i]->received_data_packets + s_pRXBlocksStack[i]->received_fec_packets);
         g_VideoDecodeStatsHistory.totalCurrentlyMissingPackets += c;
      }
      g_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[0] = g_VideoDecodeStatsHistory.totalCurrentlyMissingPackets;
      //if ( g_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[0] > 0 )
      //   log_line("DEBUG total missing on slice 0: %d", g_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[0]);

      g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0] = 0;
      for( int i=0; i<s_RXBlocksStackTopIndex; i++ )
      {
         if ( s_pRXBlocksStack[i]->data_packets > 0 )
         if ( s_pRXBlocksStack[i]->received_data_packets + s_pRXBlocksStack[i]->received_fec_packets >= s_pRXBlocksStack[i]->data_packets )
         if ( g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0] < 255 )
            g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[0]++;
      }

      memcpy((u8*)s_pSM_VideoDecodeStatsHistory, (u8*)(&g_VideoDecodeStatsHistory), sizeof(shared_mem_video_decode_stats_history));

      for( int i=MAX_HISTORY_VIDEO_INTERVALS-1; i>0; i-- )
      {
         g_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[i] = g_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[i-1];
         g_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[i] = g_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[i-1];
         g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[i] = g_VideoDecodeStatsHistory.outputHistoryMaxGoodBlocksPendingPerPeriod[i-1];
         g_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[i] = g_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[i-1];
         g_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[i] = g_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[i-1];
         g_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[i] = g_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[i-1];
         g_VideoDecodeStatsHistory.outputHistoryBlocksBadPerPeriod[i] = g_VideoDecodeStatsHistory.outputHistoryBlocksBadPerPeriod[i-1];
         g_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[i] = g_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[i-1];
         g_VideoDecodeStatsHistory.outputHistoryBlocksRetrasmitedPerPeriod[i] = g_VideoDecodeStatsHistory.outputHistoryBlocksRetrasmitedPerPeriod[i-1];
         g_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[i] = g_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[i-1];
         g_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[i] = g_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[i-1];
         g_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[i] = g_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[i-1];
      }

      g_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[0] = 0;
      g_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[0] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksOkPerPeriod[0] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksReconstructedPerPeriod[0] = 0;
      g_VideoDecodeStatsHistory.outputHistoryMaxECPacketsUsedPerPeriod[0] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksBadPerPeriod[0] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksMissingPerPeriod[0] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksRetrasmitedPerPeriod[0] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksMaxPacketsGapPerPeriod[0] = 0;
      g_VideoDecodeStatsHistory.outputHistoryBlocksDiscardedPerPeriod[0] = 0;
      g_VideoDecodeStatsHistory.missingTotalPacketsAtPeriod[0] = 0;
   }

   if ( g_TimeNow >= s_TimeLastControllerRestransmissionsStatsUpdate + g_ControllerRetransmissionsStats.refreshIntervalMs )
   {
      s_TimeLastControllerRestransmissionsStatsUpdate = g_TimeNow;

      // Compute total retransmissions requests and responses in last 500 milisec

      g_ControllerRetransmissionsStats.totalRequestedRetransmissionsLast500Ms = 0;
      g_ControllerRetransmissionsStats.totalReceivedRetransmissionsLast500Ms = 0;
      u32 dTime = 0;
      int iIndex = 0;
      while ( (dTime < 500) && (iIndex < MAX_HISTORY_VIDEO_INTERVALS) )
      {
         g_ControllerRetransmissionsStats.totalRequestedRetransmissionsLast500Ms += g_ControllerRetransmissionsStats.history[iIndex].uCountRequestedRetransmissions;
         g_ControllerRetransmissionsStats.totalReceivedRetransmissionsLast500Ms += g_ControllerRetransmissionsStats.history[iIndex].uCountAcknowledgedRetransmissions;
         dTime += g_ControllerRetransmissionsStats.refreshIntervalMs;
         iIndex++;
      }
   
      if ( g_ControllerRetransmissionsStats.history[0].uCountReceivedRetransmissionPackets > 0 )
         g_ControllerRetransmissionsStats.history[0].uAverageRetransmissionRoundtripTime /= g_ControllerRetransmissionsStats.history[0].uCountReceivedRetransmissionPackets;
        

      /*
      static u32 sTmpDebugTimeLastComputeRStats = 0;
      static int sTmpDebugLastRetrCount = 0;
      if ( g_ControllerRetransmissionsStats.iListRetransmissionsCount != sTmpDebugLastRetrCount ||
          ( g_ControllerRetransmissionsStats.iListRetransmissionsCount > 0 && ( g_TimeNow > sTmpDebugTimeLastComputeRStats+1000) ) )
      {
         sTmpDebugLastRetrCount = g_ControllerRetransmissionsStats.iListRetransmissionsCount;
         sTmpDebugTimeLastComputeRStats = g_TimeNow;
         log_line("DEBUG retransmission window: %d ms", s_iMilisecondsMaxRetransmissionWindow);
         for( int i=0; i<g_ControllerRetransmissionsStats.iListRetransmissionsCount; i++ )
         {
            log_line("DEBUG retransmission %d of %d: id: %u, segments: %d, received: %d, request time: %s, min response time: %d ms, max response time: %d ms",
             i+1, g_ControllerRetransmissionsStats.iListRetransmissionsCount,
              g_ControllerRetransmissionsStats.listRetransmissions[i].uRetransmissionId,
              g_ControllerRetransmissionsStats.listRetransmissions[i].uRequestedSegments,
              g_ControllerRetransmissionsStats.listRetransmissions[i].uReceivedSegments,
              str_format_time(g_ControllerRetransmissionsStats.listRetransmissions[i].uRequestTime),
              g_ControllerRetransmissionsStats.listRetransmissions[i].uMinResponseTime,
              g_ControllerRetransmissionsStats.listRetransmissions[i].uMaxResponseTime
              );
            for( int k=0; k<g_ControllerRetransmissionsStats.listRetransmissions[i].uRequestedSegments; k++ )
               if ( g_ControllerRetransmissionsStats.listRetransmissions[i].uReceivedSegmentCount[k] > 0 )
               log_line("DEBUG     segment %d [%u/%d]: received %d times, time recv: %d ms", k+1,
                  g_ControllerRetransmissionsStats.listRetransmissions[i].uRequestedVideoBlockIndex[k],
                  g_ControllerRetransmissionsStats.listRetransmissions[i].uRequestedVideoBlockPacketIndex[k],
                  g_ControllerRetransmissionsStats.listRetransmissions[i].uReceivedSegmentCount[k],
                  g_ControllerRetransmissionsStats.listRetransmissions[i].uReceivedSegmentTime[k] - g_ControllerRetransmissionsStats.listRetransmissions[i].uRequestTime
                  );
         }
      }
      */

      // Remove retransmissions that are too old (older than retransmission window) from the list

      int iRemoveToIndex = 0;
      int iRemoveIncompleteCount = 0;
      while ( ( iRemoveToIndex < g_ControllerRetransmissionsStats.iListRetransmissionsCount) &&
              ( g_ControllerRetransmissionsStats.listRetransmissions[iRemoveToIndex].uRequestTime < g_TimeNow - s_iMilisecondsMaxRetransmissionWindow ) )
      {
         if ( g_ControllerRetransmissionsStats.listRetransmissions[iRemoveToIndex].uReceivedSegments !=
              g_ControllerRetransmissionsStats.listRetransmissions[iRemoveToIndex].uRequestedSegments )
            iRemoveIncompleteCount++;
         iRemoveToIndex++;
      }
      if ( g_ControllerRetransmissionsStats.history[0].uCountDroppedRetransmissions < 254 - iRemoveIncompleteCount )
         g_ControllerRetransmissionsStats.history[0].uCountDroppedRetransmissions += iRemoveIncompleteCount;
      else
         g_ControllerRetransmissionsStats.history[0].uCountDroppedRetransmissions = 254;

      if ( (iRemoveToIndex > 0) && (iRemoveToIndex >= g_ControllerRetransmissionsStats.iListRetransmissionsCount) )
         g_ControllerRetransmissionsStats.iListRetransmissionsCount = 0;
      if ( (iRemoveToIndex > 0) && (iRemoveToIndex < g_ControllerRetransmissionsStats.iListRetransmissionsCount) )
      {
         for( int i=0; i<g_ControllerRetransmissionsStats.iListRetransmissionsCount - iRemoveToIndex; i++ )
         {
            memcpy((u8*)&(g_ControllerRetransmissionsStats.listRetransmissions[i]),
                   (u8*)&(g_ControllerRetransmissionsStats.listRetransmissions[i+iRemoveToIndex]),
                   sizeof(controller_retransmission_state)); 
         }
         g_ControllerRetransmissionsStats.iListRetransmissionsCount -= iRemoveToIndex;
      }
 
      memcpy((u8*)s_pSM_ControllerRetransmissionsStats, (u8*)&g_ControllerRetransmissionsStats, sizeof(shared_mem_controller_retransmissions_stats));
   
      for( int i=MAX_HISTORY_VIDEO_INTERVALS-1; i>0; i-- )
      {
         g_ControllerRetransmissionsStats.history[i].uCountRequestedRetransmissions = g_ControllerRetransmissionsStats.history[i-1].uCountRequestedRetransmissions;
         g_ControllerRetransmissionsStats.history[i].uCountAcknowledgedRetransmissions = g_ControllerRetransmissionsStats.history[i-1].uCountAcknowledgedRetransmissions;
         g_ControllerRetransmissionsStats.history[i].uCountCompletedRetransmissions = g_ControllerRetransmissionsStats.history[i-1].uCountCompletedRetransmissions;
         g_ControllerRetransmissionsStats.history[i].uCountDroppedRetransmissions = g_ControllerRetransmissionsStats.history[i-1].uCountDroppedRetransmissions;

         g_ControllerRetransmissionsStats.history[i].uMinRetransmissionRoundtripTime = g_ControllerRetransmissionsStats.history[i-1].uMinRetransmissionRoundtripTime;
         g_ControllerRetransmissionsStats.history[i].uMaxRetransmissionRoundtripTime = g_ControllerRetransmissionsStats.history[i-1].uMaxRetransmissionRoundtripTime;
         g_ControllerRetransmissionsStats.history[i].uAverageRetransmissionRoundtripTime = g_ControllerRetransmissionsStats.history[i-1].uAverageRetransmissionRoundtripTime;
         
         g_ControllerRetransmissionsStats.history[i].uCountRequestedPacketsForRetransmission = g_ControllerRetransmissionsStats.history[i-1].uCountRequestedPacketsForRetransmission;
         g_ControllerRetransmissionsStats.history[i].uCountReRequestedPacketsForRetransmission = g_ControllerRetransmissionsStats.history[i-1].uCountReRequestedPacketsForRetransmission;
         g_ControllerRetransmissionsStats.history[i].uCountReceivedRetransmissionPackets = g_ControllerRetransmissionsStats.history[i-1].uCountReceivedRetransmissionPackets;
         g_ControllerRetransmissionsStats.history[i].uCountReceivedRetransmissionPacketsDuplicate = g_ControllerRetransmissionsStats.history[i-1].uCountReceivedRetransmissionPacketsDuplicate;
         g_ControllerRetransmissionsStats.history[i].uCountReceivedRetransmissionPacketsDropped = g_ControllerRetransmissionsStats.history[i-1].uCountReceivedRetransmissionPacketsDropped;
      }

      g_ControllerRetransmissionsStats.history[0].uCountRequestedRetransmissions = 0;
      g_ControllerRetransmissionsStats.history[0].uCountAcknowledgedRetransmissions = 0;
      g_ControllerRetransmissionsStats.history[0].uCountCompletedRetransmissions = 0;
      g_ControllerRetransmissionsStats.history[0].uCountDroppedRetransmissions = 0;

      g_ControllerRetransmissionsStats.history[0].uCountRequestedPacketsForRetransmission = 0;
      g_ControllerRetransmissionsStats.history[0].uCountReRequestedPacketsForRetransmission = 0;
      g_ControllerRetransmissionsStats.history[0].uCountReceivedRetransmissionPackets = 0;
      g_ControllerRetransmissionsStats.history[0].uCountReceivedRetransmissionPacketsDuplicate = 0;
      g_ControllerRetransmissionsStats.history[0].uCountReceivedRetransmissionPacketsDropped = 0;
   }

   if ( g_TimeNow >= s_TimeLastVideoStatsUpdate+50 )
   {
      s_TimeLastVideoStatsUpdate = g_TimeNow;

      s_VDStatsCache.frames_type = radio_get_received_frames_type();
      s_VDStatsCache.iCurrentClockSyncType = g_pCurrentModel->clock_sync_type;

      if ( s_VDStatsCache.encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS )
         s_VDStatsCache.isRetransmissionsOn = 1;
      else
         s_VDStatsCache.isRetransmissionsOn = 0;

      if ( g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds != 0 )
      if ( g_TimeNow > g_uTimeLastReceivedResponseToAMessage + g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds )
         s_VDStatsCache.isRetransmissionsOn = 0;

      // Copy local copy to shared mem

      memcpy((u8*)s_pSM_VideoDecodeStats, (u8*)(&s_VDStatsCache), sizeof(shared_mem_video_decode_stats));
   }

}

bool process_data_rx_video_init()
{
   log_line("[VideoRX] Initializing...");
   bool bAllOk = true;

   g_fdLogFile = NULL;
   //g_fdLogFile = fopen(LOG_FILE_VIDEO, "a+");

   _rx_video_log_line("");
   _rx_video_log_line("");
   _rx_video_log_line("");
   _rx_video_log_line("============================");
   _rx_video_log_line("Starting video rx process...");

   fec_init();

   process_data_rx_video_reset_retransmission_stats();

   video_link_adaptive_init();
   video_link_keyframe_init();

   s_RetransmissionRetryTimeout = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log_line("Video RX: Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", s_RetransmissionRetryTimeout, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   
   s_pSM_VideoDecodeStats = shared_mem_video_decode_stats_open(false);
   if ( NULL == s_pSM_VideoDecodeStats )
   {
      log_softerror_and_alarm("Failed to open video decoder stats shared memory for read/write.");
      bAllOk = false;
   }
   else
      log_line("Opened video decoder stats shared memory for read/write: success.");

   s_pSM_VideoDecodeStatsHistory = shared_mem_video_decode_stats_history_open(false);
   if ( NULL == s_pSM_VideoDecodeStatsHistory )
   {
      log_softerror_and_alarm("Failed to open video decoder stats history shared memory for read/write.");
      bAllOk = false;
   }
   else
      log_line("Opened video decoder stats history shared memory for read/write: success.");

   s_pSM_ControllerRetransmissionsStats = shared_mem_controller_video_retransmissions_stats_open_for_write();
   if ( NULL == s_pSM_ControllerRetransmissionsStats )
   {
      log_softerror_and_alarm("Failed to open video retransmissions stats shared memory for read/write.");
      bAllOk = false;
   }
   else
      log_line("Opened video retransmissions stats shared memory for read/write: success.");

   s_VDStatsCache.isRecording = 0;
   s_VDStatsCache.frames_type = RADIO_FLAGS_FRAME_TYPE_DATA;
   s_VDStatsCache.width = s_VDStatsCache.height = 0;
   s_VDStatsCache.total_DiscardedLostPackets = 0;
   s_VDStatsCache.totalEncodingSwitches = 0;

   if ( NULL != g_pCurrentModel )
   {
      s_VDStatsCache.video_type = 0;
      s_VDStatsCache.video_link_profile = g_pCurrentModel->video_params.user_selected_video_link_profile | (g_pCurrentModel->video_params.user_selected_video_link_profile<<4);
      s_VDStatsCache.encoding_extra_flags = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags;

      s_VDStatsCache.data_packets_per_block = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_packets;
      s_VDStatsCache.fec_packets_per_block = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_fecs;
      s_VDStatsCache.video_packet_length = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].packet_length;
      s_VDStatsCache.fec_time = 0;
      s_VDStatsCache.fps = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps;
      s_VDStatsCache.keyframe = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe;
      s_VDStatsCache.width = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width;
      s_VDStatsCache.height = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height;
   }
   else
   {
      if ( g_bSearching )
         log_softerror_and_alarm("Video RX: Started video rx in search mode.");
      else
         log_softerror_and_alarm("Started RX Video with no model configured or present!");

      memset(&s_VDStatsCache, 0, sizeof(shared_mem_video_decode_stats));

      s_VDStatsCache.video_type = 0;
      s_VDStatsCache.video_link_profile = VIDEO_PROFILE_BEST_PERF | (VIDEO_PROFILE_BEST_PERF<<4);
      s_VDStatsCache.encoding_extra_flags = ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO | ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
      s_VDStatsCache.encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
      s_VDStatsCache.data_packets_per_block = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
      s_VDStatsCache.fec_packets_per_block = DEFAULT_VIDEO_BLOCK_FECS_HP;
      s_VDStatsCache.video_packet_length = DEFAULT_VIDEO_PACKET_LENGTH_HP;
      s_VDStatsCache.fps = 30;
      s_VDStatsCache.keyframe = 0;
      s_VDStatsCache.width = 1280;
      s_VDStatsCache.height = 720;   
   }

   g_VideoDecodeStatsHistory.outputHistoryIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;
   g_ControllerRetransmissionsStats.refreshIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;
   _rx_video_log_line("Using graphs slice interval of %d miliseconds.", g_VideoDecodeStatsHistory.outputHistoryIntervalMs);
   log_line("ProcessorRXVideo: Using graphs slice interval of %d miliseconds.", g_VideoDecodeStatsHistory.outputHistoryIntervalMs);

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   {
      s_pRXBlocksStack[i] = (type_received_block_info*)malloc(sizeof(type_received_block_info));
      for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
         s_pRXBlocksStack[i]->packetsInfo[k].pData = (u8*)malloc(MAX_PACKET_PAYLOAD+1);
   }
   _rx_video_log_line("Allocated %u Mb for rx video caching", (u32)MAX_RXTX_BLOCKS_BUFFER*(u32)MAX_TOTAL_PACKETS_IN_BLOCK*(u32)MAX_PACKET_PAYLOAD/(u32)1000/(u32)1000);
   log_line("ProcessorRXVideo: Allocated %u Mb for rx video caching", (u32)MAX_RXTX_BLOCKS_BUFFER*(u32)MAX_TOTAL_PACKETS_IN_BLOCK*(u32)MAX_PACKET_PAYLOAD/(u32)1000/(u32)1000);

   _rx_video_reset_receive_state();
   s_VDStatsCache.total_DiscardedBuffers = 0;

   processor_rx_video_forward_init();
  
   _rx_video_log_line("");
   _rx_video_log_line("=======================");
   _rx_video_log_line("Started video decoding.");
   _rx_video_log_line("=======================");
   _rx_video_log_line("");
   _rx_video_log_line("");

   log_line("[VideoRX] Initialize Complete. Succeeded: %s.", bAllOk?"yes":"no");

   return bAllOk;
}

bool process_data_rx_video_uninit()
{
   processor_rx_video_forward_uninit();

   
   shared_mem_video_decode_stats_close(s_pSM_VideoDecodeStats);
   shared_mem_video_decode_stats_history_close(s_pSM_VideoDecodeStatsHistory);
   shared_mem_controller_video_retransmissions_stats_close(s_pSM_ControllerRetransmissionsStats);
   s_pSM_ControllerRetransmissionsStats = NULL;

   for( int i=0; i<MAX_RXTX_BLOCKS_BUFFER; i++ )
   {
      for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
         free(s_pRXBlocksStack[i]->packetsInfo[k].pData);
      free(s_pRXBlocksStack[i]);
   }
   log_line("Video rx processor stopped.");

   if ( g_fdLogFile > 0 )
      fclose(g_fdLogFile);
   g_fdLogFile = NULL;
   
   return true;
}

void process_data_rx_video_loop()
{
   rx_video_periodic_checks();

   if ( s_VDStatsCache.encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS )
   {
      if ( s_LastReceivedVideoPacketInfo.video_block_index != MAX_U32 )
         _check_and_request_missing_packets();
   }

   // Can we output the first few blocks?

   int maxBlocksToOutputIfAvailable = MAX_BLOCKS_TO_OUTPUT_IF_AVAILABLE;
   do
   {
      if ( s_RXBlocksStackTopIndex < 0 )
         break;
      if ( s_pRXBlocksStack[0]->data_packets == 0 )
         break;
      if ( s_pRXBlocksStack[0]->received_data_packets + s_pRXBlocksStack[0]->received_fec_packets < s_pRXBlocksStack[0]->data_packets )
         break;

      _push_first_block_out();
      maxBlocksToOutputIfAvailable--;
   }
   while ( maxBlocksToOutputIfAvailable > 0 );

   _check_and_discard_too_old_blocks();
}

void process_data_rx_video_reset_state()
{
   //u32 tmp1 = s_LastHardEncodingsChangeVideoBlockIndex;
   //u32 tmp2 = s_LastVideoResolutionChangeVideoBlockIndex;
   _rx_video_log_line("Received request to reset video rx state");
   _rx_video_reset_receive_state();
   s_VDStatsCache.total_DiscardedBuffers--;
   //s_LastHardEncodingsChangeVideoBlockIndex = tmp1;
   //s_LastVideoResolutionChangeVideoBlockIndex = tmp2;

   video_line_adaptive_switch_to_med_level();
}

// Returns 1 if a video block has just finished and the flag "Can TX" is set

int process_data_rx_video_on_new_packet(int interfaceNb, u8* pBuffer, int length)
{
   #ifdef PROFILE_RX
   u32 uTimeStart = get_current_timestamp_ms();
   int iStackIndexStart = s_RXBlocksStackTopIndex;
   t_packet_header_video_full* pPHVFTemp = (t_packet_header_video_full*) (pBuffer+sizeof(t_packet_header));
   #endif

   _check_and_discard_too_old_blocks();

   #ifdef PROFILE_RX
   u32 dTime1 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime1 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Discarding old blocks took too long: %u ms. Stack top before/after: %d/%d", pPHVFTemp->video_block_index , pPHVFTemp->video_block_packet_index, interfaceNb, length, dTime1, iStackIndexStart, s_RXBlocksStackTopIndex);
   #endif

   int packetsGap = _preprocess_video_packet(interfaceNb, pBuffer, length);

   #ifdef PROFILE_RX
   u32 dTime2 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime2 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Preprocessing packet took too long: %u ms. Stack top before/after: %d/%d", pPHVFTemp->video_block_index , pPHVFTemp->video_block_packet_index, interfaceNb, length, dTime2, iStackIndexStart, s_RXBlocksStackTopIndex);
   #endif

   if ( packetsGap < 0 )
      return 0;

   t_packet_header* pPH = (t_packet_header*) pBuffer;
   t_packet_header_video_full* pPVF = (t_packet_header_video_full*) (pBuffer+sizeof(t_packet_header));

   int nReturnCanTx = 0;

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_CAN_START_TX )
   if ( !( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED) )
   if ( pPVF->video_block_packet_index >= pPVF->block_packets+pPVF->block_fecs-1 )
      nReturnCanTx = 1;

   int added_to_rx_buffer_index = -1;

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
      added_to_rx_buffer_index = _process_retransmitted_video_packet(pBuffer, length);
   else
      added_to_rx_buffer_index = _process_video_packet(pBuffer, length);

   if ( added_to_rx_buffer_index != 1 )
   {
   if ( pPH->packet_flags & PACKET_FLAGS_BIT_RETRANSMITED )
   {
      if ( g_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[0] < 254 )
         g_VideoDecodeStatsHistory.outputHistoryReceivedVideoRetransmittedPackets[0]++;
   }
   else
   {
      if ( g_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[0] < 254 )
         g_VideoDecodeStatsHistory.outputHistoryReceivedVideoPackets[0]++;
   }
   }

   #ifdef PROFILE_RX
   u32 dTime3 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime3 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Adding packet to buffers took too long: %u ms. Stack top before/after: %d/%d", pPHVFTemp->video_block_index , pPHVFTemp->video_block_packet_index, interfaceNb, length, dTime3, iStackIndexStart, s_RXBlocksStackTopIndex);
   #endif

   if ( -1 == added_to_rx_buffer_index )
      return nReturnCanTx;

   // Can we output the first few blocks?
   int maxBlocksToOutputIfAvailable = MAX_BLOCKS_TO_OUTPUT_IF_AVAILABLE;
   do
   {
      if ( s_RXBlocksStackTopIndex < 0 )
         break;
      if ( s_pRXBlocksStack[0]->data_packets == 0 )
         break;
      if ( s_pRXBlocksStack[0]->received_data_packets + s_pRXBlocksStack[0]->received_fec_packets < s_pRXBlocksStack[0]->data_packets )
         break;

      _push_first_block_out();
      nReturnCanTx = 1;
      maxBlocksToOutputIfAvailable--;
   }
   while ( maxBlocksToOutputIfAvailable > 0 );

   #ifdef PROFILE_RX
   u32 dTime4 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime4 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Sending video blocks to player took too long: %u ms. Stack top before/after: %d/%d", pPHVFTemp->video_block_index , pPHVFTemp->video_block_packet_index, interfaceNb, length, dTime4, iStackIndexStart, s_RXBlocksStackTopIndex);
   #endif

   // Output it anyway if not using bidirectional video and we are past the first block

   bool bOutputBocksAsIs = false;
   if ( !( s_VDStatsCache.encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS ) )
      bOutputBocksAsIs = true;
   if ( g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds != 0 )
   if ( g_TimeNow > g_uTimeLastReceivedResponseToAMessage + g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds )
      bOutputBocksAsIs = true;

   if ( bOutputBocksAsIs )
   if ( s_RXBlocksStackTopIndex > 0 )
   {
      //if ( !( s_VDStatsCache.encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS ) )
      //   log_line("DEBUG output first block due to one way link");
      //else
      //   log_line("DEBUG output first block due to no controller link, last reply time: %u, link timeout: %u ms", g_uTimeLastReceivedResponseToAMessage, g_pControllerSettings->iDisableRetransmissionsAfterControllerLinkLostMiliseconds );
      //_log_current_buffer(true);

      _push_first_block_out();
      nReturnCanTx = 1;

      #ifdef PROFILE_RX
      u32 dTime5 = get_current_timestamp_ms() - uTimeStart;
      if ( dTime5 >= PROFILE_RX_MAX_TIME )
         log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Force pushing video blocks to player took too long: %u ms. Stack top before/after: %d/%d", pPHVFTemp->video_block_index , pPHVFTemp->video_block_packet_index, interfaceNb, length, dTime5, iStackIndexStart, s_RXBlocksStackTopIndex);
      #endif

      return nReturnCanTx;
   }

   #ifdef PROFILE_RX
   u32 dTime6 = get_current_timestamp_ms() - uTimeStart;
   if ( dTime6 >= PROFILE_RX_MAX_TIME )
      log_softerror_and_alarm("[Profile-Rx] Video processing video packet [%u/%u], interface: %d, len: %d bytes: Total processing took too long: %u ms. Stack top before/after: %d/%d", pPHVFTemp->video_block_index , pPHVFTemp->video_block_packet_index, interfaceNb, length, dTime6, iStackIndexStart, s_RXBlocksStackTopIndex);
   #endif

   return nReturnCanTx;
}

void process_data_rx_video_reset_retransmission_stats()
{
   _rx_video_log_line("Reset retransmissions stats.");
   log_line("[VideoRX] Resetting retransmissions stats...");
   g_ControllerRetransmissionsStats.refreshIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;
   
   g_ControllerRetransmissionsStats.totalRequestedRetransmissions = 0;
   g_ControllerRetransmissionsStats.totalRequestedRetransmissionsLast500Ms = 0;

   g_ControllerRetransmissionsStats.totalReceivedRetransmissions = 0;
   g_ControllerRetransmissionsStats.totalReceivedRetransmissionsLast500Ms = 0;

   g_ControllerRetransmissionsStats.totalRequestedSegments = 0;
   g_ControllerRetransmissionsStats.totalReceivedSegments = 0;

   g_ControllerRetransmissionsStats.iListRetransmissionsCount = 0;
   
   for( int i=0; i<MAX_HISTORY_VIDEO_INTERVALS; i++ )
   {
      g_ControllerRetransmissionsStats.history[i].uCountRequestedRetransmissions = 0;
      g_ControllerRetransmissionsStats.history[i].uCountAcknowledgedRetransmissions = 0;
      g_ControllerRetransmissionsStats.history[i].uCountDroppedRetransmissions = 0;
      g_ControllerRetransmissionsStats.history[i].uCountRequestedPacketsForRetransmission = 0;
      g_ControllerRetransmissionsStats.history[i].uCountReRequestedPacketsForRetransmission = 0;
      g_ControllerRetransmissionsStats.history[i].uCountReceivedRetransmissionPackets = 0;
      g_ControllerRetransmissionsStats.history[i].uCountReceivedRetransmissionPacketsDuplicate = 0;
      g_ControllerRetransmissionsStats.history[i].uCountReceivedRetransmissionPacketsDropped = 0;

      g_ControllerRetransmissionsStats.history[i].uMinRetransmissionRoundtripTime = 0;
      g_ControllerRetransmissionsStats.history[i].uMaxRetransmissionRoundtripTime = 0;
      g_ControllerRetransmissionsStats.history[i].uAverageRetransmissionRoundtripTime = 0;
   }

   if ( NULL != s_pSM_ControllerRetransmissionsStats )
      memcpy((u8*)s_pSM_ControllerRetransmissionsStats, (u8*)&g_ControllerRetransmissionsStats, sizeof(shared_mem_controller_retransmissions_stats));
   
   s_VDStatsCache.total_DiscardedLostPackets = 0;
   s_VDStatsCache.total_DiscardedSegments = 0;
   s_VDStatsCache.total_DiscardedBuffers = 0;

   log_line("[VideoRX] Resetting retransmissions stats: Completed.");
}

void process_data_rx_video_on_controller_settings_changed()
{   
   log_line("Controller params changed. Reinitializing RX video state...");
   _rx_video_log_line("Controller params changed. Reinitializing RX video state...");

   u32 tmp1 = s_LastHardEncodingsChangeVideoBlockIndex;
   u32 tmp2 = s_LastVideoResolutionChangeVideoBlockIndex;

   g_VideoDecodeStatsHistory.outputHistoryIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;
   g_ControllerRetransmissionsStats.refreshIntervalMs = g_pControllerSettings->nGraphVideoRefreshInterval;
   _rx_video_log_line("Using graphs slice interval of %d miliseconds.", g_VideoDecodeStatsHistory.outputHistoryIntervalMs);
   log_line("ProcessorRXVideo: Using graphs slice interval of %d miliseconds.", g_VideoDecodeStatsHistory.outputHistoryIntervalMs);

   s_RetransmissionRetryTimeout = g_pControllerSettings->nRetryRetransmissionAfterTimeoutMS;
   log_line("Video RX: Using timers: Retransmission retry after timeout of %d ms; Request retransmission after video silence (no video packets) timeout of %d ms", s_RetransmissionRetryTimeout, g_pControllerSettings->nRequestRetransmissionsOnVideoSilenceMs);
   _rx_video_reset_receive_state();
   s_VDStatsCache.total_DiscardedBuffers--;

   s_LastHardEncodingsChangeVideoBlockIndex = tmp1;
   s_LastVideoResolutionChangeVideoBlockIndex = tmp2;
}

int process_data_rx_video_get_current_received_video_profile()
{
   if ( 0 == s_VDStatsCache.width )
      return -1;
   return (s_VDStatsCache.video_link_profile & 0x0F);
}

int process_data_rx_video_get_current_received_video_fps()
{
   if ( 0 == s_VDStatsCache.width )
      return -1;
   return s_VDStatsCache.fps;
}

int process_data_rx_video_get_current_received_video_keyframe()
{
   if ( 0 == s_VDStatsCache.width )
      return -1;
   return s_VDStatsCache.keyframe;
}

int process_data_rx_video_get_missing_video_packets_now()
{
   int iCount = 0;
   for( int i=0; i<s_RXBlocksStackTopIndex; i++ )
   {
      iCount += s_pRXBlocksStack[i]->data_packets + s_pRXBlocksStack[i]->fec_packets - (s_pRXBlocksStack[i]->received_data_packets + s_pRXBlocksStack[i]->received_fec_packets);
   }
   return iCount;
}