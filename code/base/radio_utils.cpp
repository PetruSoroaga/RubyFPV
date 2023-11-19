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
#include "../base/models.h"
#include "../base/radio_utils.h"
#include "../base/hw_procs.h"
#include "../base/ctrl_preferences.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"

bool radio_utils_set_interface_frequency(Model* pModel, int iRadioIndex, u32 uFrequencyKhz, shared_mem_process_stats* pProcessStats)
{
   if ( uFrequencyKhz <= 0 )
   {
      log_softerror_and_alarm("Skipping setting card (%d) to invalid uFrequencyKhz 0.", iRadioIndex);
      return false;
   }

   u32 uFreqWifi = uFrequencyKhz/1000;

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
      log_line("Setting all radio interfaces to frequency %s (guard interval: %d ms)", str_format_frequency(uFrequencyKhz), (int)delayMs);
      strcpy(szInfo, "all radio interfaces");
   }
   else
   {
      radio_hw_info_t* pRadioInfo2 = hardware_get_radio_info(iRadioIndex);
      log_line("Setting radio interface %d (%s, %s) to frequency %s (guard interval: %d ms)", iRadioIndex+1, pRadioInfo2->szName, str_get_radio_driver_description(pRadioInfo2->typeAndDriver), str_format_frequency(uFrequencyKhz), (int)delayMs);
      sprintf(szInfo, "radio interface %d (%s, %s)", iRadioIndex+1, pRadioInfo2->szName, str_get_radio_driver_description(pRadioInfo2->typeAndDriver));
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
      if ( NULL == pRadioInfo || (0 == hardware_radioindex_supports_frequency(i, uFrequencyKhz)) )
      {
         log_line("Radio interface %d (%s, %s) does not support %s. Skipping it.", i+1, pRadioInfo->szName, str_get_radio_driver_description(pRadioInfo->typeAndDriver), str_format_frequency(uFrequencyKhz));
         pRadioInfo->lastFrequencySetFailed = 1;
         pRadioInfo->uFailedFrequencyKhz = uFrequencyKhz;
         failed = true;
         continue;
      }

      if ( hardware_radio_is_sik_radio(pRadioInfo) )
      {
         if ( ! hardware_radio_sik_set_frequency(pRadioInfo, uFrequencyKhz, pProcessStats) )
         {
            log_softerror_and_alarm("Failed to switch SiK radio interface %d to frequency %s", i+1, str_format_frequency(uFrequencyKhz));
            if ( NULL != pProcessStats )
               pProcessStats->lastActiveTime = get_current_timestamp_ms();
            continue;
         }
      }
      else if ( hardware_radio_is_wifi_radio(pRadioInfo) )
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
               sprintf(cmd, "iw dev %s set freq %u HT40+ 2>&1", pRadioInfo->szName, uFreqWifi);
               bUsedHT40 = true;
            }
            else
            {
               sprintf(cmd, "iw dev %s set freq %u HT40+ 2>&1", pRadioInfo->szName, uFreqWifi);
               bUsedHT40 = true;
            }
         }
         else if ( pRadioInfo->isHighCapacityInterface )
            sprintf(cmd, "iw dev %s set freq %u 2>&1", pRadioInfo->szName, uFreqWifi);

         hw_execute_bash_command_raw(cmd, szOutput);

         if ( NULL != strstr( szOutput, "Invalid argument" ) )
         if ( bUsedHT40 )
         if ( pRadioInfo->isHighCapacityInterface )
         {
            int len = strlen(szOutput);
            for( int i=0; i<len; i++ )
               if ( szOutput[i] == 10 || szOutput[i] == 13 )
                  szOutput[i] = '.';

            log_softerror_and_alarm("Failed to switch radio interface %d (%s, %s) to frequency %s in HT40 mode, returned error: [%s]. Retry operation.", i+1, pRadioInfo->szName, str_get_radio_driver_description(pRadioInfo->typeAndDriver), str_format_frequency(uFrequencyKhz), szOutput);
            hardware_sleep_ms(delayMs);
            szOutput[0] = 0;
            sprintf(cmd, "iw dev %s set freq %u 2>&1", pRadioInfo->szName, uFreqWifi);
            hw_execute_bash_command_raw(cmd, szOutput);
         }

         if ( NULL != strstr( szOutput, "failed" ) )
         {
            pRadioInfo->lastFrequencySetFailed = 1;
            pRadioInfo->uFailedFrequencyKhz = uFrequencyKhz;
            pRadioInfo->uCurrentFrequencyKhz = 0;
            failed = true;
            int len = strlen(szOutput);
            for( int i=0; i<len; i++ )
               if ( szOutput[i] == 10 || szOutput[i] == 13 )
                  szOutput[i] = '.';
            log_softerror_and_alarm("Failed to switch radio interface %d (%s, %s) to frequency %s, returned error: [%s]", i+1, pRadioInfo->szName, str_get_radio_driver_description(pRadioInfo->typeAndDriver), str_format_frequency(uFrequencyKhz), szOutput);
            hardware_sleep_ms(delayMs);
            continue;
         }
      }
      
      else
      {
         log_softerror_and_alarm("Detected unknown radio interface type.");
         continue;
      }
      log_line("Setting radio interface %d (%s, %s) to frequency %s succeeded.", i+1, pRadioInfo->szName, str_get_radio_driver_description(pRadioInfo->typeAndDriver), str_format_frequency(uFrequencyKhz));
      pRadioInfo->uCurrentFrequencyKhz = uFrequencyKhz;
      pRadioInfo->lastFrequencySetFailed = 0;
      pRadioInfo->uFailedFrequencyKhz = 0;
      anySucceeded = true;

      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
      hardware_sleep_ms(delayMs);
   }

   if ( -1 == iRadioIndex )
      log_line("Setting %s to frequency %s result: %s, at least one radio interface succeeded: ", szInfo, str_format_frequency(uFrequencyKhz), (failed?"failed":"succeeded"), (anySucceeded?"yes":"no"));

   return anySucceeded;
}

