#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/hardware_cam_maj.h"

int hardware_hasCamera();
int hardware_camera_has_ip_camera_config();
int hardware_camera_get_ip_camera_config_file(char* pszOutFile);
u32 hardware_getCameraType();
int hardware_getCameraI2CBus();
int hardware_getVeyeCameraDevId();
int hardware_getVeyeCameraHWVer();
int hardware_isCameraVeye();
int hardware_isCameraVeye307();
int hardware_isCameraHDMI();
char* hardware_camera_get_oipc_sensor_raw_name(char* pszOutput);
void hardware_camera_check_set_oipc_sensor();
void hardware_camera_set_default_oipc_calibration(int iCameraType);

