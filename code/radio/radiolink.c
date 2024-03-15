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

#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include "../base/base.h"
#include "../base/encr.h"
#include "../base/hardware.h"
#include "../base/hardware_radio_serial.h"
#include "../base/hw_procs.h"
#include "../common/string_utils.h"
#include "radiotap.h"
#include <time.h>
#include <sys/resource.h>
#include "radiolink.h"
#include "radiopackets2.h"
#include "radio_rx.h"

//#define DEBUG_PACKET_RECEIVED
//#define DEBUG_PACKET_SENT

#define USE_PCAP_FOR_TX 1

int s_bRadioDebugFlag = 0;
int s_iRadioInterfacesBroken = 0;
int s_iRadioLastReadErrorCode = RADIO_READ_ERROR_NO_ERROR;

u8 sPayloadBufferRead[MAX_PACKET_LENGTH_PCAP];
int sEnableCRCGen = 0;

int sRadioDataRate_bps = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS; // positive: clasic in bps; negative MCS (starts from -1)
u32 sRadioFrameFlags = 0;

int sAutoIncrementPacketCounter = 1;
u32 sRadioReceivedFramesType = RADIO_FLAGS_FRAME_TYPE_DATA;
u32 sRadioLastReceivedHeadersLength = 0;

int s_iLastProcessingErrorCode = 0;

int s_iLogCount_RadioRate = 0;
int s_iLogCount_RadioFlags = 0;
u32 s_uLastTimeLog_RadioRate = 0;
u32 s_uLastTimeLog_RadioFlags = 0;
u32 s_uPacketsSentUsingCurrent_RadioRate = 0;
u32 s_uPacketsSentUsingCurrent_RadioFlags = 0;
u32 s_uLastPacketSentRadioTapHeaderLength = 0;
u32 s_uLastPacketSentIEEEHeaderLength = 0;

u32 s_uNextRadioPacketIndexes[MAX_RADIO_INTERFACES];

u32 s_uTimesLastRadioPacketsOnSlowLink[256][MAX_RADIO_INTERFACES];
u32 s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[256]; // in milisec, how often or none can send a short packet of this type from vehicle to controller, MAX_U32 for none
u32 s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[256]; // in milisec, how often or none can send a short packet of this type from controller to vehicle, MAX_U32 for none

#define MAX_LOW_DATALINK_PACKETS_HISTORY 100

u32 s_uTimesLastLowDataLinkPacketsOnInterfaces[MAX_RADIO_INTERFACES][MAX_LOW_DATALINK_PACKETS_HISTORY];
u32 s_uSizesLastLowDataLinkPacketsOnInterfaces[MAX_RADIO_INTERFACES][MAX_LOW_DATALINK_PACKETS_HISTORY];
u32 s_uIndexLastLowDataLinkPackets[MAX_RADIO_INTERFACES];

pthread_mutex_t s_pMutexRadioSyncRxTxThreads;
int s_iMutexRadioSyncRxTxThreadsInitialized = 0;


u8 s_uRadiotapHeaderLegacy[] = {
	0x00, 0x00, // <-- radiotap version
	0x0c, 0x00, // <- radiotap header length (2 bytes), 12 bytes (this header)
	0x04, 0x80, 0x00, 0x00, // <-- radiotap presence flags (rate + tx flags)
	0x00, 0x08, // datarate and tx flags (data rate will be overwritten later in packet_header_init) (tx flags was 0x08)
	0x00, 0x00 // ??
};

u8 s_uRadiotapHeaderMCS[] = {
    0x00, 0x00,             // <-- radiotap version
    0x0d, 0x00,             // <- radiotap header length (2 bytes), 13 bytes (this header)
    0x00, 0x80, 0x08, 0x00, // <-- radiotap presence flags (tx flags (0x8000), mcs (0x080000)) (4 bytes)
    0x08, 0x00,             // tx-flags (2 bytes) (has IEEE80211_RADIOTAP_F_TX_NOACK = 0x0008)
    0x37,                   // byte 11: mcs have: bw, gi, stbc ,fec (known flags, 1 byte)
    0x30,                   // byte 12: mcs: 20MHz bw, long guard interval, stbc, ldpc (flags, 1 byte)
    0x00,                   // byte 13: mcs index 0 (speed level, will be overwritten later)
};
 

u8 s_uIEEEHeaderData[] = {
	0x08, 0x01, 0x00, 0x00, // frame control field (2bytes), duration (2 bytes)
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // port = 1st byte of IEEE802.11 RA (mac) must be something odd (wifi hardware determines broadcast/multicast through odd/even check)
	0x13, 0x12, 0x34, 0x56, 0x78, 0x90, // mac
	0x13, 0x12, 0x34, 0x56, 0x78, 0x90, // mac
	0x00, 0x00 // IEEE802.11 seqnum, (will be overwritten later by Atheros firmware/wifi chip)
};
 
u8 s_uIEEEHeaderRTS[] = {
	0xb4, 0x01, 0x00, 0x00, // frame control field (2 bytes), duration (2 bytes)
	0xff, //  port = 1st byte of IEEE802.11 RA (mac) must be something odd (wifi hardware determines broadcast/multicast through odd/even check)
};

u8 s_uIEEEHeaderData_short[] = {
	0x08, 0x01, 0x00, 0x00, // frame control field (2bytes), duration (2 bytes)
	0xff // port =  1st byte of IEEE802.11 RA (mac) must be something odd (wifi hardware determines broadcast/multicast through odd/even check)
};

uint16_t uIEEEE80211SeqNb = 0; 

int _radio_encode_port(int port)
{
   //return (port * 2) + 1;
   return ((port<<4) | 0x0F);
}

void radio_init_link_structures()
{
   log_line("[Radio] Initialize.");

   radio_packets_short_init();

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_uNextRadioPacketIndexes[i] = 0;

   radio_reset_packets_default_frequencies(0);

   for( int i=0; i<256; i++ )
   {
      for( int k=0; k<MAX_RADIO_INTERFACES; k++ )
         s_uTimesLastRadioPacketsOnSlowLink[i][k] = 0;
   }

   // Init packets history on low data links

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      s_uIndexLastLowDataLinkPackets[i] = 0;
      for( int k=0; k<MAX_LOW_DATALINK_PACKETS_HISTORY; k++ )
      {
         s_uTimesLastLowDataLinkPacketsOnInterfaces[i][k] = 0;
         s_uSizesLastLowDataLinkPacketsOnInterfaces[i][k] = 0;
      }
   }

#ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   log_line("[Radio] Initialize and use radio rxtx threads syncronization.");
   if ( 0 == s_iMutexRadioSyncRxTxThreadsInitialized )
   {
      if ( 0 != pthread_mutex_init(&s_pMutexRadioSyncRxTxThreads, NULL) )
         log_softerror_and_alarm("[Radio] Failed to init RxTx threads sync mutex.");
      else
         s_iMutexRadioSyncRxTxThreadsInitialized = 1;
   }
#else
   log_line("[Radio] Do not use radio rxtx threads syncronization.");
#endif
}

void radio_link_cleanup()
{
   if ( s_iMutexRadioSyncRxTxThreadsInitialized )
   {
      pthread_mutex_destroy(&s_pMutexRadioSyncRxTxThreads);
      s_iMutexRadioSyncRxTxThreadsInitialized = 0;
   }
}

void radio_reset_packets_default_frequencies(int iRCEnabled)
{
   for( int i=0; i<256; i++ )
   {
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[i] = 500;
      s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[i] = 500;
   }


   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_RUBY_PING_CLOCK] = 100;
   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_RUBY_PING_CLOCK_REPLY] = 100;

   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_PING_CLOCK] = 100;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_PING_CLOCK_REPLY] = 100;

   // Controller to vehicle

   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_RUBY_PAIRING_REQUEST] = 200;
   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_COMMAND] = 200;
   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL] = 50;
   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE] = 50;
   s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[PACKET_TYPE_RC_FULL_FRAME] = 50;

   
   // Vehicle to controller

   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_PAIRING_CONFIRMATION] = 200;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_COMMAND_RESPONSE] = 200;
   
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_SHORT] = 300;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_EXTENDED] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_TELEMETRY] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_TELEMETRY_EXTENDED] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_RC_CHANNELS] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RC_TELEMETRY] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY] = 400;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY] = 400;
   
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_ALARM] = 100;

   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK] = 50;
   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK] = 50;

   s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_MODEL_SETTINGS] = 0; // Send at any rate

   if( iRCEnabled )
   {
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_SHORT] = 330;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_EXTENDED] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_TELEMETRY] = 330;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_TELEMETRY_EXTENDED] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_FC_RC_CHANNELS] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RC_TELEMETRY] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY] = 400;
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY] = 400;
      
      s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[PACKET_TYPE_RUBY_ALARM] = 100;
   }
}

