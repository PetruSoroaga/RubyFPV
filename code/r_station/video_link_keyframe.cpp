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
#include "../common/string_utils.h"
#include "../radio/radiopacketsqueue.h"

#include "shared_vars.h"
#include "timers.h"

#include "video_link_adaptive.h"
#include "processor_rx_video.h"

extern t_packet_queue s_QueueRadioPackets;

u32 s_uTimeLastKeyFrameCheck = 0;

static bool s_bReceivedKeyFrameFromVideoStream = false;

void video_link_keyframe_init()
{
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame = -1;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastAcknowledgedKeyFrame = -1;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrameRetryCount = 0;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedKeyFrame = 0;
   s_bReceivedKeyFrameFromVideoStream = false;
   log_line("Initialized adaptive video keyframe info, default start keyframe interval: %d", g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame);
}

void video_link_keyframe_set_intial_received_level(int iReceivedKeyframe)
{
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame = iReceivedKeyframe;
   
   if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame < 0 )
       g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps * g_pCurrentModel->video_params.uMaxAutoKeyframeInterval / 1000;

   log_line("Initial keyframe received from the video stream: %d", g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame); 
   
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastAcknowledgedKeyFrame = g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrameRetryCount = 0;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedKeyFrame = g_TimeNow;
   s_bReceivedKeyFrameFromVideoStream = true;
   log_line("Done setting initial keyframe state.");
}

void video_link_keyframe_set_current_level(int iKeyframe)
{
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame = iKeyframe;
   
   if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame < 0 )
       g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps * g_pCurrentModel->video_params.uMaxAutoKeyframeInterval / 1000;

   log_line("Set current keyframe interval: %d", g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame); 
   
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastAcknowledgedKeyFrame = g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrameRetryCount = 0;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedKeyFrame = g_TimeNow;
   s_bReceivedKeyFrameFromVideoStream = true;
   log_line("Done setting current keyframe state.");
}

void _video_link_keyframe_check_send_to_vehicle()
{
   if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame == -1 )
      return;

   if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame == g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastAcknowledgedKeyFrame )
      return;

   if ( g_TimeNow < g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedKeyFrame + g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval + g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrameRetryCount*10 )
     return;

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_VIDEO;
   PH.packet_type =  PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + sizeof(u32);
   PH.extra_flags = 0;

   u32 uKeyframe = (u32) g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame;
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), (u8*)&uKeyframe, sizeof(u32));
   packets_queue_inject_packet_first(&s_QueueRadioPackets, packet);

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedKeyFrame = g_TimeNow;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrameRetryCount++;
   if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrameRetryCount > 100 )
      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrameRetryCount = 20;
}

void _video_link_keyframe_request_new_keyframe_interval(int iInterval)
{
   if ( iInterval < 2 )
      iInterval = 2;
   if ( iInterval > 1000 )
      iInterval = 1000;

   if ( iInterval == g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame )
      return;

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame = iInterval;
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrameRetryCount = 0;

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedKeyFrame = g_TimeNow - g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval - 1;
}

