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

#include "../../base/video_capture_res.h"
#include "../../base/utils.h"
#include "../../base/controller_utils.h"
#include "menu.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_video_profile.h"
#include "menu_vehicle_video_encodings.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"

#include "../osd/osd_common.h"

MenuVehicleVideo::MenuVehicleVideo(void)
:Menu(MENU_ID_VEHICLE_VIDEO, "Video Settings", NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.1;
   float fSliderWidth = 0.12 * Menu::getScaleFactor();
   char szBuff[32];

   char szCam[256];
   char szCam2[128];
   str_get_hardware_camera_type_string( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType, szCam2);
   sprintf(szCam, "Video Settings, Active Camera: %s", szCam2);
   //addTopLine(szCam);
   setTitle(szCam);
  

   m_pItemsSelect[2] = new MenuItemSelect("Video Profile", "Change all video params to get a particular desired video quality.");  
   m_pItemsSelect[2]->addSelection("High Performance");
   m_pItemsSelect[2]->addSelection("High Quality");
   m_pItemsSelect[2]->addSelection("User");
   m_pItemsSelect[2]->disableClick();
   m_pItemsSelect[2]->setExtraHeight(3.0*Menu::getSelectionPaddingY());
   m_IndexVideoProfile = addMenuItem(m_pItemsSelect[2]);

   m_pMenuItemVideoWarning = new MenuItemText("", true);
   m_pMenuItemVideoWarning->setHidden(true);
   addMenuItem(m_pMenuItemVideoWarning);

   m_pVideoResolutions = &(g_listCaptureResolutions[0]);
   m_iVideoResolutionsCount = getOptionsVideoResolutionsCount();

   if ( g_pCurrentModel->isActiveCameraVeye() )
   {
      m_pVideoResolutions = &(g_listCaptureResolutionsVeye[0]);
      m_iVideoResolutionsCount = g_iListCaptureResolutionsVeyeCount;
   }

   if ( g_pCurrentModel->isActiveCameraVeye307() )
   {
      m_pVideoResolutions = &(g_listCaptureResolutionsVeye307[0]);
      m_iVideoResolutionsCount = g_iListCaptureResolutionsVeyeCount307;
   }

   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      m_pVideoResolutions = &(g_listCaptureResolutionsOpenIPC[0]);
      m_iVideoResolutionsCount = g_iListCaptureResolutionsOpenIPCCount;
   }

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

   m_IndexForceCameraMode = -1;
   //if ( g_pCurrentModel->isActiveCameraCSI() )
   //{
   //   m_pItemsSelect[11] = new MenuItemSelect("Force Camera Mode 1", "Force the camera sensor to 1920x1080 mode, non binned. Works only for Raspberry Pi cameras.");  
   //   m_pItemsSelect[11]->addSelection("No");
   //   m_pItemsSelect[11]->addSelection("Yes");
   //   m_IndexForceCameraMode = addMenuItem(m_pItemsSelect[11]);
   //}
      

   int iMaxFPS = 90;
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      iMaxFPS = 120;
   m_pItemsSlider[0] = new MenuItemSlider("FPS", "Sets the FPS of the video stream.", 2,iMaxFPS, 30, fSliderWidth);
   m_IndexFPS = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSlider[1] = new MenuItemSlider("Keyframe interval (ms)", "A keyframe is added every [n] miliseconds. Bigger keyframe values usually results in better quality video and lower bandwidth requirements but longer breakups if any.", 50,20000,200, fSliderWidth);
   m_pItemsSlider[1]->setStep(10);
   m_IndexKeyframe = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[2] = new MenuItemSlider("Video Bitrate (Mbps)", "Sets the video bitrate of the video stream generated by the camera.", 1,80,0, fSliderWidth);
   m_pItemsSlider[2]->setTooltip("Sets a target desired bitrate for the video stream.");
   m_pItemsSlider[2]->enableHalfSteps();
   m_IndexVideoBitrate = addMenuItem(m_pItemsSlider[2]);

   m_pItemsSelect[10] = new MenuItemSelect("Video Codec", "Change the codec used to encode the source video stream.");
   m_pItemsSelect[10]->addSelection("H264");
   m_pItemsSelect[10]->addSelection("H265");
   m_pItemsSelect[10]->setIsEditable();
   m_IndexVideoCodec = addMenuItem(m_pItemsSelect[10]);

   addMenuItem(new MenuItemSection("Video Link Mode"));

   m_pItemsRadio[0] = new MenuItemRadio("", "Set the way video link behaves: fixed broadcast video, or auto adaptive video link and stream.");
   m_pItemsRadio[0]->addSelection("Fixed One Way Video Link", "The radio video link will be fixed, one way broadcast, no retransmissions will take place, video quality or video parameters are not adjusted in real time.");
   m_pItemsRadio[0]->addSelection("Adaptive Video Link", "Radio video link and video link parameters will automatically adjust in real time based on radio conditions.");
   m_pItemsRadio[0]->setEnabled(true);
   m_pItemsRadio[0]->useSmallLegend(true);
   m_IndexVideoLinkMode = addMenuItem(m_pItemsRadio[0]);

   float dxMargin = 0.03 * Menu::getScaleFactor();
   m_pItemsSelect[3] = new MenuItemSelect("Auto Keyframing", "Automatic keyframe adjustment based on radio link conditions.");  
   m_pItemsSelect[3]->addSelection("Off");
   m_pItemsSelect[3]->addSelection("On");
   m_pItemsSelect[3]->setIsEditable();
   m_pItemsSelect[3]->setMargin(dxMargin);
   m_IndexAutoKeyframe = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[6] = new MenuItemSelect("Auto Video Quality", "Reduce the video quality when radio link quality goes down.");  
   m_pItemsSelect[6]->addSelection("Off");
   m_pItemsSelect[6]->addSelection("On");
   m_pItemsSelect[6]->setIsEditable();
   m_pItemsSelect[6]->setMargin(dxMargin);
   m_IndexAdaptiveVideo = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[8] = new MenuItemSelect("Auto Bitrate", "Adjust the camera H264/H265 quantization to try to maintain a constant video bitrate.");
   m_pItemsSelect[8]->addSelection("Off");
   m_pItemsSelect[8]->addSelection("On");
   m_pItemsSelect[8]->setIsEditable();
   m_pItemsSelect[8]->setMargin(dxMargin);
   m_IndexAutoQuantization = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[7] = new MenuItemSelect("Retransmissions", "Enable request and retransmissions of video data.");  
   m_pItemsSelect[7]->addSelection("Off");
   m_pItemsSelect[7]->addSelection("On");
   m_pItemsSelect[7]->setIsEditable();
   m_pItemsSelect[7]->setMargin(dxMargin);
   m_pItemsSelect[7]->setExtraHeight(1.5* g_pRenderEngine->textHeight(g_idFontMenu) * MENU_ITEM_SPACING);
   m_IndexRetransmissions = addMenuItem(m_pItemsSelect[7]);

   m_pMenuItemVideoRecording = new MenuItem("Start Video Recording");
   if ( g_bVideoRecordingStarted )
       m_pMenuItemVideoRecording->setTitle("Stop Video Recording");
   m_pMenuItemVideoRecording->setTooltip("Manually start or stop video recording right now for this video stream");
   m_IndexRecording = addMenuItem(m_pMenuItemVideoRecording);

   m_IndexExpert = addMenuItem(new MenuItem("Advanced Video Settings", "Change advanced video parameters for current profile."));
   m_pMenuItems[m_IndexExpert]->showArrow();
}

