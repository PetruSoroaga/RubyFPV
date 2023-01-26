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
#include "../base/hardware.h"
#include "../base/plugins_settings.h"
#include "../radio/radiolink.h"
#include "../common/string_utils.h"
#include "events.h"
#include "shared_vars.h"
#include "timers.h"
#include "local_stats.h"
#include "ruby_central.h"
#include "pairing.h"
#include "notifications.h"
#include "handle_commands.h"
#include "osd_common.h"
#include "osd.h"
#include "osd_stats.h"
#include "osd_warnings.h"
#include "link_watch.h"
#include "ui_alarms.h"
#include "warnings.h"
#include "colors.h"
#include "menu.h"
#include "menu_update_vehicle.h"
#include "menu_confirmation.h"
#include "process_router_messages.h"


void onModelAdded(u32 uModelId)
{
   log_line("[Event]: Handing event new model added (vehicle UID: %u).", uModelId);
   log_line("[Event]: Handing event new model added (vehicle UID: %u). Done.", uModelId);
}

void onModelDeleted(u32 uModelId)
{
   log_line("[Event]: Handing event model deleted (vehicle UID: %u).", uModelId);
   deletePluginModelSettings(uModelId);
   save_PluginsSettings();
   g_bGotStatsVideoBitrate = false;
   g_bGotStatsVehicleTx = false;
   g_bGotStatsVehicleRxCards = false;
   log_line("[Event]: Handing event model deleted (vehicle UID: %u). Done.", uModelId);
}


void onNewVehicleActive()
{
   log_line("[Event]: Handing event New Vehicle Selected...");

   //g_nTotalControllerCPUSpikes = 0;

   if ( NULL != g_pCurrentModel )
      local_stats_reset(g_pCurrentModel->vehicle_id);
   else
      local_stats_reset(0);

   osd_stats_init();
   
   handle_commands_reset_has_received_vehicle_core_plugins_info();

   g_uPersistentAllAlarmsVehicle = 0;
   g_bHasVideoDataOverloadAlarm = false;
   g_bHasVideoTxOverloadAlarm = false;
   g_bIsVehicleLinkToControllerLost = false;
   g_iDeltaVideoInfoBetweenVehicleController = 0;
   g_iVehicleCorePluginsCount = 0;

   if ( NULL != g_pPopupVideoOverloadAlarm )
   {
      if ( popups_has_popup(g_pPopupVideoOverloadAlarm) )
         popups_remove(g_pPopupVideoOverloadAlarm);
      g_pPopupVideoOverloadAlarm = NULL;
   }

   if ( NULL != g_pCurrentModel )
   {
      u32 scale = g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.layout] & 0xFF;
      osd_setScaleOSD((int)scale);
   
      u32 scaleStats = (g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.layout]>>16) & 0x0F;
      osd_setScaleOSDStats((int)scaleStats);

      if ( render_engine_is_raw() )
         ruby_reload_osd_fonts();

      log_line("Vehicle radio links: %d", g_pCurrentModel->radioLinksParams.links_count );
      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      {
         char szBuff[128];
         str_get_radio_frame_flags_description(g_pCurrentModel->radioLinksParams.link_radio_flags[i], szBuff);
         log_line("Vechile radio link %d radio flags: %s", i+1, szBuff);
      }
   }

   for ( int i=0; i<g_iPluginsOSDCount; i++ )
   {
      if ( NULL == g_pPluginsOSD[i] )
         continue;
      if ( NULL != g_pPluginsOSD[i]->pFunctionOnNewVehicle )
         (*(g_pPluginsOSD[i]->pFunctionOnNewVehicle))(g_pCurrentModel->vehicle_id);
   }  

   warnings_on_changed_vehicle();
   log_line("Handing event New Vehicle Selected. Done.");
}

