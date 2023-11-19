#pragma once
#include "../base/base.h"

int compute_packet_uplink_datarate(int iVehicleRadioLink, int iRadioInterface);

int send_packet_to_radio_interfaces(u8* pPacketData, int nPacketLength);

int get_controller_radio_link_stats_size();
void add_controller_radio_link_stats_to_buffer(u8* pDestBuffer);

int get_controller_radio_interface_index_for_radio_link(int iLocalRadioLinkId);
