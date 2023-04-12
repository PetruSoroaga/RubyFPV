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
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"
#include "../base/ruby_ipc.h"
#include "relay_rx.h"
#include "relay_tx.h"
#include "ruby_rt_vehicle.h"
#include "radio_utils.h"
#include "shared_vars.h"
#include "timers.h"

// It's a pointer to: type_uplink_rx_info_stats s_UplinkInfoRxStats[MAX_RADIO_INTERFACES];
type_uplink_rx_info_stats* s_pRelayRxInfoStats = NULL;

void relay_set_rx_info_stats(type_uplink_rx_info_stats* pUplinkStats)
{
   s_pRelayRxInfoStats = pUplinkStats;
}


void relay_process_received_single_radio_packet_from_controller_to_relayed_vehicle(int iRadioInterfaceIndex, u8* pBufferData, int iBufferLength)
{
   relay_send_single_packet_to_relayed_vehicle(pBufferData, iBufferLength);
}


void relay_process_received_radio_packet_from_relayed_vehicle(int iRadioLink, int iRadioInterfaceIndex, u8* pBufferData, int iBufferLength)
{
   //log_line("Received packet from relayed vehicle on radio interface %d, %d bytes", iRadioInterfaceIndex+1, iBufferLength);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioRxTime = g_TimeNow;

   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
      return;

   type_uplink_rx_info_stats* pRxInfoStats = NULL;
   if ( NULL != s_pRelayRxInfoStats )
      pRxInfoStats = (type_uplink_rx_info_stats*)(((u32*)s_pRelayRxInfoStats) + iRadioInterfaceIndex * sizeof(type_uplink_rx_info_stats));

   u8* pData = pBufferData;
   int nLength = iBufferLength;
   int iCountReceivedPackets = 0;
   bool bIsFullComposedPacketOkToForward = true;
   bool bPacketContainsDataToForward = false;
   bool bPacketContainsRubyTelemetry = false;

   while ( nLength > 0 )
   {
      t_packet_header* pPH = (t_packet_header*)pData;

      int bCRCOk = 0;
      int iPacketLength = packet_process_and_check(iRadioInterfaceIndex, pData, nLength, &bCRCOk);

      if ( iPacketLength <= 0 )
         return;

      int nRes = radio_stats_update_on_packet_received(&g_SM_RadioStats, g_TimeNow, iRadioInterfaceIndex, pData, iPacketLength, (int)bCRCOk);
      
      if ( (nRes != 1) || (pPH->vehicle_id_src != g_pCurrentModel->relay_params.uRelayVehicleId) )
      {
         pData += pPH->total_length;
         nLength -= pPH->total_length;
         if ( pPH->vehicle_id_src != g_pCurrentModel->relay_params.uRelayVehicleId )
         {
            if ( (NULL != pRxInfoStats) && (g_TimeNow > pRxInfoStats->timeLastLogWrongRxPacket + 2000) )
            {
               if ( NULL != pRxInfoStats )
                  pRxInfoStats->timeLastLogWrongRxPacket = g_TimeNow;
               log_softerror_and_alarm("[Relaying] Received radio packet on the relay link from a different vehicle than the relayed vehicle (received VID: %u, current main VID: %u, relayed VID: %u)", pPH->vehicle_id_src, g_pCurrentModel->vehicle_id, g_pCurrentModel->relay_params.uRelayVehicleId );
            }
         }
         bIsFullComposedPacketOkToForward = false;
         continue;
      }

      if ( NULL != pRxInfoStats )
      {
         pRxInfoStats->lastReceivedDBM = pRadioInfo->monitor_interface_read.radioInfo.nDbm;
         pRxInfoStats->lastReceivedDataRate = pRadioInfo->monitor_interface_read.radioInfo.nRate;
      }

      iCountReceivedPackets++;
  
      if ( g_pCurrentModel->relay_params.uRelayFlags & RELAY_FLAGS_VIDEO )
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_VIDEO )
         bPacketContainsDataToForward = true;
   
      if ( g_pCurrentModel->relay_params.uRelayFlags & RELAY_FLAGS_TELEMETRY )
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
         bPacketContainsDataToForward = true;

      // Ruby telemetry is always forwarded on the relay link
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
      if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
      {
         bPacketContainsDataToForward = true;
         bPacketContainsRubyTelemetry = true;
      }
      pData += pPH->total_length;
      nLength -= pPH->total_length;
   }

   if ( (! bIsFullComposedPacketOkToForward) || (! bPacketContainsDataToForward) )
      return;

   if ( (g_pCurrentModel->relay_params.uCurrentRelayMode == RELAY_MODE_NONE) &&
        (!bPacketContainsRubyTelemetry) )
      return;
     
   // Forward the full composed packet to the controller
   
   relay_send_packet_to_controller(pBufferData, iBufferLength);
}


void relay_on_relay_params_changed()
{
   if ( (NULL == g_pCurrentModel) || (0 == g_TimeLastNotificationRelayParamsChanged) )
      return;

   if ( g_TimeNow < g_TimeLastNotificationRelayParamsChanged + 1000 )
      return;

   g_TimeLastNotificationRelayParamsChanged = 0;

   log_line("Processing notification that relay parameters where updated by user command...");
   close_radio_interfaces();

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   int iRelayRadioLink = g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId - 1;
   if ( iRelayRadioLink < 0 || iRelayRadioLink >= g_pCurrentModel->radioLinksParams.links_count )
   {
      // Relaying is disabled. Set all radio links and interfaces as active

      log_line("Relaying is disabled. Set all radio links and interfaces as usable for radio links.");

      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
         g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);

      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
   }
   else
   {
      // Relaying is enabled on a radio link.

      log_line("Relaying is enabled on radio link %d.", iRelayRadioLink+1);
      g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRelayRadioLink] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] != iRelayRadioLink )
            continue;

         log_line("Mark radio interface %d as used for relaying.", i+1);
         g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
      }
   }

   g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);

   configure_radio_interfaces_for_current_model(g_pCurrentModel);
   
   open_radio_interfaces();

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   log_line("Done processing notification that relay parameters where updated by user command. Notify all local components about new radio config.");
   
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED;
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY | (MODEL_CHANGED_GENERIC<<8);
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);

   packet_compute_crc((u8*)&PH, PH.total_length);
   
   ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);
         
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void relay_on_relay_mode_changed(u8 uNewMode)
{
   g_TimeLastNotificationRelayParamsChanged = 0;

   log_line("New relay mode: %d", uNewMode);
}

void relay_on_relay_flags_changed(u32 uNewFlags)
{

}