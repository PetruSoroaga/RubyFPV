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

#include "launchers_vehicle.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_i2c.h"
#include "../base/hardware_cam_maj.h"
#include "../base/hw_procs.h"
#include "../base/radio_utils.h"
#include <math.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>

#include "shared_vars.h"
#include "timers.h"

static bool s_bAudioCaptureIsStarted = false;
static pthread_t s_pThreadAudioCapture;

void vehicle_launch_tx_telemetry(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching TX telemetry. Can't start TX telemetry.");
      return;
   }
   hw_execute_ruby_process(NULL, "ruby_tx_telemetry", NULL, NULL);
}

void vehicle_stop_tx_telemetry()
{
   hw_stop_process("ruby_tx_telemetry");
}

void vehicle_launch_rx_rc(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching RX RC. Can't start RX RC.");
      return;
   }
   char szPrefix[64];
   szPrefix[0] = 0;
   #ifdef HW_CAPABILITY_IONICE
   sprintf(szPrefix, "ionice -c 1 -n %d nice -n %d", DEFAULT_IO_PRIORITY_RC, pModel->processesPriorities.iNiceRC);
   #else
   sprintf(szPrefix, "nice -n %d", pModel->processesPriorities.iNiceRC);
   #endif
   hw_execute_ruby_process(szPrefix, "ruby_start", "-rc", NULL);
}

void vehicle_stop_rx_rc()
{
   sem_t* s = sem_open(SEMAPHORE_STOP_RX_RC, 0, S_IWUSR | S_IRUSR, 0);
   if ( SEM_FAILED != s )
   {
      sem_post(s);
      sem_close(s);
   }
}

void vehicle_launch_rx_commands(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching RX commands. Can't start RX commands.");
      return;
   }
   char szPrefix[64];
   szPrefix[0] = 0;
   hw_execute_ruby_process(szPrefix, "ruby_start", "-rx_commands", NULL);
}

void vehicle_stop_rx_commands()
{
   char szComm[256];
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_STOP);
   hw_execute_bash_command(szComm, NULL);

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_RUBY_TEMP);
   strcat(szFile, "cmdstop.txt");
   int iCounter = 0;
   while ( true )
   {
      iCounter++;
      hardware_sleep_ms(200);
      if ( access(szFile, R_OK) == -1 )
      {
         log_line("Stopped rx_commands process.");
         break;
      }
      if ( iCounter > 20 )
      {
         log_line("Failed to stop rx_commands process.");
         break;
      }
   }
}

void vehicle_launch_tx_router(Model* pModel)
{
   if (NULL == pModel )
   {
      log_error_and_alarm("Invalid model (NULL) on launching TX video pipeline. Can't start TX video pipeline.");
      return;
   }

   hardware_sleep_ms(20);

   char szPrefix[64];
   szPrefix[0] = 0;
   #ifdef HW_CAPABILITY_IONICE
   if ( pModel->processesPriorities.ioNiceRouter > 0 )
   {
      if ( pModel->processesPriorities.iNiceRouter != 0 )
         sprintf(szPrefix, "ionice -c 1 -n %d nice -n %d", pModel->processesPriorities.ioNiceRouter, pModel->processesPriorities.iNiceRouter );
      else
         sprintf(szPrefix, "ionice -c 1 -n %d", pModel->processesPriorities.ioNiceRouter );
   }
   else
   #endif
   if ( pModel->processesPriorities.iNiceRouter != 0 )
      sprintf(szPrefix, "nice -n %d", pModel->processesPriorities.iNiceRouter);

   hw_execute_ruby_process(szPrefix, "ruby_rt_vehicle", NULL, NULL);
}

void vehicle_stop_tx_router()
{
   char szRouter[64];
   strcpy(szRouter, "ruby_rt_vehicle");

   hw_stop_process(szRouter);
}

