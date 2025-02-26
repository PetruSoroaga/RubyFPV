/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../base/config_hw.h"
#include "../base/base.h"
#include "../base/hw_procs.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/radio_utils.h"
#include "../base/utils.h"
#include "../base/tx_powers.h"
#include "../base/hardware_radio_txpower.h"
#include "../common/string_utils.h"
#include "../utils/utils_vehicle.h"
#include "../radio/radioflags.h"


u32 vehicle_utils_getControllerId()
{
   bool bControllerIdOk = false;
   u32 uControllerId = 0;

   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CONTROLLER_ID);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%u", &uControllerId) )
         bControllerIdOk = false;
      else
         bControllerIdOk = true;
      fclose(fd);
   }

   if ( 0 == uControllerId || MAX_U32 == uControllerId )
      bControllerIdOk = false;

   if ( bControllerIdOk )
   {
      log_line("Loaded current controller id: %u", uControllerId);
      return uControllerId;
   }
   log_line("No current controller id stored on file.");
   return 0;
}

bool videoLinkProfileIsOnlyVideoKeyframeChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile)
{
   if ( NULL == pOldProfile || NULL == pNewProfile )
      return false;

   type_video_link_profile tmp;
   memcpy(&tmp, pNewProfile, sizeof(type_video_link_profile) );
   tmp.keyframe_ms = pOldProfile->keyframe_ms;
   tmp.uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
   tmp.uProfileEncodingFlags |= (pOldProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME);

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

bool videoLinkProfileIsOnlyIPQuantizationDeltaChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile)
{
   if ( NULL == pOldProfile || NULL == pNewProfile )
      return false;

   type_video_link_profile tmp;
   memcpy(&tmp, pNewProfile, sizeof(type_video_link_profile) );
   tmp.iIPQuantizationDelta = pOldProfile->iIPQuantizationDelta;

   if ( 0 == memcmp(pOldProfile, &tmp, sizeof(type_video_link_profile)) )
   if ( pOldProfile->iIPQuantizationDelta != pNewProfile->iIPQuantizationDelta )
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
 
   if ( 0 == memcmp(pOldProfile, &tmp, sizeof(type_video_link_profile)) )
   if ( (pOldProfile->block_packets != pNewProfile->block_packets) ||
        (pOldProfile->block_fecs != pNewProfile->block_fecs) )
      return true;
   return false;
}

bool videoLinkProfileIsOnlyAdaptiveVideoChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile)
{
   if ( NULL == pOldProfile || NULL == pNewProfile )
      return false;

   type_video_link_profile tmp;
   memcpy(&tmp, pNewProfile, sizeof(type_video_link_profile) );
   tmp.uProfileEncodingFlags = (tmp.uProfileEncodingFlags & (~(VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK | VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO | VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO))) | (pOldProfile->uProfileEncodingFlags & ( VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK | VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO | VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO));

   if ( 0 == memcmp(pOldProfile, &tmp, sizeof(type_video_link_profile)) )
   {
      if ( (pOldProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK) != (pNewProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK) )
         return true;
      if ( (pOldProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO) != (pNewProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO) )
         return true;
      if ( (pOldProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO) != (pNewProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO) )
         return true;
   }
   return false;
}

