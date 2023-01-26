#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/shared_mem.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"

// Define this to get profile logs about receiving and processing rx packets times
#define PROFILE_RX 1
#define PROFILE_RX_MAX_TIME 5

extern bool g_bQuit;
extern bool g_bDebug;

extern Model* g_pCurrentModel;
extern shared_mem_process_stats* s_pProcessStats;
extern u8* g_pSharedMemRaspiVidComm;
extern int g_iBoardType;

extern t_packet_queue s_QueueControlPackets;
 
extern int s_fIPCRouterToCommands;
extern int s_fIPCRouterFromCommands;
extern int s_fIPCRouterToTelemetry;
extern int s_fIPCRouterFromTelemetry;
extern int s_fIPCRouterToRC;
extern int s_fIPCRouterFromRC;

extern int s_fInputVideoStream;
extern int s_fInputAudioStream;

extern bool g_bVideoPaused;
extern int s_InputBufferVideoBytesRead;

extern u16 s_countTXVideoPacketsOutTemp;
extern u16 s_countTXDataPacketsOutTemp;
extern u16 s_countTXCompactedPacketsOutTemp;

// Router

extern shared_mem_radio_stats g_SM_RadioStats;
extern bool g_bReceivedPairingRequest;
extern bool g_bHasLinkToController;
extern bool g_bHadEverLinkToController;

extern u32 g_uControllerId;
extern int s_iPendingFrequencyChangeLinkId;
extern u32 s_uPendingFrequencyChangeTo;
extern u32 s_uTimeFrequencyChangeRequest;
extern bool g_bDidSentRaspividBitrateRefresh;

extern int g_iFramesSinceLastH264KeyFrame;
extern u32 g_uTotalRadioTxTimePerSec;

extern t_packet_header_ruby_telemetry_extended_extra_info_retransmissions g_PHTE_Retransmissions;
extern t_packet_header_vehicle_tx_history g_PHVehicleTxStats;
extern t_packet_data_controller_link_stats g_PD_LastRecvControllerLinksStats;
extern shared_mem_video_link_stats_and_overwrites g_SM_VideoLinkStats;
extern shared_mem_video_link_graphs g_SM_VideoLinkGraphs;
extern shared_mem_dev_video_bitrate_history g_SM_DevVideoBitrateHistory;

extern shared_mem_video_info_stats g_VideoInfoStats;
extern shared_mem_video_info_stats g_VideoInfoStatsRadioOut;
extern shared_mem_video_info_stats* g_pSM_VideoInfoStats;
extern shared_mem_video_info_stats* g_pSM_VideoInfoStatsRadioOut;

extern int g_iForcedVideoProfile;

// TX Telemetry
extern int s_MAVLinkRCChannels[16];

