#pragma once
#include <stdarg.h>
#include "../base/core_plugins_settings.h"

#define COMMAND_ID_SET_VEHICLE_TYPE 2
// Has no particular input/response structure

#define COMMAND_ID_SET_VEHICLE_NAME 3

#define COMMAND_ID_SET_TX_POWERS 4
// 8 bytes: 0...5: u8 global, u8 atheros, u8 RTL, u8 maxGlobal, u8 maxAtheros, u8 maxRTL,
//          6: u8 radio card index, 
//          7: u8 power, if 0 -> not set
// First 8 bytes can be 0xFF or 0x00 for no changes in those params
// Byte 9,10,11 (if present, optional): 0x81 then tx power Sik, then 0x81

#define COMMAND_ID_SET_RADIO_LINK_FREQUENCY 5
// lower 3 bytes: frequency (new format, in khz), 4-th byte: link index, most significat bit set to 1 to signal new format for this parameter

#define COMMAND_ID_SET_RADIO_LINK_CAPABILITIES 6
// low byte: link index, next 3 bytes: link_capabilities_flags (only lowest 3 bytes of it) (tx/rx/relay - video/data)

#define COMMAND_ID_SET_RADIO_LINK_FLAGS 7
// u32 - link index
// u32 - link radio flags (use MCS, MCS flags, STBC, adaptive rates, SIK flags, etc)
// int - datarate video  -1..-n MCS or 0..x classic, in bps
// int - datarate data 

#define COMMAND_ID_RADIO_LINK_FLAGS_CHANGED_CONFIRMATION 8
// param is radio link id

#define COMMAND_ID_SET_RADIO_SLOTTIME 9
#define COMMAND_ID_SET_RADIO_THRESH62 10
#define COMMAND_ID_SET_RADIO_CARD_MODEL 11
// param: low byte: card index, second byte: card model

#define COMMAND_ID_SET_MODEL_FLAGS 12
// param u32 new model flags

#define COMMAND_ID_GET_USB_INFO 13  // no params
#define COMMAND_ID_GET_USB_INFO2 14  // no params

#define COMMAND_ID_SET_NICE_VALUE_TELEMETRY 15
// byte it's a nice+20 for: telemetry

#define COMMAND_ID_SET_NICE_VALUES 16
// each byte it's a nice+20 for: video, other, router, rc

#define COMMAND_ID_SET_IONICE_VALUES 17
#define COMMAND_ID_SET_ENABLE_DHCP 18

#define COMMAND_ID_SET_RADIO_LINK_DATARATES 19 // added in v7.6
// param: radio link index
// data: type_radio_links_parameters structure

#define COMMAND_ID_REBOOT 20
#define COMMAND_ID_RESET_ALL_TO_DEFAULTS 21
#define COMMAND_ID_FACTORY_RESET 22

#define COMMAND_ID_SET_GPS_INFO 23
// param: gps present on vehicle: on/off

#define COMMAND_ID_SET_OSD_PARAMS 24
// an osd_parameters_t structure as input

#define COMMAND_ID_SET_OSD_CURRENT_LAYOUT 25
// the parameter is the layout index

#define COMMAND_ID_GET_CORE_PLUGINS_INFO 26
// request from vehicle info about core plugins present on vehicle
// returns the structure below
typedef struct
{
   CorePluginSettings listPlugins[MAX_CORE_PLUGINS_COUNT];
   int iCountPlugins;
   
} __attribute__((packed)) command_packet_core_plugins_response;


#define COMMAND_ID_SET_SERIAL_PORTS_INFO 28
// contains a model_hardware_info_t structure

#define COMMAND_ID_SET_SIK_PACKET_SIZE 29
// parameter is the sik packet size

#define COMMAND_ID_SET_CAMERA_PARAMETERS 30
// Has a camera profile index and a camera_parameters_t struct as input
// Has no particular response structure

#define COMMAND_ID_SET_CAMERA_PROFILE 31
// Have no particular input/response structure:

#define COMMAND_ID_SET_CURRENT_CAMERA 32
// param: camera index

#define COMMAND_ID_FORCE_CAMERA_TYPE 33
// param: camera type to set for current camera

#define COMMAND_ID_ROTATE_RADIO_LINKS 34

#define COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS 35    // version 5.6
// A model type_functions_parameters structure parameters

#define COMMAND_ID_SET_CONTROLLER_TELEMETRY_OPTIONS 36
// Param has: bit 0: controller telemetry output, bit 1: controller telemetry input

#define COMMAND_ID_SET_RELAY_PARAMETERS 37
// Param is a type_relay_parameters structure

#define COMMAND_ID_SET_VIDEO_PARAMS 39
// a video_parameters_t structure

#define COMMAND_ID_RESET_VIDEO_LINK_PROFILE 41

#define COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES 43
// bytes: type_video_link_profile * MAX_VIDEO_PROFILES

#define COMMAND_ID_SET_VIDEO_H264_QUANTIZATION 45
// param: 0 - auto, value: quantization

#define COMMAND_ID_SWAP_RADIO_INTERFACES 46
#define COMMAND_ID_GET_SIK_CONFIG 47
// param: local radio interface index

#define COMMAND_ID_SET_ALARMS_PARAMS 50
// an type_alarms_parameters structure as input

