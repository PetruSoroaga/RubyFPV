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

#include <errno.h>
#include <sys/stat.h>

#include "video_source_csi.h"
#include "launchers_vehicle.h"
#include "packets_utils.h"
#include "events.h"
#include "timers.h"
#include "shared_vars.h"

int s_fInputVideoStreamCSIPipe = -1;
char s_szInputVideoStreamCSIPipeName[128];
bool s_bInputVideoStreamCSIPipeOpenFailed = false;
u8 s_uInputVideoCSIPipeBuffer[MAX_PACKET_PAYLOAD];

bool s_bRequestedVideoCSICaptureRestart = false;
bool s_bVideoCSICaptureProgramStarted = false;
u32  s_uTimeToRestartVideoCapture = 0;
u32  s_uRaspiVidStartTimeMs = 0;
u8*  s_pSharedMemRaspiVidCommands = NULL;
bool s_bDidSentRaspividBitrateRefresh = false;

u32 s_uDebugTimeLastCSIVideoInputCheck = 0;
u32 s_uDebugCSIInputBytes = 0;
u32 s_uDebugCSIInputReads = 0;

void video_source_csi_close()
{
   if ( -1 != s_fInputVideoStreamCSIPipe )
   {
      log_line("[VideoSourceCSI] Closed input pipe.");
      close(s_fInputVideoStreamCSIPipe);
   }
   else
      log_line("[VideoSourceCSI] No input pipe to close.");
   s_fInputVideoStreamCSIPipe = -1;
   s_bInputVideoStreamCSIPipeOpenFailed = false;
}

int video_source_csi_open(const char* szPipeName)
{
   if ( -1 != s_fInputVideoStreamCSIPipe )
      return s_fInputVideoStreamCSIPipe;

   if ( (NULL == szPipeName) || (0 == szPipeName[0]) )
   {
      log_error_and_alarm("[VideoSourceCSI] Tried to open a video input source pipe with an empty name.");
      return s_fInputVideoStreamCSIPipe;
   }
   strcpy(s_szInputVideoStreamCSIPipeName, szPipeName);

   log_line("[VideoSourceCSI] Opening video input stream pipe: %s", s_szInputVideoStreamCSIPipeName);

   s_bInputVideoStreamCSIPipeOpenFailed = true;

   struct stat statsBuff;
   memset(&statsBuff, 0, sizeof(statsBuff));
   int iRes = stat(s_szInputVideoStreamCSIPipeName, &statsBuff);
   if ( 0 != iRes )
   {
      log_error_and_alarm("[VideoSourceCSI] Failed to stat pipe [%s], error: %d, [%s]", s_szInputVideoStreamCSIPipeName, iRes, strerror(errno));
      return -1;
   }

   if ( ! (S_ISFIFO(statsBuff.st_mode)) )
   {
      log_error_and_alarm("[VideoSourceCSI] File [%s] is not a pipe. Not opening it.", s_szInputVideoStreamCSIPipeName);
      return -1;    
   }
   /*
   if ( access( szPipeName, R_OK ) != -1 )
   {
      log_error_and_alarm("[VideoSourceCSI] Pipe [%s] can't be found. Not opening it.", s_szInputVideoStreamCSIPipeName);
      return -1;    
   }
   */

   s_fInputVideoStreamCSIPipe = open( FIFO_RUBY_CAMERA1, O_RDONLY | RUBY_PIPES_EXTRA_FLAGS);
   if ( s_fInputVideoStreamCSIPipe < 0 )
   {
      log_error_and_alarm("[VideoSourceCSI] Failed to open video input stream: %s", s_szInputVideoStreamCSIPipeName);
      return -1;
   }

   s_bInputVideoStreamCSIPipeOpenFailed = false;
   log_line("[VideoSourceCSI] Opened video input stream: %s", s_szInputVideoStreamCSIPipeName);
   log_line("[VideoSourceCSI] Pipe read end flags: %s", str_get_pipe_flags(fcntl(s_fInputVideoStreamCSIPipe, F_GETFL)));
   
   return s_fInputVideoStreamCSIPipe;
}

void video_source_csi_flush_discard()
{
   if ( -1 == s_fInputVideoStreamCSIPipe )
      return;
   
   for( int i=0; i<50; i++ )
   {
      int iReadSize = 0;
      video_source_csi_read(&iReadSize);
   }
}

