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

#include <stdio.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <locale.h>
#include <time.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/resource.h>

#include "ruby_central.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/shared_mem.h"
#include "../base/base.h"
#include "../base/hardware.h"
#include "../base/hdmi.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/controller_utils.h"
#include "../base/plugins_settings.h"
#include "../base/launchers.h"
#include "../base/commands.h"
#include "../base/ruby_ipc.h"
#include "../base/core_plugins_settings.h"
#include "../renderer/render_engine.h"
#include "../common/string_utils.h"

#include "colors.h"
#include "osd.h"
#include "osd_common.h"
#include "osd_plugins.h"
#include "menu.h"
#include "popup.h"
#include "shared_vars.h"
#include "pairing.h"
#include "link_watch.h"
#include "warnings.h"
#include "media.h"
#include "render_commands.h"
#include "handle_commands.h"
#include "menu_confirmation.h"
#include "menu_confirmation_hdmi.h"
#include "ui_alarms.h"
#include "notifications.h"
#include "local_stats.h"
#include "rx_scope.h"
#include "forward_watch.h"
#include "timers.h"
#include "launchers_controller.h"
#include "events.h"
#include "menu_info_booster.h"
#include "menu_confirmation_import.h"
#include "process_router_messages.h"

u32 s_idBgImage = 0;
u32 s_idBgImageMenu = 0;

bool g_bIsReinit = false;
bool g_bIsHDMIConfirmation = false;
bool g_bDebug = false;
bool quit = false;

static int s_iRubyFPS = 0;
static int s_iFPSCount = 0;
static u32 s_iFPSLastTimeCheck = 0;

static int s_iCountRequestsPauseWatchdog = 0;

static u32 s_iMicroTimeMenuRender = 0;
static u32 s_iMicroTimeOSDRender = 0;

static u32 s_TimeLastRender = 0;
static u32 s_TimeLastCPUCompute = 0;
static u32 s_TimeLastCPUSpeed = 0;

static u32 s_TimeCentralInitializationComplete = 0;
static u32 s_TimeLastMenuInput = 0;
static u32 s_PacketsScopeGraphZoomLevel = 1; // 0,1,2
static u32 s_TimeLastRecordingStop = 0;

static bool s_bFreezeOSD = false;
static u32 s_uTimeFreezeOSD = 0;

shared_mem_process_stats* s_pProcessStatsCentral = NULL;

Popup popupNoModel("No vehicle defined or linked to!", 0.22, 0.45, 5);
Popup popupStartup("System starting. Please wait.", 0.05, 0.16, 0);

MenuConfirmationHDMI* s_pMenuConfirmHDMI = NULL;
MenuConfirmationImport* s_pMenuConfirmationImport = NULL;

// Exported to end user
extern u32 s_uRenderEngineUIFontIdSmall;
extern u32 s_uRenderEngineUIFontIdRegular;
extern u32 s_uRenderEngineUIFontIdBig;

Popup* ruby_get_startup_popup()
{
   return &popupStartup;
}

void load_resources()
{
   Preferences* p = get_Preferences();

   ruby_reload_menu_font();
   ruby_reload_osd_fonts();

   if ( render_engine_is_raw() )
   {
      if ( access("res/custom_bg.png", R_OK) != -1 )
      {
         s_idBgImage = g_pRenderEngine->loadImage("res/custom_bg.png");
         s_idBgImageMenu = g_pRenderEngine->loadImage("res/custom_bg.png");
      }
      else
      {
         s_idBgImage = g_pRenderEngine->loadImage("res/ruby_bg2.png");
         s_idBgImageMenu = g_pRenderEngine->loadImage("res/ruby_bg3.png");
      }
   }
   else
   {
      if ( access("res/custom_bg.png", R_OK) != -1 )
      {
         s_idBgImage = g_pRenderEngine->loadImage("res/custom_bg.png");
         s_idBgImageMenu = g_pRenderEngine->loadImage("res/custom_bg.png");
      }
      else
      {
         s_idBgImage = g_pRenderEngine->loadImage("res/ruby_bg2.jpg");
         s_idBgImageMenu = g_pRenderEngine->loadImage("res/ruby_bg3.jpg");
      }
   }

   osd_load_resources();
}

u32 load_font(float hFont, const char* szName)
{
   char szFileName[256];
   szFileName[0] = 0;
   
   int nSize = hFont/2;
   nSize *= 2;
   if ( nSize < 14 )
      nSize = 14;
   if ( nSize > 56 )
      nSize = 56;
   snprintf(szFileName, sizeof(szFileName), "res/font_%s_%d.dsc", szName, nSize );
   if ( access( szFileName, R_OK ) == -1 )
   {
      log_softerror_and_alarm("Can't find font: [%s]. Using a default font instead.", szFileName );
      snprintf(szFileName, sizeof(szFileName), "res/font_rawobold_%d.dsc", nSize );
      if ( access( szFileName, R_OK ) == -1 )
      {
         log_softerror_and_alarm("Can't find font: [%s]. Using a default size instead.", szFileName );
         nSize = 56;
         snprintf(szFileName, sizeof(szFileName), "res/font_rawobold_%d.dsc", nSize );
      }
   }
   log_line("Loading font: %s (%s)", szName, szFileName);
   return g_pRenderEngine->loadFont(szFileName);
}

void ruby_reload_menu_font()
{
   Preferences* p = get_Preferences();

   u32 tmpFont1 = g_idFontMenu;
   u32 tmpFont2 = g_idFontMenuLarge;
   u32 tmpFont3 = g_idFontMenuSmall;

   //g_pRenderEngine->freeFont(g_idFontMenu);
   //g_pRenderEngine->freeFont(g_idFontMenuSmall);
   //g_pRenderEngine->freeFont(g_idFontMenuLarge);

   float hScreen = hdmi_get_current_resolution_height();   
   log_line("(Re)Loading Menu fonts for screen height: %d px", (int)hScreen);

   hScreen *= menu_getScaleMenus();

   float hFont = hScreen*(16.0/720.0);
   g_idFontMenu = load_font(hFont, "raw_bold");
   if ( 0 == g_idFontMenu )
   {
      g_idFontMenu = tmpFont1;
      log_softerror_and_alarm("Failed to load main menu font. Restoring old one.");
   }
   else
      g_pRenderEngine->freeFont(tmpFont1);

   float hFontLarge = hFont*1.3;
   g_idFontMenuLarge = load_font(hFontLarge, "raw_bold");
   if ( 0 == g_idFontMenuLarge )
   {
      g_idFontMenuLarge = tmpFont2;
      log_softerror_and_alarm("Failed to load main menu font large. Restoring old one.");
   }
   else
      g_pRenderEngine->freeFont(tmpFont2);

   float hFontSmall = hFont*0.9;
   g_idFontMenuSmall = load_font(hFontSmall, "raw_bold");
   if ( 0 == g_idFontMenuSmall )
   {
      g_idFontMenuSmall = tmpFont3;
      log_softerror_and_alarm("Failed to load main menu font small. Restoring old one.");
   }
   else
      g_pRenderEngine->freeFont(tmpFont3);

   for( int i=0; i<popups_get_count(); i++ )
   {
      Popup* p = popups_get_at(i);
      if ( p && ( p->getFontId() == tmpFont1) )
         p->setFont(g_idFontMenu);
      if ( p && ( p->getFontId() == tmpFont2 ) )
         p->setFont(g_idFontMenuLarge);
      if ( p && ( p->getFontId() == tmpFont3 ) )
         p->setFont(g_idFontMenuSmall);
   }

   for( int i=0; i<popups_get_topmost_count(); i++ )
   {
      Popup* p = popups_get_topmost_at(i);
      if ( p && ( p->getFontId() == tmpFont1) )
         p->setFont(g_idFontMenu);
      if ( p && ( p->getFontId() == tmpFont2 ) )
         p->setFont(g_idFontMenuLarge);
      if ( p && ( p->getFontId() == tmpFont3 ) )
         p->setFont(g_idFontMenuSmall);
   }

   log_line("(Re)Loaded Menu fonts for screen height: %d px. Complete.", (int)hScreen);
}

void ruby_reload_osd_fonts()
{
   Preferences* p = get_Preferences();

   u32 tmp[7];

   tmp[0] = g_idFontOSD;
   tmp[1] = g_idFontOSDBig;
   tmp[2] = g_idFontOSDSmall;
   tmp[3] = g_idFontOSDWarnings;
   tmp[4] = g_idFontStats;
   tmp[5] = g_idFontStatsSmall;
   tmp[6] = g_idFontOSDExtraSmall;

   //g_pRenderEngine->freeFont(g_idFontOSD);
   //g_pRenderEngine->freeFont(g_idFontOSDBig);
   //g_pRenderEngine->freeFont(g_idFontOSDSmall);
   //g_pRenderEngine->freeFont(g_idFontOSDWarnings);
   //g_pRenderEngine->freeFont(g_idFontStats);
   //g_pRenderEngine->freeFont(g_idFontStatsSmall);

   float hScreen = hdmi_get_current_resolution_height();
   log_line("(Re)Loading OSD fonts for screen height: %d px", (int)hScreen);

   char szFont[32];
   strcpy(szFont, "raw_bold");
   if ( p->iOSDFont == 0 )
      strcpy(szFont, "raw_bold");
   if ( p->iOSDFont == 1 )
      strcpy(szFont, "rawobold");
   if ( p->iOSDFont == 2 )
      strcpy(szFont, "ariobold");
   if ( p->iOSDFont == 3 )
      strcpy(szFont, "bt_bold");


   float hFont = hScreen*(16.0/720)*1.4;
   g_idFontOSDWarnings = load_font(hFont, szFont);
   if ( 0 == g_idFontOSDWarnings )
   {
      g_idFontOSDWarnings = tmp[3];
      log_softerror_and_alarm("Failed to load OSDWarnings font. Restoring old one.");
   }
   else
      g_pRenderEngine->freeFont(tmp[3]);

   hFont = hScreen*(16.0/720)*osd_getScaleOSD();
   g_idFontOSD = load_font(hFont, szFont);
   if ( 0 == g_idFontOSD )
   {
      g_idFontOSD = tmp[0];
      log_softerror_and_alarm("Failed to load OSD font. Restoring old one.");
   }
   else
      g_pRenderEngine->freeFont(tmp[0]);

   float hFontBig = hFont*1.4;
   g_idFontOSDBig = load_font(hFontBig, szFont);
   if ( 0 == g_idFontOSDBig )
   {
      g_idFontOSDBig = tmp[1];
      log_softerror_and_alarm("Failed to load OSDBig font. Restoring old one.");
   }
   else
      g_pRenderEngine->freeFont(tmp[1]);

   float hFontSmall = hFont*0.74;
   g_idFontOSDSmall = load_font(hFontSmall, szFont);
   if ( 0 == g_idFontOSDSmall )
   {
      g_idFontOSDSmall = tmp[2];
      log_softerror_and_alarm("Failed to load OSDSmall font. Restoring old one.");
   }
   else
      g_pRenderEngine->freeFont(tmp[2]);

   float hFontExtraSmall = hFont*0.6;
   g_idFontOSDExtraSmall = load_font(hFontExtraSmall, szFont);
   if ( 0 == g_idFontOSDExtraSmall )
   {
      g_idFontOSDExtraSmall = tmp[6];
      log_softerror_and_alarm("Failed to load OSDExtraSmall font. Restoring old one.");
   }
   else
      g_pRenderEngine->freeFont(tmp[6]);

   hFont = hScreen*(16.0/720)*osd_getScaleOSDStats();
   g_idFontStats = load_font(hFont, szFont);
   if ( 0 == g_idFontStats )
   {
      g_idFontStats = tmp[4];
      log_softerror_and_alarm("Failed to load OSDStats font. Restoring old one.");
   }
   else
      g_pRenderEngine->freeFont(tmp[4]);

   hFontSmall = hFont*0.74;
   g_idFontStatsSmall = load_font(hFontSmall, szFont);
   if ( 0 == g_idFontStatsSmall )
   {
      g_idFontStatsSmall = tmp[5];
      log_softerror_and_alarm("Failed to load OSDStatsSmall font. Restoring old one.");
   }
   else
      g_pRenderEngine->freeFont(tmp[5]);


   s_uRenderEngineUIFontIdRegular = g_idFontOSD;
   s_uRenderEngineUIFontIdSmall = g_idFontOSDSmall;
   s_uRenderEngineUIFontIdBig = g_idFontOSDBig;

   for( int i=0; i<popups_get_count(); i++ )
   {
      Popup* p = popups_get_at(i);
      if ( p && ( p->getFontId() == tmp[0]) )
         p->setFont(g_idFontOSD);
      if ( p && ( p->getFontId() == tmp[1] ) )
         p->setFont(g_idFontOSDBig);
      if ( p && ( p->getFontId() == tmp[2] ) )
         p->setFont(g_idFontOSDSmall);
      if ( p && ( p->getFontId() == tmp[3] ) )
         p->setFont(g_idFontOSDWarnings);
      if ( p && ( p->getFontId() == tmp[4] ) )
         p->setFont(g_idFontStats);
      if ( p && ( p->getFontId() == tmp[5] ) )
         p->setFont(g_idFontStatsSmall);
      if ( p && ( p->getFontId() == tmp[6] ) )
         p->setFont(g_idFontOSDExtraSmall);
   }

   for( int i=0; i<popups_get_topmost_count(); i++ )
   {
      Popup* p = popups_get_topmost_at(i);
      if ( p && ( p->getFontId() == tmp[0] ) )
         p->setFont(g_idFontOSD);
      if ( p && ( p->getFontId() == tmp[1] ) )
         p->setFont(g_idFontOSDBig);
      if ( p && ( p->getFontId() == tmp[2] ) )
         p->setFont(g_idFontOSDSmall);
      if ( p && ( p->getFontId() == tmp[3] ) )
         p->setFont(g_idFontOSDWarnings);
      if ( p && ( p->getFontId() == tmp[4] ) )
         p->setFont(g_idFontStats);
      if ( p && ( p->getFontId() == tmp[5] ) )
         p->setFont(g_idFontStatsSmall);
      if ( p && ( p->getFontId() == tmp[6] ) )
         p->setFont(g_idFontOSDExtraSmall);
   }

   log_line("(Re)Loaded OSD fonts for screen height: %d px. Complete.", (int)hScreen);   
}


