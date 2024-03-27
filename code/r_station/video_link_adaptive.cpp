/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
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
#include "../base/config.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../radio/radiopacketsqueue.h"
#include "../common/string_utils.h"

#include "shared_vars.h"
#include "timers.h"

#include "video_link_adaptive.h"
#include "video_link_keyframe.h"
#include "processor_rx_video.h"
#include "links_utils.h"

extern t_packet_queue s_QueueRadioPackets;

u32 s_uPauseAdjustmensUntilTime = 0;
u32 s_uTimeStartGoodIntervalForProfileShiftUp = 0;

void video_link_adaptive_init(u32 uVehicleId)
{
   s_uPauseAdjustmensUntilTime = 0;
   log_line("Initialized adaptive video info for VID: %u", uVehicleId);
   video_link_keyframe_init(uVehicleId);
}

void video_link_adaptive_switch_to_med_level(u32 uVehicleId)
{
   video_link_adaptive_init(uVehicleId);

   Model* pModel = findModelWithId(uVehicleId, 140);
   if ( NULL == pModel )
      return;

   int isAdaptiveVideoOn = ((pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
   if ( ! isAdaptiveVideoOn )
      return;

   int iIndex = getVehicleRuntimeIndex(uVehicleId);
   if ( -1 == iIndex )
      return;

   int iLevelsHQ = pModel->get_video_profile_total_levels(pModel->video_params.user_selected_video_link_profile);
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift = iLevelsHQ;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedLevelShift = -1;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastAckLevelShift = 0;
   s_uPauseAdjustmensUntilTime = g_TimeNow + 3000;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastLevelShiftDown = g_TimeNow+3000;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastLevelShiftUp = g_TimeNow+3000;
   s_uTimeStartGoodIntervalForProfileShiftUp = 0;
   log_line("Video adaptive: reset to MQ level for VID %u (name: %s)", uVehicleId, pModel->getLongName());
}

void video_link_adaptive_set_intial_video_adjustment_level(u32 uVehicleId, int iCurrentVideoProfile, u8 uDataPackets, u8 uECPackets)
{
   Model* pModel = findModelWithId(uVehicleId, 141);
   if ( NULL == pModel )
      return;

   log_line("Adaptive video: received initial video stream from vehicle %u: video profile: %d (%s), data/ec: %d/%d",
      uVehicleId, iCurrentVideoProfile, str_get_video_profile_name((u32)iCurrentVideoProfile), uDataPackets, uECPackets);
   if ( iCurrentVideoProfile < 0 || iCurrentVideoProfile >= MAX_VIDEO_LINK_PROFILES )
      iCurrentVideoProfile = pModel->video_params.user_selected_video_link_profile;
    
   int iIndex = getVehicleRuntimeIndex(uVehicleId);
   if ( -1 == iIndex )
      return;

   if ( iCurrentVideoProfile == pModel->video_params.user_selected_video_link_profile )
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift = 0;
   else if ( iCurrentVideoProfile == VIDEO_PROFILE_MQ )
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift = pModel->get_video_profile_total_levels(pModel->video_params.user_selected_video_link_profile);
   else
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift = pModel->get_video_profile_total_levels(pModel->video_params.user_selected_video_link_profile) + pModel->get_video_profile_total_levels(VIDEO_PROFILE_MQ);

   if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift < 0 )
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift = 0;
   
   if ( (int)uECPackets > pModel->video_link_profiles[iCurrentVideoProfile].block_fecs )
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift += ((int)uECPackets - pModel->video_link_profiles[iCurrentVideoProfile].block_fecs);
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedLevelShift = g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedLevelShiftRetryCount = 0;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastLevelShiftDown = g_TimeNow+1000;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastLevelShiftUp = g_TimeNow+1000;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastRequestedLevelShift = g_TimeNow+1000;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastAckLevelShift = 0;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastLevelShiftCheckConsistency = g_TimeNow;
   s_uTimeStartGoodIntervalForProfileShiftUp = 0;
   log_line("Adaptive video did set initial/current adaptive video level to %d, from received video stream for VID %u, last received video profile: %d",
     g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift, uVehicleId, iCurrentVideoProfile);
}

void _video_link_adaptive_check_send_to_vehicle(u32 uVehicleId, bool bForce)
{
   Model* pModel = findModelWithId(uVehicleId, 142);
   if ( NULL == pModel )
      return;

   int iIndex = getVehicleRuntimeIndex(uVehicleId);
   if ( -1 == iIndex )
      return;

   if ( ! bForce )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift == -1 )
         return;
      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift == g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastAcknowledgedLevelShift )
         return;
      if ( g_TimeNow < g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastRequestedLevelShift + g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uUpdateInterval + g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedLevelShiftRetryCount*10 )
         return;
   }

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = uVehicleId;
   PH.total_length = sizeof(t_packet_header) + sizeof(u32) + sizeof(u8);

   u32 uLevel = (u32) g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iCurrentTargetLevelShift;
   u8 uVideoStreamIndex = 0;
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), (u8*)&uLevel, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header) + sizeof(u32), (u8*)&uVideoStreamIndex, sizeof(u8));
   packets_queue_inject_packet_first(&s_QueueRadioPackets, packet);

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].uTimeLastRequestedLevelShift = g_TimeNow;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedLevelShiftRetryCount++;
   if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedLevelShiftRetryCount > 100 )
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iIndex].iLastRequestedLevelShiftRetryCount = 20;

}

