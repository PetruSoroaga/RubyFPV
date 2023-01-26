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

#include "menu_objects.h"
#include "menu_search.h"
#include "../base/base.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/ctrl_interfaces.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"

#include "../renderer/render_engine.h"
#include "colors.h"
#include "osd_common.h"
#include "menu.h"
#include <math.h>
#include "menu_preferences_buttons.h"
#include "menu_preferences_ui.h"
#include "menu_confirmation.h"
#include "shared_vars.h"
#include "popup.h"
#include "pairing.h"
#include "ruby_central.h"
#include "render_commands.h"
#include "timers.h"
#include "process_router_messages.h"

float MENU_TEXTLINE_SPACING = 0.1; // percent of item text height
float MENU_ITEM_SPACING = 0.3;  // percent of item text height

static bool s_bMenuObjectsRenderEndItems = false;

Menu::Menu(int id, const char* title, const char* subTitle)
{
   m_pParent = NULL;
   m_MenuId = id;
   m_MenuDepth = 0;
   m_iColumnsCount = 1;
   m_bEnableColumnSelection = true;
   m_bIsSelectingInsideColumn = false;
   m_bFullWidthSelection = false;
   m_bFirstShow = true;
   m_ItemsCount = 0;
   m_SelectedIndex = 0;
   m_Height = 0.0;
   m_ExtraHeight = 0;
   m_RenderTotalHeight = 0;
   m_RenderHeight = 0;
   m_RenderWidth = 0;
   m_RenderHeaderHeight = 0;
   m_RenderTitleHeight = 0.0;
   m_RenderSubtitleHeight = 0.0;
   m_RenderFooterHeight = 0.0;
   m_RenderTooltipHeight = 0.0;
   m_RenderMaxFooterHeight = 0.0;
   m_RenderMaxTooltipHeight = 0.0;
   m_fRenderScrollOffsetY = 0.0;
   m_fRenderScrollMaxY = 0.0;
   m_fRenderScrollBarsExtraWidth = 0.02;
   m_fRenderItemsStartYPos = 0.0;
   m_fRenderItemsMaxHeight = 0.0;
   m_bEnableScrolling = true;
   m_bHasScrolling = false;

   m_fSelectionWidth = 0;

   m_fAlfaWhenInBackground = MENU_ALFA_WHEN_IN_BG;
   Preferences* pP = get_Preferences();
   if ( NULL != pP && (! pP->iMenusStacked) )
      m_fAlfaWhenInBackground = MENU_ALFA_WHEN_IN_BG*2.3;

   m_szCurrentTooltip[0] = 0;
   m_szTitle[0] = 0;
   m_szSubTitle[0] = 0;
   if ( NULL != title )
      strcpy( m_szTitle, title );
   if ( NULL != subTitle )
      strcpy( m_szSubTitle, subTitle );

   m_TopLinesCount = 0;
   m_bInvalidated = true;
   m_szTopIcon[0] = 0;
   m_fIconSize = 0.0;

   m_uOnShowTime = 0;
   m_uOnChildAddTime = 0;
   m_uOnChildCloseTime = 0;
   m_bHasChildMenuActive = false;
   m_bAnimating = false;
   m_iConfirmationId = 0;
   m_xPosOriginal = -1.0;
   m_bHasConfirmationOnTop = false;
}

Menu::~Menu()
{
   m_bHasChildMenuActive = false;
   removeAllItems();
   removeAllTopLines();
}

void Menu::setParent(Menu* pParent)
{
   m_pParent = pParent;
}

void Menu::invalidate()
{
   valuesToUI();
   m_bInvalidated = true;
   for( int i=0; i<m_ItemsCount; i++ )
      m_pMenuItems[i]->invalidate();
}

void Menu::disableScrolling()
{
   m_bEnableScrolling = false;
   m_bHasScrolling = false;
   invalidate();
}

void Menu::setTitle(const char* szTitle)
{
   if ( NULL == szTitle )
      m_szTitle[0] = 0;
   else
      strcpy(m_szTitle, szTitle);
   m_bInvalidated = true;
}

void Menu::setSubTitle(const char* szSubTitle)
{
   if ( NULL == szSubTitle )
      m_szSubTitle[0] = 0;
   else
      strcpy(m_szSubTitle, szSubTitle);
   m_bInvalidated = true;
}

void Menu::removeAllTopLines()
{
   for( int i=0; i<m_TopLinesCount; i++ )
   {
      if ( NULL != m_szTopLines[i] )
         free(m_szTopLines[i]);
   }
   m_TopLinesCount = 0;
   m_bInvalidated = true;
}

void Menu::addTopLine(const char* szLine, float dx)
{
   if ( m_TopLinesCount >= MENU_MAX_TOP_LINES-1 || NULL == szLine )
      return;
   m_fTopLinesDX[m_TopLinesCount] = dx;
   m_szTopLines[m_TopLinesCount] = (char*)malloc(strlen(szLine)+1);
   strcpy(m_szTopLines[m_TopLinesCount], szLine);

   if ( strlen(m_szTopLines[m_TopLinesCount]) > 1 )
   if ( m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] == 10 || 
        m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] == 13 )
      m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] = 0;
   if ( strlen(m_szTopLines[m_TopLinesCount]) > 1 )
   if ( m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] == 10 || 
        m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] == 13 )
      m_szTopLines[m_TopLinesCount][strlen(m_szTopLines[m_TopLinesCount])-1] = 0;

   m_TopLinesCount++;
   m_bInvalidated = true;
}

void Menu::setColumnsCount(int count)
{
   m_iColumnsCount = count;
}

void Menu::enableColumnSelection(bool bEnable)
{
   m_bEnableColumnSelection = bEnable;
}

void Menu::removeAllItems()
{
   for( int i=0; i<m_ItemsCount; i++ )
      if ( NULL != m_pMenuItems[i] )
         delete m_pMenuItems[i];
   m_ItemsCount = 0;
   m_bInvalidated = true;
}

int Menu::addMenuItem(MenuItem* item)
{
   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i] == item )
         return -1;
   }

   m_pMenuItems[m_ItemsCount] = item;
   m_bHasSeparatorAfter[m_ItemsCount] = false;
   m_pMenuItems[m_ItemsCount]->m_pMenu = this;
   m_ItemsCount++;
   m_bInvalidated = true;
   if ( -1 == m_SelectedIndex )
   if ( item->isEnabled() )
      m_SelectedIndex = 0;
   return m_ItemsCount-1;
}

void Menu::addSeparator()
{
   if ( 0 == m_ItemsCount )
      return;
   m_bHasSeparatorAfter[m_ItemsCount-1] = true;
}

void Menu::setTooltip(const char* szTooltip)
{
   if ( NULL == szTooltip || 0 == szTooltip[0] )
      m_szCurrentTooltip[0] = 0;
   else
      strcpy(m_szCurrentTooltip, szTooltip);
   m_bInvalidated = true;
}

void Menu::setTopIcon(const char* szIcon)
{
   if ( NULL != szIcon )
      strcpy(m_szTopIcon, szIcon);
   else
      m_szTopIcon[0] = 0;
}

void Menu::enableMenuItem(int index, bool enable)
{
   if ( NULL != m_pMenuItems[index] )
        m_pMenuItems[index]->setEnabled(enable);
   m_bInvalidated = true;
}

int Menu::getConfirmationId()
{
   return m_iConfirmationId;
}

