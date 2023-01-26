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
#include "../base/ctrl_settings.h"
#include "../base/hw_procs.h"

#include "forward_watch.h"
#include "shared_vars.h"
#include "warnings.h"
#include "ruby_central.h"
#include "timers.h"

bool s_bForwardPairingStarted = false;
bool s_bForwardHasUSBDevice = false;

u32 s_ForwardLastTimeCheck = 0;

void _forward_on_usb_device_detected()
{
   log_line("USB device detected");

   ruby_pause_watchdog();

   char szSysType[128];
   char szBuff[1024];
   char szOutput[2048];

   if ( hardware_is_vehicle() )
      strcpy(szSysType, "VEHICLE");
   else
      strcpy(szSysType, "STATION");

   hw_stop_process("pump");

   sprintf(szBuff, "pump -i usb0 --no-ntp -h Ruby%s_FWD", szSysType);
   //sprintf(szBuff, "dhclient usb0", szSysType);
   hw_execute_bash_command_raw(szBuff, NULL);

   hardware_sleep_ms(200);
   szOutput[0] = 0;
   hw_execute_bash_command_raw("ip route show 0.0.0.0/0 dev usb0", szOutput);
   //log_line("Output: [%s]", szOutput);
   int len = strlen(szOutput);
   char szIP[32];
   szIP[0] = 0;
   if ( 0 < len )
   {
      char* pIP = szOutput;
      while ( ((*pIP) != 0 ) && ((*pIP) < '0' || (*pIP) > '9') )
         pIP++;
      if ( *pIP != 0 )
         strcpy(szIP, pIP);
      if ( strlen(szIP) > 0 )
      {
         pIP = &szIP[0];
         while ( (*pIP) == '.' || (*pIP >= '0' && (*pIP) <= '9') )
            pIP++;
         *pIP = 0;
      }   
   }
   log_line("USB Device IP Address: %s", szIP);

   bool bHasETH = hardware_has_eth();

   if ( ! bHasETH )
      log_line("No ETH port. No further action on USB tethering start.");
   else if ( ( access( "/boot/nodhcp", R_OK ) != -1 ) )
      log_line("DHCP is disabled. No further action on USB tethering start.");
   else
   {
      log_line("DHCP is enabled. Reneable the ETH routing on USB tethering start.");
      sprintf(szBuff, "nice pump -i eth0 --no-ntp -h Ruby%s &", szSysType);
      hw_execute_bash_command(szBuff, NULL);
   }

   
   sprintf(szBuff, "touch %s", TEMP_USB_TETHERING_DEVICE);
   hw_execute_bash_command(szBuff, NULL);
   sprintf(szBuff, "echo \"%s\" > %s", szIP, TEMP_USB_TETHERING_DEVICE);
   hw_execute_bash_command(szBuff, NULL);
   log_line("Done configuring USB tethering device connection.");
   ruby_resume_watchdog();
}

void _forward_on_usb_device_unplugged()
{
   char szSysType[128];
   char szBuff[1024];

   ruby_pause_watchdog();

   if ( hardware_is_vehicle() )
      strcpy(szSysType, "VEHICLE");
   else
      strcpy(szSysType, "STATION");

   bool bHasETH = hardware_has_eth();
  
   hw_stop_process("pump");

   if ( ! bHasETH )
      log_line("No ETH port. No further action on USB tethering stop.");
   else if ( ( access( "/boot/nodhcp", R_OK ) != -1 ) )
      log_line("DHCP is disabled. No further action on USB tethering stop.");
   else
   {
      log_line("DHCP is enabled. Reneable the ETH routing on USB tethering stop.");
      sprintf(szBuff, "nice pump -i eth0 --no-ntp -h Ruby%s &", szSysType);
      hw_execute_bash_command(szBuff, NULL);
   }
   ruby_resume_watchdog();
}

bool forward_streams_on_pairing_start()
{
   s_bForwardPairingStarted = true;
   s_bForwardHasUSBDevice = false;
   return true;
}

bool forward_streams_on_pairing_stop()
{
   s_bForwardPairingStarted = false;
   s_bForwardHasUSBDevice = false;
   return true;
}

bool forward_streams_is_USB_device_connected()
{
   return s_bForwardHasUSBDevice;
}

void forward_streams_loop()
{
   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
      return;


   if ( g_TimeNow > s_ForwardLastTimeCheck + 1000 )
   {
      s_ForwardLastTimeCheck = g_TimeNow;

      if ( ! s_bForwardHasUSBDevice )
      {
         if ( access("/sys/class/net/usb0", R_OK ) != -1 )
         {
            warnings_add("USB tethering device detected. Configuring, please wait...");
            render_all(g_TimeNow);
            hardware_sleep_ms(50);
            render_all(g_TimeNow);
            _forward_on_usb_device_detected();
            if ( pCS->iVideoForwardUSBType != 0 )
               warnings_add("Started Video Forward to USB.");
            s_bForwardHasUSBDevice = true;
         }
      }
      else
      {
         if ( access("/sys/class/net/usb0", R_OK ) == -1 )
         {
            warnings_add("USB tethering device unplugged. Please wait...");
            render_all(g_TimeNow);
            hardware_sleep_ms(50);
            render_all(g_TimeNow);
            if ( pCS->iVideoForwardUSBType != 0 )
               warnings_add("Stopped Video Forward to USB.");
            log_line("USB tethering device unplugged.");
            _forward_on_usb_device_unplugged();
            char szBuff[256];
            sprintf(szBuff, "rm -rf %s", TEMP_USB_TETHERING_DEVICE);
            hw_execute_bash_command(szBuff, NULL);
            s_bForwardHasUSBDevice = false;
         }
      }
   }
}

