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

#include "shared_vars_state.h"

t_structure_file_upload g_CurrentUploadingFile;
bool g_bHasFileUploadInProgress = false;

int s_StartSequence = 0;
Model* g_pCurrentModel = NULL;

// First vehicle is always the main vehicle, the next ones are relayed vehicles
t_structure_vehicle_info g_SearchVehicleRuntimeInfo;
t_structure_vehicle_info g_VehiclesRuntimeInfo[MAX_CONCURENT_VEHICLES];
int g_iCurrentActiveVehicleRuntimeInfoIndex = 0;

bool g_bSearching = false;
bool g_bSearchFoundVehicle = false;
int g_iSearchFrequency = 0;
bool g_bIsRouterReady = false;

bool g_bFirstModelPairingDone = false;
bool g_bIsFirstConnectionToCurrentVehicle = true;
bool g_bTelemetryLost = false;
bool g_bVideoLost = false;
bool g_bRCFailsafe = false;

bool g_bUpdateInProgress = false;
int g_nFailedOTAUpdates = 0;
int g_nSucceededOTAUpdates = 0;

bool g_bIsVehicleLinkToControllerLost = false;
bool g_bSyncModelSettingsOnLinkRecover = false;
int g_nTotalControllerCPUSpikes = 0;

bool g_bGotStatsVideoBitrate = false;
bool g_bGotStatsVehicleTx = false;
bool g_bGotStatsVehicleRxCards = false;

bool g_bHasVideoDecodeStatsSnapshot = false;

int g_iDeltaVideoInfoBetweenVehicleController = 0;
u32 g_uLastControllerAlarmIOFlags = 0;


void reset_vehicle_runtime_info(t_structure_vehicle_info* pInfo)
{
   if ( NULL == pInfo )
      return;

   if ( pInfo == &g_SearchVehicleRuntimeInfo )
      log_line("Reset vehicle runtime info for searching.");

   pInfo->bGotRubyTelemetryInfo = false;
   pInfo->bGotRubyTelemetryExtraInfo = false;
   pInfo->bGotRubyTelemetryExtraInfoRetransmissions = false;
   pInfo->uTimeLastRecvRubyTelemetry = 0;

   memset( &(pInfo->headerRubyTelemetryExtended), 0, sizeof(t_packet_header_ruby_telemetry_extended_v2));
   memset( &(pInfo->headerRubyTelemetryExtraInfo), 0, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
   memset( &(pInfo->headerRubyTelemetryExtraInfoRetransmissions), 0, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));

   pInfo->bGotFCTelemetry = false;
   pInfo->bGotFCTelemetryExtra = false;
   pInfo->uTimeLastRecvFCTelemetry = 0;

   memset( &(pInfo->headerFCTelemetry), 0, sizeof(t_packet_header_fc_telemetry));
   memset( &(pInfo->headerFCTelemetryExtra), 0, sizeof(t_packet_header_fc_extra));
   memset( &(pInfo->headerFCTelemetryRCChannels), 0, sizeof(t_packet_header_fc_rc_channels));

   pInfo->headerFCTelemetry.flags |= FC_TELE_FLAGS_NO_FC_TELEMETRY;
}

void shared_vars_state_reset_all_vehicles_runtime_info()
{
   reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);

   // First vehicle is always the main vehicle, the next ones are relayed vehicles

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      reset_vehicle_runtime_info(&(g_VehiclesRuntimeInfo[i]));
   }

   if ( NULL != g_pCurrentModel )
   {
      g_VehiclesRuntimeInfo[0].uVehicleId = g_pCurrentModel->vehicle_id;
   }
}
