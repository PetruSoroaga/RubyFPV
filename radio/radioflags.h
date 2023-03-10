#pragma once


#define RADIO_FLAGS_ENABLE_MCS_FLAGS  ((u32)0x01)
#define RADIO_FLAGS_STBC  (((u32)0x01)<<1)
#define RADIO_FLAGS_LDPC  (((u32)0x01)<<2)
#define RADIO_FLAGS_SHORT_GI (((u32)0x01)<<3)
#define RADIO_FLAGS_FRAME_TYPE_DATA_SHORT (((u32)0x01)<<5)
#define RADIO_FLAGS_FRAME_TYPE_DATA (((u32)0x01)<<6)
#define RADIO_FLAGS_FRAME_TYPE_RTS (((u32)0x01)<<7)
#define RADIO_FLAGS_HT20  (((u32)0x01)<<8)
#define RADIO_FLAGS_HT40  (((u32)0x01)<<9)
#define RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE  (((u32)0x01)<<10)
#define RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER  (((u32)0x01)<<11)

#define DEFAULT_RADIO_FRAMES_FLAGS (RADIO_FLAGS_FRAME_TYPE_DATA | RADIO_FLAGS_HT20 | RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE)
