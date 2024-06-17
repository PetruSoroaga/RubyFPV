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
#include "../base/models.h"
#include "../base/models_list.h"
#include "../common/string_utils.h"
#include "../common/relay_utils.h"
#include "../radio/radiopacketsqueue.h"

#include "shared_vars.h"
#include "timers.h"

#include "video_link_adaptive.h"
#include "processor_rx_video.h"

extern t_packet_queue s_QueueRadioPackets;

void video_link_keyframe_init(u32 uVehicleId)
{
   Model* pModel = findModelWithId(uVehicleId, 150);
   if ( NULL == pModel )
      return;

   int iInfoIndex = getVehicleRuntimeIndex(uVehicleId);
   if ( -1 == iInfoIndex )
      return;

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs = -1;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastAcknowledgedKeyFrameMs = -1;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMsRetryCount = 0;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].uTimeLastRequestedKeyFrame = 0;
   g_State.vehiclesRuntimeInfo[iInfoIndex].bReceivedKeyframeInfoInVideoStream = false;
   log_line("Initialized adaptive video keyframe info, default start keyframe interval (last requested): %d ms, for VID %u (name: %s)", g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[0].iLastRequestedKeyFrameMs, uVehicleId, pModel->getLongName());
}

void video_link_keyframe_set_intial_received_level(u32 uVehicleId, int iReceivedKeyframeMs)
{
   Model* pModel = findModelWithId(uVehicleId, 151);
   if ( NULL == pModel )
      return;

   int iInfoIndex = getVehicleRuntimeIndex(uVehicleId);
   if ( -1 == iInfoIndex )
      return;

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs = iReceivedKeyframeMs;
   
   if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs < 0 )
       g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs = pModel->video_params.uMaxAutoKeyframeIntervalMs;

   log_line("Initial keyframe received from the video stream: %d ms", g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs); 
   
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastAcknowledgedKeyFrameMs = g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMsRetryCount = 0;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].uTimeLastRequestedKeyFrame = g_TimeNow;
   g_State.vehiclesRuntimeInfo[iInfoIndex].bReceivedKeyframeInfoInVideoStream = true;
   log_line("Done setting initial keyframe state for VID %u based on received keframe interval of %d ms from video stream.", uVehicleId, iReceivedKeyframeMs);

}

void video_link_keyframe_set_current_level_to_request(u32 uVehicleId, int iKeyframeMs)
{
   Model* pModel = findModelWithId(uVehicleId, 152);
   if ( NULL == pModel )
      return;

   int iInfoIndex = getVehicleRuntimeIndex(uVehicleId);

   if ( -1 == iInfoIndex )
      return;

   g_State.vehiclesRuntimeInfo[iInfoIndex].bReceivedKeyframeInfoInVideoStream = true;
   
   if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs == iKeyframeMs )
      return;

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs = iKeyframeMs;
   
   if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs < 0 )
       g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs = pModel->video_params.uMaxAutoKeyframeIntervalMs;

   log_line("Set current keyframe interval to request from VID %u to %d ms", uVehicleId, g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs);
   
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMsRetryCount = 0;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].uTimeLastRequestedKeyFrame = g_TimeNow;
}

void _video_link_keyframe_check_send_to_vehicle(u32 uVehicleId)
{
   Model* pModel = findModelWithId(uVehicleId, 153);
   if ( NULL == pModel )
      return;

   int iInfoIndex = getVehicleRuntimeIndex(uVehicleId);
   if ( -1 == iInfoIndex )
      return;
   if ( ! g_State.vehiclesRuntimeInfo[iInfoIndex].bIsPairingDone )
      return;
   if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs == -1 )
      return;

   if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs == g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastAcknowledgedKeyFrameMs )
      return;

   if ( g_TimeNow < g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].uTimeLastRequestedKeyFrame + g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].uUpdateInterval + g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMsRetryCount*10 )
     return;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_VIDEO, PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = uVehicleId;
   PH.total_length = sizeof(t_packet_header) + sizeof(u32) + sizeof(u8);

   u32 uKeyframeMs = (u32) g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs;
   u8 uVideoStreamIndex = 0;
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), (u8*)&uKeyframeMs, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header) + sizeof(u32), (u8*)&uVideoStreamIndex, sizeof(u8));
   packets_queue_inject_packet_first(&s_QueueRadioPackets, packet);

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].uTimeLastRequestedKeyFrame = g_TimeNow;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMsRetryCount++;
   if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMsRetryCount > 100 )
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMsRetryCount = 20;
}

