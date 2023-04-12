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

#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include <ctype.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"
#include "../base/launchers.h"
#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"


bool gbQuit = false;
bool s_isVehicle = false;

void getSystemType()
{
   if ( hardware_is_vehicle() )
   {
      log_line("Detected system as vehicle/relay.");
      s_isVehicle = true;
   }
   else
   {
      log_line("Detected system as controller.");
      s_isVehicle = false;
   }

   log_line("");
   if ( s_isVehicle )
      log_line("| System detected as vehicle/relay.");
   else
      log_line("| System detected as controller.");
   log_line("");
}  

void validate_camera(Model* pModel)
{
   if ( hardware_isCameraVeye() || hardware_isCameraHDMI() )
   {
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         pModel->video_link_profiles[i].width = 1920;
         pModel->video_link_profiles[i].height = 1080;
         if ( pModel->video_link_profiles[i].fps > 30 )
            pModel->video_link_profiles[i].fps = 30;
      }
   }
}


void do_update_to_74()
{
   log_line("Doing update to 7.4");
 
   if ( s_isVehicle )
   {

      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      model.video_link_profiles[VIDEO_PROFILE_MQ].block_packets = DEFAULT_MQ_VIDEO_BLOCK_PACKETS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].block_fecs = DEFAULT_MQ_VIDEO_BLOCK_FECS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps = DEFAULT_MQ_VIDEO_BITRATE;

      // Remove h264 extended video profile
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         if ( model.video_link_profiles[i].h264profile == 3 )
            model.video_link_profiles[i].h264profile = 2;

         model.video_link_profiles[i].encoding_extra_flags |= ENCODING_EXTRA_FLAG_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION;
      }
      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
     
   }
}

