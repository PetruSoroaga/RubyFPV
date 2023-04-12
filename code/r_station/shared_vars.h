#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/models.h"
#include "../base/shared_mem.h"

#include "processor_rx_video.h"

// Define this to get profile logs about receiving and processing rx packets times
#define PROFILE_RX 1
#define PROFILE_RX_MAX_TIME 5

extern bool g_bQuit;
extern bool g_bDebug;

extern FILE* g_fdLogFile;
extern Model* g_pCurrentModel;
extern ControllerSettings* g_pControllerSettings; 
extern ControllerInterfacesSettings* g_pControllerInterfaces;
extern u32 g_uControllerId;

extern bool g_bSearching;
extern u32  g_uSearchFrequency;
extern bool g_bUpdateInProgress;
extern bool s_bReceivedInvalidRadioPackets;

extern u8 s_uLastPingSentId;
extern u8 s_uLastPingRecvId;
extern u8  s_uLastPingRadioLinkId;


extern bool g_bDebugIsPacketsHistoryGraphOn;
extern bool g_bDebugIsPacketsHistoryGraphPaused;

// Router

extern ProcessorRxVideo* g_pMainVideoProcessorRx;

extern shared_mem_video_info_stats g_VideoInfoStats;
extern shared_mem_video_info_stats g_VideoInfoStatsRadioIn;
extern shared_mem_video_info_stats* g_pSM_VideoInfoStats;
extern shared_mem_video_info_stats* g_pSM_VideoInfoStatsRadioIn;

extern shared_mem_controller_vehicles_adaptive_video_info g_ControllerVehiclesAdaptiveVideoInfo;
extern shared_mem_controller_vehicles_adaptive_video_info* g_pSM_ControllerVehiclesAdaptiveVideoInfo;

extern shared_mem_video_decode_stats_history g_VideoDecodeStatsHistory;
extern shared_mem_controller_retransmissions_stats g_ControllerRetransmissionsStats;
extern shared_mem_radio_stats g_Local_RadioStats;
extern shared_mem_radio_stats* g_pSM_RadioStats;
extern shared_mem_video_link_stats_and_overwrites* g_pSM_VideoLinkStats;
extern shared_mem_video_link_graphs* g_pSM_VideoLinkGraphs;
extern shared_mem_router_packets_stats_history* g_pDebug_SM_RouterPacketsStatsHistory;
extern shared_mem_process_stats* g_pProcessStats;
extern t_packet_data_controller_link_stats g_PD_ControllerLinkStats;

extern int g_fIPCFromCentral;
extern int g_fIPCToCentral;
extern int g_fIPCToTelemetry;
extern int g_fIPCFromTelemetry;
extern int g_fIPCToRC;
extern int g_fIPCFromRC;

extern int g_fPipeAudio;

extern bool g_bRuntimeControllerPairingCompleted;
extern bool g_bFirstModelPairingDone;


// Used for Atheros cards data rates set;

extern u32 g_uLastCommandCounterToSetRadioFlags;
extern u32 g_uLastRadioLinkIndexSentRadioCommand;
extern int g_iLastRadioLinkDataDataRateSentRadioCommand;

extern bool g_bIsVehicleLinkToControllerLost;

extern u32 g_uLastCommandRequestIdSent;
extern u32 g_uLastCommandRequestIdRetrySent;