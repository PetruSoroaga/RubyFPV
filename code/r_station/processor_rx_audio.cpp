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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hw_procs.h"
#include "../base/hardware_audio.h"
#include "processor_rx_audio.h"
#include <pthread.h>

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/fec.h" 
#include "packets_utils.h"

#include "shared_vars.h"
#include "timers.h"

bool s_bAudioProcessingStarted = false;
bool s_bHasAudioOutputDevice = false;
int s_fPipeAudioPlayerOutput = -1;
int s_fPipeAudioPlayerQueueWrite = -1;
int s_fPipeAudioPlayerQueueRead = -1;
int s_fPipeAudioBufferWrite = -1;
int s_fPipeAudioBufferRead = -1;

u32 s_uCurrentRxAudioBlockIndex = MAX_U32;

int s_iAudioDataPacketsPerBlock = DEFAULT_AUDIO_P_DATA;
int s_iAudioECPacketsPerBlock = DEFAULT_AUDIO_P_EC;
int s_iAudioPacketSize = 0;

u8 s_BufferAudioPackets[MAX_BUFFERED_AUDIO_PACKETS][MAX_PACKET_PAYLOAD];
bool s_BufferAudioPacketsReceived[MAX_BUFFERED_AUDIO_PACKETS];
u32 s_uReceivedDataPacketsForCurrentBlock = 0;
u32 s_uReceivedECPacketsForCurrentBlock = 0;

u32 s_uLastRecvAudioBlockIndex = MAX_U32;
u32 s_uLastRecvAudioBlockPacketIndex = MAX_U32;

u32 s_uLastOutputedBlockIndex = MAX_U32;
u32 s_uLastOutputedPacketIndex = MAX_U32;

unsigned int fec_decode_audio_missing_packets[MAX_TOTAL_PACKETS_IN_BLOCK];
unsigned int fec_decode_audio_fec_indexes[MAX_TOTAL_PACKETS_IN_BLOCK];
u8* fec_decode_audio_data_packets[MAX_TOTAL_PACKETS_IN_BLOCK];
u8* fec_decode_audio_fec_packets[MAX_TOTAL_PACKETS_IN_BLOCK];
unsigned int missing_audio_packets_count_for_fec = 0;

int s_iAudioRecordingSegment = 0;
FILE* s_pFileAudioRecording = NULL;
int s_iTokenPositionToCheck = 0;
char s_szAudioToken[24];

FILE* s_pFileRawStreamOutput = NULL;
pthread_t s_ThreadAudioBuffering;
bool s_bThreadAudioBufferingStarted = false;
bool s_bStopThreadAudioBuffering = false;

pthread_t s_ThreadAudioQueueing;
bool s_bThreadAudioQueueingStarted = false;
bool s_bStopThreadAudioQueueing = false;

u8  s_uAudioBufferedPackets[MAX_BUFFERED_AUDIO_PACKETS][MAX_PACKET_PAYLOAD];
int s_iAudioBufferedPacketsSizes[MAX_BUFFERED_AUDIO_PACKETS];
int s_iAudioBufferWritePos = 0;
int s_iAudioBufferReadPos = 0;
int s_iAudioBufferPacketsToCache = DEFAULT_AUDIO_BUFFERING_SIZE;



