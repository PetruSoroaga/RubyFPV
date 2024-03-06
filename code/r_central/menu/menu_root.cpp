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
#include "menu_root.h"
#include "menu_search.h"
#include "menu_vehicles.h"
#include "menu_spectator.h"
#include "menu_vehicle.h"
#include "menu_preferences_buttons.h"
#include "menu_controller.h"
#include "menu_controller_peripherals.h"
#include "menu_controller_radio_interfaces.h"
#include "menu_storage.h"
#include "menu_system.h"
#include "menu_radio_config.h"
#include "menu_item_text.h"

#include "osd_common.h"
#include "../launchers_controller.h"
#include "../link_watch.h"
#include <ctype.h>


const char* s_textRoot[] = { "Welcome to", SYSTEM_NAME, NULL };
static int sCounterRefresh_RootMenu = 0;


MenuRoot::MenuRoot(void)
:Menu(MENU_ID_ROOT, SYSTEM_NAME, NULL)
{
   m_Width = 0.164;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.36;
   //m_bFullWidthSelection = true;

   addMenuItem(new MenuItem("Vehicle Settings","Change vehicle settings."));
   addMenuItem(new MenuItem("My Vehicles","Manage your vehicles."));
   addMenuItem(new MenuItem("Spectator Vehicles", "See the list of vehicles you recently connected to as a spectator."));
   addMenuItem(new MenuItem("Search", "Search for vehicles."));
   //addSeparator();

   addMenuItem(new MenuItem("Radio Configuration", "Change the current radio configuration and radio settings."));
   addMenuItem(new MenuItem("Controller Settings", "Change controller settings and user interface preferences."));
   //addSeparator();

   addMenuItem(new MenuItem("Media & Storage", "Manage saved logs, screenshots and videos."));
   addMenuItem(new MenuItem("System", "Configure system options, shows detailed information about the system"));

   m_pMenuItems[m_ItemsCount-1]->setExtraHeight(Menu::getSelectionPaddingY());
   char szBuff[256];
   char szBuff2[64];
   getSystemVersionString(szBuff2, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
   sprintf(szBuff, "Version %s (b.%d)", szBuff2, SYSTEM_SW_BUILD_NUMBER);

   addMenuItem(new MenuItemText(szBuff, true, 0.01 * Menu::getScaleFactor()));
   sprintf(szBuff, "Running on: %s", str_get_hardware_board_name_short(hardware_getBoardType()));
   addMenuItem(new MenuItemText(szBuff, true, 0.01 * Menu::getScaleFactor()));
}

MenuRoot::~MenuRoot()
{
   log_line("Menu Closed.");
}

void MenuRoot::onShow()
{
   int iPrevSelectedItem = m_SelectedIndex;
   log_line("Entering root menu...");
   load_Preferences();
   sCounterRefresh_RootMenu = 0;
   Menu::onShow();
   if ( iPrevSelectedItem > 0 )
      m_SelectedIndex = iPrevSelectedItem;

   log_line("Entered root menu.");
}


void MenuRoot::RenderVehicleInfo()
{
   Preferences* pP = get_Preferences();

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 3.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   float xPos = m_RenderXPos;
   float yPos = m_RenderYPos;
   float width = m_RenderWidth*1.1 + iconWidth + m_sfMenuPaddingX;
   float height = 2 * m_sfMenuPaddingY;
   float maxTextWidth = width-iconWidth-m_sfMenuPaddingX;
   float lineSpacing = 0.2*m_sfMenuPaddingY;
   float hText1 = 0;
   float hText2 = 0;
   float hText4 = 0;
   float hText5 = 0;
   char szBuff[256];
   char szLine1[128];
   char szLine2[128];
   char szLine4[128];
   char szLine5[128];
   char szRunType[32];

   bool bConnected = false;
   if ( link_has_received_main_vehicle_ruby_telemetry() )
      bConnected = true;
   if ( g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount()) )
      bConnected = false;
   if ( NULL == g_pCurrentModel )
      bConnected = false;
   if ( 0 == g_uActiveControllerModelVID )
      bConnected = false;

   if ( (NULL == g_pCurrentModel) || (0 == g_uActiveControllerModelVID) ||
        (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) )
   {
      hText1 = g_pRenderEngine->getMessageHeight("Not paired with any vehicle", MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText1;
      hText2 = g_pRenderEngine->getMessageHeight("Search for a vehicle to find one and connect to or select one from your paired vehicles.", MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText2 + lineSpacing;
   }
   else
   {
      if ( (NULL != g_pCurrentModel) && link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
      {
         sprintf(szLine1, "Connected to %s", g_pCurrentModel->getLongName() );
         sprintf(szBuff, "Running ver %d.%d on: %s", ((g_pCurrentModel->sw_version>>8) & 0xFF), (g_pCurrentModel->sw_version & 0xFF), str_get_hardware_board_name_short(g_pCurrentModel->hwCapabilities.iBoardType));
         height += g_pRenderEngine->textHeight(g_idFontMenuSmall);
      }
      else
         sprintf(szLine1, "Looking for %s", g_pCurrentModel->getLongName() );

      hText1 = g_pRenderEngine->getMessageHeight(szLine1, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText1;

      if ( g_pCurrentModel->is_spectator )
         strcpy(szLine2, "(Spectator Mode)");
      else
         strcpy(szLine2, "(Full Control Mode)");

      hText2 = g_pRenderEngine->getMessageHeight(szLine2, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText2 + lineSpacing;

      if ( bConnected )
      {
      strcpy(szRunType, "runs");
      if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
         strcpy(szRunType, "flights");
   
      //sprintf(szLine3, "Total %s: %d", szRunType, g_pCurrentModel->stats_TotalFlights);
      //hText3 = g_pRenderEngine->getMessageHeight(szLine3, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      //height += hText3 + lineSpacing;

      int sec = (g_pCurrentModel->m_Stats.uTotalFlightTime)%60;
      int min = (g_pCurrentModel->m_Stats.uTotalFlightTime/60)%60;
      int hours = (g_pCurrentModel->m_Stats.uTotalFlightTime/3600);

      strcpy(szRunType, "run time");
      if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
           (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
         strcpy(szRunType, "flight time");
      sprintf(szLine4, "Total %s: %dh:%02dm:%02ds", szRunType, hours, min, sec);
      hText4 = g_pRenderEngine->getMessageHeight(szLine4, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText4 + lineSpacing;

      if ( pP->iUnits == prefUnitsImperial )
         sprintf(szLine5, "Odometer: %.1f Mi", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
      else
         sprintf(szLine5, "Odometer: %.1f Km", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
      hText5 = g_pRenderEngine->getMessageHeight(szLine5, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText5 + lineSpacing;
      }

      if ( height < iconHeight + 2*m_sfMenuPaddingY )
         height = iconHeight + 2*m_sfMenuPaddingY;
   }

   height += 2.0*height_text;

   yPos -= height + 0.5*m_sfMenuPaddingY;

   g_pRenderEngine->setColors(get_Color_MenuBg(), 0.9);
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, MENU_ROUND_MARGIN*m_sfMenuPaddingY);

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   xPos += m_sfMenuPaddingX;
   yPos += m_sfMenuPaddingY;

   if ( (NULL == g_pCurrentModel) || (0 == g_uActiveControllerModelVID) ||
        (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) )
   {
      g_pRenderEngine->drawMessageLines( xPos, yPos, "Not paired with any vehicle", MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      yPos += hText1 + lineSpacing;
      g_pRenderEngine->drawMessageLines( xPos, yPos, "Search for a vehicle to find one and connect to or select one of your paired vehicles.", MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
   }
   else
   {
      if ( iconWidth > 0.001 )
      {
         u32 idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );

         g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
         g_pRenderEngine->drawIcon(xPos+width - iconWidth -2.2*m_sfMenuPaddingX , yPos+height - iconHeight - 2.0*Menu::getMenuPaddingY(), iconWidth, iconHeight, idIcon);
      }
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

      g_pRenderEngine->drawMessageLines(xPos, yPos, szLine1, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      yPos += hText1;
      yPos += lineSpacing;
      g_pRenderEngine->drawMessageLines(xPos, yPos, szLine2, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      yPos += hText2 + lineSpacing;
      if ( bConnected )
      {
         //g_pRenderEngine->drawMessageLines(xPos, yPos, szLine3, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         //yPos += hText3 + lineSpacing;
         g_pRenderEngine->drawMessageLines(xPos, yPos, szLine4, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         yPos += hText4 + lineSpacing;
         g_pRenderEngine->drawMessageLines(xPos, yPos, szLine5, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         yPos += hText5 + lineSpacing;
      }

      if ( (NULL != g_pCurrentModel) && link_is_vehicle_online_now(g_pCurrentModel->uVehicleId) )
      {
         sprintf(szBuff, "Running ver %d.%d on: %s", ((g_pCurrentModel->sw_version>>8) & 0xFF), (g_pCurrentModel->sw_version & 0xFF), str_get_hardware_board_name_short(g_pCurrentModel->hwCapabilities.iBoardType));
         g_pRenderEngine->drawText(xPos, yPos, g_idFontMenuSmall, szBuff);
         yPos += g_pRenderEngine->textHeight(g_idFontMenuSmall);
      }
   }
}

void MenuRoot::Render()
{
   RenderPrepare();

   sCounterRefresh_RootMenu++;

   m_RenderHeight -= 1.2 * g_pRenderEngine->textHeight(g_idFontMenu);

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   m_RenderHeight += 1.2 * g_pRenderEngine->textHeight(g_idFontMenu);
   
   RenderVehicleInfo();

   bool bTmp1 = m_bEnableScrolling;
   bool bTmp2 = m_bHasScrolling;
   m_bEnableScrolling = false;
   m_bHasScrolling = false;

   RenderItem(0, m_RenderYPos - 0.02*m_sfMenuPaddingY - g_pRenderEngine->textHeight(g_idFontMenu) - 1.5*m_sfMenuPaddingY);

   m_bEnableScrolling = bTmp1;
   m_bHasScrolling = bTmp2;

   for( int i=1; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuRoot::createAboutInfo(Menu* pm)
{
#ifdef FEATURE_CHECK_LICENCES
   pm->addTopLine(" ");
   pm->addTopLine(" ");
   pm->addTopLine(" ");
   pm->addTopLine("Developed by: Petru Soroaga");
#else
   pm->addTopLine(" ");
   pm->addTopLine("---");
   pm->addTopLine(" ");
   pm->addTopLine("Developed by: Petru Soroaga");
   pm->addTopLine("Contributors: Tree Orbit, Piotr Kujawski, Alexey Belyaev");
   pm->addTopLine("Uses platform from: OpenIPC ");
   pm->addTopLine(" ");
   pm->addTopLine("For info on the licence terms, check the licence.txt file.");
   pm->addTopLine(" ");
   pm->addTopLine("---");
   pm->addTopLine(" ");

   pm->addTopLine("For more info, questions and suggestions find us on www.rubyfpv.com");
   pm->addTopLine(" ");
#endif
}

void MenuRoot::createHWInfo(Menu* pm)
{
   FILE* fp = NULL;
   char szBuff[256];
   char szTemp[256];
   char szFileName[256];
   int boardType = 0;
   int wifiType = 0;

   log_line("Menu System: create HW info.");

   sprintf(szFileName, "%s/board.txt", FOLDER_CONFIG);
   fp = fopen(szFileName,"r");
   if ( NULL != fp )
   {
      fscanf(fp,"%d", &boardType);
      fclose(fp);
      fp = NULL;
   }
         
   sprintf(szBuff, "Board: %s, ", str_get_hardware_board_name(boardType));
   
   sprintf(szFileName, "%s/wifi.txt", FOLDER_CONFIG);
   fp = fopen(szFileName,"r");
   if ( NULL != fp )
   {
      fscanf(fp,"%d",&wifiType);
      fclose(fp);
      fp = NULL;
   }
   if ( wifiType != 0 )
   {
      strcat(szBuff, "built in WiFi: ");
      char szT[32];
      sprintf(szT, "%s. ", str_get_hardware_wifi_name(wifiType));
      strcat(szBuff, szT);
   }
   else
      strcat(szBuff, "no built in WiFi. ");

   int temp = 0;
   fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
   if ( NULL != fp )
   {
      fscanf(fp, "%d", &temp);
      fclose(fp);
      fp = NULL;
   }
   int speed = hardware_get_cpu_speed();
   sprintf(szTemp, "CPU: %d Mhz, Temp: %d C", speed, temp/1000); 
   strcat(szBuff, szTemp);
   pm->addTopLine(szBuff);

   pm->addTopLine(" ");
   pm->addTopLine(" ");
}


void MenuRoot::show_MenuInfo()
{
   char szBuff[1024];
   char szOutput[1024];

   MenuSystem* pm = new MenuSystem();
   pm->m_xPos = menu_get_XStartPos(pm->m_Width);
   pm->m_yPos = 0.14;
   pm->m_Width = 0.42;

   szBuff[0] = 0;      
   strcpy(szBuff, "Ruby base version: N/A");

   char szFile[128];
   strcpy(szFile, FOLDER_BINARIES);
   strcat(szFile, FILE_INFO_VERSION);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
      fd = fopen("ruby_ver.txt", "r");

   if ( NULL != fd )
   {
      szOutput[0] = 0;
      if ( 1 == fscanf(fd, "%s", szOutput) )
      {
         strcpy(szBuff, "Ruby base version: ");
         strcat(szBuff, szOutput);
      }
      fclose(fd);
   }

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_INFO_LAST_UPDATE);
   fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      szOutput[0] = 0;
      if ( 1 == fscanf(fd, "%s", szOutput) )
      {
         strcat(szBuff, ", last update: ");
         strcat(szBuff, szOutput);
      }
      fclose(fd);
   }
   
   pm->addTopLine(szBuff);

   pm->addTopLine(" ");

   createHWInfo(pm);

   if ( 1 == hw_execute_bash_command_raw("df -m /home/pi/ruby | grep root", szBuff) )
   {
      char szTemp[1024];
      long lb, lu, lf;
      sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lb, &lu, &lf);
      sprintf(szBuff, "System storage: %ld Mb free out of %ld Mb total.", lf, lu+lf);
      pm->addTopLine(szBuff);
   }
   if ( 1 == hw_execute_bash_command_raw("free -m  | grep Mem", szBuff) )
   {
      char szTemp[1024];
      long lt, lu, lf;
      sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lt, &lu, &lf);
      sprintf(szBuff, "System memory: %ld Mb free out of %ld Mb total.", lf, lt);
      pm->addTopLine(szBuff);
   }

   createAboutInfo(pm);
   add_menu_to_stack(pm);
   return;
}


void MenuRoot::onSelectItem()
{
   if ( 0 == m_SelectedIndex )
   {
      if ( (NULL == g_pCurrentModel) || (0 == g_uActiveControllerModelVID) ||
        (g_bFirstModelPairingDone && (0 == getControllerModelsCount()) && (0 == getControllerModelsSpectatorCount())) )
      {
         addMessage2(0, "Not paired with any vehicle.", "Search for vehicles to find one and connect to.");
         return;
      }
      add_menu_to_stack(new MenuVehicle());
      return;
   }

   if ( 1 == m_SelectedIndex )
         add_menu_to_stack(new MenuVehicles());

   if ( 2 == m_SelectedIndex )
         add_menu_to_stack(new MenuSpectator());

   if ( 3 == m_SelectedIndex )
         add_menu_to_stack(new MenuSearch());

   if ( 4 == m_SelectedIndex )
      add_menu_to_stack(new MenuRadioConfig()); 

   if ( 5 == m_SelectedIndex )
      add_menu_to_stack(new MenuController()); 

   if ( 6 == m_SelectedIndex )
         add_menu_to_stack(new MenuStorage());

   if ( 7 == m_SelectedIndex )
      show_MenuInfo();
}
