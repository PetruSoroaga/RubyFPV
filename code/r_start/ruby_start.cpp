/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
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
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/hardware.h"
#include "../base/hardware_camera.h"
#include "../base/hw_procs.h"
#include "../base/hardware_radio_serial.h"
#include "../base/vehicle_settings.h"
#include "../radio/radioflags.h"
#include "../base/ruby_ipc.h"

#ifdef HW_PLATFORM_RASPBERRY
#include "../base/ctrl_settings.h"
#include "../base/controller_utils.h"
#endif

#include "../common/string_utils.h"
#include "r_start_vehicle.h"
#include "r_initradio.h"
#include "../r_vehicle/ruby_rx_commands.h"
#include "../r_vehicle/ruby_rx_rc.h"
#include "first_boot.h"

static sem_t* s_pSemaphoreStarted = NULL; 

static int s_iBootCount = 0;

static bool g_bDebug = false;
static bool s_isVehicle = false;
bool s_bQuit = false;
Model modelVehicle;

u32 board_type = BOARD_TYPE_NONE;


void power_leds(int onoff)
{
   #ifdef HW_PLATFORM_RASPBERRY
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
         sprintf(szBuff, "/sys/class/leds/%s/brightness", dir->d_name);
         FILE* fd = fopen(szBuff, "w");
         if ( NULL != fd )
         {
            fprintf(fd, "%d\n", onoff);
            fclose(fd);
         }
      }
      closedir(d);
   }
   #endif
}

void initLogFiles()
{
   char szComm[256];
   char szSrcFile[MAX_FILE_PATH_SIZE];
   sprintf(szComm, "rm -rf %s%s", FOLDER_LOGS, LOG_FILE_LOGGER);
   hw_execute_bash_command_silent(szComm, NULL);

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_SYSTEM);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_system_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "touch %s", szSrcFile);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_ERRORS);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_errors_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "touch %s", szSrcFile);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_ERRORS_SOFT);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_errors_soft_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "touch %s", szSrcFile);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_COMMANDS);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_commands_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "touch %s", szSrcFile);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_WATCHDOG);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_watchdog_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "touch %s", szSrcFile);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_VIDEO);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_video_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
   }

   strcpy(szSrcFile, FOLDER_LOGS);
   strcat(szSrcFile, LOG_FILE_CAPTURE_VEYE);
   if( access( szSrcFile, R_OK ) != -1 )
   {
      sprintf(szComm, "mv %s %s/log_capture_veye_%d.txt", szSrcFile, FOLDER_LOGS, s_iBootCount-1 );
      hw_execute_bash_command_silent(szComm, NULL);
   }


   if ( s_iBootCount >= 40 )
   {
      for( int i=0; i<=5; i++ )
      {
         sprintf(szComm, "rm -rf %s/logs*%d.txt 2>&1", FOLDER_LOGS, s_iBootCount-35-i);
         hw_execute_bash_command_silent(szComm, NULL);
      }
   }
}


void detectSystemType()
{
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

   #ifdef HW_PLATFORM_RASPBERRY
   FILE* fd = fopen("/boot/ruby_systype.txt", "w");
   if ( NULL != fd )
   {
      if ( s_isVehicle )
         fprintf(fd, "VEHICLE");
      else
         fprintf(fd, "STATION");
      fclose(fd);
   }
   #endif
}


