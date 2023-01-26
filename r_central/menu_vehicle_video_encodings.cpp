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
#include "menu.h"
#include "menu_vehicle_video_encodings.h"
#include "shared_vars.h"
#include "../renderer/render_engine.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "../base/commands.h"
#include "pairing.h"
#include "popup.h"
#include "colors.h"
#include "../base/utils.h"
#include "../common/string_utils.h"
#include "handle_commands.h"

const char* s_szWarningBitrate = "Warning: The current radio datarate is to small for the current video encoding settings.\n You will experience delays in the video stream.\n Increase the radio datarate, or decrease the video bitrate, decrease the encoding params.";

MenuVehicleVideoEncodings::MenuVehicleVideoEncodings(void)
:Menu(MENU_ID_VEHICLE_EXPERT_ENCODINGS, "Video Profile Parameters (Expert)", NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;
   m_ShowBitrateWarning = false;
   float fSliderWidth = 0.12;
   char szBuff[256];
   setSubTitle("Change advanced vehicle encoding settings (for expert users)");
   
  
   strcpy(szBuff, "Settings for unknown video profile:");
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_BEST_PERF ) 
      strcpy(szBuff, "Settings for High Performance video profile:");
   else if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_HIGH_QUALITY ) 
      strcpy(szBuff, "Settings for High Quality video profile:");
   else if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_USER ) 
      strcpy(szBuff, "Settings for User video profile:");
   else
      sprintf(szBuff, "Settings for %s video profile:", str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile));
   addTopLine(szBuff);

   addMenuItem(new MenuItemSection("Video Link Features"));
  
   m_pItemsSelect[3] = new MenuItemSelect("Adaptive video", "Automatically adjust video and radio transmission parameters when the radio link quality and the video link quality goes lower in order to improve range, video link and quality.");  
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("Yes");
   m_pItemsSelect[3]->setUseMultiViewLayout();
   m_IndexAdaptiveLink = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[9] = new MenuItemSelect("   Use Controller Feedback", "When vehicle adjusts video link params, use the feedback from the controller too in deciding the best video link params to be used.");  
   m_pItemsSelect[9]->addSelection("No");
   m_pItemsSelect[9]->addSelection("Yes");
   m_pItemsSelect[9]->setUseMultiViewLayout();
   m_IndexAdaptiveUseControllerToo = addMenuItem(m_pItemsSelect[9]);

   m_pItemsSelect[10] = new MenuItemSelect("   Lower Quality On Link Lost", "When vehicle looses connection from the controller, go to a lower video quality and lower radio datarate.");  
   m_pItemsSelect[10]->addSelection("No");
   m_pItemsSelect[10]->addSelection("Yes");
   m_pItemsSelect[10]->setUseMultiViewLayout();
   m_IndexVideoLinkLost = addMenuItem(m_pItemsSelect[10]);

   m_pItemsSlider[5] = new MenuItemSlider("   Auto Adjustment Strength", "How aggressive should the auto video link adjustments be. 1 is the slowest adjustment strength, 10 is the fastest and most aggressive adjustment strength.", 1,10,5, fSliderWidth);
   m_pItemsSlider[5]->setStep(1);
   m_IndexVideoAdjustStrength = addMenuItem(m_pItemsSlider[5]);

   m_pItemsSelect[2] = new MenuItemSelect("Enable retransmissions", "Enables the controller to request missing video data from the vehicle.");  
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_pItemsSelect[2]->setUseMultiViewLayout();
   m_IndexRetransmissions = addMenuItem(m_pItemsSelect[2]);

   addMenuItem(new MenuItemSection("Data & Error Correction Settings"));

   Preferences* p = get_Preferences();
   int iMaxSize = p->iDebugMaxPacketSize-sizeof(t_packet_header)-sizeof(t_packet_header_video_full);
   iMaxSize = (iMaxSize/10)*10;
   m_pItemsSlider[0] = new MenuItemSlider("Packet size", "How big is each indivisible video packet over the air link. No major impact in link quality. Smaller packets and more EC packets increase the chance of error correction but also increase the CPU usage.", 100,iMaxSize,iMaxSize/2, fSliderWidth);
   m_pItemsSlider[0]->setStep(10);
   m_IndexPacketSize = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSlider[1] = new MenuItemSlider("Data Packets in a Block", "How many indivisible packets are in a block. This has an impact on link recovery and error correction. Bigger values might increase the delay in video stream when link is degraded, but also increase the chance of error correction.", 2,MAX_DATA_PACKETS_IN_BLOCK,MAX_DATA_PACKETS_IN_BLOCK/2, fSliderWidth);
   m_IndexBlockPackets = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[2] = new MenuItemSlider("EC Packets in a Block", "How many error correcting packets to add to a block.Bigger values increase the chance of error correction but decrease the usable link data rate buget.", 0,MAX_FECS_PACKETS_IN_BLOCK,MAX_FECS_PACKETS_IN_BLOCK/2, fSliderWidth);
   m_IndexBlockFECs = addMenuItem(m_pItemsSlider[2]);


   addMenuItem(new MenuItemSection("H264 Encoder Settings"));

   m_pItemsSelect[4] = new MenuItemSelect("H264 Profile", "The higher the H264 profile, the higher the CPU usage on encode and decode and higher the end to end video latencey. Higher profiles can have lower video quality as more compression algorithms are used.");
   m_pItemsSelect[4]->addSelection("Baseline");
   m_pItemsSelect[4]->addSelection("Main");
   m_pItemsSelect[4]->addSelection("High");
   m_pItemsSelect[4]->addSelection("Extended");
   m_pItemsSelect[4]->setIsEditable();
   m_IndexH264Profile = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[5] = new MenuItemSelect("H264 Level", "");  
   m_pItemsSelect[5]->addSelection("4");
   m_pItemsSelect[5]->addSelection("4.1");
   m_pItemsSelect[5]->addSelection("4.2");
   m_pItemsSelect[5]->setIsEditable();
   m_IndexH264Level = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[6] = new MenuItemSelect("H264 Inter Refresh", "");  
   m_pItemsSelect[6]->addSelection("Cyclic");
   m_pItemsSelect[6]->addSelection("Adaptive");
   m_pItemsSelect[6]->addSelection("Both");
   m_pItemsSelect[6]->addSelection("Cyclic Rows");
   m_pItemsSelect[6]->setIsEditable();
   m_IndexH264Refresh = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[12] = new MenuItemSelect("H264 Slices", "Split video frames into multiple smaller parts. When having heavy radio interference there is a chance that not the entire video is corrupted if video is sliced up. But is uses more processing power.");
   for( int i=1; i<=16; i++ )
   {
      sprintf(szBuff, "%d", i);
      m_pItemsSelect[12]->addSelection(szBuff);
   }
   m_pItemsSelect[12]->setIsEditable();
   m_IndexH264Slices = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSelect[7] = new MenuItemSelect("H264 Insert PPS Headers", "");  
   m_pItemsSelect[7]->addSelection("No");
   m_pItemsSelect[7]->addSelection("Yes");
   m_pItemsSelect[7]->setIsEditable();
   m_IndexH264Headers = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[11] = new MenuItemSelect("H264 Fill SPS Timings", "");  
   m_pItemsSelect[11]->addSelection("No");
   m_pItemsSelect[11]->addSelection("Yes");
   m_pItemsSelect[11]->setIsEditable();
   m_IndexH264SPSTimings = addMenuItem(m_pItemsSelect[11]);

   m_pItemsSelect[8] = new MenuItemSelect("Auto H264 quantization", "Use default quantization for the H264 video encoding, or set a custom value.");  
   m_pItemsSelect[8]->addSelection("No");
   m_pItemsSelect[8]->addSelection("Yes");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexCustomQuant = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSlider[4] = new MenuItemSlider("Video quantization", 0,40,20,fSliderWidth);
   m_pItemsSlider[4]->setTooltip("Sets the quantization parameter for the video stream. Higher values reduces quality but decreases bitrate requirements and latency. The default is about 16");
   m_IndexQuantValue = addMenuItem(m_pItemsSlider[4]);

   m_IndexResetParams = addMenuItem(new MenuItem("Reset Profile", "Resets the current video profile to the default values."));
}

