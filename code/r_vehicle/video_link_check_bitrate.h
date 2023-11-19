#pragma once
#include "../base/base.h"

int video_link_get_default_quantization_for_videobitrate(u32 uVideoBitRate);
void video_link_check_adjust_quantization_for_overload_periodic_loop();
void video_link_check_adjust_bitrate_for_overload();

void video_link_set_last_quantization_set(u8 uQuantValue);
void video_link_set_fixed_quantization_values(u8 uQuantValue);
void video_link_quantization_shift(int iDelta);
u8 video_link_get_oveflow_quantization_value();
void video_link_reset_overflow_quantization_value();

