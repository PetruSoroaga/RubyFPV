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

#if defined (HW_PLATFORM_RASPBERRY) || defined (HW_PLATFORM_RADXA_ZERO3)
#define MAX_RXTX_BLOCKS_BUFFER 200
#define MAX_TOTAL_PACKETS_IN_BLOCK 64
#define MAX_DATA_PACKETS_IN_BLOCK 32
#define MAX_FECS_PACKETS_IN_BLOCK 32
#else
#define MAX_RXTX_BLOCKS_BUFFER 80
#define MAX_TOTAL_PACKETS_IN_BLOCK 32
#define MAX_DATA_PACKETS_IN_BLOCK 16
#define MAX_FECS_PACKETS_IN_BLOCK 16
#endif

#define MAX_PACKET_RADIO_HEADERS 100
#define MAX_PACKET_PAYLOAD 1250
#define MAX_PACKET_TOTAL_SIZE 1500

#define RADIO_PORT_ROUTER_UPLINK 0x0E     // from controller to vehicle
#define RADIO_PORT_ROUTER_DOWNLINK 0x0F   // from vehicle to controller

#define MAX_RADIO_STREAMS 8
#define MAX_VIDEO_STREAMS 4
// (must be less than max radio streams - id of first video stream value below)

#define STREAM_ID_DATA    ((u32)0)
#define STREAM_ID_TELEMETRY ((u32)1)
#define STREAM_ID_DATA2    ((u32)2)
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
// Deprecated in 9.7 (inclusive)
// #define _FLAGS_BIT_EXTRA_DATA       ((u8)(1<<5))
#define PACKET_FLAGS_BIT_HAS_ENCRYPTION   ((u8)(1<<6))
#define PACKET_FLAGS_BIT_CAN_START_TX     ((u8)(1<<7))

#define PACKET_FLAGS_EXTENDED_BIT_SEND_ON_HIGH_CAPACITY_LINK_ONLY  (((u16)1)<<8)
#define PACKET_FLAGS_EXTENDED_BIT_SEND_ON_LOW_CAPACITY_LINK_ONLY  (((u16)1)<<9)
#define PACKET_FLAGS_EXTENDED_BIT_REQUIRE_ACK  (((u16)1)<<10)

// Max 8 components, for the first 3 bits of packet_flags field
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

//----------------------------------------
// All packets start with this basic header and then a packet specific header
// Different packets have different headers, but all headers are preceided by this basic header
// Packet: [header] + [specific header] + [payload_data];
//         | <-total headers length-> |                    // less the start CRC
//         |              <- total length ->           |   // less the start CRC
//
// radio_link_packet_index is used only to detect missing rx packets on a radio link
// vehicle_id_src and stream_packet_idx are used to detect rx packets duplication (same packet received on multiple radio links)

typedef struct
{
   u32 uCRC; // computed for the entire packet or header only. start point is after this crc.
            // Highest byte is set to 0x00 to be able to detect and distinguish radio packets from other systems. (starting in version 8.0)
   u8 packet_flags;
   u8 packet_type; // 1...150: components packets types, 150-200: local control controller packets, 200-250: local control vehicle packets
   u32 stream_packet_idx; // high 4 bits: stream id (0..15), lower 28 bits: stream packet index
                          // monotonically increassing, to detect missing/lost packets on each stream

   u16 packet_flags_extended;  // Added in 7.4: it replaced (length of all headers)
             // byte 0: version: higher 4 bits: major version, lower 4 bits: minor version
             // byte 1:
             //    bit 0: 1: send on high capacity links only;
             //    bit 1: 1: sent on low capacity links only;
             //    bit 2: 1: requires ACK for this packet
   u16 total_length; // Total length, including all the header data, including CRC
   u16 radio_link_packet_index; // Introduced in 7.7: monotonically increasing for each radio packet sent on a radio link
                                // used to detect missing packets on receive side on a radio link, not for duplicate detection
   u32 vehicle_id_src;
   u32 vehicle_id_dest;
} __attribute__((packed)) t_packet_header;