void draw_background()
{
   if ( isMenuOn() )
      g_pRenderEngine->drawImage(0, 0, 1,1, s_idBgImageMenu);
   else
      g_pRenderEngine->drawImage(0, 0, 1,1, s_idBgImage);

   double cc[4] = { 80,30,40,0.88 };

   g_pRenderEngine->setColors(cc);
   float width_text = g_pRenderEngine->textWidth(0.02, g_idFontMenu, SYSTEM_NAME);
   char szBuff[256];
   getSystemVersionString(szBuff, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
   g_pRenderEngine->drawText(0.91, 0.92, 0.02, g_idFontMenu, SYSTEM_NAME);
   g_pRenderEngine->drawText(0.915+width_text, 0.92, 0.02, g_idFontMenu, szBuff);

   if ( NULL != g_pCurrentModel )
      return;

   double c[4] = {0,0,0,1};
   g_pRenderEngine->setGlobalAlfa(1.0);
   g_pRenderEngine->setColors(c);

   snprintf(szBuff, sizeof(szBuff), "Welcome to %s", SYSTEM_NAME);

   g_pRenderEngine->drawText(0.42, 0.2, 0.02*1.4, g_idFontMenuLarge, szBuff);
   g_pRenderEngine->drawText(0.42, 0.24, 0.02, g_idFontMenuLarge, "Digital FPV System");

   if ( s_StartSequence == START_SEQ_COMPLETED )
   if ( NULL == g_pCurrentModel )
   {
      if ( 0 == getModelsCount() )
      {
         g_pRenderEngine->drawText(0.32, 0.3, 0.02*1.2, g_idFontMenu, "Info: No vehicle defined!");
         g_pRenderEngine->drawText(0.32, 0.34, 0.02, g_idFontMenu, "You have no vehicles linked to this controller.");
         g_pRenderEngine->drawText(0.32, 0.37, 0.02, g_idFontMenu, "Please press [Menu] and then select [Search] to search for a vehicle to connect to.");
      }
      else
      {
         g_pRenderEngine->drawText(0.32, 0.3, 0.02*1.2, g_idFontMenu, "Info: No vehicle selected!");
         g_pRenderEngine->drawText(0.32, 0.34, 0.02, g_idFontMenu, "You have no vehicle selected as active.");
         g_pRenderEngine->drawText(0.32, 0.37, 0.02, g_idFontMenu, "Please press [Menu] and then select [My Vehicles] to select the vehicle to connect to.");
      }
   }
}

void render_graph_bg()
{
   float marginX = 10.0/g_pRenderEngine->getScreenWidth();
   float marginTop = 80.0/g_pRenderEngine->getScreenHeight();
   float marginBottom = 80.0/g_pRenderEngine->getScreenHeight();
   float paddingLeft = 60.0/g_pRenderEngine->getScreenWidth();
   float paddingRight = 20.0/g_pRenderEngine->getScreenWidth();
   float paddingTop = 30.0/g_pRenderEngine->getScreenHeight();
   float paddingBottom = 60.0/g_pRenderEngine->getScreenHeight();
   float width = 1.0-2.0*marginX;
   float height = 1.0-marginTop-marginBottom;

   float startX = marginX + paddingLeft;
   float endX = marginX + width - paddingRight;
   float startY = marginTop + paddingTop;
   float endY = marginTop + height - paddingBottom;
   double c[4] = {0,0,0,0.9};
   g_pRenderEngine->setFill(c[0],c[1],c[2],c[3]);
   g_pRenderEngine->setStroke(c[0],c[1],c[2],c[3]);
   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->drawRect(marginX, marginTop, width, height);
      
   double cc[4] = { 80,30,40,0.88 };
   char szBuff[256];

   g_pRenderEngine->setColors(cc);
   float height_text = g_pRenderEngine->textHeight(0.0, g_idFontMenu);

   g_pRenderEngine->setFill(255,255,255,1);
   g_pRenderEngine->setStroke(255,255,255,1);
   g_pRenderEngine->setStrokeSize(1);

   g_pRenderEngine->drawLine(startX-2.0*g_pRenderEngine->getPixelWidth(), endY-16.0*g_pRenderEngine->getPixelHeight(), endX, endY-16.0*g_pRenderEngine->getPixelHeight());
   g_pRenderEngine->drawLine(startX-2.0*g_pRenderEngine->getPixelWidth(), endY-16.0*g_pRenderEngine->getPixelHeight(), startX-2.0*g_pRenderEngine->getPixelWidth(), startY);

   g_pRenderEngine->drawText(startX+(endX-startX)/2.0-50.0/g_pRenderEngine->getScreenWidth(), endY-height_text*4.2-24.0/g_pRenderEngine->getScreenHeight(), height_text, g_idFontMenu, "ms");

   int milisec = 100;
   if ( s_PacketsScopeGraphZoomLevel == 1 )
      milisec = 50;
   if ( s_PacketsScopeGraphZoomLevel == 2 )
      milisec = 25;

   for( int i=0; i<=10; i++ )
   {
      snprintf(szBuff, sizeof(szBuff), "%d", i*milisec);
      
      float width_text = g_pRenderEngine->textWidth(height_text, g_idFontMenu, szBuff);   
      int x = startX + (endX-startX)*i/10.0;
      g_pRenderEngine->drawText(x-width_text*0.5, endY-24.0*g_pRenderEngine->getPixelHeight() - height_text*2.6, height_text, g_idFontMenu, szBuff);
      g_pRenderEngine->drawLine(x, endY-16.0*g_pRenderEngine->getPixelHeight(), x, endY-32.0*g_pRenderEngine->getPixelHeight());
      if ( i != 10 )
         g_pRenderEngine->drawLine(x+(endX-startX)/20.0,endY-16.0*g_pRenderEngine->getPixelWidth(), x+(endX-startX)/20.0, endY-26.0*g_pRenderEngine->getPixelWidth());
   }
   

   int slices = 8;
   if ( s_PacketsScopeGraphZoomLevel == 1 )
      slices = 16;
   if ( s_PacketsScopeGraphZoomLevel == 2 )
      slices = 32;

   for( int i=0; i<=slices+2; i++ )
   {
      snprintf(szBuff, sizeof(szBuff), "%d", i-2);
      if ( i < 2 )
         snprintf(szBuff, sizeof(szBuff), "%d", 2-i);
      float y = startY + (endY-startY )*i/(slices+2);
      g_pRenderEngine->drawTextLeft(startX-22.0*g_pRenderEngine->getPixelWidth(), y-height_text*0.4, height_text, g_idFontMenu, szBuff);
      g_pRenderEngine->drawLine(startX-10.0*g_pRenderEngine->getPixelWidth(), y, startX, y);
   }
}


void render_router_pachets_history()
{
   float marginX = 10.0/g_pRenderEngine->getScreenWidth();
   float marginTop = 80.0/g_pRenderEngine->getScreenHeight();
   float marginBottom = 80.0/g_pRenderEngine->getScreenHeight();
   float paddingLeft = 60.0/g_pRenderEngine->getScreenWidth();
   float paddingRight = 20.0/g_pRenderEngine->getScreenWidth();
   float paddingTop = 30.0/g_pRenderEngine->getScreenHeight();
   float paddingBottom = 60.0/g_pRenderEngine->getScreenHeight();
   float width = 1.0-2.0*marginX;
   float height = 1.0-marginTop-marginBottom;
   float contentHeight = height - paddingTop - paddingBottom;

   render_graph_bg();
   if ( NULL == g_pDebugSMRPST )
      return;

   ControllerSettings* pCS = get_ControllerSettings();

   int totalCountRC = 0;
   int totalCountRCIn = 0;
   int totalCountPing = 0;
   int totalCountTelemetry = 0;

   int slicesH = 1000;
   int slicesVert = 8;
   int bands = 1;
   float bandHeight = contentHeight;
   if ( s_PacketsScopeGraphZoomLevel == 1 )
   {
      slicesH = 500;
      slicesVert = 16;
      bands = 2;
      bandHeight = contentHeight/2.0;
   }
   if ( s_PacketsScopeGraphZoomLevel == 2 )
   {
      slicesH = 250;
      slicesVert = 32;
      bands = 4;
      bandHeight = contentHeight/4;
   }

   float sliceWidth = ((float)(width-paddingLeft-paddingRight))/(float)slicesH;
   float sliceHeight = (height - paddingTop - paddingBottom)/(slicesVert+2);

   int miliSec = g_pDebugSMRPST->lastMilisecond;
   float lineHeight = bandHeight-2*sliceHeight;

   int sliceStart = 0;

   int* pRates = getDataRates();

   double colorVideo[4] = {180,180,180,0.4};
   double colorRetr1[4] = {0,0,255,0.8};
   double colorRetr2[4] = {0,255,0,0.8};
   double colorPing[4] = {250,0,0,0.9};
   double colorTelem[4] = {250,255,0,0.7};
   double colorRC[4] = {240,0,160,0.9};

   float height_text = g_pRenderEngine->textHeight(0.0, g_idFontMenu);

   float yLeg = marginBottom+paddingBottom+contentHeight;
   float xLeg = marginX+paddingLeft + 150.0/g_pRenderEngine->getScreenWidth();

   g_pRenderEngine->setStroke(255,255,255,1);
   g_pRenderEngine->setStrokeSize(0.2);
   g_pRenderEngine->setFill(colorPing[0], colorPing[1], colorPing[2], colorPing[3]);
   g_pRenderEngine->drawRect(xLeg, yLeg, 20.0/g_pRenderEngine->getScreenWidth(), 10.0/g_pRenderEngine->getScreenHeight() );
   g_pRenderEngine->drawText(xLeg+24.0/g_pRenderEngine->getScreenWidth(), yLeg, height_text, g_idFontMenu, "ping");
   xLeg += 70.0/g_pRenderEngine->getScreenWidth();

   g_pRenderEngine->setFill(colorTelem[0], colorTelem[1], colorTelem[2], colorTelem[3]);
   g_pRenderEngine->drawRect(xLeg, yLeg, 20.0/g_pRenderEngine->getScreenWidth(), 10.0/g_pRenderEngine->getScreenHeight() );
   g_pRenderEngine->drawText(xLeg+24.0/g_pRenderEngine->getScreenWidth(), yLeg, height_text, g_idFontMenu, "telem");
   xLeg += 70.0/g_pRenderEngine->getScreenWidth();

   g_pRenderEngine->setFill(colorRC[0], colorRC[1], colorRC[2], colorRC[3]);
   g_pRenderEngine->drawRect(xLeg, yLeg, 20.0/g_pRenderEngine->getScreenWidth(), 10.0/g_pRenderEngine->getScreenHeight() );
   g_pRenderEngine->drawText(xLeg+24.0/g_pRenderEngine->getScreenWidth(), yLeg, height_text, g_idFontMenu, "rc");
   xLeg += 70.0/g_pRenderEngine->getScreenWidth();

   for( int band=0; band<bands; band++ )
   {
      float xPos = marginX + paddingLeft;
      float yPos = marginBottom + paddingBottom + (bands-band-1)*bandHeight + 2*sliceHeight;

      g_pRenderEngine->setStroke(200,200,200,0.6);
      g_pRenderEngine->setFill(200,200,200,0.6);
      g_pRenderEngine->setStrokeSize(1);

      g_pRenderEngine->drawLine(xPos, yPos-1.0*g_pRenderEngine->getPixelHeight(), xPos+(width-paddingLeft-paddingRight), yPos-1.0*g_pRenderEngine->getPixelHeight());
      
      g_pRenderEngine->setStrokeSize(0);

      float y = yPos;
      for( int i=sliceStart; i<slicesH+sliceStart; i++ )
      {
         u16 val = g_pDebugSMRPST->txHistoryPackets[i];
         u16 countRe = val & 0x07;
         u16 countPing = (val>>6) & 0x03;
         u16 countCom = (val>>3) & 0x07;
         u16 countRc = (val>>8) & 0x07;
         u16 cRateTx = (val>>13) & 0x07;

         totalCountRC += countRc;
         totalCountPing += countPing;

         y = yPos;
         if ( countRe > 0 )
         {
            float h = countRe*sliceHeight*0.8;
            g_pRenderEngine->setFill(colorRetr1[0], colorRetr1[1], colorRetr1[2], colorRetr1[3]);
            g_pRenderEngine->setStroke(colorRetr1[0], colorRetr1[1], colorRetr1[2], 0.4);
            g_pRenderEngine->setStrokeSize(0.2);

            //if ( cRateTx > 0 && pRates[cRateTx] != pCS->iDataRate )
            //{
            //   g_pRenderEngine->setFill(colorRetr2[0], colorRetr2[1], colorRetr2[2], colorRetr2[3]);
            //   g_pRenderEngine->setStroke(colorRetr2[0], colorRetr2[1], colorRetr2[2], 0.4);
            //}
            g_pRenderEngine->drawRect(xPos, y-h, sliceWidth, h);
            g_pRenderEngine->setStrokeSize(0);

            y -= h;
         }
         if ( countPing > 0  )
         {
            float h = countPing*sliceHeight*0.8;
            g_pRenderEngine->setFill(colorPing[0], colorPing[1], colorPing[2], colorPing[3]);
            g_pRenderEngine->setStroke(colorPing[0], colorPing[1], colorPing[2], 0.4);
            g_pRenderEngine->setStrokeSize(0.2);
            g_pRenderEngine->drawRect(xPos, y-h, sliceWidth, h);
            y -= h;
         }

         if ( countRc > 0  )
         {
            float h = countRc*sliceHeight*0.8;
            g_pRenderEngine->setFill(colorRC[0], colorRC[1], colorRC[2], colorRC[3]);
            g_pRenderEngine->setStroke(colorRC[0], colorRC[1], colorRC[2], 0.6);
            g_pRenderEngine->setStrokeSize(0.2);
            g_pRenderEngine->drawRect(xPos, y-h, sliceWidth, h);
            y -= h;
         }

      y = yPos;
      val = g_pDebugSMRPST->rxHistoryPackets[i];
      u8 cVideo = val & 0x07;
      u8 cRetr = (val>>3) & 0x07;
      u8 cPing  = (val>>6) & 0x01;
      u8 cTelem = (val>>7) & 0x03;
      u8 cRC = (val>>9) & 0x03;
      u8 cRate = (val>>13) & 0x07;
      totalCountTelemetry += cTelem;
      totalCountRCIn += cRC;
      if ( cVideo > 0 )
      {
         float h = sliceHeight * cVideo;
         g_pRenderEngine->setFill(colorVideo[0], colorVideo[1], colorVideo[2], colorVideo[3]);
         g_pRenderEngine->setStroke(colorVideo[0], colorVideo[1], colorVideo[2], 0.4);
         g_pRenderEngine->setStrokeSize(0.0);
         g_pRenderEngine->drawRect(xPos, y, sliceWidth, h);
         g_pRenderEngine->setStrokeSize(0);
         y += h;
      }
      if ( cRetr > 0 )
      {
         float h = sliceHeight * cRetr;
         g_pRenderEngine->setFill(colorRetr1[0], colorRetr1[1], colorRetr1[2], colorRetr1[3]);
         g_pRenderEngine->setStroke(colorRetr1[0], colorRetr1[1], colorRetr1[2], 0.4);
         g_pRenderEngine->setStrokeSize(0.2);
         if ( NULL != g_pCurrentModel && cRate > 0 && pRates[cRate] != g_pCurrentModel->radioInterfacesParams.interface_datarates[0][0] )
         {
            g_pRenderEngine->setFill(colorRetr2[0], colorRetr2[1], colorRetr2[2], colorRetr2[3]);
            g_pRenderEngine->setStroke(colorRetr2[0], colorRetr2[1], colorRetr2[2], 0.4);
         }
         g_pRenderEngine->drawRect(xPos, y, sliceWidth, h);
         g_pRenderEngine->setStrokeSize(0);
         y += h;
      }
      if ( cRC > 0 )
      {
         float h = sliceHeight * cRC;
         g_pRenderEngine->setFill(colorRC[0], colorRC[1], colorRC[2], colorRC[3]);
         g_pRenderEngine->drawRect(xPos, y, sliceWidth, h);
         y += h;
      }
      if ( cTelem > 0 )
      {
         float h = sliceHeight * cTelem;
         g_pRenderEngine->setFill(colorTelem[0], colorTelem[1], colorTelem[2], colorTelem[3]);
         g_pRenderEngine->drawRect(xPos, y, sliceWidth, h);
         y += h;
      }
      if ( cPing > 0 )
      {
         float h = sliceHeight * cPing;
         g_pRenderEngine->setFill(colorPing[0], colorPing[1], colorPing[2], colorPing[3]);
         g_pRenderEngine->drawRect(xPos, y, sliceWidth, h);
         y += h;
      }

      xPos += sliceWidth;
      }

      if ( miliSec >= sliceStart && miliSec < sliceStart+slicesH )
      {
         g_pRenderEngine->setFill(150,150,150, 0.7);
         g_pRenderEngine->setStroke(150,150,150, 0.7);
         g_pRenderEngine->setStrokeSize(1);
         g_pRenderEngine->drawLine(marginX+paddingLeft+width*(miliSec-sliceStart)/slicesH, yPos, marginX+paddingLeft+width*(miliSec-sliceStart)/slicesH, yPos + lineHeight);
      }
      sliceStart += slicesH;
   }

   g_pRenderEngine->setFill(255,255,255,1);
   g_pRenderEngine->setStroke(255,255,255,1);
   g_pRenderEngine->setStrokeSize(0.2);
   height_text = g_pRenderEngine->textHeight(0.0, g_idFontMenu);

   char szBuff[128];
   snprintf(szBuff, sizeof(szBuff), "Telemetry: %d/sec", totalCountTelemetry);
   g_pRenderEngine->drawText(marginX+paddingLeft+5.0/g_pRenderEngine->getScreenWidth(), marginBottom+height-paddingTop, height_text, g_idFontMenu, szBuff);
   snprintf(szBuff, sizeof(szBuff), "RC (Out/In): %d/%d/sec", totalCountRC, totalCountRCIn);
   g_pRenderEngine->drawText(marginX+paddingLeft+5.0/g_pRenderEngine->getScreenWidth(), marginBottom+height-paddingTop-height_text*1.3, height_text, g_idFontMenu, szBuff);
   snprintf(szBuff, sizeof(szBuff), "Ping: %d/sec", totalCountPing);
   g_pRenderEngine->drawText(marginX+paddingLeft+5.0/g_pRenderEngine->getScreenWidth(), marginBottom+height-paddingTop-height_text*2.6, height_text, g_idFontMenu, szBuff);
}


void render_all(u32 timeNow, bool bForceBackground, bool bDoInputLoop)
{
   ControllerSettings* pCS = get_ControllerSettings();

   if ( pCS->iFreezeOSD && s_bFreezeOSD )
      return;

   g_pRenderEngine->startFrame();

   if ( g_bVideoPlaying )
   {
      char szBuff[1024];
      g_pRenderEngine->setFill(255,255,255,1);
      g_pRenderEngine->setStroke(0,0,0,1);
      g_pRenderEngine->setStrokeSize(1);
      u32 t = timeNow;
      t = t - g_uVideoPlayingStartTime;
      if ( t/1000 > g_uVideoPlayingLengthSec+1 )
         snprintf(szBuff, sizeof(szBuff), "Finished.");
      else if ( (t/500)%2 )
         snprintf(szBuff, sizeof(szBuff), "Playing %02d:%02d", ((t/1000)/60), (t/1000)%60);
      else
         snprintf(szBuff, sizeof(szBuff), "Playing %02d %02d", ((t/1000)/60), (t/1000)%60);

      float text_scale = 0.017;
      g_pRenderEngine->drawText(0.03, 0.05, text_scale, g_idFontMenu, szBuff);      
      snprintf(szBuff, sizeof(szBuff), "Press [Menu] or [Back] button to close");
      text_scale = 0.012;
      g_pRenderEngine->drawText(0.18, 0.042, text_scale, g_idFontMenu, szBuff);      
      g_pRenderEngine->endFrame();
      return;
   }

   bool showBg = true;

   if ( ! g_bSearching )
   if ( ! pairing_is_connected_to_wrong_model() )
   if ( g_bIsRouterReady )
   if ( pairing_hasReceivedVideoStreamData() )
      showBg = false;

   if ( g_bSearching && (!g_bSearchFoundVehicle) )
      showBg = true;

   Preferences* p = get_Preferences();

   if ( showBg || bForceBackground )
   {
      if ( (! bForceBackground) && pairing_isStarted() && pairing_wasReceiving() )
      {
         g_pRenderEngine->setGlobalAlfa(1.0);

         double c1[4] = {0,0,0,1};
         g_pRenderEngine->setColors(c1);
         g_pRenderEngine->drawRect(0, 0, 1,1 );

         double c[4] = {255,255,255,1};
         g_pRenderEngine->setColors(c);

         g_pRenderEngine->drawText(0.44, 0.4, 0.02, g_idFontOSDBig, "No Video");
      }
      else
         draw_background();
   }

   float fScreenAspect = (float)(g_pRenderEngine->getScreenWidth())/(float)(g_pRenderEngine->getScreenHeight());
   if ( (!g_bSearching) || g_bSearchFoundVehicle )
   if ( (NULL != g_pCurrentModel) && (!bForceBackground) )
   {
      double c[4] = {0,0,0,1};
      g_pRenderEngine->setGlobalAlfa(1.0);
      g_pRenderEngine->setColors(c);

      float fVideoAspect = (float)(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width) / (float)(g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height);
      if ( fVideoAspect > fScreenAspect+0.01 )
      {
         int h = 1 + 0.5*(g_pRenderEngine->getScreenHeight() - g_pRenderEngine->getScreenWidth()/fVideoAspect);
         if ( h > 1 )
         {
            float fHBand = (float)h/(float)g_pRenderEngine->getScreenHeight();
            fHBand += g_pRenderEngine->getPixelHeight();
            g_pRenderEngine->drawRect(0, 0, 1.0, fHBand);
            g_pRenderEngine->drawRect(0,1.0-fHBand, 1.0, fHBand);
         }
      }
      else if ( fVideoAspect < fScreenAspect-0.01 )
      {
         int w = 1 + 0.5*(g_pRenderEngine->getScreenWidth() - g_pRenderEngine->getScreenHeight()*fVideoAspect);
         if ( w > 1 )
         {
            float fWBand = (float)w/(float)g_pRenderEngine->getScreenWidth();
            fWBand += g_pRenderEngine->getPixelWidth();
            g_pRenderEngine->drawRect(0, 0, fWBand, 1.0);
            g_pRenderEngine->drawRect(1.0-fWBand, 0, fWBand, 1.0);
         }
      }
   }

   if ( (!g_bSearching) || g_bSearchFoundVehicle )
   if ( ! bForceBackground )
   if ( s_StartSequence == START_SEQ_COMPLETED || s_StartSequence == START_SEQ_FAILED )
   {
      if ( g_bIsRouterReady )
      if ( pairing_isStarted() && (pairing_isReceiving() || pairing_wasReceiving()) )
      {
         u32 t = get_current_timestamp_micros();
         osd_render_all();
         t = get_current_timestamp_micros() - t;
         if ( t < 300000 )
            s_iMicroTimeOSDRender = s_iMicroTimeOSDRender*0.8 + t*0.2;
      }
      if ( g_bIsRouterReady )
         alarms_render();
   }

   if ( NULL != pCS && (0 != pCS->iDeveloperMode) )
   if ( ! bForceBackground )
   if ( (! g_bToglleAllOSDOff) && (!g_bToglleStatsOff) )
   if ( ! p->iDebugShowFullRXStats )
   if ( g_bIsRouterReady )
   {
      char szBuff[64];
      float yPos = osd_getMarginY() + osd_getBarHeight() + osd_getSecondBarHeight() + 0.014*osd_getScaleOSD();
      float xPos = osd_getMarginX() + 0.01*osd_getScaleOSD();
      if ( NULL != g_pCurrentModel && g_iCurrentOSDVehicleLayout >= 0 && g_iCurrentOSDVehicleLayout < MODEL_MAX_OSD_PROFILES )
      if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      {
         xPos = osd_getMarginX() + osd_getVerticalBarWidth() + 0.01*osd_getScaleOSD();
         yPos = osd_getMarginY() + 0.01*osd_getScaleOSD();
      }
   
      osd_set_colors();
      osd_show_value( xPos, yPos, "[D]", g_idFontOSD );

      if ( p->iShowCPULoad )
      {
         xPos += 0.02*osd_getScaleOSD();
         snprintf(szBuff, sizeof(szBuff), "UI FPS: %d", s_iRubyFPS);
         osd_show_value(xPos, yPos, szBuff, g_idFontOSD );

         xPos += 0.056*osd_getScaleOSD();
         snprintf(szBuff, sizeof(szBuff), "Menu: %.1f ms/frame", s_iMicroTimeMenuRender/1000.0);
         osd_show_value(xPos, yPos, szBuff, g_idFontOSD );

         xPos += 0.1*osd_getScaleOSD();
         snprintf(szBuff, sizeof(szBuff), "OSD: %.1f ms/frame", s_iMicroTimeOSDRender/1000.0);
         osd_show_value(xPos, yPos, szBuff, g_idFontOSD );

         xPos += 0.1*osd_getScaleOSD();
         snprintf(szBuff, sizeof(szBuff), "OSD: %d ms/sec", (int)(s_iMicroTimeOSDRender*s_iRubyFPS/1000.0));
         osd_show_value(xPos, yPos, szBuff, g_idFontOSD );
      }
   }

   u32 t = get_current_timestamp_micros();
   popups_render();

   menu_render();


   popups_render_topmost();

   t = get_current_timestamp_micros() - t;
   if ( t < 100000 )
      s_iMicroTimeMenuRender = s_iMicroTimeMenuRender*0.8 + t*0.2;
  
   if ( handle_commands_is_command_in_progress() )
      render_commands();

   s_iFPSCount++;
   if ( timeNow >= s_iFPSLastTimeCheck + 1000 )
   {
      s_iRubyFPS = s_iFPSCount;
      s_iFPSCount = 0;
      s_iFPSLastTimeCheck = timeNow;
      //printf("%u %d\n", s_iFPSLastTimeCheck, s_iRubyFPS);
      //log_line("%u %d\n", s_iFPSLastTimeCheck, s_iRubyFPS);
   }
   
   if ( g_bIsRouterPacketsHistoryGraphOn )
      render_router_pachets_history();

   if ( NULL != p && p->iOSDFlipVertical )
      g_pRenderEngine->rotate180();

   g_pRenderEngine->endFrame();
}


void compute_cpu_load(u32 timeNow)
{
   if ( timeNow < s_TimeLastCPUCompute + 500 )
      return;

   s_TimeLastCPUCompute = timeNow;

   FILE* fd = NULL;
   static unsigned long long valgcpu[4] = {0,0,0,0};
   unsigned long long tmp[4];
   unsigned long long total;
   int temp = 0;

   fd = fopen("/proc/stat", "r");
   fscanf(fd, "%*s %llu %llu %llu %llu", &tmp[0], &tmp[1], &tmp[2], &tmp[3] );
   fclose(fd);
   //printf("\n%llu, %llu, %llu, %llu", tmp[0], tmp[1], tmp[2], tmp[3] );
   
   if ( tmp[0] < valgcpu[0] || tmp[1] < valgcpu[1] || tmp[2] < valgcpu[2] || tmp[3] < valgcpu[3] )
         g_ControllerCPULoad = 0;
   else
   {
      total = (tmp[0] - valgcpu[0]) + (tmp[1] - valgcpu[1]) + (tmp[2] - valgcpu[2]);
      if ( (total + (tmp[3] - valgcpu[3])) != 0 )
         g_ControllerCPULoad = (total * 100) / (total + (tmp[3] - valgcpu[3]));
   }
      
   valgcpu[0] = tmp[0]; valgcpu[1] = tmp[1]; valgcpu[2] = tmp[2]; valgcpu[3] = tmp[3]; 
   //printf("\nCPU: %d %%\n", pDPVT->cpu_load);


   if ( timeNow < s_TimeLastCPUSpeed + 5000 )
      return;
   
   s_TimeLastCPUSpeed = timeNow;

   fd = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
   fscanf(fd, "%d", &temp);
   fclose(fd);
   g_ControllerTemp = temp/1000;

   g_ControllerCPUSpeed = hardware_get_cpu_speed();
}

void ruby_start_recording()
{
   if ( g_bVideoRecordingStarted )
      return;
   if ( (g_TimeNow < s_TimeLastRecordingStop + 1000) || g_bVideoProcessing )
   {
      Popup* p = new Popup("Please wait for the current video file processing to complete.", 0.1,0.7, 0.54, 5);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return;
   }

   char szBuff[1024];
   system("sudo mount -o remount,rw /"); 
   snprintf(szBuff, sizeof(szBuff), "mkdir -p %s",FOLDER_MEDIA);
   hw_execute_bash_command(szBuff, NULL );
   snprintf(szBuff, sizeof(szBuff), "chmod 777 %s",FOLDER_MEDIA);
   hw_execute_bash_command(szBuff, NULL );

   hw_execute_bash_command("mkdir -p tmp", NULL );
   hw_execute_bash_command("chmod 777 tmp", NULL );

   snprintf(szBuff, sizeof(szBuff), "rm -rf %s 2>/dev/null",TEMP_VIDEO_FILE_PROCESS_ERROR);
   hw_execute_bash_command(szBuff, NULL );

   if ( access( FOLDER_MEDIA, R_OK ) == -1 )
   {
      Popup* p = new Popup("There is an issue writing to the SD card.", 0.1,0.7, 0.54, 5);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      return;
   }

   g_uVideoRecordStartTime = get_current_timestamp_ms();
   if ( 1 == hw_execute_bash_command_raw("df -m /home/pi/ruby | grep root", szBuff) )
   {
      char szTemp[1024];
      long lb, lu, lf;
      sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lb, &lu, &lf);
      if ( lf < 200 )
      {
         snprintf(szTemp, sizeof(szTemp), "You don't have enough free space on the SD card to start recording (%d Mb free). Move your media files to USB memory stick.", (int)lf);
         warnings_add(szTemp, g_idIconCamera, get_Color_IconError(), 6);
         return;
      }
      if ( lf < 1000 )
      {
         snprintf(szTemp, sizeof(szTemp), "You are running low on storage space (%d Mb free). Move your media files to USB memory stick.", (int)lf);
         warnings_add(szTemp, g_idIconCamera, get_Color_IconWarning(), 6);
         log_line("Warning: Free storage space is only %d Mb. Video recording might stop", (int)lf);
      }
   }

   sem_t* s = sem_open(SEMAPHORE_START_VIDEO_RECORD, O_CREAT, S_IWUSR | S_IRUSR, 0);
   sem_post(s);
   sem_close(s); 

   Preferences* p = get_Preferences();
   if ( p->iRecordingLedAction == 1 )
      hardware_recording_led_set_on();
   if ( p->iRecordingLedAction == 2 )
      hardware_recording_led_set_blinking();
   g_bVideoRecordingStarted = true;
   notification_add_recording_start();
}