// To fix
/*
void video_overwrites_init(shared_mem_video_link_overwrites* pSMLVO, Model* pModel)
{
   if ( NULL == pSMLVO )
      return;
   if ( NULL == pModel )
      return;

   pSMLVO->userVideoLinkProfile = pModel->video_params.user_selected_video_link_profile;
   pSMLVO->currentVideoLinkProfile = pSMLVO->userVideoLinkProfile;
   pSMLVO->uTimeSetCurrentVideoLinkProfile = g_TimeNow;
   pSMLVO->currentProfileMaxVideoBitrate = utils_get_max_allowed_video_bitrate_for_profile_or_user_video_bitrate(pModel, pSMLVO->currentVideoLinkProfile);
   pSMLVO->currentProfileAndLevelDefaultBitrate = pSMLVO->currentProfileMaxVideoBitrate;
   pSMLVO->currentSetVideoBitrate = pSMLVO->currentProfileMaxVideoBitrate;

   pSMLVO->currentDataBlocks = pModel->video_link_profiles[pSMLVO->currentVideoLinkProfile].block_packets;
   pSMLVO->currentECBlocks = pModel->video_link_profiles[pSMLVO->currentVideoLinkProfile].block_fecs;
   pSMLVO->currentProfileShiftLevel = 0;
   pSMLVO->currentH264QUantization = 0;
   
   pSMLVO->hasEverSwitchedToLQMode = 0;
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      pSMLVO->profilesTopVideoBitrateOverwritesDownward[i] = 0;
}
*/

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
      if ( pModel->radioLinksParams.link_capabilities_flags[iLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         log_softerror_and_alarm("Radio Link %d is disabled!", iLink+1 );
         continue;
      }

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
   
      log_line("Radio link %d must be set to %s", iLink+1, str_format_frequency(uRadioLinkFrequency));

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
               uRadioLinkFrequency, pModel->radioInterfacesParams.interface_raw_power[iInterface],
               pModel->radioLinksParams.link_datarate_data_bps[iLink],
               (u32)((pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_ECC)?1:0),
               (u32)((pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_LBT)?1:0),
               (u32)((pModel->radioLinksParams.link_radio_flags[iLink] & RADIO_FLAGS_SIK_MCSTR)?1:0),
               pProcessStats);
            iLinkConfiguredInterfacesCount++;
         }
         else if ( hardware_radio_index_is_serial_radio(iInterface) )
         {
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

   apply_vehicle_tx_power_levels(pModel);

   log_line("CONFIGURE RADIO END ---------------------------------------------------------");

   return bMissmatch;
}

int get_vehicle_radio_link_current_tx_power_mw(Model* pModel, int iRadioLinkIndex)
{
   if ( (NULL == pModel) || (iRadioLinkIndex < 0) || (iRadioLinkIndex >= pModel->radioLinksParams.links_count) )
      return 1;

   int iMaxCardPowerMw = 1;
   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( ! hardware_radio_type_is_ieee(pModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) )
      if ( pModel->radioInterfacesParams.interface_link_id[i] != iRadioLinkIndex )
         continue;

      int iCardModel = pModel->radioInterfacesParams.interface_card_model[i];
      if ( iCardModel < 0 )
         iCardModel = -iCardModel;
      int iCardRawPower = pModel->radioInterfacesParams.interface_raw_power[i];
      int iCardPowerMw = tx_powers_convert_raw_to_mw(pModel->hwCapabilities.uBoardType, iCardModel, iCardRawPower);
      if ( pModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_2W )
         iCardPowerMw = tx_powers_get_mw_boosted_value_from_mw(iCardPowerMw, true, false);
      if ( pModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HAS_BOOSTER_4W )
         iCardPowerMw = tx_powers_get_mw_boosted_value_from_mw(iCardPowerMw, false, true);
      if ( iCardPowerMw > iMaxCardPowerMw )
         iMaxCardPowerMw = iCardPowerMw;
   }

   return iMaxCardPowerMw;
}

void apply_vehicle_tx_power_levels(Model* pModel)
{
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("Passing NULL model on applying model tx powers.");
      return;
   }

   log_line("Aplying all radio interfaces raw tx powers...");
   for( int i=0; i<pModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( hardware_radio_index_is_sik_radio(i) )
      {
         log_line("Skipping radio interface %d as it's a SiK radio.", i+1);
         continue;
      }
      if ( ! hardware_radio_index_is_wifi_radio(i) )
      {
         log_line("Skipping radio interface %d as it's not a IEEE radio.", i+1);
         continue;
      }
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( ! pRadioHWInfo->isConfigurable )
         continue;
      if ( hardware_radio_driver_is_rtl8812au_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8812au(i, pModel->radioInterfacesParams.interface_raw_power[i]);
      if ( hardware_radio_driver_is_rtl8812eu_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8812eu(i, pModel->radioInterfacesParams.interface_raw_power[i]);
      if ( hardware_radio_driver_is_rtl8733bu_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8733bu(i, pModel->radioInterfacesParams.interface_raw_power[i]);
      if ( hardware_radio_driver_is_atheros_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_atheros(i, pModel->radioInterfacesParams.interface_raw_power[i]);
   }
   log_line("Aplied all radio interfaces raw tx powers.");
}