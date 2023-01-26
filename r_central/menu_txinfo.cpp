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
#include "../base/ctrl_settings.h"
#include "../renderer/render_engine.h"
#include "menu.h"
#include "menu_txinfo.h"
#include "menu_objects.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"
#include "menu_tx_power_max.h"

#include "colors.h"
#include "../base/ctrl_settings.h"
#include "../base/hw_procs.h"
#include "pairing.h"
#include "shared_vars.h"

MenuTXInfo::MenuTXInfo()
:Menu(MENU_ID_TXINFO, "Radio Output Power Levels", NULL)
{
   m_Width = 0.72;
   m_Height = 0.61;
   m_xPos = 0.05;
   m_yPos = 0.16;
   float fSliderWidth = 0.18;

   m_xTable = m_RenderXPos + MENU_MARGINS*menu_getScaleMenus();
   m_xTable += 0.15*menu_getScaleMenus();
   m_xTableCellWidth = 0.05*menu_getScaleMenus();

   ControllerSettings* pCS = get_ControllerSettings();

   m_bShowThinLine = false;
   m_bShowVehicle = true;
   m_bShowController = true;
   
   m_bShowBothOnController = false;
   m_bShowBothOnVehicle = false;

   m_bControllerHasAtheros = false;
   m_bControllerHasRTL = false;
   m_bVehicleHasAtheros = false;
   m_bVehicleHasRTL = false;

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

   }

MenuTXInfo::~MenuTXInfo()
{
}

void MenuTXInfo::onShow()
{
   float fSliderWidth = 0.18;
   ControllerSettings* pCS = get_ControllerSettings();
 
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
         if ( m_bVehicleHasAtheros )
            m_pItemsSlider[0] = new MenuItemSlider("Vehicle Tx Power", "Sets the radio TX power used on the vehicle. Requires a reboot of the vehicle after change.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros/2, fSliderWidth);
         else
            m_pItemsSlider[0] = new MenuItemSlider("Vehicle Tx Power", "Sets the radio TX power used on the vehicle. Requires a reboot of the vehicle after change.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL/2, fSliderWidth);
         m_IndexPowerVehicle = addMenuItem(m_pItemsSlider[0]);
      }
      else
      {
         m_pItemsSlider[1] = new MenuItemSlider("Vehicle Tx Power (2.4Ghz Atheros)", "Sets the radio TX power used on the vehicle for Atheros cards. Requires a reboot of the vehicle after change.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros, g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros/2, fSliderWidth);
         m_IndexPowerVehicleAtheros = addMenuItem(m_pItemsSlider[1]);
         m_pItemsSlider[2] = new MenuItemSlider("Vehicle Tx Power (5.8Ghz RTL)", "Sets the radio TX power used on the vehicle for RTL cards. Requires a reboot of the vehicle after change.", 1, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL, g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL/2, fSliderWidth);
         m_IndexPowerVehicleRTL = addMenuItem(m_pItemsSlider[2]);
      }
   }

   if ( m_bShowController )
   {
      if ( ! m_bShowBothOnController )
      {
         if ( m_bControllerHasAtheros )
            m_pItemsSlider[5] = new MenuItemSlider("Controller Tx Power", "Sets the radio TX power used on the controller. Requires a reboot of the controller after change.", 1, pCS->iMaxTXPowerAtheros, pCS->iMaxTXPowerAtheros/2, fSliderWidth);
         else
            m_pItemsSlider[5] = new MenuItemSlider("Controller Tx Power", "Sets the radio TX power used on the controller. Requires a reboot of the controller after change.", 1, pCS->iMaxTXPowerRTL, pCS->iMaxTXPowerRTL/2, fSliderWidth);
         m_IndexPowerController = addMenuItem(m_pItemsSlider[5]);
      }
      else
      {
         m_pItemsSlider[6] = new MenuItemSlider("Controller Tx Power (2.4Ghz Atheros)", "Sets the radio TX power used on the controller for Atheros cards. Requires a reboot of the controller after change.", 1, pCS->iMaxTXPowerAtheros, pCS->iMaxTXPowerAtheros/2, fSliderWidth);
         m_IndexPowerControllerAtheros = addMenuItem(m_pItemsSlider[6]);
         m_pItemsSlider[7] = new MenuItemSlider("Controller Tx Power (5.8Ghz RTL)", "Sets the radio TX power used on the controller for RTL cards. Requires a reboot of the controller after change.", 1, pCS->iMaxTXPowerRTL, pCS->iMaxTXPowerRTL/2, fSliderWidth);
         m_IndexPowerControllerRTL = addMenuItem(m_pItemsSlider[7]);
      }
   }
   m_IndexPowerMax = addMenuItem(new MenuItem("Limit Maximum Radio Power Levels"));
   m_pMenuItems[m_IndexPowerMax]->showArrow();

   addMenuItem( new MenuItemText("Here is a table with aproximative ouput power levels for different cards:"));

   Menu::onShow();
} 
      
void MenuTXInfo::valuesToUI()
{
   m_ExtraHeight = 0;
   ControllerSettings* pCS = get_ControllerSettings();

   if ( m_bShowController )
   {
      if ( -1 != m_IndexPowerController )
      {
         if ( m_bControllerHasAtheros )
            m_pItemsSlider[5]->setCurrentValue(pCS->iTXPowerAtheros);
         if ( m_bControllerHasRTL )
            m_pItemsSlider[5]->setCurrentValue(pCS->iTXPowerRTL);
      }
      if ( -1 != m_IndexPowerControllerAtheros && m_bControllerHasAtheros )
         m_pItemsSlider[6]->setCurrentValue(pCS->iTXPowerAtheros);
      if ( -1 != m_IndexPowerControllerRTL && m_bControllerHasRTL )
         m_pItemsSlider[7]->setCurrentValue(pCS->iTXPowerRTL);
   }

   if ( (NULL == g_pCurrentModel) || (!m_bShowVehicle) )
      return;

   if ( -1 != m_IndexPowerVehicle )
   {
      if ( m_bVehicleHasAtheros )
         m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
      if ( m_bVehicleHasRTL )
         m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerRTL);
   }
   if ( -1 != m_IndexPowerVehicleAtheros && m_bVehicleHasAtheros )
      m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
   if ( -1 != m_IndexPowerVehicleRTL && m_bVehicleHasRTL )
      m_pItemsSlider[2]->setCurrentValue(g_pCurrentModel->radioInterfacesParams.txPowerRTL);
}


