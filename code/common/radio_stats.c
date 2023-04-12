/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"

#include "../radio/radioflags.h"
#include "../radio/radiopackets2.h"
#include "radio_stats.h"

#define MAX_HISTORY_RX_PACKETS_INDEXES 300

typedef struct
{
   u32 uCurrentBufferIndex;
   u32 uMaxReceivedPacketIndex;

   u32 packetsIndexes[MAX_HISTORY_RX_PACKETS_INDEXES];
   u32 packetsTimes[MAX_HISTORY_RX_PACKETS_INDEXES];

} __attribute__((packed)) t_stream_history_packets;

typedef struct
{
   u32 uVehicleId;
   t_stream_history_packets streamsHistory[MAX_RADIO_STREAMS];
   t_stream_history_packets streamsHistoryPerRadioLink[MAX_RADIO_INTERFACES][MAX_RADIO_STREAMS];
   u32 uLastReceivedPacketIndexesPerRadioInterfacePerStream[MAX_RADIO_INTERFACES][MAX_RADIO_STREAMS];
   u32 s_uLastReceivedShortPacketsIndexes[MAX_RADIO_INTERFACES];
} __attribute__((packed)) t_vehicle_history_packets;

static t_vehicle_history_packets s_ListHistoryRxVehicles[MAX_CONCURENT_VEHICLES];
static int s_iCountHistoryRxVehicles = 0;

static u32 s_uControllerLinkStats_tmpRecv[MAX_RADIO_INTERFACES];
static u32 s_uControllerLinkStats_tmpRecvBad[MAX_RADIO_INTERFACES];
static u32 s_uControllerLinkStats_tmpRecvLost[MAX_RADIO_INTERFACES];

#define MAX_RADIO_LINKS_ROUNDTRIP_TIMES_HISTORY 10
static u32 s_uLastLinkRTDelayValues[MAX_RADIO_INTERFACES][MAX_RADIO_LINKS_ROUNDTRIP_TIMES_HISTORY];
static u32 s_uLastCommandsRTDelayValues[5];

static u32 s_uLastTimeDebugPacketRecvOnNoLink = 0;

void _radio_stats_reset_local_stats_for_vehicle(int iVehicleIndex)
{
   if ( iVehicleIndex < 0 || iVehicleIndex >= MAX_CONCURENT_VEHICLES )
      return;

   s_ListHistoryRxVehicles[iVehicleIndex].uVehicleId = 0;
   for( int k=0; k<MAX_RADIO_STREAMS; k++ )
   {
      s_ListHistoryRxVehicles[iVehicleIndex].streamsHistory[k].uCurrentBufferIndex = 0;
      s_ListHistoryRxVehicles[iVehicleIndex].streamsHistory[k].uMaxReceivedPacketIndex = MAX_U32;
      memset((u8*)s_ListHistoryRxVehicles[iVehicleIndex].streamsHistory[k].packetsIndexes, 0xFF, MAX_HISTORY_RX_PACKETS_INDEXES * sizeof(u32));
      memset((u8*)s_ListHistoryRxVehicles[iVehicleIndex].streamsHistory[k].packetsTimes, 0, MAX_HISTORY_RX_PACKETS_INDEXES * sizeof(u32));
   }

   for( int l=0; l<MAX_RADIO_INTERFACES; l++ )
   for( int k=0; k<MAX_RADIO_STREAMS; k++ )
   {
      s_ListHistoryRxVehicles[iVehicleIndex].streamsHistoryPerRadioLink[l][k].uCurrentBufferIndex = 0;
      s_ListHistoryRxVehicles[iVehicleIndex].streamsHistoryPerRadioLink[l][k].uMaxReceivedPacketIndex = MAX_U32;
      memset((u8*)s_ListHistoryRxVehicles[iVehicleIndex].streamsHistoryPerRadioLink[l][k].packetsIndexes, 0xFF, MAX_HISTORY_RX_PACKETS_INDEXES * sizeof(u32));
      memset((u8*)s_ListHistoryRxVehicles[iVehicleIndex].streamsHistoryPerRadioLink[l][k].packetsTimes, 0, MAX_HISTORY_RX_PACKETS_INDEXES * sizeof(u32));
   }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   for ( int k=0; k<MAX_RADIO_STREAMS; k++ )
      s_ListHistoryRxVehicles[iVehicleIndex].uLastReceivedPacketIndexesPerRadioInterfacePerStream[i][k] = MAX_U32;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_ListHistoryRxVehicles[iVehicleIndex].s_uLastReceivedShortPacketsIndexes[i] = 0xFF;
}

void _radio_stats_reset_local_stats()
{
   s_iCountHistoryRxVehicles = 0;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      _radio_stats_reset_local_stats_for_vehicle(i);

   log_line("[RadioStats] Reset local radio stats. Total radio stats size: %u bytes, %u bytes/vehicle.",
       sizeof(t_vehicle_history_packets) * MAX_CONCURENT_VEHICLES,
       sizeof(t_vehicle_history_packets) );
}
   
