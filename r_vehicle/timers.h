#pragma once
#include "../base/base.h"
#include "../base/config.h"

// Globals

extern u32 g_TimeNow;
extern u32 g_TimeStart;
extern u32 g_TimeLastPeriodicCheck;
extern u32 g_TimeLastPipesCheck;

// Vehicle

extern u32 g_TimeLastCheckRadioSilenceFailsafe;

// Telemetry

extern u32 g_TimeLastCheckEverySecond;
extern u32 g_TimeLastRCSentToFC;
extern u32 g_TimeLastMessageFromFC;
extern u32 g_TimeLastMAVLinkHeartbeatSent;

// RC TX

extern u32 g_TimeLastFrameReceived;
extern u32 g_TimeLastQualityMeasurement;

// Commands


// Router

extern u32 g_TimeFirstReceivedRadioPacket;
extern u32 g_TimeLastReceivedRadioPacket;
extern u32 g_TimeLastVideoBlockSent;
extern u32 g_TimeLastDebugFPSComputeTime;
extern u32 g_TimeLastLiveLogCheck;
extern u32 g_TimeLastRadioLinkFlagsChange;
extern u32 g_TimeRadioReinitialized;
extern u32 g_TimeLastPacketsOutPerSecCalculation;
extern u32 g_TimeLastVideoCaptureProgramCheck;
extern u32 g_TimeStartRaspiVid;

extern u32 g_TXMiliSecTimePerSecond[MAX_RADIO_INTERFACES];
extern u32 g_TimeLastOverwriteBitrateDownOnTxOverload;
extern u32 g_TimeLastOverwriteBitrateUpOnTxOverload;

extern u32 g_TimeLastSetRadioFlagsCommandReceived;

extern u32 g_TimeLastHistoryTxComputation;
extern u32 g_TimeLastTxPacket;
extern u32 g_TimeLastVideoPacketIn;