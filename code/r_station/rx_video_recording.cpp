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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../radio/fec.h" 

#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/shared_mem.h"
#include "../base/models.h"
#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/parser_h264.h"
#include "../base/camera_utils.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"

#include "shared_vars.h"
#include "rx_video_recording.h"
#include "packets_utils.h"
#include "timers.h"

sem_t* s_pSemaphoreStartRecord = NULL; 
sem_t* s_pSemaphoreStopRecord = NULL; 
bool s_bRecording = false;
bool s_bRequestStopRecordingThread = false;

pthread_t s_pThreadVideoRecording;
int s_iPipeRecordingThreadWrite = 0;
int s_iPipeRecordingThreadRead = 0;
u32 s_TimeStartRecording = MAX_U32;
char s_szFileRecordingOutput[MAX_FILE_PATH_SIZE];
int s_iFileVideoRecordingOutput = -1;
u32 s_uRecordingFileSize = 0;
int s_iRecordingWidth = 0;
int s_iRecordingHeight = 0;
int s_iRecordingFPS = 0;
int s_iRecordingType = 0;

u8 s_uTempRecordingBuffer[32000];
int s_iTempRecordingBufferFilledInBytes = 0;

u32 s_uRecordingStreamCurrentParsedToken = 0x11111111;
u32 s_uRecordingStreamPrevParsedToken = 0x11111111;
bool s_bRecordingFoundStartOfFirstNAL = false;


void _recording_write_error(const char* szError)
{
   if ( (NULL == szError) || (0 == szError[0]) )
      return;
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
   FILE* fd = fopen(szFile, "a");
   if ( NULL != fd )
   {
      fprintf(fd, "%s\n", szError);
      fclose(fd);
   }
}

