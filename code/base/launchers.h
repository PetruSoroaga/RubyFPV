#pragma once
#include "models.h"
#include "../base/shared_mem.h"

bool launch_set_frequency(Model* pModel, int iRadioIndex, u32 uFrequency, shared_mem_process_stats* pProcessStats);
bool launch_set_datarate_atheros(Model* pModel, int iCard, int datarate);
