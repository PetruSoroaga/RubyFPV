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
#include "../base/hardware.h"
#include "../base/hardware_camera.h"
#include "../base/hardware_radio.h"
#include "../base/hw_procs.h"
#include "../base/vehicle_settings.h"
#include "../radio/radioflags.h"
#include "../base/ctrl_settings.h"
#include "first_boot.h"
#include "r_start_vehicle.h"

Model s_ModelFirstBoot;

void do_first_boot_initialization_raspberry(bool bIsVehicle, int iBoardType)
{
   log_line("Doing first time boot setup for Raspberry platform...");
   if ( bIsVehicle )
   {
      hw_execute_bash_command("sudo sed -i 's/ruby/r-vehicle/g' /etc/hosts", NULL);
      hw_execute_bash_command("sudo sed -i 's/ruby/r-vehicle/g' /etc/hostname", NULL);
   }
   else
   {
      hw_execute_bash_command("sudo sed -i 's/ruby/r-controller/g' /etc/hosts", NULL);
      hw_execute_bash_command("sudo sed -i 's/ruby/r-controller/g' /etc/hostname", NULL);
   }

   hardware_sleep_ms(200);
   hardware_mount_boot();

   if ( (iBoardType == BOARD_TYPE_PIZERO) || (iBoardType == BOARD_TYPE_PIZEROW) )
   {
      log_line("Raspberry Pi Zero detected on the first boot ever of the system. Updating settings for Pi Zero.");
      //sprintf(szBuff, "sed -i 's/over_voltage=[0-9]*/over_voltage=%d/g' /boot/config.txt", 5);
      //execute_bash_command(szBuff);
   }
   if ( iBoardType == BOARD_TYPE_PIZERO2 )
   {
      log_line("Raspberry Pi Zero 2 detected on the first boot ever of the system. Updating settings for Pi Zero 2.");
      //sprintf(szBuff, "sed -i 's/over_voltage=[0-9]*/over_voltage=%d/g' /boot/config.txt", 5);
      //execute_bash_command(szBuff);
   }
}

void do_first_boot_initialization_openipc(bool bIsVehicle, int iBoardType)
{
   log_line("Doing first time boot setup for OpenIPC platform...");
   hw_execute_bash_command("ln -s /lib/firmware/ath9k_htc/htc_9271.fw.3 /lib/firmware/ath9k_htc/htc_9271-1.4.0.fw", NULL);
   hardware_set_radio_tx_power_rtl(DEFAULT_RADIO_TX_POWER);
}

