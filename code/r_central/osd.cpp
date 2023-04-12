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
#include "osd.h"
#include "osd_ahi.h"
#include "osd_warnings.h"
#include "osd_stats.h"
#include "osd_gauges.h"
#include "osd_lean.h"
#include "osd_plugins.h"
#include "osd_links.h"
#include "../common/string_utils.h"
#include "../../mavlink/common/mavlink.h"
#include <math.h>


#include "../renderer/render_engine.h"
#include "shared_vars.h"
#include "colors.h"
#include "../base/config.h"
#include "../base/config_video.h"
#include "../base/models.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"

#include "link_watch.h"
#include "pairing.h"
#include "local_stats.h"
#include "launchers_controller.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "timers.h"
#include "warnings.h"


bool s_bDebugOSDShowAll = false;

static u32 s_RenderCount = 0;

bool s_ShowOSDFlightEndStats = false;
u32 s_TimeOSDFlightEndStatsOn = 0;

bool s_ShowOSDFlightsStats = false;
u32 s_TimeOSDFlightsStatsOn = 0;

bool osd_is_debug()
{
   return s_bDebugOSDShowAll;
}

void osd_add_stats_flight_end()
{
   if ( s_ShowOSDFlightEndStats )
      return;

   s_TimeOSDFlightEndStatsOn = g_TimeNow;
   s_ShowOSDFlightEndStats = true;
}

void osd_remove_stats_flight_end()
{
   s_ShowOSDFlightEndStats = false;
}

void osd_add_stats_total_flights()
{
   if ( s_ShowOSDFlightsStats )
      return;

   s_TimeOSDFlightsStatsOn = g_TimeNow;
   s_ShowOSDFlightsStats = true;
}


void osd_show_voltage(float x, float y, float voltage, bool bRightAlign)
{
   Preferences* p = get_Preferences();

   osd_set_colors();

   bool bMultiLine = false;
   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      bMultiLine = true;
      bRightAlign = false;
   }
   float x0 = x;
   char szBuff[32];
   snprintf(szBuff, 31, "%.1f", voltage);
   
   if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.battery_cell_count > 0 )
      g_iComputedOSDCellCount = g_pCurrentModel->osd_params.battery_cell_count;

   float w = 0;
   if ( g_bOSDWarningBatteryVoltage )
   {
      double* pC = get_Color_OSDWarning();
      float alpha = pC[3];
      if ( (( g_TimeNow / 300 ) % 3) == 0 )
         pC[3] = 0.0;
      g_pRenderEngine->setColors(pC);
      if ( bRightAlign )
         w = osd_show_value_sufix_left(x,y,szBuff,"V", g_idFontOSDBig, g_idFontOSD);
      else
         w = osd_show_value_sufix(x,y,szBuff,"V", g_idFontOSDBig, g_idFontOSD);
      pC[3] = alpha;
   }
   else
   {
      if ( bRightAlign )
         w = osd_show_value_sufix_left(x,y,szBuff,"V", g_idFontOSDBig, g_idFontOSD);
      else
         w = osd_show_value_sufix(x,y,szBuff,"V", g_idFontOSDBig, g_idFontOSD);
   }
   osd_set_colors();

   if ( NULL != g_pCurrentModel )
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.voltage > 1000 )
   {
      if ( g_pCurrentModel->osd_params.battery_cell_count > 0 )
         g_iComputedOSDCellCount = g_pCurrentModel->osd_params.battery_cell_count;
      else
      {
         float voltage = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.voltage/1000.0;
         int cellCount = floor(voltage/4.3)+1;
         if ( cellCount > g_iComputedOSDCellCount )
         {
            g_iComputedOSDCellCount = cellCount;
            log_line("Updated battery cell count to %d (for received voltage of: %.2f)", g_iComputedOSDCellCount, voltage );
         }
      }
   }

   if ( NULL != g_pCurrentModel )
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_BATTERY_CELLS ) )
   if ( 0 != g_iComputedOSDCellCount )
   {
      x -= w;
      x -= 0.016*osd_getScaleOSD();
      if ( bMultiLine )
      {
         x = x0;
         y += osd_getFontHeightBig();
      }
      snprintf(szBuff, 31, "%.2f", 31, voltage/(float)g_iComputedOSDCellCount);

      double* pC = get_Color_OSDText();
      if ( g_bOSDWarningBatteryVoltage )
         pC = get_Color_OSDWarning();
      float alpha = pC[3];
      
      if ( g_bOSDWarningBatteryVoltage )
      {
         if ( ( g_TimeNow / 500 ) % 2 )
            pC[3] = 0.0;
         g_pRenderEngine->setColors(pC);
      }
      else
         pC = NULL;
      if ( bRightAlign )
         w += osd_show_value_sufix_left(x,y,szBuff, "V", g_idFontOSDSmall, g_idFontOSDSmall);
      else
         w += osd_show_value_sufix(x,y,szBuff, "V", g_idFontOSDSmall, g_idFontOSDSmall);

      float height_text_s = g_pRenderEngine->textHeight( 0.0, g_idFontOSDSmall);
      snprintf(szBuff, 31, "(%d cells)", g_iComputedOSDCellCount);
      if ( bRightAlign )
         osd_show_value_left(x,y+height_text_s*0.8,szBuff,g_idFontOSDSmall);
      else
         osd_show_value(x,y+height_text_s*0.8,szBuff,g_idFontOSDSmall);

      if ( NULL != pC )
         pC[3] = alpha;
   }

   osd_set_colors();
}

float osd_show_amps(float x, float y, float amps, bool bRightAlign)
{
   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      bRightAlign = false;
   char szBuff[32];
   snprintf(szBuff, 31, "%.1f", amps);
   if ( bRightAlign )
      return osd_show_value_sufix_left(x,y, szBuff, "A", g_idFontOSDBig, g_idFontOSD);
   else
      return osd_show_value_sufix(x,y, szBuff, "A", g_idFontOSDBig, g_idFontOSD);
}

float osd_show_mah(float x, float y, u32 mah, bool bRightAlign)
{
   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      bRightAlign = false;

   char szBuff[32];
   sprintf(szBuff, "%u", mah);

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      if ( bRightAlign )
         return osd_show_value_sufix_left(x,y, szBuff, "mAh", g_idFontOSDBig, g_idFontOSDSmall);
      else
         return osd_show_value_sufix(x,y, szBuff, "mAh", g_idFontOSDBig, g_idFontOSDSmall);
   }
   else
   {
      if ( bRightAlign )
         return osd_show_value_sufix_left(x,y, szBuff, "mAh", g_idFontOSDBig, g_idFontOSD);
      else
         return osd_show_value_sufix(x,y, szBuff, "mAh", g_idFontOSDBig, g_idFontOSD);
   }
}

float _osd_show_gps(float x, float y, bool bMultiLine)
{
   char szBuff[32];
   float height_text = osd_getFontHeight();
   float height_text_big = osd_getFontHeightBig();

   float x0 = x;
   float w = 0;
   if ( NULL == g_pCurrentModel || (0 == g_pCurrentModel->iGPSCount) || (!g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry) )
   {
      x += osd_show_value(x,y, "No GPS", g_idFontOSD);
      x += osd_getSpacingH();
      return x-x0;
   }

   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.gps_fix_type >= GPS_FIX_TYPE_3D_FIX || (( g_TimeNow / 300 ) % 3) != 0 )
      g_pRenderEngine->drawIcon(x,y, 0.8*height_text_big/g_pRenderEngine->getAspectRatio(), 0.8*height_text_big, g_idIconGPS);
   x += 1.0*height_text_big/g_pRenderEngine->getAspectRatio();

   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.gps_fix_type >= GPS_FIX_TYPE_3D_FIX )
      g_pRenderEngine->drawText(x-0.3*height_text_big, y-0.7*osd_getSpacingV(), height_text*0.4, g_idFontOSDSmall, "3D");

   sprintf(szBuff, "%d", g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.satelites);
   w = osd_show_value(x,y, szBuff, g_idFontOSDBig);
   x += w + 0.5*osd_getSpacingH();

   if ( bMultiLine )
   {
      osd_show_value_centered((x0+x)*0.5, y+height_text_big-height_text*0.1, "HDOP", g_idFontOSDSmall);  
      sprintf(szBuff, "%.1f", g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hdop/100.0);
      osd_show_value_centered((x0+x)*0.5, y+height_text_big-height_text*0.1+osd_getFontHeightSmall(), szBuff, g_idFontOSDSmall);
   }
   else
   {
      w = osd_show_value(x, y-height_text*0.1, "HDOP", g_idFontOSDSmall);  
      sprintf(szBuff, "%.1f", g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hdop/100.0);
      osd_show_value(x, y + osd_getFontHeightSmall(), szBuff, g_idFontOSDSmall);
      x += w;
      x += osd_getSpacingH();
   }

   u16 hdop2 = (((u16)g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[3])<<8) + ((u16)g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[4]);
   u8 satelites2 = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[1];
   
   bool bShowMore = false;
   //if ( (g_pCurrentModel->iGPSCount > 1) || ((hdop2 != 0xFFFF) && (hdop2 != 0)) || ((satelites2 != 0xFF)&&(satelites2 != 0)) )
   //   bShowMore = true;
   if ( g_pCurrentModel->iGPSCount > 1 )
      bShowMore = true;

   if ( ! bShowMore )
      return x-x0;

   // Second GPU
   
   float x1 = x;

   if ( ((g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[2] >= GPS_FIX_TYPE_3D_FIX) && (g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[2] != 0xFF)) || (( g_TimeNow / 300 ) % 3) != 0 )
      g_pRenderEngine->drawIcon(x,y, 0.8*height_text_big/g_pRenderEngine->getAspectRatio(), 0.8*height_text_big, g_idIconGPS);
   x += 1.0*height_text_big/g_pRenderEngine->getAspectRatio();

   if ( (g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[2] >= GPS_FIX_TYPE_3D_FIX) && (g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[2] != 0xFF) )
      g_pRenderEngine->drawText(x-0.3*height_text_big, y-0.7*osd_getSpacingV(), height_text*0.4, g_idFontOSDSmall, "3D");

   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[1] == 0xFF )
      strcpy(szBuff, "N/A");
   else
      sprintf(szBuff, "%d", g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[1]);
   w = osd_show_value(x,y, szBuff, g_idFontOSDBig);
   x += w + 0.5*osd_getSpacingH();

   if ( bMultiLine )
   {
      osd_show_value_centered((x1+x)*0.5, y+height_text_big-height_text*0.1, "HDOP", g_idFontOSDSmall);
      if ( hdop2 == 0xFFFF )
         strcpy(szBuff, "---");
      else
         sprintf(szBuff, "%.1f", hdop2/100.0);
      osd_show_value_centered((x1+x)*0.5, y+height_text_big-height_text*0.1+osd_getFontHeightSmall(), szBuff, g_idFontOSDSmall);
   }
   else
   {
      w = osd_show_value(x, y-height_text*0.1, "HDOP", g_idFontOSDSmall);  
      if ( hdop2 == 0xFFFF )
         strcpy(szBuff, "---");
      else
         sprintf(szBuff, "%.1f", hdop2/100.0);
      osd_show_value(x, y + osd_getFontHeightSmall(), szBuff, g_idFontOSDSmall);
      x += w;
      x += osd_getSpacingH();
   }

   return x- x0;
}

float osd_show_flight_mode(float x, float y)
{
   char szBuff[32];
   strcpy(szBuff, "NONE");
   u8 mode = 0;
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
      mode = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.flight_mode & (~FLIGHT_MODE_ARMED);

   strcpy(szBuff, model_getShortFlightMode(mode));

   osd_set_colors_background_fill();

   float w = 2.3*osd_getBarHeight()/g_pRenderEngine->getAspectRatio();
   if ( g_pCurrentModel != NULL && g_pCurrentModel->is_spectator )
      w = w * 1.2;

   g_pRenderEngine->drawRect(x, y, w, osd_getBarHeight() );

   osd_set_colors();

   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry && (g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED) )
   {
      float yPos = y + 0.5*(osd_getBarHeight()-2.0*osd_getSpacingV()-osd_getFontHeight());
      osd_show_value_centered(x+0.5*w,yPos, szBuff, g_idFontOSD);
      if ( g_pCurrentModel != NULL && g_pCurrentModel->is_spectator )
         osd_show_value_centered(x+0.5*w, yPos+0.9*osd_getFontHeight(), "SPECTATOR", g_idFontOSDSmall);
   }
   else
   {
      osd_show_value_centered(x+0.5*w,y+0.5*osd_getSpacingV(), szBuff, g_idFontOSD);
      osd_show_value_centered(x+0.5*w,y+0.7*osd_getSpacingV()+0.9*osd_getFontHeight(), "DISARMED", g_idFontOSDSmall);
      if ( g_pCurrentModel != NULL && g_pCurrentModel->is_spectator )
         osd_show_value_centered(x+0.5*w, y+0.7*osd_getSpacingV()+1.8*osd_getFontHeight(), "SPECTATOR", g_idFontOSDSmall);
   }
   return w;
}

float osd_show_video_link_mbs(float xPos, float yPos, bool bLeft)
{
   char szBuff[32];
   char szSuffix[32];
   float w = 0;
   szSuffix[0] = 0;

   bool bMultiLine = false;
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT) )
      bMultiLine = true;

   u32 totalMaxVideo_bps = 0;
   if ( NULL != g_pCurrentModel && NULL != g_pSM_RadioStats )
      totalMaxVideo_bps = g_pSM_RadioStats->radio_streams[STREAM_ID_VIDEO_1].rxBytesPerSec * 8;

   strcpy(szSuffix, "Mbps");


   if ( (!g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo) || NULL == g_pCurrentModel )
      strcpy(szBuff,"0");
   else
      sprintf(szBuff, "%.1f (%.1f)", totalMaxVideo_bps/1000.0/1000.0, g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.downlink_tx_video_bitrate/1000.0/1000.0);

   if ( NULL == g_pCurrentModel )
      return 0.0;

   u32 uRealDataRate = g_pCurrentModel->getLinkRealDataRate(0);
   if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
   if ( g_pCurrentModel->getLinkRealDataRate(1) > uRealDataRate )
      uRealDataRate = g_pCurrentModel->getLinkRealDataRate(1);
   if ( g_pCurrentModel->radioLinksParams.links_count > 2 )
   if ( g_pCurrentModel->getLinkRealDataRate(2) > uRealDataRate )
      uRealDataRate = g_pCurrentModel->getLinkRealDataRate(2);

   if ( totalMaxVideo_bps >= (float)(uRealDataRate) * DEFAULT_VIDEO_LINK_MAX_LOAD_PERCENT / 100.0 )
      g_pRenderEngine->setColors(get_Color_IconWarning());
   if ( g_bHasVideoDataOverloadAlarm && (g_TimeLastVideoDataOverloadAlarm > 0) && (g_TimeNow <  g_TimeLastVideoDataOverloadAlarm + 5000) )
      g_pRenderEngine->setColors(get_Color_IconError());

   if ( bLeft )
   {
      if ( bMultiLine )
      {
         float height_text = osd_getFontHeight();
         osd_show_value_left(xPos,yPos-0.4*osd_getSpacingV(), szBuff, g_idFontOSD);
         yPos += height_text;
         osd_show_value_left(xPos,yPos-0.4*osd_getSpacingV(), szSuffix, g_idFontOSDSmall);
      }
      else
         w = osd_show_value_sufix_left(xPos,yPos-0.4*osd_getSpacingV(), szBuff, szSuffix, g_idFontOSD, g_idFontOSDSmall);
   }
   else
      w = osd_show_value_sufix(xPos,yPos-0.4*osd_getSpacingV(), szBuff, szSuffix, g_idFontOSD, g_idFontOSDSmall);

   osd_set_colors();
   return w;
}

