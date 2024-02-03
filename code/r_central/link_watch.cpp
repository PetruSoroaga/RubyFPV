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

#include "../base/base.h"
#include "../base/config.h"
//#include "../base/radio_utils.h"
#include "../radio/radiopackets2.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../common/string_utils.h"

#include "shared_vars.h"
#include "link_watch.h"
#include "popup.h"
#include "menu.h"
#include "pairing.h"
#include "warnings.h"
#include "colors.h"
#include "media.h"
#include "ruby_central.h"
#include "notifications.h"
#include "local_stats.h"
#include "osd.h"
#include "osd_common.h"
#include "menu_switch_vehicle.h"
#include "launchers_controller.h"
#include "ruby_central.h"
#include "handle_commands.h"
#include "process_router_messages.h"
#include "timers.h"
#include "events.h"
#include "menu_confirmation.h"

extern bool s_bDebugOSDShowAll;

u32 s_TimeLastProcessesCheck = 0;
u32 s_TimeLastVideoProcessingCheck = 0;
u32 s_TimeLastVideoMemoryFreeCheck = 0;
u32 s_TimeSecondsCheck = 0;
u32 s_TimeLastWarningRCHID = 0;
u32 s_TimeLastAlarmRadioLinkBehind = 0;
u32 s_TimeLastAlarmRouterProcess = 0;

u32 s_LastRouterProcessCheckedAlarmFlags = 0;

u32 s_CountProcessRouterFailures = 0;
u32 s_CountProcessTelemetryFailures = 0;

bool s_bLinkWatchIsRCOutputEnabled = false;

bool s_bLinkWatchShownSwitchVehicleMenu = false;

void link_watch_init()
{
   log_line("Link watch init.");
}

void link_watch_reset()
{
   s_TimeSecondsCheck = 0;
   s_TimeLastWarningRCHID = 0;
   s_TimeLastAlarmRadioLinkBehind = 0;
   s_TimeLastAlarmRouterProcess = 0;

   s_LastRouterProcessCheckedAlarmFlags = 0;
   s_CountProcessRouterFailures = 0;
   s_CountProcessTelemetryFailures = 0;

   s_bLinkWatchShownSwitchVehicleMenu = false;
   g_bVideoLost = false;
   
   s_TimeLastVideoMemoryFreeCheck = 0;
   link_watch_mark_started_video_processing();

   if ( NULL != g_pPopupLooking )
   {
      popups_remove(g_pPopupLooking);
      g_pPopupLooking = NULL;
      log_line("Removed popup looking for model (3).");
   }

   if ( NULL != g_pPopupLinkLost )
   {
      popups_remove(g_pPopupLinkLost);
      g_pPopupLinkLost = NULL;
      log_line("Removed popup link lost.");
   }

   link_reset_reconfiguring_radiolink();
}

void link_set_is_reconfiguring_radiolink(int iRadioLink )
{
   g_bConfiguringRadioLink = true;
}

void link_set_is_reconfiguring_radiolink(int iRadioLink, bool bConfirmFlagsChanges, bool bWaitVehicleConfirmation, bool bWaitControllerConfirmation)
{
   g_bConfiguringRadioLink = true;
}

void link_reset_reconfiguring_radiolink()
{
   g_bConfiguringRadioLink = false;
}

bool link_is_reconfiguring_radiolink()
{
   return g_bConfiguringRadioLink;
}

void link_watch_mark_started_video_processing()
{
   s_TimeLastVideoProcessingCheck = g_TimeNow;
}

void link_watch_remove_popups()
{
   if ( NULL != g_pPopupLooking )
   {
      popups_remove(g_pPopupLooking);
      g_pPopupLooking = NULL;
      log_line("Removed popup looking for model (7).");
   }
   if ( NULL != g_pPopupWrongModel )
   {
      popups_remove(g_pPopupWrongModel);
      g_pPopupWrongModel = NULL;
      log_line("Removed popup wrong model (6).");
   }

   if ( NULL != g_pPopupLinkLost )
   {
      popups_remove(g_pPopupLinkLost);
      g_pPopupLinkLost = NULL;
      log_line("Removed popup link lost.");
   }
}

