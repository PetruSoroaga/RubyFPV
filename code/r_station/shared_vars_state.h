#pragma once

#define MAX_RUNTIME_INFO_PINGS_HISTORY 3
#define MAX_RUNTIME_INFO_LINKS_RT_TIMES 5
#define MAX_RUNTIME_INFO_COMMANDS_RT_TIMES 5

typedef struct
{
   u32 uVehicleId;

   // Pairing info
   bool bIsPairingDone;
   u32 uPairingRequestTime;
   u32 uPairingRequestInterval;
   u32 uPairingRequestId;

   // Radio link roundtrip/ping times

   u32 uPingRoundtripTimeOnLocalRadioLinks[MAX_RADIO_INTERFACES];
   u32 uTimeLastPingSentToVehicleOnLocalRadioLinks[MAX_RADIO_INTERFACES][MAX_RUNTIME_INFO_PINGS_HISTORY];
   u32 uTimeLastPingReplyReceivedFromVehicleOnLocalRadioLinks[MAX_RADIO_INTERFACES];
   u8  uLastPingIdSentToVehicleOnLocalRadioLinks[MAX_RADIO_INTERFACES][MAX_RUNTIME_INFO_PINGS_HISTORY];
   u8  uLastPingIdReceivedFromVehicleOnLocalRadioLinks[MAX_RADIO_INTERFACES];

   u32 uLastLinkRoundtripTimesMs[MAX_RADIO_INTERFACES][MAX_RUNTIME_INFO_LINKS_RT_TIMES];

   u32 uRadioLinkRoundtripLastComputedTime[MAX_RADIO_INTERFACES];
   u32 uRadioLinkRoundtripMsLast[MAX_RADIO_INTERFACES];
   u32 uRadioLinkRoundtripMsAvg[MAX_RADIO_INTERFACES];
   u32 uRadioLinkRoundtripMsMin[MAX_RADIO_INTERFACES];
   u32 uRadioLinkRoundtripMsMax[MAX_RADIO_INTERFACES];

   u32 uRadioLinksMinimumRoundtripMs;
   int iVehicleClockIsBehindThisMilisec;

   // Commands roundtrip info

   u32 uTimeLastCommandIdSent;
   u32 uLastCommandIdSent;
   u32 uLastCommandIdRetrySent;
   u32 uLastCommandsRoundtripTimesMs[MAX_RUNTIME_INFO_COMMANDS_RT_TIMES];

   u32 uAverageCommandRoundtripMiliseconds;
   u32 uMaxCommandRoundtripMiliseconds;
   u32 uMinCommandRoundtripMiliseconds;

   u32 uMinPingRoundtripTime;

   // Adaptive video info
   
   bool bReceivedKeyframeInfoInVideoStream;

} __attribute__((packed)) type_global_state_vehicle_runtime_info;


typedef struct
{
   type_global_state_vehicle_runtime_info vehiclesRuntimeInfo[MAX_CONCURENT_VEHICLES];
} __attribute__((packed)) type_global_state_station;


extern type_global_state_station g_State;

void resetVehicleRuntimeInfo(int iIndex);
void resetPairingStateForVehicleRuntimeInfo(int iIndex);
void removeVehicleRuntimeInfo(int iIndex);
int  getVehicleRuntimeIndex(u32 uVehicleId);
type_global_state_vehicle_runtime_info* getVehicleRuntimeInfo(u32 uVehicleId);
void logCurrentVehiclesRuntimeInfo();

void addCommandRTTimeToRuntimeInfo(type_global_state_vehicle_runtime_info* pRuntimeInfo, u32 uRoundtripTimeMs);
void addLinkRTTimeToRuntimeInfoIndex(int iRuntimeInfoIndex, int iLocalRadioLink, u32 uRoundtripTimeMs, u32 uLocalTimeVehicleMs);
