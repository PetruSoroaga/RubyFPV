/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>

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
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/camera_utils.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"

#include "shared_vars.h"
#include "processor_rx_video_forward.h"
#include "packets_utils.h"
#include "links_utils.h"
#include "timers.h"

extern shared_mem_video_decode_stats s_VDStatsCache; // local copy to update faster

int s_fPipeVideoOutToPlayer = -1;

u32 s_uParseNALStartSequence = 0xFFFFFFFF;
u32 s_uParseNALCurrentSlices = 0;
u32 s_uParseLastNALTag = 0xFFFFFFFF;
u32 s_uParseNALTotalParsedBytes = 0;

u32 s_uLastIOErrorAlarmFlagsVideoPlayer = 0;
u32 s_uLastIOErrorAlarmFlagsUSBPlayer = 0;
u32 s_uTimeLastOkVideoPlayerOutput = 0;

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

static int s_iLastUSBVideoForwardPort = -1;
static int s_iLastUSBVideoForwardPacketSize = 0;

sem_t* s_pSemaphoreStartRecord = NULL; 
sem_t* s_pSemaphoreStopRecord = NULL; 
bool s_bRecording = false;

u32 s_TimeStartRecording = MAX_U32;
char s_szFileRecording[1024];
int s_iFileVideo = -1;

u32 s_TimeLastPeriodicChecksVideoRecording = 0;
u32 s_TimeLastPeriodicChecksUSBForward = 0;

u32 s_uLastVideoFrameTime = MAX_U32;

void _processor_rx_video_forward_open_eth_pipe()
{
   log_line("Creating ETH pipes for video forward...");
   char szComm[1024];
   sprintf(szComm, "ionice -c 1 -n 4 nice -n -5 cat %s | nice -n -5 gst-launch-1.0 fdsrc ! h264parse ! rtph264pay pt=96 config-interval=3 ! udpsink port=%d host=127.0.0.1 > /dev/null 2>&1 &", FIFO_RUBY_STATION_VIDEO_STREAM_ETH, g_pControllerSettings->nVideoForwardETHPort);
   hw_execute_bash_command(szComm, NULL);

   log_line("Opening video output pipe write endpoint for ETH forward RTS: %s", FIFO_RUBY_STATION_VIDEO_STREAM_ETH);
   s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile = open(FIFO_RUBY_STATION_VIDEO_STREAM_ETH, O_WRONLY);
   if ( s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile < 0 )
   {
      log_error_and_alarm("Failed to open video output pipe write endpoint for ETH forward RTS: %s",FIFO_RUBY_STATION_VIDEO_STREAM_ETH);
      return;
   }
   log_line("Opened video output pipe write endpoint for ETH forward RTS: %s", FIFO_RUBY_STATION_VIDEO_STREAM_ETH);
   log_line("Video output pipe to ETH flags: %s", str_get_pipe_flags(fcntl(s_VideoETHOutputInfo.s_ForwardETHVideoPipeFile, F_GETFL)));
   s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = true;
}

