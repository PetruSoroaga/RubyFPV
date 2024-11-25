#pragma once
#include "base.h"
#include "config.h"

#define SHARED_MEM_VEHICLE_RUNTIME_INFO "/SYSTEM_RUBY_VEHICLE_RT_INFO"


#ifdef __cplusplus
extern "C" {
#endif 


typedef struct
{  
   u32 uUpdateIntervalMs;
   u32 uCurrentSliceStartTime;
   int iCurrentIndex;
   u32 uLastTimeSentToController;

   u8 uSentVideoDataPackets[SYSTEM_RT_INFO_INTERVALS];
   u8 uSentVideoECPackets[SYSTEM_RT_INFO_INTERVALS];
} ALIGN_STRUCT_SPEC_INFO vehicle_runtime_info;


vehicle_runtime_info* vehicle_rt_info_open_for_read();
vehicle_runtime_info* vehicle_rt_info_open_for_write();
void vehicle_rt_info_close(vehicle_runtime_info* pAddress);
void vehicle_rt_info_init(vehicle_runtime_info* pRTInfo);
void vehicle_rt_info_check_advance_index(vehicle_runtime_info* pRTInfo, u32 uTimeNowMs);
#ifdef __cplusplus
}  
#endif 
