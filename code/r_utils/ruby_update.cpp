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

#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include <ctype.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_camera.h"
#include "../base/models_list.h"
#include "../base/hw_procs.h"
#include "../base/radio_utils.h"
#include "../base/config.h"
#include "../base/vehicle_settings.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/utils.h"
#include "../radio/radioflags.h"


bool gbQuit = false;
bool s_isVehicle = false;

int iMajor = 0;
int iMinor = 0;
int iBuild = 0;
   
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

void update_openipc_cpu(Model* pModel)
{
   hardware_set_default_sigmastar_cpu_freq();
   if ( NULL != pModel )
      pModel->processesPriorities.iFreqARM = DEFAULT_FREQ_OPENIPC_SIGMASTAR;
}


void do_update_to_105()
{
   log_line("Doing update to 10.5");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iStreamerOutputMode = 0;
      pCS->iDeveloperMode = 0;
      save_ControllerSettings();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hardware_camera_check_set_oipc_sensor();
   pModel->resetAudioParams();
   #endif

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
      pModel->video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME;
   }
   log_line("Updated model VID %u (%s) to v10.5", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_104()
{
   log_line("Doing update to 10.4");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();

      pCS->iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
      pCS->ioNiceRouter = DEFAULT_IO_PRIORITY_ROUTER;
      pCS->iNiceCentral = DEFAULT_PRIORITY_PROCESS_CENTRAL;
      pCS->iNiceRXVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_RX;
      pCS->ioNiceRXVideo = DEFAULT_IO_PRIORITY_VIDEO_RX;

      pCS->iRadioRxThreadPriority = DEFAULT_PRIORITY_THREAD_RADIO_RX;
      pCS->iRadioTxThreadPriority = DEFAULT_PRIORITY_THREAD_RADIO_TX;
      save_ControllerSettings();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      pModel->osd_params.osd_flags3[i] &= ~((u32)(((u32)0x01)<<12));


   pModel->osd_params.uFlags &= ~(OSD_BIT_FLAGS_SHOW_TEMPS_F);

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_MQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_LQ<<8);


   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      if ( (hardware_getOnlyBoardType() == BOARD_TYPE_PIZERO) ||
           (hardware_getOnlyBoardType() == BOARD_TYPE_PIZEROW) ||
           hardware_board_is_goke(hardware_getOnlyBoardType()) )
         pModel->video_link_profiles[i].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_PI_ZERO;
      if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
         pModel->video_link_profiles[i].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_OPIC_SIGMASTAR;
   }

   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = DEFAULT_HP_VIDEO_BITRATE;
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps = DEFAULT_MQ_VIDEO_BITRATE;
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].bitrate_fixed_bps = DEFAULT_LQ_VIDEO_BITRATE;
   
   pModel->video_params.videoAdjustmentStrength = 6;
   pModel->validate_radio_flags();

   pModel->processesPriorities.iThreadPriorityRadioRx = DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_RX;
   pModel->processesPriorities.iThreadPriorityRadioTx = DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_TX;
   pModel->processesPriorities.iThreadPriorityRouter = DEFAULT_PRIORITY_VEHICLE_THREAD_ROUTER;

   pModel->processesPriorities.iNiceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY;
   pModel->processesPriorities.iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   pModel->processesPriorities.iNiceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY_OIPC;
   pModel->processesPriorities.iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER_OPIC;
   #endif

   log_line("Updated model VID %u (%s) to v10.4", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_103()
{
   log_line("Doing update to 10.3");
 
   if ( ! s_isVehicle )
   {
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->uEnabledAlarms = 0xFFFFFFFF;
      save_Preferences();

      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iRadioTxUsesPPCAP = DEFAULT_USE_PPCAP_FOR_TX;
      pCS->iRadioRxThreadPriority = DEFAULT_PRIORITY_THREAD_RADIO_RX;
      pCS->iRadioTxThreadPriority = DEFAULT_PRIORITY_THREAD_RADIO_TX;
      pCS->iStreamerOutputMode = 0; // SM
      save_ControllerSettings();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;
 
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_MQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_LQ<<8);

   pModel->video_link_profiles[VIDEO_PROFILE_LQ].block_packets = DEFAULT_LQ_VIDEO_BLOCK_PACKETS;
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].block_fecs = DEFAULT_LQ_VIDEO_BLOCK_FECS;
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].bitrate_fixed_bps = DEFAULT_LQ_VIDEO_BITRATE;

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
      pModel->video_link_profiles[i].video_data_length = DEFAULT_VIDEO_DATA_LENGTH;
      pModel->video_link_profiles[i].uProfileFlags = VIDEO_PROFILE_FLAGS_NOISE_AUTO; // auto
   }
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileFlags = 0; // lowest

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      pModel->osd_params.osd_preferences[i] &= ~(OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM); // controller link lost alarm disabled

   pModel->osd_params.osd_flags2[0] |= OSD_FLAG2_LAYOUT_ENABLED;
   pModel->osd_params.osd_flags2[1] &= ~ OSD_FLAG2_LAYOUT_ENABLED;
   pModel->osd_params.osd_flags2[2] &= ~ OSD_FLAG2_LAYOUT_ENABLED;
   pModel->osd_params.osd_flags2[3] |= OSD_FLAG2_LAYOUT_ENABLED;
   pModel->osd_params.osd_flags2[4] &= ~ OSD_FLAG2_LAYOUT_ENABLED;
   if ( (pModel->osd_params.iCurrentOSDLayout != 0) && (pModel->osd_params.iCurrentOSDLayout != 3) )
      pModel->osd_params.iCurrentOSDLayout = 0;
   
   pModel->uModelFlags |= MODEL_FLAG_PRIORITIZE_UPLINK;

   if ( DEFAULT_USE_PPCAP_FOR_TX )
      pModel->uDeveloperFlags |= DEVELOPER_FLAGS_USE_PCAP_RADIO_TX;
   else
      pModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_USE_PCAP_RADIO_TX);

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   update_openipc_cpu(pModel);
   if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
   {
      pModel->processesPriorities.iFreqARM = DEFAULT_FREQ_OPENIPC_SIGMASTAR;
      pModel->processesPriorities.iFreqGPU = 0;
   }
   #endif

   pModel->processesPriorities.iThreadPriorityRadioRx = DEFAULT_PRIORITY_THREAD_RADIO_RX;
   pModel->processesPriorities.iThreadPriorityRadioTx = DEFAULT_PRIORITY_THREAD_RADIO_TX;
   pModel->processesPriorities.iThreadPriorityRouter = DEFAULT_PRIORITY_THREAD_ROUTER;
   pModel->rxtx_sync_type = RXTX_SYNC_TYPE_BASIC;
  
   pModel->video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES;
   if ( hardware_board_is_openipc(hardware_getOnlyBoardType()) )
      pModel->video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES_OIPC;

   pModel->enc_flags = MODEL_ENC_FLAGS_NONE;
   pModel->radioLinksParams.uGlobalRadioLinksFlags &= ~(MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS);
   for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
   {   
      pModel->radioLinksParams.uUplinkDataDataRateType[i] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO;
      pModel->radioLinksParams.uDownlinkDataDataRateType[i] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO;
   }

   pModel->validate_radio_flags();

   log_line("Updated model VID %u (%s) to v10.3", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_102()
{
   log_line("Doing update to 10.2");
 
   if ( ! s_isVehicle )
   {
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->nLogLevel = 1;
      save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;
 
   for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
   {
      if ( pModel->radioLinksParams.link_datarate_video_bps[i] < 0 )
      {
         pModel->radioLinksParams.link_radio_flags[i] |= RADIO_FLAGS_USE_MCS_DATARATES;
         pModel->radioLinksParams.link_radio_flags[i] &= ~RADIO_FLAGS_USE_LEGACY_DATARATES;
      }
      else
      {
         pModel->radioLinksParams.link_radio_flags[i] |= RADIO_FLAGS_USE_LEGACY_DATARATES;
         pModel->radioLinksParams.link_radio_flags[i] &= ~RADIO_FLAGS_USE_MCS_DATARATES;       
      }

      pModel->radioLinksParams.uUplinkDataDataRateType[i] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST;
      pModel->radioLinksParams.uDownlinkDataDataRateType[i] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO;
   }

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   {
      pModel->osd_params.osd_preferences[i] &= 0xFFFFFF00;
      pModel->osd_params.osd_preferences[i] |= 2;
   }   

   if ( hardware_board_is_openipc(hardware_getBoardType()) )
   {
      for( int i=0; i<MODEL_MAX_CAMERAS; i++ )
      {
         for( int k=0; k<MODEL_CAMERA_PROFILES-1; k++ )
         {
            pModel->camera_params[i].profiles[k].shutterspeed = 0; // auto
            if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
               pModel->camera_params[i].profiles[k].shutterspeed = 8; //milisec
         }
      }   
   }

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( ! hardware_board_is_goke(pModel->hwCapabilities.uBoardType) )
         pModel->video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;

      pModel->video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME;
      if ( hardware_board_is_goke(hardware_getOnlyBoardType()) )
         pModel->video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME_OIPC_GOKE;
      if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
         pModel->video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME_OIPC_SIGMASTAR;
   }


   pModel->updateRadioInterfacesRadioFlagsFromRadioLinksFlags();

   log_line("Updated model VID %u (%s) to v10.2", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_101()
{
   log_line("Doing update to 10.1");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->nGraphRadioRefreshInterval = 50;
      pCS->nGraphVideoRefreshInterval = 50;
      pCS->iFixedTxPower = 0;
      save_ControllerSettings();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;
 
   pModel->resetVideoParamsToDefaults();
   pModel->resetVideoLinkProfiles(-1);

   pModel->processesPriorities.iNiceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY;
   pModel->processesPriorities.iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   pModel->processesPriorities.iNiceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY_OIPC;
   pModel->processesPriorities.iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER_OPIC;
   #endif

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)

   if ( hardware_board_is_sigmastar(pModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) )
   {
      pModel->processesPriorities.iFreqGPU = 0;
      hardware_set_oipc_freq_boost(pModel->processesPriorities.iFreqARM, pModel->processesPriorities.iFreqGPU);
   }
   pModel->video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES_OIPC;

   if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
      pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_OPIC_SIGMASTAR;
   
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].fps = DEFAULT_VIDEO_FPS_OIPC;
   }

   #endif
   
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( ! hardware_board_is_goke(pModel->hwCapabilities.uBoardType) )
         pModel->video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;

      pModel->video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME;
      if ( hardware_board_is_goke(hardware_getOnlyBoardType()) )
         pModel->video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME_OIPC_GOKE;
      if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
         pModel->video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME_OIPC_SIGMASTAR;

      pModel->video_link_profiles[i].h264profile = 2; // high
   }

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].iIPQuantizationDelta = DEFAULT_VIDEO_H264_IPQUANTIZATION_DELTA_HP;
   }
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].iIPQuantizationDelta = DEFAULT_VIDEO_H264_IPQUANTIZATION_DELTA_HQ;
   pModel->video_link_profiles[VIDEO_PROFILE_USER].iIPQuantizationDelta = DEFAULT_VIDEO_H264_IPQUANTIZATION_DELTA_HQ;

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_MQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_LQ<<8);

   pModel->radioInterfacesParams.iAutoVehicleTxPower = 1;
   pModel->radioInterfacesParams.iAutoControllerTxPower = 1;

   pModel->rxtx_sync_type = RXTX_SYNC_TYPE_BASIC;

   log_line("Updated model VID %u (%s) to v10.1", pModel->uVehicleId, pModel->getLongName());
}

