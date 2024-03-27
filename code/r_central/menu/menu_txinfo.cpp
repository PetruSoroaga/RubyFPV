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

#include "menu.h"
#include "menu_txinfo.h"
#include "menu_objects.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"
#include "menu_tx_power_max.h"

#define TX_POWER_TABLE_COLUMNS 11

static const int s_iTxPowerLevelValues[TX_POWER_TABLE_COLUMNS] = { 10,  20,   30,   40,   50,   54,   56,   60,   63,   68,   72};

static int s_iTxBoosterGainTable4W[][2] =
{
   {2, 100},
   {5, 230},
   {10, 420},
   {20, 800},
   {30, 1100},
   {40, 1300},
   {50, 1400},
   {60, 1500},
   {70, 1600},
   {80, 1700},
   {100, 4000}
};

MenuTXInfo::MenuTXInfo()
:Menu(MENU_ID_TXINFO, "Radio Output Power Levels", NULL)
{
   m_Width = 0.72;
   m_Height = 0.61;
   m_xPos = 0.05;
   m_yPos = 0.16;

   m_xTable = m_RenderXPos + m_sfMenuPaddingY;
   m_xTable += 0.15*m_sfScaleFactor;
   m_xTableCellWidth = 0.05*m_sfScaleFactor;

   m_bSelectSecond = false;
   m_bShowThinLine = false;
   
   m_bShowVehicle = true;
   m_bShowController = true;
   
   m_bValuesChangedVehicle = false;
   m_bValuesChangedController = false;
}

MenuTXInfo::~MenuTXInfo()
{
}