bool radio_utils_set_datarate_atheros(Model* pModel, int iCard, int dataRate_bps)
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

   delayMs += 20;
   log_line("Setting global datarate for Atheros/RaLink radio interface %d to: %d (guard interval: %d ms)", iCard+1, dataRate_bps, (int)delayMs);
   
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iCard);
   if ( NULL == pRadioHWInfo )
   {
      log_softerror_and_alarm("Can't get info for radio interface %d", iCard+1);
      return false;
   }

   if ( pRadioHWInfo->iCurrentDataRate == dataRate_bps )
   {
      log_line("Atheros/RaLink radio interface %d already on datarate: %d. Done.", iCard+1, dataRate_bps);
      return true;
   }

   char cmd[1024];

   sprintf(cmd, "ifconfig %s down", pRadioHWInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   sprintf(cmd, "iw dev %s set type managed", pRadioHWInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   sprintf(cmd, "ifconfig %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   if ( dataRate_bps > 0 )
      sprintf(cmd, "iw dev %s set bitrates legacy-2.4 %d", pRadioHWInfo->szName, dataRate_bps/1000/1000 );
   else
      sprintf(cmd, "iw dev %s set bitrates ht-mcs-2.4 %d", pRadioHWInfo->szName, -dataRate_bps-1 );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   sprintf(cmd, "ifconfig %s down", pRadioHWInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   sprintf(cmd, "iw dev %s set monitor none", pRadioHWInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   sprintf(cmd, "ifconfig %s up", pRadioHWInfo->szName );
   hw_execute_bash_command(cmd, NULL);
   hardware_sleep_ms(delayMs);

   pRadioHWInfo->iCurrentDataRate = dataRate_bps;
   hardware_save_radio_info();
   log_line("Setting datarate on Atheros/RaLink radio interface %d to: %d completed.", iCard+1, dataRate_bps);
   return true;
}


void update_atheros_card_datarate(Model* pModel, int iInterfaceIndex, int iDataRate, shared_mem_process_stats* pProcessStats)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return;
   if ( ((pRadioHWInfo->typeAndDriver & 0xFF) != RADIO_TYPE_ATHEROS) &&
        ((pRadioHWInfo->typeAndDriver & 0xFF) != RADIO_TYPE_RALINK) )
      return;

   if ( pRadioHWInfo->iCurrentDataRate == iDataRate )
      return;

   log_line("Must update Atheros/RaLink radio interface %d datarate from %d to %d.", iInterfaceIndex+1, pRadioHWInfo->iCurrentDataRate, iDataRate );
   
   radio_rx_pause_interface(iInterfaceIndex);
   radio_tx_pause_radio_interface(iInterfaceIndex);

   bool bWasOpenedForWrite = false;
   bool bWasOpenedForRead = false;
   if ( pRadioHWInfo->openedForWrite )
   {
      bWasOpenedForWrite = true;
      radio_close_interface_for_write(iInterfaceIndex);
   }
   if ( pRadioHWInfo->openedForRead )
   {
      bWasOpenedForRead = true;
      radio_close_interface_for_read(iInterfaceIndex);
   }
   u32 uTimeNow = get_current_timestamp_ms();
   if ( NULL != pProcessStats )
   {
      pProcessStats->lastActiveTime = uTimeNow;
      pProcessStats->lastIPCIncomingTime = uTimeNow;
   }

   radio_utils_set_datarate_atheros(pModel, iInterfaceIndex, iDataRate);

   uTimeNow = get_current_timestamp_ms();
   if ( NULL != pProcessStats )
   {
      pProcessStats->lastActiveTime = uTimeNow;
      pProcessStats->lastIPCIncomingTime = uTimeNow;
   }

   if ( bWasOpenedForRead )
      radio_open_interface_for_read(iInterfaceIndex, RADIO_PORT_ROUTER_DOWNLINK);

   if ( bWasOpenedForWrite )
      radio_open_interface_for_write(iInterfaceIndex);

   hardware_save_radio_info();
   
   uTimeNow = get_current_timestamp_ms();
   if ( NULL != pProcessStats )
   {
      pProcessStats->lastActiveTime = uTimeNow;
      pProcessStats->lastIPCIncomingTime = uTimeNow;
   }

   radio_tx_resume_radio_interface(iInterfaceIndex);
   radio_rx_resume_interface(iInterfaceIndex);
}


// Returns true if configuration has been updated
// It's called only on vehicle side

bool configure_radio_interfaces_for_current_model(Model* pModel, shared_mem_process_stats* pProcessStats)
{
   bool bMissmatch = false;

   log_line("CONFIGURE RADIO START -----------------------------------------------------------");

   if ( NULL == pModel )
   {
      log_line("Configuring all radio interfaces (%d radio interfaces) failed.", hardware_get_radio_interfaces_count());
      log_error_and_alarm("INVALID MODEL parameter (no model, NULL)");
      log_line("CONFIGURE RADIO END ------------------------------------------------------------");
      return false;
   }

   // Populate radio interfaces radio flags from radio links radio flags and rates

   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
     if ( pModel->radioInterfacesParams.interface_link_id[i] >= 0 && pModel->radioInterfacesParams.interface_link_id[i] < pModel->radioLinksParams.links_count )
     {
         pModel->radioInterfacesParams.interface_current_radio_flags[i] = pModel->radioLinksParams.link_radio_flags[pModel->radioInterfacesParams.interface_link_id[i]];
     }
     else
        log_softerror_and_alarm("No radio link or invalid radio link (%d of %d) assigned to radio interface %d.", pModel->radioInterfacesParams.interface_link_id[i], pModel->radioLinksParams.links_count, i+1);
   }

   log_line("Configuring all radio interfaces (%d radio interfaces, %d radio links)", hardware_get_radio_interfaces_count(), pModel->radioLinksParams.links_count);
   if ( pModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      log_line("A relay link is enabled on radio link %d, on %s", pModel->relay_params.isRelayEnabledOnRadioLinkId+1, str_format_frequency(pModel->relay_params.uRelayFrequencyKhz));

   for( int iLink=0; iLink<pModel->radioLinksParams.links_count; iLink++ )
   {
      u32 uRadioLinkFrequency = pModel->radioLinksParams.link_frequency_khz[iLink];
      if ( pModel->relay_params.isRelayEnabledOnRadioLinkId == iLink )
      {
         uRadioLinkFrequency = pModel->relay_params.uRelayFrequencyKhz;
         if ( uRadioLinkFrequency == 0 )
         {
            uRadioLinkFrequency = DEFAULT_FREQUENCY58;
            for( int iInterface=0; iInterface<pModel->radioInterfacesParams.interfaces_count; iInterface++ )
            {
               if ( pModel->radioInterfacesParams.interface_link_id[iInterface] != iLink )
                  continue;
               if ( ! hardware_radioindex_supports_frequency(iInterface, uRadioLinkFrequency ) )
                  uRadioLinkFrequency = DEFAULT_FREQUENCY;
            }
            log_line("Radio link %d is a relay link and relay frequency is 0. Setting a default frequency for this link: %s.", iLink+1, str_format_frequency(uRadioLinkFrequency));
         }
         else log_line("Radio link %d is a relay link on %s", iLink+1, str_format_frequency(uRadioLinkFrequency));
      }
      else
         log_line("Radio link %d must be set to %s", iLink+1, str_format_frequency(uRadioLinkFrequency));

      if ( pModel->radioLinksParams.link_capabilities_flags[iLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         log_softerror_and_alarm("Radio Link %d is disabled!", iLink+1 );
         continue;
      }

      int iLinkConfiguredInterfacesCount = 0;

      for( int iInterface=0; iInterface<pModel->radioInterfacesParams.interfaces_count; iInterface++ )
      {
         if ( pModel->radioInterfacesParams.interface_link_id[iInterface] != iLink )
            continue;
         if ( pModel->radioInterfacesParams.interface_capabilities_flags[iInterface] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         {
            log_softerror_and_alarm("Radio Interface %d (assigned to radio link %d) is disabled!", iInterface+1, iLink+1 );
            continue;
         }
         if ( ! hardware_radioindex_supports_frequency(iInterface, uRadioLinkFrequency ) )
         {
            log_softerror_and_alarm("Radio interface %d (assigned to radio link %d) does not support radio link frequency %s!", iInterface+1, iLink+1, str_format_frequency(uRadioLinkFrequency) );
            bMissmatch = true;
            continue;
         }

         if ( hardware_radio_index_is_sik_radio(iInterface) )
         {
            log_line("Configuring SiK radio interface %d for radio link %d", iInterface+1, iLink+1);
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterface);
            if ( NULL == pRadioHWInfo )
            {
               log_softerror_and_alarm("Failed to get radio hardware info for radio interface %d.", iInterface+1);
               continue;
            }
            hardware_radio_sik_set_frequency_txpower_airspeed_lbt_ecc(pRadioHWInfo,
               uRadioLinkFrequency, pModel->radioInterfacesParams.txPowerSiK,
               pModel->radioLinksParams.link_datarate_data_bps[iLink],
               (u32)((pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_ECC)?1:0),
               (u32)((pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_LBT)?1:0),
               (u32)((pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_MCSTR)?1:0),
               pProcessStats);
            iLinkConfiguredInterfacesCount++;
         }
         else
         {
            radio_utils_set_interface_frequency(pModel, iInterface, uRadioLinkFrequency, pProcessStats);
            iLinkConfiguredInterfacesCount++;
         }
         if ( pModel->relay_params.isRelayEnabledOnRadioLinkId == iLink )
         {
            log_line("Did set radio interface %d to frequency %s (assigned to radio link %d in relay mode)", iInterface+1, str_format_frequency(uRadioLinkFrequency), iLink+1 );
            pModel->radioInterfacesParams.interface_capabilities_flags[iInterface] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
         }
         else
         {
            log_line("Did set radio interface %d to frequency %s (assigned to radio link %d)", iInterface+1, str_format_frequency(uRadioLinkFrequency), iLink+1 );
            pModel->radioInterfacesParams.interface_capabilities_flags[iInterface] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
         }
         pModel->radioInterfacesParams.interface_current_frequency_khz[iInterface] = uRadioLinkFrequency;
      }
      log_line("Configured a total of %d radio interfaces for radio link %d", iLinkConfiguredInterfacesCount, iLink+1);
   }

   if ( bMissmatch )
   if ( 1 == pModel->radioLinksParams.links_count )
   if ( 1 == pModel->radioInterfacesParams.interfaces_count )
   {
      log_line("There was a missmatch of frequency configuration for the single radio link present on vehicle. Reseting it to default.");
      pModel->radioLinksParams.link_frequency_khz[0] = DEFAULT_FREQUENCY;
      pModel->radioInterfacesParams.interface_current_frequency_khz[0] = DEFAULT_FREQUENCY;
      radio_utils_set_interface_frequency(pModel, 0, pModel->radioLinksParams.link_frequency_khz[0], pProcessStats);
      log_line("Set radio interface 1 to link 1 frequency %s", str_format_frequency(pModel->radioLinksParams.link_frequency_khz[0]) );
   }
   hardware_save_radio_info();
   hardware_sleep_ms(50);
   log_line("CONFIGURE RADIO END ---------------------------------------------------------");

   return bMissmatch;
}


void log_current_full_radio_configuration(Model* pModel)
{
   log_line("=====================================================================================");

   if ( NULL == pModel )
   {
      log_line("Current vehicle radio configuration:");
      log_error_and_alarm("INVALID MODEL parameter");
      log_line("=====================================================================================");
      return;
   }

   if ( pModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      log_line("Current vehicle radio configuration: %d radio links, of which one is a relay link:", pModel->radioLinksParams.links_count);
   else
      log_line("Current vehicle radio configuration: %d radio links:", pModel->radioLinksParams.links_count);
   log_line("");
   for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
   {
      char szPrefix[32];
      char szBuff[256];
      char szBuff2[256];
      char szBuff3[256];
      char szBuff4[256];
      char szBuff5[1200];
      szBuff[0] = 0;
      szPrefix[0] = 0;
      for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( pModel->radioInterfacesParams.interface_link_id[k] == i )
         {
            char szInfo[32];
            if ( 0 != szBuff[0] )
               sprintf(szInfo, ", %d", k+1);
            else
               sprintf(szInfo, "%d", k+1);
            strcat(szBuff, szInfo);
         }
      }
      if ( pModel->relay_params.isRelayEnabledOnRadioLinkId == i )
         strcpy(szPrefix, "Relay ");

      if ( pModel->relay_params.isRelayEnabledOnRadioLinkId == i )
         log_line("* %sRadio Link %d Info:  %s, radio interface(s) assigned to this link: [%s]", szPrefix, i+1, str_format_frequency(pModel->relay_params.uRelayFrequencyKhz), szBuff);
      else
         log_line("* %sRadio Link %d Info:  %s, radio interface(s) assigned to this link: [%s]", szPrefix, i+1, str_format_frequency(pModel->radioLinksParams.link_frequency_khz[i]), szBuff);
      
      szBuff[0] = 0;

      str_get_radio_capabilities_description(pModel->radioLinksParams.link_capabilities_flags[i], szBuff);
      str_get_radio_frame_flags_description(pModel->radioLinksParams.link_radio_flags[i], szBuff2); 
      log_line("* %sRadio Link %d Capab: %s, Radio flags: %s", szPrefix, i+1, szBuff, szBuff2);
      str_getDataRateDescription(pModel->radioLinksParams.link_datarate_video_bps[i], szBuff);
      str_getDataRateDescription(pModel->radioLinksParams.link_datarate_data_bps[i], szBuff2);
      str_getDataRateDescription(pModel->radioLinksParams.uplink_datarate_video_bps[i], szBuff3);
      str_getDataRateDescription(pModel->radioLinksParams.uplink_datarate_data_bps[i], szBuff4);
      if ( pModel->radioLinksParams.bUplinkSameAsDownlink[i] )
         sprintf(szBuff5, "video: %s, data: %s, same for uplink video: %s, data: %s;", szBuff, szBuff2, szBuff3, szBuff4);
      else
         sprintf(szBuff5, "video: %s, data: %s, uplink video: %s, data: %s;", szBuff, szBuff2, szBuff3, szBuff4);
      log_line("* %sRadio Link %d Datarates: %s", szPrefix, i+1, szBuff5);
      log_line("");
   }

   log_line("=====================================================================================");
   log_line("Physical Radio Interfaces (%d configured, %d detected):", pModel->radioInterfacesParams.interfaces_count, hardware_get_radio_interfaces_count());
   log_line("");
   int count = pModel->radioInterfacesParams.interfaces_count;
   if ( count != hardware_get_radio_interfaces_count() )
   {
      log_softerror_and_alarm("Count of detected radio interfaces is not the same as the count of configured ones for this vehicle!");
      if ( count > hardware_get_radio_interfaces_count() )
         count = hardware_get_radio_interfaces_count();
   }
   for( int i=0; i<count; i++ )
   {
      char szPrefix[32];
      char szBuff[256];
      char szBuff2[256];
      szPrefix[0] = 0;
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      str_get_radio_capabilities_description(pModel->radioInterfacesParams.interface_capabilities_flags[i], szBuff);
      str_get_radio_frame_flags_description(pModel->radioInterfacesParams.interface_current_radio_flags[i], szBuff2); 
      if ( pModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         strcpy(szPrefix, "Relay ");
      log_line("* %sRadio int %d: %s [%s] %s, current frequency: %s, assigned to radio link %d", szPrefix, i+1, pRadioInfo->szUSBPort, str_get_radio_card_model_string(pModel->radioInterfacesParams.interface_card_model[i]), pRadioInfo->szDriver, str_format_frequency(pRadioInfo->uCurrentFrequencyKhz), pModel->radioInterfacesParams.interface_link_id[i]+1);
      log_line("* %sRadio int %d Capab: %s, Radio flags: %s", szPrefix, i+1, szBuff, szBuff2);
      char szDR1[32];
      char szDR2[32];
      if ( pModel->radioInterfacesParams.interface_datarate_video_bps[i] != 0 )
         str_getDataRateDescription(pModel->radioInterfacesParams.interface_datarate_video_bps[i], szDR1);
      else
         strcpy(szDR1, "auto");

      if ( pModel->radioInterfacesParams.interface_datarate_data_bps[i] != 0 )
         str_getDataRateDescription(pModel->radioInterfacesParams.interface_datarate_data_bps[i], szDR2);
      else
         strcpy(szDR2, "auto");
      log_line("* %sRadio int %d Datarates (video/data): %s / %s", szPrefix, i+1, szDR1, szDR2);
      log_line("");
   }
   log_line("=====================================================================================");
}