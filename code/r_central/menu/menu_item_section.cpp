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
   if ( m_RenderHeight > 0.01 && m_RenderTitleHeight > 0.01 )
      return m_RenderHeight;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   m_RenderTitleHeight = height_text;

   m_RenderHeight = m_RenderTitleHeight + m_fMarginTop + m_fMarginBottom;
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
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   float yMid = yPos + m_fMarginTop + 0.3*m_RenderHeight;
   g_pRenderEngine->drawLine(xPos-dxpadding, yMid, xPos+dx, yMid);
   g_pRenderEngine->drawLine(xPos+dx+2*dxp2+m_RenderTitleWidth, yMid, xPos+m_pMenu->getUsableWidth(), yMid);
   g_pRenderEngine->setFill(0,0,0,0.4*g_pRenderEngine->getGlobalAlfa());
   g_pRenderEngine->drawRoundRect(xPos+dx, yMid-0.74*height_text, 2.0*dxp2+m_RenderTitleWidth, 1.4*height_text, 0.02*Menu::getMenuPaddingY());
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(0);

/*
   if ( ! m_bEnabled )
   {
      double* pC = get_Color_MenuText();
      double color[4];
      memcpy((u8*)&color[0], (u8*)pC, 4 * sizeof(double));
      color[3] = 0.3;
      g_pRenderEngine->setColors(&color[0]);
   }
   */
   g_pRenderEngine->drawText(xPos+dx+dxp2*0.7, yMid-0.5*height_text, g_idFontMenu, m_pszTitle);

   if ( ! m_bEnabled )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
   }
}
