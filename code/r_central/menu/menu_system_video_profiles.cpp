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
#include "menu_objects.h"
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_system_video_profiles.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "../../radio/radiolink.h"
#include "../../base/utils.h"
#include "../process_router_messages.h"
#include "../../utils/utils_controller.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>


MenuSystemVideoProfiles::MenuSystemVideoProfiles(void)
:Menu(MENU_ID_SYSTEM_VIDEO_PROFILES, "Video Profiles", NULL)
{
   m_Width = 0.44;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.15;
   float fSliderWidth = 0.12 * Menu::getScaleFactor();
   Preferences* p = get_Preferences();

   log_line("MenuSystemVideoProfiles: Enter menu.");
   char szBuff[256];
   strcpy(szBuff, "Current video profile: Unknown");
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_BEST_PERF ) 
      strcpy(szBuff, "Current video profile: High Performance");
   else if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_HIGH_QUALITY ) 
      strcpy(szBuff, "Current video profile: High Quality");
   else if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_USER ) 
      strcpy(szBuff, "Current video profile: User");
   else
      sprintf(szBuff, "Current video profile: %s", str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile));
   addTopLine(szBuff);
   log_line("MenuSystemVideoProfiles: %s", szBuff);

   for( int i=0; i<200; i++ )
   {
      m_pItemsSelect[i] = NULL;
      m_pItemsSlider[i] = NULL;
   }

   strcpy(szBuff, "Max Retransmission Window");
   if ( NULL != g_pCurrentModel )
   {
      char szUserProfile[64];
      szUserProfile[0] = 0;
      strcpy(szUserProfile, str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile));
      sprintf(szBuff, "Max Retransmission Window (for %s)", szUserProfile);
   }
   m_pItemsSlider[0] = new MenuItemSlider(szBuff, "Maximum window size (in miliseconds) to send/retry and wait for retransmissions to happen.", 0,1000,10, fSliderWidth);
   m_pItemsSlider[0]->setStep(5);
   m_IndexRetransmitWindow = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSlider[5] = new MenuItemSlider("Retry Retransmission After Timeout (ms)", "When a retransmission of a chunck is requrested, this is the timeout after which to retry the retransmission of the missing video chunk (miliseconds).", 0,50,10, fSliderWidth);
   m_pItemsSlider[5]->setStep(1);
   m_IndexRetransmitRetryTimeout = addMenuItem(m_pItemsSlider[5]);

   m_pItemsSlider[6] = new MenuItemSlider("Request Retransmissions On Video Silence (ms)", "Request a retransmission of next video packets after a period of silence (missing video data).  In miliseconds, 0 to disable.", 0,500,10, fSliderWidth);
   m_pItemsSlider[6]->setStep(5);
   m_IndexRetransmitRequestOnTimeout = addMenuItem(m_pItemsSlider[6]);

   m_pItemsSlider[1] = new MenuItemSlider("Disable Retransmissions on Link Timeout (sec)", "When the controller does not receive commands confirmation from vehicle, disable retransmissions requests and just output the video stream as is. (0 value disables this functionality)", 0,10,5, fSliderWidth);
   m_pItemsSlider[1]->setStep(1);
   m_IndexDisableRetransmissionsTimeout = addMenuItem(m_pItemsSlider[1]);

   m_pItemsSelect[0] = new MenuItemSelect("Retransmissions Duplication", "How much to duplicate the retransmissions responses in bad conditions.");
   m_pItemsSelect[0]->addSelection("Auto");
   m_pItemsSelect[0]->addSelection("0%");
   m_pItemsSelect[0]->addSelection("10%");
   m_pItemsSelect[0]->addSelection("20%");
   m_pItemsSelect[0]->addSelection("30%");
   m_pItemsSelect[0]->addSelection("40%");
   m_pItemsSelect[0]->addSelection("50%");
   m_pItemsSelect[0]->addSelection("60%");
   m_pItemsSelect[0]->addSelection("70%");
   m_pItemsSelect[0]->addSelection("80%");
   m_pItemsSelect[0]->addSelection("90%");
   m_pItemsSelect[0]->addSelection("100%");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexRetransmissionDuplication = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSlider[8] = new MenuItemSlider("Default max auto keyframe", 50,20000,0, fSliderWidth);
   m_pItemsSlider[8]->setTooltip("Sets the max allowed keyframe interval (in miliseconds) when auto adjusting the video keyframe interval.");
   m_pItemsSlider[8]->setStep(10);
   m_IndexDefaultAutoKeyframe = addMenuItem(m_pItemsSlider[8]);

   m_pItemsSlider[7] = new MenuItemSlider("Lowest Allowed Video Bitrate (Mbps)", 2,200,0, fSliderWidth);
   m_pItemsSlider[7]->setTooltip("Sets the lowest allowed video bitrate when auto adjustments are enabled on the video streams. The video stream bitrate will not go lower than this even when radio link is bad.");
   m_pItemsSlider[7]->enableHalfSteps();
   m_IndexLowestAllowedVideoBitrate = addMenuItem(m_pItemsSlider[7]);


   m_pItemsSlider[2] = new MenuItemSlider("Video Link Adjustment Strength", "How aggressive should the auto video link adjustments be. 1 is the slowest adjustment strength, 10 is the fastest and most aggressive adjustment strength.", 1,10,5, fSliderWidth);
   m_pItemsSlider[2]->setStep(1);
   m_IndexVideoAdjustStrength = addMenuItem(m_pItemsSlider[2]);

   log_line("MenuSystemVideoProfiles: Menu initialising (2)");

   for( int k=VIDEO_PROFILE_MQ; k<=VIDEO_PROFILE_LQ; k++ )
   {
      if ( k == VIDEO_PROFILE_MQ )
         addMenuItem(new MenuItemSection("Video Profile: Medium Quality"));
      if ( k == VIDEO_PROFILE_LQ )
         addMenuItem(new MenuItemSection("Video Profile: Low Quality"));

      m_pItemsSelect[k*20+11] = new MenuItemSelect("Radio Data Rate for Video", "Actual radio data rate to use for this video profile for video data transmission.");
      if ( k == VIDEO_PROFILE_MQ )
         m_pItemsSelect[k*20+11]->addSelection("Auto (Radio-1)");
      else
         m_pItemsSelect[k*20+11]->addSelection("Auto (Radio-2)");
      for( int i=0; i<getDataRatesCount(); i++ )
      {
         str_getDataRateDescription(getDataRatesBPS()[i], 0, szBuff);
         m_pItemsSelect[k*20+11]->addSelection(szBuff);
      }
      if ( NULL != g_pCurrentModel && (g_pCurrentModel->radioInterfacesParams.interface_supported_bands[0] & RADIO_HW_SUPPORTED_BAND_58 || g_pCurrentModel->radioInterfacesParams.interface_supported_bands[1] & RADIO_HW_SUPPORTED_BAND_58) )
      for( int i=0; i<=MAX_MCS_INDEX; i++ )
      {
         str_getDataRateDescription(-1-i, 0, szBuff);
         m_pItemsSelect[k*20+11]->addSelection(szBuff);
      }
      m_pItemsSelect[k*20+11]->setIsEditable();
      m_IndexVideoProfile_VideoRadioRate[k] = addMenuItem(m_pItemsSelect[k*20+11]);

      m_pItemsSelect[k*20+12] = new MenuItemSelect("Radio Data Rate for Data", "Actual radio data rate to use for this video profile for data transmission.");
      if ( k == VIDEO_PROFILE_MQ )
         m_pItemsSelect[k*20+12]->addSelection("Auto (Radio-1)");
      else
         m_pItemsSelect[k*20+12]->addSelection("Auto (Radio-2)");
      for( int i=0; i<getDataRatesCount(); i++ )
      {
         str_getDataRateDescription(getDataRatesBPS()[i], 0, szBuff);
         m_pItemsSelect[k*20+12]->addSelection(szBuff);
      }
      if ( NULL != g_pCurrentModel && (g_pCurrentModel->radioInterfacesParams.interface_supported_bands[0] & RADIO_HW_SUPPORTED_BAND_58 || g_pCurrentModel->radioInterfacesParams.interface_supported_bands[1] & RADIO_HW_SUPPORTED_BAND_58) )
      for( int i=0; i<=MAX_MCS_INDEX; i++ )
      {
         str_getDataRateDescription(-1-i, 0, szBuff);
         m_pItemsSelect[k*20+12]->addSelection(szBuff);
      }
      m_pItemsSelect[k*20+12]->setIsEditable();
      m_IndexVideoProfile_DataRadioRate[k] = addMenuItem(m_pItemsSelect[k*20+12]);

      m_pItemsSlider[k*20+10] = new MenuItemSlider("Video Bitrate (Mbps)", 0,80,0, fSliderWidth);
      m_pItemsSlider[k*20+10]->setTooltip("Sets the default video bitrate for this profile. Will auto adjust lower if adaptive video link is on. Set to 0 to use the main user profile video bitrate as default value.");
      m_pItemsSlider[k*20+10]->enableHalfSteps();
      m_IndexVideoProfile_Bitrate[k] = addMenuItem(m_pItemsSlider[k*20+10]);

      m_pItemsSlider[k*20+16] = new MenuItemSlider("FPS", "Setting it to 0 uses the FPS set by the user in the main active video profile.", 2,90, 30, fSliderWidth);
      m_IndexVideoProfile_FPS[k] = addMenuItem(m_pItemsSlider[k*20+16]);

      m_pItemsSelect[k*20+17] = new MenuItemSelect("Keyframing", "Enables or disables automatic keyframe adjustment.");  
      m_pItemsSelect[k*20+17]->addSelection("Manual");
      m_pItemsSelect[k*20+17]->addSelection("Auto");
      m_pItemsSelect[k*20+17]->setIsEditable();
      m_IndexVideoProfile_AutoKeyFrame[k] = addMenuItem(m_pItemsSelect[k*20+17]);

      m_pItemsSlider[k*20+17] = new MenuItemSlider("Keyframe interval", "A keyframe is added every N miliseconds. Bigger keyframe values usually results in better quality video and lower bandwidth requirements but potential longer breakups if any. Setting it to 0 uses the keyframe interval set by the user in the main active video profile.", 50,20000,200, fSliderWidth);
      m_pItemsSlider[k*20+17]->setStep(10);
      m_IndexVideoProfile_KeyFrame[k] = addMenuItem(m_pItemsSlider[k*20+17]);

      int iMaxSize = p->iDebugMaxPacketSize-sizeof(t_packet_header)-sizeof(t_packet_header_video_segment);
      iMaxSize = (iMaxSize/10)*10;
      m_pItemsSlider[k*20+11] = new MenuItemSlider("Video Packet size", "How big is each indivisible video packet over the air link. No major impact in link quality. Smaller packets and more EC packets increase the chance of error correction but also increase the CPU usage.", 100,iMaxSize,iMaxSize/2, fSliderWidth);
      m_pItemsSlider[k*20+11]->setStep(10);
      m_IndexVideoProfile_PacketLength[k] = addMenuItem(m_pItemsSlider[k*20+11]);


      m_pItemsSelect[k*20+18] = new MenuItemSelect("Auto EC Scheme", "Enables or disables automatic error correction scheme adjustment.");  
      m_pItemsSelect[k*20+18]->addSelection("Manual");
      m_pItemsSelect[k*20+18]->addSelection("Auto");
      m_pItemsSelect[k*20+18]->setIsEditable();
      m_IndexVideoProfile_AutoECScheme[k] = addMenuItem(m_pItemsSelect[k*20+18]);

      int iMaxPackets = MAX_TOTAL_PACKETS_IN_BLOCK;
      if ( NULL != g_pCurrentModel )
         iMaxPackets = g_pCurrentModel->hwCapabilities.iMaxTxVideoBlockPackets;
      m_pItemsSlider[k*20+12] = new MenuItemSlider("Data Packets in a Block", "How many indivisible packets are in a block. This has an impact on link recovery and error correction. Bigger values might increase the delay in video stream when link is degraded, but also increase the chance of error correction.", 2, iMaxPackets/2, iMaxPackets/4, fSliderWidth);
      m_IndexVideoProfile_BlockPackets[k] = addMenuItem(m_pItemsSlider[k*20+12]);

      m_pItemsSlider[k*20+13] = new MenuItemSlider("EC Packets in a Block", "How many error correcting packets to add to a block.Bigger values increase the chance of error correction but decrease the usable link data rate buget.", 0, iMaxPackets/2, iMaxPackets/4, fSliderWidth);
      m_IndexVideoProfile_BlockFECs[k] = addMenuItem(m_pItemsSlider[k*20+13]);

      m_pItemsSlider[k*20+15] = new MenuItemSlider("Max Retransmission Window", "Maximum window size (in miliseconds) to send/retry and wait for retransmissions to happen.", 0,1000,10, fSliderWidth);
      m_pItemsSlider[k*20+15]->setStep(5);
      m_IndexVideoProfile_RetransmissionWindow[k] = addMenuItem(m_pItemsSlider[k*20+15]);

      m_pItemsSelect[k*20+14] = new MenuItemSelect("Transmissions Shuffle Duplication", "How much to shuffle and duplicate the video packets in bad conditions.");
      m_pItemsSelect[k*20+14]->addSelection("Auto");
      m_pItemsSelect[k*20+14]->addSelection("0%");
      m_pItemsSelect[k*20+14]->addSelection("10%");
      m_pItemsSelect[k*20+14]->addSelection("20%");
      m_pItemsSelect[k*20+14]->addSelection("30%");
      m_pItemsSelect[k*20+14]->addSelection("40%");
      m_pItemsSelect[k*20+14]->addSelection("50%");
      m_pItemsSelect[k*20+14]->addSelection("60%");
      m_pItemsSelect[k*20+14]->addSelection("70%");
      m_pItemsSelect[k*20+14]->addSelection("80%");
      m_pItemsSelect[k*20+14]->addSelection("90%");
      m_pItemsSelect[k*20+14]->addSelection("100%");
      m_pItemsSelect[k*20+14]->setIsEditable();
      m_IndexVideoProfileTransmissionDuplication[k] = addMenuItem(m_pItemsSelect[k*20+14]);

      m_IndexVideoProfile_Reset[k] = addMenuItem(new MenuItem("Reset Video Profile", "Resets all the profile settings to default values."));
   }

   addMenuItem(new MenuItemSection("Manual Test Video Link Profiles"));

   m_pItemsSelect[3] = new MenuItemSelect("Switch video profile button", "Use a QA button to switch video profiles.");
   m_pItemsSelect[3]->addSelection("None");
   m_pItemsSelect[3]->addSelection("QA Button 1");
   m_pItemsSelect[3]->addSelection("QA Button 2");
   m_pItemsSelect[3]->addSelection("QA Button 3");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexSwitchUsingQAButton = addMenuItem(m_pItemsSelect[3]);

   m_IndexSwitchToUserVideo = addMenuItem( new MenuItem("Switch to regular video link profile") );
   m_IndexSwitchToMQVideo = addMenuItem( new MenuItem("Switch to MQ video link profile") );
   m_IndexSwitchToLQVideo = addMenuItem( new MenuItem("Switch to LQ video link profile") );

   for( int i=0; i<m_ItemsCount; i++ )
      m_pMenuItems[i]->setTextColor(get_Color_Dev());

   log_line("MenuSystemVideoProfiles: Menu initialised.");
}

