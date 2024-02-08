/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "menu.h"
#include "menu_preferences_ui.h"

#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_color_picker.h"
#include "../osd/osd_common.h"
#include "../popup_log.h"
#include "../fonts.h"


MenuPreferencesUI::MenuPreferencesUI(void)
:Menu(MENU_ID_PREFERENCES_UI, "User Interface Fonts, Colors and Sizes", NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.12;

   addMenuItem(new MenuItemSection("Menus"));

   m_pItemsSelect[0] = new MenuItemSelect("Menu Font Size", "Change how big the menus appear on screen.");  
   m_pItemsSelect[0]->addSelection("X-Small");
   m_pItemsSelect[0]->addSelection("Small");
   m_pItemsSelect[0]->addSelection("Normal");
   m_pItemsSelect[0]->addSelection("Large");
   m_pItemsSelect[0]->addSelection("X-Large");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexScaleMenu = addMenuItem(m_pItemsSelect[0]);
   
   m_pItemsSelect[1] = new MenuItemSelect("Menus Layout", "Changes how the menus appear on screen.");  
   m_pItemsSelect[1]->addSelection("Side by side");
   m_pItemsSelect[1]->addSelection("Stacked");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexMenuStacked = addMenuItem(m_pItemsSelect[1]);

   addMenuItem(new MenuItemSection("OSD"));

   m_pItemsSelect[2] = new MenuItemSelect("Invert Colors", "Invert colors on OSD and Menus.");
   m_pItemsSelect[2]->addSelection("Normal");
   m_pItemsSelect[2]->addSelection("Inverted");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexInvertColors = addMenuItem(m_pItemsSelect[2]);

   m_IndexColorPickerOSD = addMenuItem(new MenuItem("OSD Text Color","Change color of the text in the OSD.")); 
   m_IndexColorPickerOSDOutline = addMenuItem(new MenuItem("OSD Text Outline Color","Change color of the text outline in the OSD.")); 

   m_pItemsSelect[3] = new MenuItemSelect("OSD Outline Thickness", "Increase/decrease OSD outline thickness.");
   m_pItemsSelect[3]->addSelection("None");
   m_pItemsSelect[3]->addSelection("Smallest");
   m_pItemsSelect[3]->addSelection("Small");
   m_pItemsSelect[3]->addSelection("Normal");
   m_pItemsSelect[3]->addSelection("Large");
   m_pItemsSelect[3]->addSelection("Larger");
   m_pItemsSelect[3]->addSelection("Largest");
   //m_IndexOSDOutlineThickness = addMenuItem(m_pItemsSelect[3]);
   m_IndexColorPickerOSDOutline = -1;


   m_pItemsSelect[6] = new MenuItemSelect("OSD Screen Size", "Change how big is the OSD relative to the screen.");  
   m_pItemsSelect[6]->addSelection("100%");
   m_pItemsSelect[6]->addSelection("96%");
   m_pItemsSelect[6]->addSelection("92%");
   m_pItemsSelect[6]->addSelection("88%");
   m_pItemsSelect[6]->addSelection("84%");
   m_pItemsSelect[6]->addSelection("80%");
   m_IndexOSDSize = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[7] = new MenuItemSelect("OSD Flip Vertical", "Flips the OSD info vertically for rotated displays.");  
   m_pItemsSelect[7]->addSelection("No");
   m_pItemsSelect[7]->addSelection("Yes");
   m_pItemsSelect[7]->setIsEditable();
   m_IndexOSDFlip = addMenuItem(m_pItemsSelect[7]);

   m_IndexColorPickerAHI = addMenuItem(new MenuItem("Instruments Color","Change color of the instruments/gauges.")); 

   m_pItemsSelect[10] = new MenuItemSelect("OSD Font", "Changes the OSD font.");  
   m_pItemsSelect[10]->addSelection("Font 1 Bold");
   m_pItemsSelect[10]->addSelection("Font 1 Outlined");
   m_pItemsSelect[10]->addSelection("Font 2");
   m_pItemsSelect[10]->addSelection("Font 3 Bold");
   m_pItemsSelect[10]->setIsEditable();
   m_IndexOSDFont = addMenuItem(m_pItemsSelect[10]);

   m_pItemsSelect[12] = new MenuItemSelect("Show loop monitor", "Shows a graphical spinner (bottom left of the screen) for monitoring the controller processes.");  
   m_pItemsSelect[12]->addSelection("No");
   m_pItemsSelect[12]->addSelection("Yes");
   m_pItemsSelect[12]->setIsEditable();
   m_IndexMonitor = addMenuItem(m_pItemsSelect[12]);
}

