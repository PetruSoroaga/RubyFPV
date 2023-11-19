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
u32 g_uActiveControllerModelVID = 0;

// First vehicle is always the main vehicle, the next ones are relayed vehicles
t_structure_vehicle_info g_SearchVehicleRuntimeInfo;
t_structure_vehicle_info g_UnexpectedVehicleRuntimeInfo;
t_structure_vehicle_info g_VehiclesRuntimeInfo[MAX_CONCURENT_VEHICLES];
int g_iCurrentActiveVehicleRuntimeInfoIndex = 0;

bool g_bSearching = false;
bool g_bSearchFoundVehicle = false;
int g_iSearchFrequency = 0;
int g_iSearchSiKAirDataRate = -1;
int g_iSearchSiKECC = -1;
int g_iSearchSiKLBT = -1;
int g_iSearchSiKMCSTR = -1;

bool g_bIsRouterReady = false;

bool g_bFirstModelPairingDone = false;
bool g_bIsFirstConnectionToCurrentVehicle = true;
bool g_bVideoLost = false;

bool g_bUpdateInProgress = false;
int g_nFailedOTAUpdates = 0;
int g_nSucceededOTAUpdates = 0;

bool g_bIsVehicleLinkToControllerLost = false;
bool g_bSyncModelSettingsOnLinkRecover = false;
int g_nTotalControllerCPUSpikes = 0;

bool g_bGotStatsVideoBitrate = false;
bool g_bGotStatsVehicleTx = false;

bool g_bHasVideoDecodeStatsSnapshot = false;

int g_iDeltaVideoInfoBetweenVehicleController = 0;
u32 g_uLastControllerAlarmIOFlags = 0;

bool g_bChangedOSDStatsFontSize = false;

u32  g_uTimeStartConfiguringRadioLink = 0;
bool g_bReconfiguringRadioLinks = false;
bool g_bConfiguringRadioLink = false;
int  g_iConfiguringRadioLinkIndex = -1;
bool g_bConfiguringRadioLinkWaitFlagsConfirmation = false;
bool g_bConfiguringRadioLinkWaitVehicleReconfiguration = false;
bool g_bConfiguringRadioLinkWaitControllerReconfiguration = false;

