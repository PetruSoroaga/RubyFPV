/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and/or binary forms, with or without
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
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/hardware_camera.h"
#include "../base/hw_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/parser_h264.h"
#include "../base/utils.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <getopt.h>
#include <poll.h>

#include "video_source_majestic.h"
#include "events.h"
#include "timers.h"
#include "shared_vars.h"
#include "launchers_vehicle.h"
#include "packets_utils.h"
#include "adaptive_video.h"

//To fix extern ParserH264 s_ParserH264CameraOutput;

int s_fInputVideoStreamUDPSocket = -1;
int s_iInputVideoStreamUDPPort = 5600;
u32 s_uTimeStartVideoInput = 0;
bool s_bLogStartOfInputVideoData = true;

u8 s_uInputVideoUDPBuffer[MAX_PACKET_TOTAL_SIZE];
u8 s_uOutputUDPNALFrameSegment[MAX_PACKET_TOTAL_SIZE];

u32 s_uDebugTimeLastUDPVideoInputCheck = 0;
u32 s_uDebugUDPInputBytes = 0;
u32 s_uDebugUDPInputReads = 0;

bool s_bRequestedVideoMajesticCaptureUpdate = false;
u32 s_uRequestedVideoMajesticCaptureUpdateReason = 0;
bool s_bHasPendingMajesticRealTimeChanges = false;
u32 s_uTimeLastMajesticImageRealTimeUpdate = 0;

bool s_bLastReadIsSingleNAL = false;
bool s_bLastReadIsEndNAL = false;
bool s_bIsInsideIFrameNAL = false;
bool s_bIsInsidePictureFrameNAL = false;
int s_iLastNALSize = 0;
u32 s_uTimeLastCheckMajestic = 0;
int s_iCountMajestigProcessNotRunning = 0;

u32 s_uLastVideoSourceReadTimestamps[4];
u32 s_uLastAlarmUDPOveflowTimestamp = 0;
u32 s_uLastTimeMajesticUpdate = 0;

void video_source_majestic_start_capture_program();
void video_source_majestic_stop_capture_program(int iSignal);

void video_source_majestic_init_all_params()
{
   s_uLastTimeMajesticUpdate = g_TimeNow;
   for( int i=0; i<(int)(sizeof(s_uLastVideoSourceReadTimestamps)/sizeof(s_uLastVideoSourceReadTimestamps[0])); i++ )
      s_uLastVideoSourceReadTimestamps[i] = 0;

   log_line("[VideoSourceUDP] Majestic file size: %d bytes", get_filesize("/usr/bin/majestic") );

   hardware_set_oipc_gpu_boost(g_pCurrentModel->processesPriorities.iFreqGPU);
   
   // Stop default majestic
   char szPID[256];

   video_source_majestic_stop_capture_program(-1);
   hardware_sleep_ms(100);
   video_source_majestic_stop_capture_program(-9);
   hardware_sleep_ms(50);
   hw_execute_bash_command_raw("pidof majestic", szPID);

   log_line("[VideoSourceUDP] Init: stopping majestic: PID after: (%s)", szPID);
   int iRetry = 5;
   while ( (iRetry > 0) && (0 < strlen(szPID)) )
   {
      iRetry--;
      hardware_sleep_ms(50);
      hw_execute_bash_command_raw("killall -1 majestic", NULL);
      hardware_sleep_ms(100);
      hw_execute_bash_command_raw("pidof majestic", szPID);
   }
   log_line("[VideoSourceUDP] Init: stopping majestic (2): PID after: (%s)", szPID);

   hardware_camera_apply_all_majestic_settings(g_pCurrentModel, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile]),
          g_pCurrentModel->video_params.user_selected_video_link_profile,
          &(g_pCurrentModel->video_params));

   // Start majestic if not running

   int iRepeatCount = 2;
   bool bMajesticIsRunning = false;
   char szOutput[1024];

   while ( iRepeatCount > 0 )
   {
      iRepeatCount--;
      u32 uTimeStart = get_current_timestamp_ms();
      while ( get_current_timestamp_ms() < uTimeStart+1000 )
      {
         hardware_sleep_ms(10);
         hw_execute_bash_command_silent("pidof majestic", szOutput);
         if ( (strlen(szOutput) < 3) || (strlen(szOutput) > 5) )
         {
            hardware_sleep_ms(50);
            continue;
         }
         bMajesticIsRunning = true;
         break;
      }
      if ( ! bMajesticIsRunning )
      {
         log_line("[VideoSourceUDP] Majestic is not running on first start. Start majestic...");
         video_source_majestic_start_capture_program();
         hardware_sleep_ms(200);
         continue;
      }
      else
      {
         log_line("[VideoSourceUDP] Majestic is running on first start.Majestic pid: (%s)", szOutput);
         break;
      }
      log_line("[VideoSourceUDP] Majestic is still not running, try to start it again...");
   }

