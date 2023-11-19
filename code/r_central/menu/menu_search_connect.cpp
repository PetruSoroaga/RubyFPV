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

#include "menu_search_connect.h"
#include "../../common/models_connect_frequencies.h"
#include "../osd/osd_common.h"
#include "menu.h"

MenuSearchConnect::MenuSearchConnect(void)
//:Menu(MENU_ID_SEARCH_CONNECT, "Found Vehicle", "")
:Menu(MENU_ID_CONFIRMATION, "Found Vehicle", "") // Id confirmation to keep the parent menu on screen too
{
   m_Width = 0.38;
   m_xPos = menu_get_XStartPos(this, m_Width);
   m_yPos = 0.32;
   m_bSpectatorOnlyMode = false;

   int iCountLinks = 0;
   if ( g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo )
   for( int i=0; i<g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count; i++ )
   {
      if ( ! (g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRelayLinks & (1<<i)) )
         iCountLinks++;
   }

   if ( iCountLinks > 1 )
      m_Width = 0.46;
   addMenuItem(new MenuItem("Connect for control", "Connect to this vehicle as controller"));
   addMenuItem(new MenuItem("View as spectator", "Connect to this vehicle as spectator"));
   addMenuItem(new MenuItem("Skip", "Skip this vehicle"));
   m_ExtraItemsHeight = 1.8*g_pRenderEngine->textHeight(g_idFontMenu);
}

void MenuSearchConnect::setSpectatorOnly()
{
   m_bSpectatorOnlyMode = true;
   m_pMenuItems[1]->setEnabled(false);
}

void MenuSearchConnect::setCurrentFrequency(u32 freq)
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
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 3.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   float xPos = m_xPos + 3*m_sfMenuPaddingX + getSelectionWidth();
   float fMaxWidth = getUsableWidth() - 3*m_sfMenuPaddingX - getSelectionWidth();
   y = y0;

   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo )
   {
      u32 idIcon = osd_getVehicleIcon( g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_type );

      g_pRenderEngine->setColors(get_Color_MenuText(), 0.8);
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->setColors(get_Color_MenuText());
   }
    
   sprintf(szBuff,"Found vehicle on %s", str_format_frequency(m_CurrentSearchFrequency));
   g_pRenderEngine->drawMessageLines(xPos, y, szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);

   y += height_text *(1.0+MENU_ITEM_SPACING);
   if ( ! g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo )
   {
      sprintf(szBuff, "Can't get vehicle info");
      g_pRenderEngine->drawMessageLines(xPos, y, szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);
      y += height_text *(1.0+MENU_ITEM_SPACING);
      RenderEnd(yTop);
      return;
   }

   sprintf(szBuff,"Type: %s, Name: ", Model::getVehicleType(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_type));
   if ( 0 == g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_name[0] )
      strcat(szBuff, "No Name");
   else
      strncat(szBuff, (char*)g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_name, MAX_VEHICLE_NAME_LENGTH);
   g_pRenderEngine->drawMessageLines(xPos, y, szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);
   y += height_text *(1.0+MENU_ITEM_SPACING);

   char szLinks[256];
   szLinks[0] = 0;
   int iCountLinks = 0;
   if ( g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo )
   for( int i=0; i<g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count; i++ )
   {
      // Not a relay link (bit N - relay link)
      if ( ! (g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRelayLinks & (1<<i)) )
      {
         iCountLinks++;
         if ( 0 != szLinks[0] )
            strcat(szLinks, ", ");
         char szTmp[32];
         sprintf(szTmp, "%s", str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[i]) );
         strcat(szLinks, szTmp);
      }
   }

   if ( iCountLinks > 1 )
   {
      sprintf(szBuff, "Vehicle has %d radio links on: %s", iCountLinks, szLinks);
      g_pRenderEngine->drawMessageLines(xPos, y, szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);
      y += g_pRenderEngine->getMessageHeight(szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);
      y += height_text * MENU_ITEM_SPACING;    
   }
   u8 vMaj = g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.version;
   u8 vMin = g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.version;
   vMaj = vMaj >> 4;
   vMin = vMin & 0x0F;
   //sprintf(szBuff,"Id: %u, ver: %d.%d", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id, vMaj, vMin);
   sprintf(szBuff,"Version: %d.%d", vMaj, vMin);
   g_pRenderEngine->drawMessageLines(xPos, y, szBuff, MENU_TEXTLINE_SPACING, fMaxWidth, g_idFontMenu);
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
   
   // Add model as controller
   if ( 0 == m_SelectedIndex )
   {
      menu_stack_pop(2);
      warnings_remove_all();
      return;
   }

   // Add model as spectator
   if ( 1 == m_SelectedIndex )
   {
      menu_stack_pop(1);
      warnings_remove_all();
      return;
   }

   // Skip vehicle
   if ( 2 == m_SelectedIndex )
   {
      menu_stack_pop(0);
      return;
   }
}