MenuVehicleVideo::~MenuVehicleVideo()
{
}

void MenuVehicleVideo::valuesToUI()
{
   char szBuff[128];
   bool found = false;
   
   checkAddWarningInMenu();

   int iCameraProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   camera_profile_parameters_t* pCamProfile = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iCameraProfile]);

   m_pItemsSelect[10]->setEnabled(true);
   if ( g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
      m_pItemsSelect[10]->setSelectedIndex(1);
   else
      m_pItemsSelect[10]->setSelectedIndex(0);

   if ( m_IndexForceCameraMode != -1 )
   if ( g_pCurrentModel->isActiveCameraCSI() )
      m_pItemsSelect[11]->setSelection(pCamProfile->flags & CAMERA_FLAG_FORCE_MODE_1 ? 1:0);

   
   for(int i=0; i<m_iVideoResolutionsCount; i++ )
   {
      if ( m_pVideoResolutions[i].iWidth == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width )
      if ( m_pVideoResolutions[i].iHeight == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height )
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
      m_pItemsSlider[1]->setCurrentValue(keyframe_ms);
   else
      m_pItemsSlider[1]->setCurrentValue(-keyframe_ms);
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
   {
      m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[6]->setEnabled(false);
      m_pItemsSelect[7]->setEnabled(false);
      m_pItemsSelect[8]->setEnabled(false);
      m_pItemsSlider[1]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[3]->setEnabled(true);
      m_pItemsSelect[6]->setEnabled(true);
      m_pItemsSelect[7]->setEnabled(true);
      m_pItemsSelect[8]->setEnabled(true);

      if ( keyframe_ms > 0 )
         m_pItemsSlider[1]->setEnabled(true);
      else
         m_pItemsSlider[1]->setEnabled(false);
   }

   if ( keyframe_ms > 0 )
      m_pItemsSelect[3]->setSelectedIndex(0);
   else
      m_pItemsSelect[3]->setSelectedIndex(1);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uEncodingFlags & VIDEO_ENCODINGS_FLAGS_ENABLE_RETRANSMISSIONS )
      m_pItemsSelect[7]->setSelectedIndex(1);
   else
      m_pItemsSelect[7]->setSelectedIndex(0);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uEncodingFlags & VIDEO_ENCODINGS_FLAGS_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS )
      m_pItemsSelect[6]->setSelectedIndex(1);
   else
      m_pItemsSelect[6]->setSelectedIndex(0);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uEncodingFlags & VIDEO_ENCODINGS_FLAGS_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION )
      m_pItemsSelect[8]->setSelectedIndex(1);
   else
      m_pItemsSelect[8]->setSelectedIndex(0);

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

   
   if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
      pProfile->keyframe_ms = m_pItemsSlider[1]->getCurrentValue();
   else
      pProfile->keyframe_ms = -m_pItemsSlider[1]->getCurrentValue();    

   pProfile->bitrate_fixed_bps = m_pItemsSlider[2]->getCurrentValue()*1000*1000/4;

   pProfile->uEncodingFlags &= ~VIDEO_ENCODINGS_FLAGS_ONE_WAY_FIXED_VIDEO;
   if ( 0 == m_pItemsRadio[0]->getSelectedIndex() )
      pProfile->uEncodingFlags |= VIDEO_ENCODINGS_FLAGS_ONE_WAY_FIXED_VIDEO;

   if ( m_pItemsSelect[8]->getSelectedIndex() == 0 )
      pProfile->uEncodingFlags = pProfile->uEncodingFlags & (~VIDEO_ENCODINGS_FLAGS_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION );
   else
      pProfile->uEncodingFlags = pProfile->uEncodingFlags | VIDEO_ENCODINGS_FLAGS_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION;
  
   if ( m_pItemsSelect[7]->getSelectedIndex() == 0 )
      pProfile->uEncodingFlags = pProfile->uEncodingFlags & (~VIDEO_ENCODINGS_FLAGS_ENABLE_RETRANSMISSIONS );
   else
      pProfile->uEncodingFlags = pProfile->uEncodingFlags | VIDEO_ENCODINGS_FLAGS_ENABLE_RETRANSMISSIONS;


   if ( ! forceUpdates )
   if ( pProfile->uEncodingFlags == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uEncodingFlags )
   if ( pProfile->width == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width )
   if ( pProfile->height == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height )
   if ( pProfile->fps == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps )
   if ( pProfile->keyframe_ms == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe_ms )
   if ( pProfile->bitrate_fixed_bps == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps )
      return;

   // Propagate changes to lower video profiles

   propagate_video_profile_changes( &g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile], pProfile, &(profiles[0]));

   if ( pProfile->fps != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps )
     log_line("Sending new video FPS %d -> %d", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps, pProfile->fps);
   log_line("Sending video encoding flags: %s", str_format_video_encoding_flags(pProfile->uEncodingFlags));
      
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
   if ( m_IndexForceCameraMode == m_SelectedIndex )
   {
      type_camera_parameters cparams;
      memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
      int iCameraProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;

      if ( m_pItemsSelect[11]->getSelectedIndex() == 0 )
         cparams.profiles[iCameraProfile].flags &= ~CAMERA_FLAG_FORCE_MODE_1;
      else
         cparams.profiles[iCameraProfile].flags |= CAMERA_FLAG_FORCE_MODE_1;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters)) )
         valuesToUI();
      return;
   }

   /*
   if ( m_IndexAutoKeyframe == m_SelectedIndex )
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      addUnsupportedMessageOpenIPC(NULL);
      valuesToUI();
      return;
   }
   */
   
   if ( (m_IndexAutoKeyframe == m_SelectedIndex) || (m_IndexAdaptiveVideo == m_SelectedIndex) )
   if ( hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      addUnsupportedMessageOpenIPCGoke(NULL);
      valuesToUI();
      return;
   }

   if ( m_IndexAutoQuantization == m_SelectedIndex)
   if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      addUnsupportedMessageOpenIPC(NULL);
      valuesToUI();
      return;
   }

   if ( (m_IndexRes == m_SelectedIndex) || (m_IndexFPS == m_SelectedIndex) || (m_IndexKeyframe == m_SelectedIndex) || (m_IndexAutoKeyframe == m_SelectedIndex) )
   {
      #ifdef HW_PLATFORM_RADXA_ZERO3
      if ( m_IndexFPS == m_SelectedIndex )
      if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      if ( m_pItemsSlider[0]->getCurrentValue() > 30 )
         addMessage2(0, "Experimental", "FPS higher than 30 might have issues on Radxa+OpenIPC hardware for now. Revert to 30 if you see issues.");
      #endif

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

   if ( m_IndexRetransmissions == m_SelectedIndex || m_IndexAutoQuantization == m_SelectedIndex )
   {
      sendVideoLinkProfiles();
      return;
   }

   if ( m_IndexExpert == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleVideoEncodings());

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
      video_parameters_t paramsOld;
      memcpy(&paramsOld, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      if ( 0 == m_pItemsSelect[10]->getSelectedIndex() )
         g_pCurrentModel->video_params.uVideoExtraFlags &= ~VIDEO_FLAG_GENERATE_H265;
      else
      {
         if ( ! g_pCurrentModel->isRunningOnOpenIPCHardware() )
         {
            addMessage("Raspberry Pi hardware supports only H264 video encoder.");
            valuesToUI();
            return;
         }
         g_pCurrentModel->video_params.uVideoExtraFlags |= VIDEO_FLAG_GENERATE_H265;
      }
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      memcpy(&g_pCurrentModel->video_params, &paramsOld, sizeof(video_parameters_t));

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
   }
}