// To fix 
//   g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs = g_pCurrentModel->getInitialKeyframeIntervalMs(g_pCurrentModel->video_params.user_selected_video_link_profile);
//   g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs = g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs;
//   log_line("Completed setting initial majestic params. Initial keyframe: %d", g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs );

   if ( g_pCurrentModel->processesPriorities.iNiceVideo < 0 )
   {
      hardware_sleep_ms(50);
      int iPID = hw_process_exists("majestic");
      if ( iPID > 1 )
      {
         log_line("[VideoSourceUDP] Adjust initial majestic nice priority to %d", g_pCurrentModel->processesPriorities.iNiceVideo);
         char szComm[256];
         sprintf(szComm,"renice -n %d -p %d", g_pCurrentModel->processesPriorities.iNiceVideo, iPID);
         hw_execute_bash_command(szComm, NULL);
      }
      else
         log_softerror_and_alarm("[VideoSourceUDP] Can't find the PID of majestic");
   }
   s_uLastTimeMajesticUpdate = g_TimeNow;
}

void video_source_majestic_close()
{
   if ( -1 != s_fInputVideoStreamUDPSocket )
   {
      log_line("[VideoSourceUDP] Closed input UDP socket.");
      close(s_fInputVideoStreamUDPSocket);
   }
   else
      log_line("[VideoSourceUDP] No input UDP socket to close.");
   s_fInputVideoStreamUDPSocket = -1;
}

int video_source_majestic_open(int iUDPPort)
{
   if ( -1 != s_fInputVideoStreamUDPSocket )
      return s_fInputVideoStreamUDPSocket;

   s_iInputVideoStreamUDPPort = iUDPPort;

   struct sockaddr_in server_addr;
   s_fInputVideoStreamUDPSocket = socket(AF_INET, SOCK_DGRAM, 0);
   if (s_fInputVideoStreamUDPSocket == -1)
   {
      log_softerror_and_alarm("[VideoSourceUDP] Can't create socket for video stream on port %d.", s_iInputVideoStreamUDPPort);
      return -1;
   }


   const int optval = 1;
   if ( 0 != setsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(optval)) )
       log_softerror_and_alarm("[VideoSourceUDP] Failed to set SO_REUSEADDR: %s", strerror(errno));

   if ( 0 != setsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_RXQ_OVFL, (const void *)&optval , sizeof(optval)) )
       log_softerror_and_alarm("[VideoSourceUDP] Unable to set SO_RXQ_OVFL: %s", strerror(errno));
   
   int iRecvSize = 0;
   socklen_t iParamLen = sizeof(iRecvSize);
   getsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_RCVBUF, &iRecvSize, &iParamLen);
   log_line("[VideoSourceUDP] Current socket recv buffer size: %d", iRecvSize);

   int iWantedRecvSize = 512*1024;
   if( 0 != setsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_RCVBUF, (const void *)&iWantedRecvSize, sizeof(iWantedRecvSize)))
      log_softerror_and_alarm("[VideoSourceUDP] Unable to set SO_RCVBUF: %s", strerror(errno));

   iRecvSize = 0;
   iParamLen = sizeof(iRecvSize);
   getsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_RCVBUF, &iRecvSize, &iParamLen);
   log_line("[VideoSourceUDP] New current socket recv buffer size: %d (requested: %d)", iRecvSize, iWantedRecvSize);
 
   memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   server_addr.sin_port = htons((unsigned short)s_iInputVideoStreamUDPPort );
 
   if( bind(s_fInputVideoStreamUDPSocket,(struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
   {
      log_error_and_alarm("[VideoSourceUDP] Failed to bind socket for video stream to port %d.", s_iInputVideoStreamUDPPort);
      close(s_fInputVideoStreamUDPSocket);
      s_fInputVideoStreamUDPSocket = -1;
      return -1;
   }
   s_uTimeStartVideoInput = g_TimeNow;

   log_line("[VideoSourceUDP] Opened read socket on port %d for reading video stream. socket fd = %d", s_iInputVideoStreamUDPPort, s_fInputVideoStreamUDPSocket);
   
   return s_fInputVideoStreamUDPSocket;
}

u32 video_source_majestic_get_program_start_time()
{
   return s_uTimeStartVideoInput;
}

void video_source_majestic_start_capture_program()
{
   hardware_set_oipc_gpu_boost(g_pCurrentModel->processesPriorities.iFreqGPU);

   if ( g_pCurrentModel->processesPriorities.uProcessesFlags & PROCESSES_FLAGS_BALANCE_INT_CORES )
      hardware_balance_interupts();
   hw_execute_bash_command("/usr/bin/majestic -s 2>&1 1>/dev/null &", NULL);
   hardware_sleep_ms(50);
   int iPID = hw_process_exists("majestic");
   log_line("[VideoSourceUDP] New majestic PID: (%d)", iPID);
   if ( g_pCurrentModel->processesPriorities.iNiceVideo < 0 )
   {
      if ( iPID > 1 )
      {
         log_line("[VideoSourceUDP] Adjust majestic nice priority to %d", g_pCurrentModel->processesPriorities.iNiceVideo);
         char szComm[256];
         sprintf(szComm,"renice -n %d -p %d", g_pCurrentModel->processesPriorities.iNiceVideo, iPID);
         hw_execute_bash_command(szComm, NULL);
      }
      else
         log_softerror_and_alarm("[VideoSourceUDP] Can't find the PID of majestic");
   }

   hardware_camera_set_daylight_off( g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile].uFlags & CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF);

   adaptive_video_on_capture_restarted();
   s_uTimeLastCheckMajestic = g_TimeNow-3000;
   s_iCountMajestigProcessNotRunning = 0;
   s_uTimeStartVideoInput = g_TimeNow;
   s_bLogStartOfInputVideoData = true;
   s_uLastTimeMajesticUpdate = g_TimeNow;
}

void video_source_majestic_stop_capture_program(int iSignal)
{
   hw_kill_process("majestic", iSignal);
   char szOutput[256];
   szOutput[0] = 0;
   hw_execute_bash_command_raw("pidof majestic", szOutput);
   log_line("[VideoSourceUDP] Majestic PID after stop command: (%s)", szOutput);
}

void video_source_majestic_request_update_program(u32 uChangeReason)
{
   log_line("[VideoSourceUDP] Majestic was flagged for restart (reason: %d, %s)", uChangeReason & 0xFF, str_get_model_change_type(uChangeReason & 0xFF));
   s_bRequestedVideoMajesticCaptureUpdate = true;
   s_uRequestedVideoMajesticCaptureUpdateReason = uChangeReason;
}

uint32_t extract_udp_rxq_overflow(struct msghdr *msg)
{
    struct cmsghdr *cmsg;
    uint32_t rtn;

    for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_RXQ_OVFL) {
            memcpy(&rtn, CMSG_DATA(cmsg), sizeof(rtn));
            return rtn;
        }
    }
    return 0;
}