void do_update_to_73()
{
   log_line("Doing update to 7.3");
 
   if ( s_isVehicle )
   {
      hardware_init_serial_ports();
      
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      for( int i=0; i<model.radioLinksParams.links_count; i++ )
         model.radioLinksParams.link_capabilities_flags[i] &= ~(RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);

      for( int i=0; i<model.radioInterfacesParams.interfaces_count; i++ )
         model.radioInterfacesParams.interface_capabilities_flags[i] &= ~(RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
   
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         model.video_link_profiles[i].encoding_extra_flags |= ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
         // Auto radio datarates for all video profiles
         model.video_link_profiles[i].radio_datarate_video = 0;
         model.video_link_profiles[i].radio_datarate_data = 0;
      }
   
      model.video_params.lowestAllowedAdaptiveVideoBitrate = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;
      model.video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps = DEFAULT_MQ_VIDEO_BITRATE;
  
      
      model.setDefaultVideoBitrate();
      
      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      
      pCS->iDevSwitchVideoProfileUsingQAButton = -1;  
      save_ControllerSettings();

      loadModelsSpectator();
      loadModels();

      for( int i=0; i<getModelsCount(); i++ )
      {
         Model* pModel = getModelAtIndex(i);
         if ( NULL == pModel )
            continue;

         for( int k=0; k<pModel->radioLinksParams.links_count; k++ )
            pModel->radioLinksParams.link_capabilities_flags[k] &= ~(RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
         for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
            pModel->radioInterfacesParams.interface_capabilities_flags[k] &= ~(RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      }

      for( int i=0; i<getModelsSpectatorCount(); i++ )
      {
         Model* pModel = getModelSpectator(i);
         if ( NULL == pModel )
            continue;

         for( int k=0; k<pModel->radioLinksParams.links_count; k++ )
            pModel->radioLinksParams.link_capabilities_flags[k] &= ~(RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
         for( int k=0; k<pModel->radioInterfacesParams.interfaces_count; k++ )
            pModel->radioInterfacesParams.interface_capabilities_flags[k] &= ~(RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      }
      saveModels();
      saveModelsSpectator();

      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;
      for( int k=0; k<model.radioLinksParams.links_count; k++ )
         model.radioLinksParams.link_capabilities_flags[k] &= ~(RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      for( int k=0; k<model.radioInterfacesParams.interfaces_count; k++ )
         model.radioInterfacesParams.interface_capabilities_flags[k] &= ~(RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, true);
   }
}


void do_update_to_72()
{
   log_line("Doing update to 7.2");
 
   if ( s_isVehicle )
   {
      hardware_init_serial_ports();
      
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      //for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      //   model.osd_params.osd_flags3[i] = OSD_FLAG3_SHOW_GRID_DIAGONAL | OSD_FLAG3_SHOW_GRID_SQUARES;


      if ( 0 < model.hardware_info.serial_bus_count )
      if ( hardware_get_serial_ports_count() > 0 )
      {
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(0);
         if ( NULL != pPortInfo )
         {
            pPortInfo->lPortSpeed = model.hardware_info.serial_bus_speed[0];
            pPortInfo->iPortUsage = (int)(model.hardware_info.serial_bus_supported_and_usage[0] & 0xFF);
            hardware_serial_save_configuration();
         }
      }

      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
      hardware_init_serial_ports();
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      if ( (pCS->iTelemetryInputSerialPortIndex != -1) ||
           (pCS->iTelemetryOutputSerialPortIndex != -1) )
      if ( hardware_get_serial_ports_count() > 0 )
      {
         int iPort = pCS->iTelemetryInputSerialPortIndex;
         if ( iPort < 0 )
            iPort = pCS->iTelemetryOutputSerialPortIndex;
         if ( iPort >= hardware_get_serial_ports_count() )
            iPort = hardware_get_serial_ports_count()-1;
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(iPort);
         if ( NULL != pPortInfo )
         {
            pPortInfo->iPortUsage = SERIAL_PORT_USAGE_TELEMETRY;
            hardware_serial_save_configuration();
         }
      }

      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->iOSDFont = 1;
      save_Preferences();
   }
}


void do_update_to_71()
{
   log_line("Doing update to 7.1");
 
   if ( s_isVehicle )
   {
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      model.video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      
      validate_camera(&model);

      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
      
   }
}


void do_update_to_70()
{
   log_line("Doing update to 7.0");
 
   if ( s_isVehicle )
   {
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      model.video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;
      model.video_params.lowestAllowedAdaptiveVideoBitrate = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;
      model.video_params.uMaxAutoKeyframeInterval = DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL;
      model.video_params.uExtraFlags = 0; // Fill in H264 SPS timings

      model.video_link_profiles[VIDEO_PROFILE_MQ].block_packets = DEFAULT_MQ_VIDEO_BLOCK_PACKETS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].block_fecs = DEFAULT_MQ_VIDEO_BLOCK_FECS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps = DEFAULT_MQ_VIDEO_BITRATE;

      model.video_link_profiles[VIDEO_PROFILE_LQ].block_packets = DEFAULT_LQ_VIDEO_BLOCK_PACKETS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].block_fecs = DEFAULT_LQ_VIDEO_BLOCK_FECS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].bitrate_fixed_bps = DEFAULT_LQ_VIDEO_BITRATE;
   
      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HQ;
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HP;
      model.video_link_profiles[VIDEO_PROFILE_USER].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
      model.video_link_profiles[VIDEO_PROFILE_USER].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HQ;
      
      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      model.video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         model.video_link_profiles[i].keyframe = DEFAULT_VIDEO_KEYFRAME;
         model.video_link_profiles[i].fps = DEFAULT_VIDEO_FPS;
      }
      model.video_link_profiles[VIDEO_PROFILE_MQ].keyframe = DEFAULT_MQ_VIDEO_KEYFRAME;
      model.video_link_profiles[VIDEO_PROFILE_LQ].keyframe = DEFAULT_LQ_VIDEO_KEYFRAME;

      validate_camera(&model);

      model.populateVehicleSerialPorts();
      if ( 0 < model.hardware_info.serial_bus_count )
      {
         model.hardware_info.serial_bus_supported_and_usage[0] &= 0xFFFFFF00;
         model.hardware_info.serial_bus_supported_and_usage[0] |= SERIAL_PORT_USAGE_TELEMETRY;
         model.hardware_info.serial_bus_speed[0] = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
      }
      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->nPingClockSyncFrequency = DEFAULT_PING_FREQUENCY;
      save_ControllerSettings();
   }
}

void do_update_to_69()
{
   log_line("Doing update to 6.9");

   if ( s_isVehicle )
   {
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;
   
      for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      {
         model.osd_params.osd_preferences[i] |= OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM; // controller link lost alarm enabled
         model.osd_params.osd_flags[i] |= OSD_FLAG_SHOW_TIME_LOWER;
         model.osd_params.osd_preferences[i] &= ~(OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_TOP);
         model.osd_params.osd_preferences[i] &= ~(OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_BOTTOM);
         model.osd_params.osd_preferences[i] &= ~(OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_LEFT);
         model.osd_params.osd_preferences[i] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT;

         model.osd_params.osd_preferences[i] |= ((((u32)2)<<16)<<4); // osd stats background transparency
      }

      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         model.video_link_profiles[i].width = DEFAULT_VIDEO_WIDTH;
         model.video_link_profiles[i].height = DEFAULT_VIDEO_HEIGHT;
         model.video_link_profiles[i].h264profile = 2; // extended
         model.video_link_profiles[i].h264level = 2;
         model.video_link_profiles[i].h264refresh = 2;
      }

      model.video_link_profiles[VIDEO_PROFILE_LQ].fps = DEFAULT_LQ_VIDEO_FPS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].keyframe = DEFAULT_LQ_VIDEO_KEYFRAME;
      model.video_link_profiles[VIDEO_PROFILE_LQ].packet_length = DEFAULT_LQ_VIDEO_PACKET_LENGTH;
      model.video_link_profiles[VIDEO_PROFILE_LQ].block_packets = DEFAULT_LQ_VIDEO_BLOCK_PACKETS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].block_fecs = DEFAULT_LQ_VIDEO_BLOCK_FECS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].bitrate_fixed_bps = DEFAULT_LQ_VIDEO_BITRATE;
   
      model.video_link_profiles[VIDEO_PROFILE_MQ].fps = DEFAULT_MQ_VIDEO_FPS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].keyframe = DEFAULT_MQ_VIDEO_KEYFRAME;
      model.video_link_profiles[VIDEO_PROFILE_MQ].packet_length = DEFAULT_MQ_VIDEO_PACKET_LENGTH;
      model.video_link_profiles[VIDEO_PROFILE_MQ].block_packets = DEFAULT_MQ_VIDEO_BLOCK_PACKETS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].block_fecs = DEFAULT_MQ_VIDEO_BLOCK_FECS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps = DEFAULT_MQ_VIDEO_BITRATE;
   

      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].fps = DEFAULT_VIDEO_FPS;
      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].keyframe = DEFAULT_VIDEO_KEYFRAME;
      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HQ;
      
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].fps = DEFAULT_VIDEO_FPS;
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].keyframe = DEFAULT_VIDEO_KEYFRAME;
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HP;
      
      model.video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      model.video_link_profiles[VIDEO_PROFILE_USER].fps = DEFAULT_VIDEO_FPS;
      model.video_link_profiles[VIDEO_PROFILE_USER].keyframe = DEFAULT_VIDEO_KEYFRAME;
      model.video_link_profiles[VIDEO_PROFILE_USER].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
      model.video_link_profiles[VIDEO_PROFILE_USER].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HQ;
      

      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].encoding_extra_flags &= (~(u32)0xFF00);
      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].encoding_extra_flags &= (~(u32)0xFF00);
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
      model.video_link_profiles[VIDEO_PROFILE_USER].encoding_extra_flags &= (~(u32)0xFF00);
      model.video_link_profiles[VIDEO_PROFILE_USER].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
      

      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = 5000000;
      model.video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;

      model.video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags &= (~(u32)0xFF00);
      model.video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_MQ<<8);
      model.video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags &= (~(u32)0xFF00);
      model.video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_LQ<<8);
   
      model.niceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
      model.niceVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_TX;
      model.ioNiceVideo = DEFAULT_IO_PRIORITY_VIDEO_TX;
      model.ioNiceRouter = DEFAULT_IO_PRIORITY_ROUTER;

      model.video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;
      model.video_params.lowestAllowedAdaptiveVideoBitrate = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;
   
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         model.radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
         model.radioLinksParams.link_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
      }

      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         model.radioInterfacesParams.interface_datarates[i][0] = 0;
         model.radioInterfacesParams.interface_datarates[i][1] = 0;
      }

      model.clock_sync_type = CLOCK_SYNC_TYPE_NONE;

      validate_camera(&model);
      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
      pCS->iNiceRXVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_RX;
      pCS->ioNiceRouter = DEFAULT_IO_PRIORITY_ROUTER;
      pCS->ioNiceRXVideo = DEFAULT_IO_PRIORITY_VIDEO_RX;

      pCS->nRetryRetransmissionAfterTimeoutMS = DEFAULT_VIDEO_RETRANS_RETRY_TIME;   
      
      save_ControllerSettings();
   }
}