float osd_show_armtime(float xPos, float yPos, bool bRender)
{
   if ( NULL == g_pCurrentModel )
      return 0.0;

   int sec = (g_pCurrentModel->m_Stats.uCurrentFlightTime)%60;
   int min = (g_pCurrentModel->m_Stats.uCurrentFlightTime)/60;

   int sec_on = (g_pCurrentModel->m_Stats.uCurrentOnTime)%60;
   int min_on = (g_pCurrentModel->m_Stats.uCurrentOnTime)/60;

   float w = 0;
   char szBuff[32];

   float height_text = osd_getFontHeight();
   float height_text_big = osd_getFontHeightBig();

   if ( bRender )
      g_pRenderEngine->drawIcon(xPos, yPos+0.07*height_text_big, 0.86*height_text_big/g_pRenderEngine->getAspectRatio(), 0.86*height_text_big, g_idIconClock);
   w += 1.1*height_text_big/g_pRenderEngine->getAspectRatio();
   float w0 = w;

   if ( g_pCurrentModel->m_Stats.uCurrentFlightTime > 0 )
   {
      yPos += 0.5*(osd_getBarHeight()-2.0*osd_getSpacingV()-height_text);	
      sprintf(szBuff, "%d", min);
      if ( bRender )
         w += osd_show_value(xPos+w,yPos, szBuff, g_idFontOSD);
      else
         w += g_pRenderEngine->textWidth(g_idFontOSD, szBuff);

      w += height_text*0.11;
      if ( (g_pCurrentModel->m_Stats.uCurrentFlightTime % 2) == 0 )
      {
         g_pRenderEngine->drawBackgroundBoundingBoxes(false);
         if ( bRender )
            osd_show_value(xPos+w,yPos, ":", g_idFontOSD);
         
         if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_BACKGROUND_ON_TEXTS_ONLY )
            g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      }
      w += height_text*0.17;
      sprintf(szBuff, "%02d", sec);
      if ( bRender )
         w += osd_show_value(xPos+w,yPos, szBuff, g_idFontOSD);
      else
         w += g_pRenderEngine->textWidth(g_idFontOSD, szBuff);
      return w;
   }
   else
   {
      sprintf(szBuff, "%d", min_on);
      if ( bRender )
         w += osd_show_value(xPos+w,yPos, szBuff, g_idFontOSDSmall);
      else
         w += g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff);
      w += height_text*0.11;
      if ( (g_pCurrentModel->m_Stats.uCurrentOnTime % 2) == 0 )
      {
         g_pRenderEngine->drawBackgroundBoundingBoxes(false);
         if ( bRender )
            osd_show_value(xPos+w,yPos, ":", g_idFontOSDSmall);

         if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_BACKGROUND_ON_TEXTS_ONLY )
            g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      }
      w += height_text*0.17;
      sprintf(szBuff, "%02d", sec_on);

      if ( bRender )
         w += osd_show_value(xPos+w,yPos, szBuff, g_idFontOSDSmall);
      else
         w += g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff);

      yPos += osd_getFontHeightSmall();
      if ( bRender )
         osd_show_value(xPos+w0,yPos, "ON TIME", g_idFontOSDSmall);
      return w;
   }

   return w;
}

// Returns the width

float osd_show_home(float xPos, float yPos, bool showHeading, float fScale)
{
   if ( NULL == g_pCurrentModel || (0 == g_pCurrentModel->iGPSCount) )
      return 0.0;

   if ( (!s_bDebugOSDShowAll) && (g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE) )
      return 0.0;

   if ( ! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
      return 0.0;

   char szBuff[32];
   float height_text = osd_getFontHeight() * fScale;
   float height_text_big = osd_getFontHeightBig() * fScale;

   bool showHome = false;
   if ( NULL != g_pCurrentModel && (0 < g_pCurrentModel->iGPSCount) )
      showHome = true;
   if ( s_bDebugOSDShowAll )
      showHome = true;

   if ( ! showHome )
      return 0.0;

   float dy = 0.07*height_text_big;
   float fWidth = 1.1*height_text_big/g_pRenderEngine->getAspectRatio();
   
   if ( (g_stats_bHomeSet && link_has_fc_telemetry()) || ((g_TimeNow/500)%2) )
      g_pRenderEngine->drawIcon(xPos, yPos+dy, 0.86*height_text_big/g_pRenderEngine->getAspectRatio(), 0.86*height_text_big, g_idIconHome);

   if ( (! g_stats_bHomeSet) && (!s_bDebugOSDShowAll) )
      return fWidth;
   if ( (! link_has_fc_telemetry()) && (!s_bDebugOSDShowAll) )
      return fWidth;

   xPos += 2.0*height_text/g_pRenderEngine->getAspectRatio();
   fWidth = 2.0*height_text/g_pRenderEngine->getAspectRatio();

   float heading_home = osd_course_to(g_stats_fLastPosLat, g_stats_fLastPosLon, g_stats_fHomeLat, g_stats_fHomeLon);
   if ( g_pCurrentModel->osd_params.invert_home_arrow )
      heading_home = osd_course_to(g_stats_fHomeLat, g_stats_fHomeLon, g_stats_fLastPosLat, g_stats_fLastPosLon);
   heading_home = heading_home - 180.0;

   float rel_heading = heading_home - g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.heading; //direction arrow needs to point relative to camera/osd/craft
   if ( g_pCurrentModel->osd_params.invert_home_arrow )
      rel_heading = 180.0 - rel_heading;
   rel_heading = rel_heading + g_pCurrentModel->osd_params.home_arrow_rotate + 180;
   if (rel_heading < 0) rel_heading += 360.0;
   if (rel_heading >= 360) rel_heading -= 360.0;

   //static float rel_heading = 20;
   //rel_heading += 2.0;
   //if ( rel_heading > 360 )
   //   rel_heading = 0.0;

   fScale = (osd_getBarHeight()-2.0*osd_getSpacingV())/0.18;
   float x[5] = {0.0f,  -0.14f*fScale*g_pRenderEngine->getAspectRatio(), 0.0f, 0.14f*fScale*g_pRenderEngine->getAspectRatio(), 0.0f};
   float y[5] = {0.08f*fScale, 0.15f*fScale, -0.15f*fScale, 0.15f*fScale, 0.08f*fScale};
   for( int i=0; i<5; i++ )
      x[i] = x[i]/g_pRenderEngine->getAspectRatio();
   
   osd_rotatePoints(x, y, rel_heading, 5, xPos,yPos + 0.48*(osd_getBarHeight()-2.0*osd_getSpacingV()), 0.34);

   double* pc1 = get_Color_OSDText();
   g_pRenderEngine->setColors(pc1);

   float fStroke = g_pRenderEngine->getStrokeSize();
   g_pRenderEngine->setStrokeSize(0);
   g_pRenderEngine->fillPolygon(x,y,5);

   double pc2[4];
   pc2[0] = 0; pc2[1] = 0; pc2[2] = 0;
   pc2[3] = 0.3;
   g_pRenderEngine->setColors(pc2);
   g_pRenderEngine->setStroke(pc2, 1.5);
   g_pRenderEngine->drawPolyLine(x,y,5);
   g_pRenderEngine->setStrokeSize(fStroke);
   osd_set_colors();

   return fWidth + 0.01;
}

float osd_show_throttle(float xPos, float yPos, bool bLeft)
{
   char szBuff[32];
   float height_text = osd_getFontHeight();
   float dy = 0.5*(osd_getBarHeight() - height_text - 2.0*osd_getSpacingV());

   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
      sprintf(szBuff, "%d%%", g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.throttle);
   else
      strcpy(szBuff, "0%");

   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotRubyTelemetryExtraInfo )
   {
      u16 uThrottleInput = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtraInfo.uThrottleInput;
      if ( uThrottleInput > 2500 )
         uThrottleInput = 2500;
      char szTmp[32];
      sprintf(szTmp, " %d", (int)uThrottleInput);
      strcat(szBuff, szTmp);
   }

   float w = 0.0;
   if ( bLeft )
   {
      w = osd_show_value_left(xPos, yPos+dy,szBuff,g_idFontOSD);
      g_pRenderEngine->drawIcon(xPos - w - 0.4*osd_getSpacingH() - 1.2*height_text/g_pRenderEngine->getAspectRatio(), yPos+dy-height_text*0.1, 1.2*height_text/g_pRenderEngine->getAspectRatio(), 1.2*height_text, g_idIconThrotle);
      w += 2.0*height_text/g_pRenderEngine->getAspectRatio();
   }
   else
   {
      g_pRenderEngine->drawIcon(xPos, yPos+dy-height_text*0.1, 1.2*height_text/g_pRenderEngine->getAspectRatio(), 1.2*height_text, g_idIconThrotle);
      w = osd_show_value(xPos+0.4*osd_getSpacingH() + 1.2*height_text/g_pRenderEngine->getAspectRatio(), yPos+dy,szBuff,g_idFontOSD);
      w += 2.0*height_text/g_pRenderEngine->getAspectRatio();
   }
   return w;
}


float _osd_show_rc_rssi(float xPos, float yPos, float fScale)
{
   char szBuff[32];
   bool bHasRubyRC = false;
   
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->rc_params.rc_enabled && (!g_pCurrentModel->is_spectator) )
      bHasRubyRC = true;
 
   osd_set_colors();

   float height_text = osd_getFontHeight() * fScale;

   float x = xPos;
   float y = yPos;

   strcpy(szBuff, "RC RSSI: N/A");
   if ( NULL != g_pCurrentModel && g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo )
   {
      int val = 0;
      if ( bHasRubyRC )
         val = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_rc_rssi;
      else if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.flags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RC_RSSI )
         val = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_mavlink_rc_rssi;
      
      //if ( ! bHasRubyRC )
      //if ( val == 0 || val == 255 )
      //if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.flags & FLAG_RUBY_TELEMETRY_HAS_MAVLINK_RX_RSSI )
      //   val = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_mavlink_rx_rssi;

      if ( val != 255 )
         sprintf(szBuff, "RC RSSI: %d%%", val);
      else
         strcpy(szBuff, "RC RSSI: ---");
   }
   if ( g_bRCFailsafe )
      strcpy(szBuff, "!RC FS!");

   float w = g_pRenderEngine->textWidth(height_text, g_idFontOSDSmall, szBuff);
   if ( (!g_bRCFailsafe) || (g_bRCFailsafe && (( g_TimeNow / 300 ) % 3 )) )
      osd_show_value(x-w,y, szBuff, g_idFontOSDSmall);
   return w;
}

float osd_get_link_bars_width(float fScale)
{
   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();
   float iconHeight = height_text*0.92 + height_text_small;
   float iconWidth = 1.4*iconHeight/g_pRenderEngine->getAspectRatio();
   return iconWidth*fScale;
}

float osd_get_link_bars_height(float fScale)
{
   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();
   float iconHeight = height_text*0.92 + height_text_small - 4.0*g_pRenderEngine->getPixelHeight();
   float iconWidth = 1.5*iconHeight/g_pRenderEngine->getAspectRatio();
   return iconHeight*fScale;
}

float osd_show_link_bars(float xPos, float yPos, float fQuality, float fScale)
{
   float iconWidth = osd_get_link_bars_width(fScale);
   float iconHeight = osd_get_link_bars_height(fScale);

   osd_set_colors();
   for( int i=0; i<4; i++ )
   {
      float x = xPos - i*iconWidth/4.0;
      float h = iconHeight;
      h -= (iconHeight/4.0)*i;

      double* pc = get_Color_OSDText();
      if ( fQuality < OSD_QUALITY_LEVEL_WARNING/100.0 )
         pc = get_Color_IconWarning();
      if ( fQuality < OSD_QUALITY_LEVEL_CRITICAL/100.0 )
         pc = get_Color_IconError();
      if ( fQuality < 0.001 )
         pc = get_Color_OSDText();
      g_pRenderEngine->setColors(pc);
      g_pRenderEngine->setStroke(0,0,0,0.5);
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);

      if ( fQuality < 0.001 )
      {
         g_pRenderEngine->setFill(0,0,0,0.5);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], 0.9);
      }
      else if ( fQuality <= 1.05 - 0.25*(i+1) )
      {
         g_pRenderEngine->setFill(0,0,0,0.1);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], 0.5);
      }

      g_pRenderEngine->drawRoundRect(x-iconWidth/4.0, yPos+iconHeight-h, iconWidth/6, h, 0.003*osd_getScaleOSD()*fScale);
   }

   osd_set_colors();
   if ( fQuality < -0.1 )
      g_pRenderEngine->drawTextLeft(xPos-iconWidth*0.8, yPos, osd_getFontHeight() * fScale*0.7, g_idFontOSD, "x");
   return iconWidth;
}

