/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
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

#include "../base/config_hw.h"

#ifdef HW_CAPABILITY_WFBOHD

#include <stdio.h>
#include <stdlib.h>
#include <utime.h>
#include <time.h>
#include <sodium.h>
#include <endian.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "radiopackets_wfbohd.h"
#include "radiopackets2.h"
#include "radiolink.h"
#include "../base/flags.h"

static uint32_t s_uLastTimeGeneratedFakeRubyTelemetry = 0;
static t_packet_header_ruby_telemetry_extended_v3 s_FakeRubyTelemetryPacket;
static int s_iDiscardWFBOHDVideoPackets = 1;
static int s_iSocketWFBOHDUDPVideo = -1;

#define MAX_WFB_RX_BLOCKS_QUEUE 10

typedef struct
{
   u32 uVideoBlockIndex;
   u32 uReceivedDataPackets;
   u32 uReceivedECPackets;
   int iFragmentsReceived[MAX_TOTAL_PACKETS_IN_BLOCK];
   u8* pFragments[MAX_TOTAL_PACKETS_IN_BLOCK];
   int iFragmentsSize[MAX_TOTAL_PACKETS_IN_BLOCK];
}  __attribute__ ((packed)) type_wfb_video_block; 

type_wfb_video_block s_RxWFBVideoBlocks[MAX_WFB_RX_BLOCKS_QUEUE];
int s_iTopWFBVideoBlockStackIndex = -1;
u32 s_uLastOutputedOpenIPCVideoBlockIndex = 0;
int s_iwfbohd_wronk_key = 0;

