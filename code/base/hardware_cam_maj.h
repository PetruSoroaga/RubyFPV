#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"

void hardware_camera_maj_add_log(const char* szLog, bool bAsync);
int hardware_camera_maj_init();
int hardware_camera_maj_get_current_pid();
bool hardware_camera_maj_start_capture_program(bool bEnableLog);
bool hardware_camera_maj_stop_capture_program();
bool hardware_camera_maj_is_in_developer_mode();
void hardware_camera_maj_update_nal_size(Model* pModel, bool bDevMode, bool bAsync);

void hardware_camera_maj_apply_all_camera_settings(camera_profile_parameters_t* pCameraParams, bool bAsync);
void hardware_camera_maj_apply_all_settings(Model* pModel, camera_profile_parameters_t* pCameraParams, int iVideoProfile, video_parameters_t* pVideoParams, bool bDeveloperMode, bool bAsync);
void hardware_camera_maj_set_irfilter_off(int iOff, bool bAsync);
void hardware_camera_maj_set_daylight_off(int iDLOff);

void hardware_camera_maj_set_brightness(u32 uValue);
void hardware_camera_maj_set_contrast(u32 uValue);
void hardware_camera_maj_set_hue(u32 uValue);
void hardware_camera_maj_set_saturation(u32 uValue);

void hardware_camera_maj_set_keyframe(float fGOP);
u32  hardware_camera_maj_get_current_bitrate();
void hardware_camera_maj_set_bitrate(u32 uBitrate);
u32  hardware_camera_maj_get_temporary_bitrate();
void hardware_camera_maj_set_temporary_bitrate(u32 uBitrate);
int  hardware_camera_maj_get_current_qpdelta();
void hardware_camera_maj_set_qpdelta(int iqpdelta);
void hardware_camera_maj_set_bitrate_and_qpdelta(u32 uBitrate, int iqpdelta);

u32  hardware_camera_maj_get_last_change_time();
void hardware_camera_maj_check_apply_cached_changes();
