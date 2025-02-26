#pragma once
#include "../base/base.h"
#include "../base/config.h"

bool hardware_board_has_audio_builtin(u32 uBoardType);
bool hardware_has_audio_playback();
int hardware_enable_audio_output();
bool hardware_has_audio_volume();
int hardware_set_audio_output_volume(int iAudioVolume);
int hardware_audio_play_file_async(const char* szFile);