void do_first_boot_initialization(bool bIsVehicle, int iBoardType)
{
   log_line("-------------------------------------------------------");
   log_line("First Boot detected. Doing first boot initialization..." );

   #ifdef HW_PLATFORM_RASPBERRY
   do_first_boot_initialization_raspberry(bIsVehicle, iBoardType);
   #endif
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   do_first_boot_initialization_openipc(bIsVehicle, iBoardType);
   #endif

   char szBuff[256];
   char szComm[256];
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, LOG_USE_PROCESS);
   sprintf(szComm, "touch %s%s", FOLDER_CONFIG, LOG_USE_PROCESS);
   hw_execute_bash_command(szComm, szBuff);

   first_boot_create_default_model(bIsVehicle, iBoardType);

   if ( bIsVehicle )
   if ( access( "config/reset_info.txt", R_OK ) != -1 )
   {
      log_line("Found info for reset to defaults. Using it.");
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      if ( ! s_ModelFirstBoot.loadFromFile(szFile, true) )
      {
         s_ModelFirstBoot.resetToDefaults(true);
         s_ModelFirstBoot.is_spectator = false;
      }
      FILE* fd = fopen("config/reset_info.txt", "rb");
      if ( NULL != fd )
      {
         fscanf(fd, "%u %u %d %d %d %s",
            &s_ModelFirstBoot.uVehicleId, &s_ModelFirstBoot.uControllerId,
            &s_ModelFirstBoot.radioLinksParams.link_frequency_khz[0],
            &s_ModelFirstBoot.radioLinksParams.link_frequency_khz[1],
            &s_ModelFirstBoot.radioLinksParams.link_frequency_khz[2],
            szBuff);
         fclose(fd);
      
         if ( szBuff[0] == '*' && szBuff[1] == 0 )
            szBuff[0] = 0;

         for( int i=0; i<(int)strlen(szBuff); i++ )
            if ( szBuff[i] == '_' )
               szBuff[i] = ' ';

         strcpy(s_ModelFirstBoot.vehicle_name, szBuff);
      }
      s_ModelFirstBoot.saveToFile(szFile, false);
      
      hw_execute_bash_command("rm -rf config/reset_info.txt", NULL);
   }

   if ( bIsVehicle )
   {
      #ifdef HW_PLATFORM_OPENIPC_CAMERA
      hardware_camera_apply_all_majestic_settings(&(s_ModelFirstBoot.camera_params[s_ModelFirstBoot.iCurrentCamera].profiles[s_ModelFirstBoot.camera_params[s_ModelFirstBoot.iCurrentCamera].iCurrentProfile]),
          &(s_ModelFirstBoot.video_link_profiles[s_ModelFirstBoot.video_params.user_selected_video_link_profile]));
      #endif
   }
   else
   {
      #ifdef HW_PLATFORM_RASPBERRY
      load_ControllerSettings(); 
      ControllerSettings* pcs = get_ControllerSettings();
      if ( NULL != pcs )
      {
         pcs->iFreqARM = 900;
         if ( iBoardType == BOARD_TYPE_PIZERO2 )
            pcs->iFreqARM = 1000;
         else if ( iBoardType == BOARD_TYPE_PI3B )
            pcs->iFreqARM = 1200;
         else if ( (iBoardType == BOARD_TYPE_PI3BPLUS) || (iBoardType) == BOARD_TYPE_PI4B || (iBoardType == BOARD_TYPE_PI3APLUS) )
            pcs->iFreqARM = 1400;
         else if ( (iBoardType != BOARD_TYPE_PIZERO) && (iBoardType != BOARD_TYPE_PIZEROW) && (iBoardType != BOARD_TYPE_NONE) 
               && (iBoardType != BOARD_TYPE_PI2B) && (iBoardType != BOARD_TYPE_PI2BV11) && (iBoardType != BOARD_TYPE_PI2BV12) )
            pcs->iFreqARM = 1200;

         hardware_mount_boot();
         hardware_sleep_ms(50);
         hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

         config_file_set_value("config.txt", "arm_freq", pcs->iFreqARM);
         config_file_set_value("config.txt", "arm_freq_min", pcs->iFreqARM);

         hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
      }
      #endif
   }

   snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "rm -rf %s%s", FOLDER_CONFIG, FILE_CONFIG_FIRST_BOOT);
   hw_execute_bash_command_silent(szComm, NULL);
   hardware_sleep_ms(50);
   //if ( ! s_isVehicle )
   //   execute_bash_command("raspi-config --expand-rootfs > /dev/null 2>&1", NULL);   

   log_line("First boot initialization completed.");
   log_line("---------------------------------------------------------");
}


