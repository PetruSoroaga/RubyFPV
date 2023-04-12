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
#include "menu.h"
#include "menu_objects.h"
#include "menu_text.h"
#include "shared_vars.h"
#include "colors.h"
#include "../renderer/render_engine.h"

MenuText::MenuText(void)
:Menu(MENU_ID_TEXT , "Info", NULL)
{
   m_xPos = 0.45; m_yPos = 0.2;
   m_Width = 0.45;
   m_LinesCount = 0;
   m_TopLineIndex = 0;
   m_fScaleFactor = 1.0;
   addMenuItem(new MenuItem("Done"));
}

MenuText::~MenuText()
{
   for( int i=0; i<m_LinesCount; i++ )
         free ( m_szLines[i] );
   m_LinesCount = 0;
   m_bInvalidated = true;
}

void MenuText::addTextLine(const char* szText)
{
   if ( NULL == szText )
      return;
   if ( m_LinesCount < M_MAX_TEXT_LINES - 1 )
   {
      m_szLines[m_LinesCount] = (char*)malloc(strlen(szText)+1);
      strcpy(m_szLines[m_LinesCount], szText);
      m_LinesCount++;
   }
   else
   {
      free (m_szLines[0]);
      for( int i=0; i<M_MAX_TEXT_LINES-1; i++ )
         m_szLines[i] = m_szLines[i+1];
      m_szLines[M_MAX_TEXT_LINES-1] = (char*)malloc(strlen(szText)+1);
      strcpy(m_szLines[M_MAX_TEXT_LINES-1], szText);
   }
   m_bInvalidated = true;

}

void MenuText::onShow()
{
   Menu::onShow();
}

void MenuText::computeRenderSizes()
{
   m_RenderWidth = m_Width*menu_getScaleMenus();
   m_RenderHeight = m_ExtraHeight + 2*MENU_MARGINS*menu_getScaleMenus();
   if ( m_Height > 0.001 )
   {
      m_RenderHeight += m_Height;
      m_bInvalidated = false;
      return;
   }

       if ( 0 != m_szTitle[0] )
       {
          m_RenderHeight += g_pRenderEngine->getMessageHeight(m_szTitle, menu_getScaleMenus()*MENU_FONT_SIZE_TITLE, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
          m_RenderHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TITLE * MENU_TEXTLINE_SPACING;
       }
       if ( 0 != m_szSubTitle[0] )
       {
          m_RenderHeight += g_pRenderEngine->getMessageHeight(m_szSubTitle, menu_getScaleMenus()*MENU_FONT_SIZE_SUBTITLE, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
          m_RenderHeight += menu_getScaleMenus()*MENU_FONT_SIZE_SUBTITLE * MENU_TEXTLINE_SPACING;
       }
       m_RenderHeight += 1.0*MENU_MARGINS*menu_getScaleMenus();

       for (int i = 0; i<m_LinesCount; i++)
       {
           if ( 0 == strlen(m_szLines[i]) )
              m_RenderHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE;
           else
           {
              m_RenderHeight += g_pRenderEngine->getMessageHeight(m_szLines[i], menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
              m_RenderHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE * MENU_TEXTLINE_SPACING;
           }
       }
       m_RenderHeight += 0.5*MENU_MARGINS*menu_getScaleMenus();
   
   m_bInvalidated = false;
}

void MenuText::Render()
{
   RenderPrepare();
   if ( m_bInvalidated )
   {
      computeRenderSizes();
      m_bInvalidated = false;
   }

   float yTop = RenderFrameAndTitle(); 
   float y = yTop;

   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();

   for (int i = 0; i<m_LinesCount; i++)
   {
      y += g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus()*m_fScaleFactor, y, m_szLines[i], height_text, MENU_TEXTLINE_SPACING, m_RenderWidth-2*MENU_MARGINS*menu_getScaleMenus()*m_fScaleFactor, g_idFontMenu);
      y += height_text*MENU_TEXTLINE_SPACING;
   }
   RenderEnd(yTop);
}

void MenuText::onSelectItem()
{
   Menu::onSelectItem();
   if ( 0 == m_SelectedIndex )
      menu_stack_pop(0);
}