void* _thread_audio_queueing_playback(void *argument)
{
   s_bThreadAudioQueueingStarted = true;
   log_line("[AudioRx-ThdQue] Created audio playback queue thread.");
   int iCountReads = 0;
   fd_set readSet;
   fd_set exceSet;
   struct timeval timePipeInput;
   u8 uBuffer[2048];

   while ( ! g_bQuit )
   {
      if ( s_fPipeAudioPlayerQueueRead <= 0 )
      {
         hardware_sleep_ms(10);
         continue;
      }
      int iReadSize = 0;
      
      FD_ZERO(&readSet);
      FD_SET(s_fPipeAudioPlayerQueueRead, &readSet);
      FD_ZERO(&exceSet);
      FD_SET(s_fPipeAudioPlayerQueueRead, &exceSet);
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 20*1000; // 20 miliseconds timeout

      int iSelResult = select(s_fPipeAudioPlayerQueueRead+1, &readSet, NULL, &exceSet, &timePipeInput);
      if ( iSelResult <= 0 )
         continue;
      if( FD_ISSET(s_fPipeAudioPlayerQueueRead, &exceSet) || g_bQuit )
      {
         log_line("[AudioRx-ThdQue] Exception on read. Exit");
         break;
      }

      iReadSize = read(s_fPipeAudioPlayerQueueRead, uBuffer, s_iAudioPacketSize);

      if ( 0 == iCountReads )
         log_line("[AudioRx-ThdQue] Start reading input data (%d bytes)", iReadSize);
      iCountReads++;

      if ( iReadSize <= 0 )
      {
         if ( s_bStopThreadAudioQueueing )
            break;
         hardware_sleep_ms(10);
         continue;
      }
      if ( s_bStopThreadAudioQueueing )
         break;

      if ( s_fPipeAudioPlayerOutput > 0 )
      {
         int iRes = write(s_fPipeAudioPlayerOutput, uBuffer, iReadSize);
         if ( iRes < 0 )
         {
            log_line("[AudioRx-ThdQue] Failed to write to audio player pipe.");
            break;
         }
      }
      else
         hardware_sleep_ms(1);
   }
   log_line("[AudioRx-ThdQue] Finished audio playback queue thread.");
   s_bThreadAudioQueueingStarted = false;
   return NULL;
}


void* _thread_audio_buffering_playback(void *argument)
{
   s_bThreadAudioBufferingStarted = true;
   log_line("[AudioRx-ThdBuf] Created audio playback buffering thread.");
   fd_set readSet;
   fd_set exceSet;
   struct timeval timePipeInput;

   while ( ! g_bQuit )
   {
      if ( s_fPipeAudioBufferRead <= 0 )
      {
         hardware_sleep_ms(10);
         continue;
      }
      int iReadSize = 0;
      
      FD_ZERO(&readSet);
      FD_SET(s_fPipeAudioBufferRead, &readSet);
      FD_ZERO(&exceSet);
      FD_SET(s_fPipeAudioBufferRead, &exceSet);
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 20*1000; // 20 miliseconds timeout

      int iSelResult = select(s_fPipeAudioBufferRead+1, &readSet, NULL, &exceSet, &timePipeInput);
      if ( iSelResult <= 0 )
         continue;
      if( FD_ISSET(s_fPipeAudioBufferRead, &exceSet) )
      {
         log_line("[AudioRx-ThdBuf] Exception on pipe read. Exit thread.");
         break;
      }

      iReadSize = read(s_fPipeAudioBufferRead, &s_uAudioBufferedPackets[s_iAudioBufferWritePos][0], s_iAudioPacketSize);

      if ( iReadSize <= 0 )
      {
         if ( s_bStopThreadAudioBuffering )
            break;
         hardware_sleep_ms(10);
         continue;
      }
      if ( s_bStopThreadAudioBuffering )
         break;

      s_iAudioBufferedPacketsSizes[s_iAudioBufferWritePos] = iReadSize;
      s_iAudioBufferWritePos++;
      if ( s_iAudioBufferWritePos >= MAX_BUFFERED_AUDIO_PACKETS )
         s_iAudioBufferWritePos = 0;
      s_iAudioBufferedPacketsSizes[s_iAudioBufferWritePos] = 0;

      if ( s_iAudioBufferReadPos == s_iAudioBufferWritePos )
         continue;

      int iDiff = s_iAudioBufferWritePos - s_iAudioBufferReadPos;
      if ( s_iAudioBufferWritePos < s_iAudioBufferReadPos )
         iDiff = MAX_BUFFERED_AUDIO_PACKETS - s_iAudioBufferReadPos + s_iAudioBufferWritePos;

      if ( iDiff < s_iAudioBufferPacketsToCache )
         continue;

      while ( iDiff > s_iAudioBufferPacketsToCache )
      {
         if ( s_fPipeAudioPlayerQueueWrite > 0 )
         {
            int iRes = write(s_fPipeAudioPlayerQueueWrite, &s_uAudioBufferedPackets[s_iAudioBufferReadPos][0], s_iAudioBufferedPacketsSizes[s_iAudioBufferReadPos]);
            if ( iRes < 0 )
            {
               log_line("[AudioRx-ThdBuf] Failed to write to audio player pipe.");
               break;
            }
         }
         else
            hardware_sleep_ms(1);

         s_iAudioBufferReadPos++;
         if ( s_iAudioBufferReadPos >= MAX_BUFFERED_AUDIO_PACKETS )
            s_iAudioBufferReadPos = 0;
         iDiff--;
      }
   }
   log_line("[AudioRx-ThdBuf] Finished audio playback buffering thread.");
   s_bThreadAudioBufferingStarted = false;
   return NULL;
}

