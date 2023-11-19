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

#include "shared_vars_ipc.h"

// These are received using router messages, not shared memory:

t_packet_header_vehicle_tx_history g_PHVehicleTxHistory;


// There are shared memory objects

shared_mem_process_stats* g_pProcessStatsRouter = NULL;
shared_mem_process_stats* g_pProcessStatsTelemetry = NULL;
shared_mem_process_stats* g_pProcessStatsRC = NULL;
shared_mem_process_stats g_ProcessStatsRouter;
shared_mem_process_stats g_ProcessStatsTelemetry;
shared_mem_process_stats g_ProcessStatsRC;

t_packet_header_rc_info_downstream* g_pSM_DownstreamInfoRC = NULL; // RC Info received on ground 
t_packet_header_rc_info_downstream g_SM_DownstreamInfoRC; // RC Info received on ground 

shared_mem_router_vehicles_runtime_info* g_pSM_RouterVehiclesRuntimeInfo = NULL;
shared_mem_router_vehicles_runtime_info g_SM_RouterVehiclesRuntimeInfo;

shared_mem_radio_stats* g_pSM_RadioStats = NULL;
shared_mem_radio_stats g_SM_RadioStats;

shared_mem_radio_stats_interfaces_rx_graph* g_pSM_RadioStatsInterfaceRxGraph = NULL;
shared_mem_radio_stats_interfaces_rx_graph g_SM_RadioStatsInterfaceRxGraph;

shared_mem_radio_stats_rx_hist* g_pSM_HistoryRxStats = NULL;
shared_mem_radio_stats_rx_hist g_SM_HistoryRxStats;
shared_mem_radio_stats_rx_hist g_SM_HistoryRxStatsVehicle;

shared_mem_audio_decode_stats* g_pSM_AudioDecodeStats = NULL;
shared_mem_audio_decode_stats g_SM_AudioDecodeStats;

shared_mem_video_info_stats* g_pSM_VideoInfoStats = NULL;
shared_mem_video_info_stats g_SM_VideoInfoStats;

shared_mem_video_info_stats* g_pSM_VideoInfoStatsRadioIn = NULL;
shared_mem_video_info_stats g_SM_VideoInfoStatsRadioIn;

shared_mem_video_info_stats g_VideoInfoStatsFromVehicle;
shared_mem_video_info_stats g_VideoInfoStatsFromVehicleRadioOut;

shared_mem_video_stream_stats_rx_processors* g_pSM_VideoDecodeStats = NULL;
shared_mem_video_stream_stats_rx_processors g_SM_VideoDecodeStats;

shared_mem_video_stream_stats_history_rx_processors* g_pSM_VDS_history = NULL;
shared_mem_video_stream_stats_history_rx_processors g_SM_VDS_history;

shared_mem_controller_retransmissions_stats_rx_processors* g_pSM_ControllerRetransmissionsStats = NULL;
shared_mem_controller_retransmissions_stats_rx_processors g_SM_ControllerRetransmissionsStats;

shared_mem_video_link_stats_and_overwrites* g_pSM_VideoLinkStats = NULL;
shared_mem_video_link_stats_and_overwrites g_SM_VideoLinkStats;

shared_mem_video_link_graphs* g_pSM_VideoLinkGraphs = NULL;
shared_mem_video_link_graphs g_SM_VideoLinkGraphs;

shared_mem_dev_video_bitrate_history g_SM_DevVideoBitrateHistory;


t_shared_mem_i2c_controller_rc_in* g_pSM_RCIn = NULL; // SBUS/IBUS RC In
t_shared_mem_i2c_controller_rc_in g_SM_RCIn;

t_shared_mem_i2c_current* g_pSMVoltage = NULL;
t_shared_mem_i2c_current g_SMVoltage;

t_shared_mem_i2c_rotary_encoder_buttons_events* g_pSMRotaryEncoderButtonsEvents = NULL;

shared_mem_router_packets_stats_history* g_pDebugSMRPST = NULL; 
