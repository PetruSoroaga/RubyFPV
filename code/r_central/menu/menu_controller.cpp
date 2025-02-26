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
#include "menu_objects.h"
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "menu_controller_expert.h"
#include "menu_controller_peripherals.h"
#include "menu_controller_video.h"
#include "menu_controller_telemetry.h"
#include "menu_controller_network.h"
#include "menu_controller_plugins.h"
#include "menu_controller_encryption.h"
#include "menu_controller_recording.h"
#include "menu_controller_dev.h"
#include "menu_preferences_buttons.h"
#include "menu_preferences_ui.h"
#include "menu_preferences.h"
#include "menu_controller_radio.h"
#include "../../base/ctrl_settings.h"

#include <time.h>
#include <sys/resource.h>

MenuController::MenuController(void)
:Menu(MENU_ID_CONTROLLER, "Controller Settings", NULL)
{
   m_Width = 0.18;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;

   m_bShownHDMIChangeNotif = false;
   m_bWaitingForUserFinishUpdateConfirmation = false;
   m_iMustStartUpdate = 0;
}

void MenuController::onShow()
{
   int iTmp = getSelectedMenuItemIndex();
   removeAllItems();

   m_IndexVideo = addMenuItem(new MenuItem("Audio & Video Output", "Change Audio and Video Output Settings (HDMI, USB Tethering, Audio output device)"));
   //m_pMenuItems[m_IndexVideo]->showArrow();

   m_IndexTelemetry = addMenuItem(new MenuItem("Telemetry Input/Output", "Change the Telemetry Input/Output settings on the controller ports."));
   //m_pMenuItems[m_IndexTelemetry]->showArrow();

   m_IndexRadio = addMenuItem(new MenuItem("Radio", "Configure the radio interfaces on the controller."));
   //m_pMenuItems[m_IndexTelemetry]->showArrow();

   m_IndexCPU = addMenuItem(new MenuItem("CPU and Processes", "Set CPU Overclocking, Processes Priorities"));
   //m_pMenuItems[m_IndexCPU]->showArrow();

   m_IndexPorts = addMenuItem(new MenuItem("Peripherals / Ports", "Change controller peripherals settings (serial ports, USB devices, HID, I2C devices, etc)"));
   //m_pMenuItems[m_IndexPorts]->showArrow();

   //m_pItemsSelect[1] = new MenuItemSelect("Show CPU Info in OSD", "Shows Controller CPU information (load, frequency, temperature) on the OSD, near the vehicle CPU info, when using OSD Full Layout.");
   //m_pItemsSelect[1]->addSelection("No");
   //m_pItemsSelect[1]->addSelection("Yes");
   //m_pItemsSelect[1]->setUseMultiViewLayout();
   //m_IndexShowCPUInfo = addMenuItem(m_pItemsSelect[1]);

   //m_pItemsSelect[2] = new MenuItemSelect("Show Voltage/Current in OSD", "Shows Controller voltage/current information on the OSD, near the vehicle CPU info, when using OSD Full Layout.");
   //m_pItemsSelect[2]->addSelection("No");
   //m_pItemsSelect[2]->addSelection("Yes");
   //m_pItemsSelect[2]->setUseMultiViewLayout();
   //m_IndexShowVoltage = addMenuItem(m_pItemsSelect[2]);
   
   m_IndexNetwork = addMenuItem(new MenuItem("Local Network Settings", "Change the local network settings on the controller (DHCP/Fixed IP)"));
   //m_pMenuItems[m_IndexNetwork]->showArrow();

   m_IndexEncryption = -1;
   //m_IndexEncryption = addMenuItem(new MenuItem("Encryption", "Change the encryption global settings"));
   //m_pMenuItems[m_IndexEncryption]->showArrow();
   //m_pMenuItems[m_IndexEncryption]->setEnabled(false);

   m_IndexButtons = addMenuItem(new MenuItem("Buttons", "Change buttons actions."));
   m_IndexPreferences = -1;
   //m_IndexPreferences = addMenuItem(new MenuItem("Preferences", "Change preferences about messages."));
   m_IndexPreferencesUI = addMenuItem(new MenuItem("User Interface", "Change user interface preferences."));

   m_IndexRecording = addMenuItem(new MenuItem("Recording Settings", "Change the recording settings"));
   //m_pMenuItems[m_IndexRecording]->showArrow();

   ControllerSettings* pCS = get_ControllerSettings();
   m_IndexDeveloper = -1;
   if ( (NULL != pCS) && pCS->iDeveloperMode )
   {
      m_IndexDeveloper = addMenuItem( new MenuItem("Developer Settings") );
      m_pMenuItems[m_IndexDeveloper]->showArrow();
      m_pMenuItems[m_IndexDeveloper]->setTextColor(get_Color_Dev());
   }

   addMenuItem(new MenuItemSection("Management"));

   m_IndexPlugins = addMenuItem(new MenuItem("Manage Plugins", "Configure, add and remove controller software plugins."));
   m_IndexUpdate = addMenuItem(new MenuItem("Update Software", "Updates software on this controller using a USB memory stick."));
   m_IndexReboot = addMenuItem(new MenuItem("Restart", "Restarts the controller."));
   
   Menu::onShow();

   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex < 0 )
      m_SelectedIndex = 0;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuController::valuesToUI()
{
   //ControllerSettings* pCS = get_ControllerSettings();
   //Preferences* pp = get_Preferences();
   //m_pItemsSelect[1]->setSelection(pp->iShowControllerCPUInfo);
   /*
   if ( hardware_i2c_has_current_sensor() )
   {
      m_pItemsSelect[2]->setSelection(pCS->iShowVoltage);
      m_pItemsSelect[2]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[2]->setSelection(0);
      m_pItemsSelect[2]->setEnabled(false);
   }
   */
}

