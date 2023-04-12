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
#include "menu_controller.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_system_dev_logs.h"
#include "menu_system_video_profiles.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../radio/radiolink.h"

#include "colors.h"
#include "pairing.h"
#include "../base/utils.h"
#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "ruby_central.h"
#include "rx_scope.h"
#include "timers.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

MenuSystemDevLogs::MenuSystemDevLogs(void)
:Menu(MENU_ID_DEV_LOGS, "Developer Log Settings", NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.16;
   m_ExtraHeight += 0.5*menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS*(1+MENU_TEXTLINE_SPACING);

   float fSliderWidth = 0.14;

   m_pItemsSelect[0] = new MenuItemSelect("Log System (Controller)", "Sets the type of log system to use: directly to files or using a service. Requires a reboot after the change.");
   m_pItemsSelect[0]->addSelection("Files");
   m_pItemsSelect[0]->addSelection("Service");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexLogServiceController = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Log System (Vehicle)", "Sets the type of log system to use: directly to files or using a service. Requires a reboot after the change.");
   m_pItemsSelect[1]->addSelection("Files");
   m_pItemsSelect[1]->addSelection("Service");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexLogServiceVehicle = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[6] = new MenuItemSelect("Vehicle Log Level", "How much logs are saved on current vehicle.");
   m_pItemsSelect[6]->addSelection("Full");
   m_pItemsSelect[6]->addSelection("Errors");
   m_pItemsSelect[6]->setIsEditable();
   m_IndexLogLevelVehicle = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[7] = new MenuItemSelect("Controller Log Level", "How much logs are saved on this controller.");
   m_pItemsSelect[7]->addSelection("Full");
   m_pItemsSelect[7]->addSelection("Errors");
   m_pItemsSelect[7]->setIsEditable();
   m_IndexLogLevelController = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[2] = new MenuItemSelect("Enable Vehicle Live Log", "Gets a stream of the live log for the current vehicle.");
   m_pItemsSelect[2]->addSelection("Disabled");
   m_pItemsSelect[2]->addSelection("Enabled");
   m_IndexEnableLiveLog = addMenuItem(m_pItemsSelect[2]);

   m_IndexGetVehicleLogs = addMenuItem( new MenuItem("Get Vehicle Logs") );
   m_IndexZipAllLogs = addMenuItem( new MenuItem("Export all logs", "Exports all controller logs and all vehicle logs (that are already downloaded) to a USB memort stick.") );
   m_pMenuItems[m_IndexZipAllLogs]->showArrow();

   m_IndexClearControllerLogs = addMenuItem( new MenuItem("Clear controller logs") );
   m_pMenuItems[m_IndexClearControllerLogs]->showArrow();

   m_IndexClearVehicleLogs = addMenuItem( new MenuItem("Clear vehicle logs") );
   m_pMenuItems[m_IndexClearVehicleLogs]->showArrow();

   //for( int i=0; i<m_ItemsCount; i++ )
   //   m_pMenuItems[i]->setTextColor(get_Color_Dev());
}

void MenuSystemDevLogs::valuesToUI()
{
   m_ExtraHeight = menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
   Preferences* pP = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();

   if ( access( LOG_USE_PROCESS, R_OK ) != -1 )
      m_pItemsSelect[0]->setSelectedIndex(1);
   else
      m_pItemsSelect[0]->setSelectedIndex(0);
         
   if ( NULL == g_pCurrentModel )
   {
      m_pItemsSelect[6]->setEnabled(false);
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSelect[1]->setSelectedIndex(0);
   }
   else
   {
      m_pItemsSelect[1]->setEnabled(true);
      if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE )
         m_pItemsSelect[1]->setSelectedIndex(1);
      else
         m_pItemsSelect[1]->setSelectedIndex(0);

      m_pItemsSelect[2]->setEnabled(true);
      m_pItemsSelect[2]->setSelection( (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)?1:0 );

      m_pItemsSelect[6]->setEnabled(true);
      m_pItemsSelect[6]->setSelectedIndex(0);
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
         m_pItemsSelect[6]->setSelectedIndex(1);

   }

   m_pItemsSelect[7]->setSelectedIndex(0);
   if ( pP->nLogLevel != 0 )
      m_pItemsSelect[7]->setSelectedIndex(1);
}

