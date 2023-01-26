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
#include "osd_warnings.h"
#include "osd.h"

#include "colors.h"
#include "../renderer/render_engine.h"
#include "shared_vars.h"
#include "pairing.h"
#include "link_watch.h"
#include "timers.h"
#include "warnings.h"
#include "osd_common.h"

extern bool s_bDebugOSDShowAll;

bool g_bOSDWarningBatteryVoltage = false;

void osd_warnings_reset()
{
   g_bOSDWarningBatteryVoltage = false;
   g_bHasVideoDataOverloadAlarm = false;
   g_bHasVideoTxOverloadAlarm = false;
}

void osd_warnings_render()
{
   if ( ! pairing_wasReceiving() )
      return;

   if ( g_bHasVideoDataOverloadAlarm )
   if ( (NULL != g_psmvds) && (!(g_psmvds->encoding_extra_flags & ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE)) )
   {
      g_bHasVideoDataOverloadAlarm = false;
      if ( NULL != g_pPopupVideoOverloadAlarm )
      {
         if ( popups_has_popup(g_pPopupVideoOverloadAlarm) )
            popups_remove(g_pPopupVideoOverloadAlarm);
         g_pPopupVideoOverloadAlarm = NULL;
      }

      warnings_add("Video overload alarm cleared.", g_idIconCamera);
   }

   float fAlfaOrg = g_pRenderEngine->getGlobalAlfa();

   float height_text = g_pRenderEngine->textHeight( 0.0, g_idFontOSDWarnings);
   float height_text_big = g_pRenderEngine->textHeight( 0.0, g_idFontOSDBig);
   float sizeIcon = height_text*1.1;
   float dyAlarm = height_text*1.8;
   float yAlarm = 1.0 - 0.05*osd_getScaleOSD()-osd_getMarginX() - 2.0*osd_getBarHeight() - height_text;
   float yAlarmIcon = yAlarm;//- height_text*0.1;
   float xAlarmIcon = osd_getMarginX() + 0.007*osd_getScaleOSD();
   float xAlarm = xAlarmIcon + sizeIcon*1.8/g_pRenderEngine->getAspectRatio();

   if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      xAlarm += osd_getVerticalBarWidth();
      xAlarmIcon += osd_getVerticalBarWidth();
   }
   float fTextScale = 0.9;

   if ( g_bVideoProcessing || s_bDebugOSDShowAll )
   if ( ( g_TimeNow / 500 ) % 2 )
   {
      osd_set_colors();
      g_pRenderEngine->drawIcon(xAlarmIcon, yAlarmIcon, 1.2*height_text/g_pRenderEngine->getAspectRatio(), 1.2*height_text, g_idIconCamera);
      g_pRenderEngine->drawText(xAlarm, yAlarm, height_text*fTextScale, g_idFontOSDWarnings, "Processing video file...");

      yAlarm -= dyAlarm;
      yAlarmIcon -= dyAlarm;
   }

   if ( NULL != g_pCurrentModel )
   if ( NULL != g_pPHRTE )
   {
   u8 air_flags = g_pPHRTE->throttled;
   if ( (0 != (air_flags & 0x0F)) || s_bDebugOSDShowAll )
   if ( ( g_TimeNow / 500 ) % 2 )
   {
      g_pRenderEngine->setColors(get_Color_IconWarning());
      g_pRenderEngine->drawIcon(xAlarmIcon, yAlarmIcon, 1.2*height_text/g_pRenderEngine->getAspectRatio(), 1.2*height_text, g_idIconCPU);
      osd_set_colors();
      char szBuff[128];
      szBuff[0] = 0;
      if ( air_flags & 0b1000 )
      {
         strcat(szBuff, "Vechilce Temperature");
      }
      if ( (air_flags & 0b0100) || s_bDebugOSDShowAll )
      {
         if ( 0 != szBuff[0] )
            strcat(szBuff, ", ");
         strcat(szBuff, "Vechicle CPU Throttled");
      }
      if ( air_flags & 0b0010 )
      {
         if ( 0 != szBuff[0] )
            strcat(szBuff, ", ");
         strcat(szBuff, "Vechicle Frequency Capped");
      }
      if ( air_flags & 0b0001 )
      {
         if ( 0 != szBuff[0] )
            strcat(szBuff, ", ");
         strcat(szBuff, "Vechicle Voltage Too Low");
      }
      g_pRenderEngine->drawText(xAlarm, yAlarm, height_text*fTextScale, g_idFontOSDWarnings, szBuff);

      yAlarm -= dyAlarm;
      yAlarmIcon -= dyAlarm;
   }
   }

   bool bShowTelemetryAlarm = false;
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE )
   if ( pairing_isReceiving() || pairing_wasReceiving() )
   if ( ! link_has_fc_telemetry() )
   if ( ! pairing_is_connected_to_wrong_model() )
      bShowTelemetryAlarm = true;

   if ( s_bDebugOSDShowAll )
      bShowTelemetryAlarm = true;

   if ( bShowTelemetryAlarm )
   if ( ( g_TimeNow / 500 ) % 2 )
   {
      g_pRenderEngine->setColors(get_Color_IconWarning());
      g_pRenderEngine->drawIcon(xAlarmIcon, yAlarmIcon, 1.2*height_text/g_pRenderEngine->getAspectRatio(), 1.2*height_text, g_idIconCPU);
      osd_set_colors();
      g_pRenderEngine->drawText(xAlarm, yAlarm, height_text*fTextScale, g_idFontOSDWarnings, "No telemetry from flight controller");
      yAlarm -= dyAlarm;
      yAlarmIcon -= dyAlarm;
   }

   if ( NULL != g_pCurrentModel )
   if ( g_bRCFailsafe || s_bDebugOSDShowAll )
   if ( ( g_TimeNow / 500 ) % 2 )
   {
      g_pRenderEngine->setColors(get_Color_IconError());
      g_pRenderEngine->drawIcon(xAlarmIcon, yAlarmIcon, 1.2*height_text/g_pRenderEngine->getAspectRatio(), 1.2*height_text, g_idIconJoystick);
      osd_set_colors();
      g_pRenderEngine->drawText(xAlarm, yAlarm, height_text*fTextScale, g_idFontOSDBig, "! RC FAILSAFE !");
      yAlarm -= dyAlarm;
      yAlarmIcon -= dyAlarm;
   }


   // Battery voltage alarm is on the middle of the screen
   
   g_bOSDWarningBatteryVoltage = false;

   if ( 0 == s_computedOSDCellCount )
      s_computedOSDCellCount = 1;

   if ( NULL != g_pCurrentModel )
   if ( NULL != g_pdpfct )
   if ( g_pCurrentModel->osd_params.voltage_alarm_enabled )
   if ( g_pdpfct->voltage > 1000 )
   if ( g_pdpfct->voltage/1000.0/(float)s_computedOSDCellCount < g_pCurrentModel->osd_params.voltage_alarm )
   if ( ! pairing_is_connected_to_wrong_model() )
      g_bOSDWarningBatteryVoltage = true;

   if ( s_bDebugOSDShowAll )
      g_bOSDWarningBatteryVoltage = true;

   if ( g_bOSDWarningBatteryVoltage )
   if ( ( g_TimeNow / 300 ) % 3 )
   {
      char szBuff[64];
      g_pRenderEngine->setColors(get_Color_IconError());
      float sizeIcon = height_text_big*1.2;
      yAlarm = 1.0 - 0.05*osd_getScaleOSD()-osd_getMarginY() - 2.0*osd_getBarHeight() - height_text_big;
      xAlarm = 0.22+osd_getMarginX() + sizeIcon*1.6;
      
      g_pRenderEngine->drawIcon(xAlarm-sizeIcon/g_pRenderEngine->getAspectRatio(), yAlarm - height_text_big*0.1, sizeIcon/g_pRenderEngine->getAspectRatio(), sizeIcon, g_idIconWarning);
      osd_set_colors();
      sprintf(szBuff, "Vehicle voltage is low (%.1f V) !", g_pdpfct->voltage/1000.0 );
      g_pRenderEngine->drawText(xAlarm+0.01, yAlarm, height_text, g_idFontOSDBig, szBuff);
   }
   g_pRenderEngine->setGlobalAlfa(fAlfaOrg);
}
