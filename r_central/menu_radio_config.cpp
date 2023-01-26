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
#include "../radio/radiolink.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../common/string_utils.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "render_commands.h"
#include "osd.h"
#include "osd_common.h"
#include "popup.h"
#include "menu.h"
#include "menu_radio_config.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"
#include "menu_txinfo.h"
#include "menu_controller_radio_interface.h"
#include "menu_vehicle_radio_link.h"
#include "menu_vehicle_radio_interface.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include <ctype.h>
#include <math.h>
#include "handle_commands.h"
#include "warnings.h"
#include "notifications.h"


MenuRadioConfig::MenuRadioConfig(void)
:Menu(MENU_ID_RADIO_CONFIG, "Radio Configuration", NULL)
{
   m_Width = 0.17;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.16;
   m_bComputedHeights = false;
}

MenuRadioConfig::~MenuRadioConfig()
{
}

void MenuRadioConfig::onShow()
{
   m_Height = 0.0;
   m_ExtraHeight = 0;
   m_bComputedHeights = false;
   log_line("Entering menu radio config...");
   removeAllItems();

   m_pItemSelectTxCard = NULL;
   m_iCurrentRadioLink = -1;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_fYPosAutoTx[i] = 0.5;
      m_fYPosCtrlRadioInterfaces[i] = 0.2;
      m_fYPosVehicleRadioInterfaces[i] = 0.2;
      m_fYPosRadioLinks[i] = 0.2; 
   }

   m_bTmpHasVideoStreamsEnabled = false;
   m_bTmpHasDataStreamsEnabled = false;


   m_bHas58PowerVehicle = false;
   m_bHas24PowerVehicle = false;
   m_bHas58PowerController = false;
   m_bHas24PowerController = false;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( (NULL != pNICInfo) && (pNICInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS )
         m_bHas24PowerController = true;
      else
         m_bHas58PowerController = true;
   }
      
   if ( NULL != g_pCurrentModel )
   {
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         if ( (g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF) == RADIO_TYPE_ATHEROS )
            m_bHas24PowerVehicle = true;
         else
            m_bHas58PowerVehicle = true;
   }

   m_iCountVehicleRadioLinks =0;
   if ( NULL != g_pCurrentModel )
      m_iCountVehicleRadioLinks = g_pCurrentModel->radioLinksParams.links_count;

   log_line("Menu Radio Config: vehicle has %d radio links.", m_iCountVehicleRadioLinks);

   if ( (NULL != g_pSM_RadioStats) && (0 < g_pSM_RadioStats->countRadioLinks) )
      computeHeights();
   
   m_iIndexCurrentItem = 0;
   m_iHeaderItemsCount = 0;

   if ( m_bHas24PowerVehicle )
      m_iHeaderItemsCount++;
   if ( m_bHas58PowerVehicle )
      m_iHeaderItemsCount++;
   if ( m_bHas24PowerController )
      m_iHeaderItemsCount++;
   if ( m_bHas58PowerController )
      m_iHeaderItemsCount++;

   if ( (NULL != g_pSM_RadioStats) && (0 < g_pSM_RadioStats->countRadioLinks) )
      computeMaxItemIndex();

   Menu::onShow();

   if ( ! (menu_is_menu_on_top(this)) )
   {
      log_line("Menu radio config is not on top, close the top menu.");
      menu_stack_pop(0);
   }
   log_line("Entered menu radio config.");
}

void MenuRadioConfig::Render()
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   double color[4];
   color[0] = color[1] = color[2] = 150;
   color[3] = 0.3;
   g_pRenderEngine->setFontBackgroundBoundingBoxFillColor(color);
   g_pRenderEngine->setBackgroundBoundingBoxPadding(0.007);

   if ( ! m_bComputedHeights )
   {
      if ( NULL != g_pCurrentModel )
         m_iCountVehicleRadioLinks = g_pCurrentModel->radioLinksParams.links_count;

      if ( (NULL != g_pSM_RadioStats) && (0 < g_pSM_RadioStats->countRadioLinks) )
      {
         computeHeights();
         computeMaxItemIndex();
      }
   }

   drawRadioLinks(0.05/g_pRenderEngine->getAspectRatio(), 0.62);
   g_pRenderEngine->clearFontBackgroundBoundingBoxStrikeColor();

   if ( NULL != m_pItemSelectTxCard )
   {
      float xPos = m_RenderXPos + g_pRenderEngine->textWidth(g_idFontMenu,"TxCard");

      m_pItemSelectTxCard->setLastRenderPos(xPos, m_fYPosAutoTx[m_iCurrentRadioLink]-height_text);
      m_pItemSelectTxCard->getItemHeight(0.5);
      m_pItemSelectTxCard->Render(xPos, m_fYPosAutoTx[m_iCurrentRadioLink]-height_text, true, 0.0);
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
   if ( NULL != m_pItemSelectTxCard )
   {
      Menu::onMoveUp(bIgnoreReversion);
      return;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
      return;

   m_iIndexCurrentItem--;
   if ( m_iIndexCurrentItem < 0 )
      m_iIndexCurrentItem = m_iIndexMaxItem-1;
}

void MenuRadioConfig::onMoveDown(bool bIgnoreReversion)
{
   if ( NULL != m_pItemSelectTxCard )
   {
      Menu::onMoveDown(bIgnoreReversion);
      return;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
      return;

   m_iIndexCurrentItem++;
   if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
      m_iIndexCurrentItem = 0;
}

void MenuRadioConfig::onMoveLeft(bool bIgnoreReversion)
{
   onMoveUp(bIgnoreReversion);
}

void MenuRadioConfig::onMoveRight(bool bIgnoreReversion)
{
   onMoveDown(bIgnoreReversion);
}

void MenuRadioConfig::onFocusedItemChanged()
{

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

      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( NULL != g_pSM_RadioStats )
         if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId != m_iCurrentRadioLink )
            continue;
         controllerRemoveCardTXPreferred(pNICInfo->szMAC);
      }
        
      if ( iSelection > 0 )
      {
         int iCount = 0;
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
            if ( NULL != g_pSM_RadioStats )
            if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId != m_iCurrentRadioLink )
               continue;
            iCount++;
            if ( iCount == iSelection )
            {
               controllerSetCardTXPreferred(pNICInfo->szMAC);
               break;
            }
         }
      }
      save_ControllerInterfacesSettings();
      removeAllItems();
      m_pItemSelectTxCard = NULL;

      showProgressInfo();
      pairing_stop();
      pairing_start();
      hideProgressInfo();   
   }
}
      

