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
#include "../base/hardware.h"
#include "../base/hardware_radio.h"
#include "../base/config.h"
#include "../base/config_video.h"
#include "../base/models.h"
#include "../base/ctrl_interfaces.h"
#include "../common/string_utils.h"

#include "../renderer/render_engine.h"

#include "osd.h"
#include "osd_common.h"
#include "colors.h"
#include "osd_links.h"
#include "shared_vars.h"
#include "timers.h"

float _osd_get_radio_link_new_height()
{
   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   float fHeightLink = osd_get_link_bars_height(1.0);

   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_RADIO_INTERFACES_INFO )
      fHeightLink += height_text_small;

   if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_RADIO_LINK_INTERFACES_EXTENDED )
      fHeightLink += height_text_small;
   return fHeightLink;
}


float osd_render_radio_link_tag_new(float xPos, float yPos, int iRadioLink, bool bHorizontal, bool bDraw)
{
   char szBuff1[32];
   char szBuff2[32];
   char szBuff3[32];

   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   snprintf(szBuff1, sizeof(szBuff1), "Link %d", iRadioLink+1);

   strcpy(szBuff2, str_format_frequency_no_sufix(g_pCurrentModel->radioLinksParams.link_frequency[iRadioLink]));
   
   szBuff3[0] = 0;
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      strcpy(szBuff3, "RELAY");
      strcpy(szBuff2, str_format_frequency_no_sufix(g_pCurrentModel->relay_params.uRelayFrequency));
   }
   float fWidthLink = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, szBuff1);
   float fWidthFreq = g_pRenderEngine->textWidth(0.0, g_idFontOSD, szBuff2);
   float fWidthRelay = g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff3);

   if ( fWidthFreq > fWidthLink )
      fWidthLink = fWidthFreq;
   if ( fWidthRelay > fWidthLink )
      fWidthLink = fWidthRelay;

   fWidthLink += 0.5*height_text_small;
   float fHeight = 1.1 * height_text_small + height_text;
   
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      fHeight += 1.0*height_text_small;

   if ( bDraw )
   {
      g_pRenderEngine->drawBackgroundBoundingBoxes(false);

      double* pC = get_Color_OSDText();
      g_pRenderEngine->setFill(pC[0], pC[1], pC[2], 0.3);
      g_pRenderEngine->setStroke(0,0,0,0);
      g_pRenderEngine->drawRoundRect(xPos+1.0*g_pRenderEngine->getPixelWidth(), yPos + 1.0*g_pRenderEngine->getPixelHeight(), fWidthLink, fHeight, 0.003*osd_getScaleOSD());

      osd_set_colors();

      float fWidth = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, szBuff1);
      g_pRenderEngine->drawText(xPos+(fWidthLink-fWidth)*0.5, yPos+height_text_small*0.05, 0.0, g_idFontOSDSmall, szBuff1);

      fWidth = g_pRenderEngine->textWidth(0.0, g_idFontOSD, szBuff2);
      g_pRenderEngine->drawText(xPos+(fWidthLink-fWidth)*0.5, yPos+height_text_small*0.9, 0.0, g_idFontOSD, szBuff2);

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      {
         fWidth = g_pRenderEngine->textWidth(g_idFontOSDSmall, szBuff3);
         g_pRenderEngine->drawText(xPos+(fWidthLink-fWidth)*0.5, yPos+height_text_small*1.2 + height_text*0.7, 0.0, g_idFontOSDSmall, szBuff3);
      }

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         fWidth = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, "DIS");
         g_pRenderEngine->drawText(xPos+(fWidthLink-fWidth)*0.5, yPos+1.2*height_text_small + height_text, 0.0, g_idFontOSDSmall, "DIS");
      }

      if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_BACKGROUND_ON_TEXTS_ONLY )
         g_pRenderEngine->drawBackgroundBoundingBoxes(true);
   }

   if ( bHorizontal )
      return fWidthLink;
   else
      return fHeight;
}

