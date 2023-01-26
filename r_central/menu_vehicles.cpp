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
#include "menu.h"
#include "menu_vehicles.h"
#include "menu_confirmation.h"
#include "menu_vehicle_import.h"

#include "../base/config.h"
#include "../base/launchers.h"
#include "../base/commands.h"

#include "shared_vars.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "osd_common.h"
#include "pairing.h"
#include "ruby_central.h"
#include "local_stats.h"
#include "events.h"

const char* s_textTitle[] = { "My Vehicles",  NULL };
const char* s_szVehicleNone = "No vehicles defined. To add a vehicle:\n Search for a vehicle and then connect to it as controller.";

MenuVehicles::MenuVehicles(void)
:Menu(MENU_ID_VEHICLES, "My Vehicles", NULL)
{
   m_Width = 0.24;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;
}

void MenuVehicles::onShow()
{
   m_Height = 0.0;
   m_ExtraHeight = 0.0;

   if ( NULL != g_pCurrentModel )
      log_line("MenuVehicles: Current vehicle id: %u", g_pCurrentModel->vehicle_id);
   else
      log_line("MenuVehicles: No current vehicle.");
   removeAllTopLines();
   removeAllItems();
   addTopLine("Select the vehicle to control:");

   for( int i=0; i<getModelsCount(); i++ )
   {
      Model *p = getModelAtIndex(i);
      log_line("MenuVehicles: Iterating vehicle id: %u", p->vehicle_id);
      char szBuff[256];
      sprintf(szBuff, "%s, %d Mhz", p->getLongName(), p->radioLinksParams.link_frequency[0]);
      if ( 1 < p->radioLinksParams.links_count )
         sprintf(szBuff, "%s, %d/%d Mhz", p->getLongName(), p->radioLinksParams.link_frequency[0], p->radioLinksParams.link_frequency[1]);

      MenuItemVehicle* pItem = new MenuItemVehicle(szBuff);
      pItem->setVehicleIndex(i);
      addMenuItem( pItem );
      if ( NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) && g_pCurrentModel->vehicle_id == p->vehicle_id )
      {
         log_line("MenuVehicles: Found current vehicle in the list.");
      }
   }
   if ( 0 == getModelsCount() )
   {
      removeAllTopLines();
      m_ExtraHeight += 1.3*g_pRenderEngine->getMessageHeight(s_szVehicleNone, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu );
      m_ExtraHeight += g_pRenderEngine->textHeight(0,g_idFontMenu);
   }
   else
      m_ExtraHeight += g_pRenderEngine->textHeight(0,g_idFontMenu);
   m_IndexImport = addMenuItem(new MenuItem("Import Vehicle", "Imports a new vehicle from a model file on a USB stick."));

   Menu::onShow();
}


void MenuVehicles::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

void MenuVehicles::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   float height_text = g_pRenderEngine->textHeight(0,g_idFontMenu);
   float xPos = m_RenderXPos + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();

   if ( 0 == getModelsCount() )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      float h = g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus(), y, s_szVehicleNone, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu );
      y += h+height_text;
   }

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_IndexImport == i )
      {
         y += height_text;
         y += RenderItem(i,y);
         continue;
      }
      y += RenderItem(i,y);
   }
   RenderEnd(yTop);
}


void MenuVehicles::onSelectItem()
{
   if ( m_IndexImport == m_SelectedIndex )
   {
      if ( ! hardware_try_mount_usb() )
      {
         log_line("No USB memory stick available.");
         Popup* p = new Popup("Please insert a USB memory stick to import the vehicle from.",0.28, 0.32, 0.32, 3);
         p->setCentered();
         p->setIconId(g_idIconInfo, get_Color_IconWarning());
         popups_add_topmost(p);
         return;
      }

      MenuVehicleImport* pM = new MenuVehicleImport();
      pM->setCreateNew();
      pM->buildSettingsFileList();
      if ( 0 == pM->getSettingsFilesCount() )
      {
         hardware_unmount_usb();
         ruby_signal_alive();
         sync();
         ruby_signal_alive();
         delete pM;
         m_iConfirmationId = 10;
         Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"No vehicle settings files",NULL);
         pm->m_xPos = 0.4; pm->m_yPos = 0.4;
         pm->m_Width = 0.36;
         pm->addTopLine("There are no vehicle settings files on the USB stick.");
         add_menu_to_stack(pm);
         return;
      }
      add_menu_to_stack(pM);
      return;
   }
   if ( 0 == getModelsCount() )
   {
      menu_stack_pop();
      return;
   }
   Model *pModel = getModelAtIndex(m_SelectedIndex);
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("NULL model for vehicle: %s.", m_pMenuItems[m_SelectedIndex]->getTitle());
      return;
   }
   if ( NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) && g_pCurrentModel->vehicle_id == pModel->vehicle_id )
   {
      menu_stack_pop();
      return;
   }
   menu_close_all();
   render_all(get_current_timestamp_ms(), true);
   Popup* p = new Popup("Switching vehicles...",0.3,0.64, 0.26, 0.2);
   popups_add_topmost(p);
   render_all(get_current_timestamp_ms(), true);
         
   pairing_stop();

   saveModel(g_pCurrentModel);

   hardware_sleep_ms(100);
   g_bIsFirstConnectionToCurrentVehicle = true;
   pModel->is_spectator = false;
   g_pCurrentModel = pModel;
   g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, true);
   onNewVehicleActive();
   pairing_start(); 
}
