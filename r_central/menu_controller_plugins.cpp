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
#include "../base/core_plugins_settings.h"

#include "menu.h"
#include "menu_controller_plugins.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"

#include "osd_plugins.h"
#include "process_router_messages.h"

#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"


MenuControllerPlugins::MenuControllerPlugins(void)
:Menu(MENU_ID_CONTROLLER_PLUGINS, "Controller Plugins", NULL)
{
   m_Width = 0.24;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;

   readPlugins();
}

MenuControllerPlugins::~MenuControllerPlugins()
{
}

void MenuControllerPlugins::readPlugins()
{
   removeAllItems();
   m_SelectedIndex = 0;

   addMenuItem(new MenuItemSection("Core Plugins"));

   m_iCountCorePlugins = get_CorePluginsCount();

   if ( 0 == m_iCountCorePlugins )
      addMenuItem( new MenuItemText("No core plugins installed on this controller.") );
   else
   {
      for( int i=0; i<m_iCountCorePlugins; i++ )
      {
         char* szName = get_CorePluginName(i);
         m_pItemsSelectCore[i] = new MenuItemSelect(szName, "Enables, disables or removes this plugin");
         m_pItemsSelectCore[i]->addSelection("Disabled");
         m_pItemsSelectCore[i]->addSelection("Enabled");
         m_pItemsSelectCore[i]->addSelection("Delete");
         m_pItemsSelectCore[i]->setIsEditable();
         m_IndexCorePlugins[i] = addMenuItem(m_pItemsSelectCore[i]);
      }
   }
   addMenuItem(new MenuItemSection("OSD Plugins"));

   m_iCountOSDPlugins = osd_plugins_get_count();

   for( int i=0; i<m_iCountOSDPlugins; i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;

      char* szName = osd_plugins_get_name(i);
      m_pItemsSelect[i] = new MenuItemSelect(szName, "Enables, disables or removes this plugin");
      m_pItemsSelect[i]->addSelection("Disabled");
      m_pItemsSelect[i]->addSelection("Enabled");
      m_pItemsSelect[i]->addSelection("Delete");
      m_pItemsSelect[i]->setIsEditable();
      m_IndexOSDPlugins[i] = addMenuItem(m_pItemsSelect[i]);
   }

   if ( 0 == m_iCountOSDPlugins )
   {
      addMenuItem(new MenuItemText("No plugins installed on the system."));
   }

   addMenuItem(new MenuItemSection("Manage Plugins"));

   m_IndexImport = addMenuItem(new MenuItem("Import Plugins", "Import new plugins from a USB memory stick."));
   m_pMenuItems[m_IndexImport]->showArrow();
   m_IndexSelectedPlugin = -1;
}

void MenuControllerPlugins::valuesToUI()
{
   PluginsSettings* pPS = get_PluginsSettings();

   for( int i=0; i<m_iCountOSDPlugins; i++ )
   {
      if ( i >= MAX_OSD_PLUGINS )
         break;

      char* szUID = osd_plugins_get_uid(i);
      SinglePluginSettings* pPlugin = pluginsGetByUID(szUID);
      if ( NULL == pPlugin )
         continue;

      if ( pPlugin->nEnabled )
         m_pItemsSelect[i]->setSelectedIndex(1);
      else
         m_pItemsSelect[i]->setSelectedIndex(0);
   }

   for( int i=0; i<m_iCountCorePlugins; i++ )
   {
      if ( i >= MAX_CORE_PLUGINS_COUNT )
         break;

      char* szGUID = get_CorePluginGUID(i);
      CorePluginSettings* pSettings = get_CorePluginSettings(szGUID);
      if ( NULL == pSettings )
         continue;

      if ( pSettings->iEnabled )
         m_pItemsSelectCore[i]->setSelectedIndex(1);
      else
         m_pItemsSelectCore[i]->setSelectedIndex(0);
   }
}

