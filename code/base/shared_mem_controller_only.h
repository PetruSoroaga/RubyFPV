#pragma once
#include "base.h"
#include "config.h"

#define SHARED_MEM_ROUTER_PACKETS_STATS_HISTORY "/SYSTEM_SHARED_MEM_STATION_ROUTER_PACKETS_STATS_HISTORY"
#define SHARED_MEM_CONTROLLER_ADAPTIVE_VIDEO_INFO "R_SHARED_MEM_CONTROLLER_ADAPTIVE_VIDEO_INFO"
#define SHARED_MEM_CONTROLLER_RADIO_INTERFACES_RX_GRAPHS "R_SHARED_MEM_CONTROLLER_RADIO_INTERFACES_RX_GRAPHS"
#define SHARED_MEM_AUDIO_DECODE_STATS "/SYSTEM_SHARED_MEM_AUDIO_DECODE_STATS"
#define SHARED_MEM_VIDEO_STREAM_STATS "/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_STATS"
#define SHARED_MEM_VIDEO_STREAM_STATS_HISTORY "/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_STATS_HISTORY"
#define SHARED_MEM_VIDEO_RETRANSMISSIONS_STATS "/SYSTEM_SHARED_MEM_STATION_VIDEO_RETRANMISSIONS_STATS"


#define MAX_HISTORY_VIDEO_INTERVALS 50
#define MAX_HISTORY_STACK_RETRANSMISSION_INFO 100
#define MAX_RETRANSMISSION_PACKETS_IN_REQUEST 30