void onEventReboot()
{
   log_line("[Event]: Handing event Reboot...");
   ruby_pause_watchdog();
   save_temp_local_stats();
   hardware_sleep_ms(50);
   //pairing_stop();

   log_line("Handling event Reboot. Done.");
}

void onEventBeforePairing()
{
   log_line("[Event]: Handing event BeforePairing...");

   s_computedOSDCellCount = 1;
   if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.battery_cell_count > 0 )
      s_computedOSDCellCount = g_pCurrentModel->osd_params.battery_cell_count;

   g_bHasVideoDataOverloadAlarm = false;
   g_bHasVideoTxOverloadAlarm = false;
   g_bIsVehicleLinkToControllerLost = false;

   g_bGotStatsVideoBitrate = false;
   g_bGotStatsVehicleTx = false;
   g_bGotStatsVehicleRxCards = false;

   g_CurrentUploadingFile.uFileId = 0;
   g_CurrentUploadingFile.uTotalSegments = 0;
   g_CurrentUploadingFile.szFileName[0] = 0;
   for( int i=0; i<MAX_SEGMENTS_FILE_UPLOAD; i++ )
   {
      g_CurrentUploadingFile.pSegments[i] = NULL;
      g_CurrentUploadingFile.uSegmentsSize[i] = 0;
      g_CurrentUploadingFile.bSegmentsUploaded[i] = false;
   }
   g_bHasFileUploadInProgress = false;

   warnings_remove_all();
   log_line("Handing event BeforePairing. Done.");
}

void onEventPaired()
{
   log_line("[Event]: Handing event Paired...");

   link_watch_reset();

   log_line("[Event]: Handing event Paired. Done.");
}

void onEventPairingStopped()
{
   log_line("[Event]: Handing event Pairing Stopped...");
   g_bRCFailsafe = false;
   g_TimePendingRadioFlagsChanged = 0;
   g_PendingRadioFlagsConfirmationLinkId = -1;
   g_bHasVideoDataOverloadAlarm = false;
   g_bHasVideoTxOverloadAlarm = false;

   g_bGotStatsVideoBitrate = false;
   g_bGotStatsVehicleTx = false;
   g_bGotStatsVehicleRxCards = false;

   g_bHasVideoDecodeStatsSnapshot = false;

   if ( NULL != g_pPopupVideoOverloadAlarm )
   {
      if ( popups_has_popup(g_pPopupVideoOverloadAlarm) )
         popups_remove(g_pPopupVideoOverloadAlarm);
      g_pPopupVideoOverloadAlarm = NULL;
   }

   osd_warnings_reset();
   warnings_remove_all();
   log_line("Handing event Pairing Stopped. Done.");
}

void onEventPairingStartReceivingData()
{
   log_line("[Event]: Started receiving data from vehicle.");

   if ( NULL != g_pPopupLooking )
   {
      popups_remove(g_pPopupLooking);
      g_pPopupLooking = NULL;
      log_line("Removed popup looking for model (4).");
   }

   if ( NULL != g_pPopupWrongModel )
   {
      popups_remove(g_pPopupWrongModel);
      g_pPopupWrongModel = NULL;
      log_line("Removed popup wrong model (3).");
   }

   if ( g_bSyncModelSettingsOnLinkRecover )
   {
      g_bSyncModelSettingsOnLinkRecover = false;
      if ( NULL != g_pCurrentModel )
         g_pCurrentModel->b_mustSyncFromVehicle = true;
   }
}

void onEventArmed()
{
   log_line("Event: Handing event OnArmed...");
   log_line("Vehicle is Armed");
   local_stats_on_arm();
   notification_add_armed();
   osd_remove_stats_flight_end();
   Preferences* p = get_Preferences();
   if ( NULL != p && p->iStartVideoRecOnArm )
      ruby_start_recording();
   log_line("Handing event OnArmed. Done.");
}

