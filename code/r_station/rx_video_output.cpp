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
#include "../base/camera_utils.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"

#include "shared_vars.h"
#include "rx_video_output.h"
#include "packets_utils.h"
#include "links_utils.h"
#include "timers.h"


pthread_t s_pThreadRxVideoOutputPlayer;
sem_t* s_pRxVideoSemaphoreRestartVideoPlayer = NULL;
bool s_bRxVideoOutputPlayerThreadMustStop = false;
bool s_bRxVideoOutputPlayerReinitializing = false;
int s_iPIDVideoPlayer = -1;
int s_fPipeVideoOutToPlayer = -1;

u32 s_uParseNALStartSequence = 0xFFFFFFFF;
u32 s_uParseNALCurrentSlices = 0;
u32 s_uParseLastNALTag = 0xFFFFFFFF;
u32 s_uParseNALTotalParsedBytes = 0;

u32 s_uLastIOErrorAlarmFlagsVideoPlayer = 0;
u32 s_uLastIOErrorAlarmFlagsUSBPlayer = 0;
u32 s_uTimeLastOkVideoPlayerOutput = 0;
u32 s_uTimeStartGettingVideoIOErrors = 0;
         
typedef struct
{
   bool bVideoUSBTethering;
   u32 TimeLastVideoUSBTetheringCheck;
   char szIPUSBVideo[32];
   struct sockaddr_in sockAddrUSBDevice;
   int socketUSBOutput;
   int usbBlockSize;
   u8 usbBuffer[2050];
   int usbBufferPos;
} t_video_usb_output_info;

t_video_usb_output_info s_VideoUSBOutputInfo;

typedef struct 
{
   bool s_bForwardETHPipeEnabled;
   int s_ForwardETHVideoPipeFile;

   bool s_bForwardIsETHForwardEnabled;
   int s_ForwardETHSocketVideo;

   struct sockaddr_in s_ForwardETHSockAddr;
   u8 s_BufferETH[2050];
   int s_nBufferETHPos;
   int s_BufferETHPacketSize;
} t_video_eth_forward_info;

t_video_eth_forward_info s_VideoETHOutputInfo;

int s_iLastUSBVideoForwardPort = -1;
int s_iLastUSBVideoForwardPacketSize = 0;

sem_t* s_pSemaphoreStartRecord = NULL; 
sem_t* s_pSemaphoreStopRecord = NULL; 
bool s_bRecording = false;

u32 s_TimeStartRecording = MAX_U32;
char s_szFileRecording[1024];
int s_iFileVideo = -1;

u32 s_TimeLastPeriodicChecksVideoRecording = 0;
u32 s_TimeLastPeriodicChecksUSBForward = 0;

u32 s_uLastVideoFrameTime = MAX_U32;

int s_iLocalUDPVideoOutput = -1;
struct sockaddr_in s_LocalUDPSocketAddr;

u32 s_uLastTimeComputedOutputBitrate = 0;
u32 s_uOutputBitratePipe = 0;
u32 s_uOutputBitrateUDP = 0;

void _rx_video_output_launch_video_player()
{
   log_line("[VideoOutput] Starting video player");

   /*
   if ( hw_process_exists("gst-launch-1.0") )
   {
      log_line("[VideoOutput] Video gst player process already running. Do nothing.");
      return;
   }
   hw_execute_bash_command("gst-launch-1.0 udpsrc port=5600 caps='application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264' ! rtph264depay ! 'video/x-h264,stream-format=byte-stream' ! fdsink | ./ruby_player_s &", NULL);
   */

   if ( hw_process_exists(VIDEO_PLAYER_PIPE) )
   {
      log_line("[VideoOutput] Video player process already running. Do nothing.");
      return;
   }

   ControllerSettings* pcs = get_ControllerSettings();
   char szPlayer[1024];

   #ifdef HW_CAPABILITY_IONICE
   if ( pcs->ioNiceRXVideo > 0 )
      sprintf(szPlayer, "ionice -c 1 -n %d nice -n %d ./%s > /dev/null 2>&1&", pcs->ioNiceRXVideo, pcs->iNiceRXVideo, VIDEO_PLAYER_PIPE);
   else
   #endif
      sprintf(szPlayer, "nice -n %d ./%s > /dev/null 2>&1&", pcs->iNiceRXVideo, VIDEO_PLAYER_PIPE);

   hw_execute_bash_command(szPlayer, NULL);

   hw_set_proc_priority(VIDEO_PLAYER_PIPE, pcs->iNiceRXVideo, pcs->ioNiceRXVideo, 1);

   char szComm[128];
   char szPids[128];

   sprintf(szComm, "pidof %s", VIDEO_PLAYER_PIPE);
   szPids[0] = 0;
   int count = 0;
   while ( (strlen(szPids) <= 2) && (count < 1000) )
   {
      hw_execute_bash_command_silent(szComm, szPids);
      if ( strlen(szPids) > 2 )
         break;
      hardware_sleep_ms(2);
      szPids[0] = 0;
      count++;
   }
   s_iPIDVideoPlayer = atoi(szPids);
   log_line("[VideoOutput] Started video player, PID: %s (%d)", szPids, s_iPIDVideoPlayer);

}

void _rx_video_output_stop_video_player()
{
   //hw_stop_process("gst-launch-1.0");
   //hw_stop_process(VIDEO_PLAYER_STDIN);
   //return;

   char szComm[512];
      
   if ( s_iPIDVideoPlayer > 0 )
   {
      log_line("[VideoOutput] Stoping video player by signaling (PID %d)...", s_iPIDVideoPlayer);
      int iRet = kill(s_iPIDVideoPlayer, SIGTERM);
      if ( iRet < 0 )
         log_line("[VideoOutput] Failed to stop video player, error number: %d, [%s]", errno, strerror(errno));
   }
   else
   {
      log_line("[VideoOutput] Stoping video player by command...");
      sprintf(szComm, "ps -ef | nice grep '%s' | nice grep -v grep | awk '{print $2}' | xargs kill -9 2>/dev/null", VIDEO_PLAYER_PIPE);
      hw_execute_bash_command(szComm, NULL);
   }
   s_iPIDVideoPlayer = -1;
   log_line("[VideoOutput] Executed command to stop video player");
}

