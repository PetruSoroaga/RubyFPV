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
#include "../base/models.h"
#include "../radio/radiopacketsqueue.h"

#include "shared_vars.h"
#include "timers.h"

#include "video_link_adaptive.h"
#include "video_link_keyframe.h"
#include "processor_rx_video.h"

extern t_packet_queue s_QueueRadioPackets;

u32 s_uPauseAdjustmensUntilTime = 0;

void video_link_adaptive_init()
{
   memset((u8*)&g_ControllerVehiclesAdaptiveVideoInfo, 0, sizeof(shared_mem_controller_vehicles_adaptive_video_info));

   if ( NULL != g_pCurrentModel )
   {
      g_ControllerVehiclesAdaptiveVideoInfo.iCountVehicles = 1;
      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uVehicleId = g_pCurrentModel->vehicle_id;
   }

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[i].uUpdateInterval = 20;
      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[i].uLastUpdateTime = g_TimeNow;

      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[i].iLastRequestedLevelShift = -1; // Wait for video stream to decide the initial value
      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[i].iLastAcknowledgedLevelShift = -1;
      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[i].iLastRequestedLevelShiftRetryCount = 0;
   }
   s_uPauseAdjustmensUntilTime = 0;
   log_line("Initialized adaptive video info.");
   video_link_keyframe_init();
}

void video_line_adaptive_switch_to_med_level()
{
   video_link_adaptive_init();

   if ( NULL == g_pCurrentModel )
      return;

   int adaptiveVideoIsOn = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
   if ( ! adaptiveVideoIsOn )
      return;

   int iLevelsHQ = g_pCurrentModel->get_video_link_profile_max_level_shifts(g_pCurrentModel->video_params.user_selected_video_link_profile);
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = iLevelsHQ+1;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastAcknowledgedLevelShift = -1;
   s_uPauseAdjustmensUntilTime = g_TimeNow + 3000;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftDown = g_TimeNow+3000;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftUp = g_TimeNow+3000;
    
   log_line("Video adaptive: reset to MQ level.");
}

void video_link_adaptive_set_intial_video_adjustment_level(int iCurrentVideoProfile, u8 uDataPackets, u8 uECPackets)
{
    if ( iCurrentVideoProfile < 0 || iCurrentVideoProfile >= MAX_VIDEO_LINK_PROFILES )
       iCurrentVideoProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
    
    if ( iCurrentVideoProfile == g_pCurrentModel->video_params.user_selected_video_link_profile )
       g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = 0;
    else if ( iCurrentVideoProfile == VIDEO_PROFILE_MQ )
       g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = 1 + g_pCurrentModel->get_video_link_profile_max_level_shifts(g_pCurrentModel->video_params.user_selected_video_link_profile);
    else
       g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = 2 + g_pCurrentModel->get_video_link_profile_max_level_shifts(g_pCurrentModel->video_params.user_selected_video_link_profile) + g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_MQ);

    g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift += ((int)uECPackets - g_pCurrentModel->video_link_profiles[iCurrentVideoProfile].block_fecs);
    g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastAcknowledgedLevelShift = g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift;
    g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShiftRetryCount = 0;
    g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftDown = g_TimeNow+1000;
    g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftUp = g_TimeNow+1000;
    g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedLevelShift = g_TimeNow+1000;
    log_line("Adaptive video set initial/current adaptive video level to %d, from received video stream.", g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift);
}

void _video_link_adaptive_check_send_to_vehicle()
{
   if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift == -1 )
      return;

   if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift == g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastAcknowledgedLevelShift )
      return;
   if ( g_TimeNow < g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedLevelShift + g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval + g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShiftRetryCount*10 )
      return;

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_VIDEO;
   PH.packet_type =  PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + sizeof(u32);
   PH.extra_flags = 0;

   u32 uLevel = (u32) g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift;
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), (u8*)&uLevel, sizeof(u32));
   packets_queue_inject_packet_first(&s_QueueRadioPackets, packet);

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedLevelShift = g_TimeNow;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShiftRetryCount++;
   if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShiftRetryCount > 100 )
      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShiftRetryCount = 20;
}

