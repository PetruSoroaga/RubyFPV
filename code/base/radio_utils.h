#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/shared_mem.h"

bool radio_utils_set_interface_frequency(Model* pModel, int iRadioIndex, u32 uFrequencyKhz, shared_mem_process_stats* pProcessStats);
bool radio_utils_set_datarate_atheros(Model* pModel, int iCard, int datarate_bps);
void update_atheros_card_datarate(Model* pModel, int iInterfaceIndex, int iDataRate, shared_mem_process_stats* pProcessStats);

bool configure_radio_interfaces_for_current_model(Model* pModel, shared_mem_process_stats* pProcessStats);
void log_current_full_radio_configuration(Model* pModel);
