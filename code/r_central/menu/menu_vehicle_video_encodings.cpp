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
        Copyright info and developer info must be preserved as is in the user
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

#include "menu.h"
#include "menu_vehicle_video_encodings.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "../../base/utils.h"
#include "../../base/controller_utils.h"

const char* s_szWarningBitrate = "Warning: The current radio datarate is to small for the current video encoding settings.\n You will experience delays in the video stream.\n Increase the radio datarate, or decrease the video bitrate, decrease the encoding params.";

MenuVehicleVideoEncodings::MenuVehicleVideoEncodings(void)
:Menu(MENU_ID_VEHICLE_EXPERT_ENCODINGS, "Advanced Video Settings (Expert)", NULL)
{
   m_Width = 0.37;
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
  
   m_pItemsSelect[3] = new MenuItemSelect("Adaptive video", "Automatically adjust video and radio transmission parameters when the radio link quality and the video link quality goes lower in order to improve range, video link and quality. Full: go to lowest video quality and best transmission if needed. Medium: Moderate lower quality.");  
   m_pItemsSelect[3]->addSelection("Off");
   m_pItemsSelect[3]->addSelection("On (Medium)");
   m_pItemsSelect[3]->addSelection("On (Full)");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexAdaptiveLink = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[9] = new MenuItemSelect("   Use Controller Feedback", "When vehicle adjusts video link params, use the feedback from the controller too in deciding the best video link params to be used.");  
   m_pItemsSelect[9]->addSelection("No");
   m_pItemsSelect[9]->addSelection("Yes");
   m_pItemsSelect[9]->setIsEditable();
   m_IndexAdaptiveUseControllerToo = addMenuItem(m_pItemsSelect[9]);

   m_pItemsSelect[10] = new MenuItemSelect("   Lower Quality On Link Lost", "When vehicle looses connection from the controller, go to a lower video quality and lower radio datarate.");  
   m_pItemsSelect[10]->addSelection("No");
   m_pItemsSelect[10]->addSelection("Yes");
   m_pItemsSelect[10]->setIsEditable();
   m_IndexVideoLinkLost = addMenuItem(m_pItemsSelect[10]);

   m_pItemsSlider[5] = new MenuItemSlider("   Auto Adjustment Strength", "How aggressive should the auto video link adjustments be. 1 is the slowest adjustment strength, 10 is the fastest and most aggressive adjustment strength.", 1,10,5, fSliderWidth);
   m_pItemsSlider[5]->setStep(1);
   m_IndexVideoAdjustStrength = addMenuItem(m_pItemsSlider[5]);

   m_pItemsSelect[17] = new MenuItemSelect("Ignore Video Spikes", "Ignores momentary spikes in video bandwidth or tx time so autoadjustments are not done.");
   m_pItemsSelect[17]->addSelection("Off");
   m_pItemsSelect[17]->addSelection("On");
   m_pItemsSelect[17]->setIsEditable();
   m_IndexIgnoreTxSpikes = addMenuItem(m_pItemsSelect[17]);

   m_pItemsSlider[3] = new MenuItemSlider("Max Auto Keyframe (ms)", 50,20000,0, fSliderWidth);
   m_pItemsSlider[3]->setTooltip("Sets the max allowed keyframe interval (in miliseconds) when auto adjusting is enabled for video keyframe interval parameter.");
   m_pItemsSlider[3]->setStep(10);
   m_IndexMaxKeyFrame = addMenuItem(m_pItemsSlider[3]);

   m_pItemsSelect[2] = new MenuItemSelect("Enable retransmissions", "Enables the controller to request missing video data from the vehicle.");  
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexRetransmissions = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[18] = new MenuItemSelect("Retransmissions Algorithm", "Change the way retransmissions are requested.");  
   m_pItemsSelect[18]->addSelection("Regular");
   m_pItemsSelect[18]->addSelection("Aggressive");
   m_pItemsSelect[18]->setIsEditable();
   m_IndexRetransmissionsFast = addMenuItem(m_pItemsSelect[18]);
   
   m_pItemsSelect[1] = new MenuItemSelect("Enable Vehicle Local HDMI Output", "Enables or disables video output the the HDMI port on the vehicle.");  
   m_pItemsSelect[1]->addSelection("Off");
   m_pItemsSelect[1]->addSelection("On");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexHDMIOutput = addMenuItem(m_pItemsSelect[1]);


   addMenuItem(new MenuItemSection("Data & Error Correction Settings"));


   m_pItemsSelect[16] = new MenuItemSelect("Radio Data Rate for Video", "Actual radio data rate to use for this video profile for video data transmission.");
   m_pItemsSelect[16]->addSelection("Auto (Use Radio Link)");
   for( int i=0; i<getDataRatesCount(); i++ )
   {
      if ( getDataRatesBPS()[i] < 3000000 )
         continue;
      str_getDataRateDescription(getDataRatesBPS()[i], 0, szBuff);
      m_pItemsSelect[16]->addSelection(szBuff);
   }
   if ( NULL != g_pCurrentModel && ((g_pCurrentModel->radioInterfacesParams.interface_supported_bands[0] & RADIO_HW_SUPPORTED_BAND_58) || (g_pCurrentModel->radioInterfacesParams.interface_supported_bands[1] & RADIO_HW_SUPPORTED_BAND_58)) )
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
   {
      str_getDataRateDescription(-1-i, 0, szBuff);
      m_pItemsSelect[16]->addSelection(szBuff);
   }
   m_pItemsSelect[16]->setIsEditable();
   m_IndexDataRate = addMenuItem(m_pItemsSelect[16]);

   Preferences* p = get_Preferences();
   int iMaxSize = p->iDebugMaxPacketSize-sizeof(t_packet_header)-sizeof(t_packet_header_video_full_77);
   iMaxSize = (iMaxSize/10)*10;
   m_pItemsSlider[0] = new MenuItemSlider("Packet size", "How big is each indivisible video packet over the air link. No major impact in link quality. Smaller packets and more EC packets increase the chance of error correction but also increase the CPU usage.", 100,iMaxSize,iMaxSize/2, fSliderWidth);
   m_pItemsSlider[0]->setStep(10);
   m_IndexPacketSize = addMenuItem(m_pItemsSlider[0]);

   int iMaxPackets = MAX_TOTAL_PACKETS_IN_BLOCK;
   if ( NULL != g_pCurrentModel )
      iMaxPackets = g_pCurrentModel->hwCapabilities.iMaxTxVideoBlockPackets;
   m_pItemsSlider[1] = new MenuItemSlider("Data Packets in a Block", "How many indivisible packets are in a block. This has an impact on link recovery and error correction. Bigger values might increase the delay in video stream when link is degraded, but also increase the chance of error correction.", 2, iMaxPackets/2, iMaxPackets/4, fSliderWidth);
   m_IndexBlockPackets = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSlider[2] = new MenuItemSlider("EC Packets in a Block", "How many error correcting packets to add to a block.Bigger values increase the chance of error correction but decrease the usable link data rate buget.", 0, iMaxPackets/2, iMaxPackets/4, fSliderWidth);
   m_IndexBlockFECs = addMenuItem(m_pItemsSlider[2]);

   m_pItemsSlider[2]->setExtraHeight( (1.0 + 2.0*MENU_ITEM_SPACING) * g_pRenderEngine->textHeight(g_idFontMenuSmall));

   m_pItemsSelect[19] = new MenuItemSelect("EC Spreading Factor", "Spreads the EC packets accross multiple video blocks.");
   m_pItemsSelect[19]->addSelection("0");
   m_pItemsSelect[19]->addSelection("1");
   m_pItemsSelect[19]->addSelection("2");
   m_pItemsSelect[19]->addSelection("3");
   m_pItemsSelect[19]->setIsEditable();
   m_IndexECSchemeSpread = addMenuItem(m_pItemsSelect[19]);

   addMenuItem(new MenuItemSection("H264 Encoder Settings"));

   m_pItemsSelect[4] = new MenuItemSelect("H264 Profile", "The higher the H264 profile, the higher the CPU usage on encode and decode and higher the end to end video latencey. Higher profiles can have lower video quality as more compression algorithms are used.");
   m_pItemsSelect[4]->addSelection("Baseline");
   m_pItemsSelect[4]->addSelection("Main");
   //m_pItemsSelect[4]->addSelection("Extended");
   m_pItemsSelect[4]->addSelection("High");
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

   m_pItemsSlider[4] = new MenuItemSlider("Manual video quantization", 0,40,20,fSliderWidth);
   m_pItemsSlider[4]->setTooltip("Sets a fixed H264 quantization parameter for the video stream. Higher values reduces quality but decreases bitrate requirements and latency. The default is about 16");
   m_IndexQuantValue = addMenuItem(m_pItemsSlider[4]);

   m_pItemsSelect[14] = new MenuItemSelect("Enable adaptive H264 quantization", "Enable algorithm that auto adjusts the H264 quantization to match the desired video bitrate in realtime.");  
   m_pItemsSelect[14]->addSelection("No");
   m_pItemsSelect[14]->addSelection("Yes");
   m_pItemsSelect[14]->setIsEditable();
   m_IndexEnableAdaptiveQuantization = addMenuItem(m_pItemsSelect[14]);

   m_pItemsSelect[15] = new MenuItemSelect("Adaptive H264 quantization strength", "How strongh should the algorithm be. The algorithm that auto adjusts the H264 quantization to match the desired video bitrate in realtime.");  
   m_pItemsSelect[15]->addSelection("Low");
   m_pItemsSelect[15]->addSelection("High");
   m_pItemsSelect[15]->setIsEditable();
   m_IndexAdaptiveQuantizationStrength = addMenuItem(m_pItemsSelect[15]);

   m_IndexResetParams = addMenuItem(new MenuItem("Reset Profile", "Resets the current video profile to the default values."));
}

