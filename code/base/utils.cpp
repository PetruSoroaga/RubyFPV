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

#include "utils.h"
#include <math.h>
#include "../base/config.h"
#include "../base/models.h"
#include "../common/string_utils.h"

bool ruby_is_first_pairing_done()
{
   if ( access( FILE_FIRST_PAIRING_DONE, R_OK ) == -1 )
      return false;
   return true;
}

void ruby_set_is_first_pairing_done()
{
   FILE* fd = fopen(FILE_FIRST_PAIRING_DONE, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to create file for marking 'first pairing done' flag.");
      return;
   }
   fprintf(fd, "1");
   fclose(fd);
   log_line("Set 'first pairing done' flag to true.");
}

void reset_sik_state_info(t_sik_radio_state* pState)
{
   if ( NULL == pState )
      return;

   pState->bConfiguringSiKThreadWorking = false;
   pState->iThreadRetryCounter = 0;
   pState->bMustReinitSiKInterfaces = false;
   pState->iMustReconfigureSiKInterfaceIndex = -1;
   pState->uTimeLastSiKReinitCheck = 0;
   pState->uTimeIntervalSiKReinitCheck = 500;
   pState->uSiKInterfaceIndexThatBrokeDown = MAX_U32;

   pState->bConfiguringToolInProgress = false;
   pState->uTimeStartConfiguring = 0;
   
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      pState->bInterfacesToReopen[i] = false;
}

int _compute_controller_rc_value_button(Model* pModel, int nChannel, int prevRCValue, hw_joystick_info_t* pJoystick, t_ControllerInputInterface* pCtrlInterface)
{
   u32 dwAssignment = pModel->rc_params.rcChAssignment[nChannel];

   // Check to match description from models.h and config_rc.h bit flags
   // first byte:
          // bit 0: no assignment (0/1);
          // bit 1: 0 - axe, 1 - button;
          // bit 2: 0 - momentary, 1 - sticky button (togle);
          // bit 3: not used
          // bit 4..7 axe/button index (1 index based: first button/axe is 1)
   // each additional 4 bits: positive value (!=0) for each extra button (in increasing order of output RC value)
   // bit 31: toggle flag


   int buttonsIndexes[12];
   int countButtons = 0;

   // Get all assigned buttons to this channel

   for( int shift = 4; shift <= 24; shift += 4 )
   {
      if ( ((dwAssignment >> shift) & 0x0F ) != 0x00 )
      {
         buttonsIndexes[countButtons] = ((dwAssignment >> shift) & 0x0F) - 1;
         countButtons++;
      }
   }

   if ( 0 == countButtons )
      return 0;

   int result = 0;
   int rawValue = 0;

   bool bToggleMode = false;
   if ( dwAssignment & RC_CH_ASSIGNMENT_FLAG_TOGGLE )
      bToggleMode = true;

   //log_line("ch %d, tgl: %d", nChannel, bToggle);

   // Non toggle buttons ?

   if ( ! bToggleMode )
   {
      if ( 1 == countButtons && buttonsIndexes[0] < pJoystick->countButtons )
      {
         rawValue = pJoystick->buttonsValues[buttonsIndexes[0]];

         if ( rawValue == 0 )
         {
            result = pModel->rc_params.rcChMin[nChannel];
            if ( pModel->rc_params.rcChFlags[nChannel] & RC_CH_FLAGS_INVERTED )
               result = pModel->rc_params.rcChMax[nChannel];
         }
         else
         {
            result = pModel->rc_params.rcChMax[nChannel];
            if ( pModel->rc_params.rcChFlags[nChannel] & RC_CH_FLAGS_INVERTED )
               result = pModel->rc_params.rcChMin[nChannel];
         }
         return result;
      }

      result = pModel->rc_params.rcChMin[nChannel];
      if ( pModel->rc_params.rcChFlags[nChannel] & RC_CH_FLAGS_INVERTED )
         result = pModel->rc_params.rcChMax[nChannel];

      for( int i=0; i<countButtons; i++ )
      {
         if ( buttonsIndexes[i] >= pJoystick->countButtons )
            continue;
         rawValue = pJoystick->buttonsValues[buttonsIndexes[i]];
         if ( rawValue != 0 )
         {
            result = pModel->rc_params.rcChMin[nChannel] + (pModel->rc_params.rcChMax[nChannel]-pModel->rc_params.rcChMin[nChannel])*(i+1)/countButtons;
            if ( pModel->rc_params.rcChFlags[nChannel] & RC_CH_FLAGS_INVERTED )
               result = pModel->rc_params.rcChMax[nChannel] - (pModel->rc_params.rcChMax[nChannel]-pModel->rc_params.rcChMin[nChannel])*(i+1)/countButtons;
            return result;
         }
      }
      return result;
   }

   // Toggle buttons

   if ( 1 == countButtons )
   {
      if ( 0 == prevRCValue )
      {
         result = pModel->rc_params.rcChMin[nChannel];
         if ( pModel->rc_params.rcChFlags[nChannel] & RC_CH_FLAGS_INVERTED )
            result = pModel->rc_params.rcChMax[nChannel];
      }
      else
         result = prevRCValue;

      rawValue = pJoystick->buttonsValues[buttonsIndexes[0]];

      if ( rawValue != 0 && 0 == pJoystick->buttonsValuesPrev[buttonsIndexes[0]])
      {
         if ( prevRCValue >= (pModel->rc_params.rcChMax[nChannel]+pModel->rc_params.rcChMin[nChannel])/2 )
            result = pModel->rc_params.rcChMin[nChannel];
         else
            result = pModel->rc_params.rcChMax[nChannel];
      }
      return result;
   }

   // Multiple toggle buttons

   if ( 0 == prevRCValue )
   {
      result = pModel->rc_params.rcChMin[nChannel];
      if ( pModel->rc_params.rcChFlags[nChannel] & RC_CH_FLAGS_INVERTED )
         result = pModel->rc_params.rcChMax[nChannel];
   }
   else
      result = prevRCValue;

   for( int i=0; i<countButtons; i++ )
   {
      if ( buttonsIndexes[i] >= pJoystick->countButtons )
         continue;
      rawValue = pJoystick->buttonsValues[buttonsIndexes[i]];
      if ( rawValue != 0 && 0 == pJoystick->buttonsValuesPrev[buttonsIndexes[i]])
      {
         result = pModel->rc_params.rcChMin[nChannel] + (pModel->rc_params.rcChMax[nChannel]-pModel->rc_params.rcChMin[nChannel])*i/(countButtons-1);
         if ( pModel->rc_params.rcChFlags[nChannel] & RC_CH_FLAGS_INVERTED )
            result = pModel->rc_params.rcChMax[nChannel] - (pModel->rc_params.rcChMax[nChannel]-pModel->rc_params.rcChMin[nChannel])*i/(countButtons-1);
         return result;
      }
   }

   return result;
}

