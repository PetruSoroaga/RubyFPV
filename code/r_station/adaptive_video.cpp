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
#include "../base/config.h"
#include "../base/models_list.h"
#include "../base/shared_mem_controller_only.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopacketsqueue.h"

#include "adaptive_video.h"
#include "test_link_params.h"
#include "shared_vars.h"
#include "shared_vars_state.h"
#include "timers.h"

extern t_packet_queue s_QueueRadioPacketsHighPrio;

u32 s_uTimePauseAdaptiveVideoUntil = 0;

void adaptive_video_pause(u32 uMilisec)
{
   if ( 0 == uMilisec )
   {
      s_uTimePauseAdaptiveVideoUntil = 0;
      log_line("[AdaptiveVideo] Resumed");

   }
   else
   {
      if ( uMilisec > 20000 )
         uMilisec = 20000;
      s_uTimePauseAdaptiveVideoUntil = g_TimeNow + uMilisec;
      log_line("[AdaptiveVideo] Pause for %u milisec", uMilisec);
   }
}

int _adaptive_video_get_lower_video_profile(int iVideoProfile)
{
   if ( iVideoProfile == VIDEO_PROFILE_BEST_PERF ||
        iVideoProfile == VIDEO_PROFILE_HIGH_QUALITY ||
        iVideoProfile == VIDEO_PROFILE_USER )
     return VIDEO_PROFILE_MQ;

   return VIDEO_PROFILE_LQ;
}

int _adaptive_video_get_higher_video_profile(Model* pModel, int iVideoProfile)
{
   if ( iVideoProfile == VIDEO_PROFILE_LQ )
      return VIDEO_PROFILE_MQ;
   return pModel->video_params.user_selected_video_link_profile;
}

void adaptive_video_init()
{
   log_line("[AdaptiveVideo] Init");
}

void adaptive_video_on_new_vehicle(int iRuntimeIndex)
{
  if ( (iRuntimeIndex < 0) || (iRuntimeIndex >= MAX_CONCURENT_VEHICLES) )
     return;

  Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[iRuntimeIndex].uVehicleId, 41);
  if ( (NULL == pModel) || (! pModel->hasCamera()) )
     return;

  if ( pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
  {
     g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingKeyFrameToSet = DEFAULT_VIDEO_AUTO_INITIAL_KEYFRAME_INTERVAL;
     #if defined (HW_PLATFORM_RASPBERRY)
     if ( pModel->isRunningOnOpenIPCHardware() )
        g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingKeyFrameToSet = DEFAULT_VIDEO_KEYFRAME_OIPC_SIGMASTAR;
     #endif
  }
  else
  {
     g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingKeyFrameToSet = pModel->getInitialKeyframeIntervalMs(pModel->video_params.user_selected_video_link_profile);
  }
  log_line("[Adaptive video] Set initial keyframe interval for VID %u to %d ms", pModel->uVehicleId, (int) g_State.vehiclesRuntimeInfo[iRuntimeIndex].uPendingKeyFrameToSet);
}

void _adaptive_video_send_video_profile_to_vehicle(int iVideoProfile, u32 uVehicleId)
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;

   g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= pRuntimeInfo->uPendingVideoProfileToSetRequestedBy;

   if ( g_TimeNow > g_TimeStart+5000 )
   if ( pRuntimeInfo->uLastTimeRecvDataFromVehicle < g_TimeNow-1000 )
   {
      log_line("[AdaptiveVideo] Skip requesting new video profile from vehicle %u (req id: %u): %d (%s) as the link is lost.",
         uVehicleId, pRuntimeInfo->uVideoProfileRequestId, iVideoProfile, str_get_video_profile_name(iVideoProfile));
      return;
   }

   if ( g_TimeNow > g_TimeStart+5000 )
   if ( g_TimeNow < pRuntimeInfo->uLastTimeSentVideoProfileRequest+5 )
   {
      log_line("[AdaptiveVideo] Skip requesting new video profile from vehicle %u (req id: %u): %d (%s) as we just requested it %u ms ago.",
         uVehicleId, pRuntimeInfo->uVideoProfileRequestId, iVideoProfile, str_get_video_profile_name(iVideoProfile), g_TimeNow - pRuntimeInfo->uLastTimeSentVideoProfileRequest );
      return;    
   }
   pRuntimeInfo->uLastTimeSentVideoProfileRequest = g_TimeNow;
   pRuntimeInfo->uVideoProfileRequestId++;
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = uVehicleId;
   PH.total_length = sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8);

   u8 uVideoProfile = iVideoProfile;
   u8 uVideoStreamIndex = 0;
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), (u8*)&(pRuntimeInfo->uVideoProfileRequestId), sizeof(u32));
   memcpy(packet+sizeof(t_packet_header) + sizeof(u32), (u8*)&uVideoProfile, sizeof(u8));
   memcpy(packet+sizeof(t_packet_header) + sizeof(u32) + sizeof(u8), (u8*)&uVideoStreamIndex, sizeof(u8));
   packets_queue_inject_packet_first(&s_QueueRadioPacketsHighPrio, packet);
   log_line("[AdaptiveVideo] Requested new video profile from vehicle %u (req id: %u): %d (%s)",
      uVehicleId, pRuntimeInfo->uVideoProfileRequestId, iVideoProfile, str_get_video_profile_name(iVideoProfile));
}

