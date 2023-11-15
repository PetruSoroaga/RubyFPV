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
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <bsd/string.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/shared_mem.h"
#include "../base/models.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../radio/radiolink.h"
#include "../base/controller_utils.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"
#include "hw_config_check.h"

static sem_t* s_pSemaphoreStarted = NULL; 

static int s_iBootCount = 0;

static bool g_bDebug = false;
static bool s_isVehicle = false;
static bool s_bQuit = false;
Model modelVehicle;

int board_type = BOARD_TYPE_NONE;


void power_leds(int onoff)
{
   DIR *d;
   struct dirent *dir;
   char szBuff[512];
   d = opendir("/sys/class/leds/");
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < 3 )
            continue;
         if ( dir->d_name[0] != 'l' )
            continue;
         snprintf(szBuff, sizeof(szBuff), "/sys/class/leds/%s/brightness", dir->d_name);
         FILE* fd = fopen(szBuff, "w");
         if ( NULL != fd )
         {
            fprintf(fd, "%d\n", onoff);
            fclose(fd);
         }
      }
      closedir(d);
   }
}

void initLogFiles()
{
   char szCom[256];

   snprintf(szCom, sizeof(szCom), "mv logs/log_system.txt logs/log_system_%d.txt", s_iBootCount-1 );
   if( access( LOG_FILE_SYSTEM, R_OK ) != -1 )
      hw_execute_bash_command_silent(szCom, NULL);

   snprintf(szCom, sizeof(szCom), "mv logs/log_errors.txt logs/log_errors_%d.txt", s_iBootCount-1 );
   if( access( LOG_FILE_ERRORS, R_OK ) != -1 )
      hw_execute_bash_command_silent(szCom, NULL);

   snprintf(szCom, sizeof(szCom), "mv logs/log_errors_soft.txt logs/log_errors_soft_%d.txt", s_iBootCount-1 );
   if( access( LOG_FILE_ERRORS_SOFT, R_OK ) != -1 )
      hw_execute_bash_command_silent(szCom, NULL);

   snprintf(szCom, sizeof(szCom), "mv logs/log_commands.txt logs/log_commands_%d.txt", s_iBootCount-1 );
   if( access( LOG_FILE_COMMANDS, R_OK ) != -1 )
      hw_execute_bash_command_silent(szCom, NULL);

   snprintf(szCom, sizeof(szCom), "mv logs/log_watchdog.txt logs/log_watchdog_%d.txt", s_iBootCount-1 );
   if( access( LOG_FILE_WATCHDOG, R_OK ) != -1 )
      hw_execute_bash_command_silent(szCom, NULL);

   snprintf(szCom, sizeof(szCom), "mv logs/log_video.txt logs/log_video_%d.txt", s_iBootCount-1 );
   if( access( LOG_FILE_VIDEO, R_OK ) != -1 )
      hw_execute_bash_command_silent(szCom, NULL);

   snprintf(szCom, sizeof(szCom), "mv logs/log_capture_veye.txt logs/log_capture_veye_%d.txt", s_iBootCount-1 );
   if( access( LOG_FILE_CAPTURE_VEYE, R_OK ) != -1 )
      hw_execute_bash_command_silent(szCom, NULL);


   hw_execute_bash_command_silent("touch logs/log_system.txt", NULL);
   hw_execute_bash_command_silent("touch logs/log_errors.txt", NULL);   
   hw_execute_bash_command_silent("touch logs/log_errors_soft.txt", NULL);
   hw_execute_bash_command_silent("touch logs/log_commands.txt", NULL);   
   hw_execute_bash_command_silent("touch logs/log_watchdog.txt", NULL);   
}


void detectSystemType()
{
   hardware_detectSystemType();

   if ( hardware_is_vehicle() )
   {
      log_line("Detected system as vehicle/relay.");
      s_isVehicle = true;
   }
   else
   {
      log_line("Detected system as controller.");
      s_isVehicle = false;
   }
   
   log_line("");
   log_line("===========================================================================");
   if ( s_isVehicle )
      log_line("| System detected as vehicle/relay.");
   else
      log_line("| System detected as controller.");
   log_line("===========================================================================");
   log_line("");

   FILE* fd = fopen("/boot/ruby_systype.txt", "w");
   if ( NULL != fd )
   {
      if ( s_isVehicle )
         fprintf(fd, "VEHICLE");
      else
         fprintf(fd, "STATION");
      fclose(fd);
   }
}


