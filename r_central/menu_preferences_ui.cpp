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
#include "../base/hardware.h"
#include "menu.h"
#include "menu_preferences_ui.h"
#include "shared_vars.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_color_picker.h"
#include "popup_log.h"
#include "ruby_central.h"

MenuPreferencesUI::MenuPreferencesUI(void)
:Menu(MENU_ID_PREFERENCES_UI, "User Interface Fonts, Colors and Sizes", NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.12;

   addMenuItem(new MenuItemSection("Menus"));

   m_pItemsSelect[0] = new MenuItemSelect("Menu Font Size", "Change how big the menus appear on screen.");  
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

   m_pItemsSelect[8] = new MenuItemSelect("Display Units", "Changes how the OSD displays data: in metric system or imperial system.");  
   m_pItemsSelect[8]->addSelection("Metric (km/h)");
   m_pItemsSelect[8]->addSelection("Metric (m/s)");
   m_pItemsSelect[8]->addSelection("Imperial (mi/h)");
   m_pItemsSelect[8]->addSelection("Imperial (ft/s)");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexUnits = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[9] = new MenuItemSelect("Persistent Messages", "Keep the various messages and warnings longer on the screen.");  
   m_pItemsSelect[9]->addSelection("No");
   m_pItemsSelect[9]->addSelection("Yes");
   m_pItemsSelect[9]->setIsEditable();
   m_IndexPersistentMessages = addMenuItem(m_pItemsSelect[9]);

   m_pItemsSelect[11] = new MenuItemSelect("Log Messages Window", "Shows the log messages window.");  
   m_pItemsSelect[11]->addSelection("Off");
   m_pItemsSelect[11]->addSelection("On New Content");
   m_pItemsSelect[11]->addSelection("Always On");
   m_pItemsSelect[11]->setIsEditable();
   m_IndexLogWindow = addMenuItem(m_pItemsSelect[11]);
   
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

   m_pItemsSelect[0]->setSelection(p->iScaleMenus+1);
   m_pItemsSelect[1]->setSelection(p->iMenusStacked);

   m_pItemsSelect[2]->setSelection(p->iInvertColorsOSD);
   //m_pItemsSelect[3]->setSelection(p->iOSDOutlineThickness+3);
   m_pItemsSelect[6]->setSelection(p->iOSDScreenSize);
   m_pItemsSelect[7]->setSelection(p->iOSDFlipVertical);

   if ( p->iUnits == prefUnitsMetric )
      m_pItemsSelect[8]->setSelection(0);
   if ( p->iUnits == prefUnitsMeters )
      m_pItemsSelect[8]->setSelection(1);
   if ( p->iUnits == prefUnitsImperial )
      m_pItemsSelect[8]->setSelection(2);
   if ( p->iUnits == prefUnitsFeets )
      m_pItemsSelect[8]->setSelection(3);

   m_pItemsSelect[9]->setSelection(p->iPersistentMessages);
   m_pItemsSelect[10]->setSelectedIndex(p->iOSDFont);

   m_pItemsSelect[11]->setSelection(p->iShowLogWindow);
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
         g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth-MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio()-h*1.5, y, h*1.5, h, 0.003*menu_getScaleMenus());
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
         g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth-MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio()-h*1.5, y, h*1.5, h, 0.003*menu_getScaleMenus());
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
         g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth-MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio()-h*1.5, y, h*1.5, h, 0.003*menu_getScaleMenus());
         g_pRenderEngine->setColors(get_Color_MenuText());
      }

      y += RenderItem(i,y);

      if ( i == m_IndexOSDSize-1 )
      {
         g_pRenderEngine->setColors(get_Color_MenuBg());
         g_pRenderEngine->setStroke(get_Color_MenuText());
         Preferences* p = get_Preferences();
         float h = 2*menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS*(1+MENU_TEXTLINE_SPACING);
         float hs = h*0.7;
         float ws = hs;
         float ho = hs;
         float wo = ws;
         float dy = h*0.2;
         float x = m_xPos+m_Width*menu_getScaleMenus()-2*MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio()-ws;
         x -= 0.064*menu_getScaleMenus();
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
         g_pRenderEngine->drawRoundRect(x, y+0.5*(h-hs)-0.2*h, ws, hs, 0.002*menu_getScaleMenus());
         g_pRenderEngine->drawRoundRect(x+0.5*(ws-wo), y+0.5*(h-ho)-0.2*h, wo, ho,0.002*menu_getScaleMenus());
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
      p->iScaleMenus = m_pItemsSelect[0]->getSelectedIndex()-1;
      if ( render_engine_is_raw() )
      {
         save_Preferences();
         apply_Preferences();
         ruby_reload_menu_font();
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
      apply_Preferences();
      ruby_reload_osd_fonts();
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
      p->iOSDOutlineThickness = m_pItemsSelect[3]->getSelectedIndex()-3;

   if ( m_IndexOSDSize == m_SelectedIndex )
      p->iOSDScreenSize = m_pItemsSelect[6]->getSelectedIndex();

   if ( m_IndexOSDFlip == m_SelectedIndex && ( ! m_pMenuItems[m_SelectedIndex]->isEditing() ) )
      p->iOSDFlipVertical = m_pItemsSelect[7]->getSelectedIndex();

   if ( m_IndexUnits == m_SelectedIndex && ( ! m_pMenuItems[m_SelectedIndex]->isEditing() ) )
   {
      int nSel = m_pItemsSelect[8]->getSelectedIndex();
      if ( 0 == nSel )
         p->iUnits = prefUnitsMetric;
      if ( 1 == nSel )
         p->iUnits = prefUnitsMeters;
      if ( 2 == nSel )
         p->iUnits = prefUnitsImperial;
      if ( 3 == nSel )
         p->iUnits = prefUnitsFeets;
   }

   if ( m_IndexPersistentMessages == m_SelectedIndex )
      p->iPersistentMessages = m_pItemsSelect[9]->getSelectedIndex();

   if ( m_IndexLogWindow == m_SelectedIndex )
   {
      p->iShowLogWindow = m_pItemsSelect[11]->getSelectedIndex();
      popup_log_set_show_flag(p->iShowLogWindow);
      menu_invalidate_all();
      valuesToUI();
      return;
   }

   if ( m_IndexMonitor == m_SelectedIndex )
   {
      p->iShowProcessesMonitor = m_pItemsSelect[12]->getSelectedIndex();
      save_Preferences();
      valuesToUI();
      return;
   }
   save_Preferences();
   apply_Preferences();
}
