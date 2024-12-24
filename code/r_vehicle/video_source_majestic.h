#pragma once
#include "../base/base.h"
#include "../base/models.h"

void video_source_majestic_init_all_params();
void video_source_majestic_close();
int video_source_majestic_open(int iUDPPort);
u32 video_source_majestic_get_program_start_time();

void video_source_majestic_start_capture_program();
void video_source_majestic_stop_capture_program(int iSignal);
void video_source_majestic_request_update_program(u32 uChangeReason);
void video_source_majestic_set_keyframe_value(float fGOP);
void video_source_majestic_set_videobitrate_value(u32 uBitrate);
void video_source_majestic_set_qpdelta_value(int iqpdelta);

// Returns the buffer and number of bytes read
u8* video_source_majestic_read(int* piReadSize, bool bAsync);
bool video_source_majestic_last_read_is_single_nal();
bool video_source_majestic_last_read_is_end_nal();
bool video_source_majestic_is_inside_iframe();
bool video_source_majestic_las_read_is_picture_frame();
void video_source_majestic_periodic_checks();