void _open_audio_pipes()
{
   log_line("[AudioRx] Opening audio pipe player write endpoint: %s", FIFO_RUBY_AUDIO1);
   int iRetries = 20;
   while ( (s_fPipeAudioPlayerOutput <= 0) && (iRetries > 0) )
   {
      iRetries--;
      //s_fPipeAudioPlayerOutput = open(FIFO_RUBY_AUDIO1, O_WRONLY);
      s_fPipeAudioPlayerOutput = open(FIFO_RUBY_AUDIO1, O_WRONLY | O_NONBLOCK);
      if ( s_fPipeAudioPlayerOutput > 0 )
         break;
      log_softerror_and_alarm("[AudioRx] Failed to open audio pipe player write endpoint: %s, retry...", FIFO_RUBY_AUDIO1);
      hardware_sleep_ms(20);
   }
   if ( s_fPipeAudioPlayerOutput > 0 )
      log_line("[AudioRx] Opened successfully audio pipe player write endpoint: %s", FIFO_RUBY_AUDIO1);
   else
   {
      log_error_and_alarm("[AudioRx] Failed to open audio pipe player write endpoint: %s", FIFO_RUBY_AUDIO1);
      return;
   }
   log_line("[AudioRx] Player pipe FIFO default size: %d bytes", fcntl(s_fPipeAudioPlayerOutput, F_GETPIPE_SZ));
   
   s_fPipeAudioPlayerQueueRead = open(FIFO_RUBY_AUDIO_QUEUE, O_CREAT | O_RDONLY | O_NONBLOCK);
   if ( s_fPipeAudioPlayerQueueRead <= 0 )
   {
      log_error_and_alarm("[AudioRx] Failed to open audio pipe queue read endpoint: %s",FIFO_RUBY_AUDIO_QUEUE);
      return;
   }
   log_line("[AudioRx] Opened successfully audio pipe queue read endpoint: %s", FIFO_RUBY_AUDIO_QUEUE);

   s_fPipeAudioPlayerQueueWrite = open(FIFO_RUBY_AUDIO_QUEUE, O_CREAT | O_WRONLY | O_NONBLOCK);
   if ( s_fPipeAudioPlayerQueueWrite <= 0 )
   {
      log_error_and_alarm("[AudioRx] Failed to open audio pipe queue write endpoint: %s",FIFO_RUBY_AUDIO_QUEUE);
      return;
   }
   log_line("[AudioRx] Opened successfully audio pipe queue write endpoint: %s", FIFO_RUBY_AUDIO_QUEUE);

   s_fPipeAudioBufferRead = open(FIFO_RUBY_AUDIO_BUFF, O_CREAT | O_RDONLY | O_NONBLOCK);
   if ( s_fPipeAudioBufferRead <= 0 )
   {
      log_error_and_alarm("[AudioRx] Failed to open audio pipe buffer read endpoint: %s",FIFO_RUBY_AUDIO_BUFF);
      return;
   }
   log_line("[AudioRx] Opened successfully audio pipe buffer read endpoint: %s", FIFO_RUBY_AUDIO_BUFF);

   s_fPipeAudioBufferWrite = open(FIFO_RUBY_AUDIO_BUFF, O_CREAT | O_WRONLY | O_NONBLOCK);
   if ( s_fPipeAudioBufferWrite <= 0 )
   {
      log_error_and_alarm("[AudioRx] Failed to open audio pipe buffer write endpoint: %s",FIFO_RUBY_AUDIO_BUFF);
      return;
   }
   log_line("[AudioRx] Opened successfully audio pipe buffer write endpoint: %s", FIFO_RUBY_AUDIO_BUFF);

   log_line("[AudioRx] Player queue FIFO default size: %d bytes", fcntl(s_fPipeAudioPlayerQueueWrite, F_GETPIPE_SZ));
   fcntl(s_fPipeAudioPlayerQueueWrite, F_SETPIPE_SZ, 250000);
   log_line("[AudioRx] Player queue FIFO new size: %d bytes", fcntl(s_fPipeAudioPlayerQueueWrite, F_GETPIPE_SZ));
   log_line("[AudioRx] Player buff FIFO default size: %d bytes", fcntl(s_fPipeAudioBufferWrite, F_GETPIPE_SZ));
   fcntl(s_fPipeAudioBufferWrite, F_SETPIPE_SZ, 250000);
   log_line("[AudioRx] Player buff FIFO new size: %d bytes", fcntl(s_fPipeAudioBufferWrite, F_GETPIPE_SZ));
}

