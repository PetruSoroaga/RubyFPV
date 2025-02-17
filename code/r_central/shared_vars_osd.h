#pragma once
#include "../base/config.h"

extern float g_fOSDDbm[MAX_RADIO_INTERFACES];
extern float g_fOSDSNR[MAX_RADIO_INTERFACES];

void shared_vars_osd_reset_before_pairing();

void shared_vars_osd_update();
