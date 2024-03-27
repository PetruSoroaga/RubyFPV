#pragma once
#include "../base/base.h"
#include "../base/models.h"

void video_source_majestic_close();
int video_source_majestic_open(int iUDPPort);

void video_source_majestic_start_capture_program();
void video_source_majestic_stop_capture_program();
void video_source_majestic_request_restart_program(int iChangeReason);

// Returns the buffer and number of bytes read
u8* video_source_majestic_read(int* piReadSize, bool bAsync);

void video_source_majestic_periodic_checks();
