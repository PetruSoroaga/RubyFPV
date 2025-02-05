#include "timers.h"

// Globals

u32 g_TimeNow = 0;
u32 g_TimeStart = 0;
u32 g_TimeLastPeriodicCheck = 0;

// RC Tx
u32 g_TimeLastFPSCalculation = 0;
u32 g_TimeLastJoystickCheck = 0;

// Router
u32 g_TimeLastControllerLinkStatsSent = 0;

u32 g_TimeLastSetRadioFlagsCommandSent = 0;
u32 g_TimeLastVideoParametersOrProfileChanged = 0;
u32 g_uTimeEndedNegiciateRadioLink = 0;