static void * _thread_rx_video_output_player(void *argument)
{
   log_line("[VideoOutput] Starting worker thread for video player output");
   bool bMustRestartPlayer = false;
   sem_t* pSemaphore = sem_open(SEMAPHORE_RESTART_VIDEO_PLAYER, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == pSemaphore )
   {
      log_error_and_alarm("[VideoOutput] Thread failed to open semaphore: %s", SEMAPHORE_RESTART_VIDEO_PLAYER);
      return NULL;
   }
   else
      log_line("[VideoOutput] Thread opened semaphore for signaling video player restart");

   struct timespec tWait = {0, 100*1000*1000}; // 100 ms
   
   log_line("[VideoOutput] Started worker thread for video player output");
   
   while ( ! s_bRxVideoOutputPlayerThreadMustStop )
   {
      hardware_sleep_ms(50);
      if ( s_bRxVideoOutputPlayerThreadMustStop )
         break;
      tWait.tv_nsec = 100*1000*1000;
      int iRet = sem_timedwait(pSemaphore, &tWait);
      if ( ETIMEDOUT == iRet )
      {
         continue;
      }

      if ( (0 != iRet) && (!s_bRxVideoOutputPlayerReinitializing) )
         continue;

      bMustRestartPlayer = false;

      if ( s_bRxVideoOutputPlayerReinitializing )
         bMustRestartPlayer = true;

      int iVal = 0;
      if ( 0 == sem_getvalue(pSemaphore, &iVal) )
      if ( iVal > 0 )
         bMustRestartPlayer = true;

      if ( bMustRestartPlayer )
      {
         log_line("[VideoOutput] Thread: Semaphore to restart video player is signaled.");
         _rx_video_output_stop_video_player();
         _rx_video_output_launch_video_player();
         s_bRxVideoOutputPlayerReinitializing = false;
      }
   }

   if ( NULL != pSemaphore )
      sem_close(pSemaphore);
   pSemaphore = NULL;

   log_line("[VideoOutput] Stopped worker thread for video player output.");
   return NULL;
}

void _processor_rx_video_forward_open_eth_pipe()
{
   log_line("[VideoOutput] Creating ETH pipes for video forward...");
   char szComm[1024];
   #ifdef HW_CAPABILITY_IONICE
   sprintf(szComm, "ionice -c 1 -n 4 nice -n -5 cat %s | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=3 ! udpsink port=%d host=127.0.0.1 > /dev/null 2>&1 &", FIFO_RUBY_STATION_VIDEO_STREAM_ETH, g_pControllerSettings->nVideoForwardETHPort);
   #else
   sprintf(szComm, "nice -n -5 cat %s | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=3 ! udpsink port=%d host=127.0.0.1 > /dev/null 2>&1 &", FIFO_RUBY_STATION_VIDEO_STREAM_ETH, g_pControllerSettings->nVideoForwardETHPort);
   #endif
   hw_execute_bash_command(szComm, NULL);

   log_line("[VideoOutput] Opening video output pipe write endpoint for ETH forward RTS: %s", FIFO_RUBY_STATION_VIDEO_STREAM_ETH);
   s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = open(FIFO_RUBY_STATION_VIDEO_STREAM_ETH, O_WRONLY);
   if ( s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile < 0 )
   {
      log_error_and_alarm("[VideoOutput] Failed to open video output pipe write endpoint for ETH forward RTS: %s",FIFO_RUBY_STATION_VIDEO_STREAM_ETH);
      return;
   }
   log_line("[VideoOutput] Opened video output pipe write endpoint for ETH forward RTS: %s", FIFO_RUBY_STATION_VIDEO_STREAM_ETH);
   log_line("[VideoOutput] Video output pipe to ETH flags: %s", str_get_pipe_flags(fcntl(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile, F_GETFL)));
   s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = true;
}

void _processor_rx_video_forward_create_eth_socket()
{
   log_line("[VideoOutput] Creating ETH socket for video forward...");
   if ( -1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo )
      close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);

   s_VideoETHOutputInfo.s_ForwardETHSocketVideo = socket(AF_INET, SOCK_DGRAM, 0);
   if ( s_VideoETHOutputInfo.s_ForwardETHSocketVideo <= 0 )
   {
      log_softerror_and_alarm("[VideoOutput] Failed to create socket for video forward on ETH.");
      return;
   }

   int broadcastEnable = 1;
   int ret = setsockopt(s_VideoETHOutputInfo.s_ForwardETHSocketVideo, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
   if ( ret != 0 )
   {
      log_softerror_and_alarm("[VideoOutput] Failed to set the Video ETH forward socket broadcast flag.");
      close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
      s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
      return;
   }
  
   memset(&s_VideoETHOutputInfo.s_ForwardETHSockAddr, '\0', sizeof(struct sockaddr_in));
   s_VideoETHOutputInfo.s_ForwardETHSockAddr.sin_family = AF_INET;
   s_VideoETHOutputInfo.s_ForwardETHSockAddr.sin_port = (in_port_t)htons(g_pControllerSettings->nVideoForwardETHPort);
   s_VideoETHOutputInfo.s_ForwardETHSockAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
   //s_ForwardETHSockAddr.sin_addr.s_addr = inet_addr("192.168.1.255");

   s_VideoETHOutputInfo.s_nBufferETHPos = 0;
   s_VideoETHOutputInfo.s_BufferETHPacketSize = g_pControllerSettings->nVideoForwardETHPacketSize;
   if ( s_VideoETHOutputInfo.s_BufferETHPacketSize < 100 || s_VideoETHOutputInfo.s_BufferETHPacketSize > 2048 )
      s_VideoETHOutputInfo.s_BufferETHPacketSize = 2048;

   log_line("[VideoOutput] Opened socket [fd=%d] for video forward on ETH on port %d.", s_VideoETHOutputInfo.s_ForwardETHSocketVideo, g_pControllerSettings->nVideoForwardETHPort);
   s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = true;
}


