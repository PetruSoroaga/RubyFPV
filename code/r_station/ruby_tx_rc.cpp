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
#include <sys/resource.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pcap.h>
#include <stdint.h>
#include "../radio/radiolink.h"
#include <inttypes.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/shared_mem_i2c.h"
#include "../base/models.h"
#include "../base/launchers.h"
#include "../base/hw_procs.h"
#include "../base/utils.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"

#include "timers.h"

#include <sys/resource.h>
#include <semaphore.h>


#define MAX_SERIAL_BUFFER_SIZE 512

bool g_bDebug = false;
bool g_bSearching = false;
bool g_bQuit = false;
bool g_bUpdateInProgress = false;

u32 g_iFPSFramesCount = 0;
int g_iFPSMaxJoystickEvents = 0;
int g_iFPSTotalJoystickEvents = 0;
int g_iJoystickCheckFailureCount = 0;

int s_fIPCFromRouter = -1;
int s_fIPCToRouter = -1;

u8 s_BufferRCDownlink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeBufferFromRouter[MAX_PACKET_TOTAL_SIZE];
int s_PipeBufferFromRouterPos = 0;


Model* g_pCurrentModel = NULL;
shared_mem_process_stats* s_pProcessStats = NULL;
t_shared_mem_i2c_controller_rc_in* s_pSM_RCIn = NULL;

t_packet_header gPH;
t_packet_header_rc_full_frame_upstream g_PHRCFUpstream;
t_packet_header_rc_full_frame_upstream* s_pPHRCFUpstream = NULL;
hw_joystick_info_t s_JoystickLocalInfo;
hw_joystick_info_t* s_pJoystick = NULL;
t_ControllerInputInterface* s_pCII = NULL;
u16 s_ComputedRCValues[MAX_RC_CHANNELS];

u32 s_uLastTimeStampRCInFrame = 0;
u8 s_uLastFrameIndexRCIn = 0;

void init_controller_settings();

void populate_rc_data( t_packet_header_rc_full_frame_upstream* pPHRCF )
{
   pPHRCF->rc_frame_index++;
   for( int i=0; i<(int)(g_pCurrentModel->rc_params.channelsCount); i++ )
   {
      packet_header_rc_full_set_rc_channel_value(pPHRCF, i, s_ComputedRCValues[i]);
   }
   //log_line("RC5: %d", s_ComputedRCValues[5]);
}


bool handle_joysticks()
{
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   if ( NULL == s_pCII || NULL == s_pJoystick || (!hardware_is_joystick_opened(s_pCII->currentHardwareIndex)) )
   {
      if ( g_TimeNow < g_TimeLastJoystickCheck + 1000 + g_iJoystickCheckFailureCount*100 )
         return false;

      g_iJoystickCheckFailureCount++;
      g_TimeLastJoystickCheck = g_TimeNow;
      controllerInterfacesEnumJoysticks();

      if ( 0 == pCI->inputInterfacesCount )
         controllerInterfacesEnumJoysticks();
      if ( 0 == pCI->inputInterfacesCount )
         return false;

      for( int i=0; i<pCI->inputInterfacesCount; i++ )
      {
         t_ControllerInputInterface* pCII = controllerInterfacesGetAt(i);
         if ( NULL == pCII )
            continue;
         if ( NULL != g_pCurrentModel && (pCII->uId == g_pCurrentModel->rc_params.hid_id) )
         {
            s_pCII = controllerInterfacesGetAt(i);
            break;
         }
      }
      if ( NULL != s_pCII )
         s_pJoystick = hardware_get_joystick_info(s_pCII->currentHardwareIndex);
      if ( NULL != s_pJoystick )
      {
         if ( 0 == hardware_open_joystick(s_pCII->currentHardwareIndex) )
            s_pJoystick = NULL;
      }
      return false;  
   }

   if ( NULL == s_pJoystick || NULL == s_pCII )
      return false;
   
   int countEvents = hardware_read_joystick(s_pCII->currentHardwareIndex, 5);
   if ( countEvents < 0 )
   {
      log_line("Hardware: failed to read joystick.");
      hardware_close_joystick(s_pCII->currentHardwareIndex);
      return false;
   }

   g_iJoystickCheckFailureCount = 0;
   g_iFPSTotalJoystickEvents += countEvents;
   if ( countEvents > g_iFPSMaxJoystickEvents )
      g_iFPSMaxJoystickEvents = countEvents;

   memcpy(&s_JoystickLocalInfo, s_pJoystick, sizeof(hw_joystick_info_t));
   return true;
}


