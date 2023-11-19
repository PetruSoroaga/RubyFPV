#pragma once

#include "../base/hardware.h"

#define VEHICLE_SETTINGS_STAMP_ID "vVII.6"

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct
{
   int iDevRxLoopTimeout;
} VehicleSettings;

int save_VehicleSettings();
int load_VehicleSettings();
void reset_VehicleSettings();
VehicleSettings* get_VehicleSettings();

#ifdef __cplusplus
}  
#endif 