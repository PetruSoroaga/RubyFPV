#include "shared_vars.h"
#include "timers.h"

bool g_bQuit = false;
bool g_bDebug = false;
Model* g_pCurrentModel = NULL;

ControllerSettings* g_pControllerSettings = NULL; 
ControllerInterfacesSettings* g_pControllerInterfaces = NULL; 
u32 g_uControllerId = MAX_U32;

bool g_bSearching = false;
u32  g_uSearchFrequency = 0;
bool g_bUpdateInProgress = false;
bool s_bReceivedInvalidRadioPackets = false;

bool g_bDebugIsPacketsHistoryGraphOn = false;
bool g_bDebugIsPacketsHistoryGraphPaused = false;


// Router

ProcessorRxVideo* g_pVideoProcessorRxList[MAX_VIDEO_PROCESSORS];

shared_mem_radio_stats_rx_hist* g_pSM_HistoryRxStats = NULL;
shared_mem_radio_stats_rx_hist g_SM_HistoryRxStats;
shared_mem_audio_decode_stats* g_pSM_AudioDecodeStats = NULL;

shared_mem_video_info_stats g_VideoInfoStats;
shared_mem_video_info_stats g_VideoInfoStatsRadioIn;
shared_mem_video_info_stats* g_pSM_VideoInfoStats = NULL;
shared_mem_video_info_stats* g_pSM_VideoInfoStatsRadioIn = NULL;

shared_mem_router_vehicles_runtime_info g_SM_RouterVehiclesRuntimeInfo;
shared_mem_router_vehicles_runtime_info* g_pSM_RouterVehiclesRuntimeInfo = NULL;

shared_mem_video_stream_stats_rx_processors g_SM_VideoDecodeStats;
shared_mem_video_stream_stats_rx_processors* g_pSM_VideoDecodeStats = NULL;

shared_mem_video_stream_stats_history_rx_processors g_SM_VideoDecodeStatsHistory;
shared_mem_video_stream_stats_history_rx_processors* g_pSM_VideoDecodeStatsHistory = NULL;

shared_mem_controller_retransmissions_stats_rx_processors g_SM_ControllerRetransmissionsStats;
shared_mem_controller_retransmissions_stats_rx_processors* g_pSM_ControllerRetransmissionsStats = NULL;

shared_mem_radio_stats g_SM_RadioStats;
shared_mem_radio_stats* g_pSM_RadioStats = NULL;

shared_mem_radio_stats_interfaces_rx_graph g_SM_RadioStatsInterfacesRxGraph;
shared_mem_radio_stats_interfaces_rx_graph* g_pSM_RadioStatsInterfacesRxGraph = NULL;

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

t_sik_radio_state g_SiKRadiosState;

bool g_bFirstModelPairingDone = false;


u32 g_uLastInterceptedCommandCounterToSetRadioFlags = MAX_U32;
u32 g_uLastRadioLinkIndexForSentSetRadioLinkFlagsCommand = MAX_U32;
int g_iLastRadioLinkDataRateSentForSetRadioLinkFlagsCommand = 0;

bool g_bIsVehicleLinkToControllerLost = false;
bool g_bIsControllerLinkToVehicleLost = false;

u32 g_uLastCommandRequestIdSent = MAX_U32;
u32 g_uLastCommandRequestIdRetrySent = MAX_U32;

int g_iShowVideoKeyframesAfterRelaySwitch = 0;
int g_iGetSiKConfigAsyncResult = 0;
int g_iGetSiKConfigAsyncRadioInterfaceIndex = -1;
u8 g_uGetSiKConfigAsyncVehicleLinkIndex = 0;

void resetVehicleRuntimeInfo(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= MAX_CONCURENT_VEHICLES) )
      return;

   log_line("Reset all runtime info for runtime index %d, VID: %u", iIndex, g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[iIndex]);

   memset(&(g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex]), 0, sizeof(shared_mem_controller_adaptive_video_info_vehicle));
   g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[iIndex] = 0;
   resetPairingStateForRuntimeInfo(iIndex);

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_SM_RouterVehiclesRuntimeInfo.uRoundtripTimeVehiclesOnLocalRadioLinks[iIndex][i] = MAX_U32;
      for( int k=0; k<3; k++ )
         g_SM_RouterVehiclesRuntimeInfo.uTimeLastPingSentToVehiclesOnLocalRadioLinks[iIndex][i][k] = 0;
      g_SM_RouterVehiclesRuntimeInfo.uTimeLastPingReplyReceivedFromVehiclesOnLocalRadioLinks[iIndex][i] = 0;
      for( int k=0; k<3; k++ )
         g_SM_RouterVehiclesRuntimeInfo.uLastPingIdSentToVehiclesOnLocalRadioLinks[iIndex][i][k] = 0;
      g_SM_RouterVehiclesRuntimeInfo.uLastPingIdReceivedFromVehiclesOnLocalRadioLinks[iIndex][i] = 0;
   }

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uUpdateInterval = CONTROLLER_ADAPTIVE_VIDEO_SAMPLE_INTERVAL;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uLastUpdateTime = 0;

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift = -1; // Wait for video stream to decide the initial value
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedLevelShift = -1;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedLevelShiftRetryCount = 0;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastAckLevelShift = 0;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastLevelShiftCheckConsistency = g_TimeNow;
}

