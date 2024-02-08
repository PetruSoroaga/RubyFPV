/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../../base/base.h"
#include "../../base/hardware.h"
#include "../../base/hardware_radio.h"
#include "../../base/config.h"
#include "../../base/hdmi_video.h"
#include "../../base/models.h"
#include "../../base/ctrl_interfaces.h"
#include "../../common/string_utils.h"

#include "../../renderer/render_engine.h"

#include "osd.h"
#include "osd_common.h"
#include "../colors.h"
#include "osd_links.h"
#include "../shared_vars.h"
#include "../timers.h"

float _osd_get_radio_link_new_height()
{
   float height_text_small = osd_getFontHeightSmall();

   float fHeightLink = osd_get_link_bars_height(1.0);

   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_RADIO_INTERFACES_INFO )
      fHeightLink += height_text_small;

   if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG2_SHOW_RADIO_LINK_INTERFACES_EXTENDED )
      fHeightLink += height_text_small;
   return fHeightLink;
}


float _osd_render_local_radio_link_tag_new(float xPos, float yPos, int iLocalRadioLinkId, bool bHorizontal, bool bDraw)
{
   char szBuff1[32];
   char szBuff2[32];
   char szBuff3[32];

   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId;
   sprintf(szBuff1, "Link-%d", iVehicleRadioLinkId+1);

   strcpy(szBuff2, str_format_frequency_no_sufix(g_pCurrentModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkId]));
   
   szBuff3[0] = 0;
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      strcpy(szBuff3, "RELAY");
      strcpy(szBuff2, str_format_frequency_no_sufix(g_pCurrentModel->relay_params.uRelayFrequencyKhz));
   }
   float fWidthLink = g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff1);
   float fWidthFreq = g_pRenderEngine->textWidth(g_idFontOSD, szBuff2);
   float fWidthRelay = g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff3);

   if ( fWidthFreq > fWidthLink )
      fWidthLink = fWidthFreq;
   if ( fWidthRelay > fWidthLink )
      fWidthLink = fWidthRelay;

   fWidthLink += 0.5*height_text_small;
   float fHeight = 1.1 * height_text_small + height_text;
   
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      fHeight += 1.0*height_text_small;

   if ( bDraw )
   {
      g_pRenderEngine->drawBackgroundBoundingBoxes(false);

      double* pC = get_Color_OSDText();
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], 0.3);
      g_pRenderEngine->setStroke(0,0,0,0);
      g_pRenderEngine->drawRoundRect(xPos+1.0*g_pRenderEngine->getPixelWidth(), yPos + 1.0*g_pRenderEngine->getPixelHeight(), fWidthLink, fHeight, 0.003*osd_getScaleOSD());

      osd_set_colors();

      yPos += 0.07 * height_text_small;
      float fWidth = g_pRenderEngine->textWidth(g_idFontOSD, szBuff2);
      g_pRenderEngine->drawText(xPos+(fWidthLink-fWidth)*0.5, yPos, g_idFontOSD, szBuff2);

      yPos += height_text*0.9;
      fWidth = g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff1);
      g_pRenderEngine->drawText(xPos+(fWidthLink-fWidth)*0.5, yPos,  g_idFontOSDSmall, szBuff1);

      yPos += height_text_small*0.9;

      // Third line
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         fWidth = g_pRenderEngine->textWidth(g_idFontOSDSmall, "DIS");
         g_pRenderEngine->drawText(xPos+(fWidthLink-fWidth)*0.5, yPos, g_idFontOSDSmall, "DIS");
      }
      else if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      {
         fWidth = g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff3);
         g_pRenderEngine->drawText(xPos+(fWidthLink-fWidth)*0.5, yPos, g_idFontOSDSmall, szBuff3);
      }

      if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY )
         g_pRenderEngine->drawBackgroundBoundingBoxes(true);
   }

   if ( bHorizontal )
      return fWidthLink;
   else
      return fHeight;
}

