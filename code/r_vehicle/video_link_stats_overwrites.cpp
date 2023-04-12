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
#include "../base/shared_mem.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"

#include "video_link_stats_overwrites.h"
#include "video_link_check_bitrate.h"
#include "video_link_auto_keyframe.h"
#include "processor_tx_video.h"
#include "utils_vehicle.h"
#include "packets_utils.h"
#include "timers.h"
#include "shared_vars.h"


static bool s_bUseControllerInfo = false;
static int  s_iTargetShiftLevel = 0;
static int  s_iTargetProfile = 0;
static int  s_iMaxLevelShiftForCurrentProfile = 0;
static float s_fParamsChangeStrength = 0.0; // 0...1

static u32 s_TimeLastHistorySwitchUpdate = 0;

static int s_iLastTargetVideoFPS = 0;

// Needs to be sent to other processes when we change the currently selected video profile

void _video_stats_overwrites_signal_changes()
{
   //log_line("[Video Link Overwrites]: signal others that currently active video profile changed, to reload model");

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_UPDATED_VIDEO_LINK_OVERWRITES;
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + sizeof(shared_mem_video_link_overwrites);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer + sizeof(t_packet_header), (u8*)&(g_SM_VideoLinkStats.overwrites), sizeof(shared_mem_video_link_overwrites));
   packet_compute_crc(buffer, PH.total_length);

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
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_VEHICLE_VIDEO_PROFILE_SWITCHED;
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = (u32)g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);

   packet_compute_crc((u8*)&PH, PH.total_length);
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
   video_link_auto_keyframe_init();
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

   log_line("[Video Link Overwrites]: Init video links stats and overwrites structure: Done. Structure size: %d bytes", sizeof(shared_mem_video_link_stats_and_overwrites));
}

