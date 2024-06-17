/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#pragma once
#include "../base/base.h"
#include "../base/hardware.h"
#include "../base/shared_mem.h"
#include "radioflags.h"
#include "radiotap.h"
#include "radiopackets2.h"
#include <time.h>
#include <sys/resource.h>

#define MAX_PACKET_LENGTH_PCAP 2048

#define RADIO_PROCESSING_ERROR_NO_ERROR 0x00
#define RADIO_PROCESSING_ERROR_CODE_INVALID_CRC_RECEIVED 0x01
#define RADIO_PROCESSING_ERROR_CODE_PACKET_RECEIVED_TOO_SMALL 0x02
#define RADIO_PROCESSING_ERROR_INVALID_PARAMETERS 0x0E
#define RADIO_PROCESSING_ERROR_INVALID_RECEIVED_PACKET 0x0F

#define RADIO_READ_ERROR_NO_ERROR 0
#define RADIO_READ_ERROR_TIMEDOUT 1
#define RADIO_READ_ERROR_INTERFACE_BROKEN 2
#define RADIO_READ_ERROR_READ_ERROR 3


#ifdef __cplusplus
extern "C" {
#endif  

void radio_init_link_structures();
void radio_link_cleanup();
void radio_enable_crc_gen(int enable);
void radio_set_debug_flag();
void radio_set_link_clock_delta(int iVehicleBehindMilisec);
int  radio_get_link_clock_delta();
void radio_set_use_pcap_for_tx(int iEnablePCAPTx);
int radio_set_out_datarate(int rate_bps); // positive: classic in bps, negative: MCS; returns 1 if it was changed
void radio_set_frames_flags(u32 frameFlags); // frame type, MSC Flags
u32 radio_get_received_frames_type();

void radio_reset_packets_default_frequencies(int iRCEnabled);
int radio_can_send_packet_on_slow_link(int iLinkId, int iPacketType, int iFromController, u32 uTimeNow);
int radio_get_last_second_sent_bps_for_radio_interface(int iInterfaceIndex, u32 uTimeNow);
int radio_get_last_500ms_sent_bps_for_radio_interface(int iInterfaceIndex, u32 uTimeNow);

// Tells caller if a radio interface is broken (maybe unplugged)
int radio_interfaces_broken();

int radio_open_interface_for_read(int interfaceIndex, int portNumber);
int radio_open_interface_for_write(int interfaceIndex);
void radio_close_interfaces_for_read();
void radio_close_interface_for_read(int interfaceIndex);
void radio_close_interface_for_write(int interfaceIndex);

u8* radio_process_wlan_data_in(int interfaceNumber, int* outPacketLength);
int radio_get_last_read_error_code();

// returns 0 for failure, total length of packet for success
int packet_process_and_check(int interfaceNb, u8* pPacketBuffer, int iBufferLength, int* pbCRCOk);
int get_last_processing_error_code();

u32 radio_get_next_radio_link_packet_index(int iLocalRadioLinkId);
int radio_build_new_raw_packet(int iLocalRadioLinkId, u8* pRawPacket, u8* pPacketData, int nInputLength, int portNb, int bEncrypt, int iExtraData, u8* pExtraData);
int radio_write_raw_packet(int interfaceIndex, u8* pData, int dataLength);
int radio_write_serial_packet(int interfaceIndex, u8* pData, int dataLength, u32 uTimeNow);
int radio_write_sik_packet(int interfaceIndex, u8* pData, int dataLength, u32 uTimeNow);

#ifdef __cplusplus
}  
#endif
