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
#include <sys/resource.h>
#include <unistd.h>
#include <ctype.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/radio_utils.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"


bool gbQuit = false;
u32 g_TimeNow = 0;

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   gbQuit = true;
} 

void process_custom_commands_file()
{
   log_line("Try to execute custom commands in file: %s", FILE_UPDATE_CMD_LIST);

   if ( access( FILE_UPDATE_CMD_LIST, R_OK ) == -1 )
   {
      log_line("No custom commands file: %s", FILE_UPDATE_CMD_LIST);
      return;
   }
   FILE* fd = fopen(FILE_UPDATE_CMD_LIST, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Can't open custom commands file: %s", FILE_UPDATE_CMD_LIST);
      return;
   }

   char szComm[256];

   while ( true )
   {
      szComm[0] = 0;
      if ( 1 != fscanf(fd, "%s", szComm) )
         break;

      if ( 0 == strcmp(szComm, "cp") )
      {
         char szFileIn[256];
         char szFileOut[256];
         char szFolder[256];
         if ( 3 != fscanf(fd, "%s %s %s", szFileIn, szFolder, szFileOut) )
         {
            log_softerror_and_alarm("Invalid copy comand");
            continue;
         }
         char szCommand[1024];
         sprintf(szCommand, "cp -rf %s %s/%s", szFileIn, szFolder, szFileOut);
         hw_execute_bash_command(szCommand, NULL);
      }
      if ( 0 == strcmp(szComm, "mv") )
      {
         char szFileIn[256];
         char szFileOut[256];
         char szFolder[256];
         if ( 3 != fscanf(fd, "%s %s %s", szFileIn, szFolder, szFileOut) )
         {
            log_softerror_and_alarm("Invalid copy comand");
            continue;
         }
         char szCommand[1024];
         sprintf(szCommand, "mv -f %s %s/%s", szFileIn, szFolder, szFileOut);
         hw_execute_bash_command(szCommand, NULL);
      }
      else if ( 0 == strcmp(szComm, "cmd") )
      {
         char * line = NULL;
         size_t len = 0;
         ssize_t readline;
         readline = getline(&line, &len, fd);
         if ( readline < 1 )
         {
            log_softerror_and_alarm("Invalid comand");
            continue;
         }        
         char szCommand[1024];
         sprintf(szCommand, "%s", line);
         len = strlen(szCommand)-1;
         while ( len > 0 && (szCommand[len] == 10 || szCommand[len] == 13 || szCommand[len] == '\r' || szCommand[len] == '\t' || szCommand[len] == ' ' ) )
         {
            szCommand[len] = 0;
            len--;
         }
         log_line("Execute custom command: [%s]", szCommand);
         hw_execute_bash_command(szCommand, NULL);
      }
   }
      
   fclose(fd);
   log_line("Done executing custom commands from file: %s", FILE_UPDATE_CMD_LIST);

   // Do not delete the commands file, can be used to be sent to vehicle as on the fly archive.
   //sprintf(szComm, "rm -rf %s", FILE_UPDATE_CMD_LIST );
   //hw_execute_bash_command(szComm, NULL);
}

long compute_file_sizes()
{
   long lSize = 0;
   long lTotalSize = 0;

   lSize = get_filesize("ruby_central");
   if ( -1 == lSize )
      return -1;    

   lTotalSize += lSize;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_rt_station");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_update");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_rt_vehicle");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_start");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_update");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_rx_telemetry");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_tx_telemetry");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_tx_rc");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   lSize = get_filesize("ruby_i2c");
   if ( -1 == lSize )
      return -1;

   lTotalSize += lSize;

   return lTotalSize;
}