float _compute_controller_rc_ranged_value(Model* pModel, int nChannel, float prevRCValue, float fNormalizedValue, u32 miliSec)
{
   float rcValue = pModel->rc_params.rcChMid[nChannel];

   if ( fNormalizedValue < 0.0 ) fNormalizedValue = 0.0;
   if ( fNormalizedValue > 1.0 ) fNormalizedValue = 1.0;

   if ( pModel->rc_params.rcChFlags[nChannel] & RC_CH_FLAGS_INVERTED )
      fNormalizedValue = 1.0 - fNormalizedValue;

   int nCamSpeed = ((pModel->camera_rc_channels >> 24) & 0x1F);
   if ( nCamSpeed <= 0 )
      nCamSpeed = 1;

   int nCamPitch = (pModel->camera_rc_channels & 0x1F);
   int nCamRoll = ((pModel->camera_rc_channels >> 8) & 0x1F);
   int nCamYaw = ((pModel->camera_rc_channels >> 16) & 0x1F);
   
   bool isRelativeMove = false;

   if ( nCamPitch > 0 && (nChannel == nCamPitch-1) )
      isRelativeMove = true;
   if ( nCamRoll > 0 && (nChannel == nCamRoll-1) )
      isRelativeMove = true;
   if ( nCamYaw > 0 && (nChannel == nCamYaw-1) )
      isRelativeMove = true;

   if ( (((pModel->camera_rc_channels >> 24) & 0xFF) >> 5) & 0x01 )
      isRelativeMove = false;


   if ( fNormalizedValue >= 0.5 )
   {
      fNormalizedValue = (fNormalizedValue-0.5)*2.0;
      fNormalizedValue = powf(fNormalizedValue, 1.0+0.1*(float)pModel->rc_params.rcChExpo[nChannel]) * 0.7 + 0.3 * fNormalizedValue;

      if ( isRelativeMove )
      {
         if ( prevRCValue < 10 )
            prevRCValue = pModel->rc_params.rcChMid[nChannel];
         if ( fNormalizedValue > 0.0001 )
            rcValue = prevRCValue + fNormalizedValue * ((float)nCamSpeed) * ((float)miliSec/20.0);
         else
            rcValue = prevRCValue;
         if ( rcValue > pModel->rc_params.rcChMax[nChannel] )
            rcValue = pModel->rc_params.rcChMax[nChannel];
         if ( rcValue < pModel->rc_params.rcChMin[nChannel] )
            rcValue = pModel->rc_params.rcChMin[nChannel];
         return rcValue;
      }
      rcValue = pModel->rc_params.rcChMid[nChannel] + fNormalizedValue*(float)(pModel->rc_params.rcChMax[nChannel] - pModel->rc_params.rcChMid[nChannel]);
   }
   else
   {
      fNormalizedValue = (0.5-fNormalizedValue)*2.0;
      fNormalizedValue = powf(fNormalizedValue, 1.0+0.1*(float)pModel->rc_params.rcChExpo[nChannel]) * 0.7 + 0.3 * fNormalizedValue;

      if ( isRelativeMove )
      {
         if ( prevRCValue < 10 )
            prevRCValue = pModel->rc_params.rcChMid[nChannel];
         if ( fNormalizedValue > 0.0001 )
            rcValue = prevRCValue - fNormalizedValue * ((float)nCamSpeed) * ((float)miliSec/20.0);
         else
            rcValue = prevRCValue;
         if ( rcValue > pModel->rc_params.rcChMax[nChannel] )
            rcValue = pModel->rc_params.rcChMax[nChannel];
         if ( rcValue < pModel->rc_params.rcChMin[nChannel] )
            rcValue = pModel->rc_params.rcChMin[nChannel];
         return rcValue;
      }

      rcValue = pModel->rc_params.rcChMid[nChannel] - fNormalizedValue*(float)(pModel->rc_params.rcChMid[nChannel] - pModel->rc_params.rcChMin[nChannel]);
   }
   return rcValue;
}

