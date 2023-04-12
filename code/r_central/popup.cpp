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
#include "popup.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "osd_common.h"
#include "menu.h"
#include "shared_vars.h"
#include "timers.h"

float POPUP_LINE_SPACING = 0.5; // percentage of actual line height

Popup* sPopups[MAX_POPUPS];
int countPopups = 0;

Popup* sPopupsTopmost[MAX_POPUPS];
int countPopupsTopmost = 0;

int popups_get_count() { return countPopups; }
int popups_get_topmost_count() { return countPopupsTopmost; };

Popup* popups_get_at(int index)
{
   if ( index >= 0 && index < countPopups )
      return sPopups[index];
   return NULL;
}

Popup* popups_get_topmost_at(int index)
{
   if ( index >= 0 && index < countPopupsTopmost )
      return sPopupsTopmost[index];
   return NULL;
}

void popups_add(Popup* p)
{
   if ( NULL == p )
   {
      log_softerror_and_alarm("Tried to add a NULL popup to the stack");
      return;      
   }

   log_line("Added popup: %s", p->getTitle());

   for( int i=0; i<countPopups; i++ )
      if ( sPopups[i] == p )
      {
         p->onShow();
         return;
      }

   if ( (countPopups < MAX_POPUPS) && (NULL != p))
   {
      p->onShow();
      sPopups[countPopups] = p;
      countPopups++;
   }
}

void popups_add_topmost(Popup* p)
{
   if ( NULL == p )
   {
      log_softerror_and_alarm("Tried to add a NULL popup to the stack");
      return;      
   }

   for( int i=0; i<countPopupsTopmost; i++ )
      if ( sPopupsTopmost[i] == p )
      {
         p->onShow();
         return;
      }

   if ( (countPopupsTopmost < MAX_POPUPS) && (NULL != p))
   {
      p->onShow();
      sPopupsTopmost[countPopupsTopmost] = p;
      countPopupsTopmost++;
   }
}

bool popups_has_popup(Popup* p)
{
   if ( NULL == p )
      return false;

   for( int i=0; i<countPopups; i++ )
      if ( sPopups[i] == p )
         return true;

   for( int i=0; i<countPopupsTopmost; i++ )
      if ( sPopupsTopmost[i] == p )
         return true;
   return false;
}

void popups_remove(Popup* p)
{
   int i = 0;
   for( ; i<countPopups; i++ )
      if ( NULL != sPopups[i] && sPopups[i] == p )
         break;

   if ( i < countPopups && sPopups[i] == p )
   {
      for( ; i<countPopups-1; i++ )
         sPopups[i] = sPopups[i+1];
      countPopups--;
   }

   i = 0;
   for( ; i<countPopupsTopmost; i++ )
      if ( NULL != sPopupsTopmost[i] && sPopupsTopmost[i] == p )
         break;

   if ( i < countPopupsTopmost && sPopupsTopmost[i] == p )
   {
      for( ; i<countPopupsTopmost-1; i++ )
         sPopupsTopmost[i] = sPopupsTopmost[i+1];
      countPopupsTopmost--;
   }
}

void popups_remove_all(Popup* pExceptionPopup)
{
   int iIndexException = -1;
   for( int i=0; i<countPopups; i++ )
      if ( (NULL != sPopups[i]) && (sPopups[i] == pExceptionPopup) )
      {
         iIndexException = i;
         break;
      }

   countPopups = 0;
   if ( iIndexException != -1 )
   {
      countPopups = 1;
      sPopups[0] = sPopups[iIndexException];
   }

   iIndexException = -1;
   for( int i=0; i<countPopupsTopmost; i++ )
      if ( (NULL != sPopupsTopmost[i]) && (sPopupsTopmost[i] == pExceptionPopup) )
      {
         iIndexException = i;
         break;
      }

   countPopupsTopmost = 0;
   if ( iIndexException != -1 )
   {
      countPopupsTopmost = 1;
      sPopupsTopmost[0] = sPopupsTopmost[iIndexException];
   }
}