void radio_stats_reset(shared_mem_radio_stats* pSMRS, int graphRefreshInterval)
{
   if ( NULL == pSMRS )
      return;

   for( int iLink=0; iLink<MAX_RADIO_INTERFACES; iLink++ )
   for( int i=0; i<MAX_RADIO_LINKS_ROUNDTRIP_TIMES_HISTORY; i++ )
      s_uLastLinkRTDelayValues[iLink][i] = 1000;

   for( int i=0; i<sizeof(s_uLastCommandsRTDelayValues)/sizeof(s_uLastCommandsRTDelayValues[0]); i++ )
      s_uLastCommandsRTDelayValues[i] = 1000;

   pSMRS->refreshIntervalMs = 350;
   pSMRS->graphRefreshIntervalMs = graphRefreshInterval;

   pSMRS->countRadioLinks = 0;
   pSMRS->countRadioInterfaces = 0;
   
   pSMRS->lastComputeTime = 0;
   pSMRS->lastComputeTimeGraph = 0;

   pSMRS->all_downlinks_tx_time_per_sec = 0;
   pSMRS->tmp_all_downlinks_tx_time_per_sec = 0;

   pSMRS->uAverageCommandRoundtripMiliseconds = MAX_U32;
   pSMRS->uMaxCommandRoundtripMiliseconds = MAX_U32;
   pSMRS->uMinCommandRoundtripMiliseconds = MAX_U32;

   // Init streams

   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      pSMRS->radio_streams[i].totalRxBytes = 0;
      pSMRS->radio_streams[i].totalTxBytes = 0;
      pSMRS->radio_streams[i].rxBytesPerSec = 0;
      pSMRS->radio_streams[i].txBytesPerSec = 0;
      pSMRS->radio_streams[i].totalRxPackets = 0;
      pSMRS->radio_streams[i].totalTxPackets = 0;
      pSMRS->radio_streams[i].totalLostPackets = 0;
      pSMRS->radio_streams[i].rxPacketsPerSec = 0;
      pSMRS->radio_streams[i].txPacketsPerSec = 0;
      pSMRS->radio_streams[i].timeLastRxPacket = 0;
      pSMRS->radio_streams[i].timeLastTxPacket = 0;

      pSMRS->radio_streams[i].tmpRxBytes = 0;
      pSMRS->radio_streams[i].tmpTxBytes = 0;
      pSMRS->radio_streams[i].tmpRxPackets = 0;
      pSMRS->radio_streams[i].tmpTxPackets = 0;
   }

   // Init radio interfaces

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pSMRS->radio_interfaces[i].assignedRadioLinkId = -1;
      pSMRS->radio_interfaces[i].lastDbm = 200;
      pSMRS->radio_interfaces[i].lastDbmVideo = 200;
      pSMRS->radio_interfaces[i].lastDbmData = 200;
      pSMRS->radio_interfaces[i].lastDataRate = 0;
      pSMRS->radio_interfaces[i].lastDataRateVideo = 0;
      pSMRS->radio_interfaces[i].lastDataRateData = 0;

      pSMRS->radio_interfaces[i].totalRxBytes = 0;
      pSMRS->radio_interfaces[i].totalTxBytes = 0;
      pSMRS->radio_interfaces[i].rxBytesPerSec = 0;
      pSMRS->radio_interfaces[i].txBytesPerSec = 0;
      pSMRS->radio_interfaces[i].totalRxPackets = 0;
      pSMRS->radio_interfaces[i].totalRxPacketsBad = 0;
      pSMRS->radio_interfaces[i].totalRxPacketsLost = 0;
      pSMRS->radio_interfaces[i].totalTxPackets = 0;
      pSMRS->radio_interfaces[i].rxPacketsPerSec = 0;
      pSMRS->radio_interfaces[i].txPacketsPerSec = 0;
      pSMRS->radio_interfaces[i].timeLastRxPacket = 0;
      pSMRS->radio_interfaces[i].timeLastTxPacket = 0;

      pSMRS->radio_interfaces[i].tmpRxBytes = 0;
      pSMRS->radio_interfaces[i].tmpTxBytes = 0;
      pSMRS->radio_interfaces[i].tmpRxPackets = 0;
      pSMRS->radio_interfaces[i].tmpTxPackets = 0;

      pSMRS->radio_interfaces[i].rxQuality = 0;
      pSMRS->radio_interfaces[i].rxRelativeQuality = 0;

      for( int k=0; k<MAX_RADIO_STREAMS; k++ )
         pSMRS->radio_interfaces[i].lastReceivedStreamPacketIndex[k] = 0;

      for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
      {
         pSMRS->radio_interfaces[i].hist_rxPackets[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsBad[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxPacketsLost[k] = 0;
         pSMRS->radio_interfaces[i].hist_rxGapMiliseconds[k] = 0xFF;
      }
      pSMRS->radio_interfaces[i].hist_tmp_rxPackets = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBad = 0;
      pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLost = 0;
   }

   // Init radio links

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pSMRS->radio_links[i].refreshIntervalMs = 500;
      pSMRS->radio_links[i].lastComputeTime = 0;
      pSMRS->radio_links[i].totalRxBytes = 0;
      pSMRS->radio_links[i].totalTxBytes = 0;
      pSMRS->radio_links[i].rxBytesPerSec = 0;
      pSMRS->radio_links[i].txBytesPerSec = 0;
      pSMRS->radio_links[i].totalRxPackets = 0;
      pSMRS->radio_links[i].totalTxPackets = 0;
      pSMRS->radio_links[i].rxPacketsPerSec = 0;
      pSMRS->radio_links[i].txPacketsPerSec = 0;
      pSMRS->radio_links[i].totalTxPacketsUncompressed = 0;
      pSMRS->radio_links[i].txUncompressedPacketsPerSec = 0;

      pSMRS->radio_links[i].timeLastRxPacket = 0;
      pSMRS->radio_links[i].timeLastTxPacket = 0;

      pSMRS->radio_links[i].tmpRxBytes = 0;
      pSMRS->radio_links[i].tmpTxBytes = 0;
      pSMRS->radio_links[i].tmpRxPackets = 0;
      pSMRS->radio_links[i].tmpTxPackets = 0;
      pSMRS->radio_links[i].tmpUncompressedTxPackets = 0;

      for( int k=0; k<MAX_RADIO_STREAMS; k++ )
      {
         pSMRS->radio_links[i].streamTotalRxBytes[k] = 0;
         pSMRS->radio_links[i].streamTotalTxBytes[k] = 0;
         pSMRS->radio_links[i].streamRxBytesPerSec[k] = 0;
         pSMRS->radio_links[i].streamTxBytesPerSec[k] = 0;
         pSMRS->radio_links[i].streamTotalRxPackets[k] = 0;
         pSMRS->radio_links[i].streamTotalTxPackets[k] = 0;
         pSMRS->radio_links[i].streamRxPacketsPerSec[k] = 0;
         pSMRS->radio_links[i].streamTxPacketsPerSec[k] = 0;
         pSMRS->radio_links[i].streamTimeLastRxPacket[k] = 0;
         pSMRS->radio_links[i].streamTimeLastTxPacket[k] = 0;

         pSMRS->radio_links[i].stream_tmpRxBytes[k] = 0;
         pSMRS->radio_links[i].stream_tmpTxBytes[k] = 0;
         pSMRS->radio_links[i].stream_tmpRxPackets[k] = 0;
         pSMRS->radio_links[i].stream_tmpTxPackets[k] = 0;
      }

      pSMRS->radio_links[i].linkDelayRoundtripMsLastTime = 0;
      pSMRS->radio_links[i].linkDelayRoundtripMs = MAX_U32;
      pSMRS->radio_links[i].linkDelayRoundtripMinimMs = MAX_U32;
      pSMRS->radio_links[i].lastTxInterfaceIndex = -1;
      pSMRS->radio_links[i].downlink_tx_time_per_sec = 0;
      pSMRS->radio_links[i].tmp_downlink_tx_time_per_sec = 0;
   }

   _radio_stats_reset_local_stats();

   log_line("[RadioStats] Reset radio stats: %d/%d ms refresh intervals; total radio stats size: %d bytes", pSMRS->graphRefreshIntervalMs, pSMRS->refreshIntervalMs, sizeof(shared_mem_radio_stats));
}

void radio_stats_reset_streams_rx_history(shared_mem_radio_stats* pSMRS, u32 uCurrentVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( s_ListHistoryRxVehicles[i].uVehicleId != uCurrentVehicleId )
      if ( s_ListHistoryRxVehicles[i].uVehicleId != 0 )
         _radio_stats_reset_local_stats_for_vehicle(i);
   }
}

void radio_stats_reset_streams_rx_history_for_vehicle(shared_mem_radio_stats* pSMRS, u32 uVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( s_ListHistoryRxVehicles[i].uVehicleId == uVehicleId )
      if ( s_ListHistoryRxVehicles[i].uVehicleId != 0 )
         _radio_stats_reset_local_stats_for_vehicle(i);
   }
}

