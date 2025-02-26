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

#include "base.h"
#include "hardware_cam_maj.h"
#include "hw_procs.h"
#include "../common/string_utils.h"
#include <math.h>
#include <pthread.h>

static int s_iPIDMajestic = 0;
static u32 s_uMajesticLastChangeTime = 0;
static u32 s_uMajesticLastChangeAudioTime = 0;

pthread_t s_ThreadMajLogEntry;

static camera_profile_parameters_t s_CurrentMajesticVideoCamSettings;
static camera_profile_parameters_t s_CurrentMajesticCamSettings;
static video_parameters_t s_CurrentMajesticVideoParams;
static int s_iCurrentMajesticVideoProfile = -1;
static Model* s_pCurrentMajesticModel = NULL;

static int s_iLastMajesticIRFilterMode = -5;
pthread_t s_ThreadMajSetIRFilter;
volatile bool s_bMajThreadSetIRFilterRunning = false;

static int s_iLastMajesticDaylightMode = -5;
pthread_t s_ThreadMajSetDaylightMode;
volatile bool s_bMajThreadSetDaylightModeRunning = false;

static u32 s_uTempMajesticBrightness = MAX_U32;
static u32 s_uTempMajesticContrast = MAX_U32;
static u32 s_uTempMajesticHue = MAX_U32;
static u32 s_uTempMajesticSaturation = MAX_U32;

pthread_t s_ThreadMajSetBrightness;
pthread_t s_ThreadMajSetContrast;
pthread_t s_ThreadMajSetHue;
pthread_t s_ThreadMajSetStaturation;
volatile bool s_bMajThreadSetBrightnessRunning = false;
volatile bool s_bMajThreadSetContrastRunning = false;
volatile bool s_bMajThreadSetHueRunning = false;
volatile bool s_bMajThreadSetSaturationRunning = false;

pthread_t s_ThreadMajSetImageParams;
volatile bool s_bMajThreadSetImageParamsRunning = false;
static u32 s_uMajesticHasPendingCachedImageChangesTime = 0;

static int s_iCurrentMajesticNALSize = 0;
pthread_t s_ThreadMajSetNALSize;
volatile bool s_bMajThreadSetNALSizeRunning = false;

pthread_t s_ThreadMajSetAllParams;
volatile bool s_bMajThreadSetAllParamsRunning = false;

static float s_fCurrentMajesticGOP = -1.0;
static float s_fTemporaryMajesticGOP = -1.0;
pthread_t s_ThreadMajSetGOP;
volatile bool s_bMajThreadSetGOPRunning = false;

static u32 s_uCurrentMajesticBitrate = 0;
static u32 s_uTemporaryMajesticBitrate = 0;
pthread_t s_ThreadMajSetBitrate;
volatile bool s_bMajThreadSetBitrateRunning = false;

static int s_iCurrentMajesticQPDelta = -1000;
static int s_iTemporaryMajesticQPDelta = -1000;
pthread_t s_ThreadMajSetQPDelta;
volatile bool s_bMajThreadSetQPDeltaRunning = false;

pthread_t s_ThreadMajSetBitrateQPDelta;
volatile bool s_bMajThreadSetBitrateQPDeltaRunning = false;

static int s_iCurrentMajAudioVolume = 0;
static int s_iCurrentMajAudioBitrate = 0;

pthread_t s_ThreadMajSetAudioVolume;
volatile bool s_bMajThreadSetAudioVolumeRunning = false;

pthread_t s_ThreadMajSetAudioBitrate;
volatile bool s_bMajThreadSetAudioBitrateRunning = false;

void _adjust_thread_priority_attrs(pthread_attr_t* pAttr)
{
   if ( NULL == pAttr )
      return;
   struct sched_param params;

   pthread_attr_init(pAttr);
   pthread_attr_setdetachstate(pAttr, PTHREAD_CREATE_DETACHED);
   pthread_attr_setinheritsched(pAttr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(pAttr, SCHED_OTHER);
   params.sched_priority = 0;
   pthread_attr_setschedparam(pAttr, &params);
}
   
void* _thread_majestic_log_entry(void *argument)
{
   char* szLog = (char*)argument;
   if ( NULL == szLog )
      return NULL;
   char szComm[256];
   sprintf(szComm, "echo \"----------------------------------------------\" >> %s", CONFIG_FILE_FULLPATH_MAJESTIC_LOG);
   hw_execute_bash_command_raw_silent(szComm, NULL);
   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "echo \"Ruby: %s\" >> %s", szLog, CONFIG_FILE_FULLPATH_MAJESTIC_LOG);
   hw_execute_bash_command_raw_silent(szComm, NULL);
   free(szLog);
   return NULL;
}

void hardware_camera_maj_add_log(const char* szLog, bool bAsync)
{
   if ( NULL == szLog )
      return;
   if ( !bAsync )
   {
      char szComm[256];
      sprintf(szComm, "echo \"----------------------------------------------\" >> %s", CONFIG_FILE_FULLPATH_MAJESTIC_LOG);
      hw_execute_bash_command_raw_silent(szComm, NULL);
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "echo \"Ruby: %s\" >> %s", szLog, CONFIG_FILE_FULLPATH_MAJESTIC_LOG);
      hw_execute_bash_command_raw_silent(szComm, NULL);
      return;
   }

   char* pszLog = (char*)malloc(strlen(szLog)+1);
   if ( NULL == pszLog )
      return;
   strcpy(pszLog, szLog);
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   pthread_create(&s_ThreadMajLogEntry, &attr, &_thread_majestic_log_entry, pszLog);
   pthread_attr_destroy(&attr);
}

int hardware_camera_maj_init()
{
   s_iPIDMajestic = hw_process_exists("majestic");
   return s_iPIDMajestic;
}

int hardware_camera_maj_get_current_pid()
{
   return s_iPIDMajestic;
}

bool hardware_camera_maj_start_capture_program(bool bEnableLog)
{
   if ( 0 != s_iPIDMajestic )
      return true;

   char szOutput[1024];
   char szComm[256];

   int iStartCount = 4;
   while ( iStartCount > 0 )
   {
      iStartCount--;
      if ( bEnableLog )
         sprintf(szComm, "/usr/bin/majestic -s 2>/dev/null 1>%s &", CONFIG_FILE_FULLPATH_MAJESTIC_LOG);
      else
         sprintf(szComm, "/usr/bin/majestic -s 2>/dev/null 1>/dev/null &");

      hw_execute_bash_command_raw(szComm, NULL);
      hardware_sleep_ms(100);
      s_iPIDMajestic = hw_process_exists("majestic");
      int iRetries = 10;
      while ( (0 == s_iPIDMajestic) && (iRetries > 0) )
      {
         iRetries--;
         hardware_sleep_ms(50);
         s_iPIDMajestic = hw_process_exists("majestic");
      }
      if ( s_iPIDMajestic != 0 )
         return true;

      hw_execute_bash_command_raw("ps -aef | grep majestic | grep -v \"grep\"", szOutput);
      log_line("[HwCamMajestic] Found majestic PID(s): (%s)", szOutput);
      removeTrailingNewLines(szOutput);
      hw_execute_bash_command_raw("ps -aef | grep ruby_rt_vehicle | grep -v \"grep\"", szOutput);
      removeTrailingNewLines(szOutput);
      log_line("[HwCamMajestic] Found ruby PID(s): (%s)", szOutput);
      hardware_sleep_ms(500);
   }
   return false;
}

