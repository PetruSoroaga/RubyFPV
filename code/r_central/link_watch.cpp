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
#include "../base/launchers.h"
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

extern bool s_bDebugOSDShowAll;

u32 s_LastTelemetryTimestamp = 0;
u32 s_TelemetryLostCheckCount = 0;

u32 s_LastThrottled = 0xFFFF;
u32 s_TimeLastProcessesCheck = 0;
u32 s_TimeLastVideoProcessingCheck = 0;
u32 s_TimeLastVideoMemoryFreeCheck = 0;
u32 s_TimeSecondsCheck = 0;
u32 s_TimeLastWarningRCHID = 0;
u32 s_TimeLastAlarmsCheck = 0;
u32 s_TimeTriggerAlarmOverloadFEC = 0;
u32 s_TimeLastAlarmRadioLinkBehind = 0;
u32 s_TimeLastAlarmRouterProcess = 0;

u32 s_LastRouterProcessCheckedAlarmFlags = 0;

u32 s_CountProcessRouterFailures = 0;
u32 s_CountProcessTelemetryFailures = 0;

u8 s_LastFCFlags = 0;
u8 s_LastFCFlightMode = 0;

bool s_bIsThrottled = false;
bool s_bIsUndervoltage = false;
bool s_bIsHot = false;
bool s_bIsFreqCapped = false;
bool s_bFCTelemetryPresent = false;
bool s_bLinkWatchIsRCOutputEnabled = false;
u32 s_TimeLastMessageFromFC = 0;
char s_szLastMessageFromFC[FC_MESSAGE_MAX_LENGTH];

static bool s_bCheckForFirstPairingFlag = true;

void link_watch_init()
{
   log_line("link watch init.");
}

void link_watch_reset()
{
   s_LastTelemetryTimestamp = 0;
   s_TelemetryLostCheckCount = 0;

   s_TimeSecondsCheck = 0;
   s_LastThrottled = 0xFFFF;
   s_LastFCFlags = 0;
   s_LastFCFlightMode = 0;
   s_TimeLastWarningRCHID = 0;
   s_TimeLastAlarmRadioLinkBehind = 0;
   s_TimeLastAlarmRouterProcess = 0;

   s_LastRouterProcessCheckedAlarmFlags = 0;

   s_bIsThrottled = false;
   s_bIsUndervoltage = false;
   s_bIsHot = false;
   s_bIsFreqCapped = false;
   s_bFCTelemetryPresent = false;

   g_bTelemetryLost = false;
   g_bVideoLost = false;
   s_TimeLastMessageFromFC = 0;
   s_szLastMessageFromFC[0] = 0;

   s_TimeLastVideoMemoryFreeCheck = 0;
   link_watch_mark_started_video_processing();

   if ( NULL != g_pPopupLooking )
   {
      popups_remove(g_pPopupLooking);
      g_pPopupLooking = NULL;
      log_line("Removed popup looking for model (3).");
   }
}

void link_watch_mark_started_video_processing()
{
   s_TimeLastVideoProcessingCheck = g_TimeNow;
}


