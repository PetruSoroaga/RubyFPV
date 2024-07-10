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
#include "rx_video_output.h"
#include "rx_video_recording.h"
#include "packets_utils.h"
#include "links_utils.h"
#include "timers.h"


pthread_t s_pThreadRxVideoOutputPlayer;
sem_t* s_pRxVideoSemaphoreRestartVideoPlayer = NULL;
bool s_bRxVideoOutputPlayerThreadMustStop = false;
bool s_bRxVideoOutputPlayerMustReinitialize = false;
int s_iPIDVideoPlayer = -1;
int s_fPipeVideoOutToPlayer = -1;
bool s_bDidSentAnyDataToVideoPlayerPipe = false;

ParserH264 s_ParserH264Output;

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

u32 s_TimeLastPeriodicChecksUSBForward = 0;

u32 s_uLastVideoFrameTime = MAX_U32;

int s_iLocalVideoPlayerUDPSocket = -1;
struct sockaddr_in s_LocalVideoPlayuerUDPSocketAddr;

u32 s_uLastTimeComputedOutputBitrate = 0;
u32 s_uOutputBitrateToLocalVideoPlayerPipe = 0;
u32 s_uOutputBitrateToLocalVideoPlayerUDP = 0;

char s_szOutputVideoPlayerFilename[MAX_FILE_PATH_SIZE];

/*
static void * _thread_video_player(void *argument)
{
   char szPlayer[MAX_FILE_PATH_SIZE];
   sprintf(szPlayer, "%s -z -o hdmi /tmp/ruby/fifovidstream > /dev/null 2>&1", "omxplayer");
   //hw_execute_bash_command(szPlayer, NULL);
   //system(szPlayer);
   return NULL;
}
*/


