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
#include "../base/shared_mem.h"
#include "../base/config_video.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"

#include <ctype.h>
#include "shared_vars.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "osd_common.h"
#include "osd.h"
#include "osd_stats_dev.h"
#include "local_stats.h"
#include "launchers_controller.h"
#include "pairing.h"
#include "timers.h"

float s_fOSDStatsGraphLinesAlpha = 0.9;
float s_OSDStatsLineSpacing = 1.0;
float s_fOSDStatsMargin = 0.016;
static bool s_bDebugStatsShowAll = false;

static int s_iPeakTotalPacketsInBuffer = 0;
static int s_iPeakCountRender = 0;

static u32 s_uStatsRenderRCCount = 0;
static u32 s_TimeLastTXLoadTooBig = 0;

u32 s_idFontStats = 0;
u32 s_idFontStatsSmall = 0;

#define SNAPSHOT_HISTORY_SIZE 3

static u32 s_uOSDSnapshotTakeTime = 0;
static u32 s_uOSDSnapshotCount = 0;
static u32 s_uOSDSnapshotLastDiscardedBuffers = 0;
static u32 s_uOSDSnapshotLastDiscardedSegments = 0;
static u32 s_uOSDSnapshotLastDiscardedPackets = 0;
static t_packet_header_ruby_telemetry_extended_extra_info_retransmissions s_Snapshot_PHRTE_Retransmissions;

static u32 s_uOSDSnapshotHistoryDiscardedBuffers[SNAPSHOT_HISTORY_SIZE];
static u32 s_uOSDSnapshotHistoryDiscardedSegments[SNAPSHOT_HISTORY_SIZE];
static u32 s_uOSDSnapshotHistoryDiscardedPackets[SNAPSHOT_HISTORY_SIZE];

static shared_mem_radio_stats s_OSDSnapshot_RadioStats;
static shared_mem_video_decode_stats s_OSDSnapshot_VideoDecodeStats;
static shared_mem_video_decode_stats_history s_OSDSnapshot_VideoDecodeHist;
static shared_mem_controller_retransmissions_stats s_OSDSnapshot_ControllerVideoRetransmissionsStats;

static int   s_iCountOSDStatsBoundingBoxes;
static float s_iOSDStatsBoundingBoxesX[50];
static float s_iOSDStatsBoundingBoxesY[50];
static float s_iOSDStatsBoundingBoxesW[50];
static float s_iOSDStatsBoundingBoxesH[50];
static int   s_iOSDStatsBoundingBoxesIds[50];
static int   s_iOSDStatsBoundingBoxesColumns[50];
static float s_fOSDStatsColumnsHeights[10];
static float s_fOSDStatsColumnsWidths[10];

static float s_fOSDStatsSpacingH = 0.0;
static float s_fOSDStatsSpacingV = 0.0;
static float s_fOSDStatsMarginH = 0.0;
static float s_fOSDStatsMarginV = 0.0;

static float s_fOSDStatsWindowsMinimBoxHeight = 0.0;
static float s_fOSDVideoDecodeWidthZoom = 1.0;


static u32 s_uOSDMaxFrameDeviationCamera = 0;
static u32 s_uOSDMaxFrameDeviationTx = 0;
static u32 s_uOSDMaxFrameDeviationRx = 0;
static u32 s_uOSDMaxFrameDeviationPlayer = 0;

void _osd_stats_draw_line(float xLeft, float xRight, float y, u32 uFontId, const char* szTextLeft, const char* szTextRight)
{
   g_pRenderEngine->drawText(xLeft, y, osd_getFontHeight(), uFontId, szTextLeft);
   g_pRenderEngine->drawTextLeft(xRight, y, osd_getFontHeight(), uFontId, szTextRight);
}

void osd_stats_init()
{
   s_uStatsRenderRCCount = 0;
   s_uOSDMaxFrameDeviationCamera = 0;
   s_uOSDMaxFrameDeviationTx = 0;
   s_uOSDMaxFrameDeviationRx = 0;
   s_uOSDMaxFrameDeviationPlayer = 0;
}

void osd_render_stats_full_rx_port()
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float lineHeight = height_text*s_OSDStatsLineSpacing*1.4;
   u32 fontId = s_idFontStatsSmall;

   float padding = 0.01 * g_pRenderEngine->getAspectRatio();
   float widthCol = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA");
   
   float width = 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   if ( NULL != g_pSM_RadioStats )
      width += widthCol * g_pSM_RadioStats->countRadioInterfaces;
   else
      width += 0.3;

   width += widthCol + 1.2*padding;

   float height = 0.12 + 0.34*osd_getScaleOSDStats();

   if ( width > 0.9 ) width = 0.9;
   if ( height > 0.9 ) height = 0.9;

   float xPos = 0.04;
   float yPos = 0.16;
   char szBuff[128];
   char szBuff2[64];

   float fAlpha = g_pRenderEngine->setGlobalAlfa(0.9);

   float alfa = 0.96;
   
   double pc[4];
   pc[0] = get_Color_OSDBackground()[0];
   pc[1] = get_Color_OSDBackground()[1];
   pc[2] = get_Color_OSDBackground()[2];
   pc[3] = 0.96;
   g_pRenderEngine->setColors(pc, alfa);

   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += 2*s_fOSDStatsMargin*0.7;
   width -= 4*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();

   float widthMax = width;
   float rightMargin = xPos + width;

   float y = yPos;

   osd_set_colors();

   sprintf(szBuff, "Full Radio Interfaces RX Stats (update rate: %d ms)", g_pSM_RadioStats->refreshIntervalMs);
   g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);
   y += lineHeight;

   float y0 = y;

   if ( NULL == g_pSM_RadioStats )
   {
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Can't access radio RX stats.");
      g_pRenderEngine->setGlobalAlfa(fAlpha);
      return;
   }
 
   g_pRenderEngine->setStrokeSize(1.0);
   lineHeight = height_text*s_OSDStatsLineSpacing*1.3;

   for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      y = y0;
      if ( NULL == pNICInfo )
      {    
         g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Can't access radio interface");
         xPos += 0.1;
         continue;
      }
      int iRadioLinkId = g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId;

      char* szName = controllerGetCardUserDefinedName(pNICInfo->szMAC);
      if ( NULL != szName && 0 != szName[0] )
         sprintf(szBuff, "%s%s (%s): %s", pNICInfo->szUSBPort, (controllerIsCardInternal(pNICInfo->szMAC)?"":"(E)"), szName, pNICInfo->szDriver);
      else
         sprintf(szBuff, "%s%s: %s", pNICInfo->szUSBPort, (controllerIsCardInternal(pNICInfo->szMAC)?"":"(E)"), pNICInfo->szDriver);

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);
      y += lineHeight;

      sprintf(szBuff2, "%.1f", ((float)(g_pSM_RadioStats->radio_interfaces[i].lastDataRate))/2.0);

      sprintf(szBuff, "Mode: N/A %s Mbs", removeTrailingZero(szBuff2));
      if ( g_pSM_RadioStats->radio_interfaces[i].openedForWrite && g_pSM_RadioStats->radio_interfaces[i].openedForRead )
         sprintf(szBuff, "Mode: TX/RX %s Mbs", removeTrailingZero(szBuff2)); 
      else if ( g_pSM_RadioStats->radio_interfaces[i].openedForWrite )
         sprintf(szBuff, "Mode: TX %s Mbs", removeTrailingZero(szBuff2)); 
      else if (g_pSM_RadioStats->radio_interfaces[i].openedForRead )
         sprintf(szBuff, "Mode: RX %s Mbs", removeTrailingZero(szBuff2));

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);
      y += lineHeight + 0.01;

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Rx Quality:");
      sprintf(szBuff, "%d%%  %d dbm", g_pSM_RadioStats->radio_interfaces[i].rxQuality, g_pSM_RadioStats->radio_interfaces[i].lastDbm);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Rx Relative Quality:");
      sprintf(szBuff, "%d", g_pSM_RadioStats->radio_interfaces[i].rxRelativeQuality);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      y += lineHeight;

      if ( iRadioLinkId >= 0 )
      {
         sprintf(szBuff, "TX Time/Sec (link %d):", iRadioLinkId+1);
         g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);
         // TO DO : to fix: show tx time per radio link
         //sprintf(szBuff, "%d ms", g_pSM_RadioStats->radio_links[iRadioLinkId].downlink_tx_time_per_sec);
         if ( NULL != g_pPHRTE )
            sprintf(szBuff, "%d ms/sec", g_pPHRTE->txTimePerSec);
         else
            strcpy(szBuff, "N/A ms/sec");
         g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      }
      else
      {
         g_pRenderEngine->drawText(xPos, y, height_text, fontId, "TX Time/Sec:");
         g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, "No Link Assigned");
      }
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "RX Total:");
      sprintf(szBuff, "%d kbps", g_pSM_RadioStats->radio_interfaces[i].rxBytesPerSec * 8 / 1000 );
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "RX Packets/Sec:");
      sprintf(szBuff, "%d", g_pSM_RadioStats->radio_interfaces[i].rxPacketsPerSec);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawLine(xPos + padding, y - 0.003, xPos + widthCol - 2.0*padding, y - 0.003 );

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "RX Recv Packets:");
      sprintf(szBuff, "%d", g_pSM_RadioStats->radio_interfaces[i].totalRxPackets);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "RX OK Packets:");
      sprintf(szBuff, "%d", g_pSM_RadioStats->radio_interfaces[i].totalRxPackets - g_pSM_RadioStats->radio_interfaces[i].totalRxPacketsBad);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "RX Lost Packets:");
      sprintf(szBuff, "%d", g_pSM_RadioStats->radio_interfaces[i].totalRxPacketsLost);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "RX Broken Packets:");
      sprintf(szBuff, "%d", g_pSM_RadioStats->radio_interfaces[i].totalRxPacketsBad);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawLine(xPos + padding, y - 0.003, xPos + widthCol - 2.0*padding, y - 0.003 );

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "TX Packets/Sec:");
      sprintf(szBuff, "%d", g_pSM_RadioStats->radio_interfaces[i].txPacketsPerSec);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      y += lineHeight;

      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "TX Total Packets:");
      sprintf(szBuff, "%d", g_pSM_RadioStats->radio_interfaces[i].totalTxPackets);
      g_pRenderEngine->drawTextLeft(xPos + widthCol - 2.0*padding, y, height_text, fontId, szBuff);
      y += lineHeight;

      xPos += widthCol;
      //if ( i<g_pSM_RadioStats->countRadioInterfaces-1 )
         g_pRenderEngine->drawLine(xPos - padding, y0, xPos - padding, y);
   }

   y = y0;
   xPos += widthCol*0.04;
   float xEnd = xPos + widthCol - 0.1*padding;
   lineHeight = height_text*s_OSDStatsLineSpacing*1.2;

   g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Streams Statistics:");
   y += 2.0 * lineHeight;

   for( int i=0; i<MAX_RADIO_STREAMS; i++ )
   {
      if ( g_pSM_RadioStats->radio_streams[i].timeLastRxPacket == 0 && g_pSM_RadioStats->radio_streams[i].timeLastTxPacket == 0 )
         continue;

      sprintf(szBuff, "%s Rx Total:", str_get_radio_stream_name(i));
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);

      if ( g_pSM_RadioStats->radio_streams[i].totalRxBytes >= 1000000 )
         sprintf(szBuff, "%u MB", g_pSM_RadioStats->radio_streams[i].totalRxBytes/1000/1000);
      else if ( g_pSM_RadioStats->radio_streams[i].totalRxBytes >= 1000 )
         sprintf(szBuff, "%u kB", g_pSM_RadioStats->radio_streams[i].totalRxBytes/1000);
      else
         sprintf(szBuff, "%u bytes", g_pSM_RadioStats->radio_streams[i].totalRxBytes);
      g_pRenderEngine->drawTextLeft(xEnd, y, height_text, fontId, szBuff);
      y += lineHeight;

      sprintf(szBuff, "%s Rx:", str_get_radio_stream_name(i));
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);
      sprintf(szBuff, "%u kbps", g_pSM_RadioStats->radio_streams[i].rxBytesPerSec * 8 / 1000);
      g_pRenderEngine->drawTextLeft(xEnd, y, height_text, fontId, szBuff);
      y += lineHeight;

      sprintf(szBuff, "%s Rx Packets:", str_get_radio_stream_name(i));
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);
      sprintf(szBuff, "%u", g_pSM_RadioStats->radio_streams[i].totalRxPackets);
      g_pRenderEngine->drawTextLeft(xEnd, y, height_text, fontId, szBuff);
      y += lineHeight;


      sprintf(szBuff, "%s Rx Lost Packets:", str_get_radio_stream_name(i));
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);
      sprintf(szBuff, "%u", g_pSM_RadioStats->radio_streams[i].totalLostPackets);
      g_pRenderEngine->drawTextLeft(xEnd, y, height_text, fontId, szBuff);
      y += lineHeight;

      sprintf(szBuff, "%s Rx Lost Percent:", str_get_radio_stream_name(i));
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);
      sprintf(szBuff, "%.1f%%", (float)g_pSM_RadioStats->radio_streams[i].totalLostPackets*100.0/(float)g_pSM_RadioStats->radio_streams[i].totalRxPackets);
      g_pRenderEngine->drawTextLeft(xEnd, y, height_text, fontId, szBuff);
      y += lineHeight;

      y += 0.7 * lineHeight;
   }

   g_pRenderEngine->setGlobalAlfa(fAlpha);
}


float osd_render_stats_radio_interfaces_get_height(shared_mem_radio_stats* pStats, float scale)
{
   if ( NULL == pStats )
      return 0.0;

   ControllerSettings* pCS = get_ControllerSettings();

   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;
   float height_text_small = osd_getFontHeightSmall();
   float hGraph = height_text * 1.8;
   float height = 2.0 *s_fOSDStatsMargin*scale*1.0;

   if ( ! (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_RADIO_INTERFACES_COMPACT) )
      height += height_text*s_OSDStatsLineSpacing + s_fOSDStatsMargin*scale*0.3;
   
   height += ( height_text*s_OSDStatsLineSpacing*2.0 + hGraph + height_text*1.0) * pStats->countRadioInterfaces;


   if ( g_pCurrentModel )
   if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_VEHICLE_RADIO_INTERFACES_STATS )
   {
      if ( ! (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_RADIO_INTERFACES_COMPACT) )
         height += 2.0*(height_text*s_OSDStatsLineSpacing + height_text*0.4);
      height += ( height_text*s_OSDStatsLineSpacing*2.0 + hGraph ) * g_pCurrentModel->radioInterfacesParams.interfaces_count;
      height += height_text*1.0 * (g_pCurrentModel->radioInterfacesParams.interfaces_count-1);
   } 
   return height;
}

float osd_render_stats_radio_interfaces_get_width(shared_mem_radio_stats* pStats, float scale)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;

   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAA AAAAA AAAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

float osd_render_stats_radio_interfaces( float xPos, float yPos, const char* szTitle, shared_mem_radio_stats* pStats, float scale)
{
   if ( NULL == pStats || NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("Pointer to %s stats shared mem is NULL. Can't render %s stats.", szTitle, szTitle);
      return 0.0;
   }

   ControllerSettings* pCS = get_ControllerSettings();
   float height_text = g_pRenderEngine->textHeight(s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight(s_idFontStatsSmall);

   float width = osd_render_stats_radio_interfaces_get_width(pStats, scale);
   
   float height = osd_render_stats_radio_interfaces_get_height(pStats, scale);
   float hPixel = 1.0/g_pRenderEngine->getScreenHeight();

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;

   width -= 2*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float rightMargin = xPos + width;

   if ( ! (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_RADIO_INTERFACES_COMPACT) )
   {
      sprintf(szBuff, "%s", szTitle); 
      g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, szBuff);
   }
   osd_set_colors();

   float marginH = 0.0;//0.04*scale/g_pRenderEngine->getAspectRatio();
   float maxWidth = width - marginH - 0.001*scale;

   float y = yPos;

   if ( ! (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_RADIO_INTERFACES_COMPACT) )
   {
      sprintf(szBuff,"%d ms/bar", pStats->graphRefreshIntervalMs);
      g_pRenderEngine->drawTextLeft(rightMargin, yPos, height_text_small, s_idFontStatsSmall, szBuff);

      float w = g_pRenderEngine->textWidth(height_text_small, s_idFontStatsSmall, szBuff);
      sprintf(szBuff,"%.1f seconds, ", (((float)MAX_HISTORY_RADIO_STATS_RECV_SLICES) * pStats->graphRefreshIntervalMs)/1000.0);
      g_pRenderEngine->drawTextLeft(rightMargin-w, yPos, height_text_small, s_idFontStatsSmall, szBuff);
      w += g_pRenderEngine->textWidth( height_text_small, s_idFontStatsSmall, szBuff);
      y += height_text*1.0*s_OSDStatsLineSpacing + 2.0*s_fOSDStatsMargin*0.3;
   }


   if ( g_pCurrentModel )
   if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_VEHICLE_RADIO_INTERFACES_STATS )
   if ( ! (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_RADIO_INTERFACES_COMPACT) )
   {
      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Downlink (Controller) Rx Stats:");
      y += height_text*s_OSDStatsLineSpacing + height_text*0.4;
   }
   float hGraph = height_text * 1.8;
   float hBar = 0.0;
   float hBarLost = 0.0;
   float dxBarWidth = maxWidth/(float)MAX_HISTORY_RADIO_STATS_RECV_SLICES;
   float fStroke = OSD_STRIKE_WIDTH;

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);

   char szName[128];
   for ( int i=0; i<pStats->countRadioInterfaces; i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo )
         continue;
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
      int iRadioLinkId = pStats->radio_interfaces[i].assignedRadioLinkId;
      bool bCardUsed = true;
      bool bIsTxCard = false;
      float ySt = y;

      int iCountInterfacesForCurrentLink = 0;
      if ( iRadioLinkId >= 0 )
      {
         for( int k=0; k<pStats->countRadioInterfaces; k++ )
            if ( pStats->radio_interfaces[k].assignedRadioLinkId == iRadioLinkId )
               iCountInterfacesForCurrentLink++;
      }

      if ( iRadioLinkId >= 0 && pStats->radio_links[iRadioLinkId].lastTxInterfaceIndex == i && 1 < pStats->countRadioInterfaces )
         bIsTxCard = true;
         
      osd_set_colors();

      double* pc = get_Color_OSDText();
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setStrokeSize(fStroke);
      g_pRenderEngine->drawLine(xPos+marginH,y+hGraph + g_pRenderEngine->getPixelHeight(), xPos+marginH + maxWidth, y+hGraph + g_pRenderEngine->getPixelHeight());
      float maxValue = 0;
      u32 maxBadLost = 0;
      u32 firstLostValue = 0;
      u8 uMaxGapMs = 0;
      u32 uGapSum = 0;

      for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
      {
          if ( pStats->radio_interfaces[i].hist_rxPackets[k] + pStats->radio_interfaces[i].hist_rxPacketsBad[k] > maxValue )
             maxValue = pStats->radio_interfaces[i].hist_rxPackets[k] + pStats->radio_interfaces[i].hist_rxPacketsBad[k];

          if ( pStats->radio_interfaces[i].hist_rxPacketsBad[k] + pStats->radio_interfaces[i].hist_rxPacketsLost[k] > maxBadLost )
             maxBadLost = pStats->radio_interfaces[i].hist_rxPacketsBad[k] + pStats->radio_interfaces[i].hist_rxPacketsLost[k];

          if ( 0 == firstLostValue )
          if ( pStats->radio_interfaces[i].hist_rxPacketsBad[k] + pStats->radio_interfaces[i].hist_rxPacketsLost[k] > 0 )
             firstLostValue = pStats->radio_interfaces[i].hist_rxPacketsBad[k] + pStats->radio_interfaces[i].hist_rxPacketsLost[k];

          if ( pStats->radio_interfaces[i].hist_rxGapMiliseconds[k] != 0xFF )
          {
             uGapSum += pStats->radio_interfaces[i].hist_rxGapMiliseconds[k];
             if ( pStats->radio_interfaces[i].hist_rxGapMiliseconds[k] > uMaxGapMs )
                uMaxGapMs = pStats->radio_interfaces[i].hist_rxGapMiliseconds[k];
          }
      }
      u32 uGapAverage = (uGapSum - uMaxGapMs) / (MAX_HISTORY_RADIO_STATS_RECV_SLICES-1);

      float xBar = xPos + marginH + maxWidth - dxBarWidth;

      if ( maxValue >= 0.1 )
      for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
      {
         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
         
         if ( (pStats->radio_interfaces[i].hist_rxPacketsLost[k] + pStats->radio_interfaces[i].hist_rxPacketsBad[k]) != 0 )
         {
            hBarLost = hGraph * (float)(pStats->radio_interfaces[i].hist_rxPacketsLost[k] + pStats->radio_interfaces[i].hist_rxPacketsBad[k]) / maxValue;
            if ( hBarLost > hGraph )
               hBarLost = hGraph;
            if ( hBarLost < 0.1 * hGraph )
               hBarLost = 0.1 * hGraph;
            hBarLost = ((int)(hBarLost/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
            g_pRenderEngine->setStroke(255,0,50,0.9);
            g_pRenderEngine->setFill(255,0,50,0.9);
            g_pRenderEngine->setStrokeSize(fStroke);
            g_pRenderEngine->drawRect(xBar, y+hGraph - hBarLost, dxBarWidth - g_pRenderEngine->getPixelWidth(), hBarLost);
         }
         else
            hBarLost = 0.0;

         hBar = hGraph * (float)(pStats->radio_interfaces[i].hist_rxPackets[k]) / maxValue;
         if ( hBar + hBarLost > hGraph )
            hBar = hGraph - hBarLost;

         hBar = ((int)(hBar/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->drawRect(xBar, y+hGraph - hBarLost - hBar, dxBarWidth - g_pRenderEngine->getPixelWidth(), hBar);
         xBar -= dxBarWidth;
      }

      osd_set_colors();

      if ( bIsTxCard && (iCountInterfacesForCurrentLink>1) )
         g_pRenderEngine->drawIcon(xPos, y-height_text*0.2, hGraph*0.7/g_pRenderEngine->getAspectRatio() , hGraph*0.7, g_idIconUplink2);

      y += hGraph;

      if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
      {
         g_pRenderEngine->setColors(get_Color_Dev());
         sprintf(szBuff, "Avg/Max Gap: %d/%d ms, %d pkts", (int) uGapAverage, (int)uMaxGapMs, maxBadLost);
         g_pRenderEngine->drawBackgroundBoundingBoxes(true);
         g_pRenderEngine->drawText(xPos, y - height_text*0.8, height_text, s_idFontStats, szBuff);
         g_pRenderEngine->drawBackgroundBoundingBoxes(false);
         osd_set_colors();
      }
      else
      {
         sprintf(szBuff, "MaxGap: %d ms, %d pkts", (int)uMaxGapMs, maxBadLost);
         g_pRenderEngine->drawBackgroundBoundingBoxes(true);
         g_pRenderEngine->drawText(xPos, y - height_text*0.8, height_text, s_idFontStats, szBuff);
         g_pRenderEngine->drawBackgroundBoundingBoxes(false);       
      }
      y += height_text*0.2;

      char szBuffD[64];
      char szBuffU[64];
      str_format_bitrate(pStats->radio_interfaces[i].rxBytesPerSec * 8, szBuffD);
      str_format_bitrate(pStats->radio_interfaces[i].txBytesPerSec * 8, szBuffU);
      sprintf(szBuff, "%s / %s", szBuffU, szBuffD);
      if ( pStats->radio_interfaces[i].txBytesPerSec == 0 )
         sprintf(szBuff, "- / %s", szBuffD);
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
      float fWT = g_pRenderEngine->textWidth(0.0, s_idFontStats, szBuff);
      if ( bIsTxCard )
      {
         float fIconH = height_text*1.14;
         float fIconW = 0.6 * fIconH / g_pRenderEngine->getAspectRatio();
         g_pRenderEngine->drawIcon(rightMargin - fWT - fIconW - height_text_small*0.2, y-height_text*0.1, fIconW, fIconH, g_idIconArrowUp);
      }

      
      szName[0] = 0;
      char* szN = controllerGetCardUserDefinedName(pNICInfo->szMAC);
      if ( NULL != szN && 0 != szN[0] )
         strcpy(szName, szN);
      else if ( NULL == pCardInfo )
         strcpy(szName, "NO NAME");
      else if ( pCardInfo->cardModel != 0 )
      {
         const char* szCardModel = str_get_radio_card_model_string(pCardInfo->cardModel);
         if ( NULL != szCardModel && 0 != szCardModel[0] )
            strcpy(szName, szCardModel);
         else
            strcpy(szName, "NO NAME");
      }

      strcpy(szBuff,szName);

      sprintf(szName, "%s%s", pNICInfo->szUSBPort, (controllerIsCardInternal(pNICInfo->szMAC)?"":"(Ext)"));
      g_pRenderEngine->drawText(xPos, y, s_idFontStats, szName);
      float fW = g_pRenderEngine->textWidth(s_idFontStats, szName);
      if ( 0 != szBuff[0] )
      {
         sprintf(szName, "(%s)", szBuff);
         g_pRenderEngine->drawText(xPos + fW + height_text_small*0.2, y + 0.5*(height_text-height_text_small), s_idFontStatsSmall, szName);
         fW += height_text_small*0.2 + g_pRenderEngine->textWidth(s_idFontStatsSmall, szName);
      }    
      
      y += height_text*s_OSDStatsLineSpacing;

      szName[0] = 0;
      if ( pStats->radio_interfaces[i].openedForWrite && pStats->radio_interfaces[i].openedForRead )
      {
         if ( bIsTxCard )
            strcpy(szName, "RX/TX");
         else
            strcpy(szName,"RX");
      }   
      else if ( pStats->radio_interfaces[i].openedForWrite )
         strcpy(szName,"TX Only");
      else if ( pStats->radio_interfaces[i].openedForRead )
         strcpy(szName,"RX Only");
      else
         strcpy(szName,"NOT USED");

      sprintf(szBuff, "Link %d, %s, %d Mhz, %d dbm", iRadioLinkId+1, szName, pStats->radio_interfaces[i].currentFrequency, (int)g_fOSDDbm[i] );
      if ( controllerIsCardDisabled(pNICInfo->szMAC) )
         sprintf(szBuff, "DISABLED");
      else if ( iRadioLinkId < 0 || iRadioLinkId >= MAX_RADIO_INTERFACES )
         sprintf(szBuff, "No Radio Link");
      if ( pNICInfo->lastFrequencySetFailed || 0 == pStats->radio_interfaces[i].currentFrequency )
         sprintf(szBuff, "Set Freq Failed");

      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, szBuff);

      y += height_text*s_OSDStatsLineSpacing;

      if ( iRadioLinkId >= 0 && pStats->radio_links[iRadioLinkId].lastTxInterfaceIndex == i && 1 < pStats->countRadioInterfaces )
      {
         float fmarginy = 0.002;
         float fmarginx = 0.008/g_pRenderEngine->getAspectRatio();
         double* pC = get_Color_OSDText();
         g_pRenderEngine->setFill(0,0,0,0.2);
         g_pRenderEngine->setStroke(pC[0], pC[1], pC[2], 0.0);

         g_pRenderEngine->drawRoundRect(xPos-fmarginx, ySt-1.5*fmarginy, rightMargin-xPos+2.0*fmarginx, y-ySt+3.0*fmarginy + height_text*0.2, 0.05);
         osd_set_colors();
      }

      y += height_text*0.8;
   }

   // Vehicle side

   if ( g_pCurrentModel )
   if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_VEHICLE_RADIO_INTERFACES_STATS )
   {
      if ( ! (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_RADIO_INTERFACES_COMPACT) )
      {
         y -= height_text*0.2;
         g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Uplink (Vehicle) Rx Stats:");
         y += height_text*s_OSDStatsLineSpacing + height_text*0.6;
      }
      if ( ! g_bGotStatsVehicleRxCards )
      {
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, "No Data.");
      }
      else
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         osd_set_colors();

         double* pc = get_Color_OSDText();
         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
         g_pRenderEngine->drawLine(xPos+marginH,y+hGraph + g_pRenderEngine->getPixelHeight(), xPos+marginH + maxWidth, y+hGraph + g_pRenderEngine->getPixelHeight());
         float maxValue = 0;
         u32 maxBadLost = 0;
         u32 firstLostValue = 0;
         u8 uMaxGapMs = 0;
         u32 uGapSum = 0;

         for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
         {
             if ( g_SM_VehicleRxStats[i].hist_rxPackets[k] + g_SM_VehicleRxStats[i].hist_rxPacketsBad[k] > maxValue )
                maxValue = g_SM_VehicleRxStats[i].hist_rxPackets[k] + g_SM_VehicleRxStats[i].hist_rxPacketsBad[k];

             //if ( g_SM_VehicleRxStats[i].hist_rxPackets[k] + g_SM_VehicleRxStats[i].hist_rxPacketsLost[k] > maxValue )
             //   maxValue = g_SM_VehicleRxStats[i].hist_rxPackets[k] + g_SM_VehicleRxStats[i].hist_rxPacketsLost[k];

             if ( g_SM_VehicleRxStats[i].hist_rxPacketsBad[k] + g_SM_VehicleRxStats[i].hist_rxPacketsLost[k] > maxBadLost )
                maxBadLost = g_SM_VehicleRxStats[i].hist_rxPacketsBad[k] + g_SM_VehicleRxStats[i].hist_rxPacketsLost[k];

             if ( 0 == firstLostValue )
             if ( g_SM_VehicleRxStats[i].hist_rxPacketsBad[k] + g_SM_VehicleRxStats[i].hist_rxPacketsLost[k] > 0 )
                firstLostValue = g_SM_VehicleRxStats[i].hist_rxPacketsBad[k] + g_SM_VehicleRxStats[i].hist_rxPacketsLost[k];

             if ( g_SM_VehicleRxStats[i].hist_rxGapMiliseconds[k] != 0xFF )
             {
                uGapSum += g_SM_VehicleRxStats[i].hist_rxGapMiliseconds[k];
                if ( g_SM_VehicleRxStats[i].hist_rxGapMiliseconds[k] > uMaxGapMs )
                   uMaxGapMs = g_SM_VehicleRxStats[i].hist_rxGapMiliseconds[k];
             }
         }
         u32 uGapAverage = (uGapSum - uMaxGapMs) / (MAX_HISTORY_RADIO_STATS_RECV_SLICES-1);

         float xBar = xPos + marginH + maxWidth - dxBarWidth;

         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setStrokeSize(fStroke);
            
         if ( maxValue >= 0.1 )
         for( int k=0; k<MAX_HISTORY_RADIO_STATS_RECV_SLICES; k++ )
         {
            hBar = hGraph * (float)(g_SM_VehicleRxStats[i].hist_rxPackets[k]) / maxValue;
            hBarLost = hGraph * (float)(g_SM_VehicleRxStats[i].hist_rxPacketsLost[k] + g_SM_VehicleRxStats[i].hist_rxPacketsBad[k]) / maxValue;
            
            if ( k < MAX_HISTORY_RADIO_STATS_RECV_SLICES-1 )
            {
               if ( g_SM_VehicleRxStats[i].hist_rxPacketsLost[k+1] == 0 )
               if ( g_SM_VehicleRxStats[i].hist_rxPackets[k] > 0 )
               if ( g_SM_VehicleRxStats[i].hist_rxPacketsLost[k] > 0 )
                  hBarLost = 0.0;
            }
            if ( k > 0 )
            {
               if ( g_SM_VehicleRxStats[i].hist_rxPacketsLost[k] == 0 )
               if ( g_SM_VehicleRxStats[i].hist_rxPackets[k-1] > 0 )
               if ( g_SM_VehicleRxStats[i].hist_rxPacketsLost[k-1] > 0 )
                  hBarLost = hGraph * (float)(g_SM_VehicleRxStats[i].hist_rxPacketsLost[k-1] + g_SM_VehicleRxStats[i].hist_rxPacketsBad[k-1]) / maxValue;
            }
            

            if ( hBar + hBarLost > hGraph )
            {
               hBar *= hGraph / (hBar + hBarLost);
               hBarLost *= hGraph / (hBar + hBarLost);
            }
 
            if ( (g_SM_VehicleRxStats[i].hist_rxPacketsLost[k] + g_SM_VehicleRxStats[i].hist_rxPacketsBad[k]) != 0 ||
                 hBarLost > 0.001 )
            {
               hBarLost = ((int)(hBarLost/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
               g_pRenderEngine->setStroke(255,0,50,0.9);
               g_pRenderEngine->setFill(255,0,50,0.9);
               g_pRenderEngine->setStrokeSize(fStroke);
               g_pRenderEngine->drawRect(xBar, y+hGraph - hBarLost, dxBarWidth - g_pRenderEngine->getPixelWidth(), hBarLost);
            
               g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
               g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
               g_pRenderEngine->setStrokeSize(fStroke);
            
            }
            
            if ( g_SM_VehicleRxStats[i].hist_rxPackets[k] > 0 ||
                 hBar > 0.001 )
            {
               hBar = ((int)(hBar/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
               g_pRenderEngine->drawRect(xBar, y+hGraph - hBarLost - hBar, dxBarWidth - g_pRenderEngine->getPixelWidth(), hBar);
            }
            xBar -= dxBarWidth;
         }

         osd_set_colors();

         y += hGraph;

         if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
         {
            g_pRenderEngine->setColors(get_Color_Dev());
            sprintf(szBuff, "Avg/Max Gap: %d/%d ms, %d pkts", (int) uGapAverage, (int)uMaxGapMs, maxBadLost);
            g_pRenderEngine->drawBackgroundBoundingBoxes(true);
            g_pRenderEngine->drawText(xPos, y - height_text*0.8, height_text, s_idFontStats, szBuff);
            g_pRenderEngine->drawBackgroundBoundingBoxes(false);
            osd_set_colors();
         }
         else
         {
            sprintf(szBuff, "MaxGap: %d ms, %d pkts", (int)uMaxGapMs, maxBadLost);
            g_pRenderEngine->drawBackgroundBoundingBoxes(true);
            g_pRenderEngine->drawText(xPos, y - height_text*0.8, height_text, s_idFontStats, szBuff);
            g_pRenderEngine->drawBackgroundBoundingBoxes(false);       
         }
         y += height_text*0.2;

         //sprintf(szBuff, "Link %d, %s (%s)", i+1, g_pCurrentModel->radioInterfacesParams.interface_szPort[i], str_get_radio_card_model_string_short(g_pCurrentModel->radioInterfacesParams.interface_card_model[i]) );
         sprintf(szBuff, "Link %d, %s", i+1, g_pCurrentModel->radioInterfacesParams.interface_szPort[i]);
         if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            sprintf(szBuff, "DISABLED");
         int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
         if ( iRadioLinkId < 0 || iRadioLinkId >= MAX_RADIO_INTERFACES )
            sprintf(szBuff, "No Radio Link");
         
         g_pRenderEngine->drawText(xPos, y, s_idFontStats, szBuff);
         float fW = g_pRenderEngine->textWidth(s_idFontStats, szBuff);
         sprintf(szBuff, "(%s)", str_get_radio_card_model_string_short(g_pCurrentModel->radioInterfacesParams.interface_card_model[i]));
         g_pRenderEngine->drawText(xPos + fW + height_text_small*0.2, y + 0.5*(height_text-height_text_small), s_idFontStatsSmall, szBuff);
         fW += height_text_small*0.2 + g_pRenderEngine->textWidth(s_idFontStatsSmall, szBuff);  

         y += height_text*s_OSDStatsLineSpacing;

         char szTmp[64];
         getDataRateDescription(g_SM_VehicleRxStats[i].lastDataRate/2, szTmp);
         sprintf(szBuff, "RX Quality: %d%%, Radio rate: %s", g_SM_VehicleRxStats[i].rxQuality, szTmp);
         
         int iRadioLink = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
         if ( iRadioLink >= 0 && iRadioLink < g_pCurrentModel->radioLinksParams.links_count )
         if ( g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][1] < 0 )
            sprintf(szBuff, "RX Quality: %d%%, Radio rate: MCS-%d", g_SM_VehicleRxStats[i].rxQuality, (-g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][1])-1);
         g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, szBuff);
         y += height_text*s_OSDStatsLineSpacing;

         y += height_text*0.8;
      }
   }   
   osd_set_colors();
   return height;
}