u32 Menu::getOnShowTime()
{
   return m_uOnShowTime;
}

u32 Menu::getOnChildAddTime()
{
   return m_uOnChildAddTime;
}

u32 Menu::getOnReturnFromChildTime()
{
   return m_uOnChildCloseTime;
}

float Menu::getRenderWidth()
{
   return m_RenderWidth;
}

void Menu::onShow()
{
   if ( m_bFirstShow )
      m_xPosOriginal = m_xPos;

   //log_line("Menu [%s] on show (%s): xPos: %.2f, xRenderPos: %.2f, xPos Original: %.2f", m_szTitle, m_bFirstShow? "first show":"not first show", m_xPos, m_RenderXPos, m_xPosOriginal);

   m_bFirstShow = false;
   m_uOnShowTime = g_TimeNow;

   if ( m_MenuId == MENU_ID_SIMPLE_MESSAGE )
   {
      removeAllItems();
      addMenuItem(new MenuItem("OK",""));
   }
   valuesToUI();

   m_SelectedIndex = 0;
   if ( 0 == m_ItemsCount )
      m_SelectedIndex = -1;
   else
   {
      while ( m_SelectedIndex < m_ItemsCount && NULL != m_pMenuItems[m_SelectedIndex] && ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) )
         m_SelectedIndex++;
   }
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = -1;
   onFocusedItemChanged();
   m_bInvalidated = true;

}

bool Menu::periodicLoop()
{
   return false;
}

float Menu::getSelectionWidth()
{
   return m_fSelectionWidth;
}

float Menu::getUsableWidth()
{
   m_fIconSize = 0.0;
   if ( 0 != m_szTopIcon[0] )
      m_fIconSize = 0.04*menu_getScaleMenus();

   m_RenderWidth = m_Width*menu_getScaleMenus();
   if ( (m_iColumnsCount > 1) && (!m_bEnableColumnSelection) )
      return (m_RenderWidth - m_fIconSize - (2*MENU_MARGINS*menu_getScaleMenus()+3*MENU_MARGINS*menu_getScaleMenus()/(float)(m_iColumnsCount-1))/g_pRenderEngine->getAspectRatio())/(float)m_iColumnsCount;
   else
      return (m_RenderWidth - m_fIconSize - 2*MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio());
}

void Menu::computeRenderSizes()
{
   m_fIconSize = 0.0;
   if ( 0 != m_szTopIcon[0] )
      m_fIconSize = 0.04*menu_getScaleMenus();

   m_RenderXPos = m_xPos;
   m_RenderYPos = m_yPos;
   m_RenderWidth = m_Width*menu_getScaleMenus();
   m_RenderHeight = m_Height*menu_getScaleMenus();
   m_RenderTotalHeight = m_RenderHeight;
   m_RenderHeaderHeight = 0.0;
   m_RenderTitleHeight = 0.0;
   m_RenderSubtitleHeight = 0.0;

   m_RenderFooterHeight = 0.0;
   m_RenderTooltipHeight = 0.0;
   m_RenderMaxFooterHeight = 0.0;
   m_RenderMaxTooltipHeight = 0.0;

   if ( 0 != m_szTitle[0] )
   {
      m_RenderTitleHeight = g_pRenderEngine->textHeight(menu_getScaleMenus()*MENU_FONT_SIZE_TITLE, g_idFontMenu);
      m_RenderHeaderHeight += m_RenderTitleHeight;
      m_RenderHeaderHeight += 1.0*MENU_MARGINS*menu_getScaleMenus();
   }
   if ( 0 != m_szSubTitle[0] )
   {
      m_RenderSubtitleHeight = g_pRenderEngine->textHeight(menu_getScaleMenus()*MENU_FONT_SIZE_SUBTITLE, g_idFontMenu);
      m_RenderHeaderHeight += m_RenderSubtitleHeight;
      m_RenderHeaderHeight += 0.4*MENU_MARGINS*menu_getScaleMenus();
   }

   if ( 0 != m_szCurrentTooltip[0] )
   {
       m_RenderTooltipHeight = g_pRenderEngine->getMessageHeight(m_szCurrentTooltip, menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenuSmall);
       m_RenderFooterHeight = 0.6*MENU_MARGINS*menu_getScaleMenus();
       m_RenderFooterHeight += m_RenderTooltipHeight;
       m_RenderFooterHeight += 0.2*MENU_MARGINS*menu_getScaleMenus();
       m_RenderMaxFooterHeight = m_RenderFooterHeight;
       m_RenderMaxTooltipHeight = m_RenderTooltipHeight;
   }

   if ( m_Height < 0.001 )
   {
       m_RenderTotalHeight = m_ExtraHeight;
       m_RenderTotalHeight += m_RenderHeaderHeight;
       m_RenderTotalHeight += 0.5*MENU_MARGINS*menu_getScaleMenus();

       for (int i = 0; i<m_TopLinesCount; i++)
       {
           if ( 0 == strlen(m_szTopLines[i]) )
              m_RenderTotalHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE;
           else
           {
              m_RenderTotalHeight += g_pRenderEngine->getMessageHeight(m_szTopLines[i], menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth() - m_fTopLinesDX[i], g_idFontMenu);
              m_RenderTotalHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE * MENU_TEXTLINE_SPACING;
           }
       }
       if ( 0 < m_TopLinesCount )
          m_RenderTotalHeight += 0.5*MENU_MARGINS*menu_getScaleMenus();

       float yTemp = m_RenderTotalHeight;

       for( int i=0; i<m_ItemsCount; i++ )
       {
          if ( NULL == m_pMenuItems[i] )
             continue;
          if ( m_iColumnsCount < 2 )
          {
             m_RenderTotalHeight += m_pMenuItems[i]->getItemHeight(getUsableWidth());
             m_RenderTotalHeight += MENU_ITEM_SPACING * menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS;
             if ( m_bHasSeparatorAfter[i] )
                m_RenderTotalHeight += menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS * MENU_SEPARATOR_HEIGHT;
          }
          else if ( (i%m_iColumnsCount) == 0 )
          {
             m_RenderTotalHeight += m_pMenuItems[i]->getItemHeight(getUsableWidth());
             m_RenderTotalHeight += MENU_ITEM_SPACING * menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS;
             if ( m_bHasSeparatorAfter[i] )
                m_RenderTotalHeight += menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS * MENU_SEPARATOR_HEIGHT;
          }

          if ( NULL != m_pMenuItems[i]->getTooltip() )
          {
             float fHTooltip = g_pRenderEngine->getMessageHeight(m_pMenuItems[i]->getTooltip(), menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
             if ( fHTooltip > m_RenderMaxTooltipHeight )
                m_RenderMaxTooltipHeight = fHTooltip;
          }       
       }

       m_fRenderItemsMaxHeight = m_RenderTotalHeight - yTemp + m_ExtraHeight;
       m_RenderMaxFooterHeight = 0.6*MENU_MARGINS*menu_getScaleMenus();
       m_RenderMaxFooterHeight += m_RenderMaxTooltipHeight;
       m_RenderMaxFooterHeight += 0.2*MENU_MARGINS*menu_getScaleMenus();

       m_RenderTotalHeight += 0.4*MENU_MARGINS*menu_getScaleMenus();
       m_RenderTotalHeight += m_RenderFooterHeight;

       m_RenderHeight = m_RenderTotalHeight;
       float fMaxHeight = 0.9;
       if ( m_TopLinesCount > 10 )
          fMaxHeight = 0.94;
       if ( (m_iColumnsCount < 2) && ( m_RenderYPos + (m_RenderHeight-m_RenderFooterHeight) + m_RenderMaxFooterHeight > 0.9 ) )
       {
          if ( m_TopLinesCount > 10 )
             m_RenderYPos = 0.1;
          else
          {
             float fYOverflow = (m_RenderYPos + (m_RenderHeight-m_RenderFooterHeight) + m_RenderMaxFooterHeight) - 0.9;
             if ( fYOverflow <= (m_RenderYPos-0.14) )
             {
                m_RenderYPos -= fYOverflow;
                m_bHasScrolling = false;
                m_fRenderScrollOffsetY = 0.0;
             }
             else
             {
                if ( m_RenderYPos > 0.16 )
                {
                   float dMoveY = m_RenderYPos - 0.16;
                   m_RenderYPos -= dMoveY;
                   fYOverflow -= dMoveY;
                   m_RenderHeight += dMoveY;
                }
                
                m_bHasScrolling = true;
                float yTmp = m_RenderHeight;
                m_RenderHeight = 0.9 - m_RenderYPos - m_RenderMaxFooterHeight + m_RenderFooterHeight;
                m_fRenderItemsMaxHeight -= (yTmp - m_RenderHeight) + 0.4*MENU_MARGINS*menu_getScaleMenus();
                m_fRenderScrollMaxY = (yTmp - m_RenderHeight) + 0.4*MENU_MARGINS*menu_getScaleMenus();
             }
          }
       }
       else
       {
          m_bHasScrolling = false;
          m_fRenderScrollOffsetY = 0.0;
       }
   }
   else
   {
      m_bHasScrolling = false;
      m_fRenderScrollOffsetY = 0.0;

      for( int i=0; i<m_ItemsCount; i++ )
         if ( NULL != m_pMenuItems[i] )
            m_pMenuItems[i]->getItemHeight(getUsableWidth());
   }

   m_fSelectionWidth = 0;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      float w = 0;
      if ( NULL != m_pMenuItems[i] )
      {
         if ( m_pMenuItems[i]->isSelectable() )
            w = m_pMenuItems[i]->getTitleWidth(getUsableWidth());
         else
            m_pMenuItems[i]->getTitleWidth(getUsableWidth());
         m_pMenuItems[i]->getValueWidth(getUsableWidth());
      }
      if ( m_fSelectionWidth < w )
         m_fSelectionWidth = w;
   }
   if ( m_bFullWidthSelection )
      m_fSelectionWidth = getUsableWidth();
   m_bInvalidated = false;
}