void MenuTXInfo::onShow()
{
   float fSliderWidth = 0.18;
   ControllerSettings* pCS = get_ControllerSettings();
 
   m_bShowBothOnController = false;
   m_bShowBothOnVehicle = false;
   m_bDisplay24Cards = false;
   m_bDisplay58Cards = false;

   m_bControllerHas24Cards = false;
   m_bControllerHas58Cards = false;
   m_bVehicleHas24Cards = false;
   m_bVehicleHas58Cards = false;

   for( int n=0; n<hardware_get_radio_interfaces_count(); n++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(n);
      if ( NULL == pNIC )
         continue;
      if ( (pNIC->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS )
         m_bControllerHas24Cards = true;
      if ( (pNIC->typeAndDriver & 0xFF) == RADIO_TYPE_RALINK )
         m_bControllerHas24Cards = true;
      else
         m_bControllerHas58Cards = true;
   }

   if ( m_bControllerHas24Cards && m_bControllerHas58Cards )
      m_bShowBothOnController = true;

   if ( NULL != g_pCurrentModel )
   {
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( ((g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF00) >> 8) == RADIO_HW_DRIVER_ATHEROS )
            m_bVehicleHas24Cards = true;
         if ( ((g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF00) >> 8) == RADIO_HW_DRIVER_RALINK )
            m_bVehicleHas24Cards = true;
         else
            m_bVehicleHas58Cards = true;
      }
   }

   if ( m_bVehicleHas24Cards && m_bVehicleHas58Cards )
      m_bShowBothOnVehicle = true;

   if ( m_bShowVehicle && m_bVehicleHas24Cards )
      m_bDisplay24Cards = true;
   if ( m_bShowController && m_bControllerHas24Cards )
      m_bDisplay24Cards = true;
   if ( m_bShowVehicle && m_bVehicleHas58Cards )
      m_bDisplay58Cards = true;
   if ( m_bShowController && m_bControllerHas58Cards )
      m_bDisplay58Cards = true;

   removeAllItems();

   m_IndexPowerController = -1;
   m_IndexPowerControllerAtheros = -1;
   m_IndexPowerControllerRTL = -1;

   m_IndexPowerVehicle = -1;
   m_IndexPowerVehicleAtheros = -1;
   m_IndexPowerVehicleRTL = -1;

   if ( m_bShowVehicle && (NULL != g_pCurrentModel) )
   {
      if ( ! m_bShowBothOnVehicle )
      {
         if ( m_bVehicleHas24Cards )
            m_pItemsSlider[0] = new MenuItemSlider("Vehicle Tx Power", "Sets the radio TX power used on the vehicle. Requires a reboot of the vehicle after change.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros/2, fSliderWidth);
         else
            m_pItemsSlider[0] = new MenuItemSlider("Vehicle Tx Power", "Sets the radio TX power used on the vehicle. Requires a reboot of the vehicle after change.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL/2, fSliderWidth);
         m_IndexPowerVehicle = addMenuItem(m_pItemsSlider[0]);
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

         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Vehicle Tx Power (2.4Ghz%s)", szCards);
         m_pItemsSlider[1] = new MenuItemSlider(szBuff, "Sets the radio TX power used on the vehicle for Atheros/RaLink cards. Requires a reboot of the vehicle after change.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros/2, fSliderWidth);
         m_IndexPowerVehicleAtheros = addMenuItem(m_pItemsSlider[1]);
         m_pItemsSlider[2] = new MenuItemSlider("Vehicle Tx Power (5.8Ghz)", "Sets the radio TX power used on the vehicle for RTL cards. Requires a reboot of the vehicle after change.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL/2, fSliderWidth);
         m_IndexPowerVehicleRTL = addMenuItem(m_pItemsSlider[2]);
      }
   }

   if ( m_bShowController )
   {
      if ( ! m_bShowBothOnController )
      {
         if ( m_bControllerHas24Cards )
            m_pItemsSlider[5] = new MenuItemSlider("Controller Tx Power", "Sets the radio TX power used on the controller. Requires a reboot of the controller after change.", 1, pCS->iMaxTXPowerAtheros, pCS->iMaxTXPowerAtheros/2, fSliderWidth);
         else
            m_pItemsSlider[5] = new MenuItemSlider("Controller Tx Power", "Sets the radio TX power used on the controller. Requires a reboot of the controller after change.", 1, pCS->iMaxTXPowerRTL, pCS->iMaxTXPowerRTL/2, fSliderWidth);
         m_IndexPowerController = addMenuItem(m_pItemsSlider[5]);
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

         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Controller Tx Power (2.4Ghz%s)", szCards);
         m_pItemsSlider[6] = new MenuItemSlider(szBuff, "Sets the radio TX power used on the controller for Atheros/RaLink cards. Requires a reboot of the controller after change.", 1, pCS->iMaxTXPowerAtheros, pCS->iMaxTXPowerAtheros/2, fSliderWidth);
         m_IndexPowerControllerAtheros = addMenuItem(m_pItemsSlider[6]);
         m_pItemsSlider[7] = new MenuItemSlider("Controller Tx Power (5.8Ghz)", "Sets the radio TX power used on the controller for RTL cards. Requires a reboot of the controller after change.", 1, pCS->iMaxTXPowerRTL, pCS->iMaxTXPowerRTL/2, fSliderWidth);
         m_IndexPowerControllerRTL = addMenuItem(m_pItemsSlider[7]);
      }
   }
   m_IndexPowerMax = addMenuItem(new MenuItem("Limit Maximum Radio Power Levels"));
   m_pMenuItems[m_IndexPowerMax]->showArrow();

   m_pItemsSelect[0] = new MenuItemSelect("Show All Card Types", "Shows all card types in the table below, not just the ones detected as present.");  
   m_pItemsSelect[0]->addSelection("No");
   m_pItemsSelect[0]->addSelection("Yes");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexShowAllCards = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Show 2W/4W Boosters", "Shows the power output for current radio interfaces when connected to 2W or 4W power boosters.");  
   m_pItemsSelect[1]->addSelection("No");
   m_pItemsSelect[1]->addSelection("Yes");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexShowBoosters = addMenuItem(m_pItemsSelect[1]);


   addMenuItem( new MenuItemText("Here is a table with aproximative ouput power levels for different cards:"));

   bool bFirstShow = m_bFirstShow;
   Menu::onShow();

   if ( bFirstShow && m_bSelectSecond )
      m_SelectedIndex = 1;
} 
      
void MenuTXInfo::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pP = get_Preferences();

   m_pItemsSelect[0]->setSelectedIndex(1 - pP->iShowOnlyPresentTxPowerCards);
   m_pItemsSelect[1]->setSelectedIndex(pP->iShowTxBoosters);

   if ( m_bShowController )
   {
      if ( -1 != m_IndexPowerController )
      {
         if ( m_bControllerHas24Cards )
            m_pItemsSlider[5]->setCurrentValue(pCS->iTXPowerAtheros);
         if ( m_bControllerHas58Cards )
            m_pItemsSlider[5]->setCurrentValue(pCS->iTXPowerRTL);
      }
      if ( -1 != m_IndexPowerControllerAtheros && m_bControllerHas24Cards )
         m_pItemsSlider[6]->setCurrentValue(pCS->iTXPowerAtheros);
      if ( -1 != m_IndexPowerControllerRTL && m_bControllerHas58Cards )
         m_pItemsSlider[7]->setCurrentValue(pCS->iTXPowerRTL);
   }

   if ( (NULL == g_pCurrentModel) || (!m_bShowVehicle) )
      return;

   if ( -1 != m_IndexPowerVehicle )
   {
      if ( m_bVehicleHas24Cards )
         m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
      if ( m_bVehicleHas58Cards )
         m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerRTL);
   }
   if ( -1 != m_IndexPowerVehicleAtheros && m_bVehicleHas24Cards )
      m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
   if ( -1 != m_IndexPowerVehicleRTL && m_bVehicleHas58Cards )
      m_pItemsSlider[2]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerRTL);
}


void MenuTXInfo::RenderTableLine(int iCardModel, const char* szText, const int* piValues, bool bIsHeader, bool bIsBoosterLine)
{
   Preferences* pP = get_Preferences();

   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   float x = m_RenderXPos + m_sfMenuPaddingX;
   float y = m_yTemp;
   float hItem = (1.0 + MENU_ITEM_SPACING) * g_pRenderEngine->textHeight(g_idFontMenuSmall);

   bool bIsPresentOnVehicle = false;
   bool bIsPresentOnController = false;

   if ( 0 != iCardModel )
   {
      if ( m_bShowVehicle && (NULL != g_pCurrentModel) )
      {
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_card_model[i] == iCardModel )
               bIsPresentOnVehicle = true;
            if ( g_pCurrentModel->radioInterfacesParams.interface_card_model[i] == -iCardModel )
               bIsPresentOnVehicle = true;
         }
      }

      if ( m_bShowController )
      {
         for( int n=0; n<hardware_get_radio_interfaces_count(); n++ )
         {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(n);
            if ( NULL == pRadioHWInfo )
               continue;

            t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
            if ( NULL == pCardInfo )
               continue;
            if ( iCardModel == pCardInfo->cardModel )
               bIsPresentOnController = true;
            if ( iCardModel == -(pCardInfo->cardModel) )
               bIsPresentOnController = true;
         }
      }
   }

   if ( (!bIsHeader) && pP->iShowOnlyPresentTxPowerCards )
   {
      if ( m_bShowVehicle && (! bIsPresentOnVehicle) )
         return;
      if ( m_bShowController && (! bIsPresentOnController) )
         return;
   }

   if ( 0 != iCardModel )
   if ( (!bIsHeader) && (! pP->iShowOnlyPresentTxPowerCards) )
   if ( bIsPresentOnController || bIsPresentOnVehicle )
   {
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.2);
      g_pRenderEngine->setFill(250,200,50,0.1);
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->setStrokeSize(1);
      
      float h = hItem + 0.4 * m_sfMenuPaddingY;
      if ( pP->iShowTxBoosters )
      if ( ! bIsBoosterLine )
      if ( bIsPresentOnController || bIsPresentOnVehicle )
         h += hItem;
      g_pRenderEngine->drawRoundRect(x - m_sfMenuPaddingX*0.4, y - m_sfMenuPaddingY*0.2, m_RenderWidth - 2.0*m_sfMenuPaddingX + 0.8 * m_sfMenuPaddingX, h, MENU_ROUND_MARGIN * m_sfMenuPaddingY);

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->setStrokeSize(0);
   }

   g_pRenderEngine->drawText(x, y, g_idFontMenuSmall, const_cast<char*>(szText));

   x = m_xTable;
   if ( bIsHeader )
      x += 0.008*m_sfScaleFactor;

   char szLevel[128];
   char szValue[32];
   float yTop = m_yTemp - height_text*2.0;
   float yBottom = m_yPos+m_RenderHeight - 3.0*m_sfMenuPaddingY - 1.2*height_text;
   for( int i=0; i<TX_POWER_TABLE_COLUMNS; i++ )
   {
      if ( bIsHeader )
         sprintf(szValue, "%d", piValues[i]);
      else if ( piValues[i] <= 0 )
         sprintf(szValue, "  -");
      else if ( piValues[i] >= 1000000 )
         sprintf(szValue, " !!!");
      else if ( piValues[i] < 1000 )
         sprintf(szValue, "%d mW", piValues[i]);
      else
         sprintf(szValue, "%.1f W", ((float)piValues[i])/1000.0);
      g_pRenderEngine->drawText(x, y, g_idFontMenuSmall, szValue);
      x += m_xTableCellWidth;

      if ( bIsHeader )
      if ( i == 2 || i == 5 || i == 7 || i == 10 )
      {
         g_pRenderEngine->setColors(get_Color_MenuText(), 0.5);
         g_pRenderEngine->setStrokeSize(1);
         g_pRenderEngine->drawLine(x-0.018*m_sfScaleFactor, yTop+0.02*m_sfScaleFactor, x-0.018*m_sfScaleFactor, yBottom);
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStroke(get_Color_MenuBorder());
         g_pRenderEngine->setStrokeSize(0);
      }

      if ( bIsHeader )
      {
         szLevel[0] = 0;
         if ( i == 2 )
            strcpy(szLevel, "Low Power");
         if ( i == 5 )
            strcpy(szLevel, "Normal Power");
         if ( i == 7 )
            strcpy(szLevel, "Max Power");
         if ( i == 10 )
            strcpy(szLevel, "Experimental");
         if ( 0 != szLevel[0] )
            g_pRenderEngine->drawTextLeft( x-0.026*m_sfScaleFactor, yTop+0.015*m_sfScaleFactor, g_idFontMenuSmall, szLevel);
      }
   }
      
   m_yTemp += hItem;
   if ( bIsHeader )
   {         
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawLine(m_xPos + 1.1*m_sfMenuPaddingX,m_yTemp-0.005*m_sfScaleFactor, m_xPos - 1.1*m_sfMenuPaddingX+m_RenderWidth, m_yTemp-0.005*m_sfScaleFactor);
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->setStrokeSize(0);
      m_yTemp += MENU_TEXTLINE_SPACING * g_pRenderEngine->textHeight(g_idFontMenuSmall);
   }

   if ( m_bShowThinLine )
   {
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.5);
      g_pRenderEngine->setStrokeSize(1);
      float yLine = y-0.005*m_sfScaleFactor;
      g_pRenderEngine->drawLine(m_xPos + 1.1*m_sfMenuPaddingX, yLine, m_xPos - 1.1*m_sfMenuPaddingX+m_RenderWidth, yLine);
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->setStrokeSize(0);
   }
   m_bShowThinLine = false;
   m_iLine++;

   if ( ! bIsHeader )
   if ( ! bIsBoosterLine )
   if ( pP->iShowTxBoosters )
   if ( bIsPresentOnController || bIsPresentOnVehicle )
   {
      int iTotalPowerValues[TX_POWER_TABLE_COLUMNS];
      for( int i=0; i<TX_POWER_TABLE_COLUMNS; i++ )
      {
         if ( piValues[i] <= s_iTxBoosterGainTable4W[0][0] )
            iTotalPowerValues[i] = s_iTxBoosterGainTable4W[0][1] * piValues[i] / s_iTxBoosterGainTable4W[0][0];
         if ( piValues[i] > 100 )
            iTotalPowerValues[i] = 1000000;
         else for( int k=0; k<(int)(sizeof(s_iTxBoosterGainTable4W)/sizeof(s_iTxBoosterGainTable4W[0][0])/2)-1; k++ )
         {
            if ( piValues[i] >= s_iTxBoosterGainTable4W[k][0] )
            if ( piValues[i] <= s_iTxBoosterGainTable4W[k+1][0] )
            {
               iTotalPowerValues[i] = s_iTxBoosterGainTable4W[k][1] + (s_iTxBoosterGainTable4W[k+1][1] - s_iTxBoosterGainTable4W[k][1]) * (float)((float)(piValues[i] - s_iTxBoosterGainTable4W[k][0])/(float)(s_iTxBoosterGainTable4W[k+1][0] - s_iTxBoosterGainTable4W[k][0]));
               break;
            }
         }
      }
      RenderTableLine(iCardModel, " + 4W booster", iTotalPowerValues, false, true);
   }
}