float osd_render_stats_radio_links_get_height(shared_mem_radio_stats* pStats, float scale)
{
   if ( NULL == pStats || NULL == g_pCurrentModel )
      return 0.0;

   ControllerSettings* pCS = get_ControllerSettings();

   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall)*scale;
   float height = 2.0 *s_fOSDStatsMargin*scale*1.1 + height_text*s_OSDStatsLineSpacing;

   height += 1.3*height_text*s_OSDStatsLineSpacing;

   height += height_text*s_OSDStatsLineSpacing + 0.6*height_text;

   height += 2 * height_text * s_OSDStatsLineSpacing * g_pCurrentModel->radioLinksParams.links_count;

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      height += 5 * height_text*s_OSDStatsLineSpacing + 0.3*height_text;
      height += height_text_small*s_OSDStatsLineSpacing; // Ping frequency
      height += height_text * s_OSDStatsLineSpacing * g_pCurrentModel->radioLinksParams.links_count;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      int countStreams = 0;
      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         if ( pStats->radio_streams[i].totalRxPackets > 0 )
            countStreams++;

      height += countStreams * height_text * s_OSDStatsLineSpacing;
      height += 0.2 * height_text;

      height += g_pCurrentModel->radioLinksParams.links_count * height_text_small * s_OSDStatsLineSpacing;
   }
   return height;
}

float osd_render_stats_radio_links_get_width(shared_mem_radio_stats* pStats, float scale)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;

   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA A");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

float osd_render_stats_radio_links( float xPos, float yPos, const char* szTitle, shared_mem_radio_stats* pStats, float scale)
{
   if ( NULL == pStats || NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("Pointer to %s stats shared mem is NULL. Can't render %s stats.", szTitle, szTitle);
      return 0.0;
   }

   ControllerSettings* pCS = get_ControllerSettings();
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall)*scale;

   float width = osd_render_stats_radio_links_get_width(pStats, scale);
   
   float height = osd_render_stats_radio_links_get_height(pStats, scale);
   float hPixel = 1.0/g_pRenderEngine->getScreenHeight();

   char szBuff[64];
   char szBuff2[64];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;

   width -= 2*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float rightMargin = xPos + width;

   sprintf(szBuff, "%s", szTitle);
   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->clock_sync_type == CLOCK_SYNC_TYPE_NONE) )
         sprintf(szBuff, "%s (ping freq %d)", szTitle, pCS->nPingClockSyncFrequency/2 );
      else
         sprintf(szBuff, "%s (ping freq %d)", szTitle, pCS->nPingClockSyncFrequency );
   }
   g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, szBuff);
   
   int nQualityRadioLink[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      nQualityRadioLink[i] = 0;

   for( int i=0; i<pStats->countRadioInterfaces; i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      int iRadioLink = pStats->radio_interfaces[i].assignedRadioLinkId;
      if ( iRadioLink >= 0 )
      if ( pStats->radio_interfaces[i].rxQuality > nQualityRadioLink[iRadioLink] )
         nQualityRadioLink[iRadioLink] = pStats->radio_interfaces[i].rxQuality;
   }
   
   double* pc = get_Color_OSDText();
   bool bWarning = false;
   bool bCritical = false;
   for( int i=0; i<pStats->countRadioLinks; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( nQualityRadioLink[i] < OSD_QUALITY_LEVEL_WARNING )
         bWarning = true;
      if ( nQualityRadioLink[i] < OSD_QUALITY_LEVEL_CRITICAL )
         bCritical = true;
   }
   if ( bWarning )
      pc = get_Color_IconWarning();
   if ( bCritical )
      pc = get_Color_IconError();
   g_pRenderEngine->setColors(pc);

   szBuff[0] = 0;
   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      char szQ[16];
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         sprintf(szQ, "N/A");
      else
         sprintf(szQ, "%d%%", nQualityRadioLink[i]);
      if ( i == 0 )
         strcpy(szBuff, szQ);
      else
      {
         strcat(szBuff, ", ");
         strcat(szBuff, szQ);
      }
   }
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, height_text, s_idFontStats, szBuff);
   osd_set_colors();

   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      g_pRenderEngine->setColors(get_Color_Dev());

      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            sprintf(szBuff, "Disabled");
         else if ( 0 != g_pSM_RadioStats->radio_links[i].linkDelayRoundtripMs )
            sprintf(szBuff, "%d ms", g_pSM_RadioStats->radio_links[i].linkDelayRoundtripMs);
         else
            strcpy(szBuff, "N/A");
         sprintf(szBuff2, "Link %d RT delay:", i+1);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff2, szBuff);
         y += height_text*s_OSDStatsLineSpacing;
      }

      if ( g_pSM_RadioStats->uAverageCommandRoundtripMiliseconds == MAX_U32 )
         strcpy(szBuff, "N/A");
      else
         sprintf(szBuff, "%d", g_pSM_RadioStats->uAverageCommandRoundtripMiliseconds);
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Commands RT delay:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      if ( g_pSM_RadioStats->uMinCommandRoundtripMiliseconds == MAX_U32 )
         strcpy(szBuff, "N/A");
      else
         sprintf(szBuff, "%d", g_pSM_RadioStats->uMinCommandRoundtripMiliseconds);
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Commands RT delay (min):", szBuff);
      y += height_text*s_OSDStatsLineSpacing;
   }
   osd_set_colors();

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         strcpy(szBuff, "Disabled");
      else
         str_format_bitrate(g_pSM_RadioStats->radio_links[i].rxBytesPerSec*8, szBuff);
      sprintf(szBuff2, "Downlink %d (%d Mhz):", i+1, g_pCurrentModel->radioLinksParams.link_frequency[i]);
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff2, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
      osd_set_colors();
   }

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         strcpy(szBuff, "Disabled");
      else
         str_format_bitrate(g_pSM_RadioStats->radio_links[i].txBytesPerSec*8, szBuff);
      sprintf(szBuff2, "Uplink %d (%d Mhz):", i+1, g_pCurrentModel->radioLinksParams.link_frequency[i]);
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff2, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
      osd_set_colors();
   }

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
      {
         g_pRenderEngine->setColors(get_Color_Dev());

         sprintf(szBuff2, "Uplink %d packets:", i+1);
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            strcpy(szBuff, "Disabled");
         else
            sprintf(szBuff,"%d pk/sec", g_pSM_RadioStats->radio_links[i].txPacketsPerSec);
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, szBuff2, szBuff);
         y += height_text_small*s_OSDStatsLineSpacing;
      }
      osd_set_colors();
   }


   {
      if ( NULL != g_psmvds )
         sprintf(szBuff, "%d ms/sec", g_psmvds->fec_time/1000);
      else
         sprintf(szBuff, "N/A");

      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Video Encoding Time:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      if ( NULL != g_pPHRTE )
         sprintf(szBuff, "%d ms/sec", g_pPHRTE->txTimePerSec);
      else
         strcpy(szBuff, "N/A ms/sec");

      if ( (NULL != g_pPHRTE) && (g_pPHRTE->txTimePerSec > DEFAULT_TX_TIME_OVERLOAD) )
         s_TimeLastTXLoadTooBig = g_TimeNow;
      if ( g_bHasVideoTxOverloadAlarm && (g_TimeLastVideoTxOverloadAlarm > 0) && (g_TimeNow <  g_TimeLastVideoTxOverloadAlarm + 5000) )
         s_TimeLastTXLoadTooBig = g_TimeNow;
      if ( g_TimeNow < s_TimeLastTXLoadTooBig + 1000 )
      {
         g_pRenderEngine->setColors(get_Color_IconWarning());
         if ( g_bHasVideoTxOverloadAlarm && (g_TimeLastVideoTxOverloadAlarm > 0) && (g_TimeNow <  g_TimeLastVideoTxOverloadAlarm + 5000) )
            g_pRenderEngine->setColors(get_Color_IconError());
      }
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "TX Load:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;
      osd_set_colors();

      szBuff[0] = 0;
      int cst = 0;
      if ( NULL != g_psmvds )
         cst = g_psmvds->iCurrentClockSyncType;
      else
         cst = g_pCurrentModel->clock_sync_type;
      
      if ( cst == CLOCK_SYNC_TYPE_NONE )
         strcat(szBuff, "None");
      if ( cst == CLOCK_SYNC_TYPE_BASIC )
         strcat(szBuff, "Basic");
      if ( cst == CLOCK_SYNC_TYPE_ADV )
         strcat(szBuff, "Advanced");
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Clock Sync:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
      {
         u32 ping_freq_ms = compute_ping_frequency(g_pCurrentModel->uModelFlags, g_pCurrentModel->clock_sync_type, g_psmvds->encoding_extra_flags);
         sprintf(szBuff, "%d ms", ping_freq_ms);
         g_pRenderEngine->setColors(get_Color_Dev());
         _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStatsSmall, "Clock Sync Freq:", szBuff);
         osd_set_colors();
         y += height_text_small*s_OSDStatsLineSpacing;
      }
      y += height_text*0.3;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      double* pColorDev = get_Color_Dev();

      g_pRenderEngine->setColors(pColorDev);
      if ( NULL != g_pPHRTE )
         sprintf(szBuff, "%d/sec", (g_pPHRTE->downlink_tx_video_packets_per_sec+g_pPHRTE->downlink_tx_data_packets_per_sec));
      else
         strcpy(szBuff, "N/A");
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Downlinks Total Packets:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      if ( NULL != g_pPHRTE )
         sprintf(szBuff, "%d/sec", g_pPHRTE->downlink_tx_data_packets_per_sec);
      else
         strcpy(szBuff, "N/A");
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Downlinks Data Packets:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;


      if ( NULL != g_pPHRTE )
         sprintf(szBuff, "%d/sec", g_pPHRTE->downlink_tx_compacted_packets_per_sec);
      else
         strcpy(szBuff, "N/A");
      _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Downlinks Data Packets-C:", szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      for( int i=0; i<MAX_RADIO_STREAMS; i++ )
         if ( pStats->radio_streams[i].totalRxPackets > 0 )
         {
            sprintf(szBuff, "%s:", str_get_radio_stream_name(i));
            str_format_bitrate(pStats->radio_streams[i].rxBytesPerSec*8, szBuff2);
            _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, szBuff, szBuff2);
            y += height_text*s_OSDStatsLineSpacing;
         }
      y += height_text*0.2;
   }

   y += height_text*0.2;
   osd_set_colors();

   return height;
}


float osd_render_stats_video_decode_get_height(int iDeveloperMode, bool bIsSnapshot, shared_mem_radio_stats* pSM_RadioStats, shared_mem_video_decode_stats* pSM_VideoStats, shared_mem_video_decode_stats_history* pSM_VideoHistoryStats, shared_mem_controller_retransmissions_stats* pSM_ControllerRetransmissionsStats, float scale)
{
   ControllerSettings* pCS = get_ControllerSettings();
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = 0.04*scale;
   float hGraphHistory = 2.2*height_text;
   
   float height = 2.0 *s_fOSDStatsMargin*scale*1.0;

   // Compact stats
   if ( (!bIsSnapshot) && (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS) )
   {
      // History graph
      height += height_text_small*1.5;
      height += hGraphHistory;

      // Params
      height += 3.0*height_text*s_OSDStatsLineSpacing;

      // Stats
      height += height_text*s_OSDStatsLineSpacing;

      // Retransmissions
      if ( iDeveloperMode )
         height += 3.0*height_text*s_OSDStatsLineSpacing;

      if ( bIsSnapshot )
         height += 0.8*height_text*s_OSDStatsLineSpacing + 2.0 *s_fOSDStatsMargin*scale*0.3;
      return height;
   }

   height += 1.2*height_text*s_OSDStatsLineSpacing + 2.0 *s_fOSDStatsMargin*scale*0.3;

   // Stream Info
   if ( ! bIsSnapshot )
   {
      if ( (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex]) & OSD_FLAG_EXTENDED_VIDEO_DECODE_HISTORY )
         height += 3.0*height_text_small*s_OSDStatsLineSpacing;
      else
         height += 4.0*height_text_small*s_OSDStatsLineSpacing;
   }

   // Buffers

   if ( ! bIsSnapshot )
   if ( (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex]) & OSD_FLAG_EXTENDED_VIDEO_DECODE_HISTORY )
   {
      float hBar = 0.014*scale;
      height += hBar + height_text;
   }

   // History graph

   height += height_text_small;
   height += hGraphHistory;

   // Max EC packets used per interval

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      height += height_text_small;
      height += hGraph*0.6;
   }

   // Pending good blocks graph

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      height += height_text_small;
      height += hGraph*0.8;
   }

   // History retransm

   height += height_text_small*1.1;
   if ( bIsSnapshot )
      height += hGraph*1.6;
   else
      height += hGraph;
   height += height_text_small*0.3;

   if ( bIsSnapshot )
   {
      // History received retransmission packets and video packets
      height += height_text_small*1.1;
      height += hGraph;

      // History received/ack/completed/dropped retransmissions
      height += height_text_small*1.1;
      height += hGraph;
   }

   // History gap graph

   if ( iDeveloperMode )
   {
      height += height_text_small*1.1;
      height += hGraph*0.6;
      height += height_text_small*0.4;
   }

   if ( ! bIsSnapshot )
   if ( (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex]) & OSD_FLAG_EXT_SHOW_ADAPTIVE_VIDEO_GRAPH )
   {
      height += height_text_small*0.7;
      height += osd_render_stats_adaptive_video_graph_get_height();
   }

   if ( bIsSnapshot )
      height += (0.6 + SNAPSHOT_HISTORY_SIZE)* height_text*s_OSDStatsLineSpacing;
   else
      height += 5.3 * height_text*s_OSDStatsLineSpacing;

   //height += 2.0 * height_text*s_OSDStatsLineSpacing;

   // Recent retransmissions in periods and roundtrip times

   if ( iDeveloperMode )
      height += 5.0 * height_text*s_OSDStatsLineSpacing;

   return height;
}

float osd_render_stats_video_decode_get_width(int iDeveloperMode, bool bIsSnapshot, shared_mem_radio_stats* pSM_RadioStats, shared_mem_video_decode_stats* pSM_VideoStats, shared_mem_video_decode_stats_history* pSM_VideoHistoryStats, shared_mem_controller_retransmissions_stats* pSM_ControllerRetransmissionsStats, float scale)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
   {
      if ( (!bIsSnapshot) && (!(g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS)) )
         return g_fOSDStatsForcePanelWidth*s_fOSDVideoDecodeWidthZoom;
      else
         return g_fOSDStatsForcePanelWidth;
   }
   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA A");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   if ( NULL != g_pCurrentModel &&  ((g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex]) & OSD_FLAG_EXTENDED_VIDEO_DECODE_HISTORY) )
      width *= 1.55;
   return width;
}

