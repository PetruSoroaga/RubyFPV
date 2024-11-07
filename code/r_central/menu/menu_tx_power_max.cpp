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
#include "menu_tx_power_max.h"
#include "menu_objects.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"

MenuTXPowerMax::MenuTXPowerMax()
:Menu(MENU_ID_TX_POWER_MAX, "Set Radio Power Limits", NULL)
{
   m_Width = 0.42;
   m_xPos = menu_get_XStartPos(m_Width)-0.03; m_yPos = 0.2;

   m_bShowController = false;
   m_bShowVehicle = false;

  
   m_IndexPowerMaxControllerRTL8812AU = -1;
   m_IndexPowerMaxControllerRTL8812EU = -1;
   m_IndexPowerMaxControllerAtheros = -1;

   m_IndexPowerMaxVehicleRTL8812AU = -1;
   m_IndexPowerMaxVehicleRTL8812EU = -1;
   m_IndexPowerMaxVehicleAtheros = -1;

   addTopLine("You can limit the max output power level from your radio cards to avoid for example buring the input of a Tx booster if you are using one.");
}

MenuTXPowerMax::~MenuTXPowerMax()
{
}

void MenuTXPowerMax::onShow()
{
   float fSliderWidth = 0.18;
   removeAllItems();

   int iSliderIndex = 0;
   if ( m_bShowVehicle )
   {
      if ( g_pCurrentModel->hasRadioCardsRTL8812AU() )
      {
         m_pItemsSlider[iSliderIndex] = new MenuItemSlider("Vehicle Max Tx Power (RTL8812AU)", "Sets the maximum allowed TX power on current vehicle for RTL8812AU cards.", 1,MAX_TX_POWER,40, fSliderWidth);
         m_IndexPowerMaxVehicleRTL8812AU = addMenuItem(m_pItemsSlider[iSliderIndex]);
         iSliderIndex++;
      }
      if ( g_pCurrentModel->hasRadioCardsRTL8812EU() )
      {
         m_pItemsSlider[iSliderIndex] = new MenuItemSlider("Vehicle Max Tx Power (RTL8812EU)", "Sets the maximum allowed TX power on current vehicle for RTL8812EU cards.", 1,MAX_TX_POWER,40, fSliderWidth);
         m_IndexPowerMaxVehicleRTL8812EU = addMenuItem(m_pItemsSlider[iSliderIndex]);
         iSliderIndex++;
      }
      if ( g_pCurrentModel->hasRadioCardsAtheros() )
      {
         m_pItemsSlider[iSliderIndex] = new MenuItemSlider("Vehicle Max Tx Power (Atheros)", "Sets the maximum allowed TX power on current vehicle for Atheros cards.", 1,MAX_TX_POWER,40, fSliderWidth);
         m_IndexPowerMaxVehicleAtheros = addMenuItem(m_pItemsSlider[iSliderIndex]);
         iSliderIndex++;
      }
   }
   
   if ( m_bShowController )
   {
      if ( hardware_radio_has_rtl8812au_cards() )
      {
         m_pItemsSlider[iSliderIndex] = new MenuItemSlider("Controller Max Tx Power (RTL8812AU)", "Sets the maximum allowed TX power on controller for RTL8812AU cards.", 1,MAX_TX_POWER,40, fSliderWidth);
         m_IndexPowerMaxControllerRTL8812AU = addMenuItem(m_pItemsSlider[iSliderIndex]);
         iSliderIndex++;
      }
      if ( hardware_radio_has_rtl8812eu_cards() )
      {
         m_pItemsSlider[iSliderIndex] = new MenuItemSlider("Controller Max Tx Power (RTL8812EU)", "Sets the maximum allowed TX power on controller for RTL8812EU cards.", 1,MAX_TX_POWER,40, fSliderWidth);
         m_IndexPowerMaxControllerRTL8812EU = addMenuItem(m_pItemsSlider[iSliderIndex]);
         iSliderIndex++;
      }
      if ( hardware_radio_has_atheros_cards() )
      {
         m_pItemsSlider[iSliderIndex] = new MenuItemSlider("Controller Max Tx Power (Atheros)", "Sets the maximum allowed TX power on controller for Atheros cards.", 1,MAX_TX_POWER,40, fSliderWidth);
         m_IndexPowerMaxControllerAtheros = addMenuItem(m_pItemsSlider[iSliderIndex]);
         iSliderIndex++;
      }
   }
   Menu::onShow();
}