float compute_output_rc_value(Model* pModel, int nChannel, float prevRCValue, float fNormalizedValue, u32 miliSec)
{
   return _compute_controller_rc_ranged_value(pModel, nChannel, prevRCValue, fNormalizedValue, miliSec);
}

float _compute_controller_rc_value_axe(Model* pModel, int nChannel, float prevRCValue, hw_joystick_info_t* pJoystick, t_ControllerInputInterface* pCtrlInterface, u32 miliSec)
{
   // Check to match description from models.h and config_rc.h bit flags
   // first byte:
          // bit 0: no assignment (0/1);
          // bit 1: 0 - axe, 1 - button;
          // bit 2: 0 - momentary, 1 - sticky button (togle);
          // bit 3: not used
          // bit 4..7 axe/button index (1 index based: first button/axe is 1)
   // each additional 4 bits: positive value (!=0) for each extra button (in increasing order of output RC value)
   // bit 31: toggle flag
   
   // 0...1
   float fNormalizedValue = 0.5;

   u32 dwAssignment = pModel->rc_params.rcChAssignment[nChannel];
   int nAxe = ((dwAssignment & 0xFF) >> 4) - 1;
   if ( nAxe >= pJoystick->countAxes )
      return _compute_controller_rc_ranged_value(pModel, nChannel, prevRCValue, fNormalizedValue, miliSec);

   if ( fabs(pCtrlInterface->axesMaxValue[nAxe] - pCtrlInterface->axesMinValue[nAxe]) < 0.01 )
      return _compute_controller_rc_ranged_value(pModel, nChannel, prevRCValue, fNormalizedValue, miliSec);

   int rawValue = pJoystick->axesValues[nAxe];

   float fCenterZonePercent = (float)pCtrlInterface->axesCenterZone[nAxe]/10.0/100.0;
   float rawCenterSize = fCenterZonePercent*fabs(pCtrlInterface->axesMaxValue[nAxe] - pCtrlInterface->axesMinValue[nAxe]);

   float fMidPercent = fabs(pCtrlInterface->axesCenterValue[nAxe]-pCtrlInterface->axesMinValue[nAxe]) / fabs(pCtrlInterface->axesMaxValue[nAxe] - pCtrlInterface->axesMinValue[nAxe]);

   // Throttle with reverse/fwd?

   if ( pModel->vehicle_type == MODEL_TYPE_CAR || pModel->vehicle_type == MODEL_TYPE_BOAT || pModel->vehicle_type == MODEL_TYPE_ROBOT )
   if ( nChannel == 2 && NULL != pModel && (pModel->rc_params.rcChAssignmentThrotleReverse & RC_CH_ASSIGNMENT_FLAG_ASSIGNED) )
   {
      fNormalizedValue = (float)(rawValue - pCtrlInterface->axesMinValue[nAxe])/(float)(pCtrlInterface->axesMaxValue[nAxe] - pCtrlInterface->axesMinValue[nAxe]);
      if ( fNormalizedValue < 0.0 ) fNormalizedValue = 0.0;
      if ( fNormalizedValue > 1.0 ) fNormalizedValue = 1.0;
   
      if ( pModel->rc_params.rcChFlags[nChannel] & RC_CH_FLAGS_INVERTED )
         fNormalizedValue = 1.0 - fNormalizedValue;

      bool bIsInReverse = false;

      u32 dwAssignment = pModel->rc_params.rcChAssignmentThrotleReverse;

      if ( pModel->rc_params.rcChAssignmentThrotleReverse & RC_CH_ASSIGNMENT_FLAG_BUTTON )
      {
         int buttonsIndexes[12];
         int countButtons = 0;

         // Get all assigned buttons to reverse/fwd

         for( int shift = 4; shift <= 24; shift += 4 )
         {
            if ( ((dwAssignment >> shift) & 0x0F ) != 0x00 )
            {
               buttonsIndexes[countButtons] = ((dwAssignment >> shift) & 0x0F) - 1;
               countButtons++;
            }
         }

         if ( 1 == countButtons && buttonsIndexes[0] < pJoystick->countButtons )
         {
            rawValue = pJoystick->buttonsValues[buttonsIndexes[0]];

            if ( rawValue != 0 )
               bIsInReverse = true;
         }
      }
      else
      {
         int nAxe = ((dwAssignment & 0xFF) >> 4) - 1;
         if ( nAxe < pJoystick->countAxes )
         {
            int rawValue = pJoystick->axesValues[nAxe];
            fNormalizedValue = (float)(rawValue - pCtrlInterface->axesMinValue[nAxe])/(float)(pCtrlInterface->axesMaxValue[nAxe] - pCtrlInterface->axesMinValue[nAxe]);
            if ( fNormalizedValue < 0.0 ) fNormalizedValue = 0.0;
            if ( fNormalizedValue > 1.0 ) fNormalizedValue = 1.0;
            if ( fNormalizedValue > 0.5 )
               bIsInReverse = true;
         }
      }
      float rcValue = pModel->rc_params.rcChMid[nChannel];
      if ( bIsInReverse )
         rcValue = pModel->rc_params.rcChMid[nChannel] - fNormalizedValue*(float)(pModel->rc_params.rcChMid[nChannel] - pModel->rc_params.rcChMin[nChannel]);
      else
         rcValue = pModel->rc_params.rcChMid[nChannel] + fNormalizedValue*(float)(pModel->rc_params.rcChMax[nChannel] - pModel->rc_params.rcChMid[nChannel]);
      return rcValue;
   }

   // Linear axe with no center? (No real middle center)

   if ( fMidPercent < 0.45 || fMidPercent > 0.55 )
   {
      fNormalizedValue = (float)(rawValue - pCtrlInterface->axesMinValue[nAxe])/(float)(pCtrlInterface->axesMaxValue[nAxe] - pCtrlInterface->axesMinValue[nAxe]);
      return _compute_controller_rc_ranged_value(pModel, nChannel, prevRCValue, fNormalizedValue, miliSec);
   }


   // Upper half

   if ( rawValue > pCtrlInterface->axesCenterValue[nAxe] + rawCenterSize )
   {
      if ( rawValue > pCtrlInterface->axesMaxValue[nAxe] )
         rawValue = pCtrlInterface->axesMaxValue[nAxe];

      fNormalizedValue = 0.5 + 0.5 * (float)(rawValue - pCtrlInterface->axesCenterValue[nAxe] - rawCenterSize)/(float)(pCtrlInterface->axesMaxValue[nAxe]-pCtrlInterface->axesCenterValue[nAxe]-rawCenterSize);
      return _compute_controller_rc_ranged_value(pModel, nChannel, prevRCValue, fNormalizedValue, miliSec);
   }

   // Lower half

   if ( rawValue < pCtrlInterface->axesCenterValue[nAxe] - rawCenterSize )
   {
      if ( rawValue < pCtrlInterface->axesMinValue[nAxe] )
         rawValue = pCtrlInterface->axesMinValue[nAxe];

      fNormalizedValue = 0.5 - 0.5*(float)(pCtrlInterface->axesCenterValue[nAxe] - rawValue - rawCenterSize)/(float)(pCtrlInterface->axesCenterValue[nAxe]-pCtrlInterface->axesMinValue[nAxe] - rawCenterSize);
      return _compute_controller_rc_ranged_value(pModel, nChannel, prevRCValue, fNormalizedValue, miliSec);
   }

   return _compute_controller_rc_ranged_value(pModel, nChannel, prevRCValue, fNormalizedValue, miliSec);
}

