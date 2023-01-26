#pragma once
#include "../base/base.h"
#include "../base/shared_mem.h"
#include "../base/shared_mem_i2c.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopackets_rc.h"

extern shared_mem_process_stats* g_pProcessStatsRouter;
extern shared_mem_process_stats* g_pProcessStatsTelemetry;
extern shared_mem_process_stats* g_pProcessStatsRC;

extern t_packet_header_ruby_telemetry_extended* g_pPHRTE;
extern t_packet_header_ruby_telemetry_extended_extra_info_retransmissions* g_pPHRTE_Retransmissions;
extern t_packet_header_fc_telemetry* g_pdpfct;
extern t_packet_header_fc_extra* g_pdpfce;
extern t_packet_header_fc_rc_channels* g_pPHFCRCChannels;
extern t_packet_header_rc_info_downstream* g_pPHDownstreamInfoRC; // RC Info received on ground 
extern t_packet_header_vehicle_tx_history g_PHVehicleTxHistory;
extern shared_mem_radio_stats_radio_interface g_SM_VehicleRxStats[MAX_RADIO_INTERFACES];

extern shared_mem_controller_adaptive_video_info* g_pSM_ControllerAdaptiveVideoInfo;
extern shared_mem_radio_stats* g_pSM_RadioStats;
extern shared_mem_video_info_stats* g_pSM_VideoInfoStats;
extern shared_mem_video_info_stats* g_pSM_VideoInfoStatsRadioIn;
extern shared_mem_video_info_stats g_VideoInfoStatsFromVehicle;
extern shared_mem_video_info_stats g_VideoInfoStatsFromVehicleRadioOut;
extern shared_mem_video_decode_stats* g_psmvds;
extern shared_mem_video_decode_stats_history* g_psmvds_history;
extern shared_mem_controller_retransmissions_stats* g_pSM_ControllerRetransmissionsStats;
extern shared_mem_video_link_stats_and_overwrites* g_pSM_VideoLinkStats;
extern shared_mem_video_link_graphs* g_pSM_VideoLinkGraphs;
extern shared_mem_dev_video_bitrate_history g_SM_DevVideoBitrateHistory;

extern t_shared_mem_i2c_controller_rc_in* g_pSM_RCIn; // SBUS/IBUS RC In
extern t_shared_mem_i2c_current* g_pSMVoltage;
extern t_shared_mem_i2c_rotary_encoder_buttons_events* g_pSMRotaryEncoderButtonsEvents;

extern shared_mem_router_packets_stats_history* g_pDebugSMRPST;