bool _hardware_camera_maj_signal_stop_capture_program(int iSignal)
{
   char szOutput[256];
   szOutput[0] = 0;
   //hw_execute_bash_command_raw("/etc/init.d/S95majestic stop", szOutput);
   hw_execute_bash_command_raw("killall -9 majestic", szOutput);

   removeTrailingNewLines(szOutput);
   log_line("[HwCamMajestic] Result of stop majestic using killall: (%s)", szOutput);
   hardware_sleep_ms(100);

   hw_execute_bash_command_raw("pidof majestic", szOutput);
   removeTrailingNewLines(szOutput);
   log_line("[HwCamMajestic] Majestic PID after killall command: (%s)", szOutput);
   if ( strlen(szOutput) < 2 )
   {
      s_iPIDMajestic = 0;
      return true;    
   }


   hw_execute_bash_command_raw("killall -1 majestic", NULL);
   hardware_sleep_ms(10);
   hw_execute_bash_command_raw("pidof majestic", szOutput);
   removeTrailingNewLines(szOutput);
   log_line("[HwCamMajestic] Majestic PID after killall command: (%s)", szOutput);
   if ( strlen(szOutput) < 2 )
   {
      s_iPIDMajestic = 0;
      return true;    
   }

   hw_kill_process("majestic", iSignal);
   hardware_sleep_ms(10);
 
   hw_execute_bash_command_raw("pidof majestic", szOutput);
   removeTrailingNewLines(szOutput);
   log_line("[HwCamMajestic] Majestic PID after stop command: (%s)", szOutput);
   if ( strlen(szOutput) > 2 )
      return false;

   s_iPIDMajestic = 0;
   return true;
}

bool hardware_camera_maj_stop_capture_program()
{
   if ( _hardware_camera_maj_signal_stop_capture_program(-1) )
   {
      s_iPIDMajestic = 0;
      log_line("[HwCamMajestic] Stopped majestic.");
      return true;
   }
   hardware_sleep_ms(50);
   if ( _hardware_camera_maj_signal_stop_capture_program(-9) )
   {
      s_iPIDMajestic = 0;
      log_line("[HwCamMajestic] Stopped majestic.");
      return true;
   }
   hardware_sleep_ms(50);

   char szPID[256];
   szPID[0] = 0;
   hw_execute_bash_command_raw("pidof majestic", szPID);
   removeTrailingNewLines(szPID);
   log_line("[HwCamMajestic] Stopping majestic: PID after try signaling to stop: (%s)", szPID);
   int iRetry = 15;
   while ( (iRetry > 0) && (0 < strlen(szPID)) )
   {
      iRetry--;
      hardware_sleep_ms(50);
      hw_execute_bash_command_raw("killall -1 majestic", NULL);
      hardware_sleep_ms(100);
      hw_execute_bash_command_raw("pidof majestic", szPID);
      removeTrailingNewLines(szPID);
   }

   log_line("[HwCamMajestic] Init: stopping majestic (2): PID after force try stop: (%s)", szPID);
   if ( strlen(szPID) < 2 )
   {
      s_iPIDMajestic = 0;
      log_line("[HwCamMajestic] Stopped majestic.");
      return true;
   }

   hw_kill_process("majestic", -9);
   hw_execute_bash_command_raw("pidof majestic", szPID);

   s_iPIDMajestic = atoi(szPID);
   if ( strlen(szPID) < 2 )
   {
      s_iPIDMajestic = 0;
      log_line("[HwCamMajestic] Stopped majestic.");
      return true;
   }
   log_softerror_and_alarm("[HwCamMajestic] Init: failed to stop majestic. Current majestic PID: (%s) %d", szPID, s_iPIDMajestic);
   return false;
}

void _hardware_camera_maj_set_image_params()
{
   s_uTempMajesticBrightness = s_CurrentMajesticCamSettings.brightness;
   s_uTempMajesticContrast = s_CurrentMajesticCamSettings.contrast;
   s_uTempMajesticHue = s_CurrentMajesticCamSettings.hue;
   s_uTempMajesticSaturation = s_CurrentMajesticCamSettings.saturation;

   char szComm[128];
   sprintf(szComm, "cli -s .image.luminance %d", s_CurrentMajesticCamSettings.brightness);
   hw_execute_bash_command_raw(szComm, NULL);

   sprintf(szComm, "cli -s .image.contrast %d", s_CurrentMajesticCamSettings.contrast);
   hw_execute_bash_command_raw(szComm, NULL);

   sprintf(szComm, "cli -s .image.saturation %d", s_CurrentMajesticCamSettings.saturation/2);
   hw_execute_bash_command_raw(szComm, NULL);

   sprintf(szComm, "cli -s .image.hue %d", s_CurrentMajesticCamSettings.hue);
   hw_execute_bash_command_raw(szComm, NULL);

   if ( s_CurrentMajesticCamSettings.uFlags & CAMERA_FLAG_OPENIPC_3A_SIGMASTAR )
      hw_execute_bash_command_raw("cli -s .fpv.enabled true", NULL);
   else
      hw_execute_bash_command_raw("cli -s .fpv.enabled false", NULL);

   if ( s_CurrentMajesticCamSettings.flip_image )
   {
      hw_execute_bash_command_raw("cli -s .image.flip true", NULL);
      hw_execute_bash_command_raw("cli -s .image.mirror true", NULL);
   }
   else
   {
      hw_execute_bash_command_raw("cli -s .image.flip false", NULL);
      hw_execute_bash_command_raw("cli -s .image.mirror false", NULL);
   }

   if ( 0 == s_CurrentMajesticCamSettings.shutterspeed )
   {
      sprintf(szComm, "cli -d .isp.exposure");
      hw_execute_bash_command_raw(szComm, NULL);      
   }
   else
   {
      //if ( hardware_board_is_goke(hardware_getBoardType()) )
      //   sprintf(szComm, "cli -s .isp.exposure %.2f", (float)pCameraParams->shutterspeed/1000.0);

      // exposure is in milisec for ssc338q
      if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
         sprintf(szComm, "cli -s .isp.exposure %d", s_CurrentMajesticCamSettings.shutterspeed);
      hw_execute_bash_command_raw(szComm, NULL);
   }

   hardware_camera_maj_set_irfilter_off(s_CurrentMajesticCamSettings.uFlags & CAMERA_FLAG_IR_FILTER_OFF, false);
   hardware_camera_maj_add_log("Applied settings. Signal majestic...", false);
   hw_execute_bash_command_raw("killall -1 majestic", NULL);
}

