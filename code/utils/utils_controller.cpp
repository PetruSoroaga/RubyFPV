/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga
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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_radio.h"
#include "../base/hw_procs.h"
#include "../base/tx_powers.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../utils/utils_controller.h"
#include "../utils/utils_vehicle.h"

#if defined(HW_PLATFORM_RASPBERRY) || defined(HW_PLATFORM_RADXA_ZERO3)

u32 controller_utils_getControllerId()
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
      return uControllerId;

   log_line("Generating a new unique controller ID...");

   uControllerId = rand();
   if ( BROADCAST_VEHICLE_ID == uControllerId )
      uControllerId = rand();
   fd = fopen("/sys/firmware/devicetree/base/serial-number", "r");
   if ( NULL != fd )
   {
      char szBuff[256];
      szBuff[0] = 0;
      if ( 1 == fscanf(fd, "%s", szBuff) && 4 < strlen(szBuff) )
      {
         //log_line("Serial ID of HW: %s", szBuff);
         uControllerId += szBuff[strlen(szBuff)-1] + 256 * szBuff[strlen(szBuff)-2];
      }
      fclose(fd);
   }

   log_line("Generated a new unique controller ID: %u", uControllerId);

   fd = fopen(szFile, "w");
   if ( NULL != fd )
   {
      fprintf(fd, "%u\n", uControllerId);
      fclose(fd);
   }
   else
      log_softerror_and_alarm("Failed to save the new generated unique controller ID!");

   return uControllerId;
}


int controller_utils_export_all_to_usb()
{
   char szOutput[1024];
   char szComm[512];
   char szOutFile[MAX_FILE_PATH_SIZE];

   hw_execute_bash_command_raw("which zip", szOutput);
   if ( 0 == strlen(szOutput) || NULL == strstr(szOutput, "zip") )
      return -1;

   if ( 1 != hardware_try_mount_usb() )
      return -2;

   u32 uControllerId = controller_utils_getControllerId();
   sprintf(szOutFile, "ruby_export_all_controller_%u.zip", uControllerId);

   sprintf(szComm, "mkdir -p %s/importexport", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "chmod 777 %s/importexport", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "rm -rf %s/importexport/%s 2>/dev/null", FOLDER_RUBY_TEMP, szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "zip %s/importexport/%s %s/*", FOLDER_RUBY_TEMP, szOutFile, FOLDER_CONFIG);
   hw_execute_bash_command(szComm, NULL);

   #ifdef HW_PLATFORM_RASPBERRY
   sprintf(szComm, "zip -r %s/importexport/%s /boot/config.txt", FOLDER_RUBY_TEMP, szOutFile);
   hw_execute_bash_command(szComm, NULL);
   #endif

   sprintf(szComm, "chmod 777 %s/importexport/%s", FOLDER_RUBY_TEMP, szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "mkdir -p %s/Ruby_Exports", FOLDER_USB_MOUNT );
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "rm -rf %s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "%s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szOutFile);
   if( access( szComm, R_OK ) != -1 )
   {
      hardware_unmount_usb();
      sprintf(szComm, "rm -rf %s/importexport", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      sync();
      return -10;
   }

   sprintf(szComm, "cp -rf %s/importexport/%s %s/Ruby_Exports/%s", FOLDER_RUBY_TEMP, szOutFile, FOLDER_USB_MOUNT, szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "chmod 777 %s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szOutFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "%s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szOutFile);
   if( access( szComm, R_OK ) == -1 )
   {
      hardware_unmount_usb();
      sprintf(szComm, "rm -rf %s/importexport", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);
      sync();
      return -11;
   }

   hardware_unmount_usb();
   sprintf(szComm, "rm -rf %s/importexport", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);
   sync();

   return 0;
}