void onEventDisarmed()
{
   log_line("[Event]: Handing event OnDisarmed...");
   log_line("Vehicle is Disarmed");
   local_stats_on_disarm();
   notification_add_disarmed();
   osd_add_stats_flight_end();
   Preferences* p = get_Preferences();
   if ( NULL != p && p->iStopVideoRecOnDisarm )
      ruby_stop_recording();
   log_line("Handing event OnDisarmed. Done.");
}

bool onEventReceivedModelSettings(u8* pBuffer, int length, bool bUnsolicited)
{
   log_line("[Events]: Handing event OnReceivedModelSettings (%d bytes, expected: %s)...", length, (bUnsolicited?"no":"yes"));

   bool bOldAudioEnabled = false;
   osd_parameters_t osd_temp;

   if ( NULL != g_pCurrentModel )
   {
      bOldAudioEnabled = g_pCurrentModel->audio_params.enabled;
      memcpy((u8*)&osd_temp, (u8*)&(g_pCurrentModel->osd_params), sizeof(osd_parameters_t));
   }
   FILE* fd = fopen("tmp/last_recv_model.mdl", "wb");
   if ( NULL != fd )
   {
       fwrite(pBuffer, 1, length, fd);
       fclose(fd);
       fd = NULL;
   }
   else
   {
      log_error_and_alarm("Failed to save received vehicle configuration to temp file.");
      log_error_and_alarm("[Event]: Failed to process received model settings from vehicle.");
      return false;
   }

   fd = fopen("tmp/last_recv_model.bak", "wb");
   if ( NULL != fd )
   {
       fwrite(pBuffer, 1, length, fd);
       fclose(fd);
       fd = NULL;
   }
   else
   {
      log_error_and_alarm("Failed to save received vehicle configuration to temp file.");
      log_error_and_alarm("[Event]: Failed to process received model settings from vehicle.");
      return false;
   }

   Model modelTemp;
   if ( ! modelTemp.loadFromFile("tmp/last_recv_model.mdl", true) )
   {
      log_softerror_and_alarm("HCommands: Failed to load the received vehicle model file. Invalid file.");
      log_error_and_alarm("[Event]: Failed to process received model settings from vehicle.");
      warnings_add_error_null_model(1);
      return false;
   }

   if ( NULL != g_pCurrentModel )
      log_line("Current (before update) local model is in controll mode: %s", g_pCurrentModel->is_spectator?"no":"yes");
   else
      log_line("Current (before update) local model is NULL.");
   log_line("Currently received temp model is in controll mode: %s", modelTemp.is_spectator?"no":"yes");
   if ( modelTemp.is_spectator )
       log_softerror_and_alarm("Received model is set as spectator mode.");

   log_line("Currently received temp model is in developer mode: %s, total flights: %u", modelTemp.bDeveloperMode?"yes":"no", modelTemp.m_Stats.uTotalFlights);
   log_line("Currently received temp model osd layout: %d, enabled: %s", modelTemp.osd_params.layout, (modelTemp.osd_params.osd_flags2[modelTemp.osd_params.layout] & OSD_FLAG_EXT_LAYOUT_ENABLED)?"yes":"no");
   log_line("Currently received temp model developer flags: %s", str_get_developer_flags(modelTemp.uDeveloperFlags));
   log_line("Received vehicle info has %d radio links.", modelTemp.radioLinksParams.links_count );
   
   bool bRadioChanged = false;

   if ( g_pCurrentModel->radioLinksParams.links_count != modelTemp.radioLinksParams.links_count )
      bRadioChanged = true;
   for( int i=0; i<modelTemp.radioLinksParams.links_count; i++ )
   {
      char szBuff[128];
      char szBuffC[128];
      if ( ! bRadioChanged )
      if ( (g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] != modelTemp.radioLinksParams.link_capabilities_flags[i]) || 
           (g_pCurrentModel->radioLinksParams.link_radio_flags[i] != modelTemp.radioLinksParams.link_radio_flags[i]) ||
           (g_pCurrentModel->radioLinksParams.link_frequency[i] != modelTemp.radioLinksParams.link_frequency[i]))
         bRadioChanged = true;

      str_get_radio_capabilities_description(modelTemp.radioLinksParams.link_capabilities_flags[i], szBuffC);   
      str_get_radio_frame_flags_description(modelTemp.radioLinksParams.link_radio_flags[i], szBuff);
      log_line("Received vechile info: radio link %d: %d Mhz, capabilities flags: %s, radio flags: %s", i+1, modelTemp.radioLinksParams.link_frequency[i], szBuffC, szBuff);
   }

   bool is_spectator = g_pCurrentModel->is_spectator;
   fd = fopen(FILE_CURRENT_VEHICLE_MODEL, "wb");
   if ( NULL != fd )
   {
       fwrite(pBuffer, 1, length, fd);
       fclose(fd);
       fd = NULL;
   }
   else
   {
      log_error_and_alarm("Failed to save received vehicle configuration to file: %s", FILE_CURRENT_VEHICLE_MODEL);
      return false;
   }

   if ( !g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
   {
      log_softerror_and_alarm("HCommands: Failed to load the current vehicle model. Invalid file.");
      warnings_add_error_null_model(1);
      return false;
   }

   local_stats_reset_home_set();

   if ( bUnsolicited )
      warnings_add("Received vehicle settings.", g_idIconCheckOK);
   else
      warnings_add("Got vehicle settings.", g_idIconCheckOK);

   log_line("The vehicle has Ruby version %d.%d (b%d) (%u) and the controller %d.%d (b%d) (%u)", ((g_pCurrentModel->sw_version)>>8) & 0xFF, (g_pCurrentModel->sw_version) & 0xFF, ((g_pCurrentModel->sw_version)>>16), g_pCurrentModel->sw_version, SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR, SYSTEM_SW_BUILD_NUMBER, (SYSTEM_SW_VERSION_MAJOR*256+SYSTEM_SW_VERSION_MINOR) | (SYSTEM_SW_BUILD_NUMBER<<16) );
   
   bool bMustUpdate = false;  
   if ( ((u32)SYSTEM_SW_VERSION_MAJOR)*(int)256 + (u32)SYSTEM_SW_VERSION_MINOR > (g_pCurrentModel->sw_version & 0xFFFF) )
      bMustUpdate = true;
   if ( ((u32)SYSTEM_SW_VERSION_MAJOR)*(int)256 + (u32)SYSTEM_SW_VERSION_MINOR == (g_pCurrentModel->sw_version & 0xFFFF) )
   if ( SYSTEM_SW_BUILD_NUMBER > (g_pCurrentModel->sw_version >> 16) )
      bMustUpdate = true;
   if ( bMustUpdate )
   {
      char szBuff[256];
      char szBuff2[32];
      char szBuff3[32];
      getSystemVersionString(szBuff2, g_pCurrentModel->sw_version);
      getSystemVersionString(szBuff3, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
      sprintf(szBuff, "Your vehicle has Ruby version %s (b%d) and your controller %s (b%d). You should update your vehicle.", szBuff2, g_pCurrentModel->sw_version>>16, szBuff3, SYSTEM_SW_BUILD_NUMBER);
      warnings_add(szBuff, 0, NULL, 12);
      bool bArmed = false;
      if ( pairing_isReceiving() || pairing_wasReceiving() )
      if ( NULL != g_pdpfct )
      if ( g_pdpfct->flags & FC_TELE_FLAGS_ARMED )
         bArmed = true;
      if ( ! bArmed )
      if ( ! isMenuOn() )
      if ( ! g_bMenuPopupUpdateVehicleShown )
      {
          add_menu_to_stack( new MenuUpdateVehiclePopup(-1) );
          g_bMenuPopupUpdateVehicleShown = true;
      }
   }

   g_pCurrentModel->is_spectator = is_spectator;
   g_pCurrentModel->b_mustSyncFromVehicle = false;

   if ( g_pCurrentModel->is_spectator )
      memcpy((u8*)&(g_pCurrentModel->osd_params), (u8*)&osd_temp, sizeof(osd_parameters_t));

   log_line("Current model is in developer mode: %s", g_pCurrentModel->bDeveloperMode?"yes":"no");
   log_line("Current model is spectator: %s", g_pCurrentModel->is_spectator?"yes":"no");
   log_line("Current on time: %02d:%02d, total flights: %u", g_pCurrentModel->m_Stats.uCurrentOnTime/60, g_pCurrentModel->m_Stats.uCurrentOnTime%60, g_pCurrentModel->m_Stats.uTotalFlights);
   saveAsCurrentModel(g_pCurrentModel);
   log_line("[Events] Updated current local vehicle with received settings.");

   g_pCurrentModel->logVehicleRadioInfo();

   // Check supported cards

   if ( NULL != g_pCurrentModel )
   {
      int countUnsuported = 0;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         if ( 0 == (g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF0000) )
            countUnsuported++;
   
      if ( countUnsuported == g_pCurrentModel->radioInterfacesParams.interfaces_count )
      {
         Popup* p = new Popup("No radio interface on your vehicle is fully supported.", 0.3,0.4, 0.5, 6);
         p->setIconId(g_idIconError, get_Color_IconError());
         popups_add_topmost(p);
      }
      else if ( countUnsuported > 0 )
      {
         Popup* p = new Popup("Some radio interfaces on your vehicle are not fully supported.", 0.3,0.4, 0.5, 6);
         p->setIconId(g_idIconWarning, get_Color_IconWarning());
         popups_add_topmost(p);
      }
   }

   if ( g_pCurrentModel->alarms & ALARM_ID_UNSUPORTED_USB_SERIAL )
      warnings_add("Your vehicle has an unsupported USB to Serial adapter. Use brand name serial adapters or ones with  CP2102 chipset. The ones with 340 chipset are not compatible.", g_idIconError);

   if ( g_pCurrentModel->audio_params.enabled )
   {
      if ( ! g_pCurrentModel->audio_params.has_audio_device )
         warnings_add("Your vehicle has audio enabled but no audio capture device", g_idIconError);
      else
      {
         char szOutput[4096];
         hw_execute_bash_command_raw("aplay -l 2>&1", szOutput );
         if ( NULL != strstr(szOutput, "no soundcards") )
            warnings_add("Your vehicle has audio enabled but your controller can't output audio.", g_idIconError);
      }
   }

   if ( g_bIsFirstConnectionToCurrentVehicle )
   {
      osd_add_stats_total_flights();
      g_bIsFirstConnectionToCurrentVehicle = false;
   }

   bool bMustPair = false;
   if ( g_pCurrentModel->audio_params.enabled != bOldAudioEnabled )
      bMustPair = true;
   if ( bRadioChanged )
      bMustPair = true;

   if ( bMustPair )
   {
      Popup* p = new Popup("Radio links configuration changed on the vehicle. Updating local radio configuration...", 0.15,0.5, 0.7, 5);
      p->setIconId(g_idIconRadio, get_Color_IconWarning());
      popups_add_topmost(p);

      log_line("[Events] Critical change in radio params on the received model settings. Restart pairing with the vehicle.");
      pairing_stop();
      hardware_sleep_ms(200);
      pairing_start();
      log_line("[Events] Handing of event OnReceivedModelSettings complete.");
      return true;
   }

   log_line("[Events] No critical change in radio params on the received model settings. Just notify components to reload model.");
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED;
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.vehicle_id_dest = PACKET_COMPONENT_COMMANDS;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);
   PH.extra_flags = 0;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);

   log_line("[Events] Handing of event OnReceivedModelSettings complete.");
   return true;
}

