/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
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
#include "menu_txinfo.h"
#include "menu_system.h"
#include "menu_system_expert.h"
#include "menu_confirmation.h"
#include "menu_system_all_params.h"
#include "menu_system_hardware.h"
#include "../../base/controller_utils.h"
#include "menu_system_dev_logs.h"
#include "menu_system_alarms.h"
#include "../osd/osd_common.h"
#include "../process_router_messages.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

extern u32 g_idIconOpenIPC;


MenuSystem::MenuSystem(void)
:Menu(MENU_ID_SYSTEM, "System Info", NULL)
{
   m_Width = 0.52;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.24;
   
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

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 2.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();
   g_pRenderEngine->drawIcon(m_RenderXPos + m_RenderWidth - m_sfMenuPaddingX - iconWidth - 0.16, yEnd - iconHeight - 8.5*g_pRenderEngine->textHeight(g_idFontMenu), iconWidth, iconHeight, g_idIconRuby);
   g_pRenderEngine->drawIcon(m_RenderXPos + m_RenderWidth - m_sfMenuPaddingX - iconWidth - 0.16, yEnd - iconHeight - 5.5*g_pRenderEngine->textHeight(g_idFontMenu), iconWidth, iconHeight, g_idIconOpenIPC);

   RenderEnd(yEnd);
}

void MenuSystem::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( 1 != returnValue )
      return;

   if ( 1 == iChildMenuId/1000 )
   {
      onEventReboot();
      hw_execute_bash_command("rm -rf /home/pi/ruby/logs", NULL);
      hw_execute_bash_command("rm -rf /home/pi/ruby/media", NULL);
      hw_execute_bash_command("rm -rf /home/pi/ruby/config", NULL);
      hw_execute_bash_command("rm -rf /home/pi/ruby/tmp", NULL);
      hw_execute_bash_command("mkdir -p config", NULL);
      hw_execute_bash_command("touch /home/pi/ruby/config/firstboot.txt", NULL);
      char szBuff[128];
      sprintf(szBuff, "touch %s%s", FOLDER_CONFIG, LOG_USE_PROCESS);
      hw_execute_bash_command(szBuff, NULL);

      hardware_reboot();
      return;
   }

   if ( 10 == iChildMenuId/1000 )
   {
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

   if ( 11 == iChildMenuId/1000 )
   {
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
   

         MenuConfirmation* pMC = new MenuConfirmation("Import Succeeded","All configuration files have been successfully imported. You can now remove the USB memory stick.", 12, true);
         pMC->addTopLine(" ");
         pMC->addTopLine("The controller will reboot now.");
         add_menu_to_stack(pMC);
      }
      return;
   }

   if ( 12 == iChildMenuId/1000 )
   {
      hardware_reboot();
      return;
   }
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
      MenuConfirmation* pMC = new MenuConfirmation("Export all settings","Insert a USB memory stick and then press Yes to export everything to the memory stick.", 10);
      pMC->addTopLine("Proceed?");
      add_menu_to_stack(pMC);
   }

   if ( m_IndexImport == m_SelectedIndex )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Import","Insert a USB memory stick and then press Yes to import everything from the memory stick.", 11);
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
         log_line("MenuSystem: Changed developer mode flag to: %s", val?"true":"false");
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
      MenuConfirmation* pMC = new MenuConfirmation("Factory Reset Controller","Are you sure you want to reset the controller to default settings?", 1, false);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }
}

