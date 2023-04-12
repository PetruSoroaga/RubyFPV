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
#include "../base/config.h"

#include "menu_item_select_base.h"
#include "menu_objects.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"

MenuItemSelectBase::MenuItemSelectBase(const char* title)
:MenuItem(title)
{
   m_SelectionsCount = 0;
   m_SelectedIndex = -1;
   m_SelectedIndexBeforeEdit = 0;
   m_bDisabledClick = false;
}

MenuItemSelectBase::MenuItemSelectBase(const char* title, const char* tooltip)
:MenuItem(title, tooltip)
{
   m_SelectionsCount = 0;
   m_SelectedIndex = -1;
   m_SelectedIndexBeforeEdit = 0;
   m_bDisabledClick = false;
}

MenuItemSelectBase::~MenuItemSelectBase()
{
   removeAllSelections();
}


void MenuItemSelectBase::removeAllSelections()
{
   for( int i=0; i<m_SelectionsCount; i++ )
      free( m_szSelections[i] );
   m_SelectionsCount = 0;
}

void MenuItemSelectBase::addSelection(const char* szText)
{
   if ( NULL == szText || m_SelectionsCount >= MAX_MENU_ITEM_SELECTIONS - 1 )
      return;

   if ( -1 == m_SelectedIndex )
      m_SelectedIndex = 0;

   m_szSelections[m_SelectionsCount] = (char*)malloc(strlen(szText)+1);
   strcpy(m_szSelections[m_SelectionsCount], szText);
   m_bEnabledItems[m_SelectionsCount] = true;
   m_SelectionsCount++;
}

void MenuItemSelectBase::addSelection(const char* szText, bool bEnabled)
{
   addSelection(szText);
   if ( ! bEnabled )
      setSelectionIndexDisabled(m_SelectionsCount-1);
}

void MenuItemSelectBase::setSelection(int index)
{
   if ( index >= 0 && index < m_SelectionsCount )
      m_SelectedIndex = index;
   else
      m_SelectedIndex = 0;
}

void MenuItemSelectBase::setSelectedIndex(int index)
{
   setSelection(index);
}

void MenuItemSelectBase::setSelectionIndexDisabled(int index)
{
   m_bEnabledItems[index] = false;
}

void MenuItemSelectBase::setSelectionIndexEnabled(int index)
{
   m_bEnabledItems[index] = true;
}

int MenuItemSelectBase::getSelectedIndex()
{
   return m_SelectedIndex;
}

int MenuItemSelectBase::getSelectionsCount()
{
   return m_SelectionsCount;
}

void MenuItemSelectBase::restoreSelectedIndex()
{
   m_SelectedIndex = m_SelectedIndexBeforeEdit;
}

void MenuItemSelectBase::disableClick()
{
   m_bDisabledClick = true;
}

void MenuItemSelectBase::beginEdit()
{
   m_SelectedIndexBeforeEdit = m_SelectedIndex;

   MenuItem::beginEdit();
}

void MenuItemSelectBase::endEdit(bool bCanceled)
{
   if ( bCanceled )
      restoreSelectedIndex();
   MenuItem::endEdit(bCanceled);
}

void MenuItemSelectBase::onClick()
{
   if ( m_bDisabledClick )
      return;
   m_SelectedIndex++;
   if ( m_SelectedIndex >= m_SelectionsCount )
      m_SelectedIndex = 0;

   int k = m_SelectionsCount*2;
   while ( (!m_bEnabledItems[m_SelectedIndex]) && k > 0 )
   {
      k--;
      m_SelectedIndex++;
      if ( m_SelectedIndex >= m_SelectionsCount )
         m_SelectedIndex = 0;
   }
}


void MenuItemSelectBase::onKeyUp(bool bIgnoreReversion)
{
   m_SelectedIndex--;
   if ( m_SelectedIndex < 0 )
      m_SelectedIndex = m_SelectionsCount-1;

   int k = m_SelectionsCount*2;
   while ( (!m_bEnabledItems[m_SelectedIndex]) && k > 0 )
   {
      k--;
      m_SelectedIndex--;
      if ( m_SelectedIndex < 0 )
         m_SelectedIndex = m_SelectionsCount-1;
   }
}

void MenuItemSelectBase::onKeyDown(bool bIgnoreReversion)
{
   m_SelectedIndex++;
   if ( m_SelectedIndex >= m_SelectionsCount )
      m_SelectedIndex = 0;

   int k = m_SelectionsCount*2;
   while ( (!m_bEnabledItems[m_SelectedIndex]) && k > 0 )
   {
      k--;
      m_SelectedIndex++;
      if ( m_SelectedIndex >= m_SelectionsCount )
         m_SelectedIndex = 0;
   }
}

void MenuItemSelectBase::onKeyLeft(bool bIgnoreReversion)
{
   m_SelectedIndex--;
   if ( m_SelectedIndex < 0 )
      m_SelectedIndex = m_SelectionsCount-1;

   int k = m_SelectionsCount*2;
   while ( (!m_bEnabledItems[m_SelectedIndex]) && k > 0 )
   {
      k--;
      m_SelectedIndex--;
      if ( m_SelectedIndex < 0 )
         m_SelectedIndex = m_SelectionsCount-1;
   }
}

