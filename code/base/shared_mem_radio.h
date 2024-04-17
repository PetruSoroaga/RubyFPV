#pragma once
#include "base.h"
#include "config.h"

#define MAX_HISTORY_RADIO_STATS_RECV_SLICES 40
#define MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES 100
#define MAX_RX_GRAPH_SLICES 300

typedef struct
{
   u8 rxPackets[MAX_RX_GRAPH_SLICES];
   u8 rxPacketsBad[MAX_RX_GRAPH_SLICES];
   u8 rxPacketsLost[MAX_RX_GRAPH_SLICES];
   u8 rxGapMiliseconds[MAX_RX_GRAPH_SLICES];
   u32 tmp_rxPackets;
   u32 tmp_rxPacketsBad;
   u32 tmp_rxPacketsLost;
} __attribute__((packed)) shared_mem_radio_stats_interface_rx_graph;

typedef struct
{
   int iCurrentSlice;
   u32 uTimeSliceDurationMs;
   u32 uTimeStartCurrentSlice;
   shared_mem_radio_stats_interface_rx_graph interfaces[MAX_RADIO_INTERFACES];
} __attribute__((packed)) shared_mem_radio_stats_interfaces_rx_graph;


typedef struct
{
   u8 uHistPacketsTypes[MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES];
   u8 uHistPacketsCount[MAX_RADIO_STATS_INTERFACE_RX_HISTORY_SLICES];

   // type: radio packet type or:
   // 0x00 - none if count is 0. missing packets if count > 0
   // 0xFF - end of a second

   int iCurrentSlice;
   u32 uTimeLastUpdate;
} __attribute__((packed)) shared_mem_radio_stats_interface_rx_hist;

typedef struct
{
   shared_mem_radio_stats_interface_rx_hist interfaces_history[MAX_RADIO_INTERFACES];
} __attribute__((packed)) shared_mem_radio_stats_rx_hist;

typedef struct
{
   u32 uVehicleId;
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
   u32 uLastRecvStreamPacketIndex;
   int iHasMissingStreamPacketsFlag;
   u32 tmpRxBytes;
   u32 tmpTxBytes;
   u32 tmpRxPackets;
   u32 tmpTxPackets;
} __attribute__((packed)) shared_mem_radio_stats_stream;

// Vehicle radio interfaces order is given by physical order. Can't be changed.
// Vehicle radio links order is auto/user defined.
// Vehicle radio interfaces assignment to radio links is auto/user defined.
// Vehicle radio interfaces parameters are checked/re-computed when the vehicle starts.

typedef struct
{
   int assignedLocalRadioLinkId; // id of the radio link assigned to this radio interface
   int assignedVehicleRadioLinkId; // id of the radio link assigned to this radio interface
   u32 uCurrentFrequencyKhz;
   u8 openedForRead;
   u8 openedForWrite;
   int lastDbm;
   int lastDbmVideo;
   int lastDbmData;
   int lastRecvDataRate; // positive: bps, negative: MCS, 0 - never
   int lastRecvDataRateVideo; 
   int lastRecvDataRateData;
   int lastSentDataRateVideo; // positive: bps, negative: MCS, 0 - never
   int lastSentDataRateData;
   u32 lastReceivedRadioLinkPacketIndex; // for lost packets calculation

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
   u32 timeNow;

   u32 tmpRxBytes;
   u32 tmpTxBytes;
   u32 tmpRxPackets;
   u32 tmpTxPackets;

   int rxQuality; // 0...100%
   int rxRelativeQuality; // higher value means better link; it's relative to the other radio interfaces

   u8 uSlicesUpdated;
   u8 hist_rxPacketsCount[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxPacketsBadCount[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxPacketsLostCount[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxGapMiliseconds[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u32 hist_tmp_rxPacketsCount;
   u32 hist_tmp_rxPacketsBadCount;
   u32 hist_tmp_rxPacketsLostCount;

} __attribute__((packed)) shared_mem_radio_stats_radio_interface;

typedef struct
{
   int lastDbm;
   int lastDbmVideo;
   int lastDbmData;
   int lastRecvDataRate; // positive: bps, negative: MCS, 0 - never
   int lastRecvDataRateVideo;
   int lastRecvDataRateData;
   int lastSentDataRateVideo; // positive: bps, negative: MCS, 0 - never
   int lastSentDataRateData;

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
   u32 timeNow;
   
   int rxQuality; // 0...100%
   int rxRelativeQuality; // higher value means better link; it's relative to the other radio interfaces

   u8 hist_rxPacketsCount[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxPacketsLostCount[MAX_HISTORY_RADIO_STATS_RECV_SLICES];
   u8 hist_rxGapMiliseconds[MAX_HISTORY_RADIO_STATS_RECV_SLICES];

} __attribute__((packed)) shared_mem_radio_stats_radio_interface_compact;


typedef struct
{
   int matchingVehicleRadioLinkId;
   int refreshIntervalMs;
   u32 lastComputeTimePerSec;
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

   int lastTxInterfaceIndex;
   int lastSentDataRateVideo; // positive: bps, negative: MCS, 0 - never
   int lastSentDataRateData;

   u32 downlink_tx_time_per_sec;
   u32 tmp_downlink_tx_time_per_sec;

} __attribute__((packed)) shared_mem_radio_stats_radio_link;


typedef struct
{
   int countLocalRadioLinks;
   int countVehicleRadioLinks;
   int countLocalRadioInterfaces;
   int graphRefreshIntervalMs;
   int refreshIntervalMs;

   u32 lastComputeTime;
   u32 lastComputeTimeGraph;

   u32 all_downlinks_tx_time_per_sec;
   u32 tmp_all_downlinks_tx_time_per_sec;

   u32 uTimeLastReceivedAResponseFromVehicle;
   u32 timeLastRxPacket;
   
   int iMaxRxQuality;
   shared_mem_radio_stats_stream           radio_streams[MAX_CONCURENT_VEHICLES][MAX_RADIO_STREAMS];
   shared_mem_radio_stats_radio_interface  radio_interfaces[MAX_RADIO_INTERFACES];
   shared_mem_radio_stats_radio_link       radio_links[MAX_RADIO_INTERFACES];

} __attribute__((packed)) shared_mem_radio_stats;

