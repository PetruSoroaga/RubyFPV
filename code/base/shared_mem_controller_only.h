#pragma once
#include "base.h"
#include "config.h"

#define SHARED_MEM_CONTROLLER_ADAPTIVE_VIDEO_INFO "R_SHARED_MEM_CONTROLLER_ADAPTIVE_VIDEO_INFO"


#define MAX_HISTORY_VIDEO_INTERVALS 50
#define MAX_HISTORY_STACK_RETRANSMISSION_INFO 100
#define MAX_RETRANSMISSION_PACKETS_IN_REQUEST 30


#ifdef __cplusplus
extern "C" {
#endif 

typedef struct
{  
   int isRecording;
   int isRetransmissionsOn;
   int iCurrentClockSyncType;
   u32 frames_type;
   u32 video_type;
   int video_link_profile; // 0xF0 - user selected one, 0x0F - current active one
   u32 encoding_extra_flags; // same as video encoding flags from model and video radio packet
   int width;
   int height;
   int fps;
   int keyframe;
   u32 fec_time; // in micro seconds per second
   int data_packets_per_block;
   int fec_packets_per_block;
   int video_packet_length;
   u8 totalEncodingSwitches;

   u32 total_DiscardedLostPackets;
   u32 total_DiscardedBuffers;
   u32 total_DiscardedSegments;

   int currentPacketsInBuffers;
   int maxPacketsInBuffers;
   int maxBlocksAllowedInBuffers;

} __attribute__((packed)) shared_mem_video_decode_stats;

typedef struct
{  
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
   u8 outputHistoryIntervalMs;
   u16 totalCurrentlyMissingPackets;
} __attribute__((packed)) shared_mem_video_decode_stats_history;


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
   u8  uRequestedSegments;
   u8  uReceivedSegments;

   u32 uRequestedVideoBlockIndex[MAX_RETRANSMISSION_PACKETS_IN_REQUEST];
   u8  uRequestedVideoBlockPacketIndex[MAX_RETRANSMISSION_PACKETS_IN_REQUEST];
   u8  uRequestedVideoPacketRetryCount[MAX_RETRANSMISSION_PACKETS_IN_REQUEST];
   u32 uReceivedSegmentTime[MAX_RETRANSMISSION_PACKETS_IN_REQUEST];
   u8  uReceivedSegmentCount[MAX_RETRANSMISSION_PACKETS_IN_REQUEST];
   
   u8  uMinResponseTime;
   u8  uMaxResponseTime;
} __attribute__((packed)) controller_retransmission_state;

typedef struct
{  
   u16 totalRequestedRetransmissions;
   u16 totalReceivedRetransmissions;
   u16 totalRequestedRetransmissionsLast500Ms;
   u16 totalReceivedRetransmissionsLast500Ms;
   u16 totalRequestedSegments;
   u16 totalReceivedSegments;
   controller_retransmissions_stats_element history[MAX_HISTORY_VIDEO_INTERVALS];

   u32 refreshIntervalMs;

   controller_retransmission_state listRetransmissions[MAX_HISTORY_STACK_RETRANSMISSION_INFO];
   int iListRetransmissionsCount;

} __attribute__((packed)) shared_mem_controller_retransmissions_stats;


#define MAX_FRAMES_SAMPLES 60

typedef struct
{
   u32 uTimeLastUpdate;
   u8 uFramesTimes[MAX_FRAMES_SAMPLES];
   u8 uFramesTypesAndSizes[MAX_FRAMES_SAMPLES]; // Frame type and size in kbytes (frame type: highest bit: 0 regular, 1 keframe; lower 7 bits: frame size in kbytes)
   u32 uLastIndex;
   u32 uAverageFPS;
   u32 uAverageFrameTime;
   u32 uAverageFrameSize; // in bits
   u32 uMaxFrameDeltaTime;
   u16 uKeyframeInterval;
   u32 uExtraValue1;
   u32 uExtraValue2;
   
   u32 uTmpCurrentFrameSize;
   u32 uTmpKeframeDeltaFrames;
} __attribute__((packed)) shared_mem_video_info_stats;


#define MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS 200 // sampled every 20 ms
typedef struct
{
   u32 uVehicleId;
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
   u32 uIntervalsKeyFrame1;
   u32 uIntervalsKeyFrame2;
   u32 uIntervalsKeyFrame3;

   u32 uIntervalsAdaptive1;
   u32 uIntervalsAdaptive2;
   u32 uIntervalsAdaptive3;

   int iLastRequestedLevelShift;
   int iLastRequestedLevelShiftRetryCount;
   int iLastAcknowledgedLevelShift;
   u32 uTimeLastRequestedLevelShift;
   u32 uTimeLastLevelShiftDown;
   u32 uTimeLastLevelShiftUp;

   int iLastRequestedKeyFrame;
   int iLastRequestedKeyFrameRetryCount;
   int iLastAcknowledgedKeyFrame;
   u32 uTimeLastRequestedKeyFrame;
   u32 uTimeLastKFShiftDown;
   u32 uTimeLastKFShiftUp;
} __attribute__((packed)) shared_mem_controller_adaptive_video_info_vehicle;

typedef struct
{
   int iCountVehicles;
   shared_mem_controller_adaptive_video_info_vehicle vehicles[MAX_CONCURENT_VEHICLES];
} __attribute__((packed)) shared_mem_controller_vehicles_adaptive_video_info;


shared_mem_controller_vehicles_adaptive_video_info* shared_mem_controller_vehicles_adaptive_video_info_open_for_read();
shared_mem_controller_vehicles_adaptive_video_info* shared_mem_controller_vehicles_adaptive_video_info_open_for_write();
void shared_mem_controller_vehicles_adaptive_video_info_close(shared_mem_controller_vehicles_adaptive_video_info* pAddress);

#ifdef __cplusplus
}  
#endif 