void _processor_rx_video_forward_create_eth_socket()
{
   log_line("Creating ETH socket for video forward...");
   if ( -1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo )
      close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);

   s_VideoETHOutputInfo.s_ForwardETHSocketVideo = socket(AF_INET, SOCK_DGRAM, 0);
   if ( s_VideoETHOutputInfo.s_ForwardETHSocketVideo <= 0 )
   {
      log_softerror_and_alarm("Failed to create socket for video forward on ETH.");
      return;
   }

   int broadcastEnable = 1;
   int ret = setsockopt(s_VideoETHOutputInfo.s_ForwardETHSocketVideo, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
   if ( ret != 0 )
   {
      log_softerror_and_alarm("Failed to set the Video ETH forward socket broadcast flag.");
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

   log_line("Opened socket [fd=%d] for video forward on ETH on port %d.", s_VideoETHOutputInfo.s_ForwardETHSocketVideo, g_pControllerSettings->nVideoForwardETHPort);
   s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = true;
}


void _start_recording()
{
   if ( s_bRecording )
      return;

   log_line("Received request to start recording video.");

   s_TimeStartRecording = get_current_timestamp_ms();
   strcpy(s_szFileRecording, TEMP_VIDEO_FILE);

   load_Preferences();
   Preferences* p = get_Preferences();
   if ( p->iVideoDestination == 1 )
   {
      strcpy(s_szFileRecording, TEMP_VIDEO_MEM_FILE);
      char szComm[256];
      char szBuff[2048];
      char szTemp[1024];
   
      sprintf(szComm, "umount %s", TEMP_VIDEO_MEM_FOLDER);
      hw_execute_bash_command_silent(szComm, NULL);

      hw_execute_bash_command_raw("free -m  | grep Mem", szBuff);
      long lt, lu, lf;
      sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lt, &lu, &lf);
      lf -= 200;
      if ( lf < 50 )
         return;

      sprintf(szComm, "mount -t tmpfs -o size=%dM tmpfs %s ", (int)lf, TEMP_VIDEO_MEM_FOLDER);
      hw_execute_bash_command(szComm, NULL);
   }

   if ( s_iFileVideo > 0 )
      close(s_iFileVideo);

   s_iFileVideo = open(s_szFileRecording, O_CREAT | O_WRONLY | O_NONBLOCK);
   if ( -1 == s_iFileVideo )
   {
      FILE* fd = fopen(TEMP_VIDEO_FILE_PROCESS_ERROR, "a");
      fprintf(fd, "%s%s\n", "Failed to create video recording file ", s_szFileRecording);
      fclose(fd);
   }

   if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
   if ( 0 != fcntl(s_iFileVideo, F_SETFL, O_NONBLOCK) )
      log_softerror_and_alarm("Failed to set nonblock flag on video recording file");

   log_line("Video recording file flags: %s", str_get_pipe_flags(fcntl(s_iFileVideo, F_GETFL)));
  
   s_bRecording = true;
}

void _stop_recording()
{
   if ( -1 != s_iFileVideo )
      close(s_iFileVideo);
   s_iFileVideo = -1;

   log_line("Received request to stop recording video.");
   if ( ! s_bRecording )
   {
      log_line("Not recording. Do nothing.");
      return;
   }
   u32 duration_ms = get_current_timestamp_ms() - s_TimeStartRecording;

   log_line("Writing video info file %s ...", TEMP_VIDEO_FILE_INFO);
   FILE* fd = fopen(TEMP_VIDEO_FILE_INFO, "w");
   if ( NULL == fd )
   {
      system("sudo mount -o remount,rw /");
      char szTmp[128];
      sprintf(szTmp, "rm -rf %s", TEMP_VIDEO_FILE_INFO);
      hw_execute_bash_command(szTmp, NULL);
      fd = fopen(TEMP_VIDEO_FILE_INFO, "w");
   }

   if ( NULL == fd )
   {
      FILE* fd = fopen(TEMP_VIDEO_FILE_PROCESS_ERROR, "a");
      fprintf(fd, "%s\n", "Failed to create video recording info file.");
      fclose(fd);
      log_softerror_and_alarm("Failed to create video info file %s", TEMP_VIDEO_FILE_INFO);
   }
   else
   {
      fprintf(fd, "%s\n", s_szFileRecording);
      fprintf(fd, "%d %d\n", s_VDStatsCache.fps, duration_ms/1000 );
      fprintf(fd, "%d %d\n", s_VDStatsCache.width, s_VDStatsCache.height );
      fclose(fd);
   }

   log_line("Created video info file %s", TEMP_VIDEO_FILE_INFO);
   log_line("Video info file content: (%s, %d, %d seconds, %d, %d)", s_szFileRecording, s_VDStatsCache.fps, duration_ms/1000, s_VDStatsCache.width, s_VDStatsCache.height);
   hw_execute_bash_command("./ruby_video_proc &", NULL);
   s_bRecording = false;
   s_szFileRecording[0] = 0;
   log_line("Recording stopped.");
}