void link_watch_loop_popup_looking()
{
   // If we just paired, wait for router ready
   if ( g_bSearching || (! g_bIsRouterReady) || (NULL == g_pCurrentModel) )
      return;

   if ( link_has_received_main_vehicle_ruby_telemetry() || link_has_paired_with_main_vehicle() )
   {
      if ( NULL != g_pPopupLooking )
      {
         popups_remove(g_pPopupLooking);
         g_pPopupLooking = NULL;
         log_line("Removed popup looking for model (7).");
      }
      return;
   }

   if ( NULL != g_pPopupLooking )
      return;

   char szText[256];
   u32 idIcon = 0;

   if ( ! g_bFirstModelPairingDone )
   {
      char szFreq[256];
      szFreq[0] = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;

         // If identical frequency, skip it
         bool bIdentical = false;
         for( int k=0; k<i; k++ )
         {
            radio_hw_info_t* pRadioHWInfo2 = hardware_get_radio_info(k);
            if ( NULL == pRadioHWInfo2 )
               continue;
            if ( pRadioHWInfo2->uCurrentFrequencyKhz == pRadioHWInfo->uCurrentFrequencyKhz )
               bIdentical = true;
         }

         if ( bIdentical )
            continue;

         if ( 0 == szFreq[0] )
           strcpy(szFreq, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz));
         else
         {
            strcat(szFreq, ", ");
            strcat(szFreq, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz));
         }
      }
      sprintf(szText, "Looking for default vehicle on %s frequencies...", szFreq);
   }
   else
   {
      log_line("Will add `looking for` popup for VID %u, firmware type: %s", g_pCurrentModel->vehicle_id, str_format_firmware_type(g_pCurrentModel->getVehicleFirmwareType()));
      idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );
      if ( g_pCurrentModel->radioLinksParams.links_count < 2 )
         sprintf(szText, "Looking for %s (%s, %s)...", g_pCurrentModel->getLongName(), g_pCurrentModel->is_spectator?"Spectator Mode":"Control Mode", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[0]));
      else
      {
         if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId < 0 )
         {
            char szFreq1[64];
            char szFreq2[64];
            strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[0]));
            strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[1]));
            sprintf(szText, "Looking for %s (%s, %s / %s)...", g_pCurrentModel->getLongName(), g_pCurrentModel->is_spectator?"Spectator Mode":"Control Mode", szFreq1, szFreq2);
         }
         else
         {
             if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == 0 )
               sprintf(szText, "Looking for %s (%s, %s)...", g_pCurrentModel->getLongName(), g_pCurrentModel->is_spectator?"Spectator Mode":"Control Mode", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[1]));
             else
               sprintf(szText, "Looking for %s (%s, %s)...", g_pCurrentModel->getLongName(), g_pCurrentModel->is_spectator?"Spectator Mode":"Control Mode", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[0]));
         }
      }
   }

   g_pPopupLooking = new Popup(szText, 0.2, 0.36, 0.5, 0);
   if ( ! g_bFirstModelPairingDone )
      g_pPopupLooking->setCentered();
   
   g_pPopupLooking->setIconId(idIcon, get_Color_MenuText());

   if ( (!g_bFirstModelPairingDone) || (NULL == g_pCurrentModel) )
   {
      g_pPopupLooking->addLine(" ");
      g_pPopupLooking->addLine("If you just installed a vehicle, power up your vehicle. The connection will be made automatically when that vehicle is in radio range.");
      g_pPopupLooking->addLine(" ");
      g_pPopupLooking->addLine("Or press the [Menu] key to open up the menu and use [Search] menu to search for other vehicles to connect to.");
   }
   else
   {
      g_pPopupLooking->addLine(" ");
      g_pPopupLooking->addLine("Power up your vehicle. The connection will be made automatically when the vehicle is in radio range.");
      g_pPopupLooking->addLine(" ");
      g_pPopupLooking->addLine("Or press the [Menu] key to open up the menu and switch to another vehicle or search for other vehicles.");
   }
   popups_add(g_pPopupLooking);
}

