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

#include "wfbohd.h"

#ifdef HW_CAPABILITY_WFBOHD

#include "../base/hw_procs.h"
#include "../base/enc.h"
#include "../base/parse_fc_telemetry.h"
#include "../base/ruby_ipc.h"
#include "../radio/radiopackets_wfbohd.h"
#include "shared_vars.h"
#include "timers.h"
#include "../../mavlink/common/mavlink.h"

bool s_bWFBOHD_SearchMode = true;
bool s_bOpenIPCVideoStarted = false;

uint8_t s_rx_wfbohd_secretkey[crypto_box_SECRETKEYBYTES];
uint8_t s_tx_wfbohd_publickey[crypto_box_PUBLICKEYBYTES];
uint8_t s_wfbohd_session_key[crypto_aead_chacha20poly1305_KEYBYTES];
fec_t* s_pFECWFBOHD = NULL; 

u32 s_uTimeLastFCTelemetrySentToCentral = 0;
t_packet_header_fc_telemetry PHFCTelem_WFBOHD;
t_packet_header_fc_extra PHFCTelemExtra_WFBOHD;
t_packet_header_ruby_telemetry_extended_v3 PHRTE_WFBOHD;

void wfbohd_init(bool bSearching)
{
   wfbohd_clear_wrong_key_flag();
   s_bWFBOHD_SearchMode = bSearching;
   if ( bSearching )
      wfbohd_radio_set_discard_video_data(1);
   else
      wfbohd_radio_set_discard_video_data(0);

   if ( g_uAcceptedFirmwareType == MODEL_FIRMWARE_TYPE_OPENIPC )
   {
      wfbohd_radio_init();
      if ( ! s_bWFBOHD_SearchMode )
      {
         if ( 1 != load_openipc_keys(FILE_DEFAULT_OPENIPC_KEYS) )
            log_softerror_and_alarm("Can't load openIpc keys from file [%s].", FILE_DEFAULT_OPENIPC_KEYS);
         memset(s_wfbohd_session_key, 0, sizeof(s_wfbohd_session_key)); 
         if ( NULL != get_openipc_key1() )
            memcpy( s_rx_wfbohd_secretkey, get_openipc_key1(), crypto_box_SECRETKEYBYTES);
         if ( NULL != get_openipc_key2() )
            memcpy( s_tx_wfbohd_publickey, get_openipc_key2(), crypto_box_SECRETKEYBYTES);
      }
   }
}

void wfbohd_cleanup()
{
   if ( ! s_bWFBOHD_SearchMode )
   {
      if ( s_bOpenIPCVideoStarted )
      {
         hw_stop_process("gst-launch-1.0");
         hw_stop_process(VIDEO_PLAYER_STDIN);
         s_bOpenIPCVideoStarted = false;
      }
      if ( g_uAcceptedFirmwareType == MODEL_FIRMWARE_TYPE_OPENIPC )
         wfbohd_radio_cleanup();
   }
}

bool wfbohd_is_video_player_started()
{
   return s_bOpenIPCVideoStarted;
}

void wfbohd_check_start_video_player()
{
   if ( s_bWFBOHD_SearchMode || (NULL == g_pCurrentModel) )
      return;

   if ( g_pCurrentModel->getVehicleFirmwareType() != MODEL_FIRMWARE_TYPE_OPENIPC )
      return;

   if ( s_bOpenIPCVideoStarted )
      return;
   s_bOpenIPCVideoStarted = true;
   hw_execute_bash_command("gst-launch-1.0 udpsrc port=5600 caps='application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264' ! rtph264depay ! 'video/x-h264,stream-format=byte-stream' ! fdsink | ./ruby_player_s &", NULL);
}

