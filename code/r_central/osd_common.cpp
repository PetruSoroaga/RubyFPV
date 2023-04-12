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

#include "osd_common.h"
#include "../base/base.h"
#include "../base/hw_procs.h"
#include "../base/launchers.h"
#include "../base/config.h"
#include "../../mavlink/common/mavlink.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../common/string_utils.h"
#include "osd.h"
#include "osd_stats.h"
#include "osd_lean.h"
#include "osd_warnings.h"
#include "osd_gauges.h"
#include <math.h>
#include <ctype.h>

#include "popup.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "shared_vars.h"
#include "pairing.h"
#include "link_watch.h"
#include "osd_ahi.h"
#include "local_stats.h"
#include "launchers_controller.h"


u32 g_idIconRuby = 0;
u32 g_idIconDrone = 0;
u32 g_idIconPlane = 0;
u32 g_idIconCar = 0;

u32 g_idIconArrowUp = 0;
u32 g_idIconArrowDown = 0;
u32 g_idIconAlarm = 0;
u32 g_idIconGPS = 0;
u32 g_idIconHome = 0;
u32 g_idIconCamera = 0;
u32 g_idIconClock = 0;
u32 g_idIconHeading = 0;
u32 g_idIconThrotle = 0;
u32 g_idIconJoystick = 0;
u32 g_idIconTemperature = 0;
u32 g_idIconCPU = 0;
u32 g_idIconCheckOK = 0;
u32 g_idIconShield = 0;
u32 g_idIconInfo = 0;
u32 g_idIconWarning = 0;
u32 g_idIconError = 0;
u32 g_idIconUplink = 0;
u32 g_idIconUplink2 = 0;
u32 g_idIconRadio = 0;
u32 g_idIconWind = 0;
u32 g_idIconController = 0;
u32 g_idIconX = 0;

float sfScreenXMargin = 0.02;
float sfScreenYMargin = 0.02;
float sfSpacingH = 0.02;
float sfSpacingV = 0.02;
float sfBarHeight = 0.05;

float sfScaleOSD = 1.0;
float sfScaleOSDStats = 1.0;
float sfOSDOutlineThickness = OSD_STRIKE_WIDTH;
int s_iOSDTransparency = 1;

float g_fOSDStatsForcePanelWidth = 0.0;
float g_fOSDStatsBgTransparency = 1.0;

float osd_getFontHeight()
{
   return g_pRenderEngine->textHeight( 0.0, g_idFontOSD);
}

float osd_getFontHeightBig()
{
   return g_pRenderEngine->textHeight( 0.0, g_idFontOSDBig);
}

float osd_getFontHeightSmall()
{
   return g_pRenderEngine->textHeight( 0.0, g_idFontOSDSmall);
}


float osd_getMarginX() { return sfScreenXMargin; }
float osd_getMarginY() { return sfScreenYMargin; }
void osd_setMarginX(float x) { sfScreenXMargin = x; }
void osd_setMarginY(float y) { sfScreenYMargin = y; }

float osd_getBarHeight()
{
   return osd_getFontHeightBig() * 1.32;
}

float osd_getSecondBarHeight()
{
   return osd_getFontHeight() + 1.0 * osd_getSpacingV();
};

float osd_getVerticalBarWidth()
{
   return 4.6*osd_getFontHeightBig()/g_pRenderEngine->getAspectRatio();
}

float osd_getSpacingH()
{
   return 0.6*osd_getFontHeight();
}
float osd_getSpacingV()
{
   return 0.16*osd_getFontHeightBig();
}

float osd_getScaleOSD() { return sfScaleOSD; }
float osd_setScaleOSD(int nValue)
{
   float fOSDScale = 1.2;
   if ( nValue >= 2)
     fOSDScale = fOSDScale + (((float)nValue)-2.0)/4.4;
   else
     fOSDScale = fOSDScale + (((float)nValue)-2.0)/5.0;
   sfScaleOSD = fOSDScale;
   return sfScaleOSD; 
}

float osd_getScaleOSDStats() { return sfScaleOSDStats; }
float osd_setScaleOSDStats(int nValue)
{
   float fOSDScaleStats = 1.0;
   if ( nValue >= 2)
     fOSDScaleStats = fOSDScaleStats + (((float)nValue)-2.0)/4.4;
   else
     fOSDScaleStats = fOSDScaleStats + (((float)nValue)-2.0)/5.0;
   sfScaleOSDStats = fOSDScaleStats;
   return sfScaleOSDStats; 
}

