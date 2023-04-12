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

#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../public/telemetry_info.h"
#include "local_packets.h"
#include "radiopackets_short.h"

#define MAX_RXTX_BLOCKS_BUFFER 400

#define MAX_DATA_PACKETS_IN_BLOCK 32
#define MAX_FECS_PACKETS_IN_BLOCK 32
#define MAX_TOTAL_PACKETS_IN_BLOCK 64

#define MAX_PACKET_RADIO_HEADERS 100
#define MAX_PACKET_PAYLOAD 1250
#define MAX_PACKET_TOTAL_SIZE 1600

#define RADIO_PORT_ROUTER_UPLINK 0x0E     // from controller to vehicle
#define RADIO_PORT_ROUTER_DOWNLINK 0x0F   // from vehicle to controller

#define MAX_RADIO_STREAMS 8
#define MAX_VIDEO_STREAMS 4
// (must be less than max radio streams - id of first video stream value below)

#define STREAM_ID_DATA    ((u32)0)
#define STREAM_ID_VIDEO_1 ((u32)3)

// Packet flags is a byte and contains flags:
// bit 0..2 - target module type (video/telem/rc/comm) (3 bits)
// bit 3 - header only CRC
// bit 4 - chained flag: has more radio packets in the same buffer, just after this packet
// bit 5 - has extra data after the regular packet data
// bit 6 - packet is encrypted (everything after packet_flags field is encrypted)
// bit 7 - can TX after this one (to be used by the controller when it receives this packet)

#define PACKET_FLAGS_MASK_MODULE 0b0111
#define PACKET_FLAGS_MASK_STREAM_PACKET_IDX 0x0FFFFFFF
#define PACKET_FLAGS_MASK_STREAM_INDEX (((u32)0x0F)<<28)
#define PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX (28)
#define PACKET_FLAGS_BIT_HEADERS_ONLY_CRC ((u8)(1<<3))
#define PACKET_FLAGS_BIT_RETRANSMITED     ((u8)(1<<4))
#define PACKET_FLAGS_BIT_EXTRA_DATA       ((u8)(1<<5))
#define PACKET_FLAGS_BIT_HAS_ENCRYPTION   ((u8)(1<<6))
#define PACKET_FLAGS_BIT_CAN_START_TX     ((u8)(1<<7))

#define PACKET_COMPONENT_LOCAL_CONTROL 0 // Used only internally, to exchange data between processes
#define PACKET_COMPONENT_VIDEO 1
#define PACKET_COMPONENT_TELEMETRY 2
#define PACKET_COMPONENT_COMMANDS 3
#define PACKET_COMPONENT_RC 4
#define PACKET_COMPONENT_RUBY 5  // Used for control messages between vehicle and controller
#define PACKET_COMPONENT_AUDIO 6
#define PACKET_COMPONENT_DATA 7

#define PACKET_TYPE_EMBEDED_FULL_PACKET 0xFF
#define PACKET_TYPE_EMBEDED_SHORT_PACKET 0x00

//---------------------------------------
// COMPONENT RUBY PACKETS

#define PACKET_TYPE_RUBY_PING_CLOCK 3
// params after header:
//   u8 ping id
//   u8 radio link id
//   optional: serialized minimized t_packet_data_controller_link_stats - link stats (video and radio)


#define PACKET_TYPE_RUBY_PING_CLOCK_REPLY 4 // has: u8 original ping id after header, u32 local time in micro seconds, u8 radio link id

#define PACKET_TYPE_RUBY_RADIO_REINITIALIZED 5 // Sent by vehicle when a radio interface crashed on the vehicle.

#define PACKET_TYPE_RUBY_MODEL_SETTINGS 6 // Sent by vehicle after boot. Sends all model settings (compressed) to controller;

#define PACKET_TYPE_RUBY_PAIRING_REQUEST 7 // Sent by controller when it has link with vehicle for first time. So that vehicle has controller id. Has an optional u32 param after header: count of retires;
#define PACKET_TYPE_RUBY_PAIRING_CONFIRMATION 8 // Sent by vehicle to controller. Has an optional u32 param after header: count of received pairing requests;

#define PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED 9 // Sent by vehicle to controller to let it know about the current radio config.
                                           // Contains a type_relay_parameters, type_radio_interfaces_parameters and a type_radio_links_parameters

