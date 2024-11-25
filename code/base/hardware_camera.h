#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"

int hardware_hasCamera();
u32 hardware_getCameraType();
int hardware_getCameraI2CBus();
int hardware_getVeyeCameraDevId();
int hardware_getVeyeCameraHWVer();
int hardware_isCameraVeye();
int hardware_isCameraVeye307();
int hardware_isCameraHDMI();

void hardware_camera_apply_all_majestic_camera_settings(camera_profile_parameters_t* pCameraParams);
void hardware_camera_apply_all_majestic_settings(Model* pModel, camera_profile_parameters_t* pCameraParams, int iVideoProfile, video_parameters_t* pVideoParams);
void hardware_camera_set_irfilter_off(int iOff);
void hardware_camera_set_daylight_off(int iDLOff);
