/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
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
      g_State.vehiclesRuntimeInfo[iIndex].uTimeLastPingInitiatedToVehicleOnLocalRadioLinks[i] = 0;
      g_State.vehiclesRuntimeInfo[iIndex].uTimeLastPingSentToVehicleOnLocalRadioLinks[i] = 0;
      g_State.vehiclesRuntimeInfo[iIndex].uLastPingIdSentToVehicleOnLocalRadioLinks[i] = 0;
      g_State.vehiclesRuntimeInfo[iIndex].uTimeLastPingReplyReceivedFromVehicleOnLocalRadioLinks[i] = 0;
      g_State.vehiclesRuntimeInfo[iIndex].uLastPingIdReceivedFromVehicleOnLocalRadioLinks[i] = 0;
      g_State.vehiclesRuntimeInfo[iIndex].uPingRoundtripTimeOnLocalRadioLinks[i] = 0;
   }

   g_State.vehiclesRuntimeInfo[iIndex].bIsVehicleLinkToControllerLostAlarm = false;
   g_State.vehiclesRuntimeInfo[iIndex].uLastTimeReceivedAckFromVehicle = 0;
   g_State.vehiclesRuntimeInfo[iIndex].uLastTimeRecvDataFromVehicle = 0;
   g_State.vehiclesRuntimeInfo[iIndex].iVehicleClockDeltaMilisec = 500000000;
   g_State.vehiclesRuntimeInfo[iIndex].uMinimumPingTimeMilisec = MAX_U32;

   g_State.vehiclesRuntimeInfo[iIndex].uTimeLastCommandIdSent = 0;
   g_State.vehiclesRuntimeInfo[iIndex].uLastCommandIdSent = MAX_U32;
   g_State.vehiclesRuntimeInfo[iIndex].uLastCommandIdRetrySent = MAX_U32;
   for( int i=0; i<MAX_RUNTIME_INFO_COMMANDS_RT_TIMES; i++ )
      g_State.vehiclesRuntimeInfo[iIndex].uLastCommandsRoundtripTimesMs[i] = 0;

   g_State.vehiclesRuntimeInfo[iIndex].uAverageCommandRoundtripMiliseconds = MAX_U32;
   g_State.vehiclesRuntimeInfo[iIndex].uMaxCommandRoundtripMiliseconds = MAX_U32;
   g_State.vehiclesRuntimeInfo[iIndex].uMinCommandRoundtripMiliseconds = MAX_U32;
   
   g_State.vehiclesRuntimeInfo[iIndex].uPendingVideoProfileToSet = 0xFF;
   g_State.vehiclesRuntimeInfo[iIndex].uPendingVideoProfileToSetRequestedBy = 0;
   g_State.vehiclesRuntimeInfo[iIndex].uLastTimeSentVideoProfileRequest = 0;
   g_State.vehiclesRuntimeInfo[iIndex].uLastTimeRecvVideoProfileAck = 0;

   g_State.vehiclesRuntimeInfo[iIndex].uPendingKeyFrameToSet = 0;
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
   g_TimeLastVideoParametersOrProfileChanged = g_TimeNow;
}


void removeVehicleRuntimeInfo(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= MAX_CONCURENT_VEHICLES) )
      return;

   log_line("Removing vehicle runtime info index %d, VID %u", iIndex, g_State.vehiclesRuntimeInfo[iIndex].uVehicleId);
   for( int i=iIndex; i<MAX_CONCURENT_VEHICLES-1; i++ )
   {
      memcpy(&(g_State.vehiclesRuntimeInfo[i]), &(g_State.vehiclesRuntimeInfo[i+1]), sizeof(type_global_state_vehicle_runtime_info));
   }
   resetVehicleRuntimeInfo(MAX_CONCURENT_VEHICLES-1);
}

bool isPairingDoneWithVehicle(u32 uVehicleId)
{
   if ( (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return false;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId == 0 )
         continue;
      if ( g_State.vehiclesRuntimeInfo[i].uVehicleId != uVehicleId )
         continue;

      if ( g_State.vehiclesRuntimeInfo[i].bIsPairingDone )
         return true;
      return false;
   }
   return false;
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
}

void adjustLinkClockDeltasForVehicleRuntimeIndex(int iRuntimeInfoIndex, u32 uRoundtripTimeMs, u32 uLocalTimeVehicleMs)
{
   if ( (iRuntimeInfoIndex < 0) || (iRuntimeInfoIndex >= MAX_CONCURENT_VEHICLES) )
      return;
   
   g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].iVehicleClockDeltaMilisec = (int)uLocalTimeVehicleMs - ((int) g_TimeNow - (int)uRoundtripTimeMs/3);

   radio_set_link_clock_delta(g_State.vehiclesRuntimeInfo[iRuntimeInfoIndex].iVehicleClockDeltaMilisec);
}