void popups_render()
{
   if ( render_engine_is_raw() )
      POPUP_LINE_SPACING = 0.2;

   float alfa = g_pRenderEngine->getGlobalAlfa();

   //log_line("Render %d popups", countPopups);

   if ( isMenuOn() )
   {
      g_pRenderEngine->setGlobalAlfa(0.47);
   }
   for( int i=0; i<countPopups; i++ )
      if ( NULL != sPopups[i] )
      {
         sPopups[i]->Render();
         if ( sPopups[i]->isExpired() )
         {
            sPopups[i] = NULL;
         }
      }

   g_pRenderEngine->setGlobalAlfa(alfa);

   int index = 0;
   int skip = 0;
   while( index < countPopups )
   {
       if ( sPopups[index+skip] == NULL )
       {
          skip++;
          countPopups--;
          continue;
       }
       sPopups[index] = sPopups[index+skip];
       index++;
   }
}

void popups_render_topmost()
{
   float alfa = g_pRenderEngine->setGlobalAlfa(1.2);

   //log_line("Render %d topmost popups", countPopupsTopmost);

   for( int i=0; i<countPopupsTopmost; i++ )
      if ( NULL != sPopupsTopmost[i] )
      {
         sPopupsTopmost[i]->Render();
         if ( sPopupsTopmost[i]->isExpired() )
            sPopupsTopmost[i] = NULL;
      }

   g_pRenderEngine->setGlobalAlfa(alfa);

   int index = 0;
   int skip = 0;
   while( index < countPopupsTopmost )
   {
       if ( sPopupsTopmost[index+skip] == NULL )
       {
          skip++;
          countPopupsTopmost--;
          continue;
       }
       sPopupsTopmost[index] = sPopupsTopmost[index+skip];
       index++;
   }
}

void popups_invalidate_all()
{
   for( int i=0; i<countPopups; i++ )
      if ( NULL != sPopups[i] )
         sPopups[i]->invalidate();
   for( int i=0; i<countPopupsTopmost; i++ )
      if ( NULL != sPopupsTopmost[i] )
         sPopupsTopmost[i]->invalidate();
}


Popup::Popup(const char* title, float x, float y, float timeoutSec)
{
   m_LinesCount = 0;
   m_szTitle[0] = 0;
   if ( NULL != title )
      strcpy(m_szTitle, title);
   int pos = strlen(m_szTitle)-1;
   while (pos >= 0 && m_szTitle[pos] == ' ' )
   {
      m_szTitle[pos] = 0;
      pos--;
   }

   m_xPos = x;
   m_yPos = y;
   m_fTimeoutSeconds = timeoutSec;
   m_bInvalidated = true;
   m_fMaxWidth = 0.8;
   m_fFixedWidth = 0.0;
   m_fBottomMargin = 0.0;
   m_RenderWidth = 0;
   m_RenderHeight = 0;
   m_bTopmost = false;
   m_idFont = g_idFontMenuLarge;
   m_idIcon = 0;
   m_pColorIcon = NULL;
   m_fIconSize = 0;
   m_bCentered = false;
   m_bBottomAlign = false;
   m_bNoBackground = false;
   m_fBackgroundAlpha = 1.0;
   m_fPadding = 0.0;
}

Popup::Popup(const char* title, float x, float y, float maxWidth, float timeoutSec)
{
   m_LinesCount = 0;
   m_szTitle[0] = 0;
   if ( NULL != title )
      strcpy(m_szTitle, title);
   int pos = strlen(m_szTitle)-1;
   while (pos >= 0 && m_szTitle[pos] == ' ' )
   {
      m_szTitle[pos] = 0;
      pos--;
   }

   m_xPos = x;
   m_yPos = y;
   m_fTimeoutSeconds = timeoutSec;
   m_bInvalidated = true;
   m_fMaxWidth = maxWidth;
   m_fFixedWidth = 0.0;
   m_fBottomMargin = 0.0;
   m_RenderWidth = 0;
   m_RenderHeight = 0;
   m_bTopmost = false;
   m_idFont = g_idFontMenuLarge;
   m_idIcon = 0;
   m_pColorIcon = NULL;
   m_bCentered = false;
   m_bBottomAlign = false;
   m_bNoBackground = false;
   m_fBackgroundAlpha = 1.0;
   m_fPadding = 0.0;
}


