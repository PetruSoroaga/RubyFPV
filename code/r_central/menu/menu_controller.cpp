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

#include "menu.h"
#include "menu_objects.h"
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "menu_controller_expert.h"
#include "menu_controller_peripherals.h"
#include "menu_controller_radio_interfaces.h"
#include "menu_controller_video.h"
#include "menu_controller_telemetry.h"
#include "menu_controller_network.h"
#include "menu_controller_plugins.h"
#include "menu_controller_encryption.h"
#include "menu_controller_recording.h"
#include "menu_preferences_buttons.h"
#include "menu_preferences_ui.h"

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
   Preferences* pp = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();
   
   m_IndexPorts = addMenuItem(new MenuItem("Peripherals / Ports", "Change controller peripherals settings (serial ports, USB devices, HID, I2C devices, etc)"));
   //m_pMenuItems[m_IndexPorts]->showArrow();

   //m_IndexInterfaces = addMenuItem(new MenuItem("Radio Interfaces", "Change controller radio interfaces settings"));
   //m_pMenuItems[m_IndexInterfaces]->showArrow();
   m_IndexInterfaces = -1;
   
   m_IndexVideo = addMenuItem(new MenuItem("Audio & Video Output", "Change Audio and Video Output Settings (HDMI, USB Tethering, Audio output device)"));
   //m_pMenuItems[m_IndexVideo]->showArrow();

   m_IndexTelemetry = addMenuItem(new MenuItem("Telemetry Input/Output", "Change the Telemetry Input/Output settings on the controller ports."));
   //m_pMenuItems[m_IndexTelemetry]->showArrow();

   m_IndexRecording = addMenuItem(new MenuItem("Recording", "Change the recording settings"));
   //m_pMenuItems[m_IndexRecording]->showArrow();

   m_IndexCPU = addMenuItem(new MenuItem("CPU and Processes", "Set CPU Overclocking, Processes Priorities"));
   //m_pMenuItems[m_IndexCPU]->showArrow();

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

   m_IndexEncryption = addMenuItem(new MenuItem("Encryption", "Change the encryption global settings"));
   //m_pMenuItems[m_IndexEncryption]->showArrow();
   //m_pMenuItems[m_IndexEncryption]->setEnabled(false);

   m_IndexPreferences = addMenuItem(new MenuItem("Buttons", "Change buttons actions."));
   m_IndexPreferencesUI = addMenuItem(new MenuItem("User Interface", "Change user interface preferences."));

   addMenuItem(new MenuItemSection("Management"));

   m_IndexPlugins = addMenuItem(new MenuItem("Manage Plugins", "Configure, add and remove controller software plugins."));
   m_IndexUpdate = addMenuItem(new MenuItem("Update Software", "Updates software on this controller using a USB memory stick."));
   m_IndexReboot = addMenuItem(new MenuItem("Restart", "Restarts the controller."));
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

void MenuController::onReturnFromChild(int returnValue)
{
   log_line("Returned from child: %d, return value: %d", m_iConfirmationId, returnValue);

   Menu::onReturnFromChild(returnValue);

   if ( 1 == m_iConfirmationId && 1 == returnValue )
   {
      m_iConfirmationId = 0;
      m_iMustStartUpdate = 3;
      log_line("Signaled event to start controller update.");
      return;
   }
   if ( 3 == m_iConfirmationId )
   {
      m_bWaitingForUserFinishUpdateConfirmation = false;
      log_line("Closed message update. Will reboot now.");
      m_iConfirmationId = 0;
      onEventReboot();
      hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }
   if ( 5 == m_iConfirmationId )
   {
      m_bWaitingForUserFinishUpdateConfirmation = false;
      m_iConfirmationId = 0;

      log_line("Closed message update. Will reboot now.");
      onEventReboot();
      hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }

   if ( 10 == m_iConfirmationId && 1 == returnValue )
   {
      onEventReboot();
      hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }

   if ( 20 == m_iConfirmationId && 1 == returnValue )
   {
      onEventReboot();
      hw_execute_bash_command("sudo reboot -f", NULL);
      return;
   }

   m_iConfirmationId = 0;

   if ( ! m_bShownHDMIChangeNotif )
   if ( access( FILE_TMP_HDMI_CHANGED, R_OK ) != -1 )
   {
      m_bShownHDMIChangeNotif = true;
      m_iConfirmationId = 20;
      MenuConfirmation* pMC = new MenuConfirmation("HDMI Output Configuration Changed","You updated the HDMI output configuration. Do you want to reboot now for the changes to take effect?", m_iConfirmationId);
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

   if ( m_IndexInterfaces == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerRadioInterfaces());
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

   if ( m_IndexEncryption == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuControllerEncryption());
      return;
   }

   if ( m_IndexPreferences == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuPreferences()); 
      return;
   }

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

      m_iConfirmationId = 1;
      MenuConfirmation* pMC = new MenuConfirmation("Update Controller Software","Insert an USB stick containing the Ruby update archive file and then press Ok to start the update process.",m_iConfirmationId, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }

   if ( m_IndexReboot == m_SelectedIndex )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED )
      {
         m_iConfirmationId = 10;
         MenuConfirmation* pMC = new MenuConfirmation("Warning! Reboot Confirmation","Your vehicle is armed. Are you sure you want to reboot the controller?", m_iConfirmationId);
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            pMC->addTopLine(" ");
            pMC->addTopLine("Warning: You have the RC link enabled, the vehicle flight controller might not go into failsafe mode during reboot.");
         }
         add_menu_to_stack(pMC);
         return;
      }
      onEventReboot();
      //sync();
      hw_execute_bash_command("sudo reboot -f", NULL);
   }
}