float osd_render_stats_video_decode(float xPos, float yPos, int iDeveloperMode, bool bIsSnapshot, shared_mem_radio_stats* pSM_RadioStats, shared_mem_video_decode_stats* pSM_VideoStats, shared_mem_video_decode_stats_history* pSM_VideoHistoryStats, shared_mem_controller_retransmissions_stats* pSM_ControllerRetransmissionsStats, float scale)
{
   if ( NULL == g_pCurrentModel || NULL == pSM_RadioStats || NULL == pSM_VideoStats || NULL == pSM_VideoHistoryStats || NULL == pSM_ControllerRetransmissionsStats )
      return 0.0;

   Preferences* p = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();

   if ( g_bHasVideoDecodeStatsSnapshot && ( s_uOSDSnapshotTakeTime > 1 ) && (pCS->iVideoDecodeStatsSnapshotClosesOnTimeout == 1) )
   if ( g_TimeNow > s_uOSDSnapshotTakeTime + 10000 )
   {
      g_bHasVideoDecodeStatsSnapshot = false;
      s_uOSDSnapshotTakeTime = 1;
   }

   if ( ! bIsSnapshot )
   {
      // Initialize for the first time
      if ( 0 == s_uOSDSnapshotTakeTime )
      {
         s_uOSDSnapshotTakeTime = 1;
         s_uOSDSnapshotCount = 0;

         s_uOSDSnapshotLastDiscardedBuffers = pSM_VideoStats->total_DiscardedBuffers;
         s_uOSDSnapshotLastDiscardedSegments = pSM_VideoStats->total_DiscardedSegments;
         s_uOSDSnapshotLastDiscardedPackets = pSM_VideoStats->total_DiscardedLostPackets;

         for( int i=0; i<SNAPSHOT_HISTORY_SIZE; i++ )
         {
            s_uOSDSnapshotHistoryDiscardedBuffers[i] = 0;
            s_uOSDSnapshotHistoryDiscardedSegments[i] = 0;
            s_uOSDSnapshotHistoryDiscardedPackets[i] = 0;
         }
      }

      bool bMustTakeSnapshot = false;
      if ( g_TimeNow > pairing_getStartTime() + 5000 )
      if ( s_uOSDSnapshotLastDiscardedBuffers != pSM_VideoStats->total_DiscardedBuffers ||
           s_uOSDSnapshotLastDiscardedSegments != pSM_VideoStats->total_DiscardedSegments ||
           s_uOSDSnapshotLastDiscardedPackets != pSM_VideoStats->total_DiscardedLostPackets )
         bMustTakeSnapshot = true;

      bool bHasActivity = false;
      bool bHasSilenceAtStart = true;
      for ( int i=0; i<12; i++ )
      {
         if ( i >= MAX_HISTORY_VIDEO_INTERVALS )
            break;
         if ( i < 5 )
         if ( (pSM_ControllerRetransmissionsStats->history[i].uCountRequestedRetransmissions > 0) ||
              (pSM_ControllerRetransmissionsStats->history[i].uCountRequestedPacketsForRetransmission > 0) ||
              (pSM_VideoHistoryStats->missingTotalPacketsAtPeriod[i] > 0) )
            bHasSilenceAtStart = false;

         if ( i >= 5 )
         if ( (pSM_ControllerRetransmissionsStats->history[i].uCountRequestedRetransmissions > 0) ||
              (pSM_ControllerRetransmissionsStats->history[i].uCountRequestedPacketsForRetransmission > 0) ||
              (pSM_VideoHistoryStats->missingTotalPacketsAtPeriod[i] > 0) )
            bHasActivity = true;

         if ( i > 5 )
         if ( bHasActivity && bHasSilenceAtStart )
         {
            bMustTakeSnapshot = true;
            break;
         }
      }

      if ( bMustTakeSnapshot )
      {
         bool bTakeSnapshot = false;
         if ( !g_bHasVideoDecodeStatsSnapshot )
            bTakeSnapshot = true;
         else if ( ( pCS->iVideoDecodeStatsSnapshotClosesOnTimeout == 2 ) && (g_TimeNow > s_uOSDSnapshotTakeTime+500) )
            bTakeSnapshot = true;
         if ( bTakeSnapshot )
         {
         s_uOSDSnapshotTakeTime = g_TimeNow;
         s_uOSDSnapshotCount++;
         g_bHasVideoDecodeStatsSnapshot = true;

         for( int i=SNAPSHOT_HISTORY_SIZE-1; i>0; i-- )
         {
            s_uOSDSnapshotHistoryDiscardedBuffers[i] = s_uOSDSnapshotHistoryDiscardedBuffers[i-1];
            s_uOSDSnapshotHistoryDiscardedSegments[i] = s_uOSDSnapshotHistoryDiscardedSegments[i-1];
            s_uOSDSnapshotHistoryDiscardedPackets[i] = s_uOSDSnapshotHistoryDiscardedPackets[i-1];
         }

         s_uOSDSnapshotHistoryDiscardedBuffers[0] = pSM_VideoStats->total_DiscardedBuffers - s_uOSDSnapshotLastDiscardedBuffers;
         s_uOSDSnapshotHistoryDiscardedSegments[0] = pSM_VideoStats->total_DiscardedSegments - s_uOSDSnapshotLastDiscardedSegments;
         s_uOSDSnapshotHistoryDiscardedPackets[0] = pSM_VideoStats->total_DiscardedLostPackets - s_uOSDSnapshotLastDiscardedPackets;

         s_uOSDSnapshotLastDiscardedBuffers = pSM_VideoStats->total_DiscardedBuffers;
         s_uOSDSnapshotLastDiscardedSegments = pSM_VideoStats->total_DiscardedSegments;
         s_uOSDSnapshotLastDiscardedPackets = pSM_VideoStats->total_DiscardedLostPackets;

         memcpy( &s_OSDSnapshot_RadioStats, pSM_RadioStats, sizeof(shared_mem_radio_stats));
         memcpy( &s_OSDSnapshot_VideoDecodeStats, pSM_VideoStats, sizeof(shared_mem_video_decode_stats));
         memcpy( &s_OSDSnapshot_VideoDecodeHist, pSM_VideoHistoryStats, sizeof(shared_mem_video_decode_stats_history));
         memcpy( &s_OSDSnapshot_ControllerVideoRetransmissionsStats, pSM_ControllerRetransmissionsStats, sizeof(shared_mem_controller_retransmissions_stats));
         memcpy( &s_Snapshot_PHRTE_Retransmissions, g_pPHRTE_Retransmissions, sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions));
         }
      }
   }


   s_iPeakCountRender++;
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   float hGraph = 0.04*scale;
   float hGraphHistory = 2.2*height_text;

   float width = osd_render_stats_video_decode_get_width(iDeveloperMode, bIsSnapshot, pSM_RadioStats, pSM_VideoStats, pSM_VideoHistoryStats, pSM_ControllerRetransmissionsStats, scale);
   float height = osd_render_stats_video_decode_get_height(iDeveloperMode, bIsSnapshot, pSM_RadioStats, pSM_VideoStats, pSM_VideoHistoryStats, pSM_ControllerRetransmissionsStats, scale);
   float wPixel = g_pRenderEngine->getPixelWidth();

   char szBuff[64];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;
   width -= 2*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;
   
   if ( ! bIsSnapshot )
   {
      if ( ! (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS) )
         g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, "Decoder");
   }
   else
   {
      sprintf(szBuff, "Snapshot %d", s_uOSDSnapshotCount);
      g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, szBuff);
   }
   
   float y = yPos;

   float video_mbps = pSM_RadioStats->radio_streams[STREAM_ID_VIDEO_1].rxBytesPerSec*8/1000.0/1000.0;

   if ( ! bIsSnapshot )
   {
      szBuff[0] = 0;
      char szMode[32];
      strcpy(szMode, str_get_video_profile_name(pSM_VideoStats->video_link_profile & 0x0F));
      if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS )
         szMode[0] = 0;
      sprintf(szBuff, "%s %.1f Mbs", szMode, video_mbps);
      if ( (NULL != g_psmvds) && (g_psmvds->encoding_extra_flags & ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE) )
         sprintf(szBuff, "%s- %.1f Mbs", szMode, video_mbps);
      if ( NULL != g_pPHRTE )
      {
         sprintf(szBuff, "%s %.1f (%.1f) Mbs", szMode, video_mbps, g_pPHRTE->downlink_tx_video_bitrate/1000.0/1000.0);
         if ( (NULL != g_psmvds) && (g_psmvds->encoding_extra_flags & ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE) )
            sprintf(szBuff, "%s- %.1f (%.1f) Mbs", szMode, video_mbps, g_pPHRTE->downlink_tx_video_bitrate/1000.0/1000.0);
      }
      int nDataRate = g_pCurrentModel->getLinkDataRate(0);
      if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
      if ( g_pCurrentModel->getLinkDataRate(1) > nDataRate )
         nDataRate = g_pCurrentModel->getLinkDataRate(1);

      if ( video_mbps >= (float)(nDataRate) * DEFAULT_VIDEO_LINK_MAX_LOAD_PERCENT / 100.0 )
         g_pRenderEngine->setColors(get_Color_IconWarning());
      
      if ( g_bHasVideoDataOverloadAlarm && (g_TimeLastVideoDataOverloadAlarm > 0) && (g_TimeNow <  g_TimeLastVideoDataOverloadAlarm + 5000) )
         g_pRenderEngine->setColors(get_Color_IconError());

      if ( ! (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS) )
      {
         osd_show_value_left(xPos+width,yPos, szBuff, s_idFontStats);
         y += height_text*1.3*s_OSDStatsLineSpacing;
      }
      else
         osd_show_value_left(xPos+width,yPos, szBuff, s_idFontStatsSmall);
   }
   
   osd_set_colors();
   

   char szCurrentProfile[64];
   szCurrentProfile[0] = 0;
   strcpy(szCurrentProfile, str_get_video_profile_name(pSM_VideoStats->video_link_profile & 0x0F));
   if ( (NULL != g_psmvds) && (g_psmvds->encoding_extra_flags & ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE) )
      strcat(szCurrentProfile, "-");

   // Not compact mode

   if ( (! bIsSnapshot) && (!(g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS)) )
   {
      if ( pairing_hasReceivedVideoStreamData())
      {
         //sprintf(szBuff, "Stream: %s %dx%d, %d fps %d key", szCurrentProfile, pSM_VideoStats->width, pSM_VideoStats->height, pSM_VideoStats->fps, g_pCurrentModel->video_link_profiles[(pSM_VideoStats->video_link_profile & 0x0F)].keyframe);
         strcpy(szBuff, "N/A");
         for( int i=0; i<getOptionsVideoResolutionsCount(); i++ )
            if ( g_iOptionsVideoWidth[i] == pSM_VideoStats->width )
            if ( g_iOptionsVideoHeight[i] == pSM_VideoStats->height )
            {
               sprintf(szBuff, "%s %s %d fps %d KF", szCurrentProfile, g_szOptionsVideoRes[i], pSM_VideoStats->fps, pSM_VideoStats->keyframe);
               break;
            }

      }
      else
         sprintf(szBuff, "Stream: N/A");
      g_pRenderEngine->drawText(xPos, y, height_text*0.86, s_idFontStatsSmall, szBuff);
      y += height_text_small*s_OSDStatsLineSpacing;
   }

   // Not compact mode

   if ( (! bIsSnapshot) && (!(g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS)) )
   {
      if ( pairing_wasReceiving() || pairing_isReceiving() )
      {
      char szBuff3[64];
      szBuff3[0] = 0;
      if ( pSM_VideoStats->encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS )
         sprintf(szBuff3, "%d", g_pCurrentModel->video_params.videoAdjustmentStrength);

      char szDynamic[64];
      if ( pSM_VideoStats->encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS )
         sprintf(szDynamic, "Dynamic (%d)", g_pCurrentModel->video_params.videoAdjustmentStrength);
      else
         strcpy(szDynamic, "Fixed");

      if ( pSM_VideoStats->isRetransmissionsOn )
      {
         if ( (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex]) & OSD_FLAG_EXTENDED_VIDEO_DECODE_HISTORY )
         {
            sprintf(szBuff, "Params: 2Way / %s %s / %d ms / %d ms / %d ms", szDynamic,
                szBuff3, 5*(((pSM_VideoStats->encoding_extra_flags) & 0xFF00) >> 8), pCS->nRetryRetransmissionAfterTimeoutMS, pCS->nRequestRetransmissionsOnVideoSilenceMs );
            g_pRenderEngine->drawText(xPos, y, height_text*0.86, s_idFontStatsSmall, szBuff);
         }
         else
         {
            sprintf(szBuff, "Params: 2Way / %s %s", szDynamic, szBuff3);
            g_pRenderEngine->drawText(xPos, y, height_text*0.86, s_idFontStatsSmall, szBuff);
            y += height_text_small*s_OSDStatsLineSpacing;
            sprintf(szBuff, "     %d ms / %d ms / %d ms", 5*(((pSM_VideoStats->encoding_extra_flags) & 0xFF00) >> 8), pCS->nRetryRetransmissionAfterTimeoutMS, pCS->nRequestRetransmissionsOnVideoSilenceMs );
            g_pRenderEngine->drawText(xPos, y, height_text*0.86, s_idFontStatsSmall, szBuff);
         }
      }
      else
      {
         sprintf(szBuff, "Params: 1Way / %s %s", szDynamic, szBuff3 );
         g_pRenderEngine->drawText(xPos, y, height_text*0.86, s_idFontStatsSmall, szBuff);
      }
   }
   else
   {
      sprintf(szBuff, "Params: N/A");
      g_pRenderEngine->drawText(xPos, y, height_text*0.86, s_idFontStatsSmall, szBuff);
   }
   y += height_text_small*s_OSDStatsLineSpacing;
   }

   u32 msData = 0;
   u32 msFEC = 0;
   u32 videoBitrate = 1;
   videoBitrate = g_pCurrentModel->video_link_profiles[(pSM_VideoStats->video_link_profile & 0x0F)].bitrate_fixed_bps;
   msData = (1000*8*g_psmvds->data_packets_per_block*pSM_VideoStats->video_packet_length)/(g_pCurrentModel->video_link_profiles[(pSM_VideoStats->video_link_profile & 0x0F)].bitrate_fixed_bps+1);
   msFEC = (1000*8*g_psmvds->fec_packets_per_block*pSM_VideoStats->video_packet_length)/(g_pCurrentModel->video_link_profiles[(pSM_VideoStats->video_link_profile & 0x0F)].bitrate_fixed_bps+1);

   if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS )
      sprintf(szBuff, "EC: %s %d/%d/%d, %d KF", szCurrentProfile, pSM_VideoStats->data_packets_per_block, pSM_VideoStats->fec_packets_per_block, pSM_VideoStats->video_packet_length, pSM_VideoStats->keyframe);
   else
      sprintf(szBuff, "EC: %s %d/%d/%d, data/ec: %d/%d ms", szCurrentProfile, pSM_VideoStats->data_packets_per_block, pSM_VideoStats->fec_packets_per_block, pSM_VideoStats->video_packet_length, msData, msFEC);
   
   if ( ! bIsSnapshot )
   {
      g_pRenderEngine->drawText(xPos, y, height_text*0.86, s_idFontStatsSmall, szBuff);
      y += height_text*1.1;
   }

   int maxPacketsCount = 1;
   if ( 0 < pSM_VideoStats->maxPacketsInBuffers )
      maxPacketsCount = pSM_VideoStats->maxPacketsInBuffers;

   float fWarningFullPercent = 0.5;

  
   // ---------------------------------------
   // Begin - Draw Rx packets buffer

   if ( (! bIsSnapshot) && (!(g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS)) )
   if ( (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex]) & OSD_FLAG_EXTENDED_VIDEO_DECODE_HISTORY )
   {
      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Buffers:");
      float wPrefix = g_pRenderEngine->textWidth(height_text, s_idFontStats, "Buffers:") + 0.02*scale;
      y += height_text*0.1;

      float hBar = 0.014*scale;
      width -= wPrefix;
      osd_set_colors_background_fill();
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      g_pRenderEngine->setStroke(255,255,255,1.0);
      if ( pSM_VideoStats->currentPacketsInBuffers > fWarningFullPercent*maxPacketsCount )
         g_pRenderEngine->setStroke(250,0,0, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawRoundRect(xPos+wPrefix, y, width, hBar, 0.003*scale);
      wPrefix += 2.0/g_pRenderEngine->getScreenWidth();
      width -= 4.0/g_pRenderEngine->getScreenWidth();
      y += 2.0/g_pRenderEngine->getScreenHeight();
      hBar -= 4.0/g_pRenderEngine->getScreenHeight();

      float fBarScale = width/100.0;
      int packs = pSM_VideoStats->maxPacketsInBuffers;
      if ( packs > 0 )
         fBarScale = width/(float)(packs+1); // max buffer length

      g_pRenderEngine->setFill(145,45,45, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setStroke(0,0,0,0);
      g_pRenderEngine->setStrokeSize(0);
   
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      double* pc = get_Color_OSDText();
      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      if ( pSM_VideoStats->currentPacketsInBuffers > s_iPeakTotalPacketsInBuffer )
         s_iPeakTotalPacketsInBuffer = pSM_VideoStats->currentPacketsInBuffers;
      packs = pSM_VideoStats->maxPacketsInBuffers;
      if ( s_iPeakTotalPacketsInBuffer > packs )
         s_iPeakTotalPacketsInBuffer = packs+1;
      g_pRenderEngine->drawLine(xPos + wPrefix + fBarScale*s_iPeakTotalPacketsInBuffer, y, xPos + wPrefix + fBarScale*s_iPeakTotalPacketsInBuffer, y+hBar);
      g_pRenderEngine->drawLine(xPos + wPrefix + fBarScale*s_iPeakTotalPacketsInBuffer+wPixel, y, xPos + wPrefix + fBarScale*s_iPeakTotalPacketsInBuffer+wPixel, y+hBar);

      //if ( 0 == (s_iPeakCountRender % 3) )
      if ( s_iPeakTotalPacketsInBuffer > 0 )
         s_iPeakTotalPacketsInBuffer--;

      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      
      g_pRenderEngine->drawRect(xPos+wPrefix+1.0/g_pRenderEngine->getScreenWidth(), y, fBarScale*pSM_VideoStats->currentPacketsInBuffers-2.0/g_pRenderEngine->getScreenWidth(), hBar);
      osd_set_colors();
      
      hBar += 4.0/g_pRenderEngine->getScreenHeight();
      sprintf(szBuff, "%d", pSM_VideoStats->maxPacketsInBuffers);
      g_pRenderEngine->drawText(xPos + wPrefix-2.0/g_pRenderEngine->getScreenWidth(), y+hBar*1.0, height_text*0.9, s_idFontStatsSmall, "0");
      g_pRenderEngine->drawTextLeft(xPos + wPrefix + width-2.0/g_pRenderEngine->getScreenWidth(), y+hBar*1.0, height_text*0.9, s_idFontStatsSmall, szBuff);

      y += hBar;
      y += height_text*0.9;
   }

   // End - Draw buffers
   // ---------------------------------------

   int iHistoryStartRetransmissionsIndex = MAX_HISTORY_VIDEO_INTERVALS;
   int iHistoryEndRetransmissionsIndex = 0;
   int totalHistoryValues = MAX_HISTORY_VIDEO_INTERVALS;

   if ( (! bIsSnapshot) && (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS) )
      totalHistoryValues = MAX_HISTORY_VIDEO_INTERVALS*2/3;

   if ( bIsSnapshot )
   {
      while ( (totalHistoryValues > 10) && 
              (pSM_VideoHistoryStats->missingTotalPacketsAtPeriod[totalHistoryValues-1] == 0) &&
              (pSM_ControllerRetransmissionsStats->history[totalHistoryValues-1].uCountRequestedPacketsForRetransmission == 0) )
         totalHistoryValues--;
      iHistoryStartRetransmissionsIndex = totalHistoryValues;
      totalHistoryValues += 5;
      if ( totalHistoryValues > MAX_HISTORY_VIDEO_INTERVALS )
         totalHistoryValues = MAX_HISTORY_VIDEO_INTERVALS;

      while ( (iHistoryEndRetransmissionsIndex < iHistoryStartRetransmissionsIndex) && 
              (pSM_VideoHistoryStats->missingTotalPacketsAtPeriod[iHistoryEndRetransmissionsIndex] == 0) &&
              (pSM_ControllerRetransmissionsStats->history[iHistoryEndRetransmissionsIndex].uCountRequestedPacketsForRetransmission == 0) )
      iHistoryEndRetransmissionsIndex++;    

      if ( totalHistoryValues < 24 )
         totalHistoryValues = 24;
   }


   //------------------------------------
   // Begin - Output history graph

   
   float dxGraph = 0.01*scale;
   width = widthMax - dxGraph;
   float widthBar = width / totalHistoryValues;

   int maxGraphValue = 4;
   for( int i=0; i<totalHistoryValues; i++ )
   {
      if ( pSM_VideoHistoryStats->outputHistoryBlocksDiscardedPerPeriod[i] > maxGraphValue )
         maxGraphValue = pSM_VideoHistoryStats->outputHistoryBlocksDiscardedPerPeriod[i];
      if ( pSM_VideoHistoryStats->outputHistoryBlocksBadPerPeriod[i] > maxGraphValue )
         maxGraphValue = pSM_VideoHistoryStats->outputHistoryBlocksBadPerPeriod[i];
      if ( pSM_VideoHistoryStats->outputHistoryBlocksOkPerPeriod[i] > maxGraphValue )
         maxGraphValue = pSM_VideoHistoryStats->outputHistoryBlocksOkPerPeriod[i];
      if ( pSM_VideoHistoryStats->outputHistoryBlocksReconstructedPerPeriod[i] > maxGraphValue )
         maxGraphValue = pSM_VideoHistoryStats->outputHistoryBlocksReconstructedPerPeriod[i];
      if ( pSM_VideoHistoryStats->outputHistoryBlocksRetrasmitedPerPeriod[i] > maxGraphValue )
         maxGraphValue = pSM_VideoHistoryStats->outputHistoryBlocksRetrasmitedPerPeriod[i];
      if ( pSM_ControllerRetransmissionsStats->history[i].uCountRequestedRetransmissions > maxGraphValue )
         maxGraphValue = pSM_ControllerRetransmissionsStats->history[i].uCountRequestedRetransmissions;

      if ( pSM_VideoHistoryStats->outputHistoryBlocksRetrasmitedPerPeriod[i] + pSM_VideoHistoryStats->outputHistoryBlocksReconstructedPerPeriod[i] > maxGraphValue )
         maxGraphValue = pSM_VideoHistoryStats->outputHistoryBlocksRetrasmitedPerPeriod[i] + pSM_VideoHistoryStats->outputHistoryBlocksReconstructedPerPeriod[i];
   }

   if ( ! bIsSnapshot )
      y += height_text_small*0.1;
   sprintf(szBuff,"%d ms/bar, buff: max %d blocks", pSM_VideoHistoryStats->outputHistoryIntervalMs, pSM_VideoStats->maxBlocksAllowedInBuffers);
   if ( bIsSnapshot )
   {
      sprintf(szBuff,"%d ms/bar, dx: %d", pSM_VideoHistoryStats->outputHistoryIntervalMs, iHistoryEndRetransmissionsIndex);
      g_pRenderEngine->drawTextLeft(xPos+widthMax, y, height_text, s_idFontStats, szBuff);
   }
   else
   {
      g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small, s_idFontStatsSmall, szBuff);
      float w = g_pRenderEngine->textWidth(height_text_small, s_idFontStatsSmall, szBuff);

      sprintf(szBuff,"%.1f sec, ", (((float)totalHistoryValues) * pSM_VideoHistoryStats->outputHistoryIntervalMs)/1000.0);
      g_pRenderEngine->drawTextLeft(xPos+widthMax-w, y-height_text_small*0.2, height_text_small, s_idFontStatsSmall, szBuff);
      w += g_pRenderEngine->textWidth( height_text_small, s_idFontStatsSmall, szBuff);
   }

   if ( bIsSnapshot )
      y += height_text*1.5;
   else
      y += height_text_small*1.0;

   float yStartGraphs = y;

   sprintf(szBuff, "%d", maxGraphValue);
   g_pRenderEngine->drawText(xPos, y-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraphHistory-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, "0");

   double* pc = get_Color_OSDText();
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraphHistory, xPos + dxGraph + width, y+hGraphHistory);
   float midLine = hGraphHistory/2.0;
   for( float i=0; i<=width-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(0);

   u32 maxHistoryPacketsGap = 0;
   float hBarBad = 0.0;
   float hBarMissing = 0.0;
   float hBarReconstructed = 0.0;
   float hBarOk = 0.0;
   float hBarRetransmitted = 0.0;
   float hBarBottom = 0.0;
   float yBottomGraph = y + hGraphHistory;// - 1.0/g_pRenderEngine->getScreenHeight();

   float xBarSt = xPos + widthMax - g_pRenderEngine->getPixelWidth();
   float xBarMid = xBarSt + widthBar*0.5;
   float xBarEnd = xBarSt + widthBar - g_pRenderEngine->getPixelWidth();

   float fWidthBarRect = widthBar-wPixel;
   if ( fWidthBarRect < 2.0 * wPixel )
      fWidthBarRect = widthBar;

   for( int i=0; i<totalHistoryValues; i++ )
   {
      xBarSt -= widthBar;
      xBarEnd -= widthBar;

      if ( pSM_VideoHistoryStats->outputHistoryBlocksDiscardedPerPeriod[i] > 0 )
      {
         hBarBad = hGraph;
         float hBarBadTop = yBottomGraph-hGraphHistory;
         if ( pSM_VideoHistoryStats->outputHistoryBlocksOkPerPeriod[i] > 0 ||
              pSM_VideoHistoryStats->outputHistoryBlocksReconstructedPerPeriod[i] > 0 )
         {
            hBarBad = hGraphHistory*0.5;
            if ( pSM_VideoHistoryStats->outputHistoryBlocksReconstructedPerPeriod[i] > 0 )
               g_pRenderEngine->setFill(0,200,30, s_fOSDStatsGraphLinesAlpha);
            else
               g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
            g_pRenderEngine->drawRect(xBarSt, hBarBadTop + hBarBad, widthBar-wPixel, hBarBad);
         }
         if ( pSM_VideoHistoryStats->outputHistoryReceivedVideoPackets[i] > 0 || pSM_VideoHistoryStats->outputHistoryReceivedVideoRetransmittedPackets[i] > 0 )
            g_pRenderEngine->setFill(240,220,0,0.85);
         else
            g_pRenderEngine->setFill(240,20,0,0.85);
         g_pRenderEngine->drawRect(xBarSt, hBarBadTop, widthBar-wPixel, hBarBad);
         continue;
      }

      u32 val = pSM_VideoHistoryStats->outputHistoryBlocksMaxPacketsGapPerPeriod[i];
      if ( val > maxHistoryPacketsGap )
         maxHistoryPacketsGap = val;

      hBarBad = 0.0;
      hBarMissing = 0.0;
      hBarReconstructed = 0.0;
      hBarOk = 0.0;
      hBarRetransmitted = 0.0;
      float fPercentageUsed = 0.0;

      float percentBad = 0.0;
      percentBad = (float)(pSM_VideoHistoryStats->outputHistoryBlocksBadPerPeriod[i])/(float)(maxGraphValue);
      if ( percentBad > 1.0 ) percentBad = 1.0;
      if ( percentBad > 0.001 )
      {
         hBarBad = hGraphHistory*percentBad;
         g_pRenderEngine->setFill(240,220,0,0.65);
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hGraphHistory * fPercentageUsed - hBarBad, fWidthBarRect, hBarBad);
         fPercentageUsed += percentBad;
         if ( fPercentageUsed > 1.0 )
            fPercentageUsed = 1.0;
      }

      float percentMissing = (float)(pSM_VideoHistoryStats->outputHistoryBlocksMissingPerPeriod[i])/(float)(maxGraphValue);
      if ( percentMissing > 1.0 ) percentMissing = 1.0;
      if ( percentMissing > 0.001 )
      {
         if ( percentMissing + fPercentageUsed > 1.0 )
            percentMissing = 1.0 - fPercentageUsed;
         if ( percentMissing < 0.0 )
            percentMissing = 0.0;
         hBarMissing = hGraphHistory*percentMissing;
         g_pRenderEngine->setFill(240,0,0,0.74);
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hGraphHistory * fPercentageUsed - hBarMissing, fWidthBarRect, hBarMissing);
         fPercentageUsed += percentMissing;
         if ( fPercentageUsed > 1.0 )
            fPercentageUsed = 1.0;
      }

      float percentOk = (float)(pSM_VideoHistoryStats->outputHistoryBlocksOkPerPeriod[i])/(float)(maxGraphValue);
      if ( percentOk > 1.0 ) percentOk = 1.0;
      if ( percentOk > 0.001 )
      {
         if ( percentOk + fPercentageUsed > 1.0 )
            percentOk = 1.0 - fPercentageUsed;
         if ( percentOk < 0.0 )
            percentOk = 0.0;
         hBarOk = hGraphHistory*percentOk;
         if ( pSM_VideoHistoryStats->outputHistoryBlocksReconstructedPerPeriod[i] > 0 )
            g_pRenderEngine->setFill(pc[0]*0.9, pc[1]*0.9, pc[2]*0.6, s_fOSDStatsGraphLinesAlpha*0.5);
         else
            g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha*0.9);

         g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hGraphHistory * fPercentageUsed - hBarOk, fWidthBarRect, hBarOk);
         fPercentageUsed += percentOk;
         if ( fPercentageUsed > 1.0 )
            fPercentageUsed = 1.0;
      }
      float percentRecon = (float)(pSM_VideoHistoryStats->outputHistoryBlocksReconstructedPerPeriod[i])/(float)(maxGraphValue);
      if ( percentRecon > 1.0 ) percentRecon = 1.0;
      if ( percentRecon > 0.001 )
      {
         if ( percentRecon + fPercentageUsed > 1.0 )
            percentRecon = 1.0 - fPercentageUsed;
         if ( percentRecon < 0.0 )
            percentRecon = 0.0;
         hBarReconstructed = hGraphHistory*percentRecon;
         g_pRenderEngine->setFill(0,200,30, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hGraphHistory * fPercentageUsed - hBarReconstructed, fWidthBarRect, hBarReconstructed);
         fPercentageUsed += percentRecon;
         if ( fPercentageUsed > 1.0 )
            fPercentageUsed = 1.0;
      }

      float percentRetrans = (float)(pSM_VideoHistoryStats->outputHistoryBlocksRetrasmitedPerPeriod[i])/(float)(maxGraphValue);
      if ( percentRetrans > 1.0 ) percentRetrans = 1.0;
      if ( percentRetrans > 0.001 )
      {
         if ( percentRetrans + fPercentageUsed > 1.0 )
            fPercentageUsed = 1.0 - percentRetrans;
         
         hBarRetransmitted = hGraphHistory*percentRetrans;
         g_pRenderEngine->setFill(20,20,230, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hGraphHistory * fPercentageUsed - hBarRetransmitted, fWidthBarRect, hBarRetransmitted);
         fPercentageUsed += percentRetrans;
         if ( fPercentageUsed > 1.0 )
            fPercentageUsed = 1.0;
       
      }
      if ( 0 < pSM_ControllerRetransmissionsStats->history[i].uCountRequestedRetransmissions )
      {
         g_pRenderEngine->setFill(10,210,210, s_fOSDStatsGraphLinesAlpha);
         float hPR = hGraphHistory*(0.1 + 0.25*(float)(pSM_ControllerRetransmissionsStats->history[i].uCountRequestedRetransmissions)/(float)(maxGraphValue));
         g_pRenderEngine->drawRect(xBarSt, y, fWidthBarRect, hPR);
      }
   }

   for( int i=totalHistoryValues; i<MAX_HISTORY_VIDEO_INTERVALS; i++ )
   {
      u32 val = pSM_VideoHistoryStats->outputHistoryBlocksMaxPacketsGapPerPeriod[i];
      if ( val > maxHistoryPacketsGap )
         maxHistoryPacketsGap = val;
   }
   y += hGraphHistory;

   // End history buffer
   //----------------------------------------------------------------

   osd_set_colors();

   // ---------------------------------------------
   // Compact mode

   if ( (!bIsSnapshot) && (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS) )
   {
      y += height_text_small*0.5;

      strcpy(szBuff, "Params: ");
      if ( pSM_VideoStats->isRetransmissionsOn )
      {
         char szBuff3[64];
         sprintf(szBuff3, "2Way %d ms, ", 5*(((pSM_VideoStats->encoding_extra_flags) & 0xFF00) >> 8));
         strcat(szBuff, szBuff3);
      }
      else
         strcat(szBuff, "1Way, ");

      if ( pSM_VideoStats->encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS )
      {
         strcat(szBuff, "Dynamic ");
         char szTmp5[32];
         sprintf(szTmp5,"(%d)", g_pCurrentModel->video_params.videoAdjustmentStrength);
         strcat(szBuff, szTmp5);
      }
      else
         strcat(szBuff, "Fixed");

      //char szBuff2[64];
      //sprintf(szBuff2, " %d KF", pSM_VideoStats->keyframe);
      //strcat(szBuff, szBuff2);
      g_pRenderEngine->drawText(xPos, y, height_text*0.86, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
      
      int maxPacketsPerSec = pSM_RadioStats->radio_streams[STREAM_ID_VIDEO_1].rxPacketsPerSec; 
      g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStats, "Received Packets/sec:");
      sprintf(szBuff, "%d", maxPacketsPerSec);
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStats, "Discarded buff/seg/pack:");
      sprintf(szBuff, "%u/%u/%u", pSM_VideoStats->total_DiscardedBuffers, pSM_VideoStats->total_DiscardedSegments, pSM_VideoStats->total_DiscardedLostPackets);
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      if ( iDeveloperMode )
      {
         g_pRenderEngine->setColors(get_Color_Dev());

         g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Retransmissions (Req/Ack/Recv):");
         y += height_text*s_OSDStatsLineSpacing;

         g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "   Total:");
         strcpy(szBuff, "N/A");
         int percent = 0;
         if ( NULL != pSM_ControllerRetransmissionsStats && NULL != g_pPHRTE_Retransmissions )
         {
            if ( pSM_ControllerRetransmissionsStats->totalRequestedRetransmissions > 0 )
               percent = pSM_ControllerRetransmissionsStats->totalReceivedRetransmissions*100/pSM_ControllerRetransmissionsStats->totalRequestedRetransmissions;
            sprintf(szBuff, "%u / %u / %u %d%%", pSM_ControllerRetransmissionsStats->totalRequestedRetransmissions, g_pPHRTE_Retransmissions->totalReceivedRetransmissionsRequestsUnique, pSM_ControllerRetransmissionsStats->totalReceivedRetransmissions, percent);
         }
         g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
         y += height_text*s_OSDStatsLineSpacing;

         g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "   Last 500ms:");
         strcpy(szBuff, "N/A");
         percent = 0;
         if ( NULL != pSM_ControllerRetransmissionsStats && NULL != g_pPHRTE_Retransmissions )
         {
            if ( pSM_ControllerRetransmissionsStats->totalRequestedRetransmissionsLast500Ms > 0 )
               percent = pSM_ControllerRetransmissionsStats->totalReceivedRetransmissionsLast500Ms*100/pSM_ControllerRetransmissionsStats->totalRequestedRetransmissionsLast500Ms;
            sprintf(szBuff, "%u / %u / %u %d%%", pSM_ControllerRetransmissionsStats->totalRequestedRetransmissionsLast500Ms, g_pPHRTE_Retransmissions->totalReceivedRetransmissionsRequestsUniqueLast5Sec, pSM_ControllerRetransmissionsStats->totalReceivedRetransmissionsLast500Ms, percent);
         }
         g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
         y += height_text*s_OSDStatsLineSpacing;
         osd_set_colors();
      }

      return height;
   }

   // End compact mode
   // ---------------------------------------

   // Max EC packets used

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( !(g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS) )
   {
      g_pRenderEngine->setColors(get_Color_Dev());
      float maxValue = 2.0;
      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pSM_VideoHistoryStats->outputHistoryMaxECPacketsUsedPerPeriod[i] > maxValue )
            maxValue = pSM_VideoHistoryStats->outputHistoryMaxECPacketsUsedPerPeriod[i];
      }

      y += height_text_small*0.2;
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text_small, s_idFontStatsSmall, "Max EC packets used");
      y += height_text_small*1.0;

      sprintf(szBuff, "%d", (int)maxValue);
      g_pRenderEngine->drawText(xPos, y-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos, y+hGraph*0.6-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, "0");

      pc = get_Color_Dev();
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
      g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph*0.6, xPos + dxGraph + width, y+hGraph*0.6);
      midLine = hGraph*0.6/2.0;
      for( float i=0; i<=width-2.0*wPixel; i+= 5*wPixel )
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

      g_pRenderEngine->setStrokeSize(0);

      float hPixel = g_pRenderEngine->getPixelHeight();
      yBottomGraph = y + hGraph*0.6 - hPixel;

      xBarSt = xPos + widthMax - widthBar;
      xBarEnd = xBarSt + widthBar - g_pRenderEngine->getPixelWidth();
      xBarMid = xBarSt + widthBar*0.5;
      for( int i=0; i<totalHistoryValues; i++ )
      {
         float fPercent = (float)pSM_VideoHistoryStats->outputHistoryMaxECPacketsUsedPerPeriod[i]/maxValue;
         if ( pSM_VideoHistoryStats->outputHistoryMaxECPacketsUsedPerPeriod[i] > 0 )
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hGraph*0.6*fPercent + hPixel, widthBar-wPixel, hGraph*0.6*fPercent);
         xBarSt -= widthBar;
         xBarMid -= widthBar;
         xBarEnd -= widthBar;
      }
      y += hGraph*0.6;
   }

   // Max EC packets used
   //---------------------------------------------


   // ----------------------------------------------------
   // History packets max gap graph

   if ( iDeveloperMode )
   {
      g_pRenderEngine->setColors(get_Color_Dev());

      g_pRenderEngine->drawTextLeft(xPos+widthMax, y, height_text_small, s_idFontStatsSmall, "Packets Max Gap");
      y += height_text_small*1.0;
      int maxGraphValue = 4;

      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pSM_VideoHistoryStats->outputHistoryBlocksMaxPacketsGapPerPeriod[i] > maxGraphValue )
            maxGraphValue = pSM_VideoHistoryStats->outputHistoryBlocksMaxPacketsGapPerPeriod[i];
      }

      sprintf(szBuff, "%d", maxGraphValue);
      g_pRenderEngine->drawText(xPos, y-height_text_small*0.5, height_text_small*0.8, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText( xPos, y+hGraph*0.6-height_text_small*0.5, height_text_small*0.8, s_idFontStatsSmall, "0");

      double* pc = get_Color_Dev();
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
      g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph*0.6, xPos + dxGraph + width, y+hGraph*0.6);
      float midLine = hGraph*0.6/2.0;
   
      for( float i=0.0; i<=width-2.0*wPixel; i+= 5.0*wPixel )
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

      g_pRenderEngine->setStrokeSize(0);
      float hBarGap = 0.0;
      yBottomGraph = y + hGraph*0.6 - 1.0/g_pRenderEngine->getScreenHeight();

      g_pRenderEngine->setFill(200,50,50, s_fOSDStatsGraphLinesAlpha);

      for( int i=0; i<totalHistoryValues; i++ )
      {
         hBarGap = 0;
         u8 val = pSM_VideoHistoryStats->outputHistoryBlocksMaxPacketsGapPerPeriod[i]; 
         if ( val > 0 )
         {
            hBarGap = hGraph*0.6 * (0.1 + 0.9 * val/(float)maxGraphValue);
            if ( hBarGap > hGraph*0.6 )
               hBarGap = hGraph*0.6;
            g_pRenderEngine->drawRect(xPos+widthMax-(i+1)*widthBar, yBottomGraph-hBarGap, widthBar-wPixel, hBarGap);
         }
      }

      y += hGraph*0.6 + height_text_small*0.4;
      y += height_text_small*0.2;
   }

   // History packets max gap graph
   // ----------------------------------------------------


   // -----------------------------------------
   // Pending good blocks to output graph

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   {
      g_pRenderEngine->setColors(get_Color_Dev());
      float maxValue = 1.0;
      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pSM_VideoHistoryStats->outputHistoryMaxGoodBlocksPendingPerPeriod[i] > maxValue )
            maxValue = pSM_VideoHistoryStats->outputHistoryMaxGoodBlocksPendingPerPeriod[i];
      }

      y -= height_text_small*0.2;
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text_small, s_idFontStatsSmall, "Max good blocks pending output");
      y += height_text_small*1.1;

      sprintf(szBuff, "%d", (int)maxValue);
      g_pRenderEngine->drawText(xPos, y-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos, y+hGraph*0.8-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, "0");

      pc = get_Color_Dev();
      g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
      g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph*0.8, xPos + dxGraph + width, y+hGraph*0.8);
      midLine = hGraph*0.8/2.0;
      for( float i=0; i<=width-2.0*wPixel; i+= 5*wPixel )
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

      g_pRenderEngine->setStrokeSize(0);

      float hPixel = g_pRenderEngine->getPixelHeight();
      yBottomGraph = y + hGraph*0.8 - hPixel;

      xBarSt = xPos + widthMax - widthBar;
      xBarEnd = xBarSt + widthBar - g_pRenderEngine->getPixelWidth();
      xBarMid = xBarSt + widthBar*0.5;

      for( int i=0; i<totalHistoryValues; i++ )
      {
         float fPercent = (float)pSM_VideoHistoryStats->outputHistoryMaxGoodBlocksPendingPerPeriod[i]/maxValue;
         if ( pSM_VideoHistoryStats->outputHistoryMaxGoodBlocksPendingPerPeriod[i] > 0 )
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hGraph*0.8*fPercent+hPixel, widthBar-wPixel, hGraph*0.8*fPercent);

         xBarSt -= widthBar;
         xBarMid -= widthBar;
         xBarEnd -= widthBar;
      }
      y += hGraph*0.8;
      y += height_text_small*0.3;
      
   }

   // Pending good blocks to output graph
   // -----------------------------------------

   //-------------------------------------------------
   // History requested retransmissions vs missing packets

   float hGraphRetransmissions = hGraph;
   if ( bIsSnapshot )
       hGraphRetransmissions = hGraph*1.6;

   float maxValue = 1.0;
   for( int i=0; i<totalHistoryValues; i++ )
   {
      if ( pSM_ControllerRetransmissionsStats->history[i].uCountReRequestedPacketsForRetransmission > maxValue )
         maxValue = pSM_ControllerRetransmissionsStats->history[i].uCountReRequestedPacketsForRetransmission;
      if ( pSM_ControllerRetransmissionsStats->history[i].uCountRequestedPacketsForRetransmission > maxValue )
         maxValue = pSM_ControllerRetransmissionsStats->history[i].uCountRequestedPacketsForRetransmission;
      if ( pSM_VideoHistoryStats->missingTotalPacketsAtPeriod[i] > maxValue )
         maxValue = pSM_VideoHistoryStats->missingTotalPacketsAtPeriod[i];
   }
   //if ( maxValue > 40 )
   //   maxValue = 40;
   osd_set_colors();

   //g_pRenderEngine->drawTextLeft(rightMargin, y, height_text_small, s_idFontStatsSmall, "Missing packets / Req retrans.");
 
   float ftmp = 0;
   osd_set_colors();
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text_small, s_idFontStatsSmall, " packets");
   ftmp += g_pRenderEngine->textWidth(height_text_small, s_idFontStatsSmall, " packets");
 
   g_pRenderEngine->setFill(10,50,200, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStroke(10,50,200, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawTextLeft(rightMargin-ftmp, y, height_text_small, s_idFontStatsSmall, " Re-Req");
   ftmp += g_pRenderEngine->textWidth(height_text_small, s_idFontStatsSmall, " Re-Req");
   osd_set_colors();


   g_pRenderEngine->setFill(150,180,250, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStroke(150,180,250, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawTextLeft(rightMargin-ftmp, y, height_text_small, s_idFontStatsSmall, "/ Req /");
   ftmp += g_pRenderEngine->textWidth(height_text_small, s_idFontStatsSmall, "/ Req /");
   osd_set_colors();

   g_pRenderEngine->setFill(255,50,0, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setStroke(255,50,0, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawTextLeft(rightMargin-ftmp, y, height_text_small, s_idFontStatsSmall, "Total Missing ");
   ftmp += g_pRenderEngine->textWidth(height_text_small, s_idFontStatsSmall, "Missing ");
   osd_set_colors();

   y += height_text_small*1.0;
   y += height_text_small*0.2;
   
   osd_set_colors();

   sprintf(szBuff, "%d", (int)maxValue);
   g_pRenderEngine->drawTextLeft(xPos+dxGraph-2.0*g_pRenderEngine->getPixelWidth(), y-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, szBuff);
   sprintf(szBuff, "%d", (int)(maxValue/2.0));
   g_pRenderEngine->drawTextLeft(xPos+dxGraph-2.0*g_pRenderEngine->getPixelWidth(), y+hGraphRetransmissions*0.5-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawTextLeft(xPos+dxGraph-2.0*g_pRenderEngine->getPixelWidth(), y+hGraphRetransmissions-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, "0");

   pc = get_Color_OSDText();
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraphRetransmissions, xPos + dxGraph + width, y+hGraphRetransmissions);
   midLine = hGraphRetransmissions/2.0;
   for( float i=0; i<=width-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(0);

   yBottomGraph = y + hGraphRetransmissions - 1.0/g_pRenderEngine->getScreenHeight();

   xBarSt = xPos + widthMax - widthBar;
   xBarEnd = xBarSt + widthBar;
   
   float hPixel = g_pRenderEngine->getPixelHeight();
   
   for( int i=0; i<totalHistoryValues; i++ )
   {
      g_pRenderEngine->setStrokeSize(1.0);
       
      g_pRenderEngine->setStrokeSize(1.0);
      g_pRenderEngine->setStroke(250,250,250, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(150,180,250, s_fOSDStatsGraphLinesAlpha);
      float fPercentTotalReq = (float)pSM_ControllerRetransmissionsStats->history[i].uCountRequestedPacketsForRetransmission/maxValue;
      if ( fPercentTotalReq > 1.0 )
         fPercentTotalReq = 1.0;
      float hBarReq = hGraphRetransmissions*fPercentTotalReq;
      if ( maxValue > 20 )
      if ( pSM_ControllerRetransmissionsStats->history[i].uCountRequestedPacketsForRetransmission < maxValue-2 )
      hBarReq += 2.0*g_pRenderEngine->getPixelHeight();
      if ( pSM_ControllerRetransmissionsStats->history[i].uCountRequestedPacketsForRetransmission != 0 )
         g_pRenderEngine->drawRect(xBarSt, y+hGraphRetransmissions - hBarReq, widthBar-wPixel, hBarReq);

      g_pRenderEngine->setStroke(200,200,200, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(10,50,200, s_fOSDStatsGraphLinesAlpha);

      float fPercentTotalReReq = (float)pSM_ControllerRetransmissionsStats->history[i].uCountReRequestedPacketsForRetransmission/maxValue;
      if ( fPercentTotalReReq > 1.0 )
         fPercentTotalReReq = 1.0;

      float hBarReReq = hGraphRetransmissions*fPercentTotalReReq;
      if ( maxValue > 20 )
      if ( pSM_ControllerRetransmissionsStats->history[i].uCountReRequestedPacketsForRetransmission < maxValue-2 )
      hBarReReq += 2.0*g_pRenderEngine->getPixelHeight();
  
      if ( pSM_ControllerRetransmissionsStats->history[i].uCountReRequestedPacketsForRetransmission != 0 )
         g_pRenderEngine->drawRect(xBarSt, yBottomGraph - hBarReReq, widthBar-wPixel, hBarReReq);
           
      float fPercentTotalMissing = (float)pSM_VideoHistoryStats->missingTotalPacketsAtPeriod[i]/maxValue;
      if ( fPercentTotalMissing > 1.0 )
         fPercentTotalMissing = 1.0;
      
      if ( (pSM_VideoHistoryStats->missingTotalPacketsAtPeriod[i] != 0) ||
            ((i<totalHistoryValues-1) && (pSM_VideoHistoryStats->missingTotalPacketsAtPeriod[i+1] != 0)) )
      {
         g_pRenderEngine->setStrokeSize(2.0);
         g_pRenderEngine->setStroke(255,50,0, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->drawLine(xBarSt, y+hGraphRetransmissions-hGraphRetransmissions*fPercentTotalMissing, xBarSt + widthBar, y+hGraphRetransmissions-hGraphRetransmissions*fPercentTotalMissing);
         if ( i < totalHistoryValues-1 )
         {
            float fPercentTotalMissingNext = (float)pSM_VideoHistoryStats->missingTotalPacketsAtPeriod[i+1]/maxValue;
            if ( fPercentTotalMissingNext > 1.0 )
               fPercentTotalMissingNext = 1.0;
         
            g_pRenderEngine->drawLine(xBarSt, y+hGraphRetransmissions-hGraphRetransmissions*fPercentTotalMissing, xBarSt, y+hGraphRetransmissions-hGraphRetransmissions*fPercentTotalMissingNext);    
         }
      }

      xBarSt -= widthBar;
      xBarEnd -= widthBar;
   }

   y += hGraphRetransmissions;

   // History requested retransmissions vs missing packets
   //-------------------------------------------------

   if ( bIsSnapshot )
   {
      //-------------------------------------------------
      // History received retransmission packets and video packets

      osd_set_colors();

      float maxValue = 2.0;
      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pSM_VideoHistoryStats->outputHistoryReceivedVideoPackets[i] + pSM_VideoHistoryStats->outputHistoryReceivedVideoRetransmittedPackets[i] > maxValue )
            maxValue = pSM_VideoHistoryStats->outputHistoryReceivedVideoPackets[i] + pSM_VideoHistoryStats->outputHistoryReceivedVideoRetransmittedPackets[i];
      }

      y += height_text_small*0.2;
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text_small, s_idFontStatsSmall, "Received video / retransmission packets");
      y += height_text_small*1.1;

      sprintf(szBuff, "%d", (int)maxValue);
      g_pRenderEngine->drawText(xPos, y-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, szBuff);
      sprintf(szBuff, "%d", (int)(maxValue/2.0));
      g_pRenderEngine->drawText(xPos, y+hGraph*0.5-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, szBuff);
      g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.5, height_text*0.9, s_idFontStatsSmall, "0");

      
      g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
      g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + width, y+hGraph);
      midLine = hGraph/2.0;
      for( float i=0; i<=width-2.0*wPixel; i+= 5*wPixel )
         g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

      g_pRenderEngine->setStrokeSize(0);

      float hPixel = g_pRenderEngine->getPixelHeight();
      yBottomGraph = y + hGraph - hPixel;

      xBarSt = xPos + widthMax - widthBar;
      xBarEnd = xBarSt + widthBar - g_pRenderEngine->getPixelWidth();
      xBarMid = xBarSt + widthBar*0.5;

      for( int i=0; i<totalHistoryValues; i++ )
      {
         g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha*0.9);
         
         float fPercentVideo = (float)pSM_VideoHistoryStats->outputHistoryReceivedVideoPackets[i]/maxValue;
         if ( pSM_VideoHistoryStats->outputHistoryReceivedVideoPackets[i] > 0 )
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hGraph*fPercentVideo+hPixel, widthBar-wPixel, hGraph*fPercentVideo);
      
         g_pRenderEngine->setFill(50,100,250, s_fOSDStatsGraphLinesAlpha*0.9);
         
         float fPercentRetr = (float)pSM_VideoHistoryStats->outputHistoryReceivedVideoRetransmittedPackets[i]/maxValue;
         if ( pSM_VideoHistoryStats->outputHistoryReceivedVideoRetransmittedPackets[i] > 0 )
            g_pRenderEngine->drawRect(xBarSt, yBottomGraph-hGraph*(fPercentVideo+fPercentRetr)+hPixel, widthBar-wPixel, hGraph*fPercentRetr);

         xBarSt -= widthBar;
         xBarMid -= widthBar;
         xBarEnd -= widthBar;

      }
      y += hGraph;
      y += height_text_small*0.3;
      
      osd_set_colors();

      // History received retransmission packets and video packets
      //-------------------------------------------------
   }

   if ( ! bIsSnapshot )
   {
      int iMilisecondsToBuffer = ((pSM_VideoStats->encoding_extra_flags & 0xFF00) >> 8) * 5;
      float wTmp = (widthBar*(float)iMilisecondsToBuffer/pSM_VideoHistoryStats->outputHistoryIntervalMs);

      pc = get_Color_OSDText();
      g_pRenderEngine->setColors(pc);
      g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], 0.5);
      g_pRenderEngine->setStrokeSize(g_pRenderEngine->getPixelWidth());
      g_pRenderEngine->setFill(pc[0], pc[1], pc[2], 0.2);

      g_pRenderEngine->drawRect(xPos + widthMax - wTmp, y-hGraphRetransmissions-height_text_small*0.15, wTmp, hGraphRetransmissions + height_text_small*0.3);
   }
   osd_set_colors();


   // --------------------------------------------------------------
   // History received/ack/completed/dropped retransmissions

   if ( bIsSnapshot )
   {
      osd_set_colors();

      y += height_text_small*0.2;
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text_small, s_idFontStatsSmall, "Req/Ack/Done/Dropped Retransmissions");
      y += height_text_small*1.1;

      g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + width, y);         
      g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + width, y+hGraph);
     
      yBottomGraph = y + hGraph;

      xBarSt = xPos + widthMax - widthBar;
      xBarEnd = xBarSt + widthBar - g_pRenderEngine->getPixelWidth();
      xBarMid = xBarSt + widthBar*0.5;

      for( int i=0; i<totalHistoryValues; i++ )
      {
         if ( pSM_ControllerRetransmissionsStats->history[i].uCountRequestedRetransmissions > 0 )
         {
            g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha*0.9);
            g_pRenderEngine->fillCircle(xBarMid, yBottomGraph - hGraph*0.13, hGraph*0.12);
         }

         if ( pSM_ControllerRetransmissionsStats->history[i].uCountAcknowledgedRetransmissions > 0 )
         {
            g_pRenderEngine->setFill(50,100,250, s_fOSDStatsGraphLinesAlpha*0.9);
            g_pRenderEngine->fillCircle(xBarMid, yBottomGraph - hGraph*0.37, hGraph*0.12);
         }

         if ( pSM_ControllerRetransmissionsStats->history[i].uCountCompletedRetransmissions > 0 )
         {
            g_pRenderEngine->setFill(100, 250, 100, s_fOSDStatsGraphLinesAlpha*0.9);
            g_pRenderEngine->fillCircle(xBarMid, yBottomGraph - hGraph*0.63, hGraph*0.12);
         }
        
         if ( pSM_ControllerRetransmissionsStats->history[i].uCountDroppedRetransmissions > 0 )
         {
            g_pRenderEngine->setFill(250,50,50, s_fOSDStatsGraphLinesAlpha*0.9);
            g_pRenderEngine->fillCircle(xBarMid, yBottomGraph - hGraph*0.87, hGraph*0.12);
         }
        
         xBarSt -= widthBar;
         xBarMid -= widthBar;
         xBarEnd -= widthBar;

      }
      y += hGraph;
      y += height_text_small*0.3;
      
      osd_set_colors();
   }

   // History received/ack/completed/dropped retransmissions
   // --------------------------------------------------------------

   // End - Output history graphs
   //------------------------------------


   if ( bIsSnapshot )
   {
      g_pRenderEngine->setStrokeSize(2.0);
      g_pRenderEngine->setStroke(250,240,50, s_fOSDStatsGraphLinesAlpha);
      
      float xS = xPos + widthMax - widthBar - iHistoryStartRetransmissionsIndex*widthBar;
      float xE = xPos + widthMax - iHistoryEndRetransmissionsIndex*widthBar;
      g_pRenderEngine->drawLine(xS, yStartGraphs, xS, y);
      g_pRenderEngine->drawLine(xE, yStartGraphs, xE, y);

      if ( iHistoryStartRetransmissionsIndex - iHistoryEndRetransmissionsIndex >= 12 )
      {
         for( float yTmp=yStartGraphs; yTmp<=y-0.02; yTmp += 0.02 )
         {
             g_pRenderEngine->drawLine(xS+0.25*(xE-xS), yTmp, xS+0.25*(xE-xS), yTmp+0.01);
             g_pRenderEngine->drawLine(xS+0.5*(xE-xS), yTmp, xS+0.5*(xE-xS), yTmp+0.01);
             g_pRenderEngine->drawLine(xS+0.75*(xE-xS), yTmp, xS+0.75*(xE-xS), yTmp+0.01);
         }
      }
      else if ( iHistoryStartRetransmissionsIndex - iHistoryEndRetransmissionsIndex >= 6 )
      {
         for( float yTmp=yStartGraphs; yTmp<=y-0.02; yTmp += 0.02 )
             g_pRenderEngine->drawLine((xS+xE)/2.0, yTmp, (xS+xE)/2.0, yTmp+0.01);
      }
      osd_set_colors();
      sprintf(szBuff, "%d ms (%d intervals)", (iHistoryStartRetransmissionsIndex-iHistoryEndRetransmissionsIndex+1)*pSM_VideoHistoryStats->outputHistoryIntervalMs, iHistoryStartRetransmissionsIndex-iHistoryEndRetransmissionsIndex+1);
      osd_show_value_centered((xS+xE)/2.0,y, szBuff, s_idFontStats);
      y += 1.1*height_text*s_OSDStatsLineSpacing;
   }

   osd_set_colors();
   if ( ! bIsSnapshot )
   if ( (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex]) & OSD_FLAG_EXT_SHOW_ADAPTIVE_VIDEO_GRAPH )
   {
      y += height_text_small*0.7;
      osd_render_stats_adaptive_video_graph(xPos, y);
      y += osd_render_stats_adaptive_video_graph_get_height();
   }

   osd_set_colors();

   //if ( (!pCS->iDeveloperMode) && (!s_bDebugStatsShowAll) )
      y += height_text_small*0.3;

   g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStats, "Lost packets max gap:");
   sprintf(szBuff, "%d", maxHistoryPacketsGap);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStats, "Lost packets max gap time:");
   if ( videoBitrate > 0 )
      sprintf(szBuff, "%d ms", (maxHistoryPacketsGap*pSM_VideoStats->video_packet_length*1000*8)/videoBitrate);
   else
      sprintf(szBuff, "N/A ms");
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   int maxPacketsPerSec = pSM_RadioStats->radio_streams[STREAM_ID_VIDEO_1].rxPacketsPerSec; 
   if ( ! bIsSnapshot )
   {
      g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStats, "Received Packets/sec:");
      sprintf(szBuff, "%d", maxPacketsPerSec);
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStats, "Received Blocks/sec:");

      //int val = pSM_VideoStats->data_packets_per_block + pSM_VideoStats->fec_packets_per_block;
      //if ( val > 0 )
      //   sprintf(szBuff, "%d", maxPacketsPerSec/val);
      //else
      //   sprintf(szBuff, "0");
      u32 uCountBlocksOut = 0;
      u32 uTimeMs = 0;
      for( int i=0; i<totalHistoryValues/2; i++ )
      {
         uCountBlocksOut += pSM_VideoHistoryStats->outputHistoryBlocksOkPerPeriod[i];
         uCountBlocksOut += pSM_VideoHistoryStats->outputHistoryBlocksReconstructedPerPeriod[i];
         uTimeMs += pSM_VideoHistoryStats->outputHistoryIntervalMs;
      }
      if ( uTimeMs != 0 )
         sprintf(szBuff, "%d", uCountBlocksOut * 1000 / uTimeMs );
      else
         sprintf(szBuff, "0");

      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
   }


   if ( iDeveloperMode )
   {
      g_pRenderEngine->setColors(get_Color_Dev());

      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Retransmissions (Req/Ack/Recv):");
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "   Total:");
      strcpy(szBuff, "N/A");
      int percent = 0;
      if ( NULL != g_pPHRTE_Retransmissions && NULL != pSM_ControllerRetransmissionsStats )
      {
         if ( pSM_ControllerRetransmissionsStats->totalRequestedRetransmissions > 0 )
            percent = pSM_ControllerRetransmissionsStats->totalReceivedRetransmissions*100/pSM_ControllerRetransmissionsStats->totalRequestedRetransmissions;
         sprintf(szBuff, "%u / %u / %u %d%%", pSM_ControllerRetransmissionsStats->totalRequestedRetransmissions, g_pPHRTE_Retransmissions->totalReceivedRetransmissionsRequestsUnique, pSM_ControllerRetransmissionsStats->totalReceivedRetransmissions, percent);
      }
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "   Last 500 ms:");
      strcpy(szBuff, "N/A");
      percent = 0;
      if ( NULL != g_pPHRTE_Retransmissions && NULL != pSM_ControllerRetransmissionsStats )
      {
         if ( pSM_ControllerRetransmissionsStats->totalRequestedRetransmissionsLast500Ms > 0 )
            percent = pSM_ControllerRetransmissionsStats->totalReceivedRetransmissionsLast500Ms*100/pSM_ControllerRetransmissionsStats->totalRequestedRetransmissionsLast500Ms;
         sprintf(szBuff, "%u / %u / %u %d%%", pSM_ControllerRetransmissionsStats->totalRequestedRetransmissionsLast500Ms, g_pPHRTE_Retransmissions->totalReceivedRetransmissionsRequestsUniqueLast5Sec, pSM_ControllerRetransmissionsStats->totalReceivedRetransmissionsLast500Ms, percent);
      }
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "   Segments:");
      strcpy(szBuff, "N/A");
      percent = 0;

      if ( NULL != g_pPHRTE_Retransmissions && NULL != pSM_ControllerRetransmissionsStats )
      {
         if ( 0 != pSM_ControllerRetransmissionsStats->totalRequestedSegments )
            percent = pSM_ControllerRetransmissionsStats->totalReceivedSegments*100/pSM_ControllerRetransmissionsStats->totalRequestedSegments;

         sprintf(szBuff, "%u / %u / %u %d%%", pSM_ControllerRetransmissionsStats->totalRequestedSegments, g_pPHRTE_Retransmissions->totalReceivedRetransmissionsRequestsSegmentsUnique, pSM_ControllerRetransmissionsStats->totalReceivedSegments, percent );
      }

      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;

      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Retr Time (min/avg/max) ms:");
      
      u8 uMin = 0xFF;
      u8 uMax = 0;
      u32 uAverage = 0;
      u32 uCountValues = 0;
      for( int i=0; i<MAX_HISTORY_VIDEO_INTERVALS;i++ )
      {
         if ( pSM_ControllerRetransmissionsStats->history[i].uAverageRetransmissionRoundtripTime == 0 )
            continue;
         if ( pSM_ControllerRetransmissionsStats->history[i].uCountReceivedRetransmissionPackets == 0 )
            continue;
         uCountValues++;
         uAverage += pSM_ControllerRetransmissionsStats->history[i].uAverageRetransmissionRoundtripTime;

         if ( pSM_ControllerRetransmissionsStats->history[i].uMinRetransmissionRoundtripTime < uMin )
            uMin = pSM_ControllerRetransmissionsStats->history[i].uMinRetransmissionRoundtripTime;
         if ( pSM_ControllerRetransmissionsStats->history[i].uMaxRetransmissionRoundtripTime > uMax )
            uMax = pSM_ControllerRetransmissionsStats->history[i].uMaxRetransmissionRoundtripTime;
      }
      if ( uCountValues > 0 )
         uAverage = uAverage/uCountValues;
      if ( uMin == 0xFF )
         sprintf(szBuff, "0/0/0");
      else
         sprintf(szBuff, "%d/%d/%d", uMin, uAverage, uMax);
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
      
      osd_set_colors();
   }

   if ( ! bIsSnapshot )
   {
      g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStats, "Discarded buff/seg/pack:");
      sprintf(szBuff, "%u/%u/%u", pSM_VideoStats->total_DiscardedBuffers, pSM_VideoStats->total_DiscardedSegments, pSM_VideoStats->total_DiscardedLostPackets);
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
      y += height_text*s_OSDStatsLineSpacing;
   }
   else
   {
      /*
      for( int i=0; i<SNAPSHOT_HISTORY_SIZE; i++ )
      {
         if ( 0 == i )
            sprintf(szBuff, "Discared buff/seg/pack:");
         else
            sprintf(szBuff, "Prev Discared b/s/p:");

         g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStats, szBuff);
         sprintf(szBuff, "%u/%u/%u", s_uOSDSnapshotHistoryDiscardedBuffers[i], s_uOSDSnapshotHistoryDiscardedSegments[i], s_uOSDSnapshotHistoryDiscardedPackets[i]);
         g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
         y += height_text*s_OSDStatsLineSpacing;
      }
      */
      g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStats, "Press [Cancel] to dismiss");
      y += height_text*s_OSDStatsLineSpacing;
   }

   return height;
}