void link_watch_loop_popup_looking()
{
   if ( g_bSearching )
   {
      if ( NULL != g_pPopupLooking )
      {
         popups_remove(g_pPopupLooking);
         g_pPopupLooking = NULL;
         log_line("Removed popup looking for model (5).");
      }
      if ( NULL != g_pPopupWrongModel )
      {
         popups_remove(g_pPopupWrongModel);
         g_pPopupWrongModel = NULL;
         log_line("Removed popup wrong model (4).");
      }
      return;
   }

   // We deleted all models?
   if ( NULL == g_pCurrentModel )
   {
      if ( NULL != g_pPopupLooking )
      {
         popups_remove(g_pPopupLooking);
         g_pPopupLooking = NULL;
         log_line("Removed popup looking for model (6).");
      }
      if ( NULL != g_pPopupWrongModel )
      {
         popups_remove(g_pPopupWrongModel);
         g_pPopupWrongModel = NULL;
         log_line("Removed popup wrong model (5).");
      }
      return;
   }

   // If we just paired, wait a litte;
   if ( g_TimeNow < pairing_getStartTime() + 1000 )
      return;

   // Wrong model ?
   if ( pairing_is_connected_to_wrong_model() )
   {
      // First time ever pairing? Then use it as is.
      if( ! ruby_is_first_pairing_done() )
      {
         log_line("First time pairing. Updating the default model with the correct vehicle ID (received vehicle ID: %u, runtimeinfo index: %d).", g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id, g_iCurrentActiveVehicleRuntimeInfoIndex);
         ruby_set_is_first_pairing_done();
         g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].uVehicleId = g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id;
         g_pCurrentModel->vehicle_id = g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id;
         g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, true);
         if ( 0 == getModelsCount() )
            addNewModel();
         replaceModel(0, g_pCurrentModel);
         saveModels();
         onModelAdded( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id);
         log_line("First time pairing complete. Updated default model vehicle ID to %u.", g_pCurrentModel->vehicle_id);
         log_line("Notifying other components that model was updated.");
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         return;
      }
      if ( NULL == g_pPopupWrongModel && (! g_bSearching) )
      {
         int iReceivedVehicleRuntimeIndex = 0;
         for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
         {
            if ( 0 == g_VehiclesRuntimeInfo[i].uVehicleId || MAX_U32 == g_VehiclesRuntimeInfo[i].uVehicleId )
               continue;
            if ( ! g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo )
               continue;

            if ( g_VehiclesRuntimeInfo[i].headerRubyTelemetryExtended.vehicle_id == g_pCurrentModel->vehicle_id )
               continue;
            iReceivedVehicleRuntimeIndex = i;
            break;
         }

         log_line("Received data from wrong model. Current vehicle ID: %u, received telemetry vehicle ID: %u",
            g_pCurrentModel->vehicle_id, g_VehiclesRuntimeInfo[iReceivedVehicleRuntimeIndex].headerRubyTelemetryExtended.vehicle_id);
         char szBuff[256];
         char szName[128];
         szName[0] = 0;
         //snprintf(szName, sizeof(szName),"%s ", Model::getVehicleType(g_VehiclesRuntimeInfo[iReceivedVehicleRuntimeIndex].headerRubyTelemetryExtended.vehicle_type));
         if ( 0 == g_VehiclesRuntimeInfo[iReceivedVehicleRuntimeIndex].headerRubyTelemetryExtended.vehicle_name[0] )
            strlcat(szName, "No Name", sizeof(szName));
         else
            snprintf(szName, sizeof(szName), "%.*s", MAX_VEHICLE_NAME_LENGTH, (char*)g_VehiclesRuntimeInfo[iReceivedVehicleRuntimeIndex].headerRubyTelemetryExtended.vehicle_name);
         if ( 0 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
            snprintf(szBuff, sizeof(szBuff), "Warning: There is a different vehicle (%s) on the same frequency as your current vehicle (%s)!", szName, g_pCurrentModel->getShortName());
         else if ( 1 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
            snprintf(szBuff, sizeof(szBuff), "Warning: There is a different vehicle (%s) on the same frequency (%s) as your current vehicle (%s)!", szName, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[0]), g_pCurrentModel->getShortName());
         else if ( 2 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
         {
            char szFreq1[64];
            char szFreq2[64];
            strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[0]));
            strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[1]));
            snprintf(szBuff, sizeof(szBuff), "Warning: There is a different vehicle (%s) on the same frequencies (%s/%s) as your current vehicle (%s)!", szName, szFreq1, szFreq2, g_pCurrentModel->getShortName());
         }
         else
         {
            char szFreq1[64];
            char szFreq2[64];
            char szFreq3[64];
            strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[0]));
            strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[1]));
            strcpy(szFreq3, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[2]));
            snprintf(szBuff, sizeof(szBuff), "Warning: There is a different vehicle (%s) on the same frequencies (%s/%s/%s) as your current vehicle (%s)!", szName, szFreq1, szFreq2, szFreq3, g_pCurrentModel->getShortName());
         }
         g_pPopupWrongModel = new Popup(szBuff, 0.2, 0.32, 0.5, 0);
         g_pPopupWrongModel->setCentered();
         //g_pPopupWrongModel->addLine("Another vehicle is using the same frequency as your vehicle.");
         g_pPopupWrongModel->setIconId(g_idIconWarning, get_Color_IconWarning());
         popups_add(g_pPopupWrongModel);

         bool bFoundModel = false;
         int vehicleIndex = -1;
         for( int i=0; i<getModelsCount(); i++ )
         {
            Model* pModel = getModelAtIndex(i);
            if ( pModel != NULL )
            if ( g_VehiclesRuntimeInfo[iReceivedVehicleRuntimeIndex].bGotRubyTelemetryInfo )
            if ( pModel->vehicle_id == g_VehiclesRuntimeInfo[iReceivedVehicleRuntimeIndex].headerRubyTelemetryExtended.vehicle_id )
            {
               bFoundModel = true;
               vehicleIndex = i;
               break;
            }
         }
         log_line("Found existing model? %s, index: %d", (bFoundModel?"yes":"no"), vehicleIndex);
         log_line("There is a different vehicle (UID: %ld) than expected one (UID: %ld) on the current frequency.", g_VehiclesRuntimeInfo[iReceivedVehicleRuntimeIndex].headerRubyTelemetryExtended.vehicle_id, g_pCurrentModel->vehicle_id );
         if ( bFoundModel && -1 != vehicleIndex )
            add_menu_to_stack(new MenuSwitchVehicle(vehicleIndex));
      }
      return;      
   }

   // Getting data from the right (expected) vehicle?
   
   if ( pairing_isStarted() && pairing_isReceiving() && (NULL != g_pCurrentModel) && g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id == g_pCurrentModel->vehicle_id )
   {
      if ( NULL != g_pPopupWrongModel )
      {
         popups_remove(g_pPopupWrongModel);
         g_pPopupWrongModel = NULL;
         log_line("Removed popup wrong model. We are getting data from the expected model UID: %u", g_pCurrentModel->vehicle_id);
      }

      if ( NULL != g_pPopupLooking )
      {
         popups_remove(g_pPopupLooking);
         g_pPopupLooking = NULL;
         log_line("Removed popup looking for vehicle. We are getting data from model UID: %u", g_pCurrentModel->vehicle_id);
         if ( g_bSyncModelSettingsOnLinkRecover )
         {
            g_bSyncModelSettingsOnLinkRecover = false;
            if ( NULL != g_pCurrentModel )
               g_pCurrentModel->b_mustSyncFromVehicle = true;
         }
      }

      if ( s_bCheckForFirstPairingFlag )
      {
         s_bCheckForFirstPairingFlag = false;
         if ( ! ruby_is_first_pairing_done() )
         {
            ruby_set_is_first_pairing_done();
            log_line("Mark first time pairing as complete.");
            log_line("Notifying other components that first time pairing is done.");
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         }
      }
   }

   // Link lost?
   if ( ! pairing_isReceiving() )
   {
      if ( pairing_wasReceiving() && (NULL != g_pCurrentModel && pairing_getCurrentVehicleID() == g_pCurrentModel->vehicle_id))
      {
         if ( NULL == g_pPopupLooking )
         {
            g_pPopupLooking = new Popup("Link lost. Trying to reconnect...", 0.28, 0.3, 0.5, 0);
            g_pPopupLooking->setCentered();
            g_pPopupLooking->addLine("The radio link with the vehicle is lost. Will reconnect automatically when in range.");
            g_pPopupLooking->setIconId(g_idIconWarning, get_Color_IconWarning());
            popups_add(g_pPopupLooking);
         }
         return;
      }
      if ( NULL == g_pPopupLooking )
      {
         char szText[256];
         u32 idIcon = 0;
         if( (! ruby_is_first_pairing_done()) || NULL == g_pCurrentModel )
         {
            u32 freq = 0;
            if ( NULL != g_pCurrentModel )
            {
               radio_hw_info_t* pNICInfo = hardware_get_radio_info(0);
               if ( NULL != pNICInfo )
                  freq = pNICInfo->uCurrentFrequency;
            }
            snprintf(szText, sizeof(szText), "Looking for default vehicle (%s)...", str_format_frequency(freq));
         }
         else
         {
            idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );
            if ( g_pCurrentModel->radioInterfacesParams.interfaces_count < 2 )
               snprintf(szText, sizeof(szText), "Looking for %s (%s, %s)...", g_pCurrentModel->getLongName(), g_pCurrentModel->is_spectator?"Spectator Mode":"Control Mode", str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[0]));
            else
            {
               if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId <= 0 )
               {
                  char szFreq1[64];
                  char szFreq2[64];
                  strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[0]));
                  strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[1]));
                  snprintf(szText, sizeof(szText), "Looking for %s (%s, %s/%s)...", g_pCurrentModel->getLongName(), g_pCurrentModel->is_spectator?"Spectator Mode":"Control Mode", szFreq1, szFreq2);
               }
               else
               {
                   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId-1 == 0 )
                     snprintf(szText, sizeof(szText), "Looking for %s (%s, %s)...", g_pCurrentModel->getLongName(), g_pCurrentModel->is_spectator?"Spectator Mode":"Control Mode", str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[1]));
                   else
                     snprintf(szText, sizeof(szText), "Looking for %s (%s, %s)...", g_pCurrentModel->getLongName(), g_pCurrentModel->is_spectator?"Spectator Mode":"Control Mode", str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[0]));
               }
            }
         }
         g_pPopupLooking = new Popup(szText, 0.2, 0.3, 0.5, 0);
         g_pPopupLooking->setCentered();
         g_pPopupLooking->setIconId(idIcon, get_Color_MenuText());
         g_pPopupLooking->addLine("Power up your vehicle. The connection will be made automatically when the vehicle is in radio range.");
         g_pPopupLooking->addLine("Or press the [Menu] key to open up the menu and switch vehicles or search for other vehicles.");
         popups_add(g_pPopupLooking);
      }
      return;
   }


   // It's connected now

   if ( NULL != g_pPopupLooking )
   {
      popups_remove(g_pPopupLooking);
      g_pPopupLooking = NULL;
      if ( g_bSyncModelSettingsOnLinkRecover )
      {
         g_bSyncModelSettingsOnLinkRecover = false;
         if ( NULL != g_pCurrentModel )
            g_pCurrentModel->b_mustSyncFromVehicle = true;
      }
      log_line("Removed popup looking for vehicle (2).");
   }

   if ( NULL != g_pPopupWrongModel )
   {
      popups_remove(g_pPopupWrongModel);
      g_pPopupWrongModel = NULL;
      log_line("Removed popup wrong model (2).");
   }
}

