#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"

void log_full_current_radio_configuration(Model* pModel);
bool check_update_hardware_nics_vehicle(Model* pModel);
bool recheck_disabled_radio_interfaces(Model* pModel);
