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
#include "../common/string_utils.h"
#include "../common/relay_utils.h"

#include "processor_tx_video.h"
#include "utils_vehicle.h"
#include "ruby_rt_vehicle.h"
#include "packets_utils.h"
#include "shared_vars.h"
#include "timers.h"

#include "video_link_auto_keyframe.h"

int s_iRequestedKeyFrameIntervalMsFromController = -1;
u32 s_uLastTimeControllerRequestedAKeyFrameIntervalMs = 0;

int s_iLastRXQualityMinimumForKeyFrame = -1;
int s_iLastRXQualityMaximumForKeyFrame = -1;

u32 s_uLastTimeKeyFrameMovedDown = 0;
u32 s_uLastTimeKeyFrameMovedUp = 0;

void video_link_auto_keyframe_set_controller_requested_value(int iVideoStreamIndex, int iKeyframeMs)
{
   log_line("[Video] Set requested keyframe interval from controller to %d ms. Previous was %d ms", iKeyframeMs, s_iRequestedKeyFrameIntervalMsFromController);

   if ( NULL != g_pProcessorTxVideo )
      g_pProcessorTxVideo->setLastRequestedKeyframeFromController(iKeyframeMs);
   s_iRequestedKeyFrameIntervalMsFromController = iKeyframeMs;
   s_uLastTimeControllerRequestedAKeyFrameIntervalMs = g_TimeNow;
}

void video_link_auto_keyframe_init()
{
   s_iRequestedKeyFrameIntervalMsFromController = -1;
   s_uLastTimeControllerRequestedAKeyFrameIntervalMs = 0;

   log_line("Video auto keyframe module init complete.");
}