// Deprecated in 9.8
/*
#define PACKET_TYPE_VIDEO_DATA_FULL 2

//----------------------------------------------
// packet_header_video_full
//

typedef struct
{
   u8 video_link_profile;
      // (video stream id: 0xF0 bits, current video link profile: 0x0F bits); added in v.5.6
   u8 video_stream_and_type; // bits 0...3: video stream index, bits 4...7: video stream type: H264, H265, IP, etc
   u32 uProfileEncodingFlags; // same as video link profile's uProfileEncodingFlags;
      // byte 0:
      //    bit 0..2  - scramble blocks count
      //    bit 3     - enables restransmission of missing packets
      //    bit 4     - enable adaptive video keyframe interval
      //    bit 5     - enable adaptive video link params
      //    bit 6     - use controller info too when adjusting video link params
      //    bit 7     - go lower adaptive video profile when controller link lost

      // byte 1:   - max time to wait for retransmissions (in ms*5)// affects rx buffers size
      // byte 2:   - retransmission duplication percent (0-100%), 0xFF = auto, bit 0..3 - regular packets duplication, bit 4..7 - retransmitted packets duplication
      // byte 3:
      //    bit 0  - use medium adaptive video
      //    bit 1  - enable video auto quantization
      //    bit 2  - video auto quantization strength
      //    bit 3  - one way video link
      //    bit 4  - video profile should use EC scheme as auto;
      //    bit 5,6 - EC scheme spreading factor (0...3)
      //    bit 7  - try to keep constant video bitrate when it fluctuates

   u32 uVideoStatusFlags2;
      // byte 0: current h264 quantization value
      // byte 1:
      //    bit 0  - 0/1: has debug timings info after the video data:
      //                  u32 - delta ms between video packets
      //                  u32 - local timestamp camera capture,
      //                  u32 - local timestamp sent to radio processing;
      //                  u32 - local timestamp sent to radio output;
      //                  u32 - local timestamp received on radio;
      //                  u32 - local timestamp sent to video processing;
      //                  u32 - local timestamp sent to video output;
      //    bit 1  - 0/1: is this video packet part of a I-frame
      //    bit 2  - 1: is on lower video bitrate

   u16 video_width;
   u16 video_height;
   u8  video_fps;
   u16 video_keyframe_interval_ms;
   u8  block_packets;
   u8  block_fecs;

   u16 video_data_length;
   u32 video_block_index;
   u8  video_block_packet_index;
   u32 fec_time; // how long FEC took, in microseconds/second
   u32 uLastRecvVideoRetransmissionId; // unique id of the last retransmission request received by vehicle
   u16 uLastAckKeyframeInterval; // in milisec
   u8  uLastAckLevelShift;
   u32 uLastSetVideoBitrate; // in bps, highest bit: 1 - initial set, 0 - auto adjusted
   u32 uExtraData; // not used, for future
} __attribute__((packed)) t_packet_header_video_full_77;
*/

#define PACKET_TYPE_VIDEO_DATA_98 22

#define VIDEO_STREAM_INFO_FLAG_NONE 0
#define VIDEO_STREAM_INFO_FLAG_SIZE 1
#define VIDEO_STREAM_INFO_FLAG_FPS 2
#define VIDEO_STREAM_INFO_FLAG_FEC_TIME 3
#define VIDEO_STREAM_INFO_FLAG_VIDEO_PROFILE_FLAGS 4
#define VIDEO_STREAM_INFO_FLAG_RETRANSMISSION_ID 5