float osd_show_radio_interface(int iInterfaceIndex, int iLinkNumber, bool bVehicle, float xPos, float yPos, float fScale)
{
   if ( NULL == g_pCurrentModel || (! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo) || NULL == g_pSM_RadioStats )
      return 0.0;

   bool bVertical = false;
   bool bShowPercentages = false;
   bool bShowBars = false;
   bool bShowCardInfo = false;
   bool bShowLine3AsError = false;

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      bVertical = true;

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_RADIO_LINK_QUALITY_PERCENTAGE )
      bShowPercentages = true;

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_RADIO_LINK_QUALITY_BARS )
      bShowBars = true;

   if ( (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_RADIO_LINK_INTERFACES_EXTENDED) ||
        (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_RADIO_INTERFACES_INFO) )
      bShowCardInfo = true;

   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   char szLine1[32];
   char szLine2[32];
   char szLine3[128];
   char szLine3Max[128];
   char szLine4[128];

   szLine3[0] = 0;
   szLine3Max[0] = 0;
   szLine4[0] = 0;

   if ( bShowPercentages )
   {
      strcpy(szLine1, "999%");
      strcpy(szLine2, "55 Mbps");
   }
   else
   {
      if ( g_pCurrentModel->radioLinksParams.link_datarates[iLinkNumber][0] < 0 )
      {
      strcpy(szLine1, "MCS-5");
      strcpy(szLine2, "MCS-5");
      }
      else
      {
      strcpy(szLine1, "55");
      strcpy(szLine2, "Mbps");
      }
   }
   
   if ( bShowCardInfo )
   {
      if ( bVehicle )
      {
         sprintf(szLine3, "%s: RX/TX", g_pCurrentModel->radioInterfacesParams.interface_szPort[iInterfaceIndex] );
         strcpy(szLine3Max, szLine3);

         int dbm = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_rssi_dbm[iLinkNumber]-200;
         if ( dbm < -110 )
         {
            strcat(szLine3, " N/A dBm");
            bShowLine3AsError = true;
         }
         else
         {
            char szTmp[32];
            sprintf(szTmp, " %d dBm", dbm);
            strcat(szLine3, szTmp);
         }
         strcat(szLine3Max, " -999 dBm");
      }
      else
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(iInterfaceIndex);   
         if ( NULL == pNICInfo )
         {
            strcpy(szLine3, "N/A");
            strcpy(szLine3Max, szLine3);
         }
         else
         {
            char szCardName[128];
            if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_RADIO_LINK_INTERFACES_EXTENDED )
            {
               char* szN = controllerGetCardUserDefinedName(pNICInfo->szMAC);
               if ( NULL != szN && 0 != szN[0] )
               {
                  sprintf(szCardName, "%s%s", pNICInfo->szUSBPort, (controllerIsCardInternal(pNICInfo->szMAC)?"":"(E)"));
                  strcpy(szLine4, szN);
               }
               else
               {
                  t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
                  if ( NULL == pCardInfo )
                  {
                     strcpy(szCardName, pNICInfo->szUSBPort);
                     strcpy(szLine4, "NO NAME");
                  }
                  else
                  {
                     const char* szCardModel = str_get_radio_card_model_string(pCardInfo->cardModel);
                     if ( NULL != szCardModel && 0 != szCardModel[0] )
                     {
                        sprintf(szCardName, "%s%s", pNICInfo->szUSBPort, (controllerIsCardInternal(pNICInfo->szMAC)?"":"(E)"));
                        strcpy(szLine4, szCardModel);
                     }
                     else
                     {
                        strcpy(szCardName, pNICInfo->szUSBPort);
                        strcpy(szLine4, "NO NAME");
                     }
                  }
               }
            }
            else
            {
               strcpy(szCardName, pNICInfo->szUSBPort);
               strcpy(szLine4, "NO NAME");
            }

            if ( g_pSM_RadioStats->radio_interfaces[iInterfaceIndex].openedForRead && g_pSM_RadioStats->radio_interfaces[iInterfaceIndex].openedForWrite )
               sprintf(szLine3, "%s: RX/TX", szCardName);
            else if ( g_pSM_RadioStats->radio_interfaces[iInterfaceIndex].openedForRead )
               sprintf(szLine3, "%s: RX", szCardName);
            else if ( g_pSM_RadioStats->radio_interfaces[iInterfaceIndex].openedForWrite )
               sprintf(szLine3, "%s: TX", szCardName);
            else
               sprintf(szLine3, "%s: N/A", szCardName);

            strcpy(szLine3Max, szLine3);

            if ( g_fOSDDbm[iInterfaceIndex] < -110 )
            {
               strcat(szLine3, " N/A dBm");
               bShowLine3AsError = true;
            }
            else
            {
               char szTmp[32];
               sprintf(szTmp, " %d dBm", (int)g_fOSDDbm[iInterfaceIndex]);
               strcat(szLine3, szTmp);
            }
            strcat(szLine3Max, " -999 dBm");
         }
      }
   }

   float wText = g_pRenderEngine->textWidth(height_text, g_idFontOSD, szLine1);
   float f = g_pRenderEngine->textWidth(height_text_small, g_idFontOSDSmall, szLine2);
   if ( f > wText )
      wText = f;
   
   float fWidth = wText;
   if ( bShowBars )
      fWidth += osd_get_link_bars_width(1.0);

   if ( bShowCardInfo )
   {
      f = g_pRenderEngine->textWidth(height_text_small, g_idFontOSDSmall, szLine3Max);
      if ( f > fWidth && f < fWidth*1.5 )
         fWidth = f;
   }

   float fHeight = height_text + height_text_small;
   if ( bShowCardInfo )
      fHeight += height_text;

   int nQuality = 0;
   int nDataRateVideo = 0;
   int nDataRateData = 0;

   if ( bVehicle )
   {
      nQuality = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_link_quality[iLinkNumber];
      nDataRateVideo = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_datarate[iLinkNumber]/2;
      nDataRateData = nDataRateVideo;
   }
   else
   {
      nQuality = g_pSM_RadioStats->radio_interfaces[iInterfaceIndex].rxQuality;
      nDataRateVideo = g_pSM_RadioStats->radio_interfaces[iInterfaceIndex].lastDataRateVideo/2;
      nDataRateData = g_pSM_RadioStats->radio_interfaces[iInterfaceIndex].lastDataRateData/2;
   }

   if ( bShowBars )
      osd_show_link_bars(xPos-wText, yPos, nQuality/100.0, 1.0);

   if ( g_pCurrentModel->enc_flags != MODEL_ENC_FLAGS_NONE )
   {
      float fIconSizeY = height_text*0.9;
      float fIconSizeX = 0.5 * fIconSizeY / g_pRenderEngine->getAspectRatio();
      if ( bShowBars )
         g_pRenderEngine->drawIcon(xPos-wText-osd_get_link_bars_width(1.0),yPos, fIconSizeX, fIconSizeY, g_idIconShield);
      else
         g_pRenderEngine->drawIcon(xPos-wText,yPos, fIconSizeX, fIconSizeY, g_idIconShield);
   }

   yPos -= 0.3*osd_getSpacingV();
   osd_set_colors();

   if ( bShowPercentages )
   {
      sprintf(szLine1, "%d%%", nQuality);
      sprintf(szLine2, "%d Mbps", nDataRateVideo);
      if ( g_pCurrentModel->radioLinksParams.link_datarates[iLinkNumber][0] < 0 )
         sprintf(szLine2, "MCS-%d", -g_pCurrentModel->radioLinksParams.link_datarates[iLinkNumber][0]-1);
   }
   else
   {
      sprintf(szLine1, "%d", nDataRateVideo);
      sprintf(szLine2, "Mbps");
      if ( g_pCurrentModel->radioLinksParams.link_datarates[iLinkNumber][0] < 0 )
      {
         sprintf(szLine1, "MCS-%d", -g_pCurrentModel->radioLinksParams.link_datarates[iLinkNumber][0]-1);
         sprintf(szLine2, "MCS-%d", -g_pCurrentModel->radioLinksParams.link_datarates[iLinkNumber][1]-1);
      }
   }

   g_pRenderEngine->drawText(xPos-wText, yPos, height_text, g_idFontOSD, szLine1);
   g_pRenderEngine->drawText(xPos-wText, yPos + height_text*0.9, height_text, g_idFontOSDSmall, szLine2);

   if ( ! bShowCardInfo )
      return fWidth;

   // Show radio card info on 3rd line

   if ( bShowLine3AsError )
      g_pRenderEngine->setColors(get_Color_IconError());

   wText = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, szLine3);
   g_pRenderEngine->drawText(xPos -(fWidth+wText)*0.5, yPos + height_text*1.0 + height_text_small*0.95, 0.0, g_idFontOSDSmall, szLine3);


   // line 4 (card name)

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_RADIO_LINK_INTERFACES_EXTENDED )
   {
      wText = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, szLine4);
      g_pRenderEngine->drawText(xPos -(fWidth+wText)*0.5, yPos + height_text*1.0 + height_text_small*1.8, 0.0, g_idFontOSDSmall, szLine4);
   }

   osd_set_colors();

   return fWidth;
}

float osd_render_radio_link_tag(float xPos, float yPos, int iRadioLink, bool bVehicle, bool bDraw)
{
   char szBuff1[32];
   char szBuff2[32];

   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   sprintf(szBuff1, "Link %d", iRadioLink+1);
   if ( bVehicle )
      sprintf(szBuff1, "V %d", iRadioLink+1);

   sprintf(szBuff2, "%d", g_pCurrentModel->radioLinksParams.link_frequency[iRadioLink]);

   float fWidthNb = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, szBuff1);
   float f = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, szBuff2);
   if ( f > fWidthNb )
      fWidthNb = f;

   fWidthNb += 0.5*height_text_small;
   float fHeightNb = 2.1 * height_text_small;

   if ( bDraw )
   {
      g_pRenderEngine->drawBackgroundBoundingBoxes(false);

      double* pC = get_Color_OSDText();
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], 0.6);
      g_pRenderEngine->setStroke(0,0,0,0);
      g_pRenderEngine->drawRoundRect(xPos+1.0*g_pRenderEngine->getPixelWidth(), yPos + 1.0*g_pRenderEngine->getPixelHeight(), fWidthNb, fHeightNb, 0.003*osd_getScaleOSD());

      double pC2[4] = {0,0,0,0.9};
      g_pRenderEngine->setColors(pC2);
      float fWidth = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, szBuff1);
      g_pRenderEngine->drawTextNoOutline(xPos+(fWidthNb-fWidth)*0.5, yPos, 0.0, g_idFontOSDSmall, szBuff1);

      fWidth = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, szBuff2);
      g_pRenderEngine->drawTextNoOutline(xPos+(fWidthNb-fWidth)*0.5, yPos+height_text_small, 0.0, g_idFontOSDSmall, szBuff2);

      osd_set_colors();
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         fWidth = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, "DIS");
         g_pRenderEngine->drawText(xPos+(fWidthNb-fWidth)*0.5, yPos+2.4*height_text_small, 0.0, g_idFontOSDSmall, "DIS");
      }

      if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_BACKGROUND_ON_TEXTS_ONLY )
         g_pRenderEngine->drawBackgroundBoundingBoxes(true);
   }

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      return fHeightNb;
   else
      return fWidthNb;
}

float osd_get_radio_link_height()
{
   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   float fHeightLink = height_text + height_text_small;

   if ( (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_RADIO_LINK_INTERFACES_EXTENDED) ||
        (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_RADIO_INTERFACES_INFO) )
      fHeightLink += height_text;
   return fHeightLink;
}

float osd_show_radio_link(float xPos, float yPos, int iRadioLink, bool bVehicle)
{
   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   float fMarginY = 0.5*osd_getSpacingV();
   float fMarginX = 0.5*osd_getSpacingH();

   float xStart = xPos;
   float yStart = yPos;
   float fWidthCard = 0.0;

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      xStart -= fMarginX;
      yStart += fMarginY;
      yPos = yStart;
      xPos = xStart;
      yPos += osd_render_radio_link_tag(xPos, yPos, iRadioLink, bVehicle, false);
      yPos += fMarginY;
   }
   else
   {
      xStart -= fMarginX;
      yStart += fMarginY;
      xPos = xStart;
      yPos = yStart + 0.5*fMarginY;
   }

   if ( bVehicle )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         fWidthCard = 0.0;
      else
         fWidthCard = osd_show_radio_interface(iRadioLink, iRadioLink, true, xPos,yPos, 1.0);
      if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
         yPos += osd_get_radio_link_height() + osd_getSpacingV();
      else
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            xPos -= fWidthCard - 0.9*osd_getSpacingH();
         else
            xPos -= fWidthCard - 0.5*osd_getSpacingH();
      }
   }
   else
   {
      int iStart = 0;
      int iEnd = g_pSM_RadioStats->countRadioInterfaces;
      int iDelta = 1;
      if ( !(g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT) )
      {
         iStart = g_pSM_RadioStats->countRadioInterfaces-1;
         iEnd = -1;
         iDelta = -1;
      }

      for( int iCard=iStart; iCard!=iEnd; iCard += iDelta )
      {
         if ( g_pSM_RadioStats->radio_interfaces[iCard].assignedRadioLinkId != iRadioLink )
            continue;

         fWidthCard = osd_show_radio_interface(iCard, iRadioLink, false, xPos,yPos, 1.0);
         float fHeight = osd_get_radio_link_height();

         if ( g_pSM_RadioStats->radio_links[iRadioLink].lastTxInterfaceIndex == iCard && 1 < g_pSM_RadioStats->countRadioInterfaces )
         {
            float fIconH = height_text*0.9;
            float fIconW = 0.6 * fIconH / g_pRenderEngine->getAspectRatio();
            g_pRenderEngine->drawIcon(xPos - fWidthCard, yPos + g_pRenderEngine->getPixelHeight() * 2.0 , fIconW, fIconH, g_idIconArrowUp);

            double* pC = get_Color_OSDText();
            g_pRenderEngine->setFill(0,0,0,0.2);
            g_pRenderEngine->setStroke(pC[0], pC[1], pC[2], 0.0);

            g_pRenderEngine->drawRoundRect(xPos- fWidthCard - 2.0 * g_pRenderEngine->getPixelWidth(), yPos, fWidthCard + 6.0 * g_pRenderEngine->getPixelWidth(), fHeight-3.0*g_pRenderEngine->getPixelHeight(), 0.003*osd_getScaleOSD());
            osd_set_colors();
         }

         if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
            yPos += osd_get_radio_link_height() + osd_getSpacingV();
         else
         {
            xPos -= fWidthCard + 0.5*osd_getSpacingH();
         }
      }
      if ( !(g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT) )
         xPos += 0.8*osd_getSpacingH();
   }

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      osd_render_radio_link_tag(xPos-fWidthCard-fMarginX, yStart-fMarginY, iRadioLink, bVehicle, true);
      yPos -= fMarginY;
   }
   else
   {
      xPos -= osd_render_radio_link_tag(xPos, yStart-fMarginY, iRadioLink, bVehicle, false);
      osd_render_radio_link_tag(xPos-fMarginX, yStart-fMarginY*1.5, iRadioLink, bVehicle, true);
   }

   double* pC = get_Color_OSDText();
   g_pRenderEngine->setFill(0,0,0,0.1);
   g_pRenderEngine->setStroke(pC[0], pC[1], pC[2], 0.5);

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      g_pRenderEngine->drawRoundRect(xStart-fWidthCard-fMarginX, yStart-fMarginY, fWidthCard+2.0*fMarginX, yPos - yStart + fMarginY, 0.003*osd_getScaleOSD());
   else
   {
      float fHeightLink = osd_get_radio_link_height();
      g_pRenderEngine->drawRoundRect(xPos-fMarginX, yStart-fMarginY*1.5, xStart-xPos + 2.0*fMarginX, fHeightLink + 2.0*fMarginY, 0.003*osd_getScaleOSD());
   }
   osd_set_colors();

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      return yPos - yStart + fMarginY;
   else
      return xStart - xPos + 2.0 * fMarginX;
}

float osd_show_radio_links_controller(float xPos, float yPos, float fScale)
{
   if ( (! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo) || NULL == g_pCurrentModel || NULL == g_pSM_RadioStats )
      return 0.0;

   char szBuff[128];

   float fTotalSize = 0.0;
   float fSize = 0.0;

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
   for( int iRadioLink=0; iRadioLink<g_pSM_RadioStats->countRadioLinks; iRadioLink++ )
   {
      fSize = osd_show_radio_link(xPos, yPos, iRadioLink, false);
      if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      {
         yPos += fSize + osd_getSpacingV();
         fTotalSize += fSize + osd_getSpacingV();
      }
      else
      {
         xPos -= fSize + osd_getSpacingV();
         fTotalSize += fSize + osd_getSpacingV();
      }
   }
   }
   else
   {
   for( int iRadioLink=g_pSM_RadioStats->countRadioLinks-1; iRadioLink>=0; iRadioLink-- )
   {
      fSize = osd_show_radio_link(xPos, yPos, iRadioLink, false);
      if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      {
         yPos += fSize + osd_getSpacingV();
         fTotalSize += fSize + osd_getSpacingV();
      }
      else
      {
         xPos -= fSize + osd_getSpacingV();
         fTotalSize += fSize + osd_getSpacingV();
      }
   }
   }
   return fTotalSize;
}

float osd_show_radio_links_vehicle(float xPos, float yPos, float fScale)
{
   if ( (! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo) || NULL == g_pCurrentModel || NULL == g_pSM_RadioStats )
      return 0.0;

   char szBuff[128];

   float fTotalSize = 0.0;
   float fSize = 0.0;

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
   for( int iRadioLink=0; iRadioLink<g_pSM_RadioStats->countRadioLinks; iRadioLink++ )
   {
      fSize = osd_show_radio_link(xPos, yPos, iRadioLink, true);
      if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      {
         yPos += fSize + osd_getSpacingV();
         fTotalSize += fSize + osd_getSpacingV();
      }
      else
      {
         xPos -= fSize + osd_getSpacingV();
         fTotalSize += fSize + osd_getSpacingV();
      }
   }
   }
   else
   {
   for( int iRadioLink=g_pSM_RadioStats->countRadioLinks-1; iRadioLink>=0; iRadioLink-- )
   {
      fSize = osd_show_radio_link(xPos, yPos, iRadioLink, true);
      if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      {
         yPos += fSize + osd_getSpacingV();
         fTotalSize += fSize + osd_getSpacingV();
      }
      else
      {
         xPos -= fSize + osd_getSpacingV();
         fTotalSize += fSize + osd_getSpacingV();
      }
   }
   }
   return fTotalSize;
}