void* _thread_majestic_set_image_params(void *argument)
{
   s_bMajThreadSetImageParamsRunning = true;
   log_line("[HwCamMajestic] Started thread to set image params...");
   _hardware_camera_maj_set_image_params();
   log_line("[HwCamMajestic] Finished thread to set image params.");
   s_bMajThreadSetImageParamsRunning = false;
   return NULL;
}

void hardware_camera_maj_apply_image_settings(camera_profile_parameters_t* pCameraParams, bool bAsync)
{
   if ( NULL == pCameraParams )
   {
      log_softerror_and_alarm("[HwCamMajestic] Received invalid params to set majestic image settings.");
      return;
   }

   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetImageParamsRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to apply image params is running. Stop it.");
      pthread_cancel(s_ThreadMajSetImageParams);
   }

   memcpy(&s_CurrentMajesticCamSettings, pCameraParams, sizeof(camera_profile_parameters_t));

   if ( ! bAsync )
   {
      _hardware_camera_maj_set_image_params();
      return;
   }
   s_bMajThreadSetImageParamsRunning = true;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   if ( 0 != pthread_create(&s_ThreadMajSetImageParams, &attr, &_thread_majestic_set_image_params, NULL) )
   {
      s_bMajThreadSetImageParamsRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread to set image params. Set them manualy.");
      _hardware_camera_maj_set_image_params();
   }
   pthread_attr_destroy(&attr);
}


void _hardware_cam_maj_set_nal_size()
{
   if ( NULL == s_pCurrentMajesticModel )
      return;

   // Allow room for video header important and 5 bytes for NAL header
   
   int iVideoProfile = s_iCurrentMajesticVideoProfile;
   if ( -1 == iVideoProfile )
      iVideoProfile = s_pCurrentMajesticModel->video_params.user_selected_video_link_profile;

   int iNALSize = s_pCurrentMajesticModel->video_link_profiles[iVideoProfile].video_data_length;
   iNALSize -= 6; // NAL delimitator+header (00 00 00 01 [aa] [bb])
   iNALSize -= sizeof(t_packet_header_video_segment_important);
   iNALSize = iNALSize - (iNALSize % 4);
   s_iCurrentMajesticNALSize = iNALSize;
   log_line("[HwCamMajestic] Set majestic NAL size to %d bytes (for video profile index: %d, %s)", iNALSize, iVideoProfile, str_get_video_profile_name(iVideoProfile));
   char szComm[256];
   sprintf(szComm, "cli -s .outgoing.naluSize %d", iNALSize);
   hw_execute_bash_command_raw(szComm, NULL);
   hardware_sleep_ms(1);
   hw_execute_bash_command_raw("killall -1 majestic", NULL);
   hardware_sleep_ms(5);
}

void* _thread_majestic_set_nal_size(void *argument)
{
   s_bMajThreadSetNALSizeRunning = true;
   log_line("[HwCamMajestic] Started thread to set NAL size...");
   _hardware_cam_maj_set_nal_size();
   s_bMajThreadSetNALSizeRunning = false;
   log_line("[HwCamMajestic] Finished thread to set NAL size.");
   return NULL; 
}

int hardware_camera_maj_get_current_nal_size()
{
   return s_iCurrentMajesticNALSize;
}

void hardware_camera_maj_update_nal_size(Model* pModel, bool bAsync)
{
   if ( NULL == pModel )
      return;
   s_pCurrentMajesticModel = pModel;
   int iVideoProfile = s_iCurrentMajesticVideoProfile;
   if ( (-1 == iVideoProfile) && (NULL != s_pCurrentMajesticModel) )
      iVideoProfile = s_pCurrentMajesticModel->video_params.user_selected_video_link_profile;

   int iNALSize = s_pCurrentMajesticModel->video_link_profiles[iVideoProfile].video_data_length;
   iNALSize -= 6; // NAL delimitator+header (00 00 00 01 [aa] [bb])
   iNALSize -= sizeof(t_packet_header_video_segment_important);
   iNALSize = iNALSize - (iNALSize % 4);


   if ( iNALSize == s_iCurrentMajesticNALSize )
   {
      log_line("[HwCamMajestic] Received req to update nal size, but it's unchanged: %d bytes", s_iCurrentMajesticNALSize);
      return;
   }
   log_line("[HwCamMajestic] Received req to update %s nal size, from %d bytes to %d bytes", bAsync?"async":"sync", s_iCurrentMajesticNALSize, iNALSize);

   if ( s_bMajThreadSetNALSizeRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set NAL size is running. Stop it.");
      pthread_cancel(s_ThreadMajSetNALSize);
   }

   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( ! bAsync )
   {
      s_bMajThreadSetNALSizeRunning = false;
      _hardware_cam_maj_set_nal_size();
      return;
   }
   s_bMajThreadSetNALSizeRunning = true;
   //pthread_attr_t attr;
   //_adjust_thread_priority_attrs(&attr);
   if ( 0 != pthread_create(&s_ThreadMajSetNALSize, NULL, &_thread_majestic_set_nal_size, NULL) )
   {
      s_bMajThreadSetNALSizeRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set NAL size manualy.");
      _hardware_cam_maj_set_nal_size();
   }
   //pthread_attr_destroy(&attr);
}


