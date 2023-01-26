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
#include "../base/core_plugins_settings.h"
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
#include "menu_vehicle_management_plugins.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include <ctype.h>
#include "handle_commands.h"
#include "warnings.h"
#include "notifications.h"
#include "events.h"

MenuVehicleManagePlugins::MenuVehicleManagePlugins(void)
:Menu(MENU_ID_VEHICLE_MANAGE_PLUGINS, "Manage Vehicle Core Plugins", NULL)
{
   m_Width = 0.24;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;
   m_fIconSize = 1.0;
   m_pPopup = NULL;
   m_bWaitingForVehicleInfo = false;
   m_iCountPlugins = 0;
   m_IndexSelectedPlugin = -1;
}

MenuVehicleManagePlugins::~MenuVehicleManagePlugins()
{
   if ( NULL != m_pPopup )
   {
      popups_remove(m_pPopup);
      delete m_pPopup;
      m_pPopup = NULL;
   }
}

void MenuVehicleManagePlugins::onShow()
{
   m_Height = 0.0;
   m_ExtraHeight = 0;
   
   populateInfo();
   Menu::onShow();
}

void MenuVehicleManagePlugins::populateInfo()
{
   removeAllItems();
   removeAllTopLines();

   m_iCountPlugins = 0;
   m_IndexSelectedPlugin = -1;
   
   if ( 0 == g_iVehicleCorePluginsCount )
   {
      addTopLine("No core plugins installed on this vehicle.");
      char szBuff[256];
      if ( 0 == get_CorePluginsCount() )
         sprintf(szBuff, "No core plugins installed on this controller.");
      else if ( 1 == get_CorePluginsCount() )
         sprintf(szBuff, "1 core plugin available on this controller.");
      else
         sprintf(szBuff, "%d core plugins available on this controller.", get_CorePluginsCount());
      addTopLine(szBuff);
   }
   else
   {
      m_iCountPlugins = g_iVehicleCorePluginsCount;
      for( int i=0; i<m_iCountPlugins; i++ )
      {
         char* szName = g_listVehicleCorePlugins[i].szName;
         m_pItemsSelect[i] = new MenuItemSelect(szName, "Enables, disables or removes this plugin.");
         m_pItemsSelect[i]->addSelection("Disabled");
         m_pItemsSelect[i]->addSelection("Enabled");
         m_pItemsSelect[i]->addSelection("Delete");
         m_pItemsSelect[i]->setIsEditable();
         m_IndexPlugins[i] = addMenuItem(m_pItemsSelect[i]);

         if ( g_listVehicleCorePlugins[i].iEnabled )
            m_pItemsSelect[i]->setSelectedIndex(1);
         else
            m_pItemsSelect[i]->setSelectedIndex(0);
      }
   }

   m_IndexSync = addMenuItem(new MenuItem("Sync Plugins From Controller", "Gets all plugins from controller and install them on this vehicle."));
   
   if ( 0 == g_iVehicleCorePluginsCount && 0 == get_CorePluginsCount() )
      m_pMenuItems[m_IndexSync]->setEnabled(false);
}

void MenuVehicleManagePlugins::Render()
{
   RenderPrepare();

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


bool MenuVehicleManagePlugins::periodicLoop()
{
   static int s_iStaticMenuManagePluginsCounter = 0;
   s_iStaticMenuManagePluginsCounter++;

   if ( NULL == m_pPopup )
   {
      if ( ! m_bWaitingForVehicleInfo )
         return false;

      if ( ! handle_commands_is_command_in_progress() )
      {
         if ( handle_commands_has_received_vehicle_core_plugins_info() )
         {
            m_bWaitingForVehicleInfo = false;
            populateInfo();
            Popup* p = new Popup("Core plugins uploaded.", 0.3, 0.3, 0.5, 4 );
            popups_add_topmost(p);      
         }
      }
      return false;
   }

   char szBuff[128];
   sprintf(szBuff, "Uploading core plugins. Please wait..");
   for( int i=0; i<(s_iStaticMenuManagePluginsCounter%3); i++ )
      strcat(szBuff, ".");
   m_pPopup->setTitle(szBuff);

   if ( g_bHasFileUploadInProgress )
      return true;

   popups_remove(m_pPopup);
   delete m_pPopup;
   m_pPopup = NULL;

   if ( g_CurrentUploadingFile.uTotalSegments == 0 )
   {
      Popup* p = new Popup("Failed to upload plugins. Error from vehicle.", 0.3, 0.3, 0.5, 4 );
      popups_add_topmost(p);
   }
   else
   {
      handle_commands_reset_has_received_vehicle_core_plugins_info();
      m_bWaitingForVehicleInfo = false;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_GET_CORE_PLUGINS_INFO, 0, NULL, 0) )
      {
         Popup* p = new Popup("Core plugins uploaded.", 0.3, 0.3, 0.5, 4 );
         popups_add_topmost(p);
   
         valuesToUI();
      }
      else
         m_bWaitingForVehicleInfo = true;
   }

   valuesToUI();
      
   return false;
}

int MenuVehicleManagePlugins::onBack()
{
   if ( NULL == m_pPopup )
      return Menu::onBack();
   return 1;
}
     

void MenuVehicleManagePlugins::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   if ( 2 == m_iConfirmationId && 1 == returnValue && -1 != m_IndexSelectedPlugin )
   {
      m_iConfirmationId = 0;
      for( int i=m_IndexSelectedPlugin; i<g_iVehicleCorePluginsCount; i++ )
         memcpy((u8*)&(g_listVehicleCorePlugins[i]), (u8*)&(g_listVehicleCorePlugins[i+1]), sizeof(CorePluginSettings));
      g_iVehicleCorePluginsCount--;

      populateInfo();
      valuesToUI();
      return;
   }
   
   m_iConfirmationId = 0;
}


void MenuVehicleManagePlugins::onSelectItem()
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

   if ( m_IndexSync == m_SelectedIndex )
   {
      char szComm[256];

      strcpy(szComm, "rm -rf tmp/core_plugins.zip 2>&1");
      hw_execute_bash_command(szComm, NULL);

      strcpy(szComm, "zip tmp/core_plugins.zip plugins/core/*");
      hw_execute_bash_command(szComm, NULL);

      if ( NULL != m_pPopup )
      {
         popups_remove(m_pPopup);
         delete m_pPopup;
      }
      m_pPopup = new Popup("Uploading core plugins. Please wait...", 0.3, 0.3, 0.5, 200 );
      popups_add_topmost(m_pPopup);
      
      handle_commands_initiate_file_upload(FILE_ID_CORE_PLUGINS_ARCHIVE, "tmp/core_plugins.zip");
   }

   for( int i=0; i<m_iCountPlugins; i++ )
   {
      if ( m_SelectedIndex != m_IndexPlugins[i] )
         continue;
      m_IndexSelectedPlugin = i;
      int iAction = m_pItemsSelect[i]->getSelectedIndex();
      if ( iAction == 0 || iAction == 1 )
      {
         if ( iAction == 0 )
            g_listVehicleCorePlugins->iEnabled = 0;
         if ( iAction == 1 )
            g_listVehicleCorePlugins->iEnabled = 1;
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
}