void adaptive_video_switch_to_video_profile(int iVideoProfile, u32 uVehicleId)
{
   if ( ! isPairingDoneWithVehicle(uVehicleId) )
      return;
   if ( g_bNegociatingRadioLinks )
      return;
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;

   pRuntimeInfo->uPendingVideoProfileToSet = iVideoProfile;
   pRuntimeInfo->uPendingVideoProfileToSetRequestedBy = CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_USER;
   _adaptive_video_send_video_profile_to_vehicle(iVideoProfile, uVehicleId);
}

void adaptive_video_received_video_profile_switch_confirmation(u32 uRequestId, u8 uVideoProfile, u32 uVehicleId, int iInterfaceIndex)
{
   type_global_state_vehicle_runtime_info* pRuntimeInfo = getVehicleRuntimeInfo(uVehicleId);
   if ( NULL == pRuntimeInfo )
      return;
   if ( uVideoProfile == pRuntimeInfo->uPendingVideoProfileToSet )
   {
      pRuntimeInfo->uPendingVideoProfileToSet = 0xFF;
      pRuntimeInfo->uLastTimeRecvVideoProfileAck = g_TimeNow;
   }

   log_line("[AdaptiveVideo] Received video profile change ack from VID %u, req id: %u, new video profile: %d (%s)",
      uVehicleId, uRequestId, uVideoProfile, str_get_video_profile_name(uVideoProfile));
   
   g_SMControllerRTInfo.uFlagsAdaptiveVideo[g_SMControllerRTInfo.iCurrentIndex] |= CTRL_RT_INFO_FLAG_RECV_ACK;

   if ( pRuntimeInfo->uVideoProfileRequestId == uRequestId )
   {
      u32 uDeltaTime = g_TimeNow - pRuntimeInfo->uLastTimeSentVideoProfileRequest;
      controller_rt_info_update_ack_rt_time(&g_SMControllerRTInfo, uVehicleId, g_SM_RadioStats.radio_interfaces[iInterfaceIndex].assignedLocalRadioLinkId, uDeltaTime);
   }
}

bool _adaptive_video_should_switch_lower(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo, shared_mem_video_stream_stats* pSMVideoStreamInfo)
{
   // Adaptive adjustment strength is in: pModel->video_params.videoAdjustmentStrength
   // 1: lowest (slower) adjustment strength;
   // 10: highest (fastest) adjustment strength;

   u32 uTimeToLookBack = 50 + 10 * pModel->video_params.videoAdjustmentStrength;
   int iIntervalsToCheck = 1 + uTimeToLookBack/g_SMControllerRTInfo.uUpdateIntervalMs;
   if ( iIntervalsToCheck >= SYSTEM_RT_INFO_INTERVALS )
      iIntervalsToCheck = SYSTEM_RT_INFO_INTERVALS - 1;
   int iRTInfoIndex = g_SMControllerRTInfo.iCurrentIndex;
   
   int iECScheme = pModel->video_link_profiles[pSMVideoStreamInfo->PHVS.uCurrentVideoLinkProfile].block_fecs;
   
   if ( iECScheme > 0 )
   {
      int iECThreshold = iECScheme-1;
      if (iECThreshold < 1 )
         iECThreshold = 1;
      int iECCountThreshold = 0;
    
      u32 uTime = g_TimeNow;
      while ( iIntervalsToCheck > 0 )
      {
         // Do not go past the last video profile change
         if ( uTime <= pRuntimeInfo->uLastTimeRecvVideoProfileAck )
            break;

         if ( g_SMControllerRTInfo.uOutputedVideoPacketsMaxECUsed[iRTInfoIndex] >= iECScheme )
            iECCountThreshold++;
         if ( g_SMControllerRTInfo.uOutputedVideoPacketsMaxECUsed[iRTInfoIndex] >= iECThreshold )
            iECCountThreshold++;
         else if ( g_SMControllerRTInfo.uOutputedVideoPacketsSkippedBlocks[iRTInfoIndex] > 0 )
            iECCountThreshold+=2;

         // Go to prev (older) slice
         uTime -= g_SMControllerRTInfo.uUpdateIntervalMs;
         iIntervalsToCheck--;
         iRTInfoIndex--;
         if ( iRTInfoIndex < 0 )
            iRTInfoIndex = SYSTEM_RT_INFO_INTERVALS-1;
      }
      if ( iECCountThreshold > (10-pModel->video_params.videoAdjustmentStrength)/2 )
      {
         return true;
      }
   }
   return false;
}

