#pragma once

#include "../base/base.h"
#include "../base/config.h"
#include "../base/msp.h"
#include "../radio/radiopackets2.h"
#include "shared_vars_state.h"

void parse_msp_reset_state(type_msp_parse_state* pMSPState);

void parse_msp_incoming_data(t_structure_vehicle_info* pRuntimeInfo, u8* pData, int iDataLength);