void radio_stats_log_info(shared_mem_radio_stats* pSMRS)
{
   char szBuff[512];
   char szBuff2[64];

   log_line("----------------------------------------");
   log_line("Radio RX stats (refresh at %d ms, graphs at %d ms), %d interfaces, %d radio links:", pSMRS->refreshIntervalMs, pSMRS->graphRefreshIntervalMs, pSMRS->countRadioInterfaces, pSMRS->countRadioLinks);
   
   strcpy(szBuff, "Streams current packets indexes: ");
   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      sprintf(szBuff2, "%u, ", pSMRS->radio_streams[i].totalRxPackets);
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);
      
   strcpy(szBuff, "Radio Links current packets indexes: ");
   for( int i=0; i<pSMRS->countRadioLinks; i++ )
   {
      sprintf(szBuff2, "%u, ", pSMRS->radio_links[i].totalRxPackets);
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   strcpy(szBuff, "Radio Interf current packets indexes: ");
   for( int i=0; i<pSMRS->countRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%u, ", pSMRS->radio_interfaces[i].totalRxPackets);
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   strcpy(szBuff, "Radio Interf total rx bytes: ");
   for( int i=0; i<pSMRS->countRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%u, ", pSMRS->radio_interfaces[i].totalRxBytes);
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   strcpy(szBuff, "Radio Interf total rx bytes/sec: ");
   for( int i=0; i<pSMRS->countRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%u, ", pSMRS->radio_interfaces[i].rxBytesPerSec);
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   strcpy(szBuff, "Radio Interf current rx quality: ");
   for( int i=0; i<pSMRS->countRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%d%%, ", pSMRS->radio_interfaces[i].rxQuality);
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   strcpy(szBuff, "Radio Interf current relative rx quality: ");
   for( int i=0; i<pSMRS->countRadioInterfaces; i++ )
   {
      sprintf(szBuff2, "%d%%, ", pSMRS->radio_interfaces[i].rxRelativeQuality);
      strcat(szBuff, szBuff2);
   }
   log_line(szBuff);

   
   strcpy(szBuff, "Radio streams throughput (global): ");
   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      if ( 0 < pSMRS->radio_streams[i].rxBytesPerSec || 0 < pSMRS->radio_streams[i].rxPacketsPerSec )
      {
         sprintf(szBuff2, "stream %d: %u bps / %u pckts/s, ", i, pSMRS->radio_streams[i].rxBytesPerSec*8, pSMRS->radio_streams[i].rxPacketsPerSec);
         strcat(szBuff, szBuff2);
      }
   }
   log_line(szBuff);

   for( int iLink=0; iLink<pSMRS->countRadioLinks; iLink++ )
   {
      sprintf(szBuff, "Radio streams throughput (link %d): ", iLink+1);
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
      {
         if ( 0 < pSMRS->radio_links[iLink].streamRxBytesPerSec[i] || 0 < pSMRS->radio_links[iLink].streamRxPacketsPerSec[i] )
         {
            sprintf(szBuff2, "stream %d: %u bps / %u pckts/s, ", i,pSMRS->radio_links[iLink].streamRxBytesPerSec[i]*8, pSMRS->radio_links[iLink].streamRxPacketsPerSec[i]);
            strcat(szBuff, szBuff2);
         }
      }
      log_line(szBuff);    
   }
   
}

void _radio_stats_update_kbps_values(shared_mem_radio_stats* pSMRS)
{
   if ( NULL == pSMRS )
      return;

   // Update radio streams

   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      pSMRS->radio_streams[i].rxBytesPerSec = pSMRS->radio_streams[i].rxBytesPerSec/2 + (pSMRS->radio_streams[i].tmpRxBytes * 1000) / pSMRS->refreshIntervalMs / 2;
      pSMRS->radio_streams[i].txBytesPerSec = pSMRS->radio_streams[i].txBytesPerSec/2 + (pSMRS->radio_streams[i].tmpTxBytes * 1000) / pSMRS->refreshIntervalMs / 2;
      pSMRS->radio_streams[i].tmpRxBytes = 0;
      pSMRS->radio_streams[i].tmpTxBytes = 0;

      pSMRS->radio_streams[i].rxPacketsPerSec = pSMRS->radio_streams[i].rxPacketsPerSec/2 + (pSMRS->radio_streams[i].tmpRxPackets * 1000) / pSMRS->refreshIntervalMs / 2;
      pSMRS->radio_streams[i].txPacketsPerSec = pSMRS->radio_streams[i].txPacketsPerSec/2 + (pSMRS->radio_streams[i].tmpTxPackets * 1000) / pSMRS->refreshIntervalMs / 2;
      pSMRS->radio_streams[i].tmpRxPackets = 0;
      pSMRS->radio_streams[i].tmpTxPackets = 0;
   }

   // Update radio interfaces

   for( int i=0; i<pSMRS->countRadioInterfaces; i++ )
   {
      pSMRS->radio_interfaces[i].rxBytesPerSec = (pSMRS->radio_interfaces[i].tmpRxBytes * 1000) / pSMRS->refreshIntervalMs;
      pSMRS->radio_interfaces[i].txBytesPerSec = (pSMRS->radio_interfaces[i].tmpTxBytes * 1000) / pSMRS->refreshIntervalMs;
      pSMRS->radio_interfaces[i].tmpRxBytes = 0;
      pSMRS->radio_interfaces[i].tmpTxBytes = 0;

      pSMRS->radio_interfaces[i].rxPacketsPerSec = (pSMRS->radio_interfaces[i].tmpRxPackets * 1000) / pSMRS->refreshIntervalMs;
      pSMRS->radio_interfaces[i].txPacketsPerSec = (pSMRS->radio_interfaces[i].tmpTxPackets * 1000) / pSMRS->refreshIntervalMs;
      pSMRS->radio_interfaces[i].tmpRxPackets = 0;
      pSMRS->radio_interfaces[i].tmpTxPackets = 0;
   }

   pSMRS->all_downlinks_tx_time_per_sec = (pSMRS->tmp_all_downlinks_tx_time_per_sec * 1000) / pSMRS->refreshIntervalMs;
   pSMRS->tmp_all_downlinks_tx_time_per_sec = 0;
   // Transform from microsec to milisec
   pSMRS->all_downlinks_tx_time_per_sec /= 1000;
}

void _radio_stats_update_kbps_values_radio_links(shared_mem_radio_stats* pSMRS, int iRadioLinkId)
{
   if ( NULL == pSMRS )
      return;

   if ( (iRadioLinkId < 0) || (iRadioLinkId>= pSMRS->countRadioLinks) )
      return;

   // Update radio link kbps

   //int iRefreshInterval = pSMRS->refreshIntervalMs;
   int iRefreshInterval = pSMRS->radio_links[iRadioLinkId].refreshIntervalMs;

   pSMRS->radio_links[iRadioLinkId].rxBytesPerSec = (pSMRS->radio_links[iRadioLinkId].tmpRxBytes * 1000) / iRefreshInterval;
   pSMRS->radio_links[iRadioLinkId].txBytesPerSec = (pSMRS->radio_links[iRadioLinkId].tmpTxBytes * 1000) / iRefreshInterval;
   pSMRS->radio_links[iRadioLinkId].tmpRxBytes = 0;
   pSMRS->radio_links[iRadioLinkId].tmpTxBytes = 0;

   pSMRS->radio_links[iRadioLinkId].rxPacketsPerSec = (pSMRS->radio_links[iRadioLinkId].tmpRxPackets * 1000) / iRefreshInterval;
   pSMRS->radio_links[iRadioLinkId].txPacketsPerSec = (pSMRS->radio_links[iRadioLinkId].tmpTxPackets * 1000) / iRefreshInterval;
   pSMRS->radio_links[iRadioLinkId].tmpRxPackets = 0;
   pSMRS->radio_links[iRadioLinkId].tmpTxPackets = 0;

   pSMRS->radio_links[iRadioLinkId].txUncompressedPacketsPerSec = (pSMRS->radio_links[iRadioLinkId].tmpUncompressedTxPackets * 1000) / iRefreshInterval;
   pSMRS->radio_links[iRadioLinkId].tmpUncompressedTxPackets = 0;

   pSMRS->radio_links[iRadioLinkId].downlink_tx_time_per_sec = (pSMRS->radio_links[iRadioLinkId].tmp_downlink_tx_time_per_sec * 1000) / iRefreshInterval;
   pSMRS->radio_links[iRadioLinkId].tmp_downlink_tx_time_per_sec = 0;

   // Transform from microsec to milisec
   pSMRS->radio_links[iRadioLinkId].downlink_tx_time_per_sec /= 1000;


   for( int k=0; k<MAX_RADIO_STREAMS; k++ )
   {
      pSMRS->radio_links[iRadioLinkId].streamRxBytesPerSec[k] = (pSMRS->radio_links[iRadioLinkId].stream_tmpRxBytes[k] * 1000) / iRefreshInterval;
      pSMRS->radio_links[iRadioLinkId].streamTxBytesPerSec[k] = (pSMRS->radio_links[iRadioLinkId].stream_tmpTxBytes[k] * 1000) / iRefreshInterval;
      pSMRS->radio_links[iRadioLinkId].stream_tmpRxBytes[k] = 0;
      pSMRS->radio_links[iRadioLinkId].stream_tmpTxBytes[k] = 0;

      pSMRS->radio_links[iRadioLinkId].streamRxPacketsPerSec[k] = (pSMRS->radio_links[iRadioLinkId].stream_tmpRxPackets[k] * 1000) / iRefreshInterval;
      pSMRS->radio_links[iRadioLinkId].streamTxPacketsPerSec[k] = (pSMRS->radio_links[iRadioLinkId].stream_tmpTxPackets[k] * 1000) / iRefreshInterval;
      pSMRS->radio_links[iRadioLinkId].stream_tmpRxPackets[k] = 0;
      pSMRS->radio_links[iRadioLinkId].stream_tmpTxPackets[k] = 0;
   }
}

// returns 1 if it was updated, 0 if unchanged

int radio_stats_periodic_update(shared_mem_radio_stats* pSMRS, u32 timeNow)
{
   if ( NULL == pSMRS )
      return 0;
   int iReturn = 0;

   for( int i=0; i<pSMRS->countRadioLinks; i++ )
   {
      if ( timeNow >= pSMRS->radio_links[i].lastComputeTime + pSMRS->radio_links[i].refreshIntervalMs || timeNow < pSMRS->radio_links[i].lastComputeTime )
      {
         _radio_stats_update_kbps_values_radio_links(pSMRS, i);
         pSMRS->radio_links[i].lastComputeTime = timeNow;
         iReturn = 1;
      }
   }

   if ( timeNow >= pSMRS->lastComputeTime + pSMRS->refreshIntervalMs || timeNow < pSMRS->lastComputeTime )
   {
      pSMRS->lastComputeTime = timeNow;
      iReturn = 1;

      _radio_stats_update_kbps_values(pSMRS);

      // Update RX quality for each radio interface

      int iIntervalsToUse = 2000 / pSMRS->graphRefreshIntervalMs;
      if ( iIntervalsToUse < 3 )
         iIntervalsToUse = 3;
      if ( iIntervalsToUse >= sizeof(pSMRS->radio_interfaces[0].hist_rxPackets)/sizeof(pSMRS->radio_interfaces[0].hist_rxPackets[0]) )
         iIntervalsToUse = sizeof(pSMRS->radio_interfaces[0].hist_rxPackets)/sizeof(pSMRS->radio_interfaces[0].hist_rxPackets[0]) - 1;

      for( int i=0; i<pSMRS->countRadioInterfaces; i++ )
      {
         u32 totalRecv = 0;
         u32 totalRecvBad = 0;
         u32 totalRecvLost = 0;
         for( int k=0; k<iIntervalsToUse; k++ )
         {
            totalRecv += pSMRS->radio_interfaces[i].hist_rxPackets[k];
            totalRecvBad += pSMRS->radio_interfaces[i].hist_rxPacketsBad[k];
            totalRecvLost += pSMRS->radio_interfaces[i].hist_rxPacketsLost[k];
         }
         if ( 0 == totalRecv )
            pSMRS->radio_interfaces[i].rxQuality = 0;
         else
            pSMRS->radio_interfaces[i].rxQuality = 100 - (100*(totalRecvLost+totalRecvBad))/(totalRecv+totalRecvLost);
      }

      // Update relative RX quality for each radio interface

      for( int i=0; i<pSMRS->countRadioInterfaces; i++ )
      {
         u32 totalRecv = 0;
         u32 totalRecvBad = 0;
         u32 totalRecvLost = 0;
         for( int i=0; i<iIntervalsToUse; i++ )
         {
            totalRecv += pSMRS->radio_interfaces[i].hist_rxPackets[i];
            totalRecvBad += pSMRS->radio_interfaces[i].hist_rxPacketsBad[i];
            totalRecvLost += pSMRS->radio_interfaces[i].hist_rxPacketsLost[i];
         }

         totalRecv += pSMRS->radio_interfaces[i].hist_tmp_rxPackets;
         totalRecvBad += pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBad;
         totalRecvLost += pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLost;

         pSMRS->radio_interfaces[i].rxRelativeQuality = pSMRS->radio_interfaces[i].rxQuality;
         if ( pSMRS->radio_interfaces[i].lastDbm > 0 )
            pSMRS->radio_interfaces[i].rxRelativeQuality -= pSMRS->radio_interfaces[i].lastDbm;
         else
            pSMRS->radio_interfaces[i].rxRelativeQuality += pSMRS->radio_interfaces[i].lastDbm;

         if ( pSMRS->radio_interfaces[i].lastDbm < -100 )
            pSMRS->radio_interfaces[i].rxRelativeQuality -= 10000;

         pSMRS->radio_interfaces[i].rxRelativeQuality -= totalRecvLost;
         pSMRS->radio_interfaces[i].rxRelativeQuality += (totalRecv-totalRecvBad);
      }

      //radio_stats_log_info(pSMRS);
   }

   if ( timeNow >= pSMRS->lastComputeTimeGraph + pSMRS->graphRefreshIntervalMs || timeNow < pSMRS->lastComputeTimeGraph )
   {
      pSMRS->lastComputeTimeGraph = timeNow;
      iReturn = 1;

      for( int i=0; i<pSMRS->countRadioInterfaces; i++ )
      {
         for( int k=MAX_HISTORY_RADIO_STATS_RECV_SLICES-1; k>0; k-- )
         {
            pSMRS->radio_interfaces[i].hist_rxPackets[k] = pSMRS->radio_interfaces[i].hist_rxPackets[k-1];
            pSMRS->radio_interfaces[i].hist_rxPacketsBad[k] = pSMRS->radio_interfaces[i].hist_rxPacketsBad[k-1];
            pSMRS->radio_interfaces[i].hist_rxPacketsLost[k] = pSMRS->radio_interfaces[i].hist_rxPacketsLost[k-1];
            pSMRS->radio_interfaces[i].hist_rxGapMiliseconds[k] = pSMRS->radio_interfaces[i].hist_rxGapMiliseconds[k-1];
         }

         if ( pSMRS->radio_interfaces[i].hist_tmp_rxPackets < 255 )
            pSMRS->radio_interfaces[i].hist_rxPackets[0] = pSMRS->radio_interfaces[i].hist_tmp_rxPackets;
         else
            pSMRS->radio_interfaces[i].hist_rxPackets[0] = 0xFF;

         if ( pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBad < 255 )
            pSMRS->radio_interfaces[i].hist_rxPacketsBad[0] = pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBad;
         else
            pSMRS->radio_interfaces[i].hist_rxPacketsBad[0] = 0xFF;

         if ( pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLost < 255 )
            pSMRS->radio_interfaces[i].hist_rxPacketsLost[0] = pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLost;
         else
            pSMRS->radio_interfaces[i].hist_rxPacketsLost[0] = 0xFF;

         pSMRS->radio_interfaces[i].hist_rxGapMiliseconds[0] = 0xFF;
         pSMRS->radio_interfaces[i].hist_tmp_rxPackets = 0;
         pSMRS->radio_interfaces[i].hist_tmp_rxPacketsBad = 0;
         pSMRS->radio_interfaces[i].hist_tmp_rxPacketsLost = 0;
      }
   }
   return iReturn;
}

void radio_stats_set_radio_link_rt_delay(shared_mem_radio_stats* pSMRS, int iRadioLink, u32 delay, u32 timeNow)
{
   if ( NULL == pSMRS || iRadioLink < 0 || iRadioLink >= MAX_RADIO_INTERFACES )
      return;

   u32 avg = 0;
   for( int i=0; i<MAX_RADIO_LINKS_ROUNDTRIP_TIMES_HISTORY-1; i++ )
   {
      s_uLastLinkRTDelayValues[iRadioLink][i] = s_uLastLinkRTDelayValues[iRadioLink][i+1];
      avg += s_uLastLinkRTDelayValues[iRadioLink][i];
   }
   s_uLastLinkRTDelayValues[iRadioLink][MAX_RADIO_LINKS_ROUNDTRIP_TIMES_HISTORY-1] = delay;
   avg += delay;
   avg /= MAX_RADIO_LINKS_ROUNDTRIP_TIMES_HISTORY;
   pSMRS->radio_links[iRadioLink].linkDelayRoundtripMs = avg;
   pSMRS->radio_links[iRadioLink].linkDelayRoundtripMsLastTime = timeNow;
   if ( pSMRS->radio_links[iRadioLink].linkDelayRoundtripMs < pSMRS->radio_links[iRadioLink].linkDelayRoundtripMinimMs )
      pSMRS->radio_links[iRadioLink].linkDelayRoundtripMinimMs = pSMRS->radio_links[iRadioLink].linkDelayRoundtripMs;
}

void radio_stats_set_commands_rt_delay(shared_mem_radio_stats* pSMRS, u32 delay)
{
   if ( NULL == pSMRS )
      return;

   int count = sizeof(s_uLastCommandsRTDelayValues)/sizeof(s_uLastCommandsRTDelayValues[0]);
   u32 avg = 0;
   for( int i=0; i<count-1; i++ )
   {
      s_uLastCommandsRTDelayValues[i] = s_uLastCommandsRTDelayValues[i+1];
      avg += s_uLastCommandsRTDelayValues[i];
   }
   s_uLastCommandsRTDelayValues[count-1] = delay;
   avg += delay;
   avg /= count;

   pSMRS->uAverageCommandRoundtripMiliseconds = avg;

   if ( pSMRS->uMaxCommandRoundtripMiliseconds == MAX_U32 )
      pSMRS->uMaxCommandRoundtripMiliseconds = delay;

   if ( pSMRS->uAverageCommandRoundtripMiliseconds < pSMRS->uMinCommandRoundtripMiliseconds )
      pSMRS->uMinCommandRoundtripMiliseconds = pSMRS->uAverageCommandRoundtripMiliseconds;
}

void radio_stats_set_tx_card_for_radio_link(shared_mem_radio_stats* pSMRS, int iRadioLink, int iTxCard)
{
   if ( NULL == pSMRS )
      return;
   pSMRS->radio_links[iRadioLink].lastTxInterfaceIndex = iTxCard;
}

void radio_stats_set_card_current_frequency(shared_mem_radio_stats* pSMRS, int iRadioInterface, u32 freq)
{
   if ( NULL == pSMRS )
      return;
   pSMRS->radio_interfaces[iRadioInterface].uCurrentFrequency = freq;
}

int _radio_stats_get_vehicle_runtime_info_index(u32 uVehicleId)
{
   for( int i=0; i<s_iCountHistoryRxVehicles; i++ )
   {
     if ( uVehicleId == s_ListHistoryRxVehicles[i].uVehicleId )
        return i;
   }

   log_line("[RadioStats] Start receiving data from vehicle id: %u", uVehicleId);    
   
   char szTmp[256];
   szTmp[0] = 0;
   for( int i=0; i<s_iCountHistoryRxVehicles; i++ )
   {
      if ( 0 != szTmp[0] )
         strcat(szTmp, ", ");
      char szTmp2[32];
      sprintf(szTmp2, "%u", s_ListHistoryRxVehicles[i].uVehicleId);
      strcat(szTmp, szTmp2);
   }
   log_line("Current list of receiving vehicles (%d): [%s]", s_iCountHistoryRxVehicles, szTmp);

   if ( s_iCountHistoryRxVehicles < MAX_CONCURENT_VEHICLES )
      s_iCountHistoryRxVehicles++;

   s_ListHistoryRxVehicles[s_iCountHistoryRxVehicles-1].uVehicleId = uVehicleId;
   return s_iCountHistoryRxVehicles-1;
}

// Returns 0 if duplicate packet, 1 if ok, -1 for error

int radio_stats_update_on_packet_received(shared_mem_radio_stats* pSMRS, u32 timeNow, int iInterfaceIndex, u8* pPacketBuffer, int iPacketLength, int iCRCOk)
{
   if ( NULL == pSMRS )
      return -1;

   if ( iInterfaceIndex < 0 || iInterfaceIndex >= hardware_get_radio_interfaces_count() )
   {
      log_softerror_and_alarm("Tried to update radio stats on invalid radio interface number %d.", iInterfaceIndex+1);
      return -1;
   }
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("Tried to update radio stats on invalid radio interface number %d. Invalid radio info.", iInterfaceIndex+1);
      return -1;
   }

   pSMRS->timeLastRxPacket = timeNow;
   
   int nRadioLinkId = pSMRS->radio_interfaces[iInterfaceIndex].assignedRadioLinkId;

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   pSMRS->radio_interfaces[iInterfaceIndex].lastDbm = pRadioInfo->monitor_interface_read.radioInfo.nDbm;
   pSMRS->radio_interfaces[iInterfaceIndex].lastDataRate = pRadioInfo->monitor_interface_read.radioInfo.nRate;
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
   {
      pSMRS->radio_interfaces[iInterfaceIndex].lastDbmVideo = pRadioInfo->monitor_interface_read.radioInfo.nDbm;
      pSMRS->radio_interfaces[iInterfaceIndex].lastDataRateVideo = pRadioInfo->monitor_interface_read.radioInfo.nRate;
   }
   else
   {
      pSMRS->radio_interfaces[iInterfaceIndex].lastDbmData = pRadioInfo->monitor_interface_read.radioInfo.nDbm;
      pSMRS->radio_interfaces[iInterfaceIndex].lastDataRateData = pRadioInfo->monitor_interface_read.radioInfo.nRate;
   }

   u32 uPacketIndex = (pPH->stream_packet_idx) & PACKET_FLAGS_MASK_STREAM_PACKET_IDX;
   u32 uStreamIndex = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   if ( uStreamIndex >= MAX_RADIO_STREAMS )
      uStreamIndex = 0;

   u32 uVehicleId = pPH->vehicle_id_src;

   // Figure out what vehicle local statistics to use

   int iStatsIndexVehicle = _radio_stats_get_vehicle_runtime_info_index(uVehicleId);

   // -------------------------------------------------------------
   // Begin - Update last received packet time

   u32 uTimeGap = timeNow - pSMRS->radio_interfaces[iInterfaceIndex].timeLastRxPacket;
   if ( 0 == pSMRS->radio_interfaces[iInterfaceIndex].timeLastRxPacket )
      uTimeGap = 0;
   if ( uTimeGap > 254 )
      uTimeGap = 254;
   if ( pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[0] == 0xFF )
      pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[0] = uTimeGap;
   else if ( uTimeGap > pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[0] )
      pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[0] = uTimeGap;
     
   pSMRS->radio_interfaces[iInterfaceIndex].timeLastRxPacket = timeNow;
   pSMRS->radio_streams[uStreamIndex].timeLastRxPacket = timeNow;

   if ( nRadioLinkId >= 0 && nRadioLinkId < MAX_RADIO_INTERFACES )
   {
      pSMRS->radio_links[nRadioLinkId].timeLastRxPacket = timeNow;
      pSMRS->radio_links[nRadioLinkId].streamTimeLastRxPacket[uStreamIndex] = timeNow;
   }

   // End - Update last received packet time

   if ( nRadioLinkId < 0 || nRadioLinkId >= MAX_RADIO_INTERFACES )
   {
      if ( timeNow > s_uLastTimeDebugPacketRecvOnNoLink + 3000 )
      {
         s_uLastTimeDebugPacketRecvOnNoLink = timeNow;
         log_softerror_and_alarm("Received radio packet on radio interface %d that is not assigned to any radio links.", iInterfaceIndex+1);
      }
      return -1;
   }
   // ----------------------------------------------------------------------
   // Update rx bytes and packets count on interface

   pSMRS->radio_interfaces[iInterfaceIndex].totalRxBytes += iPacketLength;
   pSMRS->radio_interfaces[iInterfaceIndex].tmpRxBytes += iPacketLength;

   pSMRS->radio_interfaces[iInterfaceIndex].totalRxPackets++;
   pSMRS->radio_interfaces[iInterfaceIndex].tmpRxPackets++;

   // -------------------------------------------------------------------------
   // Begin - Update history and good/bad/lost packets for interface 

   pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPackets++;
   s_uControllerLinkStats_tmpRecv[iInterfaceIndex]++;

   if ( iPacketLength <= 0 || (0 == iCRCOk) )
   {
      pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPacketsBad++;
      s_uControllerLinkStats_tmpRecvBad[iInterfaceIndex]++;
   }


   if ( s_ListHistoryRxVehicles[iStatsIndexVehicle].uLastReceivedPacketIndexesPerRadioInterfacePerStream[iInterfaceIndex][uStreamIndex] != MAX_U32 )
   if ( uPacketIndex > s_ListHistoryRxVehicles[iStatsIndexVehicle].uLastReceivedPacketIndexesPerRadioInterfacePerStream[iInterfaceIndex][uStreamIndex] )
   {
      u32 diff = uPacketIndex - s_ListHistoryRxVehicles[iStatsIndexVehicle].uLastReceivedPacketIndexesPerRadioInterfacePerStream[iInterfaceIndex][uStreamIndex];
      pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPacketsLost += diff - 1;
      pSMRS->radio_interfaces[iInterfaceIndex].totalRxPacketsLost += diff - 1;
      s_uControllerLinkStats_tmpRecvLost[iInterfaceIndex] += diff - 1;
   }

   s_ListHistoryRxVehicles[iStatsIndexVehicle].uLastReceivedPacketIndexesPerRadioInterfacePerStream[iInterfaceIndex][uStreamIndex] = uPacketIndex;

   /*
   if ( pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedStreamPacketIndex[uStreamIndex] != 0 )
   if ( uPacketIndex > pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedStreamPacketIndex[uStreamIndex] )
   {
      u32 diff = uPacketIndex - pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedStreamPacketIndex[uStreamIndex];
      //if ( diff > 1 || diff == 0 )
      //   log_line("DBG missing packet for vid: %u, stream: %u, last: %u, now: %u", uVehicleId, uStreamIndex, pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedStreamPacketIndex[uStreamIndex], uPacketIndex);
      pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPacketsLost += diff - 1;
      pSMRS->radio_interfaces[iInterfaceIndex].totalRxPacketsLost += diff - 1;
      s_uControllerLinkStats_tmpRecvLost[iInterfaceIndex] += diff - 1;
   }
   */
   pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedStreamPacketIndex[uStreamIndex] = uPacketIndex;

   // End - Update history and good/bad/lost packets for interface 


   // ---------------------------------------------------
   // Check for packet duplication on stream for current vehicle

   int bIsNewPacketOnStream = 0;

   if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStream = 1;
   else if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStream = 0;
   else if ( uPacketIndex > s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStream = 1;
   else
   {
      int iFound = 0;
      if ( s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uCurrentBufferIndex > 0 )
         for( u32 u=s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uCurrentBufferIndex-1; u>=0; u-- )
         {
            if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsIndexes[u] )
               break;
            if ( timeNow > 1000 && s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsTimes[u] < timeNow-1000 )
               break;
            if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsIndexes[u] )
            {
               iFound = 1;
               break;
            }
         }
      if ( ! iFound )
         for( u32 u=MAX_HISTORY_RX_PACKETS_INDEXES-1; u>s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uCurrentBufferIndex; u-- )
         {
            if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsIndexes[u] )
               break;
            if ( timeNow > 1000 && s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsTimes[u] < timeNow-1000 )
               break;
            
            if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsIndexes[u] )
            {
               iFound = 1;
               break;
            }
         }
      if ( iFound )
         bIsNewPacketOnStream = 0;
      else
         bIsNewPacketOnStream = 1;
   }
   
   if ( bIsNewPacketOnStream )
   {
      u32 uCurrentIndex = s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uCurrentBufferIndex;
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsIndexes[uCurrentIndex] = uPacketIndex;
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsTimes[uCurrentIndex] = timeNow;
      if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex )
         s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex = uPacketIndex;
      else if ( uPacketIndex > s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex )
         s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex = uPacketIndex;

      uCurrentIndex++;
      if ( uCurrentIndex >= MAX_HISTORY_RX_PACKETS_INDEXES )
         uCurrentIndex = 0;
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uCurrentBufferIndex = uCurrentIndex;

      pSMRS->radio_streams[uStreamIndex].totalRxBytes += iPacketLength;
      pSMRS->radio_streams[uStreamIndex].tmpRxBytes += iPacketLength;

      pSMRS->radio_streams[uStreamIndex].totalRxPackets++;
      pSMRS->radio_streams[uStreamIndex].tmpRxPackets++;

      // TO FIX compute lost packets count per stream
      //if ( s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] != 0 && s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] != 0 )
      //if ( s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] != s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] + 1 )
      //   pSMRS->radio_streams[uStreamIndex].totalLostPackets += s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] - s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] - 1;
   }

   // End - Check for packet duplication on stream for current vehicle
   // ---------------------------------------------------
   
   // ------------------------------------------------------------
   // Begin - Check for packet duplication per radio link and per stream

   int bIsNewPacketOnStreamOnRadioLink = 0;

   if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStreamOnRadioLink = 1;
   else if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStreamOnRadioLink = 0;
   else if ( uPacketIndex > s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStreamOnRadioLink = 1;
   else
   {
      int iFound = 0;
      if ( s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uCurrentBufferIndex > 0 )
         for( u32 u=s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uCurrentBufferIndex-1; u>=0; u-- )
         {
            if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsIndexes[u] )
               break;
            if ( timeNow > 1000 && s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsTimes[u] < timeNow-1000 )
               break;
            if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsIndexes[u] )
            {
               iFound = 1;
               break;
            }
         }
      if ( ! iFound )
         for( u32 u=MAX_HISTORY_RX_PACKETS_INDEXES-1; u>s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uCurrentBufferIndex; u-- )
         {
            if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsIndexes[u] )
               break;
            if ( timeNow > 1000 && s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsTimes[u] < timeNow-1000 )
               break;
            
            if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsIndexes[u] )
            {
               iFound = 1;
               break;
            }
         }
      if ( iFound )
         bIsNewPacketOnStreamOnRadioLink = 0;
      else
         bIsNewPacketOnStreamOnRadioLink = 1;
   }
   
   if ( bIsNewPacketOnStreamOnRadioLink )
   {
      u32 uCurrentIndex = s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uCurrentBufferIndex;
      
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsIndexes[uCurrentIndex] = uPacketIndex;
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsTimes[uCurrentIndex] = timeNow;
      if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex )
         s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex = uPacketIndex;
      else if ( uPacketIndex > s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex )
         s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex = uPacketIndex;

      uCurrentIndex++;
      if ( uCurrentIndex >= MAX_HISTORY_RX_PACKETS_INDEXES )
         uCurrentIndex = 0;
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uCurrentBufferIndex = uCurrentIndex;

      pSMRS->radio_links[nRadioLinkId].totalRxBytes += iPacketLength;
      pSMRS->radio_links[nRadioLinkId].tmpRxBytes += iPacketLength;
      
      pSMRS->radio_links[nRadioLinkId].totalRxPackets++;
      pSMRS->radio_links[nRadioLinkId].tmpRxPackets++;

      pSMRS->radio_links[nRadioLinkId].streamTotalRxBytes[uStreamIndex] += iPacketLength;
      pSMRS->radio_links[nRadioLinkId].stream_tmpRxBytes[uStreamIndex] += iPacketLength;

      pSMRS->radio_links[nRadioLinkId].streamTotalRxPackets[uStreamIndex]++;
      pSMRS->radio_links[nRadioLinkId].stream_tmpRxPackets[uStreamIndex]++;

      // TO FIX: compute total rx time for all links
      //pSMRS->radio_links[nRadioLinkId].tmp_downlink_tx_time_per_sec += 0;
      //pSMRS->tmp_all_downlinks_tx_time_per_sec += 0;//pPH->tx_time;
      
      // TO FIX compute lost packets count per stream
      //if ( s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] != 0 && s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] != 0 )
      //if ( s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] != s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] + 1 )
      //   pSMRS->radio_streams[uStreamIndex].totalLostPackets += s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] - s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] - 1;
   }

   // End - Check for stream packet duplication per radio link

   return bIsNewPacketOnStream;
}

