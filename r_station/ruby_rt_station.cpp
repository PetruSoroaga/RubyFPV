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
#include <stdio.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/commands.h"
#include "../base/encr.h"
#include "../base/shared_mem.h"
#include "../base/models.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopacketsqueue.h"
#include "../base/controller_utils.h"
#include "../base/core_plugins_settings.h"

#include "shared_vars.h"
#include "links_utils.h"
#include "processor_rx_audio.h"
#include "processor_rx_video.h"
#include "processor_rx_video_forward.h"
#include "process_radio_in_packets.h"
#include "packets_utils.h"
#include "video_link_adaptive.h"
#include "video_link_keyframe.h"

#include "timers.h"

int s_iFailedInitRadioInterface = -1;
u32 s_TimeLastPipeCheck = 0;

u8 s_BufferCommands[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeBufferCommands[MAX_PACKET_TOTAL_SIZE];
int s_PipeBufferCommandsPos = 0;

u8 s_BufferTelemetryUplink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeBufferTelemetryUplink[MAX_PACKET_TOTAL_SIZE];
int s_PipeBufferTelemetryUplinkPos = 0;  

u8 s_BufferRCUplink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeBufferRCUplink[MAX_PACKET_TOTAL_SIZE];
int s_PipeBufferRCUplinkPos = 0;  

t_packet_queue s_QueueRadioPackets;
t_packet_queue s_QueueControlPackets;

u32 s_debugLastFPSTime = 0;
u32 s_debugFramesCount = 0; 

u32 s_uLastPairingRequestSendTime = 0;
u32 s_uPairingRequestSendIntervalMs = 50;
u32 s_uPairingRequestsSentCount = 0;

static int s_iCountCPULoopOverflows = 0;


void _broadcast_radio_interface_init_failed(int iInterfaceIndex)
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROLLER_RADIO_INTERFACE_FAILED_TO_INITIALIZE;
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = iInterfaceIndex;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);
   PH.extra_flags = 0;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   packet_compute_crc(buffer, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
      log_softerror_and_alarm("No pipe to central to send message to.");

   log_line("Sent message to central that radio interface %d failed to initialize.", iInterfaceIndex+1);
}

void _close_rxtx_radio_interfaces()
{
   log_line("Closing all radio interfaces (rx/tx).");

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( pNICInfo->openedForWrite )
         radio_close_interface_for_write(i);
   }

   radio_close_interfaces_for_read();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_Local_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_Local_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }
   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Closed all radio interfaces (rx/tx)."); 
}

void _open_rxtx_radio_interfaces_for_search( int iSearchFreq )
{
   log_line("=========================================================");
   log_line("Opening RX/TX radio interfaces for search (%d Mhz)...", iSearchFreq);

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_Local_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_Local_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }

   int iCountOpenRead = 0;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(i);
      if ( NULL == pNIC )
         continue;
      u32 flags = controllerGetCardFlags(pNIC->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pNIC->szMAC) )
         continue;

      if ( 0 == hardware_radio_supports_frequency(pNIC, iSearchFreq ) )
         continue;

      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      {
         if ( radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK) > 0 )
         {
            log_line("Opened radio interface %d for read: USB port %s %s %s", i+1, pNIC->szUSBPort, str_get_radio_type_description(pNIC->typeAndDriver), pNIC->szMAC);
            g_Local_RadioStats.radio_interfaces[i].openedForRead = 1;
            iCountOpenRead++;
         }
         else
            s_iFailedInitRadioInterface = i;
      }
   }
   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Opening RX/TX radio interfaces for search complete. %d interfaces for RX", iCountOpenRead);
   log_line("===================================================================");
}


void _open_rxtx_radio_interfaces()
{
   log_line("=========================================================");
   log_line("Opening RX/TX radio interfaces for current vehicle ...");

   if ( g_bSearching || (NULL == g_pCurrentModel) )
   {
      log_error_and_alarm("Invalid parameters for opening radio interfaces");
      return;
   }

   int totalCountForRead = 0;
   int totalCountForWrite = 0;

   int countOpenedForReadForRadioLink[MAX_RADIO_INTERFACES];
   int countOpenedForWriteForRadioLink[MAX_RADIO_INTERFACES];

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      countOpenedForReadForRadioLink[i] = 0;
      countOpenedForWriteForRadioLink[i] = 0;
      g_Local_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_Local_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);

      if ( controllerIsCardDisabled(pNICInfo->szMAC) )
         continue;

      int nRadioLinkId = g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId;
      if ( nRadioLinkId < 0 || nRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
         continue;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[nRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( NULL != pNICInfo && ((pNICInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS) )
      {
         int nRateTx = 0;
         nRateTx = controllerGetCardDataRate(pNICInfo->szMAC); // Returns 0 if radio link datarate must be used (no custom datarate set for this radio card);
         if ( 0 == nRateTx && NULL != g_pCurrentModel )
            nRateTx = g_pCurrentModel->radioLinksParams.link_datarates[nRadioLinkId][1];         
         launch_set_datarate_atheros(NULL, i, nRateTx);
      }

      u32 cardFlags = controllerGetCardFlags(pNICInfo->szMAC);

      if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) ||
           (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         if ( radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK) > 0 )
         {
            g_Local_RadioStats.radio_interfaces[i].openedForRead = 1;
            countOpenedForReadForRadioLink[nRadioLinkId]++;
            totalCountForRead++;
         }
         else
            s_iFailedInitRadioInterface = i;
      }

      if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) ||
           (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         if ( radio_open_interface_for_write(i) > 0 )
         {
            g_Local_RadioStats.radio_interfaces[i].openedForWrite = 1;
            countOpenedForWriteForRadioLink[nRadioLinkId]++;
            totalCountForWrite++;
         }
         else
            s_iFailedInitRadioInterface = i;
      }
   }

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Opening RX/TX radio interfaces complete. %d interfaces opened for RX, %d interfaces opened for TX:", totalCountForRead, totalCountForWrite);

   if ( totalCountForRead == 0 )
   {
      log_error_and_alarm("Failed to find or open any RX interface for receiving data.");
      _close_rxtx_radio_interfaces();
      return;
   }

   if ( 0 == totalCountForWrite )
   {
      log_error_and_alarm("Can't find any TX interfaces for sending data.");
      _close_rxtx_radio_interfaces();
      return;
   }

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( 0 == countOpenedForReadForRadioLink[i] )
         log_error_and_alarm("Failed to find or open any RX interface for receiving data on radio link %d.", i+1);
      if ( 0 == countOpenedForWriteForRadioLink[i] )
         log_error_and_alarm("Failed to find or open any TX interface for sending data on radio link %d.", i+1);

      if ( 0 == countOpenedForReadForRadioLink[i] && 0 == countOpenedForWriteForRadioLink[i] )
         send_alarm_to_central(ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK,i,1);
   }

   log_line("Opened radio interfaces:");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(i);
      if ( NULL == pNIC )
         continue;
      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNIC->szMAC);
   
      int nRadioLinkId = g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId;      
      char szFlags[128];
      szFlags[0] = 0;
      u32 uFlags = controllerGetCardFlags(pNIC->szMAC);
      str_get_radio_capabilities_description(uFlags, szFlags);

      char szType[128];
      strcpy(szType, pNIC->szDriver);
      if ( NULL != pCardInfo )
         strcpy(szType, str_get_radio_card_model_string(pCardInfo->cardModel));

      if ( pNIC->openedForRead && pNIC->openedForWrite )
         log_line(" * Radio Interface %d, %s, %s, %d Mhz, radio link %d, opened for read/write, flags: %s", i+1, pNIC->szName, szType, pNIC->currentFrequency, nRadioLinkId, szFlags );
      else if ( pNIC->openedForRead )
         log_line(" * Radio Interface %d, %s, %s, %d Mhz, radio link %d, opened for read, flags: %s", i+1, pNIC->szName, szType, pNIC->currentFrequency, nRadioLinkId, szFlags );
      else if ( pNIC->openedForWrite )
         log_line(" * Radio Interface %d, %s, %s, %d Mhz, radio link %d, opened for write, flags: %s", i+1, pNIC->szName, szType, pNIC->currentFrequency, nRadioLinkId, szFlags );
      else
         log_line(" * Radio Interface %d, %s, %s, %d Mhz not used. Flags: %s", i+1, pNIC->szName, szType, pNIC->currentFrequency, szFlags );
   }

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Finished opening RX/TX radio interfaces.");
   log_line("==================================================================="); 
}

