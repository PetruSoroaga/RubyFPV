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
#include "menu_controller_peripherals.h"
#include "menu_controller_joystick.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"
#include "menu_item_legend.h"
#include "launchers_controller.h"
#include "menu_device_i2c.h"

#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/core_plugins_settings.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../common/string_utils.h"
#include "../public/ruby_core_plugin.h"
#include "../renderer/render_engine.h"

#include "pairing.h"
#include "colors.h"
#include "osd_common.h"
#include "timers.h"

#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "process_router_messages.h"
#include "ruby_central.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

const char* s_szMenuJoystickNotCalibrated = "Note: This input/joystick controller is not calibrated. Calibrate it first before using it.";

static bool s_bFirstTimeI2CDetectionMenuPeripheralsDone = false;

static int s_nMenuControllerI2CDevices[256];


MenuControllerPeripherals::MenuControllerPeripherals(void)
:Menu(MENU_ID_CONTROLLER_PERIPHERALS, "Controller Peripherals Settings", NULL)
{
   m_Width = 0.31;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.21;

   m_pItemWait = NULL;
   m_IndexWait = -1;
   m_nSearchI2CDeviceAddress = 0;
   m_bShownUSBWarning = false;
   char szBuff[64];

   addMenuItem(new MenuItemSection("Serial Ports"));

   for( int i=0; i<10; i++ )
   {
      m_IndexSerialType[i] = -1;
      m_IndexSerialSpeed[i] = -1;
      m_IndexJoysticks[i] = -1;
   }
   for( int i=0; i<20; i++ )
   {
      m_IndexI2CDevices[i] = -1;
   }


   hardware_i2c_load_device_settings();
   load_ControllerInterfacesSettings();
   load_ControllerSettings();
   ControllerSettings* pcs = get_ControllerSettings();
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   m_iSerialBuiltInOptionsCount = 0;

   if ( NULL == pcs || NULL == pCI )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }
   for( int i=0; i<hardware_get_serial_ports_count(); i++ )
   {
      hw_serial_port_info_t* pInfo = hardware_get_serial_port_info(i);
      if ( NULL == pInfo )
         continue;
      sprintf( szBuff, "%s Usage", pInfo->szName );
      m_pItemsSelect[10+i*2] = new MenuItemSelect(szBuff, "Enables this serial port on the controller for a particular use.");
      m_pItemsSelect[10+i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_NONE));
      m_pItemsSelect[10+i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_TELEMETRY));
      #ifdef FEATURE_MSP_OSD
      m_pItemsSelect[10+i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_MSP_OSD_PITLAB));
      #else
      m_pItemsSelect[10+i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_MSP_OSD_PITLAB), false);
      #endif
      m_pItemsSelect[10+i*2]->addSelection(str_get_serial_port_usage(SERIAL_PORT_USAGE_DATA_LINK));
      m_iSerialBuiltInOptionsCount = 3;
      
      for( int n=0; n<get_CorePluginsCount(); n++ )
      {
         char szOption[128];
         char* szName = get_CorePluginName(n);
         char* szGUID = get_CorePluginGUID(n);
         CorePluginSettings* pSettings = get_CorePluginSettings(szGUID);
         if ( NULL == pSettings || NULL == szName )
            continue;
         if ( pSettings->uRequestedCapabilities & CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART )
         {
            sprintf(szOption, "Core Plugin %s", szName);
            m_pItemsSelect[10+i*2]->addSelection(szOption);
         }
      }
      m_pItemsSelect[10+i*2]->setIsEditable();
      m_IndexSerialType[i] = addMenuItem(m_pItemsSelect[10+i*2]);

      sprintf( szBuff, "%s Baudrate", pInfo->szName );
      m_pItemsSelect[11+i*2] = new MenuItemSelect(szBuff, "Sets the baud rate of this serial port on the controller.");
      for( int n=0; n<hardware_get_serial_baud_rates_count(); n++ )
      {
         sprintf(szBuff, "%ld bps", hardware_get_serial_baud_rates()[n]);
         m_pItemsSelect[11+i*2]->addSelection(szBuff);
      }
      m_pItemsSelect[11+i*2]->setIsEditable();
      m_IndexSerialSpeed[i] = addMenuItem(m_pItemsSelect[11+i*2]);
   }

   addMenuItem(new MenuItemSection("Input Devices / Joysticks"));

   m_ExtraHeight = menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);

   controllerInterfacesEnumJoysticks();

   log_line("Input devices: %d", pCI->inputInterfacesCount);
   if ( 0 == pCI->inputInterfacesCount )
      addMenuItem( new MenuItemLegend("No joysticks, gamepads or RC transmitters detected.","", 0) );
   else
   {
      int nCountAdded = 0;
      for( int i=0; i<pCI->inputInterfacesCount; i++ )
      {
         t_ControllerInputInterface* pCII = controllerInterfacesGetAt(i);
         if ( NULL == pCII )
            continue;
         nCountAdded++;
         m_IndexJoysticks[i] = addMenuItem( new MenuItem(pCII->szInterfaceName) );
         m_pMenuItems[m_IndexJoysticks[i]]->showArrow();

         if ( ! pCII->bCalibrated )
            m_ExtraHeight += 1.4*g_pRenderEngine->getMessageHeight(s_szMenuJoystickNotCalibrated, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
      }
      if ( 0 == nCountAdded )
         addMenuItem( new MenuItemLegend("No joysticks, gamepads or RC transmitters detected.","", 0) );
   }

   addMenuItem(new MenuItemSection("I2C Devices"));

   if ( ! s_bFirstTimeI2CDetectionMenuPeripheralsDone )
   {
      for( int i=0; i<=255; i++ )
         s_nMenuControllerI2CDevices[i] = -1;
      m_nSearchI2CDeviceAddress = 0;
      m_pItemWait = new MenuItemText("Enumerating I2C devices. Please wait...");
      m_IndexWait = addMenuItem(m_pItemWait);
   }
   else
      addI2CDevices();
}