#define PACKET_TYPE_RUBY_LOG_FILE_SEGMENT 12 // from vehicle to controller, contains a file segment header too and then file data segment

#define PACKET_TYPE_RUBY_ALARM 15 // Contains an alarm id(s) (u32), flags (u32) and a counter for the alarm (u32)

typedef struct
{
   u32 file_id; // what file is this (live log, configuration files, etc)
   u32 segment_index;
   u32 segment_size; // size in bytes of the segment
   u32 total_segments; // total segments in file
} __attribute__((packed)) t_packet_header_file_segment;


//---------------------------------------
// COMPONENT VIDEO PACKETS

#define PACKET_TYPE_VIDEO_DATA_FULL 2
#define PACKET_TYPE_VIDEO_DATA_SHORT 3

#define PACKET_TYPE_AUDIO_SEGMENT 4
// 0..3 - u32 - audio segment index
// 4...length - audio data

#define PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS 20
// params after header:
//   u8: video link id
//   u8: number of packets requested
//   (u32+u8+u8)*n = each video block index and video packet index requested + repeat count
//   optional: serialized minimized t_packet_data_controller_link_stats - link stats (video and radio)

#define PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2 21
// params after header:
//   u32: retransmission request unique id
//   u8: video link id
//   u8: number of packets requested
//   (u32+u8+u8)*n = each video block index and video packet index requested + repeat count
//   optional: serialized minimized t_packet_data_controller_link_stats - link stats (video and radio)

#define PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL 60 // From controller to vehicle. Contains an u32 - adaptive video level to switch to (0..N - HQ, M...P - MQ, R...T - LQ)
#define PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK 61 // From vehicle to controller. Contains an u32 - adaptive video level to switch to (0..N - HQ, M...P - MQ, R...T - LQ)

#define PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE 64 // From controller to vehicle. Contains the deisred keyframe as an u32
#define PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK 65 // From vehicle to controller. Contains the acknowledge keyframe value as an u32

//---------------------------------------
// COMPONENT COMMANDS PACKETS

#define PACKET_TYPE_COMMAND 11
#define PACKET_TYPE_COMMAND_RESPONSE 12 // Command response is send back on the telemetry port


//---------------------------------------
// COMPONENT TELEMETRY PACKETS


#define PACKET_TYPE_RUBY_TELEMETRY_EXTENDED 30   // Ruby telemetry, extended version
// Contains:
// t_packet_header
// t_packet_header_ruby_telemetry_extended_v2
// t_packet_header_ruby_telemetry_extended_extra_info
// t_packet_header_ruby_telemetry_extended_extra_info_retransmissions
// extraData (0 or more, as part of the packet, after headers)

#define PACKET_TYPE_FC_TELEMETRY 31
#define PACKET_TYPE_FC_TELEMETRY_EXTENDED 32 // FC telemetry + FC message
#define PACKET_TYPE_FC_RC_CHANNELS 34
#define PACKET_TYPE_RC_TELEMETRY 33
#define PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS 35 // has a shared_mem_video_link_stats_and_overwrites structure as data
#define PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_GRAPHS 36 // has a shared_mem_video_link_graphs structure as data

#define PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY 37 // has a t_packet_header_vehicle_tx_gap_history structure
#define PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS 38 // has 1 byte (count cards) then (count cards * shared_mem_radio_stats_radio_interface) after the header
#define PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY 39 // has a shared_mem_dev_video_bitrate_history structure


#define PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD 41 // download telemetry data packet from vehicle to controller
// payload is a packet_header_telemetry_raw structure and then is the actual telemetry data
#define PACKET_TYPE_TELEMETRY_RAW_UPLOAD 42  // upload telemetry data packet from controller to vehicle
// payload is a packet_header_telemetry_raw structure and then is the actual telemetry data

#define PACKET_TYPE_AUX_DATA_LINK_UPLOAD 45  // upload data link packet from controller to vehicle
// payload is a data link segment index and data
#define PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD 46  // upload data link packet from controller to vehicle
// payload is a data link segment index and data

#define PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS 47 // has a shared_mem_video_info_stats structure

//----------------------------------------
// all packets start with this basic header and then a packet specific header
// different packets have different headers, but all headers are preceided by this basic header
// Packet: [header] + [specific header] + [payload_data];
//         | <-total headers length-> |                    // less the start CRC
//         |              <- total length ->           |   // less the start CRC