void processor_rx_video_forward_init()
{
   s_fPipeVideoOutToPlayer = -1;
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
      log_line("Video ETH forwarding is enabled, type Raw.");
      _processor_rx_video_forward_open_eth_pipe();
   }
   if ( s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled )
   {
      log_line("Video ETH forwarding is enabled, type RTS.");
      _processor_rx_video_forward_create_eth_socket();
   }

   log_line("Opening video output pipe write endpoint: %s", FIFO_RUBY_STATION_VIDEO_STREAM);
   s_fPipeVideoOutToPlayer = open(FIFO_RUBY_STATION_VIDEO_STREAM, O_WRONLY);
   if ( s_fPipeVideoOutToPlayer < 0 )
   {
      log_error_and_alarm("Failed to open video output pipe write endpoint: %s",FIFO_RUBY_STATION_VIDEO_STREAM);
      return;
   }
   log_line("Opened video output pipe to player write endpoint: %s", FIFO_RUBY_STATION_VIDEO_STREAM);
   log_line("Video output pipe to player flags: %s", str_get_pipe_flags(fcntl(s_fPipeVideoOutToPlayer, F_GETFL)));

   if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
   if ( 0 != fcntl(s_fPipeVideoOutToPlayer, F_SETFL, O_NONBLOCK) )
      log_softerror_and_alarm("[IPC] Failed to set nonblock flag on PIC channel %s write endpoint.", FIFO_RUBY_STATION_VIDEO_STREAM);

   log_line("[IPC] Video player FIFO write endpoint pipe flags: %s", str_get_pipe_flags(fcntl(s_fPipeVideoOutToPlayer, F_GETFL)));
  
   log_line("Video player FIFO default size: %d bytes", fcntl(s_fPipeVideoOutToPlayer, F_GETPIPE_SZ));

   fcntl(s_fPipeVideoOutToPlayer, F_SETPIPE_SZ, 256000);

   log_line("Video player FIFO new size: %d bytes", fcntl(s_fPipeVideoOutToPlayer, F_GETPIPE_SZ));

   s_pSemaphoreStartRecord = sem_open(SEMAPHORE_START_VIDEO_RECORD, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == s_pSemaphoreStartRecord )
      log_error_and_alarm("Failed to open semaphore for reading: %s", SEMAPHORE_START_VIDEO_RECORD);

   s_pSemaphoreStopRecord = sem_open(SEMAPHORE_STOP_VIDEO_RECORD, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == s_pSemaphoreStopRecord )
      log_error_and_alarm("Failed to open semaphore for reading: %s", SEMAPHORE_STOP_VIDEO_RECORD);

   s_uLastIOErrorAlarmFlagsVideoPlayer = 0;
   s_uLastIOErrorAlarmFlagsUSBPlayer = 0;
   s_uTimeLastOkVideoPlayerOutput = 0;
}

void processor_rx_video_forward_uninit()
{
   _stop_recording();

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
      close( s_fPipeVideoOutToPlayer );
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
}

void processor_rx_video_forware_prepare_video_stream_write()
{

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
    if ( iFoundNALVideoFrame == 5 )
    {
       g_VideoInfoStats.uKeyframeInterval = g_VideoInfoStats.uTmpKeframeDeltaFrames;
       g_VideoInfoStats.uTmpKeframeDeltaFrames = 0; 
    }

    s_uLastVideoFrameTime = g_TimeNow;
    g_VideoInfoStats.uTmpCurrentFrameSize = length-iFoundPosition;      
}

