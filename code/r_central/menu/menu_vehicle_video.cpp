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

#include "../../base/video_capture_res.h"
#include "../../base/utils.h"
#include "../../utils/utils_controller.h"
#include "menu.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_video_bidir.h"
#include "menu_vehicle_video_profile.h"
#include "menu_vehicle_video_encodings.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"

#include "../osd/osd_common.h"
#include "../process_router_messages.h"

MenuVehicleVideo::MenuVehicleVideo(void)
:Menu(MENU_ID_VEHICLE_VIDEO, "Video Settings", NULL)
{
   m_Width = 0.42;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.1;
   float fSliderWidth = 0.12 * Menu::getScaleFactor();

   char szBuff[32];

   char szCam[256];
   char szCam2[128];
   str_get_hardware_camera_type_string_to_string(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType, szCam2);
   sprintf(szCam, "Video Settings, Active Camera: %s", szCam2);
   //addTopLine(szCam);
   setTitle(szCam);
  

   m_pItemsSelect[2] = new MenuItemSelect("Video Profile", "Change all video params to get a particular desired video quality.");  
   m_pItemsSelect[2]->addSelection("High Performance");
   m_pItemsSelect[2]->addSelection("High Quality");
   m_pItemsSelect[2]->addSelection("User");
   m_pItemsSelect[2]->disableClick();
   m_IndexVideoProfile = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[10] = new MenuItemSelect("Video Codec", "Change the codec used to encode the source video stream.");
   m_pItemsSelect[10]->addSelection("H264");
   m_pItemsSelect[10]->addSelection("H265");
   m_pItemsSelect[10]->setIsEditable();
   m_IndexVideoCodec = addMenuItem(m_pItemsSelect[10]);

   m_pMenuItemVideoWarning = new MenuItemText("", true);
   m_pMenuItemVideoWarning->setHidden(true);
   addMenuItem(m_pMenuItemVideoWarning);

   m_pVideoResolutions = getOptionsVideoResolutions(g_pCurrentModel->getActiveCameraType());
   m_iVideoResolutionsCount = getOptionsVideoResolutionsCount(g_pCurrentModel->getActiveCameraType());

   m_pItemsSelect[0] = new MenuItemSelect("Resolution", "Sets the resolution of the video stream.");
   
   for( int i=0; i<m_iVideoResolutionsCount; i++ )
   {
      sprintf(szBuff, "%s (%d x %d)", m_pVideoResolutions[i].szName, m_pVideoResolutions[i].iWidth, m_pVideoResolutions[i].iHeight);

      if ( (i==0) && ( g_pCurrentModel->isActiveCameraVeye() ) && ( ! g_pCurrentModel->isActiveCameraVeye307() ) )
         m_pItemsSelect[0]->addSelection(szBuff, false);
      else
         m_pItemsSelect[0]->addSelection(szBuff);
   }


   m_pItemsSelect[0]->setIsEditable();
   m_IndexRes = addMenuItem(m_pItemsSelect[0]);      

   int iMaxFPS = 90;
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      iMaxFPS = 120;
   m_pItemsSlider[0] = new MenuItemSlider("FPS", "Sets the FPS of the video stream.", 2,iMaxFPS, 30, fSliderWidth);
   m_IndexFPS = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSlider[1] = new MenuItemSlider("Keyframe interval (ms)", "A keyframe is added every [n] miliseconds. Bigger keyframe values usually results in better quality video and lower bandwidth requirements but longer breakups if any.", 50,20000,200, fSliderWidth);
   m_pItemsSlider[1]->setStep(10);
   m_pItemsSlider[1]->setSufix("ms");
   m_IndexKeyframe = addMenuItem(m_pItemsSlider[1]);

   m_pMenuItemVideoKeyframeWarning = new MenuItemText("", true);
   m_pMenuItemVideoKeyframeWarning->setHidden(true);
   addMenuItem(m_pMenuItemVideoKeyframeWarning);

   m_pItemsSlider[2] = new MenuItemSlider("Video Bitrate (Mbps)", "Sets the video bitrate of the video stream generated by the camera.", 1,120,0, fSliderWidth);
   m_pItemsSlider[2]->setTooltip("Sets a target desired bitrate for the video stream.");
   m_pItemsSlider[2]->enableHalfSteps();
   m_pItemsSlider[2]->setSufix("mbps");
   m_IndexVideoBitrate = addMenuItem(m_pItemsSlider[2]);

   addMenuItem(new MenuItemSection("Video Link Mode"));

   m_pItemsRadio[0] = new MenuItemRadio("", "Set the way video link behaves: fixed broadcast video, or auto adaptive video link and stream.");
   m_pItemsRadio[0]->addSelection(L("Fixed One Way Video Link"), L("The radio video link will be one way broadcast only, no retransmissions will take place, video quality and video parameters will not be adjusted in real time."));
   m_pItemsRadio[0]->addSelection(L("Bidirectional/Adaptive Video Link"), L("Video and radio link parameters will be adjusted automatically in real time based on radio conditions."));
   m_pItemsRadio[0]->setEnabled(true);
   m_pItemsRadio[0]->useSmallLegend(true);
   m_IndexVideoLinkMode = addMenuItem(m_pItemsRadio[0]);

   m_IndexBidirectionalVideoSettings = addMenuItem(new MenuItem("Bidirectional/Adaptive Video Settings", "Change retransmissions, auto keyframe, adaptive video settings."));
   m_pMenuItems[m_IndexBidirectionalVideoSettings]->showArrow();
   m_pMenuItems[m_IndexBidirectionalVideoSettings]->setMargin( 0.024 * Menu::getScaleFactor());

   addSeparator();

   m_IndexExpert = addMenuItem(new MenuItem("Advanced Video Settings", "Change advanced video parameters for current profile."));
   m_pMenuItems[m_IndexExpert]->showArrow();
   
   //m_pMenuItemVideoRecording = new MenuItem("Start Video Recording");
   //if ( g_bVideoRecordingStarted )
   //    m_pMenuItemVideoRecording->setTitle("Stop Video Recording");
   //m_pMenuItemVideoRecording->setTooltip("Manually start or stop video recording right now for this video stream");
   //m_IndexRecording = addMenuItem(m_pMenuItemVideoRecording);
   m_pMenuItemVideoRecording = NULL;
   m_IndexRecording = -1;
}

