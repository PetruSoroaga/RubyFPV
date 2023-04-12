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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "base.h"
#include "shared_mem.h"
#include "../radio/radiopackets2.h"

void* open_shared_mem(const char* name, int size, int readOnly)
{
   int fd;
   if ( readOnly )
      fd = shm_open(name, O_RDONLY, S_IRUSR | S_IWUSR);
   else
      fd = shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

   if(fd < 0)
   {
       log_softerror_and_alarm("[SharedMem] Failed to open shared memory: %s, error: %s", name, strerror(errno));
       return NULL;
   }
   if ( ! readOnly )
   {
      if (ftruncate(fd, size) == -1)
      {
          log_softerror_and_alarm("[SharedMem] Failed to init (ftruncate) shared memory for writing: %s", name);
          close(fd);
          return NULL;   
      }
   }
   void *retval = NULL;
   if ( readOnly )
      retval = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
   else
      retval = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (retval == MAP_FAILED)
   {
      log_softerror_and_alarm("[SharedMem] Failed to map shared memory: %s", name);
      close(fd);
      return NULL;
   }

   if ( ! readOnly )
      memset(retval, 0, size);

   close(fd);

   log_line("[SharedMem] Opened shared memory object %s in %s mode.", name, readOnly?"read":"write");
   return retval;
}

void* open_shared_mem_for_write(const char* name, int size)
{
   return open_shared_mem(name, size, 0);
}

void* open_shared_mem_for_read(const char* name, int size)
{
   return open_shared_mem(name, size, 1);
}

shared_mem_process_stats* shared_mem_process_stats_open_read(const char* szName)
{
   void *retVal =  open_shared_mem(szName, sizeof(shared_mem_process_stats), 1);
   shared_mem_process_stats *tretval = (shared_mem_process_stats*)retVal;
   return tretval;
}

shared_mem_process_stats* shared_mem_process_stats_open_write(const char* szName)
{
   void *retVal =  open_shared_mem(szName, sizeof(shared_mem_process_stats), 0);
   shared_mem_process_stats *tretval = (shared_mem_process_stats*)retVal;
   memset( tretval, 0, sizeof(shared_mem_process_stats));
   return tretval;
}

void shared_mem_process_stats_close(const char* szName, shared_mem_process_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_process_stats));
   //shm_unlink(szName);
}

void process_stats_reset(shared_mem_process_stats* pStats, u32 timeNow)
{
   if ( NULL == pStats )
      return;

   pStats->lastActiveTime = timeNow;
   pStats->lastRadioTxTime = timeNow;
   pStats->lastRadioRxTime = timeNow;
   pStats->lastIPCIncomingTime = timeNow;
   pStats->lastIPCOutgoingTime = timeNow;
}

void process_stats_mark_active(shared_mem_process_stats* pStats, u32 timeNow)
{
   if ( NULL == pStats )
      return;

   pStats->lastActiveTime = timeNow;
}


shared_mem_controller_vehicles_adaptive_video_info* shared_mem_controller_vehicles_adaptive_video_info_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_CONTROLLER_ADAPTIVE_VIDEO_INFO, sizeof(shared_mem_controller_vehicles_adaptive_video_info));
   return (shared_mem_controller_vehicles_adaptive_video_info*)retVal;
}

shared_mem_controller_vehicles_adaptive_video_info* shared_mem_controller_vehicles_adaptive_video_info_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_CONTROLLER_ADAPTIVE_VIDEO_INFO, sizeof(shared_mem_controller_vehicles_adaptive_video_info));
   return (shared_mem_controller_vehicles_adaptive_video_info*)retVal;
}

void shared_mem_controller_vehicles_adaptive_video_info_close(shared_mem_controller_vehicles_adaptive_video_info* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_controller_vehicles_adaptive_video_info));
   //shm_unlink(szName);
}

shared_mem_radio_stats* shared_mem_radio_stats_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_RADIO_STATS, sizeof(shared_mem_radio_stats));
   return (shared_mem_radio_stats*)retVal;
}

shared_mem_radio_stats* shared_mem_radio_stats_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_RADIO_STATS, sizeof(shared_mem_radio_stats));
   return (shared_mem_radio_stats*)retVal;
}

