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

#include "launchers_controller.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../base/launchers.h"
#include "../base/commands.h"
#include "shared_vars.h"
#include "popup.h"

char s_szMACControllerTXTelemetry[MAX_MAC_LENGTH];
char s_szMACControllerTXVideo[MAX_MAC_LENGTH];
char s_szMACControllerTXCommands[MAX_MAC_LENGTH];
char s_szMACControllerTXRC[MAX_MAC_LENGTH];
bool s_bControllerAudioPlayStarted = false;

void controller_launch_router()
{
   log_line("Starting controller router");
   if ( hw_process_exists("ruby_rt_station") )
   {
      log_line("Controller router process already running. Do nothing.");
      return;
   }
   ControllerSettings* pcs = get_ControllerSettings();
   char szComm[1024];
   char szPlayer[1024];

   if ( NULL == pcs )
      log_line("NNN");
   if ( g_bSearching )
      sprintf(szComm, "nice -n %d ./ruby_rt_station -search %d &", pcs->iNiceRouter, g_iSearchFrequency);
   else if ( pcs->ioNiceRouter > 0 )
      sprintf(szComm, "ionice -c 1 -n %d nice -n %d ./ruby_rt_station &", pcs->ioNiceRouter, pcs->iNiceRouter);
   else
      sprintf(szComm, "nice -n %d ./ruby_rt_station &", pcs->iNiceRouter);

   hw_execute_bash_command(szComm, NULL);

   hw_set_proc_priority( "ruby_rt_station", pcs->iNiceRouter, pcs->ioNiceRouter, 1);

   log_line("Done launching controller router.");
}

void controller_stop_router()
{
   hw_stop_process("ruby_rt_station");
}


void controller_launch_video_player()
{
   log_line("Starting video player");
   if ( hw_process_exists(VIDEO_PLAYER_PIPE) )
   {
      log_line("Video player process already running. Do nothing.");
      return;
   }

   ControllerSettings* pcs = get_ControllerSettings();
   char szPlayer[1024];

   if ( pcs->ioNiceRXVideo > 0 )
      sprintf(szPlayer, "ionice -c 1 -n %d nice -n %d ./%s > /dev/null 2>&1&", pcs->ioNiceRXVideo, pcs->iNiceRXVideo, VIDEO_PLAYER_PIPE);
   else
      sprintf(szPlayer, "nice -n %d ./%s > /dev/null 2>&1&", pcs->iNiceRXVideo, VIDEO_PLAYER_PIPE);

   hw_execute_bash_command(szPlayer, NULL);

   hw_set_proc_priority( VIDEO_PLAYER_PIPE, pcs->iNiceRXVideo, pcs->ioNiceRXVideo, 1);
}

void controller_stop_video_player()
{
   char szComm[1024];
   sprintf(szComm, "ps -ef | nice grep '%s' | nice grep -v grep | awk '{print $2}' | xargs kill -9 2>/dev/null", VIDEO_PLAYER_PIPE);
   hw_execute_bash_command(szComm, NULL);
}

void controller_launch_rx_telemetry()
{
   if ( hw_process_exists("ruby_rx_telemetry") )
   {
      log_line("ruby_rx_telemetry process already running. Do nothing.");
      return;
   }
   hw_execute_bash_command("./ruby_rx_telemetry &", NULL);
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

   char szComm[256];
   if ( g_bSearching )
      sprintf(szComm, "./ruby_tx_rc -search &");
   else if ( NULL != g_pCurrentModel )
      sprintf(szComm, "ionice -c 1 -n %d nice -n %d ./ruby_tx_rc &", DEFAULT_IO_PRIORITY_RC, g_pCurrentModel->niceRC);
   else
      sprintf(szComm, "./ruby_tx_rc &");
   hw_execute_bash_command(szComm, NULL);
}

void controller_stop_tx_rc()
{
   hw_stop_process("ruby_tx_rc");
}


static const char* s_szControllerCardErrorFrequency = "You can't set frequency %d Mhz, it's used by a different radio link. You would have two radio interfaces transmitting on the same frequency.";
static char s_szControllerCardError[1024];