void osd_setOSDOutlineThickness(float fValue) { sfOSDOutlineThickness = fValue; }


float _osd_convertKm(float km)
{
   Preferences* p = get_Preferences();

   if ( p->iUnits == prefUnitsMetric || p->iUnits == prefUnitsMeters )
      return km;
   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
      return km*0.621371;
   return km;
}


float _osd_convertMeters(float m)
{
   Preferences* p = get_Preferences();

   if ( p->iUnits == prefUnitsMetric || p->iUnits == prefUnitsMeters )
      return m;
   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
      return m*3.28084;
   return m;
}


bool osd_load_resources()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      g_fOSDDbm[i] = 0.0f;

   g_idIconRuby = g_pRenderEngine->loadIcon("res/icon_ruby.png");
   g_idIconDrone = g_pRenderEngine->loadIcon("res/icon_v_drone.png");
   g_idIconPlane = g_pRenderEngine->loadIcon("res/icon_v_plane.png");
   g_idIconCar = g_pRenderEngine->loadIcon("res/icon_v_car.png");

   g_idIconArrowUp = g_pRenderEngine->loadIcon("res/icon_arrow_up.png");
   g_idIconArrowDown = g_pRenderEngine->loadIcon("res/icon_arrow_down.png");
   g_idIconAlarm = g_pRenderEngine->loadIcon("res/icon_alarm.png");
   g_idIconCheckOK = g_pRenderEngine->loadIcon("res/icon_checkok.png");
   g_idIconInfo = g_pRenderEngine->loadIcon("res/icon_info.png");
   g_idIconWarning = g_pRenderEngine->loadIcon("res/icon_warning.png");
   g_idIconError = g_pRenderEngine->loadIcon("res/icon_error.png");

   g_idIconGPS = g_pRenderEngine->loadIcon("res/icon_gps.png");
   g_idIconHome = g_pRenderEngine->loadIcon("res/icon_home.png");
   g_idIconCamera = g_pRenderEngine->loadIcon("res/icon_camera.png");
   g_idIconClock = g_pRenderEngine->loadIcon("res/icon_clock.png");
   g_idIconHeading = g_pRenderEngine->loadIcon("res/icon_heading.png");
   g_idIconThrotle = g_pRenderEngine->loadIcon("res/icon_gauge.png");
   g_idIconTemperature = g_pRenderEngine->loadIcon("res/icon_temp.png");
   g_idIconCPU = g_pRenderEngine->loadIcon("res/icon_cpu.png");
   g_idIconJoystick = g_pRenderEngine->loadIcon("res/icon_joystick.png");
   g_idIconShield = g_pRenderEngine->loadIcon("res/icon_shield.png");
   g_idIconUplink = g_pRenderEngine->loadIcon("res/icon_uplink.png");
   g_idIconUplink2 = g_pRenderEngine->loadIcon("res/icon_uplink2.png");
   g_idIconRadio = g_pRenderEngine->loadIcon("res/icon_radio.png");
   g_idIconWind = g_pRenderEngine->loadIcon("res/icon_wind.png");
   g_idIconController = g_pRenderEngine->loadIcon("res/icon_controller.png");
   g_idIconX = g_pRenderEngine->loadIcon("res/icon_x.png");

   osd_stats_init();
   return true;
}

void osd_set_transparency(int value)
{
   s_iOSDTransparency = value;
   switch ( value )
   {
      case 0: g_pRenderEngine->setGlobalAlfa(0.85); break;
      case 1: g_pRenderEngine->setGlobalAlfa(0.9); break;
      case 2: g_pRenderEngine->setGlobalAlfa(0.95); break;
      case 3: g_pRenderEngine->setGlobalAlfa(1.0); break;
   }
}


void osd_set_colors()
{
   double* pc1 = get_Color_OSDText();
   g_pRenderEngine->setColors(pc1);
   double* pc = get_Color_OSDTextOutline();
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], pc[3]);
   g_pRenderEngine->setStrokeSize(sfOSDOutlineThickness);
}

