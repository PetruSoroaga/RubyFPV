#pragma once

#define VIDEO_TYPE_NONE 0
#define VIDEO_TYPE_H264 1
#define VIDEO_TYPE_H265 2
#define VIDEO_TYPE_RTP_H264 3
#define VIDEO_TYPE_RTP_H265 4


// Video link profiles: 5 profiles for video link quality management; 1 profile for PIP; rest: unused

#define MAX_VIDEO_LINK_PROFILES 8

#define VIDEO_PROFILE_BEST_PERF 0
#define VIDEO_PROFILE_HIGH_QUALITY 1
#define VIDEO_PROFILE_USER 2
#define VIDEO_PROFILE_MQ 3
#define VIDEO_PROFILE_LQ 4
#define VIDEO_PROFILE_PIP 5
#define VIDEO_PROFILE_LAST 6

#define VIDEO_PROFILE_FLAGS_MASK_NOISE ((u32)0x03)
#define VIDEO_PROFILE_FLAGS_NOISE_AUTO ((u32)0x02)
#define VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS ((u32)(((u32)0x01)<<3))
//#define VIDEO_PROFILE_ENCODING_FLAG_STATUS_ON_LOWER_BITRATE ((u32)(((u32)0x01)<<4))
// Deprecated in 9.5, not used, moved to status flags 2 in video radio packet

#define VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME ((u32)(((u32)0x01)<<4))
#define VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK ((u32)(((u32)0x01)<<5))
#define VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO ((u32)(((u32)0x01)<<6))
#define VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST ((u32)(((u32)0x01)<<7))

#define VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK ((u32)(0x0000FF00))

#define VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT (0xFF<<16)
#define VIDEO_PROFILE_ENCODING_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO (0xFF<<16)

#define VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO ((u32)(((u32)0x01)<<24))
#define VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION ((u32)(((u32)0x01)<<25))
#define VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH ((u32)(((u32)0x01)<<26))
#define VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME ((u32)(((u32)0x01)<<27))
#define VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO ((u32)(((u32)0x01)<<28))
#define VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT ((u32)(((u32)0x01)<<29))
#define VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT ((u32)(((u32)0x01)<<30))

#define VIDEO_STATUS_FLAGS2_HAS_DEBUG_TIMESTAMPS ((u32)(((u32)0x01)<<8))
#define VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE ((u32)(((u32)0x01)<<10))

#define VIDEO_PACKET_FLAGS_IS_END_OF_TRANSMISSION_FRAME ((u32)(((u32)0x01)<<2))
#define VIDEO_PACKET_FLAGS_CONTAINS_I_NAL ((u32)(((u32)0x01)<<3))
#define VIDEO_PACKET_FLAGS_CONTAINS_P_NAL ((u32)(((u32)0x01)<<4))
#define VIDEO_PACKET_FLAGS_CONTAINS_O_NAL ((u32)(((u32)0x01)<<5))


// Highest bit in video bitrate field tells if vehicle adjusted the videobitrate
#define VIDEO_BITRATE_FLAG_ADJUSTED     ((u32)(((u32)0x01)<<31))
#define VIDEO_BITRATE_FIELD_MASK  0x7FFFFFFF

// Video flags

//#define VIDEO_ FLAG_ FILL_H264_SPT_TIMINGS     ((u32)((u32)0x01)) // deprecated in 10.1
//#define VIDEO _FLAG_ IGNORE_TX_SPIKES          ((u32)(((u32)0x01)<<1)) // deprecated
#define VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT  ((u32)(((u32)0x01)<<2))
#define VIDEO_FLAG_RETRANSMISSIONS_FAST      ((u32)(((u32)0x01)<<3))
#define VIDEO_FLAG_GENERATE_H265             ((u32)(((u32)0x01)<<4))
#define VIDEO_FLAG_NEW_ADAPTIVE_ALGORITHM    ((u32)(((u32)0x01)<<5))
