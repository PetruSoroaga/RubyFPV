/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
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

#include "../render_commands.h"
#include "../../base/controller_utils.h"
#include "osd.h"
#include "osd_common.h"
#include "../popup.h"
#include "menu.h"
#include "menu_radio_config.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"
#include "menu_txinfo.h"
#include "menu_controller_radio_interface.h"
#include "menu_controller_radio_interface_sik.h"
#include "menu_vehicle_radio_link.h"
#include "menu_vehicle_radio_link_sik.h"
#include "menu_vehicle_radio_interface.h"
#include "menu_diagnose_radio_link.h"

#include <ctype.h>
#include <math.h>
#include "../process_router_messages.h"
#include "../link_watch.h"
#include "../pairing.h"

#define MRC_ID_SET_PREFFERED_TX 1
#define MRC_ID_SET_TX_POWER_CONTROLLER_58 2
#define MRC_ID_SET_TX_POWER_CONTROLLER_24 3
#define MRC_ID_SET_TX_POWER_CONTROLLER_2458 4
#define MRC_ID_SET_TX_POWER_CONTROLLER_SIK 5
#define MRC_ID_SET_TX_POWER_VEHICLE_58 6
#define MRC_ID_SET_TX_POWER_VEHICLE_24 7
#define MRC_ID_SET_TX_POWER_VEHICLE_2458 8
#define MRC_ID_SET_TX_POWER_VEHICLE_SIK 9
#define MRC_ID_CONFIGURE_RADIO_LINK 10
#define MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER 11
#define MRC_ID_CONFIGURE_RADIO_INTERFACE_VEHICLE 12
#define MRC_ID_SWITCH_RADIO_LINK 14
#define MRC_ID_SWAP_VEHICLE_RADIO_INTERFACES 15
#define MRC_ID_ROTATE_RADIO_LINKS 16
#define MRC_ID_DIAGNOSE_RADIO_LINK 20


static int s_iListSiKPowers[] = {1, 2, 5, 8, 11, 14, 17, 20};

MenuRadioConfig::MenuRadioConfig(void)
:Menu(MENU_ID_RADIO_CONFIG, "Radio Configuration", NULL)
{
   m_Width = 0.17;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.16;
   m_bDisableStacking = true;
   m_bDisableBackgroundAlpha = true;
   m_fHeaderHeight = 0.0;
   m_fTotalHeightRadioConfig = 0.0;
   m_bComputedHeights = false;
   m_bHasSwapInterfacesCommand = false;
   m_bHasRotateRadioLinksOrderCommand = false;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_bHasSwitchRadioLinkCommand[i] = false;
      m_bHasDiagnoseRadioLinkCommand[i] = false;
   }
   m_szCurrentTooltip[0] = 0;
   for( int i=0; i<50; i++ )
      m_szTooltips[i][0] = 0;

   m_bShowOnlyControllerUnusedInterfaces = false;
   m_bConfigSiKPowerVehicle = false;
   m_pItemSiKTxPower = NULL;
   m_pItemSelectTxCard = NULL;
   m_fXPosSiKTxPowerController = 0.1;
   m_fXPosSiKTxPowerVehicle = 0.1;
   m_iIndexCurrentItem = 0;

   m_iIdFontRegular = g_idFontMenu;
   m_iIdFontSmall = g_idFontMenuSmall;
   m_iIdFontLarge = g_idFontMenuLarge;
}

MenuRadioConfig::~MenuRadioConfig()
{
}

void MenuRadioConfig::onShow()
{
   m_Height = 0.0;
   m_bComputedHeights = false;
   log_line("Entering menu radio config...");
   removeAllItems();

   m_fFooterHeight = 1.0 * g_pRenderEngine->textHeight(m_iIdFontRegular) + m_sfMenuPaddingY;
   m_szCurrentTooltip[0] = 0;

   m_bShowOnlyControllerUnusedInterfaces = false;

   if ( (0 == getControllerModelsCount()) || (NULL == g_pCurrentModel) || (! g_bFirstModelPairingDone) )
      m_bShowOnlyControllerUnusedInterfaces = true;

   m_iCurrentRadioLink = -1;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_fYPosAutoTx[i] = 0.5;
      m_fYPosCtrlRadioInterfaces[i] = 0.2;
      m_fYPosVehicleRadioInterfaces[i] = 0.2;
      m_fYPosRadioLinks[i] = 0.2;
   }

   m_uBandsSiKVehicle = 0;
   m_uBandsSiKController = 0;

   m_bTmpHasVideoStreamsEnabled = false;
   m_bTmpHasDataStreamsEnabled = false;

   m_iCountVehicleRadioLinks = 0;
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
      m_iCountVehicleRadioLinks = g_pCurrentModel->radioLinksParams.links_count;

   log_line("Menu Radio Config: vehicle has %d radio links.", m_iCountVehicleRadioLinks);

   computeMenuItems();

   if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
      m_iIndexCurrentItem = m_iIndexMaxItem-1;
     
   if ( (0 < g_SM_RadioStats.countVehicleRadioLinks) || (0 < g_SM_RadioStats.countLocalRadioLinks) )
      computeHeights();

   setTooltipText();

   Menu::onShow();

   if ( ! (menu_is_menu_on_top(this)) )
   if ( ! (menu_has_menu(MENU_ID_TXINFO)) )
   {
      log_line("Menu radio config is not on top, close the top menu.");
      menu_stack_pop(0);
   }
   log_line("Entered menu radio config.");
}

void MenuRadioConfig::setTooltip(int iItemIndex, const char* szTooltip)
{
   if ( iItemIndex < 0 || iItemIndex >= 50 )
      return;
   if ( NULL == szTooltip )
      m_szTooltips[iItemIndex][0] = 0;
   else
      strcpy(m_szTooltips[iItemIndex], szTooltip);
}

void MenuRadioConfig::setTooltipText()
{
   if ( m_iIndexCurrentItem >= 0 && m_iIndexCurrentItem < 50 )
      strcpy(m_szCurrentTooltip, m_szTooltips[m_iIndexCurrentItem]);
   else
      m_szCurrentTooltip[0] = 0;
}

void MenuRadioConfig::computeMenuItems()
{
   log_line("MenuRadioConfig: Compute Menu Items...");
   if ( NULL != m_pItemSiKTxPower )
      removeMenuItem(m_pItemSiKTxPower);
   
   if ( NULL != m_pItemSelectTxCard )
      removeMenuItem(m_pItemSelectTxCard);

   m_pItemSiKTxPower = NULL;
   m_pItemSelectTxCard = NULL;

   m_bHas58PowerVehicle = false;
   m_bHas24PowerVehicle = false;
   m_bHasSiKPowerVehicle = false;
   m_bHas58PowerController = false;
   m_bHas24PowerController = false;
   m_bHasSiKPowerController = false;

   m_bHasSwapInterfacesCommand = false;
   m_bHasRotateRadioLinksOrderCommand = false;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_bHasSwitchRadioLinkCommand[i] = false;
      m_bHasDiagnoseRadioLinkCommand[i] = false;
   }

   if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
      m_bHasRotateRadioLinksOrderCommand = true;

   m_iHeaderItemsCount = 0;
   m_iCountItemsTxPowerVehicle = 0;
   m_iCountItemsTxPowerController = 0;

   // Check controller interfaces

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      log_line("MenuRadio: Detected controller radio interface %d type: %s", i+1, str_get_radio_type_description(pRadioHWInfo->typeAndDriver));
      if ( hardware_radio_index_is_sik_radio(i) )
      {
         m_bHasSiKPowerController = true;
         if ( NULL != pRadioHWInfo )
            m_uBandsSiKController |= pRadioHWInfo->supportedBands;
      }
      else
      {
         if ( (NULL != pRadioHWInfo) && (((pRadioHWInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS) || (((pRadioHWInfo->typeAndDriver & 0xFF) == RADIO_TYPE_RALINK))) )
            m_bHas24PowerController = true;
         else
            m_bHas58PowerController = true;
      }
   }

   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
   {
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         log_line("MenuRadio: Detected vehicle radio interface %d type: %s", i+1, str_get_radio_type_description(g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i]));

         if ( (g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF) == RADIO_TYPE_SIK )
         {
            m_bHasSiKPowerVehicle = true;
            m_uBandsSiKVehicle |= g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i];
         }
         else if ( ((g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF) == RADIO_TYPE_ATHEROS) || ((g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF) == RADIO_TYPE_RALINK) )
            m_bHas24PowerVehicle = true;
         else
            m_bHas58PowerVehicle = true;

      }
   }

   // Check if swap vehicle radio interfaces is possible
   m_bHasSwapInterfacesCommand = false;
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )  
   if ( g_pCurrentModel->bDeveloperMode )
   if ( g_pCurrentModel->canSwapEnabledHighCapacityRadioInterfaces() )
       m_bHasSwapInterfacesCommand = true;

   // Check to see to which radio links to add the swhich radio link commands
   
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone )
   for( int iVehicleRadioLink=0; iVehicleRadioLink<m_iCountVehicleRadioLinks; iVehicleRadioLink++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      if ( (! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO)) &&
           (! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) )
         continue;

      int iCountInterfacesAssignedToThisLink = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;

         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL == pCardInfo )
            continue;

         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iVehicleRadioLink )
            iCountInterfacesAssignedToThisLink++;
      }

      int iCountInterfacesAssignableToThisLink = controller_count_asignable_radio_interfaces_to_vehicle_radio_link(g_pCurrentModel, iVehicleRadioLink);
      if ( (0 == iCountInterfacesAssignedToThisLink) && (0 < iCountInterfacesAssignableToThisLink) )
      {
         m_bHasSwitchRadioLinkCommand[iVehicleRadioLink] = true;
         log_line("MenuRadioConfig: Radio link %d (of %d) has switch command.", iVehicleRadioLink+1, m_iCountVehicleRadioLinks);
      }
   }

   for( int iVehicleRadioLink=0; iVehicleRadioLink<m_iCountVehicleRadioLinks; iVehicleRadioLink++ )
   {
      m_bHasDiagnoseRadioLinkCommand[iVehicleRadioLink] = false;
      //if ( g_pCurrentModel->radioLinkIsSiKRadio(iVehicleRadioLink) )
      //   m_bHasDiagnoseRadioLinkCommand[iVehicleRadioLink] = true;
   }
   computeMaxItemIndexAndCommands();
}