float osd_show_radio_interfaces(float xPos, float yPos, float fScale)
{
   char szLine1[64];
   char szLine2[64];
   char szLineN[128];
   char szCardName[128];
   yPos -= 0.2*osd_getSpacingV();
   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   if ( NULL == g_pSM_RadioStats || NULL == g_pCurrentModel )
   {
      strcpy(szLine1, "No signal");
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->drawTextLeft(xPos, yPos + 0.5*(osd_getBarHeight()-2.0*osd_getSpacingV()-height_text), height_text, g_idFontOSD, szLine1);
      osd_set_colors();
      return height_text;
   }

   bool bMultiLine = false;
   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      bMultiLine = true;

   float x0 = xPos;
   float y0 = yPos;

   for ( int i=hardware_get_radio_interfaces_count()-1; i>=0; i-- )
   {
      float ySt = y0;
      float xSt = x0;
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);   
   
      //if ( g_fOSDDbm[i] < -100 )
      //{
      //   double* pc = get_Color_IconError();
      //   g_pRenderEngine->setColors(pc);
      //   g_pRenderEngine->setStroke(0,0,0,0.5);
      //   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      //}
      //else
         osd_set_colors();

      char* szN = controllerGetCardUserDefinedName(pNICInfo->szMAC);
      if ( NULL != szN && 0 != szN[0] )
         sprintf(szCardName, " (%s)", szN);
      else
         szCardName[0] = 0;

      strcpy(szLineN, szCardName);
      if ( bMultiLine )
         szLineN[0] = 0;

      bool bUsedForTx = false;

      int iRadioLinkId = g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId;
      if ( iRadioLinkId == -1 )
      {
         sprintf(szLine1, "%s%s: N/A", pNICInfo->szUSBPort, szLineN);
      }
      else
      {
         if ( i == g_pSM_RadioStats->radio_links[iRadioLinkId].lastTxInterfaceIndex )
            bUsedForTx = true;

         if ( pNICInfo->openedForWrite && pNICInfo->openedForRead )
            sprintf(szLine1, "%s%s: RX/TX", pNICInfo->szUSBPort, szLineN);
         else if ( pNICInfo->openedForWrite )
            sprintf(szLine1, "%s%s: TX", pNICInfo->szUSBPort, szLineN);
         else if ( pNICInfo->openedForRead )
            sprintf(szLine1, "%s%s: RX", pNICInfo->szUSBPort, szLineN);
         else
            sprintf(szLine1, "%s%s: N/A", pNICInfo->szUSBPort, szLineN);
      }

      sprintf(szLine2, "%d dbm", (int)g_fOSDDbm[i]);

      float w1 = g_pRenderEngine->textWidth(height_text_small, g_idFontOSDSmall, szLine1);
      g_pRenderEngine->drawTextLeft(x0, y0, height_text_small, g_idFontOSDSmall, szLine1);
      y0 += height_text_small;
      
      float wN = 0.0;
      if ( bMultiLine )
      if ( 0 != szCardName[0] )
      {
         float wN = g_pRenderEngine->textWidth(height_text_small, g_idFontOSDSmall, szCardName);
         g_pRenderEngine->drawTextLeft(x0, y0, height_text_small, g_idFontOSDSmall, szCardName);
         y0 += height_text_small;
      }

      float w2 = g_pRenderEngine->textWidth(height_text, g_idFontOSD, szLine2);
      g_pRenderEngine->drawTextLeft(x0, y0, height_text, g_idFontOSD, szLine2);
      y0 += height_text;

      float wMax = w1;
      if ( w2 > wMax ) wMax = w2;
      if ( wN > wMax ) wMax = wN;

      if ( bUsedForTx && (hardware_get_radio_interfaces_count() > 1) )
      {
         float fmarginy = 0.003*osd_getScaleOSD();
         float fmarginx = 0.008*osd_getScaleOSD()/g_pRenderEngine->getAspectRatio();
         g_pRenderEngine->setColors(get_Color_OSDText());   
         g_pRenderEngine->setFill(0,0,0,0.1);
         g_pRenderEngine->drawRoundRect(x0-wMax-fmarginx*0.9, ySt-fmarginy, wMax+2.0*fmarginx, (y0-ySt)+2.0*fmarginy, 0.05);
         osd_set_colors();
      }

      if ( bMultiLine )
      {
         x0 = xPos;
         y0 += osd_getSpacingV();
      }
      else
      {
         x0 -= wMax + osd_getSpacingH();
         y0 = yPos;
      }
   }

   osd_set_colors();

   if ( bMultiLine )
      return (y0 - yPos);
   return (xPos - x0);
}

float osd_show_controller_voltage(float xPos, float yPos, bool bSmall)
{
   float height_text = osd_getFontHeight();
   float x0 = xPos;
   ControllerSettings* pCS = get_ControllerSettings();
   if ( hardware_i2c_has_current_sensor() )
   if ( pCS->iShowVoltage )
   {
      if ( NULL == g_pSMVoltage )
         g_pSMVoltage = shared_mem_i2c_current_open_for_read();
      if ( NULL == g_pSMVoltage )
         return 0.0;
      float fVoltage = 0.0;
      float fCurrent = 0.0;
      if ( g_pSMVoltage->lastSetTime != MAX_U32 )
      if ( g_pSMVoltage->voltage != MAX_U32 )
         fVoltage = g_pSMVoltage->voltage*0.001f;
      if ( g_pSMVoltage->lastSetTime != MAX_U32 )
      if ( g_pSMVoltage->current != MAX_U32 )
         fCurrent = g_pSMVoltage->current*0.1f;

      char szBuff[32];
      if ( g_pSMVoltage->uParam == 0 )
         sprintf(szBuff, "%.1f V", fVoltage);
      if ( g_pSMVoltage->uParam == 1 )
         sprintf(szBuff, "%d mA", (int)fCurrent);
      if ( g_pSMVoltage->uParam == 2 )
         sprintf(szBuff, "%.1f V, %d mA", fVoltage, (int)fCurrent);
      if ( bSmall )
         xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      else
         xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSD);
      xPos -= height_text*0.2;
   }
   return x0 - xPos;
}

float osd_show_cpus(float xPos, float yPos, float fScale )
{
   char szBuff[32];
   float height_text = osd_getFontHeightSmall();
   float x0 = xPos;
   float dxIcon = 1.5*height_text/g_pRenderEngine->getAspectRatio();

   bool bMultiLine = false;
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT) )
      bMultiLine = true;

   osd_set_colors();

   // Vehicle side
   //----------------------

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_CPU_INFO ) )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo )
      {
         Preferences* p = get_Preferences();
         if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%d F",  (int)osd_convertTemperature((float)(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.temperature)));
         else
            sprintf(szBuff, "%d C",  (int)osd_convertTemperature((float)(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.temperature)));
         if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.temperature >= 70 )
         if ( (g_TimeNow/500)%2 )
            g_pRenderEngine->setColors(get_Color_IconWarning());
         if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.temperature >= 75 )
         if ( (g_TimeNow/500)%2 )
            g_pRenderEngine->setColors(get_Color_IconError());
      }
      else
         strcpy(szBuff, "N/A");
      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos += height_text*0.16;

      g_pRenderEngine->drawIcon(xPos-dxIcon, yPos-height_text*0.1, 1.0*height_text/g_pRenderEngine->getAspectRatio(), 1.0*height_text, g_idIconTemperature);
      xPos -= dxIcon;
      xPos -= height_text*0.2;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }

      osd_set_colors();

      if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo )
         sprintf(szBuff, "%d Mhz",  (int)((float)g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.cpu_mhz));// * 0.953)); // 1024 scalling
      else
         sprintf(szBuff, "- Mhz");
      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos -= height_text*0.3;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }
      if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo )
         sprintf(szBuff, "%d %%", g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.cpu_load);
      else
         strcpy(szBuff, "- %%");

      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos -= height_text*0.2;

      g_pRenderEngine->drawIcon(xPos-dxIcon, yPos-height_text*0.1, 1.0*height_text/g_pRenderEngine->getAspectRatio(), 1.0*height_text, g_idIconCPU);
      xPos -= dxIcon;
      xPos -= height_text*0.24;

      xPos -= osd_show_value_left(xPos, yPos, "V:", g_idFontOSDSmall);
      xPos -= height_text*0.4;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }
   }

   // Controller side
   // --------------------------

   Preferences* p = get_Preferences();
   if ( s_bDebugOSDShowAll || p->iShowControllerCPUInfo )
   {
      xPos -= height_text*0.5;

      if ( bMultiLine )
      {
         yPos += height_text*0.6;
         xPos = x0;
      }

      Preferences* p = get_Preferences();
      if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
         sprintf(szBuff, "%d F",  (int)osd_convertTemperature(g_ControllerTemp));
      else
         sprintf(szBuff, "%d C",  (int)osd_convertTemperature(g_ControllerTemp));

      if ( g_ControllerTemp >= 70 )
      if ( (g_TimeNow/500)%2 )
         g_pRenderEngine->setColors(get_Color_IconWarning());

      if ( g_ControllerTemp >= 75 )
      if ( (g_TimeNow/500)%2 )
         g_pRenderEngine->setColors(get_Color_IconError());

      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos += height_text*0.16;

      g_pRenderEngine->drawIcon(xPos-dxIcon, yPos-height_text*0.1, 1.0*height_text/g_pRenderEngine->getAspectRatio(), 1.0*height_text, g_idIconTemperature);
      xPos -= dxIcon;
      xPos -= height_text*0.2;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }
      osd_set_colors();

      sprintf(szBuff, "%d Mhz",  (int)((float)g_ControllerCPUSpeed)); // * 0.953) ); // 1024 scaling
      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos -= height_text*0.3;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }

      sprintf(szBuff, "%d %%", g_ControllerCPULoad);
      xPos -= osd_show_value_left(xPos, yPos, szBuff, g_idFontOSDSmall);
      xPos -= height_text*0.2;

      g_pRenderEngine->drawIcon(xPos-dxIcon, yPos-height_text*0.1, 1.0*height_text/g_pRenderEngine->getAspectRatio(), 1.0*height_text, g_idIconCPU);
      xPos -= dxIcon;
      xPos -= height_text*0.24;
      xPos -= osd_show_value_left(xPos, yPos, "C:", g_idFontOSDSmall);
      xPos -= height_text*0.24;

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }
      xPos -= osd_show_controller_voltage(xPos, yPos, true);

      if ( bMultiLine )
      {
         yPos += height_text;
         xPos = x0;
      }
   }
   return x0 - xPos;
}

float osd_show_gps_pos(float xPos, float yPos, float fScale)
{
   if ( ! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
      return 0.0;
   char szBuff[32];
   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      sprintf(szBuff, "%.6f", g_stats_fLastPosLat);
   else
      sprintf(szBuff, "%.6f,", g_stats_fLastPosLat);
   if ( NULL != g_pCurrentModel && ((g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout]) & OSD_FLAG_SCRAMBLE_GPS) )
   {
      int index = 0;
      while ( index < strlen(szBuff) )
      {
         if ( szBuff[index] == '.' )
            break;
         szBuff[index] = 'X';
         index++;
      }
   }

   osd_set_colors();

   float w = osd_show_value(xPos,yPos, szBuff, g_idFontOSDSmall);

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      yPos += osd_getFontHeightSmall();
   else
      xPos += w + osd_getSpacingH()*0.1;

   sprintf(szBuff, "%.6f", g_stats_fLastPosLon);
   if ( NULL != g_pCurrentModel && ((g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout]) & OSD_FLAG_SCRAMBLE_GPS) )
   {
      int index = 0;
      while ( index < strlen(szBuff) )
      {
         if ( szBuff[index] == '.' )
            break;
         szBuff[index] = 'X';
         index++;
      }
   }

   w = osd_show_value(xPos,yPos, szBuff, g_idFontOSDSmall);
   xPos += w + osd_getSpacingH()*0.7;
   return 0.0;
}

float osd_show_total_distance(float xPos, float yPos, float fScale)
{
   if ( ! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
      return 0.0;
   Preferences *pP = get_Preferences();
   char szBuff[128];
   char szPrefix[32];
   strcpy(szPrefix, "Total:");
   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      strcpy(szPrefix, "T:");
   if ( pP->iUnits == prefUnitsImperial || pP->iUnits == prefUnitsFeets )
   {
      if ( _osd_convertKm(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.total_distance/100.0/1000.0) > 1.0 )
         sprintf(szBuff, "%s %u mi", szPrefix, (unsigned long)_osd_convertKm(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.total_distance/100.0/1000.0));
      else
         sprintf(szBuff, "%s %u ft", szPrefix, (unsigned long)_osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.total_distance/100));
   }
   else
   {
      if ( _osd_convertKm(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.total_distance/100.0/1000.0) > 1.0 )
         sprintf(szBuff, "%s %u km", szPrefix, (unsigned long)_osd_convertKm(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.total_distance/100.0/1000.0));
      else
         sprintf(szBuff, "%s %u m", szPrefix, (unsigned long)_osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.total_distance/100));
   }
   osd_set_colors();
   float w = osd_show_value(xPos,yPos, szBuff, g_idFontOSDSmall);
   return w;
}