// Returns 0 if duplicate packet, 1 if ok, -1 for error

int radio_stats_update_on_short_packet_received(shared_mem_radio_stats* pSMRS, u32 timeNow, int iInterfaceIndex, u8* pPacketBuffer, int iPacketLength)
{
   if ( NULL == pSMRS )
      return -1;

   if ( iInterfaceIndex < 0 || iInterfaceIndex >= hardware_get_radio_interfaces_count() )
   {
      log_softerror_and_alarm("Tried to update radio stats on invalid radio interface number %d.", iInterfaceIndex+1);
      return -1;
   }
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("Tried to update radio stats on invalid radio interface number %d. Invalid radio info.", iInterfaceIndex+1);
      return -1;
   }

   pSMRS->timeLastRxPacket = timeNow;
   
   int nRadioLinkId = pSMRS->radio_interfaces[iInterfaceIndex].assignedRadioLinkId;

   t_packet_header_short* pPHS = (t_packet_header_short*)pPacketBuffer;

   u32 uPacketIndex = (pPHS->stream_packet_idx) & PACKET_FLAGS_MASK_STREAM_PACKET_IDX;
   u32 uStreamIndex = (pPHS->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   if ( uStreamIndex >= MAX_RADIO_STREAMS )
      uStreamIndex = 0;

   u32 uVehicleId = pPHS->vehicle_id_src;

   // Figure out what vehicle local statistics to use

   int iStatsIndexVehicle = _radio_stats_get_vehicle_runtime_info_index(uVehicleId);

   // -------------------------------------------------------------
   // Begin - Update last received packet time

   u32 uTimeGap = timeNow - pSMRS->radio_interfaces[iInterfaceIndex].timeLastRxPacket;
   if ( 0 == pSMRS->radio_interfaces[iInterfaceIndex].timeLastRxPacket )
      uTimeGap = 0;
   if ( uTimeGap > 254 )
      uTimeGap = 254;
   if ( pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[0] == 0xFF )
      pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[0] = uTimeGap;
   else if ( uTimeGap > pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[0] )
      pSMRS->radio_interfaces[iInterfaceIndex].hist_rxGapMiliseconds[0] = uTimeGap;
     
   pSMRS->radio_interfaces[iInterfaceIndex].timeLastRxPacket = timeNow;
   pSMRS->radio_streams[uStreamIndex].timeLastRxPacket = timeNow;

   if ( nRadioLinkId >= 0 && nRadioLinkId < MAX_RADIO_INTERFACES )
   {
      pSMRS->radio_links[nRadioLinkId].timeLastRxPacket = timeNow;
      pSMRS->radio_links[nRadioLinkId].streamTimeLastRxPacket[uStreamIndex] = timeNow;
   }

   // End - Update last received packet time
   // -----------------------------------------------------------

   if ( nRadioLinkId < 0 || nRadioLinkId >= MAX_RADIO_INTERFACES )
   {
      if ( timeNow > s_uLastTimeDebugPacketRecvOnNoLink + 3000 )
      {
         s_uLastTimeDebugPacketRecvOnNoLink = timeNow;
         log_softerror_and_alarm("Received radio packet on radio interface %d that is not assigned to any radio links.", iInterfaceIndex+1);
      }
      return -1;
   }

   // ----------------------------------------------------------------------
   // Update rx bytes and packets count on interface

   pSMRS->radio_interfaces[iInterfaceIndex].totalRxBytes += iPacketLength;
   pSMRS->radio_interfaces[iInterfaceIndex].tmpRxBytes += iPacketLength;

   pSMRS->radio_interfaces[iInterfaceIndex].totalRxPackets++;
   pSMRS->radio_interfaces[iInterfaceIndex].tmpRxPackets++;

   // -------------------------------------------------------------------------
   // Begin - Update history and good/bad/lost packets for interface 

   pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPackets++;
   s_uControllerLinkStats_tmpRecv[iInterfaceIndex]++;

   u32 diff = (u32)pPHS->packet_index - s_ListHistoryRxVehicles[iStatsIndexVehicle].s_uLastReceivedShortPacketsIndexes[iInterfaceIndex];
   if ( pPHS->packet_index < s_ListHistoryRxVehicles[iStatsIndexVehicle].s_uLastReceivedShortPacketsIndexes[iInterfaceIndex] )
      diff = (u32)pPHS->packet_index + 256 - s_ListHistoryRxVehicles[iStatsIndexVehicle].s_uLastReceivedShortPacketsIndexes[iInterfaceIndex];
   if ( diff > 1 )
   {
      pSMRS->radio_interfaces[iInterfaceIndex].hist_tmp_rxPacketsLost += diff - 1;
      pSMRS->radio_interfaces[iInterfaceIndex].totalRxPacketsLost += diff - 1;
      s_uControllerLinkStats_tmpRecvLost[iInterfaceIndex] += diff - 1;
   }

   s_ListHistoryRxVehicles[iStatsIndexVehicle].s_uLastReceivedShortPacketsIndexes[iInterfaceIndex] = pPHS->packet_index;
   s_ListHistoryRxVehicles[iStatsIndexVehicle].uLastReceivedPacketIndexesPerRadioInterfacePerStream[iInterfaceIndex][uStreamIndex] = uPacketIndex;

   pSMRS->radio_interfaces[iInterfaceIndex].lastReceivedStreamPacketIndex[uStreamIndex] = uPacketIndex;

   // End - Update history and good/bad/lost packets for interface 
   // ------------------------------------------------------------

   // -----------------------------------------
   // Check for stream packet duplication for current vehicle

   int bIsNewPacketOnStream = 0;

   if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStream = 1;
   else if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStream = 0;
   else if ( uPacketIndex > s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStream = 1;
   else
   {
      int iFound = 0;
      if ( s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uCurrentBufferIndex > 0 )
         for( u32 u=s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uCurrentBufferIndex-1; u>=0; u-- )
         {
            if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsIndexes[u] )
               break;
            if ( timeNow > 1000 && s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsTimes[u] < timeNow-1000 )
               break;
            if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsIndexes[u] )
            {
               iFound = 1;
               break;
            }
         }
      if ( ! iFound )
         for( u32 u=MAX_HISTORY_RX_PACKETS_INDEXES-1; u>s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uCurrentBufferIndex; u-- )
         {
            if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsIndexes[u] )
               break;
            if ( timeNow > 1000 && s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsTimes[u] < timeNow-1000 )
               break;
            
            if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsIndexes[u] )
            {
               iFound = 1;
               break;
            }
         }
      if ( iFound )
         bIsNewPacketOnStream = 0;
      else
         bIsNewPacketOnStream = 1;
   }
   
   if ( bIsNewPacketOnStream )
   {
      u32 uCurrentIndex = s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uCurrentBufferIndex;
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsIndexes[uCurrentIndex] = uPacketIndex;
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].packetsTimes[uCurrentIndex] = timeNow;
      if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex )
         s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex = uPacketIndex;
      else if ( uPacketIndex > s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex )
         s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uMaxReceivedPacketIndex = uPacketIndex;

      uCurrentIndex++;
      if ( uCurrentIndex >= MAX_HISTORY_RX_PACKETS_INDEXES )
         uCurrentIndex = 0;
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistory[uStreamIndex].uCurrentBufferIndex = uCurrentIndex;

      pSMRS->radio_streams[uStreamIndex].totalRxBytes += iPacketLength;
      pSMRS->radio_streams[uStreamIndex].tmpRxBytes += iPacketLength;

      pSMRS->radio_streams[uStreamIndex].totalRxPackets++;
      pSMRS->radio_streams[uStreamIndex].tmpRxPackets++;

      // TO FIX compute lost packets count per stream
      //if ( s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] != 0 && s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] != 0 )
      //if ( s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] != s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] + 1 )
      //   pSMRS->radio_streams[uStreamIndex].totalLostPackets += s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] - s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] - 1;
   }

   // End - Check for stream packet duplication

   // ------------------------------------------------------------
   // Begin - Check for stream packet duplication per radio link and per stream

   int bIsNewPacketOnStreamOnRadioLink = 0;

   if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStreamOnRadioLink = 1;
   else if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStreamOnRadioLink = 0;
   else if ( uPacketIndex > s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex )
      bIsNewPacketOnStreamOnRadioLink = 1;
   else
   {
      int iFound = 0;
      if ( s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uCurrentBufferIndex > 0 )
         for( u32 u=s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uCurrentBufferIndex-1; u>=0; u-- )
         {
            if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsIndexes[u] )
               break;
            if ( timeNow > 1000 && s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsTimes[u] < timeNow-1000 )
               break;
            if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsIndexes[u] )
            {
               iFound = 1;
               break;
            }
         }
      if ( ! iFound )
         for( u32 u=MAX_HISTORY_RX_PACKETS_INDEXES-1; u>s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uCurrentBufferIndex; u-- )
         {
            if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsIndexes[u] )
               break;
            if ( timeNow > 1000 && s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsTimes[u] < timeNow-1000 )
               break;
            
            if ( uPacketIndex == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsIndexes[u] )
            {
               iFound = 1;
               break;
            }
         }
      if ( iFound )
         bIsNewPacketOnStreamOnRadioLink = 0;
      else
         bIsNewPacketOnStreamOnRadioLink = 1;
   }
   
   if ( bIsNewPacketOnStreamOnRadioLink )
   {
      u32 uCurrentIndex = s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uCurrentBufferIndex;
      
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsIndexes[uCurrentIndex] = uPacketIndex;
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].packetsTimes[uCurrentIndex] = timeNow;
      if ( MAX_U32 == s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex )
         s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex = uPacketIndex;
      else if ( uPacketIndex > s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex )
         s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uMaxReceivedPacketIndex = uPacketIndex;

      uCurrentIndex++;
      if ( uCurrentIndex >= MAX_HISTORY_RX_PACKETS_INDEXES )
         uCurrentIndex = 0;
      s_ListHistoryRxVehicles[iStatsIndexVehicle].streamsHistoryPerRadioLink[nRadioLinkId][uStreamIndex].uCurrentBufferIndex = uCurrentIndex;

      pSMRS->radio_links[nRadioLinkId].totalRxBytes += iPacketLength;
      pSMRS->radio_links[nRadioLinkId].tmpRxBytes += iPacketLength;
      
      pSMRS->radio_links[nRadioLinkId].totalRxPackets++;
      pSMRS->radio_links[nRadioLinkId].tmpRxPackets++;

      pSMRS->radio_links[nRadioLinkId].streamTotalRxBytes[uStreamIndex] += iPacketLength;
      pSMRS->radio_links[nRadioLinkId].stream_tmpRxBytes[uStreamIndex] += iPacketLength;

      pSMRS->radio_links[nRadioLinkId].streamTotalRxPackets[uStreamIndex]++;
      pSMRS->radio_links[nRadioLinkId].stream_tmpRxPackets[uStreamIndex]++;

      // TO FIX: compute total rx time for all links
      //pSMRS->radio_links[nRadioLinkId].tmp_downlink_tx_time_per_sec += 0;
      //pSMRS->tmp_all_downlinks_tx_time_per_sec += 0;//pPH->tx_time;
      
      // TO FIX compute lost packets count per stream
      //if ( s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] != 0 && s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] != 0 )
      //if ( s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] != s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] + 1 )
      //   pSMRS->radio_streams[uStreamIndex].totalLostPackets += s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][0] - s_uStreamsLastPacketsHistoryIndexes[uStreamIndex][1] - 1;
   }

   // End - Check for stream packet duplication per radio link

   return bIsNewPacketOnStream;
}


