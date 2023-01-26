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

#include "process_received_ruby_messages.h"
#include "shared_vars.h"
#include "../base/ruby_ipc.h"
#include "../radio/radiolink.h"
#include "timers.h"
#include "packets_utils.h"
#include "processor_tx_video.h"
#include "video_link_stats_overwrites.h"
#include "video_link_auto_keyframe.h"

extern t_packet_queue s_QueueRadioPacketsOut;

static u32 s_uResendPairingConfirmationCounter = 0;

int _process_received_ping_messages(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   
   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   if ( pPH->total_length >= sizeof(t_packet_header) + sizeof(u32) + sizeof(u8) )
   {
         u32 pingId = 0;
         u8 radioLinkId = 0;
         u32 timeNow = get_current_timestamp_micros();
         memcpy( &pingId, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));
         memcpy( &radioLinkId, pPacketBuffer + sizeof(t_packet_header)+sizeof(u32), sizeof(u8));

         if ( pPH->total_length > sizeof(t_packet_header) + sizeof(u32) + sizeof(u8) )
         {
            // Extract controller radio & video links stats
            u8* pData = pPacketBuffer + sizeof(t_packet_header)+sizeof(u32) + sizeof(u8);
            //u8 flagsAndVersion = *pData;
            pData++;
         }

         t_packet_header PH;
         PH.packet_flags = PACKET_COMPONENT_RUBY;
         PH.packet_type =  PACKET_TYPE_RUBY_PING_CLOCK_REPLY;
         PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
         PH.vehicle_id_dest = pPH->vehicle_id_src;
         PH.total_headers_length = sizeof(t_packet_header);
         PH.total_length = sizeof(t_packet_header) + sizeof(u32) + sizeof(u32) + sizeof(u8);
         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet+sizeof(t_packet_header), &pingId, sizeof(u32));
         memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &timeNow, sizeof(u32));
         memcpy(packet+sizeof(t_packet_header)+2*sizeof(u32), &radioLinkId, sizeof(u8));
         packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);
   }
   return 0;
}


int _process_received_adaptive_video_message(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   
   if ( pPH->packet_type != PACKET_TYPE_RUBY_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL )
      return 0;
   if ( pPH->total_length != sizeof(t_packet_header) + sizeof(u32) )
      return 0;

   u32 uAdaptiveLevel = 0;
   memcpy( &uAdaptiveLevel, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_RUBY;
   PH.packet_type =  PACKET_TYPE_RUBY_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = pPH->vehicle_id_src;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + sizeof(u32);
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &uAdaptiveLevel, sizeof(u32));
   packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);

   int iAdaptiveLevel = uAdaptiveLevel;
   int iLevelsHQ = 1 + g_pCurrentModel->get_video_link_profile_max_level_shifts(g_pCurrentModel->video_params.user_selected_video_link_profile);
   int iLevelsMQ = 1 + g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_MQ);
   int iLevelsLQ = 1 + g_pCurrentModel->get_video_link_profile_max_level_shifts(VIDEO_PROFILE_LQ);

   int iTargetProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
   int iTargetShiftLevel = 0;

   if ( iAdaptiveLevel < iLevelsHQ )
   {
      iTargetProfile = g_pCurrentModel->video_params.user_selected_video_link_profile;
      iTargetShiftLevel = iAdaptiveLevel;
   }
   else if ( iAdaptiveLevel < iLevelsHQ + iLevelsMQ )
   {
      iTargetProfile = VIDEO_PROFILE_MQ;
      iTargetShiftLevel = iAdaptiveLevel - iLevelsHQ;    
   }
   else if ( iAdaptiveLevel < iLevelsHQ + iLevelsMQ + iLevelsLQ )
   {
      iTargetProfile = VIDEO_PROFILE_LQ;
      iTargetShiftLevel = iAdaptiveLevel - iLevelsHQ - iLevelsMQ;    
   }
   
   //log_line("DEBUG requested adaptive video change to: %d / %d.", iTargetProfile, iTargetShiftLevel);
   if ( iTargetProfile == g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile )
   if ( iTargetShiftLevel == g_SM_VideoLinkStats.overwrites.currentProfileShiftLevel )
   {
      //log_line("DEBUG no adaptive level change.");
      return 0;
   }
   video_stats_overwrites_switch_to_profile_and_level(iTargetProfile, iTargetShiftLevel);
   return 0;
}



