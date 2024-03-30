/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "process_received_ruby_messages.h"
#include "shared_vars.h"
#include "../base/models_list.h"
#include "../base/ruby_ipc.h"
#include "../base/hardware_radio_sik.h"
#include "../common/string_utils.h"
#include "../common/relay_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "timers.h"
#include "packets_utils.h"
#include "processor_tx_video.h"
#include "video_link_stats_overwrites.h"
#include "video_link_auto_keyframe.h"
#include "events.h"
#include "test_link_params.h"

u32 s_uResendPairingConfirmationCounter = 0;

int _process_received_ping_messages(int iInterfaceIndex, u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   
   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   if ( pPH->total_length >= sizeof(t_packet_header) + 4*sizeof(u8) )
   {
      u8 uLocalRadioLinkId = (u8) g_pCurrentModel->radioInterfacesParams.interface_link_id[iInterfaceIndex];
      
      u8 uPingId = 0;
      u8 uSenderLocalRadioLinkId = 0;
      u8 uTargetRelayCapabilitiesFlags = 0;
      u8 uTargetRelayMode = 0;
      u32 timeNow = get_current_timestamp_micros();
      memcpy( &uPingId, pPacketBuffer + sizeof(t_packet_header), sizeof(u8));
      memcpy( &uSenderLocalRadioLinkId, pPacketBuffer + sizeof(t_packet_header)+sizeof(u8), sizeof(u8));
      memcpy( &uTargetRelayCapabilitiesFlags, pPacketBuffer + sizeof(t_packet_header)+2*sizeof(u8), sizeof(u8));
      memcpy( &uTargetRelayMode, pPacketBuffer + sizeof(t_packet_header)+3*sizeof(u8), sizeof(u8));

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_PING_CLOCK_REPLY, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = pPH->vehicle_id_src;
      PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32);
      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &uPingId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u8), &timeNow, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u8)+sizeof(u32), &uSenderLocalRadioLinkId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header)+2*sizeof(u8)+sizeof(u32), &uLocalRadioLinkId, sizeof(u8));

      //packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
      packets_queue_inject_packet_first(&g_QueueRadioPacketsOut, packet);

      if ( g_pCurrentModel->relay_params.uCurrentRelayMode != uTargetRelayMode )
      {
         u32 uOldRelayMode = g_pCurrentModel->relay_params.uCurrentRelayMode;
         g_pCurrentModel->relay_params.uCurrentRelayMode = uTargetRelayMode;
         saveCurrentModel();
         onEventRelayModeChanged(uOldRelayMode, uTargetRelayMode, "ping");
      }
   }
   return 0;
}