void _send_fc_wfbohd_telemetry_to_central()
{
   t_packet_header PH;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   //-------------------------------
   // Send FC telemetry

   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_FC_TELEMETRY, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   
   PHFCTelem_WFBOHD.fc_telemetry_type = g_pCurrentModel->telemetry_params.fc_telemetry_type;

   if ( get_time_last_mavlink_message_from_fc() < g_TimeNow-1100 )
      PHFCTelem_WFBOHD.flags |= FC_TELE_FLAGS_NO_FC_TELEMETRY;
   else
      PHFCTelem_WFBOHD.flags &= ~FC_TELE_FLAGS_NO_FC_TELEMETRY;

   PHFCTelem_WFBOHD.flags = PHFCTelem_WFBOHD.flags & (~FC_TELE_FLAGS_HAS_GPS_FIX);
   if ( PHFCTelem_WFBOHD.gps_fix_type >= GPS_FIX_TYPE_2D_FIX )
      PHFCTelem_WFBOHD.flags = PHFCTelem_WFBOHD.flags | FC_TELE_FLAGS_HAS_GPS_FIX;
   else if ( (g_pCurrentModel->iGPSCount > 1) && ( PHFCTelem_WFBOHD.extra_info[2] != 0xFF )
             && ( PHFCTelem_WFBOHD.extra_info[1] > 0 ) && (PHFCTelem_WFBOHD.extra_info[1] != 0xFF) && ( PHFCTelem_WFBOHD.extra_info[2] >= GPS_FIX_TYPE_2D_FIX ) )
      PHFCTelem_WFBOHD.flags = PHFCTelem_WFBOHD.flags | FC_TELE_FLAGS_HAS_GPS_FIX;        

   bool bHasAnyGPSGoodInfo = false;
   if ( PHFCTelem_WFBOHD.satelites > 2 && PHFCTelem_WFBOHD.hdop < 9000 )
      bHasAnyGPSGoodInfo = true;
   if ( (g_pCurrentModel->iGPSCount > 1) 
             && ( PHFCTelem_WFBOHD.extra_info[1] > 2 ) && (PHFCTelem_WFBOHD.extra_info[1] != 0xFF)
             && ( ((int)PHFCTelem_WFBOHD.extra_info[3]) * 255 + PHFCTelem_WFBOHD.extra_info[4] < 9000 ) )
      bHasAnyGPSGoodInfo = true;

   if ( (!has_received_gps_info()) || (! has_received_flight_mode()) || (! bHasAnyGPSGoodInfo) )
   {
      PHFCTelem_WFBOHD.flags = PHFCTelem_WFBOHD.flags & (~FC_TELE_FLAGS_POS_CURRENT);
      PHFCTelem_WFBOHD.flags = PHFCTelem_WFBOHD.flags & (~FC_TELE_FLAGS_POS_HOME);
   }

   PHFCTelem_WFBOHD.extra_info[5]++;
   PHFCTelem_WFBOHD.extra_info[6] = 4; // count msg/sec from FC

   // Do we have a new message from FC?

   bool bSendFCMessage = false;
   if ( (get_last_message_time() > 0) && (0 != get_last_message()[0]) )
   if ( get_last_message_time() > g_TimeNow - TIMEOUT_FC_MESSAGE )   
      bSendFCMessage = true;

   if ( bSendFCMessage )
   {
      PHFCTelem_WFBOHD.flags = PHFCTelem_WFBOHD.flags | FC_TELE_FLAGS_HAS_MESSAGE;
      PHFCTelemExtra_WFBOHD.chunk_index = 0;
      memset(PHFCTelemExtra_WFBOHD.text, 0, FC_MESSAGE_MAX_LENGTH);
      strcpy((char*)(PHFCTelemExtra_WFBOHD.text), get_last_message());
   }
   else
   {
      PHFCTelem_WFBOHD.flags = PHFCTelem_WFBOHD.flags & (~FC_TELE_FLAGS_HAS_MESSAGE);
   }

   PHFCTelem_WFBOHD.flags = PHFCTelem_WFBOHD.flags & (~FC_TELE_FLAGS_RC_FAILSAFE);

   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_FC_TELEMETRY, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.total_length = (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry);
   if ( bSendFCMessage )
   {
      PH.packet_type = PACKET_TYPE_FC_TELEMETRY_EXTENDED;
      PH.total_length = (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry) + (u16)sizeof(t_packet_header_fc_extra);
   }

   memcpy(buffer, &PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), &PHFCTelem_WFBOHD, sizeof(t_packet_header_fc_telemetry));
   if ( bSendFCMessage )
      memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_fc_telemetry), &PHFCTelemExtra_WFBOHD, sizeof(t_packet_header_fc_extra));

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
      log_softerror_and_alarm("No pipe to central to send wfbohd telemetry message to.");

   // Set lat, lon to last values as can have home pos in them if we just sent home pos to controller.
   //sPHFCT.latitude = s_lLastPosLat;
   //sPHFCT.longitude = s_lLastPosLon;

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

}


