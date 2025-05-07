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

#include "menu.h"
#include "menu_vehicle_video_encodings.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "../../base/utils.h"
#include "../../utils/utils_controller.h"

const char* s_szWarningBitrate = "Warning: The current radio datarate is to small for the current video encoding settings.\n You will experience delays in the video stream.\n Increase the radio datarate, or decrease the video bitrate, decrease the encoding params.";

MenuVehicleVideoEncodings::MenuVehicleVideoEncodings(void)
:Menu(MENU_ID_VEHICLE_EXPERT_ENCODINGS, L("Advanced Video Settings (Expert)"), NULL)
{
   m_Width = 0.37;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;
   //m_ShowBitrateWarning = false;
   float fSliderWidth = 0.12;
   char szBuff[256];
   setSubTitle(L("Change advanced vehicle encoding settings (for expert users)"));
   
  
   strcpy(szBuff, L("Settings for unknown video profile:"));
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_BEST_PERF ) 
      strcpy(szBuff, L("Settings for High Performance video profile:"));
   else if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_HIGH_QUALITY ) 
      strcpy(szBuff, L("Settings for High Quality video profile:"));
   else if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_USER ) 
      strcpy(szBuff, L("Settings for User video profile:"));
   else
      sprintf(szBuff, "Settings for %s video profile:", str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile));
   addTopLine(szBuff);

   m_IndexHDMIOutput = -1;
   if ( ! g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      m_pItemsSelect[1] = new MenuItemSelect(L("Enable Vehicle Local HDMI Output"), L("Enables or disables video output the the HDMI port on the vehicle."));  
      m_pItemsSelect[1]->addSelection(L("Off"));
      m_pItemsSelect[1]->addSelection(L("On"));
      m_pItemsSelect[1]->setIsEditable();
      m_IndexHDMIOutput = addMenuItem(m_pItemsSelect[1]);
   }

   m_IndexNoise = -1;
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      m_pItemsSelect[20] = new MenuItemSelect(L("Noise Level"), L("Sets the video noise level. Lower values means better performance but more noise in the live video."));  
      m_pItemsSelect[20]->addSelection(L("0"));
      m_pItemsSelect[20]->addSelection(L("1"));
      m_pItemsSelect[20]->addSelection(L("Auto"));
      m_pItemsSelect[20]->setIsEditable();
      m_IndexNoise = addMenuItem(m_pItemsSelect[20]);
   }

   addMenuItem(new MenuItemSection(L("Data & Error Correction Settings")));

   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   int iMaxSize = p->iDebugMaxPacketSize-sizeof(t_packet_header)-sizeof(t_packet_header_video_segment);
   iMaxSize = (iMaxSize/10)*10;

   int iMaxPackets = MAX_TOTAL_PACKETS_IN_BLOCK;
   if ( NULL != g_pCurrentModel )
      iMaxPackets = g_pCurrentModel->hwCapabilities.iMaxTxVideoBlockPackets;

   m_IndexPacketSize = -1;
   if ( (NULL != pCS) && (0 != pCS->iDeveloperMode) )
   if ( (NULL != g_pCurrentModel) && (! g_pCurrentModel->is_spectator) )
   {
      m_pItemsSlider[0] = new MenuItemSlider(L("Packet size"), L("How big is each indivisible video packet over the air link. No major impact in link quality. Smaller packets and more EC data increases the chance of error correction but also increases the video latency."), 100,iMaxSize,iMaxSize/2, fSliderWidth);
      m_pItemsSlider[0]->setStep(10);
      m_IndexPacketSize = addMenuItem(m_pItemsSlider[0]);
      m_pMenuItems[m_IndexPacketSize]->setTextColor(get_Color_Dev());
   }

   m_pItemsSlider[1] = new MenuItemSlider(L("Video block size"), L("How many indivisible packets are in a video block. This has an impact on link recovery and error correction. Bigger values might increase the delay in video stream when link is degraded, but also increase the chance of error correction."), 2, iMaxPackets/2, iMaxPackets/4, fSliderWidth);
   m_IndexBlockPackets = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSelect[18] = new MenuItemSelect(L("Error Correction"), L("How much error correction data to add to the video data."));
   m_pItemsSelect[18]->addSelection(L("None"));
   m_pItemsSelect[18]->addSelection("10%");
   m_pItemsSelect[18]->addSelection("20%");
   m_pItemsSelect[18]->addSelection("30%");
   m_pItemsSelect[18]->addSelection("40%");
   m_pItemsSelect[18]->addSelection("50%");
   m_pItemsSelect[18]->addSelection("60%");
   m_pItemsSelect[18]->addSelection("70%");
   m_pItemsSelect[18]->setIsEditable();
   m_IndexBlockECRate = addMenuItem(m_pItemsSelect[18]);

   m_IndexECSchemeSpread = -1;
   /*
   m_pItemsSelect[19] = new MenuItemSelect(L("EC Spreading Factor"), L("Spreads the EC packets accross multiple video blocks."));
   m_pItemsSelect[19]->addSelection("0");
   m_pItemsSelect[19]->addSelection("1");
   m_pItemsSelect[19]->addSelection("2");
   m_pItemsSelect[19]->addSelection("3");
   m_pItemsSelect[19]->setIsEditable();
   m_IndexECSchemeSpread = addMenuItem(m_pItemsSelect[19]);
   */

   addMenuItem(new MenuItemSection(L("H264/H265 Encoder Settings")));

   m_IndexH264Profile = -1;
   m_IndexH264Level = -1;
   m_IndexH264Refresh = -1;

   if ( ! g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      m_pItemsSelect[4] = new MenuItemSelect("H264/H265 Profile", "The higher the H264/H265 profile, the higher the CPU usage on encode and decode and higher the end to end video latencey. Higher profiles can have lower video quality as more compression algorithms are used.");
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

      m_pItemsSelect[6] = new MenuItemSelect("H264/H265 Inter Refresh", "");  
      m_pItemsSelect[6]->addSelection("Cyclic");
      m_pItemsSelect[6]->addSelection("Adaptive");
      m_pItemsSelect[6]->addSelection("Both");
      m_pItemsSelect[6]->addSelection("Cyclic Rows");
      m_pItemsSelect[6]->setIsEditable();
      m_IndexH264Refresh = addMenuItem(m_pItemsSelect[6]);
   }

   m_IndexAutoKeyframe = -1;
   m_IndexKeyframeManual = -1;
   m_pMenuItemVideoKeyframeWarning = NULL;

   m_pItemsSelect[22] = new MenuItemSelect(L("Auto Keyframing"), L("Automatic keyframe adjustment based on radio link conditions."));
   m_pItemsSelect[22]->addSelection(L("No"));
   m_pItemsSelect[22]->addSelection(L("Yes"));
   m_pItemsSelect[22]->setIsEditable();
   m_IndexAutoKeyframe = addMenuItem(m_pItemsSelect[22]);

   m_pItemsSlider[6] = new MenuItemSlider(L("Max Auto Keyframe (ms)"), 50,20000,0, fSliderWidth);
   m_pItemsSlider[6]->setTooltip(L("Sets the max allowed keyframe interval (in miliseconds) when auto adjusting is enabled for video keyframe interval parameter."));
   m_pItemsSlider[6]->setStep(10);
   m_IndexMaxKeyFrame = addMenuItem(m_pItemsSlider[6]);

   m_pItemsSlider[5] = new MenuItemSlider(L("Keyframe interval (ms)"), L("A keyframe is added every [n] miliseconds. Bigger keyframe values usually results in better quality video and lower bandwidth requirements but longer breakups if any."), 50,20000,200, fSliderWidth);
   m_pItemsSlider[5]->setStep(10);
   m_pItemsSlider[5]->setSufix("ms");
   m_IndexKeyframeManual = addMenuItem(m_pItemsSlider[5]);
   m_pMenuItemVideoKeyframeWarning = new MenuItemText("", true);
   m_pMenuItemVideoKeyframeWarning->setHidden(true);
   addMenuItem(m_pMenuItemVideoKeyframeWarning);

   m_pItemsSelect[12] = new MenuItemSelect("H264/H265 Slices", "Split video frames into multiple smaller parts. When having heavy radio interference there is a chance that not the entire video is corrupted if video is sliced up. But is uses more processing power.");
   for( int i=1; i<=16; i++ )
   {
      sprintf(szBuff, "%d", i);
      m_pItemsSelect[12]->addSelection(szBuff);
   }
   m_pItemsSelect[12]->setIsEditable();
   m_IndexH264Slices = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSelect[17] = new MenuItemSelect("Remove extra H264/H265 frames", "Removes frames not needed for video decoding.");  
   m_pItemsSelect[17]->addSelection("No");
   m_pItemsSelect[17]->addSelection("Yes");
   m_pItemsSelect[17]->setIsEditable();
   m_IndexRemoveH264PPS = addMenuItem(m_pItemsSelect[17]);

   m_IndexInsertH264PPS = -1;
   m_IndexInsertH264SPSTimings = -1;
   if ( ! g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      m_pItemsSelect[7] = new MenuItemSelect("Insert H264/H265 PPS Headers", "");  
      m_pItemsSelect[7]->addSelection("No");
      m_pItemsSelect[7]->addSelection("Yes");
      m_pItemsSelect[7]->setIsEditable();
      m_IndexInsertH264PPS = addMenuItem(m_pItemsSelect[7]);

      m_pItemsSelect[11] = new MenuItemSelect("Fill H264/H265 SPS Timings", "");  
      m_pItemsSelect[11]->addSelection("No");
      m_pItemsSelect[11]->addSelection("Yes");
      m_pItemsSelect[11]->setIsEditable();
      m_IndexInsertH264SPSTimings = addMenuItem(m_pItemsSelect[11]);
   }

   m_IndexIPQuantizationDelta = -1;
   m_IndexCustomQuant = -1;
   m_IndexQuantValue = -1;

   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      m_pItemsSlider[18] = new MenuItemSlider("I-P Frames Quantization Delta", -12,12,-2,fSliderWidth);
      m_pItemsSlider[18]->setTooltip("Sets a relative quantization difference between P and I frames. Higher values increase the quality of I frames compared to P frames.");
      m_IndexIPQuantizationDelta = addMenuItem(m_pItemsSlider[18]);
   }
   else
   {
      m_pItemsSelect[8] = new MenuItemSelect("Auto H264/H265 quantization", "Use default quantization for the H264/H265 video encoding, or set a custom value.");  
      m_pItemsSelect[8]->addSelection("No");
      m_pItemsSelect[8]->addSelection("Yes");
      m_pItemsSelect[8]->setIsEditable();
      m_IndexCustomQuant = addMenuItem(m_pItemsSelect[8]);

      m_pItemsSlider[4] = new MenuItemSlider("Manual video quantization", 0,40,20,fSliderWidth);
      m_pItemsSlider[4]->setTooltip("Sets a fixed H264 quantization parameter for the video stream. Higher values reduces quality but decreases bitrate requirements and latency. The default is about 16");
      m_IndexQuantValue = addMenuItem(m_pItemsSlider[4]);
   }

   m_pItemsSelect[14] = new MenuItemSelect("Enable adaptive H264/H265 quantization", "Enable algorithm that auto adjusts the H264/H265 quantization to match the desired video bitrate in realtime.");  
   m_pItemsSelect[14]->addSelection("No");
   m_pItemsSelect[14]->addSelection("Yes");
   m_pItemsSelect[14]->setIsEditable();
   m_IndexEnableAdaptiveQuantization = addMenuItem(m_pItemsSelect[14]);

   m_pItemsSelect[15] = new MenuItemSelect("Adaptive H264/H265 quantization strength", "How strongh should the algorithm be. The algorithm that auto adjusts the H264/H265 quantization to match the desired video bitrate in realtime.");  
   m_pItemsSelect[15]->addSelection("Low");
   m_pItemsSelect[15]->addSelection("High");
   m_pItemsSelect[15]->setIsEditable();
   m_IndexAdaptiveH264QuantizationStrength = addMenuItem(m_pItemsSelect[15]);

   m_IndexResetParams = addMenuItem(new MenuItem(L("Reset Video Profile"), L("Resets the current video profile to the default values.")));
}

