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
#include "menu_vehicle_video_adaptive.h"
#include "menu_vehicle_video_profile.h"
#include "menu_vehicle_video_encodings.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"

#include "../osd/osd_common.h"

MenuVehicleVideoAdaptive::MenuVehicleVideoAdaptive(void)
:Menu(MENU_ID_VEHICLE_VIDEO_ADAPTIVE, "Adaptive Video Settings", NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.1;

   float dxMargin = 0;//0.03 * Menu::getScaleFactor();
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

   m_pItemsSelect[8] = new MenuItemSelect("Auto Quantization", "Adjust the camera H264/H265 quantization to try to maintain a constant video bitrate.");
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
}

MenuVehicleVideoAdaptive::~MenuVehicleVideoAdaptive()
{
}

void MenuVehicleVideoAdaptive::valuesToUI()
{ 
   int keyframe_ms = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe_ms;
   
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

void MenuVehicleVideoAdaptive::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleVideoAdaptive::sendVideoLinkProfiles()
{
   type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      memcpy(&profiles[i], &(g_pCurrentModel->video_link_profiles[i]), sizeof(type_video_link_profile));

   type_video_link_profile* pProfile = &(profiles[g_pCurrentModel->video_params.user_selected_video_link_profile]);
   int keyframe_ms = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe_ms;

   if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
      pProfile->keyframe_ms = (keyframe_ms>0)?keyframe_ms:(-keyframe_ms);
   else
      pProfile->keyframe_ms = (keyframe_ms>0)?-keyframe_ms:(keyframe_ms);
  
   if ( m_pItemsSelect[8]->getSelectedIndex() == 0 )
      pProfile->uEncodingFlags = pProfile->uEncodingFlags & (~VIDEO_ENCODINGS_FLAGS_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION );
   else
      pProfile->uEncodingFlags = pProfile->uEncodingFlags | VIDEO_ENCODINGS_FLAGS_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION;
  
   if ( m_pItemsSelect[7]->getSelectedIndex() == 0 )
      pProfile->uEncodingFlags = pProfile->uEncodingFlags & (~VIDEO_ENCODINGS_FLAGS_ENABLE_RETRANSMISSIONS );
   else
      pProfile->uEncodingFlags = pProfile->uEncodingFlags | VIDEO_ENCODINGS_FLAGS_ENABLE_RETRANSMISSIONS;


   if ( pProfile->uEncodingFlags == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uEncodingFlags )
   if ( pProfile->keyframe_ms == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe_ms )
      return;

   // Propagate changes to lower video profiles

   propagate_video_profile_changes( &g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile], pProfile, &(profiles[0]));

   log_line("Sending video encoding flags: %s", str_format_video_encoding_flags(pProfile->uEncodingFlags));
      
   u8 buffer[1024];
   memcpy( buffer, &(profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile) );

   log_line("Sending new video link profiles to vehicle.");
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES, 0, buffer, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile)) )
      valuesToUI();
}


void MenuVehicleVideoAdaptive::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
}

void MenuVehicleVideoAdaptive::onSelectItem()
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

   if ( m_IndexAutoKeyframe == m_SelectedIndex || m_IndexAdaptiveVideo == m_SelectedIndex)
   {
      sendVideoLinkProfiles();
      return;
   }

   if ( m_IndexRetransmissions == m_SelectedIndex || m_IndexAutoQuantization == m_SelectedIndex )
   {
      sendVideoLinkProfiles();
      return;
   }
}