void link_watch_loop_throttled()
{
   // If we just paired, wait a litte;
   if ( pairing_getStartTime() > g_TimeNow-500 )
      return;

   if ( pairing_is_connected_to_wrong_model() )
      return;

   u8 flags = g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.throttled;

   if ( s_LastThrottled != flags )
   {
      if ( flags & 0b1000 )
      {
         if ( ! s_bIsHot )
            warnings_add("Vehicle: High temperature!", g_idIconTemperature, get_Color_IconError());
         s_bIsHot = true;
      }
      else
      {
         if ( s_bIsHot )
            warnings_add("Vehicle: Temperature back to normal", g_idIconTemperature, get_Color_IconSucces());
         s_bIsHot = false;
      }

      if ( flags & 0b0100 )
      {
         if ( ! s_bIsThrottled )
            warnings_add("Vehicle: CPU is throttled", g_idIconCPU, get_Color_IconError());
         s_bIsThrottled = true;
      }
      else
      {
         if ( s_bIsThrottled )
            warnings_add("Vehicle: CPU throttling cleared", g_idIconCPU, get_Color_IconSucces());
         s_bIsThrottled = false;
      }

      if ( flags & 0b0010 )
      {
         if ( ! s_bIsFreqCapped )
            warnings_add("Vehicle: CPU frequency capped", g_idIconCPU, get_Color_IconError());
         s_bIsFreqCapped = true;
      }
      else
      {
         if ( s_bIsFreqCapped )
            warnings_add("Vechicle: CPU frequency back to normal", g_idIconCPU, get_Color_IconSucces());
         s_bIsFreqCapped = false;
      }

      if ( flags & 0b0001 )
      {
         if ( ! s_bIsUndervoltage )
            warnings_add("Vechicle: Undervoltage detected", g_idIconCPU, get_Color_IconError());
         s_bIsUndervoltage = true;
      }
      else
      {
         if ( s_bIsUndervoltage )
            warnings_add("Vehicle: Undervoltage cleared", g_idIconCPU, get_Color_IconSucces());
         s_bIsUndervoltage = false;
      }
   }

   s_LastThrottled = flags;
}


