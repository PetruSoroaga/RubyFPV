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
#include "radiopackets2.h"
#include "radiolink.h"

void packet_compute_crc(u8* pBuffer, int length)
{
   u32 crc = base_compute_crc32(pBuffer + sizeof(u32), length-sizeof(u32)); 
   u32* p = (u32*)pBuffer;
   *p = crc;
}

int packet_check_crc(u8* pBuffer, int length)
{
   u32 crc = base_compute_crc32(pBuffer + sizeof(u32), length-sizeof(u32)); 
   u32* p = (u32*)pBuffer;
   if ( *p != crc )
      return 0;
   return 1;
}


void radio_populate_ruby_telemetry_v2_from_ruby_telemetry_v1(t_packet_header_ruby_telemetry_extended_v2* pV2, t_packet_header_ruby_telemetry_extended_v1* pV1)
{
   if ( NULL == pV1 || NULL == pV2 )
      return;

   pV2->flags = pV1->flags;
   pV2->version = pV1->version;
   pV2->vehicle_id = pV1->vehicle_id;
   pV2->vehicle_type = pV1->vehicle_type;
   memcpy(pV2->vehicle_name, pV1->vehicle_name, MAX_VEHICLE_NAME_LENGTH);
   pV2->radio_links_count = pV1->radio_links_count;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      pV2->uRadioFrequencies[i] = pV1->radio_frequencies[i] & 0x7FFF;
   
   pV2->uRelayLinks = 0;
   
   pV2->downlink_tx_video_bitrate = pV1->downlink_tx_video_bitrate;
   pV2->downlink_tx_video_all_bitrate = pV1->downlink_tx_video_all_bitrate;
   pV2->downlink_tx_data_bitrate = pV1->downlink_tx_data_bitrate;

   pV2->downlink_tx_video_packets_per_sec = pV1->downlink_tx_video_packets_per_sec;
   pV2->downlink_tx_data_packets_per_sec = pV1->downlink_tx_data_packets_per_sec;
   pV2->downlink_tx_compacted_packets_per_sec = pV1->downlink_tx_compacted_packets_per_sec;

   pV2->temperature = pV1->temperature;
   pV2->cpu_load = pV1->cpu_load;
   pV2->cpu_mhz = pV1->cpu_mhz;
   pV2->throttled = pV1->throttled;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pV2->downlink_datarates[i][0] = pV1->downlink_datarates[i][0]; 
      pV2->downlink_datarates[i][1] = pV1->downlink_datarates[i][1]; 
      pV2->uplink_datarate[i] = pV1->uplink_datarate[i];
      pV2->uplink_rssi_dbm[i] = pV1->uplink_rssi_dbm[i];
      pV2->uplink_link_quality[i] = pV1->uplink_link_quality[i];
   }

   pV2->uplink_rc_rssi = pV1->uplink_rc_rssi;
   pV2->uplink_mavlink_rc_rssi = pV1->uplink_mavlink_rc_rssi;
   pV2->uplink_mavlink_rx_rssi = pV1->uplink_mavlink_rx_rssi;

   pV2->txTimePerSec = pV1->txTimePerSec;
   pV2->extraFlags = pV1->extraFlags;
   pV2->extraSize = pV1->extraSize;
}

int radio_convert_short_packet_to_regular_packet(t_packet_header_short* pPHS, u8* pOutPacket, int iMaxLength)
{
   if ( (NULL == pPHS) || (NULL == pOutPacket) || (iMaxLength < sizeof(t_packet_header)) )
      return 0;

   return 0;
}

int radio_convert_regular_packet_to_short_packet(t_packet_header* pPH, u8* pOutPacket, int iMaxLength)
{
   if ( (NULL == pPH) || (NULL == pOutPacket) || (iMaxLength < sizeof(t_packet_header_short)) )
      return 0;

   return 0;
}

int radio_buffer_embed_short_packet_to_packet(t_packet_header_short* pPHS, u8* pOutPacket, int iMaxLength)
{
   if ( (NULL == pPHS) || (NULL == pOutPacket) || (iMaxLength < sizeof(t_packet_header)) )
      return 0;

   return 0;
}

int radio_buffer_embed_packet_to_short_packet(t_packet_header* pPH, u8* pOutPacket, int iMaxLength)
{
   if ( (NULL == pPH) || (NULL == pOutPacket) || (iMaxLength < sizeof(t_packet_header_short)) )
      return 0;

   if ( pPH->total_length >= 255-sizeof(t_packet_header_short) )
      return 0;

   if ( pPH->total_length + sizeof(t_packet_header_short) > iMaxLength )
      return 0;

   int iMustEmbed = 0;
   
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
      iMustEmbed = 1;

   if ( ! iMustEmbed )
      return 0;

   t_packet_header_short* pPHS = (t_packet_header_short*)pOutPacket;
   pPHS->packet_type = PACKET_TYPE_EMBEDED_FULL_PACKET;
   pPHS->packet_index = radio_get_next_short_packet_index();
   pPHS->stream_packet_idx = pPH->stream_packet_idx;
   pPHS->vehicle_id_src = pPH->vehicle_id_dest;
   pPHS->vehicle_id_dest = pPH->vehicle_id_src;
   pPHS->total_length = sizeof(t_packet_header_short) + pPH->total_length;
    
   if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
      pPH->crc = base_compute_crc32(((u8*)pPH) + sizeof(u32), pPH->total_headers_length-sizeof(u32));
   else
      pPH->crc = base_compute_crc32(((u8*)pPH) + sizeof(u32), pPH->total_length-sizeof(u32));

   memcpy(pOutPacket + sizeof(t_packet_header_short), (u8*)pPH, pPH->total_length);

   return pPH->total_length + sizeof(t_packet_header_short);
}

int radio_buffer_is_valid_short_packet(u8* pBuffer, int iLength)
{
   if ( (NULL == pBuffer) || (iLength < sizeof(t_packet_header_short)) )
      return -1;

   u8* pStart = pBuffer;

   for( int i=0; i<=iLength-sizeof(t_packet_header_short); i++ )
   {
      int len = (int)(*(pStart+2*sizeof(u8)+2*sizeof(u32)));
      if ( len < sizeof(t_packet_header_short) )
      {
         pStart++;
         continue;
      }
      if ( i+len > iLength )
      {
         pStart++;
         continue;
      }
      u32 crc = base_compute_crc32(pStart+sizeof(u32), len-sizeof(u32));
      t_packet_header_short* pPHS = (t_packet_header_short*)pStart;
      if ( crc != pPHS->crc )
      {
         pStart++;
         continue;
      }
      return i;
   }
   return -1;
}