float osd_show_home_pos(float xPos, float yPos, float fScale)
{
   if ( ! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
      return 0.0;
   if ( NULL == g_pCurrentModel || (0 == g_pCurrentModel->iGPSCount) )
      return 0.0;
   char szBuff[32];
   sprintf(szBuff, "HLat: %.6f", g_stats_fHomeLat);
   float w = osd_show_value(xPos,yPos, szBuff, g_idFontOSD);
   xPos += w + osd_getSpacingH()*0.5;

   sprintf(szBuff, "HLon: %.6f", g_stats_fHomeLon);
   w = osd_show_value(xPos,yPos, szBuff, g_idFontOSD);
   xPos += w + osd_getSpacingH()*0.7;
   return 0.0;
}


void osd_show_recording(bool bShowWhenStopped, float xPos, float yPos)
{
   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();
   char szBuff[32];

   float w = 0.03*osd_getScaleOSD();

   static long s_lMemDiskOSDFree = 0;
   static long s_lMemDiskOSDTotal = 0;
   static u32  s_lMemDiskLastTime = 0;

   if ( (! g_bVideoRecordingStarted) && (!s_bDebugOSDShowAll) )
   {
      if ( !(g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT) )
      if ( g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VIDEO_MODE )
      if ( bShowWhenStopped )
      {
         s_lMemDiskOSDFree = 0;
         s_lMemDiskOSDTotal = 0;
         s_lMemDiskLastTime = 0;

         if ( g_bToglleAllOSDOff || g_bToglleOSDOff )
            return;
         if ( (!s_bDebugOSDShowAll) && (!(g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VIDEO_MODE )) )
            return;

         osd_set_colors();
         float hIcon = 1.4*height_text;
         float wIcon = 1.4*height_text/g_pRenderEngine->getAspectRatio();
         float dy = 0.4*(osd_getBarHeight()- hIcon);
         g_pRenderEngine->drawIcon(xPos-wIcon, yPos+dy, wIcon, hIcon, g_idIconCamera);
         osd_show_video_profile_mode(xPos-height_text*0.2-wIcon, yPos+dy + 0.3*(hIcon-height_text), g_idFontOSD, true);
      }
      return;
   }
   osd_set_colors();

   float hIcon = 1.4*height_text;
   float wIcon = 1.4*height_text/g_pRenderEngine->getAspectRatio();
   float dy = 0.4*(osd_getBarHeight()- hIcon);
   float fWidthMode = 0.0;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VIDEO_MODE ) )
   {
      g_pRenderEngine->drawIcon(xPos-wIcon, yPos+dy, wIcon, hIcon, g_idIconCamera);
      fWidthMode = osd_show_video_profile_mode(xPos-height_text*0.2-wIcon, yPos+dy + 0.3*(hIcon-height_text), g_idFontOSD, true);
   }

   xPos -= wIcon + fWidthMode + height_text*0.4;

   double pColorRed[4] = {140,0,0,1};
   double pColorRedLow[4] = {80,20,20,0.7};
   double pColorYellow[4] = {150,150,80,0.7};

   Preferences *p = get_Preferences();

   char szTime[64];
   u32 tMili = g_TimeNow - g_uVideoRecordStartTime;

   if ( p->iVideoDestination == 0 )
   {
      if ( (tMili/900)%2 )
         sprintf(szTime, "%02d:%02d", (tMili/1000)/60, (tMili/1000)%60 );
      else
         sprintf(szTime, "%02d %02d", (tMili/1000)/60, (tMili/1000)%60 );
      s_lMemDiskOSDFree = 0;
      s_lMemDiskOSDTotal = 0;
      s_lMemDiskLastTime = 0;
   }
   else
   {
      if  ( 0 == s_lMemDiskOSDTotal )
      {
         s_lMemDiskLastTime = g_TimeNow-3000;
         s_lMemDiskOSDTotal = 1;
      }
      if ( g_TimeNow > s_lMemDiskLastTime + 4000 )
      {
         s_lMemDiskLastTime = g_TimeNow;
         char szComm[1024];
         char szBuff[2048];
         char szTemp[64];
         sprintf(szComm, "df %s | sed -n 2p", TEMP_VIDEO_MEM_FOLDER);
         hw_execute_bash_command_raw(szComm, szBuff);
         //log_line("Output: [%s]", szBuff);
         long lu;
         sscanf(szBuff, "%s %ld %ld %ld", szTemp, &s_lMemDiskOSDTotal, &lu, &s_lMemDiskOSDFree);
      }
      sprintf(szTime, "%d/%d", s_lMemDiskOSDTotal/1000-s_lMemDiskOSDFree/1000, s_lMemDiskOSDTotal/1000);
   }

   if ( p->iShowBigRecordButton )
   {
      yPos += osd_getBarHeight()+osd_getSecondBarHeight()*1.2;
      xPos = 0.5-height_text*3.0;

      if ( (g_TimeNow / 1000 ) % 2 )
         pColorRed[3] = 0.0;
      else
         pColorRed[3] = 1.0;
      g_pRenderEngine->setColors(pColorRed);
      w = 1.1*osd_show_value(xPos,yPos+dy, "REC", g_idFontOSD);
      float m = w*0.18+0.5*w*0.1;

      pColorRed[3] = 1.0;
      g_pRenderEngine->setColors(pColorRed);
      g_pRenderEngine->setFill(0,0,0,0);
      g_pRenderEngine->setStrokeSize(2.0);
      g_pRenderEngine->drawRoundRect(xPos-m, yPos+dy, w+2*m,height_text+m, 0.01*osd_getScaleOSD());

      if ( (g_TimeNow / 1000 ) % 2 )
         pColorRed[3] = 0.0;
      else
         pColorRed[3] = 1.0;
      g_pRenderEngine->setColors(pColorRed);
      w = osd_show_value(xPos,yPos+dy, "REC", g_idFontOSD);

      osd_set_colors();
      osd_show_value(xPos-height_text*0.1, yPos + dy + height_text*1.2, szTime, g_idFontOSD);
      return;
   }

   pColorRed[3] = 1.0;

   if ( (g_TimeNow / 1000 ) % 2 )
      g_pRenderEngine->setColors(get_Color_OSDText());
   else
      g_pRenderEngine->setColors(pColorYellow);

   w = 1.1 * g_pRenderEngine->textWidth(height_text_small, g_idFontOSDSmall, "REC");
   float m = w*0.18+0.5*w*0.1;
   xPos -= w + 2*m;

   osd_show_value(xPos,yPos+dy, "REC", g_idFontOSDSmall);

   if ( (g_TimeNow / 1000 ) % 2 )
      g_pRenderEngine->setColors(pColorRed);
   else
      g_pRenderEngine->setColors(pColorRedLow);
   g_pRenderEngine->setStrokeSize(2.0);
   g_pRenderEngine->drawRoundRect(xPos-m, yPos+dy, w+2*m, height_text_small+m, 0.01*osd_getScaleOSD());

   if ( (g_TimeNow / 1000 ) % 2 )
      g_pRenderEngine->setColors(get_Color_OSDText());
   else
      g_pRenderEngine->setColors(pColorYellow);

   osd_show_value(xPos,yPos+dy, "REC", g_idFontOSDSmall);

   osd_set_colors();
   osd_show_value(xPos-m*0.3,yPos+dy+height_text_small*1.1, szTime, g_idFontOSDSmall);
}

void render_bars()
{
   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_BACKGROUND_ON_TEXTS_ONLY )
      return;

   osd_set_colors_background_fill();

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      float fWidth = osd_getVerticalBarWidth();
      g_pRenderEngine->drawRect(osd_getMarginX(), osd_getMarginY(), fWidth, 1.0-2*osd_getMarginY());
      g_pRenderEngine->drawRect(1.0-osd_getMarginX()-fWidth, osd_getMarginY(), fWidth, 1.0-2*osd_getMarginY());
      osd_set_colors();
      return;
   }
   float hBar = osd_getBarHeight();
   hBar += osd_getSecondBarHeight();
   g_pRenderEngine->drawRect(osd_getMarginX(), osd_getMarginY(), 1.0-2*osd_getMarginX(),hBar);
   g_pRenderEngine->drawRect(osd_getMarginX(), 1.0-hBar-osd_getMarginY(), 1.0-2*osd_getMarginX(),hBar);	

   /*
   {
      float widthRight = 0.0;
      if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.battery_show_per_cell )
         widthRight = 0.096*osd_getScaleOSD();
      else
         widthRight = 0.064*osd_getScaleOSD();
      g_pRenderEngine->drawRect(1.0-osd_getMarginX()-widthRight, 1.0-osd_getBarHeight()-osd_getSecondBarHeight()-osd_getMarginY() + g_pRenderEngine->getPixelHeight(), widthRight, osd_getSecondBarHeight());
   }
   */
   osd_set_colors();
}

void osd_show_signal_bars()
{
   if ( NULL == g_pCurrentModel )
      return;

   float height_text = osd_getFontHeight();
   float height_text_s = osd_getFontHeightSmall();
   char szBuff[64];

   int position = (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SIGNAL_BARS_MASK)>>14;

   float xPos = osd_getMarginX() + 2.0*g_pRenderEngine->getPixelWidth();
   float yPos = osd_getMarginY() + 2.0*osd_getSpacingV() + osd_getBarHeight() + osd_getSecondBarHeight();
   float fBarAspect = 9.0;
   float fBarHeight = height_text_s*0.8;
   float fBarWidth = fBarHeight * fBarAspect;

   if ( position == 1 )
      yPos = 1.0 - osd_getMarginY() - 2.0*osd_getSpacingV() - osd_getBarHeight() - osd_getSecondBarHeight() - fBarHeight;

   if ( position == 2 || position == 3 )
   {
      fBarAspect = 12.0;
      fBarHeight = height_text_s*0.6;
      fBarWidth = fBarHeight * fBarAspect;

      yPos = 1.0 - osd_getMarginY() - 2.0*osd_getSpacingV() - osd_getBarHeight() - osd_getSecondBarHeight() - fBarHeight;
   }
   if ( position == 3 )
      xPos = 1.0 - osd_getMarginX() - 2.0*g_pRenderEngine->getPixelWidth() - fBarHeight;
   

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      xPos = osd_getMarginX() + osd_getSpacingH() + osd_getVerticalBarWidth();
      yPos = osd_getMarginY() + 2.0*osd_getSpacingV();
      if ( position == 1 )
      {
         yPos = 1.0-osd_getMarginY() - 2.0*osd_getSpacingV();
      }
      if ( position == 2 )
      {
         yPos = 1.0-osd_getMarginY() - 2.0*osd_getSpacingV();
      }
      if ( position == 3 )
      {
         yPos = 1.0-osd_getMarginY() - 2.0*osd_getSpacingV();
         xPos = 1.0 - osd_getMarginX() - osd_getSpacingH() - fBarHeight - osd_getVerticalBarWidth();
      }
   }

   osd_set_colors();
   if ( NULL == g_pSM_RadioStats )
   {
      strcpy(szBuff, "No signal");
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontOSD, szBuff);
      osd_set_colors();
      return;
   }

   float dbmMin = -90;
   float dbmMax = -50;
   float xTextLeft = xPos + g_pSM_RadioStats->countRadioInterfaces * fBarHeight*1.2 + fBarHeight*0.3;
   float xTextRight = xPos - g_pSM_RadioStats->countRadioInterfaces * fBarHeight*1.2 - fBarHeight*0.1;

   for ( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      int cardIndex = i;
      if ( position == 3 || position == 1 )
         cardIndex = hardware_get_radio_interfaces_count()-i-1;
 
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(cardIndex);
      float fPercent = (g_fOSDDbm[cardIndex]-dbmMin)/(dbmMax - dbmMin);
      if ( fPercent < 0.0f ) fPercent = 0.0f;
      if ( fPercent > 1.0f ) fPercent = 1.0f;

      int iRadioLinkId = g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId;
      if ( -1 == iRadioLinkId )
         fPercent = 0.0;
      if ( g_fOSDDbm[cardIndex] < -110 )
         fPercent = 0.0;
      fPercent = fPercent * (float)g_pSM_RadioStats->radio_interfaces[cardIndex].rxQuality / 100.0;
      osd_set_colors_background_fill();
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      g_pRenderEngine->setStroke(get_Color_OSDText());
      if ( position == 0 || position == 1 )
         g_pRenderEngine->drawRoundRect(xPos, yPos, fBarWidth, fBarHeight, 0.0);
      else
         g_pRenderEngine->drawRoundRect(xPos, yPos-fBarWidth, fBarHeight, fBarWidth, 0.0);

      osd_set_colors();

      if ( position == 0 || position == 1 )
         g_pRenderEngine->drawRoundRect(xPos+3.0*g_pRenderEngine->getPixelWidth(), yPos+3.0*g_pRenderEngine->getPixelHeight(), fPercent*(fBarWidth-6.0*g_pRenderEngine->getPixelWidth()), fBarHeight-6.0*g_pRenderEngine->getPixelHeight(), 0.0);
      else
         g_pRenderEngine->drawRoundRect(xPos+3.0*g_pRenderEngine->getPixelWidth(), yPos-3.0*g_pRenderEngine->getPixelHeight()-fPercent*(fBarWidth-6.0*g_pRenderEngine->getPixelHeight()), fBarHeight-6.0*g_pRenderEngine->getPixelWidth(), fPercent*(fBarWidth-6.0*g_pRenderEngine->getPixelHeight()), 0.0);

      char szCardName[128];
      char* szN = controllerGetCardUserDefinedName(pNICInfo->szMAC);
      if ( NULL != szN && 0 != szN[0] )
         sprintf(szCardName, "%s (%s)", pNICInfo->szUSBPort, szN);
      else
         strcpy(szCardName, pNICInfo->szUSBPort);

      if ( -1 == iRadioLinkId )
         sprintf(szBuff, "%s: N/A", szCardName);
      else if ( pNICInfo->openedForWrite && pNICInfo->openedForRead )
         sprintf(szBuff, "%s: RX/TX, %d %%", szCardName, (int)(fPercent*100.0));
      else if ( pNICInfo->openedForWrite )
         sprintf(szBuff, "%s: TX, %d %%", szCardName, (int)(fPercent*100.0));
      else if ( pNICInfo->openedForRead )
         sprintf(szBuff, "%s: RX, %d %%", szCardName, (int)(fPercent*100.0));
      else
         sprintf(szBuff, "%s: N/A, %d %%", szCardName, (int)(fPercent*100.0));

      osd_set_colors();
      if ( position == 0 || position == 1 )
         g_pRenderEngine->drawText(xPos + fBarWidth + height_text*0.25, yPos, height_text, g_idFontOSDSmall, szBuff); 
      else if ( position == 2 )
         g_pRenderEngine->drawText(xTextLeft, yPos-height_text_s*1.2 - (float)(hardware_get_radio_interfaces_count()-i-1)*height_text*1.05, height_text_s, g_idFontOSDSmall, szBuff); 
      else
         g_pRenderEngine->drawTextLeft(xTextRight, yPos-height_text_s*1.2 - (float)i*height_text*1.05, height_text_s, g_idFontOSDSmall, szBuff); 

      if ( 0 == position )
         yPos += height_text*1.05;
      if ( 1 == position )
         yPos -= height_text*1.05;
      if ( 2 == position )
         xPos += fBarHeight*1.2;
      if ( 3 == position )
         xPos -= fBarHeight*1.2;
   }

   osd_set_colors();
}

void osd_show_grid()
{
   if ( NULL == g_pCurrentModel )
      return;

   float fA = g_pRenderEngine->setGlobalAlfa(0.6);
   double* pc = get_Color_OSDText();
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], pc[3]);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], pc[3]);
 

   g_pRenderEngine->setStrokeSize(2.01);
   if ( g_pRenderEngine->getScreenHeight() > 720 )
      g_pRenderEngine->setStrokeSize(3.01);

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_CROSSHAIR) )
   {
      g_pRenderEngine->drawLine(0.5 - 0.02/g_pRenderEngine->getAspectRatio(), 0.5, 0.5 + 0.02/g_pRenderEngine->getAspectRatio(), 0.5 ); 
      g_pRenderEngine->drawLine(0.5, 0.5 - 0.02, 0.5, 0.5 + 0.02 ); 
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_DIAGONAL) )
   {
      g_pRenderEngine->drawLine(0.001, 0.001, 0.999, 0.999 ); 
      g_pRenderEngine->drawLine(0.999, 0.001, 0.001, 0.999);  
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_THIRDS_SMALL) )
   {
      float fSizeY = 0.02;
      float fSizeX = fSizeY/g_pRenderEngine->getAspectRatio();
      float fMargin = 0.25;
      g_pRenderEngine->drawLine(fMargin, fMargin, fMargin + fSizeX, fMargin ); 
      g_pRenderEngine->drawLine(fMargin, fMargin, fMargin, fMargin + fSizeY ); 
        
      g_pRenderEngine->drawLine(fMargin, 1.0-fMargin, fMargin+fSizeX, 1.0-fMargin );
      g_pRenderEngine->drawLine(fMargin, 1.0-fMargin, fMargin, 1.0-fMargin - fSizeY ); 

      g_pRenderEngine->drawLine(1.0-fMargin, fMargin, 1.0-fMargin - fSizeX, fMargin ); 
      g_pRenderEngine->drawLine(1.0-fMargin, fMargin, 1.0-fMargin, fMargin + fSizeY ); 

      g_pRenderEngine->drawLine(1.0-fMargin, 1.0-fMargin, 1.0-fMargin - fSizeX, 1.0-fMargin ); 
      g_pRenderEngine->drawLine(1.0-fMargin, 1.0-fMargin, 1.0-fMargin, 1.0-fMargin - fSizeY );
   }
   else if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_SQUARES) )
   {
      g_pRenderEngine->drawLine(0.001, 0.25, 0.999, 0.25 ); 
      g_pRenderEngine->drawLine(0.001, 0.75, 0.999, 0.75); 
      g_pRenderEngine->drawLine(0.25, 0.001, 0.25, 0.999 ); 
      g_pRenderEngine->drawLine(0.75, 0.001, 0.75, 0.999 ); 
   }


   g_pRenderEngine->setGlobalAlfa(fA);
   osd_set_colors();
}