void try_read_pipes()
{
   int maxMsgToRead = 5;

   while ( (maxMsgToRead > 0) && NULL != ruby_ipc_try_read_message(s_fIPCFromRouter, 1000, s_PipeBufferFromRouter, &s_PipeBufferFromRouterPos, s_BufferRCDownlink) )
   {
      maxMsgToRead--;
      t_packet_header* pPH = (t_packet_header*)&s_BufferRCDownlink[0];
      if ( ! packet_check_crc(s_BufferRCDownlink, pPH->total_length) )
      {
         continue;
      }

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
      {
            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
            if ( pPH->vehicle_id_src != PACKET_COMPONENT_RC )
            {
               log_line("Received message to reload model.");
               if ( NULL == g_pCurrentModel )
                  g_pCurrentModel = new Model();
               if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true ) )
               {
                  log_line("Failed to load model. RC becomes inactive");
                  g_pCurrentModel = NULL;
               }
               if ( NULL != g_pCurrentModel )
               {
                  int miliSecInterval = 1000/g_pCurrentModel->rc_params.rc_frames_per_second;
                  log_line("Using a RC rate of %d packets/sec, %d ms between packets", g_pCurrentModel->rc_params.rc_frames_per_second, miliSecInterval);
                  log_line("RC is enabled: %s", g_pCurrentModel->rc_params.rc_enabled?"yes":"no");
               }
               load_ControllerInterfacesSettings();
            }
            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED )
            if ( pPH->vehicle_id_src != PACKET_COMPONENT_RC )
            {
               init_controller_settings();
            }
      }

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED )
         g_bUpdateInProgress = true;

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED ||
           pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED )
         g_bUpdateInProgress = false;

   }
}

void init_controller_settings()
{
   load_ControllerInterfacesSettings();
   load_ControllerSettings();
   //populate_ControllerSerialPorts();
}

void _update_loop_info(u32 tTime0)
{
   u32 tNow = get_current_timestamp_ms();
   if ( NULL != s_pProcessStats )
   {
      if ( s_pProcessStats->uMaxLoopTimeMs < tNow - tTime0 )
         s_pProcessStats->uMaxLoopTimeMs = tNow - tTime0;
      s_pProcessStats->uTotalLoopTime += tNow - tTime0;
      if ( 0 != s_pProcessStats->uLoopCounter )
         s_pProcessStats->uAverageLoopTimeMs = s_pProcessStats->uTotalLoopTime / s_pProcessStats->uLoopCounter;
   }
}

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
} 

