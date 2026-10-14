/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

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
#include "../base/hardware_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/parser_h264.h"
#include "../base/utils.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <poll.h>
#include <sched.h>

#include "video_source_udp.h"
#include "video_sources.h"
#include "video_tx_buffers.h"
#include "events.h"
#include "timers.h"
#include "shared_vars.h"
#include "packets_utils.h"
#include "adaptive_video.h"
#include "ruby_rt_vehicle.h"

// Video source for an external encoder (ie. a companion computer) that sends
// H264/H265 as RTP over UDP on a regular network link (Ethernet/USB-Ethernet).
// The video data path is identical to the one used for OpenIPC cameras;
// the encoder controls (bitrate, keyframe, QP delta, fps, IDR requests) are sent
// back to the encoder as one line JSON messages on a separate UDP control port.

#define VIDEO_SOURCE_UDP_MAX_CONTROL_MSG_LEN 256
#define VIDEO_SOURCE_UDP_NO_DATA_REOPEN_TIMEOUT_MS 10000

static int s_iVideoSourceUDPSocket = -1;
static int s_iVideoSourceUDPControlSocket = -1;
static bool s_bVideoSourceUDPSocketSetupNonBlocking = false;

static int s_iVideoSourceUDPVideoPort = IP_CAMERA_DEFAULT_VIDEO_UDP_PORT;
static int s_iVideoSourceUDPControlPort = IP_CAMERA_DEFAULT_CONTROL_UDP_PORT;
static char s_szVideoSourceUDPRemoteIP[32];
static bool s_bVideoSourceUDPRemoteIPIsFixed = false;
static bool s_bVideoSourceUDPRemoteIPIsKnown = false;
static bool s_bVideoSourceUDPMustReapplyParams = false;
static bool s_bVideoSourceUDPConfigLoaded = false;

static u16 s_uLastRTPSeqNumberUDP[256];
static u32 s_uCountSkippedRTPFramesUDP = 0;

static bool s_bLogStartOfInputVideoDataUDP = true;

static u8 s_uInputVideoUDPBufferIP[MAX_PACKET_TOTAL_SIZE];
static u8 s_uOutputUDPNALFrameSegmentIP[MAX_PACKET_TOTAL_SIZE+10];

static u32 s_uDebugTimeLastUDPVideoInputCheckIP = 0;
static u32 s_uDebugUDPInputBytesIP = 0;
static u32 s_uDebugUDPInputReadsIP = 0;

static u32 s_uLastNALTypeIP = 0;
static bool s_bLastReadIsSingleNALIP = false;
static bool s_bLastReadIsEndNALIP = false;
static bool s_bLastReadIsStartNALIP = false;

static u32 s_uTimeLastUDPVideoRecvData = 0;
static u32 s_uTimeUDPVideoSourceStarted = 0;
static u32 s_uTimeLastUDPControlMessageSent = 0;
static u32 s_uTimeLastUDPControlPongReceived = 0;
static u32 s_uTimeLastUDPVideoAlarmSent = 0;
static u32 s_uTimeLastUDPVideoSocketReopen = 0;
static u32 s_uTimeLastUDPPingSent = 0;

static u32 s_uLastVideoSourceUDPReadTimestamps[5];
static u32 s_uLastAlarmUDPOveflowTimestampIP = 0;

static u32 s_uCurrentSetBitrateBPS = 0;
static int s_iCurrentSetKeyframeMs = 0;
static int s_iCurrentSetQPDelta = -1000;

static void _video_source_udp_load_config()
{
   if ( s_bVideoSourceUDPConfigLoaded )
      return;
   s_bVideoSourceUDPConfigLoaded = true;

   s_iVideoSourceUDPVideoPort = IP_CAMERA_DEFAULT_VIDEO_UDP_PORT;
   s_iVideoSourceUDPControlPort = IP_CAMERA_DEFAULT_CONTROL_UDP_PORT;
   s_szVideoSourceUDPRemoteIP[0] = 0;
   s_bVideoSourceUDPRemoteIPIsFixed = false;

   // Same lookup as camera detection: regular config folder first, boot partition as fallback
   char szFile[MAX_FILE_PATH_SIZE];
   if ( ! hardware_camera_get_ip_camera_config_file(szFile) )
   {
      log_line("[VideoSourceUDP] No config file found. Using defaults: video port %d, control port %d, remote IP: auto detected.",
         s_iVideoSourceUDPVideoPort, s_iVideoSourceUDPControlPort);
      return;
   }

   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[VideoSourceUDP] Can't open config file (%s): %s. Using defaults: video port %d, control port %d.",
         szFile, strerror(errno), s_iVideoSourceUDPVideoPort, s_iVideoSourceUDPControlPort);
      return;
   }

   char szLine[256];
   while ( NULL != fgets(szLine, sizeof(szLine)/sizeof(szLine[0]), fd) )
   {
      removeTrailingNewLines(szLine);
      if ( (0 == szLine[0]) || ('#' == szLine[0]) )
         continue;
      char* pValue = strchr(szLine, '=');
      if ( NULL == pValue )
         continue;
      *pValue = 0;
      pValue++;
      while ( (*pValue == ' ') || (*pValue == 9) )
         pValue++;

      if ( NULL != strstr(szLine, "video_port") )
      {
         int iPort = atoi(pValue);
         if ( (iPort > 0) && (iPort < 65536) )
            s_iVideoSourceUDPVideoPort = iPort;
      }
      else if ( NULL != strstr(szLine, "control_port") )
      {
         int iPort = atoi(pValue);
         if ( (iPort > 0) && (iPort < 65536) )
            s_iVideoSourceUDPControlPort = iPort;
      }
      else if ( NULL != strstr(szLine, "remote_ip") )
      {
         if ( 0 != pValue[0] )
         {
            strncpy(s_szVideoSourceUDPRemoteIP, pValue, sizeof(s_szVideoSourceUDPRemoteIP)/sizeof(s_szVideoSourceUDPRemoteIP[0]) - 1);
            s_szVideoSourceUDPRemoteIP[sizeof(s_szVideoSourceUDPRemoteIP)/sizeof(s_szVideoSourceUDPRemoteIP[0]) - 1] = 0;
            s_bVideoSourceUDPRemoteIPIsFixed = true;
            s_bVideoSourceUDPRemoteIPIsKnown = true;
         }
      }
   }
   fclose(fd);

   log_line("[VideoSourceUDP] Loaded config from %s: video port: %d, control port: %d, remote encoder IP: %s",
      szFile, s_iVideoSourceUDPVideoPort, s_iVideoSourceUDPControlPort,
      s_bVideoSourceUDPRemoteIPIsFixed?s_szVideoSourceUDPRemoteIP:"auto detected from video stream");
}

