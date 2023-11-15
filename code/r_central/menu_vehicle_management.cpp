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
#include "../base/hardware.h"
#include "../common/string_utils.h"
#include "render_commands.h"
#include "osd.h"
#include "osd_common.h"
#include "popup.h"
#include "colors.h"
#include "menu.h"
#include "menu_vehicle_management.h"
#include "menu_vehicle_general.h"
#include "menu_vehicle_expert.h"
#include "menu_confirmation.h"
#include "menu_vehicle_import.h"
#include "menu_vehicle_management_plugins.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include <ctype.h>
#include "handle_commands.h"
#include "warnings.h"
#include "notifications.h"
#include "events.h"

MenuVehicleManagement::MenuVehicleManagement(void)
:Menu(MENU_ID_VEHICLE_MANAGEMENT, "Vehicle Management", NULL)
{
   m_Width = 0.16;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;
   m_fIconSize = 1.0;
   m_bWaitingForVehicleInfo = false;
}

MenuVehicleManagement::~MenuVehicleManagement()
{
}

void MenuVehicleManagement::onShow()
{
   m_Height = 0.0;
   m_ExtraHeight = 0;
   
   removeAllItems();

   m_IndexConfig = addMenuItem(new MenuItem("Get Config Info", "Gets the hardware capabilities and current configuration of the vehicle."));
   m_IndexModules = addMenuItem(new MenuItem("Get Modules Info", "Gets the current detected and loaded modules on the vehicle."));
   m_IndexPlugins = addMenuItem(new MenuItem("Core Plugins", "Manage the core plugins on this vehicle."));
   m_IndexExport = addMenuItem(new MenuItem("Export Model Settings","Exports the model settings to a USB stick."));
   m_IndexImport = addMenuItem(new MenuItem("Import Model Settings","Imports the model settings from a USB stick."));
   m_IndexUpdate = addMenuItem(new MenuItem("Update Software","Updates the software on the vehicle."));
   m_IndexReset  = addMenuItem(new MenuItem("Reset to defaults", "Resets all parameters for this vehicle to the default configuration (except for frequency, vehicle ID and name)."));
   m_IndexReboot = addMenuItem(new MenuItem("Restart", "Restarts the vehicle."));
   m_IndexDelete = addMenuItem(new MenuItem("Delete","Delete this vehicle from your control list."));

   bool bConnected = false;
   if ( pairing_isStarted() && (pairing_isReceiving() || pairing_wasReceiving()) )
      bConnected = true;

   if ( g_pCurrentModel->b_mustSyncFromVehicle )
      bConnected = false;

   if ( ! bConnected )
   {
      m_pMenuItems[m_IndexConfig]->setEnabled(false);
      m_pMenuItems[m_IndexModules]->setEnabled(false);
      m_pMenuItems[m_IndexImport]->setEnabled(false);
      m_pMenuItems[m_IndexUpdate]->setEnabled(false);
      m_pMenuItems[m_IndexReset]->setEnabled(false);
      m_pMenuItems[m_IndexReboot]->setEnabled(false);
   }
   
   Menu::onShow();
}

void MenuVehicleManagement::Render()
{
   RenderPrepare();

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

bool MenuVehicleManagement::periodicLoop()
{
   if ( ! m_bWaitingForVehicleInfo )
      return false;

   if ( ! handle_commands_is_command_in_progress() )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         add_menu_to_stack(new MenuVehicleManagePlugins());
         m_bWaitingForVehicleInfo = false;
         return true;
      }
   }
   return false;
}

int MenuVehicleManagement::onBack()
{
   return Menu::onBack();
}
     
