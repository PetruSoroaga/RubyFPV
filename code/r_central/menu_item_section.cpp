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
#include "menu_item_section.h"
#include "menu_objects.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"

MenuItemSection::MenuItemSection(const char* title)
:MenuItem(title)
{
   m_ItemType = MENU_ITEM_TYPE_SECTION;
   m_bEnabled = false;
   m_bIsEditable = false;
   m_fMarginTop = 0.3*g_pRenderEngine->textHeight(0,g_idFontMenu);
   m_fMarginBottom = 0.1*g_pRenderEngine->textHeight(0,g_idFontMenu);
}

MenuItemSection::MenuItemSection(const char* title, const char* tooltip)
:MenuItem(title, tooltip)
{
   m_ItemType = MENU_ITEM_TYPE_SECTION;
   m_bEnabled = false;
   m_bIsEditable = false;
   m_fMarginTop = 0.3*g_pRenderEngine->textHeight(0,g_idFontMenu);
   m_fMarginBottom = 0.1*g_pRenderEngine->textHeight(0,g_idFontMenu);
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
   if ( m_RenderHeight > 0.01 && m_RenderTitleHeight > 0.01 )
      return m_RenderHeight;

   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);
   m_RenderTitleHeight = height_text;

   m_RenderHeight = m_RenderTitleHeight + m_fMarginTop + m_fMarginBottom + menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS * MENU_SEPARATOR_HEIGHT;
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
   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);
   float dxpadding = 0.005*menu_getScaleMenus();
   float dx = 0.02*menu_getScaleMenus();
   float dxp2 = 0.02*menu_getScaleMenus();

   g_pRenderEngine->setColors(get_Color_MenuBg());
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   float yMid = yPos + m_fMarginTop + 0.46*m_RenderHeight;
   g_pRenderEngine->drawLine(xPos-dxpadding, yMid, xPos+dx, yMid);
   g_pRenderEngine->drawLine(xPos+dx+2*dxp2+m_RenderTitleWidth, yMid, xPos+m_pMenu->getUsableWidth(), yMid);
   g_pRenderEngine->setFill(0,0,0,0.4*g_pRenderEngine->getGlobalAlfa());
   g_pRenderEngine->drawRoundRect(xPos+dx, yMid-0.74*height_text, 2.0*dxp2+m_RenderTitleWidth, 1.4*height_text, 0.02*menu_getScaleMenus());
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(0);

   g_pRenderEngine->drawText(xPos+dx+dxp2*0.7, yMid-0.5*height_text, height_text, g_idFontMenu, m_pszTitle);
}
