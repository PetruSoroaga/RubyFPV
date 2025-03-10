#pragma once

#include "../base/base.h"

void rx_video_recording_init();
void rx_video_recording_uninit();

void rx_video_recording_start();
void rx_video_recording_stop();
bool rx_video_is_recording();

void rx_video_recording_on_new_data(u8* pData, int iLength);

void rx_video_recording_periodic_loop();

