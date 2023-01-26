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
#include "osd_common.h"
#include "osd_ahi.h"
#include "osd.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "shared_vars.h"
#include "link_watch.h"
#include <math.h>
#include "timers.h"

#define ALTITUDE_VALUES 48

static bool s_bDebugAHIShowAll = false;

double COLOR_OSD_AHI_TEXT_FG[4] = { 208,238,214, 1.0 };
double COLOR_OSD_AHI_TEXT_BORDER[4] = { 0,0,0, 0.4 };
double COLOR_OSD_AHI_LINE_FG[4] = { 208,238,214, 1.0 };
double COLOR_OSD_AHI_LINE_FG_WARNING[4] = { 200,0,0, 1.0 };
double COLOR_OSD_AHI_LINE_BORDER[4] = { 0,0,0, 0.4 };
double COLOR_OSD_AHI_PANEL[4] = { 0,0,0, 0.6 };

float ahi_fScale = 1.0; // global scale of AHI
float ahi_fBarAspect = 4.5; // aspect ratio of height/width of left/right AHI bars
float ahi_GradationAspect = 0.04; // percent of AHI total width
float ahi_smallGradationAspect = 0.015; // percent of AHI total width


float ahi_fMaxWidth = 0, ahi_fMaxHeight = 0;
float ahi_xs;
float ahi_xe;
float ahi_ys;
float ahi_ye;

float s_LineThickness = 1.0;
float s_LineThicknessPx = 1.0;
float s_ShadowThicknessPx = 1.0;


float s_ahi_lastGSpeed = 0.0;
float s_ahi_lastASpeed = 0.0;
float s_ahi_lastAltitude = 0.0;

float s_ahi_lastPitch = 0.0;
float s_ahi_lastRoll = 0.0;

float s_ahi_altitude_speed_history[ALTITUDE_VALUES];
int s_ahi_altitude_speed_values = 0;
u32 s_ahi_altitude_speed_last_time = 0;


void osd_set_ahi_scale(float fScale)
{
   ahi_fScale = fScale;
}

void osd_ahi_set_color(double* pC, float strokePx, float fAlpha=1.0)
{
   g_pRenderEngine->setFill(pC[0], pC[1], pC[2], pC[3]*g_pRenderEngine->getGlobalAlfa()*fAlpha);
   g_pRenderEngine->setStroke(pC[0], pC[1], pC[2], pC[3]*g_pRenderEngine->getGlobalAlfa()*fAlpha);
   g_pRenderEngine->setStrokeSize(strokePx);
}


void osd_ahi_show_plane(float roll, float pitch)
{
   float xCenter = 0.5;
   float yCenter = 0.5;
   float planeWidth = 0.03*ahi_fScale;

   osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, s_LineThicknessPx + 2*s_ShadowThicknessPx);
   g_pRenderEngine->drawLine(xCenter-planeWidth, yCenter, xCenter-planeWidth*0.25, yCenter);
   g_pRenderEngine->drawLine(xCenter-planeWidth*0.25, yCenter, xCenter, yCenter+planeWidth*0.1);
   g_pRenderEngine->drawLine(xCenter, yCenter+planeWidth*0.1, xCenter+planeWidth*0.25, yCenter);
   g_pRenderEngine->drawLine(xCenter+planeWidth*0.25, yCenter, xCenter+planeWidth, yCenter);

   osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->osd_params.ahi_warning_angle > 1 )
   if ( fabs(roll) >= g_pCurrentModel->osd_params.ahi_warning_angle ||
        fabs(pitch) >= g_pCurrentModel->osd_params.ahi_warning_angle )
      osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG_WARNING, s_LineThicknessPx);

   g_pRenderEngine->drawLine(xCenter-planeWidth, yCenter, xCenter-planeWidth*0.25, yCenter);
   g_pRenderEngine->drawLine(xCenter-planeWidth*0.25, yCenter, xCenter, yCenter+planeWidth*0.1);
   g_pRenderEngine->drawLine(xCenter, yCenter+planeWidth*0.1, xCenter+planeWidth*0.25, yCenter);
   g_pRenderEngine->drawLine(xCenter+planeWidth*0.25, yCenter, xCenter+planeWidth, yCenter);
}

void osd_show_ahi_heading(float yTop, float fWidth)
{
   int heading = 0;
   if ( NULL != g_pdpfct && link_has_fc_telemetry() )
      heading = g_pdpfct->heading;
   
   //static int heading = 0;
   //heading++;

   if ( heading > 360 )
      heading -= 360;
   if ( heading < 0 )
      heading += 360;

   char szBuff[64];
   int headingValues = 100;

   float fScaleText = 0.65;
   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   float lineHeight = 0.016*ahi_fScale;
   float yBottom = yTop + lineHeight*2.0 + height_text;

   u32 idFontValues = g_idFontOSD;
   if ( fWidth < 0.3 )
      idFontValues = g_idFontOSDSmall;

   for( int i=heading-headingValues/2; i<=heading+headingValues/2; i++ )
   {
      int hdng = i;
      if ( hdng < 0 )
         hdng += 360;

      float xPos = 0.5 + (i - heading) * fWidth/headingValues;
      if ( (i%5) == 0 )
      {         
         float scale = 1.0;
         szBuff[0] = 0; szBuff[1] = 0;
         if ( hdng == 0 || hdng == 360 )
            strcpy(szBuff, "N");
         if ( hdng == 90 )
            strcpy(szBuff, "E");
         if ( hdng == 180 )
            strcpy(szBuff, "S");
         if ( hdng == 270 )
            strcpy(szBuff, "W");

         if ( hdng == 45 )
            strcpy(szBuff, "NE");
         if ( hdng == 135 )
            strcpy(szBuff, "SE");
         if ( hdng == 225 )
            strcpy(szBuff, "SW");
         if ( hdng == 315 )
            strcpy(szBuff, "NW");

         if ( szBuff[1] != 0 )
            scale = 1.1;
    
         else if ( szBuff[0] != 0 )
            scale = 1.2;

         if ( 0 != szBuff[0] )
         {
            float wText = g_pRenderEngine->textWidth( height_text*scale, g_idFontOSD, const_cast<char*>(szBuff));
            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
            osd_show_value(xPos-wText*0.5, yBottom-lineHeight*2.2-height_text*1.5, szBuff, g_idFontOSD);
            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
            osd_show_value(xPos-wText*0.5, yBottom-lineHeight*2.2-height_text*1.5, szBuff, g_idFontOSD);

            osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx);
            g_pRenderEngine->drawRoundRect(xPos-s_LineThickness*0.5, yBottom-lineHeight*1.6-s_LineThickness*0.5, s_LineThickness, lineHeight*1.6+2*s_LineThickness, s_LineThickness*0.2 );
            osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, 0.0);
            g_pRenderEngine->drawRoundRect(xPos-s_LineThickness*0.5, yBottom-lineHeight*1.6-s_LineThickness*0.5, s_LineThickness, lineHeight*1.6+2*s_LineThickness, s_LineThickness*0.2 );
         }
         if ( 0 == szBuff[0] && ((i%10) == 0))
         {
            sprintf(szBuff, "%d", hdng);
            float wText = g_pRenderEngine->textWidth( height_text*0.9, g_idFontOSD, const_cast<char*>(szBuff));
            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
            osd_show_value(xPos-wText*0.5, yBottom-lineHeight*1.3-height_text, szBuff, idFontValues);
            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
            osd_show_value(xPos-wText*0.5, yBottom-lineHeight*1.3-height_text, szBuff, idFontValues);

            osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx);
            g_pRenderEngine->drawRoundRect(xPos-s_LineThickness*0.5, yBottom-lineHeight*0.8-s_LineThickness*0.5, s_LineThickness, lineHeight*0.8+2*s_LineThickness, s_LineThickness*0.2 );
            osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, 0.0);
            g_pRenderEngine->drawRoundRect(xPos-s_LineThickness*0.5, yBottom-lineHeight*0.8-s_LineThickness*0.5, s_LineThickness, lineHeight*0.8+2*s_LineThickness, s_LineThickness*0.2 );
         }
      }
      else if ( i%2 )
      {
         //osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, s_StrokeWidthLine + s_ShadowThickness);
         //Line( xPos, yTop, xPos, yTop + 3 );
         //osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_StrokeWidthLine);   
         //Line( xPos, yTop, xPos, yTop + 3 );
      }
   }

   osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx);
   g_pRenderEngine->drawRoundRect(0.5-s_LineThickness*0.5, yBottom+lineHeight*0.5-0.5*s_LineThickness, s_LineThickness, lineHeight*0.5+2*s_LineThickness, s_LineThickness*0.2 );
   osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, 0.0);
   g_pRenderEngine->drawRoundRect(0.5-s_LineThickness*0.5, yBottom+lineHeight*0.5-0.5*s_LineThickness, s_LineThickness, lineHeight*0.5+2*s_LineThickness, s_LineThickness*0.2 );

   sprintf(szBuff, "%03d", heading);
   float wText = 1.4*g_pRenderEngine->textWidth( height_text*fScaleText*1.3, g_idFontOSD, const_cast<char*>(szBuff));
   osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
   osd_show_value(0.5-wText*0.5, yBottom+lineHeight*1.6, szBuff, g_idFontOSD);
   osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
   osd_show_value(0.5-wText*0.5, yBottom+lineHeight*1.6, szBuff, g_idFontOSD);
}

