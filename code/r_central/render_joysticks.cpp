/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
         * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
       * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "render_joysticks.h"

#include "../base/hardware.h"
#include "../renderer/render_engine.h"
#include "fonts.h"
#include "colors.h"
#include "shared_vars.h"
#include <math.h>


void render_joystick(float xPos, float yPos, float width, float height, int numAxes, int numButtons, int* pAxes, int* pButtons, int* pCenterZones, int* pAxeCalMin, int* pAxeCalMax, int* pAxeCalCenters)
{
   char szBuff[256];
   double color_line[4] = { 200,200,200,0.9 };
   double color_high[4] = { 180,20,70,0.9 };

   float paddingV = ((float)height)*0.05;
   float paddingH = paddingV/g_pRenderEngine->getAspectRatio();
   float radiusV = ((float)height)*0.25;
   float radiusH = radiusV/g_pRenderEngine->getAspectRatio();
   float radiusAxis2V = ((float)width)*0.2;
   float radiusAxis2H = radiusAxis2V/g_pRenderEngine->getAspectRatio();
   float radiusButtonV = ((float)height)*0.04;
   float radiusButtonH = radiusButtonV/g_pRenderEngine->getAspectRatio();
   float axisHeight = ((float)height)*0.05;
   float height_text = ((float)height)*0.046;

   // Background

   g_pRenderEngine->setColors(get_Color_PopupBg());
   g_pRenderEngine->setStroke(get_Color_PopupBorder());

   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 0.01);

   // Main axes

   g_pRenderEngine->setFill(color_line[0], color_line[1], 0, 0);
   g_pRenderEngine->setStroke(color_line[0], color_line[1], color_line[2], color_line[3]);
   g_pRenderEngine->setStrokeSize(2);
   g_pRenderEngine->drawRoundRect(xPos+paddingH, yPos+paddingV, radiusH*2, radiusV*2, 0.5*radiusH);
   g_pRenderEngine->drawRoundRect(xPos+paddingH+2*radiusH+paddingH, yPos+paddingV, radiusH*2, radiusV*2, 0.5*radiusH);

   g_pRenderEngine->setFill(color_line[0], color_line[1], color_line[2], color_line[3]);
   g_pRenderEngine->setStroke(color_line[0], color_line[1], color_line[2], 0);
   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->drawText(xPos+paddingH+radiusH+height_text*0.2, yPos+paddingV+height_text*0.2, g_idFontMenu, "1");
   g_pRenderEngine->drawText(xPos+paddingH+height_text*0.3, yPos+paddingV+radiusV, g_idFontMenu, "2");
   g_pRenderEngine->drawText(xPos+paddingH-height_text*0.2, yPos+paddingV+2*radiusV+paddingV, g_idFontMenu, "3");
   g_pRenderEngine->drawText(xPos+paddingH+radiusH+height_text*0.2+2*radiusH+paddingH, yPos+paddingV+height_text*0.2, g_idFontMenu, "4");
   g_pRenderEngine->drawText(xPos+paddingH+height_text*0.3+2*radiusH+paddingH, yPos+paddingV+radiusV, g_idFontMenu, "5");
   g_pRenderEngine->drawText(xPos+paddingH-height_text*0.2+2*radiusH+paddingH, yPos+paddingV+2*radiusV+paddingV, g_idFontMenu, "6");

   g_pRenderEngine->setFill(color_high[0], color_high[1], color_high[2], color_high[3]);
   g_pRenderEngine->setStroke(color_high[0], color_high[1], color_high[2], color_high[3]);
   //g_pRenderEngine->setStrokeSize(0);

   // Compute axes values

   int axesV[MAX_JOYSTICK_AXES];
   int axesCalMin[MAX_JOYSTICK_AXES];
   int axesCalMid[MAX_JOYSTICK_AXES];
   int axesCalMax[MAX_JOYSTICK_AXES];
   int axesCenterZone[MAX_JOYSTICK_AXES];
   bool axeIsCentered[MAX_JOYSTICK_AXES];
   int buttonV[MAX_JOYSTICK_BUTTONS];

   memcpy(&(axesCalMin[0]), pAxeCalMin, sizeof(int)*MAX_JOYSTICK_AXES);
   memcpy(&(axesCalMid[0]), pAxeCalCenters, sizeof(int)*MAX_JOYSTICK_AXES);
   memcpy(&(axesCalMax[0]), pAxeCalMax, sizeof(int)*MAX_JOYSTICK_AXES);

   //for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
   //   log_line("%d %d %d", axesCalMin[i], axesCalMid[i], axesCalMax[i]);

   int* p = pAxes;
   for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
   {
      axesV[i] = *p;
      p++;
      if ( axesV[i] < axesCalMin[i] )
         axesV[i] = axesCalMin[i];
      if ( axesV[i] > axesCalMax[i] )
         axesV[i] = axesCalMax[i];
   }

   p = pCenterZones;
   for( int i=0; i<MAX_JOYSTICK_AXES; i++ )
   {
      if ( NULL == p )
         axesCenterZone[i] = (axesCalMax[i] - axesCalMin[i])/10;
      else
      {
         axesCenterZone[i] = (float)(axesCalMax[i] - axesCalMin[i]) * (float)(*p)/1000.0;
         p++;
      }
      if ( fabs(axesV[i]-axesCalMid[i]) < axesCenterZone[i] )
         axeIsCentered[i] = true;
      else
         axeIsCentered[i] = false;
   }

   p = pButtons;
   for( int i=0; i<MAX_JOYSTICK_BUTTONS; i++ )
   {
      buttonV[i] = *p;
      p++;
   }

   // Render main Axes values

   float fxAxe = 0.0;
   float fyAxe = 0.0;

   if ( axesCalMax[0] != axesCalMin[0] )
   {
      if ( axesV[0] > axesCalMid[0] )
      if ( axesCalMax[0] > axesCalMid[0] )
         fxAxe = (float)((float)axesV[0] - (float)axesCalMid[0])/(float)((float)axesCalMax[0]-(float)axesCalMid[0]);
      if ( axesV[0] < axesCalMid[0] )
      if ( axesCalMin[0] < axesCalMid[0] )
         fxAxe = (float)((float)axesV[0] - (float)axesCalMid[0])/(float)((float)axesCalMid[0]-(float)axesCalMin[0]);
   }
   if ( axesCalMax[1] != axesCalMin[1] )
   {
      if ( axesV[1] > axesCalMid[1] )
      if ( axesCalMax[1] > axesCalMid[1] )
         fyAxe = (float)((float)axesV[1] - (float)axesCalMid[1])/(float)((float)axesCalMax[1]-(float)axesCalMid[1]);
      if ( axesV[1] < axesCalMid[1] )
      if ( axesCalMin[1] < axesCalMid[1] )
         fyAxe = (float)((float)axesV[1] - (float)axesCalMid[1])/(float)((float)axesCalMid[1]-(float)axesCalMin[1]);
   }

   if (fxAxe < -1.0 ) fxAxe = -1.0;
   if (fxAxe > 1.0 )  fxAxe = 1.0;
   if (fyAxe < -1.0 ) fyAxe = -1.0;
   if (fyAxe > 1.0 )  fyAxe = 1.0;

   float x1 = xPos+paddingH + radiusH + 0.9*radiusH*fxAxe;
   float y1 = yPos+paddingV + radiusV + 0.9*radiusV*fyAxe;
   g_pRenderEngine->drawRoundRect(x1-3.0*g_pRenderEngine->getPixelWidth(), y1-3.0*g_pRenderEngine->getPixelHeight(), 6.0*g_pRenderEngine->getPixelWidth(),6.0*g_pRenderEngine->getPixelHeight(), 2.0*g_pRenderEngine->getPixelWidth());

   fxAxe = 0.0;
   fyAxe = 0.0;
   if ( axesCalMax[3] != axesCalMin[3] )
   {
      if ( axesV[3] > axesCalMid[3] )
      if ( axesCalMax[3] > axesCalMid[3] )
         fxAxe = (float)((float)axesV[3] - (float)axesCalMid[3])/(float)((float)axesCalMax[3]-(float)axesCalMid[3]);
      if ( axesV[3] < axesCalMid[3] )
      if ( axesCalMin[3] < axesCalMid[3] )
         fxAxe = (float)((float)axesV[3] - (float)axesCalMid[3])/(float)((float)axesCalMid[3]-(float)axesCalMin[3]);
   }
   if ( axesCalMax[4] != axesCalMin[4] )
   {
      if ( axesV[4] > axesCalMid[4] )
      if ( axesCalMax[4] > axesCalMid[4] )
         fyAxe = (float)((float)axesV[4] - (float)axesCalMid[4])/(float)((float)axesCalMax[4]-(float)axesCalMid[4]);
      if ( axesV[4] < axesCalMid[4] )
      if ( axesCalMin[4] < axesCalMid[4] )
         fyAxe = (float)((float)axesV[4] - (float)axesCalMid[4])/(float)((float)axesCalMid[4]-(float)axesCalMin[4]);
   }

   if (fxAxe < -1.0 ) fxAxe = -1.0;
   if (fxAxe > 1.0 )  fxAxe = 1.0;
   if (fyAxe < -1.0 ) fyAxe = -1.0;
   if (fyAxe > 1.0 )  fyAxe = 1.0;

   fyAxe = - fyAxe;

   x1 = xPos+paddingH + radiusH + 0.9*radiusH*fxAxe + 2*radiusH + paddingH;
   y1 = yPos+paddingV + radiusV + 0.9*radiusV*fyAxe;
   g_pRenderEngine->drawRoundRect(x1-3.0*g_pRenderEngine->getPixelWidth(), y1-3.0*g_pRenderEngine->getPixelHeight(), 6.0*g_pRenderEngine->getPixelWidth(),6.0*g_pRenderEngine->getPixelHeight(), 2.0*g_pRenderEngine->getPixelWidth());

   g_pRenderEngine->setFill(color_high[0], color_high[1], color_high[2], 0.3);
   g_pRenderEngine->setStroke(color_high[0], color_high[1], color_high[2], 0.3);
   g_pRenderEngine->setStrokeSize(1);

   if ( axeIsCentered[0] )
      g_pRenderEngine->drawLine( xPos+paddingH+radiusH, yPos+paddingV+radiusV-0.9*radiusV, xPos+paddingH+radiusH, yPos+paddingV+radiusV+0.9*radiusV);
   if ( axeIsCentered[1] )
      g_pRenderEngine->drawLine( xPos+paddingH+radiusH-0.9*radiusH, yPos+paddingV+radiusV, xPos+paddingH+radiusH+0.9*radiusH, yPos+paddingV+radiusV);
   if ( axeIsCentered[3] )
      g_pRenderEngine->drawLine( xPos+paddingH+radiusH+2*radiusH + paddingH, yPos+paddingV+radiusV-0.9*radiusV, xPos+paddingH+radiusH + 2*radiusH + paddingH, yPos+paddingV+radiusV+0.9*radiusV);
   if ( axeIsCentered[4] )
      g_pRenderEngine->drawLine( xPos+paddingH+radiusH+2*radiusH + paddingH - 0.9*radiusH, yPos+paddingV+radiusV, xPos+paddingH+radiusH+2*radiusH + paddingH + 0.9*radiusH, yPos+paddingV+radiusV);

   // Secondary axes

   g_pRenderEngine->setFill(color_line[0], color_line[1], 0, 0);
   g_pRenderEngine->setStroke(color_line[0], color_line[1], color_line[2], color_line[3]);
   g_pRenderEngine->setStrokeSize(2);

   float x = xPos+paddingH+radiusH;
   float y = yPos+paddingV + 2*radiusV + paddingV;

   g_pRenderEngine->drawRoundRect(x-radiusAxis2H, y, radiusAxis2H*2, axisHeight, 4.0*g_pRenderEngine->getPixelWidth());
   g_pRenderEngine->drawRoundRect(x+2*radiusH+paddingH-radiusAxis2H, y, radiusAxis2H*2, axisHeight, 4.0*g_pRenderEngine->getPixelWidth());

   g_pRenderEngine->setFill(color_high[0], color_high[1], color_high[2], color_high[3]);
   g_pRenderEngine->setStroke(color_high[0], color_high[1], color_high[2], color_high[3]);
   g_pRenderEngine->setStrokeSize(0);

   fxAxe = 0.0;
   fyAxe = 0.0;

   if ( axesCalMax[2] != axesCalMin[2] )
   {
      if ( axesV[2] > axesCalMid[2] )
      if ( axesCalMax[2] > axesCalMid[2] )
         fxAxe = (float)((float)axesV[2] - (float)axesCalMid[2])/(float)((float)axesCalMax[2]-(float)axesCalMid[2]);
      if ( axesV[2] < axesCalMid[2] )
      if ( axesCalMin[2] < axesCalMid[2] )
         fxAxe = (float)((float)axesV[2] - (float)axesCalMid[2])/(float)((float)axesCalMid[2]-(float)axesCalMin[2]);
   }

   //log_line("fxAxe: %f, %d, %d", fxAxe, axesCalMax[2],axesCalMin[2]);

   if (fxAxe < -1.0 ) fxAxe = -1.0;
   if (fxAxe > 1.0 )  fxAxe = 1.0;

   x1 = x + radiusAxis2H*0.98*fxAxe;
   if ( fxAxe < 0 )
      g_pRenderEngine->drawRoundRect(x1, y+g_pRenderEngine->getPixelHeight(), abs(x-x1), axisHeight-2.0*g_pRenderEngine->getPixelHeight(), 4.0*g_pRenderEngine->getPixelHeight());
   else
      g_pRenderEngine->drawRoundRect(x, y+g_pRenderEngine->getPixelHeight(), abs(x-x1), axisHeight-2.0*g_pRenderEngine->getPixelHeight(), 4.0*g_pRenderEngine->getPixelHeight());

   fxAxe = 0.0;
   if ( axesCalMax[5] != axesCalMin[5] )
   {
      if ( axesV[5] > axesCalMid[5] )
      if ( axesCalMax[5] > axesCalMid[5] )
         fxAxe = (float)((float)axesV[5] - (float)axesCalMid[5])/(float)((float)axesCalMax[5]-(float)axesCalMid[5]);
      if ( axesV[5] < axesCalMid[5] )
      if ( axesCalMin[5] < axesCalMid[5] )
         fxAxe = (float)((float)axesV[5] - (float)axesCalMid[5])/(float)((float)axesCalMid[5]-(float)axesCalMin[5]);
   }

   //log_line("fxAxe2: %f, %d, %d", fxAxe, axesCalMax[5],axesCalMin[5]);
   if (fxAxe < -1.0 ) fxAxe = -1.0;
   if (fxAxe > 1.0 )  fxAxe = 1.0;

   x += 2*radiusH + paddingH;
   x1 = x + radiusAxis2H*0.98*fxAxe;
   if ( fxAxe < 0 )
      g_pRenderEngine->drawRoundRect(x1, y+g_pRenderEngine->getPixelHeight(), abs(x-x1), axisHeight-2.0*g_pRenderEngine->getPixelHeight(), 4.0*g_pRenderEngine->getPixelHeight());
   else
      g_pRenderEngine->drawRoundRect(x, y+g_pRenderEngine->getPixelHeight(), abs(x-x1), axisHeight-2.0*g_pRenderEngine->getPixelHeight(), 4.0*g_pRenderEngine->getPixelHeight());

   // Other axis

   g_pRenderEngine->setFill(color_line[0], color_line[1], 0, 0);
   g_pRenderEngine->setStroke(color_line[0], color_line[1], color_line[2], 0);
   g_pRenderEngine->setStrokeSize(0);

   x = xPos + paddingH + 2*radiusH + paddingH + 2*radiusH + 2*paddingH;
   y = yPos + paddingV;
   g_pRenderEngine->drawText(x,y, g_idFontMenu, "Auxiliary Axes:");

   y += 1.9*height_text;
   for( int i=6; i<numAxes; i++ )
   {
      g_pRenderEngine->setFill(color_line[0], color_line[1], 0, 0);
      g_pRenderEngine->setStroke(color_line[0], color_line[1], color_line[2], 0);
      g_pRenderEngine->setStrokeSize(0);
      sprintf(szBuff, "%d", i+1);
      g_pRenderEngine->drawText(x-paddingH*1.2, y-height_text*0.01, g_idFontMenu, szBuff);

      g_pRenderEngine->setFill(color_line[0], color_line[1], 0, 0);
      g_pRenderEngine->setStroke(color_line[0], color_line[1], color_line[2], color_line[3]);
      g_pRenderEngine->setStrokeSize(2);
      g_pRenderEngine->drawRoundRect(x, y, radiusAxis2H*2, axisHeight, 4.0*g_pRenderEngine->getPixelHeight());

      g_pRenderEngine->setFill(color_high[0], color_high[1], color_high[2], color_high[3]);
      g_pRenderEngine->setStroke(color_high[0], color_high[1], color_high[2], color_high[3]);
      g_pRenderEngine->setStrokeSize(0);

      fxAxe = 0.0;
      if ( axesCalMax[i] != axesCalMin[i] )
      {
      if ( axesV[i] > axesCalMid[i] )
      if ( axesCalMax[i] > axesCalMid[i] )
         fxAxe = (float)((float)axesV[i] - (float)axesCalMid[i])/(float)((float)axesCalMax[i]-(float)axesCalMid[i]);
      if ( axesV[i] < axesCalMid[i] )
      if ( axesCalMin[i] < axesCalMid[i] )
         fxAxe = (float)((float)axesV[i] - (float)axesCalMid[i])/(float)((float)axesCalMid[i]-(float)axesCalMin[i]);
      }

      if (fxAxe < -1.0 ) fxAxe = -1.0;
      if (fxAxe > 1.0 )  fxAxe = 1.0;
      x1 = x + radiusAxis2H + radiusAxis2H*0.98*fxAxe;
      if ( fxAxe < 0 )
         g_pRenderEngine->drawRoundRect(x1, y+g_pRenderEngine->getPixelHeight(), abs(x1-(x+radiusAxis2H)), axisHeight-2.0*g_pRenderEngine->getPixelHeight(), 4.0*g_pRenderEngine->getPixelHeight());
      else
         g_pRenderEngine->drawRoundRect(x+radiusAxis2H, y+g_pRenderEngine->getPixelHeight(), abs(x1-(x+radiusAxis2H)), axisHeight-2.0*g_pRenderEngine->getPixelHeight(), 4.0*g_pRenderEngine->getPixelHeight());

      y += 2*axisHeight;
   }
   // Buttons

   x = xPos+paddingH;

   g_pRenderEngine->setFill(color_line[0], color_line[1], color_line[2], color_line[3]);
   g_pRenderEngine->setStroke(color_line[0], color_line[1], color_line[2], 0);
   g_pRenderEngine->setStrokeSize(0);

   y = yPos+paddingV*3.0+2*radiusV+1.2*height_text;
   g_pRenderEngine->drawText( x, y, g_idFontMenu, "Buttons:");

   y += height_text*1.8;
   float x0 = x;
   for( int i=0; i<numButtons; i++ )
   {
      g_pRenderEngine->setFill(color_line[0], color_line[1], 0, 0);
      g_pRenderEngine->setStroke(color_line[0], color_line[1], color_line[2], color_line[3]);
      g_pRenderEngine->setStrokeSize(2);
      g_pRenderEngine->drawRoundRect(x, y, radiusButtonH*2, radiusButtonV*2, 3.0*g_pRenderEngine->getPixelHeight());

      if ( buttonV[i] != 0 )
      {
         g_pRenderEngine->setFill(color_high[0], color_high[1], color_high[2], color_high[3]);
         g_pRenderEngine->setStroke(color_high[0], color_high[1], color_high[2], color_high[3]);
         g_pRenderEngine->setStrokeSize(0);
         g_pRenderEngine->drawRoundRect(x+g_pRenderEngine->getPixelWidth(), y+g_pRenderEngine->getPixelHeight(), radiusButtonH*2-2.0*g_pRenderEngine->getPixelWidth(), radiusButtonV*2-2.0*g_pRenderEngine->getPixelHeight(), 3.0*g_pRenderEngine->getPixelHeight());
      }

      g_pRenderEngine->setFill(color_line[0], color_line[1], color_line[2], color_line[3]);
      g_pRenderEngine->setStroke(color_line[0], color_line[1], color_line[2], 0);
      g_pRenderEngine->setStrokeSize(0);
      sprintf(szBuff, "%d", i+1);
      if ( i < 9 )
         g_pRenderEngine->drawText(x+0.3*height_text, y+0.3*height_text, g_idFontMenu, szBuff);
      else
         g_pRenderEngine->drawText(x+0.3*height_text, y+0.3*height_text, g_idFontMenu, szBuff);

      x += 2*radiusButtonH+10.0*g_pRenderEngine->getPixelWidth();

      if ( i == 11 )
      {
         y += height_text*2.2;
         x = x0;
      }
   }
}
