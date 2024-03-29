#pragma once
#include "../base/base.h"
#include "../base/models.h"

void video_source_majestic_close();
int video_source_majestic_open(int iUDPPort);
u32 video_source_majestic_get_program_start_time();

void video_source_majestic_start_capture_program();
void video_source_majestic_stop_capture_program();
void video_source_majestic_request_update_program(u32 uChangeReason);
void video_source_majestic_set_keyframe_value(float fGOP);
void video_source_majestic_set_videobitrate_value(u32 uBitrate);

// Returns the buffer and number of bytes read
u8* video_source_majestic_read(int* piReadSize, bool bAsync);

void video_source_majestic_periodic_checks();