void MenuPreferencesUI::valuesToUI()
{
   Preferences* p = get_Preferences();
   if ( NULL == p )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   m_pItemsSelect[0]->setSelection(p->iScaleMenus+2);
   m_pItemsSelect[1]->setSelection(p->iMenusStacked);

   m_pItemsSelect[2]->setSelection(p->iInvertColorsOSD);
   //m_pItemsSelect[3]->setSelection(p->iOSDOutlineThickness+3);
   m_pItemsSelect[6]->setSelection(p->iOSDScreenSize);
   m_pItemsSelect[7]->setSelection(p->iOSDFlipVertical);

   m_pItemsSelect[10]->setSelectedIndex(p->iOSDFont);

   m_pItemsSelect[12]->setSelection(p->iShowProcessesMonitor);
}

void MenuPreferencesUI::onShow()
{
   removeAllTopLines();
   Menu::onShow();
}

void MenuPreferencesUI::Render()
{
   Preferences* p = get_Preferences();
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      if ( i == m_IndexColorPickerOSD )
      {
         double color[4];
         color[0] = p->iColorOSD[0];
         color[1] = p->iColorOSD[1];
         color[2] = p->iColorOSD[2];
         color[3] = 1.0;
         g_pRenderEngine->setColors(color);
         float h = m_pMenuItems[i]->getItemHeight(getUsableWidth());
         g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth-m_sfMenuPaddingX-h*1.5, y, h*1.5, h, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
         g_pRenderEngine->setColors(get_Color_MenuText());
      }

      if ( i == m_IndexColorPickerOSDOutline )
      {
         double color[4];
         color[0] = p->iColorOSDOutline[0];
         color[1] = p->iColorOSDOutline[1];
         color[2] = p->iColorOSDOutline[2];
         color[3] = 1.0;
         g_pRenderEngine->setColors(color);
         float h = m_pMenuItems[i]->getItemHeight(getUsableWidth());
         g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth-m_sfMenuPaddingX-h*1.5, y, h*1.5, h, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
         g_pRenderEngine->setColors(get_Color_MenuText());
      }

      if ( i == m_IndexColorPickerAHI )
      {
         double color[4];
         color[0] = p->iColorAHI[0];
         color[1] = p->iColorAHI[1];
         color[2] = p->iColorAHI[2];
         color[3] = 1.0;
         g_pRenderEngine->setColors(color);
         float h = m_pMenuItems[i]->getItemHeight(getUsableWidth());
         g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth-m_sfMenuPaddingX-h*1.5, y, h*1.5, h, MENU_ROUND_MARGIN*m_sfMenuPaddingY);
         g_pRenderEngine->setColors(get_Color_MenuText());
      }

      y += RenderItem(i,y);

      if ( i == m_IndexOSDSize-1 )
      {
         g_pRenderEngine->setColors(get_Color_MenuBg());
         g_pRenderEngine->setStroke(get_Color_MenuText());
         Preferences* p = get_Preferences();
         float h = 2*(1+MENU_TEXTLINE_SPACING)*g_pRenderEngine->textHeight(g_idFontMenu);
         float hs = h*0.7;
         float ws = hs;
         float ho = hs;
         float wo = ws;
         float x = m_xPos+m_RenderWidth-2*m_sfMenuPaddingX-ws;
         x -= 0.04*m_sfScaleFactor;
         if ( NULL != p && p->iOSDScreenSize == 1 )
         { ho = hs*0.9; wo = ws*0.9; }
         if ( NULL != p && p->iOSDScreenSize == 2 )
         { ho = hs*0.8; wo = ws*0.8; }
         if ( NULL != p && p->iOSDScreenSize == 3 )
         { ho = hs*0.7; wo = ws*0.7; }
         if ( NULL != p && p->iOSDScreenSize == 4 )
         { ho = hs*0.6; wo = ws*0.6; }
         if ( NULL != p && p->iOSDScreenSize == 5 )
         { ho = hs*0.5; wo = ws*0.5; }
         g_pRenderEngine->drawRoundRect(x, y+0.5*(h-hs)-0.3*h, ws, hs, 0.002*m_sfMenuPaddingY);
         g_pRenderEngine->drawRoundRect(x+0.5*(ws-wo), y+0.5*(h-ho)-0.3*h, wo, ho,0.002*m_sfMenuPaddingY);
         g_pRenderEngine->setColors(get_Color_MenuText());
         //y -= h;
      }
   }
   RenderEnd(yTop);
}

