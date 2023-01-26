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
#include "menu.h"
#include "menu_vehicle_functions.h"
#include "menu_channels_select.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"

MenuVehicleFunctions::MenuVehicleFunctions(void)
:Menu(MENU_ID_VEHICLE_FUNCTIONS, "Special Functions and Triggers", NULL)
{
   m_Width = 0.31;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.20;

   m_IndexEnableFreqSwitch1 = -1;
   m_IndexFreqRCChannel1 = -1;
   m_IndexFreqRCSwitchType1 = -1;
   m_IndexFreqSelectChannels1 = -1;

   m_IndexEnableFreqSwitch2 = -1;
   m_IndexFreqRCChannel2 = -1;
   m_IndexFreqRCSwitchType2 = -1;
   m_IndexFreqSelectChannels2 = -1;

#ifdef FEATURE_ENABLE_RC_FREQ_SWITCH

   m_pItemsSelect[0] = new MenuItemSelect("Enable RC Frequency Switch", "Enables switching the main vehicle radio frequency using a RC channel switch.");
   m_pItemsSelect[0]->addSelection("No");
   m_pItemsSelect[0]->addSelection("Yes");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexEnableFreqSwitch1 = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("RC Channel Used", "Selects what RC channel to use for frequency switching.");
   m_pItemsSelect[1]->addSelection("None");
   for( int i=0; i<14; i++ )
   {
      char szBuff[64];
      sprintf(szBuff, "RC Channel %d", i+1);
      m_pItemsSelect[1]->addSelection(szBuff);
   }
   m_pItemsSelect[1]->setIsEditable();
   m_IndexFreqRCChannel1 = addMenuItem(m_pItemsSelect[1]);


   m_pItemsSelect[2] = new MenuItemSelect("RC Switch Type", "Select what type of switch is on the RC Channel. If 2 positions switch type, toggle it will cycle through the frequencies. If 3 positions switch, up and down will move through the frequencies.");
   m_pItemsSelect[2]->addSelection("2 Positions");
   m_pItemsSelect[2]->addSelection("3 Positions");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexFreqRCSwitchType1 = addMenuItem(m_pItemsSelect[2]);

   m_IndexFreqSelectChannels1 = addMenuItem(new MenuItem("Frequencies to cycle through", "Choose what frequencies to use when switching the frequency."));
   m_pMenuItems[m_IndexFreqSelectChannels1]->showArrow();

   if ( g_pCurrentModel->nic_count < 2 )
      return;


   m_pItemsSelect[3] = new MenuItemSelect("Enable RC Secondary Frequency Switch", "Enables switching the secondary vehicle radio frequency using a RC channel switch.");
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("Yes");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexEnableFreqSwitch2 = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[4] = new MenuItemSelect("RC Channel Used", "Selects what RC channel to use for secondary frequency switching.");
   m_pItemsSelect[4]->addSelection("None");
   for( int i=0; i<14; i++ )
   {
      char szBuff[64];
      sprintf(szBuff, "RC Channel %d", i+1);
      m_pItemsSelect[4]->addSelection(szBuff);
   }
   m_pItemsSelect[4]->setIsEditable();
   m_IndexFreqRCChannel2 = addMenuItem(m_pItemsSelect[4]);


   m_pItemsSelect[5] = new MenuItemSelect("RC Switch Type", "Select what type of switch is on the RC Channel. If 2 positions switch type, toggle it will cycle through the frequencies. If 3 positions switch, up and down will move through the frequencies.");
   m_pItemsSelect[5]->addSelection("2 Positions");
   m_pItemsSelect[5]->addSelection("3 Positions");
   m_pItemsSelect[5]->setIsEditable();
   m_IndexFreqRCSwitchType2 = addMenuItem(m_pItemsSelect[5]);

   m_IndexFreqSelectChannels2 = addMenuItem(new MenuItem("Frequencies to cycle through", "Choose what frequencies to use when switching the secondary frequency."));
   m_pMenuItems[m_IndexFreqSelectChannels2]->showArrow();
#endif

}

MenuVehicleFunctions::~MenuVehicleFunctions()
{
}