void _hardware_camera_maj_set_all_params()
{
   char szComm[128];

   if ( s_CurrentMajesticVideoParams.iH264Slices <= 1 )
   {
      hw_execute_bash_command_raw("cli -s .video0.sliceUnits 0", NULL);
      //hw_execute_bash_command_raw("cli -d .video0.sliceUnits", NULL);
   }
   else
   {
      sprintf(szComm, "cli -s .video0.sliceUnits %d", s_CurrentMajesticVideoParams.iH264Slices);
      hw_execute_bash_command_raw(szComm, NULL);
   }

   if ( s_CurrentMajesticVideoParams.uVideoExtraFlags & VIDEO_FLAG_GENERATE_H265 )
      hw_execute_bash_command_raw("cli -s .video0.codec h265", NULL);
   else
      hw_execute_bash_command_raw("cli -s .video0.codec h264", NULL);

   sprintf(szComm, "cli -s .video0.fps %d", s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].fps);
   hw_execute_bash_command_raw(szComm, NULL);

   s_uCurrentMajesticBitrate = s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].bitrate_fixed_bps/1000;
   if ( s_uTemporaryMajesticBitrate > 0 )
      s_uCurrentMajesticBitrate = s_uTemporaryMajesticBitrate;

   sprintf(szComm, "cli -s .video0.bitrate %u", s_uCurrentMajesticBitrate);
   hw_execute_bash_command_raw(szComm, NULL);

   sprintf(szComm, "cli -s .video0.size %dx%d", s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].width, s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].height);
   hw_execute_bash_command_raw(szComm, NULL);

   s_iCurrentMajesticQPDelta = s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].iIPQuantizationDelta;
   if ( s_iTemporaryMajesticQPDelta > -100 )
      s_iCurrentMajesticQPDelta = s_iTemporaryMajesticQPDelta;
   sprintf(szComm, "cli -s .video0.qpDelta %d", s_iCurrentMajesticQPDelta);
   hw_execute_bash_command_raw(szComm, NULL);

   s_fCurrentMajesticGOP = 0.5;
   int keyframe_ms = s_pCurrentMajesticModel->getInitialKeyframeIntervalMs(s_iCurrentMajesticVideoProfile);
   s_fCurrentMajesticGOP = ((float)keyframe_ms) / 1000.0;
   if ( s_fTemporaryMajesticGOP > 0 )
      s_fCurrentMajesticGOP = s_fTemporaryMajesticGOP;

   sprintf(szComm, "cli -s .video0.gopSize %.1f", s_fCurrentMajesticGOP);
   hw_execute_bash_command_raw(szComm, NULL);

   // Allow room for video header important and 5 bytes for NAL header
   int iNALSize = s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].video_data_length;
   iNALSize -= 6; // NAL delimitator+header (00 00 00 01 [aa] [bb])
   iNALSize -= sizeof(t_packet_header_video_segment_important);
   iNALSize = iNALSize - (iNALSize % 4);
   s_iCurrentMajesticNALSize = iNALSize;
   log_line("[HwCamMajestic] Set majestic NAL size to %d bytes (for video profile index: %d, %s)", iNALSize, s_iCurrentMajesticVideoProfile, str_get_video_profile_name(s_iCurrentMajesticVideoProfile));
   sprintf(szComm, "cli -s .outgoing.naluSize %d", iNALSize);
   hw_execute_bash_command_raw(szComm, NULL);


   u32 uNoiseLevel = s_pCurrentMajesticModel->video_link_profiles[s_iCurrentMajesticVideoProfile].uProfileFlags & VIDEO_PROFILE_FLAGS_MASK_NOISE;
   if ( uNoiseLevel >= 2 )
      hw_execute_bash_command_raw("cli -d .fpv.noiseLevel", NULL);
   else if ( uNoiseLevel == 1 )
      hw_execute_bash_command_raw("cli -s .fpv.noiseLevel 1", NULL);
   else
      hw_execute_bash_command_raw("cli -s .fpv.noiseLevel 0", NULL);

   hardware_camera_maj_apply_image_settings(&s_CurrentMajesticVideoCamSettings, false);
}

void* _thread_majestic_set_all_params(void *argument)
{
   s_bMajThreadSetAllParamsRunning = true;
   log_line("[HwCamMajestic] Started thread to set all params...");
   _hardware_camera_maj_set_all_params();
   log_line("[HwCamMajestic] Finished thread to set all params.");
   s_bMajThreadSetAllParamsRunning = false;
   return NULL;
}

void hardware_camera_maj_apply_all_settings(Model* pModel, camera_profile_parameters_t* pCameraParams, int iVideoProfile, video_parameters_t* pVideoParams, bool bAsync)
{
   if ( (NULL == pCameraParams) || (NULL == pModel) || (iVideoProfile < 0) || (NULL == pVideoParams) )
   {
      log_softerror_and_alarm("[HwCamMajestic] Received invalid params to set majestic settings.");
      return;
   }
   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetAllParamsRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to apply all params is running. Stop it.");
      pthread_cancel(s_ThreadMajSetAllParams);
   }

   memcpy(&s_CurrentMajesticVideoCamSettings, pCameraParams, sizeof(camera_profile_parameters_t));
   memcpy(&s_CurrentMajesticVideoParams, pVideoParams, sizeof(video_parameters_t));
   s_pCurrentMajesticModel = pModel;
   s_iCurrentMajesticVideoProfile = iVideoProfile;

   s_uMajesticHasPendingCachedImageChangesTime = 0;

   if ( ! bAsync )
   {
      _hardware_camera_maj_set_all_params();
      return;
   }
   s_bMajThreadSetAllParamsRunning = true;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   if ( 0 != pthread_create(&s_ThreadMajSetAllParams, &attr, &_thread_majestic_set_all_params, NULL) )
   {
      s_bMajThreadSetAllParamsRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread to set all params. Set them manualy.");
      _hardware_camera_maj_set_all_params();
   }
   pthread_attr_destroy(&attr);
}

void _hardware_camera_maj_set_irfilter_off_sync()
{
   // IR cut filer off?
   if ( s_iLastMajesticIRFilterMode )
   {
      if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
      {
         hw_execute_bash_command("gpio set 23", NULL);
         hw_execute_bash_command("gpio clear 24", NULL);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE200 )
      {
         hw_execute_bash_command("gpio set 14", NULL);
         hw_execute_bash_command("gpio clear 15", NULL);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE210 )
      {
         hw_execute_bash_command("gpio set 13", NULL);
         hw_execute_bash_command("gpio clear 15", NULL);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE300 )
      {
         hw_execute_bash_command("gpio set 10", NULL);
         hw_execute_bash_command("gpio clear 11", NULL);
      }
   }
   else
   {
      if ( hardware_board_is_sigmastar(hardware_getBoardType()) )
      {
         hw_execute_bash_command("gpio set 24", NULL);
         hw_execute_bash_command("gpio clear 23", NULL);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE200 )
      {
         hw_execute_bash_command("gpio set 15", NULL);
         hw_execute_bash_command("gpio clear 14", NULL);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE210 )
      {
         hw_execute_bash_command("gpio set 15", NULL);
         hw_execute_bash_command("gpio clear 13", NULL);
      }
      if ( (hardware_getBoardType() & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE300 )
      {
         hw_execute_bash_command("gpio set 11", NULL);
         hw_execute_bash_command("gpio clear 10", NULL);
      }
   }
}

void* _thread_majestic_set_irfilter_mode(void *argument)
{
   s_bMajThreadSetIRFilterRunning = true;
   log_line("[HwCamMajestic] Started thread to set IR filter mode...");
   _hardware_camera_maj_set_irfilter_off_sync();
   log_line("[HwCamMajestic] Finsished thread to set IR filter mode.");
   s_bMajThreadSetIRFilterRunning = false;
   return NULL;
}

void hardware_camera_maj_set_irfilter_off(int iOff, bool bAsync)
{
   if ( ! hardware_board_is_openipc(hardware_getBoardType()) )
      return;
   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetIRFilterRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set IR filter is running. Stop it.");
      pthread_cancel(s_ThreadMajSetIRFilter);
   }

   s_iLastMajesticIRFilterMode = iOff;
   if ( ! bAsync )
   {
      _hardware_camera_maj_set_irfilter_off_sync();
      return;
   }
   s_bMajThreadSetIRFilterRunning = true;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   if ( 0 != pthread_create(&s_ThreadMajSetIRFilter, &attr, &_thread_majestic_set_irfilter_mode, NULL) )
   {
      s_bMajThreadSetIRFilterRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set IR filter mode manualy.");
      _hardware_camera_maj_set_irfilter_off_sync();
   }
   pthread_attr_destroy(&attr);
}