void MenuController::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

bool MenuController::periodicLoop()
{
   if ( m_bWaitingForUserFinishUpdateConfirmation )
      log_line("Waiting for user confirmation to complete the update procedure...");

   if ( m_iMustStartUpdate > 0 )
   {
      m_iMustStartUpdate--;
      if ( m_iMustStartUpdate == 0 )
      {
         updateSoftware();
         return true;
      }
   }
   return false;
}

void MenuController::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   log_line("MenuController: Returned from child id: %d, return value: %d", iChildMenuId, returnValue);
   if ( (1 == iChildMenuId/1000) && (1 == returnValue) )
   {
      m_iMustStartUpdate = 1;
      log_line("Signaled event to start controller update.");
      return;
   }
   if ( 3 == iChildMenuId/1000 )
   {
      m_bWaitingForUserFinishUpdateConfirmation = false;
      log_line("Closed message update. Will reboot now.");
      menu_discard_all();
      popups_remove_all();
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();

      onEventReboot();
      hardware_reboot();
      return;
   }

   if ( 5 == iChildMenuId/1000 ) // Update failed.
   {
      log_line("Not waiting for user confirmation anymore");
      m_bWaitingForUserFinishUpdateConfirmation = false;

      //log_line("Closed message update. Will reboot now.");
      //onEventReboot();
      //hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }

   if ( (10 == iChildMenuId/1000) && (1 == returnValue) )
   {
      onEventReboot();
      hardware_reboot();
      return;
   }

   if ( (11 == iChildMenuId/1000) && (1 == returnValue) )
   {
      onEventReboot();
      hardware_reboot();
      return;
   }

   log_line("Add HDMI changed confirmation dialog.");
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_TEMP_HDMI_CHANGED);
   if ( ! m_bShownHDMIChangeNotif )
   if ( access(szFile, R_OK) != -1 )
   {
      m_bShownHDMIChangeNotif = true;
      MenuConfirmation* pMC = new MenuConfirmation("HDMI Output Configuration Changed","You updated the HDMI output configuration.", 11);
      pMC->addTopLine("If the new resolution is not supported by your display, Ruby will reboot again with the old display configuration.");
      pMC->addTopLine("Do you want to reboot now for the new changes to take effect?");
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }
}