void MenuVehicleFunctions::valuesToUI()
{

#ifdef FEATURE_ENABLE_RC_FREQ_SWITCH

   m_pItemsSelect[0]->setSelectedIndex(g_pCurrentModel->functions_params.bEnableRCTriggerFreqSwitchLink1);

   if ( ! g_pCurrentModel->functions_params.bEnableRCTriggerFreqSwitchLink1 )
   {
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSelect[2]->setEnabled(false);
      m_pMenuItems[3]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[1]->setEnabled(true);
      m_pItemsSelect[2]->setEnabled(true);
      m_pMenuItems[3]->setEnabled(true);
      if ( g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink1 >= 0 )
         m_pItemsSelect[1]->setSelectedIndex(g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink1+1);
      else
         m_pItemsSelect[1]->setSelectedIndex(0);

      m_pItemsSelect[2]->setSelectedIndex(g_pCurrentModel->functions_params.bRCTriggerFreqSwitchLink1_is3Position? 1:0);
   }

   if ( g_pCurrentModel->nic_count < 2 )
      return;

   m_pItemsSelect[3]->setSelectedIndex(g_pCurrentModel->functions_params.bEnableRCTriggerFreqSwitchLink2);

   if ( ! g_pCurrentModel->functions_params.bEnableRCTriggerFreqSwitchLink2 )
   {
      m_pItemsSelect[4]->setEnabled(false);
      m_pItemsSelect[5]->setEnabled(false);
      m_pMenuItems[6]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[4]->setEnabled(true);
      m_pItemsSelect[5]->setEnabled(true);
      m_pMenuItems[6]->setEnabled(true);
      if ( g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink2 >= 0 )
         m_pItemsSelect[4]->setSelectedIndex(g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink2+1);
      else
         m_pItemsSelect[4]->setSelectedIndex(0);

      m_pItemsSelect[5]->setSelectedIndex(g_pCurrentModel->functions_params.bRCTriggerFreqSwitchLink2_is3Position? 1:0);
   }
#endif

}

void MenuVehicleFunctions::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleFunctions::sendParams()
{
   type_functions_parameters params;
   memcpy(&params, &(g_pCurrentModel->functions_params), sizeof(type_functions_parameters));

#ifdef FEATURE_ENABLE_RC_FREQ_SWITCH

   if ( m_pItemsSelect[0]->getSelectedIndex() == 0 )
      params.bEnableRCTriggerFreqSwitchLink1 = false;
   else
      params.bEnableRCTriggerFreqSwitchLink1 = true;

   if ( m_pItemsSelect[1]->getSelectedIndex() == 0 )
      params.iRCTriggerChannelFreqSwitchLink1 = -1;
   else
      params.iRCTriggerChannelFreqSwitchLink1 = m_pItemsSelect[1]->getSelectedIndex()-1;

   if ( m_pItemsSelect[2]->getSelectedIndex() == 0 )
      params.bRCTriggerFreqSwitchLink1_is3Position = false;
   else
      params.bRCTriggerFreqSwitchLink1_is3Position = true;

   if ( g_pCurrentModel->nic_count > 1 )
   {
   if ( m_pItemsSelect[3]->getSelectedIndex() == 0 )
      params.bEnableRCTriggerFreqSwitchLink2 = false;
   else
      params.bEnableRCTriggerFreqSwitchLink2 = true;

   if ( m_pItemsSelect[4]->getSelectedIndex() == 0 )
      params.iRCTriggerChannelFreqSwitchLink2 = -1;
   else
      params.iRCTriggerChannelFreqSwitchLink2 = m_pItemsSelect[4]->getSelectedIndex()-1;

   if ( m_pItemsSelect[5]->getSelectedIndex() == 0 )
      params.bRCTriggerFreqSwitchLink2_is3Position = false;
   else
      params.bRCTriggerFreqSwitchLink2_is3Position = true;
   }
#else
   params.bEnableRCTriggerFreqSwitchLink1 = false;
   params.bEnableRCTriggerFreqSwitchLink2 = false;
   params.bEnableRCTriggerFreqSwitchLink3 = false;
#endif

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS, 0, (u8*)&params, sizeof(type_functions_parameters)) )
      valuesToUI();      
}

void MenuVehicleFunctions::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