void MenuVehicleVideoEncodings::valuesToUI()
{
   m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].packet_length);

   m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_packets);
   m_pItemsSlider[1]->setEnabled(true);
   m_pItemsSlider[2]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_fecs);
   m_pItemsSlider[2]->setEnabled(true);

   u32 uECSpreadHigh = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT)?1:0;
   u32 uECSpreadLow = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT)?1:0;
   u32 uECSpread = uECSpreadLow + (uECSpreadHigh*2);

   m_pItemsSelect[19]->setSelectedIndex((int) uECSpread);

   m_pItemsSelect[4]->setSelectedIndex((g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT)?1:0);
  
   m_pItemsSelect[17]->setSelectedIndex((g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_IGNORE_TX_SPIKES)?1:0);

   m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->video_params.uMaxAutoKeyframeIntervalMs);


   log_line("MenuVideoEncodings: Current video profile: %d, %s, current video datarate: %u",
      g_pCurrentModel->video_params.user_selected_video_link_profile,
      str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile),
      g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps);

   int iLowRatesToSkip = 0;
   while ( getDataRatesBPS()[iLowRatesToSkip] < 3000000 )
      iLowRatesToSkip++;

   int selectedIndex = 0;
   for( int i=0; i<getDataRatesCount(); i++ )
   {
      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps == getDataRatesBPS()[i] )
         selectedIndex = i+1-iLowRatesToSkip;
   }
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
   {
      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps == -1-i )
         selectedIndex = i+getDataRatesCount()+1 - iLowRatesToSkip;
   }
   m_pItemsSelect[16]->setSelection(selectedIndex);


   int retr = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS)?1:0;
   int adaptive = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
   int useControllerInfo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?1:0;
   int controllerLinkLost = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST)?1:0;
   
   if ( adaptive )
   {
      if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
         m_pItemsSelect[3]->setSelectedIndex(1);
      else
         m_pItemsSelect[3]->setSelectedIndex(2);
   }
   else
      m_pItemsSelect[3]->setSelectedIndex(0);

   m_pItemsSelect[9]->setSelectedIndex(useControllerInfo);
   m_pItemsSelect[10]->setSelectedIndex(controllerLinkLost);

   m_pItemsSlider[5]->setCurrentValue(g_pCurrentModel->video_params.videoAdjustmentStrength);
   
   m_pItemsSelect[2]->setSelectedIndex(retr);
   m_pItemsSelect[18]->setSelectedIndex((g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_RETRANSMISSIONS_FAST)?1:0);
   if ( retr )
      m_pItemsSelect[18]->setEnabled(true);
   else
      m_pItemsSelect[18]->setEnabled(false);

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
   
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH )
      m_pItemsSelect[15]->setSelectedIndex(1);
   else
      m_pItemsSelect[15]->setSelectedIndex(0);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION )
   {
      m_pItemsSelect[14]->setSelectedIndex(1);
      m_pItemsSelect[15]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[14]->setSelectedIndex(0);
      m_pItemsSelect[15]->setEnabled(false);    
   }


   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization > 0 )
   {
      m_pItemsSelect[8]->setSelection(0);
      m_pItemsSlider[4]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization);
      m_pItemsSlider[4]->setEnabled(true);
      m_pItemsSelect[14]->setEnabled(false);
      m_pItemsSelect[15]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[8]->setSelection(1);
      m_pItemsSlider[4]->setCurrentValue(-g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization);
      m_pItemsSlider[4]->setEnabled(false);
      m_pItemsSelect[14]->setEnabled(true);
      //m_pItemsSelect[15]->setEnabled(true);
   }

   m_pItemsSelect[11]->setSelectedIndex((g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_FILL_H264_SPT_TIMINGS)?1:0);
   m_pItemsSelect[12]->setSelectedIndex(g_pCurrentModel->video_params.iH264Slices-1);

   m_ShowBitrateWarning = false;

   u32 uRealDataRate = g_pCurrentModel->getLinkRealDataRate(0);
   if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
   if ( g_pCurrentModel->getLinkRealDataRate(1) > uRealDataRate )
      uRealDataRate = g_pCurrentModel->getLinkRealDataRate(1);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].bitrate_fixed_bps > 0.7 * (float)(uRealDataRate) )
      m_ShowBitrateWarning = true;


   if ( m_ShowBitrateWarning )
   if ( ! menu_has_menu(MENU_ID_SIMPLE_MESSAGE) )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Datarate Warning",NULL);
      pm->m_xPos = m_xPos-0.05; pm->m_yPos = m_yPos+0.05;
      pm->m_Width = 0.5;
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
   float y0 = RenderFrameAndTitle();
   float y = y0;

   float x = m_xPos + m_sfMenuPaddingX;
   
   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
      if ( (i == m_IndexBlockFECs) && didRenderedLastItem() )
      {
         char szBuff[64];
         sprintf(szBuff, "EC rate: %d%%", 100*g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_fecs/(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_packets));
         g_pRenderEngine->drawText( x, y - (1.0 + 2.0*MENU_ITEM_SPACING) * g_pRenderEngine->textHeight(g_idFontMenuSmall), g_idFontMenuSmall, szBuff);
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
      pProfile->encoding_extra_flags &= (~ (ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO) );
   else
   {
      if ( m_pItemsSelect[3]->getSelectedIndex() == 1 )
      {
         pProfile->encoding_extra_flags |= ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS;
         pProfile->encoding_extra_flags |= ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
      }
      else
      {
         pProfile->encoding_extra_flags |= ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS;
         pProfile->encoding_extra_flags &= (~ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO);
      }
   }

   u32 uECSpread = (u32) m_pItemsSelect[19]->getSelectedIndex();

   pProfile->encoding_extra_flags &= ~(ENCODING_EXTRA_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT | ENCODING_EXTRA_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT);
   if ( uECSpread & 0x01 )
      pProfile->encoding_extra_flags |= ENCODING_EXTRA_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT;
   if ( uECSpread & 0x02 )
      pProfile->encoding_extra_flags |= ENCODING_EXTRA_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT;

   if ( m_pItemsSelect[9]->getSelectedIndex() == 0 )
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO );
   else
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;

   if ( m_pItemsSelect[10]->getSelectedIndex() == 0 )
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST );
   else
      pProfile->encoding_extra_flags = pProfile->encoding_extra_flags | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;

   pProfile->encoding_extra_flags &= ~(ENCODING_EXTRA_FLAG_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION | ENCODING_EXTRA_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH);
   if ( 1 == m_pItemsSelect[14]->getSelectedIndex() )
      pProfile->encoding_extra_flags |= ENCODING_EXTRA_FLAG_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION;
   if ( 1 == m_pItemsSelect[15]->getSelectedIndex() )
      pProfile->encoding_extra_flags |= ENCODING_EXTRA_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH;
     
   log_line("Sending new video flags to vehicle for video profile %s: %s, %s, %s",
      str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile),
      (pProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS)?"Retransmissions=On":"Retransmissions=Off",
      (pProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?"AdaptiveVideo=On":"AdaptiveVideo=Off",
      (pProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?"AdaptiveUseControllerInfo=On":"AdaptiveUseControllerInfo=Off"
      );

   log_line("Adaptive video quantization: %s, %s",
    (pProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION)?"On":"ROff",
    (pProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH)?"Strength: High":"Strength: Low");

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

   int index = m_pItemsSelect[16]->getSelectedIndex();
   if ( index == 0 )
      pProfile->radio_datarate_video_bps = 0;
   else
   {
      index--;

      int iLowRatesToSkip = 0;
      while ( getDataRatesBPS()[iLowRatesToSkip] < 3000000 )
         iLowRatesToSkip++;

      if ( index < getDataRatesCount()-iLowRatesToSkip )
         pProfile->radio_datarate_video_bps = getDataRatesBPS()[index + iLowRatesToSkip];
      else
         pProfile->radio_datarate_video_bps = -1-(index + iLowRatesToSkip - getDataRatesCount());
   }

   if ( pProfile->radio_datarate_video_bps == g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps )
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

   // Propagate changes to lower video profiles

   propagate_video_profile_changes( &g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile], pProfile, &(profiles[0]));

   log_line("Sending video encoding extra flags: %s", str_format_video_encoding_flags(pProfile->encoding_extra_flags));

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
        m_IndexBlockFECs == m_SelectedIndex ||
        m_IndexECSchemeSpread == m_SelectedIndex )
      sendVideoLinkProfile();

   if ( m_IndexMaxKeyFrame == m_SelectedIndex )
   {
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.uMaxAutoKeyframeIntervalMs = m_pItemsSlider[3]->getCurrentValue();

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( m_IndexRetransmissionsFast == m_SelectedIndex )
   {
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.uVideoExtraFlags &= ~VIDEO_FLAG_RETRANSMISSIONS_FAST;
      if ( 1 == m_pItemsSelect[18]->getSelectedIndex() )
         paramsNew.uVideoExtraFlags |= VIDEO_FLAG_RETRANSMISSIONS_FAST;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( m_IndexDataRate == m_SelectedIndex )
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

   if ( m_IndexHDMIOutput == m_SelectedIndex )
   {
      video_parameters_t paramsOld;
      memcpy(&paramsOld, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      int index = m_pItemsSelect[1]->getSelectedIndex();
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

   if ( m_IndexIgnoreTxSpikes == m_SelectedIndex )
   {
      video_parameters_t paramsOld;
      memcpy(&paramsOld, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      int index = m_pItemsSelect[17]->getSelectedIndex();
      if ( index == 0 )
         g_pCurrentModel->video_params.uVideoExtraFlags &= ~(VIDEO_FLAG_IGNORE_TX_SPIKES);
      else
         g_pCurrentModel->video_params.uVideoExtraFlags |= VIDEO_FLAG_IGNORE_TX_SPIKES;

      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      memcpy(&g_pCurrentModel->video_params, &paramsOld, sizeof(video_parameters_t));

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
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
         g_pCurrentModel->video_params.uVideoExtraFlags &= ~(VIDEO_FLAG_FILL_H264_SPT_TIMINGS);
      else
         g_pCurrentModel->video_params.uVideoExtraFlags |= VIDEO_FLAG_FILL_H264_SPT_TIMINGS;

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

   if ( m_IndexEnableAdaptiveQuantization == m_SelectedIndex ||
        m_IndexAdaptiveQuantizationStrength == m_SelectedIndex )
   {
      sendVideoLinkProfile();
   }

   if ( m_IndexResetParams == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      // Reset expert params
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_RESET_VIDEO_LINK_PROFILE, 0, NULL, 0) )
         valuesToUI();
      return;
   }
}