void link_watch_loop_telemetry()
{
   if ( NULL == g_pSM_RadioStats )
      return;

   // If we just paired, wait a litte;
   if ( pairing_getStartTime() > g_TimeNow-3000 )
      return;

   // If we never connected, return;
   if ( (!pairing_isReceiving()) && (! pairing_wasReceiving()) )
      return;

   if ( pairing_is_connected_to_wrong_model() )
      return;

   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
   {
      if ( s_LastFCFlightMode != (g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flight_mode & (~FLIGHT_MODE_ARMED)) )
      {
         bool bShow = false;
         if ( 0 != s_LastFCFlightMode )
            bShow = true;
         s_LastFCFlightMode = (g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flight_mode & (~FLIGHT_MODE_ARMED));
         if ( bShow )
            notification_add_flight_mode(s_LastFCFlightMode);
      }
      #ifdef FEATURE_ENABLE_RC
      if ( ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_RC_FAILSAFE ) ||
           (g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo && (g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.flags & FLAG_RUBY_TELEMETRY_RC_FAILSAFE) ) )
      {
         if ( ! g_bRCFailsafe )
            notification_add_rc_failsafe();
         g_bRCFailsafe = true;
      }
      else if ( g_bRCFailsafe )
      {
         notification_add_rc_failsafe_cleared();
         g_bRCFailsafe = false;
      }
      #endif

      if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags != s_LastFCFlags )
      {
          if ( 0 != s_LastFCFlags )
          {
          if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED )
          if ( !(s_LastFCFlags & FC_TELE_FLAGS_ARMED) )
          {
             onEventArmed();
          }
          if ( !(g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED) )
          if ( s_LastFCFlags & FC_TELE_FLAGS_ARMED )
          {
             onEventDisarmed();
          }
          }
          s_LastFCFlags = g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags;
       }

       if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_NO_FC_TELEMETRY )
       {
          if ( s_bFCTelemetryPresent )
             warnings_add("Flight controller telemetry missing", g_idIconCPU, get_Color_IconError());
          s_bFCTelemetryPresent = false;
       }
       else
       {
          if ( ! s_bFCTelemetryPresent )
             warnings_add("Flight controller telemetry recovered", g_idIconCPU, get_Color_IconSucces());
          s_bFCTelemetryPresent = true;
       }

       if ( (g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_HAS_MESSAGE) && g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].bGotFCTelemetryExtra )
       if ( pairing_isReceiving() )
       {
          if ( 0 != strncmp(s_szLastMessageFromFC, (const char*)(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetryExtra.text), FC_MESSAGE_MAX_LENGTH-1) ||
               g_TimeNow > s_TimeLastMessageFromFC + 5000 )
          {
             strcpy(s_szLastMessageFromFC, (const char*)(g_VehiclesRuntimeInfo[g_iCurrentOSDVehicleRuntimeInfoIndex].headerFCTelemetryExtra.text));
             char szBuff[512];
             if ( s_szLastMessageFromFC[0] == 'R' && s_szLastMessageFromFC[1] == ':' )
             {
                strcpy(szBuff, "Ruby: ");
                strlcat(szBuff, &(s_szLastMessageFromFC[0]), sizeof(szBuff));
             }
             else
             {
                strcpy(szBuff, "Vehicle: ");
             	  strlcat(szBuff, s_szLastMessageFromFC, sizeof(szBuff));
             }
             warnings_add(szBuff, g_idIconInfo, get_Color_IconNormal(), 8);
             s_TimeLastMessageFromFC = g_TimeNow;
          }
       }
   }

   if ( g_pSM_RadioStats->radio_streams[STREAM_ID_DATA].timeLastRxPacket == 0 || g_TimeNow < TIMEOUT_TELEMETRY_LOST )
      return;


   if ( (NULL != g_pProcessStatsTelemetry) && (g_pProcessStatsTelemetry->timeLastReceivedPacket != 0) && (s_LastTelemetryTimestamp == g_pProcessStatsTelemetry->timeLastReceivedPacket) )
   {
      u32 uMaxLostTime = TIMEOUT_TELEMETRY_LOST;
      if ( NULL != g_pCurrentModel && g_pCurrentModel->telemetry_params.update_rate > 10 )
         uMaxLostTime = TIMEOUT_TELEMETRY_LOST/2;
      if ( g_TimeNow > uMaxLostTime )
      if ( s_LastTelemetryTimestamp != 0  && s_LastTelemetryTimestamp < g_TimeNow - uMaxLostTime )
      {
         s_TelemetryLostCheckCount++;
         if ( s_TelemetryLostCheckCount > 3 )
         {
            if ( ! g_bTelemetryLost )
               warnings_add("Telemetry Lost!", g_idIconCPU, get_Color_IconError());
            g_bTelemetryLost = true;
         }
      }
      return;
   }

   if ( (NULL != g_pProcessStatsTelemetry) && (g_pProcessStatsTelemetry->timeLastReceivedPacket != 0) )
      s_LastTelemetryTimestamp = g_pProcessStatsTelemetry->timeLastReceivedPacket;
   s_TelemetryLostCheckCount = 0;
   if ( g_bTelemetryLost )
   {
      g_bTelemetryLost = false;
      warnings_add("Telemetry Recovered", g_idIconCPU, get_Color_IconSucces());
   }
}

