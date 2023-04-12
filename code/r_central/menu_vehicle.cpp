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
#include "../base/models.h"
#include "../base/config.h"
#include "../base/launchers.h"
#include "../base/commands.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "render_commands.h"
#include "osd.h"
#include "osd_common.h"
#include "popup.h"
#include "menu.h"
#include "menu_vehicle.h"
#include "menu_vehicle_general.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_osd.h"
#include "menu_vehicle_osd_stats.h"
#include "menu_vehicle_osd_instruments.h"
#include "menu_vehicle_expert.h"
#include "menu_confirmation.h"
#include "menu_vehicle_telemetry.h"
#include "menu_vehicle_rc.h"
#include "menu_vehicle_relay.h"
#include "menu_vehicle_management.h"
#include "menu_vehicle_audio.h"
#include "menu_vehicle_data_link.h"
#include "menu_vehicle_functions.h"
#include "menu_vehicle_alarms.h"
#include "menu_vehicle_peripherals.h"
#include "menu_item_text.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include <ctype.h>
#include "handle_commands.h"
#include "warnings.h"
#include "notifications.h"

const char* s_szMenuVAlarmTemp = "Hardware alarm: temperature limit!";
const char* s_szMenuVAlarmThr = "Hardware alarm: Throttling!";
const char* s_szMenuVAlarmFreq = "Hardware alarm: Frequency capped!";
const char* s_szMenuVAlarmVoltage = "Hardware alarm: Undervoltage!";

MenuVehicle::MenuVehicle(void)
:Menu(MENU_ID_VEHICLE, "Vehicle Settings", NULL)
{
   m_Width = 0.17;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.16;
   m_fIconSize = 1.0;
   m_bWaitingForVehicleInfo = false;
}

MenuVehicle::~MenuVehicle()
{
}

void MenuVehicle::onShow()
{
   m_Height = 0.0;
   m_ExtraHeight = 0;
   
   addTopDescription();
   removeAllItems();

   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      addMenuItem(new MenuItemText("You are connected in spectator mode. Can't change vehicle settings."));

   m_IndexGeneral = addMenuItem(new MenuItem("General","Change general settings like name, type of vehicle and so on."));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexGeneral]->setEnabled(false);

   m_IndexAlarms = addMenuItem(new MenuItem("Warnings/Alarms","Change which alarms and warnings to enable/disable."));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexAlarms]->setEnabled(false);

   m_IndexOSD = addMenuItem(new MenuItem("OSD / Instruments","Change OSD type, layout and settings."));

   m_IndexCamera = addMenuItem(new MenuItem("Camera","Change camera parameters: brightness, contrast, sharpness and so on."));
   if ( NULL != g_pCurrentModel && (!g_pCurrentModel->hasCamera()) )
      m_pMenuItems[m_IndexCamera]->setEnabled(false);
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexCamera]->setEnabled(false);

   m_IndexVideo = addMenuItem(new MenuItem("Video","Change video resolution, fps so on."));
   if ( NULL != g_pCurrentModel && (!g_pCurrentModel->hasCamera()) )
      m_pMenuItems[m_IndexVideo]->setEnabled(false);
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexVideo]->setEnabled(false);

   m_IndexAudio = addMenuItem(new MenuItem("Audio","Change the audio settings"));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexAudio]->setEnabled(false);

   m_IndexTelemetry = addMenuItem(new MenuItem("Telemetry","Change telemetry parameters between flight controller and ruby vehicle."));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexTelemetry]->setEnabled(false);

   m_IndexDataLink = addMenuItem(new MenuItem("Auxiliary Data Link","Create and configure a general purpose data link between the vehicle and the controller."));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexDataLink]->setEnabled(false);
 
   m_IndexRC = addMenuItem(new MenuItem("Remote Control","Change your remote control type, channels, protocols and so on.")); 
   #ifndef FEATURE_ENABLE_RC
   m_pMenuItems[m_IndexRC]->setEnabled(false);
   #endif
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexRC]->setEnabled(false);

   m_IndexFunctions = addMenuItem(new MenuItem("Functions and Triggers","Configure special functions and triggers."));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexFunctions]->setEnabled(false);

   /*
   if ( NULL != g_pCurrentModel && g_pCurrentModel->radioInterfacesParams.interfaces_count > 1 )
      index = addMenuItem(new MenuItem("Radio Links", "Change the function, frequencies, data rates and parameters of the radio links."));
   else
      index = addMenuItem(new MenuItem("Radio Link", "Change the frequency, data rate and parameters of the radio link."));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[index]->setEnabled(false);
   */

   m_IndexRelay = addMenuItem(new MenuItem("Relaying", "Configure this vehicle as a relay."));
   #ifdef FEATURE_RELAYING
   m_pMenuItems[m_IndexRelay]->setEnabled(true);
   #else
   m_pMenuItems[m_IndexRelay]->setEnabled(false);
   #endif
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexRelay]->setEnabled(false);

   m_IndexPeripherals = addMenuItem(new MenuItem("Peripherals", "Change vehicle's serial ports settings and other peripherals."));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexPeripherals]->setEnabled(false);

   m_IndexCPU = addMenuItem(new MenuItem("CPU Settings", "Change CPU overclocking settings and processes priorities."));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexCPU]->setEnabled(false);

   m_IndexManagement = addMenuItem(new MenuItem("Management","Updates, deletes, back-up, restore the vehicle data and firmware."));
   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
      m_pMenuItems[m_IndexManagement]->setEnabled(false);

   Menu::onShow();
}

