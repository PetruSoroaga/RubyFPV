#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/alarms.h"

extern u32 g_uPersistentAllAlarmsVehicle;
extern u32 g_uPersistentAllAlarmsLocal;

void alarms_render();

void alarms_add_from_vehicle(u32 uAlarms, u32 uFlags, u32 uAlarmsCount);
void alarms_add_from_local(u32 uAlarms, u32 uFlags, u32 uAlarmsCount);

