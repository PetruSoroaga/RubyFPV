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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/hw_procs.h"
#include "../base/ruby_ipc.h"
#include "../base/parser_h264.h"
#include "../base/utils.h"
#include "../common/string_utils.h"

#include <errno.h>
#include <sys/stat.h>
#include <math.h>

#include "video_source_csi.h"
#include "packets_utils.h"
#include "processor_relay.h"
#include "launchers_vehicle.h"
#include "events.h"
#include "timers.h"
#include "shared_vars.h"

#ifdef HW_PLATFORM_RASPBERRY

extern ParserH264 s_ParserH264CameraOutput;

pthread_t s_pThreadWatchDogVideoCapture;
bool s_bIsFirstCameraParamsUpdate = true;

static type_camera_parameters s_LastAppliedVeyeCameraParams;
static type_video_link_profile s_LastAppliedVeyeVideoParams;

int s_fInputVideoStreamCSIPipe = -1;
char s_szInputVideoStreamCSIPipeName[128];
bool s_bInputVideoStreamCSIPipeOpenFailed = false;
u8 s_uInputVideoCSIPipeBuffer[4096];

bool s_bRequestedVideoCSICaptureRestart = false;
bool s_bVideoCSICaptureProgramStarted = false;
u32  s_uTimeToRestartVideoCapture = 0;
u32  s_uRaspiVidStartTimeMs = 0;
u8*  s_pSharedMemRaspiVidCommands = NULL;
bool s_bDidSentRaspividBitrateRefresh = false;

u32 s_uDebugTimeLastCSIVideoInputCheck = 0;
u32 s_uDebugCSIInputBytes = 0;
u32 s_uDebugCSIInputReads = 0;


