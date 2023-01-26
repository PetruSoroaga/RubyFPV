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
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "menu.h"
#include "menu_objects.h"
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_system.h"
#include "menu_system_expert.h"
#include "menu_confirmation.h"
#include "menu_system_all_params.h"
#include "menu_system_hardware.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/controller_utils.h"

#include "menu_system_dev_logs.h"
#include "menu_system_alarms.h"
#include "pairing.h"

#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "process_router_messages.h"
#include "ruby_central.h"
#include "events.h"


#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>


MenuSystem::MenuSystem(void)
:Menu(MENU_ID_SYSTEM, "System Info", NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.24;
   m_ExtraHeight += 0.5*menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS*(1+MENU_TEXTLINE_SPACING);

   m_IndexAlarms = addMenuItem( new MenuItem("Alarms Settings") );
   m_pMenuItems[m_IndexAlarms]->showArrow();

   m_IndexLogs = addMenuItem( new MenuItem("Logs Settings") );
   m_pMenuItems[m_IndexLogs]->showArrow();

   m_IndexAllParams = addMenuItem(new MenuItem("View All Parameters", "View all controller and vehicle parameters at once, in a single screen."));
   m_pMenuItems[m_IndexAllParams]->showArrow();

   m_IndexDevices = addMenuItem(new MenuItem("View All Devices and Peripherals", "Displays all the devices and peripherals attached to the controller and to the current vehicle."));
   m_pMenuItems[m_IndexDevices]->showArrow();

   m_IndexExport = addMenuItem(new MenuItem("Export All Settings", "Exports all vehicles, controller settings and preferences to a USB memory stick for back-up purposes."));
   m_IndexImport = addMenuItem(new MenuItem("Import All Settings", "Import all vehicles, controller settings and preferences from a USB memory stick."));

   m_pItemsSelect[1] = new MenuItemSelect("Auto Export Settings", "Automatically periodically export all settings and models to a USB memory stick, if one is found. All your settings will be synchronized to a USB memory stick for later import (after a factory reset for example).");
   m_pItemsSelect[1]->addSelection("Off");
   m_pItemsSelect[1]->addSelection("On");
   m_pItemsSelect[1]->setUseMultiViewLayout();
   m_IndexAutoExport = addMenuItem(m_pItemsSelect[1]);

   m_IndexReset = addMenuItem(new MenuItem("Factory Reset", "Resets all the settings an files on the controller, as they where when the image was flashed."));

   m_pItemsSelect[0] = new MenuItemSelect("Enable Developer Mode", "Used to debug issues. Disables fail safe checks, parameters consistency checks and other options. It's recommended to leave this [Off] as it will degrade your system performance.");
   m_pItemsSelect[0]->addSelection("Off");
   m_pItemsSelect[0]->addSelection("On");
   m_pItemsSelect[0]->setUseMultiViewLayout();
   m_IndexDeveloper = addMenuItem(m_pItemsSelect[0]);

   m_IndexExpert = addMenuItem(new MenuItem("Developer Settings", "Changes expert system settings. For debuging only."));
   m_pMenuItems[m_IndexExpert]->showArrow();
}

void MenuSystem::valuesToUI()
{
   m_ExtraHeight = menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);

   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pP = get_Preferences();

   m_pItemsSelect[0]->setSelection(0);
   if ( NULL != pCS && (0 != pCS->iDeveloperMode) )
      m_pItemsSelect[0]->setSelection(1);

   m_pItemsSelect[1]->setSelectedIndex(pP->iAutoExportSettings);

   m_pMenuItems[m_IndexExpert]->setEnabled(pCS->iDeveloperMode);
}

void MenuSystem::onShow()
{
   Menu::onShow();
}


void MenuSystem::Render()
{
   RenderPrepare();
   float yEnd = RenderFrameAndTitle();
   float y = yEnd;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yEnd);
}