float osd_render_stats_rc_get_height(float scale)
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;
   float height = 2.0 *s_fOSDStatsMargin*scale*1.1 + 0.7*height_text*s_OSDStatsLineSpacing;

   height += 0.05*scale;
   height += 4*height_text*s_OSDStatsLineSpacing;
   height += 5*height_text*s_OSDStatsLineSpacing;
   return height;
}

float osd_render_stats_rc_get_width(float scale)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

float osd_render_stats_rc(float xPos, float yPos, float scale)
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;
   float height_text_s = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;

   float width = osd_render_stats_rc_get_width(scale);
   float height = osd_render_stats_rc_get_height(scale);

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;
   width -= 2.0*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;
   float wPixel = g_pRenderEngine->getPixelWidth();

   g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, "RC Stats");
   sprintf(szBuff, "%.1f sec", RC_INFO_HISTORY_SIZE*50/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin, yPos, height_text, s_idFontStats, szBuff);
   
   float y = yPos + height_text*1.5*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Active Link:");
   strcpy(szBuff, "Ruby RC");
   if ( ! g_pCurrentModel->rc_params.rc_enabled )
      strcpy(szBuff, "External");
   if ( NULL != g_pCurrentModel && ((g_pCurrentModel->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED) == 0 ) )
      strcpy(szBuff, "External");

   g_pRenderEngine->drawTextLeft( rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing + height_text*0.2;

   float yTop = y;

   if ( NULL == g_pCurrentModel || NULL == g_pPHRTE || NULL == g_pPHDownstreamInfoRC )
   {
      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "No info from vehicle"); 
      return height + 0.012;
   }
   if ( NULL == g_pPHDownstreamInfoRC )
   {
      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Not connected"); 
      return height + 0.012;
   }

   // Graph
   float dxGraph = height_text_s*0.6;
   widthMax -= dxGraph;
   float hGraph = 0.03*scale;
   float widthBar = (float)widthMax / (float)RC_INFO_HISTORY_SIZE;
   int maxGap = 0;

   if ( ! g_pCurrentModel->rc_params.rc_enabled )
   {
      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "RC link is disabled");
      y += height_text*s_OSDStatsLineSpacing;
      g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "No RC link graph to show");
      y += hGraph + height_text*0.7 - height_text*s_OSDStatsLineSpacing;
   }
   else
   {
   g_pRenderEngine->drawText(xPos, y-height_text*0.1, height_text*0.8, s_idFontStatsSmall, "4");
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text*0.6, height_text*0.8, s_idFontStatsSmall, "0");

   double* pc = get_Color_OSDText();
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);

   if ( g_pPHDownstreamInfoRC->is_failsafe )
   {
      g_pRenderEngine->setFill(190,10,0,0.6);
      g_pRenderEngine->setStroke(190,10,0,0.6);
   }

   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph+widthMax, y);
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph+widthMax, y+hGraph);
   for( float i=0; i<widthMax-4.0*wPixel; i+=4.0*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+hGraph/2.0, xPos + dxGraph + i + 2.0*wPixel, y+hGraph/2.0);

   g_pRenderEngine->drawLine(xPos+dxGraph + g_pPHDownstreamInfoRC->last_history_slice*widthBar, y+hGraph-g_pRenderEngine->getPixelHeight(), xPos+dxGraph + g_pPHDownstreamInfoRC->last_history_slice*widthBar, y);
   g_pRenderEngine->drawLine(xPos+dxGraph + g_pPHDownstreamInfoRC->last_history_slice*widthBar+wPixel, y+hGraph-g_pRenderEngine->getPixelHeight(), xPos+dxGraph + g_pPHDownstreamInfoRC->last_history_slice*widthBar+wPixel, y);

   float yBottomGraph = y+hGraph;
   for( int i=0; i<RC_INFO_HISTORY_SIZE; i++ )
   {
      if ( i == g_pPHDownstreamInfoRC->last_history_slice )
         continue;
      u8 val = g_pPHDownstreamInfoRC->history[i];
      u8 cRecv = val & 0x0F;
      u8 cGap = (val >> 4) & 0x0F;
      if ( cGap > maxGap )
         maxGap = cGap;
      if ( cGap > 4 )
         cGap = 4;
      if ( cRecv > 4 )
         cRecv = 4;
      float hBar = hGraph*cRecv/(float)4.0;
      float hGap = hGraph*cGap/(float)4.0;
      if ( cRecv > 0 )
      {
         g_pRenderEngine->setFill(190,190,190,0.7);
         g_pRenderEngine->setStroke(190,190,190,0.7);
         g_pRenderEngine->setStrokeSize(1.0);
         g_pRenderEngine->drawRect(xPos+dxGraph+i*widthBar, yBottomGraph-hBar, widthBar, hBar);
      }
      if ( cGap > 0 )
      {
         g_pRenderEngine->setFill(190,10,0,0.7);
         g_pRenderEngine->setStroke(190,10,0,0.7);
         g_pRenderEngine->setStrokeSize(1.0);
         g_pRenderEngine->drawRect(xPos+dxGraph+i*widthBar, y, widthBar, hGap);
      }
   }

   if ( g_pPHDownstreamInfoRC->is_failsafe )
   {
      g_pRenderEngine->setFill(190,10,0,0.6);
      g_pRenderEngine->setStroke(190,10,0,0.6);
      g_pRenderEngine->setStrokeSize(0);
      g_pRenderEngine->drawText(xPos+height_text*1.2, y+hGraph-height_text*1.5, height_text*0.96, s_idFontStatsSmall, "! FAILSAFE !");
   }

   osd_set_colors();   

   y += hGraph + height_text*0.5;
   }

   osd_set_colors();

   bool bShowMAVLink = false;
   if ( NULL != g_pCurrentModel && ( ! g_pCurrentModel->rc_params.rc_enabled ) )
      bShowMAVLink = true;
   else if ( NULL != g_pCurrentModel && ((g_pCurrentModel->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED) == 0 ) )
      bShowMAVLink = true;
   if ( NULL != g_pPHDownstreamInfoRC && g_pPHDownstreamInfoRC->is_failsafe )   
   if ( NULL != g_pCurrentModel && g_pCurrentModel->rc_params.failsafeFlags == RC_FAILSAFE_NOOUTPUT )
      bShowMAVLink = true;

   sprintf(szBuff, "CH1: %04d", g_pPHDownstreamInfoRC->rc_channels[0]);
   if ( bShowMAVLink && NULL != g_pPHFCRCChannels )
      sprintf(szBuff, "ECH1: %04d", g_pPHFCRCChannels->channels[0]);
   g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStatsSmall, szBuff);

   sprintf(szBuff, "CH5: %04d", g_pPHDownstreamInfoRC->rc_channels[4]);
   if ( bShowMAVLink && NULL != g_pPHFCRCChannels )
      sprintf(szBuff, "ECH5: %04d", g_pPHFCRCChannels->channels[4]);
   g_pRenderEngine->drawTextLeft( rightMargin, y, height_text*0.9, s_idFontStatsSmall, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "CH2: %04d", g_pPHDownstreamInfoRC->rc_channels[1]);
   if ( bShowMAVLink && NULL != g_pPHFCRCChannels )
      sprintf(szBuff, "ECH2: %04d", g_pPHFCRCChannels->channels[1]);
   g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStatsSmall, szBuff);

   sprintf(szBuff, "CH6: %04d", g_pPHDownstreamInfoRC->rc_channels[5]);
   if ( bShowMAVLink && NULL != g_pPHFCRCChannels )
      sprintf(szBuff, "ECH6: %04d", g_pPHFCRCChannels->channels[5]);
   g_pRenderEngine->drawTextLeft( rightMargin, y, height_text*0.9, s_idFontStatsSmall, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "CH3: %04d", g_pPHDownstreamInfoRC->rc_channels[2]);
   if ( bShowMAVLink && NULL != g_pPHFCRCChannels )
      sprintf(szBuff, "ECH3: %04d", g_pPHFCRCChannels->channels[2]);
   g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStatsSmall, szBuff);

   sprintf(szBuff, "CH7: %04d", g_pPHDownstreamInfoRC->rc_channels[6]);
   if ( bShowMAVLink && NULL != g_pPHFCRCChannels )
      sprintf(szBuff, "ECH7: %04d", g_pPHFCRCChannels->channels[6]);
   g_pRenderEngine->drawTextLeft( rightMargin, y, height_text*0.9, s_idFontStatsSmall, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "CH4: %04d", g_pPHDownstreamInfoRC->rc_channels[3]);
   if ( bShowMAVLink && NULL != g_pPHFCRCChannels )
      sprintf(szBuff, "ECH4: %04d", g_pPHFCRCChannels->channels[3]);
   g_pRenderEngine->drawText(xPos, y, height_text*0.9, s_idFontStatsSmall, szBuff);

   sprintf(szBuff, "CH8: %04d", g_pPHDownstreamInfoRC->rc_channels[7]);
   if ( bShowMAVLink && NULL != g_pPHFCRCChannels )
      sprintf(szBuff, "ECH8: %04d", g_pPHFCRCChannels->channels[7]);
   g_pRenderEngine->drawTextLeft( rightMargin, y, height_text*0.9, s_idFontStatsSmall, szBuff);
   y += height_text*s_OSDStatsLineSpacing;
   y += height_text*0.3;

   if ( ! g_pCurrentModel->rc_params.rc_enabled )
      return height;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Recv frames:");
   sprintf(szBuff, "%u", g_pPHDownstreamInfoRC->recv_packets);
   g_pRenderEngine->drawTextLeft( rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Lost frames:");
   sprintf(szBuff, "%u", g_pPHDownstreamInfoRC->lost_packets);
   g_pRenderEngine->drawTextLeft( rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Max gap:");
   sprintf(szBuff, "%d ms", maxGap * 1000 / g_pCurrentModel->rc_params.rc_frames_per_second );
   g_pRenderEngine->drawTextLeft( rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Failsafed count:");
   sprintf(szBuff, "%d", g_pPHDownstreamInfoRC->failsafe_count);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   return height;
}

float osd_render_stats_efficiency_get_height(float scale)
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;
   float height = 2.0 *s_fOSDStatsMargin*scale*1.1 + 0.7*height_text*s_OSDStatsLineSpacing;

   height += 2*height_text*s_OSDStatsLineSpacing;
   return height;
}

float osd_render_stats_efficiency_get_width(float scale)
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

float osd_render_stats_efficiency(float xPos, float yPos, float scale)
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;

   float width = osd_render_stats_efficiency_get_width(scale);
   float height = osd_render_stats_efficiency_get_height(scale);

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;
   width -= 2*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, "Efficiency Stats");
   
   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;
   float yTop = y;

   osd_set_colors();

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "mA/km:");
   float eff = 0.0;
   u32 dist = g_pdpfct->total_distance;
   if ( NULL != g_pCurrentModel && g_pCurrentModel->m_Stats.uCurrentFlightDistance > 500 )
      eff = (float)g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent/10.0/((float)g_pCurrentModel->m_Stats.uCurrentFlightDistance/100.0/1000.0);
   sprintf(szBuff, "%d", (int)eff);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "mA/h:");
   eff = 0;
   if ( NULL != g_pCurrentModel && g_pCurrentModel->m_Stats.uCurrentFlightTime > 0 )
      eff = ( (float)g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent/10.0/((float)g_pCurrentModel->m_Stats.uCurrentFlightTime))*3600.0;
   sprintf(szBuff, "%d", (int)eff);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   return height;
}