void MenuVehicleManagement::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   // Delete model
   if ( 1 == returnValue && 1 == m_iConfirmationId )
   {
      if ( NULL != g_pCurrentModel )
         pairing_stop();
      menu_close_all();
      u32 uVehicleId = g_pCurrentModel->vehicle_id;
      deleteModel(g_pCurrentModel);
      g_pCurrentModel = NULL;
      notification_add_model_deleted();
      onModelDeleted(uVehicleId);
      m_iConfirmationId = 0;
      return;
   }

   // Upload software
   if ( 1 == returnValue && (2 == m_iConfirmationId || 4 == m_iConfirmationId) )
   {
      m_iConfirmationId = 0;
      if ( uploadSoftware() )
      {
         if ( NULL != g_pCurrentModel )
         {
            g_pCurrentModel->sw_version = (SYSTEM_SW_VERSION_MAJOR*256+SYSTEM_SW_VERSION_MINOR) | (SYSTEM_SW_BUILD_NUMBER << 16);
            saveAsCurrentModel(g_pCurrentModel);
         }
         m_iConfirmationId = 3;
         Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Upload Succeeded",NULL);
         pm->m_xPos = 0.4; pm->m_yPos = 0.4;
         pm->m_Width = 0.36;
         pm->addTopLine("Your vehicle was updated. It will reboot now.");
         add_menu_to_stack(pm);
      }
      return;
   }

   if ( 3 == m_iConfirmationId )
   {
      menu_close_all();
      m_iConfirmationId = 0;
      g_bSyncModelSettingsOnLinkRecover = true;
      return;
   }

   if ( 10 == m_iConfirmationId && 1 == returnValue )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0) )
         valuesToUI();
      else
         menu_close_all();
      m_iConfirmationId = 0;
      return;
   }

   if ( 20 == m_iConfirmationId && 1 == returnValue )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_RESET_ALL_TO_DEFAULTS, 0, NULL, 0) )
         valuesToUI();
      else
      {
         g_bSyncModelSettingsOnLinkRecover = true;
         m_iConfirmationId = 3;
         Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Reset Complete",NULL);
         pm->m_xPos = 0.4; pm->m_yPos = 0.4;
         pm->m_Width = 0.36;
         pm->addTopLine("Your vehicle was reseted to default settings. It will reboot now.");
         add_menu_to_stack(pm);
      }
      return;
   }

   m_iConfirmationId = 0;
}

