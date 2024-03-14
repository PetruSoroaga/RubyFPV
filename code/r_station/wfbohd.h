#pragma once
#include "../base/base.h"

void wfbohd_init(bool bSearching);
void wfbohd_cleanup();

bool wfbohd_is_video_player_started();
void wfbohd_check_start_video_player();

int wfbohd_process_auxiliary_radio_packet(u8* pBuffer, int iBufferLength);
