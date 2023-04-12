/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../common/string_utils.h"

#include "processor_tx_video.h"
#include "utils_vehicle.h"
#include "packets_utils.h"
#include "shared_vars.h"
#include "timers.h"

#include "video_link_auto_keyframe.h"

static int s_iControllerRequestedKeyFrameInterval = -1;
static u32 s_uLastTimeControllerRequestedKeyFrameInterval = 0;

static int s_iLastRXQualityMinimumKeyFrame = -1;
static int s_iLastRXQualityMaximumKeyFrame = -1;

static u32 s_uLastTimeKeyFrameMovedDown = 0;
static u32 s_uLastTimeKeyFrameMovedUp = 0;


void video_link_auto_keyframe_set_controller_requested_value(int iKeyframe)
{
   s_iControllerRequestedKeyFrameInterval = iKeyframe;
   s_uLastTimeControllerRequestedKeyFrameInterval = g_TimeNow;
}

void video_link_auto_keyframe_init()
{
   s_iControllerRequestedKeyFrameInterval = -1;
   s_uLastTimeControllerRequestedKeyFrameInterval = 0;

   log_line("Video auto keyframe module init complete.");
}

void video_link_auto_keyframe_periodic_loop()
{
   if ( (g_pCurrentModel == NULL) || (! g_pCurrentModel->hasCamera()) )
      return;

   if ( (g_TimeNow < g_TimeStart + 5000) ||
        (g_TimeNow < g_TimeStartRaspiVid + 3000) )
      return;
   
   int iCurrentFPS = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].fps;

   // Fixed keyframe interval, set by user? Then do nothing, just make sure it's the current one

   if ( (NULL != g_pCurrentModel) && g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile >= 0 && g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile < MAX_VIDEO_LINK_PROFILES )
   if ( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe > 0 )
   {
      if ( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe == g_SM_VideoLinkStats.overwrites.uCurrentKeyframe )
         return;
      process_data_tx_video_set_current_keyframe_interval( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe );
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
    
   int iDefaultKeyframeInterval = iCurrentFPS * 2 * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL / 1000;
        
   // Is relaying other vehicle? Go to default level

   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId > 0 )
   if ( g_pCurrentModel->relay_params.uRelayVehicleId > 0 )
   if ( g_pCurrentModel->relay_params.uRelayFlags & RELAY_FLAGS_VIDEO )
   if ( g_pCurrentModel->relay_params.uCurrentRelayMode == RELAY_MODE_REMOTE )
   if ( iDefaultKeyframeInterval != g_SM_VideoLinkStats.overwrites.uCurrentKeyframe )
   {
      log_line("[KeyFrame] Set default level due to relaying other vehicle.");
      s_iControllerRequestedKeyFrameInterval = iDefaultKeyframeInterval;
      process_data_tx_video_set_current_keyframe_interval(iDefaultKeyframeInterval);
      return;
   }


   // Have recent link from controller ok? Then do nothing.

   int iThresholdControllerLinkMs = 1000;
   if ( g_TimeNow > g_TimeStart + 5000 )
   if ( g_TimeNow > g_TimeStartRaspiVid + 3000 )
   if ( g_TimeLastReceivedRadioPacketFromController > g_TimeNow - iThresholdControllerLinkMs )
   if ( iHighestRXQuality >= 20 )
   {

      if ( -1 != s_iControllerRequestedKeyFrameInterval )
      if ( s_iControllerRequestedKeyFrameInterval != g_SM_VideoLinkStats.overwrites.uCurrentKeyframe )
      {
         log_line("[KeyFrame]: Current keyframe (%d) different from one requested by controller (%d).Switching to it.", g_SM_VideoLinkStats.overwrites.uCurrentKeyframe, s_iControllerRequestedKeyFrameInterval);
         process_data_tx_video_set_current_keyframe_interval(s_iControllerRequestedKeyFrameInterval);
      }

      return;
   }

   // Adaptive: need to go to lowest level?

   int iNewKeyframeInterval = -1;

   bool bGoToLowestLevel = false;

   if ( (! g_bReceivedPairingRequest) && (!g_bHadEverLinkToController) )
      bGoToLowestLevel = true;
   if ( g_TimeLastReceivedRadioPacketFromController < g_TimeNow - iThresholdControllerLinkMs )
      bGoToLowestLevel = true;
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
      bGoToLowestLevel = true;
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
   if ( g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel > g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_MQ)-2 )
      bGoToLowestLevel = true;

   if ( bGoToLowestLevel )
   {
      iNewKeyframeInterval = iCurrentFPS * 2 * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL / 1000;
      process_data_tx_video_set_current_keyframe_interval(iNewKeyframeInterval);
      return;
   }

   // Adaptive keyframe interval

   if ( iHighestRXQuality < 20 )
       iNewKeyframeInterval = iCurrentFPS * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL/1000;
   
   // RX Quality going down ?

   //if ( iHighestRXQuality < s_iLastRXQualityMaximumKeyFrame )
   {
      if ( iHighestRXQuality < 70 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedDown + 300 )
      {
         iNewKeyframeInterval = g_SM_VideoLinkStats.overwrites.uCurrentKeyframe/2;
         if ( iNewKeyframeInterval < iCurrentFPS * 2 * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL/1000 )
            iNewKeyframeInterval = iCurrentFPS * 2 * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL/1000;
         s_uLastTimeKeyFrameMovedDown = g_TimeNow;
      }

      if ( iHighestRXQuality < 50 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedDown + 300 )
      {
         iNewKeyframeInterval = iCurrentFPS * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL/1000;
         s_uLastTimeKeyFrameMovedDown = g_TimeNow;
      }
   }

   // RX Quality going up ?

   //if ( iHighestRXQuality > s_iLastRXQualityMaximumKeyFrame )
   {
      if ( iHighestRXQuality > 70 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedDown + 400 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedUp + 400 )
      {
         iNewKeyframeInterval = g_SM_VideoLinkStats.overwrites.uCurrentKeyframe*2;
         s_uLastTimeKeyFrameMovedUp = g_TimeNow;
      }

      if ( iHighestRXQuality > 90 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedDown + 200 )
      if ( g_TimeNow > s_uLastTimeKeyFrameMovedUp + 200 )
      {
         iNewKeyframeInterval = g_SM_VideoLinkStats.overwrites.uCurrentKeyframe*4;
         s_uLastTimeKeyFrameMovedUp = g_TimeNow;
      }
   }

   s_iLastRXQualityMinimumKeyFrame = iLowestRXQuality;
   s_iLastRXQualityMaximumKeyFrame = iHighestRXQuality;


   // Apply the new value, if it changed
   if ( iNewKeyframeInterval != -1 )
   {
      process_data_tx_video_set_current_keyframe_interval(iNewKeyframeInterval);
   }
}