void do_update_to_68()
{
   log_line("Doing update to 6.8");

   if ( access( "version_ruby_base.txt", R_OK ) == -1 )
      hw_execute_bash_command("cp -rf ruby_ver.txt version_ruby_base.txt", NULL);

   if ( s_isVehicle )
   {
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      model.video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;
      
      model.video_link_profiles[VIDEO_PROFILE_LQ].keyframe = DEFAULT_LQ_VIDEO_KEYFRAME;
      model.video_link_profiles[VIDEO_PROFILE_LQ].fps = DEFAULT_LQ_VIDEO_FPS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].packet_length = DEFAULT_LQ_VIDEO_PACKET_LENGTH;

      model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
      model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
      model.video_link_profiles[VIDEO_PROFILE_USER].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
      model.video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_MQ<<8);
      model.video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_LQ<<8);

      for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
         model.osd_params.osd_flags[i] |= OSD_FLAG_SHOW_RADIO_LINKS | OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS;

      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);     
   }
   else
   {
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->uEnabledAlarms &= (~ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD);
      save_Preferences();  
   }
}


void do_update_to_67()
{
   log_line("Doing update to 6.7");

   if ( access( "version_ruby_base.txt", R_OK ) == -1 )
      hw_execute_bash_command("cp -rf ruby_ver.txt version_ruby_base.txt", NULL);

   if ( s_isVehicle )
   {
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      for( int i=0; i<MODEL_MAX_CAMERAS; i++ )
      {
         for( int k=0; k<MODEL_CAMERA_PROFILES; k++ )
         {
            model.camera_params[i].profiles[k].exposure = 3; // sports
            model.camera_params[i].profiles[k].drc = 0; // off
         }
         model.camera_params[i].profiles[MODEL_CAMERA_PROFILES-1].exposure = 7; // off for HDMI profile
      }   

      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         model.video_link_profiles[i].keyframe = DEFAULT_VIDEO_KEYFRAME;
      }
      
      model.video_link_profiles[VIDEO_PROFILE_MQ].keyframe = DEFAULT_MQ_VIDEO_KEYFRAME;
      model.video_link_profiles[VIDEO_PROFILE_MQ].fps = DEFAULT_MQ_VIDEO_FPS;

      model.video_link_profiles[VIDEO_PROFILE_LQ].keyframe = DEFAULT_LQ_VIDEO_KEYFRAME;
      model.video_link_profiles[VIDEO_PROFILE_LQ].fps = DEFAULT_LQ_VIDEO_FPS;
      model.video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;
      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);     
   }
   else
   {
      
   }
}