void video_source_majestic_set_keyframe_value(float fGOP)
{
   char szComm[128];
   char szOutput[256];
   sprintf(szComm, "curl -s localhost/api/v1/set?video0.gopSize=%.1f", fGOP);
   hw_execute_bash_command_raw(szComm, szOutput);
   //hw_execute_bash_command_raw("curl -s localhost/api/v1/reload", szOutput);

   //sprintf(szComm, "cli -s .video0.gopSize %.1f", fGOP);
   //hw_execute_bash_command_raw(szComm, NULL);
   //hw_execute_bash_command_raw("killall -1 majestic", NULL);
   s_uLastTimeMajesticUpdate = g_TimeNow;
}

void video_source_majestic_set_videobitrate_value(u32 uBitrate)
{
   char szComm[128];
   //char szOutput[256];
   sprintf(szComm, "curl -s localhost/api/v1/set?video0.bitrate=%u", uBitrate/1000);
   hw_execute_bash_command_raw(szComm, NULL);
   sprintf(szComm, "cli -s .video0.bitrate %u", uBitrate/1000);
   hw_execute_bash_command(szComm, NULL);
   //hw_execute_bash_command_raw("curl -s localhost/api/v1/reload", szOutput); 
   s_uLastTimeMajesticUpdate = g_TimeNow;
}

void video_source_majestic_set_qpdelta_value(int iqpdelta)
{
   char szComm[128];
   //char szOutput[256];
   sprintf(szComm, "curl -s localhost/api/v1/set?video0.qpDelta=%d", iqpdelta);
   hw_execute_bash_command_raw(szComm, NULL);
   sprintf(szComm, "cli -s .video0.qpDelta %d", iqpdelta);
   hw_execute_bash_command(szComm, NULL);
   //hw_execute_bash_command_raw("curl -s localhost/api/v1/reload", szOutput);
   s_uLastTimeMajesticUpdate = g_TimeNow;
}

