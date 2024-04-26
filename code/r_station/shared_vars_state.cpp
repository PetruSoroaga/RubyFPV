/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "../base/base.h"
#include "../radio/radiolink.h"
#include "shared_vars.h"
#include "timers.h"

type_global_state_station g_State;

void resetVehicleRuntimeInfo(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= MAX_CONCURENT_VEHICLES) )
      return;

   log_line("Reset vehicle runtime info for vehicle runtime index %d, VID: %u", iIndex, g_State.vehiclesRuntimeInfo[iIndex].uVehicleId);

   g_State.vehiclesRuntimeInfo[iIndex].uVehicleId = 0;
   resetPairingStateForVehicleRuntimeInfo(iIndex);

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_State.vehiclesRuntimeInfo[iIndex].uPingRoundtripTimeOnLocalRadioLinks[i] = MAX_U32;
      for( int k=0; k<MAX_RUNTIME_INFO_PINGS_HISTORY; k++ )
      {
         g_State.vehiclesRuntimeInfo[iIndex].uTimeLastPingSentToVehicleOnLocalRadioLinks[i][k] = 0;
         g_State.vehiclesRuntimeInfo[iIndex].uLastPingIdSentToVehicleOnLocalRadioLinks[i][k] = 0;
      }
      g_State.vehiclesRuntimeInfo[iIndex].uTimeLastPingReplyReceivedFromVehicleOnLocalRadioLinks[i] = 0;
      g_State.vehiclesRuntimeInfo[iIndex].uLastPingIdReceivedFromVehicleOnLocalRadioLinks[i] = 0;
   
      for( int k=0; k<MAX_RUNTIME_INFO_LINKS_RT_TIMES; k++ )
         g_State.vehiclesRuntimeInfo[iIndex].uLastLinkRoundtripTimesMs[i][k] = 10000;

      g_State.vehiclesRuntimeInfo[iIndex].uRadioLinkRoundtripLastComputedTime[i] = 0;
      g_State.vehiclesRuntimeInfo[iIndex].uRadioLinkRoundtripMsLast[i] = MAX_U32;
      g_State.vehiclesRuntimeInfo[iIndex].uRadioLinkRoundtripMsAvg[i] = MAX_U32;
      g_State.vehiclesRuntimeInfo[iIndex].uRadioLinkRoundtripMsMin[i] = MAX_U32;
      g_State.vehiclesRuntimeInfo[iIndex].uRadioLinkRoundtripMsMax[i] = MAX_U32;
   }

   g_State.vehiclesRuntimeInfo[iIndex].uRadioLinksMinimumRoundtripMs = MAX_U32;
   g_State.vehiclesRuntimeInfo[iIndex].iVehicleClockIsBehindThisMilisec = MAX_U32;

   g_State.vehiclesRuntimeInfo[iIndex].uTimeLastCommandIdSent = 0;
   g_State.vehiclesRuntimeInfo[iIndex].uLastCommandIdSent = MAX_U32;
   g_State.vehiclesRuntimeInfo[iIndex].uLastCommandIdRetrySent = MAX_U32;
   for( int i=0; i<MAX_RUNTIME_INFO_COMMANDS_RT_TIMES; i++ )
      g_State.vehiclesRuntimeInfo[iIndex].uLastCommandsRoundtripTimesMs[i] = 0;

   g_State.vehiclesRuntimeInfo[iIndex].uAverageCommandRoundtripMiliseconds = MAX_U32;
   g_State.vehiclesRuntimeInfo[iIndex].uMaxCommandRoundtripMiliseconds = MAX_U32;
   g_State.vehiclesRuntimeInfo[iIndex].uMinCommandRoundtripMiliseconds = MAX_U32;
   g_State.vehiclesRuntimeInfo[iIndex].bReceivedKeyframeInfoInVideoStream = false;
   
   // Reset shared mem adaptive info

   memset(&(g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex]), 0, sizeof(shared_mem_controller_adaptive_video_info_vehicle));

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uUpdateInterval = CONTROLLER_ADAPTIVE_VIDEO_SAMPLE_INTERVAL;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uLastUpdateTime = 0;

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift = -1; // Wait for video stream to decide the initial value
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedLevelShift = -1;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedLevelShiftRetryCount = 0;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastAckLevelShift = 0;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastLevelShiftCheckConsistency = g_TimeNow;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_SM_RouterVehiclesRuntimeInfo.uPingRoundtripTimeVehiclesOnLocalRadioLinks[iIndex][i] = MAX_U32;
      g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMsLastTime[iIndex][i] = 0;
      g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMs[iIndex][i] = MAX_U32;
      g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMsMin[iIndex][i] = MAX_U32;
   }

   g_SM_RouterVehiclesRuntimeInfo.uAverageCommandRoundtripMiliseconds[iIndex] = MAX_U32;
   g_SM_RouterVehiclesRuntimeInfo.uMaxCommandRoundtripMiliseconds[iIndex] = MAX_U32;
   g_SM_RouterVehiclesRuntimeInfo.uMinCommandRoundtripMiliseconds[iIndex] = MAX_U32;
}


