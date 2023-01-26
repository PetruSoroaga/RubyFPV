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
#include "menu_search_connect.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/shared_mem.h"
#include "../base/launchers.h"
#include "../radio/radiopackets2.h"
#include "../base/commands.h"
#include "../base/hardware.h"
#include "../base/ctrl_interfaces.h"
#include "shared_vars.h"
#include "osd_common.h"
#include "menu.h"
#include "popup.h"
#include "pairing.h"
#include "ruby_central.h"
#include "local_stats.h"
#include "timers.h"
#include "events.h"


MenuSearchConnect::MenuSearchConnect(void)
//:Menu(MENU_ID_SEARCH_CONNECT, "Found Vehicle", "")
:Menu(MENU_ID_CONFIRMATION, "Found Vehicle", "") // Id confirmation to keep the parent menu on screen too
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.32;
   m_bSpectatorOnlyMode = false;

   addMenuItem(new MenuItem("View as spectator", "Connect to this vehicle as spectator"));
   addMenuItem(new MenuItem("Connect for control", "Connect to this vehicle as controller"));
   addMenuItem(new MenuItem("Skip", "Skip this vehicle"));
   m_ExtraHeight = 1.0*g_pRenderEngine->textHeight(0.8*menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, g_idFontMenu);
}

void MenuSearchConnect::setSpectatorOnly()
{
   m_bSpectatorOnlyMode = true;
   m_pMenuItems[1]->setEnabled(false);
}

void MenuSearchConnect::setCurrentFrequency(int freq)
{
   m_CurrentSearchFrequency = freq;
}

void MenuSearchConnect::onShow()
{
   Menu::onShow();
}

void MenuSearchConnect::Render()
{
   RenderPrepare();
   float yTop = Menu::RenderFrameAndTitle();
   float y0 = yTop;
   float y = y0;
   for( int i=0; i<3; i++ )
      y += RenderItem(i,y);

   char szBuff[128];
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();
   float iconHeight = 3.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   float xPos = m_xPos+ 2*MENU_MARGINS*menu_getScaleMenus() + getSelectionWidth();
   y = y0;

   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( NULL != g_pPHRTE )
   {
      u32 idIcon = g_idIconRuby;
      if ( NULL != g_pPHRTE )
         idIcon = osd_getVehicleIcon( g_pPHRTE->vehicle_type );

      g_pRenderEngine->setColors(get_Color_MenuText(), 0.8);
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      //g_pRenderEngine->drawIcon(m_RenderXPos+m_RenderWidth - iconWidth - 1.2*MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() , y+iconHeight*0.05, iconWidth, iconHeight, idIcon);
      g_pRenderEngine->setColors(get_Color_MenuText());
   }
    
   sprintf(szBuff,"Found vehicle on %d Mhz", m_CurrentSearchFrequency);
   g_pRenderEngine->drawMessageLines(xPos, y, szBuff, height_text, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);

   y += height_text *(1.0+MENU_ITEM_SPACING);
   if ( NULL == g_pPHRTE )
   {
      sprintf(szBuff, "Can't get vehicle info");
      g_pRenderEngine->drawMessageLines(xPos, y, szBuff, height_text, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
      y += height_text *(1.0+MENU_ITEM_SPACING);
      RenderEnd(yTop);
      return;
   }

   sprintf(szBuff,"Type: %s, Name: ", Model::getVehicleType(g_pPHRTE->vehicle_type));
   if ( 0 == g_pPHRTE->vehicle_name[0] )
      strcat(szBuff, "No Name");
   else
      strncat(szBuff, (char*)g_pPHRTE->vehicle_name, MAX_VEHICLE_NAME_LENGTH);
   g_pRenderEngine->drawMessageLines(xPos, y, szBuff, height_text, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   y += height_text *(1.0+MENU_ITEM_SPACING);

   u8 vMaj = g_pPHRTE->version;
   u8 vMin = g_pPHRTE->version;
   vMaj = vMaj >> 4;
   vMin = vMin & 0x0F;
   //sprintf(szBuff,"Id: %u, ver: %d.%d", g_pPHRTE->vehicle_id, vMaj, vMin);
   sprintf(szBuff,"Version: %d.%d", vMaj, vMin);
   g_pRenderEngine->drawMessageLines(xPos, y, szBuff, height_text, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   y += height_text *(1.0+MENU_ITEM_SPACING);
   y += height_text *0.6;

   RenderEnd(yTop);
}

int MenuSearchConnect::onBack()
{
   log_line("MenuSearchConnect::onBack()");
   log_line("Default back");
   return 0;
}

void MenuSearchConnect::onSelectItem()
{
   Menu::onSelectItem();
   
   // Add model as spectator
   if ( 0 == m_SelectedIndex )
   {
      menu_stack_pop(1);
      return;
   }

   // Add model as controller
   if ( 1 == m_SelectedIndex )
   {
      menu_stack_pop(2);
      return;
   }

   // Skip vehicle
   if ( 2 == m_SelectedIndex )
   {
      menu_stack_pop(0);
      return;
   }
}