static void * _thread_watchdog_video_capture(void *ignored_argument)
{
   int iCount = 0;
   while ( ! g_bQuit )
   {
      for( int i=0; i<10; i++)
      {
         hardware_sleep_ms(200);
         if ( g_bQuit )
            return NULL;
      }

      iCount++;

      // If video capture is not started, do nothing
      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      if ( ! video_source_csi_is_program_started() )
         continue;

      // If video capture was flagged or is in the process of restarting, do not check video capture running
      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      if ( video_source_csi_is_restart_requested() )
         continue;

      // Check video capture program up and running
   
      if ( g_pCurrentModel->hasCamera() && (video_source_cs_get_program_start_time() > 0) && (g_TimeNow > video_source_cs_get_program_start_time()+5000) )
      if ( ! g_bVideoPaused )
      //if ( g_TimeNow > g_TimeLastVideoCaptureProgramRunningCheck + 2000 )
      {
         g_TimeLastVideoCaptureProgramRunningCheck = g_TimeNow;

         bool bProgramRunning = false;
         bool bNeedsRestart = false;
         if ( (!bProgramRunning) && hw_process_exists(VIDEO_RECORDER_COMMAND) )
            bProgramRunning = true;
         if ( (!bProgramRunning) && hw_process_exists(VIDEO_RECORDER_COMMAND_VEYE) )
            bProgramRunning = true;
         if ( (!bProgramRunning) && hw_process_exists(VIDEO_RECORDER_COMMAND_VEYE307) )
            bProgramRunning = true;
         if ( (!bProgramRunning) && hw_process_exists(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME) )
            bProgramRunning = true;

         if ( ! bProgramRunning )
         {
            log_line("[VideoCaptureCSITh] Can't find the running video capture program. Search for it.");
            char szOutput[2048];
            hw_execute_bash_command("pidof ruby_capture_raspi", szOutput);
            log_line("[VideoCaptureCSITh] PID of ruby capture: [%s]", szOutput);
            hw_execute_bash_command_raw("ps -aef | grep ruby 2>&1", szOutput);
            log_line("[VideoCaptureCSITh] Current running Ruby processes: [%s]", szOutput);
            if ( NULL != strstr(szOutput, "ruby_capture_") )
            {
               log_line("[VideoCaptureCSITh] Video capture is still running. Do not restart it.");
            }
            else
            {
               log_softerror_and_alarm("[VideoCaptureCSITh] The video capture program has crashed (no process). Restarting it.");
               bNeedsRestart = true;
            }
         }
         if ( bProgramRunning )
         {
            if ( (iCount % 5) == 0 )
               log_line("[VideoCaptureCSITh] Video capture watchdog: capture is running ok.");
              
            static int s_iCheckVideoOutputBitrateCounter = 0;
            if ( relay_current_vehicle_must_send_own_video_feeds() &&
                (g_pProcessorTxVideo->getCurrentVideoBitrateAverageLastMs(1000) < 100000) )
            {
               log_softerror_and_alarm("[VideoCaptureCSITh] Current output video bitrate is less than 100 kbps!");
               s_iCheckVideoOutputBitrateCounter++;
               if ( s_iCheckVideoOutputBitrateCounter >= 3 )
               {
                  log_softerror_and_alarm("[VideoCaptureCSITh] The video capture program has crashed (no video output). Restarting it.");
                  bNeedsRestart = true;
               }
            }
            else
               s_iCheckVideoOutputBitrateCounter = 0;
         }
         if ( bNeedsRestart )
         {
            send_alarm_to_controller(ALARM_ID_VEHICLE_VIDEO_CAPTURE_RESTARTED,0,0, 5);
            log_line("[VideoCaptureCSITh] Signaled router main thread to restart video capture.");
            video_source_csi_request_restart_program();
         }
      }
   }
   return NULL;
}

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

   struct timeval timePipeInput;
   fd_set readset;
   int iSelectResult = 0;
   static bool s_bLastCameraReadTimedOut = false;

   *piReadSize = 0;
   if ( -1 == s_fInputVideoStreamCSIPipe )
   {
      if ( s_bInputVideoStreamCSIPipeOpenFailed )
         video_source_csi_open(s_szInputVideoStreamCSIPipeName);
      if ( -1 == s_fInputVideoStreamCSIPipe )
         return NULL;
   }

   FD_ZERO(&readset);
   FD_SET(s_fInputVideoStreamCSIPipe, &readset);

   // Check for exceptions first, only if we are not already getting a lot of data

   if ( s_bLastCameraReadTimedOut )
   {
      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = 10; // 0.01 miliseconds timeout

      iSelectResult = select(s_fInputVideoStreamCSIPipe+1, NULL, NULL, &readset, &timePipeInput);
      if ( iSelectResult > 0 )
      {
         log_softerror_and_alarm("[VideoSourceCSI] Exception on reading input pipe. Closing pipe.");
         close(s_fInputVideoStreamCSIPipe);
         s_fInputVideoStreamCSIPipe = -1;
         return NULL;
      }
   }

   // Try read

   FD_ZERO(&readset);
   FD_SET(s_fInputVideoStreamCSIPipe, &readset);

   timePipeInput.tv_sec = 0;

   if ( s_bLastCameraReadTimedOut )
      timePipeInput.tv_usec = 1000; // 1 miliseconds timeout
   else
      timePipeInput.tv_usec = 10; // 0.01 miliseconds timeout

   s_bLastCameraReadTimedOut = true;

   iSelectResult = select(s_fInputVideoStreamCSIPipe+1, &readset, NULL, NULL, &timePipeInput);
   if ( iSelectResult <= 0 )
      return s_uInputVideoCSIPipeBuffer;

   if( 0 == FD_ISSET(s_fInputVideoStreamCSIPipe, &readset) )
      return s_uInputVideoCSIPipeBuffer;

   s_bLastCameraReadTimedOut = false;

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

   vehicle_launch_video_capture_csi(g_pCurrentModel, &(g_SM_VideoLinkStats.overwrites));
   vehicle_check_update_processes_affinities(true, g_pCurrentModel->isActiveCameraVeye());

   if ( 0 != pthread_create(&s_pThreadWatchDogVideoCapture, NULL, &_thread_watchdog_video_capture, NULL) )
      log_softerror_and_alarm("[VideoSourceCSI] Failed to create thread for watchdog.");
   else
      log_line("[VideoSourceCSI] Created thread for watchdog.");

   s_uRaspiVidStartTimeMs = g_TimeNow;
   s_bDidSentRaspividBitrateRefresh = false;
}