int MenuRadioConfig::onBack()
{
   return Menu::onBack();
}

void MenuRadioConfig::onReturnFromChild(int returnValue)
{
   computeMaxItemIndex();

   Menu::onReturnFromChild(returnValue);
}

void MenuRadioConfig::onClickAutoTx(int iRadioLink)
{
   m_iCurrentRadioLink = iRadioLink;
   m_pItemSelectTxCard = new MenuItemSelect("");
   m_pItemSelectTxCard->addSelection("Auto");

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId == iRadioLink )
      {
         char szBuff[256];
         char szName[128];
      
         strcpy(szName, "NoName");

         radio_hw_info_t* pNIC = hardware_get_radio_info(i);
         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNIC->szMAC);
      
         char* szCardName = controllerGetCardUserDefinedName(pNIC->szMAC);
         if ( NULL != szCardName && 0 != szCardName[0] )
            strcpy(szName, szCardName);
         else if ( NULL != pCardInfo )
            strcpy(szName, str_get_radio_card_model_string(pCardInfo->cardModel) );

         sprintf(szBuff, "Int. %d, Port %s, %s", i+1, pNIC->szUSBPort, szName);
         m_pItemSelectTxCard->addSelection(szBuff);
       }    
   }
   addMenuItem(m_pItemSelectTxCard);
   m_pItemSelectTxCard->setIsEditable();
   m_pItemSelectTxCard->setPopupSelectorToRight();
   m_pItemSelectTxCard->beginEdit();
}


