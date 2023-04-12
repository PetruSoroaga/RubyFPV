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
#include "processor_rx_audio.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/fec.h" 
#include "packets_utils.h"

#include "shared_vars.h"
#include "timers.h"

u32 s_uLastReceivedAudioBlock = MAX_U32;
u32 s_uReceivedAudioDataPackets = 0;
u32 s_uReceivedAudioFECPackets = 0;
u32 s_uReceivedAudioPacketLength = 0;

u32 s_AudioPacketsPerBlock = 0;
u32 s_AudioFECPerBlock = 0;

u8 s_BufferAudioPackets[MAX_AUDIO_PACKETS][MAX_PACKET_PAYLOAD];
bool s_BufferAudioPacketsReceived[MAX_AUDIO_PACKETS];

unsigned int fec_decode_audio_missing_packets[MAX_TOTAL_PACKETS_IN_BLOCK];
unsigned int fec_decode_audio_fec_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
u8* fec_decode_audio_data_packets[MAX_TOTAL_PACKETS_IN_BLOCK];
u8* fec_decode_audio_fec_packets[MAX_TOTAL_PACKETS_IN_BLOCK];
unsigned int missing_audio_packets_count_for_fec = 0;


void _output_audio_block()
{
   if ( -1 == g_fPipeAudio )
      return;

   if ( s_uReceivedAudioDataPackets >= s_AudioPacketsPerBlock )
   {
      for( u32 i=0; i<s_AudioPacketsPerBlock; i++ )
         write(g_fPipeAudio, &s_BufferAudioPackets[i][0], s_uReceivedAudioPacketLength);
      //log_line("Sent block to audio pipe");
      return;
   }

   if ( s_uReceivedAudioDataPackets + s_uReceivedAudioFECPackets < s_AudioPacketsPerBlock )
   {
      for( u32 i=0; i<s_AudioPacketsPerBlock; i++ )
         if ( s_BufferAudioPacketsReceived[i] )
            write(g_fPipeAudio, &s_BufferAudioPackets[i][0], s_uReceivedAudioPacketLength);
      //log_line("Sent incomplete block to audio pipe");
      return;
   }

   // Reconstruct block

   // Add existing data packets, mark and count the ones that are missing

   missing_audio_packets_count_for_fec = 0;
   for( u32 i=0; i<s_AudioPacketsPerBlock; i++ )
   {
      fec_decode_audio_data_packets[i] = &s_BufferAudioPackets[i][0];
      if ( ! s_BufferAudioPacketsReceived[i] )
      {
         fec_decode_audio_missing_packets[missing_audio_packets_count_for_fec] = i;
         missing_audio_packets_count_for_fec++;
      }
   }

   // Add the needed FEC packets to the list
   unsigned int pos = 0;
   for( u32 i=0; i<s_AudioFECPerBlock; i++ )
   {
      if ( s_BufferAudioPacketsReceived[i+s_AudioPacketsPerBlock] )
      {
         fec_decode_audio_fec_packets[pos] = &s_BufferAudioPackets[i+s_AudioPacketsPerBlock][0];
         fec_decode_audio_fec_indexes[pos] = i;
         pos++;
         if ( pos == missing_audio_packets_count_for_fec )
            break;
      }
   }

   fec_decode(s_uReceivedAudioPacketLength, fec_decode_audio_data_packets, (unsigned int) s_AudioPacketsPerBlock, fec_decode_audio_fec_packets, fec_decode_audio_fec_indexes, fec_decode_audio_missing_packets, missing_audio_packets_count_for_fec );

   for( u32 i=0; i<s_AudioPacketsPerBlock; i++ )
      write(g_fPipeAudio, &s_BufferAudioPackets[i][0], s_uReceivedAudioPacketLength);
   //log_line("Sent reconstructed block to audio pipe");
}


void init_processing_audio()
{
   for( int i=0; i<MAX_AUDIO_PACKETS; i++ )
      s_BufferAudioPacketsReceived[i] = false;

   s_uLastReceivedAudioBlock = MAX_U32;

   s_AudioPacketsPerBlock = g_pCurrentModel->audio_params.flags & 0xFF;
   s_AudioFECPerBlock = (g_pCurrentModel->audio_params.flags >> 8) & 0xFF;
}

void process_audio_packet(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   u8* pData = pPacketBuffer + sizeof(t_packet_header);
   u32 segmentIndex = 0;

   memcpy((u8*)&segmentIndex, pData, sizeof(u32));
   pData += sizeof(u32);
   u32 blockIndex = segmentIndex>>8;
   u32 packetIndex = segmentIndex & 0xFF;
   //log_line("Received audio segment %u/%d, size: %d bytes, scheme %u/%u", blockIndex, packetIndex, pPH->total_length-sizeof(u32), s_AudioPacketsPerBlock, s_AudioFECPerBlock);

   if ( s_uLastReceivedAudioBlock != blockIndex && s_uLastReceivedAudioBlock != MAX_U32 )
   {
      _output_audio_block();
      for( int i=0; i<MAX_AUDIO_PACKETS; i++ )
         s_BufferAudioPacketsReceived[i] = false;
      s_uReceivedAudioDataPackets = 0;
      s_uReceivedAudioFECPackets = 0;
   }

   if ( packetIndex >= MAX_AUDIO_PACKETS )
      return;

   s_uLastReceivedAudioBlock = blockIndex;

   if ( s_BufferAudioPacketsReceived[packetIndex] )
      return;

   if ( packetIndex < s_AudioPacketsPerBlock )
      s_uReceivedAudioDataPackets++;
   else
      s_uReceivedAudioFECPackets++;

   s_BufferAudioPacketsReceived[packetIndex] = true;
   s_uReceivedAudioPacketLength = pPH->total_length-sizeof(u32);
   if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
   {
      u8 size = *(((u8*)pPH) + pPH->total_length-1);
      s_uReceivedAudioPacketLength -= size;
   }
   memcpy(&s_BufferAudioPackets[packetIndex][0], pData, s_uReceivedAudioPacketLength);
}
