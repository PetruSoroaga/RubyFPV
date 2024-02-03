#pragma once


//Deprecated in 8.1
//#define RADIO_FLAGS_ENABLE_MCS_FLAGS  ((u32)0x01)
//#define RADIO_FLAGS_STBC  (((u32)0x01)<<1)
//#define RADIO_FLAGS_LDPC  (((u32)0x01)<<2)
//#define RADIO_FLAGS_SHORT_GI (((u32)0x01)<<3)

#define RADIO_FLAGS_FRAME_TYPE_DATA_SHORT (((u32)0x01)<<5)
#define RADIO_FLAGS_FRAME_TYPE_DATA (((u32)0x01)<<6)
#define RADIO_FLAGS_FRAME_TYPE_RTS (((u32)0x01)<<7)


// Deprecated in 8.1
//#define RADIO_FLAGS_HT20  (((u32)0x01)<<8)
//#define RADIO_FLAGS_HT40  (((u32)0x01)<<9)
//#define RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE  (((u32)0x01)<<10)
//#define RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER  (((u32)0x01)<<11)


#define RADIO_FLAGS_SIK_ECC (((u32)0x01)<<12)
#define RADIO_FLAGS_SIK_LBT (((u32)0x01)<<13)
#define RADIO_FLAGS_SIK_MCSTR (((u32)0x01)<<14)


// These require MCS data rates
// Introduced in 8.1
// Third byte. If any of the bits is set, 3rd byte is not zero, so MCS rates must be used
#define RADIO_FLAGS_MCS_MASK 0x00FF0000
#define RADIO_FLAG_HT40_VEHICLE (((u32)0x01)<<16)
#define RADIO_FLAG_HT40_CONTROLLER (((u32)0x01)<<17)
#define RADIO_FLAG_SGI_VEHICLE (((u32)0x01)<<18)
#define RADIO_FLAG_SGI_CONTROLLER (((u32)0x01)<<19)
#define RADIO_FLAG_STBC_VEHICLE (((u32)0x01)<<20)
#define RADIO_FLAG_STBC_CONTROLLER (((u32)0x01)<<21)
#define RADIO_FLAG_LDPC_VEHICLE (((u32)0x01)<<22)
#define RADIO_FLAG_LDPC_CONTROLLER (((u32)0x01)<<23)