void osd_ahi_show_horizont_ladder(float roll, float pitch)
{
   bool bUseWarningColor = false;

   if ( g_pCurrentModel->osd_params.ahi_warning_angle > 1 )
   if ( fabs(roll) >= g_pCurrentModel->osd_params.ahi_warning_angle ||
        fabs(pitch) >= g_pCurrentModel->osd_params.ahi_warning_angle )
      bUseWarningColor = true;

   char szBuff[64];
   float ahiAlpha = 1.0;   

   float xCenter = 0.5;
   float yCenter = 0.5;
   float planeWidth = 0.03*ahi_fScale;

   float xt[3] = {(float)(xCenter-planeWidth*0.2), (float)(xCenter+planeWidth*0.2), xCenter };
   float yt[3] = {(float)(yCenter+planeWidth), (float)(yCenter+planeWidth), (float)(yCenter+planeWidth-planeWidth*0.4)};

   osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx);
   g_pRenderEngine->drawPolyLine(xt, yt, 3);    
   g_pRenderEngine->fillPolygon(xt, yt, 3);

   osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, 0.0);
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->osd_params.ahi_warning_angle > 1 )
   if ( fabs(roll) > g_pCurrentModel->osd_params.ahi_warning_angle )
      osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG_WARNING, 0.0);
   g_pRenderEngine->drawPolyLine(xt, yt, 3);   
   g_pRenderEngine->fillPolygon(xt, yt, 3);

   float fFontScale = 0.8;
   float height_text = osd_getFontHeight();

   float height_ladder = 0.32 * ahi_fScale;
   float width_ladder = 0.09 * ahi_fScale;
   float range = 30;
   float space_text = 0.006 * ahi_fScale;
   float ratio = height_ladder / range;
    
   sprintf(szBuff, "%d", (int)roll);
   float x2 = xCenter-height_text*0.5*0.8;
   if ( roll < 0 )
      x2 -= height_text*0.3*0.8;
   if ( roll <= -10 || roll >= 10 )
      x2 -= height_text*0.3*0.8;

   osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
   osd_show_value_centered(xCenter,yCenter+1.4*height_text, szBuff, g_idFontOSD);
   osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
   osd_show_value_centered(xCenter,yCenter+1.4*height_text, szBuff, g_idFontOSD);

   roll = -roll;

   int angle = pitch - range/2;
   int max = pitch + range/2;
   while (angle <= max)
   {
      float y = yCenter - (angle - pitch) * ratio;
      if (angle == 0)
      {
        sprintf(szBuff, "0");
        osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
        float xl = xCenter - width_ladder / 1.25f - space_text;
        float xr = xCenter + width_ladder / 1.25f + space_text;
        float xt, yt;
        osd_rotate_point(xl,y,xCenter, yCenter, roll, &xt, &yt);
        g_pRenderEngine->drawTextLeft(xt, yt - height_text*0.5, height_text, g_idFontOSD, const_cast<char*>(szBuff));
        osd_rotate_point(xr,y,xCenter, yCenter, roll, &xt, &yt);
        g_pRenderEngine->drawText(xt, yt - height_text*0.5, height_text, g_idFontOSD, const_cast<char*>(szBuff));
        osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
        osd_rotate_point(xl,y,xCenter, yCenter, roll, &xt, &yt);
        g_pRenderEngine->drawTextLeft(xt, yt - height_text*0.5, height_text, g_idFontOSD, const_cast<char*>(szBuff));
        osd_rotate_point(xr,y,xCenter, yCenter, roll, &xt, &yt);
        g_pRenderEngine->drawText(xt, yt - height_text*0.5, height_text, g_idFontOSD, const_cast<char*>(szBuff));

        xl = xCenter - width_ladder / 1.25f;
        xr = xCenter + width_ladder / 1.25f;
        float xtl, ytl;
        float xtr, ytr;

        osd_rotate_point(xl,y,xCenter, yCenter, roll, &xtl, &ytl);
        osd_rotate_point(xr,y,xCenter, yCenter, roll, &xtr, &ytr);
        osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, 0.0);
        g_pRenderEngine->drawLine(xtl, ytl, xtr, ytr);

        //osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, 0.0);
        //osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx);
        //g_pRenderEngine->drawRoundRect(xCenter - width_ladder / 1.25f, y-s_LineThickness*0.5, 2*(width_ladder /1.25f), s_LineThickness, s_LineThickness*0.2);
        //osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, 0.0);
        //g_pRenderEngine->drawRoundRect(xCenter - width_ladder / 1.25f, y-s_LineThickness*0.5, 2*(width_ladder /1.25f), s_LineThickness, s_LineThickness*0.2);
        angle++;
        continue;
      }
      if ( (angle%5) != 0 )
      {
         angle++;
         continue;
      }

      float xl = xCenter - width_ladder *0.5 - space_text;
      float xr = xCenter + width_ladder *0.5 + space_text;
      float xtl, ytl;
      float xtr, ytr;
      osd_rotate_point(xl,y,xCenter, yCenter, roll, &xtl, &ytl);
      osd_rotate_point(xr,y,xCenter, yCenter, roll, &xtr, &ytr);

      sprintf(szBuff, "%d", angle);

      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      g_pRenderEngine->drawTextLeft(xtl, ytl-0.5*height_text, height_text, g_idFontOSD, const_cast<char*>(szBuff));
      g_pRenderEngine->drawText(xtr, ytr-0.5*height_text, height_text, g_idFontOSD, const_cast<char*>(szBuff));

      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      if ( NULL != g_pCurrentModel )
      if ( g_pCurrentModel->osd_params.ahi_warning_angle > 1 )
      if ( fabs(angle) >= g_pCurrentModel->osd_params.ahi_warning_angle )
         osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG_WARNING, 0.0);

      g_pRenderEngine->drawTextLeft(xtl, ytl-0.5*height_text, height_text, g_idFontOSD, const_cast<char*>(szBuff));
      g_pRenderEngine->drawText(xtr, ytr-0.5*height_text, height_text, g_idFontOSD, const_cast<char*>(szBuff));

      xl = xCenter - width_ladder *0.5;
      xr = xCenter + width_ladder *0.5;
      float xl2 = xCenter - width_ladder *0.5 + width_ladder*0.2;
      float xr2 = xCenter + width_ladder *0.5 - width_ladder*0.2;
      float xtl2, ytl2;
      float xtr2, ytr2;
      osd_rotate_point(xl,y,xCenter, yCenter, roll, &xtl, &ytl);
      osd_rotate_point(xr,y,xCenter, yCenter, roll, &xtr, &ytr);

      osd_rotate_point(xl2,y,xCenter, yCenter, roll, &xtl2, &ytl2);
      osd_rotate_point(xr2,y,xCenter, yCenter, roll, &xtr2, &ytr2);

      if (angle > 0)
      {
        //osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx);
        //g_pRenderEngine->drawRoundRect(xCenter - width_ladder*0.5, y-s_LineThickness*0.5, width_ladder*0.2, s_LineThickness, s_LineThickness*0.2);
        //g_pRenderEngine->drawRoundRect(xCenter + width_ladder*0.3, y-s_LineThickness*0.5, width_ladder*0.2, s_LineThickness, s_LineThickness*0.2);

        osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, 0.0);
        if ( NULL != g_pCurrentModel )
        if ( g_pCurrentModel->osd_params.ahi_warning_angle > 1 )
        if ( fabs(angle) >= g_pCurrentModel->osd_params.ahi_warning_angle )
           osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG_WARNING, 0.0);
        g_pRenderEngine->drawLine(xtl, ytl, xtl2, ytl2);
        g_pRenderEngine->drawLine(xtr2, ytr2, xtr, ytr);
        //g_pRenderEngine->drawRoundRect(xCenter - width_ladder*0.5, y-s_LineThickness*0.5, width_ladder*0.2, s_LineThickness, s_LineThickness*0.2);
        //g_pRenderEngine->drawRoundRect(xCenter + width_ladder*0.3, y-s_LineThickness*0.5, width_ladder*0.2, s_LineThickness, s_LineThickness*0.2);
      }
      if (angle < 0)
      {
        //osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx);
        //g_pRenderEngine->drawRoundRect(xCenter - width_ladder*0.5, y-s_LineThickness*0.5, width_ladder*0.2, s_LineThickness, s_LineThickness*0.2 );
        //g_pRenderEngine->drawRoundRect(xCenter + width_ladder*0.3, y-s_LineThickness*0.5, width_ladder*0.2, s_LineThickness, s_LineThickness*0.2 );
        osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, 0.0);
        if ( NULL != g_pCurrentModel )
        if ( g_pCurrentModel->osd_params.ahi_warning_angle > 1 )
        if ( fabs(angle) >= g_pCurrentModel->osd_params.ahi_warning_angle )
           osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG_WARNING, 0.0);
        //g_pRenderEngine->drawRoundRect(xCenter - width_ladder*0.5, y-s_LineThickness*0.5, width_ladder*0.2, s_LineThickness, s_LineThickness*0.2 );
        //g_pRenderEngine->drawRoundRect(xCenter + width_ladder*0.3, y-s_LineThickness*0.5, width_ladder*0.2, s_LineThickness, s_LineThickness*0.2 );
        g_pRenderEngine->drawLine(xtl, ytl, xtl2, ytl2);
        g_pRenderEngine->drawLine(xtr2, ytr2, xtr, ytr);
      }
      angle++;
   }

   osd_set_colors();
}

