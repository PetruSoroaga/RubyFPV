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

#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hw_procs.h"
#include "controller_utils.h"

u32 controller_utils_getControllerId()
{
   bool bControllerIdOk = false;
   u32 uControllerId = 0;

   FILE* fd = fopen(FILE_CONTROLLER_ID, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%u", &uControllerId) )
         bControllerIdOk = false;
      else
         bControllerIdOk = true;
      fclose(fd);
   }

   if ( 0 == uControllerId || MAX_U32 == uControllerId )
      bControllerIdOk = false;

   if ( bControllerIdOk )
      return uControllerId;

   log_line("Generating a new unique controller ID...");

   uControllerId = rand();
   fd = fopen("/sys/firmware/devicetree/base/serial-number", "r");
   if ( NULL != fd )
   {
      char szBuff[256];
      szBuff[0] = 0;
      if ( 1 == fscanf(fd, "%s", szBuff) && 4 < strlen(szBuff) )
      {
         //log_line("Serial ID of HW: %s", szBuff);
         uControllerId += szBuff[strlen(szBuff)-1] + 256 * szBuff[strlen(szBuff)-2];
      }
      fclose(fd);
   }

   log_line("Generated a new unique controller ID: %u", uControllerId);

   fd = fopen(FILE_CONTROLLER_ID, "w");
   if ( NULL != fd )
   {
      fprintf(fd, "%u\n", uControllerId);
      fclose(fd);
   }
   else
      log_softerror_and_alarm("Failed to save the new generated unique controller ID!");

   return uControllerId;
}


int controller_utils_export_all_to_usb()
{
   char szOutput[1024];
   char szComm[1024];
   char szBuff[512];
   char szOutFile[256];

   hw_execute_bash_command_raw("which zip", szOutput);
   if ( 0 == strlen(szOutput) || NULL == strstr(szOutput, "zip") )
      return -1;

   if ( ! hardware_try_mount_usb() )
      return -2;

   u32 uControllerId = controller_utils_getControllerId();
   sprintf(szOutFile, "ruby_export_all_controller_%u.zip", uControllerId);

   hw_execute_bash_command("mkdir -p tmp/importexport", NULL);
   hw_execute_bash_command("chmod 777 tmp/importexport", NULL);

   sprintf(szComm, "rm -rf tmp/importexport/%s 2>/dev/null", szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "zip tmp/importexport/%s config/*", szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "zip -r tmp/importexport/%s /boot/config.txt", szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "chmod 777 tmp/importexport/%s", szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szBuff, "%s/Ruby_Exports", FOLDER_USB_MOUNT);
   sprintf(szComm, "mkdir -p %s", szBuff );
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "rm -rf %s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "%s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szOutFile);
   if( access( szComm, R_OK ) != -1 )
   {
      hardware_unmount_usb();
      hw_execute_bash_command("rm -rf tmp/importexport", NULL);
      sync();
      return -10;
   }

   sprintf(szComm, "cp -rf tmp/importexport/%s %s/Ruby_Exports/%s", szOutFile, FOLDER_USB_MOUNT, szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "chmod 777 %s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "%s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szOutFile);
   if( access( szComm, R_OK ) == -1 )
   {
      hardware_unmount_usb();
      hw_execute_bash_command("rm -rf tmp/importexport", NULL);
      sync();
      return -11;
   }

   hardware_unmount_usb();
   hw_execute_bash_command("rm -rf tmp/importexport", NULL);
   sync();

   return 0;
}

