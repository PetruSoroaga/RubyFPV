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

#include "hw_config_check.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../radio/radiolink.h"
#include "../common/string_utils.h"


// Returns true if hardware nics have changed

bool _check_update_hardware_one_interface_after_and_before(Model* pModel)
{
   radio_hw_info_t* pNIC = hardware_get_radio_info(0);
   if ( NULL == pNIC )
   {
      log_error_and_alarm("Failed to get radio interface info for radio interface 1!");
      log_line("[HW Radio Check] NICs hardware check: Aborted.");
      return false;
   }

   if ( 0 != pModel->radioInterfacesParams.interface_szMAC[0][0] && 0 != pNIC->szMAC[0] )
   if ( 0 == strcmp(pModel->radioInterfacesParams.interface_szMAC[0], pNIC->szMAC) )
   {
      log_line("[HW Radio Check] NICs hardware check complete: No change in radio interfaces (same single one present in the sistem before and after)");
      return false;
   }

   log_line("[HW Radio Check] NICs hardware check: The single radio interface has changed in the system. Updating system to the new one.");

   type_radio_interfaces_parameters currentRadioInterfacesParams;
   type_radio_links_parameters currentRadioLinksParams;
   memcpy(&currentRadioLinksParams, &(pModel->radioLinksParams), sizeof(type_radio_links_parameters));
   memcpy(&currentRadioInterfacesParams, &(pModel->radioInterfacesParams), sizeof(type_radio_interfaces_parameters));

   pModel->radioInterfacesParams.interface_supported_bands[0] = pNIC->supportedBands;
   pModel->radioInterfacesParams.interface_type_and_driver[0] = pNIC->typeAndDriver;
   if ( pNIC->isSupported )
      pModel->radioInterfacesParams.interface_type_and_driver[0] = pModel->radioInterfacesParams.interface_type_and_driver[0] | 0xFF0000;
   strcpy(pModel->radioInterfacesParams.interface_szMAC[0], pNIC->szMAC );
   strcpy(pModel->radioInterfacesParams.interface_szPort[0], pNIC->szUSBPort);

   if ( ! hardware_radio_supports_frequency(pNIC, pModel->radioLinksParams.link_frequency[0]) )
   {
      pModel->radioLinksParams.link_frequency[0] = DEFAULT_FREQUENCY;
      if ( pModel->radioInterfacesParams.interface_supported_bands[0] & RADIO_HW_SUPPORTED_BAND_58 )
         pModel->radioLinksParams.link_frequency[0] = DEFAULT_FREQUENCY58;
   }

   pModel->radioLinksParams.link_radio_flags[0] = DEFAULT_RADIO_FRAMES_FLAGS;

   // Populate radio interfaces radio flags and rates from radio links radio flags and rates

   pModel->radioInterfacesParams.interface_current_frequency[0] = pModel->radioLinksParams.link_frequency[0];
   pModel->radioInterfacesParams.interface_current_radio_flags[0] = pModel->radioLinksParams.link_radio_flags[0];
   pModel->radioInterfacesParams.interface_datarates[0][0] = 0;
   pModel->radioInterfacesParams.interface_datarates[0][1] = 0;

   log_line("[HW Radio Check] NICs hardware check: Updated radio links based on current hardware radio interfaces. Completed.");
   return true;
}