void MenuTXPowerMax::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();

   int iSliderIndex = 0;
   if ( m_bShowVehicle )
   {
      if ( g_pCurrentModel->hasRadioCardsRTL8812AU() )
      {
         m_pItemsSlider[iSliderIndex]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812AU);
         iSliderIndex++;
      }
      if ( g_pCurrentModel->hasRadioCardsRTL8812EU() )
      {
         m_pItemsSlider[iSliderIndex]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812EU);
         iSliderIndex++;
      }
      if ( g_pCurrentModel->hasRadioCardsAtheros() )
      {
         m_pItemsSlider[iSliderIndex]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros);
         iSliderIndex++;
      }
   }

   if ( m_bShowController )
   {
      if ( hardware_radio_has_rtl8812au_cards() )
      {
         m_pItemsSlider[iSliderIndex]->setCurrentValue(pCS->iMaxTXPowerRTL8812AU);
         iSliderIndex++;
      }
      if ( hardware_radio_has_rtl8812eu_cards() )
      {
         m_pItemsSlider[iSliderIndex]->setCurrentValue(pCS->iMaxTXPowerRTL8812EU);
         iSliderIndex++;
      }
      if ( hardware_radio_has_atheros_cards() )
      {
         m_pItemsSlider[iSliderIndex]->setCurrentValue(pCS->iMaxTXPowerAtheros);
         iSliderIndex++;
      }
   }
}

void MenuTXPowerMax::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);    
}

void MenuTXPowerMax::sendMaxPowerToVehicle(int txMaxRTL8812AU, int txMaxRTL8812EU, int txMaxAtheros)
{
   u8 buffer[10];
   memset(&(buffer[0]), 0, 10);
   buffer[0] = g_pCurrentModel->radioInterfacesParams.txPowerRTL8812AU;
   buffer[1] = g_pCurrentModel->radioInterfacesParams.txPowerRTL8812EU;
   buffer[2] = g_pCurrentModel->radioInterfacesParams.txPowerAtheros;
   buffer[3] = txMaxRTL8812AU;
   buffer[4] = txMaxRTL8812EU;
   buffer[5] = txMaxAtheros;

   if ( buffer[0] > buffer[3] )
      buffer[0] = buffer[3];
   if ( buffer[1] > buffer[4] )
      buffer[1] = buffer[4];
   if ( buffer[2] > buffer[5] )
      buffer[2] = buffer[5];
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TX_POWERS, 0, buffer, 8) )
       valuesToUI();
}

void MenuTXPowerMax::onSelectItem()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Menu::onSelectItem();
   
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexPowerMaxControllerRTL8812AU == m_SelectedIndex )
   {
      pCS->iMaxTXPowerRTL8812AU = m_pItemsSlider[m_IndexPowerMaxControllerRTL8812AU]->getCurrentValue();
      if ( pCS->iTXPowerRTL8812AU > pCS->iMaxTXPowerRTL8812AU )
      {
         pCS->iTXPowerRTL8812AU = pCS->iMaxTXPowerRTL8812AU;
         hardware_radio_set_txpower_rtl8812au(pCS->iTXPowerRTL8812AU);
      }
      save_ControllerSettings();
      return;
   }

   if ( m_IndexPowerMaxControllerRTL8812EU == m_SelectedIndex )
   {
      pCS->iMaxTXPowerRTL8812EU = m_pItemsSlider[m_IndexPowerMaxControllerRTL8812EU]->getCurrentValue();
      if ( pCS->iTXPowerRTL8812EU > pCS->iMaxTXPowerRTL8812EU )
      {
         pCS->iTXPowerRTL8812EU = pCS->iMaxTXPowerRTL8812EU;
         hardware_radio_set_txpower_rtl8812eu(pCS->iTXPowerRTL8812EU);
      }
      save_ControllerSettings();
      return;
   }

   if ( m_IndexPowerMaxControllerAtheros == m_SelectedIndex )
   {
      pCS->iMaxTXPowerAtheros = m_pItemsSlider[m_IndexPowerMaxControllerAtheros]->getCurrentValue();
      if ( pCS->iTXPowerAtheros > pCS->iMaxTXPowerAtheros )
      {
         pCS->iTXPowerAtheros = pCS->iMaxTXPowerAtheros;
         hardware_radio_set_txpower_atheros(pCS->iTXPowerAtheros);
      }
      save_ControllerSettings();
      return;
   }

   if ( m_IndexPowerMaxVehicleRTL8812AU == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int val = m_pItemsSlider[m_IndexPowerMaxVehicleRTL8812AU]->getCurrentValue();
      sendMaxPowerToVehicle(val, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812EU, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros);
      return;
   }

   if ( m_IndexPowerMaxVehicleRTL8812EU == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int val = m_pItemsSlider[m_IndexPowerMaxVehicleRTL8812EU]->getCurrentValue();
      sendMaxPowerToVehicle(g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812AU, val, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros);
      return;
   }

   if ( m_IndexPowerMaxVehicleAtheros == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int val = m_pItemsSlider[m_IndexPowerMaxVehicleAtheros]->getCurrentValue();
      sendMaxPowerToVehicle(g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812AU, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812EU, val);
      return;
   }
}