void radio_enable_crc_gen(int enable)
{
   sEnableCRCGen = enable;
   log_line("Set radio enable crc generation to: %d", sEnableCRCGen);
}


void radio_set_debug_flag()
{
   s_bRadioDebugFlag = 1;
}

// Returns 0 if the packet can't be sent (right now or ever)

int radio_can_send_packet_on_slow_link(int iLinkId, int iPacketType, int iFromController, u32 uTimeNow)
{
   if ( (iPacketType < 1) || (iPacketType > 254) )
      return 0;
   if ( (iLinkId < 0) || (iLinkId >= MAX_RADIO_INTERFACES) )
      return 0;

   if ( (iFromController) && (s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[iPacketType] != MAX_U32) )
   if ( uTimeNow >= s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] + s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[iPacketType] )
   {
      s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] = uTimeNow;
      return 1;
   }

   if ( (iFromController) && (s_uFrequencyRadioPacketsOnSlowLinkControllerToVehicle[iPacketType] == 0) )
   {
      s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] = uTimeNow;
      return 1;
   }
   
   if ( (! iFromController) && (s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[iPacketType] != MAX_U32) )
   if ( uTimeNow >= s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] + s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[iPacketType] )
   {
      s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] = uTimeNow;
      return 1;
   }

   if ( (! iFromController) && (s_uFrequencyRadioPacketsOnSlowLinkVehicleToController[iPacketType] == 0) )
   {
      s_uTimesLastRadioPacketsOnSlowLink[iPacketType][iLinkId] = uTimeNow;
      return 1;
   }
   return 0;
}

int radio_get_last_second_sent_bps_for_radio_interface(int iInterfaceIndex, u32 uTimeNow)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;

   int iTotalBPS = 0;
   int iCount = 0;
   int iIndex = s_uIndexLastLowDataLinkPackets[iInterfaceIndex];
   uTimeNow -= 1000;

   while ( (s_uTimesLastLowDataLinkPacketsOnInterfaces[iInterfaceIndex][iIndex] >= uTimeNow) && (iCount<MAX_LOW_DATALINK_PACKETS_HISTORY) )
   {
      iTotalBPS += 8 * s_uSizesLastLowDataLinkPacketsOnInterfaces[iInterfaceIndex][iIndex];
      iCount++;
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_LOW_DATALINK_PACKETS_HISTORY-1;
   }
   return iTotalBPS;
}

