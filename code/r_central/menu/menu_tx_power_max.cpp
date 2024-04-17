/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
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
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
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
   m_Width = 0.35;
   m_xPos = menu_get_XStartPos(m_Width)-0.03; m_yPos = 0.2;
   float fSliderWidth = 0.18;

   m_bShowBothOnController = false;
   m_bShowBothOnVehicle = false;

   m_bControllerHas24OnlyCards = false;
   m_bControllerHas58Cards = false;
   m_bVehicleHas24OnlyCards = false;
   m_bVehicleHas58Cards = false;

   for( int n=0; n<hardware_get_radio_interfaces_count(); n++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(n);
      if ( NULL == pNIC )
         continue;
      if ( (pNIC->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS )
         m_bControllerHas24OnlyCards = true;
      else if ( (pNIC->typeAndDriver & 0xFF) == RADIO_TYPE_RALINK )
         m_bControllerHas24OnlyCards = true;
      else
         m_bControllerHas58Cards = true;
   }

   if ( m_bControllerHas24OnlyCards && m_bControllerHas58Cards )
      m_bShowBothOnController = true;

   if ( NULL != g_pCurrentModel )
   {
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( ((g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF00) >> 8) == RADIO_HW_DRIVER_ATHEROS )
            m_bVehicleHas24OnlyCards = true;
         else if ( ((g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF00) >> 8) == RADIO_HW_DRIVER_RALINK )
            m_bVehicleHas24OnlyCards = true;
         else
            m_bVehicleHas58Cards = true;
      }
   }

   if ( m_bVehicleHas24OnlyCards && m_bVehicleHas58Cards )
      m_bShowBothOnVehicle = true;

   m_IndexPowerMaxController = -1;
   m_IndexPowerMaxControllerAtheros = -1;
   m_IndexPowerMaxControllerRTL = -1;

   m_IndexPowerMaxVehicle = -1;
   m_IndexPowerMaxVehicleAtheros = -1;
   m_IndexPowerMaxVehicleRTL = -1;

   if ( ! m_bShowBothOnVehicle )
   {
      m_pItemsSlider[0] = new MenuItemSlider("Vehicle Max Tx Power", "Sets the maximum allowed TX power on current vehicle.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxVehicle = addMenuItem(m_pItemsSlider[0]);
   }
   else
   {
       char szBuff[256];
       char szCards[256];

       szCards[0] = 0;
       for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
       {
          if ( ((g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF00) >> 8) == RADIO_HW_DRIVER_ATHEROS )
             strcat(szCards, " Atheros");
          if ( ((g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF00) >> 8) == RADIO_HW_DRIVER_RALINK )
             strcat(szCards, " RaLink");
       }

      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Vehicle Max Tx Power (2.4Ghz%s)", szCards);
       
      m_pItemsSlider[1] = new MenuItemSlider(szBuff, "Sets the maximum allowed TX power on current vehicle.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxVehicleAtheros = addMenuItem(m_pItemsSlider[1]);
      m_pItemsSlider[2] = new MenuItemSlider("Vehicle Max Tx Power (5.8Ghz)", "Sets the maximum allowed TX power on current vehicle.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxVehicleRTL = addMenuItem(m_pItemsSlider[2]);
   }

   if ( ! m_bShowBothOnController )
   {
      m_pItemsSlider[5] = new MenuItemSlider("Controller Max Tx Power", "Sets the maximum allowed TX power on the controller.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxController = addMenuItem(m_pItemsSlider[5]);
   }
   else
   {
      char szBuff[256];
      char szCards[256];

      szCards[0] = 0;
      for( int n=0; n<hardware_get_radio_interfaces_count(); n++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(n);
         if ( NULL == pRadioHWInfo )
            continue;
         if ( (pRadioHWInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS )
            strcat(szCards, " Atheros");
         if ( (pRadioHWInfo->typeAndDriver & 0xFF) == RADIO_TYPE_RALINK )
            strcat(szCards, " RaLink");
      }

      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Controller Max Tx Power (2.4Ghz%s)", szCards);
         
      m_pItemsSlider[6] = new MenuItemSlider(szBuff, "Sets the maximum allowed TX power on the controller.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxControllerAtheros = addMenuItem(m_pItemsSlider[6]);
      m_pItemsSlider[7] = new MenuItemSlider("Controller Max Tx Power (5.8Ghz)", "Sets the maximum allowed TX power on the controller.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxControllerRTL = addMenuItem(m_pItemsSlider[7]);
   }
}

MenuTXPowerMax::~MenuTXPowerMax()
{
}

void MenuTXPowerMax::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();

   if ( -1 != m_IndexPowerMaxController )
   {
      if ( m_bControllerHas24OnlyCards )
         m_pItemsSlider[5]->setCurrentValue( pCS->iMaxTXPowerAtheros);
      if ( m_bControllerHas58Cards )
         m_pItemsSlider[5]->setCurrentValue( pCS->iMaxTXPowerRTL);
   }
   if ( -1 != m_IndexPowerMaxControllerAtheros && m_bControllerHas24OnlyCards )
      m_pItemsSlider[6]->setCurrentValue( pCS->iMaxTXPowerAtheros);

   if ( -1 != m_IndexPowerMaxControllerRTL && m_bControllerHas58Cards )
      m_pItemsSlider[7]->setCurrentValue( pCS->iMaxTXPowerRTL);

   if ( NULL == g_pCurrentModel )
   {
      m_pItemsSlider[0]->setEnabled(false);
      return;
   }

   if ( -1 != m_IndexPowerMaxVehicle )
   {
      if ( m_bVehicleHas24OnlyCards )
         m_pItemsSlider[0]->setCurrentValue( g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros);
      if ( m_bVehicleHas58Cards )
         m_pItemsSlider[0]->setCurrentValue( g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL );
   }
   if ( -1 != m_IndexPowerMaxVehicleAtheros && m_bVehicleHas24OnlyCards )
      m_pItemsSlider[1]->setCurrentValue( g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros);

   if ( -1 != m_IndexPowerMaxVehicleRTL && m_bVehicleHas58Cards )
      m_pItemsSlider[2]->setCurrentValue( g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL);
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

void MenuTXPowerMax::sendMaxPowerToVehicle(int txMax, int txMaxAtheros, int txMaxRTL)
{
   u8 buffer[10];
   memset(&(buffer[0]), 0, 10);
   buffer[0] = g_pCurrentModel->radioInterfacesParams.txPower;
   buffer[1] = g_pCurrentModel->radioInterfacesParams.txPowerAtheros;
   buffer[2] = g_pCurrentModel->radioInterfacesParams.txPowerRTL;
   buffer[3] = txMax;
   buffer[4] = txMaxAtheros;
   buffer[5] = txMaxRTL;

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

   if ( m_IndexPowerMaxController == m_SelectedIndex )
   {
      if ( m_bControllerHas24OnlyCards )
         pCS->iMaxTXPowerAtheros = m_pItemsSlider[5]->getCurrentValue();
      if ( m_bControllerHas58Cards )
         pCS->iMaxTXPowerRTL = m_pItemsSlider[5]->getCurrentValue();
      if ( pCS->iTXPowerAtheros > pCS->iMaxTXPowerAtheros )
      {
         pCS->iTXPowerAtheros = pCS->iMaxTXPowerAtheros;
         hardware_set_radio_tx_power_atheros(pCS->iTXPowerAtheros);
      }
      if ( pCS->iTXPowerRTL > pCS->iMaxTXPowerRTL )
      {
         pCS->iTXPowerRTL = pCS->iMaxTXPowerRTL;
         hardware_set_radio_tx_power_rtl(pCS->iTXPowerRTL);
      }
      save_ControllerSettings();
      return;
   }

   if ( m_IndexPowerMaxControllerAtheros == m_SelectedIndex )
   {
      pCS->iMaxTXPowerAtheros = m_pItemsSlider[6]->getCurrentValue();
      if ( pCS->iTXPowerAtheros > pCS->iMaxTXPowerAtheros )
      {
         pCS->iTXPowerAtheros = pCS->iMaxTXPowerAtheros;
         hardware_set_radio_tx_power_atheros(pCS->iTXPowerAtheros);
      }
      save_ControllerSettings();
      return;
   }

   if ( m_IndexPowerMaxControllerRTL == m_SelectedIndex )
   {
      pCS->iMaxTXPowerRTL = m_pItemsSlider[7]->getCurrentValue();
      if ( pCS->iTXPowerRTL > pCS->iMaxTXPowerRTL )
      {
         pCS->iTXPowerRTL = pCS->iMaxTXPowerRTL;
         hardware_set_radio_tx_power_rtl(pCS->iTXPowerRTL);
      }
      save_ControllerSettings();
      return;
   }

   if ( m_IndexPowerMaxVehicle == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int val = m_pItemsSlider[0]->getCurrentValue();
      int txMax = g_pCurrentModel->radioInterfacesParams.txMaxPower;
      int txMaxAtheros = g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros;
      int txMaxRTL = g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL;

      if ( m_bVehicleHas24OnlyCards )
         txMaxAtheros = val;
      if ( m_bVehicleHas58Cards )
         txMaxRTL = val;

      sendMaxPowerToVehicle(txMax, txMaxAtheros, txMaxRTL);
      return;
   }
   if ( m_IndexPowerMaxVehicleAtheros == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int txMax = g_pCurrentModel->radioInterfacesParams.txMaxPower;
      int txMaxAtheros = m_pItemsSlider[1]->getCurrentValue();
      int txMaxRTL = g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL;
      sendMaxPowerToVehicle(txMax, txMaxAtheros, txMaxRTL);
      return;
   }

   if ( m_IndexPowerMaxVehicleRTL == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int txMax = g_pCurrentModel->radioInterfacesParams.txMaxPower;
      int txMaxAtheros = g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros;
      int txMaxRTL = m_pItemsSlider[2]->getCurrentValue();
      sendMaxPowerToVehicle(txMax, txMaxAtheros, txMaxRTL);
      return;
   }
}