void MenuTXInfo::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   valuesToUI();
   m_iConfirmationId = 0;
}


void MenuTXInfo::RenderTableLine(const char* szText, const char** szValues, bool header)
{
   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus(), g_idFontMenuSmall);

   float x = m_RenderXPos + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   float y = m_yTemp;
   g_pRenderEngine->drawText(x, y, height_text, g_idFontMenuSmall, const_cast<char*>(szText));

   x = m_xTable;
   if ( header )
      x += 0.008*menu_getScaleMenus();

   char szLevel[128];

   float yTop = m_yTemp - height_text*2.0;
   float yBottom = m_yPos+m_RenderHeight - 3.0*MENU_MARGINS*menu_getScaleMenus();
   for( int i=0; i<11; i++ )
   {
      g_pRenderEngine->drawText(x, y, height_text, g_idFontMenuSmall, const_cast<char*>(szValues[i]));
      x += m_xTableCellWidth;

      if ( (i == 2 || i == 5 || i == 7 || i == 10) && header )
      {
         g_pRenderEngine->setColors(get_Color_MenuText(), 0.5);
         g_pRenderEngine->setStrokeSize(1);
         g_pRenderEngine->drawLine(x-0.018*menu_getScaleMenus(),yTop+0.02*menu_getScaleMenus(), x-0.018*menu_getScaleMenus(), yBottom);
         g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->setStroke(get_Color_MenuBorder());
         g_pRenderEngine->setStrokeSize(0);
      }
      szLevel[0] = 0;
      if ( i == 2 )
         strcpy(szLevel, "Low Power");
      if ( i == 5 )
         strcpy(szLevel, "Normal Power");
      if ( i == 7 )
         strcpy(szLevel, "Max Power");
      if ( i == 10 )
         strcpy(szLevel, "Experimental");
      if ( header && 0 != szLevel[0] )
         g_pRenderEngine->drawTextLeft( x-0.026*menu_getScaleMenus(), yTop+0.015*menu_getScaleMenus(), height_text, g_idFontMenuSmall, szLevel);
   }
      
   m_yTemp += 1.4*menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS;
   if ( header )
   {         
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawLine(m_xPos + MENU_MARGINS*1.1*menu_getScaleMenus(),m_yTemp-0.005*menu_getScaleMenus(), m_xPos - MENU_MARGINS*1.1*menu_getScaleMenus()+m_RenderWidth, m_yTemp-0.005*menu_getScaleMenus());
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->setStrokeSize(0);
      m_yTemp += 0.2*menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS;
   }

   if ( m_bShowThinLine )
   {
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.5);
      g_pRenderEngine->setStrokeSize(1);
      float yLine = y-0.005*menu_getScaleMenus();
      g_pRenderEngine->drawLine(m_xPos + MENU_MARGINS*1.1*menu_getScaleMenus(), yLine, m_xPos - MENU_MARGINS*1.1*menu_getScaleMenus()+m_RenderWidth, yLine);
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->setStrokeSize(0);
   }
   m_bShowThinLine = false;
}

