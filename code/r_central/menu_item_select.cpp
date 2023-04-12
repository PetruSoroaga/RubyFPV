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
#include "menu_item_select.h"
#include "menu_objects.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"

MenuItemSelect::MenuItemSelect(const char* title)
:MenuItemSelectBase(title)
{
   m_bUseMultipleViewLayout = false;
   m_bUsePopupSelector = true;
   m_bPopupSelectorCentered = true;
   for( int i=0; i<MAX_MENU_ITEM_SELECTIONS; i++ )
      m_szSelectionsTooltips[i] = NULL;

   m_iCountSeparators = 0;
}

MenuItemSelect::MenuItemSelect(const char* title, const char* tooltip)
:MenuItemSelectBase(title, tooltip)
{
   m_bUseMultipleViewLayout = false;
   m_bUsePopupSelector = true;
   m_bPopupSelectorCentered = true;
   for( int i=0; i<MAX_MENU_ITEM_SELECTIONS; i++ )
      m_szSelectionsTooltips[i] = NULL;
   m_iCountSeparators = 0;
}

MenuItemSelect::~MenuItemSelect()
{
   for( int i=0; i<MAX_MENU_ITEM_SELECTIONS; i++ )
   {
      if ( NULL != m_szSelectionsTooltips[i] )
         free( m_szSelectionsTooltips[i] );
      m_szSelectionsTooltips[i] = NULL;
   }
}

void MenuItemSelect::setUseMultiViewLayout()
{
   m_bUseMultipleViewLayout = true;
   m_bUsePopupSelector = false;
}

void MenuItemSelect::setUsePopupSelector()
{
   m_bUseMultipleViewLayout = false;
   m_bUsePopupSelector = true;
}

void MenuItemSelect::disablePopupSelector()
{
   m_bUsePopupSelector = false;
}

void MenuItemSelect::setPopupSelectorToRight()
{
   m_bPopupSelectorCentered = false;
}

void MenuItemSelect::setTooltipForSelection(int selectionIndex, const char* szTooltip)
{
   if ( selectionIndex < 0 || selectionIndex >= MAX_MENU_ITEM_SELECTIONS )
      return;

   if ( NULL != m_szSelectionsTooltips[selectionIndex] )
      free( m_szSelectionsTooltips[selectionIndex] );

   m_szSelectionsTooltips[selectionIndex] = (char*)malloc(strlen(szTooltip)+1);
   strcpy(m_szSelectionsTooltips[selectionIndex], szTooltip);
}

void MenuItemSelect::addSeparator()
{
   if ( 0 == m_SelectionsCount )
      return;
   m_SeparatorIndexes[m_iCountSeparators] = m_SelectionsCount-1;
   m_iCountSeparators++;
}


void MenuItemSelect::onKeyDown(bool bIgnoreReversion)
{
   Preferences* p = get_Preferences();
   bool bReverse = false;
   if ( m_bUsePopupSelector )
   {
      if ( p->iSwapUpDownButtonsValues != 0 )
         bReverse = true;
      //if ( p->iSwapUpDownButtons != 0 )
      //   bReverse = ! bReverse;
   }
   if ( bIgnoreReversion )
      bReverse = false;

   if ( bReverse )
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
   else
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
}


void MenuItemSelect::onKeyUp(bool bIgnoreReversion)
{
   Preferences* p = get_Preferences();
   bool bReverse = false;
   if ( m_bUsePopupSelector )
   {
      if ( p->iSwapUpDownButtonsValues != 0 )
         bReverse = true;
      //if ( p->iSwapUpDownButtons != 0 )
      //   bReverse = ! bReverse;
   }

   if ( bIgnoreReversion )
      bReverse = false;

   if ( bReverse )
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
   else
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
}


