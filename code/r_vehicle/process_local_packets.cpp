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
#include "../base/encr.h"
#include "../base/models_list.h"
#include "../base/shared_mem.h"
#include "../base/ruby_ipc.h"
#include "../base/hw_procs.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/hardware_serial.h"
#include "../base/vehicle_settings.h"
#include "../base/radio_utils.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "process_local_packets.h"
#include "processor_tx_video.h"
#include "utils_vehicle.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/fec.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../radio/radio_duplicate_det.h"
#include "packets_utils.h"
#include "shared_vars.h"
#include "timers.h"
#include "ruby_rt_vehicle.h"
#include "utils_vehicle.h"
#include "launchers_vehicle.h"
#include "processor_tx_video.h"
#include "video_link_stats_overwrites.h"
#include "video_link_check_bitrate.h"
#include "video_link_auto_keyframe.h"
#include "processor_relay.h"
#include "events.h"


u32 _get_previous_frequency_switch(int nLink)
{
   u32 nCurrentFreq = g_pCurrentModel->radioLinksParams.link_frequency_khz[nLink];
   u32 nNewFreq = nCurrentFreq;

   int nBand = getBand(nCurrentFreq);
   int nChannel = getChannelIndexForFrequency(nBand,nCurrentFreq);

   int maxLoop = getChannels23Count() + getChannels24Count() + getChannels25Count() + getChannels58Count();

   while ( maxLoop > 0 )
   {
      maxLoop--;
      nChannel--;
      if ( nChannel < 0 )
      {
         if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_58;
            nChannel = getChannels58Count();
            continue;
         }
         if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_23;
            nChannel = getChannels23Count();
            continue;
         }
         if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_24;
            nChannel = getChannels24Count();
            continue;
         }
         if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_25;
            nChannel = getChannels25Count();
            continue;
         }
         continue;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
      if ( g_pCurrentModel->functions_params.uChannels23FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels23()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
      if ( g_pCurrentModel->functions_params.uChannels24FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels24()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
      if ( g_pCurrentModel->functions_params.uChannels25FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels25()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
      if ( g_pCurrentModel->functions_params.uChannels58FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels58()[nChannel];
         return nNewFreq;
      }
   }
   return nNewFreq;   
}


u32 _get_next_frequency_switch(int nLink)
{
   u32 nCurrentFreq = g_pCurrentModel->radioLinksParams.link_frequency_khz[nLink];
   u32 nNewFreq = nCurrentFreq;

   int nBand = getBand(nCurrentFreq);
   int nChannel = getChannelIndexForFrequency(nBand,nCurrentFreq);
   int nCurrentBandChannelsCount = 0;

   if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
      for( int i=0; i<getChannels23Count(); i++ )
         if ( nCurrentFreq == getChannels23()[i] )
         {
            nChannel = i;
            nCurrentBandChannelsCount = getChannels23Count();
            break;
         }

   if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
      for( int i=0; i<getChannels24Count(); i++ )
         if ( nCurrentFreq == getChannels24()[i] )
         {
            nChannel = i;
            nCurrentBandChannelsCount = getChannels24Count();
            break;
         }

   if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
      for( int i=0; i<getChannels25Count(); i++ )
         if ( nCurrentFreq == getChannels25()[i] )
         {
            nChannel = i;
            nCurrentBandChannelsCount = getChannels25Count();
            break;
         }

   if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
      for( int i=0; i<getChannels58Count(); i++ )
         if ( nCurrentFreq == getChannels58()[i] )
         {
            nChannel = i;
            nCurrentBandChannelsCount = getChannels58Count();
            break;
         }

   int maxLoop = getChannels23Count() + getChannels24Count() + getChannels25Count() + getChannels58Count();

   while ( maxLoop > 0 )
   {
      maxLoop--;
      nChannel++;
      if ( nChannel >= nCurrentBandChannelsCount )
      {
         if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_24;
            nChannel = -1;
            nCurrentBandChannelsCount = getChannels24Count();
            continue;
         }
         
         if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_25;
            nChannel = -1;
            nCurrentBandChannelsCount = getChannels25Count();
            continue;
         }

         if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_58;
            nChannel = -1;
            nCurrentBandChannelsCount = getChannels58Count();
            continue;
         }

         if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
         {
            nBand = RADIO_HW_SUPPORTED_BAND_23;
            nChannel = -1;
            nCurrentBandChannelsCount = getChannels23Count();
            continue;
         }
         continue;
      }

      if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
      if ( g_pCurrentModel->functions_params.uChannels23FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels23()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
      if ( g_pCurrentModel->functions_params.uChannels24FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels24()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
      if ( g_pCurrentModel->functions_params.uChannels25FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels25()[nChannel];
         return nNewFreq;
      }
      if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
      if ( g_pCurrentModel->functions_params.uChannels58FreqSwitch[nLink] & (((u32)0x01)<<nChannel) )
      {
         nNewFreq = getChannels58()[nChannel];
         return nNewFreq;
      }
   }
   return nNewFreq;
}

