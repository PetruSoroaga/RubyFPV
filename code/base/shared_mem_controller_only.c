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
#include "shared_mem_controller_only.h"
#include "../radio/radiopackets2.h"


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
      int *pRates = getDataRatesBPS();
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
      int *pRates = getDataRatesBPS();
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


shared_mem_radio_stats_interfaces_rx_graph* shared_mem_controller_radio_stats_interfaces_rx_graphs_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_CONTROLLER_RADIO_INTERFACES_RX_GRAPHS, sizeof(shared_mem_radio_stats_interfaces_rx_graph));
   return (shared_mem_radio_stats_interfaces_rx_graph*)retVal;
}

shared_mem_radio_stats_interfaces_rx_graph* shared_mem_controller_radio_stats_interfaces_rx_graphs_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_CONTROLLER_RADIO_INTERFACES_RX_GRAPHS, sizeof(shared_mem_radio_stats_interfaces_rx_graph));
   return (shared_mem_radio_stats_interfaces_rx_graph*)retVal;
}

void shared_mem_controller_radio_stats_interfaces_rx_graphs_close(shared_mem_radio_stats_interfaces_rx_graph* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_radio_stats_interfaces_rx_graph));
}


shared_mem_video_stream_stats_rx_processors* shared_mem_video_stream_stats_rx_processors_open(int readOnly)
{
   void *retVal =  open_shared_mem(SHARED_MEM_VIDEO_STREAM_STATS, sizeof(shared_mem_video_stream_stats_rx_processors), readOnly);
   shared_mem_video_stream_stats_rx_processors *tretval = (shared_mem_video_stream_stats_rx_processors*)retVal;
   return tretval;
}
void shared_mem_video_stream_stats_rx_processors_close(shared_mem_video_stream_stats_rx_processors* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_stream_stats_rx_processors));
   //shm_unlink(SHARED_MEM_VIDEO_STREAM_STATS);
}

shared_mem_video_stream_stats_history_rx_processors* shared_mem_video_stream_stats_history_rx_processors_open(int readOnly)
{
   void *retVal =  open_shared_mem(SHARED_MEM_VIDEO_STREAM_STATS_HISTORY, sizeof(shared_mem_video_stream_stats_history_rx_processors), readOnly);
   shared_mem_video_stream_stats_history_rx_processors *tretval = (shared_mem_video_stream_stats_history_rx_processors*)retVal;
   return tretval;
}

void shared_mem_video_stream_stats_history_rx_processors_close(shared_mem_video_stream_stats_history_rx_processors* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_video_stream_stats_history_rx_processors));
   //shm_unlink(SHARED_MEM_VIDEO_STREAM_STATS_HISTORY);
}

shared_mem_controller_retransmissions_stats_rx_processors* shared_mem_controller_video_retransmissions_stats_open_for_read()
{
   void *retVal = open_shared_mem(SHARED_MEM_VIDEO_RETRANSMISSIONS_STATS, sizeof(shared_mem_controller_retransmissions_stats_rx_processors), 1);
   return (shared_mem_controller_retransmissions_stats_rx_processors*)retVal;
}

shared_mem_controller_retransmissions_stats_rx_processors* shared_mem_controller_video_retransmissions_stats_open_for_write()
{
   void *retVal = open_shared_mem(SHARED_MEM_VIDEO_RETRANSMISSIONS_STATS, sizeof(shared_mem_controller_retransmissions_stats_rx_processors), 0);
   return (shared_mem_controller_retransmissions_stats_rx_processors*)retVal;
}

void shared_mem_controller_video_retransmissions_stats_close(shared_mem_controller_retransmissions_stats_rx_processors* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_controller_retransmissions_stats_rx_processors));
}

shared_mem_audio_decode_stats* shared_mem_controller_audio_decode_stats_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_AUDIO_DECODE_STATS, sizeof(shared_mem_audio_decode_stats));
   return (shared_mem_audio_decode_stats*)retVal;
}

shared_mem_audio_decode_stats* shared_mem_controller_audio_decode_stats_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_AUDIO_DECODE_STATS, sizeof(shared_mem_audio_decode_stats));
   return (shared_mem_audio_decode_stats*)retVal;
}

void shared_mem_controller_audio_decode_stats_close(shared_mem_audio_decode_stats* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_audio_decode_stats));
   //shm_unlink(szName);
}

shared_mem_router_vehicles_runtime_info* shared_mem_router_vehicles_runtime_info_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_CONTROLLER_ADAPTIVE_VIDEO_INFO, sizeof(shared_mem_router_vehicles_runtime_info));
   return (shared_mem_router_vehicles_runtime_info*)retVal;
}

shared_mem_router_vehicles_runtime_info* shared_mem_router_vehicles_runtime_info_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_CONTROLLER_ADAPTIVE_VIDEO_INFO, sizeof(shared_mem_router_vehicles_runtime_info));
   return (shared_mem_router_vehicles_runtime_info*)retVal;
}

void shared_mem_router_vehicles_runtime_info_close(shared_mem_router_vehicles_runtime_info* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(shared_mem_router_vehicles_runtime_info));
   //shm_unlink(szName);
}

