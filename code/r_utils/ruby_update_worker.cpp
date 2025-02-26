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
#include <ctype.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_files.h"
#include "../base/hw_procs.h"
#include "../base/radio_utils.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"


bool gbQuit = false;
bool g_bIsController = true;
char g_szUpdateZipFileFullPath[MAX_FILE_PATH_SIZE];
char g_szUpdateZipFileName[MAX_FILE_PATH_SIZE];
char g_szUpdateUnpackFolder[MAX_FILE_PATH_SIZE];

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
         while ( (len > 0) && (szCommand[len] == 10 || szCommand[len] == 13 || szCommand[len] == '\r' || szCommand[len] == '\t' || szCommand[len] == ' ' ) )
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

int _replace_runtime_binary_files()
{
   if ( (0 == g_szUpdateUnpackFolder[0]) )
   {
      log_line("Invalid function param for copy binary files.");
      _write_return_code(-10);
      return -1;
   }

   char szComm[MAX_FILE_PATH_SIZE];
   char szSrcBinariesFolder[MAX_FILE_PATH_SIZE];
   #ifdef HW_PLATFORM_RASPBERRY
   snprintf(szSrcBinariesFolder, sizeof(szSrcBinariesFolder)/sizeof(szSrcBinariesFolder[0]), "%s%s", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   #endif
   #ifdef HW_PLATFORM_RADXA_ZERO3
   snprintf(szSrcBinariesFolder, sizeof(szSrcBinariesFolder)/sizeof(szSrcBinariesFolder[0]), "%s%s", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_RADXA);
   #endif
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   snprintf(szSrcBinariesFolder, sizeof(szSrcBinariesFolder)/sizeof(szSrcBinariesFolder[0]), "%s%s", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_OIPC);
   #endif

   log_line("Check for binary files in unzipped folder [%s] ...", szSrcBinariesFolder);

   // Check if ruby binaries are present in folder
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, szSrcBinariesFolder);
   strcat(szFile, "ruby_start");
   if ( access(szFile, R_OK) == -1 )
   {
      char szOutput[4096];
      szOutput[0] = 0;
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "ls %s", g_szUpdateUnpackFolder);
      hw_execute_bash_command(szComm, szOutput);
      log_line("Content of tmp update folder:");
      log_line("[%s]", szOutput);
      log_line("Found zip archive with no valid ruby_start file in binaries folder. Ignoring it.");
      _write_return_code(-10);
      return -1;
   }

   log_line("Copying binary files from unzipped folder [%s] ...", szSrcBinariesFolder);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sruby_* %s", szSrcBinariesFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sstop* %s 2>/dev/null", szSrcBinariesFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(50);

   hw_execute_bash_command("chmod 777 ruby*", NULL);

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

   return 0;
}