void MenuRadioConfig::onSelectItem()
{
   if ( NULL != m_pItemSelectTxCard )
   {
      Menu::onSelectItem();
      return;
   }

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   int dSelectionIndex = 0;

   if ( m_bHas24PowerController && m_bHas58PowerController )
   {
      if ( 0 == m_iIndexCurrentItem || 1 == m_iIndexCurrentItem )
      {
         MenuTXInfo* pMenu = new MenuTXInfo();
         pMenu->m_bShowVehicle = false;
         add_menu_to_stack(pMenu);
         return;
      }
   }
   else if ( 0 == m_iIndexCurrentItem )
   {
      MenuTXInfo* pMenu = new MenuTXInfo();
      pMenu->m_bShowVehicle = false;
      add_menu_to_stack(pMenu);
      return;
   }

   if ( m_bHas24PowerController && m_bHas58PowerController)
      dSelectionIndex += 2;
   else
      dSelectionIndex++;

   if ( m_bHas24PowerVehicle && m_bHas58PowerVehicle)
   {
      if ( dSelectionIndex == m_iIndexCurrentItem || dSelectionIndex+1 == m_iIndexCurrentItem )
      {
         MenuTXInfo* pMenu = new MenuTXInfo();
         pMenu->m_bShowController = false;
         add_menu_to_stack(pMenu); 
         return;   
      }
   }
   else if ( dSelectionIndex == m_iIndexCurrentItem )
   {
      MenuTXInfo* pMenu = new MenuTXInfo();
      pMenu->m_bShowController = false;
      add_menu_to_stack(pMenu);
      return;
   }

   if ( m_bHas24PowerVehicle && m_bHas58PowerVehicle)
      dSelectionIndex += 2;
   else
      dSelectionIndex++;

   for( int iLink=0; iLink<m_iCountVehicleRadioLinks; iLink++ )
   {
       if ( m_iIndexCurrentItem == dSelectionIndex )
       {
          MenuVehicleRadioLink* pMenu = new MenuVehicleRadioLink(iLink);
          pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.5;
          pMenu->m_yPos = m_fYPosRadioLinks[iLink] - 5.0*height_text;
          if ( pMenu->m_yPos < 0.03 )
             pMenu->m_yPos = 0.03;
          if ( pMenu->m_yPos > 0.8 )
             pMenu->m_yPos = 0.8;
          add_menu_to_stack(pMenu);
          return;
       }
       dSelectionIndex++;
       int iCountInterfaces = 0;
       for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
          if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId == iLink )
          {
             if ( m_iIndexCurrentItem == dSelectionIndex )
             {
                MenuControllerRadioInterface* pMenu = new MenuControllerRadioInterface(i);
                pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.4;
                pMenu->m_yPos = m_fYPosCtrlRadioInterfaces[i] - 5.0*height_text;
                if ( pMenu->m_yPos < 0.03 )
                   pMenu->m_yPos = 0.03;
                if ( pMenu->m_yPos > 0.8 )
                   pMenu->m_yPos = 0.8;
                add_menu_to_stack(pMenu);
                return;
             }
             dSelectionIndex++;
             iCountInterfaces++;
          }

       if ( iCountInterfaces > 1 )
       {
          if ( m_iIndexCurrentItem == dSelectionIndex )
          {
             onClickAutoTx(iLink);
             return;
          }
          dSelectionIndex++;
       }

       if ( m_iIndexCurrentItem == dSelectionIndex )
       {
          int iInt = -1;
          for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
             if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == iLink )
             {
                iInt = i;
                break;
             }
          if ( iInt < 0 )
             iInt = iLink;
          MenuVehicleRadioInterface* pMenu = new MenuVehicleRadioInterface(iInt);
          pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.8;
          pMenu->m_yPos = m_fYPosRadioLinks[iLink] + m_fHeightLinks[iLink]*0.5 - 5.0*height_text;
          if ( pMenu->m_yPos < 0.03 )
             pMenu->m_yPos = 0.03;
          if ( pMenu->m_yPos > 0.8 )
             pMenu->m_yPos = 0.8;
          add_menu_to_stack(pMenu);
          return;
       }
       dSelectionIndex++;
   }

   // Unassigned controller radio interfaces
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( (NULL == g_pCurrentModel) || ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId < 0) )
      {
         if ( m_iIndexCurrentItem == dSelectionIndex )
         {
             MenuControllerRadioInterface* pMenu = new MenuControllerRadioInterface(i);
             pMenu->m_xPos = m_RenderXPos + m_RenderWidth*0.4;
             pMenu->m_yPos = m_fYPosCtrlRadioInterfaces[i] - 5.0*height_text;
             if ( pMenu->m_yPos < 0.03 )
                pMenu->m_yPos = 0.03;
             if ( pMenu->m_yPos > 0.8 )
                pMenu->m_yPos = 0.8;
             add_menu_to_stack(pMenu);
             return;
         }
         dSelectionIndex++;
      }
   }

}

float MenuRadioConfig::computeHeights()
{
   //if ( NULL != g_pSM_RadioStats )
   //   log_line("Menu radio config: computing heights for %d vehicle radio links and %d radio links configured on controller...", m_iCountVehicleRadioLinks, g_pSM_RadioStats->countRadioLinks);
   //else
   //   log_line("Menu radio config: computing heights for %d vehicle radio links and 0 radio links configured on controller...", m_iCountVehicleRadioLinks);

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_large = g_pRenderEngine->textHeight(g_idFontMenuLarge);
   float fPaddingInnerY = 0.02;

   float hTotalHeight = 0.0;

   for( int iLink=0; iLink<m_iCountVehicleRadioLinks; iLink++ )
   {
       int countInterfaces = 0;
       for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
          if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId == iLink )
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
          hTotalHeight += height_text*1.5;
   }
   
   int iCountUnusedControllerInterfaces = 0;
   if ( NULL == g_pCurrentModel )
      iCountUnusedControllerInterfaces = hardware_get_radio_interfaces_count();
   else
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId >= 0 )
            continue;
         iCountUnusedControllerInterfaces++;
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
   return hTotalHeight;
}

void MenuRadioConfig::computeMaxItemIndex()
{
   m_iIndexMaxItem = m_iHeaderItemsCount;
   
   if ( (NULL == g_pCurrentModel) || (NULL == g_pSM_RadioStats) || (0 == g_pSM_RadioStats->countRadioLinks) )
   {
      m_iIndexMaxItem += hardware_get_radio_interfaces_count();
      if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
         m_iIndexCurrentItem = m_iIndexMaxItem-1;
      return;
   }
   //log_line("Menu radio config: computing max indexes...");
   
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId >= 0 )
         continue;
      m_iIndexMaxItem++;
   }

   for( int iLink=0; iLink<m_iCountVehicleRadioLinks; iLink++ )
   {
       m_iIndexMaxItem += 2;

       int iCountInterfaces = 0;
       for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
          if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId == iLink )
          {
             m_iIndexMaxItem++;
             iCountInterfaces++;
          }
       if ( iCountInterfaces > 1 )
          m_iIndexMaxItem++;
   }

   if ( m_iIndexCurrentItem >= m_iIndexMaxItem )
      m_iIndexCurrentItem = m_iIndexMaxItem-1;

   //log_line("Menu radio config: computed max indexes.");
}


