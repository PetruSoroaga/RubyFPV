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

#include "menu_item_text.h"
#include "menu_objects.h"
#include "menu.h"


MenuItemText::MenuItemText(const char* title)
:MenuItem(title)
{
   m_bEnabled = true;
   m_bIsEditable = false;
   m_bUseSmallText = false;
   m_fScale = 1.0;
   m_fMarginX = 0.5 * Menu::getMenuPaddingX();
}

MenuItemText::MenuItemText(const char* title, bool bUseSmallText)
:MenuItem(title)
{
   m_bEnabled = true;
   m_bIsEditable = false;
   m_bUseSmallText = bUseSmallText;
   m_fScale = 1.0;
   m_fMarginX = 0.5 * Menu::getMenuPaddingX();
}
     
MenuItemText::MenuItemText(const char* title,  bool bUseSmallText, float fMargin)
:MenuItem(title)
{
   m_bEnabled = true;
   m_bIsEditable = false;
   m_bUseSmallText = bUseSmallText;
   m_fScale = 1.0;
   m_fMarginX = fMargin;
}

MenuItemText::~MenuItemText()
{
}

bool MenuItemText::isSelectable()
{
   return false;
}

void MenuItemText::setSmallText()
{
   m_bUseSmallText = true;
}

float MenuItemText::getItemHeight(float maxWidth)
{
   if ( m_RenderHeight > 0.1 )
      return m_RenderHeight;
   if ( m_bUseSmallText )
      m_RenderHeight = g_pRenderEngine->getMessageHeight(m_pszTitle, MENU_TEXTLINE_SPACING, maxWidth - m_fMarginX, g_idFontMenuSmall);
   else
      m_RenderHeight = g_pRenderEngine->getMessageHeight(m_pszTitle, MENU_TEXTLINE_SPACING, maxWidth - m_fMarginX, g_idFontMenu);
   return m_RenderHeight;
}

float MenuItemText::getTitleWidth(float maxWidth)
{
   return 0.0;
   //return MenuItem::getTitleWidth(maxWidth);
}


void MenuItemText::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{      
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   
   if ( m_bEnabled )
      g_pRenderEngine->setColors(get_Color_MenuText());
   else
      g_pRenderEngine->setColors(get_Color_ItemDisabledText());

   if ( m_bUseSmallText )
      g_pRenderEngine->drawMessageLines(xPos + m_fMarginX, yPos, m_pszTitle, MENU_TEXTLINE_SPACING, m_pMenu->getUsableWidth()-m_fMarginX, g_idFontMenuSmall);
   else
      g_pRenderEngine->drawMessageLines(xPos + m_fMarginX, yPos, m_pszTitle, MENU_TEXTLINE_SPACING, m_pMenu->getUsableWidth()-m_fMarginX, g_idFontMenu);
}