void MenuControllerPeripherals::valuesToUI()
{
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   char szBuff[1024];

   if ( NULL == pCI )
   {
      log_softerror_and_alarm("Failed to get pointer to controller interfaces");
      return;
   }

   for( int i=0; i<hardware_get_serial_ports_count(); i++ )
   {  
      hw_serial_port_info_t* pInfo = hardware_get_serial_port_info(i);
      if ( NULL == pInfo )
         continue;
      if ( pInfo->iPortUsage < m_iSerialBuiltInOptionsCount )
      {
         if ( pInfo->iPortUsage == SERIAL_PORT_USAGE_NONE )
            m_pItemsSelect[10+i*2]->setSelectedIndex(0);
         if ( pInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY )
            m_pItemsSelect[10+i*2]->setSelectedIndex(1);
         if ( pInfo->iPortUsage == SERIAL_PORT_USAGE_MSP_OSD_PITLAB )
            m_pItemsSelect[10+i*2]->setSelectedIndex(2);
         if ( pInfo->iPortUsage == SERIAL_PORT_USAGE_DATA_LINK )
            m_pItemsSelect[10+i*2]->setSelectedIndex(3);
      }
      else if ( pInfo->iPortUsage - 20 > get_CorePluginsCount() )
         m_pItemsSelect[10+i*2]->setSelectedIndex(0);
      else
      {
         int iCountUARTPlugins = 0;
         int iTargetPluginIndex = pInfo->iPortUsage - 20;
         for( int n=0; n<get_CorePluginsCount(); n++ )
         {
            char* szGUID = get_CorePluginGUID(n);
            CorePluginSettings* pSettings = get_CorePluginSettings(szGUID);
            if ( NULL == pSettings )
               continue;
            if ( pSettings->uRequestedCapabilities & CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART )
            {
               if ( iTargetPluginIndex == n )
               {
                  m_pItemsSelect[10+i*2]->setSelectedIndex(m_iSerialBuiltInOptionsCount + iCountUARTPlugins);
                  break;
               }
               iCountUARTPlugins++;
            }
         }
      }

      bool bFoundSpeed = false;
      for(int n=0; n<m_pItemsSelect[11+i*2]->getSelectionsCount(); n++ )
      {
         if ( hardware_get_serial_baud_rates()[n] == pInfo->lPortSpeed )
         {
            m_pItemsSelect[11+i*2]->setSelection(n);
            bFoundSpeed = true;
            break;
         }
      }
      if ( ! bFoundSpeed )
      {
         m_pItemsSelect[11+i*2]->setSelection(0);
         sprintf(szBuff, "Info: You are using a custom telemetry baud rate (%ld) on serial port %s.", pInfo->lPortSpeed, pInfo->szName);
         addTopLine(szBuff);
      }
   }
}