void processor_rx_video_forward_video_data(u8* pBuffer, int length)
{
   // Find start of video frame H264 NAL unit

   if ( (! g_bSearching) && ( NULL != g_pCurrentModel ) )
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_STATS_VIDEO_INFO)
      _processor_rx_video_forward_parse_h264_stream(pBuffer, length);

   if ( -1 != s_fPipeVideoOutToPlayer )
   {
      int iRes = write(s_fPipeVideoOutToPlayer, pBuffer, length);
      if ( iRes != length )
      {
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

         if ( g_TimeNow > process_data_rx_video_get_last_time_video_changed() + 5000 )
            send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, uFlags,1);
        
         s_uLastIOErrorAlarmFlagsVideoPlayer = uFlags;

         if ( (0 != s_uTimeLastOkVideoPlayerOutput) && (g_TimeNow > s_uTimeLastOkVideoPlayerOutput + 3000) )
         {
            processor_rx_video_forward_uninit();
            processor_rx_video_forward_init();
            //controller_stop_video_player();
            //controller_launch_video_player();
         }
         if ( 0 == s_uTimeLastOkVideoPlayerOutput && g_TimeNow > g_TimeStart + 5000 )
         {
            processor_rx_video_forward_uninit();
            processor_rx_video_forward_init();
            //controller_stop_video_player();
            //controller_launch_video_player();          
         }
      }
      else if ( 0 != s_uLastIOErrorAlarmFlagsVideoPlayer )
      {
         s_uTimeLastOkVideoPlayerOutput = g_TimeNow;
         s_uLastIOErrorAlarmFlagsVideoPlayer = 0;
         send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, 0,1);
      }
   }

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

         if ( g_TimeNow > process_data_rx_video_get_last_time_video_changed() + 5000 )
            send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, uFlags,1);
        
         s_uLastIOErrorAlarmFlagsUSBPlayer = uFlags;
      }
      else if ( 0 != s_uLastIOErrorAlarmFlagsUSBPlayer )
      {
         s_uLastIOErrorAlarmFlagsUSBPlayer = 0;
         send_alarm_to_central(ALARM_ID_CONTROLLER_IO_ERROR, 0,1);
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
                log_line("Failed to send to ETH Port %d bytes, [fd=%d]", length, s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
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
              log_line("Failed to send to USB socket");
              if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
                 close(s_VideoUSBOutputInfo.socketUSBOutput);
              s_VideoUSBOutputInfo.socketUSBOutput = -1;
              s_VideoUSBOutputInfo.bVideoUSBTethering = false;
              s_VideoUSBOutputInfo.usbBufferPos = 0;
              log_line("Video Output to USB disabled.");
              break;
            }
            s_VideoUSBOutputInfo.usbBufferPos = 0;
         }
      }
   }
}


void processor_rx_video_forward_check_controller_settings_changed()
{
   if ( s_iLastUSBVideoForwardPort != g_pControllerSettings->iVideoForwardUSBPort ||
        s_iLastUSBVideoForwardPacketSize != g_pControllerSettings->iVideoForwardUSBPacketSize )
   if ( s_VideoUSBOutputInfo.bVideoUSBTethering )
   {
      if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
         close(s_VideoUSBOutputInfo.socketUSBOutput);
      s_VideoUSBOutputInfo.socketUSBOutput = -1;
      s_VideoUSBOutputInfo.bVideoUSBTethering = false;
      log_line("Video Output to USB disabled due to settings changed.");
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
      log_line("Video ETH forwarding was disabled.");
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

      log_line("Video ETH forwarding is enabled, type Raw.");
      _processor_rx_video_forward_create_eth_socket();
   }
   else if ( g_pControllerSettings->nVideoForwardETHType == 2 )
   {
      if ( -1 != s_VideoETHOutputInfo.s_ForwardETHSocketVideo )
         close(s_VideoETHOutputInfo.s_ForwardETHSocketVideo);
      s_VideoETHOutputInfo.s_ForwardETHSocketVideo = -1;
      s_VideoETHOutputInfo.s_bForwardIsETHForwardEnabled = false;

      s_VideoETHOutputInfo.s_bForwardETHPipeEnabled = true;
      log_line("Video ETH forwarding is enabled, type RTS.");
      _processor_rx_video_forward_open_eth_pipe();
   }

   s_iLastUSBVideoForwardPort = g_pControllerSettings->iVideoForwardUSBPort;
   s_iLastUSBVideoForwardPacketSize = g_pControllerSettings->iVideoForwardUSBPacketSize;
}


