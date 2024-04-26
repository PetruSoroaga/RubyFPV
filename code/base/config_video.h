#pragma once

#define MAX_VIDEO_PROCESSORS 4

// Maximum percent of radio datarate to use for video
#define DEFAULT_VIDEO_LINK_MAX_LOAD_PERCENT 90
#define DEFAULT_VIDEO_LINK_LOAD_PERCENT 80
#define DEFAULT_LOWER_VIDEO_RADIO_DATARATE_AFTER_MS 300

#define DEFAULT_MINIMUM_OK_INTERVAL_MS_TO_SWITCH_VIDEO_PROFILE_UP 1500
#define DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH 5
#define DEFAULT_MINIMUM_INTERVALS_FOR_VIDEO_LINK_ADJUSTMENT 3
#define DEFAULT_CONTROLLER_LINK_MILISECONDS_TIMEOUT_TO_DISABLE_RETRANSMISSIONS 3000

#define DEFAULT_VIDEO_H264_QUANTIZATION -18
#define DEFAULT_VIDEO_H264_SLICES 4

#define DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL 6000 // in miliseconds
#define DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL 200 // in miliseconds
#define DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL 50 // in miliseconds
#define DEFAULT_VIDEO_KEYFRAME_INTERVAL_WHEN_RELAYING 100 // in miliseconds

#define DEFAULT_VIDEO_RETRANS_MS5_HP ((u32)17)  // 17*5 = 85 milisec
#define DEFAULT_VIDEO_RETRANS_MS5_HQ ((u32)28)  // 20*5 = 100 milisec
#define DEFAULT_VIDEO_RETRANS_MS5_MQ ((u32)30)  // 25*5 = 125 milisec
#define DEFAULT_VIDEO_RETRANS_MS5_LQ ((u32)36)  // 25*5 = 125 milisec
#define DEFAULT_VIDEO_RETRANS_MINIMUM_RETRY_INTERVAL 10 //milisec
#define DEFAULT_VIDEO_RETRANS_REQUEST_ON_VIDEO_SILENCE_MS 70 // milisec

#define DEFAULT_VIDEO_WIDTH 1280
#define DEFAULT_VIDEO_HEIGHT 720
//#define DEFAULT_VIDEO_WIDTH 853
//#define DEFAULT_VIDEO_HEIGHT 480
//#define DEFAULT_VIDEO_WIDTH 1024
//#define DEFAULT_VIDEO_HEIGHT 576
#define DEFAULT_VIDEO_FPS 30
#define DEFAULT_VIDEO_FPS_OIPC 60
#define DEFAULT_VIDEO_KEYFRAME -200 // milisec, negative: auto

#define DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE 1000000
#define DEFAULT_VIDEO_BITRATE 5000000 // in bps
#define DEFAULT_VIDEO_BITRATE_PI_ZERO 4500000 // in bps

#define DEFAULT_VIDEO_BLOCK_PACKETS_HP 9
#define DEFAULT_VIDEO_BLOCK_FECS_HP 4
#define DEFAULT_VIDEO_DATA_LENGTH_HP 1250

#define DEFAULT_VIDEO_BLOCK_PACKETS_HQ 9
#define DEFAULT_VIDEO_BLOCK_FECS_HQ 3
#define DEFAULT_VIDEO_DATA_LENGTH_HQ 1250

#define DEFAULT_HP_VIDEO_KEYFRAME 300 // milisec, positive: fixed
//#define DEFAULT_HP_VIDEO_RADIO_DATARATE 12000000
#define DEFAULT_HP_VIDEO_RADIO_DATARATE 0 // Auto
#define DEFAULT_HP_VIDEO_BITRATE 5000000

#define DEFAULT_MQ_VIDEO_BITRATE 4000000 // in bps
#define DEFAULT_MQ_VIDEO_BLOCK_PACKETS 8
#define DEFAULT_MQ_VIDEO_BLOCK_FECS 4
#define DEFAULT_MQ_VIDEO_DATA_LENGTH 1250
#define DEFAULT_MQ_VIDEO_FPS 30
#define DEFAULT_MQ_VIDEO_KEYFRAME -200 // milisec, negative: auto

#define DEFAULT_LQ_VIDEO_BITRATE 2500000 // in bps
#define DEFAULT_LQ_VIDEO_BLOCK_PACKETS 6
#define DEFAULT_LQ_VIDEO_BLOCK_FECS 3
#define DEFAULT_LQ_VIDEO_DATA_LENGTH 1250
#define DEFAULT_LQ_VIDEO_FPS 30
#define DEFAULT_LQ_VIDEO_KEYFRAME -200 // milisec, negative: auto


#define MAX_BUFFERED_AUDIO_PACKETS 32

#define MAX_BLOCKS_TO_OUTPUT_IF_AVAILABLE 20