static int _video_source_udp_open_video_socket()
{
   if ( -1 != s_iVideoSourceUDPSocket )
      return s_iVideoSourceUDPSocket;

   for( int i=0; i<256; i++ )
      s_uLastRTPSeqNumberUDP[i] = 0;

   s_bVideoSourceUDPSocketSetupNonBlocking = false;

   struct sockaddr_in server_addr;
   s_iVideoSourceUDPSocket = socket(AF_INET, SOCK_DGRAM, 0);
   if ( s_iVideoSourceUDPSocket == -1 )
   {
      log_softerror_and_alarm("[VideoSourceUDP] Can't create socket for video stream on port %d.", s_iVideoSourceUDPVideoPort);
      return -1;
   }

   const int optval = 1;
   if ( 0 != setsockopt(s_iVideoSourceUDPSocket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval)) )
      log_softerror_and_alarm("[VideoSourceUDP] Failed to set SO_REUSEADDR: %s", strerror(errno));

   if ( 0 != setsockopt(s_iVideoSourceUDPSocket, SOL_SOCKET, SO_RXQ_OVFL, (const void *)&optval, sizeof(optval)) )
      log_softerror_and_alarm("[VideoSourceUDP] Unable to set SO_RXQ_OVFL: %s", strerror(errno));

   int iRecvSize = 0;
   socklen_t iParamLen = sizeof(iRecvSize);
   getsockopt(s_iVideoSourceUDPSocket, SOL_SOCKET, SO_RCVBUF, &iRecvSize, &iParamLen);
   log_line("[VideoSourceUDP] Current socket recv buffer size: %d", iRecvSize);

   int iWantedRecvSize = 512*1024;
   if ( 0 != setsockopt(s_iVideoSourceUDPSocket, SOL_SOCKET, SO_RCVBUF, (const void *)&iWantedRecvSize, sizeof(iWantedRecvSize)) )
      log_softerror_and_alarm("[VideoSourceUDP] Unable to set SO_RCVBUF: %s", strerror(errno));

   iRecvSize = 0;
   iParamLen = sizeof(iRecvSize);
   getsockopt(s_iVideoSourceUDPSocket, SOL_SOCKET, SO_RCVBUF, &iRecvSize, &iParamLen);
   log_line("[VideoSourceUDP] New current socket recv buffer size: %d (requested: %d)", iRecvSize, iWantedRecvSize);

   memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   server_addr.sin_port = htons((unsigned short)s_iVideoSourceUDPVideoPort);

   if ( bind(s_iVideoSourceUDPSocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
   {
      log_error_and_alarm("[VideoSourceUDP] Failed to bind socket for video stream to port %d.", s_iVideoSourceUDPVideoPort);
      close(s_iVideoSourceUDPSocket);
      s_iVideoSourceUDPSocket = -1;
      return -1;
   }

   log_line("[VideoSourceUDP] Opened read socket on port %d for reading video stream. socket fd = %d", s_iVideoSourceUDPVideoPort, s_iVideoSourceUDPSocket);
   return s_iVideoSourceUDPSocket;
}

static int _video_source_udp_open_control_socket()
{
   if ( -1 != s_iVideoSourceUDPControlSocket )
      return s_iVideoSourceUDPControlSocket;

   s_iVideoSourceUDPControlSocket = socket(AF_INET, SOCK_DGRAM, 0);
   if ( s_iVideoSourceUDPControlSocket == -1 )
   {
      log_softerror_and_alarm("[VideoSourceUDP] Can't create control socket on port %d.", s_iVideoSourceUDPControlPort);
      return -1;
   }

   const int optval = 1;
   if ( 0 != setsockopt(s_iVideoSourceUDPControlSocket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(optval)) )
      log_softerror_and_alarm("[VideoSourceUDP] Failed to set SO_REUSEADDR on control socket: %s", strerror(errno));

   if ( fcntl(s_iVideoSourceUDPControlSocket, F_SETFL, fcntl(s_iVideoSourceUDPControlSocket, F_GETFL, 0) | O_NONBLOCK) < 0 )
      log_softerror_and_alarm("[VideoSourceUDP] Unable to set control socket into nonblocking mode: %s", strerror(errno));

   struct sockaddr_in local_addr;
   memset(&local_addr, 0, sizeof(local_addr));
   local_addr.sin_family = AF_INET;
   local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   local_addr.sin_port = htons((unsigned short)s_iVideoSourceUDPControlPort);

   if ( bind(s_iVideoSourceUDPControlSocket, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0 )
   {
      // Not fatal: control messages can still be sent from an ephemeral port,
      // only the replies from the remote encoder would not be received.
      log_softerror_and_alarm("[VideoSourceUDP] Failed to bind control socket to port %d: %s. Control replies will not be received.",
         s_iVideoSourceUDPControlPort, strerror(errno));
   }

   log_line("[VideoSourceUDP] Opened control socket on port %d. socket fd = %d", s_iVideoSourceUDPControlPort, s_iVideoSourceUDPControlSocket);
   return s_iVideoSourceUDPControlSocket;
}

bool video_source_udp_send_control(const char* szJSONLine)
{
   if ( (NULL == szJSONLine) || (0 == szJSONLine[0]) )
      return false;

   if ( -1 == s_iVideoSourceUDPControlSocket )
      _video_source_udp_open_control_socket();
   if ( -1 == s_iVideoSourceUDPControlSocket )
      return false;

   if ( ! s_bVideoSourceUDPRemoteIPIsKnown )
   {
      static u32 s_uTimeLastLogNoRemoteIP = 0;
      if ( g_TimeNow > s_uTimeLastLogNoRemoteIP + 5000 )
      {
         s_uTimeLastLogNoRemoteIP = g_TimeNow;
         log_line("[VideoSourceUDP] Can't send control message (%s): remote encoder address is not known yet.", szJSONLine);
      }
      return false;
   }

   struct sockaddr_in remote_addr;
   memset(&remote_addr, 0, sizeof(remote_addr));
   remote_addr.sin_family = AF_INET;
   remote_addr.sin_port = htons((unsigned short)s_iVideoSourceUDPControlPort);
   if ( 0 == inet_aton(s_szVideoSourceUDPRemoteIP, &remote_addr.sin_addr) )
   {
      log_softerror_and_alarm("[VideoSourceUDP] Invalid remote encoder IP address: [%s]", s_szVideoSourceUDPRemoteIP);
      return false;
   }

   int iLen = (int)strlen(szJSONLine);
   int iRes = sendto(s_iVideoSourceUDPControlSocket, szJSONLine, iLen, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
   if ( iRes != iLen )
   {
      log_softerror_and_alarm("[VideoSourceUDP] Failed to send control message (%s) to %s:%d: %s",
         szJSONLine, s_szVideoSourceUDPRemoteIP, s_iVideoSourceUDPControlPort, strerror(errno));
      return false;
   }
   s_uTimeLastUDPControlMessageSent = g_TimeNow;
   log_line("[VideoSourceUDP] Sent control message to %s:%d : %s", s_szVideoSourceUDPRemoteIP, s_iVideoSourceUDPControlPort, szJSONLine);
   return true;
}

static void _video_source_udp_read_control_replies()
{
   if ( -1 == s_iVideoSourceUDPControlSocket )
      return;

   char szBuff[VIDEO_SOURCE_UDP_MAX_CONTROL_MSG_LEN];
   for( int i=0; i<10; i++ )
   {
      int iRecv = recv(s_iVideoSourceUDPControlSocket, szBuff, sizeof(szBuff)/sizeof(szBuff[0]) - 1, MSG_DONTWAIT);
      if ( iRecv <= 0 )
         break;
      szBuff[iRecv] = 0;
      if ( NULL != strstr(szBuff, "pong") )
         s_uTimeLastUDPControlPongReceived = g_TimeNow;
      else
         log_line("[VideoSourceUDP] Received control reply: %s", szBuff);
   }
}

void video_source_udp_set_video_bitrate(u32 uVideoBitrateBPS, int iIPQDelta)
{
   char szBuff[VIDEO_SOURCE_UDP_MAX_CONTROL_MSG_LEN];

   if ( uVideoBitrateBPS != s_uCurrentSetBitrateBPS )
   {
      s_uCurrentSetBitrateBPS = uVideoBitrateBPS;
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "{\"bitrate_bps\":%u}", uVideoBitrateBPS);
      video_source_udp_send_control(szBuff);
   }

   if ( (iIPQDelta > -100) && (iIPQDelta != s_iCurrentSetQPDelta) )
   {
      s_iCurrentSetQPDelta = iIPQDelta;
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "{\"qp_delta\":%d}", iIPQDelta);
      video_source_udp_send_control(szBuff);
   }
}

void video_source_udp_set_keyframe(int iKeyframeMs)
{
   if ( iKeyframeMs < 0 )
      iKeyframeMs = -iKeyframeMs;
   if ( iKeyframeMs == s_iCurrentSetKeyframeMs )
      return;
   s_iCurrentSetKeyframeMs = iKeyframeMs;

   char szBuff[VIDEO_SOURCE_UDP_MAX_CONTROL_MSG_LEN];
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "{\"keyframe_ms\":%d}", iKeyframeMs);
   video_source_udp_send_control(szBuff);
}

