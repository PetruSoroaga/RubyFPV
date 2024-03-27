#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"

void do_first_boot_initialization(bool bIsVehicle, int iBoardType);
Model* first_boot_create_default_model(bool bIsVehicle, int iBoardType);
