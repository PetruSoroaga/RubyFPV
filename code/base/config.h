#pragma once

#include "config_hw.h"
#include "alarms.h"
#include "flags.h"
#include "config_rc.h"
#include "config_file_names.h"
#include "config_obj_names.h"
#include "config_video.h"
#include "config_timers.h"

#define ALIGN_STRUCT_SPEC_INFO __attribute__((aligned(4)))

//#define DISABLE_ALL_LOGS 1
#define FEATURE_ENABLE_RC 1
#define FEATURE_RELAYING 1
//#define FEATURE_CHECK_LICENCES 1
//#define FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO 1
//#define FEATURE_LOCAL_AUDIO_RECORDING 1
//#define FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
//#define LOG_RAW_TELEMETRY

#define RADIO_TX_MESSAGE_QUEUE_ID 117

#define	MAX_RADIO_INTERFACES 6
#define MAX_RADIO_ANTENNAS 4
#define MAX_MAC_LENGTH 20
#define MIN_BOOST_INPUT_SIGNAL 3
#define MAX_BOOST_INPUT_SIGNAL 100
#define MAX_RELAY_VEHICLES 5
// Should be Main vehicle + relay vehicles (above)

#define MAX_CONCURENT_VEHICLES 6

#define MAX_PLUGIN_NAME_LENGTH 64
#define MAX_OSD_PLUGINS 32
#define MAX_CAMERA_NAME_LENGTH 24

#define DEFAULT_VEHICLE_NAME ""
#ifdef HW_PLATFORM_OPENIPC_CAMERA
#define MAX_MODELS 2
#define MAX_MODELS_SPECTATOR 2
#else
#define MAX_MODELS 40
#define MAX_MODELS_SPECTATOR 20
#endif
#define MAX_TX_POWER 71
#define MAX_MCS_INDEX 9


#define SYSTEM_RT_INFO_UPDATE_INTERVAL_MS 5
#define SYSTEM_RT_INFO_INTERVALS 400

#define DEFAULT_TX_TIME_OVERLOAD 500 // milisec

// Frequencies are in kHz
#define DEFAULT_FREQUENCY_433  443000
#define DEFAULT_FREQUENCY_868  867000
#define DEFAULT_FREQUENCY_915  914000
#define DEFAULT_FREQUENCY     2472000
#define DEFAULT_FREQUENCY_2   2467000
#define DEFAULT_FREQUENCY_3   2437000
#define DEFAULT_FREQUENCY58   5825000
#define DEFAULT_FREQUENCY58_2 5885000
#define DEFAULT_FREQUENCY58_3 5745000

#define DEFAULT_RADIO_FRAMES_FLAGS (RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_FRAME_TYPE_DATA)

#define DEFAULT_RADIO_SIK_NETID 27
#define DEFAULT_RADIO_SIK_CHANNELS 5
#define DEFAULT_RADIO_SIK_FREQ_SPREAD 1000 // in kbps
// Default sik packet size is big enough to capture the full t_packet_header in the first sik short packet received for a message
#define DEFAULT_SIK_PACKET_SIZE 24

#define DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE 24
#define DEFAULT_RADIO_SERIAL_AIR_MIN_PACKET_SIZE 10
#define DEFAULT_RADIO_SERIAL_AIR_MAX_PACKET_SIZE 127
#define DEFAULT_RADIO_SERIAL_MAX_TX_LOAD 75 // in percentages

// in bps
#define DEFAULT_RADIO_DATARATE_SERIAL_AIR 4000
#define DEFAULT_RADIO_DATARATE_SIK_AIR 64000
#define DEFAULT_RADIO_DATARATE_VIDEO 18000000
#define DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS 12000000
#define DEFAULT_RADIO_DATARATE_LOWEST 6000000
#define DEFAULT_RADIO_DATARATE_DATA 6000000

#define DEFAULT_USE_PPCAP_FOR_TX 0
#define DEFAULT_BYPASS_SOCKET_BUFFERS 1
#define DEFAULT_RADIO_TX_POWER_CONTROLLER 20
#define DEFAULT_RADIO_TX_POWER 20
#define DEFAULT_RADIO_SIK_TX_POWER 11
#define DEFAULT_OVERVOLTAGE 3
#define DEFAULT_ARM_FREQ 900
#define DEFAULT_GPU_FREQ 400
#define DEFAULT_FREQ_OPENIPC_SIGMASTAR 1000
#define DEFAULT_FREQ_RADXA 1416

