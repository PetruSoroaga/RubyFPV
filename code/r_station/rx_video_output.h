#pragma once

#include "../base/base.h"

void rx_video_output_init();
void rx_video_output_uninit();

void rx_video_output_video_data(u32 uVehicleId, int width, int height, u8* pBuffer, int length);
void rx_video_output_on_controller_settings_changed();

void rx_video_output_periodic_loop();

