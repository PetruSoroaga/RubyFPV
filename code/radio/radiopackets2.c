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
         * Copyright info and developer info must be preserved as is in the user
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

#include "../base/base.h"
#include "radiopackets2.h"
#include "radiolink.h"

void radio_packet_init(t_packet_header* pPH, u8 component, u8 packet_type, u32 uStreamId)
{
   if ( NULL == pPH )
      return;
   
   if ( uStreamId >= MAX_RADIO_STREAMS )
      uStreamId = MAX_RADIO_STREAMS-1;
   pPH->uCRC = 0;
   pPH->vehicle_id_src = 0;
   pPH->vehicle_id_dest = 0;
   pPH->radio_link_packet_index = 0;
   pPH->stream_packet_idx = uStreamId << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   pPH->packet_flags = component;
   pPH->packet_type = packet_type;

   if ( SYSTEM_SW_VERSION_MINOR < 10 )
      pPH->packet_flags_extended = 0xFF & ((SYSTEM_SW_VERSION_MAJOR << 4) | SYSTEM_SW_VERSION_MINOR);
   else
      pPH->packet_flags_extended = 0xFF & ((SYSTEM_SW_VERSION_MAJOR << 4) | (SYSTEM_SW_VERSION_MINOR/10));
}

void radio_packet_compute_crc(u8* pBuffer, int length)
{
   u32 crc = base_compute_crc32(pBuffer + sizeof(u32), length-sizeof(u32)); 
   u32* p = (u32*)pBuffer;
   *p = crc;
}

int radio_packet_check_crc(u8* pBuffer, int length)
{
   u32 crc = base_compute_crc32(pBuffer + sizeof(u32), length-sizeof(u32)); 
   u32* p = (u32*)pBuffer;
   if ( *p != crc )
      return 0;
   return 1;
}


void radio_populate_ruby_telemetry_v3_from_ruby_telemetry_v1(t_packet_header_ruby_telemetry_extended_v3* pV3, t_packet_header_ruby_telemetry_extended_v1* pV1)
{
   if ( NULL == pV1 || NULL == pV3 )
      return;

   pV3->flags = pV1->flags;
   pV3->version = pV1->version;
   pV3->uVehicleId = pV1->uVehicleId;
   pV3->vehicle_type = pV1->vehicle_type;
   memcpy(pV3->vehicle_name, pV1->vehicle_name, MAX_VEHICLE_NAME_LENGTH);
   pV3->radio_links_count = pV1->radio_links_count;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      pV3->uRadioFrequenciesKhz[i] = 1000 * (u32)(pV1->radio_frequencies[i] & 0x7FFF);
   
   pV3->uRelayLinks = 0;
   
   pV3->downlink_tx_video_bitrate_bps = pV1->downlink_tx_video_bitrate;
   pV3->downlink_tx_video_all_bitrate_bps = pV1->downlink_tx_video_all_bitrate;
   pV3->downlink_tx_data_bitrate_bps = pV1->downlink_tx_data_bitrate;

   pV3->downlink_tx_video_packets_per_sec = pV1->downlink_tx_video_packets_per_sec;
   pV3->downlink_tx_data_packets_per_sec = pV1->downlink_tx_data_packets_per_sec;
   pV3->downlink_tx_compacted_packets_per_sec = pV1->downlink_tx_compacted_packets_per_sec;

   pV3->temperature = pV1->temperature;
   pV3->cpu_load = pV1->cpu_load;
   pV3->cpu_mhz = pV1->cpu_mhz;
   pV3->throttled = pV1->throttled;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      if ( pV1->downlink_datarates[i][0] < 128 )
         pV3->last_sent_datarate_bps[i][0] = 1000 * 1000 * pV1->downlink_datarates[i][0];
      else
         pV3->last_sent_datarate_bps[i][0] = (127-pV1->downlink_datarates[i][0]);
      
      if ( pV1->downlink_datarates[i][1] < 128 )
         pV3->last_sent_datarate_bps[i][1] = 1000 * 1000 * pV1->downlink_datarates[i][1];
      else
         pV3->last_sent_datarate_bps[i][1] = (127-pV1->downlink_datarates[i][1]);
      
      pV3->last_recv_datarate_bps[i] = 1000 * 1000 * pV1->uplink_datarate[i];

      pV3->uplink_rssi_dbm[i] = pV1->uplink_rssi_dbm[i];
      pV3->uplink_link_quality[i] = pV1->uplink_link_quality[i];
   }

   pV3->uplink_rc_rssi = pV1->uplink_rc_rssi;
   pV3->uplink_mavlink_rc_rssi = pV1->uplink_mavlink_rc_rssi;
   pV3->uplink_mavlink_rx_rssi = pV1->uplink_mavlink_rx_rssi;

   pV3->txTimePerSec = pV1->txTimePerSec;
   pV3->extraFlags = pV1->extraFlags;
   pV3->extraSize = pV1->extraSize;
}

