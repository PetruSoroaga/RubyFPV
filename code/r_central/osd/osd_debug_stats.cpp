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

#include "../../base/base.h"
#include "../../base/utils.h"
#include "../../base/ctrl_preferences.h"
#include "../../common/string_utils.h"
#include <math.h>
#include "osd_debug_stats.h"
#include "osd_common.h"
#include "../colors.h"
#include "../shared_vars.h"
#include "../timers.h"

extern float s_OSDStatsLineSpacing;
extern float s_fOSDStatsMargin;
extern float s_fOSDStatsGraphLinesAlpha;
extern u32 s_idFontStats;
extern u32 s_idFontStatsSmall;

bool s_bDebugStatsControllerInfoFreeze = false;
int s_bDebugStatsControllerInfoZoom = 0;
controller_runtime_info s_ControllerRTInfoFreeze;
vehicle_runtime_info s_VehicleRTInfoFreeze;

void osd_debug_stats_toggle_zoom()
{
   s_bDebugStatsControllerInfoZoom++;
   if ( s_bDebugStatsControllerInfoZoom > 2 )
      s_bDebugStatsControllerInfoZoom = 0;
}

void osd_debug_stats_toggle_freeze()
{
   s_bDebugStatsControllerInfoFreeze = ! s_bDebugStatsControllerInfoFreeze;
   if ( s_bDebugStatsControllerInfoFreeze )
   {
      memcpy(&s_ControllerRTInfoFreeze, &g_SMControllerRTInfo, sizeof(controller_runtime_info));
      memcpy(&s_VehicleRTInfoFreeze, &g_SMVehicleRTInfo, sizeof(vehicle_runtime_info));
   }
}

float _osd_render_debug_stats_graph_lines(float xPos, float yPos, float hGraph, float fWidth, int* pValues, int* pValuesMin, int* pValuesMax, int* pValuesAvg, int iCountValues)
{
   char szBuff[32];
   float height_text = g_pRenderEngine->textHeight(g_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontStatsSmall);

   int iMax = -1000;
   int iMin = 1000;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( NULL != pValues )
      if ( pValues[i] < 1000 )
      if ( pValues[i] > iMax )
         iMax = pValues[i];

      if ( NULL != pValuesMax )
      if ( pValuesMax[i] < 1000 )
      if ( pValuesMax[i] > iMax )
         iMax = pValuesMax[i];

      if ( NULL != pValues )
      if ( pValues[i] < 1000 )
      if ( pValues[i] < iMin )
         iMin = pValues[i];

      if ( NULL != pValuesMin )
      if ( pValuesMin[i] < 1000 )
      if ( pValuesMin[i] < iMin )
         iMin = pValuesMin[i];
   }
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   sprintf(szBuff, "%d", iMin);
   g_pRenderEngine->drawText(xPos, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", (iMax+iMin)/2);
   g_pRenderEngine->drawText(xPos, yPos +hGraph*0.5 - height_text_small*0.5, g_idFontStatsSmall, szBuff);

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   xPos += dx;
   fWidth -= dx;

   g_pRenderEngine->setStroke(250,250,250,0.5);
   for( float xx=xPos; xx<xPos+fWidth; xx += 0.02 )
   {
      g_pRenderEngine->drawLine(xx, yPos, xx + 0.008, yPos);
      g_pRenderEngine->drawLine(xx, yPos + hGraph*0.5, xx + 0.008, yPos+hGraph*0.5);
   }

   float xBar = xPos;
   float fWidthBar = fWidth / iCountValues;
   float hPoint = 0;
   float hPointPrev = 0;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( NULL != pValuesMin )
      if ( pValuesMin[i] < 1000 )
      {
         hPoint = hGraph * (float)(pValuesMin[i]-(float)iMin) / ((float)iMax-(float)iMin);
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar+fWidthBar, yPos + hGraph - hPoint);
         if ( i > 0 )
         if ( pValuesMin[i-1] < 1000 )
         {
            hPointPrev = hGraph * (float)(pValuesMin[i-1]-(float)iMin) / ((float)iMax-(float)iMin);
            g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar, yPos + hGraph - hPointPrev);
         }
      }
      
      if ( NULL != pValuesMin )
      if ( pValuesMax[i] < 1000 )
      {
         hPoint = hGraph * (float)(pValuesMax[i]-(float)iMin) / ((float)iMax-(float)iMin);
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar+fWidthBar, yPos + hGraph - hPoint);
         if ( i > 0 )
         if ( pValuesMax[i-1] < 1000 )
         {
            hPointPrev = hGraph * (float)(pValuesMax[i-1]-(float)iMin) / ((float)iMax-(float)iMin);
            g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar, yPos + hGraph - hPointPrev);
         }
      }

      if ( NULL != pValuesAvg )
      if ( pValuesAvg[i] < 1000 )
      {
         hPoint = hGraph * (float)(pValuesAvg[i]-(float)iMin) / ((float)iMax-(float)iMin);
         g_pRenderEngine->setStroke(100, 100, 250, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(100, 100, 250, s_fOSDStatsGraphLinesAlpha);
         //g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar+fWidthBar, yPos + hGraph - hPoint);
         if ( i > 0 )
         if ( pValuesAvg[i-1] < 1000 )
         {
            hPointPrev = hGraph * (float)(pValuesAvg[i-1]-(float)iMin) / ((float)iMax-(float)iMin);
            //g_pRenderEngine->drawLine(xBar, yPos + hGraph - hPoint, xBar, yPos + hGraph - hPointPrev);
            g_pRenderEngine->drawLine(xBar-fWidthBar, yPos + hGraph - hPointPrev, xBar, yPos + hGraph - hPoint);
         }
      }

      xBar += fWidthBar;
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   return hGraph;
}

