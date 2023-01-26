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
#include "../public/ruby_core_plugin.h"
#include "render_commands.h"
#include "osd.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "menu.h"
#include "menu_vehicle.h"
#include "menu_vehicle_general.h"
//#include "menu_vehicle_radio.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_osd.h"
#include "menu_vehicle_expert.h"
#include "menu_confirmation.h"
#include "menu_vehicle_telemetry.h"
#include "menu_vehicle_peripherals.h"
#include "menu_item_section.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include <ctype.h>
#include "handle_commands.h"

MenuVehiclePeripherals::MenuVehiclePeripherals(void)
:Menu(MENU_ID_VEHICLE_PERIPHERALS, "Vehicle Peripherals Settings", NULL)
{
   m_Width = 0.24;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.4;
   m_bWaitingForVehicleInfo = false;
   
   char szBuff[256];

   addMenuItem(new MenuItemSection("Serial Ports"));

   for( int i=0; i<g_pCurrentModel->hardware_info.serial_bus_count; i++ )
   {
      sprintf( szBuff, "%s Usage", g_pCurrentModel->hardware_info.serial_bus_names[i] );
      
      m_pItemsSelect[i*2] = new MenuItemSelect(szBuff);
      if ( ! (g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & ((1<<5)<<8)) )
      {
         m_pItemsSelect[i*2]->addSelection("Not Supported");
         m_pItemsSelect[i*2]->setEnabled(false);
      }
      else
      {
         m_pItemsSelect[i*2]->addSelection("None");
         m_pItemsSelect[i*2]->addSelection("Telemetry");
         #ifdef FEATURE_MSP_OSD
         m_pItemsSelect[i*2]->addSelection("MSP OSD PitLab");
         #else
         m_pItemsSelect[i*2]->addSelection("MSP OSD PitLab", false);
         #endif
         m_pItemsSelect[i*2]->addSelection("Custom Data Link");
         m_IndexStartPortUsagePluginsStartIndex[i] = 4;

         for( int n=0; n<g_iVehicleCorePluginsCount; n++ )
         {
            if ( g_listVehicleCorePlugins[n].uRequestedCapabilities & CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART )
            {
               char szOption[256];
               sprintf(szOption, "Core Plugin %s", g_listVehicleCorePlugins[n].szName);
               m_pItemsSelect[i*2]->addSelection(szOption);
            }
         }

         m_pItemsSelect[i*2]->setIsEditable();
      }
      m_IndexSerialPortsUsage[i] = addMenuItem(m_pItemsSelect[i*2]);

      sprintf( szBuff, "%s Baudrate", g_pCurrentModel->hardware_info.serial_bus_names[i] );
      m_pItemsSelect[i*2+1] = new MenuItemSelect(szBuff, "Sets the baud rate of this serial port.");
      for( int n=0; n<hardware_get_serial_baud_rates_count(); n++ )
      {
         sprintf(szBuff, "%ld bps", hardware_get_serial_baud_rates()[n]);
         m_pItemsSelect[i*2+1]->addSelection(szBuff);
      }
      m_pItemsSelect[i*2+1]->setIsEditable();
      m_IndexSerialPortsBaudRate[i] = addMenuItem(m_pItemsSelect[i*2+1]);
   }
}

MenuVehiclePeripherals::~MenuVehiclePeripherals()
{
}

void MenuVehiclePeripherals::onShow()
{
   Menu::onShow();
}