void osd_ahi_draw_panels()
{
   float wBar = ahi_fMaxHeight/ahi_fBarAspect/g_pRenderEngine->getAspectRatio();
   float hBar = 0.6*ahi_fMaxHeight/ahi_fBarAspect;
   if ( hBar < 1.46*osd_getFontHeightBig() )
      hBar = 1.46*osd_getFontHeightBig();
   float xS = ahi_xs;
   float xE = ahi_xe;
   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      xS = osd_getMarginX()+5.0/g_pRenderEngine->getScreenWidth();
      xE = 1.0-osd_getMarginX()-5.0/g_pRenderEngine->getScreenWidth();
      if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      {
         xS += osd_getVerticalBarWidth();
         xE -= osd_getVerticalBarWidth();
      }
   }

   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      g_pRenderEngine->drawLine(xS, ahi_ys, xS, ahi_ye);
      g_pRenderEngine->drawLine(xE, ahi_ys, xE, ahi_ye);
   }
   else
   {
      g_pRenderEngine->drawLine(xS+wBar, ahi_ys, xS+wBar, ahi_ye);
      g_pRenderEngine->drawLine(xE-wBar, ahi_ys, xE-wBar, ahi_ye);
   }

   g_pRenderEngine->drawLine(xS, ahi_ys, xS+wBar, ahi_ys);
   g_pRenderEngine->drawLine(xS, ahi_ye, xS+wBar, ahi_ye);

   g_pRenderEngine->drawLine(xE, ahi_ys, xE-wBar, ahi_ys);
   g_pRenderEngine->drawLine(xE, ahi_ye, xE-wBar, ahi_ye);

   float len = ahi_fMaxWidth * ahi_GradationAspect;
   float lenSmall = ahi_fMaxWidth * ahi_smallGradationAspect;
 
   float xLeft = ahi_xs+wBar-len;
   float xRight = ahi_xe-wBar+len;
   float yMid = (ahi_ys+ahi_ye)/2.0;
   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      xLeft = xS+len;
      xRight = xE-len;
      g_pRenderEngine->drawLine(xLeft, yMid, xLeft-len, yMid);
      g_pRenderEngine->drawLine(xRight, yMid, xRight+len, yMid);
   }
   else
   {
      g_pRenderEngine->drawLine(xLeft, yMid, xLeft+len, yMid);
      g_pRenderEngine->drawLine(xRight, yMid, xRight-len, yMid);
   }

   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      float tmp = xLeft;
      xLeft = xRight;
      xRight = tmp;
   }
   float xl[8] = {xLeft, (float)(xLeft-0.3*len), (float)(xLeft-0.3*len), (float)(xLeft-wBar*2.4), (float)(xLeft-wBar*2.4), (float)(xLeft-0.3*len), (float)(xLeft-0.3*len), xLeft};
   float yl[8] = {yMid, (float)(yMid+0.3*len), (float)(yMid+hBar*0.5), (float)(yMid+hBar*0.5), (float)(yMid-hBar*0.5), (float)(yMid-hBar*0.5), (float)(yMid-0.3*len), yMid};
   g_pRenderEngine->drawPolyLine(xl, yl, 8);

   float xr[8] = {xRight, (float)(xRight+0.3*len), (float)(xRight+0.3*len), (float)(xRight+wBar*2.4), (float)(xRight+wBar*2.4), (float)(xRight+0.3*len), (float)(xRight+0.3*len), xRight};
   float yr[8] = {yMid, (float)(yMid+0.3*len), (float)(yMid+hBar*0.5), (float)(yMid+hBar*0.5), (float)(yMid-hBar*0.5), (float)(yMid-hBar*0.5), (float)(yMid-0.3*len), yMid};
   g_pRenderEngine->drawPolyLine(xr, yr, 8);
}