float osd_render_stats_video_stream_info_get_height()
{
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height = 2.0 *s_fOSDStatsMargin*1.1 + 1.1*height_text*s_OSDStatsLineSpacing;

   float hGraph = height_text*2.0;

   ControllerSettings* pCS = get_ControllerSettings();

   height += 2*height_text*s_OSDStatsLineSpacing;

   if ( ! pCS->iShowVideoStreamInfoCompact )
   {
    height += 0.3*height_text*s_OSDStatsLineSpacing;
   // kb/frame
   height += hGraph;
   height += height_text;

   // camera source
   height += hGraph;
   height += 3*height_text*s_OSDStatsLineSpacing;

   // radio out
   height += height_text*0.6;
   height += hGraph;
   height += 3*height_text*s_OSDStatsLineSpacing;

   // radio in
   height += height_text*0.6;
   height += hGraph;
   height += 3*height_text*s_OSDStatsLineSpacing;

   // player output
   height += height_text*0.6;
   height += hGraph;
   height += 3*height_text*s_OSDStatsLineSpacing;
   }

   // stats
   height += 2.0 * height_text*s_OSDStatsLineSpacing;
   return height;
}

float osd_render_stats_video_stream_info_get_width()
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth*1.2;
   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAA AAAAA AAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}

float osd_render_stats_video_stream_info(float xPos, float yPos)
{
   ControllerSettings* pCS = get_ControllerSettings();

   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStatsSmall);
   
   float hGraph = height_text*2.0;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float fStroke = OSD_STRIKE_WIDTH;

   float width = osd_render_stats_video_stream_info_get_width();
   float height = osd_render_stats_video_stream_info_get_height();

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*0.7;
   width -= 2*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   if ( NULL != g_psmvds )
      sprintf(szBuff, "Video Stream Info (%u FPS / %d KF)", g_psmvds->fps, g_psmvds->keyframe);
   else
      strcpy(szBuff, "Video Stream Info");
   g_pRenderEngine->drawText(xPos, yPos, height_text, s_idFontStats, szBuff);
   
   float y = yPos + height_text*1.2*s_OSDStatsLineSpacing;
   float yTop = y;

   osd_set_colors();

   int iSlices = 1;
   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width <= 1280 )
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height <= 720 )
      iSlices = g_pCurrentModel->video_params.iH264Slices;

   float dxGraph = g_pRenderEngine->textWidth(0.0, s_idFontStatsSmall, "88 ms");
   float widthGraph = widthMax - dxGraph;
   float widthBar = widthGraph / (float)MAX_FRAMES_SAMPLES;
   

   if ( NULL != g_pSM_VideoInfoStats )
   if ( 0 == g_iDeltaVideoInfoBetweenVehicleController )
   if ( g_pSM_VideoInfoStats->uLastIndex > MAX_FRAMES_SAMPLES/2 )
   {
      g_iDeltaVideoInfoBetweenVehicleController = g_pSM_VideoInfoStats->uLastIndex - g_VideoInfoStatsFromVehicle.uLastIndex; 
   }


   u32 uMaxValue = 0;
   u32 uMinValue = MAX_U32;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
      if ( (g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[i] & 0x7F) < uMinValue )
         uMinValue = g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[i] & 0x7F;
      if ( (g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[i] & 0x7F) > uMaxValue )
         uMaxValue = g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[i] & 0x7F;
   }
   if ( uMinValue == MAX_U32 )
      uMinValue = 0;
   if ( uMaxValue <= uMinValue )
      uMaxValue = uMinValue+4;
   
   uMinValue *= 8;
   uMaxValue *= 8;

   u32 uMaxFrameKb = uMaxValue;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Avg/Max Frame size:");
   sprintf(szBuff, "%d / %u kbits", g_VideoInfoStatsFromVehicle.uAverageFrameSize/1000, uMaxFrameKb);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;
   
   int iKeyframeSizeBitsSum = 0;
   int iKeyframeCount = 0;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
      if ( g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[i] & 0x80 )
      {
         iKeyframeSizeBitsSum += 8*(int)(g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[i] & 0x7F);
         iKeyframeCount++;
      }
   }

   if ( iKeyframeCount > 1 )
      iKeyframeSizeBitsSum /= iKeyframeCount;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Keyframe:");
   sprintf(szBuff, "x%d @ %d kbits/keyfr", g_VideoInfoStatsFromVehicle.uKeyframeInterval, iKeyframeSizeBitsSum);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   if ( ! pCS->iShowVideoStreamInfoCompact )
   {
   //--------------------------------------------------------------------------
   // kb/frame

   sprintf(szBuff, "Camera source (%d FPS)", g_VideoInfoStatsFromVehicle.uAverageFPS/iSlices);
   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, szBuff);
   y += height_text*1.2;

   float yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();

   sprintf(szBuff, "%d kb", (int)uMaxValue);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);
   
   sprintf(szBuff, "%d kb", (int)(uMaxValue+uMinValue)/2);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y+hGraph*0.5-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   
   sprintf(szBuff, "%d kb", (int)uMinValue);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y+hGraph-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->drawLine(xPos, y, xPos + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos, y+hGraph, xPos + widthGraph, y+hGraph);

   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+i, y+0.5*hGraph, xPos + i + 2.0*wPixel, y+0.5*hGraph);
   
   g_pRenderEngine->setStrokeSize(2.1);

   float xBarStart = xPos;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
       int iRealIndex = ((i-g_iDeltaVideoInfoBetweenVehicleController)+2*MAX_FRAMES_SAMPLES)%MAX_FRAMES_SAMPLES;
       u32 uValue = 8*((u32)(g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[iRealIndex] & 0x7F));
       
       if ( iRealIndex != g_VideoInfoStatsFromVehicle.uLastIndex+1 )
       {
          float hBar = hGraph*(float)(uValue-uMinValue)/(float)(uMaxValue - uMinValue);
          
          if ( g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[iRealIndex] & (1<<7) )
          {
             g_pRenderEngine->setStroke(70,70,250,0.96);
             g_pRenderEngine->setFill(70,70,250,0.96);
             g_pRenderEngine->setStrokeSize(2.1); 
          }
          
          g_pRenderEngine->drawRect(xBarStart, y+hGraph-hBar, widthBar, hBar);

          if ( g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[iRealIndex] & (1<<7) )
          {
             osd_set_colors();
             g_pRenderEngine->setStrokeSize(2.1); 
          }
       }
       xBarStart += widthBar;

       if ( iRealIndex == g_VideoInfoStatsFromVehicle.uLastIndex )
       {
          g_pRenderEngine->setStroke(250,250,50,0.9);
          g_pRenderEngine->drawLine(xBarStart, y, xBarStart, y+hGraph);
          osd_set_colors();
          g_pRenderEngine->setStrokeSize(2.1);
       }
   }

   y += hGraph + height_text*0.4;

   y += height_text*0.4;
   

   // ------------------------------------------------------------------------
   // Camera source info

   
   yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
   
   uMaxValue = 0;
   uMinValue = MAX_U32;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
      if ( g_VideoInfoStatsFromVehicle.uFramesTimes[i] < uMinValue )
         uMinValue = g_VideoInfoStatsFromVehicle.uFramesTimes[i];
      if ( g_VideoInfoStatsFromVehicle.uFramesTimes[i] > uMaxValue )
         uMaxValue = g_VideoInfoStatsFromVehicle.uFramesTimes[i];
   }
   if ( uMinValue == MAX_U32 )
      uMinValue = 0;
   if ( uMaxValue <= uMinValue )
      uMaxValue = uMinValue+4;
   
   sprintf(szBuff, "%d ms", (int)uMaxValue);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);
   
   sprintf(szBuff, "%d ms", (int)(uMaxValue+uMinValue)/2);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y+hGraph*0.5-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   
   sprintf(szBuff, "%d ms", (int)uMinValue);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y+hGraph-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->drawLine(xPos, y, xPos + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos, y+hGraph, xPos + widthGraph, y+hGraph);

   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+i, y+0.5*hGraph, xPos + i + 2.0*wPixel, y+0.5*hGraph);
   
   g_pRenderEngine->setStrokeSize(2.1);

   u32 uPrevValue = 0;
   int iIndexPrev = 0;
   xBarStart = xPos;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
       int iRealIndex = ((i-g_iDeltaVideoInfoBetweenVehicleController)+MAX_FRAMES_SAMPLES)%MAX_FRAMES_SAMPLES;
       if ( iRealIndex != g_VideoInfoStatsFromVehicle.uLastIndex+1 )
       {
          float hBar = hGraph*(float)(g_VideoInfoStatsFromVehicle.uFramesTimes[iRealIndex]-uMinValue)/(float)(uMaxValue - uMinValue);
          
          if ( g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[iRealIndex] & (1<<7) )
          {
             g_pRenderEngine->setStroke(70,70,250,0.96);
             g_pRenderEngine->setFill(70,70,250,0.96);
             g_pRenderEngine->setStrokeSize(2.1); 
          }

          g_pRenderEngine->drawLine(xBarStart, y+hGraph-hBar, xBarStart+widthBar, y+hGraph-hBar);

          if ( (i != 0) && (g_VideoInfoStatsFromVehicle.uFramesTimes[iRealIndex] != uPrevValue ) )
          {
             float hBarPrev = hGraph*(float)(uPrevValue-uMinValue)/(float)(uMaxValue - uMinValue);
         
             if ( iIndexPrev >= 0 && iIndexPrev < MAX_FRAMES_SAMPLES )
             if ( g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[iIndexPrev] & (1<<7) )
             {
                g_pRenderEngine->setStroke(70,70,250,0.96);
                g_pRenderEngine->setFill(70,70,250,0.96);
                g_pRenderEngine->setStrokeSize(2.1); 
             }
          
             g_pRenderEngine->drawLine(xBarStart, y+hGraph-hBarPrev, xBarStart, y+hGraph-hBar);
 
             if ( iIndexPrev >= 0 && iIndexPrev < MAX_FRAMES_SAMPLES )
             if ( g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[iIndexPrev] & (1<<7) )
             {
                osd_set_colors();
                g_pRenderEngine->setStrokeSize(2.1); 
             }
          }

          if ( g_VideoInfoStatsFromVehicle.uFramesTypesAndSizes[iRealIndex] & (1<<7) )
          {
             osd_set_colors();
             g_pRenderEngine->setStrokeSize(2.1); 
          }
       }
       uPrevValue = g_VideoInfoStatsFromVehicle.uFramesTimes[iRealIndex];
       iIndexPrev = iRealIndex;
       xBarStart += widthBar;

       if ( iRealIndex == g_VideoInfoStatsFromVehicle.uLastIndex )
       {
          g_pRenderEngine->setStroke(250,250,50,0.9);
          g_pRenderEngine->drawLine(xBarStart, y, xBarStart, y+hGraph);
          osd_set_colors();
          g_pRenderEngine->setStrokeSize(2.1);
       }
   }

   y += hGraph + height_text*0.4;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStatsSmall, "Avg/Min/Max frame:");
   sprintf(szBuff, "%d / %d / %d ms", g_VideoInfoStatsFromVehicle.uAverageFrameTime, uMinValue, uMaxValue);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStatsSmall, szBuff);
   y += height_text_small*s_OSDStatsLineSpacing;
   
   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Max deviation from avg:");
   strcpy(szBuff, "N/A");
   sprintf(szBuff, "%d ms", g_VideoInfoStatsFromVehicle.uMaxFrameDeltaTime);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   y += height_text*0.4;
   
   // -----------------------------------------------------------
   // Radio output info

   sprintf(szBuff, "Radio output (%d FPS)", g_VideoInfoStatsFromVehicleRadioOut.uAverageFPS/iSlices);
   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, szBuff);
   y += height_text*1.2;

   yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
   
   uMaxValue = 0;
   uMinValue = MAX_U32;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
      if ( g_VideoInfoStatsFromVehicleRadioOut.uFramesTimes[i] < uMinValue )
         uMinValue = g_VideoInfoStatsFromVehicleRadioOut.uFramesTimes[i];
      if ( g_VideoInfoStatsFromVehicleRadioOut.uFramesTimes[i] > uMaxValue )
         uMaxValue = g_VideoInfoStatsFromVehicleRadioOut.uFramesTimes[i];
   }
   if ( uMinValue == MAX_U32 )
      uMinValue = 0;
   if ( uMaxValue <= uMinValue )
      uMaxValue = uMinValue+4;
   

   sprintf(szBuff, "%d ms", (int)uMaxValue);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);
   
   sprintf(szBuff, "%d ms", (int)(uMaxValue+uMinValue)/2);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y+hGraph*0.5-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   
   sprintf(szBuff, "%d ms", (int)uMinValue);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y+hGraph-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->drawLine(xPos, y, xPos + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos, y+hGraph, xPos +widthGraph, y+hGraph);

   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+i, y+0.5*hGraph, xPos + i + 2.0*wPixel, y+0.5*hGraph);
   
   g_pRenderEngine->setStrokeSize(2.1);

   uPrevValue = 0;
   iIndexPrev = 0;
   xBarStart = xPos;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
       int iRealIndex = ((i-g_iDeltaVideoInfoBetweenVehicleController)+MAX_FRAMES_SAMPLES)%MAX_FRAMES_SAMPLES;
       if ( iRealIndex != g_VideoInfoStatsFromVehicleRadioOut.uLastIndex+1 )
       {
          float hBar = hGraph*(float)(g_VideoInfoStatsFromVehicleRadioOut.uFramesTimes[iRealIndex]-uMinValue)/(float)(uMaxValue - uMinValue);
          
          if ( g_VideoInfoStatsFromVehicleRadioOut.uFramesTypesAndSizes[iRealIndex] & (1<<7) )
          {
             g_pRenderEngine->setStroke(70,70,250,0.96);
             g_pRenderEngine->setFill(70,70,250,0.96);
             g_pRenderEngine->setStrokeSize(2.1); 
          }

          g_pRenderEngine->drawLine(xBarStart, y+hGraph-hBar, xBarStart+widthBar, y+hGraph-hBar);

          if ( (i != 0) && (g_VideoInfoStatsFromVehicleRadioOut.uFramesTimes[iRealIndex] != uPrevValue ) )
          {
             float hBarPrev = hGraph*(float)(uPrevValue-uMinValue)/(float)(uMaxValue - uMinValue);
         
             if ( iIndexPrev >= 0 && iIndexPrev < MAX_FRAMES_SAMPLES )
             if ( g_VideoInfoStatsFromVehicleRadioOut.uFramesTypesAndSizes[iIndexPrev] & (1<<7) )
             {
                g_pRenderEngine->setStroke(70,70,250,0.96);
                g_pRenderEngine->setFill(70,70,250,0.96);
                g_pRenderEngine->setStrokeSize(2.1); 
             }
          
             g_pRenderEngine->drawLine(xBarStart, y+hGraph-hBarPrev, xBarStart, y+hGraph-hBar);
 
             if ( iIndexPrev >= 0 && iIndexPrev < MAX_FRAMES_SAMPLES )
             if ( g_VideoInfoStatsFromVehicleRadioOut.uFramesTypesAndSizes[iIndexPrev] & (1<<7) )
             {
                osd_set_colors();
                g_pRenderEngine->setStrokeSize(2.1); 
             }
          }

          if ( g_VideoInfoStatsFromVehicleRadioOut.uFramesTypesAndSizes[iRealIndex] & (1<<7) )
          {
             osd_set_colors();
             g_pRenderEngine->setStrokeSize(2.1); 
          }
       }
       uPrevValue = g_VideoInfoStatsFromVehicleRadioOut.uFramesTimes[iRealIndex];
       iIndexPrev = iRealIndex;
       xBarStart += widthBar;

       if ( iRealIndex == g_VideoInfoStatsFromVehicleRadioOut.uLastIndex )
       {
          g_pRenderEngine->setStroke(250,250,50,0.9);
          g_pRenderEngine->drawLine(xBarStart, y, xBarStart, y+hGraph);
          osd_set_colors();
          g_pRenderEngine->setStrokeSize(2.1);
       }
   }

   y += hGraph + height_text*0.4;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStatsSmall, "Avg/Min/Max frame:");
   strcpy(szBuff, "N/A");
   sprintf(szBuff, "%d / %d / %d ms", g_VideoInfoStatsFromVehicleRadioOut.uAverageFrameTime, uMinValue, uMaxValue);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStatsSmall, szBuff);
   y += height_text_small*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Max deviation from avg:");
   strcpy(szBuff, "N/A");
   sprintf(szBuff, "%d ms", g_VideoInfoStatsFromVehicleRadioOut.uMaxFrameDeltaTime);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;
   
   y += height_text*0.4;

   // -----------------------------------------------------------
   // Radio In graph

   sprintf(szBuff, "Radio In (%d FPS)", g_pSM_VideoInfoStatsRadioIn->uAverageFPS/iSlices);
   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, szBuff);
   y += height_text*1.2;

   if ( NULL == g_pSM_VideoInfoStatsRadioIn )
   {
      g_pRenderEngine->drawText(xPos, y, 0.0, s_idFontStats, "No Info.");
      return yTop;
   }

   yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
   
   uMaxValue = 0;
   uMinValue = MAX_U32;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
      if ( g_pSM_VideoInfoStatsRadioIn->uFramesTimes[i] < uMinValue )
         uMinValue = g_pSM_VideoInfoStatsRadioIn->uFramesTimes[i];
      if ( g_pSM_VideoInfoStatsRadioIn->uFramesTimes[i] > uMaxValue )
         uMaxValue = g_pSM_VideoInfoStatsRadioIn->uFramesTimes[i];
   }
   if ( uMinValue == MAX_U32 )
      uMinValue = 0;
   if ( uMaxValue <= uMinValue )
      uMaxValue = uMinValue+4;
   
   sprintf(szBuff, "%d ms", (int)uMaxValue);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);
   
   sprintf(szBuff, "%d ms", (int)(uMaxValue+uMinValue)/2);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y+hGraph*0.5-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   
   sprintf(szBuff, "%d ms", (int)uMinValue);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y+hGraph-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->drawLine(xPos, y, xPos + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos, y+hGraph, xPos +widthGraph, y+hGraph);

   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+i, y+0.5*hGraph, xPos + i + 2.0*wPixel, y+0.5*hGraph);
   
   g_pRenderEngine->setStrokeSize(2.1);

   uPrevValue = 0;
   iIndexPrev = 0;
   xBarStart = xPos;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
       if ( i != g_pSM_VideoInfoStatsRadioIn->uLastIndex+1 )
       {
          float hBar = hGraph*(float)(g_pSM_VideoInfoStatsRadioIn->uFramesTimes[i]-uMinValue)/(float)(uMaxValue - uMinValue);
          
          if ( g_pSM_VideoInfoStatsRadioIn->uFramesTypesAndSizes[i] & (1<<7) )
          {
             g_pRenderEngine->setStroke(70,70,250,0.96);
             g_pRenderEngine->setFill(70,70,250,0.96);
             g_pRenderEngine->setStrokeSize(2.1); 
          }

          g_pRenderEngine->drawLine(xBarStart, y+hGraph-hBar, xBarStart+widthBar, y+hGraph-hBar);
          
          
          if ( (i != 0) && (g_pSM_VideoInfoStatsRadioIn->uFramesTimes[i] != uPrevValue ) )
          {
             float hBarPrev = hGraph*(float)(uPrevValue-uMinValue)/(float)(uMaxValue - uMinValue);

             if ( iIndexPrev >= 0 && iIndexPrev < MAX_FRAMES_SAMPLES )
             if ( g_pSM_VideoInfoStatsRadioIn->uFramesTypesAndSizes[iIndexPrev] & (1<<7) )
             {
                g_pRenderEngine->setStroke(70,70,250,0.96);
                g_pRenderEngine->setFill(70,70,250,0.96);
                g_pRenderEngine->setStrokeSize(2.1); 
             }

             g_pRenderEngine->drawLine(xBarStart, y+hGraph-hBarPrev, xBarStart, y+hGraph-hBar);
          
             if ( iIndexPrev >= 0 && iIndexPrev < MAX_FRAMES_SAMPLES )
             if ( g_pSM_VideoInfoStatsRadioIn->uFramesTypesAndSizes[iIndexPrev] & (1<<7) )
             {
                osd_set_colors();
                g_pRenderEngine->setStrokeSize(2.1); 
             }
          }

          if ( g_pSM_VideoInfoStatsRadioIn->uFramesTypesAndSizes[i] & (1<<7) )
          {
             osd_set_colors();
             g_pRenderEngine->setStrokeSize(2.1); 
          }
       }

       uPrevValue = g_pSM_VideoInfoStatsRadioIn->uFramesTimes[i];
       iIndexPrev = i;
       xBarStart += widthBar;

       if ( i == g_pSM_VideoInfoStatsRadioIn->uLastIndex )
       {
          g_pRenderEngine->setStroke(250,250,50,0.9);
          g_pRenderEngine->drawLine(xBarStart, y, xBarStart, y+hGraph);
          osd_set_colors();
          g_pRenderEngine->setStrokeSize(2.1);
       }
   }

   y += hGraph + height_text*0.4;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStatsSmall, "Avg/Min/Max frame:");
   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_VideoInfoStatsRadioIn )
      sprintf(szBuff, "%d / %d / %d ms", g_pSM_VideoInfoStatsRadioIn->uAverageFrameTime, uMinValue, uMaxValue);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStatsSmall, szBuff);
   y += height_text_small*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Max deviation from avg:");
   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_VideoInfoStatsRadioIn )
      sprintf(szBuff, "%d ms", g_pSM_VideoInfoStatsRadioIn->uMaxFrameDeltaTime);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   // -----------------------------------------------------------
   // Player output info

   sprintf(szBuff, "Player output (%d FPS)", g_pSM_VideoInfoStats->uAverageFPS/iSlices);
   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, szBuff);
   y += height_text*1.2;

   if ( NULL == g_pSM_VideoInfoStats )
   {
      g_pRenderEngine->drawText(xPos, y, 0.0, s_idFontStats, "No Info.");
      return yTop;
   }

   yBottomGraph = y + hGraph;
   yBottomGraph = ((int)(yBottomGraph/g_pRenderEngine->getPixelHeight())) * g_pRenderEngine->getPixelHeight();
   
   uMaxValue = 0;
   uMinValue = MAX_U32;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
      if ( g_pSM_VideoInfoStats->uFramesTimes[i] < uMinValue )
         uMinValue = g_pSM_VideoInfoStats->uFramesTimes[i];
      if ( g_pSM_VideoInfoStats->uFramesTimes[i] > uMaxValue )
         uMaxValue = g_pSM_VideoInfoStats->uFramesTimes[i];
   }
   if ( uMinValue == MAX_U32 )
      uMinValue = 0;
   if ( uMaxValue <= uMinValue )
      uMaxValue = uMinValue+4;
   
   sprintf(szBuff, "%d ms", (int)uMaxValue);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y-height_text_small*0.6, height_text_small, s_idFontStatsSmall, szBuff);
   
   sprintf(szBuff, "%d ms", (int)(uMaxValue+uMinValue)/2);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y+hGraph*0.5-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   
   sprintf(szBuff, "%d ms", (int)uMinValue);
   g_pRenderEngine->drawText(xPos+widthGraph+4.0*wPixel, y+hGraph-height_text_small*0.6, height_text_small, s_idFontStatsSmall,szBuff);
   
   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->drawLine(xPos, y, xPos + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos, y+hGraph, xPos +widthGraph, y+hGraph);

   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+i, y+0.5*hGraph, xPos + i + 2.0*wPixel, y+0.5*hGraph);
   
   g_pRenderEngine->setStrokeSize(2.1);

   uPrevValue = 0;
   iIndexPrev = 0;
   xBarStart = xPos;
   for( int i=0; i<MAX_FRAMES_SAMPLES; i++ )
   {
       if ( i != g_pSM_VideoInfoStats->uLastIndex+1 )
       {
          float hBar = hGraph*(float)(g_pSM_VideoInfoStats->uFramesTimes[i]-uMinValue)/(float)(uMaxValue - uMinValue);
          
          if ( g_pSM_VideoInfoStats->uFramesTypesAndSizes[i] & (1<<7) )
          {
             g_pRenderEngine->setStroke(70,70,250,0.96);
             g_pRenderEngine->setFill(70,70,250,0.96);
             g_pRenderEngine->setStrokeSize(2.1); 
          }

          g_pRenderEngine->drawLine(xBarStart, y+hGraph-hBar, xBarStart+widthBar, y+hGraph-hBar);
          
          
          if ( (i != 0) && (g_pSM_VideoInfoStats->uFramesTimes[i] != uPrevValue ) )
          {
             float hBarPrev = hGraph*(float)(uPrevValue-uMinValue)/(float)(uMaxValue - uMinValue);

             if ( iIndexPrev >= 0 && iIndexPrev < MAX_FRAMES_SAMPLES )
             if ( g_pSM_VideoInfoStats->uFramesTypesAndSizes[iIndexPrev] & (1<<7) )
             {
                g_pRenderEngine->setStroke(70,70,250,0.96);
                g_pRenderEngine->setFill(70,70,250,0.96);
                g_pRenderEngine->setStrokeSize(2.1); 
             }

             g_pRenderEngine->drawLine(xBarStart, y+hGraph-hBarPrev, xBarStart, y+hGraph-hBar);
          
             if ( iIndexPrev >= 0 && iIndexPrev < MAX_FRAMES_SAMPLES )
             if ( g_pSM_VideoInfoStats->uFramesTypesAndSizes[iIndexPrev] & (1<<7) )
             {
                osd_set_colors();
                g_pRenderEngine->setStrokeSize(2.1); 
             }
          }

          if ( g_pSM_VideoInfoStats->uFramesTypesAndSizes[i] & (1<<7) )
          {
             osd_set_colors();
             g_pRenderEngine->setStrokeSize(2.1); 
          }
       }

       uPrevValue = g_pSM_VideoInfoStats->uFramesTimes[i];
       iIndexPrev = i;
       xBarStart += widthBar;

       if ( i == g_pSM_VideoInfoStats->uLastIndex )
       {
          g_pRenderEngine->setStroke(250,250,50,0.9);
          g_pRenderEngine->drawLine(xBarStart, y, xBarStart, y+hGraph);
          osd_set_colors();
          g_pRenderEngine->setStrokeSize(2.1);
       }
   }

   y += hGraph + height_text*0.4;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStatsSmall, "Avg/Min/Max frame:");
   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_VideoInfoStats )
      sprintf(szBuff, "%d / %d / %d ms", g_pSM_VideoInfoStats->uAverageFrameTime, uMinValue, uMaxValue);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStatsSmall, szBuff);
   y += height_text_small*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Max deviation from avg:");
   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_VideoInfoStats )
      sprintf(szBuff, "%d ms", g_pSM_VideoInfoStats->uMaxFrameDeltaTime);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   }
   
   // -------------------------
   // Stats

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Max dev cam/tx/rx now:");
   if ( NULL != g_pSM_VideoInfoStats && NULL != g_pSM_VideoInfoStatsRadioIn)
      sprintf(szBuff, "%u / %u / %d / %d ms", g_VideoInfoStatsFromVehicle.uMaxFrameDeltaTime, g_VideoInfoStatsFromVehicleRadioOut.uMaxFrameDeltaTime, g_pSM_VideoInfoStatsRadioIn->uMaxFrameDeltaTime, g_pSM_VideoInfoStats->uMaxFrameDeltaTime );
   else
      sprintf(szBuff, "%u / %u / N/A / N/A ms", g_VideoInfoStatsFromVehicle.uMaxFrameDeltaTime, g_VideoInfoStatsFromVehicleRadioOut.uMaxFrameDeltaTime );
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   if ( g_TimeNow > pairing_getStartTime() + 6000 )
   {
      if ( g_VideoInfoStatsFromVehicle.uMaxFrameDeltaTime > s_uOSDMaxFrameDeviationCamera )
         s_uOSDMaxFrameDeviationCamera = g_VideoInfoStatsFromVehicle.uMaxFrameDeltaTime;
      if ( g_VideoInfoStatsFromVehicleRadioOut.uMaxFrameDeltaTime > s_uOSDMaxFrameDeviationTx )
         s_uOSDMaxFrameDeviationTx = g_VideoInfoStatsFromVehicleRadioOut.uMaxFrameDeltaTime;
      
      if ( NULL != g_pSM_VideoInfoStatsRadioIn )
      if ( g_pSM_VideoInfoStatsRadioIn->uMaxFrameDeltaTime > s_uOSDMaxFrameDeviationRx )
         s_uOSDMaxFrameDeviationRx = g_pSM_VideoInfoStatsRadioIn->uMaxFrameDeltaTime;

      if ( NULL != g_pSM_VideoInfoStats )
      if ( g_pSM_VideoInfoStats->uMaxFrameDeltaTime > s_uOSDMaxFrameDeviationPlayer )
         s_uOSDMaxFrameDeviationPlayer = g_pSM_VideoInfoStats->uMaxFrameDeltaTime;
   }

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Max dev cam/tx/rx all:");
   if ( NULL != g_pSM_VideoInfoStats && NULL != g_pSM_VideoInfoStatsRadioIn )
      sprintf(szBuff, "%u / %u / %d / %d ms", s_uOSDMaxFrameDeviationCamera, s_uOSDMaxFrameDeviationTx, s_uOSDMaxFrameDeviationRx, s_uOSDMaxFrameDeviationPlayer );
   else
      sprintf(szBuff, "%u / %u / N/A / N/A ms", s_uOSDMaxFrameDeviationCamera, s_uOSDMaxFrameDeviationTx );
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, s_idFontStats, szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   return height;
}