void MenuController::onSelectItem()
{
   Menu::onSelectItem();
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
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

   if ( m_IndexNetwork == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerNetwork());
      return;
   }
  
   if ( m_IndexCPU == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerExpert());
      return;
   }

   if ( m_IndexPorts == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerPeripherals());
      return;
   }

   if ( m_IndexVideo == m_SelectedIndex )
   {
      m_bShownHDMIChangeNotif = false;
      add_menu_to_stack(new MenuControllerVideo());
      return;
   }

   if ( m_IndexTelemetry == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerTelemetry());
      return;
   }

   if ( m_IndexRadio == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerRadio());
      return;
   }

   if ( m_IndexRecording == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerRecording());
      return;
   }

   if ( m_IndexPlugins == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerPlugins());
      return;
   }

   if ( (-1 != m_IndexEncryption) && (m_IndexEncryption == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuControllerEncryption());
      return;
   }

   if ( m_IndexButtons == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuButtons()); 
      return;
   }

   /*
   if ( m_IndexPreferences == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuPreferences()); 
      return;    
   }
   */
   if ( m_IndexPreferencesUI == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuPreferencesUI()); 
      return;
   }

   if ( m_IndexUpdate == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;
      char szBuff[128];
      char szBuff2[64];
      getSystemVersionString(szBuff2, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);

      sprintf(szBuff, "Your controller has software version %s (b.%d)", szBuff2, SYSTEM_SW_BUILD_NUMBER);

      MenuConfirmation* pMC = new MenuConfirmation("Update Controller Software","Insert an USB stick containing the Ruby update archive file and then press Ok to start the update process.",1, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }

   if ( m_IndexReboot == m_SelectedIndex )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED )
      {
         char szText[256];
         if ( NULL != g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel )
            sprintf(szText, "Your %s is armed. Are you sure you want to reboot the controller?", g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel->getVehicleTypeString());
         else
            strcpy(szText, "Your vehicle is armed. Are you sure you want to reboot the controller?");
         MenuConfirmation* pMC = new MenuConfirmation("Warning! Reboot Confirmation", szText, 10);
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            pMC->addTopLine(" ");
            pMC->addTopLine("Warning: You have the RC link enabled, the vehicle flight controller might not go into failsafe mode during reboot.");
         }
         add_menu_to_stack(pMC);
         return;
      }
      onEventReboot();
      hardware_reboot();
   }

   if ( (m_IndexDeveloper != -1) && (m_IndexDeveloper == m_SelectedIndex) )
   {
      add_menu_to_stack(new MenuControllerDev());
      return;
   }
}


void MenuController::addMessage(const char* szMessage)
{
   Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE+11*1000,"Update Info",NULL);
   pm->m_xPos = 0.4; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->addTopLine(szMessage);
   add_menu_to_stack(pm);
}

