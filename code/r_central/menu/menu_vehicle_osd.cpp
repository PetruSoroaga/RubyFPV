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
#include "menu_vehicle_osd.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"

#include "menu_vehicle_osd_plugins.h"
#include "menu_vehicle_osd_widgets.h"
#include "menu_vehicle_osd_stats.h"
#include "menu_vehicle_osd_elements.h"
#include "menu_vehicle_instruments_general.h"
#include "menu_vehicle_alarms.h"
#include "menu_preferences_ui.h"

#include "../osd/osd_common.h"


MenuVehicleOSD::MenuVehicleOSD(void)
:Menu(MENU_ID_VEHICLE_OSD, "OSD / Instruments Settings", NULL)
{
   m_Width = 0.30;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.15;

   m_IndexSettings = addMenuItem(new MenuItem("General OSD Settings", "Sets global settings for all OSD/instruments/gauges"));
   m_pMenuItems[m_IndexSettings]->showArrow();

   m_IndexOSDController = addMenuItem( new MenuItem("OSD Size, Color, Fonts", "Sets colors, size and fonts for OSD."));
   m_pMenuItems[m_IndexOSDController]->showArrow();

   m_IndexAlarms = -1;
   m_IndexPlugins = -1;
   m_IndexOSDShowMSPOSD = -1;

   addMenuItem(new MenuItemSection("OSD Layouts and Settings")); 


   m_pItemsSelect[0] = new MenuItemSelect("Screen", "Changes the active screen of the OSD elements and what information you see on this screen.");
   m_pItemsSelect[0]->addSelection("Screen 1");
   m_pItemsSelect[0]->addSelection("Screen 2");
   m_pItemsSelect[0]->addSelection("Screen 3");
   m_pItemsSelect[0]->addSelection("Lean");
   m_pItemsSelect[0]->addSelection("Lean Extended");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexOSDLayout = addMenuItem(m_pItemsSelect[0]);


   m_pItemsSelect[1] = new MenuItemSelect("Enabled", "Enables or disables this screen");  
   m_pItemsSelect[1]->addSelection("No");
   m_pItemsSelect[1]->addSelection("Yes");
   m_pItemsSelect[1]->addSelection("Only plugins");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexOSDEnabled = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("Font Size", "Increase/decrease OSD font size for current screen.");  
   m_pItemsSelect[2]->addSelection("Smallest");
   m_pItemsSelect[2]->addSelection("Smaller");
   m_pItemsSelect[2]->addSelection("Small");
   m_pItemsSelect[2]->addSelection("Normal");
   m_pItemsSelect[2]->addSelection("Large");
   m_pItemsSelect[2]->addSelection("Larger");
   m_pItemsSelect[2]->addSelection("Largest");
   m_pItemsSelect[2]->addSelection("XLarge");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexOSDFontSize = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[4] = new MenuItemSelect("Background Type", "Change if and how the background behind OSD elements is shown.");  
   m_pItemsSelect[4]->addSelection("None");
   m_pItemsSelect[4]->addSelection("On text only");
   m_pItemsSelect[4]->addSelection("As Bars");
   m_pItemsSelect[4]->setIsEditable();
   m_IndexBgOnTexts = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[3] = new MenuItemSelect("Background Transparency", "Change how transparent the OSD background is for current screen.");  
   m_pItemsSelect[3]->addSelection("Max");
   m_pItemsSelect[3]->addSelection("Medium");
   m_pItemsSelect[3]->addSelection("Normal");
   m_pItemsSelect[3]->addSelection("Minimum");
   m_pItemsSelect[3]->addSelection("None");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexOSDTransparency = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[5] = new MenuItemSelect("Highlight Changing Elements", "Highlight when important OSD elements change (i.e. radio modulation schemes).");  
   m_pItemsSelect[5]->addSelection("No");
   m_pItemsSelect[5]->addSelection("Yes");
   m_pItemsSelect[5]->setIsEditable();
   m_IndexHighlightChangeElements = addMenuItem(m_pItemsSelect[5]);


   m_pItemsSelect[6] = new MenuItemSelect("Don't show FC messages", "Do not show messages/texts from flight controller.");  
   m_pItemsSelect[6]->addSelection("No");
   m_pItemsSelect[6]->addSelection("Yes");
   m_pItemsSelect[6]->setIsEditable();
   m_IndexDontShowFCMessages = addMenuItem(m_pItemsSelect[6]);
  
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
   {
      m_pItemsSelect[11] = new MenuItemSelect("Show MSP OSD Elements", "Render the MSP OSD Elements");
      m_pItemsSelect[11]->addSelection("No");
      m_pItemsSelect[11]->addSelection("Yes");
      m_pItemsSelect[11]->setIsEditable();
      m_IndexOSDShowMSPOSD = addMenuItem(m_pItemsSelect[11]);
   }

   m_IndexOSDElements = addMenuItem(new MenuItem("Layout OSD Elements", "Configure which OSD elements show up on current screen"));
   m_pMenuItems[m_IndexOSDElements]->showArrow();

   m_IndexOSDWidgets = addMenuItem(new MenuItem("Layout OSD Widgets & Gauges", "Configure which OSD widgets and gauges show up on current screen and where to show them."));
   m_pMenuItems[m_IndexOSDWidgets]->showArrow();

   m_IndexOSDPlugins = addMenuItem(new MenuItem("Layout OSD Plugins", "Show/Hide/Configure the custom OSD plugins that are installed on the controller."));
   m_pMenuItems[m_IndexOSDPlugins]->showArrow();

   m_IndexOSDStats = addMenuItem(new MenuItem("Layout OSD Stats Windows", "Configure which statistics windows show up on current screen"));
   m_pMenuItems[m_IndexOSDStats]->showArrow();
}

