#include "shared_vars.h"

bool g_bQuit = false;
bool g_bDebug = false;
FILE* g_fdLogFile = NULL;
Model* g_pCurrentModel = NULL;

ControllerSettings* g_pControllerSettings = NULL; 
ControllerInterfacesSettings* g_pControllerInterfaces = NULL; 
u32 g_uControllerId = MAX_U32;

bool g_bSearching = false;
u32  g_uSearchFrequency = 0;
bool g_bUpdateInProgress = false;
bool s_bReceivedInvalidRadioPackets = false;

u8 s_uLastPingSentId = 0xFF;
u8 s_uLastPingRecvId = 0xFF;
u8  s_uLastPingRadioLinkId = 0;

bool g_bDebugIsPacketsHistoryGraphOn = false;
bool g_bDebugIsPacketsHistoryGraphPaused = false;


// Router

ProcessorRxVideo* g_pMainVideoProcessorRx = NULL;

shared_mem_video_info_stats g_VideoInfoStats;
shared_mem_video_info_stats g_VideoInfoStatsRadioIn;
shared_mem_video_info_stats* g_pSM_VideoInfoStats = NULL;
shared_mem_video_info_stats* g_pSM_VideoInfoStatsRadioIn = NULL;

shared_mem_controller_vehicles_adaptive_video_info g_ControllerVehiclesAdaptiveVideoInfo;
shared_mem_controller_vehicles_adaptive_video_info* g_pSM_ControllerVehiclesAdaptiveVideoInfo = NULL;

shared_mem_video_decode_stats_history g_VideoDecodeStatsHistory;
shared_mem_controller_retransmissions_stats g_ControllerRetransmissionsStats;
shared_mem_radio_stats g_Local_RadioStats;
shared_mem_radio_stats* g_pSM_RadioStats = NULL;
shared_mem_video_link_stats_and_overwrites* g_pSM_VideoLinkStats = NULL;
shared_mem_video_link_graphs* g_pSM_VideoLinkGraphs = NULL;
shared_mem_router_packets_stats_history* g_pDebug_SM_RouterPacketsStatsHistory = NULL;
shared_mem_process_stats* g_pProcessStats = NULL;

t_packet_data_controller_link_stats g_PD_ControllerLinkStats;

int g_fIPCFromCentral = -1;
int g_fIPCToCentral = -1;
int g_fIPCToTelemetry = -1;
int g_fIPCFromTelemetry = -1;
int g_fIPCToRC = -1;
int g_fIPCFromRC = -1;

int g_fPipeAudio = -1;

bool g_bRuntimeControllerPairingCompleted = false;
bool g_bFirstModelPairingDone = false;


u32 g_uLastCommandCounterToSetRadioFlags = MAX_U32;
u32 g_uLastRadioLinkIndexSentRadioCommand = MAX_U32;
int g_iLastRadioLinkDataDataRateSentRadioCommand = 0;

bool g_bIsVehicleLinkToControllerLost = false;

u32 g_uLastCommandRequestIdSent = MAX_U32;
u32 g_uLastCommandRequestIdRetrySent = MAX_U32;