void MenuVehicleVideoEncodings::valuesToUI()
{
   int iVideoProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
   u32 uVideoProfileEncodingFlags = g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileEncodingFlags;
   int adaptiveKeyframe = (uVideoProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME)?1:0;

   if (adaptiveKeyframe && (! g_pCurrentModel->isVideoLinkFixedOneWay()) )
   {
      m_pItemsSelect[22]->setSelectedIndex(1);
      m_pItemsSlider[6]->setEnabled(true);
      m_pItemsSlider[5]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[22]->setSelectedIndex(0);
      m_pItemsSlider[6]->setEnabled(false);
      m_pItemsSlider[5]->setEnabled(true);
   }
   m_pItemsSlider[6]->setCurrentValue(g_pCurrentModel->video_params.uMaxAutoKeyframeIntervalMs);

   if ( NULL != m_pMenuItemVideoKeyframeWarning )
   {
      m_pMenuItemVideoKeyframeWarning->setHidden(true);
      if ( ! g_pCurrentModel->isVideoLinkFixedOneWay() )
      if ( g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
      {
         m_pMenuItemVideoKeyframeWarning->setTitle(L("Adaptive keyframe is turned on. Keyframe interval will be dynamically adjusted by Ruby. If you want to use a fixed keyframe, turn off Adaptive Keyframe from Bidirectional settings menu or set the video link as One Way."));
         m_pMenuItemVideoKeyframeWarning->setHidden(false);
      }
   }
   if ( -1 != m_IndexKeyframeManual )
   {
      int keyframe_ms = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe_ms;
      if ( keyframe_ms < 0 )
         keyframe_ms = -keyframe_ms;
      m_pItemsSlider[5]->setCurrentValue(keyframe_ms);
   }

   if ( m_IndexPacketSize != -1 )
      m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].video_data_length);

   m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].iBlockPackets);
   m_pItemsSlider[1]->setEnabled(true);

   int iECPercent = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].iECPercentage;
   iECPercent = iECPercent/10;
   if ( iECPercent > 7 )
      iECPercent = 7;
   m_pItemsSelect[18]->setSelectedIndex(iECPercent);

   u32 uECSpreadHigh = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT)?1:0;
   u32 uECSpreadLow = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT)?1:0;
   u32 uECSpread = uECSpreadLow + (uECSpreadHigh*2);

   if ( -1 != m_IndexECSchemeSpread )
      m_pItemsSelect[19]->setSelectedIndex((int) uECSpread);

   if ( -1 != m_IndexNoise )
      m_pItemsSelect[20]->setSelectedIndex(g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_MASK_NOISE);

   if ( -1 != m_IndexH264Profile )
      m_pItemsSelect[4]->setSelectedIndex((g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT)?1:0);

   /*
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
   */

   if ( -1 != m_IndexH264Profile )
      m_pItemsSelect[4]->setSelection(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264profile);
   if ( -1 != m_IndexH264Level )
      m_pItemsSelect[5]->setSelection(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264level);
   if ( -1 != m_IndexH264Refresh )
      m_pItemsSelect[6]->setSelection(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264refresh);
   if ( -1 != m_IndexInsertH264PPS )
      m_pItemsSelect[7]->setSelection(g_pCurrentModel->video_params.iInsertPPSVideoFrames);
   
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH )
      m_pItemsSelect[15]->setSelectedIndex(1);
   else
      m_pItemsSelect[15]->setSelectedIndex(0);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION )
   {
      m_pItemsSelect[14]->setSelectedIndex(1);
      m_pItemsSelect[15]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[14]->setSelectedIndex(0);
      m_pItemsSelect[15]->setEnabled(false);    
   }

   if ( -1 != m_IndexIPQuantizationDelta )
      m_pItemsSlider[18]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].iIPQuantizationDelta);

   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization > 0 )
   {
      if ( -1 != m_IndexCustomQuant )
         m_pItemsSelect[8]->setSelection(0);
      if ( -1 != m_IndexQuantValue )
      {
         m_pItemsSlider[4]->setCurrentValue(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization);
         m_pItemsSlider[4]->setEnabled(true);
      }
      m_pItemsSelect[14]->setEnabled(false);
      m_pItemsSelect[15]->setEnabled(false);
   }
   else
   {
      if ( -1 != m_IndexCustomQuant )
         m_pItemsSelect[8]->setSelection(1);
      if ( -1 != m_IndexQuantValue )
      {
         m_pItemsSlider[4]->setCurrentValue(-g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization);
         m_pItemsSlider[4]->setEnabled(false);
      }
      m_pItemsSelect[14]->setEnabled(true);
      //m_pItemsSelect[15]->setEnabled(true);
   }

   if ( -1 != m_IndexInsertH264SPSTimings )
      m_pItemsSelect[11]->setSelectedIndex(g_pCurrentModel->video_params.iInsertSPTVideoFramesTimings);
   if ( -1 != m_IndexRemoveH264PPS )
      m_pItemsSelect[17]->setSelectedIndex(g_pCurrentModel->video_params.iRemovePPSVideoFrames);
   
   m_pItemsSelect[12]->setSelectedIndex(g_pCurrentModel->video_params.iH264Slices-1);
   //if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
   //{
   //   m_pItemsSelect[12]->setSelectedIndex(0);
   //   m_pItemsSelect[12]->setEnabled(false);
   //}
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

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
   }
   RenderEnd(y0);
}