void do_update_to_66()
{
   log_line("Doing update to 6.6");

   if ( s_isVehicle )
   {
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      {
         model.osd_params.osd_preferences[i] &= 0xFFFFFF00;
         model.osd_params.osd_preferences[i] |= ((u32)0x05) | (((u32)2)<<8) | (((u32)3)<<16);
         model.osd_params.osd_flags2[i] |= OSD_FLAG_EXT_SHOW_BACKGROUND_ON_TEXTS_ONLY;
      }

      bool bHasAtheros = false;
      for( int i=0; i<model.radioInterfacesParams.interfaces_count; i++ )
      {
         if ( (model.radioInterfacesParams.interface_type_and_driver[i] & 0xFF) == RADIO_TYPE_ATHEROS )
            bHasAtheros = true;
      }

      if ( bHasAtheros )
      {
         for( int i=0; i<model.radioLinksParams.links_count; i++ )
         {
            if ( model.radioLinksParams.link_datarates[i][0] > 12 )
               model.radioLinksParams.link_datarates[i][0] = 12;
            if ( model.radioLinksParams.link_datarates[i][1] > 12 )
               model.radioLinksParams.link_datarates[i][1] = 12;
         }
         for( int i=0; i<model.radioInterfacesParams.interfaces_count; i++ )
         {
            model.radioInterfacesParams.interface_datarates[i][0] = 0;
            model.radioInterfacesParams.interface_datarates[i][1] = 0;
         }
         if ( model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps > 5000000 )
            model.video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = 5000000;
         if ( model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps > 5000000 )
            model.video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps = 5000000;
         if ( model.video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps > 5000000 )
            model.video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps = 5000000;
         if ( model.video_link_profiles[VIDEO_PROFILE_PIP].bitrate_fixed_bps > 5000000 )
            model.video_link_profiles[VIDEO_PROFILE_PIP].bitrate_fixed_bps = 5000000;
      }

      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);     
   }
   else
   {
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->iOSDFont = 2;
      save_Preferences();
   }
}