void link_watch_loop_video()
{   
   if ( ! g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo )
      return;
   if ( NULL == g_pProcessStatsRouter )
      return;

   if ( s_LastRouterProcessCheckedAlarmFlags != g_pProcessStatsRouter->alarmFlags ||
        g_pProcessStatsRouter->alarmTime > s_TimeLastAlarmRouterProcess )
   {
      s_LastRouterProcessCheckedAlarmFlags = g_pProcessStatsRouter->alarmFlags;
      s_TimeLastAlarmRouterProcess = g_pProcessStatsRouter->alarmTime;

      if ( g_pProcessStatsRouter->alarmFlags & PROCESS_ALARM_RADIO_STREAM_RESTARTED )
      {
         warnings_add("Video stream was restarted on vehicle.", g_idIconWarning);
      }

      if ( g_pProcessStatsRouter->alarmFlags & PROCESS_ALARM_RADIO_INTERFACE_BEHIND )
      if ( g_TimeNow > s_TimeLastAlarmRadioLinkBehind + 2000 )
      {
         ControllerSettings* pCS = get_ControllerSettings();
         if ( NULL != pCS && (0 != pCS->iDeveloperMode) )
         {
            s_TimeLastAlarmRadioLinkBehind = g_TimeNow;
            char szBuff[256];
            //snprintf(szBuff, sizeof(szBuff), "Link %d is behind on stream id %d, received packet index: %u, maximum packet index for this stream is: %u.", g_pProcessStatsRouter->alarmParam[2], g_pProcessStatsRouter->alarmParam[0], g_pProcessStatsRouter->alarmParam[3], g_pProcessStatsRouter->alarmParam[1] );
            snprintf(szBuff, sizeof(szBuff), "Link %d is behind on stream id %d by %d packets", g_pProcessStatsRouter->alarmParam[2], g_pProcessStatsRouter->alarmParam[0], g_pProcessStatsRouter->alarmParam[1] - g_pProcessStatsRouter->alarmParam[3]);
            warnings_add(szBuff, g_idIconWarning);
         }
      }
   }
}

