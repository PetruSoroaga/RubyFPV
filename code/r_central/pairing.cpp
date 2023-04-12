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
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/commands.h"
#include "../base/ruby_ipc.h"
#include "../common/radio_stats.h"
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

static bool s_isRXStarted = false;
static bool s_bPairingReceivedAnything = false;
static bool s_isVideoReceiving = false;
static u32 s_PairingStartTime = 0;
static u32 s_PairingFirstVideoPacketTime = 0;
static bool s_PairingHasReceivedVideoData = false;

sem_t* pSemaphoreRestartVideoPlayer = NULL;

static u32 s_uTimeToSetAffinities = 0;

void _pairing_open_shared_mem();
void _pairing_close_shared_mem();

u32 pairing_getStartTime()
{
   return s_PairingStartTime;
}

bool pairing_start()
{
   log_line("-----------------------------");
   if ( g_bSearching )
      log_line("Starting pairing processes in search mode...");
   else
      log_line("Starting pairing processes for current model...");

   if ( ! g_bSearching )
      onEventBeforePairing();

   s_bPairingReceivedAnything = false;

   if ( (NULL == g_pCurrentModel) && (!g_bSearching) )
   {
      log_line("No current model and no searching. Do nothing (no pairing).");
      log_line("------------------------------------------");
      return true;
   }
   if ( 0 == hardware_get_radio_interfaces_count() )
   {
      log_line("No radio interfaces detect on the system. Do nothing (no pairing).");
      log_line("------------------------------------------");
      return true;
   }

   controller_start_audio(g_pCurrentModel);
   controller_launch_router();
   controller_launch_video_player();

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

   pSemaphoreRestartVideoPlayer = sem_open(SEMAPHORE_RESTART_VIDEO_PLAYER, O_CREAT, S_IWUSR | S_IRUSR, 0);
   if ( NULL == pSemaphoreRestartVideoPlayer )
      log_error_and_alarm("Failed to open semaphore for writing: %s", SEMAPHORE_RESTART_VIDEO_PLAYER);
 
   s_isRXStarted = true;
   s_isVideoReceiving = false;
   s_PairingStartTime = g_TimeNow;
   s_PairingFirstVideoPacketTime = 0;
   s_PairingHasReceivedVideoData = false;
   g_TimePendingRadioFlagsChanged = 0;
   g_PendingRadioFlagsConfirmationLinkId = -1;

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

   if ( ! g_bSearching )
      onEventPaired();

   s_uTimeToSetAffinities = g_TimeNow + 4000;
   
   log_line("Started pairing processes successfully.");
   log_line("---------------------------------------");
   return true;
}

bool pairing_stop()
{
   if ( ! s_isRXStarted )
      return true;
   log_line("-----------------------------");
   log_line("Stopping pairing processes...");

   forward_streams_on_pairing_stop();
   hardware_recording_led_set_off();

   s_PairingStartTime = 0;
   s_uTimeToSetAffinities = 0;
   if ( NULL != pSemaphoreRestartVideoPlayer )
      sem_close(pSemaphoreRestartVideoPlayer);
   pSemaphoreRestartVideoPlayer = NULL;

   _pairing_close_shared_mem();

   ruby_stop_recording();

   handle_commands_start_on_pairing();

   controller_stop_audio();
   controller_stop_rx_telemetry();
   controller_stop_tx_rc();
   controller_stop_video_player();
   controller_stop_router();

   controller_wait_for_stop_all();

   onEventPairingStopped();

   stop_pipes_to_router();
   handle_commands_stop_on_pairing();

   s_isRXStarted = false;
   s_isVideoReceiving = false;

   ruby_clear_all_ipc_channels();

   g_bIsRouterReady = false;
   g_RouterIsReadyTimestamp = 0;

   log_line("Stopped pairing processes successfully.");
   log_line("---------------------------------------");
   return true;
}