int radio_get_last_500ms_sent_bps_for_radio_interface(int iInterfaceIndex, u32 uTimeNow)
{
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;
   if ( (iInterfaceIndex < 0) || (iInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return 0;

   int iTotalBPS = 0;
   int iCount = 0;
   int iIndex = s_uIndexLastLowDataLinkPackets[iInterfaceIndex];
   uTimeNow -= 500;

   while ( (s_uTimesLastLowDataLinkPacketsOnInterfaces[iInterfaceIndex][iIndex] >= uTimeNow) && (iCount<MAX_LOW_DATALINK_PACKETS_HISTORY) )
   {
      iTotalBPS += 8 * s_uSizesLastLowDataLinkPacketsOnInterfaces[iInterfaceIndex][iIndex];
      iCount++;
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_LOW_DATALINK_PACKETS_HISTORY-1;
   }
   return iTotalBPS;
}

int radio_set_out_datarate(int rate_bps)
{
   int nReturn = 0;

   if ( rate_bps == 0 )
      rate_bps = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS;

   if ( (sRadioDataRate_bps != rate_bps) || (s_iLogCount_RadioRate == 0) )
   if ( s_iLogCount_RadioRate < 4 || (s_uLastTimeLog_RadioRate + 20000) < get_current_timestamp_ms() )
   {
      if ( s_iLogCount_RadioRate == 4 )
         s_iLogCount_RadioRate = 0;
      char szBuff[64];
      char szBuff2[64];
      str_getDataRateDescription(rate_bps, szBuff);
      str_getDataRateDescription(sRadioDataRate_bps, szBuff2);
      log_line("Radio: Set TX DR to: %s (prev was: %s, %u pckts sent on it)", szBuff, szBuff2, s_uPacketsSentUsingCurrent_RadioRate);
      if ( s_iLogCount_RadioRate == 3 )
         log_line("Radio: Too many radio datarate changes, pausing log.");
      s_iLogCount_RadioRate++;
      if ( get_current_timestamp_ms() > s_uLastTimeLog_RadioRate + 1000 )
         s_iLogCount_RadioRate = 0;
      s_uLastTimeLog_RadioRate = get_current_timestamp_ms();
   }

   if ( sRadioDataRate_bps != rate_bps )
   {
      s_uPacketsSentUsingCurrent_RadioRate = 0;
      nReturn = 1;
   }

   sRadioDataRate_bps = rate_bps;

   if ( sRadioDataRate_bps > 0 )
   {
      if ( sRadioDataRate_bps > 56 )
      {
         s_uRadiotapHeaderLegacy[8] = (uint8_t)((int)(sRadioDataRate_bps/1000/1000) * 2);
      }
      else if ( sRadioDataRate_bps <= 56 )
      {
         s_uRadiotapHeaderLegacy[8] = (uint8_t)(sRadioDataRate_bps*2);
         if ( 5 == sRadioDataRate_bps )
            s_uRadiotapHeaderLegacy[8] = (uint8_t)11;
      }
   }
   else
   {
      int mcsRate = -sRadioDataRate_bps-1;
      if ( mcsRate < 0 )
         mcsRate = 0;
      s_uRadiotapHeaderMCS[12] = (uint8_t)mcsRate;
   }

   return nReturn;
}

void radio_set_frames_flags(u32 frameFlags)
{
   u32 frameFlagsFiltered = frameFlags;

   if ( (sRadioFrameFlags != frameFlagsFiltered) || (s_iLogCount_RadioFlags == 0) )
   if ( s_iLogCount_RadioFlags < 10 || (s_uLastTimeLog_RadioFlags + 20000) < get_current_timestamp_ms() )
   {
      if ( s_iLogCount_RadioFlags == 10 )
         s_iLogCount_RadioFlags = 0;
      char szBuff[128];
      char szBuff2[128];
      str_get_radio_frame_flags_description(frameFlags, szBuff);
      str_get_radio_frame_flags_description(sRadioFrameFlags, szBuff2);
      log_line("Radio: Set radio frames flags to 0x%04X: %s (previous was: %s, %u packets sent on it)", frameFlags, szBuff, szBuff2, s_uPacketsSentUsingCurrent_RadioFlags);
      if ( s_iLogCount_RadioFlags == 9 )
         log_line("Radio: Too many radio flags changes, pausing log.");
      s_iLogCount_RadioFlags++;
      if ( get_current_timestamp_ms() > s_uLastTimeLog_RadioFlags + 1000 )
         s_iLogCount_RadioFlags = 0;
      s_uLastTimeLog_RadioFlags = get_current_timestamp_ms();
   }

   if ( sRadioFrameFlags != frameFlagsFiltered )
      s_uPacketsSentUsingCurrent_RadioFlags = 0;

   sRadioFrameFlags = frameFlagsFiltered;

   if ( (sRadioFrameFlags & RADIO_FLAGS_MCS_MASK) || (sRadioDataRate_bps < 0) )
   {
      u8 mcs_flags = 0;
      u8 mcs_known = (IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_BW | IEEE80211_RADIOTAP_MCS_HAVE_GI | IEEE80211_RADIOTAP_MCS_HAVE_STBC);
      mcs_known |= IEEE80211_RADIOTAP_MCS_HAVE_FEC;
      int mcsRate = -sRadioDataRate_bps-1;
      if ( mcsRate < 0 )
         mcsRate = 0;

      if ( hardware_is_station() )
      {
         if ( sRadioFrameFlags & RADIO_FLAG_HT40_CONTROLLER )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_BW_40;
         else
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_BW_20;

         if ( sRadioFrameFlags & RADIO_FLAG_LDPC_CONTROLLER )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_FEC_LDPC; 
         if ( sRadioFrameFlags & RADIO_FLAG_SGI_CONTROLLER )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_SGI;
         if ( sRadioFrameFlags & RADIO_FLAG_STBC_CONTROLLER )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_STBC_1 << IEEE80211_RADIOTAP_MCS_STBC_SHIFT;
      }
      else
      {
         if ( sRadioFrameFlags & RADIO_FLAG_HT40_VEHICLE )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_BW_40;
         else
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_BW_20;

         if ( sRadioFrameFlags & RADIO_FLAG_LDPC_VEHICLE )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_FEC_LDPC; 
         if ( sRadioFrameFlags & RADIO_FLAG_SGI_VEHICLE )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_SGI;
         if ( sRadioFrameFlags & RADIO_FLAG_STBC_VEHICLE )
            mcs_flags = mcs_flags | IEEE80211_RADIOTAP_MCS_STBC_1 << IEEE80211_RADIOTAP_MCS_STBC_SHIFT;       
      }
      s_uRadiotapHeaderMCS[10] = mcs_known;
      s_uRadiotapHeaderMCS[11] = mcs_flags;
      s_uRadiotapHeaderMCS[12] = (uint8_t)mcsRate;
    }
}


u32 radio_get_received_frames_type()
{
   return sRadioReceivedFramesType;
}

int radio_interfaces_broken()
{
   return s_iRadioInterfacesBroken;
}

int radio_get_last_read_error_code()
{
   return s_iRadioLastReadErrorCode; 
}

int _radio_open_interface_for_read_with_filter(int interfaceIndex, char* szFilter, char* szFilterPrism)
{
   s_iRadioInterfacesBroken = 0;

   struct bpf_program bpfprogram;
   char szProgram[512];
   char szErrbuf[PCAP_ERRBUF_SIZE];

   if ( (NULL == szFilter) || (NULL == szFilterPrism) )
   {
      log_softerror_and_alarm("Invalid filters for opening radio interface for read.");
      return -1;
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      log_softerror_and_alarm("Invalid radio interface hardware info to open for interface index %d", interfaceIndex+1);
      return -1;
   }

   if ( ! hardware_radio_is_wifi_radio(pRadioHWInfo) )
   {
      log_softerror_and_alarm("Tried to open a non 2.4/5.8 radio in high capacity 2.4/5.8 mode.");
      return -1;
   }

   pRadioHWInfo->openedForRead = 0;
   pRadioHWInfo->monitor_interface_read.selectable_fd = -1;
   pRadioHWInfo->monitor_interface_read.iErrorCount = 0;

   szErrbuf[0] = '\0';
   //pRadioHWInfo->monitor_interface_read.ppcap = pcap_open_live(pRadioHWInfo->szName, 4096, 1, 1, szErrbuf);
   pRadioHWInfo->monitor_interface_read.ppcap = pcap_create(pRadioHWInfo->szName, szErrbuf);

   if (pRadioHWInfo->monitor_interface_read.ppcap == NULL)
   {
      log_softerror_and_alarm("Unable to open [%s]: %s", pRadioHWInfo->szName, szErrbuf);
      return -1;
   }
   if ( pcap_set_snaplen(pRadioHWInfo->monitor_interface_read.ppcap, 4096) !=0 )
      log_softerror_and_alarm("Error setting [%s] snap buffer length: %s", pRadioHWInfo->szName, pcap_geterr(pRadioHWInfo->monitor_interface_read.ppcap));

   if ( pcap_set_promisc(pRadioHWInfo->monitor_interface_read.ppcap, 1) != 0 )
      log_softerror_and_alarm("Error setting [%s] to promiscous mode: %s", pRadioHWInfo->szName, pcap_geterr(pRadioHWInfo->monitor_interface_read.ppcap));

   if ( pcap_set_timeout(pRadioHWInfo->monitor_interface_read.ppcap, -1) !=0) 
      log_softerror_and_alarm("Error setting [%s] timeout: %s", pRadioHWInfo->szName, pcap_geterr(pRadioHWInfo->monitor_interface_read.ppcap));
   
   if ( pcap_set_immediate_mode(pRadioHWInfo->monitor_interface_read.ppcap, 1) != 0 )
      log_softerror_and_alarm("Error setting [%s] to immediate mode: %s", pRadioHWInfo->szName, pcap_geterr(pRadioHWInfo->monitor_interface_read.ppcap));
   
   if ( pcap_activate(pRadioHWInfo->monitor_interface_read.ppcap) !=0) 
      log_softerror_and_alarm("Error setting [%s] to immediate mode: %s", pRadioHWInfo->szName, pcap_geterr(pRadioHWInfo->monitor_interface_read.ppcap));
    
   if ( pcap_setnonblock(pRadioHWInfo->monitor_interface_read.ppcap, 1, szErrbuf) < 0 )
      log_softerror_and_alarm("Error setting [%s] to nonblocking mode: %s", pRadioHWInfo->szName, szErrbuf);
        
   if ( pcap_setdirection(pRadioHWInfo->monitor_interface_read.ppcap, PCAP_D_IN) < 0 )
      log_softerror_and_alarm("Error setting [%s] direction", pRadioHWInfo->szName);

   int nLinkEncap = pcap_datalink(pRadioHWInfo->monitor_interface_read.ppcap);

   if (nLinkEncap == DLT_IEEE802_11_RADIO)
      sprintf(szProgram, "%s", szFilter);
   else if (nLinkEncap == DLT_PRISM_HEADER)
      sprintf(szProgram, "%s", szFilterPrism);
   else
   {
      log_softerror_and_alarm("ERROR: unknown encapsulation on [%s]! check if monitor mode is supported and enabled", pRadioHWInfo->szName);
      return -1;
   }

   if (pcap_compile(pRadioHWInfo->monitor_interface_read.ppcap, &bpfprogram, szProgram, 1, 0) == -1)
   {
      puts(szProgram);
      puts(pcap_geterr(pRadioHWInfo->monitor_interface_read.ppcap));
      log_softerror_and_alarm("ERROR: compiling program for interface [%s]", pRadioHWInfo->szName);
      return -1;
   }
   else
   {
      if (pcap_setfilter(pRadioHWInfo->monitor_interface_read.ppcap, &bpfprogram) == -1)
      {
         log_softerror_and_alarm("Failed to set pcap filter: %s", szProgram);
         log_softerror_and_alarm("Failed to set pacp filter: %s", pcap_geterr(pRadioHWInfo->monitor_interface_read.ppcap));
      }
      pcap_freecode(&bpfprogram);
   }
   pRadioHWInfo->monitor_interface_read.selectable_fd = pcap_get_selectable_fd(pRadioHWInfo->monitor_interface_read.ppcap);
   pRadioHWInfo->monitor_interface_read.radioInfo.nDbm = -127;
   pRadioHWInfo->monitor_interface_read.radioInfo.nDbmNoise = -127;
   pRadioHWInfo->openedForRead = 1;

   log_line("Opened radio interface %d (%s) for reading on %s, filter: [%s]. Returned fd=%d, ppcap: %d", interfaceIndex+1, pRadioHWInfo->szName, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFilter, pRadioHWInfo->monitor_interface_read.selectable_fd, pRadioHWInfo->monitor_interface_read.ppcap);

   return pRadioHWInfo->monitor_interface_read.selectable_fd;
}

int radio_open_interface_for_read(int interfaceIndex, int portNumber)
{
   char szFilter[256];
   char szFilterPrism[256];

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
      return -1;

   int port_encoded = _radio_encode_port(portNumber);
   sprintf(szFilter, "ether[0x00:2] == 0x0801 && ether[0x0a:4] == 0x13123456 && ether[0x04:1] == 0x%.2x", port_encoded);
   sprintf(szFilterPrism, "radio[0x40:2] == 0x0801 && radio[0x4a:4] == 0x13123456 && radio[0x44:1] == 0x%.2x", port_encoded);

   int iResult = _radio_open_interface_for_read_with_filter(interfaceIndex, szFilter, szFilterPrism);
   
   if ( iResult < 0 )
      return iResult;

   pRadioHWInfo->monitor_interface_read.nPort = portNumber;

   if ( portNumber == RADIO_PORT_ROUTER_UPLINK )
      log_line("Opened radio interface %d (%s) for reading on uplink on %s. Returned fd=%d, ppcap: %d", interfaceIndex+1, pRadioHWInfo->szName, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), pRadioHWInfo->monitor_interface_read.selectable_fd, pRadioHWInfo->monitor_interface_read.ppcap);
   else if ( portNumber == RADIO_PORT_ROUTER_DOWNLINK )
      log_line("Opened radio interface %d (%s) for reading on downlink on %s. Returned fd=%d, ppcap: %d", interfaceIndex+1, pRadioHWInfo->szName, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), pRadioHWInfo->monitor_interface_read.selectable_fd, pRadioHWInfo->monitor_interface_read.ppcap);
   else
      log_line("Opened radio interface %d (%s) for reading on %s. Returned fd=%d, ppcap: %d", interfaceIndex+1, pRadioHWInfo->szName, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), pRadioHWInfo->monitor_interface_read.selectable_fd, pRadioHWInfo->monitor_interface_read.ppcap);

   return pRadioHWInfo->monitor_interface_read.selectable_fd;
}

