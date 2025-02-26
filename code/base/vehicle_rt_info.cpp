/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "base.h"
#include "shared_mem.h"
#include "vehicle_rt_info.h"


vehicle_runtime_info* vehicle_rt_info_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_VEHICLE_RUNTIME_INFO, sizeof(vehicle_runtime_info));
   return (vehicle_runtime_info*)retVal;
}

vehicle_runtime_info* vehicle_rt_info_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_VEHICLE_RUNTIME_INFO, sizeof(vehicle_runtime_info));
   vehicle_runtime_info* pRTInfo = (vehicle_runtime_info*)retVal;
   vehicle_rt_info_init(pRTInfo);
   return pRTInfo;
}

void vehicle_rt_info_close(vehicle_runtime_info* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(vehicle_runtime_info));
   //shm_unlink(szName);
}

void vehicle_rt_info_init(vehicle_runtime_info* pRTInfo)
{
   if ( NULL == pRTInfo )
      return;

   memset(pRTInfo, 0, sizeof(vehicle_runtime_info));
   
   pRTInfo->uUpdateIntervalMs = SYSTEM_RT_INFO_UPDATE_INTERVAL_MS;
   pRTInfo->uCurrentSliceStartTime = 0;
   pRTInfo->iCurrentIndex = 0;
   pRTInfo->uLastTimeSentToController = 0;
}

void vehicle_rt_info_check_advance_index(vehicle_runtime_info* pRTInfo, u32 uTimeNowMs)
{
   if ( NULL == pRTInfo )
      return;
   if ( uTimeNowMs < (pRTInfo->uCurrentSliceStartTime + pRTInfo->uUpdateIntervalMs) )
      return;

   pRTInfo->uCurrentSliceStartTime = uTimeNowMs;
   pRTInfo->iCurrentIndex++;
   if ( pRTInfo->iCurrentIndex >= SYSTEM_RT_INFO_INTERVALS )
      pRTInfo->iCurrentIndex = 0;

   pRTInfo->uSentVideoDataPackets[pRTInfo->iCurrentIndex] = 0;
   pRTInfo->uSentVideoECPackets[pRTInfo->iCurrentIndex] = 0;
}