void _start_recording()
{
   if ( s_bRecording )
      return;

   log_line("[VideoOutput] Received request to start recording video.");

   s_TimeStartRecording = get_current_timestamp_ms();
   strcpy(s_szFileRecording, FOLDER_RUBY_TEMP);
   strcat(s_szFileRecording, FILE_TEMP_VIDEO_FILE);

   load_Preferences();
   Preferences* p = get_Preferences();
   if ( p->iVideoDestination == 1 )
   {
      strcpy(s_szFileRecording, FOLDER_TEMP_VIDEO_MEM);
      strcat(s_szFileRecording, FILE_TEMP_VIDEO_MEM_FILE);
      char szComm[256];
      char szBuff[2048];
      char szTemp[1024];
   
      sprintf(szComm, "umount %s", FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command_silent(szComm, NULL);

      hw_execute_bash_command_raw("free -m  | grep Mem", szBuff);
      long lt, lu, lf;
      sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lt, &lu, &lf);
      lf -= 200;
      if ( lf < 50 )
         return;

      sprintf(szComm, "mount -t tmpfs -o size=%dM tmpfs %s ", (int)lf, FOLDER_TEMP_VIDEO_MEM);
      hw_execute_bash_command(szComm, NULL);
   }

   if ( s_iFileVideo > 0 )
      close(s_iFileVideo);

   s_iFileVideo = open(s_szFileRecording, O_CREAT | O_WRONLY | O_NONBLOCK);
   if ( -1 == s_iFileVideo )
   {
      char szFile[128];
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
      FILE* fd = fopen(szFile, "a");
      fprintf(fd, "%s%s\n", "Failed to create video recording file ", s_szFileRecording);
      fclose(fd);
   }

   if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
   if ( 0 != fcntl(s_iFileVideo, F_SETFL, O_NONBLOCK) )
      log_softerror_and_alarm("[VideoOutput] Failed to set nonblock flag on video recording file");

   log_line("[VideoOutput] Video recording file flags: %s", str_get_pipe_flags(fcntl(s_iFileVideo, F_GETFL)));
  
   s_bRecording = true;
}

void _stop_recording()
{
   if ( -1 != s_iFileVideo )
      close(s_iFileVideo);
   s_iFileVideo = -1;

   log_line("[VideoOutput] Received request to stop recording video.");
   if ( ! s_bRecording )
   {
      log_line("[VideoOutput] Not recording. Do nothing.");
      return;
   }
   u32 duration_ms = get_current_timestamp_ms() - s_TimeStartRecording;

   int width = 1280;
   int height = 720;
   int fps = 30;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if( NULL == g_pVideoProcessorRxList[i] )
         break;
      if ( g_pCurrentModel->uVehicleId != g_pVideoProcessorRxList[i]->m_uVehicleId )
         continue;
      width = g_pVideoProcessorRxList[i]->getVideoWidth();
      height = g_pVideoProcessorRxList[i]->getVideoHeight();
      fps = g_pVideoProcessorRxList[i]->getVideoFPS();
      break;
   }

   char szFile[128];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_VIDEO_FILE_INFO);
   log_line("[VideoOutput] Writing video info file %s ...", szFile);
   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
   {
      system("sudo mount -o remount,rw /");
      char szTmp[128];
      sprintf(szTmp, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_VIDEO_FILE_INFO);
      hw_execute_bash_command(szTmp, NULL);
      fd = fopen(szFile, "w");
   }

   if ( NULL == fd )
   {
      log_softerror_and_alarm("[VideoOutput] Failed to create video info file %s", szFile);
      strcpy(szFile, FOLDER_RUBY_TEMP);
      strcat(szFile, FILE_TEMP_VIDEO_FILE_PROCESS_ERROR);
      FILE* fd = fopen(szFile, "a");
      fprintf(fd, "%s\n", "Failed to create video recording info file.");
      fclose(fd);
   }
   else
   {
      fprintf(fd, "%s\n", s_szFileRecording);
      fprintf(fd, "%d %d\n", fps, duration_ms/1000 );
      fprintf(fd, "%d %d\n", width, height );
      fclose(fd);
   }

   log_line("[VideoOutput] Created video info file %s, for resolution: %d x %d, %d fps",
      szFile, width, height, fps);

   log_line("[VideoOutput] Video info file content: (%s, %d fps, %d seconds, %d x %d)", s_szFileRecording, fps, duration_ms/1000, width, height);
   hw_execute_bash_command("./ruby_video_proc &", NULL);
   s_bRecording = false;
   s_szFileRecording[0] = 0;
   log_line("[VideoOutput] Recording stopped.");
}

