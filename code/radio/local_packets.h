#pragma once

//---------------------------------------
// COMPONENT LOCAL CONTROL PACKETS
// Packets sent only locally on the same machine
// 0...128 packets over radio
// 150-200: controller local controll packets
// 200-250: vehicle local controll packets

#define PACKET_TYPE_LOCAL_CONTROL_PAUSE_VIDEO 151
#define PACKET_TYPE_LOCAL_CONTROL_RESUME_VIDEO 152
#define PACKET_TYPE_LOCAL_CONTROL_UPDATE_VIDEO_PROGRAM 153
#define PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED 154  // vehicle_id_src is the component id that triggered the change (first byte) and the type of change (second byte)
#define PACKET_TYPE_LOCAL_CONTROL_PAUSE_RESUME_AUDIO 155 // vehicle_id_dest: 1: pause, 0: resume
#define PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED 157  // vehicle_id_src is the component id that triggered the change
#define PACKET_TYPE_LOCAL_CONTROL_START_VIDEO_PROGRAM 158
#define PACKET_TYPE_LOCAL_CONTROL_REBOOT 160  // vehicle_id_src is the component id that triggered the change
#define PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED 161
#define PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED 162
#define PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED 163
#define PACKET_TYPE_LOCAL_CONTROL_UPDATED_VIDEO_LINK_OVERWRITES 170 // sent by vehicle to other components locally to tell them the updated video link overwrites (a shared_mem_video_link_overwrites structure after header)

#define PACKET_TYPE_LOCAL_CONTROL_RELAY_MODE_SWITCHED 171 // vehicle_id_src is the new relay mode

#define PACKET_TYPE_LOCAL_CONTROL_SWITCH_RADIO_LINK 172 // request: vehicle_id_dest has the vehicle radio link id to switch to; response: vehicle_id_src has the radio link id switched to, vehicle_id_dest has the result code

#define PACKET_TYPE_LOCAL_CONTROL_SWITCH_FAVORIVE_VEHICLE 173 // vehicle_id_dest or src is the vehicle id to switch to or switched to

#define PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED 175
#define PACKET_TYPE_LOCAL_CONTROL_RECEIVED_MODEL_SETTING 176
#define PACKET_TYPE_LOCAL_CONTROL_REINITIALIZE_RADIO_LINKS 177
#define PACKET_TYPE_LOCAL_CONTROL_UPDATED_RADIO_TX_POWERS 178
#define PACKET_TYPE_LOCAL_CONTROL_RECEIVED_VEHICLE_LOG_SEGMENT 180 // Same as a PACKET_TYPE_RUBY_LOG_FILE_SEGMENT
#define PACKET_TYPE_LOCAL_CONTROL_PAUSE_LOCAL_VIDEO_DISPLAY 181 // vehicle_id_dest or src: 0: resume, 1: pause
#define PACKET_TYPE_LOCAL_CONTROL_PASSPHRASE_CHANGED 184
#define PACKET_TYPE_LOCAL_CONTROLL_VIDEO_DETECTED_ON_SEARCH 185 // vehicle id src is search freq in Khz

#define PACKET_TYPE_LOCAL_CONTROL_I2C_DEVICE_CHANGED 190
#define PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY 191
#define PACKET_TYPE_LOCAL_CONTROLLER_RADIO_INTERFACE_FAILED_TO_INITIALIZE 192 // vehicle_id_dest: radio interface index
#define PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS 193 // sent to controller router
#define PACKET_TYPE_LOCAL_CONTROL_FORCE_VIDEO_PROFILE 194 // vehicle_id_dest contains the new video profile to force and keep, or 0xFF to reset it


#define PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_CAMERA_PARAMS 201
// u8 camera index
// 1..N camera  params (type_camera_parameters struct)

#define PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS 205 // contains a type_vehicle_stats_info structure

#define PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED 206 // used when changed the search frequency on the controller // vehicle_dest_src is the new frequency to search on
#define PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED 207 // u32: link id, u32: new freq

#define PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE 208 // vehicle_id_dest: timeout to pause adaptive video (int milisec) or zero to resume

#define PACKET_TYPE_LOCAL_CONTROL_VEHICLE_VIDEO_PROFILE_SWITCHED 216 // vehicle_id_dest contains the new video profile used right now
#define PACKET_TYPE_LOCAL_CONTROL_VEHICLE_ROUTER_READY 220
#define PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_SIK_RADIO_SERIAL_SPEED 221 // vehicle_id_src is the index of the radio interface, vehicle_id_dest is the baudrate to use to connect to radio (old baud rate)
#define PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS 222

#define PACKET_TYPE_APPLY_SIK_PARAMS 230
