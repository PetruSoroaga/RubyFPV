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
#include "../base/hardware.h"
#include "../base/launchers.h"
#include "../base/commands.h"
#include "menu.h"
#include "menu_vehicle_data_link.h"
#include "menu_confirmation.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include "handle_commands.h"


MenuVehicleDataLink::MenuVehicleDataLink(void)
:Menu(MENU_ID_VEHICLE_DATA_LINK, "Auxiliary Data Link", NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.24;

   addTopLine("Enable an auxiliary general purpose data link to be used for arbitrary data transport between the vehicle and the controller.");

   m_pItemsSelect[0] = new MenuItemSelect("Vehicle Serial Port", "The vehicle serial port to use for the auxiliary data link.");
   m_pItemsSelect[0]->addSelection("None");
   for( int i=0; i<g_pCurrentModel->hardware_info.serial_bus_count; i++ )
      m_pItemsSelect[0]->addSelection(g_pCurrentModel->hardware_info.serial_bus_names[i]);

   m_pItemsSelect[0]->setIsEditable();
   m_IndexPort = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Vehicle Serial Baudrate", "Sets the baud rate on the vehicle serial port for the auxiliary data link.");
   for( int i=0; i<hardware_get_serial_baud_rates_count(); i++ )
   {
      char szBuff[32];
      sprintf(szBuff, "%ld bps", hardware_get_serial_baud_rates()[i]);
      m_pItemsSelect[1]->addSelection(szBuff);
   }
   m_pItemsSelect[1]->setIsEditable();
   m_IndexSpeed = addMenuItem(m_pItemsSelect[1]);

   addMenuItem(new MenuItemText("Note: Don't forget to assign a serial port on the controller too, to be used for the auxiliary data link."));
}

MenuVehicleDataLink::~MenuVehicleDataLink()
{
}

void MenuVehicleDataLink::valuesToUI()
{
   int iCurrentSerialPortIndex = -1;
   u32 uCurrentSerialPortSpeed = 0;
   for( int i=0; i<g_pCurrentModel->hardware_info.serial_bus_count; i++ )
   {
       
       if ( g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & ((1<<5)<<8) )
       if ( (g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & 0xFF) == SERIAL_PORT_USAGE_DATA_LINK )
       {
          iCurrentSerialPortIndex = i;
          uCurrentSerialPortSpeed = g_pCurrentModel->hardware_info.serial_bus_speed[i];
          break;
       }
   }

   if ( -1 == iCurrentSerialPortIndex || 0 == uCurrentSerialPortSpeed )
   {
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSelect[1]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[0]->setSelectedIndex( 1 + iCurrentSerialPortIndex );
      m_pItemsSelect[1]->setEnabled(true);
   }
   bool bSpeedFound = false;
   for(int i=0; i<m_pItemsSelect[1]->getSelectionsCount(); i++ )
      if ( hardware_get_serial_baud_rates()[i] == uCurrentSerialPortSpeed )
      {
         m_pItemsSelect[1]->setSelection(i);
         bSpeedFound = true;
         break;
      }

   if ( ! bSpeedFound )
   {
      char szBuff[256];
      sprintf(szBuff, "Info: You are using a custom baud rate (%ld) for the auxiliary data link.", uCurrentSerialPortSpeed);
      addTopLine(szBuff);
   }
}

void MenuVehicleDataLink::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleDataLink::onSelectItem()
{
   Menu::onSelectItem();
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;


   int iCurrentSerialPortIndex = -1;
   u32 uCurrentSerialPortSpeed = 0;
   for( int i=0; i<g_pCurrentModel->hardware_info.serial_bus_count; i++ )
   {
       
       if ( g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & ((1<<5)<<8) )
       if ( (g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & 0xFF) == SERIAL_PORT_USAGE_DATA_LINK )
       {
          iCurrentSerialPortIndex = i;
          uCurrentSerialPortSpeed = g_pCurrentModel->hardware_info.serial_bus_speed[i];
          break;
       }
   }


   if ( m_IndexPort == m_SelectedIndex )
   {
      int iSerialPort = m_pItemsSelect[0]->getSelectedIndex();
      if ( iSerialPort != 0 )
      if ( (g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[iSerialPort-1] & 0xFF) == SERIAL_PORT_USAGE_HARDWARE_RADIO )
      {
         m_iConfirmationId = 1;
         MenuConfirmation* pMC = new MenuConfirmation("Can't use serial port", "The serial port you selected can't be used for a custom data link as it's used for a radio interface.",m_iConfirmationId, true);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
         valuesToUI();
         return;
      }

      if ( 0 == iSerialPort )
      {
         if ( iCurrentSerialPortIndex != -1 )
         {
            model_hardware_info_t new_info;
            memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardware_info), sizeof(model_hardware_info_t));
            new_info.serial_bus_supported_and_usage[iCurrentSerialPortIndex] &= 0xFFFFFF00;
            new_info.serial_bus_supported_and_usage[iCurrentSerialPortIndex] |= SERIAL_PORT_USAGE_NONE;
            
            log_line("Sending disabling data link serial port selection to vehicle.");
            if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(model_hardware_info_t)) )
               valuesToUI();

            return;
         }
      }
      else
      {
         model_hardware_info_t new_info;
         memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardware_info), sizeof(model_hardware_info_t));

         u8 uCurrentUsage = new_info.serial_bus_supported_and_usage[iSerialPort-1] & 0xFF;
         if ( uCurrentUsage != SERIAL_PORT_USAGE_DATA_LINK )
         {
            if ( uCurrentUsage == SERIAL_PORT_USAGE_TELEMETRY )
            {
               m_iConfirmationId = 1;
               MenuConfirmation* pMC = new MenuConfirmation("Telemetry Link Disabled", "The serial port was used by your telemetry link. It was reasigned to the auxiliary data link.",m_iConfirmationId, true);
               pMC->m_yPos = 0.3;
               add_menu_to_stack(pMC);      
            }
         }

         if ( iCurrentSerialPortIndex != -1 )
         {
            new_info.serial_bus_supported_and_usage[iCurrentSerialPortIndex] &= 0xFFFFFF00;
            new_info.serial_bus_supported_and_usage[iCurrentSerialPortIndex] |= SERIAL_PORT_USAGE_NONE;
         }
         new_info.serial_bus_supported_and_usage[iSerialPort-1] &= 0xFFFFFF00;
         new_info.serial_bus_supported_and_usage[iSerialPort-1] |= SERIAL_PORT_USAGE_DATA_LINK;
         
         log_line("Sending new serial port to be used for data link to vehicle.");
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(model_hardware_info_t)) )
            valuesToUI();
         return;
      }
   }

   if ( m_IndexSpeed == m_SelectedIndex )
   {
      if ( -1 == iCurrentSerialPortIndex )
         return;

      long val = hardware_get_serial_baud_rates()[m_pItemsSelect[1]->getSelectedIndex()];
      if ( val == g_pCurrentModel->hardware_info.serial_bus_speed[iCurrentSerialPortIndex] )
         return;

      model_hardware_info_t new_info;
      memcpy((u8*)&new_info, (u8*)&(g_pCurrentModel->hardware_info), sizeof(model_hardware_info_t));
      new_info.serial_bus_speed[iCurrentSerialPortIndex] = (u32)val;
      log_line("Sending new serial port speed for data link to vehicle.");
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_SERIAL_PORTS_INFO, 0, (u8*)&new_info, sizeof(model_hardware_info_t)) )
         valuesToUI();
      return;
   }
}