void MenuSystem::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   if ( 1 != returnValue )
      return;

   if ( 1 == m_iConfirmationId )
   {
      onEventReboot();
      hw_execute_bash_command("rm -rf /home/pi/ruby/logs", NULL);
      hw_execute_bash_command("rm -rf /home/pi/ruby/media", NULL);
      hw_execute_bash_command("rm -rf /home/pi/ruby/config", NULL);
      hw_execute_bash_command("rm -rf /home/pi/ruby/tmp", NULL);
      hw_execute_bash_command("mkdir -p config", NULL);
      hw_execute_bash_command("touch /home/pi/ruby/config/firstboot.txt", NULL);
      char szBuff[128];
      sprintf(szBuff, "touch %s", LOG_USE_PROCESS);
      hw_execute_bash_command(szBuff, NULL);

      hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }

   if ( 10 == m_iConfirmationId )
   {
      m_iConfirmationId = 0;
      ruby_signal_alive();
      ruby_pause_watchdog();
      int nReturn = controller_utils_export_all_to_usb();
      ruby_resume_watchdog();
      ruby_signal_alive();

      if ( nReturn == -1 )
      {
         addMessage("ZIP program is missing!");
         return;
      }
      else if ( nReturn == -2 )
      {
         addMessage("No USB memory stick detected. Please insert a USB stick to export to.");
         return;
      }
      else if ( nReturn < 0 )
      {
         addMessage("Something failed during export!");
         return;
      }
      ruby_signal_alive();
      
      ruby_resume_watchdog();
      addMessage("Done. All configuration files have been successfully exported. You can now remove the USB memory stick.");
      return;
   }

   if ( 11 == m_iConfirmationId )
   {
      m_iConfirmationId = 0;
      ruby_signal_alive();
      ruby_pause_watchdog();
      pairing_stop();
      int nReturn = controller_utils_import_all_from_usb(false);
      ruby_resume_watchdog();
      ruby_signal_alive();
      if ( nReturn == -1 )
      {
         addMessage("ZIP program is missing!");
         return;
      }
      else if ( nReturn == -2 )
      {
         addMessage("No USB memory stick detected. Please insert a USB stick to import from.");
         return;
      }
      else if ( nReturn == -3 )
      {
         addMessage("No export found for this controller Id on the memory stick.");
         return;
      }
      else if ( nReturn < 0 )
      {
         addMessage("Something failed during import!");
         return;
      }
      ruby_signal_alive();

      if ( nReturn >= 0 )
      {
         ruby_load_models();

         if ( ! load_Preferences() )
            save_Preferences();

         if ( ! load_ControllerSettings() )
            save_ControllerSettings();

         if ( ! load_ControllerInterfacesSettings() )
            save_ControllerInterfacesSettings();
   

         m_iConfirmationId = 12;
         MenuConfirmation* pMC = new MenuConfirmation("Import Succeeded","All configuration files have been successfully imported. You can now remove the USB memory stick.", m_iConfirmationId, true);
         pMC->addTopLine(" ");
         pMC->addTopLine("The controller will reboot now.");
         add_menu_to_stack(pMC);
      }
      return;
   }

   if ( 12 == m_iConfirmationId )
   {
      hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }
   m_iConfirmationId = 0;
}



void MenuSystem::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_IndexAlarms == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuSystemAlarms());
      return;
   }

   if ( m_IndexAllParams == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuSystemAllParams());
      return;
   }

   if ( m_IndexDevices == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuSystemHardware());
      return;
   }

   if ( -1 != m_IndexLogs )
   if ( m_IndexLogs == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuSystemDevLogs());
      return;
   }

   if ( m_IndexExport == m_SelectedIndex )
   {
      m_iConfirmationId = 10;
      MenuConfirmation* pMC = new MenuConfirmation("Export all settings","Insert a USB memory stick and then press Yes to export everything to the memory stick.", m_iConfirmationId);
      pMC->addTopLine("Proceed?");
      add_menu_to_stack(pMC);
   }

   if ( m_IndexImport == m_SelectedIndex )
   {
      m_iConfirmationId = 11;
      MenuConfirmation* pMC = new MenuConfirmation("Import","Insert a USB memory stick and then press Yes to import everything from the memory stick.", m_iConfirmationId);
      pMC->addTopLine(" ");
      pMC->addTopLine("Warning: This will overwrite all your vehicles memory, vehicle settings, controller settings and preferences!");
      pMC->addTopLine("Proceed?");
      add_menu_to_stack(pMC);
   }

   if ( m_IndexAutoExport == m_SelectedIndex )
   {
      Preferences* pP = get_Preferences();
      pP->iAutoExportSettings = m_pItemsSelect[1]->getSelectedIndex();
      pP->iAutoExportSettingsWasModified = 1;
      save_Preferences();
   }

   if ( m_IndexDeveloper == m_SelectedIndex && (! m_pMenuItems[m_IndexDeveloper]->isEditing()) )
   {
      int val = m_pItemsSelect[0]->getSelectedIndex();
      ControllerSettings* pCS = get_ControllerSettings();

      if ( val != pCS->iDeveloperMode )
      {
         pCS->iDeveloperMode = val;
         save_ControllerSettings();
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);      
         if ( NULL != g_pCurrentModel )
            g_pCurrentModel->b_mustSyncFromVehicle = true;
         valuesToUI();
      }
      return;
   }

   if ( m_IndexExpert == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuSystemExpert());
      return;
   }

   if ( m_IndexReset == m_SelectedIndex )
   {
      m_iConfirmationId = 1;
      MenuConfirmation* pMC = new MenuConfirmation("Factory Reset Controller","Are you sure you want to reset the controller to default settings?", m_iConfirmationId, false);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }
}