void MenuPreferencesUI::onSelectItem()
{
   Menu::onSelectItem();

   Preferences* p = get_Preferences();
   if ( NULL == p )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }

   if ( m_IndexScaleMenu == m_SelectedIndex && ( ! m_pMenuItems[m_SelectedIndex]->isEditing() ) )
   {
      p->iScaleMenus = m_pItemsSelect[0]->getSelectedIndex()-2;
      if ( render_engine_is_raw() )
      {
         save_Preferences();
         applyFontScaleChanges();
         menu_invalidate_all();
         popups_invalidate_all();
      }
   }

   if ( m_IndexMenuStacked == m_SelectedIndex  && ( ! m_pMenuItems[m_SelectedIndex]->isEditing() ) )
   {
      p->iMenusStacked = m_pItemsSelect[1]->getSelectedIndex();
      save_Preferences();
      return;
   }

   if ( m_IndexInvertColors == m_SelectedIndex  && ( ! m_pMenuItems[m_SelectedIndex]->isEditing() ) )
   {
      p->iInvertColorsOSD = m_pItemsSelect[2]->getSelectedIndex();
      p->iInvertColorsMenu = m_pItemsSelect[2]->getSelectedIndex();
      menu_invalidate_all();
   }

   if ( m_IndexOSDFont == m_SelectedIndex && ( ! m_pMenuItems[m_SelectedIndex]->isEditing() ) )
   {
      p->iOSDFont = m_pItemsSelect[10]->getSelectedIndex();
      save_Preferences();
      loadAllFonts(false);
   }

   if ( m_IndexColorPickerOSD == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuColorPicker());
      return;
   }

   if ( m_IndexColorPickerOSDOutline == m_SelectedIndex )
   {
      MenuColorPicker* mp = new MenuColorPicker();
      mp->m_ColorType = COLORPICKER_TYPE_OSD_OUTLINE;
      add_menu_to_stack(mp);
      return;
   }

   if ( m_IndexColorPickerAHI == m_SelectedIndex )
   {
      MenuColorPicker* mp = new MenuColorPicker();
      mp->m_ColorType = COLORPICKER_TYPE_AHI;
      add_menu_to_stack(mp);
      return;
   }

   if ( m_IndexOSDOutlineThickness == m_SelectedIndex )
   {
      p->iOSDOutlineThickness = m_pItemsSelect[3]->getSelectedIndex()-3;
      save_Preferences();
      osd_apply_preferences();
   }

   if ( m_IndexOSDSize == m_SelectedIndex )
      p->iOSDScreenSize = m_pItemsSelect[6]->getSelectedIndex();

   if ( m_IndexOSDFlip == m_SelectedIndex && ( ! m_pMenuItems[m_SelectedIndex]->isEditing() ) )
      p->iOSDFlipVertical = m_pItemsSelect[7]->getSelectedIndex();

   if ( m_IndexMonitor == m_SelectedIndex )
   {
      p->iShowProcessesMonitor = m_pItemsSelect[12]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      return;
   }
   save_Preferences();
}