int radio_open_interface_for_read_wfbohd(int interfaceIndex, int iChannelId)
{
   char szFilter[256];
   char szFilterPrism[256];

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
      return -1;

   int addr_encoded = iChannelId;
   sprintf(szFilter, "ether[0x00:2] == 0x0801 && ether[0x0a:2] == 0x5742 && ether[0x0c:4] == 0x%08x", addr_encoded);
   sprintf(szFilterPrism, "radio[0x40:2] == 0x0801 && radio[0x4a:2] == 0x5742 && radio[0x4c:4] == 0x%08x", addr_encoded);

   int iResult = _radio_open_interface_for_read_with_filter(interfaceIndex, szFilter, szFilterPrism);
   
   if ( iResult < 0 )
      return iResult;

   pRadioHWInfo->monitor_interface_read.nPort = 0;

   log_line("Opened radio interface %d (%s) for reading on %s on wfbohd channel id %d. Returned fd=%d, ppcap: %d", interfaceIndex+1, pRadioHWInfo->szName, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), iChannelId, pRadioHWInfo->monitor_interface_read.selectable_fd, pRadioHWInfo->monitor_interface_read.ppcap);
   
   return pRadioHWInfo->monitor_interface_read.selectable_fd;
}

int radio_open_interface_for_write(int interfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
      return -1;

   if ( ! hardware_radio_is_wifi_radio(pRadioHWInfo) )
   {
      log_softerror_and_alarm("Tried to open a non 2.4/5.8 radio in high capacity 2.4/5.8 mode.");
      return -1;
   }

   pRadioHWInfo->openedForWrite = 0;
   pRadioHWInfo->monitor_interface_write.selectable_fd = -1;
   pRadioHWInfo->monitor_interface_write.iErrorCount = 0;

   #ifdef USE_PCAP_FOR_TX

   char errbuf[PCAP_ERRBUF_SIZE];

   pRadioHWInfo->monitor_interface_write.ppcap = pcap_create(pRadioHWInfo->szName, errbuf);
   if (pRadioHWInfo->monitor_interface_write.ppcap == NULL)
   {
      log_error_and_alarm("Failed to get ppcap for write");
      return -1;
   }
   if (pcap_set_snaplen(pRadioHWInfo->monitor_interface_write.ppcap, 4096) !=0) log_line("set_snaplen failed");
   if (pcap_set_promisc(pRadioHWInfo->monitor_interface_write.ppcap, 1) != 0) log_line("set_promisc failed");
   if (pcap_set_timeout(pRadioHWInfo->monitor_interface_write.ppcap, -1) !=0) log_line("set_timeout failed");
   if (pcap_set_immediate_mode(pRadioHWInfo->monitor_interface_write.ppcap, 1) != 0) log_line("pcap_set_immediate_mode failed: %s", pcap_geterr(pRadioHWInfo->monitor_interface_write.ppcap));
   if (pcap_activate(pRadioHWInfo->monitor_interface_write.ppcap) !=0) log_line("pcap_activate failed: %s", pcap_geterr(pRadioHWInfo->monitor_interface_write.ppcap));

   pRadioHWInfo->monitor_interface_write.selectable_fd = pcap_get_selectable_fd(pRadioHWInfo->monitor_interface_write.ppcap);
   log_line("PCAP returned a selectable fd for write for interface %d: ppcap: %d, fd=%d", interfaceIndex + 1, pRadioHWInfo->monitor_interface_write.ppcap, pRadioHWInfo->monitor_interface_write.selectable_fd);
   
   //if ( pRadioHWInfo->openedForRead )
   //   pRadioHWInfo->monitor_interface_write.selectable_fd = pRadioHWInfo->monitor_interface_read.selectable_fd;
   //else
   //   log_line("Failed to reuse read interface for write.");

   #else
   
   struct sockaddr_ll ll_addr;
   struct ifreq ifr;
   pRadioHWInfo->monitor_interface_write.selectable_fd = socket(AF_PACKET, SOCK_RAW, 0);
   if (pRadioHWInfo->monitor_interface_write.selectable_fd == -1)
   {
      log_error_and_alarm("Error:\tSocket failed");
      return -1;
   }
   ll_addr.sll_family = AF_PACKET;
   ll_addr.sll_protocol = 0;
   ll_addr.sll_halen = ETH_ALEN;

   if ( strlen(pRadioHWInfo->szName) < IFNAMSIZ-1 )
      strcpy(ifr.ifr_name, pRadioHWInfo->szName);
   else
   {
      char szBuff[64];
      strcpy(szBuff, pRadioHWInfo->szName);
      szBuff[IFNAMSIZ-1] = 0;
      strcpy(ifr.ifr_name, szBuff);
   }
   if (ioctl(pRadioHWInfo->monitor_interface_write.selectable_fd, SIOCGIFINDEX, &ifr) < 0)
   {
     	log_error_and_alarm("Error:\tioctl(SIOCGIFINDEX) failed");
     	return -1;
   }
   ll_addr.sll_ifindex = ifr.ifr_ifindex;
   if (ioctl(pRadioHWInfo->monitor_interface_write.selectable_fd, SIOCGIFHWADDR, &ifr) < 0)
   {
      	log_error_and_alarm("Error:\tioctl(SIOCGIFHWADDR) failed");
      	return -1;
   }
   memcpy(ll_addr.sll_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
   if (bind (pRadioHWInfo->monitor_interface_write.selectable_fd, (struct sockaddr *)&ll_addr, sizeof(ll_addr)) == -1)
   {
     	log_error_and_alarm("Error:\tbind failed");
     	close(pRadioHWInfo->monitor_interface_write.selectable_fd);
      pRadioHWInfo->monitor_interface_write.selectable_fd = -1;
	     return -1;
   }
   if (pRadioHWInfo->monitor_interface_write.selectable_fd == -1 ) 
   {
       log_error_and_alarm("Error:\tCannot open socket.\tInfo: Must be root with an 802.11 card with RFMON enabled");
       return -1;
   }
   log_line("Opened socket for write fd=%d.", pRadioHWInfo->monitor_interface_write.selectable_fd);
   #endif

   pRadioHWInfo->openedForWrite = 1;
   log_line("Opened radio interface %d (%s) for writing on %s. Returned fd=%d", interfaceIndex+1, pRadioHWInfo->szName, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), pRadioHWInfo->monitor_interface_write.selectable_fd);
   return pRadioHWInfo->monitor_interface_write.selectable_fd;
}

void radio_close_interface_for_read(int interfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      log_softerror_and_alarm("Trying to close invalid interface index. Interface index: %d", interfaceIndex+1);         
      return;
   }

   radio_rx_pause_interface(interfaceIndex);
   
   if ( NULL != pRadioHWInfo->monitor_interface_read.ppcap )
   {
      log_line("Closed radio interface %d [%s] that was used for read, selectable read fd: %d", interfaceIndex+1, pRadioHWInfo->szName, pRadioHWInfo->monitor_interface_read.selectable_fd);
      pcap_close(pRadioHWInfo->monitor_interface_read.ppcap);
   }
   else
      log_line("Radio interface %d was not opened for read.", interfaceIndex+1);

   pRadioHWInfo->monitor_interface_read.ppcap = NULL;
   pRadioHWInfo->monitor_interface_read.selectable_fd = -1;
   pRadioHWInfo->monitor_interface_read.iErrorCount = 0;
   pRadioHWInfo->openedForRead = 0;

   radio_rx_resume_interface(interfaceIndex);
}

void radio_close_interfaces_for_read()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL != pRadioHWInfo) && pRadioHWInfo->openedForRead )
         radio_close_interface_for_read(i);
   }
   log_line("Closed all radio interfaces used for read (in pcap mode).");
}

void radio_close_interface_for_write(int interfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo )
   {
      log_softerror_and_alarm("Trying to close invalid interface index. Interface index: %d", interfaceIndex+1);         
      return;
   }

   log_line("Closed radio interface %d (%s) that was used for write. Selectable write fd: %d", interfaceIndex+1, pRadioHWInfo->szName, pRadioHWInfo->monitor_interface_write.selectable_fd);

   #ifdef USE_PCAP_FOR_TX

   if ( NULL != pRadioHWInfo->monitor_interface_write.ppcap )
      pcap_close(pRadioHWInfo->monitor_interface_write.ppcap);
   else
      log_line("Radio interface %d was not opened for read.", interfaceIndex+1);

   #else

   if ( -1 != pRadioHWInfo->monitor_interface_write.selectable_fd )
      close(pRadioHWInfo->monitor_interface_write.selectable_fd);
   else
      log_line("Radio interface %d was not opened for write.", interfaceIndex+1);
   
   #endif

   pRadioHWInfo->monitor_interface_write.ppcap = NULL;
   pRadioHWInfo->monitor_interface_write.selectable_fd = -1;
   pRadioHWInfo->monitor_interface_write.iErrorCount = 0;
   pRadioHWInfo->openedForWrite = 0;
}

