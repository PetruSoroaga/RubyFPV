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
#include "../base/launchers.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"

#include "radio_utils.h"
#include "shared_vars.h"

// Not used anymore

bool radio_utils_check_radio()
{
   log_line("Checking radio modules...");

   int fRX = radio_open_interface_for_read(0, RADIO_PORT_ROUTER_DOWNLINK);
   if ( fRX < 0 )
   {
      log_softerror_and_alarm("RadioError: failed to open radio interface for read.");
      return false;
   }

   int fTX = radio_open_interface_for_write(0);
   if ( fTX <= 0 )
   {
      log_softerror_and_alarm("RadioError: failed to open radio interface for write.");
      radio_close_interface_for_read(0); 
      return false;
   }

   for( int i=0; i<5; i++ )
   {
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_RUBY;
   PH.packet_type =  PACKET_TYPE_RUBY_PING_CLOCK;
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = 0;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + 2*sizeof(u8);
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   u8 rawPacket[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));   
   int totalLength = radio_build_packet(rawPacket, packet, PH.total_length, RADIO_PORT_ROUTER_DOWNLINK, 0, 0, NULL);
   if ( 0 == radio_write_packet(0, rawPacket, totalLength) )
   {
      log_softerror_and_alarm("RadioError: Failed to write to radio interface.");
      radio_close_interface_for_read(0); 
      close(fTX);
      return false;
   }
   }

   fd_set setRX;
   FD_ZERO(&setRX);
   int maxfd = 0;
   struct timeval timeRXRadio;
   radio_hw_info_t* pNICRX = hardware_get_radio_info(fRX);

   log_line("Try read on fd: %d", pNICRX->monitor_interface_read.selectable_fd);

   for( int i=0; i<10; i++ )
   {
      FD_ZERO(&setRX);
      FD_SET(pNICRX->monitor_interface_read.selectable_fd, &setRX);
      if ( pNICRX->monitor_interface_read.selectable_fd > maxfd )
         maxfd = pNICRX->monitor_interface_read.selectable_fd;
     
      timeRXRadio.tv_sec = 0;
      timeRXRadio.tv_usec = 10; // 0.01 miliseconds timeout
      int nResult = select(maxfd+1, NULL, NULL, &setRX, &timeRXRadio);
      if ( nResult < 0 || nResult > 0 )
      {
         log_line("RadioError: Radio interfaces have broken up. Exception on radio handle. Reinitalizing everything.");
         radio_close_interfaces_for_read(); 
         close(fTX);
         return false;
      }

      FD_ZERO(&setRX);
      maxfd = 0;

      FD_SET(pNICRX->monitor_interface_read.selectable_fd, &setRX);
      if ( pNICRX->monitor_interface_read.selectable_fd > maxfd )
         maxfd = pNICRX->monitor_interface_read.selectable_fd;
     
      timeRXRadio.tv_sec = 0;
      timeRXRadio.tv_usec = 10000; // 10 miliseconds timeout

      nResult = select(maxfd+1, &setRX, NULL, NULL, &timeRXRadio);
      if ( nResult < 0 )
      {
         log_line("RadioError: Radio interfaces have broken up. Exception on radio handle. Reinitalizing everything.");
         radio_close_interfaces_for_read(); 
         close(fTX);
         return false;
      }
      if( nResult > 0 )
      if( 0 != FD_ISSET(pNICRX->monitor_interface_read.selectable_fd, &setRX) )
      {
         log_line("RadioCheck: Received a packet.");
         int packetLength = 0;
         u8* pBuffer = NULL;
         pBuffer = radio_process_wlan_data_in(fRX, &packetLength);
         if ( NULL == pBuffer && (radio_get_last_read_error_code() == RADIO_READ_ERROR_TIMEDOUT) )
         {
            log_softerror_and_alarm("Checking radio: Rx ppcap read timedout reading a packet.");
            radio_close_interfaces_for_read(); 
            close(fTX);
            log_line("Checking radio modules: succeeded (with read timeout).");
            return true;
         }

         if ( pBuffer == NULL && radio_interfaces_broken() )
         {
            log_line("RadioError:Radio interfaces returned a NULL buffer.");
            if ( radio_interfaces_broken() )
            {
               log_line("RadioError:Radio interfaces have broken up. Reinitaliziging everything.");
               radio_close_interfaces_for_read(); 
               close(fTX);
               return false;
            }
         }
         else
         {
            radio_close_interfaces_for_read(); 
            close(fTX);
            log_line("Checking radio modules: succeeded.");
            return true;
         }
      }

      hardware_sleep_ms(5);
   }
   radio_close_interface_for_read(0); 
   close(fTX);
   log_line("Checking radio modules: timedout.");
   return false;
}


