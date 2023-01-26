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
#include "../base/models.h"
#include "../base/config.h"
#include "../base/launchers.h"
#include "../base/commands.h"
#include "../base/ctrl_settings.h"
#include "../common/string_utils.h"
#include "menu.h"
#include "menu_vehicle_osd_stats.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"

#include "colors.h"
#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include "process_router_messages.h"
#include "osd_common.h"

MenuVehicleOSDStats::MenuVehicleOSDStats(void)
:Menu(MENU_ID_VEHICLE_OSD, "OSD Statistics Panels", NULL)
{
   ControllerSettings* pCS = get_ControllerSettings();

   m_Width = 0.33;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.22;

   char szBuff[256];
   sprintf(szBuff, "OSD Statistics Panels (%s)", str_get_osd_screen_name(g_pCurrentModel->osd_params.layout));
   setTitle(szBuff);
   
   m_IndexDevStatsVideo = -1;
   m_IndexDevVehicleVideoBitrateHistory = -1;
   m_IndexDevStatsRadio = -1;
   m_IndexDevFullRXStats = -1;
   m_IndexShowControllerAdaptiveInfoStats = -1;
   m_IndexDevStatsVehicleTx = -1;
   m_IndexDevStatsVehicleVideo = -1;
   m_IndexSnapshot = -1;

   m_pItemsSelect[0] = new MenuItemSelect("Font Size", "Increase/decrease OSD font size for stats windows for current OSD screen.");  
   m_pItemsSelect[0]->addSelection("Smallest");
   m_pItemsSelect[0]->addSelection("Smaller");
   m_pItemsSelect[0]->addSelection("Small");
   m_pItemsSelect[0]->addSelection("Normal");
   m_pItemsSelect[0]->addSelection("Large");
   m_pItemsSelect[0]->addSelection("Larger");
   m_pItemsSelect[0]->addSelection("Largest");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexFontSize = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[21] = new MenuItemSelect("Transparency", "Change how transparent the statistics background is.");  
   m_pItemsSelect[21]->addSelection("Max");
   m_pItemsSelect[21]->addSelection("Medium");
   m_pItemsSelect[21]->addSelection("Normal");
   m_pItemsSelect[21]->addSelection("Minimum");
   m_pItemsSelect[21]->setIsEditable();
   m_IndexTransparency = addMenuItem(m_pItemsSelect[21]);


   m_pItemsSelect[7] = new MenuItemSelect("Stats panels layout", "Changes the layout of the statistics panels.");  
   m_pItemsSelect[7]->addSelection("Auto (Left)");
   m_pItemsSelect[7]->addSelection("Auto (Right)");
   m_pItemsSelect[7]->addSelection("Auto (Top)");
   m_pItemsSelect[7]->addSelection("Auto (Bottom)");
   m_pItemsSelect[7]->setIsEditable();
   m_IndexPanelsDirection = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[1] = new MenuItemSelect("Show Radio Links stats", "Show statistics about the radio links health.");  
   m_pItemsSelect[1]->addSelection("Off");
   m_pItemsSelect[1]->addSelection("On");
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexStatsRadioLinks = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("Show Radio Interfaces stats", "Show statistics about the radio interfaces present on the controller and how they perform.");  
   m_pItemsSelect[2]->addSelection("Off");
   m_pItemsSelect[2]->addSelection("On");
   m_pItemsSelect[2]->setUseMultiViewLayout();
   m_IndexStatsRadioInterfaces = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[17] = new MenuItemSelect("    Displayed Radio Cards", "Choose what radio cards statistics to see: from controller radio cards or vehicle radio cards or both.");  
   m_pItemsSelect[17]->addSelection("Controller Only");
   m_pItemsSelect[17]->addSelection("Controller & Vehicle");
   m_pItemsSelect[17]->setIsEditable();
   m_IndexVehicleRadioRxStats = addMenuItem(m_pItemsSelect[17]);


   m_pItemsSelect[8] = new MenuItemSelect("    Graphs Resolution", "The resolution of the graphs, in miliseconds / bar.");  
   m_pItemsSelect[8]->addSelection("500 ms/bar");
   m_pItemsSelect[8]->addSelection("200 ms/bar");
   m_pItemsSelect[8]->addSelection("100 ms/bar");
   m_pItemsSelect[8]->addSelection("50 ms/bar");
   m_pItemsSelect[8]->addSelection("20 ms/bar");
   m_pItemsSelect[8]->addSelection("10 ms/bar");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexRadioRefreshInterval = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[18] = new MenuItemSelect("    Displayed Style", "Choose how to display the radio interfaces.");  
   m_pItemsSelect[18]->addSelection("Normal");
   m_pItemsSelect[18]->addSelection("Compact");
   m_pItemsSelect[18]->setIsEditable();
   m_IndexRadioInterfacesCompact = addMenuItem(m_pItemsSelect[18]);

   m_pItemsSelect[3] = new MenuItemSelect("Show video decoding stats", "Show statistics about the video decoding process and quality.");  
   m_pItemsSelect[3]->addSelection("Off");
   m_pItemsSelect[3]->addSelection("On");
   m_pItemsSelect[3]->setUseMultiViewLayout();
   m_IndexStatsDecode = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[4] = new MenuItemSelect("    Display Style", "Selects the style and how much information to show in the video decoding stats.");  
   m_pItemsSelect[4]->addSelection("Compact");
   m_pItemsSelect[4]->addSelection("Normal");
   m_pItemsSelect[4]->addSelection("Extended");
   m_pItemsSelect[4]->setIsEditable();
   m_IndexStatsVideoExtended = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[9] = new MenuItemSelect("    Graphs Resolution", "The resolution of the graphs, in miliseconds / bar.");  
   m_pItemsSelect[9]->addSelection("500 ms/bar");
   m_pItemsSelect[9]->addSelection("200 ms/bar");
   m_pItemsSelect[9]->addSelection("100 ms/bar");
   m_pItemsSelect[9]->addSelection("50 ms/bar");
   m_pItemsSelect[9]->addSelection("20 ms/bar");
   m_pItemsSelect[9]->addSelection("10 ms/bar");
   m_pItemsSelect[9]->setIsEditable();
   m_IndexVideoRefreshInterval = addMenuItem(m_pItemsSelect[9]);

   m_pItemsSelect[16] = new MenuItemSelect("    Adaptive video graph", "Shows a live graph of adaptive video link changes.");  
   m_pItemsSelect[16]->addSelection("Off");
   m_pItemsSelect[16]->addSelection("On");
   m_pItemsSelect[16]->setUseMultiViewLayout();
   m_IndexStatsAdaptiveVideoGraph = addMenuItem(m_pItemsSelect[16]);

   if ( pCS->iDeveloperMode )
   {
      m_pItemsSelect[13] = new MenuItemSelect("    Show snapshot on retransmissions", "Shows a snapshot of buffers and state when a video block is discarded.");  
      m_pItemsSelect[13]->addSelection("Off");
      m_pItemsSelect[13]->addSelection("On");
      m_pItemsSelect[13]->setTextColor(get_Color_Dev());
      m_IndexSnapshot = addMenuItem(m_pItemsSelect[13]);

      m_pItemsSelect[22] = new MenuItemSelect("    Snapshot closes on", "Select how the snapshot window is closed.");  
      m_pItemsSelect[22]->addSelection("Back Key");
      m_pItemsSelect[22]->addSelection("Timeout Period");
      m_pItemsSelect[22]->addSelection("Next Snapshot");
      m_pItemsSelect[22]->setTextColor(get_Color_Dev());
      m_pItemsSelect[22]->setIsEditable();
      m_IndexSnapshotTimeout = addMenuItem(m_pItemsSelect[22]);
   }
   else
   {
      m_IndexSnapshot = -1;
      m_IndexSnapshotTimeout = -1;
   }

   m_pItemsSelect[19] = new MenuItemSelect("Show Video Bitrate History", "Shows history graph on video bitrate and radio datarates.");
   m_pItemsSelect[19]->addSelection("Off");
   m_pItemsSelect[19]->addSelection("On");
   m_pItemsSelect[19]->setUseMultiViewLayout();
   m_IndexDevVehicleVideoBitrateHistory = addMenuItem(m_pItemsSelect[19]);

   m_pItemsSelect[23] = new MenuItemSelect("Show Video Stream Stats", "Show statistics about the video stream.");
   m_pItemsSelect[23]->addSelection("Off");
   m_pItemsSelect[23]->addSelection("Compact");
   m_pItemsSelect[23]->addSelection("Full");
   m_pItemsSelect[23]->setIsEditable();
   m_IndexStatsVideoStreamInfo = addMenuItem(m_pItemsSelect[23]);
   
   m_pItemsSelect[5] = new MenuItemSelect("Show efficiency stats", "Show statistics about power usage efficiency.");  
   m_pItemsSelect[5]->addSelection("Off");
   m_pItemsSelect[5]->addSelection("On");
   m_pItemsSelect[5]->setUseMultiViewLayout();
   m_IndexStatsEff = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[6] = new MenuItemSelect("Show vehicle RC stats", "Show statistics received from vehicle about the RC link status and health.");  
   m_pItemsSelect[6]->addSelection("Off");
   m_pItemsSelect[6]->addSelection("On");
   m_pItemsSelect[6]->setUseMultiViewLayout();
   m_IndexStatsRC = addMenuItem(m_pItemsSelect[6]);
   #ifndef FEATURE_ENABLE_RC
   m_pMenuItems[m_IndexStatsRC]->setEnabled(false);
   #endif

   if ( pCS->iDeveloperMode )
   {
      m_pItemsSelect[10] = new MenuItemSelect("Show Retransmissions Stats", "Shows the extended developer video retransmissions stats.");
      m_pItemsSelect[10]->addSelection("Off");
      m_pItemsSelect[10]->addSelection("On");
      m_pItemsSelect[10]->setUseMultiViewLayout();
      m_pItemsSelect[10]->setTextColor(get_Color_Dev());
      m_IndexDevStatsVideo = addMenuItem(m_pItemsSelect[10]);

      m_pItemsSelect[11] = new MenuItemSelect("Show Extended Radio Stats", "Shows the extended developer radio stats.");
      m_pItemsSelect[11]->addSelection("Off");
      m_pItemsSelect[11]->addSelection("On");
      m_pItemsSelect[11]->setUseMultiViewLayout();
      m_pItemsSelect[11]->setTextColor(get_Color_Dev());
      m_IndexDevStatsRadio = addMenuItem(m_pItemsSelect[11]);

      m_pItemsSelect[12] = new MenuItemSelect("Show Full Radio RX Stats", "Shows full statistics about radio stack state.");
      m_pItemsSelect[12]->addSelection("Off");
      m_pItemsSelect[12]->addSelection("On");
      m_pItemsSelect[12]->setUseMultiViewLayout();
      m_pItemsSelect[12]->setTextColor(get_Color_Dev());
      m_IndexDevFullRXStats = addMenuItem(m_pItemsSelect[12]);

      m_pItemsSelect[24] = new MenuItemSelect("Show Controller Adaptive Video Info Stats", "");
      m_pItemsSelect[24]->addSelection("Off");
      m_pItemsSelect[24]->addSelection("On");
      m_pItemsSelect[24]->setUseMultiViewLayout();
      m_pItemsSelect[24]->setTextColor(get_Color_Dev());
      m_IndexShowControllerAdaptiveInfoStats = addMenuItem(m_pItemsSelect[24]);

      m_pItemsSelect[20] = new MenuItemSelect("Show Vehicle Tx Stats", "Shows graphs about vehicle transmissions.");
      m_pItemsSelect[20]->addSelection("Off");
      m_pItemsSelect[20]->addSelection("On");
      m_pItemsSelect[20]->setUseMultiViewLayout();
      m_pItemsSelect[20]->setTextColor(get_Color_Dev());
      m_IndexDevStatsVehicleTx = addMenuItem(m_pItemsSelect[20]);

      m_pItemsSelect[14] = new MenuItemSelect("Show Vehicle's Video Link Stats", "Shows statistics (generated on the vehicle side) about the vehicle video link state params.");
      m_pItemsSelect[14]->addSelection("Off");
      m_pItemsSelect[14]->addSelection("On");
      m_pItemsSelect[14]->setUseMultiViewLayout();
      m_pItemsSelect[14]->setTextColor(get_Color_Dev());
      m_IndexDevStatsVehicleVideo = addMenuItem(m_pItemsSelect[14]);

      m_pItemsSelect[15] = new MenuItemSelect("Show Vehicle's Video Link Stats Graphs", "Shows graphs (generated on the vehicle side) about the vehicle video link state params.");
      m_pItemsSelect[15]->addSelection("Off");
      m_pItemsSelect[15]->addSelection("On");
      m_pItemsSelect[15]->setUseMultiViewLayout();
      m_pItemsSelect[15]->setTextColor(get_Color_Dev());
      m_IndexDevStatsVehicleVideoGraphs = addMenuItem(m_pItemsSelect[15]);
   }
}