void video_source_udp_request_idr()
{
   video_source_udp_send_control("{\"request_idr\":1}");
}

u32 video_source_udp_get_current_bitrate()
{
   return s_uCurrentSetBitrateBPS;
}

int video_source_udp_get_current_keyframe()
{
   return s_iCurrentSetKeyframeMs;
}

int video_source_udp_get_current_qpdelta()
{
   return s_iCurrentSetQPDelta;
}

void video_source_udp_apply_all_parameters()
{
   for( int i=0; i<(int)(sizeof(s_uLastVideoSourceUDPReadTimestamps)/sizeof(s_uLastVideoSourceUDPReadTimestamps[0])); i++ )
      s_uLastVideoSourceUDPReadTimestamps[i] = 0;

   if ( NULL == g_pCurrentModel )
      return;

   signal_start_long_op();

   int iVideoProfile = g_pCurrentModel->video_params.iCurrentVideoProfile;
   char szBuff[VIDEO_SOURCE_UDP_MAX_CONTROL_MSG_LEN];

   // Send everything again, unconditionally: the remote encoder might have been restarted.
   s_uCurrentSetBitrateBPS = g_pCurrentModel->video_link_profiles[iVideoProfile].uTargetVideoBitrateBPS;
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "{\"bitrate_bps\":%u}", s_uCurrentSetBitrateBPS);
   video_source_udp_send_control(szBuff);

   s_iCurrentSetKeyframeMs = g_pCurrentModel->getInitialKeyframeIntervalMs(iVideoProfile);
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "{\"keyframe_ms\":%d}", s_iCurrentSetKeyframeMs);
   video_source_udp_send_control(szBuff);

   s_iCurrentSetQPDelta = g_pCurrentModel->video_link_profiles[iVideoProfile].iIPQuantizationDelta;
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "{\"qp_delta\":%d}", s_iCurrentSetQPDelta);
   video_source_udp_send_control(szBuff);

   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "{\"fps\":%d}", g_pCurrentModel->video_params.iVideoFPS);
   video_source_udp_send_control(szBuff);

   video_source_udp_request_idr();

   video_source_udp_clear_input_buffers();

   signal_end_long_op();
   log_line("[VideoSourceUDP] Applied all parameters: bitrate: %.2f Mbps, keyframe: %d ms, QPDelta: %d, fps: %d",
      (float)s_uCurrentSetBitrateBPS/1000.0/1000.0, s_iCurrentSetKeyframeMs, s_iCurrentSetQPDelta, g_pCurrentModel->video_params.iVideoFPS);
}

