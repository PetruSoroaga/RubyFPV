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
#include "menu.h"
#include "menu_objects.h"
#include "menu_controller_radio_interface.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_confirmation.h"
#include "menu_item_section.h"
#include "menu_item_text.h"

#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../common/string_utils.h"
#include "../renderer/render_engine.h"

#include "pairing.h"
#include "colors.h"

#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "ruby_central.h"

#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

MenuControllerRadioInterface::MenuControllerRadioInterface(int iInterfaceIndex)
:Menu(MENU_ID_CONTROLLER_RADIO_INTERFACE, "Controller Radio Interface Settings", NULL)
{
   m_Width = 0.3;
   m_xPos = 0.08;
   m_yPos = 0.1;
   m_pPopupProgress = NULL;
   m_iInterfaceIndex = iInterfaceIndex;
   
   ControllerSettings* pCS = get_ControllerSettings();

   load_ControllerInterfacesSettings();

   char szBuff[1024];

   m_IndexDataRate = -1;
   m_IndexName = -1;

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      addTopLine("No radio interfaces detected on the system!");
      return;
   }

   m_pItemsSelect[1] = createMenuItemCardModelSelector("Card Model");
   m_IndexCardModel = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("Enabled", "Enables or disables this card.");
   m_pItemsSelect[2]->addSelection("No");
   m_pItemsSelect[2]->addSelection("Yes");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexEnabled = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[0] = new MenuItemSelect("Preferred Uplink Card", "Sets this card as preffered one for uplink.");
   m_pItemsSelect[0]->addSelection("No (Auto)");
   m_pItemsSelect[0]->addSelection("Yes");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexTXPreferred = addMenuItem(m_pItemsSelect[0]);

   if ( 1 == hardware_get_radio_interfaces_count() )
      addMenuItem(new MenuItemText("You have a single radio interface on this controller. You can not change it's main functionality."));

   /*
   m_pItemsSelect[3] = new MenuItemSelect("Usage", "Sets what this radio interface is used for.");
   m_pItemsSelect[3]->addSelection("Video & Data");
   m_pItemsSelect[3]->addSelection("Video Only");
   m_pItemsSelect[3]->addSelection("Data Only");
   //m_pItemsSelect[3]->addSelection("Secondary Controller");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexUsage = addMenuItem(m_pItemsSelect[3]);
   */
   m_IndexUsage = -1;

   m_pItemsSelect[4] = new MenuItemSelect("Capabilities", "Sets the uplink/downlink capabilities of the card. If the card has attached an external LNA or a unidirectional booster, it can't be used for both uplink and downlink, so it must be marked to be used only for uplink or downlink accordingly.");
   m_pItemsSelect[4]->addSelection("Uplink & Downlink");
   m_pItemsSelect[4]->addSelection("Downlink Only");
   m_pItemsSelect[4]->addSelection("Uplink Only");
   m_pItemsSelect[4]->setIsEditable();
   m_IndexCapabilities = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[5] = new MenuItemSelect("Radio Data Rate", "Overwrites the radio link datarate settings and sets a physical radio data rate to use on this interface. Default (auto) is to use the vehicle radio datarate set for the radio link that will be associated with this interface. Or can be set to a fixed custom value, no matter what the vehicle radio link configuration is.");
   m_pItemsSelect[5]->addSelection("Auto (Radio Link)");
   for( int i=0; i<getDataRatesCount(); i++ )
   {
      sprintf(szBuff, "%d Mbps", getDataRates()[i]);
      m_pItemsSelect[5]->addSelection(szBuff);
   }
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
   {
      sprintf(szBuff, "MCS-%d (%d Mbps)", i, getMCSRate(i));
      m_pItemsSelect[5]->addSelection(szBuff);
   }
   m_pItemsSelect[5]->setIsEditable();
   m_IndexDataRate = addMenuItem(m_pItemsSelect[5]);

   m_pItemsSelect[6] = new MenuItemSelect("Card Location", "Marks this card as internal or external.");
   m_pItemsSelect[6]->addSelection("External");
   m_pItemsSelect[6]->addSelection("Internal");
   m_pItemsSelect[6]->setIsEditable();
   m_IndexInternal = addMenuItem(m_pItemsSelect[6]);

   m_pItemsEdit[0] = new MenuItemEdit("Custom Name", "Sets a custom name for this unique physical radio interface; This name is to be displayed in the OSD and menus, to easily identify and distinguish each physical radio interface from the others.", "");
   m_pItemsEdit[0]->setMaxLength(48);
   m_IndexName = addMenuItem(m_pItemsEdit[0]);
}

