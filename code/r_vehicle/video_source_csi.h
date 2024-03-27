#pragma once
#include "../base/base.h"

void video_source_csi_close();
int video_source_csi_open(const char* szPipeName);

void video_source_csi_flush_discard();

// Returns the buffer and number of bytes read
u8* video_source_csi_read(int* piReadSize);

void video_source_csi_start_program();
void video_source_csi_stop_program();
bool video_source_csi_is_program_started();
u32 video_source_cs_get_program_start_time();
void video_source_csi_request_restart_program();
bool video_source_csi_is_restart_requested();

void video_source_csi_send_control_message(u8 parameter, u8 value);
void video_source_csi_periodic_checks();

bool vehicle_launch_video_capture_csi(Model* pModel, shared_mem_video_link_overwrites* pVideoOverwrites);
void vehicle_stop_video_capture_csi(Model* pModel);
void vehicle_update_camera_params_csi(Model* pModel, int iCameraIndex);
