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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/ctrl_interfaces.h"
#include "../base/radio_utils.h"
#include "../base/hardware.h"
#include "../base/commands.h"
#include "../base/ruby_ipc.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include <semaphore.h>

#include "pairing.h"
#include "shared_vars.h"
#include "link_watch.h"
#include "warnings.h"
#include "popup.h"
#include "colors.h"
#include "osd_common.h"
#include "ruby_central.h"
#include "local_stats.h"
#include "launchers_controller.h"
#include "handle_commands.h"
#include "forward_watch.h"
#include "timers.h"
#include "events.h"
#include "process_router_messages.h"
#include "notifications.h"

extern bool s_bDebugOSDShowAll;

bool s_isRXStarted = false;
bool s_isVideoReceiving = false;

u32 s_uTimeToSetAffinities = 0;

void _pairing_open_shared_mem();
void _pairing_close_shared_mem();

bool _pairing_start()
{
   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_softerror_and_alarm("No radio interfaces detect on the system. Do nothing (no pairing).");
      log_line("------------------------------------------");
      return false;
   }

   controller_launch_router(g_bSearching);

   controller_launch_rx_telemetry();
   controller_launch_tx_rc();
   handle_commands_start_on_pairing();
   start_pipes_to_router();

   if ( NULL != g_pCurrentModel )
   {
      char szFile[128];
      snprintf(szFile, 127, LOG_FILE_VEHICLE, g_pCurrentModel->getShortName());      
      FILE* fd = fopen(szFile, "wb");
      if ( NULL != fd )
         fclose(fd);
   }
 
   s_isRXStarted = true;
   s_isVideoReceiving = false;
   
   link_reset_reconfiguring_radiolink();

   if ( NULL != g_pCurrentModel && (! g_pCurrentModel->is_spectator) )
      g_pCurrentModel->b_mustSyncFromVehicle = true;

   if ( NULL != g_pCurrentModel && g_pCurrentModel->rc_params.rc_enabled )
   {
       Popup* p = new Popup(true, "RC link is enabled", 3 );
       p->setIconId(g_idIconJoystick, get_Color_IconWarning());
       popups_add_topmost(p);
   }

   forward_streams_on_pairing_start();

   g_bMenuPopupUpdateVehicleShown = false;

   s_uTimeToSetAffinities = g_TimeNow + 8000;
   
   log_line("-----------------------------------------");
   log_line("Started pairing processes successfully.");
   log_line("-----------------------------------------");
   return true;
}

bool pairing_start_normal()
{
   log_line("-----------------------------------------------");
   log_line("Starting pairing processes for current model...");

   g_bSearching = false;
   g_iSearchFrequency = 0;
   g_iSearchSiKAirDataRate = -1;
   g_iSearchSiKECC = -1;
   g_iSearchSiKLBT = -1;
   g_iSearchSiKMCSTR = -1;

   onEventBeforePairing();

   if ( NULL == g_pCurrentModel )
   {
      log_line("No current model. Do nothing (no pairing).");
      log_line("----------------------------------------------");
      return true;
   }

   bool bRes = _pairing_start();

   if ( bRes )
      onEventPaired();

   return bRes;
}

bool pairing_start_search_mode(int iFreqKhz)
{
   log_line("-----------------------------------------------");
   log_line("Starting pairing processes in search mode (%s)...", str_format_frequency(iFreqKhz));

   g_bSearching = true;
   g_iSearchFrequency = iFreqKhz;
   g_iSearchSiKAirDataRate = -1;
   g_iSearchSiKECC = -1;
   g_iSearchSiKLBT = -1;
   g_iSearchSiKMCSTR = -1;

   return _pairing_start();
}