pcap_t* radio_open_auxiliary_wfbohd_channel(int iInterfaceIndex, int iChannelId)
{
   char szFilter[256];
   char szFilterPrism[256];

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return NULL;

   int addr_encoded = iChannelId;
   sprintf(szFilter, "ether[0x00:2] == 0x0801 && ether[0x0a:2] == 0x5742 && ether[0x0c:4] == 0x%08x", addr_encoded);
   sprintf(szFilterPrism, "radio[0x40:2] == 0x0801 && radio[0x4a:2] == 0x5742 && radio[0x4c:4] == 0x%08x", addr_encoded);

   struct bpf_program bpfprogram;
   char szProgram[512];
   char szErrbuf[PCAP_ERRBUF_SIZE];
   pcap_t* pPCAP = NULL;

   szErrbuf[0] = '\0';
   //pRadioHWInfo->monitor_interface_read.ppcap = pcap_open_live(pRadioHWInfo->szName, 4096, 1, 1, szErrbuf);
   pPCAP = pcap_create(pRadioHWInfo->szName, szErrbuf);

   if (pPCAP == NULL)
   {
      log_softerror_and_alarm("Unable to open auxiliary channel on [%s]: %s", pRadioHWInfo->szName, szErrbuf);
      return NULL;
   }
   if ( pcap_set_snaplen(pPCAP, 4096) !=0 )
      log_softerror_and_alarm("Error setting [%s] snap buffer length: %s", pRadioHWInfo->szName, pcap_geterr(pPCAP));

   if ( pcap_set_promisc(pPCAP, 1) != 0 )
      log_softerror_and_alarm("Error setting auxiliary [%s] to promiscous mode: %s", pRadioHWInfo->szName, pcap_geterr(pPCAP));

   if ( pcap_set_timeout(pPCAP, -1) !=0) 
      log_softerror_and_alarm("Error setting auxiliary [%s] timeout: %s", pRadioHWInfo->szName, pcap_geterr(pPCAP));
   
   if ( pcap_set_immediate_mode(pPCAP, 1) != 0 )
      log_softerror_and_alarm("Error setting auxiliary [%s] to immediate mode: %s", pRadioHWInfo->szName, pcap_geterr(pPCAP));
   
   if ( pcap_activate(pPCAP) !=0) 
      log_softerror_and_alarm("Error setting auxiliary [%s] to immediate mode: %s", pRadioHWInfo->szName, pcap_geterr(pPCAP));
    
   if ( pcap_setnonblock(pPCAP, 1, szErrbuf) < 0 )
      log_softerror_and_alarm("Error setting auxiliary [%s] to nonblocking mode: %s", pRadioHWInfo->szName, szErrbuf);
        
   if ( pcap_setdirection(pPCAP, PCAP_D_IN) < 0 )
      log_softerror_and_alarm("Error setting auxiliary [%s] direction", pRadioHWInfo->szName);

   int nLinkEncap = pcap_datalink(pPCAP);

   if (nLinkEncap == DLT_IEEE802_11_RADIO)
      sprintf(szProgram, "%s", szFilter);
   else if (nLinkEncap == DLT_PRISM_HEADER)
      sprintf(szProgram, "%s", szFilterPrism);
   else
   {
      log_softerror_and_alarm("ERROR: unknown encapsulation on auxiliary [%s]! check if monitor mode is supported and enabled", pRadioHWInfo->szName);
      return NULL;
   }

   if (pcap_compile(pPCAP, &bpfprogram, szProgram, 1, 0) == -1)
   {
      puts(szProgram);
      puts(pcap_geterr(pPCAP));
      log_softerror_and_alarm("ERROR: compiling auxiliary program for interface [%s]", pRadioHWInfo->szName);
      return NULL;
   }
   else
   {
      if (pcap_setfilter(pPCAP, &bpfprogram) == -1)
      {
         log_softerror_and_alarm("Failed to set auxiliary pcap filter: %s", szProgram);
         log_softerror_and_alarm("Failed to set auxiliary pcap filter: %s", pcap_geterr(pPCAP));
      }
      pcap_freecode(&bpfprogram);
   }
   int iFD = pcap_get_selectable_fd(pPCAP);
  
   log_line("Opened auxiliary radio interface %d for reading on %s, filter: [%s]. Returned fd=%d, ppcap: %d", iInterfaceIndex+1, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), szFilter, iFD, pPCAP);

   return pPCAP;
}

void radio_close_auxiliary_wfbohd_channel(pcap_t* pPCAP)
{
   if ( NULL == pPCAP )
      return;

   pcap_close(pPCAP);
}

u8* radio_process_wlan_data_in_pcap(int iInterfaceNumber, pcap_t* pPCAP, int* outPacketLength)
{
   s_iRadioLastReadErrorCode = RADIO_READ_ERROR_NO_ERROR;
   if ( NULL != outPacketLength )
      *outPacketLength = 0;

   if ( NULL == pPCAP )
      return NULL;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterfaceNumber);
   if ( NULL == pRadioHWInfo )
      return NULL;

#ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
#endif

   struct pcap_pkthdr * ppcapPacketHeader = NULL;
   struct ieee80211_radiotap_iterator rti;
   u8 *pRadioPayload = sPayloadBufferRead;
   int payloadLength = 0;
   int n = 0;

   int retval = pcap_next_ex(pPCAP, &ppcapPacketHeader, (const u_char**)&pRadioPayload);
   if (retval < 0)
   {
      s_iRadioLastReadErrorCode = RADIO_READ_ERROR_INTERFACE_BROKEN;
      if (strcmp("The interface went down",pcap_geterr(pPCAP)) == 0)
      {
         #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
         if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
            pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
         #endif
         return NULL;
      }
      else
      {
         #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
         if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
            pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
         #endif
         return NULL;
      }
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return NULL;
   }

   if (retval != 1)
   {
      if ( 0 == retval )
      {
         s_iRadioLastReadErrorCode = RADIO_READ_ERROR_TIMEDOUT;
         log_softerror_and_alarm("Rx ppcap timedout reading a packet.");
         #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
         if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
            pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
         #endif
         return NULL;
      }
      s_iRadioLastReadErrorCode = RADIO_READ_ERROR_READ_ERROR;
      log_softerror_and_alarm("rx pcap ERROR getting received data on ppcap: %x; retval != 1; retval is: %d", pPCAP, retval);
      log_softerror_and_alarm("pcap error: %s", pcap_geterr(pPCAP)); 
      log_line("rx pcap received: %d - %d", ppcapPacketHeader->caplen, ppcapPacketHeader->len);
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return NULL;
   }   
   
   if (ieee80211_radiotap_iterator_init(&rti,(struct ieee80211_radiotap_header *)pRadioPayload, ppcapPacketHeader->len) < 0)
   {
      log_softerror_and_alarm("rx pcap ERROR: radiotap_iterator_init < 0");
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return NULL;
   }

   while ((n = ieee80211_radiotap_iterator_next(&rti)) == 0)
   {

   }
   

   #ifdef DEBUG_PACKET_RECEIVED
   log_line("ieee iterator length: %d, iee header: %d", rti.max_length,sizeof(s_uIEEEHeaderData) );
   if ( ppcapPacketHeader->caplen <= 96 )
   {
      log_line("Received buffer over the air (%d bytes):", ppcapPacketHeader->caplen );
      log_buffer2(pRadioPayload, ppcapPacketHeader->caplen, rti.max_length, sizeof(s_uIEEEHeaderData));
   }
   #endif

   u32 uRadioHeadersLength = rti.max_length + sizeof(s_uIEEEHeaderData);
   pRadioPayload += uRadioHeadersLength;
   payloadLength = ppcapPacketHeader->len - uRadioHeadersLength;
   // Ralink and Atheros both always supply the FCS to userspace at the end, so remove it from size
   if (pRadioHWInfo->monitor_interface_read.radioInfo.nRadiotapFlags & IEEE80211_RADIOTAP_F_FCS)
      payloadLength -= 4;
   //log_line("pRadioPayload size adjusted: %d: ", payloadLength);

   if ( NULL != outPacketLength )
      *outPacketLength = payloadLength;

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   #ifdef DEBUG_PACKET_RECEIVED
   if ( payloadLength <= 96 )
   {
      log_line("Processed received packet over the air (%d bytes):", payloadLength );
      log_buffer4(pRadioPayload, payloadLength, sizeof(s_uIEEEHeaderData)-5, 10, 6, 8);
   }
   #endif

   return pRadioPayload;
}

u8* radio_process_wlan_data_in(int interfaceNumber, int* outPacketLength)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceNumber);

   s_iRadioLastReadErrorCode = RADIO_READ_ERROR_NO_ERROR;

   if ( NULL != outPacketLength )
      *outPacketLength = 0;


#ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
#endif

   struct pcap_pkthdr * ppcapPacketHeader = NULL;
   struct ieee80211_radiotap_iterator rti;
   u8 *pRadioPayload = sPayloadBufferRead;
   int payloadLength = 0;
   int n = 0;

   int retval = pcap_next_ex(pRadioHWInfo->monitor_interface_read.ppcap, &ppcapPacketHeader, (const u_char**)&pRadioPayload);
   if (retval < 0)
   {
      s_iRadioLastReadErrorCode = RADIO_READ_ERROR_INTERFACE_BROKEN;
      if (strcmp("The interface went down",pcap_geterr(pRadioHWInfo->monitor_interface_read.ppcap)) == 0)
      {
         pRadioHWInfo->monitor_interface_read.iErrorCount++;

         log_softerror_and_alarm("rx pcap ERROR: Radio interface %d (%s) went down. Error count: %d, ppcap = %x, fd = %d", 
            interfaceNumber+1,
            pRadioHWInfo->szName,
            pRadioHWInfo->monitor_interface_read.iErrorCount,
            pRadioHWInfo->monitor_interface_read.ppcap,
            pRadioHWInfo->monitor_interface_read.selectable_fd );

         // Recovery of a broken Rx interface should be done by Rx thread
         /*
         if ( pRadioHWInfo->monitor_interface_read.iErrorCount > 10 )
         {
            log_softerror_and_alarm("rx pcap ERROR: Try to recover interface...");
            int iPort = pRadioHWInfo->monitor_interface_read.nPort;
            radio_close_interface_for_read(interfaceNumber);
            hardware_sleep_ms(DEFAULT_DELAY_WIFI_CHANGE);
            
            char szOutput[4096];
            szOutput[0] = 0;
            hw_execute_bash_command_raw("sudo ifconfig -a | grep wlan", szOutput);
            
            log_line("Reinitializing radio interface %d (%s): found interfaces on ifconfig: [%s]",
               interfaceNumber+1, 
               pRadioHWInfo->szName, szOutput);
            
            if ( radio_open_interface_for_read(interfaceNumber, iPort) > 0 )
            {
               log_line("Recovered the rx interface (closed and repoened).");
               s_iRadioLastReadErrorCode = RADIO_READ_ERROR_TIMEDOUT;
               return NULL;
            }
            else
               log_softerror_and_alarm("rx pcap: Could not recover the interface.");
            s_iRadioInterfacesBroken++;
         }
         */
         #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
         if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
            pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
         #endif
         return NULL;
      }
      else
      {
         pRadioHWInfo->monitor_interface_read.iErrorCount++;
         log_softerror_and_alarm("rx pcap ERROR: %s, error count: %d", pcap_geterr(pRadioHWInfo->monitor_interface_read.ppcap), pRadioHWInfo->monitor_interface_read.iErrorCount);
         s_iRadioInterfacesBroken++;
         #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
         if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
            pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
         #endif
         return NULL;
      }
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return NULL;
   }

   if (retval != 1)
   {
      if ( 0 == retval )
      {
         s_iRadioLastReadErrorCode = RADIO_READ_ERROR_TIMEDOUT;
         log_softerror_and_alarm("Rx ppcap timedout reading a packet.");
         #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
         if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
            pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
         #endif
         return NULL;
      }
      s_iRadioLastReadErrorCode = RADIO_READ_ERROR_READ_ERROR;
      log_softerror_and_alarm("rx pcap ERROR getting received data on ppcap: %x; retval != 1; retval is: %d", pRadioHWInfo->monitor_interface_read.ppcap, retval);
      log_softerror_and_alarm("pcap error: %s", pcap_geterr(pRadioHWInfo->monitor_interface_read.ppcap)); 
      log_line("rx pcap received: %d - %d", ppcapPacketHeader->caplen, ppcapPacketHeader->len);
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return NULL;
   }
   
   #ifdef DEBUG_PACKET_RECEIVED
   log_line("RX Buffer: caplen: %d bytes, len: %d", ppcapPacketHeader->caplen, ppcapPacketHeader->len);
   #endif

   if (ieee80211_radiotap_iterator_init(&rti,(struct ieee80211_radiotap_header *)pRadioPayload, ppcapPacketHeader->len) < 0)
   {
      log_softerror_and_alarm("rx pcap ERROR: radiotap_iterator_init < 0");
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return NULL;
   }

   pRadioHWInfo->monitor_interface_read.iErrorCount = 0;
   pRadioHWInfo->monitor_interface_read.radioInfo.nRadiotapFlags = 0;
   int idBm = 0;
   while ((n = ieee80211_radiotap_iterator_next(&rti)) == 0)
   {
      switch (rti.this_arg_index)
      {
         case IEEE80211_RADIOTAP_RATE:
	           pRadioHWInfo->monitor_interface_read.radioInfo.nDataRateBPSMCS = getRealDataRateFromRadioDataRate((int)(*((u8*)(rti.this_arg)))/2);
            //log_line("recv datarate: %d", (int)(*((u8*)(rti.this_arg))));
            break;

         case IEEE80211_RADIOTAP_MCS:
            {
            //u8 known = rti.this_arg[0];
            //u8 flags = rti.this_arg[1];
            u8 mcs = rti.this_arg[2];
            //log_line("recv MCS, %d, %d, %d", (int)known, (int)flags, (int)mcs);
            pRadioHWInfo->monitor_interface_read.radioInfo.nDataRateBPSMCS = -mcs-1;
            break;
            }

         case IEEE80211_RADIOTAP_CHANNEL:
            pRadioHWInfo->monitor_interface_read.radioInfo.nChannel = le16_to_cpu(*((u16 *)rti.this_arg));
            pRadioHWInfo->monitor_interface_read.radioInfo.nChannelFlags = le16_to_cpu(*((u16 *)(rti.this_arg + 2)));
            //log_line("Channel: %d, flags: %d", pRadioHWInfo->monitor_interface_read.radioInfo.nChannel, pRadioHWInfo->monitor_interface_read.radioInfo.nChannelFlags);
            break;
	
         case IEEE80211_RADIOTAP_ANTENNA:
            pRadioHWInfo->monitor_interface_read.radioInfo.nAntenna = (*rti.this_arg) + 1;
            break;
		
         case IEEE80211_RADIOTAP_FLAGS:
             pRadioHWInfo->monitor_interface_read.radioInfo.nRadiotapFlags = (int)(*(uint8_t*)(rti.this_arg));
             //log_line("radio tap flags: %d", pRadioHWInfo->monitor_interface_read.radioInfo.nRadiotapFlags);
             break;

         case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
             {
             idBm = (int8_t)(*rti.this_arg);
             if ( idBm < -2 )
             pRadioHWInfo->monitor_interface_read.radioInfo.nDbm = idBm;
             //log_line("dbm: %d", pRadioHWInfo->monitor_interface_read.radioInfo.nDbm);
             break;
             }
         case IEEE80211_RADIOTAP_DBM_ANTNOISE:
             pRadioHWInfo->monitor_interface_read.radioInfo.nDbmNoise = (int8_t)(*rti.this_arg);
             //log_line("dbmnoise: %d", pRadioHWInfo->monitor_interface_read.radioInfo.nDbmNoise);
             break;
      }
   }

   #ifdef DEBUG_PACKET_RECEIVED
   log_line("ieee iterator length: %d, iee header: %d", rti.max_length,sizeof(s_uIEEEHeaderData) );
   if ( ppcapPacketHeader->caplen <= 96 )
   {
      log_line("Received buffer over the air (%d bytes):", ppcapPacketHeader->caplen );
      log_buffer2(pRadioPayload, ppcapPacketHeader->caplen, rti.max_length, sizeof(s_uIEEEHeaderData));
   }
   #endif

   sRadioLastReceivedHeadersLength = rti.max_length + sizeof(s_uIEEEHeaderData);
   pRadioPayload += sRadioLastReceivedHeadersLength;
   payloadLength = ppcapPacketHeader->len - sRadioLastReceivedHeadersLength;
   // Ralink and Atheros both always supply the FCS to userspace at the end, so remove it from size
   if (pRadioHWInfo->monitor_interface_read.radioInfo.nRadiotapFlags & IEEE80211_RADIOTAP_F_FCS)
      payloadLength -= 4;
   //log_line("pRadioPayload size adjusted: %d: ", payloadLength);

   if ( NULL != outPacketLength )
      *outPacketLength = payloadLength;

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   #ifdef DEBUG_PACKET_RECEIVED
   if ( payloadLength <= 96 )
   {
      log_line("Processed received packet over the air (%d bytes):", payloadLength );
      log_buffer4(pRadioPayload, payloadLength, sizeof(s_uIEEEHeaderData)-5, 10, 6, 8);
   }
   #endif

   return pRadioPayload;
}

// returns 0 for failure, total length of packet for success

