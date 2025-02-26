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

#include "radio_links.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/hardware_radio_serial.h"
#include "../base/radio_utils.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../radio/radio_tx.h"
#include "shared_vars.h"
#include "timers.h"


int radio_links_open_rxtx_radio_interfaces()
{
   log_line("OPENING INTERFACES BEGIN =========================================================");
   log_line("Opening RX/TX radio interfaces...");
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      log_line("Relaying is enabled on radio link %d on frequency: %s.", g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId+1, str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequencyKhz));

   int countOpenedForRead = 0;
   int countOpenedForWrite = 0;
   int iCountSikInterfacesOpened = 0;
   int iCountSerialInterfacesOpened = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId = iRadioLinkId;
      g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = iRadioLinkId;
      g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }

   // Init RX interfaces

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( pRadioHWInfo->lastFrequencySetFailed )
         continue;
      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      if ( iRadioLinkId < 0 )
      {
         log_softerror_and_alarm("No radio link is assigned to radio interface %d", i+1);
         continue;
      }
      if ( iRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
      {
         log_softerror_and_alarm("Invalid radio link (%d of %d) is assigned to radio interface %d", iRadioLinkId+1, g_pCurrentModel->radioLinksParams.links_count, i+1);
         continue;
      }

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         continue;

      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         continue;

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iRadioLinkId )
         log_line("Open radio interface %d (%s) for radio link %d in relay mode ...", i+1, pRadioHWInfo->szName, iRadioLinkId+1);
      else
         log_line("Open radio interface %d (%s) for radio link %d ...", i+1, pRadioHWInfo->szName, iRadioLinkId+1);

      if ( (pRadioHWInfo->iRadioType == RADIO_TYPE_ATHEROS) ||
           (pRadioHWInfo->iRadioType == RADIO_TYPE_RALINK) )
      {
         int nRateTx = g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iRadioLinkId];
         radio_utils_set_datarate_atheros(g_pCurrentModel, i, nRateTx, 0);
      }

      if ( (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) ||
           (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         {
            if ( hardware_radio_sik_open_for_read_write(i) > 0 )
            {
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               countOpenedForRead++;
               iCountSikInterfacesOpened++;
            }
         }
         else if ( hardware_radio_is_elrs_radio(pRadioHWInfo) )
         {
            if ( hardware_radio_serial_open_for_read_write(i) > 0 )
            {
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               countOpenedForRead++;
               iCountSerialInterfacesOpened++;
            }
            radio_tx_set_serial_packet_size(i, DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE);
         }
         else
         {
            if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
               radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK);
            else
               radio_open_interface_for_read(i, RADIO_PORT_ROUTER_UPLINK);

            g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;      
            countOpenedForRead++;
         }
      }
   }

   if ( countOpenedForRead == 0 )
   {
      log_error_and_alarm("Failed to find or open any RX interface for receiving data.");
      radio_links_close_rxtx_radio_interfaces();
      return -1;
   }


   // Init TX interfaces

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( pRadioHWInfo->lastFrequencySetFailed )
         continue;
      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      if ( iRadioLinkId < 0 )
      {
         log_softerror_and_alarm("No radio link is assigned to radio interface %d", i+1);
         continue;
      }
      if ( iRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
      {
         log_softerror_and_alarm("Invalid radio link (%d of %d) is assigned to radio interface %d", iRadioLinkId+1, g_pCurrentModel->radioLinksParams.links_count, i+1);
         continue;
      }

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) ||
           (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         {
            if ( ! g_SM_RadioStats.radio_interfaces[i].openedForRead )
            {
               if ( hardware_radio_sik_open_for_read_write(i) > 0 )
               {
                 g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
                 countOpenedForWrite++;
                 iCountSikInterfacesOpened++;
               }
            }
            else
            {
               g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
               countOpenedForWrite++;
            }
         }
         else if ( hardware_radio_is_elrs_radio(pRadioHWInfo) )
         {
            if ( ! g_SM_RadioStats.radio_interfaces[i].openedForRead )
            {
               if ( hardware_radio_serial_open_for_read_write(i) > 0 )
               {
                 g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
                 countOpenedForWrite++;
                 iCountSerialInterfacesOpened++;
               }
            }
            else
            {
               g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
               countOpenedForWrite++;
            }
         }
         else
         {
            radio_open_interface_for_write(i);
            g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
            countOpenedForWrite++;
         }
      }
   }

   if ( (0 < iCountSikInterfacesOpened) || (0 < iCountSerialInterfacesOpened) )
   {
      radio_tx_set_sik_packet_size(g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      radio_tx_start_tx_thread();
   }

   if ( 0 == countOpenedForWrite )
   {
      log_error_and_alarm("Can't find any TX interfaces for video/data or failed to init it.");
      radio_links_close_rxtx_radio_interfaces();
      return -1;
   }

   log_line("Opening RX/TX radio interfaces complete. %d interfaces for RX, %d interfaces for TX :", countOpenedForRead, countOpenedForWrite);
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      char szFlags[128];
      szFlags[0] = 0;
      str_get_radio_capabilities_description(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i], szFlags);

      if ( pRadioHWInfo->openedForRead && pRadioHWInfo->openedForWrite )
         log_line(" * Interface %s, %s, %s opened for read/write, flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFlags );
      else if ( pRadioHWInfo->openedForRead )
         log_line(" * Interface %s, %s, %s opened for read, flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFlags );
      else if ( pRadioHWInfo->openedForWrite )
         log_line(" * Interface %s, %s, %s opened for write, flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFlags );
      else
         log_line(" * Interface %s, %s, %s not used. Flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFlags );
   }
   log_line("OPENING INTERFACES END ============================================================");

   g_pCurrentModel->logVehicleRadioInfo();

   log_line("Initializing video data tx...");
   if ( ! process_data_tx_video_init() )
   {
      log_error_and_alarm("Failed to initialize video data tx processor.");
      radio_links_close_rxtx_radio_interfaces();
      return -1;
   }
   log_line("Initialized video data tx.");
   return 0;
}


void radio_links_close_rxtx_radio_interfaces()
{
   log_line("Closing all radio interfaces (rx/tx).");

   radio_tx_mark_quit();
   hardware_sleep_ms(10);
   radio_tx_stop_tx_thread();

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         hardware_radio_sik_close(i);
      else if ( hardware_radio_is_serial_radio(pRadioHWInfo) )
         hardware_radio_serial_close(i);
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( pRadioHWInfo->openedForWrite )
         radio_close_interface_for_write(i);
   }

   radio_close_interfaces_for_read();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }
   log_line("Closed all radio interfaces (rx/tx)."); 
}