MenuVehicleVideo::~MenuVehicleVideo()
{
}

void MenuVehicleVideo::valuesToUI()
{
   char szBuff[128];
   bool found = false;
   
   checkAddWarningInMenu();

   u32 uVideoProfileEncodingFlags = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags;

   m_pItemsSelect[10]->setEnabled(true);
   if ( g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
      m_pItemsSelect[10]->setSelectedIndex(1);
   else
      m_pItemsSelect[10]->setSelectedIndex(0);

   int iMaxFPS = 90;
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      iMaxFPS = 120;
    
   for(int i=0; i<m_iVideoResolutionsCount; i++ )
   {
      if ( m_pVideoResolutions[i].iWidth == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width )
      if ( m_pVideoResolutions[i].iHeight == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height )
      {
         m_pItemsSelect[0]->setSelection(i);
         iMaxFPS = getOptionsVideoResolutionMaxFPS(g_pCurrentModel->getActiveCameraType(), m_pVideoResolutions[i].iWidth, m_pVideoResolutions[i].iHeight);
         found = true;
         break;
      }
   }

   if ( ! found )
   {
      sprintf(szBuff, "Info: You are using a custom resolution (%d x %d) on this %s.", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height, g_pCurrentModel->getVehicleTypeString());
      addTopLine(szBuff);
   }
   
   m_pItemsSlider[0]->setMaxValue(iMaxFPS);
   m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps);

   int keyframe_ms = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe_ms;
   if ( keyframe_ms < 0 )
      keyframe_ms = -keyframe_ms;
   m_pItemsSlider[1]->setCurrentValue(keyframe_ms);

   if ( (uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME) && (! g_pCurrentModel->isVideoLinkFixedOneWay()) )
      m_pItemsSlider[1]->setEnabled(false);
   else
      m_pItemsSlider[1]->setEnabled(true);

   m_pItemsSlider[2]->setCurrentValue(4*g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps/1000/1000);
   m_pItemsSlider[2]->setEnabled( true );

   m_pItemsSelect[2]->setSelectedIndex(2);
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_BEST_PERF )
      m_pItemsSelect[2]->setSelectedIndex(0);
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_HIGH_QUALITY )
      m_pItemsSelect[2]->setSelectedIndex(1);

   int iFixedVideoLink = 0;
   if ( g_pCurrentModel->isVideoLinkFixedOneWay() )
      iFixedVideoLink = 1;
   m_pItemsRadio[0]->setSelectedIndex(1-iFixedVideoLink);
   m_pItemsRadio[0]->setFocusedIndex(1-iFixedVideoLink);

   if ( iFixedVideoLink )
      m_pMenuItems[m_IndexBidirectionalVideoSettings]->setEnabled(false);
   else
      m_pMenuItems[m_IndexBidirectionalVideoSettings]->setEnabled(true);
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