int _video_source_majestic_try_read_input_udp_data(bool bAsync)
{
   if ( -1 == s_fInputVideoStreamUDPSocket )
      return -1;

   int nRecvBytes = 0;
   if ( bAsync )
   {
      static uint8_t cmsgbuf[CMSG_SPACE(sizeof(uint32_t))];

      static int nfds = 1;
      static struct pollfd fds[2];
      static bool s_bFirstVideoUDPReadSetup = true;

      if ( s_bFirstVideoUDPReadSetup )
      {
         s_bFirstVideoUDPReadSetup = false;
         memset(fds, '\0', sizeof(fds));
         if ( fcntl(s_fInputVideoStreamUDPSocket, F_SETFL, fcntl(s_fInputVideoStreamUDPSocket, F_GETFL, 0) | O_NONBLOCK) < 0 )
            log_softerror_and_alarm("[VideoSourceUDP] Unable to set socket into nonblocked mode: %s", strerror(errno));

         fds[0].fd = s_fInputVideoStreamUDPSocket;
         fds[0].events = POLLIN;

         log_line("[VideoSourceUDP] Done first time video USD socket setup.");
      }

      int res = poll(fds, nfds, 1);
      if ( res < 0 )
      {
         log_error_and_alarm("[VideoSourceUDP] Failed to poll UDP socket.");
         return -1;
      }
      if ( 0 == res )
         return 0;

      if (fds[0].revents & (POLLERR | POLLNVAL))
      {
         log_softerror_and_alarm("[VideoSourceUDP] Socket error pooling: %s", strerror(errno));
         return -1;
      }
      if ( ! (fds[0].revents & POLLIN) )
         return 0;
    
      struct iovec iov = { .iov_base = (void*)s_uInputVideoUDPBuffer,
                                            .iov_len = sizeof(s_uInputVideoUDPBuffer) };
      struct msghdr msghdr = { .msg_name = NULL,
                               .msg_namelen = 0,
                               .msg_iov = &iov,
                               .msg_iovlen = 1,
                               .msg_control = &cmsgbuf,
                               .msg_controllen = sizeof(cmsgbuf),
                               .msg_flags = 0 };

      memset(cmsgbuf, '\0', sizeof(cmsgbuf));

      nRecvBytes = recvmsg(s_fInputVideoStreamUDPSocket, &msghdr, 0);
      if ( nRecvBytes < 0 )
      {
         log_softerror_and_alarm("[VideoSourceUDP] Failed to recvmsg from UDP socket, error: %s", strerror(errno));
         return -1;
      }

      for( int i=3; i>0; i-- )
         s_uLastVideoSourceReadTimestamps[i] = s_uLastVideoSourceReadTimestamps[i-1];
      s_uLastVideoSourceReadTimestamps[0] = g_TimeNow;

      if ( nRecvBytes > MAX_PACKET_TOTAL_SIZE )
      {
         log_softerror_and_alarm("[VideoSourceUDP] Read too much data %d bytes from UDP socket", nRecvBytes);
         nRecvBytes = MAX_PACKET_TOTAL_SIZE;
      }

      static uint32_t rxq_overflow = 0;
      uint32_t cur_rxq_overflow = extract_udp_rxq_overflow(&msghdr);
      if (cur_rxq_overflow != rxq_overflow)
      {
          u32 uDroppedCount = cur_rxq_overflow - rxq_overflow;
          log_softerror_and_alarm("[VideoSourceUDP] UDP rxq overflow: %u packets dropped (from %u to %u)", uDroppedCount, rxq_overflow, cur_rxq_overflow);
          
          if ( g_TimeNow > s_uLastAlarmUDPOveflowTimestamp + 10000 )
          if ( g_TimeNow > g_TimeStart + 10000 )
          if ( g_TimeNow > s_uLastTimeMajesticUpdate + 3000 )
          {
             s_uLastAlarmUDPOveflowTimestamp = g_TimeNow;
             u32 uFlags2 = 0;
             u32 uDelta = s_uLastVideoSourceReadTimestamps[0] - s_uLastVideoSourceReadTimestamps[1];
             if ( uDelta > 255 )
                uDelta = 255;
             uFlags2 |= uDelta & 0xFF;
             uDelta = s_uLastVideoSourceReadTimestamps[1] - s_uLastVideoSourceReadTimestamps[2];
             if ( uDelta > 255 )
                uDelta = 255;
             uFlags2 |= (uDelta & 0xFF) << 8;
             uDelta = s_uLastVideoSourceReadTimestamps[2] - s_uLastVideoSourceReadTimestamps[3];
             if ( uDelta > 255 )
                uDelta = 255;
             uFlags2 |= (uDelta & 0xFF) << 16;
             
             send_alarm_to_controller(ALARM_ID_VIDEO_CAPTURE_MALFUNCTION, 0x0002 | ((uDroppedCount & 0xFF) << 8), uFlags2, 5);
          }
          rxq_overflow = cur_rxq_overflow;
      }
   }
   else
   {
      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(s_fInputVideoStreamUDPSocket, &readset);

      struct timeval timePipeInput;
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 5*1000; // 5 miliseconds timeout

      int iSelectResult = select(s_fInputVideoStreamUDPSocket+1, &readset, NULL, NULL, &timePipeInput);
      if ( iSelectResult < 0 )
      {
         log_error_and_alarm("[VideoSourceUDP] Failed to select socket.");
         return -1;
      }
      if ( iSelectResult == 0 )
         return 0;

      if( 0 == FD_ISSET(s_fInputVideoStreamUDPSocket, &readset) )
         return 0;

      struct sockaddr_in client_addr;
      socklen_t len = sizeof(client_addr);
      
      memset(&client_addr, 0, sizeof(client_addr));
      client_addr.sin_family = AF_INET;
      client_addr.sin_addr.s_addr = INADDR_ANY;
      client_addr.sin_port = htons( s_iInputVideoStreamUDPPort );

      nRecvBytes = recvfrom(s_fInputVideoStreamUDPSocket, s_uInputVideoUDPBuffer, sizeof(s_uInputVideoUDPBuffer)/sizeof(s_uInputVideoUDPBuffer[0]), 
                MSG_WAITALL, ( struct sockaddr *) &client_addr,
                &len);
      //int nRecv = recv(socket_server, szBuff, 1024, )
      if ( nRecvBytes < 0 )
      {
         log_error_and_alarm("[VideoSourceUDP] Failed to receive from UDP socket.");
         return -1;
      }
      if ( nRecvBytes == 0 )
         return 0;
   }
   return nRecvBytes;
}