void _compute_radio_interfaces_assignment()
{
   log_line("------------------------------------------------------------------");
   if ( g_bSearching || (NULL == g_pCurrentModel) )
   {
      log_error_and_alarm("Invalid parameters for assigning radio interfaces");
      return;
   }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId = -1;

   // Model with a single radio link

   if ( 1 == g_pCurrentModel->radioLinksParams.links_count )
   {
      log_line("Computing radio interfaces assignment to vehicle radio link (1 radio link)");
      bool bAnyAssigned = false;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( controllerIsCardDisabled(pNICInfo->szMAC) )
         {
            log_line("  * Radio interface %d is disabled, do not assign.", i+1);
            continue;
         }
         if ( ! hardware_radio_supports_frequency(pNICInfo, g_pCurrentModel->radioLinksParams.link_frequency[0]) )
         {
            log_line("  * Radio interface %d does not support %d Mhz, do not assign.", i+1, g_pCurrentModel->radioLinksParams.link_frequency[0]);
            continue;
         }
         bAnyAssigned = true;
         g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId = 0;

         t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
         if ( NULL != pCardInfo )  
            log_line("  * Assigned radio interface %d (%s) to radio link %d", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), 1);
         else
            log_line("  * Assigned radio interface %d (%s) to radio link %d", i+1, "Unknown Type", 1);
      }
      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));
      if ( ! bAnyAssigned )
         send_alarm_to_central(ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK,0,1);
      log_line("Done computing radio interfaces assignment to radio links.");
      log_line("------------------------------------------------------------------");
      return;
   }

   log_line("Computing radio interfaces assignment to radio links (%d radio links)", g_pCurrentModel->radioLinksParams.links_count);

   // Check what radio links are supported by each radio interface.

   bool bInterfaceSupportsLink[MAX_RADIO_INTERFACES][MAX_RADIO_INTERFACES];
   int iInterfaceSupportedLinksCount[MAX_RADIO_INTERFACES];
   int iInterfaceSupportedLinkId[MAX_RADIO_INTERFACES];

   bool bInterfaceWasAssigned[MAX_RADIO_INTERFACES];
   bool bLinkWasAssigned[MAX_RADIO_INTERFACES];

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
      bInterfaceSupportsLink[i][k] = false;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      iInterfaceSupportedLinksCount[i] = 0;
      iInterfaceSupportedLinkId[i] = -1;
      bInterfaceWasAssigned[i] = false;
      bLinkWasAssigned[i] = false;
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo || controllerIsCardDisabled(pNICInfo->szMAC) )
         continue;

      u32 cardFlags = controllerGetCardFlags(pNICInfo->szMAC);

      for( int k=0; k<g_pCurrentModel->radioLinksParams.links_count; k++ )
      {
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;

         // Uplink type radio link and RX only radio interface
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         if ( ! (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
            continue;

         // Downlink type radio link and TX only radio interface
         if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
         if ( ! (cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
            continue;

         if ( hardware_radio_supports_frequency(pNICInfo, g_pCurrentModel->radioLinksParams.link_frequency[k]) )
         {
            bInterfaceSupportsLink[i][k] = true;
            iInterfaceSupportedLinkId[i] = k;
            iInterfaceSupportedLinksCount[i]++;
         }
      }
   }

   // Assign first the radio interfaces that support only a single radio link

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo || controllerIsCardDisabled(pNICInfo->szMAC) )
         continue;
      if ( iInterfaceSupportedLinksCount[i] != 1 )
         continue;
      if ( iInterfaceSupportedLinkId[i] < 0 || iInterfaceSupportedLinkId[i] >= g_pCurrentModel->radioLinksParams.links_count )
         continue;

      g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId = iInterfaceSupportedLinkId[i];
      bInterfaceWasAssigned[i] = true;
      bLinkWasAssigned[iInterfaceSupportedLinkId[i]] = true;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
      if ( NULL != pCardInfo )  
         log_line("  * A) Assigned radio interface %d (%s) to radio link %d", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), iInterfaceSupportedLinkId[i]+1);
      else
         log_line("  * A) Assigned radio interface %d (%s) to radio link %d", i+1, "Unknown Type", iInterfaceSupportedLinkId[i]+1);
   }

   // Assign alternativelly each remaining radio interfaces to one radio link

   int iLinkId = 0;
   
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo || controllerIsCardDisabled(pNICInfo->szMAC) )
         continue;
      if ( iInterfaceSupportedLinksCount[i] < 2 )
         continue;
      int k=0;
      do
      {
         if ( bInterfaceSupportsLink[i][iLinkId] )
         {
            g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId = iLinkId;
            bInterfaceWasAssigned[i] = true;
            bLinkWasAssigned[iLinkId] = true;
            t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pNICInfo->szMAC);
            if ( NULL != pCardInfo )  
               log_line("  * B) Assigned radio interface %d (%s) to radio link %d", i+1, str_get_radio_card_model_string(pCardInfo->cardModel), iLinkId+1);
            else
               log_line("  * B) Assigned radio interface %d (%s) to radio link %d", i+1, "Unknown Type", iLinkId+1);

         }
         k++;
         iLinkId++;
         if ( iLinkId >= g_pCurrentModel->radioLinksParams.links_count )
            iLinkId = 0;
      }
      while ( (! bInterfaceWasAssigned[i]) && (k <= MAX_RADIO_INTERFACES) );
   }

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));

   // Log errors

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
      if ( NULL == pNICInfo || controllerIsCardDisabled(pNICInfo->szMAC) )
      {
         log_line("  * Radio interface %d is disabled. It was not assigned to any radio link.", i+1 );
         continue;
      }
      if ( iInterfaceSupportedLinksCount[i] == 0 )
      {
         log_line("  * Radio interface %d does not support any radio links.", i+1 );
         continue;
      }
   }

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! bLinkWasAssigned[i] )
      {
         log_softerror_and_alarm("  * No radio interfaces where assigned to radio link %d !", i+1);
         send_alarm_to_central(ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK, (u32)i,1);
      }
   }
   log_line("Done computing radio interfaces assignment to radio links.");
   log_line("------------------------------------------------------------------");
}