void video_source_csi_stop_program()
{
   s_bVideoCSICaptureProgramStarted = false;
   s_uRaspiVidStartTimeMs = 0;

   pthread_cancel(s_pThreadWatchDogVideoCapture); 
   vehicle_stop_video_capture_csi(g_pCurrentModel);
   
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

   if ( parameter == RASPIVID_COMMAND_ID_KEYFRAME )
   {
      //log_line("DBG set keyframe to %d frames", value);
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
         g_pProcessorTxVideo->setLastSetCaptureVideoBitrate(g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate, true, 0);
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

   #endif

   if ( g_TimeNow >= s_uDebugTimeLastCSIVideoInputCheck+10000 )
   {
      log_line("[VideoSourceCSI] Input video data: %u bytes/sec, %u bps, %u reads/sec",
         s_uDebugCSIInputBytes/10, s_uDebugCSIInputBytes/10*8, s_uDebugCSIInputReads/10);
      log_line("[VideoSourceCSI] Detected video stream fps: %d, slices: %d", (int)s_ParserH264CameraOutput.getDetectedFPS(), s_ParserH264CameraOutput.getDetectedSlices());
      s_uDebugTimeLastCSIVideoInputCheck = g_TimeNow;
      s_uDebugCSIInputBytes = 0;
      s_uDebugCSIInputReads = 0;
   }
}


bool vehicle_launch_video_capture_csi(Model* pModel, shared_mem_video_link_overwrites* pVideoOverwrites)
{
   if ( NULL == pModel )
   {
      log_error_and_alarm("Tried to launch the video program without a model definition.");
      return false;
   }

   if ( s_bIsFirstCameraParamsUpdate )
   {
      memset((u8*)&s_LastAppliedVeyeCameraParams, 0, sizeof(type_camera_parameters));
      memset((u8*)&s_LastAppliedVeyeVideoParams, 0, sizeof(video_parameters_t));
   }

   bool bResult = true;

   if ( pModel->isActiveCameraVeye() )
   {
      char szComm[1024];
      char szOutput[1024];
 
      int nBus = hardware_get_i2c_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      log_line("Applying VeYe camera commands to I2C bus number %d, dev address: 0x%02X", nBus, I2C_DEVICE_ADDRESS_CAMERA_VEYE);

      sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f devid -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      hw_execute_bash_command_raw(szComm, szOutput);
      log_line("VEYE Camera Dev Id output: %s", szOutput);

      sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f hdver -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      hw_execute_bash_command_raw(szComm, szOutput);
      log_line("VEYE Camera HW Ver output: %s", szOutput);

      vehicle_update_camera_params_csi(pModel, pModel->iCurrentCamera);
      hardware_sleep_ms(200);
   }

   if ( pModel->isActiveCameraHDMI() )
      hardware_sleep_ms(200);

   log_line("Computing video capture parameters for active camera type: %d ...", pModel->getActiveCameraType());

   char szFile[128];
   char szBuff[1024];
   char szCameraFlags[256];
   char szVideoFlags[256];
   char szPriority[64];
   szCameraFlags[0] = 0;
   szVideoFlags[0] = 0;
   szPriority[0] = 0;
   pModel->getCameraFlags(szCameraFlags);
   pModel->getVideoFlags(szVideoFlags, pModel->video_params.user_selected_video_link_profile, pVideoOverwrites);

   #ifdef HW_CAPABILITY_IONICE
   if ( pModel->processesPriorities.ioNiceVideo > 0 )
   {
      if ( pModel->processesPriorities.iNiceVideo != 0 )
         sprintf(szPriority, "ionice -c 1 -n %d nice -n %d", pModel->processesPriorities.ioNiceVideo, pModel->processesPriorities.iNiceVideo );
      else
         sprintf(szPriority, "ionice -c 1 -n %d", pModel->processesPriorities.ioNiceVideo);
   }
   else
   #endif
   if ( pModel->processesPriorities.iNiceVideo != 0 )
      sprintf(szPriority, "nice -n %d", pModel->processesPriorities.iNiceVideo );

   if ( pModel->isActiveCameraVeye() )
   {
      if ( pModel->camera_params[pModel->iCurrentCamera].iCameraType == CAMERA_TYPE_VEYE307 )
      {
         if ( pModel->bDeveloperMode )
            sprintf(szBuff, "%s %s -dbg %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE307, szVideoFlags, FIFO_RUBY_CAMERA1 );
         else
            sprintf(szBuff, "%s %s %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE307, szVideoFlags, FIFO_RUBY_CAMERA1 );
      }
      else
      {
         if ( pModel->bDeveloperMode )
            sprintf(szBuff, "%s %s -dbg %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE, szVideoFlags, FIFO_RUBY_CAMERA1 );
         else
            sprintf(szBuff, "%s %s %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND_VEYE, szVideoFlags, FIFO_RUBY_CAMERA1 );
      }
   }
   else
   {
      if ( pModel->bDeveloperMode )
         sprintf(szBuff, "%s ./%s -dbg %s %s -log -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND, szVideoFlags, szCameraFlags, FIFO_RUBY_CAMERA1 );
      else
         sprintf(szBuff, "%s ./%s %s %s -t 0 -o - >> %s &", szPriority, VIDEO_RECORDER_COMMAND, szVideoFlags, szCameraFlags, FIFO_RUBY_CAMERA1 );
   }

   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, FILE_TEMP_CURRENT_VIDEO_PARAMS);

   FILE* fd = fopen(szFile, "w");
   if ( NULL == fd )
      log_softerror_and_alarm("Failed to save current video config log to file: %s", szFile);
   else
   {
      fprintf(fd, "Video Flags: %s  # ", szVideoFlags);
      fprintf(fd, "Camera Profile: %s; Camera Flags: %s", model_getCameraProfileName(pModel->camera_params[pModel->iCurrentCamera].iCurrentProfile), szCameraFlags);
      fclose(fd);
   }

   //log_line("Executing video pipeline: [%s]", szBuff);
   bResult = hw_execute_bash_command(szBuff, NULL);

   if ( pModel->isActiveCameraVeye() )
   {
      if ( pModel->isActiveCameraVeye307() )
         hw_set_proc_priority(VIDEO_RECORDER_COMMAND_VEYE307, pModel->processesPriorities.iNiceVideo, pModel->processesPriorities.ioNiceVideo, 1 );
      else
         hw_set_proc_priority(VIDEO_RECORDER_COMMAND_VEYE, pModel->processesPriorities.iNiceVideo, pModel->processesPriorities.ioNiceVideo, 1 );
      hw_set_proc_priority(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME, pModel->processesPriorities.iNiceVideo, pModel->processesPriorities.ioNiceVideo, 1 );
   }
   else
   {
      hw_set_proc_priority(VIDEO_RECORDER_COMMAND, pModel->processesPriorities.iNiceVideo, pModel->processesPriorities.ioNiceVideo, 1 );
   }
   
   g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs = pModel->getInitialKeyframeIntervalMs(pModel->video_params.user_selected_video_link_profile);
   g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs = g_SM_VideoLinkStats.overwrites.uCurrentPendingKeyframeMs;
   log_line("Completed launching video capture. Initial keyframe: %d", g_SM_VideoLinkStats.overwrites.uCurrentActiveKeyframeMs );
   return bResult;
}

void vehicle_stop_video_capture_csi(Model* pModel)
{
   g_SM_VideoLinkStats.overwrites.hasEverSwitchedToLQMode = 0;

   if ( pModel->isActiveCameraVeye() )
   {
      hw_stop_process(VIDEO_RECORDER_COMMAND_VEYE);
      hw_stop_process(VIDEO_RECORDER_COMMAND_VEYE307);
      hw_stop_process(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME);
   }
   else
      hw_stop_process(VIDEO_RECORDER_COMMAND);

   log_line("Stopped video capture process.");
}


void vehicle_update_camera_params_csi(Model* pModel, int iCameraIndex)
{
   if ( ! pModel->isActiveCameraVeye() )
      return;

   char szComm[1024];
   char szCameraFlags[512];
   szCameraFlags[0] = 0;
   pModel->getCameraFlags(szCameraFlags);
   if ( iCameraIndex < 0 || iCameraIndex >= pModel->iCameraCount )
      iCameraIndex = 0;

   int iProfile = pModel->camera_params[iCameraIndex].iCurrentProfile;
   bool bApplyAll = false;
   if ( s_LastAppliedVeyeVideoParams.width == 0 )
      bApplyAll = true;

   if ( s_bIsFirstCameraParamsUpdate )
   {
      memset((u8*)&s_LastAppliedVeyeCameraParams, 0, sizeof(type_camera_parameters));
      memset((u8*)&s_LastAppliedVeyeVideoParams, 0, sizeof(video_parameters_t));
      s_bIsFirstCameraParamsUpdate = false;

      if ( pModel->isActiveCameraVeye327290() )
      {
         int nBus = hardware_get_i2c_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f  videofmt -p1 NTSC -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
      bApplyAll = true;
   }

   if ( pModel->isActiveCameraVeye307() )
   {
      int nBus = hardware_get_i2c_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      
      if ( s_LastAppliedVeyeVideoParams.width != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].width ||
           s_LastAppliedVeyeVideoParams.height != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].height ||
           s_LastAppliedVeyeVideoParams.fps != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].fps )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f  videofmt -p1 %d -p2 %d -p3 %d -b %d; cd $current_dir",
          VEYE_COMMANDS_FOLDER307, pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].width, pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].height, pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].fps, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].dayNightMode != pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode == 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f daynightmode -p1 0x1 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f daynightmode -p1 0x2 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].brightness != pModel->camera_params[iCameraIndex].profiles[iProfile].brightness )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f aetarget -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, (int)(2.5f*(pModel->camera_params[iCameraIndex].profiles[iProfile].brightness)), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].contrast != pModel->camera_params[iCameraIndex].profiles[iProfile].contrast )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f contrast -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, pModel->camera_params[iCameraIndex].profiles[iProfile].contrast, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].saturation != pModel->camera_params[iCameraIndex].profiles[iProfile].saturation )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f satu -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, (pModel->camera_params[iCameraIndex].profiles[iProfile].saturation/2), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || (fabs(s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].hue - pModel->camera_params[iCameraIndex].profiles[iProfile].hue) > 0.1) )
      {
         int hue = pModel->camera_params[iCameraIndex].profiles[iProfile].hue;
         if ( (hue > 1) && (hue < 100) )
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f hue -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, hue, nBus);
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].shutterspeed != pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed == 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f expmode -p1 0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f expmode -p1 1 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
            hw_execute_bash_command(szComm, NULL);

            char szShutter[24];
            sprintf(szShutter, "%d", (int)(1000000l/(long)pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed));
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f metime -p1 %s -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, szShutter, nBus);
         }
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].whitebalance != pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
      {
         if ( 0 == pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f awbmode -p1 1 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f awbmode -p1 0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].flip_image != pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f imagedir -p1 3 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f imagedir -p1 0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
   }
   else // IMX 327 camera
   {
      int nBus = hardware_get_i2c_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      log_line("Applying VeYe camera commands to I2C bus number %d, dev address: 0x%02X", nBus, I2C_DEVICE_ADDRESS_CAMERA_VEYE);
      if ( bApplyAll )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f  videofmt -p1 NTSC -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].dayNightMode != pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].dayNightMode == 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f daynightmode -p1 0xFF -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f daynightmode -p1 0xFE -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].brightness != pModel->camera_params[iCameraIndex].profiles[iProfile].brightness )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f brightness -p1 0x%02X -b %d 2>&1; cd $current_dir", VEYE_COMMANDS_FOLDER, (int)(pModel->camera_params[iCameraIndex].profiles[iProfile].brightness), nBus);
         char szOutput[1024];
         szOutput[0] = 0;
         hw_execute_bash_command_raw(szComm, szOutput);
         log_line("output: [%s]", szOutput);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].contrast != pModel->camera_params[iCameraIndex].profiles[iProfile].contrast )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f contrast -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, (int)(2.5*(float)(pModel->camera_params[iCameraIndex].profiles[iProfile].contrast)), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].saturation != pModel->camera_params[iCameraIndex].profiles[iProfile].saturation )
      {
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f saturation -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, (pModel->camera_params[iCameraIndex].profiles[iProfile].saturation/2), nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].wdr != pModel->camera_params[iCameraIndex].profiles[iProfile].wdr )
      {
         int wdrmode = (int) pModel->camera_params[iCameraIndex].profiles[iProfile].wdr;
         if ( wdrmode < 0 || wdrmode > 3 )
            wdrmode = 0;
         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wdrmode -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, wdrmode, nBus);
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].shutterspeed != pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed == 0 )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mshutter -p1 0x40 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
         {
         char szShutter[24];
         strcpy(szShutter, "0x40");
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 50 )
            strcpy(szShutter, "0x41"); // 1/30
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 100 )
            strcpy(szShutter, "0x42"); // 1/60
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 200 )
            strcpy(szShutter, "0x43"); // 1/120
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 400 )
            strcpy(szShutter, "0x44"); // 1/240
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 700 )
            strcpy(szShutter, "0x45"); // 1/480
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 1600 )
            strcpy(szShutter, "0x46"); // 1/1000
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 3500 )
            strcpy(szShutter, "0x47"); // 1/2000
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 7000 )
            strcpy(szShutter, "0x48"); // 1/5000
         else if ( pModel->camera_params[iCameraIndex].profiles[iProfile].shutterspeed < 20000 )
            strcpy(szShutter, "0x49"); // 1/10000
         else
            strcpy(szShutter, "0x4A"); // 1/50000

         sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mshutter -p1 %s -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, szShutter, nBus);
         }
         hw_execute_bash_command(szComm, NULL);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].whitebalance != pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
      {
         if ( 0 == pModel->camera_params[iCameraIndex].profiles[iProfile].whitebalance )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wbmode -p1 0x1B -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f wbmode -p1 0x18 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].sharpness != pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness >= 100 )
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness <= 110 )
         {
            if ( pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness == 100 )
               sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f sharppen -p1 0x0 -p2 0x0 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
            else
               sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f sharppen -p1 0x1 -p2 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, pModel->camera_params[iCameraIndex].profiles[iProfile].sharpness - 101, nBus);
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].drc != pModel->camera_params[iCameraIndex].profiles[iProfile].drc )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].drc<16 )
         {
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f agc -p1 0x%02X -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, pModel->camera_params[iCameraIndex].profiles[iProfile].drc, nBus );
            hw_execute_bash_command(szComm, NULL);
         }
      }

      if ( bApplyAll || s_LastAppliedVeyeCameraParams.profiles[s_LastAppliedVeyeCameraParams.iCurrentProfile].flip_image != pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
      {
         if ( pModel->camera_params[iCameraIndex].profiles[iProfile].flip_image )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mirrormode -p1 0x03 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         else
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f mirrormode -p1 0x00 -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
   }
   
   memcpy((u8*)&s_LastAppliedVeyeCameraParams, (u8*)&(pModel->camera_params[iCameraIndex]), sizeof(type_camera_parameters));
   memcpy((u8*)&s_LastAppliedVeyeVideoParams, (u8*)&(pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile]), sizeof(type_video_link_profile));
}

#else

void video_source_csi_close() {}
int video_source_csi_open(const char* szPipeName) {return 0;}
void video_source_csi_flush_discard() {}
u8* video_source_csi_read(int* piReadSize) {return NULL;}
void video_source_csi_start_program() {}
void video_source_csi_stop_program() {}
bool video_source_csi_is_program_started() {return false;}
u32 video_source_cs_get_program_start_time() { return 0;}
void video_source_csi_request_restart_program() {}
bool video_source_csi_is_restart_requested() { return false;}
void video_source_csi_send_control_message(u8 parameter, u8 value) {}
void video_source_csi_periodic_checks() {}

bool vehicle_launch_video_capture_csi(Model* pModel, shared_mem_video_link_overwrites* pVideoOverwrites) { return false; }
void vehicle_stop_video_capture_csi(Model* pModel) {}
void vehicle_update_camera_params_csi(Model* pModel, int iCameraIndex) {}

#endif