#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"


bool radio_utils_check_radio();
bool configure_radio_interfaces_for_current_model(Model* pModel);
void log_current_full_radio_configuration(Model* pModel);