u32 _get_bitrate_for_current_level_and_profile()
{
   u32 uMaxShiftLevels = (u32) g_pCurrentModel->get_video_link_profile_max_level_shifts(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile);
   int currentVideoProfile = g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile;

   u32 uTopVideoBitrate = g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate;
   u32 uOverwriteDown = g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[currentVideoProfile];
   if ( uTopVideoBitrate > uOverwriteDown )
      uTopVideoBitrate -= uOverwriteDown;
   else
      uTopVideoBitrate = 0;

   if ( uTopVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      uTopVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   if ( 0 == uMaxShiftLevels )
      return uTopVideoBitrate;

   u32 uTopTotalBitrate = uTopVideoBitrate * (g_pCurrentModel->video_link_profiles[currentVideoProfile].block_packets + g_pCurrentModel->video_link_profiles[currentVideoProfile].block_fecs) / g_pCurrentModel->video_link_profiles[currentVideoProfile].block_packets;
   u32 uBottomVideoBitrate = uTopTotalBitrate * g_pCurrentModel->video_link_profiles[currentVideoProfile].block_packets / (g_pCurrentModel->video_link_profiles[currentVideoProfile].block_packets + g_pCurrentModel->video_link_profiles[currentVideoProfile].block_fecs + uMaxShiftLevels);
   
   uBottomVideoBitrate = uBottomVideoBitrate * 9 / 10;
   if ( uBottomVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      uBottomVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
   {
      // If MQ is on 12mb radio rate, lowest video is 2mb
      if ( g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_video == 0 )
      if ( getRealDataRateFromRadioDataRate(g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].radio_datarate_video) >= 12000000 )
      if ( uBottomVideoBitrate < 2000000 )
         uBottomVideoBitrate = 2000000;

      // If MQ is on 12mb radio rate, lowest video is 2mb
      if ( getRealDataRateFromRadioDataRate(g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_video) >= 12000000 )
      if ( uBottomVideoBitrate < 2000000 )
         uBottomVideoBitrate = 2000000;

      // Do not go above what the user set as default bitrate for this profile
      if ( uBottomVideoBitrate > g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate )
         uBottomVideoBitrate = g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate;
   }

   // Begin: Compute the max video bitrate of the next video profile top range
   int radioDataRateNextLevelDown = 0;
   int videoProfileNextDown = VIDEO_PROFILE_MQ;
   if ( currentVideoProfile == g_SM_VideoLinkStats.overwrites.userVideoLinkProfile )
   {
      radioDataRateNextLevelDown = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_video;
      if ( radioDataRateNextLevelDown == 0 )
         radioDataRateNextLevelDown = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].radio_datarate_video;
   }
   if ( currentVideoProfile == VIDEO_PROFILE_MQ )
   {
      videoProfileNextDown = VIDEO_PROFILE_LQ;
      radioDataRateNextLevelDown = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].radio_datarate_video;    
      if ( radioDataRateNextLevelDown == 0 )
         radioDataRateNextLevelDown = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_video;
      if ( radioDataRateNextLevelDown == 0 )
         radioDataRateNextLevelDown = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].radio_datarate_video;
   }
   u32 radioBitRateNextLevelDown = getRealDataRateFromRadioDataRate(radioDataRateNextLevelDown);
   u32 uBottomVideoBitrate2 = radioBitRateNextLevelDown / (g_pCurrentModel->video_link_profiles[videoProfileNextDown].block_packets + g_pCurrentModel->video_link_profiles[videoProfileNextDown].block_fecs);
   uBottomVideoBitrate2 *= g_pCurrentModel->video_link_profiles[videoProfileNextDown].block_packets * DEFAULT_VIDEO_LINK_MAX_LOAD_PERCENT / 100;
   if ( uBottomVideoBitrate2 < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      uBottomVideoBitrate2 = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;
   
   // End: Compute the max video bitrate of the next video profile top range

   // Move current profile lowest video bitrate shift down to match the next profile top video bitrate

   if ( uBottomVideoBitrate2 < uBottomVideoBitrate )
      uBottomVideoBitrate = uBottomVideoBitrate2;

   u32 uVideoBitrateChangePerLevel = (uTopVideoBitrate - uBottomVideoBitrate) / uMaxShiftLevels;

   u32 resultVideoBitrate  = uTopVideoBitrate - uVideoBitrateChangePerLevel * g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel;

   if ( resultVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      resultVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;


   // Set video bitrate so that radio link is at least 30% used (data+ec)
   /*
   u32 uMaxTotalBitrate = 0;
   if ( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].radio_datarate_video != 0 )
      uMaxTotalBitrate = getRealDataRateFromRadioDataRate(g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].radio_datarate_video);
   else
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
         uMaxTotalBitrate = getRealDataRateFromRadioDataRate(g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].radio_datarate_video);
      else
         uMaxTotalBitrate = getRealDataRateFromRadioDataRate(g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_video);
   }
   if ( resultVideoBitrate * ((g_SM_VideoLinkStats.overwrites.currentDataBlocks+g_SM_VideoLinkStats.overwrites.currentECBlocks)/g_SM_VideoLinkStats.overwrites.currentDataBlocks) < uMaxTotalBitrate/3 )
   {
      resultVideoBitrate = uMaxTotalBitrate/3 / (((g_SM_VideoLinkStats.overwrites.currentDataBlocks+g_SM_VideoLinkStats.overwrites.currentECBlocks)/g_SM_VideoLinkStats.overwrites.currentDataBlocks));
   }
   */
     
   return resultVideoBitrate;
}

void video_stats_overwrites_switch_to_profile_and_level(int iVideoProfile, int iLevelShift)
{
   g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile = iVideoProfile;
   g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel = iLevelShift;
   g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].bitrate_fixed_bps;

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
      g_SM_VideoLinkStats.overwrites.hasEverSwitchedToLQMode = 1;

   if ( 0 == g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate )
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
         g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps;
   }
   if ( 0 == g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate )
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ ||
           g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
         g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps;
   }

   g_SM_VideoLinkStats.overwrites.currentDataBlocks = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_packets;
   g_SM_VideoLinkStats.overwrites.currentECBlocks = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs;
   g_SM_VideoLinkStats.overwrites.currentECBlocks += g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel;

   g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile();

   g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].width = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width;
   g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].height = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height;
   
   // Do not use more video bitrate than the user set originaly in the user profile

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ || g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
   if ( g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate > g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps )
   {
      g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps;
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile();
   }

   if ( g_SM_VideoLinkStats.overwrites.currentECBlocks > MAX_FECS_PACKETS_IN_BLOCK )
      g_SM_VideoLinkStats.overwrites.currentECBlocks = MAX_FECS_PACKETS_IN_BLOCK;

   g_SM_VideoLinkStats.historySwitches[0] = g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel | (g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile<<4);
   g_SM_VideoLinkStats.totalSwitches++;

   g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown = g_TimeNow;
   g_SM_VideoLinkStats.timeLastProfileChangeDown = g_TimeNow;
   g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeUp = g_TimeNow;
   g_SM_VideoLinkStats.timeLastProfileChangeUp = g_TimeNow;
   
   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;
   send_control_message_to_raspivid(RASPIVID_COMMAND_ID_VIDEO_BITRATE, g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/100000);

   
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
   g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile = s_iTargetProfile;
   g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel = s_iTargetShiftLevel;
   g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].bitrate_fixed_bps;

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
      g_SM_VideoLinkStats.overwrites.hasEverSwitchedToLQMode = 1;

   if ( 0 == g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate )
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
         g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps;
   }
   if ( 0 == g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate )
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ ||
           g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
         g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps;
   }

   g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile();
   g_SM_VideoLinkStats.overwrites.currentDataBlocks = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_packets;
   g_SM_VideoLinkStats.overwrites.currentECBlocks = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].block_fecs;


   g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].width = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width;
   g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].height = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height;
   
   // Do not use more video bitrate than the user set originaly in the user profile

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ || g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
   if ( g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate > g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps )
   {
      g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].bitrate_fixed_bps;
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile();
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

   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;
   send_control_message_to_raspivid(RASPIVID_COMMAND_ID_VIDEO_BITRATE, g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/100000);

   int nTargetFPS = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].fps;
   if ( 0 == nTargetFPS )
      nTargetFPS = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps;

   if ( s_iLastTargetVideoFPS != nTargetFPS )
   {
      s_iLastTargetVideoFPS = nTargetFPS;
      
      log_line("[Video Link Overwrites]: Setting the video capture FPS rate to: %d", nTargetFPS);
      send_control_message_to_raspivid(RASPIVID_COMMAND_ID_FPS, nTargetFPS);
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

   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   send_control_message_to_raspivid(RASPIVID_COMMAND_ID_VIDEO_BITRATE, g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/100000);

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
   _video_stats_overwrites_apply_profile_changes(true);
}

