/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

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

#include "menu_item_section.h"
#include "menu_objects.h"
#include "menu.h"


MenuItemSection::MenuItemSection(const char* title)
:MenuItem(title)
{
   m_ItemType = MENU_ITEM_TYPE_SECTION;
   m_bEnabled = false;
   m_bIsEditable = false;
   m_fMarginTop = Menu::getMenuPaddingY()*0.5;
   m_fMarginBottom = 0.4*g_pRenderEngine->textHeight(g_idFontMenu);
}

MenuItemSection::MenuItemSection(const char* title, const char* tooltip)
:MenuItem(title, tooltip)
{
   m_ItemType = MENU_ITEM_TYPE_SECTION;
   m_bEnabled = false;
   m_bIsEditable = false;
   m_fMarginTop = 0.3*g_pRenderEngine->textHeight(g_idFontMenu);
   m_fMarginBottom = 0.2*g_pRenderEngine->textHeight(g_idFontMenu);
}

MenuItemSection::~MenuItemSection()
{
}

void MenuItemSection::setEnabled(bool enabled)
{
   m_bEnabled = false;
}

float MenuItemSection::getItemHeight(float maxWidth)
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   m_RenderTitleHeight = height_text;

   m_RenderHeight = m_RenderTitleHeight + m_fMarginTop + m_fMarginBottom + m_fExtraHeight;
   return m_RenderHeight;
}

float MenuItemSection::getTitleWidth(float maxWidth)
{
   return MenuItem::getTitleWidth(maxWidth);
}

void MenuItemSection::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float dxpadding = 0.05*Menu::getMenuPaddingX();
   float dx = 0.6*Menu::getMenuPaddingX();
   float dxp2 = 0.6*Menu::getMenuPaddingX();

   g_pRenderEngine->setColors(get_Color_MenuBg());
   g_pRenderEngine->setStroke(get_Color_MenuText());
   float yMid = yPos + m_fMarginTop + 0.3*m_RenderHeight;
   g_pRenderEngine->drawLine(xPos-dxpadding, yMid, xPos+dx-g_pRenderEngine->getPixelWidth(), yMid);
   g_pRenderEngine->drawLine(xPos+dx+2*dxp2+m_RenderTitleWidth, yMid, xPos+m_pMenu->getUsableWidth(), yMid);
   if ( g_pRenderEngine->getScreenHeight() > 760 )
   {
      float dh = 1.1 * g_pRenderEngine->getPixelHeight();
      g_pRenderEngine->drawLine(xPos-dxpadding, yMid+dh, xPos+dx-g_pRenderEngine->getPixelWidth(), yMid+dh);
      g_pRenderEngine->drawLine(xPos+dx+2*dxp2+m_RenderTitleWidth, yMid+dh, xPos+m_pMenu->getUsableWidth(), yMid+dh);
   }
   g_pRenderEngine->setFill(2,2,2,0.98);
   g_pRenderEngine->drawRoundRect(xPos+dx, yMid-0.74*height_text, 2.0*dxp2+m_RenderTitleWidth, 1.4*height_text, 0.02*Menu::getMenuPaddingY());
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(0);

   g_pRenderEngine->drawText(xPos+dx+dxp2*0.7, yMid-0.5*height_text, g_idFontMenu, m_pszTitle);

   if ( ! m_bEnabled )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
   }
}