void MenuVehicleVideoEncodings::valuesToUI()
{
   m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].packet_length);

   m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_packets);
   m_pItemsSlider[1]->setEnabled(true);
   m_pItemsSlider[2]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_fecs);
   m_pItemsSlider[2]->setEnabled(true);


   int retr = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS)?1:0;
   int adaptive = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
   int useControllerInfo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?1:0;
   int controllerLinkLost = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST)?1:0;
   
   m_pItemsSelect[3]->setSelection(adaptive);
   m_pItemsSelect[9]->setSelectedIndex(useControllerInfo);
   m_pItemsSelect[10]->setSelectedIndex(controllerLinkLost);

   m_pItemsSlider[5]->setCurrentValue(g_pCurrentModel->video_params.videoAdjustmentStrength);
   m_pItemsSelect[2]->setSelectedIndex(retr);
   if ( 0 == adaptive )
   {
      m_pItemsSelect[9]->setEnabled(false);
      m_pItemsSelect[10]->setEnabled(false);
      m_pItemsSlider[5]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[9]->setEnabled(true);
      m_pItemsSelect[10]->setEnabled(true);
      m_pItemsSlider[5]->setEnabled(true);
   }

   m_pItemsSelect[4]->setSelection(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264profile);
   m_pItemsSelect[5]->setSelection(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264level);
   m_pItemsSelect[6]->setSelection(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264refresh);
   m_pItemsSelect[7]->setSelection((int)(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].insertPPS));
   
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization > 0 )
   {
      m_pItemsSelect[8]->setSelection(0);
      m_pItemsSlider[4]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization);
      m_pItemsSlider[4]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[8]->setSelection(1);
      m_pItemsSlider[4]->setCurrentValue(-g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization);
      m_pItemsSlider[4]->setEnabled(false);
   }

   m_pItemsSelect[11]->setSelectedIndex((g_pCurrentModel->video_params.uExtraFlags & 0x01)?1:0);
   m_pItemsSelect[12]->setSelectedIndex(g_pCurrentModel->video_params.iH264Slices-1);

   m_ExtraHeight = 0;
   m_ExtraHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS + 0.01*menu_getScaleMenus();

   m_ShowBitrateWarning = false;

   int nDataRate = g_pCurrentModel->getLinkDataRate(0);
   if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
   if ( g_pCurrentModel->getLinkDataRate(1) > nDataRate )
      nDataRate = g_pCurrentModel->getLinkDataRate(1);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps/1000/1000 > 0.7 * (float)(nDataRate) )
      m_ShowBitrateWarning = true;


   if ( m_ShowBitrateWarning )
   if ( ! menu_has_menu(MENU_ID_SIMPLE_MESSAGE) )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Datarate Warning",NULL);
      pm->m_xPos = m_xPos-0.05; pm->m_yPos = m_yPos+0.05;
      pm->m_Width = 0.5;
      pm->setTopIcon("");
      pm->addTopLine(s_szWarningBitrate);
      pm->m_fAlfaWhenInBackground = 1.0;
      add_menu_to_stack(pm);
   }
}