float _osd_show_radio_bars_info(float xPos, float yPos, int iLastRxDeltaTime, int iMaxRxQuality, int dbm, int iDataRateBps, bool bShowBars, bool bShowPercentages, bool bUplink, bool bHorizontal, bool bDraw, bool bDatarateChanged)
{
   if ( ! bShowPercentages )
   if ( ! bShowBars )
      return 0.0;

   float height_text_small = osd_getFontHeightSmall();

   float fTotalWidth = 0.0;
   float fTotalHeight = osd_get_link_bars_height(1.0);

   /*
   bool bShowRed = false;
   if ( iLastRxDeltaTime > 1000 )
      bShowRed = true;
   if ( iMaxRxQuality <= 0 )
      bShowRed = true;
   */

   if ( bShowBars )
   {
       float fSize = osd_get_link_bars_width(1.0);
       if ( bDraw )
          osd_show_link_bars(xPos+fSize,yPos, iLastRxDeltaTime, iMaxRxQuality, 1.0);
       xPos += fSize;
       fTotalWidth += fSize;
   }

   float fIconHeight = fTotalHeight;
   float fIconWidth = 0.5*fIconHeight/g_pRenderEngine->getAspectRatio();

   if ( bDraw )
   {
      if ( bUplink )
         g_pRenderEngine->drawIcon(xPos-3.0*g_pRenderEngine->getPixelWidth(),yPos, fIconWidth, fIconHeight, g_idIconArrowUp);
      else
         g_pRenderEngine->drawIcon(xPos-3.0*g_pRenderEngine->getPixelWidth(),yPos, fIconWidth, fIconHeight, g_idIconArrowDown);
   }

   xPos += fIconWidth*1.1;
   fTotalWidth += fIconWidth*1.1;

   char szDBM[32];
   if ( dbm < -110 )
      strcpy(szDBM, "--- dBm");
   else
      sprintf(szDBM, "%d dBm", dbm);

   if ( bShowPercentages )
   {
      char szBuff[32];
      sprintf(szBuff, "%d%%", iMaxRxQuality);
      
      /*char szBuffDR[32];
      str_format_bitrate(iDataRateBps, szBuffDR);
      if ( strlen(szBuffDR) > 4 )
      if ( szBuffDR[strlen(szBuffDR)-2] == 'p' )
         szBuffDR[strlen(szBuffDR)-2] = 0;
      */

      osd_set_colors();
      float fWidthPercentage = g_pRenderEngine->textWidth(g_idFontOSDSmall, "1999%");
      float fWidthDBM = g_pRenderEngine->textWidth(g_idFontOSDSmall, "-110 dBm");
      if ( fWidthDBM > fWidthPercentage )
         fWidthPercentage = fWidthDBM;
      if ( bDraw )
      {
         if ( iMaxRxQuality >= 0 )
            g_pRenderEngine->drawText(xPos, yPos, g_idFontOSDSmall, szBuff);
         g_pRenderEngine->drawText(xPos, yPos+height_text_small, g_idFontOSDSmall, szDBM);
      }
      fTotalWidth += fWidthPercentage;
   }
   if ( bHorizontal )
      return fTotalWidth;
   else
      return fTotalHeight;
}