void MenuControllerRadioInterface::valuesToUI()
{
   m_ExtraHeight = 0;

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();
   ControllerSettings* pCS = get_ControllerSettings();

   if ( 0 == hardware_get_radio_interfaces_count() || m_iInterfaceIndex < 0 || m_iInterfaceIndex >= hardware_get_radio_interfaces_count() )
       return;

   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
      
   t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNIC->szMAC);
      
   if ( NULL == pCardInfo )
      return;

   m_pItemsSelect[0]->setEnabled(true);
   m_pItemsSelect[0]->setSelection(0);
   if ( controllerIsCardTXPreferred(pNIC->szMAC) && (! controllerIsCardDisabled(pNIC->szMAC) ) )
      m_pItemsSelect[0]->setSelectedIndex(1);

   if ( pCardInfo->cardModel < 0 )
   {
      m_pItemsSelect[1]->setSelection(-pCardInfo->cardModel);
      m_pItemsSelect[1]->setEnabled(true);
   }
   else
   {
      m_pItemsSelect[1]->setSelection(pCardInfo->cardModel);
      m_pItemsSelect[1]->setEnabled(true);
   }


   m_pItemsSelect[2]->setSelection(1);
   m_pItemsSelect[4]->setSelection(0);

   m_pItemsSelect[2]->setEnabled(true);
   m_pItemsSelect[4]->setEnabled(true);

   if ( -1 != m_IndexUsage )
   {
      m_pItemsSelect[3]->setSelection(0);
      m_pItemsSelect[3]->setEnabled(true);
   }

   if ( 1 == hardware_get_radio_interfaces_count() )
   {
      m_pItemsSelect[2]->setSelection(1);
      m_pItemsSelect[2]->setEnabled(false);
      if ( -1 != m_IndexUsage )
         m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[4]->setEnabled(false);
      controllerRemoveCardDisabled(pNIC->szMAC);
      controllerSetCardTXRX(pNIC->szMAC);
   }

     
   if ( controllerIsCardDisabled(pNIC->szMAC) )
   {
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSelect[0]->setEnabled(false);
      m_pItemsSelect[2]->setSelection(0);
      if ( -1 != m_IndexUsage )
         m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[4]->setEnabled(false);
      m_pItemsSelect[5]->setEnabled(false);
   }

   if ( controllerIsCardRXOnly(pNIC->szMAC) )
      m_pItemsSelect[4]->setSelection(1);
   if ( controllerIsCardTXOnly(pNIC->szMAC) )
      m_pItemsSelect[4]->setSelection(2);

   u32 cardFlags = controllerGetCardFlags(pNIC->szMAC);

   if ( -1 != m_IndexUsage )
   {
      if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) && (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
         m_pItemsSelect[3]->setSelection(0);
      else if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         m_pItemsSelect[3]->setSelection(1);
      else if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
         m_pItemsSelect[3]->setSelection(2);

      if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_RELAY )
         m_pItemsSelect[3]->setSelection(3);
   }

   int nRateTx = controllerGetCardDataRate(pNIC->szMAC);
   if ( 0 == nRateTx )
      m_pItemsSelect[5]->setSelection(0);
   else
   {
      int selectedIndex = 0;
      for( int i=0; i<getDataRatesCount(); i++ )
         if ( nRateTx == getDataRates()[i] )
            selectedIndex = i+1;
      for( int i=0; i<=MAX_MCS_INDEX; i++ )
         if ( nRateTx == -1-i )
            selectedIndex = i+getDataRatesCount()+1;

      m_pItemsSelect[5]->setSelection(selectedIndex);
   }

   m_pItemsSelect[6]->setSelectedIndex(controllerIsCardInternal(pNIC->szMAC));

   if ( NULL != m_pItemsEdit[0] )
      m_pItemsEdit[0]->setCurrentValue(controllerGetCardUserDefinedName(pNIC->szMAC));

   if ( pCS->nAutomaticTxCard )
      m_pItemsSelect[0]->setSelectedIndex(0);
}


