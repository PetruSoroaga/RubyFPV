#pragma once
#include "../base/base.h"

void video_source_csi_close();
int video_source_csi_open(const char* szPipeName);

void video_source_csi_flush_discard();
int video_source_csi_get_buffer_size();

// Returns the buffer and number of bytes read
u8* video_source_csi_read(int* piReadSize);
bool video_source_csi_read_any_data();
void video_source_csi_start_program();
void video_source_csi_stop_program();
bool video_source_csi_is_program_started();
u32 video_source_cs_get_program_start_time();
void video_source_csi_request_restart_program();
bool video_source_csi_is_restart_requested();

void video_source_csi_send_control_message(u8 parameter, u16 value1, u16 value2);
u32 video_source_csi_get_last_set_videobitrate();
void video_source_csi_periodic_checks();

bool vehicle_launch_video_capture_csi(Model* pModel);
void vehicle_stop_video_capture_csi(Model* pModel);
void vehicle_update_camera_params_csi(Model* pModel, int iCameraIndex);
