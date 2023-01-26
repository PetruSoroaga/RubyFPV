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
#include "menu.h"
#include "menu_vehicle_alarms.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"

MenuVehicleAlarms::MenuVehicleAlarms(void)
:Menu(MENU_ID_VEHICLE_ALARMS, "Vehicle Alarms and Warnings Settings", NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.20;

   m_pItemsSelect[0] = new MenuItemSelect("Enable Voltage Alarm", "Show an alarm on screen when vehicle battery voltage drops below a value.");  
   m_pItemsSelect[0]->addSelection("No");
   m_pItemsSelect[0]->addSelection("Yes");
   addMenuItem(m_pItemsSelect[0]);

   m_pItemsRange[0] = new MenuItemRange("Alarm voltage:", "Voltage at which the alarm will trigger.", 2.0, 4.6, g_pCurrentModel->osd_params.voltage_alarm, 0.1 );  
   m_pItemsRange[0]->setSufix("V");
   addMenuItem(m_pItemsRange[0]);

   m_pItemsRange[1] = new MenuItemRange("Pitch/Roll warning angle:", "Pitch and roll angle at witch to show a OSD warning indication about attitude (0 for disabled).", 0, 80, g_pCurrentModel->osd_params.ahi_warning_angle, 5 );  
   m_pItemsRange[1]->setSufix("°");
   addMenuItem(m_pItemsRange[1]);

   //m_pItemsSelect[1] = new MenuItemSelect("Enable Overload Alarm", "Shows an alarm when the vehicle CPU is overloaded or the datalink is overloaded.");  
   //m_pItemsSelect[1]->addSelection("No");
   //m_pItemsSelect[1]->addSelection("Yes");
   //m_IndexOverload = addMenuItem(m_pItemsSelect[1]);
   m_IndexOverload = -1;

   m_pItemsSelect[2] = new MenuItemSelect("Controller Link Lost Alarm", "Shows an alarm when the vehicle looses connection with the controller. That is, vehicle does not receive the uplink data for a period of time.");  
   m_pItemsSelect[2]->addSelection("Disabled");
   m_pItemsSelect[2]->addSelection("Enabled");
   m_IndexLinkLost = addMenuItem(m_pItemsSelect[2]);
}

MenuVehicleAlarms::~MenuVehicleAlarms()
{
}

void MenuVehicleAlarms::valuesToUI()
{
   m_pItemsSelect[0]->setSelection(g_pCurrentModel->osd_params.voltage_alarm_enabled);

   m_pItemsRange[0]->setCurrentValue(g_pCurrentModel->osd_params.voltage_alarm);
   m_pItemsRange[1]->setCurrentValue(g_pCurrentModel->osd_params.ahi_warning_angle);

   if ( g_pCurrentModel->osd_params.voltage_alarm_enabled )
      m_pItemsRange[0]->setEnabled(true);
   else
      m_pItemsRange[0]->setEnabled(false);

   //m_pItemsSelect[1]->setSelection(g_pCurrentModel->osd_params.show_overload_alarm);
}

void MenuVehicleAlarms::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleAlarms::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   bool sendToVehicle = false;
   osd_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));

   if ( 0 == m_SelectedIndex )
   {
      params.voltage_alarm_enabled = m_pItemsSelect[0]->getSelectedIndex();
      sendToVehicle = true;
   }

   if ( 1 == m_SelectedIndex )
   {
      params.voltage_alarm = m_pItemsRange[0]->getCurrentValue();
      if ( params.voltage_alarm < 0 )
         params.voltage_alarm = 0;
      sendToVehicle = true;
   }

   if ( 2 == m_SelectedIndex )
   {
      params.ahi_warning_angle = m_pItemsRange[1]->getCurrentValue();
      if ( params.ahi_warning_angle < 0 )
         params.ahi_warning_angle = 0;
      sendToVehicle = true;
   }
   
   //if ( m_IndexOverload == m_SelectedIndex )
   //{
   //   params.show_overload_alarm = (bool) m_pItemsSelect[1]->getSelectedIndex();
   //   sendToVehicle = true;
   //}

   if ( m_IndexLinkLost == m_SelectedIndex )
   {
      if ( m_pItemsSelect[2]->getSelectedIndex() == 0 )
      {
         for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
            params.osd_preferences[i] &= ~(OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM); // controller link lost alarm disabled
      }
      else
      {
         for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
            params.osd_preferences[i] |= OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM; // controller link lost alarm enabled
      }
      sendToVehicle = true;
   }

   if ( sendToVehicle )
   {
      if ( g_pCurrentModel->is_spectator )
      {
         memcpy(&(g_pCurrentModel->osd_params), &params, sizeof(osd_parameters_t));
         saveAsCurrentModel(g_pCurrentModel);
         valuesToUI();
         return;
      }
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t)) )
         valuesToUI();
      return;
   }
}
