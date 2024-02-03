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

#include "../../base/hdmi_video.h"
#include "../../base/utils.h"
#include "menu.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_video_profile.h"
#include "menu_vehicle_video_encodings.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"
#include "menu_item_text.h"

#include "../osd/osd_common.h"

MenuVehicleVideo::MenuVehicleVideo(void)
:Menu(MENU_ID_VEHICLE_VIDEO, "Video Settings", NULL)
{
   m_Width = 0.32;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;
   float fSliderWidth = 0.12 * Menu::getScaleFactor();
   m_pMenuItemVideoWarning = NULL;
   m_idxSelection = 0;
   char szBuff[32];

   char szCam[256];
   char szCam2[246];
   str_get_hardware_camera_type_string( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType, szCam2);
   sprintf(szCam, "Camera: %s", szCam2);
   addTopLine(szCam);
  
   m_pItemsSelect[0] = new MenuItemSelect("Resolution");
   if ( g_pCurrentModel->isActiveCameraVeye() )
   {
      if ( g_pCurrentModel->isActiveCameraVeye307() )
      {
         sprintf(szBuff, "%s (%d x %d)", g_szOptionsVideoRes[g_iOptionsVideoIndex720p], g_iOptionsVideoWidth[g_iOptionsVideoIndex720p], g_iOptionsVideoHeight[g_iOptionsVideoIndex720p]);
         m_pItemsSelect[0]->addSelection(szBuff);
      }
      sprintf(szBuff, "%s (%d x %d)", g_szOptionsVideoRes[g_iOptionsVideoIndex1080p], g_iOptionsVideoWidth[g_iOptionsVideoIndex1080p], g_iOptionsVideoHeight[g_iOptionsVideoIndex1080p]);
      m_pItemsSelect[0]->addSelection(szBuff);
   }
   else 
   {
      for( int i=0; i<getOptionsVideoResolutionsCount(); i++ )
      {
         sprintf(szBuff, "%s (%d x %d)", g_szOptionsVideoRes[i], g_iOptionsVideoWidth[i], g_iOptionsVideoHeight[i]);
         m_pItemsSelect[0]->addSelection(szBuff);
      }
   }

   m_pItemsSelect[0]->setIsEditable();
   m_IndexRes = addMenuItem(m_pItemsSelect[0]);

   //if ( ! g_pCurrentModel->isActiveCameraVeye() )
   //{
   //   m_pItemsSelect[10] = new MenuItemSelect("Force Camera Mode 1", "Force the camera sensor to 1920x1080 mode, non binned. Works only for Raspberry Pi cameras.");  
   //   m_pItemsSelect[10]->addSelection("No");
   //   m_pItemsSelect[10]->addSelection("Yes");
   //   m_IndexForceCameraMode = addMenuItem(m_pItemsSelect[10]);
   //}
   //else
      m_IndexForceCameraMode = -1;

   m_pItemsSlider[0] = new MenuItemSlider("FPS", 2,90, 30, fSliderWidth);
   m_IndexFPS = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSelect[3] = new MenuItemSelect("Keyframing", "Enables or disables automatic keyframe adjustment.");  
   m_pItemsSelect[3]->addSelection("Manual");
   m_pItemsSelect[3]->addSelection("Auto");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexAutoKeyframe = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSlider[1] = new MenuItemSlider("Keyframe interval (fixed, ms)", "A keyframe is added every N miliseconds. Bigger keyframe values usually results in better quality video and lower bandwidth requirements but longer breakups if any.", 50,20000,200, fSliderWidth);
   m_pItemsSlider[1]->setStep(10);
   m_IndexKeyframe = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[3] = new MenuItemSlider("Max Auto Keyframe (ms)", 50,20000,0, fSliderWidth);
   m_pItemsSlider[3]->setTooltip("Sets the max allowed keyframe interval (in miliseconds) when auto adjusting is enabled for video keyframe interval.");
   m_pItemsSlider[3]->setStep(10);
   m_IndexMaxKeyFrame = addMenuItem(m_pItemsSlider[3]);

   m_pItemsSlider[2] = new MenuItemSlider("Video Bitrate (Mbps)", 1,80,0, fSliderWidth);
   m_pItemsSlider[2]->setTooltip("Sets a target desired bitrate for the video stream.");
   m_pItemsSlider[2]->enableHalfSteps();
   m_IndexFixedBitrate = addMenuItem(m_pItemsSlider[2]);

   m_pItemsSelect[2] = new MenuItemSelect("Video Profile", "Change all video params to get a particular desired video quality.");  
   m_pItemsSelect[2]->addSelection("High Performance");
   m_pItemsSelect[2]->addSelection("High Quality");
   m_pItemsSelect[2]->addSelection("User");
   m_pItemsSelect[2]->disableClick();
   m_IndexVideoProfile = addMenuItem(m_pItemsSelect[2]);

   m_IndexDevECScheme = -1;
   if ( g_pCurrentModel->bDeveloperMode )
   {
      m_pItemsSelect[5] = new MenuItemSelect("EC Algorithm", "Change the algorithm used for video error correction.");  
      m_pItemsSelect[5]->addSelection("Legacy");
      m_pItemsSelect[5]->addSelection("New");
      m_pItemsSelect[5]->setIsEditable();
      m_pItemsSelect[5]->setTextColor(get_Color_Dev());
      m_IndexDevECScheme = addMenuItem(m_pItemsSelect[5]);
   }
   m_pItemsSelect[4] = new MenuItemSelect("Enable HDMI Output", "Enables or disables video output the the HDMI port on the vehicle.");  
   m_pItemsSelect[4]->addSelection("Off");
   m_pItemsSelect[4]->addSelection("On");
   m_pItemsSelect[4]->setIsEditable();
   m_IndexHDMIOutput = addMenuItem(m_pItemsSelect[4]);

   m_IndexExpert = addMenuItem(new MenuItem("Advanced Video Parameters", "Change advanced video parameters for current profile."));
   m_pMenuItems[m_IndexExpert]->showArrow();
}