#ifdef __cplusplus
extern "C" {
#endif 

typedef struct
{  
   u16 rxHistoryPackets[1000]; // one for each milisecond in a second
   // 3 bits: bit 0..2 - video packets count
   // 3 bits: bit 3..5 - retransmitted packets
   // 1 bit:  bit 6..6 - ping reply
   // 2 bits: bit 7..8 - telemetry packets count
   // 2 bits: bit 9..10 - rc packets count
   // 2 bits: bit 11..12 - ruby/other packets count
   // 3 bits: bit 13..15 - datarate // 1, 2, 6, 9, 11, 12, 18, 24

   u16 txHistoryPackets[1000];
   // 3 bits: bit 0..2 - retransmissions
   // 3 bits: bit 3..5 - commands
   // 2 bits: bit 6..7 - ping
   // 3 bits: bit 8..10 - rc
   // 2 bits: bit 11..12 - ruby/other packets count
   // 3 bits: bit 13..15 - datarate // 1, 2, 6, 9, 11, 12, 18, 24

   u32 lastMilisecond;
} __attribute__((packed)) shared_mem_router_packets_stats_history;


#define MAX_AUDIO_HISTORY_BUFFERS 32

typedef struct
{
    u32 uLastReceivedAudioPacketIndex; // byte 0-2: block index, byte 3: packet index
    u8 uCurrentSchemeDataPackets;
    u8 uCurrentSchemeECPackets;
    u16 uCurrentSchemePacketLength;
    int uCurrentReceivedAudioBps;

    u32 uGraphUpdateIntervalMs;
    u8 uHistoryReceivedDataPackets[MAX_AUDIO_HISTORY_BUFFERS];
    u8 uHistoryReceivedECPackets[MAX_HISTORY_VIDEO_INTERVALS];
    u8 uHistoryReceivedIncompleteBlocks[MAX_HISTORY_VIDEO_INTERVALS];
    u8 uHistoryRepairedBlocks[MAX_HISTORY_VIDEO_INTERVALS];
    u8 uHistoryBadBlocks[MAX_HISTORY_VIDEO_INTERVALS];
} __attribute__((packed)) shared_mem_audio_decode_stats;

typedef struct
{
   u32 uVehicleId;
   u8 uVideoStreamIndex;
   int isRecording;
   int iCurrentRxTxSyncType;
   u32 frames_type;
   u32 video_stream_and_type; // bits 0...3: video stream index, bits 4...7: video stream type: h264, IP, etc
   int video_link_profile; // 0xF0 - user selected one, 0x0F - current active one
   u32 encoding_extra_flags; // same as video encoding flags from model and video radio packet
   u32 uEncodingExtraFlags2;
   int width;
   int height;
   int fps;
   int keyframe_ms;
   u32 fec_time; // in micro seconds per second
   int data_packets_per_block;
   int fec_packets_per_block;
   int video_data_length;
   u8 totalEncodingSwitches;
   int iLastAckKeyframeInterval;
   int iLastAckVideoLevelShift;
   u32 uLastSetVideoBitrate;
   u32 total_DiscardedLostPackets;
   u32 total_DiscardedBuffers;
   u32 total_DiscardedSegments;

   int currentPacketsInBuffers;
   int maxPacketsInBuffers;
   int maxBlocksAllowedInBuffers;

} __attribute__((packed)) shared_mem_video_stream_stats;

typedef struct
{
   shared_mem_video_stream_stats video_streams[MAX_VIDEO_PROCESSORS];
} __attribute__((packed)) shared_mem_video_stream_stats_rx_processors;

typedef struct
{
   u32 uVehicleId;
   u8 uVideoStreamIndex;
   u8 outputHistoryIntervalMs;
   u8 outputHistoryReceivedVideoPackets[MAX_HISTORY_VIDEO_INTERVALS];
   u8 outputHistoryReceivedVideoRetransmittedPackets[MAX_HISTORY_VIDEO_INTERVALS];
   
   u8 outputHistoryMaxGoodBlocksPendingPerPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u8 outputHistoryBlocksOkPerPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u8 outputHistoryBlocksReconstructedPerPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u8 outputHistoryMaxECPacketsUsedPerPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u8 outputHistoryBlocksBadPerPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u8 outputHistoryBlocksMissingPerPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u8 outputHistoryBlocksRetrasmitedPerPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u8 outputHistoryBlocksMaxPacketsGapPerPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u8 outputHistoryPacketsRetrasmitedPerPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u8 outputHistoryBlocksDiscardedPerPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u16 missingTotalPacketsAtPeriod[MAX_HISTORY_VIDEO_INTERVALS];
   u16 totalCurrentlyMissingPackets;
} __attribute__((packed)) shared_mem_video_stream_stats_history;

typedef struct
{
   shared_mem_video_stream_stats_history video_streams[MAX_VIDEO_PROCESSORS];
} __attribute__((packed)) shared_mem_video_stream_stats_history_rx_processors;

typedef struct 
{
   u8 uCountRequestedRetransmissions;
   u8 uCountAcknowledgedRetransmissions; // retransmissions for which we received at least a response packet
   u8 uCountCompletedRetransmissions; // retransmissions for which we received all packets.
   u8 uCountDroppedRetransmissions;

   u8 uMinRetransmissionRoundtripTime;
   u8 uMaxRetransmissionRoundtripTime;
   u16 uAverageRetransmissionRoundtripTime;

   u16 uCountRequestedPacketsForRetransmission;
   u16 uCountReRequestedPacketsForRetransmission;
   u16 uCountReceivedRetransmissionPackets;
   u16 uCountReceivedRetransmissionPacketsDuplicate;
   u16 uCountReceivedRetransmissionPacketsDropped;
} __attribute__((packed)) controller_retransmissions_stats_element;

typedef struct 
{
   u32 uRetransmissionId;
   u32 uRequestTime;
   u8  uRequestedPackets;
   u8  uReceivedPackets;

   u32 uRequestedVideoBlockIndex[MAX_RETRANSMISSION_PACKETS_IN_REQUEST];
   u8  uRequestedVideoBlockPacketIndex[MAX_RETRANSMISSION_PACKETS_IN_REQUEST];
   u8  uRequestedVideoPacketRetryCount[MAX_RETRANSMISSION_PACKETS_IN_REQUEST];
   u32 uReceivedPacketTime[MAX_RETRANSMISSION_PACKETS_IN_REQUEST];
   u8  uReceivedPacketCount[MAX_RETRANSMISSION_PACKETS_IN_REQUEST];
   
   u8  uMinResponseTime;
   u8  uMaxResponseTime;
} __attribute__((packed)) controller_single_retransmission_state;

typedef struct
{  
   u32 uVehicleId;
   u8 uVideoStreamIndex;
   u16 totalRequestedRetransmissions;
   u16 totalReceivedRetransmissions;
   u16 totalRequestedRetransmissionsLast500Ms;
   u16 totalReceivedRetransmissionsLast500Ms;
   u16 totalRequestedVideoPackets;
   u16 totalReceivedVideoPackets;
   controller_retransmissions_stats_element history[MAX_HISTORY_VIDEO_INTERVALS];

   u32 uGraphRefreshIntervalMs;

   controller_single_retransmission_state listActiveRetransmissions[MAX_HISTORY_STACK_RETRANSMISSION_INFO];
   int iCountActiveRetransmissions;

} __attribute__((packed)) shared_mem_controller_retransmissions_stats;

typedef struct
{
   shared_mem_controller_retransmissions_stats video_streams[MAX_VIDEO_PROCESSORS];
} __attribute__((packed)) shared_mem_controller_retransmissions_stats_rx_processors;


#define MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS 70 // sampled every 40 ms
#define CONTROLLER_ADAPTIVE_VIDEO_SAMPLE_INTERVAL 40
typedef struct
{
   u32 uLastUpdateTime;
   u32 uUpdateInterval;
   u32 uCurrentIntervalIndex;
   int iChangeStrength;

   u16 uIntervalsOuputCleanVideoPackets[MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS];
   u16 uIntervalsOuputRecontructedVideoPackets[MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS];
   u16 uIntervalsMissingVideoPackets[MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS];
   u16 uIntervalsRequestedRetransmissions[MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS];
   u16 uIntervalsRetriedRetransmissions[MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS];

   u16 uCurrentKFMeasuredThUp1;
   u16 uCurrentKFMeasuredThUp2;
   u16 uCurrentKFMeasuredThDown1;
   u16 uCurrentKFMeasuredThDown2;
   u16 uCurrentKFMeasuredThDown3;
   u32 uIntervalsKeyFrameMs1;
   u32 uIntervalsKeyFrameMs2;
   u32 uIntervalsKeyFrameMs3;

   u32 uIntervalsAdaptive1;
   u32 uIntervalsAdaptive2;
   u32 uIntervalsAdaptive3;

   int iCurrentTargetLevelShift;
   int iLastRequestedLevelShiftRetryCount;
   int iLastAcknowledgedLevelShift;
   u32 uTimeLastRequestedLevelShift;
   u32 uTimeLastAckLevelShift;
   u32 uTimeLastLevelShiftCheckConsistency;
   u32 uTimeLastLevelShiftDown;
   u32 uTimeLastLevelShiftUp;

   int iLastRequestedKeyFrameMs;
   int iLastRequestedKeyFrameMsRetryCount;
   int iLastAcknowledgedKeyFrameMs;
   u32 uTimeLastRequestedKeyFrame;
   u32 uTimeLastKFShiftDown;
   u32 uTimeLastKFShiftUp;

   u32 uLastSetVideoBitrate; // in bps, highest bit: 1 - initial set, 0 - auto adjusted
} __attribute__((packed)) shared_mem_controller_adaptive_video_info_vehicle;

typedef struct
{
   u32 uVehiclesIds[MAX_CONCURENT_VEHICLES];
   u32 uPairingRequestTime[MAX_CONCURENT_VEHICLES];
   u32 uPairingRequestInterval[MAX_CONCURENT_VEHICLES];
   u32 uPairingRequestId[MAX_CONCURENT_VEHICLES];
   int iPairingDone[MAX_CONCURENT_VEHICLES];

   u32 uRoundtripTimeVehiclesOnLocalRadioLinks[MAX_CONCURENT_VEHICLES][MAX_RADIO_INTERFACES];
   u32 uTimeLastPingSentToVehiclesOnLocalRadioLinks[MAX_CONCURENT_VEHICLES][MAX_RADIO_INTERFACES][3];
   u32 uTimeLastPingReplyReceivedFromVehiclesOnLocalRadioLinks[MAX_CONCURENT_VEHICLES][MAX_RADIO_INTERFACES];
   u8  uLastPingIdSentToVehiclesOnLocalRadioLinks[MAX_CONCURENT_VEHICLES][MAX_RADIO_INTERFACES][3];
   u8  uLastPingIdReceivedFromVehiclesOnLocalRadioLinks[MAX_CONCURENT_VEHICLES][MAX_RADIO_INTERFACES];
   shared_mem_controller_adaptive_video_info_vehicle vehicles_adaptive_video[MAX_CONCURENT_VEHICLES];
} __attribute__((packed)) shared_mem_router_vehicles_runtime_info;

shared_mem_router_packets_stats_history* shared_mem_router_packets_stats_history_open_read();
shared_mem_router_packets_stats_history* shared_mem_router_packets_stats_history_open_write();
void shared_mem_router_packets_stats_history_close(shared_mem_router_packets_stats_history* pAddress);

void add_detailed_history_rx_packets(shared_mem_router_packets_stats_history* pSMRPSH, int timeNowMs, int countVideo, int countRetransmited, int countTelemetry, int countRC, int countPing, int countOther, int dataRateBPSMCS);
void add_detailed_history_tx_packets(shared_mem_router_packets_stats_history* pSMRPSH, int timeNowMs, int countRetransmited, int countComands, int countPing, int countRC, int countOther, int dataRate);

shared_mem_radio_stats_interfaces_rx_graph* shared_mem_controller_radio_stats_interfaces_rx_graphs_open_for_read();
shared_mem_radio_stats_interfaces_rx_graph* shared_mem_controller_radio_stats_interfaces_rx_graphs_open_for_write();
void shared_mem_controller_radio_stats_interfaces_rx_graphs_close(shared_mem_radio_stats_interfaces_rx_graph* pAddress);

shared_mem_video_stream_stats_rx_processors* shared_mem_video_stream_stats_rx_processors_open(int readOnly);
void shared_mem_video_stream_stats_rx_processors_close(shared_mem_video_stream_stats_rx_processors* pAddress);

shared_mem_video_stream_stats_history_rx_processors* shared_mem_video_stream_stats_history_rx_processors_open(int readOnly);
void shared_mem_video_stream_stats_history_rx_processors_close(shared_mem_video_stream_stats_history_rx_processors* pAddress);

shared_mem_audio_decode_stats* shared_mem_controller_audio_decode_stats_open_for_read();
shared_mem_audio_decode_stats* shared_mem_controller_audio_decode_stats_open_for_write();
void shared_mem_controller_audio_decode_stats_close(shared_mem_audio_decode_stats* pAddress);

shared_mem_router_vehicles_runtime_info* shared_mem_router_vehicles_runtime_info_open_for_read();
shared_mem_router_vehicles_runtime_info* shared_mem_router_vehicles_runtime_info_open_for_write();
void shared_mem_router_vehicles_runtime_info_close(shared_mem_router_vehicles_runtime_info* pAddress);

shared_mem_controller_retransmissions_stats_rx_processors* shared_mem_controller_video_retransmissions_stats_open_for_read();
shared_mem_controller_retransmissions_stats_rx_processors* shared_mem_controller_video_retransmissions_stats_open_for_write();
void shared_mem_controller_video_retransmissions_stats_close(shared_mem_controller_retransmissions_stats_rx_processors* pAddress);

#ifdef __cplusplus
}  
#endif 