void shared_mem_radio_stats_close(shared_mem_radio_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_radio_stats));
   //shm_unlink(szName);
}


shared_mem_video_info_stats* shared_mem_video_info_stats_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_VIDEO_STREAM_INFO_STATS , sizeof(shared_mem_video_info_stats));
   return (shared_mem_video_info_stats*)retVal;
}

shared_mem_video_info_stats* shared_mem_video_info_stats_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_VIDEO_STREAM_INFO_STATS , sizeof(shared_mem_video_info_stats));
   return (shared_mem_video_info_stats*)retVal;
}

void shared_mem_video_info_stats_close(shared_mem_video_info_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_info_stats));
}


shared_mem_video_info_stats* shared_mem_video_info_stats_radio_in_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_VIDEO_STREAM_INFO_STATS_RADIO_IN , sizeof(shared_mem_video_info_stats));
   return (shared_mem_video_info_stats*)retVal;
}

shared_mem_video_info_stats* shared_mem_video_info_stats_radio_in_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_VIDEO_STREAM_INFO_STATS_RADIO_IN , sizeof(shared_mem_video_info_stats));
   return (shared_mem_video_info_stats*)retVal;
}

void shared_mem_video_info_stats_radio_in_close(shared_mem_video_info_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_info_stats));
}

shared_mem_video_info_stats* shared_mem_video_info_stats_radio_out_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_VIDEO_STREAM_INFO_STATS_RADIO_OUT , sizeof(shared_mem_video_info_stats));
   return (shared_mem_video_info_stats*)retVal;
}

shared_mem_video_info_stats* shared_mem_video_info_stats_radio_out_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_VIDEO_STREAM_INFO_STATS_RADIO_OUT , sizeof(shared_mem_video_info_stats));
   return (shared_mem_video_info_stats*)retVal;
}

void shared_mem_video_info_stats_radio_out_close(shared_mem_video_info_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_info_stats));
}

shared_mem_video_link_stats_and_overwrites* shared_mem_video_link_stats_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_VIDEO_LINK_STATS , sizeof(shared_mem_video_link_stats_and_overwrites));
   return (shared_mem_video_link_stats_and_overwrites*)retVal;
}

shared_mem_video_link_stats_and_overwrites* shared_mem_video_link_stats_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_VIDEO_LINK_STATS , sizeof(shared_mem_video_link_stats_and_overwrites));
   return (shared_mem_video_link_stats_and_overwrites*)retVal;
}

void shared_mem_video_link_stats_close(shared_mem_video_link_stats_and_overwrites* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_link_stats_and_overwrites));
}

shared_mem_video_link_graphs* shared_mem_video_link_graphs_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_VIDEO_LINK_GRAPHS , sizeof(shared_mem_video_link_graphs));
   return (shared_mem_video_link_graphs*)retVal;
}

shared_mem_video_link_graphs* shared_mem_video_link_graphs_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_VIDEO_LINK_GRAPHS , sizeof(shared_mem_video_link_graphs));
   return (shared_mem_video_link_graphs*)retVal;
}

void shared_mem_video_link_graphs_close(shared_mem_video_link_graphs* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_link_graphs));
}

shared_mem_video_decode_stats* shared_mem_video_decode_stats_open(int readOnly)
{
   void *retVal =  open_shared_mem(SHARED_MEM_VIDEO_DECODE_STATS, sizeof(shared_mem_video_decode_stats), readOnly);
   shared_mem_video_decode_stats *tretval = (shared_mem_video_decode_stats*)retVal;
   return tretval;
}
void shared_mem_video_decode_stats_close(shared_mem_video_decode_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_decode_stats));
   //shm_unlink(SHARED_MEM_VIDEO_DECODE_STATS);
}


shared_mem_video_decode_stats_history* shared_mem_video_decode_stats_history_open(int readOnly)
{
   void *retVal =  open_shared_mem(SHARED_MEM_VIDEO_DECODE_STATS_HISTORY, sizeof(shared_mem_video_decode_stats_history), readOnly);
   shared_mem_video_decode_stats_history *tretval = (shared_mem_video_decode_stats_history*)retVal;
   return tretval;
}