void MenuControllerPeripherals::onShow()
{
   Menu::onShow();
}

bool MenuControllerPeripherals::periodicLoop()
{
   if ( (!m_bAnimating) && g_TimeNow > getOnShowTime() + 400 )
   if ( ! m_bShownUSBWarning )
   {
      ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
      if ( NULL == pCI )
      {
         log_softerror_and_alarm("Failed to get pointer to controller settings structure");
         return false;
      }
      for( int i=0; i<hardware_get_serial_ports_count(); i++ )
      {
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);

         if ( (NULL != pPortInfo) && (! pPortInfo->iSupported) )
         {
           m_iConfirmationId = 1;
           char szError[1024];
           sprintf(szError, "Your USB to Serial adapter on port %s is not compatible. Use brand name adapters or ones with CP2102 chipset. The ones with 340 chipset are not compatible.", pPortInfo->szName);
           MenuConfirmation* pMC = new MenuConfirmation("Unsupported USB to Serial Adapter", szError, m_iConfirmationId, true);
           pMC->m_yPos = 0.3;
           add_menu_to_stack(pMC);
           m_bShownUSBWarning = true;
           break;
         }
      }
   }

   if ( s_bFirstTimeI2CDetectionMenuPeripheralsDone )
      return false;

   if ( NULL == m_pItemWait || m_nSearchI2CDeviceAddress == 128 )
      return false;
   if ( m_bAnimating )
      return false;
   if ( m_nSearchI2CDeviceAddress < 0 )
   {
      m_nSearchI2CDeviceAddress++;
      return false;
   }

   // i2cdetect -y 1 | tr '\n' ' ' | sed -e 's/[^0-9a-fA-F]/ /g' -e 's/^ *//g' -e 's/ *$//g' | tr -s ' ' | sed $'s/ /\\\n/g'

   for( int i=0; i<10; i++ )
   {
      if ( m_nSearchI2CDeviceAddress >= 128 )
         break;
      if ( hardware_is_known_i2c_device(m_nSearchI2CDeviceAddress) )
      if ( hardware_has_i2c_device_id(m_nSearchI2CDeviceAddress) )
      {
         s_nMenuControllerI2CDevices[m_nSearchI2CDeviceAddress] = m_nSearchI2CDeviceAddress;
         hardware_i2c_get_device_settings((u8)m_nSearchI2CDeviceAddress);
      }
      m_nSearchI2CDeviceAddress++;
   }

   if ( NULL != m_pItemWait )
   {
      char szBuff[128];
      int percent = m_nSearchI2CDeviceAddress*100/128;
      if ( percent > 100 ) percent = 100;
      sprintf(szBuff, "Enumerating I2C devices %d%%. Please wait...", percent);
      m_pItemWait->setTitle(szBuff);
      invalidate();     
   }
   if ( m_nSearchI2CDeviceAddress >= 128 )
   {
      hardware_i2c_save_device_settings();
      m_ItemsCount--;
      addI2CDevices();
      invalidate();
      s_bFirstTimeI2CDetectionMenuPeripheralsDone = true;
   }
   return true;
}