int _copy_update_binary_files()
{
   if ( 0 == g_szUpdateUnpackFolder[0] )
   {
      log_line("Invalid function param for copy update binary files.");
      _write_return_code(-10);
      return -1;
   }

   char szComm[MAX_FILE_PATH_SIZE];

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %sbin/", FOLDER_UPDATES);
   hw_execute_bash_command(szComm, NULL);
   
   sprintf(szComm, "mkdir -p %sbin/", FOLDER_UPDATES);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_PI);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_RADXA);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_OIPC);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sbin/* %sbin 2>/dev/null", g_szUpdateUnpackFolder, FOLDER_UPDATES);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_PI);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_RADXA);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_OIPC);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s%s*", FOLDER_UPDATES, SUBFOLDER_UPDATES_DRIVERS);
   hw_execute_bash_command(szComm, NULL);

   hardware_sleep_ms(50);

   return 0;
}


int _copy_res_files()
{
   if ( 0 == g_szUpdateUnpackFolder[0] )
   {
      log_line("Invalid function param for copy res files.");
      _write_return_code(-10);
      return -1;
   }

   char szComm[256];

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s%s %s%s 2>/dev/null", g_szUpdateUnpackFolder, FILE_INFO_SHORT_LAST_UPDATE, FOLDER_CONFIG, FILE_INFO_LAST_UPDATE);
   hw_execute_bash_command(szComm, NULL);


   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %sres/* %sres/ 2>/dev/null", g_szUpdateUnpackFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);

   return 0;
}

int _copy_plugin_files()
{
   if ( 0 == g_szUpdateUnpackFolder[0] )
   {
      log_line("Invalid function param for copy plugin files.");
      _write_return_code(-10);
      return -1;
   }

   char szComm[MAX_FILE_PATH_SIZE];
   char szSrcPluginsFolder[MAX_FILE_PATH_SIZE];

   #ifdef HW_PLATFORM_RASPBERRY
   snprintf(szSrcPluginsFolder, sizeof(szSrcPluginsFolder)/sizeof(szSrcPluginsFolder[0]), "%s%splugins/", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);
   #endif
   #ifdef HW_PLATFORM_RADXA_ZERO3
   snprintf(szSrcPluginsFolder, sizeof(szSrcPluginsFolder)/sizeof(szSrcPluginsFolder[0]), "%s%splugins/", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_RADXA);
   #endif
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   snprintf(szSrcPluginsFolder, sizeof(szSrcPluginsFolder)/sizeof(szSrcPluginsFolder[0]), "%s%splugins/", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_OIPC);
   #endif

   log_line("Copying plugins files from source folder: (%s)", szSrcPluginsFolder);

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s* %splugins/ 2>/dev/null", szSrcPluginsFolder, FOLDER_BINARIES);
   hw_execute_bash_command(szComm, NULL);

   hw_execute_bash_command("chmod 777 plugins/*", NULL);
   hw_execute_bash_command("chmod 777 plugins/osd/*", NULL);
   hw_execute_bash_command("chmod 777 plugins/core/*", NULL);
   return 0;
}


int _copy_config_files()
{
   if ( 0 == g_szUpdateUnpackFolder[0] )
   {
      log_line("Invalid function param for copy config files.");
      _write_return_code(-10);
      return -1;
   }
   #ifdef HW_PLATFORM_RASPBERRY
   char szSourceFile[MAX_FILE_PATH_SIZE];
   snprintf(szSourceFile, sizeof(szSourceFile)/sizeof(szSourceFile[0]), "%s%sruby_profile", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);

   if ( access( szSourceFile, R_OK ) != -1 )
   {
      char szComm[MAX_FILE_PATH_SIZE];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /root/.profile 2>/dev/null", szSourceFile);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_bash_command("chmod 777 /root/.profile", NULL);
   }

   snprintf(szSourceFile, sizeof(szSourceFile)/sizeof(szSourceFile[0]), "%s%sruby_config.txt", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_PI);

   if ( access( szSourceFile, R_OK ) != -1 )
   {
      hardware_mount_boot();
      hardware_sleep_ms(200);
      char szComm[MAX_FILE_PATH_SIZE];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s /boot/config.txt 2>/dev/null", szSourceFile);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_bash_command("chmod 777 /boot/config.txt", NULL);
   }
   #endif
   return 0;
}


int _copy_update_drivers()
{
   if ( 0 == g_szUpdateUnpackFolder[0] )
   {
      log_line("Invalid function param for copy update drivers files.");
      _write_return_code(-10);
      return -1;
   }

   char szDrivers[MAX_FILE_PATH_SIZE];

   snprintf(szDrivers, sizeof(szDrivers)/sizeof(szDrivers[0]), "%s%s*", g_szUpdateUnpackFolder, SUBFOLDER_UPDATES_DRIVERS);
   
   char szComm[MAX_FILE_PATH_SIZE];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mkdir -p %s", FOLDER_DRIVERS);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s %s 2>/dev/null", szDrivers, FOLDER_DRIVERS);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s*", FOLDER_DRIVERS);
   hw_execute_bash_command(szComm, NULL);

   // Drivers are installed after reboot, by the presence of ruby_update_controller
   //char szOutput[2048];
   //szOutput[0] = 0;
   //snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "find %s.ko 2>/dev/null", szDrivers);
   //hw_execute_bash_command(szComm, szOutput);
   //if ( (0 < strlen(szOutput)) && (NULL != strstr(szOutput, ".ko")) )
   //   hardware_install_drivers(0);
   return 0;
}

bool _find_update_zip_file()
{
   char szComm[MAX_FILE_PATH_SIZE];
   char szOutput[1024];

   g_szUpdateZipFileFullPath[0] = 0;
   g_szUpdateZipFileName[0] = 0;

   if ( g_bIsController )
      sprintf(szComm, "find %sruby_update*.zip 2>/dev/null", FOLDER_USB_MOUNT);
   else
      sprintf(szComm, "find ruby_update*.zip 2>/dev/null");

   hw_execute_bash_command(szComm, szOutput);

   if ( (0 == strlen(szOutput)) || (NULL == strstr(szOutput, "ruby_update")) )
      return false;

   int iLen = strlen(szOutput);
   for( int i=0; i<iLen; i++ )
   {
      if ( (szOutput[i] == ' ') || (szOutput[i] == 10) || (szOutput[i] == 13) )
      {
         szOutput[i] = 0;
         break;
      }
   }
   strncpy(g_szUpdateZipFileFullPath, szOutput, MAX_FILE_PATH_SIZE-1);
   strncpy(g_szUpdateZipFileName, strstr(szOutput, "ruby_update"), MAX_FILE_PATH_SIZE-1);

   log_line("Found zip archive full path: [%s]", g_szUpdateZipFileFullPath);
   log_line("Found zip archive filename: [%s]", g_szUpdateZipFileName);
   return true;
}

void _step_copy_and_extract_zip()
{
   char szComm[MAX_FILE_PATH_SIZE];

   //-----------------------------------------------------
   // Begin - Copy update zip file to updates folder

   sprintf(szComm, "mkdir -p %s", FOLDER_UPDATES);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "chmod 777 %s", FOLDER_UPDATES);
   hw_execute_bash_command(szComm, NULL);
   
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cp -rf %s %s", g_szUpdateZipFileFullPath, FOLDER_UPDATES);
   hw_execute_bash_command(szComm, NULL);

   // End - Copy update zip file to updates folder
   //------------------------------------------------------

   //------------------------------------------------------
   // Begin - Extract archive to a temp folder
   
   strcpy(g_szUpdateUnpackFolder, FOLDER_RUBY_TEMP);
   strcat(g_szUpdateUnpackFolder, "tmpUpdate/");

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mkdir -p %s", g_szUpdateUnpackFolder);
   hw_execute_bash_command(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s", g_szUpdateUnpackFolder);
   hw_execute_bash_command(szComm, NULL);
   if ( 0 < strlen(g_szUpdateUnpackFolder) )
   {
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s/*", g_szUpdateUnpackFolder);
      hw_execute_bash_command(szComm, NULL);
   }
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "unzip %s%s -d %s", FOLDER_UPDATES, g_szUpdateZipFileName, g_szUpdateUnpackFolder);
   hw_execute_bash_command(szComm, NULL);

   // End - Extract archive to a temp folder
   //------------------------------------------------------
}

bool _find_update_info_file()
{
   char szFile[MAX_FILE_PATH_SIZE];
   char szComm[MAX_FILE_PATH_SIZE];

   snprintf(szFile, sizeof(szFile)/sizeof(szFile[0]), "%s%s", g_szUpdateUnpackFolder, FILE_INFO_SHORT_LAST_UPDATE);
   if( access( szFile, R_OK ) == -1 )
   {
      char szOutput[4096];
      szOutput[0] = 0;
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "ls %s", g_szUpdateUnpackFolder);
      hw_execute_bash_command(szComm, szOutput);
      log_line("Content of tmp update folder:");
      log_line("[%s]", szOutput);
      log_line("Found zip archive with no valid update info file (missing update file: [%s]). Ignoring it.", szFile);
      _write_return_code(-10);
      return false;
   }
   log_line("Found update info file in zip update (%s)", g_szUpdateZipFileName);
   return true;
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

   char szComm[MAX_FILE_PATH_SIZE];

   g_szUpdateZipFileFullPath[0] = 0;
   g_szUpdateZipFileName[0] = 0;
   g_szUpdateUnpackFolder[0] = 0;

   g_bIsController = (bool) hardware_is_station();
   log_line("Executing update for %s...", g_bIsController?"controller":"vehicle");

   if ( ! _find_update_zip_file() )
   {
      if ( g_bIsController )
         log_line("There is no update update archive on the USB stick.");
      else
         log_line("There is no update update archive on the Ruby main folder.");

      _write_return_code(-1);
      return -1;
   }

   _step_copy_and_extract_zip();
  
   if ( ! _find_update_info_file() )
      return -1;


   g_TimeNow = get_current_timestamp_ms();

   if ( _replace_runtime_binary_files() < 0 )
      return -1;

   if ( _copy_update_binary_files() < 0 )
      return -1;

   if ( _copy_res_files() < 0 )
      return -1;

   _copy_plugin_files();

   _copy_config_files();

   _copy_update_drivers();


   g_TimeNow = get_current_timestamp_ms();

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s", g_szUpdateUnpackFolder);
   hw_execute_bash_command(szComm, NULL);

   process_custom_commands_file();


   if( access( "ruby_update", R_OK ) != -1 )
   {
      hw_execute_bash_command("./ruby_update -pre", NULL);
      if ( g_bIsController )
         hw_execute_bash_command("cp -rf ruby_update ruby_update_controller", NULL);
      hw_execute_bash_command("chmod 777 ruby_update*", NULL);
   }

   if ( ! g_bIsController )
      hw_execute_bash_command("rm -rf ruby_update*.zip", NULL);

   if ( g_bIsController )
      log_line("Update controller finished.");
   else
      log_line("Update vehicle finished.");

   _write_return_code(0);
   return (0);
} 