void wfbohd_radio_init()
{
   for( int i=0; i<MAX_WFB_RX_BLOCKS_QUEUE; i++ )
   {
      s_RxWFBVideoBlocks[i].uVideoBlockIndex = MAX_U32;
      s_RxWFBVideoBlocks[i].uReceivedDataPackets = 0;
      s_RxWFBVideoBlocks[i].uReceivedECPackets = 0;
      for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
      {
         s_RxWFBVideoBlocks[i].pFragments[k] = (u8*)malloc(MAX_PACKET_TOTAL_SIZE);
         s_RxWFBVideoBlocks[i].iFragmentsReceived[k] = 0;
         s_RxWFBVideoBlocks[i].iFragmentsSize[k] = 0;
      }
   }

   s_iTopWFBVideoBlockStackIndex = -1;
   s_uLastOutputedOpenIPCVideoBlockIndex = MAX_U32;


   struct sockaddr_in saddr;
   s_iSocketWFBOHDUDPVideo = socket(AF_INET, SOCK_DGRAM, 0);
   if ( s_iSocketWFBOHDUDPVideo < 0 )
      return;

   bzero((char *) &saddr, sizeof(saddr));
   saddr.sin_family = AF_INET;
   saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
   saddr.sin_port = htons((unsigned short)5600);

   if (connect(s_iSocketWFBOHDUDPVideo, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
      s_iSocketWFBOHDUDPVideo = -1;
}


void wfbohd_radio_cleanup()
{
   if ( -1 != s_iSocketWFBOHDUDPVideo )
      close(s_iSocketWFBOHDUDPVideo);
   s_iSocketWFBOHDUDPVideo = -1;

   for( int i=0; i<MAX_WFB_RX_BLOCKS_QUEUE; i++ )
   for( int k=0; k<MAX_TOTAL_PACKETS_IN_BLOCK; k++ )
      free(s_RxWFBVideoBlocks[i].pFragments[k]);
}

void wfbohd_clear_wrong_key_flag()
{
   s_iwfbohd_wronk_key = 0;
   log_line("Clear wrong OpenIPC key flag.");
}

void wfbohd_set_wrong_key_flag()
{
   s_iwfbohd_wronk_key = 1;
   log_line("Set wrong OpenIPC key flag.");
}

int wfbohd_is_wronk_key_flag_set()
{
   return s_iwfbohd_wronk_key;
}


void wfbohd_radio_set_discard_video_data(int iDiscard)
{
   s_iDiscardWFBOHDVideoPackets = iDiscard;
}

uint32_t wfbohd_radio_generate_vehicle_id(uint32_t uFreqKhz)
{
   return uFreqKhz/1000 + 1;
}

int _wfb_rx_stack_discard_blocks(int N, int iCountDiscard)
{
   if ( iCountDiscard >= MAX_WFB_RX_BLOCKS_QUEUE )
      iCountDiscard = MAX_WFB_RX_BLOCKS_QUEUE-1;
   for( int i=0; i<MAX_WFB_RX_BLOCKS_QUEUE - iCountDiscard; i++ )
   {
      s_RxWFBVideoBlocks[i].uVideoBlockIndex = s_RxWFBVideoBlocks[i+iCountDiscard].uVideoBlockIndex;
      s_RxWFBVideoBlocks[i].uReceivedDataPackets = s_RxWFBVideoBlocks[i+iCountDiscard].uReceivedDataPackets;
      s_RxWFBVideoBlocks[i].uReceivedECPackets = s_RxWFBVideoBlocks[i+iCountDiscard].uReceivedECPackets;
      for( int k=0; k<N; k++ )
      {
         s_RxWFBVideoBlocks[i].iFragmentsReceived[k] = s_RxWFBVideoBlocks[i+iCountDiscard].iFragmentsReceived[k];
         s_RxWFBVideoBlocks[i].iFragmentsSize[k] = s_RxWFBVideoBlocks[i+iCountDiscard].iFragmentsSize[k];
         memcpy(s_RxWFBVideoBlocks[i].pFragments[k], s_RxWFBVideoBlocks[i+iCountDiscard].pFragments[k], MAX_PACKET_TOTAL_SIZE);
      }
   }
   s_iTopWFBVideoBlockStackIndex -= iCountDiscard;
   for( int i=s_iTopWFBVideoBlockStackIndex+1; i<MAX_WFB_RX_BLOCKS_QUEUE; i++ )
   {
       s_RxWFBVideoBlocks[i].uVideoBlockIndex = MAX_U32;
       s_RxWFBVideoBlocks[i].uReceivedDataPackets = 0;
       s_RxWFBVideoBlocks[i].uReceivedECPackets = 0;
       for( int k=0; k<N; k++ )
       {
          s_RxWFBVideoBlocks[i].iFragmentsReceived[k] = 0;
          s_RxWFBVideoBlocks[i].iFragmentsSize[k] = 0;
       }
   }
   return iCountDiscard;
}

void _wfb_radio_do_ec(fec_t* pFEC, int iStackBlockIndex)
{
   if ( NULL == pFEC )
      return;
   unsigned indexPackets[MAX_TOTAL_PACKETS_IN_BLOCK];
   uint8_t *in_blocks[MAX_TOTAL_PACKETS_IN_BLOCK];
   uint8_t *out_blocks[MAX_TOTAL_PACKETS_IN_BLOCK];
   int indexFECFragment = pFEC->k;
   int iOutIndex = 0;

   for(int i=0; i <pFEC->k; i++)
   {
      if ( s_RxWFBVideoBlocks[iStackBlockIndex].iFragmentsReceived[i] )
      {
         in_blocks[i] = s_RxWFBVideoBlocks[iStackBlockIndex].pFragments[i];
         indexPackets[i] = i;
         continue;
      }
      for(; indexFECFragment<pFEC->n; indexFECFragment++)
      {
         if ( ! s_RxWFBVideoBlocks[iStackBlockIndex].iFragmentsReceived[indexFECFragment] )
           continue;

         in_blocks[i] = s_RxWFBVideoBlocks[iStackBlockIndex].pFragments[indexFECFragment];
         out_blocks[iOutIndex] = s_RxWFBVideoBlocks[iStackBlockIndex].pFragments[i];
         iOutIndex++;
         indexPackets[i] = indexFECFragment;
         indexFECFragment++;
         break;
      }
   }
   zfec_decode(pFEC, (const uint8_t**)in_blocks, out_blocks, indexPackets, MAX_PACKET_TOTAL_SIZE); 
}

uint8_t* wfbohd_radio_on_received_video_packet(uint32_t uFreqKhz, fec_t* pFEC, type_wfb_block_header* pBlockHeader, uint8_t* pData, int iDataLength)
{
   if ( s_iDiscardWFBOHDVideoPackets || (NULL == pFEC) || (NULL == pBlockHeader) || (NULL == pData) )
      return NULL;

   type_wfb_packet_header* pPacketHeader = (type_wfb_packet_header*)(pData);
   u32 u = be64toh(pBlockHeader->uDataNonce);
   u32 uBlockIndex = u >> 8;
   u32 uFragmentIndex = u & 0xFF;
   u32 uVideoSize = be16toh(pPacketHeader->uPacketSize);

   if ( iDataLength > MAX_PACKET_TOTAL_SIZE )
      return NULL;

   uVideoSize = iDataLength - sizeof(type_wfb_packet_header);
   
   // Discard packets too old (video block already outputed)

   if ( s_uLastOutputedOpenIPCVideoBlockIndex != MAX_U32 )
   if ( uBlockIndex <= s_uLastOutputedOpenIPCVideoBlockIndex )
   {
      if ( (s_uLastOutputedOpenIPCVideoBlockIndex > 1000) && (uBlockIndex < (s_uLastOutputedOpenIPCVideoBlockIndex-1000)) )
         s_uLastOutputedOpenIPCVideoBlockIndex = MAX_U32;
      else
         return NULL;
   }

   if ( uFragmentIndex >= pFEC->n )
      return NULL;

   // Compute rx stack index
   
   int iStackIndexToAddTo = -1;

   if ( -1 == s_iTopWFBVideoBlockStackIndex )
   {
      iStackIndexToAddTo = 0;
      s_RxWFBVideoBlocks[0].uVideoBlockIndex = uBlockIndex;
   }
   else
      iStackIndexToAddTo = uBlockIndex - s_RxWFBVideoBlocks[0].uVideoBlockIndex;

   if ( iStackIndexToAddTo < 0 )
      return NULL;

   // Mo more room for anything (gap too big)? Discard everything
   if ( iStackIndexToAddTo >= MAX_WFB_RX_BLOCKS_QUEUE*2 - 1 )
   {
      for( int i=0; i<MAX_WFB_RX_BLOCKS_QUEUE; i++ )
      {
          s_RxWFBVideoBlocks[i].uVideoBlockIndex = MAX_U32;
          s_RxWFBVideoBlocks[i].uReceivedDataPackets = 0;
          s_RxWFBVideoBlocks[i].uReceivedECPackets = 0;
          for( int k=0; k<pFEC->n; k++ )
          {
             s_RxWFBVideoBlocks[i].iFragmentsReceived[k] = 0;
             s_RxWFBVideoBlocks[i].iFragmentsSize[k] = 0;
          }
      }
      iStackIndexToAddTo = 0;
      s_iTopWFBVideoBlockStackIndex = 0;
      s_RxWFBVideoBlocks[0].uVideoBlockIndex = uBlockIndex;
   }
   else
   // No more room? Discard few blocks
   if ( iStackIndexToAddTo >= MAX_WFB_RX_BLOCKS_QUEUE )
   {
      int iCountDiscard = iStackIndexToAddTo - MAX_WFB_RX_BLOCKS_QUEUE + MAX_WFB_RX_BLOCKS_QUEUE/2;
      iCountDiscard = _wfb_rx_stack_discard_blocks(pFEC->n, iCountDiscard);
      iStackIndexToAddTo -= iCountDiscard;
   }

   if ( -1 == s_iTopWFBVideoBlockStackIndex )
   {
      s_iTopWFBVideoBlockStackIndex = 0;
      s_RxWFBVideoBlocks[iStackIndexToAddTo].uVideoBlockIndex = uBlockIndex;
   }
   else if ( iStackIndexToAddTo > s_iTopWFBVideoBlockStackIndex )
   {
      for( int i=s_iTopWFBVideoBlockStackIndex; i<=iStackIndexToAddTo; i++ )
         s_RxWFBVideoBlocks[i].uVideoBlockIndex = uBlockIndex - (iStackIndexToAddTo-i);
      s_iTopWFBVideoBlockStackIndex = iStackIndexToAddTo;
   }
   s_RxWFBVideoBlocks[iStackIndexToAddTo].uVideoBlockIndex = uBlockIndex;
   
   // Discard duplicate packets
   if ( s_RxWFBVideoBlocks[iStackIndexToAddTo].iFragmentsReceived[uFragmentIndex] )
      return NULL;

   // ---------------------------------------------------------------
   // Generate fake video packet to pass to router if needed

   t_packet_header PH;
   t_packet_header_video_full_77 PHVF;
   memset(&PH, 0, sizeof(t_packet_header));
   memset(&PHVF, 0, sizeof(t_packet_header_video_full_77));

   radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_DATA_FULL, STREAM_ID_VIDEO_1);
   PH.vehicle_id_src = wfbohd_radio_generate_vehicle_id(uFreqKhz);
   PH.total_length = sizeof(t_packet_header) + sizeof(t_packet_header_video_full_77);
   PH.radio_link_packet_index = radio_get_next_radio_link_packet_index(0);

   PHVF.video_link_profile = 0; // HQ
   PHVF.video_stream_and_type = 0 | (VIDEO_TYPE_OPENIPC<<4);
   PHVF.video_width = 1280;
   PHVF.video_height = 720;
   PHVF.video_fps = 30;
   PHVF.block_packets = pFEC->k;
   PHVF.block_fecs = pFEC->n - pFEC->k;
   PHVF.video_data_length = uVideoSize;
   PHVF.video_block_index = uBlockIndex;
   PHVF.video_block_packet_index = uFragmentIndex;

   static uint8_t s_uFakeVideoPacketBuffer[MAX_PACKET_PAYLOAD];
   memcpy(s_uFakeVideoPacketBuffer, &PH, sizeof(t_packet_header));
   memcpy(s_uFakeVideoPacketBuffer + sizeof(t_packet_header), &PHVF, sizeof(t_packet_header_video_full_77));
   radio_packet_compute_crc(s_uFakeVideoPacketBuffer, PH.total_length);

   // End generate fake video packet
   // ---------------------------------------------------------------


   // Save the new received video fragment

   memcpy(s_RxWFBVideoBlocks[iStackIndexToAddTo].pFragments[uFragmentIndex], pData, iDataLength);
   s_RxWFBVideoBlocks[iStackIndexToAddTo].iFragmentsReceived[uFragmentIndex] = 1;
   s_RxWFBVideoBlocks[iStackIndexToAddTo].iFragmentsSize[uFragmentIndex] = iDataLength;
   if ( uFragmentIndex < pFEC->k )
      s_RxWFBVideoBlocks[iStackIndexToAddTo].uReceivedDataPackets++;
   else
      s_RxWFBVideoBlocks[iStackIndexToAddTo].uReceivedECPackets++;

   // Can't output it and can't reconstruct it and no other blocks? Then just do nothing
   if ( (iStackIndexToAddTo == 0 ) && ( s_RxWFBVideoBlocks[iStackIndexToAddTo].uReceivedDataPackets + s_RxWFBVideoBlocks[iStackIndexToAddTo].uReceivedECPackets < pFEC->k ) )
      return s_uFakeVideoPacketBuffer;

   // First block in stack is not complete. Can't output it or recontruct it. Do nothing.
   if ( s_RxWFBVideoBlocks[0].uReceivedDataPackets + s_RxWFBVideoBlocks[0].uReceivedECPackets < pFEC->k )
      return s_uFakeVideoPacketBuffer;

   // We have at least k packets for first block;
   // Reconstruct it (if needed) and output the block

   while ( (s_iTopWFBVideoBlockStackIndex >= 0) && (s_RxWFBVideoBlocks[0].uReceivedDataPackets + s_RxWFBVideoBlocks[0].uReceivedECPackets >= pFEC->k) )
   {
      // Do reconstruction if needed
      if ( s_RxWFBVideoBlocks[0].uReceivedDataPackets < pFEC->k )
          _wfb_radio_do_ec(pFEC, 0);
   
      
      if ( -1 != s_iSocketWFBOHDUDPVideo )
      for( int i=0; i<pFEC->k; i++ )
      {
         pPacketHeader = (type_wfb_packet_header*)(s_RxWFBVideoBlocks[0].pFragments[i]);
         uVideoSize = s_RxWFBVideoBlocks[0].iFragmentsSize[i] - sizeof(type_wfb_packet_header);

         send(s_iSocketWFBOHDUDPVideo, s_RxWFBVideoBlocks[0].pFragments[i] + sizeof(type_wfb_packet_header), uVideoSize, MSG_DONTWAIT); 
      }

      s_uLastOutputedOpenIPCVideoBlockIndex = s_RxWFBVideoBlocks[0].uVideoBlockIndex;

      // Remove this block from stack
      _wfb_rx_stack_discard_blocks(pFEC->n, 1);
   }

   return s_uFakeVideoPacketBuffer;
}

uint8_t* radiopackets_wfbohd_generate_fake_ruby_telemetry_header(uint32_t uTimeNow, uint32_t uFreqKhz)
{
   if ( uTimeNow < s_uLastTimeGeneratedFakeRubyTelemetry + 250 )
      return NULL;

   s_uLastTimeGeneratedFakeRubyTelemetry = uTimeNow;

   memset(&s_FakeRubyTelemetryPacket, 0, sizeof(t_packet_header_ruby_telemetry_extended_v3));
   s_FakeRubyTelemetryPacket.flags = FLAG_RUBY_TELEMETRY_VEHICLE_HAS_CAMERA | FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;
   s_FakeRubyTelemetryPacket.version = (8<<4);
   s_FakeRubyTelemetryPacket.uVehicleId = wfbohd_radio_generate_vehicle_id(uFreqKhz);
   s_FakeRubyTelemetryPacket.vehicle_type = MODEL_TYPE_GENERIC | (MODEL_FIRMWARE_TYPE_OPENIPC<<5);
   strcpy((char*)s_FakeRubyTelemetryPacket.vehicle_name, "OpenIPC");
   s_FakeRubyTelemetryPacket.radio_links_count = 1;
   s_FakeRubyTelemetryPacket.uRadioFrequenciesKhz[0] = uFreqKhz;
   s_FakeRubyTelemetryPacket.uRelayLinks = 0;
   s_FakeRubyTelemetryPacket.downlink_tx_video_bitrate_bps = 5000000;
   s_FakeRubyTelemetryPacket.downlink_tx_video_all_bitrate_bps = 5000000;
   s_FakeRubyTelemetryPacket.downlink_tx_data_bitrate_bps = 1000;
   return (uint8_t*) &s_FakeRubyTelemetryPacket;
}

int radiopackets_wfbohd_generate_fake_full_ruby_telemetry_packet(uint32_t uFreqKhz, uint8_t* pRubyTelemetryHeader, uint8_t* pOutput)
{
   if ( (NULL == pRubyTelemetryHeader) || (NULL == pOutput) )
      return 0;

   t_packet_header PH;
   memset(&PH, 0, sizeof(t_packet_header));

   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_RUBY_TELEMETRY_EXTENDED, STREAM_ID_DATA);
   PH.vehicle_id_src = wfbohd_radio_generate_vehicle_id(uFreqKhz);
   PH.total_length = sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v3);
   PH.radio_link_packet_index = radio_get_next_radio_link_packet_index(0);
   memcpy(pOutput, &PH, sizeof(t_packet_header));
   memcpy(pOutput + sizeof(t_packet_header), pRubyTelemetryHeader, sizeof(t_packet_header_ruby_telemetry_extended_v3));
   radio_packet_compute_crc(pOutput, PH.total_length);
   return PH.total_length;
}

#endif