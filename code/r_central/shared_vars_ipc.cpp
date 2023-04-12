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

shared_mem_process_stats* g_pProcessStatsRouter = NULL;
shared_mem_process_stats* g_pProcessStatsTelemetry = NULL;
shared_mem_process_stats* g_pProcessStatsRC = NULL;

t_packet_header_rc_info_downstream* g_pPHDownstreamInfoRC = NULL; // RC Info received on ground 
t_packet_header_vehicle_tx_history g_PHVehicleTxHistory;
shared_mem_radio_stats_radio_interface g_SM_VehicleRxStats[MAX_RADIO_INTERFACES];

shared_mem_controller_vehicles_adaptive_video_info* g_pSM_ControllerVehiclesAdaptiveVideoInfo = NULL;
shared_mem_radio_stats* g_pSM_RadioStats = NULL;
shared_mem_video_info_stats* g_pSM_VideoInfoStats = NULL;
shared_mem_video_info_stats* g_pSM_VideoInfoStatsRadioIn = NULL;
shared_mem_video_info_stats g_VideoInfoStatsFromVehicle;
shared_mem_video_info_stats g_VideoInfoStatsFromVehicleRadioOut;
shared_mem_video_decode_stats* g_psmvds = NULL;
shared_mem_video_decode_stats_history* g_psmvds_history = NULL;
shared_mem_controller_retransmissions_stats* g_pSM_ControllerRetransmissionsStats = NULL;
shared_mem_video_link_stats_and_overwrites* g_pSM_VideoLinkStats = NULL;
shared_mem_video_link_graphs* g_pSM_VideoLinkGraphs = NULL;
shared_mem_dev_video_bitrate_history g_SM_DevVideoBitrateHistory;


t_shared_mem_i2c_controller_rc_in* g_pSM_RCIn = NULL; // SBUS/IBUS RC In
t_shared_mem_i2c_current* g_pSMVoltage = NULL;
t_shared_mem_i2c_rotary_encoder_buttons_events* g_pSMRotaryEncoderButtonsEvents = NULL;

shared_mem_router_packets_stats_history* g_pDebugSMRPST = NULL; 