void radio_stats_update_on_packet_sent_on_radio_interface(shared_mem_radio_stats* pSMRS, u32 timeNow, int interfaceIndex, int iPacketLength)
{
   if ( NULL == pSMRS )
      return;
   if ( interfaceIndex < 0 || interfaceIndex >= hardware_get_radio_interfaces_count() )
      return;
   
   // Update radio interfaces

   pSMRS->radio_interfaces[interfaceIndex].totalTxBytes += iPacketLength;
   pSMRS->radio_interfaces[interfaceIndex].tmpTxBytes += iPacketLength;

   pSMRS->radio_interfaces[interfaceIndex].totalTxPackets++;
   pSMRS->radio_interfaces[interfaceIndex].tmpTxPackets++;
}

void radio_stats_update_on_packet_sent_on_radio_link(shared_mem_radio_stats* pSMRS, u32 timeNow, int iLinkIndex, int iStreamIndex, int iPacketLength, int iChainedCount)
{
   if ( NULL == pSMRS )
      return;
   if ( iLinkIndex < 0 || iLinkIndex >= MAX_RADIO_INTERFACES )
      iLinkIndex = 0;
   if ( iStreamIndex < 0 || iStreamIndex >= MAX_RADIO_STREAMS )
      iStreamIndex = 0;

   // Update radio link

   pSMRS->radio_links[iLinkIndex].totalTxBytes += iPacketLength;
   pSMRS->radio_links[iLinkIndex].tmpTxBytes += iPacketLength;

   pSMRS->radio_links[iLinkIndex].totalTxPackets++;
   pSMRS->radio_links[iLinkIndex].tmpTxPackets++;

   pSMRS->radio_links[iLinkIndex].totalTxPacketsUncompressed += iChainedCount;
   pSMRS->radio_links[iLinkIndex].tmpUncompressedTxPackets += iChainedCount;

   pSMRS->radio_links[iLinkIndex].streamTotalTxBytes[iStreamIndex] += iPacketLength;
   pSMRS->radio_links[iLinkIndex].stream_tmpTxBytes[iStreamIndex] += iPacketLength;

   pSMRS->radio_links[iLinkIndex].streamTotalTxPackets[iStreamIndex]++;
   pSMRS->radio_links[iLinkIndex].stream_tmpTxPackets[iStreamIndex]++;
}

