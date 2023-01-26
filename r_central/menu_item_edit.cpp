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
#include "menu_item_edit.h"
#include "menu_objects.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"


MenuItemEdit::MenuItemEdit(const char* title, const char* szValue)
:MenuItem(title)
{
   m_bOnlyLetters = false;
   m_bIsEditable = true;
   if ( NULL == szValue )
   {
      m_szValue[0] = 0;
      m_szValueOriginal[0] = 0;
   }
   else
   {
      strcpy(m_szValue, szValue);
      strcpy(m_szValueOriginal, szValue);
   }
   m_EditPos = 0;
   m_MaxLength = MAX_EDIT_LENGTH;
}

MenuItemEdit::MenuItemEdit(const char* title, const char* tooltip, const char* szValue)
:MenuItem(title, tooltip)
{
   m_bOnlyLetters = false;
   m_bIsEditable = true;
   if ( NULL == szValue )
   {
      m_szValue[0] = 0;
      m_szValueOriginal[0] = 0;
   }
   else
   {
      strcpy(m_szValue, szValue);
      strcpy(m_szValueOriginal, szValue);
   }
   m_EditPos = 0;
   m_MaxLength = MAX_EDIT_LENGTH;
}

MenuItemEdit::~MenuItemEdit()
{
}

void MenuItemEdit::setOnlyLetters()
{
   m_bOnlyLetters = true;
}

void MenuItemEdit::setMaxLength(int l)
{
   m_MaxLength = l;
}

void MenuItemEdit::setCurrentValue(const char* szValue)
{
   if ( NULL == szValue || 0 == szValue[0] )
   {
      m_szValue[0] = 0;
   }
   else
   {
      strcpy(m_szValue, szValue);
   }
   strcpy(m_szValueOriginal, m_szValue);
   invalidate();
   m_RenderValueWidth = 0.0;
}

const char* MenuItemEdit::getCurrentValue()
{
   return m_szValue;
}

void MenuItemEdit::beginEdit()
{
   if ( ! m_bIsEditing )
   {
      MenuItem::beginEdit();
      m_EditPos = 0;
      return;
   }

   if ( m_szValue[m_EditPos] == 0 )
   {
      m_szValue[m_EditPos] = ' ';
      m_szValue[m_EditPos+1] = 0;
   }
   m_EditPos++;
   if ( m_EditPos >= m_MaxLength )
      m_EditPos = 0;

   m_RenderValueWidth = 0.0;
}


void MenuItemEdit::onKeyUp(bool bIgnoreReversion)
{
   if ( !m_bIsEditing )
      return;
   if ( 0 == m_szValue[m_EditPos] )
   {
      m_szValue[m_EditPos] = 65;
      m_szValue[m_EditPos+1] = 0;
   }
   else
   {
      m_szValue[m_EditPos]--;
      if ( m_bOnlyLetters )
      {
         if ( m_szValue[m_EditPos] == 64 )
            m_szValue[m_EditPos] = 32;
         if ( m_szValue[m_EditPos] < 32 )
            m_szValue[m_EditPos] = 90;
      }
      if ( (!m_bOnlyLetters) && m_szValue[m_EditPos] < 32 )
         m_szValue[m_EditPos] = 122;
   }
   m_RenderValueWidth = 0.0;
}

void MenuItemEdit::onKeyDown(bool bIgnoreReversion)
{
   if ( !m_bIsEditing )
      return;

   if ( 0 == m_szValue[m_EditPos] )
   {
      m_szValue[m_EditPos] = 65;
      m_szValue[m_EditPos+1] = 0;
   }
   else
   {
      m_szValue[m_EditPos]++;
      if ( m_bOnlyLetters )
      {
         if ( m_szValue[m_EditPos] == 33 )
            m_szValue[m_EditPos] = 65;
         if ( m_szValue[m_EditPos] > 90 )
            m_szValue[m_EditPos] = 32;
      }
      if ( (!m_bOnlyLetters) && m_szValue[m_EditPos] > 122 )
         m_szValue[m_EditPos] = 32;
   }
   m_RenderValueWidth = 0.0;
}

void MenuItemEdit::onKeyLeft(bool bIgnoreReversion)
{
   if ( !m_bIsEditing )
      return;
}

void MenuItemEdit::onKeyRight(bool bIgnoreReversion)
{
   if ( !m_bIsEditing )
      return;
}

float MenuItemEdit::getValueWidth(float maxWidth)
{
   if ( m_RenderValueWidth > 0.01 )
      return m_RenderValueWidth;

   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);
   m_RenderValueWidth = g_pRenderEngine->textWidth(height_text, g_idFontMenu, m_szValue);
   return m_RenderValueWidth;
}

void MenuItemEdit::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   RenderBaseTitle(xPos, yPos, bSelected, fWidthSelection);
   getValueWidth(0);
   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);
   float padding = MENU_SELECTION_PADDING*menu_getScaleMenus();
   float paddingH = padding/g_pRenderEngine->getAspectRatio();

   if ( ! m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->drawText(xPos+m_pMenu->getUsableWidth()-m_RenderValueWidth, yPos, height_text, g_idFontMenu, m_szValue);
      return;
   }

   // Editing

   g_pRenderEngine->setColors(get_Color_ItemSelectedBg());
   g_pRenderEngine->drawRoundRect(xPos+m_pMenu->getUsableWidth()-m_RenderValueWidth - paddingH, yPos-0.8*padding, m_RenderValueWidth + 2*paddingH, m_RenderTitleHeight + 2*padding, 0.01*menu_getScaleMenus());
   g_pRenderEngine->setColors(get_Color_MenuText());

   u32 timeNow = get_current_timestamp_ms();
   bool flash = false;
   if ( (timeNow/400)%2 )
      flash = true;

   float w = 0;
   float x = xPos + m_pMenu->getUsableWidth()-m_RenderValueWidth;
   char szBuff[16];
   double cInv[4] = { 255,255,255,0.8 };
   for( int i=0; i<m_MaxLength; i++ )
   {
      szBuff[0] = m_szValue[i];
      szBuff[1] = 0;
      float w = g_pRenderEngine->textWidth( height_text, g_idFontMenu, szBuff);
      if ( flash && (i == m_EditPos))
      {
         g_pRenderEngine->setColors(get_Color_ItemSelectedText());
         g_pRenderEngine->drawRect(x-0.001*menu_getScaleMenus(), yPos-0.003*menu_getScaleMenus(), w + 0.002*menu_getScaleMenus(), m_RenderTitleHeight + 2*0.003*menu_getScaleMenus());
         g_pRenderEngine->setColors(cInv);
      }
      else
         g_pRenderEngine->setColors(get_Color_ItemSelectedText());
   
      if ( 0 != szBuff[0] )
         g_pRenderEngine->drawText(x, yPos, height_text, g_idFontMenu, szBuff);
      if ( szBuff[0] == 0 )
         break;
      x += w;
   }
   g_pRenderEngine->setColors(get_Color_MenuText());
}
