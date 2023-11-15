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
#include "../base/hw_procs.h"
#include "../base/launchers.h"
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
   char szComm[1024];
   //char szCommOutput[4096];
   char szFileIn[256];
   char szFileInfo[1024];
   int fps = 0;
   int length = 0;
   int width, height;

   FILE* fd = fopen(TEMP_VIDEO_FILE_INFO, "r");
   if ( NULL == fd )
   {
      fd = fopen(TEMP_VIDEO_FILE_PROCESS_ERROR, "a");
      fprintf(fd, "%s\n", "Failed to read video recording info file.");
      fclose(fd);
      log_softerror_and_alarm("Failed to open video recording info file: %s", TEMP_VIDEO_FILE_INFO);
      return false;
   }
   if ( 3 != fscanf(fd, "%s %d %d", szFileIn, &fps, &length) )
   {
      fclose(fd);
      fd = fopen(TEMP_VIDEO_FILE_PROCESS_ERROR, "a");
      fprintf(fd, "%s\n", "Failed to read video recording info file. Invalid file.");
      log_softerror_and_alarm("Failed to read video recording info file 1/2: %s", TEMP_VIDEO_FILE_INFO);
      fclose(fd);
      return false;
   }
   if ( 2 != fscanf(fd, "%d %d", &width, &height) )
   {
      fclose(fd);
      fd = fopen(TEMP_VIDEO_FILE_PROCESS_ERROR, "a");
      fprintf(fd, "%s\n", "Failed to read video recording info file. Invalid file.");
      log_softerror_and_alarm("Failed to read video recording info file 2/2: %s", TEMP_VIDEO_FILE_INFO);
      fclose(fd);
      return false;
   }
   fclose(fd);

   if ( NULL != strstr(szFileIn, TEMP_VIDEO_MEM_FOLDER) )
   {
      snprintf(szComm, sizeof(szComm), "nice -n %d mv %s %s", niceValue, szFileIn, TEMP_VIDEO_FILE);
      hw_execute_bash_command(szComm, NULL);

      snprintf(szComm, sizeof(szComm), "umount %s", TEMP_VIDEO_MEM_FOLDER);
      hw_execute_bash_command(szComm, NULL);
      strcpy(szFileIn, TEMP_VIDEO_FILE);
   }

   char szOutFile[512];
   char szOutFileInfo[512];
   szOutFile[0] = 0;
   szOutFileInfo[0] = 0;

   fd = fopen(TEMP_VIDEO_FILE_INFO, "r");
   if ( NULL == fd )
   {
      fd = fopen(TEMP_VIDEO_FILE_PROCESS_ERROR, "a");
      fprintf(fd, "%s\n", "Failed to read video output info file.");
      log_softerror_and_alarm("Failed to read video output info file 1/2: %s", TEMP_VIDEO_FILE_INFO);
      fclose(fd);
      return false;
   }
   else
   {
      if ( 1 != fscanf(fd, "%s", szOutFileInfo) )
      {
         fclose(fd);
         fd = fopen(TEMP_VIDEO_FILE_PROCESS_ERROR, "a");
         fprintf(fd, "%s\n", "Failed to read video output info file. Invalid file.");
         log_softerror_and_alarm("Failed to read video output info file 1/2: %s", TEMP_VIDEO_FILE_INFO);
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
   snprintf(szOutFileInfo, sizeof(szOutFileInfo), FILE_FORMAT_VIDEO_INFO, vehicle_name, g_iBootCount, timeNow/1000, timeNow%1000 );

    strlcpy(szOutFile, szOutFileInfo, sizeof(szOutFile));
   szOutFile[strlen(szOutFile)-4] = 'h';
   szOutFile[strlen(szOutFile)-3] = '2';
   szOutFile[strlen(szOutFile)-2] = '6';
   szOutFile[strlen(szOutFile)-1] = '4';

   snprintf(szFileInfo, sizeof(szFileInfo), "%s%s", FOLDER_MEDIA, szOutFileInfo);
   fd = fopen(szFileInfo, "w");
   if ( NULL == fd )
   {
      fd = fopen(TEMP_VIDEO_FILE_PROCESS_ERROR, "a");
      fprintf(fd, "%s\n", "Failed to write video output information file.");
      log_softerror_and_alarm("Failed to write video output info file: %s", TEMP_VIDEO_FILE_PROCESS_ERROR);
      fclose(fd);
      return false;
   }
   fprintf(fd, "%s\n", szOutFile);
   fprintf(fd, "%d %d\n", fps, length );
   fprintf(fd, "%d %d\n", width, height );
   fclose(fd);

   log_line("Moving video file %s to: %s%s", szFileIn, FOLDER_MEDIA, szOutFile);

   snprintf(szComm, sizeof(szComm), "nice -n %d mv %s %s%s", niceValue, szFileIn, FOLDER_MEDIA, szOutFile);
   hw_execute_bash_command(szComm, NULL);
   //launch_set_proc_priority("cp", 10,0,1);

   snprintf(szComm, sizeof(szComm), "rm -rf %s", TEMP_VIDEO_FILE_INFO);
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
   snprintf(szComm, sizeof(szComm), "ffmpeg -framerate %d -y -i %s%s -c:v copy %s 2>&1 1>/dev/null", fps, FOLDER_MEDIA, szFileIn, szFileOut);
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
   if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL) )
   {
      log_softerror_and_alarm("Failed to load current model.");
      g_pCurrentModel = new Model();
   }

   FILE* fd = fopen(FILE_BOOT_COUNT, "r");
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