float osd_render_stats_flight_end(float scale)
{
   u32 fontId = g_idFontOSD;
   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;
   Preferences* pP = get_Preferences();

   //float width = 0.38 * scale;
   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAA AAAAAA AAA AAAAAAAA");

   float lineHeight = height_text*s_OSDStatsLineSpacing*1.4;
   float height = 4.0 *s_fOSDStatsMargin*scale*1.1 + lineHeight;
   height += 12.6*lineHeight;
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      height += lineHeight;

   float iconHeight = 2.2*lineHeight;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();
   float xPos = (1.0-width)/2.0;
   float yPos = (1.0-height)/2.0;
   char szBuff[512];

   float fAlpha = g_pRenderEngine->setGlobalAlfa(0.9);

   float alfa = 0.9;
   if ( pP->iInvertColorsOSD )
      alfa = 0.8;
   double pc[4];
   pc[0] = get_Color_OSDBackground()[0];
   pc[1] = get_Color_OSDBackground()[1];
   pc[2] = get_Color_OSDBackground()[2];
   pc[3] = 0.9;
   g_pRenderEngine->setColors(pc, alfa);

   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN*scale);
   osd_set_colors();

   xPos += 2*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += 2*s_fOSDStatsMargin*scale*0.7;
   width -= 4*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();

   float widthMax = width;
   float rightMargin = xPos + width;

   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      g_pRenderEngine->drawText(xPos, yPos, height_text, fontId, "Post Flight Statistics");
   else
      g_pRenderEngine->drawText(xPos, yPos, height_text, fontId, "Post Run Statistics");

   if ( iconWidth > 0.001 )
   {
      u32 idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );
      g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
      g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
      g_pRenderEngine->drawIcon(xPos+width - iconWidth - 0.9*POPUP_ROUND_MARGIN*scale/g_pRenderEngine->getAspectRatio() , yPos-height_text*0.1, iconWidth, iconHeight, idIcon);
   }
   float y = yPos + lineHeight;

   osd_set_colors();

   if ( NULL == g_pCurrentModel || NULL == g_pdpfct )
   {
      g_pRenderEngine->setGlobalAlfa(fAlpha);
      return height;
   }

   strcpy(szBuff, g_pCurrentModel->getLongName());
   szBuff[0] = toupper(szBuff[0]);
   g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);
   y += lineHeight*1.9;

   g_pRenderEngine->drawLine(xPos, y-lineHeight*0.6, xPos+width, y-lineHeight*0.6);
   
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      sprintf(szBuff, "Flight No. %d", (int)g_pCurrentModel->m_Stats.uTotalFlights);
   else
      sprintf(szBuff, "Run No. %d", (int)g_pCurrentModel->m_Stats.uTotalFlights);

   g_pRenderEngine->drawText(xPos, y, height_text, fontId, szBuff);
   y += lineHeight;

   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Flight time:");
   else
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Run time:");
   if ( g_pCurrentModel->m_Stats.uCurrentFlightTime >= 3600 )
      sprintf(szBuff, "%dh:%02dm:%02ds", g_pCurrentModel->m_Stats.uCurrentFlightTime/3600, (g_pCurrentModel->m_Stats.uCurrentFlightTime/60)%60, (g_pCurrentModel->m_Stats.uCurrentFlightTime)%60);
   else
      sprintf(szBuff, "%02dm:%02ds", (g_pCurrentModel->m_Stats.uCurrentFlightTime/60)%60, (g_pCurrentModel->m_Stats.uCurrentFlightTime)%60);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
   y += lineHeight;

   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Total flight distance:");
   else
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Total traveled distance:");

   if ( pP->iUnits == prefUnitsImperial )
   {
      if ( NULL != g_pdpfct )
         sprintf(szBuff, "%.1f mi", _osd_convertKm(g_pCurrentModel->m_Stats.uCurrentFlightDistance/100)/1000.0);
      else
         sprintf(szBuff, "0.0 mi");
   }
   else
   {
      if ( NULL != g_pdpfct )
         sprintf(szBuff, "%.1f km", (g_pCurrentModel->m_Stats.uCurrentFlightDistance/100)/1000.0);
      else
         sprintf(szBuff, "0.0 km");
   }
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
   y += lineHeight;

   g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Total current:");
   sprintf(szBuff, "%d mA", (int)(g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent/10));
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
   y += lineHeight;

   g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Max Distance:");
   if ( g_pCurrentModel->m_Stats.uCurrentMaxDistance < 1500 )
   {
      if ( pP->iUnits == prefUnitsImperial )
         sprintf(szBuff, "%d ft", (int)_osd_convertMeters(g_pCurrentModel->m_Stats.uCurrentMaxDistance));
      else
         sprintf(szBuff, "%d m", (int)_osd_convertMeters(g_pCurrentModel->m_Stats.uCurrentMaxDistance));
   }
   else
   {
      if ( pP->iUnits == prefUnitsImperial )
         sprintf(szBuff, "%.1f mi", _osd_convertKm(g_pCurrentModel->m_Stats.uCurrentMaxDistance/1000.0));
      else
         sprintf(szBuff, "%.1f km", _osd_convertKm(g_pCurrentModel->m_Stats.uCurrentMaxDistance/1000.0));
   }
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
   y += lineHeight;

   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
   {
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Max Altitude:");
      if ( pP->iUnits == prefUnitsImperial )
         sprintf(szBuff, "%d ft", (int)_osd_convertMeters(g_pCurrentModel->m_Stats.uCurrentMaxAltitude));
      else
         sprintf(szBuff, "%d m", (int)_osd_convertMeters(g_pCurrentModel->m_Stats.uCurrentMaxAltitude));
      g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
      y += lineHeight;
   }

   g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Max Current:");
   sprintf(szBuff, "%.1f A", (float)g_pCurrentModel->m_Stats.uCurrentMaxCurrent/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
   y += lineHeight;

   g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Min Voltage:");
   sprintf(szBuff, "%.1f V", (float)g_pCurrentModel->m_Stats.uCurrentMinVoltage/1000.0);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
   y += lineHeight;

   if ( pP->iUnits == prefUnitsImperial )
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Efficiency mA/mi:");
   else
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Efficiency mA/km:");
   float eff = 0.0;
   if ( g_pCurrentModel->m_Stats.uCurrentFlightDistance > 500 )
   {
      float fv = (float)_osd_convertKm((float)g_pCurrentModel->m_Stats.uCurrentFlightDistance/100.0/1000.0);
      if ( fv > 0.0000001 )
         eff = g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent/10.0/fv;
      sprintf(szBuff, "%d", (int)eff);
   }
   else
      sprintf(szBuff, "Too Short");
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
   y += lineHeight;

   g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Efficiency mA/h:");
   eff = 0;
   if ( g_pCurrentModel->m_Stats.uCurrentFlightTime > 0 )
      eff = ((float)g_pCurrentModel->m_Stats.uCurrentTotalCurrent/10.0/((float)g_pCurrentModel->m_Stats.uCurrentFlightTime))*3600.0;
   sprintf(szBuff, "%d", (int)eff);
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
   y += lineHeight;

   y += lineHeight*0.5;
   g_pRenderEngine->drawLine(xPos, y-3.0*g_pRenderEngine->getPixelHeight(), xPos+width, y-3.0*g_pRenderEngine->getPixelHeight());

   g_pRenderEngine->drawText(xPos, y, height_text, fontId, "Odometer:");
   if ( pP->iUnits == prefUnitsImperial )
      sprintf(szBuff, "%.1f Mi", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
   else
      sprintf(szBuff, "%.1f Km", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
   y += lineHeight;

   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
        g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "All time flight time:");
   else
      g_pRenderEngine->drawText(xPos, y, height_text, fontId, "All time drive time:");

   if ( g_pCurrentModel->m_Stats.uTotalFlightTime >= 3600 )
      sprintf(szBuff, "%dh:%02dm:%02ds", g_pCurrentModel->m_Stats.uTotalFlightTime/3600, (g_pCurrentModel->m_Stats.uTotalFlightTime/60)%60, (g_pCurrentModel->m_Stats.uTotalFlightTime)%60);
   else
      sprintf(szBuff, "%02dm:%02ds", (g_pCurrentModel->m_Stats.uTotalFlightTime/60)%60, (g_pCurrentModel->m_Stats.uTotalFlightTime)%60);

   g_pRenderEngine->drawTextLeft(rightMargin, y, height_text, fontId, szBuff);
   y += lineHeight;

   g_pRenderEngine->setGlobalAlfa(fAlpha);

   return height;
}

float osd_render_stats_flights(float scale)
{
/*
   Preferences* pP = get_Preferences();
   float text_scale = osd_getFontHeight()*scale;
   float height_text = TextHeight(*render_getFontOSD(), text_scale);

   text_scale *= 0.6;
   height_text *= 0.6;

   float width = toScreenX(0.64) * scale;
   float height = height_text*4.0;

   float xPos = toScreenX(0.2) * scale;
   float yPos = toScreenY(0.18) * scale;
   char szBuff[512];

   float fAlpha = render_get_globalAlfa(); 
   render_set_globalAlfa(0.85);
   float alfa = 0.7;
   if ( pP->iInvertColorsOSD )
      alfa = 0.74;

   render_set_colors(get_Color_OSDBackground(), alfa);

   Roundrect(xPos, yPos, width, height, 1.5*toScreenX(POPUP_ROUND_MARGIN)*scale, 1.5*toScreenX(POPUP_ROUND_MARGIN)*scale);
   osd_set_colors();

   xPos += toScreenX(s_fOSDStatsMargin)*scale;
   yPos = yPos + height - toScreenX(s_fOSDStatsMargin)*scale*0.5;
   width -= 2*toScreenX(s_fOSDStatsMargin)*scale;

   float widthMax = width;
   float rightMargin = xPos + width;

   if ( NULL == g_pCurrentModel )
   {
      render_set_globalAlfa(fAlpha);
      return height;
   }

   sprintf(szBuff, "%s Statistics", g_pCurrentModel->getShortName());
   draw_message(szBuff, xPos, yPos, text_scale*0.98, render_getFontOSD());
   
   yPos -= height_text*1.4;

   osd_set_colors();

   if ( pP->iUnits == prefUnitsImperial )
      sprintf(szBuff, "Total flights: %d,  Total flight time: %dh:%02dm:%02ds,  Total flight distance: %.1f mi",
                       g_pCurrentModel->stats_TotalFlights, g_pCurrentModel->stats_TotalFlightTime/10/3600,
                       (g_pCurrentModel->stats_TotalFlightTime/10/60)%60, (g_pCurrentModel->stats_TotalFlightTime/10)%60,
                       _osd_convertKm(g_pCurrentModel->stats_TotalFlightDistance/10.0/1000.0));
   else
      sprintf(szBuff, "Total flights: %d,  Total flight time: %dh:%02dm:%02ds,  Total flight distance: %.1f km",
              g_pCurrentModel->stats_TotalFlights, g_pCurrentModel->stats_TotalFlightTime/10/3600,
              (g_pCurrentModel->stats_TotalFlightTime/10/60)%60, (g_pCurrentModel->stats_TotalFlightTime/10)%60,
              g_pCurrentModel->stats_TotalFlightDistance/10.0/1000.0);
   draw_message(szBuff, xPos, yPos, text_scale*0.9, render_getFontOSD());

   render_set_globalAlfa(fAlpha);

   return height;
*/
return 0.0;
}

float osd_render_stats_dev_get_height()
{
   Preferences* p = get_Preferences();

   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStats);
   float hGraph = 0.04;
   float hGraphSmall = 0.028;

   float height = 1.6 *s_fOSDStatsMargin*1.3 + 2 * 0.7*height_text*s_OSDStatsLineSpacing;

   if ( p->iDebugShowDevRadioStats )
      height += 12 * height_text * s_OSDStatsLineSpacing;

   if ( p->iDebugShowDevVideoStats )
   {
      height += 1.0*(hGraph + height_text) + 4.0*(hGraphSmall + height_text);
   }
   return height;
}