float _osd_show_radio_bars_info(float xPos, float yPos, int iMaxRxQuality, int iDataRate, bool bShowBars, bool bShowPercentages, bool bUplink, bool bHorizontal, bool bDraw)
{
   if ( ! bShowPercentages )
   if ( ! bShowBars )
      return 0.0;

   float height_text = osd_getFontHeight();
   float height_text_small = osd_getFontHeightSmall();

   float fTotalWidth = 0.0;
   float fTotalHeight = osd_get_link_bars_height(1.0);

   if ( bShowBars )
   {
       float fSize = osd_get_link_bars_width(1.0);
       if ( bDraw )
          osd_show_link_bars(xPos+fSize,yPos, iMaxRxQuality, 1.0);
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


   if ( bShowPercentages )
   {
      char szBuff[32];
      char szBuff2[32];
      snprintf(szBuff, sizeof(szBuff), "%d%%", iMaxRxQuality);
      if ( iDataRate >= 0 )
         snprintf(szBuff2, sizeof(szBuff2), "%d Mb", iDataRate );
      else
         snprintf(szBuff2, sizeof(szBuff2),"MCS-%d", -iDataRate-1);
      osd_set_colors();
      float fWidthPercentage = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, "1999%");
      float fWidthDataRate = g_pRenderEngine->textWidth(0.0, g_idFontOSDSmall, "99 Mb");
      if ( fWidthDataRate > fWidthPercentage )
         fWidthPercentage = fWidthDataRate;
      if ( bDraw )
      {
         g_pRenderEngine->drawText(xPos, yPos, 0.0, g_idFontOSDSmall, szBuff);
         g_pRenderEngine->drawText(xPos, yPos+height_text_small, 0.0, g_idFontOSDSmall, szBuff2);
      }
      fTotalWidth += fWidthPercentage;
   }
   if ( bHorizontal )
      return fTotalWidth;
   else
      return fTotalHeight;
}