void MenuItemSelectBase::onKeyRight(bool bIgnoreReversion)
{
   m_SelectedIndex++;
   if ( m_SelectedIndex >= m_SelectionsCount )
      m_SelectedIndex = 0;

   int k = m_SelectionsCount*2;
   while ( (!m_bEnabledItems[m_SelectedIndex]) && k > 0 )
   {
      k--;
      m_SelectedIndex++;
      if ( m_SelectedIndex >= m_SelectionsCount )
         m_SelectedIndex = 0;
   }
}

void MenuItemSelectBase::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   RenderBaseTitle(xPos,yPos, bSelected, fWidthSelection);

   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);

   if ( -1 == m_SelectedIndex )
      return;

   float width_text = g_pRenderEngine->textWidth( height_text, g_idFontMenu, m_szSelections[m_SelectedIndex]);
   float padding = MENU_SELECTION_PADDING*menu_getScaleMenus();
   float paddingH = padding/g_pRenderEngine->getAspectRatio();
   float triSize = 0.0062*menu_getScaleMenus();
   float totalWidth = width_text + 2.0*triSize + 2.0*paddingH;
   float xValue = xPos + m_pMenu->getUsableWidth() - totalWidth;
   
   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_ItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xValue-paddingH, yPos-0.9*padding, totalWidth + 2.0*paddingH , m_RenderHeight + 2.0*padding, 0.01*menu_getScaleMenus());
      g_pRenderEngine->setColors(get_Color_MenuText());
   }  

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_ItemSelectedText());
   else
      g_pRenderEngine->setColors(get_Color_MenuText());
   if ( ! m_bEnabled )
      g_pRenderEngine->setColors(get_Color_ItemDisabledText());

   g_pRenderEngine->drawText(xValue + paddingH + triSize, yPos, height_text, g_idFontMenu, m_szSelections[m_SelectedIndex]);

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_ItemSelectedText());
   else
      g_pRenderEngine->setColors(get_Color_MenuText());

   if ( ! m_bEnabled )
      g_pRenderEngine->setColors(get_Color_ItemDisabledText());

   float yT = yPos+m_RenderHeight*0.5;
   g_pRenderEngine->drawTriangle(xValue, yT, xValue+triSize, yT+triSize, xValue+triSize, yT-triSize);
   xValue += totalWidth;
   g_pRenderEngine->drawTriangle(xValue, yT, xValue-triSize, yT+triSize, xValue-triSize, yT-triSize);
}

void MenuItemSelectBase::RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   m_RenderLastY = yPos;
   m_RenderLastX = xPos;
   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);

   float width_text = g_pRenderEngine->textWidth( height_text, g_idFontMenu, m_szSelections[m_SelectedIndex]);
   float padding = MENU_SELECTION_PADDING*menu_getScaleMenus();
   float triSize = 0.0062*menu_getScaleMenus();
   float totalWidth = width_text + 2*triSize + 2*padding;
   
   if ( m_bIsEditing )
   {
      g_pRenderEngine->setColors(get_Color_ItemSelectedBg());
      g_pRenderEngine->drawRoundRect(xPos-padding, yPos-0.8*padding, 3.0*padding + totalWidth , m_RenderHeight + 2.0*padding, 0.01*menu_getScaleMenus());
      g_pRenderEngine->setColors(get_Color_MenuText());
   }
   else if ( bSelected )
   {
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->setStrokeSize(1);
      g_pRenderEngine->drawRoundRect(xPos-padding, yPos-0.8*padding, 3.0*padding + totalWidth , m_RenderHeight + 2.0*padding, 0.01*menu_getScaleMenus());
      g_pRenderEngine->setColors(get_Color_MenuText());
   }

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_ItemSelectedText());
   else if ( m_bCustomTextColor )
      g_pRenderEngine->setColors(&m_TextColor[0]);
   else
      g_pRenderEngine->setColors(get_Color_MenuText());
   if ( ! m_bEnabled )
      g_pRenderEngine->setColors(get_Color_ItemDisabledText());

   g_pRenderEngine->drawText(xPos + padding + triSize, yPos, height_text, g_idFontMenu, m_szSelections[m_SelectedIndex]);

   if ( m_bIsEditing )
      g_pRenderEngine->setColors(get_Color_ItemSelectedText());
   else if ( m_bCustomTextColor )
      g_pRenderEngine->setColors(&m_TextColor[0]);
   else
      g_pRenderEngine->setColors(get_Color_MenuText());

   if ( ! m_bEnabled )
      g_pRenderEngine->setColors(get_Color_ItemDisabledText());
   float yT = yPos+m_RenderHeight*0.5;
   g_pRenderEngine->drawTriangle(xPos, yT, xPos+triSize, yT+triSize, xPos+triSize, yT-triSize);
   xPos += totalWidth;
   g_pRenderEngine->drawTriangle(xPos, yT, xPos-triSize, yT+triSize, xPos-triSize, yT-triSize);
}
