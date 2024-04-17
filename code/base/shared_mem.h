#pragma once

#include <stdint.h>
#include <stdlib.h>
#include "base.h"
#include "config.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopackets_rc.h"

#include "shared_mem_radio.h"

#define SHARED_MEM_RADIO_STATS "/SYSTEM_SHARED_MEM_RUBY_RADIO_STATS"
#define SHARED_MEM_RADIO_STATS_RX_HIST "/SYSTEM_SHARED_MEM_RUBY_RADIO_STATS_RX_HIST"

#define SHARED_MEM_VIDEO_STREAM_INFO_STATS "/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_INFO"
#define SHARED_MEM_VIDEO_STREAM_INFO_STATS_RADIO_IN "/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_INFO_RADIO_IN"
#define SHARED_MEM_VIDEO_STREAM_INFO_STATS_RADIO_OUT "/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_INFO_RADIO_OUT"
#define SHARED_MEM_VIDEO_LINK_STATS "/SYSTEM_SHARED_MEM_STATION_VIDEO_LINK_STATS"
#define SHARED_MEM_VIDEO_LINK_GRAPHS "/SYSTEM_SHARED_MEM_STATION_VIDEO_LINK_GRAPHS"
#define SHARED_MEM_RC_DOWNLOAD_INFO "R_SHARED_MEM_VEHICLE_RC_DOWNLOAD_INFO"
#define SHARED_MEM_RC_UPSTREAM_FRAME "R_SHARED_MEM_RC_UPSTREAM_FRAME"

#define SHARED_MEM_WATCHDOG_CENTRAL "/SYSTEM_SHARED_MEM_WATCHDOG_CENTRAL"
#define SHARED_MEM_WATCHDOG_ROUTER_RX "/SYSTEM_SHARED_MEM_WATCHDOG_ROUTER_RX"
#define SHARED_MEM_WATCHDOG_TELEMETRY_RX "/SYSTEM_SHARED_MEM_WATCHDOG_TELEMETRY_RX"
#define SHARED_MEM_WATCHDOG_RC_TX "/SYSTEM_SHARED_MEM_WATCHDOG_RC_TX"

#define SHARED_MEM_WATCHDOG_ROUTER_TX "/SYSTEM_SHARED_MEM_WATCHDOG_ROUTER_TX"
#define SHARED_MEM_WATCHDOG_TELEMETRY_TX "/SYSTEM_SHARED_MEM_WATCHDOG_TELEMETRY_TX"
#define SHARED_MEM_WATCHDOG_COMMANDS_RX "/SYSTEM_SHARED_MEM_WATCHDOG_COMMANDS_RX"
#define SHARED_MEM_WATCHDOG_RC_RX "/SYSTEM_SHARED_MEM_WATCHDOG_RC_RX"

#define SHARED_MEM_RASPIVIDEO_COMMAND "/SYSTEM_SHARED_MEM_RASPIVID_COMM"
#define SIZE_OF_SHARED_MEM_RASPIVID_COMM 32
// it's a 32 byte shared mem (8 u32) gives the commands to raspivid
// first u32 and last u32: modify counter, for checksum purpose
// u32 1...6 - commands: byte 0: command number, byte 1: command type, byte 2,3: params


