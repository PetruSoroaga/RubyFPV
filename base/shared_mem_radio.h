#pragma once
#include "base.h"
#include "config.h"

#define MAX_HISTORY_RADIO_STATS_RECV_SLICES 40



typedef struct
{
   u32 totalRxBytes;
   u32 totalTxBytes;
   u32 rxBytesPerSec;
   u32 txBytesPerSec;
   u32 totalRxPackets;
   u32 totalTxPackets;
   u32 totalLostPackets;
   u32 rxPacketsPerSec;
   u32 txPacketsPerSec;
   u32 timeLastRxPacket;
   u32 timeLastTxPacket;

   u32 tmpRxBytes;
   u32 tmpTxBytes;
   u32 tmpRxPackets;
   u32 tmpTxPackets;
} __attribute__((packed)) shared_mem_radio_stats_stream;

typedef struct
{
   int assignedRadioLinkId; // id of the radio link assigned to this radio interface
   int currentFrequency;
   u8 openedForRead;
   u8 openedForWrite;
   int lastDbm;
   int lastDbmVideo;
   int lastDbmData;
   u8  lastDataRate;
   u8  lastDataRateVideo; // in 0.5 mbs increments
   u8  lastDataRateData; // in 0.5 mbs increments

   u32 totalRxBytes;
   u32 totalTxBytes;
   u32 rxBytesPerSec;
   u32 txBytesPerSec;
   u32 totalRxPackets;
   u32 totalRxPacketsBad;
   u32 totalRxPacketsLost;
   u32 totalTxPackets;
   u32 rxPacketsPerSec;
   u32 txPacketsPerSec;
   u32 timeLastRxPacket;
   u32 timeLastTxPacket;

   u32 tmpRxBytes;
   u32 tmpTxBytes;
   u32 tmpRxPackets;
   u32 tmpTxPackets;

   int rxQuality; // 0...100%
   int rxRelativeQuality; // higher value means better link; it's relative to the other radio interfaces

   u32 lastReceivedStreamPacketIndex[MAX_RADIO_STREAMS]; // Used for lost packets computation

   u8 hist_rxPackets[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxPacketsBad[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxPacketsLost[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxGapMiliseconds[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u32 hist_tmp_rxPackets;
   u32 hist_tmp_rxPacketsBad;
   u32 hist_tmp_rxPacketsLost;

} __attribute__((packed)) shared_mem_radio_stats_radio_interface;


typedef struct
{
   u32 totalRxBytes;
   u32 totalTxBytes;
   u32 rxBytesPerSec;
   u32 txBytesPerSec;
   u32 totalRxPackets;
   u32 totalTxPackets;
   u32 rxPacketsPerSec;
   u32 txPacketsPerSec;
   u32 totalTxPacketsUncompressed;
   u32 txUncompressedPacketsPerSec;
   u32 timeLastRxPacket;
   u32 timeLastTxPacket;

   u32 tmpRxBytes;
   u32 tmpTxBytes;
   u32 tmpRxPackets;
   u32 tmpTxPackets;
   u32 tmpUncompressedTxPackets;

   u32 streamTotalRxBytes[MAX_RADIO_STREAMS];
   u32 streamTotalTxBytes[MAX_RADIO_STREAMS];
   u32 streamRxBytesPerSec[MAX_RADIO_STREAMS];
   u32 streamTxBytesPerSec[MAX_RADIO_STREAMS];
   u32 streamTotalRxPackets[MAX_RADIO_STREAMS];
   u32 streamTotalTxPackets[MAX_RADIO_STREAMS];
   u32 streamRxPacketsPerSec[MAX_RADIO_STREAMS];
   u32 streamTxPacketsPerSec[MAX_RADIO_STREAMS];
   u32 streamTimeLastRxPacket[MAX_RADIO_STREAMS];
   u32 streamTimeLastTxPacket[MAX_RADIO_STREAMS];

   u32 stream_tmpRxBytes[MAX_RADIO_STREAMS];
   u32 stream_tmpTxBytes[MAX_RADIO_STREAMS];
   u32 stream_tmpRxPackets[MAX_RADIO_STREAMS];
   u32 stream_tmpTxPackets[MAX_RADIO_STREAMS];

   u32 linkDelayRoundtripMs;
   u32 linkDelayRoundtripMinimMs;
   int lastTxInterfaceIndex;

   u32 downlink_tx_time_per_sec;
   u32 tmp_downlink_tx_time_per_sec;

} __attribute__((packed)) shared_mem_radio_stats_radio_link;


typedef struct
{
   int countRadioLinks;
   int countRadioInterfaces;
   int graphRefreshIntervalMs;
   int refreshIntervalMs;

   u32 lastComputeTime;
   u32 lastComputeTimeGraph;

   u32 all_downlinks_tx_time_per_sec;
   u32 tmp_all_downlinks_tx_time_per_sec;

   u32 uAverageCommandRoundtripMiliseconds;
   u32 uMaxCommandRoundtripMiliseconds;
   u32 uMinCommandRoundtripMiliseconds;

   u32 timeLastRxPacket;
   
   shared_mem_radio_stats_stream           radio_streams[MAX_RADIO_STREAMS];
   shared_mem_radio_stats_radio_interface  radio_interfaces[MAX_RADIO_INTERFACES];
   shared_mem_radio_stats_radio_link       radio_links[MAX_RADIO_INTERFACES];

} __attribute__((packed)) shared_mem_radio_stats;