void MenuVehicle::addTopDescription()
{
   removeAllTopLines();
   char szBuff[256];
   sprintf(szBuff, "%s Settings", g_pCurrentModel->getLongName());
   szBuff[0] = toupper(szBuff[0]);
   setTitle(szBuff);
   //setSubTitle("Vehicle Settings");


   m_ExtraHeight = 0;
   m_fIconSize = 2.6 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS;

   m_Flags = 0xFFFF;

   return;

   if ( NULL != g_pCurrentModel && (g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo) )
      m_Flags = g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.throttled;

   if ( 0xFFFF == m_Flags || 0 == m_Flags )
      addTopLine("No hardware alarms.");
   else
   {
      if ( m_Flags & 0b1000 )
      {
         m_ExtraHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
         m_ExtraHeight += g_pRenderEngine->getMessageHeight(s_szMenuVAlarmTemp, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth() - m_fIconSize, g_idFontMenu);
      }
      if ( m_Flags & 0b0100 )
      {
         m_ExtraHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
         m_ExtraHeight += g_pRenderEngine->getMessageHeight(s_szMenuVAlarmThr, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth() - m_fIconSize, g_idFontMenu);
      }
      if ( m_Flags & 0b0010 )
      {
         m_ExtraHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
         m_ExtraHeight += g_pRenderEngine->getMessageHeight(s_szMenuVAlarmFreq, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth() - m_fIconSize, g_idFontMenu);
      }
      if ( m_Flags & 0b0001 )
      {
         m_ExtraHeight += menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
         m_ExtraHeight += g_pRenderEngine->getMessageHeight(s_szMenuVAlarmVoltage, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth() - m_fIconSize, g_idFontMenu);
      }      
   }
}

void MenuVehicle::Render()
{
   RenderPrepare();
   if ( m_bInvalidated )
      addTopDescription();

   float height_text = MENU_FONT_SIZE_TOOLTIPS*menu_getScaleMenus();

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   if ( m_Flags != 0xFFFF )
   {
      y -= 1 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
      if ( m_Flags & 0b1000 )
      {
         y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
         //g_pRenderEngine->setColors(get_Color_IconWarning());
         //g_pRenderEngine->drawText(m_xPos+MENU_MARGINS*menu_getScaleMenus(), y+m_fIconSize*0.75, 0.6*m_fIconSize, g_idFontOSDIcons, "");
         g_pRenderEngine->setColors(get_Color_MenuText());
         y += g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus() + m_fIconSize + 0.01*menu_getScaleMenus(), y, s_szMenuVAlarmTemp, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth() - m_fIconSize, g_idFontMenu);
         y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
      }
      if ( m_Flags & 0b0100 )
      {
         y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
         //g_pRenderEngine->setColors(get_Color_IconWarning());
         //g_pRenderEngine->drawText(m_xPos+MENU_MARGINS*menu_getScaleMenus(), y+m_fIconSize*0.75, 0.6*m_fIconSize, g_idFontOSDIcons, "");
         g_pRenderEngine->setColors(get_Color_MenuText());
         y += g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus() + m_fIconSize + 0.01*menu_getScaleMenus(), y, s_szMenuVAlarmThr, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth() - m_fIconSize, g_idFontMenu);
         y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
      }
      if ( m_Flags & 0b0010 )
      {
         y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
         //g_pRenderEngine->setColors(get_Color_IconWarning());
         //g_pRenderEngine->drawText(m_xPos+MENU_MARGINS*menu_getScaleMenus(), y+m_fIconSize*0.75, 0.6*m_fIconSize, g_idFontOSDIcons, "");
         g_pRenderEngine->setColors(get_Color_MenuText());
         y += g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus() + m_fIconSize + 0.01*menu_getScaleMenus(), y, s_szMenuVAlarmFreq, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth() - m_fIconSize, g_idFontMenu);
         y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
      }
      if ( m_Flags & 0b0001 )
      {
         y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
         //g_pRenderEngine->setColors(get_Color_IconWarning());
         //g_pRenderEngine->drawText(m_xPos+MENU_MARGINS*menu_getScaleMenus(), y+m_fIconSize*0.75, 0.6*m_fIconSize, g_idFontOSDIcons, "");
         g_pRenderEngine->setColors(get_Color_MenuText());
         y += g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus() + m_fIconSize + 0.01*menu_getScaleMenus(), y, s_szMenuVAlarmVoltage, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth() - m_fIconSize, g_idFontMenu);
         y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
      }      
      y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS*(1+MENU_TEXTLINE_SPACING);
   }

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