void MenuControllerRadioInterface::onShow()
{
   Menu::onShow();
}

void MenuControllerRadioInterface::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
   }
   RenderEnd(yTop);
}

void MenuControllerRadioInterface::showProgressInfo()
{
   ruby_pause_watchdog();
   m_pPopupProgress = new Popup("Updating Radio Configuration. Please wait...",0.3,0.4, 0.5, 15);
   popups_add_topmost(m_pPopupProgress);

   g_pRenderEngine->startFrame();
   popups_render();
   popups_render_topmost();
   g_pRenderEngine->endFrame();

}

void MenuControllerRadioInterface::hideProgressInfo()
{
   popups_remove(m_pPopupProgress);
   m_pPopupProgress = NULL;
}

void MenuControllerRadioInterface::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

bool MenuControllerRadioInterface::checkFlagsConsistency()
{
   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
   if ( NULL == pNIC )
      return false;

   u32 cardFlags = controllerGetCardFlags(pNIC->szMAC);

   int enabled = m_pItemsSelect[2]->getSelectedIndex();

   if ( 0 == enabled )
   {
      bool anyEnabled = false;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( i != m_iInterfaceIndex )
      {
         radio_hw_info_t* pNIC2 = hardware_get_radio_info(i);
         if ( NULL == pNIC2 )
            break;
         u32 flags = controllerGetCardFlags(pNIC2->szMAC);
         if ( !(flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
         {
            anyEnabled = true;
            break;
         }
      }
      if ( ! anyEnabled )
      {
         addMessage("You can't disable all the radio interfaces!");
         return false;
      }
   }
   return true;
}

bool MenuControllerRadioInterface::setCardFlags()
{
   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
   if ( NULL == pNIC )
      return false;

   if ( ! checkFlagsConsistency() )
      return false;

   u32 cardFlags = controllerGetCardFlags(pNIC->szMAC);

   int enabled = m_pItemsSelect[2]->getSelectedIndex();
   if ( 1 == enabled )
   {
      controllerRemoveCardDisabled(pNIC->szMAC);
      cardFlags &= (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
   }
   else
   {
      controllerSetCardDisabled(pNIC->szMAC);
      cardFlags |= RADIO_HW_CAPABILITY_FLAG_DISABLED;
   }

   int indexCapabilities = m_pItemsSelect[4]->getSelectedIndex();

   cardFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;

   if ( 0 == indexCapabilities )
      controllerSetCardTXRX(pNIC->szMAC);
   if ( 1 == indexCapabilities )
   {
      controllerSetCardRXOnly(pNIC->szMAC);
      cardFlags &= (~RADIO_HW_CAPABILITY_FLAG_CAN_TX);
   }
   if ( 2 == indexCapabilities )
   {
      controllerSetCardTXOnly(pNIC->szMAC);
      cardFlags &= (~RADIO_HW_CAPABILITY_FLAG_CAN_RX);
   }

   int indexUsage = 0;

   if ( -1 != m_IndexUsage )
      indexUsage = m_pItemsSelect[3]->getSelectedIndex();

   cardFlags &= (~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
   cardFlags &= (~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);
   cardFlags &= (~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_RELAY);
   if ( 0 == indexUsage )
      cardFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   if ( 1 == indexUsage )
      cardFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
   if ( 2 == indexUsage )
      cardFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   if ( 3 == indexUsage )
      cardFlags |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_RELAY;

   controllerSetCardFlags(pNIC->szMAC, cardFlags);
   save_ControllerInterfacesSettings();
   return true;
}

int MenuControllerRadioInterface::onBack()
{
   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);
   if ( NULL != pNIC )
   if ( m_IndexName == m_SelectedIndex )
   if ( m_pItemsEdit[0]->isEditing() )
   {
      m_pItemsEdit[0]->endEdit(false);
      char szBuff[1024];
      strcpy(szBuff, m_pItemsEdit[0]->getCurrentValue());
      controllerSetCardUserDefinedName(pNIC->szMAC, szBuff);
      save_ControllerInterfacesSettings();
      invalidate();
      valuesToUI();
      return 1;
   }

   return Menu::onBack();
}


void MenuControllerRadioInterface::onSelectItem()
{
   if ( m_IndexName == m_SelectedIndex )
   {
      m_pItemsEdit[0]->beginEdit();
      return;
   }

   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   char szBuff[256];

   radio_hw_info_t* pNIC = hardware_get_radio_info(m_iInterfaceIndex);

   if ( m_IndexCardModel == m_SelectedIndex )
   {
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNIC->szMAC);
      if ( NULL != pCardInfo )
      {
         pCardInfo->cardModel = m_pItemsSelect[1]->getSelectedIndex();
         if ( pCardInfo->cardModel > 0 )
            pCardInfo->cardModel = - pCardInfo->cardModel;
         save_ControllerInterfacesSettings();
         valuesToUI();
      }
      return;
   }

   if ( m_IndexEnabled == m_SelectedIndex )
   {
      if ( setCardFlags() )
      {
         showProgressInfo();
         pairing_stop();
         pairing_start();
         hideProgressInfo();
      }
      else
         valuesToUI();
      return;
   }

   if ( m_IndexCapabilities == m_SelectedIndex )
   {
      if ( 1 == hardware_get_radio_interfaces_count()  )
      {
            Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Invalid option",NULL);
            pm->m_xPos = 0.4; pm->m_yPos = 0.4;
            pm->m_Width = 0.36;
            pm->addTopLine("Can't set the interface as RX or TX only because it's the only interface on the system.");
            add_menu_to_stack(pm);
            valuesToUI();
            return;
      }

      if ( setCardFlags() )
      {
         showProgressInfo();
         pairing_stop();
         pairing_start();
         hideProgressInfo();
      }
      else
         valuesToUI();  
      return;
   }

   if ( -1 != m_IndexUsage )
   if ( m_IndexUsage == m_SelectedIndex )
   {
      if ( setCardFlags() )
      {
         showProgressInfo();
         pairing_stop();
         pairing_start();
         hideProgressInfo();
      }
      else
         valuesToUI();
      return;
   }

   if ( m_IndexDataRate == m_SelectedIndex )
   {
         int dataRate = 0; // Auto
         int indexRate = m_pItemsSelect[5]->getSelectedIndex();
         if ( indexRate > 0 && indexRate <= getDataRatesCount() )
            dataRate = getDataRates()[indexRate-1];
         else if ( indexRate > getDataRatesCount() )
            dataRate = -1-(indexRate - getDataRatesCount()-1);

         controllerSetCardDataRate(pNIC->szMAC, dataRate);
         save_ControllerInterfacesSettings();
         valuesToUI();
         showProgressInfo();
         pairing_stop();
         pairing_start();
         hideProgressInfo();
         return;
   }

   if ( m_IndexInternal == m_SelectedIndex )
   {
      controllerSetCardInternal(pNIC->szMAC, (bool)m_pItemsSelect[6]->getSelectedIndex());
      save_ControllerInterfacesSettings();
      valuesToUI();
      return;
   }

   if ( m_IndexTXPreferred == m_SelectedIndex )
   {
      int index = m_pItemsSelect[0]->getSelectedIndex();
      if ( 1 == index )
      {
         if ( controllerIsCardDisabled(pNIC->szMAC) )
            return;

         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            radio_hw_info_t* pNIC2 = hardware_get_radio_info(i);
            bool bOnSameRadioLink = true;
            if ( NULL != g_pSM_RadioStats )
            if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId != g_pSM_RadioStats->radio_interfaces[m_iInterfaceIndex].assignedRadioLinkId )
               bOnSameRadioLink = false;
            if ( i != m_iInterfaceIndex )
            if ( bOnSameRadioLink )
            if ( controllerIsCardTXPreferred(pNIC2->szMAC) )
               controllerRemoveCardTXPreferred(pNIC->szMAC);
         }
        
         controllerSetCardTXPreferred(pNIC->szMAC);
         save_ControllerInterfacesSettings();
         valuesToUI();
         showProgressInfo();
         pairing_stop();
         pairing_start();
         hideProgressInfo();
         return;
      }
   }
}
