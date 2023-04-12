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
#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_serial.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/hw_procs.h"
#include "../base/launchers.h"
#include "../base/models.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../common/string_utils.h"


void write_result(bool bSucceeded)
{
   FILE* fd = fopen(FILE_TMP_SIK_CONFIG_FINISHED, "wb");
   if ( NULL != fd )
   {
      fprintf(fd, "%d\n", (int)bSucceeded);
      fclose(fd);
   }
}

int main(int argc, char *argv[])
{
   char szBuff[256];
   sprintf(szBuff, "rm -rf %s", FILE_TMP_SIK_CONFIG_FINISHED);
   hw_execute_bash_command(szBuff, NULL);

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      write_result(false);
      return 0;
   }
   
   if ( argc < 5 )
   {
      printf("Usage: ruby_sik_config [serial_port] [baudrate]  (-show all / -serialspeed [speed] / -freq [freq])\n");
      write_result(false);
      return 0;
   }

   log_init("RubySiKConfig");

   hardware_reload_serial_ports();
   hardware_enumerate_radio_interfaces();
   hardware_radio_sik_load_configuration();

   bool bFailed = false;
   bool bMustSaveFlash = false;
   bool bSaveSerialPorts = false;
   bool bSaveRadioConfig = false;
   char szCommand[256];
   char szSerialPort[256];
   int iSerialSpeed = 0;
   int iParam1 = 0;

   strcpy(szSerialPort, argv[1]);
   iSerialSpeed = atoi(argv[2]);

   strcpy(szCommand, argv[3]);
   iParam1 = atoi(argv[4]);

   if ( 0 == strcmp(szCommand, "-show") )
      log_enable_stdout();

   int iSerialPort = hardware_open_serial_port(szSerialPort, iSerialSpeed);
   if ( iSerialPort <= 0 )
   {
      log_error_and_alarm("Failed to open serial port for SiK radio %s at %d bps.", szSerialPort, iSerialSpeed);
      write_result(false);
      return 0;
   }

   // Enter AT mode
   
   if ( ! hardware_serial_send_sik_command(iSerialPort, "+++") )
   {
      log_softerror_and_alarm("Failed to send to SiK radio command mode command.");
      close(iSerialPort);
      write_result(false);
      return 0;
   }
  
   log_line("Sent +++ AT mode command. Waiting for response.");

   u8 bufferResponse[1024];
   int iBufferSize = 1024;
   
   int iLen = hardware_serial_wait_sik_response(iSerialPort, 2500, 1, bufferResponse, &iBufferSize);
   if ( iLen <= 2 )
   {
      if ( iLen >= 0 )
         bufferResponse[iLen] = 0;
      else
         bufferResponse[0] = 0;
      log_softerror_and_alarm("Failed to enter SiK radio into command mode. Received response (%d bytes): [%s]", iLen, (char*)bufferResponse);
      close(iSerialPort);
      write_result(false);
      return 0;
   }

   if ( NULL != strstr((const char*)bufferResponse, "+++"))
   {
      log_line("Already in AT command mode.");
      // It's already in AT command mode;
   }
   else if ( NULL == strstr((const char*)bufferResponse, "OK") )
   {
      log_softerror_and_alarm("Failed to set SiK radio into command mode. Received response (%d bytes): [%s]", iLen, (char*)bufferResponse);
      close(iSerialPort);
      write_result(false);
      return 0;
   }
   else
   {
      log_line("Entered AT command mode.");
      // Entered AT command mode;
   }

   // Send params

   char szComm[32];
   
   if ( 0 == strcmp(szCommand, "-show") )
   {
      if ( hardware_radio_sik_send_command(iSerialPort, "ATI", bufferResponse, 255) )
      {
         bufferResponse[62] = 0;
         log_line("ATI: [%s]", (char*)bufferResponse);
      }
      else
         log_softerror_and_alarm("Failed to send ATI command.");

      int iMaxParam = 15;
      if ( iMaxParam > MAX_RADIO_HW_PARAMS )
         iMaxParam = MAX_RADIO_HW_PARAMS;

      for( int i=0; i<iMaxParam; i++ )
      {
         char szComm[32];
         sprintf(szComm, "ATS%d?", i);
         if ( hardware_radio_sik_send_command(iSerialPort, szComm, bufferResponse, 255) )
         {
            u32 uParam = atoi((char*)bufferResponse);
            log_line("DEBUG: SiK %s = %u", szComm, uParam);
         }
         else
            log_softerror_and_alarm("Failed to get param: %s", szComm); 
      }
   }

   if ( 0 == strcmp(szCommand, "-serialspeed") )
   {
      sprintf(szComm, "ATS%d=%d", 1, iParam1);

      if ( hardware_radio_sik_send_command(iSerialPort, szComm, bufferResponse, 255) )
      {
         bMustSaveFlash = true;
         log_line("DEBUG: Set SiK %s to %d, response: [%s]", szComm, iParam1, bufferResponse);
         hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(szSerialPort);
         if ( NULL != pSerialPort )
         {
            pSerialPort->lPortSpeed = (long) iParam1;
            bSaveSerialPorts = true;
         }
         else
         {
             log_softerror_and_alarm("Failed to get serial port hardware info for serial port [%s].", szSerialPort);
             bFailed = true;
         }

         radio_hw_info_t* pRadioInfo = hardware_radio_sik_get_from_serial_port(szSerialPort);
         if ( NULL != pRadioInfo )
         {
            pRadioInfo->uHardwareParamsList[1] = (u32)iParam1;
            bSaveRadioConfig = true;
         }
         else
         {
            log_softerror_and_alarm("Failed to get SiK radio info for serial port [%s].", szSerialPort);
            bFailed = true;
         }
      }
      else
      {
         log_softerror_and_alarm("Failed to set SiK %s to %d, response: [%s]", szComm, iParam1, bufferResponse);
         bFailed = true;
      }
   }

   // Exit AT command mode
   //hardware_radio_sik_send_command(iSerialPort, "ATO", bufferResponse, 255);
   //log_line("Send command to exit AT command mode.");
   
   if ( bMustSaveFlash )
   {
      if ( hardware_radio_sik_send_command(iSerialPort, "AT&W", bufferResponse, 255) )
         log_line("Wrote SiK parameters to flash, response: [%s]", bufferResponse);
      else
         log_softerror_and_alarm("Failed to write SiK parameters to flash, response: [%s]", bufferResponse);

      if ( hardware_radio_sik_send_command(iSerialPort, "ATZ", bufferResponse, 255) )
         log_line("Restart SiK radio, response: [%s]", bufferResponse);
      else
         log_softerror_and_alarm("Failed to restart SiK radio, response: [%s]", bufferResponse);
   }


   close(iSerialPort);

   if ( bSaveSerialPorts )
      hardware_serial_save_configuration();

   if ( bSaveRadioConfig )
   {
      hardware_radio_sik_save_configuration();
      hardware_save_radio_info();
   }

   if ( ! bFailed )
   {
      log_line("Ruby SiK configuration completed without errors.");
      write_result(false);
      return 0;
   }

   log_line("Ruby SiK configuration completed with errors.");
   write_result(true);
   return 0;
} 