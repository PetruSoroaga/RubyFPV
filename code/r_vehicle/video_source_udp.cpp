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

#include "video_source_udp.h"
#include "events.h"
#include "timers.h"
#include "shared_vars.h"

int s_fInputVideoStreamUDP = -1;
int s_iInputVideoStreamUDPPort = 5600;

u8 s_uInputVideoUDPBuffer[MAX_PACKET_TOTAL_SIZE];

u32 s_uDebugTimeLastUDPVideoInputCheck = 0;
u32 s_uDebugUDPInputBytes = 0;
u32 s_uDebugUDPInputReads = 0;

void video_source_udp_close()
{
   if ( -1 != s_fInputVideoStreamUDP )
   {
      log_line("[VideoSourceUDP] Closed input UDP socket.");
      close(s_fInputVideoStreamUDP);
   }
   else
      log_line("[VideoSourceUDP] No input UDP socket to close.");
   s_fInputVideoStreamUDP = -1;
}

int video_source_udp_open(int iUDPPort)
{
   if ( -1 != s_fInputVideoStreamUDP )
      return s_fInputVideoStreamUDP;

   s_iInputVideoStreamUDPPort = iUDPPort;

   struct sockaddr_in server_addr;
   s_fInputVideoStreamUDP = socket(AF_INET, SOCK_DGRAM, 0);
   if (s_fInputVideoStreamUDP == -1)
   {
      log_softerror_and_alarm("[VideoSourceUDP] Can't create socket for video stream on port %d.", s_iInputVideoStreamUDPPort);
      return -1;
   }

   const int optval = 1;
   if ( 0 != setsockopt(s_fInputVideoStreamUDP, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(optval)) )
       log_softerror_and_alarm("[VideoSourceUDP] Failed to set SO_REUSEADDR: %s", strerror(errno));

   if ( 0 != setsockopt(s_fInputVideoStreamUDP, SOL_SOCKET, SO_RXQ_OVFL, (const void *)&optval , sizeof(optval)) )
       log_softerror_and_alarm("[VideoSourceUDP] Unable to set SO_RXQ_OVFL: %s", strerror(errno));

   int iRecvSize = MAX_PACKET_TOTAL_SIZE;
   if( 0 != setsockopt(s_fInputVideoStreamUDP, SOL_SOCKET, SO_RCVBUF, (const void *)&iRecvSize , sizeof(iRecvSize)))
      log_softerror_and_alarm("[VideoSourceUDP] Unable to set SO_RCVBUF: %s", strerror(errno));

   memset(&server_addr, 0, sizeof(server_addr));
   server_addr.sin_family = AF_INET;
   server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   server_addr.sin_port = htons((unsigned short)s_iInputVideoStreamUDPPort );
 
   if( bind(s_fInputVideoStreamUDP,(struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
   {
      log_error_and_alarm("[VideoSourceUDP] Failed to bind socket for video stream to port %d.", s_iInputVideoStreamUDPPort);
      close(s_fInputVideoStreamUDP);
      s_fInputVideoStreamUDP = -1;
      return -1;
   }
   log_line("[VideoSourceUDP] Opened read socket on port %d for reading video stream. fd = %d", s_iInputVideoStreamUDPPort, s_fInputVideoStreamUDP);
   
   return s_fInputVideoStreamUDP;
}


// Extract SO_RXQ_OVFL counter
uint32_t extract_rxq_overflow(struct msghdr *msg)
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

   /*
   struct sockaddr_in client_addr;
   memset(&client_addr, 0, sizeof(client_addr));
   client_addr.sin_family = AF_INET;
   client_addr.sin_addr.s_addr = INADDR_ANY;
   client_addr.sin_port = htons( 5600 );
   socklen_t len = sizeof(client_addr);
   iRead = recvfrom(s_fInputVideoStream, pBufferToReadInto, iBufferMaxRead, 
                MSG_WAITALL, ( struct sockaddr *) &client_addr,
                &len);
   if ( iRead < 0 )
   {
      log_error_and_alarm("Failed to read from video input socket, socket fd: %d, returned code: %d, error: %s", s_fInputVideoStream, iRead, strerror(errno));
      return -1;
   }
   */

/*
   static uint8_t bufvideoin[MAX_PACKET_TOTAL_SIZE + 1];
   static ssize_t rsize = 0;
   static uint8_t cmsgbuf[CMSG_SPACE(sizeof(uint32_t))];

   static int nfds = 1;
   static struct pollfd fds[2];
   static bool s_bFirstVideoReadSetup = true;

   static u32 suTimeLastDebug = 0;
   static u32 suTotalBytesRead = 0;
   static u32 suTotalReads = 0;

   if ( s_bFirstVideoReadSetup )
   {
      s_bFirstVideoReadSetup = false;
      memset(fds, '\0', sizeof(fds));
      if ( fcntl(s_fInputVideoStream, F_SETFL, fcntl(s_fInputVideoStream, F_GETFL, 0) | O_NONBLOCK) < 0 )
         log_softerror_and_alarm("Unable to set socket into nonblocked mode: %s", strerror(errno));

      fds[0].fd = s_fInputVideoStream;
      fds[0].events = POLLIN;

      log_line("Done first time video read setup.");
   }

   if ( rsize == 0 )
   {
      int res = poll(fds, nfds, 1);
      if ( res < 0 )
         log_error_and_alarm("Failed to poll video read file.");

      if ( 0 == res )
         return 0;

      if (fds[0].revents & (POLLERR | POLLNVAL))
         log_softerror_and_alarm("socket error: %s", strerror(errno));

      if (fds[0].revents & POLLIN)
      {
         struct iovec iov = { .iov_base = (void*)bufvideoin,
                                            .iov_len = sizeof(bufvideoin) };

         struct msghdr msghdr = { .msg_name = NULL,
                                  .msg_namelen = 0,
                                  .msg_iov = &iov,
                                  .msg_iovlen = 1,
                                  .msg_control = &cmsgbuf,
                                  .msg_controllen = sizeof(cmsgbuf),
                                  .msg_flags = 0 };

         memset(cmsgbuf, '\0', sizeof(cmsgbuf));

         rsize = recvmsg(s_fInputVideoStream, &msghdr, 0);
         if ( rsize < 0 )
         {
            log_softerror_and_alarm("Failed to recvmsg, error: %s", strerror(errno));
            return -1;
         }

         if ( rsize > MAX_PACKET_TOTAL_SIZE )
         {
            log_softerror_and_alarm("Read too much data %d bytes", rsize);
            rsize = MAX_PACKET_TOTAL_SIZE;
         }

         static uint32_t rxq_overflow = 0;
         uint32_t cur_rxq_overflow = extract_rxq_overflow(&msghdr);
         if (cur_rxq_overflow != rxq_overflow)
         {
             log_softerror_and_alarm("UDP rxq overflow: %u packets dropped (from %u to %u)", cur_rxq_overflow - rxq_overflow, rxq_overflow, cur_rxq_overflow);
             rxq_overflow = cur_rxq_overflow;
         }
         suTotalReads++;
         suTotalBytesRead += rsize;
         //log_line("read %d bytes", (int)rsize);
         if ( g_TimeNow >= suTimeLastDebug + 1000 )
         {
            log_line("video read: %u bytes/sec, %u bps, %u pack/sec", suTotalBytesRead, suTotalBytesRead*8, suTotalReads);
            suTimeLastDebug = g_TimeNow;
            suTotalBytesRead = 0;
            suTotalReads = 0;
         }
      }
   }
   else
   {
      if ( rsize <= iBufferMaxRead )
      {
         memcpy(pBufferToReadInto, bufvideoin, rsize);
         iRead = rsize;
         rsize = 0;
      }
      else
      {
         memcpy(pBufferToReadInto, bufvideoin, iBufferMaxRead);
         iRead = iBufferMaxRead;

         for( int i=0; i<rsize - iBufferMaxRead; i++ )
            bufvideoin[i] = bufvideoin[i+iBufferMaxRead];

         rsize -= iBufferMaxRead;
      }
   }
*/

// Returns the buffer and number of bytes read
u8* video_source_udp_read(int* piReadSize)
{
   if ( (NULL == piReadSize) )
      return NULL;

   *piReadSize = 0;
   if ( -1 == s_fInputVideoStreamUDP )
      return NULL;

   fd_set readset;
   FD_ZERO(&readset);
   FD_SET(s_fInputVideoStreamUDP, &readset);

   struct timeval timePipeInput;
   timePipeInput.tv_sec = 0;
   timePipeInput.tv_usec = 5*1000; // 5 miliseconds timeout

   int iSelectResult = select(s_fInputVideoStreamUDP+1, &readset, NULL, NULL, &timePipeInput);
   if ( iSelectResult < 0 )
   {
      log_error_and_alarm("[VideoSourceUDP] Failed to select socket.");
      return NULL;
   }
   if ( iSelectResult == 0 )
      return NULL;

   if( 0 == FD_ISSET(s_fInputVideoStreamUDP, &readset) )
      return NULL;

   struct sockaddr_in client_addr;
   socklen_t len = sizeof(client_addr);
   
   memset(&client_addr, 0, sizeof(client_addr));
   client_addr.sin_family = AF_INET;
   client_addr.sin_addr.s_addr = INADDR_ANY;
   client_addr.sin_port = htons( s_iInputVideoStreamUDPPort );

   int nRecv = recvfrom(s_fInputVideoStreamUDP, s_uInputVideoUDPBuffer, sizeof(s_uInputVideoUDPBuffer)/sizeof(s_uInputVideoUDPBuffer[0]), 
             MSG_WAITALL, ( struct sockaddr *) &client_addr,
             &len);
   //int nRecv = recv(socket_server, szBuff, 1024, )
   if ( nRecv < 0 )
   {
      log_error_and_alarm("[VideoSourceUDP] Failed to receive from client.");
      return NULL;
   }
   if ( nRecv == 0 )
      return NULL;

   s_uDebugUDPInputBytes += nRecv;
   s_uDebugUDPInputReads++;

   *piReadSize = nRecv;
   return s_uInputVideoUDPBuffer;
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