// Parse input raw bytes and returns a NAL packet (in s_uOutputUDPNALFrameSegment)

int _video_source_majestic_parse_rtp_data(u8* pInputRawData, int iInputBytes)
{
   s_bLastReadIsSingleNAL = false;
   s_bLastReadIsEndNAL = false;

   if ( iInputBytes <= 12 )
   {
      log_softerror_and_alarm("[VideoSourceUDP] Process RTP packet too small, only %d bytes", iInputBytes);
      return 0;
   }
 
   int iRTPHeaderLength = 0;
   if ( (pInputRawData[0] & 0x80) && (pInputRawData[1] & 0x60) )
      iRTPHeaderLength = 12;

   // ----------------------------------------------
   // Begin - Check RTP sequence number
   static u16 s_uLastRTLSeqNumberInUDPFrame = 0;
   
   u16 uRTPSeqNb = (((u16)pInputRawData[2]) << 8) | pInputRawData[3];

   if ( (0 != s_uLastRTLSeqNumberInUDPFrame) && (65535 != s_uLastRTLSeqNumberInUDPFrame) )
   if ( (s_uLastRTLSeqNumberInUDPFrame + 1) != uRTPSeqNb )
      log_softerror_and_alarm("[VideoSourceUDP] Read skipped RTP frames, from seqnb %d to seqnb %d", s_uLastRTLSeqNumberInUDPFrame, uRTPSeqNb);
   s_uLastRTLSeqNumberInUDPFrame = uRTPSeqNb;

   // End - Check RTP sequence number
   // ----------------------------------------------

   u8 uNALOutputHeader[6];
   uNALOutputHeader[0] = 0;
   uNALOutputHeader[1] = 0;
   uNALOutputHeader[2] = 0;
   uNALOutputHeader[3] = 0x01;
   uNALOutputHeader[4] = 0xFF; // NAL type H264/H265
   uNALOutputHeader[5] = 0xFF; // H265 extra header
   
   int iNALOutputHeaderSize = 5;

   pInputRawData += iRTPHeaderLength;
   iInputBytes -= iRTPHeaderLength;

   u8 uNALHeaderByte = *pInputRawData;
   u8 uNALTypeH264 = uNALHeaderByte & 0x1F;
   u8 uNALTypeH265 = (uNALHeaderByte>>1) & 0x3F;

   pInputRawData++;
   iInputBytes--;

   int iOutputBytes = 0;

   // Single compact (non-fragmented, non I/P frame (slice)) NAL unit (ie. SPS, PPS, etc)
   if ( (uNALTypeH264 != 28) && (uNALTypeH265 != 49) )
   {
      // Regular NAL unit
      s_bLastReadIsSingleNAL = true;
      s_bLastReadIsEndNAL = true;
      s_bIsInsidePictureFrameNAL = false;
      s_bIsInsideIFrameNAL = false;
      uNALOutputHeader[4] = uNALHeaderByte;
      memcpy(s_uOutputUDPNALFrameSegment, uNALOutputHeader, iNALOutputHeaderSize);
      memcpy(&s_uOutputUDPNALFrameSegment[iNALOutputHeaderSize], pInputRawData, iInputBytes);
      iOutputBytes = iInputBytes + iNALOutputHeaderSize;
      s_iLastNALSize += iOutputBytes;
      //log_line("DEBUG NAL SPS/PPS: %d bytes", s_iLastNALSize);
      s_iLastNALSize = 0;
      return iOutputBytes;
   }
   
   // Fragmentation unit (fragmented NAL over multiple packets)

   s_bLastReadIsSingleNAL = false;
   s_bLastReadIsEndNAL = false;
   s_bIsInsidePictureFrameNAL = true;

   u8 uFUHeaderByte = 0;
   u8 uFUStartBit = 0;
   u8 uFUEndBit = 0;

   if ( uNALTypeH264 == 28 ) 
   {
      // H264 fragment
      uFUHeaderByte = *pInputRawData;
      uFUStartBit = uFUHeaderByte & 0x80;
      uFUEndBit = uFUHeaderByte & 0x40;
      pInputRawData++;
      iInputBytes--;
      uNALOutputHeader[4] = (uNALHeaderByte & 0xE0) | (uFUHeaderByte & 0x1F);
      // H264 frame type: lower 5 bits (&0x1F) of uNALOutputHeader[4]: 5 - Iframe, 1 - Pframe
      //log_line("DEBUG fragment NAL264 %d, type: %d (%d)", uNALTypeH264, uNALOutputHeader[4], uNALOutputHeader[4] & 0x1F);
   
      if ( (uNALOutputHeader[4] & 0x1F) == 5 )
         s_bIsInsideIFrameNAL = true;
      else
         s_bIsInsideIFrameNAL = false;
   }
   else 
   {
      // H265 fragment

      pInputRawData++;
      iInputBytes--;
      uFUHeaderByte = *pInputRawData;
      uFUStartBit = uFUHeaderByte & 0x80;
      uFUEndBit = uFUHeaderByte & 0x40;
      pInputRawData++;
      iInputBytes--;

      uNALOutputHeader[4] = (uNALHeaderByte & 0x81) | (uFUHeaderByte & 0x3F) << 1;
      uNALOutputHeader[5] = 1;
      iNALOutputHeaderSize++;

      //log_line("DEBUG fragment NAL265 %d, type: %d (%d) (%d) (%d)", uNALTypeH265, uNALOutputHeader[4], (uNALOutputHeader[4] >>1) & 0x3F, uNALOutputHeader[4] & 0x3F, (uNALOutputHeader[4] & 0x3F) >> 1);
   
      // (uNALOutputHeader[4] >> 1) & 0x3F   ->  19: Iframe;  1: Pframe

      if ( ((uNALOutputHeader[4] >> 1) & 0x3F) == 19 )
         s_bIsInsideIFrameNAL = true;
      else
         s_bIsInsideIFrameNAL = false;

   }

   if ( uFUEndBit )
   {
      s_bLastReadIsEndNAL = true;
      s_iLastNALSize += iInputBytes;
      //if ( s_bIsInsideIFrameNAL )
      //   log_line("DEBUG NAL I: %d bytes", s_iLastNALSize);
      //else
      //   log_line("DEBUG NAL P: %d bytes", s_iLastNALSize);
      s_iLastNALSize = 0;
   }
   else
      s_iLastNALSize += iInputBytes;

   if (uFUStartBit) 
   {
      memcpy(s_uOutputUDPNALFrameSegment, uNALOutputHeader, iNALOutputHeaderSize);
      memcpy(&s_uOutputUDPNALFrameSegment[iNALOutputHeaderSize], pInputRawData, iInputBytes);
      iOutputBytes = iInputBytes + iNALOutputHeaderSize;
      //log_line("DEBUG parsed start of NAL, type %d, end bit: %d, output %d bytes as start of NAL",
      //   uNALTypeH264, uFUEndBit?1:0, iOutputBytes);
   }
   else 
   {
      memcpy(s_uOutputUDPNALFrameSegment, pInputRawData, iInputBytes);
      iOutputBytes = iInputBytes;
      //log_line("DEBUG parsed middle of NAL, type %d, end bit: %d, output %d bytes as middle of NAL",
      //   uNALTypeH264, uFUEndBit?1:0, iOutputBytes);
   }
   return iOutputBytes;
}


