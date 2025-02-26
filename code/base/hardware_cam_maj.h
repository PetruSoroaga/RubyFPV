#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"

void hardware_camera_maj_add_log(const char* szLog, bool bAsync);
int hardware_camera_maj_init();
int hardware_camera_maj_get_current_pid();
bool hardware_camera_maj_start_capture_program(bool bEnableLog);
bool hardware_camera_maj_stop_capture_program();
int  hardware_camera_maj_get_current_nal_size();
void hardware_camera_maj_update_nal_size(Model* pModel, bool bAsync);

void hardware_camera_maj_apply_image_settings(camera_profile_parameters_t* pCameraParams, bool bAsync);
void hardware_camera_maj_apply_all_settings(Model* pModel, camera_profile_parameters_t* pCameraParams, int iVideoProfile, video_parameters_t* pVideoParams, bool bAsync);
void hardware_camera_maj_set_irfilter_off(int iOff, bool bAsync);
void hardware_camera_maj_set_daylight_off(int iDLOff);

void hardware_camera_maj_set_brightness(u32 uValue);
void hardware_camera_maj_set_contrast(u32 uValue);
void hardware_camera_maj_set_hue(u32 uValue);
void hardware_camera_maj_set_saturation(u32 uValue);

void hardware_camera_maj_clear_temp_values();
void hardware_camera_maj_set_keyframe(float fGOP);
u32  hardware_camera_maj_get_current_bitrate();
void hardware_camera_maj_set_bitrate(u32 uBitrate);
int  hardware_camera_maj_get_current_qpdelta();
void hardware_camera_maj_set_qpdelta(int iQPDelta);
void hardware_camera_maj_set_bitrate_and_qpdelta(u32 uBitrate, int iQPDelta);

u32  hardware_camera_maj_get_last_change_time();
void hardware_camera_maj_check_apply_cached_image_changes(u32 uTimeNow);


void hardware_camera_maj_enable_audio(bool bEnable, int iBitrate, int iVolume);
void hardware_camera_maj_set_audio_volume(int iVolume);
void hardware_camera_maj_set_audio_quality(int iBitrate);
u32  hardware_camera_maj_get_last_audio_change_time();
