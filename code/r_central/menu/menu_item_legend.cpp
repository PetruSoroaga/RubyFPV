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

#include "menu_item_legend.h"
#include "menu_objects.h"
#include "menu.h"


MenuItemLegend::MenuItemLegend(const char* szTitle, const char* szDesc, float maxWidth)
:MenuItem(szTitle, szDesc)
{
   m_bEnabled = false;
   m_bIsEditable = false;
   m_fMarginX = Menu::getMenuPaddingX();
   m_fMaxWidth = maxWidth;
   m_pszTitle = (char*)malloc(strlen(szTitle)+7);
   //strcpy(m_pszTitle, "• ");
   //strcat(m_pszTitle, szTitle);
   strcpy(m_pszTitle, szTitle);
   if ( NULL != szDesc && 0 < strlen(szDesc) )
      strcat(m_pszTitle, ":");
}

MenuItemLegend::~MenuItemLegend()
{
}

float MenuItemLegend::getItemHeight(float maxWidth)
{
   MenuItem::getItemHeight(maxWidth);
   getTitleWidth(maxWidth);
   if ( maxWidth > 0.0001 )
      getValueWidth(maxWidth-m_RenderTitleWidth-Menu::getMenuPaddingX());
   else
      getValueWidth(0);
   float h = g_pRenderEngine->getMessageHeight(m_pszTooltip, MENU_TEXTLINE_SPACING, m_RenderValueWidth, g_idFontMenu);
   if ( h > m_RenderHeight )
      m_RenderHeight = h;
   return m_RenderHeight + m_fExtraHeight;
}

float MenuItemLegend::getTitleWidth(float maxWidth)
{
   if ( m_fMaxWidth > 0.001 )
      return MenuItem::getTitleWidth(m_fMaxWidth);

   m_RenderTitleWidth = g_pRenderEngine->getMessageHeight(m_pszTitle, MENU_TEXTLINE_SPACING, maxWidth - m_fMarginX, g_idFontMenu);
   return m_RenderTitleWidth;
}

float MenuItemLegend::getValueWidth(float maxWidth)
{
   m_RenderValueWidth = maxWidth;
   return m_RenderValueWidth;
}


void MenuItemLegend::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
      
   g_pRenderEngine->setColors(get_Color_MenuText());

   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenu, m_pszTitle); 
   //g_pRenderEngine->drawMessageLines(xPos, yPos, m_pszTitle, MENU_TEXTLINE_SPACING, m_RenderTitleWidth, g_idFontMenu);

   xPos += m_RenderTitleWidth + Menu::getMenuPaddingX();

   g_pRenderEngine->drawMessageLines(xPos, yPos, m_pszTooltip, MENU_TEXTLINE_SPACING, m_RenderValueWidth, g_idFontMenu);
}