void MenuTXInfo::drawPowerLine(const char* szText, float yPos, int value)
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   float xPos = m_RenderXPos + m_sfMenuPaddingX;
   char szBuff[64];

   g_pRenderEngine->drawText( xPos, yPos, g_idFontMenuSmall, szText);

   float x = m_xTable;
   float xEnd = x;
   for( int i=0; i<TX_POWER_TABLE_COLUMNS; i++ )
   {
      if ( value <= s_iTxPowerLevelValues[i] )
         break;
      float width = m_xTableCellWidth;
      if ( (i < TX_POWER_TABLE_COLUMNS-1) && (value < s_iTxPowerLevelValues[i+1]) )
         width *= 0.5;
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawLine(x, yPos+0.6*height_text, x+width,yPos+0.6*height_text);
      xEnd = x+width;
      g_pRenderEngine->setColors(get_Color_MenuText());
      x += m_xTableCellWidth;
   }

   sprintf(szBuff, "%d", value);
   if ( value <= 0 )
      sprintf(szBuff, "N/A");
   g_pRenderEngine->drawText( xEnd+0.2*height_text, yPos, g_idFontMenuSmall, szBuff);
}


void MenuTXInfo::Render()
{
   Preferences* pP = get_Preferences();
   RenderPrepare();
   float height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   m_xTable = m_RenderXPos + m_sfMenuPaddingX;
   m_xTable += 0.15*m_sfScaleFactor;
   m_xTableCellWidth = 0.05*m_sfScaleFactor;
   
   for( int i=0; i<=m_IndexPowerMax+2; i++ )
      y += RenderItem(i,y);
   
   y += 1.0*height_text;

   m_yTopRender = y + 0.01*m_sfScaleFactor;
   m_yTemp = y+0.01*m_sfScaleFactor + 1.0*height_text;
   if ( m_bShowVehicle || m_bShowController )
      m_yTemp += 1.3 * height_text;
   if ( m_bShowController && m_bShowVehicle )
      m_yTemp += 1.3 * height_text;
   if ( m_bShowVehicle && m_bShowBothOnVehicle )
      m_yTemp += 1.4*height_text;
   if ( m_bShowController && m_bShowBothOnController )
      m_yTemp += 1.4*height_text;

   //nst int infoH[TX_POWER_TABLE_COLUMNS] =          { 10,  20,   30,   40,   50,   54,   56,   60,   63,   68,   72};

   const int info722N[TX_POWER_TABLE_COLUMNS] =       {  1,   2,    3,   10,   35,   60,   80,   90,    0,    0,    0};
   const int info722N2W[TX_POWER_TABLE_COLUMNS] =     {  6,  20,   70,  205,  650,  900, 1000, 1900, 2000, 2000, 2000};
   const int info722N4W[TX_POWER_TABLE_COLUMNS] =     { 10,  60,  200,  450, 1200, 1900, 2100, 2100, 2100, 2100, 2100};
   const int infoBlueStick[TX_POWER_TABLE_COLUMNS]  = {  2,   4,    8,   28,   80,  110,  280, 1000,   0,     0,    0};
   //nst int infoGreenStick[TX_POWER_TABLE_COLUMNS] = {  2,   5,    5,   60,   75,   75,    0,    0,    0,    0,    0};
   const int infoAWUS036NH[TX_POWER_TABLE_COLUMNS] =  { 10,  20,   30,   40,   60,    0,    0,    0,    0,    0,    0};
   const int infoAWUS036NHA[TX_POWER_TABLE_COLUMNS] = {  1,   2,    6,   17,  120,  180,  215,  310,  460,    0,    0};
   
   //const int infoH[TX_POWER_TABLE_COLUMNS] =        { 10,  20,   30,   40,   50,   54,   56,   60,   63,   68,   72};

   const int infoGeneric[TX_POWER_TABLE_COLUMNS] =    {  1,   2,    3,   10,   35,   60,   80,   90,    0,    0,    0};
   const int infoAWUS036ACH[TX_POWER_TABLE_COLUMNS] = {  2,   5,   20,   50,  160,  250,  300,  420,  500,    0,    0};
   const int infoASUSUSB56[TX_POWER_TABLE_COLUMNS]  = {  2,   9,   30,   80,  200,  250,  300,  340,  370,  370,  150};
   const int infoRTLDualAnt[TX_POWER_TABLE_COLUMNS] = {  5,  17,   50,  150,  190,  210,  261,  310,  310,  310,  310};
   const int infoAli1W[TX_POWER_TABLE_COLUMNS]  =     {  1,   2,    5,   10,   30,   50,  100,  300,  450,  450,  450};
   const int infoA6100[TX_POWER_TABLE_COLUMNS] =      {  1,   3,    9,   17,   30,   30,   35,   40,    0,    0,    0};
   const int infoAWUS036ACS[TX_POWER_TABLE_COLUMNS] = {  1,   2,    3,   10,   35,   50,   60,   90,  110,    0,    0};
   const int infoArcherT2UP[TX_POWER_TABLE_COLUMNS] = {  3,  10,   25,   55,  110,  120,  140,  150,    0,    0,    0};
   
   RenderTableLine(0, "Card / Power Level", s_iTxPowerLevelValues, true, false);
   
   m_yTemp += 0.3 * height_text;
   if ( pP->iShowOnlyPresentTxPowerCards )
      m_yTemp += height_text;

   m_iLine = 0;

   if ( m_bDisplay24Cards )
   {
      RenderTableLine(CARD_MODEL_TPLINK722N, "TPLink WN722N", info722N, false, false);
      RenderTableLine(CARD_MODEL_TPLINK722N, "WN722N + 2W Booster", info722N2W, false, false);
      RenderTableLine(CARD_MODEL_TPLINK722N, "WN722N + 4W Booster", info722N4W, false, false);
      RenderTableLine(CARD_MODEL_BLUE_STICK, "Blue Stick 2.4Ghz AR9271", infoBlueStick, false, false);
      //RenderTableLine(CARD_MODEL_TPLINK722N, "Green Stick 2.4Ghz AR9271", infoGreenStick, false, false);
      RenderTableLine(CARD_MODEL_ALFA_AWUS036NH, "Alfa AWUS036NH", infoAWUS036NH, false, false);
      RenderTableLine(CARD_MODEL_ALFA_AWUS036NHA, "Alfa AWUS036NHA", infoAWUS036NHA, false, false);
   }
   if ( m_iLine != 0 )
   if ( m_bDisplay24Cards && m_bDisplay58Cards )
      m_bShowThinLine = true;
   
   if ( m_bDisplay58Cards )
   {
      RenderTableLine(CARD_MODEL_RTL8812AU_DUAL_ANTENNA, "RTLDualAntenna 5.8Ghz", infoRTLDualAnt, false, false);
      RenderTableLine(CARD_MODEL_ASUS_AC56, "ASUS AC-56", infoASUSUSB56, false, false);
      RenderTableLine(CARD_MODEL_ALFA_AWUS036ACH, "Alfa AWUS036ACH", infoAWUS036ACH, false, false);
      RenderTableLine(CARD_MODEL_ALFA_AWUS036ACS, "Alfa AWUS036ACS", infoAWUS036ACS, false, false);
      RenderTableLine(CARD_MODEL_ZIPRAY, "1 Watt 5.8Ghz", infoAli1W, false, false);
      RenderTableLine(CARD_MODEL_NETGEAR_A6100, "Netgear A6100", infoA6100, false, false);
      RenderTableLine(CARD_MODEL_TENDA_U12, "Tenda U12", infoGeneric, false, false);
      RenderTableLine(CARD_MODEL_ARCHER_T2UPLUS, "Archer T2U Plus", infoArcherT2UP, false, false);
      RenderTableLine(CARD_MODEL_RTL8814AU, "RTL8814AU", infoGeneric, false, false);
   }

   height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);

   if ( (NULL != g_pCurrentModel) && m_bShowVehicle )
   {
      if ( m_bShowBothOnVehicle )
      {
         drawPowerLine("Vehicle TX Power (2.4 Ghz band):", m_yTopRender, g_pCurrentModel->radioInterfacesParams.txPowerAtheros );
         drawPowerLine("Vehicle TX Power (5.8 Ghz band):", m_yTopRender, g_pCurrentModel->radioInterfacesParams.txPowerRTL );
      }
      else
      {
         if ( m_bVehicleHas24Cards )
            drawPowerLine("Vehicle TX Power:", m_yTopRender, g_pCurrentModel->radioInterfacesParams.txPowerAtheros );
         else
            drawPowerLine("Vehicle TX Power:", m_yTopRender, g_pCurrentModel->radioInterfacesParams.txPowerRTL );
      }
      m_yTopRender += 1.4*height_text;   
   }

   ControllerSettings* pCS = get_ControllerSettings();

   if ( m_bShowController )
   {
      if ( m_bShowBothOnController )
      {
         drawPowerLine("Controller TX Power (2.4 Ghz band):", m_yTopRender, pCS->iTXPowerAtheros );
         m_yTopRender += 1.4*height_text;      
         drawPowerLine("Controller TX Power (5.8 Ghz band):", m_yTopRender, pCS->iTXPowerRTL );
      }
      else
      {
         if ( m_bControllerHas24Cards )
            drawPowerLine("Controller TX Power:", m_yTopRender, pCS->iTXPowerAtheros );
         else
            drawPowerLine("Controller TX Power:", m_yTopRender, pCS->iTXPowerRTL );
      }
      m_yTopRender += 1.4*height_text;   
   }

   g_pRenderEngine->drawText(m_xPos + Menu::getMenuPaddingX(), m_yPos+m_RenderHeight - 3.0*m_sfMenuPaddingY - height_text, g_idFontMenu, "* Power levels are measured at 5805 Mhz. Lower frequencies do increase the power a little bit.");
}