int controller_utils_import_all_from_usb(bool bImportAnyFound)
{
   char szOutput[1024];
   char szComm[512];
   char szUSBFile[MAX_FILE_PATH_SIZE];
   char szInFile[MAX_FILE_PATH_SIZE];

   hw_execute_bash_command_raw("which unzip", szOutput);
   if ( 0 == strlen(szOutput) || NULL == strstr(szOutput, "unzip") )
      return -1;

   if ( 1 != hardware_try_mount_usb() )
      return -2;

   u32 uControllerId = controller_utils_getControllerId();
   sprintf(szInFile, "ruby_export_all_controller_%u.zip", uControllerId);
   snprintf(szUSBFile, sizeof(szUSBFile)/sizeof(szUSBFile[0]), "%s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szInFile);

   if( access( szUSBFile, R_OK ) == -1 )
   {
      if ( bImportAnyFound )
      {
         sprintf(szInFile, "%s/Ruby_Exports/ruby_export_all_controller_*", FOLDER_USB_MOUNT);
         sprintf(szComm, "find %s 2>/dev/null", szInFile);
         szOutput[0] = 0;
         hw_execute_bash_command(szComm, szOutput);
         if ( (0 != szOutput[0]) && (NULL != strstr(szOutput, "ruby_export_all_controller_") ) )
         {
            int len = strlen(szOutput);
            for( int i=0; i<len; i++ )
               if ( szOutput[i] == 10 || szOutput[i] == 13 )
                  szOutput[i] = 0;

            strcpy(szInFile, szOutput);
            snprintf(szUSBFile, sizeof(szUSBFile)/sizeof(szUSBFile[0]), "%s/Ruby_Exports/%s", FOLDER_USB_MOUNT, szInFile);
         }
         if( access( szUSBFile, R_OK ) == -1 )
         {
            log_line("Can't access the import archive on USB stick: [%s]", szInFile);
            hardware_unmount_usb();
            return -3;
         }
      }
      else
      {
         log_line("Can't find the import archive on USB stick: [%s]", szInFile);
         hardware_unmount_usb();
         return -3;
      }
   }

   sprintf(szComm, "mkdir -p %s/importexport", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "chmod 777 %s/importexport", FOLDER_RUBY_TEMP);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "rm -rf %s/importexport/%s 2>/dev/null", FOLDER_RUBY_TEMP, szInFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "cp -rf %s %s/importexport/%s", szUSBFile, FOLDER_RUBY_TEMP, szInFile);
   hw_execute_bash_command(szComm, NULL);

   if ( 0 < strlen(FOLDER_CONFIG) )
   {
      sprintf(szComm, "rm -rf %s/*", FOLDER_CONFIG);
      hw_execute_bash_command(szComm, NULL);
   }
   sprintf(szComm, "unzip %s/importexport/%s", FOLDER_RUBY_TEMP, szInFile);
   hw_execute_bash_command(szComm, NULL);

   #ifdef HW_PLATFORM_RASPBERRY
   hw_execute_bash_command("cp -rf boot/config.txt /boot/config.txt", NULL);
   hw_execute_bash_command("rm -rf boot/config.txt", NULL);
   #endif

   sprintf(szComm, "rm -rf %s/importexport/%s", FOLDER_RUBY_TEMP, szInFile);
   hw_execute_bash_command(szComm, NULL);

   hardware_unmount_usb();
   sync();

   return 0;
}

bool controller_utils_usb_import_has_matching_controller_id_file()
{
   if ( 1 != hardware_try_mount_usb() )
      return false;

   u32 uControllerId = controller_utils_getControllerId();

   char szOutput[2048];
   char szFile[256];
   char szComm[1024];
   szOutput[0] = 0;
   sprintf(szFile, "%s/Ruby_Exports/ruby_export_all_controller_%u.zip", FOLDER_USB_MOUNT, uControllerId);
   sprintf(szComm, "find %s 2>/dev/null", szFile);
   hw_execute_bash_command(szComm, szOutput);

   hardware_unmount_usb();

   if ( 0 != szOutput[0] )
   if (  NULL != strstr(szOutput, "ruby_export_all_controller") )
   if (  NULL != strstr(szOutput, ".zip") )
      return true;

   return false;
}

bool controller_utils_usb_import_has_any_controller_id_file()
{
   if ( 1 != hardware_try_mount_usb() )
      return false;

   char szOutput[2048];
   char szFile[256];
   char szComm[1024];
   szOutput[0] = 0;
   sprintf(szFile, "%s/Ruby_Exports/ruby_export_all_controller_*", FOLDER_USB_MOUNT);
   sprintf(szComm, "find %s 2>/dev/null", szFile);
   hw_execute_bash_command(szComm, szOutput);

   hardware_unmount_usb();

   if ( 0 != szOutput[0] )
   if (  NULL != strstr(szOutput, "ruby_export_all_controller") )
   if (  NULL != strstr(szOutput, ".zip") )
      return true;

   return false;
}

