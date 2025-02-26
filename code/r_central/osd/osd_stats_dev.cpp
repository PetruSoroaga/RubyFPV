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
#include "../../common/string_utils.h"
#include <math.h>
#include "osd_stats_dev.h"
#include "osd_common.h"
#include "../colors.h"
#include "../shared_vars.h"
#include "../timers.h"

extern float s_OSDStatsLineSpacing;
extern float s_fOSDStatsMargin;
extern float s_fOSDStatsGraphLinesAlpha;
extern u32 s_idFontStats;
extern u32 s_idFontStatsSmall;

float osd_render_stats_adaptive_video_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float hGraph = height_text * 2.0;
   
   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;

   if ( NULL == g_pCurrentModel || NULL == g_pSM_RouterVehiclesRuntimeInfo )
   {
      height += height_text*s_OSDStatsLineSpacing;
      return height;
   }
   return height;
}

float osd_render_stats_adaptive_video_get_width()
{
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   return width;
}

void osd_render_stats_adaptive_video(float xPos, float yPos)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   
   int iIndexVehicleRuntimeInfo = -1;

   shared_mem_video_stream_stats* pVDS = get_shared_mem_video_stream_stats_for_vehicle(&g_SM_VideoDecodeStats, uActiveVehicleId);
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uActiveVehicleId )
         iIndexVehicleRuntimeInfo = i;
   }
   if ( (NULL == pVDS) || (NULL == pActiveModel) || (-1 == iIndexVehicleRuntimeInfo) )
      return; 

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 2.0;

   float width = osd_render_stats_adaptive_video_get_width();
   float height = osd_render_stats_adaptive_video_get_height();

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Ctrl Adaptive Video Stats");
}


float osd_render_stats_adaptive_video_graph_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 12.0;

   float height = hGraph + height_text_small;
   return height;
}

float osd_render_stats_adaptive_video_graph_get_width()
{
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   return width;
}

