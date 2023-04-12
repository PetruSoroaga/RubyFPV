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

#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pcap.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <semaphore.h>
#include <dirent.h>

#include "../base/base.h"
#include "../base/shared_mem.h"
#include "../base/encr.h"
#include "../base/models.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/utils.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"
#include "launchers_vehicle.h"
#include "radio_utils.h"
#include "../radio/radiolink.h"
#include "shared_vars.h"
#include "timers.h"
#include "utils_vehicle.h"


Model modelVehicle;

shared_mem_process_stats* s_pProcessStatsRouter = NULL;
shared_mem_process_stats* s_pProcessStatsTelemetry = NULL;
shared_mem_process_stats* s_pProcessStatsCommands = NULL;
shared_mem_process_stats* s_pProcessStatsRC = NULL;

u32 s_failCountProcessThreshold = 5;
u32 s_failCountProcessRouter = 0;
u32 s_failCountProcessTelemetry = 0;
u32 s_failCountProcessCommands = 0;
u32 s_failCountProcessRC = 0;

int s_iRadioSilenceFailsafeTimeoutCounts = 0;

void try_open_process_stats()
{
   if ( NULL == s_pProcessStatsRouter )
   {
      s_pProcessStatsRouter = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_ROUTER_TX);
      if ( NULL == s_pProcessStatsRouter )
         log_softerror_and_alarm("Failed to open shared mem to router process watchdog for reading: %s", SHARED_MEM_WATCHDOG_ROUTER_TX);
      else
         log_line("Opened shared mem to router process watchdog for reading.");
   }
   if ( NULL == s_pProcessStatsTelemetry )
   {
      s_pProcessStatsTelemetry = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_TELEMETRY_TX);
      if ( NULL == s_pProcessStatsTelemetry )
         log_softerror_and_alarm("Failed to open shared mem to telemetry Tx process watchdog for reading: %s", SHARED_MEM_WATCHDOG_TELEMETRY_TX);
      else
         log_line("Opened shared mem to telemetry Tx process watchdog for reading.");
   }
   if ( NULL == s_pProcessStatsCommands )
   {
      s_pProcessStatsCommands = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_COMMANDS_RX);
      if ( NULL == s_pProcessStatsCommands )
         log_softerror_and_alarm("Failed to open shared mem to commands Rx process watchdog for reading: %s", SHARED_MEM_WATCHDOG_COMMANDS_RX);
      else
         log_line("Opened shared mem to commands Rx process watchdog for reading.");
   }
   if ( NULL == s_pProcessStatsRC )
   {
      s_pProcessStatsRC = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_RC_RX);
      if ( NULL == s_pProcessStatsRC )
         log_softerror_and_alarm("Failed to open shared mem to RC Rx process watchdog for reading: %s", SHARED_MEM_WATCHDOG_RC_RX);
      else
         log_line("Opened shared mem to RC Rx process watchdog for reading.");
   }
}

// returns true if values changed

bool read_config_file()
{
   int f = config_file_get_value("arm_freq");
   int g = config_file_get_value("gpu_freq");
   int v = config_file_get_value("over_voltage");

   log_line("Read Pi config file: overvoltage: %d, arm freq: %d, gpu freq: %d", v,f,g);
   if ( f != modelVehicle.iFreqARM || g != modelVehicle.iFreqGPU || v != modelVehicle.iOverVoltage )
   {
      modelVehicle.iFreqARM = f;
      modelVehicle.iFreqGPU = g;
      modelVehicle.iOverVoltage = v;
      log_line("Read Pi config file values changed. Will update model file.");
      return true;
   }
   return false;
}


void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
} 
  