void* _thread_majestic_set_daylight_mode(void *argument)
{
   s_bMajThreadSetDaylightModeRunning = true;
   log_line("[HwCamMajestic] Started thread to set daylight mode...");
   // Daylight Off? Activate Night Mode
   if (s_iLastMajesticDaylightMode)
      hw_execute_bash_command_raw("curl -s localhost/night/on", NULL);
   else 
      hw_execute_bash_command_raw("curl -s localhost/night/off", NULL);
   log_line("[HwCamMajestic] Finished thread to set daylight mode.");
   s_bMajThreadSetDaylightModeRunning = false;
   return NULL; 
}

void hardware_camera_maj_set_daylight_off(int iDLOff)
{
   if ( !hardware_board_is_openipc(hardware_getBoardType()) )
      return;
   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( iDLOff == s_iLastMajesticDaylightMode )
      return;

   if ( s_bMajThreadSetDaylightModeRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set daylight mode is running. Stop it.");
      pthread_cancel(s_ThreadMajSetDaylightMode);
   }

   s_iLastMajesticDaylightMode = iDLOff;

   s_bMajThreadSetDaylightModeRunning = true;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   if ( 0 != pthread_create(&s_ThreadMajSetDaylightMode, &attr, &_thread_majestic_set_daylight_mode, NULL) )
   {
      s_bMajThreadSetDaylightModeRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set daylight mode manualy.");
      // Daylight Off? Activate Night Mode
      if (s_iLastMajesticDaylightMode)
         hw_execute_bash_command_raw("curl -s localhost/night/on", NULL);
      else 
         hw_execute_bash_command_raw("curl -s localhost/night/off", NULL);
   }
   pthread_attr_destroy(&attr);
}

void* _thread_majestic_set_brightness(void *argument)
{
   s_bMajThreadSetBrightnessRunning = true;
   log_line("[HwCamMajestic] Started thread to set brightness...");
   char szComm[128];
   sprintf(szComm, "curl -s localhost/api/v1/set?image.luminance=%u", s_uTempMajesticBrightness);
   hw_execute_bash_command_raw(szComm, NULL);
   s_bMajThreadSetBrightnessRunning = false;
   log_line("[HwCamMajestic] Finished thread to set brightness.");
   return NULL; 
}

void hardware_camera_maj_set_brightness(u32 uValue)
{
   if ( uValue == s_uTempMajesticBrightness )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetBrightnessRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set brightness is running. Stop it.");
      pthread_cancel(s_ThreadMajSetBrightness);
   }

   s_uTempMajesticBrightness = uValue;
   s_uMajesticHasPendingCachedImageChangesTime = get_current_timestamp_ms();

   s_bMajThreadSetBrightnessRunning = true;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   if ( 0 != pthread_create(&s_ThreadMajSetBrightness, &attr, &_thread_majestic_set_brightness, NULL) )
   {
      s_bMajThreadSetBrightnessRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set brightness manualy.");
      char szComm[128];
      sprintf(szComm, "curl -s localhost/api/v1/set?image.luminance=%u", s_uTempMajesticBrightness);
      hw_execute_bash_command_raw(szComm, NULL);
   }
   pthread_attr_destroy(&attr);
}

void* _thread_majestic_set_contrast(void *argument)
{
   s_bMajThreadSetContrastRunning = true;
   log_line("[HwCamMajestic] Started thread to set contrast...");
   char szComm[128];
   sprintf(szComm, "curl -s localhost/api/v1/set?image.contrast=%u", s_uTempMajesticContrast);
   hw_execute_bash_command_raw(szComm, NULL);
   s_bMajThreadSetContrastRunning = false;
   log_line("[HwCamMajestic] Finished thread to set contrast.");
   return NULL; 
}

void hardware_camera_maj_set_contrast(u32 uValue)
{
   if ( uValue == s_uTempMajesticContrast )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetContrastRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set contrast is running. Stop it.");
      pthread_cancel(s_ThreadMajSetContrast);
   }

   s_uTempMajesticContrast = uValue;
   s_uMajesticHasPendingCachedImageChangesTime = get_current_timestamp_ms();

   s_bMajThreadSetContrastRunning = true;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   if ( 0 != pthread_create(&s_ThreadMajSetContrast, &attr, &_thread_majestic_set_contrast, NULL) )
   {
      s_bMajThreadSetContrastRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set contrast manualy.");
      char szComm[128];
      sprintf(szComm, "curl -s localhost/api/v1/set?image.contrast=%u", s_uTempMajesticContrast);
      hw_execute_bash_command_raw(szComm, NULL);
   }
   pthread_attr_destroy(&attr);
}

void* _thread_majestic_set_hue(void *argument)
{
   s_bMajThreadSetHueRunning = true;
   log_line("[HwCamMajestic] Started thread to set hue...");
   char szComm[128];
   sprintf(szComm, "curl -s localhost/api/v1/set?image.hue=%u", s_uTempMajesticHue);
   hw_execute_bash_command_raw(szComm, NULL);
   s_bMajThreadSetHueRunning = false;
   log_line("[HwCamMajestic] Finished thread to set hue.");
   return NULL; 
}

void hardware_camera_maj_set_hue(u32 uValue)
{
   if ( uValue == s_uTempMajesticHue )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetHueRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set hue is running. Stop it.");
      pthread_cancel(s_ThreadMajSetHue);
   }

   s_uTempMajesticHue = uValue;
   s_uMajesticHasPendingCachedImageChangesTime = get_current_timestamp_ms();

   s_bMajThreadSetHueRunning = true;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   if ( 0 != pthread_create(&s_ThreadMajSetHue, &attr, &_thread_majestic_set_hue, NULL) )
   {
      s_bMajThreadSetHueRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set hue manualy.");
      char szComm[128];
      sprintf(szComm, "curl -s localhost/api/v1/set?image.hue=%u", s_uTempMajesticHue);
      hw_execute_bash_command_raw(szComm, NULL);
   }
   pthread_attr_destroy(&attr);
}

