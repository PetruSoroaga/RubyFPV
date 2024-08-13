/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
         * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
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
#include "../base/shared_mem.h"
#include "../base/ruby_ipc.h"
#include "../base/utils.h"
#include "../common/string_utils.h"

#include "video_link_stats_overwrites.h"
#include "video_link_check_bitrate.h"
#include "video_link_auto_keyframe.h"
#include "processor_tx_video.h"
#include "ruby_rt_vehicle.h"
#include "utils_vehicle.h"
#include "packets_utils.h"
#include "video_source_csi.h"
#include "video_source_majestic.h"
#include "events.h"
#include "timers.h"
#include "shared_vars.h"


bool s_bUseControllerInfo = false;
int  s_iTargetShiftLevel = 0;
int  s_iTargetProfile = 0;
int  s_iMaxLevelShiftForCurrentProfile = 0;
float s_fParamsChangeStrength = 0.0; // 0...1

u32 s_TimeLastHistorySwitchUpdate = 0;
u32 s_uTimeLastShiftLevelDown = 0;
int s_iLastTotalLevelsShift = 0;
int s_iLastTargetVideoFPS = 0;

u32 s_uTimeStartGoodIntervalForProfileShiftUp = 0;


void _video_overwrites_set_capture_video_bitrate(u32 uBitrateBPS, bool bIsInitialValue, int iReason)
{
   if ( g_pCurrentModel->hasCamera() )
   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      video_source_csi_send_control_message(RASPIVID_COMMAND_ID_VIDEO_BITRATE, uBitrateBPS/100000);
   
   if ( g_pCurrentModel->hasCamera() )
   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      video_source_majestic_set_videobitrate_value(uBitrateBPS);
   
   if ( NULL != g_pProcessorTxVideo )
      g_pProcessorTxVideo->setLastSetCaptureVideoBitrate(uBitrateBPS, bIsInitialValue, 1);
}


// Needs to be sent to other processes when we change the currently selected video profile

void _video_stats_overwrites_signal_changes()
{
   //log_line("[Video Link Overwrites]: signal others that currently active video profile changed, to reload model");

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_UPDATED_VIDEO_LINK_OVERWRITES, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.total_length = sizeof(t_packet_header) + sizeof(shared_mem_video_link_overwrites);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer + sizeof(t_packet_header), (u8*)&(g_SM_VideoLinkStats.overwrites), sizeof(shared_mem_video_link_overwrites));
   
   ruby_ipc_channel_send_message(s_fIPCRouterToCommands, buffer, PH.total_length);
   
   if ( NULL != g_pProcessStats )
   {
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }
}

void _video_stats_overwrites_signal_video_prifle_changed()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_VIDEO_PROFILE_SWITCHED, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = (u32)g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile;
   PH.total_length = sizeof(t_packet_header);

   if ( 0 != s_fIPCRouterToTelemetry )
      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}  

void video_stats_overwrites_init()
{
   log_line("[Video Link Overwrites]: Init and reset structures."); 
   memset((u8*)&g_SM_VideoLinkStats, 0, sizeof(shared_mem_video_link_stats_and_overwrites));
   memset((u8*)&g_SM_VideoLinkGraphs, 0, sizeof(shared_mem_video_link_graphs));

   if ( NULL == g_pCurrentModel )
      return;

   g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].width = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width;
   g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].height = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height;
   g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].fps = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps;
   g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].width = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width;
   g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].height = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height;

   video_overwrites_init(&(g_SM_VideoLinkStats.overwrites), g_pCurrentModel);
   onEventBeforeRuntimeCurrentVideoProfileChanged(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile, g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile);

   video_link_reset_overflow_quantization_value();

   g_SM_VideoLinkStats.backIntervalsToLookForDownStep = DEFAULT_MINIMUM_INTERVALS_FOR_VIDEO_LINK_ADJUSTMENT;
   g_SM_VideoLinkStats.backIntervalsToLookForUpStep = 2*DEFAULT_MINIMUM_INTERVALS_FOR_VIDEO_LINK_ADJUSTMENT;

   g_SM_VideoLinkStats.timeLastStatsCheck = 0;
   g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown = 0;
   g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp = 0;
   g_SM_VideoLinkStats.timeLastProfileChangeDown = 0;
   g_SM_VideoLinkStats.timeLastProfileChangeUp = 0;
   g_SM_VideoLinkStats.timeLastReceivedControllerLinkInfo = 0;

   g_SM_VideoLinkStats.usedControllerInfo = 0;
   g_SM_VideoLinkStats.computed_min_interval_for_change_up = 500;
   g_SM_VideoLinkStats.computed_min_interval_for_change_down = 200;
   g_SM_VideoLinkStats.computed_rx_quality_vehicle = 0;
   g_SM_VideoLinkStats.computed_rx_quality_controller = 0;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_SWITCHES; i++ )
      g_SM_VideoLinkStats.historySwitches[i] = g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel | (g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile<<4);
   g_SM_VideoLinkStats.totalSwitches = 0;

   s_TimeLastHistorySwitchUpdate = g_TimeNow;

   g_SM_VideoLinkGraphs.timeLastStatsUpdate = 0;
   g_SM_VideoLinkGraphs.tmp_vehileReceivedRetransmissionsRequestsCount = 0;
   g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPackets = 0;
   g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPacketsRetried = 0;

   for( int i=0; i<MAX_INTERVALS_VIDEO_LINK_STATS; i++ )
   {
      g_SM_VideoLinkGraphs.vehicleRXQuality[i] = 255;
      g_SM_VideoLinkGraphs.vehicleRXMaxTimeGap[i] = 255;
      g_SM_VideoLinkGraphs.vehileReceivedRetransmissionsRequestsCount[i] = 0;
      g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPackets[i] = 0;
      g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPacketsRetried[i] = 0;
   }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   for( int k=0; k<MAX_INTERVALS_VIDEO_LINK_STATS; k++ )
      g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[i][k] = 255;

   for( int i=0; i<MAX_VIDEO_STREAMS; i++ )
   for( int k=0; k<MAX_INTERVALS_VIDEO_LINK_STATS; k++ )
   {
      g_SM_VideoLinkGraphs.controller_received_radio_streams_rx_quality[i][k] = 255;
      g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[i][k] = 255;
      g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[i][k] = 255;
      g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[i][k] = 255;
      g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[i][k] = 255;
   }

   g_PD_LastRecvControllerLinksStats.lastUpdateTime = 0;
   g_PD_LastRecvControllerLinksStats.radio_interfaces_count = 1;
   g_PD_LastRecvControllerLinksStats.video_streams_count = 1;

   s_uTimeLastShiftLevelDown = 0;
   s_iLastTotalLevelsShift = 0;

   log_line("[Video Link Overwrites]: Init video links stats and overwrites structure: Done. Structure size: %d bytes", sizeof(shared_mem_video_link_stats_and_overwrites));
}