void link_watch_loop_unexpected_vehicles()
{
   // If we just paired, wait for router ready
   if ( g_bSearching || (!g_bIsRouterReady) || (NULL == g_pCurrentModel) )
      return;

   if ( g_bSwitchingFavoriteVehicle )
      return;
     
   // Did not received any info from no unexpected vehicles? Then do nothing.
   if ( ! g_UnexpectedVehicleRuntimeInfo.bGotRubyTelemetryInfo )
      return;

   // First time ever pairing was not done, then ignore this.

   if( ! g_bFirstModelPairingDone )
      return;

   Model* pModelTemp = findModelWithId(g_UnexpectedVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id, 10);
   
   // Received unexpected known vehicle

   if ( NULL != pModelTemp )
   {
      if ( g_UnexpectedVehicleRuntimeInfo.uTimeLastRecvRubyTelemetry < g_TimeNow-5000 )
      {
         s_bLinkWatchShownSwitchVehicleMenu = false;
         if ( menu_has_menu(MENU_ID_SWITCH_VEHICLE) )
         {
            Menu* pMenu = menu_get_menu_by_id(MENU_ID_SWITCH_VEHICLE);
            remove_menu_from_stack(pMenu);
         }
         return;
      }

      if ( s_bLinkWatchShownSwitchVehicleMenu )
         return;
      s_bLinkWatchShownSwitchVehicleMenu = true;

      log_line("Received data from unexpected model. Current vehicle ID: %u, received telemetry vehicle ID: %u",
            g_pCurrentModel->vehicle_id, g_UnexpectedVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id);
      add_menu_to_stack(new MenuSwitchVehicle(pModelTemp->vehicle_id));
      return;
   }
   
   // Received unexpected unknown vehicle

   // Too old?
   if ( g_UnexpectedVehicleRuntimeInfo.uTimeLastRecvRubyTelemetry < g_TimeNow-5000 )
   {
      if ( NULL != g_pPopupWrongModel )
      {
         popups_remove(g_pPopupWrongModel);
         g_pPopupWrongModel = NULL;
         log_line("Removed popup wrong model. We are not getting data anymore from the unexpected model UID: %u", g_UnexpectedVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id);
      }
      return;
   }  

   // Fresh unexpected data

   if ( NULL != g_pPopupWrongModel )
      return;

   log_line("Received data from unexpected model. Current vehicle ID: %u, received telemetry vehicle ID: %u",
      g_pCurrentModel->vehicle_id, g_UnexpectedVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id);
   char szBuff[256];
   char szName[128];
   szName[0] = 0;

   if ( 0 == g_UnexpectedVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_name[0] )
      strcat(szName, "No Name");
   else
      strcat(szName, (char*)g_UnexpectedVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_name);
   
   if ( 0 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
      sprintf(szBuff, "Warning: There is a different vehicle (%s) on the same frequency as your current vehicle (%s)!", szName, g_pCurrentModel->getLongName());
   else if ( 1 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
      sprintf(szBuff, "Warning: There is a different vehicle (%s) on the same frequency (%s) as your current vehicle (%s)!", szName, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[0]), g_pCurrentModel->getLongName());
   else if ( 2 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
   {
      char szFreq1[64];
      char szFreq2[64];
      strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[0]));
      strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[1]));
      sprintf(szBuff, "Warning: There is a different vehicle (%s) on the same frequencies (%s/%s) as your current vehicle (%s)!", szName, szFreq1, szFreq2, g_pCurrentModel->getLongName());
   }
   else
   {
      char szFreq1[64];
      char szFreq2[64];
      char szFreq3[64];
      strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[0]));
      strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[1]));
      strcpy(szFreq3, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[2]));
      sprintf(szBuff, "Warning: There is a different vehicle (%s) on the same frequencies (%s/%s/%s) as your current vehicle (%s)!", szName, szFreq1, szFreq2, szFreq3, g_pCurrentModel->getLongName());
   }

   float yPos = 0.36;
   if ( NULL != g_pPopupLooking )
      yPos = 0.61;
   g_pPopupWrongModel = new Popup(szBuff, 0.2, yPos, 0.5, 0);
   if ( NULL == g_pPopupLooking )
      g_pPopupWrongModel->setCentered();
   g_pPopupWrongModel->addLine("Another unpaired vehicle is using the same frequency as your current vehicle. Search and connect (pair) to this new vehicle or power it off as it will impact your radio link quality.");
   g_pPopupWrongModel->setIconId(g_idIconWarning, get_Color_IconWarning());
   popups_add(g_pPopupWrongModel);
}

void link_watch_link_lost()
{
   // If we just paired, wait till router ready
   if ( g_bSearching || (!g_bIsRouterReady) || (NULL == g_pCurrentModel) )
      return;

   if ( g_bFirstModelPairingDone && ( 0 == g_uActiveControllerModelVID) )
      return;
   
   // If we never received main vehicle telemetry, do nothing
   if ( ! link_has_received_main_vehicle_ruby_telemetry() )
      return;

   if ( g_TimeNow < g_VehiclesRuntimeInfo[0].uTimeLastRecvAnyRubyTelemetry + TIMEOUT_TELEMETRY_LOST )
   {
      if ( NULL != g_pPopupLinkLost )
      {
         popups_remove(g_pPopupLinkLost);
         g_pPopupLinkLost = NULL;
         log_line("Removed popup link lost.");
      }
      return;
   }

   // Link is lost

   if ( NULL != g_pPopupLinkLost )
      return;
   
   g_pPopupLinkLost = new Popup("Link lost. Trying to reconnect...", 0.28, 0.3, 0.5, 0);
   g_pPopupLinkLost->setCentered();
   g_pPopupLinkLost->addLine("The radio link with the vehicle is lost. Will reconnect automatically when in range.");
   g_pPopupLinkLost->setIconId(g_idIconWarning, get_Color_IconWarning());
   popups_add(g_pPopupLinkLost);
}