MenuVehicleVideo::~MenuVehicleVideo()
{
}

void MenuVehicleVideo::valuesToUI()
{
   char szBuff[128];
   bool found = false;

   removeMenuItem(m_pMenuItemVideoWarning);
   m_pMenuItemVideoWarning = NULL;
   m_idxSelection = 0;

   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   camera_profile_parameters_t* pCamProfile = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile]);

   bool bShowWarning = false;

   u32 uMaxVideoRadioDataRate = utils_get_max_radio_datarate_for_profile(g_pCurrentModel, g_pCurrentModel->video_params.user_selected_video_link_profile);
   u32 uMaxVideoRate = utils_get_max_allowed_video_bitrate_for_profile(g_pCurrentModel, g_pCurrentModel->video_params.user_selected_video_link_profile);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps > uMaxVideoRate )
   {
      char szBuff2[128];
      char szBuff3[128];
      str_format_bitrate(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps, szBuff);
      str_format_bitrate(uMaxVideoRate, szBuff2);
      str_format_bitrate(uMaxVideoRadioDataRate, szBuff3);
      log_line("MenuVehicleVideo: Current video profile (%s) set video bitrate (%s) is greater than %d%% of max allowed radio datarate (%s of max radio datarate of %s). Show warning.",
         str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile), szBuff, DEFAULT_VIDEO_LINK_LOAD_PERCENT, szBuff2, szBuff3);
      bShowWarning = true;
   }

   if ( m_IndexForceCameraMode != -1 )
   if ( ! g_pCurrentModel->isActiveCameraVeye() )
      m_pItemsSelect[10]->setSelection(pCamProfile->flags & CAMERA_FLAG_FORCE_MODE_1 ? 1:0);

   if ( g_pCurrentModel->isActiveCameraVeye() )
   {
      if ( g_iOptionsVideoWidth[g_iOptionsVideoIndex720p] == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width )
      if ( g_iOptionsVideoHeight[g_iOptionsVideoIndex720p] == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height )
      {
         m_pItemsSelect[0]->setSelection(0);
         found = true;
      }
      if ( g_iOptionsVideoWidth[g_iOptionsVideoIndex1080p] == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width )
      if ( g_iOptionsVideoHeight[g_iOptionsVideoIndex1080p] == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height )
      {
         if ( g_pCurrentModel->isActiveCameraVeye327290() )
            m_pItemsSelect[0]->setSelection(0);
         else
            m_pItemsSelect[0]->setSelection(1);
         found = true;
      }
   }
   else
   {
      for(int i=0; i<m_pItemsSelect[0]->getSelectionsCount(); i++ )
         if ( g_iOptionsVideoWidth[i] == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width )
         if ( g_iOptionsVideoHeight[i] == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height )
         {
         m_pItemsSelect[0]->setSelection(i);
         found = true;
         break;
         }
   }
   if ( ! found )
   {
      sprintf(szBuff, "Info: You are using a custom resolution (%d x %d) on this %s.", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height, g_pCurrentModel->getVehicleTypeString());
      addTopLine(szBuff);
   }
   
   m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps);

   int keyframe_ms = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe_ms;
   if ( keyframe_ms > 0 )
   {
      m_pItemsSelect[3]->setSelectedIndex(0);
      m_pItemsSlider[1]->setCurrentValue(keyframe_ms);
      m_pItemsSlider[1]->setEnabled(true);
      m_pItemsSlider[3]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[3]->setSelectedIndex(1);
      m_pItemsSlider[1]->setCurrentValue(-keyframe_ms);
      m_pItemsSlider[1]->setEnabled(false);
      m_pItemsSlider[3]->setEnabled(true);
   }

   m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->video_params.uMaxAutoKeyframeIntervalMs);

   m_pItemsSlider[2]->setCurrentValue(4*g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps/1000/1000);
   m_pItemsSlider[2]->setEnabled( true );

   m_pItemsSelect[2]->setSelectedIndex(2);
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_BEST_PERF )
      m_pItemsSelect[2]->setSelectedIndex(0);
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_HIGH_QUALITY )
      m_pItemsSelect[2]->setSelectedIndex(1);


   m_pItemsSelect[4]->setSelectedIndex((g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT)?1:0);
  

   if ( g_pCurrentModel->bDeveloperMode )
   if ( -1 != m_IndexDevECScheme )
   {
      m_pItemsSelect[5]->setSelectedIndex((g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_USE_OLD_EC_SCHEME)?0:1);
   }

   if ( bShowWarning )
   {
      char* szVideoWarning = addMessageVideoBitrate(g_pCurrentModel);
      if ( (NULL != szVideoWarning) && (0 != szVideoWarning[0]) )
      {
         m_pMenuItemVideoWarning = new MenuItemText(szVideoWarning, true);
         insertMenuItem(m_pMenuItemVideoWarning, 0);
         m_idxSelection = 1;
      }
   }
}