u32 _get_bitrate_for_current_level_and_profile_including_overwrites()
{
   u32 uTargetVideoBitrate = utils_get_max_allowed_video_bitrate_for_profile_and_level(g_pCurrentModel, g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile, (int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel);

   u32 uOverwriteDown = g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile];
   if ( uTargetVideoBitrate > uOverwriteDown )
      uTargetVideoBitrate -= uOverwriteDown;
   else
      uTargetVideoBitrate = 0;

   if ( uTargetVideoBitrate < 250000 )
      uTargetVideoBitrate = 250000;

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_USER )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_BEST_PERF )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_HIGH_QUALITY )
   if ( uTargetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      uTargetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   // Minimum for MQ profile, for 12 Mb datarate is at least 2Mb, do not go below that
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
   if ( g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_video_bps == 0 )
   if ( getRealDataRateFromRadioDataRate(g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].radio_datarate_video_bps, 0) >= 12000000 )
   if ( uTargetVideoBitrate < 2000000 )
      uTargetVideoBitrate = 2000000;
     
   return uTargetVideoBitrate;
}

void video_stats_overwrites_switch_to_profile_and_level(int iTotalLevelsShift, int iVideoProfile, int iLevelShift)
{
   onEventBeforeRuntimeCurrentVideoProfileChanged(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile, iVideoProfile);
   
   if ( iTotalLevelsShift > s_iLastTotalLevelsShift )
      s_uTimeLastShiftLevelDown = g_TimeNow;
   s_iLastTotalLevelsShift = iTotalLevelsShift;

   bool bVideoProfileChanged = false;

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != iVideoProfile )
      bVideoProfileChanged = true;

   g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile = iVideoProfile;
   g_SM_VideoLinkStats.overwrites.uTimeSetCurrentVideoLinkProfile = g_TimeNow;
   g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel = iLevelShift;
   g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate = utils_get_max_allowed_video_bitrate_for_profile_or_user_video_bitrate( g_pCurrentModel, g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile);

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
      g_SM_VideoLinkStats.overwrites.hasEverSwitchedToLQMode = 1;

   if ( 0 == g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate )
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
         g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps;
   }
   if ( 0 == g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate )
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ ||
           g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
         g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps;
   }
   g_SM_VideoLinkStats.overwrites.currentProfileAndLevelDefaultBitrate = utils_get_max_allowed_video_bitrate_for_profile_and_level(g_pCurrentModel, g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile, (int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel);

   int iData = 0;
   int iEC = 0;
   g_pCurrentModel->get_level_shift_ec_scheme(iTotalLevelsShift, &iData, &iEC);

   g_SM_VideoLinkStats.overwrites.currentDataBlocks = iData;
   g_SM_VideoLinkStats.overwrites.currentECBlocks = iEC;

   g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile_including_overwrites();

   g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].width = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width;
   g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].height = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height;
   
   // Do not use more video bitrate than the user set originaly in the user profile

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ || g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
   if ( g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate > g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps )
   {
      g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps;
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile_including_overwrites();
   }

   if ( g_SM_VideoLinkStats.overwrites.currentECBlocks > MAX_FECS_PACKETS_IN_BLOCK )
      g_SM_VideoLinkStats.overwrites.currentECBlocks = MAX_FECS_PACKETS_IN_BLOCK;

   g_SM_VideoLinkStats.historySwitches[0] = g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel | (g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile<<4);
   g_SM_VideoLinkStats.totalSwitches++;

   g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown = g_TimeNow;
   g_SM_VideoLinkStats.timeLastProfileChangeDown = g_TimeNow;
   g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp = g_TimeNow;
   g_SM_VideoLinkStats.timeLastProfileChangeUp = g_TimeNow;
   
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_USER )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_HIGH_QUALITY )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_BEST_PERF )
   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;
   
   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < 200000 )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = 200000;
   
   // If video profile changed (so did the target video bitrate):
   // Send default video quantization param to raspivid based on target video bitrate
   if ( bVideoProfileChanged )
   if ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION )
   {
      g_SM_VideoLinkStats.overwrites.currentH264QUantization = 0;
      video_link_quantization_shift(2);
   }

   _video_overwrites_set_capture_video_bitrate(g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate, false, 1);
   
   // Prevent video bitrate changes due to overload or underload right after a profile change;
   // (A profile change usually sets a new, different video bitrate, so ignore this delta for a while)

   g_TimeLastOverwriteBitrateDownOnTxOverload = g_TimeNow;
   g_TimeLastOverwriteBitrateUpOnTxOverload = g_TimeNow;

   // Signal other components (rx_command) to refresh their copy of current video overwrites structure

   _video_stats_overwrites_signal_changes();
   _video_stats_overwrites_signal_video_prifle_changed();

   // Signal tx video that encoding scheme changed

   process_data_tx_video_signal_encoding_changed();

}