void radio_stats_update_on_packet_sent_for_radio_stream(shared_mem_radio_stats* pSMRS, u32 timeNow, int iStreamIndex, int iPacketLength)
{
   if ( NULL == pSMRS )
      return;
   if ( iStreamIndex < 0 || iStreamIndex >= MAX_RADIO_STREAMS )
      iStreamIndex = 0;

   // Update radio streams

   pSMRS->radio_streams[iStreamIndex].totalTxBytes += iPacketLength;
   pSMRS->radio_streams[iStreamIndex].tmpTxBytes += iPacketLength;

   pSMRS->radio_streams[iStreamIndex].totalTxPackets++;
   pSMRS->radio_streams[iStreamIndex].tmpTxPackets++;
}

void radio_controller_links_stats_reset(t_packet_data_controller_link_stats* pControllerStats)
{
   if ( NULL == pControllerStats )
      return;

   pControllerStats->flagsAndVersion = 0;
   pControllerStats->lastUpdateTime = 0;
   pControllerStats->radio_interfaces_count = hardware_get_radio_interfaces_count();
   pControllerStats->video_streams_count = 1;

   for( int i=0; i<CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES; i++ )
   {
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
         pControllerStats->radio_interfaces_rx_quality[k][i] = 255;
      for( int k=0; k<MAX_VIDEO_STREAMS; k++ )
      {
         pControllerStats->radio_streams_rx_quality[k][i] = 255;
         pControllerStats->video_streams_blocks_clean[k][i] = 0;
         pControllerStats->video_streams_blocks_reconstructed[k][i] = 0;
         pControllerStats->video_streams_blocks_max_ec_packets_used[k][i] = 0;
         pControllerStats->video_streams_requested_retransmission_packets[k][i] = 0;
      }
   }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      s_uControllerLinkStats_tmpRecv[i] = 0;
      s_uControllerLinkStats_tmpRecvBad[i] = 0;
      s_uControllerLinkStats_tmpRecvLost[i] = 0;
   }

   for( int k=0; k<MAX_VIDEO_STREAMS; k++ )
   {
      pControllerStats->tmp_radio_streams_rx_quality[k] = 0;
      pControllerStats->tmp_video_streams_blocks_clean[k] = 0;
      pControllerStats->tmp_video_streams_blocks_reconstructed[k] = 0;
      pControllerStats->tmp_video_streams_requested_retransmission_packets[k] = 0;
   }

}