Popup::Popup(bool bCentered, const char* title, float timeoutSec)
{
   m_LinesCount = 0;
   m_szTitle[0] = 0;
   if ( NULL != title )
      strcpy(m_szTitle, title);
   m_xPos = 0;
   m_yPos = 0;
   m_fTimeoutSeconds = timeoutSec;
   m_bInvalidated = true;
   m_fMaxWidth = 0.8;
   m_fFixedWidth = 0.0;
   m_fBottomMargin = 0.0;
   m_RenderWidth = 0;
   m_RenderHeight = 0;
   m_bTopmost = false;
   m_idFont = g_idFontMenuLarge;
   m_idIcon = 0;
   m_pColorIcon = NULL;
   m_bCentered = bCentered;
   m_bNoBackground = false;
   m_fBackgroundAlpha = 1.0;
   m_fPadding = 0.0;

   if ( m_bCentered )
      m_bBottomAlign = false;
}


Popup::~Popup()
{
   removeAllLines();
}

const char* Popup::getTitle()
{
   return m_szTitle;
}

void Popup::setFont(u32 idFont)
{
   m_idFont = idFont;
   m_bInvalidated = true;
}

u32 Popup::getFontId()
{
   return m_idFont;
}

void Popup::setMaxWidth(float width)
{
   m_fMaxWidth = width;
   m_bInvalidated = true;
}

void Popup::setFixedWidth(float width)
{
   m_fFixedWidth = width;
   m_bInvalidated = true;
}

void Popup::setXPos(float xPos)
{
   m_xPos = xPos;
   m_bInvalidated = true;
}

void Popup::setBottomAlign(bool b)
{
   m_bBottomAlign = b;
   m_bInvalidated = true;
}

void Popup::setBottomMargin(float dymargin)
{
   m_fBottomMargin = dymargin;
}

void Popup::setNoBackground()
{
   m_bNoBackground = true;
   m_bInvalidated = true;
}

void Popup::setBackgroundAlpha(float fAlpha)
{
   m_fBackgroundAlpha = fAlpha;
}

void Popup::setCustomPadding(float fPadding)
{
   m_fPadding = fPadding;
}

int Popup::isExpired()
{
   if ( m_fTimeoutSeconds > 0.001)
   if ((g_TimeNow-m_StartTime) > m_fTimeoutSeconds*1000.0)
      return 1;
   return 0;
}

void Popup::resetTimer()
{
   m_StartTime = g_TimeNow;
   m_bInvalidated = true;
}

void Popup::setTimeout(float timeoutSeconds)
{
   m_fTimeoutSeconds = timeoutSeconds;
   m_bInvalidated = true;
}

void Popup::setIconId(u32 idIcon, double* pColor)
{
   m_idIcon = idIcon;
   m_pColorIcon = pColor;
   m_bInvalidated = true;
}

void Popup::invalidate()
{
   m_bInvalidated = true;
}

void Popup::refresh()
{
   m_bInvalidated = true;
}

void Popup::setCentered()
{
   m_bCentered = true;
   m_bBottomAlign = false;
}

void Popup::setTitle(const char* szTitle)
{
   if ( NULL == szTitle )
      m_szTitle[0] = 0;
   else
      strcpy(m_szTitle, szTitle);
   m_bInvalidated = true;
}

void Popup::addLine(const char* szLine)
{
   if ( NULL == szLine )
      return;
   if ( m_LinesCount < MAX_POPUP_LINES - 1 )
   {
      m_szLines[m_LinesCount] = (char*)malloc(strlen(szLine)+1);
      strcpy(m_szLines[m_LinesCount], szLine);
      m_LinesCount++;
   }
   else
   {
      free (m_szLines[0]);
      for( int i=0; i<MAX_POPUP_LINES-1; i++ )
         m_szLines[i] = m_szLines[i+1];
      m_szLines[MAX_POPUP_LINES-1] = (char*)malloc(strlen(szLine)+1);
      strcpy(m_szLines[MAX_POPUP_LINES-1], szLine);
   }
   m_bInvalidated = true;
}

