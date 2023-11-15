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
#include "../common/string_utils.h"
#include "media.h"
#include "../base/launchers.h"
#include "../renderer/render_engine.h"
#include "popup.h"
#include "ruby_central.h"
#include "shared_vars.h"
#include "timers.h"

#include <sys/types.h>
#include <dirent.h>
#include <string.h>


static int s_iScreenshotsCountOnDisk = 0;
static int s_iVideoCountOnDisk = 0;

static int s_iMediaBootCount = 0;
static int s_iMediaInitCount = 0;
static char s_szMediaCurrentScreenshotFileName[1024];
static char s_szMediaCurrentVideoFileInfo[1024];

void _media_remove_invalid_files()
{
   DIR *d;
   FILE* fd;
   struct dirent *dir;
   char szFile[1024];
   char szComm[1024];
   log_line("Searching and removing invalid media files...");
   d = opendir(FOLDER_MEDIA);
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if ( strlen(dir->d_name) < 4 )
            continue;

         snprintf(szFile, sizeof(szFile), "%s%s", FOLDER_MEDIA, dir->d_name);
         long lSize = 0;
         fd = fopen(szFile, "rb");
         if ( NULL != fd )
         {
            fseek(fd, 0, SEEK_END);
            lSize = ftell(fd);
            fseek(fd, 0, SEEK_SET);
            fclose(fd);
         }
         else
            log_softerror_and_alarm("Failed to open file [%s] for checking it's size.", szFile);
         if ( lSize > 3 )
            continue;
         log_line("Removing invalid media file: [%s]", dir->d_name);

         // Remove invalid file and info file for it

         strcpy(szFile, dir->d_name);
         int pos = strlen(szFile);
         while (pos > 0 && szFile[pos] != '.')
            pos--;
         pos++;

         szFile[pos] = 0;
         strlcat(szFile, "h264", sizeof(szFile));
         snprintf(szComm, sizeof(szComm), "rm -rf %s%s", FOLDER_MEDIA, szFile);
         hw_execute_bash_command(szComm, NULL);

         szFile[pos] = 0;
         strlcat(szFile, "info", sizeof(szFile));
         snprintf(szComm, sizeof(szComm), "rm -rf %s%s", FOLDER_MEDIA, szFile);
         hw_execute_bash_command(szComm, NULL);

         szFile[pos] = 0;
         strlcat(szFile, "png", sizeof(szFile));
         snprintf(szComm, sizeof(szComm), "rm -rf %s%s", FOLDER_MEDIA, szFile);
         hw_execute_bash_command(szComm, NULL);

         ruby_signal_alive();
      }
      closedir(d);
   }
   else
      log_softerror_and_alarm("Failed to open media dir to search for invalid videos.");
   log_line("Searching and removing invalid media files complete.");
}


bool media_init_and_scan()
{
   log_line("Media Storage: Init media storage...");
   ruby_pause_watchdog();

   s_iMediaBootCount = 0;
   s_szMediaCurrentScreenshotFileName[0] = 0;
   s_szMediaCurrentVideoFileInfo[0] = 0;
   s_iMediaInitCount++;

   FILE* fd = fopen(FILE_BOOT_COUNT, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%d", &s_iMediaBootCount) )
         s_iMediaBootCount = 0;
      fclose(fd);
   }

   media_scan_files();

   ruby_resume_watchdog();
   log_line("Media Storage: Init media storage completed.");

   return true;
}

void media_scan_files()
{
   char szFile[1024];
   char szOutBuff[1024];

   _media_remove_invalid_files();
 

   // Count files in a folder:
   // ls media/ | grep picture- | wc -l
   // ls media/ | grep '.info' | wc -l

   s_iScreenshotsCountOnDisk = 0;
   szOutBuff[0] = 0;
   hw_execute_bash_command("ls media/ | grep picture- | wc -l", szOutBuff);
   if ( 0 < strlen(szOutBuff) )
      s_iScreenshotsCountOnDisk = strtol(szOutBuff, NULL, 10);

   s_iVideoCountOnDisk = 0;
   szOutBuff[0] = 0;
   hw_execute_bash_command("ls media/ | grep '.info' | wc -l", szOutBuff);
   if ( 0 < strlen(szOutBuff) )
      s_iVideoCountOnDisk = strtol(szOutBuff, NULL, 10);

   ruby_signal_alive();

   log_line("Media storage: Found %d screenshots on storage.", s_iScreenshotsCountOnDisk );
   log_line("Media storage: Found %d videos on storage.", s_iVideoCountOnDisk );
}

int media_get_screenshots_count()
{
   return s_iScreenshotsCountOnDisk;
}

int media_get_videos_count()
{
   return s_iVideoCountOnDisk;
}


char* media_get_screenshot_filename()
{
   char vehicle_name[MAX_VEHICLE_NAME_LENGTH+1];

   strcpy(vehicle_name, "none");
   if ( NULL != g_pCurrentModel )
      strcpy(vehicle_name, g_pCurrentModel->vehicle_name);
   if ( 0 == strlen(vehicle_name) || (1 == strlen(vehicle_name) && vehicle_name[0] == ' ') )
      strcpy(vehicle_name, "none");
    
   str_sanitize_filename(vehicle_name);

   snprintf(s_szMediaCurrentScreenshotFileName, sizeof(s_szMediaCurrentScreenshotFileName), FILE_FORMAT_SCREENSHOT, vehicle_name, s_iMediaBootCount, g_TimeNow/1000, g_TimeNow%1000 );
   return s_szMediaCurrentScreenshotFileName;
}

char* media_get_video_filename()
{
   char vehicle_name[MAX_VEHICLE_NAME_LENGTH+1];

   strcpy(vehicle_name, "none");
   if ( NULL != g_pCurrentModel )
      strcpy(vehicle_name, g_pCurrentModel->vehicle_name);
   if ( 0 == strlen(vehicle_name) || (1 == strlen(vehicle_name) && vehicle_name[0] == ' ') )
      strcpy(vehicle_name, "none");

   str_sanitize_filename(vehicle_name);

   snprintf(s_szMediaCurrentVideoFileInfo, sizeof(s_szMediaCurrentVideoFileInfo), FILE_FORMAT_VIDEO_INFO, vehicle_name, s_iMediaBootCount, g_TimeNow/1000, g_TimeNow%1000 );
   return s_szMediaCurrentVideoFileInfo;
}

bool media_take_screenshot(bool bIncludeOSD)
{
   if ( ! bIncludeOSD )
   {
      g_pRenderEngine->startFrame();
      g_pRenderEngine->endFrame();
      hardware_sleep_ms(20);
   }

   char szFile[1024];

   media_get_screenshot_filename();
   strcpy(szFile, FOLDER_MEDIA);
   strlcat(szFile, s_szMediaCurrentScreenshotFileName, sizeof(szFile));

   ruby_signal_alive();
   hw_launch_process2("./raspi2png", "-p", szFile);
   ruby_signal_alive();

   log_line("Media Storage: Took a screenshot to file: %s", szFile);
   s_iScreenshotsCountOnDisk++;

   Popup* p = new Popup("Screenshot taken", 0.1,0.72, 2);
   popups_add_topmost(p);
   return true;
}