float _osd_render_debug_stats_graph_bars(float xPos, float yPos, float hGraph, float fWidth, u8* pValues, u8* pValues2, u8* pValues3, int iCountValues, int iLog)
{
   char szBuff[32];
   float height_text = g_pRenderEngine->textHeight(g_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontStatsSmall);

   int iMax = 0;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( pValues[i] > iMax )
         iMax = pValues[i];
      if ( NULL != pValues2 )
      if ( (int)pValues[i] + (int)pValues2[i] > iMax )
         iMax = (int)pValues[i] + (int)pValues2[i];
      if ( NULL != pValues3 )
      if ( (int)pValues[i] + (int)pValues3[i] > iMax )
         iMax = (int)pValues[i] + (int)pValues3[i];

      if ( NULL != pValues2 )
      if ( NULL != pValues3 )
      if ( (int)pValues[i] + (int)pValues2[i] + (int)pValues3[i] > iMax )
         iMax = (int)pValues[i] + (int)pValues2[i] + (int)pValues3[i];
   }
   float fMaxLog = logf((float)iMax);

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   g_pRenderEngine->drawText(xPos, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, "0");
   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, yPos +hGraph*0.5 - height_text_small*0.5, g_idFontStatsSmall, szBuff);

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   xPos += dx;
   fWidth -= dx;
   float xBar = xPos;
   float fWidthPixel = g_pRenderEngine->getPixelWidth();
   float fHeightPixel = g_pRenderEngine->getPixelHeight();
   float fWidthBar = fWidth / iCountValues;

   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->drawLine(xPos, yPos-g_pRenderEngine->getPixelHeight(), xPos+fWidth, yPos-g_pRenderEngine->getPixelHeight());
   g_pRenderEngine->drawLine(xPos, yPos+hGraph+g_pRenderEngine->getPixelHeight(), xPos+fWidth, yPos+hGraph+g_pRenderEngine->getPixelHeight());

   for( int i=0; i<iCountValues; i++ )
   {
      float hBar1 = hGraph * (float)pValues[i] / (float)iMax;
      float hBar2 = 0;
      float hBar3 = 0;
      if ( NULL != pValues2 )
         hBar2 = hGraph * (float)pValues2[i] / (float)iMax;
      if ( NULL != pValues3 )
         hBar3 = hGraph * (float)pValues3[i] / (float)iMax;

      if ( iLog )
      {
         float fAllSum = pValues[i];
         if ( NULL != pValues2 )
            fAllSum += pValues2[i];
         if ( NULL != pValues3 )
            fAllSum += pValues3[i];

         if ( pValues[i] > 0 )
            hBar1 = hGraph * logf((float)pValues[i]) / fMaxLog;

         if ( NULL != pValues2 )
         if ( pValues2[i] > 0 )
            hBar2 = hGraph * logf(fAllSum) / fMaxLog - hBar1;

         if ( NULL != pValues3 )
         if ( pValues3[i] > 0 )
            hBar3 = hGraph * logf(fAllSum) / fMaxLog - hBar1;

         if ( NULL != pValues2 )
         if ( pValues2[i] > 0 )
         if ( NULL != pValues3 )
         if ( pValues3[i] > 0 )
         {
            float hBar23 = hGraph * logf(fAllSum) / fMaxLog - hBar1;
            hBar2 = hBar23 * pValues2[i]/(pValues2[i] + pValues3[i]);
            hBar3 = hBar23 - hBar2;
         }
      }

      float yBar = yPos + hGraph;
      if ( pValues[i] > 0 )
      {
         yBar -= hBar1;
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, fWidthBar - g_pRenderEngine->getPixelWidth(), hBar1);
      }
      if ( NULL != pValues2 )
      if ( pValues2[i] > 0 )
      {
         yBar -= hBar2;
         g_pRenderEngine->setStroke(0, 200, 0, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(0, 200, 0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, fWidthBar - g_pRenderEngine->getPixelWidth(), hBar2);
      }

      if ( NULL != pValues3 )
      if ( pValues3[i] > 0 )
      {
         yBar -= hBar3;
         g_pRenderEngine->setStroke(0, 0, 250, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(0, 0, 250, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar, yBar, fWidthBar - g_pRenderEngine->getPixelWidth(), hBar3);
      }

      if ( pValues[i] == 0 )
      if ( (NULL == pValues2) || (0 == pValues2[i]) )
      if ( (NULL == pValues3) || (0 == pValues3[i]) )
      {
         g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBar + fWidthBar*0.5 - fWidthPixel, yPos + 0.5*hGraph - fHeightPixel, 2.0*fWidthPixel, 2.0*fHeightPixel);
      }
      xBar += fWidthBar;
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   return hGraph;
}