void MenuRadioConfig::Render()
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   double color[4];
   color[0] = color[1] = color[2] = 150;
   color[3] = 0.3;
   g_pRenderEngine->setFontBackgroundBoundingBoxFillColor(color);
   g_pRenderEngine->setBackgroundBoundingBoxPadding(0.007);

   if ( ! m_bComputedHeights )
   {
      if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
         m_iCountVehicleRadioLinks = g_pCurrentModel->radioLinksParams.links_count;

      computeHeights();
      computeMaxItemIndexAndCommands();
   }

   // Draw radio links

   float yEnd = drawRadioLinks(0.05/g_pRenderEngine->getAspectRatio(), 0.68);
   g_pRenderEngine->clearFontBackgroundBoundingBoxStrikeColor();

   
   if ( NULL != m_pItemSelectTxCard )
   {
      float xPos = m_RenderXPos + g_pRenderEngine->textWidth(m_iIdFontRegular, "TxCard");

      m_pItemSelectTxCard->setLastRenderPos(xPos, m_fYPosAutoTx[m_iCurrentRadioLink]-height_text);
      m_pItemSelectTxCard->getItemHeight(0.5);
      m_pItemSelectTxCard->Render(xPos, m_fYPosAutoTx[m_iCurrentRadioLink]-height_text, true, 0.0);
   }

   if ( NULL != m_pItemSiKTxPower )
   {
      float xPos = m_fXPosSiKTxPowerController;
      if ( m_bConfigSiKPowerVehicle )
         xPos = m_fXPosSiKTxPowerVehicle;

      m_pItemSiKTxPower->setLastRenderPos(xPos, m_fHeaderHeight-height_text);
      m_pItemSiKTxPower->getItemHeight(0.5);
      m_pItemSiKTxPower->Render(xPos, m_fHeaderHeight-height_text, true, 0.0);
   }

   // Draw bottom items

   float xLeft = m_RenderXPos + m_RenderWidth - 2.0*m_sfMenuPaddingX;
   yEnd -= height_text;
   if ( m_bHasRotateRadioLinksOrderCommand )
   {
      bool bBBox = false;
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_ROTATE_RADIO_LINKS )
            bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      
      float fWidthText = g_pRenderEngine->textWidth(m_iIdFontRegular, "Rotate Radio Links Order");
      g_pRenderEngine->drawTextLeft(xLeft, yEnd, m_iIdFontRegular, "Rotate Radio Links Order");

      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_ROTATE_RADIO_LINKS )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      xLeft -= fWidthText + 2.0 * m_sfMenuPaddingX;
   }

   if ( m_bHasSwapInterfacesCommand )
   {
      bool bBBox = false;
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SWAP_VEHICLE_RADIO_INTERFACES )
            bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      
      float fWidthText = g_pRenderEngine->textWidth(m_iIdFontRegular, "Swap Vehicle's Radio Interfaces");
      g_pRenderEngine->drawTextLeft(xLeft, yEnd, m_iIdFontRegular, "Swap Vehicle's Radio Interfaces");

      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SWAP_VEHICLE_RADIO_INTERFACES )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      xLeft -= fWidthText + 2.0 * m_sfMenuPaddingX;
   }

}


void MenuRadioConfig::showProgressInfo()
{
   ruby_pause_watchdog();
   m_pPopupProgress = new Popup("Updating Radio Configuration. Please wait...",0.3,0.4, 0.5, 15);
   popups_add_topmost(m_pPopupProgress);

   g_pRenderEngine->startFrame();
   popups_render();
   popups_render_topmost();
   g_pRenderEngine->endFrame();
}

void MenuRadioConfig::hideProgressInfo()
{
   popups_remove(m_pPopupProgress);
   m_pPopupProgress = NULL;
}


void MenuRadioConfig::onMoveUp(bool bIgnoreReversion)
{
   if ( g_bSwitchingRadioLink )
      return;
   if ( (NULL != m_pItemSelectTxCard) || (NULL != m_pItemSiKTxPower) )
   {
      Menu::onMoveUp(bIgnoreReversion);
      setTooltipText();
      return;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      setTooltipText();
      return;
   }

   m_iIndexCurrentItem--;
   if ( m_iIndexCurrentItem < 0 )
      m_iIndexCurrentItem = m_iIndexMaxItem-1;

   setTooltipText();
}

void MenuRadioConfig::onMoveDown(bool bIgnoreReversion)
{
   if ( g_bSwitchingRadioLink )
      return;
   if ( (NULL != m_pItemSelectTxCard) || (NULL != m_pItemSiKTxPower) )
   {
      Menu::onMoveDown(bIgnoreReversion);
      setTooltipText();
      return;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      setTooltipText();
      return;
   }
   m_iIndexCurrentItem++;
   if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
      m_iIndexCurrentItem = 0;

   setTooltipText();
}

void MenuRadioConfig::onMoveLeft(bool bIgnoreReversion)
{
   if ( g_bSwitchingRadioLink )
      return;
   onMoveUp(bIgnoreReversion);
}

void MenuRadioConfig::onMoveRight(bool bIgnoreReversion)
{
   if ( g_bSwitchingRadioLink )
      return;
   onMoveDown(bIgnoreReversion);
}

void MenuRadioConfig::onFocusedItemChanged()
{
   setTooltipText();
}

void MenuRadioConfig::onItemValueChanged(int itemIndex)
{
}

void MenuRadioConfig::onItemEndEdit(int itemIndex)
{
   if ( NULL != m_pItemSelectTxCard )
   {
      int iSelection = m_pItemSelectTxCard->getSelectedIndex();
      log_line("Changed auto tx card for radio link %d to: %d", m_iCurrentRadioLink+1, iSelection);

      // Remove all first
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId != m_iCurrentRadioLink )
            continue;
         controllerRemoveCardTXPreferred(pNICInfo->szMAC);
      }
        
      if ( iSelection > 0 )
      {
         int iCount = 0;
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
            if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId != m_iCurrentRadioLink )
               continue;
            iCount++;
            if ( iCount == iSelection )
            {
               controllerSetCardTXPreferred(pNICInfo->szMAC);
               break;
            }
         }
      }
      removeAllItems();
      m_pItemSelectTxCard = NULL;

      showProgressInfo();
      pairing_stop();
      pairing_start_normal();
      hideProgressInfo();   
   }

   if ( NULL != m_pItemSiKTxPower )
   {
      int iSelection = m_pItemSiKTxPower->getSelectedIndex();
      int iTxPower = 0;
      if ( iSelection >= 0 && (iSelection < sizeof(s_iListSiKPowers)/sizeof(s_iListSiKPowers[0])) )
         iTxPower = s_iListSiKPowers[iSelection];
      log_line("Changed SiK Tx power to index: %d, power: %d", iSelection, iTxPower);
      removeMenuItem(m_pItemSiKTxPower);
      m_pItemSiKTxPower = NULL;

      if ( m_bConfigSiKPowerVehicle )
      { 
         u8 buffer[11];
         memset(&(buffer[0]), 0xFF, 11);
         buffer[8] = 0x81;
         buffer[9] = iTxPower;
         buffer[10] = 0x81;
        
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TX_POWERS, 0, buffer, 11) )
             valuesToUI();
      }
      else
      {
         ControllerSettings* pCS = get_ControllerSettings();
         pCS->iTXPowerSiK = iTxPower;
         save_ControllerSettings();
         send_model_changed_message_to_router(MODEL_CHANGED_RADIO_POWERS, 0);
      }
      valuesToUI();
   }
}
      

void MenuRadioConfig::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
   computeMaxItemIndexAndCommands();
}

void MenuRadioConfig::onClickAutoTx(int iRadioLink)
{
   m_iCurrentRadioLink = iRadioLink;
   m_pItemSelectTxCard = new MenuItemSelect("");
   m_pItemSelectTxCard->addSelection("Auto");

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId != iRadioLink )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;

      if ( controllerIsCardRXOnly(pRadioHWInfo->szMAC) )
         continue;

      char szBuff[256];
      char szName[128];
   
      strcpy(szName, "NoName");
   
      char* szCardName = controllerGetCardUserDefinedName(pRadioHWInfo->szMAC);
      if ( NULL != szCardName && 0 != szCardName[0] )
         strcpy(szName, szCardName);
      else if ( NULL != pCardInfo )
         strcpy(szName, str_get_radio_card_model_string(pCardInfo->cardModel) );

      sprintf(szBuff, "Int. %d, Port %s, %s", i+1, pRadioHWInfo->szUSBPort, szName);
      m_pItemSelectTxCard->addSelection(szBuff);
   
   }
   addMenuItem(m_pItemSelectTxCard);
   m_pItemSelectTxCard->setIsEditable();
   m_pItemSelectTxCard->setPopupSelectorToRight();
   m_pItemSelectTxCard->beginEdit();
}

void MenuRadioConfig::onClickSiKTxPower(bool bVehicle)
{
   m_bConfigSiKPowerVehicle = bVehicle;
   removeMenuItem(m_pItemSiKTxPower);   
   m_pItemSiKTxPower = new MenuItemSelect("");
   ControllerSettings* pCS = get_ControllerSettings();
   
   // 1 2 5 8 11 14 17 20
   int iSelectedIndex = -1;
   for( int i=0; i<sizeof(s_iListSiKPowers)/sizeof(s_iListSiKPowers[0]); i++ )
   {
      char szBuff[256];
      sprintf(szBuff, "%d", s_iListSiKPowers[i]);
      m_pItemSiKTxPower->addSelection(szBuff);

      if ( m_bConfigSiKPowerVehicle )
      if ( (NULL != g_pCurrentModel) && g_pCurrentModel->radioInterfacesParams.txPowerSiK == s_iListSiKPowers[i] )
         iSelectedIndex = i;

      if ( ! m_bConfigSiKPowerVehicle )
      if ( pCS->iTXPowerSiK == s_iListSiKPowers[i] )
         iSelectedIndex = i;
   }
   addMenuItem(m_pItemSiKTxPower);

   m_pItemSiKTxPower->setSelectedIndex(iSelectedIndex);
   m_pItemSiKTxPower->setIsEditable();
   m_pItemSiKTxPower->setPopupSelectorToRight();
   m_pItemSiKTxPower->beginEdit();
}

