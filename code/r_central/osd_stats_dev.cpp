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
#include "../common/string_utils.h"
#include <math.h>
#include "osd_stats_dev.h"
#include "osd_common.h"
#include "colors.h"
#include "shared_vars.h"
#include "timers.h"

extern float s_OSDStatsLineSpacing;
extern float s_fOSDStatsMargin;
extern float s_fOSDStatsGraphLinesAlpha;
extern u32 s_idFontStats;
extern u32 s_idFontStatsSmall;



float osd_render_stats_adaptive_video_get_height()
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 2.0;
   
   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;

   if ( NULL == g_pCurrentModel || NULL == g_pSM_ControllerVehiclesAdaptiveVideoInfo )
   {
      height += height_text*s_OSDStatsLineSpacing;
      return height;
   }

   height += 6.0*height_text*s_OSDStatsLineSpacing;

   height += 2.0 * hGraph;

   height += 9.0*height_text*s_OSDStatsLineSpacing;

   height += 2.0*height_text*s_OSDStatsLineSpacing;

   return height;
}

float osd_render_stats_adaptive_video_get_width()
{
   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   return width;
}

void osd_render_stats_adaptive_video(float xPos, float yPos)
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 2.0;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float fStroke = OSD_STRIKE_WIDTH;

   float width = osd_render_stats_adaptive_video_get_width();
   float height = osd_render_stats_adaptive_video_get_height();

   int iIndexVehicle = 0;

   char szBuff[128];
   char szBuff1[64];
   char szBuff2[64];

   if ( NULL == g_pSM_ControllerVehiclesAdaptiveVideoInfo )
      return;

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, "Ctrl Adaptive Video Stats");
   
   snprintf(szBuff, sizeof(szBuff), "%u ms/bar", g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uUpdateInterval);
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStatsSmall, szBuff);
   

   int iIntervalsDown = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame1 & 0xFFFF;
   int iIntervalsUp = (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame1 >> 16) & 0xFFFF;

   int iIntervalsAdaptiveDown = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsAdaptive1 & 0xFFFF;
   int iIntervalsAdaptiveUp = (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsAdaptive1 >> 16) & 0xFFFF;

   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;
   float yTop = y;

   double* pc = get_Color_Dev();
   g_pRenderEngine->setColors(get_Color_Dev());

   float dxGraph = g_pRenderEngine->textWidth(0.0, s_idFontStatsSmall, "8878");
   float widthGraph = widthMax - dxGraph;
   
   // ---------------------------------------------------------------
   // Graph output ok/partial

   g_pRenderEngine->drawText(xPos, y-height_text_small*0.3, s_idFontStatsSmall, "Video packets output ok/partial/missing:");
   y += height_text*s_OSDStatsLineSpacing;

   float yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
   
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);

   int iValuesToShow = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS;
   float widthBar = widthGraph / iValuesToShow;

   int iMaxValue = 0;
   int iIndex = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uCurrentIntervalIndex;
   for( int i=0; i<iValuesToShow; i++ )
   {
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;
   
       if ( g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsOuputCleanVideoPackets[iIndex] + g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsOuputRecontructedVideoPackets[iIndex]*3 + g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsMissingVideoPackets[iIndex] > iMaxValue )
          iMaxValue = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsOuputCleanVideoPackets[iIndex] + g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsOuputRecontructedVideoPackets[iIndex]*3 + g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsMissingVideoPackets[iIndex];
   }
   if ( iMaxValue == 0 )
      iMaxValue = 1;
   snprintf(szBuff, sizeof(szBuff), "%d", iMaxValue);
   g_pRenderEngine->drawText(xPos, y - height_text_small*0.6, s_idFontStats, szBuff);
   g_pRenderEngine->drawText(xPos, y + hGraph - height_text_small*0.6, s_idFontStats, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);


   float xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   iIndex = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uCurrentIntervalIndex;
   for( int i=0; i<iValuesToShow; i++ )
   {
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;

      float hBarOk = hGraph * (float)g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsOuputCleanVideoPackets[iIndex]/(float)iMaxValue;
      float hBarRec = hGraph * (float)g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsOuputRecontructedVideoPackets[iIndex]*3/(float)iMaxValue;
      float hBarMissing = hGraph * (float)g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsMissingVideoPackets[iIndex]/(float)iMaxValue;
 
      if ( g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsOuputCleanVideoPackets[iIndex] > 0 )
      {
         g_pRenderEngine->setStroke(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(200, 200, 200, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
         g_pRenderEngine->drawRect(xBarSt, y+hGraph - hBarOk, widthBar, hBarOk);
      }

      if ( g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsOuputRecontructedVideoPackets[iIndex] > 0 )
      {
         g_pRenderEngine->setStroke(0, 200, 0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(0, 200, 0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
         g_pRenderEngine->drawRect(xBarSt, y+hGraph - hBarOk - hBarRec, widthBar, hBarRec);
      }

      if ( g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsMissingVideoPackets[iIndex] > 0 )
      {
         g_pRenderEngine->setStroke(250, 50, 50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(250, 50, 50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
         g_pRenderEngine->drawRect(xBarSt, y+hGraph - hBarOk - hBarRec - hBarMissing, widthBar, hBarMissing);
      }

      if ( i == iIntervalsUp )
      {
         g_pRenderEngine->setStroke(50, 250, 50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(50, 250, 50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(2.1);
         g_pRenderEngine->drawLine(xBarSt, y, xBarSt, y+hGraph);
      }
      if ( i == iIntervalsDown )
      {
         g_pRenderEngine->setStroke(250, 50, 50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(250, 50, 50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(2.1);
         g_pRenderEngine->drawLine(xBarSt, y, xBarSt, y+hGraph);
      }

      if ( i == iIntervalsAdaptiveUp )
      {
         g_pRenderEngine->setStroke(50, 50, 250, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(50, 50, 250, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(2.1);
         g_pRenderEngine->drawLine(xBarSt, y+hGraph-height_text*0.2, xBarSt, y+hGraph+height_text*0.5);
      }
      if ( i == iIntervalsAdaptiveDown )
      {
         g_pRenderEngine->setStroke(250, 200, 50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(250, 200, 50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(2.1);
         g_pRenderEngine->drawLine(xBarSt, y+hGraph-height_text*0.2, xBarSt, y+hGraph+height_text*0.5);
      }
      xBarSt -= widthBar;
   }

   y += hGraph + height_text_small;

   g_pRenderEngine->setColors(get_Color_Dev());
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.3, s_idFontStatsSmall, "Retransmissions/ Retried retr:");
   y += height_text*s_OSDStatsLineSpacing;

   
   iMaxValue = 0;
   iIndex = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uCurrentIntervalIndex;
   for( int i=0; i<iValuesToShow; i++ )
   {
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;
   
       if ( g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsRequestedRetransmissions[iIndex] + g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsRetriedRetransmissions[iIndex] > iMaxValue )
          iMaxValue = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsRequestedRetransmissions[iIndex] + g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsRetriedRetransmissions[iIndex];
   }

   if ( iMaxValue == 0 )
      iMaxValue = 1;

   yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
   
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setColors(get_Color_Dev());
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);

   snprintf(szBuff, sizeof(szBuff), "%d", iMaxValue);
   g_pRenderEngine->drawText(xPos, y - height_text_small*0.6, s_idFontStats, szBuff);
   g_pRenderEngine->drawText(xPos, y + hGraph - height_text_small*0.6, s_idFontStats, "0");
   
   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   iIndex = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uCurrentIntervalIndex;
   for( int i=0; i<iValuesToShow; i++ )
   {
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;

      float hBarReq = hGraph * (float)g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsRequestedRetransmissions[iIndex]/(float)iMaxValue;
      float hBarRetry = hGraph * (float)g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsRetriedRetransmissions[iIndex]/(float)iMaxValue;
      
      if ( g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsRequestedRetransmissions[iIndex] > 0 )
      {
         g_pRenderEngine->setStroke(100, 200, 250, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(100, 200, 250, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
         g_pRenderEngine->drawRect(xBarSt, y+hGraph - hBarReq, widthBar, hBarReq);
      }

      if ( g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsRetriedRetransmissions[iIndex] > 0 )
      {
         g_pRenderEngine->setStroke(0, 0,250, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(0, 0, 250, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
         g_pRenderEngine->drawRect(xBarSt, y+hGraph - hBarReq - hBarRetry, widthBar, hBarRetry);
      }

      xBarSt -= widthBar;
   }

   y += hGraph + 0.5*height_text_small;

   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setColors(get_Color_Dev());
   
   
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Change strength:");
   snprintf(szBuff, sizeof(szBuff), "%d", g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].iChangeStrength);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Last update time:");
   if ( g_TimeNow >= g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uLastUpdateTime )
      snprintf(szBuff, sizeof(szBuff), "%u ms ago", g_TimeNow - g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uLastUpdateTime );
   else if ( g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uLastUpdateTime != 0 )
      strcpy(szBuff, "[0] ms ago");
   else
      strcpy(szBuff, "Never");
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   y += 0.25 * height_text*s_OSDStatsLineSpacing;
   g_pRenderEngine->drawLine(xPos+0.02, y, rightMargin-0.02, y);
   y += 0.25 * height_text*s_OSDStatsLineSpacing;

   if ( g_TimeNow < g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uTimeLastRequestedKeyFrame + 500 )
      g_pRenderEngine->setColors(get_Color_IconError());
      
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "KF Last Change Time:");
   if ( 0 == g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uTimeLastRequestedKeyFrame )
      strcpy(szBuff, "Never");
   else if ( g_TimeNow < g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uTimeLastRequestedKeyFrame )
      strcpy(szBuff, "[0] ms ago");
   else if ( g_TimeNow - g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uTimeLastRequestedKeyFrame < 5000 )
      snprintf(szBuff, sizeof(szBuff), "%u ms ago", g_TimeNow - g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uTimeLastRequestedKeyFrame );
   else
      snprintf(szBuff, sizeof(szBuff), "%u s ago", (g_TimeNow - g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uTimeLastRequestedKeyFrame) / 1000 );
   
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "KF Last Changed Value:");
   snprintf(szBuff, sizeof(szBuff), "%d", g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].iLastRequestedKeyFrame);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->setColors(get_Color_Dev());

   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, "KF Lookup Intervals U/D:");
   snprintf(szBuff, sizeof(szBuff), "%u / %u of %u", (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame1 >> 16 ) & 0xFFFF, g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame1 & 0xFFFF, MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "KF current U/D values:");
   snprintf(szBuff, sizeof(szBuff), "%u / %u, %u", g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uCurrentKFMeasuredThUp1, g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uCurrentKFMeasuredThDown1, g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uCurrentKFMeasuredThDown2);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "KF Thresholds Move Up:");
   if ( iIntervalsUp > 0 )
      snprintf(szBuff, sizeof(szBuff), "%u (%d%%)", g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame2 & 0xFF, ((int)(g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame2 & 0xFF))*100/iIntervalsUp );
   else
      snprintf(szBuff, sizeof(szBuff), "%u", g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame2 & 0xFF );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, "KF Thresholds Move Down:");
   snprintf(szBuff, sizeof(szBuff), "%u / %u / %u / %u",
      (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame2 >> 8) & 0xFF,
      (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame2 >> 16) & 0xFF,
      (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame3 ) & 0xFF,
      (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsKeyFrame3 >> 8) & 0xFF );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;


   y += 0.25 * height_text*s_OSDStatsLineSpacing;
   g_pRenderEngine->drawLine(xPos+0.02, y, rightMargin-0.02, y);
   y += 0.25 * height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Video Lookup Intv. U/D:");
   int adaptiveVideoIsOn = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
   if ( adaptiveVideoIsOn )
      snprintf(szBuff, sizeof(szBuff), "%u / %u of %d", (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsAdaptive1 >> 16 ) & 0xFFFF, g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsAdaptive1 & 0xFFFF, MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS );
   else
      strcpy(szBuff, "Disabled");
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Video Thresholds Up:");
   if ( adaptiveVideoIsOn )
      snprintf(szBuff, sizeof(szBuff), "%u, %u", (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsAdaptive2 & 0xFF ), (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsAdaptive2 >> 8) & 0xFF );
   else
      strcpy(szBuff, "Disabled");
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Video Thresholds Down:");
   if ( adaptiveVideoIsOn )
      snprintf(szBuff, sizeof(szBuff), "%u, %u", (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsAdaptive2>>16) & 0xFF, (g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uIntervalsAdaptive2 >> 24) & 0xFF );
   else
      strcpy(szBuff, "Disabled");
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;


   if ( g_TimeNow < g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uTimeLastRequestedLevelShift + 700 )
      g_pRenderEngine->setColors(get_Color_IconError());

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Last requested:");
   u32 dtime = 0;
   if ( g_TimeNow > g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uTimeLastRequestedLevelShift)
      dtime = g_TimeNow - g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].uTimeLastRequestedLevelShift;
   
   int iLevelsHQ = g_pCurrentModel->get_video_link_profile_max_level_shifts(g_pCurrentModel->video_params.user_selected_video_link_profile);
   int iLevelsMQ = g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_MQ);
   int iLevelsLQ = g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_LQ);
   int iShift = 0;
   char szTmp[32];
   strcpy(szTmp, "N/A");
   if ( g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].iLastRequestedLevelShift <= iLevelsHQ )
   {
      iShift = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].iLastRequestedLevelShift;
      strcpy(szTmp, "HQ");
      if ( iShift > 0 )
         snprintf(szTmp, sizeof(szTmp), "HQ-%d", iShift);
   }
   else if ( g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].iLastRequestedLevelShift <= iLevelsHQ+iLevelsMQ+1 )
   {
      iShift = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].iLastRequestedLevelShift-(iLevelsHQ+1);
      strcpy(szTmp, "MQ");
      if ( iShift > 0 )
         snprintf(szTmp, sizeof(szTmp), "MQ-%d", iShift);
   }
   else
   {
      iShift = g_pSM_ControllerVehiclesAdaptiveVideoInfo->vehicles[iIndexVehicle].iLastRequestedLevelShift-(iLevelsHQ+iLevelsLQ+2);
      strcpy(szTmp, "LQ");
      if ( iShift > 0 )
         snprintf(szTmp, sizeof(szTmp), "LQ-%d", iShift);
   }
   if ( dtime < 2000 )
      snprintf(szBuff, sizeof(szBuff), "%s %u ms ago", szTmp, dtime);
   else
      snprintf(szBuff, sizeof(szBuff), "%s %u sec ago", szTmp, dtime/1000);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->setColors(get_Color_Dev());

   static u32 s_uTimeOSDStatsDevLastVideoLevelChange = 0;
   static u32 s_uLastOSDStatsDevVideoProfileReceived = 0;
   if ( s_uLastOSDStatsDevVideoProfileReceived != (g_psmvds->video_link_profile & 0x0F) )
   {
      s_uLastOSDStatsDevVideoProfileReceived = (g_psmvds->video_link_profile & 0x0F);
      s_uTimeOSDStatsDevLastVideoLevelChange = g_TimeNow;
   }

   if ( g_TimeNow < s_uTimeOSDStatsDevLastVideoLevelChange + 700 )
      g_pRenderEngine->setColors(get_Color_IconError());

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Current level:");
   strcpy(szBuff, "N/A");

   if ( NULL != g_psmvds && NULL != g_pCurrentModel )
   {
      strcpy(szBuff, str_get_video_profile_name(g_psmvds->video_link_profile & 0x0F));
      int diffEC = g_psmvds->fec_packets_per_block - g_pCurrentModel->video_link_profiles[g_psmvds->video_link_profile & 0x0F].block_fecs;
      if ( diffEC > 0 )
      {
         char szTmp[16];
         snprintf(szTmp, sizeof(szTmp), "-%d", diffEC);
         strlcat(szBuff, szTmp, sizeof(szBuff));
      }
      else if ( g_psmvds->encoding_extra_flags & ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE )
         strlcat(szBuff, "-", sizeof(szBuff));
   }
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

}


float osd_render_stats_adaptive_video_graph_get_height()
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 5.0;

   float height = hGraph + height_text_small;
   return height;
}

