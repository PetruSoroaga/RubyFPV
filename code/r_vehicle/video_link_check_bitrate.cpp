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
#include "../base/shared_mem.h"
#include "../common/string_utils.h"

#include "video_link_stats_overwrites.h"
#include "video_link_check_bitrate.h"
#include "processor_tx_video.h"
#include "utils_vehicle.h"
#include "packets_utils.h"
#include "timers.h"
#include "shared_vars.h"

static u32 s_uTimeLastH264QuantizationCheck =0;
static u32 s_uTimeLastQuantizationChangeTime = 0;
static u8 s_uLastQuantizationSet = 0;
static u8 s_uQuantizationOverflowValue = 0;

void video_link_set_last_quantization_set(u8 uQuantValue)
{
   s_uLastQuantizationSet = uQuantValue;
   s_uTimeLastQuantizationChangeTime = g_TimeNow;
}

u8 video_link_get_oveflow_quantization_value()
{
   return s_uQuantizationOverflowValue;
}

void video_link_reset_overflow_quantization_value()
{
   s_uQuantizationOverflowValue = 0;
}

int video_link_get_default_quantization_for_videobitrate(u32 uVideoBitRate )
{
   if ( uVideoBitRate < 2000000 )
      return 40;
   else if ( uVideoBitRate < 5000000 )
      return 30;
   else
      return 20;
}

