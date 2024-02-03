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

#include "menu_item_radio.h"
#include "menu_objects.h"
#include "menu.h"

MenuItemRadio::MenuItemRadio(const char* title)
:MenuItem(title)
{
   m_nSelectionsCount = 0;
   m_nSelectedIndex = -1;
   m_nFocusedIndex = -1;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   m_fSelectorWidth = 0.9*height_text;
   m_fLegendDx = 0.5 * Menu::getMenuPaddingX();
   m_bSmallLegend = false;
}

MenuItemRadio::MenuItemRadio(const char* title, const char* tooltip)
:MenuItem(title, tooltip)
{
   m_nSelectionsCount = 0;
   m_nSelectedIndex = -1;
   m_nFocusedIndex = -1;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   m_fSelectorWidth = 0.9*height_text;
   m_fLegendDx = 0.5 * Menu::getMenuPaddingX();
   m_bSmallLegend = false;
}

MenuItemRadio::~MenuItemRadio()
{
   removeAllSelections();
}

void MenuItemRadio::removeAllSelections()
{
   for( int i=0; i<m_nSelectionsCount; i++ )
   {
      free( m_szSelections[i] );
      if ( NULL != m_szSelectionsLegend[i] )
         free( m_szSelectionsLegend[i] );
   }
   m_nSelectionsCount = 0;
   m_nSelectedIndex = -1;
   m_nFocusedIndex = -1;
}

void MenuItemRadio::addSelection(const char* szText)
{
   if ( NULL == szText || m_nSelectionsCount >= MAX_MENU_ITEM_RADIO_SELECTIONS - 1 )
      return;

   if ( -1 == m_nSelectedIndex )
      m_nSelectedIndex = 0;

   if ( -1 == m_nFocusedIndex )
      m_nFocusedIndex = 0;

   m_szSelections[m_nSelectionsCount] = (char*)malloc(strlen(szText)+1);
   strcpy(m_szSelections[m_nSelectionsCount], szText);
   m_szSelectionsLegend[m_nSelectionsCount] = NULL;
   m_bEnabledSelections[m_nSelectionsCount] = true;
   m_nSelectionsCount++;
}

void MenuItemRadio::addSelection(const char* szText, const char* szLegend)
{
   if ( NULL == szText || m_nSelectionsCount >= MAX_MENU_ITEM_RADIO_SELECTIONS - 1 )
      return;

   if ( -1 == m_nSelectedIndex )
      m_nSelectedIndex = 0;

   if ( -1 == m_nFocusedIndex )
      m_nFocusedIndex = 0;

   m_szSelections[m_nSelectionsCount] = (char*)malloc(strlen(szText)+1);
   strcpy(m_szSelections[m_nSelectionsCount], szText);
   m_szSelectionsLegend[m_nSelectionsCount] = NULL;
   if ( NULL != szLegend && 0 != szLegend[0] )
   {
      bool bAddSeparator = false;
      int len = strlen(szLegend)+1;
      if ( szLegend[len-2] != '.' && szLegend[len-2] != ';' )
      {
         len++;
         bAddSeparator = true;
      }
      m_szSelectionsLegend[m_nSelectionsCount] = (char*)malloc(len);
      strcpy(m_szSelectionsLegend[m_nSelectionsCount], szLegend);
      if ( bAddSeparator )
         strcat(m_szSelectionsLegend[m_nSelectionsCount], ".");
   }
   m_bEnabledSelections[m_nSelectionsCount] = true;
   m_nSelectionsCount++;
}

void MenuItemRadio::setSelectedIndex(int index)
{
   if ( index >= 0 && index < m_nSelectionsCount )
      m_nSelectedIndex = index;
   else
      m_nSelectedIndex = 0;
}


void MenuItemRadio::enableSelectionIndex(int index, bool bEnable)
{
   if ( index >= 0 && index < m_nSelectionsCount )
   {
      if ( m_nFocusedIndex == index && (!bEnable) )
         setNextAvailableFocusableItem();
      if ( -1 == m_nFocusedIndex && bEnable )
         m_nFocusedIndex = index;

      m_bEnabledSelections[index] = bEnable;
   }
}

void MenuItemRadio::setFocusedIndex(int index)
{
   if ( index >= 0 && index < m_nSelectionsCount )
   {
      m_nFocusedIndex = index;
      if ( ! m_bEnabledSelections[m_nFocusedIndex] )
         setNextAvailableFocusableItem();
   }
}

int MenuItemRadio::getFocusedIndex()
{
   return m_nFocusedIndex;
}

int MenuItemRadio::getSelectedIndex()
{
   return m_nSelectedIndex;
}

int MenuItemRadio::getSelectionsCount()
{
   return m_nSelectionsCount;
}

void MenuItemRadio::useSmallLegend(bool bSmall)
{
   m_bSmallLegend = bSmall;
}

void MenuItemRadio::setPrevAvailableFocusableItem()
{
   bool bFound = false;

   for( int k=0; k<m_nSelectionsCount*2; k++ )
   {
      m_nFocusedIndex--;
      if ( m_nFocusedIndex < 0 )
         m_nFocusedIndex = m_nSelectionsCount-1;
      if ( m_bEnabledSelections[m_nFocusedIndex] )
      {
         bFound = true;
         break;
      }
   }
   if ( ! bFound )
      m_nFocusedIndex = -1;
}

void MenuItemRadio::setNextAvailableFocusableItem()
{
   bool bFound = false;

   for( int k=0; k<m_nSelectionsCount*2; k++ )
   {
      m_nFocusedIndex++;
      if ( m_nFocusedIndex >= m_nSelectionsCount )
         m_nFocusedIndex = 0;
      if ( m_bEnabledSelections[m_nFocusedIndex] )
      {
         bFound = true;
         break;
      }
   }
   if ( ! bFound )
      m_nFocusedIndex = -1;
}