float osd_render_stats_adaptive_video_graph_get_width()
{
   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   return width;
}

void osd_render_stats_adaptive_video_graph(float xPos, float yPos)
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 5.0;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float fStroke = OSD_STRIKE_WIDTH;

   float width = osd_render_stats_video_stats_get_width();
   float height = osd_render_stats_video_stats_get_height();
   float y = yPos;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   char szBuff[128];
   char szBuff1[64];
   char szBuff2[64];

   float dxGraph = g_pRenderEngine->textWidth(0.0, s_idFontStatsSmall, "8878");
   float widthGraph = widthMax - dxGraph*2;
   float widthBar = widthGraph / MAX_INTERVALS_VIDEO_LINK_SWITCHES;
   float xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   float yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
   double* pc = get_Color_Dev();

   g_pRenderEngine->setColors(get_Color_Dev());

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();

   int nLevels = 0;
   int lev1 = g_pCurrentModel->get_video_link_profile_max_level_shifts(g_pCurrentModel->video_params.user_selected_video_link_profile);
   if ( lev1 < 0 ) lev1 = 0;
   nLevels += 1 + lev1;
   int lev2 = g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_MQ);
   if ( lev2 < 0 ) lev2 = 0;
   nLevels += 1 + lev2;
   int lev3 = g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_LQ);
   if ( lev3 < 0 ) lev3 = 0;
   nLevels += 1 + lev3;
   float hLevel = hGraph / (float)nLevels;

   strcpy(szBuff, str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile));
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, height_text_small, s_idFontStatsSmall, szBuff);

   float hL1 = hLevel*(1 + lev1);
   strcpy(szBuff, str_get_video_profile_name(VIDEO_PROFILE_MQ));
   g_pRenderEngine->drawText(xPos, y+hL1-height_text_small*0.2, height_text_small, s_idFontStatsSmall,szBuff);

   float hL2 = hL1 + hLevel*(1 + lev2);
   strcpy(szBuff, str_get_video_profile_name(VIDEO_PROFILE_LQ));
   g_pRenderEngine->drawText(xPos, y+hL2-height_text_small*0.2, height_text_small, s_idFontStatsSmall,szBuff);

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   //g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   //g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStroke(20, 255, 80, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(20, 255, 80, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hL1, xPos + dxGraph + widthGraph, y+hL1);
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hL2, xPos + dxGraph + widthGraph, y+hL2);

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);

   //for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   //   g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hL1*0.5, xPos + dxGraph + i + 2.0*wPixel, y+hL1*0.5);

   //for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   //   g_pRenderEngine->drawLine(xPos+dxGraph+i, y+(hL1+hL2)*0.5, xPos + dxGraph + i + 2.0*wPixel, y+(hL1+hL2)*0.5);

   //for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   //   g_pRenderEngine->drawLine(xPos+dxGraph+i, y+(hL2+hGraph)*0.5, xPos + dxGraph + i + 2.0*wPixel, y+(hL2+hGraph)*0.5);

   for( int k=1; k<nLevels; k++ )
      if ( k != lev1+1 && k != (lev1+lev2)+2 )
         for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
            g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hLevel*k, xPos + dxGraph + i + 2.0*wPixel, y+hLevel*k);
   
   int nProfile = g_pSM_VideoLinkStats->historySwitches[0]>>4;
   int nLevelShift = g_pSM_VideoLinkStats->historySwitches[0] & 0x0F;
   int nLevel = 0;
   if ( nProfile == g_pCurrentModel->video_params.user_selected_video_link_profile )
      nLevel = nLevelShift;
   else if ( nProfile == VIDEO_PROFILE_MQ )
      nLevel = lev1 + nLevelShift + 1;
   else if ( nProfile == VIDEO_PROFILE_LQ )
      nLevel = lev1 + lev2 + nLevelShift + 2;
   float hPointPrev = hLevel * nLevel;

   g_pRenderEngine->setStroke(250,230,150, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStrokeSize(2.0);

   for( int i=1; i<MAX_INTERVALS_VIDEO_LINK_SWITCHES; i++ )
   {
      nProfile = g_pSM_VideoLinkStats->historySwitches[i]>>4;
      nLevelShift = g_pSM_VideoLinkStats->historySwitches[i] & 0x0F;
      nLevel = 0;
      if ( nProfile == g_pCurrentModel->video_params.user_selected_video_link_profile )
         nLevel = nLevelShift;
      else if ( nProfile == VIDEO_PROFILE_MQ )
         nLevel = lev1 + nLevelShift + 1;
      else if ( nProfile == VIDEO_PROFILE_LQ )
         nLevel = lev1 + lev2 + nLevelShift + 2;
      float hPoint = hLevel * nLevel;

      g_pRenderEngine->drawLine(xBarSt, y+hPoint, xBarSt+widthBar, y+hPointPrev);

      xBarSt -= widthBar;
      hPointPrev = hPoint;
   }

   g_pRenderEngine->setStrokeSize(0);
   osd_set_colors();
}