float osd_render_stats_dev_get_width()
{
   if ( g_fOSDStatsForcePanelWidth > 0.01 )
      return g_fOSDStatsForcePanelWidth;
   float width = g_pRenderEngine->textWidth(0.0, s_idFontStats, "AAAAAAAA AAAAAAAA AAAAAAAA AAA");
   width += 2.0*s_fOSDStatsMargin/g_pRenderEngine->getAspectRatio();
   return width;
}


float osd_render_stats_dev(float xPos, float yPos, float scale)
{
   Preferences* p = get_Preferences();

   float height_text = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;
   float height_text_small = g_pRenderEngine->textHeight( 0.0, s_idFontStats)*scale;
   float hGraphRegular = 0.04*scale;
   float hGraphSmall = 0.028*scale;
   float hGraph = hGraphRegular;

   float width = osd_render_stats_dev_get_width();
   float height = osd_render_stats_dev_get_height();

   char szBuff[128];

   osd_set_colors_background_fill(g_fOSDStatsBgTransparency);
   g_pRenderEngine->drawRoundRect(xPos, yPos, width, height, 1.5*POPUP_ROUND_MARGIN);
   osd_set_colors();

   xPos += s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   yPos += s_fOSDStatsMargin*scale*0.7;
   width -= 2*s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();
   float widthMax = width;
   float rightMargin = xPos + width;

   g_pRenderEngine->setColors(get_Color_Dev());

   g_pRenderEngine->drawText( xPos, yPos, height_text, s_idFontStats, "Developer Mode");
   
   float y = yPos + height_text*1.3*s_OSDStatsLineSpacing;
   float yTop = y;

   u32 linkMin = 900;
   u32 linkMax = 0;

   if ( NULL != g_pSM_RadioStats )
   {
      for( int i=0; i<g_pSM_RadioStats->countRadioLinks; i++ )
      {
         if ( g_pSM_RadioStats->radio_links[i].linkDelayRoundtripMs > linkMax )
            linkMax = g_pSM_RadioStats->radio_links[i].linkDelayRoundtripMs;
         if ( g_pSM_RadioStats->radio_links[i].linkDelayRoundtripMinimMs < linkMin )
            linkMin = g_pSM_RadioStats->radio_links[i].linkDelayRoundtripMinimMs;
      }
   }

   if ( p->iDebugShowDevRadioStats )
   {
   if ( 0 != linkMax )
      sprintf(szBuff, "%d ms", linkMax);
   else
      strcpy(szBuff, "N/A");
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Link RT (max):", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   if ( 0 != linkMin )
      sprintf(szBuff, "%d ms", linkMin);
   else
      strcpy(szBuff, "N/A");
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Link RT (minim):", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   // TO FIX
   /*
   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_ControllerRetransmissionsStats && MAX_U32 != g_pSM_ControllerRetransmissionsStats->retransmissionTimeAverage && MAX_U32 != g_pSM_ControllerRetransmissionsStats->retransmissionTimeMinim )
      sprintf(szBuff, "%d/%d ms", g_pSM_ControllerRetransmissionsStats->retransmissionTimeMinim, g_pSM_ControllerRetransmissionsStats->retransmissionTimeAverage);
   if ( ! (g_pCurrentModel->video_link_profiles[(g_psmvds->video_link_profile & 0x0F)].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS ) )
      strcpy(szBuff, "Disabled");
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Video RT Min/Avg:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_ControllerRetransmissionsStats && MAX_U32 != g_pSM_ControllerRetransmissionsStats->retransmissionTimeLast )
      sprintf(szBuff, "%d ms", g_pSM_ControllerRetransmissionsStats->retransmissionTimeLast);
   if ( ! (g_pCurrentModel->video_link_profiles[(g_psmvds->video_link_profile & 0x0F)].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS ) )
      strcpy(szBuff, "Disabled");
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "Video RT Last:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   g_pRenderEngine->drawText(xPos, y, height_text, s_idFontStats, "Retransmissions:");
   y += height_text*s_OSDStatsLineSpacing;
   float dx = s_fOSDStatsMargin*scale/g_pRenderEngine->getAspectRatio();

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_ControllerRetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_pSM_ControllerRetransmissionsStats->totalCountRequestedRetransmissions[1], ((g_pSM_ControllerRetransmissionsStats->totalCountReceivedVideoPackets[1] + g_pSM_ControllerRetransmissionsStats->totalCountRequestedRetransmissions[1])>0)?g_pSM_ControllerRetransmissionsStats->totalCountRequestedRetransmissions[1]*100/(g_pSM_ControllerRetransmissionsStats->totalCountRequestedRetransmissions[1]+g_pSM_ControllerRetransmissionsStats->totalCountReceivedVideoPackets[1]):0);
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Requested:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_ControllerRetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_pSM_ControllerRetransmissionsStats->totalCountReRequestedPackets[1], (g_pSM_ControllerRetransmissionsStats->totalCountRequestedRetransmissions[1]>0)?(g_pSM_ControllerRetransmissionsStats->totalCountReRequestedPackets[1]*100/g_pSM_ControllerRetransmissionsStats->totalCountRequestedRetransmissions[1]):0);
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Retried:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_ControllerRetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_pSM_ControllerRetransmissionsStats->totalCountReceivedRetransmissions[1], (g_pSM_ControllerRetransmissionsStats->totalCountRequestedRetransmissions[1]>0)?(g_pSM_ControllerRetransmissionsStats->totalCountReceivedRetransmissions[1]*100/g_pSM_ControllerRetransmissionsStats->totalCountRequestedRetransmissions[1]):0);
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Received:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_RetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_pSM_RetransmissionsStats->totalCountReceivedRetransmissionsDuplicated[1], (g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]>0)?(g_pSM_RetransmissionsStats->totalCountReceivedRetransmissionsDuplicated[1]*100/g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]):0);
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Recv Duplicated:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_RetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_pSM_RetransmissionsStats->totalCountReceivedRetransmissionsDiscarded[1], (g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]>0)?(g_pSM_RetransmissionsStats->totalCountReceivedRetransmissionsDiscarded[1]*100/g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]):0 );
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Discarded:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;


   strcpy(szBuff, "N/A");
   if ( NULL != g_pSM_RetransmissionsStats )
      sprintf(szBuff, "%u/sec (%d%%)", g_pSM_RetransmissionsStats->totalCountRecvOriginalAfterRetransmission[1], (g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]>0)?(g_pSM_RetransmissionsStats->totalCountRecvOriginalAfterRetransmission[1]*100/g_pSM_RetransmissionsStats->totalCountReceivedRetransmissions[1]):0 );
   _osd_stats_draw_line(xPos+dx, rightMargin, y, s_idFontStats, "Original recv after retr.:", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   sprintf(szBuff, "N/A");
   if ( NULL != g_pdpfct )
      sprintf(szBuff, "%d", (int)(g_pdpfct->extra_info[6]));
   _osd_stats_draw_line(xPos, rightMargin, y, s_idFontStats, "FC msg/sec", szBuff);
   y += height_text*s_OSDStatsLineSpacing;

   */
   }
   

   osd_set_colors();

   if ( p->iDebugShowDevVideoStats )
   {
   // ------------------------------------------------
   // Retransmissions graphs

   // TO FIX
    /*
   y += (height_text-height_text_small);

   static int s_iOSDStatsDevMaxGraphValue = 0;

   int maxGraphValue = g_pSM_RetransmissionsStats->maxValue;
   if ( maxGraphValue >= s_iOSDStatsDevMaxGraphValue )
   {
      s_iOSDStatsDevMaxGraphValue = maxGraphValue;
   }
   else
   {
      if ( s_iOSDStatsDevMaxGraphValue > 50 )
         s_iOSDStatsDevMaxGraphValue-=5;
      else if ( s_iOSDStatsDevMaxGraphValue > 0 )
         s_iOSDStatsDevMaxGraphValue--;
   }

   int totalHistoryValues = MAX_HISTORY_INTERVALS_RETRANSMISSIONS_STATS;
   float dxGraph = 0.01*scale;
   float widthGraph = widthMax - dxGraph;
   float widthBar = widthGraph / totalHistoryValues;

   g_pRenderEngine->setColors(get_Color_Dev());

   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, "Regular vs Retransmissions");

   y += height_text_small*0.9;

   float yTopGrid = y;

   hGraph = hGraphSmall;

   double* pc = get_Color_OSDText();
   float midLine = hGraph/2.0;
   float wPixel = g_pRenderEngine->getPixelWidth();
   float yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();
   float yTmp;
   float yTmpPrev;


   // Actually received and retransmissions counts

   int iMax = s_iOSDStatsDevMaxGraphValue;
   for( int i=0; i<totalHistoryValues; i++ )
   {
      if ( iMax < g_pSM_RetransmissionsStats->expectedVideoPackets[i] )
         iMax = g_pSM_RetransmissionsStats->expectedVideoPackets[i];
      if ( iMax < g_pSM_RetransmissionsStats->receivedVideoPackets[i] )
         iMax = g_pSM_RetransmissionsStats->receivedVideoPackets[i];
   }
   if ( iMax <= 0 )
      iMax = 1;
   sprintf(szBuff, "%d", iMax);
   g_pRenderEngine->drawText(xPos, y-0.3*height_text_small, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   //sprintf(szBuff, "%d", g_pSM_RetransmissionsStats->expectedVideoPackets[totalHistoryValues-1]);
   sprintf(szBuff, "%d", iMax/2);
   g_pRenderEngine->drawText(xPos, y+0.5*hGraph-0.6*height_text_small, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   //for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
   //   g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   float xBarSt = xPos + dxGraph;
   float xBarCenter = xPos + dxGraph + widthBar*0.5;
   float xBarCenterPrev = xPos + dxGraph - widthBar*0.5;

   int index = g_pSM_RetransmissionsStats->currentSlice;

   for( int i=0; i<totalHistoryValues; i++ )
   {
      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->requestedRetransmissions[index] / (float)iMax;
      g_pRenderEngine->setStroke(50,120,200,0.7);
      g_pRenderEngine->setFill(50,120,200,0.7);
      //g_pRenderEngine->drawLine(xBarSt, yTmp, xBarSt + widthBar, yTmp);
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);

      if ( i != 0 && index != totalHistoryValues-1 )
      {
      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->expectedVideoPackets[index] / (float)iMax;
      yTmpPrev = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->expectedVideoPackets[index+1] / (float)iMax;
      g_pRenderEngine->setStroke(230,230,0,0.75);
      g_pRenderEngine->drawLine(xBarCenter, yTmp, xBarCenterPrev, yTmpPrev);
      g_pRenderEngine->drawLine(xBarCenter, yTmp-g_pRenderEngine->getPixelHeight(), xBarCenterPrev, yTmpPrev-g_pRenderEngine->getPixelHeight());

      yTmp = yBtm - g_pRenderEngine->getPixelHeight() - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->receivedVideoPackets[index] / (float)iMax;
      yTmpPrev = yBtm - g_pRenderEngine->getPixelHeight() - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->receivedVideoPackets[index+1] / (float)iMax;
      g_pRenderEngine->setStroke(20,250,70,0.8);
      g_pRenderEngine->drawLine(xBarCenter, yTmp, xBarCenterPrev, yTmpPrev);
      g_pRenderEngine->drawLine(xBarCenter, yTmp-g_pRenderEngine->getPixelHeight(), xBarCenterPrev, yTmpPrev-g_pRenderEngine->getPixelHeight());
      }
      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }

   y += hGraph;

   // Retransmissions graphs

   g_pRenderEngine->setColors(get_Color_Dev());

   hGraph = hGraphRegular;

   y += (height_text-height_text_small);
   sprintf(szBuff,"%d ms/bar", g_pSM_RetransmissionsStats->refreshInterval);
   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   float w = g_pRenderEngine->textWidth(height_text*0.9, s_idFontStats, szBuff);
   w += 0.02*scale;
   sprintf(szBuff,"%.1f seconds", (((float)totalHistoryValues) * g_pSM_RetransmissionsStats->refreshInterval)/1000.0);
   g_pRenderEngine->drawTextLeft(xPos+widthMax-w, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   w += g_pRenderEngine->textWidth( height_text*0.9, s_idFontStats, szBuff);

   y += height_text_small*0.9;

   sprintf(szBuff, "%d", s_iOSDStatsDevMaxGraphValue);
   g_pRenderEngine->drawText(xPos, y-0.3*height_text_small, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   xBarSt = xPos + dxGraph;
   xBarCenter = xPos + dxGraph + widthBar*0.5;
   xBarCenterPrev = xPos + dxGraph - widthBar*0.5;
   yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();

   index = g_pSM_RetransmissionsStats->currentSlice;

   for( int i=0; i<totalHistoryValues; i++ )
   {
      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->requestedRetransmissions[index] / (float)s_iOSDStatsDevMaxGraphValue;
      g_pRenderEngine->setStroke(50,120,200,0.7);
      g_pRenderEngine->setFill(50,120,200,0.7);
      //g_pRenderEngine->drawLine(xBarSt, yTmp, xBarSt + widthBar, yTmp);
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);

      if ( i != 0 && index != totalHistoryValues-1 )
      {
      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->receivedRetransmissions[index] / (float)s_iOSDStatsDevMaxGraphValue;
      yTmpPrev = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->receivedRetransmissions[index+1] / (float)s_iOSDStatsDevMaxGraphValue;
      g_pRenderEngine->setStroke(20,250,70,0.8);
      g_pRenderEngine->drawLine(xBarCenter, yTmp, xBarCenterPrev, yTmpPrev);
      g_pRenderEngine->drawLine(xBarCenter, yTmp-g_pRenderEngine->getPixelHeight(), xBarCenterPrev, yTmpPrev-g_pRenderEngine->getPixelHeight());

      yTmp = yBtm - g_pRenderEngine->getPixelHeight() - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->ignoredRetransmissions[index] / (float)s_iOSDStatsDevMaxGraphValue;
      yTmpPrev = yBtm - g_pRenderEngine->getPixelHeight() - (hGraph-2*g_pRenderEngine->getPixelHeight())* (float)g_pSM_RetransmissionsStats->ignoredRetransmissions[index+1] / (float)s_iOSDStatsDevMaxGraphValue;
      if ( g_pSM_RetransmissionsStats->ignoredRetransmissions[i] > 0 )
      {
         g_pRenderEngine->setStroke(200,20,0,0.65);
         g_pRenderEngine->setFill(200,20,0,0.65);
         g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);
      }
      //g_pRenderEngine->drawLine(xBarCenter, yTmp, xBarCenterPrev, yTmpPrev);
      //g_pRenderEngine->drawLine(xBarCenter, yTmp+g_pRenderEngine->getPixelHeight(), xBarCenterPrev, yTmpPrev+g_pRenderEngine->getPixelHeight());
      }

      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }

   y += hGraph;

   // Percent retried

   g_pRenderEngine->setColors(get_Color_Dev());

   hGraph = hGraphSmall;

   float fPercentMax = 0;
   int sumReceived = 0;
   int sumIgnored = 0;
   int sumRequested = 0;
   int sumReRequested = 0;
   for( int i=0; i<totalHistoryValues; i++ )
   {
      if ( g_pSM_RetransmissionsStats->requestedRetransmissions[i] > 0 )
      if ( (float)g_pSM_RetransmissionsStats->reRequestedRetransmissions[i]/(float)g_pSM_RetransmissionsStats->requestedRetransmissions[i] > fPercentMax )
         fPercentMax = (float)g_pSM_RetransmissionsStats->reRequestedRetransmissions[i]/(float)g_pSM_RetransmissionsStats->requestedRetransmissions[i];

      sumReceived += g_pSM_RetransmissionsStats->receivedRetransmissions[i];
      sumIgnored += g_pSM_RetransmissionsStats->ignoredRetransmissions[i];
      sumRequested += g_pSM_RetransmissionsStats->requestedRetransmissions[i];
      sumReRequested += g_pSM_RetransmissionsStats->reRequestedRetransmissions[i];
   }
   int average = 0;
   if ( sumRequested > 0 )
      average = sumReRequested*100/sumRequested;

   if ( fPercentMax < 1.0 )
      fPercentMax = 1.0;
   y += (height_text-height_text_small);
   sprintf(szBuff,"Percentage Retried (avg: %d%%)", average);
   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   y += height_text_small*0.9;

   sprintf(szBuff, "%d%%", (int)(fPercentMax*100.0) );
   g_pRenderEngine->drawText(xPos, y-0.2*height_text_small, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0%");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   midLine = hGraph/2.0;
   wPixel = g_pRenderEngine->getPixelWidth();
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   xBarSt = xPos + dxGraph;
   yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();

   index = g_pSM_RetransmissionsStats->currentSlice;

   
   for( int i=0; i<totalHistoryValues; i++ )
   {
      g_pRenderEngine->setStroke(250,250,240, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(250,250,240, s_fOSDStatsGraphLinesAlpha);

      if ( g_pSM_RetransmissionsStats->requestedRetransmissions[i] > 0 && index != totalHistoryValues-1 )
      {
      float percent = (float)g_pSM_RetransmissionsStats->reRequestedRetransmissions[index]/(float)g_pSM_RetransmissionsStats->requestedRetransmissions[index];

      g_pRenderEngine->setStroke(250,250,50, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(250,250,50, s_fOSDStatsGraphLinesAlpha);

      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight()) * percent / fPercentMax;
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);
      }
      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }
   y += hGraph;


   // Percent received

   g_pRenderEngine->setColors(get_Color_Dev());

   hGraph = hGraphSmall;
   y += (height_text-height_text_small);

   average = 0;
   if ( sumRequested > 0 )
      average = sumReceived*100/sumRequested;

   sprintf(szBuff,"Percentage Received (avg %d%%)", average);
   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   y += height_text_small*0.9;

   g_pRenderEngine->drawText(xPos, y-0.2*height_text_small, height_text_small*0.9, s_idFontStatsSmall, "100%");
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0%");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   midLine = hGraph/2.0;
   wPixel = g_pRenderEngine->getPixelWidth();
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   xBarSt = xPos + dxGraph;
   yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();

   index = g_pSM_RetransmissionsStats->currentSlice;
   
   for( int i=0; i<totalHistoryValues; i++ )
   {
      g_pRenderEngine->setStroke(250,250,240, s_fOSDStatsGraphLinesAlpha);
      g_pRenderEngine->setFill(250,250,240, s_fOSDStatsGraphLinesAlpha);

      if ( index != totalHistoryValues-1 )
      {
      float percent = 0.0;
      if ( g_pSM_RetransmissionsStats->requestedRetransmissions[index] > 0 )
         percent = (float)g_pSM_RetransmissionsStats->receivedRetransmissions[index] / (float)g_pSM_RetransmissionsStats->requestedRetransmissions[index];
      if ( percent < 0.0 ) percent = 0.0;
      if ( percent > 1.0 )
      {
         percent = 1.0;
         g_pRenderEngine->setStroke(250,250,50, s_fOSDStatsGraphLinesAlpha);
         g_pRenderEngine->setFill(250,250,50, s_fOSDStatsGraphLinesAlpha);
      }
      yTmp = yBtm - (hGraph-2*g_pRenderEngine->getPixelHeight())* percent;
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);
      }

      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }
   y += hGraph;

   // Percent discarded

   g_pRenderEngine->setColors(get_Color_Dev());

   y += (height_text-height_text_small);

   average = 0;
   if ( sumReceived > 0 )
      average = sumIgnored*100/sumReceived;

   sprintf(szBuff,"Percentage Discarded (avg %d%%)", average);
   g_pRenderEngine->drawTextLeft(xPos+widthMax, y-height_text_small*0.2, height_text_small*0.9, s_idFontStatsSmall, szBuff);
   y += height_text_small*0.9;

   g_pRenderEngine->drawText(xPos, y-0.2*height_text_small, height_text_small*0.9, s_idFontStatsSmall, "100%");
   g_pRenderEngine->drawText(xPos, y+hGraph-height_text_small*0.8, height_text_small*0.9, s_idFontStatsSmall, "0%");

   g_pRenderEngine->setStrokeSize(OSD_STRIKE_WIDTH);
   g_pRenderEngine->setStroke(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(pc[0], pc[1], pc[2], s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->drawLine(xPos+dxGraph, y, xPos + dxGraph + widthGraph, y);         
   g_pRenderEngine->drawLine(xPos+dxGraph, y+hGraph, xPos + dxGraph + widthGraph, y+hGraph);
   midLine = hGraph/2.0;
   wPixel = g_pRenderEngine->getPixelWidth();
   for( float i=0; i<=widthGraph-2.0*wPixel; i+= 5*wPixel )
      g_pRenderEngine->drawLine(xPos+dxGraph+i, y+midLine, xPos + dxGraph + i + 2.0*wPixel, y+midLine);         

   g_pRenderEngine->setStrokeSize(1.0);

   xBarSt = xPos + dxGraph;
   yBtm = y + hGraph - g_pRenderEngine->getPixelHeight();

   index = g_pSM_RetransmissionsStats->currentSlice;
   
   g_pRenderEngine->setStroke(250,250,240, s_fOSDStatsGraphLinesAlpha);
   g_pRenderEngine->setFill(250,250,240, s_fOSDStatsGraphLinesAlpha);

   for( int i=0; i<totalHistoryValues; i++ )
   {
      if ( index != totalHistoryValues-1 )
      {
      float percent = 0.0;
      if ( g_pSM_RetransmissionsStats->receivedRetransmissions[index] > 0 )
         percent = (float)g_pSM_RetransmissionsStats->ignoredRetransmissions[index] / (float)g_pSM_RetransmissionsStats->receivedRetransmissions[index];
      if ( percent < 0.0 ) percent = 0.0;
      if ( percent > 1.0 ) percent = 1.0;
      float fh = (hGraph-2*g_pRenderEngine->getPixelHeight())* percent;
      int tmp = fh/g_pRenderEngine->getPixelHeight();
      fh = g_pRenderEngine->getPixelHeight() * (float)tmp;
      yTmp = yBtm - fh;
      g_pRenderEngine->drawRect(xBarSt, yTmp, widthBar-wPixel, yBtm-yTmp);
      }
      index--;
      if ( index < 0 )
         index = totalHistoryValues-1;
      xBarSt += widthBar;
      xBarCenter += widthBar;
      xBarCenterPrev += widthBar;
   }
   y += hGraph;

   float yBottomGrid = y;

   xBarSt = xPos + dxGraph + widthGraph;


   osd_set_colors();
   g_pRenderEngine->setStrokeSize(1.0);
   g_pRenderEngine->drawLine(xPos+dxGraph + g_pSM_RetransmissionsStats->currentSlice*widthBar+widthBar*0.5, yTopGrid, xPos+dxGraph + g_pSM_RetransmissionsStats->currentSlice*widthBar, yBottomGrid);
   g_pRenderEngine->drawLine(xPos+dxGraph + g_pSM_RetransmissionsStats->currentSlice*widthBar+widthBar*0.5 + wPixel, yTopGrid, xPos+dxGraph + g_pSM_RetransmissionsStats->currentSlice*widthBar+wPixel, yBottomGrid);
  
   g_pRenderEngine->setStroke(250,250,240, s_fOSDStatsGraphLinesAlpha*0.7);
   g_pRenderEngine->setFill(250,250,240, s_fOSDStatsGraphLinesAlpha*0.7);

   for( int i=0; i<totalHistoryValues; i++ )
   {
      xBarSt -= widthBar;
      if ( i%3 )
         continue;

      g_pRenderEngine->drawLine(xBarSt, yTopGrid, xBarSt, yBottomGrid);
   }
*/
   }
   return height;
}

void _osd_render_stats_panels_horizontal()
{
   if ( NULL == g_pCurrentModel )
      return;
   
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   float fStatsSize = 1.0;

   float yMax = 1.0-osd_getMarginY()-osd_getBarHeight()-0.01 - osd_getSecondBarHeight();

   if ( g_pCurrentModel->osd_params.layout == osdLayoutLean )
      yMax -= 0.05; 

   float xWidth = 0.44*fStatsSize;
   float xSpacing = 0.01*fStatsSize;

   float yStats = yMax;
   float xStats = 0.99-osd_getMarginX();

   if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      float fWidth = osd_getVerticalBarWidth();
      xStats -= fWidth;
      yMax = 1.0-osd_getMarginY()-0.01;
      yStats = yMax;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowDevVideoStats || p->iDebugShowDevRadioStats )
   {
      osd_render_stats_dev(xStats, yStats-osd_render_stats_dev_get_height(), fStatsSize);
      xStats -= osd_render_stats_dev_get_width() + xSpacing;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowVehicleVideoGraphs )
   {
      osd_render_stats_video_graphs(xStats, yStats-osd_render_stats_video_graphs_get_height());
      xStats -= osd_render_stats_video_graphs_get_width() + xSpacing;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowVehicleVideoStats )
   {
      osd_render_stats_video_stats(xStats, yStats-osd_render_stats_video_stats_get_height());
      xStats -= osd_render_stats_video_stats_get_width() + xSpacing;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP) )
   {
      osd_render_stats_graphs_vehicle_tx_gap(xStats, yStats-osd_render_stats_graphs_vehicle_tx_gap_get_height());
      xStats -= osd_render_stats_graphs_vehicle_tx_gap_get_width() + xSpacing;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_VIDEO_BITRATE_HISTORY) )
   {
      osd_render_stats_video_bitrate_history(xStats - osd_render_stats_video_bitrate_history_get_width(), yStats-osd_render_stats_video_bitrate_history_get_height());
      xStats -= osd_render_stats_video_bitrate_history_get_width() + xSpacing;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_VIDEO) )
   {
      float hStat = osd_render_stats_video_decode_get_height(pCS->iDeveloperMode, false, g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, fStatsSize);
      osd_render_stats_video_decode(xStats, yStats-hStat, pCS->iDeveloperMode, false, g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, fStatsSize);
      xStats -= osd_render_stats_video_decode_get_width(pCS->iDeveloperMode, false, g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, fStatsSize) + xSpacing;
      if ( p->iDebugShowVideoSnapshotOnDiscard )
      if ( s_uOSDSnapshotTakeTime > 1 && g_TimeNow < s_uOSDSnapshotTakeTime + 15000 )
      {
         hStat = osd_render_stats_video_decode_get_height(pCS->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, &s_OSDSnapshot_VideoDecodeHist, &s_OSDSnapshot_ControllerVideoRetransmissionsStats, fStatsSize);
         osd_render_stats_video_decode(xStats, yStats-hStat, pCS->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, &s_OSDSnapshot_VideoDecodeHist, &s_OSDSnapshot_ControllerVideoRetransmissionsStats, fStatsSize);
         xStats -= osd_render_stats_video_decode_get_width(pCS->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, &s_OSDSnapshot_VideoDecodeHist, &s_OSDSnapshot_ControllerVideoRetransmissionsStats, fStatsSize) + xSpacing;
      }
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_RADIO_LINKS) )
   if ( NULL != g_pSM_RadioStats )
   {
      osd_render_stats_radio_links( xStats, yStats-osd_render_stats_radio_links_get_height(g_pSM_RadioStats, fStatsSize), "Radio Links", g_pSM_RadioStats, fStatsSize);
      xStats -= osd_render_stats_radio_links_get_width(g_pSM_RadioStats, fStatsSize) + xSpacing;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_RADIO_INTERFACES) )
   if ( NULL != g_pSM_RadioStats )
   {
      osd_render_stats_radio_interfaces( xStats, yStats-osd_render_stats_radio_interfaces_get_height(g_pSM_RadioStats, fStatsSize), "Radio Interfaces", g_pSM_RadioStats, fStatsSize);
      xStats -= osd_render_stats_radio_interfaces_get_width(g_pSM_RadioStats, fStatsSize) + xSpacing;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_EFFICIENCY_STATS) )
   {
      osd_render_stats_efficiency(xStats, yStats - osd_render_stats_efficiency_get_height(fStatsSize), fStatsSize);
      xStats -= osd_render_stats_efficiency_get_width(fStatsSize) + xSpacing;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_RC) )
   {
      osd_render_stats_rc(xStats, yStats-osd_render_stats_rc_get_height(fStatsSize), fStatsSize);
      xStats -= osd_render_stats_rc_get_width(fStatsSize) + xSpacing;
   }

}