void link_watch_loop_throttled()
{
   // If we just paired, wait till router ready
   if ( ! g_bIsRouterReady )
      return;

   if ( ! link_has_received_main_vehicle_ruby_telemetry() )
      return;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( ! g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo )
         continue;
      if ( (0 == g_VehiclesRuntimeInfo[i].uVehicleId) || (MAX_U32 == g_VehiclesRuntimeInfo[i].uVehicleId) )
         continue;
      if ( NULL == g_VehiclesRuntimeInfo[i].pModel )
         continue;

      u8 newFlags = g_VehiclesRuntimeInfo[i].headerRubyTelemetryExtended.throttled;
      u8 oldFlags = g_VehiclesRuntimeInfo[i].uTmpLastThrottledFlags;
      if ( oldFlags == newFlags )
         continue;

      if ( newFlags & 0b1000 )
      {
         if ( ! (oldFlags & 0b1000) )
            warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Vehicle: High temperature!", g_idIconTemperature, get_Color_IconError());
      }
      else
      {
         if ( oldFlags & 0b1000 )
            warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Vehicle: Temperature back to normal", g_idIconTemperature, get_Color_IconSucces());
      }

      if ( newFlags & 0b0100 )
      {
         if ( ! (oldFlags & 0b0100) )
            warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Vehicle: CPU is throttled", g_idIconCPU, get_Color_IconError());
      }
      else
      {
         if ( oldFlags & 0b0100 )
            warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Vehicle: CPU throttling cleared", g_idIconCPU, get_Color_IconSucces());
      }

      if ( newFlags & 0b0010 )
      {
         if ( ! (oldFlags & 0b0010) )
            warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Vehicle: CPU frequency capped", g_idIconCPU, get_Color_IconError());
      }
      else
      {
         if ( oldFlags & 0b0010 )
            warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Vechicle: CPU frequency back to normal", g_idIconCPU, get_Color_IconSucces());
      }

      if ( newFlags & 0b0001 )
      {
         if ( ! (oldFlags & 0b0001) )
            warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Vechicle: Undervoltage detected", g_idIconCPU, get_Color_IconError());
      }
      else
      {
         if ( oldFlags & 0b0001 )
            warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Vehicle: Undervoltage cleared", g_idIconCPU, get_Color_IconSucces());
      }

      g_VehiclesRuntimeInfo[i].uTmpLastThrottledFlags = newFlags;
   }
}