int main (int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   log_init("Vehicle");

   bool noWatchDog = false;
   if ( argc > 1 )
   if ( 0 == strcmp(argv[1], "-nowd") )
      noWatchDog = true;

   char szBuff[2048];

   check_licences();

   hardware_enumerate_radio_interfaces();
   int retry = 0;
   while ( 0 == hardware_get_radio_interfaces_count() && retry < 30 )
   {
      hardware_sleep_ms(200);
      hardware_enumerate_radio_interfaces();
      retry++;
   }

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_error_and_alarm("There are no radio interfaces (NICS/wlans) on this device.");
      return -1;
   }
   
   radio_init_link_structures();
   radio_enable_crc_gen(1);

   ruby_clear_all_ipc_channels();

   g_iBoardType = 0;
   char szBoardId[256];
   FILE* fd = fopen("config/board.txt", "r");
   if ( NULL != fd )
   {
      fscanf(fd, "%d %s", &g_iBoardType, szBoardId);
      fclose(fd);
   }
   log_line("Board type: %d -> %s", g_iBoardType, str_get_hardware_board_name(g_iBoardType));


   sprintf(szBuff, "rm -rf %s", FILE_TMP_ALARM_ON);
   hw_execute_bash_command_silent(szBuff, NULL);
 
   bool bMustSave = false;

   if ( ! modelVehicle.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
   {
      modelVehicle.resetToDefaults(true);
      modelVehicle.is_spectator = false;
      bMustSave = true;
   }

   log_line("Loaded model. Developer flags: live log: %s, enable radio silence failsafe: %s, log only errors: %s, radio config guard interval: %d ms",
         (modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)?"yes":"no",
         (modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE)?"yes":"no",
         (modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS)?"yes":"no",
          (int)((modelVehicle.uDeveloperFlags >> 8) & 0xFF) );
   log_line("Model has vehicle developer video link stats flag on: %s", (modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS)?"yes":"no");
   log_line("Model has vehicle developer video link stats graphs on: %s", (modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS)?"yes":"no");

   if ( access( FILE_VEHICLE_REBOOT_CACHE, R_OK ) == -1 )
   {
      log_line("No cached telemetry info, reset current vehicle stats.");
      modelVehicle.m_Stats.uCurrentOnTime = 0;
      modelVehicle.m_Stats.uCurrentFlightTime = 0;
      modelVehicle.m_Stats.uCurrentFlightDistance = 0;
      modelVehicle.m_Stats.uCurrentFlightTotalCurrent = 0;
      modelVehicle.m_Stats.uCurrentTotalCurrent = 0;
      modelVehicle.m_Stats.uCurrentMaxAltitude = 0;
      modelVehicle.m_Stats.uCurrentMaxDistance = 0;
      modelVehicle.m_Stats.uCurrentMaxCurrent = 0;
      modelVehicle.m_Stats.uCurrentMinVoltage = 100000;
      bMustSave = true;
   }

   if ( read_config_file() )
      bMustSave = true;

   radio_info_wifi_t dptr;
   dptr.slot_time = modelVehicle.radioInterfacesParams.slotTime;
   dptr.thresh62 = modelVehicle.radioInterfacesParams.thresh62;
   if ( hardware_get_basic_radio_wifi_info(&dptr) )
   if ( dptr.tx_power != modelVehicle.radioInterfacesParams.txPower ||
        dptr.tx_powerAtheros != modelVehicle.radioInterfacesParams.txPowerAtheros ||
        dptr.tx_powerRTL != modelVehicle.radioInterfacesParams.txPowerRTL ||
        dptr.slot_time != modelVehicle.radioInterfacesParams.slotTime ||
        dptr.thresh62 != modelVehicle.radioInterfacesParams.thresh62 )
   {
      log_line("2.4/5.8 Radio interfaces power parameters have changed (tx power: %d,%d,%d slot time: %d, thresh62: %d). Updating model.", dptr.tx_power, dptr.tx_powerAtheros, dptr.tx_powerRTL, dptr.slot_time, dptr.thresh62);
      modelVehicle.radioInterfacesParams.txPower = dptr.tx_power;
      modelVehicle.radioInterfacesParams.txPowerAtheros = dptr.tx_powerAtheros;
      modelVehicle.radioInterfacesParams.txPowerRTL = dptr.tx_powerRTL;
      modelVehicle.radioInterfacesParams.slotTime = dptr.slot_time;
      modelVehicle.radioInterfacesParams.thresh62 = dptr.thresh62;
      bMustSave = true;
   }

   int board_type = hardware_getBoardType();
   if ( board_type != modelVehicle.board_type )
   {
      modelVehicle.board_type = board_type;
      bMustSave = true;
   }

   u32 alarmsOriginal = modelVehicle.alarms;

   // Check the file system for write access
   
   log_line("Checking the file system for write access...");

   hw_execute_bash_command("rm -rf tmp/testwrite.txt", NULL);
   FILE* fdTemp = fopen("tmp/testwrite.txt", "wb");
   if ( NULL == fdTemp )
      modelVehicle.alarms |= ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR;
   else
   {
      fprintf(fdTemp, "test1234\n");
      fclose(fdTemp);
      fdTemp = fopen("tmp/testwrite.txt", "rb");
      if ( NULL == fdTemp )
         modelVehicle.alarms |= ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR;
      else
      {
         char szTmp[256];
         if ( 1 != fscanf(fdTemp, "%s", szTmp) )
            modelVehicle.alarms |= ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR;
         else if ( 0 != strcmp(szTmp, "test1234") )
            modelVehicle.alarms |= ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR;
         fclose(fdTemp);
         hw_execute_bash_command("rm -rf tmp/testwrite.txt", NULL);
      }
   }

   if ( modelVehicle.alarms & ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR )
      log_line("Checking the file system for write access: Failed.");
   else
      log_line("Checking the file system for write access: Succeeded.");

   // Detect serial UARTs

   hardware_init_serial_ports();

   u32 telemetry_flags = modelVehicle.telemetry_params.flags;
   
   modelVehicle.alarms &= (~ALARM_ID_UNSUPORTED_USB_SERIAL);

   if ( hardware_has_unsupported_serial_ports() )
      modelVehicle.alarms |= ALARM_ID_UNSUPORTED_USB_SERIAL;
   
   log_line("Rechecking model serial ports for changes based on current hardware serial ports...");
   if ( modelVehicle.populateVehicleSerialPorts() )
      bMustSave = true;
   log_line("Done rechecking serial ports for changes.");

   if ( alarmsOriginal != modelVehicle.alarms )
      bMustSave = true;
   if ( telemetry_flags != modelVehicle.telemetry_params.flags )
      bMustSave = true;

   // Detect audio cards

   log_line("Detecting audio devices...");
   bool bHadAudioDevice = modelVehicle.audio_params.has_audio_device;

   hw_execute_bash_command_raw( "arecord -l 2>&1 | grep card | grep USB", szBuff );
   if ( 0 < strlen(szBuff) && NULL != strstr(szBuff, "USB") )
      modelVehicle.audio_params.has_audio_device = true;
   else
   {
      modelVehicle.audio_params.has_audio_device = false;
      modelVehicle.audio_params.enabled = false;
   }

   log_line("Finished detecting audio device. %s", modelVehicle.audio_params.has_audio_device?"Audio capture device found.":"No audio capture devices found.");

   if ( bHadAudioDevice != modelVehicle.audio_params.has_audio_device )
      bMustSave = true;

   // Validate camera/video settings

   if ( modelVehicle.validate_camera_settings() )
      bMustSave = true;

   // Validate encryption settings
   if ( modelVehicle.enc_flags != MODEL_ENC_FLAGS_NONE )
   {
      if ( ! lpp(NULL, 0) )
      {
         modelVehicle.enc_flags = MODEL_ENC_FLAGS_NONE;
         bMustSave = true;
      }
   }

   // Check logger service flag change

   if( access( LOG_USE_PROCESS, R_OK ) != -1 )
   {
      if ( ! (modelVehicle.uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE) )
      {
         modelVehicle.uModelFlags |= MODEL_FLAG_USE_LOGER_SERVICE;
         bMustSave = true;
      }
   }
   else
   {
      if ( modelVehicle.uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE )
      {
         modelVehicle.uModelFlags &= ~MODEL_FLAG_USE_LOGER_SERVICE;
         bMustSave = true;
      }
   }

   modelVehicle.setAWBMode();

   // Check & update radio card models

   log_line("Checking radio interfaces cards models. %d hardware radio cards, %d model radio cards...", hardware_get_radio_interfaces_count(), modelVehicle.radioInterfacesParams.interfaces_count);
   for( int i=0; i<modelVehicle.radioInterfacesParams.interfaces_count; i++ )
      log_line("Vehicle model radio interface %d: MAC: %s, model: [%s]", i+1, modelVehicle.radioInterfacesParams.interface_szMAC[i], str_get_radio_card_model_string(modelVehicle.radioInterfacesParams.interface_card_model[i]));
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
      {
         log_softerror_and_alarm("Failed to get hardware radio card info for card %d.", i+1);
         continue;
      }
      log_line("Hardware radio interface %d: %s, model: [%s]", i+1, pRadioInfo->szMAC, str_get_radio_card_model_string(pRadioInfo->iCardModel));
   }
   
   for( int i=0; i<modelVehicle.radioInterfacesParams.interfaces_count; i++ )
   {
       radio_hw_info_t* pRadioInfo = hardware_get_radio_info_from_mac(modelVehicle.radioInterfacesParams.interface_szMAC[i]);
       if ( NULL == pRadioInfo )
       {
          log_softerror_and_alarm("Failed to get radio card info for card %d with MAC: [%s].", i+1, modelVehicle.radioInterfacesParams.interface_szMAC[i]);
          continue;
       }
       // If user defined, do not change it
       if ( modelVehicle.radioInterfacesParams.interface_card_model[i] < 0 )
       {
          log_line("Radio interface %d model was set by user to [%s]. Do not change it (to [%s]).", i+1, str_get_radio_card_model_string(-modelVehicle.radioInterfacesParams.interface_card_model[i]), str_get_radio_card_model_string(pRadioInfo->iCardModel) );
          continue;
       }
       char szModel[128];
       strcpy(szModel, str_get_radio_card_model_string(pRadioInfo->iCardModel));
       log_line("Hardware radio interface %d type: [%s]. Current model radio interface %d type: [%s].", i+1, szModel, i+1, str_get_radio_card_model_string(modelVehicle.radioInterfacesParams.interface_card_model[i]));
       if ( modelVehicle.radioInterfacesParams.interface_card_model[i] != pRadioInfo->iCardModel )
       {
          log_line("Updating model radio type.");
          modelVehicle.radioInterfacesParams.interface_card_model[i] = pRadioInfo->iCardModel;
          bMustSave = true;
       }
   }

   log_current_full_radio_configuration(&modelVehicle);

   log_line("Setting all the radio cards params (%s)...", (modelVehicle.relay_params.isRelayEnabledOnRadioLinkId > 0)?"relaying enabled":"no relay links");

   
   if ( modelVehicle.relay_params.uCurrentRelayMode != RELAY_MODE_NONE )
   {
      log_line("Relaying was active. Disable it on start up.");
      modelVehicle.relay_params.uCurrentRelayMode = RELAY_MODE_NONE;
      bMustSave = true;
   }

   if ( modelVehicle.relay_params.isRelayEnabledOnRadioLinkId > 0 )
   {
      bool bConflict = false;
      int iConflictLink = -1;
      for( int i=0; i<modelVehicle.radioLinksParams.links_count; i++ )
         if ( modelVehicle.radioLinksParams.link_frequency[i] == modelVehicle.relay_params.uRelayFrequency )
         if ( i != modelVehicle.relay_params.isRelayEnabledOnRadioLinkId-1 )
         {
            iConflictLink = i;
            bConflict = true;
            break;
         }
      if ( bConflict )
      {
         log_softerror_and_alarm("Invalid relaying configuration: relay link frequency %s is the same as frequency for regular radio link %d. Reset and disabling relaying.", str_format_frequency(modelVehicle.relay_params.uRelayFrequency), iConflictLink+1 );
         modelVehicle.relay_params.isRelayEnabledOnRadioLinkId = 0;
         modelVehicle.relay_params.uRelayFrequency = 0;
         modelVehicle.relay_params.uRelayVehicleId = 0;
         bMustSave = true;
      }
   }
   

   if ( configure_radio_interfaces_for_current_model(&modelVehicle) )
      bMustSave = true;
   bMustSave = true;
   log_line("Configured all the radio cards params.");

   log_current_full_radio_configuration(&modelVehicle);

   // Update model settings to disk

   if ( bMustSave )
   {
      log_line("Updating local model file.");
      modelVehicle.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }

   for( int i=0; i<modelVehicle.hardware_info.serial_bus_count; i++ )
   {
      if ( modelVehicle.hardware_info.serial_bus_supported_and_usage[i] & ((1<<5)<<8) )
      if ( (modelVehicle.hardware_info.serial_bus_supported_and_usage[i] & 0xFF) != SERIAL_PORT_USAGE_NONE )
      {
         int iPortId = ( modelVehicle.hardware_info.serial_bus_supported_and_usage[i] >> 8 ) & 0x0F;
         // USB serial
         if ( modelVehicle.hardware_info.serial_bus_supported_and_usage[i] & ((1<<4)<<8) )
         {
            char szPort[32];
            sprintf(szPort, "/dev/ttyUSB%d", iPortId);
            hardware_configure_serial(szPort, (long)modelVehicle.hardware_info.serial_bus_speed[i]);
         }
         // Hardware serial
         else
         {
            char szPort[32];
            sprintf(szPort, "/dev/serial%d", iPortId);
            hardware_configure_serial(szPort, (long)modelVehicle.hardware_info.serial_bus_speed[i]);
         }
      }
   }
   hw_launch_process("./ruby_alive");
   
   hardware_sleep_ms(20);
   vehicle_launch_tx_telemetry(&modelVehicle);
   
   hardware_sleep_ms(20);
   vehicle_launch_rx_commands(&modelVehicle);

   hardware_sleep_ms(20);
   vehicle_launch_rx_rc(&modelVehicle);

   if ( modelVehicle.audio_params.has_audio_device )
      vehicle_launch_audio_capture(&modelVehicle);
   if ( modelVehicle.hasCamera() )
      vehicle_launch_video_capture(&modelVehicle, NULL);

   vehicle_launch_tx_router(&modelVehicle);

   log_line("Done launching router/video pipeline");

   /*
   if ( modelVehicle.hasCamera() )
   if ( ! modelVehicle.isCameraHDMI() )
   {
      for( int i=0; i<7; i++ )
         hardware_sleep_ms(900);
      vehicle_stop_video_capture(&modelVehicle);
      vehicle_launch_video_capture(&modelVehicle, NULL);
   }
   */

   log_line("");
   log_line("----------------------------------------------------------");
   log_line("Finished launching processes. Switching to watchdog state.");
   log_line("----------------------------------------------------------");
   log_line("");

   // Wait for all the processes to start (takes time for I2C detection)
   for( int i=0; i<10; i++ )
      hardware_sleep_ms(800);

   vehicle_check_update_processes_affinities();

   if ( noWatchDog )
      log_line_watchdog("Watchdog is disabled");
   else
      log_line_watchdog("Watchdog state started.");

   if ( modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
      log_only_errors();

   g_TimeStart = get_current_timestamp_ms();

   u32 maxTimeForProcess = 1;
   char szTime[64];
   char szTime2[64];
   char szTime3[64];
   int counter = 0;
   bool bMustRestart = false;

   while ( ! g_bQuit )
   {
      sleep(maxTimeForProcess);
      counter++;
      g_TimeNow = get_current_timestamp_ms();

      if ( access( FILE_TMP_UPDATE_IN_PROGRESS, R_OK ) != -1 )
      {
         while (! g_bQuit )
         {
            hardware_sleep_ms(500);
         }
         if ( g_bQuit )
            break;
      }

      if ( g_TimeNow > g_TimeLastCheckRadioSilenceFailsafe + 10000 )
      if ( g_TimeNow > 10000 )
      {
         g_TimeLastCheckRadioSilenceFailsafe = g_TimeNow;

         log_line("Vehicle is alive.");
         if ( NULL == s_pProcessStatsRouter || NULL == s_pProcessStatsTelemetry || NULL == s_pProcessStatsCommands || NULL == s_pProcessStatsRC )
            try_open_process_stats();
         if ( NULL == s_pProcessStatsRouter )
            continue;
         log_format_time(s_pProcessStatsRouter->lastRadioRxTime, szTime);
         log_line("Last radio RX time: %s", szTime);
         log_format_time(s_pProcessStatsRouter->lastRadioTxTime, szTime);
         log_line("Last radio TX time: %s", szTime);

         bool bCheckRadioFailSafe = false;
         //if ( modelVehicle.bDeveloperMode )
         if ( modelVehicle.uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE )
         if ( access( FILE_TMP_ARMED, R_OK ) != -1 ) 
            bCheckRadioFailSafe = true;

         //if ( access( FILE_TMP_RC_DETECTED, R_OK ) != -1 ) 
         //   bCheckRadioFailSafe = true;

         if ( bCheckRadioFailSafe )
         if ( s_pProcessStatsRouter->lastRadioRxTime > 0 )
         if ( (s_pProcessStatsRouter->lastRadioRxTime < g_TimeNow-10000) ||
              (s_pProcessStatsRouter->lastRadioRxTime < g_TimeNow-10000) )
         {
            char szComm[64];
            sprintf(szComm, "touch %s", FILE_TMP_ALARM_ON);
            hw_execute_bash_command_silent(szComm, NULL);

            log_line("Radio silence failsafe is enabled and radio timeout has triggered. Signal router to reinit radio interfaces.");
            if( access( FILE_TMP_REINIT_RADIO_IN_PROGRESS, R_OK ) == -1 )
            {
               if ( 0 == s_iRadioSilenceFailsafeTimeoutCounts )
               {
                  char szComm[64];
                  sprintf(szComm, "touch %s", FILE_TMP_REINIT_RADIO_REQUEST);
                  hw_execute_bash_command_silent(szComm, NULL);
                  s_iRadioSilenceFailsafeTimeoutCounts++;
               }
               else
               {
                  hw_execute_bash_command("sudo reboot -f", NULL);
                  sleep(10); 
               }
            }
            else
               log_line("Will not signal router, a radio reinit is in progress already.");
         }

         /*
         if( access( FILE_TMP_REINIT_RADIO_IN_PROGRESS, R_OK ) == -1 )
         {
            if ( ! radio_utils_check_radio() )
            {
            log_line("Radio interfaces have broken up. Signal router to reinitalize everything.");
            char szComm[64];
            sprintf(szComm, "touch %s", FILE_TMP_REINIT_RADIO_REQUEST);
            hw_execute_bash_command_silent(szComm, NULL); 
            }
         }
         else
            log_line("Radio reinit is in progress. Skip checking for radio interfaces health.");
         */
      }
      if ( noWatchDog )
         continue;

      modelVehicle.reloadIfChanged(true);

      if ( NULL == s_pProcessStatsRouter || NULL == s_pProcessStatsTelemetry || NULL == s_pProcessStatsCommands || NULL == s_pProcessStatsRC )
         try_open_process_stats();
      
      bMustRestart = false;

      if ( access( FILE_TMP_REINIT_RADIO_IN_PROGRESS, R_OK ) != -1 )
         continue;

      if ( NULL != s_pProcessStatsRouter )
      {
         if ( s_pProcessStatsRouter->lastActiveTime < g_TimeNow - maxTimeForProcess*1000 )
            s_failCountProcessRouter++;
         else
            s_failCountProcessRouter = 0;

         if ( s_failCountProcessRouter >= s_failCountProcessThreshold )
         {
            log_format_time(s_pProcessStatsRouter->lastActiveTime, szTime);
            log_format_time(s_pProcessStatsRouter->lastIPCIncomingTime, szTime2);
            log_format_time(s_pProcessStatsRouter->lastRadioTxTime, szTime3);
            log_line_watchdog("Router pipeline watchdog check failed: router process has stopped !!! Last active time: %s, last IPC incoming time: %s, last radio TX time: %s", szTime, szTime2, szTime3);
            bMustRestart = true;
         }
      }

      if ( NULL != s_pProcessStatsTelemetry )
      {
         if ( s_pProcessStatsTelemetry->lastActiveTime < g_TimeNow - maxTimeForProcess*1000 )
            s_failCountProcessTelemetry++;
         else
            s_failCountProcessTelemetry = 0;
         if ( s_failCountProcessTelemetry >= s_failCountProcessThreshold )
         {
            log_format_time(s_pProcessStatsTelemetry->lastActiveTime, szTime);
            log_format_time(s_pProcessStatsTelemetry->lastIPCOutgoingTime, szTime2);
            log_line_watchdog("Telemetry TX pipeline watchdog check failed: telemetry tx process has stopped !!! Last active time: %s, last IPC outgoing time: %s", szTime, szTime2);
            bMustRestart = true;
         }
      }
      
      if ( NULL != s_pProcessStatsCommands )
      {
         if ( s_pProcessStatsCommands->lastActiveTime < g_TimeNow - 3*maxTimeForProcess*1000 )
            s_failCountProcessCommands++;
         else
            s_failCountProcessCommands = 0;
         if ( s_failCountProcessCommands >= 2*s_failCountProcessThreshold )
         {
            log_format_time(s_pProcessStatsCommands->lastActiveTime, szTime);
            log_format_time(s_pProcessStatsCommands->lastIPCIncomingTime, szTime2);
            log_line_watchdog("Commands RX process watchdog check failed: commands rx process has stopped !!! Last active time: %s", szTime);
            bMustRestart = true;
         }
      }

      if ( NULL != s_pProcessStatsRC )
      if ( modelVehicle.rc_params.rc_enabled )
      {
         if ( s_pProcessStatsRC->lastActiveTime < g_TimeNow - maxTimeForProcess*1000 )
            s_failCountProcessRC++;
         else
            s_failCountProcessRC = 0;

         if ( s_failCountProcessRC >= s_failCountProcessThreshold )
         {
            log_format_time(s_pProcessStatsRC->lastActiveTime, szTime);
            log_format_time(s_pProcessStatsRC->lastIPCIncomingTime, szTime2);
            log_line_watchdog("RC RX process watchdog check failed: RC rx process has stopped !!! Last active time: %s, last IPC incoming time: %s", szTime, szTime2);
            bMustRestart = true;
         }
      }

      if ( bMustRestart )
      {
         log_line("Restarting processes...");

         vehicle_stop_tx_router();
         vehicle_stop_audio_capture(&modelVehicle);

         if ( modelVehicle.hasCamera() )
            vehicle_stop_video_capture(&modelVehicle);
         hw_stop_process("ruby_rx_commands");
         vehicle_stop_tx_telemetry();
         vehicle_stop_rx_rc();

         hardware_sleep_ms(200);

         vehicle_launch_tx_telemetry(&modelVehicle);
         hardware_sleep_ms(20);
         vehicle_launch_rx_commands(&modelVehicle);
         hardware_sleep_ms(20);
         vehicle_launch_rx_rc(&modelVehicle);
         hardware_sleep_ms(20);

         if ( modelVehicle.audio_params.has_audio_device )
            vehicle_launch_audio_capture(&modelVehicle);
         if ( modelVehicle.hasCamera() )
            vehicle_launch_video_capture(&modelVehicle, NULL);
         vehicle_launch_tx_router(&modelVehicle);

         log_line("Restarting processes. Done.");

         vehicle_check_update_processes_affinities();
         s_failCountProcessRouter = 0;
         s_failCountProcessTelemetry = 0;
         s_failCountProcessCommands = 0;
         s_failCountProcessRC = 0;
         bMustRestart = false;
      }
   }

   log_line_watchdog("Received request to quit. Stopping watchdog.");
    
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, s_pProcessStatsRouter);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_TELEMETRY_TX, s_pProcessStatsTelemetry);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_COMMANDS_RX, s_pProcessStatsCommands);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_RC_RX, s_pProcessStatsRC);
     
   return 0;
}