void MenuRadioConfig::onSelectItem()
{
   if ( g_bSwitchingRadioLink )
      return;

   bool bConnectedToVehicle = false;
   if ( link_has_received_main_vehicle_ruby_telemetry() )
   if ( pairing_isStarted() )
   if ( NULL != g_pCurrentModel )
   if ( NULL != get_vehicle_runtime_info_for_vehicle_id(g_pCurrentModel->vehicle_id) )
   if ( ! get_vehicle_runtime_info_for_vehicle_id(g_pCurrentModel->vehicle_id)->bLinkLost )
      bConnectedToVehicle = true;

   char szConnectTitle[32];
   char szConnectMsg[128];
   strcpy(szConnectTitle, "Vehicle radio link");
   strcpy(szConnectMsg, "To change parameters, you must be connected to the vehicle and have an active radio link to the vehicle.");

   log_line("MenuRadioConfig: Selected item index %d, command: %d, extra param: %d. Connected to vehicle? %s",
      m_iIndexCurrentItem, (int)(m_uCommandsIds[m_iIndexCurrentItem] & 0xFF),
      (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8),
      bConnectedToVehicle?"yes":"no");

   if ( (NULL != m_pItemSelectTxCard) || (NULL != m_pItemSiKTxPower) )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      Menu::onSelectItem();
      return;
   }

   log_line("MenuRadioConfig: Clicked on current item %d", m_iIndexCurrentItem);

   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   
   if ( m_bHasRotateRadioLinksOrderCommand )
   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_ROTATE_RADIO_LINKS )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      log_line("User selected command to rotate vehicle's radio links.");
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_ROTATE_RADIO_LINKS, 0, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( m_bHasSwapInterfacesCommand )
   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SWAP_VEHICLE_RADIO_INTERFACES )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      log_line("User selected command to swap vehicle's radio interfaces.");
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SWAP_RADIO_INTERFACES, 0, NULL, 0) )
         valuesToUI();
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_DIAGNOSE_RADIO_LINK )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      int iVehicleRadioLinkId = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);
      log_line("MenuRadioConfig: Selected item to diagnose vehicle radio link %d", iVehicleRadioLinkId+1);

      if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
         return;

      MenuDiagnoseRadioLink* pMenu = new MenuDiagnoseRadioLink(iVehicleRadioLinkId);
      add_menu_to_stack(pMenu);
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_24 )
   {
      MenuTXInfo* pMenu = new MenuTXInfo();
      pMenu->m_bShowVehicle = false;
      add_menu_to_stack(pMenu);
      return;
   }

   if ( ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_58) ||
        ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_2458) )
   {
      MenuTXInfo* pMenu = new MenuTXInfo();
      pMenu->m_bShowVehicle = false;
      if ( m_bHas24PowerController && m_bHas58PowerController )
         pMenu->m_bSelectSecond = true;
      add_menu_to_stack(pMenu);
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_SIK )
   {
      onClickSiKTxPower(false);
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_24 )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessage(szConnectMsg);
         return;
      }
      MenuTXInfo* pMenu = new MenuTXInfo();
      pMenu->m_bShowController = false;
      add_menu_to_stack(pMenu);
      return;
   }

   if ( ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_58) ||
        ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_2458) )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      MenuTXInfo* pMenu = new MenuTXInfo();
      pMenu->m_bShowController = false;
      if ( m_bHas24PowerVehicle && m_bHas58PowerVehicle )
         pMenu->m_bSelectSecond = true;
      add_menu_to_stack(pMenu);
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_SIK )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      onClickSiKTxPower(true);
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_CONFIGURE_RADIO_LINK )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      int iVehicleRadioLinkId = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);
      log_line("MenuRadioConfig: Selected item to configure vehicle radio link %d", iVehicleRadioLinkId+1);

      if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
         return;

      Menu* pMenu = NULL;
      if ( g_pCurrentModel->radioLinkIsSiKRadio(iVehicleRadioLinkId) )
         pMenu = new MenuVehicleRadioLinkSiK(iVehicleRadioLinkId);
      else
         pMenu = new MenuVehicleRadioLink(iVehicleRadioLinkId);
        
      int iCountInterfacesAssignedToThisLink = 0;
      for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iVehicleRadioLinkId )
            iCountInterfacesAssignedToThisLink++;
      }

      if ( 0 != iCountInterfacesAssignedToThisLink )
         pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.5;
      else
         pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.8;
      pMenu->m_yPos = m_fYPosRadioLinks[iVehicleRadioLinkId] - 5.0*height_text;
      if ( pMenu->m_yPos < 0.03 )
         pMenu->m_yPos = 0.03;
      if ( pMenu->m_yPos > 0.8 )
         pMenu->m_yPos = 0.8;
      add_menu_to_stack(pMenu);
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER )
   {
      int iRadioInterface = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);
      log_line("MenuRadioConfig: Configuring controller radio interface %d", iRadioInterface+1);
      Menu* pMenu = NULL;
      if ( hardware_radio_index_is_sik_radio(iRadioInterface) )
         pMenu = new MenuControllerRadioInterfaceSiK(iRadioInterface);
      else
         pMenu = new MenuControllerRadioInterface(iRadioInterface);
      pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.4;
      pMenu->m_yPos = m_fYPosCtrlRadioInterfaces[iRadioInterface] - 5.0*height_text;
      if ( pMenu->m_yPos < 0.03 )
         pMenu->m_yPos = 0.03;
      if ( pMenu->m_yPos > 0.8 )
         pMenu->m_yPos = 0.8;
      add_menu_to_stack(pMenu);
      return;  
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_CONFIGURE_RADIO_INTERFACE_VEHICLE )
   {
      if ( ! bConnectedToVehicle )
      {
         addMessageWithTitle(0, szConnectTitle, szConnectMsg);
         return;
      }
      int iVehicleRadioInterface = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);
      log_line("MenuRadioConfig: Configuring vehicle radio interface %d", iVehicleRadioInterface+1);

      if ( (iVehicleRadioInterface < 0) || (iVehicleRadioInterface >= g_pCurrentModel->radioInterfacesParams.interfaces_count) )
         return;
      int iVehicleRadioLink = g_pCurrentModel->radioInterfacesParams.interface_link_id[iVehicleRadioInterface];
      if ( (iVehicleRadioLink < 0) || (iVehicleRadioLink >= g_pCurrentModel->radioLinksParams.links_count) )
         return;
      MenuVehicleRadioInterface* pMenu = new MenuVehicleRadioInterface(iVehicleRadioInterface);
      pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.8;
      pMenu->m_yPos = m_fYPosRadioLinks[iVehicleRadioLink] + m_fHeightLinks[iVehicleRadioLink]*0.5 - 5.0*height_text;
      if ( pMenu->m_yPos < 0.03 )
         pMenu->m_yPos = 0.03;
      if ( pMenu->m_yPos > 0.8 )
         pMenu->m_yPos = 0.8;
      add_menu_to_stack(pMenu);
      return;  
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_PREFFERED_TX )
   {
      int iLink = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);
      onClickAutoTx(iLink);
      return;
   }

   if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SWITCH_RADIO_LINK )
   {
      int iLink = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);

      if ( (iLink < 0) || (iLink >= g_pCurrentModel->radioLinksParams.links_count) )
         return;

      g_bSwitchingRadioLink = true;
      warnings_add_switching_radio_link(iLink, g_pCurrentModel->radioLinksParams.link_frequency_khz[iLink]);
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_SWITCH_RADIO_LINK, iLink);
      return;
   }
}

float MenuRadioConfig::computeHeights()
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float height_text_large = g_pRenderEngine->textHeight(m_iIdFontLarge);
   float fPaddingInnerY = 0.02;

   float hTotalHeight = 0.0;

   if ( (0 < g_SM_RadioStats.countVehicleRadioLinks) || (0 < g_SM_RadioStats.countLocalRadioLinks) )
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
   {
      for( int iLink=0; iLink<m_iCountVehicleRadioLinks; iLink++ )
      {
          int countInterfaces = 0;
          for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
             if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iLink )
                countInterfaces++;
          if ( countInterfaces == 0 )
             countInterfaces = 1;

          m_fHeightLinks[iLink] = height_text_large;
          m_fHeightLinks[iLink] += 2.0*fPaddingInnerY + height_text*2.0;
          m_fHeightLinks[iLink] += countInterfaces * height_text*3.0 + (countInterfaces-1)*height_text*1.0;

          if ( countInterfaces > 1 )
             m_fHeightLinks[iLink] += height_text*2.0;

          hTotalHeight += m_fHeightLinks[iLink];
          if ( iLink != 0 )
             hTotalHeight += height_text*2.5;
      }
   }
   
   int iCountUnusedControllerInterfaces = 0;
   if ( (NULL == g_pCurrentModel) || (!g_bFirstModelPairingDone) || m_bShowOnlyControllerUnusedInterfaces )
      iCountUnusedControllerInterfaces = hardware_get_radio_interfaces_count();
   else
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId >= 0 )
            continue;
         iCountUnusedControllerInterfaces++;
      }
   }
   hTotalHeight += iCountUnusedControllerInterfaces * height_text*2.0;
   if ( iCountUnusedControllerInterfaces > 0 )
   {
      hTotalHeight += height_text*4.0;
      if ( iCountUnusedControllerInterfaces > 1 )
         hTotalHeight += (iCountUnusedControllerInterfaces-1)*height_text*1.5;
   }

   //log_line("Menu radio config: computed heights.");
   m_bComputedHeights = true;
   m_fTotalHeightRadioConfig = hTotalHeight;
   return hTotalHeight;
}