void _video_stats_overwrites_apply_profile_changes(bool bDownDirection)
{
   onEventBeforeRuntimeCurrentVideoProfileChanged(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile, s_iTargetProfile);

   g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile = s_iTargetProfile;
   g_SM_VideoLinkStats.overwrites.uTimeSetCurrentVideoLinkProfile = g_TimeNow;
   g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel = s_iTargetShiftLevel;
   g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate = utils_get_max_allowed_video_bitrate_for_profile_or_user_video_bitrate(g_pCurrentModel, g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile);

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
      g_SM_VideoLinkStats.overwrites.hasEverSwitchedToLQMode = 1;

   if ( 0 == g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate )
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
         g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps;
   }
   if ( 0 == g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate )
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ ||
           g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
         g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps;
   }
   g_SM_VideoLinkStats.overwrites.currentProfileAndLevelDefaultBitrate = utils_get_max_allowed_video_bitrate_for_profile_and_level(g_pCurrentModel, g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile, (int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel);

   g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile_including_overwrites();
   g_SM_VideoLinkStats.overwrites.currentDataBlocks = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_packets;
   g_SM_VideoLinkStats.overwrites.currentECBlocks = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs;


   g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].width = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width;
   g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].height = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height;
   
   // Do not use more video bitrate than the user set originaly in the user profile

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ || g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
   if ( g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate > g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps )
   {
      g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps;
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile_including_overwrites();
   }

   if ( g_SM_VideoLinkStats.overwrites.currentECBlocks > MAX_FECS_PACKETS_IN_BLOCK )
      g_SM_VideoLinkStats.overwrites.currentECBlocks = MAX_FECS_PACKETS_IN_BLOCK;

   g_SM_VideoLinkStats.historySwitches[0] = g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel | (g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile<<4);
   g_SM_VideoLinkStats.totalSwitches++;

   if ( bDownDirection )
   {
      g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown = g_TimeNow;
      g_SM_VideoLinkStats.timeLastProfileChangeDown = g_TimeNow;
   }
   else
   {
      g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp = g_TimeNow;
      g_SM_VideoLinkStats.timeLastProfileChangeUp = g_TimeNow;
   }

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_USER )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_HIGH_QUALITY )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_BEST_PERF )
   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < 200000 )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = 200000;

   _video_overwrites_set_capture_video_bitrate(g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate, false, 2);
         
   int nTargetFPS = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].fps;
   if ( 0 == nTargetFPS )
      nTargetFPS = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps;

   if ( s_iLastTargetVideoFPS != nTargetFPS )
   {
      s_iLastTargetVideoFPS = nTargetFPS;
      
      log_line("[Video Link Overwrites]: Setting the video capture FPS rate to: %d", nTargetFPS);
      if ( g_pCurrentModel->hasCamera() )
      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
         video_source_csi_send_control_message(RASPIVID_COMMAND_ID_FPS, nTargetFPS);
   }
   
   // Prevent video bitrate changes due to overload or underload right after a profile change;
   // (A profile change usually sets a new, different video bitrate, so ignore this delta for a while)

   g_TimeLastOverwriteBitrateDownOnTxOverload = g_TimeNow;
   g_TimeLastOverwriteBitrateUpOnTxOverload = g_TimeNow;

   // Signal other components (rx_command) to refresh their copy of current video overwrites structure

   _video_stats_overwrites_signal_changes();
   _video_stats_overwrites_signal_video_prifle_changed();

   // Signal tx video that encoding scheme changed

   process_data_tx_video_signal_encoding_changed();
}

void _video_stats_overwrites_apply_ec_bitrate_changes(bool bDownDirection)
{
   if ( bDownDirection )
      g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown = g_TimeNow;
   else
      g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp = g_TimeNow;

   g_SM_VideoLinkStats.historySwitches[0] = g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel | (g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile<<4);
   g_SM_VideoLinkStats.totalSwitches++;

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_USER )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_HIGH_QUALITY )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_BEST_PERF )
   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < 200000 )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = 200000;

   _video_overwrites_set_capture_video_bitrate(g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate, false, 3);
         
   // Signal other components (rx_command) to refresh their copy of current video overwrites structure

   _video_stats_overwrites_signal_changes();
   _video_stats_overwrites_signal_video_prifle_changed();

   // Signal tx video that encoding scheme changed

   process_data_tx_video_signal_encoding_changed();
}

void video_stats_overwrites_reset_to_highest_level()
{
   log_line("[Video Link Overwrites]: Reset to highest level (user profile, no adjustment)."); 

   s_iTargetProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
   s_iTargetShiftLevel = 0;

   g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_pCurrentModel->video_params.user_selected_video_link_profile] = 0;
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[i] = 0;
   _video_stats_overwrites_apply_profile_changes(false);

   video_link_reset_overflow_quantization_value();
}

void video_stats_overwrites_reset_to_forced_profile()
{
   if ( g_iForcedVideoProfile == -1 )
      return;
   s_iTargetProfile = g_iForcedVideoProfile;
   s_iTargetShiftLevel = 0;
   s_uTimeStartGoodIntervalForProfileShiftUp = 0;
   _video_stats_overwrites_apply_profile_changes(true);
}