void shared_mem_video_decode_stats_history_close(shared_mem_video_decode_stats_history* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_decode_stats_history));
   //shm_unlink(SHARED_MEM_VIDEO_DECODE_STATS_HISTORY);
}

shared_mem_router_packets_stats_history* shared_mem_router_packets_stats_history_open_read()
{
   void *retVal =  open_shared_mem(SHARED_MEM_ROUTER_PACKETS_STATS_HISTORY, sizeof(shared_mem_router_packets_stats_history), 1);
   shared_mem_router_packets_stats_history *tretval = (shared_mem_router_packets_stats_history*)retVal;
   return tretval;
}

shared_mem_router_packets_stats_history* shared_mem_router_packets_stats_history_open_write()
{
   void *retVal =  open_shared_mem(SHARED_MEM_ROUTER_PACKETS_STATS_HISTORY, sizeof(shared_mem_router_packets_stats_history), 0);
   shared_mem_router_packets_stats_history *tretval = (shared_mem_router_packets_stats_history*)retVal;
   return tretval;
}

void shared_mem_router_packets_stats_history_close(shared_mem_router_packets_stats_history* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_router_packets_stats_history));
   //shm_unlink(SHARED_MEM_ROUTER_PACKETS_STATS_HISTORY);
}

shared_mem_controller_retransmissions_stats* shared_mem_controller_video_retransmissions_stats_open_for_read()
{
   void *retVal = open_shared_mem(SHARED_MEM_VIDEO_RETRANSMISSIONS_STATS, sizeof(shared_mem_controller_retransmissions_stats), 1);
   return (shared_mem_controller_retransmissions_stats*)retVal;
}

shared_mem_controller_retransmissions_stats* shared_mem_controller_video_retransmissions_stats_open_for_write()
{
   void *retVal = open_shared_mem(SHARED_MEM_VIDEO_RETRANSMISSIONS_STATS, sizeof(shared_mem_controller_retransmissions_stats), 0);
   return (shared_mem_controller_retransmissions_stats*)retVal;
}

void shared_mem_controller_video_retransmissions_stats_close(shared_mem_controller_retransmissions_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_controller_retransmissions_stats));
}

t_packet_header_rc_info_downstream* shared_mem_rc_downstream_info_open_read()
{
   void *retVal =  open_shared_mem(SHARED_MEM_RC_DOWNLOAD_INFO, sizeof(t_packet_header_rc_info_downstream), 1);
   t_packet_header_rc_info_downstream *tretval = (t_packet_header_rc_info_downstream*)retVal;
   return tretval;
}

t_packet_header_rc_info_downstream* shared_mem_rc_downstream_info_open_write()
{
   void *retVal =  open_shared_mem(SHARED_MEM_RC_DOWNLOAD_INFO, sizeof(t_packet_header_rc_info_downstream), 0);
   t_packet_header_rc_info_downstream *tretval = (t_packet_header_rc_info_downstream*)retVal;
   return tretval;
}

void shared_mem_rc_downstream_info_close(t_packet_header_rc_info_downstream* pRCInfo)
{
   if ( NULL != pRCInfo )
      munmap(pRCInfo, sizeof(t_packet_header_rc_info_downstream));
   //shm_unlink(SHARED_MEM_RC_DOWNLOAD_INFO);
}


t_packet_header_rc_full_frame_upstream* shared_mem_rc_upstream_frame_open_read()
{
   void *retVal =  open_shared_mem(SHARED_MEM_RC_UPSTREAM_FRAME, sizeof(t_packet_header_rc_full_frame_upstream), 1);
   t_packet_header_rc_full_frame_upstream *tretval = (t_packet_header_rc_full_frame_upstream*)retVal;
   return tretval;
}

t_packet_header_rc_full_frame_upstream* shared_mem_rc_upstream_frame_open_write()
{
   void *retVal =  open_shared_mem(SHARED_MEM_RC_UPSTREAM_FRAME, sizeof(t_packet_header_rc_full_frame_upstream), 0);
   t_packet_header_rc_full_frame_upstream *tretval = (t_packet_header_rc_full_frame_upstream*)retVal;
   return tretval;
}