void MenuVehiclePeripherals::valuesToUI()
{
   for( int i=0; i<g_pCurrentModel->hardware_info.serial_bus_count; i++ )
   {
      if ( ! (g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & ((1<<5)<<8)) )
      {
         m_pItemsSelect[i*2]->setSelectedIndex(0);
         m_pItemsSelect[i*2]->setEnabled(false);
         continue;
      }

      m_pItemsSelect[i*2]->setEnabled(true);
      
      u8 uUsage = g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & 0xFF;
      if ( uUsage < SERIAL_PORT_USAGE_CORE_PLUGIN )
      {
         if ( uUsage == SERIAL_PORT_USAGE_NONE )
            m_pItemsSelect[i*2]->setSelectedIndex(0);
         if ( uUsage == SERIAL_PORT_USAGE_TELEMETRY )
            m_pItemsSelect[i*2]->setSelectedIndex(1);
         if ( uUsage == SERIAL_PORT_USAGE_MSP_OSD_PITLAB )
            m_pItemsSelect[i*2]->setSelectedIndex(2);
         if ( uUsage == SERIAL_PORT_USAGE_DATA_LINK )
            m_pItemsSelect[i*2]->setSelectedIndex(3);
      }
      else if ( uUsage - 20 > g_iVehicleCorePluginsCount )
         m_pItemsSelect[i*2]->setSelectedIndex(0);
      else
      {
         int iCountUARTPlugins = 0;
         int iTargetPluginIndex = uUsage - 20;
         for( int n=0; n<g_iVehicleCorePluginsCount; n++ )
         {
            if ( g_listVehicleCorePlugins[n].uRequestedCapabilities & CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART )
            {
               if ( iTargetPluginIndex == n )
               {
                  m_pItemsSelect[i*2]->setSelectedIndex(m_IndexStartPortUsagePluginsStartIndex[i] + iCountUARTPlugins);
                  break;
               }
               iCountUARTPlugins++;
            }
         }
      }

      bool bFoundSpeed = false;
      for(int n=0; n<m_pItemsSelect[i*2+1]->getSelectionsCount(); n++ )
      {
         if ( hardware_get_serial_baud_rates()[n] == g_pCurrentModel->hardware_info.serial_bus_speed[i] )
         {
            m_pItemsSelect[i*2+1]->setSelection(n);
            bFoundSpeed = true;
            break;
         }
      }
      if ( ! bFoundSpeed )
      {
         m_pItemsSelect[i*2+1]->setSelection(0);
      }
   }
}

void MenuVehiclePeripherals::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();   
   float y = yTop + m_ExtraHeight;
 
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehiclePeripherals::onSelectItem()
{
   
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   for( int i=0; i<g_pCurrentModel->hardware_info.serial_bus_count; i++ )
   {
      if ( m_IndexSerialPortsUsage[i] == m_SelectedIndex )
      {
         int newUsage = -1;

         if ( m_pItemsSelect[i*2]->getSelectedIndex() < m_IndexStartPortUsagePluginsStartIndex[i] )
         {
            if ( 0 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_NONE;
            if ( 1 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_TELEMETRY;
            if ( 2 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_MSP_OSD_PITLAB;
            if ( 3 == m_pItemsSelect[i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_DATA_LINK;
            if ( newUsage == (g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & 0xFF) )
               return;
         }
         else
         {
            int iCountUARTPlugins = 0;
            for( int n=0; n<g_iVehicleCorePluginsCount; n++ )
            {
               if ( g_listVehicleCorePlugins[n].uRequestedCapabilities & CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART )
               {
                  if ( (m_pItemsSelect[i*2]->getSelectedIndex() - m_IndexStartPortUsagePluginsStartIndex[i]) == iCountUARTPlugins )
                  {
                     newUsage = SERIAL_PORT_USAGE_CORE_PLUGIN + n;
                     break;
                  }
                  iCountUARTPlugins++;
               }
            }
         }
         if ( newUsage < 0 )
            return;
         if ( newUsage == g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & 0xFF )
            return;

         model_hardware_info_t new_info;
         memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardware_info), sizeof(model_hardware_info_t));
         new_info.serial_bus_supported_and_usage[i] &= 0xFFFFFF00;
         new_info.serial_bus_supported_and_usage[i] |= (u8)newUsage;
         log_line("Sending new serial ports usage to vehicle.");
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(model_hardware_info_t)) )
            valuesToUI();
         return;
      }

      if ( m_IndexSerialPortsBaudRate[i] == m_SelectedIndex )
      {
         long val = hardware_get_serial_baud_rates()[m_pItemsSelect[i*2+1]->getSelectedIndex()];
         if ( val == g_pCurrentModel->hardware_info.serial_bus_speed[i] )
            return;

         model_hardware_info_t new_info;
         memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardware_info), sizeof(model_hardware_info_t));
         new_info.serial_bus_speed[i] = val;
         log_line("Sending new serial ports info to vehicle.");
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(model_hardware_info_t)) )
            valuesToUI();
         return;
      }
   }
}
