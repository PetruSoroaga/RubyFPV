#pragma once

#include "../base/base.h"

void init_processing_audio();
void uninit_processing_audio();

void process_received_audio_packet(u8* pPacketBuffer);
