#pragma once
#include "../base/base.h"

bool load_favorites();
bool save_favorites();
bool vehicle_is_favorite(u32 uVehicleId);
bool remove_favorite(u32 uVehicleId);
bool add_favorite(u32 uVehicleId);
u32 get_next_favorite(u32 uVehicleId);
