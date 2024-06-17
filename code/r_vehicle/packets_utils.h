#pragma once
#include "../base/base.h"

int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength, int iSendToSingleRadioLink);
int get_last_tx_used_datarate(int iInterface, int iType);
int get_last_tx_video_datarate_mbps();

void send_packet_vehicle_log(u8* pBuffer, int length);

void send_alarm_to_controller(u32 uAlarm, u32 uFlags1, u32 uFlags2, u32 uRepeatCount);
void send_pending_alarms_to_controller();