void MenuRadioConfig::computeMaxItemIndexAndCommands()
{
   m_iHeaderItemsCount = 0;
   m_iIndexMaxItem = 0;

   if ( m_bHas24PowerController )
   {
      setTooltip(m_iIndexMaxItem, "Sets the Tx power for controller's radio interfaces on 2.4 Ghz band.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SET_TX_POWER_CONTROLLER_24;
      m_iCountItemsTxPowerController++;
      m_iHeaderItemsCount++;
      m_iIndexMaxItem++;
   }

   if ( m_bHas58PowerController )
   {
      setTooltip(m_iIndexMaxItem, "Sets the Tx power for controller's radio interfaces on 5.8 Ghz band.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SET_TX_POWER_CONTROLLER_58;
      if ( ! m_bHas24PowerController )
      {
         setTooltip(m_iIndexMaxItem, "Sets the Tx power for controller's radio interfaces on 2.4 Ghz and 5.8 Ghz bands.");      
         m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SET_TX_POWER_CONTROLLER_2458;
      }
      m_iCountItemsTxPowerController++; 
      m_iHeaderItemsCount++;
      m_iIndexMaxItem++;
   }

   if ( m_bHasSiKPowerController )
   {
      setTooltip(m_iIndexMaxItem, "Sets the Tx power for controller's SiK radio interfaces.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SET_TX_POWER_CONTROLLER_SIK;
      m_iCountItemsTxPowerController++;
      m_iHeaderItemsCount++;
      m_iIndexMaxItem++;
   }

   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (! m_bShowOnlyControllerUnusedInterfaces) )
   if ( m_bHas24PowerVehicle )
   {
      setTooltip(m_iIndexMaxItem, "Sets the Tx power for vehicle's radio interfaces on 2.4 Ghz band.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SET_TX_POWER_VEHICLE_24;
      m_iCountItemsTxPowerVehicle++;
      m_iHeaderItemsCount++;
      m_iIndexMaxItem++;
   }

   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (! m_bShowOnlyControllerUnusedInterfaces) )
   if ( m_bHas58PowerVehicle )
   {
      setTooltip(m_iIndexMaxItem, "Sets the Tx power for vehicle's radio interfaces on 5.8 Ghz band.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SET_TX_POWER_VEHICLE_58;
      if ( ! m_bHas24PowerVehicle )
      {
         setTooltip(m_iIndexMaxItem, "Sets the Tx power for vehicle's radio interfaces on 2.4 Ghz and 5.8 Ghz bands.");      
         m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SET_TX_POWER_VEHICLE_2458;
      }
      m_iCountItemsTxPowerVehicle++;
      m_iHeaderItemsCount++;
      m_iIndexMaxItem++;
   }

   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (! m_bShowOnlyControllerUnusedInterfaces) )
   if ( m_bHasSiKPowerVehicle )
   {
      setTooltip(m_iIndexMaxItem, "Sets the Tx power for vehicle's SiK radio interfaces.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SET_TX_POWER_VEHICLE_SIK;
      m_iCountItemsTxPowerVehicle++;
      m_iHeaderItemsCount++;
      m_iIndexMaxItem++;
   }

   if ( (NULL == g_pCurrentModel) || (0 == g_SM_RadioStats.countVehicleRadioLinks) || m_bShowOnlyControllerUnusedInterfaces )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         setTooltip(m_iIndexMaxItem, "Configure this controller's radio interface.");
         m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER + (((u32)i) << 8);
         m_iIndexMaxItem++;
      }
      if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
         m_iIndexCurrentItem = m_iIndexMaxItem-1;

      return;
   }
   
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone )
   for( int iLink=0; iLink<m_iCountVehicleRadioLinks; iLink++ )
   {
       setTooltip(m_iIndexMaxItem, "Configure this radio link.");
       m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_CONFIGURE_RADIO_LINK + (((u32)iLink) << 8);
       m_iIndexMaxItem++;

       if ( m_bHasDiagnoseRadioLinkCommand[iLink] )
       {
          setTooltip(m_iIndexMaxItem, "Test this radio link.");
          m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_DIAGNOSE_RADIO_LINK + (((u32)iLink) << 8);
          m_iIndexMaxItem++;
       }
       int iCountInterfacesAssignedToThisLink = 0;
       for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
       {
          if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iLink )
          {
             setTooltip(m_iIndexMaxItem, "Configure this controller's radio interface.");
             m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER + (((u32)i) << 8);
             m_iIndexMaxItem++;
             iCountInterfacesAssignedToThisLink++;
          }
       }
       if ( (0 == iCountInterfacesAssignedToThisLink) && m_bHasSwitchRadioLinkCommand[iLink] )
       {
          setTooltip(m_iIndexMaxItem, "Switch to and connect to this vehicle radio link.");
          m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SWITCH_RADIO_LINK + (((u32)iLink) << 8);
          m_iIndexMaxItem++;
       }

       if ( iCountInterfacesAssignedToThisLink > 1 )
       {
          setTooltip(m_iIndexMaxItem, "Set the preffered Tx card (on the controller side) to be used for sending data to vehicle on this radio link.");
          m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SET_PREFFERED_TX + (((u32)iLink) << 8);
          m_iIndexMaxItem++;
       }

       int iVehicleRadioInterfaceForThisLink = -1;
       for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
       {
          if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == iLink )
          {
             iVehicleRadioInterfaceForThisLink = i;
             break;
          }
       }
       if ( -1 != iVehicleRadioInterfaceForThisLink )
       {
          setTooltip(m_iIndexMaxItem, "Configure this vehicle's radio interface.");
          m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_CONFIGURE_RADIO_INTERFACE_VEHICLE + (((u32)iVehicleRadioInterfaceForThisLink) << 8);
          m_iIndexMaxItem++;
       }
   }

   // Controller's unused interfaces

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId >= 0 )
         continue;
      setTooltip(m_iIndexMaxItem, "Configure this radio interface.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER + (((u32)i) << 8);
      m_iIndexMaxItem++;
   }

   if ( m_bHasSwapInterfacesCommand )
   {
      setTooltip(m_iIndexMaxItem, "Swap vehicle's radio interfaces assignment to radio links.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_SWAP_VEHICLE_RADIO_INTERFACES;
      m_iIndexMaxItem++;
   }

   if ( m_bHasRotateRadioLinksOrderCommand )
   {
      setTooltip(m_iIndexMaxItem, "Rotate vehicle's radio links order. This doesn't change any radio links parameters.");
      m_uCommandsIds[m_iIndexMaxItem] = MRC_ID_ROTATE_RADIO_LINKS;
      m_iIndexMaxItem++;
   }

   if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
      m_iIndexCurrentItem = m_iIndexMaxItem-1;

   //log_line("Menu radio config: computed max indexes.");
}


float MenuRadioConfig::drawRadioLinks(float xStart, float xEnd)
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float height_text_large = g_pRenderEngine->textHeight(m_iIdFontLarge);
   float hIconBig = height_text*3.0;
   float hIcon = height_text*2.0;
   float fMarginY = 0.04;
   float fMarginX = fMarginY/g_pRenderEngine->getAspectRatio();
   float fPaddingY = 0.042;
   float fPaddingX = fPaddingY/g_pRenderEngine->getAspectRatio();
   float fPaddingInnerY = 0.02;
   float fPaddingInnerX = fPaddingInnerY/g_pRenderEngine->getAspectRatio();
   float yStart = fMarginY + fPaddingY;
   float yEnd = 1.0 - fMarginY - fPaddingY;
   float fWidth = (xEnd - xStart);
   float fHeight = (yEnd-yStart);
   float fXMid = xStart + fWidth*0.5;
   
   char szBuff[128];
   double pColor[4];
   memcpy(pColor,get_Color_MenuBg(), 4*sizeof(double));
   pColor[3] = 0.94; 
   g_pRenderEngine->setColors(pColor);

   g_pRenderEngine->drawRoundRect(xStart, fMarginY, fWidth, 1.0-2.0*fMarginY, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
   
   if ( 1 )
   {
      static double bottomTooltipBgColor[4] = {250,30,50,0.1};
      g_pRenderEngine->setColors(bottomTooltipBgColor);
      float yTooltip = 1.0-fMarginY-m_fFooterHeight;
      g_pRenderEngine->drawRoundRect(xStart, yTooltip, fWidth, m_fFooterHeight, MENU_ROUND_MARGIN*m_sfMenuPaddingY);

      g_pRenderEngine->setColors(get_Color_MenuBg());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->drawLine(xStart, 1.0-fMarginY-m_fFooterHeight, xStart+fWidth, 1.0-fMarginY-m_fFooterHeight);

      yTooltip += 0.4*m_sfMenuPaddingY;

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->drawMessageLines( xStart+m_sfMenuPaddingX, yTooltip, m_szCurrentTooltip, MENU_TEXTLINE_SPACING, fWidth-2.0*fPaddingX, m_iIdFontRegular);
   }

   xStart += fPaddingX;
   xEnd -= fPaddingX;
   fWidth -= 2.0 * fPaddingX;

   m_RenderXPos = xStart;
   m_RenderWidth = fWidth;
   
   computeMaxItemIndexAndCommands();

   m_bTmpHasVideoStreamsEnabled = false;
   m_bTmpHasDataStreamsEnabled = false;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   float yPos = yStart;

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      yPos += height_text*4.0;
      g_pRenderEngine->drawText(xStart, yPos, m_iIdFontLarge, "No radio interfaces on controller!");
      return yPos;   
   }

   /*
   if ( NULL == g_pCurrentModel )
   {
      yPos += height_text*4.0;
      g_pRenderEngine->drawText(xStart, yPos, m_iIdFontLarge, "No active vehicle. Nothing to configure.");
      yPos += height_text*1.2;
      g_pRenderEngine->drawText(xStart, yPos, m_iIdFontLarge, "Select a vehicle from your vehicle list or search and connect to a vehicle.");
      return yPos;  
   }
   */
   /*
   if ( 0 == g_SM_RadioStats.countVehicleRadioLinks )
   {
      yPos += height_text*4.0;
      g_pRenderEngine->drawText(xStart, yPos, m_iIdFontLarge, "No radio configuration received yet from current active vehicle.");
      yPos += height_text*1.2;
      g_pRenderEngine->drawText(xStart, yPos, m_iIdFontLarge, "Nothing to configure.");
      return yPos;  
   }
   */

   double colorStrike[4] = {255,250,200,0.8};
   g_pRenderEngine->setFontBackgroundBoundingBoxStrikeColor(colorStrike);

   float yTmp = yPos;
   m_fHeaderHeight = drawRadioPowersHeader(xStart, xEnd, yPos);
   yPos += m_fHeaderHeight;

   float hTotalHeight = computeHeights();
   
   yPos += 0.24*((yEnd-yPos) - hTotalHeight);


   // ----------------------
   // Draw mid separator

   float fAlpha = g_pRenderEngine->setGlobalAlfa(0.5);
   for( float fy = yTmp+height_text_large*1.2 ; fy<yPos-0.04; fy += 0.02 )
      g_pRenderEngine->drawLine(fXMid, fy, fXMid, fy+0.01);
   g_pRenderEngine->setGlobalAlfa(fAlpha);


   // ---------------------------------
   // Draw each radio link

   for( int iLink = 0; iLink < m_iCountVehicleRadioLinks; iLink++ )
   {
      drawVehicleRadioLink(xStart, xEnd, yPos, iLink );
      yPos += m_fHeightLinks[iLink] + height_text*2.0;
   }

   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
   {
      if ( ! m_bTmpHasVideoStreamsEnabled )
      {
         g_pRenderEngine->setColors(get_Color_IconError());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
         g_pRenderEngine->drawText(xStart, yPos-height_text*0.3, m_iIdFontRegular, "Warning: No radio links are setup to transmit video streams.");
         yPos += height_text;
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      }
      if ( ! m_bTmpHasDataStreamsEnabled )
      {
         g_pRenderEngine->setColors(get_Color_IconError());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
         g_pRenderEngine->drawText(xStart, yPos-height_text*0.3, m_iIdFontRegular, "Warning: No radio links are setup to transmit data streams.");
         yPos += height_text;
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      }
   }

   // Show unused interfaces on the controller (disabled or not usable for current vehicle)

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   int iCountUnusedControllerInterfaces = 0;
   if ( (NULL == g_pCurrentModel) || (!g_bFirstModelPairingDone) || m_bShowOnlyControllerUnusedInterfaces )
      iCountUnusedControllerInterfaces = hardware_get_radio_interfaces_count();
   else
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId >= 0 )
            continue;
         iCountUnusedControllerInterfaces++;
      }
   }
   if ( 0 == iCountUnusedControllerInterfaces )
      return yPos;

   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (! m_bShowOnlyControllerUnusedInterfaces) )
   {
      g_pRenderEngine->drawText(xStart, yPos, m_iIdFontRegular, "Unused controller radio interfaces:");
      yPos += height_text;
      g_pRenderEngine->drawText(xStart, yPos, m_iIdFontRegular, "  (disabled or not usable for current vehicle radio configuration)");
      yPos += height_text*1.5;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( (NULL == g_pCurrentModel) || ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId < 0) || (!g_bFirstModelPairingDone) || m_bShowOnlyControllerUnusedInterfaces )
         yPos += drawRadioInterfaceController(fXMid, xEnd, yPos, -1, i);
   }

   g_pRenderEngine->clearFontBackgroundBoundingBoxStrikeColor();

   return yPos;
}

