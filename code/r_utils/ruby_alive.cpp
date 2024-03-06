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

bool gbQuit = false;

void power_leds(int onoff)
{
   #ifdef HW_PLATFORM_RASPBERRY
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
         snprintf(szBuff, 1023, "/sys/class/leds/%s/brightness", dir->d_name);
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
   #endif
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

   char szFile[128];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_UPDATE_IN_PROGRESS);

   char szFileAlarm[128];
   strcpy(szFileAlarm, FOLDER_RUBY_TEMP);
   strcat(szFileAlarm, FILE_TEMP_ALARM_ON);

   bool bHasAlarm = false;
   bool bHasUpdateInProgress = false;
   
   while ( ! gbQuit )
   {
      u32 uTimeStart = get_current_timestamp_ms();

      hardware_sleep_ms(nSleepMs);
      counter++;

      if ( (counter % 20) == 0 )
      {
         if ( access(szFileAlarm, R_OK) != -1 )
            bHasAlarm = true;
         else
            bHasAlarm = false;

         if ( access(szFile, R_OK) != -1 )
            bHasUpdateInProgress = true;
         else
            bHasUpdateInProgress = false;
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
      else if ( bHasUpdateInProgress )
      {
         if ( (counter % 2) == 0 )
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