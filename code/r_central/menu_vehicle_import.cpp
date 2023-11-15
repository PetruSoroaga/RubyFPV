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
#include "render_commands.h"
#include "osd.h"
#include "popup.h"
#include "menu.h"
#include "menu_vehicle_import.h"
#include "menu_confirmation.h"

#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include <ctype.h>
#include "handle_commands.h"
#include "warnings.h"
#include "notifications.h"

MenuVehicleImport::MenuVehicleImport(void)
:Menu(MENU_ID_VEHICLE_IMPORT, "Import Vehicle Settings", NULL)
{
   m_Width = 0.45;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.24;
   m_TempFilesCount = 0;
   buildSettingsFileList();
   m_bCreateNew = false;
}

MenuVehicleImport::~MenuVehicleImport()
{
   hardware_unmount_usb();
   sync();
   ruby_signal_alive();
   for( int i=0; i<m_TempFilesCount; i++ )
      free(m_szTempFiles[i]);
   m_TempFilesCount = 0;
}

int MenuVehicleImport::getSettingsFilesCount()
{
   return m_TempFilesCount;
}

void MenuVehicleImport::setCreateNew()
{
   m_bCreateNew = true;
}


void MenuVehicleImport::onShow()
{
   m_Height = 0.0;
   m_ExtraHeight = 0;
   
   removeAllTopLines();
   removeAllItems();
   buildSettingsFileList();

   char szId[256];
   snprintf(szId, sizeof(szId), "%u", g_pCurrentModel->vehicle_id);
   bool found = false;
   int selectedIndex = -1;

   if ( ! m_bCreateNew )
   for( int i=0; i<m_TempFilesCount; i++ )
      if ( NULL != strstr(m_szTempFiles[i], szId) )
      {
         found = true;
         selectedIndex = i;
         break;
      }

   if ( found )
      addTopLine("Found matching settings file for current vehicle.");
   else if ( m_bCreateNew )
      addTopLine("Select the file you wish to import as a new vehicle.");
   else
      addTopLine("Select the file you wish to import. Your current settings for this vehicle will be overwritten with the ones from the file you import.");

   for( int i=0; i<m_TempFilesCount; i++ )
      addMenuItem(new MenuItem(m_szTempFiles[i]));

   if ( -1 != selectedIndex )
      m_SelectedIndex = selectedIndex;
   Menu::onShow();
}


void MenuVehicleImport::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleImport::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   m_iConfirmationId = 0;
}

void MenuVehicleImport::onSelectItem()
{
   if ( m_SelectedIndex < 0 || m_SelectedIndex >= m_TempFilesCount )
      return;

   if ( ! m_bCreateNew )
   if ( (! pairing_isStarted()) || (! pairing_isReceiving()) )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Vehicle not running",NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine("You can not import the settings if the vehicle is not running and connected to this controller.");
      add_menu_to_stack(pm);
      return;
   }

   Model m;
   char szFile[1024];
   snprintf(szFile, sizeof(szFile), "%s/%s/%s",FOLDER_RUBY, FOLDER_USB_MOUNT, m_szTempFiles[m_SelectedIndex]);

   if ( ! m.loadFromFile(szFile, true) )
   {
      m_iConfirmationId = 0;
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Invalid file",NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine("This file is not a valid model settings file.");
      add_menu_to_stack(pm);
      return;
   }

   if ( m_bCreateNew )
   {
      Model* pModel = addNewModel();
      pModel->loadFromFile(szFile, true);
      pModel->is_spectator = false;
      saveModels();
      menu_close_all();
      hardware_unmount_usb();
      ruby_signal_alive();
      sync();
      ruby_signal_alive();
      return;
   }

   u8 pData[2048];
   int length = 0;
   FILE* fd = fopen(szFile, "rb");
   if ( NULL != fd )
   {
      length = fread(pData, 1, 2000, fd);
      fclose(fd);
   }
   else
   {
      log_error_and_alarm("Failed to load current vehicle configuration from file: %s", FILE_CURRENT_VEHICLE_MODEL);
      return;
   }
 
   fd = fopen("tmp/tempVehicleSettings.txt", "wb");
   if ( NULL != fd )
   {
       fwrite(pData, 1, length, fd);
       fclose(fd);
       fd = NULL;
   } 

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_ALL_PARAMS, 0, pData, length) )
      valuesToUI();
   else
   {
      menu_close_all();
      hardware_unmount_usb();
      ruby_signal_alive();
      sync();
      ruby_signal_alive();
   }
}

void MenuVehicleImport::buildSettingsFileList()
{
   DIR *d;
   struct dirent *dir;
   char szBuff[1024];
   char szFolder[1024];

   for( int i=0; i<m_TempFilesCount; i++ )
      free(m_szTempFiles[i]);
   m_TempFilesCount = 0;

   snprintf(szFolder, sizeof(szFolder), "%s/%s/",FOLDER_RUBY, FOLDER_USB_MOUNT);
   log_line("Searching for model settings files on: %s", szFolder);
   d = opendir(szFolder);
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < strlen("ruby_model") )
            continue;
         if ( strncmp(dir->d_name, "ruby_model", strlen("ruby_model")) != 0 )
            continue;
         if ( dir->d_name[strlen(dir->d_name)-3] != 't' )
            continue;
         if ( dir->d_name[strlen(dir->d_name)-2] != 'x' )
            continue;
         if ( dir->d_name[strlen(dir->d_name)-1] != 't' )
            continue;
         if ( m_TempFilesCount >= MAX_SETTINGS_IMPORT_FILES )
            continue;
         m_szTempFiles[m_TempFilesCount] = (char*)malloc(strlen(dir->d_name)+1);
         if ( NULL == m_szTempFiles[m_TempFilesCount] )
            break;
         log_line("Add model settings file to list: %s", dir->d_name);
         strcpy(m_szTempFiles[m_TempFilesCount], dir->d_name);
         m_TempFilesCount++;
      }
      closedir(d);
   }
   else
      log_softerror_and_alarm("Failed to enumerate files on the USB stick");
}

void MenuVehicleImport::addMessage(const char* szMessage)
{
   m_iConfirmationId = 0;
   Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Info",NULL);
   pm->m_xPos = 0.4; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->addTopLine(szMessage);
   add_menu_to_stack(pm);
}