void MenuTXInfo::drawPowerLine(const char* szText, float yPos, int value)
{
   const char* szH[11] =          {    "10",    "20",    "30",     "40",     "50",     "54",     "56",       "60",     "63",    "68",       "72"};
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();   
   float xPos = m_RenderXPos + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   char szBuff[64];

   g_pRenderEngine->drawText( xPos, yPos, height_text, g_idFontMenuSmall, szText);

   float x = m_xTable;
   float xEnd = x;
   for( int i=0; i<11; i++ )
   {
      if ( value <= atoi(szH[i]) )
         break;
      float width = m_xTableCellWidth;
      if ( i<10 && value < atoi(szH[i+1]) )
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
   g_pRenderEngine->drawText( xEnd+0.2*height_text, yPos, height_text, g_idFontMenuSmall, szBuff);
}


void MenuTXInfo::Render()
{
   RenderPrepare();
   float height_text = g_pRenderEngine->textHeight(0.8*MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus(), g_idFontMenuSmall);

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   m_xTable = m_RenderXPos + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   m_xTable += 0.15*menu_getScaleMenus();
   m_xTableCellWidth = 0.05*menu_getScaleMenus();
   
   for( int i=0; i<=m_IndexPowerMax+1; i++ )
      y += RenderItem(i,y);

   m_yTopRender = y + 0.01*menu_getScaleMenus();
   m_yTemp = y+0.01*menu_getScaleMenus()+ 2.0*height_text;
   if ( m_bShowVehicle )
      m_yTemp += 1.3 * height_text;
   if ( m_bShowController )
      m_yTemp += 1.3 * height_text;
   if ( m_bShowVehicle && m_bShowBothOnVehicle )
      m_yTemp += 1.4*height_text;
   if ( m_bShowController && m_bShowBothOnController )
      m_yTemp += 1.4*height_text;

   const char* szH[11] =          {    "10",    "20",    "30",      "40",     "50",     "54",     "56",       "60",     "63",    "68",       "72"};
   //const char* sz722N[11] =     {"0.5 mW",  "2 mW",  "5 mW",   "17 mW",  "50 mW",  "80 mW", "115 mW",  " 310 mW",   "- mW",  "- mW",     "- mW"};
   const char* sz722N[11] =       {"0.3 mW",  "1 mW","2.5 mW",   "10 mW",  "35 mW",  "60 mW",  "80 mW",   " 90 mW",   "- mW",  "- mW",     "- mW"};
   const char* sz722N2W[11] =     {  "6 mW", "20 mW", "70 mW",  "205 mW", "650 mW", "900 mW",    "1 W",    "1.9 W",    "2 W",   "2 W",      "2 W"};
   const char* sz722N4W[11] =     { "10 mW", "60 mW", "200 mW", "450 mW",  "1.2 W",  "1.9 W",  "2.1 W",    "2.1 W",  "2.1 W", "2.1 W",    "2.1 W"};
   const char* szBlueStick[11]  = {  "2 mW",  "4 mW",  "8 mW",   "28 mW",  "80 mW", "110 mW", "280 mW",      "1 W",   "? mW",    "? mW",   "? mW"};
   const char* szGreenStick[11] = {  "2 mW",  "5 mW", "15 mW",   "60 mW",  "75 mW",  "75 mW",   "- mW",     "- mW",   "- mW",    "- mW",   "- mW"};
   const char* szAWUS036NH[11] =  { "10 mW", "20 mW", "30 mW",   "40 mW",  "60 mW",   "- mW",   "- mW",     "- mW",   "- mW",  "- mW",     "- mW"};
   const char* szAWUS036NHA[11] = {"0.5 mW",  "2 mW",  "6 mW",   "17 mW", "120 mW", "180 mW", "215 mW",   "310 mW", "460 mW",  "- mW",     "- mW"};
   const char* szASUSUSB56[11]  = {  "2 mW",  "9 mW", "30 mW",   "80 mW", "200 mW", "250 mW", "300 mW",   "340 mW", "370 mW",  "370 mW", "150 mW"};
   const char* szTabaoo[11]  =    {  "2 mW",  "5 mW", "20 mW",   "60 mW", "170 mW", "230 mW", "261 mW",   "310 mW", "350 mW",  "350 mW", "350 mW"};
   const char* szAli1W[11]  =     {  "1 mW",  "2 mW",  "5 mW",   "10 mW",  "30 mW", "50 mW",  "100 mW",   "300 mW", "450 mW",  "450 mW", "450 mW"};

   RenderTableLine("Card / Power Level", szH, true);
   RenderTableLine("TPLink WN722N", sz722N, false);
   RenderTableLine("WN722N + 2W Booster", sz722N2W, false);
   RenderTableLine("WN722N + 4W Booster", sz722N4W, false);
   RenderTableLine("Blue Stick 2.4Ghz AR9271", szBlueStick, false);
   RenderTableLine("Green Stick 2.4Ghz AR9271", szGreenStick, false);
   m_bShowThinLine = true;
   RenderTableLine("Alfa AWUS036NH", szAWUS036NH, false);
   RenderTableLine("Alfa AWUS036NHA", szAWUS036NHA, false);
   RenderTableLine("ASUS AC-56 USB", szASUSUSB56, false);
   RenderTableLine("Taobao 5.8Ghz", szTabaoo, false);
   RenderTableLine("1 Watt 5.8Ghz", szAli1W, false);


   height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();   
   float xPos = m_xPos + MENU_MARGINS*1.1*menu_getScaleMenus();

   if ( (NULL != g_pCurrentModel) && m_bShowVehicle )
   {
      if ( m_bShowBothOnVehicle )
      {
         drawPowerLine("Vehicle TX Power (2.4 Ghz band):", m_yTopRender, g_pCurrentModel->radioInterfacesParams.txPowerAtheros );
         drawPowerLine("Vehicle TX Power (5.8 Ghz band):", m_yTopRender, g_pCurrentModel->radioInterfacesParams.txPowerRTL );
      }
      else
      {
         if ( m_bVehicleHasAtheros )
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
         drawPowerLine("Controller TX Power:", m_yTopRender, pCS->iTXPower );
      m_yTopRender += 1.4*height_text;      
   }
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
   char szBuff[1024];

   if ( m_IndexPowerController == m_SelectedIndex )
   {
      if ( m_bControllerHasAtheros )
         pCS->iTXPowerAtheros = m_pItemsSlider[5]->getCurrentValue();
      if ( m_bControllerHasRTL )
         pCS->iTXPowerRTL = m_pItemsSlider[5]->getCurrentValue();
      if ( m_bControllerHasAtheros )
      {
         hardware_set_radio_tx_power_atheros(pCS->iTXPowerAtheros);
      }
      if ( m_bControllerHasRTL )
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


   if ( m_IndexPowerVehicle == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int val = m_pItemsSlider[0]->getCurrentValue();
      int tx = g_pCurrentModel->radioInterfacesParams.txPower;
      int txAtheros = g_pCurrentModel->radioInterfacesParams.txPowerAtheros;
      int txRTL = g_pCurrentModel->radioInterfacesParams.txPowerRTL;

      if ( m_bVehicleHasAtheros )
         txAtheros = val;
      if ( m_bVehicleHasRTL )
         txRTL = val;

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
   if ( m_IndexPowerVehicleAtheros == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int tx = g_pCurrentModel->radioInterfacesParams.txPower;
      int txAtheros = m_pItemsSlider[1]->getCurrentValue();
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

   if ( m_IndexPowerVehicleRTL == m_SelectedIndex && menu_check_current_model_ok_for_edit() )
   {
      int tx = g_pCurrentModel->radioInterfacesParams.txPower;
      int txAtheros = g_pCurrentModel->radioInterfacesParams.txPowerAtheros;
      int txRTL = m_pItemsSlider[2]->getCurrentValue();
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