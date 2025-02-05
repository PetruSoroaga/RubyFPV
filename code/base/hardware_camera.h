#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/hardware_cam_maj.h"

int hardware_hasCamera();
u32 hardware_getCameraType();
int hardware_getCameraI2CBus();
int hardware_getVeyeCameraDevId();
int hardware_getVeyeCameraHWVer();
int hardware_isCameraVeye();
int hardware_isCameraVeye307();
int hardware_isCameraHDMI();
