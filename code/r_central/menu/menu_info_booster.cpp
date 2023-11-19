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

#include "menu.h"
#include "menu_info_booster.h"
#include "../osd/osd_common.h"

const char* s_szInfoBoost1 = "A new radio card has been detected.\n If you have a booster or a low noise amplifier connected to the antenna of that card please set it (the new radio card) as Uplink only or Downlink only from [Menu]->[Radio Configuration]";
const char* s_szInfoBoost1B = "A new radio card (%s) has been detected.\n If you have a booster or a low noise amplifier connected to the antenna of that card please set it (the new radio card) as Uplink only or Downlink only from [Menu]->[Radio Configuration]";
const char* s_szInfoBoost2 = "A booster should be set to UPLINK ONLY";
const char* s_szInfoBoost3 = "A LNA should be set to DOWNLINK ONLY";

MenuInfoBooster::MenuInfoBooster()
:Menu(MENU_ID_INFO_BOOSTER, "Info Booster", NULL)
{
   m_iConfirmationId = -1;
   m_xPos = 0.18; m_yPos = 0.1;
   m_Width = 0.58;
   m_Height = 0.74;
   addMenuItem(new MenuItem("Ok"));

   m_iRadioCardIndex = -1;
   m_idImg1 = g_pRenderEngine->loadIcon("res/info_booster.png");
   m_idImg2 = g_pRenderEngine->loadIcon("res/info_booster_lna.png");
}


MenuInfoBooster::~MenuInfoBooster()
{
}


void MenuInfoBooster::onShow()
{      
   Menu::onShow();
}


void MenuInfoBooster::Render()
{
   char szBuff[256];

   RenderPrepare();   
   float yTop = RenderFrameAndTitle();
   float w = m_RenderWidth-2.0*m_sfMenuPaddingX;
   float dx = 0.1*m_sfMenuPaddingX;
   RenderItem(0,m_RenderYPos+m_RenderHeight-m_sfMenuPaddingY-m_pMenuItems[0]->getItemHeight(getUsableWidth()), w*0.48);
   RenderEnd(yTop);

   float x = m_RenderXPos+m_sfMenuPaddingX;
   float y = m_RenderYPos + m_RenderHeaderHeight + m_sfMenuPaddingY;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   strcpy(szBuff, s_szInfoBoost1);
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(m_iRadioCardIndex);
   if ( NULL != pRadioInfo )
   {
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioInfo->szMAC);
      if ( NULL != pCardInfo )
      {
         char szName[128];
         strcpy(szName, str_get_radio_card_model_string(pCardInfo->cardModel) );
         if ( (0 != szName[0]) && (NULL == strstr(szName, "Generic")) )
            sprintf(szBuff, s_szInfoBoost1B, szName);
      }
   }
   float h = g_pRenderEngine->getMessageHeight(szBuff, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenu);
   g_pRenderEngine->drawMessageLines(x+dx, y, szBuff, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenu);
   g_pRenderEngine->drawIcon(x, y, dx*0.7, dx*0.7*g_pRenderEngine->getAspectRatio(), g_idIconWarning);
         
   y += h;
   y += m_sfMenuPaddingY;

   float wImg = m_RenderWidth-4.0*m_sfMenuPaddingX;
   float hImg = wImg*g_pRenderEngine->getAspectRatio()*0.23;

   g_pRenderEngine->drawIcon(x+wImg*0.1, y+hImg*0.1, wImg*0.8, hImg*0.8, m_idImg1);
   y += hImg*0.9;

   dx = m_RenderWidth*0.24;
   h = g_pRenderEngine->getMessageHeight(s_szInfoBoost2, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenuLarge);
   g_pRenderEngine->drawMessageLines(x+dx, y, s_szInfoBoost2, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenuLarge);
   y += h;

   g_pRenderEngine->drawIcon(x+wImg*0.1, y+hImg*0.1, wImg*0.8, hImg*0.8, m_idImg2);
   y += hImg*0.9;

   h = g_pRenderEngine->getMessageHeight(s_szInfoBoost3, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenuLarge);
   g_pRenderEngine->drawMessageLines(x+dx, y, s_szInfoBoost3, MENU_TEXTLINE_SPACING, getUsableWidth()-dx, g_idFontMenuLarge);
   y += h;
}

void MenuInfoBooster::onSelectItem()
{
   menu_stack_pop(1);
   return;
}