void resetPairingStateForVehicleRuntimeInfo(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= MAX_CONCURENT_VEHICLES) )
      return;
   log_line("Reset pairing state for vehicle runtime index %d, VID %u", iIndex, g_State.vehiclesRuntimeInfo[iIndex].uVehicleId);
   g_State.vehiclesRuntimeInfo[iIndex].bIsPairingDone = false;
   g_State.vehiclesRuntimeInfo[iIndex].uPairingRequestId = 0;
   g_State.vehiclesRuntimeInfo[iIndex].uPairingRequestTime = g_TimeNow;
   g_State.vehiclesRuntimeInfo[iIndex].uPairingRequestInterval = 200;
}


void removeVehicleRuntimeInfo(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= MAX_CONCURENT_VEHICLES) )
      return;

   log_line("Removing vehicle runtime info index %d, VID %u", iIndex, g_State.vehiclesRuntimeInfo[iIndex].uVehicleId);
   for( int i=iIndex; i<MAX_CONCURENT_VEHICLES-1; i++ )
   {
      memcpy(&(g_State.vehiclesRuntimeInfo[i]), &(g_State.vehiclesRuntimeInfo[i+1]), sizeof(type_global_state_vehicle_runtime_info));
      
      // Move shared mem vehicles info

      memcpy(&(g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i]), &(g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i+1]), sizeof(shared_mem_controller_adaptive_video_info_vehicle));
      g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] = g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i+1];
      g_SM_RouterVehiclesRuntimeInfo.uAverageCommandRoundtripMiliseconds[i] = g_SM_RouterVehiclesRuntimeInfo.uAverageCommandRoundtripMiliseconds[i+1];
      g_SM_RouterVehiclesRuntimeInfo.uMaxCommandRoundtripMiliseconds[i] = g_SM_RouterVehiclesRuntimeInfo.uMaxCommandRoundtripMiliseconds[i+1];
      g_SM_RouterVehiclesRuntimeInfo.uMinCommandRoundtripMiliseconds[i] = g_SM_RouterVehiclesRuntimeInfo.uMinCommandRoundtripMiliseconds[i+1];
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      {
         g_SM_RouterVehiclesRuntimeInfo.uPingRoundtripTimeVehiclesOnLocalRadioLinks[i][k] = g_SM_RouterVehiclesRuntimeInfo.uPingRoundtripTimeVehiclesOnLocalRadioLinks[i+1][k];
         g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMsLastTime[i][k] = g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMsLastTime[i+1][k];
         g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMs[i][k] = g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMs[i+1][k];
         g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMsMin[i][k] = g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMsMin[i+1][k];
      }
   }
   resetVehicleRuntimeInfo(MAX_CONCURENT_VEHICLES-1);
}

int getVehicleRuntimeIndex(u32 uVehicleId)
{
   if ( 0 == uVehicleId )
      return -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         return i;
   }
   return -1;
}

type_global_state_vehicle_runtime_info* getVehicleRuntimeInfo(u32 uVehicleId)
{
   if ( 0 == uVehicleId )
      return NULL;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
         return &(g_State.vehiclesRuntimeInfo[i]);
   }
   return NULL; 
}

void logCurrentVehiclesRuntimeInfo()
{
   char szBuff[256];
   int iCount = 0;
   szBuff[0] = 0;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == 0 )
         continue;

      iCount++;
      char szTmp[32];
      sprintf(szTmp, " %u", g_State.vehiclesRuntimeInfo[i].uVehicleId);
      strcat(szBuff, szTmp);
   }
   log_line("Currently known runtime vehicle IDs (%d):%s", iCount, szBuff);
}