void do_update_to_65()
{
   log_line("Doing update to 6.5");

   if ( s_isVehicle )
   {
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      model.video_link_profiles[VIDEO_PROFILE_LQ].packet_length = DEFAULT_LQ_VIDEO_PACKET_LENGTH;
      model.video_link_profiles[VIDEO_PROFILE_LQ].block_packets = DEFAULT_LQ_VIDEO_BLOCK_PACKETS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].block_fecs = DEFAULT_LQ_VIDEO_BLOCK_FECS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].bitrate_fixed_bps = DEFAULT_LQ_VIDEO_BITRATE;
   
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         model.video_link_profiles[i].encoding_extra_flags |= ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
         model.video_link_profiles[i].h264profile = 1; // main profile
         model.video_link_profiles[i].h264level = 1; // 4.1
      }
      model.resetVideoLinkProfiles(VIDEO_PROFILE_MQ);
      model.resetVideoLinkProfiles(VIDEO_PROFILE_LQ);
      model.video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;
 
      for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      {
         model.osd_params.osd_flags2[i] |= OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS;
         model.osd_params.osd_flags[i] &= (~OSD_FLAG_EXTENDED_VIDEO_DECODE_HISTORY);
      }

      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->nPingClockSyncFrequency = DEFAULT_PING_FREQUENCY;
      save_ControllerSettings();
   }
}


void do_update_to_64()
{
   log_line("Doing update to 6.4");

   if ( s_isVehicle )
   {
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
         model.video_link_profiles[i].h264level = 1;

      for( int i=0; i<MODEL_MAX_CAMERAS; i++ )
      for( int k=0; k<MODEL_CAMERA_PROFILES; k++ )
         model.camera_params[i].profiles[k].wdr = 1; // for IMX327

      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
      
   }
}