// Returns number of asignable radio interfaces or a negative error code number
int controller_count_asignable_radio_interfaces_to_vehicle_radio_link(Model* pModel, int iVehicleRadioLinkId)
{
   int iCountAssignableRadioInterfaces = 0;
   int iCountPotentialAssignableRadioInterfaces = 0;
   int iErrorCode = 0;

   if ( NULL == pModel )
      return -10;
   if ( (iVehicleRadioLinkId < 0) || (iVehicleRadioLinkId >= pModel->radioLinksParams.links_count) )
      return -11;

   if ( pModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      return -12;
   if ( pModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      return -14;
   if ( ! (pModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      return -15;
   
   for( int iInterface = 0; iInterface < hardware_get_radio_interfaces_count(); iInterface++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iInterface);
      if ( NULL == pRadioHWInfo )
         continue;

      t_ControllerRadioInterfaceInfo* pCardInfo = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCardInfo )
         continue;

      if ( ! hardware_radio_supports_frequency(pRadioHWInfo, pModel->radioLinksParams.link_frequency_khz[iVehicleRadioLinkId]) )
         continue;

      if ( pModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( pModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         continue;

      if ( ! (pModel->radioLinksParams.link_capabilities_flags[iVehicleRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( ! (pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         continue;

      if ( pCardInfo->capabilities_flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         iCountPotentialAssignableRadioInterfaces++;
         continue;
      }
      iCountAssignableRadioInterfaces++;
   }

   if ( iCountAssignableRadioInterfaces > 0 )
      return iCountAssignableRadioInterfaces;

   if ( iCountPotentialAssignableRadioInterfaces > 0 )
      return -iCountPotentialAssignableRadioInterfaces;

   return iErrorCode;
}

void propagate_video_profile_changes(type_video_link_profile* pOrgProfile, type_video_link_profile* pUpdatedProfile, type_video_link_profile* pAllProfiles)
{
   if ( (NULL == pOrgProfile) || (NULL == pUpdatedProfile) || (NULL == pAllProfiles) )
      return;

   // If EC scheme spreading factor has changed by user, update MQ and LQ video profiles spreading factor too
   
   if ( ( (pOrgProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT) != (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT) ) ||
      ( (pOrgProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT) != (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT) ) )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT | VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT);
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT);
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT);
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT | VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT);
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT);
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT);
   }

   if ( ( (pOrgProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION) != (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION) ) ||
      ( (pOrgProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH) != (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH) ) )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION | VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH);
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION);
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH);
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION | VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH);
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION);
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= (pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH);
   }

   pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO;
   pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO;
   if ( pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO;
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO;
   }

   pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS;
   pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS;
   if ( pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS;
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS;
   }

   pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
   pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
   if ( pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
   }

   pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
   pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
   if ( pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
   }

   pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;
   pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;
   if ( pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;
   }

   pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
   pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
   if ( pUpdatedProfile->uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
      pAllProfiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
   }

   if ( (pOrgProfile->width != pUpdatedProfile->width) || (pOrgProfile->height != pUpdatedProfile->height) )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].width = pUpdatedProfile->width;
      pAllProfiles[VIDEO_PROFILE_MQ].height = pUpdatedProfile->height;
      pAllProfiles[VIDEO_PROFILE_LQ].width = pUpdatedProfile->width;
      pAllProfiles[VIDEO_PROFILE_LQ].height = pUpdatedProfile->height;
   }

   if ( pOrgProfile->fps != pUpdatedProfile->fps )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].fps = pUpdatedProfile->fps;
      pAllProfiles[VIDEO_PROFILE_LQ].fps = pUpdatedProfile->fps;
   }

   if ( pOrgProfile->keyframe_ms != pUpdatedProfile->keyframe_ms )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].keyframe_ms = pUpdatedProfile->keyframe_ms;
      pAllProfiles[VIDEO_PROFILE_LQ].keyframe_ms = pUpdatedProfile->keyframe_ms;
   }

   if ( pOrgProfile->h264profile != pUpdatedProfile->h264profile )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].h264profile = pUpdatedProfile->h264profile;
      pAllProfiles[VIDEO_PROFILE_LQ].h264profile = pUpdatedProfile->h264profile;
   }

   if ( pOrgProfile->h264level != pUpdatedProfile->h264level )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].h264level = pUpdatedProfile->h264level;
      pAllProfiles[VIDEO_PROFILE_LQ].h264level = pUpdatedProfile->h264level;
   }

   if ( pOrgProfile->h264refresh != pUpdatedProfile->h264refresh )
   {
      pAllProfiles[VIDEO_PROFILE_MQ].h264refresh = pUpdatedProfile->h264refresh;
      pAllProfiles[VIDEO_PROFILE_LQ].h264refresh = pUpdatedProfile->h264refresh;
   }

   pAllProfiles[VIDEO_PROFILE_LQ].video_data_length = pUpdatedProfile->video_data_length;
   pAllProfiles[VIDEO_PROFILE_MQ].video_data_length = pUpdatedProfile->video_data_length;

   u32 uNoiseLevel = pUpdatedProfile->uProfileFlags & VIDEO_PROFILE_FLAGS_MASK_NOISE;

   pAllProfiles[VIDEO_PROFILE_MQ].uProfileFlags &= ~ VIDEO_PROFILE_FLAGS_MASK_NOISE;
   pAllProfiles[VIDEO_PROFILE_MQ].uProfileFlags |= uNoiseLevel;
   pAllProfiles[VIDEO_PROFILE_LQ].uProfileFlags &= ~ VIDEO_PROFILE_FLAGS_MASK_NOISE;
   pAllProfiles[VIDEO_PROFILE_LQ].uProfileFlags |= uNoiseLevel;
}