void _video_link_keyframe_request_new_keyframe_interval(u32 uVehicleId, int iIntervalMs)
{
   if ( iIntervalMs < DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL )
      iIntervalMs = DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL;
   if ( iIntervalMs > 20000 )
      iIntervalMs = 20000;

   Model* pModel = findModelWithId(uVehicleId, 154);
   if ( NULL == pModel )
      return;

   if ( iIntervalMs > (int)pModel->video_params.uMaxAutoKeyframeIntervalMs )
      iIntervalMs = pModel->video_params.uMaxAutoKeyframeIntervalMs;

   int iInfoIndex = getVehicleRuntimeIndex(uVehicleId);
   if ( -1 == iInfoIndex )
      return;

   if ( iIntervalMs == g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs )
      return;

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMs = iIntervalMs;
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].iLastRequestedKeyFrameMsRetryCount = 0;

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].uTimeLastRequestedKeyFrame = g_TimeNow - g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iInfoIndex].uUpdateInterval - 1;
}

void _video_link_keyframe_do_adaptive_check(int iVehicleIndex)
{
   Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[iVehicleIndex].uVehicleId, 157);
   if ( (NULL == pModel) || (pModel->is_spectator) )
      return;

   ProcessorRxVideo* pProcessorVideo = NULL;
   for( int i=0; i<MAX_VIDEO_PROCESSORS; i++ )
   {
      if ( NULL != g_pVideoProcessorRxList[i] )
      if ( g_pVideoProcessorRxList[i]->m_uVehicleId == g_State.vehiclesRuntimeInfo[iVehicleIndex].uVehicleId )
      {
         pProcessorVideo = g_pVideoProcessorRxList[i];
         break;
      }
   }

   // Max interval time we can check is:
   // MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS * CONTROLLER_ADAPTIVE_VIDEO_SAMPLE_INTERVAL ms
   //    = 3.6 seconds
   // Minus one because the current index is still processing/invalid

   int iIntervalsToCheckDown = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;
   int iIntervalsToCheckUp = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;
   
   int iIntervalsSinceLastShiftDown = (int)(g_TimeNow - g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastKFShiftDown) / (int)g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uUpdateInterval;
   if ( iIntervalsSinceLastShiftDown < iIntervalsToCheckDown )
      iIntervalsToCheckDown = iIntervalsSinceLastShiftDown;
   
   if ( iIntervalsToCheckDown < 0 )
      iIntervalsToCheckDown = 0;

   int iIntervalsSinceLastShiftUp = (int)(g_TimeNow - g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastKFShiftUp) / (int)g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uUpdateInterval;
   if ( iIntervalsSinceLastShiftUp < iIntervalsToCheckUp )
      iIntervalsToCheckUp = iIntervalsSinceLastShiftUp;
   
   if ( iIntervalsToCheckUp < 0 )
      iIntervalsToCheckUp = 0;

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uStats_CheckIntervalsKeyFrame = ((u16)iIntervalsToCheckDown) | (((u16)iIntervalsToCheckUp)<<16);
   
   // videoAdjustmentStrength is 1 (lowest strength) to 10 (highest strength)
   // fParamsChangeStrength: higher value means more aggresive adjustments. From 0.1 to 1
   float fParamsChangeStrength = (float)pModel->video_params.videoAdjustmentStrength / 10.0;

   int iThresholdDownLevelSlow = 2 + 20.0 * (1.0-fParamsChangeStrength);
   int iThresholdDownLevelFast = 5 + 30.0 * (1.0-fParamsChangeStrength);
   int iThresholdDownLevelRetrSlow = 1 + 5.0 * (1.0-fParamsChangeStrength);
   int iThresholdDownLevelRetrFast = 2 + 10.0 * (1.0-fParamsChangeStrength);

   // Up levels thresholds must be smaller than down levels thresholds
   int iThresholdUpLevel = iThresholdDownLevelSlow - 2;
   if ( iThresholdUpLevel < 1 )
      iThresholdUpLevel = 1;
   int iThresholdUpLevelRetr = iThresholdDownLevelRetrSlow-2;
   if ( iThresholdUpLevelRetr < 1 )
      iThresholdUpLevelRetr = 1;

   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uStats_KeyFrameThresholdIntervalsUp = (iThresholdUpLevel & 0xFF) | ((iThresholdUpLevelRetr & 0xFF) << 8);
   g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uStats_KeyFrameThresholdIntervalsDown = (iThresholdDownLevelSlow & 0xFF) | ((iThresholdDownLevelFast & 0xFF) << 8) | ((iThresholdDownLevelRetrSlow & 0xFF) << 16) | ((iThresholdDownLevelRetrFast & 0xFF)<<24);

   // Check if we need to down shift keyframe (decrease keyframe interval)

   if ( (NULL != pProcessorVideo) && (pProcessorVideo->getLastTimeReceivedVideoPacket() < g_TimeNow - (10-pModel->video_params.videoAdjustmentStrength)*20) )
   {
      g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAG_NO_VIDEO_PACKETS;
      if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastRequestedKeyFrameMs > DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL )
      {
         int iNewKeyFrameIntervalMs = g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastRequestedKeyFrameMs / 2;
         if ( iNewKeyFrameIntervalMs < DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL )
            iNewKeyFrameIntervalMs = DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL;
         if ( iNewKeyFrameIntervalMs != g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastRequestedKeyFrameMs )
         {
            _video_link_keyframe_request_new_keyframe_interval(pModel->uVehicleId, iNewKeyFrameIntervalMs);
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastKFShiftDown = g_TimeNow;
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAGS_KEYFRAME_SHIFTED;
         }
      }
      return;
   }

   if ( g_TimeNow > g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastRequestedKeyFrame + 100 )
   if ( g_TimeNow > g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastKFShiftDown + 100 )
   if ( g_TimeNow > g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastKFShiftUp + 100 )
   if ( iIntervalsToCheckDown > 0 )
   {
      int iCountReconstructed = 0;
      int iCountRetransmissions = 0;
      int iCountReRetransmissions = 0;
      int iCountMissingSegments = 0;

      int iIndex = g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex;
      for( int k=0; k<=iIntervalsToCheckDown; k++ )
      {
         iCountReconstructed += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsOuputRecontructedVideoPackets[iIndex]!=0)?1:0;
         iCountRetransmissions += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsRequestedRetransmissions[iIndex]!=0)?1:0;
         iCountReRetransmissions += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsRetriedRetransmissions[iIndex]!=0)?1:0;
         iCountMissingSegments += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsMissingVideoPackets[iIndex]!=0)?1:0;
         iIndex--;
         if ( iIndex < 0 )
            iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;
      }

      int iNewKeyFrameIntervalMs = g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastRequestedKeyFrameMs;
      // Check larger thresholds first
      
      if ( iIntervalsToCheckDown >= iThresholdDownLevelSlow )
      if ( iCountReconstructed >= iThresholdDownLevelSlow )
      if ( iNewKeyFrameIntervalMs > 4*DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL )
      {
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAGS_SHIFT_DOWN;
         iNewKeyFrameIntervalMs = (iNewKeyFrameIntervalMs * 3)/4;
      }

      if ( iIntervalsToCheckDown >= iThresholdDownLevelFast )
      if ( iCountReconstructed >= iThresholdDownLevelFast )
      {
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAGS_SHIFT_DOWN_FAST;
         iNewKeyFrameIntervalMs = iNewKeyFrameIntervalMs/2;
      }

      if ( iIntervalsToCheckDown >= iThresholdDownLevelRetrSlow )
      if ( (iCountRetransmissions >= iThresholdDownLevelRetrSlow) || (iCountMissingSegments >= iThresholdDownLevelRetrSlow/2+1) )
      if ( iNewKeyFrameIntervalMs > 2*DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL )
      {
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAGS_SHIFT_DOWN;
         iNewKeyFrameIntervalMs = 2*DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL;
      }

      if ( iIntervalsToCheckDown >= iThresholdDownLevelRetrFast )
      if ( (iCountRetransmissions >= iThresholdDownLevelRetrFast) || (iCountMissingSegments >= iThresholdDownLevelRetrFast/2+1) )
      {
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAGS_SHIFT_DOWN_FAST;
         iNewKeyFrameIntervalMs = DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL;
      }

      // Apply the change, if any

      if ( iNewKeyFrameIntervalMs < DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL )
         iNewKeyFrameIntervalMs = DEFAULT_VIDEO_MIN_AUTO_KEYFRAME_INTERVAL;
      
      if ( iNewKeyFrameIntervalMs != g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastRequestedKeyFrameMs )
      {
         _video_link_keyframe_request_new_keyframe_interval(pModel->uVehicleId, iNewKeyFrameIntervalMs);
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastKFShiftDown = g_TimeNow;
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAGS_KEYFRAME_SHIFTED;
      }
   }

   // Check if we need to up shift keyframe (increase keyframe interval )

   if ( g_TimeNow > g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastRequestedKeyFrame + 900 )
   if ( g_TimeNow > g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastKFShiftUp + 900 )
   if ( g_TimeNow > g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastKFShiftDown + 900 )
   if ( iIntervalsToCheckUp >= iThresholdUpLevel )
   {
      int iCountReconstructed = 0;
      int iCountRetransmissions = 0;
      int iCountReRetransmissions = 0;
      int iCountMissingSegments = 0;

      int iIndex = g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex;
      for( int k=0; k<=iIntervalsToCheckUp; k++ )
      {
         iCountReconstructed += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsOuputRecontructedVideoPackets[iIndex]!=0)?1:0;
         iCountRetransmissions += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsRequestedRetransmissions[iIndex]!=0)?1:0;
         iCountReRetransmissions += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsRetriedRetransmissions[iIndex]!=0)?1:0;
         iCountMissingSegments += (g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsMissingVideoPackets[iIndex]!=0)?1:0;
         iIndex--;
         if ( iIndex < 0 )
            iIndex = MAX_CONTROLLER_ADAPTIVE_VIDEO_INFO_INTERVALS-1;
      }

      if ( iIntervalsToCheckUp >= iThresholdUpLevel )
      if ( iCountReconstructed < iThresholdUpLevel )
      if ( iIntervalsToCheckUp >= iThresholdUpLevelRetr )
      if ( iCountRetransmissions < iThresholdUpLevelRetr )
      {
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAGS_SHIFT_UP;
         int iNewKeyFrameIntervalMs = g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastRequestedKeyFrameMs * 4 / 3;
         if ( iNewKeyFrameIntervalMs > (int)pModel->video_params.uMaxAutoKeyframeIntervalMs )
            iNewKeyFrameIntervalMs = pModel->video_params.uMaxAutoKeyframeIntervalMs;
         if ( iNewKeyFrameIntervalMs != g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastRequestedKeyFrameMs )
         {
            _video_link_keyframe_request_new_keyframe_interval(pModel->uVehicleId, iNewKeyFrameIntervalMs);
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastKFShiftUp = g_TimeNow;
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAGS_KEYFRAME_SHIFTED;
         }
      }

      /*
      if ( iIntervalsToCheckUp >= iThresholdUpLevelRetr )
      if ( iCountRetransmissions < iThresholdUpLevelRetr )
      {
         g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAGS_SHIFT_UP;
         int iNewKeyFrameIntervalMs = g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastRequestedKeyFrameMs * 4 / 3;
         if ( iNewKeyFrameIntervalMs > (int)pModel->video_params.uMaxAutoKeyframeIntervalMs )
            iNewKeyFrameIntervalMs = pModel->video_params.uMaxAutoKeyframeIntervalMs;
         if ( iNewKeyFrameIntervalMs != g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iLastRequestedKeyFrameMs )
         {
            _video_link_keyframe_request_new_keyframe_interval(pModel->uVehicleId, iNewKeyFrameIntervalMs);
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uTimeLastKFShiftUp = g_TimeNow;
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].uIntervalsFlags[g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[iVehicleIndex].iCurrentIntervalIndex] |= ADAPTIVE_STATS_FLAGS_KEYFRAME_SHIFTED;
         }
      }
      */
   }
}


