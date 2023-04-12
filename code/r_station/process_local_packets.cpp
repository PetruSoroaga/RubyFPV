
#include <stdlib.h>
#include <stdio.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/models.h"
#include "../base/encr.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"

#include "shared_vars.h"
#include "links_utils.h"
#include "ruby_rt_station.h"
#include "processor_rx_audio.h"
#include "processor_rx_video.h"
#include "processor_rx_video_forward.h"
#include "process_radio_in_packets.h"
#include "packets_utils.h"
#include "video_link_adaptive.h"
#include "video_link_keyframe.h"
#include "timers.h"


void _process_local_control_packet(t_packet_header* pPH)
{
   //log_line("Process received local controll packet type: %d", pPH->packet_type);

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY )
   {
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS )
   {
      ruby_ipc_channel_send_message(g_fIPCToCentral, (u8*)pPH, pPH->total_length);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS )
   {
      log_line("Router received a local message to reload core plugins.");
      refresh_CorePlugins(0);
      log_line("Router finished reloading core plugins.");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED )
   {
      if ( ! g_bSearching )
      {
         log_softerror_and_alarm("Received local message to change the search frequency to %s, but no search is in progress!", str_format_frequency(pPH->vehicle_id_dest));
         return;
      }
      log_line("Received local message to change the search frequency to %s", str_format_frequency(pPH->vehicle_id_dest));
      links_set_cards_frequencies_for_search((int)pPH->vehicle_id_dest);
      log_line("Switched search frequency to %s. Broadcasting that router is ready.", str_format_frequency(pPH->vehicle_id_dest));
      broadcast_router_ready();
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_LINK_FREQUENCY_CHANGED )
   {
      u32* pI = (u32*)(((u8*)(pPH))+sizeof(t_packet_header));
      u32 uLinkId = *pI;
      pI++;
      u32 uNewFreq = *pI;
               
      log_line("Received control packet from central that a vehicle radio link frequency changed (radio link %u new freq: %s)", uLinkId+1, str_format_frequency(uNewFreq));
      if ( NULL != g_pCurrentModel && (uLinkId < (u32)g_pCurrentModel->radioLinksParams.links_count) )
      {
         g_pCurrentModel->radioLinksParams.link_frequency[uLinkId] = uNewFreq;
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkId )
               g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i] = uNewFreq;
         }
         links_set_cards_frequencies();

         process_data_rx_video_reset_retransmission_stats();
      }
      log_line("Done processing control packet of frequency change.");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_PASSPHRASE_CHANGED )
   {
      lpp(NULL,0);
      return;
   }

   if ( pPH->packet_type == PACKET_TYLE_LOCAL_CONTROL_RELAY_MODE_SWITCHED )
   {
      g_pCurrentModel->relay_params.uCurrentRelayMode = pPH->vehicle_id_src;
      log_line("Received notification from central that relay mode changed to %d.", g_pCurrentModel->relay_params.uCurrentRelayMode);
      int iCurrentFPS = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps;
      int iDefaultKeyframeInterval = iCurrentFPS * 2 * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL / 1000;
      log_line("Set current keyframe to %d for FPS %d", iDefaultKeyframeInterval, iCurrentFPS);
      video_link_keyframe_set_current_level(iDefaultKeyframeInterval);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
   {
      u32 uChangeType = pPH->vehicle_id_src >> 8;
      log_line("Received control packet to reload model, change type: %u.", uChangeType);

      if ( ! g_bFirstModelPairingDone )
      {
         if ( access( FILE_FIRST_PAIRING_DONE, R_OK ) != -1 )
            g_bFirstModelPairingDone = true;
         if ( g_bFirstModelPairingDone )
            log_line("First model pairing was completed. Using valid user model.");
         else
            log_line("First model pairing is still not done. Still using default model.");
      }

      u32 oldEFlags = g_pCurrentModel->enc_flags;

      if ( NULL != g_pCurrentModel )
      if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL) )
         log_softerror_and_alarm("Failed to load current model.");

      if ( g_pCurrentModel->enc_flags != oldEFlags )
         lpp(NULL, 0);

      if ( uChangeType == MODEL_CHANGED_RELAY_PARAMS )
      {
         log_line("Received notification from central that relay flags and params changed.");
         return;
      }

      // Signal other components about the model change if it's not from central

      if ( pPH->vehicle_id_src == PACKET_COMPONENT_COMMANDS )
      {
         if ( -1 != g_fIPCToTelemetry )
            ruby_ipc_channel_send_message(g_fIPCToTelemetry, (u8*)pPH, pPH->total_length);
         if ( -1 != g_fIPCToRC )
            ruby_ipc_channel_send_message(g_fIPCToRC, (u8*)pPH, pPH->total_length);
      }
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED )
   {
      log_line("Received control packet to reload controller settings.");

      load_ControllerSettings();
      g_pControllerSettings = get_ControllerSettings();

      int iLinks = g_Local_RadioStats.countRadioLinks;
      int iInterf = g_Local_RadioStats.countRadioInterfaces;
      shared_mem_radio_stats_radio_interface  radio_interfaces[MAX_RADIO_INTERFACES];
      memcpy((u8*)&radio_interfaces[0], (u8*)&g_Local_RadioStats.radio_interfaces[0], MAX_RADIO_INTERFACES * sizeof(shared_mem_radio_stats_radio_interface) );

      radio_stats_reset(&g_Local_RadioStats, g_pControllerSettings->nGraphRadioRefreshInterval);
      
      g_Local_RadioStats.countRadioLinks = iLinks;
      g_Local_RadioStats.countRadioInterfaces = iInterf;
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         g_Local_RadioStats.radio_interfaces[i].assignedRadioLinkId = radio_interfaces[i].assignedRadioLinkId;
         g_Local_RadioStats.radio_interfaces[i].uCurrentFrequency = radio_interfaces[i].uCurrentFrequency;
         g_Local_RadioStats.radio_interfaces[i].openedForRead = radio_interfaces[i].openedForRead;
         g_Local_RadioStats.radio_interfaces[i].openedForWrite = radio_interfaces[i].openedForWrite;
         g_Local_RadioStats.radio_interfaces[i].rxQuality = radio_interfaces[i].rxQuality;
         g_Local_RadioStats.radio_interfaces[i].rxRelativeQuality = radio_interfaces[i].rxRelativeQuality;
      }

      if ( NULL != g_pSM_RadioStats )
         memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));

      if ( g_pCurrentModel->hasCamera() )
      {
         processor_rx_video_forward_check_controller_settings_changed();
         process_data_rx_video_on_controller_settings_changed();
      }

      // Signal other components about the model change
      if ( pPH->vehicle_id_src == PACKET_COMPONENT_LOCAL_CONTROL )
      {
         if ( -1 != g_fIPCToTelemetry )
            ruby_ipc_channel_send_message(g_fIPCToTelemetry, (u8*)pPH, pPH->total_length);
         if ( -1 != g_fIPCToRC )
            ruby_ipc_channel_send_message(g_fIPCToRC, (u8*)pPH, pPH->total_length);
      }
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED ||
        pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED ||
        pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED )
   {
      if ( -1 != g_fIPCToTelemetry )
         ruby_ipc_channel_send_message(g_fIPCToTelemetry, (u8*)pPH, pPH->total_length);
      if ( -1 != g_fIPCToRC )
         ruby_ipc_channel_send_message(g_fIPCToRC, (u8*)pPH, pPH->total_length);

      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED )
         g_bUpdateInProgress = true;
      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED ||
           pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED )
         g_bUpdateInProgress = false;
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_START )
   {
      log_line("Starting debug router packets history graph");
      g_bDebugIsPacketsHistoryGraphOn = true;
      g_bDebugIsPacketsHistoryGraphPaused = false;
      g_pDebug_SM_RouterPacketsStatsHistory = shared_mem_router_packets_stats_history_open_write();
      if ( NULL == g_pDebug_SM_RouterPacketsStatsHistory )
      {
         log_softerror_and_alarm("Failed to open shared mem for write for router packets stats history.");
         g_bDebugIsPacketsHistoryGraphOn = false;
         g_bDebugIsPacketsHistoryGraphPaused = true;
      }
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_STOP )
   {
      g_bDebugIsPacketsHistoryGraphOn = false;
      g_bDebugIsPacketsHistoryGraphPaused = true;
      shared_mem_router_packets_stats_history_close(g_pDebug_SM_RouterPacketsStatsHistory);
      return;
   }
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_PAUSE )
   {
      g_bDebugIsPacketsHistoryGraphPaused = true;
   }
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_RESUME )
   {
      g_bDebugIsPacketsHistoryGraphPaused = false;
   }
}


void process_local_control_packets(t_packet_queue* pQueue)
{
   while ( packets_queue_has_packets(pQueue) )
   {
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      int length = -1;
      u8* pBuffer = packets_queue_pop_packet(pQueue, &length);
      if ( NULL == pBuffer || -1 == length )
         break;

      t_packet_header* pPH = (t_packet_header*)pBuffer;
      _process_local_control_packet(pPH);
   }
}