void* _thread_video_recording(void *argument)
{
   log_line("[VideoRecording-Th] Thread to record started.");

   char szComm[256];
   sprintf(szComm, "mkdir -p %s",FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL );
   sprintf(szComm, "chmod 777 %s",FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL );

   sprintf(szComm, "chmod 777 %s* 2>/dev/null", FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL);

   strcpy(s_szFileRecordingOutput, FOLDER_RUBY_TEMP);
   strcat(s_szFileRecordingOutput, FILE_TEMP_VIDEO_FILE);

   Preferences* p = get_Preferences();
   if ( p->iVideoDestination == 1 )
   {
      strcpy(s_szFileRecordingOutput, FOLDER_TEMP_VIDEO_MEM);
      strcat(s_szFileRecordingOutput, FILE_TEMP_VIDEO_MEM_FILE);
      char szBuff[2048];
      char szTemp[1024];
      sprintf(szComm, "umount %s", FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command_silent(szComm, NULL);

      hw_execute_bash_command_raw("free -m  | grep Mem", szBuff);
      long lt, lu, lf;
      sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lt, &lu, &lf);
      lf -= 200;
      if ( lf < 50 )
      {
         _recording_write_error("Failed to create RAM file.");
         s_bRecording = false;
         return NULL;
      }
      sprintf(szComm, "mount -t tmpfs -o size=%dM tmpfs %s", (int)lf, FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command(szComm, NULL);
   }

   log_line("[VideoRecording-Th] Recording to output file: (%s)", s_szFileRecordingOutput);

   int iOpenFlags = O_CREAT | O_WRONLY;
   //if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
   //   iOpenFlags |= O_NONBLOCK;
   s_iFileVideoRecordingOutput = open(s_szFileRecordingOutput, iOpenFlags);
   if ( -1 == s_iFileVideoRecordingOutput )
   {
      _recording_write_error("Failed to create recording file.");
      close(s_iPipeRecordingThreadRead);
      s_iPipeRecordingThreadRead = -1;
      close(s_iPipeRecordingThreadWrite);
      s_iPipeRecordingThreadWrite = -1;
      s_bRecording = false;
      return NULL;
   }

   //if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
   //if ( 0 != fcntl(s_iFileVideoRecordingOutput, F_SETFL, O_NONBLOCK) )
   //   log_softerror_and_alarm("[VideoRecording] Failed to set nonblock flag on video recording file");

   log_line("[VideoRecording-Th] Video recording file flags: %s", str_get_pipe_flags(fcntl(s_iFileVideoRecordingOutput, F_GETFL)));

   s_TimeStartRecording = 0;
   s_uRecordingFileSize = 0;
   s_uRecordingStreamCurrentParsedToken = 0x11111111;
   s_uRecordingStreamPrevParsedToken = 0x11111111;
   s_bRecordingFoundStartOfFirstNAL = false;

   fd_set fdSet;
   u8 uRecBuffer[32000];
   while ( (! g_bQuit) && (! s_bRequestStopRecordingThread) )
   {
      FD_ZERO(&fdSet);
      FD_SET(s_iPipeRecordingThreadRead, &fdSet);

      struct timeval timeInput;
      timeInput.tv_sec = 0;
      timeInput.tv_usec = 50*1000; // 50 miliseconds timeout

      int iSelectResult = select(s_iPipeRecordingThreadRead+1, &fdSet, NULL, NULL, &timeInput);
      if ( s_bRequestStopRecordingThread )
      {
         log_line("[VideoRecording-Th] Received request to stop recording");
         break;
      }
      if ( g_bQuit )
      {
         log_line("[VideoRecording-Th] Received request to stop recording due to quit.");
         break;
      }
      if ( iSelectResult < 0 )
      {
         log_line("[VideoRecording-Th] Thread select failed. Exit recording thread.");
         break;
      }
      if ( iSelectResult == 0 )
         continue;


      int iRead = read(s_iPipeRecordingThreadRead, uRecBuffer, sizeof(uRecBuffer)/sizeof(uRecBuffer[0]));
      if ( iRead < 0 )
      {
         log_line("[VideoRecording-Th] Read recording pipe failed. Exit recording thread.");
         break;
      }
      if ( iRead == 0 )
      {
         hardware_sleep_ms(10);
         continue;
      }

      int iRes = write(s_iFileVideoRecordingOutput, uRecBuffer, iRead);
      if ( iRes == iRead )
         continue;

      // Error writing to recording file
      log_softerror_and_alarm("[VideoRecording-Th] Recording thread failed to write %d bytes to recording file, result: %d , error: %d (%s)", iRead, iRes, errno, strerror(errno));
   }

   log_line("[VideoRecording-Th] Finishing recording...");

   close( s_iPipeRecordingThreadWrite );
   s_iPipeRecordingThreadWrite = -1;

   close( s_iPipeRecordingThreadRead );
   s_iPipeRecordingThreadRead = -1;

   close(s_iFileVideoRecordingOutput);
   s_iFileVideoRecordingOutput = -1;

   if ( (0 == s_TimeStartRecording) || (s_uRecordingFileSize < 10000) )
   {
      log_line("[VideoRecording-Th] Not recorded anything as first NAL was not found (start time: %u) or size too small (recording size: %u bytes)", s_TimeStartRecording, s_uRecordingFileSize);
      s_uRecordingStreamCurrentParsedToken = 0x11111111;
      s_uRecordingStreamPrevParsedToken = 0x11111111;
      s_bRecordingFoundStartOfFirstNAL = false;

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s 2>/dev/null 1>/dev/null", s_szFileRecordingOutput);
      hw_execute_bash_command_silent(szComm, NULL);
      s_szFileRecordingOutput[0] = 0;

      log_line("[VideoRecording-Th] Exit recording thread.");
      s_bRequestStopRecordingThread = false;
      s_bRecording = false;
      return NULL;
   }

   u32 duration_ms = get_current_timestamp_ms() - s_TimeStartRecording;
   log_line("[VideoRecording-Th] Recording duration: %u ms (%u sec), total %u bytes", duration_ms, duration_ms/1000, s_uRecordingFileSize);

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_VIDEO_FILE_INFO);
   log_line("[VideoRecording-Th] Writing video info file %s ...", szFile);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      system("sudo mount -o remount,rw /");
      char szTmp[MAX_FILE_PATH_SIZE];
      sprintf(szTmp, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_INFO);
      hw_execute_bash_command(szTmp, NULL);
      fd = fopen(szFile, "w");
   }

   if ( NULL == fd )
   {
      log_softerror_and_alarm("[VideoRecording-Th] Failed to create video info file %s", szFile);
      _recording_write_error("Failed to create video recording info file");
   }
   else
   {
      fprintf(fd, "%s\n", s_szFileRecordingOutput);
      fprintf(fd, "%d %d\n", s_iRecordingFPS, (int)(duration_ms/1000));
      fprintf(fd, "%d %d\n", s_iRecordingWidth, s_iRecordingHeight);
      fprintf(fd, "%d\n", s_iRecordingType);
      fclose(fd);
      log_line("[VideoRecording-Th] Created video info file %s, for raw stream: (%s) resolution: %d x %d, %d fps, video type: %d",
         szFile, s_szFileRecordingOutput, s_iRecordingWidth, s_iRecordingHeight, s_iRecordingFPS, s_iRecordingType);

      hw_execute_bash_command_nonblock("./ruby_video_proc", NULL);
   }

   log_line("[VideoRecording-Th] Exit recording thread.");
   s_uRecordingFileSize = 0;
   s_uRecordingStreamCurrentParsedToken = 0x11111111;
   s_uRecordingStreamPrevParsedToken = 0x11111111;
   s_bRecordingFoundStartOfFirstNAL = false;
   s_szFileRecordingOutput[0] = 0;
   s_bRequestStopRecordingThread = false;
   s_bRecording = false;
   return NULL;
}