typedef struct
{
   u32 crc; // computed for the entire packet or header only. start point is after this crc.
   u8 packet_flags;
   u8 packet_type; // 1...150: components packets types, 150-200: local control controller packets, 200-250: local control vehicle packets
   u32 stream_packet_idx; // high 4 bits: stream id (0..15), lower 28 bits: stream packet index

   u16 total_headers_length;  // Total length of all headers
   u16 total_length; // Total length, including all the header data, including CRC
   u16 extra_flags; // bit 15: 1 - if valid flags are present
   u32 vehicle_id_src;
   u32 vehicle_id_dest;
} __attribute__((packed)) t_packet_header;


//------------------------------
// Packet extra info:
//
// bytes data
// u8 type;
// u8 size; (of all this)

// id < 128 - extra data from vehicle -> controller
// id > 128 - extra data from controller -> vehicle

// 6 bytes: u32 - new frequency, data[4] = message below; data[5] = 6;
#define EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK1  0x01
#define EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK2  0x02
#define EXTRA_PACKET_INFO_TYPE_FREQ_CHANGE_LINK3  0x03

#define VIDEO_TYPE_NONE 0
#define VIDEO_TYPE_H264 1

//----------------------------------------------
// packet_header_video_full
//
typedef struct
{
   u8 video_link_profile;
      // (video stream id: 0xF0 bits, current video link profile: 0x0F bits); added in v.5.6
   u8 video_type; // h264, IP, etc
   u32 encoding_extra_flags; // same as video_params.encoding_extra_flags;
      // byte 0:
      //    bit 0..2  - scramble blocks count
      //    bit 3     - enables feature: restransmission of missing packets
      //    bit 4     - signals that current video is on reduced video bitrate due to tx time overload on vehicle
      //    bit 5     - enable adaptive video link params
      //    bit 6     - use controller info too when adjusting video link params
      //    bit 7     - go lower adaptive video profile when controller link lost

      // byte 1:   - max time to wait for retransmissions (in ms*5)// affects rx buffers size
      // byte 2:   - retransmission duplication percent (0-100%), 0xFF = auto, bit 0..3 - regular packets duplication, bit 4..7 - retransmitted packets duplication
      // byte 3:
      //    bit 0  - use medium adaptive video
      //    bit 1  - enable video auto quantization
      //    bit 2  - video auto quantization strength

   u16 video_width; // highest bit is video stream id bit 1 (0..3);  // For retransmitted packets w+h is the u32 received retransmission unique id
   u16 video_height; // highest bit is video stream id bit 2 (0..3); // For retransmitted packets w+h is the u32 received retransmission unique id
   u8  video_fps;
   u8  video_keyframe;
   u8  block_packets;
   u8  block_fecs;
   u16 video_packet_length;

   u32 video_block_index;
   u8  video_block_packet_index;
   u32 fec_time; // how long FEC took, in microseconds/second
} __attribute__((packed)) t_packet_header_video_full;


#define COMMAND_TYPE_FLAG_NO_RESPONSE_NEEDED ( (u16) (((u16)0x01)<<15) )
#define COMMAND_TYPE_MASK ( (u16) (0x07FF) )

//----------------------------------------------
// packet_header_command
//
typedef struct
{
   u16 command_type;
   u32 command_counter; // command index;
   u8  command_resend_counter; // how many times we tried to send the command; or 255 if not using fast retry logic
   u32 command_param;
   u32 command_param2;
} __attribute__((packed)) t_packet_header_command;


//----------------------------------------------
// packet_header_command_response
//
typedef struct
{
   u16 origin_command_type;
   u32 origin_command_counter;
   u8  origin_command_resend_counter;
   u32 response_counter;
   u8  command_response_flags;
   u32 command_response_param; // to be used if needed to send back a param in the response
} __attribute__((packed)) t_packet_header_command_response;


