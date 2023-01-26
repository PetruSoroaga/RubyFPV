#include "timers.h"

// Globals

u32 g_TimeNow = 0;
u32 g_TimeStart = 0;
u32 g_TimeLastPeriodicCheck = 0;
u32 g_TimeLastPipesCheck = 0;

// Vehicle

u32 g_TimeLastCheckRadioSilenceFailsafe = 0;

// Telemetry

u32 g_TimeLastCheckEverySecond = 0;
u32 g_TimeLastRCSentToFC = 0;
u32 g_TimeLastMessageFromFC = 0;
u32 g_TimeLastMAVLinkHeartbeatSent = 0;

// RC TX

u32 g_TimeLastFrameReceived = 0;
u32 g_TimeLastQualityMeasurement = 0;

// Commands


// Router

u32 g_TimeFirstReceivedRadioPacket = 0;
u32 g_TimeLastReceivedRadioPacket = 0;
u32 g_TimeLastVideoBlockSent = 0;
u32 g_TimeLastDebugFPSComputeTime = 0;
u32 g_TimeLastLiveLogCheck = 0;
u32 g_TimeLastRadioLinkFlagsChange = 0;
u32 g_TimeRadioReinitialized = 0;
u32 g_TimeLastPacketsOutPerSecCalculation = 0;

u32 g_TimeLastVideoCaptureProgramCheck = 0;
u32 g_TimeStartRaspiVid = 0;
u32 g_TXMiliSecTimePerSecond[MAX_RADIO_INTERFACES];
u32 g_TimeLastOverwriteBitrateDownOnTxOverload = 0;
u32 g_TimeLastOverwriteBitrateUpOnTxOverload = 0;

u32 g_TimeLastSetRadioFlagsCommandReceived = 0;
u32 g_TimeLastHistoryTxComputation = 0;
u32 g_TimeLastTxPacket = 0;
u32 g_TimeLastVideoPacketIn = 0;