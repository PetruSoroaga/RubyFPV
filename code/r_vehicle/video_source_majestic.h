#pragma once
#include "../base/base.h"
#include "../base/models.h"

#define MAJESTIC_UDP_PORT 5600

void video_source_majestic_init_all_params();
void video_source_majestic_start_and_configure();
void video_source_majestic_cleanup();
void video_source_majestic_close();
int video_source_majestic_open(int iUDPPort);
u32 video_source_majestic_get_program_start_time();
bool video_source_majestic_is_restarting();

void video_source_majestic_request_update_program(u32 uChangeReason);

// Returns the buffer and number of bytes read
u8* video_source_majestic_read(int* piReadSize, bool bAsync);
u8* video_source_majestic_raw_read(int* piReadSize, bool bAsync);
int video_source_majestic_get_audio_data(u8* pOutputBuffer, int iMaxToRead);
void video_source_majestic_clear_audio_buffers();
void video_source_majestic_clear_input_buffers();

bool video_source_majestic_last_read_is_single_nal();
bool video_source_majestic_last_read_is_end_nal();
u32 video_source_majestic_get_last_nal_type();
bool video_source_majestic_periodic_checks();