void MenuVehicleManagement::onSelectItem()
{
   if ( NULL == g_pCurrentModel )
   {
      Popup* p = new Popup("Vehicle is offline", 0.3, 0.3, 0.5, 4 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
   {
      Popup* p = new Popup("Vehicle Settings can not be changed on a spectator vehicle.", 0.3, 0.3, 0.5, 4 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( m_IndexConfig == m_SelectedIndex )
   {
      if ( handle_commands_is_command_in_progress() )
      {
         handle_commands_show_popup_progress();
         return;
      }
      handle_commands_send_to_vehicle(COMMAND_ID_GET_CURRENT_VIDEO_CONFIG, 0, NULL, 0);
   }

   if ( m_IndexModules == m_SelectedIndex )
   {
      if ( handle_commands_is_command_in_progress() )
      {
         handle_commands_show_popup_progress();
         return;
      }
      handle_commands_send_to_vehicle(COMMAND_ID_GET_MODULES_INFO, 0, NULL, 0);
   }

   if ( m_IndexPlugins == m_SelectedIndex )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         add_menu_to_stack(new MenuVehicleManagePlugins());
         return;
      }

      if ( ((g_pCurrentModel->sw_version >>8) & 0xFF) == 6 )
      if ( ((g_pCurrentModel->sw_version & 0xFF) < 9) || ( (g_pCurrentModel->sw_version & 0xFF) >= 10 && (g_pCurrentModel->sw_version & 0xFF) < 90) )
      {
         addMessage("You need to update your vehicle sowftware to be able to use core plugins.");
         return;
      }
      handle_commands_reset_has_received_vehicle_core_plugins_info();
      m_bWaitingForVehicleInfo = false;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_GET_CORE_PLUGINS_INFO, 0, NULL, 0) )
         valuesToUI();
      else
         m_bWaitingForVehicleInfo = true;
      return;
   }

   if ( m_IndexExport == m_SelectedIndex )
   {
      if ( ! hardware_try_mount_usb() )
      {
         log_line("No USB memory stick available.");
         Popup* p = new Popup("Please insert a USB memory stick.",0.28, 0.32, 0.32, 3);
         p->setCentered();
         p->setIconId(g_idIconInfo, get_Color_IconWarning());
         popups_add_topmost(p);
         return;
      }
      char szFile[256];
      char szModelName[256];

      strcpy(szModelName, g_pCurrentModel->getLongName());
      str_sanitize_filename(szModelName);

      snprintf(szFile, sizeof(szFile), "%s/%s/ruby_model_%s_%u.txt", FOLDER_RUBY, FOLDER_USB_MOUNT, szModelName, g_pCurrentModel->vehicle_id);
      g_pCurrentModel->saveToFile(szFile, false);
   
      hardware_unmount_usb();
      ruby_signal_alive();
      sync();
      ruby_signal_alive();
      m_iConfirmationId = 10;
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Export Succeeded",NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine("Your vehicle settings have been stored to the USB stick. You can now remove the USB stick.");
      add_menu_to_stack(pm);
      return;
   }

   if ( m_IndexImport == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;
      if ( ! hardware_try_mount_usb() )
      {
         log_line("No USB memory stick available.");
         Popup* p = new Popup("Please insert a USB memory stick.",0.28, 0.32, 0.32, 3);
         p->setCentered();
         p->setIconId(g_idIconInfo, get_Color_IconWarning());
         popups_add_topmost(p);
         return;
      }

      MenuVehicleImport* pM = new MenuVehicleImport();
      pM->buildSettingsFileList();
      if ( 0 == pM->getSettingsFilesCount() )
      {
         hardware_unmount_usb();
         ruby_signal_alive();
         sync();
         ruby_signal_alive();
         delete pM;
         m_iConfirmationId = 10;
         Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"No settings files",NULL);
         pm->m_xPos = 0.4; pm->m_yPos = 0.4;
         pm->m_Width = 0.36;
         pm->addTopLine("There are no vehicle settings files on the USB stick.");
         add_menu_to_stack(pm);
         return;
      }
      add_menu_to_stack(pM);
   }

   if ( m_IndexUpdate == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;
      if ( (! pairing_isStarted()) || (! pairing_isReceiving()) )
      {
         addMessage("Please connect to your vehicle first, if you want to update the sowftware on the vehicle.");
         return;
      }

      char szBuff2[64];
      getSystemVersionString(szBuff2, g_pCurrentModel->sw_version);

      if ( ((g_pCurrentModel->sw_version) & 0xFFFF) >= (SYSTEM_SW_VERSION_MAJOR*256)+SYSTEM_SW_VERSION_MINOR )
      {
         char szBuff[256];

         snprintf(szBuff, sizeof(szBuff), "Your vehicle already has the latest version of the software (version %s). Do you still want to upgrade vehicle?", szBuff2);
         m_iConfirmationId = 4;
         MenuConfirmation* pMC = new MenuConfirmation("Upgrade Confirmation",szBuff, m_iConfirmationId);
         add_menu_to_stack(pMC);
         //pMC->addTopLine(" ");
         //pMC->addTopLine("Note: Do not keep the vehicle very close to the controller as the radio power might be too powerfull and generate noise.");
         return;
      }
      char szBuff[256];
      char szBuff3[64];
      getSystemVersionString(szBuff3, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
      snprintf(szBuff, sizeof(szBuff), "Your vehicle has software version %s and software version %s is available on the controller. Do you want to upgrade vehicle?", szBuff2, szBuff3);
      m_iConfirmationId = 2;
      MenuConfirmation* pMC = new MenuConfirmation("Upgrade Confirmation",szBuff, m_iConfirmationId);
      add_menu_to_stack(pMC);
   }
        
   if ( m_IndexReset == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;
      char szBuff[256];
      snprintf(szBuff, sizeof(szBuff), "Are you sure you want to reset all parameters for %s?", g_pCurrentModel->getLongName());
      m_iConfirmationId = 20;
      MenuConfirmation* pMC = new MenuConfirmation("Confirmation",szBuff, m_iConfirmationId);
      add_menu_to_stack(pMC);
   }
 
   if ( m_IndexDelete == m_SelectedIndex )
   {
      if ( checkIsArmed() )
         return;
      char szBuff[64];
      snprintf(szBuff, sizeof(szBuff), "Are you sure you want to delete %s?", g_pCurrentModel->getLongName());
      add_menu_to_stack(new MenuConfirmation("Confirmation",szBuff, 1));
      m_iConfirmationId = 1;
   }

   if ( m_IndexReboot == m_SelectedIndex )
   {
      if ( pairing_isReceiving() || pairing_wasReceiving() )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED )
      {
         m_iConfirmationId = 10;
         MenuConfirmation* pMC = new MenuConfirmation("Warning! Reboot Confirmation","Your vehicle is armed. Are you sure you want to reboot the vehicle?", m_iConfirmationId);
         if ( g_pCurrentModel->rc_params.rc_enabled )
         {
            pMC->addTopLine(" ");
            pMC->addTopLine("Warning: You have the RC link enabled, the vehicle flight controller will go into RC failsafe mode during reboot.");
         }
         add_menu_to_stack(pMC);
         return;
      }
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0) )
         valuesToUI();
      else
         menu_close_all();
   }
}