int controller_utils_import_all_from_usb(bool bImportAnyFound)
{
   char szOutput[1024];
   char szComm[1024];
   char szUSBFile[512];
   char szInFile[256];

   hw_execute_bash_command_raw("which zip", szOutput);
   if ( 0 == strlen(szOutput) || NULL == strstr(szOutput, "zip") )
      return -1;

   if ( ! hardware_try_mount_usb() )
      return -2;

   u32 uControllerId = controller_utils_getControllerId();
   sprintf(szInFile, "ruby_export_all_controller_%u.zip", uControllerId);
   snprintf(szUSBFile, 511, "%s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szInFile);

   if( access( szUSBFile, R_OK ) == -1 )
   {
      if ( bImportAnyFound )
      {
         sprintf(szInFile, "%s/Ruby_Exports/ruby_export_all_controller_*", FOLDER_USB_MOUNT);
         sprintf(szComm, "find %s 2>/dev/null", szInFile);
         szOutput[0] = 0;
         hw_execute_bash_command(szComm, szOutput);
         if ( (0 != szOutput[0]) && (NULL != strstr(szOutput, "ruby_export_all_controller_") ) )
         {
            int len = strlen(szOutput);
            for( int i=0; i<len; i++ )
               if ( szOutput[i] == 10 || szOutput[i] == 13 )
                  szOutput[i] = 0;

            strcpy(szInFile, szOutput);
            snprintf(szUSBFile, 511, "%s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szInFile);
         }
         if( access( szUSBFile, R_OK ) == -1 )
         {
            log_line("Can't access the import archive on USB stick: [%s]", szInFile);
            hardware_unmount_usb();
            return -3;
         }
      }
      else
      {
         log_line("Can't find the import archive on USB stick: [%s]", szInFile);
         hardware_unmount_usb();
         return -3;
      }
   }

   hw_execute_bash_command("mkdir -p tmp/importexport", NULL);
   hw_execute_bash_command("chmod 777 tmp/importexport", NULL);

   sprintf(szComm, "rm -rf tmp/importexport/%s 2>/dev/null", szInFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "cp -rf %s tmp/importexport/%s", szUSBFile, szInFile);
   hw_execute_bash_command(szComm, NULL);

   hw_execute_bash_command("rm -rf config/*", NULL);

   sprintf(szComm, "unzip tmp/importexport/%s", szInFile);
   hw_execute_bash_command(szComm, NULL);

   hw_execute_bash_command("cp -rf boot/config.txt /boot/config.txt", NULL);
   hw_execute_bash_command("rm -rf boot/config.txt", NULL);

   sprintf(szComm, "rm -rf tmp/importexport/%s", szInFile);
   hw_execute_bash_command(szComm, NULL);

   hardware_unmount_usb();
   sync();

   return 0;
}

bool controller_utils_usb_import_has_matching_controller_id_file()
{
   if ( ! hardware_try_mount_usb() )
      return false;

   u32 uControllerId = controller_utils_getControllerId();

   char szOutput[2048];
   char szFile[256];
   char szComm[1024];
   szOutput[0] = 0;
   sprintf(szFile, "%s/Ruby_Exports/ruby_export_all_controller_%u.zip", FOLDER_USB_MOUNT, uControllerId);
   sprintf(szComm, "find %s 2>/dev/null", szFile);
   hw_execute_bash_command(szComm, szOutput);

   hardware_unmount_usb();

   if ( 0 != szOutput[0] )
   if (  NULL != strstr(szOutput, "ruby_export_all_controller") )
   if (  NULL != strstr(szOutput, ".zip") )
      return true;

   return false;
}

bool controller_utils_usb_import_has_any_controller_id_file()
{
   if ( ! hardware_try_mount_usb() )
      return false;

   char szOutput[2048];
   char szFile[256];
   char szComm[1024];
   szOutput[0] = 0;
   sprintf(szFile, "%s/Ruby_Exports/ruby_export_all_controller_*", FOLDER_USB_MOUNT);
   sprintf(szComm, "find %s 2>/dev/null", szFile);
   hw_execute_bash_command(szComm, szOutput);

   hardware_unmount_usb();

   if ( 0 != szOutput[0] )
   if (  NULL != strstr(szOutput, "ruby_export_all_controller") )
   if (  NULL != strstr(szOutput, ".zip") )
      return true;

   return false;
}