void _video_link_adaptive_check_adjust_video_params()
{
   int adaptiveVideoIsOn = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
   if ( ! adaptiveVideoIsOn )
      return;

   _video_link_adaptive_check_send_to_vehicle();

   //float fParamsChangeStrength = 0.9*(float)g_pCurrentModel->video_params.videoAdjustmentStrength / 10.0;
   
   if ( g_TimeNow < g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftDown + 50 )
      return;
   if ( g_TimeNow < g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftUp + 50 )
      return;

   int iLevelsHQ = g_pCurrentModel->get_video_link_profile_max_level_shifts(g_pCurrentModel->video_params.user_selected_video_link_profile);
   int iLevelsMQ = g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_MQ);
   int iLevelsLQ = g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_LQ);
   int iMaxLevels = 1 + iLevelsHQ;
   iMaxLevels += 1 + iLevelsMQ;
   if ( ! (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO) )
      iMaxLevels += 1 + iLevelsLQ;

   float fParamsChangeStrength = (float)g_pCurrentModel->video_params.videoAdjustmentStrength / 10.0;
   
   // When on HQ video profile, switch faster to MQ video profile;
   
   if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift < iLevelsHQ )
   {
      fParamsChangeStrength += 0.2;
      if ( fParamsChangeStrength > 1.0 )
         fParamsChangeStrength = 1.0;
   }

   // Max intervals is MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS * 20 ms = 4 seconds
   // Minus one because the current index is still processing/invalid

   int iIntervalsToCheckDown = (1.0-0.4*fParamsChangeStrength) * MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS;
   int iIntervalsToCheckUp = (1.0-0.3*fParamsChangeStrength) * MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS;
   if ( iIntervalsToCheckDown >= MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1 )
      iIntervalsToCheckDown = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-2;
   if ( iIntervalsToCheckUp >= MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1 )
      iIntervalsToCheckUp = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-2;

   int iIntervalsSinceLastShiftDown = (g_TimeNow - g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftDown) / g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval;
   if ( iIntervalsToCheckDown > iIntervalsSinceLastShiftDown )
      iIntervalsToCheckDown = iIntervalsSinceLastShiftDown;
   if ( iIntervalsToCheckDown <= 0 )
      iIntervalsToCheckDown = 1;

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsAdaptive1 = ((u16)iIntervalsToCheckDown) | (((u16)iIntervalsToCheckUp)<<16);
   
   int iCountReconstructedDown = 0;
   int iLongestReconstructionDown = 0;
   int iCountRetransmissionsDown = 0;
   int iCountReRetransmissionsDown = 0;
   int iCountMissingSegmentsDown = 0;

   int iCountReconstructedUp = 0;
   int iLongestReconstructionUp = 0;
   int iCountRetransmissionsUp = 0;
   int iCountReRetransmissionsUp = 0;
   int iCountMissingSegmentsUp = 0;

   int iIndex = g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex - 1;
   if ( iIndex < 0 )
      iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;

   int iCurrentReconstructionLength = 0;

   for( int i=0; i<iIntervalsToCheckDown; i++ )
   {
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;
      iCountReconstructedDown += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsOuputRecontructedVideoPackets[iIndex]!=0)?1:0;
      iCountRetransmissionsDown += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsRequestedRetransmissions[iIndex]!=0)?1:0;
      iCountReRetransmissionsDown += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsRetriedRetransmissions[iIndex]!=0)?1:0;
      iCountMissingSegmentsDown += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsMissingVideoPackets[iIndex]!=0)?1:0;
   
      if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsOuputRecontructedVideoPackets[iIndex] != 0 )
         iCurrentReconstructionLength++;
      else
      {
         if ( iCurrentReconstructionLength > iLongestReconstructionDown )
            iLongestReconstructionDown = iCurrentReconstructionLength;
         iCurrentReconstructionLength = 0;
      }
   }

   iCountReconstructedUp = iCountReconstructedDown;
   iCountRetransmissionsUp = iCountRetransmissionsDown;
   iCountReRetransmissionsUp = iCountReRetransmissionsDown;
   iCountMissingSegmentsUp = iCountMissingSegmentsDown;
   iLongestReconstructionUp = iLongestReconstructionDown;

   for( int i=iIntervalsToCheckDown; i<iIntervalsToCheckUp; i++ )
   {
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;
      iCountReconstructedUp += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsOuputRecontructedVideoPackets[iIndex]!=0)?1:0;
      iCountRetransmissionsUp += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsRequestedRetransmissions[iIndex]!=0)?1:0;
      iCountReRetransmissionsUp += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsRetriedRetransmissions[iIndex]!=0)?1:0;
      iCountMissingSegmentsUp += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsMissingVideoPackets[iIndex]!=0)?1:0;
   
      if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsOuputRecontructedVideoPackets[iIndex] != 0 )
         iCurrentReconstructionLength++;
      else
      {
         if ( iCurrentReconstructionLength > iLongestReconstructionUp )
            iLongestReconstructionUp = iCurrentReconstructionLength;
         iCurrentReconstructionLength = 0;
      }
   }

   // Check for shift down

   int iThresholdReconstructedDown = 2 + (1.0-fParamsChangeStrength)*iIntervalsToCheckDown;
   int iThresholdRetransmissionsDown = 2 + (1.0-fParamsChangeStrength)*iIntervalsToCheckDown;
   int iThresholdLongestRecontructionDown = 2 + (1.0-fParamsChangeStrength)*iIntervalsToCheckDown/2.0;

   u32 uMinTimeSinceLastShift = iIntervalsToCheckDown * g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval;
   if ( iThresholdReconstructedDown * g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval < uMinTimeSinceLastShift )
      uMinTimeSinceLastShift = iThresholdReconstructedDown * g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval;
   if ( iThresholdRetransmissionsDown * g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval < uMinTimeSinceLastShift )
      uMinTimeSinceLastShift = iThresholdRetransmissionsDown * g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval;

   if ( uMinTimeSinceLastShift < 70 )
      uMinTimeSinceLastShift = 70;
   if ( uMinTimeSinceLastShift > 500 )
      uMinTimeSinceLastShift = 500;
   if ( g_TimeNow > g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftDown + uMinTimeSinceLastShift )
   {
      if ( (iCountReconstructedDown > iThresholdReconstructedDown) ||
           (iLongestReconstructionDown > iThresholdLongestRecontructionDown) )
      {
         g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftDown = g_TimeNow;
         g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift++;
         
         // Skip data:fec == 1:1 leves
         if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift == iLevelsHQ )
            g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift++;
         if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift == iLevelsHQ + iLevelsMQ + 1 )
            g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift++;
         if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift == iLevelsHQ + iLevelsMQ + iLevelsLQ + 2 )
            g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift++;

         if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift >= iMaxLevels - 1 )
            g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = iMaxLevels - 1;
      }

      if ( (iCountRetransmissionsDown > iThresholdRetransmissionsDown) ||
           (iCountReRetransmissionsDown > 0) )
      {
         g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftDown = g_TimeNow;
         if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift <= iLevelsHQ+1 )
            g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = iLevelsHQ+2;
         else if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift <= iLevelsHQ+1 + iLevelsMQ+1)
            g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = iLevelsHQ+iLevelsMQ+3;
         else 
            g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = iMaxLevels - 1;

         if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift >= iMaxLevels - 1 )
            g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = iMaxLevels - 1;
      }
   }

   // Check for shift up ?
   // Only if we did not shifted down or up recently

   int iThresholdReconstructedUp = 2 + (1.0-fParamsChangeStrength)*14.0;
   int iThresholdLongestRecontructionUp = 2 + (1.0-fParamsChangeStrength)*6.0;
   u32 uTimeForShiftUp = 1000 - (500.0*fParamsChangeStrength);
   if ( uTimeForShiftUp < 100 )
      uTimeForShiftUp = 100;
   if ( uTimeForShiftUp > 1000 )
      uTimeForShiftUp = 1000;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsAdaptive2 = ((u32)iThresholdReconstructedUp) | (((u32)iThresholdLongestRecontructionUp)<<8);
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsAdaptive2 |= (((u32)iThresholdReconstructedDown)<<16) | (((u32)iThresholdLongestRecontructionDown)<<24);
   
   if ( g_TimeNow > g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftDown + uTimeForShiftUp )
   if ( g_TimeNow > g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftUp + uTimeForShiftUp )
   {
      if ( iCountReconstructedUp < iThresholdReconstructedUp )
      if ( iLongestReconstructionUp < iThresholdLongestRecontructionUp )
      if ( iCountRetransmissionsUp < 5 )
      {
         g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastLevelShiftUp = g_TimeNow;
         if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift > 0 )
         {
            g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift--;
            // Skip data:fec == 1:1 leves
            if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift == iLevelsHQ )
               g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift--;
            if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift == iLevelsHQ + iLevelsMQ + 1 )
               g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift--;
            if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift == iLevelsHQ + iLevelsMQ + iLevelsLQ + 2 )
               g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift--;
         }
         if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift < 0 )
            g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = 0;
      }
   }   
}