#define COMMAND_ID_SET_ENCRYPTION_PARAMS 54 // (added ruby 5.1)
// byte 0: flags, byte 1: pass key lenght; byte 2..N: pass key


#define COMMAND_ID_SET_TELEMETRY_PARAMETERS 60
#define COMMAND_ID_SET_RC_CAMERA_PARAMS 61 // param is a u32 with camera control params

#define COMMAND_ID_SET_RC_PARAMS 70 // params are a rc_parameters_t struct

#define COMMAND_ID_SET_AUDIO_PARAMS 73  // (added v.4.4) an audio_parameters_t struct

#define COMMAND_ID_SET_RXTX_SYNC_TYPE 80 // param is rxtx sync type enum (0..2)

#define COMMAND_ID_RESET_CPU_SPEED 81 // (added v5.0)

#define COMMAND_ID_SET_ALL_PARAMS 99
// contains the model file

#define COMMAND_ID_GET_ALL_PARAMS_ZIP 100
// param:
//  byte 0:
//    bit 0: set developer mode on/off: 1/0
//    bit 1: enable telemetry output;
//    bit 2: enable telemetry input;
//    bit 3: enable developer vehicle video link stats
//    bit 4: enable developer vehicle video link graphs
//    bit 5: request sending of full mavlink/ltm telemetry packets
//    bit 6: send back response in small segments (150 bytes each, for low rate radio links)

//  byte 1:
//    MAVLink sys id of the controller
//  byte 2:
//    wifi guard delay (0..100)
//  byte 3:
//    bit 0..3: radio interfaces graph refresh interval: 1...6, same translation to miliseconds as for nGraphRadioRefreshInterval: 10,20,50,100,200,500 ms
//
// Response has one of two types:
//   * the zip model settings, if single packet mode was set (more than 150 bytes)
//   * segments of the zip model settings, if multiple small segments response was requested (smaller than 150 bytes)
//     each segment has:
//               1 byte: file unique id
//               1 byte: segment number
//               1 byte: total segments
//               1 byte: segment size
//               N bytes - segment data (150 bytes)


#define COMMAND_ID_GET_CURRENT_VIDEO_CONFIG 101
#define COMMAND_ID_GET_MODULES_INFO 103
#define COMMAND_ID_GET_MEMORY_INFO 104
#define COMMAND_ID_GET_CPU_INFO 105
#define COMMAND_ID_SET_OVERCLOCKING_PARAMS 107
typedef struct
{
   int overvoltage; // 0 or negative - disabled, negative - default value
   int freq_arm; // 0 - disabled
   int freq_gpu; // 0 - disabled
   
} __attribute__((packed)) command_packet_overclocking_params;



#define COMMAND_ID_DEBUG_GET_TOP 201

#define COMMAND_ID_ENABLE_DEBUG 202
// param - 0/1 to enable debug (developer mode)

#define COMMAND_ID_ENABLE_LIVE_LOG 203
// param - 0/1 to enable live log

#define COMMAND_ID_SET_DEVELOPER_FLAGS 205
// u32 param - developer flags
// extra params (optional):
// u32 - radio rx loop timeout interval

#define COMMAND_ID_RESET_ALL_DEVELOPER_FLAGS 206

#define COMMAND_ID_UPLOAD_FILE_SEGMENT 208 // uploads a file segment to the vehicle. Has this structure then file segment data;

typedef struct
{
   char szFileName[128];
   u32 uFileId;
   u32 uTotalFileSize;
   u32 uTotalSegments;
   u32 uSegmentIndex;
   u32 uSegmentSize; // must be less that max packet size minus all the headers. Last segment will probablly be smaller.
} __attribute__((packed)) command_packet_upload_file_segment;


#define COMMAND_ID_UPLOAD_SW_TO_VEHICLE63 209
#define COMMAND_ID_UPLOAD_SW_TO_VEHICLE 210
typedef struct
{
   int type; // 0: update zip, 1: generated tar file from controller
   u32 total_size; // total_size and block_length are zero to cancel an upload
   u32 file_block_index; // MAX_U32 to cancel an upload
   bool last_block;
   int block_length; // total_size and block_length are zero to cancel an upload
} __attribute__((packed)) command_packet_sw_package;


#define COMMAND_ID_DOWNLOAD_FILE 211 // has as param the ID of the file to download (high bit: request just status); has a response info about the file: t_packet_header_download_file_info

typedef struct
{
   u32 file_id; // what file was requested
   u8  szFileName[128]; // original filename
   u32 segments_count; // how many segments are in the file
   u32 segment_size; // how many bytes are in a segment
   u8 isReady; // 1 - file is ready: 0 - file is being preprocessed; 2 - error
} __attribute__((packed)) t_packet_header_download_file_info;

#define COMMAND_ID_DOWNLOAD_FILE_SEGMENT 212 // has as param: low word: file ID, high word: file segment; has as response data: dword: fileid and segment id then segment data


#define COMMAND_ID_CLEAR_LOGS 213


#define COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_LOW 220
#define COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_MED 221
#define COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_NORMAL 222
#define COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_AUTO 225

//------------------------------------------------------
const char* commands_get_description(u8 command_type);