void rx_video_output_init()
{
   log_line("[VideoOutput] Init start...");
   s_fPipeVideoOutToPlayer = -1;
   s_iLocalUDPVideoOutput = -1;
   s_uLastVideoFrameTime= MAX_U32;
   
   s_uParseNALStartSequence = 0xFFFFFFFF;
   s_uParseNALCurrentSlices = 0;
   s_uParseLastNALTag = 0xFFFFFFFF;
   s_uParseNALTotalParsedBytes = 0;

   s_VideoUSBOutputInfo.bVideoUSBTethering = false;
   s_VideoUSBOutputInfo.TimeLastVideoUSBTetheringCheck = 0;
   s_VideoUSBOutputInfo.szIPUSBVideo[0] = 0;
   s_VideoUSBOutputInfo.socketUSBOutput = -1;
   s_VideoUSBOutputInfo.usbBlockSize = 1024;
   s_VideoUSBOutputInfo.usbBufferPos = 0;
   
   s_iLastUSBVideoForwardPort = g_pControllerSettings->iVideoForwardUSBPort;
   s_iLastUSBVideoForwardPacketSize = g_pControllerSettings->iVideoForwardUSBPacketSize;

   s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = false;
   s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = false;
   s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = -1;
   s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
   s_VideoETHOutputInfo.s_nBufferETHPos = 0;
   s_VideoETHOutputInfo.s_BufferETHPacketSize = 1024;

   if ( NULL != g_pControllerSettings && ( g_pControllerSettings->nVideoForwardETHType == 1 ) )
      s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = true;
   if ( NULL != g_pControllerSettings && ( g_pControllerSettings->nVideoForwardETHType == 2 ) )
      s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = true;

   if ( s_VideoETHOutputInfo.s_bForwardETHPipeEnabled )
   {
      log_line("[VideoOutput] Video ETH forwarding is enabled, type Raw.");
      _processor_rx_video_forward_open_eth_pipe();
   }
   if ( s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled )
   {
      log_line("[VideoOutput] Video ETH forwarding is enabled, type RTS.");
      _processor_rx_video_forward_create_eth_socket();
   }

   _rx_video_output_launch_video_player();
   
   s_pRxVideoSemaphoreRestartVideoPlayer = sem_open(SEMAPHORE_RESTART_VIDEO_PLAYER, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == s_pRxVideoSemaphoreRestartVideoPlayer )
      log_error_and_alarm("[VideoOutput] Failed to open semaphore for writing: %s", SEMAPHORE_RESTART_VIDEO_PLAYER);
   else
      log_line("[VideoOutput] Created semaphore for signaling video player worker thread.");
   s_bRxVideoOutputPlayerThreadMustStop = false;
   s_bRxVideoOutputPlayerReinitializing = false;
   if ( 0 != pthread_create(&s_pThreadRxVideoOutputPlayer, NULL, &_thread_rx_video_output_player, NULL) )
      log_error_and_alarm("[VideoOutput] Failed to create thread for video player output check");

   s_pSemaphoreStartRecord = sem_open(SEMAPHORE_START_VIDEO_RECORD, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == s_pSemaphoreStartRecord )
      log_error_and_alarm("[VideoOutput] Failed to open semaphore for reading: %s", SEMAPHORE_START_VIDEO_RECORD);

   s_pSemaphoreStopRecord = sem_open(SEMAPHORE_STOP_VIDEO_RECORD, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == s_pSemaphoreStopRecord )
      log_error_and_alarm("[VideoOutput] Failed to open semaphore for reading: %s", SEMAPHORE_STOP_VIDEO_RECORD);

   s_uLastIOErrorAlarmFlagsVideoPlayer = 0;
   s_uLastIOErrorAlarmFlagsUSBPlayer = 0;
   s_uTimeLastOkVideoPlayerOutput = 0;
   s_uTimeStartGettingVideoIOErrors = 0;

   log_line("[VideoOutput] Init start complete.");
}

void rx_video_output_uninit()
{
   log_line("[VideoOutput] Uninit start...");
   _stop_recording();

   if ( -1 != s_iLocalUDPVideoOutput )
   {
      close(s_iLocalUDPVideoOutput);
      s_iLocalUDPVideoOutput = -1;
      log_line("[VideoOutput] Closed local socket for local UDP output.");
   }

   s_bRxVideoOutputPlayerThreadMustStop = true;
   s_bRxVideoOutputPlayerReinitializing = true;
   if ( NULL != s_pRxVideoSemaphoreRestartVideoPlayer )
      sem_post(s_pRxVideoSemaphoreRestartVideoPlayer);
 
   pthread_cancel(s_pThreadRxVideoOutputPlayer);

   if ( NULL != s_pRxVideoSemaphoreRestartVideoPlayer )
      sem_close(s_pRxVideoSemaphoreRestartVideoPlayer);
   s_pRxVideoSemaphoreRestartVideoPlayer = NULL;

   if ( 0 == sem_unlink(SEMAPHORE_RESTART_VIDEO_PLAYER) )
      log_line("[VideoOutput] Destroied the semaphore for restarting video player.");
   else
      log_softerror_and_alarm("[VideoOutput] Failed to destroy the semaphore for restarting video player.");
   s_pRxVideoSemaphoreRestartVideoPlayer = NULL;

   _rx_video_output_stop_video_player();
   
   if ( -1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo )
      close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
   s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;

   if ( -1 != s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile )
      close(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile);
   s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = -1;

   if ( s_VideoETHOutputInfo.s_bForwardETHPipeEnabled )
      hw_stop_process("gst-launch-1.0");
   s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = false;
   s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = false;

   if ( -1 != s_fPipeVideoOutToPlayer )
   {
      log_line("[VideoOutput] Closed video output pipe.");
      close( s_fPipeVideoOutToPlayer );
   }
   s_fPipeVideoOutToPlayer = -1;

   if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
      close(s_VideoUSBOutputInfo.socketUSBOutput);
   s_VideoUSBOutputInfo.socketUSBOutput = -1;
   s_VideoUSBOutputInfo.bVideoUSBTethering = false;
   s_VideoUSBOutputInfo.usbBufferPos = 0;

   if ( NULL != s_pSemaphoreStartRecord )
      sem_close(s_pSemaphoreStartRecord);

   if ( NULL != s_pSemaphoreStopRecord )
      sem_close(s_pSemaphoreStopRecord);

   log_line("[VideoOutput] Uninit complete.");
}