int packet_process_and_check(int interfaceNb, u8* pPacketBuffer, int iBufferLength, int* pbCRCOk)
{
   //log_line("Check radio packet: %d", length);

   s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_NO_ERROR;

   if ( NULL != pbCRCOk )
      *pbCRCOk = 0;

   if ( NULL == pPacketBuffer || 0 == iBufferLength )
   {
      s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_INVALID_PARAMETERS;
      return 0;
   }

   if ( iBufferLength < (int)sizeof(t_packet_header) )
   {
      s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_CODE_PACKET_RECEIVED_TOO_SMALL;

      #ifdef DEBUG_PACKET_RECEIVED
      printf("Received packet to small: packet length: %d bytes, header minimum: %d\n", iBufferLength, (int)sizeof(t_packet_header));
      log_line("Received packet to small: packet length: %d bytes, header minimum: %d", iBufferLength, (int)sizeof(t_packet_header));
      #endif
      return 0;
   }

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   int packetLength = pPH->total_length;

   if ( packetLength > iBufferLength )
   {
      s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_CODE_PACKET_RECEIVED_TOO_SMALL;

      #ifdef DEBUG_PACKET_RECEIVED
      printf("Received packet to small: packet length: %d bytes, buffer length is: %d\n", packetLength, iBufferLength);
      log_line("Received packet to small: packet length: %d bytes, buffer length is: %d", packetLength, iBufferLength);
      #endif
      return 0;
   }

   if ( pPH->packet_flags & PACKET_FLAGS_BIT_HAS_ENCRYPTION )
   {
      #ifdef DEBUG_PACKET_RECEIVED
      log_line("enc detected");
      #endif
      int dx = sizeof(t_packet_header) - sizeof(u32) - sizeof(u32);
      int l = packetLength-dx;
      dpp(pPacketBuffer + dx, l);
   }

   u32 uCRC = 0;
   if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
      uCRC = base_compute_crc32(pPacketBuffer+sizeof(u32), sizeof(t_packet_header)-sizeof(u32));
   else
      uCRC = base_compute_crc32(pPacketBuffer+sizeof(u32), pPH->total_length-sizeof(u32));

   if ( (uCRC & 0x00FFFFFF) != (pPH->uCRC & 0x00FFFFFF) )
   {
      s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_CODE_INVALID_CRC_RECEIVED;
      #ifdef DEBUG_PACKET_RECEIVED
      log_line("Received packet bad headers, CRC headers: %u, computed CRC(%s): %u, packet length: %d bytes", pPH->uCRC, (pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC)?"headers only":"full", uCRC, packetLength);
      if ( packetLength <= 48 )
         log_buffer(pPacketBuffer, packetLength);
      #endif
      if ( NULL != pbCRCOk )
         *pbCRCOk = 0;
      return 0;
   }

   if ( NULL != pbCRCOk )
      *pbCRCOk = 1;

   //#ifdef DEBUG_PACKET_RECEIVED
   //printf("Received packet correctly. Packet length: %d bytes\n", packetLength);
   //log_line("Received packet correctly. Packet length: %d bytes", packetLength);
   //#endif

   s_iLastProcessingErrorCode = RADIO_PROCESSING_ERROR_NO_ERROR;
   return packetLength;     
}

int get_last_processing_error_code()
{
   return s_iLastProcessingErrorCode;
}

u32 radio_get_next_radio_link_packet_index(int iLocalRadioLinkId)
{
   if ( (iLocalRadioLinkId < 0) || (iLocalRadioLinkId >= MAX_RADIO_INTERFACES) )
      iLocalRadioLinkId = 0;
   u32 uRadioLinkPacketIndex = s_uNextRadioPacketIndexes[iLocalRadioLinkId];
   s_uNextRadioPacketIndexes[iLocalRadioLinkId]++;

   return uRadioLinkPacketIndex;
}

int radio_build_new_raw_packet(int iLocalRadioLinkId, u8* pRawPacket, u8* pPacketData, int nInputLength, int portNb, int bEncrypt, int iExtraData, u8* pExtraData)
{
   int totalRadioLength = 0;

   s_uIEEEHeaderData_short[4] = _radio_encode_port(portNb);
   s_uIEEEHeaderData[4] = _radio_encode_port(portNb);
   s_uIEEEHeaderRTS[4] = _radio_encode_port(portNb);
   s_uIEEEHeaderData[22] = uIEEEE80211SeqNb & 0xff;
   s_uIEEEHeaderData[23] = (uIEEEE80211SeqNb >> 8) & 0xff;
   uIEEEE80211SeqNb += 16;
   
   if ( (sRadioFrameFlags & RADIO_FLAGS_MCS_MASK) || (sRadioDataRate_bps < 0) )
   {
      memcpy(pRawPacket, s_uRadiotapHeaderMCS, sizeof(s_uRadiotapHeaderMCS));
      pRawPacket += sizeof(s_uRadiotapHeaderMCS);
      totalRadioLength += sizeof(s_uRadiotapHeaderMCS);
      s_uLastPacketSentRadioTapHeaderLength = sizeof(s_uRadiotapHeaderMCS);
   }
   else
   {
      memcpy(pRawPacket, s_uRadiotapHeaderLegacy, sizeof(s_uRadiotapHeaderLegacy));
      pRawPacket += sizeof(s_uRadiotapHeaderLegacy);
      totalRadioLength += sizeof(s_uRadiotapHeaderLegacy);
      s_uLastPacketSentRadioTapHeaderLength = sizeof(s_uRadiotapHeaderLegacy);
   }

   if ( sRadioFrameFlags & RADIO_FLAGS_FRAME_TYPE_DATA )
   {
      memcpy(pRawPacket, s_uIEEEHeaderData, sizeof (s_uIEEEHeaderData));
      pRawPacket += sizeof(s_uIEEEHeaderData);
      totalRadioLength += sizeof(s_uIEEEHeaderData);
      s_uLastPacketSentIEEEHeaderLength = sizeof(s_uIEEEHeaderData);
   }
   else if ( sRadioFrameFlags & RADIO_FLAGS_FRAME_TYPE_RTS )
   {   
      memcpy(pRawPacket, s_uIEEEHeaderRTS, sizeof (s_uIEEEHeaderRTS));
      pRawPacket += sizeof(s_uIEEEHeaderRTS);
      totalRadioLength += sizeof(s_uIEEEHeaderRTS);
      s_uLastPacketSentIEEEHeaderLength = sizeof(s_uIEEEHeaderRTS);
   }	
   else if ( sRadioFrameFlags & RADIO_FLAGS_FRAME_TYPE_DATA_SHORT )
   {
      memcpy(pRawPacket, s_uIEEEHeaderData_short, sizeof (s_uIEEEHeaderData_short));
      pRawPacket += sizeof(s_uIEEEHeaderData_short);
      totalRadioLength += sizeof(s_uIEEEHeaderData_short);
      s_uLastPacketSentIEEEHeaderLength = sizeof(s_uIEEEHeaderData_short);
   }
   else
   {
      memcpy(pRawPacket, s_uIEEEHeaderData, sizeof (s_uIEEEHeaderData));
      pRawPacket += sizeof(s_uIEEEHeaderData);
      totalRadioLength += sizeof(s_uIEEEHeaderData);
      s_uLastPacketSentIEEEHeaderLength = sizeof(s_uIEEEHeaderData);
   }
   
   memcpy(pRawPacket, pPacketData, nInputLength);
   totalRadioLength += nInputLength;

   if ( 0 < iExtraData && NULL != pExtraData )
   {
      memcpy(pRawPacket+nInputLength, pExtraData, iExtraData);
      totalRadioLength += iExtraData;
   }

   #ifdef DEBUG_PACKET_SENT
   log_line("Building a composed packet of total size: %d, extra data: %d", nInputLength + iExtraData, iExtraData);
   #endif

   if ( (iLocalRadioLinkId < 0) || (iLocalRadioLinkId >= MAX_RADIO_INTERFACES) )
      iLocalRadioLinkId = 0;
   u16 uRadioLinkPacketIndex = radio_get_next_radio_link_packet_index(iLocalRadioLinkId);

   // Compute CRC/encrypt all packets in this buffer

   int nLength = nInputLength;
   u8* pData = pRawPacket;

   int nPCount = 0;
   while ( nLength > 0 )
   {
      nPCount++;
      t_packet_header* pPH = (t_packet_header*)pData;
      int nPacketLength = pPH->total_length;

      // Last packet in the chain? Add the extra data if present
      if ( nLength == nPacketLength )
      {
         if ( 0 < iExtraData && NULL != pExtraData )
         {
            #ifdef DEBUG_PACKET_SENT
            log_line("Adding extra data at the end: %d len", iExtraData);
            #endif
            pPH->total_length += iExtraData;
            pPH->packet_flags |= PACKET_FLAGS_BIT_EXTRA_DATA;
         }
      }
      pPH->radio_link_packet_index = uRadioLinkPacketIndex;
      if ( bEncrypt )
         pPH->packet_flags |= PACKET_FLAGS_BIT_HAS_ENCRYPTION;


      if ( pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC )
         radio_packet_compute_crc((u8*)pPH, sizeof(t_packet_header));
      else
         radio_packet_compute_crc((u8*)pPH, pPH->total_length);

      #ifdef DEBUG_PACKET_SENT
      log_line("Packet %d in composed packet: enc: %d, crc (%s): %u, len: %d", nPCount, bEncrypt, (pPH->packet_flags & PACKET_FLAGS_BIT_HEADERS_ONLY_CRC)?"headers only":"full", pPH->uCRC, pPH->total_length);
      if ( pPH->total_length <= 125 )
         log_buffer3(pData, pPH->total_length, 10,6,8);
      #endif

      if ( bEncrypt )
      {
         int dx = sizeof(t_packet_header) - sizeof(u32) - sizeof(u32);
         epp(pData+dx, pPH->total_length-dx);
      }
      nLength -= nPacketLength;
      pData += nPacketLength;
   }

   return totalRadioLength;
}