void do_update_to_63()
{
   log_line("Doing update to 6.3");

   if ( s_isVehicle )
   {
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      model.resetVideoLinkProfiles();
      model.video_params.iH264Slices = 1;
      model.video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
         model.video_link_profiles[i].encoding_extra_flags |= ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;

      model.video_link_profiles[VIDEO_PROFILE_MQ].block_packets = DEFAULT_MQ_VIDEO_BLOCK_PACKETS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].block_fecs = DEFAULT_MQ_VIDEO_BLOCK_FECS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps = DEFAULT_MQ_VIDEO_BITRATE;

      model.video_link_profiles[VIDEO_PROFILE_LQ].block_packets = DEFAULT_LQ_VIDEO_BLOCK_PACKETS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].block_fecs = DEFAULT_LQ_VIDEO_BLOCK_FECS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].bitrate_fixed_bps = DEFAULT_LQ_VIDEO_BITRATE;

      model.uModelFlags |= MODEL_FLAG_USE_LOGER_SERVICE;

      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
      
   }
}


void do_update_to_62()
{
   log_line("Doing update to 6.2");

   char szBuff[256];
   sprintf(szBuff, "touch %s", LOG_USE_PROCESS);
   hw_execute_bash_command(szBuff,NULL);

   if ( s_isVehicle )
   {
      Model model;
      if ( ! model.loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
         return;

      for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      {
         model.osd_params.osd_preferences[i] &= ~(0xFF0000);
         model.osd_params.osd_preferences[i] |= ((u32)2)<<16;
      }

      for( int k=0; k<MODEL_MAX_CAMERAS; k++ )
      for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
      {
         model.camera_params[k].profiles[i].brightness = 48;
         model.camera_params[k].profiles[i].contrast = 65;
         model.camera_params[k].profiles[i].saturation = 80;
         model.camera_params[k].profiles[i].sharpness = 110; // 100 is zero
      }

      model.resetVideoLinkProfiles();
      model.video_params.iH264Slices = 1;
      model.video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
         model.video_link_profiles[i].encoding_extra_flags |= ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;

      model.video_link_profiles[VIDEO_PROFILE_MQ].block_packets = DEFAULT_MQ_VIDEO_BLOCK_PACKETS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].block_fecs = DEFAULT_MQ_VIDEO_BLOCK_FECS;
      model.video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps = DEFAULT_MQ_VIDEO_BITRATE;

      model.video_link_profiles[VIDEO_PROFILE_LQ].block_packets = DEFAULT_LQ_VIDEO_BLOCK_PACKETS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].block_fecs = DEFAULT_LQ_VIDEO_BLOCK_FECS;
      model.video_link_profiles[VIDEO_PROFILE_LQ].bitrate_fixed_bps = DEFAULT_LQ_VIDEO_BITRATE;

      model.uModelFlags |= MODEL_FLAG_USE_LOGER_SERVICE;

      model.saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
   }
   else
   {
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iDisableRetransmissionsAfterControllerLinkLostMiliseconds = DEFAULT_CONTROLLER_LINK_MILISECONDS_TIMEOUT_TO_DISABLE_RETRANSMISSIONS;
      pCS->nRetryRetransmissionAfterTimeoutMS = DEFAULT_VIDEO_RETRANS_RETRY_TIME;   
      pCS->nPingClockSyncFrequency = DEFAULT_PING_FREQUENCY;      
      save_ControllerSettings();

      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->iMenusStacked = 0;
      save_Preferences();
   }
}