void* _thread_majestic_set_saturation(void *argument)
{
   s_bMajThreadSetSaturationRunning = true;
   log_line("[HwCamMajestic] Started thread to set saturation...");
   char szComm[128];
   sprintf(szComm, "curl -s localhost/api/v1/set?image.saturation=%u", s_uTempMajesticSaturation/2);
   hw_execute_bash_command_raw(szComm, NULL);
   s_bMajThreadSetSaturationRunning = false;
   log_line("[HwCamMajestic] Finished thread to set saturation.");
   return NULL; 
}

void hardware_camera_maj_set_saturation(u32 uValue)
{
   if ( uValue == s_uTempMajesticSaturation )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetSaturationRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set saturation is running. Stop it.");
      pthread_cancel(s_ThreadMajSetStaturation);
   }

   s_uTempMajesticSaturation = uValue;
   s_uMajesticHasPendingCachedImageChangesTime = get_current_timestamp_ms();

   s_bMajThreadSetSaturationRunning = true;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   if ( 0 != pthread_create(&s_ThreadMajSetStaturation, &attr, &_thread_majestic_set_saturation, NULL) )
   {
      s_bMajThreadSetSaturationRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set hue manualy.");
      char szComm[128];
      sprintf(szComm, "curl -s localhost/api/v1/set?image.saturation=%u", s_uTempMajesticSaturation/2);
      hw_execute_bash_command_raw(szComm, NULL);
   }
   pthread_attr_destroy(&attr);
}

void hardware_camera_maj_clear_temp_values()
{
   s_fTemporaryMajesticGOP = -1;
   s_uTemporaryMajesticBitrate = 0;
   s_iTemporaryMajesticQPDelta = -1000;
   log_line("[HwCamMajestic] Cleared temp runtime values.");
}

void* _thread_majestic_set_temp_gop(void *argument)
{
   s_bMajThreadSetGOPRunning = true;
   log_line("[HwCamMajestic] Started thread to set GOP to %.1f ...", s_fTemporaryMajesticGOP);
   char szComm[128];
   sprintf(szComm, "curl -s localhost/api/v1/set?video0.gopSize=%.1f", s_fTemporaryMajesticGOP);
   hw_execute_bash_command_raw(szComm, NULL);
   s_bMajThreadSetGOPRunning = false;
   log_line("[HwCamMajestic] Finished thread to set GOP.");
   return NULL; 
}

void hardware_camera_maj_set_keyframe(float fGOP)
{
   if ( (fabs(fGOP-s_fCurrentMajesticGOP)<0.0001) && (fabs(fGOP-s_fTemporaryMajesticGOP)<0.0001) )
      return;
   if ( (s_fTemporaryMajesticGOP < 0.0001) && (fabs(fGOP-s_fCurrentMajesticGOP)<0.0001) )
      return;
   if ( fGOP <= 0.0001 )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetGOPRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set GOP is running. Stop it.");
      pthread_cancel(s_ThreadMajSetGOP);
   }

   s_fTemporaryMajesticGOP = fGOP;

   s_bMajThreadSetGOPRunning = true;
   //pthread_attr_t attr;
   //_adjust_thread_priority_attrs(&attr);
   //pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
   if ( 0 != pthread_create(&s_ThreadMajSetGOP, NULL, &_thread_majestic_set_temp_gop, NULL) )
   {
      s_bMajThreadSetGOPRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set GOP manualy.");
      char szComm[128];
      sprintf(szComm, "curl -s localhost/api/v1/set?video0.gopSize=%.1f", s_fTemporaryMajesticGOP);
      hw_execute_bash_command_raw(szComm, NULL);
   }
   //pthread_attr_destroy(&attr);
}

void* _thread_majestic_set_bitrate(void *argument)
{
   s_bMajThreadSetBitrateRunning = true;
   u32 uBitrate = s_uCurrentMajesticBitrate;
   if ( s_uTemporaryMajesticBitrate != 0 )
      uBitrate = s_uTemporaryMajesticBitrate;
   log_line("[HwCamMajestic] Started thread to set bitrate to %u bps ...", uBitrate);
   
   char szComm[128];
   sprintf(szComm, "curl -s localhost/api/v1/set?video0.bitrate=%u", uBitrate/1000);
   hw_execute_bash_command_raw(szComm, NULL);

   s_bMajThreadSetBitrateRunning = false;
   log_line("[HwCamMajestic] Finished thread to set bitrate.");
   return NULL; 
}

u32 hardware_camera_maj_get_current_bitrate()
{
   if ( 0 != s_uTemporaryMajesticBitrate )
      return s_uTemporaryMajesticBitrate;
   return s_uCurrentMajesticBitrate;
}

void hardware_camera_maj_set_bitrate(u32 uBitrate)
{
   if ( (uBitrate == s_uCurrentMajesticBitrate) && (uBitrate == s_uTemporaryMajesticBitrate) )
      return;
   if ( (0 == s_uTemporaryMajesticBitrate) && (uBitrate == s_uCurrentMajesticBitrate) )
      return;
   if ( 0 == uBitrate )
      return;
   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetBitrateRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set bitrate is running. Stop it.");
      pthread_cancel(s_ThreadMajSetBitrate);
   }

   s_uTemporaryMajesticBitrate = uBitrate;
   
   s_bMajThreadSetBitrateRunning = true;
   //pthread_attr_t attr;
   //_adjust_thread_priority_attrs(&attr);
   //pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
   if ( 0 != pthread_create(&s_ThreadMajSetBitrate, NULL, &_thread_majestic_set_bitrate, NULL) )
   {
      s_bMajThreadSetBitrateRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set bitrate manualy.");
      char szComm[128];
      sprintf(szComm, "curl -s localhost/api/v1/set?video0.bitrate=%u", s_uTemporaryMajesticBitrate/1000);
      hw_execute_bash_command_raw(szComm, NULL);
   }
   //pthread_attr_destroy(&attr);
}

void* _thread_majestic_set_qpdelta(void *argument)
{
   s_bMajThreadSetQPDeltaRunning = true;
   int iQPDelta = s_iCurrentMajesticQPDelta;
   if ( s_iTemporaryMajesticQPDelta > -100 )
      iQPDelta = s_iTemporaryMajesticQPDelta;
   log_line("[HwCamMajestic] Started thread to set QP delta to %d ...", iQPDelta);
   
   char szComm[128];
   sprintf(szComm, "curl -s localhost/api/v1/set?video0.qpDelta=%d", iQPDelta);
   hw_execute_bash_command_raw(szComm, NULL);

   s_bMajThreadSetQPDeltaRunning = false;
   log_line("[HwCamMajestic] Finished thread to set QP delta.");
   return NULL; 
}