void MenuVehicleVideo::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleVideo::showFPSWarning(int w, int h, int fps)
{
   char szBuff[128];
   sprintf(szBuff, "Max FPS for this video mode (%d x %d) for this camera is %d FPS", w,h,fps);
   Popup* p = new Popup(true, szBuff, 5 );
   p->setIconId(g_idIconWarning, get_Color_IconWarning());
   popups_add_topmost(p);
}

void MenuVehicleVideo::sendVideoLinkProfiles()
{
   type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      memcpy(&profiles[i], &(g_pCurrentModel->video_link_profiles[i]), sizeof(type_video_link_profile));

   type_video_link_profile* pProfile = &(profiles[g_pCurrentModel->video_params.user_selected_video_link_profile]);

   int videoMode = m_pItemsSelect[0]->getSelectedIndex();
   bool forceUpdates = false;
   if ( g_pCurrentModel->isActiveCameraVeye() )
   {
      if ( pProfile->fps != m_pItemsSlider[0]->getCurrentValue() )
      {
         pProfile->fps = m_pItemsSlider[0]->getCurrentValue();
         if ( pProfile->fps > 30 )
         {
            if ( g_pCurrentModel->isActiveCameraVeye327290() )
               pProfile->fps = 30;
            else
            {
               pProfile->width = g_iOptionsVideoWidth[g_iOptionsVideoIndex720p];
               pProfile->height = g_iOptionsVideoHeight[g_iOptionsVideoIndex720p];
               m_pItemsSelect[0]->setSelection(0);
            }
         }
      }

      if ( 0 == videoMode )
      {
         if ( g_pCurrentModel->isActiveCameraVeye327290() )
         {
            pProfile->width = g_iOptionsVideoWidth[g_iOptionsVideoIndex1080p];
            pProfile->height = g_iOptionsVideoHeight[g_iOptionsVideoIndex1080p];
            if ( pProfile->fps > 30 )
               pProfile->fps = 30;
         }
         else
         {
            pProfile->width = g_iOptionsVideoWidth[g_iOptionsVideoIndex720p];
            pProfile->height = g_iOptionsVideoHeight[g_iOptionsVideoIndex720p];
         }
      }
      if ( 1 == videoMode )
      {
         pProfile->width = g_iOptionsVideoWidth[g_iOptionsVideoIndex1080p];
         pProfile->height = g_iOptionsVideoHeight[g_iOptionsVideoIndex1080p];
         if ( pProfile->fps > 30 )
         {
            pProfile->fps = 30;
            showFPSWarning(pProfile->width, pProfile->height, pProfile->fps);
         }
      }
      forceUpdates = true;
   }
   else
   {
      pProfile->width = g_iOptionsVideoWidth[videoMode];
      pProfile->height = g_iOptionsVideoHeight[videoMode];
      pProfile->fps = m_pItemsSlider[0]->getCurrentValue();
   }

   if ( pProfile->fps > g_iOptionsVideoMaxFPS[videoMode] )
   {
      pProfile->fps = g_iOptionsVideoMaxFPS[videoMode];
      showFPSWarning(g_iOptionsVideoWidth[videoMode], g_iOptionsVideoHeight[videoMode], g_iOptionsVideoMaxFPS[videoMode]);
   }
   
   if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
   {
      pProfile->keyframe_ms = m_pItemsSlider[1]->getCurrentValue();
   }
   else
   {
      pProfile->keyframe_ms = -m_pItemsSlider[1]->getCurrentValue();    
   }
   pProfile->bitrate_fixed_bps = m_pItemsSlider[2]->getCurrentValue()*1000*1000/4;


   if ( ! forceUpdates )
   if ( pProfile->width == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width )
   if ( pProfile->height == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height )
   if ( pProfile->fps == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps )
   if ( pProfile->keyframe_ms == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe_ms )
   if ( pProfile->bitrate_fixed_bps == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps )
      return;

   profiles[VIDEO_PROFILE_MQ].fps = pProfile->fps;
   profiles[VIDEO_PROFILE_MQ].keyframe_ms = pProfile->keyframe_ms;
   profiles[VIDEO_PROFILE_LQ].fps = pProfile->fps;
   profiles[VIDEO_PROFILE_LQ].keyframe_ms = pProfile->keyframe_ms;
   
   u8 buffer[1024];
   memcpy( buffer, &(profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile) );

   log_line("Sending new video link profiles to vehicle.");
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES, 0, buffer, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile)) )
      valuesToUI();
}