//----------------------------------------------
// packet_header_ruby_telemetry
//
#define FLAG_RUBY_TELEMETRY_RC_ALIVE    ((u32)1)
#define FLAG_RUBY_TELEMETRY_RC_FAILSAFE ((u32)(((u32)0x01)<<1))
// Unused anymore #define FLAG_RUBY_TELEMETRY_HAS_RC_INFO ((u32)(((u32)0x01)<<2))
#define FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY ((u32)(((u32)0x01)<<3))
#define FLAG_RUBY_TELEMETRY_ENCRYPTED_DATA ((u32)(((u32)0x01)<<4))
#define FLAG_RUBY_TELEMETRY_ENCRYPTED_VIDEO ((u32)(((u32)0x01)<<5))
#define FLAG_RUBY_TELEMETRY_ENCRYPTED_HAS_KEY ((u32)(((u32)0x01)<<6))
#define FLAG_RUBY_TELEMETRY_HAS_RC_RSSI ((u32)(((u32)0x01)<<7))
#define FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI ((u32)(((u32)0x01)<<7))
#define FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RX_RSSI ((u32)(((u32)0x01)<<8))
#define FLAG_RUBY_TELEMETRY_HAS_RELAY_LINK ((u32)(((u32)0x01)<<9))  // true if the link to the relayed vehicle is present and live
#define FLAG_RUBY_TELEMETRY_IS_RELAYING ((u32)(((u32)0x01)<<10))  // true if the vehicle is currently relaying another vehicle
#define FLAG_RUBY_TELEMETRY_HAS_EXTENDED_INFO ((u32)(((u32)0x01)<<11)) // if true, has the extended telemetry info after this telemetry header

// t_packet_header_ruby_telemetry_extended;

