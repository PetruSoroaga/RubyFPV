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

#include <math.h>
#include "menu_items.h"
#include "menu_objects.h"
#include "menu.h"

MenuItem::MenuItem(const char* title)
{
   m_ItemType = MENU_ITEM_TYPE_SIMPLE;
   m_bEnabled = true;
   m_bIsEditable = false;
   m_bIsEditing = false;
   m_bShowArrow = false;
   m_bCondensedOnly = false;
   m_RenderTitleWidth = 0.0;
   m_RenderTitleHeight = 0.0;
   m_RenderValueWidth = 0.0;
   m_RenderWidth = 0.0;
   m_RenderHeight = 0.0;
   m_RenderLastY = 0.0;
   m_RenderLastX = 0.0;

   m_fExtraHeight = 0.0;
   m_bCustomTextColor = false;
   m_pMenu = NULL;
   m_pszTitle = NULL;
   m_pszTooltip = NULL;
   m_fMarginX = 0.0;
   setTitle(title);
   setTooltip("");
}

MenuItem::MenuItem(const char* title, const char* tooltip)
{
   m_ItemType = MENU_ITEM_TYPE_SIMPLE;
   m_bEnabled = true;
   m_bIsEditable = false;
   m_bIsEditing = false;
   m_bShowArrow = false;
   m_bCondensedOnly = false;
   m_RenderTitleWidth = 0.0;
   m_RenderTitleHeight = 0.0;
   m_RenderValueWidth = 0.0;
   m_RenderWidth = 0.0;
   m_RenderHeight = 0.0;
   m_RenderLastY = 0.0;
   m_RenderLastX = 0.0;

   m_fExtraHeight = 0.0;
   m_bCustomTextColor = false;
   m_pMenu = NULL;
   m_pszTitle = NULL;
   m_pszTooltip = NULL;
   m_fMarginX = 0.0;
   setTitle(title);
   setTooltip(tooltip);
}


MenuItem::~MenuItem()
{
   if ( NULL != m_pszTitle )
      free(m_pszTitle);
   if ( NULL != m_pszTooltip )
      free(m_pszTooltip);
   m_pszTitle = NULL;
   m_pszTooltip = NULL;
}

bool MenuItem::isEnabled()
{
   return m_bEnabled;
}

bool MenuItem::isSelectable()
{
   return m_bEnabled;
}

void MenuItem::setEnabled(bool enabled)
{
   m_bEnabled = enabled;
}

void MenuItem::setIsEditable() { m_bIsEditable = true; }
void MenuItem::setNotEditable() { m_bIsEditable = false; }
void MenuItem::beginEdit() { if ( m_bIsEditable ) m_bIsEditing = true; }
void MenuItem::endEdit(bool bCanceled) { if ( m_bIsEditable ) m_bIsEditing = false; }
bool MenuItem::isEditing() { return m_bIsEditing; }
bool MenuItem::isEditable() { return m_bIsEditable; }
void MenuItem::showArrow() { m_bShowArrow = true; }
void MenuItem::setCondensedOnly() { m_bCondensedOnly = true; }

void MenuItem::setMargin(float fMargin)
{
   m_fMarginX = fMargin;
}

void MenuItem::setTitle( const char* title )
{
   if ( NULL != m_pszTitle )
      free(m_pszTitle);

   if ( NULL == title || 0 == title[0] )
   {
      m_pszTitle = (char*)malloc(2);
      m_pszTitle[0] = 0;
      return;
   }
   m_pszTitle = (char*)malloc(strlen(title)+1);
   strcpy(m_pszTitle, title);

   invalidate();

   m_RenderTitleWidth = 0.0;
   m_RenderTitleHeight = 0.0;
   m_RenderValueWidth = 0.0;
   m_RenderWidth = 0.0;
   m_RenderHeight = 0.0;
   //if ( NULL != m_pMenu )
   //   m_pMenu->invalidate();
}

char* MenuItem::getTitle()
{
   return m_pszTitle;
}