bool _adaptive_video_should_switch_higher(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo, shared_mem_video_stream_stats* pSMVideoStreamInfo)
{
   // Adaptive adjustment strength is in: pModel->video_params.videoAdjustmentStrength
   // 1: lowest (slower) adjustment strength;
   // 10: highest (fastest) adjustment strength;

   u32 uTimeToLookBack = 1000 + 10 * pModel->video_params.videoAdjustmentStrength;
   int iIntervalsToCheck = 1 + uTimeToLookBack/g_SMControllerRTInfo.uUpdateIntervalMs;
   if ( iIntervalsToCheck >= SYSTEM_RT_INFO_INTERVALS )
      iIntervalsToCheck = SYSTEM_RT_INFO_INTERVALS - 1;
   int iRTInfoIndex = g_SMControllerRTInfo.iCurrentIndex;
   
   int iECScheme = pModel->video_link_profiles[pSMVideoStreamInfo->PHVS.uCurrentVideoLinkProfile].block_fecs;
   if ( iECScheme > 0 )
   {
      int iECThreshold = iECScheme-1;
      if (iECThreshold < 1 )
         iECThreshold = 1;

      int iECCountThreshold = 0;
      u32 uTime = g_TimeNow;
      while ( iIntervalsToCheck > 0 )
      {
         if ( g_SMControllerRTInfo.uOutputedVideoPacketsMaxECUsed[iRTInfoIndex] >= iECThreshold )
            iECCountThreshold++;
         else if ( g_SMControllerRTInfo.uOutputedVideoPacketsSkippedBlocks[iRTInfoIndex] > 0 )
            iECCountThreshold+=2;

         // Go to prev (older) slice
         uTime -= g_SMControllerRTInfo.uUpdateIntervalMs;
         iIntervalsToCheck--;
         iRTInfoIndex--;
         if ( iRTInfoIndex < 0 )
            iRTInfoIndex = SYSTEM_RT_INFO_INTERVALS-1;
      }

      if ( iECCountThreshold < (10-pModel->video_params.videoAdjustmentStrength)/3 + 1 )
      {
         return true; 
      }
   }
   return false;
}

void _adaptive_video_check_vehicle(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo, shared_mem_video_stream_stats* pSMVideoStreamInfo)
{
   if ( (NULL == pRuntimeInfo) || (NULL == pSMVideoStreamInfo) || (NULL == pModel) )
      return;

   // Adaptive adjustment strength is in: pModel->video_params.videoAdjustmentStrength
   // 1: lowest (slower) adjustment strength;
   // 10: highest (fastest) adjustment strength;

   int iLowestProfile = VIDEO_PROFILE_LQ;
   if ( (pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
      iLowestProfile = VIDEO_PROFILE_MQ;

   if ( pSMVideoStreamInfo->PHVS.uCurrentVideoLinkProfile != iLowestProfile )
   if ( pRuntimeInfo->uPendingVideoProfileToSet != iLowestProfile )
   if ( pRuntimeInfo->uLastTimeSentVideoProfileRequest < g_TimeNow - 30 )
   if ( _adaptive_video_should_switch_lower(pModel, pRuntimeInfo, pSMVideoStreamInfo) )
   {
      int iVideoProfile = _adaptive_video_get_lower_video_profile(pSMVideoStreamInfo->PHVS.uCurrentVideoLinkProfile);
      pRuntimeInfo->uPendingVideoProfileToSet = iVideoProfile;
      pRuntimeInfo->uPendingVideoProfileToSetRequestedBy = CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_ADAPTIVE_LOWER;
      _adaptive_video_send_video_profile_to_vehicle(iVideoProfile, pModel->uVehicleId);
   }

   u32 uMinTimeToSwitchHigher = 3000 + (10 - pModel->video_params.videoAdjustmentStrength) * 400;
   if ( pSMVideoStreamInfo->PHVS.uCurrentVideoLinkProfile != pModel->video_params.user_selected_video_link_profile )
   if ( pRuntimeInfo->uPendingVideoProfileToSet != pModel->video_params.user_selected_video_link_profile )
   if ( g_TimeNow > g_TimeStart + uMinTimeToSwitchHigher )
   if ( pRuntimeInfo->uLastTimeSentVideoProfileRequest < g_TimeNow - uMinTimeToSwitchHigher )
   if ( _adaptive_video_should_switch_higher(pModel, pRuntimeInfo, pSMVideoStreamInfo) )
   {
      int iVideoProfile = _adaptive_video_get_higher_video_profile(pModel, pSMVideoStreamInfo->PHVS.uCurrentVideoLinkProfile);
      pRuntimeInfo->uPendingVideoProfileToSet = iVideoProfile;
      pRuntimeInfo->uPendingVideoProfileToSetRequestedBy = CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_ADAPTIVE_HIGHER;
      _adaptive_video_send_video_profile_to_vehicle(iVideoProfile, pModel->uVehicleId);
   }
}

void _adaptive_keyframe_check_vehicle(Model* pModel, type_global_state_vehicle_runtime_info* pRuntimeInfo, shared_mem_video_stream_stats* pSMVideoStreamInfo)
{
   if ( (NULL == pRuntimeInfo) || (NULL == pSMVideoStreamInfo) || (NULL == pModel) )
      return;

   if ( pRuntimeInfo->uPendingKeyFrameToSet == 0 )
      return;

   if ( pSMVideoStreamInfo->PHVS.uCurrentVideoKeyframeIntervalMs == pRuntimeInfo->uPendingKeyFrameToSet )
   {
      pRuntimeInfo->uPendingKeyFrameToSet = 0;
      return;
   }

   pRuntimeInfo->uVideoKeyframeRequestId++;
   
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = pModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + sizeof(u32) + 2*sizeof(u8);

   u32 uKeyframeMs = pRuntimeInfo->uPendingKeyFrameToSet;
   u8 uVideoStreamIndex = 0;

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), (u8*)&(pRuntimeInfo->uVideoKeyframeRequestId), sizeof(u8));
   memcpy(packet+sizeof(t_packet_header) + sizeof(u8), (u8*)&uKeyframeMs, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header) + sizeof(u8) + sizeof(u32), (u8*)&uVideoStreamIndex, sizeof(u8));
   packets_queue_inject_packet_first(&s_QueueRadioPacketsHighPrio, packet);
   log_line("[AdaptiveVideo] Requested new video keyframe from vehicle %u (req id: %u): %u ms",
      pModel->uVehicleId, pRuntimeInfo->uVideoKeyframeRequestId, uKeyframeMs);
}