int tx_powers_get_max_usable_power_mw_for_controller()
{
   // Select the max mw power for any present card
   int iMaxPowerMw = 10;
   
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( ! hardware_radio_index_is_wifi_radio(i) )
         continue;
      if ( hardware_radio_index_is_sik_radio(i) )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
         continue;

      t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCRII )
         continue;

      int iCardModel = pCRII->cardModel;
      if ( iCardModel < 0 )
         iCardModel = -iCardModel;

      //int iPowerMaxRaw = tx_powers_get_max_usable_power_raw_for_card(0, iCardModel);
      //int iPowerMw = tx_powers_convert_raw_to_mw(0, iCardModel, iPowerMaxRaw);
      int iPowerMw = tx_powers_get_max_usable_power_mw_for_card(0, iCardModel);
      if ( iPowerMw > iMaxPowerMw )
         iMaxPowerMw = iPowerMw;
   }
   return iMaxPowerMw;
}

void compute_controller_radio_tx_powers(Model* pModel, shared_mem_radio_stats* pSMRS)
{
   ControllerSettings* pCS = get_ControllerSettings();

   log_line("Computing controller radio tx powers. Is fixed power? %s, current model: %s",
      pCS->iFixedTxPower?"yes":"no", (NULL != pModel)?pModel->getLongName():"None");
   if ( pCS->iFixedTxPower )
   {
      log_line("Nothing to compute. Fixed tx powers.");
      return;
   }

   if ( (NULL == pModel) || (NULL == pSMRS) )
   {
      log_line("No model or no radio cards assignment. Set all tx powers to default value.");
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( ! hardware_radio_index_is_wifi_radio(i) )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
            continue;

         t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL == pCRII )
            continue;

         int iCardModel = pCRII->cardModel;
         if ( iCardModel < 0 )
            iCardModel = -iCardModel;
         int iCardMaxPowerMw = tx_powers_get_max_usable_power_mw_for_card(0, iCardModel);
         pCRII->iRawPowerLevel = DEFAULT_RADIO_TX_POWER_CONTROLLER;
         int iPowerMw = tx_powers_convert_raw_to_mw(0, iCardModel, pCRII->iRawPowerLevel);
         log_line("Radio interface %d computed raw tx power to set: %d (%d mW out of max %d mW for this card)",
            i+1, pCRII->iRawPowerLevel, iPowerMw, iCardMaxPowerMw);
      }
      log_line("Done setting all radio cards to default tx power level.");
      return;
   }

   log_line("Computing tx powers for %d vehicle radio links...", pModel->radioLinksParams.links_count);
   
   for( int iLink=0; iLink<pModel->radioLinksParams.links_count; iLink++ )
   {
      int iVehicleLinkMwPower = get_vehicle_radio_link_current_tx_power_mw(pModel, iLink);
      log_line("Vehicle radio link %d current tx power: %d mw", iLink+1, iVehicleLinkMwPower);
      // Set uplink power twice as downlink so it has 1.4 times the range
      iVehicleLinkMwPower *= 2;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         if ( pSMRS->radio_interfaces[i].assignedVehicleRadioLinkId != iLink )
            continue;
         if ( ! hardware_radio_index_is_wifi_radio(i) )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
            continue;

         t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
         if ( NULL == pCRII )
            continue;

         int iCardModel = pCRII->cardModel;
         if ( iCardModel < 0 )
            iCardModel = -iCardModel;
         int iCardMaxPowerMw = tx_powers_get_max_usable_power_mw_for_card(0, iCardModel);
         int iCardPowerMwToSet = iVehicleLinkMwPower;
         if ( iCardPowerMwToSet > iCardMaxPowerMw )
            iCardPowerMwToSet = iCardMaxPowerMw;

         pCRII->iRawPowerLevel = tx_powers_convert_mw_to_raw(0, iCardModel, iCardPowerMwToSet);
         log_line("Radio interface %d for radio link %d computed raw tx power to set: %d (%d mW out of max %d mW for this card)",
            i+1, iLink+1, pCRII->iRawPowerLevel, iCardPowerMwToSet, iCardMaxPowerMw);
      }
   }
   log_line("Done computing all radio cards power levels based on vehicle powers.");
}

