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
#include "processor_tx_audio.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/fec.h" 
#include "radio_utils.h"
#include "packets_utils.h"
#include "shared_vars.h"
#include "timers.h"


u8 s_BufferAudio[MAX_AUDIO_PACKETS][MAX_PACKET_PAYLOAD];
u8 s_BufferAudioFEC[10][MAX_PACKET_PAYLOAD];
u32 s_BufferAudioCurrentWritePacket = 0;
u32 s_BufferAudioWritePosition = 0;
u32 s_BufferAudioTimeLastRead = 0;
u32 s_BufferAudioTimeLastKbps = 0;
u32 s_BufferAudioDataCount = 0;
u32 s_BufferAudioKbps = 0;
u32 s_AudioSegmentIndex = 0;

u8* p_fec_audio_packets[MAX_DATA_PACKETS_IN_BLOCK];
u8* p_fec_audio_fecs[MAX_FECS_PACKETS_IN_BLOCK];


u32 get_audio_bps()
{
   return s_BufferAudioKbps*1000;
}

int try_read_audio_input(int fStream)
{
   if ( -1 == fStream )
      return 0;
   if ( g_TimeNow < s_BufferAudioTimeLastRead + 10 )
      return 0;
   s_BufferAudioTimeLastRead = g_TimeNow;

   fd_set readset;
   u8 buffer[MAX_PACKET_PAYLOAD];
   u32 maxSegmentSize = MAX_PACKET_PAYLOAD-sizeof(u32);

   FD_ZERO(&readset);
   FD_SET(fStream, &readset);

   struct timeval timePipeInput;
   timePipeInput.tv_sec = 0;
   timePipeInput.tv_usec = 100; // 0.1 miliseconds timeout

   int selectResult = select(fStream+1, &readset, NULL, NULL, &timePipeInput);
   if ( selectResult <= 0 )
      return 0;

   if( 0 == FD_ISSET(fStream, &readset) )
      return 0;

   int count = read(fStream, buffer, maxSegmentSize);
   if ( count < 0 )
   {
      log_error_and_alarm("Failed to read from audio input fifo: %s, returned code: %d, error: %s", FIFO_RUBY_AUDIO1, count, strerror(errno));
      return -1;
   }
   if ( count == 0 )
      return 0;

   if ( s_BufferAudioTimeLastKbps < g_TimeNow-500 )
   {
      //log_line("Audio: %d kbps", s_BufferAudioDataCount*2*8/1000);
      s_BufferAudioKbps = s_BufferAudioDataCount*2*8/1000;
      s_BufferAudioDataCount = 0;
      s_BufferAudioTimeLastKbps = g_TimeNow;
   }

   s_BufferAudioDataCount += count;

   while ( count > 0 )
   {
      if ( s_BufferAudioWritePosition + count <= maxSegmentSize )
      {
         memcpy(&s_BufferAudio[s_BufferAudioCurrentWritePacket][s_BufferAudioWritePosition], buffer, count );
         s_BufferAudioWritePosition += count;
         if ( s_BufferAudioWritePosition == maxSegmentSize )
         {
            s_BufferAudioWritePosition = 0;
            s_BufferAudioCurrentWritePacket++;
            if ( s_BufferAudioCurrentWritePacket >= MAX_AUDIO_PACKETS )
               s_BufferAudioCurrentWritePacket = MAX_AUDIO_PACKETS-1;
         }
         count = 0;
         break;
      }
      u32 dx = (maxSegmentSize-s_BufferAudioWritePosition);
      memcpy(&s_BufferAudio[s_BufferAudioCurrentWritePacket][s_BufferAudioWritePosition], buffer, dx );
      s_BufferAudioWritePosition += dx;
      count -= dx;
      for( int i= 0; i<count; i++ )
         buffer[i] = buffer[i+dx];

      if ( s_BufferAudioWritePosition >= maxSegmentSize )
      {
         s_BufferAudioWritePosition = 0;
         s_BufferAudioCurrentWritePacket++;
         if ( s_BufferAudioCurrentWritePacket >= MAX_AUDIO_PACKETS )
            s_BufferAudioCurrentWritePacket = MAX_AUDIO_PACKETS-1;
      }
   }
   return 0;
}

