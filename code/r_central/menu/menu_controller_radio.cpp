/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "menu.h"
#include "menu_objects.h"
#include "menu_controller_radio.h"
#include "menu_text.h"
#include "menu_item_section.h"
#include "menu_item_legend.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"
#include "menu_radio_config.h"
#include "menu_tx_raw_power.h"
#include "../../base/ctrl_settings.h"
#include "../../base/tx_powers.h"
#include "../../utils/utils_controller.h"

#include <time.h>
#include <sys/resource.h>

MenuControllerRadio::MenuControllerRadio(void)
:Menu(MENU_ID_CONTROLLER_RADIO, "Controller Radio Settings", NULL)
{
   m_Width = 0.38;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.18;
   m_iMaxPowerMw = 0;
}

void MenuControllerRadio::onShow()
{
   int iTmp = getSelectedMenuItemIndex();

   valuesToUI();
   Menu::onShow();
   invalidate();

   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuControllerRadio::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();

   addItems();

   int iMwPowers[MAX_RADIO_INTERFACES];
   int iCountPowers = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( ! hardware_radio_is_index_wifi_radio(i) )
         continue;
      if ( hardware_radio_index_is_sik_radio(i) )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
         continue;

      t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCRII )
         continue;
      int iDriver = pRadioHWInfo->iRadioDriver;
      int iCardModel = pCRII->cardModel;
      iMwPowers[iCountPowers] = tx_powers_convert_raw_to_mw(iDriver, iCardModel, pCRII->iRawPowerLevel);
      log_line("MenuControllerRadio: Current radio interface %d tx raw power: %d (%d mW, index %d)", i+1, pCRII->iRawPowerLevel, iMwPowers[iCountPowers], iCountPowers);
      
      iCountPowers++;
   }

   m_pItemsSelect[0]->setSelectedIndex(1-pCS->iFixedTxPower);
   
   if ( pCS->iFixedTxPower )
   {
      m_iMaxPowerMw = tx_powers_get_max_usable_power_mw_for_controller();
      selectMenuItemTxPowersValue(m_pItemsSelect[1], false, &(iMwPowers[0]), iCountPowers, m_iMaxPowerMw);
   }
   else
   {
      m_pItemsSelect[1]->setSelectedIndex(0);
      m_pItemsSelect[1]->setEnabled(false);
   }
}

void MenuControllerRadio::addItems()
{
   int iTmp = getSelectedMenuItemIndex();
   removeAllItems();

   ControllerSettings* pCS = get_ControllerSettings();

   int iMwPowers[MAX_RADIO_INTERFACES];
   int iCountPowers = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( ! hardware_radio_is_index_wifi_radio(i) )
         continue;
      if ( hardware_radio_index_is_sik_radio(i) )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
         continue;

      t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCRII )
         continue;

      iMwPowers[iCountPowers] = pCRII->iRawPowerLevel;
      iCountPowers++;
   }

   m_pItemsSelect[0] = new MenuItemSelect("Controller Power Model", "Sets the transmission power mode for the controller side.");
   m_pItemsSelect[0]->addSelection("Always use fixed power");
   m_pItemsSelect[0]->addSelection("Match current vehicle");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexTxPowerMode = addMenuItem(m_pItemsSelect[0]);

   m_iMaxPowerMw = tx_powers_get_max_usable_power_mw_for_controller();
   m_pItemsSelect[1] = createMenuItemTxPowers("Radio Tx Power (mW)", pCS->iFixedTxPower?false:true, &(iMwPowers[0]), iCountPowers, m_iMaxPowerMw);
   m_IndexTxPower = addMenuItem(m_pItemsSelect[1]);

   if ( (! pCS->iFixedTxPower) && (NULL != g_pCurrentModel) )
   {
      int iCurrentMaxSetPowerMw = apply_controller_radio_tx_powers(g_pCurrentModel, pCS->iFixedTxPower, true);
      char szText[128];
      sprintf(szText, "Current Tx Power: %d mW (%s)", iCurrentMaxSetPowerMw, getString(4));
      addMenuItem(new MenuItemText(szText, true));
   }

   MenuItemLegend* pLegend = NULL;
   if ( pCS->iFixedTxPower )
      pLegend = new MenuItemLegend("Note", "Maximum selectable Tx power is computed based on detected radio interfaces on the controller.", 0, true);
   else
      pLegend = new MenuItemLegend("Note", "Maximum selectable Tx power is computed based on detected radio interfaces on the vehicle and controller.", 0, true);
   
   pLegend->setExtraHeight(0.4*g_pRenderEngine->textHeight(g_idFontMenu));
   addMenuItem(pLegend);

   m_IndexRadioConfig = addMenuItem(new MenuItem("Radio Links Config", "Full radio configuration"));
   m_pMenuItems[m_IndexRadioConfig]->showArrow();

   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuControllerRadio::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