bool pairing_start_search_sik_mode(int iFreqKhz, int iAirDataRate, int iECC, int iLBT, int iMCSTR)
{
   log_line("----------------------------------------------------");
   log_line("Starting pairing processes in SiK search mode (%s)...", str_format_frequency(iFreqKhz));

   g_bSearching = true;
   g_iSearchFrequency = iFreqKhz;
   g_iSearchSiKAirDataRate = iAirDataRate;
   g_iSearchSiKECC = iECC;
   g_iSearchSiKLBT = iLBT;
   g_iSearchSiKMCSTR = iMCSTR;

   return _pairing_start();
}

bool pairing_stop()
{
   if ( ! s_isRXStarted )
      return true;
   log_line("----------------------------------");
   log_line("Stopping pairing processes...");

   onEventBeforePairingStop();

   forward_streams_on_pairing_stop();
   hardware_recording_led_set_off();

   s_uTimeToSetAffinities = 0;

   _pairing_close_shared_mem();

   ruby_stop_recording();

   handle_commands_stop_on_pairing();

   controller_stop_tx_rc();

   controller_stop_router();

   controller_stop_rx_telemetry();

   controller_wait_for_stop_all();

   onEventPairingStopped();

   stop_pipes_to_router();
   handle_commands_stop_on_pairing();

   s_isRXStarted = false;
   s_isVideoReceiving = false;

   ruby_clear_all_ipc_channels();

   g_bIsRouterReady = false;
   g_RouterIsReadyTimestamp = 0;

   log_line("------------------------------------------");
   log_line("Stopped pairing processes successfully.");
   log_line("------------------------------------------");
   return true;
}

void pairing_on_router_ready()
{
   log_line("Pairing: received event that router is ready. Radio interfaces configuration (refreshed) when router is ready:");
   hardware_load_radio_info();
   log_line("Pairing: received event that router is ready. Open shared mem objects...");
   _pairing_open_shared_mem();
   log_line("Pairing: received event that router is ready. Open shared mem objects complete.");
}

void _pairing_open_shared_mem()
{
   hardware_sleep_ms(50);

   int iAnyNewOpen = 0;
   int iAnyFailed = 0;

   for( int i=0; i<10; i++ )
   {
      if ( NULL != g_pSM_RCIn )
         break;
      g_pSM_RCIn = shared_mem_i2c_controller_rc_in_open_for_read();
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_RCIn )
      iAnyFailed++;

   ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_RadioStats )
         break;
      g_pSM_RadioStats = shared_mem_radio_stats_open_for_read();
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_RadioStats )
      iAnyFailed++;

    ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_RadioStatsInterfaceRxGraph )
         break;
      g_pSM_RadioStatsInterfaceRxGraph = shared_mem_controller_radio_stats_interfaces_rx_graphs_open_for_read();
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_RadioStatsInterfaceRxGraph )
      iAnyFailed++;

   ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_VideoLinkStats )
         break;
      g_pSM_VideoLinkStats = shared_mem_video_link_stats_open_for_read();
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_VideoLinkStats )
      iAnyFailed++;

   ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_VideoInfoStats )
         break;
      g_pSM_VideoInfoStats = shared_mem_video_info_stats_open_for_read();
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_VideoInfoStats )
      iAnyFailed++;

   ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_VideoInfoStatsRadioIn )
         break;
      g_pSM_VideoInfoStatsRadioIn = shared_mem_video_info_stats_radio_in_open_for_read();
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_VideoInfoStatsRadioIn )
      iAnyFailed++;

   ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_VideoLinkGraphs )
         break;
      g_pSM_VideoLinkGraphs = shared_mem_video_link_graphs_open_for_read();
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_VideoLinkGraphs )
      iAnyFailed++;

   ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_VideoDecodeStats )
         break;
      g_pSM_VideoDecodeStats = shared_mem_video_stream_stats_rx_processors_open(true);
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_VideoDecodeStats )
      iAnyFailed++;

   ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_ControllerRetransmissionsStats )
         break;
      g_pSM_ControllerRetransmissionsStats = shared_mem_controller_video_retransmissions_stats_open_for_read();
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_ControllerRetransmissionsStats )
      iAnyFailed++;


   ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_VDS_history )
         break;
      g_pSM_VDS_history = shared_mem_video_stream_stats_history_rx_processors_open(true);
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_VDS_history )
      iAnyFailed++;

   ruby_signal_alive();

   if ( (NULL != g_pCurrentModel) && (!g_bSearching) )
   {
      for( int i=0; i<20; i++ )
      {
         if ( NULL != g_pSM_DownstreamInfoRC )
            break;
         g_pSM_DownstreamInfoRC = shared_mem_rc_downstream_info_open_read();
         hardware_sleep_ms(10);
         iAnyNewOpen++;
      }
      if ( NULL == g_pSM_DownstreamInfoRC )
         iAnyFailed++;
   }

   ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_RouterVehiclesRuntimeInfo )
         break;
      g_pSM_RouterVehiclesRuntimeInfo = shared_mem_router_vehicles_runtime_info_open_for_read();
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_RouterVehiclesRuntimeInfo )
      iAnyFailed++;

   if ( 0 == iAnyNewOpen )
      log_line("No new shared mem objects to open.");
   else
      log_line("Opened an additional %d shared mem objects.", iAnyNewOpen );

   if ( 0 != iAnyFailed )
      log_softerror_and_alarm("Failed to open %d shared memory objects.", iAnyFailed);

   synchronize_shared_mems();
}