void _video_stats_overwrites_apply_changes()
{
   bool bTriedAnyShift = false;

   if ( s_iTargetProfile == g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
   if ( s_iTargetShiftLevel > (int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel )
   {
      bTriedAnyShift = true;
      s_uTimeStartGoodIntervalForProfileShiftUp = 0;

      if ( (g_iForcedVideoProfile != -1) && s_iTargetShiftLevel > s_iMaxLevelShiftForCurrentProfile )
         s_iTargetShiftLevel = s_iMaxLevelShiftForCurrentProfile;

      if ( s_iTargetShiftLevel > s_iMaxLevelShiftForCurrentProfile )
      {
          if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_SM_VideoLinkStats.overwrites.userVideoLinkProfile )
          {
             s_iTargetProfile = VIDEO_PROFILE_MQ;
             s_iTargetShiftLevel = 0;
          }
          else if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
          {
             s_iTargetProfile = VIDEO_PROFILE_LQ;
             s_iTargetShiftLevel = 0;
          }
          else
          {
             // Reached bottom profile, can't go lower
             s_iTargetShiftLevel = s_iMaxLevelShiftForCurrentProfile;
          }
      }

      if ( s_iTargetProfile != g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
      {
         _video_stats_overwrites_apply_profile_changes(true);
      }
      else if ( s_iTargetShiftLevel != (int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel )
      {
         g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel = s_iTargetShiftLevel;
         g_SM_VideoLinkStats.overwrites.currentECBlocks = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs;
         g_SM_VideoLinkStats.overwrites.currentECBlocks += g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel;

         if ( g_SM_VideoLinkStats.overwrites.currentECBlocks > MAX_FECS_PACKETS_IN_BLOCK )
            g_SM_VideoLinkStats.overwrites.currentECBlocks = MAX_FECS_PACKETS_IN_BLOCK;
         if ( g_SM_VideoLinkStats.overwrites.currentECBlocks < g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs )
            g_SM_VideoLinkStats.overwrites.currentECBlocks = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs;
         if ( g_SM_VideoLinkStats.overwrites.currentECBlocks > 3*g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_packets )
            g_SM_VideoLinkStats.overwrites.currentECBlocks = 3*g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_packets;

         g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile_including_overwrites();

         _video_stats_overwrites_apply_ec_bitrate_changes(true);
      }
      return;
   }

   if ( s_iTargetProfile == g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
   if ( s_iTargetShiftLevel < (int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel )
   {
      bTriedAnyShift = true;

      if ( (g_iForcedVideoProfile != -1) && s_iTargetShiftLevel < 0 )
         s_iTargetShiftLevel = 0;

      if ( s_iTargetShiftLevel < 0 )
      {
          if ( 0 == s_uTimeStartGoodIntervalForProfileShiftUp )
             s_uTimeStartGoodIntervalForProfileShiftUp = g_TimeNow;
          else if ( g_TimeNow >= s_uTimeStartGoodIntervalForProfileShiftUp + DEFAULT_MINIMUM_OK_INTERVAL_MS_TO_SWITCH_VIDEO_PROFILE_UP )
          {
             if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
             {
                s_iTargetProfile = VIDEO_PROFILE_MQ;
                s_iTargetShiftLevel = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].block_packets - g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].block_fecs;
             }
             else if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
             {
                s_iTargetProfile = g_SM_VideoLinkStats.overwrites.userVideoLinkProfile;
                s_iTargetShiftLevel = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].block_packets - g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].block_fecs;

             }
             else
             {
                // Reached top profile, can't go higher
                s_iTargetShiftLevel = 0;
             }
             s_uTimeStartGoodIntervalForProfileShiftUp = 0;
          }
      }

      if ( s_iTargetProfile != g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
      {
         _video_stats_overwrites_apply_profile_changes(false);
      }
      else if ( s_iTargetShiftLevel != (int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel )
      {
         if ( s_iTargetShiftLevel < 0 )
            s_iTargetShiftLevel = 0;

         g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel = s_iTargetShiftLevel;
         g_SM_VideoLinkStats.overwrites.currentECBlocks = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs;
         g_SM_VideoLinkStats.overwrites.currentECBlocks += g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel;

         if ( g_SM_VideoLinkStats.overwrites.currentECBlocks > MAX_FECS_PACKETS_IN_BLOCK )
            g_SM_VideoLinkStats.overwrites.currentECBlocks = MAX_FECS_PACKETS_IN_BLOCK;
         if ( g_SM_VideoLinkStats.overwrites.currentECBlocks < g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs )
            g_SM_VideoLinkStats.overwrites.currentECBlocks = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs;

         g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile_including_overwrites();

         _video_stats_overwrites_apply_ec_bitrate_changes(false);
      }
      return;
   }

   if ( ! bTriedAnyShift )
      s_uTimeStartGoodIntervalForProfileShiftUp = 0;
}

void _check_video_link_params_using_vehicle_info(bool bDownDirection)
{
   int vehicleMinRXQuality = 255;
   int vehicleAvgRXQuality = 0;
   int iCountRetransmissions = 0;
   int iCountPacketsRetried = 0;
   int iCountIntervalsWithRetires = 0;

   int iIntervals = g_SM_VideoLinkStats.backIntervalsToLookForUpStep;
   if ( bDownDirection )
      iIntervals = g_SM_VideoLinkStats.backIntervalsToLookForDownStep;
   if ( iIntervals > MAX_INTERVALS_VIDEO_LINK_STATS )
      iIntervals = MAX_INTERVALS_VIDEO_LINK_STATS;

   for( int i=0; i<iIntervals; i++ )
   {
      if ( 255 != g_SM_VideoLinkGraphs.vehicleRXQuality[i] )
      {
         vehicleAvgRXQuality += g_SM_VideoLinkGraphs.vehicleRXQuality[i];
         if ( g_SM_VideoLinkGraphs.vehicleRXQuality[i] < vehicleMinRXQuality )
            vehicleMinRXQuality = g_SM_VideoLinkGraphs.vehicleRXQuality[i];
      }
      if ( 255 != g_SM_VideoLinkGraphs.vehileReceivedRetransmissionsRequestsCount[i] )
         iCountRetransmissions += g_SM_VideoLinkGraphs.vehileReceivedRetransmissionsRequestsCount[i];
      if ( 255 != g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPacketsRetried[i] )
      if ( g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPacketsRetried[i] > 0 )
      {
         iCountPacketsRetried += g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPacketsRetried[i];
         iCountIntervalsWithRetires++;
      }
   }

   if ( vehicleMinRXQuality == 255 )
      vehicleMinRXQuality = 0;

   vehicleAvgRXQuality /= g_SM_VideoLinkStats.backIntervalsToLookForDownStep;
   vehicleMinRXQuality = vehicleAvgRXQuality;

   g_SM_VideoLinkStats.computed_rx_quality_vehicle = vehicleMinRXQuality;

   int rxQualityPerLevel = (1.0-s_fParamsChangeStrength) * 50;

   if ( bDownDirection )
   {
      if ( g_SM_VideoLinkStats.computed_rx_quality_vehicle < 90 )
      {
         int iTargetLevel = (90-g_SM_VideoLinkStats.computed_rx_quality_vehicle)/rxQualityPerLevel;
         s_iTargetShiftLevel = iTargetLevel;
      }

      if ( (0 == s_iTargetShiftLevel) && ( (iCountRetransmissions != 0) || (iCountIntervalsWithRetires != 0) ) )
         s_iTargetShiftLevel = 1;
      else
      {
         if ( iCountRetransmissions >= 0.3*iIntervals )
         {
         int iLevel = g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel+1;
         if ( iLevel > s_iTargetShiftLevel )
            s_iTargetShiftLevel = iLevel;
         }

         if ( iCountIntervalsWithRetires > 0.2 * iIntervals )
         {
          // Go down to next profile
          s_iTargetShiftLevel = s_iMaxLevelShiftForCurrentProfile + 1;
         }
     }
   }
   else
   {
      int iTargetLevelFromRx = (100-g_SM_VideoLinkStats.computed_rx_quality_vehicle)/rxQualityPerLevel;
      if ( iTargetLevelFromRx < 0 )
      {
         iTargetLevelFromRx = 0;
      }
      s_iTargetShiftLevel = iTargetLevelFromRx;

      // Do only one level shift, not more than one
      if ( s_iTargetShiftLevel < ((int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel)-1 )
         s_iTargetShiftLevel = ((int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel)-1;

      if ( iCountRetransmissions == 0 && iCountIntervalsWithRetires == 0 )
         s_iTargetShiftLevel--;
      else if ( iCountRetransmissions < 0.1*iIntervals )
      {
         int iLevelFromR = ((int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel)-1;
         if ( iLevelFromR < s_iTargetShiftLevel )
            s_iTargetShiftLevel = iLevelFromR;
      }
      else if ( iCountRetransmissions < 0.3*iIntervals &&
                0 == iCountIntervalsWithRetires && (g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != g_SM_VideoLinkStats.overwrites.userVideoLinkProfile) )
      {
         s_iTargetShiftLevel--;
      }
   }
}

void _check_video_link_params_using_controller_info(bool bDownDirection)
{
   int iIntervals = g_SM_VideoLinkStats.backIntervalsToLookForUpStep;
   if ( bDownDirection )
      iIntervals = g_SM_VideoLinkStats.backIntervalsToLookForDownStep;
   if ( iIntervals > MAX_INTERVALS_VIDEO_LINK_STATS )
      iIntervals = MAX_INTERVALS_VIDEO_LINK_STATS;

   int maxECPacketsUsed = 0;
   int intervalsWithReconstructedBlocks = 0;
   int intervalsWithReconstructedBlocksAtECLimit = 0;
   int intervalsWithMissingData = 0;
   int intervalsWithRetransmissions = 0;
   int intervalsWithBadOutputBlocks = 0;

   for( int i=0; i<iIntervals; i++ )
   {
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i] == 0xFF )
         intervalsWithMissingData++;
      else if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i] > maxECPacketsUsed )
         maxECPacketsUsed = g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i];

      if ( ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i] == 0xFF )
         || ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i] == 0 ) )
      if ( ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i] == 0xFF )
         || ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i] == 0 ) )
         intervalsWithBadOutputBlocks++;

      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i] != 0xFF )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[0][i] != 0 )
         intervalsWithReconstructedBlocks++;

      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i] != 0xFF )
      if ( ((int)g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i]) > ((int)g_SM_VideoLinkStats.overwrites.currentECBlocks)-2 )
         intervalsWithReconstructedBlocksAtECLimit++;

      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[0][i] != 0xFF )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[0][i] != 0 )
         intervalsWithRetransmissions++;
   }

   if ( bDownDirection )
   {
      int nTargetShift = 0;

      //if ( intervalsWithMissingData > iIntervals * 0.5 + 1 )
      //   nTargetShift = 1;

      //if ( intervalsWithReconstructedBlocks > iIntervals * (0.7*(1.0-s_fParamsChangeStrength)+0.1) )
      //   nTargetShift = 1;

      if ( intervalsWithReconstructedBlocksAtECLimit > iIntervals * (0.7*(1.0-s_fParamsChangeStrength)+0.1) )
         nTargetShift = 1;

      if ( intervalsWithRetransmissions > iIntervals * (0.5*(1.0-s_fParamsChangeStrength)+0.2 ) )
         nTargetShift = 1;

      // Switch to lower level on high retransmissions count or on many bad blocks outputs or discarded segments

      if ( intervalsWithBadOutputBlocks > iIntervals * 0.5 + 1 )
         nTargetShift += s_iMaxLevelShiftForCurrentProfile+1;

      if ( intervalsWithRetransmissions > iIntervals * (0.9*(1.0-s_fParamsChangeStrength)+0.1 ) )
         nTargetShift += s_iMaxLevelShiftForCurrentProfile+1;

      if ( nTargetShift != 0 )
      {
         s_iTargetShiftLevel += nTargetShift;
      }
   }
   else if ( intervalsWithMissingData < iIntervals * 0.5 )
   {
      int nTargetShift = 0;

      //if ( intervalsWithReconstructedBlocks < iIntervals * (0.3*(1.0-s_fParamsChangeStrength)+0.05)+1 )
      //   nTargetShift = -1;

      if ( intervalsWithReconstructedBlocksAtECLimit < iIntervals * (0.3*(1.0-s_fParamsChangeStrength)+0.05)+1 )
         nTargetShift = -1;

      // Switch to higher level on very low retransmissions count

      if ( intervalsWithRetransmissions < iIntervals * (0.2*(1.0-s_fParamsChangeStrength)+0.1) + 1 )
         nTargetShift -= s_iMaxLevelShiftForCurrentProfile+1;

      if ( nTargetShift != 0 )
      {
         s_iTargetShiftLevel += nTargetShift;
      }
   }
}

