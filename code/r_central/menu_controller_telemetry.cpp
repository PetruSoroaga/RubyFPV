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
#include "menu.h"
#include "menu_objects.h"
#include "menu_controller_telemetry.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "menu_item_range.h"

#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/launchers.h"
#include "../base/hardware.h"

#include "pairing.h"

#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "process_router_messages.h"
#include "ruby_central.h"


MenuControllerTelemetry::MenuControllerTelemetry(void)
:Menu(MENU_ID_CONTROLLER_TELEMETRY, "Telemetry Input/Output Settings", NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.25;
   m_iConfirmationId = 0;
   
   load_ControllerSettings();
   ControllerSettings* pCS = get_ControllerSettings();
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   Preferences* pp = get_Preferences();

   m_pItemsSelect[0] = new MenuItemSelect("Telemetry Input/Output", "Enables or disables the telemetry output on controller's serial ports");
   m_pItemsSelect[0]->addSelection("None");
   m_pItemsSelect[0]->addSelection("Outgoing");
   m_pItemsSelect[0]->addSelection("Incoming");
   m_pItemsSelect[0]->addSelection("Bidirectional");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexInOut = addMenuItem(m_pItemsSelect[0]);
   
   m_pItemsSelect[1] = new MenuItemSelect("   Telemetry Serial Port", "Sets the serial port to use for telemetry input/output to other devices.");
   m_pItemsSelect[1]->addSelection("None");
   for( int i=0; i<hardware_get_serial_ports_count(); i++ )
   {
      hw_serial_port_info_t* pInfo = hardware_get_serial_port_info(i);
      if ( NULL == pInfo )
         continue;
      m_pItemsSelect[1]->addSelection(pInfo->szName);
   }
   m_pItemsSelect[1]->setIsEditable();
   m_IndexSerialPort = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("   Telemetry Port Baudrate", "Sets the baud rate of the serial port used for telemetry.");
   for( int n=0; n<hardware_get_serial_baud_rates_count(); n++ )
   {
      char szBuff[32];
      snprintf(szBuff, sizeof(szBuff), "%d bps", hardware_get_serial_baud_rates()[n]);
      m_pItemsSelect[2]->addSelection(szBuff);
   }
   m_pItemsSelect[2]->setIsEditable();
   m_IndexSerialSpeed = addMenuItem(m_pItemsSelect[2]);


   addMenuItem(new MenuItemSection("Other Telemetry Outputs"));

   m_pItemsSelect[3] = new MenuItemSelect("Enable USB Device Telemetry Output", "Enables or disables forwarding the telemetry stream received from vehicle to an external device using a USB connection.");
   m_pItemsSelect[3]->addSelection("Disabled");
   m_pItemsSelect[3]->addSelection("Enabled");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexTelemetryUSBForward = addMenuItem(m_pItemsSelect[3]);

   m_pItemsRange[0] = new MenuItemRange("    USB Telemetry Port Number", "Sets the USB port number to output telemetry to.", 1025, 32000, 5001, 1 );
   m_pItemsRange[0]->setSufix("");
   m_IndexTelemetryUSBPort = addMenuItem(m_pItemsRange[0]);

   m_pItemsRange[1] = new MenuItemRange("    USB Packet Size", "Sets the data packet size send to USB. Some VR apps are sensitive to this value.", 10, 2048, 1024, 1 );
   m_pItemsRange[1]->setSufix("");
   m_IndexTelemetryUSBPacket = addMenuItem(m_pItemsRange[1]);

   m_ExtraHeight = 0;
}

MenuControllerTelemetry::~MenuControllerTelemetry(void)
{
}

