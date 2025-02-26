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
#include "controller_rt_info.h"
#include "hardware_radio.h"


controller_runtime_info* controller_rt_info_open_for_read()
{
   void *retVal = open_shared_mem_for_read(SHARED_MEM_CONTROLLER_RUNTIME_INFO, sizeof(controller_runtime_info));
   return (controller_runtime_info*)retVal;
}

controller_runtime_info* controller_rt_info_open_for_write()
{
   void *retVal = open_shared_mem_for_write(SHARED_MEM_CONTROLLER_RUNTIME_INFO, sizeof(controller_runtime_info));
   controller_runtime_info* pRTInfo = (controller_runtime_info*)retVal;
   controller_rt_info_init(pRTInfo);
   return pRTInfo;
}

void controller_rt_info_close(controller_runtime_info* pAddress)
{
   if ( NULL != pAddress )
      munmap(pAddress, sizeof(controller_runtime_info));
   //shm_unlink(szName);
}

void _controller_runtime_info_reset_dbm_slice(controller_runtime_info* pRTInfo, int iSliceIndex)
{
   if ( NULL == pRTInfo )
      return;
   for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
   {
      for( int j=0; j<MAX_RADIO_ANTENNAS; j++ )
      {
         pRTInfo->radioInterfacesDbm[iSliceIndex][k].iDbmLast[j] = 1000;
         pRTInfo->radioInterfacesDbm[iSliceIndex][k].iDbmMin[j] = 1000;
         pRTInfo->radioInterfacesDbm[iSliceIndex][k].iDbmMax[j] = 1000;
         pRTInfo->radioInterfacesDbm[iSliceIndex][k].iDbmAvg[j] = 1000;
         pRTInfo->radioInterfacesDbm[iSliceIndex][k].iDbmChangeSpeedMin[j] = 1000;
         pRTInfo->radioInterfacesDbm[iSliceIndex][k].iDbmChangeSpeedMax[j] = 1000;
         pRTInfo->radioInterfacesDbm[iSliceIndex][k].iDbmNoiseLast[j] = 1000;
         pRTInfo->radioInterfacesDbm[iSliceIndex][k].iDbmNoiseMin[j] = 1000;
         pRTInfo->radioInterfacesDbm[iSliceIndex][k].iDbmNoiseMax[j] = 1000;
         pRTInfo->radioInterfacesDbm[iSliceIndex][k].iDbmNoiseAvg[j] = 1000;
      }
   }
}

void controller_rt_info_init(controller_runtime_info* pRTInfo)
{
   if ( NULL == pRTInfo )
      return;

   log_line("controller_runtime_info total size: %d", sizeof(controller_runtime_info));
   log_line("controller_runtime_info dbm size: %d", sizeof(pRTInfo->radioInterfacesDbm));
   memset(pRTInfo, 0, sizeof(controller_runtime_info));
   
   pRTInfo->uUpdateIntervalMs = SYSTEM_RT_INFO_UPDATE_INTERVAL_MS;
   pRTInfo->uCurrentSliceStartTime = 0;
   pRTInfo->iCurrentIndex = 0;
   pRTInfo->iCurrentIndex2 = 0;
   pRTInfo->iCurrentIndex3 = 0;

   for( int i=0; i<SYSTEM_RT_INFO_INTERVALS; i++ )
   {
      pRTInfo->uSliceUpdateTime[i] = 0;
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         pRTInfo->radioInterfacesDbm[i][k].iCountAntennas = 0;
      }
      _controller_runtime_info_reset_dbm_slice(pRTInfo, i);
   }

   pRTInfo->uTotalCountOutputSkippedBlocks = 0;
}

controller_runtime_info_vehicle* controller_rt_info_get_vehicle_info(controller_runtime_info* pRTInfo, u32 uVehicleId)
{
   if ( (NULL == pRTInfo) || (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return NULL;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if (pRTInfo->vehicles[i].uVehicleId == uVehicleId )
         return &(pRTInfo->vehicles[i]);
   }

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( pRTInfo->vehicles[i].uVehicleId == 0 )
      {
         pRTInfo->vehicles[i].uVehicleId = uVehicleId;
         return &(pRTInfo->vehicles[i]);
      }
   }
   return NULL;
}