void rx_video_recording_init()
{
   log_line("[VideoRecording] Init start...");

   s_szFileRecordingOutput[0] = 0;
   s_bRecording = false;
   s_uRecordingFileSize = 0;
   s_iFileVideoRecordingOutput = -1;
   s_uRecordingStreamCurrentParsedToken = 0x11111111;
   s_uRecordingStreamPrevParsedToken = 0x11111111;
   s_bRecordingFoundStartOfFirstNAL = false;

   s_pSemaphoreStartRecord = sem_open(SEMAPHORE_START_VIDEO_RECORD, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == s_pSemaphoreStartRecord )
      log_error_and_alarm("[VideoRecording] Failed to open semaphore for reading: %s", SEMAPHORE_START_VIDEO_RECORD);

   s_pSemaphoreStopRecord = sem_open(SEMAPHORE_STOP_VIDEO_RECORD, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == s_pSemaphoreStopRecord )
      log_error_and_alarm("[VideoRecording] Failed to open semaphore for reading: %s", SEMAPHORE_STOP_VIDEO_RECORD);

   if ( (NULL != s_pSemaphoreStopRecord) && (NULL != s_pSemaphoreStartRecord) )
      log_line("[VideoRecording] Opened semaphores for signaling video recording start/stop.");

   char szComm[MAX_FILE_PATH_SIZE];
   sprintf(szComm, "chmod 777 %s 2>/dev/null 1>/dev/null", FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "chmod 777 %s* 2>/dev/null 1>/dev/null", FOLDER_MEDIA);
   hw_execute_bash_command(szComm, NULL);

   load_Preferences();

   log_line("[VideoRecording] Init start complete.");
}

void rx_video_recording_uninit()
{
   log_line("[VideoRecording] Uninit start...");
   rx_video_recording_stop();

   if ( NULL != s_pSemaphoreStartRecord )
      sem_close(s_pSemaphoreStartRecord);
   s_pSemaphoreStartRecord = NULL;
   
   if ( NULL != s_pSemaphoreStopRecord )
      sem_close(s_pSemaphoreStopRecord);
   s_pSemaphoreStopRecord = NULL;
   log_line("[VideoRecording] Uninit complete.");
}