void _video_link_quantization_shift(int iDelta)
{
   if (!((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION) )
      return;

   if ( 0 == g_SM_VideoLinkStats.overwrites.currentH264QUantization )
      g_SM_VideoLinkStats.overwrites.currentH264QUantization = video_link_get_default_quantization_for_videobitrate(g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate);
   
   g_SM_VideoLinkStats.overwrites.currentH264QUantization += iDelta;

   if ( g_SM_VideoLinkStats.overwrites.currentH264QUantization > 44 )
      g_SM_VideoLinkStats.overwrites.currentH264QUantization = 44;
   
   if ( g_SM_VideoLinkStats.overwrites.currentH264QUantization < 5 )
      g_SM_VideoLinkStats.overwrites.currentH264QUantization = 5;
   
   if ( g_SM_VideoLinkStats.overwrites.currentH264QUantization == s_uLastQuantizationSet )
      return;
   
   s_uLastQuantizationSet = g_SM_VideoLinkStats.overwrites.currentH264QUantization;
   s_uTimeLastQuantizationChangeTime = g_TimeNow;
   send_control_message_to_raspivid(RASPIVID_COMMAND_ID_QUANTIZATION_MIN, g_SM_VideoLinkStats.overwrites.currentH264QUantization);
}


void video_link_check_adjust_quantization_for_overload()
{
   if ( (g_pCurrentModel == NULL) || (! g_pCurrentModel->hasCamera()) )
      return;
   if ( g_bVideoPaused )
      return;
   if ( (0 == g_TimeStartRaspiVid) || (g_TimeNow < g_TimeStartRaspiVid + 4000) )
      return;
   
   if ( g_TimeNow < s_uTimeLastH264QuantizationCheck + 100 )
      return;
   if ( g_TimeNow < s_uTimeLastQuantizationChangeTime + 200 )
      return;

   s_uTimeLastH264QuantizationCheck = g_TimeNow;

   u32 uTotalVideoBitRate = g_pProcessorTxVideo->getCurrentTotalVideoBitrate();
   u32 uTotalVideoBitRateAvg500 = g_pProcessorTxVideo->getCurrentTotalVideoBitrateAverage500Ms();
   u32 uTotalVideoBitRateAvg1000 = g_pProcessorTxVideo->getCurrentTotalVideoBitrateAverage1Sec();
   
   u32 uVideoBitRate = g_pProcessorTxVideo->getCurrentVideoBitrate();
   u32 uVideoBitRateAvg500 = g_pProcessorTxVideo->getCurrentVideoBitrateAverage500Ms();
   u32 uVideoBitRateAvg1000 = g_pProcessorTxVideo->getCurrentVideoBitrateAverage1Sec();
   u32 uVideoBitRateAvg = g_pProcessorTxVideo->getCurrentVideoBitrateAverage();

   // This in Mbps. 1 = 1 Mbps
   u32 uMinVideoBitrateTxUsed = (u32) get_last_tx_video_datarate_mbps();
   if ( uMinVideoBitrateTxUsed < 2 )
   {
      log_softerror_and_alarm("VideoAdaptiveBitrate: Invalid minimum tx video radio datarate: %d Mbps. Reseting to 2 Mbps.", uMinVideoBitrateTxUsed);
      uMinVideoBitrateTxUsed = 2;
      send_alarm_to_controller(ALARM_ID_VEHICLE_VIDEO_TX_BITRATE_TOO_LOW, 0, 1); 
   }
   
   u32 uMaxVideoBitrateThreshold = (uMinVideoBitrateTxUsed * 1000 * 1000 / 100) * DEFAULT_VIDEO_LINK_MAX_LOAD_PERCENT;
      
   // Auto quantization is turned off ?
   if (!((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION) )
      return;

   // Adaptive video is turned off

   if (!((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS) )
   {
      int dTime = 1000;
      bool bOverflow = false;

      if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH )
      {
         dTime = 500;
         if ( uVideoBitRateAvg500 > g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate * 1.2 )
            bOverflow = true;
         if ( uTotalVideoBitRate > uMaxVideoBitrateThreshold )
            bOverflow = true;
      }
      else
      {
         dTime = 1000;
         if ( uVideoBitRateAvg1000 > g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate * 1.2 )
            bOverflow = true;
         if ( uTotalVideoBitRateAvg500 > uMaxVideoBitrateThreshold )
            bOverflow = true;
      }

      if ( g_TimeNow >= s_uTimeLastQuantizationChangeTime + dTime )
      if ( bOverflow )
      {
         if ( g_SM_VideoLinkStats.overwrites.currentH264QUantization != 0 )
         if ( g_SM_VideoLinkStats.overwrites.currentH264QUantization != 0xFF )
         if ( g_SM_VideoLinkStats.overwrites.currentH264QUantization > s_uQuantizationOverflowValue )
            s_uQuantizationOverflowValue = g_SM_VideoLinkStats.overwrites.currentH264QUantization;
         _video_link_quantization_shift(1);
         return;
      }

      if ( g_TimeNow >= s_uTimeLastQuantizationChangeTime + dTime*2 )
      {
         if ( uVideoBitRateAvg < g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate*0.8 )
         //if ( g_SM_VideoLinkStats.overwrites.currentH264QUantization > s_uQuantizationOverflowValue+1 )
            _video_link_quantization_shift(-1);
      }
      return;
   }

   // Check first if we must quickly shift bitrate down right now fast

   bool bOverflow = false;
   int dTime = 200;
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH )
   {
      dTime = 100;
      if ( uVideoBitRate >= g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate * 1.4 )
         bOverflow = true;
      if ( uTotalVideoBitRate > uMaxVideoBitrateThreshold*1.2 )
         bOverflow = true;
   }
   else
   {
      dTime = 200;
      if ( uVideoBitRateAvg500 >= g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate * 1.4 )
         bOverflow = true;    
      if ( uTotalVideoBitRateAvg500 > uMaxVideoBitrateThreshold*1.2 )
         bOverflow = true;
   }

   if ( g_TimeNow >= s_uTimeLastQuantizationChangeTime + dTime )  
   if ( bOverflow )
   {
      _video_link_quantization_shift(2);
      //if ( g_pCurrentModel->bDeveloperMode )
      //   log_line("[DEV] Auto bitrate: Must qickly shift video bitrate down. Current video bitrate: %.1f Mbps, total video data: %.1f Mbps. Current set video bitrate: %.1f Mbps. Current Q: %d.", uVideoBitRate/1000.0/1000.0,uTotalVideoBitRate/1000.0/1000.0, g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/1000.0/1000.0, g_SM_VideoLinkStats.overwrites.currentH264QUantization);
      return;
   }

   // Must shift bitrate down slower to target

   bOverflow = false;
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH )
   {
      dTime = 200;
      if ( uVideoBitRateAvg500 >= g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate )
         bOverflow = true;
      if ( uTotalVideoBitRateAvg500 >= uMaxVideoBitrateThreshold )
         bOverflow = true;
   }
   else
   {
      dTime = 400;
      if ( uVideoBitRateAvg1000 >= g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate )
         bOverflow = true;    
      if ( uTotalVideoBitRateAvg1000 >= uMaxVideoBitrateThreshold )
         bOverflow = true;
   }

   if ( g_TimeNow >= s_uTimeLastQuantizationChangeTime + dTime )  
   if ( bOverflow )
   {
      _video_link_quantization_shift(1);
      //if ( g_pCurrentModel->bDeveloperMode )
      //   log_line("[DEV] Auto bitrate: Must shift video bitrate down. Current video bitrate: %.1f Mbps, total video data: %.1f Mbps. Current set video bitrate: %.1f Mbps. Current Q: %d.", uVideoBitRate/1000.0/1000.0,uTotalVideoBitRate/1000.0/1000.0, g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/1000.0/1000.0, g_SM_VideoLinkStats.overwrites.currentH264QUantization);
     return;
   }

   // Must shift up to target

   bool bUndeflow = false;
   if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_VIDEO_ADAPTIVE_QUANTIZATION_STRENGTH )
   {
      dTime = 500;
      if ( uVideoBitRateAvg1000 < g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate*0.8 )
         bUndeflow = true;
   }
   else
   {
      dTime = 1000;
      if ( uVideoBitRateAvg < g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate*0.8 )
         bUndeflow = true;    
   }

   if ( g_TimeNow >= s_uTimeLastQuantizationChangeTime + dTime )  
   if ( bUndeflow )
   {
      _video_link_quantization_shift(-1);
      //if ( g_pCurrentModel->bDeveloperMode )
      //   log_line("[DEV] Auto bitrate: Must shift video bitrate up. Current video bitrate: %.1f Mbps, total video data: %.1f Mbps. Current set video bitrate: %.1f Mbps. Current Q: %d.", uVideoBitRate/1000.0/1000.0,uTotalVideoBitRate/1000.0/1000.0, g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/1000.0/1000.0, g_SM_VideoLinkStats.overwrites.currentH264QUantization);
      return;
   }
}

