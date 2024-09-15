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

extern ParserH264 s_ParserH264CameraOutput;

int s_fInputVideoStreamUDPSocket = -1;
int s_iInputVideoStreamUDPPort = 5600;
u32 s_uTimeStartVideoInput = 0;

u8 s_uInputVideoUDPBuffer[MAX_PACKET_TOTAL_SIZE];
u8 s_uOutputUDPNALFrameSegment[MAX_PACKET_TOTAL_SIZE];

u32 s_uDebugTimeLastUDPVideoInputCheck = 0;
u32 s_uDebugUDPInputBytes = 0;
u32 s_uDebugUDPInputReads = 0;

bool s_bRequestedVideoMajesticCaptureUpdate = false;
u32 s_uRequestedVideoMajesticCaptureUpdateReason = 0;
bool s_bHasPendingMajesticRealTimeChanges = false;
u32 s_uTimeLastMajesticImageRealTimeUpdate = 0;

u32 s_uTimeLastCheckMajestic = 0;
int s_iCountMajestigProcessNotRunning = 0;

void video_source_majestic_start_capture_program();

void video_source_majestic_init_all_params()
{
   log_line("[VideoSourceUDP] Majestic file size: %d bytes", get_filesize("/usr/bin/majestic") );

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

   hardware_camera_apply_all_majestic_settings(g_pCurrentModel, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile]),
          g_pCurrentModel->video_params.user_selected_video_link_profile,
          &(g_pCurrentModel->video_params));
   g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs = g_pCurrentModel->getInitialKeyframeIntervalMs(g_pCurrentModel->video_params.user_selected_video_link_profile);
   g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs = g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs;
   log_line("Completed setting initial majestic params. Initial keyframe: %d", g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs );

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
   /*
   int iRecvSize = MAX_PACKET_TOTAL_SIZE;
   if( 0 != setsockopt(s_fInputVideoStreamUDPSocket, SOL_SOCKET, SO_RCVBUF, (const void *)&iRecvSize , sizeof(iRecvSize)))
      log_softerror_and_alarm("[VideoSourceUDP] Unable to set SO_RCVBUF: %s", strerror(errno));
   */

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
   hw_execute_bash_command("/usr/bin/majestic -s 2>&1 1>/dev/null &", NULL);
   s_uTimeLastCheckMajestic = g_TimeNow;
   hardware_sleep_ms(50);
   if ( g_pCurrentModel->processesPriorities.iNiceVideo < 0 )
   {
      int iPID = hw_process_exists("majestic");
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
}

void video_source_majestic_stop_capture_program()
{
   hw_execute_bash_command_raw("pidof majestic | xargs kill -9 2>/dev/null ", NULL);
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
   sprintf(szComm, "curl localhost/api/v1/set?video0.gopSize=%.1f", fGOP);
   hw_execute_bash_command_raw(szComm, szOutput);
   //hw_execute_bash_command_raw("curl localhost/api/v1/reload", szOutput);

   //sprintf(szComm, "cli -s .video0.gopSize %.1f", fGOP);
   //hw_execute_bash_command_raw(szComm, NULL);
   //hw_execute_bash_command_raw("killall -1 majestic", NULL);
}