void link_watch_loop_processes()
{
   if ( ! g_bSearching )
   if ( g_TimeNow > s_TimeLastProcessesCheck + 1000 )
   {
      s_TimeLastProcessesCheck = g_TimeNow;
      if ( pairing_isStarted() )
      {
         if ( NULL == g_pProcessStatsTelemetry )
         {
            g_pProcessStatsTelemetry = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_TELEMETRY_RX);
            if ( NULL == g_pProcessStatsTelemetry )
               log_softerror_and_alarm("Failed to open shared mem to telemetry rx process watchdog stats for reading: %s", SHARED_MEM_WATCHDOG_TELEMETRY_RX);
            else
               log_line("Opened shared mem to telemetry rx process watchdog stats for reading.");
         }
         if ( NULL == g_pProcessStatsRouter )
         {
            g_pProcessStatsRouter = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_ROUTER_RX);
            if ( NULL == g_pProcessStatsRouter )
               log_softerror_and_alarm("Failed to open shared mem to video rx process watchdog stats for reading: %s", SHARED_MEM_WATCHDOG_ROUTER_RX);
            else
               log_line("Opened shared mem to video rx process watchdog stats for reading.");
         }

         if ( NULL != g_pCurrentModel && g_pCurrentModel->rc_params.rc_enabled )
         if ( NULL == g_pProcessStatsRC )
         {
            g_pProcessStatsRC = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_RC_TX);
            if ( NULL == g_pProcessStatsRC )
               log_softerror_and_alarm("Failed to open shared mem to RC tx process watchdog stats for reading: %s", SHARED_MEM_WATCHDOG_RC_TX);
            else
               log_line("Opened shared mem to RC tx process watchdog stats for reading.");
         }

         if ( NULL != g_pProcessStatsRouter && g_pProcessStatsRouter->lastActiveTime < g_TimeNow - 1100 )
            s_CountProcessRouterFailures++;
         else
            s_CountProcessRouterFailures = 0;

         if ( NULL != g_pProcessStatsTelemetry && g_pProcessStatsTelemetry->lastActiveTime < g_TimeNow - 1100 )
            s_CountProcessTelemetryFailures++;
         else
            s_CountProcessTelemetryFailures = 0;

         bool bNeedsRestart = false;
         int failureCountMax = 4;

         if ( g_bVideoProcessing )
            failureCountMax = 8;
         if ( s_CountProcessRouterFailures > failureCountMax )
         {
            warnings_add("Controller router process is malfunctioning! Restarting it.", g_idIconCPU, get_Color_IconError());
            bNeedsRestart = true;
         }
         if ( s_CountProcessTelemetryFailures > failureCountMax )
         {
            warnings_add("Controller telemetry process is malfunctioning! Restarting it.", g_idIconCPU, get_Color_IconError());
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
            pairing_start();
         }
      }
   }

   if ( ! g_bSearching )
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
            snprintf(szComm, sizeof(szComm), "df %s | sed -n 2p", TEMP_VIDEO_MEM_FOLDER);
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
            warnings_add("Video file processing failed.", g_idIconCamera, get_Color_IconWarning());

            char szBuff[256];
            char * line = NULL;
            size_t len = 0;
            ssize_t read;
            FILE* fd = fopen(TEMP_VIDEO_FILE_PROCESS_ERROR, "r");

            while ( (NULL != fd) && ((read = getline(&line, &len, fd)) != -1))
            {
              printf("Retrieved line of length %zu:\n", read);
              if ( read > 0 )
                 warnings_add(line, g_idIconCamera, get_Color_IconNormal());
            }
            if ( NULL != fd )
               fclose(fd);
            snprintf(szBuff, sizeof(szBuff), "rm -rf %s 2>/dev/null",TEMP_VIDEO_FILE_PROCESS_ERROR);
            hw_execute_bash_command(szBuff, NULL );
         }
         else
             warnings_add("Video file processing complete.", g_idIconCamera, get_Color_IconNormal());
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
   if ( pairing_getStartTime() > g_TimeNow-500 )
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
             warnings_add("RC is enabled on current vehicle and the input controller device is missing!", g_idIconJoystick, get_Color_IconError());
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
          warnings_add("RC is enabled on current vehicle and the detected RC input controller device is different from the one setup on the vehicle!", g_idIconJoystick, get_Color_IconError());
          return;
      }
   }
}

