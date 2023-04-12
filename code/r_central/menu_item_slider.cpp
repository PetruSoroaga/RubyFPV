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
#include "menu_item_slider.h"
#include "menu_objects.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"


MenuItemSlider::MenuItemSlider(const char* title, int min, int max, int mid, float sliderWidth)
:MenuItem(title)
{
   m_ValueMin = min;
   m_ValueMax = max;
   m_ValueMid = mid;
   m_ValueCurrent = mid;
   m_SliderWidth = sliderWidth;
   m_bIsEditable = true;
   m_Step = 1;
   m_HalfStepsEnabled = false;
}

MenuItemSlider::MenuItemSlider(const char* title, const char* tooltip, int min, int max, int mid, float sliderWidth)
:MenuItem(title, tooltip)
{
   m_ValueMin = min;
   m_ValueMax = max;
   m_ValueMid = mid;
   m_ValueCurrent = mid;
   m_SliderWidth = sliderWidth;
   m_bIsEditable = true;
   m_Step = 1;
   m_HalfStepsEnabled = false;
}

MenuItemSlider::~MenuItemSlider()
{
}

void MenuItemSlider::enableHalfSteps()
{
   m_HalfStepsEnabled = true;
}

void MenuItemSlider::setStep(int step)
{
   m_Step = step;
}

int MenuItemSlider::getCurrentValue()
{
   return m_ValueCurrent;
}

void MenuItemSlider::setCurrentValue(int val)
{
   m_ValueCurrent = val;
   if ( m_ValueCurrent < m_ValueMin )
      m_ValueCurrent = m_ValueMin;
   if ( m_ValueCurrent > m_ValueMax )
      m_ValueCurrent = m_ValueMax;
}

void MenuItemSlider::setMaxValue(int val)
{
   m_ValueMax = val;
}

void MenuItemSlider::beginEdit()
{
   m_ValuePrev = m_ValueCurrent;
   MenuItem::beginEdit();
}

void MenuItemSlider::endEdit(bool bCanceled)
{
   if ( bCanceled )
      m_ValueCurrent = m_ValuePrev;
   MenuItem::endEdit(bCanceled);
}


void MenuItemSlider::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   RenderBaseTitle(xPos, yPos, bSelected, fWidthSelection);

   char szBuff[32];
   sprintf(szBuff, "%d", m_ValueCurrent);
   if ( m_HalfStepsEnabled )
      sprintf(szBuff, "%.2f", m_ValueCurrent/4.0);
   float valueWidth = 0.03*menu_getScaleMenus();

   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);

   float padding = MENU_SELECTION_PADDING*menu_getScaleMenus();
   float paddingH = padding/g_pRenderEngine->getAspectRatio();

   float sliderHeight = 0.4 * m_RenderTitleHeight;
   float sliderWidth = m_SliderWidth*menu_getScaleMenus();
   if ( fWidthSelection + sliderWidth + valueWidth + 3*paddingH > m_pMenu->getUsableWidth() )
      sliderWidth = m_pMenu->getUsableWidth() - fWidthSelection - valueWidth - 3*paddingH;

   float xPosSlider = xPos + m_pMenu->getUsableWidth() - sliderWidth;

   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_ItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPosSlider-valueWidth-paddingH*2, yPos-0.8*padding, sliderWidth + valueWidth + 4.0*paddingH, m_RenderHeight + 2.0*padding, 0.01*menu_getScaleMenus());
   }

   float fA = g_pRenderEngine->getGlobalAlfa();
   if (! m_bEnabled )
      g_pRenderEngine->setGlobalAlfa(0.3);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(0);

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_ItemSelectedText());
   else
   {
      if ( m_bCustomTextColor )
         g_pRenderEngine->setColors(&m_TextColor[0]);
      else
         g_pRenderEngine->setColors(get_Color_MenuText());
   }
   if ( ! m_bEnabled )
      g_pRenderEngine->setColors(get_Color_ItemDisabledText());

   g_pRenderEngine->drawTextLeft(xPosSlider-1.5*padding, yPos-0.002*menu_getScaleMenus(), height_text, g_idFontMenu, szBuff);
   
   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_ItemSelectedText(), 0.2);
   else
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.2);
   g_pRenderEngine->setStrokeSize(1);

   g_pRenderEngine->drawRoundRect(xPosSlider, yPos+m_RenderHeight-1.3*sliderHeight, sliderWidth, sliderHeight, 0.002*menu_getScaleMenus());

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_ItemSelectedText());
   else
      g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(1);

   float yBottom = yPos + m_RenderHeight - 0.9*sliderHeight;
   g_pRenderEngine->drawLine(xPosSlider, yBottom, xPosSlider, yBottom-1.6*sliderHeight);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth, yBottom, xPosSlider+sliderWidth, yBottom-1.6*sliderHeight);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth/2, yBottom, xPosSlider+sliderWidth/2, yBottom-1.6*sliderHeight);

   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.1, yBottom, xPosSlider+sliderWidth*0.1, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.2, yBottom, xPosSlider+sliderWidth*0.2, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.3, yBottom, xPosSlider+sliderWidth*0.3, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.4, yBottom, xPosSlider+sliderWidth*0.4, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.6, yBottom, xPosSlider+sliderWidth*0.6, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.7, yBottom, xPosSlider+sliderWidth*0.7, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.8, yBottom, xPosSlider+sliderWidth*0.8, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.9, yBottom, xPosSlider+sliderWidth*0.9, yBottom-sliderHeight*0.8);

   float size = sliderHeight*1.8;
   xPosSlider = xPosSlider + (float)(sliderWidth-size)*(m_ValueCurrent-m_ValueMin)/(float)(m_ValueMax-m_ValueMin);
   g_pRenderEngine->drawRoundRect(xPosSlider, yBottom-size*0.6, size, size, 0.002*menu_getScaleMenus());

   g_pRenderEngine->setGlobalAlfa(fA);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->setStrokeSize(0);
}

void MenuItemSlider::onKeyUp(bool bIgnoreReversion)
{
   setCurrentValue(m_ValueCurrent-m_Step);
}

void MenuItemSlider::onKeyDown(bool bIgnoreReversion)
{
   setCurrentValue(m_ValueCurrent+m_Step);
}

void MenuItemSlider::onKeyLeft(bool bIgnoreReversion)
{
   setCurrentValue(m_ValueCurrent-m_Step);
}

void MenuItemSlider::onKeyRight(bool bIgnoreReversion)
{
   setCurrentValue(m_ValueCurrent+m_Step);
}