void osd_ahi_detailed_show_main_panels_info(float roll, float pitch)
{
   char szBuff[1024];
   Preferences* p = get_Preferences();

   float wBar = ahi_fMaxHeight/ahi_fBarAspect/g_pRenderEngine->getAspectRatio();
   float hBar = 0.6*ahi_fMaxHeight/ahi_fBarAspect;
   float len = ahi_fMaxWidth * ahi_GradationAspect;
   float lenSmall = ahi_fMaxWidth * ahi_smallGradationAspect;
   float xLeft = ahi_xs+wBar-len;
   float xRight = ahi_xe-wBar+len;
   float yMid = (ahi_ys+ahi_ye)/2.0;

   if ( hBar < 1.46*osd_getFontHeightBig() )
      hBar = 1.46*osd_getFontHeightBig();

   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      xLeft = osd_getMarginX()+5.0/g_pRenderEngine->getScreenWidth();
      xRight = 1.0-osd_getMarginX()-5.0/g_pRenderEngine->getScreenWidth();
      if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      {
         xLeft += osd_getVerticalBarWidth();
         xRight -= osd_getVerticalBarWidth();
      }
   }

   float height_text = osd_getFontHeight() * ahi_fScale;
   
   strcpy(szBuff,"0");
   float fUISpeed = 0.0;
   if ( g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_AIR_SPEED_MAIN )
   {
      float fSpeed = 0.0;
      u32 tmp32 = 0;
      if ( NULL != g_pdpfct )
      {
        tmp32 = g_pdpfct->aspeed;
        fSpeed = (tmp32/100.0f-1000.0)*3.6f; // m/s->km/h
        fSpeed = _osd_convertKm(fSpeed);     
        s_ahi_lastASpeed = s_ahi_lastASpeed * 0.6 + 0.4 * fSpeed;
        fUISpeed = s_ahi_lastASpeed;
        sprintf(szBuff, "%.1f", s_ahi_lastASpeed);
        if ( 0 == tmp32 )
        {
           strcpy(szBuff, "0");
           fUISpeed = 0.0;
        }
      }
   }
   else if ( NULL != g_pCurrentModel && link_has_fc_telemetry() && NULL != g_pdpfct && 0 != g_pdpfct->hspeed)
   {
      float fSpeed = g_pdpfct->hspeed/100.0f-1000.0f;
      if ( p->iUnits == prefUnitsMetric || p->iUnits == prefUnitsImperial )
      {
         fSpeed = fSpeed * 3.6f; // m/s->km/h
         fSpeed = _osd_convertKm(fSpeed);
      }
      else
         fSpeed = _osd_convertMeters(fSpeed);
      s_ahi_lastGSpeed = s_ahi_lastGSpeed * 0.6 + 0.4 * fSpeed;
      fUISpeed = s_ahi_lastGSpeed;
      sprintf(szBuff, "%.1f", s_ahi_lastGSpeed);
      if ( 0 == g_pdpfct->hspeed )
      {
         strcpy(szBuff, "0");
         fUISpeed = 0.0;
      }
   }
 
   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      osd_show_value(xLeft+2*len,yMid-height_text*0.86, szBuff, g_idFontOSDBig);
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      osd_show_value(xLeft+2*len,yMid-height_text*0.86, szBuff, g_idFontOSDBig);
   }
   else
   {
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      osd_show_value_left(xLeft-len,yMid-height_text*0.86, szBuff, g_idFontOSDBig);
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      osd_show_value_left(xLeft-len,yMid-height_text*0.86, szBuff, g_idFontOSDBig);
   }

   if ( NULL != g_pCurrentModel && link_has_fc_telemetry() && NULL != g_pdpfct && 0 != g_pdpfct->altitude_abs )
   {
      float fAltitude = g_pdpfct->altitude_abs/100.0f-1000.0;
      if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.altitude_relative && 0 != g_pdpfct->altitude )
         fAltitude = g_pdpfct->altitude/100.0f-1000.0;

      fAltitude = _osd_convertMeters(fAltitude);

      s_ahi_lastAltitude = s_ahi_lastAltitude * 0.6 + 0.4 * fAltitude;
      if ( fabs(s_ahi_lastAltitude) < 1000.0 )
      {
         if ( fabs(s_ahi_lastAltitude) < 0.1 )
           s_ahi_lastAltitude = 0.0;
         sprintf(szBuff, "%.1f", s_ahi_lastAltitude);
      }
      else
         sprintf(szBuff, "%d", (int)s_ahi_lastAltitude);
   }
   else
      sprintf(szBuff, "---");
    
   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      osd_show_value_left(xRight-2*len,yMid-height_text*0.86, szBuff, g_idFontOSDBig);
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      osd_show_value_left(xRight-2*len,yMid-height_text*0.86, szBuff, g_idFontOSDBig);
   }
   else
   {
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      osd_show_value(xRight+len,yMid-height_text*0.86, szBuff, g_idFontOSDBig);
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      osd_show_value(xRight+len,yMid-height_text*0.86, szBuff, g_idFontOSDBig);
   }
   
   float fScaleText = 0.88;
 
   char szSpeed[32];
   int dxSpeed = 0.0;
   strcpy(szSpeed, "SPD");
   if ( g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_AIR_SPEED_MAIN )
   {
      strcpy(szSpeed, "SPD(A)");
      dxSpeed = 0.03 * ahi_fScale;
   }
   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      osd_show_value(xLeft+wBar*1.8+lenSmall-dxSpeed,yMid+height_text*0.01, szSpeed, g_idFontOSD);
      osd_show_value_left(xRight-wBar*1.9-lenSmall,yMid+height_text*0.01, "ALT", g_idFontOSD);
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      osd_show_value(xLeft+wBar*1.8+lenSmall-dxSpeed,yMid+height_text*0.01, szSpeed, g_idFontOSD);
      osd_show_value_left(xRight-wBar*1.9-lenSmall,yMid+height_text*0.01, "ALT", g_idFontOSD);
   }
   else
   {
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      osd_show_value(xLeft-wBar*2.4+lenSmall,yMid+height_text*0.01, szSpeed, g_idFontOSD);
      osd_show_value_left(xRight+wBar*2.4-lenSmall,yMid+height_text*0.01, "ALT", g_idFontOSD);
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      osd_show_value(xLeft-wBar*2.4+lenSmall,yMid+height_text*0.01, szSpeed, g_idFontOSD);
      osd_show_value_left(xRight+wBar*2.4-lenSmall,yMid+height_text*0.01, "ALT", g_idFontOSD);
   }

   fScaleText = 0.6;
   
   char szSpeedUnits[32];
   char szAltUnits[32];
   sprintf(szSpeedUnits, "(km/h)");
   if ( p->iUnits == prefUnitsMeters )
      sprintf(szSpeedUnits, "(m/s)");
   sprintf(szAltUnits, "(m)");
   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
   {
      sprintf(szSpeedUnits, "(mi/h)");
      if ( p->iUnits == prefUnitsFeets )
         sprintf(szSpeedUnits, "(ft/s)");
      sprintf(szAltUnits, "(ft)");
   }

   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      osd_show_value(xLeft+wBar*1.8+lenSmall*1.05,yMid-0.5*hBar+height_text*0.2, szSpeedUnits, g_idFontOSDSmall);
      osd_show_value_left(xRight-wBar*1.9-lenSmall*1.2,yMid-0.5*hBar+height_text*0.2, szAltUnits, g_idFontOSDSmall);
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      osd_show_value(xLeft+wBar*1.8+lenSmall*1.05,yMid-0.5*hBar+height_text*0.2, szSpeedUnits, g_idFontOSDSmall);
      osd_show_value_left(xRight-wBar*1.9-lenSmall*1.2,yMid-0.5*hBar+height_text*0.2, szAltUnits, g_idFontOSDSmall);
   }
   else
   {
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      osd_show_value(xLeft-wBar*2.4+lenSmall*1.05,yMid-0.5*hBar+height_text*0.2, szSpeedUnits, g_idFontOSDSmall);
      osd_show_value_left(xRight+wBar*2.4-lenSmall*1.2,yMid-0.5*hBar+height_text*0.2, szAltUnits, g_idFontOSDSmall);
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      osd_show_value(xLeft-wBar*2.4+lenSmall*1.05,yMid-0.5*hBar+height_text*0.2, szSpeedUnits, g_idFontOSDSmall);
      osd_show_value_left(xRight+wBar*2.4-lenSmall*1.2,yMid-0.5*hBar+height_text*0.2, szAltUnits, g_idFontOSDSmall);
   }

   fScaleText = 0.7;

   float dyStep = ahi_fMaxHeight / 20.0;
   int iValueStep = 2;
   int iValue = (int) (s_ahi_lastAltitude+20*iValueStep);
   iValue = iValue + (iValue%iValueStep);
   while ( iValue > s_ahi_lastAltitude-20*iValueStep )
   {
      float y = yMid - dyStep*(((float)iValue)-s_ahi_lastAltitude)/iValueStep;
      if ( y >= ahi_ye )
      {
         iValue -= iValueStep;
         continue;
      }
      if ( y <= ahi_ys )
      {
         iValue -= iValueStep;
         continue;
      }

      if ( ( iValue % 10 ) == 0 )
      {
         osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xRight-lenSmall*0.3, y, xRight-lenSmall*0.3-len, y);
         else
            g_pRenderEngine->drawLine(ahi_xe-wBar+lenSmall*0.3, y, ahi_xe-wBar+len+lenSmall*0.3, y);

         osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xRight-lenSmall*0.3, y, xRight-lenSmall*0.3-len, y);
         else
            g_pRenderEngine->drawLine(ahi_xe-wBar+lenSmall*0.3, y, ahi_xe-wBar+len+lenSmall*0.3, y);

         sprintf(szBuff, "%d", iValue);
         if ( ( y > yMid + hBar*0.5 + height_text*0.6 ||
              y < yMid - hBar*0.5 - height_text*0.6 ) &&
              y < ahi_ye - height_text*0.6 &&
              y > ahi_ys + height_text*0.6 )
         {
            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
            if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
               osd_show_value_left(xRight-lenSmall*0.8-len, y-height_text*0.45, szBuff, g_idFontOSD);
            else
               osd_show_value(ahi_xe-wBar+len+lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);

            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
            if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
               osd_show_value_left(xRight-lenSmall*0.8-len, y-height_text*0.45, szBuff, g_idFontOSD);
            else
               osd_show_value(ahi_xe-wBar+len+lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);
         }
      }
      else
      {
         osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xRight-lenSmall*0.3, y, xRight-lenSmall*0.3-lenSmall, y);
         else
            g_pRenderEngine->drawLine(ahi_xe-wBar+lenSmall*0.3, y, ahi_xe-wBar+lenSmall+lenSmall*0.3, y);

         osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xRight-lenSmall*0.3, y, xRight-lenSmall*0.3-lenSmall, y);
         else
            g_pRenderEngine->drawLine(ahi_xe-wBar+lenSmall*0.3, y, ahi_xe-wBar+lenSmall+lenSmall*0.3, y);
      }

      if ( ( iValue % 10 ) == 0 )
      {
         osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xRight-lenSmall*0.3, y, xRight-lenSmall*0.3-len, y);
         else
            g_pRenderEngine->drawLine(ahi_xe-wBar+lenSmall*0.3, y, ahi_xe-wBar+len+lenSmall*0.3, y);

         osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xRight-lenSmall*0.3, y, xRight-lenSmall*0.3-len, y);
         else
            g_pRenderEngine->drawLine(ahi_xe-wBar+lenSmall*0.3, y, ahi_xe-wBar+len+lenSmall*0.3, y);

         sprintf(szBuff, "%d", iValue);
         if ( ( y > yMid + hBar*0.5 + height_text*0.6 ||
              y < yMid - hBar*0.5 - height_text*0.6 ) &&
              y < ahi_ye - height_text*0.6 &&
              y > ahi_ys + height_text*0.6 )
         {
            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
            if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
               osd_show_value_left(xRight-lenSmall*0.8-len, y-height_text*0.45, szBuff, g_idFontOSD);
            else
               osd_show_value(ahi_xe-wBar+len+lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);

            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
            if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
               osd_show_value_left(xRight-lenSmall*0.8-len, y-height_text*0.45, szBuff, g_idFontOSD);
            else
               osd_show_value(ahi_xe-wBar+len+lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);
         }
      }
      else
      {
         osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xRight-lenSmall*0.3, y, xRight-lenSmall*0.3-lenSmall, y);
         else
            g_pRenderEngine->drawLine(ahi_xe-wBar+lenSmall*0.3, y, ahi_xe-wBar+lenSmall+lenSmall*0.3, y);

         osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xRight-lenSmall*0.3, y, xRight-lenSmall*0.3-lenSmall, y);
         else
            g_pRenderEngine->drawLine(ahi_xe-wBar+lenSmall*0.3, y, ahi_xe-wBar+lenSmall+lenSmall*0.3, y);
      }

      iValue -= iValueStep;
   }

   iValueStep = 2;
   iValue = (int) (fUISpeed+20*iValueStep);
   iValue = iValue + (iValue%iValueStep);
   while ( iValue > fUISpeed-20*iValueStep )
   {
      if ( iValue < 0 )
         break;

      float y = yMid - dyStep*(((float)iValue)-fUISpeed)/iValueStep;
      if ( y >= ahi_ye )
      {
         iValue -= iValueStep;
         continue;
      }
      if ( y <= ahi_ys )
      {
         iValue -= iValueStep;
         continue;
      }

      if ( ( iValue % 10 ) == 0 )
      {
         osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xLeft+lenSmall*0.3, y, xLeft+lenSmall*0.3+len, y);
         else
            g_pRenderEngine->drawLine(ahi_xs+wBar-lenSmall*0.3, y, ahi_xs+wBar-len-lenSmall*0.3, y);

         osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xLeft+lenSmall*0.3, y, xLeft+lenSmall*0.3+len, y);
         else
            g_pRenderEngine->drawLine(ahi_xs+wBar-lenSmall*0.3, y, ahi_xs+wBar-len-lenSmall*0.3, y);

         sprintf(szBuff, "%d", iValue);
         if ( ( y > yMid + hBar*0.5 + height_text*0.6 ||
              y < yMid - hBar*0.5 - height_text*0.6 ) &&
              y < ahi_ye - height_text*0.6 &&
              y > ahi_ys + height_text*0.6 )
         {
            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
            if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
               osd_show_value(xLeft+len+lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);
            else
               osd_show_value_left(ahi_xs+wBar-len-lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);

            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
            if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
               osd_show_value(xLeft+len+lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);
            else
               osd_show_value_left(ahi_xs+wBar-len-lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);
         }
      }
      else
      {
         osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xLeft+lenSmall*0.3, y, xLeft+lenSmall*0.3+lenSmall, y);
         else
            g_pRenderEngine->drawLine(ahi_xs+wBar-lenSmall*0.3, y, ahi_xs+wBar-lenSmall-lenSmall*0.3, y);
         osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xLeft+lenSmall*0.3, y, xLeft+lenSmall*0.3+lenSmall, y);
         else
            g_pRenderEngine->drawLine(ahi_xs+wBar-lenSmall*0.3, y, ahi_xs+wBar-lenSmall-lenSmall*0.3, y);
      }

      if ( ( iValue % 10 ) == 0 )
      {
         osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xLeft+lenSmall*0.3, y, xLeft+lenSmall*0.3+len, y);
         else
            g_pRenderEngine->drawLine(ahi_xs+wBar-lenSmall*0.3, y, ahi_xs+wBar-len-lenSmall*0.3, y);

         osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xLeft+lenSmall*0.3, y, xLeft+lenSmall*0.3+len, y);
         else
            g_pRenderEngine->drawLine(ahi_xs+wBar-lenSmall*0.3, y, ahi_xs+wBar-len-lenSmall*0.3, y);

         sprintf(szBuff, "%d", iValue);
         if ( ( y > yMid + hBar*0.5 + height_text*0.6 ||
              y < yMid - hBar*0.5 - height_text*0.6 ) &&
              y < ahi_ye - height_text*0.6 &&
              y > ahi_ys + height_text*0.6 )
         {
            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
            if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
               osd_show_value(xLeft+len+lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);
            else
               osd_show_value_left(ahi_xs+wBar-len-lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);

            osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
            if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
               osd_show_value(xLeft+len+lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);
            else
               osd_show_value_left(ahi_xs+wBar-len-lenSmall*0.8, y-height_text*0.45, szBuff, g_idFontOSD);
         }
      }
      else
      {
         osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xLeft+lenSmall*0.3, y, xLeft+lenSmall*0.3+lenSmall, y);
         else
            g_pRenderEngine->drawLine(ahi_xs+wBar-lenSmall*0.3, y, ahi_xs+wBar-lenSmall-lenSmall*0.3, y);

         osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
         if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
            g_pRenderEngine->drawLine(xLeft+lenSmall*0.3, y, xLeft+lenSmall*0.3+lenSmall, y);
         else
            g_pRenderEngine->drawLine(ahi_xs+wBar-lenSmall*0.3, y, ahi_xs+wBar-lenSmall-lenSmall*0.3, y);
      }

      iValue -= iValueStep;
   }
}