void adaptive_video_periodic_loop(bool bForceSyncNow)
{
   if ( (g_TimeNow < g_TimeStart + 4000) || g_bNegociatingRadioLinks || test_link_is_in_progress() )
      return;

   if ( 0 != s_uTimePauseAdaptiveVideoUntil )
   {
      if ( g_TimeNow < s_uTimePauseAdaptiveVideoUntil )
         return;
      log_line("[AdaptiveVideo] Resumed after pause.");
      s_uTimePauseAdaptiveVideoUntil = 0;
   }
   
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (g_State.vehiclesRuntimeInfo[i].uVehicleId == 0) || (g_State.vehiclesRuntimeInfo[i].uVehicleId == MAX_U32) )
         continue;
      if ( ! g_State.vehiclesRuntimeInfo[i].bIsPairingDone )
         continue;
      type_global_state_vehicle_runtime_info* pRuntimeInfo = &(g_State.vehiclesRuntimeInfo[i]);
      Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[i].uVehicleId, 28);

      if ( (NULL == pModel) || pModel->isVideoLinkFixedOneWay() || (! pModel->hasCamera()) || (get_sw_version_build(pModel) < 242) )
         continue;

      shared_mem_video_stream_stats* pSMVideoStreamInfo = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, g_State.vehiclesRuntimeInfo[i].uVehicleId);
      ProcessorRxVideo* pProcessorRxVideo = ProcessorRxVideo::getVideoProcessorForVehicleId(pModel->uVehicleId, 0);

      // If haven't received yet the video stream, just skip
      if ( (NULL == pSMVideoStreamInfo) || (NULL == pProcessorRxVideo) )
         continue;

      if ( (pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK )
         _adaptive_video_check_vehicle(pModel, pRuntimeInfo, pSMVideoStreamInfo);

      if ( (pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
         _adaptive_keyframe_check_vehicle(pModel, pRuntimeInfo, pSMVideoStreamInfo);

      if ( pSMVideoStreamInfo->PHVS.uCurrentVideoLinkProfile == pRuntimeInfo->uPendingVideoProfileToSet )
         pRuntimeInfo->uPendingVideoProfileToSet = 0xFF;

      if ( pRuntimeInfo->uPendingVideoProfileToSet == 0xFF )
         continue;

      // Use last rx time to back off on link lost
      if ( bForceSyncNow || (g_TimeNow >= pRuntimeInfo->uLastTimeSentVideoProfileRequest+10) )
      {
         if ( bForceSyncNow )
            pRuntimeInfo->uLastTimeSentVideoProfileRequest = g_TimeNow-50;
         _adaptive_video_send_video_profile_to_vehicle(pRuntimeInfo->uPendingVideoProfileToSet, pRuntimeInfo->uVehicleId);
      }
   }
}