void MenuTXInfo::sendPowerToVehicle(int tx, int txAtheros, int txRTL)
{
   u8 buffer[10];
   memset(&(buffer[0]), 0, 10);
   buffer[0] = tx;
   buffer[1] = txAtheros;
   buffer[2] = txRTL;
   buffer[3] = g_pCurrentModel->radioInterfacesParams.txMaxPower;
   buffer[4] = g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros;
   buffer[5] = g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TX_POWERS, 0, buffer, 8) )
       valuesToUI();
}


int MenuTXInfo::onBack()
{
   if ( m_bValuesChangedController )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Restart Required","You need to restart the controller for the power changes to take effect.", 2);
      pMC->m_yPos = 0.3;
      pMC->addTopLine("");

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->rc_params.rc_enabled) )
      {
         pMC->addTopLine("Warning! You have RC link enabled. RC link will be lost while the controller restarts.");
      }
      pMC->addTopLine("Do you want to restart your controller now?");
      add_menu_to_stack(pMC);
      return 1;
   }

   if ( m_bValuesChangedVehicle )
   if ( (g_pCurrentModel != NULL) && (! g_pCurrentModel->isRunningOnOpenIPCHardware() ) )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Restart Required","You need to restart the vehicle for the power changes to take effect.", 3);
      pMC->m_yPos = 0.3;
      pMC->addTopLine("");

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->rc_params.rc_enabled) )
      {
         pMC->addTopLine("Warning! You have RC link enabled. RC link will be lost while the vehicle restarts.");
      }
      pMC->addTopLine("Do you want to restart your vehicle now?");
      add_menu_to_stack(pMC);
      return 1;
   }
   return Menu::onBack();
}