void shared_mem_rc_upstream_frame_close(t_packet_header_rc_full_frame_upstream* pRCInfo)
{
   if ( NULL != pRCInfo )
      munmap(pRCInfo, sizeof(t_packet_header_rc_full_frame_upstream));
   //shm_unlink(SHARED_MEM_RC_UPSTREAM_FRAME);
}


void add_detailed_history_rx_packets(shared_mem_router_packets_stats_history* pSMRPSH, int timeNowMs, int countVideo, int countRetransmited, int countTelemetry, int countRC, int countPing, int countOther, int dataRate)
{
   if ( NULL == pSMRPSH )
      return;

   while( pSMRPSH->lastMilisecond != timeNowMs )
   {
      pSMRPSH->lastMilisecond++;
      if ( pSMRPSH->lastMilisecond == 1000 )
         pSMRPSH->lastMilisecond = 0;
      pSMRPSH->rxHistoryPackets[pSMRPSH->lastMilisecond] = 0;
      pSMRPSH->txHistoryPackets[pSMRPSH->lastMilisecond] = 0;
   }

   u16 val = pSMRPSH->rxHistoryPackets[pSMRPSH->lastMilisecond];
   if ( countVideo > 0 )
   if ( (val & 0x07) != 0x07 )
      val = (val & (~0x07)) | ((val & 0x07)+1);

   if ( countRetransmited > 0 )
   if ( ((val>>3) & 0x07) != 0x07 )
      val = (val & (~(0x07<<3))) | ((((val>>3) & 0x07)+1)<<3);

   if ( countPing > 0 )
   if ( ((val>>6) & 0x01) != 0x01 )
      val = (val & (~(0x01<<6))) | ((((val>>6) & 0x01)+1)<<6);

   if ( countTelemetry > 0 )
   if ( ((val>>7) & 0x03) != 0x03 )
      val = (val & (~(0x03<<7))) | ((((val>>7) & 0x03)+1)<<7);

   if ( countRC > 0 )
   if ( ((val>>9) & 0x03) != 0x03 )
      val = (val & (~(0x03<<9))) | ((((val>>9) & 0x03)+1)<<9);

   if ( countOther > 0 )
   if ( ((val>>11) & 0x03) != 0x03 )
      val = (val & (~(0x03<<11))) | ((((val>>11) & 0x03)+1)<<11);

   if ( countRetransmited > 0 && dataRate > 0 )
   {
      u32 nRateIndex = 7;
      int *pRates = getDataRates();
      for ( int i=0; i<getDataRatesCount(); i++ )
      if ( pRates[i] == dataRate )
      {
         nRateIndex = (u32)i;
         break;
      }
      if ( nRateIndex > 7 )
         nRateIndex = 7;
      val = (val & (~(0x07<<13))) | (nRateIndex<<13);
   }

   pSMRPSH->rxHistoryPackets[pSMRPSH->lastMilisecond] = val;
}

void add_detailed_history_tx_packets(shared_mem_router_packets_stats_history* pSMRPSH, int timeNowMs, int countRetransmited, int countComands, int countPing, int countRC, int countOther, int dataRate)
{
   if ( NULL == pSMRPSH )
      return;

   while( pSMRPSH->lastMilisecond != timeNowMs )
   {
      pSMRPSH->lastMilisecond++;
      if ( pSMRPSH->lastMilisecond == 1000 )
         pSMRPSH->lastMilisecond = 0;
      pSMRPSH->rxHistoryPackets[pSMRPSH->lastMilisecond] = 0;
      pSMRPSH->txHistoryPackets[pSMRPSH->lastMilisecond] = 0;
   }
      
   u16 val = pSMRPSH->txHistoryPackets[pSMRPSH->lastMilisecond];
   if ( countRetransmited > 0 )
   if ( (val & 0x07) != 0x07 )
      val++;

   if ( countComands > 0 )
   if ( ((val>>3) & 0x07) != 0x07 )
      val = (val & (~(0x07<<3))) | ((((val>>3) & 0x07)+1)<<3);

   if ( countPing > 0 )
   if ( ((val>>6) & 0x03) != 0x03 )
      val = (val & (~(0x03<<6))) | ((((val>>6) & 0x03)+1)<<6);

   if ( countRC > 0 )
   if ( ((val>>8) & 0x07) != 0x07 )
      val = (val & (~(0x07<<8))) | ((((val>>8) & 0x07)+1)<<8);

   if ( countOther > 0 )
   if ( ((val>>11) & 0x03) != 0x03 )
      val = (val & (~(0x04<<11))) | ((((val>>11) & 0x03)+1)<<11);

   if ( countRetransmited > 0 && dataRate > 0 )
   {
      u32 nRateIndex = 7;
      int *pRates = getDataRates();
      for ( int i=0; i<getDataRatesCount(); i++ )
         if ( pRates[i] == dataRate )
         {
            nRateIndex = (u32)i;
            break;
         }
      if ( nRateIndex > 7 )
         nRateIndex = 7;
      val = (val & (~(0x07<<13))) | (nRateIndex<<13);
   }
   pSMRPSH->txHistoryPackets[pSMRPSH->lastMilisecond] = val;
}