void stop_audio_player_and_pipe()
{
   if ( -1 != s_fPipeAudioPlayerQueueWrite )
      close(s_fPipeAudioPlayerQueueWrite);
   s_fPipeAudioPlayerQueueWrite = -1;

   if ( -1 != s_fPipeAudioPlayerQueueRead )
      close(s_fPipeAudioPlayerQueueRead);
   s_fPipeAudioPlayerQueueRead = -1;

   if ( s_fPipeAudioBufferWrite > 0 )
      close(s_fPipeAudioBufferWrite);
   s_fPipeAudioBufferWrite = -1;

   if ( s_fPipeAudioBufferRead > 0 )
      close(s_fPipeAudioBufferRead);
   s_fPipeAudioBufferRead = -1;

   if ( s_fPipeAudioPlayerOutput > 0 )
      close(s_fPipeAudioPlayerOutput);
   s_fPipeAudioPlayerOutput = -1;

   s_bStopThreadAudioBuffering = true;
   if ( s_bThreadAudioBufferingStarted )
   {
      pthread_cancel(s_ThreadAudioBuffering);
   }
   s_bThreadAudioBufferingStarted = false;

   s_bStopThreadAudioQueueing = true;
   if ( s_bThreadAudioQueueingStarted )
   {
      pthread_cancel(s_ThreadAudioQueueing);
   }
   s_bThreadAudioQueueingStarted = false;

   if ( g_bSearching )
      return;
   if ( ! g_pCurrentModel->isAudioCapableAndEnabled() )
      return;
   if ( ! s_bHasAudioOutputDevice )
      return;    

   hw_stop_process("aplay");
}

void start_audio_player_and_pipe()
{
   if ( g_bSearching )
   {
      log_line("[AudioRx] Audio not started, controller is in search mode.");
      return;
   }
   if ( NULL == g_pCurrentModel )
   {
      log_line("[AudioRx] Audio not started, controller has no active vehicle.");
      return;
   }

   if ( ! s_bHasAudioOutputDevice )
   {
      log_line("[AudioRx] Controller has no output device. Not starting audio player and pipe.");
      return;    
   }

   if ( ! g_pCurrentModel->isAudioCapableAndEnabled() )
   {
      log_line("[AudioRx] Current vehicle did not passed audio checks. Not starting audio player and pipe.");
      return;
   }
   
   char szComm[128];
   sprintf(szComm, "aplay -q --disable-resample -N -R 10000 -c 1 --rate 44100 --format S16_LE %s 2>/dev/null &", FIFO_RUBY_AUDIO1);
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
      sprintf(szComm, "aplay -q -N -R 10000 -c 1 --rate 8000 --format S16_BE %s 2>/dev/null &", FIFO_RUBY_AUDIO1);
   hw_execute_bash_command_nonblock(szComm, NULL);
   hardware_sleep_ms(20);

   _open_audio_pipes();

   pthread_attr_t attr;
   struct sched_param params;

   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
   pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
   params.sched_priority = 0;
   pthread_attr_setschedparam(&attr, &params);
   s_bThreadAudioQueueingStarted = false;
   s_bStopThreadAudioQueueing = false;
   if ( 0 != pthread_create(&s_ThreadAudioQueueing, &attr, &_thread_audio_queueing_playback, NULL) )
      log_softerror_and_alarm("[AudioRx] Failed to create queueing thread.");
   else
      s_bThreadAudioQueueingStarted = true;
   pthread_attr_destroy(&attr);

   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
   pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
   params.sched_priority = 0;
   pthread_attr_setschedparam(&attr, &params);
   s_bThreadAudioBufferingStarted = false;
   s_bStopThreadAudioBuffering = false;
   if ( 0 != pthread_create(&s_ThreadAudioBuffering, &attr, &_thread_audio_buffering_playback, NULL) )
      log_softerror_and_alarm("[AudioRx] Failed to create playback thread.");
   else
      s_bThreadAudioBufferingStarted = true;
   pthread_attr_destroy(&attr);
}