void MenuSystemVideoProfiles::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();

   if ( NULL == g_pCurrentModel )
      m_pItemsSlider[0]->setEnabled(false);
   else
   {
      int miliSec = ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & 0xFF00 ) >> 8;
      m_pItemsSlider[0]->setCurrentValue(miliSec*5);
      m_pItemsSlider[0]->setEnabled(true);
   }

   m_pItemsSlider[1]->setCurrentValue(pCS->iDisableRetransmissionsAfterControllerLinkLostMiliseconds/1000);
   m_pItemsSlider[5]->setCurrentValue(pCS->nRetryRetransmissionAfterTimeoutMS);
   m_pItemsSlider[6]->setCurrentValue(pCS->nRequestRetransmissionsOnVideoSilenceMs);

   if ( NULL != g_pCurrentModel )
   {
      m_pItemsSlider[7]->setCurrentValue(4*g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate/1000/1000);
      m_pItemsSlider[8]->setCurrentValue(g_pCurrentModel->video_params.uMaxAutoKeyframeIntervalMs);
      m_pItemsSlider[8]->setEnabled(true);
   }
   else
   {
      m_pItemsSlider[7]->setCurrentValue(4*DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE/1000/1000);
      m_pItemsSlider[8]->setCurrentValue(50);
      m_pItemsSlider[8]->setEnabled(false);
   }
   if ( NULL == g_pCurrentModel )
      m_pItemsSlider[2]->setEnabled(false);
   else
   {
      m_pItemsSlider[2]->setEnabled(true);
      m_pItemsSlider[2]->setCurrentValue(g_pCurrentModel->video_params.videoAdjustmentStrength);
   }

   m_pItemsSelect[3]->setSelectedIndex(pCS->iDevSwitchVideoProfileUsingQAButton+1);

   m_pMenuItems[m_IndexSwitchToUserVideo]->setEnabled(true);
   m_pMenuItems[m_IndexSwitchToMQVideo]->setEnabled(true);
   m_pMenuItems[m_IndexSwitchToLQVideo]->setEnabled(true);

   if ( NULL == g_pCurrentModel )
   {
      m_pItemsSelect[0]->setSelection(0);
      m_pItemsSelect[0]->setEnabled(false);

      for( int i=10; i<200; i++ )
      {
         if ( NULL != m_pItemsSelect[i] )
            m_pItemsSelect[i]->setEnabled(false);
         if ( NULL != m_pItemsSlider[i] )
            m_pItemsSlider[i]->setEnabled(false);
      }

      m_pMenuItems[m_IndexSwitchToUserVideo]->setEnabled(false);
      m_pMenuItems[m_IndexSwitchToMQVideo]->setEnabled(false);
      m_pMenuItems[m_IndexSwitchToLQVideo]->setEnabled(false);
      return;
   }
   m_pItemsSelect[0]->setEnabled(true);
   m_pItemsSelect[0]->setSelectedIndex(0);


   for( int i=10; i<200; i++ )
   {
      if ( NULL != m_pItemsSelect[i] )
         m_pItemsSelect[i]->setEnabled(true);
      if ( NULL != m_pItemsSlider[i] )
         m_pItemsSlider[i]->setEnabled(true);
   }

   if ( NULL == g_pCurrentModel )
   {
      for( int k=VIDEO_PROFILE_MQ; k<=VIDEO_PROFILE_LQ; k++ )
      {
         if ( NULL != m_pItemsSelect[k*20+11] )
            m_pItemsSelect[k*20+11]->setEnabled(false);
         if ( NULL != m_pItemsSelect[k*20+12] )
            m_pItemsSelect[k*20+12]->setEnabled(false);
         if ( NULL != m_pItemsSelect[k*20+17] )
            m_pItemsSelect[k*20+17]->setEnabled(false);
         if ( NULL != m_pItemsSelect[k*20+18] )
            m_pItemsSelect[k*20+18]->setEnabled(false);

         if ( NULL != m_pItemsSlider[k*20+10] )
            m_pItemsSlider[k*20+10]->setEnabled(false);
         if ( NULL != m_pItemsSlider[k*20+11] )
            m_pItemsSlider[k*20+11]->setEnabled(false);
         if ( NULL != m_pItemsSlider[k*20+12] )
            m_pItemsSlider[k*20+12]->setEnabled(false);
         if ( NULL != m_pItemsSlider[k*20+13] )
            m_pItemsSlider[k*20+13]->setEnabled(false);
         if ( NULL != m_pItemsSlider[k*20+14] )
            m_pItemsSlider[k*20+14]->setEnabled(false);
         if ( NULL != m_pItemsSlider[k*20+15] )
            m_pItemsSlider[k*20+15]->setEnabled(false);
         if ( NULL != m_pItemsSlider[k*20+16] )
            m_pItemsSlider[k*20+16]->setEnabled(false);
         if ( NULL != m_pItemsSlider[k*20+17] )
            m_pItemsSlider[k*20+17]->setEnabled(false);
      }
      return;
   }

   if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT ) != VIDEO_PROFILE_ENCODING_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO )
   {
       u32 percent = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT;
       percent = percent >> 16;
       percent = (percent >> 4) & 0x0F;
       if ( percent <= 10 )
          m_pItemsSelect[0]->setSelectedIndex(percent+1);
       else
          m_pItemsSelect[0]->setSelectedIndex(0);
   }
   else
       m_pItemsSelect[0]->setSelectedIndex(0);

   for( int k=VIDEO_PROFILE_MQ; k<=VIDEO_PROFILE_LQ; k++ )
   {
      int selectedIndex = 0;
      for( int i=0; i<getDataRatesCount(); i++ )
         if ( g_pCurrentModel->video_link_profiles[k].radio_datarate_video_bps == getDataRatesBPS()[i] )
            selectedIndex = i+1;
      for( int i=0; i<=MAX_MCS_INDEX; i++ )
         if ( g_pCurrentModel->video_link_profiles[k].radio_datarate_video_bps == -1-i )
            selectedIndex = i+getDataRatesCount()+1;

      m_pItemsSelect[k*20+11]->setSelection(selectedIndex);

      selectedIndex = 0;
      for( int i=0; i<getDataRatesCount(); i++ )
         if ( g_pCurrentModel->video_link_profiles[k].radio_datarate_data_bps == getDataRatesBPS()[i] )
            selectedIndex = i+1;
      for( int i=0; i<=MAX_MCS_INDEX; i++ )
         if ( g_pCurrentModel->video_link_profiles[k].radio_datarate_data_bps == -1-i )
            selectedIndex = i+getDataRatesCount()+1;

      m_pItemsSelect[k*20+12]->setSelection(selectedIndex);

      m_pItemsSlider[k*20+10]->setCurrentValue(4*g_pCurrentModel->video_link_profiles[k].bitrate_fixed_bps/1000/1000);
      m_pItemsSlider[k*20+11]->setCurrentValue(g_pCurrentModel->video_link_profiles[k].video_data_length);
      m_pItemsSlider[k*20+11]->setEnabled(false);
      m_pItemsSlider[k*20+12]->setCurrentValue(g_pCurrentModel->video_link_profiles[k].block_packets);
      m_pItemsSlider[k*20+13]->setCurrentValue(g_pCurrentModel->video_link_profiles[k].block_fecs);

      m_pItemsSlider[k*20+16]->setCurrentValue(g_pCurrentModel->video_link_profiles[k].fps);
      m_pItemsSlider[k*20+16]->setEnabled(true);
      
      m_pItemsSelect[k*20+17]->setEnabled(true);

      int keyframe_ms = g_pCurrentModel->video_link_profiles[k].keyframe_ms;

      if ( g_pCurrentModel->video_link_profiles[k].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
      {
         m_pItemsSelect[k*20+17]->setSelectedIndex(1);
         m_pItemsSlider[k*20+17]->setCurrentValue(keyframe_ms);
         m_pItemsSlider[k*20+17]->setEnabled(false);       
      }
      else
      {
         m_pItemsSelect[k*20+17]->setSelectedIndex(0);
         m_pItemsSlider[k*20+17]->setCurrentValue(keyframe_ms);
         m_pItemsSlider[k*20+17]->setEnabled(true);
      }

      m_pItemsSelect[k*20+18]->setEnabled(true);
      m_pItemsSelect[k*20+18]->setSelectedIndex((g_pCurrentModel->video_link_profiles[k].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME)?1:0);
      if ( g_pCurrentModel->video_link_profiles[k].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME )
      {
         if ( NULL != m_pItemsSlider[k*20+12] )
            m_pItemsSlider[k*20+12]->setEnabled(false);
         if ( NULL != m_pItemsSlider[k*20+13] )
            m_pItemsSlider[k*20+13]->setEnabled(false);
      }

      int miliSec = ( g_pCurrentModel->video_link_profiles[k].uProfileEncodingFlags & 0xFF00 ) >> 8;
      m_pItemsSlider[k*20+15]->setCurrentValue(miliSec*5);
      m_pItemsSlider[k*20+15]->setEnabled(true);

      if ( (g_pCurrentModel->video_link_profiles[k].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT ) != VIDEO_PROFILE_ENCODING_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO )
      {
         u32 percent = g_pCurrentModel->video_link_profiles[k].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT;
         percent = percent >> 16;
         percent = percent & 0x0F;
         if ( percent <= 10 )
            m_pItemsSelect[k*20+14]->setSelectedIndex(percent+1);
         else
            m_pItemsSelect[k*20+14]->setSelectedIndex(0);
      }
      else
          m_pItemsSelect[k*20+14]->setSelectedIndex(0);
   }
}