void ruby_stop_recording()
{
   s_TimeLastRecordingStop = g_TimeNow;

   if ( ! g_bVideoRecordingStarted )
      return;
   sem_t* s = sem_open(SEMAPHORE_STOP_VIDEO_RECORD, O_CREAT, S_IWUSR | S_IRUSR, 0);
   sem_post(s);
   sem_close(s); 
   g_bVideoRecordingStarted = false;

   hardware_recording_led_set_off();

   notification_add_recording_end();
   g_bVideoProcessing = true;
   link_watch_mark_started_video_processing();
   warnings_add("Processing video file...", g_idIconCamera, get_Color_IconNormal());
}

bool quickActionCheckVehicle(const char* szText)
{
   bool bHasVehicle = false;
   if ( pairing_isStarted() && pairing_isReceiving() && (! pairing_is_connected_to_wrong_model()) )
      bHasVehicle = true;

   if ( bHasVehicle )
      return true;

   char szBuff[256];
   if ( NULL == szText || 0 == szText[0] )
      strcpy(szBuff, "You must be connected to a vehicle to use this Quick Action button function");
   else
      snprintf(szBuff, sizeof(szBuff), "You must be connected to a vehicle to %s", szText);

   Popup* p = new Popup(szBuff, 0.1,0.7, 0.54, 4);
   p->setCentered();
   p->setIconId(g_idIconInfo, get_Color_IconWarning());
   popups_add_topmost(p);
   return false;  
}

