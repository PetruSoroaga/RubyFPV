#pragma once

#include "../base/hardware.h"
#include "../base/models.h"

#define VEHICLE_SETTINGS_STAMP_ID "vVII.6"

typedef struct
{
   int iDevRxLoopTimeout;
} VehicleSettings;

int save_VehicleSettings();
int load_VehicleSettings();
void reset_VehicleSettings();
VehicleSettings* get_VehicleSettings();