// Returns initial set video bitrate
u32 video_source_udp_start_program(u32 uOverwriteInitialBitrate, int iOverwriteInitialKFMs, int iOverwriteInitialQPDelta, int* pInitialKFSet)
{
   s_bVideoSourceUDPConfigLoaded = false;
   _video_source_udp_load_config();

   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("[VideoSourceUDP] Start program: no current model.");
      return 0;
   }

   int iVideoProfile = g_pCurrentModel->video_params.iCurrentVideoProfile;

   u32 uInitialVideoBitrate = uOverwriteInitialBitrate;
   if ( 0 == uInitialVideoBitrate )
      uInitialVideoBitrate = g_pCurrentModel->video_link_profiles[iVideoProfile].uTargetVideoBitrateBPS;

   int iInitialKeyframeMs = iOverwriteInitialKFMs;
   if ( iInitialKeyframeMs <= 0 )
      iInitialKeyframeMs = g_pCurrentModel->getInitialKeyframeIntervalMs(iVideoProfile);

   int iInitialQPDelta = iOverwriteInitialQPDelta;
   if ( iInitialQPDelta < -100 )
      iInitialQPDelta = g_pCurrentModel->video_link_profiles[iVideoProfile].iIPQuantizationDelta;

   log_line("[VideoSourceUDP] Start program: no local capture process is started for an IP camera.");
   log_line("[VideoSourceUDP] Start program: initial bitrate: %.3f Mbps, initial keyframe: %d ms, initial QPDelta: %d",
      (float)uInitialVideoBitrate/1000.0/1000.0, iInitialKeyframeMs, iInitialQPDelta);

   // Reset them, so that they are all sent again on the control channel
   s_uCurrentSetBitrateBPS = 0;
   s_iCurrentSetKeyframeMs = 0;
   s_iCurrentSetQPDelta = -1000;

   _video_source_udp_open_video_socket();
   _video_source_udp_open_control_socket();
   video_source_udp_clear_input_buffers();

   video_source_udp_set_video_bitrate(uInitialVideoBitrate, iInitialQPDelta);
   video_source_udp_set_keyframe(iInitialKeyframeMs);

   char szBuff[VIDEO_SOURCE_UDP_MAX_CONTROL_MSG_LEN];
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "{\"fps\":%d}", g_pCurrentModel->video_params.iVideoFPS);
   video_source_udp_send_control(szBuff);
   video_source_udp_request_idr();

   // If the remote address is not known yet, resend everything as soon as it's detected
   if ( ! s_bVideoSourceUDPRemoteIPIsKnown )
      s_bVideoSourceUDPMustReapplyParams = true;

   s_bLogStartOfInputVideoDataUDP = true;
   s_uTimeUDPVideoSourceStarted = g_TimeNow;
   s_uTimeLastUDPVideoRecvData = g_TimeNow;
   s_uCountSkippedRTPFramesUDP = 0;

   if ( NULL != pInitialKFSet )
      *pInitialKFSet = iInitialKeyframeMs;

   log_line("[VideoSourceUDP] Start program: completed. Initial video bitrate: %.3f Mbps", (float)uInitialVideoBitrate/1000.0/1000.0);
   return uInitialVideoBitrate;
}

void video_source_udp_stop_program()
{
   if ( -1 != s_iVideoSourceUDPSocket )
   {
      log_line("[VideoSourceUDP] Stop program: closed input UDP socket.");
      close(s_iVideoSourceUDPSocket);
   }
   else
      log_line("[VideoSourceUDP] Stop program: no input UDP socket to close.");
   s_iVideoSourceUDPSocket = -1;

   if ( -1 != s_iVideoSourceUDPControlSocket )
   {
      log_line("[VideoSourceUDP] Stop program: closed control UDP socket.");
      close(s_iVideoSourceUDPControlSocket);
   }
   s_iVideoSourceUDPControlSocket = -1;

   s_uTimeUDPVideoSourceStarted = 0;
   log_line("[VideoSourceUDP] Stopped program.");
}

u32 video_source_udp_get_program_start_time()
{
   return s_uTimeUDPVideoSourceStarted;
}

static u32 _video_source_udp_extract_rxq_overflow(struct msghdr *msg)
{
   struct cmsghdr *cmsg;
   u32 uRet = 0;

   for( cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg) )
   {
      if ( (cmsg->cmsg_level == SOL_SOCKET) && (cmsg->cmsg_type == SO_RXQ_OVFL) )
      {
         memcpy(&uRet, CMSG_DATA(cmsg), sizeof(uRet));
         return uRet;
      }
   }
   return 0;
}