void MenuSystemVideoProfiles::onShow()
{
   Menu::onShow();
}

void MenuSystemVideoProfiles::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuSystemVideoProfiles::sendVideoLinkProfiles()
{
   type_video_link_profile profiles[MAX_VIDEO_LINK_PROFILES];
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      memcpy(&profiles[i], &(g_pCurrentModel->video_link_profiles[i]), sizeof(type_video_link_profile));

   u32 miliSec = m_pItemsSlider[0]->getCurrentValue()/5;
   miliSec = (miliSec & 0xFF) << 8;
   profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags &= 0xFFFF00FF;
   profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags |= miliSec;

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      type_video_link_profile* pProfile = &(profiles[i]);

      int iRetrDuplication = m_pItemsSelect[0]->getSelectedIndex();
      int iDuplication = -1;
      if ( i == VIDEO_PROFILE_MQ || i == VIDEO_PROFILE_LQ )
         iDuplication = m_pItemsSelect[i*20+14]->getSelectedIndex();

      pProfile->uProfileEncodingFlags &= (~VIDEO_PROFILE_ENCODING_FLAG_MASK_RETRANSMISSIONS_DUPLICATION_PERCENT);
      u32 value = 0;
      if ( iDuplication >= 1 )
         value = value | ((u32)(iDuplication-1));
      else
         value = value | 0x0F;

      if ( iRetrDuplication != 0 )
         value = value | (((u32)(iRetrDuplication-1))<<4);
      else
         value = value | 0xF0;

      pProfile->uProfileEncodingFlags |= (value<<16);
   }

   // Propagate changes to lower video profile

   propagate_video_profile_changes(&g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile], &(profiles[g_pCurrentModel->video_params.user_selected_video_link_profile]), &(profiles[0]));

   for( int k=VIDEO_PROFILE_MQ; k<=VIDEO_PROFILE_LQ; k++ )
   {
      int index = m_pItemsSelect[k*20+11]->getSelectedIndex();
      if ( index == 0 )
         profiles[k].radio_datarate_video_bps = 0;
      else
      {
         index--;
         if ( index < getDataRatesCount() )
            profiles[k].radio_datarate_video_bps = getDataRatesBPS()[index];
         else
            profiles[k].radio_datarate_video_bps = -1-(index - getDataRatesCount());
      }

      index = m_pItemsSelect[k*20+12]->getSelectedIndex();
      if ( index == 0 )
         profiles[k].radio_datarate_data_bps = 0;
      else
      {
         index--;
         if ( index < getDataRatesCount() )
            profiles[k].radio_datarate_data_bps = getDataRatesBPS()[index];
         else
            profiles[k].radio_datarate_data_bps = -1-(index - getDataRatesCount());
      }

      miliSec = m_pItemsSlider[k*20+15]->getCurrentValue()/5;
      miliSec = (miliSec & 0xFF) << 8;
      profiles[k].uProfileEncodingFlags &= 0xFFFF00FF;
      profiles[k].uProfileEncodingFlags |= miliSec;

      log_line("Set video profile %d radio data rate to (video/data): %d/%d", k, profiles[k].radio_datarate_video_bps, profiles[k].radio_datarate_data_bps);

      profiles[k].fps = m_pItemsSlider[k*20+16]->getCurrentValue();

      if ( profiles[k].fps == 1 )
         profiles[k].fps = 2;
      
      profiles[k].keyframe_ms = m_pItemsSlider[k*20+17]->getCurrentValue();
      if ( 0 == m_pItemsSelect[k*20+17]->getSelectedIndex() )
         profiles[k].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
      else
         profiles[k].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
      
      if ( 0 == m_pItemsSelect[k*20+18]->getSelectedIndex() )
         profiles[k].uProfileEncodingFlags &= (~VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME);
      else
         profiles[k].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME;
      profiles[k].video_data_length = m_pItemsSlider[k*20+11]->getCurrentValue();
      profiles[k].block_packets = m_pItemsSlider[k*20+12]->getCurrentValue();
      profiles[k].block_fecs    = m_pItemsSlider[k*20+13]->getCurrentValue();
      profiles[k].bitrate_fixed_bps = m_pItemsSlider[k*20+10]->getCurrentValue()*1000*1000/4;
   }

   log_line("Sending video encoding flags for user selected profile: %s", str_format_video_encoding_flags(profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags));
   log_line("Sending video encoding flags for MQ video profile: %s", str_format_video_encoding_flags(profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags));
   log_line("Sending video encoding flags for LQ video profile: %s", str_format_video_encoding_flags(profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags));

   u8 buffer[1024];
   memcpy( buffer, &(profiles[0]), MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile) );

   log_line("Sending new video link profiles to vehicle.");
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES, 0, buffer, MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile)) )
      valuesToUI();
   else
      send_control_message_to_router(PACEKT_TYPE_LOCAL_CONTROLLER_ADAPTIVE_VIDEO_PAUSE, 7000);
}


