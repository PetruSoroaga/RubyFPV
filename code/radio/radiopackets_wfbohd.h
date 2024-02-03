#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <utime.h>
#include <time.h>
#include <sodium.h>
#include "zfec.h"

// Formats for radio packets from other FPV systems

#define DEFAULT_WFBOHD_CHANNEL_ID 7669206
#define DEFAULT_WFBOHD_PORD_ID_VIDEO 0
#define DEFAULT_WFBOHD_PORD_ID_TELEMETRY 16

#define PACKET_TYPE_WFB_DATA 0x01
#define PACKET_TYPE_WFB_KEY  0x02 

typedef struct
{
   uint8_t uPacketType;
   uint8_t sessionNonce[crypto_box_NONCEBYTES];
}  __attribute__ ((packed)) type_wfb_session_header; 

typedef struct
{
   uint64_t uTime;
   uint32_t uChannelId;
   uint8_t uECType;
   uint8_t uK;
   uint8_t uN;
   uint8_t sessionKey[crypto_aead_chacha20poly1305_KEYBYTES];
} __attribute__ ((packed)) type_wfb_session_data;


typedef struct
{
   uint8_t uPacketType;
   uint64_t uDataNonce; // big endian, data_nonce = (block_idx << 8) + fragment_idx
}  __attribute__ ((packed)) type_wfb_block_header;

typedef struct
{
   uint8_t uFlags;
   uint16_t uPacketSize; // big endian
}  __attribute__ ((packed)) type_wfb_packet_header;

#ifdef __cplusplus
extern "C" {
#endif  

void wfbohd_radio_init();
void wfbohd_radio_cleanup();

void wfbohd_clear_wrong_key_flag();
void wfbohd_set_wrong_key_flag();
int wfbohd_is_wronk_key_flag_set();

void wfbohd_radio_set_discard_video_data(int iDiscard);

uint32_t wfbohd_radio_generate_vehicle_id(uint32_t uFreqKhz);

// returns fake video packet to pass along to router
uint8_t* wfbohd_radio_on_received_video_packet(uint32_t uFreqKhz, fec_t* pFEC, type_wfb_block_header* pBlockHeader, uint8_t* pData, int iDataLength);

uint8_t* radiopackets_wfbohd_generate_fake_ruby_telemetry_header(uint32_t uTimeNow, uint32_t uFreqKhz);
int radiopackets_wfbohd_generate_fake_full_ruby_telemetry_packet(uint32_t uFreqKhz, uint8_t* pRubyTelemetryHeader, uint8_t* pOutput);

#ifdef __cplusplus
}  
#endif