#ifdef FEATURE_ENABLE_RC_FREQ_SWITCH

   if ( 1 == m_iConfirmationId && 1 == returnValue )
   {
      type_functions_parameters params;
      memcpy(&params, &(g_pCurrentModel->functions_params), sizeof(type_functions_parameters));
   
      params.uChannels433FreqSwitch[0] = s_uChannelsSelect433Band;
      params.uChannels868FreqSwitch[0] = s_uChannelsSelect868Band;
      params.uChannels23FreqSwitch[0] = s_uChannelsSelect23Band;
      params.uChannels24FreqSwitch[0] = s_uChannelsSelect24Band;
      params.uChannels25FreqSwitch[0] = s_uChannelsSelect25Band;
      params.uChannels58FreqSwitch[0] = s_uChannelsSelect58Band;
      log_line("New channels set for main frequency: 433: %u, 868: %u, 23: %u, 24: %u, 25: %u, 58: %u", s_uChannelsSelect433Band, s_uChannelsSelect868Band, s_uChannelsSelect23Band, s_uChannelsSelect24Band, s_uChannelsSelect25Band, s_uChannelsSelect58Band );
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS, 0, (u8*)&params, sizeof(type_functions_parameters)) )
         valuesToUI();      
   }

   if ( 2 == m_iConfirmationId && 1 == returnValue )
   {
      type_functions_parameters params;
      memcpy(&params, &(g_pCurrentModel->functions_params), sizeof(type_functions_parameters));
   
      params.uChannels433FreqSwitch[1] = s_uChannelsSelect433Band;
      params.uChannels868FreqSwitch[1] = s_uChannelsSelect868Band;
      params.uChannels23FreqSwitch[1] = s_uChannelsSelect23Band;
      params.uChannels24FreqSwitch[1] = s_uChannelsSelect24Band;
      params.uChannels25FreqSwitch[1] = s_uChannelsSelect25Band;
      params.uChannels58FreqSwitch[1] = s_uChannelsSelect58Band;
      log_line("New channels set for secondary frequency: 433: %u, 868: %u, 23: %u, 24: %u, 25: %u, 58: %u", s_uChannelsSelect433Band, s_uChannelsSelect868Band, s_uChannelsSelect23Band, s_uChannelsSelect24Band, s_uChannelsSelect25Band, s_uChannelsSelect58Band );
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS, 0, (u8*)&params, sizeof(type_functions_parameters)) )
         valuesToUI();      
   }
#endif

}

void MenuVehicleFunctions::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   bool sendToVehicle = false;

#ifdef FEATURE_ENABLE_RC_FREQ_SWITCH

   if ( m_IndexFreqSelectChannels1 == m_SelectedIndex )
   {
      u8 controllerBands = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         controllerBands |= pNICInfo->supportedBands;
      }
      
      u32 uFrequencyBands = g_pCurrentModel->nic_supported_bands[0];
      uFrequencyBands &= controllerBands;
      s_uChannelsSelect433Band = g_pCurrentModel->functions_params.uChannels433FreqSwitch[0];
      s_uChannelsSelect868Band = g_pCurrentModel->functions_params.uChannels868FreqSwitch[0];
      s_uChannelsSelect23Band = g_pCurrentModel->functions_params.uChannels23FreqSwitch[0];
      s_uChannelsSelect24Band = g_pCurrentModel->functions_params.uChannels24FreqSwitch[0];
      s_uChannelsSelect25Band = g_pCurrentModel->functions_params.uChannels25FreqSwitch[0];
      s_uChannelsSelect58Band = g_pCurrentModel->functions_params.uChannels58FreqSwitch[0];
      m_iConfirmationId = 1;
      add_menu_to_stack(new MenuChannelsSelect(uFrequencyBands));
      return;
   }

   if ( m_IndexFreqSelectChannels2 == m_SelectedIndex )
   {
      u8 controllerBands = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         controllerBands |= pNICInfo->supportedBands;
      }
      
      u32 uFrequencyBands = g_pCurrentModel->nic_supported_bands[1];
      uFrequencyBands &= controllerBands;
      s_uChannelsSelect433Band = g_pCurrentModel->functions_params.uChannels433FreqSwitch[1];
      s_uChannelsSelect868Band = g_pCurrentModel->functions_params.uChannels868FreqSwitch[1];
      s_uChannelsSelect23Band = g_pCurrentModel->functions_params.uChannels23FreqSwitch[1];
      s_uChannelsSelect24Band = g_pCurrentModel->functions_params.uChannels24FreqSwitch[1];
      s_uChannelsSelect25Band = g_pCurrentModel->functions_params.uChannels25FreqSwitch[1];
      s_uChannelsSelect58Band = g_pCurrentModel->functions_params.uChannels58FreqSwitch[1];
      m_iConfirmationId = 2;
      add_menu_to_stack(new MenuChannelsSelect(uFrequencyBands));
      return;
   }
   
   if ( m_IndexEnableFreqSwitch1 == m_SelectedIndex || m_IndexEnableFreqSwitch2 == m_SelectedIndex )
      sendToVehicle = true;

   if ( m_IndexFreqRCChannel1 == m_SelectedIndex || m_IndexFreqRCChannel2 == m_SelectedIndex )
      sendToVehicle = true;

   if ( m_IndexFreqRCSwitchType1 == m_SelectedIndex || m_IndexFreqRCSwitchType2 == m_SelectedIndex )
      sendToVehicle = true;
#endif

   if ( sendToVehicle )
   {
      sendParams();
      return;
   }
}