void addCommandRTTimeToRuntimeInfo(type_global_state_vehicle_runtime_info* pRuntimeInfo, u32 uRoundtripTimeMs)
{
   if ( NULL == pRuntimeInfo )
      return;

   u32 avg = 0;
   int iCount = 0;
   for( int i=0; i<MAX_RUNTIME_INFO_COMMANDS_RT_TIMES-1; i++ )
   {
      pRuntimeInfo->uLastCommandsRoundtripTimesMs[i] = pRuntimeInfo->uLastCommandsRoundtripTimesMs[i+1];
      if ( 0 != pRuntimeInfo->uLastCommandsRoundtripTimesMs[i] )
      {
         avg += pRuntimeInfo->uLastCommandsRoundtripTimesMs[i];
         iCount++;
      }
   }
   pRuntimeInfo->uLastCommandsRoundtripTimesMs[MAX_RUNTIME_INFO_COMMANDS_RT_TIMES-1] = uRoundtripTimeMs;
   avg += uRoundtripTimeMs;
   iCount++;
   avg /= iCount;

   pRuntimeInfo->uAverageCommandRoundtripMiliseconds = avg;

   if ( pRuntimeInfo->uMaxCommandRoundtripMiliseconds == MAX_U32 )
      pRuntimeInfo->uMaxCommandRoundtripMiliseconds = uRoundtripTimeMs;
   if ( pRuntimeInfo->uAverageCommandRoundtripMiliseconds > pRuntimeInfo->uMaxCommandRoundtripMiliseconds )
      pRuntimeInfo->uMaxCommandRoundtripMiliseconds = pRuntimeInfo->uAverageCommandRoundtripMiliseconds;

   if ( pRuntimeInfo->uAverageCommandRoundtripMiliseconds < pRuntimeInfo->uMinCommandRoundtripMiliseconds )
      pRuntimeInfo->uMinCommandRoundtripMiliseconds = pRuntimeInfo->uAverageCommandRoundtripMiliseconds;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == 0 )
         break;
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId != pRuntimeInfo->uVehicleId )
         continue;
      g_SM_RouterVehiclesRuntimeInfo.uAverageCommandRoundtripMiliseconds[i] = pRuntimeInfo->uAverageCommandRoundtripMiliseconds;
      g_SM_RouterVehiclesRuntimeInfo.uMaxCommandRoundtripMiliseconds[i] = pRuntimeInfo->uMaxCommandRoundtripMiliseconds;
      g_SM_RouterVehiclesRuntimeInfo.uMinCommandRoundtripMiliseconds[i] = pRuntimeInfo->uMinCommandRoundtripMiliseconds;
      break;
   }
}

void addLinkRTTimeToRuntimeInfoIndex(int iRuntimeInfoIndex, int iLocalRadioLink, u32 uRoundtripTimeMs, u32 uLocalTimeVehicleMs)
{
   if ( (iRuntimeInfoIndex < 0) || (iRuntimeInfoIndex >= MAX_CONCURENT_VEHICLES) )
      return;
   if ( (iLocalRadioLink < 0) || (iLocalRadioLink >= MAX_RADIO_INTERFACES) )
      return;

   u32 avg = 0;
   for( int i=0; i<MAX_RUNTIME_INFO_LINKS_RT_TIMES-1; i++ )
   {
      g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uLastLinkRoundtripTimesMs[iLocalRadioLink][i] = g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uLastLinkRoundtripTimesMs[iLocalRadioLink][i+1];
      avg += g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uLastLinkRoundtripTimesMs[iLocalRadioLink][i];
   }
   g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uLastLinkRoundtripTimesMs[iLocalRadioLink][MAX_RUNTIME_INFO_LINKS_RT_TIMES-1] = uRoundtripTimeMs;
   avg += uRoundtripTimeMs;
   avg /= MAX_RUNTIME_INFO_LINKS_RT_TIMES;


   g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsLast[iLocalRadioLink] = uRoundtripTimeMs;
   
   if ( g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsMax[iLocalRadioLink] == MAX_U32 )
      g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsMax[iLocalRadioLink] = uRoundtripTimeMs;
   else if ( uRoundtripTimeMs > g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsMax[iLocalRadioLink] )
      g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsMax[iLocalRadioLink] = uRoundtripTimeMs;

   if ( g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsMin[iLocalRadioLink] == MAX_U32 )
      g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsMin[iLocalRadioLink] = uRoundtripTimeMs;
   else if ( uRoundtripTimeMs < g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsMin[iLocalRadioLink] )
      g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsMin[iLocalRadioLink] = uRoundtripTimeMs;

   g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsAvg[iLocalRadioLink] = (avg*3 + uRoundtripTimeMs)/4;
   g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripLastComputedTime[iLocalRadioLink] = g_TimeNow;

   if ( g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinksMinimumRoundtripMs == MAX_U32 )
   {
      g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinksMinimumRoundtripMs = uRoundtripTimeMs;
      g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].iVehicleClockIsBehindThisMilisec = (int) get_current_timestamp_ms() - (int)uRoundtripTimeMs/2 - (int)uLocalTimeVehicleMs;
   }
   else if ( uRoundtripTimeMs < g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinksMinimumRoundtripMs )
   {
      g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinksMinimumRoundtripMs = uRoundtripTimeMs;
      g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].iVehicleClockIsBehindThisMilisec = (int) get_current_timestamp_ms() - (int)uRoundtripTimeMs/2 - (int)uLocalTimeVehicleMs;
   }
   
   radio_set_link_clock_delta(g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].iVehicleClockIsBehindThisMilisec);

   g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMsLastTime[iRuntimeInfoIndex][iLocalRadioLink] = g_TimeNow;
   g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMs[iRuntimeInfoIndex][iLocalRadioLink] = g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsAvg[iLocalRadioLink] ;
   g_SM_RouterVehiclesRuntimeInfo.uRadioLinksDelayRoundtripMsMin[iRuntimeInfoIndex][iLocalRadioLink] = g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].uRadioLinkRoundtripMsMin[iLocalRadioLink];
}