int process_received_ruby_message(int iInterfaceIndex, u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   {
      return _process_received_ping_messages(iInterfaceIndex, pPacketBuffer);
   }

   if ( pPH->packet_type == PACKET_TYPE_TEST_RADIO_LINK )
   {
      test_link_process_received_message(iInterfaceIndex, pPacketBuffer);
      return 0;
   }


   if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST )
   {
      u32 uResendCount = 0;
      if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(u32) )
         memcpy(&uResendCount, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));

      log_line("Received pairing request from controller (received resend count: %u). CID: %u, VID: %u. Sending confirmation back.", uResendCount, pPH->vehicle_id_src, pPH->vehicle_id_dest);
      
      process_data_tx_video_reset_retransmissions_history_info();
      
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
      if ( g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE )
      {
         u32 uOldRelayMode = g_pCurrentModel->relay_params.uCurrentRelayMode;
         g_pCurrentModel->relay_params.uCurrentRelayMode = RELAY_MODE_MAIN | RELAY_MODE_IS_RELAY_NODE;
         saveCurrentModel();
         onEventRelayModeChanged(uOldRelayMode, g_pCurrentModel->relay_params.uCurrentRelayMode, "stop");
      }

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_PAIRING_CONFIRMATION, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = pPH->vehicle_id_src;
      PH.total_length = sizeof(t_packet_header) + sizeof(u32);

      s_uResendPairingConfirmationCounter++;

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &s_uResendPairingConfirmationCounter, sizeof(u32));
      packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);

      g_bReceivedPairingRequest = true;
      g_uControllerId = pPH->vehicle_id_src;
      if ( g_pCurrentModel->uControllerId != g_uControllerId )
      {
         g_pCurrentModel->uControllerId = g_uControllerId;
         saveCurrentModel();
         char szFile[128];
         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_CONTROLLER_ID);
         FILE* fd = fopen(szFile, "w");
         if ( NULL != fd )
         {
            fprintf(fd, "%u\n", g_uControllerId);
            fclose(fd);
         }
      }

      // Forward to other components
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, pPacketBuffer, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, pPacketBuffer, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToRC, pPacketBuffer, pPH->total_length);
      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_SIK_CONFIG )
   {
      u8 uVehicleLinkId = *(pPacketBuffer + sizeof(t_packet_header));
      u8 uCommandId = *(pPacketBuffer + sizeof(t_packet_header) + sizeof(u8));

      log_line("Received SiK config message for local radio link %d, command: %d", (int) uVehicleLinkId+1, (int) uCommandId);

      bool bInvalidParams = false;
      int iLocalRadioInterfaceIndex = -1;
      if ( (uVehicleLinkId < 0) || (uVehicleLinkId >= g_pCurrentModel->radioLinksParams.links_count) )
         bInvalidParams = true;

      if ( ! bInvalidParams )
      {
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == uVehicleLinkId )
            if ( hardware_radio_index_is_sik_radio(i) )
            {
               iLocalRadioInterfaceIndex = i;
               break;
            }
         }
      }

      if ( -1 == iLocalRadioInterfaceIndex )
         bInvalidParams = true;

      if ( bInvalidParams )
      {
         char szBuff[256];
         strcpy(szBuff, "Invalid radio interface selected.");

         t_packet_header PH;
         radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
         PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
         PH.vehicle_id_dest = pPH->vehicle_id_src;
         PH.total_length = sizeof(t_packet_header) + strlen(szBuff)+3*sizeof(u8);

         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet+sizeof(t_packet_header), &uVehicleLinkId, sizeof(u8));
         memcpy(packet+sizeof(t_packet_header) + sizeof(u8), &uCommandId, sizeof(u8));
         memcpy(packet+sizeof(t_packet_header) + 2*sizeof(u8), szBuff, strlen(szBuff)+1);
         packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
         return 0;
      }

      char szBuff[256];
      strcpy(szBuff, "SiK config: done.");

      if ( uCommandId == 1 )
      {
         radio_rx_pause_interface(iLocalRadioInterfaceIndex);
         radio_tx_pause_radio_interface(iLocalRadioInterfaceIndex);
      }
      if ( uCommandId == 2 )
      {
         radio_rx_resume_interface(iLocalRadioInterfaceIndex);
         radio_tx_resume_radio_interface(iLocalRadioInterfaceIndex);
      }

      if ( uCommandId == 0 )
      {
         log_line("Received message to get local SiK radio config for vehicle radio link %d", (int) uVehicleLinkId+1);
         g_iGetSiKConfigAsyncResult = 0;
         g_iGetSiKConfigAsyncRadioInterfaceIndex = iLocalRadioInterfaceIndex;
         g_uGetSiKConfigAsyncVehicleLinkIndex = uVehicleLinkId;
         hardware_radio_sik_get_all_params_async(iLocalRadioInterfaceIndex, &g_iGetSiKConfigAsyncResult);
         return 0;
      }

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_SIK_CONFIG, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = pPH->vehicle_id_src;
      PH.total_length = sizeof(t_packet_header) + strlen(szBuff)+3*sizeof(u8);

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &uVehicleLinkId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header) + sizeof(u8), &uCommandId, sizeof(u8));
      memcpy(packet+sizeof(t_packet_header) + 2*sizeof(u8), szBuff, strlen(szBuff)+1);
      packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);

      return 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_VEHICLE_RECORDING )
   {
      log_line("Received recording command radio packet.");
      if ( pPH->total_length != sizeof(t_packet_header) + 8 * sizeof(u8) )
      {
         log_softerror_and_alarm("Received radio packet for recording of unexpected size. Received %d bytes, expected %d bytes.",
          pPH->total_length, sizeof(t_packet_header) + 8 * sizeof(u8));
         return 0;
      }
      if ( NULL != g_pProcessorTxAudio )
      {
         u8 uCommand = pPacketBuffer[sizeof(t_packet_header)];
         if ( uCommand == 1 || uCommand == 5 )
            g_pProcessorTxAudio->startLocalRecording();
         if ( uCommand == 2 || uCommand == 6 )
            g_pProcessorTxAudio->stopLocalRecording();
      }
      return 0;
   }
   
   log_line("Received unprocessed Ruby message from controller, message type: %d", pPH->packet_type);

   return 0;
}
