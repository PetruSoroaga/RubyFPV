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
#include "../base/encr.h"
#include "../base/shared_mem.h"
#include "../base/ruby_ipc.h"
#include "../base/hw_procs.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/hardware_serial.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "process_local_packets.h"
#include "processor_tx_video.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/fec.h" 
#include "radio_utils.h"
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
#include "relay_rx.h"


u32 _get_previous_frequency_switch(int nLink)
{
   u32 nCurrentFreq = g_pCurrentModel->radioLinksParams.link_frequency[nLink];
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
   u32 nCurrentFreq = g_pCurrentModel->radioLinksParams.link_frequency[nLink];
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

void process_local_control_packet(t_packet_header* pPH)
{
   if ( NULL != g_pProcessStats )
   {
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
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
      log_line("Received local message to change Sik Radio (radio interface %d) baud rate (Must communicate at: %d bps).", iRadioIndex+1, iBaudRate);

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

      if ( g_bHasSikConfigCommandInProgress )
      {
         log_softerror_and_alarm("Another SiK configuration is in progress. Ignoring this one.");
         return;
      }

      g_bHasSikConfigCommandInProgress = true;
      g_uTimeStartSiKConfigCommand = g_TimeNow;
      
      char szCommand[256];
      snprintf(szCommand, sizeof(szCommand), "rm -rf %s", FILE_TMP_SIK_CONFIG_FINISHED);
      hw_execute_bash_command(szCommand, NULL);

      snprintf(szCommand, sizeof(szCommand), "./ruby_sik_config %s %d -serialspeed %d &", pRadioHWInfo->szDriver, iBaudRate, (int)pSerialPort->lPortSpeed);
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
         if ( g_pCurrentModel->isCameraHDMI() )
            hardware_sleep_ms(800);
         vehicle_launch_video_capture(g_pCurrentModel, &(g_SM_VideoLinkStats.overwrites));
         vehicle_check_update_processes_affinities();
      } 
      return;
   }

   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM )
   {
      log_line("Received controll message to restart video capture program. Parameter: %u", pPH->vehicle_id_dest);
      if ( g_pCurrentModel->hasCamera() )
      {
         vehicle_stop_video_capture(g_pCurrentModel); 
         if ( g_pCurrentModel->isCameraHDMI() )
            hardware_sleep_ms(800);
         vehicle_launch_video_capture(g_pCurrentModel, &(g_SM_VideoLinkStats.overwrites));
         vehicle_check_update_processes_affinities();
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
      u8 changeType = (pPH->vehicle_id_src >> 8 ) & 0xFF;
      u8 fromComponentId = (pPH->vehicle_id_src & 0xFF);
      log_line("Received local event to reload model. Change type: %d, from component id: %d", (int)changeType, (int)fromComponentId);
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

      type_radio_links_parameters oldRadioLinksParams;
      audio_parameters_t oldAudioParams;
      type_relay_parameters oldRelayParams;

      memcpy(&oldRadioLinksParams, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
      memcpy(&oldAudioParams, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t));
      memcpy(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
      u32 old_ef = g_pCurrentModel->enc_flags;
      //bool bOldUseAdaptiveVideo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?true:false;
      //u8 uOldUserSelectedVideoProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
      bool bMustSignalOtherComponents = true;
      bool bMustReinitVideo = true;

      // Reload model first

      if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, false) )
         log_error_and_alarm("Can't load current model vehicle.");

      if ( changeType == MODEL_CHANGED_CONTROLLER_TELEMETRY )
      {
         return;
      }

      if ( changeType == MODEL_CHANGED_SWAPED_RADIO_INTERFACES )
      {
         log_line("Received notification to reload model due to radio interfaces swaped.");
         close_radio_interfaces();

         if ( NULL != g_pProcessStats )
         {
            g_TimeNow = get_current_timestamp_ms();
            g_pProcessStats->lastActiveTime = g_TimeNow;
            g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
         }

         configure_radio_interfaces_for_current_model(g_pCurrentModel);

         open_radio_interfaces();

         if ( NULL != g_pProcessStats )
         {
            g_TimeNow = get_current_timestamp_ms();
            g_pProcessStats->lastActiveTime = g_TimeNow;
            g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
         }

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
            {
               log_line("Changed relay mode from %u to %u.", uOldRelayMode, g_pCurrentModel->relay_params.uCurrentRelayMode);
               relay_on_relay_mode_changed(g_pCurrentModel->relay_params.uCurrentRelayMode);
            }
            else
               log_line("No change in relay flags or relay mode detected. Ignoring notification.");
            return;
         }

         oldRelayParams.uCurrentRelayMode = uOldRelayMode;
         
         u32 uOldRelayFlags = oldRelayParams.uRelayFlags;
         oldRelayParams.uRelayFlags = g_pCurrentModel->relay_params.uRelayFlags;

         // Only relay flags changed?
         if ( 0 == memcmp(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters)) )
         {
            if ( uOldRelayFlags != g_pCurrentModel->relay_params.uRelayFlags )
            {
               log_line("Changed relay flags from %u to %u.", uOldRelayFlags, g_pCurrentModel->relay_params.uRelayFlags);
               relay_on_relay_flags_changed(g_pCurrentModel->relay_params.uRelayFlags);
            }
            else
               log_line("No change in relay flags or relay mode detected. Ignoring notification.");
            return;
         }
         oldRelayParams.uRelayFlags = uOldRelayFlags;
         
         if ( oldRelayParams.uRelayVehicleId != g_pCurrentModel->relay_params.uRelayVehicleId )
         {
            log_line("Changed relayed vehicle id, from %u to %u.", oldRelayParams.uRelayVehicleId, g_pCurrentModel->relay_params.uRelayVehicleId);
            radio_stats_reset_streams_rx_history(&g_SM_RadioStats, g_pCurrentModel->vehicle_id);
         }
         g_TimeLastNotificationRelayParamsChanged = g_TimeNow;
         return;
      }
      if ( changeType == MODEL_CHANGED_VIDEO_H264_QUANTIZATION )
      {
         int iQuant = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization;
         log_line("Received local notification that h264 quantization changed. New value: %d", iQuant);
         
         bool bUseAdaptiveVideo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?true:false;
         if ( ! bUseAdaptiveVideo )
         {
            if ( iQuant > 0 )
            {
               g_SM_VideoLinkStats.overwrites.currentH264QUantization = (u8)iQuant;
               video_link_set_last_quantization_set(g_SM_VideoLinkStats.overwrites.currentH264QUantization);
               send_control_message_to_raspivid( RASPIVID_COMMAND_ID_QUANTIZATION_INIT, g_SM_VideoLinkStats.overwrites.currentH264QUantization );
               send_control_message_to_raspivid( RASPIVID_COMMAND_ID_QUANTIZATION_MIN, g_SM_VideoLinkStats.overwrites.currentH264QUantization );
            }
            else if ( g_pCurrentModel->hasCamera() )
            {
               g_SM_VideoLinkStats.overwrites.currentH264QUantization = 0;
               vehicle_stop_video_capture(g_pCurrentModel); 
               if ( g_pCurrentModel->isCameraHDMI() )
                  hardware_sleep_ms(800);
               vehicle_launch_video_capture(g_pCurrentModel, &(g_SM_VideoLinkStats.overwrites));
               vehicle_check_update_processes_affinities();
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
         log_line("Received local notification that default max adaptive keyframe interval changed. New value: %u ms", g_pCurrentModel->video_params.uMaxAutoKeyframeInterval);
         if ( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe > 0 )
            log_line("Current video profile %s is on fixed keyframe: %d. Nothing to do.", str_get_video_profile_name(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile), g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe);
         else
         {
            log_line("Current video profile %s is on auto keyframe. Set new default auto max keyframe value.", str_get_video_profile_name(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile) );
            video_link_auto_keyframe_init();
         }
         return;
      }

      if ( changeType == MODEL_CHANGED_VIDEO_KEYFRAME )
      {
         int keyframe = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe;
         log_line("Received local notification that video keyframe interval changed. New value: %d", keyframe);
         if ( keyframe > 0 )
         {
            log_line("Current video profile %s is on fixed keyframe, new value: %d. Adjust now the video capture keyframe param.", str_get_video_profile_name(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile), keyframe);  
            process_data_tx_video_set_current_keyframe_interval(keyframe);
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
         log_line("Received local notification (type %d) that some params changed that do not require additional processing besides reloading the current model.", (int)changeType);
         bMustSignalOtherComponents = false;
         bMustReinitVideo = false;
         return;
      }

      if ( changeType == MODEL_CHANGED_RADIO_POWERS )
      {
         log_line("Received local notification that radio powers have changed.");
         bMustReinitVideo = false;
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

      if ( changeType == MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK0 ||
           changeType == MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK1 ||
           changeType == MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK2 ||
           changeType == MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK3 )
      {
         int iLink = 0;
         if ( changeType == MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK0 )
            log_line("Received local notification that radio flags or datarates changed on radio link 1. Update radio frames type right away.");
         else if ( changeType == MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK1 )
         {
            log_line("Received local notification that radio flags or datarates changed on radio link 2. Update radio frames type right away.");
            iLink = 1;
         }
         else if ( changeType == MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK2 )
         {
            log_line("Received local notification that radio flags or datarates changed on radio link 3. Update radio frames type right away.");
            iLink = 2;
         }
         else if ( changeType == MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK3 )
         {
            log_line("Received local notification that radio flags or datarates changed on radio link 4. Update radio frames type right away.");
            iLink = 3;
         }
         log_line("Radio link %d datarates now: %d/%d", iLink+1, g_pCurrentModel->radioLinksParams.link_datarates[iLink][0], g_pCurrentModel->radioLinksParams.link_datarates[iLink][1]);
         u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[iLink];
         if ( ! (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE) )
         {
            radioFlags &= ~(RADIO_FLAGS_STBC | RADIO_FLAGS_LDPC | RADIO_FLAGS_SHORT_GI | RADIO_FLAGS_HT40);
            radioFlags |= RADIO_FLAGS_HT20;
         }
         radio_set_frames_flags(radioFlags);
         bMustSignalOtherComponents = false;
         bMustReinitVideo = false;
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
               g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
            }
         }
         bMustSignalOtherComponents = false;
         bMustReinitVideo = false;
      }

      if ( bMustReinitVideo )
         process_data_tx_video_signal_model_changed();

      if ( 0 != memcmp(&oldAudioParams, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t)) )
      {
         if ( -1 != s_fInputAudioStream )
            close(s_fInputAudioStream);
         s_fInputAudioStream = -1;
         if ( g_pCurrentModel->audio_params.has_audio_device && g_pCurrentModel->audio_params.enabled )
         {
            log_line("Opening audio input stream: %s", FIFO_RUBY_AUDIO1);
            s_fInputAudioStream = open(FIFO_RUBY_AUDIO1, O_RDONLY);
            if ( s_fInputAudioStream < 0 )
               log_softerror_and_alarm("Failed to open audio input stream: %s", FIFO_RUBY_AUDIO1);
            else
               log_line("Opened audio input stream: %s", FIFO_RUBY_AUDIO1);
         }
         else
         {
            log_line("Vehicle with no audio capture device or audio is disabled on the vehicle. Do not try to read audio stream.");
            s_fInputAudioStream = -1;
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
         log_line("Switching (using RC switch) radio link 1 from %s to %s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[0]), szFreq2 );
         if ( freq != g_pCurrentModel->radioLinksParams.link_frequency[0] )
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
         log_line("Switching (using RC switch) radio link 1 from %s to %s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[0]), szFreq2 );
         if ( freq != g_pCurrentModel->radioLinksParams.link_frequency[0] )
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
         log_line("Switching (using RC switch) radio link 2 from %s to %s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[1]), szFreq2 );
         if ( freq != g_pCurrentModel->radioLinksParams.link_frequency[1] )
         {
            s_iPendingFrequencyChangeLinkId = 1;
            s_uPendingFrequencyChangeTo = freq;
            s_uTimeFrequencyChangeRequest = g_TimeNow;
         }
      }
   }
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ2_DOWN )
   {
      log_line("Received request to change frequency down on radio link 2 using a RC channel");
      if ( g_pCurrentModel->functions_params.bEnableRCTriggerFreqSwitchLink2 )
      if ( g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink2 >= 0 )
      {
         u32 freq = _get_next_frequency_switch(1);
         char szFreq2[64];
         strcpy(szFreq2, str_format_frequency(freq));
         log_line("Switching (using RC switch) radio link 2 from %s to %s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[1]), szFreq2 );
         if ( freq != g_pCurrentModel->radioLinksParams.link_frequency[2] )
         {
            s_iPendingFrequencyChangeLinkId = 1;
            s_uPendingFrequencyChangeTo = freq;
            s_uTimeFrequencyChangeRequest = g_TimeNow;
         }
      }
      return;
   }
}

void process_local_control_packets()
{
   while ( packets_queue_has_packets(&s_QueueControlPackets) )
   {
      int length = -1;
      u8* pBuffer = packets_queue_pop_packet(&s_QueueControlPackets, &length);
      if ( NULL == pBuffer || -1 == length )
         break;

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      t_packet_header* pPH = (t_packet_header*)pBuffer;
      process_local_control_packet(pPH);
   }
}