void video_link_adaptive_periodic_loop()
{
   if ( g_bSearching || NULL == g_pCurrentModel || g_bUpdateInProgress )
      return;
   if ( g_pCurrentModel->is_spectator )
      return;

   int adaptiveVideoIsOn = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
   int adaptiveKeyframe = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe > 0 ? 0 : 1;

   #ifdef FEATURE_VEHICLE_COMPUTES_ADAPTIVE_VIDEO
   int useControllerInfo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?1:0;
   #else
   int useControllerInfo = 1;
   #endif

   if ( ! adaptiveVideoIsOn )
   {
       if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift != 0 )
         g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift = 0;
       if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastAcknowledgedLevelShift != 0 )
          _video_link_adaptive_check_send_to_vehicle();
   }

   if ( g_TimeNow < g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uLastUpdateTime + g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval )
      return;

   if ( (! adaptiveVideoIsOn) && (! adaptiveKeyframe) )
      return;
   
   // Update info

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uLastUpdateTime = g_TimeNow;

   // Compute total missing video packets as of now

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iChangeStrength = g_pCurrentModel->video_params.videoAdjustmentStrength;

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsMissingVideoPackets[g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex] = (u16) process_data_rx_video_get_missing_video_packets_now();

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex++;
   if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex >= MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS )
      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex = 0;

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsOuputCleanVideoPackets[g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex] = 0;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsOuputRecontructedVideoPackets[g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex] = 0;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsMissingVideoPackets[g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex] = 0;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsRequestedRetransmissions[g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex] = 0;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsRetriedRetransmissions[g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex] = 0;
   
   // Do adaptive video calculations only after we start receiving the video stream
   // (a valid initial last requested level shift is obtained from the received video stream)

   if ( (0 != s_uPauseAdjustmensUntilTime) && (g_TimeNow < s_uPauseAdjustmensUntilTime) )
   {
      _video_link_adaptive_check_send_to_vehicle();
   }
   else if ( adaptiveVideoIsOn && useControllerInfo )
   {
      if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedLevelShift != -1 )
         _video_link_adaptive_check_adjust_video_params();
   }

   video_link_keyframe_periodic_loop();

   if ( NULL != g_pSM_ControllerVehiclesAdaptiveVideoInfo )
   if ( (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex % 5) == 0 )
      memcpy((u8*)g_pSM_ControllerVehiclesAdaptiveVideoInfo, (u8*)&g_ControllerVehiclesAdaptiveVideoInfo, sizeof(shared_mem_controller_vehicles_adaptive_video_info));
}
