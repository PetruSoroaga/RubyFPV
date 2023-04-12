#pragma once
#include "../base/config.h"

extern int g_iComputedOSDCellCount;
extern float g_fOSDDbm[MAX_RADIO_INTERFACES];

extern int g_iCurrentOSDVehicleLayout;
extern int g_iCurrentOSDVehicleRuntimeInfoIndex;

void shared_vars_osd_reset_before_pairing();