void MenuItem::setTooltip( const char* tooltip )
{
   if ( NULL != m_pszTooltip )
      free(m_pszTooltip);

   if ( NULL == tooltip || 0 == tooltip[0])
   {
      m_pszTooltip = (char*)malloc(1);
      m_pszTooltip[0] = 0;
      return;
   }
   m_pszTooltip = (char*)malloc(strlen(tooltip)+1);
   strcpy(m_pszTooltip, tooltip);
}

char* MenuItem::getTooltip()
{
   return m_pszTooltip;
}

void MenuItem::setTextColor(double* pColor)
{
   if ( NULL == pColor )
      return;

   m_bCustomTextColor = true;
   memcpy(&m_TextColor[0], pColor, 4*sizeof(double));
}

void MenuItem::invalidate()
{
   m_RenderTitleWidth = 0.0;
   m_RenderTitleHeight = 0.0;
   m_RenderValueWidth = 0.0;
   m_RenderWidth = 0.0;
   m_RenderHeight = 0.0;
}

bool MenuItem::preProcessKeyUp()
{
   return false;
}

bool MenuItem::preProcessKeyDown()
{
   return false;
}

bool MenuItem::preProcessKeyLeft()
{
   return preProcessKeyUp();
}

bool MenuItem::preProcessKeyRight()
{
   return preProcessKeyDown();
}

void MenuItem::onClick()
{
}

float MenuItem::getItemRenderYPos()
{
   return m_RenderLastY;
}

float MenuItem::getItemHeight(float maxWidth)
{
   if ( m_RenderHeight > 0.001 )
      return m_RenderHeight;
   m_RenderTitleHeight = g_pRenderEngine->textHeight(g_idFontMenu);
   m_RenderHeight = m_RenderTitleHeight;
   return m_RenderHeight;
}

float MenuItem::getTitleWidth(float maxWidth)
{
   if ( m_RenderTitleWidth > 0.001 )
      return m_RenderTitleWidth;

   m_RenderTitleWidth = g_pRenderEngine->textWidth(g_idFontMenu, m_pszTitle);

   if ( m_bShowArrow )
      m_RenderTitleWidth += 0.66*g_pRenderEngine->textHeight(g_idFontMenu);
   return m_RenderTitleWidth;
}

float MenuItem::getValueWidth(float maxWidth)
{
   return 0.0;
}

void MenuItem::RenderBaseTitle(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float width_text = m_RenderTitleWidth;
    
   float selectionMarginX = Menu::getSelectionPaddingX();
   float selectionMarginY = Menu::getSelectionPaddingY();
   if ( bSelected && (!m_bIsEditing) )
   {
      g_pRenderEngine->setColors(get_Color_ItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPos-selectionMarginX, yPos-selectionMarginY, fWidthSelection+2.0*selectionMarginX, height_text+2.0*selectionMarginY + g_pRenderEngine->getPixelHeight(), selectionMarginY);
      g_pRenderEngine->setColors(get_Color_ItemSelectedText());
   }
   else
   {
      if ( m_bEnabled )
      {
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
      }
      else
         g_pRenderEngine->setColors(get_Color_ItemDisabledText());
   }
   g_pRenderEngine->drawText(xPos, yPos+g_pRenderEngine->getPixelHeight()*0.2, g_idFontMenu, m_pszTitle);

   if ( m_bShowArrow )
   {
      if ( (!bSelected) )
      {
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
      }
      else
         g_pRenderEngine->setColors(get_Color_ItemSelectedText());
      if ( ! m_bEnabled )
         g_pRenderEngine->setColors(get_Color_ItemDisabledText());
  
      float size = height_text*0.28;

      float xEnd = xPos + width_text - 2.0*g_pRenderEngine->getPixelWidth();
      float yEnd = yPos + height_text*0.5;
      g_pRenderEngine->drawTriangle(xEnd,yEnd, xEnd-size, yEnd+size, xEnd-size, yEnd-size);
      g_pRenderEngine->setColors(get_Color_MenuText());
   }
}

void MenuItem::setLastRenderPos(float xPos, float yPos)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
}

void MenuItem::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   RenderBaseTitle(xPos, yPos, bSelected, fWidthSelection);
}

void MenuItem::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
}