void Menu::RenderPrepare()
{
   Preferences* pP = get_Preferences();
   if ( ! pP->iMenusStacked )
   {
      // TO FIx ??
      //if ( m_bHasChildMenuActive )
      //   m_xPos = m_xPosOriginal - m_Width*0.2;
      //else
      //   m_xPos = m_xPosOriginal;
      return;
   }

   if ( menu_has_menu_confirmation_above(this) )
   {
      m_bHasConfirmationOnTop = true;
      m_xPos = m_xPosOriginal;
      //log_line("Menu [%s] reset xpos to original (%s): xPos: %.2f, xRenderPos: %.2f, xPos Original: %.2f", m_szTitle, m_bFirstShow? "first show":"not first show", m_xPos, m_RenderXPos, m_xPosOriginal);
      return;
   }

   u32 t = g_TimeNow;
   float xPosEnd = -0.09 + 0.045*m_MenuDepth;

   if ( m_bAnimating )
   if ( ! m_bHasConfirmationOnTop )
   if ( m_bHasChildMenuActive )
   {
      int animTime = 280;
      float f = (t-m_uOnChildAddTime)/(float)animTime;
      if ( f < 0.0 ) f = 0.0;
      if ( f > 1.0 )
      {
         f = 1.0;
         m_bAnimating = false;
      }
      m_xPos = m_xPosOriginal - (m_xPosOriginal-xPosEnd)*f;
      m_bInvalidated = true;
      //log_line("Menu [%s] set xpos on animate child active (%s): xPos: %.2f, xRenderPos: %.2f, xPos Original: %.2f", m_szTitle, m_bFirstShow? "first show":"not first show", m_xPos, m_RenderXPos, m_xPosOriginal);
   }
   
   if ( m_bAnimating )
   if ( ! m_bHasConfirmationOnTop )
   if ( m_uOnChildCloseTime > m_uOnChildAddTime )
   {
      int animTime = 200;
      float f = (t-m_uOnChildCloseTime)/(float)animTime;
      if ( f < 0.0 ) f = 0.0;
      if ( f > 1.0 )
      {
         f = 1.0;
         m_bAnimating = false;
         m_xPos = m_xPosOriginal;
         m_uOnChildCloseTime = 0;
      }
      m_xPos = xPosEnd + (m_xPosOriginal-xPosEnd)*f;
      m_bInvalidated = true;
      //log_line("Menu [%s] set xpos on child close (%s): xPos: %.2f, xRenderPos: %.2f, xPos Original: %.2f", m_szTitle, m_bFirstShow? "first show":"not first show", m_xPos, m_RenderXPos, m_xPosOriginal);
   }
}

void Menu::Render()
{
   RenderPrepare();

   float yTop = RenderFrameAndTitle();
   float yPos = yTop;

   s_bMenuObjectsRenderEndItems = false;
   for( int i=0; i<m_ItemsCount; i++ )
      yPos += RenderItem(i,yPos);

   RenderEnd(yTop);
   g_pRenderEngine->setColors(get_Color_MenuText());
}

void Menu::RenderEnd(float yPos)
{
   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i]->isEditing() )
      {
         s_bMenuObjectsRenderEndItems = true;
         RenderItem(i,m_pMenuItems[i]->m_RenderLastY, m_pMenuItems[i]->m_RenderLastX - (m_RenderXPos + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio()));
         s_bMenuObjectsRenderEndItems = false;
      }
   }
}


