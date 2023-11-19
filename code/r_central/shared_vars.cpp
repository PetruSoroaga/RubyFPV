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

#include "shared_vars.h"

vehicle_and_telemetry_info_t g_VehicleTelemetryInfo;

u32 g_uControllerId = 0;
int g_iBootCount = 0;

RenderEngine* g_pRenderEngine = NULL;
RenderEngineUI* g_pRenderEngineOSDPlugins = NULL;

u32 g_uVideoRecordStartTime = 0;
bool g_bVideoRecordingStarted = false;
bool g_bVideoProcessing = false;
bool g_bVideoPlaying = false;
u32 g_uVideoPlayingStartTime = 0;
u32 g_uVideoPlayingLengthSec = 0;

int g_ControllerCPUSpeed = 0;
int g_ControllerCPULoad = 0;
int g_ControllerTemp = 0;

bool g_bToglleOSDOff = false;
bool g_bToglleStatsOff = false;
bool g_bToglleAllOSDOff = false;
bool g_bFreezeOSD = false;

bool g_bIsRouterPacketsHistoryGraphOn = false;
bool g_bIsRouterPacketsHistoryGraphPaused = false;

bool g_bMenuPopupUpdateVehicleShown = false;
type_radio_links_parameters g_LastGoodRadioLinksParams;

bool g_bSwitchingRadioLink = false;

PopupCameraParams* g_pPopupCameraParams = NULL;
Popup* g_pPopupLooking = NULL;
Popup* g_pPopupWrongModel = NULL;
Popup* g_pPopupLinkLost = NULL;

bool g_bHasVideoDataOverloadAlarm = false;
bool g_bHasVideoTxOverloadAlarm = false;
Popup* g_pPopupVideoOverloadAlarm = NULL;

int g_iMustSendCurrentActiveOSDLayoutCounter = 0;

CorePluginSettings g_listVehicleCorePlugins[MAX_CORE_PLUGINS_COUNT];
int g_iVehicleCorePluginsCount = 0;
