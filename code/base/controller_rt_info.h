#pragma once
#include "base.h"
#include "config.h"

#define SHARED_MEM_CONTROLLER_RUNTIME_INFO "/SYSTEM_RUBY_CONTROLLER_RT_INFO"

#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_LOWER ((u32)(((u32)0x01)<<1))
#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_HIGHER ((u32)(((u32)0x01)<<2))
#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_USER_SELECTABLE ((u32)(((u32)0x01)<<3))
#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_USER ((u32)(((u32)0x01)<<5))
#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_ADAPTIVE_LOWER ((u32)(((u32)0x01)<<6))
#define CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_ADAPTIVE_HIGHER ((u32)(((u32)0x01)<<7))
#define CTRL_RT_INFO_FLAG_RECV_ACK ((u32)(((u32)0x01)<<8))

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct 
{
   int iCountAntennas;
   int iDbmLast[MAX_RADIO_ANTENNAS];
   int iDbmMin[MAX_RADIO_ANTENNAS];
   int iDbmMax[MAX_RADIO_ANTENNAS];
   int iDbmAvg[MAX_RADIO_ANTENNAS];
   int iDbmChangeSpeedMin[MAX_RADIO_ANTENNAS];
   int iDbmChangeSpeedMax[MAX_RADIO_ANTENNAS];
   int iDbmNoiseLast[MAX_RADIO_ANTENNAS];
   int iDbmNoiseMin[MAX_RADIO_ANTENNAS];
   int iDbmNoiseMax[MAX_RADIO_ANTENNAS];
   int iDbmNoiseAvg[MAX_RADIO_ANTENNAS];
} ALIGN_STRUCT_SPEC_INFO controller_runtime_info_radio_interface_rx_signal;


typedef struct
{
   u32 uVehicleId;
   u8 uMinAckTime[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uMaxAckTime[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uCountReqRetrPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uCountReqRetransmissions[SYSTEM_RT_INFO_INTERVALS];
   u8 uCountAckRetransmissions[SYSTEM_RT_INFO_INTERVALS];
   u8 uAckTimes[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   int iAckTimeIndex[MAX_RADIO_INTERFACES];
} ALIGN_STRUCT_SPEC_INFO controller_runtime_info_vehicle;

typedef struct
{
   u32 uUpdateIntervalMs;
   u32 uCurrentSliceStartTime;
   int iCurrentIndex;
   int iCurrentIndex2;
   int iCurrentIndex3;
   int iDeltaIndexFromVehicle;
   u32 uSliceUpdateTime[SYSTEM_RT_INFO_INTERVALS];
   u8 uRxVideoPackets[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxVideoECPackets[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxDataPackets[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxHighPriorityPackets[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxMissingPackets[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxMissingPacketsMaxGap[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRxProcessedPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uRxMaxAirgapSlots[SYSTEM_RT_INFO_INTERVALS];
   u8 uRxMaxAirgapSlots2[SYSTEM_RT_INFO_INTERVALS];

   u8 uTxPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uTxHighPriorityPackets[SYSTEM_RT_INFO_INTERVALS];

   u8 uRecvVideoDataPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uRecvVideoECPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uRecvFramesInfo[SYSTEM_RT_INFO_INTERVALS];
     // bit 0: is IFrame
     // bit 1: is PFrame
     // bit 2: end of IFrame
     // bit 3: end of PFrame
     // bit 4: parsed start output of IFrame
     // bit 5: parsed start output of PFrame
     // bit 6: parsed start output of other frames
   
   u8 uOutputedVideoPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoPacketsRetransmitted[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoPacketsSingleECUsed[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoPacketsTwoECUsed[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoPacketsMultipleECUsed[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoPacketsMaxECUsed[SYSTEM_RT_INFO_INTERVALS];
   u8 uOutputedVideoPacketsSkippedBlocks[SYSTEM_RT_INFO_INTERVALS];

   controller_runtime_info_radio_interface_rx_signal radioInterfacesDbm[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uDbmChangeSpeed[SYSTEM_RT_INFO_INTERVALS][MAX_RADIO_INTERFACES];
   u8 uRadioLinkQuality[SYSTEM_RT_INFO_INTERVALS];

   u32 uFlagsAdaptiveVideo[SYSTEM_RT_INFO_INTERVALS];
   u32 uTotalCountOutputSkippedBlocks;

   controller_runtime_info_vehicle vehicles[MAX_CONCURENT_VEHICLES];
} ALIGN_STRUCT_SPEC_INFO controller_runtime_info;


controller_runtime_info* controller_rt_info_open_for_read();
controller_runtime_info* controller_rt_info_open_for_write();
void controller_rt_info_close(controller_runtime_info* pAddress);
void controller_rt_info_init(controller_runtime_info* pRTInfo);
controller_runtime_info_vehicle* controller_rt_info_get_vehicle_info(controller_runtime_info* pRTInfo, u32 uVehicleId);
void controller_rt_info_update_ack_rt_time(controller_runtime_info* pRTInfo, u32 uVehicleId, int iRadioLink, u32 uRoundTripTime);
int controller_rt_info_will_advance_index(controller_runtime_info* pRTInfo, u32 uTimeNowMs);
int controller_rt_info_check_advance_index(controller_runtime_info* pRTInfo, u32 uTimeNowMs);
#ifdef __cplusplus
}  
#endif 
