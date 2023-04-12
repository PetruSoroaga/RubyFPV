#pragma once

#include "alarms.h"
#include "flags.h"
#include "config_rc.h"
#include "config_file_names.h"
#include "config_obj_names.h"

//#define DISABLE_ALL_LOGS 1
#define FEATURE_ENABLE_RC 1
#define FEATURE_RELAYING 1
//#define FEATURE_ENABLE_RC_FREQ_SWITCH 1
//#define FEATURE_CHECK_LICENCES 1
//#define FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO 1
//#define FEATURE_MSP_OSD 1

#define SYSTEM_NAME "Ruby"
#define SYSTEM_SW_VERSION_MAJOR 7
#define SYSTEM_SW_VERSION_MINOR 40
#define SYSTEM_SW_BUILD_NUMBER  55

#define LOGGER_MESSAGE_QUEUE_ID 123

#define	MAX_RADIO_INTERFACES 6
#define MAX_MAC_LENGTH 20

#define MAX_RELAY_VEHICLES 5
// Should be Main vehicle + relay vehicles (above)
#define MAX_CONCURENT_VEHICLES 6

#define MAX_PLUGIN_NAME_LENGTH 64
#define MAX_OSD_PLUGINS 32
#define MAX_CAMERA_NAME_LENGTH 24

#define DEFAULT_VEHICLE_NAME ""
#define MAX_MODELS 40
#define MAX_MODELS_SPECTATOR 20
#define MAX_TX_POWER 71
#define MAX_MCS_INDEX 5

#define DEFAULT_TX_TIME_OVERLOAD 250 // milisec

#define DEFAULT_FREQUENCY 2472
#define DEFAULT_FREQUENCY_2 2467
#define DEFAULT_FREQUENCY_3 2437
#define DEFAULT_FREQUENCY58 5805
#define DEFAULT_FREQUENCY58_2 5745
#define DEFAULT_FREQUENCY58_3 5680
#define DEFAULT_FREQUENCY_433 430000
#define DEFAULT_FREQUENCY_868 867000
#define DEFAULT_FREQUENCY_915 914000

#define DEFAULT_RADIO_SIK_NETID 27
#define DEFAULT_RADIO_SIK_CHANNELS 5
#define DEFAULT_RADIO_SIK_FREQ_SPREAD 1000 // in kbps
#define DEFAULT_RADIO_SIK_AIR_SPEED 64000
#define DEFAULT_RADIO_SIK_EC 0
#define DEFAULT_RADIO_SIK_LBT 0

// in bps
#define DEFAULT_RADIO_DATARATE_SIK_AIR 64000
// in 1 Mbs increments
#define DEFAULT_RADIO_DATARATE 18
#define DEFAULT_RADIO_TX_POWER_CONTROLLER 45
#define DEFAULT_RADIO_TX_POWER 40
#define DEFAULT_RADIO_SIK_TX_POWER 20
#define DEFAULT_RADIO_SLOTTIME 9
#define DEFAULT_RADIO_THRESH62 26
#define DEFAULT_OVERVOLTAGE 3
#define DEFAULT_ARM_FREQ 900
#define DEFAULT_GPU_FREQ 400

#define DEFAULT_PRIORITY_PROCESS_ROUTER -11
#define DEFAULT_IO_PRIORITY_ROUTER 3 //(negative for disabled)
#define DEFAULT_PRIORITY_PROCESS_RC -9
#define DEFAULT_IO_PRIORITY_RC 2 // (negative for disabled)
#define DEFAULT_PRIORITY_PROCESS_VIDEO_TX -10
#define DEFAULT_PRIORITY_PROCESS_VIDEO_RX -10
#define DEFAULT_IO_PRIORITY_VIDEO_TX 3
#define DEFAULT_IO_PRIORITY_VIDEO_RX 3
#define DEFAULT_PRIORITY_PROCESS_TELEMETRY -8
#define DEFAULT_PRIORITY_PROCESS_OTHERS -3
#define DEFAULT_PRIORITY_PROCESS_CENTRAL -3

#define DEFAULT_MAX_LOOP_TIME_MILISECONDS 30

#define DEFAULT_DELAY_WIFI_CHANGE 50

// Maximum percent of radio datarate to use for video
#define DEFAULT_VIDEO_LINK_MAX_LOAD_PERCENT 80

#define DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH 5
#define DEFAULT_MINIMUM_INTERVALS_FOR_VIDEO_LINK_ADJUSTMENT 3
#define DEFAULT_CONTROLLER_LINK_MILISECONDS_TIMEOUT_TO_DISABLE_RETRANSMISSIONS 3000

#define DEFAULT_VIDEO_H264_QUANTIZATION -18
#define DEFAULT_VIDEO_H264_SLICES 1

#define DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL 6000 // in miliseconds
#define DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL 200 // in miliseconds
#define DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL 100 // in miliseconds

#define DEFAULT_VIDEO_RETRANS_MS5_HP ((u32)28)  // 28*5 = 140 milisec
#define DEFAULT_VIDEO_RETRANS_MS5_HQ ((u32)28)  // 28*5 = 140 milisec
#define DEFAULT_VIDEO_RETRANS_MS5_MQ ((u32)30)  // 30*5 = 150 milisec
#define DEFAULT_VIDEO_RETRANS_MS5_LQ ((u32)36)  // 36*5 = 180 milisec
#define DEFAULT_VIDEO_RETRANS_RETRY_TIME 30 //milisec
#define DEFAULT_VIDEO_RETRANS_REQUEST_ON_VIDEO_SILENCE_MS 50 // milisec