void MenuController::addMessage(const char* szMessage)
{
   m_iConfirmationId = 20;
   Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Update Info",NULL);
   pm->m_xPos = 0.4; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->addTopLine(szMessage);
   add_menu_to_stack(pm);
}

void MenuController::updateSoftware()
{
   if ( ! hardware_try_mount_usb() )
   {
      addMessage("No USB memory stick detected. Please insert a USB stick");
      return;
   }

   log_line("Starting controller update procedure.");

   ruby_pause_watchdog();

   pairing_stop();

   FILE* fd = fopen("tmp/tmp_update_result.txt", "wb");
   if ( fd != NULL )
   {
      fprintf(fd, "-10\n");
      fclose(fd);
   }

   hw_execute_bash_command("./ruby_update_worker &", NULL);

   ruby_signal_alive();

   Popup* p = new Popup("Updating. Please wait...",0.3,0.4, 0.5, 15);
   popups_add_topmost(p);

   log_line("Waiting for update process to start and finish...");
   int counter = 0;
   bool bFoundProcess = false;
   bool bFinishedProcess = false;
   do
   {
      g_TimeNow = get_current_timestamp_ms();
      char szTitle[64];
      strcpy(szTitle, "Updating. Please wait..");
      if ( 0 == (counter % 3) )
         strcat(szTitle, ".");
      if ( 1 == (counter % 3) )
         strcat(szTitle, "..");
      if ( 2 == (counter % 3) )
         strcat(szTitle, "...");
      p->setTitle(szTitle);
      ruby_processing_loop(true);
      render_all(g_TimeNow);
      ruby_signal_alive();
      hardware_sleep_ms(200);
      if ( bFoundProcess )
         log_line("Waiting for update worker to finish (%d)...", counter);
      else
         log_line("Waiting for update worker to start (%d)...", counter);
      counter++;

      if ( bFoundProcess )
      {
          if ( 0 == hw_process_exists("ruby_update_worker") )
          {
             log_line("Update worker process finished.");
             bFinishedProcess = true;
          }
      }

      if ( ! bFoundProcess )
      {
         if ( 1 == hw_process_exists("ruby_update_worker") )
         {
            log_line("Found update worker process.");
            bFoundProcess = true;
         }
      }

      if ( bFoundProcess )
      if ( bFinishedProcess )
         break;

   }
   while ( (counter < 50) && (! bFinishedProcess) );


   log_line("Done waiting for update worker process (on counter %d).", counter);
   hardware_unmount_usb();

   ruby_processing_loop(true);
   render_all(g_TimeNow);
   ruby_signal_alive();

   ruby_resume_watchdog();

   popups_remove(p);

   int iResult = -10;
   fd = fopen("tmp/tmp_update_result.txt", "rb");
   if ( fd != NULL )
   {
      if ( 1 != fscanf(fd, "%d", &iResult ) )
         iResult = -10;
      fclose(fd);
   }

   hw_execute_bash_command("rm -rf tmp/tmp_update_result.txt", NULL);

   if ( counter >= 50 )
   {
      m_bWaitingForUserFinishUpdateConfirmation = true;
      m_iConfirmationId = 5;
      MenuConfirmation* pMC = new MenuConfirmation("Update Failed", "Update failed.",m_iConfirmationId, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      log_line("Exit from main update procedure (1).");
      return;

   }
   if ( iResult < 0 )
   {
      m_bWaitingForUserFinishUpdateConfirmation = true;
      m_iConfirmationId = 5;
      MenuConfirmation* pMC = NULL;
      
      if ( iResult == -1 )
         pMC = new MenuConfirmation("Update Failed", "No update archive file found on the USB memory stick. No update done.",m_iConfirmationId, true);
      else if ( iResult == -2 )
         pMC = new MenuConfirmation("Update Failed", "Can't do update. Can't access the controller files.",m_iConfirmationId, true);
      else if ( iResult == -3 )
         pMC = new MenuConfirmation("Update Failed", "Update failed. Can't access the controller files.",m_iConfirmationId, true);
      else if ( iResult == -4 )
         pMC = new MenuConfirmation("Update Info", "Files unchanged. Either you applyed the same update twice, either the update failed to write the new files.",m_iConfirmationId, true);
      else if ( iResult == -10 )
         pMC = new MenuConfirmation("Update Failed", "The update archive file found on the USB memory stick looks to be invalid.",m_iConfirmationId, true);
      else
         pMC = new MenuConfirmation("Update Failed", "Update failed.",m_iConfirmationId, true);

      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      log_line("Exit from main update procedure (2).");
      return;
   }

   m_bWaitingForUserFinishUpdateConfirmation = true;
   m_iConfirmationId = 3;
   MenuConfirmation* pMC = new MenuConfirmation("Update Complete", "Update complete. You can now remove the USB stick. The system will reboot now.",m_iConfirmationId, true);
   pMC->m_yPos = 0.3;
   add_menu_to_stack(pMC);
   log_line("Exit from main update procedure (normal exit).");
}
