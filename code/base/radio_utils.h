#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/shared_mem.h"

void update_atheros_card_datarate(Model* pModel, int iInterfaceIndex, int iDataRate, shared_mem_process_stats* pProcessStats);