bool _links_set_cards_frequencies()
{
   if ( g_bSearching || (NULL == g_pCurrentModel) )
   {
      log_error_and_alarm("Invalid parameters for setting radio interfaces frequencies");
      return false;
   }

   log_line("Setting all cards frequencies according to radio links...");
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(i);
      if ( NULL == pNIC )
         continue;
      radio_stats_set_card_current_frequency(&g_Local_RadioStats, i, 0);

      if ( controllerIsCardDisabled(pNIC->szMAC) )
         continue;
      int nRadioLinkId = g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId;
      if ( nRadioLinkId < 0 || nRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
         continue;

      if ( 0 == hardware_radio_supports_frequency(pNIC, g_pCurrentModel->radioLinksParams.link_frequency[nRadioLinkId] ) )
         continue;

      launch_set_frequency(NULL, i, g_pCurrentModel->radioLinksParams.link_frequency[nRadioLinkId]);
      g_Local_RadioStats.radio_interfaces[i].currentFrequency = g_pCurrentModel->radioLinksParams.link_frequency[nRadioLinkId];
      radio_stats_set_card_current_frequency(&g_Local_RadioStats, i, g_pCurrentModel->radioLinksParams.link_frequency[nRadioLinkId]);
   }

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));

   return true;
}

void _broadcast_router_ready()
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY;
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = 0;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);
   PH.extra_flags = 0;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   packet_compute_crc(buffer, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
      log_softerror_and_alarm("No pipe to central to broadcast router ready to.");

   if ( -1 != g_fIPCToTelemetry )
      ruby_ipc_channel_send_message(g_fIPCToTelemetry, buffer, PH.total_length);

   log_line("Broadcasted that router is ready.");
}

void process_local_control_packet(t_packet_header* pPH)
{
   //log_line("Process received local controll packet type: %d", pPH->packet_type);

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY )
   {
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS )
   {
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS )
   {
      log_line("Router received a local message to reload core plugins.");
      refresh_CorePlugins(0);
      log_line("Router finished reloading core plugins.");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED )
   {
      if ( ! g_bSearching )
      {
         log_softerror_and_alarm("Received local message to change the search frequency to %d Mhz, but no search is in progress!", (int)pPH->vehicle_id_dest);
         return;
      }
      log_line("Received local message to change the search frequency to %d Mhz", (int)pPH->vehicle_id_dest);
      links_set_cards_frequencies_for_search((int)pPH->vehicle_id_dest);
      log_line("Switched search frequency to %d Mhz. Broadcasting that router is ready.", (int)pPH->vehicle_id_dest);
      _broadcast_router_ready();
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_LINK_FREQUENCY_CHANGED )
   {
      int* pI = (int*)(((u8*)(pPH))+sizeof(t_packet_header));
      int nLink = *pI;
      pI++;
      int newFreq = *pI;
               
      log_line("Received control packet from central that a vehicle radio link frequency changed (radio link %d new freq: %d Mhz)", nLink+1, newFreq);
      if ( NULL != g_pCurrentModel && (nLink < g_pCurrentModel->radioLinksParams.links_count) )
      {
         g_pCurrentModel->radioLinksParams.link_frequency[nLink] = newFreq;
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == nLink )
               g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i] = newFreq;
         }
         _links_set_cards_frequencies();

         process_data_rx_video_reset_retransmission_stats();
      }
      log_line("Done processing control packet of frequency change.");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_PASSPHRASE_CHANGED )
   {
      lpp(NULL,0);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
   {
      log_line("Received control packet to reload model");

      if ( ! g_bFirstModelPairingDone )
      {
         if ( access( FILE_FIRST_PAIRING_DONE, R_OK ) != -1 )
            g_bFirstModelPairingDone = true;
         if ( g_bFirstModelPairingDone )
            log_line("First model pairing was completed. Using valid user model.");
         else
            log_line("First model pairing is still not done. Still using default model.");
      }

      u32 oldEFlags = g_pCurrentModel->enc_flags;

      if ( NULL != g_pCurrentModel )
      if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL) )
         log_softerror_and_alarm("Failed to load current model.");

      if ( g_pCurrentModel->enc_flags != oldEFlags )
         lpp(NULL, 0);
      // Signal other components about the model change
      if ( pPH->vehicle_id_src == PACKET_COMPONENT_COMMANDS )
      {
         if ( -1 != g_fIPCToTelemetry )
            ruby_ipc_channel_send_message(g_fIPCToTelemetry, (u8*)pPH, pPH->total_length);
         if ( -1 != g_fIPCToRC )
            ruby_ipc_channel_send_message(g_fIPCToRC, (u8*)pPH, pPH->total_length);
      }
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED )
   {
      log_line("Received control packet to reload controller settings.");

      load_ControllerSettings();
      g_pControllerSettings = get_ControllerSettings();

      int iLinks = g_Local_RadioStats.countRadioLinks;
      int iInterf = g_Local_RadioStats.countRadioInterfaces;
      shared_mem_radio_stats_radio_interface  radio_interfaces[MAX_RADIO_INTERFACES];
      memcpy((u8*)&radio_interfaces[0], (u8*)&g_Local_RadioStats.radio_interfaces[0], MAX_RADIO_INTERFACES * sizeof(shared_mem_radio_stats_radio_interface) );

      radio_stats_reset(&g_Local_RadioStats, g_pControllerSettings->nGraphRadioRefreshInterval);
      
      g_Local_RadioStats.countRadioLinks = iLinks;
      g_Local_RadioStats.countRadioInterfaces = iInterf;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId = radio_interfaces[i].assignedRadioLinkId;
         g_Local_RadioStats.radio_interfaces[i].currentFrequency = radio_interfaces[i].currentFrequency;
         g_Local_RadioStats.radio_interfaces[i].openedForRead = radio_interfaces[i].openedForRead;
         g_Local_RadioStats.radio_interfaces[i].openedForWrite = radio_interfaces[i].openedForWrite;
         g_Local_RadioStats.radio_interfaces[i].rxQuality = radio_interfaces[i].rxQuality;
         g_Local_RadioStats.radio_interfaces[i].rxRelativeQuality = radio_interfaces[i].rxRelativeQuality;
      }

      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));

      if ( g_pCurrentModel->hasCamera() )
      {
         processor_rx_video_forward_check_controller_settings_changed();
         process_data_rx_video_on_controller_settings_changed();
      }

      // Signal other components about the model change
      if ( pPH->vehicle_id_src == PACKET_COMPONENT_LOCAL_CONTROL )
      {
         if ( -1 != g_fIPCToTelemetry )
            ruby_ipc_channel_send_message(g_fIPCToTelemetry, (u8*)pPH, pPH->total_length);
         if ( -1 != g_fIPCToRC )
            ruby_ipc_channel_send_message(g_fIPCToRC, (u8*)pPH, pPH->total_length);
      }
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED ||
        pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED ||
        pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED )
   {
      if ( -1 != g_fIPCToTelemetry )
         ruby_ipc_channel_send_message(g_fIPCToTelemetry, (u8*)pPH, pPH->total_length);
      if ( -1 != g_fIPCToRC )
         ruby_ipc_channel_send_message(g_fIPCToRC, (u8*)pPH, pPH->total_length);

      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED )
         g_bUpdateInProgress = true;
      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED ||
           pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED )
         g_bUpdateInProgress = false;
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_START )
   {
      log_line("Starting debug router packets history graph");
      g_bDebugIsPacketsHistoryGraphOn = true;
      g_bDebugIsPacketsHistoryGraphPaused = false;
      g_pDebug_SM_RouterPacketsStatsHistory = shared_mem_router_packets_stats_history_open_write();
      if ( NULL == g_pDebug_SM_RouterPacketsStatsHistory )
      {
         log_softerror_and_alarm("Failed to open shared mem for write for router packets stats history.");
         g_bDebugIsPacketsHistoryGraphOn = false;
         g_bDebugIsPacketsHistoryGraphPaused = true;
      }
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_STOP )
   {
      g_bDebugIsPacketsHistoryGraphOn = false;
      g_bDebugIsPacketsHistoryGraphPaused = true;
      shared_mem_router_packets_stats_history_close(g_pDebug_SM_RouterPacketsStatsHistory);
      return;
   }
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_PAUSE )
   {
      g_bDebugIsPacketsHistoryGraphPaused = true;
   }
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_RESUME )
   {
      g_bDebugIsPacketsHistoryGraphPaused = false;
   }
}