// Returns the buffer and number of bytes read
u8* video_source_csi_read(int* piReadSize)
{
   if ( (NULL == piReadSize) )
      return NULL;

   *piReadSize = 0;
   if ( -1 == s_fInputVideoStreamCSIPipe )
   {
      if ( s_bInputVideoStreamCSIPipeOpenFailed )
         video_source_csi_open(s_szInputVideoStreamCSIPipeName);
      if ( -1 == s_fInputVideoStreamCSIPipe )
         return NULL;
   }

   fd_set readset;
   FD_ZERO(&readset);
   FD_SET(s_fInputVideoStreamCSIPipe, &readset);

   // Check for exceptions first

   struct timeval timePipeInput;
   timePipeInput.tv_sec = 0;
   timePipeInput.tv_usec = 100; // 0.1 miliseconds timeout

   int iSelectResult = select(s_fInputVideoStreamCSIPipe+1, NULL, NULL, &readset, &timePipeInput);
   if ( iSelectResult > 0 )
   {
      log_softerror_and_alarm("[VideoSourceCSI] Exception on reading input pipe. Closing pipe.");
      close(s_fInputVideoStreamCSIPipe);
      s_fInputVideoStreamCSIPipe = -1;
      return NULL;
   }

   // Try read

   FD_ZERO(&readset);
   FD_SET(s_fInputVideoStreamCSIPipe, &readset);

   timePipeInput.tv_sec = 0;
   timePipeInput.tv_usec = 1000; // 1 miliseconds timeout

   iSelectResult = select(s_fInputVideoStreamCSIPipe+1, &readset, NULL, NULL, &timePipeInput);
   if ( iSelectResult <= 0 )
      return s_uInputVideoCSIPipeBuffer;

   if( 0 == FD_ISSET(s_fInputVideoStreamCSIPipe, &readset) )
      return s_uInputVideoCSIPipeBuffer;

   int iRead = read(s_fInputVideoStreamCSIPipe, s_uInputVideoCSIPipeBuffer, sizeof(s_uInputVideoCSIPipeBuffer)/sizeof(s_uInputVideoCSIPipeBuffer[0]));
   if ( iRead < 0 )
   {
      log_error_and_alarm("[VideoSourceCSI] Failed to read from video input pipe, returned code: %d, error: %s. Closing pipe.", iRead, strerror(errno));
      close(s_fInputVideoStreamCSIPipe);
      s_fInputVideoStreamCSIPipe = -1;
      return NULL;
   }

   s_uDebugCSIInputBytes += iRead;
   s_uDebugCSIInputReads++;

   *piReadSize = iRead;
   return s_uInputVideoCSIPipeBuffer;
}

void video_source_csi_start_program()
{
   s_bVideoCSICaptureProgramStarted = true;

   #ifdef HW_PLATFORM_RASPBERRY
   vehicle_launch_video_capture_csi(g_pCurrentModel, &(g_SM_VideoLinkStats.overwrites));
   vehicle_check_update_processes_affinities(true, g_pCurrentModel->isActiveCameraVeye());
   #endif
   s_uRaspiVidStartTimeMs = g_TimeNow;
   s_bDidSentRaspividBitrateRefresh = false;
}

void video_source_csi_stop_program()
{
   s_bVideoCSICaptureProgramStarted = false;
   s_uRaspiVidStartTimeMs = 0;

   #ifdef HW_PLATFORM_RASPBERRY
   vehicle_stop_video_capture_csi(g_pCurrentModel);
   #endif
   
   if ( NULL != s_pSharedMemRaspiVidCommands )
      munmap(s_pSharedMemRaspiVidCommands, SIZE_OF_SHARED_MEM_RASPIVID_COMM);
   s_pSharedMemRaspiVidCommands = NULL; 
   log_line("[VideoSourceCSI] Closed shared mem to raspi video commands.");

   s_bDidSentRaspividBitrateRefresh = false;
}

bool video_source_csi_is_program_started()
{
   return s_bVideoCSICaptureProgramStarted;
}

u32 video_source_cs_get_program_start_time()
{
   return s_uRaspiVidStartTimeMs;
}

void video_source_csi_request_restart_program()
{
   s_bRequestedVideoCSICaptureRestart = true;
}

bool video_source_csi_is_restart_requested()
{
   return s_bRequestedVideoCSICaptureRestart;
}

