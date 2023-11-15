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
#include "../common/string_utils.h"
#include "menu.h"

#include "menu_vehicle_osd_instruments.h"
#include "menu_vehicle_osd_plugins.h"
#include "menu_vehicle_osd_plugin.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"
#include "menu_vehicle_instruments_general.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include "osd_plugins.h"


MenuVehicleOSDInstruments::MenuVehicleOSDInstruments(void)
:Menu(MENU_ID_OSD_AHI, "Intruments/Gauges Settings", NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.30;
   float fSliderWidth = 0.10;

   char szBuff[256];
   snprintf(szBuff, sizeof(szBuff), "Instruments/Gauges Settings (%s)", str_get_osd_screen_name(g_pCurrentModel->osd_params.layout));
   setTitle(szBuff);
   
   for( int i=0; i<50; i++ )
   {
      m_pItemsSelect[i] = NULL;
      m_IndexPlugins[i] = -1;
      m_IndexPluginsConfigure[i] = -1;
   }

   m_pItemsSelect[6] = new MenuItemSelect("Show Speed/Alt", "Shows the speed and altitude instruments");
   m_pItemsSelect[6]->addSelection("No");
   m_pItemsSelect[6]->addSelection("Yes");
   m_pItemsSelect[6]->setUseMultiViewLayout();
   m_IndexAHIShowSpeedAlt = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[8] = new MenuItemSelect("   Side Speed/Alt", "Shows the instruments for speed and altitude to the edge of the screen");  
   m_pItemsSelect[8]->addSelection("No");
   m_pItemsSelect[8]->addSelection("Yes");
   m_pItemsSelect[8]->setUseMultiViewLayout();
   m_IndexAHIToSides = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[9] = new MenuItemSelect("Show Heading", "Shows the heading instrument");  
   m_pItemsSelect[9]->addSelection("No");
   m_pItemsSelect[9]->addSelection("Yes");
   m_pItemsSelect[9]->setUseMultiViewLayout();
   m_IndexAHIShowHeading = addMenuItem(m_pItemsSelect[9]);

   m_pItemsSelect[10] = new MenuItemSelect("Show Altitude Graph/Glide", "Shows the altitude graph and glide efficiency.");
   m_pItemsSelect[10]->addSelection("No");
   m_pItemsSelect[10]->addSelection("Yes");
   m_pItemsSelect[10]->setUseMultiViewLayout();
   m_IndexAHIShowAltGraph = addMenuItem(m_pItemsSelect[10]);

   addMenuItem(new MenuItemSection("Plugins"));

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;
      char* szName = osd_plugins_get_short_name(i);
      m_pItemsSelect[20+i] = new MenuItemSelect(szName, "Show/hide this plugin on current OSD layout");
      m_pItemsSelect[20+i]->addSelection("No");
      m_pItemsSelect[20+i]->addSelection("Yes");
      m_pItemsSelect[20+i]->setUseMultiViewLayout();
      m_IndexPlugins[20+i] = addMenuItem(m_pItemsSelect[20+i]);

      char szDesc[128];
      snprintf(szDesc, sizeof(szDesc), "Configure this plugin (%s)", szName);
      MenuItem* pItem = new MenuItem("   Configure", szDesc);
      pItem->showArrow();
      m_IndexPluginsConfigure[20+i] = addMenuItem(pItem);
   }
}

MenuVehicleOSDInstruments::~MenuVehicleOSDInstruments()
{
}

void MenuVehicleOSDInstruments::valuesToUI()
{
   Preferences* p = get_Preferences();
   int layoutIndex = g_pCurrentModel->osd_params.layout;

   //log_dword("start: osd flags", g_pCurrentModel->osd_params.osd_flags[layoutIndex]);
   //log_dword("start: instruments flags", g_pCurrentModel->osd_params.instruments_flags[layoutIndex]);
   //log_line("current layout: %d, show instr: %d", (g_pCurrentModel->osd_params.osd_flags[layoutIndex] & OSD_FLAG_AHI_STYLE_MASK)>>3, g_pCurrentModel->osd_params.instruments_flags[layoutIndex] & INSTRUMENTS_FLAG_SHOW_INSTRUMENTS);

   m_pItemsSelect[6]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SHOW_SPEEDALT)?1:0);
   m_pItemsSelect[8]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SPEED_TO_SIDES)?1:0);

   m_pItemsSelect[9]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SHOW_HEADING)?1:0);
   m_pItemsSelect[10]->setSelection(((g_pCurrentModel->osd_params.instruments_flags[layoutIndex]) & INSTRUMENTS_FLAG_SHOW_ALTGRAPH)?1:0);


   if ( m_pItemsSelect[6]->isEnabled() && 1 == m_pItemsSelect[6]->getSelectedIndex() )
      m_pItemsSelect[8]->setEnabled(true);
   else
      m_pItemsSelect[8]->setEnabled(false);

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;
      u32 flags = g_pCurrentModel->osd_params.instruments_flags[layoutIndex];
      if ( flags & (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i) )
      {
         m_pItemsSelect[20+i]->setSelectedIndex(1);
         m_pMenuItems[m_IndexPluginsConfigure[20+i]]->setEnabled(true);
      }
      else
      {
         m_pItemsSelect[20+i]->setSelectedIndex(0);
         m_pMenuItems[m_IndexPluginsConfigure[20+i]]->setEnabled(false);
      }
   }
}


void MenuVehicleOSDInstruments::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}

void MenuVehicleOSDInstruments::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
}

void MenuVehicleOSDInstruments::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   Preferences* p = get_Preferences();
   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   bool sendToVehicle = false;
   int layoutIndex = g_pCurrentModel->osd_params.layout;

   if ( m_IndexAHIShowSpeedAlt == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[6]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SHOW_SPEEDALT;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SHOW_SPEEDALT;
      sendToVehicle = true;
   }

   if ( m_IndexAHIToSides == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[8]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SPEED_TO_SIDES;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SPEED_TO_SIDES;
      sendToVehicle = true;
   }

   if ( m_IndexAHIShowHeading == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[9]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SHOW_HEADING;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SHOW_HEADING;
      sendToVehicle = true;
   }

   if ( m_IndexAHIShowAltGraph == m_SelectedIndex )
   {
      u32 idx = m_pItemsSelect[10]->getSelectedIndex();
      params.instruments_flags[layoutIndex] &= ~INSTRUMENTS_FLAG_SHOW_ALTGRAPH;
      if ( idx > 0 )
         params.instruments_flags[layoutIndex] |= INSTRUMENTS_FLAG_SHOW_ALTGRAPH;
      sendToVehicle = true;
   }

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;

      if ( m_IndexPlugins[20+i] != -1 )
      if ( m_SelectedIndex == m_IndexPlugins[20+i] )
      {
         u32 flags = g_pCurrentModel->osd_params.instruments_flags[layoutIndex];
         if ( m_pItemsSelect[20+i]->getSelectedIndex() == 1 )
            params.instruments_flags[layoutIndex] |= (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i);
         else
            params.instruments_flags[layoutIndex] &= ~(INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i);
         sendToVehicle = true;
      }
      if ( m_IndexPluginsConfigure[20+i] != -1 )
      if ( m_SelectedIndex == m_IndexPluginsConfigure[20+i] )
      {
         MenuVehicleOSDPlugin* pMenu = new MenuVehicleOSDPlugin(i);
         add_menu_to_stack(pMenu);
         return;
      }
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