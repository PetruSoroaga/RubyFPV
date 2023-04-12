#pragma once
#include "../base/base.h"
#include "shared_vars.h"

void relay_set_rx_info_stats(type_uplink_rx_info_stats* pUplinkStats);
void relay_process_received_radio_packet_from_relayed_vehicle(int iRadioLink, int iRadioInterfaceIndex, u8* pBufferData, int iBufferLength);
void relay_process_received_single_radio_packet_from_controller_to_relayed_vehicle(int iRadioInterfaceIndex, u8* pBufferData, int iBufferLength);

void relay_on_relay_params_changed();
void relay_on_relay_mode_changed(u8 uNewMode);
void relay_on_relay_flags_changed(u32 uNewFlags);

