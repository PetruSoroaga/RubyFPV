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
#include "../base/hw_procs.h"
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


#include "../base/config.h"
#include "../base/ctrl_interfaces.h"
#include "../common/string_utils.h"
#include "shared_vars.h"
#include "pairing.h"
#include "../base/launchers.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "osd_common.h"
#include "launchers_controller.h"
#include <ctype.h>
#include "timers.h"


const char* s_textRoot[] = { "Welcome to", SYSTEM_NAME, NULL };
static int sCounterRefresh_RootMenu = 0;


MenuRoot::MenuRoot(void)
:Menu(MENU_ID_ROOT, SYSTEM_NAME, NULL)
{
   m_Width = 0.16;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.36;
   m_bFullWidthSelection = true;

   addMenuItem(new MenuItem("Vehicle Settings","Change vehicle settings."));
   addMenuItem(new MenuItem("My Vehicles","Manage your vehicles."));
   addMenuItem(new MenuItem("Spectator Vehicles", "See the list of vehicles you recently connected to as a spectator."));
   addMenuItem(new MenuItem("Search", "Search for vehicles."));
   addSeparator();

   addMenuItem(new MenuItem("Radio Configuration", "Change the current radio configuration and radio settings."));
   addMenuItem(new MenuItem("Controller Settings", "Change controller settings and user interface preferences."));
   addSeparator();

   addMenuItem(new MenuItem("Media & Storage", "Manage saved logs, screenshots and videos."));
   addMenuItem(new MenuItem("System", "Configure system options, shows detailed information about the system"));

   hasChanged = true;
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
   m_Height = 0.0;
   sCounterRefresh_RootMenu = 0;
   Menu::onShow();
   if ( iPrevSelectedItem > 0 )
      m_SelectedIndex = iPrevSelectedItem;
   log_line("Entered root menu.");
}


