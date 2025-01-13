/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
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
#include "menu_tx_raw_power.h"
#include "menu_objects.h"
#include "menu_item_text.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"
#include "../../utils/utils_controller.h"
#include "../../base/tx_powers.h"

MenuTXRawPower::MenuTXRawPower()
:Menu(MENU_ID_TX_RAW_POWER, "Custom Radio Power Levels", NULL)
{
   m_Width = 0.42;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.18;
   m_bShowVehicleSide = true;
}

MenuTXRawPower::~MenuTXRawPower()
{
}

void MenuTXRawPower::onShow()
{
   addItems();
   Menu::onShow();
} 
      
void MenuTXRawPower::valuesToUI()
{
   addItems();
}


void MenuTXRawPower::addItems()
{
   int iTmp = getSelectedMenuItemIndex();
   ControllerSettings* pCS = get_ControllerSettings();

   removeAllItems();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_iIndexVehicleCards[i] = -1;
      m_iIndexControllerCards[i] = -1;
      m_pItemSelectVehicleCards[i] = NULL;
      m_pItemSelectControllerCards[i] = NULL;
   }

   if ( m_bShowVehicleSide )
   {
      addMenuItem(new MenuItemSection("Vehicle's Radio Interfaces Tx Powers"));

      if ( NULL == g_pCurrentModel )
      {
         addMenuItem(new MenuItemText("Not connected to any vehicle", true));
      }
      else
      {
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            int iDriver = (g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] >> 8) & 0xFF;
            int iCardModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[i];
            int iPowerRaw = g_pCurrentModel->radioInterfacesParams.interface_raw_power[i];
            
            m_pItemSelectVehicleCards[i] = createItemCard(true, i, iCardModel, iDriver, iPowerRaw);
            if ( NULL != m_pItemSelectVehicleCards[i] )
               m_iIndexVehicleCards[i] = addMenuItem(m_pItemSelectVehicleCards[i]);
         }
      }
   }

   addMenuItem(new MenuItemSection("Controller's Radio Interfaces Tx Powers"));

   m_iIndexAutoControllerPower = -1;
   if ( m_bShowVehicleSide && pCS->iFixedTxPower )
   {
      addMenuItem(new MenuItemText("Controller is set globally to fixed Tx power. Set it to Auto in the Controller menu if you want to be able to customize it per vehicle."));
   }
   else
   {
      if ( m_bShowVehicleSide && (NULL != g_pCurrentModel) )
      {
         m_pItemsSelect[0] = new MenuItemSelect("Auto-adjust Controller Tx Power", "Controller will auto-adjusts radio tx power to match the current vehicle.");
         m_pItemsSelect[0]->addSelection("No");
         m_pItemsSelect[0]->addSelection("Yes");
         m_pItemsSelect[0]->setIsEditable();
         m_iIndexAutoControllerPower = addMenuItem(m_pItemsSelect[0]);
         if ( g_pCurrentModel->radioInterfacesParams.iAutoControllerTxPower )
            m_pItemsSelect[0]->setSelectedIndex(1);
         else
            m_pItemsSelect[0]->setSelectedIndex(0);
      }
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

         m_pItemSelectControllerCards[i] = createItemCard(false, i, pCRII->cardModel, pRadioHWInfo->iRadioDriver, pCRII->iRawPowerLevel);
         if ( NULL != m_pItemSelectControllerCards[i] )
         {
            m_iIndexControllerCards[i] = addMenuItem(m_pItemSelectControllerCards[i]);
            if ( m_bShowVehicleSide )
            if ( g_pCurrentModel->radioInterfacesParams.iAutoControllerTxPower )
               m_pItemSelectControllerCards[i]->setEnabled(false);
         }
      }
   }
   m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}


MenuItemSelect* MenuTXRawPower::createItemCard(bool bVehicle, int iCardIndex, int iCardModel, int iDriver, int iRawPower)
{
   char szText[128];
   char szTooltip[256];
   sprintf(szText, "Interface %d (%s)", iCardIndex+1, str_get_radio_card_model_string(iCardModel));
   sprintf(szTooltip, "Adjust the output tx power of %s's radio interface %d", bVehicle?"vehicle":"controller", iCardIndex+1);
   MenuItemSelect* pItemSelect = new MenuItemSelect(szText, szTooltip);
   
   int iMaxRawPowerForCard = tx_powers_get_max_usable_power_raw_for_card(iDriver, iCardModel);
   int iIndexSelected = -1;
   int iMinRawDiff = 10000;

   int iRawValuesCount = 0;
   const int* piRawValues = tx_powers_get_raw_radio_power_values(&iRawValuesCount);

   for( int i=0; i<iRawValuesCount; i++ )
   {
      if ( piRawValues[i] > iMaxRawPowerForCard )
         sprintf(szText, "%d -> !!!", piRawValues[i]);
      else
      {
         int iMwValue = tx_powers_convert_raw_to_mw(iDriver, iCardModel, piRawValues[i]);
         sprintf(szText, "%d -> %d mW", piRawValues[i], iMwValue);
      }
      pItemSelect->addSelection(szText);
      int iDiff = iRawPower - piRawValues[i];
      if ( iDiff < 0 )
         iDiff = - iDiff;

      if ( iDiff < iMinRawDiff )
      {
         iMinRawDiff = iDiff;
         iIndexSelected = pItemSelect->getSelectionsCount()-1;
      }
   }

   pItemSelect->setIsEditable();
   if ( -1 != iIndexSelected )
      pItemSelect->setSelectedIndex(iIndexSelected);
   return pItemSelect;
}

