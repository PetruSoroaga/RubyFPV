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
#include "menu_tx_power.h"
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

MenuTXPower::MenuTXPower()
:Menu(MENU_ID_TXINFO, "Radio Output Power Levels", NULL)
{
   m_Width = 0.72;
   m_Height = 0.64;
   m_xPos = 0.05;
   m_yPos = 0.16;

   m_xTable = m_RenderXPos + m_sfMenuPaddingY;
   m_xTable += 0.15*m_sfScaleFactor;
   m_xTableCellWidth = 0.05*m_sfScaleFactor;

   m_bShowThinLine = false;
   
   m_bShowVehicle = true;
   m_bShowController = true;
   
   m_bValuesChangedVehicle = false;
   m_bValuesChangedController = false;

   m_IndexPowerVehicleRTL8812AU = -1;
   m_IndexPowerVehicleRTL8812EU = -1;
   m_IndexPowerVehicleAtheros = -1;
   m_IndexPowerControllerRTL8812AU = -1;
   m_IndexPowerControllerRTL8812EU = -1;
   m_IndexPowerControllerAtheros = -1;
}

MenuTXPower::~MenuTXPower()
{
}

void MenuTXPower::onShow()
{
   float fSliderWidth = 0.18;
   ControllerSettings* pCS = get_ControllerSettings();

   removeAllItems();

   int iIndexSlider = 0;
   if ( m_bShowVehicle && (NULL != g_pCurrentModel) )
   { 
      if ( g_pCurrentModel->hasRadioCardsRTL8812AU() )
      {
         m_pItemsSlider[iIndexSlider] = new MenuItemSlider("Vehicle Tx Power (RTL8812AU)", "Sets the radio TX power used on the vehicle for RTL8812AU cards.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812AU, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812AU/2, fSliderWidth);
         m_IndexPowerVehicleRTL8812AU = addMenuItem(m_pItemsSlider[iIndexSlider]);
         iIndexSlider++;
      }
      if ( g_pCurrentModel->hasRadioCardsRTL8812EU() )
      {
         m_pItemsSlider[iIndexSlider] = new MenuItemSlider("Vehicle Tx Power (RTL8812EU)", "Sets the radio TX power used on the vehicle for RTL8812EU cards.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812EU, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812EU/2, fSliderWidth);
         m_IndexPowerVehicleRTL8812EU = addMenuItem(m_pItemsSlider[iIndexSlider]);
         iIndexSlider++;
      }
      if ( g_pCurrentModel->hasRadioCardsAtheros() )
      {
         m_pItemsSlider[iIndexSlider] = new MenuItemSlider("Vehicle Tx Power (Atheros)", "Sets the radio TX power used on the vehicle for Atheros cards.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros/2, fSliderWidth);
         m_IndexPowerVehicleAtheros = addMenuItem(m_pItemsSlider[iIndexSlider]);
         iIndexSlider++;
      }
   }

   if ( m_bShowController )
   {
      if ( hardware_radio_has_rtl8812au_cards() )
      {
         m_pItemsSlider[iIndexSlider] = new MenuItemSlider("Controller Tx Power (RTL8812AU)", "Sets the radio TX power used on controller for RTL8812AU cards.", 1, pCS->iMaxTXPowerRTL8812AU, pCS->iMaxTXPowerRTL8812AU/2, fSliderWidth);
         m_IndexPowerControllerRTL8812AU = addMenuItem(m_pItemsSlider[iIndexSlider]);
         iIndexSlider++;
      }
      if ( hardware_radio_has_rtl8812eu_cards() )
      {
         m_pItemsSlider[iIndexSlider] = new MenuItemSlider("Controller Tx Power (RTL8812EU)", "Sets the radio TX power used on controller for RTL8812EU cards.", 1, pCS->iMaxTXPowerRTL8812EU, pCS->iMaxTXPowerRTL8812EU/2, fSliderWidth);
         m_IndexPowerControllerRTL8812EU = addMenuItem(m_pItemsSlider[iIndexSlider]);
         iIndexSlider++;
      }
      if ( hardware_radio_has_atheros_cards() )
      {
         m_pItemsSlider[iIndexSlider] = new MenuItemSlider("Controller Tx Power (Atheros)", "Sets the radio TX power used on controller for Atheros cards.", 1, pCS->iMaxTXPowerAtheros, pCS->iMaxTXPowerAtheros/2, fSliderWidth);
         m_IndexPowerControllerAtheros = addMenuItem(m_pItemsSlider[iIndexSlider]);
         iIndexSlider++;
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

   Menu::onShow();
} 
      
void MenuTXPower::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pP = get_Preferences();

   m_pItemsSelect[0]->setSelectedIndex(1 - pP->iShowOnlyPresentTxPowerCards);
   m_pItemsSelect[1]->setSelectedIndex(pP->iShowTxBoosters);


   int iIndexSlider = 0;
   if ( m_bShowVehicle && (NULL != g_pCurrentModel) )
   { 
      if ( g_pCurrentModel->hasRadioCardsRTL8812AU() )
      {
         m_pItemsSlider[iIndexSlider]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerRTL8812AU);
         iIndexSlider++;
      }
      if ( g_pCurrentModel->hasRadioCardsRTL8812EU() )
      {
         m_pItemsSlider[iIndexSlider]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerRTL8812EU);
         iIndexSlider++;
      }
      if ( g_pCurrentModel->hasRadioCardsAtheros() )
      {
         m_pItemsSlider[iIndexSlider]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
         iIndexSlider++;
      }
   }

   if ( m_bShowController )
   {
      if ( hardware_radio_has_rtl8812au_cards() )
      {
         m_pItemsSlider[iIndexSlider]->setCurrentValue(pCS->iTXPowerRTL8812AU);
         iIndexSlider++;
      }
      if ( hardware_radio_has_rtl8812eu_cards() )
      {
         m_pItemsSlider[iIndexSlider]->setCurrentValue(pCS->iTXPowerRTL8812EU);
         iIndexSlider++;
      }
      if ( hardware_radio_has_atheros_cards() )
      {
         m_pItemsSlider[iIndexSlider]->setCurrentValue(pCS->iTXPowerAtheros);
         iIndexSlider++;
      }
   }
}


void MenuTXPower::RenderTableLine(int iCardModel, const char* szText, const int* piValues, bool bIsHeader, bool bIsBoosterLine)
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
      //g_pRenderEngine->setColors(get_Color_MenuText(), 0.2);
      //g_pRenderEngine->setFill(250,200,50,0.1);
      double pColor[4];
      memcpy(pColor,get_Color_MenuBgTooltip(), 4*sizeof(double));
      pColor[0] -= 20;
      pColor[1] -= 20;
      pColor[2] -= 20;
      g_pRenderEngine->setFill(pColor);
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->setStrokeSize(1);
      
      float h = hItem + 0.4 * m_sfMenuPaddingY;
      if ( pP->iShowTxBoosters )
      if ( ! bIsBoosterLine )
      if ( bIsPresentOnController || bIsPresentOnVehicle )
         h += hItem;
      g_pRenderEngine->drawRoundRect(x - m_sfMenuPaddingX*0.4, y - m_sfMenuPaddingY*0.2, m_RenderWidth - 2.0*m_sfMenuPaddingX + 0.8 * m_sfMenuPaddingX, h, MENU_ROUND_MARGIN * m_sfMenuPaddingY);

      g_pRenderEngine->setColors(get_Color_MenuText());
      //g_pRenderEngine->setStroke(get_Color_MenuBorder());
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

void MenuTXPower::drawPowerLine(const char* szText, float yPos, int value)
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


void MenuTXPower::Render()
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

   m_yTopRender = y - 0.01*m_sfScaleFactor;
   m_yTemp = y+0.01*m_sfScaleFactor + 1.0*height_text;
   m_yTemp += 1.3 * height_text * m_IndexPowerMax;

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

   const int infoRTL8812EU[TX_POWER_TABLE_COLUMNS] = {  3,  10,   25,   55,  110,  220,  340,  500,    0,    0,    0};
   
   RenderTableLine(0, "Card / Power Level", s_iTxPowerLevelValues, true, false);
   
   m_yTemp += 0.3 * height_text;
   if ( pP->iShowOnlyPresentTxPowerCards )
      m_yTemp += height_text;

   m_iLine = 0;

   bool bShowAtherosCards = false;
   if ( m_bShowController && hardware_radio_has_atheros_cards() )
      bShowAtherosCards = true;
   if ( m_bShowVehicle && (NULL != g_pCurrentModel) && g_pCurrentModel->hasRadioCardsAtheros() )
      bShowAtherosCards = true;

   bool bShowRTL8812AUCards = false;
   if ( m_bShowController && hardware_radio_has_rtl8812au_cards() )
      bShowRTL8812AUCards = true;
   if ( m_bShowVehicle && (NULL != g_pCurrentModel) && g_pCurrentModel->hasRadioCardsRTL8812AU() )
      bShowRTL8812AUCards = true;

   bool bShowRTL8812EUCards = false;
   if ( m_bShowController && hardware_radio_has_rtl8812eu_cards() )
      bShowRTL8812EUCards = true;
   if ( m_bShowVehicle && (NULL != g_pCurrentModel) && g_pCurrentModel->hasRadioCardsRTL8812EU() )
      bShowRTL8812EUCards = true;

   if ( bShowAtherosCards )
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
   if ( (bShowRTL8812AUCards || bShowRTL8812EUCards) && bShowAtherosCards )
      m_bShowThinLine = true;
   
   if ( bShowRTL8812AUCards )
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

   if ( bShowRTL8812EUCards )
   {
      RenderTableLine(CARD_MODEL_BLUE_8812EU, "RTL8812EU Blue", infoRTL8812EU, false, false);
   }

   height_text = g_pRenderEngine->textHeight(g_idFontMenuSmall);

   if ( m_bShowVehicle && (NULL != g_pCurrentModel) )
   {
      if ( g_pCurrentModel->hasRadioCardsRTL8812AU() )
      {
         drawPowerLine("Vehicle TX Power (RTL8812AU):", m_yTopRender, g_pCurrentModel->radioInterfacesParams.txPowerRTL8812AU);
         m_yTopRender += 1.4*height_text;
      }
      if ( g_pCurrentModel->hasRadioCardsRTL8812EU() )
      {
         drawPowerLine("Vehicle TX Power (RTL8812EU):", m_yTopRender, g_pCurrentModel->radioInterfacesParams.txPowerRTL8812EU);
         m_yTopRender += 1.4*height_text;
      }
      if ( g_pCurrentModel->hasRadioCardsAtheros() )
      {
         drawPowerLine("Vehicle TX Power (Atheros):", m_yTopRender, g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
         m_yTopRender += 1.4*height_text;
      }
   }

   ControllerSettings* pCS = get_ControllerSettings();

   if ( m_bShowController )
   {
      if ( hardware_radio_has_rtl8812au_cards() )
      {
         drawPowerLine("Controller TX Power (RTL8812AU):", m_yTopRender, pCS->iTXPowerRTL8812AU);
         m_yTopRender += 1.4*height_text;
      }
      if ( hardware_radio_has_rtl8812eu_cards() )
      {
         drawPowerLine("Controller TX Power (RTL8812EU):", m_yTopRender, pCS->iTXPowerRTL8812EU);
         m_yTopRender += 1.4*height_text;
      }
      if ( hardware_radio_has_atheros_cards() )
      {
         drawPowerLine("Controller TX Power (Atheros):", m_yTopRender, pCS->iTXPowerAtheros);
         m_yTopRender += 1.4*height_text;
      }
   }

   g_pRenderEngine->drawText(m_xPos + Menu::getMenuPaddingX(), m_yTemp + 0.5*m_sfMenuPaddingY, g_idFontMenu, "* Power levels are measured at 5805 Mhz. Lower frequencies do increase the power a little bit.");
}


void MenuTXPower::sendPowerToVehicle(int txRTL8812AU, int txRTL8812EU, int txAtheros)
{
   u8 buffer[10];
   memset(&(buffer[0]), 0, 10);
   buffer[0] = txRTL8812AU;
   buffer[1] = txRTL8812EU;
   buffer[2] = txAtheros;
   buffer[3] = g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812AU;
   buffer[4] = g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL8812EU;
   buffer[5] = g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_TX_POWERS, 0, buffer, 8) )
       valuesToUI();
}


