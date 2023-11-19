#pragma once
#include "../base/base.h"

void onEventBeforeRuntimeCurrentVideoProfileChanged(int iOldVideoProfile, int iNewVideoProfile);

void onEventRelayModeChanged(u32 uOldRelayMode, u32 uNewRelayMode, const char* szSource);