void MenuVehicleVideoEncodings::sendVideoLinkProfile()
{
   if ( get_sw_version_build(g_pCurrentModel) < 283 )
   {
      addMessage(L("Video functionality has changed. You need to update your vehicle sowftware."));
      return;
   }
   type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      memcpy(&profiles[i], &(g_pCurrentModel->video_link_profiles[i]), sizeof(type_video_link_profile));

   type_video_link_profile* pProfile = &(profiles[g_pCurrentModel->video_params.user_selected_video_link_profile]);
   int iVideoProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
   if ( -1 != m_IndexNoise )
   {
      int iNoise = m_pItemsSelect[20]->getSelectedIndex();
      pProfile->uProfileFlags &= ~ VIDEO_PROFILE_FLAGS_MASK_NOISE;
      pProfile->uProfileFlags |= ((u32)(iNoise)) & VIDEO_PROFILE_FLAGS_MASK_NOISE;
   }

   if ( 0 == m_pItemsSelect[22]->getSelectedIndex() )
      pProfile->uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
   else
      pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;

   if ( -1 != m_IndexKeyframeManual )
      pProfile->keyframe_ms = m_pItemsSlider[5]->getCurrentValue();

   if ( -1 != m_IndexPacketSize )
      pProfile->video_data_length = m_pItemsSlider[0]->getCurrentValue();
   
   pProfile->iBlockPackets = m_pItemsSlider[1]->getCurrentValue();
   pProfile->iECPercentage = m_pItemsSelect[18]->getSelectedIndex() * 10;
   g_pCurrentModel->convertECPercentageToData(pProfile);

   if ( -1 != m_IndexECSchemeSpread )
   {
      u32 uECSpread = (u32) m_pItemsSelect[19]->getSelectedIndex();

      pProfile->uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT | VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT);
      if ( uECSpread & 0x01 )
         pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT;
      if ( uECSpread & 0x02 )
         pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT;
   }

   pProfile->uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION | VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH);
   if ( 1 == m_pItemsSelect[14]->getSelectedIndex() )
      pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION;
   if ( 1 == m_pItemsSelect[15]->getSelectedIndex() )
      pProfile->uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH;
     
   log_line("Sending new video flags to vehicle for video profile %s: %s, %s, %s",
      str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile),
      (pProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS)?"Retransmissions=On":"Retransmissions=Off",
      (pProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK)?"AdaptiveVideo=On":"AdaptiveVideo=Off",
      (pProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?"AdaptiveUseControllerInfo=On":"AdaptiveUseControllerInfo=Off"
      );

   log_line("Adaptive video quantization: %s, %s",
    (pProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION)?"On":"ROff",
    (pProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH)?"Strength: High":"Strength: Low");

   if ( -1 != m_IndexH264Profile )
      pProfile->h264profile = m_pItemsSelect[4]->getSelectedIndex();
   if ( -1 != m_IndexH264Level )
      pProfile->h264level = m_pItemsSelect[5]->getSelectedIndex();
   if ( -1 != m_IndexH264Refresh )
      pProfile->h264refresh = m_pItemsSelect[6]->getSelectedIndex();

   if ( (-1 != m_IndexCustomQuant) && (-1 != m_IndexQuantValue) )
   {
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
   }
   log_line("H264 quantization: %d", pProfile->h264quantization);

   if ( -1 != m_IndexIPQuantizationDelta )
      pProfile->iIPQuantizationDelta = m_pItemsSlider[18]->getCurrentValue();

   /*
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
   */

   if ( pProfile->uProfileFlags == g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileFlags )
   if ( pProfile->radio_datarate_video_bps == g_pCurrentModel->video_link_profiles[iVideoProfile].radio_datarate_video_bps )
   if ( pProfile->video_data_length == g_pCurrentModel->video_link_profiles[iVideoProfile].video_data_length )
   if ( pProfile->iBlockPackets == g_pCurrentModel->video_link_profiles[iVideoProfile].iBlockPackets )
   if ( pProfile->iBlockECs == g_pCurrentModel->video_link_profiles[iVideoProfile].iBlockECs )
   if ( pProfile->iECPercentage == g_pCurrentModel->video_link_profiles[iVideoProfile].iECPercentage )
   if ( pProfile->uProfileEncodingFlags == g_pCurrentModel->video_link_profiles[iVideoProfile].uProfileEncodingFlags )
   if ( pProfile->h264level == g_pCurrentModel->video_link_profiles[iVideoProfile].h264level )
   if ( pProfile->h264profile == g_pCurrentModel->video_link_profiles[iVideoProfile].h264profile )
   if ( pProfile->h264refresh == g_pCurrentModel->video_link_profiles[iVideoProfile].h264refresh )
   if ( pProfile->h264quantization == g_pCurrentModel->video_link_profiles[iVideoProfile].h264quantization )
   if ( pProfile->iIPQuantizationDelta == g_pCurrentModel->video_link_profiles[iVideoProfile].iIPQuantizationDelta )
      return;

   // Propagate changes to lower video profiles

   propagate_video_profile_changes_to_lower_profiles(g_pCurrentModel, &g_pCurrentModel->video_link_profiles[iVideoProfile], pProfile, &(profiles[0]));

   log_line("Sending video encoding flags: %s", str_format_video_encoding_flags(pProfile->uProfileEncodingFlags));

   u8 buffer[1024];
   memcpy( buffer, &(profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile) );

   log_line("Sending new video link profiles to vehicle.");
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES, 0, buffer, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile)) )
      valuesToUI();
}

