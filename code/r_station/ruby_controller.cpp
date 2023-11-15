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
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/gpio.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/shared_mem.h"

bool g_bQuit = false;
u32 g_TimeNow = 0;
shared_mem_process_stats* s_pProcessStatsCentral = NULL; 

void try_open_process_stats()
{
   if ( NULL == s_pProcessStatsCentral )
   {
      s_pProcessStatsCentral = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_CENTRAL);
      if ( NULL == s_pProcessStatsCentral )
         log_softerror_and_alarm("Failed to open shared mem to ruby central process watchdog for reading: %s", SHARED_MEM_WATCHDOG_CENTRAL);
      else
         log_line("Opened shared mem to ruby central process watchdog for reading.");
   }
}

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
} 

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);
   
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }   

   log_init("RubyController");

   hw_launch_process("./ruby_central");
   
   hardware_sleep_ms(800);
   hardware_sleep_ms(800);
   hardware_sleep_ms(800);
   hardware_sleep_ms(800);
   hardware_sleep_ms(800);

   int sleepIntervalMs = 200;
   u32 maxTimeForProcess = 6000;
   int counter = 0;

   log_line("Enter watchdog state");
   log_line_watchdog("Watchdog state started.");
 
   while ( ! g_bQuit )
   {
      u32 uTimeStart = get_current_timestamp_ms();

      hardware_sleep_ms(sleepIntervalMs);
      counter++;
      if ( counter % 10 )
      {
         u32 dTime = get_current_timestamp_ms() - uTimeStart;
         if ( dTime > 250 )
            log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
         continue;
      }
      
      // Executes once every 2 seconds

      g_TimeNow = get_current_timestamp_ms();
      if ( NULL == s_pProcessStatsCentral )
         try_open_process_stats();

      if ( access( FILE_TMP_CONTROLLER_PAUSE_WATCHDOG, R_OK ) != -1 )
      {
         log_line("Watchdog is paused.");
         u32 dTime = get_current_timestamp_ms() - uTimeStart;
         if ( dTime > 250 )
            log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
         continue;
      }

      bool bMustRestart = false;
      char szTime[64];
      if ( NULL != s_pProcessStatsCentral )
      {
         if ( (g_TimeNow > maxTimeForProcess) && (s_pProcessStatsCentral->lastActiveTime < g_TimeNow - maxTimeForProcess) )
         {
            log_format_time(s_pProcessStatsCentral->lastActiveTime, szTime, sizeof(szTime));
            log_line_watchdog("Ruby controller watchdog check failed: ruby_central process has stopped !!! Last active time: %s", szTime);
            bMustRestart = true;
         }
      }

      u32 dTime = get_current_timestamp_ms() - uTimeStart;
      if ( dTime > 250 )
         log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);

      if ( bMustRestart )
      {
         log_line_watchdog("Restarting ruby_central process...");

         hw_stop_process("ruby_central");
         shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_CENTRAL, s_pProcessStatsCentral);   
         s_pProcessStatsCentral = NULL;

         char szComm[256];
         snprintf(szComm, sizeof(szComm), "touch %s", FILE_TMP_CONTROLLER_CENTRAL_CRASHED);
         hw_execute_bash_command(szComm, NULL);
   
         hardware_sleep_ms(200);

         hw_execute_bash_command("./ruby_central -reinit &", NULL);
         hardware_sleep_ms(900);
         hardware_sleep_ms(900);
         while ( hw_process_exists("ruby_central") )
         {
            log_line("Ruby reinit still in progress...");
            hardware_sleep_ms(200);
         }
         hardware_sleep_ms(200);
         hw_launch_process("./ruby_central");
         log_line_watchdog("Restarting ruby_central process done.");
      } 
   }

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_CENTRAL, s_pProcessStatsCentral);   
   s_pProcessStatsCentral = NULL;
   log_line("Finished execution.");
   log_line_watchdog("Finished execution");
   return (0);
} 