void radio_controller_links_stats_periodic_update(t_packet_data_controller_link_stats* pControllerStats, u32 timeNow)
{
   if ( NULL == pControllerStats )
      return;

   if ( timeNow >= pControllerStats->lastUpdateTime && timeNow < pControllerStats->lastUpdateTime + CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL_MS )
      return;
   
   pControllerStats->lastUpdateTime = timeNow;

   for( int i=0; i<pControllerStats->radio_interfaces_count; i++ )
   {
      for( int k=CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES-1; k>0; k-- )
         pControllerStats->radio_interfaces_rx_quality[i][k] = pControllerStats->radio_interfaces_rx_quality[i][k-1];

      if ( 0 == s_uControllerLinkStats_tmpRecv[i] )
         pControllerStats->radio_interfaces_rx_quality[i][0] = 0;
      else
         pControllerStats->radio_interfaces_rx_quality[i][0] = 100 - (100*(s_uControllerLinkStats_tmpRecvBad[i]+s_uControllerLinkStats_tmpRecvLost[i]))/(s_uControllerLinkStats_tmpRecv[i]+s_uControllerLinkStats_tmpRecvLost[i]);

      s_uControllerLinkStats_tmpRecv[i] = 0;
      s_uControllerLinkStats_tmpRecvBad[i] = 0;
      s_uControllerLinkStats_tmpRecvLost[i] = 0;
   }

   for( int i=0; i<pControllerStats->video_streams_count; i++ )
   {
      for( int k=CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES-1; k>0; k-- )
      {
         pControllerStats->radio_streams_rx_quality[i][k] = pControllerStats->radio_streams_rx_quality[i][k-1];
         pControllerStats->video_streams_blocks_clean[i][k] = pControllerStats->video_streams_blocks_clean[i][k-1];
         pControllerStats->video_streams_blocks_reconstructed[i][k] = pControllerStats->video_streams_blocks_reconstructed[i][k-1];
         pControllerStats->video_streams_blocks_max_ec_packets_used[i][k] = pControllerStats->video_streams_blocks_max_ec_packets_used[i][k-1];
         pControllerStats->video_streams_requested_retransmission_packets[i][k] = pControllerStats->video_streams_requested_retransmission_packets[i][k-1];
      }
      pControllerStats->radio_streams_rx_quality[i][0] = pControllerStats->tmp_radio_streams_rx_quality[i];
      pControllerStats->video_streams_blocks_clean[i][0] = pControllerStats->tmp_video_streams_blocks_clean[i];
      pControllerStats->video_streams_blocks_reconstructed[i][0] = pControllerStats->tmp_video_streams_blocks_reconstructed[i];
      pControllerStats->video_streams_blocks_max_ec_packets_used[i][0] = pControllerStats->tmp_video_streams_blocks_max_ec_packets_used[i];
      pControllerStats->video_streams_requested_retransmission_packets[i][0] = pControllerStats->tmp_video_streams_requested_retransmission_packets[i];

      pControllerStats->tmp_radio_streams_rx_quality[i] = 0;
      pControllerStats->tmp_video_streams_blocks_clean[i] = 0;
      pControllerStats->tmp_video_streams_blocks_reconstructed[i] = 0;
      pControllerStats->tmp_video_streams_blocks_max_ec_packets_used[i] = 0;
      pControllerStats->tmp_video_streams_requested_retransmission_packets[i] = 0;
   }
}

u32 radio_stats_get_max_received_packet_index_for_stream(u32 uVehicleId, u32 uStreamId)
{
   if ( uVehicleId == 0 || uVehicleId == MAX_U32 )
      return 0;

   if ( uStreamId >= MAX_RADIO_STREAMS )
      return 0;

   for( int i=0; i<s_iCountHistoryRxVehicles; i++ )
   {
      if ( s_ListHistoryRxVehicles[i].uVehicleId != uVehicleId )
         continue;
      if ( MAX_U32 != s_ListHistoryRxVehicles[i].streamsHistory[uStreamId].uMaxReceivedPacketIndex )
         return s_ListHistoryRxVehicles[i].streamsHistory[uStreamId].uMaxReceivedPacketIndex;
   }

   return 0;
}