void _rx_video_output_launch_video_player()
{
   #ifdef HW_PLATFORM_RASPBERRY

   strcpy(s_szOutputVideoPlayerFilename, VIDEO_PLAYER_PIPE);
   
   log_line("[VideoOutput] Starting video player [%s]", s_szOutputVideoPlayerFilename);

   if ( hw_process_exists(s_szOutputVideoPlayerFilename) )
   {
      log_line("[VideoOutput] Video player process already running. Do nothing.");
      return;
   }

   ControllerSettings* pcs = get_ControllerSettings();
   char szPlayer[1024];

   if ( pcs->iNiceRXVideo >= 0 )
   {
      // Auto priority
      sprintf(szPlayer, "./%s > /dev/null 2>&1&", s_szOutputVideoPlayerFilename);
      //sprintf(szPlayer, "%s -z -o hdmi /tmp/ruby/fifovidstream > /dev/null 2>&1 &", s_szOutputVideoPlayerFilename);
   }
   else
   {
      #ifdef HW_CAPABILITY_IONICE
      if ( pcs->ioNiceRXVideo > 0 )
         sprintf(szPlayer, "ionice -c 1 -n %d nice -n %d ./%s > /dev/null 2>&1 &", pcs->ioNiceRXVideo, pcs->iNiceRXVideo, s_szOutputVideoPlayerFilename);
      else
      #endif
         sprintf(szPlayer, "nice -n %d ./%s > /dev/null 2>&1 &", pcs->iNiceRXVideo, s_szOutputVideoPlayerFilename);
   }

   //pthread_t th;
   //pthread_create(&th, NULL, &_thread_video_player, NULL );

   hw_execute_bash_command(szPlayer, NULL);
   if ( pcs->iNiceRXVideo < 0 )
      hw_set_proc_priority(s_szOutputVideoPlayerFilename, pcs->iNiceRXVideo, pcs->ioNiceRXVideo, 1);

   char szComm[256];
   char szPids[128];

   sprintf(szComm, "pidof %s", s_szOutputVideoPlayerFilename);
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
   log_line("[VideoOutput] Started video player [%s], PID: %s (%d)", s_szOutputVideoPlayerFilename, szPids, s_iPIDVideoPlayer);

   #endif

   #ifdef HW_PLATFORM_RADXA_ZERO3

   strcpy(s_szOutputVideoPlayerFilename, VIDEO_PLAYER_UDP);
   log_line("[VideoOutput] Starting video player [%s]", s_szOutputVideoPlayerFilename);
   if ( hw_process_exists(s_szOutputVideoPlayerFilename) )
   {
      log_line("[VideoOutput] Video player process already running. Do nothing.");
      return;
   }

   ControllerSettings* pcs = get_ControllerSettings();
   char szPlayer[1024];

   char szCodec[32];
   szCodec[0] = 0;
   if ( g_pCurrentModel->video_params.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
      strcpy(szCodec, " -h265");

   if ( pcs->iNiceRXVideo >= 0 )
   {
      // Auto priority
      sprintf(szPlayer, "./%s%s -p > /dev/null 2>&1&", s_szOutputVideoPlayerFilename, szCodec);
   }
   else
   {
      #ifdef HW_CAPABILITY_IONICE
      if ( pcs->ioNiceRXVideo > 0 )
         sprintf(szPlayer, "ionice -c 1 -n %d nice -n %d ./%s%s -p > /dev/null 2>&1 &", pcs->ioNiceRXVideo, pcs->iNiceRXVideo, s_szOutputVideoPlayerFilename, szCodec);
      else
      #endif
         sprintf(szPlayer, "nice -n %d ./%s%s -p > /dev/null 2>&1 &", pcs->iNiceRXVideo, s_szOutputVideoPlayerFilename, szCodec);
   }

   hw_execute_bash_command(szPlayer, NULL);
   if ( pcs->iNiceRXVideo < 0 )
      hw_set_proc_priority(s_szOutputVideoPlayerFilename, pcs->iNiceRXVideo, pcs->ioNiceRXVideo, 1);

   char szComm[256];
   char szPids[128];

   sprintf(szComm, "pidof %s", s_szOutputVideoPlayerFilename);
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
   log_line("[VideoOutput] Started video player [%s], PID: %s (%d)", s_szOutputVideoPlayerFilename, szPids, s_iPIDVideoPlayer);

   #endif
}

void _rx_video_output_stop_video_player()
{
   char szComm[512];
      
   if ( s_iPIDVideoPlayer > 0 )
   {
      log_line("[VideoOutput] Stoping video player by signaling (PID %d)...", s_iPIDVideoPlayer);
      int iRet = kill(s_iPIDVideoPlayer, SIGTERM);
      if ( iRet < 0 )
         log_line("[VideoOutput] Failed to stop video player, error number: %d, [%s]", errno, strerror(errno));
   }
   else if ( 0 != s_szOutputVideoPlayerFilename[0] )
   {
      log_line("[VideoOutput] Stoping video player (%s) by command...", s_szOutputVideoPlayerFilename);
      sprintf(szComm, "ps -ef | nice grep '%s' | nice grep -v grep | awk '{print $2}' | xargs kill -9 2>/dev/null", s_szOutputVideoPlayerFilename);
      hw_execute_bash_command(szComm, NULL);
   }
   s_iPIDVideoPlayer = -1;
   log_line("[VideoOutput] Executed command to stop video player");
}

static void * _thread_rx_video_output_player(void *argument)
{
   log_line("[VideoOutputThread] Starting worker thread for video player output watchdog");
   bool bMustRestartPlayer = false;
   sem_t* pSemaphore = sem_open(SEMAPHORE_RESTART_VIDEO_PLAYER, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == pSemaphore )
   {
      log_error_and_alarm("[VideoOutputThread] Thread failed to open semaphore: %s", SEMAPHORE_RESTART_VIDEO_PLAYER);
      return NULL;
   }
   else
      log_line("[VideoOutputThread] Thread opened semaphore for signaling video player restart");

   struct timespec tWait = {0, 100*1000*1000}; // 100 ms
   
   log_line("[VideoOutputThread] Started worker thread for video player output");
   
   while ( ! s_bRxVideoOutputPlayerThreadMustStop )
   {
      hardware_sleep_ms(50);
      if ( s_bRxVideoOutputPlayerThreadMustStop )
         break;
      tWait.tv_nsec = 100*1000*1000;
      int iRet = sem_timedwait(pSemaphore, &tWait);
      if ( ETIMEDOUT == iRet )
         continue;
   
      if ( 0 == iRet )
         log_line("[VideoOutputThread] Semaphore to restart video player was set.");

      if ( (0 != iRet) && (!s_bRxVideoOutputPlayerMustReinitialize) )
         continue;

      bMustRestartPlayer = false;

      if ( s_bRxVideoOutputPlayerMustReinitialize )
         bMustRestartPlayer = true;

      int iVal = 0;
      if ( 0 == sem_getvalue(pSemaphore, &iVal) )
      if ( iVal > 0 )
         bMustRestartPlayer = true;

      if ( bMustRestartPlayer )
      {
         log_line("[VideoOutputThread] Thread: Semaphore to restart video player is signaled.");
         _rx_video_output_stop_video_player();
         _rx_video_output_launch_video_player();
         s_bRxVideoOutputPlayerMustReinitialize = false;
      }

      for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
      {
         if ( NULL == g_pVideoProcessorRxList[i] )
            break;
         g_pVideoProcessorRxList[i]->resumeProcessing();
      }
   }

   if ( NULL != pSemaphore )
      sem_close(pSemaphore);
   pSemaphore = NULL;

   log_line("[VideoOutputThread] Stopped worker thread for video player output.");
   return NULL;
}

void _processor_rx_video_forward_open_eth_pipe()
{
   log_line("[VideoOutput] Creating ETH pipes for video forward...");
   char szComm[1024];
   #ifdef HW_CAPABILITY_IONICE
   sprintf(szComm, "ionice -c 1 -n 4 nice -n -5 cat %s | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=3 ! udpsink port=%d host=127.0.0.1 > /dev/null 2>&1 &", FIFO_RUBY_STATION_ETH_VIDEO_STREAM, g_pControllerSettings->nVideoForwardETHPort);
   #else
   sprintf(szComm, "nice -n -5 cat %s | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=3 ! udpsink port=%d host=127.0.0.1 > /dev/null 2>&1 &", FIFO_RUBY_STATION_ETH_VIDEO_STREAM, g_pControllerSettings->nVideoForwardETHPort);
   #endif
   hw_execute_bash_command(szComm, NULL);

   log_line("[VideoOutput] Opening video output pipe write endpoint for ETH forward RTS: %s", FIFO_RUBY_STATION_ETH_VIDEO_STREAM);
   s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = open(FIFO_RUBY_STATION_ETH_VIDEO_STREAM, O_WRONLY);
   if ( s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile < 0 )
   {
      log_error_and_alarm("[VideoOutput] Failed to open video output pipe write endpoint for ETH forward RTS: %s",FIFO_RUBY_STATION_ETH_VIDEO_STREAM);
      return;
   }
   log_line("[VideoOutput] Opened video output pipe write endpoint for ETH forward RTS: %s", FIFO_RUBY_STATION_ETH_VIDEO_STREAM);
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


void rx_video_output_init()
{
   log_line("[VideoOutput] Init start...");
   s_szOutputVideoPlayerFilename[0] = 0;
   s_fPipeVideoOutToPlayer = -1;
   s_iLocalVideoPlayerUDPSocket = -1;
   s_uLastVideoFrameTime= MAX_U32;
   s_bDidSentAnyDataToVideoPlayerPipe = false;
   
   s_ParserH264Output.init(camera_get_active_camera_h264_slices(g_pCurrentModel));
   
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
   s_bRxVideoOutputPlayerMustReinitialize = false;
   if ( 0 != pthread_create(&s_pThreadRxVideoOutputPlayer, NULL, &_thread_rx_video_output_player, NULL) )
      log_error_and_alarm("[VideoOutput] Failed to create thread for video player output check");

   rx_video_recording_init();

   s_uLastIOErrorAlarmFlagsVideoPlayer = 0;
   s_uLastIOErrorAlarmFlagsUSBPlayer = 0;
   s_uTimeLastOkVideoPlayerOutput = 0;
   s_uTimeStartGettingVideoIOErrors = 0;

   log_line("[VideoOutput] Init start complete.");
}

void rx_video_output_uninit()
{
   log_line("[VideoOutput] Uninit start...");

   if ( -1 != s_iLocalVideoPlayerUDPSocket )
   {
      close(s_iLocalVideoPlayerUDPSocket);
      s_iLocalVideoPlayerUDPSocket = -1;
      log_line("[VideoOutput] Closed local socket for local video player UDP output.");
   }

   s_bRxVideoOutputPlayerThreadMustStop = true;
   rx_video_output_signal_restart_player();
 
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
   s_bDidSentAnyDataToVideoPlayerPipe = false;

   if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
      close(s_VideoUSBOutputInfo.socketUSBOutput);
   s_VideoUSBOutputInfo.socketUSBOutput = -1;
   s_VideoUSBOutputInfo.bVideoUSBTethering = false;
   s_VideoUSBOutputInfo.usbBufferPos = 0;

   rx_video_recording_uninit();

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

   //if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
   //if ( 0 != fcntl(s_fPipeVideoOutToPlayer, F_SETFL, O_NONBLOCK) )
   //   log_softerror_and_alarm("[IPC] Failed to set nonblock flag on PIC channel %s write endpoint.", FIFO_RUBY_STATION_VIDEO_STREAM);

   log_line("[IPC] Video player FIFO write endpoint pipe flags: %s", str_get_pipe_flags(fcntl(s_fPipeVideoOutToPlayer, F_GETFL)));
  
   log_line("[VideoOutput] Video player FIFO default size: %d bytes", fcntl(s_fPipeVideoOutToPlayer, F_GETPIPE_SZ));

   //fcntl(s_fPipeVideoOutToPlayer, F_SETPIPE_SZ, 9000);
   //log_line("[VideoOutput] Video player FIFO new size: %d bytes", fcntl(s_fPipeVideoOutToPlayer, F_GETPIPE_SZ));
   s_bDidSentAnyDataToVideoPlayerPipe = false;
}

void rx_video_output_disable_pipe_output()
{
   log_line("[VideoOutput] Disable video pipe output.");
   if ( -1 != s_fPipeVideoOutToPlayer )
   {
      close( s_fPipeVideoOutToPlayer );
      log_line("[VideoOutput] Closed video output to pipe.");
   }
   s_fPipeVideoOutToPlayer = -1;
   s_bDidSentAnyDataToVideoPlayerPipe = false;
}

void rx_video_output_enable_local_player_udp_output()
{
   log_line("[VideoOutput] Enabling video output to local video player UDP port...");

   if ( -1 != s_iLocalVideoPlayerUDPSocket )
   {
      log_line("[VideoOutput] Local output to video player UDP port already enabled.");
      return;
   }

   s_iLocalVideoPlayerUDPSocket = socket(AF_INET, SOCK_DGRAM, 0);
   if ( s_iLocalVideoPlayerUDPSocket < 0 )
   {
      log_error_and_alarm("[VideoOutput] Failed to create local video player UDP video output socket.");
      return;
   }
   bzero((char *) &s_LocalVideoPlayuerUDPSocketAddr, sizeof(s_LocalVideoPlayuerUDPSocketAddr));
   s_LocalVideoPlayuerUDPSocketAddr.sin_family = AF_INET;
   s_LocalVideoPlayuerUDPSocketAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
   s_LocalVideoPlayuerUDPSocketAddr.sin_port = htons((unsigned short)DEFAULT_LOCAL_VIDEO_PLAYER_UDP_PORT);

   if ( connect(s_iLocalVideoPlayerUDPSocket, (struct sockaddr *) &s_LocalVideoPlayuerUDPSocketAddr, sizeof(s_LocalVideoPlayuerUDPSocketAddr)) < 0 )
   {
      log_error_and_alarm("[VideoOutput] Failed to connect local video player UDP video output socket.");
      s_iLocalVideoPlayerUDPSocket = -1;
   }
   log_line("[VideoOutput] Created socket for local video player UDP output on port %d, fd=%d", DEFAULT_LOCAL_VIDEO_PLAYER_UDP_PORT, s_iLocalVideoPlayerUDPSocket);
}

void rx_video_output_disable_local_player_udp_output()
{
   if ( -1 != s_iLocalVideoPlayerUDPSocket )
   {
      close(s_iLocalVideoPlayerUDPSocket);
      log_line("[VideoOutput] Closed socket for local video player UDP output.");
   }
   s_iLocalVideoPlayerUDPSocket = -1;
}

void _processor_rx_video_forward_parse_h264_stream(u8* pBuffer, int length)
{
   bool bStartOfFrameDetected = s_ParserH264Output.parseData(pBuffer, length, g_TimeNow);
   if ( ! bStartOfFrameDetected )
      return;
   
   u32 uLastFrameDuration = s_ParserH264Output.getTimeDurationOfLastCompleteFrame();
   if ( uLastFrameDuration > 127 )
      uLastFrameDuration = 127;
   if ( uLastFrameDuration < 1 )
      uLastFrameDuration = 1;

   u32 uLastFrameSize = s_ParserH264Output.getSizeOfLastCompleteFrame();
   uLastFrameSize /= 1000; // transform to kbytes

   if ( uLastFrameSize > 127 )
      uLastFrameSize = 127; // kbytes

   g_SM_VideoInfoStatsOutput.uLastIndex = (g_SM_VideoInfoStatsOutput.uLastIndex+1) % MAX_FRAMES_SAMPLES;
   g_SM_VideoInfoStatsOutput.uFramesDuration[g_SM_VideoInfoStatsOutput.uLastIndex] = uLastFrameDuration;
   g_SM_VideoInfoStatsOutput.uFramesTypesAndSizes[g_SM_VideoInfoStatsOutput.uLastIndex] = (g_SM_VideoInfoStatsOutput.uFramesTypesAndSizes[g_SM_VideoInfoStatsOutput.uLastIndex] & 0x80) | ((u8)uLastFrameSize);
 
   u32 uNextIndex = (g_SM_VideoInfoStatsOutput.uLastIndex+1) % MAX_FRAMES_SAMPLES;
  
   if ( s_ParserH264Output.IsInsideIFrame() )
      g_SM_VideoInfoStatsOutput.uFramesTypesAndSizes[uNextIndex] |= (1<<7);
   else
      g_SM_VideoInfoStatsOutput.uFramesTypesAndSizes[uNextIndex] &= 0x7F;

   g_SM_VideoInfoStatsOutput.uKeyframeIntervalMs = s_ParserH264Output.getCurrentlyDetectedKeyframeIntervalMs();
   g_SM_VideoInfoStatsOutput.uDetectedFPS = s_ParserH264Output.getDetectedFPS();
   g_SM_VideoInfoStatsOutput.uDetectedSlices = (u32) s_ParserH264Output.getDetectedSlices();
}

void _rx_video_output_to_video_player(u32 uVehicleId, int width, int height, u8* pBuffer, int length)
{
   if ( (-1 == s_fPipeVideoOutToPlayer) || s_bRxVideoOutputPlayerMustReinitialize )
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
      rx_video_output_signal_restart_player();
      log_line("[VideoOutput] Signaled restart of player");
      return;
   }

   if ( ! s_bDidSentAnyDataToVideoPlayerPipe )
   {
      log_line("[VideoOutput] Send first data to video output pipe for local video player");
      s_bDidSentAnyDataToVideoPlayerPipe = true;
   }
   
   int iRes = write(s_fPipeVideoOutToPlayer, pBuffer, length);
   if ( iRes == length )
   {
      s_uOutputBitrateToLocalVideoPlayerPipe += iRes*8;
      s_uTimeStartGettingVideoIOErrors = 0;
      if ( 0 != s_uLastIOErrorAlarmFlagsVideoPlayer )
      {
         s_uTimeLastOkVideoPlayerOutput = g_TimeNow;
         s_uLastIOErrorAlarmFlagsVideoPlayer = 0;
         send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, 0,0);
      }
      //fsync(s_fPipeVideoOutToPlayer);
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

   if ( g_TimeNow > s_uTimeStartGettingVideoIOErrors + 1000 )
   {
      if ( g_TimeNow > uTimeLastVideoChanged + 3000 )
         send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, uFlags, 0);

      if ( (0 != s_uTimeLastOkVideoPlayerOutput) && (g_TimeNow > s_uTimeLastOkVideoPlayerOutput + 3000) )
      {
         log_line("[VideoOutput] Error outputing video data to video player. Reinitialize video player...");
         rx_video_output_signal_restart_player();
         log_line("[VideoOutput] Signaled restart of player");
      }
      if ( 0 == s_uTimeLastOkVideoPlayerOutput && g_TimeNow > g_TimeStart + 5000 )
      {
         log_line("[VideoOutput] Error outputing any video data to video player. Reinitialize video player...");
         rx_video_output_signal_restart_player();
         log_line("[VideoOutput] Signaled restart of player");
      }
   }
}

