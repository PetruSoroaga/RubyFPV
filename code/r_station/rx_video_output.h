#pragma once

#include "../base/base.h"

void rx_video_output_init();
void rx_video_output_uninit();

void processor_rx_video_forware_prepare_video_stream_write();

void processor_rx_video_forward_video_data(u32 uVehicleId, int width, int height, u8* pBuffer, int length);
void processor_rx_video_forward_check_controller_settings_changed();

void processor_rx_video_forward_loop();

