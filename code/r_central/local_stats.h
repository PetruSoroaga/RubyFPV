#pragma once
#include "../base/base.h"

void local_stats_reset_all();
void local_stats_reset_vehicle_index(int iVehicleIndex);
void local_stats_on_arm(u32 uVehicleId);
void local_stats_on_disarm(u32 uVehicleId);

void local_stats_update_loop();

bool load_temp_local_stats();
void save_temp_local_stats();