int wfbohd_process_auxiliary_radio_packet(u8* pBuffer, int iBufferLength)
{
   if ( (NULL == pBuffer) || (iBufferLength < (int)sizeof(type_wfb_block_header)) )
      return 0;
   
   if ( (*pBuffer) == PACKET_TYPE_WFB_KEY )
   {
      if ( iBufferLength != sizeof(type_wfb_session_header) + sizeof(type_wfb_session_data) + crypto_box_MACBYTES )
         return 0;
      type_wfb_session_data sessionData;
      if ( 0 != crypto_box_open_easy((uint8_t*)&sessionData,  pBuffer + sizeof(type_wfb_session_header),
                                sizeof(type_wfb_session_data) + crypto_box_MACBYTES,
                                ((type_wfb_session_header*)pBuffer)->sessionNonce,
                                s_tx_wfbohd_publickey, s_rx_wfbohd_secretkey) )
         return 0;

      if ( 0 != memcmp(s_wfbohd_session_key, sessionData.sessionKey, sizeof(s_wfbohd_session_key)) )
      {
         memcpy(s_wfbohd_session_key, sessionData.sessionKey, sizeof(s_wfbohd_session_key));

         if ( NULL != s_pFECWFBOHD )
            zfec_free(s_pFECWFBOHD);

         s_pFECWFBOHD = zfec_new(sessionData.uK, sessionData.uN); 
      }
      log_line("Created new decode context for OpenIPC telemetry stream, %d/%d.", sessionData.uK, sessionData.uN);
      return 0; 
   }

   if ( NULL == s_pFECWFBOHD )
      return 0;
   if ( s_pFECWFBOHD->k > s_pFECWFBOHD->n )
      return 0;

   if ( (*pBuffer) != PACKET_TYPE_WFB_DATA )
      return 0;

   if ( iBufferLength < (int)(sizeof(type_wfb_block_header) + sizeof(type_wfb_packet_header)) )
      return 0;

   uint8_t decrypted[2000];
   long long unsigned int decrypted_len = 0;
   type_wfb_block_header *pBlockHeader = (type_wfb_block_header*)pBuffer;

   if ( 0 != crypto_aead_chacha20poly1305_decrypt(decrypted, &decrypted_len, NULL,
                                          pBuffer + sizeof(type_wfb_block_header), iBufferLength - sizeof(type_wfb_block_header),
                                          pBuffer,
                                          sizeof(type_wfb_block_header),
                                          (uint8_t*)(&(pBlockHeader->uDataNonce)), s_wfbohd_session_key) )
      return 0;

   if ( decrypted_len < sizeof(type_wfb_block_header) + sizeof(type_wfb_packet_header) )
      return 0;

   
   //u32 u = be64toh(pBlockHeader->uDataNonce);
   //u32 uBlockIndex = u >> 8;
   //u32 uFragmentIndex = u & 0xFF;

   //static u32 s_uLastBlockAuxReceived = 0;
   //static u32 s_uLastFragmentAuxReceived = 0;

   //s_uLastBlockAuxReceived = uBlockIndex;
   //s_uLastFragmentAuxReceived = uFragmentIndex;

   //pPacketHeader = (type_wfb_packet_header*)(s_pCurrentBlockFragments[i]);
   //uVideoSize = (pPacketHeader->uPacketSize & 0xFF) << 8;


   type_wfb_packet_header* pPacketHeader = (type_wfb_packet_header*)(pBuffer);
   //u32 u = be64toh(pBlockHeader->uDataNonce);
   //u32 uBlockIndex = u >> 8;
   //u32 uFragmentIndex = u & 0xFF;
   u32 uDataSize = be16toh(pPacketHeader->uPacketSize);
   uDataSize += (pPacketHeader->uPacketSize >> 8) & 0xFF;
   
   //if ( uFragmentIndex == 0 )
   {
   parse_telemetry_allow_any_sysid(1);
   parse_telemetry_from_fc( &decrypted[sizeof(type_wfb_packet_header)], decrypted_len - sizeof(type_wfb_packet_header), &PHFCTelem_WFBOHD, &PHRTE_WFBOHD, g_pCurrentModel->vehicle_type, TELEMETRY_TYPE_MAVLINK );
   if ( g_TimeNow >= s_uTimeLastFCTelemetrySentToCentral + 250 )
   {
      s_uTimeLastFCTelemetrySentToCentral = g_TimeNow;
      _send_fc_wfbohd_telemetry_to_central();
   }
   }
   return 1;
}

#else
void wfbohd_init(bool bSearching){}
void wfbohd_cleanup(){}
bool wfbohd_is_video_player_started() {return true;}
void wfbohd_check_start_video_player(){}
int wfbohd_process_auxiliary_radio_packet(u8* pBuffer, int iBufferLength) {return 0;}
#endif