int MenuTXPower::onBack()
{
   if ( m_bValuesChangedController )
   {
      #if defined(HW_PLATFORM_RASPBERRY)
      
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
      #endif
   }

   if ( m_bValuesChangedVehicle )
   if ( (g_pCurrentModel != NULL) && (! g_pCurrentModel->isRunningOnOpenIPCHardware() ) )
   {
      char szTextW[256];
      sprintf(szTextW, "You need to restart the %s for the power changes to take effect.", g_pCurrentModel->getVehicleTypeString());
      MenuConfirmation* pMC = new MenuConfirmation("Restart Required",szTextW, 3);
      pMC->m_yPos = 0.3;
      pMC->addTopLine("");

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->rc_params.rc_enabled) )
      {
         pMC->addTopLine("Warning! You have RC link enabled. RC link will be lost while the vehicle restarts.");
      }
      sprintf(szTextW, "Do you want to restart your %s now?", g_pCurrentModel->getVehicleTypeString());
      pMC->addTopLine(szTextW);
      add_menu_to_stack(pMC);
      return 1;
   }
   return Menu::onBack();
}

void MenuTXPower::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( 2 == iChildMenuId/1000 )
   {
      m_bValuesChangedController = false;
      if ( 1 == returnValue )
      {
         onEventReboot();
         hardware_reboot();
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

void MenuTXPower::onSelectItem()
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

   if ( m_IndexPowerControllerRTL8812AU == m_SelectedIndex )
   {
      if ( pCS->iTXPowerRTL8812AU != m_pItemsSlider[m_SelectedIndex]->getCurrentValue() )
         m_bValuesChangedController = true;
      pCS->iTXPowerRTL8812AU = m_pItemsSlider[m_SelectedIndex]->getCurrentValue();
      save_ControllerSettings();
      hardware_radio_set_txpower_rtl8812au(pCS->iTXPowerRTL8812AU);
    
      if ( m_pItemsSlider[m_SelectedIndex]->getCurrentValue() > 59 )
      {
         MenuConfirmation* pMC = new MenuConfirmation("High Power Levels","Setting a card to a very high power level can fry it if it does not have proper cooling.", 1, true);
         pMC->m_yPos = 0.3;
         pMC->addTopLine("");
         pMC->addTopLine("Proceed with caution!");
         add_menu_to_stack(pMC);
      }
      return;
   }

   if ( m_IndexPowerControllerRTL8812EU == m_SelectedIndex )
   {
      if ( pCS->iTXPowerRTL8812EU != m_pItemsSlider[m_SelectedIndex]->getCurrentValue() )
         m_bValuesChangedController = true;
      pCS->iTXPowerRTL8812EU = m_pItemsSlider[m_SelectedIndex]->getCurrentValue();
      save_ControllerSettings();
      hardware_radio_set_txpower_rtl8812eu(pCS->iTXPowerRTL8812EU);
    
      if ( m_pItemsSlider[m_SelectedIndex]->getCurrentValue() > 59 )
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
      if ( pCS->iTXPowerAtheros != m_pItemsSlider[m_SelectedIndex]->getCurrentValue() )
         m_bValuesChangedController = true;
      pCS->iTXPowerAtheros = m_pItemsSlider[m_SelectedIndex]->getCurrentValue();
      save_ControllerSettings();
      hardware_radio_set_txpower_atheros(pCS->iTXPowerAtheros);
    
      if ( m_pItemsSlider[m_SelectedIndex]->getCurrentValue() > 59 )
      {
         MenuConfirmation* pMC = new MenuConfirmation("High Power Levels","Setting a card to a very high power level can fry it if it does not have proper cooling.", 1, true);
         pMC->m_yPos = 0.3;
         pMC->addTopLine("");
         pMC->addTopLine("Proceed with caution!");
         add_menu_to_stack(pMC);
      }
      return;
   }

   if ( (m_IndexPowerVehicleRTL8812AU == m_SelectedIndex) && menu_check_current_model_ok_for_edit() )
   {
      int val = m_pItemsSlider[m_SelectedIndex]->getCurrentValue();
      if ( g_pCurrentModel->radioInterfacesParams.txPowerRTL8812AU != val )
         m_bValuesChangedVehicle = true;
      
      sendPowerToVehicle(val, g_pCurrentModel->radioInterfacesParams.txPowerRTL8812EU, g_pCurrentModel->radioInterfacesParams.txPowerAtheros);

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

   if ( (m_IndexPowerVehicleRTL8812EU == m_SelectedIndex) && menu_check_current_model_ok_for_edit() )
   {
      int val = m_pItemsSlider[m_SelectedIndex]->getCurrentValue();
      if ( g_pCurrentModel->radioInterfacesParams.txPowerRTL8812EU != val )
         m_bValuesChangedVehicle = true;
      
      sendPowerToVehicle(g_pCurrentModel->radioInterfacesParams.txPowerRTL8812AU, val, g_pCurrentModel->radioInterfacesParams.txPowerAtheros);

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
      int val = m_pItemsSlider[m_SelectedIndex]->getCurrentValue();
      if ( g_pCurrentModel->radioInterfacesParams.txPowerAtheros != val )
         m_bValuesChangedVehicle = true;
      
      sendPowerToVehicle(g_pCurrentModel->radioInterfacesParams.txPowerRTL8812AU, g_pCurrentModel->radioInterfacesParams.txPowerRTL8812EU, val);

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

   if ( m_IndexPowerMax == m_SelectedIndex )
   {
      MenuTXPowerMax* pMenu = new MenuTXPowerMax();
      pMenu->m_bShowVehicle = m_bShowVehicle;
      pMenu->m_bShowController = m_bShowController;
      add_menu_to_stack(pMenu);
   }
}
