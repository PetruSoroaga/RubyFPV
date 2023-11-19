/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "timers.h"

// Globals

u32 g_TimeNow = 0;
u32 g_TimeStart = 0;
u32 g_TimeNowMicros = 0;

u32 g_TimeLastVideoCameraChangeCommand = 0;
u32 g_TimeStartPendingRadioFlagsChange = 0;

// Central

u32 g_RouterIsReadyTimestamp = 0;
u32 g_TimeLastVideoDataOverloadAlarm = 0;
u32 g_TimeLastVideoTxOverloadAlarm = 0;
u32 g_TimeLastSentCurrentActiveOSDLayout = 0;

u32 g_uTimeLastRadioLinkOverloadAlarm = 0;
