/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include "shared_vars.h"
#include "timers.h"

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
   
   for( int i=0; i<5; i++ )
      hardware_sleep_ms(800);
   
   int sleepIntervalMs = 200;
   u32 maxTimeForProcess = 9000;
   int counter = 0;

   log_line("Enter watchdog state");
   log_line_watchdog("Watchdog state started.");
 
   char szFilePause[128];
   strcpy(szFilePause, FOLDER_RUBY_TEMP);
   strcat(szFilePause, FILE_TEMP_CONTROLLER_PAUSE_WATCHDOG);

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

      static u32 s_uTimeLastClearScreen = 0;
      if ( g_TimeNow > s_uTimeLastClearScreen + 20000 )
      {
         s_uTimeLastClearScreen = g_TimeNow;
         system("clear");
      }

      if ( access(szFilePause, R_OK) != -1 )
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
         if ( (g_TimeNow > maxTimeForProcess) && (s_pProcessStatsCentral->lastActiveTime + maxTimeForProcess < g_TimeNow ) )
         {
            log_format_time(s_pProcessStatsCentral->lastActiveTime, szTime);
            log_line_watchdog("Ruby controller watchdog check failed: ruby_central process has stopped !!! Last active time: %s", szTime);
            bMustRestart = true;
         }
      }

      u32 dTime = get_current_timestamp_ms() - uTimeStart;
      if ( dTime > 250 )
         log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);

      if ( bMustRestart )
      {
         for( int i=0; i<10000; i++ )
            hardware_sleep_ms(500);
           
         log_line_watchdog("Restarting ruby_central process...");

         hw_stop_process("ruby_central");
         shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_CENTRAL, s_pProcessStatsCentral);   
         s_pProcessStatsCentral = NULL;

         char szComm[128];
         sprintf(szComm, "touch %s%s", FOLDER_CONFIG, FILE_TEMP_CONTROLLER_CENTRAL_CRASHED);
         hw_execute_bash_command(szComm, NULL);
   
         hardware_sleep_ms(200);

         hw_execute_bash_command("./ruby_central -reinit &", NULL);
         for( int i=0; i<3; i++ )
            hardware_sleep_ms(800);
   
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