void process_local_control_packets()
{
   while ( packets_queue_has_packets(&s_QueueControlPackets) )
   {
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      int length = -1;
      u8* pBuffer = packets_queue_pop_packet(&s_QueueControlPackets, &length);
      if ( NULL == pBuffer || -1 == length )
         break;

      t_packet_header* pPH = (t_packet_header*)pBuffer;
      process_local_control_packet(pPH);
   }
}

// returns 0 if not ping must be sent now, or the ping frequency in ms if it must be inserted
int _must_inject_ping_now()
{
   u32 ping_freq_ms = compute_ping_frequency(g_pCurrentModel->uModelFlags, g_pCurrentModel->clock_sync_type, g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags);

   if ( NULL != g_pCurrentModel && (! g_pCurrentModel->b_mustSyncFromVehicle) )
   if ( ! g_pCurrentModel->is_spectator )
   if ( ! g_bSearching )
   {
      u32 microTime = get_current_timestamp_micros();
      if ( microTime > g_uLastPingSendTimeMicroSec + ping_freq_ms*1000 ||
           microTime < g_uLastPingSendTimeMicroSec )
         return ping_freq_ms;
   }

   return 0;
}

void _check_for_atheros_datarate_change(u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
      return;
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_COMMANDS )
      return;

   if ( pPH->packet_type != PACKET_TYPE_COMMAND )
      return;

   t_packet_header_command* pPHC = (t_packet_header_command*)(pPacketBuffer + sizeof(t_packet_header));

   if ( pPHC->command_type != COMMAND_ID_SET_RADIO_LINK_FLAGS )
      return;

   u32* pInfo = (u32*)(pPacketBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
   u32 linkIndex = *pInfo;
   pInfo++;
   u32 linkFlags = *pInfo;
   pInfo++;
   int* piInfo = (int*)pInfo;
   int datarateVideo = *piInfo;
   piInfo++;
   int datarateData = *piInfo;

   g_TimeLastSetRadioFlagsCommandSent = g_TimeNow;

   log_line("Intercepted Set Radio Links Flags command to vehicle: Link %d, Link flags: %u, Datarate %d/%d", linkIndex+1, linkFlags, datarateVideo, datarateData);
   g_uLastRadioLinkIndexSentRadioCommand = linkIndex;
   g_iLastRadioLinkDataDataRateSentRadioCommand = datarateData;
}