void MenuSystemDevLogs::onShow()
{
   Menu::onShow();
}


void MenuSystemDevLogs::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}

void MenuSystemDevLogs::exportVehicleLogs(char* szVehicleFolder)
{
   if ( NULL == szVehicleFolder || 0 == szVehicleFolder[0] )
      return;

   DIR *d;
   struct dirent *dir;
   char szFolder[256];
   char szComm[256];

   sprintf(szFolder, "%s/%s/", FOLDER_MEDIA, szVehicleFolder);
   d = opendir(szFolder);
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < 4 )
            continue;
         ruby_signal_alive();
         sprintf(szComm, "mkdir -p %s/%s/Ruby/%s/", FOLDER_RUBY, FOLDER_USB_MOUNT, szVehicleFolder);
         hw_execute_bash_command(szComm, NULL);

         sprintf(szComm, "cp -rf %s/%s %s/%s/Ruby/%s/%s", szFolder, dir->d_name, FOLDER_RUBY, FOLDER_USB_MOUNT, szVehicleFolder, dir->d_name);
         hw_execute_bash_command(szComm, NULL);
      }
      closedir(d);
   }
}

void MenuSystemDevLogs::exportAllLogs()
{
   if ( ! hardware_try_mount_usb() )
   {
      addMessage("No USB memory stick detected. Please insert a USB stick");
      return;
   }
   ruby_signal_alive();

   char szOutput[1024];
   char szComm[1024];
   char szBuff[1024];
   hw_execute_bash_command_raw("which zip", szOutput);
   if ( 0 == strlen(szOutput) || NULL == strstr(szOutput, "zip") )
   {
      hardware_unmount_usb();
      addMessage("Failed, could not find zip program.");
      return;
   }

   ruby_signal_alive();
   ruby_pause_watchdog();

   Popup* p = new Popup("Exporting. Please wait...",0.3,0.4, 0.5, 15);
   popups_add_topmost(p);
   ruby_input_loop(true);
   g_TimeNow = get_current_timestamp_ms();
   render_all(g_TimeNow);
   ruby_signal_alive();
      
   hw_execute_bash_command("mkdir -p tmp/exportcontrollerlogs", NULL);
   hw_execute_bash_command("chmod 777 tmp/exportcontrollerlogs", NULL);
   hw_execute_bash_command("rm -rf tmp/exportcontrollerlogs/* 2>/dev/null", NULL);
   sprintf(szComm, "zip tmp/exportcontrollerlogs/ruby_controller_logs_controller_id_%u_%d.zip logs/*", g_uControllerId, g_iBootCount);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szBuff, "%s/%s/Ruby", FOLDER_RUBY, FOLDER_USB_MOUNT);
   sprintf(szComm, "mkdir -p %s", szBuff );
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "cp -rf tmp/exportcontrollerlogs/ruby_controller_logs_controller_id_%u_%d.zip %s/%s/Ruby/ruby_controller_logs_controller_id_%u_%d.zip", g_uControllerId, g_iBootCount, FOLDER_RUBY, FOLDER_USB_MOUNT, g_uControllerId, g_iBootCount);
   hw_execute_bash_command(szComm, NULL);
   hw_execute_bash_command("rm -rf tmp/exportcontrollerlogs 2>/dev/null", NULL);

   DIR *d;
   struct dirent *dir;
   d = opendir(FOLDER_MEDIA);
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < 4 )
            continue;
         ruby_signal_alive();

         if ( strncmp(dir->d_name, "vehicle-", 8) != 0 )
            continue;
         exportVehicleLogs(dir->d_name);
      }
      closedir(d);
   }

   hardware_unmount_usb();
   ruby_signal_alive();
   sync();
   ruby_signal_alive();
   ruby_resume_watchdog();
   popups_remove(p);
   addMessage("Done. All logs have been copied to the USB memory stick on a folder named Ruby. You can now remove the USB memory stick.");
}