void controller_rt_info_update_ack_rt_time(controller_runtime_info* pRTInfo, u32 uVehicleId, int iRadioLink, u32 uRoundTripTime)
{
   if ( (NULL == pRTInfo) || (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return;
   if ( (iRadioLink < 0) || (iRadioLink >= MAX_RADIO_INTERFACES) )
      return;

   controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(pRTInfo, uVehicleId);
   if ( NULL == pRTInfoVehicle )
      return;


   pRTInfoVehicle->iAckTimeIndex[iRadioLink]++;
   if ( pRTInfoVehicle->iAckTimeIndex[iRadioLink] >= SYSTEM_RT_INFO_INTERVALS/4 )
      pRTInfoVehicle->iAckTimeIndex[iRadioLink] = 0;
   pRTInfoVehicle->uAckTimes[pRTInfoVehicle->iAckTimeIndex[iRadioLink]][iRadioLink] = uRoundTripTime;

   if ( 0 == pRTInfoVehicle->uMinAckTime[pRTInfo->iCurrentIndex][iRadioLink] )
      pRTInfoVehicle->uMinAckTime[pRTInfo->iCurrentIndex][iRadioLink] = (u8)uRoundTripTime;
   else if ( (u8)uRoundTripTime < pRTInfoVehicle->uMinAckTime[pRTInfo->iCurrentIndex][iRadioLink] )
      pRTInfoVehicle->uMinAckTime[pRTInfo->iCurrentIndex][iRadioLink] = (u8)uRoundTripTime;
   if ( 0 == pRTInfoVehicle->uMaxAckTime[pRTInfo->iCurrentIndex][iRadioLink] )
      pRTInfoVehicle->uMaxAckTime[pRTInfo->iCurrentIndex][iRadioLink] = (u8)uRoundTripTime;
   else if ( (u8)uRoundTripTime > pRTInfoVehicle->uMaxAckTime[pRTInfo->iCurrentIndex][iRadioLink] )
      pRTInfoVehicle->uMaxAckTime[pRTInfo->iCurrentIndex][iRadioLink] = (u8)uRoundTripTime;
}

int controller_rt_info_will_advance_index(controller_runtime_info* pRTInfo, u32 uTimeNowMs)
{
   if ( NULL == pRTInfo )
      return 0;
   if ( uTimeNowMs < (pRTInfo->uCurrentSliceStartTime + pRTInfo->uUpdateIntervalMs) )
      return 0;
   return 1;
}

int controller_rt_info_check_advance_index(controller_runtime_info* pRTInfo, u32 uTimeNowMs)
{
   if ( ! controller_rt_info_will_advance_index(pRTInfo, uTimeNowMs) )
      return 0;

   int iIndex = pRTInfo->iCurrentIndex;

   // ------------------------------------------------
   // Begin Compute derived values

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      pRTInfo->uDbmChangeSpeed[iIndex][i] = 0;
      for( int k=0; k<pRTInfo->radioInterfacesDbm[iIndex][i].iCountAntennas; i++ )
      {
         int iDbmChangeSpeed = pRTInfo->radioInterfacesDbm[iIndex][i].iDbmMax - pRTInfo->radioInterfacesDbm[iIndex][i].iDbmMin;
         if ( iDbmChangeSpeed > pRTInfo->uDbmChangeSpeed[iIndex][i] )
            pRTInfo->uDbmChangeSpeed[iIndex][i] = iDbmChangeSpeed;
      }
   }   
   
   // End Compute derived values
   // ------------------------------------------------
   // Advance index

   pRTInfo->uCurrentSliceStartTime = uTimeNowMs;
   pRTInfo->iCurrentIndex++;
   if ( pRTInfo->iCurrentIndex >= SYSTEM_RT_INFO_INTERVALS )
      pRTInfo->iCurrentIndex = 0;
   pRTInfo->iCurrentIndex2 = pRTInfo->iCurrentIndex;
   pRTInfo->iCurrentIndex3 = pRTInfo->iCurrentIndex;

   // ---------------------------------------------------
   // Reset the new slice

   iIndex = pRTInfo->iCurrentIndex;
   pRTInfo->uSliceUpdateTime[iIndex] = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      pRTInfo->uRxVideoPackets[iIndex][i] = 0;
      pRTInfo->uRxVideoECPackets[iIndex][i] = 0;
      pRTInfo->uRxDataPackets[iIndex][i] = 0;
      pRTInfo->uRxHighPriorityPackets[iIndex][i] = 0;
      pRTInfo->uRxMissingPackets[iIndex][i] = 0;
      pRTInfo->uRxMissingPacketsMaxGap[iIndex][i] = 0;
   }
   pRTInfo->uRxProcessedPackets[iIndex] = 0;
   pRTInfo->uRxMaxAirgapSlots[iIndex] = 0;
   pRTInfo->uRxMaxAirgapSlots2[iIndex] = 0;

   pRTInfo->uTxPackets[iIndex] = 0;
   pRTInfo->uTxHighPriorityPackets[iIndex] = 0;

   pRTInfo->uRecvVideoDataPackets[iIndex] = 0;
   pRTInfo->uRecvVideoECPackets[iIndex] = 0;
   pRTInfo->uRecvFramesInfo[iIndex] = 0;
 
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         pRTInfo->vehicles[i].uMinAckTime[iIndex][k] = 0;
         pRTInfo->vehicles[i].uMaxAckTime[iIndex][k] = 0;
      }
      pRTInfo->vehicles[i].uCountReqRetransmissions[iIndex] = 0;
      pRTInfo->vehicles[i].uCountReqRetrPackets[iIndex] = 0;
      pRTInfo->vehicles[i].uCountAckRetransmissions[iIndex] = 0;
   }
   pRTInfo->uOutputedVideoPackets[iIndex] = 0;
   pRTInfo->uOutputedVideoPacketsRetransmitted[iIndex] = 0;
   pRTInfo->uOutputedVideoPacketsSingleECUsed[iIndex] = 0;
   pRTInfo->uOutputedVideoPacketsTwoECUsed[iIndex] = 0;
   pRTInfo->uOutputedVideoPacketsMultipleECUsed[iIndex] = 0;
   pRTInfo->uOutputedVideoPacketsMaxECUsed[iIndex] = 0;
   pRTInfo->uOutputedVideoPacketsSkippedBlocks[iIndex] = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      pRTInfo->uDbmChangeSpeed[iIndex][i] = 0;
   pRTInfo->uRadioLinkQuality[iIndex] = 0;

   pRTInfo->uFlagsAdaptiveVideo[iIndex] = 0;
   _controller_runtime_info_reset_dbm_slice(pRTInfo, iIndex);
   return 1;
}