void MenuVehicleVideo::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( 10 == iChildMenuId/1000 )
   {
      valuesToUI();
      return;
   }
}

void MenuVehicleVideo::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   g_TimeLastVideoCameraChangeCommand = g_TimeNow;

   if ( ! g_pCurrentModel->isActiveCameraVeye() )
   if ( -1 != m_IndexForceCameraMode )
   if ( (m_IndexForceCameraMode + m_idxSelection) == m_SelectedIndex )
   {
      type_camera_parameters cparams;
      memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
      int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;

      if ( m_pItemsSelect[10]->getSelectedIndex() == 0 )
         cparams.profiles[iProfile].flags &= ~CAMERA_FLAG_FORCE_MODE_1;
      else
         cparams.profiles[iProfile].flags |= CAMERA_FLAG_FORCE_MODE_1;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters)) )
         valuesToUI();
      return;
   }

   if ( ((m_IndexRes + m_idxSelection) == m_SelectedIndex) || ((m_IndexFPS + m_idxSelection) == m_SelectedIndex) || ((m_IndexKeyframe + m_idxSelection) == m_SelectedIndex) || ((m_IndexAutoKeyframe + m_idxSelection) == m_SelectedIndex) )
   {
      sendVideoLinkProfiles();
      return;
   }

   if ( (m_IndexMaxKeyFrame + m_idxSelection) == m_SelectedIndex )
   {
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.uMaxAutoKeyframeIntervalMs = m_pItemsSlider[3]->getCurrentValue();

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( (m_IndexFixedBitrate + m_idxSelection) == m_SelectedIndex )
   {
      sendVideoLinkProfiles();
      return;
   }
   if ( (m_IndexVideoProfile + m_idxSelection) == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleVideoProfileSelector());
      return;
   }

   if ( g_pCurrentModel->bDeveloperMode )
   if ( -1 != m_IndexDevECScheme )
   if ( (m_IndexDevECScheme + m_idxSelection) == m_SelectedIndex )
   {
      int iIndex = m_pItemsSelect[5]->getSelectedIndex();
      if ( 0 == iIndex )
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_USE_OLD_EC_SCHEME;
      else
         g_pCurrentModel->uDeveloperFlags &= ~DEVELOPER_FLAGS_BIT_USE_OLD_EC_SCHEME;
      if ( ! handle_commands_send_developer_flags() )
         valuesToUI();
   }


   if ( (m_IndexHDMIOutput + m_idxSelection) == m_SelectedIndex )
   {
      video_parameters_t paramsOld;
      memcpy(&paramsOld, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      int index = m_pItemsSelect[4]->getSelectedIndex();
      if ( index == 0 )
         g_pCurrentModel->video_params.uVideoExtraFlags &= ~(VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT);
      else
         g_pCurrentModel->video_params.uVideoExtraFlags |= VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT;

      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      memcpy(&g_pCurrentModel->video_params, &paramsOld, sizeof(video_parameters_t));

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( (m_IndexExpert + m_idxSelection) == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleVideoEncodings());
}
