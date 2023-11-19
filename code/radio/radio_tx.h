#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"


#ifdef __cplusplus
extern "C" {
#endif

int radio_tx_start_tx_thread();
void radio_tx_stop_tx_thread();
void radio_tx_mark_quit();

void radio_tx_pause_radio_interface(int iRadioInterfaceIndex);
void radio_tx_resume_radio_interface(int iRadioInterfaceIndex);
void radio_tx_set_sik_packet_size(int iSiKPacketSize);

// Sends a regular radio packet to SiK radios.
// Returns 1 for success.
int radio_tx_send_sik_packet(int iRadioInterfaceIndex, u8* pData, int iDataLength);

#ifdef __cplusplus
}  
#endif
