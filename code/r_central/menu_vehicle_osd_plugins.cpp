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
#include "../base/plugins_settings.h"
#include "../common/string_utils.h"
#include "menu.h"
#include "menu_vehicle_osd_plugins.h"
#include "menu_vehicle_osd_plugin.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"

#include "osd_plugins.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"

bool s_bMenuOSDPluginsPluginDeleted = false;

MenuVehicleOSDPlugins::MenuVehicleOSDPlugins(void)
:Menu(MENU_ID_VEHICLE_OSD_PLUGINS, "Plugins for OSD/Instruments/Gauges", NULL)
{
   m_Width = 0.22;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;

   char szBuff[256];
   snprintf(szBuff, sizeof(szBuff), "Plugins (%s)", str_get_osd_screen_name(g_pCurrentModel->osd_params.layout));
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

void MenuVehicleOSDPlugins::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   if ( s_bMenuOSDPluginsPluginDeleted )
   {
      readPlugins();
      s_bMenuOSDPluginsPluginDeleted = false;
      return;
   }

   if ( 1 == m_iConfirmationId && 1 == returnValue )
   {
      m_iConfirmationId = 0;
      importFromUSB();
      return;
   }
   
   m_iConfirmationId = 0;
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
      m_iConfirmationId = 1;
      MenuConfirmation* pMC = new MenuConfirmation("Import OSD Plugins","Insert an USB stick containing your plugins and then press Ok to start the import process.",m_iConfirmationId, true);
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

      snprintf(szFile, sizeof(szFile), "%s/%s", FOLDER_USB_MOUNT, dir->d_name);
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
         snprintf(szComm, sizeof(szComm), "cp -rf %s/%s %s", FOLDER_USB_MOUNT, dir->d_name, FOLDER_OSD_PLUGINS);
         hw_execute_bash_command(szComm, NULL);
         continue;
      }

      if ( NULL == strstr(dir->d_name, ".so") )
         continue;

      log_line("Found OSD plugin: [%s]", dir->d_name);
      snprintf(szComm, sizeof(szComm), "cp -rf %s/%s %s", FOLDER_USB_MOUNT, dir->d_name, FOLDER_OSD_PLUGINS);
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
   snprintf(szBuff, sizeof(szBuff), "Found and imported %d OSD plugins.", countImported);
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
