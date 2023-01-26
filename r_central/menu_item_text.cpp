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
#include "menu_item_text.h"
#include "menu_objects.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"


MenuItemText::MenuItemText(const char* title)
:MenuItem(title)
{
   m_bEnabled = true;
   m_bIsEditable = false;
   m_fScale = 1.0;
   m_fMarginX = MENU_MARGINS*menu_getScaleMenus()*0.2;
}

MenuItemText::MenuItemText(const char* title, float fScale)
:MenuItem(title)
{
   m_bEnabled = true;
   m_bIsEditable = false;
   m_fScale = fScale;
   m_fMarginX = MENU_MARGINS*menu_getScaleMenus()*0.2;
}

MenuItemText::~MenuItemText()
{
}

bool MenuItemText::isSelectable()
{
   return false;
}

float MenuItemText::getItemHeight(float maxWidth)
{
   if ( m_RenderHeight > 0.1 )
      return m_RenderHeight;
   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus()*0.8*m_fScale, g_idFontMenu);
   
   m_RenderHeight = g_pRenderEngine->getMessageHeight(m_pszTitle, height_text, MENU_TEXTLINE_SPACING, maxWidth-m_fMarginX, g_idFontMenu);
   m_RenderHeight += 0.005*menu_getScaleMenus();
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
   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus()*0.8*m_fScale, g_idFontMenu);

   if ( m_bEnabled )
      g_pRenderEngine->setColors(get_Color_MenuText());
   else
      g_pRenderEngine->setColors(get_Color_ItemDisabledText());

   g_pRenderEngine->drawMessageLines(xPos + m_fMarginX, yPos+MENU_SELECTION_PADDING*menu_getScaleMenus(), m_pszTitle, height_text, MENU_TEXTLINE_SPACING, m_pMenu->getUsableWidth()-m_fMarginX, g_idFontMenu);
}