void link_watch_loop_telemetry()
{
   if ( ! g_bIsRouterReady )
      return;

   if ( g_iCurrentActiveVehicleRuntimeInfoIndex < 0 )
      return;

   // Update telemetry packets frequency

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (g_VehiclesRuntimeInfo[i].uVehicleId == 0) || (g_VehiclesRuntimeInfo[i].uVehicleId == MAX_U32) )
         continue;

      if ( g_TimeNow >= g_VehiclesRuntimeInfo[i].tmp_uTimeLastTelemetryFrequencyComputeTime + 1000 )
      {
         g_VehiclesRuntimeInfo[i].tmp_uTimeLastTelemetryFrequencyComputeTime = g_TimeNow;
         g_VehiclesRuntimeInfo[i].iFrequencyRubyTelemetryFull = g_VehiclesRuntimeInfo[i].tmp_iCountRubyTelemetryPacketsFull;
         g_VehiclesRuntimeInfo[i].iFrequencyRubyTelemetryShort = g_VehiclesRuntimeInfo[i].tmp_iCountRubyTelemetryPacketsShort;
         g_VehiclesRuntimeInfo[i].iFrequencyFCTelemetryFull = g_VehiclesRuntimeInfo[i].tmp_iCountFCTelemetryPacketsFull;
         g_VehiclesRuntimeInfo[i].iFrequencyFCTelemetryShort = g_VehiclesRuntimeInfo[i].tmp_iCountFCTelemetryPacketsShort;
         g_VehiclesRuntimeInfo[i].tmp_iCountRubyTelemetryPacketsFull = 0;
         g_VehiclesRuntimeInfo[i].tmp_iCountRubyTelemetryPacketsShort = 0;
         g_VehiclesRuntimeInfo[i].tmp_iCountFCTelemetryPacketsFull = 0;
         g_VehiclesRuntimeInfo[i].tmp_iCountFCTelemetryPacketsShort = 0;
      }
   }

   if ( NULL == g_pCurrentModel )
      return;

   if ( ! link_has_received_main_vehicle_ruby_telemetry() )
   if ( ! link_has_received_relayed_vehicle_telemetry_info() )
      return;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (g_VehiclesRuntimeInfo[i].uVehicleId == 0) || (g_VehiclesRuntimeInfo[i].uVehicleId == MAX_U32) )
         continue;
      if ( NULL == g_VehiclesRuntimeInfo[i].pModel )
         continue;

      if ( g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo || g_VehiclesRuntimeInfo[i].bGotFCTelemetry )
      {
         u32 uMaxLostTime = TIMEOUT_TELEMETRY_LOST;
         if ( (NULL != g_VehiclesRuntimeInfo[i].pModel) && (g_VehiclesRuntimeInfo[i].pModel->telemetry_params.update_rate > 10) )
            uMaxLostTime = TIMEOUT_TELEMETRY_LOST/2;

         g_VehiclesRuntimeInfo[i].bLinkLost = false;
         if ( g_TimeNow > g_VehiclesRuntimeInfo[i].uTimeLastRecvRubyTelemetry + uMaxLostTime )
         if ( g_TimeNow > g_VehiclesRuntimeInfo[i].uTimeLastRecvRubyTelemetryExtended + uMaxLostTime )
         if ( g_TimeNow > g_VehiclesRuntimeInfo[i].uTimeLastRecvRubyTelemetryShort + uMaxLostTime )
         if ( g_TimeNow > g_VehiclesRuntimeInfo[i].uTimeLastRecvFCTelemetry + uMaxLostTime )
         if ( g_TimeNow > g_VehiclesRuntimeInfo[i].uTimeLastRecvFCTelemetryFull + uMaxLostTime )
         if ( g_TimeNow > g_VehiclesRuntimeInfo[i].uTimeLastRecvFCTelemetryShort + uMaxLostTime )
            g_VehiclesRuntimeInfo[i].bLinkLost = true;
      
      }
      if ( g_VehiclesRuntimeInfo[i].bGotFCTelemetry )
      {
         // Flight mode changed?

         if ( g_VehiclesRuntimeInfo[i].uLastFCFlightMode != (g_VehiclesRuntimeInfo[i].headerFCTelemetry.flight_mode & (~FLIGHT_MODE_ARMED)) )
         {
            bool bShow = false;
            if ( 0 != g_VehiclesRuntimeInfo[i].uLastFCFlightMode )
               bShow = true;
            g_VehiclesRuntimeInfo[i].uLastFCFlightMode = (g_VehiclesRuntimeInfo[i].headerFCTelemetry.flight_mode & (~FLIGHT_MODE_ARMED));
            if ( bShow )
               notification_add_flight_mode( g_VehiclesRuntimeInfo[i].uVehicleId, g_VehiclesRuntimeInfo[i].uLastFCFlightMode);
         }

         // RC failsafe changed ?

         #ifdef FEATURE_ENABLE_RC
         if ( ( g_VehiclesRuntimeInfo[i].headerFCTelemetry.flags & FC_TELE_FLAGS_RC_FAILSAFE ) ||
              (g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo && (g_VehiclesRuntimeInfo[i].headerRubyTelemetryExtended.flags & FLAG_RUBY_TELEMETRY_RC_FAILSAFE) ) )
         {
            if ( ! g_VehiclesRuntimeInfo[i].bRCFailsafeState )
               notification_add_rc_failsafe(g_VehiclesRuntimeInfo[i].uVehicleId);
            g_VehiclesRuntimeInfo[i].bRCFailsafeState = true;
         }
         else if ( g_VehiclesRuntimeInfo[i].bRCFailsafeState )
         {
            notification_add_rc_failsafe_cleared(g_VehiclesRuntimeInfo[i].uVehicleId);
            g_VehiclesRuntimeInfo[i].bRCFailsafeState = false;
         }
         #endif

         // FC telemetry flags changed ?
         if ( g_VehiclesRuntimeInfo[i].headerFCTelemetry.flags != g_VehiclesRuntimeInfo[i].uLastFCFlags )
         {
            if ( 0 != g_VehiclesRuntimeInfo[i].uLastFCFlags )
            {
               if ( g_VehiclesRuntimeInfo[i].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED )
               if ( !(g_VehiclesRuntimeInfo[i].uLastFCFlags & FC_TELE_FLAGS_ARMED) )
               {
                  onEventArmed(g_VehiclesRuntimeInfo[i].uVehicleId);
               }
               if ( !(g_VehiclesRuntimeInfo[i].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED) )
               if ( g_VehiclesRuntimeInfo[i].uLastFCFlags & FC_TELE_FLAGS_ARMED )
               {
                  onEventDisarmed(g_VehiclesRuntimeInfo[i].uVehicleId);
               }
            }
            g_VehiclesRuntimeInfo[i].uLastFCFlags = g_VehiclesRuntimeInfo[i].headerFCTelemetry.flags;
         }

         // FC source telemetry data present or lost ?

         if ( g_VehiclesRuntimeInfo[i].headerFCTelemetry.flags & FC_TELE_FLAGS_NO_FC_TELEMETRY )
         {
            if ( g_VehiclesRuntimeInfo[i].bFCTelemetrySourcePresent )
               warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Flight controller telemetry missing", g_idIconCPU, get_Color_IconError());
            g_VehiclesRuntimeInfo[i].bFCTelemetrySourcePresent = false;
         }
         else
         {
            if ( ! g_VehiclesRuntimeInfo[i].bFCTelemetrySourcePresent )
               warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Flight controller telemetry recovered", g_idIconCPU, get_Color_IconSucces());
            g_VehiclesRuntimeInfo[i].bFCTelemetrySourcePresent = true;
         }
      }

      // Check for Ruby telemetry lost or recovered state

      if ( g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo )
      {
         u32 uMaxLostTime = TIMEOUT_TELEMETRY_LOST;
         if ( (NULL != g_VehiclesRuntimeInfo[i].pModel) && (g_VehiclesRuntimeInfo[i].pModel->telemetry_params.update_rate > 10) )
            uMaxLostTime = TIMEOUT_TELEMETRY_LOST/2;
         bool bOk = true;
         if ( g_TimeNow > g_VehiclesRuntimeInfo[i].uTimeLastRecvRubyTelemetry + uMaxLostTime )
         if ( g_TimeNow > g_VehiclesRuntimeInfo[i].uTimeLastRecvRubyTelemetryExtended + uMaxLostTime )
         if ( g_TimeNow > g_VehiclesRuntimeInfo[i].uTimeLastRecvRubyTelemetryShort + uMaxLostTime )
         {
            bOk = false;
            if ( ! g_VehiclesRuntimeInfo[i].bRubyTelemetryLost )
               warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Telemetry from vehicle lost!", g_idIconCPU, get_Color_IconError());
            g_VehiclesRuntimeInfo[i].bRubyTelemetryLost = true;
         }
         if ( bOk )
         {
            if ( g_VehiclesRuntimeInfo[i].bRubyTelemetryLost )
               warnings_add(g_VehiclesRuntimeInfo[i].uVehicleId, "Telemetry from vehicle recovered", g_idIconCPU, get_Color_IconSucces());
            g_VehiclesRuntimeInfo[i].bRubyTelemetryLost = false;
         }
      }
   }
}