float MenuRadioConfig::drawRadioPowersHeader(float xStart, float xEnd, float yStart)
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float height_text_large = g_pRenderEngine->textHeight(m_iIdFontLarge);
   float hIconBig = height_text*3.0;
   float hIcon = height_text*2.0;
   float fWidth = xEnd - xStart;
   float yPos = yStart;
   float xMid = (xStart+xEnd)*0.5;
   float xMidMargin = 0.1/g_pRenderEngine->getAspectRatio();
   bool bBBox = false;

   char szBuff[128];

   ControllerSettings* pCS = get_ControllerSettings();

   float ftw = g_pRenderEngine->textWidth(m_iIdFontLarge, "Current Radio Configuration");
   g_pRenderEngine->drawText(xStart + (xEnd-xStart - ftw)*0.5, yPos-height_text_large*0.5, m_iIdFontLarge, "Current Radio Configuration");
   yPos += height_text_large*1.2;

   u32 uIdIconVehicle = g_idIconDrone;
   if ( NULL != g_pCurrentModel )
      uIdIconVehicle = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );
   //g_pRenderEngine->drawIcon(xStart + fWidth*0.2-hIconBig*0.5/g_pRenderEngine->getAspectRatio(), yPos, hIconBig/g_pRenderEngine->getAspectRatio(), hIconBig, g_idIconJoystick);
   //g_pRenderEngine->drawIcon(xStart + fWidth*0.8-hIconBig*0.5/g_pRenderEngine->getAspectRatio(), yPos, hIconBig/g_pRenderEngine->getAspectRatio(), hIconBig, uIdIconVehicle);
   
   float fScale = 1.7;
   g_pRenderEngine->drawIcon(xMid - xMidMargin - hIconBig*fScale/g_pRenderEngine->getAspectRatio(), yPos-hIconBig*0.3, hIconBig*fScale/g_pRenderEngine->getAspectRatio(), hIconBig*fScale, g_idIconController);
   if ( (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (!m_bShowOnlyControllerUnusedInterfaces) )
      g_pRenderEngine->drawIcon(xMid + xMidMargin, yPos, hIconBig/g_pRenderEngine->getAspectRatio(), hIconBig, uIdIconVehicle);
   
   float xLeft = xMid - xMidMargin - hIconBig*fScale/g_pRenderEngine->getAspectRatio() - 0.02/g_pRenderEngine->getAspectRatio();
   float xRight = xMid + xMidMargin + hIconBig/g_pRenderEngine->getAspectRatio() + 0.02/g_pRenderEngine->getAspectRatio();

   g_pRenderEngine->drawTextLeft(xLeft, yPos + (hIconBig-height_text)*0.5, m_iIdFontRegular, "Controller");

   if ( (NULL == g_pCurrentModel) || ( ! g_bFirstModelPairingDone) || m_bShowOnlyControllerUnusedInterfaces )
      strcpy(szBuff, "No Vehicle Active");
   else
      strcpy(szBuff, g_pCurrentModel->getLongName());

   szBuff[0] = toupper(szBuff[0]);
   g_pRenderEngine->drawText(xRight, yPos + (hIconBig-height_text)*0.5, m_iIdFontRegular, szBuff);
   
   yPos += height_text*1.5;

   int iTotalLines = 0;
   float dy = 0.0;

   if ( m_iIndexCurrentItem < 0 )
      m_iIndexCurrentItem = 0;

   m_fXPosSiKTxPowerController = xMid - xMidMargin*1.2 - 0.03;
   if ( m_bHas24PowerController && m_bHas58PowerController )
   {
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_24 )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      sprintf(szBuff,"Tx Power 2.4Ghz: %d", pCS->iTXPowerAtheros);
      g_pRenderEngine->drawTextLeft(xMid - xMidMargin*1.2, yPos + hIconBig, m_iIdFontRegular, szBuff);
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_24 )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_58 )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      sprintf(szBuff,"Tx Power 5.8Ghz: %d", pCS->iTXPowerRTL);
      g_pRenderEngine->drawTextLeft(xMid - xMidMargin*1.2, yPos + hIconBig + height_text*1.4, m_iIdFontRegular, szBuff);
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_58 )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      iTotalLines += 2;
      dy += 2.0*height_text*1.4;
   }
   else
   {
      if ( ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_24) ||
           ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_58) ||
           ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_2458) )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      if ( m_bHas58PowerController )
         sprintf(szBuff,"Tx Power (2.4/5.8 Ghz): %d", pCS->iTXPowerRTL);
      else
         sprintf(szBuff,"Tx Power (2.4/5.8 Ghz): %d", pCS->iTXPowerAtheros);
      g_pRenderEngine->drawTextLeft(xMid - xMidMargin*1.2, yPos + hIconBig, m_iIdFontRegular, szBuff);

      if ( ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_24) ||
           ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_58) ||
           ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_2458) )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      iTotalLines++;
      dy += height_text*1.4;
   }

   if ( m_bHasSiKPowerController )
   {
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_SIK )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      sprintf(szBuff,"Tx Power SiK (%s): %d", str_getBandName(m_uBandsSiKController), pCS->iTXPowerSiK);
      g_pRenderEngine->drawTextLeft(xMid - xMidMargin*1.2, yPos + hIconBig + dy, m_iIdFontRegular, szBuff);
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_CONTROLLER_SIK )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      iTotalLines++;
      dy += height_text*1.4;
   }

   if ( (NULL == g_pCurrentModel) || (! g_bFirstModelPairingDone) || m_bShowOnlyControllerUnusedInterfaces )
   {
      yPos += height_text*1.1;
      g_pRenderEngine->drawMessageLines("You have no active model. No radio links are created.", xMid + xMidMargin*0.6, yPos, MENU_TEXTLINE_SPACING, (xEnd - xMid) - m_sfMenuPaddingX - xMidMargin*0.6, m_iIdFontRegular);
      yPos += iTotalLines*height_text*1.2;
      return yPos - yStart;
   }
   
   int iLines = 0;
   dy = 0.0;

   if ( m_bHas24PowerVehicle && m_bHas58PowerVehicle )
   {
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_24 )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      sprintf(szBuff,"Tx Power 2.4Ghz: %d", g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
      g_pRenderEngine->drawText(xMid + xMidMargin*1.2, yPos + hIconBig, m_iIdFontRegular, szBuff);
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_24 )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_58 )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      sprintf(szBuff,"Tx Power 5.8Ghz: %d", g_pCurrentModel->radioInterfacesParams.txPowerRTL);
      g_pRenderEngine->drawText(xMid + xMidMargin*1.2, yPos +hIconBig + height_text*1.4, m_iIdFontRegular, szBuff);
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_58 )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      iLines += 2;
      dy += 2.0*height_text*1.4;
   }
   else
   {
      if ( ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_24) ||
           ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_58) ||
           ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_2458) )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      if ( m_bHas58PowerVehicle )
         sprintf(szBuff,"Tx Power (2.4/5.8 Ghz): %d", g_pCurrentModel->radioInterfacesParams.txPowerRTL);
      else
         sprintf(szBuff,"Tx Power (2.4/5.8 Ghz): %d", g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
      g_pRenderEngine->drawText(xMid + xMidMargin*1.2, yPos + hIconBig, m_iIdFontRegular, szBuff);

      if ( ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_24) ||
           ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_58) ||
           ((m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_2458) )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      iLines++;
      dy += height_text*1.4;
   }

   if ( m_bHasSiKPowerVehicle )
   {
      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_SIK )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      sprintf(szBuff,"Tx Power SiK (%s): %d", str_getBandName(m_uBandsSiKVehicle), g_pCurrentModel->radioInterfacesParams.txPowerSiK);
      g_pRenderEngine->drawText(xMid + xMidMargin*1.2, yPos + hIconBig + dy, m_iIdFontRegular, szBuff);

      m_fXPosSiKTxPowerVehicle = xMid + xMidMargin*1.2 - 0.03 + g_pRenderEngine->textWidth(m_iIdFontRegular, szBuff);

      if ( (m_uCommandsIds[m_iIndexCurrentItem] & 0xFF) == MRC_ID_SET_TX_POWER_VEHICLE_SIK )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      iTotalLines++;
      dy += height_text*1.4;
   }

   if ( iLines > iTotalLines )
      iTotalLines = iLines;

   yPos += hIconBig + iTotalLines*height_text*1.4;
   
   return yPos - yStart;
}


