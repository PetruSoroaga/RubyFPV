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
#include "../base/models.h"
#include "../base/config.h"
#include "../base/launchers.h"
#include "../base/commands.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "menu.h"

#include "menu_color_picker.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

#include "shared_vars.h"
#include "ruby_central.h"


MenuColorPicker::MenuColorPicker(void)
:Menu(MENU_ID_COLOR_PICKER, "Color Picker", NULL)
{
   m_Width = 0.1;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.40;

   m_pItemsRange[0] = new MenuItemRange("R:", "", 1, 255, 255, 1 );  
   m_pItemsRange[0]->setSufix("");
   m_IndexColor[0] = addMenuItem(m_pItemsRange[0]);

   m_pItemsRange[1] = new MenuItemRange("G:", "", 1, 255, 255, 1 );  
   m_pItemsRange[1]->setSufix("");
   m_IndexColor[1] = addMenuItem(m_pItemsRange[1]);

   m_pItemsRange[2] = new MenuItemRange("B:", "", 1, 255, 255, 1 );  
   m_pItemsRange[2]->setSufix("");
   m_IndexColor[2] = addMenuItem(m_pItemsRange[2]);

   m_ColorType = COLORPICKER_TYPE_OSD;
}

MenuColorPicker::~MenuColorPicker()
{
}

void MenuColorPicker::valuesToUI()
{
   Preferences* p = get_Preferences();
 
   if ( m_ColorType == COLORPICKER_TYPE_OSD )
   {
      m_pItemsRange[0]->setCurrentValue(p->iColorOSD[0]);
      m_pItemsRange[1]->setCurrentValue(p->iColorOSD[1]);
      m_pItemsRange[2]->setCurrentValue(p->iColorOSD[2]);
   }
   if ( m_ColorType == COLORPICKER_TYPE_OSD_OUTLINE )
   {
      m_pItemsRange[0]->setCurrentValue(p->iColorOSDOutline[0]);
      m_pItemsRange[1]->setCurrentValue(p->iColorOSDOutline[1]);
      m_pItemsRange[2]->setCurrentValue(p->iColorOSDOutline[2]);
   }
   if ( m_ColorType == COLORPICKER_TYPE_AHI )
   {
      m_pItemsRange[0]->setCurrentValue(p->iColorAHI[0]);
      m_pItemsRange[1]->setCurrentValue(p->iColorAHI[1]);
      m_pItemsRange[2]->setCurrentValue(p->iColorAHI[2]);
   }

   m_Color[0] = m_pItemsRange[0]->getCurrentValue();
   m_Color[1] = m_pItemsRange[1]->getCurrentValue();
   m_Color[2] = m_pItemsRange[2]->getCurrentValue();
   m_Color[3] = 1.0;
}


void MenuColorPicker::Render()
{
   RenderPrepare();
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();
   float sizeV = m_RenderHeight - m_RenderHeaderHeight - 2*MENU_MARGINS*menu_getScaleMenus();
   float sizeH = sizeV/g_pRenderEngine->getAspectRatio();

   float w = m_RenderWidth;
   m_RenderWidth += sizeH + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   m_RenderWidth -= sizeH + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   g_pRenderEngine->setColors(&m_Color[0], 1.0);
   g_pRenderEngine->drawRoundRect(m_RenderXPos + m_RenderWidth, yTop, sizeH, sizeV, 0.003*menu_getScaleMenus());
}

void MenuColorPicker::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
   m_Color[0] = m_pItemsRange[0]->getCurrentValue();
   m_Color[1] = m_pItemsRange[1]->getCurrentValue();
   m_Color[2] = m_pItemsRange[2]->getCurrentValue();
   m_Color[3] = 1.0;
}

void MenuColorPicker::onSelectItem()
{
   Menu::onSelectItem();
   Preferences* p = get_Preferences();
   
   if ( m_IndexColor[0] == m_SelectedIndex && ! m_pMenuItems[m_IndexColor[0]]->isEditing() )
   {
      if ( m_ColorType == COLORPICKER_TYPE_OSD )
         p->iColorOSD[0] = m_pItemsRange[0]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_OSD_OUTLINE )
         p->iColorOSDOutline[0] = m_pItemsRange[0]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_AHI )
         p->iColorAHI[0] = m_pItemsRange[0]->getCurrentValue();
      save_Preferences();
   }
   if ( m_IndexColor[1] == m_SelectedIndex && ! m_pMenuItems[m_IndexColor[1]]->isEditing() )
   {
      if ( m_ColorType == COLORPICKER_TYPE_OSD )
         p->iColorOSD[1] = m_pItemsRange[1]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_OSD_OUTLINE )
         p->iColorOSDOutline[1] = m_pItemsRange[1]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_AHI )
         p->iColorAHI[1] = m_pItemsRange[1]->getCurrentValue();
      save_Preferences();
   }
   if ( m_IndexColor[2] == m_SelectedIndex && ! m_pMenuItems[m_IndexColor[2]]->isEditing() )
   {
      if ( m_ColorType == COLORPICKER_TYPE_OSD )
         p->iColorOSD[2] = m_pItemsRange[2]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_OSD_OUTLINE )
         p->iColorOSDOutline[2] = m_pItemsRange[2]->getCurrentValue();
      if ( m_ColorType == COLORPICKER_TYPE_AHI )
         p->iColorAHI[2] = m_pItemsRange[2]->getCurrentValue();
      save_Preferences();
   }
}