void processor_rx_video_forward_loop()
{
   if ( g_TimeNow > s_TimeLastPeriodicChecksUSBForward + 50 )
   {
      s_TimeLastPeriodicChecksUSBForward = g_TimeNow;

      // Stopped USB forward?
      if ( s_VideoUSBOutputInfo.bVideoUSBTethering && g_pControllerSettings->iVideoForwardUSBType == 0 )
      {
         if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
            close(s_VideoUSBOutputInfo.socketUSBOutput);
         s_VideoUSBOutputInfo.socketUSBOutput = -1;
         s_VideoUSBOutputInfo.bVideoUSBTethering = false;
         log_line("Video Output to USB disabled.");
      }

      if ( g_pControllerSettings->iVideoForwardUSBType != 0 )
      if ( g_TimeNow > s_VideoUSBOutputInfo.TimeLastVideoUSBTetheringCheck + 1000 )
      {
         s_VideoUSBOutputInfo.TimeLastVideoUSBTetheringCheck = g_TimeNow;
         if ( ! s_VideoUSBOutputInfo.bVideoUSBTethering )
         if ( access(TEMP_USB_TETHERING_DEVICE, R_OK) != -1 )
         {
         s_VideoUSBOutputInfo.szIPUSBVideo[0] = 0;
         FILE* fd = fopen(TEMP_USB_TETHERING_DEVICE, "r");
         if ( NULL != fd )
         {
            fscanf(fd, "%s", s_VideoUSBOutputInfo.szIPUSBVideo);
            fclose(fd);
         }
         log_line("USB Device Tethered for Video Output. Device IP: %s", s_VideoUSBOutputInfo.szIPUSBVideo);

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
         if ( access(TEMP_USB_TETHERING_DEVICE, R_OK) == -1 )
         {
            log_line("Tethered USB Device for Video Output Unplugged.");
            if ( -1 != s_VideoUSBOutputInfo.socketUSBOutput )
               close(s_VideoUSBOutputInfo.socketUSBOutput);
            s_VideoUSBOutputInfo.socketUSBOutput = -1;
            s_VideoUSBOutputInfo.usbBufferPos = 0;
            s_VideoUSBOutputInfo.bVideoUSBTethering = false;
         }
      }
   }

   if ( g_TimeNow >= s_TimeLastPeriodicChecksVideoRecording + 100 )
   {
      s_TimeLastPeriodicChecksVideoRecording = g_TimeNow;

      int val = 0;
      if ( NULL != s_pSemaphoreStartRecord )
      if ( 0 == sem_getvalue(s_pSemaphoreStartRecord, &val) )
      if ( 0 < val )
      if ( EAGAIN != sem_trywait(s_pSemaphoreStartRecord) )
      {
      log_line("Event to start recording is set.");
      if ( ! s_bRecording )
      {
         _start_recording();
         log_line("Recording started.");
      }
      else
         log_softerror_and_alarm("Recording is already started.");
      }
   
      val = 0;
      if ( NULL != s_pSemaphoreStopRecord )
      if ( 0 == sem_getvalue(s_pSemaphoreStopRecord, &val) )
      if ( 0 < val )
      if ( EAGAIN != sem_trywait(s_pSemaphoreStopRecord) )
      {
      log_line("Event to stop recording is set.");
      if ( s_bRecording )
      {
         _stop_recording();
         log_line("Recording stopped.");
      }
      else
         log_softerror_and_alarm("Recording is already stopped.");
      }
   }
}