#define DEFAULT_PRIORITY_PROCESS_ROUTER -10
#define DEFAULT_PRIORITY_PROCESS_ROUTER_OPIC -12
#define DEFAULT_IO_PRIORITY_ROUTER 3 //(negative for disabled)
#define DEFAULT_PRIORITY_PROCESS_RC -9
#define DEFAULT_IO_PRIORITY_RC 2 // (negative for disabled)
#define DEFAULT_PRIORITY_PROCESS_VIDEO_TX -9
#define DEFAULT_PRIORITY_PROCESS_VIDEO_RX -9 // 0 - auto
#define DEFAULT_IO_PRIORITY_VIDEO_TX 3
#define DEFAULT_IO_PRIORITY_VIDEO_RX 3
#define DEFAULT_PRIORITY_PROCESS_TELEMETRY -7
#define DEFAULT_PRIORITY_PROCESS_TELEMETRY_OIPC -3
#define DEFAULT_PRIORITY_PROCESS_OTHERS -3
#define DEFAULT_PRIORITY_PROCESS_CENTRAL -3

#define DEFAULT_PRIORITY_VEHICLE_THREAD_ROUTER 5
#define DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_RX 20 // of 99
#define DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_TX 2 // of 99

#define DEFAULT_PRIORITY_THREAD_ROUTER 5
#define DEFAULT_PRIORITY_THREAD_RADIO_RX 20 // of 99
#define DEFAULT_PRIORITY_THREAD_RADIO_TX 2 // of 99

#define DEFAULT_MAVLINK_SYS_ID_VEHICLE 1
#define DEFAULT_MAVLINK_SYS_ID_CONTROLLER 255

#if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA_ZERO3)
#define DEFAULT_FC_TELEMETRY_SERIAL_SPEED 57600
#endif
#ifdef HW_PLATFORM_OPENIPC_CAMERA
#define DEFAULT_FC_TELEMETRY_SERIAL_SPEED 115200
#endif

#define DEFAULT_TELEMETRY_SEND_RATE 4 // Times per second. How often the Ruby/FC telemetry gets sent from vehicle to controller

#define RAW_TELEMETRY_MAX_BUFFER 512  // bytes
#define RAW_TELEMETRY_SEND_TIMEOUT 200 // miliseconds. how much to wait until to send whatever is in a telemetry serial buffer to the radio
#define RAW_TELEMETRY_MIN_SEND_LENGTH 255 // minimum data length to send right away to radio

#define AUXILIARY_DATA_LINK_SEND_TIMEOUT 100 // miliseconds. how much to wait until to send whatever is in a data link serial buffer to the radio
#define AUXILIARY_DATA_LINK_MIN_SEND_LENGTH 255 // minimum data length to send right away to radio

#define DEFAULT_PING_FREQUENCY 2

#define DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY 10

#define DEFAULT_RADXA_DISPLAY_WIDTH 1280
#define DEFAULT_RADXA_DISPLAY_HEIGHT 720
#define DEFAULT_RADXA_DISPLAY_REFRESH 60
#define DEFAULT_MPP_BUFFERS_SIZE 32

#ifdef __cplusplus
extern "C" {
#endif 

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

int getBand(u32 freqKhz);
int getChannelIndexForFrequency(u32 nBand, u32 freqKhz);
int isFrequencyInBands(u32 freqKhz, u8 bands);
int getSupportedChannels(u32 supportedBands, int includeSeparator, u32* pOutChannels, int maxChannels);

int* getSiKAirDataRates();
int  getSiKAirDataRatesCount();
int* getDataRatesBPS();
int getDataRatesCount();
u32 getRealDataRateFromMCSRate(int mcsIndex, int iHT40);
u32 getRealDataRateFromRadioDataRate(int dataRateBPS, int iHT40);

void getSystemVersionString(char* p, u32 swversion);

int config_file_get_value(const char* szPropName);
void config_file_set_value(const char* szFile, const char* szPropName, int value);
void config_file_force_value(const char* szFile, const char* szPropName, int value);
void config_file_add_value(const char* szFile, const char* szPropName, int value);

void save_simple_config_fileU(const char* fileName, u32 value);
u32 load_simple_config_fileU(const char* fileName, u32 defaultValue);

void save_simple_config_fileI(const char* fileName, int value);
int load_simple_config_fileI(const char* fileName, int defaultValue);

FILE* try_open_base_version_file(char* szOutputFile);
void get_Ruby_BaseVersion(int* pMajor, int* pMinor);
void get_Ruby_UpdatedVersion(int* pMajor, int* pMinor);

#ifdef __cplusplus
}  
#endif 