void MenuVehicleVideoEncodings::onShow()
{
   valuesToUI();
   Menu::onShow();
}

void MenuVehicleVideoEncodings::Render()
{
   RenderPrepare();
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();   
   float y0 = RenderFrameAndTitle();
   float y = y0;

   float maxWidth = m_Width*menu_getScaleMenus() - 2*MENU_MARGINS*menu_getScaleMenus();

   float x = m_xPos+ MENU_MARGINS*menu_getScaleMenus();
   float h = 5*menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS + 0.006*menu_getScaleMenus();
   
   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
      if ( i == 8 )
      {
         y += 0.004*menu_getScaleMenus();
         char szBuff[64];
         sprintf(szBuff, "EC rate: %d%%", 100*g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_fecs/(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_packets));
         g_pRenderEngine->drawText( x, y, height_text, g_idFontMenu, szBuff);
         y += menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS + 0.006*menu_getScaleMenus();
      }
   }
   RenderEnd(y0);
}

void MenuVehicleVideoEncodings::sendVideoLinkProfile()
{
   type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      memcpy(&profiles[i], &(g_pCurrentModel->video_link_profiles[i]), sizeof(type_video_link_profile));

   type_video_link_profile* pProfile = &(profiles[g_pCurrentModel->video_params.user_selected_video_link_profile]);

   pProfile->packet_length = m_pItemsSlider[0]->getCurrentValue();
   pProfile->block_packets = m_pItemsSlider[1]->getCurrentValue();
   pProfile->block_fecs = m_pItemsSlider[2]->getCurrentValue();
   
   if ( m_pItemsSelect[2]->getSelectedIndex() == 0 )
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS );
   else
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags | ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS;

   if ( m_pItemsSelect[3]->getSelectedIndex() == 0 )
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS );
   else
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS;

   if ( m_pItemsSelect[9]->getSelectedIndex() == 0 )
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO );
   else
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;

   if ( m_pItemsSelect[10]->getSelectedIndex() == 0 )
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST );
   else
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;

   log_line("Sending new video flags to vehicle for video profile %s: %s, %s, %s",
      str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile),
      (pProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS)?"Retransmissions=On":"Retransmissions=Off",
      (pProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?"AdaptiveVideo=On":"AdaptiveVideo=Off",
      (pProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?"AdaptiveUseControllerInfo=On":"AdaptiveUseControllerInfo=Off"
      );
   pProfile->h264profile = m_pItemsSelect[4]->getSelectedIndex();
   pProfile->h264level = m_pItemsSelect[5]->getSelectedIndex();
   pProfile->h264refresh = m_pItemsSelect[6]->getSelectedIndex();
   pProfile->insertPPS = m_pItemsSelect[7]->getSelectedIndex()?1:0;
   if ( 0 == m_pItemsSelect[8]->getSelectedIndex() )
   {
      pProfile->h264quantization = m_pItemsSlider[4]->getCurrentValue();
      if ( pProfile->h264quantization < 5 )
         pProfile->h264quantization = 5;
   }
   else
   {
      if ( pProfile->h264quantization > 0 )
         pProfile->h264quantization = - pProfile->h264quantization;
   }
   log_line("H264 quantization: %d", pProfile->h264quantization);

   if ( pProfile->packet_length == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].packet_length )
   if ( pProfile->block_packets == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_packets )
   if ( pProfile->block_fecs == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_fecs )
   if ( pProfile->encoding_extra_flags == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags )
   if ( pProfile->h264level == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264level )
   if ( pProfile->h264profile == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264profile )
   if ( pProfile->h264refresh == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264refresh )
   if ( pProfile->insertPPS == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].insertPPS )
   if ( pProfile->h264quantization == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization )
      return;

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( i == g_pCurrentModel->video_params.user_selected_video_link_profile )
         continue;
      if ( (i != VIDEO_PROFILE_MQ) && (i != VIDEO_PROFILE_LQ) )
         continue;
      g_pCurrentModel->video_link_profiles[i].h264profile = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264profile;
      g_pCurrentModel->video_link_profiles[i].h264level = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264level; 
      g_pCurrentModel->video_link_profiles[i].h264refresh = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264refresh;
      g_pCurrentModel->video_link_profiles[i].insertPPS = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].insertPPS;
   }

   u8 buffer[1024];
   memcpy( buffer, &(profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile) );

   log_line("Sending new video link profiles to vehicle.");
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES, 0, buffer, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile)) )
      valuesToUI();
}