#ifdef __cplusplus
extern "C" {
#endif 

#define PROCESS_ALARM_NONE 0
#define PROCESS_ALARM_RADIO_INTERFACE_BEHIND 1
#define PROCESS_ALARM_RADIO_STREAM_RESTARTED 2



typedef struct
{
   u32 lastActiveTime;
   u32 lastRadioRxTime;
   u32 lastRadioTxTime;
   u32 lastIPCIncomingTime;
   u32 lastIPCOutgoingTime;
   u32 timeLastReceivedPacket;
   u32 alarmFlags;
   u32 alarmTime;
   u32 alarmParam[4];
   u32 uLoopCounter;
   u32 uTotalLoopTime;
   u32 uAverageLoopTimeMs;
   u32 uMaxLoopTimeMs;
} __attribute__((packed)) shared_mem_process_stats;


#define MAX_INTERVALS_VIDEO_LINK_SWITCHES 50
#define MAX_INTERVALS_VIDEO_LINK_STATS 24
#define VIDEO_LINK_STATS_REFRESH_INTERVAL_MS 80
// one every 80 milisec, for 2 sec total
// !!! Interval should be the same as the one send by controller in link stats: CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL


typedef struct
{
   u8 userVideoLinkProfile;
   u8 currentVideoLinkProfile;
   u32 currentProfileMaxVideoBitrate;
   u32 currentProfileAndLevelDefaultBitrate;
   u32 currentSetVideoBitrate;
   u8  hasEverSwitchedToLQMode;
   u8  currentDataBlocks;
   u8  currentECBlocks;
   u8  currentProfileShiftLevel;
   u8  currentH264QUantization;

   u16 uCurrentActiveKeyframeMs;
   u16 uCurrentPendingKeyframeMs;
   u16 uCurrentLocalRequestedKeyframeMs;
   u16 uCurrentControllerRequestedKeyframeMs;
   u32 uLastTimeLocalRequestedAKeyframe;
   u32 uLastTimeControllerRequestedAKeyframe;

   u32 profilesTopVideoBitrateOverwritesDownward[MAX_VIDEO_LINK_PROFILES]; // How much to decrease the target top bitrate for each profile

} __attribute__((packed)) shared_mem_video_link_overwrites;


typedef struct
{
   shared_mem_video_link_overwrites overwrites;

   u8  historySwitches[MAX_INTERVALS_VIDEO_LINK_SWITCHES]; // bit 0..3 - level, bit 4..7 - profile
   u32 totalSwitches;
   u32 timeLastAdaptiveParamsChangeDown;
   u32 timeLastAdaptiveParamsChangeUp;
   u32 timeLastProfileChangeDown;
   u32 timeLastProfileChangeUp;

   u32 timeLastStatsCheck;
   u32 timeLastReceivedControllerLinkInfo;

   u8 usedControllerInfo;
   u8 backIntervalsToLookForDownStep;
   u8 backIntervalsToLookForUpStep;
   int computed_min_interval_for_change_down;
   int computed_min_interval_for_change_up;
   u8 computed_rx_quality_vehicle;
   u8 computed_rx_quality_controller;
} __attribute__((packed)) shared_mem_video_link_stats_and_overwrites;


typedef struct
{
   u32 timeLastStatsUpdate;

   u8 tmp_vehileReceivedRetransmissionsRequestsCount;
   u8 tmp_vehicleReceivedRetransmissionsRequestsPackets;
   u8 tmp_vehicleReceivedRetransmissionsRequestsPacketsRetried;
   u8 vehicleRXQuality[MAX_INTERVALS_VIDEO_LINK_STATS];
   u8 vehicleRXMaxTimeGap[MAX_INTERVALS_VIDEO_LINK_STATS];
   u8 vehileReceivedRetransmissionsRequestsCount[MAX_INTERVALS_VIDEO_LINK_STATS];
   u8 vehicleReceivedRetransmissionsRequestsPackets[MAX_INTERVALS_VIDEO_LINK_STATS];
   u8 vehicleReceivedRetransmissionsRequestsPacketsRetried[MAX_INTERVALS_VIDEO_LINK_STATS];
   
   u8 controller_received_radio_interfaces_rx_quality[MAX_RADIO_INTERFACES][MAX_INTERVALS_VIDEO_LINK_STATS]; // 0...100 %, or 255 for no packets received or lost for this time slice
   u8 controller_received_radio_streams_rx_quality[MAX_VIDEO_STREAMS][MAX_INTERVALS_VIDEO_LINK_STATS]; // 0...100 %, or 255 for no packets received or lost for this time slice
   u8 controller_received_video_streams_blocks_clean[MAX_VIDEO_STREAMS][MAX_INTERVALS_VIDEO_LINK_STATS];
   u8 controller_received_video_streams_blocks_reconstructed[MAX_VIDEO_STREAMS][MAX_INTERVALS_VIDEO_LINK_STATS];
   u8 controller_received_video_streams_blocks_max_ec_packets_used[MAX_VIDEO_STREAMS][MAX_INTERVALS_VIDEO_LINK_STATS];
   u8 controller_received_video_streams_requested_retransmission_packets[MAX_VIDEO_STREAMS][MAX_INTERVALS_VIDEO_LINK_STATS];

} __attribute__((packed)) shared_mem_video_link_graphs;


#define MAX_INTERVALS_VIDEO_BITRATE_HISTORY 30

typedef struct
{
   u8  uMaxVideoDataRateMbps; // in Mbps (mcs is converted to Mbps)
   u8  uVideoQuantization; // H264 quantization
   u16 uVideoBitrateCurrentProfileKb; // set video bitrate for current video profile
   u16 uVideoBitrateTargetKb; // target video bitrate for current video profile and overwrites
   u16 uVideoBitrateKb; // only video data
   u16 uVideoBitrateAvgKb; // only video data
   u16 uTotalVideoBitrateKb; // video data + EC + radio headers
   u16 uTotalVideoBitrateAvgKb; // video data + EC + radio headers
   u8  uVideoProfileSwitches; // bit 0..3 - level, bit 4..7 - profile 
} __attribute__((packed)) shared_mem_dev_video_bitrate_history_datapoint;

typedef struct
{
   u32 uGraphSliceInterval;
   u32 uLastGraphSliceTime;
   u8  uTotalDataPoints;
   u8  uCurrentDataPoint;
   u8  uQuantizationOverflowValue;
   u32 uCurrentTargetVideoBitrate;

   shared_mem_dev_video_bitrate_history_datapoint  history[MAX_INTERVALS_VIDEO_BITRATE_HISTORY];
} __attribute__((packed)) shared_mem_dev_video_bitrate_history;


#define MAX_FRAMES_SAMPLES 60

typedef struct
{
   u32 uTimeLastUpdate; // Last time statistics where updated
   u8 uFramesDuration[MAX_FRAMES_SAMPLES]; // highest bit: 1 if keyframe interval was adjusted. lower 7 bits: frame duration in ms
   u8 uFramesTypesAndSizes[MAX_FRAMES_SAMPLES]; // Frame type and size in kbytes (frame type: highest bit: 0 regular, 1 keframe; lower 7 bits: frame size in kbytes)
   u32 uLastIndex; // Increases on each video frame detected

   // Computed on H264 parser
   u32 uDetectedFPS;
   u32 uDetectedSlices;
   
   // Computed on OSD side
   u32 uAverageFPS;
   u32 uAverageFrameTime;
   u32 uAverageFrameSize; // in bits
   u32 uAveragePFrameSize; // in bits
   u32 uMaxFrameDeltaTime;
   u16 uKeyframeIntervalMs;
   u32 uExtraValue1;
   u32 uExtraValue2;
   
} __attribute__((packed)) shared_mem_video_info_stats;


void* open_shared_mem(const char* name, int size, int readOnly);
void* open_shared_mem_for_write(const char* name, int size);
void* open_shared_mem_for_read(const char* name, int size);

shared_mem_process_stats* shared_mem_process_stats_open_read(const char* szName);
shared_mem_process_stats* shared_mem_process_stats_open_write(const char* szName);
void shared_mem_process_stats_close(const char* szName, shared_mem_process_stats* pAddress);

void process_stats_reset(shared_mem_process_stats* pStats, u32 timeNow);
void process_stats_mark_active(shared_mem_process_stats* pStats, u32 timeNow);

shared_mem_radio_stats* shared_mem_radio_stats_open_for_read();
shared_mem_radio_stats* shared_mem_radio_stats_open_for_write();
void shared_mem_radio_stats_close(shared_mem_radio_stats* pAddress);

shared_mem_radio_stats_rx_hist* shared_mem_radio_stats_rx_hist_open_for_read();
shared_mem_radio_stats_rx_hist* shared_mem_radio_stats_rx_hist_open_for_write();
void shared_mem_radio_stats_rx_hist_close(shared_mem_radio_stats_rx_hist* pAddress);

shared_mem_video_info_stats* shared_mem_video_info_stats_open_for_read();
shared_mem_video_info_stats* shared_mem_video_info_stats_open_for_write();
void shared_mem_video_info_stats_close(shared_mem_video_info_stats* pAddress);

shared_mem_video_info_stats* shared_mem_video_info_stats_radio_in_open_for_read();
shared_mem_video_info_stats* shared_mem_video_info_stats_radio_in_open_for_write();
void shared_mem_video_info_stats_radio_in_close(shared_mem_video_info_stats* pAddress);

shared_mem_video_info_stats* shared_mem_video_info_stats_radio_out_open_for_read();
shared_mem_video_info_stats* shared_mem_video_info_stats_radio_out_open_for_write();
void shared_mem_video_info_stats_radio_out_close(shared_mem_video_info_stats* pAddress);


shared_mem_video_link_stats_and_overwrites* shared_mem_video_link_stats_open_for_read();
shared_mem_video_link_stats_and_overwrites* shared_mem_video_link_stats_open_for_write();
void shared_mem_video_link_stats_close(shared_mem_video_link_stats_and_overwrites* pAddress);

shared_mem_video_link_graphs* shared_mem_video_link_graphs_open_for_read();
shared_mem_video_link_graphs* shared_mem_video_link_graphs_open_for_write();
void shared_mem_video_link_graphs_close(shared_mem_video_link_graphs* pAddress);

t_packet_header_rc_info_downstream* shared_mem_rc_downstream_info_open_read();
t_packet_header_rc_info_downstream* shared_mem_rc_downstream_info_open_write();
void shared_mem_rc_downstream_info_close(t_packet_header_rc_info_downstream* pRCInfo);

t_packet_header_rc_full_frame_upstream* shared_mem_rc_upstream_frame_open_read();
t_packet_header_rc_full_frame_upstream* shared_mem_rc_upstream_frame_open_write();
void shared_mem_rc_upstream_frame_close(t_packet_header_rc_full_frame_upstream* pRCFrame);

void update_shared_mem_video_info_stats(shared_mem_video_info_stats* pSMVIStats, u32 uTimeNow);

#ifdef __cplusplus
}  
#endif 