float Menu::RenderFrameAndTitle()
{
   if ( m_bInvalidated )
   {
      computeRenderSizes();
      m_bInvalidated = false;
   }

   static double topTitleBgColor[4] = {90,2,20,0.45};

   g_pRenderEngine->setColors(get_Color_MenuBg());
   g_pRenderEngine->setStroke(get_Color_MenuBorder());

   float fExtraWidth = 0.0;
   if ( m_bEnableScrolling && m_bHasScrolling )
      fExtraWidth = m_fRenderScrollBarsExtraWidth;
   g_pRenderEngine->drawRoundRect(m_RenderXPos, m_RenderYPos, m_RenderWidth + fExtraWidth, m_RenderHeight, MENU_ROUND_MARGIN*menu_getScaleMenus());
   g_pRenderEngine->drawLine(m_RenderXPos, m_RenderYPos + m_RenderHeaderHeight, m_RenderXPos + m_RenderWidth + fExtraWidth, m_RenderYPos + m_RenderHeaderHeight);

   g_pRenderEngine->setColors(topTitleBgColor);
   g_pRenderEngine->drawRoundRect(m_RenderXPos, m_RenderYPos, m_RenderWidth + fExtraWidth, m_RenderHeaderHeight, MENU_ROUND_MARGIN*menu_getScaleMenus());

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   float yPos = m_RenderYPos + 0.5*MENU_MARGINS*menu_getScaleMenus();
   g_pRenderEngine->drawText(m_RenderXPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio(), yPos, MENU_FONT_SIZE_TITLE*menu_getScaleMenus(), g_idFontMenu, m_szTitle);
   yPos += m_RenderTitleHeight;

   if ( 0 != m_szSubTitle[0] )
   {
      yPos += 0.4*MENU_MARGINS*menu_getScaleMenus();
      g_pRenderEngine->drawText(m_RenderXPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio(), yPos, MENU_FONT_SIZE_SUBTITLE*menu_getScaleMenus(), g_idFontMenu, m_szSubTitle);
      yPos += m_RenderSubtitleHeight;
   }

   yPos = m_RenderYPos + m_RenderHeaderHeight + 0.5*MENU_MARGINS*menu_getScaleMenus();;

   for (int i=0; i<m_TopLinesCount; i++)
   {
      if ( 0 == strlen(m_szTopLines[i]) )
        yPos += menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE;
      else
      {
         yPos += g_pRenderEngine->drawMessageLines(m_RenderXPos + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() + m_fIconSize + m_fTopLinesDX[i], yPos, m_szTopLines[i], MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus(), MENU_TEXTLINE_SPACING, getUsableWidth()-m_fTopLinesDX[i], g_idFontMenu);
         yPos += menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE * MENU_TEXTLINE_SPACING;
      }
   }
   if ( 0 < m_TopLinesCount )
      yPos += 0.5*MENU_MARGINS*menu_getScaleMenus();

   m_fRenderItemsStartYPos = yPos;

   if ( m_bEnableScrolling && m_bHasScrolling )
   {
      float wPixel = g_pRenderEngine->getPixelWidth();
      float fScrollH = m_RenderHeight - (yPos-m_RenderYPos) - m_RenderFooterHeight - 2.0*MENU_MARGINS*menu_getScaleMenus();
      float fScrollBarH = fScrollH*0.2;
      float xPos = m_RenderXPos + m_RenderWidth + fExtraWidth - 0.2*MENU_MARGINS*menu_getScaleMenus() - fExtraWidth*0.5;
      double* pC = get_Color_MenuText();
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], 0.2);
      g_pRenderEngine->drawRect( xPos - wPixel*2.0, yPos, wPixel*4.0, fScrollH);

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], 0.5);
      float fP = m_fRenderScrollOffsetY / (m_fRenderScrollMaxY-m_ExtraHeight);
      if ( fP > 1.0 )
         fP = 1.0;
      float y = yPos + (fScrollH-fScrollBarH) * fP;
      g_pRenderEngine->drawRoundRect(xPos - 0.25*fExtraWidth, y, fExtraWidth*0.5, fScrollBarH, MENU_ROUND_MARGIN*menu_getScaleMenus());
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   }

   if ( 0 != m_szCurrentTooltip[0] )
   {
      static double bottomTooltipBgColor[4] = {250,30,50,0.1};
      g_pRenderEngine->setColors(bottomTooltipBgColor);
      g_pRenderEngine->drawRoundRect(m_RenderXPos, m_RenderYPos + m_RenderHeight - m_RenderFooterHeight, m_RenderWidth + fExtraWidth, m_RenderFooterHeight, MENU_ROUND_MARGIN*menu_getScaleMenus());

      float yTooltip = m_RenderYPos+m_RenderHeight-m_RenderFooterHeight;
      yTooltip += 0.4 * MENU_MARGINS*menu_getScaleMenus();

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->drawMessageLines( m_RenderXPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio(), yTooltip, m_szCurrentTooltip, menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenuSmall);

      g_pRenderEngine->setColors(get_Color_MenuBg());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      g_pRenderEngine->drawLine(m_RenderXPos, m_RenderYPos + m_RenderHeight - m_RenderFooterHeight, m_RenderXPos + m_RenderWidth + fExtraWidth, m_RenderYPos + m_RenderHeight - m_RenderFooterHeight);
   }

   return m_fRenderItemsStartYPos - m_fRenderScrollOffsetY;
}

float Menu::RenderItem(int index, float yPos, float dx)
{
   if ( NULL == m_pMenuItems[index] )
      return 0.0;

   m_pMenuItems[index]->setLastRenderPos(m_RenderXPos + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() + dx, yPos);

   float fHeight = m_pMenuItems[index]->getItemHeight(getUsableWidth());
   if ( m_bHasSeparatorAfter[index] )
      fHeight += menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS * MENU_SEPARATOR_HEIGHT;
   fHeight += MENU_ITEM_SPACING * menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS;

   if ( m_bEnableScrolling && m_bHasScrolling )
   {
      if ( yPos < m_fRenderItemsStartYPos-0.01 )
         return fHeight;
      if ( yPos + m_pMenuItems[index]->getItemHeight(getUsableWidth()) > m_fRenderItemsStartYPos + m_fRenderItemsMaxHeight+0.01 )
         return fHeight;
   }

   m_pMenuItems[index]->Render(m_RenderXPos + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() + dx, yPos, index == m_SelectedIndex, m_fSelectionWidth);
   float h = m_pMenuItems[index]->getItemHeight(getUsableWidth());

   if ( m_bHasSeparatorAfter[index] && (!s_bMenuObjectsRenderEndItems) )
   {
      g_pRenderEngine->setColors(get_Color_MenuBg());
      g_pRenderEngine->setStroke(get_Color_MenuBorder());
      float yLine = yPos+h+0.55*(MENU_ITEM_SPACING * menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS + menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS * MENU_SEPARATOR_HEIGHT);
      g_pRenderEngine->drawLine(m_RenderXPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio()+dx, yLine, m_RenderXPos+m_RenderWidth - MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio(), yLine);

      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(0);
   }

   if ( m_bHasSeparatorAfter[index] )
      h += menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS * MENU_SEPARATOR_HEIGHT;

   g_pRenderEngine->setColors(get_Color_MenuText());

   return fHeight;
}

void Menu::updateScrollingOnSelectionChange()
{
   if ( ! m_bEnableScrolling )
      return;
   if ( ! m_bHasScrolling )
      return;
   if ( m_fRenderItemsStartYPos < 0.001 || m_fRenderItemsMaxHeight < 0.001 )
      return;

   float yPosItem = m_fRenderItemsStartYPos;
   yPosItem -= m_fRenderScrollOffsetY;

   int countDisabledAfterSelection = 0;
   while ( m_SelectedIndex+countDisabledAfterSelection+1 < m_ItemsCount )
   {
      if ( m_pMenuItems[m_SelectedIndex+countDisabledAfterSelection+1]->m_bEnabled )
         break;
      countDisabledAfterSelection++;
   }

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( i == m_SelectedIndex || i == m_SelectedIndex-1 || i == m_SelectedIndex-2 )
      if ( yPosItem < m_fRenderItemsStartYPos+0.02 )
      {
         float dy = (m_fRenderItemsStartYPos+0.02 - yPosItem);
         float yTmp = m_fRenderScrollOffsetY;
         m_fRenderScrollOffsetY -= dy;
         if ( m_fRenderScrollOffsetY < 0 )
            m_fRenderScrollOffsetY = 0;
         yPosItem += (yTmp - m_fRenderScrollOffsetY);
      }

      float hItem = m_pMenuItems[i]->getItemHeight(getUsableWidth());
      yPosItem += hItem;
      if ( m_bHasSeparatorAfter[i] && (!s_bMenuObjectsRenderEndItems) )
         yPosItem += menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS * MENU_SEPARATOR_HEIGHT;
      yPosItem += MENU_ITEM_SPACING * menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS;

      bool bCheckLowLimit = false;
      if ( i == m_SelectedIndex || i == m_SelectedIndex+1 || i == m_SelectedIndex+2 )
         bCheckLowLimit = true;
      if ( i == m_SelectedIndex + countDisabledAfterSelection )
         bCheckLowLimit = true;

      if ( bCheckLowLimit )
      if ( yPosItem > m_fRenderItemsStartYPos + m_fRenderItemsMaxHeight-0.02 )
      {
         float dy = yPosItem - (m_fRenderItemsStartYPos + m_fRenderItemsMaxHeight-0.02);
         m_fRenderScrollOffsetY += dy;
         yPosItem -= dy;
         if ( m_fRenderScrollMaxY < m_fRenderScrollOffsetY )
            m_fRenderScrollMaxY = m_fRenderScrollOffsetY;
    
      }
   }
}