void MenuRadioConfig::drawRadioLinks(float xStart, float xEnd)
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_large = g_pRenderEngine->textHeight(g_idFontMenuLarge);
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

   g_pRenderEngine->drawRoundRect(xStart, fMarginY, fWidth, 1.0-2.0*fMarginY, MENU_ROUND_MARGIN*menu_getScaleMenus());
   
   xStart += fPaddingX;
   xEnd -= fPaddingX;
   fWidth -= 2.0 * fPaddingX;

   m_RenderXPos = xStart;
   m_RenderWidth = fWidth;
   
   computeMaxItemIndex();

   m_bTmpHasVideoStreamsEnabled = false;
   m_bTmpHasDataStreamsEnabled = false;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   float yPos = yStart;

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      yPos += height_text*4.0;
      g_pRenderEngine->drawText(xStart, yPos, g_idFontMenuLarge, "No radio interfaces on controller!");
      return;   
   }

   if ( (NULL == g_pSM_RadioStats) || (0 == g_pSM_RadioStats->countRadioLinks) )
   {
      yPos += height_text*4.0;
      g_pRenderEngine->drawText(xStart, yPos, g_idFontMenuLarge, "Radio interfaces not yet configured on the controller.");
      return;  
   }

   double colorStrike[4] = {255,250,200,0.8};
   g_pRenderEngine->setFontBackgroundBoundingBoxStrikeColor(colorStrike);

   float yTmp = yPos;
   yPos += drawRadioHeader(xStart, xEnd, yPos);

   float hTotalHeight = computeHeights();
   
   yPos += 0.3*((yEnd-yPos) - hTotalHeight);


   // ----------------------
   // Draw mid separator

   float fAlpha = g_pRenderEngine->setGlobalAlfa(0.5);
   for( float fy = yTmp+height_text_large*1.2 ; fy<yPos-0.01; fy += 0.02 )
      g_pRenderEngine->drawLine(fXMid, fy, fXMid, fy+0.01);
   g_pRenderEngine->setGlobalAlfa(fAlpha);

   for( int iLink = 0; iLink < m_iCountVehicleRadioLinks; iLink++ )
   {
      drawRadioLink(xStart, xEnd, yPos, iLink );
      yPos += m_fHeightLinks[iLink] + height_text*1.5;
   }

   if ( ! m_bTmpHasVideoStreamsEnabled )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->drawText(xStart, yPos-height_text*0.3, g_idFontMenu, "Warning: No radio links are setup to transmit video streams.");
      yPos += height_text;
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }
   if ( ! m_bTmpHasDataStreamsEnabled )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->drawText(xStart, yPos-height_text*0.3, g_idFontMenu, "Warning: No radio links are setup to transmit data streams.");
      yPos += height_text;
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }

   // Show unused interfaces on the controller (disabled or not usable for current vehicle)

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   int iCountUnusedControllerInterfaces = 0;
   if ( NULL == g_pCurrentModel )
      iCountUnusedControllerInterfaces = hardware_get_radio_interfaces_count();
   else
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId >= 0 )
            continue;
         iCountUnusedControllerInterfaces++;
      }

   if ( 0 == iCountUnusedControllerInterfaces )
      return;

   if ( NULL != g_pCurrentModel )
   {
      yPos += height_text;
      g_pRenderEngine->drawText(xStart, yPos, g_idFontMenu, "Unused controller radio interfaces:");
      yPos += height_text;
      g_pRenderEngine->drawText(xStart, yPos, g_idFontMenu, "  (disabled or not usable for current vehicle radio configuration)");
      yPos += height_text;
      yPos += height_text;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( (NULL == g_pCurrentModel) || ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId < 0) )
         yPos += drawRadioInterfaceController(fXMid, xEnd, yPos, -1, i);
   }

   g_pRenderEngine->clearFontBackgroundBoundingBoxStrikeColor();
}

