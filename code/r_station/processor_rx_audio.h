#pragma once

#include "../base/base.h"

void init_processing_audio();
void uninit_processing_audio();
void init_audio_rx_state();
void stop_audio_player_and_pipe();
void start_audio_player_and_pipe();
bool is_audio_processing_started();

void process_received_audio_packet(u8* pPacketBuffer);
