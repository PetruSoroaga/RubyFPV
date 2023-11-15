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
#include "menu.h"
#include "menu_objects.h"
#include "menu_text.h"
#include "menu_system_all_params.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "launchers_controller.h"

#include "../renderer/render_engine.h"
#include "pairing.h"
#include "colors.h"

#include "shared_vars.h"
#include "popup.h"


MenuSystemAllParams::MenuSystemAllParams(void)
:Menu(MENU_ID_SYSTEM_ALL_PARAMS, "All System Parameters (controller + vehicle)", NULL)
{
   m_Width = 0.8;
   m_Height = 0.91;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.05;
   m_bGotIP = false;
   m_szIP[0] = 0;
}

void MenuSystemAllParams::onShow()
{
   Menu::onShow();

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL != pCS && (0 != pCS->iDeveloperMode) )
      setTitle("All System Parameters (controller + vehicle), Developer Mode");
}


void MenuSystemAllParams::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   FILE* fd = NULL;
   char szBuff[256];
   char szFile[256];
   char szOutput[1024];

   int boardType = 0;
   int wifiType = 0;

   float height_text = 0.9*MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();   

   float xPos = m_xPos+MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   float yPos = y;
   float width = m_Width*menu_getScaleMenus()-2*MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   float height = m_RenderHeight-2*MENU_MARGINS*menu_getScaleMenus();

   snprintf(szFile, sizeof(szFile), "%s/board.txt", FOLDER_CONFIG);
   fd = fopen(szFile,"r");
   if ( NULL != fd )
   {
      fscanf(fd,"%d", &boardType);
      fclose(fd);
   }
         
   snprintf(szBuff, sizeof(szBuff), "Controller board: %s, no vehicle selected.", str_get_hardware_board_name(boardType));
   if ( NULL != g_pCurrentModel )
      snprintf(szBuff, sizeof(szBuff), "Controller board: %s, vehicle board: %s, vehicle name: %s, %s", str_get_hardware_board_name(boardType), str_get_hardware_board_name(g_pCurrentModel->board_type), g_pCurrentModel->getLongName(), g_pCurrentModel->is_spectator?"(Spectator Mode)":"(Control Mode)");

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.3);
   g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontMenu, szBuff);
   yPos += 1.9*height_text;
   g_pRenderEngine->setColors(get_Color_MenuText());


   float yPosCol2 = yPos;

   if ( ! m_bGotIP )
   {
      m_szIP[0] = 0;
      if ( boardType != BOARD_TYPE_PIZERO && boardType != BOARD_TYPE_PIZEROW && boardType != BOARD_TYPE_PI3APLUS )
      {
      szOutput[0] = 0;
      hw_execute_bash_command_raw("ifconfig | grep -A1 eth | head -2 | tail -1", szOutput);
      log_line("Out: [%s]", szOutput);
      int cSpaces = 0;
      int cOther = 0;
      for( int i=0; i<strlen(szOutput); i++ )
      {
         if ( szOutput[i] != ' ' )
            cOther++;

         if ( szOutput[i] == ' ' )
         if ( cOther )
            cSpaces++;
         if ( szOutput[i] == 10 || szOutput[i] == 13 || cSpaces >= 2 )
         {
            szOutput[i] = 0;
            break;
         }
      }
      log_line("Out: [%s]", szOutput);
      char* p = &(szOutput[0]);
      while ( *p == ' ' && *p != 0 )
         p++;
      if ( *p != 0 && strlen(p)>4 )
         strcpy(m_szIP, p+4);
      m_bGotIP = true;
      }
      else
         strcpy(m_szIP, "No ETH");
      m_bGotIP = true;
   }
   snprintf(szBuff, sizeof(szBuff), "Controller IP: %s", m_szIP);
   g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontMenu, szBuff);
   yPos += 2.0*height_text;

   yPos += renderRadioInfo(xPos, yPos, width, height, 0.9);
   yPos += renderDataRates(xPos, yPos, width, 0.8);
   yPos += renderProcesses(xPos, yPos, width, 0.8);
   yPos += renderSoftware(xPos, yPos, width, 0.8);


   float fScale = 0.86;
   float dxCol = 0.35*menu_getScaleMenus();

   g_pRenderEngine->drawLine(xPos+dxCol - 0.02*menu_getScaleMenus(), yPosCol2, xPos+dxCol - 0.02*menu_getScaleMenus(), m_RenderHeight-(yPosCol2-m_yPos+0.04*menu_getScaleMenus()));
   yPosCol2 += renderVehicleCamera(xPos + dxCol, yPosCol2, width - dxCol, fScale);
   yPosCol2 += renderVehicleRC(xPos + dxCol, yPosCol2, width - dxCol, fScale);
   yPosCol2 += renderControllerParams(xPos + dxCol, yPosCol2, width - dxCol, fScale);
   yPosCol2 += renderCPUParams(xPos + dxCol, yPosCol2, width - dxCol, fScale);
   yPosCol2 += renderDeveloperFlags(xPos + dxCol, yPosCol2, width - dxCol, fScale);
}