void osd_set_colors_alpha(float alpha)
{
   double* pC = get_Color_OSDText();
   double* pCO = get_Color_OSDTextOutline();

   g_pRenderEngine->setFill(pC[0], pC[1], pC[2], pC[3]*alpha);
   g_pRenderEngine->setStroke(pCO[0], pCO[1], pCO[2], pCO[3]*alpha);
   g_pRenderEngine->setStrokeSize(sfOSDOutlineThickness);
}


void osd_set_colors_background_fill()
{
   Preferences* p = get_Preferences();
   float alfa = 0.9;
   switch ( s_iOSDTransparency )
   {
      case 0: alfa = 0.0; break;
      case 1: alfa = 0.15; break;
      case 2: alfa = 0.3; break;
      case 3: alfa = 0.7; break;
   }

   if ( p->iInvertColorsOSD )
   {
   switch ( s_iOSDTransparency )
   {
      case 0: alfa = 0.5; break;
      case 1: alfa = 0.62; break;
      case 2: alfa = 0.67; break;
      case 3: alfa = 0.75; break;
   }
   }

   g_pRenderEngine->setColors(get_Color_OSDBackground(), alfa);
}

void osd_set_colors_background_fill(float fAlpha)
{
   g_pRenderEngine->setColors(get_Color_OSDBackground(), fAlpha);
}

float osd_course_to(double lat1, double long1, double lat2, double long2)
{
    double dlon = (long2-long1)*0.017453292519;
    lat1 = (lat1)*0.017453292519;
    lat2 = (lat2)*0.017453292519;
    double a1 = sin(dlon) * cos(lat2);
    double a2 = sin(lat1) * cos(lat2) * cos(dlon);
    a2 = cos(lat1) * sin(lat2) - a2;
    a2 = atan2(a1, a2);
    if (a2 < 0.0) a2 += 2.0*3.141592653589793;
    return a2*180.0/3.141592653589793;
    
    /*
    double dlon = long2 - long1;
    double x = cos(lat2)*sin(dlon);
    double y = cos(lat1)*sin(lat2) - cos(lat2)*sin(lat1)*cos(dlon);
    double a = atan2(x,y);
    if (a < 0.0) a += 2.0*3.141592653589793;
    return a*180.0/3.141592653589793;
    */
}

float osd_show_value(float x, float y, const char* szValue, u32 fontId)
{
   float height_text = g_pRenderEngine->textHeight(fontId);
   float w = 0;
   g_pRenderEngine->drawText(x,y, height_text, fontId, szValue);
   w += g_pRenderEngine->textWidth(height_text, fontId, szValue);
   return w;
}

float osd_show_value_sufix(float x, float y, const char* szValue, const char* szSufix, u32 fontId, u32 fontIdSufix )
{
   float height_text = g_pRenderEngine->textHeight( 0.0, fontId);
   float height_text_s = g_pRenderEngine->textHeight( 0.0, fontIdSufix);
   float w = 0;
   g_pRenderEngine->drawText(x,y, height_text, fontId, szValue);
   w += g_pRenderEngine->textWidth(height_text, fontId, szValue);
   if ( NULL != szSufix && 0 != szSufix[0] )
   {
      w += height_text_s*0.25;
      g_pRenderEngine->drawText(x+w,y+(height_text-height_text_s)*0.6, height_text_s, fontIdSufix, szSufix);
      w += g_pRenderEngine->textWidth(height_text_s, fontIdSufix, szSufix);
   }
   return w;
}

float osd_show_value_sufix_left(float x, float y, const char* szValue, const char* szSufix, u32 fontId, u32 fontIdSufix )
{
   float height_text = g_pRenderEngine->textHeight( 0.0, fontId);
   float height_text_s = g_pRenderEngine->textHeight( 0.0, fontIdSufix);

   float w = g_pRenderEngine->textWidth(height_text, fontId, szValue);
   float ws = g_pRenderEngine->textWidth(height_text_s, fontIdSufix, szSufix);

   g_pRenderEngine->drawText(x-ws,y+(height_text-height_text_s)*0.6, height_text_s, fontIdSufix, szSufix);
   x -= w + ws + height_text_s*0.25;
   g_pRenderEngine->drawText(x,y, height_text, fontId, szValue);
   return w + ws + height_text_s*0.25;
}