int Menu::onBack()
{
   if ( m_MenuId == 0 ) // simple message menu? just pop it and return.
   {
      menu_stack_pop();
      return 1;
   }

   for( int i=0; i<m_ItemsCount; i++ )
      if ( m_pMenuItems[i]->isSelectable() && m_pMenuItems[i]->isEditing() )
      {
         m_pMenuItems[i]->endEdit(true);
         onItemValueChanged(i);
         onItemEndEdit(i);
         return 1;
      }

   if ( m_iColumnsCount > 1 && m_bEnableColumnSelection && m_bIsSelectingInsideColumn )
   {
      m_bIsSelectingInsideColumn = false;
      m_SelectedIndex = ((int)(m_SelectedIndex/m_iColumnsCount))*m_iColumnsCount;
      onFocusedItemChanged();
      return 1;
   }
   return 0;
}

void Menu::onSelectItem()
{
   if ( m_MenuId == MENU_ID_SIMPLE_MESSAGE ) // simple message menu? just pop it and return.
   {
      menu_stack_pop();
      return;
   }
   if ( m_SelectedIndex < 0 || m_SelectedIndex >= m_ItemsCount )
      return;
   if ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() )
      return;

   if ( m_pMenuItems[m_SelectedIndex]->isEditable() )
   {
      MenuItem* pSelectedItem = m_pMenuItems[m_SelectedIndex];
      if ( pSelectedItem->isEditing() )
      {
         pSelectedItem->endEdit(false);
         onItemEndEdit(m_SelectedIndex);
      }
      else
         pSelectedItem->beginEdit();
   }
   else
   {
      if ( m_iColumnsCount > 1 && m_bEnableColumnSelection && (!m_bIsSelectingInsideColumn) )
      {
         m_bIsSelectingInsideColumn = true;
         m_SelectedIndex++;
         onFocusedItemChanged();
      }
      else
         m_pMenuItems[m_SelectedIndex]->onClick();
   }
}


void Menu::onMoveUp(bool bIgnoreReversion)
{
   Preferences* p = get_Preferences();
   if ( m_MenuId == 0 ) // simple message menu? just pop it and return.
   {
      menu_stack_pop();
      return;
   }
   if ( 0 == m_ItemsCount )
      return;

   // Check for edit first
   for( int i=0; i<m_ItemsCount; i++ )
      if ( m_pMenuItems[i]->isSelectable() && m_pMenuItems[i]->isEditing() )
      {
         if ( bIgnoreReversion || p->iSwapUpDownButtonsValues == 0 )
         {
            m_pMenuItems[i]->onKeyUp(bIgnoreReversion);
            if ( isKeyMinusLongPressed() )
               for(int k=0; k<0; k++ )
                  m_pMenuItems[i]->onKeyUp(bIgnoreReversion);
            if ( isKeyMinusLongLongPressed() )
               for(int k=0; k<1; k++ )
                  m_pMenuItems[i]->onKeyUp(bIgnoreReversion);
         }
         else
         {
            m_pMenuItems[i]->onKeyDown(bIgnoreReversion);
            if ( isKeyMinusLongPressed() )
               for(int k=0; k<0; k++ )
                  m_pMenuItems[i]->onKeyDown(bIgnoreReversion);
            if ( isKeyMinusLongLongPressed() )
               for(int k=0; k<1; k++ )
                  m_pMenuItems[i]->onKeyDown(bIgnoreReversion);
         }
         onItemValueChanged(i);
         return;
      }

   // Check if current selected items wants to preprocess up/down/left/right
   if ( m_SelectedIndex >= 0 && m_SelectedIndex < m_ItemsCount && NULL != m_pMenuItems[m_SelectedIndex] && m_pMenuItems[m_SelectedIndex]->isEnabled() )
   if ( m_pMenuItems[m_SelectedIndex]->preProcessKeyUp() )
   {
      onItemValueChanged(m_SelectedIndex);
      return;
   }

   int loopCount = 0;

   if ( m_iColumnsCount > 1 && m_bEnableColumnSelection && m_bIsSelectingInsideColumn )
   {
      int indexStartColumn = 1 + ((int)(m_SelectedIndex/m_iColumnsCount))*m_iColumnsCount;
      int indexEndColumn = indexStartColumn + m_iColumnsCount - 2;
      do
      {
      loopCount++;
      if ( m_SelectedIndex > indexStartColumn )
         m_SelectedIndex--;
      else
         m_SelectedIndex = indexEndColumn;
      }
      while ( (loopCount < (m_iColumnsCount+2)*2) && NULL != m_pMenuItems[m_SelectedIndex] && ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) );
   }
   else
   {
      if ( m_bEnableColumnSelection )
      {
         do
         {
            loopCount++;
            if ( m_SelectedIndex >= m_iColumnsCount )
               m_SelectedIndex -= m_iColumnsCount;
            else
            {
               m_SelectedIndex = m_ItemsCount-1;
               m_SelectedIndex = ((int)(m_SelectedIndex/m_iColumnsCount))*m_iColumnsCount;
            }
         }
         while ( (loopCount < (m_ItemsCount+2)*2) && NULL != m_pMenuItems[m_SelectedIndex] && ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) );
      }
      else
      {
         do
         {
            loopCount++;
            if ( m_SelectedIndex >= 1 )
               m_SelectedIndex--;
            else
               m_SelectedIndex = m_ItemsCount-1;
         }
         while ( (loopCount < (m_ItemsCount+2)*2) && NULL != m_pMenuItems[m_SelectedIndex] && ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) );
      }
   }
   if ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() )
      m_SelectedIndex = -1;
   onFocusedItemChanged();
}