void _check_files()
{
   char szFilesMissing[1024];
   szFilesMissing[0] = 0;
   bool failed = false;
   #ifdef HW_PLATFORM_RASPBERRY
   if( access( "ruby_controller", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_controller"); }
   if( access( "ruby_rt_station", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_rt_station"); }
   if( access( "ruby_rx_telemetry", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_rx_telemetry"); }
   if( access( "ruby_video_proc", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_video_proc"); }
   #endif
   if( access( "ruby_rt_vehicle", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_rt_vehicle"); }
   if( access( "ruby_tx_telemetry", R_OK ) == -1 )
      { failed = true; strcat(szFilesMissing, " ruby_tx_telemetry"); }
   //if( access( VIDEO_PLAYER_PIPE, R_OK ) == -1 )
   //   { failed = true; strcat(szFilesMissing, " "); strcat(szFilesMissing, VIDEO_PLAYER_PIPE); }
   //if( access( VIDEO_PLAYER_OFFLINE, R_OK ) == -1 )
   //   { failed = true; strcat(szFilesMissing, " "); strcat(szFilesMissing, VIDEO_PLAYER_OFFLINE); }

   #ifdef HW_PLATFORM_RASPBERRY
   if ( access( "/etc/modprobe.d/ath9k_hw.conf.org", R_OK ) == -1 )
   {
      hw_execute_bash_command("cp -rf /etc/modprobe.d/ath9k_hw.conf /etc/modprobe.d/ath9k_hw.conf.org", NULL);
      if ( access( "/etc/modprobe.d/ath9k_hw.conf.org", R_OK ) == -1 )
         {failed = true; strcat(szFilesMissing, " Atheros_config");}
   }

   if ( access( "/etc/modprobe.d/rtl8812au.conf.org", R_OK ) == -1 )
   {
      hw_execute_bash_command("cp -rf /etc/modprobe.d/rtl8812au.conf /etc/modprobe.d/rtl8812au.conf.org", NULL);
      if ( access( "/etc/modprobe.d/rtl8812au.conf.org", R_OK ) == -1 )
         {failed = true; strcat(szFilesMissing, " RTL_config");}
   }
   
   if ( access( "/etc/modprobe.d/rtl88XXau.conf.org", R_OK ) == -1 )
   {
      hw_execute_bash_command("cp -rf /etc/modprobe.d/rtl88XXau.conf /etc/modprobe.d/rtl88XXau.conf.org", NULL);
      if ( access( "/etc/modprobe.d/rtl88XXau.conf.org", R_OK ) == -1 )
         {failed = true; strcat(szFilesMissing, " RTL_XX_config");}
   }
   #endif

   if ( failed )
      printf("Ruby: Checked files consistency: failed.\n");
   else
      printf("Ruby: Checked files consistency: ok.\n");
}


bool _check_for_update_from_boot()
{
   #ifdef HW_PLATFORM_RASPBERRY
   char szComm[2048];
   char szFoundFile[1024];
   char szZipFile[1024];
   sprintf(szComm, "find /boot/ruby_update*.zip 2>/dev/null");

   hw_execute_bash_command(szComm, szFoundFile);

   if ( (strlen(szFoundFile) == 0) || (NULL == strstr(szFoundFile, "ruby_update")) )
   {
      log_line("No update archive found on /boot folder. Skipping update from /boot");
      return false;
   }
   szFoundFile[127] = 0;
   strcpy(szZipFile, szFoundFile);
   log_line("Found zip archive [%s] on /boot folder.", szZipFile);

   if ( hardware_is_vehicle() )
   {
      sprintf(szComm, "cp -rf %s .", szZipFile);
      hw_execute_bash_command(szComm, NULL);
   }
   else
   {
      sprintf(szComm, "mkdir -p %s", FOLDER_USB_MOUNT);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "cp -rf %s %s/", szZipFile, FOLDER_USB_MOUNT);
      hw_execute_bash_command(szComm, NULL);
   }

   for( int i=0; i<20; i++ )
   {
      hardware_sleep_ms(100);
      power_leds(i%2);
   }
   
   hw_execute_ruby_process_wait(NULL, "ruby_update_worker", NULL, NULL, 0);
   
   hw_execute_bash_command("rm -rf /boot/ruby_update*.zip", NULL);
   hw_execute_bash_command("rm -rf ruby_update*.zip", NULL);
   sprintf(szComm, "rm -rf %s/ruby_update*.zip", FOLDER_USB_MOUNT);
   hw_execute_bash_command(szComm, NULL);


   if ( hardware_is_vehicle() )
      hw_execute_bash_command("cp -rf ruby_update ruby_update_vehicle", NULL);
   else
      hw_execute_bash_command("cp -rf ruby_update ruby_update_controller", NULL);

   for( int i=0; i<30; i++ )
   {
      hardware_sleep_ms(100);
      power_leds(i%2);
   }

   log_line("Done executing update from /boot folder. Rebooting now.");
   fflush(stdout);
   hw_execute_bash_command("reboot -f", NULL);
   return true;
   #else
   return false;
   #endif
}

bool _init_timestamp_and_boot_count()
{
   bool bFirstBoot = false;
   s_iBootCount = 0;

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_BOOT_COUNT);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%d", &s_iBootCount) )
      {
         s_iBootCount = 0;
         bFirstBoot = true;
      }
      fclose(fd);
   }
   else
      bFirstBoot = true;

   static long long lStartTimeStamp_ms;
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   lStartTimeStamp_ms = t.tv_sec*1000LL + t.tv_nsec/1000LL/1000LL;
   
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_BOOT_TIMESTAMP);

   int count = 0;
   while ( count < 10 )
   {
      fd = fopen(szFile, "w");
      if ( NULL != fd )
      {
         fprintf(fd, "%lld\n", lStartTimeStamp_ms);
         fclose(fd);
         break;
      }
      #ifdef HW_PLATFORM_RASPBERRY
      system("sudo mount -o remount,rw /");
      hardware_sleep_ms(50);
      #endif
      count++;
   }
   s_iBootCount++;
   return bFirstBoot;
}


void _test()
{
   log_init("Test");
   log_enable_stdout();
   log_line("\nStarted.\n");


   u32 uTime1 = get_current_timestamp_ms();
   u32 uTime2 = 0;
   for( int i=0; i<50; i++ )
   {
      hardware_sleep_ms(1);
      uTime2 = get_current_timestamp_ms();
      log_line("Sleep 1ms, diff: %u - %u = %u", uTime2, uTime1, uTime2-uTime1);
      uTime1 = uTime2;
   }

   uTime1 = get_current_timestamp_ms();
   int tmp = 0;
   float f = 0;
   float b = 0;

   for( int i=0; i<40; i++ )
   {
      for( int k=0; k<i*20000; k++ )
      {
         tmp += i;
         f += k;
         b = f-tmp*3;
         b = b-1;
      }
      uTime2 = get_current_timestamp_ms();
      log_line("Work %d, diff: %u - %u = %u", i, uTime2, uTime1, uTime2-uTime1);
      uTime1 = uTime2;
   }

   int socket_server;
   struct sockaddr_in server_addr, client_addr;
 
   socket_server = socket(AF_INET , SOCK_DGRAM, 0);
   if (socket_server == -1)
   {
      return;
   }

   memset(&server_addr, 0, sizeof(server_addr));
   memset(&client_addr, 0, sizeof(client_addr));
    
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = INADDR_ANY;
   server_addr.sin_port = htons( 5555 );
 
   client_addr.sin_family = AF_INET;
   client_addr.sin_addr.s_addr = INADDR_ANY;
   client_addr.sin_port = htons( 5555 );

   if( bind(socket_server,(struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
   {
      return;
   }

   uTime1 = get_current_timestamp_ms();
   int iLoop = 0;
   while ( ! s_bQuit )
   {
       iLoop++;
       if ( iLoop > 20 )
          break;

      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(socket_server, &readset);

      struct timeval timePipeInput;
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = iLoop*500; // x miliseconds timeout

      int selectResult = select(socket_server+1, &readset, NULL, NULL, &timePipeInput);
      
      uTime2 = get_current_timestamp_ms();
      log_line("Select %d, %d ms, result: %d, diff: %u - %u = %u", iLoop, iLoop/2, selectResult, uTime2, uTime1, uTime2-uTime1);
      uTime1 = uTime2;
   }

   uTime1 = get_current_timestamp_ms();
   iLoop = 0;
   while ( ! s_bQuit )
   {
       iLoop++;
       if ( iLoop > 20 )
          break;

      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(socket_server, &readset);

      struct timeval timePipeInput;
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 0;

      int selectResult = select(socket_server+1, &readset, NULL, NULL, &timePipeInput);
      
      uTime2 = get_current_timestamp_ms();
      log_line("Select no wait, %d result: %d, diff: %u - %u = %u", iLoop, selectResult, uTime2, uTime1, uTime2-uTime1);
      uTime1 = uTime2;
   }
   close(socket_server);
}

void _test_log(int argc, char *argv[])
{
   log_enable_stdout();
   log_init_local_only("RubyStart");
   log_arguments(argc, argv);

   int iKeyId = LOGGER_MESSAGE_QUEUE_ID;
   if ( argc >= 2 )
   if ( strcmp(argv[argc-2], "-testlog") == 0 )
   {
      iKeyId = atoi(argv[argc-1]);
   }

   log_line_forced_to_file("Test: Using initial message queue key id: %d", iKeyId);
   
   /*
   hw_stop_process("ruby_logger");

   if ( ! hw_process_exists("ruby_logger") )
   {
      if ( iKeyId == LOGGER_MESSAGE_QUEUE_ID )
         hw_execute_ruby_process(NULL, "ruby_logger", NULL, NULL);
      else
      {
         char szParams[128];
         sprintf(szParams, "-id %d", iKeyId);
         hw_execute_ruby_process(NULL, "ruby_logger", szParams, NULL);
      }
      hardware_sleep_ms(300);
   }
   */
   key_t logServiceKey = 0;
   int logServiceMessageQueue = -1;

   logServiceKey = generate_msgqueue_key(iKeyId);
   log_line_forced_to_file("Test: Generated logger msgqueue key: 0x%X", logServiceKey);
   
   logServiceMessageQueue = msgget(logServiceKey, S_IWUSR | S_IWGRP | S_IWOTH);
   if ( logServiceMessageQueue == -1 )
      log_line_forced_to_file("Test: Failed to get logger message queue id, error: %d, [%s]", errno,  strerror(errno));
   else
      log_line_forced_to_file("Test: Got logger message queue id: %d", logServiceMessageQueue);

   type_log_message_buffer msg;
   msg.type = 1;
   msg.text[0] = 0;

   strcpy(msg.text, "Test: Manual log entry sent directly to logger service, waiting for it.");

   if ( msgsnd(logServiceMessageQueue, &msg, strlen(msg.text)+1, 0) < 0 )
      log_line_forced_to_file("Test: Failed to send manual log entry (waiting for it).");
   else
      log_line_forced_to_file("Test: Sent successfully manual log entry (waiting for it)");


   strcpy(msg.text, "Test: Manual log entry sent directly to logger service, no wait.");

   if ( msgsnd(logServiceMessageQueue, &msg, strlen(msg.text)+1, IPC_NOWAIT) < 0 )
      log_line_forced_to_file("Test: Failed to send manual log entry (no wait).");
   else
      log_line_forced_to_file("Test: Sent successfully manual log entry (no wait)");


   log_line("Test log line automated");

   //if ( bStartedLog )
   //   hw_stop_process("ruby_logger");
}


void _log_platform(bool bNewLine)
{
   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   printf("Built for OpenIPC camera.");
   #elif defined(HW_PLATFORM_LINUX_GENERIC)
   printf("Built for Linux");
   #elif defined(HW_PLATFORM_RASPBERRY)
   printf("Built for Raspberry");
   #else
   printf("Built for N/A");
   #endif
   if ( bNewLine )
      printf("\n");
}

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   s_bQuit = true;
} 
  
int main(int argc, char *argv[])
{
   signal(SIGPIPE, SIG_IGN);
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   if ( strcmp(argv[argc-1], "-noop") == 0 )
   {
      _log_platform(true);
      return 0;
   }

   if ( strcmp(argv[argc-1], "-vehicle") == 0 )
   {
      r_start_vehicle(argc, argv);
      return 0;
   }

   if ( strcmp(argv[argc-1], "-rx_commands") == 0 )
   {
      return r_start_commands_rx(argc, argv);
   }

   if ( strcmp(argv[argc-1], "-rc") == 0 )
   {
      return r_start_rx_rc(argc, argv);
   }

   if ( (strcmp(argv[argc-1], "-initradio") == 0) || 
        (strcmp(argv[0], "-initradio") == 0) ||
        ((argc>1) && (strcmp(argv[1], "-initradio") == 0)) )
   {
      r_initradio(argc, argv);
      return 0;
   }

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d) ", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      _log_platform(false);
      return 0;
   }

   if ( strcmp(argv[argc-1], "-test") == 0 )
   {
      _test();
      return 0;
   }

   if ( strcmp(argv[argc-1], "-testlog") == 0 )
   {
      _test_log(argc, argv);
      return 0;
   }

   if ( argc >= 2 )
   if ( strcmp(argv[argc-2], "-testlog") == 0 )
   {
      _test_log(argc, argv);
      return 0;
   }

   g_bDebug = false;
   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebug = true;
  
   char szBuff[256];
   char szComm[1024];
   char szFile[256];
   char *tty_name = ttyname(STDIN_FILENO);
   bool foundGoodConsole = false;
  
   sprintf(szComm, "echo 'Ruby console to start in: [%s]' >> /tmp/ruby_boot.log", ((tty_name != NULL)?tty_name:"N/A"));
   hw_execute_bash_command_silent(szComm, NULL);
   printf("\nRuby: Start on console (%s)\n", ((tty_name != NULL)? tty_name:"N/A"));
   fflush(stdout);
      
   if ( g_bDebug )
      foundGoodConsole = true;
   if ( (NULL != tty_name) && strcmp(tty_name, "/dev/tty1") == 0 )
      foundGoodConsole = true;
   if ( (NULL != tty_name) && strcmp(tty_name, "/dev/pts/0") == 0 )
      foundGoodConsole = true;

   #ifdef HW_PLATFORM_RASPBERRY   
   sprintf(szComm, "echo 'Ruby execute for Raspberry platform' >> /tmp/ruby_boot.log");
   hw_execute_bash_command_silent(szComm, NULL);
   #endif

   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   sprintf(szComm, "echo 'Ruby execute for OpenIPC platform' >> /tmp/ruby_boot.log");
   hw_execute_bash_command_silent(szComm, NULL);
   foundGoodConsole = true;
   char szConsoleName[12];
   strcpy(szConsoleName, "OpenIPCCon");
   tty_name = szConsoleName;
   #endif

   if ( (NULL == tty_name) || (!foundGoodConsole) )
   {
      sprintf(szComm, "echo 'Ruby execute in wrong console. Abort.' >> /tmp/ruby_boot.log");
      hw_execute_bash_command_silent(szComm, NULL);
      printf("\nRuby: Try to execute in wrong console (%s). Exiting.\n", tty_name != NULL ? tty_name:"N/A");
      fflush(stdout);
      return 0;
   }
   
   sprintf(szComm, "echo 'Ruby check semaphore...' >> /tmp/ruby_boot.log");
   hw_execute_bash_command_silent(szComm, NULL);
   s_pSemaphoreStarted = sem_open("/RUBY_STARTED_SEMAPHORE", O_CREAT | O_EXCL, S_IWUSR | S_IRUSR, 0);
   if ( s_pSemaphoreStarted == SEM_FAILED && (!g_bDebug) )
   {
      printf("\nRuby (v %d.%d b.%d) is starting...\n", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      fflush(stdout);
      sleep(8);
      return -1;
   }
   sprintf(szComm, "echo 'Ruby processes are starting...' >> /tmp/ruby_boot.log");
   hw_execute_bash_command_silent(szComm, NULL);

   printf("\nRuby is starting...\n");
   fflush(stdout);

   #ifdef HW_PLATFORM_RASPBERRY
   //execute_bash_command_silent("con2fbmap 1 0", NULL);
   system("sudo mount -o remount,rw /");
   system("sudo mount -o remount,rw /boot");
   system("cd /boot; sudo mount -o remount,rw /boot; cd /home/pi/ruby");
   hardware_sleep_ms(50);
   #endif

   sprintf(szComm, "mkdir -p %s", FOLDER_CONFIG);
   hw_execute_bash_command_silent(szComm, NULL);

   bool bIsFirstBoot = false;
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_FIRST_BOOT);
   if ( access( szFile, R_OK ) != -1 )
      bIsFirstBoot = true;

   if ( _init_timestamp_and_boot_count() )
      bIsFirstBoot = true;

   initLogFiles();

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, LOG_USE_PROCESS);

   if( access( szFile, R_OK ) != -1 )
   if ( ! hw_process_exists("ruby_logger") )
   {
      hw_execute_ruby_process(NULL, "ruby_logger", NULL, NULL);
      hardware_sleep_ms(300);
   }

   log_init("RubyStart");
   log_arguments(argc, argv);

   log_line("Found good console, starting Ruby...");

   printf("\nRuby Start (v %d.%d b.%d) r%d\n", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER, s_iBootCount);
   fflush(stdout);
   bool readWriteOk = false;
   int readWriteRetryCount = 0;
   FILE* fd = NULL;
   
   while ( ! readWriteOk )
   {
      printf("Ruby: Trying to access files...\n");
      readWriteRetryCount++;
      power_leds(readWriteRetryCount%2);

      hardware_sleep_ms(100);

      #ifdef HW_PLATFORM_RASPBERRY
      system("sudo mount -o remount,rw /");
      system("sudo mount -o remount,rw /boot");
      system("cd /boot; sudo mount -o remount,rw /boot; cd /home/pi/ruby");
      hardware_mount_root();
      hardware_mount_boot();
      hw_execute_bash_command_silent("cd /boot; mount -o remount,rw /boot; cd /home/pi/ruby", NULL);
      hardware_sleep_ms(100);
      #endif

      // For temporary fifo-s, same tmp root folder on all platforms
      hw_execute_bash_command_silent("mkdir -p /tmp/ruby/", NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod -p %s", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "rm -rf %s/*", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      
      sprintf(szComm, "mkdir -p %s/ruby", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod -p %s/ruby", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "rm -rf %s/ruby/*", FOLDER_RUBY_TEMP);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s", FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "umount %s", FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_LOGS);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "mkdir -p %s", FOLDER_CONFIG);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "mkdir -p %s", FOLDER_CONFIG_MODELS);
      hw_execute_bash_command_silent(szComm, NULL);
      hw_execute_bash_command_silent("mkdir -p media", NULL);
      hw_execute_bash_command_silent("mkdir -p updates", NULL);

      sprintf(szComm, "chmod 777 %s/*", FOLDER_LOGS);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "chmod 777 %s/*", FOLDER_CONFIG);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "chmod 777 %s/*", FOLDER_CONFIG_MODELS);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "chmod 777 %s/*", FOLDER_MEDIA);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "chmod 777 %s/*", FOLDER_UPDATES);
      hw_execute_bash_command_silent(szComm, NULL);

      sprintf(szComm, "mkdir -p %s", FOLDER_OSD_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s", FOLDER_OSD_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);


      sprintf(szComm, "mkdir -p %s", FOLDER_CORE_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);
      sprintf(szComm, "chmod 777 %s", FOLDER_CORE_PLUGINS);
      hw_execute_bash_command_silent(szComm, NULL);

      strcpy(szFile, FOLDER_LOGS);
      strcat(szFile, LOG_FILE_START);
      fd = fopen(szFile, "a+");
      if ( NULL == fd )
         continue;

      fprintf(fd, "Check for write access, succeeded on try number: %d (boot count: %d, Ruby on TTY name: %s)\n", readWriteRetryCount, s_iBootCount, ((tty_name != NULL)?tty_name:"N/A"));

      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_BOOT_COUNT);
      FILE* fd2 = fopen(szFile, "wb");
      if ( NULL == fd2 )
      {
         fprintf(fd, "Failed to open boot count config file for write [%s].", szFile);
         fclose(fd);
         fd = NULL;
         continue;
      }
      fprintf(fd2, "%d\n", s_iBootCount);
      fclose(fd2);
      fd2 = NULL;

      fclose(fd);
      fd = NULL;
      readWriteOk = true;
   }

   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_START);
   fd = fopen(szFile, "a+");
   if ( NULL != fd )
   {
      fprintf(fd, "Access to files ok. Boot count now: %d\n", s_iBootCount);
      fclose(fd);
      fd = NULL;
   }

   printf("Ruby: Access to files: Ok.\n");
   fflush(stdout);

   strcpy(szFile, FOLDER_LOGS);
   strcat(szFile, LOG_FILE_START);
   fd = fopen(szFile, "a+");
   if ( NULL != fd )
   {
      fprintf(fd, "Starting run number %d; Starting Ruby on TTY name: %s\n\n", s_iBootCount, ((tty_name != NULL)?tty_name:"N/A"));
      fclose(fd);
      fd = NULL;
   }

   //power_leds(0);

   log_line("Files are ok, checking processes and init log files...");

   #ifdef HW_PLATFORM_RASPBERRY
   _check_files();
   #endif

   log_line("Ruby Start on verison %d.%d (b %d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
   printf("Ruby Start on verison %d.%d (b %d)\n", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
   fflush(stdout);
   u32 uBaseVersion = hardware_get_base_ruby_version();
   log_line("Ruby base version is %d.%d", (uBaseVersion >> 8) & 0xFF, uBaseVersion & 0xFF);
   printf("Ruby base version is %d.%d\n", (uBaseVersion >> 8) & 0xFF, uBaseVersion & 0xFF);
   fflush(stdout);
   char szOutput[4096];
   szOutput[0] = 0;

   #ifdef HW_CAPABILITY_I2C
   hw_execute_bash_command("modprobe i2c-dev", NULL);
   #endif

   hardware_radio_load_radio_modules();
     
   hardware_sleep_ms(50);

   hw_execute_bash_command_raw("lsusb", szOutput);
   strcat(szOutput, "\n*END*\n");
   log_line("USB Devices:");
   log_line(szOutput);

   hw_execute_bash_command_raw("lsmod", szOutput);
   strcat(szOutput, "\n*END*\n");
   log_line("Loaded Modules:");
   log_line(szOutput);      
      
   #ifdef HW_CAPABILITY_I2C
   hw_execute_bash_command_raw("i2cdetect -l", szOutput);
   log_line("I2C buses:");
   log_line(szOutput);      
   #endif


   int iCount = 0;
   bool bWiFiDetected = false;
   int iMaxWifiCardsToDetect = 4;
   int iWifiIndexToTry = 0;

   while ( iCount < 10 )
   {
      iCount++;
      szOutput[0] = 0;
      hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);

      for( int i=0; i<iMaxWifiCardsToDetect; i++ )
      {
         sprintf(szBuff, "wlan%d", i);
         if ( i == iWifiIndexToTry )
         if ( NULL != strstr(szOutput, szBuff) )
         {
            log_line("WiFi detected on the first try: [%s]", szOutput);
            bWiFiDetected = true;
            break;
         }
      }

      if ( bWiFiDetected )
         break;

      for( int i=0; i<iMaxWifiCardsToDetect; i++ )
      {
         sprintf(szComm, "ifconfig wlan%d down", i );
         hw_execute_bash_command(szComm, NULL);
         hardware_sleep_ms(2*DEFAULT_DELAY_WIFI_CHANGE);
         sprintf(szComm, "ifconfig wlan%d up", i );
         hw_execute_bash_command(szComm, NULL);
         hardware_sleep_ms(2*DEFAULT_DELAY_WIFI_CHANGE);
      }
      
      log_line("Trying detect wifi cards (%d)...", iCount);
      printf("Ruby: Trying to detect 2.4/5.8 Ghz cards...\n");
      fflush(stdout);
      
      hardware_sleep_ms(500);
      for( int i=0; i<5; i++ )
      {
         power_leds(1);
         hardware_sleep_ms(170);
         power_leds(0);
         hardware_sleep_ms(170);
      }

      iWifiIndexToTry++;
      if ( iWifiIndexToTry >= iMaxWifiCardsToDetect )
         iWifiIndexToTry = 0;
   }
   
   sprintf(szComm, "ifconfig -a 2>&1 | grep wlan%d", iWifiIndexToTry);
   hw_execute_bash_command_raw(szComm, szOutput);
   log_line("Radio interface wlan%d state: [%s]", iWifiIndexToTry, szOutput);

   if ( ! bWiFiDetected )
      log_softerror_and_alarm("Failed to find any wifi cards.");

   hw_execute_bash_command_raw("ls /sys/class/net/", szOutput);
   log_line("Network devices found: [%s]", szOutput);

   for( int i=0; i<iMaxWifiCardsToDetect; i++ )
   {
      sprintf(szBuff, "/sys/class/net/wlan%d/device/uevent", i);
      if ( access(szBuff, R_OK) != -1 )
      {
         sprintf(szComm, "cat /sys/class/net/wlan%d/device/uevent", i);
         hw_execute_bash_command_raw(szComm, szOutput);
         log_line("Network wlan0 info: [%s]", szOutput);
      }
   }

   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_CONFIG_SYSTEM_TYPE);
   hw_execute_bash_command_silent(szComm, NULL);
   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_CONFIG_CAMERA_TYPE);
   hw_execute_bash_command_silent(szComm, NULL);

   init_hardware_only_status_led();

   if ( access( FILE_FORCE_RESET, R_OK ) != -1 )
   {
      unlink(FILE_FORCE_RESET);
      hw_execute_bash_command("rm -rf config/*", NULL);
      sprintf(szComm, "touch %s%s", FOLDER_CONFIG, FILE_CONFIG_FIRST_BOOT);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_bash_command("reboot -f", NULL);
      hardware_sleep_ms(900);
   }

   board_type = (hardware_getOnlyBoardType() & BOARD_TYPE_MASK);

   #ifdef HW_CAPABILITY_I2C
   // Initialize I2C bus 0 for different boards types
   {
      log_line("Initialize I2C busses...");
      sprintf(szComm, "current_dir=$PWD; cd %s/; ./camera_i2c_config 2>/dev/null; cd $current_dir", VEYE_COMMANDS_FOLDER);
      hw_execute_bash_command(szComm, NULL);
      if ( (board_type == BOARD_TYPE_PI3APLUS) || (board_type == BOARD_TYPE_PI3B) || (board_type == BOARD_TYPE_PI3BPLUS) || (board_type == BOARD_TYPE_PI4B) )
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

   log_line("Ruby: Finding external I2C devices add-ons...");
   printf("Ruby: Finding external I2C devices add-ons...\n");
   fflush(stdout);
   hardware_i2c_reset_enumerated_flag();
   hardware_enumerate_i2c_busses();
   int iKnown = hardware_get_i2c_found_count_known_devices();
   int iConfigurable = hardware_get_i2c_found_count_configurable_devices();
   if ( 0 == iKnown && 0 == iConfigurable )
   {
      log_line("Ruby: Done finding external I2C devices add-ons. None known found." );
      printf("Ruby: Done finding external I2C devices add-ons. None known found.\n" );
   }
   else
   {
      log_line("Ruby: Done finding external I2C devices add-ons. Found %d known devices of which %d are configurable.", iKnown, iConfigurable );
      printf("Ruby: Done finding external I2C devices add-ons. Found %d known devices of which %d are configurable.\n", iKnown, iConfigurable );
   }
   #endif

   fflush(stdout);

   // Detect hardware
   hardware_getCameraType();
   hardware_getBoardType();

   #ifdef HW_PLATFORM_RASPBERRY
   hw_execute_ruby_process(NULL, "ruby_initdhcp", NULL, NULL);
   #endif

   detectSystemType();

   hardware_release();


   log_line("Starting Ruby system...");
   fflush(stdout);
   
   printf("Ruby: Finding serial ports...\n");
   log_line("Ruby: Finding serial ports...");
   fflush(stdout);

   hardware_init_serial_ports();

   printf("Ruby: Enumerating supported 2.4/5.8Ghz radio interfaces...\n");
   printf("\n");
   log_line("Ruby: Enumerating supported 2.4/5.8Ghz radio interfaces...");
   fflush(stdout);

   sprintf(szComm, "rm -rf %s%s", FOLDER_CONFIG, FILE_CONFIG_CURRENT_RADIO_HW_CONFIG);
   hw_execute_bash_command(szComm, NULL);

   hardware_enumerate_radio_interfaces_step(0);

   //int iCountHighCapacityInterfaces = hardware_get_radio_interfaces_count();

   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      printf("Ruby: No 2.4/5.8 Ghz radio interfaces found!\n");
      printf("\n");
      log_line("Ruby: No 2.4/5.8 Ghz radio interfaces found!");
      fflush(stdout);
   }
   else
   {
      printf("Ruby: %d radio interfaces found on 2.4/5.8 Ghz bands\n", hardware_get_radio_interfaces_count());
      printf("\n");
      log_line("Ruby: %d radio interfaces found on 2.4/5.8 Ghz bands", hardware_get_radio_interfaces_count());
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

   printf("Ruby: Finding serial radio interfaces...\n");
   log_line("Ruby: Finding serial radio interfaces...");
   fflush(stdout);

   int iCountAdded = hardware_radio_serial_parse_and_add_from_serial_ports_config();

   if ( 0 == iCountAdded )
   {
      printf("Ruby: No serial radio interfaces found.\n");
      log_line("Ruby: No serial radio interfaces found.");
   }
   else
   {
      printf("\nRuby: %d serial radio interfaces found.\n\n", iCountAdded);
      log_line("Ruby: %d serial radio interfaces found.", iCountAdded);
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
      if ( pSerialPort->iPortUsage == SERIAL_PORT_USAGE_SIK_RADIO )
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

#ifdef HW_PLATFORM_RASPBERRY
   log_line("Running on Raspberry Pi hardware");
#endif
#ifdef HW_PLATFORM_OPENIPC_CAMERA
   log_line("Running on OpenIPC hardware");
#endif

#ifdef FEATURE_CONCATENATE_SMALL_RADIO_PACKETS
   log_line("Feature to concatenate small radio packets is: On.");
#else
   log_line("Feature to concatenate small radio packets is: Off.");
#endif

#ifdef FEATURE_LOCAL_AUDIO_RECORDING
   log_line("Feature local audio recording is: On.");
#else
   log_line("Feature local audio recording is: Off.");
#endif

#ifdef FEATURE_MSP_OSD
   log_line("Feature MSP OSD is: On.");
#else
   log_line("Feature MSP OSD is: Off.");
#endif

#ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
   log_line("Feature vehicle computes adaptive video is: On.");
#else
   log_line("Feature vehicle computes adaptive video is: Off.");
#endif

#ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   log_line("Feature radio RxTx thread syncronization is: On.");
#else
   log_line("Feature radio RxTx thread syncronization is: Off.");
#endif

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

   if ( bIsFirstBoot )
      do_first_boot_initialization(s_isVehicle, board_type);
   
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
   if ( access( szFile, R_OK) == -1 )
      first_boot_create_default_model(s_isVehicle, board_type);
   
   hw_execute_ruby_process_wait(NULL, "ruby_start", "-ver", szOutput, 1);
   log_line("ruby_start: [%s]", szOutput);

   hw_execute_ruby_process_wait(NULL, "ruby_rt_vehicle", "-ver", szOutput, 1);
   log_line("ruby_rt_vehicle: [%s]", szOutput);
   hw_execute_ruby_process_wait(NULL, "ruby_tx_telemetry", "-ver", szOutput, 1);
   log_line("ruby_tx_telemetry: [%s]", szOutput);

   #ifdef HW_PLATFORM_RASPBERRY
   hw_execute_ruby_process_wait(NULL, "ruby_rt_station", "-ver", szOutput, 1);
   log_line("ruby_rt_station: [%s]", szOutput);
   hw_execute_ruby_process_wait(NULL, "ruby_rx_telemetry", "-ver", szOutput, 1);
   log_line("ruby_rx_telemetry: [%s]", szOutput);
   hw_execute_ruby_process_wait(NULL, "ruby_tx_rc", "-ver", szOutput, 1);
   log_line("ruby_tx_rc: [%s]", szOutput);
   hw_execute_ruby_process_wait(NULL, "ruby_central", "-ver", szOutput, 1);
   log_line("ruby_central: [%s]", szOutput);
   #endif


   _check_for_update_from_boot();

   if ( s_isVehicle )
   {
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      if ( ! modelVehicle.loadFromFile(szFile, true) )
      {
         modelVehicle.resetToDefaults(true);
         modelVehicle.is_spectator = false;
         modelVehicle.saveToFile(szFile, false);
      }
      
      char szOutput[4096];
      hw_execute_bash_command_raw("ls -al ruby_update* 2>/dev/null", szOutput);
      log_line("Update files: [%s]", szOutput);

      if ( access( "ruby_update_vehicle", R_OK ) != -1 )
         log_line("ruby_update_vehicle is present.");
      else
         log_line("ruby_update_vehicle is NOT present.");

      if ( access( "ruby_update", R_OK ) != -1 )
         log_line("ruby_update is present.");
      else
         log_line("ruby_update is NOT present.");
        
      if ( access( "ruby_update_worker", R_OK ) != -1 )
         log_line("ruby_update_worker is present.");
      else
         log_line("ruby_update_worker is NOT present.");

      if( access( "ruby_update_vehicle", R_OK ) != -1 )
      {
         printf("Ruby: Executing post update changes...\n");
         log_line("Executing post update changes...");
         fflush(stdout);
         hw_execute_ruby_process_wait(NULL, "ruby_update_vehicle", NULL, NULL, 1);
         hw_execute_bash_command("rm -f ruby_update_vehicle", NULL);
         printf("Ruby: Executing post update changes. Done.\n");
         log_line("Executing post update changes. Done.");

         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
         if ( ! modelVehicle.loadFromFile(szFile, true) )
         {
            modelVehicle.resetToDefaults(true);
            modelVehicle.is_spectator = false;
            modelVehicle.saveToFile(szFile, false);
         }
      }
   }
   else
   {
      if ( access( "ruby_update_controller", R_OK ) != -1 )
         log_line("ruby_update_controller is present.");
      else
         log_line("ruby_update_controller is NOT present.");

      if( access( "ruby_update_controller", R_OK ) != -1 )
      {
         printf("Ruby: Executing post update changes...\n");
         log_line("Executing post update changes...");
         fflush(stdout);
         hw_execute_ruby_process_wait(NULL, "ruby_update_controller", NULL, NULL, 1);
         hw_execute_bash_command("rm -f ruby_update_controller", NULL);
         printf("Ruby: Executing post update changes. Done.\n");
         log_line("Executing post update changes. Done.");
      }
   }

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VERSION);
   fd = fopen(szFile, "w");
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
      #ifdef HW_PLATFORM_RASPBERRY
      hw_execute_bash_command("cp -rf /home/pi/ruby/logs/log_start.txt /home/pi/ruby/logs/log_firstboot_start.txt", NULL);
      hw_execute_bash_command("cp -rf /home/pi/ruby/logs/log_system.txt /home/pi/ruby/logs/log_firstboot.txt", NULL);
      #endif
      #ifdef HW_PLATFORM_OPENIPC_CAMERA
      hw_execute_bash_command("cp -rf /tmp/logs/log_start.txt /root/ruby/log_firstboot_start.txt", NULL);
      hw_execute_bash_command("cp -rf /tmp/logs/log_system.txt /root/ruby/log_firstboot.txt", NULL);
      #endif
      hardware_sleep_ms(500);
      if ( ! g_bDebug )
         hw_execute_bash_command("reboot -f", NULL);
      return 0;
   }
 
   printf("Ruby: Checking for HW changes...");
   log_line("Checking for HW changes...");
   fflush(stdout);

   if ( s_isVehicle )
   {
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      if ( ! modelVehicle.loadFromFile(szFile, true) )
      {
         modelVehicle.resetToDefaults(true);
         modelVehicle.is_spectator = false;
         modelVehicle.saveToFile(szFile, false);
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
      log_line("|  Vehicle Id: %u", modelVehicle.uVehicleId);
      log_line("---------------------------------");
   }
   printf("Ruby: Starting processes...\n");
   log_line("Starting processes...");
   fflush(stdout);

   printf("Ruby: Init radio interfaces...\n");
   log_line("Init radio interfaces...");
   fflush(stdout);

   char szParams[64];
   strcpy(szParams, "-initradio");
   if ( s_isVehicle )
   {
      for ( int i=0; i<modelVehicle.radioInterfacesParams.interfaces_count; i++ )
      {
         if ( (modelVehicle.radioInterfacesParams.interface_type_and_driver[i] & 0xFF) == RADIO_TYPE_ATHEROS )
         if ( modelVehicle.radioInterfacesParams.interface_link_id[i] >= 0 )
         if ( modelVehicle.radioInterfacesParams.interface_link_id[i] < modelVehicle.radioLinksParams.links_count )
         {
            int dataRateMb = modelVehicle.radioLinksParams.link_datarate_video_bps[modelVehicle.radioInterfacesParams.interface_link_id[i]];
            if ( dataRateMb > 0 )
               dataRateMb = dataRateMb / 1000 / 1000;
            if ( dataRateMb > 0 )
            {
               sprintf(szParams, "-initradio %d", dataRateMb);
               break;
            }
         }
      }
   }
   hw_execute_ruby_process_wait(NULL, "ruby_start", szParams, NULL, 1);
   
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
         printf("Ruby: Current frequency: %s\n", str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[0]));
      else if ( modelVehicle.radioLinksParams.links_count == 2 )
      {
         char szFreq1[64];
         char szFreq2[64];
         strcpy(szFreq1, str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[1]));
         printf("Ruby: Current frequencies: %s/%s\n", szFreq1, szFreq2);
      }
      else
      {
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];
         strcpy(szFreq1, str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[1]));
         strcpy(szFreq3, str_format_frequency(modelVehicle.radioLinksParams.link_frequency_khz[2]));
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
      hw_execute_ruby_process(NULL, "ruby_start", "-vehicle", NULL);
   }
   else
   {
      #ifdef HW_PLATFORM_RASPBERRY

      u32 uControllerId = controller_utils_getControllerId();
      log_line("Controller UID: %u", uControllerId);

      hw_execute_bash_command_silent("con2fbmap 1 0", NULL);
      //execute_bash_command_silent("printf \"\\033c\"", NULL);
      //hw_launch_process("./ruby_controller");

      #ifdef HW_CAPABILITY_GPIO
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CONTROLLER_BUTTONS);
      if ( access(szFile, R_OK ) == -1 )
         hw_execute_ruby_process(NULL, "ruby_gpio_detect", NULL, NULL);
      #endif

      if ( hardware_radio_has_sik_radios() )
      {
         printf("Ruby: Configuring SiK radios...\n");
         log_line("Configuring SiK radios...");
         fflush(stdout);

         load_ControllerSettings();

         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
         if ( ! modelVehicle.loadFromFile(szFile, true) )
         {
            modelVehicle.resetToDefaults(true);
            modelVehicle.is_spectator = false;
            modelVehicle.saveToFile(szFile, false);
         }
         log_line("Setting default SiK radio params for all SiK radio interfaces on controller...");

         int iSiKRadioLinkIndex = -1;
         for( int i=0; i<modelVehicle.radioLinksParams.links_count; i++ )
         {
            if ( modelVehicle.radioLinkIsSiKRadio(i) )
            {
               iSiKRadioLinkIndex = i;
               break;
            }
         }

         u32 uFreq = DEFAULT_FREQUENCY_433;
         u32 uTxPower = DEFAULT_RADIO_SIK_TX_POWER;
         u32 uDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
         u32 uECC = 0;
         u32 uLBT = 0;
         u32 uMCSTR = 0;
         if ( iSiKRadioLinkIndex == -1 )
            log_line("No SiK radio link on current vehicle. Configuring SiK radio interfaces on controller with default parameters.");
         else
         {
            log_line("Current model radio link %d is a SiK radio link. Use it to configure controller.", iSiKRadioLinkIndex+1);
            uFreq = modelVehicle.radioLinksParams.link_frequency_khz[iSiKRadioLinkIndex];
            uTxPower = modelVehicle.radioInterfacesParams.txPowerSiK,
            uDataRate = modelVehicle.radioLinksParams.link_datarate_data_bps[iSiKRadioLinkIndex],
            uECC = (modelVehicle.radioLinksParams.link_radio_flags[iSiKRadioLinkIndex] & RADIO_FLAGS_SIK_ECC)?1:0;
            uLBT = (modelVehicle.radioLinksParams.link_radio_flags[iSiKRadioLinkIndex] & RADIO_FLAGS_SIK_LBT)?1:0;
            uMCSTR = (modelVehicle.radioLinksParams.link_radio_flags[iSiKRadioLinkIndex] & RADIO_FLAGS_SIK_MCSTR)?1:0;
         }

         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( hardware_radio_index_is_sik_radio(i) )
            {
               radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
               if ( NULL == pRadioHWInfo )
               {
                  log_softerror_and_alarm("Failed to get radio hardware info for radio interface %d.", i+1);
                  continue;
               }
               hardware_radio_sik_set_frequency_txpower_airspeed_lbt_ecc(pRadioHWInfo,
                  uFreq, uTxPower, uDataRate,
                  uECC, uLBT, uMCSTR,
                  NULL);
            }
         }
         log_line("Setting default SiK radio params for all SiK interfaces on controller. Done.");

         printf("Ruby: Configured SiK radios.\n");
         log_line("Configured SiK radios.");
         fflush(stdout);
      }

      printf("Ruby: Starting controller...\n");
      log_line("Starting controller...");
      fflush(stdout);

      hw_execute_ruby_process(NULL, "ruby_controller", NULL, NULL);

      #endif
   }
   
   printf("Ruby: Started processes. Checking if all ok...\n");
   fflush(stdout);

   for( int i=0; i<5; i++ )
      hardware_sleep_ms(500);

   if ( ! g_bDebug )
   for( int i=0; i<10; i++ )
      hardware_sleep_ms(500);
   
   log_line("Checking processes start...");
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, LOG_USE_PROCESS);
   if( access(szFile, R_OK) != -1 )
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

      #ifdef HW_PLATFORM_RASPBERRY
      hw_execute_bash_command("rm -rf /boot/last_ruby_boot.txt", NULL);
      hw_execute_bash_command("cp -rf logs/log_system.txt /boot/last_ruby_boot.txt", NULL);
      log_line("Copy boot log to /boot partition. Done.");
      #endif

      int iCheckCount = 0;
      while ( true )
      {
         bool bError = false;

         if ( hw_process_exists("ruby_start") )
         {
           if ( iCheckCount == 0 )
              log_line("ruby_start is started");
         }
         else
            { log_error_and_alarm("ruby_start is not running"); bError = true; }


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

         if ( bError )
            printf("Error: Some processes are not running.\n");
         else
         {
            u32 uTimeNow = get_current_timestamp_ms();
            char szTime[128];
            sprintf(szTime,"%d %d:%02d:%02d.%03d", s_iBootCount, (int)(uTimeNow/1000/60/60), (int)(uTimeNow/1000/60)%60, (int)((uTimeNow/1000)%60), (int)(uTimeNow%1000));
            printf("%s All processes are running fine.\n", szTime);
         }
         fflush(stdout);
         iCheckCount++;

         if ( g_bDebug )
            break;
           
         for( int i=0; i<10; i++ )
            hardware_sleep_ms(500);
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

      #ifdef HW_PLATFORM_RASPBERRY
      hw_execute_bash_command("rm -rf /boot/last_ruby_boot.txt", NULL);
      hw_execute_bash_command("cp -rf logs/log_system.txt /boot/last_ruby_boot.txt", NULL);      
      log_line("Copy boot log to /boot partition. Done.");
      #endif
      system("clear");
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
