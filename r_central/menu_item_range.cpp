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
#include "menu_item_range.h"
#include "menu_objects.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"

MenuItemRange::MenuItemRange(const char* title, float min, float max, float current, float step)
:MenuItem(title)
{
   m_ValueMin = min;
   m_ValueMax = max;
   m_ValueCurrent = current;
   m_ValueStep = step;
   m_bIsEditable = true;
   m_szSufix[0] = 0;
   m_szPrefix[0] = 0;
}

MenuItemRange::MenuItemRange(const char* title, const char* tooltip, float min, float max, float current, float step)
:MenuItem(title, tooltip)
{
   m_ValueMin = min;
   m_ValueMax = max;
   m_ValueCurrent = current;
   m_ValueStep = step;
   m_bIsEditable = true;
   m_szSufix[0] = 0;
   m_szPrefix[0] = 0;
}

MenuItemRange::~MenuItemRange()
{
}

void MenuItemRange::setSufix(const char* sufix)
{
   if ( NULL == sufix )
      m_szSufix[0] = 0;
   else
      strcpy(m_szSufix, sufix);
}

void MenuItemRange::setPrefix(const char* szPrefix)
{
   if ( NULL == szPrefix )
      m_szPrefix[0] = 0;
   else
      strcpy(m_szPrefix, szPrefix);
}

float MenuItemRange::getCurrentValue()
{
   return m_ValueCurrent;
}

void MenuItemRange::setCurrentValue(float val)
{
   m_ValueCurrent = val;
   if ( m_ValueCurrent < m_ValueMin )
      m_ValueCurrent = m_ValueMin;
   if ( m_ValueCurrent > m_ValueMax )
      m_ValueCurrent = m_ValueMax;
   m_RenderValueWidth = 0.0;
}

void MenuItemRange::onKeyUp(bool bIgnoreReversion)
{
   m_ValueCurrent -= m_ValueStep;
   if ( m_ValueCurrent < m_ValueMin )
      m_ValueCurrent = m_ValueMin;
   m_RenderValueWidth = 0.0;
}

void MenuItemRange::onKeyDown(bool bIgnoreReversion)
{
   m_ValueCurrent += m_ValueStep;
   if ( m_ValueCurrent > m_ValueMax )
      m_ValueCurrent = m_ValueMax;
   m_RenderValueWidth = 0.0;
}

void MenuItemRange::onKeyLeft(bool bIgnoreReversion)
{
   m_ValueCurrent -= m_ValueStep;
   if ( m_ValueCurrent < m_ValueMin )
      m_ValueCurrent = m_ValueMin;
   m_RenderValueWidth = 0.0;
}

void MenuItemRange::onKeyRight(bool bIgnoreReversion)
{
   m_ValueCurrent += m_ValueStep;
   if ( m_ValueCurrent > m_ValueMax )
      m_ValueCurrent = m_ValueMax;
   m_RenderValueWidth = 0.0;
}

float MenuItemRange::getValueWidth(float maxWidth)
{
   //if ( (!m_bIsEditing) && m_RenderValueWidth > 0.01 )
   //   return m_RenderValueWidth;

   char szBuff[128];
   sprintf(szBuff, "%.1f", m_ValueCurrent);
   if ( szBuff[strlen(szBuff)-1] == '0' )
   if ( szBuff[strlen(szBuff)-2] == '.' )
      szBuff[strlen(szBuff)-2] = 0;
   if ( 0 != m_szPrefix[0] )
   {
      strcat(szBuff, " ");
      strcat(szBuff, m_szPrefix);
   }
   if ( 0 != m_szSufix[0] )
   {
      strcat(szBuff, " ");
      strcat(szBuff, m_szSufix);
   }
   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);
   m_RenderValueWidth = g_pRenderEngine->textWidth(height_text, g_idFontMenu, szBuff);
   return m_RenderValueWidth;
}

void MenuItemRange::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   if ( m_bCondensedOnly )
      return;
   
   RenderBaseTitle(xPos, yPos, bSelected, fWidthSelection);

   float padding = MENU_SELECTION_PADDING*menu_getScaleMenus();

   char szBuff[128];
   szBuff[0] = 0;
   if ( 0 != m_szPrefix[0] )
   {
      strcat(szBuff, m_szPrefix);
      strcat(szBuff, " ");
   }

   char szValue[32];
   sprintf(szValue, "%.1f", m_ValueCurrent);
   if ( szValue[strlen(szValue)-1] == '0' )
   if ( szValue[strlen(szValue)-2] == '.' )
      szValue[strlen(szValue)-2] = 0;
   strcat(szBuff, szValue);

   if ( 0 != m_szSufix[0] )
   {
      strcat(szBuff, " ");
      strcat(szBuff, m_szSufix);
   }

   m_RenderValueWidth = getValueWidth(m_pMenu->getUsableWidth());
   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);
   float width = m_RenderValueWidth + 2*padding;
   
   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_ItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPos+m_pMenu->getUsableWidth()-m_RenderValueWidth-padding, yPos-0.8*padding, width, m_RenderTitleHeight + 2*padding, 0.01*menu_getScaleMenus());
   }

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_ItemSelectedText());
   else
   {
      if ( m_bEnabled )
         g_pRenderEngine->setColors(get_Color_MenuText());
      else
         g_pRenderEngine->setColors(get_Color_ItemDisabledText());
   }
   g_pRenderEngine->drawText(xPos+m_pMenu->getUsableWidth()-m_RenderValueWidth, yPos, height_text, g_idFontMenu, szBuff);
}

void MenuItemRange::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   float padding = MENU_SELECTION_PADDING*menu_getScaleMenus();

   char szBuff[32];
   sprintf(szBuff, "%.1f", m_ValueCurrent);
   if ( szBuff[strlen(szBuff)-1] == '0' )
   if ( szBuff[strlen(szBuff)-2] == '.' )
      szBuff[strlen(szBuff)-2] = 0;
   if ( 0 != m_szSufix[0] )
   {
      strcat(szBuff, " ");
      strcat(szBuff, m_szSufix);
   }

   if ( m_bIsEditing )
      m_RenderValueWidth = getValueWidth(m_pMenu->getUsableWidth());
   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);
   float width = m_RenderValueWidth + 2*padding;
   
   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_ItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPos-padding, yPos-0.8*padding, width, m_RenderTitleHeight + 2*padding, 0.01*menu_getScaleMenus());
   }
   else if ( bSelected )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setFill(0,0,0,0);   
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawRoundRect(xPos-padding, yPos-0.8*padding, width, m_RenderTitleHeight + 2*padding, 0.01*menu_getScaleMenus());
   }

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_ItemSelectedText());
   else
   {
      if ( m_bEnabled )
         g_pRenderEngine->setColors(get_Color_MenuText());
      else
         g_pRenderEngine->setColors(get_Color_ItemDisabledText());
   }
   g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontMenu, szBuff);
}