void Menu::onMoveDown(bool bIgnoreReversion)
{
   Preferences* p = get_Preferences();
   if ( m_MenuId == 0 ) // simple message menu? just pop it and return.
   {
      menu_stack_pop();
      return;
   }
   if ( 0 == m_ItemsCount )
      return;

   // Check for edit first

   for( int i=0; i<m_ItemsCount; i++ )
      if ( m_pMenuItems[i]->isSelectable() && m_pMenuItems[i]->isEditing() )
      {
         if ( bIgnoreReversion || p->iSwapUpDownButtonsValues == 0 )
         {
            m_pMenuItems[i]->onKeyDown(bIgnoreReversion);
            if ( isKeyPlusLongPressed() )
               for(int k=0; k<0; k++ )
                  m_pMenuItems[i]->onKeyDown(bIgnoreReversion);
            if ( isKeyPlusLongLongPressed() )
               for(int k=0; k<1; k++ )
                  m_pMenuItems[i]->onKeyDown(bIgnoreReversion);
         }
         else
         {
            m_pMenuItems[i]->onKeyUp(bIgnoreReversion);
            if ( isKeyPlusLongPressed() )
               for(int k=0; k<0; k++ )
                  m_pMenuItems[i]->onKeyUp(bIgnoreReversion);
            if ( isKeyPlusLongLongPressed() )
               for(int k=0; k<1; k++ )
                  m_pMenuItems[i]->onKeyUp(bIgnoreReversion);
         }
         onItemValueChanged(i);
         return;
      }

   // Check if current selected items wants to preprocess up/down/left/right
   if ( m_SelectedIndex >= 0 && m_SelectedIndex < m_ItemsCount && NULL != m_pMenuItems[m_SelectedIndex] && m_pMenuItems[m_SelectedIndex]->isEnabled() )
   if ( m_pMenuItems[m_SelectedIndex]->preProcessKeyDown() )
   {
      onItemValueChanged(m_SelectedIndex);
      return;
   }

   int loopCount = 0;

   if ( m_iColumnsCount > 1 && m_bEnableColumnSelection && m_bIsSelectingInsideColumn )
   {
      int indexStartColumn = 1 + ((int)(m_SelectedIndex/m_iColumnsCount))*m_iColumnsCount;
      int indexEndColumn = indexStartColumn + m_iColumnsCount - 2;
      do
      {
      loopCount++;
      if ( m_SelectedIndex < indexEndColumn )
         m_SelectedIndex++;
      else
         m_SelectedIndex = indexStartColumn;
      }
      while ( (loopCount < (m_iColumnsCount+2)*2) && NULL != m_pMenuItems[m_SelectedIndex] && ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) );
   }
   else
   {
      if ( m_bEnableColumnSelection )
      {
         do
         {
            loopCount++;
            if ( m_SelectedIndex < m_ItemsCount-m_iColumnsCount )
               m_SelectedIndex += m_iColumnsCount;
            else
               m_SelectedIndex = 0;
         }
         while ( (loopCount < (m_ItemsCount+2)*2) && NULL != m_pMenuItems[m_SelectedIndex] && ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) );
      }
      else
      {
         do
         {
            loopCount++;
            if ( m_SelectedIndex < m_ItemsCount-1 )
               m_SelectedIndex++;
            else
               m_SelectedIndex = 0;
         }
         while ( (loopCount < (m_ItemsCount+2)*2) && NULL != m_pMenuItems[m_SelectedIndex] && ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() ) );
      }

   }
   if ( ! m_pMenuItems[m_SelectedIndex]->isSelectable() )
      m_SelectedIndex = -1;
   onFocusedItemChanged();
}

void Menu::onMoveLeft(bool bIgnoreReversion)
{
   if ( m_MenuId == 0 ) // simple message menu? just pop it and return.
   {
      menu_stack_pop();
      return;
   }
   if ( 0 == m_ItemsCount )
      return;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i]->isSelectable() && m_pMenuItems[i]->isEditing() )
      {
         m_pMenuItems[i]->onKeyLeft(bIgnoreReversion);
         onItemValueChanged(i);
         return;
      }
   }

   // Check if current selected items wants to preprocess up/down/left/right
   if ( m_SelectedIndex >= 0 && m_SelectedIndex < m_ItemsCount && NULL != m_pMenuItems[m_SelectedIndex] && m_pMenuItems[m_SelectedIndex]->isEnabled() )
   if ( m_pMenuItems[m_SelectedIndex]->preProcessKeyLeft() )
   {
      onItemValueChanged(m_SelectedIndex);
      return;
   }

}

void Menu::onMoveRight(bool bIgnoreReversion)
{
   if ( m_MenuId == 0 ) // simple message menu? just pop it and return.
   {
      menu_stack_pop();
      return;
   }
   if ( 0 == m_ItemsCount )
      return;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( m_pMenuItems[i]->isSelectable() && m_pMenuItems[i]->isEditing() )
      {
         m_pMenuItems[i]->onKeyRight(bIgnoreReversion);
         onItemValueChanged(i);
         return;
      }
   }

   // Check if current selected items wants to preprocess up/down/left/right
   if ( m_SelectedIndex >= 0 && m_SelectedIndex < m_ItemsCount && NULL != m_pMenuItems[m_SelectedIndex] && m_pMenuItems[m_SelectedIndex]->isEnabled() )
   if ( m_pMenuItems[m_SelectedIndex]->preProcessKeyRight() )
   {
      onItemValueChanged(m_SelectedIndex);
      return;
   }

}


void Menu::onFocusedItemChanged()
{
   if ( 0 < m_ItemsCount && m_SelectedIndex >= 0 && m_SelectedIndex < m_ItemsCount && NULL != m_pMenuItems[m_SelectedIndex] )
      setTooltip( m_pMenuItems[m_SelectedIndex]->getTooltip() );

   updateScrollingOnSelectionChange();
}

void Menu::onItemValueChanged(int itemIndex)
{
}

void Menu::onItemEndEdit(int itemIndex)
{
}

void Menu::onReturnFromChild(int returnValue)
{
   if ( ! m_bHasChildMenuActive )
      return;
   m_bHasChildMenuActive = false;
   m_uOnChildCloseTime = g_TimeNow;
   m_bInvalidated = true;
   if ( m_bHasConfirmationOnTop )
      m_bAnimating = false;
   else
      m_bAnimating = true;
   m_bHasConfirmationOnTop = false;
}

void Menu::onChildMenuAdd()
{
   if ( menu_has_menu_confirmation_above(this) )
      m_bHasConfirmationOnTop = true;

   if ( m_bHasChildMenuActive )
      return;
   if ( m_xPosOriginal < -0.05 )
      m_xPosOriginal = m_xPos;

   m_bHasChildMenuActive = true;
   m_uOnChildAddTime = g_TimeNow;
   m_bInvalidated = true;
   m_bAnimating = true;
}


bool Menu::checkIsArmed()
{
   if ( pairing_isReceiving() || pairing_wasReceiving() )
   if ( NULL != g_pdpfct )
   if ( g_pdpfct->flags & FC_TELE_FLAGS_ARMED )
   {
      Popup* p = new Popup("Your vehicle is armed, you can't perform this operation now. Please stop/disarm your vehicle first.", 0.3, 0.3, 0.5, 6 );
      p->setCentered();
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return true;
   }
   return false;
}

void Menu::addMessage(const char* szMessage)
{
   m_iConfirmationId = 0;
   Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Info",NULL);
   pm->m_xPos = 0.32; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->addTopLine(szMessage);
   add_menu_to_stack(pm);
}

bool Menu::checkCancelUpload()
{
   hardware_loop();
   if ( ! isKeyBackPressed() )
      return false;

   g_bUpdateInProgress = false;
   log_line("The software update was canceled by user.");
   hardware_sleep_ms(50);
   addMessage("Update was canceled.");
   g_nFailedOTAUpdates++;
   return true;
}