void MenuRadioConfig::drawVehicleRadioLinkCapabilities(float xStart, float xEnd, float yStart, int iVehicleRadioLink, bool bIsLinkActive, bool bIsRelayLink)
{
   char szDescription[128];
   char szDescriptionSmall[128];
   char szCapabilities[128];
   char szDRVideo[128];
   char szDRDataU[128];
   char szDRDataD[128];
   char szAuto[128];

   int iCountInterfacesAssignableToThisLink = controller_count_asignable_radio_interfaces_to_vehicle_radio_link(g_pCurrentModel, iVehicleRadioLink);
   int iCountInterfacesAssignedToThisLink = 0;
   int iRadioInterfaceIndex = -1;

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == iVehicleRadioLink )
         iRadioInterfaceIndex = i;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iVehicleRadioLink )
         iCountInterfacesAssignedToThisLink++;
   }

   szDescription[0] = 0;
   szDescriptionSmall[0] = 0;
   szCapabilities[0] = 0;

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      strcpy(szCapabilities, "RELAY LINK");
   else
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         strcpy(szCapabilities, "Downlink Only");
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         strcpy(szCapabilities, "Uplink Only");
   }

   if ( 0 < strlen(szCapabilities) )
      strcat(szCapabilities, ",  ");

   char szTmpRadioLink[256];
   strcpy(szTmpRadioLink, szCapabilities);
   bool bDataOnly = false;
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      strcat(szCapabilities, "Video Only,");
      
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
   {
      bDataOnly = true;
      strcat(szCapabilities, "Data Only,");
   }

   str_getDataRateDescription(g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iVehicleRadioLink], szDRVideo);
   str_getDataRateDescription(g_pCurrentModel->radioLinksParams.link_datarate_data_bps[iVehicleRadioLink], szDRDataD);
   str_getDataRateDescription(g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iVehicleRadioLink], szDRDataU);

   if ( g_pCurrentModel->radioLinksParams.uDownlinkDataDataRateType[iVehicleRadioLink] == FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO )
      str_getDataRateDescription(g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iVehicleRadioLink], szDRDataU);
   if ( g_pCurrentModel->radioLinksParams.uUplinkDataDataRateType[iVehicleRadioLink] == FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO )
      str_getDataRateDescription(g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iVehicleRadioLink], szDRDataU);
   if ( g_pCurrentModel->radioLinksParams.uDownlinkDataDataRateType[iVehicleRadioLink] == FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST )
      str_getDataRateDescription(DEFAULT_RADIO_DATARATE_LOWEST, szDRDataD);
   if ( g_pCurrentModel->radioLinksParams.uUplinkDataDataRateType[iVehicleRadioLink] == FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST )
      str_getDataRateDescription(DEFAULT_RADIO_DATARATE_LOWEST, szDRDataU);
   
   szAuto[0] = 0;
   if ( (NULL != g_pCurrentModel) && ( ! g_pCurrentModel->radioLinkIsSiKRadio(iVehicleRadioLink) ) )
   {
      int adaptive = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
      if ( adaptive )
         strcpy(szAuto, " (Auto)");
      else
         strcpy(szAuto, " (Fixed)");
   } 

   bool bShowLinkRed = false;

   if ( 0 == iCountInterfacesAssignedToThisLink )
   {
      if ( !bIsLinkActive )
      {
         strcpy(szDescription, "This radio link is disabled.");
         bShowLinkRed = true;
      }
      else if ( bIsRelayLink )
         strcpy(szDescription, "This is a relay radio link.");
      else if ( 0 < iCountInterfacesAssignableToThisLink )
      {
         strcpy(szDescription, "Not connected. ");
         strcpy(szDescriptionSmall, "Ready to connect to this link.");
      }
      else
      {
         strcpy(szDescription, "Not connected. ");
         strcpy(szDescriptionSmall, "No controller interfaces can connect to this link.");
      }
   }
   else
   {
      if ( g_pCurrentModel->radioLinkIsSiKRadio(iVehicleRadioLink) && (-1 != iRadioInterfaceIndex) )
      {
         char szTmp[64];
         str_format_bitrate(hardware_radio_sik_get_air_baudrate_in_bytes(iRadioInterfaceIndex)*8, szTmp);

         if ( 0 != szCapabilities[0] )
            sprintf(szDescription, "%s Data Rate: %s", szCapabilities, szTmp);
         else
            sprintf(szDescription, "Data Rate: %s", szTmp);
      }
      else if ( 0 != szCapabilities[0] )
         sprintf(szDescription, "%s Data Rates: Video: %s%s, Data: %s, Uplink: %s", szCapabilities, szDRVideo, szAuto, szDRDataD, szDRDataU);
      else
         sprintf(szDescription, "Data Rates: Video: %s%s, Data: %s, Uplink: %s", szDRVideo, szAuto, szDRDataD, szDRDataU);
   }

   if ( bShowLinkRed )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }

   float fwt = g_pRenderEngine->textWidth(m_iIdFontRegular, szDescription);

   g_pRenderEngine->drawText(xStart, yStart, m_iIdFontRegular, szDescription);
   if ( 0 != szDescriptionSmall[0] )
      g_pRenderEngine->drawText(xStart+fwt, yStart + 0.8*(g_pRenderEngine->textHeight(m_iIdFontRegular) - g_pRenderEngine->textHeight(m_iIdFontSmall)), m_iIdFontSmall, szDescriptionSmall);

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   if ( bDataOnly )
   {
      double cY[4] = {255,240,100,1.0};

      g_pRenderEngine->setColors(cY);
      g_pRenderEngine->drawText(xStart, yStart, m_iIdFontRegular, "Data Only");

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }
}