MenuVehicleOSDStats::~MenuVehicleOSDStats()
{
}

void MenuVehicleOSDStats::valuesToUI()
{
   int layoutIndex = g_pCurrentModel->osd_params.layout;
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pP = get_Preferences();

   m_pItemsSelect[0]->setSelectedIndex(((g_pCurrentModel->osd_params.osd_preferences[layoutIndex])>>16) & 0x0F);
   m_pItemsSelect[21]->setSelectedIndex(((g_pCurrentModel->osd_params.osd_preferences[layoutIndex])>>20) & 0x0F);
   m_pItemsSelect[1]->setSelection((g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG_EXT_SHOW_STATS_RADIO_LINKS)?1:0);
   m_pItemsSelect[2]->setSelection((g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG_EXT_SHOW_STATS_RADIO_INTERFACES)?1:0);

   if ( pCS->nGraphRadioRefreshInterval > 200 )
      m_pItemsSelect[8]->setSelection(0);
   else if ( pCS->nGraphRadioRefreshInterval > 100 )
      m_pItemsSelect[8]->setSelection(1);
   else if ( pCS->nGraphRadioRefreshInterval > 50 )
      m_pItemsSelect[8]->setSelection(2);
   else if ( pCS->nGraphRadioRefreshInterval > 20 )
      m_pItemsSelect[8]->setSelection(3);
   else if ( pCS->nGraphRadioRefreshInterval > 10 )
      m_pItemsSelect[8]->setSelection(4);
   else
      m_pItemsSelect[8]->setSelection(5);

   if ( pCS->nGraphVideoRefreshInterval > 200 )
      m_pItemsSelect[9]->setSelection(0);
   else if ( pCS->nGraphVideoRefreshInterval > 100 )
      m_pItemsSelect[9]->setSelection(1);
   else if ( pCS->nGraphVideoRefreshInterval > 50 )
      m_pItemsSelect[9]->setSelection(2);
   else if ( pCS->nGraphVideoRefreshInterval > 20 )
      m_pItemsSelect[9]->setSelection(3);
   else if ( pCS->nGraphVideoRefreshInterval > 10 )
      m_pItemsSelect[9]->setSelection(4);
   else
      m_pItemsSelect[9]->setSelection(5);

   if ( g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG_EXT_SHOW_STATS_RADIO_INTERFACES )
   {
      m_pItemsSelect[8]->setEnabled(true);
      m_pItemsSelect[17]->setEnabled(true);
      m_pItemsSelect[18]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[8]->setEnabled(false);
      m_pItemsSelect[17]->setEnabled(false);
      m_pItemsSelect[18]->setEnabled(false);
   }
   m_pItemsSelect[17]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG_EXT_SHOW_VEHICLE_RADIO_INTERFACES_STATS)?1:0);
   m_pItemsSelect[18]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG_EXT_SHOW_RADIO_INTERFACES_COMPACT)?1:0);

   if ( g_pCurrentModel->osd_params.osd_flags[layoutIndex] & OSD_FLAG_SHOW_STATS_VIDEO_INFO )
   {
      if ( pCS->iShowVideoStreamInfoCompact )
         m_pItemsSelect[23]->setSelectedIndex(1);
      else
         m_pItemsSelect[23]->setSelectedIndex(2);
   }
   else
      m_pItemsSelect[23]->setSelectedIndex(0);

   if ( g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG_EXT_SHOW_STATS_VIDEO )
   {
      m_pItemsSelect[4]->setEnabled(true);
      m_pItemsSelect[9]->setEnabled(true);
      m_pItemsSelect[16]->setEnabled(true);
      if ( pCS->iDeveloperMode )
      {
         if ( -1 != m_IndexSnapshot )
            m_pItemsSelect[13]->setEnabled(true);
         if ( -1 != m_IndexSnapshotTimeout )
            m_pItemsSelect[22]->setEnabled(true);
      }
   }
   else
   {
      m_pItemsSelect[4]->setEnabled(false);
      m_pItemsSelect[9]->setEnabled(false);
      m_pItemsSelect[16]->setEnabled(false);
      //if ( pCS->iDeveloperMode )
      {
         if ( -1 != m_IndexSnapshot )
            m_pItemsSelect[13]->setEnabled(false);
         if ( -1 != m_IndexSnapshotTimeout )
            m_pItemsSelect[22]->setEnabled(false);
      }
   }

   m_pItemsSelect[3]->setSelection((g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG_EXT_SHOW_STATS_VIDEO)?1:0);
   m_pItemsSelect[4]->setSelectedIndex(1);
   if ( g_pCurrentModel->osd_params.osd_flags[layoutIndex] & OSD_FLAG_EXTENDED_VIDEO_DECODE_HISTORY )
      m_pItemsSelect[4]->setSelectedIndex(2);
   if ( g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS )
      m_pItemsSelect[4]->setSelectedIndex(0);

   m_pItemsSelect[16]->setSelection((g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG_EXT_SHOW_ADAPTIVE_VIDEO_GRAPH)?1:0);

   m_pItemsSelect[5]->setSelection((g_pCurrentModel->osd_params.osd_flags[layoutIndex] & OSD_FLAG_SHOW_EFFICIENCY_STATS)?1:0);
   m_pItemsSelect[6]->setSelection((g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG_EXT_SHOW_STATS_RC)?1:0);

   m_pItemsSelect[19]->setSelectedIndex(0);
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_VIDEO_BITRATE_HISTORY )
      m_pItemsSelect[19]->setSelectedIndex(1);


   if ( pCS->iDeveloperMode )
   {
      m_pItemsSelect[20]->setSelectedIndex(0);
      if ( NULL != g_pCurrentModel )
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP )
         m_pItemsSelect[20]->setSelectedIndex(1);

      m_pItemsSelect[10]->setSelectedIndex(pP->iDebugShowDevVideoStats);
      m_pItemsSelect[11]->setSelectedIndex(pP->iDebugShowDevRadioStats);
      m_pItemsSelect[12]->setSelectedIndex(pP->iDebugShowFullRXStats);
      if ( -1 != m_IndexSnapshot )
         m_pItemsSelect[13]->setSelectedIndex(pP->iDebugShowVideoSnapshotOnDiscard);

      if ( -1 != m_IndexSnapshotTimeout )
      {
         m_pItemsSelect[22]->setSelectedIndex(pCS->iVideoDecodeStatsSnapshotClosesOnTimeout);
         if ( pP->iDebugShowVideoSnapshotOnDiscard )
            m_pItemsSelect[22]->setEnabled(true);
         else
            m_pItemsSelect[22]->setEnabled(false);
      }
      m_pItemsSelect[14]->setSelectedIndex(pP->iDebugShowVehicleVideoStats);
      m_pItemsSelect[15]->setSelectedIndex(pP->iDebugShowVehicleVideoGraphs);

      m_pItemsSelect[24]->setSelectedIndex( (g_pCurrentModel->osd_params.osd_flags3[layoutIndex] & OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO)?1:0);
   }

   if ( g_pCurrentModel->osd_params.osd_preferences[layoutIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT )
      m_pItemsSelect[7]->setSelectedIndex(0);
   else if ( g_pCurrentModel->osd_params.osd_preferences[layoutIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT )
      m_pItemsSelect[7]->setSelectedIndex(1);
   else if ( g_pCurrentModel->osd_params.osd_preferences[layoutIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_TOP )
      m_pItemsSelect[7]->setSelectedIndex(2);
   else if ( g_pCurrentModel->osd_params.osd_preferences[layoutIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM )
      m_pItemsSelect[7]->setSelectedIndex(3);
   else   
      m_pItemsSelect[7]->setSelectedIndex(0);
}

void MenuVehicleOSDStats::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleOSDStats::onSelectItem()
{
   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pP = get_Preferences();
   bool sendToVehicle = false;
   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   int layoutIndex = g_pCurrentModel->osd_params.layout;

   if ( m_IndexFontSize == m_SelectedIndex )
   {
      params.osd_preferences[layoutIndex] &= ~(0x0F0000);
      params.osd_preferences[layoutIndex] |= ((u32)m_pItemsSelect[0]->getSelectedIndex())<<16;
      sendToVehicle = true;

      u32 scale = (params.osd_preferences[layoutIndex]>>16) & 0x0F;

      osd_setScaleOSDStats((int)scale);

      if ( render_engine_is_raw() )
         ruby_reload_osd_fonts();
   }

   if ( m_IndexTransparency == m_SelectedIndex )
   {
      params.osd_preferences[layoutIndex] &= ~(0xF00000);
      params.osd_preferences[layoutIndex] |= ((u32)m_pItemsSelect[21]->getSelectedIndex())<<20;
      sendToVehicle = true;
   }


   if ( m_IndexPanelsDirection == m_SelectedIndex )
   {
      params.osd_preferences[layoutIndex] &= ~OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT;
      params.osd_preferences[layoutIndex] &= ~OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT;
      params.osd_preferences[layoutIndex] &= ~OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_TOP;
      params.osd_preferences[layoutIndex] &= ~OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM;
      if ( 0 == m_pItemsSelect[7]->getSelectedIndex() )
         params.osd_preferences[layoutIndex] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT;
      else if ( 1 == m_pItemsSelect[7]->getSelectedIndex() )
         params.osd_preferences[layoutIndex] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT;
      else if ( 2 == m_pItemsSelect[7]->getSelectedIndex() )
         params.osd_preferences[layoutIndex] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_TOP;
      else if ( 3 == m_pItemsSelect[7]->getSelectedIndex() )
         params.osd_preferences[layoutIndex] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM;
      sendToVehicle = true;
   }


   if ( m_IndexStatsRadioLinks == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG_EXT_SHOW_STATS_RADIO_LINKS;
      else
         params.osd_flags2[layoutIndex] |= OSD_FLAG_EXT_SHOW_STATS_RADIO_LINKS;

      sendToVehicle = true;
   }

   if ( m_IndexStatsRadioInterfaces == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG_EXT_SHOW_STATS_RADIO_INTERFACES;
      else
         params.osd_flags2[layoutIndex] |= OSD_FLAG_EXT_SHOW_STATS_RADIO_INTERFACES;

      sendToVehicle = true;
   }

   if ( m_IndexVehicleRadioRxStats == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[17]->getSelectedIndex() )
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG_EXT_SHOW_VEHICLE_RADIO_INTERFACES_STATS;
      else
         params.osd_flags2[layoutIndex] |= OSD_FLAG_EXT_SHOW_VEHICLE_RADIO_INTERFACES_STATS;

      sendToVehicle = true;    
   }

   if ( m_IndexRadioInterfacesCompact == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[18]->getSelectedIndex() )
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG_EXT_SHOW_RADIO_INTERFACES_COMPACT;
      else
         params.osd_flags2[layoutIndex] |= OSD_FLAG_EXT_SHOW_RADIO_INTERFACES_COMPACT;

      sendToVehicle = true;    
   }

   if ( m_IndexRadioRefreshInterval == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[8]->getSelectedIndex() )
         pCS->nGraphRadioRefreshInterval = 500;
      if ( 1 == m_pItemsSelect[8]->getSelectedIndex() )
         pCS->nGraphRadioRefreshInterval = 200;
      if ( 2 == m_pItemsSelect[8]->getSelectedIndex() )
         pCS->nGraphRadioRefreshInterval = 100;
      if ( 3 == m_pItemsSelect[8]->getSelectedIndex() )
         pCS->nGraphRadioRefreshInterval = 50;
      if ( 4 == m_pItemsSelect[8]->getSelectedIndex() )
         pCS->nGraphRadioRefreshInterval = 20;
      if ( 5 == m_pItemsSelect[8]->getSelectedIndex() )
         pCS->nGraphRadioRefreshInterval = 10;
      save_ControllerSettings();
      invalidate();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }

   if ( m_IndexVideoRefreshInterval == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[9]->getSelectedIndex() )
         pCS->nGraphVideoRefreshInterval = 500;
      if ( 1 == m_pItemsSelect[9]->getSelectedIndex() )
         pCS->nGraphVideoRefreshInterval = 200;
      if ( 2 == m_pItemsSelect[9]->getSelectedIndex() )
         pCS->nGraphVideoRefreshInterval = 100;
      if ( 3 == m_pItemsSelect[9]->getSelectedIndex() )
         pCS->nGraphVideoRefreshInterval = 50;
      if ( 4 == m_pItemsSelect[9]->getSelectedIndex() )
         pCS->nGraphVideoRefreshInterval = 20;
      if ( 5 == m_pItemsSelect[9]->getSelectedIndex() )
         pCS->nGraphVideoRefreshInterval = 10;
      save_ControllerSettings();
      invalidate();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      return;
   }

   if ( m_IndexStatsDecode == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG_EXT_SHOW_STATS_VIDEO;
      else
         params.osd_flags2[layoutIndex] |= OSD_FLAG_EXT_SHOW_STATS_VIDEO;
      sendToVehicle = true;
   }

   if ( m_IndexStatsVideoExtended == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         params.osd_flags[layoutIndex] &= ~OSD_FLAG_EXTENDED_VIDEO_DECODE_HISTORY;
         params.osd_flags2[layoutIndex] |= OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS;
      }
      else if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
      {
         params.osd_flags[layoutIndex] &= ~OSD_FLAG_EXTENDED_VIDEO_DECODE_HISTORY;
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS;
      }
      else
      {
         params.osd_flags[layoutIndex] |= OSD_FLAG_EXTENDED_VIDEO_DECODE_HISTORY;
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS;
      }
      sendToVehicle = true;
   }

   if ( m_IndexStatsAdaptiveVideoGraph == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[16]->getSelectedIndex() )
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG_EXT_SHOW_ADAPTIVE_VIDEO_GRAPH;
      else
         params.osd_flags2[layoutIndex] |= OSD_FLAG_EXT_SHOW_ADAPTIVE_VIDEO_GRAPH;
      sendToVehicle = true;
   }

   if ( m_IndexStatsVideoStreamInfo == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[23]->getSelectedIndex() )
         params.osd_flags[layoutIndex] &= ~OSD_FLAG_SHOW_STATS_VIDEO_INFO;
      else
      {
         params.osd_flags[layoutIndex] |= OSD_FLAG_SHOW_STATS_VIDEO_INFO;
         if ( 1 == m_pItemsSelect[23]->getSelectedIndex() )
            pCS->iShowVideoStreamInfoCompact = 1;
         else
            pCS->iShowVideoStreamInfoCompact = 0;
         save_ControllerSettings();
      }
      sendToVehicle = true;    
   }


   if ( m_IndexSnapshot == m_SelectedIndex )
   {
      pP->iDebugShowVideoSnapshotOnDiscard = m_pItemsSelect[13]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
   }

   if ( m_IndexSnapshotTimeout == m_SelectedIndex )
   {
      pCS->iVideoDecodeStatsSnapshotClosesOnTimeout = m_pItemsSelect[22]->getSelectedIndex();
      save_ControllerSettings();
      invalidate();
      valuesToUI();
   }

   if ( m_IndexStatsEff == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
         params.osd_flags[layoutIndex] &= ~OSD_FLAG_SHOW_EFFICIENCY_STATS;
      else
         params.osd_flags[layoutIndex] |= OSD_FLAG_SHOW_EFFICIENCY_STATS;
      sendToVehicle = true;
   }

   if ( m_IndexStatsRC == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[6]->getSelectedIndex() )
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG_EXT_SHOW_STATS_RC;
      else
         params.osd_flags2[layoutIndex] |= OSD_FLAG_EXT_SHOW_STATS_RC;
      sendToVehicle = true;
   }

   if ( m_IndexDevStatsVideo == m_SelectedIndex )
   {
      pP->iDebugShowDevVideoStats = m_pItemsSelect[10]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
   }

   if ( m_IndexShowControllerAdaptiveInfoStats == m_SelectedIndex )
   {
      params.osd_flags3[layoutIndex] &= ~OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO;
      if ( 1 == m_pItemsSelect[24]->getSelectedIndex() )
         params.osd_flags3[layoutIndex] |= OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO;
      sendToVehicle = true;
   }
   
   if ( m_IndexDevVehicleVideoBitrateHistory == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessageNeedsVehcile("You need to be connected to a vehicle to change this.", 1);
         valuesToUI();
         return;
      }

      if ( 0 == m_pItemsSelect[19]->getSelectedIndex() )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_VIDEO_BITRATE_HISTORY);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_VIDEO_BITRATE_HISTORY;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_DEVELOPER_FLAGS, g_pCurrentModel->uDeveloperFlags, NULL, 0) )
         valuesToUI(); 
   }

   if ( m_IndexDevStatsVehicleTx == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessageNeedsVehcile("You need to be connected to a vehicle to change this.", 1);
         valuesToUI();
         return;
      }

      if ( 0 == m_pItemsSelect[20]->getSelectedIndex() )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_DEVELOPER_FLAGS, g_pCurrentModel->uDeveloperFlags, NULL, 0) )
         valuesToUI();  
      return;    
   }


   if ( m_IndexDevStatsRadio == m_SelectedIndex )
   {
      pP->iDebugShowDevRadioStats = m_pItemsSelect[11]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
   }

   if ( m_IndexDevFullRXStats == m_SelectedIndex )
   {
      pP->iDebugShowFullRXStats = m_pItemsSelect[12]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
   }

   if ( m_IndexDevStatsVehicleVideo == m_SelectedIndex )
   {
      pP->iDebugShowVehicleVideoStats = m_pItemsSelect[14]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      if ( NULL != g_pCurrentModel )
         g_pCurrentModel->b_mustSyncFromVehicle = true;
   }
   
   if ( m_IndexDevStatsVehicleVideoGraphs == m_SelectedIndex )
   {
      pP->iDebugShowVehicleVideoGraphs = m_pItemsSelect[15]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      if ( NULL != g_pCurrentModel )
         g_pCurrentModel->b_mustSyncFromVehicle = true;
   }

   if ( g_pCurrentModel->is_spectator )
   {
      memcpy(&(g_pCurrentModel->osd_params), &params, sizeof(osd_parameters_t));
      saveAsCurrentModel(g_pCurrentModel);
      valuesToUI();
   }
   else if ( sendToVehicle )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;
   }
}