void video_link_check_adjust_bitrate_for_overload()
{
   if ( (0 == g_TimeStartRaspiVid) || (g_TimeNow < g_TimeStartRaspiVid + 5000) )
      return;

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_DISABLE_VIDEO_OVERLOAD_CHECK )
      return;
      
   u32 uTotalVideoBitRate = g_pProcessorTxVideo->getCurrentTotalVideoBitrateAverage();
   u32 uMaxTxTime = DEFAULT_TX_TIME_OVERLOAD;
   uMaxTxTime += (uTotalVideoBitRate/1000/1000)*10;

   // This is in Mbps. 1 = 1 Mbps
   int iMinVideoBitrateTxUsed = get_last_tx_video_datarate_mbps();
   if ( iMinVideoBitrateTxUsed < 2 )
   {
      log_softerror_and_alarm("VideoAdaptiveBitrate: Invalid minimum tx video radio datarate: %d Mbps. Reseting to 2 Mbps.", iMinVideoBitrateTxUsed);
      iMinVideoBitrateTxUsed = 2;
      send_alarm_to_controller(ALARM_ID_VEHICLE_VIDEO_TX_BITRATE_TOO_LOW, 0, 1); 
   }

   int iMinThreshold = (iMinVideoBitrateTxUsed * 1000 * 1000 / 100) * DEFAULT_VIDEO_LINK_MAX_LOAD_PERCENT;
   
   // Check for TX time overload or data rate overload

   if ( g_TimeNow >= g_SM_VideoLinkStats.timeLastAdaptiveParamsChangeDown + 200 )
   if ( g_TimeNow >= g_TimeLastOverwriteBitrateDownOnTxOverload + 1000 )
   if ( g_TimeNow >= g_TimeLastOverwriteBitrateUpOnTxOverload + 1000 )
   if ( (g_uTotalRadioTxTimePerSec > uMaxTxTime) || (uTotalVideoBitRate > (u32)iMinThreshold) )
   if ( video_stats_overwrites_lower_current_profile_top_bitrate(uTotalVideoBitRate) )
   {
      g_TimeLastOverwriteBitrateDownOnTxOverload = g_TimeNow;

      u32 uParam = 0;
      if ( g_uTotalRadioTxTimePerSec > uMaxTxTime )
      {
         uParam = (g_uTotalRadioTxTimePerSec & 0xFFFF) | (uMaxTxTime << 16);
         send_alarm_to_controller(ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD, uParam, 5);
      }
      else
      {
         u32 bitrateTotal = uTotalVideoBitRate;
         bitrateTotal /= 1000;
         u32 bitrateBar = iMinThreshold;
         bitrateBar /= 1000;
         uParam = (bitrateTotal & 0xFFFF) | (bitrateBar << 16);
         send_alarm_to_controller(ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD, uParam, 5);
      }
      return;
   }

   if ( g_TimeNow > g_TimeLastOverwriteBitrateDownOnTxOverload + 4000 )
   if ( g_TimeNow > g_TimeLastOverwriteBitrateUpOnTxOverload + 4000 )
   if ( (g_uTotalRadioTxTimePerSec < uMaxTxTime/3) && (uTotalVideoBitRate < (u32)iMinThreshold) )
   if ( video_stats_overwrites_increase_current_profile_top_bitrate() )
   {
      g_TimeLastOverwriteBitrateUpOnTxOverload = g_TimeNow;
      return;
   }
}