void executeQuickActions()
{
   static u32 s_uTimeLastQuickActionPress = 0;
   static u32 s_uLastQuickActionSwitchVideoProfile = 0;

   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   if ( NULL == p )
      return;
   if ( g_bIsReinit )
      return;

   if ( isKeyQA1Pressed() )
      log_line("Pressed button QA1");
   if ( isKeyQA2Pressed() )
      log_line("Pressed button QA2");
   if ( isKeyQA3Pressed() )
      log_line("Pressed button QA3");

   if ( NULL != g_pCurrentModel )
   if ( pairing_isStarted() )
   if ( pCS->iDevSwitchVideoProfileUsingQAButton >= 0 && pCS->iDevSwitchVideoProfileUsingQAButton < 3 )
   if ( (isKeyQA1Pressed() && (pCS->iDevSwitchVideoProfileUsingQAButton==0)) ||
        (isKeyQA2Pressed() && (pCS->iDevSwitchVideoProfileUsingQAButton==1)) ||
        (isKeyQA3Pressed() && (pCS->iDevSwitchVideoProfileUsingQAButton==2)) )
   {
      s_uLastQuickActionSwitchVideoProfile++;
      if ( s_uLastQuickActionSwitchVideoProfile > 2 )
         s_uLastQuickActionSwitchVideoProfile = 0;

      if ( 0 == s_uLastQuickActionSwitchVideoProfile )
      {
         handle_commands_send_to_vehicle(COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_AUTO, 0, NULL, 0);
         warnings_add("Dev: Switch to Auto Video Link Quality.");
      }
      if ( 1 == s_uLastQuickActionSwitchVideoProfile )
      {
         handle_commands_send_to_vehicle(COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_MED, 0, NULL, 0);
         warnings_add("Dev: Switch to Med Video Link Quality.");
      }
      if ( 2 == s_uLastQuickActionSwitchVideoProfile )
      {
         handle_commands_send_to_vehicle(COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_LOW, 0, NULL, 0);
         warnings_add("Dev: Switch to Low Video Link Quality.");
      }
      return;
   }

   if ( NULL != g_pCurrentModel )
   if ( (isKeyQA1Pressed() && quickActionCycleOSD == p->iActionQuickButton1) || 
        (isKeyQA2Pressed() && quickActionCycleOSD == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionCycleOSD == p->iActionQuickButton3) )
   {
      //if ( ! quickActionCheckVehicle("cycle the OSD screens") )
      //   return;

      g_bHasVideoDecodeStatsSnapshot = false;

      if ( NULL == g_pCurrentModel )
         return;

      int curentLayout = g_pCurrentModel->osd_params.layout;
      int k=0; 
      while ( k < 10 )
      {
         k++;
         g_pCurrentModel->osd_params.layout++;
         if ( g_pCurrentModel->osd_params.layout >= osdLayoutLast )
            g_pCurrentModel->osd_params.layout = osdLayout1;
         if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_LAYOUT_ENABLED )
            break; 
      }

      if ( curentLayout == g_pCurrentModel->osd_params.layout )
      {
         Popup* p = new Popup("You have a single OSD screen enabled. Enable more to be able to switch them.", 0.1,0.7, 0.54, 4);
         p->setCentered();
         p->setIconId(g_idIconInfo, get_Color_IconWarning());
         popups_add_topmost(p);
         return;
      }

      u32 scale = g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.layout] & 0xFF;
      osd_setScaleOSD((int)scale);
      scale = (g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.layout]>>16) & 0x0F;
      osd_setScaleOSDStats((int)scale);

      if ( render_engine_is_raw() )
         ruby_reload_osd_fonts();

      saveAsCurrentModel(g_pCurrentModel);
      save_Preferences();

      if ( g_pCurrentModel->osd_params.layout == 0 )
         warnings_add("OSD Screen changed to Screen 1");
      if ( g_pCurrentModel->osd_params.layout == 1 )
         warnings_add("OSD Screen changed to Screen 2");
      if ( g_pCurrentModel->osd_params.layout == 2 )
         warnings_add("OSD Screen changed to Screen 3");
      if ( g_pCurrentModel->osd_params.layout == 3 )
         warnings_add("OSD Screen changed to Screen Lean");
      if ( g_pCurrentModel->osd_params.layout == 4 )
         warnings_add("OSD Screen changed to Screen Lean Extended");

      if ( g_pCurrentModel->is_spectator )
         return;

      //osd_parameters_t params;
      //memcpy(&params, &(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
      handle_commands_abandon_command();
      //handle_commands_send_to_vehicle(COMMAND_ID_SET_OSD_PARAMS, 0, (u8*)&params, sizeof(osd_parameters_t));
      handle_commands_send_single_oneway_command(1, COMMAND_ID_SET_OSD_CURRENT_LAYOUT, (u32)g_pCurrentModel->osd_params.layout, NULL, 0);
      g_iMustSendCurrentActiveOSDLayoutCounter = 10; // send it 10 times, every 200 ms
      g_TimeLastSentCurrentActiveOSDLayout = g_TimeNow;
      return;
   }

   if ( (isKeyQA1Pressed() && quickActionOSDSize == p->iActionQuickButton1) ||
        (isKeyQA2Pressed() && quickActionOSDSize == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionOSDSize == p->iActionQuickButton3) )
   {
      if ( ! quickActionCheckVehicle("change OSD size") )
         return;
      p->iScaleOSD++;
      if ( p->iScaleOSD > 3 )
         p->iScaleOSD = -1;
      save_Preferences();
      apply_Preferences();
      return;
   }

   if ( (isKeyQA1Pressed() && quickActionTakePicture == p->iActionQuickButton1) ||
        (isKeyQA2Pressed() && quickActionTakePicture == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionTakePicture == p->iActionQuickButton3) )
   {
      if ( get_current_timestamp_ms() < s_uTimeLastQuickActionPress + 500 )
         return;
      s_uTimeLastQuickActionPress = get_current_timestamp_ms();
      media_take_screenshot(p->iAddOSDOnScreenshots);
      return;
   }
         
   if ( (isKeyQA1Pressed() && quickActionToggleOSD == p->iActionQuickButton1) ||
        (isKeyQA2Pressed() && quickActionToggleOSD == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionToggleOSD == p->iActionQuickButton3) )
   {

      if ( ! quickActionCheckVehicle("toggle the OSD") )
         return;
      g_bToglleOSDOff = ! g_bToglleOSDOff;
      return;
   }

   if ( (isKeyQA1Pressed() && quickActionToggleStats == p->iActionQuickButton1) ||
        (isKeyQA2Pressed() && quickActionToggleStats == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionToggleStats == p->iActionQuickButton3) )
   {
      if ( ! quickActionCheckVehicle("toggle the statistics") )
         return;
      g_bToglleStatsOff = ! g_bToglleStatsOff;
      return;
   }
         
   if ( (isKeyQA1Pressed() && quickActionToggleAllOff == p->iActionQuickButton1) ||
        (isKeyQA2Pressed() && quickActionToggleAllOff == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionToggleAllOff == p->iActionQuickButton3) )
   {
      if ( ! quickActionCheckVehicle("toggle all info on/off") )
         return;
      g_bToglleAllOSDOff = ! g_bToglleAllOSDOff;
      return;
   }

   if ( NULL != g_pCurrentModel )
   if ( (isKeyQA1Pressed() && quickActionCameraProfileSwitch == p->iActionQuickButton1) ||
        (isKeyQA2Pressed() && quickActionCameraProfileSwitch == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionCameraProfileSwitch == p->iActionQuickButton3) )
   {
      if ( ! quickActionCheckVehicle("switch camera profile") )
         return;
      if ( NULL == g_pCurrentModel || g_pCurrentModel->is_spectator )
         return;

      int tmp = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
      g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile++;
      if ( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile >= MODEL_CAMERA_PROFILES )
         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile = 0;
      handle_commands_abandon_command();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PROFILE, g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile, NULL, 0) )
         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile = tmp;
      return;
   }

   if ( (isKeyQA1Pressed() && quickActionRelaySwitch == p->iActionQuickButton1) ||
        (isKeyQA2Pressed() && quickActionRelaySwitch == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionRelaySwitch == p->iActionQuickButton3) )
   {
      if ( ! quickActionCheckVehicle("relay switch") )
         return;
      if ( NULL == g_pCurrentModel || g_pCurrentModel->is_spectator )
         return;
      if ( get_current_timestamp_ms() < s_uTimeLastQuickActionPress + 500 )
         return;
      
      s_uTimeLastQuickActionPress = get_current_timestamp_ms();

      if ( NULL == g_pCurrentModel || ( ! pairing_isReceiving() ) )
      {
         Popup* p =new Popup("You must be connected to a vehicle to switch relaying!", 0.1,0.8, 0.54, 4);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
         return;
      }

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId <= 0 )
      {
         char szBuff[128];
         snprintf(szBuff, sizeof(szBuff), "Relaying is not enabled on this vehicle (%s)!", g_pCurrentModel->getLongName() );
         Popup* p = new Popup(szBuff, 0.1,0.8, 0.5, 4);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
         return;
      }

      if ( g_pCurrentModel->relay_params.uCurrentRelayMode == RELAY_MODE_NONE )
      if ( ! link_is_relayed_vehicle_online() )
      {
         Popup* p = new Popup("Relayed vehicle is not online. Can't switch to relay it.", 0.1,0.8, 0.54, 4);
         p->setCentered();
         p->setBottomAlign(true);
         p->setBottomMargin(0.2);
         p->setIconId(g_idIconInfo, get_Color_IconWarning());
         popups_add_topmost(p);
         //return;
      }
      
      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

      if ( params.uCurrentRelayMode == RELAY_MODE_NONE )
         params.uCurrentRelayMode = RELAY_MODE_REMOTE;
      else
         params.uCurrentRelayMode = RELAY_MODE_NONE;

      log_line("Pressed QA button to switch relay mode to mode: %d", params.uCurrentRelayMode);

      handle_commands_abandon_command();
      handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters));
      return;
   }

   if ( (isKeyQA1Pressed() && quickActionVideoRecord == p->iActionQuickButton1) ||
        (isKeyQA2Pressed() && quickActionVideoRecord == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionVideoRecord == p->iActionQuickButton3) )
   {
      if ( get_current_timestamp_ms() < s_uTimeLastQuickActionPress + 500 )
         return;
      s_uTimeLastQuickActionPress = get_current_timestamp_ms();

      if ( NULL == g_pCurrentModel )
      {
         Popup* p = new Popup("You must be connected to a vehicle to start video recording.", 0.1,0.8, 0.54, 5);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
         return;
      }
      if ( g_bVideoRecordingStarted )
      {
         ruby_stop_recording();
      }
      else
      {
         if ( ! pairing_isReceiving() )
         {
            Popup* p = new Popup("You must be connected to a vehicle to start video recording.", 0.1,0.8, 0.54, 5);
            p->setIconId(g_idIconError, get_Color_IconError());
            popups_add_topmost(p);
         }
         else
            ruby_start_recording();
      }
      return;
   }

   #ifdef FEATURE_ENABLE_RC
   if ( NULL != g_pCurrentModel )
   if ( (isKeyQA1Pressed() && quickActionRCEnable == p->iActionQuickButton1) ||
        (isKeyQA2Pressed() && quickActionRCEnable == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionRCEnable == p->iActionQuickButton3) )
   {
      if ( ! quickActionCheckVehicle("enable/disable the RC link output") )
         return;

      rc_parameters_t params;
      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));

      if ( params.flags & RC_FLAGS_OUTPUT_ENABLED )
         params.flags &= (~RC_FLAGS_OUTPUT_ENABLED);
      else
         params.flags |= RC_FLAGS_OUTPUT_ENABLED;
      handle_commands_abandon_command();
      handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t));
      return;
   }
   #endif

   if ( (isKeyQA1Pressed() && quickActionRotaryFunction == p->iActionQuickButton1) ||
        (isKeyQA2Pressed() && quickActionRotaryFunction == p->iActionQuickButton2) ||
        (isKeyQA3Pressed() && quickActionRotaryFunction == p->iActionQuickButton3) )
   {
      pCS->nRotaryEncoderFunction++;
      if ( pCS->nRotaryEncoderFunction > 2 )
         pCS->nRotaryEncoderFunction = 1;
      save_ControllerSettings();
      if ( 0 == pCS->nRotaryEncoderFunction )
         warnings_add("Rotary Encoder function changed to: None");
      if ( 1 == pCS->nRotaryEncoderFunction )
         warnings_add("Rotary Encoder function changed to: Menu Navigation");
      if ( 2 == pCS->nRotaryEncoderFunction )
         warnings_add("Rotary Encoder function changed to: Camera Adjustment");

      return;
   }
}