float osd_render_stats_video_stats_get_height()
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   
   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;

   if ( NULL == g_pCurrentModel || NULL == g_pSM_VideoLinkStats || 0 == g_pSM_VideoLinkStats->timeLastStatsCheck )
   {
      height += height_text*s_OSDStatsLineSpacing;
      return height;
   }

   height += 18.0*height_text*s_OSDStatsLineSpacing;

   height += osd_render_stats_adaptive_video_graph_get_height();

   return height;
}

float osd_render_stats_video_stats_get_width()
{
   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   return width;
}

void osd_render_stats_video_stats(float xPos, float yPos)
{
   bool useControllerInfo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?true:false;
   
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 5.0;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float fStroke = OSD_STRIKE_WIDTH;

   float width = osd_render_stats_video_stats_get_width();
   float height = osd_render_stats_video_stats_get_height();

   char szBuff[128];
   char szBuff1[64];
   char szBuff2[64];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, "Video Link Stats");
   
   //snprintf(szBuff, sizeof(szBuff),"%d ms/bar", VIDEO_LINK_STATS_REFRESH_INTERVAL_MS);
   //g_pRenderEngine->drawTextLeft(rightMargin, yPos, height_text_small, s_idFontStatsSmall, szBuff);
   //float wT = g_pRenderEngine->textWidth(height_text_small, s_idFontStatsSmall, szBuff);
   //snprintf(szBuff, sizeof(szBuff),"%.1f sec, ", (((float)MAX_INTERVALS_VIDEO_LINK_STATS) * VIDEO_LINK_STATS_REFRESH_INTERVAL_MS)/1000.0);
   //g_pRenderEngine->drawTextLeft(rightMargin-wT, yPos, height_text_small, s_idFontStatsSmall, szBuff);

   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;
   float yTop = y;

   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   if ( NULL == g_pCurrentModel || NULL == g_pSM_VideoLinkStats || 0 == g_pSM_VideoLinkStats->timeLastStatsCheck )
   {
      bool bAdaptiveVideoOn = false;
      if ( NULL != g_pCurrentModel )
         bAdaptiveVideoOn = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS?true:false;
      if ( bAdaptiveVideoOn )
         g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "No Info Available");
      else
         g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Adaptive Video is Off.");
      return;
   }

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "User / Current Profile :");
   strcpy(szBuff1, str_get_video_profile_name(g_pSM_VideoLinkStats->overwrites.userVideoLinkProfile) );
   strcpy(szBuff2, str_get_video_profile_name(g_pSM_VideoLinkStats->overwrites.currentVideoLinkProfile) );
   //if ( (NULL != g_psmvds) && (g_psmvds->encoding_extra_flags & ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE) )
   //   strlcat(szBuff2, "-", sizeof(szBuff2));
   snprintf(szBuff, sizeof(szBuff), "%s / %s", szBuff1, szBuff2 );
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "User/Current Prof. Bitrate:");
   str_format_bitrate_no_sufix(g_pCurrentModel->video_link_profiles[g_pSM_VideoLinkStats->overwrites.userVideoLinkProfile].bitrate_fixed_bps, szBuff1);
   str_format_bitrate(g_pSM_VideoLinkStats->overwrites.currentProfileDefaultVideoBitrate, szBuff2);
   snprintf(szBuff, sizeof(szBuff), "%s / %s", szBuff1, szBuff2);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Current Bitrate Overwrite:");
   //str_format_bitrate_no_sufix(g_pSM_VideoLinkStats->overwrites.profilesTopVideoBitrateOverwritesDownward[g_pSM_VideoLinkStats->overwrites.userVideoLinkProfile], szBuff1);
   //str_format_bitrate(g_pSM_VideoLinkStats->overwrites.profilesTopVideoBitrateOverwritesDownward[g_pSM_VideoLinkStats->overwrites.currentVideoLinkProfile], szBuff2);
   //snprintf(szBuff, sizeof(szBuff), "%s / %s", szBuff1, szBuff2);
   str_format_bitrate(g_pSM_VideoLinkStats->overwrites.profilesTopVideoBitrateOverwritesDownward[g_pSM_VideoLinkStats->overwrites.currentVideoLinkProfile], szBuff);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Current Target V. Bitrate:");
   str_format_bitrate(g_pSM_VideoLinkStats->overwrites.currentSetVideoBitrate, szBuff);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Current EC Scheme:");
   snprintf(szBuff, sizeof(szBuff), "%d / %d", g_pSM_VideoLinkStats->overwrites.currentDataBlocks, g_pSM_VideoLinkStats->overwrites.currentECBlocks);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Current Level Shift:");
   snprintf(szBuff, sizeof(szBuff),"%d", (int)g_pSM_VideoLinkStats->overwrites.currentProfileShiftLevel);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Uses Controller Feedback:");
   if ( (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO )
      strcpy(szBuff, "Yes");
   else
      strcpy(szBuff, "No");
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Used Controller Feedback:");
   if ( g_pSM_VideoLinkStats->usedControllerInfo )
      strcpy(szBuff, "Yes");
   else
      strcpy(szBuff, "No");
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Video Adjustment Strength:");
   snprintf(szBuff, sizeof(szBuff), "%d%%", g_pCurrentModel->video_params.videoAdjustmentStrength * 10 );
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Intervals Used for Down-Comp:");
   snprintf(szBuff, sizeof(szBuff), "%d", g_pSM_VideoLinkStats->backIntervalsToLookForDownStep );
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Intervals Used for Up-Comp:");
   snprintf(szBuff, sizeof(szBuff), "%d", g_pSM_VideoLinkStats->backIntervalsToLookForUpStep );
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Min Down Change Interval:");
   snprintf(szBuff, sizeof(szBuff), "%d ms", g_pSM_VideoLinkStats->computed_min_interval_for_change_down );
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Min Up Change Interval:");
   snprintf(szBuff, sizeof(szBuff), "%d ms", g_pSM_VideoLinkStats->computed_min_interval_for_change_up );
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Comp. RX Vehicle Quality:");
   snprintf(szBuff, sizeof(szBuff), "%d%%", g_pSM_VideoLinkStats->computed_rx_quality_vehicle);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Com. RX Controller Quality:");
   snprintf(szBuff, sizeof(szBuff), "%d%%", g_pSM_VideoLinkStats->computed_rx_quality_controller);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Last params changed down:");

   if ( g_pSM_VideoLinkStats->timeLastAdaptiveParamsChangeDown == 0 )
      strcpy(szBuff, "Never");
   else if ( g_pSM_VideoLinkStats->timeLastAdaptiveParamsChangeDown >= g_pSM_VideoLinkStats->timeLastStatsCheck )
      strcpy(szBuff, "Now");
   else
      snprintf(szBuff, sizeof(szBuff), "%d sec ago",  (int)((g_pSM_VideoLinkStats->timeLastStatsCheck - g_pSM_VideoLinkStats->timeLastAdaptiveParamsChangeDown) / 1000) );

   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Last params changed up:");

   if ( g_pSM_VideoLinkStats->timeLastAdaptiveParamsChangeUp == 0 )
      strcpy(szBuff, "Never");
   else if ( g_pSM_VideoLinkStats->timeLastAdaptiveParamsChangeUp >= g_pSM_VideoLinkStats->timeLastStatsCheck )
      strcpy(szBuff, "Now");
   else
      snprintf(szBuff, sizeof(szBuff), "%d sec ago",  (int)((g_pSM_VideoLinkStats->timeLastStatsCheck - g_pSM_VideoLinkStats->timeLastAdaptiveParamsChangeUp) / 1000) );

   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Total Shifts:");
   snprintf(szBuff, sizeof(szBuff),"%d", (int)g_pSM_VideoLinkStats->totalSwitches);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   // ---------------------------------------------------------------
   // Graphs
   y += height_text_small*0.7;

   osd_render_stats_adaptive_video_graph(xPos, y);
   y += osd_render_stats_adaptive_video_graph_get_height();
   osd_set_colors();
}


