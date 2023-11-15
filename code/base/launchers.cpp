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
#include "hardware_radio_sik.h"
#include "hw_procs.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"

bool launch_set_frequency(Model* pModel, int iRadioIndex, u32 uFrequency, shared_mem_process_stats* pProcessStats)
{
   if ( uFrequency <= 0 )
   {
      log_softerror_and_alarm("Skipping setting card (%d) to invalid uFrequency 0.", iRadioIndex);
      return false;
   }

   u32 uFreqWifi = uFrequency;
   if ( uFreqWifi > 10000 )
     uFreqWifi /= 1000;

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

   if ( -1 == iRadioIndex )
   {
      log_line("Setting all radio interfaces to frequency %s (guard interval: %d ms)", str_format_frequency(uFrequency), (int)delayMs);
      strcpy(szInfo, "all radio interfaces");
   }
   else
   {
      radio_hw_info_t* pRadioInfo2 = hardware_get_radio_info(iRadioIndex);
      log_line("Setting radio interface %d (%s, %s) to frequency %s (guard interval: %d ms)", iRadioIndex+1, pRadioInfo2->szName, str_get_radio_driver_description(pRadioInfo2->typeAndDriver), str_format_frequency(uFrequency), (int)delayMs);
      snprintf(szInfo, sizeof(szInfo), "radio interface %d (%s, %s)", iRadioIndex+1, pRadioInfo2->szName, str_get_radio_driver_description(pRadioInfo2->typeAndDriver));
      iStartIndex = iRadioIndex;
      iEndIndex = iRadioIndex;
   }

   char cmd[128];
   char szOutput[512];
   bool failed = false;
   bool anySucceeded = false;

   for( int i=iStartIndex; i<=iEndIndex; i++ )
   {
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
      
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo || (0 == hardware_radioindex_supports_frequency(i, uFrequency)) )
      {
         log_line("Radio interface %d (%s, %s) does not support %s. Skipping it.", i+1, pRadioInfo->szName, str_get_radio_driver_description(pRadioInfo->typeAndDriver), str_format_frequency(uFrequency));
         pRadioInfo->lastFrequencySetFailed = 1;
         pRadioInfo->failedFrequency = uFrequency;
         failed = true;
         continue;
      }

      if ( hardware_radio_is_wifi_radio(pRadioInfo) )
      {
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
            if ( (pRadioInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS )
            {
               snprintf(cmd, sizeof(cmd), "iw dev %s set freq %u HT40+ 2>&1", pRadioInfo->szName, uFreqWifi);
               bUsedHT40 = true;
            }
            else
            {
               snprintf(cmd, sizeof(cmd), "iw dev %s set freq %u HT40+ 2>&1", pRadioInfo->szName, uFreqWifi);
               bUsedHT40 = true;
            }
         }
         else if ( pRadioInfo->isHighCapacityInterface )
            snprintf(cmd, sizeof(cmd), "iw dev %s set freq %u 2>&1", pRadioInfo->szName, uFreqWifi);

         hw_execute_bash_command_raw(cmd, szOutput);

         if ( NULL != strstr( szOutput, "Invalid argument" ) )
         if ( bUsedHT40 )
         if ( pRadioInfo->isHighCapacityInterface )
         {
            int len = strlen(szOutput);
            for( int i=0; i<len; i++ )
               if ( szOutput[i] == 10 || szOutput[i] == 13 )
                  szOutput[i] = '.';

            log_softerror_and_alarm("Failed to switch radio interface %d (%s, %s) to frequency %s in HT40 mode, returned error: [%s]. Retry operation.", i+1, pRadioInfo->szName, str_get_radio_driver_description(pRadioInfo->typeAndDriver), str_format_frequency(uFrequency), szOutput);
            hardware_sleep_ms(delayMs);
            szOutput[0] = 0;
            snprintf(cmd, sizeof(cmd), "iw dev %s set freq %u 2>&1", pRadioInfo->szName, uFreqWifi);
            hw_execute_bash_command_raw(cmd, szOutput);
         }

         if ( NULL != strstr( szOutput, "failed" ) )
         {
            pRadioInfo->lastFrequencySetFailed = 1;
            pRadioInfo->failedFrequency = uFrequency;
            pRadioInfo->uCurrentFrequency = 0;
            failed = true;
            int len = strlen(szOutput);
            for( int i=0; i<len; i++ )
               if ( szOutput[i] == 10 || szOutput[i] == 13 )
                  szOutput[i] = '.';
            log_softerror_and_alarm("Failed to switch radio interface %d (%s, %s) to frequency %s, returned error: [%s]", i+1, pRadioInfo->szName, str_get_radio_driver_description(pRadioInfo->typeAndDriver), str_format_frequency(uFrequency), szOutput);
            hardware_sleep_ms(delayMs);
            continue;
         }
      }
      else if ( hardware_radio_is_sik_radio(pRadioInfo) )
      {
         if ( ! hardware_radio_sik_set_frequency(pRadioInfo, uFrequency, pProcessStats) )
         {
            log_softerror_and_alarm("Failed to switch SiK radio interface %d to frequency %s", i+1, str_format_frequency(uFrequency));
         }
      }
      else
      {
         log_softerror_and_alarm("Detected unknown radio interface type.");
         continue;
      }
      log_line("Setting radio interface %d (%s, %s) to frequency %s succeeded.", i+1, pRadioInfo->szName, str_get_radio_driver_description(pRadioInfo->typeAndDriver), str_format_frequency(uFrequency));
      pRadioInfo->uCurrentFrequency = uFrequency;
      pRadioInfo->lastFrequencySetFailed = 0;
      pRadioInfo->failedFrequency = 0;
      anySucceeded = true;

      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
      hardware_sleep_ms(delayMs);
   }

   if ( -1 == iRadioIndex )
      log_line("Setting %s to frequency %s result: %s, at least one radio interface succeeded: ", szInfo, str_format_frequency(uFrequency), (failed?"failed":"succeeded"), (anySucceeded?"yes":"no"));

   return anySucceeded;
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

   snprintf(cmd, sizeof(cmd), "ifconfig %s down", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   snprintf(cmd, sizeof(cmd), "iw dev %s set type managed", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   snprintf(cmd, sizeof(cmd), "ifconfig %s up", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   if ( dataRate > 0 )
      snprintf(cmd, sizeof(cmd), "iw dev %s set bitrates legacy-2.4 %d", pNICInfo->szName, dataRate );
   else
      snprintf(cmd, sizeof(cmd), "iw dev %s set bitrates ht-mcs-2.4 %d", pNICInfo->szName, -dataRate-1 );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   snprintf(cmd, sizeof(cmd), "ifconfig %s down", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   snprintf(cmd, sizeof(cmd), "iw dev %s set monitor none", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   snprintf(cmd, sizeof(cmd), "ifconfig %s up", pNICInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   log_line("Setting datarate on Atheros radio interface %d to: %d completed.", iCard+1, dataRate);
   return true;
}