bool radio_links_apply_settings(Model* pModel, int iRadioLink, type_radio_links_parameters* pRadioLinkParamsOld, type_radio_links_parameters* pRadioLinkParamsNew)
{
   if ( (NULL == pModel) || (NULL == pRadioLinkParamsNew) )
      return false;
   if ( (iRadioLink < 0) || (iRadioLink >= pModel->radioLinksParams.links_count) )
      return false;

   // Update frequencies if needed
   // Update HT20/HT40 if needed

   bool bUpdateFreq = false;
   if ( pRadioLinkParamsOld->link_frequency_khz[iRadioLink] != pRadioLinkParamsNew->link_frequency_khz[iRadioLink] )
      bUpdateFreq = true;
   if ( (pRadioLinkParamsOld->link_radio_flags[iRadioLink] & RADIO_FLAG_HT40_VEHICLE) != 
        (pRadioLinkParamsNew->link_radio_flags[iRadioLink] & RADIO_FLAG_HT40_VEHICLE) )
      bUpdateFreq = true;

   if ( bUpdateFreq )     
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
            continue;
         if ( iRadioLink != pModel->radioInterfacesParams.interface_link_id[i] )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;

         if ( ! hardware_radioindex_supports_frequency(i, pRadioLinkParamsNew->link_frequency_khz[iRadioLink]) )
            continue;
         radio_utils_set_interface_frequency(pModel, i, iRadioLink, pRadioLinkParamsNew->link_frequency_khz[iRadioLink], g_pProcessStats, 0);
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, pRadioLinkParamsNew->link_frequency_khz[iRadioLink]);
      }

      hardware_save_radio_info();
   }

   // Apply data rates

   // If downlink data rate for an Atheros card has changed, update it.
      
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      
      if ( (pRadioHWInfo->iRadioType != RADIO_TYPE_ATHEROS) &&
           (pRadioHWInfo->iRadioType != RADIO_TYPE_RALINK) )
         continue;

      int nRateTx = pRadioLinkParamsNew->link_datarate_video_bps[iRadioLink];
      update_atheros_card_datarate(pModel, i, nRateTx, g_pProcessStats);
      g_TimeNow = get_current_timestamp_ms();
   }

   // Radio flags are applied on the fly, when sending each radio packet
   
   return true;
}