void _reset_current_audio_rx_block()
{
   for( int i=0; i<MAX_BUFFERED_AUDIO_PACKETS; i++ )
      s_BufferAudioPacketsReceived[i] = false;
   s_uReceivedDataPacketsForCurrentBlock = 0;
   s_uReceivedECPacketsForCurrentBlock = 0;
}

void _output_audio_block(u32 uBlockIndex, u32 uPacketIndex, int iAudioSize)
{
   s_uLastOutputedBlockIndex = uBlockIndex;
   s_uLastOutputedPacketIndex = uPacketIndex;

   int iBreakFoundPosition = -1;
   u8* pData = &s_BufferAudioPackets[uPacketIndex][0];

   for( int i=0; i<iAudioSize; i++ )
   {
      if ( (*pData) != s_szAudioToken[s_iTokenPositionToCheck] )
      {
         s_iTokenPositionToCheck = 0;
         pData++;
         continue;
      }
      pData++;
      s_iTokenPositionToCheck++;
      if ( s_szAudioToken[s_iTokenPositionToCheck] == 0 )
      {
         iBreakFoundPosition = i+1;
         break;
      }
   }

   if ( iBreakFoundPosition == -1 )
   {
      if ( s_fPipeAudioBufferWrite > 0 )
         write(s_fPipeAudioBufferWrite, &s_BufferAudioPackets[uPacketIndex][0], iAudioSize);
      #ifdef FEATURE_LOCAL_AUDIO_RECORDING
      if ( NULL != s_pFileAudioRecording )
        fwrite(&s_BufferAudioPackets[uPacketIndex][0], 1, iAudioSize, s_pFileAudioRecording);
      #endif
      return;
   }

   #ifdef FEATURE_LOCAL_AUDIO_RECORDING
   if ( NULL != s_pFileAudioRecording )
   {
      fclose(s_pFileAudioRecording);
      s_pFileAudioRecording = NULL;
   }
   #endif

   if ( iBreakFoundPosition >= 0 )
   {
      if ( ! g_bSearching )
      if ( g_pCurrentModel->isAudioCapableAndEnabled() )
      if ( hardware_has_audio_playback() )
      {
         stop_audio_player_and_pipe();
         start_audio_player_and_pipe();
      }

      int iToOutput = iAudioSize - iBreakFoundPosition;

      if ( (iToOutput > 0) && (s_fPipeAudioBufferWrite > 0) )
         write(s_fPipeAudioBufferWrite, &s_BufferAudioPackets[uPacketIndex][iBreakFoundPosition], iToOutput);
   
      #ifdef FEATURE_LOCAL_AUDIO_RECORDING
      s_iAudioRecordingSegment++;
      char szBuff[128];
      sprintf(szBuff, "%s%s%d", FOLDER_RUBY_TEMP, FILE_TEMP_AUDIO_RECORDING, s_iAudioRecordingSegment);
      s_pFileAudioRecording = fopen(szBuff, "wb");

      if ( NULL != s_pFileAudioRecording )
      if ( iToOutput > 0 )
         fwrite(&s_BufferAudioPackets[uPacketIndex][iBreakFoundPosition], 1, iToOutput, s_pFileAudioRecording);
      #endif
   }
}