typedef struct
{
   u8 uVideoStreamIndexAndType; // bits 0...3: video stream index, bits 4...7: video stream type: H264, H265, IP, etc
   u32 uVideoStatusFlags2;
      // byte 0: current h264 quantization value
      // byte 1:
      //    bit 0  - 0/1: has debug timings info after the video data:
      //                  u32 - delta ms between video packets
      //                  u32 - local timestamp camera capture,
      //                  u32 - local timestamp sent to radio processing;
      //                  u32 - local timestamp sent to radio output;
      //                  u32 - local timestamp received on radio;
      //                  u32 - local timestamp sent to video processing;
      //                  u32 - local timestamp sent to video output;
      //    bit 1  - 0/1: is this video packet part of a I-frame
      //    bit 2  - 1: is on lower video bitrate
      //    bit 3  - 0/1: is end of current frame

   u8 uCurrentVideoLinkProfile;
  
   u8 uStreamInfoFlags;
   // See enum above
   //  0: none;
   //  1: video width (low 16 bits) and video height (high 16 bits)
   //  2: video fps
   //  3: fec time: how long FEC took, in microseconds/second
   //  4: contains uProfileEncodingFlags; same as video link profile's uProfileEncodingFlags;
   //  5: retransmission id
   
   u32 uStreamInfo; // value dependent on uStreamInfoFlags;

   u16 uCurrentVideoKeyframeIntervalMs;

   u32 uCurrentBlockIndex;
   u8  uCurrentBlockPacketIndex;
   u16 uCurrentBlockPacketSize;
   u8  uCurrentBlockDataPackets;
   u8  uCurrentBlockECPackets;

   u32 uLastRecvVideoRetransmissionId; // unique id of the last retransmission request received by vehicle
   u16 uLastAckKeyframeInterval; // in milisec
   u8  uLastAckLevelShift;
   u32 uLastSetVideoBitrate; // in bps, highest bit: 1 - initial set, 0 - auto adjusted
   u32 uExtraData; // not used, for future
   // After video header, first two bites are the size of actual video data, part of error reconstruction as video data
} __attribute__((packed)) t_packet_header_video_full_98;

typedef struct
{
   u32 uTime1;
   u32 uTime2;
   u32 uTime3;
   u32 uTime4;
   u32 uTime5;
   u32 uTime6;
   u32 uVideoCRC;
} __attribute__((packed)) t_packet_header_video_full_98_debug_info;

// PING packets do not increase stream packet index as they are sent on each radio link separatelly
#define PACKET_TYPE_RUBY_PING_CLOCK 3
// params after header:
//   u8 ping id
//   u8 controller local radio link id
//   u8 relay flags for the receiving side (for who receives this ping) // added in v.7.7
//   u8 relay mode for the receiving side (for who receives this ping) // added in v.7.7
//   optional: serialized minimized t_packet_data_controller_link_stats - link stats (video and radio)


#define PACKET_TYPE_RUBY_PING_CLOCK_REPLY 4
// has:
//  u8 original ping id after header
//  u32 local time in miliseconds
//  u8 sender original local radio link id
//  u8 reply local radio link id (vehicle local radio link)

#define PACKET_TYPE_RUBY_RADIO_REINITIALIZED 5 // Sent by vehicle when a radio interface crashed on the vehicle.

#define PACKET_TYPE_RUBY_MODEL_SETTINGS 6 // Sent by vehicle after boot or when requested all model settings as zip from controller. Sends all model settings (compressed) to controller;
// Updated in v7.7
// Params:
// u32 startflags 0xFFFFFFFF
//                0xFFFFFFF0 added in 8.3 for tar+gzip format file
// u32 uUniqueSendCounter;
// u8 uSendAsSmallSegments: 0 - big segments / 1 - small segments

// Data itself, after params, can have one of two type of responses:
//   * the zip model settings, if single packet (more than 150 bytes)
//   * segments of the zip model settings, if multiple small segments response is received (smaller than 150 bytes, for slow links)
//     each segment has:
//        1 byte - segment index
//        1 byte - total segments
//        1 byte - segment size
//        N bytes - segment data

#define PACKET_TYPE_RUBY_PAIRING_REQUEST 7 // Sent by controller when it has link with vehicle for first time. So that vehicle has controller id. Has an optional u32 param after header: count of retires;
#define PACKET_TYPE_RUBY_PAIRING_CONFIRMATION 8 // Sent by vehicle to controller. Has an optional u32 param after header: count of received pairing requests;

#define PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED 9 // Sent by vehicle to controller to let it know about the current radio config.
                                           // Contains a type_relay_parameters, type_radio_interfaces_parameters and a type_radio_links_parameters


//---------------------------------------
// COMPONENT COMMANDS PACKETS

#define PACKET_TYPE_COMMAND 11
#define PACKET_TYPE_COMMAND_RESPONSE 12 // Command response is send back on the telemetry port


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


#define PACKET_TYPE_RUBY_LOG_FILE_SEGMENT 13 // from vehicle to controller, contains a file segment header too and then file data segment

#define PACKET_TYPE_RUBY_ALARM 15
// Contains 4 u32: alarm index (u32), alarm id(s) (u32), flags1 (u32) and flags2 (u32)

