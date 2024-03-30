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
        Copyright info and developer info must be preserved as is in the user
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

#include "../base/base.h"
#include "../base/config.h"
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

Model* g_pCurrentModel = NULL;
int g_iBootCount = 0;

bool gbQuit = false;
bool g_bDebug = false;
int niceValue = 10;

bool store_video()
{
   char szFile[128];
   char szComm[1024];
   //char szCommOutput[4096];
   char szFileIn[256];
   char szFileInfo[1024];
   int fps = 0;
   int length = 0;
   int width, height;

   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_VIDEO_FILE_INFO);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to open video recording info file: %s", szFile);
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
      fd = fopen(szFile, "a");
      fprintf(fd, "%s\n", "Failed to read video recording info file.");
      fclose(fd);
      return false;
   }
   if ( 3 != fscanf(fd, "%s %d %d", szFileIn, &fps, &length) )
   {
      fclose(fd);
      log_softerror_and_alarm("Failed to read video recording info file 1/2: %s", szFile);
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
      fd = fopen(szFile, "a");
      fprintf(fd, "%s\n", "Failed to read video recording info file. Invalid file.");
      fclose(fd);
      return false;
   }
   if ( 2 != fscanf(fd, "%d %d", &width, &height) )
   {
      log_softerror_and_alarm("Failed to read video recording info file 2/2: %s", szFile);
      fclose(fd);
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
      fd = fopen(szFile, "a");
      fprintf(fd, "%s\n", "Failed to read video recording info file. Invalid file.");
      fclose(fd);
      return false;
   }
   fclose(fd);

   if ( NULL != strstr(szFileIn, FOLDER_TEMP_VIDEO_MEM) )
   {
      snprintf(szComm, 511, "nice -n %d mv %s %s%s", niceValue, szFileIn, FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE);
      hw_execute_bash_command(szComm, NULL);

      sprintf(szComm, "umount %s", FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command(szComm, NULL);
      strcpy(szFileIn, FOLDER_RUBY_TEMP);
      strcat(szFileIn, FILE_TEMP_VIDEO_FILE);
   }

   char szOutFile[512];
   char szOutFileInfo[512];
   szOutFile[0] = 0;
   szOutFileInfo[0] = 0;

   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_VIDEO_FILE_INFO);
   fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to read video output info file 1/2: %s", szFile);
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
      fd = fopen(szFile, "a");
      fprintf(fd, "%s\n", "Failed to read video output info file.");
      fclose(fd);
      return false;
   }
   else
   {
      if ( 1 != fscanf(fd, "%s", szOutFileInfo) )
      {
         fclose(fd);
         log_softerror_and_alarm("Failed to read video output info file 1/2: %s", szFile);
         strcpy(szFile, FOLDER_RUBY_TEMP);
         strcat(szFile, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
         fd = fopen(szFile, "a");
         fprintf(fd, "%s\n", "Failed to read video output info file. Invalid file.");
         fclose(fd);
         return false;
      }
   }
   fclose(fd);

   char vehicle_name[MAX_VEHICLE_NAME_LENGTH+1];

   strcpy(vehicle_name, "none");
   if ( NULL != g_pCurrentModel )
      strcpy(vehicle_name, g_pCurrentModel->vehicle_name);
   if ( 0 == strlen(vehicle_name) || (1 == strlen(vehicle_name) && vehicle_name[0] == ' ') )
      strcpy(vehicle_name, "none");

   str_sanitize_filename(vehicle_name);

   u32 timeNow = get_current_timestamp_ms();
   sprintf(szOutFileInfo, FILE_FORMAT_VIDEO_INFO, vehicle_name, g_iBootCount, timeNow/1000, timeNow%1000 );

   strncpy(szOutFile, szOutFileInfo, 512);
   szOutFile[strlen(szOutFile)-4] = 'h';
   szOutFile[strlen(szOutFile)-3] = '2';
   szOutFile[strlen(szOutFile)-2] = '6';
   szOutFile[strlen(szOutFile)-1] = '4';

   snprintf(szFileInfo, 1023, "%s%s", FOLDER_MEDIA, szOutFileInfo);
   fd = fopen(szFileInfo, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to write video output info file: %s", szFile);
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_VIDEO_FILE_INFO);
      fd = fopen(szFile, "a");
      fprintf(fd, "%s\n", "Failed to write video output information file.");
      fclose(fd);
      return false;
   }
   fprintf(fd, "%s\n", szOutFile);
   fprintf(fd, "%d %d\n", fps, length );
   fprintf(fd, "%d %d\n", width, height );
   fclose(fd);

   log_line("Moving video file %s to: %s%s", szFileIn, FOLDER_MEDIA, szOutFile);

   snprintf(szComm, 1023, "nice -n %d mv %s %s%s", niceValue, szFileIn, FOLDER_MEDIA, szOutFile);
   hw_execute_bash_command(szComm, NULL);
   //launch_set_proc_priority("cp", 10,0,1);

   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_INFO);
   hw_execute_bash_command(szComm, NULL);

   return true;
}


bool process_video(char* szFileInfo, char* szFileOut)
{
   char szFileIn[512];
   char szComm[1024];
   int fps = 0;
   int length = 0;

   FILE* fd = fopen(szFileInfo, "r");
   if ( NULL == fd )
      return false;
   if ( 3 != fscanf(fd, "%s %d %d", szFileIn, &fps, &length) )
   {
      fclose(fd);
      return false;
   }
   fclose(fd);

   log_line("Processing video file: %s, fps: %d, length: %d seconds.", szFileIn, fps, length);
   if ( length < 2 )
   {
      log_softerror_and_alarm("Empty video file (less than 3 seconds) received for processing. Ignoring it.");
      return true;
   }

   // Convert input file to output file
   snprintf(szComm, 1023, "ffmpeg -framerate %d -y -i %s%s -c:v copy %s 2>&1 1>/dev/null", fps, FOLDER_MEDIA, szFileIn, szFileOut);
   log_line("Execute ffmpeg: %s", szComm);
   //hw_execute_bash_command(szComm, NULL);
   system(szComm);
   log_line("Finished processing video to mp4: %s", szFileOut);
   //launcher_set_proc_priority("ffmpeg", 10,0,1);
   return true;
}


void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   gbQuit = true;
} 

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("RubyVideoProcessor");

   //hardware_set_priority(2);

   g_pCurrentModel = new Model();
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
   if ( ! g_pCurrentModel->loadFromFile(szFile) )
   {
      log_softerror_and_alarm("Failed to load current model.");
      g_pCurrentModel = new Model();
   }

   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_BOOT_COUNT);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%d", &g_iBootCount) )
         g_iBootCount = 0;
      fclose(fd);
   }


   char szFileInfo[1024];
   char szFileOut[1024];
   bool bStoreOnly = true;

   if ( argc >= 2 )
   {
      strcpy(szFileInfo, argv[1]);
      strcpy(szFileOut, argv[2]);
      bStoreOnly = false;
   }

   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebug = true;
   if ( g_bDebug )
      log_enable_stdout();
   
   if ( bStoreOnly )
   {
      log_line("Processing input video file.");
      store_video();
   }
   else
   {
      log_line("Processing info file %s to output video file %s", szFileInfo, szFileOut);
      process_video(szFileInfo, szFileOut);
   }

   log_line("Finished processing video file.");
   return (0);
} 