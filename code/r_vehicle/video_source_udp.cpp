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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/ruby_ipc.h"
#include "../base/utils.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <getopt.h>
#include <poll.h>

#include "video_source_udp.h"
#include "events.h"
#include "timers.h"
#include "shared_vars.h"

int s_fInputVideoStreamUDPSocket = -1;
int s_iInputVideoStreamUDPPort = 5600;

u8 s_uInputVideoUDPBuffer[MAX_PACKET_TOTAL_SIZE];
u8 s_uOutputUDPNALFrameSegment[MAX_PACKET_TOTAL_SIZE];

u32 s_uDebugTimeLastUDPVideoInputCheck = 0;
u32 s_uDebugUDPInputBytes = 0;
u32 s_uDebugUDPInputReads = 0;

void video_source_udp_close()
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

int video_source_udp_open(int iUDPPort)
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
   log_line("[VideoSourceUDP] Opened read socket on port %d for reading video stream. socket fd = %d", s_iInputVideoStreamUDPPort, s_fInputVideoStreamUDPSocket);
   
   return s_fInputVideoStreamUDPSocket;
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

// Returns the buffer and number of bytes read
u8* video_source_udp_read(int* piReadSize, bool bAsync)
{
   if ( (NULL == piReadSize) )
      return NULL;

   *piReadSize = 0;
   if ( -1 == s_fInputVideoStreamUDPSocket )
      return NULL;

   int nRecv = 0;
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
         return NULL;
      }
      if ( 0 == res )
         return NULL;

      if (fds[0].revents & (POLLERR | POLLNVAL))
      {
         log_softerror_and_alarm("[VideoSourceUDP] Socket error pooling: %s", strerror(errno));
         return NULL;
      }
      if ( ! (fds[0].revents & POLLIN) )
         return NULL;
    
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

      nRecv = recvmsg(s_fInputVideoStreamUDPSocket, &msghdr, 0);
      if ( nRecv < 0 )
      {
         log_softerror_and_alarm("[VideoSourceUDP] Failed to recvmsg from UDP socket, error: %s", strerror(errno));
         return NULL;
      }

      if ( nRecv > MAX_PACKET_TOTAL_SIZE )
      {
         log_softerror_and_alarm("[VideoSourceUDP] Read too much data %d bytes from UDP socket", nRecv);
         nRecv = MAX_PACKET_TOTAL_SIZE;
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
         return NULL;
      }
      if ( iSelectResult == 0 )
         return NULL;

      if( 0 == FD_ISSET(s_fInputVideoStreamUDPSocket, &readset) )
         return NULL;

      struct sockaddr_in client_addr;
      socklen_t len = sizeof(client_addr);
      
      memset(&client_addr, 0, sizeof(client_addr));
      client_addr.sin_family = AF_INET;
      client_addr.sin_addr.s_addr = INADDR_ANY;
      client_addr.sin_port = htons( s_iInputVideoStreamUDPPort );

      nRecv = recvfrom(s_fInputVideoStreamUDPSocket, s_uInputVideoUDPBuffer, sizeof(s_uInputVideoUDPBuffer)/sizeof(s_uInputVideoUDPBuffer[0]), 
                MSG_WAITALL, ( struct sockaddr *) &client_addr,
                &len);
      //int nRecv = recv(socket_server, szBuff, 1024, )
      if ( nRecv < 0 )
      {
         log_error_and_alarm("[VideoSourceUDP] Failed to receive from UDP socket.");
         return NULL;
      }
      if ( nRecv == 0 )
         return NULL;
   }
   s_uDebugUDPInputBytes += nRecv;
   s_uDebugUDPInputReads++;

   // Parse RTP frame, extract NAL frame or fragment

   if ( nRecv <= 12 )
      return NULL;

   static u16 s_uLastRTLSeqNumberInUDPFrame = 0;
   
   u16 uRTPSeqNb = (((u16)s_uInputVideoUDPBuffer[2]) << 8) | s_uInputVideoUDPBuffer[3];

   if ( 0 != s_uLastRTLSeqNumberInUDPFrame )
   if ( (s_uLastRTLSeqNumberInUDPFrame + 1) != uRTPSeqNb )
      log_softerror_and_alarm("[VideoSourceUDP] Read skipped RTP frames, from seqnb %d to seqnb %d", s_uLastRTLSeqNumberInUDPFrame, uRTPSeqNb);
   s_uLastRTLSeqNumberInUDPFrame = uRTPSeqNb;

   u8 uNALHeaderByte = s_uInputVideoUDPBuffer[12];
   //u8 uFUIndicator = s_uInputVideoUDPBuffer[12];
   u8 uFUHeaderByte = s_uInputVideoUDPBuffer[13];

   u8 uNALStartPrefix[4];
   uNALStartPrefix[0] = 0;
   uNALStartPrefix[1] = 0;
   uNALStartPrefix[2] = 0;
   uNALStartPrefix[3] = 0x01;

   u8 uNALType = uNALHeaderByte & 0x1F;
   u8 uFUStartBit = (uFUHeaderByte >> 7) & 0x01;
   u8 uFUEndBit = (uFUHeaderByte>>6) & 0x01;

   int iOutputSize = 0;

   if ( uNALType < 24 )
   {
      memcpy(s_uOutputUDPNALFrameSegment, uNALStartPrefix, 4);
      memcpy(&s_uOutputUDPNALFrameSegment[4], &s_uInputVideoUDPBuffer[12], nRecv-12);
      iOutputSize = nRecv - 12 + 4;
   }
   else if ( uFUStartBit )
   {
      uNALType = 0;
      uNALType = 0x60 | (uFUHeaderByte & 0x1F);
      memcpy(s_uOutputUDPNALFrameSegment, uNALStartPrefix, 4);
      memcpy(&s_uOutputUDPNALFrameSegment[4], &uNALType, 1);
      memcpy(&s_uOutputUDPNALFrameSegment[5], &s_uInputVideoUDPBuffer[14], nRecv-14);
      iOutputSize = nRecv - 14 + 5;      
   }
   else if ( uFUEndBit )
   {
      memcpy(s_uOutputUDPNALFrameSegment, &s_uInputVideoUDPBuffer[14], nRecv-14);
      iOutputSize = nRecv - 14;
   }
   else
   {
      memcpy(s_uOutputUDPNALFrameSegment, &s_uInputVideoUDPBuffer[14], nRecv-14);
      iOutputSize = nRecv - 14;
   }

   *piReadSize = iOutputSize;
   return s_uOutputUDPNALFrameSegment;
}

void video_source_udp_periodic_checks()
{
   if ( g_TimeNow >= s_uDebugTimeLastUDPVideoInputCheck+1000 )
   {
      log_line("[VideoSourceUDP] Input video data: %u bytes/sec, %u bps, %u reads/sec",
         s_uDebugUDPInputBytes, s_uDebugUDPInputBytes*8, s_uDebugUDPInputReads);
      s_uDebugTimeLastUDPVideoInputCheck = g_TimeNow;
      s_uDebugUDPInputBytes = 0;
      s_uDebugUDPInputReads = 0;
   }
}