void MenuTXInfo::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( 2 == iChildMenuId/1000 )
   {
      m_bValuesChangedController = false;
      if ( 1 == returnValue )
      {
         onEventReboot();
         hw_execute_bash_command("sudo reboot -f", NULL);
         return;
      }
      menu_stack_pop(0);
      return;
   }

   if ( 3 == iChildMenuId/1000 )
   {
      m_bValuesChangedVehicle = false;
      if ( 1 == returnValue )
      {
         handle_commands_send_to_vehicle(COMMAND_ID_REBOOT, 0, NULL, 0);
         valuesToUI();
         menu_discard_all();
         return;
      }
      return;
   }

   valuesToUI();
}

void MenuTXInfo::onSelectItem()
{
   Menu::onSelectItem();
   
   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pP = get_Preferences();

   if ( m_IndexShowAllCards == m_SelectedIndex )
   {
      pP->iShowOnlyPresentTxPowerCards = 1- m_pItemsSelect[0]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      invalidate();
      return;
   }

   if ( m_IndexShowBoosters == m_SelectedIndex )
   {
      pP->iShowTxBoosters = m_pItemsSelect[1]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      invalidate();
      return;
   }

   if ( m_IndexPowerController == m_SelectedIndex )
   {
      if ( m_bControllerHas24Cards )
      {
         if ( pCS->iTXPowerAtheros != m_pItemsSlider[5]->getCurrentValue() )
            m_bValuesChangedController = true;
         pCS->iTXPowerAtheros = m_pItemsSlider[5]->getCurrentValue();
      }
      if ( m_bControllerHas58Cards )
      {
         if ( pCS->iTXPowerRTL != m_pItemsSlider[5]->getCurrentValue() )
            m_bValuesChangedController = true;
         pCS->iTXPowerRTL = m_pItemsSlider[5]->getCurrentValue();
      }
      if ( m_bControllerHas24Cards )
      {
         hardware_set_radio_tx_power_atheros(pCS->iTXPowerAtheros);
      }
      if ( m_bControllerHas58Cards )
      {
         hardware_set_radio_tx_power_rtl(pCS->iTXPowerRTL);
      }
      save_ControllerSettings();

      if ( m_pItemsSlider[5]->getCurrentValue() > 59 )
      {
         MenuConfirmation* pMC = new MenuConfirmation("High Power Levels","Setting a card to a very high power level can fry it if it does not have proper cooling.", 1, true);
         pMC->m_yPos = 0.3;
         pMC->addTopLine("");
         pMC->addTopLine("Proceed with caution!");
         add_menu_to_stack(pMC);
      }

      return;
   }

   if ( m_IndexPowerControllerAtheros == m_SelectedIndex )
   {
      if ( pCS->iTXPowerAtheros != m_pItemsSlider[6]->getCurrentValue() )
            m_bValuesChangedController = true;
      pCS->iTXPowerAtheros = m_pItemsSlider[6]->getCurrentValue();
      hardware_set_radio_tx_power_atheros(pCS->iTXPowerAtheros);
      save_ControllerSettings();

      if ( m_pItemsSlider[6]->getCurrentValue() > 59 )
      {
         MenuConfirmation* pMC = new MenuConfirmation("High Power Levels","Setting a card to a very high power level can fry it if it does not have proper cooling.", 1, true);
         pMC->m_yPos = 0.3;
         pMC->addTopLine("");
         pMC->addTopLine("Proceed with caution!");
         add_menu_to_stack(pMC);
      }
      return;
   }

   if ( m_IndexPowerControllerRTL == m_SelectedIndex )
   {
      if ( pCS->iTXPowerRTL != m_pItemsSlider[7]->getCurrentValue() )
         m_bValuesChangedController = true;
      pCS->iTXPowerRTL = m_pItemsSlider[7]->getCurrentValue();
      hardware_set_radio_tx_power_rtl(pCS->iTXPowerRTL);
      save_ControllerSettings();

      if ( m_pItemsSlider[7]->getCurrentValue() > 59 )
      {
         MenuConfirmation* pMC = new MenuConfirmation("High Power Levels","Setting a card to a very high power level can fry it if it does not have proper cooling.", 1, true);
         pMC->m_yPos = 0.3;
         pMC->addTopLine("");
         pMC->addTopLine("Proceed with caution!");
         add_menu_to_stack(pMC);
      }

      return;
   }


   if ( (m_IndexPowerVehicle == m_SelectedIndex) && menu_check_current_model_ok_for_edit() )
   {
      int val = m_pItemsSlider[0]->getCurrentValue();
      int tx = g_pCurrentModel->radioInterfacesParams.txPower;
      int txAtheros = g_pCurrentModel->radioInterfacesParams.txPowerAtheros;
      int txRTL = g_pCurrentModel->radioInterfacesParams.txPowerRTL;

      if ( m_bVehicleHas24Cards )
      {
         if ( val != txAtheros )
            m_bValuesChangedVehicle = true;
         txAtheros = val;
      }
      if ( m_bVehicleHas58Cards )
      {
         if ( val != txRTL )
            m_bValuesChangedVehicle = true;
         txRTL = val;
      }
      sendPowerToVehicle(tx, txAtheros, txRTL);

      if ( val > 59 )
      {
         MenuConfirmation* pMC = new MenuConfirmation("High Power Levels","Setting a card to a very high power level can fry it if it does not have proper cooling.", 1, true);
         pMC->m_yPos = 0.3;
         pMC->addTopLine("");
         pMC->addTopLine("Proceed with caution!");
         add_menu_to_stack(pMC);
      }

      return;
   }
   if ( (m_IndexPowerVehicleAtheros == m_SelectedIndex) && menu_check_current_model_ok_for_edit() )
   {
      int tx = g_pCurrentModel->radioInterfacesParams.txPower;
      int txAtheros = m_pItemsSlider[1]->getCurrentValue();
      if ( g_pCurrentModel->radioInterfacesParams.txPowerAtheros != txAtheros )
         m_bValuesChangedVehicle = true;

      int txRTL = g_pCurrentModel->radioInterfacesParams.txPowerRTL;
      sendPowerToVehicle(tx, txAtheros, txRTL);

      if ( txAtheros > 59 )
      {
         MenuConfirmation* pMC = new MenuConfirmation("High Power Levels","Setting a card to a very high power level can fry it if it does not have proper cooling.", 1, true);
         pMC->m_yPos = 0.3;
         pMC->addTopLine("");
         pMC->addTopLine("Proceed with caution!");
         add_menu_to_stack(pMC);
      }
      return;
   }

   if ( (m_IndexPowerVehicleRTL == m_SelectedIndex) && menu_check_current_model_ok_for_edit() )
   {
      int tx = g_pCurrentModel->radioInterfacesParams.txPower;
      int txAtheros = g_pCurrentModel->radioInterfacesParams.txPowerAtheros;
      int txRTL = m_pItemsSlider[2]->getCurrentValue();
      if ( g_pCurrentModel->radioInterfacesParams.txPowerRTL != txRTL )
         m_bValuesChangedVehicle = true;
      sendPowerToVehicle(tx, txAtheros, txRTL);

      if ( txRTL > 59 )
      {
         MenuConfirmation* pMC = new MenuConfirmation("High Power Levels","Setting a card to a very high power level can fry it if it does not have proper cooling.", 1, true);
         pMC->m_yPos = 0.3;
         pMC->addTopLine("");
         pMC->addTopLine("Proceed with caution!");
         add_menu_to_stack(pMC);
      }

      return;
   }

   if ( m_IndexPowerMax == m_SelectedIndex )
      add_menu_to_stack(new MenuTXPowerMax());

}