void MenuRoot::RenderVehicleInfo()
{
   Preferences* pP = get_Preferences();

   float height_text = g_pRenderEngine->textHeight(0.94*MENU_FONT_SIZE_TOOLTIPS*menu_getScaleMenus(), g_idFontMenu);
   float iconHeight = 4.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   float xPos = m_RenderXPos;
   float yPos = m_RenderYPos;
   float width = m_RenderWidth*1.1 + iconWidth + MENU_MARGINS*menu_getScaleMenus();
   float height = 2 * MENU_MARGINS*menu_getScaleMenus();
   float maxTextWidth = width-iconWidth-MENU_MARGINS*menu_getScaleMenus();
   float lineSpacing = 0.2*MENU_MARGINS*menu_getScaleMenus();
   float hText1 = 0;
   float hText2 = 0;
   float hText3 = 0;
   float hText4 = 0;
   float hText5 = 0;
   char szLine1[128];
   char szLine2[128];
   char szLine3[128];
   char szLine4[128];
   char szLine5[128];
   char szRunType[32];

   bool bConnected = false;
   if ( pairing_isReceiving() || pairing_wasReceiving() )
   if ( ! pairing_is_connected_to_wrong_model() )
      bConnected = true;

   if ( NULL == g_pCurrentModel )
   {
      hText1 = g_pRenderEngine->getMessageHeight("Not paired with any vehicle", height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText1;
   }
   else
   {
      if ( pairing_isReceiving() )
         snprintf(szLine1, sizeof(szLine1), "Connected to %s", g_pCurrentModel->getLongName() );
      else
         snprintf(szLine1, sizeof(szLine1), "Looking for %s", g_pCurrentModel->getLongName() );

      hText1 = g_pRenderEngine->getMessageHeight(szLine1, height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText1;

      if ( g_pCurrentModel->is_spectator )
         strcpy(szLine2, "(Spectator Mode)");
      else
         strcpy(szLine2, "(Full Control Mode)");

      hText2 = g_pRenderEngine->getMessageHeight(szLine2, height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText2 + lineSpacing;

      if ( bConnected )
      {
      strcpy(szRunType, "runs");
      if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
         g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
         g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
         strcpy(szRunType, "flights");
   
      //snprintf(szLine3, sizeof(szLine3), "Total %s: %d", szRunType, g_pCurrentModel->stats_TotalFlights);
      //hText3 = g_pRenderEngine->getMessageHeight(szLine3, height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      //height += hText3 + lineSpacing;

      int sec = (g_pCurrentModel->m_Stats.uTotalFlightTime)%60;
      int min = (g_pCurrentModel->m_Stats.uTotalFlightTime/60)%60;
      int hours = (g_pCurrentModel->m_Stats.uTotalFlightTime/3600);

      strcpy(szRunType, "run time");
      if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
         g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
         g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
         strcpy(szRunType, "flight time");
      snprintf(szLine4, sizeof(szLine4), "Total %s: %dh:%02dm:%02ds", szRunType, hours, min, sec);
      hText4 = g_pRenderEngine->getMessageHeight(szLine4, height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText4 + lineSpacing;

      if ( pP->iUnits == prefUnitsImperial )
         snprintf(szLine5, sizeof(szLine5), "Odometer: %.1f Mi", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
      else
         snprintf(szLine5, sizeof(szLine5), "Odometer: %.1f Km", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
      hText5 = g_pRenderEngine->getMessageHeight(szLine5, height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      height += hText5 + lineSpacing;
      }

      if ( height < iconHeight + 2*MENU_MARGINS*menu_getScaleMenus() )
         height = iconHeight + 2*MENU_MARGINS*menu_getScaleMenus();
   }

   height += 2.0*height_text;

   yPos -= height + 0.02*menu_getScaleMenus();

   g_pRenderEngine->setColors(get_Color_MenuBg(), 0.9);
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, MENU_ROUND_MARGIN*menu_getScaleMenus());

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   xPos += MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   yPos += MENU_MARGINS*menu_getScaleMenus();

   if ( NULL == g_pCurrentModel )
   {
      g_pRenderEngine->drawMessageLines( xPos, yPos, "Not paired with any vehicle", height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
   }
   else
   {
      if ( iconWidth > 0.001 )
      {
         u32 idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );

         g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
         g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
         g_pRenderEngine->drawIcon(xPos+width - iconWidth -2.2*MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() , yPos+iconHeight*0.05, iconWidth, iconHeight, idIcon);
         //xPos += iconWidth + 0.6*MENU_MARGINS*menu_getScaleMenus();
      }
      g_pRenderEngine->setColors(get_Color_MenuText());
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

      g_pRenderEngine->drawMessageLines(xPos, yPos, szLine1, height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      yPos += hText1 + lineSpacing;
      g_pRenderEngine->drawMessageLines(xPos, yPos, szLine2, height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
      yPos += hText2 + lineSpacing;
      if ( bConnected )
      {
         //g_pRenderEngine->drawMessageLines(xPos, yPos, szLine3, height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         //yPos += hText3 + lineSpacing;
         g_pRenderEngine->drawMessageLines(xPos, yPos, szLine4, height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         yPos += hText4 + lineSpacing;
         g_pRenderEngine->drawMessageLines(xPos, yPos, szLine5, height_text, MENU_TEXTLINE_SPACING, maxTextWidth, g_idFontMenu);
         yPos += hText5 + lineSpacing;
      }
   }
}

void MenuRoot::Render()
{
   RenderPrepare();

   sCounterRefresh_RootMenu++;

   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus(), g_idFontMenu);
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   RenderVehicleInfo();

   bool bTmp1 = m_bEnableScrolling;
   bool bTmp2 = m_bHasScrolling;

   m_bEnableScrolling = false;
   m_bHasScrolling = false;

   RenderItem(0, m_RenderYPos - 0.02*menu_getScaleMenus() - height_text - MENU_MARGINS*menu_getScaleMenus());

   m_bEnableScrolling = bTmp1;
   m_bHasScrolling = bTmp2;

   for( int i=1; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   char szBuff[256];
   char szBuff2[64];
   getSystemVersionString(szBuff2, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);

   snprintf(szBuff, sizeof(szBuff), "Version %s (b.%d)", szBuff2, SYSTEM_SW_BUILD_NUMBER);

   y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS;
   y += g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio(), y, szBuff, MENU_FONT_SIZE_TOOLTIPS*menu_getScaleMenus()*0.9, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   y += 0.3 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS;
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
   pm->addTopLine(" ");
   pm->addTopLine(" ");
   pm->addTopLine("Developed by: Petru Soroaga");
   pm->addTopLine("Contributors: Tree Orbit, Piotr Kujawski (aka bitkuna)");
   pm->addTopLine(" ");
   pm->addTopLine("For info on the licence terms, check the licence.txt file.");
   pm->addTopLine(" ");
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
   char szBoard[256];
   szBoard[0] = 0;
   int boardType = 0;
   int wifiType = 0;

   log_line("Menu System: create HW info.");

   snprintf(szFileName, sizeof(szFileName), "%s/board.txt", FOLDER_CONFIG);
   fp = fopen(szFileName,"r");
   if ( NULL != fp )
   {
      fscanf(fp,"%d", &boardType);
      fclose(fp);
   }
         
   snprintf(szBuff, sizeof(szBuff), "Board: %s, ", str_get_hardware_board_name(boardType));
   
   snprintf(szFileName, sizeof(szFileName), "%s/wifi.txt", FOLDER_CONFIG);
   fp = fopen(szFileName,"r");
   if ( NULL != fp )
   {
      fscanf(fp,"%d",&wifiType);
      fclose(fp);
   }
   if ( wifiType != 0 )
   {
      strlcat(szBuff, "built in WiFi: ", sizeof(szBuff));
      char szT[32];
      snprintf(szT, sizeof(szT), "%s. ", str_get_hardware_wifi_name(wifiType));
      strlcat(szBuff, szT, sizeof(szBuff));
   }
   else
      strlcat(szBuff, "no built in WiFi. ", sizeof(szBuff));

   int temp = 0;
   fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
   fscanf(fp, "%d", &temp);
   fclose(fp);

   int speed = hardware_get_cpu_speed();
   snprintf(szTemp, sizeof(szTemp), "CPU: %d Mhz, Temp: %d C", speed, temp/1000); 
   strlcat(szBuff, szTemp, sizeof(szBuff));
   pm->addTopLine(szBuff);

   u16 flags = hardware_get_flags();
   
   pm->addTopLine(" ");
   pm->addTopLine(" ");
}


void MenuRoot::show_MenuInfo()
{
   char szTitle[256];
   char szBuff[1024];
   char szOutput[1024];

   MenuSystem* pm = new MenuSystem();
   pm->m_xPos = menu_get_XStartPos(pm->m_Width);
   pm->m_yPos = 0.14;
   pm->m_Width = 0.42;

   szBuff[0] = 0;      
   strcpy(szBuff, "Ruby base version: N/A");

   FILE* fd = fopen(FILE_INFO_VERSION, "r");
   if ( NULL == fd )
      fd = fopen("ruby_ver.txt", "r");

   if ( NULL != fd )
   {
      szOutput[0] = 0;
      if ( 1 == fscanf(fd, "%s", szOutput) )
      {
         strcpy(szBuff, "Ruby base version: ");
         strlcat(szBuff, szOutput, sizeof(szBuff));
      }
      fclose(fd);
   }

   fd = fopen(FILE_INFO_LAST_UPDATE, "r");
   if ( NULL != fd )
   {
      szOutput[0] = 0;
      if ( 1 == fscanf(fd, "%s", szOutput) )
      {
         strlcat(szBuff, ", last update: ", sizeof(szBuff));
         strlcat(szBuff, szOutput, sizeof(szBuff));
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
      snprintf(szBuff, sizeof(szBuff), "System storage: %ld Mb free out of %ld Mb total.", lf, lu+lf);
      pm->addTopLine(szBuff);
   }
   if ( 1 == hw_execute_bash_command_raw("free -m  | grep Mem", szBuff) )
   {
      char szTemp[1024];
      long lt, lu, lf;
      sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lt, &lu, &lf);
      snprintf(szBuff, sizeof(szBuff), "System memory: %ld Mb free out of %ld Mb total.", lf, lt);
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