void _process_local_notification_model_changed(t_packet_header* pPH, int changeType, int fromComponentId, int iExtraParam)
{
   log_line("Received local notification to reload model. Change type: %d (%s), from component id: %d (%s)", changeType, str_get_model_change_type(changeType), fromComponentId, str_get_component_id(fromComponentId));
   
   if ( (changeType == MODEL_CHANGED_STATS) && (fromComponentId == PACKET_COMPONENT_TELEMETRY) )
   {
      log_line("Received event from telemetry component that model stats where updated. Updating local copy. Signal other components too.");
      if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, false) )
         log_error_and_alarm("Can't load current model vehicle.");
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)pPH, pPH->total_length);
      return;
   }

   if ( changeType == MODEL_CHANGED_SERIAL_PORTS )
   {
      if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, false) )
         log_error_and_alarm("Can't load current model vehicle.");
      hardware_reload_serial_ports();
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   type_radio_interfaces_parameters oldRadioInterfacesParams;
   type_radio_links_parameters oldRadioLinksParams;
   audio_parameters_t oldAudioParams;
   type_relay_parameters oldRelayParams;

   memcpy(&oldRadioInterfacesParams, &(g_pCurrentModel->radioInterfacesParams), sizeof(type_radio_interfaces_parameters));
   memcpy(&oldRadioLinksParams, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   memcpy(&oldAudioParams, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t));
   memcpy(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
   u32 old_ef = g_pCurrentModel->enc_flags;
   bool bMustSignalOtherComponents = true;
   bool bMustReinitVideo = true;
   int iPreviousRadioGraphsRefreshInterval = g_pCurrentModel->m_iRadioInterfacesGraphRefreshInterval;

   if ( changeType == MODEL_CHANGED_RADIO_POWERS )
   {
      log_line("Tx powers before updating local model: global: %d, Atheros: %d, RTL: %d, SiK: %d",
         g_pCurrentModel->radioInterfacesParams.txPower,
         g_pCurrentModel->radioInterfacesParams.txPowerAtheros,
         g_pCurrentModel->radioInterfacesParams.txPowerRTL,
         g_pCurrentModel->radioInterfacesParams.txPowerSiK);
   }

   // Reload model first

   load_VehicleSettings();

   VehicleSettings* pVS = get_VehicleSettings();
   if ( NULL != pVS )
      radio_rx_set_timeout_interval(pVS->iDevRxLoopTimeout);
     
   if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, false) )
      log_error_and_alarm("Can't load current model vehicle.");

   if ( iPreviousRadioGraphsRefreshInterval != g_pCurrentModel->m_iRadioInterfacesGraphRefreshInterval )
   {
      u32 uRefreshIntervalMs = 100;
      switch ( g_pCurrentModel->m_iRadioInterfacesGraphRefreshInterval )
      {
         case 0: uRefreshIntervalMs = 10; break;
         case 1: uRefreshIntervalMs = 20; break;
         case 2: uRefreshIntervalMs = 50; break;
         case 3: uRefreshIntervalMs = 100; break;
         case 4: uRefreshIntervalMs = 200; break;
         case 5: uRefreshIntervalMs = 500; break;
      }
      radio_stats_set_graph_refresh_interval(&g_SM_RadioStats, uRefreshIntervalMs);
   }

   if ( changeType == MODEL_CHANGED_CONTROLLER_TELEMETRY )
   {
      return;
   }

   if ( changeType == MODEL_CHANGED_SIK_PACKET_SIZE )
   {
      log_line("Received local notification that SiK packet size was changed to %d bytes", g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      radio_tx_set_sik_packet_size(g_pCurrentModel->radioLinksParams.iSiKPacketSize);
      return;
   }

   if ( changeType == MODEL_CHANGED_OSD_PARAMS )
   {
      log_line("Received local notification that model OSD params have been changed.");
      bMustReinitVideo = false;
   }

   if ( changeType == MODEL_CHANGED_ROTATED_RADIO_LINKS )
   {
      log_line("Received local notification to rotate vehicle radio links.");
      g_pCurrentModel->rotateRadioLinksOrder();
      saveCurrentModel();

      int iTmp = 0;

      // Rotate info in radio stats: rotate radio links ids
      for( int i=0; i<g_SM_RadioStats.countLocalRadioLinks; i++ )
      {
         iTmp = g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId;
         g_SM_RadioStats.radio_links[i].lastTxInterfaceIndex = -1;
         if ( g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId >= 0 )
         {
            g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId++;
            if ( g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId >= g_SM_RadioStats.countVehicleRadioLinks )
               g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId = 0;
            log_line("Rotated radio stats local radio link %d from vehicle radio link %d to vehicle radio link %d",
               i+1, iTmp+1, g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId);
         }
      }

      // Rotate info in radio stats: rotate radio interfaces assigned radio links
      for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId >= 0 )
         {
            iTmp = g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId;
            g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId++;
            if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId >= g_SM_RadioStats.countVehicleRadioLinks )
               g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId = 0;
            log_line("Rotated radio stats local radio interface %d (assigned to local radio link %d) from vehicle radio link %d to vehicle radio link %d",
               i, g_SM_RadioStats.radio_interfaces[i].assignedLocalRadioLinkId+1,
               iTmp, g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId);
         }
      }

      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)pPH, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)pPH, pPH->total_length);
      return;
   }

   if ( changeType == MODEL_CHANGED_AUDIO_PARAMS )
   {
      log_line("Received notification that audio parameters have changed.");
      if ( NULL != g_pProcessorTxAudio )
         g_pProcessorTxAudio->closeAudioStream();
      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->audio_params.has_audio_device) )
         vehicle_stop_audio_capture(g_pCurrentModel);

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->audio_params.has_audio_device) && (g_pCurrentModel->audio_params.enabled) )
      {
         vehicle_launch_audio_capture(g_pCurrentModel);
         g_pProcessorTxAudio->openAudioStream();
      }
      return;
   }

   if ( changeType == MODEL_CHANGED_SWAPED_RADIO_INTERFACES )
   {
      log_line("Received notification to reload model due to radio interfaces swap command.");
      if ( ! g_pCurrentModel->canSwapEnabledHighCapacityRadioInterfaces() )
      {
         log_line("No high capacity radio interfaces that have same capabilities that can be swapped. No swap done.");
         send_alarm_to_controller(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_NOT_POSSIBLE, 0, 10);
         return;
      }
      
      // Swap model radio interfaces and update local radio stats info structures

      if ( ! g_pCurrentModel->swapEnabledHighCapacityRadioInterfaces() )
      {
         log_line("No high capacity radio interfaces that have same capabilities that can be swapped. No swap done.");
         send_alarm_to_controller(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_FAILED, 0, 10);
         return;
      }
      saveCurrentModel();

      int iInterface1 = g_pCurrentModel->getLastSwappedRadioInterface1();
      int iInterface2 = g_pCurrentModel->getLastSwappedRadioInterface2();

      if ( (iInterface1 < 0) || (iInterface1 >= hardware_get_radio_interfaces_count()) )
      {
         log_line("Invalid radio interfaces indexes to swap.");
         send_alarm_to_controller(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_FAILED, 0, 10);
         return;
      }

      if ( (iInterface2 < 0) || (iInterface2 >= hardware_get_radio_interfaces_count()) )
      {
         log_line("Invalid radio interfaces indexes to swap.");
         send_alarm_to_controller(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_FAILED, 0, 10);
         return;
      }

      int iTmp = g_SM_RadioStats.radio_interfaces[iInterface1].assignedVehicleRadioLinkId;
      g_SM_RadioStats.radio_interfaces[iInterface1].assignedVehicleRadioLinkId = g_SM_RadioStats.radio_interfaces[iInterface2].assignedVehicleRadioLinkId;
      g_SM_RadioStats.radio_interfaces[iInterface2].assignedVehicleRadioLinkId = iTmp;

      iTmp = g_SM_RadioStats.radio_interfaces[iInterface1].assignedLocalRadioLinkId;
      g_SM_RadioStats.radio_interfaces[iInterface1].assignedLocalRadioLinkId = g_SM_RadioStats.radio_interfaces[iInterface2].assignedLocalRadioLinkId;
      g_SM_RadioStats.radio_interfaces[iInterface2].assignedLocalRadioLinkId = iTmp;

      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[iInterface1];
      if ( (iRadioLinkId >= 0) && (iRadioLinkId < g_pCurrentModel->radioLinksParams.links_count) )
      if ( radio_utils_set_interface_frequency(g_pCurrentModel, iInterface1, iRadioLinkId, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId], g_pProcessStats, 0) )
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, iInterface1, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId]);

      iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[iInterface2];
      if ( (iRadioLinkId >= 0) && (iRadioLinkId < g_pCurrentModel->radioLinksParams.links_count) )
      if ( radio_utils_set_interface_frequency(g_pCurrentModel, iInterface2, iRadioLinkId, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId], g_pProcessStats, 0) )
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, iInterface2, g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId]);

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)pPH, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)pPH, pPH->total_length);
      
      send_alarm_to_controller(ALARM_ID_GENERIC, ALARM_ID_GENERIC_TYPE_SWAPPED_RADIO_INTERFACES, 0, 10);
      return;
      
      /*
      close_radio_interfaces();

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      configure_radio_interfaces_for_current_model(g_pCurrentModel, g_pProcessStats);

      open_radio_interfaces();

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }
      */
      log_line("Completed reinitializing radio interfaces due to local notification that radio interfaces where swaped.");
      return;
   }

   if ( changeType == MODEL_CHANGED_RELAY_PARAMS )
   {
      log_line("Received notification from commands that relay params or mode changed.");
      u8 uOldRelayMode = oldRelayParams.uCurrentRelayMode;
      oldRelayParams.uCurrentRelayMode = g_pCurrentModel->relay_params.uCurrentRelayMode;
      
      // Only relay mode changed?

      if ( 0 == memcmp(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters)) )
      {
         if ( uOldRelayMode != g_pCurrentModel->relay_params.uCurrentRelayMode )
            onEventRelayModeChanged(uOldRelayMode, g_pCurrentModel->relay_params.uCurrentRelayMode, "command");
         else
            log_line("No change in relay flags or relay mode detected. Ignoring notification.");
         return;
      }

      oldRelayParams.uCurrentRelayMode = uOldRelayMode;
      
      u32 uOldRelayFlags = oldRelayParams.uRelayCapabilitiesFlags;
      oldRelayParams.uRelayCapabilitiesFlags = g_pCurrentModel->relay_params.uRelayCapabilitiesFlags;

      // Only relay flags changed?
      
      if ( 0 == memcmp(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters)) )
      {
         if ( uOldRelayFlags != g_pCurrentModel->relay_params.uRelayCapabilitiesFlags )
         {
            log_line("Changed relay flags from %u to %u.", uOldRelayFlags, g_pCurrentModel->relay_params.uRelayCapabilitiesFlags);
            relay_on_relay_flags_changed(g_pCurrentModel->relay_params.uRelayCapabilitiesFlags);
         }
         else
            log_line("No change in relay flags or relay mode detected. Ignoring notification.");
         return;
      }
      oldRelayParams.uRelayCapabilitiesFlags = uOldRelayFlags;
      
      
      // Relay link changed?
      if ( oldRelayParams.isRelayEnabledOnRadioLinkId != g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId )
      {
         // Notify main router and let the main router decide when to update the relay configuration. 
         g_TimeLastNotificationRelayParamsChanged = g_TimeNow;
      
      }

      // Relayed vehicle ID changed?
      if ( oldRelayParams.uRelayedVehicleId != g_pCurrentModel->relay_params.uRelayedVehicleId )
      {
         log_line("Changed relayed vehicle id, from %u to %u.", oldRelayParams.uRelayedVehicleId, g_pCurrentModel->relay_params.uRelayedVehicleId);
         radio_duplicate_detection_remove_data_for_all_except(g_uControllerId);
         relay_on_relayed_vehicle_id_changed(g_pCurrentModel->relay_params.uRelayedVehicleId);
      }
      return;
   }

   if ( changeType == MODEL_CHANGED_VIDEO_H264_QUANTIZATION )
   {
      int iQuant = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization;
      log_line("Received local notification that h264 quantization changed. New value: %d", iQuant);
      if ( ! g_pCurrentModel->hasCamera() )
         return;

      bool bUseAdaptiveVideo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?true:false;
      if ( ! bUseAdaptiveVideo )
      {
         if ( iQuant > 0 )
            video_link_set_fixed_quantization_values((u8)iQuant);
         else
         {
            g_SM_VideoLinkStats.overwrites.currentH264QUantization = 0;
            flag_need_video_capture_restart();
         }
      }
      else
      {
         if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_SM_VideoLinkStats.overwrites.userVideoLinkProfile )
         {
            if ( iQuant > 0 )
            {
               g_SM_VideoLinkStats.overwrites.currentH264QUantization = (u8)iQuant;
               video_link_set_last_quantization_set(g_SM_VideoLinkStats.overwrites.currentH264QUantization);
               send_control_message_to_raspivid( RASPIVID_COMMAND_ID_QUANTIZATION_MIN, g_SM_VideoLinkStats.overwrites.currentH264QUantization );
            }
         } 
      }
      return;
   }

   if ( changeType == MODEL_CHANGED_DEFAULT_MAX_ADATIVE_KEYFRAME )
   {
      log_line("Received local notification that default max adaptive keyframe interval changed. New value: %u ms", g_pCurrentModel->video_params.uMaxAutoKeyframeIntervalMs);
      if ( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe_ms > 0 )
         log_line("Current video profile %s is on fixed keyframe: %d ms. Nothing to do.", str_get_video_profile_name(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile), g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe_ms);
      else
      {
         log_line("Current video profile %s is on auto keyframe. Set new default auto max keyframe value.", str_get_video_profile_name(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile) );
         video_link_auto_keyframe_init();
      }
      return;
   }

   if ( changeType == MODEL_CHANGED_VIDEO_KEYFRAME )
   {
      int keyframe_ms = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe_ms;
      log_line("Received local notification that video keyframe interval changed. New value: %d ms", keyframe_ms);
      if ( keyframe_ms > 0 )
      {
         log_line("Current video profile %s is on fixed keyframe, new value: %d ms. Adjust now the video capture keyframe param.", str_get_video_profile_name(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile), keyframe_ms);  
         process_data_tx_video_set_current_keyframe_interval(keyframe_ms, "user set fixed kf");
      }
      else
      {
         log_line("Current video profile %s is on auto keyframe. Set the default auto max keyframe value.", str_get_video_profile_name(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile) ); 
      }
      return;
   }

   if ( changeType == MODEL_CHANGED_USER_SELECTED_VIDEO_PROFILE )
   {
      log_line("Received local notification that the user selected video profile has changed.");
      g_SM_VideoLinkStats.overwrites.userVideoLinkProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
      video_stats_overwrites_reset_to_highest_level();
   }

   if ( changeType == MODEL_CHANGED_RADIO_LINK_CAPABILITIES )
   {
      log_line("Received local notification (type %d) that link capabilities flags changed, that do not require additional processing besides reloading the current model.", (int)changeType);
      bMustSignalOtherComponents = false;
      bMustReinitVideo = false;
      return;
   }

   if ( changeType == MODEL_CHANGED_RADIO_LINK_PARAMS )
   {
      log_line("Received local notification (type %d) that radio link params have changed on model's radio link %d.", (int)changeType, iExtraParam+1);
      bMustSignalOtherComponents = false;
      bMustReinitVideo = false;
      return;
   }

   if ( changeType == MODEL_CHANGED_RADIO_POWERS )
   {
      log_line("Received local notification that radio powers have changed.");
      log_line("Tx powers after updating local model: global: %d, Atheros: %d, RTL: %d, SiK: %d",
         g_pCurrentModel->radioInterfacesParams.txPower,
         g_pCurrentModel->radioInterfacesParams.txPowerAtheros,
         g_pCurrentModel->radioInterfacesParams.txPowerRTL,
         g_pCurrentModel->radioInterfacesParams.txPowerSiK);

      if ( g_pCurrentModel->radioInterfacesParams.txPowerSiK != oldRadioInterfacesParams.txPowerSiK )
      {
         log_line("SiK radio interfaces tx power was changed from %d to %d. Updating SiK radio interfaces...", oldRadioInterfacesParams.txPowerSiK, g_pCurrentModel->radioInterfacesParams.txPowerSiK );
         
         if ( g_SiKRadiosState.bConfiguringToolInProgress )
         {
            log_softerror_and_alarm("Another SiK configuration is in progress. Ignoring this one.");
            return;
         }

         if ( g_SiKRadiosState.bConfiguringSiKThreadWorking )
         {
            log_softerror_and_alarm("SiK reinitialization thread is in progress. Ignoring this notification.");
            return;
         }

         if ( g_SiKRadiosState.bMustReinitSiKInterfaces )
         {
            log_softerror_and_alarm("SiK reinitialization is already flagged. Ignoring this notification.");
            return;
         }

         close_and_mark_sik_interfaces_to_reopen();
         g_SiKRadiosState.bConfiguringToolInProgress = true;
         g_SiKRadiosState.uTimeStartConfiguring = g_TimeNow;
         
         send_alarm_to_controller(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE, 0, 10);

         char szCommand[256];
         sprintf(szCommand, "rm -rf %s", FILE_TMP_SIK_CONFIG_FINISHED);
         hw_execute_bash_command(szCommand, NULL);

         sprintf(szCommand, "./ruby_sik_config none 0 -power %d &", g_pCurrentModel->radioInterfacesParams.txPowerSiK);
         hw_execute_bash_command(szCommand, NULL);
         return;
      }
      bMustReinitVideo = false;
   }

   if ( changeType == MODEL_CHANGED_RADIO_DATARATES )
   {
      int iRadioLink = iExtraParam;
      if ( 0 == memcmp((u8*)&oldRadioLinksParams, (u8*)&g_pCurrentModel->radioLinksParams, sizeof(type_radio_links_parameters) ) )
      {
         log_line("Received local notification that radio data rates have changed on radio link %d but no data rate was changed. Ignoring it.", iRadioLink + 1);
         return;
      }
      log_line("Received local notification that radio data rates have changed on radio link %d", iRadioLink+1);
      log_line("New radio data rates for link %d: v: %d, d: %d/%d, u: %d/%d",
            iRadioLink+1, g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.uDownlinkDataDataRateType[iRadioLink],
            g_pCurrentModel->radioLinksParams.link_datarate_data_bps[iRadioLink],
            g_pCurrentModel->radioLinksParams.uUplinkDataDataRateType[iRadioLink],
            g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[iRadioLink]);

      // If downlink data rate for an Atheros card has changed, update it.
      
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != iRadioLink )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;
         
         int nRateTx = g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iRadioLink];
         update_atheros_card_datarate(g_pCurrentModel, i, nRateTx, g_pProcessStats);
         g_TimeNow = get_current_timestamp_ms();
      }
      return;
   }
   if ( changeType == MODEL_CHANGED_ADAPTIVE_VIDEO_FLAGS )
   {
      log_line("Received local notification that adaptive video link capabilities changed.");
      bMustSignalOtherComponents = false;
      bMustReinitVideo = false;
      //bool bUseAdaptiveVideo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?true:false;

      //if ( (! bOldUseAdaptiveVideo) && bUseAdaptiveVideo )
         video_stats_overwrites_init();
      //if ( bOldUseAdaptiveVideo && (! bUseAdaptiveVideo ) )
         video_stats_overwrites_reset_to_highest_level();
      return;
   }

   if ( changeType == MODEL_CHANGED_EC_SCHEME )
   {
      log_line("Received local notification that EC scheme was changed by user.");
      bMustSignalOtherComponents = false;
      bMustReinitVideo = false;
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_pCurrentModel->video_params.user_selected_video_link_profile )
         video_stats_overwrites_reset_to_highest_level();
      return;
   }

   if ( changeType == MODEL_CHANGED_VIDEO_BITRATE )
   {
      log_line("Received local notification that video bitrate changed by user.");
      bMustSignalOtherComponents = false;
      bMustReinitVideo = false;
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_pCurrentModel->video_params.user_selected_video_link_profile )
         video_stats_overwrites_reset_to_highest_level();
      return;
   }

   if ( changeType == MODEL_CHANGED_SIK_FREQUENCY )
   {
      log_line("Received local notification that a SiK radio frequency was changed. Updating SiK interfaces...");
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( hardware_radio_index_is_sik_radio(i) )
            flag_update_sik_interface(i);
      }
      bMustSignalOtherComponents = false;
      bMustReinitVideo = false;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   if ( changeType == MODEL_CHANGED_RESET_RADIO_LINK )
   {
      int iLink = iExtraParam;
      log_line("Received local notification that radio link %d was reseted.", iLink+1);
      bMustSignalOtherComponents = true;
      bMustReinitVideo = false;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   if ( changeType == MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS )
   {
      int iLink = iExtraParam;
      log_line("Received local notification that radio flags or datarates changed on radio link %d. Update radio frames type right away.", iLink+1);
      
      u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[iLink];
      log_line("Radio link %d new datarates: %d/%d", iLink+1, g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iLink], g_pCurrentModel->radioLinksParams.link_datarate_data_bps[iLink]);
      log_line("Radio link %d new radio flags: %s", iLink+1, str_get_radio_frame_flags_description2(radioFlags));
      
      if( g_pCurrentModel->radioLinkIsSiKRadio(iLink) )
      {
         log_line("Radio flags or radio datarates changed on a SiK radio link (link %d).", iLink+1);
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == iLink )
            if ( hardware_radio_index_is_sik_radio(i) )
               flag_update_sik_interface(i);
         }
      }
      else
      {
         log_line("Radio link %d is regular 2.4/5.8 link.", iLink+1);
         radio_set_frames_flags(radioFlags);
      }

      bMustSignalOtherComponents = false;
      bMustReinitVideo = false;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   if ( old_ef != g_pCurrentModel->enc_flags )
   {
      if ( g_pCurrentModel->enc_flags == MODEL_ENC_FLAGS_NONE )
         rpp();
      else
      {
         if ( ! lpp(NULL, 0) )
         {
            g_pCurrentModel->enc_flags = MODEL_ENC_FLAGS_NONE;
            saveCurrentModel();
         }
      }
      bMustSignalOtherComponents = false;
      bMustReinitVideo = false;
   }

   if ( bMustReinitVideo )
      process_data_tx_video_signal_model_changed();

   if ( 0 != memcmp(&oldAudioParams, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t)) )
   {
      if ( NULL != g_pProcessorTxAudio )
      {
         g_pProcessorTxAudio->closeAudioStream();
         g_pProcessorTxAudio->openAudioStream();
      }
   }

   // Signal other components about the model change

   if ( bMustSignalOtherComponents )
   {
      if ( fromComponentId == PACKET_COMPONENT_COMMANDS )
      {
         ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
         ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)pPH, pPH->total_length);
      }
      if ( fromComponentId == PACKET_COMPONENT_TELEMETRY )
      {
         ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)pPH, pPH->total_length);
         ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)pPH, pPH->total_length);
      }
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}