float osd_show_radio_link_new(float xPos, float yPos, int iRadioLink, bool bHorizontal)
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

   float fWidthTag = osd_render_radio_link_tag_new(xPos,yPos, iRadioLink, true, false);
   float fHeightTag = osd_render_radio_link_tag_new(xPos,yPos, iRadioLink, false, false);

   float fTotalWidthLink = 0.0;
   float fTotalHeightLink = fHeightTag;
   float fHeightLink = _osd_get_radio_link_new_height();

   if ( ! bHorizontal )
      fTotalHeightLink = 0.0;

   if ( bHorizontal )
   if ( fHeightLink > fTotalHeightLink )
      fTotalHeightLink = fHeightLink;

   bool bShowVehicle = false;
   bool bShowController = false;
   bool bShowBars = false;
   bool bShowPercentages = false;

   bool bShowSummary = true;
   bool bShowInterfaces = false;
   bool bShowInterfacesExtended = false;

   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_RADIO_LINKS )
      bShowController = true;

   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS )
      bShowVehicle = true;

   if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_RADIO_LINK_QUALITY_BARS )
      bShowBars = true;

   if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_RADIO_LINK_QUALITY_PERCENTAGE )
      bShowPercentages = true;

   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_RADIO_INTERFACES_INFO )
   {
      bShowInterfaces = true;
      bShowSummary = false;
   }

   if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_RADIO_LINK_INTERFACES_EXTENDED )
   {
      bShowInterfaces = true;
      bShowInterfacesExtended = true;
      bShowSummary = false;    
   }

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      bShowVehicle = true;
      bShowController = false;
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
      osd_render_radio_link_tag_new(xStart,yStart, iRadioLink, bHorizontal, true);
      fTotalHeightLink += fHeightTag;
      yPos += fHeightTag;
   }

   float dySignalBars = 0.0;
   if ( (! bShowInterfaces) && (! bShowInterfacesExtended) )
       dySignalBars = height_text_small*0.3;
   else
      dySignalBars = height_text_small*0.1;

   if ( bShowVehicle )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
   {
      if ( bShowSummary )
      {
         int nRxQuality = 0;
         int iDataRate = 0;
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != iRadioLink )
               continue;
            if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_link_quality[i] > nRxQuality )
               nRxQuality = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_link_quality[i];
            if ( g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0] < 0 )
                iDataRate = g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0];
            else if ( g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_datarate[i]/2 > iDataRate )
               iDataRate = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_datarate[i]/2;
         }
         bool bAsUplink = true;
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
            bAsUplink = false;
         float fSize = _osd_show_radio_bars_info(xPos, yPos+dySignalBars, nRxQuality, iDataRate, bShowBars, bShowPercentages, bAsUplink, bHorizontal, false);
         if ( bHorizontal )
         {
            xPos -= fSize;
            fTotalWidthLink += fSize;
         }
         else if ( fSize > fTotalWidthLink )
            fTotalWidthLink = fSize;

         _osd_show_radio_bars_info(xPos, yPos+dySignalBars, nRxQuality, iDataRate, bShowBars, bShowPercentages, bAsUplink, bHorizontal, true);
         if ( ! bHorizontal )
         {
             yPos += fSize;
             fTotalHeightLink += fSize;
         }
      }
      else // Not summary
      {
         int iCountShown = 0;
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != iRadioLink )
               continue;
            int nRxQuality = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_link_quality[i];
            int iDataRate = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_datarate[i]/2;
            if ( g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0] < 0 )
                iDataRate = g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0];
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
            if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
               bAsUplink = false;
         
            float fSize = _osd_show_radio_bars_info(xPos, yPos+dySignalBars, nRxQuality, iDataRate, bShowBars, bShowPercentages, bAsUplink, bHorizontal, false);
            if ( bHorizontal )
            {
               xPos -= fSize;
               fTotalWidthLink += fSize;
            }
            else if ( fSize > fTotalWidthLink )
               fTotalWidthLink = fSize;
            _osd_show_radio_bars_info(xPos, yPos+dySignalBars, nRxQuality, iDataRate, bShowBars, bShowPercentages, bAsUplink, bHorizontal, true);
            if ( ! bHorizontal )
            {
                yPos += fSize;
                fTotalHeightLink += fSize;
            }

            char szLine1[128];
            bool bShowLine1AsError = false;
            snprintf(szLine1, sizeof(szLine1), "%s: RX/TX", g_pCurrentModel->radioInterfacesParams.interface_szPort[i] );
            int dbm = g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.uplink_rssi_dbm[i]-200;
            if ( dbm < -110 )
            {
               strlcat(szLine1, " N/A dBm", sizeof(szLine1));
               bShowLine1AsError = true;
            }
            else
            {
               char szTmp[32];
               snprintf(szTmp, sizeof(szTmp), " %d dBm", dbm);
               strlcat(szLine1, szTmp, sizeof(szLine1));
            }

            if ( bShowLine1AsError )
               g_pRenderEngine->setColors(get_Color_IconError());

            float fWidthLine1 = g_pRenderEngine->textWidth(g_idFontOSDExtraSmall, szLine1);
            int iFontLine1 = g_idFontOSDSmall;
            float dyLine1 = 0.0;
            if ( fWidthLine1 > fSize*0.8 )
            {
               iFontLine1 = g_idFontOSDExtraSmall;
               dyLine1 = (height_text_small - height_text_extra_small)*0.6;
            }
            fWidthLine1 = g_pRenderEngine->textWidth(iFontLine1, szLine1);
            
            if ( bHorizontal )
               g_pRenderEngine->drawText(xPos + (fSize - fWidthLine1)*0.5, yPos + dyLine1 + dySignalBars + osd_get_link_bars_height(1.0), 0.0, iFontLine1, szLine1);
            else
            {
               g_pRenderEngine->drawText(xStart + (osd_getVerticalBarWidth() - fWidthLine1)*0.5, yPos + dyLine1 + dySignalBars, 0.0, iFontLine1, szLine1);
               yPos += height_text_small;
               fTotalHeightLink += height_text_small;             
            }
            if ( bShowLine1AsError )
               osd_set_colors();

            iCountShown++;
         }
      }
   }

   if ( bShowVehicle && bShowController )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
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

   if ( bShowController )
   if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
   {
      int iCountInterfacesForCurrentLink = 0;
      if ( iRadioLink >= 0 )
      {
         for( int k=0; k<g_pSM_RadioStats->countRadioInterfaces; k++ )
            if ( g_pSM_RadioStats->radio_interfaces[k].assignedRadioLinkId == iRadioLink )
               iCountInterfacesForCurrentLink++;
      }

      if ( bShowSummary )
      {
         int nRxQuality = 0;
         int iDataRate = 0;
         int iDataRateData = 0;

         for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
         {
            if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId != iRadioLink )
               continue;
            if ( g_pSM_RadioStats->radio_interfaces[i].rxQuality > nRxQuality )
               nRxQuality = g_pSM_RadioStats->radio_interfaces[i].rxQuality;
            if ( g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0] < 0 )
                iDataRate = g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0];
            else
            {
               if ( g_pSM_RadioStats->radio_interfaces[i].lastDataRateVideo/2 > iDataRate )
                 iDataRate = g_pSM_RadioStats->radio_interfaces[i].lastDataRateVideo/2;
               if ( g_pSM_RadioStats->radio_interfaces[i].lastDataRateData/2 > iDataRateData )
                 iDataRateData = g_pSM_RadioStats->radio_interfaces[i].lastDataRateData/2;
            }
         }

         if ( iDataRate == 0 )
            iDataRate = iDataRateData;

         float fSize = _osd_show_radio_bars_info(xPos, yPos+dySignalBars, nRxQuality, iDataRate, bShowBars, bShowPercentages, false, bHorizontal, false);
         if ( bHorizontal )
         {
            xPos -= fSize;
            fTotalWidthLink += fSize;
         }
         else if ( fSize > fTotalWidthLink )
            fTotalWidthLink = fSize;

         _osd_show_radio_bars_info(xPos, yPos+dySignalBars, nRxQuality, iDataRate, bShowBars, bShowPercentages, false, bHorizontal, true);
         if ( ! bHorizontal )
         {
             yPos += fSize;
             fTotalHeightLink += fSize;
         }
      }
      else // Not summary
      {
         int iCountShown = 0;
         for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
         {
            if ( g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId != iRadioLink )
               continue;
            int nRxQuality = g_pSM_RadioStats->radio_interfaces[i].rxQuality;
            int iDataRate = g_pSM_RadioStats->radio_interfaces[i].lastDataRateVideo/2;
            if ( 0 == iDataRate )
               iDataRate = g_pSM_RadioStats->radio_interfaces[i].lastDataRateData/2;
              
            if ( g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0] < 0 )
                iDataRate = g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0];

            bool bIsTxCard = false;
            if ( iRadioLink >= 0 && g_pSM_RadioStats->radio_links[iRadioLink].lastTxInterfaceIndex == i && 1 < g_pSM_RadioStats->countRadioInterfaces )
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
            float fSize = _osd_show_radio_bars_info(xPos, yPos+dySignalBars, nRxQuality, iDataRate, bShowBars, bShowPercentages, false, bHorizontal, false);
            if ( bHorizontal )
            {
               xPos -= fSize;
               fTotalWidthLink += fSize;
            }
            else if ( fSize > fTotalWidthLink )
               fTotalWidthLink = fSize;
            _osd_show_radio_bars_info(xPos, yPos+dySignalBars, nRxQuality, iDataRate, bShowBars, bShowPercentages, false, bHorizontal, true);
            
            if ( bIsTxCard && (1<iCountInterfacesForCurrentLink) )
               g_pRenderEngine->drawIcon(xPos-height_text*0.1, yPos+dySignalBars, height_text*0.9/g_pRenderEngine->getAspectRatio() , height_text*0.9, g_idIconUplink2);

            if ( ! bHorizontal )
            {
                yPos += fSize;
                fTotalHeightLink += fSize;
            }

            char szLine1[128];
            bool bShowLine1AsError = false;
            radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);

            if ( g_pSM_RadioStats->radio_links[iRadioLink].lastTxInterfaceIndex == i )
               snprintf(szLine1, sizeof(szLine1), "%s: RX/TX", pNICInfo->szUSBPort );
            else if ( g_pSM_RadioStats->radio_interfaces[i].openedForRead )
               snprintf(szLine1, sizeof(szLine1), "%s: RX", pNICInfo->szUSBPort );
            else if ( g_pSM_RadioStats->radio_interfaces[i].openedForWrite )
               snprintf(szLine1, sizeof(szLine1), "%s: --", pNICInfo->szUSBPort );
            else
               snprintf(szLine1, sizeof(szLine1), "%s: N/A", pNICInfo->szUSBPort );

            if ( g_fOSDDbm[i] < -110 )
            {
               strlcat(szLine1, " N/A dBm", sizeof(szLine1));
               bShowLine1AsError = true;
            }
            else
            {
               char szTmp[32];
               snprintf(szTmp, sizeof(szTmp), " %d dBm", (int)g_fOSDDbm[i]);
               strlcat(szLine1, szTmp, sizeof(szLine1));
            }

            if ( bShowLine1AsError )
               g_pRenderEngine->setColors(get_Color_IconError());

            float fWidthLine1 = g_pRenderEngine->textWidth(g_idFontOSDExtraSmall, szLine1);
            int iFontLine1 = g_idFontOSDSmall;
            float dyLine1 = 0.0;
            if ( fWidthLine1 > fSize*0.8 )
            {
               iFontLine1 = g_idFontOSDExtraSmall;
               dyLine1 = (height_text_small - height_text_extra_small)*0.6;
            }
            fWidthLine1 = g_pRenderEngine->textWidth(iFontLine1, szLine1);
            
            if ( bHorizontal )
               g_pRenderEngine->drawText(xPos + (fSize - fWidthLine1)*0.5, yPos + dyLine1 + dySignalBars + osd_get_link_bars_height(1.0), 0.0, iFontLine1, szLine1);
            else
            {
               g_pRenderEngine->drawText(xStart + (osd_getVerticalBarWidth() - fWidthLine1)*0.5, yPos + dyLine1 + dySignalBars, 0.0, iFontLine1, szLine1);
               yPos += height_text_small;
               fTotalHeightLink += height_text_small;             
            }
            if ( bShowLine1AsError )
               osd_set_colors();

            if ( bShowInterfacesExtended )
            {
               char szCardName[128];
               szCardName[0] = 0;
               char* szN = controllerGetCardUserDefinedName(pNICInfo->szMAC);
               if ( NULL != szN && 0 != szN[0] )
                  strcpy(szCardName, szN);
               else
               {
                  t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
                  if ( NULL != pCardInfo )
                  {
                     const char* szCardModel = str_get_radio_card_model_string(pCardInfo->cardModel);
                     if ( NULL != szCardModel && 0 != szCardModel[0] )
                        strcpy(szCardName, szCardModel);
                  }
               }
               if ( 0 == szCardName[0] )
                  strcpy(szCardName, "Generic");
               snprintf(szLine1, sizeof(szLine1), "%s%s", (controllerIsCardInternal(pNICInfo->szMAC)?"":"(Ext) "), szCardName );

               float fWidthLine1 = g_pRenderEngine->textWidth(0, g_idFontOSDExtraSmall, szLine1);
               if ( bHorizontal )
                  g_pRenderEngine->drawText(xPos + (fSize - fWidthLine1)*0.5, yPos + dySignalBars + osd_get_link_bars_height(1.0) + height_text_small, g_idFontOSDExtraSmall, szLine1);
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
      osd_render_radio_link_tag_new(xStart-fTotalWidthLink-2.0*fMarginX,yStart, iRadioLink, bHorizontal, true);
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