float _osd_render_debug_stats_graph_values_c(float xPos, float yPos, float hGraph, float fWidth, u8* pValues, int iCountValues, double* pColor1, double* pColor2, double* pColor3, double* pColor4, double* pColor5)
{
   char szBuff[32];
   float height_text = g_pRenderEngine->textHeight(g_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(g_idFontStatsSmall);

   double cRed[] = {220, 0, 0, 1};
   double cRose[] = {250, 130, 130, 1};
   double cBlue[] = {80, 80, 250, 1};
   double cYellow[] = {250, 250, 50, 1};
   double cWhite[] = {250, 250, 250, 1};

   if ( NULL == pColor1 )
      pColor1 = &cRed[0];
   if ( NULL == pColor2 )
      pColor2 = &cRose[0];
   if ( NULL == pColor3 )
      pColor3 = &cBlue[0];
   if ( NULL == pColor4 )
      pColor4 = &cYellow[0];
   if ( NULL == pColor5 )
      pColor5 = &cWhite[0];

   int iMax = 0;
   for( int i=0; i<iCountValues; i++ )
   {
      if ( pValues[i] > iMax )
         iMax = pValues[i];
   }

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   g_pRenderEngine->drawText(xPos, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, "0");
   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   xPos += dx;
   fWidth -= dx;
   float xBar = xPos;
   float fWidthBar = fWidth / iCountValues;

   for( int i=0; i<iCountValues; i++ )
   {
      float hBar = hGraph*0.5;
      float dyBar = 0.0;
      if ( pValues[i] == 0 )
      {
         xBar += fWidthBar;
         continue;
      }
      if ( pValues[i] == 1 )
      {
         g_pRenderEngine->setStroke(pColor1[0], pColor1[1], pColor1[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColor1[0], pColor1[1], pColor1[2], s_fOSDStatsGraphLinesAlpha);
         hBar = hGraph * 0.7;
      }
      else if ( pValues[i] == 2 )
      {
         g_pRenderEngine->setStroke(pColor2[0], pColor2[1], pColor2[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColor2[0], pColor2[1], pColor2[2], s_fOSDStatsGraphLinesAlpha);
         dyBar = 0.5*hGraph;
      }
      else if ( pValues[i] == 3 )
      {
         g_pRenderEngine->setStroke(pColor3[0], pColor3[1], pColor3[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColor3[0], pColor3[1], pColor3[2], s_fOSDStatsGraphLinesAlpha);
         dyBar = 0.5*hGraph;
      }
      else if ( pValues[i] == 4 )
      {
         hBar = hGraph;
         g_pRenderEngine->setStroke(pColor4[0], pColor4[1], pColor4[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColor4[0], pColor4[1], pColor4[2], s_fOSDStatsGraphLinesAlpha);
      }
      else if ( pValues[i] > 4 )
      {
         hBar = hGraph;
         g_pRenderEngine->setStroke(pColor5[0], pColor5[1], pColor5[2], OSD_STRIKE_WIDTH);
         g_pRenderEngine->setFill(pColor5[0], pColor5[1], pColor5[2], s_fOSDStatsGraphLinesAlpha);
      }
      g_pRenderEngine->drawRect(xBar, yPos + hGraph - hBar - dyBar, fWidthBar - g_pRenderEngine->getPixelWidth(), hBar);
      xBar += fWidthBar;
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   return hGraph;
}

float _osd_render_debug_stats_graph_values(float xPos, float yPos, float hGraph, float fWidth, u8* pValues, int iCountValues)
{
   double cRed[] = {220, 0, 0, 1};
   double cRose[] = {250, 130, 130, 1};
   double cBlue[] = {80, 80, 250, 1};
   double cYellow[] = {250, 250, 50, 1};
   double cWhite[] = {250, 250, 250, 1};
   return _osd_render_debug_stats_graph_values_c(xPos, yPos, hGraph, fWidth, pValues, iCountValues, cRed, cRose, cBlue, cYellow, cWhite);
}


float _osd_render_ack_time_hist(controller_runtime_info_vehicle* pRTInfoVehicle, float xPos, float fGraphXStart, float yPos, float hGraph, float fWidthGraph )
{
   char szBuff[128];
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);

   for( int iLink=0; iLink<g_SM_RadioStats.countLocalRadioLinks; iLink++ )
   {
      int iMaxValue = pRTInfoVehicle->uAckTimes[pRTInfoVehicle->iAckTimeIndex[iLink]][iLink];
      int iMinValue = 1000;
      int iAvgValueSum = 0;
      int iAvgCount = 0;
      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS/4; i++ )
      {
         if ( pRTInfoVehicle->uAckTimes[i][iLink] > iMaxValue )
            iMaxValue = pRTInfoVehicle->uAckTimes[i][iLink];
         if ( pRTInfoVehicle->uAckTimes[i][iLink] != 0 )
         {
            iAvgValueSum += pRTInfoVehicle->uAckTimes[i][iLink];
            iAvgCount++;
            if ( pRTInfoVehicle->uAckTimes[i][iLink] < iMinValue )
               iMinValue = pRTInfoVehicle->uAckTimes[i][iLink];
         }
      }

      int iAvgValue = 0;
      if ( iAvgCount > 0 )
         iAvgValue = iAvgValueSum / iAvgCount;
      sprintf(szBuff, "RadioLink-%d Ack Time History: Min: %d ms, Max: %d ms, Avg: %d ms",
         iLink+1, iMinValue, iMaxValue, iAvgValue);
      g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, szBuff);
      yPos += height_text*1.3;

      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      osd_set_colors();
      g_pRenderEngine->setColors(get_Color_Dev());
      g_pRenderEngine->drawText(xPos, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, "0");
      g_pRenderEngine->drawText(xPos + fWidthGraph, yPos+hGraph - height_text_small*0.7, g_idFontStatsSmall, "0");
      sprintf(szBuff, "%d", iMaxValue);
      g_pRenderEngine->drawText(xPos, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos + fWidthGraph, yPos - height_text_small*0.3, g_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d", iMaxValue/2);
      g_pRenderEngine->drawText(xPos, yPos+hGraph*0.5 - height_text_small*0.5, g_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos+fWidthGraph, yPos+hGraph*0.5 - height_text_small*0.5, g_idFontStatsSmall, szBuff);

      float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
      xPos += dx;
      fWidthGraph -= dx;
      float xBar = xPos;
      float fWidthBar = fWidthGraph / (SYSTEM_RT_INFO_INTERVALS/4);

      g_pRenderEngine->setStroke(250,250,250,0.5);
      for( float xx=xPos; xx<xPos+fWidthGraph; xx += 0.02 )
      {
         g_pRenderEngine->drawLine(xx, yPos, xx + 0.008, yPos);
         g_pRenderEngine->drawLine(xx, yPos + hGraph*0.5, xx + 0.008, yPos+hGraph*0.5);
      }

      for( int i=0; i<SYSTEM_RT_INFO_INTERVALS/4; i++ )
      {
         if ( i == (pRTInfoVehicle->iAckTimeIndex[iLink] + 1) )
            g_pRenderEngine->drawLine(xBar + 0.5*fWidthBar, yPos, xBar + 0.5*fWidthBar, yPos + hGraph);
         else
         {
            float hBar = hGraph * (float)pRTInfoVehicle->uAckTimes[i][iLink] / (float)iMaxValue;
            g_pRenderEngine->setStroke(200, 200, 200, OSD_STRIKE_WIDTH);
            g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->drawRect(xBar, yPos + hGraph - hBar, fWidthBar - g_pRenderEngine->getPixelWidth(), hBar);
         }
         xBar += fWidthBar;
      }
      osd_set_colors();
      g_pRenderEngine->setColors(get_Color_Dev());
      yPos += hGraph;
      yPos += height_text_small;
   }
   return yPos;
}

void osd_render_debug_stats()
{
   Preferences* pP = get_Preferences();
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   controller_runtime_info* pCRTInfo = &g_SMControllerRTInfo;
   //vehicle_runtime_info* pVRTInfo = &g_SMVehicleRTInfo;
   if ( s_bDebugStatsControllerInfoFreeze )
   {
       pCRTInfo = &s_ControllerRTInfoFreeze;
       //pVRTInfo = &s_VehicleRTInfoFreeze;
   }
   controller_runtime_info_vehicle* pCRTInfoVehicle = controller_rt_info_get_vehicle_info(pCRTInfo, pActiveModel->uVehicleId);

   int iStartIntervals = 0;
   int iCountIntervals = SYSTEM_RT_INFO_INTERVALS;
   if ( 1 == s_bDebugStatsControllerInfoZoom )
   {
     iCountIntervals = SYSTEM_RT_INFO_INTERVALS/2;
   }
   if ( 2 == s_bDebugStatsControllerInfoZoom )
   {
      iCountIntervals = SYSTEM_RT_INFO_INTERVALS/4;
   }
   int iIndexVehicleStart = iStartIntervals + pCRTInfo->iDeltaIndexFromVehicle;
   if ( iIndexVehicleStart < 0 )
      iIndexVehicleStart += SYSTEM_RT_INFO_INTERVALS;
   if ( iIndexVehicleStart >= SYSTEM_RT_INFO_INTERVALS )
      iIndexVehicleStart -= SYSTEM_RT_INFO_INTERVALS;

   s_idFontStats = g_idFontStats;
   s_idFontStatsSmall = g_idFontStatsSmall;
   s_OSDStatsLineSpacing = 1.0;

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 3.0;
   float hGraphSmall = height_text * 1.6;

   float xPos = 0.03;
   float width = 0.94;

   static float s_fStaticYPosDebugStats = 0.7;
   static float s_fStaticHeightDebugStats = 0.2;

   char szBuff[128];

   int iCountGraphs = 0;

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, s_fStaticYPosDebugStats, width, s_fStaticHeightDebugStats, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());


   float yPos = s_fStaticYPosDebugStats;
   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;

   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float height = s_fStaticHeightDebugStats;
   height -= s_fOSDStatsMargin*0.7*2.0;

   float widthMax = width;
   float rightMargin = xPos + width;

   float fGraphXStart = xPos;
   float fWidthGraph = widthMax;

   float dx = g_pRenderEngine->textWidth(g_idFontStats, "000");
   float xPosSlice = fGraphXStart + dx;
   float fWidthBar = (fWidthGraph-dx) / iCountIntervals;
   float fWidthPixel = g_pRenderEngine->getPixelWidth();

   szBuff[0] = 0;
   Preferences* p = get_Preferences();
   if ( p->iDebugStatsQAButton > 0 )
      sprintf(szBuff, "or QA%d button", p->iDebugStatsQAButton );
   char szTitle[128];
   snprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), "Debug Stats  (press [Cancel] %s to exit stats, [Up]/[Down] to zoom, any other [QA] to freeze", szBuff);
   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, szTitle);
   
   sprintf(szBuff, "%d %u ms", g_SMControllerRTInfo.iCurrentIndex, g_SMControllerRTInfo.uCurrentSliceStartTime);
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStatsSmall, szBuff);
   float y = yPos;
   y += 1.5*height_text*s_OSDStatsLineSpacing;

   float yTop = y;

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(180,180,180, OSD_STRIKE_WIDTH);

   for( float yLine=yTop; yLine<=yPos+height-0.05; yLine += 0.05 )
   {
      for( float dxLine=0.1; dxLine<1.0; dxLine += 0.1 )
         g_pRenderEngine->drawLine(fGraphXStart+dx + fWidthGraph*dxLine, yLine, fGraphXStart+dx + fWidthGraph*dxLine, yLine + 0.03);
   }
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());


   u8 uTmp[SYSTEM_RT_INFO_INTERVALS];
   u8 uTmp1[SYSTEM_RT_INFO_INTERVALS];
   u8 uTmp2[SYSTEM_RT_INFO_INTERVALS];
   u8 uTmp3[SYSTEM_RT_INFO_INTERVALS];
   u8 uTmp4[SYSTEM_RT_INFO_INTERVALS];

   int iTmp1[SYSTEM_RT_INFO_INTERVALS];
   int iTmp2[SYSTEM_RT_INFO_INTERVALS];
   int iTmp3[SYSTEM_RT_INFO_INTERVALS];
   int iTmp4[SYSTEM_RT_INFO_INTERVALS];

   //--------------------------------------------
   /*
   int iVehicleIndex = iIndexVehicleStart;
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pVRTInfo->uSentVideoDataPackets[iVehicleIndex];
      uTmp2[i] = pVRTInfo->uSentVideoECPackets[iVehicleIndex];
      iVehicleIndex++;
      if ( iVehicleIndex >= SYSTEM_RT_INFO_INTERVALS )
         iVehicleIndex = 0;
   }

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Vehicle Tx Video Packets");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, uTmp2, NULL, iCountIntervals, 1);
   y += height_text_small;
   /**/

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_TX_PACKETS )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = 0;
         uTmp2[i] = 0;
         uTmp3[i] = 0;
         for( int k=0; k<hardware_get_radio_interfaces_count(); k++ )
         {
            uTmp[i] += pCRTInfo->uRxVideoPackets[i+iStartIntervals][k];
            uTmp2[i]+= 5*pCRTInfo->uRxDataPackets[i+iStartIntervals][k];
            uTmp3[i]+= 5*pCRTInfo->uRxHighPriorityPackets[i+iStartIntervals][k];
         }
         if ( uTmp3[i] > 0 )
         if ( uTmp3[i] < uTmp[i]/4 )
            uTmp3[i] = uTmp[i]/4;
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Rx Video/Data/High Priority Packets");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, uTmp2, uTmp3, iCountIntervals, 1);
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_TX_HIGH_REG_PACKETS )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfo->uTxPackets[i+iStartIntervals];
         uTmp2[i] = 0;
         uTmp3[i] = pCRTInfo->uTxHighPriorityPackets[i+iStartIntervals];
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Tx Regular/High Priority Packets");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, uTmp2, uTmp3, iCountIntervals, 1);
      y += height_text_small;
      iCountGraphs++;
   }


   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_AIR_GAPS )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfo->uRxMaxAirgapSlots[i+iStartIntervals];
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Rx Max air gaps");
      y += height_text*1.3;
      //y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 0);
      y += height_text_small;
      iCountGraphs++;

      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfo->uRxMaxAirgapSlots2[i+iStartIntervals];
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Rx Max air gaps (2)");
      y += height_text*1.3;
      //y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 0);
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_H264265_FRAMES )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         /*
         uTmp[i] = 0;
         if ( pCRTInfo->uRecvFramesInfo[i+iStartIntervals] & 0b01 )
           uTmp[i] = 1;
         if ( pCRTInfo->uRecvFramesInfo[i+iStartIntervals] & 0b10 )
           uTmp[i] = 2;
         */
         if ( pCRTInfo->uRecvFramesInfo[i+iStartIntervals] & 0b1000000 )
           uTmp[i] = 3;
         else if ( pCRTInfo->uRecvFramesInfo[i+iStartIntervals] & 0b100000 )
           uTmp[i] = 2;
         else if ( pCRTInfo->uRecvFramesInfo[i+iStartIntervals] & 0b10000 )
           uTmp[i] = 1;
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Received P/I frames");
      y += height_text*1.3;

      double cRed[] = {220, 0, 0, 1};
      double cBlue[] = {80, 80, 250, 1};
      double cWhite[] = {250, 250, 250, 1};

      y += _osd_render_debug_stats_graph_values_c(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals, cRed, cBlue, cWhite, NULL, NULL);
      y += height_text_small;
      iCountGraphs++;
   }

   //-----------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_DBM )
   {
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++)
   {
   for( int iAnt=0; iAnt<pCRTInfo->radioInterfacesDbm[0][iInt].iCountAntennas; iAnt++ )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         iTmp1[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmLast[iAnt];
         iTmp2[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmMin[iAnt];
         iTmp3[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmMax[iAnt];
         iTmp4[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmAvg[iAnt];
      }
      sprintf(szBuff, "Dbm (interface %d, antenna: %d)", iInt+1, iAnt+1);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Dbm");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_lines(fGraphXStart, y, hGraph, fWidthGraph, iTmp1, iTmp2, iTmp3, iTmp4, iCountIntervals);
      y += height_text_small;
   }
   }
   iCountGraphs++;
   }

   //-----------------------------------------------
   /*
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++)
   {
   for( int iAnt=0; iAnt<pCRTInfo->radioInterfacesDbm[0][iInt].iCountAntennas; iAnt++ )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         iTmp2[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmChangeSpeedMin[iAnt];
         iTmp3[i] = pCRTInfo->radioInterfacesDbm[i+iStartIntervals][iInt].iDbmChangeSpeedMax[iAnt];
      }
      sprintf(szBuff, "Dbm Change Speed (interface %d, antenna: %d)", iInt+1, iAnt+1);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Dbm");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_lines(fGraphXStart, y, hGraph, fWidthGraph, NULL, iTmp2, iTmp3, NULL, iCountIntervals);
      y += height_text_small;
   }
   }
   */

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS )
   {
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++)
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfo->uRxMissingPackets[i+iStartIntervals][iInt];
      }
      sprintf(szBuff, "Rx Missing Packets (interface %d)", iInt+1);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
      y += height_text_small;
   }
   iCountGraphs++;
   }

   //--------------------------------------------------------------------------

   if ( iCountGraphs >=3 )
   {
   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->setStroke(255,255,255, 1.0);
   xPosSlice = fGraphXStart + dx;
   for( int i=0; i<iCountIntervals; i++ )
   {
      xPosSlice += fWidthBar;
      g_pRenderEngine->drawLine(xPosSlice + fWidthBar*0.5, y, xPosSlice + fWidthBar*0.5, y + height_text_small);
      if ( 0 != s_bDebugStatsControllerInfoZoom )
      {
         g_pRenderEngine->drawLine(xPosSlice + fWidthBar*0.5 + fWidthPixel, y, xPosSlice + fWidthBar*0.5 + fWidthPixel, y + height_text_small);
         g_pRenderEngine->drawLine(xPosSlice + fWidthBar*0.5 + fWidthPixel*2.0, y, xPosSlice + fWidthBar*0.5 + fWidthPixel*2.0, y + height_text_small);
      }
   }
   y += height_text_small;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_MISSING_PACKETS_MAX_GAP )
   {
   for( int iInt=0; iInt<hardware_get_radio_interfaces_count(); iInt++)
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfo->uRxMissingPacketsMaxGap[i+iStartIntervals][iInt];
      }
      sprintf(szBuff, "Rx Missing Packets Max Gap (interface %d)", iInt+1);
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 0);
      y += height_text_small;
   }
   iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_CONSUMED_PACKETS )
   {
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uRxProcessedPackets[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Rx Consumed Packets");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraph, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 1);
   y += height_text_small;
   iCountGraphs++;
   }

   //--------------------------------------------
   /*
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoPackets[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Outputed clean video packets");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 1);
   y += height_text_small;
   */

   //--------------------------------------------
   /*
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoPacketsSingleECUsed[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Outputed video with single EC used");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
   y += height_text_small;

   //--------------------------------------------
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoPacketsTwoECUsed[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Outputed video with two EC used");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
   y += height_text_small;

   //--------------------------------------------
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoPacketsMultipleECUsed[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Outputed video with more than two EC used");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
   y += height_text_small;
   */

   // ----------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_MIN_MAX_ACK_TIME )
   {   
      controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(pCRTInfo, pActiveModel->uVehicleId);
      for( int iLink=0; iLink<g_SM_RadioStats.countLocalRadioLinks; iLink++ )
      {
         xPosSlice = fGraphXStart + dx;
         for( int i=0; i<iCountIntervals; i++ )
         {
            iTmp1[i] = pRTInfoVehicle->uMinAckTime[i+iStartIntervals][iLink];  
            iTmp2[i] = pRTInfoVehicle->uMaxAckTime[i+iStartIntervals][iLink];
            if ( iTmp2[i] > 0 )
            {
               g_pRenderEngine->setColors(get_Color_OSDText(), 0.3);
               g_pRenderEngine->drawRect(xPosSlice, yTop, fWidthBar, 0.9 - yTop);
               osd_set_colors();
            }
            xPosSlice += fWidthBar;
         }
         sprintf(szBuff, "RadioLink-%d Min/Max Ack Time (ms)", iLink+1);
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         y += height_text*1.3;
         y += _osd_render_debug_stats_graph_lines(fGraphXStart, y, hGraphSmall*1.3, fWidthGraph, NULL, iTmp1, iTmp2, NULL, iCountIntervals);
         y += height_text_small;
         iCountGraphs++;
      }
   }

   // ----------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_ACK_TIME_HISTORY )
   {
      controller_runtime_info_vehicle* pRTInfoVehicle = controller_rt_info_get_vehicle_info(pCRTInfo, pActiveModel->uVehicleId);
      y = _osd_render_ack_time_hist(pRTInfoVehicle, xPos, fGraphXStart, y, hGraphSmall, fWidthGraph*0.5 );
      iCountGraphs++;
   }

   // ----------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_MAX_EC_USED )
   {
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoPacketsMaxECUsed[i+iStartIntervals];  
   }
   sprintf(szBuff, "Outputed video EC max packets used ( %d / %d )", pActiveModel->video_link_profiles[pActiveModel->video_params.user_selected_video_link_profile].block_packets, pActiveModel->video_link_profiles[pActiveModel->video_params.user_selected_video_link_profile].block_fecs);
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
   y += height_text_small;
   iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_RX_VIDEO_UNRECOVERABLE_BLOCKS )
   {
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uOutputedVideoPacketsSkippedBlocks[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Unrecoverable Video Blocks skipped");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_values(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, iCountIntervals);
   y += height_text_small;
   iCountGraphs++;
   }

   //--------------------------------------------
   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_RETRANSMISSIONS )
   {
      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfoVehicle->uCountReqRetransmissions[i+iStartIntervals];
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Requested Retransmissions");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 0);
      y += height_text_small;

      for( int i=0; i<iCountIntervals; i++ )
      {
         uTmp[i] = pCRTInfoVehicle->uCountAckRetransmissions[i+iStartIntervals];
      }
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Ack Retransmissions");
      y += height_text*1.3;
      y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraphSmall, fWidthGraph, uTmp, NULL, NULL, iCountIntervals, 0);
      y += height_text_small;
      iCountGraphs++;
   }

   //--------------------------------------------
   /*
   for( int i=0; i<iCountIntervals; i++ )
   {
      uTmp[i] = pCRTInfo->uRecvVideoDataPackets[i+iStartIntervals];
      uTmp2[i] = pCRTInfo->uRecvVideoECPackets[i+iStartIntervals];
   }
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Rx Video Decode Data/EC");
   y += height_text*1.3;
   y += _osd_render_debug_stats_graph_bars(fGraphXStart, y, hGraph, fWidthGraph, uTmp, uTmp2, NULL, iCountIntervals, 1);
   y += height_text_small;
   /**/
   //------------------------------------------------


   // ----------------------------------------------
   float fStrokeSize = 2.0;
   g_pRenderEngine->setStrokeSize(fStrokeSize);
   float yMid = 0.5*(y+yTop);
   xPosSlice = fGraphXStart + dx;

   if ( pP->uDebugStatsFlags & CTRL_RT_DEBUG_INFO_FLAG_SHOW_VIDEO_PROFILE_CHANGES )
   {
   for( int i=0; i<iCountIntervals; i++ )
   {
      u32 uFlags = pCRTInfo->uFlagsAdaptiveVideo[i+iStartIntervals];
      if ( 0 == uFlags )
      {
         xPosSlice += fWidthBar;
         continue;
      }

      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_USER_SELECTABLE )
      {
         g_pRenderEngine->setStroke(80, 80, 250, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawRect(xPosSlice - fWidthPixel, yTop, fWidthBar + 2.0*fWidthPixel, y - yTop);
      }
      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_LOWER )
      {
         g_pRenderEngine->setStroke(250, 80, 80, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawRect(xPosSlice - fWidthPixel, yTop, fWidthBar + 2.0*fWidthPixel, y - yTop);
      }
      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCHED_HIGHER )
      {
         g_pRenderEngine->setStroke(80, 250, 80, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawRect(xPosSlice - fWidthPixel, yTop, fWidthBar + 2.0*fWidthPixel, y - yTop);
      }

      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_USER )
      {
         g_pRenderEngine->setStroke(80, 80, 250, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawLine(xPosSlice - fWidthPixel, yMid, xPosSlice - fWidthPixel, y);
         g_pRenderEngine->drawLine(xPosSlice, yMid, xPosSlice, y);
      }
      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_ADAPTIVE_LOWER )
      {
         g_pRenderEngine->setStroke(250, 80, 80, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawLine(xPosSlice - fWidthPixel, yMid, xPosSlice - fWidthPixel, y);
         g_pRenderEngine->drawLine(xPosSlice, yMid, xPosSlice, y);
      }
      if ( uFlags & CTRL_RT_INFO_FLAG_VIDEO_PROF_SWITCH_REQ_BY_ADAPTIVE_HIGHER )
      {
         g_pRenderEngine->setStroke(80, 250, 80, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawLine(xPosSlice - fWidthPixel, yMid, xPosSlice - fWidthPixel, y);
         g_pRenderEngine->drawLine(xPosSlice, yMid, xPosSlice, y);
      }

      if ( uFlags & CTRL_RT_INFO_FLAG_RECV_ACK )
      {
         g_pRenderEngine->setStroke(50, 50, 250, fStrokeSize);
         g_pRenderEngine->setFill(0, 0, 0, 0);
         g_pRenderEngine->drawLine(xPosSlice + fWidthPixel, yMid, xPosSlice + fWidthPixel, y);
         g_pRenderEngine->drawLine(xPosSlice + fWidthPixel*2, yMid, xPosSlice + fWidthPixel*2, y);
      }

      xPosSlice += fWidthBar;
   }
   iCountGraphs++;
   }
   
   float xLine = fGraphXStart + dx + pCRTInfo->iCurrentIndex * fWidthBar;
   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->setStroke(255,255,100, OSD_STRIKE_WIDTH);
   g_pRenderEngine->drawLine(xLine, yPos, xLine, yPos+height);
   g_pRenderEngine->drawLine(xLine+g_pRenderEngine->getPixelWidth(), yPos, xLine + g_pRenderEngine->getPixelWidth(), yPos+height);
   osd_set_colors();

   s_fStaticHeightDebugStats = y - s_fStaticYPosDebugStats + s_fOSDStatsMargin*0.7;
   s_fStaticYPosDebugStats = 0.9 - s_fStaticHeightDebugStats;
}