float MenuSystemAllParams::renderVehicleCamera(float xPos, float yPos, float width, float fScale)
{
   if ( NULL == g_pCurrentModel )
      return 0.0;
   float y0 = yPos;
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus()*fScale;
   
   char szCamera[256];
   char szVideo[256];
   char szBuff[1024];
   char szTemp[1024];

   g_pCurrentModel->getCameraFlags(szCamera);
   g_pCurrentModel->getVideoFlags(szVideo, g_pCurrentModel->video_params.user_selected_video_link_profile, NULL);
   snprintf(szBuff, sizeof(szBuff), "Vehicle camera flags: %s", szCamera );

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   snprintf(szBuff, sizeof(szBuff), "Vehicle video flags: %s", szVideo );

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 1.0 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   snprintf(szBuff, sizeof(szBuff), "EC: %d/%d/%d", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_packets, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].block_fecs, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].packet_length);

   g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);

   strcpy(szTemp, "[N/A]");
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_BEST_PERF )
      strcpy(szTemp, "[HP]");
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_HIGH_QUALITY )
      strcpy(szTemp, "[HQ]");
   if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_USER )
      strcpy(szTemp, "[USR]");

   snprintf(szBuff, sizeof(szBuff), "S: %s/%d/%d/%d", szTemp, 
              (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS)?1:0,
              0,
              (((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags)>>8)&0xFF)*5);
   yPos += g_pRenderEngine->drawMessageLines(xPos+ 0.16*menu_getScaleMenus(), yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   snprintf(szBuff, sizeof(szBuff), "H264: %d/%d/%d", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264profile, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264level);
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   g_pRenderEngine->setColors(get_Color_MenuText());

   yPos += 0.8 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING) * fScale;
   return yPos-y0;
}