void rx_video_recording_start()
{
   if ( s_bRecording )
   {
      log_line("[VideoRecording] Received request to start recording but recording is already started. Ignore it.");
      return;
   }
   log_line("[VideoRecording] Received request to start recording video.");

   s_iTempRecordingBufferFilledInBytes = 0;
   s_iRecordingWidth = 1280;
   s_iRecordingHeight = 720;
   s_iRecordingFPS = 0;
   s_iRecordingType = VIDEO_TYPE_H264;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if( NULL == g_pVideoProcessorRxList[i] )
         break;
      if ( g_pCurrentModel->uVehicleId != g_pVideoProcessorRxList[i]->m_uVehicleId )
         continue;
      s_iRecordingWidth = g_pVideoProcessorRxList[i]->getVideoWidth();
      s_iRecordingHeight = g_pVideoProcessorRxList[i]->getVideoHeight();
      s_iRecordingFPS = g_pVideoProcessorRxList[i]->getVideoFPS();
      s_iRecordingType = g_pVideoProcessorRxList[i]->getVideoType();
      log_line("[VideoRecording] Found info for VID %u: w/h/fps: %dx%d@%d, type: %d", g_pCurrentModel->uVehicleId, s_iRecordingWidth, s_iRecordingHeight, s_iRecordingFPS, s_iRecordingType);
      break;
   }
   if ( 0 == s_iRecordingWidth )
   {
      log_softerror_and_alarm("[VideoRecording] Can't find processor rx video stream info for VID: %u", g_pCurrentModel->uVehicleId);
      return;
   }

   s_bRecording = true;
   s_bRequestStopRecordingThread = false;
   int iRetries = 100;
   while ( iRetries > 0 )
   {
      iRetries--;
      s_iPipeRecordingThreadRead = open(FIFO_RUBY_STATION_VIDEO_STREAM_RECORDING, O_CREAT | O_RDONLY | O_NONBLOCK);
      if ( s_iPipeRecordingThreadRead > 0 )
         break;
      log_line("[VideoRecording] Failed to open video recording pipe read endpoint: %s, error code (%d): [%s]",
            FIFO_RUBY_STATION_VIDEO_STREAM_RECORDING, errno, strerror(errno));
      hardware_sleep_ms(5);
   }

   if ( s_iPipeRecordingThreadRead <= 0 )
   {
      log_softerror_and_alarm("[VideoRecording] Failed to open video recording pipe read endpoint: %s", FIFO_RUBY_STATION_VIDEO_STREAM_RECORDING);
      _recording_write_error("Failed to create video recording pipe read endpoint.");
      s_bRecording = false;
      return;
   }
   log_line("[VideoRecording] Opened video recording pipe read endpoint: %s", FIFO_RUBY_STATION_VIDEO_STREAM_RECORDING);

   iRetries = 100;
   while ( iRetries > 0 )
   {
      iRetries--;
      s_iPipeRecordingThreadWrite = open(FIFO_RUBY_STATION_VIDEO_STREAM_RECORDING, O_CREAT | O_WRONLY | O_NONBLOCK);
      if ( s_iPipeRecordingThreadWrite > 0 )
         break;
      log_line("[VideoRecording] Failed to open video recording pipe write endpoint: %s, error code (%d): [%s]",
         FIFO_RUBY_STATION_VIDEO_STREAM_RECORDING, errno, strerror(errno));
      hardware_sleep_ms(5);
   }

   if ( s_iPipeRecordingThreadRead <= 0 )
   {
      log_softerror_and_alarm("[VideoRecording] Failed to open video recording pipe write endpoint: %s", FIFO_RUBY_STATION_VIDEO_STREAM_RECORDING);
      close(s_iPipeRecordingThreadRead);
      s_iPipeRecordingThreadRead = -1;
      _recording_write_error("Failed to create video recording pipe write endpoint.");
      s_bRecording = false;
      return;
   }
   log_line("[VideoRecording] Opened video recording pipe write endpoint: %s", FIFO_RUBY_STATION_VIDEO_STREAM_RECORDING);
   log_line("[VideoRecording] Video recording pipe write flags: %s", str_get_pipe_flags(fcntl(s_iPipeRecordingThreadWrite, F_GETFL)));
   log_line("[VideoRecording] Video recording FIFO write default size: %d bytes", fcntl(s_iPipeRecordingThreadWrite, F_GETPIPE_SZ));

   fcntl(s_iPipeRecordingThreadWrite, F_SETPIPE_SZ, 512000*4);
   log_line("[VideoRecording] Video recording FIFO write new size: %d bytes", fcntl(s_iPipeRecordingThreadWrite, F_GETPIPE_SZ));

   pthread_attr_t attr;
   struct sched_param params;
   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
   pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
   params.sched_priority = 0;
   pthread_attr_setschedparam(&attr, &params);
   
   if ( 0 != pthread_create(&s_pThreadVideoRecording, &attr, &_thread_video_recording, NULL) )
   {
      log_softerror_and_alarm("[VideoRecording] Failed to create recording thread.");
      _recording_write_error("Failed to create video recording thread.");
      pthread_attr_destroy(&attr);
      close(s_iPipeRecordingThreadRead);
      s_iPipeRecordingThreadRead = -1;
      close(s_iPipeRecordingThreadWrite);
      s_iPipeRecordingThreadWrite = -1;
      s_bRecording = false;
      return;
   }
   pthread_attr_destroy(&attr);
   log_line("[VideoRecording] Started thread to do video recording.");
   return;
}