void process_local_control_packet(t_packet_header* pPH)
{
   if ( NULL != g_pProcessStats )
   {
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }
 
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS )
   {
      log_line("Router received local notification to send model settings to controller.");
      log_line("Tell rx_commands to do it.");

      g_bHasSentVehicleSettingsAtLeastOnce = true;
      
      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS, STREAM_ID_DATA);
      PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
      PH.total_length = sizeof(t_packet_header);

      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_PAUSE_VIDEO )
   {
      log_line("Received controll message: Video is paused.");
      process_data_tx_video_pause_tx();
      g_bVideoPaused = true;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_SIK_RADIO_SERIAL_SPEED )
   {
      int iRadioIndex = (int) pPH->vehicle_id_src;
      int iBaudRate = (int) pPH->vehicle_id_dest;
      log_line("Received local message to change Sik Radio (radio interface %d) local baud rate (Must communicate at: %d bps).", iRadioIndex+1, iBaudRate);

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioIndex);
      if ( NULL == pRadioHWInfo || (!hardware_radio_is_sik_radio(pRadioHWInfo)) )
      {
         log_softerror_and_alarm("Radio interface %d is not a SiK radio.", iRadioIndex+1);
         return;
      }
      hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioHWInfo->szDriver);
      if ( NULL == pSerialPort )
      {
         log_softerror_and_alarm("Failed to get serial port hardware info for serial port [%s].", pRadioHWInfo->szDriver);
         return;
      }
      log_line("New serial port [%s] baudrate: %ld bps", pRadioHWInfo->szDriver, pSerialPort->lPortSpeed);

      if ( g_SiKRadiosState.bConfiguringToolInProgress )
      {
         log_softerror_and_alarm("Another SiK configuration is in progress. Ignoring this one.");
         return;
      }

      if ( g_SiKRadiosState.bConfiguringSiKThreadWorking )
      {
         log_softerror_and_alarm("SiK reinitialization thread is in progress. Ignoring this notification.");
         return;
      }
      if ( g_SiKRadiosState.bMustReinitSiKInterfaces )
      {
         log_softerror_and_alarm("SiK reinitialization is already flagged. Ignoring this notification.");
         return;
      }

      close_and_mark_sik_interfaces_to_reopen();
      g_SiKRadiosState.bConfiguringToolInProgress = true;
      g_SiKRadiosState.uTimeStartConfiguring = g_TimeNow;
      
      send_alarm_to_controller(ALARM_ID_GENERIC_STATUS_UPDATE, ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE, 0, 10);

      char szCommand[256];
      sprintf(szCommand, "rm -rf %s", FILE_TMP_SIK_CONFIG_FINISHED);
      hw_execute_bash_command(szCommand, NULL);

      sprintf(szCommand, "./ruby_sik_config %s %d -serialspeed %d &", pRadioHWInfo->szDriver, iBaudRate, (int)pSerialPort->lPortSpeed);
      hw_execute_bash_command(szCommand, NULL);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_RESUME_VIDEO )
   {
      log_line("Received controll message: Video is resumed.");
      process_data_tx_video_resume_tx();
      g_bVideoPaused = false;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_START_VIDEO_PROGRAM )
   {
      log_line("Received controll message to start video capture program. Parameter: %u", pPH->vehicle_id_dest);
      if ( g_pCurrentModel->hasCamera() )
      {
         if ( g_pCurrentModel->isActiveCameraHDMI() )
            hardware_sleep_ms(800);
         vehicle_launch_video_capture(g_pCurrentModel, &(g_SM_VideoLinkStats.overwrites));
         vehicle_check_update_processes_affinities(true, g_pCurrentModel->isActiveCameraVeye());
      }
      else
         log_line("Vehicle has no camera. No video capture started.");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM )
   {
      log_line("Received controll message to restart video capture program. Parameter: %u", pPH->vehicle_id_dest);
      if ( g_pCurrentModel->hasCamera() )
      {
         flag_need_video_capture_restart();
      } 
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_FORCE_AUTO_VIDEO_PROFILE )
   {
      int iVideoProfile = -1;
      if ( NULL != g_pCurrentModel )
         iVideoProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
      if ( pPH->vehicle_id_dest < 0xFF )
         iVideoProfile = (int) pPH->vehicle_id_dest;
      if ( iVideoProfile < 0 || iVideoProfile >= MAX_VIDEO_LINK_PROFILES )
      {
         iVideoProfile = 0;
         if ( NULL != g_pCurrentModel )
            iVideoProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
      }
      log_line("Received local message to force video profile to %s", str_get_video_profile_name(iVideoProfile));
      if ( iVideoProfile < 0 || iVideoProfile == g_pCurrentModel->video_params.user_selected_video_link_profile )
      {
         g_iForcedVideoProfile = iVideoProfile;
         video_stats_overwrites_reset_to_forced_profile();
         g_iForcedVideoProfile = -1;
         log_line("Cleared forced video profile flag.");
      }
      else
      {
         g_iForcedVideoProfile = iVideoProfile;
         video_stats_overwrites_reset_to_forced_profile();
      }
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
   {
      u8 fromComponentId = (pPH->vehicle_id_src & 0xFF);
      u8 changeType = (pPH->vehicle_id_src >> 8 ) & 0xFF;
      u8 uExtraParam = (pPH->vehicle_id_src >> 16 ) & 0xFF;
      _process_local_notification_model_changed(pPH, (int)changeType, (int)fromComponentId, (int)uExtraParam);
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_SIGNAL_USER_SELECTED_VIDEO_PROFILE_CHANGED )
   {
      log_line("Router received message that user selected video link profile was changed.");
      video_stats_overwrites_init();
      process_data_tx_video_signal_encoding_changed();
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_REINITIALIZE_RADIO_LINKS )
   {
      log_line("Received local request to reinitialize radio interfaces from a controller command...");

      if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, false) )
         log_error_and_alarm("Can't load current model vehicle.");
      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      close_radio_interfaces();

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      open_radio_interfaces();

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      log_line("Completed reinitializing radio interfaces due to local request from a controller command.");
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_REBOOT )
   {
      // Signal other components about the reboot request
      if ( pPH->vehicle_id_src == PACKET_COMPONENT_COMMANDS )
      {
         log_line("Received reboot request from RX Commands. Sending it to telemetry.");
         ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)pPH, pPH->total_length);
      }
      else
         log_line("Received reboot request.");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }


   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_SIGNAL_VIDEO_ENCODINGS_CHANGED )
   {
      if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, false) )
         log_error_and_alarm("Can't load current model vehicle.");
      process_data_tx_video_signal_encoding_changed();
      s_InputBufferVideoBytesRead = 0;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_CAMERA_PARAM )
   {
      log_line("Router received message to change camera params.");
      send_control_message_to_raspivid( (pPH->stream_packet_idx) & 0xFF, (((pPH->stream_packet_idx)>>8) & 0xFF) );
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS )
   {
      memcpy((u8*)(&(g_pCurrentModel->m_Stats)),(u8*)(((u8*)pPH) + sizeof(t_packet_header)), sizeof(type_vehicle_stats_info));
      // Signal other components about the vehicle stats
      if ( pPH->vehicle_id_src == PACKET_COMPONENT_TELEMETRY )
         ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)pPH, pPH->total_length);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ1_UP )
   {
      log_line("Received request to change frequency up on radio link 1 using a RC channel");
      if ( g_pCurrentModel->functions_params.bEnableRCTriggerFreqSwitchLink1 )
      if ( g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink1 >= 0 )
      {
         u32 freq = _get_previous_frequency_switch(0);
         char szFreq2[64];
         strcpy(szFreq2, str_format_frequency(freq));
         log_line("Switching (using RC switch) radio link 1 from %s to %s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[0]), szFreq2 );
         if ( freq != g_pCurrentModel->radioLinksParams.link_frequency_khz[0] )
         {
            s_iPendingFrequencyChangeLinkId = 0;
            s_uPendingFrequencyChangeTo = freq;
            s_uTimeFrequencyChangeRequest = g_TimeNow;
         }
      }
   }
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ1_DOWN )
   {
      log_line("Received request to change frequency down on radio link 1 using a RC channel");
      if ( g_pCurrentModel->functions_params.bEnableRCTriggerFreqSwitchLink1 )
      if ( g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink1 >= 0 )
      {
         u32 freq = _get_next_frequency_switch(0);
         char szFreq2[64];
         strcpy(szFreq2, str_format_frequency(freq));
         log_line("Switching (using RC switch) radio link 1 from %s to %s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[0]), szFreq2 );
         if ( freq != g_pCurrentModel->radioLinksParams.link_frequency_khz[0] )
         {
            s_iPendingFrequencyChangeLinkId = 0;
            s_uPendingFrequencyChangeTo = freq;
            s_uTimeFrequencyChangeRequest = g_TimeNow;
         }
      }
      return;
   }
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ2_UP )
   {
      log_line("Received request to change frequency up on radio link 2 using a RC channel");
      if ( g_pCurrentModel->functions_params.bEnableRCTriggerFreqSwitchLink2 )
      if ( g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink2 >= 0 )
      {
         u32 freq = _get_previous_frequency_switch(1);
         char szFreq2[64];
         strcpy(szFreq2, str_format_frequency(freq));
         log_line("Switching (using RC switch) radio link 2 from %s to %s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[1]), szFreq2 );
         if ( freq != g_pCurrentModel->radioLinksParams.link_frequency_khz[1] )
         {
            s_iPendingFrequencyChangeLinkId = 1;
            s_uPendingFrequencyChangeTo = freq;
            s_uTimeFrequencyChangeRequest = g_TimeNow;
         }
      }
   }
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ2_DOWN )
   {
      log_line("Received local request to change frequency down on radio link 2 using a RC channel");
      if ( g_pCurrentModel->functions_params.bEnableRCTriggerFreqSwitchLink2 )
      if ( g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink2 >= 0 )
      {
         u32 freq = _get_next_frequency_switch(1);
         char szFreq2[64];
         strcpy(szFreq2, str_format_frequency(freq));
         log_line("Switching (using RC switch) radio link 2 from %s to %s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[2]), szFreq2 );
         if ( freq != g_pCurrentModel->radioLinksParams.link_frequency_khz[2] )
         {
            s_iPendingFrequencyChangeLinkId = 1;
            s_uPendingFrequencyChangeTo = freq;
            s_uTimeFrequencyChangeRequest = g_TimeNow;
         }
      }
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_APPLY_SIK_PARAMS )
   {
      log_line("Received local request to apply all SiK radio parameters.");

   }
}