float osd_show_value_left(float x, float y, const char* szValue, u32 fontId)
{
   float height_text = g_pRenderEngine->textHeight( 0.0, fontId);
   float w = g_pRenderEngine->textWidth(height_text, fontId, szValue);
   g_pRenderEngine->drawText(x-w,y, height_text, fontId, szValue);
   return w;
}


float osd_show_value_centered(float x, float y, const char* szValue, u32 fontId)
{
   float height_text = g_pRenderEngine->textHeight( 0.0, fontId);

   float w = g_pRenderEngine->textWidth(height_text, fontId, szValue);

   g_pRenderEngine->drawText(x-0.5*w,y, height_text, fontId, szValue);
   
   return w;
}

u32 osd_getVehicleIcon(int vehicle_type)
{
   u32 idIcon = g_idIconRuby;
   if ( vehicle_type == MODEL_TYPE_DRONE )
      idIcon = g_idIconDrone;
   if ( vehicle_type == MODEL_TYPE_AIRPLANE )
      idIcon = g_idIconPlane;
   if ( vehicle_type == MODEL_TYPE_CAR )
      idIcon = g_idIconCar;
   return idIcon;
}


void osd_rotatePoints(float *x, float *y, float angle, int points, float center_x, float center_y, float fScale)
{
    double cosAngle = cos(angle * 0.017453292519);
    double sinAngle = sin(angle * 0.017453292519);

    float tmp_x = 0;
    float tmp_y = 0;
    for( int i=0; i<points; i++ )
    {
       tmp_x = center_x + (x[i]*cosAngle-y[i]*sinAngle)*fScale/g_pRenderEngine->getAspectRatio();
       tmp_y = center_y + (x[i]*sinAngle + y[i]*cosAngle)*fScale;
       x[i] = tmp_x;
       y[i] = tmp_y;
    }
}


void osd_rotate_point(float x, float y, float xCenter, float yCenter, float angle, float* px, float* py)
{
   if ( NULL == px || NULL == py )
      return;
   float s = sin(angle*3.14159/180.0);
   float c = cos(angle*3.14159/180.0);

   x = (x-xCenter)*g_pRenderEngine->getAspectRatio();
   y = (y-yCenter);
   *px = x * c - y * s;
   *py = x * s + y * c;

   *px /= g_pRenderEngine->getAspectRatio();
   *px += xCenter;
   *py += yCenter;
}

float osd_show_video_profile_mode(float xPos, float yPos, u32 uFontId, bool bLeft)
{
   if ( NULL == g_psmvds || NULL == g_pCurrentModel )
      return 0.0;
   
   float fWidth = 0;
   char szBuff[32];

   szBuff[0] = 0;
   
   strcpy(szBuff, str_get_video_profile_name(g_psmvds->video_link_profile & 0x0F));
   int diffEC = g_psmvds->fec_packets_per_block - g_pCurrentModel->video_link_profiles[g_psmvds->video_link_profile & 0x0F].block_fecs;
   if ( diffEC > 0 )
   {
      char szTmp[16];
      sprintf(szTmp, "-%d", diffEC);
      strcat(szBuff, szTmp);
   }
   else if ( g_psmvds->encoding_extra_flags & ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE )
      strcat(szBuff, "-");
   fWidth = g_pRenderEngine->textWidth(uFontId, szBuff);

   if ( bLeft )
      g_pRenderEngine->drawTextLeft(xPos, yPos, 0.0, uFontId, szBuff);
   else
      g_pRenderEngine->drawText(xPos, yPos, uFontId, szBuff);    
   return fWidth;
}

float osd_convertTemperature(float c)
{
   Preferences* p = get_Preferences();

   if ( p->iUnits == prefUnitsMetric || p->iUnits == prefUnitsMeters )
      return c;
   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
      return c*1.8 + 32.0;
   return c;
}

