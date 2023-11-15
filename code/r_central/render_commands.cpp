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

#include "colors.h"
#include "../renderer/render_engine.h"
#include "render_commands.h"
#include "menu.h"
#include "shared_vars.h"
#include "timers.h"
#include "osd_common.h"


double COLOR_RADIO_BARS[4] = { 180,180,180, 0.9 };
double COLOR_RADIO_BARS_HIGH[4] = { 250,250,250, 1 };
double s_deltaColorHigh = 70;
double s_delatAlfaColorHigh = 0.1;

static u32 s_lastTimeRenderAnimCommands = 0;
static float s_fBarHighPosX = 0.0;
static int s_ProgressPercent = -1;
static bool s_bUploadNewMethod = true;

void render_animation_bars( float xPos, float yPos, float fWidth, float fHeight, bool bCentered )
{
   int numBars = 12;
   float fBarWidth = 0.64*fWidth/numBars;
   float fBarSpacing = 0.36*fWidth/numBars;

   if ( bCentered )
   {
      xPos -= fWidth*0.5;
      yPos -= fHeight*0.5;
   }
   s_fBarHighPosX += fWidth * (g_TimeNow-s_lastTimeRenderAnimCommands)/1000.0;
   if ( s_fBarHighPosX >= fWidth + 2*fBarWidth )
     s_fBarHighPosX = -2*fBarWidth;

   g_pRenderEngine->setStrokeSize(0);

   for( int i=0; i<numBars; i++ )
   {
      g_pRenderEngine->setFill(COLOR_RADIO_BARS[0], COLOR_RADIO_BARS[1], COLOR_RADIO_BARS[2], COLOR_RADIO_BARS[3]);

      float fDistance = i*(fBarWidth+fBarSpacing) + 0.5 * fBarWidth - s_fBarHighPosX;
      if ( fDistance < 0 )
         fDistance = -fDistance;
      float fWeight = fDistance/(0.28*fWidth);
      if ( fWeight > 1.0 )
         fWeight = 1.0;
      fWeight = 1.0 - fWeight;
      fWeight *= fWeight;

      float fBarHeight = 0.4*fHeight;
      fBarHeight += 0.6 * fHeight * fWeight;

      g_pRenderEngine->setFill(COLOR_RADIO_BARS[0]+s_deltaColorHigh*fWeight, COLOR_RADIO_BARS[1]+s_deltaColorHigh*fWeight, COLOR_RADIO_BARS[2]+s_deltaColorHigh*fWeight, COLOR_RADIO_BARS[3]+s_delatAlfaColorHigh*fWeight);
      g_pRenderEngine->drawRoundRect(xPos+i*(fBarWidth+fBarSpacing), yPos+0.5*(fHeight-fBarHeight), fBarWidth, fBarHeight, 0.006);
   }

   if ( s_ProgressPercent >= 0 )
   {
      char szBuff[32];
      snprintf(szBuff, sizeof(szBuff), "%d%%", s_ProgressPercent);
      if ( ! s_bUploadNewMethod )
         snprintf(szBuff, sizeof(szBuff), "* %d%%", s_ProgressPercent);
      float height_text = osd_getFontHeightBig();
      g_pRenderEngine->drawText(xPos+fWidth+0.01, yPos+height_text*0.1, height_text, g_idFontOSDBig, szBuff);
   }
}


void render_commands()
{
   render_animation_bars(0.5, 0.8, 0.12, 0.06, true );
   s_lastTimeRenderAnimCommands = g_TimeNow;
}

void render_commands_set_progress_percent(int percent, bool bNewMethod)
{
   s_ProgressPercent = percent;
   s_bUploadNewMethod = bNewMethod;
}