void start_check_processes()
{
   char szFilesMissing[1024];
   szFilesMissing[0] = 0;
   bool failed = false;
   if( access( "ruby_controller", R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ruby_controller", sizeof(szFilesMissing)); }
   if( access( "ruby_rt_station", R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ruby_rt_station", sizeof(szFilesMissing)); }
   if( access( "ruby_rt_vehicle", R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ruby_rt_vehicle", sizeof(szFilesMissing)); }
   if( access( "ruby_rx_telemetry", R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ruby_rx_telemetry", sizeof(szFilesMissing)); }
   if( access( "ruby_tx_telemetry", R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ruby_tx_telemetry", sizeof(szFilesMissing)); }
   if( access( "ruby_rx_commands", R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ruby_rx_commands", sizeof(szFilesMissing)); }
   if( access( "ruby_video_proc", R_OK ) == -1 )
      { failed = true; strlcat(szFilesMissing, " ruby_video_proc", sizeof(szFilesMissing)); }
   //if( access( VIDEO_PLAYER_PIPE, R_OK ) == -1 )
   //   { failed = true; strlcat(szFilesMissing, " ", sizeof(szFilesMissing)); strlcat(szFilesMissing, VIDEO_PLAYER_PIPE, sizeof(szFilesMissing)); }
   //if( access( VIDEO_PLAYER_OFFLINE, R_OK ) == -1 )
   //   { failed = true; strlcat(szFilesMissing, " ", sizeof(szFilesMissing)); strlcat(szFilesMissing, VIDEO_PLAYER_OFFLINE, sizeof(szFilesMissing)); }

   if ( access( "/etc/modprobe.d/ath9k_hw.conf.org", R_OK ) == -1 )
   {
      hw_execute_bash_command("cp -rf /etc/modprobe.d/ath9k_hw.conf /etc/modprobe.d/ath9k_hw.conf.org", NULL);
      if ( access( "/etc/modprobe.d/ath9k_hw.conf.org", R_OK ) == -1 )
         {failed = true; strlcat(szFilesMissing, " Atheros_config", sizeof(szFilesMissing));}
   }

   if ( access( "/etc/modprobe.d/rtl8812au.conf.org", R_OK ) == -1 )
   {
      hw_execute_bash_command("cp -rf /etc/modprobe.d/rtl8812au.conf /etc/modprobe.d/rtl8812au.conf.org", NULL);
      if ( access( "/etc/modprobe.d/rtl8812au.conf.org", R_OK ) == -1 )
         {failed = true; strlcat(szFilesMissing, " RTL_config", sizeof(szFilesMissing));}
   }
   
   if ( access( "/etc/modprobe.d/rtl88XXau.conf.org", R_OK ) == -1 )
   {
      hw_execute_bash_command("cp -rf /etc/modprobe.d/rtl88XXau.conf /etc/modprobe.d/rtl88XXau.conf.org", NULL);
      if ( access( "/etc/modprobe.d/rtl88XXau.conf.org", R_OK ) == -1 )
         {failed = true; strlcat(szFilesMissing, " RTL_XX_config", sizeof(szFilesMissing));}
   }
   
   if ( failed )
      printf("Ruby: Checked files consistency: failed.\n");
   else
      printf("Ruby: Checked files consistency: ok.\n");
}

void _create_default_model()
{
   log_line("Creating a default model.");
   Model m;
   m.resetToDefaults(true);

   if ( s_isVehicle )
   {
      hardware_mount_boot();
      hardware_sleep_ms(50);
      hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

      m.board_type = board_type;
      m.iFreqARM = 900;
      if ( board_type == BOARD_TYPE_PIZERO2 )
         m.iFreqARM = 1000;
      else if ( board_type == BOARD_TYPE_PI3B )
         m.iFreqARM = 1200;
      else if ( board_type == BOARD_TYPE_PI3BPLUS || board_type == BOARD_TYPE_PI4B )
         m.iFreqARM = 1400;
      else if ( board_type != BOARD_TYPE_PIZERO && board_type != BOARD_TYPE_PIZEROW && board_type != BOARD_TYPE_NONE 
               && board_type != BOARD_TYPE_PI2B && board_type != BOARD_TYPE_PI2BV11 && board_type != BOARD_TYPE_PI2BV12 )
         m.iFreqARM = 1200;

      config_file_set_value("config.txt", "arm_freq", m.iFreqARM);
      config_file_set_value("config.txt", "arm_freq_min", m.iFreqARM);
      // Do not change hdmi, we might need it for debug
      config_file_force_value("config.txt", "hdmi_force_hotplug", 0);
      config_file_force_value("config.txt", "ignore_lcd", 1);
      //config_file_force_value("config.txt", "hdmi_safe", 1);
        
      hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);

      bool bHasAtheros = false;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( (pNICInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS )
            bHasAtheros = true;
      }

      m.setDefaultVideoBitrate();
      
      if ( bHasAtheros )
      {
         for( int i=0; i<m.radioLinksParams.links_count; i++ )
         {
            m.radioLinksParams.link_datarates[i][0] = 12;
            m.radioLinksParams.link_datarates[i][1] = 12;
         }
         for( int i=0; i<m.radioInterfacesParams.interfaces_count; i++ )
         {
            m.radioInterfacesParams.interface_datarates[i][0] = 0;
            m.radioInterfacesParams.interface_datarates[i][1] = 0;
         }
         m.video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = 5000000;
         m.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps = 5000000;
         m.video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps = 5000000;
         m.video_link_profiles[VIDEO_PROFILE_PIP].bitrate_fixed_bps = 5000000;
      }

      m.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
      m.resetRadioLinksParams();

         m.radioInterfacesParams.interfaces_count = 1;
         m.radioInterfacesParams.interface_type_and_driver[0] = 0xFF0000;
         m.radioInterfacesParams.interface_supported_bands[0] = RADIO_HW_SUPPORTED_BAND_24;
         strcpy(m.radioInterfacesParams.interface_szMAC[0], "YYYYYY");
         strcpy(m.radioInterfacesParams.interface_szPort[0], "Y");
         m.radioInterfacesParams.interface_current_frequency[0] = DEFAULT_FREQUENCY;
         m.radioInterfacesParams.interface_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
         m.radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;

         m.radioLinksParams.link_radio_flags[0] = DEFAULT_RADIO_FRAMES_FLAGS;
         m.radioLinksParams.link_datarates[0][0] = DEFAULT_RADIO_DATARATE;
         m.radioLinksParams.link_datarates[0][1] = DEFAULT_RADIO_DATARATE;

      m.populateRadioInterfacesInfoFromHardware();

      m.b_mustSyncFromVehicle = true;
      m.is_spectator = false;
      m.saveToFile(FILE_CURRENT_VEHICLE_MODEL, true);
   }
}

void do_first_boot_initialization()
{
   log_line("-------------------------------------------------------");
   log_line("First Boot detected. Doing first boot initialization...");


   if ( s_isVehicle )
   {
      hw_execute_bash_command("sudo sed -i 's/ruby/r-vehicle/g' /etc/hosts", NULL);
      hw_execute_bash_command("sudo sed -i 's/ruby/r-vehicle/g' /etc/hostname", NULL);
   }
   else
   {
      hw_execute_bash_command("sudo sed -i 's/ruby/r-controller/g' /etc/hosts", NULL);
      hw_execute_bash_command("sudo sed -i 's/ruby/r-controller/g' /etc/hostname", NULL);
   }


   char szBuff[128];
   hardware_sleep_ms(200);
   hardware_mount_boot();

      if ( board_type == BOARD_TYPE_PIZERO || board_type == BOARD_TYPE_PIZEROW )
      {
         log_line("Raspberry Pi Zero detected on the first boot ever of the system. Updating settings for Pi Zero.");
         //snprintf(szBuff, sizeof(szBuff), "sed -i 's/over_voltage=[0-9]*/over_voltage=%d/g' /boot/config.txt", 5);
         //execute_bash_command(szBuff);
      }
      if ( board_type == BOARD_TYPE_PIZERO2 )
      {
         log_line("Raspberry Pi Zero 2 detected on the first boot ever of the system. Updating settings for Pi Zero 2.");
         //snprintf(szBuff, sizeof(szBuff), "sed -i 's/over_voltage=[0-9]*/over_voltage=%d/g' /boot/config.txt", 5);
         //execute_bash_command(szBuff);
      }

   _create_default_model();

   if ( s_isVehicle )
   {
      if ( access( "config/reset_info.txt", R_OK ) != -1 )
      {
         log_line("Found info for reset to defaults. Using it.");
         Model m;
         if ( ! m.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         {
            m.resetToDefaults(true);
            m.is_spectator = false;
         }
         FILE* fd = fopen("config/reset_info.txt", "rb");
         if ( NULL != fd )
         {
            fscanf(fd, "%u %u %d %d %d %s",
               &m.vehicle_id, &m.controller_id,
               &m.radioLinksParams.link_frequency[0],
               &m.radioLinksParams.link_frequency[1],
               &m.radioLinksParams.link_frequency[2],
               szBuff);
            fclose(fd);
         
            if ( szBuff[0] == '*' && szBuff[1] == 0 )
               szBuff[0] = 0;

            for( int i=0; i<(int)strlen(szBuff); i++ )
               if ( szBuff[i] == '_' )
                  szBuff[i] = ' ';

            strcpy(m.vehicle_name, szBuff);
         }
         m.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
         
         hw_execute_bash_command("rm -rf /home/pi/ruby/config/reset_info.txt", NULL);
      }
   }
   else
      {
         load_ControllerSettings(); 
         ControllerSettings* pcs = get_ControllerSettings();
         if ( NULL != pcs )
         {
            pcs->iFreqARM = 900;
            if ( board_type == BOARD_TYPE_PIZERO2 )
               pcs->iFreqARM = 1000;
            else if ( board_type == BOARD_TYPE_PI3B )
               pcs->iFreqARM = 1200;
            else if ( board_type == BOARD_TYPE_PI3BPLUS || board_type == BOARD_TYPE_PI4B )
               pcs->iFreqARM = 1400;
            else if ( board_type != BOARD_TYPE_PIZERO && board_type != BOARD_TYPE_PIZEROW && board_type != BOARD_TYPE_NONE 
                  && board_type != BOARD_TYPE_PI2B && board_type != BOARD_TYPE_PI2BV11 && board_type != BOARD_TYPE_PI2BV12 )
               pcs->iFreqARM = 1200;
   
            hardware_mount_boot();
            hardware_sleep_ms(50);
            hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

            config_file_set_value("config.txt", "arm_freq", pcs->iFreqARM);
            config_file_set_value("config.txt", "arm_freq_min", pcs->iFreqARM);

            hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
         }
      }

   snprintf(szBuff, sizeof(szBuff), "rm -rf %s", FILE_FIRST_BOOT);
   hw_execute_bash_command_silent(szBuff, NULL);
   hardware_sleep_ms(50);
   //if ( ! s_isVehicle )
   //   execute_bash_command("raspi-config --expand-rootfs > /dev/null 2>&1", NULL);   

   log_line("First Boot detected. First boot initialization completed.");
   log_line("---------------------------------------------------------");
}


void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   s_bQuit = true;
} 
  
int main (int argc, char *argv[])
{
   signal(SIGPIPE, SIG_IGN);
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   g_bDebug = false;
   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebug = true;
  
   char szComm[1024];
   char *tty_name = ttyname(STDIN_FILENO);
   bool foundGoodConsole = false;
  
   printf("\nRuby: Start on console (%s)\n", tty_name != NULL ? tty_name:"N/A");
   fflush(stdout);
      
   if ( g_bDebug )
      foundGoodConsole = true;
   if ( (NULL != tty_name) && strcmp(tty_name, "/dev/tty1") == 0 )
      foundGoodConsole = true;
   if ( (NULL != tty_name) && strcmp(tty_name, "/dev/pts/0") == 0 )
      foundGoodConsole = true;

   if ( NULL == tty_name || (!foundGoodConsole) )
   {
      printf("\nRuby: Try to execute in wrong console (%s). Exiting.\n", tty_name != NULL ? tty_name:"N/A");
      fflush(stdout);
      return 0;
   }
   
   s_pSemaphoreStarted = sem_open("RUBY_STARTED_SEMAPHORE", O_CREAT | O_EXCL, S_IWUSR | S_IRUSR, 0);
   if ( s_pSemaphoreStarted == SEM_FAILED && (!g_bDebug) )
   {
      printf("\nRuby (v %d.%d) is starting...\n", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10);
      fflush(stdout);
      sleep(8);
      return -1;
   }
  
   printf("\nRuby is starting...\n");
   fflush(stdout);

   //execute_bash_command_silent("con2fbmap 1 0", NULL);
   system("sudo mount -o remount,rw /");
   system("sudo mount -o remount,rw /boot");
   system("cd /boot; sudo mount -o remount,rw /boot; cd /home/pi/ruby");
   hardware_sleep_ms(50);

   s_iBootCount = 0;
   FILE* fd = fopen(FILE_BOOT_COUNT, "r");
   if ( NULL != fd )
   {
      fscanf(fd, "%d", &s_iBootCount);
      fclose(fd);
      fd = NULL;
   }
   s_iBootCount++;

   initLogFiles();
   hw_execute_bash_command_silent("./ruby_timeinit", NULL);

   if( access( LOG_USE_PROCESS, R_OK ) != -1 )
   {
      hw_execute_bash_command("./ruby_logger&", NULL);
      hardware_sleep_ms(300);
   }

   log_init("RubyStart");

   log_line("Found good console, starting Ruby...");

   printf("\nRuby Start (v %d.%d) r%d\n", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, s_iBootCount);
   fflush(stdout);
   bool readWriteOk = false;
   int readWriteRetryCount = 0;
   while ( ! readWriteOk )
   {
      printf("Ruby: Trying to access files...\n");
      readWriteRetryCount++;
      power_leds(readWriteRetryCount%2);

      hardware_sleep_ms(100);
      system("sudo mount -o remount,rw /");
      system("sudo mount -o remount,rw /boot");
      system("cd /boot; sudo mount -o remount,rw /boot; cd /home/pi/ruby");
      hardware_mount_root();
      hardware_mount_boot();
      hw_execute_bash_command_silent("cd /boot; mount -o remount,rw /boot; cd /home/pi/ruby", NULL);
      hardware_sleep_ms(100);
      hw_execute_bash_command_silent("mkdir -p tmp", NULL);
      hw_execute_bash_command_silent("chmod 777 tmp", NULL);
      hw_execute_bash_command_silent("rm -rf tmp/*", NULL);
      hw_execute_bash_command_silent("mkdir -p tmp/ruby", NULL);
      hw_execute_bash_command_silent("chmod 777 tmp/ruby", NULL);
      hw_execute_bash_command_silent("rm -rf tmp/ruby/*", NULL);
      
      snprintf(szComm, sizeof(szComm), "mkdir -p %s", TEMP_VIDEO_MEM_FOLDER);
      hw_execute_bash_command_silent(szComm, NULL);
      snprintf(szComm, sizeof(szComm), "chmod 777 %s", TEMP_VIDEO_MEM_FOLDER);
      hw_execute_bash_command_silent(szComm, NULL);
      snprintf(szComm, sizeof(szComm), "umount %s", TEMP_VIDEO_MEM_FOLDER);
      hw_execute_bash_command_silent(szComm, NULL);

      hw_execute_bash_command_silent("mkdir -p logs", NULL);
      hw_execute_bash_command_silent("mkdir -p config", NULL);
      hw_execute_bash_command_silent("mkdir -p config/models", NULL);
      hw_execute_bash_command_silent("mkdir -p media", NULL);
      hw_execute_bash_command_silent("mkdir -p updates", NULL);

      hw_execute_bash_command_silent("chmod 777 logs/*", NULL);
      hw_execute_bash_command_silent("chmod 777 config/*", NULL);
      hw_execute_bash_command_silent("chmod 777 config/models/*", NULL);
      hw_execute_bash_command_silent("chmod 777 media/*", NULL);
      hw_execute_bash_command_silent("chmod 777 updates/*", NULL);
   
      snprintf(szComm, sizeof(szComm), "mkdir -p %s", FOLDER_OSD_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);
      snprintf(szComm, sizeof(szComm), "chmod 777 %s", FOLDER_OSD_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);


      snprintf(szComm, sizeof(szComm), "mkdir -p %s", FOLDER_CORE_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);
      snprintf(szComm, sizeof(szComm), "chmod 777 %s", FOLDER_CORE_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);

      fd = fopen(LOG_FILE_START, "a+");
      if ( NULL == fd )
         continue;

      fprintf(fd, "Check for write access, succeeded on try number: %d (boot count: %d, Ruby on TTY name: %s)\n", readWriteRetryCount, s_iBootCount, tty_name);
      fclose(fd);
      fd = NULL;

      fd = fopen(FILE_BOOT_COUNT, "w");
      if ( NULL == fd )
         continue;
      fprintf(fd, "%d\n", s_iBootCount);
      fclose(fd);

      readWriteOk = true;
   }

   printf("Ruby: Access to files: Ok.\n");
   fflush(stdout);

   fd = fopen(LOG_FILE_START, "a+");
   if ( NULL != fd )
   {
      fprintf(fd, "Starting run number %d; Starting Ruby on TTY name: %s\n\n", s_iBootCount, tty_name);
      fclose(fd);
      fd = NULL;
   }

   //power_leds(0);

   log_line("Files are ok, checking processes and init log files...");

   start_check_processes();

   log_line("Ruby Start on verison %d.%d", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10);
   fflush(stdout);

   
   char szOutput[4096];
   szOutput[0] = 0;
   hw_execute_bash_command("sudo modprobe i2c-dev", NULL);
   hw_execute_bash_command("sudo modprobe -f 88XXau 2>&1", szOutput);
   if ( 0 != szOutput[0] )
   if ( strlen(szOutput) > 10 )
      log_line("Error on loading driver: [%s]", szOutput);
     
   hardware_sleep_ms(50);

   hw_execute_bash_command("./ruby_initdhcp &", NULL);
   
   hw_execute_bash_command_raw("lsusb", szOutput);
   strlcat(szOutput, "\n*END*\n", sizeof(szOutput));
   log_line("USB Devices:");
   log_line(szOutput);

   hw_execute_bash_command_raw("lsmod", szOutput);
   strlcat(szOutput, "\n*END*\n", sizeof(szOutput));
   log_line("Loaded Modules:");
   log_line(szOutput);      
      
   hw_execute_bash_command_raw("i2cdetect -l", szOutput);
   log_line("I2C buses:");
   log_line(szOutput);      

   int iCount = 0;
   bool bWiFiDetected = false;
   while ( iCount < 30 )
   {
      iCount++;
      szOutput[0] = 0;
      hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
      if ( 0 != strstr(szOutput, "wlan0") )
      {
         log_line("WiFi detected on the first try: [%s]", szOutput);
         bWiFiDetected = true;
         break;
      }
      for( int i=0; i<3; i++ )
      {
         snprintf(szComm, sizeof(szComm), "ifconfig wlan%d down", i );
         hw_execute_bash_command(szComm, NULL);
         hardware_sleep_ms(2*DEFAULT_DELAY_WIFI_CHANGE);
         snprintf(szComm, sizeof(szComm), "ifconfig wlan%d up", i );
         hw_execute_bash_command(szComm, NULL);
         hardware_sleep_ms(2*DEFAULT_DELAY_WIFI_CHANGE);
      }
      
      log_line("Trying detect wifi cards (%d)...", iCount);
      printf("Ruby: Trying to detect 2.4/5.8 Ghz cards...\n");
      fflush(stdout);
      
      hardware_sleep_ms(100);
      for( int i=0; i<3; i++ )
      {
         power_leds(1);
         hardware_sleep_ms(170);
         power_leds(0);
         hardware_sleep_ms(170);
      }
   }
   
   hw_execute_bash_command_raw("ifconfig 2>&1 | grep wlan0", szOutput);
   log_line("Radio interface wlan0 state: [%s]", szOutput);

   ///////////////
   /*
   if ( 0 == iCountHighCapacityInterfaces )
   {
      log_line("Try to find again wifi and high capacity radio interfaces as none where detected...");
      
      int iRetry = 0;
      while ( iRetry < 20 )
      {
         for( int i=0; i<4; i++ )
         {   
            snprintf(szComm, sizeof(szComm), "ifconfig wlan%d down", i );
            hw_execute_bash_command(szComm, NULL);
            hardware_sleep_ms(DEFAULT_DELAY_WIFI_CHANGE);
            snprintf(szComm, sizeof(szComm), "ifconfig wlan%d up", i );
            hw_execute_bash_command(szComm, NULL);
            hardware_sleep_ms(DEFAULT_DELAY_WIFI_CHANGE);
         }     
         hardware_sleep_ms(1000);
         hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
         log_line("Network devices found (try %d): [%s]", iRetry, szOutput);
         if ( 0 != strstr(szOutput, "wlan0") )
         {
            break;
         }
         iRetry++;
      }
      snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_CURRENT_RADIO_HW_CONFIG);
      hw_execute_bash_command(szComm, NULL);
      hardware_reset_radio_enumerated_flag();
      hardware_enumerate_radio_interfaces();
   }
   */
   /////////////////

   if ( ! bWiFiDetected )
      log_softerror_and_alarm("Failed to find any wifi cards.");

   hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
   log_line("Network devices found: [%s]", szOutput);

   if ( access("/sys/class/net/wlan0/device/uevent", R_OK) != -1 )
   {
      hw_execute_bash_command_raw("cat /sys/class/net/wlan0/device/uevent", szOutput);
      log_line("Network wlan0 info: [%s]", szOutput);
   }
   if ( access("/sys/class/net/wlan1/device/uevent", R_OK) != -1 )
   {
      hw_execute_bash_command_raw("cat /sys/class/net/wlan1/device/uevent", szOutput);
      log_line("Network wlan1 info: [%s]", szOutput);
   }
   if ( access("/sys/class/net/wlan2/device/uevent", R_OK) != -1 )
   {
      hw_execute_bash_command_raw("cat /sys/class/net/wlan2/device/uevent", szOutput);
      log_line("Network wlan2 info: [%s]", szOutput);
   }

   snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_SYSTEM_TYPE);
   hw_execute_bash_command_silent(szComm, NULL);

   init_hardware_only_status_led();

   if ( access( FILE_FORCE_RESET, R_OK ) != -1 )
   {
      hw_execute_bash_command("rm -rf config/*", NULL);
      
      snprintf(szComm, sizeof(szComm), "touch %s", FILE_FIRST_BOOT);
      hw_execute_bash_command(szComm, NULL);
   }

   board_type = hardware_detectBoardType();

   // Initialize I2C bus 0 for different boards types
   {
      log_line("Initialize I2C busses...");
      snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./camera_i2c_config 2>/dev/null; cd $current_dir", VEYE_COMMANDS_FOLDER);
      hw_execute_bash_command(szComm, NULL);
      if ( board_type == BOARD_TYPE_PI3APLUS || board_type == BOARD_TYPE_PI3B || board_type == BOARD_TYPE_PI3BPLUS || board_type == BOARD_TYPE_PI4B )
      {
         log_line("Initializing I2C busses for Pi 3/4...");
         hw_execute_bash_command("raspi-gpio set 0 ip", NULL);
         hw_execute_bash_command("raspi-gpio set 1 ip", NULL);
         hw_execute_bash_command("raspi-gpio set 44 ip", NULL);
         hw_execute_bash_command("raspi-gpio set 44 a1", NULL);
         hw_execute_bash_command("raspi-gpio set 45 ip", NULL);
         hw_execute_bash_command("raspi-gpio set 45 a1", NULL);
         hardware_sleep_ms(200);
         hw_execute_bash_command("i2cdetect -y 0 0x0F 0x0F", NULL);
         hardware_sleep_ms(200);
         log_line("Done initializing I2C busses for Pi 3/4.");
      }
   }

   printf("Ruby: Finding external I2C devices add-ons...\n");
   fflush(stdout);
   hardware_enumerate_i2c_busses();
   int iKnown = hardware_get_i2c_found_count_known_devices();
   int iConfigurable = hardware_get_i2c_found_count_configurable_devices();
   if ( 0 == iKnown && 0 == iConfigurable )
      printf("Ruby: Done finding external I2C devices add-ons. None known found.\n" );
   else
      printf("Ruby: Done finding external I2C devices add-ons. Found %d known devices of which %d are configurable.\n", iKnown, iConfigurable );

   fflush(stdout);
   detectSystemType();

   hardware_release();


   log_line("Starting Ruby system...");
   fflush(stdout);
   
   printf("Ruby: Finding serial ports...\n");
   log_line("Ruby: Finding serial ports...");
   fflush(stdout);

   hardware_init_serial_ports();

   printf("Ruby: Enumerating supported 2.4/5.8Ghz radio interfaces...\n");
   log_line("Ruby: Enumerating supported 2.4/5.8Ghz radio interfaces...");
   fflush(stdout);

   snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_CURRENT_RADIO_HW_CONFIG);
   hw_execute_bash_command(szComm, NULL);

   hardware_enumerate_radio_interfaces_step(0);

   //int iCountHighCapacityInterfaces = hardware_get_radio_interfaces_count();

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      printf("Ruby: No 2.4/5.8 Ghz radio interfaces found!\n");
      log_line("Ruby: No 2.4/5.8 Ghz radio interfaces found!");
      fflush(stdout);
   }
   else
   {
      printf("Ruby: %d 2.4/5.8 Ghz radio interfaces found!\n", hardware_get_radio_interfaces_count());
      log_line("Ruby: %d 2.4/5.8 Ghz radio interfaces found!", hardware_get_radio_interfaces_count());
      fflush(stdout);    
   }
   printf("Ruby: Finding SiK radio interfaces...\n");
   log_line("Ruby: Finding SiK radio interfaces...");
   fflush(stdout);

   hardware_enumerate_radio_interfaces_step(1);

   if ( ! hardware_radio_has_sik_radios() )
   {
      printf("Ruby: No SiK radio interfaces found.\n");
      log_line("Ruby: No SiK radio interfaces found.");
      fflush(stdout);
   }
   else
   {
      printf("Ruby: %d SiK radio interfaces found.\n", hardware_radio_has_sik_radios());
      log_line("Ruby: %d SiK radio interfaces found.", hardware_radio_has_sik_radios());
      fflush(stdout);    
   }
   printf("Ruby: Done finding radio interfaces.\n");
   log_line("Ruby: Done finding radio interfaces.");
   fflush(stdout);
   
   // Reenable serial ports that where used for SiK radio and now are just regular serial ports
   
   bool bSerialPortsUpdated = false;
   for( int i=0; i<hardware_get_serial_ports_count(); i++ )
   {
      hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info(i);
      if ( NULL == pSerialPort )
         continue;
      if ( pSerialPort->iPortUsage == SERIAL_PORT_USAGE_HARDWARE_RADIO )
      if ( ! hardware_serial_is_sik_radio(pSerialPort->szPortDeviceName) )
      {
         log_line("Serial port %d [%s] was a SiK radio. Now it's a regular serial port. Setting it as used for nothing.", i+1, pSerialPort->szPortDeviceName);
         pSerialPort->iPortUsage = SERIAL_PORT_USAGE_NONE;
         bSerialPortsUpdated = true;
      }
   }
   if ( bSerialPortsUpdated )
   {
      hardware_serial_save_configuration();
   }


   if ( s_isVehicle )
   {
      int c = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNIC = hardware_get_radio_info(i);
         if ( pNIC->isSupported )
            c++;
      }
      if ( 0 == c || 0 == hardware_get_radio_interfaces_count() )
      {
         printf("Ruby: No supported radio interfaces found. Total radio interfaces found: %d\n", hardware_get_radio_interfaces_count());
         fflush(stdout);

         //hw_execute_bash_command("./ruby_initdhcp -now &", NULL);

         if ( NULL != s_pSemaphoreStarted )
            sem_close(s_pSemaphoreStarted);
         log_line("");
         log_line("------------------------------");
         log_line("Ruby Start Finished.");
         log_line("------------------------------");
         log_line("");
         return 0;
      }
   }

   bool bIsFirstBoot = false;
   if ( access( FILE_FIRST_BOOT, R_OK ) != -1 )
   {
      do_first_boot_initialization();
      bIsFirstBoot = true;
   }

   if ( access( FILE_CURRENT_VEHICLE_MODEL, R_OK) == -1 )
      _create_default_model();
   
   if ( s_isVehicle )
   {
      if ( ! modelVehicle.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
      {
         modelVehicle.resetToDefaults(true);
         modelVehicle.is_spectator = false;
         modelVehicle.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      }
      log_line("[HW Radio Check] Full radio configuration before doing any changes:");
      log_full_current_radio_configuration(&modelVehicle);
      if ( check_update_hardware_nics_vehicle(&modelVehicle) )
      {
         log_line("[HW Radio Check] Hardware radio interfaces configuration check complete and configuration was changed. This is the new hardware radio interfaces and radio links configuration:");
         log_full_current_radio_configuration(&modelVehicle);
         modelVehicle.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      }
      else
         log_line("[HW Radio Check] No radio hardware configuration change detected. No change.");

      if ( check_update_radio_links(&modelVehicle) )
      {
         log_line("[HW Radio Check] Radio links checks was completed and additional changes where made. This is the new hardware radio interfaces and radio links configuration:");
         log_full_current_radio_configuration(&modelVehicle);
         modelVehicle.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      }
      else
         log_line("[HW Radio Check] No additional radio links updates.");

      if( access( "ruby_update_vehicle", R_OK ) != -1 )
      {
         printf("Ruby: Executing post update changes...\n");
         log_line("Executing post update changes...");
         fflush(stdout);
         hw_execute_bash_command("./ruby_update_vehicle", NULL);
         hw_execute_bash_command("rm -f ruby_update_vehicle", NULL);
         printf("Ruby: Executing post update changes. Done.\n");
         log_line("Executing post update changes. Done.");

         if ( ! modelVehicle.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         {
            modelVehicle.resetToDefaults(true);
            modelVehicle.is_spectator = false;
            modelVehicle.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
         }
      }
   }
   else
   {
      if( access( "ruby_update_controller", R_OK ) != -1 )
      {
         printf("Ruby: Executing post update changes...\n");
         log_line("Executing post update changes...");
         fflush(stdout);
         hw_execute_bash_command("./ruby_update_controller", NULL);
         hw_execute_bash_command("rm -f ruby_update_controller", NULL);
         printf("Ruby: Executing post update changes. Done.\n");
         log_line("Executing post update changes. Done.");
      }
   }

   fd = fopen(FILE_CONFIG_CURRENT_VERSION, "w");
   if ( NULL != fd )
   {
      fprintf(fd, "%u\n", (((u32)SYSTEM_SW_VERSION_MAJOR)<<8) | (u32)SYSTEM_SW_VERSION_MINOR | (((u32)SYSTEM_SW_BUILD_NUMBER)<<16));
      fclose(fd);
   }
   else
      log_softerror_and_alarm("Failed to save current version id to file: %s",FILE_CONFIG_CURRENT_VERSION);

   if ( bIsFirstBoot )
   {
      printf("\n\n\n");
      printf("Ruby: First install initialization complete. Rebooting now...\n");
      printf("\n\n\n");
      log_line("Ruby: First install initialization complete. Rebooting now...");
      fflush(stdout);
      hardware_sleep_ms(500);
      hw_execute_bash_command("sudo reboot -f", NULL);
      return 0;
   }
 
   printf("Ruby: Checking for HW changes...");
   log_line("Checking for HW changes...");
   fflush(stdout);

   if ( s_isVehicle )
   {
      if ( ! modelVehicle.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
      {
         modelVehicle.resetToDefaults(true);
         modelVehicle.is_spectator = false;
         modelVehicle.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      }
   }

   hardware_i2c_check_and_update_device_settings();
   printf(" Done.\n");
   log_line("Checking for HW changes complete.");
   fflush(stdout);

   ruby_init_ipc_channels();
   
   if ( s_isVehicle )
   {
      log_line("---------------------------------");
      log_line("|  Vehicle Id: %u", modelVehicle.vehicle_id);
      log_line("---------------------------------");
   }
   printf("Ruby: Starting processes...\n");
   log_line("Starting processes...");
   fflush(stdout);

   //hw_execute_bash_command("./ruby_initdhcp &", NULL);
   printf("Ruby: Init radio interfaces...\n");
   log_line("Init radio interfaces...");
   fflush(stdout);
   hw_execute_bash_command("./ruby_initradio", NULL);
   
   printf("Ruby: Starting main process...\n");
   log_line("Starting main process...");
   fflush(stdout);
   
   check_licences();
   
   hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
   log_line("Network devices found: [%s]", szOutput);

   if ( s_isVehicle )
   {
      printf("Ruby: Starting vehicle...\n");
      log_line("Starting vehicle...");
      fflush(stdout);
      if ( modelVehicle.radioLinksParams.links_count == 1 )
         printf("Ruby: Current frequency: %s\n", str_format_frequency(modelVehicle.radioLinksParams.link_frequency[0]));
      else if ( modelVehicle.radioLinksParams.links_count == 2 )
      {
         char szFreq1[64];
         char szFreq2[64];
         strcpy(szFreq1, str_format_frequency(modelVehicle.radioLinksParams.link_frequency[0]));
         strcpy(szFreq2, str_format_frequency(modelVehicle.radioLinksParams.link_frequency[1]));
         printf("Ruby: Current frequencies: %s/%s\n", szFreq1, szFreq2);
      }
      else
      {
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];
         strcpy(szFreq1, str_format_frequency(modelVehicle.radioLinksParams.link_frequency[0]));
         strcpy(szFreq2, str_format_frequency(modelVehicle.radioLinksParams.link_frequency[1]));
         strcpy(szFreq3, str_format_frequency(modelVehicle.radioLinksParams.link_frequency[2]));
         printf("Ruby: Current frequencies: %s/%s/%s\n", szFreq1, szFreq2, szFreq3);
      }
      printf("Ruby: Detected radio interfaces: %d\n", hardware_get_radio_interfaces_count());
      printf("Ruby: Supported radio interfaces:\n");
      fflush(stdout);
      int c = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNIC = hardware_get_radio_info(i);
         if ( pNIC->isSupported )
         {
            printf("  %d: %s, driver: %s, USB port: %s\n", c+1, pNIC->szDescription, pNIC->szDriver, pNIC->szUSBPort);
            c++;
         }
      }
      printf("Ruby: Total supported radio interfaces: %d\n", c);
      fflush(stdout);
      //hw_launch_process("./ruby_vehicle");
      hw_execute_bash_command("./ruby_vehicle&", NULL);
   }
   else
   {
      printf("Ruby: Starting controller...\n");
      log_line("Starting controller...");
      fflush(stdout);

      u32 uControllerId = controller_utils_getControllerId();
      log_line("Controller UID: %u", uControllerId);

      hw_execute_bash_command_silent("con2fbmap 1 0", NULL);
      //execute_bash_command_silent("printf \"\\033c\"", NULL);
      //hw_launch_process("./ruby_controller");
      hw_execute_bash_command("./ruby_controller&", NULL);
   }
    
   for( int i=0; i<15; i++ )
      hardware_sleep_ms(500);
   
   log_line("Checking processes start...");
   if( access( LOG_USE_PROCESS, R_OK ) != -1 )
   {
      if ( hw_process_exists("ruby_logger") )
         log_line("ruby_logger is started");
      else
         log_error_and_alarm("ruby_logger is not running");
   }

   if ( s_isVehicle )
   {
      if ( NULL != s_pSemaphoreStarted )
         sem_close(s_pSemaphoreStarted);
      log_line("");
      log_line("------------------------------");
      log_line("Ruby Start Finished.");
      log_line("------------------------------");
      log_line("");

      hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
      log_line("Network devices found: [%s]", szOutput);

      hw_execute_bash_command("rm -rf /boot/last_ruby_boot.txt", NULL);
      hw_execute_bash_command("cp -rf logs/log_system.txt /boot/last_ruby_boot.txt", NULL);
      
      log_line("Copy boot log to /boot partition. Done.");

      int iCheckCount = 0;
      while ( true )
      {
         bool bError = false;

         if ( hw_process_exists("ruby_vehicle") )
         {
           if ( iCheckCount == 0 )
              log_line("ruby_vehicle is started");
         }
         else
            { log_error_and_alarm("ruby_vehicle is not running"); bError = true; }

         if ( hw_process_exists("ruby_rt_vehicle") )
         {
           if ( iCheckCount == 0 )
              log_line("ruby_rt_vehicle is started");
         }
         else
            { log_error_and_alarm("ruby_rt_vehicle is not running"); bError = true; }

         if ( hw_process_exists("ruby_tx_telemetry") )
         {
           if ( iCheckCount == 0 )
              log_line("ruby_tx_telemetry is started");
         }
         else
            { log_error_and_alarm("ruby_tx_telemetry is not running"); bError = true; }

         if ( hw_process_exists("ruby_rx_commands") )
         {
           if ( iCheckCount == 0 )
              log_line("ruby_rx_commands is started");
         }
         else
            { log_error_and_alarm("ruby_rx_commands is not running"); bError = true; }

         if ( bError )
            printf("Error: Some processes are not running.\n");
         else
         {
            u32 uTimeNow = get_current_timestamp_ms();
            char szTime[128];
            snprintf(szTime, sizeof(szTime),"%d %d:%02d:%02d", s_iBootCount, (int)(uTimeNow/1000/60/60), (int)(uTimeNow/1000/60)%60, (int)((uTimeNow/1000)%60));
            printf("%s All processes are running fine.\n", szTime);
         }
         fflush(stdout);
         iCheckCount++;
         for( int i=0; i<3; i++ )
            hardware_sleep_ms(5000);
      }
   }
   else
   {
      if ( hw_process_exists("ruby_central") )
         log_line("ruby_central is started");
      else
         log_error_and_alarm("ruby_central is not running");

      if ( hw_process_exists("ruby_controller") )
         log_line("ruby_controller is started");
      else
         log_error_and_alarm("ruby_controller is not running");

      hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
      log_line("Network devices found: [%s]", szOutput);

      hw_execute_bash_command("rm -rf /boot/last_ruby_boot.txt", NULL);
      hw_execute_bash_command("cp -rf logs/log_system.txt /boot/last_ruby_boot.txt", NULL);
      
      log_line("Copy boot log to /boot partition. Done.");
   }

   if ( NULL != s_pSemaphoreStarted )
      sem_close(s_pSemaphoreStarted);
   log_line("");
   log_line("------------------------------");
   log_line("Ruby Start Finished.");
   log_line("------------------------------");
   log_line("");
   return 0;
}