float osd_render_relay(float xCenter, float yBottom, bool bHorizontal)
{
   if ( NULL == g_pCurrentModel || ( g_pCurrentModel->is_spectator) )
      return 0.0;
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId <= 0 )
      return 0.0;

   const char* szTextMain = "Main Vehicle";
   const char* szTextRelay = "Relayed Vehicle";

   double* pc1 = get_Color_OSDText();
   g_pRenderEngine->setColors(pc1);
   g_pRenderEngine->setStroke(pc1[0], pc1[1], pc1[2], pc1[3]);
   g_pRenderEngine->setStrokeSize(sfOSDOutlineThickness);

   char szBuff[256];
   float height_text = osd_getFontHeight();
   float height_text_big = osd_getFontHeightBig();
   float height_text_small = osd_getFontHeightSmall();

   float fPaddingY = height_text_small*0.4;
   float fPaddingX = 2.0*fPaddingY/g_pRenderEngine->getAspectRatio();
   float fHeight = height_text_small*2.0 + height_text_small + 2.0*fPaddingY;
   float fWidth = 0.0;
   float yPos = yBottom-fHeight;
   
   Model *pModel = findModelWithId(g_pCurrentModel->relay_params.uRelayVehicleId);
   if ( NULL == pModel || ( pModel->vehicle_id == g_pCurrentModel->vehicle_id ) )
   {
      fHeight = height_text + 2.0*fPaddingY;
      yPos = yBottom - fHeight;
      fWidth = g_pRenderEngine->textWidth(g_idFontOSD, "RELAY: Invalid target relayed vehicle.");
      fWidth += 2.0*fPaddingX;
      g_pRenderEngine->setFill(0,0,0,0.2);
      g_pRenderEngine->drawRoundRect(xCenter-fWidth*0.5, yPos, fWidth, fHeight, 0.01);
      osd_set_colors();
      g_pRenderEngine->drawText(xCenter - fWidth*0.5 + fPaddingX, yPos + fPaddingY, g_idFontOSD, "RELAY: Invalid target relayed vehicle.");
      return fWidth;
   }


   g_pRenderEngine->setColors(pc1);
   g_pRenderEngine->setStroke(pc1[0], pc1[1], pc1[2], pc1[3]);
   g_pRenderEngine->setStrokeSize(sfOSDOutlineThickness);

   bool bRelayedVehicleIsOnline = link_is_relayed_vehicle_online();

   char szName1[128];
   char szName2[128];
   strncpy(szName1, g_pCurrentModel->getLongName(), 127);
   szName1[0] = toupper(szName1[0]);

   strncpy(szName2, pModel->getLongName(), 127);
   szName2[0] = toupper(szName2[0]);

   float fWidthName1 = g_pRenderEngine->textWidth(g_idFontOSDSmall, szName1);
   float fWidthName2 = g_pRenderEngine->textWidth(g_idFontOSDSmall, szName2);

   float fWidthTextMain = g_pRenderEngine->textWidth(g_idFontOSDSmall, szTextMain);
   float fWidthTextRelay = g_pRenderEngine->textWidth(g_idFontOSDSmall, szTextRelay);
   
   float fWidthLeft = fWidthName1;
   if ( fWidthTextMain > fWidthLeft )
      fWidthLeft = fWidthTextMain;

   float fWidthRight = fWidthName2;
   if ( fWidthTextRelay > fWidthRight )
      fWidthRight = fWidthTextRelay;

   float xLeft = xCenter - fWidthLeft - 2.0 * fPaddingX;
   float xRight = xCenter + fWidthRight + 2.0 * fPaddingX;
   fWidth = xRight - xLeft;

   g_pRenderEngine->setFill(0,0,0,0.1);
   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->drawRoundRect(xLeft, yPos, fWidth, fHeight, 0.01);
   g_pRenderEngine->drawLine(xCenter, yPos, xCenter, yBottom - g_pRenderEngine->getPixelHeight());


   bool bMainActive = true;
   if (( g_pCurrentModel->relay_params.uCurrentRelayMode == RELAY_MODE_REMOTE ||
        g_pCurrentModel->relay_params.uCurrentRelayMode == RELAY_MODE_PIP_REMOTE ) &&
       bRelayedVehicleIsOnline )
      bMainActive = false;

   if ( bMainActive )
   {
      g_pRenderEngine->setFill(0,0,0,0.5);
      g_pRenderEngine->drawRoundRect(xLeft, yPos, xCenter-xLeft, fHeight, 0.01);
   }
   else
   {
      g_pRenderEngine->setFill(0,0,0,0.5);
      g_pRenderEngine->drawRoundRect(xCenter, yPos, xRight-xCenter, fHeight, 0.01);
   }
 
   osd_set_colors();
   float fInactiveAlpha = 0.7;

   yPos += fPaddingY;
   if ( bMainActive )
   {
      g_pRenderEngine->drawText(xLeft + fPaddingX, yPos, g_idFontOSDSmall, szName1);
      float fa = g_pRenderEngine->setGlobalAlfa(fInactiveAlpha);
      osd_set_colors();
      g_pRenderEngine->drawText(xCenter + fPaddingX, yPos, g_idFontOSDSmall, szName2);
      g_pRenderEngine->setGlobalAlfa(fa);
      osd_set_colors();
   }
   else
   {
      float fa = g_pRenderEngine->setGlobalAlfa(fInactiveAlpha);
      osd_set_colors();
      g_pRenderEngine->drawText(xLeft + fPaddingX, yPos, g_idFontOSDSmall, szName1);
      g_pRenderEngine->setGlobalAlfa(fa);
      osd_set_colors();
      g_pRenderEngine->drawText(xCenter + fPaddingX, yPos, g_idFontOSDSmall, szName2);
   }

   yPos += height_text_small*1.2;

   if ( bMainActive )
   {
      g_pRenderEngine->drawText(xLeft + fPaddingX + 0.5*(fWidthLeft - fWidthTextMain), yPos, g_idFontOSDSmall, szTextMain);
      float fa = g_pRenderEngine->setGlobalAlfa(fInactiveAlpha);
      osd_set_colors();
      g_pRenderEngine->drawText(xCenter + fPaddingX + 0.5*(fWidthRight - fWidthTextRelay), yPos, g_idFontOSDSmall, szTextRelay);
      g_pRenderEngine->setGlobalAlfa(fa);
      osd_set_colors();
   }
   else
   {
      float fa = g_pRenderEngine->setGlobalAlfa(fInactiveAlpha);
      osd_set_colors();
      g_pRenderEngine->drawText(xLeft + fPaddingX + 0.5*(fWidthLeft - fWidthTextMain), yPos, g_idFontOSDSmall, szTextMain);
      g_pRenderEngine->setGlobalAlfa(fa);
      osd_set_colors();
      g_pRenderEngine->drawText(xCenter + fPaddingX + 0.5*(fWidthRight - fWidthTextRelay), yPos, g_idFontOSDSmall, szTextRelay);
   }

   yPos += height_text_small;

   float fRadius = height_text_small*0.34;
   if ( bRelayedVehicleIsOnline )
   {
      float fa = g_pRenderEngine->setGlobalAlfa(fInactiveAlpha);
      osd_set_colors();
      float fwt = g_pRenderEngine->textWidth(g_idFontOSDSmall, "Online");
      g_pRenderEngine->drawText(xCenter + fPaddingX + 0.5*(fWidthRight - fwt), yPos, g_idFontOSDSmall, "Online");
      double* pc = get_Color_IconSucces();
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], pc[3]);
      g_pRenderEngine->fillCircle(xCenter + fPaddingX + 0.5*(fWidthRight-fwt) - height_text_small/g_pRenderEngine->getAspectRatio(), yPos + height_text_small*0.5, fRadius);
   
      g_pRenderEngine->setGlobalAlfa(fa);
      osd_set_colors();
   }
   else
   {
      float fwt = g_pRenderEngine->textWidth(g_idFontOSDSmall, "Offline");
      g_pRenderEngine->drawText(xCenter + fPaddingX + 0.5*(fWidthRight - fwt), yPos, g_idFontOSDSmall, "Offline");
      double* pc = get_Color_IconError();
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], pc[3]);
      g_pRenderEngine->fillCircle(xCenter + fPaddingX + 0.5*(fWidthRight-fwt) - height_text_small/g_pRenderEngine->getAspectRatio(), yPos + height_text_small*0.5, fRadius);
   }

   /*
   
   
   if ( g_pCurrentModel->relay_params.uCurrentRelayMode == RELAY_MODE_REMOTE ||
        g_pCurrentModel->relay_params.uCurrentRelayMode == RELAY_MODE_PIP_REMOTE )
      sprintf(szBuff,"RELAYING: %s", szName);
   else
      sprintf(szBuff,"RELAY Off: %s", szName);

   fWidth += osd_show_value(xPos, yPos, szBuff, g_idFontOSD);
*/

   osd_set_colors();
   return fWidth;
}