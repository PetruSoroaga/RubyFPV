/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
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
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "menu_item_slider.h"
#include "menu_objects.h"
#include "menu.h"


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

   if ( ! m_HalfStepsEnabled )
   if ( m_Step > 1 )
   {
      if ( ((m_ValueCurrent-m_ValueMin) % m_Step) != 0 )
         m_ValueCurrent = m_ValueMin + m_Step * ((int)((m_ValueCurrent - m_ValueMin)/m_Step));
   }
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

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float valueWidth = g_pRenderEngine->textWidth(g_idFontMenu, "AAA");
   float valueMargin = 0.4*height_text;
   float paddingV = Menu::getSelectionPaddingY();
   float paddingH = Menu::getSelectionPaddingX();

   float sliderHeight = 0.4 * m_RenderTitleHeight;
   float sliderWidth = m_SliderWidth * Menu::getScaleFactor();
   float fMinSliderWidth = 0.05;
   if ( sliderWidth < fMinSliderWidth )
      sliderWidth = fMinSliderWidth;

   if ( fWidthSelection + sliderWidth + valueWidth + valueMargin + Menu::getMenuPaddingX() + m_fMarginX > m_pMenu->getUsableWidth() )
   {
      sliderWidth = m_pMenu->getUsableWidth() - m_fMarginX - fWidthSelection - valueWidth - valueMargin - Menu::getMenuPaddingX();
      if ( sliderWidth < fMinSliderWidth )
         sliderWidth = fMinSliderWidth;
   }
   float fSizeSelectorH = m_RenderTitleHeight*0.8;
   float fSizeSelectorW = fSizeSelectorH*0.4;

   float xPosSlider = xPos + m_pMenu->getUsableWidth() - sliderWidth - m_fMarginX;
   float xPosSelector = xPosSlider + (float)(sliderWidth-fSizeSelectorW)*(m_ValueCurrent-m_ValueMin)/(float)(m_ValueMax-m_ValueMin);

   float yTop = yPos + paddingV;
   float yBottom = yPos + m_RenderHeight - paddingV;
   float yMid = yPos + 0.5*m_RenderHeight;

   if (! m_bEnabled )
   {
      g_pRenderEngine->setColors(get_Color_MenuItemDisabledText());
   }
   else
   {
      if ( m_bIsEditing )
      {
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedBg());
         g_pRenderEngine->drawRoundRect(xPosSlider-valueWidth-valueMargin-paddingH, yPos-paddingV, sliderWidth + valueWidth + valueMargin + 2.0*paddingH, m_RenderHeight + 2.0*paddingV, 0.1*Menu::getMenuPaddingY());
      }
      if ( m_bIsEditing )
         g_pRenderEngine->setColors(get_Color_MenuItemSelectedText());
      else
      {
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
      }
   }

   g_pRenderEngine->setStrokeSize(0);

   g_pRenderEngine->drawTextLeft(xPosSlider-valueMargin, yPos, g_idFontMenu, szBuff);
   
   g_pRenderEngine->setStrokeSize(1);
   g_pRenderEngine->drawRoundRect(xPosSelector, yTop, fSizeSelectorW, fSizeSelectorH, 0.1*Menu::getMenuPaddingY());

   g_pRenderEngine->setFill(0,0,0,0);
   g_pRenderEngine->drawRoundRect(xPosSlider, yMid-sliderHeight*0.5, sliderWidth, sliderHeight, 0.1*Menu::getMenuPaddingY());

   //g_pRenderEngine->drawLine(xPosSlider, yTop, xPosSlider, yBottom);
   //g_pRenderEngine->drawLine(xPosSlider+sliderWidth, yTop, xPosSlider+sliderWidth, yBottom);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth/2, yTop, xPosSlider+sliderWidth/2, yBottom);

   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.1, yMid - sliderHeight*0.5, xPosSlider+sliderWidth*0.1, yMid + sliderHeight*0.5);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.2, yMid - sliderHeight*0.5, xPosSlider+sliderWidth*0.2, yMid + sliderHeight*0.5);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.3, yMid - sliderHeight*0.5, xPosSlider+sliderWidth*0.3, yMid + sliderHeight*0.5);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.4, yMid - sliderHeight*0.5, xPosSlider+sliderWidth*0.4, yMid + sliderHeight*0.5);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.6, yMid - sliderHeight*0.5, xPosSlider+sliderWidth*0.6, yMid + sliderHeight*0.5);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.7, yMid - sliderHeight*0.5, xPosSlider+sliderWidth*0.7, yMid + sliderHeight*0.5);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.8, yMid - sliderHeight*0.5, xPosSlider+sliderWidth*0.8, yMid + sliderHeight*0.5);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.9, yMid - sliderHeight*0.5, xPosSlider+sliderWidth*0.9, yMid + sliderHeight*0.5);

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->setStrokeSize(1);
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