float render_osd_voltagesamps(float x, float y)
{
   float x0 = x;

   bool bMultiLine = false;
   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      bMultiLine = true;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_BATTERY ) )
   {
      y -= osd_getSecondBarHeight()+0.3*osd_getSpacingV();
      osd_show_voltage(x,y, g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.voltage/1000.0, true);
      if ( bMultiLine )
         y += 2.0*osd_getFontHeightBig() + osd_getSpacingV();
      else
         y += osd_getSecondBarHeight();
   }
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_BATTERY ) )
   {
      osd_show_amps(x,y, g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.current/1000.0, true);
      if ( bMultiLine )
      {
         x = x0;
         y += osd_getSecondBarHeight();
         osd_show_mah(x,y, g_pCurrentModel->m_Stats.uCurrentTotalCurrent/10, true);
      }
      else
      {
         x -= 0.05*osd_getScaleOSD();
         x -= osd_show_mah(x,y, g_pCurrentModel->m_Stats.uCurrentTotalCurrent/10, true);
      }
   }
   return x0 - x;
}

void _render_osd_left_right()
{
   Preferences* p = get_Preferences();
   char szBuff[64];
   char szBuff2[64];
   float height_text = osd_getFontHeight();
   float height_text_big = osd_getFontHeightBig();
   float height_text_small = osd_getFontHeightSmall();
   float vSpacing = osd_getSpacingV()*2.0;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_BGBARS) )
      render_bars();

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_HID_IN_OSD) )
      osd_show_HID();

   if ( s_bDebugOSDShowAll ||
      (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_CROSSHAIR) ||
      (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_DIAGONAL) ||
      (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_SQUARES) ||
      (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_THIRDS_SMALL) )
      osd_show_grid();

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_SIGNAL_BARS) )
      osd_show_signal_bars();

   // -------------------------------------------------
   // Left side

   float x0 = osd_getMarginX() + osd_getSpacingH()*0.5;
   float y0 = osd_getMarginY() + osd_getSpacingV()*1.5;

   float x = x0;
   float y = y0;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_GPS_INFO ) )
   {
      _osd_show_gps(x,y, false);
      y += height_text_big + vSpacing;
   }
 
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_GPS_POS ) )
   {
      osd_show_gps_pos(x,y, 0.9);
      y += 2.0*height_text_small + vSpacing*0.4;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_TOTAL_DISTANCE ) )
   {
      osd_show_total_distance(x,y, 1.0);
      y += height_text_small + vSpacing*0.4;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_FLIGHT_MODE ) )
   {
      osd_show_flight_mode(x,y);
      y += height_text_big + 2.0*vSpacing;
   }
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_TIME ) )
   {
      osd_show_armtime(x,y, true);
      y += height_text + vSpacing;
   }


   bool bShowAnySpeed = false;
   bool bShowBothSpeeds = false;
   if ( s_bDebugOSDShowAll || ((g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_GROUND_SPEED) && (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_AIR_SPEED)) )
      bShowBothSpeeds = true;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_GROUND_SPEED) || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_AIR_SPEED) )
      bShowAnySpeed = true;

   if ( bShowAnySpeed || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_DISTANCE) )
      y += 1.0*vSpacing;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_GROUND_SPEED) )
   {
      strcpy(szBuff,"0");
      if ( link_has_fc_telemetry() )
      {
        if ( p->iUnits == prefUnitsMeters || p->iUnits == prefUnitsFeets )
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed/100.0f-1000.0));
        else
           sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed/100.0f-1000.0)*3.6f));
        if ( 0 == g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed )
           strcpy(szBuff,"0");
      }
      if ( bShowBothSpeeds )
         sprintf(szBuff2,"G: %s", szBuff);
      else
         strcpy(szBuff2, szBuff);

      if ( p->iUnits == prefUnitsMetric )
         osd_show_value_sufix(x, y, szBuff2, "km/h", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsMeters )
         osd_show_value_sufix(x, y, szBuff2, "m/s", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsImperial )
         osd_show_value_sufix(x, y, szBuff2, "mi/h", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(x, y, szBuff2, "ft/s", g_idFontOSD, g_idFontOSDSmall);

      y += height_text;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_AIR_SPEED) )
   {
      strcpy(szBuff,"0");
      if ( link_has_fc_telemetry() )
      {
        if ( p->iUnits == prefUnitsMeters || p->iUnits == prefUnitsFeets )
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.aspeed/100.0f-1000.0));
        else
           sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.aspeed/100.0f-1000.0)*3.6f));
        if ( 0 == g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.aspeed )
           strcpy(szBuff,"0");
      }
      if ( bShowBothSpeeds )
         sprintf(szBuff2,"A: %s", szBuff);
      else
         strcpy(szBuff2, szBuff);

      if ( p->iUnits == prefUnitsMetric )
         osd_show_value_sufix(x, y, szBuff2, "km/h", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsMeters )
         osd_show_value_sufix(x, y, szBuff2, "m/s", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsImperial )
         osd_show_value_sufix(x, y, szBuff2, "mi/h", g_idFontOSD, g_idFontOSDSmall);
      else
         osd_show_value_sufix(x, y, szBuff2, "ft/s", g_idFontOSD, g_idFontOSDSmall);

      y += height_text;
   }

   bool bShowDistance = false;
   if ( s_bDebugOSDShowAll || ((g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_DISTANCE) && (g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE)) )
   if ( (g_stats_bHomeSet && link_has_fc_telemetry()) || ((g_TimeNow/500)%2) )
   {
      bShowDistance = true;
      float fDistMeters = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.distance/100.0;
      if ( fDistMeters/1000.0 > 500.0 )
      {
         g_stats_fHomeLat = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.latitude/10000000.0f;
         g_stats_fHomeLon = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.longitude/10000000.0f;
      }
      sprintf(szBuff, "D: %u", (unsigned int) _osd_convertMeters(fDistMeters));
      if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
      {
        if ( _osd_convertMeters(fDistMeters) >= 10000.0 )
           osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
        else
           osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
      }
      else
      {
         if ( _osd_convertMeters(fDistMeters) >= 10000 )
            osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);
         else
            osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);
      }
      y += height_text_big + 1.0*vSpacing;
   }

   bool bShowAlt = false;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_ALTITUDE ) )
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_GENERIC ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE || 
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
   {
     bShowAlt = true;
     if ( link_has_fc_telemetry() )
     {
        if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_LOCAL_VERTICAL_SPEED )
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.vspeed/100.0f-1000.0));
        else
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.vspeed/100.0f-1000.0));
     }
     else
        sprintf(szBuff,"0.0");
     if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
        osd_show_value_sufix(x+0.5*osd_getSpacingH(),y, removeTrailingZero(szBuff), "ft/s",g_idFontOSD, g_idFontOSDSmall);
     else
        osd_show_value_sufix(x+0.5*osd_getSpacingH(),y, removeTrailingZero(szBuff), "m/s",g_idFontOSD, g_idFontOSDSmall);

     y += height_text;

     float alt = _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.altitude_abs/100.0f-1000.0);
     if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.altitude_relative )
        alt = _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.altitude/100.0f-1000.0);
     if ( fabs(alt) < 10.0 )
     {
        if ( fabs(alt) < 0.1 )
          alt = 0.0;
        sprintf(szBuff, "H: %.1f", alt);
     }
     else
        sprintf(szBuff, "H: %d", (int)alt);
     if ( (! link_has_fc_telemetry()) || (alt < -500.0) )
        sprintf(szBuff, "H: ---");
     if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
        osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
     else
        osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);

      y += height_text_big + 1.0*vSpacing;
   }

   if ( bShowDistance || bShowAlt )
     y += 1.0*vSpacing;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_BATTERY ) )
   {
      y += 2.0*vSpacing;
      render_osd_voltagesamps(x,y);
      y += 3.0*height_text_big + vSpacing*0.2;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_DISTANCE ) )
   {
      y += 1.0*vSpacing;
      g_pRenderEngine->drawIcon(x, y+height_text_big*0.1, height_text_big*0.8/g_pRenderEngine->getAspectRatio(), height_text_big*0.8, g_idIconHeading);
      x += 1.1*height_text_big/g_pRenderEngine->getAspectRatio();

      if ( link_has_fc_telemetry() )
         sprintf(szBuff, "%03d", g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.heading);
      else
         sprintf(szBuff, "---");

      x += osd_show_value(x,y+(height_text_big-height_text)*0.5, szBuff, g_idFontOSD);
      x += osd_show_value(x+height_text*0.05,y, "o", g_idFontOSDSmall);
      x = x0;
      y += height_text + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_HOME ) )
   {
      osd_show_home(x, y, true, 1.0);   
      y += height_text + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_THROTTLE ) )
   {
      osd_show_throttle(x,y,false);
      y += height_text + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_PITCH ) )
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_GENERIC ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE || 
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )  
         sprintf(szBuff, "Pitch: %d", (int)(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.pitch/100.0-180.0));
      else
         sprintf(szBuff, "Pitch: -");
      osd_show_value(x,y, szBuff, g_idFontOSD);
      y += height_text + vSpacing;
   }

   //---------------------------------------------
   // Right side

   x0 = 1.0 - osd_getMarginX() - osd_getSpacingH()*0.5;
   y0 = osd_getMarginY() + osd_getSpacingV()*1.5;

   x = x0;
   y = y0;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_RC_RSSI ) )
   {
      _osd_show_rc_rssi(x,y, 1.0);
      y += height_text + 1.0*vSpacing;
   }

   /*
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS) )
   {
      y += osd_show_radio_links_vehicle(x,y, 1.0);
      y += 2.0*vSpacing;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_RADIO_LINKS) )
   {
      y += osd_show_radio_links_controller(x,y, 1.0);
      y += 2.0*vSpacing;
   }
   */

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_RADIO_LINKS) || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS) )
      for( int iRadioLink=0; iRadioLink<g_pSM_RadioStats->countRadioLinks; iRadioLink++ )
      {
         y += osd_show_radio_link_new(1.0 - osd_getMarginX() - osd_getVerticalBarWidth() ,y, iRadioLink, false);
         y += 2.0*vSpacing;
      }

   //if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_RADIO_INTERFACES ) )
   //{
   //   float fheight = osd_show_radio_interfaces(x,y, 0.6);
   //   if ( NULL != g_pSM_RadioStats )
   //      y += fheight + vSpacing;
   //}

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VIDEO_MODE) )
   {
      osd_set_colors();
      float dy = -0.2*height_text;
      g_pRenderEngine->drawIcon(x-1.0*height_text, y+dy, 1.4*height_text/g_pRenderEngine->getAspectRatio(), 1.4*height_text, g_idIconCamera);
      osd_show_video_profile_mode(x-1.1*height_text, y, g_idFontOSD, true);
      y += height_text + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VIDEO_MBPS ) )
   {
      osd_show_video_link_mbs(x,y, true);
      y += height_text_big + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VIDEO_MODE_EXTENDED ) )
   {
      if ( NULL != g_psmvds && pairing_hasReceivedVideoStreamData())
      {
         //sprintf(szBuff, "%d x %d", g_psmvds->width, g_psmvds->height);
         strcpy(szBuff, "N/A");
         for( int i=0; i<getOptionsVideoResolutionsCount(); i++ )
            if ( g_iOptionsVideoWidth[i] == g_psmvds->width )
            if ( g_iOptionsVideoHeight[i] == g_psmvds->height )
            {
               strcpy(szBuff, g_szOptionsVideoRes[i]);
               break;
            }
      }
      else
         sprintf(szBuff, "[waiting]");
      osd_show_value_left(x,y, szBuff, g_idFontOSD);
      y += height_text;

      if ( NULL != g_psmvds && pairing_hasReceivedVideoStreamData())
      {
         if ( g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VIDEO_MODE )
            sprintf(szBuff, "%d fps", g_psmvds->fps); 
      }
      else
         sprintf(szBuff, "[waiting]");
      osd_show_value_left(x,y, szBuff, g_idFontOSDSmall);
      y += height_text_small + vSpacing;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_CPU_INFO ) ||
        p->iShowControllerCPUInfo )
   {
      osd_show_cpus(x,y, 1.0);
      y += 4.0*height_text_big + 2.0*vSpacing;
   }

}

void osd_debug()
{
   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry = true;
   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.flags = FC_TELE_FLAGS_ARMED | FC_TELE_FLAGS_HAS_ATTITUDE;
   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.flight_mode = FLIGHT_MODE_STAB | FLIGHT_MODE_ARMED;
   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.gps_fix_type = GPS_FIX_TYPE_3D_FIX;
   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hdop = 111;

   if ( s_RenderCount < 2 || g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed < 5000 )
   {
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.altitude = 101000;
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.altitude_abs = 101000;
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.vspeed = 101000;
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed = 101000;
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.heading = 2;
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.roll = 180*100;
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.pitch = 180*100;
   }

   if ( s_RenderCount % 10 )
   {
      // Current pos: 47.14378586227096, 27.583722513468402
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.latitude = 47.136136 * 10000000 + 20000 * sin(s_RenderCount/50.0);
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.longitude = 27.5777667 * 10000000 + 20000 * cos(s_RenderCount/50.0);
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.flags |= FC_TELE_FLAGS_POS_CURRENT;
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.distance = 100 * distance_meters_between( g_stats_fHomeLat, g_stats_fHomeLon, g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.latitude/10000000.0, g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.longitude/10000000.0 );
   }
   else
   {
      // Home:  47.13607918623999, 27.577757896625428
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.latitude = 47.136136 * 10000000;
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.longitude = 27.5777667 * 10000000;
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.flags |= FC_TELE_FLAGS_POS_HOME;     
   }

   u32 tm = g_TimeNow;

   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.satelites++;
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.satelites > 24 )
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.satelites = 0;

   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.throttle++;
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.throttle > 100 )
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.throttle = 0;

   //g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.heading = 120;
   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.heading+=2;
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.heading > 360 )
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.heading -= 360;

   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.voltage+=50;
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.voltage > 16000 )
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.voltage = 0;

   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.altitude_abs+=20;
   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.altitude+=20;
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.altitude > 105240 )
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.altitude = 99580;

   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed+=10;
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed > 104000 )
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed = 100000;

   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.vspeed+=10;
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.vspeed > 102540 )
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.vspeed = 98770;

   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.roll+=20;
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.roll > 230*100 )
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.roll = 130*100;

   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.pitch+=20;
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.pitch > 230*100 )
      g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.pitch = 130*100;

   u16 uWindHeading = ((u16)g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[7] << 8) | g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[8];
   uWindHeading++;
   if ( uWindHeading > 359 )
      uWindHeading = 1;
   
   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[7] = uWindHeading >> 8;
   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[8] = uWindHeading & 0xFF;

   u16 uWindSpeed = ((u16)g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[9] << 8) | g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[10];
   uWindSpeed+=5;
   if ( uWindSpeed > 1000)
      uWindSpeed = 1;

   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[9] = uWindSpeed >> 8;
   g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[10] = uWindSpeed & 0xFF;
}

