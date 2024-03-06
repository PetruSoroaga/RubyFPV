#pragma once
#include "../base/base.h"

void video_source_udp_close();
int video_source_udp_open(int iUDPPort);

// Returns the buffer and number of bytes read
u8* video_source_udp_read(int* piReadSize, bool bAsync);

void video_source_udp_periodic_checks();