void osd_render_stats_adaptive_video_graph(float xPos, float yPos)
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   
   shared_mem_video_stream_stats* pVDS = NULL;
   int iIndexVehicleRuntimeInfo = -1;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( g_SM_VideoDecodeStats.video_streams[i].uVehicleId == uActiveVehicleId )
         pVDS = &(g_SM_VideoDecodeStats.video_streams[i]);
   }
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uActiveVehicleId )
         iIndexVehicleRuntimeInfo = i;
   }
   if ( (NULL == pVDS) || (NULL == pActiveModel) || (-1 == iIndexVehicleRuntimeInfo) )
      return;

   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 12.0;
   float wPixel = g_pRenderEngine->getPixelWidth();

   float width = osd_render_stats_video_stats_get_width();
   float y = yPos;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;

   char szBuff[128];

   float dxGraph = g_pRenderEngine->textWidth(s_idFontStatsSmall, "8878");
   dxGraph += g_pRenderEngine->textWidth(s_idFontStatsSmall, "8878");
   float widthGraph = widthMax - dxGraph;
   float widthBar = widthGraph / MAX_INTERVALS_VIDEO_LINK_SWITCHES;
   float xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   float yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
   double pc[4];
   memcpy(pc, get_Color_Dev(), 4*sizeof(double));

   g_pRenderEngine->setColors(get_Color_Dev());

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();

   int nLevels = 0;
   int lev1 = pActiveModel->get_video_profile_total_levels(pActiveModel->video_params.user_selected_video_link_profile);
   if ( lev1 < 1 ) lev1 = 1;
   nLevels += lev1;
   int lev2 = pActiveModel->get_video_profile_total_levels(VIDEO_PROFILE_MQ);
   if ( lev2 < 1 ) lev2 = 1;
   nLevels += lev2;
   int lev3 = pActiveModel->get_video_profile_total_levels(VIDEO_PROFILE_LQ);
   if ( lev3 < 1 ) lev3 = 1;
   nLevels += lev3;
   float hLevel = hGraph / (float)(nLevels-1);

   float fWTemp = g_pRenderEngine->textWidth(s_idFontStatsSmall, "2131");

   strcpy(szBuff, str_get_video_profile_name(pActiveModel->video_params.user_selected_video_link_profile));
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.4, s_idFontStatsSmall, szBuff);

   float hL1 = hLevel*lev1;
   strcpy(szBuff, str_get_video_profile_name(VIDEO_PROFILE_MQ));
   g_pRenderEngine->drawText(xPos, y+hL1-height_text_small*0.4, s_idFontStatsSmall,szBuff);

   float hL2 = hL1 + hLevel*lev2;
   strcpy(szBuff, str_get_video_profile_name(VIDEO_PROFILE_LQ));
   g_pRenderEngine->drawText(xPos, y+hL2-height_text_small*0.4, s_idFontStatsSmall,szBuff);

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

   int iLevel = 0;
   for( int k=0; k<nLevels; k++ )
   {
      if ( (k != 0) && (k != lev1) && (k != (lev1+lev2)) )
      {
         for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
            g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hLevel*k, xPos + dxGraph + i + 2.0*wPixel, y+hLevel*k);
      }
      int iProfile = pActiveModel->video_params.user_selected_video_link_profile;
      if ( k >= lev1 + lev2 )
         iProfile = VIDEO_PROFILE_LQ;
      else if ( k >= lev1 )
         iProfile = VIDEO_PROFILE_MQ;
      if ( (k == lev1) || (k == lev1 + lev2) )
         iLevel = 0;
      u32 uVideoBitrate = utils_get_max_allowed_video_bitrate_for_profile_and_level(pActiveModel, iProfile, iLevel);
      str_format_bitrate_no_sufix((int)uVideoBitrate, szBuff);
      g_pRenderEngine->drawText(xPos + fWTemp, y + hLevel*k - height_text_small*0.5, s_idFontStats, szBuff);
      iLevel++;
   }
   // To fix
   /*
   int nProfile = g_SM_VideoLinkStats.historySwitches[0]>>4;
   int nLevelShift = g_SM_VideoLinkStats.historySwitches[0] & 0x0F;
   int nLevel = 0;
   if ( nProfile == pActiveModel->video_params.user_selected_video_link_profile )
      nLevel = nLevelShift;
   else if ( nProfile == VIDEO_PROFILE_MQ )
      nLevel = lev1 + nLevelShift;
   else if ( nProfile == VIDEO_PROFILE_LQ )
      nLevel = lev1 + lev2 + nLevelShift;
   float hPointPrev = hLevel * nLevel;

   g_pRenderEngine->setStroke(250,230,150, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStrokeSize(2.0);

   for( int i=1; i<MAX_INTERVALS_VIDEO_LINK_SWITCHES; i++ )
   {
      nProfile = g_SM_VideoLinkStats.historySwitches[i]>>4;
      nLevelShift = g_SM_VideoLinkStats.historySwitches[i] & 0x0F;
      nLevel = 0;
      if ( nProfile == pActiveModel->video_params.user_selected_video_link_profile )
         nLevel = nLevelShift;
      else if ( nProfile == VIDEO_PROFILE_MQ )
         nLevel = lev1 + nLevelShift;
      else if ( nProfile == VIDEO_PROFILE_LQ )
         nLevel = lev1 + lev2 + nLevelShift;
      float hPoint = hLevel * nLevel;

      g_pRenderEngine->drawLine(xBarSt, y+hPoint, xBarSt+widthBar, y+hPointPrev);

      xBarSt -= widthBar;
      hPointPrev = hPoint;
   }
*/
   g_pRenderEngine->setStrokeSize(0);
   osd_set_colors();
}


float osd_render_stats_video_stats_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   
   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;
   // To fix if ( NULL == g_pCurrentModel || NULL == g_pSM_VideoLinkStats || 0 == g_SM_VideoLinkStats.timeLastStatsCheck )
   {
      height += height_text*s_OSDStatsLineSpacing;
      return height;
   }

   height += 15.0*height_text*s_OSDStatsLineSpacing;

   height += osd_render_stats_adaptive_video_graph_get_height();

   return height;
}