void send_first_audio_packet()
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_AUDIO;
   PH.packet_type =  PACKET_TYPE_AUDIO_SEGMENT;
   PH.stream_packet_idx = (STREAM_ID_VIDEO_1) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = 0;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + MAX_PACKET_PAYLOAD;

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), (u8*)&s_AudioSegmentIndex, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &s_BufferAudio[0][0], MAX_PACKET_PAYLOAD-sizeof(u32));

   send_packet_to_radio_interfaces(packet, PH.total_length);

   s_AudioSegmentIndex += 0xFF;

   for( u32 i=0; i<s_BufferAudioCurrentWritePacket; i++ )
      memcpy(&s_BufferAudio[i][0], &s_BufferAudio[i+1][0], MAX_PACKET_PAYLOAD);
   s_BufferAudioCurrentWritePacket--;
}

int send_audio_packets()
{
   if ( NULL == g_pCurrentModel || (! g_pCurrentModel->audio_params.has_audio_device) || (! g_pCurrentModel->audio_params.enabled) )
      return 0;
   
   u8 data_packets = g_pCurrentModel->audio_params.flags & 0xFF;
   u8 fec_packets = ( g_pCurrentModel->audio_params.flags >> 8 ) & 0xFF;
   unsigned int packetLength = MAX_PACKET_PAYLOAD - sizeof(u32);

   //log_line("Sending audio encoding blocks: %d/%d, len: %d", data_packets, fec_packets, packetLength);
   if ( fec_packets > 10 )
      fec_packets = 10;

   if ( 0 == fec_packets )
   {
      int countSent = 0;
      int maxCountAudio = 3;
      while( s_BufferAudioCurrentWritePacket > 0 && maxCountAudio > 0 )
      {
         send_first_audio_packet();
         maxCountAudio--;
         countSent++;
      }
      return countSent;
   }

   // Enough packets for a block ?
   if ( s_BufferAudioCurrentWritePacket <= data_packets )
      return 0;

   // Generate FEC packets

   for( int i=0; i<data_packets; i++ )
      p_fec_audio_packets[i] = &s_BufferAudio[i][0];

   for( int i=0; i<fec_packets; i++ )
      p_fec_audio_fecs[i] = &s_BufferAudioFEC[i][0];

   fec_encode(packetLength, p_fec_audio_packets, (unsigned int)data_packets, p_fec_audio_fecs, (unsigned int)fec_packets);

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_AUDIO;
   PH.packet_type =  PACKET_TYPE_AUDIO_SEGMENT;
   PH.stream_packet_idx = (STREAM_ID_VIDEO_1) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = 0;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + MAX_PACKET_PAYLOAD;

   u8 packet[MAX_PACKET_TOTAL_SIZE];

   for( int i=0; i<data_packets; i++ )
   {
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), (u8*)&s_AudioSegmentIndex, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &s_BufferAudio[i][0], MAX_PACKET_PAYLOAD-sizeof(u32));

      send_packet_to_radio_interfaces(packet, PH.total_length);
      s_AudioSegmentIndex++;

      for( u32 i=0; i<s_BufferAudioCurrentWritePacket; i++ )
         memcpy(&s_BufferAudio[i][0], &s_BufferAudio[i+1][0], MAX_PACKET_PAYLOAD);
      s_BufferAudioCurrentWritePacket--;
   }

   for( int i=0; i<fec_packets; i++ )
   {
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), (u8*)&s_AudioSegmentIndex, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &s_BufferAudioFEC[i][0], MAX_PACKET_PAYLOAD-sizeof(u32));

      send_packet_to_radio_interfaces(packet, PH.total_length);
      s_AudioSegmentIndex++;
   }

   u32 block = s_AudioSegmentIndex >> 8;
   block++;
   s_AudioSegmentIndex = block << 8;
   return data_packets;
}