bool _check_update_hardware_one_interface_after_multiple_before(Model* pModel)
{
   log_line("[HW Radio Check] NICs hardware check: Radio interfaces where removed, only one remaining.");
   radio_hw_info_t* pNICInfo = hardware_get_radio_info(0);
   if ( NULL == pNICInfo )
   {
      log_error_and_alarm("Failed to get radio interface info for radio interface 1!");
      log_line("[HW Radio Check] NICs hardware check: Aborted.");
      return false;
   }

   // See if the interface was present in the system before

   int iRadioInterfaceIndex = -1;
   int iRadioLinkIndex = -1;

   for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
   {
      if ( 0 != pModel->radioInterfacesParams.interface_szMAC[k][0] && 0 != pNICInfo->szMAC[0] )
      if ( 0 == strcmp(pModel->radioInterfacesParams.interface_szMAC[k], pNICInfo->szMAC) )
      {
         iRadioInterfaceIndex = k;
         break;
      }
   }

   if ( iRadioInterfaceIndex == -1 )
   {
      // It's a new radio interface.
      // Try to assign an existing radio link to this new interface. If not possible, just reset to the default radio link params.

      for( int k=0; k<pModel->radioLinksParams.links_count; k++ )
      {
         if ( hardware_radio_supports_frequency(pNICInfo, pModel->radioLinksParams.link_frequency[k]) )
         {
            iRadioLinkIndex = k;
            break;
         }
      }

      if ( -1 == iRadioLinkIndex )
      {
         log_line("[HW Radio Check] Could not reasign an existing radio link to the new radio interface in the system. Reseting radio links and radio interfaces to default.");
         pModel->radioInterfacesParams.interfaces_count = 1;
         pModel->populateDefaultRadioInterfacesInfo();
         return true;
      }

      // Remove all radio links except the one we are keeping
      // Move it to first in the list of radio links and make sure it can be used for data and video

      pModel->radioLinksParams.link_frequency[0] = pModel->radioLinksParams.link_frequency[iRadioLinkIndex];
      pModel->radioLinksParams.link_capabilities_flags[0] = pModel->radioLinksParams.link_capabilities_flags[iRadioLinkIndex];
      pModel->radioLinksParams.link_radio_flags[0] = pModel->radioLinksParams.link_radio_flags[iRadioLinkIndex];
      pModel->radioLinksParams.link_datarates[0][0] = pModel->radioLinksParams.link_datarates[iRadioLinkIndex][0];
      pModel->radioLinksParams.link_datarates[0][1] = pModel->radioLinksParams.link_datarates[iRadioLinkIndex][1];
      pModel->radioLinksParams.bUplinkSameAsDownlink[0] = pModel->radioLinksParams.bUplinkSameAsDownlink[iRadioLinkIndex];
      pModel->radioLinksParams.uplink_radio_flags[0] = pModel->radioLinksParams.uplink_radio_flags[iRadioLinkIndex];
      pModel->radioLinksParams.uplink_datarates[0][0] = pModel->radioLinksParams.uplink_datarates[iRadioLinkIndex][0];
      pModel->radioLinksParams.uplink_datarates[0][1] = pModel->radioLinksParams.uplink_datarates[iRadioLinkIndex][1];

      pModel->radioLinksParams.link_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
      pModel->radioLinksParams.links_count = 1;

      // Update the radio interface to match the assigned radio link

      pModel->radioInterfacesParams.interface_card_model[0] = 0;
      pModel->radioInterfacesParams.interface_link_id[0] = 0;
      pModel->radioInterfacesParams.interface_current_frequency[0] = pModel->radioLinksParams.link_frequency[0];
      pModel->radioInterfacesParams.interface_current_radio_flags[0] = pModel->radioLinksParams.link_radio_flags[0];
      pModel->radioInterfacesParams.interface_datarates[0][0] = 0;
      pModel->radioInterfacesParams.interface_datarates[0][1] = 0;

      pModel->radioInterfacesParams.interface_type_and_driver[0] = pNICInfo->typeAndDriver;
      pModel->radioInterfacesParams.interface_supported_bands[0] = pNICInfo->supportedBands;
      if ( pNICInfo->isSupported )
         pModel->radioInterfacesParams.interface_type_and_driver[0] = pModel->radioInterfacesParams.interface_type_and_driver[0] | 0xFF0000;
      
      strcpy(pModel->radioInterfacesParams.interface_szMAC[0], pNICInfo->szMAC );
      strcpy(pModel->radioInterfacesParams.interface_szPort[0], pNICInfo->szUSBPort);

      pModel->radioInterfacesParams.interface_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      pModel->radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      pModel->radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
      pModel->radioInterfacesParams.interfaces_count = 1;

      log_line("[HW Radio Check] NICs hardware check: Updated radio link and radio interface based on current hardware radio interface. Completed.");
      return true;
   }


   // The remaining radio interface was present in the system before,
   // Just remove the radio links that do not have a radio interface anymore;

   iRadioLinkIndex = pModel->radioInterfacesParams.interface_link_id[iRadioInterfaceIndex];

   // Remove all radio links except the one we are keeping
   // Move it to first in the list of radio links and make sure it can be used for data and video

   pModel->radioLinksParams.link_frequency[0] = pModel->radioLinksParams.link_frequency[iRadioLinkIndex];
   pModel->radioLinksParams.link_capabilities_flags[0] = pModel->radioLinksParams.link_capabilities_flags[iRadioLinkIndex];
   pModel->radioLinksParams.link_radio_flags[0] = pModel->radioLinksParams.link_radio_flags[iRadioLinkIndex];
   pModel->radioLinksParams.link_datarates[0][0] = pModel->radioLinksParams.link_datarates[iRadioLinkIndex][0];
   pModel->radioLinksParams.link_datarates[0][1] = pModel->radioLinksParams.link_datarates[iRadioLinkIndex][1];
   pModel->radioLinksParams.bUplinkSameAsDownlink[0] = pModel->radioLinksParams.bUplinkSameAsDownlink[iRadioLinkIndex];
   pModel->radioLinksParams.uplink_radio_flags[0] = pModel->radioLinksParams.uplink_radio_flags[iRadioLinkIndex];
   pModel->radioLinksParams.uplink_datarates[0][0] = pModel->radioLinksParams.uplink_datarates[iRadioLinkIndex][0];
   pModel->radioLinksParams.uplink_datarates[0][1] = pModel->radioLinksParams.uplink_datarates[iRadioLinkIndex][1];

   pModel->radioLinksParams.link_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
   pModel->radioLinksParams.links_count = 1;

   // Update the radio interface to match the assigned radio link

   pModel->radioInterfacesParams.interface_card_model[0] = 0;
   pModel->radioInterfacesParams.interface_link_id[0] = 0;
   pModel->radioInterfacesParams.interface_current_frequency[0] = pModel->radioLinksParams.link_frequency[0];
   pModel->radioInterfacesParams.interface_current_radio_flags[0] = pModel->radioLinksParams.link_radio_flags[0];
   pModel->radioInterfacesParams.interface_datarates[0][0] = 0;
   pModel->radioInterfacesParams.interface_datarates[0][1] = 0;

   pModel->radioInterfacesParams.interface_type_and_driver[0] = pNICInfo->typeAndDriver;
   pModel->radioInterfacesParams.interface_supported_bands[0] = pNICInfo->supportedBands;
   if ( pNICInfo->isSupported )
      pModel->radioInterfacesParams.interface_type_and_driver[0] = pModel->radioInterfacesParams.interface_type_and_driver[0] | 0xFF0000;
      
   strcpy(pModel->radioInterfacesParams.interface_szMAC[0], pNICInfo->szMAC );
   strcpy(pModel->radioInterfacesParams.interface_szPort[0], pNICInfo->szUSBPort);

   pModel->radioInterfacesParams.interface_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   pModel->radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   pModel->radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
   pModel->radioInterfacesParams.interfaces_count = 1;

   log_line("[HW Radio Check] NICs hardware check: Updated radio links based on current hardware radio interfaces. Completed.");
   return true;
}