void _try_reconstruct_and_output_audio()
{
   if ( s_uReceivedDataPacketsForCurrentBlock + s_uReceivedECPacketsForCurrentBlock < (u32)s_iAudioDataPacketsPerBlock )
      return;

   // Reconstruct block

   // Add existing data packets, mark and count the ones that are missing

   missing_audio_packets_count_for_fec = 0;
   for(int i=0; i<s_iAudioDataPacketsPerBlock; i++)
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
   for( int i=0; i<s_iAudioECPacketsPerBlock; i++ )
   {
      if ( s_BufferAudioPacketsReceived[i+s_iAudioDataPacketsPerBlock] )
      {
         fec_decode_audio_fec_packets[pos] = &s_BufferAudioPackets[i+s_iAudioDataPacketsPerBlock][0];
         fec_decode_audio_fec_indexes[pos] = i;
         pos++;
         if ( pos == missing_audio_packets_count_for_fec )
            break;
      }
   }

   fec_decode(s_iAudioPacketSize, fec_decode_audio_data_packets, (unsigned int) s_iAudioDataPacketsPerBlock, fec_decode_audio_fec_packets, fec_decode_audio_fec_indexes, fec_decode_audio_missing_packets, missing_audio_packets_count_for_fec );

   for(int i=0; i<s_iAudioDataPacketsPerBlock; i++)
      _output_audio_block(s_uCurrentRxAudioBlockIndex, (u32)i, s_iAudioPacketSize);
}

bool is_audio_processing_started()
{
   return s_bAudioProcessingStarted;
}

void init_processing_audio()
{
   init_audio_rx_state();

   s_fPipeAudioPlayerOutput = -1;
   s_fPipeAudioBufferWrite = -1;
   s_fPipeAudioBufferRead = -1;
   s_bHasAudioOutputDevice = false;
   s_bAudioProcessingStarted = false;

   if ( g_bSearching || (NULL == g_pCurrentModel) )
      return;

   if ( ! g_pCurrentModel->audio_params.has_audio_device )
   {
      log_line("[AudioRx] Current vehicle has no audio device. Do not enable audio rx.");
      return;
   }

   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   if ( (g_pCurrentModel->audio_params.uFlags & 0x03) == 0 )
   {
      log_line("[AudioRx] Current vehicle has an audio device but no microphone. Do not enable audio rx.");
      return;
   }

   if ( ! g_pCurrentModel->audio_params.enabled )
   {
      log_line("[AudioRx] Current vehicle has an audio device and microphone but audio stream is disabled. Do not enable audio rx.");
      return;
   }

   if ( ! g_pCurrentModel->isAudioCapableAndEnabled() )
   {
      log_line("[AudioRx] Current vehicle did not passed audio checks. Do not enable audio rx.");
   }

   s_iAudioDataPacketsPerBlock = (int)((g_pCurrentModel->audio_params.uECScheme >> 4) & 0x0F);
   s_iAudioECPacketsPerBlock = (int)(g_pCurrentModel->audio_params.uECScheme & 0x0F);
   s_bAudioProcessingStarted = true;

   if ( hardware_has_audio_playback() )
   {
      log_line("[AudioRx] Init: current EC scheme: %d/%d, packet length: %d bytes", s_iAudioDataPacketsPerBlock, s_iAudioECPacketsPerBlock, s_iAudioPacketSize);
      s_bHasAudioOutputDevice = true;
      start_audio_player_and_pipe();
   }
   else
      log_softerror_and_alarm("[AudioRx] No output audio devices/soundcards on the controller. Audio output is disabled.");

   //s_pFileRawStreamOutput = fopen("raw_audio_in_stream.data", "wb");
   //if ( s_pFileRawStreamOutput <= 0 )
   //   log_softerror_and_alarm("[AudioRx] Failed to open audio rx input stream (%s)", "raw_audio_in_stream.data");
}

