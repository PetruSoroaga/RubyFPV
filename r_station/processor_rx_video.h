#pragma once
#include "../base/base.h"
#include "../base/models.h"

bool process_data_rx_video_init();
bool process_data_rx_video_uninit();

void process_data_rx_video_reset_state();

void process_data_rx_video_loop();

int process_data_rx_video_on_new_packet(int interfaceNb, u8* pBuffer, int length);

void process_data_rx_video_reset_retransmission_stats();
void process_data_rx_video_on_controller_settings_changed();

int process_data_rx_video_get_current_received_video_profile();
int process_data_rx_video_get_current_received_video_fps();
int process_data_rx_video_get_current_received_video_keyframe();
int process_data_rx_video_get_missing_video_packets_now();