void _rx_video_output_to_local_video_player_udp(u8* pData, int iLength)
{
   s_uOutputBitrateToLocalVideoPlayerUDP += iLength*8;
   
   sendto(s_iLocalVideoPlayerUDPSocket, pData, iLength, 0, (struct sockaddr *)&s_LocalVideoPlayuerUDPSocketAddr, sizeof(s_LocalVideoPlayuerUDPSocketAddr) );
   /*
   struct iovec iov;
   struct msghdr msghdr;
   iov.iov_base = pData;
   iov.iov_len = iLength;
   
   memset(&msghdr, 0, sizeof(msghdr));
   msghdr.msg_iov = &iov;
   msghdr.msg_iovlen = 1;

   sendmsg(s_iLocalVideoPlayerUDPSocket, &msghdr, MSG_DONTWAIT);
   */
}

void _rx_video_output_to_eth(u8* pData, int iLength)
{
   int dataLen = iLength;
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
             log_line("[VideoOutput] Failed to send to ETH Port %d bytes, [fd=%d]", iLength, s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
             close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
             s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
          }
          //else
          //   log_line("Sent %d bytes to port %d", video_data_length, g_pControllerSettings->nVideoForwardETHPort);
          s_VideoETHOutputInfo.s_nBufferETHPos = 0;
       }
   }
}