bool Menu::uploadSoftware()
{
   ruby_pause_watchdog();
   g_bUpdateInProgress = true;
   render_commands_set_progress_percent(0, true);
   g_pRenderEngine->startFrame();
   popups_render();
   render_commands();
   popups_render_topmost();
   g_pRenderEngine->endFrame();


   // See what we have available to upload:
   // Either the latest update zip if present, either the binaries present on the controller
   // If we had failed uploads, use the binaries on the controller

   char szComm[256];
   char szBuff[1024];
   char szArchiveToUpload[256];
   szArchiveToUpload[0] = 0;
   int iUpdateType = -1;

   //bool bForceUseBinaries = false;
   //if ( (((g_pCurrentModel->sw_version)>>8) & 0xFF) == 6 )
   //if ( ((g_pCurrentModel->sw_version) & 0xFF) < 30 )
   //   bForceUseBinaries = true;
   bool bForceUseBinaries = true;

   if ( bForceUseBinaries )
      log_line("Using controller binaries for update.");
   else
   {
      if ( (g_nFailedOTAUpdates == 0) && (g_nSucceededOTAUpdates == 0) )
      {
         sprintf(szComm, "find updates/ruby_update_%d.%d.zip 2>/dev/null", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10);
         hw_execute_bash_command(szComm, szBuff);
         if ( 0 < strlen(szBuff) && NULL != strstr(szBuff, "ruby_update") )
         {
            log_line("Found zip update archive to upload to vehicle: [%s]", szBuff);
            strcpy(szArchiveToUpload, szBuff);
            iUpdateType = 0;
         }
         else
            log_line("Zip update archive to upload to vehicle not found. Using regular update of binary files.");
      }
      else
         log_line("There are previous updates done or failed. Using regular update of binary files.");
   }

   if ( iUpdateType == -1 )
   {
      strcpy(szArchiveToUpload, "tmp/sw.tar");
      iUpdateType = 1;

      // Add update info file
      if( access( FILE_INFO_LAST_UPDATE, R_OK ) == -1 )
      {
         sprintf(szComm, "cp %s %s 2>/dev/null", FILE_INFO_VERSION, FILE_INFO_LAST_UPDATE);
         hw_execute_bash_command(szComm, NULL);
      }

      sprintf(szComm, "rm -rf %s 2>/dev/null", szArchiveToUpload);
      hw_execute_bash_command(szComm, NULL);

      g_TimeNow = get_current_timestamp_ms();
      ruby_signal_alive();

      hw_execute_bash_command("cp -rf ruby_update ruby_update_vehicle", NULL);

      sprintf(szComm, "tar -czf %s ruby_*", szArchiveToUpload);
      hw_execute_bash_command(szComm, NULL);
      g_TimeNow = get_current_timestamp_ms();
      ruby_signal_alive();
   }

   if ( ! _uploadVehicleUpdate(iUpdateType, szArchiveToUpload) )
   {
      ruby_resume_watchdog();
      g_bUpdateInProgress = false;
      addMessage("There was an error updating your vehicle.");
      return false;
   }

   g_nSucceededOTAUpdates++;
   render_commands_set_progress_percent(-1, true);
   log_line("Successfully sent software package to vehicle.");
   send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);
   ruby_resume_watchdog();
   g_bUpdateInProgress = false;

   return true;
}

MenuItemSelect* Menu::createMenuItemCardModelSelector(const char* szName)
{
   MenuItemSelect* pItem = new MenuItemSelect(szName, "Sets the radio interface model.");
   pItem->addSelection("Generic");
   for( int i=1; i<255; i++ )
   {
      const char* szDesc = str_get_radio_card_model_string(i);
      if ( NULL != szDesc && 0 != szDesc[0] )
      if ( NULL == strstr(szDesc, "NONAME") )
      if ( NULL == strstr(szDesc, "Generic") )
         pItem->addSelection(szDesc);
   }
   pItem->setIsEditable();
   return pItem;
}