void _process_and_send_packets(int iCountPendingVideoRetransmissionsRequests)
{
   if ( g_bSearching || NULL == g_pCurrentModel || ( (NULL != g_pCurrentModel) && (g_pCurrentModel->is_spectator)) )
   {
      // Empty queue
      packets_queue_init(&s_QueueRadioPackets);
      return;
   }

   int iPingFreqMs = _must_inject_ping_now();
   if ( iPingFreqMs )
   {
      g_uLastPingSendTimeMicroSec = get_current_timestamp_micros();
      s_uLastPingSentId++;
      s_uLastPingRadioLinkId++;
      if ( s_uLastPingRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
         s_uLastPingRadioLinkId = 0;

      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_RUBY;
      PH.packet_type =  PACKET_TYPE_RUBY_PING_CLOCK;
      PH.vehicle_id_src = g_uControllerId;
      PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
      PH.total_headers_length = sizeof(t_packet_header);
      PH.total_length = sizeof(t_packet_header) + sizeof(u32) + sizeof(u8);

      #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS )
      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO )
      if ( g_TimeNow > g_TimeLastControllerLinkStatsSent + CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL_MS/2 )
         PH.total_length += get_controller_radio_link_stats_size();
      #endif
      PH.extra_flags = 0;

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &s_uLastPingSentId, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &s_uLastPingRadioLinkId, sizeof(u8));
      #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
      if ( NULL != g_pCurrentModel && (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS) )
      if ( g_TimeNow > g_TimeLastControllerLinkStatsSent + CONTROLLER_LINK_STATS_HISTORY_SLICE_INTERVAL_MS/2 )
      {
         add_controller_radio_link_stats_to_buffer(packet+sizeof(t_packet_header)+sizeof(u32)+sizeof(u8));
         g_TimeLastControllerLinkStatsSent = g_TimeNow;
      }
      #endif
      //packets_queue_add_packet(&s_QueueRadioPackets, packet);
      packets_queue_inject_packet_first(&s_QueueRadioPackets, packet);

      if ( g_bDebugIsPacketsHistoryGraphOn && (!g_bDebugIsPacketsHistoryGraphPaused) )
         add_detailed_history_tx_packets(g_pDebug_SM_RouterPacketsStatsHistory, g_TimeNow % 1000, 0, 0, 1, 0, 0, 0);
   }
   else if ( 0 == packets_queue_has_packets(&s_QueueRadioPackets) )
      return;

   Preferences* pP = get_Preferences();
   
   int maxLengthAllowedInRadioPacket = pP->iDebugMaxPacketSize;
   if ( maxLengthAllowedInRadioPacket > MAX_PACKET_PAYLOAD )
      maxLengthAllowedInRadioPacket = MAX_PACKET_PAYLOAD;

   u8 composed_packet[MAX_PACKET_TOTAL_SIZE];
   int composed_packet_length = 0;
   int send_count = 1;
   int countComm = 0;
   int countRC = 0;

   int iMaxPacketsToSend = 4 - iCountPendingVideoRetransmissionsRequests;
   
   // Send retransmissions first, if any

   if( iCountPendingVideoRetransmissionsRequests > 0 )
   {
      for( int i=0; i<packets_queue_has_packets(&s_QueueRadioPackets); i++ )
      {
         int length = 0;
         u8* pData = packets_queue_peek_packet(&s_QueueRadioPackets, i, &length);
         if ( NULL == pData || length <= 0 )
            continue;

         t_packet_header* pPH = (t_packet_header*)pData;
         if ( pPH->packet_flags == PACKET_COMPONENT_VIDEO )
         if ( (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS) || (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2) )
         {
            pPH->vehicle_id_src = g_uControllerId;
            send_packet_to_radio_interfaces(pData, length);
            if ( g_bDebugIsPacketsHistoryGraphOn && (!g_bDebugIsPacketsHistoryGraphPaused) )
               add_detailed_history_tx_packets(g_pDebug_SM_RouterPacketsStatsHistory, g_TimeNow % 1000, 0, 0, 0, 0, 0, 0);

            iCountPendingVideoRetransmissionsRequests--;

            if ( 0 == iCountPendingVideoRetransmissionsRequests )
               break;
         }
      } 
   }

   while ( packets_queue_has_packets(&s_QueueRadioPackets) && iMaxPacketsToSend > 0 )
   {
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      int length = -1;
      u8* pBuffer = packets_queue_pop_packet(&s_QueueRadioPackets, &length);
      if ( NULL == pBuffer || -1 == length )
         break;

      _check_for_atheros_datarate_change(pBuffer);

      t_packet_header* pPH = (t_packet_header*)pBuffer;
      pPH->vehicle_id_src = g_uControllerId;
      
      if ( pPH->packet_flags == PACKET_COMPONENT_VIDEO )
      if ( (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS) || (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2) )
         continue;

      bool bSendNow = false;

      if ( (composed_packet_length + length > maxLengthAllowedInRadioPacket) )
         bSendNow = true;

      if ( g_bUpdateInProgress )
         bSendNow = true;

      if ( bSendNow && composed_packet_length > 0 )
      {
         for( int i=0; i<send_count; i++ )
         {
            if ( i != 0 )
               hardware_sleep_ms(2);

            send_packet_to_radio_interfaces(composed_packet, composed_packet_length);
            if ( g_bDebugIsPacketsHistoryGraphOn && (!g_bDebugIsPacketsHistoryGraphPaused) )
               add_detailed_history_tx_packets(g_pDebug_SM_RouterPacketsStatsHistory, g_TimeNow % 1000, 0, countComm, 0, countRC, 0, 0);
         }
         iMaxPacketsToSend--;
         composed_packet_length = 0;
         send_count = 1;
         countComm = 0;
         countRC = 0;
      }

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
      if ( pPH->packet_type == COMMAND_ID_SET_RADIO_LINK_FREQUENCY )
         send_count = 10;

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
      if ( pPH->packet_type == COMMAND_ID_SET_CAMERA_PARAMETERS )
         video_line_adaptive_switch_to_med_level();

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
         countComm = 1;
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RC )
         countRC = 1;

      memcpy(&composed_packet[composed_packet_length], pBuffer, length);
      composed_packet_length += length;
   }

   if ( composed_packet_length > 0 )
   {
      for( int i=0; i<send_count; i++ )
      {
         if ( i != 0 )
            hardware_sleep_ms(2);

         send_packet_to_radio_interfaces(composed_packet, composed_packet_length);

         if ( g_bDebugIsPacketsHistoryGraphOn && (!g_bDebugIsPacketsHistoryGraphPaused) )
            add_detailed_history_tx_packets(g_pDebug_SM_RouterPacketsStatsHistory, g_TimeNow % 1000, 0, countComm, 0, countRC, 0, 0);
      }
      composed_packet_length = 0;
      send_count = 1;
      countComm = 0;
      countRC = 0;
   }
}

void try_read_pipes()
{
   int maxPacketsToRead = 5;
   maxPacketsToRead += DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY;
   while ( (maxPacketsToRead > 0) && NULL != ruby_ipc_try_read_message(g_fIPCFromCentral, 50, s_PipeBufferCommands, &s_PipeBufferCommandsPos, s_BufferCommands) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferCommands;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferCommands); 
      else
         packets_queue_add_packet(&s_QueueRadioPackets, s_BufferCommands); 
   }

   maxPacketsToRead = 5;
   while ( (maxPacketsToRead > 0) && NULL != ruby_ipc_try_read_message(g_fIPCFromTelemetry, 50, s_PipeBufferTelemetryUplink, &s_PipeBufferTelemetryUplinkPos, s_BufferTelemetryUplink) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferTelemetryUplink;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferTelemetryUplink); 
      else
         packets_queue_add_packet(&s_QueueRadioPackets, s_BufferTelemetryUplink); 
   }

   maxPacketsToRead = 5;
   while ( (maxPacketsToRead > 0) && NULL != ruby_ipc_try_read_message(g_fIPCFromRC, 50, s_PipeBufferRCUplink, &s_PipeBufferRCUplinkPos, s_BufferRCUplink) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferRCUplink;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferRCUplink); 
      else
         packets_queue_add_packet(&s_QueueRadioPackets, s_BufferRCUplink);
   }
}