float compute_controller_rc_value(Model* pModel, int nChannel, float prevRCValue, t_shared_mem_i2c_controller_rc_in* pRCIn, hw_joystick_info_t* pJoystick, t_ControllerInputInterface* pCtrlInterface, u32 miliSec)
{
   if ( NULL == pModel )
      return 0;
   if ( nChannel < 0 || nChannel >= (int) pModel->rc_params.channelsCount )
      return 0;

   if ( pModel->rc_params.inputType == RC_INPUT_TYPE_RC_IN_SBUS_IBUS )
   {
      if ( NULL == pRCIn )
         return get_rc_channel_failsafe_value(pModel, nChannel, prevRCValue);
      if ( ! (pRCIn->uFlags & RC_IN_FLAG_HAS_INPUT) )
         return get_rc_channel_failsafe_value(pModel, nChannel, prevRCValue);

      if ( nChannel >= (int) pRCIn->uChannelsCount )
         return 0;
      return (float)(pRCIn->uChannels[nChannel]);               
   }

   if ( pModel->rc_params.inputType != RC_INPUT_TYPE_USB )
      return 0;

   if ( NULL == pJoystick || NULL == pCtrlInterface )
      return get_rc_channel_failsafe_value(pModel, nChannel, prevRCValue);

   if ( (pModel->rc_params.rcChAssignment[nChannel] & RC_CH_ASSIGNMENT_FLAG_ASSIGNED) == 0 )
   {
       if ( pModel->rc_params.rcChFlags[nChannel] & RC_CH_FLAGS_INVERTED )
          return pModel->rc_params.rcChMax[nChannel];
       return pModel->rc_params.rcChMin[nChannel];
   }

   if ( (pModel->rc_params.rcChAssignment[nChannel] & RC_CH_ASSIGNMENT_FLAG_BUTTON) == 0 )
      return _compute_controller_rc_value_axe(pModel, nChannel, prevRCValue, pJoystick, pCtrlInterface, miliSec);
   
   return _compute_controller_rc_value_button(pModel, nChannel, prevRCValue, pJoystick, pCtrlInterface);
}