float osd_render_stats_video_graphs_get_height()
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 2.0;
   float hGraphSmall = hGraph*0.6;
   
   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;

   if ( NULL == g_pCurrentModel || NULL == g_pSM_VideoLinkGraphs || 0 == g_pSM_VideoLinkGraphs->timeLastStatsUpdate )
   {
      height += height_text*s_OSDStatsLineSpacing;
      return height;
   }

   // Vehicle graphs

   height += 2.0 * (hGraphSmall + 1.5*height_text_small);
   height += 2.0 * (hGraph + 1.5*height_text_small);

   // Controller graphs

   height += hardware_get_radio_interfaces_count() * (hGraphSmall + 1.5*height_text_small);

   height += 2 * (hGraph + 1.5*height_text_small);

   // controller Max EC used
   height += hGraphSmall + 1.5*height_text_small;

   return height;
}

float osd_render_stats_video_graphs_get_width()
{
   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAA AAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   return width;
}

void osd_render_stats_video_graphs(float xPos, float yPos)
{
   bool useControllerInfo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?true:false;
   
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 2.0;
   float hGraphSmall = hGraph*0.6;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float fStroke = OSD_STRIKE_WIDTH;

   float width = osd_render_stats_video_graphs_get_width();
   float height = osd_render_stats_video_graphs_get_height();

   char szBuff[64];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, "Video Link Graphs");
   
   snprintf(szBuff, sizeof(szBuff),"%d ms/bar", VIDEO_LINK_STATS_REFRESH_INTERVAL_MS);
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, height_text_small, s_idFontStatsSmall, szBuff);
   float wT = g_pRenderEngine->textWidth(height_text_small, s_idFontStatsSmall, szBuff);
   snprintf(szBuff, sizeof(szBuff),"%.1f sec, ", (((float)MAX_INTERVALS_VIDEO_LINK_STATS) * VIDEO_LINK_STATS_REFRESH_INTERVAL_MS)/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin-wT, yPos, height_text_small, s_idFontStatsSmall, szBuff);

   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;
   float yTop = y;

   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   if ( NULL == g_pCurrentModel || NULL == g_pSM_VideoLinkGraphs || 0 == g_pSM_VideoLinkGraphs->timeLastStatsUpdate )
   {
      bool bAdaptiveVideoOn = false;
      if ( NULL != g_pCurrentModel )
         bAdaptiveVideoOn = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS?true:false;
      if ( bAdaptiveVideoOn )
         g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "No Info Available");
      else
         g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Adaptive Video is Off.");
      return;
   }
   
   // ---------------------------------------------------------------
   // Graphs

   float dxGraph = g_pRenderEngine->textWidth(0.0, s_idFontStatsSmall, "8887 ms");
   float widthGraph = widthMax - dxGraph;
   float widthBar = widthGraph / MAX_INTERVALS_VIDEO_LINK_STATS;
   float xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   float yBottomGraph = y + hGraphSmall - g_pRenderEngine->getPixelHeight();
   float midLine = hGraph/2.0;
   int maxGraphValue = 1;
   double* pc = get_Color_Dev();
   float fAlphaOrg = g_pRenderEngine->getGlobalAlfa();
   float fAlphaLow = 0.5;

   // ---------------------------------------------------------------
   // Graphs controller radio interfaces rx quality

   if ( ! useControllerInfo )
      g_pRenderEngine->setGlobalAlfa(fAlphaLow);

   for( int iNIC=0; iNIC < hardware_get_radio_interfaces_count(); iNIC++ )
   {
   g_pRenderEngine->setColors(get_Color_Dev());
   y += 0.2*height_text_small;
   snprintf(szBuff, sizeof(szBuff), "C: RX quality radio interface %d", iNIC+1);
   g_pRenderEngine->drawText(xPos, y, height_text_small, s_idFontStatsSmall, szBuff);
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraphSmall - g_pRenderEngine->getPixelHeight();
   midLine = hGraphSmall/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
      if ( g_pSM_VideoLinkGraphs->controller_received_radio_interfaces_rx_quality[iNIC][i] != 255 )
      if ( g_pSM_VideoLinkGraphs->controller_received_radio_interfaces_rx_quality[iNIC][i] > maxGraphValue )
         maxGraphValue = g_pSM_VideoLinkGraphs->controller_received_radio_interfaces_rx_quality[iNIC][i];

   snprintf(szBuff, sizeof(szBuff), "%d%%", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, height_text_small, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraphSmall-height_text_small*0.7, height_text_small, s_idFontStatsSmall, "0%");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraphSmall, xPos + dxGraph + widthGraph, y+hGraphSmall);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      float percent = (float)(g_pSM_VideoLinkGraphs->controller_received_radio_interfaces_rx_quality[iNIC][i])/(float)(maxGraphValue);
      if ( g_pSM_VideoLinkGraphs->controller_received_radio_interfaces_rx_quality[iNIC][i] == 255 )
      {
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->setFill(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBarSt + 0.1*widthBar, yBottomGraph-midLine-0.5*widthBar, 0.8*widthBar, 1.5*widthBar);
         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         xBarSt -= widthBar;
         continue;
      }
      else if ( g_pSM_VideoLinkGraphs->controller_received_radio_interfaces_rx_quality[iNIC][i] < 100 )
      {
         g_pRenderEngine->setStroke(255,250,50,0.9);
         g_pRenderEngine->setFill(255,250,50,0.9);
         g_pRenderEngine->setStrokeSize(fStroke);
      }
      else
      {
         g_pRenderEngine->setColors(get_Color_Dev());
         g_pRenderEngine->setStrokeSize(fStroke);
      }

      if ( percent > 1.0 ) percent = 1.0;

      if ( percent > 0.001 )
      {
         float hBar = hGraphSmall*percent;
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hBar, widthBar-wPixel, hBar);
      }

      xBarSt -= widthBar;
   }
   y += hGraphSmall;

   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());
   }

   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   // ---------------------------------------------------------------
   // Graph vehicle rx quality

   if ( useControllerInfo )
      g_pRenderEngine->setGlobalAlfa(fAlphaLow);

   g_pRenderEngine->setColors(get_Color_Dev());
   y += 0.2*height_text_small;
   g_pRenderEngine->drawText(xPos, y, height_text_small, s_idFontStatsSmall, "V: Max RX quality");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraphSmall - g_pRenderEngine->getPixelHeight();
   midLine = hGraphSmall/2.0;
   maxGraphValue = 100;

   snprintf(szBuff, sizeof(szBuff), "%d%%", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, height_text_small, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraphSmall-height_text_small*0.7, height_text_small, s_idFontStatsSmall, "0%");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setColors(get_Color_Dev());
      
   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      if ( g_pSM_VideoLinkGraphs->vehicleRXQuality[i] == 255 )
      {
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->setFill(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBarSt + 0.1*widthBar, yBottomGraph-midLine-0.5*widthBar, 0.8*widthBar, 1.5*widthBar);
         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         xBarSt -= widthBar;
         continue;
      }
      g_pRenderEngine->setColors(get_Color_Dev());
      float hBar = hGraphSmall*g_pSM_VideoLinkGraphs->vehicleRXQuality[i]/100.0;
      g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hBar, widthBar-wPixel, hBar);

      xBarSt -= widthBar;
   }
   y += hGraphSmall;

   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   // ---------------------------------------------------------------
   // Graph vehicle rx time gap

   if ( useControllerInfo )
      g_pRenderEngine->setGlobalAlfa(fAlphaLow);

   g_pRenderEngine->setColors(get_Color_Dev());
   y += 0.2*height_text_small;
   g_pRenderEngine->drawText(xPos, y, height_text_small, s_idFontStatsSmall, "V: Max RX time gap");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraphSmall - g_pRenderEngine->getPixelHeight();
   midLine = hGraphSmall/2.0;
   maxGraphValue = 254;

   snprintf(szBuff, sizeof(szBuff), "%d ms", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, height_text_small, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraphSmall-height_text_small*0.7, height_text_small, s_idFontStatsSmall, "0 ms");

   pc = get_Color_Dev();
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraphSmall, xPos + dxGraph + widthGraph, y+hGraphSmall);
   
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      if ( g_pSM_VideoLinkGraphs->vehicleRXMaxTimeGap[i] == 255 )
      {
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->setFill(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBarSt + 0.1*widthBar, yBottomGraph-midLine-0.5*widthBar, 0.8*widthBar, 1.5*widthBar);
         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         xBarSt -= widthBar;
         continue;
      }

      float percent = (float)(g_pSM_VideoLinkGraphs->vehicleRXMaxTimeGap[i])/(float)(maxGraphValue);
      if ( percent > 1.0 ) percent = 1.0;
      if ( percent > 0.001 )
      {
         float hBar = hGraphSmall*percent;
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hBar, widthBar-wPixel, hBar);
      }

      xBarSt -= widthBar;
   }
   y += hGraphSmall;

   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());


   // ---------------------------------------------------------------
   // Controller Requested packets for retransmission

   if ( ! useControllerInfo )
      g_pRenderEngine->setGlobalAlfa(fAlphaLow);

   g_pRenderEngine->setColors(get_Color_Dev());
   y += 0.2*height_text_small;
   g_pRenderEngine->drawText(xPos, y, height_text_small, s_idFontStatsSmall, "C: Requested packets for retransmission");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();
   midLine = hGraph/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_requested_retransmission_packets[0][i] != 255 )
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_requested_retransmission_packets[0][i] > maxGraphValue )
         maxGraphValue = g_pSM_VideoLinkGraphs->controller_received_video_streams_requested_retransmission_packets[0][i];

   snprintf(szBuff, sizeof(szBuff), "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, height_text*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.7, height_text*0.9, s_idFontStatsSmall, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      float percent = (float)(g_pSM_VideoLinkGraphs->controller_received_video_streams_requested_retransmission_packets[0][i])/(float)(maxGraphValue);
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_requested_retransmission_packets[0][i] == 255 )
      {
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->setFill(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBarSt + 0.1*widthBar, yBottomGraph-midLine-0.5*widthBar, 0.8*widthBar, 1.5*widthBar);
         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         xBarSt -= widthBar;
         continue;
      }
      if ( percent > 1.0 ) percent = 1.0;
      if ( percent > 0.001 )
      {
         float hBar = hGraph*percent;
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hBar, widthBar-wPixel, hBar);
      }

      xBarSt -= widthBar;
   }
   y += hGraph;

   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   // ---------------------------------------------------------------
   // Graph vehicle video info

   if ( useControllerInfo )
      g_pRenderEngine->setGlobalAlfa(fAlphaLow);

   g_pRenderEngine->setColors(get_Color_Dev());
   y += 0.2*height_text_small;
   g_pRenderEngine->drawText(xPos, y, height_text_small, s_idFontStatsSmall, "V: Requested packets in retransmissions");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();
   midLine = hGraph/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
      if ( g_pSM_VideoLinkGraphs->vehicleReceivedRetransmissionsRequestsPackets[i] > maxGraphValue )
         maxGraphValue = g_pSM_VideoLinkGraphs->vehicleReceivedRetransmissionsRequestsPackets[i];

   snprintf(szBuff, sizeof(szBuff), "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, height_text*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.7, height_text*0.9, s_idFontStatsSmall, "0");

   pc = get_Color_Dev();
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      float percent = (float)(g_pSM_VideoLinkGraphs->vehicleReceivedRetransmissionsRequestsPackets[i])/(float)(maxGraphValue);
      if ( percent > 1.0 ) percent = 1.0;
      if ( percent > 0.001 )
      {
         float hBar = hGraph*percent;
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hBar, widthBar-wPixel, hBar);
      }

      xBarSt -= widthBar;
   }
   y += hGraph;

   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   // ---------------------------------------------------------------
   // Second vehicle video graph

   if ( useControllerInfo )
      g_pRenderEngine->setGlobalAlfa(fAlphaLow);

   g_pRenderEngine->setColors(get_Color_Dev());
   y += 0.2*height_text_small;
   g_pRenderEngine->drawText(xPos, y, height_text_small, s_idFontStatsSmall, "V: Retried packets in retransmissions");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();
   midLine = hGraph/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
      if ( g_pSM_VideoLinkGraphs->vehicleReceivedRetransmissionsRequestsPacketsRetried[i] > maxGraphValue )
         maxGraphValue = g_pSM_VideoLinkGraphs->vehicleReceivedRetransmissionsRequestsPacketsRetried[i];

   snprintf(szBuff, sizeof(szBuff), "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, height_text*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.7, height_text*0.9, s_idFontStatsSmall, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      float percent = (float)(g_pSM_VideoLinkGraphs->vehicleReceivedRetransmissionsRequestsPacketsRetried[i])/(float)(maxGraphValue);
      if ( percent > 1.0 ) percent = 1.0;
      if ( percent > 0.001 )
      {
         float hBar = hGraph*percent;
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hBar, widthBar-wPixel, hBar);
      }

      xBarSt -= widthBar;
   }
   y += hGraph;

   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());


   // ---------------------------------------------------------------
   // Controller Video Stream blocks

   if ( ! useControllerInfo )
      g_pRenderEngine->setGlobalAlfa(fAlphaLow);

   g_pRenderEngine->setColors(get_Color_Dev());
   y += 0.2*height_text_small;
   g_pRenderEngine->drawText(xPos, y, height_text_small, s_idFontStatsSmall, "C: Received good/reconstructed video blocks");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();
   midLine = hGraph/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_clean[0][i] != 255 )
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_clean[0][i] > maxGraphValue )
         maxGraphValue = g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_clean[0][i];
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_reconstructed[0][i] != 255 )
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_reconstructed[0][i] > maxGraphValue )
         maxGraphValue = g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_reconstructed[0][i];

      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_clean[0][i] != 255 )
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_reconstructed[0][i] != 255 )
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_clean[0][i] + g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_reconstructed[0][i] > maxGraphValue )
         maxGraphValue = g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_clean[0][i] + g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_reconstructed[0][i];
   }

   snprintf(szBuff, sizeof(szBuff), "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, height_text*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.7, height_text*0.9, s_idFontStatsSmall, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   pc = get_Color_OSDText();
   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      float percentGood = (float)(g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_clean[0][i])/(float)(maxGraphValue);
      float percentRec = (float)(g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_reconstructed[0][i])/(float)(maxGraphValue);
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_clean[0][i] == 255 )
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_reconstructed[0][i] == 255 )
      {
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->setFill(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBarSt + 0.1*widthBar, yBottomGraph-midLine-0.5*widthBar, 0.8*widthBar, 1.5*widthBar);
         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         xBarSt -= widthBar;
         continue;
      }

      float hBarGood = 0.0;
      if ( percentGood > 1.0 ) percentGood = 1.0;
      if ( percentGood > 0.001 )
      {
         hBarGood = hGraph*percentGood;
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hBarGood+g_pRenderEngine->getPixelHeight(), widthBar-wPixel, hBarGood-g_pRenderEngine->getPixelHeight());
      }

      if ( percentRec > 1.0 ) percentRec = 1.0;
      if ( percentRec > 0.001 )
      {
         g_pRenderEngine->setFill(20,255,50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(20,255,50, s_fOSDStatsGraphLinesAlpha);
         float hBar = hGraph*percentRec;
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hBar-hBarGood, widthBar-wPixel, hBar);
         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      }

      xBarSt -= widthBar;
   }
   y += hGraph;

   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   // ---------------------------------------------------------------
   // Controller max EC used

   if ( ! useControllerInfo )
      g_pRenderEngine->setGlobalAlfa(fAlphaLow);

   g_pRenderEngine->setColors(get_Color_Dev());
   y += 0.2*height_text_small;
   g_pRenderEngine->drawText(xPos, y, height_text_small, s_idFontStatsSmall, "C: Max EC packets used");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraphSmall - g_pRenderEngine->getPixelHeight();
   midLine = hGraphSmall/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_max_ec_packets_used[0][i] != 255 )
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_max_ec_packets_used[0][i] > maxGraphValue )
         maxGraphValue = g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_max_ec_packets_used[0][i];
   }

   snprintf(szBuff, sizeof(szBuff), "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, height_text*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraphSmall-height_text_small*0.7, height_text*0.9, s_idFontStatsSmall, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraphSmall, xPos + dxGraph + widthGraph, y+hGraphSmall);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   pc = get_Color_OSDText();
   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      float percent = (float)(g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_max_ec_packets_used[0][i])/(float)(maxGraphValue);
      if ( g_pSM_VideoLinkGraphs->controller_received_video_streams_blocks_max_ec_packets_used[0][i] == 255 )
      {
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->setFill(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBarSt + 0.1*widthBar, yBottomGraph-midLine-0.5*widthBar, 0.8*widthBar, 1.5*widthBar);
         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         xBarSt -= widthBar;
         continue;
      }

      if ( percent > 1.0 ) percent = 1.0;
      if ( percent > 0.001 )
      {
         float hBar = hGraphSmall*percent;
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hBar+g_pRenderEngine->getPixelHeight(), widthBar-wPixel, hBar-g_pRenderEngine->getPixelHeight());
      }
      xBarSt -= widthBar;
   }
   y += hGraphSmall;

   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());


   g_pRenderEngine->setGlobalAlfa(0.5);
   g_pRenderEngine->setColors(get_Color_Dev());
   g_pRenderEngine->setStrokeSize(1.0);

   float x1 = xPos+dxGraph+widthGraph - widthBar*g_pSM_VideoLinkStats->backIntervalsToLookForDownStep;
   g_pRenderEngine->drawLine(x1, yTop+height_text, x1, y);

   float x2 = xPos+dxGraph+widthGraph - widthBar*g_pSM_VideoLinkStats->backIntervalsToLookForUpStep;
   g_pRenderEngine->drawLine(x2, yTop+height_text, x2, y);


   //-----------------------
   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   osd_set_colors();
}