int radio_write_raw_packet(int interfaceIndex, u8* pData, int dataLength)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo || ( 0 == pRadioHWInfo->openedForWrite) || (pRadioHWInfo->monitor_interface_write.selectable_fd < 0 ) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a radio message to an invalid interface (%d).", interfaceIndex+1);
      return 0;
   }

   if ( (NULL == pData) || (dataLength <= 0) )
   {
      log_softerror_and_alarm("RadioError: Tried to send an empty radio message.");
      return 0;
   }

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   s_uPacketsSentUsingCurrent_RadioRate++;
   s_uPacketsSentUsingCurrent_RadioFlags++;

   int len = 0;

   #ifdef USE_PCAP_FOR_TX

   len = pcap_inject(pRadioHWInfo->monitor_interface_write.ppcap, pData, dataLength);
   if ( len < dataLength )
   {
      log_softerror_and_alarm("RadioError: tx ppcap failed to send radio message (%d bytes sent of %d bytes).", len, dataLength);
      pRadioHWInfo->monitor_interface_write.iErrorCount++;
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return 0;
   }
   else
      pRadioHWInfo->monitor_interface_write.iErrorCount = 0;

   #else

   len = write(pRadioHWInfo->monitor_interface_write.selectable_fd, pData, dataLength);
   if ( len < dataLength )
   {
      log_softerror_and_alarm("RadioError: Failed to send radio message on radio interface %d, fd=%d (%d bytes sent of %d bytes).",
        interfaceIndex+1, pRadioHWInfo->monitor_interface_write.selectable_fd, len, dataLength);
      pRadioHWInfo->monitor_interface_write.iErrorCount++;
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return 0;
   }
   else
      pRadioHWInfo->monitor_interface_write.iErrorCount = 0;

   #endif

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   #ifdef DEBUG_PACKET_SENT
   if ( dataLength <= 96 )
   {
      log_line("Sent buffer over the radio (%d bytes [%d headers, %d data]):", dataLength, s_uLastPacketSentRadioTapHeaderLength + s_uLastPacketSentIEEEHeaderLength, dataLength - s_uLastPacketSentRadioTapHeaderLength - s_uLastPacketSentIEEEHeaderLength);
      log_buffer5(pData, dataLength, s_uLastPacketSentRadioTapHeaderLength, s_uLastPacketSentIEEEHeaderLength, 10,6,8 ); // 24 is size of Ruby packet header
   }
   #endif

   return 1;
}


// Returns the number of bytes written or -1 for error, -2 for write error

int radio_write_serial_packet(int interfaceIndex, u8* pData, int dataLength, u32 uTimeNow)
{
   if ( (interfaceIndex < 0) || (interfaceIndex >= MAX_RADIO_INTERFACES) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a serial radio message to an invalid interface index %d.", interfaceIndex+1);
      return -1;    
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo || ( 0 == pRadioHWInfo->openedForWrite) || (pRadioHWInfo->monitor_interface_write.selectable_fd < 0 ) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a serial radio message to an invalid interface (%d).", interfaceIndex+1);
      return -1;
   }
   if ( ! hardware_radio_is_serial_radio(pRadioHWInfo) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a serial radio message to an interface that is not a serial radio (%d).", interfaceIndex+1);
      return -1;    
   }
   if ( (NULL == pData) || (dataLength <= 0) )
   {
      log_softerror_and_alarm("RadioError: Tried to send an empty radio message.");
      return -1;
   }

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   if ( ! hardware_radio_serial_write_packet(interfaceIndex, pData, dataLength) )
   {
      log_softerror_and_alarm("RadioError: Failed to write a serial radio message (%d bytes) to serial radio interface %d.", dataLength, interfaceIndex+1);
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return (-2);
   }

   s_uIndexLastLowDataLinkPackets[interfaceIndex]++;
   if ( s_uIndexLastLowDataLinkPackets[interfaceIndex] >= MAX_LOW_DATALINK_PACKETS_HISTORY )
      s_uIndexLastLowDataLinkPackets[interfaceIndex] = 0;

   s_uTimesLastLowDataLinkPacketsOnInterfaces[interfaceIndex][s_uIndexLastLowDataLinkPackets[interfaceIndex]] = uTimeNow;
   s_uSizesLastLowDataLinkPacketsOnInterfaces[interfaceIndex][s_uIndexLastLowDataLinkPackets[interfaceIndex]] = dataLength;

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   return dataLength;
}

// Returns the number of bytes written or -1 for error, -2 for write error

int radio_write_sik_packet(int interfaceIndex, u8* pData, int dataLength, u32 uTimeNow)
{
   if ( (interfaceIndex < 0) || (interfaceIndex >= MAX_RADIO_INTERFACES) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a SiK radio message to an invalid interface index %d.", interfaceIndex+1);
      return -1;    
   }

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(interfaceIndex);
   if ( NULL == pRadioHWInfo || ( 0 == pRadioHWInfo->openedForWrite) || (pRadioHWInfo->monitor_interface_write.selectable_fd < 0 ) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a SiK radio message to an invalid interface (%d).", interfaceIndex+1);
      return -1;
   }
   if ( ! hardware_radio_is_sik_radio(pRadioHWInfo) )
   {
      log_softerror_and_alarm("RadioError: Tried to write a SiK radio message to an interface that is not SiK radio (%d).", interfaceIndex+1);
      return -1;    
   }
   if ( (NULL == pData) || (dataLength <= 0) )
   {
      log_softerror_and_alarm("RadioError: Tried to send an empty radio message.");
      return -1;
   }

   // Do not send packets that contain "+++" or "OK\n"
   int iOk = 1;
   u8* pTmp = pData;
   for( int i=0; i<dataLength-2; i++ )
   {
      if ( (*pTmp) == '+' )
      if ( (*(pTmp+1)) == '+' )
      if ( (*(pTmp+2)) == '+' )
      {
         iOk = 0;
         break;
      }
      if ( ((*pTmp) == 'O') || ((*pTmp) == 'o') )
      if ( ((*(pTmp+1)) == 'K') || ((*(pTmp+1)) == 'k') )
      if ( ((*(pTmp+2)) == 10)  || ((*(pTmp+2)) == 13) )
      {
         iOk = 0;
         break;
      }
      pTmp++;
   }
   if ( ! iOk )
   {
      log_softerror_and_alarm("RadioError: Ignored sending a SiK radio message (%d bytes) as it contained control codes.", dataLength);
      return -1;
   }

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_lock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   if ( ! hardware_radio_sik_write_packet(interfaceIndex, pData, dataLength) )
   {
      log_softerror_and_alarm("RadioError: Failed to write a SiK radio message (%d bytes) to SiK radio interface %d.", dataLength, interfaceIndex+1);
      #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
      if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
         pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
      #endif
      return (-2);
   }

   s_uIndexLastLowDataLinkPackets[interfaceIndex]++;
   if ( s_uIndexLastLowDataLinkPackets[interfaceIndex] >= MAX_LOW_DATALINK_PACKETS_HISTORY )
      s_uIndexLastLowDataLinkPackets[interfaceIndex] = 0;

   s_uTimesLastLowDataLinkPacketsOnInterfaces[interfaceIndex][s_uIndexLastLowDataLinkPackets[interfaceIndex]] = uTimeNow;
   s_uSizesLastLowDataLinkPacketsOnInterfaces[interfaceIndex][s_uIndexLastLowDataLinkPackets[interfaceIndex]] = dataLength;

   #ifdef FEATURE_RADIO_SYNCHRONIZE_RXTX_THREADS
   if ( 1 == s_iMutexRadioSyncRxTxThreadsInitialized )
      pthread_mutex_unlock(&s_pMutexRadioSyncRxTxThreads);
   #endif

   return dataLength;
}