u32 get_rc_channel_failsafe_type(Model* pModel, int nChannel)
{
   if ( NULL == pModel )
      return RC_FAILSAFE_NOOUTPUT;
   if ( nChannel < 0 || nChannel >= MAX_RC_CHANNELS )
      return RC_FAILSAFE_NOOUTPUT;

   if ( pModel->rc_params.failsafeFlags == RC_FAILSAFE_NOOUTPUT )
      return RC_FAILSAFE_NOOUTPUT;
   if ( pModel->rc_params.failsafeFlags == RC_FAILSAFE_KEEPLAST )
      return RC_FAILSAFE_KEEPLAST;
   if ( pModel->rc_params.failsafeFlags == RC_FAILSAFE_BELOWRANGE )
      return RC_FAILSAFE_BELOWRANGE;
   if ( pModel->rc_params.failsafeFlags == RC_FAILSAFE_VALUE )
      return RC_FAILSAFE_VALUE;

   u32 fsType = (pModel->rc_params.rcChFlags[nChannel]>>1) & 0x07;
   return fsType;
}

int get_rc_channel_failsafe_value(Model* pModel, int nChannel, int prevRCValue)
{
   if ( NULL == pModel )
      return 0;
   if ( nChannel < 0 || nChannel >= MAX_RC_CHANNELS )
      return 0;

   if ( pModel->rc_params.failsafeFlags == RC_FAILSAFE_NOOUTPUT )
      return 0;
   if ( pModel->rc_params.failsafeFlags == RC_FAILSAFE_KEEPLAST )
      return prevRCValue;
   if ( pModel->rc_params.failsafeFlags == RC_FAILSAFE_BELOWRANGE )
      return pModel->rc_params.rcChMin[nChannel]-50;
   if ( pModel->rc_params.failsafeFlags == RC_FAILSAFE_VALUE )
      return (pModel->rc_params.failsafeFlags>>8) & 0xFFFF;

   int fsType = (pModel->rc_params.rcChFlags[nChannel]>>1) & 0x07;

   if ( fsType == RC_FAILSAFE_NOOUTPUT )
      return 0;
   if ( fsType == RC_FAILSAFE_KEEPLAST )
      return prevRCValue;
   if ( fsType == RC_FAILSAFE_BELOWRANGE )
      return pModel->rc_params.rcChMin[nChannel]-50;
   if ( fsType == RC_FAILSAFE_VALUE )
      return pModel->rc_params.rcChFailSafe[nChannel];
   return 0;
}