float osd_render_stats_video_stats_get_width()
{
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   return width;
}

void osd_render_stats_video_stats(float xPos, float yPos)
{   
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);

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
   float rightMargin = xPos + width;

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Video Link Stats");
   
   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   // To fix if ( NULL == g_pCurrentModel || NULL == g_pSM_VideoLinkStats || 0 == g_SM_VideoLinkStats.timeLastStatsCheck )
   {
      bool bAdaptiveVideoOn = false;
      if ( NULL != g_pCurrentModel )
         bAdaptiveVideoOn = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK?true:false;
      if ( bAdaptiveVideoOn )
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, "No Info Available");
      else
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Adaptive Video is Off.");
      return;
   }
/*
   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "User / Current Profile :");
   strcpy(szBuff1, str_get_video_profile_name(g_SM_VideoLinkStats.overwrites.userVideoLinkProfile) );
   strcpy(szBuff2, str_get_video_profile_name(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile) );
   //if ( (NULL != g_pSM_VideoDecodeStats) && (g_SM_VideoDecodeStats.uVideoStatusFlags2 & VIDEO_STATUS_FLAGS2_IS_ON_LOWER_BITRATE) )
   //   strcat(szBuff2, "-");
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%s / %s", szBuff1, szBuff2 );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "User Profile Target Bitrate:");
   str_format_bitrate(g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps, szBuff1);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff1);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Current Prof Target Bitrate:");
   str_format_bitrate(g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate, szBuff2);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff2);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Current Prof & Level Bitrate:");
   str_format_bitrate(g_SM_VideoLinkStats.overwrites.currentProfileAndLevelDefaultBitrate, szBuff2);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff2);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Video Bitrate Down-Overwrite:");
   //str_format_bitrate_no_sufix(g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile], szBuff1);
   //str_format_bitrate(g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile], szBuff2);
   //sprintf(szBuff, "%s / %s", szBuff1, szBuff2);
   str_format_bitrate(g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile], szBuff);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Current Target V. Bitrate:");
   str_format_bitrate(g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate, szBuff);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Current Level Shift:");
   sprintf(szBuff,"%d", (int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Current EC Scheme:");
   sprintf(szBuff, "%d / %d", g_SM_VideoLinkStats.overwrites.currentDataBlocks, g_SM_VideoLinkStats.overwrites.currentECBlocks);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

  

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Intervals Used for Down-Comp:");
   sprintf(szBuff, "%d", g_SM_VideoLinkStats.backIntervalsToLookForDownStep );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Intervals Used for Up-Comp:");
   sprintf(szBuff, "%d", g_SM_VideoLinkStats.backIntervalsToLookForUpStep );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Min Down Change Interval:");
   sprintf(szBuff, "%d ms", g_SM_VideoLinkStats.computed_min_interval_for_change_down );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Min Up Change Interval:");
   sprintf(szBuff, "%d ms", g_SM_VideoLinkStats.computed_min_interval_for_change_up );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;


   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Last params changed down:");

   if ( g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown == 0 )
      strcpy(szBuff, "Never");
   else if ( g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown >= g_SM_VideoLinkStats.timeLastStatsCheck )
      strcpy(szBuff, "Now");
   else
      sprintf(szBuff, "%d sec ago",  (int)((g_SM_VideoLinkStats.timeLastStatsCheck - g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown) / 1000) );

   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Last params changed up:");

   if ( g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp == 0 )
      strcpy(szBuff, "Never");
   else if ( g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp >= g_SM_VideoLinkStats.timeLastStatsCheck )
      strcpy(szBuff, "Now");
   else
      sprintf(szBuff, "%d sec ago",  (int)((g_SM_VideoLinkStats.timeLastStatsCheck - g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp) / 1000) );

   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Total Shifts:");
   sprintf(szBuff,"%d", (int)g_SM_VideoLinkStats.totalSwitches);
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   // ---------------------------------------------------------------
   // Graphs
   y += height_text_small*0.7;
*/
   osd_render_stats_adaptive_video_graph(xPos, y);
   y += osd_render_stats_adaptive_video_graph_get_height();
   osd_set_colors();
}