void video_source_csi_send_control_message(u8 parameter, u8 value)
{
   if ( (NULL == g_pCurrentModel) || (! g_pCurrentModel->hasCamera()) || (-1 == s_fInputVideoStreamCSIPipe) )
      return;

   #ifdef HW_PLATFORM_RASPBERRY

   if ( (! g_pCurrentModel->isActiveCameraCSICompatible()) && (! g_pCurrentModel->isActiveCameraVeye()) )
   {
      log_softerror_and_alarm("[VideoSourceCSI] Can't signal on the fly video capture parameter change. Capture camera is not raspi or veye.");
      return;
   }

   if ( s_bRequestedVideoCSICaptureRestart )
   {
      log_line("[VideoSourceCSI] Video capture is restarting, do not send command (%d) to video capture program.", parameter);
      return;
   }

   if ( ! s_bVideoCSICaptureProgramStarted )
   {
      log_line("[VideoSourceCSI] Video capture is not started, do not send command (%d) to video capture program.", parameter);
      return;
   }

   if ( parameter == RASPIVID_COMMAND_ID_VIDEO_BITRATE )
   {
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = ((u32)value) * 100000;
      s_bDidSentRaspividBitrateRefresh = true;
      //log_line("Setting video capture bitrate to: %u", g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate );
   }
   if ( NULL == s_pSharedMemRaspiVidCommands )
   {
      log_line("[VideoSourceCSI] Opening video capture program commands pipe write endpoint...");
      s_pSharedMemRaspiVidCommands = (u8*)open_shared_mem_for_write(SHARED_MEM_RASPIVIDEO_COMMAND, SIZE_OF_SHARED_MEM_RASPIVID_COMM);
      if ( NULL == s_pSharedMemRaspiVidCommands )
         log_error_and_alarm("[VideoSourceCSI] Failed to open video capture program commands pipe write endpoint!");
      else
         log_line("[VideoSourceCSI] Opened video capture program commands pipe write endpoint.");
   }

   if ( NULL == s_pSharedMemRaspiVidCommands )
   {
      log_softerror_and_alarm("[VideoSourceCSI] Can't send video/camera command to video capture programm, no pipe.");
      return;
   }

   static u32 s_lastCameraCommandModifyCounter = 0;
   static u8 s_CameraCommandNumber = 0;
   static u32 s_uCameraCommandsBuffer[8];

   if ( 0 == s_lastCameraCommandModifyCounter )
      memset( (u8*)&s_uCameraCommandsBuffer[0], 0, 8*sizeof(u32));

   s_lastCameraCommandModifyCounter++;
   s_CameraCommandNumber++;
   if ( 0 == s_CameraCommandNumber )
    s_CameraCommandNumber++;
   
   // u32 0: command modify stamp (must match u32 7)
   // u32 1: byte 0: command counter;
   //        byte 1: command type;
   //        byte 2: command parameter 1;
   //        byte 3: command parameter 2;
   // u32 2-6: history of previous commands (after current one from u32-1)
   // u32 7: command modify stamp (must match u32 0)

   s_uCameraCommandsBuffer[0] = s_lastCameraCommandModifyCounter;
   s_uCameraCommandsBuffer[7] = s_lastCameraCommandModifyCounter;
   for( int i=6; i>1; i-- )
      s_uCameraCommandsBuffer[i] = s_uCameraCommandsBuffer[i-1];
   s_uCameraCommandsBuffer[1] = ((u32)s_CameraCommandNumber) | (((u32)parameter)<<8) | (((u32)value)<<16);

   memcpy(s_pSharedMemRaspiVidCommands, (u8*)&(s_uCameraCommandsBuffer[0]), 8*sizeof(u32));
   #endif
}

void video_source_csi_periodic_checks()
{
   #ifdef HW_PLATFORM_RASPBERRY

   if ( ! s_bDidSentRaspividBitrateRefresh )
   if ( (0 != s_uRaspiVidStartTimeMs) && ( g_TimeNow > s_uRaspiVidStartTimeMs + 2000 ) )
   {
      log_line("[VideoSourceCSI] Send initial bitrate (%u bps) to video capture program.", g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate);
      video_source_csi_send_control_message( RASPIVID_COMMAND_ID_VIDEO_BITRATE, g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/100000 );
      if ( NULL != g_pProcessorTxVideo )
         g_pProcessorTxVideo->setLastSetCaptureVideoBitrate(g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate, true);
      s_bDidSentRaspividBitrateRefresh = true;
   }

   if ( s_bRequestedVideoCSICaptureRestart )
   if ( 0 == s_uTimeToRestartVideoCapture )
   {
      if ( s_bVideoCSICaptureProgramStarted )
      if ( 0 != s_uRaspiVidStartTimeMs )
         video_source_csi_stop_program();

      g_TimeNow = get_current_timestamp_ms();
      s_uTimeToRestartVideoCapture = g_TimeNow + 50;
      if ( g_pCurrentModel->isActiveCameraHDMI() )
      {
         log_line("[VideoSourceCSI] HDMI camera detected.");
         s_uTimeToRestartVideoCapture = g_TimeNow + 1500;
      }
      log_line("[VideoSourceCSI] Set start video capture %u miliseconds from now.", s_uTimeToRestartVideoCapture - g_TimeNow);
   }

   if ( s_bRequestedVideoCSICaptureRestart )
   if ( 0 != s_uTimeToRestartVideoCapture )
   {
      log_line("[VideoSourceCSI] It's time to restart raspi video capture...");
      video_source_csi_start_program();
      s_bRequestedVideoCSICaptureRestart = false;
      s_uTimeToRestartVideoCapture = 0;
      send_alarm_to_controller(ALARM_ID_VEHICLE_VIDEO_CAPTURE_RESTARTED,1,0, 5);
   }


   if ( g_TimeNow >= s_uDebugTimeLastCSIVideoInputCheck+1000 )
   {
      log_line("[VideoSourceCSI] Input video data: %u bytes/sec, %u bps, %u reads/sec",
         s_uDebugCSIInputBytes, s_uDebugCSIInputBytes*8, s_uDebugCSIInputReads);
      s_uDebugTimeLastCSIVideoInputCheck = g_TimeNow;
      s_uDebugCSIInputBytes = 0;
      s_uDebugCSIInputReads = 0;
   }

   #endif
}