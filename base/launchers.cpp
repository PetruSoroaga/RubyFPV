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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "launchers.h"
#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hw_procs.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"

bool launch_set_frequency(Model* pModel, int nicIndex, int frequency)
{
   if ( frequency <= 0 )
   {
      log_softerror_and_alarm("Skipping setting card (%d) to invalid frequency 0 Mhz", nicIndex);
      return false;
   }

   char szInfo[64];

   u32 delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   if ( hardware_is_station() )
   {
      Preferences* pP = get_Preferences();
      if ( NULL != pP )
         delayMs = (u32) pP->iDebugWiFiChangeDelay;
      if ( delayMs<1 || delayMs > 200 )
         delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   }
   else if ( NULL != pModel )
      delayMs = (pModel->uDeveloperFlags >> 8) & 0xFF; 

   int iStartIndex = 0;
   int iEndIndex = hardware_get_radio_interfaces_count()-1;

   if ( -1 == nicIndex )
   {
      log_line("Setting all radio interfaces to frequency %d Mhz (guard interval: %d ms)", frequency, (int)delayMs);
      strcpy(szInfo, "all radio interfaces");
   }
   else
   {
      radio_hw_info_t* pNIC2 = hardware_get_radio_info(nicIndex);
      log_line("Setting radio interface %d (%s, %s) to frequency %d Mhz (guard interval: %d ms)", nicIndex+1, pNIC2->szName, str_get_radio_driver_description(pNIC2->typeAndDriver), frequency, (int)delayMs);
      sprintf(szInfo, "radio interface %d (%s, %s)", nicIndex+1, pNIC2->szName, str_get_radio_driver_description(pNIC2->typeAndDriver));
      iStartIndex = nicIndex;
      iEndIndex = nicIndex;
   }

   char cmd[128];
   char szOutput[512];
   bool failed = false;
   bool anySucceeded = false;

   for( int i=iStartIndex; i<=iEndIndex; i++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(i);
      if ( NULL == pNIC || (0 == hardware_radioindex_supports_frequency(i, frequency)) )
      {
         log_line("Radio interface %d (%s, %s) does not support %d Mhz. Skipping it.", i+1, pNIC->szName, str_get_radio_driver_description(pNIC->typeAndDriver), frequency);
         pNIC->lastFrequencySetFailed = 1;
         pNIC->failedFrequency = frequency;
         failed = true;
         continue;
      }

      bool bTryHT40 = false;
      bool bUsedHT40 = false;
      szOutput[0] = 0;

      // pModel parameter is always null on controller, do not check for HT40
      // pModel parameter is always valid on vehicle;
      if ( NULL != pModel )
      {
         int iRadioLinkId = pModel->radioInterfacesParams.interface_link_id[i];
         if ( iRadioLinkId >= 0 && iRadioLinkId < pModel->radioLinksParams.links_count )
         {
            if ( pModel->radioLinksParams.link_radio_flags[iRadioLinkId] & RADIO_FLAGS_HT40 )
               bTryHT40 = true;
         }
      }

      if ( bTryHT40 )
      {
         if ( (pNIC->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS )
         {
            sprintf(cmd, "iw dev %s set freq %d HT40+ 2>&1", pNIC->szName, frequency);
            bUsedHT40 = true;
         }
         else
         {
            sprintf(cmd, "iw dev %s set freq %d HT40+ 2>&1", pNIC->szName, frequency);
            bUsedHT40 = true;
         }
      }
      else
         sprintf(cmd, "iw dev %s set freq %d 2>&1", pNIC->szName, frequency);

      hw_execute_bash_command_raw(cmd, szOutput);

      if ( NULL != strstr( szOutput, "Invalid argument" ) )
      if ( bUsedHT40 )
      {
         int len = strlen(szOutput);
         for( int i=0; i<len; i++ )
            if ( szOutput[i] == 10 || szOutput[i] == 13 )
               szOutput[i] = '.';

         log_softerror_and_alarm("Failed to switch radio interface %d (%s, %s) to frequency %d Mhz in HT40 mode, returned error: [%s]. Retry operation.", i+1, pNIC->szName, str_get_radio_driver_description(pNIC->typeAndDriver), frequency, szOutput);
         hardware_sleep_ms(delayMs);
         szOutput[0] = 0;
         sprintf(cmd, "iw dev %s set freq %d 2>&1", pNIC->szName, frequency);
         hw_execute_bash_command_raw(cmd, szOutput);
      }

      if ( NULL != strstr( szOutput, "failed" ) )
      {
         pNIC->lastFrequencySetFailed = 1;
         pNIC->failedFrequency = frequency;
         pNIC->currentFrequency = 0;
         failed = true;
         int len = strlen(szOutput);
         for( int i=0; i<len; i++ )
            if ( szOutput[i] == 10 || szOutput[i] == 13 )
               szOutput[i] = '.';
         log_softerror_and_alarm("Failed to switch radio interface %d (%s, %s) to frequency %d Mhz, returned error: [%s]", i+1, pNIC->szName, str_get_radio_driver_description(pNIC->typeAndDriver), frequency, szOutput);
         hardware_sleep_ms(delayMs);
         continue;
      }

      log_line("Setting radio interface %d (%s, %s) to frequency %d Mhz succeeded.", i+1, pNIC->szName, str_get_radio_driver_description(pNIC->typeAndDriver), frequency);
      pNIC->currentFrequency = frequency;
      pNIC->lastFrequencySetFailed = 0;
      pNIC->failedFrequency = 0;
      anySucceeded = true;
      hardware_sleep_ms(delayMs);
   }

   if ( -1 == nicIndex )
      log_line("Setting %s to frequency %d Mhz result: %s, at least one radio interface succeeded: ", szInfo, frequency, (failed?"failed":"succeeded"), (anySucceeded?"yes":"no"));

   return anySucceeded;

   /*
   log_line("=====================================================");
   log_line("All radio interfaces and current frequencies:");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(i);
      log_line("  * %d: %s %s %s: %d Mhz", i, pNIC->szName, pNIC->szMAC, pNIC->szDescription, pNIC->currentFrequency);
   }
   log_line("======================================================");
   */
}

bool launch_set_datarate_atheros(Model* pModel, int iCard, int dataRate)
{
   u32 delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   if ( hardware_is_station() )
   {
      Preferences* pP = get_Preferences();
      if ( NULL != pP )
         delayMs = (u32) pP->iDebugWiFiChangeDelay;
      if ( delayMs<1 || delayMs > 200 )
         delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   }
   else if ( NULL != pModel )
      delayMs = (pModel->uDeveloperFlags >> 8) & 0xFF; 

   delayMs *= 2;
   log_line("Setting global datarate for Atheros radio interface %d to: %d (guard interval: %d ms)", iCard+1, dataRate, (int)delayMs);
   
   char cmd[1024];
   radio_hw_info_t* pNICInfo = hardware_get_radio_info(iCard);
   if ( NULL == pNICInfo )
   {
      log_softerror_and_alarm("Can't get info for radio interface %d", iCard+1);
      return false;
   }

   sprintf(cmd, "ifconfig %s down", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   sprintf(cmd, "iw dev %s set type managed", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   sprintf(cmd, "ifconfig %s up", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   if ( dataRate > 0 )
      sprintf(cmd, "iw dev %s set bitrates legacy-2.4 %d", pNICInfo->szName, dataRate );
   else
      sprintf(cmd, "iw dev %s set bitrates ht-mcs-2.4 %d", pNICInfo->szName, -dataRate-1 );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   sprintf(cmd, "ifconfig %s down", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   sprintf(cmd, "iw dev %s set monitor none", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   sprintf(cmd, "ifconfig %s up", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   log_line("Setting datarate on Atheros radio interface %d to: %d completed.", iCard+1, dataRate);
   return true;
}