typedef struct
{
   u32 file_id; // what file is this (live log, configuration files, etc)
   u32 segment_index;
   u32 segment_size; // size in bytes of the segment
   u32 total_segments; // total segments in file
} __attribute__((packed)) t_packet_header_file_segment;

#define PACKET_TYPE_FIRST_PAIRING_DONE 16

#define PACKET_TYPE_AUDIO_SEGMENT 18

// 4 + N bytes
// bytes 0..3 BBBP  (B block, higher 3 bytes, P packet index, lower byte)
// byte 4-N - audio data

#define PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS 20
// params after header:
//   u32: retransmission request id
//   u8: video stream index
//   u8: number of video packets requested
//   (u32+u8)*n = each (video block index + video packet index) requested 

//---------------------------------------
// COMPONENT RC PACKETS

#define PACKET_TYPE_RC_FULL_FRAME     25   // RC Info sent from ground to vehicle
#define PACKET_TYPE_RC_DOWNLOAD_INFO  26   // RC Info sent back from vehicle to ground station

#define PACKET_TYPE_EVENT 27
// params: u32 event type
//         u32 event extra info
#define EVENT_TYPE_RELAY_MODE_CHANGED 1

#define PACKET_TYPE_DEBUG_INFO 28
// has:
// * type_u32_couters structure for vehicle router main loop info
// * type_radio_tx_timers structure for vehicle total radio tx times history

//---------------------------------------
// COMPONENT TELEMETRY PACKETS

#define PACKET_TYPE_RUBY_TELEMETRY_SHORT 29   // Ruby telemetry, small version, contains t_packet_header and t_packet_ruby_telemetry_short

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
// To fix
#define PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS 35 // has a shared_mem_video_link_stats_and_overwrites structure as data
#define PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_GRAPHS 36 // has a shared_mem_video_link_graphs structure as data

#define PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY 37 // has a t_packet_header_vehicle_tx_gap_history structure
#define PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS 38 // has 1 byte (count cards) then (count cards * shared_mem_radio_stats_radio_interface) after the header
#define PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY 39 // has a shared_mem_dev_video_bitrate_history structure


#define PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD 41 // download telemetry data packet from vehicle to controller
// payload is a packet_header_telemetry_raw structure and then is the actual telemetry data
#define PACKET_TYPE_TELEMETRY_RAW_UPLOAD 42  // upload telemetry data packet from controller to vehicle
// payload is a packet_header_telemetry_raw structure and then is the actual telemetry data


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
#define FLAG_RUBY_TELEMETRY_VEHICLE_HAS_CAMERA ((u32)(((u32)0x01)<<12)) // if true, vehicle has at least one camera
#define FLAG_RUBY_TELEMETRY_HAS_VEHICLE_TELEMETRY_DATA ((u32)(((u32)0x01)<<13)) // if the FC sends any data to Ruby serial port


typedef struct
{
   u16 uFlags;    // see above
   u8  version;  // version x.y 4bits each        
   u8  radio_links_count;
   u32 uRadioFrequenciesKhz[3]; // lowest 31 bits: frequency. highest bit: 0 - regular link, 1 - relay link
   
   u8 flight_mode;
   u8 throttle;
   u16 voltage; // 1/1000 volts
   u16 current; // 1/1000 amps
   u32 altitude; // 1/100 meters  -1000 m
   u32 altitude_abs; // 1/100 meters -1000 m
   u32 distance; // 1/100 meters
   u16 heading;
   
   u32 vspeed; // 1/100 meters -1000 m
   u32 aspeed; // airspeed (1/100 meters - 1000 m)
   u32 hspeed; // 1/100 meters -1000 m
} __attribute__((packed)) t_packet_header_ruby_telemetry_short;