float osd_render_stats_video_graphs_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text * 2.0;
   float hGraphSmall = hGraph*0.6;
   
   float height = 2.0 *s_fOSDStatsMargin*1.1 + 0.9*height_text*s_OSDStatsLineSpacing;

   if ( NULL == g_pCurrentModel || NULL == g_pSM_VideoLinkGraphs || 0 == g_SM_VideoLinkGraphs.timeLastStatsUpdate )
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
   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAA AAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   return width;
}

void osd_render_stats_video_graphs(float xPos, float yPos)
{
   bool useControllerInfo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?true:false;
   
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
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

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Video Link Graphs");
   
   sprintf(szBuff,"%d ms/bar", VIDEO_LINK_STATS_REFRESH_INTERVAL_MS);
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStatsSmall, szBuff);
   float wT = g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);
   sprintf(szBuff,"%.1f sec, ", (((float)MAX_INTERVALS_VIDEO_LINK_STATS) * VIDEO_LINK_STATS_REFRESH_INTERVAL_MS)/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin-wT, yPos, s_idFontStatsSmall, szBuff);

   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;
   float yTop = y;

   osd_set_colors();
   g_pRenderEngine->setColors(get_Color_Dev());

   if ( NULL == g_pCurrentModel || NULL == g_pSM_VideoLinkGraphs || 0 == g_SM_VideoLinkGraphs.timeLastStatsUpdate )
   {
      bool bAdaptiveVideoOn = false;
      if ( NULL != g_pCurrentModel )
         bAdaptiveVideoOn = (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK?true:false;
      if ( bAdaptiveVideoOn )
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, "No Info Available");
      else
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Adaptive Video is Off.");
      return;
   }
   
   // ---------------------------------------------------------------
   // Graphs

   float dxGraph = g_pRenderEngine->textWidth(s_idFontStatsSmall, "8887 ms");
   float widthGraph = widthMax - dxGraph;
   float widthBar = widthGraph / MAX_INTERVALS_VIDEO_LINK_STATS;
   float xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   float yBottomGraph = y + hGraphSmall - g_pRenderEngine->getPixelHeight();
   float midLine = hGraph/2.0;
   int maxGraphValue = 1;
   double pc[4];
   memcpy(pc, get_Color_Dev(), 4*sizeof(double));

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
   sprintf(szBuff, "C: RX quality radio interface %d", iNIC+1);
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, szBuff);
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraphSmall - g_pRenderEngine->getPixelHeight();
   midLine = hGraphSmall/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
      if ( g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[iNIC][i] != 255 )
      if ( g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[iNIC][i] > maxGraphValue )
         maxGraphValue = g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[iNIC][i];

   sprintf(szBuff, "%d%%", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraphSmall-height_text_small*0.7, s_idFontStatsSmall, "0%");

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
      float percent = (float)(g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[iNIC][i])/(float)(maxGraphValue);
      if ( g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[iNIC][i] == 255 )
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
      else if ( g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[iNIC][i] < 100 )
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
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, "V: Max RX quality");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraphSmall - g_pRenderEngine->getPixelHeight();
   midLine = hGraphSmall/2.0;
   maxGraphValue = 100;

   sprintf(szBuff, "%d%%", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraphSmall-height_text_small*0.7, s_idFontStatsSmall, "0%");

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
      if ( g_SM_VideoLinkGraphs.vehicleRXQuality[i] == 255 )
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
      float hBar = hGraphSmall*g_SM_VideoLinkGraphs.vehicleRXQuality[i]/100.0;
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
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, "V: Max RX time gap");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraphSmall - g_pRenderEngine->getPixelHeight();
   midLine = hGraphSmall/2.0;
   maxGraphValue = 254;

   sprintf(szBuff, "%d ms", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraphSmall-height_text_small*0.7, s_idFontStatsSmall, "0 ms");

   memcpy(pc, get_Color_Dev(), 4*sizeof(double));
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
      if ( g_SM_VideoLinkGraphs.vehicleRXMaxTimeGap[i] == 255 )
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

      float percent = (float)(g_SM_VideoLinkGraphs.vehicleRXMaxTimeGap[i])/(float)(maxGraphValue);
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
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, "C: Requested packets for retransmission");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();
   midLine = hGraph/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[0][i] != 255 )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[0][i] > maxGraphValue )
         maxGraphValue = g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[0][i];

   sprintf(szBuff, "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.7, s_idFontStatsSmall, "0");

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
      float percent = (float)(g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[0][i])/(float)(maxGraphValue);
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[0][i] == 255 )
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
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, "V: Requested packets in retransmissions");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();
   midLine = hGraph/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
      if ( g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPackets[i] > maxGraphValue )
         maxGraphValue = g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPackets[i];

   sprintf(szBuff, "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.7, s_idFontStatsSmall, "0");

   memcpy(pc, get_Color_Dev(), 4*sizeof(double));
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
      float percent = (float)(g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPackets[i])/(float)(maxGraphValue);
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
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, "V: Retried packets in retransmissions");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();
   midLine = hGraph/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
      if ( g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPacketsRetried[i] > maxGraphValue )
         maxGraphValue = g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPacketsRetried[i];

   sprintf(szBuff, "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.7, s_idFontStatsSmall, "0");

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
      float percent = (float)(g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPacketsRetried[i])/(float)(maxGraphValue);
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
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, "C: Received good/reconstructed video blocks");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraph - g_pRenderEngine->getPixelHeight();
   midLine = hGraph/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i] != 255 )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i] > maxGraphValue )
         maxGraphValue = g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i];
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i] != 255 )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i] > maxGraphValue )
         maxGraphValue = g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i];

      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i] != 255 )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i] != 255 )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i] + g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i] > maxGraphValue )
         maxGraphValue = g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i] + g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i];
   }

   sprintf(szBuff, "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.7, s_idFontStatsSmall, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   memcpy(pc, get_Color_OSDText(), 4*sizeof(double));
   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      float percentGood = (float)(g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i])/(float)(maxGraphValue);
      float percentRec = (float)(g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i])/(float)(maxGraphValue);
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i] == 255 )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i] == 255 )
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
   g_pRenderEngine->drawText(xPos, y, s_idFontStatsSmall, "C: Max EC packets used");
   y += 1.3*height_text_small;

   xBarSt = xPos + dxGraph + widthGraph - widthBar - g_pRenderEngine->getPixelWidth();
   yBottomGraph = y + hGraphSmall - g_pRenderEngine->getPixelHeight();
   midLine = hGraphSmall/2.0;
   maxGraphValue = 1;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i] != 255 )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i] > maxGraphValue )
         maxGraphValue = g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i];
   }

   sprintf(szBuff, "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.2, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraphSmall-height_text_small*0.7, s_idFontStatsSmall, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraphSmall, xPos + dxGraph + widthGraph, y+hGraphSmall);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   memcpy(pc, get_Color_OSDText(), 4*sizeof(double));
   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      float percent = (float)(g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i])/(float)(maxGraphValue);
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i] == 255 )
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

   // To fix
   /*
   float x1 = xPos+dxGraph+widthGraph - widthBar*g_SM_VideoLinkStats.backIntervalsToLookForDownStep;
   g_pRenderEngine->drawLine(x1, yTop+height_text, x1, y);

   float x2 = xPos+dxGraph+widthGraph - widthBar*g_SM_VideoLinkStats.backIntervalsToLookForUpStep;
   g_pRenderEngine->drawLine(x2, yTop+height_text, x2, y);
*/

   //-----------------------
   g_pRenderEngine->setGlobalAlfa(fAlphaOrg);
   osd_set_colors();
}