int _process_received_video_keyframe_message(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   
   if ( pPH->packet_type != PACKET_TYPE_RUBY_SWITCH_VIDEO_KEYFRAME_TO_VALUE )
      return 0;
   if ( pPH->total_length != sizeof(t_packet_header) + sizeof(u32) )
      return 0;

   u32 uNewKeyframeValue = 0;
   memcpy( &uNewKeyframeValue, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_RUBY;
   PH.packet_type =  PACKET_TYPE_RUBY_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = pPH->vehicle_id_src;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + sizeof(u32);
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &uNewKeyframeValue, sizeof(u32));
   packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);

   // Discard requests if we are on fixed keyframe
   if ( g_pCurrentModel->video_link_profiles[g_SM_VideoLinkStats.overwrites.currentVideoLinkProfile].keyframe > 0 )
      return 0;
   //log_line("DEBUG: received valid request from controller for new keyframe: %d", (int) uNewKeyframeValue);
   process_data_tx_video_set_current_keyframe_interval((int)uNewKeyframeValue);
   video_link_auto_keyframe_set_controller_requested_value((int)uNewKeyframeValue);
   return 0;
}

int process_received_ruby_message(u8* pPacketBuffer)
{
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;

   if ( pPH->packet_type == PACKET_TYPE_RUBY_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL )
   {
      return _process_received_adaptive_video_message(pPacketBuffer);
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_SWITCH_VIDEO_KEYFRAME_TO_VALUE )
   {
      return _process_received_video_keyframe_message(pPacketBuffer);
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PING_CLOCK )
   {
      return _process_received_ping_messages(pPacketBuffer);
   }

   if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST )
   {
      u32 uResendCount = 0;
      if ( pPH->total_length >= pPH->total_headers_length + sizeof(u32) )
         memcpy(&uResendCount, pPacketBuffer + sizeof(t_packet_header), sizeof(u32));

      log_line("Received pairing request from controller (received resend count: %u). CID: %u, VID: %u. Sending confirmation back.", uResendCount, pPH->vehicle_id_src, pPH->vehicle_id_dest);
      
      process_data_tx_video_reset_retransmissions_history_info();
      
      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_RUBY;
      PH.packet_type =  PACKET_TYPE_RUBY_PAIRING_CONFIRMATION;
      PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
      PH.vehicle_id_dest = pPH->vehicle_id_src;
      PH.total_headers_length = sizeof(t_packet_header);
      PH.total_length = sizeof(t_packet_header) + sizeof(u32);

      s_uResendPairingConfirmationCounter++;

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &s_uResendPairingConfirmationCounter, sizeof(u32));
      packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);

      g_bReceivedPairingRequest = true;
      g_uControllerId = pPH->vehicle_id_src;
      if ( g_pCurrentModel->controller_id != g_uControllerId )
      {
         g_pCurrentModel->controller_id = g_uControllerId;
         g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
         FILE* fd = fopen(FILE_CONTROLLER_ID, "w");
         if ( NULL != fd )
         {
            fprintf(fd, "%u\n", g_uControllerId);
            fclose(fd);
         }
      }

      // Forward to other components
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, pPacketBuffer, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, pPacketBuffer, pPH->total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToRC, pPacketBuffer, pPH->total_length);
      return 0;
   }
   
   log_line("Received unprocessed Ruby message from controller, message type: %d", pPH->packet_type);

   return 0;
}