static void _video_source_udp_on_received_from(struct sockaddr_in* pFromAddr)
{
   if ( (NULL == pFromAddr) || s_bVideoSourceUDPRemoteIPIsFixed )
      return;
   if ( 0 == pFromAddr->sin_addr.s_addr )
      return;

   char szIP[32];
   szIP[0] = 0;
   if ( NULL == inet_ntop(AF_INET, &(pFromAddr->sin_addr), szIP, sizeof(szIP)/sizeof(szIP[0])) )
      return;
   if ( 0 == szIP[0] )
      return;
   if ( 0 == strcmp(szIP, s_szVideoSourceUDPRemoteIP) )
      return;

   log_line("[VideoSourceUDP] Detected remote encoder address: %s (was: %s)", szIP,
      (0 == s_szVideoSourceUDPRemoteIP[0])?"none":s_szVideoSourceUDPRemoteIP);
   strcpy(s_szVideoSourceUDPRemoteIP, szIP);
   s_bVideoSourceUDPRemoteIPIsKnown = true;
   s_bVideoSourceUDPMustReapplyParams = true;
}

static int _video_source_udp_try_read_input_udp_data(bool bAsync)
{
   if ( -1 == s_iVideoSourceUDPSocket )
      return -1;

   int nRecvBytes = 0;
   struct sockaddr_in fromAddr;
   memset(&fromAddr, 0, sizeof(fromAddr));

   if ( bAsync )
   {
      if ( ! s_bVideoSourceUDPSocketSetupNonBlocking )
      {
         s_bVideoSourceUDPSocketSetupNonBlocking = true;
         if ( fcntl(s_iVideoSourceUDPSocket, F_SETFL, fcntl(s_iVideoSourceUDPSocket, F_GETFL, 0) | O_NONBLOCK) < 0 )
            log_softerror_and_alarm("[VideoSourceUDP] Unable to set socket into nonblocking mode: %s", strerror(errno));
         else
            log_line("[VideoSourceUDP] Done first time video UDP socket setup (nonblocking).");
      }

      static uint8_t s_uCMsgBuffUDPVideo[CMSG_SPACE(sizeof(uint32_t))];

      fd_set fdSet;
      FD_ZERO(&fdSet);
      FD_SET(s_iVideoSourceUDPSocket, &fdSet);
      struct timeval timeWait;
      timeWait.tv_sec = 0;
      timeWait.tv_usec = 100;
      int res = select(s_iVideoSourceUDPSocket+1, &fdSet, NULL, NULL, &timeWait);
      if ( res < 0 )
      {
         log_error_and_alarm("[VideoSourceUDP] Failed to poll UDP socket.");
         return -1;
      }
      if ( 0 == res )
         return 0;
      if ( 0 == FD_ISSET(s_iVideoSourceUDPSocket, &fdSet) )
         return 0;

      struct iovec iov = { .iov_base = (void*)s_uInputVideoUDPBufferIP,
                           .iov_len = sizeof(s_uInputVideoUDPBufferIP) };
      struct msghdr msghdr = { .msg_name = (void*)&fromAddr,
                               .msg_namelen = sizeof(fromAddr),
                               .msg_iov = &iov,
                               .msg_iovlen = 1,
                               .msg_control = &s_uCMsgBuffUDPVideo,
                               .msg_controllen = sizeof(s_uCMsgBuffUDPVideo),
                               .msg_flags = 0 };

      memset(s_uCMsgBuffUDPVideo, 0, sizeof(s_uCMsgBuffUDPVideo));

      nRecvBytes = recvmsg(s_iVideoSourceUDPSocket, &msghdr, 0);
      if ( nRecvBytes < 0 )
      {
         log_softerror_and_alarm("[VideoSourceUDP] Failed to recvmsg from UDP socket, error: %s", strerror(errno));
         return -1;
      }

      for( int i=4; i>0; i-- )
         s_uLastVideoSourceUDPReadTimestamps[i] = s_uLastVideoSourceUDPReadTimestamps[i-1];
      s_uLastVideoSourceUDPReadTimestamps[0] = g_TimeNow;

      if ( nRecvBytes > MAX_PACKET_TOTAL_SIZE )
      {
         log_softerror_and_alarm("[VideoSourceUDP] Read too much data %d bytes from UDP socket", nRecvBytes);
         nRecvBytes = MAX_PACKET_TOTAL_SIZE;
      }

      static u32 s_uRxqOverflowUDPVideo = 0;
      static int s_iRxUDPOverflowCounterVideo = 0;
      u32 uCurrentRxqOverflow = _video_source_udp_extract_rxq_overflow(&msghdr);
      if ( uCurrentRxqOverflow != s_uRxqOverflowUDPVideo )
      {
         u32 uDroppedCount = uCurrentRxqOverflow - s_uRxqOverflowUDPVideo;
         log_softerror_and_alarm("[VideoSourceUDP] UDP rxq overflow: %u packets dropped (from %u to %u), overflow counter: %d",
            uDroppedCount, s_uRxqOverflowUDPVideo, uCurrentRxqOverflow, s_iRxUDPOverflowCounterVideo);
         s_iRxUDPOverflowCounterVideo++;

         if ( uCurrentRxqOverflow > s_uRxqOverflowUDPVideo + 1 )
         if ( g_TimeNow > s_uLastAlarmUDPOveflowTimestampIP + 10000 )
         if ( g_TimeNow > g_TimeStart + 10000 )
         {
            s_uLastAlarmUDPOveflowTimestampIP = g_TimeNow;
            u32 uFlags2 = 0;
            u32 uDelta = s_uLastVideoSourceUDPReadTimestamps[0] - s_uLastVideoSourceUDPReadTimestamps[1];
            if ( uDelta > 255 )
               uDelta = 255;
            uFlags2 |= uDelta & 0xFF;
            uDelta = s_uLastVideoSourceUDPReadTimestamps[1] - s_uLastVideoSourceUDPReadTimestamps[2];
            if ( uDelta > 255 )
               uDelta = 255;
            uFlags2 |= (uDelta & 0xFF) << 8;
            uDelta = s_uLastVideoSourceUDPReadTimestamps[2] - s_uLastVideoSourceUDPReadTimestamps[3];
            if ( uDelta > 255 )
               uDelta = 255;
            uFlags2 |= (uDelta & 0xFF) << 16;

            send_alarm_to_controller(ALARM_ID_DEVELOPER_ALARM, ALARM_FLAG_DEVELOPER_ALARM_UDP_SKIPPED | ((uDroppedCount & 0xFF) << 8), uFlags2, 5);
         }

         if ( s_iRxUDPOverflowCounterVideo > 10 )
         {
            log_softerror_and_alarm("[VideoSourceUDP] Too many UDP overflows: Reopen UDP port...");
            if ( -1 != s_iVideoSourceUDPSocket )
               close(s_iVideoSourceUDPSocket);
            s_iVideoSourceUDPSocket = -1;
            _video_source_udp_open_video_socket();
            s_iRxUDPOverflowCounterVideo = 0;
            nRecvBytes = 0;
            log_softerror_and_alarm("[VideoSourceUDP] Too many UDP overflows: Reopened UDP port.");
         }
         s_uRxqOverflowUDPVideo = uCurrentRxqOverflow;
      }
      else
         s_iRxUDPOverflowCounterVideo = 0;
   }
   else
   {
      fd_set readset;
      FD_ZERO(&readset);
      FD_SET(s_iVideoSourceUDPSocket, &readset);

      struct timeval timeInput;
      timeInput.tv_sec = 0;
      timeInput.tv_usec = 5*1000; // 5 miliseconds timeout

      int iSelectResult = select(s_iVideoSourceUDPSocket+1, &readset, NULL, NULL, &timeInput);
      if ( iSelectResult < 0 )
      {
         log_error_and_alarm("[VideoSourceUDP] Failed to select socket.");
         return -1;
      }
      if ( 0 == iSelectResult )
         return 0;
      if ( 0 == FD_ISSET(s_iVideoSourceUDPSocket, &readset) )
         return 0;

      socklen_t len = sizeof(fromAddr);
      nRecvBytes = recvfrom(s_iVideoSourceUDPSocket, s_uInputVideoUDPBufferIP, sizeof(s_uInputVideoUDPBufferIP)/sizeof(s_uInputVideoUDPBufferIP[0]),
                MSG_WAITALL, (struct sockaddr *)&fromAddr, &len);
      if ( nRecvBytes < 0 )
      {
         log_error_and_alarm("[VideoSourceUDP] Failed to receive from UDP socket.");
         return -1;
      }
      if ( 0 == nRecvBytes )
         return 0;
   }

   if ( nRecvBytes > 0 )
      _video_source_udp_on_received_from(&fromAddr);
   return nRecvBytes;
}