#define DEFAULT_VIDEO_WIDTH 1280
#define DEFAULT_VIDEO_HEIGHT 720
//#define DEFAULT_VIDEO_WIDTH 853
//#define DEFAULT_VIDEO_HEIGHT 480
//#define DEFAULT_VIDEO_WIDTH 1024
//#define DEFAULT_VIDEO_HEIGHT 576
#define DEFAULT_VIDEO_FPS 30
#define DEFAULT_VIDEO_KEYFRAME -7

#define DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE 1700000
#define DEFAULT_VIDEO_BITRATE 7000000 // in bps

#define DEFAULT_VIDEO_BLOCK_PACKETS_HP 4
#define DEFAULT_VIDEO_BLOCK_FECS_HP 1
#define DEFAULT_VIDEO_PACKET_LENGTH_HP 1250

#define DEFAULT_VIDEO_BLOCK_PACKETS_HQ 6
#define DEFAULT_VIDEO_BLOCK_FECS_HQ 2
#define DEFAULT_VIDEO_PACKET_LENGTH_HQ 1250


#define DEFAULT_MQ_VIDEO_BITRATE 4500000 // in bps
#define DEFAULT_MQ_VIDEO_BLOCK_PACKETS 6
#define DEFAULT_MQ_VIDEO_BLOCK_FECS 3
#define DEFAULT_MQ_VIDEO_PACKET_LENGTH 1250
#define DEFAULT_MQ_VIDEO_FPS 30
#define DEFAULT_MQ_VIDEO_KEYFRAME -7

#define DEFAULT_LQ_VIDEO_BITRATE 2000000 // in bps
#define DEFAULT_LQ_VIDEO_BLOCK_PACKETS 6
#define DEFAULT_LQ_VIDEO_BLOCK_FECS 3
#define DEFAULT_LQ_VIDEO_PACKET_LENGTH 1200
#define DEFAULT_LQ_VIDEO_FPS 30
#define DEFAULT_LQ_VIDEO_KEYFRAME -7


#define MAX_AUDIO_PACKETS 32

#define DEFAULT_MAVLINK_SYS_ID_VEHICLE 1
#define DEFAULT_MAVLINK_SYS_ID_CONTROLLER 255

#define DEFAULT_FC_TELEMETRY_SERIAL_SPEED 57600
#define DEFAULT_FC_TELEMETRY_UPDATE_RATE 5 // Times per second, for FC telemetry from vehicle to controller
#define DEFAULT_RUBY_TELEMETRY_UPDATE_RATE 4 // Times per second. How often the Ruby telemetry gets sent from vehicle to controller
#define TIMEOUT_TELEMETRY_LOST 1200 // miliseconds
#define FC_MESSAGE_TIMEOUT 2000  // How long should the last message from FC should be send to controller. Send it for 2 seconds

#define RAW_TELEMETRY_MAX_BUFFER 512  // bytes
#define RAW_TELEMETRY_SEND_TIMEOUT 200 // miliseconds. how much to wait until to send whatever is in a telemetry serial buffer to the radio
#define RAW_TELEMETRY_MIN_SEND_LENGTH 255 // minimum data length to send right away to radio

#define AUXILIARY_DATA_LINK_SEND_TIMEOUT 100 // miliseconds. how much to wait until to send whatever is in a data link serial buffer to the radio
#define AUXILIARY_DATA_LINK_MIN_SEND_LENGTH 255 // minimum data length to send right away to radio

#define VEHICLE_SWITCH_FREQUENCY_AFTER_MS 200

#define TIMEOUT_RADIO_FRAMES_FLAGS_CHANGE_CONFIRMATION 5000

#define DEFAULT_PING_FREQUENCY 4

#define DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY 10

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
} Preferences;

u32* getChannels433();
int getChannels433Count();
u32* getChannels868();
int getChannels868Count();
u32* getChannels915();
int getChannels915Count();
u32* getChannels24();
int getChannels24Count();
u32* getChannels23();
int getChannels23Count();
u32* getChannels25();
int getChannels25Count();
u32* getChannels58();
int getChannels58Count();

u32 getFrequencyInKHz(u32 uFreq);

int getBand(u32 freq);
int getChannelIndexForFrequency(u32 nBand, u32 freq);
int isFrequencyInBands(u32 freq, u8 bands);
int getSupportedChannels(u32 supportedBands, int includeSeparator, u32* pOutChannels, int maxChannels);

int* getDataRates();
int getDataRatesCount();
u32 getRealDataRateFromMCSRate(int mcsIndex);
u32 getRealDataRateFromRadioDataRate(int dataRate);

void getSystemVersionString(char* p, u32 swversion);

int config_file_get_value(const char* szPropName);
void config_file_set_value(const char* szFile, const char* szPropName, int value);
void config_file_force_value(const char* szFile, const char* szPropName, int value);
void config_file_add_value(const char* szFile, const char* szPropName, int value);

void save_simple_config_fileU(const char* fileName, u32 value);
u32 load_simple_config_fileU(const char* fileName, u32 defaultValue);

void save_simple_config_fileI(const char* fileName, int value);
int load_simple_config_fileI(const char* fileName, int defaultValue);

int save_Preferences();
int load_Preferences();
void reset_Preferences();
Preferences* get_Preferences();

void get_Ruby_BaseVersion(int* pMajor, int* pMinor);
void get_Ruby_UpdatedVersion(int* pMajor, int* pMinor);

#ifdef __cplusplus
}  
#endif 