void osd_render_elements()
{
   if ( NULL != g_pCurrentModel && ( 0 < g_pCurrentModel->iGPSCount) && g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_POS_CURRENT )
      {
         g_stats_fLastPosLat = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.latitude/10000000.0f;
         g_stats_fLastPosLon = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.longitude/10000000.0f;
      }
      if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_POS_HOME )
      {
         if ( ! g_stats_bHomeSet )
            warnings_add("Home Position Aquired and Stored");
         g_stats_bHomeSet = true;
         g_stats_fHomeLat = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.latitude/10000000.0f;
         g_stats_fHomeLon = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.longitude/10000000.0f;
      }
   }

   if ( s_bDebugOSDShowAll )
      osd_debug();

   char szBuff[64];
   float height_text = osd_getFontHeight();
   float height_text_big = osd_getFontHeightBig();
   float height_text_small = osd_getFontHeightSmall();
   float x,x0,y,y0,w;

   Preferences* p = get_Preferences();
   set_Color_OSDText( p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2], ((float)p->iColorOSD[3])/100.0);
   set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
   osd_set_colors();

   if ( (g_bToglleAllOSDOff || g_bToglleOSDOff) && (!s_bDebugOSDShowAll) )
   {
      if ( ! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
      {
         g_pRenderEngine->enableFontScaling(false);
         return;
      }

      float x = osd_getMarginX() + osd_getSpacingV() + 0.225*osd_getScaleOSD();
      float y = osd_getMarginY();

      if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
         x = 0.5 - 0.02*osd_getScaleOSD();
      if ( g_bToglleAllOSDOff || g_bToglleOSDOff )
         x = 0.5 - 0.02*osd_getScaleOSD();
      osd_show_recording(true,x,y);


      // Bottom part
      //--------------------------
      x = 1.0 - osd_getSpacingH()-osd_getMarginX();
      y = 1.0 - osd_getMarginY() - osd_getBarHeight()-osd_getSecondBarHeight() + osd_getSpacingV();
      osd_show_voltage(x,y, g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.voltage/1000.0, true);
   
      x = 1.0 - osd_getSpacingH()-osd_getMarginX();
      y += osd_getSecondBarHeight();
      osd_show_amps(x,y, g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.current/1000.0, true);

      x -= 0.074*osd_getScaleOSD();
      osd_show_mah(x,y, g_pCurrentModel->m_Stats.uCurrentTotalCurrent/10, true);

      g_pRenderEngine->enableFontScaling(false);
      return;
   }

   if ( !(g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_ENABLED) )
      return;

   if ( g_pCurrentModel->osd_params.layout == osdLayoutLean )
   {
      render_osd_layout_lean();
      osd_set_colors();

      float x = osd_getMarginX() + osd_getSpacingV() + 0.225*osd_getScaleOSD();
      float y = osd_getMarginY();
      osd_show_recording(false,x,y);
      g_pRenderEngine->enableFontScaling(false);
      return;
   }

   if ( g_pCurrentModel->osd_params.layout == osdLayoutLeanExtended )
   {
      render_osd_layout_lean_extended();
      osd_set_colors();

      float x = osd_getMarginX() + osd_getSpacingV() + 0.225*osd_getScaleOSD();
      float y = osd_getMarginY();
      osd_show_recording(false,x,y);
      g_pRenderEngine->enableFontScaling(false);
      return;
   }

   osd_set_colors();

   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      _render_osd_left_right();
      osd_set_colors();

      float x = osd_getMarginX() + osd_getSpacingV() + 0.225*osd_getScaleOSD();
      float y = osd_getMarginY();
      x = 0.5 - 0.02*osd_getScaleOSD();
      osd_show_recording(true,x,y);
      g_pRenderEngine->enableFontScaling(false);
      return;
   }
   
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_BGBARS) )
      render_bars();
   
   osd_set_colors();

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_HID_IN_OSD) )
      osd_show_HID();

   if ( s_bDebugOSDShowAll ||
      (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_CROSSHAIR) ||
      (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_DIAGONAL) ||
      (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_SQUARES) ||
      (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_GRID_THIRDS_SMALL) )
      osd_show_grid();
  
   // Top part - left
   // ------------------------------

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_GPS_POS ) )
   {
      float y = 1.0 - osd_getMarginY() - osd_getBarHeight() - osd_getSecondBarHeight() + osd_getSpacingV();
      float x = 0.3 + 0.07*osd_getScaleOSD();
      osd_show_gps_pos(x,y, 0.9);
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_TOTAL_DISTANCE ) )
   {
      float y = 1.0 - osd_getMarginY() - osd_getBarHeight() - osd_getSecondBarHeight() + osd_getSpacingV();
      float x = 0.2*osd_getScaleOSD();
      osd_show_total_distance(x,y, 1.0);
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_SIGNAL_BARS ) )
      osd_show_signal_bars();

   x = osd_getMarginX() + osd_getSpacingH();
   y = osd_getMarginY() + osd_getSpacingV();

   x = osd_getMarginX() + osd_getSpacingH()*0.5;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_GPS_INFO ) )
      x += _osd_show_gps(x,y, true);
   else
      x += osd_getSpacingH()*0.5;

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_FLIGHT_MODE ) )
   {
      x += osd_show_flight_mode(x,osd_getMarginY());
      x += osd_getSpacingH();
   }
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_TIME ) )
   {
      if ( !(g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_TIME_LOWER) )
        osd_show_armtime(x,y, true);
   }

   // Top part - right
   //----------------------------------------

   x = 1.0 - osd_getMarginX() - 0.5*osd_getSpacingH();

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_RC_RSSI ) )
   {
      x -= _osd_show_rc_rssi(x,y, 1.0) + osd_getSpacingH();
   }

   /*
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS ) )
   {
      x -= osd_show_radio_links_vehicle(x,osd_getMarginY() + 0.5*osd_getSpacingV(), 1.0);
      //x -= osd_getSpacingH();
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_RADIO_LINKS ) )
   {
      x -= osd_show_radio_links_controller(x,osd_getMarginY() + 0.5*osd_getSpacingV(), 1.0);
      //x -= osd_getSpacingH();
   }
   */
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_RADIO_LINKS) || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS) )
      for( int iRadioLink=0; iRadioLink<g_pSM_RadioStats->countRadioLinks; iRadioLink++ )
      {
         x -= osd_show_radio_link_new(x,osd_getMarginY() + 0.5*osd_getSpacingV(), iRadioLink, true);
         x -= osd_getSpacingH();
      }

   //if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_RADIO_INTERFACES ) )
   //   osd_show_radio_interfaces(x,y, 0.6);
   float xStartRecording = x;
   osd_show_recording(true,x,y);

   // Bottom part - left
   //------------------------------------

   if ( ! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
      return;

   x = osd_getMarginX() + osd_getSpacingH()*0.6;
   y = 1.0 - osd_getMarginY()-osd_getBarHeight()+osd_getSpacingV();
   y0 = 1.0 - osd_getMarginY()-osd_getBarHeight() - osd_getSecondBarHeight() + osd_getSpacingV();
   x0 = x;

   if ( s_bDebugOSDShowAll || ((g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_DISTANCE) && (g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE) ) )
   if ( (g_stats_bHomeSet && link_has_fc_telemetry()) || ((g_TimeNow/500)%2) )
   {
      float fDistMeters = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.distance/100.0;
      if ( fDistMeters/1000.0 > 500.0 )
      {
         g_stats_bHomeSet = true;
         g_stats_fHomeLat = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.latitude/10000000.0f;
         g_stats_fHomeLon = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.longitude/10000000.0f;
      }
      sprintf(szBuff, "D: %u", (unsigned int) _osd_convertMeters(fDistMeters));
      if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
      {
        if ( _osd_convertMeters(fDistMeters) >= 10000.0 )
           osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
        else
           osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
      }
      else
      {
         if ( _osd_convertMeters(fDistMeters) >= 10000 )
            osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);
         else
            osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);
      }
   }

   bool bShowBothSpeeds = false;
   if ( s_bDebugOSDShowAll || ((g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_GROUND_SPEED) && (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_AIR_SPEED)) )
      bShowBothSpeeds = true;
   float xSpeed = x;
   if ( bShowBothSpeeds )
      xSpeed -= 0.5 * osd_getSpacingH();
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_GROUND_SPEED) )
   {
      char szBuff2[64];
      strcpy(szBuff,"0");
      if ( link_has_fc_telemetry() )
      {
        if ( p->iUnits == prefUnitsMeters || p->iUnits == prefUnitsFeets )
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed/100.0f-1000.0));
        else
           sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed/100.0f-1000.0)*3.6f));

        if ( 0 == g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.hspeed )
           strcpy(szBuff,"0");
      }

      if ( bShowBothSpeeds )
         sprintf(szBuff2, "G: %s", szBuff);
      else
         strcpy(szBuff2, szBuff);

      if ( p->iUnits == prefUnitsMetric )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "km/h", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsMeters )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "m/s", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsImperial )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "mi/h", g_idFontOSD, g_idFontOSDSmall);
      else
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "ft/s", g_idFontOSD, g_idFontOSDSmall);

      xSpeed += 0.5*osd_getSpacingH();
   }
      
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_AIR_SPEED) )
   {
      char szBuff2[64];
      strcpy(szBuff,"0");
      if ( link_has_fc_telemetry() )
      {
        if ( p->iUnits == prefUnitsMeters || p->iUnits == prefUnitsFeets )
           sprintf(szBuff, "%.1f", _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.aspeed/100.0f-1000.0));
        else
           sprintf(szBuff, "%.1f", _osd_convertKm((g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.aspeed/100.0f-1000.0)*3.6f));
        if ( 0 == g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.aspeed )
           strcpy(szBuff,"0");
      }

      if ( bShowBothSpeeds )
         sprintf(szBuff2, "A: %s", szBuff);
      else
         strcpy(szBuff2, szBuff);

      if ( p->iUnits == prefUnitsMetric )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "km/h", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsMeters )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "m/s", g_idFontOSD, g_idFontOSDSmall);
      else if ( p->iUnits == prefUnitsImperial )
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "mi/h", g_idFontOSD, g_idFontOSDSmall);
      else
         xSpeed += osd_show_value_sufix(xSpeed, y0, szBuff2, "ft/s", g_idFontOSD, g_idFontOSDSmall);

      xSpeed += 0.5*osd_getSpacingH();
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_GROUND_SPEED) || (g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_AIR_SPEED) || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_DISTANCE) )
   {
      x += 0.08*osd_getScaleOSD();
      if ( bShowBothSpeeds )
         x += g_pRenderEngine->textWidth(height_text, g_idFontOSD, "AAA");
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_ALTITUDE ) )
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_GENERIC ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE || 
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
   {
     float alt = _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.altitude_abs/100.0f-1000.0);
     if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.altitude_relative )
        alt = _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.altitude/100.0f-1000.0);
     if ( fabs(alt) < 10.0 )
     {
        if ( fabs(alt) < 0.1 )
          alt = 0.0;
        sprintf(szBuff, "H: %.1f", alt);
     }
     else
        sprintf(szBuff, "H: %d", (int)alt);
     if ( (! link_has_fc_telemetry()) || (alt < -500.0) )
        sprintf(szBuff, "H: ---");
     if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
        osd_show_value_sufix(x,y, szBuff, "ft", g_idFontOSDBig, g_idFontOSD);
     else
        osd_show_value_sufix(x,y, szBuff, "m", g_idFontOSDBig, g_idFontOSD);

     //if ( ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.vspeed/100.0f - 1000.0 ) > 0.001 )
     //   g_pRenderEngine->drawText(x, y,height_text*0.95, g_idFontOSDIcons, "");
     //else if ( ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.vspeed/100.0f - 1000.0 ) < -0.001 )
     //   g_pRenderEngine->drawText(x, y,height_text*0.95, g_idFontOSDIcons, "");
     //else
     //   g_pRenderEngine->drawText(x, y,height_text*0.95, g_idFontOSD, "-");

     if ( link_has_fc_telemetry() )
     {
        float vSpeed = 0.0;
        if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_LOCAL_VERTICAL_SPEED )
           vSpeed = _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.vspeed/100.0f-1000.0);
        else
           vSpeed = _osd_convertMeters(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.vspeed/100.0f-1000.0);
        if ( fabs(vSpeed) < 0.1 )
           vSpeed = 0.0;
        sprintf(szBuff, "%.1f", vSpeed);
     }
     else
        sprintf(szBuff,"0.0");

     if ( bShowBothSpeeds )
      x += 1.0 * osd_getSpacingH();

     if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
        osd_show_value_sufix(x,y0, removeTrailingZero(szBuff), "ft/s",g_idFontOSD, g_idFontOSDSmall);
     else
        osd_show_value_sufix(x,y0, removeTrailingZero(szBuff), "m/s",g_idFontOSD, g_idFontOSDSmall);

     x += 0.06*osd_getScaleOSD();
     if ( alt > 999.99 )
        x += 0.02*osd_getScaleOSD();
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_DISTANCE ) )
   {
      g_pRenderEngine->drawIcon(x, y+height_text_big*0.14, height_text_big*0.8/g_pRenderEngine->getAspectRatio(), height_text_big*0.8, g_idIconHeading);
      x += 1.2*height_text_big/g_pRenderEngine->getAspectRatio();

      if ( link_has_fc_telemetry() )
         sprintf(szBuff, "%03d", g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.heading);
      else
         sprintf(szBuff, "---");

      x += osd_show_value(x,y+(height_text_big-height_text)*0.5, szBuff, g_idFontOSD);
      x += osd_show_value(x+height_text*0.05,y, "o", g_idFontOSDSmall);
      x += 1.5*osd_getSpacingH();
   }


   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_HOME ) )
   {
      x += osd_show_home(x, y, true, 1.0);
      x += osd_getSpacingH(); 
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_WIND ) )
   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )
   {
      float fIconHeight = 1.0*height_text_big;
      float fIconWidth = fIconHeight/g_pRenderEngine->getAspectRatio();
      g_pRenderEngine->drawIcon(x,y, fIconWidth, fIconHeight, g_idIconWind);
      x += fIconWidth + height_text_small*0.4 / g_pRenderEngine->getAspectRatio();
      u16 uDir = ((u16)g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[7] << 8) | g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[8];
      u16 uSpeed = ((u16)g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[9] << 8) | g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.extra_info[10];
      if ( 0 == uDir )
      {
         strcpy(szBuff, "H: N/A");
         sprintf(szBuff, "H: %d", (int)uDir-1);
         osd_show_value(x,y-height_text_small*0.2, szBuff, g_idFontOSDSmall);
      }
      else
      {
         float fScale = (osd_getBarHeight()-2.0*osd_getSpacingV())/0.21;
         float xp[5] = {0.0f,  -0.14f*fScale*g_pRenderEngine->getAspectRatio(), 0.0f, 0.14f*fScale*g_pRenderEngine->getAspectRatio(), 0.0f};
         float yp[5] = {0.08f*fScale, 0.15f*fScale, -0.15f*fScale, 0.15f*fScale, 0.08f*fScale};
         for( int i=0; i<5; i++ )
            xp[i] = xp[i]/g_pRenderEngine->getAspectRatio();
         
         float rel_heading = (float)(uDir-1) - g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.heading; //direction arrow needs to point relative to camera/osd/craft
         //rel_heading -= 180;

         osd_rotatePoints(xp, yp, rel_heading, 5, x + 0.02/g_pRenderEngine->getAspectRatio(), y, 0.34);

         double* pc1 = get_Color_OSDText();
         g_pRenderEngine->setColors(pc1);

         float fStroke = g_pRenderEngine->getStrokeSize();
         g_pRenderEngine->setStrokeSize(0);
         g_pRenderEngine->fillPolygon(xp,yp,5);

         double pc2[4];
         pc2[0] = 0; pc2[1] = 0; pc2[2] = 0;
         pc2[3] = 0.3;
         g_pRenderEngine->setColors(pc2);
         g_pRenderEngine->setStroke(pc2, 1.5);
         g_pRenderEngine->drawPolyLine(xp,yp,5);
         g_pRenderEngine->setStrokeSize(fStroke);
         osd_set_colors();
      }
      if ( 0 == uSpeed )
         strcpy(szBuff, "N/A");
      else
      {
         sprintf(szBuff, "%.1f m/s", ((float)uSpeed-1)/100.0);
         if ( p->iUnits == prefUnitsMetric )
            sprintf(szBuff, "%.1f km/h", ((float)uSpeed-1)*3.6/100.0);
         else if ( p->iUnits == prefUnitsImperial )
            sprintf(szBuff, "%.1f mi/h", _osd_convertKm(((float)uSpeed-1)*3.6/100.0));
         else if ( p->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%.1f ft/s", _osd_convertMeters(((float)uSpeed-1)/100.0));
      }
      x += osd_show_value(x,y+height_text_small*0.76, szBuff, g_idFontOSD);
      x += osd_getSpacingH();
   }

   // Bottom part - right
   //------------------------------------

   x = 1.0 - osd_getMarginX() - osd_getSpacingH()*0.6;
   y = 1.0 - osd_getMarginY()-osd_getBarHeight()+osd_getSpacingV();

   x -= render_osd_voltagesamps(x,y);
   x -= osd_getSpacingH();

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_TIME ) )
   {
      if ( (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_TIME_LOWER) )
      {
         x -= osd_getSpacingH()*0.5;
         float fW = osd_show_armtime(x,y-osd_getSpacingV(),false);
         x -= osd_show_armtime(x-fW,y-osd_getSpacingV(), true);
         x -= osd_getSpacingH();
      }
   }

   float xStart = x;
   float xEnd1 = x;
   float xEnd2 = x;
   float xEnd3 = x;
   bool bShownThrottle = false;
   bool bShownPitch = false;
   bool bShownTemp = false;
   float yTemp = y;
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_THROTTLE ) )
   {
      yTemp = 1.0 - osd_getMarginY()-osd_getSpacingV() - height_text*1.2;
      xEnd1 -= osd_show_throttle(xStart,yTemp,true);
      xEnd1 -= osd_getSpacingH();
      yTemp -= height_text*0.9;
      bShownThrottle = true;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_PITCH ) )
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_GENERIC ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE || 
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
   {
      if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry )  
         sprintf(szBuff, "%d", (int)(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.pitch/100.0-180.0));
      else
         sprintf(szBuff, "-");
      xEnd2 -= osd_show_value_left(xStart,yTemp, szBuff, g_idFontOSD);
      xEnd2 -= height_text*0.15;
      xEnd2 -= osd_show_value_left(xEnd2,yTemp + (height_text-height_text_small)*0.5, "Pitch: ", g_idFontOSDSmall);
      xEnd2 -= osd_getSpacingH();
      yTemp -= height_text*0.9;
      bShownPitch = true;
   }
   
   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags3[g_iCurrentOSDVehicleLayout] & OSD_FLAG3_SHOW_FC_TEMPERATURE ) )
   {
      if ( (! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetry) || 0 == g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.temperature )
         strcpy(szBuff, "N/A");
      else
      {
         if ( p->iUnits == prefUnitsImperial || p->iUnits == prefUnitsFeets )
            sprintf(szBuff, "%d F", (int)osd_convertTemperature(((float)g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.temperature)-100.0));
         else
            sprintf(szBuff, "%d C", (int)osd_convertTemperature(((float)g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.temperature)-100.0));
      }
      xEnd3 -= osd_show_value_left(xStart,yTemp, szBuff, g_idFontOSD);
      xEnd3 -= height_text*0.15;
      xEnd3 -= osd_show_value_left(xEnd3,yTemp + (height_text-height_text_small)*0.5, "Temp: ", g_idFontOSDSmall);
      xEnd3 -= osd_getSpacingH();
      bShownTemp = true;
   }

   x = xEnd1;
   if ( xEnd2 < x )
      x = xEnd2;
   if ( xEnd3 < x )
      x = xEnd3;

   // Top part full layout right
   //--------------------------------------------------------

   x = 0.2*osd_getScaleOSD()+osd_getMarginX();
   y = osd_getMarginY() + osd_getBarHeight() + 0.5*osd_getSpacingV();

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VIDEO_MBPS ) )
   {
      x = xStartRecording;
      x -= osd_show_video_link_mbs(x,y, true);
      x -= osd_getSpacingH()*0.6;
   }

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_VIDEO_MODE_EXTENDED ) )
   {
      if ( NULL != g_psmvds && pairing_hasReceivedVideoStreamData())
      {
         //sprintf(szBuff, "%d x %d  %d fps", g_psmvds->width, g_psmvds->height, g_psmvds->fps);
         strcpy(szBuff, "N/A");
         for( int i=0; i<getOptionsVideoResolutionsCount(); i++ )
            if ( g_iOptionsVideoWidth[i] == g_psmvds->width )
            if ( g_iOptionsVideoHeight[i] == g_psmvds->height )
            {
               sprintf(szBuff, "%s %d fps", g_szOptionsVideoRes[i], g_psmvds->fps);
               break;
            }
      }
      else
         sprintf(szBuff, "[waiting]");
      x += osd_show_value_left(x,y, szBuff, g_idFontOSDSmall);
   }

   // Top part full layout right line 3
   //--------------------------------------------------------

   y = osd_getMarginY() + osd_getBarHeight() + 0.7*osd_getSpacingV();
   x = 1.0 - osd_getMarginX() - osd_getSpacingH();

   if ( s_bDebugOSDShowAll || (g_pCurrentModel->osd_params.osd_flags[g_iCurrentOSDVehicleLayout] & OSD_FLAG_SHOW_CPU_INFO ) ||
        p->iShowControllerCPUInfo )
   {
      y = osd_getMarginY() + osd_getBarHeight() + osd_getSecondBarHeight() + 0.7*osd_getSpacingV();
      x = 1.0 - osd_getMarginX();
      x -= osd_show_cpus(x,y, 1.0);
   }

   g_pRenderEngine->enableFontScaling(false);
}