void init_shared_memory_objects()
{
   g_pSM_RadioStats = shared_mem_radio_stats_open_for_write();
   if ( NULL == g_pSM_RadioStats )
      log_softerror_and_alarm("Failed to open radio stats shared memory for write.");
   else
      log_line("Opened radio stats shared memory for write: success.");

   if ( NULL == g_pCurrentModel )
      radio_stats_reset(&g_Local_RadioStats, g_pControllerSettings->nGraphRadioRefreshInterval);
   else
      radio_stats_reset(&g_Local_RadioStats, g_pControllerSettings->nGraphRadioRefreshInterval);

   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));

   g_pSM_VideoInfoStats = shared_mem_video_info_stats_open_for_write();
   if ( NULL == g_pSM_VideoInfoStats )
      log_softerror_and_alarm("Failed to open shared mem video info stats for writing: %s", SHARED_MEM_VIDEO_STREAM_INFO_STATS);
   else
      log_line("Opened shared mem video info stats stats for writing.");

   g_pSM_VideoInfoStatsRadioIn = shared_mem_video_info_stats_radio_in_open_for_write();
   if ( NULL == g_pSM_VideoInfoStatsRadioIn )
      log_softerror_and_alarm("Failed to open shared mem video info radio in stats for writing: %s", SHARED_MEM_VIDEO_STREAM_INFO_STATS_RADIO_IN);
   else
      log_line("Opened shared mem video info radio in stats stats for writing.");

   g_pSM_ControllerAdaptiveVideoInfo = shared_mem_controller_adaptive_video_info_open_for_write();
   if ( NULL == g_pSM_ControllerAdaptiveVideoInfo )
      log_softerror_and_alarm("Failed to open shared mem controller adaptive video info for writing: %s", SHARED_MEM_CONTROLLER_ADAPTIVE_VIDEO_INFO);
   else
      log_line("Opened shared mem controller adaptive video info for writing.");

   if ( NULL != g_pSM_ControllerAdaptiveVideoInfo )
      memcpy( (u8*)g_pSM_ControllerAdaptiveVideoInfo, (u8*)&g_ControllerAdaptiveVideoInfo, sizeof(shared_mem_controller_adaptive_video_info));

   g_pSM_VideoLinkStats = shared_mem_video_link_stats_open_for_write();
   if ( NULL == g_pSM_VideoLinkStats )
      log_softerror_and_alarm("Failed to open shared mem video link stats for writing: %s", SHARED_MEM_VIDEO_LINK_STATS);
   else
      log_line("Opened shared mem video link stats stats for writing.");

   g_pSM_VideoLinkGraphs = shared_mem_video_link_graphs_open_for_write();
   if ( NULL == g_pSM_VideoLinkGraphs )
      log_softerror_and_alarm("Failed to open shared mem video link graphs for writing: %s", SHARED_MEM_VIDEO_LINK_GRAPHS);
   else
      log_line("Opened shared mem video link graphs stats for writing.");

   g_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_ROUTER_RX);
   if ( NULL == g_pProcessStats )
      log_softerror_and_alarm("Failed to open shared mem for video rx process watchdog stats for writing: %s", SHARED_MEM_WATCHDOG_ROUTER_RX);
   else
      log_line("Opened shared mem for video rx process watchdog stats for writing.");

   if ( NULL != g_pProcessStats )
   {
      g_pProcessStats->alarmFlags = 0;
      g_pProcessStats->alarmTime = 0;
      g_pProcessStats->alarmParam[0] = g_pProcessStats->alarmParam[1] = g_pProcessStats->alarmParam[2] = g_pProcessStats->alarmParam[3] = 0;
   }
}

int open_pipes()
{
   g_fIPCFromRC = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_RC_TO_ROUTER);
   if ( g_fIPCFromRC < 0 )
      return -1;

   g_fIPCToRC = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_RC);
   if ( g_fIPCToRC < 0 )
      return -1;
   
   g_fIPCFromCentral = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_CENTRAL_TO_ROUTER);
   if ( g_fIPCFromCentral < 0 )
      return -1;

   g_fIPCToCentral = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_CENTRAL);
   if ( g_fIPCToCentral < 0 )
      return -1;
   
   g_fIPCToTelemetry = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY);
   if ( g_fIPCToTelemetry < 0 )
      return -1;
   
   g_fIPCFromTelemetry = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER);
   if ( g_fIPCFromTelemetry < 0 )
      return -1;
   
   if ( NULL == g_pCurrentModel || (!g_pCurrentModel->audio_params.enabled) )
   {
      log_line("Audio is disabled on current vehicle.");
      return 0;
   }
   if ( NULL == g_pCurrentModel || (! g_pCurrentModel->audio_params.has_audio_device) )
   {
      log_line("No audio capture device on current vehicle.");
      return 0;
   }

   char szOutput[4096];
   hw_execute_bash_command_raw("aplay -l 2>&1", szOutput );
   if ( NULL != strstr(szOutput, "no soundcards") )
      log_softerror_and_alarm("No output audio devices/soundcards on the controller. Audio output is disabled.");
   else
   {
      log_line("Opening audio pipe write endpoint: %s", FIFO_RUBY_AUDIO1);
      g_fPipeAudio = open(FIFO_RUBY_AUDIO1, O_WRONLY);
      if ( g_fPipeAudio < 0 )
      {
         log_error_and_alarm("Failed to open audio pipe write endpoint: %s",FIFO_RUBY_AUDIO1);
         return -1;
      }
      log_line("Opened successfully audio pipe write endpoint: %s", FIFO_RUBY_AUDIO1);
   }
   return 0;
}

