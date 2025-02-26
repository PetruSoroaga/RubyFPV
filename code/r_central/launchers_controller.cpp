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

#include "launchers_controller.h"
#include <pthread.h>
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
//#include "../base/radio_utils.h"
#include "../base/commands.h"
#include "../common/string_utils.h"

#include "shared_vars.h"
#include "popup.h"

char s_szMACControllerTXTelemetry[MAX_MAC_LENGTH];
char s_szMACControllerTXVideo[MAX_MAC_LENGTH];
char s_szMACControllerTXCommands[MAX_MAC_LENGTH];
char s_szMACControllerTXRC[MAX_MAC_LENGTH];

static int s_iCPUCoresCount = 1;

void controller_compute_cpu_info()
{
   char szOutput[128];
   hw_execute_bash_command_raw("nproc --all", szOutput);
   s_iCPUCoresCount = 1;
   if ( 1 != sscanf(szOutput, "%d", &s_iCPUCoresCount) )
   {
      log_softerror_and_alarm("Failed to get CPU cores count. Default to 1 core.");
      return;    
   }
   log_line("Detected CPU with %d cores.", s_iCPUCoresCount);
}

void controller_launch_router(bool bSearchMode, int iFirmwareType)
{
   log_line("Starting controller router (%s)", bSearchMode?"in search mode":"in normal mode");
   if ( hw_process_exists("ruby_rt_station") )
   {
      log_line("Controller router process already running. Do nothing.");
      return;
   }
   ControllerSettings* pcs = get_ControllerSettings();

   char szParams[256];
   char szPrefix[256];
   szPrefix[0] = 0;
   szParams[0] = 0;
   if ( bSearchMode )
   {
      if ( (g_iSearchSiKAirDataRate >= 0) && (iFirmwareType == MODEL_FIRMWARE_TYPE_RUBY) )
      {
         sprintf(szPrefix, "nice -n %d", pcs->iNiceRouter);
         sprintf(szParams, "-search %d -sik %d %d %d %d",
             g_iSearchFrequency, g_iSearchSiKAirDataRate, g_iSearchSiKECC, g_iSearchSiKLBT, g_iSearchSiKMCSTR);
      }
      else
      {
         sprintf(szPrefix, "nice -n %d", pcs->iNiceRouter);
         sprintf(szParams, "-search %d -firmware %d", g_iSearchFrequency, iFirmwareType);
      }
   }
   else
   {
      #ifdef HW_CAPABILITY_IONICE
      if ( pcs->ioNiceRouter > 0 )
         sprintf(szPrefix, "ionice -c 1 -n %d nice -n %d", pcs->ioNiceRouter, pcs->iNiceRouter);
      else
      #endif
         sprintf(szPrefix, "nice -n %d", pcs->iNiceRouter);
   }

   if ( ! pcs->iPrioritiesAdjustment )
      szPrefix[0] = 0;

   hw_execute_ruby_process(szPrefix, "ruby_rt_station", szParams, NULL);

   if ( pcs->iPrioritiesAdjustment )
      hw_set_proc_priority( "ruby_rt_station", pcs->iNiceRouter, pcs->ioNiceRouter, 1);

   log_line("Done launching controller router.");
}

void controller_stop_router()
{
   hw_stop_process("ruby_rt_station");
}

void controller_launch_rx_telemetry()
{
   if ( hw_process_exists("ruby_rx_telemetry") )
   {
      log_line("ruby_rx_telemetry process already running. Do nothing.");
      return;
   }
   hw_execute_ruby_process(NULL, "ruby_rx_telemetry", NULL, NULL);
}

void controller_stop_rx_telemetry()
{
   hw_stop_process("ruby_rx_telemetry");
}