void _video_link_adaptive_check_adjust_video_params(u32 uVehicleId)
{
   Model* pModel = findModelWithId(uVehicleId, 143);
   if ( NULL == pModel )
      return;

   int isAdaptiveVideoOn = ((pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
   if ( ! isAdaptiveVideoOn )
      return;

   _video_link_adaptive_check_send_to_vehicle(uVehicleId, false);

   static u32 s_uTimeLastPeriodicResentCurrentAdaptiveLevel = 0;
   if ( ! pModel->isVideoLinkFixedOneWay() )
   if ( g_TimeNow > s_uTimeLastPeriodicResentCurrentAdaptiveLevel + 5000 )
   {
      s_uTimeLastPeriodicResentCurrentAdaptiveLevel = g_TimeNow;
      _video_link_adaptive_check_send_to_vehicle(uVehicleId, true);
   }

   int iVehicleIndex = getVehicleRuntimeIndex(uVehicleId);
   if ( -1 == iVehicleIndex )
      return;

   ProcessorRxVideo* pProcessorVideo = NULL;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL != g_pVideoProcessorRxList[i] )
      if ( g_pVideoProcessorRxList[i]->m_uVehicleId == uVehicleId )
      {
         pProcessorVideo = g_pVideoProcessorRxList[i];
         break;
      }
   }

   if ( g_TimeNow < g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftDown + 50 )
      return;
   if ( g_TimeNow < g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftUp + 50 )
      return;

   int iLevelsHQ = pModel->get_video_profile_total_levels(pModel->video_params.user_selected_video_link_profile);
   int iLevelsMQ = pModel->get_video_profile_total_levels(VIDEO_PROFILE_MQ);
   int iLevelsLQ = pModel->get_video_profile_total_levels(VIDEO_PROFILE_LQ);
   int iMaxLevels = iLevelsHQ;
   iMaxLevels +=  iLevelsMQ;
   if ( ! (pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO) )
      iMaxLevels += iLevelsLQ;

   if ( hardware_board_is_openipc(pModel->hwCapabilities.iBoardType) )
      iMaxLevels = iLevelsHQ;

   float fParamsChangeStrength = (float)pModel->video_params.videoAdjustmentStrength / 10.0;
   
   // When on HQ video profile, switch faster to MQ video profile;
   /*
   if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[0].iCurrentTargetLevelShift < iLevelsHQ )
   {
      fParamsChangeStrength += 0.2;
      if ( fParamsChangeStrength > 1.0 )
         fParamsChangeStrength = 1.0;
   }
   */

   // When in MQ video profile, switch slower to LQ video profile

   if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift > iLevelsHQ )
   {
      fParamsChangeStrength -= 0.1;
      if ( fParamsChangeStrength < 0.1 )
         fParamsChangeStrength = 0.1;
   }

   // Check for video profile received from vehicle missmatch
   if ( g_TimeNow > g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftCheckConsistency + 4000 )
   {
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftCheckConsistency = g_TimeNow;

      bool bHasRecentRxData = false;
      for( int i=0; i<g_SM_RadioStats.countLocalRadioLinks; i++ )
      {
         if ( g_TimeNow > 2000 && g_SM_RadioStats.radio_links[i].timeLastRxPacket >= g_TimeNow-1000 )
         {
            bHasRecentRxData = true;
            break;
         }
      }
      if ( bHasRecentRxData && (NULL != pProcessorVideo) )
      {
         if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift < iLevelsHQ )
         {
            if ( pProcessorVideo->getCurrentlyReceivedVideoProfile() != VIDEO_PROFILE_USER )
            if ( pProcessorVideo->getCurrentlyReceivedVideoProfile() != VIDEO_PROFILE_HIGH_QUALITY )
            if ( pProcessorVideo->getCurrentlyReceivedVideoProfile() != VIDEO_PROFILE_BEST_PERF )
            {
               char szTmp[64];
               strcpy(szTmp, str_get_video_profile_name(pModel->video_params.user_selected_video_link_profile));
               log_softerror_and_alarm("Adaptive video: Missmatch between last requested video profile (%s) and currently received video profile (%s)", szTmp, str_get_video_profile_name(pProcessorVideo->getCurrentlyReceivedVideoProfile()));
               g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastAcknowledgedLevelShift = -1;
               _video_link_adaptive_check_send_to_vehicle(uVehicleId, false);

               u32 uParam = (u32)pProcessorVideo->getCurrentlyReceivedVideoProfile();
               uParam = uParam | (((u32)g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift)<<16);
               if ( pModel->bDeveloperMode )
                  send_alarm_to_central(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_ADAPTIVE_VIDEO_LEVEL_MISSMATCH, uParam );
            }
         }
         else if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift < iLevelsHQ + iLevelsMQ )
         {
            if ( pProcessorVideo->getCurrentlyReceivedVideoProfile() != VIDEO_PROFILE_MQ )
            {
               char szTmp[64];
               strcpy(szTmp, str_get_video_profile_name(VIDEO_PROFILE_MQ));
               log_softerror_and_alarm("Adaptive video: Missmatch between last requested video profile (%s) and currently received video profile (%s)", szTmp, str_get_video_profile_name(pProcessorVideo->getCurrentlyReceivedVideoProfile()));
               g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastAcknowledgedLevelShift = -1;
               _video_link_adaptive_check_send_to_vehicle(uVehicleId, false);

               u32 uParam = (u32)pProcessorVideo->getCurrentlyReceivedVideoProfile();
               uParam = uParam | (((u32)g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift)<<16);
               if ( pModel->bDeveloperMode )
                  send_alarm_to_central(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_ADAPTIVE_VIDEO_LEVEL_MISSMATCH, uParam );
            }
         }
         else if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift < iLevelsHQ + iLevelsMQ + iLevelsLQ )
         {
            if ( pProcessorVideo->getCurrentlyReceivedVideoProfile() != VIDEO_PROFILE_LQ )
            {
               char szTmp[64];
               strcpy(szTmp, str_get_video_profile_name(VIDEO_PROFILE_LQ));
               log_softerror_and_alarm("Adaptive video: Missmatch between last requested video profile (%s) and currently received video profile (%s)", szTmp, str_get_video_profile_name(pProcessorVideo->getCurrentlyReceivedVideoProfile()));
               g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastAcknowledgedLevelShift = -1;
               _video_link_adaptive_check_send_to_vehicle(uVehicleId, false);

               u32 uParam = (u32)pProcessorVideo->getCurrentlyReceivedVideoProfile();
               uParam = uParam | (((u32)g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift)<<16);
               if ( pModel->bDeveloperMode )
                  send_alarm_to_central(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_ADAPTIVE_VIDEO_LEVEL_MISSMATCH, uParam );
            }
         }
      }
   }

   // Max intervals is MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS * 20 ms = 2.4 seconds
   // Minus one because the current index is still processing/invalid

   int iIntervalsToCheckDown = (1.0-0.4*fParamsChangeStrength) * MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS;
   int iIntervalsToCheckUp = (1.0-0.3*fParamsChangeStrength) * MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS;
   if ( iIntervalsToCheckDown >= MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1 )
      iIntervalsToCheckDown = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-2;
   if ( iIntervalsToCheckUp >= MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1 )
      iIntervalsToCheckUp = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-2;

   int iIntervalsSinceLastShiftDown = (g_TimeNow - g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftDown) / g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uUpdateInterval;
   if ( iIntervalsToCheckDown > iIntervalsSinceLastShiftDown )
      iIntervalsToCheckDown = iIntervalsSinceLastShiftDown;
   if ( iIntervalsToCheckDown <= 0 )
      iIntervalsToCheckDown = 1;

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsAdaptive1 = ((u16)iIntervalsToCheckDown) | (((u16)iIntervalsToCheckUp)<<16);
   
   int iCountReconstructedDown = 0;
   int iLongestReconstructionDown = 0;
   int iCountRetransmissionsDown = 0;
   int iCountReRetransmissionsDown = 0;
   int iCountMissingSegmentsDown = 0;

   int iCountReconstructedUp = 0;
   int iLongestReconstructionUp = 0;
   int iCountRetransmissionsUp = 0;
   int iCountReRetransmissionsUp = 0;
   int iCountMissingSegmentsUp = 0;

   int iIndex = g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uCurrentIntervalIndex - 1;
   if ( iIndex < 0 )
      iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;

   int iCurrentReconstructionLength = 0;

   for( int i=0; i<iIntervalsToCheckDown; i++ )
   {
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;
      iCountReconstructedDown += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsOuputRecontructedVideoPackets[iIndex]!=0)?1:0;
      iCountRetransmissionsDown += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsRequestedRetransmissions[iIndex]!=0)?1:0;
      iCountReRetransmissionsDown += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsRetriedRetransmissions[iIndex]!=0)?1:0;
      iCountMissingSegmentsDown += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsMissingVideoPackets[iIndex]!=0)?1:0;
   
      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsOuputRecontructedVideoPackets[iIndex] != 0 )
         iCurrentReconstructionLength++;
      else
      {
         if ( iCurrentReconstructionLength > iLongestReconstructionDown )
            iLongestReconstructionDown = iCurrentReconstructionLength;
         iCurrentReconstructionLength = 0;
      }
   }

   if ( iCurrentReconstructionLength > iLongestReconstructionDown )
      iLongestReconstructionDown = iCurrentReconstructionLength;
   iCurrentReconstructionLength = 0;

   iCountReconstructedUp = iCountReconstructedDown;
   iCountRetransmissionsUp = iCountRetransmissionsDown;
   iCountReRetransmissionsUp = iCountReRetransmissionsDown;
   iCountMissingSegmentsUp = iCountMissingSegmentsDown;
   iLongestReconstructionUp = iLongestReconstructionDown;

   for( int i=iIntervalsToCheckDown; i<iIntervalsToCheckUp; i++ )
   {
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;
      iCountReconstructedUp += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsOuputRecontructedVideoPackets[iIndex]!=0)?1:0;
      iCountRetransmissionsUp += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsRequestedRetransmissions[iIndex]!=0)?1:0;
      iCountReRetransmissionsUp += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsRetriedRetransmissions[iIndex]!=0)?1:0;
      iCountMissingSegmentsUp += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsMissingVideoPackets[iIndex]!=0)?1:0;
   
      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsOuputRecontructedVideoPackets[iIndex] != 0 )
         iCurrentReconstructionLength++;
      else
      {
         if ( iCurrentReconstructionLength > iLongestReconstructionUp )
            iLongestReconstructionUp = iCurrentReconstructionLength;
         iCurrentReconstructionLength = 0;
      }
   }

   if ( iCurrentReconstructionLength > iLongestReconstructionUp )
      iLongestReconstructionUp = iCurrentReconstructionLength;
   iCurrentReconstructionLength = 0;

   bool bTriedAnyShift = false;

   // Check for shift down

   int iThresholdReconstructedDown = 2 + (1.0-fParamsChangeStrength)*iIntervalsToCheckDown*0.9;
   int iThresholdLongestRecontructionDown = 2 + (1.0-fParamsChangeStrength)*iIntervalsToCheckDown*0.7;
   int iThresholdRetransmissionsDown = 1 + (1.0-fParamsChangeStrength)*iIntervalsToCheckDown/8.0;
   
   u32 uMinTimeSinceLastShift = iIntervalsToCheckDown * g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uUpdateInterval;
   if ( iThresholdReconstructedDown * g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uUpdateInterval < uMinTimeSinceLastShift )
      uMinTimeSinceLastShift = iThresholdReconstructedDown * g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uUpdateInterval;
   if ( iThresholdRetransmissionsDown * g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uUpdateInterval < uMinTimeSinceLastShift )
      uMinTimeSinceLastShift = iThresholdRetransmissionsDown * g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uUpdateInterval;

   if ( uMinTimeSinceLastShift < 100 )
      uMinTimeSinceLastShift = 100;
   if ( uMinTimeSinceLastShift > 500 )
      uMinTimeSinceLastShift = 500;
   if ( g_TimeNow > g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftDown + uMinTimeSinceLastShift )
   {
      // To fix : on openIPC reenable lower video profile when majestic bitrate can be changed
      if ( iCountRetransmissionsDown > iThresholdRetransmissionsDown )
      if ( ! hardware_board_is_openipc(pModel->hwCapabilities.iBoardType) )
      {
         bTriedAnyShift = true;
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftDown = g_TimeNow;
         
         // Go directly to next video profile down
         if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift < iLevelsHQ )
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift = iLevelsHQ;
         else if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift < iLevelsHQ + iLevelsMQ)
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift = iLevelsHQ+iLevelsMQ;
         else 
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift = iMaxLevels - 1;

         if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift >= iMaxLevels )
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift = iMaxLevels - 1;
      }

      if ( ! bTriedAnyShift )
      if ( (iCountReconstructedDown > iThresholdReconstructedDown) ||
           (iLongestReconstructionDown > iThresholdLongestRecontructionDown) )
      {
         bTriedAnyShift = true;
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftDown = g_TimeNow;
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift++;
         
         if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift >= iMaxLevels )
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift = iMaxLevels - 1;
      }
   }

   // Check for shift up ?
   // Only if we did not shifted down or up recently

   int iThresholdReconstructedUp = 2 + (1.0-fParamsChangeStrength)*iIntervalsToCheckUp*0.6;
   int iThresholdLongestRecontructionUp = 2 + (1.0-fParamsChangeStrength)*iIntervalsToCheckUp*0.3;
   int iThresholdRetransmissionsUp = 2 + (1.0-fParamsChangeStrength)*iIntervalsToCheckDown/14.0;

   u32 uTimeForShiftUp = 1000 - (500.0*fParamsChangeStrength);
   if ( uTimeForShiftUp < 100 )
      uTimeForShiftUp = 100;
   if ( uTimeForShiftUp > 1000 )
      uTimeForShiftUp = 1000;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsAdaptive2 = ((u32)iThresholdReconstructedUp) | (((u32)iThresholdLongestRecontructionUp)<<8);
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsAdaptive2 |= (((u32)iThresholdReconstructedDown)<<16) | (((u32)iThresholdLongestRecontructionDown)<<24);
   
   if ( g_TimeNow > g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftDown + uTimeForShiftUp )
   if ( g_TimeNow > g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftUp + uTimeForShiftUp )
   {
      if ( iCountReconstructedUp < iThresholdReconstructedUp )
      if ( iLongestReconstructionUp < iThresholdLongestRecontructionUp )
      if ( iCountRetransmissionsUp < iThresholdRetransmissionsUp )
      {
         bTriedAnyShift = true;

         if ( 0 == s_uTimeStartGoodIntervalForProfileShiftUp )
            s_uTimeStartGoodIntervalForProfileShiftUp = g_TimeNow;
         else if ( g_TimeNow >= s_uTimeStartGoodIntervalForProfileShiftUp + DEFAULT_MINIMUM_OK_INTERVAL_MS_TO_SWITCH_VIDEO_PROFILE_UP )
         {
            s_uTimeStartGoodIntervalForProfileShiftUp = 0;
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastLevelShiftUp = g_TimeNow;
            if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift > 0 )
            {
               g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift--;
               // Skip data:fec == 1:1 leves
               if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift == iLevelsHQ-1 )
                  g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift--;
               if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift == iLevelsHQ + iLevelsMQ - 1 )
                  g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift--;
               if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift == iLevelsHQ + iLevelsMQ + iLevelsLQ - 1 )
                  g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift--;
            }
            if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift < 0 )
               g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentTargetLevelShift = 0;
         }
      }
   }
   
   if ( ! bTriedAnyShift )
      s_uTimeStartGoodIntervalForProfileShiftUp = 0;
}