// Returns the buffer and number of bytes read
u8* video_source_majestic_read(int* piReadSize, bool bAsync)
{
   if ( NULL == piReadSize )
      return NULL;

   *piReadSize = 0;

   int iRecvBytes = _video_source_majestic_try_read_input_udp_data(bAsync);
   if ( iRecvBytes <= 0 )
      return NULL;

   if ( s_bLogStartOfInputVideoData )
   {
      log_line("[VideoSourceUDP] Start receiving data (H264/H265 stream) from camera");
      s_bLogStartOfInputVideoData = false;
   }
   s_uDebugUDPInputBytes += iRecvBytes;
   s_uDebugUDPInputReads++;

   //log_line("DEBUG recv %d bytes from camera", iRecvBytes);
   int iOutputBytes = _video_source_majestic_parse_rtp_data(s_uInputVideoUDPBuffer, iRecvBytes);
   static int siMinP = 10000;
   static int siMaxP = 0;
   if ( iOutputBytes > siMaxP )
      siMaxP = iOutputBytes;
   if ( iOutputBytes > 0 )
   if ( iOutputBytes < siMinP )
      siMinP = iOutputBytes;
   //log_line("DEBUG read %d bytes, %d H264 bytes, %d min, %d max", iRecvBytes, iOutputBytes, siMinP, siMaxP);

   *piReadSize = iOutputBytes;
   return s_uOutputUDPNALFrameSegment;
}

