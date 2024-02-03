/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
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
#include "../../base/plugins_settings.h"
#include "menu.h"
#include "menu_vehicle_osd_plugins.h"
#include "menu_vehicle_osd_plugin.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"

#include "../osd/osd_plugins.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>


bool s_bMenuOSDPluginsPluginDeleted = false;

MenuVehicleOSDPlugins::MenuVehicleOSDPlugins(void)
:Menu(MENU_ID_VEHICLE_OSD_PLUGINS, "Plugins for OSD/Instruments/Gauges", NULL)
{
   m_Width = 0.22;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;

   char szBuff[256];
   sprintf(szBuff, "Plugins (%s)", str_get_osd_screen_name(g_pCurrentModel->osd_params.layout));
   setTitle(szBuff);
   
   readPlugins();
}

MenuVehicleOSDPlugins::~MenuVehicleOSDPlugins()
{
}

void MenuVehicleOSDPlugins::readPlugins()
{
   removeAllItems();

   for( int i=0; i<osd_plugins_get_count(); i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;

      char* szName = osd_plugins_get_name(i);
      addMenuItem(new MenuItem(szName, "Configure this plugin"));
   }
   
   //addMenuItem(new MenuItemSection("Manage Plugins"));
   //m_IndexImport = addMenuItem(new MenuItem("Import Plugins", "Import new OSD plugins from a USB memory stick."));
   //m_pMenuItems[m_IndexImport]->showArrow();
}

void MenuVehicleOSDPlugins::valuesToUI()
{
}

void MenuVehicleOSDPlugins::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleOSDPlugins::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( s_bMenuOSDPluginsPluginDeleted )
   {
      readPlugins();
      s_bMenuOSDPluginsPluginDeleted = false;
      return;
   }

   if ( (1 == iChildMenuId/1000) && (1 == returnValue) )
   {
      importFromUSB();
      return;
   }
}


void MenuVehicleOSDPlugins::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( m_SelectedIndex < osd_plugins_get_count() )
   {
      MenuVehicleOSDPlugin* pMenu = new MenuVehicleOSDPlugin(m_SelectedIndex);
      add_menu_to_stack(pMenu);
      return;
   }

   /*
   if ( m_IndexImport == m_SelectedIndex )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Import OSD Plugins","Insert an USB stick containing your plugins and then press Ok to start the import process.",1, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }  
   */
}

void MenuVehicleOSDPlugins::importFromUSB()
{
   if ( ! hardware_try_mount_usb() )
   {
      addMessage("No USB memory stick detected. Please insert a USB stick");
      return;
   }

   DIR *d;
   FILE* fd;
   struct dirent *dir;
   char szFile[1024];
   char szComm[1024];
   log_line("Searching for plugins...");
   
   d = opendir(FOLDER_USB_MOUNT);
   if (!d)
   {
      log_softerror_and_alarm("Failed to open USB mount dir to search for OSD plugins.");
      addMessage("Failed to read the USB memory stick.");
      hardware_unmount_usb();
      return;
   }

   int countImported = 0;

   while ((dir = readdir(d)) != NULL)
   {
      if ( strlen(dir->d_name) < 4 )
         continue;

      sprintf(szFile, "%s/%s", FOLDER_USB_MOUNT, dir->d_name);
      long lSize = 0;
      fd = fopen(szFile, "rb");
      if ( NULL != fd )
      {
         fseek(fd, 0, SEEK_END);
         lSize = ftell(fd);
         fseek(fd, 0, SEEK_SET);
         fclose(fd);
      }
      else
         log_softerror_and_alarm("Failed to open file [%s] for checking it's size.", szFile);
      if ( lSize < 3 )
         continue;

      if ( NULL != strstr(dir->d_name, ".png") )
      {
         sprintf(szComm, "cp -rf %s/%s %s", FOLDER_USB_MOUNT, dir->d_name, FOLDER_OSD_PLUGINS);
         hw_execute_bash_command(szComm, NULL);
         continue;
      }

      if ( NULL == strstr(dir->d_name, ".so") )
         continue;

      log_line("Found OSD plugin: [%s]", dir->d_name);
      sprintf(szComm, "cp -rf %s/%s %s", FOLDER_USB_MOUNT, dir->d_name, FOLDER_OSD_PLUGINS);
      hw_execute_bash_command(szComm, NULL);
      countImported++;
   }
   closedir(d);
   log_line("Searching for OSD plugins complete.");
   hardware_unmount_usb();

   if ( 0 == countImported )
   {
      addMessage("No OSD plugins found.");
      hardware_unmount_usb();
      return;
   }
   char szBuff[256];
   sprintf(szBuff, "Found and imported %d OSD plugins.", countImported);
   addMessage(szBuff);
   hardware_unmount_usb();
   osd_plugins_load();

   for( int i=osd_plugins_get_count()-countImported; i<osd_plugins_get_count(); i++ )
   {
      if ( i < 0 || i >= MAX_OSD_PLUGINS )
         break;
      if ( NULL == g_pCurrentModel )
         break;
      int layoutIndex = g_pCurrentModel->osd_params.layout;
      u32 flags = g_pCurrentModel->osd_params.instruments_flags[layoutIndex];
      g_pCurrentModel->osd_params.instruments_flags[layoutIndex] |= (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i);
   }
   readPlugins();
}