void MenuItemSelect::Render(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   if ( m_bUsePopupSelector && m_bIsEditing )
   {
      RenderPopupSelections(xPos, yPos, bSelected, fWidthSelection);
      return;
   }

   if ( ! m_bUseMultipleViewLayout )
      return MenuItemSelectBase::Render(xPos, yPos, bSelected, fWidthSelection);

   RenderBaseTitle(xPos,yPos, bSelected, fWidthSelection);

   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);
   float padding = MENU_SELECTION_PADDING*menu_getScaleMenus();  

   float fAlphaOrg = g_pRenderEngine->getGlobalAlfa();
   float fAlphaLow = 0.3;
   height_text = 0.9*height_text;

   float xEnd = xPos + m_pMenu->getUsableWidth();
   float x0 = xEnd;
   float y0 = yPos;

   float dxPaddings = 0.01*menu_getScaleMenus();
   float dyMargins = 0.0016*menu_getScaleMenus();

   float dyText = 0.2*(m_RenderHeight - m_RenderTitleHeight)-0.07*height_text;
   for( int i=m_SelectionsCount-1; i>=0; i-- )
   {
      float width_text = g_pRenderEngine->textWidth( height_text, g_idFontMenu, m_szSelections[i]);
      if ( m_SelectedIndex != i )
      {
         g_pRenderEngine->setGlobalAlfa(fAlphaLow);
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
         g_pRenderEngine->drawTextLeft(xEnd-dxPaddings, yPos+dyText, height_text, g_idFontMenu, m_szSelections[i]);
      }
      else
      {
         g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
         double* pC = get_Color_ItemSelectedBg();
         float f = pC[3];
         if ( ! m_bEnabled )
            pC[3] = 0.2;
         g_pRenderEngine->setColors(pC);
         g_pRenderEngine->drawRoundRect(xEnd-width_text-2*dxPaddings, yPos-dyMargins, width_text+2*dxPaddings, m_RenderHeight+2*dyMargins, 0.005*menu_getScaleMenus());
         g_pRenderEngine->setColors(get_Color_ItemSelectedText());
         g_pRenderEngine->drawTextLeft(xEnd-dxPaddings, yPos+dyText, height_text, g_idFontMenu, m_szSelections[i]);
         pC[3] = f;
      }

      xEnd -= width_text+2*dxPaddings;
      g_pRenderEngine->setColors(get_Color_MenuBg());
      g_pRenderEngine->setGlobalAlfa(fAlphaLow);
      g_pRenderEngine->setStroke(get_Color_MenuText());
      if ( i != 0 )
         g_pRenderEngine->drawLine(xEnd, yPos, xEnd, yPos+m_RenderHeight);
      g_pRenderEngine->setColors(get_Color_MenuText(),1);
      g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   }

   g_pRenderEngine->setColors(get_Color_MenuBg());
   g_pRenderEngine->setGlobalAlfa(fAlphaLow);
   g_pRenderEngine->setStroke(get_Color_MenuText());
   g_pRenderEngine->setFill(0,0,0,0);
   g_pRenderEngine->drawRoundRect(xEnd, yPos-dyMargins, (x0-xEnd), m_RenderHeight+2*dyMargins, 0.01*menu_getScaleMenus());

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
}