void apply_controller_radio_tx_powers(Model* pModel, shared_mem_radio_stats* pSMRS)
{
   compute_controller_radio_tx_powers(pModel, pSMRS);

   ControllerSettings* pCS = get_ControllerSettings();

   log_line("Applying controller radio tx powers. Is fixed power? %s, current model: %s",
      pCS->iFixedTxPower?"yes":"no", (NULL != pModel)?pModel->getLongName():"None");
   log_line("Applying all radio interfaces raw tx powers...");

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( ! hardware_radio_index_is_wifi_radio(i) )
         continue;
      if ( hardware_radio_index_is_sik_radio(i) )
         continue;
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (! pRadioHWInfo->isConfigurable) || (! pRadioHWInfo->isSupported) )
         continue;

      t_ControllerRadioInterfaceInfo* pCRII = controllerGetRadioCardInfo(pRadioHWInfo->szMAC);
      if ( NULL == pCRII )
         continue;

      int iCardModel = pCRII->cardModel;
      if ( iCardModel < 0 )
         iCardModel = -iCardModel;
      int iPowerRawToSet = pCRII->iRawPowerLevel;
      int iPowerMwToSet = tx_powers_convert_raw_to_mw(0, iCardModel, iPowerRawToSet);
      int iCardMaxPowerMw = tx_powers_get_max_usable_power_mw_for_card(0, iCardModel);
      log_line("Radio interface %d set raw tx power: %d (%d mW out of max %d mW for this card)",
          i+1, iPowerRawToSet, iPowerMwToSet, iCardMaxPowerMw);

      if ( hardware_radio_driver_is_rtl8812au_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8812au(i, iPowerRawToSet);
      if ( hardware_radio_driver_is_rtl8812eu_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_rtl8812eu(i, iPowerRawToSet);
      if ( hardware_radio_driver_is_atheros_card(pRadioHWInfo->iRadioDriver) )
         hardware_radio_set_txpower_raw_atheros(i, iPowerRawToSet);
   }
   log_line("Applied all radio interfaces raw tx powers.");
}


bool modelvideoLinkProfileIsOnlyVideoKeyframeChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile)
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

#else
u32 controller_utils_getControllerId() { return 0; }
int controller_utils_export_all_to_usb() { return 0; }
int controller_utils_import_all_from_usb(bool bImportAnyFound) { return 0; }
bool controller_utils_usb_import_has_matching_controller_id_file() { return false; }
bool controller_utils_usb_import_has_any_controller_id_file() { return false; }
int controller_count_asignable_radio_interfaces_to_vehicle_radio_link(Model* pModel, int iVehicleRadioLinkId) { return 0; }
void propagate_video_profile_changes(type_video_link_profile* pOrg, type_video_link_profile* pUpdated, type_video_link_profile* pAllProfiles){}
int tx_powers_get_max_usable_power_mw_for_controller(){ return 0; }
void compute_controller_radio_tx_powers(Model* pModel, shared_mem_radio_stats* pSMRS){}
void apply_controller_radio_tx_powers(Model* pModel, shared_mem_radio_stats* pSMRS){}
bool modelvideoLinkProfileIsOnlyVideoKeyframeChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile) { return false; }
#endif