bool MenuControllerRadio::periodicLoop()
{
   return false;
}

void MenuControllerRadio::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
   valuesToUI();
}

void MenuControllerRadio::onSelectItem()
{
   Menu::onSelectItem();
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;


   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   if ( m_IndexRadioConfig == m_SelectedIndex )
   {
      MenuRadioConfig* pM = new MenuRadioConfig();
      add_menu_to_stack(pM);
      return;
   }

   if ( m_IndexTxPowerMode == m_SelectedIndex )
   {
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iFixedTxPower = 1 - m_pItemsSelect[0]->getSelectedIndex();
      save_ControllerSettings();
      save_ControllerInterfacesSettings();
      apply_controller_radio_tx_powers(g_pCurrentModel, pCS->iFixedTxPower, false);
      save_ControllerInterfacesSettings();
      valuesToUI();
      send_model_changed_message_to_router(MODEL_CHANGED_RADIO_POWERS, 0);
      return;
   }

   if ( m_IndexTxPower == m_SelectedIndex )
   {
      ControllerSettings* pCS = get_ControllerSettings();
      int iIndex = m_pItemsSelect[1]->getSelectedIndex();
      if ( iIndex == m_pItemsSelect[1]->getSelectionsCount() -1 )
      {
         MenuTXRawPower* pMenu = new MenuTXRawPower();
         pMenu->m_bShowVehicleSide = false;
         add_menu_to_stack(pMenu);
         return;
      }
      else
      {
         pCS->iFixedTxPower = 1;

         int iPowerLevelsCount = 0;
         const int* piPowerLevelsMw = tx_powers_get_ui_levels_mw(&iPowerLevelsCount);
         int iPowerMwToSet = piPowerLevelsMw[iIndex];
         log_line("MenuControllerRadio: Setting all cards mw tx power to: %d mw", iPowerMwToSet);
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( ! hardware_radio_is_index_wifi_radio(i) )
               continue;
            if ( hardware_radio_index_is_sik_radio(i) )
               continue;
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
            if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
               continue;

            t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
            if ( NULL == pCRII )
               continue;

            int iDriver = pRadioHWInfo->iRadioDriver;
            int iCardModel = pCRII->cardModel;

            int iCardMaxPowerMw = tx_powers_get_max_usable_power_mw_for_card(iDriver, iCardModel);
            int iCardNewPowerMw = iPowerMwToSet;
            if ( iCardNewPowerMw > iCardMaxPowerMw )
               iCardNewPowerMw = iCardMaxPowerMw;
            int iTxPowerRawToSet = tx_powers_convert_mw_to_raw(iDriver, iCardModel, iCardNewPowerMw);
            log_line("MenuControllerRadio: Setting tx raw power for card %d from %d to: %d (%d mw out of max %d mw for this card)",
               i+1, pCRII->iRawPowerLevel, iTxPowerRawToSet, iCardNewPowerMw, iCardMaxPowerMw);
            pCRII->iRawPowerLevel = iTxPowerRawToSet;
         }
      }
      save_ControllerSettings();
      save_ControllerInterfacesSettings();
      apply_controller_radio_tx_powers(g_pCurrentModel, pCS->iFixedTxPower, false);
      save_ControllerInterfacesSettings();
      valuesToUI();
      send_model_changed_message_to_router(MODEL_CHANGED_RADIO_POWERS, 0);
      return;
   }
}