bool check_update_hardware_nics_vehicle(Model* pModel)
{
   log_line("[HW Radio Check] Checking for NICs/Radio interfaces hardware change...");
   if ( NULL == pModel )
   {
      log_error_and_alarm("[HW Radio Check] Checking for NICs/Radio hardware change failed: No model.");
      return false;
   }

   int nic_count = hardware_get_radio_interfaces_count();

   // Handle the case with a single radio interface before and after ( 1 interface before and 1 interface now)

   if ( 1 == nic_count && 1 == pModel->radioInterfacesParams.interfaces_count )
      return _check_update_hardware_one_interface_after_and_before(pModel);

   // Handle the case with a single remaining radio interface ( multiple interfaces before and only one interface now)

   if ( 1 == nic_count && pModel->radioInterfacesParams.interfaces_count > 1 )
      return _check_update_hardware_one_interface_after_multiple_before(pModel);

   // Handle the case with multiple radio interfaces

   int iCountSameInterfaces = 0;
   int iCountMovedInterfaces = 0;
   bool bDefault24Used = false;
   bool bDefault58Used = false;
   bool bIsExistingInterface[MAX_RADIO_INTERFACES];
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      bIsExistingInterface[i] = false;

   // Find unchanged interfaces, and move them in the currently assigned hardware slot index;

   for( int i=0; i<nic_count; i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo )
         continue;
      for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( 0 != pModel->radioInterfacesParams.interface_szMAC[k][0] && 0 != pNICInfo->szMAC[0] )
         if ( 0 == strcmp(pModel->radioInterfacesParams.interface_szMAC[k], pNICInfo->szMAC) )
         {
            iCountSameInterfaces++;
            bIsExistingInterface[i] = true;
            if ( pModel->radioInterfacesParams.interface_current_frequency[k] == DEFAULT_FREQUENCY )
               bDefault24Used = true;
            if ( pModel->radioInterfacesParams.interface_current_frequency[k] == DEFAULT_FREQUENCY58 )
               bDefault58Used = true;

            if ( k != i )
            {
               log_line("[HW Radio Check] Moving existing model radio interface %s (%s, now at %d Mhz) from HW slot index %d to HW slot index %d", pNICInfo->szMAC, str_get_radio_driver_description(pModel->radioInterfacesParams.interface_type_and_driver[k]), pModel->radioInterfacesParams.interface_current_frequency[k], k+1, i+1);
               iCountMovedInterfaces++;
               int tmp;
               u32 u;
               char szTmp[MAX_MAC_LENGTH+1];

               tmp = pModel->radioInterfacesParams.interface_card_model[i];
               pModel->radioInterfacesParams.interface_card_model[i] = pModel->radioInterfacesParams.interface_card_model[k];
               pModel->radioInterfacesParams.interface_card_model[k] = tmp;

               tmp = pModel->radioInterfacesParams.interface_link_id[i];
               pModel->radioInterfacesParams.interface_link_id[i] = pModel->radioInterfacesParams.interface_link_id[k];
               pModel->radioInterfacesParams.interface_link_id[k] = tmp;

               tmp = pModel->radioInterfacesParams.interface_power[i];
               pModel->radioInterfacesParams.interface_power[i] = pModel->radioInterfacesParams.interface_power[k];
               pModel->radioInterfacesParams.interface_power[k] = tmp;

               u = pModel->radioInterfacesParams.interface_type_and_driver[i];
               pModel->radioInterfacesParams.interface_type_and_driver[i] = pModel->radioInterfacesParams.interface_type_and_driver[k];
               pModel->radioInterfacesParams.interface_type_and_driver[k] = u;

               u = pModel->radioInterfacesParams.interface_supported_bands[i];
               pModel->radioInterfacesParams.interface_supported_bands[i] = pModel->radioInterfacesParams.interface_supported_bands[k];
               pModel->radioInterfacesParams.interface_supported_bands[k] = u;

               strcpy( szTmp, pModel->radioInterfacesParams.interface_szMAC[i]);
               strcpy( pModel->radioInterfacesParams.interface_szMAC[i], pModel->radioInterfacesParams.interface_szMAC[k]);
               strcpy( pModel->radioInterfacesParams.interface_szMAC[k], szTmp);

               strcpy( szTmp, pModel->radioInterfacesParams.interface_szPort[i]);
               strcpy( pModel->radioInterfacesParams.interface_szPort[i], pModel->radioInterfacesParams.interface_szPort[k]);
               strcpy( pModel->radioInterfacesParams.interface_szPort[k], szTmp);

               u = pModel->radioInterfacesParams.interface_capabilities_flags[i];
               pModel->radioInterfacesParams.interface_capabilities_flags[i] = pModel->radioInterfacesParams.interface_capabilities_flags[k];
               pModel->radioInterfacesParams.interface_capabilities_flags[k] = u;

               tmp = pModel->radioInterfacesParams.interface_current_frequency[i];
               pModel->radioInterfacesParams.interface_current_frequency[i] = pModel->radioInterfacesParams.interface_current_frequency[k];
               pModel->radioInterfacesParams.interface_current_frequency[k] = tmp;

               u = pModel->radioInterfacesParams.interface_current_radio_flags[i];
               pModel->radioInterfacesParams.interface_current_radio_flags[i] = pModel->radioInterfacesParams.interface_current_radio_flags[k];
               pModel->radioInterfacesParams.interface_current_radio_flags[k] = u;

               tmp = pModel->radioInterfacesParams.interface_datarates[i][0];
               pModel->radioInterfacesParams.interface_datarates[i][0] = pModel->radioInterfacesParams.interface_datarates[k][0];
               pModel->radioInterfacesParams.interface_datarates[k][0] = tmp;

               tmp = pModel->radioInterfacesParams.interface_datarates[i][1];
               pModel->radioInterfacesParams.interface_datarates[i][1] = pModel->radioInterfacesParams.interface_datarates[k][1];
               pModel->radioInterfacesParams.interface_datarates[k][1] = tmp;
            }
            break;
         }
      }
   }

   // Case: No change in radio interfaces, all are the same, maybe different order

   if ( nic_count == pModel->radioInterfacesParams.interfaces_count )
   if ( iCountSameInterfaces == nic_count )
   {
      if ( iCountMovedInterfaces > 0 )
      {
         log_line("[HW Radio Check] Same radio interfaces are present on the system. Updated only positions for %d radio interfaces.", iCountMovedInterfaces);
         return true;
      }
      log_line("[HW Radio Check] Same radio interfaces are present on the system on the same positions, no change detected or required.");
      return false;
   }

   // Case: Only added new radio interfaces

   if ( iCountSameInterfaces == pModel->radioInterfacesParams.interfaces_count )
   if ( nic_count > pModel->radioInterfacesParams.interfaces_count )
   {
      log_line("[HW Radio Check] Detected only new radio interfaces added. All existing ones where present.");
      log_line("[HW Radio Check] %d existing radio interfaces (same as old ones), %d new radio interfaces.", iCountSameInterfaces, nic_count - iCountSameInterfaces );

      pModel->radioInterfacesParams.interfaces_count = nic_count;

      // Add all new radio interfaces to new radio links

      for( int i=0; i<nic_count; i++ )
      {
         if ( bIsExistingInterface[i] )
            continue;
         radio_hw_info_t* pNIC = hardware_get_radio_info(i);

         // Add a new radio link      
         pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] = 0;
         pModel->radioLinksParams.link_radio_flags[pModel->radioLinksParams.links_count] = DEFAULT_RADIO_FRAMES_FLAGS;
         pModel->radioLinksParams.link_capabilities_flags[pModel->radioLinksParams.links_count] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA | RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
         pModel->radioLinksParams.link_datarates[pModel->radioLinksParams.links_count][0] = DEFAULT_DATARATE_VIDEO;
         pModel->radioLinksParams.link_datarates[pModel->radioLinksParams.links_count][1] = DEFAULT_DATARATE_DATA;
         pModel->radioLinksParams.bUplinkSameAsDownlink[pModel->radioLinksParams.links_count] = 1;
         pModel->radioLinksParams.uplink_radio_flags[pModel->radioLinksParams.links_count] = DEFAULT_RADIO_FRAMES_FLAGS;
         pModel->radioLinksParams.uplink_datarates[pModel->radioLinksParams.links_count][0] = DEFAULT_DATARATE_VIDEO;
         pModel->radioLinksParams.uplink_datarates[pModel->radioLinksParams.links_count][1] = DEFAULT_DATARATE_DATA;

         // Populate new radio interface info
         pModel->radioInterfacesParams.interface_card_model[i] = 0;
         pModel->radioInterfacesParams.interface_link_id[i] = pModel->radioLinksParams.links_count;
         pModel->radioInterfacesParams.interface_current_frequency[i] = 0;
         pModel->radioInterfacesParams.interface_current_radio_flags[i] = pModel->radioLinksParams.link_radio_flags[pModel->radioLinksParams.links_count];
         pModel->radioInterfacesParams.interface_datarates[i][0] = 0;
         pModel->radioInterfacesParams.interface_datarates[i][1] = 0;

         pModel->radioInterfacesParams.interface_type_and_driver[i] = pNIC->typeAndDriver;
         pModel->radioInterfacesParams.interface_supported_bands[i] = pNIC->supportedBands;
         if ( pNIC->isSupported )
            pModel->radioInterfacesParams.interface_type_and_driver[i] = pModel->radioInterfacesParams.interface_type_and_driver[i] | 0xFF0000;
      
         strcpy(pModel->radioInterfacesParams.interface_szMAC[i], pNIC->szMAC );
         strcpy(pModel->radioInterfacesParams.interface_szPort[i], pNIC->szUSBPort);

         pModel->radioInterfacesParams.interface_capabilities_flags[i] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
         // Assign a frequency to the new radio link and the radio interface

         pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] = DEFAULT_FREQUENCY;
         if ( pModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_58 )
            pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] = DEFAULT_FREQUENCY58;
            
         if ( bDefault58Used && pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] == DEFAULT_FREQUENCY58 )
            pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] = DEFAULT_FREQUENCY58_2;
         if ( bDefault24Used && pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] == DEFAULT_FREQUENCY )
            pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] = DEFAULT_FREQUENCY_2;
         
         pModel->radioInterfacesParams.interface_current_frequency[i] = pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count];
         pModel->radioLinksParams.links_count++;
         log_line("[HW Radio Check] Added a new radio link (radio link %d) for radio interface %d, on %d Mhz", pModel->radioLinksParams.links_count, i+1, pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count-1]);
      }
      return true;
   }

   // Case: Only removed radio interfaces, no new ones

   if ( nic_count < pModel->radioInterfacesParams.interfaces_count )
   if ( iCountSameInterfaces == nic_count )
   {
      log_line("[HW Radio Check] Only some radio interfaces where removed. All other radio interfaces are the same. No new radio interfaces detected.");

      // Check which radio links are still active (have cards)
      bool bRadioLinksHaveInterfaces[MAX_RADIO_INTERFACES];
      bool bHasTXData = false;
      bool bHasRXData = false;
      bool bHasTXVideo = false;
      bool bHasRXVideo = false;

      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         bRadioLinksHaveInterfaces[i] = false;

      for( int i=0; i<nic_count; i++ )
      {
         int nLinkId = pModel->radioInterfacesParams.interface_link_id[i];
         if ( nLinkId < 0 )
            continue;
         bRadioLinksHaveInterfaces[nLinkId] = true;
         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;

         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
            bHasTXVideo = true;

         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
            bHasRXVideo = true;

         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
            bHasTXData = true;

         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
            bHasRXData = true;
      }

      // Remove gaps in radio links (remove radio links that do not have a radio card associated anymore)
      for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
      {
         if ( bRadioLinksHaveInterfaces[i] )
            continue;

         // Unassign any cards that still use this radio link

         for( int card=0; card<nic_count; card++ )
            if ( pModel->radioInterfacesParams.interface_link_id[card] == i )
               pModel->radioInterfacesParams.interface_link_id[card] = -1;

         for( int k=i; k<pModel->radioLinksParams.links_count-1; k++ )
         {
            pModel->radioLinksParams.link_frequency[k] = pModel->radioLinksParams.link_frequency[k+1];
            pModel->radioLinksParams.link_capabilities_flags[k] = pModel->radioLinksParams.link_capabilities_flags[k+1];
            pModel->radioLinksParams.link_radio_flags[k] = pModel->radioLinksParams.link_radio_flags[k+1];
            pModel->radioLinksParams.link_datarates[k][0] = pModel->radioLinksParams.link_datarates[k+1][0];
            pModel->radioLinksParams.link_datarates[k][1] = pModel->radioLinksParams.link_datarates[k+1][1];
            pModel->radioLinksParams.bUplinkSameAsDownlink[k] = pModel->radioLinksParams.bUplinkSameAsDownlink[k+1];
            pModel->radioLinksParams.uplink_radio_flags[k] = pModel->radioLinksParams.uplink_radio_flags[k+1];
            pModel->radioLinksParams.uplink_datarates[k][0] = pModel->radioLinksParams.uplink_datarates[k+1][0];
            pModel->radioLinksParams.uplink_datarates[k][1] = pModel->radioLinksParams.uplink_datarates[k+1][1];

            for( int card=0; card<nic_count; card++ )
               if ( pModel->radioInterfacesParams.interface_link_id[card] == k+1 )
                  pModel->radioInterfacesParams.interface_link_id[card] = k;
         }
         pModel->radioLinksParams.links_count--;
      }

      // If not all capabilities are present, enable them on radio link 0
      if ( (! bHasTXData) || (! bHasTXVideo) || (! bHasRXData) || (! bHasRXVideo) )
      {
         pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
         pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
         pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
         pModel->radioLinksParams.link_capabilities_flags[0] &= (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
      }

      pModel->radioInterfacesParams.interfaces_count = nic_count;
      return true;
   }

   // Case: mix of new and removed radio interfaces

   log_line("[HW Radio Check] Some radio interfaces where removed and some new ones where added.");

   // Leave only old radio links that are still present, for all other new radio cards, create new radio links

      // Check which radio links are still active (have cards)
      bool bRadioLinksActive[MAX_RADIO_INTERFACES];
      bool bHasTXData = false;
      bool bHasRXData = false;
      bool bHasTXVideo = false;
      bool bHasRXVideo = false;

      for( int i=0; i<nic_count; i++ )
      {
         int nLinkId = pModel->radioInterfacesParams.interface_link_id[i];
         if ( nLinkId < 0 )
            continue;
         bRadioLinksActive[nLinkId] = true;
         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;

         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
            bHasTXVideo = true;

         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
            bHasRXVideo = true;

         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
            bHasTXData = true;

         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
         if ( pModel->radioLinksParams.link_capabilities_flags[nLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
            bHasRXData = true;
      }

      // Remove gaps in radio links (remove radio links that do not have a radio card anymore)
      for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
      {
         if ( bRadioLinksActive[i] )
            continue;
         for( int k=i; k<pModel->radioLinksParams.links_count-1; k++ )
         {
            pModel->radioLinksParams.link_frequency[k] = pModel->radioLinksParams.link_frequency[k+1];
            pModel->radioLinksParams.link_capabilities_flags[k] = pModel->radioLinksParams.link_capabilities_flags[k+1];
            pModel->radioLinksParams.link_radio_flags[k] = pModel->radioLinksParams.link_radio_flags[k+1];
            pModel->radioLinksParams.link_datarates[k][0] = pModel->radioLinksParams.link_datarates[k+1][0];
            pModel->radioLinksParams.link_datarates[k][1] = pModel->radioLinksParams.link_datarates[k+1][1];
            pModel->radioLinksParams.bUplinkSameAsDownlink[k] = pModel->radioLinksParams.bUplinkSameAsDownlink[k+1];
            pModel->radioLinksParams.uplink_radio_flags[k] = pModel->radioLinksParams.uplink_radio_flags[k+1];
            pModel->radioLinksParams.uplink_datarates[k][0] = pModel->radioLinksParams.uplink_datarates[k+1][0];
            pModel->radioLinksParams.uplink_datarates[k][1] = pModel->radioLinksParams.uplink_datarates[k+1][1];
         }
         pModel->radioLinksParams.links_count--;
      }

      // If not all capabilities are present, enable them on radio link 0
      if ( (! bHasTXData) || (! bHasTXVideo) || (! bHasRXData) || (! bHasRXVideo) )
      {
         pModel->radioLinksParams.link_capabilities_flags[0] = pModel->radioLinksParams.link_capabilities_flags[0] | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
         pModel->radioLinksParams.link_capabilities_flags[0] = pModel->radioLinksParams.link_capabilities_flags[0] | RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
         pModel->radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
         pModel->radioLinksParams.link_capabilities_flags[0] = pModel->radioLinksParams.link_capabilities_flags[0] & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
      }

      // Create radio links for the new cards

      // Check what default frequencies are used

      bDefault24Used = false;
      bDefault58Used = false;
      for( int i=0; i<iCountSameInterfaces; i++ )
      {
         if ( pModel->radioLinksParams.link_frequency[i] == DEFAULT_FREQUENCY )
            bDefault24Used = true;
         if ( pModel->radioLinksParams.link_frequency[i] == DEFAULT_FREQUENCY58 )
            bDefault58Used = true;
      }


   for( int i=0; i<nic_count; i++ )
   {
      int nLinkId = pModel->radioInterfacesParams.interface_link_id[i];
      if ( nLinkId >= 0 )
         continue;

      radio_hw_info_t* pNIC = hardware_get_radio_info(i);

         // Add a new radio link      
         pModel->radioLinksParams.link_radio_flags[pModel->radioLinksParams.links_count] = DEFAULT_RADIO_FRAMES_FLAGS;
         pModel->radioLinksParams.link_datarates[pModel->radioLinksParams.links_count][0] = DEFAULT_DATARATE_VIDEO;
         pModel->radioLinksParams.link_datarates[pModel->radioLinksParams.links_count][1] = DEFAULT_DATARATE_DATA;
         pModel->radioLinksParams.bUplinkSameAsDownlink[pModel->radioLinksParams.links_count] = 1;
         pModel->radioLinksParams.uplink_radio_flags[pModel->radioLinksParams.links_count] = DEFAULT_RADIO_FRAMES_FLAGS;
         pModel->radioLinksParams.uplink_datarates[pModel->radioLinksParams.links_count][0] = DEFAULT_DATARATE_VIDEO;
         pModel->radioLinksParams.uplink_datarates[pModel->radioLinksParams.links_count][1] = DEFAULT_DATARATE_DATA;

         // Populate the new radio interface info
         pModel->radioInterfacesParams.interface_card_model[i] = 0;
         pModel->radioInterfacesParams.interface_link_id[i] = -1;
         pModel->radioInterfacesParams.interface_current_frequency[i] = 0;
         pModel->radioInterfacesParams.interface_current_radio_flags[i] = 0;
         pModel->radioInterfacesParams.interface_datarates[i][0] = 0;
         pModel->radioInterfacesParams.interface_datarates[i][1] = 0;

         pModel->radioInterfacesParams.interface_type_and_driver[i] = 0;
         pModel->radioInterfacesParams.interface_supported_bands[i] = 0;
         pModel->radioInterfacesParams.interface_szMAC[i][0] = 0;
         pModel->radioInterfacesParams.interface_szPort[i][0] = 0;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
         pModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
         
         pModel->radioInterfacesParams.interface_supported_bands[i] = pNIC->supportedBands;
         pModel->radioInterfacesParams.interface_type_and_driver[i] = pNIC->typeAndDriver;
         if ( pNIC->isSupported )
            pModel->radioInterfacesParams.interface_type_and_driver[i] = pModel->radioInterfacesParams.interface_type_and_driver[i] | 0xFF0000;
         strcpy(pModel->radioInterfacesParams.interface_szMAC[i], pNIC->szMAC );
         strcpy(pModel->radioInterfacesParams.interface_szPort[i], pNIC->szUSBPort);

         pModel->radioInterfacesParams.interface_link_id[i] = pModel->radioLinksParams.links_count;
         pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] = DEFAULT_FREQUENCY;
         if ( pModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_58 )
         {
            if ( ! bDefault58Used )
            {
               pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] = DEFAULT_FREQUENCY58;
               bDefault58Used = true;
            }
            else
               pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] = DEFAULT_FREQUENCY58_2;
         }
         else
         {
            if ( ! bDefault24Used )
            {
               pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] = DEFAULT_FREQUENCY;
               bDefault24Used = true;
            }
            else
               pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count] = DEFAULT_FREQUENCY_2;
         }
         pModel->radioInterfacesParams.interface_current_frequency[i] = pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count];
         pModel->radioInterfacesParams.interface_current_radio_flags[i] = pModel->radioLinksParams.link_radio_flags[pModel->radioInterfacesParams.interface_link_id[pModel->radioLinksParams.links_count]];
         pModel->radioInterfacesParams.interface_datarates[i][0] = 0;
         pModel->radioInterfacesParams.interface_datarates[i][1] = 0;
         pModel->radioLinksParams.links_count++;

         log_line("[HW Radio Check] Added a new radio link (as disabled) (radio link %d) for radio interface %d, on %d Mhz", pModel->radioLinksParams.links_count, i+1, pModel->radioLinksParams.link_frequency[pModel->radioLinksParams.links_count-1]);
   }

   return true;
}