void _video_stats_overwrites_update_graphs()
{
      for( int i=MAX_INTERVALS_VIDEO_LINK_STATS-1; i>0; i-- )
      {
         g_SM_VideoLinkGraphs.vehicleRXQuality[i] = g_SM_VideoLinkGraphs.vehicleRXQuality[i-1];
         g_SM_VideoLinkGraphs.vehicleRXMaxTimeGap[i] = g_SM_VideoLinkGraphs.vehicleRXMaxTimeGap[i-1];
         g_SM_VideoLinkGraphs.vehileReceivedRetransmissionsRequestsCount[i] = g_SM_VideoLinkGraphs.vehileReceivedRetransmissionsRequestsCount[i-1];
         g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPackets[i] = g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPackets[i-1];
         g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPacketsRetried[i] = g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPacketsRetried[i-1];

         for( int k=0; k<g_PD_LastRecvControllerLinksStats.radio_interfaces_count; k++ )
            g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[k][i] = g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[k][i-1];

         for( int k=0; k<g_PD_LastRecvControllerLinksStats.video_streams_count; k++ )
         {
            g_SM_VideoLinkGraphs.controller_received_radio_streams_rx_quality[k][i] = g_SM_VideoLinkGraphs.controller_received_radio_streams_rx_quality[k][i-1];
            g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[k][i] = g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[k][i-1];
            g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[k][i] = g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[k][i-1];
            g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[k][i] = g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[k][i-1];
            g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[k][i] = g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[k][i-1];
         }
      }

      g_SM_VideoLinkGraphs.vehicleRXQuality[0] = 255;
      g_SM_VideoLinkGraphs.vehicleRXMaxTimeGap[0] = 255;
      g_SM_VideoLinkGraphs.vehileReceivedRetransmissionsRequestsCount[0] = g_SM_VideoLinkGraphs.tmp_vehileReceivedRetransmissionsRequestsCount;
      g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPackets[0] = g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPackets;
      g_SM_VideoLinkGraphs.vehicleReceivedRetransmissionsRequestsPacketsRetried[0] = g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPacketsRetried;
      g_SM_VideoLinkGraphs.tmp_vehileReceivedRetransmissionsRequestsCount = 0;
      g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPackets = 0;
      g_SM_VideoLinkGraphs.tmp_vehicleReceivedRetransmissionsRequestsPacketsRetried = 0;

      for( int k=0; k<g_PD_LastRecvControllerLinksStats.radio_interfaces_count; k++ )
         g_SM_VideoLinkGraphs.controller_received_radio_interfaces_rx_quality[k][0] = 255;

      for( int k=0; k<g_PD_LastRecvControllerLinksStats.video_streams_count; k++ )
      {
         g_SM_VideoLinkGraphs.controller_received_radio_streams_rx_quality[k][0] = 255;
         g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[k][0] = 255;
         g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_reconstructed[k][0] = 255;
         g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[k][0] = 255;
         g_SM_VideoLinkGraphs.controller_received_video_streams_requested_retransmission_packets[k][0] = 255;
      }
}

