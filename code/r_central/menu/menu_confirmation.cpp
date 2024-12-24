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

#include "../../base/base.h"
#include "../../base/config.h"
#include "menu.h"
#include "menu_confirmation.h"

MenuConfirmation::MenuConfirmation(const char* szTitle, const char* szText, int id)
:Menu(MENU_ID_CONFIRMATION + id*1000, szTitle, NULL)
{
   m_bDisableStacking = true;
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;
   m_bSingleOption = false;
   m_uIconId = 0;
   if ( NULL != szText )
      addTopLine(szText);
   addMenuItem(new MenuItem("No"));
   addMenuItem(new MenuItem("Yes"));
}

MenuConfirmation::MenuConfirmation(const char* szTitle, const char* szText, int id, bool singleOption)
:Menu(MENU_ID_CONFIRMATION + id*1000, szTitle, NULL)
{
   m_bDisableStacking = true;
   m_xPos = 0.35; m_yPos = 0.35;
   m_Width = 0.3;
   m_bSingleOption = singleOption;
   if ( NULL != szText )
      addTopLine(szText);
   if ( m_bSingleOption )
   {
      addMenuItem(new MenuItem("Ok"));
      m_yPos = 0.45;
   }
   else
   {
      addMenuItem(new MenuItem("No"));
      addMenuItem(new MenuItem("Yes"));
   }
}

MenuConfirmation::~MenuConfirmation()
{
}

void MenuConfirmation::setIconId(u32 uIconId)
{
   m_uIconId = uIconId;
   invalidate();
}


void MenuConfirmation::Render()
{
   RenderPrepare();

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 3.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   if ( m_uIconId != 0 )
      m_RenderWidth += iconWidth + m_sfMenuPaddingX;
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   
   if ( m_uIconId != 0 )
      m_RenderWidth -= iconWidth + m_sfMenuPaddingX;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
  
   if ( m_uIconId != 0 )
   {
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.9);
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->drawIcon(m_RenderXPos + m_RenderWidth, m_RenderYPos + 0.5*(m_RenderHeight-iconHeight), iconWidth, iconHeight, m_uIconId);
   }
}

void MenuConfirmation::onSelectItem()
{
   log_line("Menu Confirmation: selected item: %d", m_SelectedIndex);
   if ( m_bSingleOption )
   {
      menu_stack_pop(1);
      return;
   }

   if ( 0 == m_SelectedIndex )
      menu_stack_pop(0);

   if ( 1 == m_SelectedIndex )
      menu_stack_pop(1);
}