void MenuControllerPeripherals::addI2CDevices()
{
   int countAdded = 0;
   for( int i=0; i<128; i++ )
   {
      if ( s_nMenuControllerI2CDevices[i] <= 0 )
         continue;
      t_i2c_device_settings* pInfo = hardware_i2c_get_device_settings((u8)s_nMenuControllerI2CDevices[i]);
      if ( NULL == pInfo ) 
         continue;
      if ( i == I2C_DEVICE_ADDRESS_PICO_EXTENDER )
      {
         log_line("Pico Extender version: %d.%d (%d)", (pInfo->nVersion)>>4, (pInfo->nVersion) & 0x0F, pInfo->nVersion );
         char szBuff[128];
         sprintf(szBuff, "%s %d.%d", pInfo->szDeviceName, (pInfo->nVersion)>>4, (pInfo->nVersion) & 0x0F);
         m_IndexI2CDevices[countAdded] = addMenuItem( new MenuItem(szBuff, "Configure this I2C device.") );
      }
      else
         m_IndexI2CDevices[countAdded] = addMenuItem( new MenuItem(pInfo->szDeviceName, "Configure this I2C device.") );
      countAdded++;
   }
   if ( 0 == countAdded )
      addMenuItem(new MenuItemText("No I2C devices found."));
}

void MenuControllerPeripherals::Render()
{
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);

      for( int j=0; j<pCI->inputInterfacesCount; j++ )
      {
         t_ControllerInputInterface* pCII = controllerInterfacesGetAt(j);
         if ( i == m_IndexJoysticks[j] && (!pCII->bCalibrated ) )
         {
            g_pRenderEngine->setColors(get_Color_MenuText());
            y += 0.3 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING);
            y += g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus() + 0.01*menu_getScaleMenus(), y, s_szMenuJoystickNotCalibrated, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
            y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING);
         }
      }
   }

   RenderEnd(yTop);
}

void MenuControllerPeripherals::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