float osd_render_stats_graphs_vehicle_tx_gap_get_height()
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 4.0;
   float hGraph2 = height_text * 3.0;

   float height = 2.0 *s_fOSDStatsMargin*1.1 + height_text*s_OSDStatsLineSpacing;
   height += hGraph + height_text*3.6 + 0.9 * height_text*s_OSDStatsLineSpacing + 2.0*hGraph2 + 2.0*height_text*s_OSDStatsLineSpacing;
   return height;
}

float osd_render_stats_graphs_vehicle_tx_gap_get_width()
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;

   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

void  osd_render_stats_graphs_vehicle_tx_gap(float xPos, float yPos)
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 4.0;
   float hGraph2 = height_text * 3.0;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float fStroke = OSD_STRIKE_WIDTH;

   float width = osd_render_stats_graphs_vehicle_tx_gap_get_width();
   float height = osd_render_stats_graphs_vehicle_tx_gap_get_height();
   
   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;

   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, "Vehicle Tx Stats");
   
   osd_set_colors();

   if ( ! g_bGotStatsVehicleTx )
   {
      g_pRenderEngine->drawText(xPos, yPos + 2.0 * height_text, s_idFontStats, "No Data.");
      return;
   }
   
   float rightMargin = xPos + width;
   float marginH = 0.0;
   float widthMax = width - marginH - 0.001;

   char szBuff[64];
   snprintf(szBuff, sizeof(szBuff),"%d ms/bar", g_PHVehicleTxHistory.iSliceInterval);
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, height_text_small, s_idFontStatsSmall, szBuff);

   float w = g_pRenderEngine->textWidth(height_text_small, s_idFontStatsSmall, szBuff);
   snprintf(szBuff, sizeof(szBuff),"%.1f seconds, ", (((float)g_PHVehicleTxHistory.uCountValues) * g_PHVehicleTxHistory.iSliceInterval)/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin-w, yPos, height_text_small, s_idFontStatsSmall, szBuff);
   w += g_pRenderEngine->textWidth( height_text_small, s_idFontStatsSmall, szBuff);
   
   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   // First graph 

   g_pRenderEngine->drawTextLeft(rightMargin, y-0.2*height_text, height_text, s_idFontStats, "Tx Min/Max/Avg Gaps (ms)");
   y += height_text_small + height_text*0.2;

   float dxGraph = g_pRenderEngine->textWidth(0.0, s_idFontStatsSmall, "8878");
   float widthGraph = widthMax - dxGraph;
   float widthBar = widthGraph / g_PHVehicleTxHistory.uCountValues;
   float xBar = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   float yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

   if ( 0 == g_PHVehicleTxHistory.uCountValues )
      g_PHVehicleTxHistory.uCountValues = 1;
   if ( g_PHVehicleTxHistory.uCountValues > sizeof(g_PHVehicleTxHistory.historyTxGapMaxMiliseconds)/sizeof(g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[0]))
       g_PHVehicleTxHistory.uCountValues = sizeof(g_PHVehicleTxHistory.historyTxGapMaxMiliseconds)/sizeof(g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[0]);

   u32 uAverageTxGapSum = 0;
   u8 uAverageTxGapCount = 0;
   u32 uAverageVideoPacketsIntervalSum = 0;
   u8 uAverageVideoPacketsIntervalCount = 0;
   u8 uValMaxMax = 0;
   u8 uMaxTxPackets = 0;
   u8 uMaxVideoPacketsGap = 0;
   for( int k=0; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF != g_PHVehicleTxHistory.historyTxGapAvgMiliseconds[k] )
      {
         uAverageTxGapCount++;
         uAverageTxGapSum += g_PHVehicleTxHistory.historyTxGapAvgMiliseconds[k];
      }

      if ( 0xFF != g_PHVehicleTxHistory.historyVideoPacketsGapAvg[k] )
      {
         uAverageVideoPacketsIntervalCount++;
         uAverageVideoPacketsIntervalSum += g_PHVehicleTxHistory.historyVideoPacketsGapAvg[k];
      }

      if ( 0xFF != g_PHVehicleTxHistory.historyVideoPacketsGapMax[k] )
      if ( g_PHVehicleTxHistory.historyVideoPacketsGapMax[k] > uMaxVideoPacketsGap )
         uMaxVideoPacketsGap = g_PHVehicleTxHistory.historyVideoPacketsGapMax[k];

      if ( 0xFF != g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[k] )
      if ( g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[k] > uValMaxMax )
         uValMaxMax = g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[k];

      if ( 0xFF != g_PHVehicleTxHistory.historyTxPackets[k] )
      if ( g_PHVehicleTxHistory.historyTxPackets[k] > uMaxTxPackets )
         uMaxTxPackets = g_PHVehicleTxHistory.historyTxPackets[k];
   }

   double* pc = get_Color_Dev();
   g_pRenderEngine->setColors(get_Color_Dev());

   int iMax = 200;
   if ( uValMaxMax <= 100 )
      iMax = 100;
   if ( uValMaxMax <= 50 )
      iMax = 50;

   snprintf(szBuff, sizeof(szBuff), "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5, height_text_small, s_idFontStatsSmall, szBuff);
   snprintf(szBuff, sizeof(szBuff), "%d", iMax*3/4);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph*0.25, height_text_small, s_idFontStatsSmall, szBuff);
   snprintf(szBuff, sizeof(szBuff), "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph*0.5, height_text_small, s_idFontStatsSmall, szBuff);
   snprintf(szBuff, sizeof(szBuff), "%d", iMax/4);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph*0.75, height_text_small, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph, height_text_small, s_idFontStatsSmall, "0");

   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStrokeSize(fStroke);
   g_pRenderEngine->drawLine(xPos+marginH+dxGraph,y+hGraph + g_pRenderEngine->getPixelHeight(), xPos+marginH + widthMax, y+hGraph + g_pRenderEngine->getPixelHeight());
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   {
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y, xPos + dxGraph + i + 2.0*wPixel, y);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph*0.25, xPos + dxGraph + i + 2.0*wPixel, y+hGraph*0.25);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph*0.5, xPos + dxGraph + i + 2.0*wPixel, y+hGraph*0.5);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph*0.75, xPos + dxGraph + i + 2.0*wPixel, y+hGraph*0.75);
   }
     
   for( int k=0; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF == g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[k] )
      {
         g_pRenderEngine->setStroke(255,0,50,0.9);
         g_pRenderEngine->setFill(255,0,50,0.9);
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->drawRect(xBar, y+hGraph*0.4, widthBar - g_pRenderEngine->getPixelWidth(), hGraph*0.4);

         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
      }
      else
      {
         float hBar = hGraph * ((float)g_PHVehicleTxHistory.historyTxGapMaxMiliseconds[k])/iMax;
         if ( hBar > hGraph )
            hBar = hGraph;
         g_pRenderEngine->drawRect(xBar, y+hGraph - hBar, widthBar - g_pRenderEngine->getPixelWidth(), hBar);
      }
      xBar -= widthBar;
   }

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH*2.0);
   g_pRenderEngine->setStroke(250,70,50, s_fOSDStatsGraphLinesAlpha);

   xBar = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   float hValPrev = hGraph * ((float)g_PHVehicleTxHistory.historyTxGapAvgMiliseconds[0])/iMax;
   if ( hValPrev > hGraph )
      hValPrev = hGraph;
   xBar -= widthBar;


   for( int k=1; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF == g_PHVehicleTxHistory.historyTxGapAvgMiliseconds[k] )
         continue;
      float hBar = hGraph * ((float)g_PHVehicleTxHistory.historyTxGapAvgMiliseconds[k])/iMax;
      if ( hBar > hGraph )
         hBar = hGraph;

      g_pRenderEngine->drawLine(xBar + 0.5*widthBar, y+hGraph - hBar, xBar + 1.5*widthBar, y+hGraph - hValPrev);
      hValPrev = hBar;
      xBar -= widthBar;
   }

   y += hGraph + 0.4 * height_text*s_OSDStatsLineSpacing;
   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setColors(get_Color_Dev());

   // Second graph (tx packets / sec )

   osd_set_colors();
   g_pRenderEngine->drawTextLeft(rightMargin, y-0.2*height_text, height_text, s_idFontStats, "Tx Packets / Slice");
   y += height_text_small + height_text*0.2;

   xBar = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph2 - g_pRenderEngine->getPixelHeight();
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

   g_pRenderEngine->setColors(get_Color_Dev());

   iMax = 200;
   if ( uMaxTxPackets <= 100 )
      iMax = 100;
   if ( uMaxTxPackets <= 50 )
      iMax = 50;

   snprintf(szBuff, sizeof(szBuff), "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5, height_text_small, s_idFontStatsSmall, szBuff);
   snprintf(szBuff, sizeof(szBuff), "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2*0.5, height_text_small, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2, height_text_small, s_idFontStatsSmall, "0");

   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStrokeSize(fStroke);
   g_pRenderEngine->drawLine(xPos+marginH+dxGraph,y+hGraph2 + g_pRenderEngine->getPixelHeight(), xPos+marginH + widthMax, y+hGraph2 + g_pRenderEngine->getPixelHeight());
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   {
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y, xPos + dxGraph + i + 2.0*wPixel, y);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph2*0.5, xPos + dxGraph + i + 2.0*wPixel, y+hGraph2*0.5);
   }
     
   for( int k=0; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF == g_PHVehicleTxHistory.historyTxPackets[k] )
      {
         g_pRenderEngine->setStroke(255,0,50,0.9);
         g_pRenderEngine->setFill(255,0,50,0.9);
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->drawRect(xBar, y+hGraph2*0.4, widthBar - g_pRenderEngine->getPixelWidth(), hGraph2*0.4);

         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
      }
      else
      {
         float hBar = hGraph2 * ((float)g_PHVehicleTxHistory.historyTxPackets[k])/iMax;
         if ( hBar > hGraph2 )
            hBar = hGraph2;
         g_pRenderEngine->drawRect(xBar, y+hGraph2 - hBar, widthBar - g_pRenderEngine->getPixelWidth(), hBar);
      }
      xBar -= widthBar;
   }

   y += hGraph2 + 0.4 * height_text*s_OSDStatsLineSpacing;
   g_pRenderEngine->setStrokeSize(0);
   osd_set_colors();

   // Third graph (video packets in gaps )

   osd_set_colors();
   g_pRenderEngine->drawTextLeft(rightMargin, y-0.2*height_text, height_text, s_idFontStats, "Video Packets Max Intervals (ms)");
   y += height_text_small + height_text*0.2;

   xBar = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph2 - g_pRenderEngine->getPixelHeight();
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

   g_pRenderEngine->setColors(get_Color_Dev());

   iMax = 200;
   if ( uMaxVideoPacketsGap <= 100 )
      iMax = 100;
   if ( uMaxVideoPacketsGap <= 50 )
      iMax = 50;

   snprintf(szBuff, sizeof(szBuff), "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5, height_text_small, s_idFontStatsSmall, szBuff);
   snprintf(szBuff, sizeof(szBuff), "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2*0.5, height_text_small, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2, height_text_small, s_idFontStatsSmall, "0");

   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStrokeSize(fStroke);
   g_pRenderEngine->drawLine(xPos+marginH+dxGraph,y+hGraph2 + g_pRenderEngine->getPixelHeight(), xPos+marginH + widthMax, y+hGraph2 + g_pRenderEngine->getPixelHeight());
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   {
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y, xPos + dxGraph + i + 2.0*wPixel, y);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph2*0.5, xPos + dxGraph + i + 2.0*wPixel, y+hGraph2*0.5);
   }
     
   for( int k=0; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF == g_PHVehicleTxHistory.historyVideoPacketsGapMax[k] )
      {
         g_pRenderEngine->setStroke(255,0,50,0.9);
         g_pRenderEngine->setFill(255,0,50,0.9);
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->drawRect(xBar, y+hGraph2*0.4, widthBar - g_pRenderEngine->getPixelWidth(), hGraph2*0.4);

         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
      }
      else
      {
         float hBar = hGraph2 * ((float)g_PHVehicleTxHistory.historyVideoPacketsGapMax[k])/iMax;
         if ( hBar > hGraph2 )
            hBar = hGraph2;
         g_pRenderEngine->drawRect(xBar, y+hGraph2 - hBar, widthBar - g_pRenderEngine->getPixelWidth(), hBar);
      }
      xBar -= widthBar;
   }

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH*2.0);
   g_pRenderEngine->setStroke(250,70,70, s_fOSDStatsGraphLinesAlpha);

   xBar = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   hValPrev = hGraph2 * ((float)g_PHVehicleTxHistory.historyVideoPacketsGapAvg[0])/iMax;
   if ( hValPrev > hGraph2 )
      hValPrev = hGraph2;
   xBar -= widthBar;


   for( int k=1; k<g_PHVehicleTxHistory.uCountValues; k++ )
   {
      if ( 0xFF == g_PHVehicleTxHistory.historyVideoPacketsGapAvg[k] )
         continue;
      float hBar = hGraph2 * ((float)g_PHVehicleTxHistory.historyVideoPacketsGapAvg[k])/iMax;
      if ( hBar > hGraph2 )
         hBar = hGraph2;

      g_pRenderEngine->drawLine(xBar + 0.5*widthBar, y+hGraph2 - hBar, xBar + 1.5*widthBar, y+hGraph2 - hValPrev);
      hValPrev = hBar;
      xBar -= widthBar;
   }

   y += hGraph2 + 0.4 * height_text*s_OSDStatsLineSpacing;
   g_pRenderEngine->setStrokeSize(0);
   osd_set_colors();

   g_pRenderEngine->setColors(get_Color_Dev());


   // Final Texts

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Average Tx Gap:");
   if ( 0 == uAverageTxGapCount )
      strcpy(szBuff, "N/A ms");
   else
      snprintf(szBuff, sizeof(szBuff), "%d ms", uAverageTxGapSum/uAverageTxGapCount );
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Average  Video Packets Interval:");
   if ( 0 == uAverageVideoPacketsIntervalCount )
      strcpy(szBuff, "N/A ms");
   else
      snprintf(szBuff, sizeof(szBuff), "%d ms", uAverageVideoPacketsIntervalSum/uAverageVideoPacketsIntervalCount );
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;
}