void update_shared_mem_video_info_stats(shared_mem_video_info_stats* pSMVIStats, u32 uTimeNow)
{
   if ( NULL == pSMVIStats )
      return;
    
   pSMVIStats->uTimeLastUpdate = uTimeNow;

   u32 uMinFrameTime = MAX_U32;
   u32 uMaxFrameTime = 0;

   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
      if ( pSMVIStats->uFramesTimes[i] > uMaxFrameTime )
         uMaxFrameTime = pSMVIStats->uFramesTimes[i];
      if ( pSMVIStats->uFramesTimes[i] < uMinFrameTime )
         uMinFrameTime = pSMVIStats->uFramesTimes[i];
   }

   pSMVIStats->uAverageFPS = 0;
   pSMVIStats->uAverageFrameTime = 0;
   pSMVIStats->uAverageFrameSize = 0;

   u32 uSumTime = 0;
   u32 uSumSizes = 0;
   u32 uSumFramesCount = 0;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
      //if ( pSMVIStats->uFramesTimes[i] == 0 || pSMVIStats->uFramesTimes[i] == uMinFrame || pSMVIStats->uFramesTimes[i] == uMaxFrame )
      //   continue;
      if ( pSMVIStats->uFramesTimes[i] == 0 || (pSMVIStats->uFramesTypesAndSizes[i] & (1<<7)) )
         continue;
      uSumTime += pSMVIStats->uFramesTimes[i];
      uSumSizes += ((u32)(pSMVIStats->uFramesTypesAndSizes[i] & 0x7F)) * 8000;
      uSumFramesCount++;
   }

   if ( 0 < uSumFramesCount && 0 < uSumTime )
   {
      pSMVIStats->uAverageFPS = uSumFramesCount * 1000 / uSumTime;
      pSMVIStats->uAverageFrameTime = uSumTime / uSumFramesCount;
   }

   if ( 0 < uSumFramesCount && 0 < uSumSizes )
      pSMVIStats->uAverageFrameSize = uSumSizes / uSumFramesCount;

   
   pSMVIStats->uMaxFrameDeltaTime = 0;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
      if ( pSMVIStats->uFramesTimes[i] == 0 )
         continue;
      if ( pSMVIStats->uFramesTimes[i] > pSMVIStats->uAverageFrameTime )
      if ( pSMVIStats->uFramesTimes[i] - pSMVIStats->uAverageFrameTime > pSMVIStats->uMaxFrameDeltaTime )
         pSMVIStats->uMaxFrameDeltaTime = pSMVIStats->uFramesTimes[i] - pSMVIStats->uAverageFrameTime;
      //if ( pSMVIStats->uFramesTimes[i] < pSMVIStats->uAverageFrameTime )
      //if ( pSMVIStats->uAverageFrameTime - pSMVIStats->uFramesTimes[i] > pSMVIStats->uMaxFrameDeltaTime )
      //   pSMVIStats->uMaxFrameDeltaTime = pSMVIStats->uAverageFrameTime - pSMVIStats->uFramesTimes[i];
   }
}