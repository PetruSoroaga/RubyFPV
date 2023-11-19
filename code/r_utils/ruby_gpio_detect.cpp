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
#include "../base/gpio.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/models.h"
#include "../common/string_utils.h"
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


bool g_bQuit = false;
bool g_bDebug = false;

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

   log_init("RubyGPIODetect");


   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebug = true;
   if ( g_bDebug )
      log_enable_stdout();
   
   if ( access(FILE_CONTROLLER_BUTTONS, R_OK ) != -1 )
   {
      log_line("GPIO configuration is already detected (config buttons file exists).");
      FILE* fp = fopen(FILE_CONTROLLER_BUTTONS, "rb");
      if ( NULL != fp )
      {
         int iDetected = 0;
         int iPullDirection = 0;
         if ( 2 == fscanf(fp, "%d %d", &iDetected, &iPullDirection) )
         {
            log_line("GPIO direction detected: %s, direction: %s",
                (iDetected==1)?"yes":"not yet",
                (iPullDirection==0)?"pulled down":"pulled up");
            fclose(fp);
            return 0;
         }
         log_softerror_and_alarm("Failed to read buttons config file [%s]", FILE_CONTROLLER_BUTTONS);
         fclose(fp);
      }
      else
         log_softerror_and_alarm("Failed to open [%s] buttons config file.", FILE_CONTROLLER_BUTTONS);
   }
   else
      log_line("Buttons config file [%s] does not exists.", FILE_CONTROLLER_BUTTONS);


   log_line("Start to detect GPIO configuration...");

   while ( ! g_bQuit )
   {
      GPIOInitButtons();
   
      while ( ! g_bQuit )
      {
         hardware_sleep_ms(10);
         int iRes = GPIOButtonsTryDetectPullUpDown();
         if ( iRes == 1 )
         {
            log_line("Detected GPIO direction.");
            break;
         }
      }

      log_line("Finished doing GPIO detection (quit: %s).", g_bQuit?"yes":"no");
      FILE* fp = fopen(FILE_CONTROLLER_BUTTONS, "rb");
      if ( NULL != fp )
      {
         int iDetected = 0;
         int iPullDirection = 0;
         int iGPIOPin = 0;
         if ( 3 == fscanf(fp, "%d %d %d", &iDetected, &iPullDirection, &iGPIOPin) )
         {
            log_line("GPIO direction detected: %s, direction: %s, initial GPIO pin that was detected: %d",
                   (iDetected==1)?"yes":"not yet",
                   (iPullDirection==0)?"pulled down":"pulled up",
                   iGPIOPin);
            fclose(fp);
            break;
         }
         log_softerror_and_alarm("Failed to read values from buttons config file [%s]", FILE_CONTROLLER_BUTTONS);
         fclose(fp);
      }
      else
         log_softerror_and_alarm("Failed to open [%s] buttons config file. Retry detection.", FILE_CONTROLLER_BUTTONS);
   }
   log_line("GPIO detection process finished.");
   return 0;
} 