void MenuVehicleVideoEncodings::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }


   if ( m_IndexPacketSize == m_SelectedIndex || 
        m_IndexBlockPackets == m_SelectedIndex || 
        m_IndexBlockFECs == m_SelectedIndex )
      sendVideoLinkProfile();

   if ( m_IndexVideoAdjustStrength == m_SelectedIndex )
   {
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.videoAdjustmentStrength = m_pItemsSlider[5]->getCurrentValue();

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( m_IndexBidirectionalVideo == m_SelectedIndex || m_IndexRetransmissions == m_SelectedIndex ||
        m_IndexAdaptiveLink == m_SelectedIndex || m_IndexAdaptiveUseControllerToo == m_SelectedIndex ||
        m_IndexVideoLinkLost == m_SelectedIndex )
   {
      if ( m_IndexAdaptiveLink == m_SelectedIndex )
      {
         if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
         {
            m_pItemsSelect[9]->setEnabled(false);
            m_pItemsSelect[10]->setEnabled(false);
            m_pItemsSlider[5]->setEnabled(false);
         }
         else
         {
            m_pItemsSelect[9]->setEnabled(true);
            m_pItemsSelect[10]->setEnabled(true);
            m_pItemsSlider[5]->setEnabled(true);
         }
      }

      sendVideoLinkProfile();
      return;
   }

   if ( m_IndexH264Profile == m_SelectedIndex )
      sendVideoLinkProfile();
   if ( m_IndexH264Level == m_SelectedIndex )
      sendVideoLinkProfile();
   if ( m_IndexH264Refresh == m_SelectedIndex )
      sendVideoLinkProfile();
   if ( m_IndexH264Headers == m_SelectedIndex )
      sendVideoLinkProfile();

   if ( m_IndexH264SPSTimings == m_SelectedIndex )
   {
      video_parameters_t paramsOld;
      memcpy(&paramsOld, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      int index = m_pItemsSelect[11]->getSelectedIndex();
      if ( index == 0 )
         g_pCurrentModel->video_params.uExtraFlags &= ~(0x01);
      else
         g_pCurrentModel->video_params.uExtraFlags |= 0x01;

      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      memcpy(&g_pCurrentModel->video_params, &paramsOld, sizeof(video_parameters_t));

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( m_IndexH264Slices == m_SelectedIndex )
   {
      video_parameters_t paramsOld;
      memcpy(&paramsOld, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      g_pCurrentModel->video_params.iH264Slices = 1 + m_pItemsSelect[12]->getSelectedIndex();

      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      memcpy(&g_pCurrentModel->video_params, &paramsOld, sizeof(video_parameters_t));

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( (m_IndexCustomQuant == m_SelectedIndex || m_IndexQuantValue == m_SelectedIndex) && menu_check_current_model_ok_for_edit() )
   {
      u32 uParam = 0;
      if ( 0 == m_pItemsSelect[8]->getSelectedIndex() )
      {
         uParam = m_pItemsSlider[4]->getCurrentValue();
         if ( uParam < 5 )
            uParam = 5;
      }
      else
         uParam = 0;
      log_line("Send H264 quantization command, param: %u", uParam);
 
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_H264_QUANTIZATION, uParam, NULL, 0) )
         valuesToUI();
      return;    
   }

   if ( m_IndexResetParams == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      // Reset expert params
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_RESET_VIDEO_LINK_PROFILE, 0, NULL, 0) )
         valuesToUI();
      return;
   }
}
