#pragma once
#include "base.h"

// Lower 2 bytes: alarms from vehicle
// Higher 2 bytes: alarms on controller

#define ALARM_ID_UNSUPORTED_USB_SERIAL  ((u32)(((u32)0x01)))
#define ALARM_ID_RECEIVED_INVALID_RADIO_PACKET  ((u32)(((u32)0x01)<<1))
#define ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED  ((u32)(((u32)0x01)<<2))
#define ALARM_ID_LINK_TO_CONTROLLER_LOST  ((u32)(((u32)0x01)<<4))
#define ALARM_ID_LINK_TO_CONTROLLER_RECOVERED  ((u32)(((u32)0x01)<<5))
#define ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD ((u32)(((u32)0x01)<<6)) // param: low word: video total bitrate in kb, high word: current radio datarate in kb
#define ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD ((u32)(((u32)0x01)<<7)) // param: low word: current tx time in ms/sec, threshold for max tx time in ms/sec
#define ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD ((u32)(((u32)0x01)<<8)) // param: low word: loop miliseconds average, highword: loop miliseconds spike
#define ALARM_ID_VEHICLE_RX_TIMEOUT ((u32)(((u32)0x01)<<9))


#define ALARM_ID_CONTROLLER_RECEIVED_INVALID_RADIO_PACKET  ((u32)(((u32)0x01)<<17))
#define ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK  ((u32)(((u32)0x01)<<18))
#define ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD ((u32)(((u32)0x01)<<19)) // param: low word: loop miliseconds average, highword: loop miliseconds spike
#define ALARM_ID_CONTROLLER_RX_TIMEOUT  ((u32)(((u32)0x01)<<20)) // param: which interface timedout
#define ALARM_ID_CONTROLLER_IO_ERROR  ((u32)(((u32)0x01)<<21)) 

#define ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT ((u32)(((u32)0x01)<<1))
#define ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_TRUNCATED ((u32)(((u32)0x01)<<2))
#define ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_WOULD_BLOCK ((u32)(((u32)0x01)<<3))
#define ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT ((u32)(((u32)0x01)<<4))
#define ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_TRUNCATED ((u32)(((u32)0x01)<<5))
#define ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_WOULD_BLOCK ((u32)(((u32)0x01)<<6))

#ifdef __cplusplus
extern "C" {
#endif

void alarms_to_string(unsigned int alarms, char* szOutput);

#ifdef __cplusplus
}  
#endif