void video_source_majestic_set_videobitrate_value(u32 uBitrate)
{
   char szComm[128];
   char szOutput[256];
   sprintf(szComm, "curl localhost/api/v1/set?video0.bitrate=%u", uBitrate/1000);
   hw_execute_bash_command_raw(szComm, szOutput);
   //hw_execute_bash_command_raw("curl localhost/api/v1/reload", szOutput); 
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

      if ( nRecvBytes > MAX_PACKET_TOTAL_SIZE )
      {
         log_softerror_and_alarm("[VideoSourceUDP] Read too much data %d bytes from UDP socket", nRecvBytes);
         nRecvBytes = MAX_PACKET_TOTAL_SIZE;
      }

      static uint32_t rxq_overflow = 0;
      uint32_t cur_rxq_overflow = extract_udp_rxq_overflow(&msghdr);
      if (cur_rxq_overflow != rxq_overflow)
      {
          log_softerror_and_alarm("[VideoSourceUDP] UDP rxq overflow: %u packets dropped (from %u to %u)", cur_rxq_overflow - rxq_overflow, rxq_overflow, cur_rxq_overflow);
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

   if ( 0 != s_uLastRTLSeqNumberInUDPFrame )
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

   if ( (uNALTypeH264 != 28) && (uNALTypeH265 != 49) )
   {
      //log_line("DEBUG regular NAL %d, header byte: %d", uNALTypeH264, uNALHeaderByte);
      // Regular NAL unit
      uNALOutputHeader[4] = uNALHeaderByte;
      memcpy(s_uOutputUDPNALFrameSegment, uNALOutputHeader, iNALOutputHeaderSize);
      memcpy(&s_uOutputUDPNALFrameSegment[iNALOutputHeaderSize], pInputRawData, iInputBytes);
      iOutputBytes = iInputBytes + iNALOutputHeaderSize;
   }
   else
   {
      // Fragmentation unit (fragmented NAL over multiple packets)
      u8 uFUHeaderByte = *pInputRawData;
      u8 uFUStartBit = uFUHeaderByte & 0x80;
      pInputRawData++;
      iInputBytes--;

      if ( uNALTypeH264 == 28 ) 
      {
         // H264 fragment
         uNALOutputHeader[4] = (uNALHeaderByte & 0xE0) | (uFUHeaderByte & 0x1F);
         //log_line("DEBUG fragment NAL %d, %d", uNALTypeH264, uNALOutputHeader[4]);
      }
      else 
      {
         // H265 fragment
         uFUHeaderByte = *pInputRawData;
         uFUStartBit = uFUHeaderByte & 0x80;
         pInputRawData++;
         iInputBytes--;

         uNALOutputHeader[4] = (uNALHeaderByte & 0x81) | (uFUHeaderByte & 0x3F) << 1;
         uNALOutputHeader[5] = 1;
         iNALOutputHeaderSize++;
      }

      if (uFUStartBit) 
      {
         //log_line("DEBUG start bit");
         memcpy(s_uOutputUDPNALFrameSegment, uNALOutputHeader, iNALOutputHeaderSize);
         memcpy(&s_uOutputUDPNALFrameSegment[iNALOutputHeaderSize], pInputRawData, iInputBytes);
         iOutputBytes = iInputBytes + iNALOutputHeaderSize;
      }
      else 
      {
         memcpy(s_uOutputUDPNALFrameSegment, pInputRawData, iInputBytes);
         iOutputBytes = iInputBytes;
      }
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

   s_uDebugUDPInputBytes += iRecvBytes;
   s_uDebugUDPInputReads++;

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

void video_source_majestic_periodic_checks()
{
   if ( g_TimeNow >= s_uDebugTimeLastUDPVideoInputCheck+10000 )
   {
      log_line("[VideoSourceUDP] Input video data: %u bytes/sec, %u bps, %u reads/sec",
         s_uDebugUDPInputBytes/10, s_uDebugUDPInputBytes/10*8, s_uDebugUDPInputReads/10);
      s_uDebugTimeLastUDPVideoInputCheck = g_TimeNow;
      log_line("[VideoSourceUDP] Detected video stream fps: %d, slices: %d", (int)s_ParserH264CameraOutput.getDetectedFPS(), s_ParserH264CameraOutput.getDetectedSlices());
      s_uDebugUDPInputBytes = 0;
      s_uDebugUDPInputReads = 0;
   }

   if ( g_TimeNow > s_uTimeLastCheckMajestic + 5000 )
   {
      s_uTimeLastCheckMajestic = g_TimeNow;
      char szOutput[128];
      szOutput[0] = 0;
      hw_execute_bash_command_silent("pidof majestic", szOutput);
      if ( (strlen(szOutput) < 3) || (strlen(szOutput) > 5) )
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
      // Save image settings to yaml file, but do not restart majestic
      hardware_camera_apply_all_majestic_camera_settings(pCameraParams, false);
   }

   if ( s_bRequestedVideoMajesticCaptureUpdate )
   {
      char szComm[128];
      char szOutput[256];
      s_bRequestedVideoMajesticCaptureUpdate = false;
      camera_profile_parameters_t* pCameraParams = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile]);
      video_parameters_t* pVideoParams = &(g_pCurrentModel->video_params);
      u32 uParam = (s_uRequestedVideoMajesticCaptureUpdateReason>>16);

      bool bUpdatedImageParams = false;

      if ( (s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_CAMERA_BRIGHTNESS )
      {
         sprintf(szComm, "curl localhost/api/v1/set?image.luminance=%u", uParam);
         hw_execute_bash_command_raw(szComm, szOutput);
         //hw_execute_bash_command_raw("curl localhost/api/v1/reload", szOutput);
         bUpdatedImageParams = true;
      }
      else if ( (s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_CAMERA_CONTRAST )
      {
         sprintf(szComm, "curl localhost/api/v1/set?image.contrast=%u", uParam);
         hw_execute_bash_command_raw(szComm, szOutput);
         //hw_execute_bash_command_raw("curl localhost/api/v1/reload", szOutput);
         bUpdatedImageParams = true;
      }
      else if ( (s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_CAMERA_SATURATION )
      {
         sprintf(szComm, "curl localhost/api/v1/set?image.saturation=%u", uParam/2);
         hw_execute_bash_command_raw(szComm, szOutput);
         //hw_execute_bash_command_raw("curl localhost/api/v1/reload", szOutput);
         bUpdatedImageParams = true;
      }
      else if ( (s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_CAMERA_HUE )
      {
         sprintf(szComm, "curl localhost/api/v1/set?image.hue=%u", uParam);
         hw_execute_bash_command_raw(szComm, szOutput);
         //hw_execute_bash_command_raw("curl localhost/api/v1/reload", szOutput);
         bUpdatedImageParams = true;
      }
      else if ( (s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_CAMERA_PARAMS )
         hardware_camera_apply_all_majestic_camera_settings(pCameraParams, true);
      else
      {
         hardware_camera_apply_all_majestic_settings(g_pCurrentModel, pCameraParams, g_pCurrentModel->video_params.user_selected_video_link_profile, pVideoParams);
         g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs = g_pCurrentModel->getInitialKeyframeIntervalMs(g_pCurrentModel->video_params.user_selected_video_link_profile);
         g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs = g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs;
      }

      if ( bUpdatedImageParams )
      {
         s_bHasPendingMajesticRealTimeChanges = true;
         s_uTimeLastMajesticImageRealTimeUpdate = g_TimeNow;
      }

      if ( ((s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_VIDEO_RESOLUTION) ||
           ((s_uRequestedVideoMajesticCaptureUpdateReason & 0xFF) == MODEL_CHANGED_VIDEO_CODEC) )
      {
         hardware_sleep_ms(50);
         video_source_majestic_stop_capture_program();
         hardware_sleep_ms(50);
         video_source_majestic_stop_capture_program();
         hardware_sleep_ms(50);
         video_source_majestic_start_capture_program();
         vehicle_check_update_processes_affinities(false, false);
      }
      s_uRequestedVideoMajesticCaptureUpdateReason = 0;
   }
}