void do_update_to_100()
{
   log_line("Doing update to 10.0");
 
   if ( ! s_isVehicle )
   {
      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->iColorOSDOutline[0] = 10;
      pP->iColorOSDOutline[1] = 10;
      pP->iColorOSDOutline[2] = 10;
      pP->iColorOSDOutline[3] = 90; // 90%
      save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;
 
   pModel->setDefaultVideoBitrate();
   pModel->video_params.user_selected_video_link_profile = VIDEO_PROFILE_HIGH_QUALITY;

   pModel->rxtx_sync_type = RXTX_SYNC_TYPE_BASIC;
   pModel->processesPriorities.uProcessesFlags = PROCESSES_FLAGS_BALANCE_INT_CORES;

   for( int k=0; k<MODEL_MAX_CAMERAS; k++ )
   {
      for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
      {
         pModel->camera_params[k].profiles[i].uFlags |= CAMERA_FLAG_OPENIPC_3A_SIGMASTAR;
      }
   }

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   {
      pModel->osd_params.osd_flags[i] |= OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE;
   }

   pModel->resetVideoLinkProfiles(-1);

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   if ( hardware_board_is_sigmastar(pModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) )
   {
      pModel->processesPriorities.iFreqGPU = 0;
      hardware_set_oipc_freq_boost(pModel->processesPriorities.iFreqARM, pModel->processesPriorities.iFreqGPU);
   }
   #endif

   log_line("Updated model VID %u (%s) to v10.0", pModel->uVehicleId, pModel->getLongName());
}

void do_update_to_98()
{
   log_line("Doing update to 9.8");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      //ControllerSettings* pCS = get_ControllerSettings();
      save_ControllerSettings(); 
      
      load_Preferences();
      //Preferences* pP = get_Preferences();
      save_Preferences();

      #if defined (HW_PLATFORM_RADXA_ZERO3)
      hardware_set_default_radxa_cpu_freq();
      hw_execute_bash_command("sed -i '/98:03:cf/d' /etc/udev/rules.d/98-custom-wifi.rules", NULL);
      #endif
   }

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hw_execute_bash_command("sed -i 's/console:/#console:/' /etc/inittab", NULL);
   #endif

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   pModel->rc_params.iRCTranslationType = RC_TRANSLATION_TYPE_2000;

   pModel->setDefaultVideoBitrate();
   
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   if ( hardware_board_is_sigmastar(pModel->hwCapabilities.uBoardType & BOARD_TYPE_MASK) )
   {
      pModel->processesPriorities.iFreqGPU = 0;
      hardware_set_oipc_freq_boost(pModel->processesPriorities.iFreqARM, pModel->processesPriorities.iFreqGPU);
   
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         pModel->video_link_profiles[i].fps = DEFAULT_VIDEO_FPS_OIPC;
      }
   }
   #endif

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME;
      if ( hardware_board_is_goke(hardware_getOnlyBoardType()) )
         pModel->video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME_OIPC_GOKE;

      pModel->video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
   }

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HQ;
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].block_packets = DEFAULT_MQ_VIDEO_BLOCK_PACKETS;
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].block_fecs = DEFAULT_MQ_VIDEO_BLOCK_FECS;
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].block_packets = DEFAULT_LQ_VIDEO_BLOCK_PACKETS;
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].block_fecs = DEFAULT_LQ_VIDEO_BLOCK_FECS;

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_MQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_LQ<<8);

   log_line("Updated model VID %u (%s) to v9.8", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_97()
{
   log_line("Doing update to 9.7");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iRadioTxUsesPPCAP = DEFAULT_USE_PPCAP_FOR_TX;
      pCS->iRadioBypassSocketBuffers = DEFAULT_BYPASS_SOCKET_BUFFERS;
      save_ControllerSettings(); 

      for( int i=0; i<hardware_get_serial_ports_count(); i++ )
      {
         hw_serial_port_info_t* pInfo = hardware_get_serial_port_info(i);
         if ( NULL == pInfo )
            continue;
         if ( pInfo->iPortUsage > 0 )
         if ( pInfo->iPortUsage < SERIAL_PORT_USAGE_DATA_LINK )
         {
            pInfo->iPortUsage = SERIAL_PORT_USAGE_TELEMETRY_MAVLINK;
            if ( pInfo->lPortSpeed <= 0 )
               pInfo->lPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
            hardware_serial_save_configuration();
         }
      }

      load_Preferences();
      Preferences* pP = get_Preferences();
      pP->iDebugMaxPacketSize = MAX_VIDEO_PACKET_DATA_SIZE;
      save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   if ( DEFAULT_USE_PPCAP_FOR_TX )
      pModel->uDeveloperFlags |= DEVELOPER_FLAGS_USE_PCAP_RADIO_TX;
   else
      pModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_USE_PCAP_RADIO_TX);

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   update_openipc_cpu(pModel);
   #endif
     
   for( int i=0; i<pModel->hardwareInterfacesInfo.serial_port_count; i++ )
   {
      u32 uUsage = pModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF;
      if ( uUsage > 0 )
      if ( uUsage < SERIAL_PORT_USAGE_MSP_OSD )
      {
         pModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] = pModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFFFFFF00;
         pModel->hardwareInterfacesInfo.serial_port_supported_and_usage[i] |= SERIAL_PORT_USAGE_TELEMETRY_MAVLINK;
      }
   }

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].video_data_length = DEFAULT_VIDEO_DATA_LENGTH;
   }

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   {
      pModel->osd_params.osd_flags3[i] |= OSD_FLAG3_RENDER_MSP_OSD;
   }
   pModel->osd_params.uFlags = OSD_BIT_FLAGS_SHOW_FLIGHT_END_STATS;
   
   log_line("Updated model VID %u (%s) to v9.7", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_96()
{
   log_line("Doing update to 9.6");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iRadioTxUsesPPCAP = DEFAULT_USE_PPCAP_FOR_TX;
      pCS->iRadioBypassSocketBuffers = DEFAULT_BYPASS_SOCKET_BUFFERS;
      save_ControllerSettings();      
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   update_openipc_cpu(pModel);
   #endif

   if ( DEFAULT_USE_PPCAP_FOR_TX )
      pModel->uDeveloperFlags |= DEVELOPER_FLAGS_USE_PCAP_RADIO_TX;
   else
      pModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_USE_PCAP_RADIO_TX);

   pModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE;

   if ( DEFAULT_BYPASS_SOCKET_BUFFERS )
      pModel->radioLinksParams.uGlobalRadioLinksFlags |= MODEL_RADIOLINKS_FLAGS_BYPASS_SOCKETS_BUFFERS;
   else
      pModel->radioLinksParams.uGlobalRadioLinksFlags &= ~MODEL_RADIOLINKS_FLAGS_BYPASS_SOCKETS_BUFFERS;

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( pModel->video_link_profiles[i].keyframe_ms < 0 )
      {
         pModel->video_link_profiles[i].keyframe_ms = - pModel->video_link_profiles[i].keyframe_ms;
         pModel->video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;   
      }
      else
         pModel->video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;   
   }

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   {
      pModel->osd_params.osd_flags2[i] &= (~OSD_FLAG2_SHOW_STATS_RADIO_LINKS);
   }

   log_line("Updated model VID %u (%s) to v9.6", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_95()
{
   log_line("Doing update to 9.5");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      //ControllerSettings* pCS = get_ControllerSettings();
      save_ControllerSettings();      
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   update_openipc_cpu(pModel);
   #endif

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;   
      pModel->video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION;
      pModel->video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH_HIGH;

      #if defined (HW_PLATFORM_OPENIPC_CAMERA)
      if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
      {
         if ( pModel->video_link_profiles[i].fps < DEFAULT_VIDEO_FPS_OIPC )
            pModel->video_link_profiles[i].fps = DEFAULT_VIDEO_FPS_OIPC;
      }
      #endif
   }
   pModel->processesPriorities.iThreadPriorityRadioRx = DEFAULT_PRIORITY_THREAD_RADIO_RX;
   pModel->processesPriorities.iThreadPriorityRadioTx = DEFAULT_PRIORITY_THREAD_RADIO_TX;
   pModel->processesPriorities.iThreadPriorityRouter = DEFAULT_PRIORITY_THREAD_ROUTER;

   log_line("Updated model VID %u (%s) to v9.5", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_94()
{
   log_line("Doing update to 9.4");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
      pCS->iNiceRXVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_RX;
      save_ControllerSettings();

      #ifdef HW_PLATFORM_RADXA_ZERO3
      char szOutput[2048];
      szOutput[0] = 0;
      hw_execute_bash_command_raw("cat /etc/NetworkManager/NetworkManager.conf | grep unmanaged-devices", szOutput);
      if ( (0 == szOutput[0]) || (NULL == strstr(szOutput, "unmanaged-devices")) )
      {
         log_line("Updating network manager conf for Radxa...");
         hw_execute_bash_command(" echo '[keyfile]' >> /etc/NetworkManager/NetworkManager.conf", NULL);
         hw_execute_bash_command(" echo 'unmanaged-devices=interface-name:wlan0;interface-name:wlan1;interface-name:wlan2;interface-name:wlan3;interface-name:wlx' >> /etc/NetworkManager/NetworkManager.conf", NULL);
         hw_execute_bash_command(" echo '' >> /etc/NetworkManager/NetworkManager.conf", NULL);
      }
      else
         log_line("Network manager conf for Radxa is ok.");
      #endif
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   log_line("Updated model VID %u (%s) to v9.4", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_93()
{
   log_line("Doing update to 9.3");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
      pCS->iNiceRXVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_RX;
      save_ControllerSettings();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   if ( pModel->telemetry_params.update_rate > DEFAULT_TELEMETRY_SEND_RATE )
      pModel->telemetry_params.update_rate = DEFAULT_TELEMETRY_SEND_RATE;


   // To fix: Remove this when fast FPS decoding works fine on Radxa, for now set fps to 30
   //for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   //   pModel->video_link_profiles[i].fps = DEFAULT_VIDEO_FPS;

   log_line("Updated model VID %u (%s) to v9.3", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_92()
{
   log_line("Doing update to 9.2");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
      pCS->iNiceRXVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_RX;
      save_ControllerSettings();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   if ( pModel->telemetry_params.update_rate > DEFAULT_TELEMETRY_SEND_RATE )
      pModel->telemetry_params.update_rate = DEFAULT_TELEMETRY_SEND_RATE;

   log_line("Updated model VID %u (%s) to v9.2", pModel->uVehicleId, pModel->getLongName());
}

void do_update_to_91()
{
   log_line("Doing update to 9.1");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->iNiceRXVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_RX;
      pCS->iShowVideoStreamInfoCompactType = 1;
      save_ControllerSettings();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   pModel->telemetry_params.flags |= TELEMETRY_FLAGS_ALLOW_ANY_VEHICLE_SYSID;
   pModel->video_params.uMaxAutoKeyframeIntervalMs = DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL;
   pModel->video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES;
   
   if ( s_isVehicle )
   if ( hardware_board_is_openipc(hardware_getBoardType()) )
   {
      for( int i=0; i<MODEL_MAX_CAMERAS; i++ )
      {
         for( int k=0; k<MODEL_CAMERA_PROFILES-1; k++ )
         {
            pModel->camera_params[i].profiles[k].shutterspeed = 0; // auto
            if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
               pModel->camera_params[i].profiles[k].shutterspeed = 10; //milisec
         }
      }   
   }

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   {
      //pModel->osd_params.osd_flags2[i] |= OSD_FLAG2_FLASH_OSD_ON_TELEMETRY_DATA_LOST;
   }

   log_line("Updated model VID %u (%s) to v9.1", pModel->uVehicleId, pModel->getLongName());
}


void do_update_to_90()
{
   log_line("Doing update to 9.0");
 
   if ( ! s_isVehicle )
   {
      load_ControllerSettings();
      ControllerSettings* pCS = get_ControllerSettings();
      pCS->nRetryRetransmissionAfterTimeoutMS = DEFAULT_VIDEO_RETRANS_MINIMUM_RETRY_INTERVAL;
      pCS->nRequestRetransmissionsOnVideoSilenceMs = DEFAULT_VIDEO_RETRANS_REQUEST_ON_VIDEO_SILENCE_MS;
      pCS->nRetryRetransmissionAfterTimeoutMS = DEFAULT_VIDEO_RETRANS_MINIMUM_RETRY_INTERVAL;   

      save_ControllerSettings();

      //load_Preferences();
      //Preferences* pP = get_Preferences();
      //pP->iPersistentMessages = 1;
      //save_Preferences();
   }

   Model* pModel = getCurrentModel();
   if ( NULL == pModel )
      return;

   // Remove deprecated DEVELOPER-FLAGS_BIT-USE_OLD_EC_SCHEME flag
   pModel->uDeveloperFlags &= ~((u32)(((u32)0x01)<<18));

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HQ;

   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HP;
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].radio_datarate_video_bps = DEFAULT_HP_VIDEO_RADIO_DATARATE;
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = DEFAULT_HP_VIDEO_BITRATE;

   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS | VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST;
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION;
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;

   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME;
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME;

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      pModel->video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT;
      pModel->video_link_profiles[i].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_LOWBIT);
   }

   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_MQ<<8);
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~(VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK);
   pModel->video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_LQ<<8);

   pModel->video_params.user_selected_video_link_profile = VIDEO_PROFILE_BEST_PERF;

   pModel->rc_params.rc_frames_per_second = DEFAULT_RC_FRAMES_PER_SECOND;
   
   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   {
      pModel->osd_params.osd_flags3[i] |= OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS;
   }

   log_line("Updated model VID %u (%s) to v9.0", pModel->uVehicleId, pModel->getLongName());
}

void do_generic_update()
{
   log_line("Doing generic update step");
   hw_execute_bash_command("chmod 777 ruby*", NULL);

   #if defined (HW_PLATFORM_RASPBERRY)
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
   #endif

   hw_execute_bash_command("chmod 777 ruby*", NULL);
   hw_execute_bash_command("chown root:root ruby_*", NULL);
   
   #if defined (HW_PLATFORM_RASPBERRY)
   hw_execute_bash_command("cp -rf ruby_update.log /boot/", NULL);
   #endif
   #if defined (HW_PLATFORM_RADXA_ZERO3)
   hw_execute_bash_command("cp -rf ruby_update.log /config/", NULL);
   #endif
   #if defined (HW_PLATFORM_OPENIPC_CAMERA)
   hw_execute_bash_command("cp -rf ruby_update.log /root/ruby/", NULL);
   #endif

   // Remove old unsuported plugins formats

   hw_execute_bash_command("rm -rf plugins/osd/ruby_ahi*", NULL);
   hw_execute_bash_command("rm -rf plugins/osd/ruby_plugin_gauge_speed.so*", NULL);
   hw_execute_bash_command("rm -rf plugins/osd/ruby_plugin_gauge_altitude.so*", NULL);
   hw_execute_bash_command("rm -rf plugins/osd/ruby_plugin_gauge_ahi.so*", NULL);
   hw_execute_bash_command("rm -rf plugins/osd/ruby_plugin_gauge_heading.so*", NULL);
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
   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("RubyUpdate");

   if ( argc >= 3 )
   {
      iMajor = atoi(argv[1]);
      iMinor = atoi(argv[2]);
      if ( iMinor >= 10 )
         iMinor /= 10;
      if ( (iMajor < 9) || ((iMajor == 9) && (iMinor <= 4)) )
         printf("There is a new driver added for RTL8812EU cards. Please do this update again to complete the drivers instalation.\n");
      return 0;
   }

   char szUpdateCommand[1024];

   szUpdateCommand[0] = 0;
   if ( argc >= 2 )
      strcpy(szUpdateCommand,argv[1]);

   log_line("Executing update commands with parameter: [%s]", szUpdateCommand);

   if ( NULL != strstr(szUpdateCommand, "pre" ) )
   {
      log_line("Pre-update step...");
      log_line("Done executing pre-update step. Exit.");
      return 0;
   }

   init_hardware_only_status_led();
   getSystemType();

   u32 uCurrentVersion = 0;
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VERSION);
   FILE* fd = fopen(szFile, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%u", &uCurrentVersion) )
         uCurrentVersion = 0;
      fclose(fd);
   }
   else
      log_softerror_and_alarm("Failed to read current version id from file: %s",FILE_CONFIG_CURRENT_VERSION);
 
   iMajor = (int)((uCurrentVersion >> 8) & 0xFF);
   iMinor = (int)(uCurrentVersion & 0xFF);
   iBuild = (int)(uCurrentVersion>>16);
   if ( uCurrentVersion == 0 )
   {
      char szVersionPresent[32];
      szVersionPresent[0] = 0;
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_INFO_LAST_UPDATE);
      fd = fopen(szFile, "r");
      if ( NULL != fd )
      {
         if ( 1 != fscanf(fd, "%s", szVersionPresent) )
            szVersionPresent[0] = 0;
         fclose(fd);
      }
      if ( 0 == szVersionPresent[0] )
      {
         fd = try_open_base_version_file(NULL);

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

   do_generic_update();

   loadAllModels();

   if ( (iMajor < 9) || (iMajor == 9 && iMinor < 1) )
      do_update_to_90();
   if ( (iMajor < 9) || (iMajor == 9 && iMinor <= 1) )
      do_update_to_91();
   if ( (iMajor < 9) || (iMajor == 9 && iMinor <= 2) )
      do_update_to_92();
   if ( (iMajor < 9) || (iMajor == 9 && iMinor <= 3) )
      do_update_to_93();

   if ( (iMajor < 9) || (iMajor == 9 && iMinor <= 4) )
      do_update_to_94();

   if ( (iMajor < 9) || (iMajor == 9 && iMinor <= 5) )
      do_update_to_95();
   if ( (iMajor < 9) || (iMajor == 9 && iMinor <= 6) )
      do_update_to_96();
   if ( (iMajor < 9) || (iMajor == 9 && iMinor <= 7) )
      do_update_to_97();
   if ( (iMajor < 9) || (iMajor == 9 && iMinor <= 8) )
      do_update_to_98();
   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 0) )
      do_update_to_100();
   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 1) )
      do_update_to_101();
   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 2) )
      do_update_to_102();
   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 3) )
      do_update_to_103();
   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 4) )
      do_update_to_104();
   if ( (iMajor < 10) || (iMajor == 10 && iMinor <= 5) )
      do_update_to_105();


   saveCurrentModel();
   
   log_line("Update finished.");
   hardware_release();
   return (0);
} 