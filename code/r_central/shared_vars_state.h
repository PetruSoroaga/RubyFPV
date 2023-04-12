#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../base/commands.h"

#define MAX_SEGMENTS_FILE_UPLOAD 10000 // About 10 Mbytes of data maximum

typedef struct
{
   u32 uFileId;
   char szFileName[128];
   command_packet_upload_file_segment currentUploadSegment;

   u32 uTotalSegments;
   u8* pSegments[MAX_SEGMENTS_FILE_UPLOAD];
   u32 uSegmentsSize[MAX_SEGMENTS_FILE_UPLOAD];
   bool bSegmentsUploaded[MAX_SEGMENTS_FILE_UPLOAD];

   u32 uTimeLastUploadSegment;
   u32 uLastSegmentIndexUploaded;
} __attribute__((packed)) t_structure_file_upload;

extern t_structure_file_upload g_CurrentUploadingFile;
extern bool g_bHasFileUploadInProgress;

typedef struct 
{
   u32 uVehicleId;

   // Ruby telemetry

   bool bGotRubyTelemetryInfo;
   bool bGotRubyTelemetryExtraInfo;
   bool bGotRubyTelemetryExtraInfoRetransmissions;
   u32 uTimeLastRecvRubyTelemetry;

   t_packet_header_ruby_telemetry_extended_v2 headerRubyTelemetryExtended;
   t_packet_header_ruby_telemetry_extended_extra_info headerRubyTelemetryExtraInfo;
   t_packet_header_ruby_telemetry_extended_extra_info_retransmissions headerRubyTelemetryExtraInfoRetransmissions;

   // FC telemetry

   bool bGotFCTelemetry;
   bool bGotFCTelemetryExtra;
   u32 uTimeLastRecvFCTelemetry;

   t_packet_header_fc_telemetry headerFCTelemetry;
   t_packet_header_fc_extra     headerFCTelemetryExtra;
   t_packet_header_fc_rc_channels headerFCTelemetryRCChannels;
} t_structure_vehicle_info;

extern int s_StartSequence;
extern Model* g_pCurrentModel;

// First vehicle is always the main vehicle, the next ones are relayed vehicles
extern t_structure_vehicle_info g_SearchVehicleRuntimeInfo;
extern t_structure_vehicle_info g_VehiclesRuntimeInfo[MAX_CONCURENT_VEHICLES];
extern int g_iCurrentActiveVehicleRuntimeInfoIndex;

extern bool g_bSearching;
extern bool g_bSearchFoundVehicle;
extern int g_iSearchFrequency;
extern bool g_bIsReinit;
extern bool g_bIsRouterReady;

extern bool g_bFirstModelPairingDone;
extern bool g_bIsFirstConnectionToCurrentVehicle;
extern bool g_bTelemetryLost;
extern bool g_bVideoLost;
extern bool g_bRCFailsafe;

extern bool g_bUpdateInProgress;
extern int g_nFailedOTAUpdates;
extern int g_nSucceededOTAUpdates;

extern bool g_bIsVehicleLinkToControllerLost;
extern bool g_bSyncModelSettingsOnLinkRecover;

extern int g_nTotalControllerCPUSpikes;

extern bool g_bGotStatsVideoBitrate;
extern bool g_bGotStatsVehicleTx;
extern bool g_bGotStatsVehicleRxCards;

extern bool g_bHasVideoDecodeStatsSnapshot;

extern int g_iDeltaVideoInfoBetweenVehicleController;

extern u32 g_uLastControllerAlarmIOFlags;

void reset_vehicle_runtime_info(t_structure_vehicle_info* pInfo);
void shared_vars_state_reset_all_vehicles_runtime_info();
