#pragma once
#include "../base/base.h"

extern bool g_stats_bHomeSet;
extern double g_stats_fHomeLat;
extern double g_stats_fHomeLon;
extern double g_stats_fLastPosLat;
extern double g_stats_fLastPosLon;

void local_stats_reset_home_set();
void local_stats_reset(u32 uVehicleId);
void local_stats_on_arm();
void local_stats_on_disarm();

void local_stats_update_loop();

bool load_temp_local_stats();
void save_temp_local_stats();