MenuVehicleOSD::~MenuVehicleOSD()
{
}

void MenuVehicleOSD::valuesToUI()
{
   int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDLayout;
   
   m_pItemsSelect[0]->setSelectedIndex(layoutIndex);
   m_pItemsSelect[2]->setSelectedIndex(g_pCurrentModel->osd_params.osd_preferences[layoutIndex] & 0xFF);
   m_pItemsSelect[3]->setSelectedIndex(((g_pCurrentModel->osd_params.osd_preferences[layoutIndex]) & OSD_PREFERENCES_OSD_TRANSPARENCY_BITMASK) >> OSD_PREFERENCES_OSD_TRANSPARENCY_SHIFT);
   
   m_pItemsSelect[4]->setSelectedIndex(0);
   if ( g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY )
      m_pItemsSelect[4]->setSelectedIndex(1);
   else if ( g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG2_SHOW_BACKGROUND_BARS )
      m_pItemsSelect[4]->setSelectedIndex(2);
   
   m_pItemsSelect[5]->setSelectedIndex((g_pCurrentModel->osd_params.osd_flags3[layoutIndex] & OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS) ? 1:0);
   m_pItemsSelect[6]->setSelectedIndex((g_pCurrentModel->osd_params.osd_preferences[layoutIndex] & OSD_PREFERENCES_BIT_FLAG_DONT_SHOW_FC_MESSAGES) ? 1:0);
   
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
   if ( -1 != m_IndexOSDShowMSPOSD )
   {
      if ( g_pCurrentModel->osd_params.osd_flags3[layoutIndex] & OSD_FLAG3_RENDER_MSP_OSD )
         m_pItemsSelect[11]->setSelectedIndex(1);
      else
         m_pItemsSelect[11]->setSelectedIndex(0);
   }

   if ( g_pCurrentModel->osd_params.osd_flags2[layoutIndex] & OSD_FLAG2_LAYOUT_ENABLED )
   {
      m_pItemsSelect[1]->setSelectedIndex(1);
      m_pItemsSelect[2]->setEnabled(true);
      m_pItemsSelect[4]->setEnabled(true);
      if ( m_pItemsSelect[4]->getSelectedIndex() != 0 )
         m_pItemsSelect[3]->setEnabled(true);
      else
         m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[5]->setEnabled(true);
      m_pMenuItems[m_IndexOSDElements]->setEnabled(true);
      m_pMenuItems[m_IndexOSDWidgets]->setEnabled(true);
      m_pMenuItems[m_IndexOSDPlugins]->setEnabled(true);
      m_pMenuItems[m_IndexOSDStats]->setEnabled(true);

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      if ( -1 != m_IndexOSDShowMSPOSD )
         m_pItemsSelect[11]->setEnabled(true);
   }
   else if ( g_pCurrentModel->osd_params.osd_flags3[layoutIndex] & OSD_FLAG3_LAYOUT_ENABLED_PLUGINS_ONLY )
   {
      m_pItemsSelect[1]->setSelectedIndex(2);
      m_pItemsSelect[2]->setEnabled(true);
      m_pItemsSelect[4]->setEnabled(true);
      if ( m_pItemsSelect[4]->getSelectedIndex() != 0 )
         m_pItemsSelect[3]->setEnabled(true);
      else
         m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[5]->setEnabled(false);
      m_pMenuItems[m_IndexOSDElements]->setEnabled(false);
      m_pMenuItems[m_IndexOSDWidgets]->setEnabled(true);
      m_pMenuItems[m_IndexOSDPlugins]->setEnabled(true);
      m_pMenuItems[m_IndexOSDStats]->setEnabled(false);
      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      if ( -1 != m_IndexOSDShowMSPOSD )
         m_pItemsSelect[11]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[1]->setSelectedIndex(0);
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[4]->setEnabled(false);
      m_pItemsSelect[5]->setEnabled(true);
      m_pMenuItems[m_IndexOSDElements]->setEnabled(false);
      m_pMenuItems[m_IndexOSDWidgets]->setEnabled(false);
      m_pMenuItems[m_IndexOSDPlugins]->setEnabled(false);
      m_pMenuItems[m_IndexOSDStats]->setEnabled(false);
      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
      if ( -1 != m_IndexOSDShowMSPOSD )
         m_pItemsSelect[11]->setEnabled(false);
   }
}