void _write_return_code(int iCode)
{
   hw_execute_bash_command("chmod 777 tmp/tmp_update_result.txt", NULL);
   FILE* fd = fopen("tmp/tmp_update_result.txt", "wb");
   if ( fd != NULL )
   {
      fprintf(fd, "%d\n", iCode);
      fclose(fd);
      return;
   }
   log_softerror_and_alarm("Failed to write update result to file tmp/tmp_update_result.txt");
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
   
   log_init("RubyUpdateWorker");
   hardware_sleep_ms(500);

   char szComm[256];
   char szFile[MAX_FILE_PATH_SIZE];
   char szFoundFile[MAX_FILE_PATH_SIZE];
   char szZipFileName[MAX_FILE_PATH_SIZE];
   char szZipFullFilePath[MAX_FILE_PATH_SIZE];
   char szUpdateFromSrcFolder[MAX_FILE_PATH_SIZE];
   strcpy(szUpdateFromSrcFolder, FOLDER_RUBY_TEMP);
   strcat(szUpdateFromSrcFolder, "tmpUpdate/");

   sprintf(szComm, "mkdir -p %s", szUpdateFromSrcFolder);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "chmod 777 %s", szUpdateFromSrcFolder);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "rm -rf %s/*", szUpdateFromSrcFolder);
   hw_execute_bash_command(szComm, NULL);

   bool bFoundZip = false;
   szZipFileName[0] = 0;
   szZipFullFilePath[0] = 0;
   bool bIsController = (bool) hardware_is_station();

   log_line("Executing update for %s...", bIsController?"station":"vehicle");

   if ( bIsController )
      sprintf(szComm, "find %sruby_update*.zip 2>/dev/null", FOLDER_USB_MOUNT);
   else
      sprintf(szComm, "find ruby_update*.zip 2>/dev/null");

   hw_execute_bash_command(szComm, szFoundFile);

   if ( (0 < strlen(szFoundFile)) && (NULL != strstr(szFoundFile, "ruby_update")) )
   {
      szFoundFile[MAX_FILE_PATH_SIZE-1] = 0;
      strcpy(szZipFullFilePath, szFoundFile);
      strcpy(szZipFileName, strstr(szZipFullFilePath, "ruby_update"));
      log_line("Found zip archive full path: [%s]", szZipFullFilePath);
      log_line("Found zip archive filename: [%s]", szZipFileName);

      sprintf(szComm, "mkdir -p %s", FOLDER_UPDATES);
      hw_execute_bash_command(szComm, NULL);

      sprintf(szComm, "chmod 777 %s", FOLDER_UPDATES);
      hw_execute_bash_command(szComm, NULL);
      
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %sbin/", FOLDER_UPDATES);
      hw_execute_bash_command(szComm, NULL);

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s %s", szZipFullFilePath, FOLDER_UPDATES);
      hw_execute_bash_command(szComm, NULL);

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "unzip %s%s -d %s", FOLDER_UPDATES, szZipFileName, szUpdateFromSrcFolder);
      hw_execute_bash_command(szComm, NULL);

      bFoundZip = true;
   }

   if ( ! bFoundZip )
   {
      if ( bIsController )
         log_line("There is no update update archive on the USB stick.");
      else
         log_line("There is no update update archive on the Ruby main folder.");

      _write_return_code(-1);
      return -1;
   }

   bool bValidZip = false;
   snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s%s", szUpdateFromSrcFolder, FILE_INFO_SHORT_LAST_UPDATE);
   if( access( szFile, R_OK ) != -1 )
   {
      log_line("Found update info file in zip file %s", szZipFileName);
      bValidZip = true;
   }
   if ( ! bValidZip )
   {
      char szOutput[4096];
      szOutput[0] = 0;
      sprintf(szComm, "ls %s", szUpdateFromSrcFolder);
      hw_execute_bash_command(szComm, szOutput);
      log_line("Content of tmp update folder:");
      log_line("[%s]", szOutput);
      log_line("Found zip archive with no valid update info file (missing update file: [%s]). Ignoring it.", szFile);
      _write_return_code(-10);
      return -1;
   }

   /*
   long lSizeBefore = compute_file_sizes();
   if ( -1 == lSizeBefore )
   {
      log_error_and_alarm("Failed to do update: can't get original binaries file sizes.");
      _write_return_code(-2);
      return -1;    
   }
   */

   g_TimeNow = get_current_timestamp_ms();

   char szSrcBinariesFolder[MAX_FILE_PATH_SIZE];
   strcpy(szSrcBinariesFolder, szUpdateFromSrcFolder);
   #ifdef HW_PLATFORM_RASPBERRY
   strcat(szSrcBinariesFolder, SUBFOLDER_UPDATES_PI);
   #endif
   #ifdef HW_PLATFORM_RADXA_ZERO3
   strcat(szSrcBinariesFolder, SUBFOLDER_UPDATES_RADXA);
   #endif
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   strcat(szSrcBinariesFolder, SUBFOLDER_UPDATES_OIPC);
   #endif

   // Check if ruby binaries are present in folder
   strcpy(szFile, szSrcBinariesFolder);
   strcat(szFile, "ruby_start");
   if ( access(szFile, R_OK) == -1 )
   {
      char szOutput[4096];
      szOutput[0] = 0;
      sprintf(szComm, "ls %s", szUpdateFromSrcFolder);
      hw_execute_bash_command(szComm, szOutput);
      log_line("Content of tmp update folder:");
      log_line("[%s]", szOutput);
      log_line("Found zip archive with no valid ruby_start file in binaries folder. Ignoring it.");
      _write_return_code(-10);
      return -1;
   }

   log_line("Copying update files from zip archive [%s], from unzipped folder [%s] ...", szZipFileName, szUpdateFromSrcFolder);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s%s %s%s 2>/dev/null", szUpdateFromSrcFolder, FILE_INFO_SHORT_LAST_UPDATE, FOLDER_CONFIG, FILE_INFO_LAST_UPDATE);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sres/* %sres/ 2>/dev/null", szUpdateFromSrcFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "mkdir -p %sbin/", FOLDER_UPDATES);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %sbin/", FOLDER_UPDATES);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sbin/* %sbin 2>/dev/null", szUpdateFromSrcFolder, FOLDER_UPDATES);
   hw_execute_bash_command(szComm, NULL);

   hardware_sleep_ms(50);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sruby_* %s", szSrcBinariesFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(50);

   // Begin Check and update drivers

   char szDriver[MAX_FILE_PATH_SIZE];
   strcpy(szDriver, szUpdateFromSrcFolder);
   #ifdef HW_PLATFORM_RASPBERRY
   strcat(szDriver, SUBFOLDER_UPDATES_PI);
   strcat(szDriver, "drivers/8812eu_pi.ko");
   #endif
   #ifdef HW_PLATFORM_RADXA_ZERO3
   strcat(szDriver, SUBFOLDER_UPDATES_RADXA);
   strcat(szDriver, "drivers/8812eu_radxa.ko");
   #endif
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   strcat(szDriver, SUBFOLDER_UPDATES_OIPC);
   strcat(szDriver, "drivers/8812eu.ko");
   #endif

   if ( access(szDriver, R_OK) != -1 )
   {
      #ifdef HW_PLATFORM_RADXA_ZERO3
      hw_execute_bash_command("sudo modprobe cfg80211", NULL);
      sprintf(szComm, "cp -rf %s /home/", szDriver);
      hw_execute_bash_command(szComm, NULL);
      sprintf(szComm, "cp -rf %s /lib/modules/$(uname -r)/kernel/drivers/net/wireless/", szDriver);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_bash_command("insmod /lib/modules/$(uname -r)/kernel/drivers/net/wireless/8812eu_radxa.ko rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0 2>&1 1>/dev/null", NULL);
      #endif
   }
   else
      log_line("No driver file found (%d)", szDriver);

   // End check and update drivers

   g_TimeNow = get_current_timestamp_ms();

   #ifdef HW_PLATFORM_RASPBERRY
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sraspi* %s", szSrcBinariesFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);
   if ( access( "ruby_capture_raspi", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_raspi /opt/vc/bin/raspivid", NULL);
   if ( access( "ruby_capture_veye", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_veye /usr/local/bin/veye_raspivid", NULL);
   if ( access( "ruby_capture_veye307", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_veye307 /usr/local/bin/307/veye_raspivid", NULL);
   #endif

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sstop* %s 2>/dev/null", szSrcBinariesFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %splugins/* %splugins/ 2>/dev/null", szSrcBinariesFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %splugins/osd/* %splugins/osd 2>/dev/null", szSrcBinariesFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);

   hw_execute_bash_command("chmod 777 ruby*", NULL);
   hw_execute_bash_command("chmod 777 plugins/*", NULL);
   hw_execute_bash_command("chmod 777 plugins/osd/*", NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_PI);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_RADXA);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_OIPC);
   hw_execute_bash_command(szComm, NULL);
   //hw_execute_bash_command("chmod 777 bin/linux/*", NULL);
   //hw_execute_bash_command("chmod 777 bin/windows/*", NULL);

   if ( access( "ruby_profile", R_OK ) != -1 )
   {
      #ifdef HW_PLATFORM_RASPBERRY
      hw_execute_bash_command("cp -rf ruby_profile /root/.profile", NULL);
      hw_execute_bash_command("chmod 777 /root/.profile", NULL);
      #endif
   }

   if ( access( "ruby_config.txt", R_OK ) != -1 )
   {
      #ifdef HW_PLATFORM_RASPBERRY
      hardware_mount_boot();
      hardware_sleep_ms(200);
      hw_execute_bash_command("cp -f ruby_config.txt /boot/config.txt", NULL);
      hw_execute_bash_command("chmod 777 /boot/config.txt", NULL);
      #endif
   }

   sprintf(szComm, "rm -rf %s", szUpdateFromSrcFolder);
   hw_execute_bash_command(szComm, NULL);

   process_custom_commands_file();

   /*
   long lSizeAfter = compute_file_sizes();
   if ( -1 == lSizeAfter )
   {
      log_error_and_alarm("Failed to do update: can't get updated binaries file sizes.");
      _write_return_code(-3);
      return -1;    
   }
   
   log_line("Files updated. Before files size: %d bytes, after files size: %d bytes", lSizeBefore, lSizeAfter);
   */

   if( access( "ruby_update", R_OK ) != -1 )
   {
      hw_execute_bash_command("./ruby_update -pre", NULL);
      if ( bIsController )
         hw_execute_bash_command("cp -rf ruby_update ruby_update_controller", NULL);
      hw_execute_bash_command("chmod 777 ruby_update*", NULL);
   }

   if ( ! bIsController )
      hw_execute_bash_command("rm -rf ruby_update*.zip", NULL);

   if ( bIsController )
      log_line("Update controller finished.");
   else
      log_line("Update vehicle finished.");

   /*
   if ( lSizeBefore == lSizeAfter )
   {
      log_error_and_alarm("Failed to do update: can't write files: files sizes before is the same as after.");
      _write_return_code(-4);
      return -1;    
   }
   */
   _write_return_code(0);
   return (0);
} 