const char* controller_validate_radio_settings(Model* pModel, int* pVehicleNICFreq, int* pVehicleNICFlags, int* pVehicleNICRadioFlags, int* pControllerNICFlags, int* pControllerNICRadioFlags)
{
   s_szControllerCardError[0] = 0;

   // Check frequencies

   if ( NULL != pModel && pModel->radioInterfacesParams.interfaces_count > 1 )
   if ( NULL != pVehicleNICFreq )
   {
      bool bDuplicate = false;
      for( int i=0; i<pModel->radioInterfacesParams.interfaces_count-1; i++ )
      for( int k=i+1; k<pModel->radioInterfacesParams.interfaces_count; k++ )
         if ( (NULL != pVehicleNICFlags) && (!(pVehicleNICFlags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED)) )
         if ( (NULL != pVehicleNICFlags) && (!(pVehicleNICFlags[k] & RADIO_HW_CAPABILITY_FLAG_DISABLED)) )
         if ( pVehicleNICFreq[i] == pVehicleNICFreq[k] )
         {
            bDuplicate = true;
            sprintf(s_szControllerCardError, s_szControllerCardErrorFrequency, pVehicleNICFreq[i]);
            return s_szControllerCardError;
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

   if ( NULL != pModel && NULL != pVehicleNICFlags )
   {
      bool bHasVideo = false;
      bool bHasData = false;
      for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( pVehicleNICFlags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            continue;
         if ( pVehicleNICFlags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_RELAY )
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

void controller_start_audio(Model* pModel)
{
   if ( g_bSearching )
   {
      log_line("Audio not started, is in search mode.");
      s_bControllerAudioPlayStarted = false;
      return;
   }
   if ( NULL == pModel || (!pModel->audio_params.enabled) )
   {
      log_line("Audio is disabled on current vehicle.");
      s_bControllerAudioPlayStarted = false;
      return;
   }
   if ( NULL == pModel || (! pModel->audio_params.has_audio_device) )
   {
      log_line("No audio capture device on current vehicle.");
      s_bControllerAudioPlayStarted = false;
      return;
   }

   if ( hw_process_exists("aplay") )
   {
      log_line("Audio aplay process already running. Do nothing.");
      return;
   }

   char szComm[256];
   char szOutput[4096];

   hw_execute_bash_command_raw("aplay -l 2>&1", szOutput );
   if ( 0 == szOutput[0] || NULL != strstr(szOutput, "no soundcards") || NULL != strstr(szOutput, "no sound") )
   {
      log_softerror_and_alarm("No output audio devices/soundcards on the controller. Audio output is disabled.");
      s_bControllerAudioPlayStarted = false;
      return;
   }
   sprintf(szComm, "aplay -c 1 --rate 44100 --format S16_LE %s 2>/dev/null &", FIFO_RUBY_AUDIO1);
   hw_execute_bash_command(szComm, NULL);
   s_bControllerAudioPlayStarted = true;
}

void controller_stop_audio()
{
   if ( s_bControllerAudioPlayStarted )
      hw_stop_process("aplay");
   s_bControllerAudioPlayStarted = false;
}


void controller_start_i2c()
{
   char szComm[256];
   sprintf(szComm, "nice -n %d ./ruby_i2c &", DEFAULT_PRIORITY_PROCESS_RC);
   hw_execute_bash_command(szComm, NULL);
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
      hardware_sleep_ms(70);
      szPids[0] = 0;
      hw_execute_bash_command(szComm, szPids);
      if ( 0 == szPids[0] || strlen(szPids) < 2 )
      {
         log_line("Process %s has finished and exited.", szProcName);
         return true;
      }
      retryCount--;
   }
   log_softerror_and_alarm("Process %s failed to exit after 3 seconds.", szProcName);
   return false;
}

void controller_wait_for_stop_all()
{
   log_line("Waiting for all pairing processes to stop...");

   if ( ! _controller_wait_for_stop_process("aplay") )
      log_softerror_and_alarm("Failed to wait for stopping: aplay");

   if ( ! _controller_wait_for_stop_process("ruby_rx_telemetry") )
      log_softerror_and_alarm("Failed to wait for stopping: ruby_rx_telemetry");

   if ( ! _controller_wait_for_stop_process("ruby_tx_rc") )
      log_softerror_and_alarm("Failed to wait for stopping: ruby_tx_rc");

   if ( ! _controller_wait_for_stop_process("ruby_rt_router") )
      log_softerror_and_alarm("Failed to wait for stopping: ruby_rt_router");

   log_line("All pairing processes have finished and exited.");
   hardware_sleep_ms(200);
}


void controller_check_update_processes_affinities()
{
   char szOutput[128];

   hw_execute_bash_command_raw("nproc --all", szOutput);
   int iCores = 0;
   if ( 1 != sscanf(szOutput, "%d", &iCores) )
   {
      log_softerror_and_alarm("Failed to get CPU cores count. No affinity adjustments for processes to be done.");
      return;    
   }

   if ( iCores < 2 || iCores > 32 )
   {
      log_line("Single core CPU, no affinity adjustments for processes to be done.");
      return;
   }
   log_line("%d CPU cores, doing affinity adjustments for processes...", iCores);

   if ( iCores > 2 )
   {
      hw_set_proc_affinity("ruby_rt_station", 1,1);
      hw_set_proc_affinity("ruby_central", 2,2);
      hw_set_proc_affinity("ruby_rx_telemetry", 3, 3);
      hw_set_proc_affinity("ruby_tx_rc", 3, 3);
      hw_set_proc_affinity("ruby_player_p", 3, iCores);

   }
   else
   {
      hw_set_proc_affinity("ruby_rt_station", 1,1);
      hw_set_proc_affinity("ruby_central", 2,2); 
   }
}
