#pragma once


typedef struct
{
   u32 crc;
   u8 packet_type; // same as radio packet type: 0...150: components packets types, 150-200: local control controller packets, 200-250: local control vehicle packets
   u8 packet_index;
   u32 stream_packet_idx; // high 4 bits: stream id (0..15), lower 28 bits: stream packet index
   u8 total_length; // Total length, including all the header data, including CRC
   u32 vehicle_id_src; // Only last 2 bytes
   u32 vehicle_id_dest; // Only last 2 bytes
} __attribute__((packed)) t_packet_header_short;

