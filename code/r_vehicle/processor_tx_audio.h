#pragma once

#include "../base/base.h"

u32 get_audio_bps();

int try_read_audio_input(int fStream);
int send_audio_packets();