void link_watch_alarms()
{
   if ( NULL == g_pCurrentModel || (!g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo) || NULL == g_pSM_RadioStats )
      return;

   if ( s_TimeLastAlarmsCheck > g_TimeNow - 200 )
      return;

   s_TimeLastAlarmsCheck = g_TimeNow;

   if ( NULL != g_psmvds && ((g_psmvds->fec_time/1000) >= 300) )
   {
      if ( 0 == s_TimeTriggerAlarmOverloadFEC )
         s_TimeTriggerAlarmOverloadFEC = g_TimeNow;
      else if ( g_TimeNow > s_TimeTriggerAlarmOverloadFEC + 500 )
      {
         //log_line("FEC time too big: %d ms/s", g_psmvds->fec_time/1000);
         if ( g_pCurrentModel->osd_params.show_overload_alarm )
            warnings_add_vehicle_overloaded();
         s_TimeTriggerAlarmOverloadFEC = 0;
      }
   }
   else
      s_TimeTriggerAlarmOverloadFEC = 0;

   long max_data_kbps = 0;
   long max_video_kbps = 0;
}

void link_watch_loop()
{
   if ( NULL == g_pCurrentModel || g_bSearching )
   {
      g_bTelemetryLost = false;
      g_bVideoLost = false;

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
      return;
   }
   if ( ! pairing_isStarted() )
   {
      g_bTelemetryLost = false;
      g_bVideoLost = false;
      return;
   }

   link_watch_loop_popup_looking();
   link_watch_loop_telemetry();
   link_watch_loop_throttled();
   link_watch_loop_video();
   link_watch_loop_processes();
   link_watch_rc();
   link_watch_alarms();
}


bool link_has_fc_telemetry()
{
   if ( s_bDebugOSDShowAll )
      return true;

   //log_line("has Fc tele: %d", s_bFCTelemetryPresent);
   return s_bFCTelemetryPresent;
}

bool link_is_relayed_vehicle_online()
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId != g_pCurrentModel->relay_params.uRelayVehicleId )
         continue;
      if ( g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo )
      if ( g_VehiclesRuntimeInfo[i].uTimeLastRecvRubyTelemetry > g_TimeNow - 2000 )
         return true;
   }
   return false;
}