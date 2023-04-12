#pragma once
#include "../base/base.h"

// Globals

extern u32 g_TimeNow;
extern u32 g_TimeStart;
extern u32 g_TimeNowMicros;
extern u32 g_TimeLastPeriodicCheck;
extern u32 g_TimeLastPipesCheck;


// RC Tx

extern u32 g_TimeLastFPSCalculation;
extern u32 g_TimeLastJoystickCheck;
extern u32 g_TimeLastRCFrameSent;

// Router

extern u32 g_uLastPingSendTimeMicroSec;
extern u32 g_uLastReceivedPingResponseTime;
extern u32 g_uTimeLastReceivedResponseToAMessage;

extern u32 g_TimeLastControllerLinkStatsSent;
extern u32 g_TimeLastSetRadioFlagsCommandSent;

extern u32 g_uTimeLastCommandRequestSent;

