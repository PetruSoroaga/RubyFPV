#pragma once

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif 

#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_DATA_PACKETS  ((u32)(((u32)0x01)<<2))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_H264265_FRAMES      ((u32)(((u32)0x01)<<3))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_DBM                 ((u32)(((u32)0x01)<<4))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS     ((u32)(((u32)0x01)<<5))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS_MAX_GAP ((u32)(((u32)0x01)<<6))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_CONSUMED_PACKETS    ((u32)(((u32)0x01)<<7))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_MIN_MAX_ACK_TIME       ((u32)(((u32)0x01)<<8))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_MAX_EC_USED   ((u32)(((u32)0x01)<<9))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_UNRECOVERABLE_BLOCKS ((u32)(((u32)0x01)<<10))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_PROFILE_CHANGES  ((u32)(((u32)0x01)<<11))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_RETRANSMISSIONS  ((u32)(((u32)0x01)<<12))
#define CTRL_RT_DEBUG_INFO_FLAG_SHOW_ACK_TIME_HISTORY       ((u32)(((u32)0x01)<<13))


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
   int iColorOSD[4]; // 0...255
   int iColorOSDOutline[4]; // 0...255
   int iColorAHI[4]; // 0...255
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
   int iMenuStyle; // 0: clasic, 1: sticky left

   int iDebugStatsQAButton;
   u32 uDebugStatsFlags;
} Preferences;

int save_Preferences();
int load_Preferences();
void reset_Preferences();
Preferences* get_Preferences();

#ifdef __cplusplus
}  
#endif 