bool video_source_majestic_last_read_is_single_nal()
{
   return s_bLastReadIsSingleNAL;
}

bool video_source_majestic_last_read_is_end_nal()
{
   return s_bLastReadIsEndNAL;
}

bool video_source_majestic_is_inside_iframe()
{
   return s_bIsInsideIFrameNAL;
}

bool video_source_majestic_las_read_is_picture_frame()
{
   return s_bIsInsidePictureFrameNAL;
}

void _video_source_majestic_check_params_update()
{
   char szComm[128];
   char szOutput[256];

   camera_profile_parameters_t* pCameraParams = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile]);
   video_parameters_t* pVideoParams = &(g_pCurrentModel->video_params);
   u32 uParam = (s_uRequestedVideoMajesticCaptureUpdateReason>>16);

   bool bUpdatedImageParams = false;
   if ( (s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_CAMERA_BRIGHTNESS )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?image.luminance=%u", uParam);
      hw_execute_bash_command_raw(szComm, szOutput);
      //hw_execute_bash_command_raw("curl -s localhost/api/v1/reload", szOutput);
      bUpdatedImageParams = true;
   }
   else if ( (s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_CAMERA_CONTRAST )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?image.contrast=%u", uParam);
      hw_execute_bash_command_raw(szComm, szOutput);
      //hw_execute_bash_command_raw("curl -s localhost/api/v1/reload", szOutput);
      bUpdatedImageParams = true;
   }
   else if ( (s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_CAMERA_SATURATION )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?image.saturation=%u", uParam/2);
      hw_execute_bash_command_raw(szComm, szOutput);
      //hw_execute_bash_command_raw("curl -s localhost/api/v1/reload", szOutput);
      bUpdatedImageParams = true;
   }
   else if ( (s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_CAMERA_HUE )
   {
      sprintf(szComm, "curl -s localhost/api/v1/set?image.hue=%u", uParam);
      hw_execute_bash_command_raw(szComm, szOutput);
      //hw_execute_bash_command_raw("curl -s localhost/api/v1/reload", szOutput);
      bUpdatedImageParams = true;
   }

   if ( bUpdatedImageParams )
   {
      s_bRequestedVideoMajesticCaptureUpdate = false;
      s_bHasPendingMajesticRealTimeChanges = true;
      s_uTimeLastMajesticImageRealTimeUpdate = g_TimeNow;
      s_uRequestedVideoMajesticCaptureUpdateReason = 0;
      s_uLastTimeMajesticUpdate = g_TimeNow;
      return;
   }

   // Stop majestic, apply settings, restart majestic
   log_line("[VideoSourceUDP] Has pending changes and restart of majestic...");

   char szPIDBefore[256];
   char szPIDAfter[256];
   szPIDBefore[0] = 0;
   szPIDAfter[0] = 0;
   hw_execute_bash_command_raw("pidof majestic", szPIDBefore);
   hardware_sleep_ms(50);
   video_source_majestic_stop_capture_program(-1);
   hardware_sleep_ms(100);
   video_source_majestic_stop_capture_program(-9);
   hardware_sleep_ms(50);
   hw_execute_bash_command_raw("pidof majestic", szPIDAfter);

   log_line("[VideoSourceUDP] Stopping majestic: PID before: (%s), PID after: (%s)", szPIDBefore, szPIDAfter);
   int iRetry = 5;
   while ( (iRetry > 0) && (0 < strlen(szPIDAfter)) )
   {
      iRetry--;
      hardware_sleep_ms(50);
      hw_execute_bash_command_raw("killall -1 majestic", NULL);
      hardware_sleep_ms(100);
      hw_execute_bash_command_raw("pidof majestic", szPIDAfter);
   }

   if ( 0 < strlen(szPIDAfter) )
   {
      send_alarm_to_controller_now(ALARM_ID_VIDEO_CAPTURE_MALFUNCTION, 1,0, 20);
      hardware_reboot();
   }

   if ( (s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_CAMERA_PARAMS )
   {
      log_line("[VideoSourceUDP] Periodic loop: apply all majestic camera only settings.");
      hardware_camera_apply_all_majestic_camera_settings(pCameraParams);
   }
   else
   {
      log_line("[VideoSourceUDP] Periodic loop: signaled to apply all majestic settings.");
      hardware_camera_apply_all_majestic_settings(g_pCurrentModel, pCameraParams, g_pCurrentModel->video_params.user_selected_video_link_profile, pVideoParams);
      // To fix 
      // g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs = g_pCurrentModel->getInitialKeyframeIntervalMs(g_pCurrentModel->video_params.user_selected_video_link_profile);
      // g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs = g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs;
   }

   if ( ((s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_VIDEO_RESOLUTION) ||
        ((s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_VIDEO_CODEC) )
   {
      log_line("[VideoSourceUDP] Periodic loop: signaled changed video resolution or codec.");
   }

   hardware_set_oipc_gpu_boost(g_pCurrentModel->processesPriorities.iFreqGPU);
   video_source_majestic_start_capture_program();
   vehicle_check_update_processes_affinities(false, false);
   s_bRequestedVideoMajesticCaptureUpdate = false;
   s_uRequestedVideoMajesticCaptureUpdateReason = 0;
}

void video_source_majestic_periodic_checks()
{
   if ( g_TimeNow >= s_uDebugTimeLastUDPVideoInputCheck+10000 )
   {
      char szBitrate[64];
      str_format_bitrate(s_uDebugUDPInputBytes/10*8, szBitrate);

      log_line("[VideoSourceUDP] Input video data: %u bytes/sec, %s, %u reads/sec",
         s_uDebugUDPInputBytes/10, szBitrate, s_uDebugUDPInputReads/10);
      s_uDebugTimeLastUDPVideoInputCheck = g_TimeNow;
      // To fix log_line("[VideoSourceUDP] Detected video stream fps: %d, slices: %d", (int)s_ParserH264CameraOutput.getDetectedFPS(), s_ParserH264CameraOutput.getDetectedSlices());
      s_uDebugUDPInputBytes = 0;
      s_uDebugUDPInputReads = 0;
   }

   if ( g_TimeNow > s_uTimeLastCheckMajestic + 5000 )
   {
      s_uTimeLastCheckMajestic = g_TimeNow;
      char szOutput[128];
      szOutput[0] = 0;
      hw_execute_bash_command_silent("pidof majestic", szOutput);
      if ( (strlen(szOutput) < 3) || (strlen(szOutput) > 6) )
      {
         s_iCountMajestigProcessNotRunning++;
         if ( s_iCountMajestigProcessNotRunning >= 2 )
         {
            send_alarm_to_controller(ALARM_ID_VIDEO_CAPTURE_MALFUNCTION,0,0, 5);
            s_iCountMajestigProcessNotRunning = -2;
         }

         log_softerror_and_alarm("[VideoSourceUDP] majestic is not running. starting it.");
         video_source_majestic_start_capture_program();
         vehicle_check_update_processes_affinities(false, false);
      }
   }

   if ( s_bHasPendingMajesticRealTimeChanges )
   if ( g_TimeNow > s_uTimeLastMajesticImageRealTimeUpdate + 2000 )
   {
      log_line("[VideoSourceUDP] Has pending image settings changes. Persist them now.");
      s_bHasPendingMajesticRealTimeChanges = false;
      s_uTimeLastMajesticImageRealTimeUpdate = g_TimeNow;
      camera_profile_parameters_t* pCameraParams = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile]);
      // Save image settings to yaml file
      hardware_camera_apply_all_majestic_camera_settings(pCameraParams);
      s_uLastTimeMajesticUpdate = g_TimeNow;
   }

   if ( s_bRequestedVideoMajesticCaptureUpdate )
      _video_source_majestic_check_params_update();

}