void rx_video_recording_stop()
{
   if ( ! s_bRecording )
   {
      log_line("[VideoRecording] Received request to stop recording video but recording is not started. Ignore it.");
      return;
   }
   s_bRequestStopRecordingThread = true;
   log_line("[VideoRecording] Signaled recording thread to stop.");
}

bool rx_video_is_recording()
{
   return s_bRecording;
}

void rx_video_recording_on_new_data(u8* pData, int iLength)
{
   if ( (!s_bRecording) || (-1 == s_iFileVideoRecordingOutput) || (NULL == pData) || (iLength <= 0) || (s_iPipeRecordingThreadWrite <= 0) )
      return;

   while ( ! s_bRecordingFoundStartOfFirstNAL )
   {
      s_uRecordingStreamPrevParsedToken = (s_uRecordingStreamPrevParsedToken << 8) | (s_uRecordingStreamCurrentParsedToken & 0xFF);
      s_uRecordingStreamCurrentParsedToken = (s_uRecordingStreamCurrentParsedToken << 8) | (*pData);
      pData++;
      iLength--;
      s_uRecordingFileSize++;
      if ( s_uRecordingStreamCurrentParsedToken == 0x00000001 )
      {
         log_line("[VideoRecording] Found start of first NAL at position %u (bytes) in recording stream.", s_uRecordingFileSize);
         s_bRecordingFoundStartOfFirstNAL = true;
         s_TimeStartRecording = get_current_timestamp_ms();
         u8 uHeader[5];
         uHeader[0] = 0; uHeader[1] = 0; uHeader[2] = 0; uHeader[3] = 0x01;
         int iRes = write(s_iPipeRecordingThreadWrite, uHeader, 4);
         if ( iRes != 4 )
            log_softerror_and_alarm("[VideoRecording] Failed to write initial NAL header to recording file (%s), result: %d, error: %d (%s)", s_szFileRecordingOutput, iRes, errno, strerror(errno));
         break;
      }
   }
   if ( iLength <= 0 )
      return;

   s_uRecordingFileSize += iLength;

   if ( s_iTempRecordingBufferFilledInBytes + iLength > (int)sizeof(s_uTempRecordingBuffer)/(int)sizeof(s_uTempRecordingBuffer[0]) )
   {
      int iRes = write(s_iPipeRecordingThreadWrite, s_uTempRecordingBuffer, s_iTempRecordingBufferFilledInBytes);
      if ( iRes != s_iTempRecordingBufferFilledInBytes )
         log_softerror_and_alarm("[VideoRecording] Failed to write to recorder pipe %d bytes. Ret code: %d, Error code: %d, err string: (%s)",
            s_iTempRecordingBufferFilledInBytes, iRes, errno, strerror(errno));
      s_iTempRecordingBufferFilledInBytes = 0;
   }

   memcpy(&(s_uTempRecordingBuffer[s_iTempRecordingBufferFilledInBytes]), pData, iLength);
   s_iTempRecordingBufferFilledInBytes += iLength;
}

void rx_video_recording_periodic_loop()
{
   static u32 s_TimeLastPeriodicChecksVideoRecording = 0;

   if ( g_TimeNow < s_TimeLastPeriodicChecksVideoRecording + 200 )
      return;

   s_TimeLastPeriodicChecksVideoRecording = g_TimeNow;

   int val = 0;
   if ( NULL != s_pSemaphoreStartRecord )
   if ( 0 == sem_getvalue(s_pSemaphoreStartRecord, &val) )
   if ( 0 < val )
   if ( EAGAIN != sem_trywait(s_pSemaphoreStartRecord) )
   {
      log_line("[VideoRecording] Event to start recording is set.");
      if ( ! s_bRecording )
         rx_video_recording_start();
      else
         log_softerror_and_alarm("[VideoRecording] Recording is already started.");
   }

   val = 0;
   if ( NULL != s_pSemaphoreStopRecord )
   if ( 0 == sem_getvalue(s_pSemaphoreStopRecord, &val) )
   if ( 0 < val )
   if ( EAGAIN != sem_trywait(s_pSemaphoreStopRecord) )
   {
      log_line("[VideoRecording] Event to stop recording is set.");
      if ( s_bRecording )
         rx_video_recording_stop();
      else
         log_softerror_and_alarm("[VideoRecording] Recording is already stopped.");
   }
}