void _video_stats_overwrites_check_update_params()
{
   if ( g_TimeNow < g_SM_VideoLinkStats.timeLastStatsCheck + g_SM_VideoLinkStats.computed_min_interval_for_change_down )
      return;

   g_SM_VideoLinkStats.timeLastStatsCheck = g_TimeNow;

   if ( g_TimeLastReceivedRadioPacketFromController == 0 || (g_TimeNow < g_TimeFirstReceivedRadioPacketFromController + 2000) )
      return;

   // Init computation data

   s_iMaxLevelShiftForCurrentProfile = g_pCurrentModel->get_video_profile_total_levels(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile);
   s_bUseControllerInfo = (g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?true:false;
   
   s_iTargetProfile = g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile;
   s_iTargetShiftLevel = g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel;

   s_fParamsChangeStrength = 0.8*(float)g_pCurrentModel->video_params.videoAdjustmentStrength / 10.0;
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_SM_VideoLinkStats.overwrites.userVideoLinkProfile )
      s_fParamsChangeStrength = 0.8*(float)(g_pCurrentModel->video_params.videoAdjustmentStrength+2) / 10.0;
   if ( s_fParamsChangeStrength > 1.0 )
      s_fParamsChangeStrength = 1.0;

   g_SM_VideoLinkStats.backIntervalsToLookForDownStep = DEFAULT_MINIMUM_INTERVALS_FOR_VIDEO_LINK_ADJUSTMENT + (1.0 - s_fParamsChangeStrength) * 0.5 * MAX_INTERVALS_VIDEO_LINK_STATS;
   if ( g_SM_VideoLinkStats.backIntervalsToLookForDownStep > MAX_INTERVALS_VIDEO_LINK_STATS )
      g_SM_VideoLinkStats.backIntervalsToLookForDownStep = MAX_INTERVALS_VIDEO_LINK_STATS;

   g_SM_VideoLinkStats.backIntervalsToLookForUpStep = 6 + s_fParamsChangeStrength * MAX_INTERVALS_VIDEO_LINK_STATS;
   if ( g_SM_VideoLinkStats.backIntervalsToLookForUpStep > MAX_INTERVALS_VIDEO_LINK_STATS )
      g_SM_VideoLinkStats.backIntervalsToLookForUpStep = MAX_INTERVALS_VIDEO_LINK_STATS;

   g_SM_VideoLinkStats.computed_min_interval_for_change_down = 50 + (1.0 - s_fParamsChangeStrength)*500;
   g_SM_VideoLinkStats.computed_min_interval_for_change_up = 500 + (1.0 - s_fParamsChangeStrength)*1500;
   
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
   {
      s_fParamsChangeStrength -= 0.1;
      if ( s_fParamsChangeStrength < 0.0 )
         s_fParamsChangeStrength = 0.0;

      g_SM_VideoLinkStats.backIntervalsToLookForDownStep = DEFAULT_MINIMUM_INTERVALS_FOR_VIDEO_LINK_ADJUSTMENT*2 + (1.0 - s_fParamsChangeStrength) * 0.6 * MAX_INTERVALS_VIDEO_LINK_STATS;
      if ( g_SM_VideoLinkStats.backIntervalsToLookForDownStep > MAX_INTERVALS_VIDEO_LINK_STATS )
         g_SM_VideoLinkStats.backIntervalsToLookForDownStep = MAX_INTERVALS_VIDEO_LINK_STATS;
      g_SM_VideoLinkStats.computed_min_interval_for_change_down = 200 + (1.0 - s_fParamsChangeStrength)*1000;
   }
   //log_line("usr profile %d-%d, curent profile %d, level %d", g_pCurrentModel->video_params.user_selected_video_link_profile, g_SM_VideoLinkStats.overwrites.userVideoLinkProfile, g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile, g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel);


   int countIntervalsReceivedOkFromControllerForDownShift = 0;
   int countIntervalsReceivedOkFromControllerForUpShift = 0;
   for( int i=0; i<g_SM_VideoLinkStats.backIntervalsToLookForDownStep; i++ )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i] != 255 || 
           g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i] != 255 )
         countIntervalsReceivedOkFromControllerForDownShift++;
   for( int i=0; i<g_SM_VideoLinkStats.backIntervalsToLookForUpStep; i++ )
      if ( g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_clean[0][i] != 255 || 
           g_SM_VideoLinkGraphs.controller_received_video_streams_blocks_max_ec_packets_used[0][i] != 255 )
         countIntervalsReceivedOkFromControllerForUpShift++;

   bool bUseControllerDown = false;
   bool bUseControllerUp = false;
   g_SM_VideoLinkStats.usedControllerInfo = 0;

   if ( s_bUseControllerInfo )
   {
      bUseControllerDown = true;
      bUseControllerUp = true;
      g_SM_VideoLinkStats.usedControllerInfo = 1;    
   }

   /*
   if ( countIntervalsOkDownFromController >= g_SM_VideoLinkStats.backIntervalsToLookForDownStep/2 + 1 )
   {
      g_SM_VideoLinkStats.usedControllerInfo = 1;
      bUseControllerDown = true;
   }
   if ( countIntervalsOkUpFromController >= g_SM_VideoLinkStats.backIntervalsToLookForUpStep/2 + 1 )
   {
      g_SM_VideoLinkStats.usedControllerInfo = 1;
      bUseControllerUp = true;
   }
   */
   
   if ( g_iForcedVideoProfile != -1 )
   if ( g_iForcedVideoProfile != g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
   {
      s_iTargetProfile = g_iForcedVideoProfile;
      s_iTargetShiftLevel = 0;
      _video_stats_overwrites_apply_profile_changes(true);

      g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown = g_TimeNow;
      g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp = g_TimeNow;
      g_SM_VideoLinkStats.timeLastProfileChangeDown = g_TimeNow;
      g_SM_VideoLinkStats.timeLastProfileChangeUp = g_TimeNow;

      return;
   }

   // Check for actual video bitrate outputed to be in overload or underload condition

   video_link_check_adjust_bitrate_for_overload();

   // Check for shift down

   if ( g_TimeNow >= g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown + g_SM_VideoLinkStats.computed_min_interval_for_change_down )
   if ( g_TimeNow >= g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp + g_SM_VideoLinkStats.computed_min_interval_for_change_down )
   {
      if ( (! s_bUseControllerInfo) || (!bUseControllerDown) )
         _check_video_link_params_using_vehicle_info(true);
      else if ( bUseControllerDown )
         _check_video_link_params_using_controller_info(true);

      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK )
      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST )
      if ( g_bHadEverLinkToController )
      if ( (! g_bHasLinkToController) || ( g_SM_RadioStats.iMaxRxQuality< 30) )
      {
          if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_SM_VideoLinkStats.overwrites.userVideoLinkProfile )
          {
             s_iTargetProfile = VIDEO_PROFILE_MQ;
             s_iTargetShiftLevel = 0;
          }
          else if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
          {
             s_iTargetProfile = VIDEO_PROFILE_LQ;
             s_iTargetShiftLevel = 0;
          }
      }

      if ( (s_iTargetShiftLevel != g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel) || (s_iTargetProfile != g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile) )
      {
         _video_stats_overwrites_apply_changes();
         return;
      }
      else
         s_uTimeStartGoodIntervalForProfileShiftUp = 0;
   }

   // Check for shift up

   if ( g_TimeNow >= g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown + g_SM_VideoLinkStats.computed_min_interval_for_change_up )
   if ( g_TimeNow >= g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp + g_SM_VideoLinkStats.computed_min_interval_for_change_up )
   {
      if ( (! s_bUseControllerInfo) || (!bUseControllerUp) )
         _check_video_link_params_using_vehicle_info(false);
      else if ( bUseControllerUp )
         _check_video_link_params_using_controller_info(false);

      if ( (s_iTargetShiftLevel != g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel) || (s_iTargetProfile != g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile) )
      {
         _video_stats_overwrites_apply_changes();
         return;
      }
      else
         s_uTimeStartGoodIntervalForProfileShiftUp = 0;
   }
}