void osd_render_instruments()
{
   if ( NULL == g_pCurrentModel )
      return;

   if ( g_pCurrentModel->is_spectator && (!(g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE)) )
      return;

   if ( pairing_isStarted() && pairing_isReceiving() && NULL != g_pCurrentModel && g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id != g_pCurrentModel->vehicle_id )
      return;

   if ( g_bToglleAllOSDOff || g_bToglleOSDOff )
      return;

   g_iCurrentOSDVehicleLayout = g_pCurrentModel->osd_params.layout;
   if ( !(g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_ENABLED) )
      return;

   float fAlfaOrg = g_pRenderEngine->getGlobalAlfa();
   Preferences* p = get_Preferences();
   set_Color_OSDText( p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2], ((float)p->iColorOSD[3])/100.0);
   set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
   osd_set_colors();

   if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_HAS_ATTITUDE )
      osd_show_ahi(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.roll/100.0-180.0, g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetry.pitch/100.0-180.0);
   else
      osd_show_ahi(0,0);

   g_pRenderEngine->setGlobalAlfa(fAlfaOrg);
}

void osd_render_stats()
{
   if ( NULL == g_pCurrentModel || (! g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo) || g_pCurrentModel->is_spectator )
      return;

   if ( pairing_isStarted() && pairing_isReceiving() && NULL != g_pCurrentModel && g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id != g_pCurrentModel->vehicle_id )
      return;

   float fAlfaOrg = g_pRenderEngine->getGlobalAlfa();
   Preferences* p = get_Preferences();
   set_Color_OSDText( p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2], ((float)p->iColorOSD[3])/100.0);
   set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
   osd_set_colors();


   bool showStats = true;
   if ( g_bToglleAllOSDOff || g_bToglleStatsOff )
      showStats = false;
   if ( s_bDebugOSDShowAll )
      showStats = true;

   if ( !(g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_ENABLED) )
      return;

   if ( showStats )
      osd_render_stats_panels();


   if ( ! g_bIsRouterPacketsHistoryGraphOn )
   if ( s_ShowOSDFlightEndStats )
   {
      osd_render_stats_flight_end(0.8);
      if ( g_TimeNow > s_TimeOSDFlightEndStatsOn + 20000 )
         s_ShowOSDFlightEndStats = false;
   }

   if ( ! g_bIsRouterPacketsHistoryGraphOn )
   if ( s_ShowOSDFlightsStats )
   {
      osd_render_stats_flights(1.0);
      if ( g_TimeNow > s_TimeOSDFlightsStatsOn + 10000 )
         s_ShowOSDFlightsStats = false;
   }

   g_pRenderEngine->setGlobalAlfa(fAlfaOrg);
}

void osd_render_warnings()
{
   if ( NULL == g_pCurrentModel )
     return;

   if ( g_pCurrentModel->is_spectator && (!(g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE)) )
      return;

   g_iCurrentOSDVehicleLayout = g_pCurrentModel->osd_params.layout;

   if ( s_bDebugOSDShowAll || (! g_bIsRouterPacketsHistoryGraphOn) )
      osd_warnings_render();
}


void osd_show_monitor()
{
   Preferences* p = get_Preferences();
   if ( NULL == p )
       return;

      float fScreenScale = 1.0;
      if ( p->iOSDScreenSize == 1 ) fScreenScale = 0.95;
      if ( p->iOSDScreenSize == 2 ) fScreenScale = 0.92;
      if ( p->iOSDScreenSize == 3 ) fScreenScale = 0.88;
      if ( p->iOSDScreenSize == 4 ) fScreenScale = 0.84;
      if ( p->iOSDScreenSize == 5 ) fScreenScale = 0.80;
   osd_setMarginX(0.5*(1.0-fScreenScale));
   osd_setMarginY(0.5*(1.0-fScreenScale)*g_pRenderEngine->getAspectRatio()*0.8);

   set_Color_OSDText( p->iColorOSD[0], p->iColorOSD[1], p->iColorOSD[2], ((float)p->iColorOSD[3])/100.0);
   set_Color_OSDOutline( p->iColorOSDOutline[0], p->iColorOSDOutline[1], p->iColorOSDOutline[2], ((float)p->iColorOSDOutline[3])/100.0);
   osd_set_colors();
   g_pRenderEngine->setStroke(0,0,0,0.7);
   g_pRenderEngine->setStrokeSize(2.0);
   float fDotHeight = 4.0*g_pRenderEngine->getPixelHeight();
   float fDotWidth = 4.0*g_pRenderEngine->getPixelWidth();

   int pos = s_RenderCount%10;
   g_pRenderEngine->drawRect(osd_getMarginX() + pos*fDotWidth*2.0, 1.0 - osd_getMarginY()-fDotHeight*5.0, fDotWidth, fDotHeight);

   int dy = (g_pProcessStatsRouter->uLoopCounter)%4;
   pos = (g_pProcessStatsRouter->uLoopCounter/4)%10;
   g_pRenderEngine->drawRect(osd_getMarginX() + pos*fDotWidth*2.0, 1.0 - osd_getMarginY()-fDotHeight*3.0+dy*0.5*fDotHeight, fDotWidth, fDotHeight);

   osd_set_colors();

   char szBuff[32];
   sprintf(szBuff, "%d", g_nTotalControllerCPUSpikes );
   g_pRenderEngine->drawText(osd_getMarginX(), 1.0 - osd_getMarginY() - fDotHeight*5.0 - g_pRenderEngine->textHeight(g_idFontOSDSmall), g_idFontOSDSmall, szBuff);
}

void osd_render_all()
{
   if ( NULL == g_pCurrentModel )
      return;

   Preferences* p = get_Preferences();
   
   s_RenderCount++;

   if ( (NULL != p) && (p->iShowProcessesMonitor) )
      osd_show_monitor();

   if ( g_pCurrentModel->is_spectator && (!(g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE)) )
      return;

   g_iCurrentOSDVehicleRuntimeInfoIndex = 0;

   if ( g_pCurrentModel->relay_params.uRelayVehicleId != 0 )
   if ( g_pCurrentModel->relay_params.uRelayFlags & RELAY_FLAGS_TELEMETRY )
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId > 0 )
   if ( g_pCurrentModel->relay_params.uCurrentRelayMode != RELAY_MODE_NONE )
   {
      for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      {
         if ( g_VehiclesRuntimeInfo[i].uVehicleId != g_pCurrentModel->relay_params.uRelayVehicleId )
            continue;
         g_iCurrentOSDVehicleRuntimeInfoIndex = i;
         break;
      }    
   }
   
   // Wrong model ?
   if ( pairing_isStarted() && pairing_isReceiving() && g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id != g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].uVehicleId )
      return;

   if ( 0 == g_iCurrentOSDVehicleRuntimeInfoIndex )
   if ( pairing_isStarted() && pairing_isReceiving() && g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id != g_pCurrentModel->vehicle_id )
      return;

   float fAlfaOrg = g_pRenderEngine->getGlobalAlfa();

   g_iCurrentOSDVehicleLayout = g_pCurrentModel->osd_params.layout;
   
   osd_setMarginX(0.0);
   osd_setMarginY(0.0);

   u32 tr = ((g_pCurrentModel->osd_params.osd_preferences[g_iCurrentOSDVehicleLayout])>>8) & 0xFF;
   osd_set_transparency((int)tr);

   u32 scale = g_pCurrentModel->osd_params.osd_preferences[g_iCurrentOSDVehicleLayout] & 0xFF;
   osd_setScaleOSD((int)scale);

   scale = (g_pCurrentModel->osd_params.osd_preferences[g_iCurrentOSDVehicleLayout]>>16) & 0x0F;
   osd_setScaleOSDStats((int)scale);

   if ( NULL == p )
      log_softerror_and_alarm("Failed to get pointer to preferences structure.");
   else
   {
      float fScreenScale = 1.0;
      if ( p->iOSDScreenSize == 1 ) fScreenScale = 0.95;
      if ( p->iOSDScreenSize == 2 ) fScreenScale = 0.92;
      if ( p->iOSDScreenSize == 3 ) fScreenScale = 0.88;
      if ( p->iOSDScreenSize == 4 ) fScreenScale = 0.84;
      if ( p->iOSDScreenSize == 5 ) fScreenScale = 0.80;
      osd_setMarginX(0.5*(1.0-fScreenScale));
      osd_setMarginY(0.5*(1.0-fScreenScale)*g_pRenderEngine->getAspectRatio()*0.8);
   }

   if ( NULL != g_pSM_RadioStats )
   for ( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      g_fOSDDbm[i] = 0.96 * g_fOSDDbm[i] + 0.04 * (float)g_pSM_RadioStats->radio_interfaces[i].lastDbm;
   }


   if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_SHOW_BACKGROUND_ON_TEXTS_ONLY )
   {
      g_pRenderEngine->drawBackgroundBoundingBoxes(true);
      double color2[4];
      color2[0] = color2[1] = color2[2] = 0;
      color2[0] = 0;
      color2[3] = 1.0;
      switch ( tr )
      {
         case 0: color2[3] = 0.05; break;
         case 1: color2[3] = 0.26; break;
         case 2: color2[3] = 0.5; break;
         case 3: color2[3] = 0.9; break;
      }
      g_pRenderEngine->setFontBackgroundBoundingBoxFillColor(color2);
      g_pRenderEngine->setBackgroundBoundingBoxPadding(0.0);
   }
   osd_render_elements();
   osd_render_instruments();
   osd_plugins_render();
   g_pRenderEngine->drawBackgroundBoundingBoxes(false);
   osd_render_stats();
   osd_render_warnings();

   if ( (NULL != p) && (p->iShowProcessesMonitor) )
      osd_show_monitor();

   //if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_ENABLED )
   if ( g_pCurrentModel->relay_params.uRelayFlags & RELAY_FLAGS_SHOW_OSD )
   if ( ! (g_bToglleAllOSDOff || g_bToglleOSDOff) )
   {
      osd_set_colors();
      if ( g_pCurrentModel->osd_params.osd_flags2[g_iCurrentOSDVehicleLayout] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
         osd_render_relay( 0.5, 1.0 - osd_getMarginY(), true);
      else
         osd_render_relay( 0.5, 1.0 - osd_getMarginY() - osd_getBarHeight() - osd_getSecondBarHeight() - osd_getSpacingV(), true);
   }

   g_pRenderEngine->drawBackgroundBoundingBoxes(false);
   g_pRenderEngine->setGlobalAlfa(fAlfaOrg);
}