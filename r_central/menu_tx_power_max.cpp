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
#include "../base/config.h"
#include "../renderer/render_engine.h"
#include "menu.h"
#include "menu_tx_power_max.h"
#include "menu_objects.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"

#include "colors.h"
#include "../base/ctrl_settings.h"
#include "../base/hw_procs.h"
#include "pairing.h"
#include "shared_vars.h"

MenuTXPowerMax::MenuTXPowerMax()
:Menu(MENU_ID_TX_POWER_MAX, "Set Radio Power Limits", NULL)
{
   m_Width = 0.35;
   m_xPos = menu_get_XStartPos(m_Width)-0.03; m_yPos = 0.2;
   float fSliderWidth = 0.18;

   m_bShowBothOnController = false;
   m_bShowBothOnVehicle = false;

   m_bControllerHasAtheros = false;
   m_bControllerHasRTL = false;
   m_bVehicleHasAtheros = false;
   m_bVehicleHasRTL = false;

   ControllerSettings* pCS = get_ControllerSettings();

   for( int n=0; n<hardware_get_radio_interfaces_count(); n++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(n);
      if ( NULL == pNIC )
         continue;
      if ( (pNIC->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS )
         m_bControllerHasAtheros = true;
      else
         m_bControllerHasRTL = true;
   }

   if ( m_bControllerHasAtheros && m_bControllerHasRTL )
      m_bShowBothOnController = true;

   if ( NULL != g_pCurrentModel )
   {
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( ((g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF00) >> 8) == RADIO_HW_DRIVER_ATHEROS )
            m_bVehicleHasAtheros = true;
         else
            m_bVehicleHasRTL = true;
      }
   }

   if ( m_bVehicleHasAtheros && m_bVehicleHasRTL )
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
      m_pItemsSlider[1] = new MenuItemSlider("Vehicle Max Tx Power (2.4Ghz Atheros)", "Sets the maximum allowed TX power on current vehicle.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxVehicleAtheros = addMenuItem(m_pItemsSlider[1]);
      m_pItemsSlider[2] = new MenuItemSlider("Vehicle Max Tx Power (5.8Ghz RTL)", "Sets the maximum allowed TX power on current vehicle.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxVehicleRTL = addMenuItem(m_pItemsSlider[2]);
   }

   if ( ! m_bShowBothOnController )
   {
      m_pItemsSlider[5] = new MenuItemSlider("Controller Max Tx Power", "Sets the maximum allowed TX power on the controller.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxController = addMenuItem(m_pItemsSlider[5]);
   }
   else
   {
      m_pItemsSlider[6] = new MenuItemSlider("Controller Max Tx Power (2.4Ghz, Atheros)", "Sets the maximum allowed TX power on the controller.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxControllerAtheros = addMenuItem(m_pItemsSlider[6]);
      m_pItemsSlider[7] = new MenuItemSlider("Controller Max Tx Power (5.8Ghz, RTL)", "Sets the maximum allowed TX power on the controller.", 1,MAX_TX_POWER,40, fSliderWidth);
      m_IndexPowerMaxControllerRTL = addMenuItem(m_pItemsSlider[7]);
   }
}

MenuTXPowerMax::~MenuTXPowerMax()
{
}

void MenuTXPowerMax::valuesToUI()
{
   m_ExtraHeight = 0;
   ControllerSettings* pCS = get_ControllerSettings();

   if ( -1 != m_IndexPowerMaxController )
   {
      if ( m_bControllerHasAtheros )
         m_pItemsSlider[5]->setCurrentValue( pCS->iMaxTXPowerAtheros);
      if ( m_bControllerHasRTL )
         m_pItemsSlider[5]->setCurrentValue( pCS->iMaxTXPowerRTL);
   }
   if ( -1 != m_IndexPowerMaxControllerAtheros && m_bControllerHasAtheros )
      m_pItemsSlider[6]->setCurrentValue( pCS->iMaxTXPowerAtheros);

   if ( -1 != m_IndexPowerMaxControllerRTL && m_bControllerHasRTL )
      m_pItemsSlider[7]->setCurrentValue( pCS->iMaxTXPowerRTL);

   if ( NULL == g_pCurrentModel )
   {
      m_pItemsSlider[0]->setEnabled(false);
      return;
   }

   if ( -1 != m_IndexPowerMaxVehicle )
   {
      if ( m_bVehicleHasAtheros )
         m_pItemsSlider[0]->setCurrentValue( g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros);
      if ( m_bVehicleHasRTL )
         m_pItemsSlider[0]->setCurrentValue( g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL );
   }
   if ( -1 != m_IndexPowerMaxVehicleAtheros && m_bVehicleHasAtheros )
      m_pItemsSlider[1]->setCurrentValue( g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros);

   if ( -1 != m_IndexPowerMaxVehicleRTL && m_bVehicleHasRTL )
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
   char szBuff[1024];

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
      if ( m_bControllerHasAtheros )
         pCS->iMaxTXPowerAtheros = m_pItemsSlider[5]->getCurrentValue();
      if ( m_bControllerHasRTL )
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

      if ( m_bVehicleHasAtheros )
         txMaxAtheros = val;
      if ( m_bVehicleHasRTL )
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