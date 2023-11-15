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
#include "../base/gpio.h"
#include "../base/hardware.h"

bool gbQuit = false;

void power_leds(int onoff)
{
   DIR *d;
   struct dirent *dir;
   char szBuff[1024];
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

   if ( onoff )
      GPIOWrite(GPIO_PIN_LED_ERROR, HIGH);
   else
      GPIOWrite(GPIO_PIN_LED_ERROR, LOW);
}

void handle_sigint(int sig) 
{ 
   gbQuit = true;
} 

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("RubyAlive");

   init_hardware_only_status_led();

   int counter = 0;
   int nSleepMs = 100;
   int nLeds = 0;
   
   bool bHasAlarm = false;

   while ( ! gbQuit )
   {
      u32 uTimeStart = get_current_timestamp_ms();

      hardware_sleep_ms(nSleepMs);
      counter++;

      if ( (counter % 20) == 0 )
      {
         if ( access( FILE_TMP_ALARM_ON, R_OK ) != -1 )
            bHasAlarm = true;
         else
            bHasAlarm = false;
      }

      if ( (counter % 100) == 0 )
      {
         if ( bHasAlarm )
            log_line("Pi is alive. Alarm on.");
         else
            log_line("Pi is alive");
      }

      if ( bHasAlarm )
      {
         if ( counter % 2 )
         {
            nLeds = 1 - nLeds;
            power_leds(nLeds);
         }
      }
      else
      {
         if ( hardware_is_vehicle() )
         {
            if ( (counter%10) == 0 )
            {
               nLeds = 1;
               power_leds(nLeds);
            }
            if ( (counter%10) == 3 )
            {
               nLeds = 0;
               power_leds(nLeds);
            }
         }
         else
         {
            if ( (counter%30) == 0 )
            {
               nLeds = 1;
               power_leds(nLeds);
            }
            if ( (counter%30) == 5 )
            {
               nLeds = 0;
               power_leds(nLeds);
            }
         }
      }

      u32 dTime = get_current_timestamp_ms() - uTimeStart;
      if ( dTime > 150 )
         log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
   }

   power_leds(1);
   hardware_release();
   return (0);
} 