void _rx_video_output_to_usb(u8* pData, int iLength)
{
   int dataLen = iLength;
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

void rx_video_output_video_data(u32 uVehicleId, u8 uVideoStreamType, int width, int height, u8* pBuffer, int video_data_length, int packet_length)
{
   if ( g_bSearching )
      return;

   if ( g_pCurrentModel->bDeveloperMode )
   if ( packet_length > video_data_length )
   {
      u8* pExtraData = pBuffer + video_data_length;
      u32* pExtraDataU32 = (u32*)pExtraData;
      pExtraDataU32[6] = get_current_timestamp_ms();
      
      /*
      u32 uVehicleTimestampOrg = pExtraDataU32[3];
               int iVehicleTimestampNow = (int)uVehicleTimestampOrg + radio_get_link_clock_delta();
               if ( (int)uTimeNow - (int)iVehicleTimestampNow > 3 )
                  log_line("DEBUG (%s)[%u/%d]%d rx v-org: %u, now-loc: %u, tr-time: %u, dclks: %d",
                    (pPHVF->uEncodingFlags2 & VIDEO_ENCODING_FLAGS2_IS_IFRAME)?"I":"P",
                    pPHVF->video_block_index, (int)pPHVF->video_block_packet_index, iCountReads,
                    uVehicleTimestampOrg, uTimeNow,
                    (int)uTimeNow - (int)iVehicleTimestampNow, radio_get_link_clock_delta());
     */
   }

   if ( NULL != g_pCurrentModel )
   if ( uVideoStreamType == VIDEO_TYPE_H264 )
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_STATS_VIDEO_KEYFRAMES_INFO)
   if ( get_ControllerSettings()->iShowVideoStreamInfoCompactType == 0 )
   {
      _processor_rx_video_forward_parse_h264_stream(pBuffer, video_data_length);
   }

   if ( -1 != s_fPipeVideoOutToPlayer ) 
      _rx_video_output_to_video_player(uVehicleId, width, height, pBuffer, video_data_length);
   if ( -1 != s_iLocalVideoPlayerUDPSocket )
      _rx_video_output_to_local_video_player_udp(pBuffer, video_data_length);

   if ( s_VideoETHOutputInfo.s_bForwardETHPipeEnabled && (-1 != s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile) )
      write(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile, pBuffer, video_data_length);

   rx_video_recording_on_new_data(pBuffer, video_data_length);

   if ( s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled && (-1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo ) )
      _rx_video_output_to_eth(pBuffer, video_data_length);

   if ( s_VideoUSBOutputInfo.bVideoUSBTethering && 0 != s_VideoUSBOutputInfo.szIPUSBVideo[0] )
      _rx_video_output_to_usb(pBuffer, video_data_length);
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

void rx_video_output_signal_restart_player()
{
   if ( NULL != s_pRxVideoSemaphoreRestartVideoPlayer )
   {
      s_bRxVideoOutputPlayerMustReinitialize = true;
      sem_post(s_pRxVideoSemaphoreRestartVideoPlayer);
      log_line("[VideoOutput] Signaled restart of player on request.");
   }
   else
      log_softerror_and_alarm("[VideoOutput] Can't signal video player to restart. No signal.");

   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL == g_pVideoProcessorRxList[i] )
         break;
      g_pVideoProcessorRxList[i]->pauseProcessing();
   }
}

void rx_video_output_periodic_loop()
{
   rx_video_recording_periodic_loop();

   if ( g_bDebugState )
   if ( g_TimeNow >= s_uLastTimeComputedOutputBitrate + 1000 )
   {
      log_line("[VideoOutput] Output to pipe: %u bps, output to UDP: %u bps", s_uOutputBitrateToLocalVideoPlayerPipe, s_uOutputBitrateToLocalVideoPlayerUDP );
      s_uLastTimeComputedOutputBitrate = g_TimeNow;
      s_uOutputBitrateToLocalVideoPlayerPipe = 0;
      s_uOutputBitrateToLocalVideoPlayerUDP = 0;
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
}
