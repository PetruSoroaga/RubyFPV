#pragma once
#include "../base/base.h"

void video_link_check_adjust_quantization_for_overload();
void video_link_check_adjust_bitrate_for_overload();

void video_link_set_last_quantization_set(u8 uQuantValue);
u8 video_link_get_oveflow_quantization_value();
void video_link_reset_overflow_quantization_value();