void MenuVehicleVideoEncodings::sendVideoParams()
{
   if ( get_sw_version_build(g_pCurrentModel) < 283 )
   {
      addMessage(L("Video functionality has changed. You need to update your vehicle sowftware."));
      return;
   }
   video_parameters_t params;
   memcpy(&params, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
   
   if ( -1 != m_IndexInsertH264PPS )
      params.iInsertPPSVideoFrames = m_pItemsSelect[7]->getSelectedIndex();
   if ( -1 != m_IndexInsertH264SPSTimings )
      params.iInsertSPTVideoFramesTimings = m_pItemsSelect[11]->getSelectedIndex();
   if ( -1 != m_IndexRemoveH264PPS )
      params.iRemovePPSVideoFrames = m_pItemsSelect[17]->getSelectedIndex();
  
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&params, sizeof(video_parameters_t)) )
      valuesToUI();
}

void MenuVehicleVideoEncodings::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
   if ( ((m_IndexHDMIOutput != -1) && (m_IndexHDMIOutput == m_SelectedIndex)) ||
        (m_IndexCustomQuant == m_SelectedIndex) ||
        (m_IndexQuantValue == m_SelectedIndex) ||
        (m_IndexEnableAdaptiveQuantization == m_SelectedIndex) ||
        (m_IndexAdaptiveH264QuantizationStrength == m_SelectedIndex) )
   {
      addUnsupportedMessageOpenIPC(NULL);
      valuesToUI();
      return;    
   }

   if ( ((m_IndexPacketSize != -1) && (m_IndexPacketSize == m_SelectedIndex)) || 
        m_IndexBlockPackets == m_SelectedIndex ||
        m_IndexBlockECRate == m_SelectedIndex || 
        ((-1 != m_IndexECSchemeSpread) && (m_IndexECSchemeSpread == m_SelectedIndex)) )
      sendVideoLinkProfile();

   if ( (-1 != m_IndexHDMIOutput) && (m_IndexHDMIOutput == m_SelectedIndex) )
   {
      if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
      {
         addUnsupportedMessageOpenIPC(NULL);
         return;
      }
      if ( get_sw_version_build(g_pCurrentModel) < 283 )
      {
         addMessage(L("Video functionality has changed. You need to update your vehicle sowftware."));
         return;
      }
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

   if ( (-1 != m_IndexNoise) && (m_IndexNoise == m_SelectedIndex) )
      sendVideoLinkProfile();

   if ( (-1 != m_IndexH264Profile) && (m_IndexH264Profile == m_SelectedIndex) )
      sendVideoLinkProfile();
   if ( (-1 != m_IndexH264Level) && (m_IndexH264Level == m_SelectedIndex) )
      sendVideoLinkProfile();
   if ( (-1 != m_IndexH264Refresh) && (m_IndexH264Refresh == m_SelectedIndex) )
      sendVideoLinkProfile();

   if ( (-1 != m_IndexAutoKeyframe) && (m_IndexAutoKeyframe == m_SelectedIndex) )
   {
      sendVideoLinkProfile();
      return;
   }
   if ( (-1 != m_IndexKeyframeManual) && (m_IndexKeyframeManual == m_SelectedIndex) )
   {
      sendVideoLinkProfile();
      return;
   }

   if ( (-1 != m_IndexRemoveH264PPS) && (m_IndexRemoveH264PPS == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }
   if ( (-1 != m_IndexInsertH264PPS) && (m_IndexInsertH264PPS == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }
   if ( (-1 != m_IndexInsertH264SPSTimings) && (m_IndexInsertH264SPSTimings == m_SelectedIndex) )
   {
      sendVideoParams();
      return;
   }

   if ( m_IndexH264Slices == m_SelectedIndex )
   {
      if ( get_sw_version_build(g_pCurrentModel) < 283 )
      {
         addMessage(L("Video functionality has changed. You need to update your vehicle sowftware."));
         return;
      }
      video_parameters_t paramsOld;
      memcpy(&paramsOld, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      g_pCurrentModel->video_params.iH264Slices = 1 + m_pItemsSelect[12]->getSelectedIndex();

      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      memcpy(&g_pCurrentModel->video_params, &paramsOld, sizeof(video_parameters_t));

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      else
      {
         #if defined HW_PLATFORM_RASPBERRY
         if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
            addMessage(L("Slice units might not work properly when a Raspberry Pi controller is paired with a vehicle running on OpenIPC hardware."));
         #endif
      }
      return;
   }

   if ( (-1 != m_IndexCustomQuant) && (-1 != m_IndexQuantValue) )
   if ( (m_IndexCustomQuant == m_SelectedIndex) || (m_IndexQuantValue == m_SelectedIndex) )
   if ( menu_check_current_model_ok_for_edit() )
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

   if ( (-1 != m_IndexIPQuantizationDelta) && (m_IndexIPQuantizationDelta == m_SelectedIndex) )
   {
      sendVideoLinkProfile();
      return;
   }
   if ( (m_IndexEnableAdaptiveQuantization == m_SelectedIndex) ||
        (m_IndexAdaptiveH264QuantizationStrength == m_SelectedIndex) )
   {
      sendVideoLinkProfile();
      return;
   }

   if ( m_IndexMaxKeyFrame == m_SelectedIndex )
   {
      if ( get_sw_version_build(g_pCurrentModel) < 283 )
      {
         addMessage(L("Video functionality has changed. You need to update your vehicle sowftware."));
         return;
      }
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.uMaxAutoKeyframeIntervalMs = m_pItemsSlider[6]->getCurrentValue();

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( (m_IndexResetParams == m_SelectedIndex) && menu_check_current_model_ok_for_edit() )
   {
      // Reset expert params
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_RESET_VIDEO_LINK_PROFILE, 0, NULL, 0) )
         valuesToUI();
      return;
   }
}
