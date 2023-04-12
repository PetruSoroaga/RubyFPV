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
#include "utils_vehicle.h"
#include "shared_vars.h"

static u32 s_lastCameraCommandModifyCounter = 0;
static u8 s_CameraCommandNumber = 0;
static u32 s_uCameraCommandsBuffer[8];


bool videoLinkProfileIsOnlyVideoKeyframeChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile)
{
   if ( NULL == pOldProfile || NULL == pNewProfile )
      return false;

   type_video_link_profile tmp;
   memcpy(&tmp, pNewProfile, sizeof(type_video_link_profile) );
   tmp.keyframe = pOldProfile->keyframe;

   if ( 0 == memcmp(pOldProfile, &tmp, sizeof(type_video_link_profile)) )
      return true;
   return false;
}

bool videoLinkProfileIsOnlyBitrateChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile)
{
   if ( NULL == pOldProfile || NULL == pNewProfile )
      return false;

   type_video_link_profile tmp;
   memcpy(&tmp, pNewProfile, sizeof(type_video_link_profile) );
   tmp.bitrate_fixed_bps = pOldProfile->bitrate_fixed_bps;

   if ( 0 == memcmp(pOldProfile, &tmp, sizeof(type_video_link_profile)) )
   if ( pOldProfile->bitrate_fixed_bps != pNewProfile->bitrate_fixed_bps )
      return true;

   return false;
}

bool videoLinkProfileIsOnlyECSchemeChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile)
{
   if ( NULL == pOldProfile || NULL == pNewProfile )
      return false;

   type_video_link_profile tmp;
   memcpy(&tmp, pNewProfile, sizeof(type_video_link_profile) );
   tmp.block_packets = pOldProfile->block_packets;
   tmp.block_fecs = pOldProfile->block_fecs;
   tmp.packet_length = pOldProfile->packet_length;

   if ( 0 == memcmp(pOldProfile, &tmp, sizeof(type_video_link_profile)) )
   if ( (pOldProfile->block_packets != pNewProfile->block_packets) ||
        (pOldProfile->block_fecs != pNewProfile->block_fecs) ||
        (pOldProfile->packet_length != pNewProfile->packet_length) )
      return true;
   return false;
}

bool videoLinkProfileIsOnlyAdaptiveVideoChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile)
{
   if ( NULL == pOldProfile || NULL == pNewProfile )
      return false;

   type_video_link_profile tmp;
   memcpy(&tmp, pNewProfile, sizeof(type_video_link_profile) );
   tmp.encoding_extra_flags = (tmp.encoding_extra_flags & (~(ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO))) | (pOldProfile->encoding_extra_flags & ( ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO));

   if ( 0 == memcmp(pOldProfile, &tmp, sizeof(type_video_link_profile)) )
   {
      if ( (pOldProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS) != (pNewProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS) )
         return true;
      if ( (pOldProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO) != (pNewProfile->encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO) )
         return true;
   }
   return false;
}

void video_overwrites_init(shared_mem_video_link_overwrites* pSMLVO, Model* pModel)
{
   if ( NULL == pSMLVO )
      return;
   if ( NULL == pModel )
      return;

   pSMLVO->userVideoLinkProfile = pModel->video_params.user_selected_video_link_profile;
   pSMLVO->currentVideoLinkProfile = pSMLVO->userVideoLinkProfile;

   pSMLVO->currentProfileDefaultVideoBitrate = pModel->video_link_profiles[pSMLVO->currentVideoLinkProfile].bitrate_fixed_bps;
   pSMLVO->currentSetVideoBitrate = pSMLVO->currentProfileDefaultVideoBitrate;

   pSMLVO->currentDataBlocks = pModel->video_link_profiles[pSMLVO->currentVideoLinkProfile].block_packets;
   pSMLVO->currentECBlocks = pModel->video_link_profiles[pSMLVO->currentVideoLinkProfile].block_fecs;
   pSMLVO->currentProfileShiftLevel = 0;
   pSMLVO->currentH264QUantization = 0;
   
   if ( pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].keyframe > 0 )
      pSMLVO->uCurrentKeyframe = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].keyframe;
   else
      pSMLVO->uCurrentKeyframe = DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL * pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].fps / 1000;
      
   pSMLVO->hasEverSwitchedToLQMode = 0;
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      pSMLVO->profilesTopVideoBitrateOverwritesDownward[i] = 0;
}

void send_control_message_to_raspivid(u8 parameter, u8 value)
{
   if ( NULL == g_pCurrentModel || (! g_pCurrentModel->hasCamera()) )
      return;
   if ( (! g_pCurrentModel->isCameraCSICompatible()) && (! g_pCurrentModel->isCameraVeye()) )
   {
      log_softerror_and_alarm("Can't signal on the fly video capture parameter change. Capture camera is not raspi or veye.");
      return;
   }
   if ( parameter == RASPIVID_COMMAND_ID_VIDEO_BITRATE )
   {
      g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate = ((u32)value) * 100000;
      g_bDidSentRaspividBitrateRefresh = true;
      //log_line("Setting video capture bitrate to: %u", g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate );
   }
   if ( NULL == g_pSharedMemRaspiVidComm )
   {
      log_line("Opening video capture program commands pipe write endpoint...");
      g_pSharedMemRaspiVidComm = (u8*)open_shared_mem_for_write(SHARED_MEM_RASPIVIDEO_COMMAND, SIZE_OF_SHARED_MEM_RASPIVID_COMM);
      if ( NULL == g_pSharedMemRaspiVidComm )
         log_error_and_alarm("Failed to open video capture program commands pipe write endpoint!");
      else
         log_line("Opened video capture program commands pipe write endpoint.");
   }

   if ( NULL == g_pSharedMemRaspiVidComm )
   {
      log_softerror_and_alarm("Can't send video/camera command to video capture programm, no pipe.");
      return;
   }

   if ( 0 == s_lastCameraCommandModifyCounter )
      memset( (u8*)&s_uCameraCommandsBuffer[0], 0, 8*sizeof(u32));

   s_lastCameraCommandModifyCounter++;
   s_CameraCommandNumber++;
   if ( 0 == s_CameraCommandNumber )
    s_CameraCommandNumber++;
   
   // u32 0: command modify stamp (must match u32 7)
   // u32 1: byte 0: command counter;
   //        byte 1: command type;
   //        byte 2: command parameter 1;
   //        byte 3: command parameter 2;
   // u32 2-6: history of previous commands (after current one from u32-1)
   // u32 7: command modify stamp (must match u32 0)

   s_uCameraCommandsBuffer[0] = s_lastCameraCommandModifyCounter;
   s_uCameraCommandsBuffer[7] = s_lastCameraCommandModifyCounter;
   for( int i=6; i>1; i-- )
      s_uCameraCommandsBuffer[i] = s_uCameraCommandsBuffer[i-1];
   s_uCameraCommandsBuffer[1] = ((u32)s_CameraCommandNumber) | (((u32)parameter)<<8) | (((u32)value)<<16);

   memcpy(g_pSharedMemRaspiVidComm, (u8*)&(s_uCameraCommandsBuffer[0]), 8*sizeof(u32));
}