// Parse input raw bytes and returns a NAL packet (in s_uOutputUDPNALFrameSegmentIP)
static int _video_source_udp_parse_rtp_data(u8* pInputRawData, int iInputBytes)
{
   s_bLastReadIsSingleNALIP = false;
   s_bLastReadIsEndNALIP = false;
   s_bLastReadIsStartNALIP = false;

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
   bool bHasPadding = ((pInputRawData[0]>>5) & 0x01)?true:false;
   u8  uRTPPacketType = (pInputRawData[1] & 0x7F);
   u16 uRTPSeqNb = (((u16)pInputRawData[2]) << 8) | pInputRawData[3];
   int iPaddingBytes = pInputRawData[iInputBytes-1];

   if ( (0 != s_uLastRTPSeqNumberUDP[uRTPPacketType]) && (65535 != s_uLastRTPSeqNumberUDP[uRTPPacketType]) )
   if ( (s_uLastRTPSeqNumberUDP[uRTPPacketType] + 1) != uRTPSeqNb )
   {
      s_uCountSkippedRTPFramesUDP++;
      log_softerror_and_alarm("[VideoSourceUDP] Read skipped RTP frames type %d, from seqnb %d to seqnb %d",
         uRTPPacketType, s_uLastRTPSeqNumberUDP[uRTPPacketType], uRTPSeqNb);
   }
   s_uLastRTPSeqNumberUDP[uRTPPacketType] = uRTPSeqNb;
   // End - Check RTP sequence number
   // ----------------------------------------------

   pInputRawData += iRTPHeaderLength;
   iInputBytes -= iRTPHeaderLength;

   if ( iInputBytes < 1 )
   {
      log_softerror_and_alarm("[VideoSourceUDP] Received invalid RTP size (12 bytes header only)");
      return 0;
   }

   u8 uFragmentTypeH264 = pInputRawData[0] & 0x1F;
   u8 uFragmentTypeH265 = (pInputRawData[0]>>1) & 0x3F;

   static u8 uNALOutputHeader[6];
   int iNALOutputHeaderSize = 4;

   // Single compact (non-fragmented, non I/P frame (slice)) NAL unit (ie. SPS, PPS, etc)
   if ( (uFragmentTypeH264 != 28) && (uFragmentTypeH265 != 49) )
   {
      uNALOutputHeader[0] = 0;
      uNALOutputHeader[1] = 0;
      uNALOutputHeader[2] = 0;
      uNALOutputHeader[3] = 0x01;

      s_bLastReadIsSingleNALIP = true;
      s_bLastReadIsEndNALIP = true;
      s_bLastReadIsStartNALIP = true;

      // H264 frame type: lower 5 bits (&0x1F): 5 - Iframe, 1 - Pframe
      s_uLastNALTypeIP = pInputRawData[0] & 0x1F;

      memcpy(s_uOutputUDPNALFrameSegmentIP, uNALOutputHeader, iNALOutputHeaderSize);
      memcpy(&s_uOutputUDPNALFrameSegmentIP[iNALOutputHeaderSize], pInputRawData, iInputBytes);
      int iOutputSize = iInputBytes + iNALOutputHeaderSize;
      if ( bHasPadding )
         iOutputSize -= iPaddingBytes;
      return iOutputSize;
   }

   // Fragmentation unit (fragmented NAL over multiple packets)

   u8 uFUStartBit = 0;
   u8 uFUEndBit = 0;

   if ( uFragmentTypeH264 == 28 )
   {
      if ( iInputBytes < 2 )
      {
         log_softerror_and_alarm("[VideoSourceUDP] Received invalid RTP size (1 byte only for 28 fragment)");
         return 0;
      }
      // H264 fragment
      uFUStartBit = pInputRawData[1] & 0x80;
      uFUEndBit = pInputRawData[1] & 0x40;
      uNALOutputHeader[4] = (pInputRawData[0] & 0xE0) | (pInputRawData[1] & 0x1F);
      // H264 frame type: lower 5 bits (& 0x1F) of uNALOutputHeader[4]: 5 - Iframe, 1 - Pframe
      s_uLastNALTypeIP = uNALOutputHeader[4] & 0x1F;

      iNALOutputHeaderSize++;
      pInputRawData++;
      iInputBytes--;
      pInputRawData++;
      iInputBytes--;
   }
   else
   {
      if ( iInputBytes < 3 )
      {
         log_softerror_and_alarm("[VideoSourceUDP] Received invalid RTP size (2 bytes only for 49 fragment)");
         return 0;
      }
      // H265 fragment
      uFUStartBit = pInputRawData[2] & 0x80;
      uFUEndBit = pInputRawData[2] & 0x40;

      uNALOutputHeader[4] = (pInputRawData[0] & 0x81) | (pInputRawData[2] & 0x3F) << 1;
      uNALOutputHeader[5] = 1;
      // (uNALOutputHeader[4] >> 1) & 0x3F   ->  19: Iframe;  1: Pframe
      s_uLastNALTypeIP = (uNALOutputHeader[4] >> 1) & 0x3F;

      iNALOutputHeaderSize++;
      iNALOutputHeaderSize++;
      pInputRawData++;
      iInputBytes--;
      pInputRawData++;
      iInputBytes--;
      pInputRawData++;
      iInputBytes--;
   }

   if ( uFUStartBit )
      s_bLastReadIsStartNALIP = true;
   if ( uFUEndBit )
      s_bLastReadIsEndNALIP = true;

   if ( uFUStartBit )
   {
      uNALOutputHeader[0] = 0;
      uNALOutputHeader[1] = 0;
      uNALOutputHeader[2] = 0;
      uNALOutputHeader[3] = 0x01;
      memcpy(s_uOutputUDPNALFrameSegmentIP, uNALOutputHeader, iNALOutputHeaderSize);
      memcpy(&s_uOutputUDPNALFrameSegmentIP[iNALOutputHeaderSize], pInputRawData, iInputBytes);
      int iOutputSize = iInputBytes + iNALOutputHeaderSize;
      if ( bHasPadding )
         iOutputSize -= iPaddingBytes;
      return iOutputSize;
   }

   memcpy(s_uOutputUDPNALFrameSegmentIP, pInputRawData, iInputBytes);
   if ( bHasPadding )
      iInputBytes -= iPaddingBytes;
   return iInputBytes;
}