void rx_video_output_enable_pipe_output()
{
   log_line("[VideoOutput] Opening video output pipe write endpoint: %s", FIFO_RUBY_STATION_VIDEO_STREAM);
   if ( -1 != s_fPipeVideoOutToPlayer )
   {
      log_line("[VideoOutput] Pipe for video output already opened.");
      return;
   }

   s_fPipeVideoOutToPlayer = open(FIFO_RUBY_STATION_VIDEO_STREAM, O_CREAT | O_WRONLY);
   if ( s_fPipeVideoOutToPlayer < 0 )
   {
      log_error_and_alarm("[VideoOutput] Failed to open video output pipe write endpoint: %s, error code (%d): [%s]",
         FIFO_RUBY_STATION_VIDEO_STREAM, errno, strerror(errno));
      return;
   }
   log_line("[VideoOutput] Opened video output pipe to player write endpoint: %s", FIFO_RUBY_STATION_VIDEO_STREAM);
   log_line("[VideoOutput] Video output pipe to player flags: %s", str_get_pipe_flags(fcntl(s_fPipeVideoOutToPlayer, F_GETFL)));

   if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
   if ( 0 != fcntl(s_fPipeVideoOutToPlayer, F_SETFL, O_NONBLOCK) )
      log_softerror_and_alarm("[IPC] Failed to set nonblock flag on PIC channel %s write endpoint.", FIFO_RUBY_STATION_VIDEO_STREAM);

   log_line("[IPC] Video player FIFO write endpoint pipe flags: %s", str_get_pipe_flags(fcntl(s_fPipeVideoOutToPlayer, F_GETFL)));
  
   log_line("[VideoOutput] Video player FIFO default size: %d bytes", fcntl(s_fPipeVideoOutToPlayer, F_GETPIPE_SZ));

   fcntl(s_fPipeVideoOutToPlayer, F_SETPIPE_SZ, 256000);

   log_line("[VideoOutput] Video player FIFO new size: %d bytes", fcntl(s_fPipeVideoOutToPlayer, F_GETPIPE_SZ));
}
void rx_video_output_disable_pipe_output()
{
   if ( -1 != s_fPipeVideoOutToPlayer )
   {
      close( s_fPipeVideoOutToPlayer );
      log_line("[VideoOutput] Closed video output to pipe.");
   }
   s_fPipeVideoOutToPlayer = -1;
}

void rx_video_output_enable_udp_output()
{
   log_line("[VideoOutput] Enabling video output to local UDP port...");

   if ( -1 != s_iLocalUDPVideoOutput )
   {
      log_line("[VideoOutput] Local output to UDP port already enabled.");
      return;
   }

   int iPort = 5600;
   s_iLocalUDPVideoOutput = socket(AF_INET, SOCK_DGRAM, 0);
   if ( s_iLocalUDPVideoOutput < 0 )
   {
      log_error_and_alarm("[VideoOutput] Failed to create local UDP video output socket.");
      return;
   }
   bzero((char *) &s_LocalUDPSocketAddr, sizeof(s_LocalUDPSocketAddr));
   s_LocalUDPSocketAddr.sin_family = AF_INET;
   s_LocalUDPSocketAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
   s_LocalUDPSocketAddr.sin_port = htons((unsigned short)iPort);

   if (connect(s_iLocalUDPVideoOutput, (struct sockaddr *) &s_LocalUDPSocketAddr, sizeof(s_LocalUDPSocketAddr)) < 0)
   {
      log_error_and_alarm("[VideoOutput] Failed to connect local UDP video output socket.");
      s_iLocalUDPVideoOutput = -1;
   }
   log_line("[VideoOutput] Created socket for local UDP output on port %d, fd=%d", iPort, s_iLocalUDPVideoOutput);
}

void rx_video_output_disable_udp_output()
{
   if ( -1 != s_iLocalUDPVideoOutput )
   {
      close(s_iLocalUDPVideoOutput);
      log_line("[VideoOutput] Closed socket for local UDP output.");
   }
   s_iLocalUDPVideoOutput = -1;
}



void _processor_rx_video_forward_parse_h264_stream(u8* pBuffer, int length)
{
   int iFoundNALVideoFrame = 0;
   int iFoundPosition = -1;
   u8* pTmp = pBuffer;
   
   for( int k=0; k<length; k++ )
   {
      s_uParseNALStartSequence = (s_uParseNALStartSequence<<8) | (*pTmp);
      s_uParseNALTotalParsedBytes++;
      pTmp++;

      if ( (s_uParseNALStartSequence & 0xFFFFFF00) == 0x0100 )
      {
         s_uParseLastNALTag = s_uParseNALStartSequence & 0b11111;
         if ( s_uParseLastNALTag == 1 || s_uParseLastNALTag == 5 )
         {
            s_uParseNALCurrentSlices++;
            if ( (int)s_uParseNALCurrentSlices >= camera_get_active_camera_h264_slices(g_pCurrentModel) )
            {
               s_uParseNALCurrentSlices = 0;
               iFoundNALVideoFrame = s_uParseLastNALTag;
               g_VideoInfoStats.uTmpCurrentFrameSize += k;
               iFoundPosition = k;
            }
         }
         //if ((s_uParseVideoTag == 0x0121) || (s_uParseVideoTag == 0x0127))
      }
   }

   if ( ! iFoundNALVideoFrame )
   {
      g_VideoInfoStats.uTmpCurrentFrameSize += length;
      return;
   }

    u32 delta = g_TimeNow - s_uLastVideoFrameTime;
    if ( delta > 254 )
       delta = 254;
    if ( delta == 0 )
       delta = 1;

    g_VideoInfoStats.uTmpCurrentFrameSize = g_VideoInfoStats.uTmpCurrentFrameSize / 1000;
    if ( g_VideoInfoStats.uTmpCurrentFrameSize > 127 )
       g_VideoInfoStats.uTmpCurrentFrameSize = 127;

    g_VideoInfoStats.uLastIndex = (g_VideoInfoStats.uLastIndex+1) % MAX_FRAMES_SAMPLES;
    g_VideoInfoStats.uFramesTimes[g_VideoInfoStats.uLastIndex] = delta;
    g_VideoInfoStats.uFramesTypesAndSizes[g_VideoInfoStats.uLastIndex] = (g_VideoInfoStats.uFramesTypesAndSizes[g_VideoInfoStats.uLastIndex] & 0x80) | ((u8)g_VideoInfoStats.uTmpCurrentFrameSize);
    
    u32 uNextIndex = (g_VideoInfoStats.uLastIndex+1) % MAX_FRAMES_SAMPLES;
    if ( iFoundNALVideoFrame == 5 )
       g_VideoInfoStats.uFramesTypesAndSizes[uNextIndex] |= (1<<7);
    else
       g_VideoInfoStats.uFramesTypesAndSizes[uNextIndex] &= 0x7F;

    g_VideoInfoStats.uTmpKeframeDeltaFrames++;
    static u32 s_uLastTimeReceivedKeyframeNALForward = 0;

    if ( iFoundNALVideoFrame == 5 )
    {
       g_VideoInfoStats.uKeyframeIntervalMs = g_TimeNow - s_uLastTimeReceivedKeyframeNALForward;
       s_uLastTimeReceivedKeyframeNALForward = g_TimeNow;
       g_VideoInfoStats.uTmpKeframeDeltaFrames = 0; 
    }

    s_uLastVideoFrameTime = g_TimeNow;
    g_VideoInfoStats.uTmpCurrentFrameSize = length-iFoundPosition;      
}