void link_watch_loop_video()
{   
   if ( ! g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo )
      return;
   if ( NULL == g_pProcessStatsRouter )
      return;

   if ( s_LastRouterProcessCheckedAlarmFlags != g_ProcessStatsRouter.alarmFlags ||
        g_ProcessStatsRouter.alarmTime > s_TimeLastAlarmRouterProcess )
   {
      s_LastRouterProcessCheckedAlarmFlags = g_ProcessStatsRouter.alarmFlags;
      s_TimeLastAlarmRouterProcess = g_ProcessStatsRouter.alarmTime;

      if ( g_ProcessStatsRouter.alarmFlags & PROCESS_ALARM_RADIO_STREAM_RESTARTED )
      {
         if ( (NULL != g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel) &&
              ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].pModel->iCameraCount > 0 ) )
            warnings_add(g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].uVehicleId, "Video stream was restarted on vehicle.", g_idIconWarning);
         else
            warnings_add(g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].uVehicleId, "Radio streams where restarted on vehicle.", g_idIconWarning);
      }
      if ( g_ProcessStatsRouter.alarmFlags & PROCESS_ALARM_RADIO_INTERFACE_BEHIND )
      if ( g_TimeNow > s_TimeLastAlarmRadioLinkBehind + 2000 )
      {
         ControllerSettings* pCS = get_ControllerSettings();
         if ( NULL != pCS && (0 != pCS->iDeveloperMode) )
         {
            s_TimeLastAlarmRadioLinkBehind = g_TimeNow;
            char szBuff[256];
            //sprintf(szBuff, "Link %d is behind on stream id %d, received packet index: %u, maximum packet index for this stream is: %u.", g_ProcessStatsRouter.alarmParam[2], g_ProcessStatsRouter.alarmParam[0], g_ProcessStatsRouter.alarmParam[3], g_ProcessStatsRouter.alarmParam[1] );
            sprintf(szBuff, "Link %d is behind on stream id %d by %d packets", g_ProcessStatsRouter.alarmParam[2], g_ProcessStatsRouter.alarmParam[0], g_ProcessStatsRouter.alarmParam[1] - g_ProcessStatsRouter.alarmParam[3]);
            warnings_add(g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].uVehicleId, szBuff, g_idIconWarning);
         }
      }
   }
}