void pairing_on_router_ready()
{
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
      if ( NULL != g_psmvds )
         break;
      g_psmvds = shared_mem_video_decode_stats_open(true);
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_psmvds )
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
      if ( NULL != g_psmvds_history )
         break;
      g_psmvds_history = shared_mem_video_decode_stats_history_open(true);
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_psmvds_history )
      iAnyFailed++;

   ruby_signal_alive();

   if ( (NULL != g_pCurrentModel) && (!g_bSearching) )
   {
      for( int i=0; i<20; i++ )
      {
         if ( NULL != g_pPHDownstreamInfoRC )
            break;
         g_pPHDownstreamInfoRC = shared_mem_rc_downstream_info_open_read();
         hardware_sleep_ms(30);
         iAnyNewOpen++;
      }
      if ( NULL == g_pPHDownstreamInfoRC )
         iAnyFailed++;
   }

   ruby_signal_alive();
   for( int i=0; i<20; i++ )
   {
      if ( NULL != g_pSM_ControllerVehiclesAdaptiveVideoInfo )
         break;
      g_pSM_ControllerVehiclesAdaptiveVideoInfo = shared_mem_controller_vehicles_adaptive_video_info_open_for_read();
      hardware_sleep_ms(5);
      iAnyNewOpen++;
   }
   if ( NULL == g_pSM_ControllerVehiclesAdaptiveVideoInfo )
      iAnyFailed++;

   if ( 0 == iAnyNewOpen )
      log_line("No new shared mem objects to open.");
   else
      log_line("Opened an additional %d shared mem objects.", iAnyNewOpen );

   if ( 0 != iAnyFailed )
      log_softerror_and_alarm("Failed to open %d shared memory objects.", iAnyFailed);
}

void _pairing_close_shared_mem()
{
   shared_mem_controller_vehicles_adaptive_video_info_close(g_pSM_ControllerVehiclesAdaptiveVideoInfo);
   g_pSM_ControllerVehiclesAdaptiveVideoInfo = NULL;

   shared_mem_radio_stats_close(g_pSM_RadioStats);
   g_pSM_RadioStats = NULL;

   shared_mem_video_link_stats_close(g_pSM_VideoLinkStats);
   g_pSM_VideoLinkStats = NULL;

   shared_mem_video_info_stats_close(g_pSM_VideoInfoStats);
   g_pSM_VideoInfoStats = NULL;

   shared_mem_video_info_stats_radio_in_close(g_pSM_VideoInfoStatsRadioIn);
   g_pSM_VideoInfoStatsRadioIn = NULL;

   shared_mem_video_link_graphs_close(g_pSM_VideoLinkGraphs);
   g_pSM_VideoLinkGraphs = NULL;

   shared_mem_video_decode_stats_close(g_psmvds);
   g_psmvds = NULL;

   shared_mem_video_decode_stats_history_close(g_psmvds_history);
   g_psmvds_history = NULL;

   shared_mem_controller_video_retransmissions_stats_close(g_pSM_ControllerRetransmissionsStats);
   g_pSM_ControllerRetransmissionsStats = NULL;

   shared_mem_rc_downstream_info_close(g_pPHDownstreamInfoRC);
   g_pPHDownstreamInfoRC = NULL;

   shared_mem_i2c_controller_rc_in_close(g_pSM_RCIn);
   g_pSM_RCIn = NULL;
}

bool pairing_isStarted()
{
   return s_isRXStarted;
}

bool pairing_isReceiving()
{
   if ( ! pairing_isStarted() )
      return false;
   if ( g_bSearching )
      return false;
   if ( NULL == g_pSM_RadioStats )
      return false;
   if ( ! s_bPairingReceivedAnything )
      return false;

   for( int i=0; i<g_pSM_RadioStats->countRadioLinks; i++ )
   {
      if ( g_TimeNow > 2200 && g_pSM_RadioStats->radio_links[i].timeLastRxPacket >= g_TimeNow-2000 )
         return true;
   }

   return false;
}

bool pairing_wasReceiving()
{
   if ( NULL == g_pSM_RadioStats )
      return false;

   return s_bPairingReceivedAnything;
}

