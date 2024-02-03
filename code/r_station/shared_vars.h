#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/models.h"
#include "../base/shared_mem.h"
#include "../base/shared_mem_controller_only.h"
#include "../base/utils.h"

#include "processor_rx_video.h"

// Define this to get profile logs about receiving and processing rx packets times
#define PROFILE_RX 1
#define PROFILE_RX_MAX_TIME 15

extern bool g_bQuit;
extern bool g_bDebug;

extern Model* g_pCurrentModel;
extern ControllerSettings* g_pControllerSettings; 
extern ControllerInterfacesSettings* g_pControllerInterfaces;
extern u32 g_uControllerId;

extern bool g_bSearching;
extern u32  g_uSearchFrequency;
extern u32  g_uAcceptedFirmwareType;
extern bool g_bUpdateInProgress;
extern bool s_bReceivedInvalidRadioPackets;

extern bool g_bDebugIsPacketsHistoryGraphOn;
extern bool g_bDebugIsPacketsHistoryGraphPaused;

// Router

extern ProcessorRxVideo* g_pVideoProcessorRxList[MAX_VIDEO_PROCESSORS];

extern shared_mem_radio_stats_rx_hist* g_pSM_HistoryRxStats;
extern shared_mem_radio_stats_rx_hist g_SM_HistoryRxStats;
extern shared_mem_audio_decode_stats* g_pSM_AudioDecodeStats;

extern shared_mem_video_info_stats g_VideoInfoStats;
extern shared_mem_video_info_stats g_VideoInfoStatsRadioIn;
extern shared_mem_video_info_stats* g_pSM_VideoInfoStats;
extern shared_mem_video_info_stats* g_pSM_VideoInfoStatsRadioIn;

extern shared_mem_router_vehicles_runtime_info g_SM_RouterVehiclesRuntimeInfo;
extern shared_mem_router_vehicles_runtime_info* g_pSM_RouterVehiclesRuntimeInfo;

extern shared_mem_video_stream_stats_rx_processors g_SM_VideoDecodeStats;
extern shared_mem_video_stream_stats_rx_processors* g_pSM_VideoDecodeStats;

extern shared_mem_video_stream_stats_history_rx_processors g_SM_VideoDecodeStatsHistory;
extern shared_mem_video_stream_stats_history_rx_processors* g_pSM_VideoDecodeStatsHistory;

extern shared_mem_controller_retransmissions_stats_rx_processors g_SM_ControllerRetransmissionsStats;
extern shared_mem_controller_retransmissions_stats_rx_processors* g_pSM_ControllerRetransmissionsStats;

extern shared_mem_radio_stats g_SM_RadioStats;
extern shared_mem_radio_stats* g_pSM_RadioStats;
extern shared_mem_radio_stats_interfaces_rx_graph g_SM_RadioStatsInterfacesRxGraph;
extern shared_mem_radio_stats_interfaces_rx_graph* g_pSM_RadioStatsInterfacesRxGraph;

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

extern t_sik_radio_state g_SiKRadiosState;

extern bool g_bFirstModelPairingDone;


// Used for Atheros cards data rates set;
extern u32 g_uLastInterceptedCommandCounterToSetRadioFlags;
extern u32 g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand;
extern int g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand;

extern bool g_bIsVehicleLinkToControllerLost;
extern bool g_bIsControllerLinkToVehicleLost;

extern u32 g_uLastCommandRequestIdSent;
extern u32 g_uLastCommandRequestIdRetrySent;

extern int g_iShowVideoKeyframesAfterRelaySwitch;

extern int g_iGetSiKConfigAsyncResult;
extern int g_iGetSiKConfigAsyncRadioInterfaceIndex;
extern u8 g_uGetSiKConfigAsyncVehicleLinkIndex;

void resetVehicleRuntimeInfo(int iIndex);
void removeVehicleRuntimeInfo(int iIndex);
int  getVehicleRuntimeIndex(u32 uVehicleId);
void resetPairingStateForRuntimeInfo(int iIndex);
void logCurrentVehiclesRuntimeInfo();