bool ruby_is_first_pairing_done()
{
   if ( access( FILE_FIRST_PAIRING_DONE, R_OK ) == -1 )
      return false;
   return true;
}

void ruby_set_is_first_pairing_done()
{
   FILE* fd = fopen(FILE_FIRST_PAIRING_DONE, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to create file for marking 'first pairing done' flag.");
      return;
   }
   fprintf(fd, "1");
   fclose(fd);
   g_bFirstModelPairingDone = true;
   log_line("Set 'first pairing done' flag.");
}

void r_check_processes_filesystem()
{
   char szFilesMissing[1024];
   szFilesMissing[0] = 0;
   bool failed = false;
   if( access( "ruby_rt_station", R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ruby_rt_station", sizeof(szFilesMissing)); }
   if( access( "ruby_rx_telemetry", R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ruby_rx_telemetry", sizeof(szFilesMissing)); }
   if( access( "ruby_video_proc", R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ruby_video_proc", sizeof(szFilesMissing)); }
   if( access( VIDEO_PLAYER_PIPE, R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ", sizeof(szFilesMissing)); strlcat(szFilesMissing, VIDEO_PLAYER_PIPE, sizeof(szFilesMissing)); }
   if( access( VIDEO_PLAYER_OFFLINE, R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ", sizeof(szFilesMissing)); strlcat(szFilesMissing, VIDEO_PLAYER_OFFLINE, sizeof(szFilesMissing)); }

   log_line("Checked proccesses. Result: %s", failed?"failed":"all ok");

   if ( failed )
   {
      log_error_and_alarm("Some Ruby binaries are missing: [%s].", szFilesMissing);

      Popup* p = new Popup("Some processes are missing from the SD card!", 0.2, 0.65, 0.6, 0);
      p->setCentered();
      p->setIconId(g_idIconError,get_Color_IconError());
      popups_add(p);
   }
}

void ruby_load_models()
{
   g_pCurrentModel = NULL;

   log_line("Loading models...");
   loadModels();
   log_line("Loading models complete. Loading spectator models...");
   loadModelsSpectator();
   log_line("Loaded spectator models complete.");


   if( access( FILE_CURRENT_VEHICLE_MODEL, R_OK ) == -1 )
   if( access( FILE_CURRENT_VEHICLE_MODEL_BACKUP, R_OK ) == -1 )
   {
      log_softerror_and_alarm("Failed to load the current vehicle model. No current vehicle file.");
      return;
   }
   log_line("Loading current model...");
   g_pCurrentModel = new Model();
   if ( !g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
   {
      log_softerror_and_alarm("Failed to load the current vehicle model. Invalid file.");
      g_pCurrentModel = NULL;
   }
   if ( NULL != g_pCurrentModel )
   {
      log_line("Current model is in %s mode", g_pCurrentModel->is_spectator?"spectator":"controller");
      char szFreq1[64];
      char szFreq2[64];
      char szFreq3[64];
      strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[0]));
      strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[1]));
      strcpy(szFreq3, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[2]));
      
      log_line("Current model radio links: %d, 1st radio link frequency: %s, 2nd radio link: %s, 3rd radio link: %s",
         g_pCurrentModel->radioLinksParams.links_count, 
         szFreq1, szFreq2, szFreq3);
   }
}


void start_loop()
{
   hardware_sleep_ms(START_SEQ_DELAY);
   log_line("Executing start up sequence step: %d", s_StartSequence);
   if ( s_StartSequence == START_SEQ_PRE_LOAD_CONFIG )
   {
      log_line("Loading configuration...");
      popupStartup.setTitle(SYSTEM_NAME);
      if ( g_bIsReinit )
         popupStartup.addLine("Restarting...");
      
      popupStartup.addLine("Loading configuration...");
      log_line("Finished executing start up sequence step: %d", s_StartSequence);
      s_StartSequence = START_SEQ_LOAD_CONFIG;
      return;
   }
   if ( s_StartSequence == START_SEQ_LOAD_CONFIG )
   {
      if ( ! load_Preferences() )
         save_Preferences();
      apply_Preferences();

      load_PluginsSettings();
      load_CorePluginsSettings();
      load_CorePlugins(1);
      if ( access("/sys/class/net/usb0", R_OK ) == -1 )
      {
         char szBuff[256];
         snprintf(szBuff, sizeof(szBuff), "rm -rf %s", TEMP_USB_TETHERING_DEVICE);
         hw_execute_bash_command(szBuff, NULL);
      }

      log_line("Finished executing start up sequence step: %d", s_StartSequence);
      s_StartSequence = START_SEQ_PRE_SEARCH_DEVICES;
      return;
   }

   if ( s_StartSequence == START_SEQ_PRE_SEARCH_DEVICES )
   {
      popupStartup.addLine("Searching for external devices...");
      s_StartSequence = START_SEQ_SEARCH_DEVICES;
      return;
   }

   if ( s_StartSequence == START_SEQ_SEARCH_DEVICES )
   {
      hardware_i2c_check_and_update_device_settings();
      ruby_signal_alive();
      controller_stop_i2c();
      controller_start_i2c();
      ruby_signal_alive();
      hardware_sleep_ms(200);

      s_StartSequence = START_SEQ_PRE_SEARCH_INTERFACES;
      return;
   }

   if ( s_StartSequence == START_SEQ_PRE_SEARCH_INTERFACES )
   {
      log_line("Enumerating controller input interfaces...");
      popupStartup.addLine("Enumerating controller input interfaces...");
      s_StartSequence = START_SEQ_SEARCH_INTERFACES;
      return;
   }

   if ( s_StartSequence == START_SEQ_SEARCH_INTERFACES )
   {
      controllerInterfacesEnumJoysticks();
      s_StartSequence = START_SEQ_PRE_NICS;
      return;
   }

   if ( s_StartSequence == START_SEQ_PRE_NICS )
   {
      log_line("Getting radio hardware info...");
      popupStartup.addLine("Getting radio hardware info...");
      hardware_enumerate_radio_interfaces();

      ControllerSettings* pcs = get_ControllerSettings();
      radio_info_wifi_t dptr;
      if ( hardware_get_basic_radio_wifi_info(&dptr) )
      {
         if ( dptr.tx_power != pcs->iTXPower ||
              dptr.tx_powerAtheros != pcs->iTXPowerAtheros ||
              dptr.tx_powerRTL != pcs->iTXPowerRTL )
         {
            log_line("Radio config differs from stored preferences (txpower on system: %d,%d,%d != txpower on config file: %d,%d,%d). Updating controller settings.", dptr.tx_power, dptr.tx_powerAtheros, dptr.tx_powerRTL, pcs->iTXPower, pcs->iTXPowerAtheros, pcs->iTXPowerRTL);
            pcs->iTXPower = dptr.tx_power;
            pcs->iTXPowerAtheros = dptr.tx_powerAtheros;
            pcs->iTXPowerRTL = dptr.tx_powerRTL;
            save_ControllerSettings();
            popupStartup.addLine("Radio config was updated.");
         }
         else
            popupStartup.addLine("Radio config loaded ok.");
      }

      log_line("Finished executing start up sequence step: %d", s_StartSequence);
      s_StartSequence = START_SEQ_NICS;
      return;
   }


   if ( s_StartSequence == START_SEQ_NICS )
   {
     if ( 0 == hardware_get_radio_interfaces_count() )
     {
         Popup* p = new Popup("No radio interfaces detected on your controller.",0.3,0.4, 0.5, 6);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
     }
     else
     {
        bool allDisabled = true;
        for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
        {
           radio_hw_info_t* pNIC = hardware_get_radio_info(i);
           u32 flags = controllerGetCardFlags(pNIC->szMAC);
           if ( ! ( flags & RADIO_HW_CAPABILITY_FLAG_DISABLED ) )
              allDisabled = false;
        }
        if ( allDisabled )
        {
           Popup* p = new Popup("All radio interfaces are disabled on the controller.", 0.3,0.4, 0.5, 6);
           p->setIconId(g_idIconError, get_Color_IconError());
           p->addLine("Go to [Menu]->[Controller]->[Radio Interfaces] and enable at least one radio interface.");
           popups_add_topmost(p);
           radio_set_out_datarate(DEFAULT_RADIO_DATARATE);
           log_line("Finished executing start up sequence step: %d", s_StartSequence);
           s_StartSequence = START_SEQ_PRE_SYNC_DATA;
           return;
        }

        int countUnsuported = 0;
        for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
        {
           radio_hw_info_t* pNIC = hardware_get_radio_info(i);
           if ( 0 == pNIC->isSupported )
              countUnsuported++;
        }
        if ( countUnsuported == hardware_get_radio_interfaces_count() )
        {
            Popup* p = new Popup("No radio interface on your controller is fully supported.", 0.3,0.4, 0.5, 6);
            p->setIconId(g_idIconError, get_Color_IconError());
            popups_add_topmost(p);
        }
        else if ( countUnsuported > 0 )
        {
            Popup* p = new Popup("Some radio interfaces on your controller are not fully supported.", 0.3,0.4, 0.5, 6);
            p->setIconId(g_idIconWarning, get_Color_IconWarning());
            popups_add_topmost(p);
        }

        if ( controllerAddedNewRadioInterfaces() )
        {
           save_ControllerInterfacesSettings();
           add_menu_to_stack(new MenuInfoBooster());
        }

        // Check & update radio cards models
        bool bChanged = false;
        ControllerInterfacesSettings* pCIS = get_ControllerInterfacesSettings();
        for( int i=0; i<pCIS->radioInterfacesCount; i++ )
        {
            // If user defined, do not change it
            if ( pCIS->listRadioInterfaces[i].cardModel < 0 )
               continue;
            radio_hw_info_t* pNICInfo = hardware_get_radio_info_from_mac(pCIS->listRadioInterfaces[i].szMAC);
            if ( NULL != pNICInfo )
            if ( pCIS->listRadioInterfaces[i].cardModel != pNICInfo->iCardModel )
            {
               pCIS->listRadioInterfaces[i].cardModel = pNICInfo->iCardModel;
               bChanged = true;
            }
        }
        if ( bChanged )
          save_ControllerInterfacesSettings();
     }
     radio_set_out_datarate(DEFAULT_RADIO_DATARATE);
     log_line("Finished executing start up sequence step: %d", s_StartSequence);
     s_StartSequence = START_SEQ_PRE_SYNC_DATA;
     return;
   }

   if ( s_StartSequence == START_SEQ_PRE_SYNC_DATA )
   {
      Preferences* pP = get_Preferences();
      if ( 1 == pP->iAutoExportSettings )
      {
         popupStartup.addLine("Synchronizing data...");
         s_StartSequence = START_SEQ_SYNC_DATA;
         return;
      }

      s_StartSequence = START_SEQ_PRE_LOAD_DATA;
      return;
   }

   if ( s_StartSequence == START_SEQ_SYNC_DATA )
   {
      log_line("Auto export settings is enabled. Checking boot count...");

      if ( g_iBootCount < 3 )
      {
         log_line("First or second boot (%d), skipping auto export", g_iBootCount);
         popupStartup.addLine("Synchronizing data skipped.");
         log_line("Finished executing start up sequence step: %d", s_StartSequence);
         s_StartSequence = START_SEQ_PRE_LOAD_DATA;
         return;
      }

      log_line("Doing auto export...");
      int nResult = controller_utils_export_all_to_usb();
      if ( nResult != 0 )
      {
            if ( nResult == -2 )
            {
               log_line("Failed to synchonize data to USB memory stick, no memory stick.");
               popupStartup.addLine("Synchronizing data skipped. No USB memory stick.");
            }
            else if ( nResult == -10 || nResult == -11 )
            {
               log_softerror_and_alarm("Failed to synchonize data to USB memory stick, failed to write data to USB stick.");
               popupStartup.addLine("Synchronizing data failed to write to USB memory stick.");
            }
            else
            {
               log_softerror_and_alarm("Failed to synchonize data to USB memory stick.");
               popupStartup.addLine("Synchronizing data failed.");
            }
            s_StartSequence = START_SEQ_SYNC_DATA_FAILED;
            log_line("Finished executing start up sequence step: %d", s_StartSequence);
            return;
      }
      log_line("Auto export done.");

      log_line("Finished executing start up sequence step: %d", s_StartSequence);
      popupStartup.addLine("Synchronizing data complete.");
      s_StartSequence = START_SEQ_PRE_LOAD_DATA;
      return;
   }

   if ( s_StartSequence == START_SEQ_SYNC_DATA_FAILED )
   {
      static int s_iSyncFailedLoop = 0;
      s_iSyncFailedLoop++;
      if ( s_iSyncFailedLoop > 5 )
      {
         log_line("Finished executing start up sequence step: %d", s_StartSequence);
         s_StartSequence = START_SEQ_PRE_LOAD_DATA;
      }
      return;
   }

   if ( s_StartSequence == START_SEQ_PRE_LOAD_DATA )
   {
      log_line("Start sequence: PRE_LOAD_DATA");
      log_line("Loading models...");
      popupStartup.addLine("Loading models...");
      log_line("Finished executing start up sequence step: %d", s_StartSequence);
      s_StartSequence = START_SEQ_LOAD_DATA;
      return;
   }

   if ( s_StartSequence == START_SEQ_LOAD_DATA )
   {
      log_line("Start sequence: LOAD_DATA");

      // Check file system for write access

      log_line("Checking the file system for write access...");
      bool bWriteFailed = false;

      hw_execute_bash_command("rm -rf tmp/testwrite.txt", NULL);
      FILE* fdTemp = fopen("tmp/testwrite.txt", "wb");
      if ( NULL == fdTemp )
      {
         alarms_add_from_local(ALARM_ID_CONTROLLER_STORAGE_WRITE_ERRROR, 0, 1);
         bWriteFailed = true;
      }
      else
      {
         fprintf(fdTemp, "test1234\n");
         fclose(fdTemp);
         fdTemp = fopen("tmp/testwrite.txt", "rb");
         if ( NULL == fdTemp )
         {
            alarms_add_from_local(ALARM_ID_CONTROLLER_STORAGE_WRITE_ERRROR, 0, 1);
            bWriteFailed = true;
         }
         else
         {
            char szTmp[256];
            if ( 1 != fscanf(fdTemp, "%s", szTmp) )
            {
               alarms_add_from_local(ALARM_ID_CONTROLLER_STORAGE_WRITE_ERRROR, 0, 1);
               bWriteFailed = true;
            }
            else if ( 0 != strcmp(szTmp, "test1234") )
            {
               alarms_add_from_local(ALARM_ID_CONTROLLER_STORAGE_WRITE_ERRROR, 0, 1);
               bWriteFailed = true;
            }
            fclose(fdTemp);
            hw_execute_bash_command("rm -rf tmp/testwrite.txt", NULL);
         }
      }
               
      if ( bWriteFailed )
         log_line("Checking the file system for write access: Failed.");
      else
         log_line("Checking the file system for write access: Succeeded.");
         
      ruby_load_models();
      
      if ( NULL == g_pCurrentModel )
      {
         if ( 0 == getModelsCount() )
         {
            popupNoModel.setTitle("Info");
            popupNoModel.addLine("You have no vehicles linked to this controller.");
            popupNoModel.addLine("Please press [Menu] and then select [Search] to search for a vehicle to connect to.");
            popupNoModel.setIconId(g_idIconInfo,get_Color_IconWarning());
         }
         else
         {
            popupNoModel.setTitle("Info");
            popupNoModel.addLine("You have no vehicle selected as active.");
            popupNoModel.addLine("Please press [Menu] and then select [My Vehicles] to select the vehicle to connect to.");
            popupNoModel.setIconId(g_idIconInfo,get_Color_IconWarning());
         }

         popups_add(&popupNoModel);
         //launch_configure_nics(false, NULL);
         log_line("Finished executing start up sequence step: %d", s_StartSequence);
         s_StartSequence = START_SEQ_SCAN_MEDIA_PRE;
         return;
      }
      if ( ! load_temp_local_stats() )
         local_stats_on_disarm();

      log_line("Configuring radio...");
      popupStartup.addLine("Configuring radio...");
      log_line("Finished executing start up sequence step: %d", s_StartSequence);
      s_StartSequence = START_SEQ_SCAN_MEDIA_PRE;
      return;
   }


   if ( s_StartSequence == START_SEQ_SCAN_MEDIA_PRE )
   {
      log_line("Start sequence: START_SEQ_SCAN_MEDIA_PRE");
      popupStartup.addLine("Scanning media storage...");
      log_line("Start sequence: Scanning media storage...");
      if ( access(TEMP_VIDEO_FILE, R_OK) != -1 )
      if ( access(TEMP_VIDEO_FILE_INFO, R_OK) != -1 )
      {
         long fSize = 0;
         FILE *pF = fopen(TEMP_VIDEO_FILE, "rb");
         if ( NULL != pF )
         {
            fseek(pF, 0, SEEK_END);
            fSize = ftell(pF);
            fseek(pF, 0, SEEK_SET);
            fclose(pF);
         }

         log_line("Processing unfinished video file %s, length: %d bytes", TEMP_VIDEO_FILE, fSize);

         char szBuff[64];
         snprintf(szBuff, sizeof(szBuff), "nice -n 5 ./ruby_video_proc &");
         hw_execute_bash_command(szBuff, NULL);
         log_line("Start sequence: Processing unfinished video file...");
         popupStartup.addLine("Processing unfinished video file...");
         hardware_sleep_ms(200);

         g_bVideoRecordingStarted = false;
         g_bVideoProcessing = true;
         link_watch_mark_started_video_processing();
         warnings_add("Processing video file...", g_idIconCamera, get_Color_IconNormal());

         log_line("Finished executing start up sequence step: %d", s_StartSequence);
         s_StartSequence = START_SEQ_PROCESS_VIDEO;
         return;
      }
      log_line("Finished executing start up sequence step: %d", s_StartSequence);
      s_StartSequence = START_SEQ_SCAN_MEDIA;
      return;
   }
   if ( s_StartSequence == START_SEQ_PROCESS_VIDEO )
   {
      hardware_sleep_ms(200);
      /*
      char szPids[1024];
      bool procRunning = false;
      hw_execute_bash_command_silent("pidof ruby_video_proc", szPids);
      if ( strlen(szPids) > 2 )
         procRunning = true;
      if ( procRunning )
         return;

      log_line("Start sequence: Done processing temporary video file.");
      popupStartup.addLine("Done processing temporary video file.");
      */
      log_line("Start sequence: Processing temporary video file will be done in background.");
      popupStartup.addLine("Moved processing of temporary video file to background.");
      log_line("Finished executing start up sequence step: %d", s_StartSequence);
      s_StartSequence = START_SEQ_SCAN_MEDIA;
   }

   if ( s_StartSequence == START_SEQ_SCAN_MEDIA )
   {
      log_line("Start sequence: START_SEQ_SCAN_MEDIA");
      media_init_and_scan();
      Preferences* p = get_Preferences();
      popup_log_set_show_flag(p->iShowLogWindow);

      if ( is_first_boot() )
         popups_add_topmost(new Popup("One time automatic setup after instalation done: Your CPU settings have been adjusted to match your Raspberry Pi. Please reboot the controller at your convenience for the new changes to take effect.", 0.2,0.1, 0.74, 11));

      log_line("Configuring processes...");
      popupStartup.addLine("Configuring processes...");
      log_line("Finished executing start up sequence step: %d", s_StartSequence);
      s_StartSequence = START_SEQ_START_PROCESSES;
      return;
   }
   
   if ( s_StartSequence == START_SEQ_START_PROCESSES )
   {
      log_line("Start sequence: START_SEQ_START_PROCESSES");
      link_watch_init();
      osd_plugins_load();
      r_check_processes_filesystem();
      if ( NULL == g_pCurrentModel )
      {
         s_StartSequence = START_SEQ_COMPLETED;
         log_line("Start sequence: COMPLETED.");
         log_line("System Configured. Start sequence done.");
         popupStartup.addLine("System Configured.");
         popupStartup.addLine("Start sequence done.");
         popupStartup.setTimeout(4);
         popupStartup.resetTimer();
         log_line("Error on finished executing start up sequence step: %d", s_StartSequence);
         return;
      }
      onMainVehicleChanged();
      if ( 0 < hardware_get_radio_interfaces_count() )
         pairing_start();

      g_pProcessStatsTelemetry = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_TELEMETRY_RX);
      if ( NULL == g_pProcessStatsTelemetry )
         log_softerror_and_alarm("Failed to open shared mem to telemetry rx process watchdog stats for reading: %s", SHARED_MEM_WATCHDOG_TELEMETRY_RX);
      else
         log_line("Opened shared mem to telemetry rx process watchdog stats for reading.");

      g_pProcessStatsRouter = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_ROUTER_RX);
      if ( NULL == g_pProcessStatsRouter )
         log_softerror_and_alarm("Failed to open shared mem to video rx process watchdog stats for reading: %s", SHARED_MEM_WATCHDOG_ROUTER_RX);
      else
         log_line("Opened shared mem to video rx process watchdog stats for reading.");

      if ( NULL != g_pCurrentModel && g_pCurrentModel->rc_params.rc_enabled )
      {
         g_pProcessStatsRC = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_RC_TX);
         if ( NULL == g_pProcessStatsRC )
            log_softerror_and_alarm("Failed to open shared mem to RC tx process watchdog stats for reading: %s", SHARED_MEM_WATCHDOG_RC_TX);
         else
            log_line("Opened shared mem to RC tx process watchdog stats for reading.");
      }

      if ( g_bDebug )
      {
         s_StartSequence = START_SEQ_COMPLETED;
         g_bIsRouterPacketsHistoryGraphOn = true;
         t_packet_header PH; 
         PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
         PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_START;
         handle_commands_send_ruby_message(&PH, NULL, 0);
         hardware_sleep_ms(500);
         g_pDebugSMRPST = shared_mem_router_packets_stats_history_open_read();
         if ( NULL == g_pDebugSMRPST )
         {
            log_softerror_and_alarm("Failed to open shared mem for read for router packets stats history.");
            return;
         } 
      }

      s_StartSequence = START_SEQ_COMPLETED;
      log_line("Start sequence: COMPLETED.");
      log_line("System Configured. Start sequence done.");
      popupStartup.addLine("System Configured.");
      if ( g_bIsReinit )
         popupStartup.addLine("Restarting checks finished. Will restart now...");
      else
         popupStartup.addLine("Start sequence done.");
      popupStartup.setTimeout(4);
      popupStartup.resetTimer();

      s_TimeCentralInitializationComplete = g_TimeNow;

      if ( g_bIsHDMIConfirmation )
      {
         s_pMenuConfirmHDMI = new MenuConfirmationHDMI("HDMI Output Configuration Updated","Does the HDMI output looks ok? Select [Yes] to keep the canges or select [No] to revert to the old HDMI configuration.", 0);
         s_pMenuConfirmHDMI->m_yPos = 0.3;
         add_menu_to_stack(s_pMenuConfirmHDMI);
      }

      int iMajor = 0;
      int iMinor = 0;
      get_Ruby_BaseVersion(&iMajor, &iMinor);
      if ( iMajor < 6 )
      {
         Popup* p = new Popup(true, "Can't update from version older than 6.0. You need to do a full install of version 6.0 or a newer one.", 10);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
      }

      if ( g_iBootCount == 2 )
      {
         log_line("First boot detected of the UI after install. Checking for import settings from USB...");
         if ( controller_utils_usb_import_has_any_controller_id_file() )
         {
            log_line("USB has exported settings. Add confirmation to import them.");
            menu_close_all();
            s_pMenuConfirmationImport = new MenuConfirmationImport("Automatic Import", "There are controller settings saved and present on the USB stick. Do you want to import them?", 55);
            s_pMenuConfirmationImport->m_yPos = 0.3;
            add_menu_to_stack(s_pMenuConfirmationImport);
         }
      }
      ruby_resume_watchdog();
      log_line("Finished executing start up sequence step: %d", s_StartSequence);
      return;
   }
}

void packets_scope_input_loop()
{
   hardware_sleep_ms(20);
   t_packet_header PH;

   if ( isKeyPlusPressed() )
   {
      s_PacketsScopeGraphZoomLevel++;
      if ( s_PacketsScopeGraphZoomLevel > 2 )
         s_PacketsScopeGraphZoomLevel = 0;
   }

   if ( isKeyMinusPressed() )
   {
      s_PacketsScopeGraphZoomLevel--;
      if ( s_PacketsScopeGraphZoomLevel > 2 )
         s_PacketsScopeGraphZoomLevel = 2;
   }

   if ( isKeyMenuPressed() )
   {
      g_bIsRouterPacketsHistoryGraphPaused = ! g_bIsRouterPacketsHistoryGraphPaused;

         if ( g_bIsRouterPacketsHistoryGraphPaused )
         {
            PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
            PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_PAUSE;
            handle_commands_send_ruby_message(&PH, NULL, 0);
         }
         else
         {
            PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
            PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_RESUME;
            handle_commands_send_ruby_message(&PH, NULL, 0);
         }
   }

   if ( isKeyBackPressed() )
   {
         PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
         PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_STOP;
         handle_commands_send_ruby_message(&PH, NULL, 0);
         g_bIsRouterPacketsHistoryGraphPaused = false;
         g_bIsRouterPacketsHistoryGraphOn = false;
   }
}

void ruby_input_loop(bool bNoKeys)
{
   ControllerSettings* pCS = get_ControllerSettings();

   hardware_loop();

   if ( 0 != hardware_get_radio_interfaces_count() )
      handle_commands_loop();

   if ( g_bIsRouterPacketsHistoryGraphOn )
      packets_scope_input_loop();
   else
   {
       if ( pCS->iFreezeOSD )
       if ( pCS->iDeveloperMode )
       if ( isKeyBackPressed() )
       if ( ! isMenuOn() )
       if ( g_TimeNow > s_uTimeFreezeOSD + 200 )
       {
          s_uTimeFreezeOSD = g_TimeNow;
          s_bFreezeOSD = ! s_bFreezeOSD;
       }

      //if ( g_TimeNow > s_TimeLastMenuInput + 20 )
      {
         s_TimeLastMenuInput = g_TimeNow;
         if ( ! bNoKeys )
         {
            menu_loop();
            if ( isKeyQA1Pressed() || isKeyQA2Pressed() || isKeyQA3Pressed() )
               executeQuickActions();
         }
      }
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
      return;

   if ( g_iMustSendCurrentActiveOSDLayoutCounter > 0 )
   if ( g_TimeNow >= g_TimeLastSentCurrentActiveOSDLayout+200 )
   {
      g_iMustSendCurrentActiveOSDLayoutCounter--;
      g_TimeLastSentCurrentActiveOSDLayout = g_TimeNow;
      handle_commands_decrement_command_counter();
      handle_commands_send_single_oneway_command(0, COMMAND_ID_SET_OSD_CURRENT_LAYOUT, (u32)g_pCurrentModel->osd_params.layout, NULL, 0);
   }

   pairing_loop();
   if ( g_bIsRouterReady )
   {
      link_watch_loop();
      local_stats_update_loop();
      forward_streams_loop();
   }
}

void main_loop_r_central()
{
   ControllerSettings* pCS = get_ControllerSettings();

   hardware_sleep_ms(2);
   
   try_read_messages_from_router(7);

   ruby_input_loop(false);

   if ( s_StartSequence != START_SEQ_COMPLETED && s_StartSequence != START_SEQ_FAILED )
   {
      hardware_sleep_ms(5);
      start_loop();
      render_all(g_TimeNow, false, false);
      if ( NULL != s_pProcessStatsCentral )
         s_pProcessStatsCentral->lastActiveTime = g_TimeNow;
      return;
   }

   if ( s_StartSequence != START_SEQ_COMPLETED )
      return;

   compute_cpu_load(g_TimeNow);

   int dt = 1000/15;
   if ( 0 != pCS->iRenderFPS )
      dt = 1000/pCS->iRenderFPS;
   if ( g_TimeNow >= s_TimeLastRender+dt )
   {
      ruby_signal_alive();
      s_TimeLastRender = g_TimeNow;
      render_all(g_TimeNow, false, false);
      if ( NULL != s_pProcessStatsCentral )
         s_pProcessStatsCentral->lastActiveTime = g_TimeNow;

      if ( g_bIsReinit )
      if ( s_iFPSCount > 5 )
         quit = true;
   }

   if ( g_bIsHDMIConfirmation )
   if ( NULL != s_pMenuConfirmHDMI )
   if ( g_TimeNow > s_TimeCentralInitializationComplete + 10000 )
   if ( menu_is_menu_on_top(s_pMenuConfirmHDMI) )
   if ( access( FILE_TMP_HDMI_CHANGED, R_OK ) != -1 )         
   {
      log_line("Reverting HDMI resolution change...");
      ruby_pause_watchdog();
      save_temp_local_stats();
      hardware_sleep_ms(50);

      FILE* fd = fopen(FILE_TMP_HDMI_CHANGED, "r");
      if ( NULL != fd )
      {
         char szBuff[256];
         int group, mode;
         int tmp1, tmp2, tmp3;
         fscanf(fd, "%d %d", &tmp1, &tmp2 );
         fscanf(fd, "%d %d %d", &tmp1, &tmp2, &tmp3 );
         fscanf(fd, "%d %d", &group, &mode );
         fclose(fd);
         log_line("Reverting HDMI resolution back to: group: %d, mode: %d", group, mode);

         snprintf(szBuff, sizeof(szBuff), "rm -rf %s", FILE_TMP_HDMI_CHANGED);
         hw_execute_bash_command(szBuff, NULL);

         hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

         snprintf(szBuff, sizeof(szBuff), "sed -i 's/hdmi_group=[0-9]*/hdmi_group=%d/g' config.txt", group);
         hw_execute_bash_command(szBuff, NULL);
         snprintf(szBuff, sizeof(szBuff), "sed -i 's/hdmi_mode=[0-9]*/hdmi_mode=%d/g' config.txt", mode);
         hw_execute_bash_command(szBuff, NULL);
         hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
      }
      onEventReboot();
      hw_execute_bash_command("sudo reboot -f", NULL);
   }
}

void ruby_signal_alive()
{
   if ( NULL != s_pProcessStatsCentral )
      s_pProcessStatsCentral->lastActiveTime = g_TimeNow;
   else
      log_softerror_and_alarm("Shared mem for self process stats is NULL. Can't signal to others that process is active.");
}

void ruby_pause_watchdog()
{
   g_TimeNow = get_current_timestamp_ms();
   ruby_signal_alive();
   s_iCountRequestsPauseWatchdog++;
   if ( 1 == s_iCountRequestsPauseWatchdog )
   {
      log_line("Pause watchdog [%d] signal others.", s_iCountRequestsPauseWatchdog);
      char szComm[256];
      snprintf(szComm, sizeof(szComm), "touch %s", FILE_TMP_CONTROLLER_PAUSE_WATCHDOG);
      hw_execute_bash_command_silent(szComm, NULL);
   }
   else
      log_line("Pause watchdog [%d].", s_iCountRequestsPauseWatchdog);
}

void ruby_resume_watchdog()
{
   g_TimeNow = get_current_timestamp_ms();
   ruby_signal_alive();
   s_iCountRequestsPauseWatchdog--;
   if ( 0 == s_iCountRequestsPauseWatchdog )
   {
      hardware_sleep_ms(20);
      log_line("Resumed watchdog [%d] signal others.", s_iCountRequestsPauseWatchdog);
      char szComm[256];
      snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_TMP_CONTROLLER_PAUSE_WATCHDOG);
      hw_execute_bash_command_silent(szComm, NULL);
   }
   else
   {
      if ( s_iCountRequestsPauseWatchdog < 0 )
         s_iCountRequestsPauseWatchdog = 0;
      log_line("Resumed watchdog [%d].", s_iCountRequestsPauseWatchdog);
   }
}

   
void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   quit = true;
} 

int main(int argc, char *argv[])
{
   signal(SIGPIPE, SIG_IGN);
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }
   
   setlocale(LC_ALL, "en_GB.UTF-8");
   log_init("Central");

   check_licences();
   
   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebug = true;

   g_uControllerId = controller_utils_getControllerId();
   log_line("Controller UID: %u", g_uControllerId);
 
   if ( strcmp(argv[argc-1], "-reinit") == 0 )
   {
      log_line("Ruby Central crashed last time, reinitializing graphics engine...");
      g_bIsReinit = true;
      /*
      g_pRenderEngine = render_init_engine();
      s_idBgImage = g_pRenderEngine->loadImage("res/ruby_bg2.png");

      char szComm[256];
      snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_TMP_CONTROLLER_CENTRAL_CRASHED);
      hw_execute_bash_command(szComm, NULL);         

      g_TimeNow = get_current_timestamp_ms();

      g_pRenderEngine->startFrame();
      g_pRenderEngine->drawImage(0, 0, 1,1, s_idBgImage);
      g_pRenderEngine->endFrame();

      while ( get_current_timestamp_ms() < g_TimeNow+5000 )
      {
         hardware_sleep_ms(5);
         g_pRenderEngine->startFrame();
         g_pRenderEngine->drawImage(0, 0, 1,1, s_idBgImage);
         g_pRenderEngine->endFrame();
      }
      
      log_line("Done reinitializing graphics engine. Exit now.");
      return 0;
      */
   }

   g_bIsHDMIConfirmation = false;
   if ( access( FILE_TMP_HDMI_CHANGED, R_OK ) != -1 )
      g_bIsHDMIConfirmation = true;

   log_line("Ruby UI starting");

   init_hardware();
   hardware_init_serial_ports();
   hdmi_init_modes();
   radio_init_link_structures();
   radio_enable_crc_gen(1);

   ruby_clear_all_ipc_channels();

   s_pProcessStatsCentral = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_CENTRAL);
   if ( NULL == s_pProcessStatsCentral )
      log_softerror_and_alarm("Failed to open shared mem for ruby_central process watchdog for writing: %s", SHARED_MEM_WATCHDOG_CENTRAL);
   else
      log_line("Opened shared mem for ruby_centrall process watchdog for writing.");
 
   ruby_pause_watchdog();

   if ( ! load_Preferences() )
      save_Preferences();

   if ( ! load_ControllerSettings() )
      save_ControllerSettings();

   hardware_i2c_load_device_settings();

   if ( ! load_ControllerInterfacesSettings() )
      save_ControllerInterfacesSettings();
      
   save_ControllerInterfacesSettings();

   Preferences* p = get_Preferences();
   ControllerSettings* pcs = get_ControllerSettings();
   hw_set_priority_current_proc(pcs->iNiceCentral); 

   if ( p->nLogLevel != 0 )
      log_only_errors();

   apply_Preferences();

   g_pRenderEngine = render_init_engine();

   load_resources();

   menu_init();
   log_line("Started.");

   popupStartup.setFont(g_idFontMenu);
   popupNoModel.setFont(g_idFontMenu);
   popups_add(&popupStartup);

   if ( g_bIsReinit )
   {
      Popup* p = new Popup(true, "User Interface is reinitializing, please wait...", 0);
      p->setFont(g_idFontOSDBig);
      p->setIconId(g_idIconInfo, get_Color_IconWarning());
      popups_add(p);
   }

   g_iBootCount = 0;
   FILE* fd = fopen(FILE_BOOT_COUNT, "r");
   if ( NULL != fd )
   {
      fscanf(fd, "%d", &g_iBootCount);
      fclose(fd);
      fd = NULL;
   }

   shared_vars_state_reset_all_vehicles_runtime_info();

   g_bFirstModelPairingDone = false;
   if ( access( FILE_FIRST_PAIRING_DONE, R_OK ) != -1 )
      g_bFirstModelPairingDone = true;
 
   s_StartSequence = START_SEQ_PRE_LOAD_CONFIG;
   log_line("Started main loop.");
   g_TimeStart = get_current_timestamp_ms();

   while (!quit) 
   {
      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();

      if ( rx_scope_is_started() )
      {
         try_read_messages_from_router(10);
         rx_scope_loop();
      }
      else
      {
         u32 uTimeStart = get_current_timestamp_ms();
         main_loop_r_central();
         u32 dTime = get_current_timestamp_ms() - uTimeStart;
         if ( dTime > 200 )
         if ( s_StartSequence == START_SEQ_COMPLETED || s_StartSequence == START_SEQ_FAILED )
            log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
      }
   }
   
   if ( ! g_bIsReinit )
      pairing_stop();
   controller_stop_i2c();
   for( int i=0; i<g_iPluginsOSDCount; i++ )
      if ( NULL != g_pPluginsOSD[i] )
      if ( NULL != g_pPluginsOSD[i]->pLibrary )
         dlclose(g_pPluginsOSD[i]->pLibrary);

   shared_mem_i2c_current_close(g_pSMVoltage);
   shared_mem_i2c_rotary_encoder_buttons_events_close(g_pSMRotaryEncoderButtonsEvents);

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_TELEMETRY_RX, g_pProcessStatsTelemetry);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_RX, g_pProcessStatsRouter);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_RC_TX, g_pProcessStatsRC);

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_CENTRAL, s_pProcessStatsCentral);
 
   hardware_release();
   return 0;
}
