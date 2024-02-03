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

#include "../base/config_hw.h"
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/radio_utils.h"
#include "../common/string_utils.h"
#include "utils_vehicle.h"
#include "shared_vars.h"

u32 s_lastCameraCommandModifyCounter = 0;
u8 s_CameraCommandNumber = 0;
u32 s_uCameraCommandsBuffer[8];


bool videoLinkProfileIsOnlyVideoKeyframeChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile)
{
   if ( NULL == pOldProfile || NULL == pNewProfile )
      return false;

   type_video_link_profile tmp;
   memcpy(&tmp, pNewProfile, sizeof(type_video_link_profile) );
   tmp.keyframe_ms = pOldProfile->keyframe_ms;

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

   pSMLVO->currentProfileMaxVideoBitrate = utils_get_max_allowed_video_bitrate_for_profile_or_user_video_bitrate(pModel, pSMLVO->currentVideoLinkProfile);
   pSMLVO->currentProfileAndLevelDefaultBitrate = pSMLVO->currentProfileMaxVideoBitrate;
   pSMLVO->currentSetVideoBitrate = pSMLVO->currentProfileMaxVideoBitrate;

   pSMLVO->currentDataBlocks = pModel->video_link_profiles[pSMLVO->currentVideoLinkProfile].block_packets;
   pSMLVO->currentECBlocks = pModel->video_link_profiles[pSMLVO->currentVideoLinkProfile].block_fecs;
   pSMLVO->currentProfileShiftLevel = 0;
   pSMLVO->currentH264QUantization = 0;
   
   if ( pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].keyframe_ms > 0 )
      pSMLVO->uCurrentKeyframeMs = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].keyframe_ms;
   else
      pSMLVO->uCurrentKeyframeMs = DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL;
      
   pSMLVO->hasEverSwitchedToLQMode = 0;
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      pSMLVO->profilesTopVideoBitrateOverwritesDownward[i] = 0;
}