void video_link_auto_keyframe_periodic_loop()
{
   if ( (g_pCurrentModel == NULL) || (! g_pCurrentModel->hasCamera()) )
      return;

   if ( (g_TimeNow < g_TimeStart + 5000) ||
        (g_TimeNow < get_video_capture_start_program_time() + 3000) )
      return;
   
   static u32 s_uTimeLastVideoAutoKeyFrameCheck = 0;
   if ( g_TimeNow < s_uTimeLastVideoAutoKeyFrameCheck+10 )
      return;

   s_uTimeLastVideoAutoKeyFrameCheck = g_TimeNow;

   // Is relaying a vehicle and no own video is needed now?
   // Then go to a fast keyframe interval so that relay video switching happens fast

   if ( ! relay_vehicle_must_forward_own_video(g_pCurrentModel) )
   {
      s_iRequestedKeyFrameIntervalMsFromController = DEFAULT_VIDEO_KEYFRAME_INTERVAL_WHEN_RELAYING;
  
      if ( g_SM_VideoLinkStats.overwrites.uCurrentKeyframeMs != DEFAULT_VIDEO_KEYFRAME_INTERVAL_WHEN_RELAYING)
      {
         log_line("[KeyFrame] Set default relay keframe level (%u ms) due to relaying other vehicle (VID %u)", DEFAULT_VIDEO_KEYFRAME_INTERVAL_WHEN_RELAYING, g_pCurrentModel->relay_params.uRelayedVehicleId);
         process_data_tx_video_set_current_keyframe_interval(DEFAULT_VIDEO_KEYFRAME_INTERVAL_WHEN_RELAYING, "relaying remote vehicle video");
      }
      return;
   }
   
   // Fixed keyframe interval, set by user? Then do nothing, just make sure it's the current one

   if ( (NULL != g_pCurrentModel) && g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile >= 0 && g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile < MAX_VIDEO_LINK_PROFILES )
   if ( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe_ms > 0 )
   {
      if ( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe_ms == g_SM_VideoLinkStats.overwrites.uCurrentKeyframeMs )
         return;
      process_data_tx_video_set_current_keyframe_interval( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe_ms, "fixed kf set by user" );
      return;
   }

   // Compute highest RX quality link

   int iLowestRXQuality = 1000;
   int iHighestRXQuality = 0;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);

      if ( (NULL != pRadioInfo) && (pRadioInfo->isHighCapacityInterface) )
      if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != -1 )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].rxQuality < iLowestRXQuality )
            iLowestRXQuality = g_SM_RadioStats.radio_interfaces[i].rxQuality;
         if ( g_SM_RadioStats.radio_interfaces[i].rxQuality > iHighestRXQuality )
            iHighestRXQuality = g_SM_RadioStats.radio_interfaces[i].rxQuality;
      }
   }

   // Have recent link from controller ok? Then do nothing.

   int iThresholdControllerLinkMs = 1000;
   if ( g_TimeNow > g_TimeStart + 5000 )
   if ( g_TimeNow > get_video_capture_start_program_time() + 3000 )
   if ( g_TimeLastReceivedRadioPacketFromController > g_TimeNow - iThresholdControllerLinkMs )
   if ( iHighestRXQuality >= 20 )
   {

      if ( -1 != s_iRequestedKeyFrameIntervalMsFromController )
      if ( s_iRequestedKeyFrameIntervalMsFromController != g_SM_VideoLinkStats.overwrites.uCurrentKeyframeMs )
      {
         log_line("[KeyFrame]: Current keyframe (%d ms) different from one requested by controller (%d ms). Switching to it.", g_SM_VideoLinkStats.overwrites.uCurrentKeyframeMs, s_iRequestedKeyFrameIntervalMsFromController);
         process_data_tx_video_set_current_keyframe_interval(s_iRequestedKeyFrameIntervalMsFromController, "requested by controller");
      }

      return;
   }

   // Adaptive: need to go to lowest level?

   int iNewKeyframeIntervalMs = -1;

   bool bGoToLowestLevel = false;

   if ( (! g_bReceivedPairingRequest) && (!g_bHadEverLinkToController) )
      bGoToLowestLevel = true;
   if ( g_TimeLastReceivedRadioPacketFromController < g_TimeNow - iThresholdControllerLinkMs )
      bGoToLowestLevel = true;
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
      bGoToLowestLevel = true;
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
   if ( g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel >= g_pCurrentModel->get_video_profile_total_levels(VIDEO_PROFILE_MQ) )
      bGoToLowestLevel = true;

   if ( bGoToLowestLevel )
   {
      iNewKeyframeIntervalMs = DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL;
      process_data_tx_video_set_current_keyframe_interval(iNewKeyframeIntervalMs, "adaptive go lowest");
      return;
   }

   // Adaptive keyframe interval

   if ( iHighestRXQuality < 20 )
       iNewKeyframeIntervalMs = DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL;
   
   // RX Quality going down ?

   //if ( iHighestRXQuality < s_iLastRXQualityMaximumForKeyFrame )
   {
      if ( iHighestRXQuality < 70 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedDown + 300 )
      {
         iNewKeyframeIntervalMs = g_SM_VideoLinkStats.overwrites.uCurrentKeyframeMs/2;
         if ( iNewKeyframeIntervalMs < 2 * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL )
            iNewKeyframeIntervalMs = 2 * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL;
         s_uLastTimeKeyFrameMovedDown = g_TimeNow;
      }

      if ( iHighestRXQuality < 50 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedDown + 300 )
      {
         iNewKeyframeIntervalMs = DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL;
         s_uLastTimeKeyFrameMovedDown = g_TimeNow;
      }
   }

   // RX Quality going up ?

   //if ( iHighestRXQuality > s_iLastRXQualityMaximumForKeyFrame )
   {
      if ( iHighestRXQuality > 70 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedDown + 400 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedUp + 400 )
      {
         iNewKeyframeIntervalMs = g_SM_VideoLinkStats.overwrites.uCurrentKeyframeMs*2;
         s_uLastTimeKeyFrameMovedUp = g_TimeNow;
      }

      if ( iHighestRXQuality > 90 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedDown + 200 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedUp + 200 )
      {
         iNewKeyframeIntervalMs = g_SM_VideoLinkStats.overwrites.uCurrentKeyframeMs*4;
         s_uLastTimeKeyFrameMovedUp = g_TimeNow;
      }
   }

   s_iLastRXQualityMinimumForKeyFrame = iLowestRXQuality;
   s_iLastRXQualityMaximumForKeyFrame = iHighestRXQuality;


   // Apply the new value, if it changed
   if ( iNewKeyframeIntervalMs != -1 )
   {
      process_data_tx_video_set_current_keyframe_interval(iNewKeyframeIntervalMs, "adaptive");
   }
}
