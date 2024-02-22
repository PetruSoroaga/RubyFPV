#pragma once

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif 

typedef enum
{
   osdLayout1 = 0,
   osdLayout2,
   osdLayout3,
   osdLayoutLean,
   osdLayoutLeanExtended,
   osdLayoutLast
} osdLayout;

typedef enum
{
   quickActionNone = 0,
   quickActionCycleOSD,
   quickActionOSDSize,
   quickActionTakePicture,
   quickActionVideoRecord,
   quickActionToggleOSD,
   quickActionToggleStats,
   quickActionToggleAllOff,
   quickActionRelaySwitch,
   quickActionCameraProfileSwitch,
   quickActionRCEnable,
   quickActionRotaryFunction,
   quickActionOSDFreeze,
   quickActionSwitchFavorite,
   quickActionLast
} quickAction;


typedef enum
{
   prefVideoDestination_Disk = 0,
   prefVideoDestination_USB,
   prefVideoDestination_Mem
} prefVideoDestination;


typedef enum
{
   prefUnitsMetric = 0,
   prefUnitsImperial = 1,
   prefUnitsMeters = 2,
   prefUnitsFeets = 3
} prefUnits;

typedef struct
{
   int iMenusStacked;
   int iInvertColorsOSD;
   int iInvertColorsMenu;
   int iOSDScreenSize;
   int iOSDFlipVertical;
   int iScaleOSD; // -1 ..0.. +3
   int iScaleAHI; // -3...0 .. 3
   int iScaleMenus; // -3 ..0.. +3
   int iActionQuickButton1;
   int iActionQuickButton2;
   int iActionQuickButton3;
   int iAddOSDOnScreenshots;
   int iStatsToggledOff;
   int iShowLogWindow; // 0 - No, 1 - Only on new content, 2 - Always
   int iMenuDismissesAlarm; // 0
   int iVideoDestination; // 0 - disk, 1 - memory
   int iStartVideoRecOnArm;
   int iStopVideoRecOnDisarm;
   int iShowControllerCPUInfo;
   int iShowBigRecordButton;
   int iSwapUpDownButtons;
   int iSwapUpDownButtonsValues;
   int iAHIToSides;
   int iAHIShowAirSpeed;
   int iAHIStrokeSize; // -2..0..2
   int iUnits;
   int iColorOSD[4];
   int iColorOSDOutline[4];
   int iColorAHI[4];
   int iOSDOutlineThickness; //-3..0..3 (-3 is none)
   int iOSDFont;
   int iRecordingLedAction; // 0 - none, 1 - turn on/off, 2 - blink

   int iDebugMaxPacketSize;
   int iDebugSBWS; // Scramble Blocks Window Size. In HQ mode
   int iDebugRestartOnRadioSilence;
   int iDebugShowDevVideoStats;
   int iDebugShowDevRadioStats;
   int iDebugShowFullRXStats;
   int iDebugShowVehicleVideoStats;
   int iDebugShowVehicleVideoGraphs;
   int iDebugShowVideoSnapshotOnDiscard;
   int iDebugWiFiChangeDelay; // 1...100 milisec
   int iPersistentMessages;
   int nLogLevel; // 0 - all, 1 - errors

   int iAutoExportSettings;
   int iAutoExportSettingsWasModified;
   int iShowProcessesMonitor;
   int iShowCPULoad;
   u32 uEnabledAlarms;
   int iShowOnlyPresentTxPowerCards;
   int iShowTxBoosters;
} Preferences;

int save_Preferences();
int load_Preferences();
void reset_Preferences();
Preferences* get_Preferences();

#ifdef __cplusplus
}  
#endif 