void uninit_processing_audio()
{
   if ( s_bHasAudioOutputDevice )
      stop_audio_player_and_pipe();

   if ( NULL != s_pFileRawStreamOutput )
      fclose(s_pFileRawStreamOutput);
   s_pFileRawStreamOutput = NULL;

   s_bAudioProcessingStarted = false;
}

void init_audio_rx_state()
{
   s_uCurrentRxAudioBlockIndex = MAX_U32;
   _reset_current_audio_rx_block();

   s_uLastRecvAudioBlockIndex = MAX_U32;
   s_uLastRecvAudioBlockPacketIndex = MAX_U32;
   s_uLastOutputedBlockIndex = MAX_U32;
   s_uLastOutputedPacketIndex = MAX_U32;

   s_iAudioDataPacketsPerBlock = DEFAULT_AUDIO_P_DATA;
   s_iAudioECPacketsPerBlock = DEFAULT_AUDIO_P_EC;
   s_iAudioPacketSize = DEFAULT_AUDIO_PACKET_LENGTH;
   s_iAudioRecordingSegment = 0;
   s_iTokenPositionToCheck = 0;
   strcpy(s_szAudioToken, "0123456789");
   s_szAudioToken[10] = 10;
   s_szAudioToken[11] = 0;

   s_iAudioBufferWritePos = 0;
   s_iAudioBufferReadPos = 0;
   s_iAudioBufferPacketsToCache = DEFAULT_AUDIO_BUFFERING_SIZE;

   if ( NULL != g_pCurrentModel )
   {
      s_iAudioDataPacketsPerBlock = (int)((g_pCurrentModel->audio_params.uECScheme >> 4) & 0x0F);
      s_iAudioECPacketsPerBlock = (int)(g_pCurrentModel->audio_params.uECScheme & 0x0F);
      s_iAudioPacketSize = (int)g_pCurrentModel->audio_params.uPacketLength;
      s_iAudioBufferPacketsToCache = (int)((g_pCurrentModel->audio_params.uFlags >> 8) & 0xFF);
   }

   log_line("[AudioRx] Rx state init: current EC scheme: %d/%d, packet length: %d bytes, cache %d packets", s_iAudioDataPacketsPerBlock, s_iAudioECPacketsPerBlock, s_iAudioPacketSize, s_iAudioBufferPacketsToCache);
}

