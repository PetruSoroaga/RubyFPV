#pragma once

void init_radio_in_packets_state();
void process_received_radio_packets();
int try_receive_radio_packets(u32 uMaxMicroSeconds);