void _router_periodic_loop()
{
   if ( radio_stats_periodic_update(&g_Local_RadioStats, g_TimeNow) )
   {
      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));
   }
   radio_controller_links_stats_periodic_update(&g_PD_ControllerLinkStats, g_TimeNow);
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = g_TimeNow;

   s_debugFramesCount++;
   if ( g_TimeNow > s_debugLastFPSTime + 500 )
   {
      //log_line("Loop FPS: %d", s_debugFramesCount*2);
      //send_alarm_to_central(ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK, 1,1);
      s_debugLastFPSTime = g_TimeNow;
      s_debugFramesCount = 0;
   }

   if ( NULL != g_pCurrentModel )
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_STATS_VIDEO_INFO)
   if ( g_TimeNow >= g_VideoInfoStats.uTimeLastUpdate + 200 )
   {
      update_shared_mem_video_info_stats( &g_VideoInfoStats, g_TimeNow);
      update_shared_mem_video_info_stats( &g_VideoInfoStatsRadioIn, g_TimeNow);

      if ( NULL != g_pSM_VideoInfoStats )
         memcpy((u8*)g_pSM_VideoInfoStats, (u8*)&g_VideoInfoStats, sizeof(shared_mem_video_info_stats));
      if ( NULL != g_pSM_VideoInfoStatsRadioIn )
         memcpy((u8*)g_pSM_VideoInfoStatsRadioIn, (u8*)&g_VideoInfoStatsRadioIn, sizeof(shared_mem_video_info_stats));
   }


   if ( ! g_bRuntimeControllerPairingCompleted )
   if ( ! g_bSearching )
   if ( NULL != g_pCurrentModel )
   if ( ! g_pCurrentModel->is_spectator )
   if ( g_TimeNow > s_uLastPairingRequestSendTime+s_uPairingRequestSendIntervalMs )
   {
      s_uPairingRequestsSentCount++;
      s_uLastPairingRequestSendTime = g_TimeNow;
      if ( s_uPairingRequestSendIntervalMs < 400 )
         s_uPairingRequestSendIntervalMs++;
      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_RUBY;
      PH.packet_type =  PACKET_TYPE_RUBY_PAIRING_REQUEST;
      PH.vehicle_id_src = g_uControllerId;
      PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
      PH.total_headers_length = sizeof(t_packet_header);
      PH.total_length = sizeof(t_packet_header) + sizeof(u32);
      PH.extra_flags = 0;
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet + sizeof(t_packet_header), &s_uPairingRequestsSentCount, sizeof(u32));
      send_packet_to_radio_interfaces(packet, PH.total_length);

      if ( (s_uPairingRequestsSentCount % 5) == 0 )
         log_line("Sent pairing request to vehicle (retry count: %u). CID: %u, VID: %u", s_uPairingRequestsSentCount, PH.vehicle_id_src, PH.vehicle_id_dest);
   }
}

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
} 
  