void MenuSystemDevLogs::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   // Export controller logs to USB zip file
   if ( 1 == m_iConfirmationId && 1 == returnValue )
   {
      m_iConfirmationId = 0;
      exportAllLogs();
      return;
   }   

   // Delete controller logs
   if ( 2 == m_iConfirmationId && 1 == returnValue )
   {
      m_iConfirmationId = 0;
      hw_execute_bash_command("rm -rf logs/*", NULL);
      addMessage("Done. All controller logs have been cleared.");
      return;
   }   

   // Delete vehicle logs
   if ( 3 == m_iConfirmationId && 1 == returnValue )
   {
      m_iConfirmationId = 0;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_CLEAR_LOGS, 0, NULL, 0) )
         valuesToUI();
      else
         addMessage("Done. All vehicle logs have been cleared.");
      return;
   }   

   m_iConfirmationId = 0;
}


void MenuSystemDevLogs::onSelectItem()
{
   Menu::onSelectItem();
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   Preferences* pP = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();

   if ( m_IndexLogServiceController == m_SelectedIndex )
   {
      char szBuff[128];
      if ( 0 == m_pItemsSelect[0]->getSelectedIndex() )
         sprintf(szBuff, "rm -rf %s", LOG_USE_PROCESS);
      else
         sprintf(szBuff, "touch %s", LOG_USE_PROCESS);
      hw_execute_bash_command(szBuff, NULL);
      return;
   }

   if ( m_IndexLogServiceVehicle == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
         return;
      int val = m_pItemsSelect[1]->getSelectedIndex();
      u32 uFlags = g_pCurrentModel->uModelFlags;
      if ( 1 == val )
        uFlags |= MODEL_FLAG_USE_LOGER_SERVICE;
      else
         uFlags &= ~MODEL_FLAG_USE_LOGER_SERVICE;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_MODEL_FLAGS, uFlags, NULL, 0) )
         valuesToUI();  
      return;
   }

   if ( m_IndexEnableLiveLog == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
         return;
      u32 enable = m_pItemsSelect[2]->getSelectedIndex();
      if ( 0 == enable )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_LIVE_LOG);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_LIVE_LOG;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_ENABLE_LIVE_LOG, enable, NULL, 0) )
         valuesToUI();  
   }

   if ( m_IndexGetVehicleLogs == m_SelectedIndex )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_DOWNLOAD_FILE, FILE_ID_VEHICLE_LOGS_ARCHIVE, NULL, 0) )
         valuesToUI();
      else
         menu_close_all();
   }

   if ( m_IndexLogLevelVehicle == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
         return;
      int log = m_pItemsSelect[6]->getSelectedIndex();
      if ( 0 == log )
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS);
      else
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_DEVELOPER_FLAGS, g_pCurrentModel->uDeveloperFlags, NULL, 0) )
         valuesToUI();  
   }

   if ( m_IndexLogLevelController == m_SelectedIndex )
   {
      pP->nLogLevel = m_pItemsSelect[7]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
   }

   if ( m_IndexZipAllLogs == m_SelectedIndex )
   {
      m_iConfirmationId = 1;
      MenuConfirmation* pMC = new MenuConfirmation("Export all logs","Insert an USB memory stick and press [Ok] to export the logs to the memory stick.",m_iConfirmationId, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }

   if ( m_IndexClearControllerLogs == m_SelectedIndex )
   {
      m_iConfirmationId = 2;
      MenuConfirmation* pMC = new MenuConfirmation("Clear controller logs", "Are you sure you want to delete all the controller logs?",m_iConfirmationId);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }

   if ( m_IndexClearVehicleLogs == m_SelectedIndex )
   {
      if ( NULL == g_pCurrentModel )
      {
         addMessage("Connect to a vehicle first.");
         return;
      }

      m_iConfirmationId = 3;
      MenuConfirmation* pMC = new MenuConfirmation("Clear vehicle logs", "Are you sure you want to delete all the vehicle logs?",m_iConfirmationId);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }
}