void _output_to_video_player(u32 uVehicleId, int width, int height, u8* pBuffer, int length)
{
   if ( (-1 == s_fPipeVideoOutToPlayer) || s_bRxVideoOutputPlayerReinitializing )
      return;

   // Check for video resolution changes
   static int s_iRxVideoForwardLastWidth = 0;
   static int s_iRxVideoForwardLastHeight = 0;

   if ( (s_iRxVideoForwardLastWidth == 0) || (s_iRxVideoForwardLastHeight == 0) )
   {
      s_iRxVideoForwardLastWidth = width;
      s_iRxVideoForwardLastHeight = height;
   }

   if ( (width != s_iRxVideoForwardLastWidth) || (height != s_iRxVideoForwardLastHeight) )
   {
      log_line("[VideoOutput] Video resolution changed at VID %u (From %d x %d to %d x %d). Signal reload of the video player.",
         uVehicleId, s_iRxVideoForwardLastWidth, s_iRxVideoForwardLastHeight, width, height);
      s_iRxVideoForwardLastWidth = width;
      s_iRxVideoForwardLastHeight = height;
      s_bRxVideoOutputPlayerReinitializing = true;
      if ( NULL != s_pRxVideoSemaphoreRestartVideoPlayer )
      {
         sem_post(s_pRxVideoSemaphoreRestartVideoPlayer);
         log_line("[VideoOutput] Signaled restart of player");
      }
      return;
   }


   int iRes = write(s_fPipeVideoOutToPlayer, pBuffer, length);
   if ( iRes == length )
   {
      s_uOutputBitratePipe += iRes*8;
      s_uTimeStartGettingVideoIOErrors = 0;
      if ( 0 != s_uLastIOErrorAlarmFlagsVideoPlayer )
      {
         s_uTimeLastOkVideoPlayerOutput = g_TimeNow;
         s_uLastIOErrorAlarmFlagsVideoPlayer = 0;
         send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, 0,0);
      }
      return;
   }
   
   // Error outputing video to player

   u32 uFlags = 0;

   if ( iRes >= 0 )
      uFlags = ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_TRUNCATED;
   else
   {
      if ( EAGAIN == errno )
         uFlags = ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_WOULD_BLOCK;
      else
         uFlags = ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT;
   }

   u32 uTimeLastVideoChanged = 0;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL == g_pVideoProcessorRxList[i] )
         break;
      uTimeLastVideoChanged = g_pVideoProcessorRxList[i]->getLastTimeVideoStreamChanged();
      break;
   }

   s_uLastIOErrorAlarmFlagsVideoPlayer = uFlags;

   if ( 0 == s_uTimeStartGettingVideoIOErrors )
      s_uTimeStartGettingVideoIOErrors = g_TimeNow;
   else if ( g_TimeNow > s_uTimeStartGettingVideoIOErrors + 1000 )
   {
      if ( g_TimeNow > uTimeLastVideoChanged + 5000 )
         send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, uFlags, 0);

      if ( (0 != s_uTimeLastOkVideoPlayerOutput) && (g_TimeNow > s_uTimeLastOkVideoPlayerOutput + 3000) )
      {
         log_line("[VideoOutput] Error outputing video data to video player. Reinitialize video player...");
         s_bRxVideoOutputPlayerReinitializing = true;
         if ( NULL != s_pRxVideoSemaphoreRestartVideoPlayer )
         {
            sem_post(s_pRxVideoSemaphoreRestartVideoPlayer);
            log_line("[VideoOutput] Signaled restart of player");
         }
      }
      if ( 0 == s_uTimeLastOkVideoPlayerOutput && g_TimeNow > g_TimeStart + 5000 )
      {
         log_line("[VideoOutput] Error outputing any video data to video player. Reinitialize video player...");
         s_bRxVideoOutputPlayerReinitializing = true;
         if ( NULL != s_pRxVideoSemaphoreRestartVideoPlayer )
         {
            sem_post(s_pRxVideoSemaphoreRestartVideoPlayer);
            log_line("[VideoOutput] Signaled restart of player");
         }
      }
   }
}

