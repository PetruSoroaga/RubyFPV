#pragma once

#include "../base/base.h"

void init_radio_rx_structures();

// Returns 1 if end of a video block was reached
int process_received_radio_packets();
int try_receive_radio_packets(u32 uMaxMicroSeconds);
