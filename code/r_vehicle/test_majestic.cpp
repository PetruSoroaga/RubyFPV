/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <getopt.h>
#include <poll.h>

#include "test_majestic.h"
#include "../base/base.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/parser_h264.h"
#include "../base/hardware_cam_maj.h"
#include "../common/string_utils.h"
#include "shared_vars.h"
#include "timers.h"
#include "video_source_majestic.h"

#ifdef NONE

u32 s_uPrevToken = 0x11111111;
u32 s_uCurrentToken = 0x11111111;
u32 s_uTimeLastNAL = 0;
u32 s_uNALSize = 0;

void _parse_stream(unsigned char* pBuffer, int iLength)
{
   while ( iLength > 0 )
   {
      s_uPrevToken = (s_uPrevToken << 8) | (s_uCurrentToken & 0xFF);
      s_uCurrentToken = (s_uCurrentToken << 8) | (*pBuffer);
      pBuffer++;
      iLength--;
      s_uNALSize++;
      if ( s_uPrevToken == 0x00000001 )
      {
         unsigned char uNALType = s_uCurrentToken & 0b11111;
         u32 uTime = get_current_timestamp_ms();
         log_line("NAL: %d, prev %u bytes, %u ms from last", uNALType, s_uNALSize, uTime - s_uTimeLastNAL);
         s_uTimeLastNAL = uTime;
         s_uNALSize = 0;
      }
   } 
}

static FILE* fpAudio = NULL;
static FILE* fpRTPAudio = NULL;
static FILE* fpRTPAll = NULL;
static int iAudioSize = 0;

void _parse_rtp_data(u8* pData, int iLength)
{
   //_parse_stream(pData, iLength);
      
   if ( iLength < 12 )
   {
      log_softerror_and_alarm("Invalid RTP header, too small, %d bytes", iLength);
      return;
   }

   log_line("RTP: v:%d p:%d ext:%d cc:%d mark:%d type: %d seq-nb:%u src-id:%u, len: %d, audio: %d, %d bps",
      pData[0] >> 6, (pData[0]>>5) & 0x01, (pData[0]>>4) & 0x01, pData[0] & 0x0F,
      (pData[1] & 0x80)?1:0, pData[1] & 0x7F,
      (pData[2] << 8) | pData[3],
      (pData[8]<<24) | (pData[9]<<16) | (pData[10]<<8) | pData[11],
      iLength, iAudioSize, iAudioSize*8);


  if ( NULL != fpAudio )
  if ( (pData[1] & 0x7F) == 100 )
  {
     fwrite(pData+12, 1, iLength-12, fpAudio);
     iAudioSize += iLength-12;

     //fwrite(pData, 1, iLength, fpRTPAudio);
  }

 //fwrite(pData, 1, iLength, fpRTPAll);

}


void test_majestic()
{
   log_enable_full();
   log_enable_stdout();

   loadAllModels();
   g_pCurrentModel = getCurrentModel();

   hardware_camera_maj_init();
   video_source_majestic_start_and_configure();
   int iUDPSocket = video_source_majestic_open(MAJESTIC_UDP_PORT);

   fpAudio = fopen("/tmp/audio.pcm", "wb");
   //fpRTPAudio = fopen("/tmp/rtpaudio.rtp", "wb");
   //fpRTPAll = fopen("/tmp/rtpall.h264", "wb");

   log_line("Waiting for packets...");
   while ( ! g_bQuit )
   {
      int iLength = 0;
      u8* pData = video_source_majestic_raw_read(&iLength, true);
      if ( (iLength > 0) && (NULL != pData) )
         _parse_rtp_data(pData, iLength);
   }

   fclose(fpAudio);
   //fclose(fpRTPAudio);
   //fclose(fpRTPAll);
   if ( -1 != iUDPSocket )
      close(iUDPSocket);
}

#endif
void test_majestic()
{
}