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
#include "relay_tx.h"
#include "shared_vars.h"
#include "timers.h"

static u8 s_RadioRawPacketRelayed[MAX_PACKET_TOTAL_SIZE];

void relay_send_packet_to_controller(u8* pBufferData, int iBufferLength)
{
   u8* pData = pBufferData;
   int nLength = iBufferLength;
   u32 uCountChainedPackets = 0;
   u32 uStreamId = 0;
   u32 uSourceVehicleId = 0;
   while ( nLength > 0 )
   {
      t_packet_header* pPH = (t_packet_header*)pData;
      
      uSourceVehicleId = pPH->vehicle_id_src;
      uStreamId = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      if ( uStreamId < 0 || uStreamId >= MAX_RADIO_STREAMS )
         uStreamId = 0;

      nLength -= pPH->total_length;
      pData += pPH->total_length;
      uCountChainedPackets++;
   }

   if ( uSourceVehicleId != g_pCurrentModel->relay_params.uRelayVehicleId )
   {
      return;
   } 
   // Send packet on all radio links to controller (that are not set as relay links)

   bool bPacketSent = false;

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;

      if ( !(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      int iRadioInterfaceIndex = -1;
      for( int k=0; k<g_pCurrentModel->radioInterfacesParams.interfaces_count; k++ )
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[k] == iRadioLinkId )
         {
            iRadioInterfaceIndex = k;
            break;
         }
      if ( iRadioInterfaceIndex < 0 )
         continue;

      radio_hw_info_t* pNICInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( ! pNICInfo->openedForWrite )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      
      int nRateTx = g_pCurrentModel->radioLinksParams.link_datarates[iRadioLinkId][0];
      radio_set_out_datarate(nRateTx);

      u32 radioFlags = g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[iRadioInterfaceIndex];
      if ( ! (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE) )
      {
         radioFlags &= ~(RADIO_FLAGS_STBC | RADIO_FLAGS_LDPC | RADIO_FLAGS_SHORT_GI | RADIO_FLAGS_HT40);
         radioFlags |= RADIO_FLAGS_HT20;
      }
      radio_set_frames_flags(radioFlags);

      int totalLength = radio_build_packet(s_RadioRawPacketRelayed, pBufferData, iBufferLength, RADIO_PORT_ROUTER_DOWNLINK, 0, 0, NULL);

      if ( (totalLength >0) && radio_write_packet(iRadioInterfaceIndex, s_RadioRawPacketRelayed, totalLength) )
      {           
         bPacketSent = true;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxPackets++;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxBytes += iBufferLength;
         g_SM_RadioStats.radio_links[iRadioLinkId].streamTotalTxPackets[uStreamId]++;
         g_SM_RadioStats.radio_links[iRadioLinkId].streamTotalTxBytes[uStreamId] += iBufferLength;
      }
      else
         log_softerror_and_alarm("[RelayTX] Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   }

   if ( ! bPacketSent )
   {
      log_softerror_and_alarm("[RelayTX] Packet not sent! No radio interface could send it.");
      return;
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioTxTime = g_TimeNow;

   //log_line("[RelayTX] Sent relayed packet to controller, stream %u, %d bytes, relayed VID: %u, current VID: %u", uStreamId, iBufferLength, uRelayedVehicleId, g_pCurrentModel->vehicle_id);
}

void relay_send_single_packet_to_relayed_vehicle(u8* pBufferData, int iBufferLength)
{
   if ( (NULL == pBufferData) || (iBufferLength <= 0) )
      return;

   t_packet_header* pPH = (t_packet_header*)pBufferData;

   u32 uRelayedVehicleId = pPH->vehicle_id_dest;
   u32 uStreamId = (pPH->stream_packet_idx)>>PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   if ( uStreamId < 0 || uStreamId >= MAX_RADIO_STREAMS )
      uStreamId = 0;

   if ( uRelayedVehicleId != g_pCurrentModel->relay_params.uRelayVehicleId )
   {
      return;
   }

   // Send packet on all radio links to relayed vehicle

   bool bPacketSent = false;

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY) )
         continue;

      if ( !(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      int iRadioInterfaceIndex = -1;
      for( int k=0; k<g_pCurrentModel->radioInterfacesParams.interfaces_count; k++ )
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[k] == iRadioLinkId )
         {
            iRadioInterfaceIndex = k;
            break;
         }
      if ( iRadioInterfaceIndex < 0 )
         continue;

      radio_hw_info_t* pNICInfo = hardware_get_radio_info(iRadioInterfaceIndex);
      if ( ! pNICInfo->openedForWrite )
         continue;
      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;
      
      int nRateTx = g_pCurrentModel->radioLinksParams.link_datarates[iRadioLinkId][1];
      radio_set_out_datarate(nRateTx);

      u32 radioFlags = g_pCurrentModel->radioInterfacesParams.interface_current_radio_flags[iRadioInterfaceIndex];
      if ( ! (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE) )
      {
         radioFlags &= ~(RADIO_FLAGS_STBC | RADIO_FLAGS_LDPC | RADIO_FLAGS_SHORT_GI | RADIO_FLAGS_HT40);
         radioFlags |= RADIO_FLAGS_HT20;
      }
      radio_set_frames_flags(radioFlags);

      int totalLength = radio_build_packet(s_RadioRawPacketRelayed, pBufferData, iBufferLength, RADIO_PORT_ROUTER_UPLINK, 0, 0, NULL);

      if ( (totalLength>0) && radio_write_packet(iRadioInterfaceIndex, s_RadioRawPacketRelayed, totalLength) )
      {           
         bPacketSent = true;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxPackets++;
         g_SM_RadioStats.radio_links[iRadioLinkId].totalTxBytes += iBufferLength;
         g_SM_RadioStats.radio_links[iRadioLinkId].streamTotalTxPackets[uStreamId]++;
         g_SM_RadioStats.radio_links[iRadioLinkId].streamTotalTxBytes[uStreamId] += iBufferLength;
      }
      else
         log_softerror_and_alarm("[RelayTX] Failed to write to radio interface %d.", iRadioInterfaceIndex+1);
   }

   if ( ! bPacketSent )
   {
      log_softerror_and_alarm("[RelayTX] Packet not sent to relayed vehicle! No radio interface could send it.");
      return;
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
}