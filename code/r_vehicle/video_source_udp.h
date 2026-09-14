/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include "../base/base.h"
#include "../base/models.h"

// Default UDP ports used by the IP/Ethernet video source.
// The remote encoder (ie. a companion computer) sends H264/H265 RTP to the video port
// and listens for one line JSON control messages on the control port.
#define IP_CAMERA_DEFAULT_VIDEO_UDP_PORT 5600
#define IP_CAMERA_DEFAULT_CONTROL_UDP_PORT 5601

void video_source_udp_apply_all_parameters();
// Returns initial set video bitrate
u32 video_source_udp_start_program(u32 uOverwriteInitialBitrate, int iOverwriteInitialKFMs, int iOverwriteInitialQPDelta, int* pInitialKFSet);
void video_source_udp_stop_program();
u32 video_source_udp_get_program_start_time();

// Returns the buffer and number of bytes read
u8* video_source_udp_read(int* piReadSize, bool bAsync, u32* puOutTimeDataAvailable);
int video_source_udp_get_audio_data(u8* pOutputBuffer, int iMaxToRead);
void video_source_udp_clear_audio_buffers();
void video_source_udp_clear_input_buffers();

bool video_source_udp_last_read_is_single_nal();
bool video_source_udp_last_read_is_start_nal();
bool video_source_udp_last_read_is_end_nal();
u32 video_source_udp_get_last_nal_type();
bool video_source_udp_periodic_health_checks();

// Control channel to the remote encoder
bool video_source_udp_send_control(const char* szJSONLine);
void video_source_udp_set_video_bitrate(u32 uVideoBitrateBPS, int iIPQDelta);
void video_source_udp_set_keyframe(int iKeyframeMs);
void video_source_udp_request_idr();
u32 video_source_udp_get_current_bitrate();
int video_source_udp_get_current_keyframe();
int video_source_udp_get_current_qpdelta();
