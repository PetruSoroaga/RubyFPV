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
#include "colors.h"
#include "../renderer/render_engine.h"
#include "render_commands.h"
#include "osd.h"
#include "osd_common.h"
#include "popup.h"
#include "menu.h"
#include "menu_vehicle_selector.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include "events.h"
#include "notifications.h"
#include "warnings.h"


MenuVehicleSelector::MenuVehicleSelector(void)
:Menu(MENU_ID_VEHICLE_SELECTOR, "Vehicle Actions", NULL)
{
   m_Width = 0.16;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.18;
}

MenuVehicleSelector::~MenuVehicleSelector()
{
}

void MenuVehicleSelector::onShow()
{
   m_Height = 0.0;
   m_ExtraHeight = 0;
   
   removeAllItems();

   Model *pModel = getModelAtIndex(m_IndexSelectedVehicle);
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("NULL model for vehicle index %d.", m_IndexSelectedVehicle);
      addMenuItem(new MenuItemText("Can't get vehicle info!"));
      return;
   }

   char szBuff[256];
   snprintf(szBuff, sizeof(szBuff), "%s Actions", pModel->getLongName());
   setTitle(szBuff);
   m_IndexSelect = addMenuItem(new MenuItem("Select","Make this vehicle the active one."));
   m_IndexDelete = addMenuItem(new MenuItem("Delete","Deletes this vehicle."));
  
   Menu::onShow();
}

void MenuVehicleSelector::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleSelector::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   Model *pModel = getModelAtIndex(m_IndexSelectedVehicle);
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("NULL model for vehicle index %d.", m_IndexSelectedVehicle);
      return;
   }

   // Delete model
   if ( (1 == returnValue) && (1 == m_iConfirmationId) )
   {
      deleteModel(pModel);
      deletePluginModelSettings(pModel->vehicle_id);
      save_PluginsSettings();
      log_line("Menu Vehicle Selector: Deleted model 1/2.");
      notification_add_model_deleted();
      m_iConfirmationId = 0;
      m_IndexSelectedVehicle = -1;
      menu_refresh_all_menus();
      menu_stack_pop(0);
      log_line("Menu Vehicle Selector: Deleted model 2/2.");
      return;
   }

   m_iConfirmationId = 0;
}


void MenuVehicleSelector::onSelectItem()
{
   Model *pModel = getModelAtIndex(m_IndexSelectedVehicle);
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("NULL model for vehicle index %d.", m_IndexSelectedVehicle);
      return;
   }

   if ( m_IndexDelete == m_SelectedIndex )
   {
      if ( (NULL != g_pCurrentModel) && (pModel->vehicle_id == g_pCurrentModel->vehicle_id) )
      if ( checkIsArmed() )
         return;
      char szBuff[64];
      snprintf(szBuff, sizeof(szBuff), "Are you sure you want to delete %s?", g_pCurrentModel->getLongName());
      add_menu_to_stack(new MenuConfirmation("Confirmation",szBuff, 1));
      m_iConfirmationId = 1;
      return;
   }

   if ( m_IndexSelect == m_SelectedIndex )
   {
      if ( NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) && g_pCurrentModel->vehicle_id == pModel->vehicle_id )
      {
         menu_stack_pop();
         return;
      }

      menu_close_all();
      warnings_remove_all();
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
      onMainVehicleChanged();
      pairing_start(); 
   }
}