void Popup::removeLastLine()
{
   if ( m_LinesCount > 0 )
   {
      free ( m_szLines[m_LinesCount-1] );
      m_LinesCount--;
   }
   m_bInvalidated = true;
}

void Popup::removeAllLines()
{
   for( int i=0; i<m_LinesCount; i++ )
      free ( m_szLines[i] );
   m_LinesCount = 0;
   m_bInvalidated = true;
}

float Popup::getRenderHeight()
{
   if ( m_bInvalidated )
   {
      computeSize();
      m_bInvalidated = false;
   }
   return m_RenderHeight;
}

void Popup::onShow()
{
   resetTimer();
   m_bInvalidated = true;
}

void Popup::computeSize()
{
   float height_text = g_pRenderEngine->textHeight(m_idFont);

   float fPaddingX = POPUP_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   float fPaddingY = POPUP_MARGINS*menu_getScaleMenus();

   if ( m_fPadding > 0.0001 )
   {
      fPaddingX = height_text * m_fPadding/g_pRenderEngine->getAspectRatio();
      fPaddingY = height_text * m_fPadding;
   }

   m_RenderWidth = 2.0 * fPaddingX;
   m_RenderHeight = 2.0 * fPaddingY;

   if ( m_fMaxWidth > 0.8 )
      m_fMaxWidth = 0.8;

   // Compute width

   float fMaxTextWidth = 0.0;
      
   if ( m_fFixedWidth > 0.01 )
   {
      m_RenderWidth = m_fFixedWidth;
      fMaxTextWidth = m_fFixedWidth - 2.0 * fPaddingX;
   }
   else
   {
      if ( 0 != m_szTitle[0] )
         fMaxTextWidth = g_pRenderEngine->textWidth(m_idFont, m_szTitle);
         
      for (int i = 0; i<m_LinesCount; i++)
      {
         float fTextWidth = g_pRenderEngine->textWidth(m_idFont, (const char*)(m_szLines[i]));
         if ( fTextWidth > fMaxTextWidth )
            fMaxTextWidth = fTextWidth;
      }

      if ( fMaxTextWidth > m_fMaxWidth-2.0*fPaddingX )
         fMaxTextWidth = m_fMaxWidth-2.0*fPaddingX;

      m_RenderWidth = fMaxTextWidth + 2.0 * fPaddingX;
   }

   // Compute height

   if ( 0 != m_szTitle[0] )
   {
      float fHeight = g_pRenderEngine->getMessageHeight(m_szTitle, POPUP_LINE_SPACING, fMaxTextWidth, m_idFont);
      m_RenderHeight += fHeight;
   }

   for (int i = 0; i<m_LinesCount; i++)
   {
      m_RenderHeight += height_text*POPUP_LINE_SPACING;
      
      float fHeight = g_pRenderEngine->getMessageHeight((const char*)(m_szLines[i]), POPUP_LINE_SPACING, fMaxTextWidth, m_idFont);
      m_RenderHeight += fHeight;
   }

   if ( m_bCentered )
   {
      m_RenderXPos = (1.0-m_RenderWidth)/2.0;
      m_RenderYPos = (1.0-m_RenderHeight)/2.0;
   }
   else
   {
      m_RenderXPos = m_xPos;
      m_RenderYPos = m_yPos;
   }

   if ( m_bBottomAlign )
   {
      m_RenderYPos = 0.84 - m_RenderHeight;
   }
   m_RenderYPos -= m_fBottomMargin;


   m_bInvalidated = false;
}