void MenuControllerTelemetry::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   Preferences* pp = get_Preferences();
   char szBuff[256];

   m_pItemsSelect[0]->setSelectedIndex(0);
   m_pItemsSelect[1]->setSelectedIndex(0);
   m_pItemsSelect[1]->setEnabled(false);
   m_pItemsSelect[2]->setEnabled(false);

   if ( NULL == pCS || NULL == pCI || NULL == pp )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structures.");
      return;
   }

   if ( pCS->iTelemetryOutputSerialPortIndex >= hardware_get_serial_ports_count() )
   {
      log_softerror_and_alarm("Controller is using invalid serial port %d for telemetry output. Currently available serial ports on controller: %d.", pCS->iTelemetryOutputSerialPortIndex+1, hardware_get_serial_ports_count());
      pCS->iTelemetryOutputSerialPortIndex = -1;
      save_ControllerSettings();
   }
   if ( pCS->iTelemetryInputSerialPortIndex >= hardware_get_serial_ports_count() )
   {
      log_softerror_and_alarm("Controller is using invalid serial port %d for telemetry input. Currently available serial ports on controller: %d.", pCS->iTelemetryInputSerialPortIndex+1, hardware_get_serial_ports_count());
      pCS->iTelemetryInputSerialPortIndex = -1;
      save_ControllerSettings();
   }

   m_pItemsSelect[1]->setSelectedIndex(0);
   if ( -1 == pCS->iTelemetryOutputSerialPortIndex && -1 == pCS->iTelemetryInputSerialPortIndex )
   {
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSelect[1]->setSelectedIndex(0);
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSelect[2]->setEnabled(false);
   }
   else if ( pCS->iTelemetryOutputSerialPortIndex >= 0 && pCS->iTelemetryInputSerialPortIndex >= 0 )
   {
      m_pItemsSelect[0]->setSelectedIndex(3);
      m_pItemsSelect[1]->setEnabled(true);
      m_pItemsSelect[2]->setEnabled(true);
      if ( pCS->iTelemetryOutputSerialPortIndex >= 0 )
      {
         m_pItemsSelect[1]->setSelectedIndex(pCS->iTelemetryOutputSerialPortIndex+1);
         m_pItemsSelect[2]->setEnabled(true);
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(pCS->iTelemetryOutputSerialPortIndex);
         for(int n=0; n<m_pItemsSelect[2]->getSelectionsCount(); n++ )
            if ( (NULL != pPortInfo) && (hardware_get_serial_baud_rates()[n] == pPortInfo->lPortSpeed) )
            {
               m_pItemsSelect[2]->setSelection(n);
               break;
            }
      }
   }
   else if ( pCS->iTelemetryOutputSerialPortIndex >= 0 )
   {
      m_pItemsSelect[0]->setSelectedIndex(1);
      m_pItemsSelect[1]->setEnabled(true);
      m_pItemsSelect[2]->setEnabled(true);
      if ( pCS->iTelemetryOutputSerialPortIndex >= 0 )
      {
         m_pItemsSelect[1]->setSelectedIndex(pCS->iTelemetryOutputSerialPortIndex+1);
         m_pItemsSelect[2]->setEnabled(true);
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(pCS->iTelemetryOutputSerialPortIndex);
         for(int n=0; n<m_pItemsSelect[2]->getSelectionsCount(); n++ )
            if ( (NULL != pPortInfo) && (hardware_get_serial_baud_rates()[n] == pPortInfo->lPortSpeed) )
            {
               m_pItemsSelect[2]->setSelection(n);
               break;
            }
      }
   }
   else if ( pCS->iTelemetryInputSerialPortIndex >= 0 )
   {
      m_pItemsSelect[0]->setSelectedIndex(2);
      m_pItemsSelect[1]->setEnabled(true);
      m_pItemsSelect[2]->setEnabled(true);
      if ( pCS->iTelemetryInputSerialPortIndex >= 0 )
      {
         m_pItemsSelect[1]->setSelectedIndex(pCS->iTelemetryInputSerialPortIndex+1);
         m_pItemsSelect[2]->setEnabled(true);
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(pCS->iTelemetryInputSerialPortIndex);
         for(int n=0; n<m_pItemsSelect[2]->getSelectionsCount(); n++ )
            if ( (NULL != pPortInfo) && (hardware_get_serial_baud_rates()[n] == pPortInfo->lPortSpeed) )
            {
               m_pItemsSelect[2]->setSelection(n);
               break;
            }
      }
   }


   m_pItemsSelect[3]->setSelectedIndex(pCS->iTelemetryForwardUSBType);

   m_pItemsRange[0]->setCurrentValue(pCS->iTelemetryForwardUSBPort);
   m_pItemsRange[1]->setCurrentValue(pCS->iTelemetryForwardUSBPacketSize);
   if ( 0 == pCS->iTelemetryForwardUSBType )
   {
      m_pItemsRange[0]->setEnabled(false);
      m_pItemsRange[1]->setEnabled(false);
   }
   else
   {
      m_pItemsRange[0]->setEnabled(true);
      m_pItemsRange[1]->setEnabled(true);
   }
}

void MenuControllerTelemetry::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuControllerTelemetry::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
   m_iConfirmationId = 0;
}