void _pairing_close_shared_mem()
{
   shared_mem_router_vehicles_runtime_info_close(g_pSM_RouterVehiclesRuntimeInfo);
   g_pSM_RouterVehiclesRuntimeInfo = NULL;

   shared_mem_controller_audio_decode_stats_close(g_pSM_AudioDecodeStats);
   g_pSM_AudioDecodeStats = NULL;


   shared_mem_radio_stats_rx_hist_close(g_pSM_HistoryRxStats);
   g_pSM_HistoryRxStats = NULL;

   shared_mem_radio_stats_close(g_pSM_RadioStats);
   g_pSM_RadioStats = NULL;

   shared_mem_controller_radio_stats_interfaces_rx_graphs_close(g_pSM_RadioStatsInterfaceRxGraph);
   g_pSM_RadioStatsInterfaceRxGraph = NULL;

   shared_mem_video_link_stats_close(g_pSM_VideoLinkStats);
   g_pSM_VideoLinkStats = NULL;

   shared_mem_video_info_stats_close(g_pSM_VideoInfoStats);
   g_pSM_VideoInfoStats = NULL;

   shared_mem_video_info_stats_radio_in_close(g_pSM_VideoInfoStatsRadioIn);
   g_pSM_VideoInfoStatsRadioIn = NULL;

   shared_mem_video_link_graphs_close(g_pSM_VideoLinkGraphs);
   g_pSM_VideoLinkGraphs = NULL;

   shared_mem_video_stream_stats_rx_processors_close(g_pSM_VideoDecodeStats);
   g_pSM_VideoDecodeStats = NULL;

   shared_mem_video_stream_stats_history_rx_processors_close(g_pSM_VDS_history);
   g_pSM_VDS_history = NULL;

   shared_mem_controller_video_retransmissions_stats_close(g_pSM_ControllerRetransmissionsStats);
   g_pSM_ControllerRetransmissionsStats = NULL;

   shared_mem_rc_downstream_info_close(g_pSM_DownstreamInfoRC);
   g_pSM_DownstreamInfoRC = NULL;

   shared_mem_i2c_controller_rc_in_close(g_pSM_RCIn);
   g_pSM_RCIn = NULL;
}

bool pairing_isStarted()
{
   return s_isRXStarted;
}

void pairing_loop()
{
   if ( ! pairing_isStarted() )
      return;
   
   if ( g_bSearching )
      return;


   if ( (s_uTimeToSetAffinities != 0) && s_isRXStarted )
   if ( g_TimeNow > s_uTimeToSetAffinities )
   {
      controller_check_update_processes_affinities();
      s_uTimeToSetAffinities = 0;
   }
}