void Popup::Render()
{
   if ( m_bInvalidated )
   {
      computeSize();
      m_bInvalidated = false;
   }
   if ( m_fTimeoutSeconds > 0.001 )
   if ((g_TimeNow-m_StartTime) > m_fTimeoutSeconds*1000.0)
      return;

   float alfaOrg = g_pRenderEngine->getGlobalAlfa();
   float alfa = alfaOrg;

   if ( m_fTimeoutSeconds > 1.0 )
   if ( (g_TimeNow-m_StartTime) > (m_fTimeoutSeconds-1.0)*1000.0+400 )
      alfa = alfaOrg - alfaOrg *((g_TimeNow-m_StartTime)-((m_fTimeoutSeconds-1.0)*1000.0+400))/600.0;
   
   if ( alfa < 0 ) alfa = 0;
   if ( alfa > alfaOrg ) alfa = alfaOrg;

   g_pRenderEngine->setGlobalAlfa(alfa);

   double colorBg[4];
   memcpy(&colorBg, get_Color_PopupBg(), 4*sizeof(double));
   colorBg[3] = colorBg[3] * m_fBackgroundAlpha;

   if ( m_fBackgroundAlpha < 0.99 )
   {
      g_pRenderEngine->setColors(colorBg);
   }
   else
   {
      g_pRenderEngine->setColors(get_Color_PopupBg());
      g_pRenderEngine->setStroke(get_Color_PopupBorder());
   }

   float height_text = g_pRenderEngine->textHeight(m_idFont);
   float fPaddingX = POPUP_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   float fPaddingY = POPUP_MARGINS*menu_getScaleMenus();

   if ( m_fPadding > 0.0001 )
   {
      fPaddingX = height_text * m_fPadding/g_pRenderEngine->getAspectRatio();
      fPaddingY = height_text * m_fPadding;
   }

   m_fIconSize = 0.0;
   float fdWIcon = 0.0;
   if ( 0 != m_idIcon )
   {
      m_fIconSize = height_text*1.4;
      if ( m_RenderHeight-2.0*fPaddingY > 2.0*height_text )
         m_fIconSize = height_text*2.2;
      if ( m_RenderHeight-2.0*fPaddingY > 3.6*height_text )
         m_fIconSize = height_text*3.2;
      if ( m_RenderHeight-2.0*fPaddingY >= 4.6*height_text )
         m_fIconSize = height_text*3.8;

      if ( m_fIconSize < height_text*1.8 )
      if ( m_idIcon == g_idIconController )
         m_fIconSize = height_text*2.0;

      fdWIcon = m_fIconSize/g_pRenderEngine->getAspectRatio() + fPaddingX;
   }
   if ( ! m_bNoBackground )
      g_pRenderEngine->drawRoundRect(m_RenderXPos, m_RenderYPos, m_RenderWidth+fdWIcon, m_RenderHeight, POPUP_ROUND_MARGIN);

   float x = m_RenderXPos+fPaddingX;
   float y = m_RenderYPos+fPaddingY;
   
   if ( 0 != m_idIcon )
   {
      if ( NULL != m_pColorIcon )
         g_pRenderEngine->setColors(m_pColorIcon);
      float yIcon = m_RenderYPos + 0.5*(m_RenderHeight - m_fIconSize);
      g_pRenderEngine->drawIcon(x, yIcon, m_fIconSize/g_pRenderEngine->getAspectRatio(), m_fIconSize, m_idIcon);  
      x += m_fIconSize/g_pRenderEngine->getAspectRatio() + fPaddingX;
   }
   
   g_pRenderEngine->setColors(get_Color_PopupText());
   
   float fMaxTextWidth = m_RenderWidth - 2.0*fPaddingX + 0.001;
   
   if ( 0 != m_szTitle[0] )
   {
      float fHeightText = g_pRenderEngine->drawMessageLines(x, y, m_szTitle, height_text, POPUP_LINE_SPACING, fMaxTextWidth, m_idFont);
      y += fHeightText;
   }
   
   for (int i = 0; i<m_LinesCount; i++)
   {
      y += height_text*POPUP_LINE_SPACING;
      float fHeightText = g_pRenderEngine->drawMessageLines(x, y, m_szLines[i], height_text, POPUP_LINE_SPACING, fMaxTextWidth, m_idFont);
      y += fHeightText;
   }

   g_pRenderEngine->setGlobalAlfa(alfaOrg);
}

