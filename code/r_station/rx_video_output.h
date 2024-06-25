#pragma once

#include "../base/base.h"

void rx_video_output_init();
void rx_video_output_uninit();

void rx_video_output_enable_pipe_output();
void rx_video_output_disable_pipe_output();
void rx_video_output_enable_local_player_udp_output();
void rx_video_output_disable_local_player_udp_output();

void rx_video_output_video_data(u32 uVehicleId, u8 uVideoStreamType, int width, int height, u8* pBuffer, int video_data_length, int packet_length);
void rx_video_output_on_controller_settings_changed();

void rx_video_output_signal_restart_player();
void rx_video_output_periodic_loop();