void radio_populate_ruby_telemetry_v3_from_ruby_telemetry_v2(t_packet_header_ruby_telemetry_extended_v3* pV3, t_packet_header_ruby_telemetry_extended_v2* pV2)
{
   if ( NULL == pV3 || NULL == pV2 )
      return;

   pV3->flags = pV2->flags;
   pV3->version = pV2->version;
   pV3->uVehicleId = pV2->uVehicleId;
   pV3->vehicle_type = pV2->vehicle_type;
   memcpy(pV3->vehicle_name, pV2->vehicle_name, MAX_VEHICLE_NAME_LENGTH);
   pV3->radio_links_count = pV2->radio_links_count;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pV3->uRadioFrequenciesKhz[i] = pV2->uRadioFrequenciesKhz[i];
      if ( pV3->uRadioFrequenciesKhz[i] < 10000 )
         pV3->uRadioFrequenciesKhz[i] *= 1000;
   }

   pV3->uRelayLinks = 0;
   
   pV3->downlink_tx_video_bitrate_bps = pV2->downlink_tx_video_bitrate;
   pV3->downlink_tx_video_all_bitrate_bps = pV2->downlink_tx_video_all_bitrate;
   pV3->downlink_tx_data_bitrate_bps = pV2->downlink_tx_data_bitrate;

   pV3->downlink_tx_video_packets_per_sec = pV2->downlink_tx_video_packets_per_sec;
   pV3->downlink_tx_data_packets_per_sec = pV2->downlink_tx_data_packets_per_sec;
   pV3->downlink_tx_compacted_packets_per_sec = pV2->downlink_tx_compacted_packets_per_sec;

   pV3->temperature = pV2->temperature;
   pV3->cpu_load = pV2->cpu_load;
   pV3->cpu_mhz = pV2->cpu_mhz;
   pV3->throttled = pV2->throttled;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      if ( pV2->downlink_datarates[i][0] < 128 )
         pV3->last_sent_datarate_bps[i][0] = 1000 * 1000 * pV2->downlink_datarates[i][0];
      else
         pV3->last_sent_datarate_bps[i][0] = (127-pV2->downlink_datarates[i][0]);
      
      if ( pV2->downlink_datarates[i][1] < 128 )
         pV3->last_sent_datarate_bps[i][1] = 1000 * 1000 * pV2->downlink_datarates[i][1];
      else
         pV3->last_sent_datarate_bps[i][1] = (127-pV2->downlink_datarates[i][1]);
      
      pV3->last_recv_datarate_bps[i] = 1000 * 1000 * pV2->uplink_datarate[i];

      pV3->uplink_rssi_dbm[i] = pV2->uplink_rssi_dbm[i];
      pV3->uplink_link_quality[i] = pV2->uplink_link_quality[i];
   }

   pV3->uplink_rc_rssi = pV2->uplink_rc_rssi;
   pV3->uplink_mavlink_rc_rssi = pV2->uplink_mavlink_rc_rssi;
   pV3->uplink_mavlink_rx_rssi = pV2->uplink_mavlink_rx_rssi;

   pV3->txTimePerSec = pV2->txTimePerSec;
   pV3->extraFlags = pV2->extraFlags;
   pV3->extraSize = pV2->extraSize;
}