void MenuVehicleOSD::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleOSD::onSelectItem()
{
   Menu::onSelectItem();
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexSettings == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleInstrumentsGeneral());
      return;
   }

   if ( m_IndexOSDController == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuPreferencesUI(true));
      return;
   }

   if ( m_IndexAlarms == m_SelectedIndex )
   {
      //add_menu_to_stack(new MenuVehicleAlarms());
      return;
   }

   if ( -1 != m_IndexPlugins )
   if ( m_IndexPlugins == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSDPlugins());
      return;
   }


   if ( m_IndexOSDElements == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSDElements());
      return;
   }

   if ( m_IndexOSDWidgets == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSDWidgets());
      return;
   }

   if ( m_IndexOSDPlugins == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSDPlugins());
      return;
   }

   if ( m_IndexOSDStats == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleOSDStats());
      return;
   }


   bool sendToVehicle = false;
   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   int layoutIndex = g_pCurrentModel->osd_params.iCurrentOSDLayout;

   if ( m_IndexOSDLayout == m_SelectedIndex )
   {
      params.iCurrentOSDLayout = m_pItemsSelect[0]->getSelectedIndex();
      sendToVehicle = true;

      u32 scale = params.osd_preferences[params.iCurrentOSDLayout] & 0xFF;
      osd_setScaleOSD((int)scale);
      osd_apply_preferences();
      applyFontScaleChanges();
      g_bHasVideoDecodeStatsSnapshot = false;
   }

   if ( m_IndexOSDEnabled == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[1]->getSelectedIndex() )
      {
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG2_LAYOUT_ENABLED;
         params.osd_flags3[layoutIndex] &= ~OSD_FLAG3_LAYOUT_ENABLED_PLUGINS_ONLY;
      }
      else if ( 1 == m_pItemsSelect[1]->getSelectedIndex() )
         params.osd_flags2[layoutIndex] |= OSD_FLAG2_LAYOUT_ENABLED;
      else
      {
         params.osd_flags2[layoutIndex] &= ~OSD_FLAG2_LAYOUT_ENABLED;
         params.osd_flags3[layoutIndex] |= OSD_FLAG3_LAYOUT_ENABLED_PLUGINS_ONLY;
      }
      sendToVehicle = true;
   }

   if ( m_IndexOSDFontSize == m_SelectedIndex )
   {
      params.osd_preferences[layoutIndex] &= 0xFFFFFF00;
      params.osd_preferences[layoutIndex] |= (u32)m_pItemsSelect[2]->getSelectedIndex();
      sendToVehicle = true;

      u32 scale = params.osd_preferences[layoutIndex] & 0xFF;
      osd_setScaleOSD((int)scale);
      osd_apply_preferences();
      applyFontScaleChanges();
   }

   if ( m_IndexOSDTransparency == m_SelectedIndex )
   {
      params.osd_preferences[layoutIndex] &= 0xFFFF00FF;
      params.osd_preferences[layoutIndex] |= ((((u32)m_pItemsSelect[3]->getSelectedIndex()) << OSD_PREFERENCES_OSD_TRANSPARENCY_SHIFT) & OSD_PREFERENCES_OSD_TRANSPARENCY_BITMASK);
      sendToVehicle = true;
   }

   if ( m_IndexBgOnTexts == m_SelectedIndex )
   {
      params.osd_flags2[layoutIndex] &= ~OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY;
      params.osd_flags2[layoutIndex] &= ~OSD_FLAG2_SHOW_BACKGROUND_BARS;
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         params.osd_flags2[layoutIndex] |= OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY;
      if ( 2 == m_pItemsSelect[4]->getSelectedIndex() )
         params.osd_flags2[layoutIndex] |= OSD_FLAG2_SHOW_BACKGROUND_BARS;
      sendToVehicle = true;
   }

   if ( m_IndexHighlightChangeElements == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
         params.osd_flags3[layoutIndex] &= ~OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS;
      else
         params.osd_flags3[layoutIndex] |= OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS;
      sendToVehicle = true;
   }

   if ( m_IndexDontShowFCMessages == m_SelectedIndex )
   {
      if ( 0 == m_pItemsSelect[6]->getSelectedIndex() )
         params.osd_preferences[layoutIndex] &= ~OSD_PREFERENCES_BIT_FLAG_DONT_SHOW_FC_MESSAGES;
      else
         params.osd_preferences[layoutIndex] |= OSD_PREFERENCES_BIT_FLAG_DONT_SHOW_FC_MESSAGES;
      sendToVehicle = true;
   }

   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MSP) )
   if ( (-1 != m_IndexOSDShowMSPOSD) && (m_IndexOSDShowMSPOSD == m_SelectedIndex) )
   {
      if ( 0 == m_pItemsSelect[11]->getSelectedIndex() )
         params.osd_flags3[layoutIndex] &= ~OSD_FLAG3_RENDER_MSP_OSD;
      else
         params.osd_flags3[layoutIndex] |= OSD_FLAG3_RENDER_MSP_OSD;
      sendToVehicle = true;
   }

   if ( g_pCurrentModel->is_spectator )
   {
      memcpy(&(g_pCurrentModel->osd_params), &params, sizeof(osd_parameters_t));
      saveControllerModel(g_pCurrentModel);
      valuesToUI();
   }
   else if ( sendToVehicle )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;
   }
}