float osd_render_stats_graphs_vehicle_tx_gap_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
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

   float width = g_pRenderEngine->textWidth(s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAA");
   
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

void  osd_render_stats_graphs_vehicle_tx_gap(float xPos, float yPos)
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
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

   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Vehicle Tx Stats");
   
   osd_set_colors();

   if ( ! g_bGotStatsVehicleTx )
   {
      g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "No Data.");
      return;
   }
   
   float rightMargin = xPos + width;
   float marginH = 0.0;
   float widthMax = width - marginH - 0.001;

   char szBuff[64];
   sprintf(szBuff,"%d ms/bar", g_PHVehicleTxHistory.iSliceInterval);
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, s_idFontStatsSmall, szBuff);

   float w = g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);
   sprintf(szBuff,"%.1f seconds, ", (((float)g_PHVehicleTxHistory.uCountValues) * g_PHVehicleTxHistory.iSliceInterval)/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin-w, yPos, s_idFontStatsSmall, szBuff);
   w += g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);
   
   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   // First graph 

   g_pRenderEngine->drawTextLeft(rightMargin, y-0.2*height_text, s_idFontStats, "Tx Min/Max/Avg Gaps (ms)");
   y += height_text_small + height_text*0.2;

   float dxGraph = g_pRenderEngine->textWidth(s_idFontStatsSmall, "8878");
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

   double pc[4];
   memcpy(pc, get_Color_Dev(), 4*sizeof(double));
   g_pRenderEngine->setColors(get_Color_Dev());

   int iMax = 200;
   if ( uValMaxMax <= 100 )
      iMax = 100;
   if ( uValMaxMax <= 50 )
      iMax = 50;

   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax*3/4);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph*0.25, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph*0.5, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax/4);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph*0.75, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph, s_idFontStatsSmall, "0");

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
   g_pRenderEngine->drawTextLeft(rightMargin, y-0.2*height_text, s_idFontStats, "Tx Packets / Slice");
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

   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2*0.5, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2, s_idFontStatsSmall, "0");

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
   g_pRenderEngine->drawTextLeft(rightMargin, y-0.2*height_text, s_idFontStats, "Video Packets Max Intervals (ms)");
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

   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2*0.5, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y-height_text*0.5+hGraph2, s_idFontStatsSmall, "0");

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

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Average Tx Gap:");
   if ( 0 == uAverageTxGapCount )
      strcpy(szBuff, "N/A ms");
   else
      sprintf(szBuff, "%d ms", uAverageTxGapSum/uAverageTxGapCount );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, s_idFontStats, "Average  Video Packets Interval:");
   if ( 0 == uAverageVideoPacketsIntervalCount )
      strcpy(szBuff, "N/A ms");
   else
      sprintf(szBuff, "%d ms", uAverageVideoPacketsIntervalSum/uAverageVideoPacketsIntervalCount );
   g_pRenderEngine->drawTextLeft(rightMargin, y, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;
}

float osd_render_stats_dev_adaptive_video_get_height()
{
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   float hGraph = height_text*1.0;

   float h = 2.0*height_text*s_OSDStatsLineSpacing;
   return h;
}

float osd_render_stats_dev_adaptive_video_info(float xPos, float yPos, float fWidth)
{
   u32 uActiveVehicleId = osd_get_current_data_source_vehicle_id();
   int iIndexVehicleRuntimeInfo = -1;
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_SM_RouterVehiclesRuntimeInfo.uVehiclesIds[i] == uActiveVehicleId )
         iIndexVehicleRuntimeInfo = i;
   }

   if ( -1 == iIndexVehicleRuntimeInfo )
      return 0.0;

   char szBuff[128];
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);
   
   float fRightMargin = xPos + fWidth;
   
   g_pRenderEngine->setColors(get_Color_Dev());

  
   g_pRenderEngine->drawText(xPos, yPos, s_idFontStats, "Adaptive/Keyframe Info");
   
   yPos += height_text*s_OSDStatsLineSpacing;

   return osd_render_stats_dev_adaptive_video_get_height();
}