Model* first_boot_create_default_model(bool bIsVehicle, int iBoardType)
{
   log_line("Creating a default model.");
   s_ModelFirstBoot.resetToDefaults(true);
   s_ModelFirstBoot.find_and_validate_camera_settings();
   
   if ( bIsVehicle )
   {
      #ifdef HW_PLATFORM_RASPBERRY

      hardware_mount_boot();
      hardware_sleep_ms(50);
      hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

      s_ModelFirstBoot.hwCapabilities.iBoardType = iBoardType;
      s_ModelFirstBoot.iFreqARM = 900;
      if ( iBoardType == BOARD_TYPE_PIZERO2 )
         s_ModelFirstBoot.iFreqARM = 1000;
      else if ( iBoardType == BOARD_TYPE_PI3B )
         s_ModelFirstBoot.iFreqARM = 1200;
      else if ( (iBoardType == BOARD_TYPE_PI3BPLUS) || (iBoardType == BOARD_TYPE_PI4B) || (iBoardType == BOARD_TYPE_PI3APLUS) )
         s_ModelFirstBoot.iFreqARM = 1400;
      else if ( (iBoardType != BOARD_TYPE_PIZERO) && (iBoardType != BOARD_TYPE_PIZEROW) && (iBoardType != BOARD_TYPE_NONE) 
               && (iBoardType != BOARD_TYPE_PI2B) && (iBoardType != BOARD_TYPE_PI2BV11) && (iBoardType != BOARD_TYPE_PI2BV12) )
         s_ModelFirstBoot.iFreqARM = 1200;

      config_file_set_value("config.txt", "arm_freq", s_ModelFirstBoot.iFreqARM);
      config_file_set_value("config.txt", "arm_freq_min", s_ModelFirstBoot.iFreqARM);
      // Enable hdmi, we might need it for debug
      config_file_force_value("config.txt", "hdmi_force_hotplug", 1);
      config_file_force_value("config.txt", "ignore_lcd", 0);
      //config_file_force_value("config.txt", "hdmi_safe", 1);
        
      hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
      #endif

      bool bHasAtheros = false;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( (pNICInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS )
            bHasAtheros = true;
         if ( (pNICInfo->typeAndDriver & 0xFF) == RADIO_TYPE_RALINK )
            bHasAtheros = true;
      }

      s_ModelFirstBoot.setDefaultVideoBitrate();
      
      if ( bHasAtheros )
      {
         for( int i=0; i<s_ModelFirstBoot.radioLinksParams.links_count; i++ )
         {
            s_ModelFirstBoot.radioLinksParams.link_datarate_video_bps[i] = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS;
            s_ModelFirstBoot.radioLinksParams.link_datarate_data_bps[i] = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS;
         }
         for( int i=0; i<s_ModelFirstBoot.radioInterfacesParams.interfaces_count; i++ )
         {
            s_ModelFirstBoot.radioInterfacesParams.interface_datarate_video_bps[i] = 0;
            s_ModelFirstBoot.radioInterfacesParams.interface_datarate_data_bps[i] = 0;
         }
         s_ModelFirstBoot.video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = 5000000;
         s_ModelFirstBoot.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps = 5000000;
         s_ModelFirstBoot.video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps = 5000000;
         s_ModelFirstBoot.video_link_profiles[VIDEO_PROFILE_PIP].bitrate_fixed_bps = 5000000;
      }

      char szFile[128];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      _set_default_sik_params_for_vehicle(&s_ModelFirstBoot);
      s_ModelFirstBoot.saveToFile(szFile, false);
   }
   else
   {
      s_ModelFirstBoot.resetRadioLinksParams();

      s_ModelFirstBoot.radioInterfacesParams.interfaces_count = 1;
      s_ModelFirstBoot.radioInterfacesParams.interface_type_and_driver[0] = 0xFF0000;
      s_ModelFirstBoot.radioInterfacesParams.interface_supported_bands[0] = RADIO_HW_SUPPORTED_BAND_24;
      strcpy(s_ModelFirstBoot.radioInterfacesParams.interface_szMAC[0], "YYYYYY");
      strcpy(s_ModelFirstBoot.radioInterfacesParams.interface_szPort[0], "Y");
      s_ModelFirstBoot.radioInterfacesParams.interface_current_frequency_khz[0] = DEFAULT_FREQUENCY;
      s_ModelFirstBoot.radioInterfacesParams.interface_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      s_ModelFirstBoot.radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;

      s_ModelFirstBoot.radioLinksParams.link_radio_flags[0] = DEFAULT_RADIO_FRAMES_FLAGS;
      s_ModelFirstBoot.radioLinksParams.link_datarate_video_bps[0] = DEFAULT_RADIO_DATARATE_VIDEO;
      s_ModelFirstBoot.radioLinksParams.link_datarate_data_bps[0] = DEFAULT_RADIO_DATARATE_DATA;

      s_ModelFirstBoot.populateRadioInterfacesInfoFromHardware();

      s_ModelFirstBoot.b_mustSyncFromVehicle = true;
      s_ModelFirstBoot.is_spectator = false;
      char szFile[128];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
      s_ModelFirstBoot.saveToFile(szFile, false);
   }

   return &s_ModelFirstBoot;
}