void rx_video_output_video_data(u32 uVehicleId, u8 uVideoStreamType, int width, int height, u8* pBuffer, int length)
{
   if ( g_bSearching )
      return;

   if ( -1 != s_iLocalUDPVideoOutput )
   {
      s_uOutputBitrateUDP += length*8;
      
      //send(s_iLocalUDPVideoOutput, pBuffer, length, MSG_DONTWAIT); 
      sendto(s_iLocalUDPVideoOutput, pBuffer, length, 0, (struct sockaddr *)&s_LocalUDPSocketAddr, sizeof(s_LocalUDPSocketAddr) );

      static bool bStartedGST = false;
      if ( ! bStartedGST )
      {
         bStartedGST = true;
         hw_execute_bash_command("gst-launch-1.0 udpsrc port=5600 caps='application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264' ! rtph264depay ! 'video/x-h264,stream-format=byte-stream' ! fdsink | ./ruby_player_s &", NULL);
      }
   }

   if ( NULL != g_pCurrentModel )
   if ( uVideoStreamType == VIDEO_TYPE_H264 )
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_STATS_VIDEO_INFO)
      _processor_rx_video_forward_parse_h264_stream(pBuffer, length);

   _output_to_video_player(uVehicleId, width, height, pBuffer, length);

   if ( s_VideoETHOutputInfo.s_bForwardETHPipeEnabled && (-1 != s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile) )
   {
      write(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile, pBuffer, length);
   }

   if ( s_bRecording && -1 != s_iFileVideo )
   {
      int iRes = write(s_iFileVideo, pBuffer, length);
      if ( iRes != length )
      {
         u32 uFlags = 0;
         if ( iRes >= 0 )
            uFlags = ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_TRUNCATED;
         else
         {
            if ( EAGAIN == errno )
               uFlags = ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_WOULD_BLOCK;
            else
               uFlags = ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT;
         }

         u32 uTimeLastVideoChanged = 0;
         for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
         {
            if ( NULL == g_pVideoProcessorRxList[i] )
               break;
            if ( g_pCurrentModel->uVehicleId != g_pVideoProcessorRxList[i]->m_uVehicleId )
               continue;
            uTimeLastVideoChanged = g_pVideoProcessorRxList[i]->getLastTimeVideoStreamChanged();
            break;
         }
         if ( g_TimeNow > uTimeLastVideoChanged + 5000 )
            send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, uFlags,0);
        
         s_uLastIOErrorAlarmFlagsUSBPlayer = uFlags;
      }
      else if ( 0 != s_uLastIOErrorAlarmFlagsUSBPlayer )
      {
         s_uLastIOErrorAlarmFlagsUSBPlayer = 0;
         send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, 0,0);
      }
   }

   if ( s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled && (-1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo ) )
   {
      u8* pData = pBuffer;
      int dataLen = length;
      while ( dataLen > 0 )
      {
          int maxCopy = dataLen;
          if ( maxCopy > s_VideoETHOutputInfo.s_BufferETHPacketSize - s_VideoETHOutputInfo.s_nBufferETHPos )
             maxCopy = s_VideoETHOutputInfo.s_BufferETHPacketSize - s_VideoETHOutputInfo.s_nBufferETHPos;
          memcpy( &(s_VideoETHOutputInfo.s_BufferETH[s_VideoETHOutputInfo.s_nBufferETHPos]), pData, maxCopy );
          pData += maxCopy;
          dataLen -= maxCopy;
          s_VideoETHOutputInfo.s_nBufferETHPos += maxCopy;
          if ( s_VideoETHOutputInfo.s_nBufferETHPos >= s_VideoETHOutputInfo.s_BufferETHPacketSize )
          {
             int res = sendto(s_VideoETHOutputInfo.s_ForwardETHSocketVideo, s_VideoETHOutputInfo.s_BufferETH, s_VideoETHOutputInfo.s_nBufferETHPos,
                           0, (struct sockaddr *)&s_VideoETHOutputInfo.s_ForwardETHSockAddr, sizeof(s_VideoETHOutputInfo.s_ForwardETHSockAddr) );
             if ( res < 0 )
             {
                log_line("[VideoOutput] Failed to send to ETH Port %d bytes, [fd=%d]", length, s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
                close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
                s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
             }
             //else
             //   log_line("Sent %d bytes to port %d", length, g_pControllerSettings->nVideoForwardETHPort);
             s_VideoETHOutputInfo.s_nBufferETHPos = 0;
          }
      }
   }

   if ( s_VideoUSBOutputInfo.bVideoUSBTethering && 0 != s_VideoUSBOutputInfo.szIPUSBVideo[0] )
   {
      u8* pData = pBuffer;
      int dataLen = length;
      while ( dataLen > 0 )
      {
         int maxCopy = dataLen;
         if ( maxCopy > s_VideoUSBOutputInfo.usbBlockSize - s_VideoUSBOutputInfo.usbBufferPos )
             maxCopy = s_VideoUSBOutputInfo.usbBlockSize - s_VideoUSBOutputInfo.usbBufferPos;
         memcpy( &(s_VideoUSBOutputInfo.usbBuffer[s_VideoUSBOutputInfo.usbBufferPos]), pData, maxCopy );
         pData += maxCopy;
         dataLen -= maxCopy;
         s_VideoUSBOutputInfo.usbBufferPos += maxCopy;
         if ( s_VideoUSBOutputInfo.usbBufferPos >= s_VideoUSBOutputInfo.usbBlockSize )
         {
            int res = sendto(s_VideoUSBOutputInfo.socketUSBOutput, s_VideoUSBOutputInfo.usbBuffer, s_VideoUSBOutputInfo.usbBlockSize,
                  0, (struct sockaddr *)&s_VideoUSBOutputInfo.sockAddrUSBDevice, sizeof(s_VideoUSBOutputInfo.sockAddrUSBDevice) );
            if ( res < 0 )
            {
              log_line("[VideoOutput] Failed to send to USB socket");
              if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
                 close(s_VideoUSBOutputInfo.socketUSBOutput);
              s_VideoUSBOutputInfo.socketUSBOutput = -1;
              s_VideoUSBOutputInfo.bVideoUSBTethering = false;
              s_VideoUSBOutputInfo.usbBufferPos = 0;
              log_line("[VideoOutput] Video Output to USB disabled.");
              break;
            }
            s_VideoUSBOutputInfo.usbBufferPos = 0;
         }
      }
   }
}


void rx_video_output_on_controller_settings_changed()
{
   if ( s_iLastUSBVideoForwardPort != g_pControllerSettings->iVideoForwardUSBPort ||
        s_iLastUSBVideoForwardPacketSize != g_pControllerSettings->iVideoForwardUSBPacketSize )
   if ( s_VideoUSBOutputInfo.bVideoUSBTethering )
   {
      if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
         close(s_VideoUSBOutputInfo.socketUSBOutput);
      s_VideoUSBOutputInfo.socketUSBOutput = -1;
      s_VideoUSBOutputInfo.bVideoUSBTethering = false;
      log_line("[VideoOutput] Video Output to USB disabled due to settings changed.");
   }

   if ( g_pControllerSettings->nVideoForwardETHType == 0 )
   {
      if ( -1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo )
         close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
      s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
      if ( -1 != s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile )
         close(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile);
      s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = -1;

      s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = false;
      s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = false;
      log_line("[VideoOutput] Video ETH forwarding was disabled.");
   }
   else if ( g_pControllerSettings->nVideoForwardETHType == 1 )
   {
      if ( -1 != s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile )
         close(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile);
      s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = -1;
      if ( s_VideoETHOutputInfo.s_bForwardETHPipeEnabled )
         hw_stop_process("gst-launch-1.0");
      s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = false;
      s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = true;

      log_line("[VideoOutput] Video ETH forwarding is enabled, type Raw.");
      _processor_rx_video_forward_create_eth_socket();
   }
   else if ( g_pControllerSettings->nVideoForwardETHType == 2 )
   {
      if ( -1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo )
         close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
      s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
      s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = false;

      s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = true;
      log_line("[VideoOutput] Video ETH forwarding is enabled, type RTS.");
      _processor_rx_video_forward_open_eth_pipe();
   }

   s_iLastUSBVideoForwardPort = g_pControllerSettings->iVideoForwardUSBPort;
   s_iLastUSBVideoForwardPacketSize = g_pControllerSettings->iVideoForwardUSBPacketSize;
}


