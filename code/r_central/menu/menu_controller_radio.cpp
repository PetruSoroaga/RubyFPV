/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

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
}

void MenuControllerRadio::onShow()
{
   int iTmp = getSelectedMenuItemIndex();
   Menu::onShow();
   addItems();
   invalidate();

   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex < 0 )
      m_SelectedIndex = 0;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}


void MenuControllerRadio::addItems()
{
   int iTmp = getSelectedMenuItemIndex();
   removeAllItems();

   ControllerSettings* pCS = get_ControllerSettings();

   m_IndexTxPowerSingle = -1;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      m_IndexTxPowerRadioLinks[i] = -1;

   m_pItemsSelect[0] = new MenuItemSelect(L("Controller Tx Power Mode"), L("Sets the radio transmission power mode selector for the controller side: custom fixed power or auto computed for each radio link based on vechile radio links tx powers."));
   m_pItemsSelect[0]->addSelection(L("Fixed uplink Tx power"));
   m_pItemsSelect[0]->addSelection(L("Match current vehicle"));
   m_pItemsSelect[0]->setIsEditable();
   m_pItemsSelect[0]->setSelectedIndex(1-pCS->iFixedTxPower);
   m_pItemsSelect[0]->setExtraHeight(0.2*g_pRenderEngine->textHeight(g_idFontMenu));
   m_IndexTxPowerMode = addMenuItem(m_pItemsSelect[0]);
   
   if ( pCS->iFixedTxPower )
      addItemsFixedPower();
   else
      addItemsVehiclePower();
    /*
   int iMwPowers[MAX_RADIO_INTERFACES];
   int iCountPowers = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( ! hardware_radio_index_is_wifi_radio(i) )
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

   m_iMaxPowerMw = tx_powers_get_max_usable_power_mw_for_controller();
   m_pItemsSelect[1] = createMenuItemTxPowers("Radio Tx Power (mW)", pCS->iFixedTxPower?false:true, false, false, &(iMwPowers[0]), iCountPowers, m_iMaxPowerMw);
   m_IndexTxPower = addMenuItem(m_pItemsSelect[1]);

   if ( (! pCS->iFixedTxPower) && (NULL != g_pCurrentModel) )
   {
      int iCurrentMaxSetPowerMw = apply_controller_radio_tx_powers(g_pCurrentModel, pCS->iFixedTxPower, true);
      char szText[128];
      sprintf(szText, "Current Tx Power: %d mW (%s)", iCurrentMaxSetPowerMw, getString(4));
      addMenuItem(new MenuItemText(szText, true));
   }

   MenuItemLegend* pLegend = NULL;
   char szText[256];
   if ( pCS->iFixedTxPower )
      strcpy(szText, L("Maximum selectable controller Tx power is computed based on detected radio interfaces on the controller."));
   else
      strcpy(szText, L("Controller Tx power is computed automatically for each radio link based on vehicle's radio links Tx power and the Tx capabilities of the controller's radio interfaces."));

   char szBuff[255];
   szBuff[0] = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( ! hardware_radio_index_is_wifi_radio(i) )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( 0 != szBuff[0] )
         strcat(szBuff, ", ");
      //str_get_radio_driver_description(pRadioHWInfo->iRadioDriver);
      strcat(szBuff, str_get_radio_card_model_string_short(pRadioHWInfo->iCardModel));
   }
   strcat(szText, " (");
   strcat(szText, L("Controller"));
   strcat(szText, ": ");
   strcat(szText, szBuff);
   strcat(szText, ")");
   pLegend = new MenuItemLegend(L("Note"), szText, 0);
   
   pLegend->setExtraHeight(0.4*g_pRenderEngine->textHeight(g_idFontMenu));
   addMenuItem(pLegend);

   */

   m_pMenuItems[m_ItemsCount-1]->setExtraHeight(0.4*g_pRenderEngine->textHeight(g_idFontMenu));

   m_IndexRadioConfig = addMenuItem(new MenuItem(L("Full Radio Config"), L("Full radio configuration")));
   m_pMenuItems[m_IndexRadioConfig]->showArrow();

   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuControllerRadio::addItemsFixedPower()
{
   MenuItemLegend* pLegend = NULL;
   char szText[256];
   strcpy(szText, L("Controller Tx power is set to fixed values."));
   pLegend = new MenuItemLegend(L("Mode"), szText, 0);
   pLegend->setExtraHeight(0.2*g_pRenderEngine->textHeight(g_idFontMenu));
   addMenuItem(pLegend);


   int iMwPowers[MAX_RADIO_INTERFACES];
   int iCountPowers = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( ! hardware_radio_index_is_wifi_radio(i) )
         continue;
      if ( hardware_radio_index_is_sik_radio(i) )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
         continue;

      t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCRII )
         continue;

      iMwPowers[iCountPowers] = tx_powers_convert_raw_to_mw(0, pCRII->cardModel, pCRII->iRawPowerLevel);
      iCountPowers++;
   }

   int iMaxPower = tx_powers_get_max_usable_power_mw_for_controller();
   m_pItemsSelect[1] = createMenuItemTxPowers("Radio Tx Power (mW)", false, false, false, iMaxPower);

   selectMenuItemTxPowersValue(m_pItemsSelect[1], false, false, false, &(iMwPowers[0]), iCountPowers, iMaxPower);

   m_IndexTxPowerSingle = addMenuItem(m_pItemsSelect[1]);
}

