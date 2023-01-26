#pragma once
#include "../base/base.h"

int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength);

int get_controller_radio_link_stats_size();
void add_controller_radio_link_stats_to_buffer(u8* pDestBuffer);