void rx_video_output_periodic_loop()
{
   if ( g_bDebugState )
   if ( g_TimeNow >= s_uLastTimeComputedOutputBitrate + 1000 )
   {
      log_line("[VideoOutput] Output to pipe: %u bps, output to UDP: %u bps", s_uOutputBitratePipe, s_uOutputBitrateUDP );
      s_uLastTimeComputedOutputBitrate = g_TimeNow;
      s_uOutputBitratePipe = 0;
      s_uOutputBitrateUDP = 0;
   }

   if ( g_TimeNow > s_TimeLastPeriodicChecksUSBForward + 300 )
   {
      s_TimeLastPeriodicChecksUSBForward = g_TimeNow;

      // Stopped USB forward?
      if ( s_VideoUSBOutputInfo.bVideoUSBTethering && (g_pControllerSettings->iVideoForwardUSBType == 0) )
      {
         if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
            close(s_VideoUSBOutputInfo.socketUSBOutput);
         s_VideoUSBOutputInfo.socketUSBOutput = -1;
         s_VideoUSBOutputInfo.bVideoUSBTethering = false;
         log_line("[VideoOutput] Video Output to USB disabled.");
      }

      if ( g_pControllerSettings->iVideoForwardUSBType != 0 )
      if ( g_TimeNow > s_VideoUSBOutputInfo.TimeLastVideoUSBTetheringCheck + 1000 )
      {
         char szFile[128];
         strcpy(szFile, FOLDER_RUBY_TEMP);
         strcat(szFile, FILE_TEMP_USB_TETHERING_DEVICE);
         s_VideoUSBOutputInfo.TimeLastVideoUSBTetheringCheck = g_TimeNow;
         if ( ! s_VideoUSBOutputInfo.bVideoUSBTethering )
         if ( access(szFile, R_OK) != -1 )
         {
         s_VideoUSBOutputInfo.szIPUSBVideo[0] = 0;
         FILE* fd = fopen(szFile, "r");
         if ( NULL != fd )
         {
            fscanf(fd, "%s", s_VideoUSBOutputInfo.szIPUSBVideo);
            fclose(fd);
         }
         log_line("[VideoOutput] USB Device Tethered for Video Output. Device IP: %s", s_VideoUSBOutputInfo.szIPUSBVideo);

         s_VideoUSBOutputInfo.socketUSBOutput = socket(AF_INET , SOCK_DGRAM, 0);
         if ( s_VideoUSBOutputInfo.socketUSBOutput != -1 && 0 != s_VideoUSBOutputInfo.szIPUSBVideo[0] )
         {
            memset(&s_VideoUSBOutputInfo.sockAddrUSBDevice, 0, sizeof(s_VideoUSBOutputInfo.sockAddrUSBDevice));
            s_VideoUSBOutputInfo.sockAddrUSBDevice.sin_family = AF_INET;
            s_VideoUSBOutputInfo.sockAddrUSBDevice.sin_addr.s_addr = inet_addr(s_VideoUSBOutputInfo.szIPUSBVideo);
            s_VideoUSBOutputInfo.sockAddrUSBDevice.sin_port = htons( g_pControllerSettings->iVideoForwardUSBPort );
         }
         s_VideoUSBOutputInfo.usbBlockSize = g_pControllerSettings->iVideoForwardUSBPacketSize;
         s_VideoUSBOutputInfo.usbBufferPos = 0;
         s_VideoUSBOutputInfo.bVideoUSBTethering = true;
         return;
         }

         if ( s_VideoUSBOutputInfo.bVideoUSBTethering )
         if ( access(szFile, R_OK) == -1 )
         {
            log_line("[VideoOutput] Tethered USB Device for Video Output Unplugged.");
            if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
               close(s_VideoUSBOutputInfo.socketUSBOutput);
            s_VideoUSBOutputInfo.socketUSBOutput = -1;
            s_VideoUSBOutputInfo.usbBufferPos = 0;
            s_VideoUSBOutputInfo.bVideoUSBTethering = false;
         }
      }
   }

   if ( g_TimeNow >= s_TimeLastPeriodicChecksVideoRecording + 300 )
   {
      s_TimeLastPeriodicChecksVideoRecording = g_TimeNow;

      int val = 0;
      if ( NULL != s_pSemaphoreStartRecord )
      if ( 0 == sem_getvalue(s_pSemaphoreStartRecord, &val) )
      if ( 0 < val )
      if ( EAGAIN != sem_trywait(s_pSemaphoreStartRecord) )
      {
         log_line("[VideoOutput] Event to start recording is set.");
         if ( ! s_bRecording )
         {
            _start_recording();
            log_line("[VideoOutput] Recording started.");
         }
         else
            log_softerror_and_alarm("[VideoOutput] Recording is already started.");
      }
   
      val = 0;
      if ( NULL != s_pSemaphoreStopRecord )
      if ( 0 == sem_getvalue(s_pSemaphoreStopRecord, &val) )
      if ( 0 < val )
      if ( EAGAIN != sem_trywait(s_pSemaphoreStopRecord) )
      {
         log_line("[VideoOutput] Event to stop recording is set.");
         if ( s_bRecording )
         {
            _stop_recording();
            log_line("[VideoOutput] Recording stopped.");
         }
         else
            log_softerror_and_alarm("[VideoOutput] Recording is already stopped.");
      }
   }
}