#if defined (HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA_ZERO3)
static void * _thread_audio_capture(void *argument)
{
   s_bAudioCaptureIsStarted = true;

   Model* pModel = (Model*) argument;
   if ( NULL == pModel )
   {
      s_bAudioCaptureIsStarted = false;
      return NULL;
   }

   char szCommFlag[256];
   char szCommCapture[256];
   char szRate[32];
   char szPriority[64];
   int iIntervalSec = 5;

   sprintf(szCommCapture, "amixer -c 1 sset Mic %d%%", pModel->audio_params.volume);
   hw_execute_bash_command(szCommCapture, NULL);

   strcpy(szRate, "8000");
   if ( 0 == pModel->audio_params.quality )
      strcpy(szRate, "14000");
   if ( 1 == pModel->audio_params.quality )
      strcpy(szRate, "24000");
   if ( 2 == pModel->audio_params.quality )
      strcpy(szRate, "32000");
   if ( 3 == pModel->audio_params.quality )
      strcpy(szRate, "44100");

   strcpy(szRate, "44100");

   szPriority[0] = 0;
   #ifdef HW_CAPABILITY_IONICE
   if ( pModel->processesPriorities.ioNiceVideo > 0 )
      sprintf(szPriority, "ionice -c 1 -n %d nice -n %d", pModel->processesPriorities.ioNiceVideo, pModel->processesPriorities.iNiceVideo );
   else
   #endif
      sprintf(szPriority, "nice -n %d", pModel->processesPriorities.iNiceVideo );

   sprintf(szCommCapture, "%s arecord --device=hw:1,0 --file-type wav --format S16_LE --rate %s -c 1 -d %d -q >> %s",
      szPriority, szRate, iIntervalSec, FIFO_RUBY_AUDIO1);

   sprintf(szCommFlag, "echo '0123456789' > %s", FIFO_RUBY_AUDIO1);

   hw_stop_process("arecord");

   while ( s_bAudioCaptureIsStarted && ( ! g_bQuit) )
   {
      if ( g_bReinitializeRadioInProgress )
      {
         hardware_sleep_ms(50);
         continue;         
      }

      u32 uTimeCheck = get_current_timestamp_ms();

      hw_execute_bash_command(szCommCapture, NULL);
      
      u32 uTimeNow = get_current_timestamp_ms();
      if ( uTimeNow < uTimeCheck + (u32)iIntervalSec * 500 )
      {
         log_softerror_and_alarm("[AudioCaptureThread] Audio capture segment finished too soon (took %u ms, expected %d ms)",
             uTimeNow - uTimeCheck, iIntervalSec*1000);
         for( int i=0; i<10; i++ )
            hardware_sleep_ms(iIntervalSec*50);
      }

      hw_execute_bash_command(szCommFlag, NULL);
   }
   s_bAudioCaptureIsStarted = false;
   return NULL;
}
#endif

bool vehicle_is_audio_capture_started()
{
  return s_bAudioCaptureIsStarted;
}

void vehicle_launch_audio_capture(Model* pModel)
{
   if ( s_bAudioCaptureIsStarted )
      return;

   if ( NULL == pModel || (! pModel->audio_params.has_audio_device) || (! pModel->audio_params.enabled) )
      return;

   s_bAudioCaptureIsStarted = true;

   #if defined (HW_PLATFORM_RASPBERRY)
   if ( 0 != pthread_create(&s_pThreadAudioCapture, NULL, &_thread_audio_capture, g_pCurrentModel) )
   {
      log_softerror_and_alarm("Failed to create thread for audio capture.");
      s_bAudioCaptureIsStarted = false;
      return;
   }
   log_line("Created thread for audio capture.");
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hardware_camera_maj_enable_audio(true, 4000*(1+pModel->audio_params.quality), pModel->audio_params.volume);
   #endif
}

void vehicle_stop_audio_capture(Model* pModel)
{
   if ( ! s_bAudioCaptureIsStarted )
      return;
   s_bAudioCaptureIsStarted = false;

   if ( NULL == pModel || (! pModel->audio_params.has_audio_device) )
      return;

   //hw_execute_bash_command("kill -9 $(pidof arecord) 2>/dev/null", NULL);
   #if defined (HW_PLATFORM_RASPBERRY)
   hw_stop_process("arecord");
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hardware_camera_maj_enable_audio(false, 0, 0);
   #endif
}

static bool s_bThreadBgAffinitiesStarted = false;
static int s_iCPUCoresCount = -1;
static bool s_bAdjustAffinitiesIsVeyeCamera = false;
static bool s_bLaunchedAffinitiesInThread = false;