typedef struct
{
   u16 flags;    // see above
   u8  version;  // version x.y 4bits each        
   u32 vehicle_id; // to which vehicle this telemetry refers to
   u8  vehicle_type;
   u8  vehicle_name[MAX_VEHICLE_NAME_LENGTH];
   u8  radio_links_count;
   u16 radio_frequencies[MAX_RADIO_INTERFACES]; // lowest 15 bits: frequency. highest bit: 0 - regular link, 1 - relay link

   u32 downlink_tx_video_bitrate;
   u32 downlink_tx_video_all_bitrate;
   u32 downlink_tx_data_bitrate;

   u16 downlink_tx_video_packets_per_sec;
   u16 downlink_tx_data_packets_per_sec;
   u16 downlink_tx_compacted_packets_per_sec;

   u8  temperature;
   u8  cpu_load;
   u16 cpu_mhz;
   u8  throttled;

   u8  downlink_datarates[MAX_RADIO_INTERFACES][2]; // in 0.5 Mb increments: 0...127 regular datarates, 128+: mcs rates; index 0 - video, index 1 - data
   u8  uplink_datarate[MAX_RADIO_INTERFACES]; // in 0.5 Mb increments: 0...127 regular datarates, 128+: mcs rates
   u8  uplink_rssi_dbm[MAX_RADIO_INTERFACES]; // 200 offset. that is: rssi_dbm = 200 + dbm (dbm is negative);
   u8  uplink_link_quality[MAX_RADIO_INTERFACES]; // 0...100
   u8  uplink_rc_rssi;      // 0...100, 255 - not available
   u8  uplink_mavlink_rc_rssi; // 0...100, 255 - not available
   u8  uplink_mavlink_rx_rssi; // 0...100, 255 - not available

   u16 txTimePerSec; // miliseconds
   u16 extraFlags; // 0, not used for now
   u8 extraSize; // Extra info after this header and the next one if present (usually has the packet below: t_packet_header_ruby_telemetry_extended_extra_info_retransmissions )
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_v1;


typedef struct
{
   u16 flags;    // see above
   u8  version;  // version x.y 4bits each        
   u32 vehicle_id; // to which vehicle this telemetry refers to
   u8  vehicle_type;
   u8  vehicle_name[MAX_VEHICLE_NAME_LENGTH];
   u8  radio_links_count;
   u32 uRadioFrequencies[MAX_RADIO_INTERFACES]; // lowest 31 bits: frequency. highest bit: 0 - regular link, 1 - relay link
   u8  uRelayLinks; // each bit tells if radio link N is a relay link
   u32 downlink_tx_video_bitrate;
   u32 downlink_tx_video_all_bitrate;
   u32 downlink_tx_data_bitrate;

   u16 downlink_tx_video_packets_per_sec;
   u16 downlink_tx_data_packets_per_sec;
   u16 downlink_tx_compacted_packets_per_sec;

   u8  temperature;
   u8  cpu_load;
   u16 cpu_mhz;
   u8  throttled;

   u8  downlink_datarates[MAX_RADIO_INTERFACES][2]; // in 0.5 Mb increments: 0...127 regular datarates, 128+: mcs rates; index 0 - video, index 1 - data
   u8  uplink_datarate[MAX_RADIO_INTERFACES]; // in 0.5 Mb increments: 0...127 regular datarates, 128+: mcs rates
   u8  uplink_rssi_dbm[MAX_RADIO_INTERFACES]; // 200 offset. that is: rssi_dbm = 200 + dbm (dbm is negative);
   u8  uplink_link_quality[MAX_RADIO_INTERFACES]; // 0...100
   u8  uplink_rc_rssi;      // 0...100, 255 - not available
   u8  uplink_mavlink_rc_rssi; // 0...100, 255 - not available
   u8  uplink_mavlink_rx_rssi; // 0...100, 255 - not available

   u16 txTimePerSec; // miliseconds
   u16 extraFlags; // 0, not used for now
   u8 extraSize; // Extra info as part of the packet, after headers
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_v2;

#define FLAG_RUBY_TELEMETRY_EXTRA_INFO_IS_VALID ((u32)(((u32)0x01)<<1))

typedef struct
{
   u32 flags;
   u32 uTimeNow; // Vehicle time
   u32 uRelayedVehicleId;
   u16 uThrottleInput; // usually 1000...2000
   u16 uThrottleOutput; // usually 0...100

   u32 uDummy[10];
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_extra_info;

typedef struct
{
   u16 totalReceivedRetransmissionsRequestsUnique;
   u16 totalReceivedRetransmissionsRequestsDuplicate;

   u16 totalReceivedRetransmissionsRequestsSegmentsUnique;
   u16 totalReceivedRetransmissionsRequestsSegmentsDuplicate;
   u16 totalReceivedRetransmissionsRequestsSegmentsRetried;
      
   u16 totalReceivedRetransmissionsRequestsUniqueLast5Sec;
   u16 totalReceivedRetransmissionsRequestsDuplicateLast5Sec;

   u16 totalReceivedRetransmissionsRequestsSegmentsUniqueLast5Sec;
   u16 totalReceivedRetransmissionsRequestsSegmentsDuplicateLast5Sec;
   u16 totalReceivedRetransmissionsRequestsSegmentsRetriedLast5Sec;
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_extra_info_retransmissions;

//----------------------------------------------
// packet_header_fc_telemetry
//
typedef struct
{
   u8 flags;
   u8 fc_telemetry_type; // type of telemetry sent by the FC. The ones defined in models.h
   u8 flight_mode;
   u32 arm_time;
   u8 throttle;
   u16 voltage; // 1/1000 volts
   u16 current; // 1/1000 amps
   u16 voltage2; // 1/1000 volts
   u16 current2; // 1/1000 amps
   u16 mah;
   u32 altitude; // 1/100 meters  -1000 m
   u32 altitude_abs; // 1/100 meters -1000 m
   u32 distance; // 1/100 meters
   u32 total_distance; // 1/100 meters

   u32 vspeed; // 1/100 meters -1000 m
   u32 aspeed; // airspeed (1/100 meters - 1000 m)
   u32 hspeed; // 1/100 meters -1000 m
   u32 roll; // 1/100 degree + 180 degree
   u32 pitch; // 1/100 degree + 180 degree

   u8 satelites;
   u8 gps_fix_type;
   u16 hdop; // 1/100 th
   u16 heading;
   int32_t latitude; // 1/10000000;
   int32_t longitude; // 1/10000000;

   u8 temperature; // temperature - 100 degree celsius (value 100 = 0 celsius)
   u16 fc_kbps; // kbits/s serial link volume from FC to Pi
   u8 rc_rssi;
   u8 extra_info[12];
               // byte 0: not used
               // byte 1: satelites2; // 0xFF if no second GPS
               // byte 2: gps_fix_type2; // 0xFF if no second GPS
               // byte 3,4: hdop2; // 0xFF if no second GPS
               // byte 5: FC telemetry packet index (ascending)
               // byte 6: MAVLink messages per second
               // byte 7-8: wind heading (+1, 0 for N/A)
               // byte 9-10: wind speed (1/100 m) (+1, 0 for N/A)
} __attribute__((packed)) t_packet_header_fc_telemetry;


typedef struct
{
   u16 channels[MAX_MAVLINK_RC_CHANNELS];
} __attribute__((packed)) t_packet_header_fc_rc_channels;


//----------------------------------------------
// packet_header_fc_extra, present in PACKET_TYPE_TELEMETRY_ALL2
//
typedef struct
{
   u8 chunk_index;  // 0 for a complete single message; 1 based index if it's a part of a longer message
   u8 text[FC_MESSAGE_MAX_LENGTH];     // text is zero based terminated, even if part of a longer message.
} __attribute__((packed)) t_packet_header_fc_extra;


//----------------------------------------------
// packet_header_telemetry_raw, present in PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD and PACKET_TYPE_TELEMETRY_RAW_UPLOAD
//
typedef struct
{
   u32 telem_segment_index;
   u32 telem_total_data;
   u32 telem_total_serial;
} __attribute__((packed)) t_packet_header_telemetry_raw;


#define MAX_HISTORY_VEHICLE_TX_STATS_SLICES 40
typedef struct
{
   int iSliceInterval; // in miliseconds
   u8 uCountValues;
   u8 historyTxGapMaxMiliseconds[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u8 historyTxGapMinMiliseconds[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u8 historyTxGapAvgMiliseconds[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u8 historyTxPackets[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u8 historyVideoPacketsGapMax[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u8 historyVideoPacketsGapAvg[MAX_HISTORY_VEHICLE_TX_STATS_SLICES];
   u32 tmp_uAverageTxSum;
   u32 tmp_uAverageTxCount;
   u32 tmp_uVideoIntervalsSum;
   u32 tmp_uVideoIntervalsCount;
} __attribute__((packed)) t_packet_header_vehicle_tx_history;


//----------------------------------------------
// packet_data_controller_links_stats;
// sent by controller in PACKET_TYPE_RUBY_PING_CLOCK and PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS
//

// 5 intervals, 80 ms each
// !!! Should be the same as stats kept by vehicle: VIDEO_LINK_STATS_REFRESH_INTERVAL_MS
#define CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES 5
#define CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL_MS 80

typedef struct
{
   u8 flagsAndVersion;
   u32 lastUpdateTime;
   u8 radio_interfaces_count; // only those present are sent over radio
   u8 video_streams_count; // only those present are sent over radio
   u8 radio_interfaces_rx_quality[MAX_RADIO_INTERFACES][CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES]; // 0...100 %, or 255 for no packets received or lost for this time slice
   u8 radio_streams_rx_quality[MAX_VIDEO_STREAMS][CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES]; // 0...100 %, or 255 for no packets received or lost for this time slice
   u8 video_streams_blocks_clean[MAX_VIDEO_STREAMS][CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES];
   u8 video_streams_blocks_reconstructed[MAX_VIDEO_STREAMS][CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES];
   u8 video_streams_blocks_max_ec_packets_used[MAX_VIDEO_STREAMS][CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES];
   u8 video_streams_requested_retransmission_packets[MAX_VIDEO_STREAMS][CONTROLLER_LINK_STATS_HISTORY_MAX_SLICES];

   u8 tmp_radio_streams_rx_quality[MAX_VIDEO_STREAMS];
   u8 tmp_video_streams_blocks_clean[MAX_VIDEO_STREAMS];
   u8 tmp_video_streams_blocks_reconstructed[MAX_VIDEO_STREAMS];
   u8 tmp_video_streams_blocks_max_ec_packets_used[MAX_VIDEO_STREAMS];
   u8 tmp_video_streams_requested_retransmission_packets[MAX_VIDEO_STREAMS];
} __attribute__((packed)) t_packet_data_controller_link_stats;


#ifdef __cplusplus
extern "C" {
#endif  

void packet_compute_crc(u8* pBuffer, int length);
int packet_check_crc(u8* pBuffer, int length);

void radio_populate_ruby_telemetry_v2_from_ruby_telemetry_v1(t_packet_header_ruby_telemetry_extended_v2* pV2, t_packet_header_ruby_telemetry_extended_v1* pV1);

int radio_convert_short_packet_to_regular_packet(t_packet_header_short* pPHS, u8* pOutPacket, int iMaxLength);
int radio_convert_regular_packet_to_short_packet(t_packet_header* pPH, u8* pOutPacket, int iMaxLength);
int radio_buffer_embed_short_packet_to_packet(t_packet_header_short* pPHS, u8* pOutPacket, int iMaxLength);
int radio_buffer_embed_packet_to_short_packet(t_packet_header* pPH, u8* pOutPacket, int iMaxLength);
int radio_buffer_is_valid_short_packet(u8* pBuffer, int iLength);

#ifdef __cplusplus
}  
#endif