void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   gbQuit = true;
} 

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("RubyUpdate");
   char szUpdateCommand[1024];

   szUpdateCommand[0] = 0;
   if ( argc >= 2 )
      strcpy(szUpdateCommand,argv[1]);

   log_line("Executing update commands with parameter: [%s]", szUpdateCommand);

   if ( NULL != strstr(szUpdateCommand, "pre" ) )
   {
      log_line("Pre-update step. Do nothing. Exit");
      return 0;
   }

   init_hardware_only_status_led();
   getSystemType();

   u32 uCurrentVersion = 0;
   FILE* fd = fopen(FILE_CONFIG_CURRENT_VERSION, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%u", &uCurrentVersion) )
         uCurrentVersion = 0;
      fclose(fd);
   }
   else
      log_softerror_and_alarm("Failed to read current version id from file: %s",FILE_CONFIG_CURRENT_VERSION);
 
   int iMajor = (int)((uCurrentVersion >> 8) & 0xFF);
   int iMinor = (int)(uCurrentVersion & 0xFF);
   int iBuild = (int)(uCurrentVersion>>16);
   if ( uCurrentVersion == 0 )
   {
      char szVersionPresent[32];
      szVersionPresent[0] = 0;
      fd = fopen(FILE_INFO_LAST_UPDATE, "r");
      if ( NULL != fd )
      {
         if ( 1 != fscanf(fd, "%s", szVersionPresent) )
            szVersionPresent[0] = 0;
         fclose(fd);
      }
      if ( 0 == szVersionPresent[0] )
      {
         fd = fopen(FILE_INFO_VERSION, "r");
         if ( NULL == fd )
            fd = fopen("ruby_ver.txt", "r");

         if ( NULL != fd )
         {
            if ( 1 != fscanf(fd, "%s", szVersionPresent) )
               szVersionPresent[0] = 0;
            fclose(fd);
         }
      }
      if ( 0 == szVersionPresent[0] )
         strcpy(szVersionPresent,"1.0");

      char* p = &szVersionPresent[0];
      while ( *p )
      {
         if ( isdigit(*p) )
           iMajor = iMajor * 10 + ((*p)-'0');
         if ( (*p) == '.' )
           break;
         p++;
      }
      if ( 0 != *p )
      {
         p++;
         while ( *p )
         {
            if ( isdigit(*p) )
               iMinor = iMinor * 10 + ((*p)-'0');
            if ( (*p) == '.' )
              break;
            p++;
         }
      }
      if ( iMinor > 9 )
         iMinor = iMinor/10;
   }

   if ( iMinor > 9 )
      iMinor = iMinor/10;

   log_line("Applying update on existing version: %d.%d (b%d)", iMajor, iMinor, iBuild);
   log_line("Updating to version: %d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);

   hw_execute_bash_command("chmod 777 ruby*", NULL);

   if( access( "ruby_capture_raspi", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_raspi /opt/vc/bin/raspivid", NULL);
   else
      hw_execute_bash_command("cp -rf /opt/vc/bin/raspivid ruby_capture_raspi", NULL);

   if( access( "ruby_capture_veye", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_veye /usr/local/bin/veye_raspivid", NULL);
   else
      hw_execute_bash_command("cp -rf /usr/local/bin/veye_raspivid ruby_capture_veye", NULL);

   if( access( "ruby_capture_veye307", R_OK ) != -1 )
      hw_execute_bash_command("cp -rf ruby_capture_veye307 /usr/local/bin/307/veye_raspivid", NULL);
   else
      hw_execute_bash_command("cp -rf /usr/local/bin/307/veye_raspivid ruby_capture_veye307", NULL);

   hw_execute_bash_command("chmod 777 ruby*", NULL);
   hw_execute_bash_command("chown root:root ruby_*", NULL);
   hw_execute_bash_command("cp -rf ruby_update.log /boot/", NULL);
   
   if ( (iMajor == 6 && iMinor < 2) )
      do_update_to_62();

   if ( (iMajor == 6 && iMinor < 3) )
      do_update_to_63();

   if ( (iMajor == 6 && iMinor < 4) )
      do_update_to_64();

   if ( (iMajor == 6 && iMinor < 5) )
      do_update_to_65();

   if ( (iMajor == 6 && iMinor < 6) )
      do_update_to_66();

   if ( (iMajor == 6 && iMinor < 7) )
      do_update_to_67();

   if ( (iMajor == 6 && iMinor <= 8) )
      do_update_to_68();

   if ( (iMajor == 6 && iMinor <= 9) )
      do_update_to_69();

   if ( iMajor < 7 )
      do_update_to_70();

   if ( (iMajor == 7 && iMinor <= 1) )
      do_update_to_71();

   if ( (iMajor == 7 && iMinor <= 2) )
      do_update_to_72();

   if ( (iMajor == 7 && iMinor <= 3) )
      do_update_to_73();

   if ( (iMajor == 7 && iMinor <= 4) )
      do_update_to_74();

   log_line("Update finished.");
   hardware_release();
   return (0);
} 