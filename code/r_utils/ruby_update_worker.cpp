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
#include <ctype.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/launchers.h"
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
         snprintf(szCommand, sizeof(szCommand), "cp -rf %s %s/%s", szFileIn, szFolder, szFileOut);
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
         snprintf(szCommand, sizeof(szCommand), "mv -f %s %s/%s", szFileIn, szFolder, szFileOut);
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
         snprintf(szCommand, sizeof(szCommand), "%s", line);
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
   //snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_UPDATE_CMD_LIST );
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

   lSize = get_filesize("ruby_vehicle");
   if ( -1 == lSize )
      return -1;    

   lTotalSize += lSize;

   lSize = get_filesize("ruby_rt_station");
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

   lSize = get_filesize("ruby_rx_rc");
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

   char szFile[128];
   char szComm[256];
   char szFoundFile[1024];
   char szZipFile[128];
   char szUpdateFromSrcFolder[128];

   bool bFoundZip = false;
   szZipFile[0] = 0;
   szUpdateFromSrcFolder[0] = 0;

   bool bIsController = (bool) hardware_is_station();

   log_line("Executing update for %s...", bIsController?"station":"vehicle");

   if ( bIsController )
      snprintf(szComm, sizeof(szComm), "find %s/ruby_update*.zip 2>/dev/null", FOLDER_USB_MOUNT);
   else
      snprintf(szComm, sizeof(szComm), "find ruby_update*.zip 2>/dev/null");

   hw_execute_bash_command(szComm, szFoundFile);

   if ( 0 < strlen(szFoundFile) && NULL != strstr(szFoundFile, "ruby_update") )
   {
      szFoundFile[127] = 0;
      strcpy(szZipFile, szFoundFile);

      hw_execute_bash_command("mkdir -p updates", NULL);
      hw_execute_bash_command("chmod 777 updates", NULL);
      hw_execute_bash_command("rm -rf updates/*", NULL);
      snprintf(szComm, sizeof(szComm), "cp -rf %s updates/", szZipFile);
      hw_execute_bash_command(szComm, NULL);

      snprintf(szComm, sizeof(szComm), "mkdir -p %s/tmpUpdate", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      snprintf(szComm, sizeof(szComm), "chmod 777 %s/tmpUpdate", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      snprintf(szComm, sizeof(szComm), "unzip %s -d %s/tmpUpdate", szZipFile, FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);

      snprintf(szFile, sizeof(szFile), "%s/tmpUpdate/%s", FOLDER_RUBY_TEMP, FILE_INFO_LAST_UPDATE);
      if( access( szFile, R_OK ) != -1 )
      {
         log_line("Found update in zip file: %s", szZipFile);
         snprintf(szUpdateFromSrcFolder, sizeof(szUpdateFromSrcFolder), "%s/tmpUpdate", FOLDER_RUBY_TEMP);
         bFoundZip = true;
      }
      if ( ! bFoundZip )
         log_line("Found zip archive with no update (missing update file: [%s]). Ignoring it.", szFile);
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

   long lSizeBefore = compute_file_sizes();
   if ( -1 == lSizeBefore )
   {
      log_error_and_alarm("Failed to do update: can't get original binaries file sizes.");
      _write_return_code(-2);
      return -1;    
   }

   g_TimeNow = get_current_timestamp_ms();

   log_line("Copying update files from zip archive [%s], unpacked in folder [%s] ...", szZipFile, szUpdateFromSrcFolder);

   snprintf(szComm, sizeof(szComm), "cp -rf %s/res/* res/ 2>/dev/null", szUpdateFromSrcFolder);
   hw_execute_bash_command(szComm, NULL);

   hardware_sleep_ms(50);
   snprintf(szComm, sizeof(szComm), "cp -rf %s/ruby_* .", szUpdateFromSrcFolder);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(50);


   g_TimeNow = get_current_timestamp_ms();

   snprintf(szComm, sizeof(szComm), "cp -rf %s/raspi* .", szUpdateFromSrcFolder);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm), "cp -rf %s/stop* .  2>/dev/null", szUpdateFromSrcFolder);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm), "cp -rf %s/plugins/* plugins/ 2>/dev/null", szUpdateFromSrcFolder);
   hw_execute_bash_command(szComm, NULL);

   snprintf(szComm, sizeof(szComm), "cp -rf %s/plugins/osd/* plugins/osd 2>/dev/null", szUpdateFromSrcFolder);
   hw_execute_bash_command(szComm, NULL);

   hw_execute_bash_command("chmod 777 ruby*", NULL);
   hw_execute_bash_command("chmod 777 plugins/*", NULL);
   hw_execute_bash_command("chmod 777 plugins/osd/*", NULL);
   //hw_execute_bash_command("rename 's/ruby_lib/lib' *", NULL);

   if ( access( "ruby_capture_raspi", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_raspi /opt/vc/bin/raspivid", NULL);
   if ( access( "ruby_capture_veye", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_veye /usr/local/bin/veye_raspivid", NULL);
   if ( access( "ruby_capture_veye307", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_veye307 /usr/local/bin/307/veye_raspivid", NULL);

   if ( access( "ruby_profile", R_OK ) != -1 )
   {
      hw_execute_bash_command("cp -rf ruby_profile /root/.profile", NULL);
      hw_execute_bash_command("chmod 777 /root/.profile", NULL);
   }

   if ( access( "ruby_config.txt", R_OK ) != -1 )
   {
      hardware_mount_boot();
      hardware_sleep_ms(200);
      hw_execute_bash_command("cp -f ruby_config.txt /boot/config.txt", NULL);
      hw_execute_bash_command("chmod 777 /boot/config.txt", NULL);
   }

   snprintf(szComm, sizeof(szComm), "rm -rf %s/tmpUpdate", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);

   process_custom_commands_file();

   long lSizeAfter = compute_file_sizes();
   if ( -1 == lSizeAfter )
   {
      log_error_and_alarm("Failed to do update: can't get updated binaries file sizes.");
      _write_return_code(-3);
      return -1;    
   }

   log_line("Files updated. Before files size: %d bytes, after files size: %d bytes", lSizeBefore, lSizeAfter);

   if ( lSizeBefore == lSizeAfter )
   {
      log_error_and_alarm("Failed to do update: can't write files: files sizes before is the same as after.");
      _write_return_code(-4);
      return -1;    
   }
   
   if( access( "ruby_update", R_OK ) != -1 )
   {
      hw_execute_bash_command("./ruby_update -pre", NULL);
      if ( bIsController )
         hw_execute_bash_command("cp -rf ruby_update ruby_update_controller", NULL);
      hw_execute_bash_command("cp -rf ruby_update ruby_update_vehicle", NULL);
   }

   if ( ! bIsController )
      hw_execute_bash_command("rm -rf ruby_update*.zip", NULL);

   if ( bIsController )
      log_line("Update controller finished.");
   else
      log_line("Update vehicle finished.");
     
   _write_return_code(0);
   return (0);
} 