u32 utils_get_max_allowed_video_bitrate_for_profile(Model* pModel, int iProfile)
{
   if ( NULL == pModel )
      return DEFAULT_VIDEO_BITRATE;

   if ( iProfile < 0 || iProfile >= MAX_VIDEO_LINK_PROFILES )
      return DEFAULT_VIDEO_BITRATE;

   int iMinRadioDataRate = 0; 
   u32 uMinRadioDataRateBPS = 0;

   // First get the minimum radio datarate set on radio links that can transport video streams

   for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
   {
      if ( pModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (pModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
         continue;
      if ( ! (pModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
      {
         log_line("Missing video flag from link %d", i+1);
         continue;
      }
      if ( getRealDataRateFromRadioDataRate(pModel->radioLinksParams.link_datarate_video_bps[i]) < 2000000)
         continue;

      if ( 0 == uMinRadioDataRateBPS )
      {
         uMinRadioDataRateBPS = getRealDataRateFromRadioDataRate(pModel->radioLinksParams.link_datarate_video_bps[i]);
         iMinRadioDataRate = pModel->radioLinksParams.link_datarate_video_bps[i];
      }
      else if ( getRealDataRateFromRadioDataRate(pModel->radioLinksParams.link_datarate_video_bps[i]) < uMinRadioDataRateBPS )
      {
         uMinRadioDataRateBPS = getRealDataRateFromRadioDataRate(pModel->radioLinksParams.link_datarate_video_bps[i]);
         iMinRadioDataRate = pModel->radioLinksParams.link_datarate_video_bps[i];
      }
   }

   // If the video profile has a set radio datarate, use it

   if ( 0 != pModel->video_link_profiles[iProfile].radio_datarate_video_bps )
   {
      uMinRadioDataRateBPS = getRealDataRateFromRadioDataRate(pModel->video_link_profiles[iProfile].radio_datarate_video_bps);
      iMinRadioDataRate = pModel->video_link_profiles[iProfile].radio_datarate_video_bps;
   }

   //For MQ, LQ profiles, use a lower radio datarate if one was not set for them
   else if ( iProfile == VIDEO_PROFILE_MQ )
   {
      iMinRadioDataRate = utils_get_video_profile_mq_radio_datarate(pModel);
      uMinRadioDataRateBPS = getRealDataRateFromRadioDataRate(iMinRadioDataRate);
   }
   else if ( iProfile == VIDEO_PROFILE_LQ )
   {
      iMinRadioDataRate = utils_get_video_profile_lq_radio_datarate(pModel);
      uMinRadioDataRateBPS = getRealDataRateFromRadioDataRate(iMinRadioDataRate);
   } 

   // If the user did set a radio datarate for the video link, and is lower than current profile data rate, use it

   if ( 0 != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps )
   if ( getRealDataRateFromRadioDataRate(pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps) < uMinRadioDataRateBPS )
   {
      uMinRadioDataRateBPS = getRealDataRateFromRadioDataRate(pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps);
      iMinRadioDataRate = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps;
   }

   if ( 0 == uMinRadioDataRateBPS )
      uMinRadioDataRateBPS = DEFAULT_VIDEO_BITRATE;
   
   // Compute now max actual video bitrate that is set for profile and that does not go above the radio maxim

   u32 uMaxVideoBitrateBPSForRadioRate = (uMinRadioDataRateBPS * DEFAULT_VIDEO_LINK_MAX_LOAD_PERCENT) / 100;
   
   uMaxVideoBitrateBPSForRadioRate = (uMaxVideoBitrateBPSForRadioRate * pModel->video_link_profiles[iProfile].block_packets) / (pModel->video_link_profiles[iProfile].block_packets + pModel->video_link_profiles[iProfile].block_fecs);

   return uMaxVideoBitrateBPSForRadioRate;
}

u32 utils_get_max_allowed_video_bitrate_for_profile_or_user_video_bitrate(Model* pModel, int iProfile)
{
   u32 uMaxVideoBitrateBPSForRadioRate = utils_get_max_allowed_video_bitrate_for_profile(pModel, iProfile);

   if ( 0 != pModel->video_link_profiles[iProfile].bitrate_fixed_bps )
   if ( pModel->video_link_profiles[iProfile].bitrate_fixed_bps < uMaxVideoBitrateBPSForRadioRate )
      uMaxVideoBitrateBPSForRadioRate = pModel->video_link_profiles[iProfile].bitrate_fixed_bps;
   return uMaxVideoBitrateBPSForRadioRate;
}

u32 utils_get_max_allowed_video_bitrate_for_profile_and_level(Model* pModel, int iProfile, int iLevel)
{
   if ( NULL == pModel )
      return DEFAULT_VIDEO_BITRATE;

   if ( iProfile < 0 || iProfile >= MAX_VIDEO_LINK_PROFILES )
      return DEFAULT_VIDEO_BITRATE;

   u32 uMaxVideoBitrateForProfile = utils_get_max_allowed_video_bitrate_for_profile_or_user_video_bitrate(pModel, iProfile);

   int iMaxLevels = pModel->get_video_profile_total_levels(iProfile);
   if ( iMaxLevels <= 1 )
      return uMaxVideoBitrateForProfile;
   if ( iLevel >= iMaxLevels )
      iLevel = iMaxLevels-1;

   u32 uTotalBitrateUsedForProfile = uMaxVideoBitrateForProfile * (pModel->video_link_profiles[iProfile].block_packets + pModel->video_link_profiles[iProfile].block_fecs) / pModel->video_link_profiles[iProfile].block_packets;
   
   u32 uBottomVideoBitrate = uTotalBitrateUsedForProfile / 2; // fec is equal to data packets on the lowest level
   
   if ( iProfile != VIDEO_PROFILE_USER )
   if ( iProfile != VIDEO_PROFILE_BEST_PERF )
   if ( iProfile != VIDEO_PROFILE_HIGH_QUALITY )
   if ( uBottomVideoBitrate < pModel->video_params.lowestAllowedAdaptiveVideoBitrate )
      uBottomVideoBitrate = pModel->video_params.lowestAllowedAdaptiveVideoBitrate;

   // Minimum for MQ profile, for 12 Mb datarate is at least 2Mb, do not go below that
   if ( iProfile == VIDEO_PROFILE_MQ )
   if ( pModel->video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_video_bps == 0 )
   if ( getRealDataRateFromRadioDataRate(pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps) >= 12000000 )
   if ( uBottomVideoBitrate < 2000000 )
      uBottomVideoBitrate = 2000000;

   if ( uBottomVideoBitrate < 250000 )
      uBottomVideoBitrate = 250000;

   u32 uVideoBitrateChangePerLevel = (uMaxVideoBitrateForProfile - uBottomVideoBitrate) / (iMaxLevels-1);

   u32 uFinalVideoBitrate  = uMaxVideoBitrateForProfile - uVideoBitrateChangePerLevel * iLevel;
   
   return uFinalVideoBitrate;
}

int utils_get_video_profile_mq_radio_datarate(Model* pModel)
{
   if ( NULL == pModel )
      return 9000000;

   int iMaxRadioDataRate = 0; 
   u32 uMaxRadioDataRateBPS = 0;

   // First get the maximum radio datarate set on radio links

   for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
   {
      if ( ! (pModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
      if ( getRealDataRateFromRadioDataRate(pModel->radioLinksParams.link_datarate_video_bps[i]) < 2000000)
         continue;
      if ( 0 == uMaxRadioDataRateBPS )
         uMaxRadioDataRateBPS = getRealDataRateFromRadioDataRate(pModel->radioLinksParams.link_datarate_video_bps[i]);

      if ( 0 == iMaxRadioDataRate )
         iMaxRadioDataRate = pModel->radioLinksParams.link_datarate_video_bps[i];
   }

   // If the user selected video profile has a set radio datarate, use it

   if ( 0 != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps )
   {
      uMaxRadioDataRateBPS = getRealDataRateFromRadioDataRate(pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps);
      iMaxRadioDataRate = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps;
   }

   int iDataRate = pModel->video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_video_bps;
   if ( iDataRate != 0 )
   {
      if ( getRealDataRateFromRadioDataRate(iDataRate) > getRealDataRateFromRadioDataRate(iMaxRadioDataRate) )
         iDataRate = iMaxRadioDataRate;
      return iDataRate;
   }

   if ( iMaxRadioDataRate > 0 )
   {
      if ( getRealDataRateFromRadioDataRate(iMaxRadioDataRate) > 12000000 )
         return 12000000;
      if ( getRealDataRateFromRadioDataRate(iMaxRadioDataRate) > 9000000 )
         return 9000000;
      return 6000000;
   }
   iDataRate = iMaxRadioDataRate;
   if ( iDataRate < -1 )
      iDataRate++;
   return iDataRate;
}

int utils_get_video_profile_lq_radio_datarate(Model* pModel)
{
   if ( NULL == pModel )
      return 6000000;

   int iMaxRadioDataRate = 0; 
   u32 uMaxRadioDataRateBPS = 0;

   // First get the maximum radio datarate set on radio links

   for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
   {
      if ( ! (pModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
      if ( getRealDataRateFromRadioDataRate(pModel->radioLinksParams.link_datarate_video_bps[i]) < 2000000)
         continue;
      if ( 0 == uMaxRadioDataRateBPS )
         uMaxRadioDataRateBPS = getRealDataRateFromRadioDataRate(pModel->radioLinksParams.link_datarate_video_bps[i]);

      if ( 0 == iMaxRadioDataRate )
         iMaxRadioDataRate = pModel->radioLinksParams.link_datarate_video_bps[i];
   }

   // If the user selected video profile has a set radio datarate, use it

   if ( 0 != pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps )
   {
      uMaxRadioDataRateBPS = getRealDataRateFromRadioDataRate(pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps);
      iMaxRadioDataRate = pModel->video_link_profiles[pModel->video_params.user_selected_video_link_profile].radio_datarate_video_bps;
   }

   int iDataRate = pModel->video_link_profiles[VIDEO_PROFILE_LQ].radio_datarate_video_bps;
   if ( iDataRate != 0 )
   {
      if ( getRealDataRateFromRadioDataRate(iDataRate) > getRealDataRateFromRadioDataRate(iMaxRadioDataRate) )
         iDataRate = iMaxRadioDataRate;
      return iDataRate;
   }

   if ( iMaxRadioDataRate > 0 )
      return 6000000;
   return -1;
}