void video_link_keyframe_periodic_loop()
{
   if ( g_bSearching || (NULL == g_pCurrentModel) || g_bUpdateInProgress )
      return;
     
   if ( g_bIsControllerLinkToVehicleLost || g_bIsVehicleLinkToControllerLost )
      return;
     
   if ( g_bDebugState )
      return;
     
   u32 s_uTimeLastKeyFrameCheckVehicles = 0;


   // Send/Resend keyframe to vehicle if needed

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( 0 == g_State.vehiclesRuntimeInfo[i].uVehicleId )
         continue;
      if ( ! g_State.vehiclesRuntimeInfo[i].bIsPairingDone )
         continue;
      if ( ! g_State.vehiclesRuntimeInfo[i].bReceivedKeyframeInfoInVideoStream )
         return;

      Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[i].uVehicleId, 155);
      if ( (NULL == pModel) || (pModel->is_spectator) )
         continue;

      if ( pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].keyframe_ms > 0 )
         continue;
      if ( pModel->isVideoLinkFixedOneWay() )
         continue;

      _video_link_keyframe_check_send_to_vehicle(pModel->uVehicleId);
   }

   // Do adjustments only once ever 100 ms
   if ( g_TimeNow < s_uTimeLastKeyFrameCheckVehicles + 100 )
       return;

   s_uTimeLastKeyFrameCheckVehicles = g_TimeNow;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( 0 == g_State.vehiclesRuntimeInfo[i].uVehicleId )
         continue;
      if ( ! g_State.vehiclesRuntimeInfo[i].bIsPairingDone )
         continue;
      if ( ! g_State.vehiclesRuntimeInfo[i].bReceivedKeyframeInfoInVideoStream )
         return;

      Model* pModel = findModelWithId(g_State.vehiclesRuntimeInfo[i].uVehicleId, 156);
      if ( (NULL == pModel) || (pModel->is_spectator) )
         continue;
        
      if ( pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].keyframe_ms > 0 )
         continue;
      if ( pModel->isVideoLinkFixedOneWay() )
         continue;

      if ( hardware_board_is_goke(pModel->hwCapabilities.uBoardType) )
         continue;

      int iCurrentVideoProfile = 0;
      int iCurrentFPS = 0;
      for( int k=0; k<MAX_VIDEO_PROCESSORS; k++ )
      {
         if ( NULL == g_pVideoProcessorRxList[k] )
            break;
         if ( pModel->uVehicleId != g_pVideoProcessorRxList[k]->m_uVehicleId )
            continue;
         iCurrentVideoProfile = g_pVideoProcessorRxList[k]->getCurrentlyReceivedVideoProfile();
         iCurrentFPS = g_pVideoProcessorRxList[k]->getCurrentlyReceivedVideoFPS();
         break;
      }
      if ( iCurrentVideoProfile == -1 )
         iCurrentVideoProfile = pModel->video_params.user_selected_video_link_profile;
   
      int iCurrentProfileKeyFrameMs = pModel->video_link_profiles[iCurrentVideoProfile].keyframe_ms;

      if ( iCurrentFPS < 1 )
         iCurrentFPS = pModel->video_link_profiles[iCurrentVideoProfile].fps;

      _video_link_keyframe_check_send_to_vehicle(pModel->uVehicleId);

      // If relaying is enabled on this vehicle and video from this vehicle is not visible, do not adjust anything (will default to relay keyframe interval)
   
      if ( ! relay_controller_must_display_video_from(g_pCurrentModel, pModel->uVehicleId) )
         continue;

      // User set a fixed keyframe value? Then do nothing (just set it if not set);

      if ( iCurrentProfileKeyFrameMs > 0 )
      {
         if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iLastRequestedKeyFrameMs != iCurrentProfileKeyFrameMs )
            g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iLastRequestedKeyFrameMs = iCurrentProfileKeyFrameMs;

         if ( g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iLastRequestedKeyFrameMs == g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].iLastAcknowledgedKeyFrameMs )
            continue;

         _video_link_keyframe_check_send_to_vehicle(pModel->uVehicleId);
         continue;
      }

      if ( g_TimeNow < g_SM_RouterVehiclesRuntimeInfo.vehicles_adaptive_video[i].uTimeLastRequestedKeyFrame + 50 )
         continue;

      // Do actual check and adjustments for autokeyframe
      _video_link_keyframe_do_adaptive_check(i);
   }
}