void MenuControllerPeripherals::onSelectItem()
{
   Menu::onSelectItem();
   ControllerSettings* pCS = get_ControllerSettings();
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   if ( NULL == pCS || NULL == pCI)
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   for( int i=0; i<hardware_get_serial_ports_count(); i++ )
   {
      hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);
      if ( NULL == pPortInfo )
         continue;

      if ( m_IndexSerialType[i] == m_SelectedIndex && (! m_pMenuItems[m_SelectedIndex]->isEditing()) )
      {
         int newUsage = -1;

         if ( m_pItemsSelect[10+i*2]->getSelectedIndex() < m_iSerialBuiltInOptionsCount )
         {
            if ( 0 == m_pItemsSelect[10+i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_NONE;
            if ( 1 == m_pItemsSelect[10+i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_TELEMETRY;
            if ( 2 == m_pItemsSelect[10+i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_MSP_OSD_PITLAB;
            if ( 3 == m_pItemsSelect[10+i*2]->getSelectedIndex() )
               newUsage = SERIAL_PORT_USAGE_DATA_LINK;
            if ( newUsage == pPortInfo->iPortUsage )
               return;
         }
         else
         {
            int iCountUARTPlugins = 0;
            for( int n=0; n<get_CorePluginsCount(); n++ )
            {
               char* szGUID = get_CorePluginGUID(n);
               CorePluginSettings* pSettings = get_CorePluginSettings(szGUID);
               if ( NULL == pSettings )
                  continue;
               if ( pSettings->uRequestedCapabilities & CORE_PLUGIN_CAPABILITY_HARDWARE_ACCESS_UART )
               {
                  if ( (m_pItemsSelect[10+i*2]->getSelectedIndex() - m_iSerialBuiltInOptionsCount) == iCountUARTPlugins )
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
         if ( newUsage == pPortInfo->iPortUsage )
            return;

         bool bTelemetryChanged = false;
         bool bHadTelemetryOutput = false;
         bool bHadTelemetryInput = false;
         if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY)
         {
            if ( pCS->iTelemetryOutputSerialPortIndex != -1 )
               bHadTelemetryOutput = true;
            if ( pCS->iTelemetryInputSerialPortIndex != -1 )
               bHadTelemetryInput = true;
            pCS->iTelemetryOutputSerialPortIndex = -1;
            pCS->iTelemetryInputSerialPortIndex = -1;
            bTelemetryChanged = true;
         }

         pPortInfo->iPortUsage = newUsage;

         // Allow only one port for telemetry
         int iCountPortsTelemetry = 0;
         for( int n=0; n<hardware_get_serial_ports_count(); n++ )
         {
            hw_serial_port_info_t* pInfo = hardware_get_serial_port_info(n);
            if ( NULL == pInfo )
               continue;
            if ( pInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY )
               iCountPortsTelemetry++;
         }

         if ( iCountPortsTelemetry > 1 )
            addMessage("You have multiple serial ports assigned to telemetry. This will be an issue.");

         // Allow only one port for a specific usage

         for( int k=0; k<hardware_get_serial_ports_count(); k++ )
         {
            hw_serial_port_info_t* pInfo = hardware_get_serial_port_info(k);
            if ( NULL == pInfo )
               continue;
            if ( pInfo->iPortUsage == newUsage )
            if ( k != i )
            {
               char szBuff[128];
               sprintf(szBuff, "You have multiple serial ports assigned to %s. This will be an issue.", str_get_serial_port_usage(newUsage));
               addMessage(szBuff);
               break;
            }
         }

         
         if ( newUsage == SERIAL_PORT_USAGE_TELEMETRY )
         {
            pCS->iTelemetryOutputSerialPortIndex = i;
            pCS->iTelemetryInputSerialPortIndex = i;
            bTelemetryChanged = true;
         }

         hardware_serial_save_configuration();
         save_ControllerSettings();
         save_ControllerInterfacesSettings();
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);

         valuesToUI();
         if ( bTelemetryChanged && (NULL != g_pCurrentModel) )
         {
            u32 uParam = 0;
            if ( pCS->iTelemetryOutputSerialPortIndex >= 0 )
               uParam |= 0x01;

            if ( pCS->iTelemetryInputSerialPortIndex >= 0 )
               uParam |= 0x02;

            if ( pairing_isStarted() )
            if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CONTROLLER_TELEMETRY_OPTIONS, uParam, NULL, 0) )
               valuesToUI();
         }
         /* if ( newVal == serialPortUsageRCInIBus )
         {
            Popup* p = new Popup(true, "IBUS protocol not supported", 4 );
            p->setIconId(g_idIconWarning, get_Color_IconWarning());
            popups_add_topmost(p);
         }
         if ( newVal == serialPortUsageRCInSBus )
         {
            Popup* p = new Popup(true, "SBUS protocol not supported", 4 );
            p->setIconId(g_idIconWarning, get_Color_IconWarning());
            popups_add_topmost(p);
         } */
         return;
      } 

      if ( m_IndexSerialSpeed[i] == m_SelectedIndex && (! m_pMenuItems[m_SelectedIndex]->isEditing()) )
      {
         long val = hardware_get_serial_baud_rates()[m_pItemsSelect[11+i*2]->getSelectedIndex()];
         if ( val != pPortInfo->lPortSpeed )
         {
            pPortInfo->lPortSpeed = val;
            hardware_serial_save_configuration();
            save_ControllerInterfacesSettings();
            send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);
         }
         valuesToUI();
         return;
      }
   }

   for( int j=0; j<pCI->inputInterfacesCount; j++ )
   {
      t_ControllerInputInterface* pCII = controllerInterfacesGetAt(j);
      if ( m_SelectedIndex == m_IndexJoysticks[j] )
      {
         add_menu_to_stack(new MenuControllerJoystick(j));
         return;
      }
   }

   int devIndex = 0;
   for( int i=0; i<128; i++ )
   {
      if ( s_nMenuControllerI2CDevices[i] <= 0 )
         continue;
      t_i2c_device_settings* pInfo = hardware_i2c_get_device_settings((u8)s_nMenuControllerI2CDevices[i]);
      if ( NULL == pInfo ) 
         continue;

      if ( m_IndexI2CDevices[devIndex] == m_SelectedIndex && (! m_pMenuItems[m_SelectedIndex]->isEditing()) )
      {
         MenuDeviceI2C* pMenu = new MenuDeviceI2C();
         pMenu->setDeviceId(pInfo->nI2CAddress);
         char szBuff[128];
         sprintf(szBuff, "Configure %s I2C Device", pInfo->szDeviceName);
         add_menu_to_stack(pMenu);
         return;
      }
      devIndex++;
   }
}