// Returns true if configuration has been updated

bool configure_radio_interfaces_for_current_model(Model* pModel)
{
   bool bMissmatch = false;

   log_line("-----------------------------------------------------------------");

   if ( NULL == pModel )
   {
      log_line("Configuring all radio interfaces (%d radio interfaces)", hardware_get_radio_interfaces_count());
      log_error_and_alarm("INVALID MODEL parameter (no model, NULL)");
      log_line("-----------------------------------------------------------------");
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
   if ( pModel->relay_params.isRelayEnabledOnRadioLinkId > 0 )
      log_line("One relay link is enabled on radio link %d, on %s", pModel->relay_params.isRelayEnabledOnRadioLinkId, str_format_frequency(pModel->relay_params.uRelayFrequency));

   for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
   {
      u32 uRadioLinkFrequency = pModel->radioLinksParams.link_frequency[i];
      if ( pModel->relay_params.isRelayEnabledOnRadioLinkId-1 == i )
         uRadioLinkFrequency = pModel->relay_params.uRelayFrequency;

      if ( uRadioLinkFrequency <= 0 )
         continue;
      if ( pModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         log_softerror_and_alarm("Radio Link %d is disabled!", i+1 );
         continue;
      }

      for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
      {
         if ( pModel->radioInterfacesParams.interface_link_id[k] != i )
            continue;
         if ( pModel->radioInterfacesParams.interface_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         {
            log_softerror_and_alarm("Radio Interface %d (assigned to radio link %d) is disabled!", k+1, i+1 );
            continue;
         }
         if ( ! hardware_radioindex_supports_frequency(k, uRadioLinkFrequency ) )
         {
            log_softerror_and_alarm("Radio interface %d (assigned to radio link %d) does not support radio link frequency %s!", k+1, i+1, str_format_frequency(uRadioLinkFrequency) );
            bMissmatch = true;
            continue;
         }

         launch_set_frequency(pModel, k, uRadioLinkFrequency, g_pProcessStats);
         if ( pModel->relay_params.isRelayEnabledOnRadioLinkId-1 == i )
            log_line("Set radio interface %d to frequency %s (assigned to radio link %d in relay mode)", k+1, str_format_frequency(uRadioLinkFrequency), i+1 );
         else
            log_line("Set radio interface %d to frequency %s (assigned to radio link %d)", k+1, str_format_frequency(uRadioLinkFrequency), i+1 );
         pModel->radioInterfacesParams.interface_current_frequency[k] = uRadioLinkFrequency;
      }
   }

   if ( bMissmatch )
   if ( 1 == pModel->radioLinksParams.links_count )
   if ( 1 == pModel->radioInterfacesParams.interfaces_count )
   {
      log_line("There was a missmatch of frequency configuration for the single radio link present on vehicle. Reseting it to default.");
      pModel->radioLinksParams.link_frequency[0] = DEFAULT_FREQUENCY;
      pModel->radioInterfacesParams.interface_current_frequency[0] = DEFAULT_FREQUENCY;
      launch_set_frequency(pModel, 0, pModel->radioLinksParams.link_frequency[0], g_pProcessStats);
      log_line("Set radio interface 1 to link 1 frequency %s", str_format_frequency(pModel->radioLinksParams.link_frequency[0]) );
   }
   hardware_save_radio_info();
   hardware_sleep_ms(50);
   log_line("-----------------------------------------------------------------");

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

   if ( pModel->relay_params.isRelayEnabledOnRadioLinkId > 0 )
      log_line("Current vehicle radio configuration: %d radio links, 1 relay link:", pModel->radioLinksParams.links_count-1);
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
               snprintf(szInfo, sizeof(szInfo), ", %d", k+1);
            else
               snprintf(szInfo, sizeof(szInfo), "%d", k+1);
            strlcat(szBuff, szInfo, sizeof(szBuff));
         }
      }
      if ( pModel->relay_params.isRelayEnabledOnRadioLinkId - 1 == i )
         strcpy(szPrefix, "Relay ");

      if ( pModel->relay_params.isRelayEnabledOnRadioLinkId - 1 == i )
         log_line("* %sRadio Link %d Info:  %s, radio interface(s) assigned to this link: [%s]", szPrefix, i+1, str_format_frequency(pModel->relay_params.uRelayFrequency), szBuff);
      else
         log_line("* %sRadio Link %d Info:  %s, radio interface(s) assigned to this link: [%s]", szPrefix, i+1, str_format_frequency(pModel->radioLinksParams.link_frequency[i]), szBuff);
      
      szBuff[0] = 0;

      str_get_radio_capabilities_description(pModel->radioLinksParams.link_capabilities_flags[i], szBuff);
      str_get_radio_frame_flags_description(pModel->radioLinksParams.link_radio_flags[i], szBuff2); 
      log_line("* %sRadio Link %d Capab: %s, Radio flags: %s", szPrefix, i+1, szBuff, szBuff2);
      str_getDataRateDescription(pModel->radioLinksParams.link_datarates[i][0], szBuff);
      str_getDataRateDescription(pModel->radioLinksParams.link_datarates[i][1], szBuff2);
      str_getDataRateDescription(pModel->radioLinksParams.uplink_datarates[i][0], szBuff3);
      str_getDataRateDescription(pModel->radioLinksParams.uplink_datarates[i][1], szBuff4);
      if ( pModel->radioLinksParams.bUplinkSameAsDownlink[i] )
         snprintf(szBuff5, sizeof(szBuff5), "video: %s, data: %s, same for uplink video: %s, data: %s;", szBuff, szBuff2, szBuff3, szBuff4);
      else
         snprintf(szBuff5, sizeof(szBuff5), "video: %s, data: %s, uplink video: %s, data: %s;", szBuff, szBuff2, szBuff3, szBuff4);
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
      log_line("* %sRadio int %d: %s [%s] %s, current frequency: %s, assigned to radio link %d", szPrefix, i+1, pRadioInfo->szUSBPort, str_get_radio_card_model_string(pModel->radioInterfacesParams.interface_card_model[i]), pRadioInfo->szDriver, str_format_frequency(pRadioInfo->uCurrentFrequency), pModel->radioInterfacesParams.interface_link_id[i]+1);
      log_line("* %sRadio int %d Capab: %s, Radio flags: %s", szPrefix, i+1, szBuff, szBuff2);
      char szDR1[32];
      char szDR2[32];
      if ( pModel->radioInterfacesParams.interface_datarates[i][0] != 0 )
         str_getDataRateDescription(pModel->radioInterfacesParams.interface_datarates[i][0], szDR1);
      else
         strcpy(szDR1, "auto");

      if ( pModel->radioInterfacesParams.interface_datarates[i][1] != 0 )
         str_getDataRateDescription(pModel->radioInterfacesParams.interface_datarates[i][1], szDR2);
      else
         strcpy(szDR2, "auto");
      log_line("* %sRadio int %d Datarates (video/data): %s / %s", szPrefix, i+1, szDR1, szDR2);
      log_line("");
   }
   log_line("=====================================================================================");
}