int hardware_camera_maj_get_current_qpdelta()
{
   if ( s_iTemporaryMajesticQPDelta > -100 )
      return s_iTemporaryMajesticQPDelta;
   return s_iCurrentMajesticQPDelta;
}

void hardware_camera_maj_set_qpdelta(int iQPDelta)
{
   if ( (iQPDelta == s_iTemporaryMajesticQPDelta) && (iQPDelta == s_iCurrentMajesticQPDelta) )
      return;
   if ( (s_iTemporaryMajesticQPDelta < -100) && (iQPDelta == s_iCurrentMajesticQPDelta) )
      return;

   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetQPDeltaRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set qp delta is running. Stop it.");
      pthread_cancel(s_ThreadMajSetQPDelta);
   }

   s_iTemporaryMajesticQPDelta = iQPDelta;
   
   s_bMajThreadSetQPDeltaRunning = true;
   //pthread_attr_t attr;
   //_adjust_thread_priority_attrs(&attr);
   //pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
   if ( 0 != pthread_create(&s_ThreadMajSetQPDelta, NULL, &_thread_majestic_set_qpdelta, NULL) )
   {
      s_bMajThreadSetQPDeltaRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set qp delta manualy.");
      char szComm[128];
      sprintf(szComm, "curl -s localhost/api/v1/set?video0.qpDelta=%d", iQPDelta);
      hw_execute_bash_command_raw(szComm, NULL);
   }
   //pthread_attr_destroy(&attr);
}

void* _thread_majestic_set_bitrate_qpdelta(void *argument)
{
   s_bMajThreadSetBitrateQPDeltaRunning = true;
   u32 uBitrate = s_uCurrentMajesticBitrate;
   if ( s_uTemporaryMajesticBitrate != 0 )
      uBitrate = s_uTemporaryMajesticBitrate;
   int iQPDelta = s_iCurrentMajesticQPDelta;
   if ( s_iTemporaryMajesticQPDelta > -100 )
      iQPDelta = s_iTemporaryMajesticQPDelta;

   log_line("[HwCamMajestic] Started thread to set bitrate to %u bps and QP delta to %d ...", uBitrate, iQPDelta);
   
   char szComm[128];
   
   sprintf(szComm, "curl -s localhost/api/v1/set?video0.bitrate=%u", uBitrate/1000);
   hw_execute_bash_command_raw(szComm, NULL);

   sprintf(szComm, "curl -s localhost/api/v1/set?video0.qpDelta=%d", iQPDelta);
   hw_execute_bash_command_raw(szComm, NULL);

   s_bMajThreadSetBitrateQPDeltaRunning = false;
   log_line("[HwCamMajestic] Finished thread to set bitrate and QP delta.");
   return NULL; 
}

void hardware_camera_maj_set_bitrate_and_qpdelta(u32 uBitrate, int iQPDelta)
{
   if ( (iQPDelta == s_iCurrentMajesticQPDelta) && (iQPDelta == s_iTemporaryMajesticQPDelta) &&
        (uBitrate == s_uCurrentMajesticBitrate) && (uBitrate == s_uTemporaryMajesticBitrate) )
      return;

   bool bBitrateDif = true;
   if ( (uBitrate == s_uCurrentMajesticBitrate) && (uBitrate == s_uTemporaryMajesticBitrate) )
      bBitrateDif = false;
   if ( (0 == s_uTemporaryMajesticBitrate) && (uBitrate == s_uCurrentMajesticBitrate) )
      bBitrateDif = false;

   bool bQPDiff = true;
   if ( (iQPDelta == s_iTemporaryMajesticQPDelta) && (iQPDelta == s_iCurrentMajesticQPDelta) )
      bQPDiff = false;
   if ( (s_iTemporaryMajesticQPDelta < -100) && (iQPDelta == s_iCurrentMajesticQPDelta) )
      bQPDiff = false;

   if ( (!bBitrateDif) && (!bQPDiff) )
      return;

   if ( iQPDelta < -100 )
   {
      hardware_camera_maj_set_bitrate(uBitrate);
      return;
   }
   if ( uBitrate == 0 )
   {
      hardware_camera_maj_set_qpdelta(iQPDelta);
      return;
   }
   s_uMajesticLastChangeTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetBitrateQPDeltaRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set bitrate and qp delta is running. Stop it.");
      pthread_cancel(s_ThreadMajSetBitrateQPDelta);
   }

   s_uTemporaryMajesticBitrate = uBitrate;
   s_iTemporaryMajesticQPDelta = iQPDelta;
   
   s_bMajThreadSetBitrateQPDeltaRunning = true;
   //pthread_attr_t attr;
   //_adjust_thread_priority_attrs(&attr);
   //pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
   if ( 0 != pthread_create(&s_ThreadMajSetBitrateQPDelta, NULL, &_thread_majestic_set_bitrate_qpdelta, NULL) )
   {
      s_bMajThreadSetBitrateQPDeltaRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set bitrate and qp delta manualy.");
      char szComm[128];
      sprintf(szComm, "curl -s localhost/api/v1/set?video0.bitrate=%u", s_uTemporaryMajesticBitrate/1000);
      hw_execute_bash_command_raw(szComm, NULL);
      sprintf(szComm, "curl -s localhost/api/v1/set?video0.qpDelta=%d", s_iTemporaryMajesticQPDelta);
      hw_execute_bash_command_raw(szComm, NULL);
   }
   //pthread_attr_destroy(&attr);
}


void _apply_cached_majestic_image_settings()
{
   char szComm[128];

   if ( MAX_U32 != s_uTempMajesticBrightness )
   {
      sprintf(szComm, "cli -s .image.luminance %u", s_uTempMajesticBrightness);
      hw_execute_bash_command_raw(szComm, NULL);
   }

   if ( MAX_U32 != s_uTempMajesticContrast )
   {
      sprintf(szComm, "cli -s .image.contrast %u", s_uTempMajesticContrast);
      hw_execute_bash_command_raw(szComm, NULL);
   }

   if ( MAX_U32 != s_uTempMajesticHue )
   {
      sprintf(szComm, "cli -s .image.hue %u", s_uTempMajesticHue);
      hw_execute_bash_command_raw(szComm, NULL);
   }

   if ( MAX_U32 != s_uTempMajesticSaturation )
   {
      sprintf(szComm, "cli -s .image.saturation %u", s_uTempMajesticSaturation/2);
      hw_execute_bash_command_raw(szComm, NULL);
   }
}

void* _thread_majestic_apply_cached_image_changes(void *argument)
{
   log_line("[HwCamMajestic] Started thread to apply cached image changes...");
   _apply_cached_majestic_image_settings();
   log_line("[HwCamMajestic] Finished thread to apply cached image changes.");
   return NULL; 
}