void video_stats_overwrites_periodic_loop()
{
   if ( (g_pCurrentModel == NULL) || (! g_pCurrentModel->hasCamera()) )
      return;
   if ( g_bVideoPaused )
      return;
   if ( hardware_board_is_goke(hardware_getBoardType()) )
      return;
   if ( g_TimeNow >= g_SM_VideoLinkGraphs.timeLastStatsUpdate + VIDEO_LINK_STATS_REFRESH_INTERVAL_MS )
   {
      g_SM_VideoLinkGraphs.timeLastStatsUpdate = g_TimeNow;
      _video_stats_overwrites_update_graphs();
   }

   if ( g_TimeNow >= s_TimeLastHistorySwitchUpdate + 100 )
   {
      s_TimeLastHistorySwitchUpdate = g_TimeNow;
      for( int i=MAX_INTERVALS_VIDEO_LINK_SWITCHES-1; i>0; i-- )
         g_SM_VideoLinkStats.historySwitches[i] = g_SM_VideoLinkStats.historySwitches[i-1];
      g_SM_VideoLinkStats.historySwitches[0] = g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel | (g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile<<4);
   
      #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
      #else
      g_SM_VideoLinkStats.timeLastStatsCheck = g_TimeNow;
      #endif
   }

   // Update adaptive video rate for tx radio:

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_SM_VideoLinkStats.overwrites.userVideoLinkProfile )
      packet_utils_set_adaptive_video_datarate(0);
   else
   {
      int nRateTxVideo = DEFAULT_RADIO_DATARATE_VIDEO;
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
         nRateTxVideo = utils_get_video_profile_mq_radio_datarate(g_pCurrentModel);

      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
         nRateTxVideo = utils_get_video_profile_lq_radio_datarate(g_pCurrentModel);

      // If it's increasing, set it right away
      if ( (0 != packet_utils_get_last_set_adaptive_video_datarate()) &&
           (getRealDataRateFromRadioDataRate(nRateTxVideo, 0) >= getRealDataRateFromRadioDataRate(packet_utils_get_last_set_adaptive_video_datarate(), 0)) )
         packet_utils_set_adaptive_video_datarate(nRateTxVideo);
      // It's decreasing, set it after a short period
      else if ( g_TimeNow > g_SM_VideoLinkStats.overwrites.uTimeSetCurrentVideoLinkProfile + DEFAULT_LOWER_VIDEO_RADIO_DATARATE_AFTER_MS )
         packet_utils_set_adaptive_video_datarate(nRateTxVideo);
   }

   video_link_check_adjust_quantization_for_overload_periodic_loop();
   
   // On link from controller lost, switch to lower profile and keyframe

   int iThresholdControllerLinkMs = 500;

   // Change on link to controller lost

   float fParamsChangeStrength = (float)g_pCurrentModel->video_params.videoAdjustmentStrength / 10.0;
   iThresholdControllerLinkMs = 1000 + (1.0 - fParamsChangeStrength)*2000.0;

   if ( ! g_pCurrentModel->isVideoLinkFixedOneWay() )
   if ( g_TimeNow > g_TimeStart + 5000 )
   if ( g_TimeNow > get_video_capture_start_program_time() + 3000 )
   if ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK )
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST )
   if ( (g_TimeNow > g_TimeLastReceivedRadioPacketFromController + iThresholdControllerLinkMs) ||
        (g_SM_RadioStats.iMaxRxQuality< 30) )
   if ( g_TimeNow > g_SM_VideoLinkStats.timeLastProfileChangeDown + iThresholdControllerLinkMs*0.7 )
   if ( g_TimeNow > g_SM_VideoLinkStats.timeLastProfileChangeUp + iThresholdControllerLinkMs*0.7 )
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_SM_VideoLinkStats.overwrites.userVideoLinkProfile )
      {
         log_line("[Video] Switch to MQ profile due to controller link lost and adaptive video is on.");
         video_stats_overwrites_switch_to_profile_and_level(g_pCurrentModel->get_video_profile_total_levels(g_pCurrentModel->video_params.user_selected_video_link_profile), VIDEO_PROFILE_MQ,0);
      }
      else if ( ! (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO) )
      {
         if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
         {
            log_line("[Video] Switch to LQ profile due to controller link lost and adaptive video is on (full).");
            video_stats_overwrites_switch_to_profile_and_level(g_pCurrentModel->get_video_profile_total_levels(g_pCurrentModel->video_params.user_selected_video_link_profile) + g_pCurrentModel->get_video_profile_total_levels(VIDEO_PROFILE_MQ), VIDEO_PROFILE_LQ,0);
         }
      }
   }
   
   if ( ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK) ||
        ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_KEEP_CONSTANT_BITRATE) )
      video_link_check_adjust_bitrate_for_overload();
   

   if (!((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].uProfileEncodingFlags) & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK) )
      return;

   #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
   if ( ! g_pCurrentModel->isVideoLinkFixedOneWay() )
      _video_stats_overwrites_check_update_params();
   #endif
}