float osd_render_stats_video_bitrate_history_get_height()
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 3.5;
   float hGraph2 = height_text * 2.0;

   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;

   height += hGraph + height_text;

   //if ( ! ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS) )
   //   height += height_text*s_OSDStatsLineSpacing;
   //else
      height += hGraph2 + height_text*s_OSDStatsLineSpacing + height_text;
   
   height += height_text*s_OSDStatsLineSpacing;
   return height;
}

float osd_render_stats_video_bitrate_history_get_width()
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;

   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAAAAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

void osd_render_stats_video_bitrate_history(float xPos, float yPos)
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = height_text * 3.5;
   float hGraph2 = height_text * 2.0;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float hPixel = g_pRenderEngine->getPixelHeight();
   float fStroke = OSD_STRIKE_WIDTH;

   float width = osd_render_stats_video_bitrate_history_get_width();
   float height = osd_render_stats_video_bitrate_history_get_height();

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();
   

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   osd_set_colors();
   //g_pRenderEngine->setColors(get_Color_Dev());

   g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, "Video Bitrate");
   
   char szBuff[128];
   snprintf(szBuff, sizeof(szBuff), "(Mbps, %d ms/slice)", (int)g_SM_DevVideoBitrateHistory.uComputeInterval);
   osd_show_value_left(rightMargin,yPos, szBuff, s_idFontStatsSmall);

   float y = yPos + height_text*2.0*s_OSDStatsLineSpacing;
   float yTop = y;
   
   //double* pc = get_Color_Dev();
   //g_pRenderEngine->setColors(get_Color_Dev());
   
   char szBuff1[64];
   char szBuff2[64];

   if ( ! g_bGotStatsVideoBitrate )
   {
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, "No Data");
      return;
   }

   float dxGraph = g_pRenderEngine->textWidth(0.0, s_idFontStatsSmall, "888");
   float widthGraph = widthMax - dxGraph*2;
   float widthBar = widthGraph / g_SM_DevVideoBitrateHistory.uSlices;
   float yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
   
   u32 s_uMaxValueStatsDevVideoBitrate = 0;
   for( int i=0; i<(int)g_SM_DevVideoBitrateHistory.uSlices; i++ )
   {
      if ( g_SM_DevVideoBitrateHistory.uHistMaxVideoDataRateMbps[i] > s_uMaxValueStatsDevVideoBitrate )
         s_uMaxValueStatsDevVideoBitrate = g_SM_DevVideoBitrateHistory.uHistMaxVideoDataRateMbps[i];
      if ( g_SM_DevVideoBitrateHistory.uHistVideoBitrateKb[i]/1000 > s_uMaxValueStatsDevVideoBitrate )
         s_uMaxValueStatsDevVideoBitrate = g_SM_DevVideoBitrateHistory.uHistVideoBitrateKb[i]/1000;
      if ( g_SM_DevVideoBitrateHistory.uHistVideoBitrateAvgKb[i]/1000 > s_uMaxValueStatsDevVideoBitrate )
         s_uMaxValueStatsDevVideoBitrate = g_SM_DevVideoBitrateHistory.uHistVideoBitrateAvgKb[i]/1000;
      if ( g_SM_DevVideoBitrateHistory.uHistTotalVideoBitrateAvgKb[i]/1000 > s_uMaxValueStatsDevVideoBitrate )
         s_uMaxValueStatsDevVideoBitrate = g_SM_DevVideoBitrateHistory.uHistTotalVideoBitrateAvgKb[i]/1000;
   }
   if ( s_uMaxValueStatsDevVideoBitrate < 4 )
      s_uMaxValueStatsDevVideoBitrate = 4;

   snprintf(szBuff, sizeof(szBuff), "%d", (int)s_uMaxValueStatsDevVideoBitrate);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos + dxGraph+widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);

   snprintf(szBuff, sizeof(szBuff), "%d", (int)s_uMaxValueStatsDevVideoBitrate/2);
   g_pRenderEngine->drawText(xPos, y+0.5*hGraph-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos + dxGraph + widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y+0.5*hGraph-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);

   strcpy(szBuff, "0");
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   g_pRenderEngine->drawText(xPos + dxGraph + widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y+hGraph-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   //g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   //g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);

   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   {
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y, xPos + dxGraph + i + 2.0*wPixel, y);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+0.5*hGraph, xPos + dxGraph + i + 2.0*wPixel, y+0.5*hGraph);
   }

   float hDataratePrev = 0.0;
   float hDatarateLowPrev = 0.0;
   float hVideoBitratePrev = 0.0;
   float hVideoBitrateAvgPrev = 0.0;
   float hTotalBitrateAvgPrev = 0.0;

   g_pRenderEngine->setStrokeSize(2.0);
   float xBarMiddle = xPos + dxGraph + widthGraph - widthBar*0.5 - g_pRenderEngine->getPixelWidth();
   for( int i=0; i<(int)g_SM_DevVideoBitrateHistory.uSlices; i++ )
   {
      float hDatarate = g_SM_DevVideoBitrateHistory.uHistMaxVideoDataRateMbps[i]*hGraph/s_uMaxValueStatsDevVideoBitrate;
      float hDatarateLow = hDatarate*DEFAULT_VIDEO_LINK_MAX_LOAD_PERCENT/100.0;
      float hVideoBitrate = g_SM_DevVideoBitrateHistory.uHistVideoBitrateKb[i]*hGraph/s_uMaxValueStatsDevVideoBitrate/1000.0;
      float hVideoBitrateAvg = g_SM_DevVideoBitrateHistory.uHistVideoBitrateAvgKb[i]*hGraph/s_uMaxValueStatsDevVideoBitrate/1000.0;
      float hTotalBitrateAvg = g_SM_DevVideoBitrateHistory.uHistTotalVideoBitrateAvgKb[i]*hGraph/s_uMaxValueStatsDevVideoBitrate/1000.0;
      if ( i == 0 )
      {

      }
      else
      {
         g_pRenderEngine->setStroke(200,200,255, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBarMiddle, y+hGraph-hVideoBitrate+hPixel, xBarMiddle+widthBar, y+hGraph-hVideoBitratePrev+hPixel);

         g_pRenderEngine->setStroke(250,230,50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBarMiddle, y+hGraph-hVideoBitrateAvg, xBarMiddle+widthBar, y+hGraph-hVideoBitrateAvgPrev);

         g_pRenderEngine->setStroke(255,250,220, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBarMiddle, y+hGraph-hTotalBitrateAvg, xBarMiddle+widthBar, y+hGraph-hTotalBitrateAvgPrev);

         g_pRenderEngine->setStroke(250,50,100, s_fOSDStatsGraphLinesAlpha);

         g_pRenderEngine->drawLine(xBarMiddle, y+hGraph-hDatarate, xBarMiddle+widthBar, y+hGraph-hDatarate);
         if ( fabs(hDatarate-hDataratePrev) >= g_pRenderEngine->getPixelHeight() )           
            g_pRenderEngine->drawLine(xBarMiddle+widthBar, y+hGraph-hDatarate, xBarMiddle+widthBar, y+hGraph-hDataratePrev);
         
         g_pRenderEngine->setStroke(250,100,150, s_fOSDStatsGraphLinesAlpha);

         g_pRenderEngine->drawLine(xBarMiddle, y+hGraph-hDatarateLow, xBarMiddle+widthBar, y+hGraph-hDatarateLow);
         if ( fabs(hDatarateLow-hDatarateLowPrev) >= g_pRenderEngine->getPixelHeight() )           
            g_pRenderEngine->drawLine(xBarMiddle+widthBar, y+hGraph-hDatarateLow, xBarMiddle+widthBar, y+hGraph-hDatarateLowPrev);
      }

      hDataratePrev = hDatarate;
      hDatarateLowPrev = hDatarateLow;
      hVideoBitratePrev = hVideoBitrate;
      hVideoBitrateAvgPrev = hVideoBitrateAvg;
      hTotalBitrateAvgPrev = hTotalBitrateAvg;
      xBarMiddle -= widthBar;
   }
   y += hGraph + height_text*0.4;

   //---------------------------------------------------------------------------
   // Second graph

   
   u32 uMaxQuant = 0;
   u32 uMinQuant = 1000;
   for( int i=0; i<(int)g_SM_DevVideoBitrateHistory.uSlices; i++ )
   {
      if ( g_SM_DevVideoBitrateHistory.uHistVideoQuantization[i] == 0xFF )
         continue;
      if ( g_SM_DevVideoBitrateHistory.uHistVideoQuantization[i] > uMaxQuant )
         uMaxQuant = g_SM_DevVideoBitrateHistory.uHistVideoQuantization[i];
      if ( g_SM_DevVideoBitrateHistory.uHistVideoQuantization[i] < uMinQuant )
         uMinQuant = g_SM_DevVideoBitrateHistory.uHistVideoQuantization[i];
   }

   if ( uMaxQuant == uMinQuant || uMaxQuant == uMinQuant+1 )
   {
      //if ( ! ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS) )
      //{
      //   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Auto Adjustments is Off.");
      //   return;
      //}
      uMaxQuant = uMinQuant + 2;
   }

   u32 uMaxDelta = uMaxQuant - uMinQuant;

   g_pRenderEngine->setStrokeSize(1.0);

   snprintf(szBuff, sizeof(szBuff), "Auto Adjustments (overflow at %d):", g_SM_DevVideoBitrateHistory.uQuantizationOverflowValue);
   if (!((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION) )
      strcpy(szBuff, "Auto Quantization Adjustments is Off!");
   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "Current target video bitrate: 0 bps");
   if ( NULL != g_pSM_VideoLinkStats )
   {
      char szTmp[64];
      str_format_bitrate(g_pSM_VideoLinkStats->overwrites.currentSetVideoBitrate, szTmp);
      snprintf(szBuff, sizeof(szBuff), "Current target video bitrate: %s", szTmp);
   }
   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, szBuff);
   y += height_text*0.6;
   y += height_text*s_OSDStatsLineSpacing;

   yBottomGraph = y + hGraph2;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

   snprintf(szBuff, sizeof(szBuff), "%d", (int)uMaxQuant);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos + dxGraph+widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);

   snprintf(szBuff, sizeof(szBuff), "%d", (int)(uMaxQuant+uMinQuant)/2);
   g_pRenderEngine->drawText(xPos, y+0.5*hGraph2-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos + dxGraph + widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y+0.5*hGraph2-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);

   snprintf(szBuff, sizeof(szBuff), "%d", (int)uMinQuant);
   g_pRenderEngine->drawText(xPos, y+hGraph2-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   g_pRenderEngine->drawText(xPos + dxGraph + widthGraph + 3.0*g_pRenderEngine->getPixelWidth(), y+hGraph2-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   //g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   //g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   {
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y, xPos + dxGraph + i + 2.0*wPixel, y);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+0.5*hGraph2, xPos + dxGraph + i + 2.0*wPixel, y+0.5*hGraph2);
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph2, xPos + dxGraph + i + 2.0*wPixel, y+hGraph2);
   }

   float hVideoQuantizationPrev = 0.0;
   xBarMiddle = xPos + dxGraph + widthGraph - widthBar*0.5 - g_pRenderEngine->getPixelWidth();

   g_pRenderEngine->setStroke(150,200,250, s_fOSDStatsGraphLinesAlpha*0.9);
         
   g_pRenderEngine->setStrokeSize(2.0);

   for( int i=0; i<(int)g_SM_DevVideoBitrateHistory.uSlices; i++ )
   {
      float hVideoQuantization = (g_SM_DevVideoBitrateHistory.uHistVideoQuantization[i]-uMinQuant)*hGraph2/uMaxDelta;
 
      if ( (g_SM_DevVideoBitrateHistory.uHistVideoQuantization[i] == 0xFF) ||
           (i > 0 && g_SM_DevVideoBitrateHistory.uHistVideoQuantization[i-1] == 0xFF ) )
      {
 
         g_pRenderEngine->setFill(100,100,200, s_fOSDStatsGraphLinesAlpha*0.6);
         g_pRenderEngine->setStroke(150,200,250, s_fOSDStatsGraphLinesAlpha*0.9*0.6);   
         g_pRenderEngine->setStrokeSize(0.0);
      
         g_pRenderEngine->drawRect(xBarMiddle-widthBar*0.5, y+4.0*g_pRenderEngine->getPixelHeight(), widthBar, hGraph2-8.0*g_pRenderEngine->getPixelHeight());
 
         g_pRenderEngine->setStroke(150,200,250, s_fOSDStatsGraphLinesAlpha*0.9);   
         g_pRenderEngine->setStrokeSize(2.0);
      }
      else if ( i != 0 )
         g_pRenderEngine->drawLine(xBarMiddle, y+hGraph2-hVideoQuantization, xBarMiddle+widthBar, y+hGraph2-hVideoQuantizationPrev);
 
      hVideoQuantizationPrev = hVideoQuantization;
      xBarMiddle -= widthBar;
   }

   g_pRenderEngine->setStrokeSize(0);
   osd_set_colors();
}