void osd_ahi_detailed_show_auxiliary_info(float roll, float pitch)
{
   Preferences* p = get_Preferences();
   char szBuff[1024];
   float fSpeed = 0.0;
   float fAltitude = 0.0;

   float wBar = ahi_fMaxHeight/ahi_fBarAspect/g_pRenderEngine->getAspectRatio();
   float hBar = 0.6*ahi_fMaxHeight/ahi_fBarAspect;
   float len = ahi_fMaxWidth * ahi_GradationAspect;
   float lenSmall = ahi_fMaxWidth * ahi_smallGradationAspect;
   float xLeft = ahi_xs+wBar;
   float xRight = ahi_xe-wBar;
   bool bAlignLeft = true;
   bool bAlignRight = false;
   float fScale = 0.86;
   float height_text = osd_getFontHeight() * ahi_fScale * fScale;

   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      bAlignRight = true;
      bAlignLeft = false;
      xLeft = osd_getMarginX();
      xRight = 1.0-osd_getMarginX();
      if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      {
         xLeft += osd_getVerticalBarWidth();
         xRight -= osd_getVerticalBarWidth();
      }
   }

   // Left side

   float y = ahi_ye + height_text*0.6;

   if ( g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_AIR_SPEED_MAIN )
   {
      strcpy(szBuff,"GS: 0");
      if ( NULL != g_pCurrentModel && link_has_fc_telemetry() && NULL != g_pdpfct && 0 != g_pdpfct->hspeed)
      {
         float fSpeed = g_pdpfct->hspeed/100.0f-1000.0f;

         if ( p->iUnits == prefUnitsMetric || p->iUnits == prefUnitsImperial )
         {
            fSpeed = fSpeed * 3.6f; // m/s->km/h
            fSpeed = _osd_convertKm(fSpeed);
         }
         else
            fSpeed = _osd_convertMeters(fSpeed);

         s_ahi_lastGSpeed = s_ahi_lastGSpeed * 0.6 + 0.4 * fSpeed;
         sprintf(szBuff, "GS: %.1f", s_ahi_lastGSpeed);
         if ( 0 == g_pdpfct->hspeed )
            strcpy(szBuff, "GS: 0");
      }

      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);

      if ( p->iUnits == prefUnitsMetric )
      {
         if ( bAlignLeft )
            osd_show_value_sufix_left(xLeft,y, szBuff, "km/h", g_idFontOSD, g_idFontOSDSmall);
         else
            osd_show_value_sufix(xLeft,y, szBuff, "km/h", g_idFontOSD, g_idFontOSDSmall);
      }
      else if ( p->iUnits == prefUnitsMeters )
      {
         if ( bAlignLeft )
            osd_show_value_sufix_left(xLeft,y, szBuff, "m/s", g_idFontOSD, g_idFontOSDSmall);
         else
            osd_show_value_sufix(xLeft,y, szBuff, "m/s", g_idFontOSD, g_idFontOSDSmall);
      }
      else if ( p->iUnits == prefUnitsImperial )
      {
         if ( bAlignLeft )
            osd_show_value_sufix_left(xLeft,y, szBuff, "mi/h", g_idFontOSD, g_idFontOSDSmall);
         else
            osd_show_value_sufix(xLeft,y, szBuff, "mi/h", g_idFontOSD, g_idFontOSDSmall);
      }
      else
      {
         if ( bAlignLeft )
            osd_show_value_sufix_left(xLeft,y, szBuff, "ft/s", g_idFontOSD, g_idFontOSDSmall);
         else
            osd_show_value_sufix(xLeft,y, szBuff, "ft/s", g_idFontOSD, g_idFontOSDSmall);
      }

      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);

      if ( p->iUnits == prefUnitsMetric )
      {
         if ( bAlignLeft )
            osd_show_value_sufix_left(xLeft,y, szBuff, "km/h", g_idFontOSD, g_idFontOSDSmall);
         else
            osd_show_value_sufix(xLeft,y, szBuff, "km/h", g_idFontOSD, g_idFontOSDSmall);
      }
      else if ( p->iUnits == prefUnitsMeters )
      {
         if ( bAlignLeft )
            osd_show_value_sufix_left(xLeft,y, szBuff, "m/s", g_idFontOSD, g_idFontOSDSmall);
         else
            osd_show_value_sufix(xLeft,y, szBuff, "m/s", g_idFontOSD, g_idFontOSDSmall);
      }
      else if ( p->iUnits == prefUnitsImperial )
      {
         if ( bAlignLeft )
            osd_show_value_sufix_left(xLeft,y, szBuff, "mi/h", g_idFontOSD, g_idFontOSDSmall);
         else
            osd_show_value_sufix(xLeft,y, szBuff, "mi/h", g_idFontOSD, g_idFontOSDSmall);
      }
      else
      {
         if ( bAlignLeft )
            osd_show_value_sufix_left(xLeft,y, szBuff, "ft/s", g_idFontOSD, g_idFontOSDSmall);
         else
            osd_show_value_sufix(xLeft,y, szBuff, "ft/s", g_idFontOSD, g_idFontOSDSmall);
      }

      y += height_text*1.5;
   }

   int dist = 0;
   if ( NULL != g_pCurrentModel && (0 < g_pCurrentModel->iGPSCount) && NULL != g_pdpfct )
     dist = g_pdpfct->distance/100;

   sprintf(szBuff,"D: %d", dist);
   dist = _osd_convertMeters((float)dist);

   osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
   {
      if ( bAlignLeft )
         osd_show_value_sufix_left(xLeft,y, szBuff, "ft", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(xLeft,y, szBuff, "ft", g_idFontOSD, g_idFontOSDSmall);
   }
   else
   {
      if ( bAlignLeft )
         osd_show_value_sufix_left(xLeft,y, szBuff, "m", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(xLeft,y, szBuff, "m", g_idFontOSD, g_idFontOSDSmall);
   }
   osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
   {
      if ( bAlignLeft )
         osd_show_value_sufix_left(xLeft,y, szBuff, "ft", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(xLeft,y, szBuff, "ft", g_idFontOSD, g_idFontOSDSmall);
   }
   else
   {
      if ( bAlignLeft )
         osd_show_value_sufix_left(xLeft,y, szBuff, "m", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(xLeft,y, szBuff, "m", g_idFontOSD, g_idFontOSDSmall);
   }

   y += height_text*1.5;

   if ( NULL != g_pdpfct && link_has_fc_telemetry() )
      sprintf(szBuff, "%03d", g_pdpfct->heading);
   else
      sprintf(szBuff, "---");
     
   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      //float dx = osd_ahi_show_icon(xLeft, y,"", "", bAlignLeft, fScale);
      //dx += 0.006 * ahi_fScale;
      float dx = 0.0;
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      //osd_ahi_show_icon(xLeft, y,"", "", bAlignLeft, fScale);
      if ( bAlignLeft )
         osd_show_value_left(xLeft+dx,y, szBuff, g_idFontOSD);
      else
         osd_show_value(xLeft+dx,y, szBuff, g_idFontOSD);

      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      //osd_ahi_show_icon(xLeft, y,"", "", bAlignLeft, fScale);
      if ( bAlignLeft )
         osd_show_value_left(xLeft+dx,y, szBuff, g_idFontOSD);
      else
         osd_show_value(xLeft+dx,y, szBuff, g_idFontOSD);
   }
   else
   {
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      if ( bAlignLeft )
         osd_show_value_left(xLeft,y, szBuff, g_idFontOSD);
      else
         osd_show_value(xLeft,y, szBuff, g_idFontOSD);

      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
      if ( bAlignLeft )
         osd_show_value_left(xLeft,y, szBuff, g_idFontOSD);
      else
         osd_show_value(xLeft,y, szBuff, g_idFontOSD);
   }
      
   // Right side
   
   y = ahi_ye + height_text*0.6;

   if ( s_bDebugAHIShowAll || (g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SHOW_ALTGRAPH) )
      return;

   if ( NULL != g_pdpfct && link_has_fc_telemetry() )
      sprintf(szBuff, "VS: %.1f", _osd_convertMeters(g_pdpfct->vspeed/100.0f-1000.0));
   else
      sprintf(szBuff,"VS: 0.0");
     
   osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
   {
      if ( bAlignRight )
         osd_show_value_sufix_left(xRight,y, szBuff, "ft/s", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(xRight,y, szBuff, "ft/s", g_idFontOSD, g_idFontOSDSmall);
   }
   else
   {
      if ( bAlignRight )
         osd_show_value_sufix_left(xRight,y, szBuff, "m/s", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(xRight,y, szBuff, "m/s", g_idFontOSD, g_idFontOSDSmall);
   }

   osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0.0);
   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
   {
      if ( bAlignRight )
         osd_show_value_sufix_left(xRight,y, szBuff, "ft/s", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(xRight,y, szBuff, "ft/s", g_idFontOSD, g_idFontOSDSmall);
   }
   else
   {
      if ( bAlignRight )
         osd_show_value_sufix_left(xRight,y, szBuff, "m/s", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(xRight,y, szBuff, "m/s", g_idFontOSD, g_idFontOSDSmall);
   }
}

void osd_ahi_show_altgraph()
{
   Preferences* p = get_Preferences();
   if ( g_TimeNow > s_ahi_altitude_speed_last_time + 100 )
   {
      s_ahi_altitude_speed_last_time = g_TimeNow;

      if ( s_ahi_altitude_speed_values < ALTITUDE_VALUES )
      {
         for( int i=s_ahi_altitude_speed_values; i<ALTITUDE_VALUES; i++ )
            s_ahi_altitude_speed_history[i] = 0;
         s_ahi_altitude_speed_values++;
      }
      for( int i = ALTITUDE_VALUES-1; i > 0; i-- )
         s_ahi_altitude_speed_history[i] = s_ahi_altitude_speed_history[i-1];

      if ( link_has_fc_telemetry() && NULL != g_pdpfct )
         s_ahi_altitude_speed_history[0] = g_pdpfct->vspeed/100.0f-1000.0;
      //s_ahi_altitude_speed_history[0] = ((g_TimeNow/100)%20)-10.0;
   }

   float height_text = osd_getFontHeight() * ahi_fScale*0.8;

   float height = 0.04 * ahi_fScale;
   float width = height*4.0/g_pRenderEngine->getAspectRatio();

   float widthBar = width/(float)ALTITUDE_VALUES;
   float xPos = ahi_xe-width;
   float yPos = ahi_ye + height_text*1.2;

   if ( g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SPEED_TO_SIDES )
   {
      xPos = 1.0 - osd_getMarginX()-5.0/g_pRenderEngine->getScreenWidth() - width;
      if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
         xPos -= osd_getVerticalBarWidth();
   }
   char szBuff[32];
   float xText = xPos - 0.016 * ahi_fScale;

   osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
   g_pRenderEngine->drawTextLeft( xText, yPos, height_text, g_idFontOSD, "V-SPD");

   osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0);
   g_pRenderEngine->drawTextLeft( xText, yPos, height_text, g_idFontOSD, "V-SPD");

   if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
   {
      sprintf(szBuff, "%.1f", _osd_convertMeters(s_ahi_altitude_speed_history[0]) );
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      g_pRenderEngine->drawTextLeft(xText, yPos+1.2*height_text, height_text, g_idFontOSD, szBuff);
      g_pRenderEngine->drawTextLeft(xText, yPos+2.4*height_text, height_text*0.8, g_idFontOSD, "ft/s");

      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0);
      g_pRenderEngine->drawTextLeft(xText, yPos+1.2*height_text, height_text, g_idFontOSD, szBuff);
      g_pRenderEngine->drawTextLeft(xText, yPos+2.4*height_text, height_text*0.8, g_idFontOSD, "ft/s");
   }
   else
   {
      sprintf(szBuff, "%.1f", _osd_convertMeters(s_ahi_altitude_speed_history[0]) );
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_BORDER, 2.0*s_ShadowThicknessPx);
      g_pRenderEngine->drawTextLeft(xText, yPos+1.2*height_text, height_text, g_idFontOSD, szBuff);
      g_pRenderEngine->drawTextLeft(xText, yPos+2.4*height_text, height_text*0.8, g_idFontOSD, "m/s");
      osd_ahi_set_color(COLOR_OSD_AHI_TEXT_FG, 0);
      g_pRenderEngine->drawTextLeft(xText, yPos+1.2*height_text, height_text, g_idFontOSD, szBuff);
      g_pRenderEngine->drawTextLeft(xText, yPos+2.4*height_text, height_text*0.8, g_idFontOSD, "m/s");
   }

   float wPixel = 1.0/g_pRenderEngine->getScreenWidth();
   osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
   for( float i=0.0; i<width-4.0*wPixel; i+=4.0*wPixel )
      g_pRenderEngine->drawLine(xPos + i, yPos+0.5*height, xPos + i + 2.0*wPixel, yPos+0.5*height);
   osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
   for( float i=0.0; i<width-4.0*wPixel; i+=4.0*wPixel )
      g_pRenderEngine->drawLine(xPos + i, yPos+0.5*height, xPos + i + 2.0*wPixel, yPos+0.5*height);

   float fMin = 0.0;
   float fMax = 0.0;
   for( int i=0; i<ALTITUDE_VALUES; i++ )
   {
      if ( s_ahi_altitude_speed_history[i] > fMax )
         fMax = s_ahi_altitude_speed_history[i];
      if ( s_ahi_altitude_speed_history[i] < fMin )
         fMin = s_ahi_altitude_speed_history[i];
   }

   float hBar = 1.0;
   float fD = fabs(fMax);
   if ( fabs(fMin) > fD )
      fD = fabs(fMin);
   if ( fD < 0.5 )
      fD = 0.5;
   if ( fD > 0.00001 )
      hBar = 0.5*height/fD;

   osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
   float x = xPos+width;
   for( int i=1; i<ALTITUDE_VALUES; i++ )
   {
      g_pRenderEngine->drawLine(x - widthBar, yPos+0.5*height-s_ahi_altitude_speed_history[i]*hBar, x, yPos+0.5*height-s_ahi_altitude_speed_history[i-1]*hBar);
      x -= widthBar;
   }
   osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
   x = xPos+width;
   for( int i=1; i<ALTITUDE_VALUES; i++ )
   {
      g_pRenderEngine->drawLine(x - widthBar, yPos+0.5*height-s_ahi_altitude_speed_history[i]*hBar, x, yPos+0.5*height-s_ahi_altitude_speed_history[i-1]*hBar);
      x -= widthBar;
   }
}

void osd_show_ahi(float roll, float pitch)
{
   if ( NULL == g_pCurrentModel )
      return;

   Preferences* p = get_Preferences();
   s_LineThicknessPx = 2.0;
   switch( p->iAHIStrokeSize )
   {
/*
      case -2: s_LineThicknessPx = 1.4; break;
      case -1: s_LineThicknessPx = 2.1; break;
      case 0: s_LineThicknessPx = 2.8; break;
      case 1: s_LineThicknessPx = 3.1; break;
      case 2: s_LineThicknessPx = 3.6; break;
*/
      case -2: s_LineThicknessPx = 1.0; break;
      case -1: s_LineThicknessPx = 1.8; break;
      case 0: s_LineThicknessPx = 2.4; break;
      case 1: s_LineThicknessPx = 3.1; break;
      case 2: s_LineThicknessPx = 3.6; break;
   }

   //s_ShadowThicknessPx = s_LineThicknessPx*0.5;
   s_ShadowThicknessPx = 1.0;
   s_LineThickness = s_LineThicknessPx/g_pRenderEngine->getScreenHeight();

   //log_line("line thickens: %f, %f px", s_LineThickness, s_LineThicknessPx);

   COLOR_OSD_AHI_TEXT_FG[0] = p->iColorAHI[0];
   COLOR_OSD_AHI_TEXT_FG[1] = p->iColorAHI[1];
   COLOR_OSD_AHI_TEXT_FG[2] = p->iColorAHI[2];
   COLOR_OSD_AHI_TEXT_FG[3] = p->iColorAHI[3]/100.0;

   COLOR_OSD_AHI_LINE_FG[0] = p->iColorAHI[0];
   COLOR_OSD_AHI_LINE_FG[1] = p->iColorAHI[1];
   COLOR_OSD_AHI_LINE_FG[2] = p->iColorAHI[2];
   COLOR_OSD_AHI_LINE_FG[3] = p->iColorAHI[3]/100.0;

   if ( (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex]) & OSD_FLAG_REVERT_PITCH )
      pitch = -pitch;
   if ( (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex]) & OSD_FLAG_REVERT_ROLL )
      roll = -roll;

   if ( s_bDebugAHIShowAll || (g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SHOW_HEADING) )
   {
      float yHeading = osd_getMarginY() + osd_getBarHeight() + 0.03*osd_getScaleOSD();
      yHeading += osd_getBarHeight();

      if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
         yHeading = osd_getMarginY() + 0.03*osd_getScaleOSD();;

      osd_show_ahi_heading(yHeading, 0.32*ahi_fScale);
   }
   s_ahi_lastRoll = s_ahi_lastRoll * 0.6 + 0.4 * roll;
   s_ahi_lastPitch = s_ahi_lastPitch * 0.6 + 0.4 * pitch;

   char szBuff[64];
   float ahiAlpha = 0.6;   

   /*
   if ( s_bDebugAHIShowAll || (g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SHOW_HORIZONT) )
   {
      osd_ahi_show_plane(s_ahi_lastRoll, s_ahi_lastPitch);
      osd_ahi_show_horizont_ladder(s_ahi_lastRoll, s_ahi_lastPitch);
      osd_set_colors();
   }
   */

   ahi_fMaxWidth = 0.55*(1.0-2*osd_getMarginX()) * ahi_fScale;
   ahi_fMaxHeight = 0.55*(1.0-2*osd_getMarginY()) * ahi_fScale;
   ahi_xs = (1.0-ahi_fMaxWidth)/2;
   ahi_ys = (1.0-ahi_fMaxHeight)/2;
   ahi_xe = ahi_xs + ahi_fMaxWidth;
   ahi_ye = ahi_ys + ahi_fMaxHeight;

   if ( s_bDebugAHIShowAll || (g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SHOW_SPEEDALT) )
   {
      osd_ahi_set_color(COLOR_OSD_AHI_LINE_BORDER, 2.0*s_ShadowThicknessPx + s_LineThicknessPx);
      osd_ahi_draw_panels();     
      osd_ahi_set_color(COLOR_OSD_AHI_LINE_FG, s_LineThicknessPx);
      osd_ahi_draw_panels();

      osd_ahi_detailed_show_main_panels_info(s_ahi_lastRoll, s_ahi_lastPitch);
      osd_ahi_detailed_show_auxiliary_info(s_ahi_lastRoll, s_ahi_lastPitch);
   }

   if ( s_bDebugAHIShowAll || (g_pCurrentModel->osd_params.instruments_flags[s_nCurrentOSDFlagsIndex] & INSTRUMENTS_FLAG_SHOW_ALTGRAPH) )
      osd_ahi_show_altgraph();
}