u32 hardware_camera_maj_get_last_change_time()
{
   return s_uMajesticLastChangeTime;
}

void hardware_camera_maj_check_apply_cached_image_changes(u32 uTimeNow)
{
   if ( 0 == s_uMajesticHasPendingCachedImageChangesTime )
      return;

   if ( uTimeNow < s_uMajesticHasPendingCachedImageChangesTime + 2000 )
      return;

   s_uMajesticHasPendingCachedImageChangesTime = 0;
   pthread_t pt;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   if ( 0 != pthread_create(&pt, &attr, &_thread_majestic_apply_cached_image_changes, NULL) )
   {
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Apply cached image changes manualy.");
      _apply_cached_majestic_image_settings();
   }
   pthread_attr_destroy(&attr);
}

void hardware_camera_maj_enable_audio(bool bEnable, int iBitrate, int iVolume)
{
   log_line("[HwCamMajestic] Enable audio: %s", bEnable?"yes":"no");

   s_uMajesticLastChangeTime = s_uMajesticLastChangeAudioTime = get_current_timestamp_ms();
   char szComm[128];
   if ( bEnable )
   {
      s_iCurrentMajAudioVolume = iVolume;
      s_iCurrentMajAudioBitrate = iBitrate;
      if ( s_iCurrentMajAudioBitrate < 4000 )
         s_iCurrentMajAudioBitrate = 4000;
      if ( s_iCurrentMajAudioBitrate >= 32000 )
         s_iCurrentMajAudioBitrate = 48000;
      hw_execute_bash_command_raw("cli -s .audio.outputEnabled false", NULL);
      hw_execute_bash_command_raw("cli -s .audio.codec pcm", NULL);
      sprintf(szComm, "cli -s .audio.srate %d", s_iCurrentMajAudioBitrate);
      hw_execute_bash_command_raw(szComm, NULL);
      sprintf(szComm, "cli -s .audio.volume %d", s_iCurrentMajAudioVolume);
      hw_execute_bash_command_raw(szComm, NULL);
      hw_execute_bash_command_raw("cli -s .audio.enabled true", NULL);
   }
   else
   {
      hw_execute_bash_command_raw("cli -s .audio.enabled false", NULL);
   }
   hardware_sleep_ms(10);

   hw_execute_bash_command_raw("killall -1 majestic", NULL);
}


void* _thread_majestic_set_audio_volume(void *argument)
{
   s_bMajThreadSetAudioVolumeRunning = true;
   log_line("[HwCamMajestic] Started thread to set audio volume to %d ...", s_iCurrentMajAudioVolume);
   
   char szComm[128];
   sprintf(szComm, "cli -s .audio.volume %d", s_iCurrentMajAudioVolume);
   hw_execute_bash_command_raw(szComm, NULL);
   hw_execute_bash_command_raw("killall -1 majestic", NULL);

   s_bMajThreadSetAudioVolumeRunning = false;
   log_line("[HwCamMajestic] Finished thread to set audio volume.");
   return NULL; 
}


void hardware_camera_maj_set_audio_volume(int iVolume)
{
   if ( iVolume == s_iCurrentMajAudioVolume )
   {
      log_line("[HwCamMajestic] Received request to change audio volume, but it's unchanged: %d", iVolume);
      return;
   }
   s_uMajesticLastChangeTime = s_uMajesticLastChangeAudioTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetAudioVolumeRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set audio volume is running. Stop it.");
      pthread_cancel(s_ThreadMajSetAudioVolume);
   }

   s_iCurrentMajAudioVolume = iVolume;
   
   s_bMajThreadSetAudioVolumeRunning = true;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
   if ( 0 != pthread_create(&s_ThreadMajSetAudioVolume, &attr, &_thread_majestic_set_audio_volume, NULL) )
   {
      s_bMajThreadSetAudioVolumeRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set audio volume manualy.");
      char szComm[128];
      sprintf(szComm, "cli -s .audio.volume %d", s_iCurrentMajAudioVolume);
      hw_execute_bash_command_raw(szComm, NULL);
      hw_execute_bash_command_raw("killall -1 majestic", NULL);
   }
   pthread_attr_destroy(&attr);
}

void* _thread_majestic_set_audio_bitrate(void *argument)
{
   s_bMajThreadSetAudioBitrateRunning = true;
   log_line("[HwCamMajestic] Started thread to set audio bitrate to %d ...", s_iCurrentMajAudioBitrate);
   
   char szComm[128];
   sprintf(szComm, "cli -s .audio.srate %d", s_iCurrentMajAudioBitrate);
   hw_execute_bash_command_raw(szComm, NULL);
   hw_execute_bash_command_raw("killall -1 majestic", NULL);

   s_bMajThreadSetAudioBitrateRunning = false;
   log_line("[HwCamMajestic] Finished thread to set audio bitrate.");
   return NULL; 
}


void hardware_camera_maj_set_audio_quality(int iBitrate)
{
   if ( iBitrate == s_iCurrentMajAudioBitrate )
      return;

   int iNewBitrate = iBitrate;
   if ( iNewBitrate < 4000 )
      iNewBitrate = 4000;
   if ( iNewBitrate >= 32000 )
      iNewBitrate = 48000;
   
   if ( iNewBitrate == s_iCurrentMajAudioBitrate )
   {
      log_line("[HwCamMajestic] Received request to change bitrate, but it's unchanged: %d", iNewBitrate);
      return;
   }
   
   s_uMajesticLastChangeTime = s_uMajesticLastChangeAudioTime = get_current_timestamp_ms();

   if ( s_bMajThreadSetAudioBitrateRunning )
   {
      log_softerror_and_alarm("[HwCamMajestic] Another thread to set audio bitrate is running. Stop it.");
      pthread_cancel(s_ThreadMajSetAudioBitrate);
   }

   s_iCurrentMajAudioBitrate = iNewBitrate;
   
   s_bMajThreadSetAudioBitrateRunning = true;
   pthread_attr_t attr;
   _adjust_thread_priority_attrs(&attr);
   pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
   if ( 0 != pthread_create(&s_ThreadMajSetAudioBitrate, &attr, &_thread_majestic_set_audio_bitrate, NULL) )
   {
      s_bMajThreadSetAudioBitrateRunning = false;
      log_softerror_and_alarm("[HwCamMajestic] Can't create thread. Set audio bitrate manualy.");
      char szComm[128];
      sprintf(szComm, "cli -s .audio.srate %d", s_iCurrentMajAudioBitrate);
      hw_execute_bash_command_raw(szComm, NULL);
      hw_execute_bash_command_raw("killall -1 majestic", NULL);
   }
   pthread_attr_destroy(&attr);
}

u32  hardware_camera_maj_get_last_audio_change_time()
{
   return s_uMajesticLastChangeAudioTime;
}