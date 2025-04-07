#pragma once
#include "base.h"
#include "config.h"

#define SHARED_MEM_CONTROLLER_ROUTER_VEHICLES_INFO "R_SHARED_MEM_CONTROLLER_ROUTER_VEHICLE_INFO"
#define SHARED_MEM_VIDEO_STREAM_STATS "/SYSTEM_SHARED_MEM_STATION_VIDEO_STREAM_STATS"
#define SHARED_MEM_RADIO_RX_QUEUE_INFO_STATS "/SYSTEM_SHARED_MEM_RADIO_RX_QUEUE_STATS"

#define MAX_HISTORY_VIDEO_INTERVALS 50
#define MAX_HISTORY_STACK_RETRANSMISSION_INFO 100
#define MAX_RETRANSMISSION_PACKETS_IN_REQUEST 20

#define MAX_RADIO_RX_QUEUE_INFO_VALUES 50

typedef struct
{
   u32 uVehicleId;
   u8 uVideoStreamIndex;
   t_packet_header_video_segment PHVS;
   u32 uCurrentVideoProfileEncodingFlags;
   int iCurrentVideoWidth;
   int iCurrentVideoHeight;
   int iCurrentVideoFPS;
   u32 uCurrentFECTimeMicros; // in micro seconds per second   
   int iCurrentPacketsInBuffers;
   int iMaxPacketsInBuffers;
   u8 uDetectedH264Profile;
   u8 uDetectedH264ProfileConstrains;
   u8 uDetectedH264Level;
} ALIGN_STRUCT_SPEC_INFO shared_mem_video_stream_stats;

typedef struct
{
   shared_mem_video_stream_stats video_streams[MAX_VIDEO_PROCESSORS];
} ALIGN_STRUCT_SPEC_INFO shared_mem_video_stream_stats_rx_processors;

typedef struct
{
   u32 uVehiclesIds[MAX_CONCURENT_VEHICLES];

   bool bIsVehicleFastUplinkFromControllerLost[MAX_CONCURENT_VEHICLES];
   bool bIsVehicleSlowUplinkFromControllerLost[MAX_CONCURENT_VEHICLES];
   u32 uLastTimeReceivedAckFromVehicle[MAX_CONCURENT_VEHICLES];
   int iVehicleClockDeltaMilisec[MAX_CONCURENT_VEHICLES];
   bool bIsDoingRetransmissions[MAX_CONCURENT_VEHICLES];
   bool bIsDoingAdaptive[MAX_CONCURENT_VEHICLES];
   u32 uAverageCommandRoundtripMiliseconds[MAX_CONCURENT_VEHICLES];
   u32 uMaxCommandRoundtripMiliseconds[MAX_CONCURENT_VEHICLES];
   u32 uMinCommandRoundtripMiliseconds[MAX_CONCURENT_VEHICLES];
} ALIGN_STRUCT_SPEC_INFO shared_mem_router_vehicles_runtime_info;


typedef struct
{
   u8 uPendingRxPackets[MAX_RADIO_RX_QUEUE_INFO_VALUES];
   u32 uMeasureIntervalMs;
   u32 uLastMeasureTime;
   u8 uCurrentIndex;
} ALIGN_STRUCT_SPEC_INFO shared_mem_radio_rx_queue_info;


shared_mem_video_stream_stats_rx_processors* shared_mem_video_stream_stats_rx_processors_open_for_read();
shared_mem_video_stream_stats_rx_processors* shared_mem_video_stream_stats_rx_processors_open_for_write();
void shared_mem_video_stream_stats_rx_processors_close(shared_mem_video_stream_stats_rx_processors* pAddress);
shared_mem_video_stream_stats* get_shared_mem_video_stream_stats_for_vehicle(shared_mem_video_stream_stats_rx_processors* pSM, u32 uVehicleId);

shared_mem_router_vehicles_runtime_info* shared_mem_router_vehicles_runtime_info_open_for_read();
shared_mem_router_vehicles_runtime_info* shared_mem_router_vehicles_runtime_info_open_for_write();
void shared_mem_router_vehicles_runtime_info_close(shared_mem_router_vehicles_runtime_info* pAddress);

shared_mem_radio_rx_queue_info* shared_mem_radio_rx_queue_info_open_for_read();
shared_mem_radio_rx_queue_info* shared_mem_radio_rx_queue_info_open_for_write();
void shared_mem_radio_rx_queue_info_close(shared_mem_radio_rx_queue_info* pAddress);