bool Menu::_uploadVehicleUpdate(int iUpdateType, const char* szArchiveToUpload)
{
   command_packet_sw_package cpswp_cancel;
   cpswp_cancel.type = iUpdateType;
   cpswp_cancel.total_size = 0;
   cpswp_cancel.file_block_index = MAX_U32;
   cpswp_cancel.last_block = false;
   cpswp_cancel.block_length = 0;

   g_bUpdateInProgress = true;

   long lSize = 0;
   FILE* fd = fopen(szArchiveToUpload, "rb");
   if ( NULL != fd )
   {
      fseek(fd, 0, SEEK_END);
      lSize = ftell(fd);
      fseek(fd, 0, SEEK_SET);
      fclose(fd);
   }

   log_line("Sending to vehicle the update archive (method 6.3): [%s], size: %d bytes", szArchiveToUpload, (int)lSize);

   fd = fopen(szArchiveToUpload, "rb");
   if ( NULL == fd )
   {
      addMessage("There was an error generating the software package.");
      g_bUpdateInProgress = false;
      return false;
   }

   u32 blockSize = 1100;
   u32 nPackets = ((u32)lSize) / blockSize;
   if ( lSize > nPackets * blockSize )
      nPackets++;

   u8** pPackets = (u8**) malloc(nPackets*sizeof(u8*));
   if ( NULL == pPackets )
   {
      addMessage("There was an error generating the upload package.");
      g_bUpdateInProgress = false;
      return false;
   }

   // Read packets in memory

   u32 nTotalPackets = 0;
   for( long l=0; l<lSize; )
   {
      u8* pPacket = (u8*) malloc(1500);
      if ( NULL == pPacket )
      {
         //free(pPackets);
         addMessage("There was an error generating the upload package.");
         g_bUpdateInProgress = false;
         return false;
      }
      command_packet_sw_package* pcpsp = (command_packet_sw_package*)pPacket;

      int nRead = fread(pPacket+sizeof(command_packet_sw_package), 1, blockSize, fd);
      if ( nRead < 0 )
      {
         //free((u8*)pPackets);
         addMessage("There was an error generating the upload package.");
         g_bUpdateInProgress = false;
         return false;
      }

      pcpsp->block_length = nRead;
      l += nRead;
      pcpsp->total_size = (u32)lSize;
      pcpsp->file_block_index = nTotalPackets;
      pcpsp->last_block = ((l == lSize)?true:false);
      pcpsp->type = iUpdateType;
      pPackets[nTotalPackets] = pPacket;
      nTotalPackets++;
   }

   log_line("Uploading %d sw segments", nTotalPackets);
   ruby_signal_alive();

   send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED,0);

   g_TimeNow = get_current_timestamp_ms();
   g_TimeNowMicros = get_current_timestamp_micros();
   u32 uTimeLastRender = 0;

   int iLastAcknowledgedPacket = -1;
   int iPacketToSend = 0;
   int iCountMaxRetriesForCurrentSegments = 10;
   while ( iPacketToSend < nTotalPackets )
   {
      u8* pPacket = pPackets[iPacketToSend];
      command_packet_sw_package* pcpsp = (command_packet_sw_package*)pPacket;

      bool bWaitAck = true;
      if ( (! pcpsp->last_block) && ((iPacketToSend % DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY) !=0 ) )
         bWaitAck = false;
      
      if ( ! bWaitAck )
      {
         log_line("Send sw package block %d of %d", iPacketToSend + 1, nTotalPackets);

         g_TimeNow = get_current_timestamp_ms();
         g_TimeNowMicros = get_current_timestamp_micros();
         ruby_signal_alive();

         for( int k=0; k<2; k++ )
            handle_commands_send_single_oneway_command(0, COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, bWaitAck, pPacket, pcpsp->block_length+sizeof(command_packet_sw_package));
         hardware_sleep_ms(2);
         iPacketToSend++;
         continue;
      }

      u32 commandUID = handle_commands_increment_command_counter();
      u8 resendCounter = 0;
      int waitReplyTime = 60;
      bool gotResponse = false;
      bool responseOk = false;

      // Send packet and wait for acknoledge

      if ( iPacketToSend == nTotalPackets - 1 )
         log_line("Send last sw package with ack, segment %d of %d", iPacketToSend + 1, nTotalPackets);
      else
         log_line("Send sw package with ack, segment %d of %d", iPacketToSend + 1, nTotalPackets);

      do
      {
         g_TimeNow = get_current_timestamp_ms();
         g_TimeNowMicros = get_current_timestamp_micros();
         ruby_signal_alive();

         if ( ! handle_commands_send_single_command_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, resendCounter, bWaitAck, pPacket, pcpsp->block_length+sizeof(command_packet_sw_package)) )
         {
            addMessage("There was an error uploading the software package.");
            fclose(fd);
            g_nFailedOTAUpdates++;
            for( int i=0; i<5; i++ )
            {
               handle_commands_increment_command_counter();
               handle_commands_send_single_command_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, 0, 0, (u8*)&cpswp_cancel, sizeof(command_packet_sw_package));
               hardware_sleep_ms(20);
            }
            send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);
            //for( int i=0; i<nTotalPackets; i++ )
            //   free((u8*)pPackets[i]);
            //free((u8*)pPackets);
            g_bUpdateInProgress = false;
            return false;
         }

         resendCounter++;
         gotResponse = false;
         u32 timeToWaitReply = g_TimeNow + waitReplyTime;
         log_line("Waiting for ACK for SW package segment %d, for %d ms, on retry: %d", iPacketToSend + 1, waitReplyTime, resendCounter-1);

         while ( g_TimeNow < timeToWaitReply && (! gotResponse) )
         {
            if ( checkCancelUpload() )
            {
               fclose(fd);
               for( int i=0; i<5; i++ )
               {
                  handle_commands_increment_command_counter();
                  handle_commands_send_single_command_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, 0, 0, (u8*)&cpswp_cancel, sizeof(command_packet_sw_package));
                  hardware_sleep_ms(20);
               }
               send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);
               //for( int i=0; i<nTotalPackets; i++ )
               //   free((u8*)pPackets[i]);
               //free((u8*)pPackets);
               g_bUpdateInProgress = false;
               return false;
            }

            g_TimeNow = get_current_timestamp_ms();
            g_TimeNowMicros = get_current_timestamp_micros();
            if ( try_read_messages_from_router(waitReplyTime) )
            {
               u8* pReplyBuffer = get_last_command_reply_from_router();
               if ( NULL != pReplyBuffer )
               {
                  t_packet_header* pPH = (t_packet_header*) pReplyBuffer;
                  t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pReplyBuffer + sizeof(t_packet_header));
                  if ( pPHCR->origin_command_counter == commandUID )
                  {
                     gotResponse = true;
                     responseOk = false;
                     if ( pPHCR->command_response_flags & COMMAND_RESPONSE_FLAGS_OK )
                        responseOk = true;
                     log_line("Did got an ACK. Succeded: %s", responseOk?"yes":"no");
                     break;
                  }
               }
            }
            log_line("Did not get an ACK from vehicle. Keep waiting...");
         } // finised waiting for ACK

         if ( ! gotResponse )
         {
            log_line("Did not get an ACK from vehicle. Increase wait time.");
            waitReplyTime += 50;
            if ( waitReplyTime > 500 )
               waitReplyTime = 500;
         }
      }
      while ( (resendCounter < 15) && (! gotResponse) );

      // Finished sending and waiting for packet with ACK wait

      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();
      ruby_signal_alive();

      if ( ! gotResponse )
      {
         log_softerror_and_alarm("Did not get a confirmation from vehicle about the software upload.");
         g_nFailedOTAUpdates++;
         fclose(fd);
         for( int i=0; i<5; i++ )
         {
            handle_commands_increment_command_counter();
            handle_commands_send_single_command_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, 0, 0, (u8*)&cpswp_cancel, sizeof(command_packet_sw_package));
            hardware_sleep_ms(20);
         }
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);

         //for( int i=0; i<nTotalPackets; i++ )
         //   free((u8*)pPackets[i]);
         //free((u8*)pPackets);
         g_bUpdateInProgress = false;
         return false;
      }

      if ( gotResponse && (!responseOk) )
      {
         // Restart from last confirmed packet
         log_softerror_and_alarm("The software package block (segment index %d) was rejected by the vehicle.", iPacketToSend+1);
         iPacketToSend = iLastAcknowledgedPacket; // will be +1 down below
         iCountMaxRetriesForCurrentSegments--;
         if ( iCountMaxRetriesForCurrentSegments < 0 )
         {
            g_nFailedOTAUpdates++;
            fclose(fd);
            for( int i=0; i<5; i++ )
            {
               handle_commands_increment_command_counter();
               handle_commands_send_single_command_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, 0, 0, (u8*)&cpswp_cancel, sizeof(command_packet_sw_package));
               hardware_sleep_ms(20);
            }
            send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);

            g_bUpdateInProgress = false;
            return false;
         }
      }

      if ( gotResponse && responseOk )
      {
         iCountMaxRetriesForCurrentSegments = 10;
         iLastAcknowledgedPacket = iPacketToSend;
         log_line("Got ACK for segment %d", iPacketToSend+1);
      }
      int percent = pcpsp->file_block_index*100/(pcpsp->total_size/blockSize);

      if ( checkCancelUpload() )
      {
         fclose(fd);
         for( int i=0; i<5; i++ )
         {
            handle_commands_increment_command_counter();
            handle_commands_send_single_command_to_vehicle(COMMAND_ID_UPLOAD_SW_TO_VEHICLE63, 0, 0, (u8*)&cpswp_cancel, sizeof(command_packet_sw_package));
            hardware_sleep_ms(20);
         }
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED,0);
         //for( int i=0; i<nTotalPackets; i++ )
         //   free((u8*)pPackets[i]);
         //free((u8*)pPackets);
         g_bUpdateInProgress = false;
         return false;
      }

      if ( g_TimeNow > (uTimeLastRender+100) )
      {
         uTimeLastRender = g_TimeNow;
         render_commands_set_progress_percent(percent, true);
         g_pRenderEngine->startFrame();
         //osd_render();
         popups_render();
         //menu_render();
         render_commands();
         popups_render_topmost();
         g_pRenderEngine->endFrame();
      }
      iPacketToSend++;
   }

   //for( int i=0; i<nTotalPackets; i++ )
   //   free((u8*)pPackets[i]);
   //free((u8*)pPackets);

   g_bUpdateInProgress = false;
   return true;
}

void Menu::addMessageNeedsVehcile(const char* szMessage, int iConfirmationId)
{
   m_iConfirmationId = iConfirmationId;
   Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Not connected",NULL);
   pm->m_xPos = 0.4; pm->m_yPos = 0.4;
   pm->m_Width = 0.36;
   pm->addTopLine(szMessage);
   add_menu_to_stack(pm);
}