void MenuControllerRadio::addItemsVehiclePower()
{
   MenuItemLegend* pLegend = NULL;
   char szText[256];
   strcpy(szText, L("Controller Tx power is computed automatically for each radio link based on vehicle's radio links Tx power and the Tx capabilities of the controller's radio interfaces."));
   pLegend = new MenuItemLegend(L("Mode"), szText, 0);
   pLegend->setExtraHeight(0.2*g_pRenderEngine->textHeight(g_idFontMenu));
   addMenuItem(pLegend);


   if ( (NULL == g_pCurrentModel) || (! pairing_isStarted()) )
   {
      pLegend = new MenuItemLegend(L("No vehicle"), L("You are not connected to any vehicles. The Tx power is set automatically to a low value."), 0);
      addMenuItem(pLegend);
      return;
   }

   for( int iLink=0; iLink<g_pCurrentModel->radioLinksParams.links_count; iLink++ )
   {
      char szTitle[64];
      strcpy(szTitle, L("Radio Uplink"));
      if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
         sprintf(szTitle, L("Radio Uplink %d"), iLink+1);
   
      int iCountInterfacesForLink = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iLink )
            continue;
         if ( ! hardware_radio_index_is_wifi_radio(i) )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
            continue;

         t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL == pCRII )
            continue;
         iCountInterfacesForLink++;
      }

      szText[0] = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iLink )
            continue;
         if ( ! hardware_radio_index_is_wifi_radio(i) )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
            continue;

         t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL == pCRII )
            continue;

         int iCardModel = pCRII->cardModel;
         int iCardPowerMwNow = tx_powers_convert_raw_to_mw(0, iCardModel, pCRII->iRawPowerLevel);
         char szBuff[128];
         if ( iCountInterfacesForLink < 2 )
         {
            if ( iCardPowerMwNow >= 1000 )
               sprintf(szBuff, "Radio Interface %s: %.1f W", str_get_radio_card_model_string_short(pRadioHWInfo->iCardModel), (float)iCardPowerMwNow/1000.0);
            else
               sprintf(szBuff, "Radio Interface %s: %d mW", str_get_radio_card_model_string_short(pRadioHWInfo->iCardModel), iCardPowerMwNow);
         }
         else
         {
            if ( iCardPowerMwNow >= 1000 )
               sprintf(szBuff, "Radio Interface %d (%s): %.1f W", i+1, str_get_radio_card_model_string_short(pRadioHWInfo->iCardModel), (float)iCardPowerMwNow/1000.0);
            else
               sprintf(szBuff, "Radio Interface %d (%s): %d mW", i+1, str_get_radio_card_model_string_short(pRadioHWInfo->iCardModel), iCardPowerMwNow);
         }
         if ( 0 != szText[0] )
            strcat(szText, "\n");
         strcat(szText, szBuff);
      }

      pLegend = new MenuItemLegend(szTitle, szText, 0);
      addMenuItem(pLegend);
   }
}

void MenuControllerRadio::valuesToUI()
{
   addItems();
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
      compute_controller_radio_tx_powers(g_pCurrentModel, &g_SM_RadioStats);
      send_model_changed_message_to_router(MODEL_CHANGED_RADIO_POWERS, 0);
      valuesToUI();
      return;
   }

  
   if ( (-1 != m_IndexTxPowerSingle) && (m_IndexTxPowerSingle == m_SelectedIndex) )
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
      
      pCS->iFixedTxPower = 1;

      int iPowerLevelsCount = 0;
      const int* piPowerLevelsMw = tx_powers_get_ui_levels_mw(&iPowerLevelsCount);
      int iPowerMwToSet = piPowerLevelsMw[iIndex];
      log_line("MenuControllerRadio: Setting all cards mw tx power to: %d mw", iPowerMwToSet);
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( ! hardware_radio_index_is_wifi_radio(i) )
            continue;
         if ( hardware_radio_index_is_sik_radio(i) )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
            continue;

         t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL == pCRII )
            continue;

         int iCardModel = pCRII->cardModel;

         int iCardMaxPowerMw = tx_powers_get_max_usable_power_mw_for_card(0, iCardModel);
         int iCardNewPowerMw = iPowerMwToSet;
         if ( iCardNewPowerMw > iCardMaxPowerMw )
            iCardNewPowerMw = iCardMaxPowerMw;
         int iTxPowerRawToSet = tx_powers_convert_mw_to_raw(0, iCardModel, iCardNewPowerMw);
         log_line("MenuControllerRadio: Setting tx raw power for card %d from %d to: %d (%d mw out of max %d mw for this card)",
            i+1, pCRII->iRawPowerLevel, iTxPowerRawToSet, iCardNewPowerMw, iCardMaxPowerMw);
         pCRII->iRawPowerLevel = iTxPowerRawToSet;
      }
      save_ControllerSettings();
      save_ControllerInterfacesSettings();
      compute_controller_radio_tx_powers(g_pCurrentModel, &g_SM_RadioStats);
      send_model_changed_message_to_router(MODEL_CHANGED_RADIO_POWERS, 0);
      valuesToUI();
      return;
   }
}