static void * _thread_adjust_affinities_vehicle(void *argument)
{
   s_bThreadBgAffinitiesStarted = true;
   if ( s_bLaunchedAffinitiesInThread )
      sched_yield();
   bool bVeYe = false;
   if ( NULL != argument )
   {
      bool* pB = (bool*)argument;
      bVeYe = *pB;
   }

   log_line("Started background thread to adjust processes affinities (arg: %p, veye: %d (%d))...", argument, (int)bVeYe, (int)s_bAdjustAffinitiesIsVeyeCamera);
   int iSelfPID = getpid();
   int iSelfId = 0;
   #if defined(HW_PLATFORM_RADXA_ZERO3) || defined(HW_PLATFORM_OPENIPC_CAMERA)
   iSelfId = gettid();
   #endif

   log_line("[BGThread] Background thread id: %d, PID: %d", iSelfId, iSelfPID);
   if ( ! s_bLaunchedAffinitiesInThread )
      iSelfId = 0;
   if ( s_iCPUCoresCount < 1 )
   {
      s_iCPUCoresCount = 1;
      char szOutput[128];
      hw_execute_bash_command_raw("nproc --all", szOutput);
      if ( 1 != sscanf(szOutput, "%d", &s_iCPUCoresCount) )
      {
         log_softerror_and_alarm("Failed to get CPU cores count. No affinity adjustments for processes to be done.");
         s_bThreadBgAffinitiesStarted = false;
         s_iCPUCoresCount = 1;
         return NULL;    
      }
   }

   if ( s_iCPUCoresCount < 2 || s_iCPUCoresCount > 32 )
   {
      log_line("Single core CPU (%d), no affinity adjustments for processes to be done.", s_iCPUCoresCount);
      s_bThreadBgAffinitiesStarted = false;
      return NULL;
   }

   log_line("%d CPU cores, doing affinity adjustments for processes...", s_iCPUCoresCount);
   if ( s_iCPUCoresCount > 2 )
   {
      hw_set_proc_affinity("ruby_rt_vehicle", iSelfId, 1,1);
      hw_set_proc_affinity("ruby_tx_telemetry", iSelfId, 2,2);
      // To fix
      //hw_set_proc_affinity("ruby_rx_rc", iSelfId, 2,2);

      //#if defined (HW_PLATFORM_OPENIPC_CAMERA)
      //hw_set_proc_affinity("majestic", iSelfId, 3, s_iCPUCoresCount);
      //#endif

      #ifdef HW_PLATFORM_RASPBERRY
      if ( bVeYe )
         hw_set_proc_affinity(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME, iSelfId, 3, s_iCPUCoresCount);
      else
         hw_set_proc_affinity(VIDEO_RECORDER_COMMAND, iSelfId, 3, s_iCPUCoresCount);
      #endif
   }
   else
   {
      hw_set_proc_affinity("ruby_rt_vehicle", iSelfId, 1,1);
      //#if defined (HW_PLATFORM_OPENIPC_CAMERA)
      //hw_set_proc_affinity("majestic", iSelfId, 2, s_iCPUCoresCount);
      //#endif
      #ifdef HW_PLATFORM_RASPBERRY
      if ( bVeYe )
         hw_set_proc_affinity(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME, iSelfId, 2, s_iCPUCoresCount);
      else
         hw_set_proc_affinity(VIDEO_RECORDER_COMMAND, iSelfId, 2, s_iCPUCoresCount);
      #endif
   }

   log_line("Background thread to adjust processes affinities completed.");
   s_bThreadBgAffinitiesStarted = false;
   return NULL;
}


void vehicle_check_update_processes_affinities(bool bUseThread, bool bVeYe)
{
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   log_line("Adjusting process affinities for OpenIPC hardware. Use thread: %s", (bUseThread?"Yes":"No"));
   s_bAdjustAffinitiesIsVeyeCamera = false;
   #endif
  
   #if defined (HW_PLATFORM_RASPBERRY)
   s_bAdjustAffinitiesIsVeyeCamera = bVeYe;
   log_line("Adjust processes affinities. Use thread: %s, veye camera: %s",
      (bUseThread?"Yes":"No"), (s_bAdjustAffinitiesIsVeyeCamera?"Yes":"No"));
   #endif

   if ( s_bThreadBgAffinitiesStarted )
   {
      log_line("A background thread to adjust processes affinities is already running. Do nothing.");
      return;
   }
   s_bLaunchedAffinitiesInThread = bUseThread;
   if ( ! bUseThread )
   {
      _thread_adjust_affinities_vehicle(&s_bAdjustAffinitiesIsVeyeCamera);
      log_line("Adjusted processes affinities");
      return;
   }
   
   s_bThreadBgAffinitiesStarted = true;
   pthread_t pThreadBgAffinities;
   pthread_attr_t attr;
   struct sched_param params;

   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
   pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
   params.sched_priority = 0;
   pthread_attr_setschedparam(&attr, &params);

   if ( 0 != pthread_create(&pThreadBgAffinities, &attr, &_thread_adjust_affinities_vehicle, &s_bAdjustAffinitiesIsVeyeCamera) )
   {
      log_error_and_alarm("Failed to create thread for adjusting processes affinities.");
      pthread_attr_destroy(&attr);
      s_bThreadBgAffinitiesStarted = false;
      return;
   }
   pthread_attr_destroy(&attr);
   log_line("Created thread for adjusting processes affinities (veye: %s)", (s_bAdjustAffinitiesIsVeyeCamera?"Yes":"No"));
}