float MenuRadioConfig::drawVehicleRadioLink(float xStart, float xEnd, float yStart, int iVehicleRadioLink)
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float height_text_large = g_pRenderEngine->textHeight(m_iIdFontLarge);
   float hIcon = height_text*2.4;
   float fPaddingInnerY = 0.02;
   float fPaddingInnerX = fPaddingInnerY/g_pRenderEngine->getAspectRatio();
   
   bool bBBox = false;
   bool bRadioLinkHasUplink = false;
   bool bRadioLinkHasDownlink = false;
   bool bShowLinkRed = false;
   bool bShowRed = false;
   bool bHasAutoTxOption = false;

   double cY[4] = {255,240,100,1.0};
   
   char szTmp[64];
   char szBuff[128];
   char szError[128];
   
   float xLeftMax[MAX_RADIO_INTERFACES];
   float xRightMin = 1.0;
   float yMidVehicle = 0.0;
   float yMidController[MAX_RADIO_INTERFACES];
   float fWidth = xEnd - xStart;
   float xMid = (xStart + xEnd)*0.5;
   float xMidMargin = 0.07/g_pRenderEngine->getAspectRatio();
   float yPos = yStart;

   szBuff[0] = 0;
   szTmp[0] = 0;
   szError[0] = 0;

   m_fYPosRadioLinks[iVehicleRadioLink] = yPos;

   int iVehicleRadioInterfaceForThisLink = -1;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == iVehicleRadioLink )
      {
         iVehicleRadioInterfaceForThisLink = i;
         break;
      }
   }

   int iCountInterfacesAssignedToThisLink = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iVehicleRadioLink )
         iCountInterfacesAssignedToThisLink++;
   }

   int iCountInterfacesAssignableToThisLink = controller_count_asignable_radio_interfaces_to_vehicle_radio_link(g_pCurrentModel, iVehicleRadioLink);

   int iCurrentMenuItemCommand = (int)(m_uCommandsIds[m_iIndexCurrentItem] & 0xFF);
   int iCurrentMenuItemExtraParam = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);

   if ( iCountInterfacesAssignedToThisLink > 1 )
      bHasAutoTxOption = true;

   bool bIsLinkActive = true;
   bool bIsRelayLink = false;
   bool bDataOnly = false;
   
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      strcpy(szError, "Disabled");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }
   if ( -1 == iVehicleRadioInterfaceForThisLink )
   {
      strcpy(szError, "No radio interface");
      bShowLinkRed = true;
      bIsLinkActive = false;    
   }
   else if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceForThisLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      strcpy(szError, "Disabled");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA ) )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO ) )
   {
      strcpy(szError, "No data streams enabled on this radio link.");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }
   
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX ) )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX ) )
   {
      strcpy(szError, "Both Uplink and Downlink capabilities are disabled.");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      bIsRelayLink = true;
   }
   else
   {
      if ( ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX ) &&
           ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX ) )
      {
         bRadioLinkHasUplink = true;
         bRadioLinkHasDownlink = true;
      }
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      {
         bRadioLinkHasDownlink = true;
      }
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      {
         bRadioLinkHasUplink = true;
      }
   }

   if ( ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO ) &&
        ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA ) )
   {
      if ( bIsLinkActive )
      {
         m_bTmpHasVideoStreamsEnabled = true;
         m_bTmpHasDataStreamsEnabled = true;
      }
   }
   else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
   {
      if ( bIsLinkActive )
         m_bTmpHasVideoStreamsEnabled = true;
   }
   else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   {
      bDataOnly = true;
      if ( bIsLinkActive )
         m_bTmpHasDataStreamsEnabled = true;
   }
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   if ( bIsRelayLink )
       sprintf(szBuff, "RELAY Link-%d  %s", iVehicleRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLink]));
   else if ( bIsLinkActive )
       sprintf(szBuff, "Radio Link-%d  %s", iVehicleRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLink]));
   else
      sprintf(szBuff, "Radio Link-%d  %s", iVehicleRadioLink+1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLink]));
      

   // -------------------------------------------------------
   // Begin - Draw link name/title

   float fxTitleEnd = 0.0;

   if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
      bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);

   if ( bShowLinkRed )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }

   if ( iCountInterfacesAssignedToThisLink == 0 )
   {
      g_pRenderEngine->drawText(xMid + 0.01, yPos-height_text*0.3, m_iIdFontLarge, szBuff);
      fxTitleEnd = xMid + 0.01 + height_text + g_pRenderEngine->textWidth(m_iIdFontLarge, szBuff);
   }
   else
   {
      g_pRenderEngine->drawText(xStart + 0.01, yPos-height_text*0.3, m_iIdFontLarge, szBuff);
      fxTitleEnd = xStart + 0.01 + height_text + g_pRenderEngine->textWidth(m_iIdFontLarge, szBuff);
   }

   if ( bShowLinkRed && bIsLinkActive )
   {
      float ftw = g_pRenderEngine->textWidth(m_iIdFontLarge, szBuff);
      if ( iCountInterfacesAssignedToThisLink == 0 )
         g_pRenderEngine->drawText(xMid + ftw + 0.02, yPos-height_text*0.2, m_iIdFontRegular, szError);
      else
         g_pRenderEngine->drawText(xStart + ftw + 0.02, yPos-height_text*0.2, m_iIdFontRegular, szError);

      fxTitleEnd += 0.01 + ftw;
   }

   if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

   if ( m_bHasDiagnoseRadioLinkCommand[iVehicleRadioLink] )
   {
      if ( (iCurrentMenuItemCommand == MRC_ID_DIAGNOSE_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);

      g_pRenderEngine->drawText(fxTitleEnd, yPos-height_text*0.2, m_iIdFontRegular, "Test");
      
      if ( (iCurrentMenuItemCommand == MRC_ID_DIAGNOSE_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

   }

   // End - Draw link name/title
   // -------------------------------------------------------

   yPos += height_text_large;

   // ------------------------------------------------------
   // Begin - Draw link container rectangle

   double pColor[4];
   memcpy(pColor,get_Color_MenuText(), 4*sizeof(double));
   pColor[3] = 0.7; 
   g_pRenderEngine->setColors(pColor);
   g_pRenderEngine->setStrokeSize(2.0);
   g_pRenderEngine->setFill(0,0,0, 0.2);

   if ( bShowLinkRed )
   {
      g_pRenderEngine->setStroke(get_Color_IconError(),1.0);
      g_pRenderEngine->setStrokeSize(2.0);
   }

   if ( 0 == iCountInterfacesAssignedToThisLink )
      g_pRenderEngine->drawRoundRect(xMid, yPos, fWidth - (xMid - xStart), m_fHeightLinks[iVehicleRadioLink]-height_text_large, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
   else
      g_pRenderEngine->drawRoundRect(xStart, yPos, fWidth, m_fHeightLinks[iVehicleRadioLink]-height_text_large, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
   
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   // End - Draw link container rectangle
   // ------------------------------------------------------

   yPos += fPaddingInnerY * 0.4;

   if ( 0 == iCountInterfacesAssignedToThisLink )
      drawVehicleRadioLinkCapabilities(xMid + fPaddingInnerX, xEnd - fPaddingInnerX, yPos, iVehicleRadioLink, bIsLinkActive, bIsRelayLink);
   else
      drawVehicleRadioLinkCapabilities(xStart + fPaddingInnerX, xMid - fPaddingInnerX, yPos, iVehicleRadioLink, bIsLinkActive, bIsRelayLink);

   yPos += height_text*2.0;
   yPos += fPaddingInnerY * 0.4;
   
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   //--------------------------------------------------
   // Draw vehicle interfaces

   float y1 = yPos;

   if ( bHasAutoTxOption )
      yPos += (m_fHeightLinks[iVehicleRadioLink]-height_text_large - 2.0*fPaddingInnerY-4.0*height_text) * 0.5 - height_text*1.5;
   else
      yPos += (m_fHeightLinks[iVehicleRadioLink]-height_text_large - 2.0*fPaddingInnerY-2.0*height_text) * 0.5 - height_text*1.5;

   int iVehicleRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(iVehicleRadioLink);
      
   float yStartVehicle = yPos;
   yMidVehicle = yPos + height_text*1.5;
   szTmp[0] = 0;
   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      strcat(szTmp, " Disabled");
      bShowRed = true;
   }
   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
   {
      strcat(szTmp, " Can't Uplink or Downlink");
      bShowRed = true;
   }
   
   g_pRenderEngine->drawIcon(xMid + xMidMargin, yPos+height_text*0.5, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, g_idIconRadio);

   float xiRight = xMid + xMidMargin + hIcon/g_pRenderEngine->getAspectRatio() + fPaddingInnerX;
   sprintf(szBuff, "Interface %d, Port %s", iVehicleRadioInterfaceId+1, g_pCurrentModel->radioInterfacesParams.interface_szPort[iVehicleRadioInterfaceId]);
   
   if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_INTERFACE_VEHICLE) && (iCurrentMenuItemExtraParam == iVehicleRadioInterfaceId) )
      bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
    
   g_pRenderEngine->drawText(xiRight, yPos, m_iIdFontRegular, szBuff);

   if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_INTERFACE_VEHICLE) && (iCurrentMenuItemExtraParam == iVehicleRadioInterfaceId) )
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

   yPos += height_text*1.3;

   sprintf(szBuff, "Type: %s", str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[iVehicleRadioInterfaceId]));
   g_pRenderEngine->drawText(xiRight, yPos, m_iIdFontRegular, szBuff);
   yPos += height_text;

   szBuff[0] = 0;

   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA ))
   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO ))
   {
      bShowRed = true;
      strcat(szTmp, " No streams enabled");
   }

   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX ) )
      strcat(szBuff, "Downlink Only");
   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
   if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX ) )
      strcat(szBuff, "Uplink Only");
   else
   {
      bShowRed = true;
      strcat(szTmp, " Disabled");
   }
   
   g_pRenderEngine->drawText(xiRight, yPos,  m_iIdFontRegular, szBuff);
   yPos += height_text;

   //g_pRenderEngine->drawLine(xiRight - fPaddingInnerX, yStartVehicle + height_text*0.5, xiRight- fPaddingInnerX, yPos - height_text*0.5);

   //------------------------------------------------
   // Draw controller interfaces
   
   float yTopInterfaces = y1;
   yPos = y1;
   iCountInterfacesAssignedToThisLink = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      xLeftMax[i] = 0.0;
      yMidController[i] = 0.0;
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iVehicleRadioLink )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      iCountInterfacesAssignedToThisLink++;

      if ( iCountInterfacesAssignedToThisLink != 1 )
         yPos += height_text*1.0;

      m_fYPosCtrlRadioInterfaces[i] = yPos;
      y1 = yPos;
      yMidController[i] = yPos + height_text*1.5;

      bool bSelected = false;
      if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER) && (iCurrentMenuItemExtraParam == i) )
         bSelected = true;

      xLeftMax[i] = xMid - xMidMargin - drawRadioInterfaceCtrlInfo(xMid - xMidMargin, 1.0, yPos, iVehicleRadioLink, i, bSelected);
      yPos += height_text*1.2;
      yPos += height_text*1.1;
      yPos += height_text;

   }

   if ( 0 == iCountInterfacesAssignedToThisLink )
   {
      yPos -= height_text*0.5;

      if ( bIsRelayLink )
      {
         strcpy(szBuff, "This is a Relay link between vehicles.");
         yPos += g_pRenderEngine->drawMessageLines(xStart + 0.02 + fPaddingInnerX, yPos, szBuff, MENU_TEXTLINE_SPACING, (xMid-xMidMargin-xStart - 0.02 - fPaddingInnerX), m_iIdFontRegular);
      }
      else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         strcpy(szBuff, "This vehicle radio link is disabled.");
         yPos += g_pRenderEngine->drawMessageLines(xStart + 0.02 + fPaddingInnerX, yPos, szBuff, MENU_TEXTLINE_SPACING, (xMid-xMidMargin-xStart - 0.02 - fPaddingInnerX), m_iIdFontRegular);
      }
      else if ( -1 == iVehicleRadioInterfaceForThisLink )
      {
         strcpy(szBuff, "There is no radio interface on vehicle assigned to this radio link.");
         yPos += g_pRenderEngine->drawMessageLines(xStart + 0.02 + fPaddingInnerX, yPos, szBuff, MENU_TEXTLINE_SPACING, (xMid-xMidMargin-xStart - 0.02 - fPaddingInnerX), m_iIdFontRegular);
      }
      else if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceForThisLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         strcpy(szBuff, "The vehicle radio interface assigned to this radio link is disabled.");
         yPos += g_pRenderEngine->drawMessageLines(xStart + 0.02 + fPaddingInnerX, yPos, szBuff, MENU_TEXTLINE_SPACING, (xMid-xMidMargin-xStart - 0.02 - fPaddingInnerX), m_iIdFontRegular);
      }
      else
      {
         if ( 1 == g_SM_RadioStats.countLocalRadioInterfaces )
            strcpy(szBuff, "No more controller interfaces available for this vehicle radio link.");
         else
         {
            strcpy(szBuff, "No controller interfaces that can handle this vehicle radio link.");
            g_pRenderEngine->setColors(get_Color_IconError());
            g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
         }
         if ( m_bHasSwitchRadioLinkCommand[iVehicleRadioLink] )
            yPos -= height_text * 0.4;
         yPos += g_pRenderEngine->drawMessageLines(xStart + 0.02 + fPaddingInnerX, yPos, szBuff, MENU_TEXTLINE_SPACING, (xMid-xMidMargin-xStart - 0.02 - fPaddingInnerX), m_iIdFontRegular); 
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      }
   }

   if ( (0 == iCountInterfacesAssignedToThisLink) && (iCountInterfacesAssignableToThisLink > 0) )
   {
      yPos += height_text*0.8;

      if ( (iCurrentMenuItemCommand == MRC_ID_SWITCH_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
          bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
     
       g_pRenderEngine->drawText(xStart + 0.02 + fPaddingInnerX, yPos, m_iIdFontLarge, "Switch to this radio link");
       
       if ( (iCurrentMenuItemCommand == MRC_ID_SWITCH_RADIO_LINK) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
          g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);
       
       yPos += 1.0*height_text_large;
   }

   //--------------------------------------------------
   // Draw auto tx card

   if ( iCountInterfacesAssignedToThisLink > 1 )
   {
      yPos += 0.5 * height_text;
      m_fYPosAutoTx[iVehicleRadioLink] = yPos;

      sprintf(szBuff, "Preferred Tx Card: Auto");
      int iMinPriority = 10000;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iVehicleRadioLink )
            continue;

         radio_hw_info_t* pNICInfo2 = hardware_get_radio_info(i);
         if ( NULL == pNICInfo2 )
            continue;

         if ( controllerIsCardRXOnly(pNICInfo2->szMAC) )
            continue;

         int iCardPriority = controllerIsCardTXPreferred(pNICInfo2->szMAC);
         if ( iCardPriority <= 0 || iCardPriority > iMinPriority )
            continue;
         iMinPriority = iCardPriority;
         sprintf(szBuff, "Preferred Tx Card: Interface %d, Port %s", i+1, pNICInfo2->szUSBPort);
         if ( ! controllerIsCardInternal(pNICInfo2->szMAC) )
            strcat(szBuff, " (Ext)");
      }

      if ( (iCurrentMenuItemCommand == MRC_ID_SET_PREFFERED_TX) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
    
      g_pRenderEngine->drawText(xStart + 0.02 + fPaddingInnerX, yPos, m_iIdFontRegular, szBuff);
      
      if ( (iCurrentMenuItemCommand == MRC_ID_SET_PREFFERED_TX) && (iCurrentMenuItemExtraParam == iVehicleRadioLink) )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);
      
      yPos += 1.5*height_text;
   }

   //---------------------------------------
   // Draw mid separator

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   float fAlpha = g_pRenderEngine->setGlobalAlfa(0.2);
   for( float y = yTopInterfaces; y<yPos-0.01; y += 0.02 )
      g_pRenderEngine->drawLine(xMid, y, xMid, y+0.01);
   g_pRenderEngine->setGlobalAlfa(fAlpha);
   
   //--------------------------------------------------
   // Draw arrows

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      
   float xLeftMaxMax = 0.0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      if ( xLeftMax[i] > xLeftMaxMax )
         xLeftMaxMax = xLeftMax[i];

   float dyArrow = 0.0;
   float arrowSpacing = 0.0;
   if ( iCountInterfacesAssignedToThisLink > 1 )
   {
      arrowSpacing = height_text*1.6/(iCountInterfacesAssignedToThisLink-1);
      dyArrow = - height_text*0.8;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iVehicleRadioLink )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      int iVehicleRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(iVehicleRadioLink);
   
      bShowRed = false;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         bShowRed = true;
      if ( !(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      if ( !(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         bShowRed = true;

      bool bCanUplink = false;
      bool bCanDownlink = false;
      bool bCanExchangeData = false;
      bool bCanExchangeVideo = false;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         bCanUplink = true;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         bCanDownlink = true;   

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
         bCanExchangeData = true;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iVehicleRadioInterfaceId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         bCanExchangeVideo = true;

      if ( (! bCanExchangeData) && (! bCanExchangeVideo) )
      {
         bShowRed = true;
         bCanUplink = false;
         bCanDownlink = false;
      }
      //if ( bShowRed )
      //   continue;

      if ( bShowRed )
      {
         g_pRenderEngine->setColors(get_Color_IconError());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      }
      else
      {
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      }

      //g_pRenderEngine->drawLine(xLeftMaxMax + fPaddingInnerX, yMidController[i] - height_text, xLeftMaxMax + fPaddingInnerX, yMidController[i] + height_text);

      float xLineStart = xMid - xMidMargin + fPaddingInnerX*0.2;
      float xLineEnd = xMid + xMidMargin - fPaddingInnerX*0.2;
      float yLineStart = yMidController[i];
      float yLineEnd = yMidVehicle + dyArrow;

      //if ( bCanExchangeData && bCanExchangeVideo )
      //{
      //   g_pRenderEngine->drawLine(xLineStart + 6.0*g_pRenderEngine->getPixelWidth(), yLineStart - 1.0*g_pRenderEngine->getPixelHeight(), xLineEnd - 6.0*g_pRenderEngine->getPixelWidth(), yLineEnd - 1.0*g_pRenderEngine->getPixelHeight() );
      //   g_pRenderEngine->drawLine(xLineStart + 6.0*g_pRenderEngine->getPixelWidth(), yLineStart + 1.0*g_pRenderEngine->getPixelHeight(), xLineEnd - 6.0*g_pRenderEngine->getPixelWidth(), yLineEnd + 1.0*g_pRenderEngine->getPixelHeight());
      //}
      //else

      if ( ! bShowRed )
      if ( bCanExchangeData != bCanExchangeVideo )
      {
         g_pRenderEngine->setColors(cY);
         g_pRenderEngine->setStrokeSize(1.0);
      }
      
      g_pRenderEngine->drawLine(xLineStart, yLineStart, xLineEnd, yLineEnd );
 
      if ( bCanUplink )
      {
         float dx = xLineEnd - xLineStart;
         float dy = yLineEnd - yLineStart;
         float fLength = sqrtf(dx*dx+dy*dy);
         float x1 = xLineEnd - dx*0.01/fLength;
         float y1 = yLineEnd - dy*0.01/fLength;
         float xp[3], yp[3];
         xp[0] = xLineEnd;
         yp[0] = yLineEnd;
         osd_rotate_point(x1, y1, xLineEnd, yLineEnd, 26, &xp[1], &yp[1]);
         osd_rotate_point(x1, y1, xLineEnd, yLineEnd, -26, &xp[2], &yp[2]);
         g_pRenderEngine->drawPolyLine(xp,yp,3);
         g_pRenderEngine->fillPolygon(xp,yp,3);
      }
      if ( bCanDownlink )
      {
         float dx = xLineEnd - xLineStart;
         float dy = yLineEnd - yLineStart;
         float fLength = sqrtf(dx*dx+dy*dy);         
         float x1 = xLineStart + dx*0.01/fLength;
         float y1 = yLineStart + dy*0.01/fLength;
         float xp[3], yp[3];
         xp[0] = xLineStart;
         yp[0] = yLineStart;
         osd_rotate_point(x1, y1, xLineStart, yLineStart, 26, &xp[1], &yp[1]);
         osd_rotate_point(x1, y1, xLineStart, yLineStart, -26, &xp[2], &yp[2]);
         g_pRenderEngine->drawPolyLine(xp,yp,3);
         g_pRenderEngine->fillPolygon(xp,yp,3);
      }

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
     
      dyArrow += arrowSpacing;
   }

   return yPos - yStart;
}


float MenuRadioConfig::drawRadioInterfaceController(float xStart, float xEnd, float yStart, int iRadioLink, int iRadioInterface)
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float fPaddingInnerY = 0.02;
   float fPaddingInnerX = fPaddingInnerY/g_pRenderEngine->getAspectRatio();

   char szBuff[128];
   char szError[128];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   int iCurrentMenuItemCommand = (int)(m_uCommandsIds[m_iIndexCurrentItem] & 0xFF);
   int iCurrentMenuItemExtraParam = (int)(m_uCommandsIds[m_iIndexCurrentItem] >> 8);

   bool bSelected = false;
   
   if ( (iCurrentMenuItemCommand == MRC_ID_CONFIGURE_RADIO_INTERFACE_CONTROLLER) && (iCurrentMenuItemExtraParam == iRadioInterface) )
      bSelected = true;

   if ( (!m_bShowOnlyControllerUnusedInterfaces) && (NULL != g_pCurrentModel) && g_bFirstModelPairingDone && (g_SM_RadioStats.radio_interfaces[iRadioInterface].assignedLocalRadioLinkId >= 0) )
      return 0.0;

   drawRadioInterfaceCtrlInfo(xStart - 0.03/g_pRenderEngine->getAspectRatio(), 1.0, yStart, iRadioLink, iRadioInterface, bSelected);
   float yPos = yStart;
   yPos += height_text*1.2;
   yPos += height_text*1.1;
   yPos += height_text;

   yPos += height_text*1.5;
   return yPos - yStart;
}

float MenuRadioConfig::drawRadioInterfaceCtrlInfo(float xStart, float xEnd, float yStart, int iRadioLink, int iRadioInterface, bool bSelected)
{
   float height_text = g_pRenderEngine->textHeight(m_iIdFontRegular);
   float hIcon = height_text*2.0;
   float fPaddingInnerY = 0.02;
   float fPaddingInnerX = fPaddingInnerY/g_pRenderEngine->getAspectRatio();
   
   float yPos = yStart;
   float fMaxWidth = 0.0;

   char szBuff[128];
   char szError[128];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   bool bShowRed = false;
   bool bBBox = false;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterface);
   if ( NULL == pRadioHWInfo )
   {
      sprintf(szBuff, "Can't get info about radio interface %d.", iRadioInterface+1);
      g_pRenderEngine->drawTextLeft(xStart, yPos, m_iIdFontRegular, szBuff);
      return g_pRenderEngine->textWidth(m_iIdFontRegular, szBuff);
   }

   t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
   if ( NULL == pCardInfo )
   {
      sprintf(szBuff, "Can't get info about radio interface %d.", iRadioInterface+1);
      g_pRenderEngine->drawTextLeft(xStart, yPos, m_iIdFontRegular, szBuff);
      return g_pRenderEngine->textWidth(m_iIdFontRegular, szBuff);
   }

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   
   bShowRed = false;

   if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      bShowRed = true;
   if ( !(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
   if ( !(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      bShowRed = true;
      
   m_fYPosCtrlRadioInterfaces[iRadioInterface] = yPos;

   g_pRenderEngine->drawIcon(xStart - hIcon/g_pRenderEngine->getAspectRatio(), yPos+height_text*0.5, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, g_idIconRadio);
      
   float xTextLeft = xStart - hIcon/g_pRenderEngine->getAspectRatio() - fPaddingInnerX;
   sprintf(szBuff, "Interface %d, Port %s", iRadioInterface+1, pRadioHWInfo->szUSBPort);
   if ( ! controllerIsCardInternal(pRadioHWInfo->szMAC) )
      strcat(szBuff, " (Ext)");

   if ( bSelected )
      bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      
   g_pRenderEngine->drawTextLeft(xTextLeft, yPos, m_iIdFontRegular, szBuff);
   fMaxWidth = g_pRenderEngine->textWidth(m_iIdFontRegular, szBuff);

   if ( bSelected )
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

   yPos += height_text*1.2;

   char szBands[128];
   str_get_supported_bands_string(pRadioHWInfo->supportedBands, szBands);
   sprintf(szBuff, "Type: %s,  %s", str_get_radio_card_model_string(pCardInfo->cardModel), szBands);
   if ( strlen(str_get_radio_card_model_string(pCardInfo->cardModel)) > 10 )
      sprintf(szBuff, "%s,  %s", str_get_radio_card_model_string(pCardInfo->cardModel), szBands);
   g_pRenderEngine->drawTextLeft(xTextLeft, yPos, m_iIdFontSmall, szBuff);
   if ( g_pRenderEngine->textWidth(m_iIdFontSmall, szBuff) > fMaxWidth )
      fMaxWidth = g_pRenderEngine->textWidth(m_iIdFontSmall, szBuff);
   yPos += height_text*1.1;

   szBuff[0] = 0;
   szError[0] = 0;

   if ( (!(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) &&
        (!(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO)) )
   {
      bShowRed = true;
      strcpy(szError, "No streams enabled");
   }

   if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( ! (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      strcat(szBuff, "Uplink Only");
   if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
   if ( ! (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      strcat(szBuff, "Downlink Only");
   if ( ! (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
   if ( ! (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
   {
      bShowRed = true;
      strcpy(szError, "Neither Uplink or Downlink enabled");
   }

   if ( controllerGetCardDataRate(pRadioHWInfo->szMAC) != 0 )
   if ( iRadioLink >= 0 )
   if ( controllerGetCardDataRate(pRadioHWInfo->szMAC) != g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iRadioLink] )
   {
      if ( 0 != szBuff[0] )
         strcat(szBuff, ", ");
      char szTmp[64], szTmp2[64];
      str_getDataRateDescription(controllerGetCardDataRate(pRadioHWInfo->szMAC), szTmp2);
      sprintf(szTmp, "Datarate: %s", szTmp2);
      strcat(szBuff, szTmp);
   }


   if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      bShowRed = true;
      strcpy(szError, "Card Is Disabled");
   }

   float ftw = g_pRenderEngine->textWidth(m_iIdFontRegular, szBuff);
   g_pRenderEngine->drawTextLeft(xTextLeft, yPos, m_iIdFontRegular, szBuff);
   if ( ftw > fMaxWidth )
      fMaxWidth = ftw;

   if ( bShowRed && (0 != szError[0]) )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->drawTextLeft(xTextLeft - ftw - 0.01, yPos, m_iIdFontRegular, szError);
      if ( ftw + 0.01 + g_pRenderEngine->textWidth(m_iIdFontRegular, szError) > fMaxWidth )
         fMaxWidth = ftw + 0.01 + g_pRenderEngine->textWidth(m_iIdFontRegular, szError);
   }

   yPos += height_text;
   yPos += height_text*1.5;

   return (xTextLeft-xStart) + fMaxWidth;
}