void process_received_audio_packet(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   u8* pData = pPacketBuffer + sizeof(t_packet_header);

   if ( NULL != s_pFileRawStreamOutput )
      fwrite(pPacketBuffer, 1, pPH->total_length, s_pFileRawStreamOutput);

   u32 uAudioBlockSegmentIndex = 0;
   memcpy((u8*)&uAudioBlockSegmentIndex, pData, sizeof(u32));
   u32 uAudioBlockIndex = uAudioBlockSegmentIndex>>8;
   u32 uAudioBlockPacketIndex = uAudioBlockSegmentIndex & 0xFF;
   pData += sizeof(u32);
   int iAudioSize = (int)(pPH->total_length - sizeof(t_packet_header)- sizeof(u32));
   s_iAudioPacketSize = iAudioSize;

   if ( s_uLastRecvAudioBlockIndex != MAX_U32 )
   if ( s_uLastRecvAudioBlockIndex > 20 )
   if ( uAudioBlockIndex < s_uLastRecvAudioBlockIndex - 20 )
   {
      log_line("[AudioRx] Detected audio stream restart. Reset audio rx state.");
      init_audio_rx_state();
   }

   /*
   if ( s_uLastRecvAudioBlockIndex != MAX_U32 )
   {
      if ( uAudioBlockIndex == s_uLastRecvAudioBlockIndex)
      {
         if ( uAudioBlockPacketIndex != s_uLastRecvAudioBlockPacketIndex+1 )
            log_line("DBG audio gap from [%u/%u] to [%u/%u]", s_uLastRecvAudioBlockIndex, s_uLastRecvAudioBlockPacketIndex, uAudioBlockIndex, uAudioBlockPacketIndex);
      }
      else if ( uAudioBlockIndex != s_uLastRecvAudioBlockIndex+1 )
         log_line("DBG audio gap from [%u/%u] to [%u/%u]", s_uLastRecvAudioBlockIndex, s_uLastRecvAudioBlockPacketIndex, uAudioBlockIndex, uAudioBlockPacketIndex);
      else
      {
         if ( s_uLastRecvAudioBlockPacketIndex != (u32)(s_iAudioDataPacketsPerBlock + s_iAudioECPacketsPerBlock - 1) )
            log_line("DBG audio gap from [%u/%u] to [%u/%u]", s_uLastRecvAudioBlockIndex, s_uLastRecvAudioBlockPacketIndex, uAudioBlockIndex, uAudioBlockPacketIndex);
         if ( uAudioBlockPacketIndex != 0 )
            log_line("DBG audio gap from [%u/%u] to [%u/%u]", s_uLastRecvAudioBlockIndex, s_uLastRecvAudioBlockPacketIndex, uAudioBlockIndex, uAudioBlockPacketIndex);
      }
   }
   */
   s_uLastRecvAudioBlockIndex = uAudioBlockIndex;
   s_uLastRecvAudioBlockPacketIndex = uAudioBlockPacketIndex;
   
   if ( uAudioBlockPacketIndex >= MAX_BUFFERED_AUDIO_PACKETS )
      return;

   // First time we try to process audio packets?

   if ( s_uCurrentRxAudioBlockIndex == MAX_U32 )
   {
      // Wait for the begining of the next block
      if ( uAudioBlockPacketIndex >= (u32)s_iAudioDataPacketsPerBlock )
         return;
  
      s_uCurrentRxAudioBlockIndex = uAudioBlockIndex;
      _reset_current_audio_rx_block();
   }

   if ( uAudioBlockIndex < s_uCurrentRxAudioBlockIndex )
      return;

   if ( uAudioBlockIndex != s_uCurrentRxAudioBlockIndex )
   {
      // Try reconstruction of last block if needed
      if ( s_uLastOutputedPacketIndex < (u32)s_iAudioDataPacketsPerBlock-1 )
      if ( s_uReceivedDataPacketsForCurrentBlock + s_uReceivedECPacketsForCurrentBlock >= (u32)s_iAudioDataPacketsPerBlock )
      {
         _try_reconstruct_and_output_audio();
      }

      s_uLastOutputedBlockIndex = s_uCurrentRxAudioBlockIndex;
      s_uLastOutputedPacketIndex = s_iAudioDataPacketsPerBlock-1;

      _reset_current_audio_rx_block();
      s_uCurrentRxAudioBlockIndex = uAudioBlockIndex;

      // Wait for begining of the next block
      if ( uAudioBlockPacketIndex >= (u32)s_iAudioDataPacketsPerBlock )
         return;
   }

   if ( s_BufferAudioPacketsReceived[uAudioBlockPacketIndex] )
      return;

   if ( uAudioBlockPacketIndex < (u32)s_iAudioDataPacketsPerBlock )
      s_uReceivedDataPacketsForCurrentBlock++;
   else
      s_uReceivedECPacketsForCurrentBlock++;

   s_BufferAudioPacketsReceived[uAudioBlockPacketIndex] = true;
   memcpy(&s_BufferAudioPackets[uAudioBlockPacketIndex][0], pData, iAudioSize);

   u32 uNextBlockToOutput = s_uLastOutputedBlockIndex;
   u32 uNextPacketToOutput = s_uLastOutputedPacketIndex;

   uNextPacketToOutput++;
   if ( uNextPacketToOutput >= (u32)s_iAudioDataPacketsPerBlock )
   {
      uNextBlockToOutput++;
      uNextPacketToOutput = 0;
   }

   if ( uAudioBlockPacketIndex < (u32)s_iAudioDataPacketsPerBlock )
   if ( (uAudioBlockIndex == uNextBlockToOutput) && (uAudioBlockPacketIndex == uNextPacketToOutput) )
   {
      _output_audio_block(uAudioBlockIndex, uAudioBlockPacketIndex, iAudioSize);
   }
}