// Returns the buffer and number of bytes read
u8* video_source_udp_read(int* piReadSize, bool bAsync, u32* puOutTimeDataAvailable)
{
   if ( NULL == piReadSize )
      return NULL;

   *piReadSize = 0;

   int iRecvBytes = _video_source_udp_try_read_input_udp_data(bAsync);
   if ( iRecvBytes <= 0 )
      return NULL;

   if ( NULL != puOutTimeDataAvailable )
   {
      g_TimeNow = get_current_timestamp_ms();
      *puOutTimeDataAvailable = g_TimeNow;
   }

   if ( s_bLogStartOfInputVideoDataUDP )
   {
      log_line("[VideoSourceUDP] Start receiving data (H264/H265 stream) from IP camera at %s",
         s_bVideoSourceUDPRemoteIPIsKnown?s_szVideoSourceUDPRemoteIP:"unknown address");
      s_bLogStartOfInputVideoDataUDP = false;
   }

   s_uTimeLastUDPVideoRecvData = g_TimeNow;
   s_uDebugUDPInputBytesIP += iRecvBytes;
   s_uDebugUDPInputReadsIP++;

   int iOutputBytes = _video_source_udp_parse_rtp_data(s_uInputVideoUDPBufferIP, iRecvBytes);

   *piReadSize = iOutputBytes;
   if ( iOutputBytes <= 0 )
      return NULL;
   return s_uOutputUDPNALFrameSegmentIP;
}

int video_source_udp_get_audio_data(u8* pOutputBuffer, int iMaxToRead)
{
   // Audio over the IP video link is not supported
   return 0;
}

void video_source_udp_clear_audio_buffers()
{
   // Audio over the IP video link is not supported
}

void video_source_udp_clear_input_buffers()
{
   log_line("[VideoSourceUDP] Clearing input buffers...");
   int iCount = 0;
   int iBytes = 0;
   for( int i=0; i<1000; i++ )
   {
      int iReadSize = 0;
      u8* pVideoData = video_source_udp_read(&iReadSize, true, NULL);
      if ( NULL == pVideoData )
         break;
      iCount++;
      iBytes += iReadSize;
   }
   log_line("[VideoSourceUDP] Done clearing input buffers. Cleared %d packets, total %d bytes.", iCount, iBytes);
}