void video_link_adaptive_periodic_loop()
{
   if ( g_bSearching || NULL == g_pCurrentModel || g_bUpdateInProgress )
      return;
   
   if ( g_bDebugState )
      return;
     
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( 0 == g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] )
         continue;
      Model* pModel = findModelWithId(g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i], 144);
      if ( (NULL == pModel) || (pModel->is_spectator) )
         continue;
      if ( ! g_SM_RouterVehiclesRuntimeInfo.iPairingDone[i] )
         continue;
        
      if ( pModel->isVideoLinkFixedOneWay() )
      {
         if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iCurrentTargetLevelShift != 0 )
           g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iCurrentTargetLevelShift = 0;
         if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iLastAcknowledgedLevelShift != 0 )
            _video_link_adaptive_check_send_to_vehicle(g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i], false);
         continue;
      }

      int isAdaptiveVideoOn = ((pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
      int isAdaptiveKeyframe = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].keyframe_ms > 0 ? 0 : 1;

      if ( ! isAdaptiveVideoOn )
      {
          if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iCurrentTargetLevelShift != 0 )
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iCurrentTargetLevelShift = 0;
          if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iLastAcknowledgedLevelShift != 0 )
             _video_link_adaptive_check_send_to_vehicle(g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i], false);
      }

      if ( g_TimeNow < g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uLastUpdateTime + g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uUpdateInterval )
         continue;

      if ( (! isAdaptiveVideoOn) && (! isAdaptiveKeyframe) )
         continue;
   
      // Update info

      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uLastUpdateTime = g_TimeNow;

      // Compute total missing video packets as of now

      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iChangeStrength = pModel->video_params.videoAdjustmentStrength;

      int iMissing = 0;
      for( int k=0; k<MAX_VIDEO_PROCESSORS; k++ )
      {
         if ( NULL == g_pVideoProcessorRxList[k] )
            break;
         if ( pModel->uVehicleId != g_pVideoProcessorRxList[k]->m_uVehicleId )
            continue;
         iMissing = g_pVideoProcessorRxList[k]->getCurrentlyMissingVideoPackets();
         break;
      }
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uIntervalsMissingVideoPackets[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uCurrentIntervalIndex] = (u16) iMissing;

      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uCurrentIntervalIndex++;
      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uCurrentIntervalIndex >= MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS )
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uCurrentIntervalIndex = 0;

      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uIntervalsOuputCleanVideoPackets[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uCurrentIntervalIndex] = 0;
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uIntervalsOuputRecontructedVideoPackets[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uCurrentIntervalIndex] = 0;
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uIntervalsMissingVideoPackets[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uCurrentIntervalIndex] = 0;
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uIntervalsRequestedRetransmissions[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uCurrentIntervalIndex] = 0;
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uIntervalsRetriedRetransmissions[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uCurrentIntervalIndex] = 0;
      
      // Do adaptive video calculations only after we start receiving the video stream
      // (a valid initial last requested level shift is obtained from the received video stream)

      if ( (0 != s_uPauseAdjustmensUntilTime) && (g_TimeNow < s_uPauseAdjustmensUntilTime) )
      {
         _video_link_adaptive_check_send_to_vehicle(g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i], false);
      }
      else if ( isAdaptiveVideoOn )
      {
         if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iCurrentTargetLevelShift != -1 )
            _video_link_adaptive_check_adjust_video_params(g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i]);
      }
   }

   video_link_keyframe_periodic_loop();

   if ( NULL != g_pSM_RouterVehiclesRuntimeInfo )
   if ( (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[0].uCurrentIntervalIndex % 5) == 0 )
      memcpy((u8*)g_pSM_RouterVehiclesRuntimeInfo, (u8*)&g_SM_RouterVehiclesRuntimeInfo, sizeof(shared_mem_router_vehicles_runtime_info));
}