void controller_launch_tx_rc()
{
   if ( hw_process_exists("ruby_tx_rc") )
   {
      log_line("ruby_tx_rc process already running. Do nothing.");
      return;
   }

   char szPrefix[128];
   char szParams[128];
   szPrefix[0] = 0;
   szParams[0] = 0;

   #ifdef HW_CAPABILITY_IONICE
   if ( g_bSearching )
      sprintf(szParams, "-search");
   else if ( NULL != g_pCurrentModel )
      sprintf(szPrefix, "ionice -c 1 -n %d nice -n %d", DEFAULT_IO_PRIORITY_RC, g_pCurrentModel->processesPriorities.iNiceRC);
   #else
   if ( g_bSearching )
      sprintf(szParams, "-search &");
   else if ( NULL != g_pCurrentModel )
      sprintf(szPrefix, "nice -n %d", g_pCurrentModel->processesPriorities.iNiceRC);
   #endif

   hw_execute_ruby_process(szPrefix, "ruby_tx_rc", szParams, NULL);
}

void controller_stop_tx_rc()
{
   hw_stop_process("ruby_tx_rc");
}


static const char* s_szControllerCardErrorFrequency = "You can't set frequency %s, it's used by a different radio link. You would have two radio interfaces transmitting on the same frequency.";
static char s_szControllerCardError[1024];