void MenuItemSelect::RenderPopupSelections(float xPos, float yPos, bool bSelected, float fWidthSelection)
{
   if ( ! m_bCondensedOnly )
      RenderBaseTitle(xPos,yPos, bSelected, fWidthSelection);

   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_ITEMS*menu_getScaleMenus(), g_idFontMenu);

   if ( -1 == m_SelectedIndex )
      return;

   float width_text = 0.0;
   for( int i=0; i<m_SelectionsCount; i++ )
   {
      float f = g_pRenderEngine->textWidth( height_text, g_idFontMenu, m_szSelections[i]);
      if ( f > width_text )
         width_text = f;
   }
   float padding = MENU_MARGINS*menu_getScaleMenus();
   float paddingH = padding/g_pRenderEngine->getAspectRatio();
   float selectionMargin = MENU_SELECTION_PADDING*menu_getScaleMenus();
   float totalWidth = width_text + 2*paddingH + 2*selectionMargin;

   int columns = 1;
   if ( 0 < m_iCountSeparators )
   {
       columns = m_iCountSeparators+1;
       if ( 0 < m_iCountSeparators )
       if ( m_SeparatorIndexes[m_iCountSeparators-1] == m_SelectionsCount-1 )
          columns = m_iCountSeparators;
       totalWidth = (2*selectionMargin + width_text)*columns + 2*paddingH;
   }
   float xValues = xPos + m_pMenu->getUsableWidth() - width_text + 0.01*menu_getScaleMenus();
   if ( ! m_bPopupSelectorCentered )
      xValues = xPos;
   
   float heightPopup = 0.0;
   float hTmp = 0.0;
   int maxItemsInColumn = 0;
   int countInColumn = 0;
   for( int i=0; i<m_SelectionsCount; i++ )
   {
      hTmp += m_RenderHeight + MENU_ITEM_SPACING * menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS;
      countInColumn++;
      for( int c=0; c<m_iCountSeparators; c++ )
         if ( m_SeparatorIndexes[c] == i )
         {
            if ( hTmp > heightPopup )
               heightPopup = hTmp;
            if ( countInColumn > maxItemsInColumn )
               maxItemsInColumn = countInColumn;
            hTmp = 0.0;
            countInColumn = 0;
            break;
         }
   }
   if ( hTmp > heightPopup )
      heightPopup = hTmp;
   heightPopup += 2.0*padding;

   float yTopSelections = yPos - maxItemsInColumn * m_RenderHeight / 2;
   if ( yPos > 0.7 )
      yTopSelections = yPos - maxItemsInColumn * m_RenderHeight * 3 / 4;
   if ( yPos < 0.3 )
      yTopSelections = yPos - maxItemsInColumn * m_RenderHeight / 4;

   if ( heightPopup > 0.7 )
      yTopSelections = 0.5*(1.0-heightPopup);
   if ( yTopSelections + heightPopup >= 0.99 )
      yTopSelections = 0.99 - heightPopup;
   if ( yTopSelections < 0.01 )
      yTopSelections = 0.01;

   double* pColor = get_Color_MenuBg();
   float fA = pColor[3];
   pColor[3] = 0.9;
   g_pRenderEngine->setColors(pColor, 1.5);
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->drawRoundRect(xValues, yTopSelections, totalWidth, heightPopup, MENU_ROUND_MARGIN*menu_getScaleMenus());
   pColor[3] = fA;

   if ( m_bCustomTextColor )
      g_pRenderEngine->setColors(&m_TextColor[0]);
   else
      g_pRenderEngine->setColors(get_Color_MenuText());

   float y = yTopSelections + padding;
   for( int i=0; i<m_SelectionsCount; i++ )
   {
      if ( i == m_SelectedIndex )
      {
         g_pRenderEngine->setColors(get_Color_ItemSelectedBg());
         g_pRenderEngine->drawRoundRect(xValues+paddingH-selectionMargin, y - 0.8*selectionMargin, width_text+2.1*selectionMargin , m_RenderHeight + 2.0*selectionMargin, 0.01*menu_getScaleMenus());
         g_pRenderEngine->setColors(get_Color_ItemSelectedText());
         g_pRenderEngine->drawText(xValues+paddingH, y, height_text, g_idFontMenu, m_szSelections[i]);
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
      }
      else
      {
         if ( ! m_bEnabledItems[i] )
         {
            g_pRenderEngine->setColors(get_Color_ItemDisabledText());
         }
         g_pRenderEngine->drawText(xValues+paddingH, y, height_text, g_idFontMenu, m_szSelections[i]);
         if ( m_bCustomTextColor )
            g_pRenderEngine->setColors(&m_TextColor[0]);
         else
            g_pRenderEngine->setColors(get_Color_MenuText());
      }
      y += m_RenderHeight;
      y += MENU_ITEM_SPACING * menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS;
      for( int c=0; c<m_iCountSeparators; c++ )
         if ( m_SeparatorIndexes[c] == i )
         {
            y = yTopSelections + padding;
            xValues += width_text + 2.0*selectionMargin;
            break;
         }
   }
}