float MenuSystemAllParams::renderVehicleRC(float xPos, float yPos, float width, float fScale)
{
   if ( NULL == g_pCurrentModel )
      return 0.0;
   float y0 = yPos;
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus()*fScale;

   char szBuff[1024];
   char szTemp[1024];

   snprintf(szBuff, sizeof(szBuff), "RC: %s, ch %d / fps %d / output enabled: %d / fs %d ms", g_pCurrentModel->rc_params.rc_enabled?"Enabled":"Disabled", g_pCurrentModel->rc_params.channelsCount, g_pCurrentModel->rc_params.rc_frames_per_second, (g_pCurrentModel->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED), g_pCurrentModel->rc_params.rc_failsafe_timeout_ms);
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   snprintf(szBuff, sizeof(szBuff), "RC Failsafe Type: %d, (value: %d)", (g_pCurrentModel->rc_params.failsafeFlags & 0xFF), ((g_pCurrentModel->rc_params.failsafeFlags >> 8) & 0xFFFF));
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   bool bSI = false;
   if ( g_pCurrentModel->osd_params.show_stats_rc || (g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_HID_IN_OSD) )
      bSI = true;
   snprintf(szBuff, sizeof(szBuff), "RC: send info back: %s, show on OSD: %d/%d", bSI?"Yes":"No", g_pCurrentModel->osd_params.show_stats_rc, (g_pCurrentModel->osd_params.osd_flags[0] & OSD_FLAG_SHOW_HID_IN_OSD));
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, "Telemetry", height_text*1.3, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale * 1.3;

   char szTelemetryType[64];
   snprintf(szTelemetryType, sizeof(szTelemetryType), "%d", g_pCurrentModel->telemetry_params.fc_telemetry_type);
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_NONE )
      strcpy(szTelemetryType, "None");
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_MAVLINK )
      strcpy(szTelemetryType, "MAVLink");
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_LTM )
      strcpy(szTelemetryType, "LTM");
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == TELEMETRY_TYPE_TRANSPARENT )
      strcpy(szTelemetryType, "Transp.");
   snprintf(szBuff, sizeof(szBuff), "  %s / rate %d / duplex: in: %s / out: %s", szTelemetryType, g_pCurrentModel->telemetry_params.update_rate, g_pCurrentModel->telemetry_params.bControllerHasInputTelemetry?"yes":"no", g_pCurrentModel->telemetry_params.bControllerHasOutputTelemetry?"yes":"no");

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   if ( NULL != g_pCurrentModel )
   {
      snprintf(szBuff, sizeof(szBuff), "  Vehicle MAVLink Id: %d, Controller MAVLink Id: %d", g_pCurrentModel->telemetry_params.vehicle_mavlink_id, g_pCurrentModel->telemetry_params.controller_mavlink_id );
      yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;
   }

   g_pRenderEngine->setColors(get_Color_MenuText());

   yPos += 0.8 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING) * fScale;
   return yPos - y0;
}