void _video_stats_overwrites_apply_changes()
{
   if ( s_iTargetProfile == g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
   if ( s_iTargetShiftLevel > (int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel )
   {
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

         g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile();

         _video_stats_overwrites_apply_ec_bitrate_changes(true);
      }
      return;
   }

   if ( s_iTargetProfile == g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
   if ( s_iTargetShiftLevel < (int)g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel )
   {
      if ( (g_iForcedVideoProfile != -1) && s_iTargetShiftLevel < 0 )
         s_iTargetShiftLevel = 0;

      if ( s_iTargetShiftLevel < 0 )
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

         g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile();

         _video_stats_overwrites_apply_ec_bitrate_changes(false);
      }
      return;
   }
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

   s_iMaxLevelShiftForCurrentProfile = g_pCurrentModel->get_video_link_profile_max_level_shifts(g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile);
   s_bUseControllerInfo = (g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?true:false;
   
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

      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST )
      if ( g_bHadEverLinkToController )
      if ( ! g_bHasLinkToController )
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
   }
}

void video_stats_overwrites_periodic_loop()
{
   if ( (g_pCurrentModel == NULL) || (! g_pCurrentModel->hasCamera()) )
      return;
   if ( g_bVideoPaused )
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

   video_link_check_adjust_quantization_for_overload();
   video_link_auto_keyframe_periodic_loop();

   // On link from controller lost, switch to lower profile and keyframe

   int iThresholdControllerLinkMs = 500;

   // Change on link to controller lost

   float fParamsChangeStrength = (float)g_pCurrentModel->video_params.videoAdjustmentStrength / 10.0;
   iThresholdControllerLinkMs = 500 + (1.0 - fParamsChangeStrength)*1000.0;
   if ( g_TimeNow > g_TimeStart + 5000 )
   if ( g_TimeNow > g_TimeStartRaspiVid + 3000 )
   if ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS )
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST )
   if ( g_TimeNow > g_TimeLastReceivedRadioPacketFromController + iThresholdControllerLinkMs )
   if ( g_TimeNow > g_SM_VideoLinkStats.timeLastProfileChangeDown + iThresholdControllerLinkMs*0.7 )
   if ( g_TimeNow > g_SM_VideoLinkStats.timeLastProfileChangeUp + iThresholdControllerLinkMs*0.7 )
   {
      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
      {
         if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_MQ )
            log_line("[Video] Switch to lower profile due to controller link lost and adaptive video is on (medium).");
      }
      else
      {   
         if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile != VIDEO_PROFILE_LQ )
            log_line("[Video] Switch to lower profile due to controller link lost and adaptive video is on (full).");
      }
      
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_SM_VideoLinkStats.overwrites.userVideoLinkProfile )
         video_stats_overwrites_switch_to_profile_and_level(VIDEO_PROFILE_MQ,0);
      else if ( ! (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO) )
      {
         if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
            video_stats_overwrites_switch_to_profile_and_level(VIDEO_PROFILE_LQ,0);
      }
   }
   
   if (!((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS) )
      return;

   #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
   _video_stats_overwrites_check_update_params();
   #endif
}


bool video_stats_overwrites_lower_current_profile_top_bitrate(u32 uCurrentTotalBitrate)
{
   u32 uTopVideoBitrate = g_SM_VideoLinkStats.overwrites.currentProfileDefaultVideoBitrate;
   uTopVideoBitrate -= g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile];
   
   // Can't go lower
   if ( uTopVideoBitrate <= g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      return false;

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
      g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile] += 250000;
   else
   {
      u32 uShift = uCurrentTotalBitrate / 10;
      if ( uShift < 100000 )
         uShift = 100000;
      if ( uShift > g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/4 )
         uShift = g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/4;
      g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile] += uShift;
   }
   
   g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile();

   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   send_control_message_to_raspivid(RASPIVID_COMMAND_ID_VIDEO_BITRATE, g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/100000);
   return true;
}