void _osd_render_stats_panels_vertical()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();
   float fStatsSize = 1.0;
  
   float hBar = osd_getBarHeight();
   float hBar2 = osd_getSecondBarHeight();
   if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      hBar = 0.0;
      hBar2 = 0.0;
   }

   float yMin = osd_getMarginY() + hBar + hBar2 + 0.04; 
   float yMax = 1.0-osd_getMarginY() - hBar - hBar2 - 0.03;
   
   if ( g_pCurrentModel->osd_params.layout == osdLayoutLean )
      yMax -= 0.05; 

   float yStats = yMin;
   float xStats = 0.99-osd_getMarginX();
   float fMaxColumnWidth = 0.0;
   float fSpacingH = 0.01;
   float fSpacingV = 0.01;

   if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
      xStats -= osd_getVerticalBarWidth();

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowDevVideoStats || p->iDebugShowDevRadioStats )
   {
      if ( yStats + osd_render_stats_dev_get_height() > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }     
      osd_render_stats_dev(xStats-osd_render_stats_dev_get_width(), yStats, fStatsSize);
      yStats += osd_render_stats_dev_get_height();
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_dev_get_width() )
         fMaxColumnWidth = osd_render_stats_dev_get_width();
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowVehicleVideoGraphs )
   {
      if ( yStats + osd_render_stats_video_graphs_get_height() > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }     
      osd_render_stats_video_graphs(xStats-osd_render_stats_video_graphs_get_width(), yStats);
      yStats += osd_render_stats_video_graphs_get_height();
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_video_graphs_get_width() )
         fMaxColumnWidth = osd_render_stats_video_graphs_get_width();
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowVehicleVideoStats )
   {
      if ( yStats + osd_render_stats_video_stats_get_height() > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }     
      osd_render_stats_video_stats(xStats-osd_render_stats_video_stats_get_width(), yStats);
      yStats += osd_render_stats_video_stats_get_height();
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_video_stats_get_width() )
         fMaxColumnWidth = osd_render_stats_video_stats_get_width();
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP) )
   {
      if ( yStats + osd_render_stats_graphs_vehicle_tx_gap_get_height() > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }     
      osd_render_stats_graphs_vehicle_tx_gap(xStats-osd_render_stats_graphs_vehicle_tx_gap_get_width(), yStats);
      yStats += osd_render_stats_graphs_vehicle_tx_gap_get_height();
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_graphs_vehicle_tx_gap_get_width() )
         fMaxColumnWidth = osd_render_stats_graphs_vehicle_tx_gap_get_width();
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_VIDEO_BITRATE_HISTORY) )
   {
      if ( yStats + osd_render_stats_video_bitrate_history_get_height() > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }     
      osd_render_stats_video_bitrate_history(xStats-osd_render_stats_video_bitrate_history_get_width(), yStats);
      yStats += osd_render_stats_video_bitrate_history_get_height();
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_video_bitrate_history_get_width() )
         fMaxColumnWidth = osd_render_stats_video_bitrate_history_get_width();
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_EFFICIENCY_STATS) )
   {
      if ( yStats + osd_render_stats_efficiency_get_height(fStatsSize) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }  
      osd_render_stats_efficiency(xStats-osd_render_stats_efficiency_get_width(1.0), yStats, fStatsSize);
      yStats += osd_render_stats_efficiency_get_height(fStatsSize);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_efficiency_get_width(fStatsSize) )
         fMaxColumnWidth = osd_render_stats_efficiency_get_width(fStatsSize);
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_RADIO_LINKS) )
   if ( NULL != g_pSM_RadioStats )
   {
      if ( yStats + osd_render_stats_radio_links_get_height(g_pSM_RadioStats, fStatsSize) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }         
      osd_render_stats_radio_links( xStats-osd_render_stats_radio_links_get_width(g_pSM_RadioStats, fStatsSize), yStats, "Radio Links", g_pSM_RadioStats, fStatsSize);
      yStats += osd_render_stats_radio_links_get_height(g_pSM_RadioStats, fStatsSize);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_radio_links_get_width(g_pSM_RadioStats, fStatsSize) )
         fMaxColumnWidth = osd_render_stats_radio_links_get_width(g_pSM_RadioStats, fStatsSize);
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_RADIO_INTERFACES) )
   if ( NULL != g_pSM_RadioStats )
   {
      if ( yStats + osd_render_stats_radio_interfaces_get_height(g_pSM_RadioStats, fStatsSize) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }         
      osd_render_stats_radio_interfaces( xStats-osd_render_stats_radio_interfaces_get_width(g_pSM_RadioStats, fStatsSize), yStats, "Radio Interfaces", g_pSM_RadioStats, fStatsSize);
      yStats += osd_render_stats_radio_interfaces_get_height(g_pSM_RadioStats, fStatsSize);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_radio_interfaces_get_width(g_pSM_RadioStats, fStatsSize) )
         fMaxColumnWidth = osd_render_stats_radio_interfaces_get_width(g_pSM_RadioStats, fStatsSize);
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_RC) )
   {
      if ( yStats + osd_render_stats_rc_get_height(fStatsSize) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }  
      osd_render_stats_rc(xStats-osd_render_stats_rc_get_width(fStatsSize), yStats, fStatsSize);
      yStats += osd_render_stats_rc_get_height(fStatsSize);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_rc_get_width(fStatsSize) )
         fMaxColumnWidth = osd_render_stats_rc_get_width(fStatsSize);
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_VIDEO) )
   {
      if ( yStats + osd_render_stats_video_decode_get_height(pCS->iDeveloperMode, false, g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, fStatsSize) > yMax )
      {
         yStats = yMin;
         xStats -= fMaxColumnWidth + fSpacingH;
         fMaxColumnWidth = 0.0;
      }         
      osd_render_stats_video_decode(xStats - osd_render_stats_video_decode_get_width(pCS->iDeveloperMode, false,  g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, fStatsSize), yStats, pCS->iDeveloperMode, false, g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, fStatsSize);
      yStats += osd_render_stats_video_decode_get_height(pCS->iDeveloperMode, false, g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, fStatsSize);
      yStats += fSpacingV;
      if ( fMaxColumnWidth < osd_render_stats_video_decode_get_width(pCS->iDeveloperMode, false,  g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, fStatsSize) )
         fMaxColumnWidth = osd_render_stats_video_decode_get_width(pCS->iDeveloperMode, false,  g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, fStatsSize);
   }

   if ( p->iDebugShowFullRXStats )
      osd_render_stats_full_rx_port();
}

void _osd_stats_swap_pannels(int iIndex1, int iIndex2)
{
   int tmp = s_iOSDStatsBoundingBoxesIds[iIndex1];
   s_iOSDStatsBoundingBoxesIds[iIndex1] = s_iOSDStatsBoundingBoxesIds[iIndex2];
   s_iOSDStatsBoundingBoxesIds[iIndex2] = tmp;

   float f = s_iOSDStatsBoundingBoxesX[iIndex1];
   s_iOSDStatsBoundingBoxesX[iIndex1] = s_iOSDStatsBoundingBoxesX[iIndex2];
   s_iOSDStatsBoundingBoxesX[iIndex2] = f;

   f = s_iOSDStatsBoundingBoxesY[iIndex1];
   s_iOSDStatsBoundingBoxesY[iIndex1] = s_iOSDStatsBoundingBoxesY[iIndex2];
   s_iOSDStatsBoundingBoxesY[iIndex2] = f;

   f = s_iOSDStatsBoundingBoxesW[iIndex1];
   s_iOSDStatsBoundingBoxesW[iIndex1] = s_iOSDStatsBoundingBoxesW[iIndex2];
   s_iOSDStatsBoundingBoxesW[iIndex2] = f;

   f = s_iOSDStatsBoundingBoxesH[iIndex1];
   s_iOSDStatsBoundingBoxesH[iIndex1] = s_iOSDStatsBoundingBoxesH[iIndex2];
   s_iOSDStatsBoundingBoxesH[iIndex2] = f;
}

void _osd_stats_autoarange_left( int iCountArranged, int iCurrentColumn )
{
   if ( iCountArranged >= s_iCountOSDStatsBoundingBoxes )
      return;

   if ( 0 == iCountArranged && 0 == iCurrentColumn )
      for( int i=0; i<10; i++ )
         s_fOSDStatsColumnsWidths[i] = g_fOSDStatsForcePanelWidth;

   float fColumnCurrentHeight = 0.0;
   float fCurrentColumnX = s_fOSDStatsMarginH;
   for( int i=0; i<iCurrentColumn; i++ )
      fCurrentColumnX += (s_fOSDStatsSpacingH + s_fOSDStatsColumnsWidths[i]);
   
   s_iOSDStatsBoundingBoxesX[iCountArranged] = fCurrentColumnX;// + s_iOSDStatsBoundingBoxesW[iCountArranged];
   s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginV;
   fColumnCurrentHeight += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;  

   if ( s_iOSDStatsBoundingBoxesW[iCountArranged] > s_fOSDStatsColumnsWidths[iCurrentColumn] )
      s_fOSDStatsColumnsWidths[iCurrentColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];

   iCountArranged++;
   
   // Try to fit more panels to current column
 
   int iCountAdded = 0;
   for( int i=iCountArranged; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      if ( fColumnCurrentHeight + s_iOSDStatsBoundingBoxesH[i] <= 1.0 - 2.0*s_fOSDStatsMarginV )
      {
         // Move it to sorted position
         _osd_stats_swap_pannels(iCountArranged, i);
         s_iOSDStatsBoundingBoxesX[iCountArranged] = fCurrentColumnX;// + s_iOSDStatsBoundingBoxesW[iCountArranged];
         s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginV + fColumnCurrentHeight;
         fColumnCurrentHeight += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
         iCountArranged++;
      }
   }

   // Still have panels to add? Move to next column
   if ( iCountArranged < s_iCountOSDStatsBoundingBoxes )
   {
      _osd_stats_autoarange_left(iCountArranged, iCurrentColumn+1);
   }
}


void _osd_stats_autoarange_right( int iCountArranged, int iCurrentColumn )
{
   if ( iCountArranged >= s_iCountOSDStatsBoundingBoxes )
      return;

   if ( 0 == iCountArranged && 0 == iCurrentColumn )
      for( int i=0; i<10; i++ )
         s_fOSDStatsColumnsWidths[i] = g_fOSDStatsForcePanelWidth;

   float fColumnCurrentHeight = 0.0;
   float fCurrentColumnX = 1.0-s_fOSDStatsMarginH;
   for( int i=0; i<iCurrentColumn; i++ )
      fCurrentColumnX -= (s_fOSDStatsSpacingH + s_fOSDStatsColumnsWidths[i]);
   
   s_iOSDStatsBoundingBoxesX[iCountArranged] = fCurrentColumnX - s_iOSDStatsBoundingBoxesW[iCountArranged];
   s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginV;
   
   if ( s_iOSDStatsBoundingBoxesW[iCountArranged] > s_fOSDStatsColumnsWidths[iCurrentColumn] )
      s_fOSDStatsColumnsWidths[iCurrentColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];

   fColumnCurrentHeight += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;  
   iCountArranged++;
   
   // Try to fit more panels to current column
 
   int iCountAdded = 0;
   for( int i=iCountArranged; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      if ( fColumnCurrentHeight + s_iOSDStatsBoundingBoxesH[i] <= 1.0 - 2.8*s_fOSDStatsMarginV )
      {
         // Move it to sorted position
         _osd_stats_swap_pannels(iCountArranged, i);
         s_iOSDStatsBoundingBoxesX[iCountArranged] = fCurrentColumnX - s_iOSDStatsBoundingBoxesW[iCountArranged];
         s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginV + fColumnCurrentHeight;
         fColumnCurrentHeight += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
         iCountArranged++;
      }
   }

   // Still have panels to add? Move to next column

   if ( iCountArranged < s_iCountOSDStatsBoundingBoxes )
   {
      _osd_stats_autoarange_right(iCountArranged, iCurrentColumn+1);
   }
}


void _osd_stats_autoarange_top()
{
   for( int i=0; i<10; i++ )
      s_fOSDStatsColumnsHeights[i] = s_fOSDStatsMarginV;

   // Add first the tall ones that take full height

   int iCountArranged = 0;
   int iColumn = 0;
   float fRowCurrentWidth = 0;
   for( int i=0; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      if ( s_iOSDStatsBoundingBoxesH[i] >= 1.0 - 2.0*s_fOSDStatsMarginV - s_fOSDStatsWindowsMinimBoxHeight - s_fOSDStatsSpacingV )
      {
         // Move it to sorted position
         _osd_stats_swap_pannels(iCountArranged, i);
         s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginH - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginV;
         s_iOSDStatsBoundingBoxesColumns[iCountArranged] = iColumn;
         s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
         s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];

         fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH;   
         iCountArranged++;
         iColumn++;
      }
   }

   // Add to remaning empty columns

   if ( fRowCurrentWidth < 1.0 - 2.0*s_fOSDStatsMarginH )
   {
      for( int i=iCountArranged; i<s_iCountOSDStatsBoundingBoxes; i++ )
      {
         if ( fRowCurrentWidth + s_iOSDStatsBoundingBoxesW[iCountArranged] <= 1.0 - 2.0*s_fOSDStatsMarginH )
         {
            // Move it to sorted position
            _osd_stats_swap_pannels(iCountArranged, i);
            s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginH - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
            s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginV;
            s_iOSDStatsBoundingBoxesColumns[iCountArranged] = iColumn;
            fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH;
            s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
            s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];
           
            iCountArranged++;
            iColumn++;
         }
      }
   }
   
   // Add rempaining panels to where they can fit
   
   while ( iCountArranged < s_iCountOSDStatsBoundingBoxes )
   {
      bool bFittedInColumn = false;
      for( int iColumn=0; iColumn<9; iColumn++ )
      {
         if ( s_fOSDStatsColumnsHeights[iColumn] <= s_fOSDStatsMarginV + 0.01 )
            continue;
         if ( s_fOSDStatsColumnsHeights[iColumn] + s_iOSDStatsBoundingBoxesH[iCountArranged]  <= 1.0 - 0.7*s_fOSDStatsMarginV )
         {
            s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginH - s_iOSDStatsBoundingBoxesW[iCountArranged];
            for( int i=0; i<iColumn; i++ )
                s_iOSDStatsBoundingBoxesX[iCountArranged] -= (s_fOSDStatsSpacingH + s_fOSDStatsColumnsWidths[i]);
            
            if ( s_iOSDStatsBoundingBoxesW[iCountArranged] > s_fOSDStatsColumnsWidths[iColumn] )
            {
               float dx = s_iOSDStatsBoundingBoxesW[iCountArranged] - s_fOSDStatsColumnsWidths[iColumn];
               s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];
               // Move all already arranged on next columns
               for( int i=0; i<iCountArranged; i++ )
                  if ( s_iOSDStatsBoundingBoxesColumns[i] >= iColumn )
                     s_iOSDStatsBoundingBoxesX[i] -= dx;
            }

            s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsColumnsHeights[iColumn];
            s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
            
            bFittedInColumn = true;
            break;
         }
      }

      if ( ! bFittedInColumn )
      {
         //s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginH - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         //s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsMarginV;
         //fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH; 

         s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginH - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         s_iOSDStatsBoundingBoxesY[iCountArranged] = s_fOSDStatsColumnsHeights[9];
         s_fOSDStatsColumnsHeights[9] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
      }
      iCountArranged++;
   }
}

void _osd_stats_autoarange_bottom()
{
   for( int i=0; i<10; i++ )
      s_fOSDStatsColumnsHeights[i] = s_fOSDStatsMarginV;

   // Add first the tall ones that take full height

   int iCountArranged = 0;
   int iColumn = 0;
   float fRowCurrentWidth = 0;
   for( int i=0; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      if ( s_iOSDStatsBoundingBoxesH[i] >= 1.0 - 2.0*s_fOSDStatsMarginV - s_fOSDStatsWindowsMinimBoxHeight - s_fOSDStatsSpacingV )
      {
         // Move it to sorted position
         _osd_stats_swap_pannels(iCountArranged, i);
         s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginH - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         s_iOSDStatsBoundingBoxesY[iCountArranged] = 1.0 - s_fOSDStatsMarginV - s_iOSDStatsBoundingBoxesH[iCountArranged];
         s_iOSDStatsBoundingBoxesColumns[iCountArranged] = iColumn;
         s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
         s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];
         
         fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH;
         iCountArranged++;
         iColumn++;
      }
   }

   // Add to remaning empty columns

   if ( fRowCurrentWidth < 1.0 - 2.0*s_fOSDStatsMarginH )
   {
      for( int i=iCountArranged; i<s_iCountOSDStatsBoundingBoxes; i++ )
      {
         if ( fRowCurrentWidth + s_iOSDStatsBoundingBoxesW[iCountArranged] <= 1.0 - 2.0*s_fOSDStatsMarginH )
         {
            // Move it to sorted position
            _osd_stats_swap_pannels(iCountArranged, i);
            s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginH - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
            s_iOSDStatsBoundingBoxesY[iCountArranged] = 1.0 - s_fOSDStatsMarginV - s_iOSDStatsBoundingBoxesH[iCountArranged];
            s_iOSDStatsBoundingBoxesColumns[iCountArranged] = iColumn;
            fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH;
            s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
            s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];
            iCountArranged++;
            iColumn++;
         }
      }
   }
   
   // Add rempaining panels to where they can fit
   
   while ( iCountArranged < s_iCountOSDStatsBoundingBoxes )
   {
      bool bFittedInColumn = false;
      for( int iColumn=0; iColumn<9; iColumn++ )
      {
         if ( s_fOSDStatsColumnsHeights[iColumn] <= s_fOSDStatsMarginV + 0.01 )
            continue;
         if ( s_fOSDStatsColumnsHeights[iColumn] + s_iOSDStatsBoundingBoxesH[iCountArranged] <= 1.0 - 0.7*s_fOSDStatsMarginV )
         {
            s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginH - s_iOSDStatsBoundingBoxesW[iCountArranged];
            for( int i=0; i<iColumn; i++ )
                s_iOSDStatsBoundingBoxesX[iCountArranged] -= (s_fOSDStatsSpacingH + s_fOSDStatsColumnsWidths[i]);
            
            if ( s_iOSDStatsBoundingBoxesW[iCountArranged] > s_fOSDStatsColumnsWidths[iColumn] )
            {
               float dx = s_iOSDStatsBoundingBoxesW[iCountArranged] - s_fOSDStatsColumnsWidths[iColumn];
               s_fOSDStatsColumnsWidths[iColumn] = s_iOSDStatsBoundingBoxesW[iCountArranged];
               // Move all already arranged on next columns
               for( int i=0; i<iCountArranged; i++ )
                  if ( s_iOSDStatsBoundingBoxesColumns[i] >= iColumn )
                     s_iOSDStatsBoundingBoxesX[i] -= dx;
            }

            s_iOSDStatsBoundingBoxesY[iCountArranged] = 1.0 - s_fOSDStatsColumnsHeights[iColumn] - s_iOSDStatsBoundingBoxesH[iCountArranged];
            s_fOSDStatsColumnsHeights[iColumn] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
            bFittedInColumn = true;
            break;
         }
      }

      if ( ! bFittedInColumn )
      {
         //s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginH - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         //s_iOSDStatsBoundingBoxesY[iCountArranged] = 1.0 - s_fOSDStatsMarginV - s_iOSDStatsBoundingBoxesH[iCountArranged];
         //fRowCurrentWidth += s_iOSDStatsBoundingBoxesW[iCountArranged] + s_fOSDStatsSpacingH; 
         s_iOSDStatsBoundingBoxesX[iCountArranged] = 1.0 - s_fOSDStatsMarginH - s_iOSDStatsBoundingBoxesW[iCountArranged] - fRowCurrentWidth;
         s_iOSDStatsBoundingBoxesY[iCountArranged] = 1.0 - s_fOSDStatsColumnsHeights[9] - s_iOSDStatsBoundingBoxesH[iCountArranged];
         s_fOSDStatsColumnsHeights[9] += s_iOSDStatsBoundingBoxesH[iCountArranged] + s_fOSDStatsSpacingV;
      }
      iCountArranged++;
   }
}

void osd_render_stats_panels()
{
   if ( NULL == g_pCurrentModel )
      return;
   
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* p = get_Preferences();

   s_idFontStats = g_idFontStats;
   s_idFontStatsSmall = g_idFontStatsSmall;
   s_nCurrentOSDFlagsIndex = g_pCurrentModel->osd_params.layout;
   s_OSDStatsLineSpacing = 1.0;

   if ( osd_getScaleOSDStats() >= 1.0 )
      g_fOSDStatsForcePanelWidth = 0.19*osd_getScaleOSDStats();
   else
      g_fOSDStatsForcePanelWidth = 0.2*osd_getScaleOSDStats()*1.4;

   for( int i=0; i<10; i++ )
      s_fOSDStatsColumnsWidths[i] = g_fOSDStatsForcePanelWidth;

   s_iCountOSDStatsBoundingBoxes = 0;

   g_fOSDStatsBgTransparency = 1.0;
   switch ( ((g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.layout])>>20) & 0x0F )
   {
      case 0: g_fOSDStatsBgTransparency = 0.25; break;
      case 1: g_fOSDStatsBgTransparency = 0.5; break;
      case 2: g_fOSDStatsBgTransparency = 0.75; break;
      case 3: g_fOSDStatsBgTransparency = 0.98; break;
   }

   s_fOSDStatsSpacingH = 0.014/g_pRenderEngine->getAspectRatio();
   s_fOSDStatsSpacingV = 0.014;

   s_fOSDStatsMarginH = osd_getMarginX();
   s_fOSDStatsMarginV = osd_getMarginY() + osd_getBarHeight() + osd_getSecondBarHeight();

   if ( g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_LAYOUT_LEFT_RIGHT )
   {
      s_fOSDStatsMarginH = osd_getVerticalBarWidth() + osd_getMarginX() + 0.02;
      s_fOSDStatsMarginV = osd_getMarginY();
   }

   if ( s_fOSDStatsMarginH < 0.01 )
      s_fOSDStatsMarginH = 0.01;
   if ( s_fOSDStatsMarginV < 0.01 )
      s_fOSDStatsMarginV = 0.01;
   
   if ( g_pCurrentModel->osd_params.osd_flags3[s_nCurrentOSDFlagsIndex] & OSD_FLAG3_SHOW_CONTROLLER_ADAPTIVE_VIDEO_INFO )
   if ( NULL != g_pSM_ControllerAdaptiveVideoInfo )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 14;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_adaptive_video_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_adaptive_video_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_RADIO_LINKS) )
   if ( NULL != g_pSM_RadioStats )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 7;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_radio_links_get_width(g_pSM_RadioStats, 1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_radio_links_get_height(g_pSM_RadioStats, 1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_RADIO_INTERFACES) )
   if ( NULL != g_pSM_RadioStats )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 8;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_radio_interfaces_get_width(g_pSM_RadioStats, 1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_radio_interfaces_get_height(g_pSM_RadioStats, 1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_VIDEO) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 6;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_decode_get_width(pCS->iDeveloperMode, false, g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, 1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_decode_get_height(pCS->iDeveloperMode, false, g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, 1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( p->iDebugShowVideoSnapshotOnDiscard )
   if ( g_bHasVideoDecodeStatsSnapshot )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 11;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_decode_get_width(pCS->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, &s_OSDSnapshot_VideoDecodeHist, &s_OSDSnapshot_ControllerVideoRetransmissionsStats, 1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_decode_get_height(pCS->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, &s_OSDSnapshot_VideoDecodeHist, &s_OSDSnapshot_ControllerVideoRetransmissionsStats, 1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_EFFICIENCY_STATS) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 9;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_efficiency_get_width(1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_efficiency_get_height(1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags[s_nCurrentOSDFlagsIndex] & OSD_FLAG_SHOW_STATS_VIDEO_INFO) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 12;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_stream_info_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_stream_info_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( s_bDebugStatsShowAll || (g_pCurrentModel->osd_params.osd_flags2[s_nCurrentOSDFlagsIndex] & OSD_FLAG_EXT_SHOW_STATS_RC) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 10;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_rc_get_width(1.0);
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_rc_get_height(1.0);
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowDevVideoStats || p->iDebugShowDevRadioStats )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 1;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_dev_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_dev_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 4;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_graphs_vehicle_tx_gap_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_graphs_vehicle_tx_gap_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( NULL != g_pCurrentModel && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_VIDEO_BITRATE_HISTORY) )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 5;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_bitrate_history_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_bitrate_history_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowVehicleVideoStats )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 3;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_stats_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_stats_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   if ( pCS->iDeveloperMode || s_bDebugStatsShowAll )
   if ( p->iDebugShowVehicleVideoGraphs )
   {
      s_iOSDStatsBoundingBoxesIds[s_iCountOSDStatsBoundingBoxes] = 2;
      s_iOSDStatsBoundingBoxesW[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_graphs_get_width();
      s_iOSDStatsBoundingBoxesH[s_iCountOSDStatsBoundingBoxes] = osd_render_stats_video_graphs_get_height();
      s_iCountOSDStatsBoundingBoxes++;
   }

   s_fOSDStatsWindowsMinimBoxHeight = 2.0;
   for( int i=0; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      s_iOSDStatsBoundingBoxesColumns[i] = -1;

      if ( s_iOSDStatsBoundingBoxesH[i] < s_fOSDStatsWindowsMinimBoxHeight  )
         s_fOSDStatsWindowsMinimBoxHeight = s_iOSDStatsBoundingBoxesH[i];
   }


   // Auto arange

   if ( g_pCurrentModel->osd_params.osd_preferences[s_nCurrentOSDFlagsIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT )
      _osd_stats_autoarange_left(0, 0);
   else if ( g_pCurrentModel->osd_params.osd_preferences[s_nCurrentOSDFlagsIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT )
      _osd_stats_autoarange_right(0, 0);
   else if ( g_pCurrentModel->osd_params.osd_preferences[s_nCurrentOSDFlagsIndex] & OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM )
      _osd_stats_autoarange_bottom();
   else
      _osd_stats_autoarange_top();

   // Draw

   for( int i=0; i<s_iCountOSDStatsBoundingBoxes; i++ )
   {
      if ( s_iOSDStatsBoundingBoxesIds[i] == 14 )
         osd_render_stats_adaptive_video(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);
      
      if ( s_iOSDStatsBoundingBoxesIds[i] == 1 )
         osd_render_stats_dev(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], 1.0);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 2 )
         osd_render_stats_video_graphs(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 3 )
         osd_render_stats_video_stats(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 4 )
         osd_render_stats_graphs_vehicle_tx_gap(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 5 )
         osd_render_stats_video_bitrate_history(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 6 )
         osd_render_stats_video_decode(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], pCS->iDeveloperMode, false, g_pSM_RadioStats, g_psmvds, g_psmvds_history, g_pSM_ControllerRetransmissionsStats, 1.0);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 11 )
         osd_render_stats_video_decode(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], pCS->iDeveloperMode, true, &s_OSDSnapshot_RadioStats, &s_OSDSnapshot_VideoDecodeStats, &s_OSDSnapshot_VideoDecodeHist, &s_OSDSnapshot_ControllerVideoRetransmissionsStats, 1.0);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 12 )
         osd_render_stats_video_stream_info(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i]);
      
      if ( s_iOSDStatsBoundingBoxesIds[i] == 7 )
         osd_render_stats_radio_links( s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], "Radio Links", g_pSM_RadioStats, 1.0);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 8 )
         osd_render_stats_radio_interfaces( s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], "Radio Interfaces", g_pSM_RadioStats, 1.0);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 9 )
         osd_render_stats_efficiency(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], 1.0);

      if ( s_iOSDStatsBoundingBoxesIds[i] == 10 )
         osd_render_stats_rc(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], 1.0);

      //char szBuff[32];
      //sprintf(szBuff, "%d", i);
      //g_pRenderEngine->drawText(s_iOSDStatsBoundingBoxesX[i], s_iOSDStatsBoundingBoxesY[i], s_idFontStats, szBuff);
   }

   if ( p->iDebugShowFullRXStats )
      osd_render_stats_full_rx_port();
}
