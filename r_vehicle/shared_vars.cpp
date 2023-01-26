#include "shared_vars.h"

bool g_bQuit = false;
bool g_bDebug = false;
Model* g_pCurrentModel = NULL;

shared_mem_process_stats* s_pProcessStats = NULL;
u8* g_pSharedMemRaspiVidComm = NULL;
int g_iBoardType = 0;

t_packet_queue s_QueueControlPackets;

int s_fIPCRouterToCommands = -1;
int s_fIPCRouterFromCommands = -1;
int s_fIPCRouterToTelemetry = -1;
int s_fIPCRouterFromTelemetry = -1;
int s_fIPCRouterToRC = -1;
int s_fIPCRouterFromRC = -1;

int s_fInputVideoStream = -1;
int s_fInputAudioStream = -1;

bool g_bVideoPaused = false;
int s_InputBufferVideoBytesRead = 0;

u16 s_countTXVideoPacketsOutTemp = 0;
u16 s_countTXDataPacketsOutTemp = 0;
u16 s_countTXCompactedPacketsOutTemp = 0;

// Router
shared_mem_radio_stats g_SM_RadioStats;
bool g_bReceivedPairingRequest = false;
bool g_bHasLinkToController = false;
bool g_bHadEverLinkToController = false;

u32  g_uControllerId = 0;
int s_iPendingFrequencyChangeLinkId = -1;
u32 s_uPendingFrequencyChangeTo = 0;
u32 s_uTimeFrequencyChangeRequest = 0;
bool g_bDidSentRaspividBitrateRefresh = false;

int g_iFramesSinceLastH264KeyFrame = 0;
u32 g_uTotalRadioTxTimePerSec = 0;

t_packet_header_ruby_telemetry_extended_extra_info_retransmissions g_PHTE_Retransmissions;
t_packet_header_vehicle_tx_history g_PHVehicleTxStats;
t_packet_data_controller_link_stats g_PD_LastRecvControllerLinksStats;
shared_mem_video_link_stats_and_overwrites g_SM_VideoLinkStats;
shared_mem_video_link_graphs g_SM_VideoLinkGraphs;
shared_mem_dev_video_bitrate_history g_SM_DevVideoBitrateHistory;

shared_mem_video_info_stats g_VideoInfoStats;
shared_mem_video_info_stats g_VideoInfoStatsRadioOut;
shared_mem_video_info_stats* g_pSM_VideoInfoStatsRadioOut = NULL;
shared_mem_video_info_stats* g_pSM_VideoInfoStats = NULL;

int g_iForcedVideoProfile = -1;

// TX Telemetry
int s_MAVLinkRCChannels[16];