void MenuControllerTelemetry::onSelectItem()
{
   Menu::onSelectItem();

   ControllerSettings* pCS = get_ControllerSettings();
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   if ( NULL == pCS || NULL == pCI )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   Preferences* pp = get_Preferences();
   if ( NULL == pp )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( m_IndexInOut == m_SelectedIndex )
   {
      int idx = m_pItemsSelect[0]->getSelectedIndex();
      if ( 0 == idx )
      {
         pCS->iTelemetryOutputSerialPortIndex = -1;
         pCS->iTelemetryInputSerialPortIndex = -1;
      }
      if ( 1 == idx )
      {
         if ( pCS->iTelemetryOutputSerialPortIndex == -1 )
            pCS->iTelemetryOutputSerialPortIndex = 0;
         pCS->iTelemetryInputSerialPortIndex = -1;
      }
      if ( 2 == idx )
      {
         if ( pCS->iTelemetryInputSerialPortIndex == -1 )
            pCS->iTelemetryInputSerialPortIndex = 0;
         pCS->iTelemetryOutputSerialPortIndex = -1;
      }
      if ( 3 == idx )
      {
         if ( pCS->iTelemetryOutputSerialPortIndex == -1 )
            pCS->iTelemetryOutputSerialPortIndex = 0;
         if ( pCS->iTelemetryInputSerialPortIndex == -1 )
            pCS->iTelemetryInputSerialPortIndex = 0;
         if ( pCS->iTelemetryInputSerialPortIndex != pCS->iTelemetryOutputSerialPortIndex )
            pCS->iTelemetryInputSerialPortIndex = pCS->iTelemetryOutputSerialPortIndex;
      }

      int portIndex = pCS->iTelemetryOutputSerialPortIndex;
      if ( -1 == portIndex )
         portIndex = pCS->iTelemetryInputSerialPortIndex;

      for( int i=0; i<hardware_get_serial_ports_count(); i++ )
      {
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);
         if ( NULL == pPortInfo )
            continue;
         if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY )
            pPortInfo->iPortUsage = SERIAL_PORT_USAGE_NONE;
         if ( i == portIndex )
            pPortInfo->iPortUsage = SERIAL_PORT_USAGE_TELEMETRY;
      }

      hardware_serial_save_configuration();
      save_ControllerSettings();
      save_ControllerInterfacesSettings();

      u32 uParam = 0;
      if ( pCS->iTelemetryOutputSerialPortIndex >= 0 )
         uParam |= 0x01;

      if ( pCS->iTelemetryInputSerialPortIndex >= 0 )
         uParam |= 0x02;

      if ( NULL != g_pCurrentModel && pairing_isStarted() )
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CONTROLLER_TELEMETRY_OPTIONS, uParam, NULL, 0) )
         valuesToUI();    
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      valuesToUI();
   }

   if ( m_IndexSerialPort == m_SelectedIndex )
   {
      int idx = m_pItemsSelect[1]->getSelectedIndex();
      bool bTelemetryChanged = false;
      if ( 0 == idx )
      {
         for( int i=0; i<hardware_get_serial_ports_count(); i++ )
         {
            hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);
            if ( NULL == pPortInfo )
               continue;
            if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY )
               pPortInfo->iPortUsage = SERIAL_PORT_USAGE_NONE;
         }
         hardware_serial_save_configuration();
         save_ControllerSettings();
         save_ControllerInterfacesSettings();

         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
         valuesToUI();   
      }
      else
      {
         int portIndex = idx-1;
         
         for( int i=0; i<hardware_get_serial_ports_count(); i++ )
         {
            hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);
            if ( NULL == pPortInfo )
               continue;
            if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY )
               pPortInfo->iPortUsage = SERIAL_PORT_USAGE_NONE;
            if ( i == portIndex )
               pPortInfo->iPortUsage = SERIAL_PORT_USAGE_TELEMETRY;
         }
         hardware_serial_save_configuration();
         save_ControllerSettings();
         save_ControllerInterfacesSettings();

         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
         valuesToUI(); 
      }
   }


   if ( m_IndexSerialSpeed == m_SelectedIndex )
   if ( -1 != pCS->iTelemetryOutputSerialPortIndex || -1 != pCS->iTelemetryInputSerialPortIndex )
   {
      int portIndex = pCS->iTelemetryOutputSerialPortIndex;
      if ( -1 == portIndex )
         portIndex = pCS->iTelemetryInputSerialPortIndex;
      if ( portIndex >= hardware_get_serial_ports_count() )
         return;
      long val = hardware_get_serial_baud_rates()[m_pItemsSelect[2]->getSelectedIndex()];
      if ( portIndex >= 0 )
      {
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(portIndex);
            
         if ( (NULL != pPortInfo) && (val != pPortInfo->lPortSpeed) )
         {
            pPortInfo->lPortSpeed = val;
            hardware_serial_save_configuration();
            save_ControllerSettings();
            save_ControllerInterfacesSettings();
         }
      }
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      valuesToUI();
   }


   if ( m_IndexTelemetryUSBForward == m_SelectedIndex )
   {
      if ( pCS->iTelemetryForwardUSBType != m_pItemsSelect[3]->getSelectedIndex() )
      {
         pCS->iTelemetryForwardUSBType = m_pItemsSelect[3]->getSelectedIndex();
         save_ControllerSettings();
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
         invalidate();
         valuesToUI();
         if ( NULL != g_pCurrentModel )
            g_pCurrentModel->b_mustSyncFromVehicle = true;
      }
   }

   if ( m_IndexTelemetryUSBPort == m_SelectedIndex )
   {
      pCS->iTelemetryForwardUSBPort = m_pItemsRange[0]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
   }
   
   if ( m_IndexTelemetryUSBPacket == m_SelectedIndex )
   {
      pCS->iTelemetryForwardUSBPacketSize = m_pItemsRange[1]->getCurrentValue();
      save_ControllerSettings();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
      invalidate();
      valuesToUI();
   }
}