void MenuVehicleVideo::checkAddWarningInMenu()
{
   bool bShowWarning = false;
   u32 uMaxVideoRadioDataRate = utils_get_max_radio_datarate_for_profile(g_pCurrentModel, g_pCurrentModel->video_params.user_selected_video_link_profile);
   u32 uMaxVideoRate = utils_get_max_allowed_video_bitrate_for_profile(g_pCurrentModel, g_pCurrentModel->video_params.user_selected_video_link_profile);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps > uMaxVideoRate )
   {
      char szBuff[128];
      char szBuff2[128];
      char szBuff3[128];
      str_format_bitrate(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps, szBuff);
      str_format_bitrate(uMaxVideoRate, szBuff2);
      str_format_bitrate(uMaxVideoRadioDataRate, szBuff3);
      log_line("MenuVehicleVideo: Current video profile (%s) set video bitrate (%s) is greater than %d%% of maximum safe allowed radio datarate (%s of max radio datarate of %s). Show warning.",
         str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile), szBuff, DEFAULT_VIDEO_LINK_LOAD_PERCENT, szBuff2, szBuff3);
      bShowWarning = true;
   }

   if ( NULL != m_pMenuItemVideoWarning )
   {
      m_pMenuItemVideoWarning->setHidden(true);
      if ( bShowWarning )
      {
         char* szVideoWarning = addMessageVideoBitrate(g_pCurrentModel);
         if ( (NULL != szVideoWarning) && (0 != szVideoWarning[0]) )
         {
            m_pMenuItemVideoWarning->setTitle(szVideoWarning);
            m_pMenuItemVideoWarning->setHidden(false);
         }
      }
   }

   if ( NULL != m_pMenuItemVideoKeyframeWarning )
   {
      m_pMenuItemVideoKeyframeWarning->setHidden(true);
      if ( ! g_pCurrentModel->isVideoLinkFixedOneWay() )
      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
      {
         m_pMenuItemVideoKeyframeWarning->setTitle("Adaptive keyframe is turned on. Keyframe interval will be dynamically adjusted by Ruby. If you want to use a fixed keyframe, turn off Adaptive Keyframe from Bidirectional settings menu or set the video link as One Way.");
         m_pMenuItemVideoKeyframeWarning->setHidden(false);
      }
   }
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

   int iCurrentProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
   type_video_link_profile* pProfile = &(profiles[iCurrentProfile]);

   int videoResolutionIndex = m_pItemsSelect[0]->getSelectedIndex();
   bool forceUpdates = false;
   /*
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

      if ( 0 == videoResolutionIndex )
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
      if ( 1 == videoResolutionIndex )
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
   */
   {
      pProfile->width = m_pVideoResolutions[videoResolutionIndex].iWidth;
      pProfile->height = m_pVideoResolutions[videoResolutionIndex].iHeight;
      pProfile->fps = m_pItemsSlider[0]->getCurrentValue();
      if ( pProfile->fps > m_pVideoResolutions[videoResolutionIndex].iMaxFPS )
      {
         pProfile->fps = m_pVideoResolutions[videoResolutionIndex].iMaxFPS;
         showFPSWarning(pProfile->width, pProfile->height, pProfile->fps);
      }
   }

   if ( (pProfile->width != g_pCurrentModel->video_link_profiles[iCurrentProfile].width) ||
        (pProfile->height != g_pCurrentModel->video_link_profiles[iCurrentProfile].height) )
   {
      if ( pProfile->width > 1500 )
      if ( pProfile->fps > 60 )
         pProfile->fps = 60;
      if ( pProfile->width > 1980 )
      if ( pProfile->fps > 30 )
         pProfile->fps = 30;
      if ( pProfile->width >= 2048 )
      if ( pProfile->fps > 20 )
         pProfile->fps = 20;
   }

   #if defined (HW_PLATFORM_RASPBERRY)
   if ( (pProfile->width > 1980) || (pProfile->height > 1080) )
   {
      addMessage(L("Can't use 2k,4k resolutions on Raspberry Pi controllers."));
      valuesToUI();
      return;
   }
   #endif

   pProfile->keyframe_ms = m_pItemsSlider[1]->getCurrentValue();
   pProfile->bitrate_fixed_bps = m_pItemsSlider[2]->getCurrentValue()*1000*1000/4;

   pProfile->uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO;
   if ( 0 == m_pItemsRadio[0]->getSelectedIndex() )
      pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO;

   if ( ! forceUpdates )
   if ( pProfile->uProfileEncodingFlags == g_pCurrentModel->video_link_profiles[iCurrentProfile].uProfileEncodingFlags )
   if ( pProfile->width == g_pCurrentModel->video_link_profiles[iCurrentProfile].width )
   if ( pProfile->height == g_pCurrentModel->video_link_profiles[iCurrentProfile].height )
   if ( pProfile->fps == g_pCurrentModel->video_link_profiles[iCurrentProfile].fps )
   if ( pProfile->keyframe_ms == g_pCurrentModel->video_link_profiles[iCurrentProfile].keyframe_ms )
   if ( pProfile->bitrate_fixed_bps == g_pCurrentModel->video_link_profiles[iCurrentProfile].bitrate_fixed_bps )
      return;

   // Propagate changes to lower video profiles

   propagate_video_profile_changes( &g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile], pProfile, &(profiles[0]));

   if ( pProfile->fps != g_pCurrentModel->video_link_profiles[iCurrentProfile].fps )
     log_line("Sending new video FPS %d -> %d", g_pCurrentModel->video_link_profiles[iCurrentProfile].fps, pProfile->fps);
   log_line("Sending video encoding flags: %s", str_format_video_encoding_flags(pProfile->uProfileEncodingFlags));
      
   u8 buffer[1024];
   memcpy( buffer, &(profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile) );

   log_line("Sending new video link profiles to vehicle.");
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES, 0, buffer, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile)) )
      valuesToUI();
   else
   {
      if ( modelvideoLinkProfileIsOnlyVideoKeyframeChanged(&(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile]), pProfile) )
         send_control_message_to_router(PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE, 500);
      else
         send_control_message_to_router(PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE, 7000);
   }
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

   /*
   if ( m_IndexAutoKeyframe == m_SelectedIndex )
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      addUnsupportedMessageOpenIPC(NULL);
      valuesToUI();
      return;
   }
   */
   
   if ( (m_IndexRes == m_SelectedIndex) || (m_IndexFPS == m_SelectedIndex) || (m_IndexKeyframe == m_SelectedIndex) )
   {
      sendVideoLinkProfiles();
      return;
   }

   if ( m_IndexVideoBitrate == m_SelectedIndex )
   {
      sendVideoLinkProfiles();
      return;
   }
   if ( m_IndexVideoProfile == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleVideoProfileSelector());
      return;
   }

   if ( m_IndexVideoLinkMode == m_SelectedIndex )
   {
      int iIndex = m_pItemsRadio[0]->getFocusedIndex();
      log_line("MenuVideo: Selected video link mode option %d", iIndex);

      if ( iIndex == 1 )
      if ( g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY )
      {
         addMessage2(0, "Incompatible settings", "You have disabled all the radio uplinks. Adaptive video mode is not possible without any radio uplink. Enable radio uplinks from Menu->Vehicle->Radio.");
         valuesToUI();
         return;
      } 
      m_pItemsRadio[0]->setSelectedIndex(iIndex);
      sendVideoLinkProfiles();
   }

   if ( m_IndexBidirectionalVideoSettings == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleVideoBidirectional());

   if ( m_IndexExpert == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleVideoEncodings());

   if ( -1 != m_IndexRecording )
   if ( NULL != m_pMenuItemVideoRecording )
   if ( m_IndexRecording == m_SelectedIndex )
   {
      if ( g_bVideoRecordingStarted )
      {
         if ( 0 == ruby_stop_recording() )
            m_pMenuItemVideoRecording->setTitle("Start Video Recording");
      }
      else
      {
         if ( 0 == ruby_start_recording() )
            m_pMenuItemVideoRecording->setTitle("Stop Video Recording");
      }
   }

   if ( m_IndexVideoCodec == m_SelectedIndex )
   {
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));

      if ( 0 == m_pItemsSelect[10]->getSelectedIndex() )
         paramsNew.uVideoExtraFlags &= ~VIDEO_FLAG_GENERATE_H265;
      else
      {
         #if defined (HW_PLATFORM_RASPBERRY)
         addMessage("Your controller Raspberry Pi hardware supports only H264 video decoder. Can't use H265 codec.");
         valuesToUI();
         return;
         #endif
         if ( ! g_pCurrentModel->isRunningOnOpenIPCHardware() )
         {
            char szTextW[256];
            sprintf(szTextW, "Your %s's Raspberry Pi hardware supports only H264 video encoder/decoder.", g_pCurrentModel->getVehicleTypeString());
            addMessage(szTextW);
            valuesToUI();
            return;
         }         
         paramsNew.uVideoExtraFlags |= VIDEO_FLAG_GENERATE_H265;
      }

      log_line("Changed video codec type from %s to %s",
         (g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265)?"H265":"H264",
         (paramsNew.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265)?"H265":"H264");

      if ( g_pCurrentModel->video_params.uVideoExtraFlags != paramsNew.uVideoExtraFlags )
      {
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
            valuesToUI();
         else
            send_control_message_to_router(PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE, 10000);
      }
   }
}