void link_watch_loop_processes()
{
   if ( g_bSearching )
      return;

   if ( g_TimeNow > s_TimeLastProcessesCheck + 1000 )
   {
      s_TimeLastProcessesCheck = g_TimeNow;
      if ( pairing_isStarted() )
      {
         if ( (NULL != g_pProcessStatsRouter) && (g_ProcessStatsRouter.lastActiveTime < g_TimeNow - 1100) )
            s_CountProcessRouterFailures++;
         else
            s_CountProcessRouterFailures = 0;

         if ( (NULL != g_pProcessStatsTelemetry) && (g_ProcessStatsTelemetry.lastActiveTime < g_TimeNow - 1100) )
            s_CountProcessTelemetryFailures++;
         else
            s_CountProcessTelemetryFailures = 0;

         bool bNeedsRestart = false;
         int failureCountMax = 4;

         if ( g_bVideoProcessing )
            failureCountMax = 8;
         if ( s_CountProcessRouterFailures > failureCountMax )
         {
            log_softerror_and_alarm("Router process has failed. Current router PIDS: [%s].", hw_process_get_pid("ruby_rt_station"));
            warnings_add(0, "Controller router process is malfunctioning! Restarting it.", g_idIconCPU, get_Color_IconError());
            bNeedsRestart = true;
         }
         if ( s_CountProcessTelemetryFailures > failureCountMax )
         {
            log_softerror_and_alarm("Telemetry process has failed. Current router PIDS: [%s].", hw_process_get_pid("ruby_rx_telemetry"));
            warnings_add(0, "Controller telemetry process is malfunctioning! Restarting it.", g_idIconCPU, get_Color_IconError());
            bNeedsRestart = true;
         }

         if ( bNeedsRestart )
         {
            log_line("Will restart processes.");
            char szPids[1024];
            szPids[0] = 0;
            hw_execute_bash_command("pidof ruby_rx_telemetry", szPids);
            if ( strlen(szPids) > 2 )
               log_line("Process ruby_rx_telemetry is still present, pid: %s.", szPids);
            else
               log_line("Process ruby_rx_telemetry is not present, has crashed.");

            szPids[0] = 0;
            hw_execute_bash_command("pidof ruby_rt_station", szPids);
            if ( strlen(szPids) > 2 )
               log_line("Process ruby_rt_station is still present, pid: %s.", szPids);
            else
               log_line("Process ruby_rt_station is not present, has crashed.");

            s_CountProcessRouterFailures = 0;
            s_CountProcessTelemetryFailures = 0;
            pairing_stop();
            hardware_sleep_ms(100);
            pairing_start_normal();
         }
      }
   }

   if ( g_TimeNow > s_TimeLastVideoProcessingCheck + 1000 )
   {
      s_TimeLastVideoProcessingCheck = g_TimeNow;

      if ( g_bVideoRecordingStarted )
      {
      Preferences *p = get_Preferences();
      if ( p->iVideoDestination == 1 )
      {
         if ( g_TimeNow > s_TimeLastVideoMemoryFreeCheck + 4000 )
         {
            s_TimeLastVideoMemoryFreeCheck = g_TimeNow;
            char szComm[1024];
            char szBuff[2048];
            char szTemp[64];
            sprintf(szComm, "df %s | sed -n 2p", TEMP_VIDEO_MEM_FOLDER);
            hw_execute_bash_command_raw(szComm, szBuff);
            long lu, lf, lt;
            sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lt, &lu, &lf);
            if ( lf/1000 < 20 )
               ruby_stop_recording();
         }
      }
      }

      if ( g_bVideoProcessing )
      {
      char szPids[1024];
      bool procRunning = false;
      hw_execute_bash_command_silent("pidof ruby_video_proc", szPids);
      if ( strlen(szPids) > 2 )
         procRunning = true;
      if ( ! procRunning )
      {
         log_line("Video processing process finished.");
         g_bVideoProcessing = false;
         if ( access( TEMP_VIDEO_FILE_PROCESS_ERROR, R_OK ) != -1 )
         {
            warnings_add(0, "Video file processing failed.", g_idIconCamera, get_Color_IconWarning());

            char szBuff[256];
            char * line = NULL;
            size_t len = 0;
            ssize_t read;
            FILE* fd = fopen(TEMP_VIDEO_FILE_PROCESS_ERROR, "r");

            while ( (NULL != fd) && ((read = getline(&line, &len, fd)) != -1))
            {
              printf("Retrieved line of length %zu:\n", read);
              if ( read > 0 )
                 warnings_add(0, line, g_idIconCamera, get_Color_IconNormal());
            }
            if ( NULL != fd )
               fclose(fd);
            sprintf(szBuff, "rm -rf %s 2>/dev/null",TEMP_VIDEO_FILE_PROCESS_ERROR);
            hw_execute_bash_command(szBuff, NULL );
         }
         else
             warnings_add(0, "Video file processing complete.", g_idIconCamera, get_Color_IconNormal());
      }
      }
   }
}

