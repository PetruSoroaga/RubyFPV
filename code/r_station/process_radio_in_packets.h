#pragma once

#include "../base/base.h"

void init_radio_rx_structures();

// Returns 1 if end of a video block was reached
int process_received_single_radio_packet(int iInterfaceIndex, u8* pData, int iDataLength);