void removeVehicleRuntimeInfo(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= MAX_CONCURENT_VEHICLES) )
      return;

   log_line("Removing vehicle runtime info index %d, VID %u", iIndex, g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[iIndex]);
   for( int i=iIndex; i<MAX_CONCURENT_VEHICLES-1; i++ )
   {
      memcpy(&(g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i]), &(g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i+1]), sizeof(shared_mem_controller_adaptive_video_info_vehicle));
      g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] = g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i+1];
      g_SM_RouterVehiclesRuntimeInfo.uPairingRequestTime[i] = g_SM_RouterVehiclesRuntimeInfo.uPairingRequestTime[i+1];
      g_SM_RouterVehiclesRuntimeInfo.uPairingRequestId[i] = g_SM_RouterVehiclesRuntimeInfo.uPairingRequestId[i+1];
      g_SM_RouterVehiclesRuntimeInfo.iPairingDone[i] = g_SM_RouterVehiclesRuntimeInfo.iPairingDone[i+1];

      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         g_SM_RouterVehiclesRuntimeInfo.uRoundtripTimeVehiclesOnLocalRadioLinks[i][k] = g_SM_RouterVehiclesRuntimeInfo.uRoundtripTimeVehiclesOnLocalRadioLinks[i+1][k];
         for( int j=0; j<3; j++ )
            g_SM_RouterVehiclesRuntimeInfo.uTimeLastPingSentToVehiclesOnLocalRadioLinks[i][k][j] = g_SM_RouterVehiclesRuntimeInfo.uTimeLastPingSentToVehiclesOnLocalRadioLinks[i+1][k][j];
         g_SM_RouterVehiclesRuntimeInfo.uTimeLastPingReplyReceivedFromVehiclesOnLocalRadioLinks[i][k] = g_SM_RouterVehiclesRuntimeInfo.uTimeLastPingReplyReceivedFromVehiclesOnLocalRadioLinks[i+1][k];
         for( int j=0; j<3; j++ )
            g_SM_RouterVehiclesRuntimeInfo.uLastPingIdSentToVehiclesOnLocalRadioLinks[i][k][j] = g_SM_RouterVehiclesRuntimeInfo.uLastPingIdSentToVehiclesOnLocalRadioLinks[i+1][k][j];
         g_SM_RouterVehiclesRuntimeInfo.uLastPingIdReceivedFromVehiclesOnLocalRadioLinks[i][k] = g_SM_RouterVehiclesRuntimeInfo.uLastPingIdReceivedFromVehiclesOnLocalRadioLinks[i+1][k];
      }
   }
   resetVehicleRuntimeInfo(MAX_CONCURENT_VEHICLES-1);

}

int getVehicleRuntimeIndex(u32 uVehicleId)
{
   if ( 0 == uVehicleId )
      return -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uVehicleId )
         return i;

   return -1;
}

void resetPairingStateForRuntimeInfo(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= MAX_CONCURENT_VEHICLES) )
      return;
   log_line("Reset pairing state for runtime index %d, VID %u", iIndex, g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[iIndex]);
   g_SM_RouterVehiclesRuntimeInfo.iPairingDone[iIndex] = 0;
   g_SM_RouterVehiclesRuntimeInfo.uPairingRequestId[iIndex] = 0;
   g_SM_RouterVehiclesRuntimeInfo.uPairingRequestTime[iIndex] = g_TimeNow;
   g_SM_RouterVehiclesRuntimeInfo.uPairingRequestInterval[iIndex] = 200;
}

void logCurrentVehiclesRuntimeInfo()
{
   char szBuff[256];
   int iCount = 0;
   szBuff[0] = 0;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] != 0 )
      {
         iCount++;
         char szTmp[32];
         sprintf(szTmp, " %u", g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i]);
         strcat(szBuff, szTmp);
      }
   }
   log_line("Currently known runtime vehicle IDs (%d):%s", iCount, szBuff);
}