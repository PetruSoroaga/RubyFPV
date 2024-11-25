#pragma once
#include "../base/base.h"

void packet_utils_init();
void packet_utils_set_adaptive_video_datarate(int iDatarateBPS);
int packet_utils_get_last_set_adaptive_video_datarate();

int get_last_tx_used_datarate_bps_video(int iInterface);
int get_last_tx_used_datarate_bps_data(int iInterface);
int get_last_tx_minimum_video_radio_datarate_bps();

int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength, int iSendToSingleRadioLink);
void send_packet_vehicle_log(u8* pBuffer, int length);

void send_alarm_to_controller(u32 uAlarm, u32 uFlags1, u32 uFlags2, u32 uRepeatCount);
void send_alarm_to_controller_now(u32 uAlarm, u32 uFlags1, u32 uFlags2, u32 uRepeatCount);
void send_pending_alarms_to_controller();