bool MenuVehicle::periodicLoop()
{
   if ( ! m_bWaitingForVehicleInfo )
      return false;

   if ( ! handle_commands_is_command_in_progress() )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         if ( m_IndexPeripherals == m_SelectedIndex )
            add_menu_to_stack(new MenuVehiclePeripherals());
         if ( m_IndexTelemetry == m_SelectedIndex )
            add_menu_to_stack(new MenuVehicleTelemetry());
         if ( m_IndexDataLink == m_SelectedIndex )
            add_menu_to_stack(new MenuVehicleDataLink());
         m_bWaitingForVehicleInfo = false;
         return true;
      }
   }
   return false;
}


void MenuVehicle::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

void MenuVehicle::onSelectItem()
{
   if ( NULL == g_pCurrentModel )
   {
      Popup* p = new Popup(true, "Vehicle is offline", 4 );
      p->setIconId(g_idIconWarning, get_Color_IconWarning());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( NULL != g_pCurrentModel && g_pCurrentModel->is_spectator )
   {
      if ( (m_IndexOSD != m_SelectedIndex) && ( m_IndexManagement != m_SelectedIndex) )
      {
      Popup* p = new Popup(true, "Vehicle Settings can not be changed on a spectator vehicle.", 5 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
      }
      if ( m_IndexOSD == m_SelectedIndex )
         add_menu_to_stack(new MenuVehicleOSD());
      return;
   }

   if ( m_IndexManagement == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleManagement());
      return;
   }

   if ( m_IndexPeripherals == m_SelectedIndex )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         add_menu_to_stack(new MenuVehiclePeripherals());
         return;
      }
      if ( ((g_pCurrentModel->sw_version >>8) & 0xFF) < 7 )
      {
         addMessage("You need to update your vehicle sowftware to be able to use this menu.");
         return;
      }
      handle_commands_reset_has_received_vehicle_core_plugins_info();
      m_bWaitingForVehicleInfo = false;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_GET_CORE_PLUGINS_INFO, 0, NULL, 0) )
         valuesToUI();
      else
         m_bWaitingForVehicleInfo = true;
      return;
   }

   if ( (!pairing_isStarted()) || (!pairing_isReceiving()) )
   {
      Popup* p = new Popup(true, "Can't change settings when not connected to the vehicle.", 5 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( g_pCurrentModel->b_mustSyncFromVehicle && ( m_IndexManagement != m_SelectedIndex) )
   {
      Popup* p = new Popup(true, "Vehicle Settings not synched yet", 4 );
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      valuesToUI();
      return;
   }

   if ( m_IndexGeneral == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleGeneral());
   if ( m_IndexCamera == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleCamera());
   if ( m_IndexVideo == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleVideo());
   if ( m_IndexAudio == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleAudio());
   if ( m_IndexTelemetry == m_SelectedIndex )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         add_menu_to_stack(new MenuVehicleTelemetry());
         return;
      }
      if ( ((g_pCurrentModel->sw_version >>8) & 0xFF) < 7 )
      {
         addMessage("You need to update your vehicle sowftware to be able to use this menu.");
         return;
      }
      handle_commands_reset_has_received_vehicle_core_plugins_info();
      m_bWaitingForVehicleInfo = false;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_GET_CORE_PLUGINS_INFO, 0, NULL, 0) )
         valuesToUI();
      else
         m_bWaitingForVehicleInfo = true;
      return;
   }
   if ( m_IndexDataLink == m_SelectedIndex )
   {
      if ( handle_commands_has_received_vehicle_core_plugins_info() )
      {
         add_menu_to_stack(new MenuVehicleDataLink());
         return;
      }
      if ( ((g_pCurrentModel->sw_version >>8) & 0xFF) < 7 )
      {
         addMessage("You need to update your vehicle sowftware to be able to use this menu.");
         return;
      }
      handle_commands_reset_has_received_vehicle_core_plugins_info();
      m_bWaitingForVehicleInfo = false;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_GET_CORE_PLUGINS_INFO, 0, NULL, 0) )
         valuesToUI();
      else
         m_bWaitingForVehicleInfo = true;
      return;
   }
   if ( m_IndexOSD == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleOSD());
   if ( m_IndexRC == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleRC());

   if ( m_IndexFunctions == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleFunctions());
      return;
   }

   if ( m_IndexRelay == m_SelectedIndex )
   {
      bool bCanUse = true;
      if ( g_pCurrentModel->radioInterfacesParams.interfaces_count < 2 )
         bCanUse = false;

      int countCardsOk = 0;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] )
            countCardsOk++;
      if ( countCardsOk < 2 )
         bCanUse = false;

      if ( ! bCanUse )
      {
         m_iConfirmationId = 10;
         MenuConfirmation* pMC = new MenuConfirmation("No Suitable Hardware","You need at least two radio interfaces on the vehicle to be able to use the relay functionality.",m_iConfirmationId, true);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
         return;
      }
      add_menu_to_stack(new MenuVehicleRelay());
   }

   if ( m_IndexCPU == m_SelectedIndex )
      add_menu_to_stack(new MenuVehicleExpert());

   if ( m_IndexAlarms == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuVehicleAlarms());
      return;
   }
}
