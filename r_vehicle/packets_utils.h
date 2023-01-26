#pragma once
#include "../base/base.h"

int send_packet_to_radio_interfaces(bool bIsVideoPacket, u8* pPacketData, int nPacketLength);
int get_last_tx_used_datarate(int iInterface, int iType);
int get_last_tx_video_datarate_mbps();

void send_packet_vehicle_log(u8* pBuffer, int length);
void send_alarm_to_controller(u32 uAlarms, u32 uFlags, u32 uRepeatCount);
