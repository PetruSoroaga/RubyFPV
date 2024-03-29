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

void hardware_camera_apply_all_majestic_camera_settings(camera_profile_parameters_t* pCameraParams, bool bForceUpdate);
void hardware_camera_apply_all_majestic_settings(camera_profile_parameters_t* pCameraParams, type_video_link_profile* pVideoParams);
