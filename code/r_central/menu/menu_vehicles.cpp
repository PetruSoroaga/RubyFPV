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
#include "menu_vehicles.h"
#include "menu_confirmation.h"
#include "menu_vehicle_import.h"
#include "menu_vehicle_selector.h"

#include "../osd/osd_common.h"
#include "../../base/radio_utils.h"

const char* s_textTitle[] = { "My Vehicles",  NULL };
const char* s_szVehicleNone = "No vehicles defined. To add a vehicle:\n Search for a vehicle and then connect to it as controller.";

MenuVehicles::MenuVehicles(void)
:Menu(MENU_ID_VEHICLES, "My Vehicles", NULL)
{
   m_Width = 0.33;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;
   m_IndexSelectedVehicle = -1;

   log_line("MenuVehicles: On open, this is the current radio interfaces configuration:");
   hardware_load_radio_info();

   m_bDisableStacking = true;
   m_bDisableBackgroundAlpha = true;
}

void MenuVehicles::onShow()
{
   m_Height = 0.0;
   
   if ( (NULL != g_pCurrentModel) && ( 0 != g_uActiveControllerModelVID) )
      log_line("MenuVehicles: Current vehicle id: %u (%u)", g_pCurrentModel->vehicle_id, g_uActiveControllerModelVID);
   else
      log_line("MenuVehicles: No current vehicle.");
   removeAllTopLines();
   removeAllItems();

   m_IndexSelectedVehicle = -1;

   addTopLine("Select the vehicle to control:");

   for( int i=0; i<getControllerModelsCount(); i++ )
   {
      Model *p = getModelAtIndex(i);
      log_line("MenuVehicles: Iterating vehicles: id: %u", p->vehicle_id);
      char szBuff[256];
      if ( 1 == p->radioLinksParams.links_count )
         sprintf(szBuff, "%s, %s", p->getLongName(), str_format_frequency(p->radioLinksParams.link_frequency_khz[0]));
      else if ( 2 == p->radioLinksParams.links_count )
      {
         char szFreq1[64];
         char szFreq2[64];
         strcpy(szFreq1, str_format_frequency(p->radioLinksParams.link_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(p->radioLinksParams.link_frequency_khz[1]));
         sprintf(szBuff, "%s, %s/%s", p->getLongName(), szFreq1, szFreq2);
      }
      else
      {
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];
         strcpy(szFreq1, str_format_frequency(p->radioLinksParams.link_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(p->radioLinksParams.link_frequency_khz[1]));
         strcpy(szFreq3, str_format_frequency(p->radioLinksParams.link_frequency_khz[2]));
         sprintf(szBuff, "%s, %s/%s/%s", p->getLongName(), szFreq1, szFreq2, szFreq3);
      }

      MenuItemVehicle* pItem = new MenuItemVehicle(szBuff);
      pItem->setVehicleIndex(i);
      addMenuItem( pItem );
      if ( (NULL != g_pCurrentModel) && (!g_pCurrentModel->is_spectator) )
      if ( (g_uActiveControllerModelVID == p->vehicle_id) && (g_pCurrentModel->vehicle_id == p->vehicle_id) )
      {
         log_line("MenuVehicles: Found current vehicle in the list.");
      }
   }
   if ( 0 == getControllerModelsCount() )
   {
      removeAllTopLines();
      m_ExtraItemsHeight += 1.2*g_pRenderEngine->getMessageHeight(s_szVehicleNone, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu );
      m_ExtraItemsHeight += g_pRenderEngine->textHeight(g_idFontMenu);
   }
   else
      m_ExtraItemsHeight += g_pRenderEngine->textHeight(g_idFontMenu);
   m_IndexImport = addMenuItem(new MenuItem("Import Vehicle", "Imports a new vehicle from a model file on a USB stick."));

   Menu::onShow();
}


void MenuVehicles::onReturnFromChild(int returnValue)
{
   invalidate();
   Menu::onReturnFromChild(returnValue);
}

void MenuVehicles::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float xPos = m_RenderXPos + m_sfMenuPaddingX;

   if ( 0 == getControllerModelsCount() )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      float h = g_pRenderEngine->drawMessageLines(xPos, y, s_szVehicleNone, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu );
      y += 1.2*h;
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

   if ( 0 == getControllerModelsCount() )
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

   m_IndexSelectedVehicle = m_SelectedIndex;
   log_line("Adding menu for vehicle index %d", m_IndexSelectedVehicle);
   
   MenuVehicleSelector* pMenu = new MenuVehicleSelector();
   pMenu->m_IndexSelectedVehicle = m_SelectedIndex;
   pMenu->m_yPos = m_pMenuItems[m_SelectedIndex]->getItemRenderYPos() - g_pRenderEngine->textHeight(g_idFontMenu);
   add_menu_to_stack(pMenu);
}
