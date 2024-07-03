#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"

void do_first_boot_pre_initialization();

void do_first_boot_initialization(bool bIsVehicle, u32 uBoardType);
Model* first_boot_create_default_model(bool bIsVehicle, u32 uBoardType);