bool video_stats_overwrites_increase_current_profile_top_bitrate()
{
   if ( g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile] == 0 )
      return false;

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
   {
      if ( g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile] >= 250000 )
         g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile] -= 250000;
      else
         g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile] = 0;
   }
   else
   {
      if ( g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile] >= 500000 )
         g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile] -= 500000;
      else
         g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile] = 0;
   }

   g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = _get_bitrate_for_current_level_and_profile();
   
   if ( g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate < g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = g_pCurrentModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   send_control_message_to_raspivid(RASPIVID_COMMAND_ID_VIDEO_BITRATE, g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/100000);
   return true;
}

int _video_stats_overwrites_get_lower_datarate_value(int iDataRate, int iLevelsDown )
{
   if ( iDataRate < 0 )
   {
      for( int i=0; i<iLevelsDown; i++ )
      {
         if ( iDataRate < -1 )
            iDataRate++;
      }

      // Lowest rate is MCS-0, about 6mbps
      return iDataRate;
   }

   int iCurrentIndex = -1;
   for( int i=0; i<getDataRatesCount(); i++ )
   {
      if ( getDataRates()[i] == iDataRate )
      {
         iCurrentIndex = i;
         break;
      }
   }

   // Do not decrease below 6 mbps
   for( int i=0; i<iLevelsDown; i++ )
   {
      if ( (iCurrentIndex > 0) )
      if ( getRealDataRateFromRadioDataRate(getDataRates()[iCurrentIndex]) > 6000000)
         iCurrentIndex--;
   }

   if ( iCurrentIndex >= 0 )
      iDataRate = getDataRates()[iCurrentIndex];
   return iDataRate;
}

int video_stats_overwrites_get_current_radio_datarate_video(int iRadioLink, int iRadioInterface)
{
   int nRateTx = g_pCurrentModel->radioLinksParams.link_datarates[iRadioLink][0];
   if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_datarates[iRadioInterface][0] )
      nRateTx = g_pCurrentModel->radioInterfacesParams.interface_datarates[iRadioInterface][0];

   int nRateUserVideoProfile = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.userVideoLinkProfile].radio_datarate_video;
   if ( 0 != nRateUserVideoProfile )
   if ( getRealDataRateFromRadioDataRate(nRateUserVideoProfile) < getRealDataRateFromRadioDataRate(nRateTx) )
      nRateTx = nRateUserVideoProfile;

   // nRateTx is now the top radio rate (highest one)

   if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == g_SM_VideoLinkStats.overwrites.userVideoLinkProfile )
      return nRateTx;

   int nRateCurrentVideoProfile = g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].radio_datarate_video;
   if ( (0 != nRateCurrentVideoProfile) && ( getRealDataRateFromRadioDataRate(nRateCurrentVideoProfile) < getRealDataRateFromRadioDataRate(nRateTx) ) )
      nRateTx = nRateCurrentVideoProfile;

   // Decrease nRateTx for MQ, LQ profiles

   if ( 0 == nRateCurrentVideoProfile )
   if ( (g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ) || (g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ) )
   {
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_MQ )
      {
         if ( nRateTx > 0 )
         if ( getRealDataRateFromRadioDataRate(12) < getRealDataRateFromRadioDataRate(nRateTx) )
            nRateTx = 12;

         if ( nRateTx < 0 )
         if ( getRealDataRateFromRadioDataRate(-2) < getRealDataRateFromRadioDataRate(nRateTx) )
            nRateTx = -2;
      }
      if ( g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile == VIDEO_PROFILE_LQ )
      {
         if ( nRateTx > 0 )
         if ( getRealDataRateFromRadioDataRate(6) < getRealDataRateFromRadioDataRate(nRateTx) )
            nRateTx = 6;

         if ( nRateTx < 0 )
         if ( getRealDataRateFromRadioDataRate(-1) < getRealDataRateFromRadioDataRate(nRateTx) )
            nRateTx = -1;
      }  
   }
   return nRateTx;
}

int video_stats_overwrites_get_next_level_down_radio_datarate_video(int iRadioLink, int iRadioInterface)
{
   int nRateTx = video_stats_overwrites_get_current_radio_datarate_video(iRadioLink, iRadioInterface);
 
   nRateTx = _video_stats_overwrites_get_lower_datarate_value(nRateTx, 1);
   return nRateTx;
}
