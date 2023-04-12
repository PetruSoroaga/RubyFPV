#pragma once
#include "../base/base.h"

void warnings_on_changed_vehicle();
void warnings_remove_all();
void warnings_add(const char* szTitle);
void warnings_add(const char* szTitle, u32 iconId);
void warnings_add(const char* szTitle, u32 iconId, double* pColorIcon, int timeout=8);

void warnings_add_error_null_model(int code);
void warnings_add_radio_reinitialized();
void warnings_add_too_much_data(int current_kbps, int max_kbps);
void warnings_add_vehicle_overloaded();

void warnings_add_link_to_controller_lost();
void warnings_add_link_to_controller_recovered();