bool pairing_hasReceivedVideoStreamData()
{
   if ( NULL == g_pSM_RadioStats )
      return false;

   if ( g_bSearching )
      return false;

   if ( s_PairingHasReceivedVideoData )
      return true;

   if ( 0 != s_PairingFirstVideoPacketTime )
   {
      if ( g_TimeNow >= s_PairingFirstVideoPacketTime + 100 )
      {
          s_PairingHasReceivedVideoData = true;
          return true;
      }
      return false;
   }

   if ( g_TimeNow > 300 && g_pSM_RadioStats->radio_streams[STREAM_ID_VIDEO_1].timeLastRxPacket >= g_TimeNow-300 )
      s_PairingFirstVideoPacketTime = g_pSM_RadioStats->radio_streams[STREAM_ID_VIDEO_1].timeLastRxPacket;
   return false;
}


u32 pairing_getCurrentVehicleID()
{
   if ( ! pairing_isStarted() )
      return 0;
   if ( ! g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo )
      return 0;
   if ( pairing_isReceiving() || pairing_wasReceiving() )
      return g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id;
   return 0;
}

bool pairing_is_connected_to_wrong_model()
{
   if ( ! pairing_isStarted() )
      return false;
   if ( ! pairing_isReceiving() )
      return false;
   if ( NULL == g_pCurrentModel )
      return false;

   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo )
   if ( 0 != g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id )
   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id == g_pCurrentModel->vehicle_id )
      return false;

   bool bHasRelayVehicle = false;
   u32 uRelayVehicleId = 0;

   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId > 0 )
   if ( g_pCurrentModel->relay_params.uRelayVehicleId != 0 )
   if ( g_pCurrentModel->relay_params.uRelayVehicleId != MAX_U32 )
   {
      bHasRelayVehicle = true;
      uRelayVehicleId = g_pCurrentModel->relay_params.uRelayVehicleId;
   }

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( 0 == g_VehiclesRuntimeInfo[i].uVehicleId || MAX_U32 == g_VehiclesRuntimeInfo[i].uVehicleId )
         continue;

      if ( ! g_VehiclesRuntimeInfo[i].bGotRubyTelemetryInfo )
         continue;

      if ( g_VehiclesRuntimeInfo[i].headerRubyTelemetryExtended.vehicle_id != g_pCurrentModel->vehicle_id )
      if ( (!bHasRelayVehicle) || (bHasRelayVehicle && (g_VehiclesRuntimeInfo[i].headerRubyTelemetryExtended.vehicle_id != uRelayVehicleId)) )
         return true;
   }
   return false;
}

void pairing_loop()
{
   if ( ! pairing_isStarted() )
      return;
   
   if ( g_bSearching )
      return;


   if ( s_uTimeToSetAffinities != 0 && s_PairingStartTime != 0 )
   if ( g_TimeNow > s_uTimeToSetAffinities )
   {
      controller_check_update_processes_affinities();
      s_uTimeToSetAffinities = 0;
   }

   if ( g_bIsRouterReady && (NULL != g_pSM_RadioStats) )
   if ( ! s_bPairingReceivedAnything )
   {
      for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
      {
         if ( g_pSM_RadioStats->radio_interfaces[i].totalRxPackets > 0 )
         //if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotTelemetryInfo && 0 != g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_id )
         {
            s_bPairingReceivedAnything = true;
            onEventPairingStartReceivingData();
            break;
         }
      }
   }

   int val = 0;
   if ( NULL != pSemaphoreRestartVideoPlayer )
   if ( 0 == sem_getvalue(pSemaphoreRestartVideoPlayer, &val) )
   if ( 0 < val )
   if ( EAGAIN != sem_trywait(pSemaphoreRestartVideoPlayer) )
   {
      log_line("Received event to restart video player.");
      ruby_stop_recording();
      controller_stop_video_player();
      controller_launch_video_player();

      s_uTimeToSetAffinities = g_TimeNow+4000;
   } 
}