// Returns true if it increased

bool video_stats_overwrites_increase_videobitrate_overwrite(u32 uCurrentTotalBitrate)
{
   int iCurrentVideoProfile = g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile;
   int iCurrentOverwriteDown = g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iCurrentVideoProfile];

   u32 uTopVideoBitrate = g_SM_VideoLinkStats.overwrites.currentProfileMaxVideoBitrate;
   uTopVideoBitrate -= iCurrentOverwriteDown;
   
   // Can't go lower
   if ( uTopVideoBitrate <= g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      return false;

   if ( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].bitrate_fixed_bps > 0 )
   if ( (u32)iCurrentOverwriteDown >= g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].bitrate_fixed_bps/2 )
      return false;

   if ( iCurrentVideoProfile == VIDEO_PROFILE_LQ )
      g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iCurrentVideoProfile] += 250000;
   else
   {
      u32 uShift = uCurrentTotalBitrate / 10;
      if ( uShift < 100000 )
         uShift = 100000;
      if ( uShift > g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/4 )
         uShift = g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/4;
      g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iCurrentVideoProfile] += uShift;
   }
   
   if ( g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iCurrentVideoProfile] > (uTopVideoBitrate*2)/3 )
      g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iCurrentVideoProfile] = (uTopVideoBitrate*2)/3;

   g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile_including_overwrites();

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_USER )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_HIGH_QUALITY )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_BEST_PERF )
   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < 200000 )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = 200000;

   _video_overwrites_set_capture_video_bitrate(g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate, false, 4);

   return true;
}

// Returns true if it decreased
bool video_stats_overwrites_decrease_videobitrate_overwrite()
{
   int iCurrentVideoProfile = g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile;
   int iCurrentOverwriteDown = g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iCurrentVideoProfile];
   if ( iCurrentOverwriteDown == 0 )
      return false;

   if ( iCurrentVideoProfile == VIDEO_PROFILE_LQ )
   {
      if ( iCurrentOverwriteDown >= 250000 )
         g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iCurrentVideoProfile] -= 250000;
      else
         g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iCurrentVideoProfile] = 0;
   }
   else
   {
      if ( iCurrentOverwriteDown >= 500000 )
         g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iCurrentVideoProfile] -= 500000;
      else
         g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iCurrentVideoProfile] = 0;
   }

   g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile_including_overwrites();
   
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_USER )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_HIGH_QUALITY )
   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_BEST_PERF )
   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < 200000 )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = 200000;

   _video_overwrites_set_capture_video_bitrate(g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate, false, 5);

   return true;
}

int _video_stats_overwrites_get_lower_datarate_value(int iDataRateBPS, int iLevelsDown )
{
   if ( iDataRateBPS < 0 )
   {
      for( int i=0; i<iLevelsDown; i++ )
      {
         if ( iDataRateBPS < -1 )
            iDataRateBPS++;
      }

      // Lowest rate is MCS-0, about 6mbps
      return iDataRateBPS;
   }

   int iCurrentIndex = -1;
   for( int i=0; i<getDataRatesCount(); i++ )
   {
      if ( getDataRatesBPS()[i] == iDataRateBPS )
      {
         iCurrentIndex = i;
         break;
      }
   }

   // Do not decrease below 6 mbps
   for( int i=0; i<iLevelsDown; i++ )
   {
      if ( (iCurrentIndex > 0) )
      if ( getRealDataRateFromRadioDataRate(getDataRatesBPS()[iCurrentIndex], 0) > 6000000)
         iCurrentIndex--;
   }

   if ( iCurrentIndex >= 0 )
      iDataRateBPS = getDataRatesBPS()[iCurrentIndex];
   return iDataRateBPS;
}

int video_stats_overwrites_get_current_radio_datarate_video(int iRadioLink, int iRadioInterface)
{
   int nRateTx = g_pCurrentModel->radioLinksParams.link_datarate_video_bps[iRadioLink];
   bool bUsesHT40 = false;
   if ( g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink] & RADIO_FLAG_HT40_VEHICLE )
      bUsesHT40 = true;
   
   int nRateUserVideoProfile = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].radio_datarate_video_bps;
   if ( 0 != nRateUserVideoProfile )
   if ( getRealDataRateFromRadioDataRate(nRateUserVideoProfile, bUsesHT40) < getRealDataRateFromRadioDataRate(nRateTx, bUsesHT40) )
      nRateTx = nRateUserVideoProfile;

   // nRateTx is now the top radio rate (highest one, set by user)

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_SM_VideoLinkStats.overwrites.userVideoLinkProfile )
      return nRateTx;


   int nRateCurrentVideoProfile = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].radio_datarate_video_bps;
   if ( (0 != nRateCurrentVideoProfile) && ( getRealDataRateFromRadioDataRate(nRateCurrentVideoProfile, bUsesHT40) < getRealDataRateFromRadioDataRate(nRateTx, bUsesHT40) ) )
      nRateTx = nRateCurrentVideoProfile;

   // Decrease nRateTx for MQ, LQ profiles

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
      nRateTx = utils_get_video_profile_mq_radio_datarate(g_pCurrentModel);

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
      nRateTx = utils_get_video_profile_lq_radio_datarate(g_pCurrentModel);

   return nRateTx;
}

int video_stats_overwrites_get_next_level_down_radio_datarate_video(int iRadioLink, int iRadioInterface)
{
   int nRateTx = video_stats_overwrites_get_current_radio_datarate_video(iRadioLink, iRadioInterface);
 
   nRateTx = _video_stats_overwrites_get_lower_datarate_value(nRateTx, 1);
   return nRateTx;
}

u32 video_stats_overwrites_get_time_last_shift_down()
{
   return s_uTimeLastShiftLevelDown;
}