int main (int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);
 
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }
   
   log_init("TX RC");
   //log_add_file("logs/log_tx_rc.log"); 
   if ( strcmp(argv[argc-1], "-search") == 0 )
      g_bSearching = true;

   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebug = true;
   if ( g_bDebug )
      log_enable_stdout();
  
   g_pCurrentModel = new Model();
   if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL) )
   {
      log_error_and_alarm("Can't load current model vehicle.");
      g_pCurrentModel = NULL;
   }

   if ( NULL != g_pCurrentModel )
      hw_set_priority_current_proc(g_pCurrentModel->niceRC); 

   Preferences* p = get_Preferences();   
   if ( p->nLogLevel != 0 )
      log_only_errors();

   s_pPHRCFUpstream = shared_mem_rc_upstream_frame_open_write();

   s_fIPCToRouter = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_RC_TO_ROUTER);
   if ( s_fIPCToRouter < 0 )
      return -1;

   s_fIPCFromRouter = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_RC);
   if ( s_fIPCFromRouter < 0 )
      return -1;

   s_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_RC_TX);
   if ( NULL == s_pProcessStats )
      log_softerror_and_alarm("Failed to open shared mem for RC tx process watchdog stats for writing: %s", SHARED_MEM_WATCHDOG_TELEMETRY_RX);
   else
      log_line("Opened shared mem for RC tx process watchdog stats for writing.");
 
   long miliSecInterval = 100;
   if ( NULL != g_pCurrentModel )
   {
      miliSecInterval = 1000/g_pCurrentModel->rc_params.rc_frames_per_second;
      log_line("Using a RC rate of %d packets/sec, %d ms between packets", g_pCurrentModel->rc_params.rc_frames_per_second, miliSecInterval);
      log_line("RC is enabled: %s", g_pCurrentModel->rc_params.rc_enabled?"yes":"no");
   }
   else
      log_line("No model. RC is inactive.");

   init_controller_settings();
   load_ControllerInterfacesSettings();

   if ( ! g_bSearching )
      controllerInterfacesEnumJoysticks();

   log_line("Started all ok. Running now.");
   log_line("--------------------------------");

   gPH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   if ( NULL != g_pCurrentModel )
   {
      gPH.vehicle_id_src = g_pCurrentModel->vehicle_id;
      gPH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   }
   else
   {
      gPH.vehicle_id_src = 0;
      gPH.vehicle_id_dest = 0;
   }
   g_PHRCFUpstream.rc_frame_index = 0;
   g_PHRCFUpstream.flags = 0;
   for( int i=0; i<MAX_RC_CHANNELS; i++ )
   {
      s_ComputedRCValues[i] = 0;
      packet_header_rc_full_set_rc_channel_value(&g_PHRCFUpstream, i, 1000);
   }

   if ( NULL != s_pPHRCFUpstream )
      memcpy(s_pPHRCFUpstream, &g_PHRCFUpstream, sizeof(t_packet_header_rc_full_frame_upstream) );

   g_TimeStart = get_current_timestamp_ms(); 

   while ( !g_bQuit )
   { 
      g_iFPSFramesCount++;
      hardware_sleep_ms(10);

      if ( NULL != s_pProcessStats )
      {
         s_pProcessStats->uLoopCounter++;
         s_pProcessStats->lastActiveTime = g_TimeNow;
      }

      g_TimeNow = get_current_timestamp_ms();
      u32 tTime0 = g_TimeNow;

      if ( g_TimeNow > g_TimeLastFPSCalculation + 1000 )
      {
         //log_line("FPS: %d; Average joystick events: %d, max joystick events: %d", g_iFPSFramesCount, g_iFPSTotalJoystickEvents/g_iFPSFramesCount, g_iFPSMaxJoystickEvents );
         g_TimeLastFPSCalculation = g_TimeNow;
         g_iFPSFramesCount = 0;
         g_iFPSMaxJoystickEvents = 0;
         g_iFPSTotalJoystickEvents = 0;
      }

      try_read_pipes();

      if ( g_bSearching || g_bUpdateInProgress )
      {
         _update_loop_info(tTime0);
         continue;
      }
      if ( NULL == g_pCurrentModel )
      {
         _update_loop_info(tTime0);
         continue;
      }
      if ( (! g_pCurrentModel->rc_params.rc_enabled) || g_pCurrentModel->is_spectator )
      {
         _update_loop_info(tTime0);
         continue;
      }
   
      #ifdef FEATURE_ENABLE_RC

      if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_RC_IN_SBUS_IBUS )
      {
         g_PHRCFUpstream.flags &= (~RC_FULL_FRAME_FLAGS_HAS_INPUT);

         if ( NULL == s_pSM_RCIn )
            s_pSM_RCIn = shared_mem_i2c_controller_rc_in_open_for_read();
         if ( NULL != s_pSM_RCIn )
         if ( s_pSM_RCIn->uFlags & RC_IN_FLAG_HAS_INPUT )
         {
            g_PHRCFUpstream.flags |= RC_FULL_FRAME_FLAGS_HAS_INPUT;
            if ( s_uLastTimeStampRCInFrame != s_pSM_RCIn->uTimeStamp || s_uLastFrameIndexRCIn != s_pSM_RCIn->uFrameIndex )
            {
               s_uLastTimeStampRCInFrame = s_pSM_RCIn->uTimeStamp;
               s_uLastFrameIndexRCIn = s_pSM_RCIn->uFrameIndex;
               int nCh = g_pCurrentModel->rc_params.channelsCount;
               if ( nCh > (int)(s_pSM_RCIn->uChannelsCount) )
                  nCh = (int)(s_pSM_RCIn->uChannelsCount);
               for( int i=0; i<nCh; i++ )
                  s_ComputedRCValues[i] = s_pSM_RCIn->uChannels[i];

               //log_line("%d %d %d", s_pSM_RCIn->uChannels[0], s_pSM_RCIn->uChannels[1], s_pSM_RCIn->uChannels[2] );
            }
         }
      }

      if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_USB )
      {
         if ( handle_joysticks() )
            g_PHRCFUpstream.flags |= RC_FULL_FRAME_FLAGS_HAS_INPUT;
         else
            g_PHRCFUpstream.flags &= (~RC_FULL_FRAME_FLAGS_HAS_INPUT);
      }

      if ( g_TimeNow < g_TimeLastRCFrameSent + miliSecInterval )
      {
         _update_loop_info(tTime0);
         continue;
      }

      u32 miliSec = g_TimeNow - g_TimeLastRCFrameSent;
      g_TimeLastRCFrameSent = g_TimeNow;

      if ( g_pCurrentModel->rc_params.inputType == RC_INPUT_TYPE_USB )
      {
         for( int i=0; i<(int)(g_pCurrentModel->rc_params.channelsCount); i++ )
            s_ComputedRCValues[i] = (u16) compute_controller_rc_value(g_pCurrentModel, i, (float)(s_ComputedRCValues[i]), NULL, &s_JoystickLocalInfo, s_pCII, miliSec);
      }

      populate_rc_data(&g_PHRCFUpstream);

      if ( NULL != s_pPHRCFUpstream )
         memcpy(s_pPHRCFUpstream, &g_PHRCFUpstream, sizeof(t_packet_header_rc_full_frame_upstream) );

      gPH.extra_flags = 0;
      gPH.packet_flags = PACKET_COMPONENT_RC;
      gPH.packet_type =  PACKET_TYPE_RC_FULL_FRAME;
      gPH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      gPH.vehicle_id_src = g_pCurrentModel->vehicle_id;
      gPH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
      gPH.total_headers_length = sizeof(t_packet_header)+sizeof(t_packet_header_rc_full_frame_upstream);
      gPH.total_length = gPH.total_headers_length;
      
      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, &gPH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), (u8*)&g_PHRCFUpstream, sizeof(t_packet_header_rc_full_frame_upstream));
      packet_compute_crc(buffer, gPH.total_length);
      ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, gPH.total_length);
      //log_line("sending rc frame index: %d", g_PHRCFUpstream.rc_frame_index);

      #endif

      _update_loop_info(tTime0);
   }

   if ( NULL != s_pCII )
      hardware_close_joystick(s_pCII->currentHardwareIndex);

   ruby_close_ipc_channel(s_fIPCFromRouter);
   ruby_close_ipc_channel(s_fIPCToRouter);
   s_fIPCFromRouter = -1;
   s_fIPCToRouter = -1;
  
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_RC_TX, s_pProcessStats);
   shared_mem_i2c_controller_rc_in_close(s_pSM_RCIn);
   shared_mem_rc_upstream_frame_close(s_pPHRCFUpstream);
   return 0;
}
