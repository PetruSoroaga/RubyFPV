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

bool g_bSearching = false;
bool g_bSearchFoundVehicle = false;
int g_iSearchFrequency = 0;
bool g_bIsRouterReady = false;

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