typedef struct
{
   u16 flags;    // see above
   u8  version;  // version x.y 4bits each        
   u32 uVehicleId; // to which vehicle this telemetry refers to
   u8  vehicle_type;
         // semantic changed in version 8.0
         // bit 0...4 - vehicle type: car, drone, plane, etc
         // bit 5..7 - firmware type: Ruby, OpenIPC, etc

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
   u32 uVehicleId; // to which vehicle this telemetry refers to
   u8  vehicle_type;
         // semantic changed in version 8.0
         // bit 0...4 - vehicle type: car, drone, plane, etc
         // bit 5..7 - firmware type: Ruby, OpenIPC, etc
   u8  vehicle_name[MAX_VEHICLE_NAME_LENGTH];
   u8  radio_links_count;
   u32 uRadioFrequenciesKhz[MAX_RADIO_INTERFACES]; // lowest 31 bits: frequency. highest bit: 0 - regular link, 1 - relay link
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
   u8 extraSize; // Extra info as part of the packet, after headers, can be retransmission info
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_v2;


typedef struct // introduced in version 7.4
{
   u16 flags;    // see above
   u8  version;  // version x.y 4bits each        
   u32 uVehicleId; // to which vehicle this telemetry refers to
   u8  vehicle_type;
         // semantic changed in version 8.0
         // bit 0...4 - vehicle type: car, drone, plane, etc
         // bit 5..7 - firmware type: Ruby, OpenIPC, etc
   u8  vehicle_name[MAX_VEHICLE_NAME_LENGTH];
   u8  radio_links_count;
   u32 uRadioFrequenciesKhz[MAX_RADIO_INTERFACES]; // lowest 31 bits: frequency. highest bit: 0 - regular link, 1 - relay link
   u8  uRelayLinks; // each bit tells if radio link N is a relay link
   u32 downlink_tx_video_bitrate_bps; // The transmitted video bitrate by vehicle
   u32 downlink_tx_video_all_bitrate_bps; // Total transmitted video bitrate (+EC + headers) by vehicle
   u32 downlink_tx_data_bitrate_bps;

   u16 downlink_tx_video_packets_per_sec;
   u16 downlink_tx_data_packets_per_sec;
   u16 downlink_tx_compacted_packets_per_sec;

   u8  temperature;
   u8  cpu_load;
   u16 cpu_mhz;
   u8  throttled;

   int last_sent_datarate_bps[MAX_RADIO_INTERFACES][2]; // in bps, positive, negative: mcs rates; 0: never, index 0 - video, index 1 - data
   int last_recv_datarate_bps[MAX_RADIO_INTERFACES]; // in bps, positive, negative: mcs rates, 0: never
   u8  uplink_rssi_dbm[MAX_RADIO_INTERFACES]; // 200 offset. that is: rssi_dbm = 200 + dbm (dbm is negative);
   u8  uplink_link_quality[MAX_RADIO_INTERFACES]; // 0...100
   u8  uplink_rc_rssi;      // 0...100, 255 - not available
   u8  uplink_mavlink_rc_rssi; // 0...100, 255 - not available
   u8  uplink_mavlink_rx_rssi; // 0...100, 255 - not available

   u16 txTimePerSec; // miliseconds
   u16 extraFlags; // bits 0..3 : structure version (0 for now, first one, starting at v3)
   u8 extraSize; // Extra info as part of the packet, after headers, can be retransmission info
} __attribute__((packed)) t_packet_header_ruby_telemetry_extended_v3;


// Flags for structure t_packet_header_ruby_telemetry_extended_extra_info 
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
   // Changed in v7.5 to 2 8 bit values
   //u16 fc_kbps; // kbits/s serial link volume from FC to Pi
   u8 fc_hudmsgpersec; // low 4 bits: heartbeat messages/sec; high 4 bits: system messages/sec; 
   u8 fc_kbps;
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


#define PACKET_TYPE_TELEMETRY_MSP 43

#define MSP_FLAGS_FC_TYPE_MASK ((u32)0x07)
#define MSP_FLAGS_FC_TYPE_BETAFLIGHT 1
#define MSP_FLAGS_FC_TYPE_INAV 2
#define MSP_FLAGS_FC_TYPE_ARDUPILOT 3

typedef struct
{
   u32 uFlags;
   // bit 0,1,2: FC type: 0 BF, 1 INAV
   u8 uRows;
   u8 uCols;
   u32 uDummy;
} __attribute__((packed)) t_packet_header_telemetry_msp;


#define PACKET_TYPE_AUX_DATA_LINK_UPLOAD 45  // upload data link packet from controller to vehicle
// payload is a data link segment index and data
#define PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD 46  // upload data link packet from controller to vehicle
// payload is a data link segment index and data

#define PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS 47 // has a shared_mem_video_info_stats structure

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

#define PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY 48
// has:
// u32 - interface index;
// shared_mem_radio_stats_interface_rx_hist structure


#define PACKET_TYPE_VEHICLE_RECORDING 50
// Has extra info 8 bytes
/*
byte 0: command type:
         1 - start audio recording
         2 - stop audio recording
         3 - start video recording
         4 - stop video recording
         5 - start AV recording
         6 - stop AV recording
         10 - notif: video recording started
         11 - notif: video recording stopped
         7 - notif: video recording restarted due to resolution change
*/

#define PACKET_TYPE_TEST_RADIO_LINK 51
/*
byte 0: protocol version (1)
byte 1: header size (5 bytes)
byte 2: vehicle (model) radio link id to test
byte 3: test number; // added in 8.3
byte 4: command type:
   0 - status message;
         byte 5: length of string including end byte;
         byte 6+: has zero terminated string after that;
   1 - start test link: (from controller to vehicle and vicevera for confirmation)
         byte 5+: radio_link_params structure;
   2 - ping uplink(from controller to vehicle and viceversa)
         byte 5..8:  u32: pings count sent by sender of the message;
         byte 9..12: u32: pings count received so far by the sender of the message from the other side;
   3 - ping downlink(from controller to vehicle and viceversa)
         byte 5..8:  u32: pings count sent by sender of the message;
         byte 9..12: u32: pings count received so far by the sender of the message from the other side;
   4 - end test link (from controller to vehicle and viceversa)
         byte 5: success (0/1)
   5 - test ended (from controller to central)
         byte 5: success (0/1)
*/
#define PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE 5
#define PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION 1
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_STATUS 0
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START  1
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_UPLINK   2
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING_DOWNLINK 3
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END    4
#define PACKET_TYPE_TEST_RADIO_LINK_COMMAND_ENDED  5

#define PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL 60
// From controller to vehicle. Contains:
// u32 - request id, monotonically increasing
// u8 - adaptive video level to switch to (video profile): HQ,MQ,LQ etc
// u8 - video stream index

#define PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK 61
// From vehicle to controller. Contains:
// u32 - request id
// u8 - adaptive video level switched to (video profile): HQ,MQ,LQ etc, or 0xFF if not changed

#define PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE 64 // From controller to vehicle. Contains the deisred keyframe milisec value as an u32 and then u8 video stream index
#define PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK 65 // From vehicle to controller. Contains the acknowledge keyframe milisec value as an u32

#define PACKET_TYPE_SIK_CONFIG 70
// Can be send locally or to vehicle
// u8: vehicle radio link id
// u8: command id:
//        0 - get SiK config
//        1 - pause SiK interface
//        2 - resume SiK interface
// u8+: data response

#define PACKET_TYPE_OTA_UPDATE_STATUS 75
// From vehicle to controller, during OTA update
// u8 status
// u32 counter
#define OTA_UPDATE_STATUS_START_PROCESSING 1
#define OTA_UPDATE_STATUS_UNPACK 2
#define OTA_UPDATE_STATUS_UPDATING 3
#define OTA_UPDATE_STATUS_POST_UPDATING 4
#define OTA_UPDATE_STATUS_COMPLETED 5


#define PACKET_TYPE_DEBUG_VEHICLE_RT_INFO 110
// contains a vehicle_runtime_info structure

#ifdef __cplusplus
extern "C" {
#endif  

void radio_packet_init(t_packet_header* pPH, u8 component, u8 packet_type, u32 uStreamId);
void radio_packet_compute_crc(u8* pBuffer, int length);
int radio_packet_check_crc(u8* pBuffer, int length);

int radio_packet_type_is_high_priority(u8 uPacketType);

void radio_populate_ruby_telemetry_v3_from_ruby_telemetry_v1(t_packet_header_ruby_telemetry_extended_v3* pV3, t_packet_header_ruby_telemetry_extended_v1* pV1);
void radio_populate_ruby_telemetry_v3_from_ruby_telemetry_v2(t_packet_header_ruby_telemetry_extended_v3* pV3, t_packet_header_ruby_telemetry_extended_v2* pV2);

#ifdef __cplusplus
}  
#endif