void MenuSystemVideoProfiles::onSelectItem()
{
   Menu::onSelectItem();
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   ControllerSettings* pCS = get_ControllerSettings();

   
   if ( m_IndexRetransmitWindow == m_SelectedIndex  )
      sendVideoLinkProfiles();

   if ( m_IndexRetransmissionDuplication == m_SelectedIndex )
      sendVideoLinkProfiles();

   if ( m_IndexDisableRetransmissionsTimeout == m_SelectedIndex )
   {
      pCS->iDisableRetransmissionsAfterControllerLinkLostMiliseconds = m_pItemsSlider[1]->getCurrentValue()*1000;
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }

   if ( m_IndexRetransmitRetryTimeout == m_SelectedIndex )
   {
      pCS->nRetryRetransmissionAfterTimeoutMS = m_pItemsSlider[5]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }

   if ( m_IndexRetransmitRequestOnTimeout == m_SelectedIndex )
   {
      pCS->nRequestRetransmissionsOnVideoSilenceMs = m_pItemsSlider[6]->getCurrentValue();
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }

   if ( m_IndexDefaultAutoKeyframe == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessageNeedsVehcile("You can't adjust the parameter when not connected to a vehicle.", 5);
         valuesToUI();
         return;
      }
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.uMaxAutoKeyframeIntervalMs = m_pItemsSlider[8]->getCurrentValue();

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( m_IndexLowestAllowedVideoBitrate == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessageNeedsVehcile("You can't adjust the minimum allowed video bitrate when not connected to a vehicle.", 5);
         valuesToUI();
         return;
      }
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.lowestAllowedAdaptiveVideoBitrate = m_pItemsSlider[7]->getCurrentValue()*1000*1000/4;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
      return;
   }

   if ( m_IndexVideoAdjustStrength == m_SelectedIndex )
   {
      video_parameters_t paramsNew;
      memcpy(&paramsNew, &g_pCurrentModel->video_params, sizeof(video_parameters_t));
      paramsNew.videoAdjustmentStrength = m_pItemsSlider[2]->getCurrentValue();

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VIDEO_PARAMS, 0, (u8*)&paramsNew, sizeof(video_parameters_t)) )
         valuesToUI();
   }

   if ( m_IndexSwitchUsingQAButton == m_SelectedIndex )
   {
      pCS->iDevSwitchVideoProfileUsingQAButton = m_pItemsSelect[3]->getSelectedIndex()-1;
      save_ControllerSettings();
      valuesToUI();
      return;
   }
   
   if ( (m_IndexSwitchToUserVideo == m_SelectedIndex) || (m_IndexSwitchToMQVideo == m_SelectedIndex) || (m_IndexSwitchToLQVideo == m_SelectedIndex) )
   {
      if ( NULL == g_pCurrentModel )
         return;

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_FORCE_VIDEO_PROFILE, STREAM_ID_DATA);
      PH.vehicle_id_src = PACKET_COMPONENT_RUBY;

      if ( m_IndexSwitchToUserVideo == m_SelectedIndex )
      {
         PH.vehicle_id_dest = 0xFF;
         warnings_add(0, "Dev: Switch to Auto Video Link Quality.");
      }
      if ( m_IndexSwitchToMQVideo == m_SelectedIndex )
      {
         PH.vehicle_id_dest = VIDEO_PROFILE_MQ;
         warnings_add(0, "Dev: Switch to Med Video Link Quality.");
      }
      if ( m_IndexSwitchToLQVideo == m_SelectedIndex )
      {
         PH.vehicle_id_dest = VIDEO_PROFILE_LQ;
         warnings_add(0, "Dev: Switch to Low Video Link Quality.");
      }
      PH.total_length = sizeof(t_packet_header);

      u8 buffer[1024];
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      send_packet_to_router(buffer, PH.total_length);
      return;
   }


   for( int k=VIDEO_PROFILE_MQ; k<=VIDEO_PROFILE_LQ; k++ )
   {
      if ( m_IndexVideoProfile_VideoRadioRate[k] == m_SelectedIndex )
         sendVideoLinkProfiles();
      if ( m_IndexVideoProfile_DataRadioRate[k] == m_SelectedIndex )
         sendVideoLinkProfiles();
      if ( m_IndexVideoProfile_Bitrate[k] == m_SelectedIndex )
         sendVideoLinkProfiles();

      if ( m_IndexVideoProfile_KeyFrame[k] == m_SelectedIndex || m_IndexVideoProfile_AutoKeyFrame[k] == m_SelectedIndex )
         sendVideoLinkProfiles();
      if ( m_IndexVideoProfile_FPS[k] == m_SelectedIndex )
         sendVideoLinkProfiles();

      if ( m_IndexVideoProfile_PacketLength[k] == m_SelectedIndex )
         sendVideoLinkProfiles();
      if ( m_IndexVideoProfile_AutoECScheme[k] == m_SelectedIndex )
         sendVideoLinkProfiles();
      if ( m_IndexVideoProfile_BlockPackets[k] == m_SelectedIndex )
         sendVideoLinkProfiles();
      if ( m_IndexVideoProfile_BlockFECs[k] == m_SelectedIndex )
         sendVideoLinkProfiles();
      if ( m_IndexVideoProfileTransmissionDuplication[k] == m_SelectedIndex )
         sendVideoLinkProfiles();
      if ( m_IndexVideoProfile_RetransmissionWindow[k] == m_SelectedIndex )
         sendVideoLinkProfiles();

      if ( m_IndexVideoProfile_Reset[k] == m_SelectedIndex )
      {
         g_pCurrentModel->resetVideoLinkProfiles(k);
         valuesToUI();
         sendVideoLinkProfiles();
      }
   }
}