void MenuControllerPlugins::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuControllerPlugins::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   if ( 2 == m_iConfirmationId && 1 == returnValue && -1 != m_IndexSelectedPlugin )
   {
      m_iConfirmationId = 0;
      char* szUID = osd_plugins_get_uid(m_IndexSelectedPlugin);
      delete_PluginByUID(szUID);
      save_PluginsSettings();
      osd_plugins_delete(m_IndexSelectedPlugin);
      readPlugins();
      valuesToUI();
      return;
   }
   
   if ( 5 == m_iConfirmationId && 1 == returnValue && -1 != m_IndexSelectedPlugin )
   {
      m_iConfirmationId = 0;
      char* szGUID = get_CorePluginGUID(m_IndexSelectedPlugin);
      delete_CorePlugin(szGUID);
      readPlugins();
      valuesToUI();

      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
      PH.packet_type = PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS;
      PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
      PH.vehicle_id_dest = PACKET_COMPONENT_COMMANDS;
      PH.total_headers_length = sizeof(t_packet_header);
      PH.total_length = sizeof(t_packet_header);
      PH.extra_flags = 0;
      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      packet_compute_crc(buffer, PH.total_length);
      send_packet_to_router(buffer, PH.total_length);
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


void MenuControllerPlugins::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( (m_iCountOSDPlugins > 0) && (m_SelectedIndex >= m_IndexOSDPlugins[0]) && (m_SelectedIndex <= m_IndexOSDPlugins[m_iCountOSDPlugins-1]) )
   {
      m_IndexSelectedPlugin = m_SelectedIndex - m_IndexOSDPlugins[0];
      int iAction = m_pItemsSelect[m_SelectedIndex]->getSelectedIndex();
      if ( iAction == 0 || iAction == 1 )
      {
         char* szUID = osd_plugins_get_uid(m_IndexSelectedPlugin);
         SinglePluginSettings* pPlugin = pluginsGetByUID(szUID);
         if ( NULL == pPlugin )
            return;

         if ( iAction == 0 )
            pPlugin->nEnabled = 0;
         if ( iAction == 1 )
            pPlugin->nEnabled = 1;
         save_PluginsSettings();
         valuesToUI();
         return;
      }
      if ( iAction == 2 )
      {
         m_iConfirmationId = 2;
         MenuConfirmation* pMC = new MenuConfirmation("Delete Plugin","Are you sure you want to delete this plugin?",m_iConfirmationId);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
         return;
      }
      return;
   }

   if ( (m_iCountCorePlugins > 0) && (m_SelectedIndex >= m_IndexCorePlugins[0]) && (m_SelectedIndex <= m_IndexCorePlugins[m_iCountCorePlugins-1]) )
   {
      m_IndexSelectedPlugin = m_SelectedIndex - m_IndexCorePlugins[0];
      int iAction = m_pItemsSelectCore[m_IndexSelectedPlugin]->getSelectedIndex();
      if ( iAction == 0 || iAction == 1 )
      {
         CorePluginSettings* pSettings = get_CorePluginSettings(get_CorePluginGUID(m_IndexSelectedPlugin));
         if ( NULL == pSettings )
            return;

         if ( iAction == 0 )
            pSettings->iEnabled = 0;
         if ( iAction == 1 )
            pSettings->iEnabled = 1;
         save_CorePluginsSettings();
         valuesToUI();
         return;
      }
      if ( iAction == 2 )
      {
         m_iConfirmationId = 5;
         MenuConfirmation* pMC = new MenuConfirmation("Delete Plugin","Are you sure you want to delete this plugin?",m_iConfirmationId);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
         return;
      }
      return;
   }

   if ( m_IndexImport == m_SelectedIndex )
   {
      m_iConfirmationId = 1;
      MenuConfirmation* pMC = new MenuConfirmation("Import Plugins","Insert an USB stick containing your plugins and then press Ok to start the import process.",m_iConfirmationId, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return;
   }
   
}

void MenuControllerPlugins::importFromUSB()
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
      log_softerror_and_alarm("Failed to open USB mount dir to search for plugins.");
      addMessage("Failed to read the USB memory stick.");
      hardware_unmount_usb();
      return;
   }

   int countImportedTotal = 0;
   int countImportedOSD = 0;
   int countImportedCore = 0;

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

      sprintf(szFile, "%s/%s", FOLDER_USB_MOUNT, dir->d_name);
      void* pLibrary = NULL;
      pLibrary = dlopen(szFile, RTLD_LAZY | RTLD_GLOBAL);
      if ( NULL == pLibrary )
      {
         log_softerror_and_alarm("Invalid plugin library on USB stick: %s. Skipped.", dir->d_name);
         continue;
      }

      void (*pFunctionOSDInit)(void*);
      void (*pFunctionOSDRender)(vehicle_and_telemetry_info_t*, plugin_settings_info_t2*, float, float, float, float);
      char* (*pFunctionOSDGetName)(void);
      char* (*pFunctionOSDGetUID)(void);
   
      pFunctionOSDInit = (void (*)(void*)) dlsym(pLibrary, "init");
      pFunctionOSDRender = (void (*)(vehicle_and_telemetry_info_t*, plugin_settings_info_t2*, float, float, float, float)) dlsym(pLibrary, "render");
      pFunctionOSDGetName = (char* (*)(void)) dlsym(pLibrary, "getName");
      pFunctionOSDGetUID = (char* (*)(void)) dlsym(pLibrary, "getUID");
      
      if ( NULL != pFunctionOSDInit && NULL != pFunctionOSDRender && NULL != pFunctionOSDGetName && NULL != pFunctionOSDGetUID )
      {
         dlclose(pLibrary);
         sprintf(szComm, "cp -rf %s/%s %s", FOLDER_USB_MOUNT, dir->d_name, FOLDER_OSD_PLUGINS);
         hw_execute_bash_command(szComm, NULL);
      
         log_line("Found OSD plugin: [%s]", dir->d_name);
         countImportedOSD++;
         countImportedTotal++;
         continue;
      }

      int (*pFunctionCoreInit)(u32, u32);
      u32 (*pFunctionCoreRequestCapab)(void);
      const char* (*pFunctionCoreGetName)(void);
      const char* (*pFunctionCoreGetUID)(void);
   
      pFunctionCoreInit = (int (*)(u32, u32)) dlsym(pLibrary, "core_plugin_init");
      pFunctionCoreRequestCapab = (u32 (*)(void)) dlsym(pLibrary, "core_plugin_on_requested_capabilities");
      pFunctionCoreGetName = (const char* (*)(void)) dlsym(pLibrary, "core_plugin_get_name");
      pFunctionCoreGetUID = (const char* (*)(void)) dlsym(pLibrary, "core_plugin_get_guid");

      if ( NULL != pFunctionCoreInit && NULL != pFunctionCoreRequestCapab && NULL != pFunctionCoreGetName && NULL != pFunctionCoreGetUID )
      {
         dlclose(pLibrary);
         sprintf(szComm, "cp -rf %s/%s %s", FOLDER_USB_MOUNT, dir->d_name, FOLDER_CORE_PLUGINS);
         hw_execute_bash_command(szComm, NULL);
      
         log_line("Found Core plugin: [%s]", dir->d_name);
         countImportedCore++;
         countImportedTotal++;
         continue;
      }
      dlclose(pLibrary);
   }
   
   closedir(d);

   log_line("Searching for plugins complete.");
   hardware_unmount_usb();

   if ( 0 == countImportedTotal )
   {
      addMessage("No plugins found.");
      hardware_unmount_usb();
      return;
   }
   char szBuff[128];
   if ( 1 == countImportedTotal )
   {
      if ( 1 == countImportedOSD )
         strcpy(szBuff, "Found and imported one OSD plugin.");
      else if ( 1 == countImportedCore )
         strcpy(szBuff, "Found and imported one core plugin.");
      else
         strcpy(szBuff, "Found and imported one plugin.");
   }
   else
   {
      if ( 0 < countImportedOSD && 0 < countImportedCore )
         sprintf(szBuff, "Found and imported %d plugins (%d OSD plugins, %d core plugins).", countImportedTotal, countImportedOSD, countImportedCore);
      else if ( 0 < countImportedOSD )
         sprintf(szBuff, "Found and imported %d plugins (%d OSD plugins).", countImportedTotal, countImportedOSD);
      else if ( 0 < countImportedCore )
         sprintf(szBuff, "Found and imported %d plugins (%d core plugins).", countImportedTotal, countImportedCore);
      else
         sprintf(szBuff, "Found and imported %d plugins.", countImportedTotal);
   }
   addMessage(szBuff);
   hardware_unmount_usb();

   if ( countImportedOSD > 0 )
   {
      osd_plugins_load();

      for( int i=osd_plugins_get_count()-countImportedOSD; i<osd_plugins_get_count(); i++ )
      {
         if ( i < 0 || i >= MAX_OSD_PLUGINS )
            break;
         if ( NULL == g_pCurrentModel )
            break;
         int layoutIndex = g_pCurrentModel->osd_params.layout;
         u32 flags = g_pCurrentModel->osd_params.instruments_flags[layoutIndex];
         g_pCurrentModel->osd_params.instruments_flags[layoutIndex] |= (INSTRUMENTS_FLAG_SHOW_FIRST_OSD_PLUGIN << i);
      }

      handle_commands_send_single_oneway_command(5, COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&(g_pCurrentModel->osd_params), sizeof(osd_parameters_t), 5);

      readPlugins();
   }

   if ( countImportedTotal > 0 )
      valuesToUI();

   if ( countImportedCore > 0 )
   {
      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
      PH.packet_type = PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS;
      PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
      PH.vehicle_id_dest = PACKET_COMPONENT_COMMANDS;
      PH.total_headers_length = sizeof(t_packet_header);
      PH.total_length = sizeof(t_packet_header);
      PH.extra_flags = 0;
      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      packet_compute_crc(buffer, PH.total_length);
      send_packet_to_router(buffer, PH.total_length);
   }
}