void send_control_message_to_raspivid(u8 parameter, u8 value)
{
   if ( (NULL == g_pCurrentModel) || (! g_pCurrentModel->hasCamera()) )
      return;

   #ifdef HW_PLATFORM_RASPBERRY

   if ( (! g_pCurrentModel->isActiveCameraCSICompatible()) && (! g_pCurrentModel->isActiveCameraVeye()) )
   {
      log_softerror_and_alarm("Can't signal on the fly video capture parameter change. Capture camera is not raspi or veye.");
      return;
   }

   if ( g_uRouterState & ROUTER_STATE_NEEDS_RESTART_VIDEO_CAPTURE )
   {
      log_line("Video capture is restarting, do not send command (%d) to video capture program.", parameter);
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
   #endif
}

// Returns true if configuration has been updated
// It's called only on vehicle side

bool configure_radio_interfaces_for_current_model(Model* pModel, shared_mem_process_stats* pProcessStats)
{
   bool bMissmatch = false;

   log_line("CONFIGURE RADIO START -----------------------------------------------------------");

   if ( NULL == pModel )
   {
      log_line("Configuring all radio interfaces (%d radio interfaces) failed.", hardware_get_radio_interfaces_count());
      log_error_and_alarm("INVALID MODEL parameter (no model, NULL)");
      log_line("CONFIGURE RADIO END ------------------------------------------------------------");
      return false;
   }

   // Populate radio interfaces radio flags from radio links radio flags and rates

   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
     if ( pModel->radioInterfacesParams.interface_link_id[i] >= 0 && pModel->radioInterfacesParams.interface_link_id[i] < pModel->radioLinksParams.links_count )
     {
         pModel->radioInterfacesParams.interface_current_radio_flags[i] = pModel->radioLinksParams.link_radio_flags[pModel->radioInterfacesParams.interface_link_id[i]];
     }
     else
        log_softerror_and_alarm("No radio link or invalid radio link (%d of %d) assigned to radio interface %d.", pModel->radioInterfacesParams.interface_link_id[i], pModel->radioLinksParams.links_count, i+1);
   }

   log_line("Configuring all radio interfaces (%d radio interfaces, %d radio links)", hardware_get_radio_interfaces_count(), pModel->radioLinksParams.links_count);
   if ( pModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      log_line("A relay link is enabled on radio link %d, on %s", pModel->relay_params.isRelayEnabledOnRadioLinkId+1, str_format_frequency(pModel->relay_params.uRelayFrequencyKhz));

   for( int iLink=0; iLink<pModel->radioLinksParams.links_count; iLink++ )
   {
      u32 uRadioLinkFrequency = pModel->radioLinksParams.link_frequency_khz[iLink];
      if ( pModel->relay_params.isRelayEnabledOnRadioLinkId == iLink )
      {
         uRadioLinkFrequency = pModel->relay_params.uRelayFrequencyKhz;
         if ( uRadioLinkFrequency == 0 )
         {
            uRadioLinkFrequency = DEFAULT_FREQUENCY58;
            for( int iInterface=0; iInterface<pModel->radioInterfacesParams.interfaces_count; iInterface++ )
            {
               if ( pModel->radioInterfacesParams.interface_link_id[iInterface] != iLink )
                  continue;
               if ( ! hardware_radioindex_supports_frequency(iInterface, uRadioLinkFrequency ) )
                  uRadioLinkFrequency = DEFAULT_FREQUENCY;
            }
            log_line("Radio link %d is a relay link and relay frequency is 0. Setting a default frequency for this link: %s.", iLink+1, str_format_frequency(uRadioLinkFrequency));
         }
         else log_line("Radio link %d is a relay link on %s", iLink+1, str_format_frequency(uRadioLinkFrequency));
      }
      else
         log_line("Radio link %d must be set to %s", iLink+1, str_format_frequency(uRadioLinkFrequency));

      if ( pModel->radioLinksParams.link_capabilities_flags[iLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         log_softerror_and_alarm("Radio Link %d is disabled!", iLink+1 );
         continue;
      }

      int iLinkConfiguredInterfacesCount = 0;

      for( int iInterface=0; iInterface<pModel->radioInterfacesParams.interfaces_count; iInterface++ )
      {
         if ( pModel->radioInterfacesParams.interface_link_id[iInterface] != iLink )
            continue;
         if ( pModel->radioInterfacesParams.interface_capabilities_flags[iInterface] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         {
            log_softerror_and_alarm("Radio Interface %d (assigned to radio link %d) is disabled!", iInterface+1, iLink+1 );
            continue;
         }
         if ( ! hardware_radioindex_supports_frequency(iInterface, uRadioLinkFrequency ) )
         {
            log_softerror_and_alarm("Radio interface %d (assigned to radio link %d) does not support radio link frequency %s!", iInterface+1, iLink+1, str_format_frequency(uRadioLinkFrequency) );
            bMissmatch = true;
            continue;
         }

         if ( hardware_radio_index_is_sik_radio(iInterface) )
         {
            log_line("Configuring SiK radio interface %d for radio link %d", iInterface+1, iLink+1);
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterface);
            if ( NULL == pRadioHWInfo )
            {
               log_softerror_and_alarm("Failed to get radio hardware info for radio interface %d.", iInterface+1);
               continue;
            }
            hardware_radio_sik_set_frequency_txpower_airspeed_lbt_ecc(pRadioHWInfo,
               uRadioLinkFrequency, pModel->radioInterfacesParams.txPowerSiK,
               pModel->radioLinksParams.link_datarate_data_bps[iLink],
               (u32)((pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_ECC)?1:0),
               (u32)((pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_LBT)?1:0),
               (u32)((pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_MCSTR)?1:0),
               pProcessStats);
            iLinkConfiguredInterfacesCount++;
         }
         else
         {
            radio_utils_set_interface_frequency(pModel, iInterface, iLink, uRadioLinkFrequency, pProcessStats, 0);
            iLinkConfiguredInterfacesCount++;
         }
         if ( pModel->relay_params.isRelayEnabledOnRadioLinkId == iLink )
         {
            log_line("Did set radio interface %d to frequency %s (assigned to radio link %d in relay mode)", iInterface+1, str_format_frequency(uRadioLinkFrequency), iLink+1 );
            pModel->radioInterfacesParams.interface_capabilities_flags[iInterface] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
         }
         else
         {
            log_line("Did set radio interface %d to frequency %s (assigned to radio link %d)", iInterface+1, str_format_frequency(uRadioLinkFrequency), iLink+1 );
            pModel->radioInterfacesParams.interface_capabilities_flags[iInterface] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
         }
         pModel->radioInterfacesParams.interface_current_frequency_khz[iInterface] = uRadioLinkFrequency;
      }
      log_line("Configured a total of %d radio interfaces for radio link %d", iLinkConfiguredInterfacesCount, iLink+1);
   }

   if ( bMissmatch )
   if ( 1 == pModel->radioLinksParams.links_count )
   if ( 1 == pModel->radioInterfacesParams.interfaces_count )
   {
      log_line("There was a missmatch of frequency configuration for the single radio link present on vehicle. Reseting it to default.");
      pModel->radioLinksParams.link_frequency_khz[0] = DEFAULT_FREQUENCY;
      pModel->radioInterfacesParams.interface_current_frequency_khz[0] = DEFAULT_FREQUENCY;
      radio_utils_set_interface_frequency(pModel, 0, 0, pModel->radioLinksParams.link_frequency_khz[0], pProcessStats, 0);
      log_line("Set radio interface 1 to link 1 frequency %s", str_format_frequency(pModel->radioLinksParams.link_frequency_khz[0]) );
   }
   hardware_save_radio_info();
   hardware_sleep_ms(50);
   log_line("CONFIGURE RADIO END ---------------------------------------------------------");

   return bMissmatch;
}