const char* controller_validate_radio_settings(Model* pModel, u32* pVehicleNICFreq, int* pVehicleNICFlags, int* pVehicleNICRadioFlags, int* pControllerNICFlags, int* pControllerNICRadioFlags)
{
   s_szControllerCardError[0] = 0;

   // Check frequencies

   if ( NULL != pModel && pModel->radioInterfacesParams.interfaces_count > 1 )
   if ( NULL != pVehicleNICFreq )
   {
      for( int i=0; i<pModel->radioInterfacesParams.interfaces_count-1; i++ )
      {
         for( int k=i+1; k<pModel->radioInterfacesParams.interfaces_count; k++ )
         {
            if ( NULL == pVehicleNICFlags )
               continue;
            if ( pVehicleNICFlags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
               continue;
            if ( pVehicleNICFlags[k] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
               continue;

            u32 uFreq1 = pVehicleNICFreq[i];
            u32 uFreq2 = pVehicleNICFreq[k];

            if ( uFreq1 == uFreq2 )
            {
               sprintf(s_szControllerCardError, s_szControllerCardErrorFrequency, str_format_frequency(uFreq1));
               return s_szControllerCardError;
            }
            
            if ( pModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
            if ( pModel->relay_params.uRelayedVehicleId != 0 )
            if ( pModel->relay_params.uRelayedVehicleId != MAX_U32 )
            if ( pModel->relay_params.uRelayFrequencyKhz != 0 )
            if ( uFreq1 == pModel->relay_params.uRelayFrequencyKhz )
            {
               sprintf(s_szControllerCardError, "You can't set frequency %s, it's used for relaying.", str_format_frequency(uFreq1));
               return s_szControllerCardError;
            }        
         }
      }
   }
   

   // Check [all cards disabled] on vehicle

   if ( NULL != pModel && NULL != pVehicleNICFlags )
   {
      bool bAllDisabled = true;
      for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
         if ( !(pVehicleNICFlags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
            bAllDisabled = false;
      if ( bAllDisabled )
      {
         strcpy( s_szControllerCardError, "You can't disable all radio links on the vehicle.");
         return s_szControllerCardError;
      }
   }

   // Check missing data capable cards on vehicle

   if ( (NULL != pModel) && (NULL != pVehicleNICFlags) )
   {
      bool bHasData = false;
      for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( pVehicleNICFlags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( (pModel->sw_version>>16) >= 79 )
         if ( pVehicleNICFlags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
            continue;
         //if ( pVehicleNICFlags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         //   bHasVideo = true;
         if ( pVehicleNICFlags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
            bHasData = true;
      }
      if ( ! bHasData )
      {
         strcpy( s_szControllerCardError, "You can't remove all data links from the vehicle.");
         return s_szControllerCardError;
      }
   }

   return NULL;
}


void controller_start_i2c()
{
   char szPrefix[128];
   sprintf(szPrefix, "nice -n %d", DEFAULT_PRIORITY_PROCESS_RC);
   hw_execute_ruby_process(szPrefix, "ruby_i2c", NULL, NULL);
}

void controller_stop_i2c()
{
   hw_stop_process("ruby_i2c");
}

bool _controller_wait_for_stop_process(const char* szProcName)
{
   char szComm[1024];
   char szPids[1024];

   if ( NULL == szProcName || 0 == szProcName[0] )
      return false;

   sprintf(szComm, "pidof %s", szProcName);

   int retryCount = 40;
   while ( retryCount > 0 )
   {
      hardware_sleep_ms(10);
      szPids[0] = 0;
      hw_execute_bash_command(szComm, szPids);
      if ( 0 == szPids[0] || strlen(szPids) < 2 )
      {
         log_line("Process %s has finished and exited.", szProcName);
         return true;
      }
      hardware_sleep_ms(10);
      retryCount--;
   }
   log_softerror_and_alarm("Process %s failed to exit after 3 seconds.", szProcName);
   return false;
}

void controller_wait_for_stop_all()
{
   log_line("Waiting for all pairing processes to stop...");

   if ( ! _controller_wait_for_stop_process("ruby_rx_telemetry") )
      log_softerror_and_alarm("Failed to wait for stopping: ruby_rx_telemetry");

   if ( ! _controller_wait_for_stop_process("ruby_tx_rc") )
      log_softerror_and_alarm("Failed to wait for stopping: ruby_tx_rc");

   if ( ! _controller_wait_for_stop_process("ruby_rt_router") )
      log_softerror_and_alarm("Failed to wait for stopping: ruby_rt_router");

   log_line("All pairing processes have finished and exited.");
}

static void * _thread_adjust_affinities(void *argument)
{
   sched_yield();
   log_line("[BGThread] Started background thread to adjust processes affinities...");
   int iSelfPID = getpid();
   int iSelfId = 0;
   #if defined(HW_PLATFORM_RADXA_ZERO3)
   iSelfId = gettid();
   #endif
   log_line("[BGThread] Background thread id: %d, PID: %d", iSelfId, iSelfPID);
   if ( s_iCPUCoresCount > 2 )
   {
      hw_set_proc_affinity("ruby_rt_station", iSelfId, 1,1);
      hw_set_proc_affinity("ruby_central", iSelfId, 2,2);
      hw_set_proc_affinity("ruby_rx_telemetry", iSelfId, 3, 3);
      hw_set_proc_affinity("ruby_tx_rc", iSelfId, 3, 3);
      #if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA_ZERO3)
      char szFile[MAX_FILE_PATH_SIZE];
      ControllerSettings* pCS = get_ControllerSettings();
      if ( 0 == pCS->iStreamerOutputMode )
         strcpy(szFile, VIDEO_PLAYER_SM);
      else
         strcpy(szFile, VIDEO_PLAYER_PIPE);
      hw_set_proc_affinity(szFile, iSelfId, 3, s_iCPUCoresCount);
      #endif
   }
   else
   {
      hw_set_proc_affinity("ruby_rt_station", iSelfId, 1,1);
      hw_set_proc_affinity("ruby_central", iSelfId, 2,2); 
   }
   log_line("[BGThread] Background thread to adjust processes affinities completed.");
   return NULL;
}

void controller_check_update_processes_affinities()
{
   if ( (s_iCPUCoresCount < 2) || (s_iCPUCoresCount > 32) )
   {
      log_line("Single core CPU (%d), no affinity adjustments for processes to be done.", s_iCPUCoresCount);
      return;
   }
   ControllerSettings* pCS = get_ControllerSettings();
   if ( (NULL == pCS) || (0 == pCS->iCoresAdjustment) )
   {
      log_line("%d CPU cores, affinity adjustments is disabled. Do nothing.", s_iCPUCoresCount);
      return;
   }
   
   log_line("%d CPU cores, doing affinity adjustments for processes...", s_iCPUCoresCount);

   pthread_t pThreadBg;
   pthread_attr_t attr;
   struct sched_param params;

   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
   pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
   params.sched_priority = 0;
   pthread_attr_setschedparam(&attr, &params);

   if ( 0 != pthread_create(&pThreadBg, &attr, &_thread_adjust_affinities, NULL) )
   {
      log_error_and_alarm("Failed to create thread for adjusting processes affinities.");
      pthread_attr_destroy(&attr);
      return;
   }
   pthread_attr_destroy(&attr);
   log_line("Done launching worker thread to adjust affinities.");
}