bool video_source_udp_last_read_is_single_nal()
{
   return s_bLastReadIsSingleNALIP;
}

bool video_source_udp_last_read_is_start_nal()
{
   return s_bLastReadIsStartNALIP;
}

bool video_source_udp_last_read_is_end_nal()
{
   return s_bLastReadIsEndNALIP;
}

u32 video_source_udp_get_last_nal_type()
{
   return s_uLastNALTypeIP;
}

// Returns true if a full restart of the vehicle is needed.
// There is no local capture process for an IP camera, so it never requests a restart.
bool video_source_udp_periodic_health_checks()
{
   if ( g_TimeNow >= s_uDebugTimeLastUDPVideoInputCheckIP + 10000 )
   {
      char szBitrate[64];
      str_format_bitrate(s_uDebugUDPInputBytesIP/10*8, szBitrate);

      char szPong[64];
      if ( 0 == s_uTimeLastUDPControlPongReceived )
         strcpy(szPong, "never");
      else
         sprintf(szPong, "%u ms ago", g_TimeNow - s_uTimeLastUDPControlPongReceived);

      log_line("[VideoSourceUDP] Input video data: %u bytes/sec, %s, %u reads/sec, skipped RTP packets: %u, remote encoder: %s, last reply from it: %s",
         s_uDebugUDPInputBytesIP/10, szBitrate, s_uDebugUDPInputReadsIP/10, s_uCountSkippedRTPFramesUDP,
         s_bVideoSourceUDPRemoteIPIsKnown?s_szVideoSourceUDPRemoteIP:"unknown", szPong);
      s_uCountSkippedRTPFramesUDP = 0;
      s_uDebugTimeLastUDPVideoInputCheckIP = g_TimeNow;
      s_uDebugUDPInputBytesIP = 0;
      s_uDebugUDPInputReadsIP = 0;
   }

   _video_source_udp_read_control_replies();

   if ( g_bVideoPaused )
      return false;

   // Remote encoder address was just detected: (re)send all the parameters to it
   if ( s_bVideoSourceUDPMustReapplyParams && s_bVideoSourceUDPRemoteIPIsKnown )
   {
      s_bVideoSourceUDPMustReapplyParams = false;
      log_line("[VideoSourceUDP] Remote encoder address is now known. Resending all parameters to it.");
      video_source_udp_apply_all_parameters();
      return false;
   }

   // Ping the remote encoder from time to time
   if ( s_bVideoSourceUDPRemoteIPIsKnown )
   if ( g_TimeNow > s_uTimeLastUDPPingSent + 5000 )
   {
      s_uTimeLastUDPPingSent = g_TimeNow;
      char szBuff[VIDEO_SOURCE_UDP_MAX_CONTROL_MSG_LEN];
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "{\"ping\":%u}", g_TimeNow);
      video_source_udp_send_control(szBuff);
   }

   if ( 0 == s_uTimeUDPVideoSourceStarted )
      return false;
   if ( g_TimeNow < s_uTimeUDPVideoSourceStarted + 3000 )
      return false;

   // No video data from the remote encoder?
   if ( g_TimeNow > s_uTimeLastUDPVideoRecvData + 3000 )
   {
      if ( g_TimeNow > s_uTimeLastUDPVideoAlarmSent + 5000 )
      {
         s_uTimeLastUDPVideoAlarmSent = g_TimeNow;
         log_softerror_and_alarm("[VideoSourceUDP] No video stream received from the IP camera for %u ms.", g_TimeNow - s_uTimeLastUDPVideoRecvData);
         send_alarm_to_controller(ALARM_ID_VIDEO_CAPTURE_MALFUNCTION, 0, 0, 5);
         video_source_udp_request_idr();
      }

      // Reopen the input socket from time to time, in case the network interface went down and up
      if ( g_TimeNow > s_uTimeLastUDPVideoRecvData + VIDEO_SOURCE_UDP_NO_DATA_REOPEN_TIMEOUT_MS )
      if ( g_TimeNow > s_uTimeLastUDPVideoSocketReopen + VIDEO_SOURCE_UDP_NO_DATA_REOPEN_TIMEOUT_MS )
      {
         s_uTimeLastUDPVideoSocketReopen = g_TimeNow;
         log_softerror_and_alarm("[VideoSourceUDP] Reopening the input UDP socket after %u ms without any video data.", g_TimeNow - s_uTimeLastUDPVideoRecvData);
         if ( -1 != s_iVideoSourceUDPSocket )
            close(s_iVideoSourceUDPSocket);
         s_iVideoSourceUDPSocket = -1;
         _video_source_udp_open_video_socket();
         s_bLogStartOfInputVideoDataUDP = true;
         // Remote encoder might have been restarted too: push all the parameters again
         s_bVideoSourceUDPMustReapplyParams = true;
      }
      return false;
   }

   static u32 s_uTimeLastOverflowCheckUDPIP = 0;
   if ( g_TimeNow > s_uTimeLastOverflowCheckUDPIP + 500 )
   if ( g_TimeNow > s_uTimeLastUDPControlMessageSent + 500 )
   if ( NULL != g_pVideoTxBuffers )
   {
      s_uTimeLastOverflowCheckUDPIP = g_TimeNow;
      if ( g_pVideoTxBuffers->getResetOverflowFlag() )
      {
         log_softerror_and_alarm("[VideoSourceUDP] Detected inconsistency in packets sizes. Max raw usable video in tx buffers now: %d bytes",
            g_pVideoTxBuffers->getCurrentMaxUsableRawVideoDataSize());
         video_source_udp_clear_input_buffers();
      }
   }

   return false;
}
