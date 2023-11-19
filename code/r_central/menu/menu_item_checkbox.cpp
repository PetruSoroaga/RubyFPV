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

#include "menu.h"
#include "menu_item_checkbox.h"
#include "menu_objects.h"


MenuItemCheckbox::MenuItemCheckbox(const char* title)
:MenuItem(title)
{
   m_bChecked = false;
}

MenuItemCheckbox::MenuItemCheckbox(const char* title, const char* tooltip)
:MenuItem(title, tooltip)
{
   m_bChecked = false;
}

MenuItemCheckbox::~MenuItemCheckbox()
{
}

bool MenuItemCheckbox::isChecked()
{
   return m_bChecked;
}

void MenuItemCheckbox::setChecked(bool bChecked)
{
   m_bChecked = bChecked;
}


void MenuItemCheckbox::beginEdit()
{
   MenuItem::beginEdit();
}

void MenuItemCheckbox::endEdit(bool bCanceled)
{
   MenuItem::endEdit(bCanceled);
}

void MenuItemCheckbox::onClick()
{
   m_bChecked = ! m_bChecked;
   invalidate();
}

float MenuItemCheckbox::getItemHeight(float maxWidth)
{
   if ( m_RenderHeight > 0.001 )
      return m_RenderHeight;

   m_RenderTitleHeight = g_pRenderEngine->textHeight(g_idFontMenu);
   m_RenderHeight = m_RenderTitleHeight;
   return m_RenderHeight;
}


float MenuItemCheckbox::getTitleWidth(float maxWidth)
{
   if ( m_RenderTitleWidth > 0.001 )
      return m_RenderTitleWidth;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   m_RenderTitleWidth = g_pRenderEngine->textWidth(g_idFontMenu, m_pszTitle);
   
   return m_RenderTitleWidth;
}

float MenuItemCheckbox::getValueWidth(float maxWidth)
{
   return getItemHeight(0.0) + Menu::getMenuPaddingY();
}


void MenuItemCheckbox::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   float fCheckWidth = getValueWidth(0.0);
   RenderCondensed(xPos, yPos, bSelected, fWidthSelection);
   RenderBaseTitle(xPos+fCheckWidth, yPos, bSelected, fWidthSelection);
}


void MenuItemCheckbox::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float paddingV = Menu::getSelectionPaddingY();
   float paddingH = Menu::getSelectionPaddingX();
   float width = m_RenderTitleHeight/g_pRenderEngine->getAspectRatio();

   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_ItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPos-paddingH, yPos-paddingV, width + 2.0*paddingH, m_RenderTitleHeight + 2.0*paddingV, 0.01*Menu::getMenuPaddingY());
   }
   else if ( bSelected )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setFill(0,0,0,0);   
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawRoundRect(xPos-paddingH, yPos-paddingV, width + 2.0*paddingH, m_RenderTitleHeight + 2.0*paddingV, 0.01*Menu::getMenuPaddingY());
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

   g_pRenderEngine->setFill(0,0,0,0);   
   g_pRenderEngine->setStrokeSize(2);
   float corner = 0.1*Menu::getMenuPaddingY();
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, m_RenderTitleHeight, corner);
   corner = 0.4*corner;
   float cornerH = corner/g_pRenderEngine->getAspectRatio();
   if ( m_bChecked )
   {
      g_pRenderEngine->drawLine(xPos+cornerH, yPos+corner, xPos+width-cornerH, yPos+m_RenderTitleHeight-corner);
      g_pRenderEngine->drawLine(xPos+cornerH, yPos+m_RenderTitleHeight-corner, xPos+width-cornerH, yPos+corner);
   }
}
