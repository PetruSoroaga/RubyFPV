#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/alarms.h"

extern u32 g_uPersistentAllAlarmsVehicle;
extern u32 g_uPersistentAllAlarmsLocal;

void alarms_reset();
void alarms_reset_vehicle();

void alarms_remove_all();
void alarms_render();

void alarms_add_from_vehicle(u32 uVehicleId, u32 uAlarms, u32 uFlags1, u32 uFlags2);
void alarms_add_from_local(u32 uAlarms, u32 uFlags1, u32 uFlags2);