bool MenuItemRadio::preProcessKeyUp()
{
   int n = m_nFocusedIndex;
   setPrevAvailableFocusableItem();
   if ( m_nFocusedIndex > n )
   {
      m_nFocusedIndex = n;
      return false;
   }
   return true;
}

bool MenuItemRadio::preProcessKeyDown()
{
   int n = m_nFocusedIndex;
   setNextAvailableFocusableItem();
   if ( m_nFocusedIndex < n )
   {
      m_nFocusedIndex = n;
      return false;
   }
   return true;
}

void MenuItemRadio::onClick()
{
   m_nSelectedIndex = m_nFocusedIndex;
}


void MenuItemRadio::onKeyUp(bool bIgnoreReversion)
{
   setPrevAvailableFocusableItem();
}

void MenuItemRadio::onKeyDown(bool bIgnoreReversion)
{
   setNextAvailableFocusableItem();
}

void MenuItemRadio::onKeyLeft(bool bIgnoreReversion)
{
   setPrevAvailableFocusableItem();
}

void MenuItemRadio::onKeyRight(bool bIgnoreReversion)
{
   setNextAvailableFocusableItem();
}


float MenuItemRadio::getItemHeight(float maxWidth)
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   float fHeight = 0.0;

   for( int i=0; i<m_nSelectionsCount; i++ )
   {
      fHeight += g_pRenderEngine->getMessageHeight(m_szSelections[i], MENU_TEXTLINE_SPACING, maxWidth - m_fSelectorWidth, g_idFontMenu);
      if ( NULL != m_szSelectionsLegend[i] && 0 != m_szSelectionsLegend[i][0] )
      {
         if ( m_bSmallLegend )
            fHeight += g_pRenderEngine->getMessageHeight(m_szSelectionsLegend[i], MENU_TEXTLINE_SPACING, maxWidth-m_fSelectorWidth-m_fLegendDx, g_idFontMenuSmall);
         else
            fHeight += g_pRenderEngine->getMessageHeight(m_szSelectionsLegend[i], MENU_TEXTLINE_SPACING, maxWidth-m_fSelectorWidth-m_fLegendDx, g_idFontMenu) + height_text_small*0.2;
      }
   }

   if ( m_nSelectionsCount > 1 )
      fHeight += (float)(m_nSelectionsCount-1) * MENU_ITEM_SPACING * height_text;

   fHeight += m_fExtraHeight;
   return fHeight;
}

float MenuItemRadio::getTitleWidth(float maxWidth)
{
   //return MenuItem::getTitleWidth(maxWidth);
   return maxWidth;
}

float MenuItemRadio::getValueWidth(float maxWidth)
{
   //return MenuItem::getValueWidth(maxWidth);
   return maxWidth;
}


void MenuItemRadio::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontMenuSmall);
   
   g_pRenderEngine->setColors(get_Color_MenuText());
   float y = yPos;

   for( int i=0; i<m_nSelectionsCount; i++ )
   {
      float yItem = y;
      g_pRenderEngine->setColors(get_Color_MenuText());
      if ( ! m_bEnabledSelections[i] )
         g_pRenderEngine->setColors(get_Color_ItemDisabledText());

      g_pRenderEngine->drawCircle(xPos + m_fSelectorWidth*0.4, y + m_fSelectorWidth*0.3*g_pRenderEngine->getAspectRatio(), m_fSelectorWidth*0.5);
      if ( i == m_nSelectedIndex )
         g_pRenderEngine->fillCircle(xPos + m_fSelectorWidth*0.4, y + m_fSelectorWidth*0.3*g_pRenderEngine->getAspectRatio(), m_fSelectorWidth*0.3);

      y += g_pRenderEngine->drawMessageLines(xPos + m_fSelectorWidth, y, m_szSelections[i], MENU_TEXTLINE_SPACING, fWidthSelection-m_fSelectorWidth, g_idFontMenu);
      if ( NULL != m_szSelectionsLegend[i] && 0 != m_szSelectionsLegend[i][0] )
      {
         if ( m_bSmallLegend )
            y += g_pRenderEngine->drawMessageLines(xPos + m_fSelectorWidth + m_fLegendDx, y, m_szSelectionsLegend[i], MENU_TEXTLINE_SPACING, fWidthSelection-m_fSelectorWidth-m_fLegendDx, g_idFontMenuSmall);
         else
            y += g_pRenderEngine->drawMessageLines(xPos + m_fSelectorWidth + m_fLegendDx, y + height_text_small*0.2, m_szSelectionsLegend[i], MENU_TEXTLINE_SPACING, fWidthSelection-m_fSelectorWidth-m_fLegendDx, g_idFontMenu) + height_text_small*0.2;
      }

      if ( i == m_nFocusedIndex )
      {
         g_pRenderEngine->setColors(get_Color_ItemSelectedBg());
         g_pRenderEngine->setFill(0,0,0,0);
         g_pRenderEngine->setStrokeSize(1);
         g_pRenderEngine->drawRoundRect(xPos-Menu::getSelectionPaddingX(), yItem-Menu::getSelectionPaddingY(), fWidthSelection + 2.0 * Menu::getSelectionPaddingX(), (y-yItem) + 2.0 * Menu::getSelectionPaddingY(), 0.01);
      }

      y += MENU_ITEM_SPACING * height_text;
   }

   g_pRenderEngine->setColors(get_Color_MenuText());
}

void MenuItemRadio::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   return;
}