float MenuRadioConfig::drawRadioHeader(float xStart, float xEnd, float yStart)
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_large = g_pRenderEngine->textHeight(g_idFontMenuLarge);
   float hIconBig = height_text*3.0;
   float hIcon = height_text*2.0;
   float fWidth = xEnd - xStart;
   float yPos = yStart;
   float xMid = (xStart+xEnd)*0.5;
   float xMidMargin = 0.03/g_pRenderEngine->getAspectRatio();
   bool bBBox = false;

   char szBuff[128];

   ControllerSettings* pCS = get_ControllerSettings();

   float ftw = g_pRenderEngine->textWidth(g_idFontMenuLarge, "Current Radio Configuration");
   g_pRenderEngine->drawText(xStart + (xEnd-xStart - ftw)*0.5, yPos-height_text_large*0.2, g_idFontMenuLarge, "Current Radio Configuration");
   yPos += height_text_large*1.2;

   u32 uIdIconVehicle = g_idIconDrone;
   if ( NULL != g_pCurrentModel )
      uIdIconVehicle = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );
   //g_pRenderEngine->drawIcon(xStart + fWidth*0.2-hIconBig*0.5/g_pRenderEngine->getAspectRatio(), yPos, hIconBig/g_pRenderEngine->getAspectRatio(), hIconBig, g_idIconJoystick);
   //g_pRenderEngine->drawIcon(xStart + fWidth*0.8-hIconBig*0.5/g_pRenderEngine->getAspectRatio(), yPos, hIconBig/g_pRenderEngine->getAspectRatio(), hIconBig, uIdIconVehicle);
   
   g_pRenderEngine->drawIcon(xMid - xMidMargin - hIconBig/g_pRenderEngine->getAspectRatio(), yPos, hIconBig/g_pRenderEngine->getAspectRatio(), hIconBig, g_idIconJoystick);
   g_pRenderEngine->drawIcon(xMid + xMidMargin, yPos, hIconBig/g_pRenderEngine->getAspectRatio(), hIconBig, uIdIconVehicle);
   
   float xLeft = xMid - xMidMargin - hIconBig/g_pRenderEngine->getAspectRatio() - 0.02/g_pRenderEngine->getAspectRatio();
   float xRight = xMid + xMidMargin + hIconBig/g_pRenderEngine->getAspectRatio() + 0.02/g_pRenderEngine->getAspectRatio();

   g_pRenderEngine->drawTextLeft(xLeft, yPos + (hIconBig-height_text)*0.5, 0.0, g_idFontMenu, "Controller");

   if ( NULL == g_pCurrentModel )
      strcpy(szBuff, "No Vehicle Active");
   else
      strcpy(szBuff, g_pCurrentModel->getLongName());

   g_pRenderEngine->drawText(xRight, yPos + (hIconBig-height_text)*0.5, g_idFontMenu, szBuff);
   
   yPos += height_text*0.6;

   int iLines = 1;
   int dSelectionIndex = 0;
   if ( m_bHas24PowerController && m_bHas58PowerController )
   {
      iLines = 2;
      if ( m_iIndexCurrentItem == 0 )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      sprintf(szBuff,"Tx Power 2.4Ghz: %d", pCS->iTXPowerAtheros);
      g_pRenderEngine->drawTextLeft(xMid - xMidMargin*1.2, yPos + hIconBig, 0.0, g_idFontMenu, szBuff);

      if ( m_iIndexCurrentItem == 0 )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      if ( m_iIndexCurrentItem == 1 )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      sprintf(szBuff,"Tx Power 5.8Ghz: %d", pCS->iTXPowerRTL);
      g_pRenderEngine->drawTextLeft(xMid - xMidMargin*1.2, yPos + hIconBig + height_text*1.4, 0.0, g_idFontMenu, szBuff);

      if ( m_iIndexCurrentItem == 1 )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

      dSelectionIndex = 2;
   }
   else
   {
      if ( m_iIndexCurrentItem == 0 )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      if ( m_bHas58PowerController )
         sprintf(szBuff,"Tx Power: %d", pCS->iTXPowerRTL);
      else
         sprintf(szBuff,"Tx Power: %d", pCS->iTXPowerAtheros);
      g_pRenderEngine->drawTextLeft(xMid - xMidMargin*1.2, yPos + hIconBig, g_idFontMenu, szBuff);

      if ( m_iIndexCurrentItem == 0 )
         g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);
      dSelectionIndex = 1;
   }

   if ( NULL != g_pCurrentModel )
   {
      if ( m_bHas24PowerVehicle && m_bHas58PowerVehicle )
      {
         iLines = 2;
         if ( m_iIndexCurrentItem == dSelectionIndex )
            bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
         sprintf(szBuff,"Tx Power 2.4Ghz: %d", g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
         g_pRenderEngine->drawText(xMid + xMidMargin*1.2, yPos + hIconBig, 0, g_idFontMenu, szBuff);

         if ( m_iIndexCurrentItem == dSelectionIndex )
            g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

         if ( m_iIndexCurrentItem == dSelectionIndex+1 )
            bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
         sprintf(szBuff,"Tx Power 5.8Ghz: %d", g_pCurrentModel->radioInterfacesParams.txPowerRTL);
         g_pRenderEngine->drawText(xMid + xMidMargin*1.2, yPos +hIconBig + height_text*1.4, 0, g_idFontMenu, szBuff);

         if ( m_iIndexCurrentItem == dSelectionIndex+1 )
            g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);
      }
      else
      {
         if ( m_iIndexCurrentItem == dSelectionIndex )
            bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
         if ( m_bHas58PowerVehicle )
            sprintf(szBuff,"Tx Power: %d", g_pCurrentModel->radioInterfacesParams.txPowerRTL);
         else
            sprintf(szBuff,"Tx Power: %d", g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
         g_pRenderEngine->drawText(xMid + xMidMargin*1.2, yPos + hIconBig, 0, g_idFontMenu, szBuff);

         if ( m_iIndexCurrentItem == dSelectionIndex )
            g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);
      }
   }

   yPos += hIconBig + iLines*height_text*1.4;
   
   return yPos - yStart;
}


