#pragma once

#include "../base/base.h"

void processor_rx_video_forward_init();
void processor_rx_video_forward_uninit();

void processor_rx_video_forware_prepare_video_stream_write();

void processor_rx_video_forward_video_data(u8* pBuffer, int length);
void processor_rx_video_forward_check_controller_settings_changed();

void processor_rx_video_forward_loop();