void MenuController::updateSoftware()
{
   Popup* p = new Popup("Updating. Please wait...",0.36,0.4, 0.5, 60);
   popups_add_topmost(p);

   ruby_processing_loop(true);
   render_all(g_TimeNow);
   ruby_signal_alive();

   int iMountRes = hardware_try_mount_usb();
   if ( 1 != iMountRes )
   {
      popups_remove(p);
      ruby_signal_alive();
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();

      if ( 0 == iMountRes )
         addMessage("No USB memory stick detected. Please insert a USB stick.");
      else
      {
         if ( -1 == iMountRes )
            iMountRes = hardware_try_mount_usb();
         if ( 1 != iMountRes )
            addMessage("USB memory stick detected but could not be mounted. Please try again.");
      }
      ruby_signal_alive();
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();
      if ( 1 != iMountRes )
         return;
   }
   ruby_signal_alive();

   log_line("Starting controller update procedure.");

   popups_remove(p);

   ruby_pause_watchdog();
   pairing_stop();
   
   p = new Popup("Updating. Please wait...",0.36,0.4, 0.5, 60);
   popups_add_topmost(p);

   // Execute ruby_update_worker twice as the ruby_update_worker might have been updated itself too

   FILE* fd = fopen("tmp/tmp_update_result.txt", "wb");
   if ( fd != NULL )
   {
      fprintf(fd, "-10\n");
      fclose(fd);
      fd = NULL;
   }

   int iCounter[2];
   int iResult[2];
   iCounter[0] = iCounter[1] = 0;
   iResult[0] = iResult[1] = -1000;

   for( int iCount=0; iCount<2; iCount++ )
   {
      hw_execute_ruby_process(NULL, "ruby_update_worker", NULL, NULL);
      ruby_signal_alive();

      log_line("Waiting for update process to start and finish...");
      bool bFoundProcess = false;
      bool bFinishedProcess = false;
      do
      {
         g_TimeNow = get_current_timestamp_ms();
         char szTitle[64];
         strcpy(szTitle, "Updating. Please wait..");
         if ( 0 == (iCounter[iCount] % 3) )
            strcat(szTitle, ".");
         if ( 1 == (iCounter[iCount] % 3) )
            strcat(szTitle, "..");
         if ( 2 == (iCounter[iCount] % 3) )
            strcat(szTitle, "...");
         p->setTitle(szTitle);
         ruby_processing_loop(true);
         render_all_with_menus(g_TimeNow, false);
         ruby_signal_alive();
         hardware_sleep_ms(100);
         if ( bFoundProcess )
            log_line("Waiting for update worker to finish (%d)...", iCounter[iCount]);
         else
            log_line("Waiting for update worker to start (%d)...", iCounter[iCount]);
         iCounter[iCount]++;

         if ( bFoundProcess )
         {
             if ( 0 == hw_process_exists("ruby_update_worker") )
             if ( 0 == hw_process_exists("unzip") )
             {
                log_line("Update worker process finished (test 1)");
                hardware_sleep_ms(100);
                if ( 0 == hw_process_exists("ruby_update_worker") )
                if ( 0 == hw_process_exists("unzip") )
                {
                   log_line("Update worker process finished (test 2)");
                   bFinishedProcess = true;
                }
             }
         }

         if ( ! bFoundProcess )
         {
            if ( 0 != hw_process_exists("ruby_update_worker") )
            {
               log_line("Found update worker process.");
               bFoundProcess = true;
            }
         }

         if ( bFoundProcess )
         if ( bFinishedProcess )
            break;

      }
      while ( (iCounter[iCount] < 500) && (! bFinishedProcess) );
      
      log_line("Done waiting for update worker process (on counter %d).", iCounter[iCount]);
      iResult[iCount] = -10;
      fd = fopen("tmp/tmp_update_result.txt", "rb");
      if ( fd != NULL )
      {
         if ( 1 != fscanf(fd, "%d", &iResult[iCount] ) )
            iResult[iCount] = -10;
         fclose(fd);
         fd = NULL;
      }
      hw_execute_bash_command("rm -rf tmp/tmp_update_result.txt", NULL);
      if ( iResult[iCount] < 0 )
         break;
   }

   hardware_unmount_usb();
   ruby_processing_loop(true);
   render_all(g_TimeNow);
   ruby_signal_alive();

   ruby_resume_watchdog();

   popups_remove(p);
   ruby_processing_loop(true);
   render_all(g_TimeNow);
   ruby_signal_alive();

   if ( (iCounter[0] >= 500) || (iCounter[1] >= 500) )
   {
      m_bWaitingForUserFinishUpdateConfirmation = true;
      MenuConfirmation* pMC = new MenuConfirmation("Update Failed", "Update failed.", 5, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      log_line("Exit from main update procedure (1).");
      return;
   }

   if ( (iResult[0] < 0) || (iResult[1] < 0) )
   {
      m_bWaitingForUserFinishUpdateConfirmation = true;
      MenuConfirmation* pMC = NULL;
      
      if ( iResult[0] == -1 )
         pMC = new MenuConfirmation("Update Failed", "No update archive file found on the USB memory stick. No update done.",5, true);
      else if ( iResult[0] == -2 )
         pMC = new MenuConfirmation("Update Failed", "Can't do update. Can't access the controller files.",5, true);
      else if ( iResult[0] == -3 )
         pMC = new MenuConfirmation("Update Failed", "Update failed. Can't access the controller files.",5, true);
      else if ( iResult[0] == -4 )
         pMC = new MenuConfirmation("Update Info", "Files unchanged. Either you applyed the same update twice, either the update failed to write the new files.",5, true);
      else if ( iResult[0] == -10 )
         pMC = new MenuConfirmation("Update Failed", "The update archive file found on the USB memory stick looks to be invalid.",5, true);
      else if ( iResult[1] < 0 )
      {
         char szMsg[256];
         sprintf(szMsg, "The update procedure failed on second step, error code %d.", iResult[1]);
         pMC = new MenuConfirmation("Update Failed", szMsg, 5, true);
      }
      else
         pMC = new MenuConfirmation("Update Failed", "Update failed.", 5, true);

      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      log_line("Exit from main update procedure (2).");
      return;
   }

   m_bWaitingForUserFinishUpdateConfirmation = true;

   char szComm[256];
   char szOutput[4096];
   memset(szOutput, 0, 4096);
   sprintf(szComm, "./ruby_update %d %d", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR);
   hw_execute_bash_command_raw(szComm, szOutput);


   MenuConfirmation* pMC = new MenuConfirmation("Update Complete", "Update complete. You can now remove the USB stick. The system will reboot now.",3, true);
   pMC->m_yPos = 0.3;

   if ( 0 < strlen(szOutput) )
   {
      char* pSt = szOutput;
      while (*pSt)
      {
         char* pEnd = pSt;
         while ( (*pEnd != 0) && (*pEnd != 10) && (*pEnd != 13) )
            pEnd++;
         *pEnd = 0;
         if ( pEnd > pSt )
            pMC->addTopLine(pSt);
         pSt = pEnd+1;
      }
   }

   add_menu_to_stack(pMC);
   log_line("Exit from main update procedure (normal exit).");
}
