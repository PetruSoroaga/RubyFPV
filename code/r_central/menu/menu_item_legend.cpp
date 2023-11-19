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
   strcpy(m_pszTitle, "• ");
   strcat(m_pszTitle, szTitle);
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
   getValueWidth(maxWidth-m_RenderTitleWidth-Menu::getMenuPaddingX());
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float h = g_pRenderEngine->getMessageHeight(m_pszTooltip, MENU_TEXTLINE_SPACING, m_RenderValueWidth, g_idFontMenu);
   if ( h > m_RenderHeight )
      m_RenderHeight = h;
   return m_RenderHeight;
}

float MenuItemLegend::getTitleWidth(float maxWidth)
{
   return MenuItem::getTitleWidth(maxWidth);
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
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
      
   g_pRenderEngine->setColors(get_Color_MenuText());

   g_pRenderEngine->drawText(xPos, yPos, g_idFontMenu, m_pszTitle); 
   xPos += m_RenderTitleWidth + Menu::getMenuPaddingX();

   g_pRenderEngine->drawMessageLines(xPos, yPos, m_pszTooltip, MENU_TEXTLINE_SPACING, m_RenderValueWidth, g_idFontMenu);
}