float MenuSystemAllParams::renderRadioInfo(float xPos, float yPos, float width, float height, float fScale)
{
   char szBuff[256];
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus() * fScale;   

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, height_text*1.2, g_idFontMenu, "Controller Radio Interfaces:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      snprintf(szBuff, sizeof(szBuff), "Radio Interface %d: USB port %s: %s, driver: %s", i+1, pNICInfo->szUSBPort, pNICInfo->szDescription, str_get_radio_driver_description(pNICInfo->typeAndDriver));
      g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontMenu, szBuff);
      yPos += 1.4*height_text;

      if ( pNICInfo->lastFrequencySetFailed )
         snprintf(szBuff, sizeof(szBuff), "   Now at: 0 Mhz, bands:");
      else
         snprintf(szBuff, sizeof(szBuff), "   Now at: %s, bands:", str_format_frequency(pNICInfo->uCurrentFrequency));
      if ( pNICInfo->supportedBands & 1 )
         strlcat(szBuff, "2.3, ", sizeof(szBuff));
      if ( pNICInfo->supportedBands & 2 )
         strlcat(szBuff, "2.4, ", sizeof(szBuff));
      if ( pNICInfo->supportedBands & 4 )
         strlcat(szBuff, "2.5, ", sizeof(szBuff));
      if ( pNICInfo->supportedBands & 8 )
         strlcat(szBuff, "5.8, ", sizeof(szBuff));
      
      g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontMenu, szBuff);
      yPos += 1.4*height_text;

      snprintf(szBuff, sizeof(szBuff), "   Flags: ");
      if ( controllerIsCardTXOnly(pNICInfo->szMAC) )
         strlcat(szBuff, "TX Only, ", sizeof(szBuff));
      else if ( controllerIsCardRXOnly(pNICInfo->szMAC) )
         strlcat(szBuff, "RX Only, ", sizeof(szBuff));
      else
         strlcat(szBuff, "TX/RX, ", sizeof(szBuff));
      
      if ( controllerIsCardDisabled(pNICInfo->szMAC) )
         strlcat(szBuff, "Disabled", sizeof(szBuff));
      else
         strlcat(szBuff, "Enabled", sizeof(szBuff));

      if ( pNICInfo->openedForWrite )
         strlcat(szBuff, ", [Current TX Card]", sizeof(szBuff));
      if ( controllerIsCardTXPreferred(pNICInfo->szMAC) )
         strlcat(szBuff, ", [Preffered TX Card]", sizeof(szBuff));
      g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontMenu, szBuff);
      yPos += 1.8*height_text;
   }

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);

   g_pRenderEngine->drawText(xPos, yPos, height_text*1.2, g_idFontMenu, "Vehicle Radio Interfaces:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( NULL == g_pCurrentModel )
   {
      g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontMenu, "No vehicle selected.");
      yPos += 1.4*height_text;
   }
   else
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            snprintf(szBuff, sizeof(szBuff), "Radio Interface %d: USB port %s,  %s, driver %s", i+1, g_pCurrentModel->radioInterfacesParams.interface_szPort[i], str_get_radio_type_description(g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i]), str_get_radio_driver_description(g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i]));
            g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontMenu, szBuff);
            yPos += 1.4*height_text;
            snprintf(szBuff, sizeof(szBuff), "   Now at %s, bands: ", str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i]));
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_23 )
               strlcat(szBuff, "2.3 ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_24 )
               strlcat(szBuff, "2.4 ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_25 )
               strlcat(szBuff, "2.5 ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_58 )
               strlcat(szBuff, "5.8 ", sizeof(szBuff));
 
            g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontMenu, szBuff);
            yPos += 1.4*height_text;

            snprintf(szBuff, sizeof(szBuff), "   Flags: ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
               strlcat(szBuff, "TX/RX, ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
            if ( !( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
               strlcat(szBuff, "TX Only, ", sizeof(szBuff));
            if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
               strlcat(szBuff, "RX Only, ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
               strlcat(szBuff, "Video, ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
               strlcat(szBuff, "Data, ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
               strlcat(szBuff, "Relay", sizeof(szBuff));

            g_pRenderEngine->drawText(xPos, yPos, height_text, g_idFontMenu, szBuff);
            yPos += 1.8*height_text;
         }

   return 5.0*height_text + (1.8*height_text+ 2*1.4*height_text)*(2+3);
}

float MenuSystemAllParams::renderDataRates(float xPos, float yPos, float width, float fScale)
{
   ControllerSettings* pCS = get_ControllerSettings();
   
   float y0 = yPos;
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus() * fScale;   

   char szBuff[1024];
   char szTemp[1024];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, height_text*1.2, g_idFontMenu, "Radio data rates and power:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   snprintf(szBuff, sizeof(szBuff), "Controller TX power: %d", pCS->iTXPower);

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   snprintf(szBuff, sizeof(szBuff), "Controller radio datarates: ");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      int dataRate = controllerGetCardDataRate(pNICInfo->szMAC);
      snprintf(szTemp, sizeof(szTemp), "%d: ", i);
      strlcat(szBuff, szTemp, sizeof(szBuff));
      if ( dataRate == 0 )
         strlcat(szBuff, "Same as vehicle", sizeof(szBuff));
      else if ( dataRate < 0 )
      {
         snprintf(szTemp, sizeof(szTemp), "MCS-%d (%d Mbps)", -dataRate-1, getRealDataRateFromMCSRate(-dataRate-1)/1000/1000);
         strlcat(szBuff, szTemp, sizeof(szBuff));
      }
      else
      {
         snprintf(szTemp, sizeof(szTemp), "%d Mbps", dataRate);
         strlcat(szBuff, szTemp, sizeof(szBuff));
      }
      if ( i < hardware_get_radio_interfaces_count()-1 )
         strlcat(szBuff, ", ", sizeof(szBuff));
   }

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   if ( NULL == g_pCurrentModel )
      return 0.0;

   snprintf(szBuff, sizeof(szBuff), "Vehicle TX power: %d", g_pCurrentModel->radioInterfacesParams.txPower);

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   snprintf(szBuff, sizeof(szBuff), "Vehicle radio datarates: ");
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( i != 0 )
         strlcat(szBuff, ", ", sizeof(szBuff));
      if ( g_pCurrentModel->radioInterfacesParams.interface_datarates[i][0] < 0 )
      {
         snprintf(szTemp, sizeof(szTemp), "MCS-%d (%d Mbps), STBC: %s", -g_pCurrentModel->radioInterfacesParams.interface_datarates[i][0]-1, getRealDataRateFromMCSRate(-g_pCurrentModel->radioInterfacesParams.interface_datarates[i][0]-1)/1000/1000, (g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[i] & RADIO_FLAGS_STBC)?"Yes":"No");
         strlcat(szBuff, szTemp, sizeof(szBuff));
      }
      else
      {
         snprintf(szTemp, sizeof(szTemp), "%d Mbps", g_pCurrentModel->radioInterfacesParams.interface_datarates[i][0]);
         strlcat(szBuff, szTemp, sizeof(szBuff));
      }
   }
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   //snprintf(szBuff, sizeof(szBuff), "Vehicle adaptive video link: %s", (g_pCurrentModel->video_params.encoding_extra_flags & ENCODING_EXTRA_FLAG_AUTO_SWITCH_VIDEO_LINK_QUALITY)?"yes":"no");
   //yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   //yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   snprintf(szBuff, sizeof(szBuff), "Vehicle radio CTS: %s", (g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[0] & RADIO_FLAGS_FRAME_TYPE_RTS)?"Off":"On");

   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}

float MenuSystemAllParams::renderProcesses(float xPos, float yPos, float width, float fScale)
{
   ControllerSettings* pCS = get_ControllerSettings();
   
   float y0 = yPos;
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus() * fScale;   

   char szBuff[1024];
   char szBuff2[1024];

   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, height_text*1.2, g_idFontMenu, "Processes Priorities:");

   g_pRenderEngine->setColors(get_Color_MenuText());

   snprintf(szBuff, sizeof(szBuff), "Controller router/video/UI: %d/%d/%d", -pCS->iNiceRouter, -pCS->iNiceRXVideo, -pCS->iNiceCentral);
   if ( NULL == g_pCurrentModel )
      strlcpy(szBuff2, "No vehicle", sizeof(szBuff2));
   else
      snprintf(szBuff2, sizeof(szBuff2), "Vehicle router/video/telemetry/RC: %d/%d/%d/%d", -g_pCurrentModel->niceRouter, -g_pCurrentModel->niceVideo, -g_pCurrentModel->niceTelemetry, -g_pCurrentModel->niceRC);

   g_pRenderEngine->drawMessageLines(xPos+0.16*menu_getScaleMenus(), yPos, szBuff, height_text*1.2, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += g_pRenderEngine->drawMessageLines(xPos+0.41*menu_getScaleMenus(), yPos, szBuff2, height_text*1.2, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.2 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}


float MenuSystemAllParams::renderSoftware(float xPos, float yPos, float width, float fScale)
{
   ControllerSettings* pCS = get_ControllerSettings();

   float y0 = yPos;
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus() * fScale;

   char szBuff[64];
   char szBuff2[64];

   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, height_text*1.2, g_idFontMenu, "Software:");

   g_pRenderEngine->setColors(get_Color_MenuText());

   snprintf(szBuff, sizeof(szBuff), "Controller version %u.%u (b%u)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
   if ( NULL == g_pCurrentModel )
      strlcpy(szBuff2, "No vehicle", sizeof(szBuff2));
   else
      snprintf(szBuff2, sizeof(szBuff2), "Vehicle version: %u.%u (b%u)", (g_pCurrentModel->sw_version>>8) & 0xFF, (g_pCurrentModel->sw_version & 0xFF)/10, g_pCurrentModel->sw_version >> 16);

   g_pRenderEngine->drawMessageLines(xPos+0.16*menu_getScaleMenus(), yPos, szBuff, height_text*1.2, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += g_pRenderEngine->drawMessageLines(xPos+0.32*menu_getScaleMenus(), yPos, szBuff2, height_text*1.2, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.2 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}

float MenuSystemAllParams::renderDeveloperFlags(float xPos, float yPos, float width, float fScale)
{
   ControllerSettings* pCS = get_ControllerSettings();
   
   float y0 = yPos;
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus() * fScale;   

   char szBuff[1024];
   char szTemp[1024];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, height_text*1.2, g_idFontMenu, "Developer Flags:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   if ( NULL != g_pCurrentModel )
      snprintf(szBuff, sizeof(szBuff), "LiveLog: %d, RSFS: %d, RTW: %d ms", (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)?1:0, (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE)?1:0, (( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & 0xFF00 ) >> 8 ) * 5);
   else
      snprintf(szBuff, sizeof(szBuff), "No vehicle selected.");
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   if ( NULL != g_pCurrentModel )
   {
      strcpy(szBuff, "Clock Sync Type: ");
      if ( g_pCurrentModel->clock_sync_type == CLOCK_SYNC_TYPE_NONE )
         strlcat(szBuff, "None", sizeof(szBuff));
      if ( g_pCurrentModel->clock_sync_type == CLOCK_SYNC_TYPE_BASIC )
         strlcat(szBuff, "Basic", sizeof(szBuff));
      if ( g_pCurrentModel->clock_sync_type == CLOCK_SYNC_TYPE_ADV )
         strlcat(szBuff, "Advanced", sizeof(szBuff));
      yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
      yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;
   }
   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}

float MenuSystemAllParams::renderControllerParams(float xPos, float yPos, float width, float fScale)
{
   ControllerSettings* pCS = get_ControllerSettings();
   
   float y0 = yPos;
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus()*fScale;   

   char szBuff[1024];
   char szTemp[1024];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, height_text*1.2, g_idFontMenu, "Controller Params:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   snprintf(szBuff, sizeof(szBuff), "USB Tethering: Video: %s", pCS->iVideoForwardUSBType?"yes":"no");
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   snprintf(szBuff, sizeof(szBuff), "Telemetry Export: Out: %s, In: %s", pCS->iTelemetryOutputSerialPortIndex==-1?"No":"Yes", pCS->iTelemetryInputSerialPortIndex==-1?"No":"Yes");
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}

float MenuSystemAllParams::renderCPUParams(float xPos, float yPos, float width, float fScale)
{
   ControllerSettings* pCS = get_ControllerSettings();
   float y0 = yPos;
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus()*fScale;   

   char szBuff[1024];
   char szTemp[1024];

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText(), 0.6);
   g_pRenderEngine->drawText(xPos, yPos, height_text*1.2, g_idFontMenu, "CPU Params:");
   yPos += 2.0*height_text;

   g_pRenderEngine->setColors(get_Color_MenuText());

   snprintf(szBuff, sizeof(szBuff), "Controller: CPU: %d Mhz, GPU: %d Mhz, OverVoltage: %d, IOPriority: %d, %d", pCS->iFreqARM, pCS->iFreqGPU, pCS->iOverVoltage, pCS->ioNiceRouter, pCS->ioNiceRXVideo);
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   if ( NULL != g_pCurrentModel )
      snprintf(szBuff, sizeof(szBuff), "Vehicle: CPU: %d Mhz, GPU: %d Mhz, OverVoltage: %d, IOPriority: %d, %d", g_pCurrentModel->iFreqARM, g_pCurrentModel->iFreqGPU, g_pCurrentModel->iOverVoltage, g_pCurrentModel->ioNiceRouter, g_pCurrentModel->ioNiceVideo);
   else
      snprintf(szBuff, sizeof(szBuff), "Vehicle: None");
   yPos += g_pRenderEngine->drawMessageLines(xPos, yPos, szBuff, height_text, MENU_TEXTLINE_SPACING, width, g_idFontMenu);
   yPos += 0.4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING)*fScale;

   g_pRenderEngine->setColors(get_Color_MenuText());
   return yPos - y0;
}

void MenuSystemAllParams::onSelectItem()
{
   Menu::onSelectItem();
}