int main (int argc, char *argv[])
{
   signal(SIGPIPE, SIG_IGN);
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);
   
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }
      
   log_init("R-Router Station");
   
   g_bSearching = false;
   g_iSearchFrequency = 0;
   if ( strcmp(argv[argc-2], "-search") == 0 )
   {
      g_bSearching = true;
      g_iSearchFrequency = atoi(argv[argc-1]);
   }
   g_bDebug = false;
   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebug = true;
   if ( g_bDebug )
      log_enable_stdout();
 
   if ( g_bSearching )
      log_line("Launched router in search mode, search frequency: %d Mhz", g_iSearchFrequency);

   radio_init_link_structures();
   radio_enable_crc_gen(1);

   hardware_enumerate_radio_interfaces(); 

   load_Preferences();   
   load_ControllerSettings();
   load_ControllerInterfacesSettings();
   hardware_i2c_load_device_settings();

   controllerRadioInterfacesLogInfo();

   g_pControllerSettings = get_ControllerSettings();
   g_pControllerInterfaces = get_ControllerInterfacesSettings();
   Preferences* pP = get_Preferences();   
   if ( pP->nLogLevel != 0 )
      log_only_errors();
 
   g_uControllerId = controller_utils_getControllerId();
   log_line("Controller UID: %u", g_uControllerId);

   g_pCurrentModel = NULL;
   if ( ! g_bSearching )
   {
      g_pCurrentModel = new Model();
      if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL) )
         g_pCurrentModel = NULL;
      if ( g_pCurrentModel->enc_flags != MODEL_ENC_FLAGS_NONE )
         lpp(NULL, 0);
      g_pCurrentModel->logVehicleRadioInfo();
   }

   if ( NULL != g_pControllerSettings )
      hw_set_priority_current_proc(g_pControllerSettings->iNiceRouter);
 
   if ( ! process_data_rx_video_init() )
      log_softerror_and_alarm("Failed to init process packets rx video");

   log_line("Init video RX state: done");

   if ( ! g_bSearching )
      init_processing_audio();

   init_shared_memory_objects();

   log_line("Init shared mem objects: done");

   if ( -1 == open_pipes() )
   {
      log_error_and_alarm("Failed to open required pipes. Exit.");
      return -1;
   }

   init_radio_rx_structures();
   radio_controller_links_stats_reset(&g_PD_ControllerLinkStats);

   u32 delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   if ( NULL != pP )
      delayMs = (u32) pP->iDebugWiFiChangeDelay;
   if ( delayMs<1 || delayMs > 200 )
      delayMs = DEFAULT_DELAY_WIFI_CHANGE;

   hardware_sleep_ms(delayMs);

   g_Local_RadioStats.countRadioInterfaces = hardware_get_radio_interfaces_count();
   if ( NULL != g_pCurrentModel )
      g_Local_RadioStats.countRadioLinks = g_pCurrentModel->radioLinksParams.links_count;
   else
      g_Local_RadioStats.countRadioLinks = 1;

   if ( g_bSearching )
   {
      links_set_cards_frequencies_for_search(g_iSearchFrequency);
      _open_rxtx_radio_interfaces_for_search(g_iSearchFrequency);
   }
   else
   {
      _compute_radio_interfaces_assignment();
      _links_set_cards_frequencies();
      _open_rxtx_radio_interfaces();
   }

   packets_queue_init(&s_QueueRadioPackets);
   packets_queue_init(&s_QueueControlPackets);

   log_line("IPC Queues Init Complete.");
   
   g_TimeStart = get_current_timestamp_ms();

   if ( NULL != g_pCurrentModel )
   {
      char szBuffF[128];
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         str_get_radio_frame_flags_description(g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[i], szBuffF);
         log_line("Radio frame flags for radio interface %d: %u, %s", i+1, g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[i], szBuffF);
      }
   }

   g_bFirstModelPairingDone = false;
   if ( access( FILE_FIRST_PAIRING_DONE, R_OK ) != -1 )
      g_bFirstModelPairingDone = true;

   memset((u8*)&g_PH_RTE, 0, sizeof(t_packet_header_ruby_telemetry_extended));
   if ( g_bSearching )
      log_line("Router started in search mode");
   else if ( g_bFirstModelPairingDone )
      log_line("Router started with a valid user model (first model pairing was already completed)");
   else
      log_line("Router started with the default model (first model pairing was never completed)");

   processor_rx_video_forware_prepare_video_stream_write();
   video_link_adaptive_init();
   video_link_keyframe_init();
   
   load_CorePlugins(0);

   log_line("Broadcasting that router is ready.");
   _broadcast_router_ready();

   if ( s_iFailedInitRadioInterface >= 0 )
      _broadcast_radio_interface_init_failed(s_iFailedInitRadioInterface);

   log_line("");
   log_line("");
   log_line("----------------------------------------------");
   log_line("         Started all ok. Running now.");
   log_line("----------------------------------------------");
   log_line("");
   log_line("");

   u32 uMaxLoopTime = DEFAULT_MAX_LOOP_TIME_MILISECONDS;

   while (!g_bQuit) 
   {
      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->uLoopCounter++;
         g_pProcessStats->lastActiveTime = g_TimeNow;
      }

      g_TimeNow = get_current_timestamp_ms();
      g_TimeNowMicros = get_current_timestamp_micros();
      u32 tTime0 = g_TimeNow;

      _router_periodic_loop();
      
      u32 tTime1 = get_current_timestamp_ms();

      if ( g_TimeNow >= s_TimeLastPipeCheck + 10 )
      {
         s_TimeLastPipeCheck = g_TimeNow;
         try_read_pipes();
         process_local_control_packets();
      } 
      
      u32 tTime2 = get_current_timestamp_ms();

      int receivedAny = try_receive_radio_packets(2000);

      if ( receivedAny < 0 )
         break;

      u32 tTime3 = get_current_timestamp_ms();
      
      int nEndOfVideoBlock = 0;
      for( int i=0; i<6; i++ )
      {
         receivedAny = try_receive_radio_packets(200);
         if ( receivedAny > 0 )
            nEndOfVideoBlock |= process_received_radio_packets();
         else
            break;
      }
      u32 tTime4 = get_current_timestamp_ms();
      
      if ( g_bSearching )
      {
         u32 tNow = get_current_timestamp_ms();
         if ( tNow > g_TimeNow + uMaxLoopTime )
            log_softerror_and_alarm("Router loop took too long to complete (%d milisec)!!!", tNow - g_TimeNow);
         else
            s_iCountCPULoopOverflows = 0;
         continue;
      }

      if ( NULL != g_pCurrentModel && g_pCurrentModel->hasCamera() )
      {
         process_data_rx_video_loop();
         processor_rx_video_forward_loop();
         video_link_adaptive_periodic_loop();
      }

      u32 tTime5 = get_current_timestamp_ms();
      
      int iContainsVideoRequestsCount = 0;
      int iContainsVideoAdjustmentsRequests = 0;
      for( int i=0; i<packets_queue_has_packets(&s_QueueRadioPackets); i++ )
      {
         int length = 0;
         u8* pData = packets_queue_peek_packet(&s_QueueRadioPackets, i, &length);
         if ( NULL == pData || length <= 0 )
            continue;
         t_packet_header* pPH = (t_packet_header*)pData;

         if ( pPH->packet_flags == PACKET_COMPONENT_VIDEO )
         if ( (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS) || (pPH->packet_type == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2) )
            iContainsVideoRequestsCount++;

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
         if ( pPH->packet_type == PACKET_TYPE_RUBY_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL )
         {
            u32 uLevel = 0;
            memcpy((u8*)&uLevel, pData + sizeof(t_packet_header), sizeof(u32));
            g_ControllerAdaptiveVideoInfo.iLastRequestedLevelShift = (int) uLevel;
            iContainsVideoAdjustmentsRequests++;
         }   
      }

      bool bSendNow = false;

      if ( !g_pCurrentModel->hasCamera())
         bSendNow = true;
      if ( g_pCurrentModel->clock_sync_type == CLOCK_SYNC_TYPE_NONE )
         bSendNow = true;
      if ( g_bUpdateInProgress || nEndOfVideoBlock || s_QueueRadioPackets.timeFirstPacket < g_TimeNow-100 )
         bSendNow = true;
      if ( iContainsVideoRequestsCount > 0 || iContainsVideoAdjustmentsRequests > 0 )
         bSendNow = true;
 
      if ( bSendNow )
      {
         _process_and_send_packets(iContainsVideoRequestsCount);
      }

      u32 tTime6 = get_current_timestamp_ms();
      if ( (g_TimeNow > g_TimeStart + 10000) && (tTime6 > tTime0 + uMaxLoopTime) )
      {
         log_softerror_and_alarm("Router loop took too long to complete (%d milisec: %u + %u + %u + %u + %u + %u), repeat count: %u!!!", tTime6 - tTime0, tTime1-tTime0, tTime2-tTime1, tTime3-tTime2, tTime4-tTime3, tTime5-tTime4, tTime6-tTime5, s_iCountCPULoopOverflows+1);

         s_iCountCPULoopOverflows++;
         if ( s_iCountCPULoopOverflows > 5 )
         if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandSent + 5000 )
            send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD,(tTime6-tTime0),1);

         if ( tTime6 >= tTime0 + 300 )
         if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandSent + 5000 )
            send_alarm_to_central(ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD,(tTime6-tTime0)<<16,1);
      }
      else
      {
         s_iCountCPULoopOverflows = 0;
      }

      if ( NULL != g_pProcessStats )
      {
         if ( g_pProcessStats->uMaxLoopTimeMs < tTime6 - tTime0 )
            g_pProcessStats->uMaxLoopTimeMs = tTime6 - tTime0;
         g_pProcessStats->uTotalLoopTime += tTime6 - tTime0;
         if ( 0 != g_pProcessStats->uLoopCounter )
            g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
      }
   }

   log_line("Stopping...");

   unload_CorePlugins();

   if ( ! process_data_rx_video_uninit() )
      log_softerror_and_alarm("Failed to uninit process packets rx video");

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_RX, g_pProcessStats);
   shared_mem_video_link_stats_close(g_pSM_VideoLinkStats);
   shared_mem_video_link_graphs_close(g_pSM_VideoLinkGraphs);
   shared_mem_radio_stats_close(g_pSM_RadioStats);
   shared_mem_video_info_stats_close(g_pSM_VideoInfoStats);
   shared_mem_video_info_stats_radio_in_close(g_pSM_VideoInfoStatsRadioIn);
   shared_mem_controller_adaptive_video_info_close(g_pSM_ControllerAdaptiveVideoInfo);

   _close_rxtx_radio_interfaces(); 
  
   ruby_close_ipc_channel(g_fIPCFromCentral);
   ruby_close_ipc_channel(g_fIPCToCentral);
   ruby_close_ipc_channel(g_fIPCFromTelemetry);
   ruby_close_ipc_channel(g_fIPCToTelemetry);
   ruby_close_ipc_channel(g_fIPCFromRC);
   ruby_close_ipc_channel(g_fIPCToRC);

   if ( -1 != g_fPipeAudio)
      close(g_fPipeAudio);

   g_fIPCFromCentral = -1;
   g_fIPCToCentral = -1;
   g_fIPCToTelemetry = -1;
   g_fIPCFromTelemetry = -1;
   g_fIPCToRC = -1;
   g_fIPCFromRC = -1;
   g_fPipeAudio = -1;
   log_line("Execution Finished. Exit.");
   log_line("--------------------");
 
   return 0;
}