float MenuRadioConfig::drawRadioLink(float xStart, float xEnd, float yStart, int iRadioLink)
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_large = g_pRenderEngine->textHeight(g_idFontMenuLarge);
   float hIcon = height_text*2.4;
   float fPaddingInnerY = 0.02;
   float fPaddingInnerX = fPaddingInnerY/g_pRenderEngine->getAspectRatio();
   
   bool bBBox = false;
   bool bRadioLinkHasUplink = false;
   bool bRadioLinkHasDownlink = false;
   bool bShowLinkRed = false;
   bool bShowRed = false;
   bool bHasAutoTxOption = false;

   char szTmp[64];
   char szBuff[128];
   char szError[128];
   char szCapab[128];
   
   float xLeftMax[MAX_RADIO_INTERFACES];
   float xRightMin = 1.0;
   float yMidVehicle = 0.0;
   float yMidController[MAX_RADIO_INTERFACES];
   float fWidth = xEnd - xStart;
   float xMid = (xStart + xEnd)*0.5;
   float xMidMargin = 0.05/g_pRenderEngine->getAspectRatio();
   float yPos = yStart;

   szBuff[0] = 0;
   szTmp[0] = 0;
   szError[0] = 0;

   m_fYPosRadioLinks[iRadioLink] = yPos;

   // Begin - compute start selection menu index

   int iCountInterfaces = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId != iRadioLink )
         continue;

      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
      if ( NULL == pNICInfo )
         continue;
      iCountInterfaces++;
   }

   if ( iCountInterfaces > 1 )
      bHasAutoTxOption = true;

   int iStartSelectionIndex = m_iHeaderItemsCount;
   for( int iLink = 0; iLink < iRadioLink; iLink++ )
   {
      iStartSelectionIndex += 2;
      int iCInterfaces = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId != iLink )
            continue;

         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( NULL == pNICInfo )
            continue;

         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
         if ( NULL == pNICInfo )
            continue;

         iStartSelectionIndex++;
         iCInterfaces++;
      }
      if ( iCInterfaces > 1 )
         iStartSelectionIndex++;
   }

   int iSelectionIndexVehicleInterface = iStartSelectionIndex + iCountInterfaces+1;
   if ( iCountInterfaces > 1 )
      iSelectionIndexVehicleInterface++;

   // End - compute start selection menu index
   
   bool bIsLinkActive = true;

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      strcpy(szError, "Disabled");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA ) )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO ) )
   {
      strcpy(szError, "No data streams enabled on this radio link.");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }
   
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX ) )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX ) )
   {
      strcpy(szError, "Both Uplink and Downlink capabilities are disabled.");
      bShowLinkRed = true;
      bIsLinkActive = false;
   }

   if ( ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX ) &&
        ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX ) )
   {
      strcpy(szCapab, "Uplink & Downlink");
      bRadioLinkHasUplink = true;
      bRadioLinkHasDownlink = true;
   }
   else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   {
      strcpy(szCapab, "Downlink Only");
      bRadioLinkHasDownlink = true;
   }
   else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
   {
      strcpy(szCapab, "Uplink Only");
      bRadioLinkHasUplink = true;
   }
   if ( 0 < strlen(szCapab) )
      strcat(szCapab, ",   ");
   if ( ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO ) &&
        ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA ) )
   {
      strcat(szCapab, "Video & Data,");
      if ( bIsLinkActive )
      {
         m_bTmpHasVideoStreamsEnabled = true;
         m_bTmpHasDataStreamsEnabled = true;
      }
   }
   else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
   {
      strcat(szCapab, "Video Only,");
      if ( bIsLinkActive )
         m_bTmpHasVideoStreamsEnabled = true;
   }
   else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   {
      strcat(szCapab, "Data Only,");
      if ( bIsLinkActive )
         m_bTmpHasDataStreamsEnabled = true;
   }
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   if ( bIsLinkActive )
       sprintf(szBuff, "Radio Link %d   %d Mhz", iRadioLink+1, g_pCurrentModel->radioLinksParams.link_frequency[iRadioLink]);
   else
      sprintf(szBuff, "Radio Link %d (Not Active)", iRadioLink+1);
      
   if ( bShowLinkRed )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }
   g_pRenderEngine->drawText(xStart + 0.02, yPos-height_text*0.3, g_idFontMenuLarge, szBuff);

   if ( bShowLinkRed )
   {
      float ftw = g_pRenderEngine->textWidth(g_idFontMenuLarge, szBuff);
      g_pRenderEngine->drawText(xStart + ftw + 0.03, yPos, g_idFontMenu, szError);
   }
   yPos += height_text_large;

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

   g_pRenderEngine->drawRoundRect(xStart, yPos, fWidth, m_fHeightLinks[iRadioLink]-height_text_large, MENU_ROUND_MARGIN*menu_getScaleMenus());
  
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   yPos += fPaddingInnerY;
   getDataRateDescription(g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0], szTmp);
   sprintf(szBuff, "%d Mhz   %s   Data rate: %s", g_pCurrentModel->radioLinksParams.link_frequency[iRadioLink], szCapab, szTmp);
   if ( g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0] != g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][1] )
   {
      getDataRateDescription(g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][1], szTmp);
      strcat(szBuff, " / ");
      strcat(szBuff, szTmp); 
   }

   int adaptive = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
   if ( adaptive )
      strcat(szBuff, " (Auto Adjust)");
   else
      strcat(szBuff, " (Fixed)");
   
   if ( m_iIndexCurrentItem == iStartSelectionIndex )
      bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
         
   float fwt = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawText(xStart + fPaddingInnerX, yPos, g_idFontMenu, szBuff);

   if ( m_iIndexCurrentItem == iStartSelectionIndex )
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);
   
   yPos += height_text*2.0;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   //--------------------------------------------------
   // Draw vehicle interfaces

   float y1 = yPos;

   if ( bHasAutoTxOption )
      yPos += (m_fHeightLinks[iRadioLink]-height_text_large - 2.0*fPaddingInnerY-4.0*height_text) * 0.5 - height_text*1.5;
   else
      yPos += (m_fHeightLinks[iRadioLink]-height_text_large - 2.0*fPaddingInnerY-2.0*height_text) * 0.5 - height_text*1.5;

   float yStartVehicle = yPos;
   yMidVehicle = yPos + height_text*1.5;
   szTmp[0] = 0;
   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      strcat(szTmp, " Disabled");
      bShowRed = true;
   }
   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
   {
      strcat(szTmp, " Can't Uplink or Downlink");
      bShowRed = true;
   }
   
   g_pRenderEngine->drawIcon(xMid + xMidMargin, yPos+height_text*0.5, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, g_idIconRadio);

   float xiRight = xMid + xMidMargin + hIcon/g_pRenderEngine->getAspectRatio() + fPaddingInnerX;
   sprintf(szBuff, "Interface %d, Port %s", iRadioLink+1, g_pCurrentModel->radioInterfacesParams.interface_szPort[iRadioLink]);
   
   if ( m_iIndexCurrentItem == iSelectionIndexVehicleInterface  )
      bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
    
   g_pRenderEngine->drawText(xiRight, yPos, g_idFontMenu, szBuff);

   if ( m_iIndexCurrentItem == iSelectionIndexVehicleInterface  )
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

   yPos += height_text*1.3;

   sprintf(szBuff, "Type: %s", str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioLink]));
   g_pRenderEngine->drawText(xiRight, yPos, g_idFontMenu, szBuff);
   yPos += height_text;

   szBuff[0] = 0;

   /*
   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      strcat(szBuff, "Data & Video");
   else if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      strcat(szBuff, "Video Only");
   else if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      strcat(szBuff, "Data Only");
   else
   {
      bShowRed = true;
      strcat(szTmp, " No streams enabled");
   } 
   */
   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA ))
   if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO ))
   {
      bShowRed = true;
      strcat(szTmp, " No streams enabled");
   }

   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      strcat(szBuff, "Uplink & Downlink");
   else if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      strcat(szBuff, "Downlink Only");
   else if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      strcat(szBuff, "Uplink Only");
   else
   {
      bShowRed = true;
      strcat(szTmp, " Disabled");
   }
   
   g_pRenderEngine->drawText(xiRight, yPos,  g_idFontMenu, szBuff);
   yPos += height_text;

   //g_pRenderEngine->drawLine(xiRight - fPaddingInnerX, yStartVehicle + height_text*0.5, xiRight- fPaddingInnerX, yPos - height_text*0.5);

   //------------------------------------------------
   // Draw controller interfaces
   
   float yTopInterfaces = y1;
   yPos = y1;
   iCountInterfaces = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      xLeftMax[i] = 0.0;
      yMidController[i] = 0.0;
      if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId != iRadioLink )
         continue;

      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
      if ( NULL == pNICInfo )
         continue;

      iCountInterfaces++;

      if ( iCountInterfaces != 1 )
         yPos += height_text*1.0;

      m_fYPosCtrlRadioInterfaces[i] = yPos;
      y1 = yPos;
      yMidController[i] = yPos + height_text*1.5;

      bool bSelected = false;
      if ( m_iIndexCurrentItem == iStartSelectionIndex + iCountInterfaces )
         bSelected = true;

      xLeftMax[i] = xMid - xMidMargin - drawRadioInterfaceCtrlInfo(xMid - xMidMargin, 1.0, yPos, iRadioLink, i, bSelected);
      yPos += height_text*1.2;
      yPos += height_text*1.1;
      yPos += height_text;

   }

   //--------------------------------------------------
   // Draw auto tx card
   if ( iCountInterfaces > 1 )
   {
      yPos += 0.5 * height_text;
      m_fYPosAutoTx[iRadioLink] = yPos;

      sprintf(szBuff, "Preferred Tx Card: Auto");
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId != iRadioLink )
         continue;

         radio_hw_info_t* pNICInfo2 = hardware_get_radio_info(i);
         if ( NULL == pNICInfo2 )
            continue;
         if ( controllerIsCardTXPreferred(pNICInfo2->szMAC) )
         {
            sprintf(szBuff, "Preferred Tx Card: Interface %d, Port %s", i+1, pNICInfo2->szUSBPort);
            if ( ! controllerIsCardInternal(pNICInfo2->szMAC) )
               strcat(szBuff, " (Ext)");
            break;
         }
      }

      if ( m_iIndexCurrentItem == iStartSelectionIndex + iCountInterfaces + 1 )
         bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
    
      g_pRenderEngine->drawText(xStart + 0.02 + fPaddingInnerX, yPos, g_idFontMenu, szBuff);
      
      if ( m_iIndexCurrentItem == iStartSelectionIndex + iCountInterfaces + 1 )
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
   if ( iCountInterfaces > 1 )
   {
      arrowSpacing = height_text*1.6/(iCountInterfaces-1);
      dyArrow = - height_text*0.8;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId != iRadioLink )
         continue;

      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
      if ( NULL == pNICInfo )
         continue;

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
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         bCanUplink = true;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         bCanDownlink = true;   

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
         bCanExchangeData = true;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
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
         double cY[4] = {255,240,100,1.0};
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
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float fPaddingInnerY = 0.02;
   float fPaddingInnerX = fPaddingInnerY/g_pRenderEngine->getAspectRatio();

   char szBuff[128];
   char szError[128];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   bool bSelected = false;
   
   int iCountAfterIt = 0;
   for( int i=hardware_get_radio_interfaces_count()-1; i>=0; i-- )
   {
       if ( i == iRadioInterface )
          break;
       if ( (NULL == g_pCurrentModel) || (g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId == -1) )
          iCountAfterIt++;
   }
   if ( m_iIndexCurrentItem == (m_iIndexMaxItem-iCountAfterIt-1) )
      bSelected = true;

   if ( (NULL != g_pCurrentModel) && (g_pSM_RadioStats->radio_interfaces[iRadioInterface].assignedRadioLinkId >= 0) )
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
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
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

   radio_hw_info_t* pNICInfo = hardware_get_radio_info(iRadioInterface);
   if ( NULL == pNICInfo )
   {
      sprintf(szBuff, "Can't get info about radio interface %d.", iRadioInterface+1);
      g_pRenderEngine->drawTextLeft(xStart, yPos, g_idFontMenu, szBuff);
      return g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   }

   t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
   if ( NULL == pNICInfo )
   {
      sprintf(szBuff, "Can't get info about radio interface %d.", iRadioInterface+1);
      g_pRenderEngine->drawTextLeft(xStart, yPos, g_idFontMenu, szBuff);
      return g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
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
   sprintf(szBuff, "Interface %d, Port %s", iRadioInterface+1, pNICInfo->szUSBPort);
   if ( ! controllerIsCardInternal(pNICInfo->szMAC) )
      strcat(szBuff, " (Ext)");

   if ( bSelected )
      bBBox = g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      
   g_pRenderEngine->drawTextLeft(xTextLeft, yPos, g_idFontMenu, szBuff);
   fMaxWidth = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);

   if ( bSelected )
      g_pRenderEngine->drawBackgroundBoundingBoxes(bBBox);

   yPos += height_text*1.2;

   char szBands[128];
   str_get_supported_bands_string(pNICInfo->supportedBands, szBands);
   sprintf(szBuff, "Type: %s,  %s", str_get_radio_card_model_string(pCardInfo->cardModel), szBands);
   if ( strlen(str_get_radio_card_model_string(pCardInfo->cardModel)) > 10 )
      sprintf(szBuff, "%s,  %s", str_get_radio_card_model_string(pCardInfo->cardModel), szBands);
   g_pRenderEngine->drawTextLeft(xTextLeft, yPos, 0.0, g_idFontMenu, szBuff);
   if ( g_pRenderEngine->textWidth(g_idFontMenu, szBuff) > fMaxWidth )
      fMaxWidth = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   yPos += height_text*1.1;

   szBuff[0] = 0;
   szError[0] = 0;

   /*
   if ( (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) &&
        (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
      strcat(szBuff, "Video & Data");
   else if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      strcat(szBuff, "Video Only");
   else if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      strcat(szBuff, "Data Only");
   else
   {
      bShowRed = true;
      strcpy(szError, "No streams enabled");
   }
   if ( 0 < strlen(szBuff) )
      strcat(szBuff, ",  ");
   */

   if ( (!(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) &&
        (!(pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO)) )
   {
      bShowRed = true;
      strcpy(szError, "No streams enabled");
   }

   if ( (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) &&
        (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      strcat(szBuff, "Uplink & Downlink");
   else if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      strcat(szBuff, "Uplink Only");
   else if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      strcat(szBuff, "Downlink Only");
   else
   {
      bShowRed = true;
      strcpy(szError, "Neither Uplink or Downlink enabled");
   }

   if ( controllerGetCardDataRate(pNICInfo->szMAC) != 0 )
   if ( iRadioLink >= 0 )
   if ( controllerGetCardDataRate(pNICInfo->szMAC) != g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0] )
   {
      if ( 0 != szBuff[0] )
         strcat(szBuff, ", ");
      char szTmp[64], szTmp2[64];
      getDataRateDescription(controllerGetCardDataRate(pNICInfo->szMAC), szTmp2);
      sprintf(szTmp, "Datarate: %s", szTmp2);
      strcat(szBuff, szTmp);
   }


   if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      bShowRed = true;
      strcpy(szError, "Card Is Disabled");
   }

   float ftw = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawTextLeft(xTextLeft, yPos, g_idFontMenu, szBuff);
   if ( ftw > fMaxWidth )
      fMaxWidth = ftw;

   if ( bShowRed && (0 != szError[0]) )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->drawTextLeft(xTextLeft - ftw - 0.01, yPos, g_idFontMenu, szError);
      if ( ftw + 0.01 + g_pRenderEngine->textWidth(g_idFontMenu, szError) > fMaxWidth )
         fMaxWidth = ftw + 0.01 + g_pRenderEngine->textWidth(g_idFontMenu, szError);
   }

   yPos += height_text;
   yPos += height_text*1.5;

   return (xTextLeft-xStart) + fMaxWidth;
}