void reset_vehicle_runtime_info(t_structure_vehicle_info* pInfo)
{
   if ( NULL == pInfo )
      return;

   if ( pInfo == &g_SearchVehicleRuntimeInfo )
      log_line("Reset vehicle runtime info for searching.");
   else
      log_line("Reset vehicle runtime info for VID %u", pInfo->uVehicleId);

   pInfo->uVehicleId = 0;
   pInfo->pModel = NULL;

   // Reset Ruby telemetry info

   pInfo->bGotRubyTelemetryInfo = false;
   pInfo->bGotRubyTelemetryInfoShort = false;
   pInfo->bGotRubyTelemetryExtraInfo = false;
   pInfo->bGotRubyTelemetryExtraInfoRetransmissions = false;
   pInfo->bGotStatsVehicleRxCards = false;
   
   pInfo->bRubyTelemetryLost = false;
   pInfo->iFrequencyRubyTelemetryFull = 0;
   pInfo->iFrequencyRubyTelemetryShort = 0;
   pInfo->uTimeLastRecvRubyTelemetry = 0;
   pInfo->uTimeLastRecvRubyTelemetryExtended = 0;
   pInfo->uTimeLastRecvRubyTelemetryShort = 0;
   pInfo->uTimeLastRecvAnyRubyTelemetry = 0;
   
   pInfo->uTimeLastRecvVehicleRxStats = 0;

   memset( &(pInfo->headerRubyTelemetryExtended), 0, sizeof(t_packet_header_ruby_telemetry_extended_v3));
   memset( &(pInfo->headerRubyTelemetryExtraInfo), 0, sizeof(t_packet_header_ruby_telemetry_extended_extra_info));
   memset( &(pInfo->headerRubyTelemetryExtraInfoRetransmissions), 0, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      memset( &(pInfo->SMVehicleRxStats[i]), 0, sizeof(shared_mem_radio_stats_radio_interface));
   // Reset FC telemetry info

   pInfo->bGotFCTelemetry = false;
   pInfo->bGotFCTelemetryShort = false;
   pInfo->bGotFCTelemetryExtra = false;
   pInfo->bFCTelemetrySourcePresent = false;
   pInfo->iFrequencyFCTelemetryFull = 0;
   pInfo->iFrequencyFCTelemetryShort = 0;
   pInfo->uTimeLastRecvFCTelemetry = 0;
   pInfo->uTimeLastRecvFCTelemetryFull = 0;
   pInfo->uTimeLastRecvFCTelemetryShort = 0;
   
   pInfo->uLastFCFlightMode = 0;
   pInfo->uLastFCFlags = 0;

   memset( &(pInfo->headerFCTelemetry), 0, sizeof(t_packet_header_fc_telemetry));
   memset( &(pInfo->headerFCTelemetryExtra), 0, sizeof(t_packet_header_fc_extra));
   memset( &(pInfo->headerFCTelemetryRCChannels), 0, sizeof(t_packet_header_fc_rc_channels));

   pInfo->headerFCTelemetry.flags |= FC_TELE_FLAGS_NO_FC_TELEMETRY;

   pInfo->uTimeLastMessageFromFC = 0;
   pInfo->szLastMessageFromFC[0] = 0;
   
   // Reset other settings

   pInfo->bLinkLost = false;
   pInfo->bRCFailsafeState = false;
   pInfo->bWarningBatteryVoltage = false;
   pInfo->iComputedBatteryCellCount = 1;
   pInfo->bNotificationPairingRequestSent = false;
   pInfo->bPairedConfirmed = false;

   // These are reset by local_stats functions
   //pInfo->bIsArmed = false;
   //pInfo->bHomeSet = false;
   //pInfo->fHomeLat = 0.0;
   //pInfo->fHomeLon = 0.0;
   //pInfo->fHomeLastLat = 0.0;
   //pInfo->fHomeLastLon = 0.0;
   
   // Reset temporary info

   for( int i=0; i<20; i++ )
   {
      pInfo->uSegmentsModelSettingsSize[i] = 0;
      pInfo->uSegmentsModelSettingsIds[i] = 0;
   }
   pInfo->uSegmentsModelSettingsCount = 0;
   pInfo->bWaitingForModelSettings = true;
   pInfo->uTimeLastReceivedModelSettings = MAX_U32;
   pInfo->uTmpLastThrottledFlags = 0;

   pInfo->tmp_iCountRubyTelemetryPacketsFull = 0;
   pInfo->tmp_iCountRubyTelemetryPacketsShort = 0;
   pInfo->tmp_iCountFCTelemetryPacketsFull = 0;
   pInfo->tmp_iCountFCTelemetryPacketsShort = 0;
   pInfo->tmp_uTimeLastTelemetryFrequencyComputeTime = 0;
}

void shared_vars_state_reset_all_vehicles_runtime_info()
{
   log_line("Reset all vehicles runtime info.");

   reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
   reset_vehicle_runtime_info(&g_UnexpectedVehicleRuntimeInfo);
   
   g_iCurrentActiveVehicleRuntimeInfoIndex = 0;
   
   // First vehicle is always the main vehicle, the next ones are relayed vehicles

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      reset_vehicle_runtime_info(&(g_VehiclesRuntimeInfo[i]));
}

void reset_model_settings_download_buffers(u32 uVehicleId)
{
   if ( (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
      {
         for( int k=0; k<20; k++ )
         {
            g_VehiclesRuntimeInfo[i].uSegmentsModelSettingsSize[k] = 0;
            g_VehiclesRuntimeInfo[i].uSegmentsModelSettingsIds[k] = 0;
         }
         g_VehiclesRuntimeInfo[i].uSegmentsModelSettingsCount = 0;
         g_VehiclesRuntimeInfo[i].bWaitingForModelSettings = true;
         g_VehiclesRuntimeInfo[i].uTimeLastReceivedModelSettings = MAX_U32;
         log_line("[Commands]: Did reset model settings download buffers and state for vehicle id %u.", uVehicleId);
      }
   }
}

t_structure_vehicle_info* get_vehicle_runtime_info_for_vehicle_id(u32 uVehicleId)
{
   if ( (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return NULL;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         return &(g_VehiclesRuntimeInfo[i]);
   }
   return NULL;
}

t_packet_header_ruby_telemetry_extended_v3* get_received_relayed_vehicle_telemetry_info()
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( NULL != g_pCurrentModel )
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == g_pCurrentModel->relay_params.uRelayedVehicleId )
         return &(g_VehiclesRuntimeInfo[i].headerRubyTelemetryExtended);
   }
   return NULL;
}


void log_current_runtime_vehicles_info()
{
   char szBuff[1024];
   strcpy(szBuff, "None");
   int iCount = 0;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (g_VehiclesRuntimeInfo[i].uVehicleId == 0) || (g_VehiclesRuntimeInfo[i].uVehicleId == MAX_U32) )
         continue;

      if ( iCount == 0 )
         szBuff[0] = 0;
      else
         strcat(szBuff, ", ");
      char szTmp[128];
      sprintf(szTmp, "%d: %u (%s, %s, Model: %X, model VID: %u)", i, g_VehiclesRuntimeInfo[i].uVehicleId,
         (g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo)?"RT":"no RT",
         (g_VehiclesRuntimeInfo[i].bGotFCTelemetry)?"FCT":"no FCT",
         g_VehiclesRuntimeInfo[i].pModel,
         (g_VehiclesRuntimeInfo[i].pModel != NULL)?(g_VehiclesRuntimeInfo[i].pModel->vehicle_id):0);
      strcat(szBuff, szTmp);
      iCount++;
   }
   log_line("Current runtime info for vehicles: count vehicles found in list: %d", iCount);
   log_line("Current runtime info for vehicles: [%s]", szBuff);
   log_line("Current runtime info for vehicles: active vehicle runtime index: %d", g_iCurrentActiveVehicleRuntimeInfoIndex);
}