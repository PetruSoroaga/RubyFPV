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
#include "../base/ctrl_settings.h"
#include "render_commands.h"
#include "osd.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "menu.h"
#include "menu_vehicle.h"
#include "menu_vehicle_general.h"
//#include "menu_vehicle_radio.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_osd.h"
#include "menu_vehicle_expert.h"
#include "menu_confirmation.h"
#include "menu_vehicle_telemetry.h"
#include "menu_vehicle_relay.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include <ctype.h>
#include "handle_commands.h"

MenuVehicleRelay::MenuVehicleRelay(void)
:Menu(MENU_ID_VEHICLE_RELAY, "Relay Settings", NULL)
{
   m_Width = 0.4;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.4;
   m_bIsConfigurable = false;
}

MenuVehicleRelay::~MenuVehicleRelay()
{
}

void MenuVehicleRelay::onShow()
{
   m_Height = 0.0;
   m_ExtraHeight = 0.07;
   
   removeAllItems();
   removeAllTopLines();

   m_pItemsSelect[0] = new MenuItemSelect("Enable Relay Mode", "Enables or disables the relay functionality on this vehicle (this vehicle will relay data/video from another vehicle).");  
   m_pItemsSelect[0]->addSelection("Disabled");
   m_pItemsSelect[0]->addSelection("Enabled");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexItemEnabled = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Relay Switching", "Selects the QA button used to switch between this vehicle and the relayed one.");  
   m_pItemsSelect[1]->addSelection("Disabled");
   m_pItemsSelect[1]->addSelection("QA Button 1");
   m_pItemsSelect[1]->addSelection("QA Button 2");
   m_pItemsSelect[1]->addSelection("QA Button 3");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexQAButton = addMenuItem(m_pItemsSelect[1]);

   
   m_pItemsSelect[2] = new MenuItemSelect("Relay Mode", "Sets what data to be relayed.");  
   m_pItemsSelect[2]->addSelection("All");
   m_pItemsSelect[2]->addSelection("Video Only");
   m_pItemsSelect[2]->addSelection("Video & Telemetry");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexItemType = addMenuItem(m_pItemsSelect[2]);


   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      log_line("Vehicle interface %d supported bands: %d", i, g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i]);
      
   int countCardsOk = 0;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] )
         countCardsOk++;

   m_bIsConfigurable = true;
   if ( countCardsOk < 2 )
   {
      m_bIsConfigurable = false;
      float iconSize = 4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS;
      addTopLine("You can not enalbe relay functionality on this vehicle!", iconSize);
      addTopLine("You need at least two radio interfaces on the vehicle to be able to relay from this vehicle.", iconSize);
      disableRelayUI();
   }
   
   Menu::onShow();
}


void MenuVehicleRelay::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pP = get_Preferences();
   
   if ( NULL == pCS || NULL == pP )
      return;

   m_pItemsSelect[0]->setSelection(g_pCurrentModel->relay_params.is_RelayEnabled);
   m_pItemsSelect[2]->setSelection(1);

   m_pItemsSelect[1]->setSelection(0);
   if ( pCS->iQAButtonRelaySwitching >= 0 )
       m_pItemsSelect[1]->setSelectedIndex(pCS->iQAButtonRelaySwitching+1);

   int countCardsOk = 0;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] )
         countCardsOk++;

   m_bIsConfigurable = true;
   if ( countCardsOk < 2 )
   {
      m_bIsConfigurable = false;
      disableRelayUI();
   }
}

void MenuVehicleRelay::disableRelayUI()
{
   m_pItemsSelect[0]->setSelection(0);
   m_pMenuItems[m_IndexItemEnabled]->setEnabled(false);
   m_pMenuItems[m_IndexQAButton]->setEnabled(false);
   m_pMenuItems[m_IndexItemType]->setEnabled(false);
   m_bIsConfigurable = false;
}

void MenuVehicleRelay::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();   
   float y = yTop + m_ExtraHeight;

   //float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();
   //float iconSize = 3 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS;
   //float yIcon = m_yPos+MENU_MARGINS*menu_getScaleMenus()*(1+MENU_TEXTLINE_SPACING);
   //yIcon += g_pRenderEngine->getMessageHeight(m_szTitle, menu_getScaleMenus()*MENU_FONT_SIZE_TITLE, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   //yIcon += menu_getScaleMenus()*MENU_FONT_SIZE_TITLE * MENU_TEXTLINE_SPACING;
   //g_pRenderEngine->setColors(get_Color_MenuText());
      
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleRelay::onSelectItem()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pP = get_Preferences();
   
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( NULL == pCS || NULL == pP )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexItemEnabled == m_SelectedIndex )
   {
   }

   if ( m_IndexQAButton == m_SelectedIndex )
   {
      int iOldButton = pCS->iQAButtonRelaySwitching;
      pCS->iQAButtonRelaySwitching = m_pItemsSelect[1]->getSelectedIndex()-1;

      if ( iOldButton == pCS->iQAButtonRelaySwitching )
         return;

      if ( iOldButton > 0 )
      {
         if ( iOldButton == 0 )
            pP->iActionQuickButton1 = quickActionVideoRecord;
         if ( iOldButton == 1 )
            pP->iActionQuickButton2 = quickActionCycleOSD;
         if ( iOldButton == 2 )
            pP->iActionQuickButton3 = quickActionTakePicture;
      }

      if ( pCS->iQAButtonRelaySwitching > 0 )
      {
         if ( pCS->iQAButtonRelaySwitching == 0 )
            pP->iActionQuickButton1 = quickActionRelaySwitch;
         if ( pCS->iQAButtonRelaySwitching == 1 )
            pP->iActionQuickButton2 = quickActionRelaySwitch;
         if ( pCS->iQAButtonRelaySwitching == 2 )
            pP->iActionQuickButton3 = quickActionRelaySwitch;
      }
      
      save_ControllerSettings();
      save_Preferences();
      return;
   }

   if ( m_IndexItemType == m_SelectedIndex )
   {
       if ( m_pItemsSelect[2]->getSelectedIndex() != 1 )
          addMessage("Only video can be relayed");
       m_pItemsSelect[2]->setSelectedIndex(1);
       valuesToUI();
       return;
   }
}