void video_link_keyframe_periodic_loop()
{
   if ( g_bSearching || NULL == g_pCurrentModel || g_bUpdateInProgress )
      return;
   if ( g_pCurrentModel->is_spectator )
      return;
   if ( ! s_bReceivedKeyFrameFromVideoStream )
      return;
     
   int iCurrentVideoProfile = process_data_rx_video_get_current_received_video_profile();
   if ( iCurrentVideoProfile == -1 )
      iCurrentVideoProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
   
   int iCurrentProfileKeyFrame = g_pCurrentModel->video_link_profiles[iCurrentVideoProfile].keyframe;

   int iCurrentFPS = process_data_rx_video_get_current_received_video_fps();
   if ( iCurrentFPS < 1 )
      iCurrentFPS = g_pCurrentModel->video_link_profiles[iCurrentVideoProfile].fps;


   // User set a fixed keyframe value? Then do nothing (just set it if not set);

   if ( iCurrentProfileKeyFrame > 0 )
   {
      if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame != iCurrentProfileKeyFrame )
         g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame = iCurrentProfileKeyFrame;

      if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame == g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastAcknowledgedKeyFrame )
         return;

      _video_link_keyframe_check_send_to_vehicle();

      return;
   }

   _video_link_keyframe_check_send_to_vehicle();

   if ( g_TimeNow < g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedKeyFrame + 50 )
      return;

   if ( g_TimeNow < s_uTimeLastKeyFrameCheck + 100 )
      return;

   s_uTimeLastKeyFrameCheck = g_TimeNow;

   float fParamsChangeStrength = (float)g_pCurrentModel->video_params.videoAdjustmentStrength / 10.0;
   
   // Max intervals is MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS * 20 ms = 4 seconds
   // Minus one because the current index is still processing/invalid

   int iIntervalsToCheckDown = 3 + (1.0-0.7*fParamsChangeStrength) * MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS;
   int iIntervalsToCheckUp = iIntervalsToCheckDown + 20 + 40 * (1.0-fParamsChangeStrength);
   if ( iIntervalsToCheckDown >= MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1 )
      iIntervalsToCheckDown = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-2;
   if ( iIntervalsToCheckUp >= MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1 )
      iIntervalsToCheckUp = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-2;
   
   int iThresholdUp = 1 + (0.1 + 0.08*(0.9-fParamsChangeStrength)) * iIntervalsToCheckUp;
   int iThresholdDownLevel1 = 1 + (0.01 + 0.08 * (1.0-fParamsChangeStrength) )* iIntervalsToCheckDown;
   int iThresholdDownLevel2 = 1 + (0.04 + 0.16 * (1.0-fParamsChangeStrength) )* iIntervalsToCheckDown;
   int iThresholdDownLevel4 = (0.03 + 0.2*(1.0-fParamsChangeStrength)) * iIntervalsToCheckDown;
   int iThresholdDownLevel5 = (0.03 + 0.1*(1.0-fParamsChangeStrength))* iIntervalsToCheckDown;


   //if ( (int)(g_TimeNow - g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastKFShiftUp) / (int)g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval < iIntervalsToCheckUp )
   //   iIntervalsToCheckUp = (g_TimeNow - g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastKFShiftUp) / g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval;

   if ( (int)(g_TimeNow - g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastKFShiftDown) / (int)g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval < iIntervalsToCheckDown )
      iIntervalsToCheckDown = (g_TimeNow - g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastKFShiftDown) / g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uUpdateInterval;
   
   if ( iIntervalsToCheckDown < 1 )
      iIntervalsToCheckDown = 1;
   if ( iIntervalsToCheckUp < iIntervalsToCheckDown )
      iIntervalsToCheckUp = iIntervalsToCheckDown;

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsKeyFrame1 = ((u16)iIntervalsToCheckDown) | (((u16)iIntervalsToCheckUp)<<16);
   int iCountReconstructedDown = 0;
   int iCountRetransmissionsDown = 0;
   int iCountReRetransmissionsDown = 0;
   int iCountMissingSegmentsDown = 0;

   int iCountReconstructedUp = 0;
   int iCountRetransmissionsUp = 0;
   int iCountReRetransmissionsUp = 0;
   int iCountMissingSegmentsUp = 0;

   int iIndex = g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uCurrentIntervalIndex - 1;
   if ( iIndex < 0 )
      iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS;

   for( int i=0; i<iIntervalsToCheckDown; i++ )
   {
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS;
      iCountReconstructedDown += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsOuputRecontructedVideoPackets[iIndex]!=0)?1:0;
      iCountRetransmissionsDown += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsRequestedRetransmissions[iIndex]!=0)?1:0;
      iCountReRetransmissionsDown += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsRetriedRetransmissions[iIndex]!=0)?1:0;
      iCountMissingSegmentsDown += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsMissingVideoPackets[iIndex]!=0)?1:0;
   }

   iCountReconstructedUp = iCountReconstructedDown;
   iCountRetransmissionsUp = iCountRetransmissionsDown;
   iCountReRetransmissionsUp = iCountReRetransmissionsDown;
   iCountMissingSegmentsUp = iCountMissingSegmentsDown;

   for( int i=iIntervalsToCheckDown; i<iIntervalsToCheckUp; i++ )
   {
      iIndex--;
      if ( iIndex < 0 )
         iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS;
      iCountReconstructedUp += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsOuputRecontructedVideoPackets[iIndex]!=0)?1:0;
      iCountRetransmissionsUp += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsRequestedRetransmissions[iIndex]!=0)?1:0;
      iCountReRetransmissionsUp += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsRetriedRetransmissions[iIndex]!=0)?1:0;
      iCountMissingSegmentsUp += (g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsMissingVideoPackets[iIndex]!=0)?1:0;
   }

   int iNewKeyFrameInterval = -1;

   // Check move up

   if ( g_TimeNow > g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastRequestedKeyFrame + 900 )
   if ( iCountReconstructedUp < iThresholdUp )
   if ( iCountRetransmissionsUp < iThresholdUp )
   if ( iCountReRetransmissionsUp < iThresholdUp )
   if ( iCountMissingSegmentsUp < iThresholdUp )
   {
      iNewKeyFrameInterval = g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame * 2;
      if ( iNewKeyFrameInterval > iCurrentFPS * DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL / 1000 )
         iNewKeyFrameInterval = iCurrentFPS * DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL / 1000;
      if ( iNewKeyFrameInterval != g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame )
      {
         _video_link_keyframe_request_new_keyframe_interval(iNewKeyFrameInterval);
         g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastKFShiftUp = g_TimeNow;
      }
   }

   // Check move down

   if ( iCountReconstructedDown > iThresholdDownLevel1 )
   {
      if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame > iCurrentFPS * DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL / 1000 / 2)
      {
         iNewKeyFrameInterval = iCurrentFPS * DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL / 1000 / 2;
         _video_link_keyframe_request_new_keyframe_interval(iNewKeyFrameInterval);
         g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastKFShiftDown = g_TimeNow;
      }
   }

   if ( iCountReconstructedDown > iThresholdDownLevel2 )
   {
      if ( g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].iLastRequestedKeyFrame > iCurrentFPS * DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL / 1000 / 4)
      {
         iNewKeyFrameInterval = iCurrentFPS * DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL / 1000 / 4;
         _video_link_keyframe_request_new_keyframe_interval(iNewKeyFrameInterval);
         g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastKFShiftDown = g_TimeNow;
      }
   }

   if ( (iCountRetransmissionsDown > iThresholdDownLevel4) ||
        (iCountMissingSegmentsDown > iThresholdDownLevel4) )
   {
      iNewKeyFrameInterval = 2 * iCurrentFPS * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL / 1000;
      _video_link_keyframe_request_new_keyframe_interval(iNewKeyFrameInterval);
      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastKFShiftDown = g_TimeNow;
   }

   if ( iCountReRetransmissionsDown > iThresholdDownLevel5 )
   {
      iNewKeyFrameInterval = iCurrentFPS * DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL / 1000;
      _video_link_keyframe_request_new_keyframe_interval(iNewKeyFrameInterval);
      g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uTimeLastKFShiftDown = g_TimeNow;
   }

   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsKeyFrame2 = ((u8)iThresholdUp) | (((u8)iThresholdDownLevel1)<<8) | (((u8)iThresholdDownLevel2)<<16);
   g_ControllerVehiclesAdaptiveVideoInfo.vehicles[0].uIntervalsKeyFrame3 = ((u8)iThresholdDownLevel4) | (((u8)iThresholdDownLevel5)<<8);
}