void MenuTXRawPower::computeSendPowerToVehicle(int iCardIndex)
{
   if ( (iCardIndex < 0) || (iCardIndex >= g_pCurrentModel->radioInterfacesParams.interfaces_count) )
   {
      addMessage("Invalid card.");
      return;
   }
   int iRawValuesCount = 0;
   const int* piRawValues = tx_powers_get_raw_radio_power_values(&iRawValuesCount);

   int iPowerIndex = m_pItemSelectVehicleCards[iCardIndex]->getSelectedIndex();
   if ( (iPowerIndex < 0) || (iPowerIndex >= iRawValuesCount) )
   {
      addMessage("Invalid value");
      return;
   }
   int iRawPowerToSet = piRawValues[iPowerIndex];
   log_line("MenuTXRawPower: Setting raw tx power for card %d to: %d", iCardIndex+1, iRawPowerToSet);

   u8 uCommandParams[MAX_RADIO_INTERFACES+1];
   uCommandParams[0] = g_pCurrentModel->radioInterfacesParams.interfaces_count;

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      int iDriver = (g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] >> 8) & 0xFF;
      int iCardModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[i];
      uCommandParams[i+1] = g_pCurrentModel->radioInterfacesParams.interface_raw_power[i];
      if ( i == iCardIndex )
         uCommandParams[i+1] = iRawPowerToSet;

      int iCardMaxPowerMw = tx_powers_get_max_usable_power_mw_for_card(iDriver, iCardModel);
      int iCardPowerMw = tx_powers_convert_raw_to_mw(iDriver, iCardModel, uCommandParams[i+1]);

      log_line("MenuTXRawPower: Setting tx raw power for card %d from %d to: %d (%d mw, max is %d mw)",
         i+1, g_pCurrentModel->radioInterfacesParams.interface_raw_power[i], uCommandParams[i+1], iCardPowerMw, iCardMaxPowerMw);
   }
   
   save_ControllerInterfacesSettings();
   apply_controller_radio_tx_powers(g_pCurrentModel, get_ControllerSettings()->iFixedTxPower, false);
   save_ControllerInterfacesSettings();

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TX_POWERS, 0, uCommandParams, uCommandParams[0]+1) )
       valuesToUI();
}

void MenuTXRawPower::computeApplyControllerPower(int iCardIndex)
{
   if ( (iCardIndex < 0) || (iCardIndex >= hardware_get_radio_interfaces_count()) )
   {
      addMessage("Invalid card.");
      return;
   }

   int iRawValuesCount = 0;
   const int* piRawValues = tx_powers_get_raw_radio_power_values(&iRawValuesCount);

   int iPowerIndex = m_pItemSelectControllerCards[iCardIndex]->getSelectedIndex();
   if ( (iPowerIndex < 0) || (iPowerIndex >= iRawValuesCount) )
   {
      addMessage("Invalid value");
      return;
   }
   int iRawPowerToSet = piRawValues[iPowerIndex];
   
   if ( ! hardware_radio_index_is_wifi_radio(iCardIndex) )
      return;

   if ( hardware_radio_index_is_sik_radio(iCardIndex) )
      return;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iCardIndex);
   if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
      return;

   t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
   if ( NULL == pCRII )
      return;

   pCRII->iRawPowerLevel = iRawPowerToSet;

   save_ControllerInterfacesSettings();
   valuesToUI();
   apply_controller_radio_tx_powers(g_pCurrentModel, get_ControllerSettings()->iFixedTxPower, false);
   save_ControllerInterfacesSettings();
   send_model_changed_message_to_router(MODEL_CHANGED_RADIO_POWERS, 0);
}

void MenuTXRawPower::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}

int MenuTXRawPower::onBack()
{
   return Menu::onBack();
}

void MenuTXRawPower::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
   valuesToUI();
}

void MenuTXRawPower::onSelectItem()
{
   Menu::onSelectItem();
   
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( (-1 != m_iIndexAutoControllerPower) && (m_iIndexAutoControllerPower == m_SelectedIndex) )
   if ( NULL != g_pCurrentModel )
   {
      u32 uIndex = m_pItemsSelect[0]->getSelectedIndex();
      save_ControllerSettings();
      u32 uParam = 0;
      uParam |= uIndex << 8;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_AUTO_TX_POWERS, uParam, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_bShowVehicleSide )
   {
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( (m_iIndexVehicleCards[i] != -1) && (m_iIndexVehicleCards[i] == m_SelectedIndex) )
         if ( NULL != m_pItemSelectVehicleCards[i] )
         {
            computeSendPowerToVehicle(i);
            return;
         }
      }
   }
   
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( (m_iIndexControllerCards[i] != -1) && (m_iIndexControllerCards[i] == m_SelectedIndex) )
      if ( NULL != m_pItemSelectControllerCards[i] )
      {
         computeApplyControllerPower(i);
         return;
      }
   }
}