float osd_show_local_radio_link_new(float xPos, float yPos, int iLocalRadioLinkId, bool bHorizontal)
{
   float fMarginY = 0.5*osd_getSpacingV();
   float fMarginX = 0.5*osd_getSpacingH();

   if ( ! bHorizontal )
      xPos += 4.0*g_pRenderEngine->getPixelWidth();
   float xStart = xPos;
   float yStart = yPos;

   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();
   float height_text_extra_small = g_pRenderEngine->textHeight(g_idFontOSDExtraSmall);

   float fWidthTag = _osd_render_local_radio_link_tag_new(xPos,yPos, iLocalRadioLinkId, true, false);
   float fHeightTag = _osd_render_local_radio_link_tag_new(xPos,yPos, iLocalRadioLinkId, false, false);

   float fTotalWidthLink = 0.0;
   float fTotalHeightLink = fHeightTag;
   float fHeightLink = _osd_get_radio_link_new_height();

   if ( ! bHorizontal )
      fTotalHeightLink = 0.0;

   if ( bHorizontal )
   if ( fHeightLink > fTotalHeightLink )
      fTotalHeightLink = fHeightLink;

   int iVehicleRadioLinkId = g_SM_RadioStats.radio_links[iLocalRadioLinkId].matchingVehicleRadioLinkId;
   
   int iRuntimeInfoToUse = 0; // Main vehicle
   Model* pModelToUse = g_pCurrentModel;
   // int iRuntimeInfoToUse = osd_get_current_data_source_vehicle_index;
   //Model* pModelToUse = osd_get_current_data_source_vehicle_model();
   
   bool bShowVehicleSide = false;
   bool bShowControllerSide = false;
   bool bShowBars = false;
   bool bShowPercentages = false;

   bool bShowSummary = true;
   bool bShowInterfaces = false;
   bool bShowInterfacesExtended = false;

   if ( pModelToUse->osd_params.osd_flags[pModelToUse->osd_params.layout] & OSD_FLAG_SHOW_RADIO_LINKS )
      bShowControllerSide = true;

   if ( pModelToUse->osd_params.osd_flags[pModelToUse->osd_params.layout] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS )
      bShowVehicleSide = true;

   if ( pModelToUse->osd_params.osd_flags2[pModelToUse->osd_params.layout] & OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_BARS )
      bShowBars = true;

   if ( pModelToUse->osd_params.osd_flags2[pModelToUse->osd_params.layout] & OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_PERCENTAGE )
      bShowPercentages = true;

   if ( pModelToUse->osd_params.osd_flags[pModelToUse->osd_params.layout] & OSD_FLAG_SHOW_RADIO_INTERFACES_INFO )
   {
      bShowInterfaces = true;
      bShowSummary = false;
   }

   if ( pModelToUse->osd_params.osd_flags2[pModelToUse->osd_params.layout] & OSD_FLAG2_SHOW_RADIO_LINK_INTERFACES_EXTENDED )
   {
      bShowInterfaces = true;
      bShowInterfacesExtended = true;
      bShowSummary = false;    
   }

   if ( pModelToUse->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      bShowVehicleSide = true;
      bShowControllerSide = false;
   }

   if ( bHorizontal )
   {
      xPos -= fMarginX;
      yPos += fMarginY;
   }
   else
   {
      xPos += fMarginX;
      yPos += fMarginY;
   }

   if ( ! bHorizontal )
   {
      _osd_render_local_radio_link_tag_new(xStart,yStart, iLocalRadioLinkId, bHorizontal, true);
      fTotalHeightLink += fHeightTag;
      yPos += fHeightTag;
   }

   float dySignalBars = 0.0;
   if ( (! bShowInterfaces) && (! bShowInterfacesExtended) )
       dySignalBars = height_text_small*0.3;
   else
      dySignalBars = height_text_small*0.1;

   // Show vehicle side

   if ( bShowVehicleSide )
   {
      int nRxQuality = 0;
      int iDataRateUplinkBPS = 0;
      int iLastRxDeltaTime = 100000;
      int dbm = -200;
      
      for( int i=0; i<pModelToUse->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( pModelToUse->radioInterfacesParams.interface_link_id[i] != iVehicleRadioLinkId )
            continue;
         if ( g_VehiclesRuntimeInfo[iRuntimeInfoToUse].headerRubyTelemetryExtended.uplink_link_quality[i] > nRxQuality )
            nRxQuality = g_VehiclesRuntimeInfo[iRuntimeInfoToUse].headerRubyTelemetryExtended.uplink_link_quality[i];
         
         iDataRateUplinkBPS = g_VehiclesRuntimeInfo[iRuntimeInfoToUse].headerRubyTelemetryExtended.last_recv_datarate_bps[i];
         if ( pModelToUse->radioLinkIsSiKRadio(iVehicleRadioLinkId) )
            iDataRateUplinkBPS = pModelToUse->radioLinksParams.link_datarate_data_bps[iVehicleRadioLinkId];

         if ( ! pModelToUse->radioLinkIsSiKRadio(iVehicleRadioLinkId) )
         if ( (g_VehiclesRuntimeInfo[iRuntimeInfoToUse].headerRubyTelemetryExtended.uplink_rssi_dbm[i]-200) > dbm )
            dbm = g_VehiclesRuntimeInfo[iRuntimeInfoToUse].headerRubyTelemetryExtended.uplink_rssi_dbm[i]-200;
      }

      int iVehicleRadioCard = -1;
      for( int i=0; i<pModelToUse->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( pModelToUse->radioInterfacesParams.interface_link_id[i] == iVehicleRadioLinkId )
         {
            iVehicleRadioCard = i;
            break;
         }
      }
      if ( iVehicleRadioCard != -1 )
          iLastRxDeltaTime = g_VehiclesRuntimeInfo[iRuntimeInfoToUse].SMVehicleRxStats[iVehicleRadioLinkId].timeNow - g_VehiclesRuntimeInfo[osd_get_current_data_source_vehicle_index()].SMVehicleRxStats[iVehicleRadioLinkId].timeLastRxPacket;
            
      if ( bShowSummary )
      {
         bool bAsUplink = true;
         if ( pModelToUse->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
            bAsUplink = false;
         float fSize = _osd_show_radio_bars_info(xPos, yPos+dySignalBars, iLastRxDeltaTime, nRxQuality, dbm, iDataRateUplinkBPS, bShowBars, bShowPercentages, bAsUplink, bHorizontal, false, false);
         if ( bHorizontal )
         {
            xPos -= fSize;
            fTotalWidthLink += fSize;
         }
         else if ( fSize > fTotalWidthLink )
            fTotalWidthLink = fSize;

         _osd_show_radio_bars_info(xPos, yPos+dySignalBars, iLastRxDeltaTime, nRxQuality, dbm, iDataRateUplinkBPS, bShowBars, bShowPercentages, bAsUplink, bHorizontal, true, false);
         if ( ! bHorizontal )
         {
             yPos += fSize;
             fTotalHeightLink += fSize;
         }
      }
      else // Vehicle side, not summary
      {
         static int s_iLastOSDVehRadioLinkInterfacesRecvDatarates[MAX_RADIO_INTERFACES][MAX_RADIO_INTERFACES];
         static u32 s_uLastOSDVehRadioLinkInterfacesRecvDataratesChangeTimes[MAX_RADIO_INTERFACES][MAX_RADIO_INTERFACES];
         static bool s_bLastOSDVehRadioLinKInterfacesRecvDataratesInit = true;
         if ( s_bLastOSDVehRadioLinKInterfacesRecvDataratesInit )
         {
            s_bLastOSDVehRadioLinKInterfacesRecvDataratesInit = false;
            for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
            for( int j=0; j<MAX_RADIO_INTERFACES; j++ )
            {
               s_iLastOSDVehRadioLinkInterfacesRecvDatarates[k][j] = 0;
               s_uLastOSDVehRadioLinkInterfacesRecvDataratesChangeTimes[k][j] = 0;
            }
         }
            
         int iCountShown = 0;
         for( int i=0; i<pModelToUse->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( pModelToUse->radioInterfacesParams.interface_link_id[i] != iVehicleRadioLinkId )
               continue;
            nRxQuality = g_VehiclesRuntimeInfo[iRuntimeInfoToUse].headerRubyTelemetryExtended.uplink_link_quality[i];
            iLastRxDeltaTime = g_VehiclesRuntimeInfo[iRuntimeInfoToUse].SMVehicleRxStats[i].timeNow - g_VehiclesRuntimeInfo[iRuntimeInfoToUse].SMVehicleRxStats[i].timeLastRxPacket;
            iDataRateUplinkBPS = g_VehiclesRuntimeInfo[iRuntimeInfoToUse].headerRubyTelemetryExtended.last_recv_datarate_bps[i];
            dbm = g_VehiclesRuntimeInfo[iRuntimeInfoToUse].headerRubyTelemetryExtended.uplink_rssi_dbm[i]-200;

            if ( pModelToUse->radioLinkIsSiKRadio(iVehicleRadioLinkId) )
            {
               iDataRateUplinkBPS = g_pCurrentModel->radioLinksParams.link_datarate_data_bps[iVehicleRadioLinkId];
               dbm = -200;
            }

            if ( iDataRateUplinkBPS != s_iLastOSDVehRadioLinkInterfacesRecvDatarates[iVehicleRadioLinkId][i] )
            {
               s_iLastOSDVehRadioLinkInterfacesRecvDatarates[iVehicleRadioLinkId][i] = iDataRateUplinkBPS;
               s_uLastOSDVehRadioLinkInterfacesRecvDataratesChangeTimes[iVehicleRadioLinkId][i] = g_TimeNow;
            }

            if ( iCountShown > 0 )
            {
               if ( bHorizontal )
               {
                  xPos -= osd_getSpacingH()*0.5;
                  fTotalWidthLink += osd_getSpacingH()*0.5;
               }
               else
               {
                  yPos += osd_getSpacingV()*0.5;
                  fTotalHeightLink += osd_getSpacingV()*0.5;
               }
            }

            bool bAsUplink = true;
            if ( pModelToUse->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
               bAsUplink = false;
         
            bool bDRChanged = false;
            if ( g_bOSDElementChangeNotification )
            if ( pModelToUse->osd_params.osd_flags3[pModelToUse->osd_params.layout] & OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS )
            if ( g_TimeNow < s_uLastOSDVehRadioLinkInterfacesRecvDataratesChangeTimes[iVehicleRadioLinkId][i] + g_uOSDElementChangeTimeout )
               bDRChanged = true;

            float fSize = _osd_show_radio_bars_info(xPos, yPos+dySignalBars, iLastRxDeltaTime, nRxQuality, dbm, iDataRateUplinkBPS, bShowBars, bShowPercentages, bAsUplink, bHorizontal, false, false);
            if ( bHorizontal )
            {
               xPos -= fSize;
               fTotalWidthLink += fSize;
            }
            else if ( fSize > fTotalWidthLink )
               fTotalWidthLink = fSize;
            _osd_show_radio_bars_info(xPos, yPos+dySignalBars, iLastRxDeltaTime, nRxQuality, dbm, iDataRateUplinkBPS, bShowBars, bShowPercentages, bAsUplink, bHorizontal, true, false);
            if ( ! bHorizontal )
            {
                yPos += fSize;
                fTotalHeightLink += fSize;
            }

            char szLine1[128];
            char szBuffDR[32];
            bool bShowLine1AsError = false;
            sprintf(szLine1, "%s: RX/TX ", pModelToUse->radioInterfacesParams.interface_szPort[i] );
            
            str_format_bitrate(iDataRateUplinkBPS, szBuffDR);
            if ( strlen(szBuffDR) > 4 )
            if ( szBuffDR[strlen(szBuffDR)-2] == 'p' )
               szBuffDR[strlen(szBuffDR)-2] = 0;
            
            strcat(szLine1, szBuffDR);

            if ( bShowLine1AsError )
               g_pRenderEngine->setColors(get_Color_IconError());

            if ( bDRChanged && g_bOSDElementChangeNotification )
            if ( pModelToUse->osd_params.osd_flags3[pModelToUse->osd_params.layout] & OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS )
               osd_set_colors_text(get_Color_OSDElementChanged());

            float fWidthLine1 = g_pRenderEngine->textWidth(g_idFontOSDExtraSmall, szLine1);
            int iFontLine1 = g_idFontOSDSmall;
            float dyLine1 = 0.0;
            if ( fWidthLine1 > fSize*0.8 )
            {
               iFontLine1 = g_idFontOSDExtraSmall;
               dyLine1 = (height_text_small - height_text_extra_small)*0.6;
            }
            fWidthLine1 = g_pRenderEngine->textWidth(iFontLine1, szLine1);
            
            if ( (! bDRChanged) || (g_TimeNow >= s_uLastOSDVehRadioLinkInterfacesRecvDataratesChangeTimes[iVehicleRadioLinkId][i] + g_uOSDElementChangeTimeout/2) ||
                 (bDRChanged && ((g_TimeNow/g_uOSDElementChangeBlinkInterval)%2)))
            {
               if ( bHorizontal )
                  g_pRenderEngine->drawText(xPos + (fSize - fWidthLine1)*0.5, yPos + dyLine1 + dySignalBars + osd_get_link_bars_height(1.0), iFontLine1, szLine1);
               else
               {
                  g_pRenderEngine->drawText(xStart + (osd_getVerticalBarWidth() - fWidthLine1)*0.5, yPos + dyLine1 + dySignalBars, iFontLine1, szLine1);
                  yPos += height_text_small;
                  fTotalHeightLink += height_text_small;             
               }
            }
            if ( bShowLine1AsError || bDRChanged )
               osd_set_colors();

            iCountShown++;
         }
      }
   }

   // Separator between controller and vehicle side

   if ( bShowVehicleSide && bShowControllerSide )
   {
       if ( bHorizontal )
       {
          xPos -= osd_getSpacingH()*0.5;
          fTotalWidthLink += osd_getSpacingH()*0.5;
       }
       else
       {
          yPos += osd_getSpacingV()*0.6;
          fTotalHeightLink += osd_getSpacingV()*0.6;
       }
   }

   // Controller side

   if ( bShowControllerSide )
   {
      int iCountInterfacesForCurrentLink = 0;
      if ( iLocalRadioLinkId >= 0 )
      {
         for( int k=0; k<g_SM_RadioStats.countLocalRadioInterfaces; k++ )
            if ( g_SM_RadioStats.radio_interfaces[k].assignedLocalRadioLinkId == iLocalRadioLinkId )
               iCountInterfacesForCurrentLink++;
      }

      int nRxQuality = 0;
      int iRecvDataRateVideo = 0;
      int iRecvDataRateData = 0;
      int iLastRxDeltaTime = 100000;
      int dbm = -200;
      for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId != iLocalRadioLinkId )
            continue;
         if ( g_SM_RadioStats.radio_interfaces[i].rxQuality > nRxQuality )
            nRxQuality = g_SM_RadioStats.radio_interfaces[i].rxQuality;
      
         if ( g_SM_RadioStats.radio_interfaces[i].lastRecvDataRateVideo > iRecvDataRateVideo )
           iRecvDataRateVideo = g_SM_RadioStats.radio_interfaces[i].lastRecvDataRateVideo;
         if ( g_SM_RadioStats.radio_interfaces[i].lastRecvDataRateData > iRecvDataRateData )
           iRecvDataRateData = g_SM_RadioStats.radio_interfaces[i].lastRecvDataRateData;

         int iRxDeltaTime = g_TimeNow - g_SM_RadioStats.radio_interfaces[i].timeLastRxPacket;
         if ( iRxDeltaTime < iLastRxDeltaTime )
            iLastRxDeltaTime = iRxDeltaTime;

         if ( g_fOSDDbm[i] > dbm )
            dbm = g_fOSDDbm[i];
      }

      if ( 0 == iRecvDataRateVideo )
         iRecvDataRateVideo = iRecvDataRateData;

      if ( pModelToUse->radioLinkIsSiKRadio(iVehicleRadioLinkId) )
      {
         iRecvDataRateVideo = pModelToUse->radioLinksParams.link_datarate_data_bps[iVehicleRadioLinkId];
         iRecvDataRateData = iRecvDataRateVideo;
      }
      
      if ( bShowSummary )
      {
         float fSize = _osd_show_radio_bars_info(xPos, yPos+dySignalBars, iLastRxDeltaTime, nRxQuality, dbm, iRecvDataRateVideo, bShowBars, bShowPercentages, false, bHorizontal, false, false);
         if ( bHorizontal )
         {
            xPos -= fSize;
            fTotalWidthLink += fSize;
         }
         else if ( fSize > fTotalWidthLink )
            fTotalWidthLink = fSize;

         _osd_show_radio_bars_info(xPos, yPos+dySignalBars, iLastRxDeltaTime, nRxQuality, dbm, iRecvDataRateVideo, bShowBars, bShowPercentages, false, bHorizontal, true, true);
         if ( ! bHorizontal )
         {
             yPos += fSize;
             fTotalHeightLink += fSize;
         }
      }
      else // Controller side, not summary
      {
         static int s_iLastOSDRadioLinkInterfacesRecvDatarates[MAX_RADIO_INTERFACES][MAX_RADIO_INTERFACES];
         static u32 s_uLastOSDRadioLinkInterfacesRecvDataratesChangeTimes[MAX_RADIO_INTERFACES][MAX_RADIO_INTERFACES];
         static bool s_bLastOSDRadioLinKInterfacesRecvDataratesInit = true;
         if ( s_bLastOSDRadioLinKInterfacesRecvDataratesInit )
         {
            s_bLastOSDRadioLinKInterfacesRecvDataratesInit = false;
            for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
            for( int j=0; j<MAX_RADIO_INTERFACES; j++ )
            {
               s_iLastOSDRadioLinkInterfacesRecvDatarates[k][j] = 0;
               s_uLastOSDRadioLinkInterfacesRecvDataratesChangeTimes[k][j] = 0;
            }
         }

         int iCountShown = 0;
         for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
         {
            if ( g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId != iLocalRadioLinkId )
               continue;
            nRxQuality = g_SM_RadioStats.radio_interfaces[i].rxQuality;
            iLastRxDeltaTime = g_TimeNow - g_SM_RadioStats.radio_interfaces[i].timeLastRxPacket;
            iRecvDataRateVideo = g_SM_RadioStats.radio_interfaces[i].lastRecvDataRateVideo;
            if ( 0 == iRecvDataRateVideo )
               iRecvDataRateVideo = g_SM_RadioStats.radio_interfaces[i].lastRecvDataRateData;
              
            if ( g_pCurrentModel->radioLinkIsSiKRadio(iVehicleRadioLinkId) )
               iRecvDataRateVideo = g_pCurrentModel->radioLinksParams.link_datarate_data_bps[iVehicleRadioLinkId];

            if ( iRecvDataRateVideo != s_iLastOSDRadioLinkInterfacesRecvDatarates[iLocalRadioLinkId][i] )
            {
               s_iLastOSDRadioLinkInterfacesRecvDatarates[iLocalRadioLinkId][i] = iRecvDataRateVideo;
               s_uLastOSDRadioLinkInterfacesRecvDataratesChangeTimes[iLocalRadioLinkId][i] = g_TimeNow;
            }

            bool bDRChanged = false;
            if ( g_bOSDElementChangeNotification )
            if ( pModelToUse->osd_params.osd_flags3[pModelToUse->osd_params.layout] & OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS )
            if ( g_TimeNow < s_uLastOSDRadioLinkInterfacesRecvDataratesChangeTimes[iLocalRadioLinkId][i] + g_uOSDElementChangeTimeout )
               bDRChanged = true;

            bool bIsTxCard = false;
            if ( (iLocalRadioLinkId >= 0) && (g_SM_RadioStats.radio_links[iLocalRadioLinkId].lastTxInterfaceIndex == i) && (1 < g_SM_RadioStats.countLocalRadioInterfaces) )
               bIsTxCard = true;
      
            if ( iCountShown > 0 )
            {
               if ( bHorizontal )
               {
                  xPos -= osd_getSpacingH()*0.5;
                  fTotalWidthLink += osd_getSpacingH()*0.5;
               }
               else
               {
                  yPos += osd_getSpacingV()*0.5;
                  fTotalHeightLink += osd_getSpacingV()*0.5;
               }
            }
            float fSize = _osd_show_radio_bars_info(xPos, yPos+dySignalBars, iLastRxDeltaTime, nRxQuality, g_fOSDDbm[i], iRecvDataRateVideo, bShowBars, bShowPercentages, false, bHorizontal, false, false);
            if ( bHorizontal )
            {
               xPos -= fSize;
               fTotalWidthLink += fSize;
            }
            else if ( fSize > fTotalWidthLink )
               fTotalWidthLink = fSize;
            _osd_show_radio_bars_info(xPos, yPos+dySignalBars, iLastRxDeltaTime, nRxQuality, g_fOSDDbm[i], iRecvDataRateVideo, bShowBars, bShowPercentages, false, bHorizontal, true, true);
            
            if ( bIsTxCard && (1<iCountInterfacesForCurrentLink) )
               g_pRenderEngine->drawIcon(xPos-height_text*0.1, yPos+dySignalBars, height_text*0.9/g_pRenderEngine->getAspectRatio() , height_text*0.9, g_idIconUplink2);

            if ( ! bHorizontal )
            {
                yPos += fSize;
                fTotalHeightLink += fSize;
            }

            char szLine1[128];
            bool bShowLine1AsError = false;
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);

            if ( NULL != pRadioHWInfo )
            {
               if ( g_SM_RadioStats.radio_links[iLocalRadioLinkId].lastTxInterfaceIndex == i )
                  sprintf(szLine1, "%s: RX/TX ", pRadioHWInfo->szUSBPort );
               else if ( g_SM_RadioStats.radio_interfaces[i].openedForRead )
                  sprintf(szLine1, "%s: RX ", pRadioHWInfo->szUSBPort );
               else if ( g_SM_RadioStats.radio_interfaces[i].openedForWrite )
                  sprintf(szLine1, "%s: -- ", pRadioHWInfo->szUSBPort );
               else
                  sprintf(szLine1, "%s: N/A ", pRadioHWInfo->szUSBPort );
            }
            else
               strcpy(szLine1, "N/A ");

            char szBuffDR[32];
            str_format_bitrate(iRecvDataRateVideo, szBuffDR);
            if ( strlen(szBuffDR) > 4 )
            if ( szBuffDR[strlen(szBuffDR)-2] == 'p' )
               szBuffDR[strlen(szBuffDR)-2] = 0;

            strcat(szLine1, szBuffDR);
            
            if ( bShowLine1AsError )
               g_pRenderEngine->setColors(get_Color_IconError());

            if ( bDRChanged && g_bOSDElementChangeNotification )
            if ( pModelToUse->osd_params.osd_flags3[pModelToUse->osd_params.layout] & OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS )
               osd_set_colors_text(get_Color_OSDElementChanged());

            float fWidthLine1 = g_pRenderEngine->textWidth(g_idFontOSDExtraSmall, szLine1);
            int iFontLine1 = g_idFontOSDSmall;
            float dyLine1 = 0.0;
            if ( fWidthLine1 > fSize*0.8 )
            {
               iFontLine1 = g_idFontOSDExtraSmall;
               dyLine1 = (height_text_small - height_text_extra_small)*0.6;
            }
            fWidthLine1 = g_pRenderEngine->textWidth(iFontLine1, szLine1);
            
            if ( (! bDRChanged) || (g_TimeNow >= s_uLastOSDRadioLinkInterfacesRecvDataratesChangeTimes[iLocalRadioLinkId][i] + g_uOSDElementChangeTimeout/2) ||
                 (bDRChanged && ((g_TimeNow/g_uOSDElementChangeBlinkInterval)%2)))
            {
               if ( bHorizontal )
                  g_pRenderEngine->drawText(xPos, yPos + dyLine1 + dySignalBars + osd_get_link_bars_height(1.0), iFontLine1, szLine1);
               else
               {
                  g_pRenderEngine->drawText(xStart + (osd_getVerticalBarWidth() - fWidthLine1)*0.5, yPos + dyLine1 + dySignalBars, iFontLine1, szLine1);
                  yPos += height_text_small;
                  fTotalHeightLink += height_text_small;             
               }
            }
            if ( bShowLine1AsError || bDRChanged )
               osd_set_colors();

            if ( bShowInterfacesExtended )
            {
               char szCardName[128];
               controllerGetCardUserDefinedNameOrType(pRadioHWInfo, szCardName);
          
               if ( NULL != pRadioHWInfo )
                  snprintf(szLine1, sizeof(szLine1)/sizeof(szLine1[0]), "%s%s", (controllerIsCardInternal(pRadioHWInfo->szMAC)?"":"(Ext) "), szCardName );
               else
                  strcpy(szLine1, szCardName);
                 
               float fWidthLine1 = g_pRenderEngine->textWidth(g_idFontOSDExtraSmall, szLine1);
               if ( bHorizontal )
                  g_pRenderEngine->drawText(xPos, yPos + dySignalBars + osd_get_link_bars_height(1.0) + height_text_small, g_idFontOSDExtraSmall, szLine1);
               else
               {
                  g_pRenderEngine->drawText(xStart + (osd_getVerticalBarWidth() - fWidthLine1)*0.5, yPos + dySignalBars, g_idFontOSDExtraSmall, szLine1);
                  yPos += height_text_extra_small;
                  fTotalHeightLink += height_text_extra_small;             
               }
            }
            iCountShown++;
         }
      }
   }

   if ( bHorizontal )
   {
      xPos -= fWidthTag;
      fTotalWidthLink += fWidthTag;
      _osd_render_local_radio_link_tag_new(xStart-fTotalWidthLink-2.0*fMarginX,yStart, iLocalRadioLinkId, bHorizontal, true);
   }

   double* pC = get_Color_OSDText();
   g_pRenderEngine->setFill(0,0,0,0.1);
   g_pRenderEngine->setStroke(pC[0], pC[1], pC[2], 0.7);
   //g_pRenderEngine->setStroke(255,50,50,1);
   g_pRenderEngine->setStrokeSize(2.0);

   if ( bHorizontal )
      g_pRenderEngine->drawRoundRect(xStart-fTotalWidthLink-2.0*fMarginX, yStart, fTotalWidthLink + 2.0*fMarginX, fTotalHeightLink + 2.0*fMarginY, 0.003*osd_getScaleOSD());
   else
      g_pRenderEngine->drawRoundRect(xStart, yStart, osd_getVerticalBarWidth() - 8.0*g_pRenderEngine->getPixelWidth(), fTotalHeightLink + 2.0*fMarginY, 0.003*osd_getScaleOSD());
   osd_set_colors();

   if ( bHorizontal )
      return fTotalWidthLink + 2.0 * fMarginX;
   else
      return fTotalHeightLink + 2.0 * fMarginY;
}