void link_watch_rc()
{
   if ( g_bSearching )
      return;
   if ( NULL == g_pCurrentModel || (!g_pCurrentModel->rc_params.rc_enabled) || g_pCurrentModel->is_spectator )
      return;
   if ( ! pairing_isStarted() )
      return;

   // If we just paired, wait till router ready
   if ( ! g_bIsRouterReady )
      return;

   if ( s_TimeLastWarningRCHID != 0 && g_TimeNow < s_TimeLastWarningRCHID+20000 )
      return;

   if ( g_pCurrentModel->rc_params.rc_enabled )
   {
      if ( g_pCurrentModel->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED )
      {
         if ( ! s_bLinkWatchIsRCOutputEnabled )
            notification_add_rc_output_enabled();
         s_bLinkWatchIsRCOutputEnabled = true;
      }
      else
      {
         if ( s_bLinkWatchIsRCOutputEnabled )
            notification_add_rc_output_disabled();
         s_bLinkWatchIsRCOutputEnabled = false;
      }
   }

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_USB )
   {
      if ( pCI->inputInterfacesCount == 0 )
      {
         controllerInterfacesEnumJoysticks();
         if ( pCI->inputInterfacesCount  == 0 )
         {
             s_TimeLastWarningRCHID = g_TimeNow;
             warnings_add(g_pCurrentModel->vehicle_id, "RC is enabled on current vehicle and the input controller device is missing!", g_idIconJoystick, get_Color_IconError());
             return;
         }
      }

      bool bFound = false;
      for( int i=0; i<pCI->inputInterfacesCount; i++ )
      {
         t_ControllerInputInterface* pCII = controllerInterfacesGetAt(i);
         if ( pCII != NULL && NULL != g_pCurrentModel && g_pCurrentModel->rc_params.hid_id == pCII->uId )
            bFound = true;
      }
      if ( ! bFound )
      {
          s_TimeLastWarningRCHID = g_TimeNow;
          warnings_add(g_pCurrentModel->vehicle_id, "RC is enabled on current vehicle and the detected RC input controller device is different from the one setup on the vehicle!", g_idIconJoystick, get_Color_IconError());
          return;
      }
   }
}

void link_watch_loop()
{
   // We deleted all models or are we searching?

   if ( NULL == g_pCurrentModel || g_bSearching )
   {
      g_bVideoLost = false;
      link_watch_remove_popups();
      return;
   }


   if ( ! pairing_isStarted() )
   {
      g_bVideoLost = false;
      return;
   }

   link_watch_loop_telemetry();
   link_watch_loop_popup_looking();
   link_watch_loop_unexpected_vehicles();
   link_watch_link_lost();
   link_watch_loop_throttled();
   link_watch_loop_video();
   link_watch_loop_processes();
   link_watch_rc();
}

bool link_has_received_videostream(u32 uVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (uVehicleId == 0) || (g_SM_RadioStats.radio_streams[i][0].uVehicleId == uVehicleId) || (g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].uVehicleId == uVehicleId) )
      if ( g_SM_RadioStats.radio_streams[i][STREAM_ID_VIDEO_1].totalRxBytes >= 20000 )
         return true;
   }

   return false;
}

bool link_has_received_vehicle_telemetry_info(u32 uVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
      if ( g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo )
         return true;
   }
   return false;
}

bool link_has_received_main_vehicle_ruby_telemetry()
{
   if ( NULL == g_pCurrentModel )
      return false;

   if ( ! g_VehiclesRuntimeInfo[0].bGotRubyTelemetryInfo )
      return false;
 
   if ( g_bFirstModelPairingDone )
   if ( g_VehiclesRuntimeInfo[0].uVehicleId == g_pCurrentModel->vehicle_id )
      return true;

   if ( ! g_bFirstModelPairingDone )
   if ( g_VehiclesRuntimeInfo[0].uVehicleId != 0 )
      return true;

   return false;
}

bool link_has_received_relayed_vehicle_telemetry_info()
{
   for( int i=1; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId != 0 )
      if ( g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo )
         return true;
   }
   return false;
}


bool link_has_received_any_telemetry_info()
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      if ( (g_VehiclesRuntimeInfo[i].uVehicleId != 0) && g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo )
         return true;
   return false;
}

bool link_has_paired_with_main_vehicle()
{
   if ( NULL == g_pCurrentModel )
      return false;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (g_VehiclesRuntimeInfo[i].uVehicleId == g_pCurrentModel->vehicle_id) && g_VehiclesRuntimeInfo[i].bPairedConfirmed )
         return true;
   }
   return false;
}

bool link_is_relayed_vehicle_online()
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId != g_pCurrentModel->relay_params.uRelayedVehicleId )
         continue;
      if ( g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo )
      if ( g_VehiclesRuntimeInfo[i].uTimeLastRecvRubyTelemetry > g_TimeNow - 2000 )
         return true;
   }
   return false;
}

bool link_is_vehicle_online_now(u32 uVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId != uVehicleId )
         continue;
      if ( g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo )
      if ( g_VehiclesRuntimeInfo[i].uTimeLastRecvAnyRubyTelemetry > g_TimeNow - 2000 )
         return true;
   }
   return false; 
}