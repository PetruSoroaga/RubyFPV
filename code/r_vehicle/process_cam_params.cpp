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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware_camera.h"
#include "../base/hw_procs.h"
#include <math.h>
#include "shared_vars.h"
#include "timers.h"
#include "video_source_csi.h"
#include "ruby_rt_vehicle.h"

bool _try_process_live_changes(int iNewCameraIndex, type_camera_parameters* pNewCamParams, int iCurrentCameraIndex, type_camera_parameters* pCurrentCamParams)
{
   if ( (NULL == pNewCamParams) || (NULL == pCurrentCamParams) )
      return false;

   int iCurCamProfile = pCurrentCamParams[iCurrentCameraIndex].iCurrentProfile;
   int iNewCamProfile = pNewCamParams[iNewCameraIndex].iCurrentProfile;
   camera_profile_parameters_t* pCurCamProfile = &(pCurrentCamParams[iCurrentCameraIndex].profiles[iCurCamProfile]);
   camera_profile_parameters_t* pNewCamProfile = &(pNewCamParams[iNewCameraIndex].profiles[iNewCamProfile]);

   bool bUpdated = false;
   if ( g_pCurrentModel->isRunningOnOpenIPCHardware() )
   {
      u32 uCurrentFlags = pCurCamProfile->uFlags;
      u32 uNewFlags = pNewCamProfile->uFlags;
      if ( (uCurrentFlags & CAMERA_FLAG_IR_FILTER_OFF) != (uNewFlags & CAMERA_FLAG_IR_FILTER_OFF) )
      {
         hardware_camera_maj_set_irfilter_off(uNewFlags & CAMERA_FLAG_IR_FILTER_OFF, true);
         pNewCamProfile->uFlags = uCurrentFlags;
         bUpdated = true;
      }
      if ( (uCurrentFlags & CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF) != (uNewFlags & CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF) )
      {
         hardware_camera_maj_set_daylight_off(uNewFlags & CAMERA_FLAG_OPENIPC_DAYLIGHT_OFF);
         pNewCamProfile->uFlags = uCurrentFlags;
         bUpdated = true;
      }
   }

   if ( pNewCamProfile->brightness != pCurCamProfile->brightness )
   {
      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() && (! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType)) )
      {
         hardware_camera_maj_set_brightness(pNewCamProfile->brightness);
         pNewCamProfile->brightness = pCurCamProfile->brightness;
         bUpdated = true;
      }
      else if ( g_pCurrentModel->isActiveCameraCSICompatible() )
      {
         video_source_csi_send_control_message( RASPIVID_COMMAND_ID_BRIGHTNESS, pNewCamProfile->brightness, 0 );
         pNewCamProfile->brightness = pCurCamProfile->brightness;
         bUpdated = true;       
      }
   }

   if ( pNewCamProfile->contrast != pCurCamProfile->contrast )
   {
      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() && (! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType)) )
      {
         hardware_camera_maj_set_contrast(pNewCamProfile->contrast);
         pNewCamProfile->contrast = pCurCamProfile->contrast;
         bUpdated = true;
      }
      else if ( g_pCurrentModel->isActiveCameraCSICompatible() )
      {
         video_source_csi_send_control_message( RASPIVID_COMMAND_ID_CONTRAST, pNewCamProfile->contrast, 0 );
         pNewCamProfile->contrast = pCurCamProfile->contrast;
         bUpdated = true;       
      }
   }

   if ( pNewCamProfile->saturation != pCurCamProfile->saturation )
   {
      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() && (! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType)) )
      {
         hardware_camera_maj_set_saturation(pNewCamProfile->saturation);
         pNewCamProfile->saturation = pCurCamProfile->saturation;
         bUpdated = true;
      }
      else if ( g_pCurrentModel->isActiveCameraCSICompatible() )
      {
         video_source_csi_send_control_message( RASPIVID_COMMAND_ID_SATURATION, pNewCamProfile->saturation, 0 );
         pNewCamProfile->saturation = pCurCamProfile->saturation;
         bUpdated = true;       
      }
   }

   if ( pNewCamProfile->sharpness != pCurCamProfile->sharpness )
   {
      if ( g_pCurrentModel->isActiveCameraCSICompatible() )
      {
         video_source_csi_send_control_message( RASPIVID_COMMAND_ID_SHARPNESS, pNewCamProfile->sharpness, 0 );
         pNewCamProfile->sharpness = pCurCamProfile->sharpness;
         bUpdated = true;       
      }
   }

   if ( pNewCamProfile->hue != pCurCamProfile->hue )
   {
      if ( g_pCurrentModel->isRunningOnOpenIPCHardware() && (! hardware_board_is_goke(g_pCurrentModel->hwCapabilities.uBoardType)) )
      {
         hardware_camera_maj_set_hue(pNewCamProfile->hue);
         pNewCamProfile->hue = pCurCamProfile->hue;
         bUpdated = true;
      }
   }

   return bUpdated;
}

void _apply_all_camera_params(type_camera_parameters* pNewCamParams, type_camera_parameters* pOldCamParams)
{
   if ( (NULL == pNewCamParams) || (NULL == pOldCamParams) )
      return;
   log_line("Start applying all camera params due to received changes...");

   if ( g_pCurrentModel->isActiveCameraVeye307() && (fabs(pNewCamParams->profiles[pNewCamParams->iCurrentProfile].hue - pOldCamParams->profiles[pOldCamParams->iCurrentProfile].hue) > 0.1 ) )
      vehicle_update_camera_params_csi(g_pCurrentModel, g_pCurrentModel->iCurrentCamera);
   else if ( g_pCurrentModel->isActiveCameraVeye327290() )
      vehicle_update_camera_params_csi(g_pCurrentModel, g_pCurrentModel->iCurrentCamera);
   else if ( g_pCurrentModel->isActiveCameraVeye307() )
      vehicle_update_camera_params_csi(g_pCurrentModel, g_pCurrentModel->iCurrentCamera);
   else if ( g_pCurrentModel->hasCamera() )
      video_source_capture_mark_needs_update_or_restart(MODEL_CHANGED_CAMERA_PARAMS);
   log_line("Done applying all camera params due to received changes.");
}

void process_camera_params_changed(u8* pPacketBuffer, int iPacketLength)
{
   if ( (NULL == pPacketBuffer) || (iPacketLength < (int)sizeof(t_packet_header)) )
      return;

   int iOldCameraIndex = g_pCurrentModel->iCurrentCamera;
   type_camera_parameters oldCamParams;
   memcpy((u8*)(&oldCamParams), &(g_pCurrentModel->camera_params[iOldCameraIndex]), sizeof(type_camera_parameters));
   
   u8 uCameraChangedIndex = 0;
   type_camera_parameters newCamParams;

   memcpy(&uCameraChangedIndex, pPacketBuffer + sizeof(t_packet_header), sizeof(u8));
   memcpy((u8*)&newCamParams, pPacketBuffer + sizeof(t_packet_header) + sizeof(u8), sizeof(type_camera_parameters));

   // Do not save model. Saved by rx_commands. Just update to the new values.
   memcpy(&(g_pCurrentModel->camera_params[uCameraChangedIndex]), (u8*)&newCamParams, sizeof(type_camera_parameters));
   g_pCurrentModel->iCurrentCamera = uCameraChangedIndex;


   // Do changes on the fly, if possible
   _try_process_live_changes(uCameraChangedIndex, &newCamParams, iOldCameraIndex, &oldCamParams);
   
   // Do all the other remaining changes if any are remaining
   if ( 0 != memcmp(&newCamParams, &oldCamParams, sizeof(type_camera_parameters)) )
   {
      g_pCurrentModel->log_camera_profiles_differences(&(oldCamParams.profiles[oldCamParams.iCurrentProfile]), &(newCamParams.profiles[newCamParams.iCurrentProfile]), oldCamParams.iCurrentProfile, newCamParams.iCurrentProfile);
      _apply_all_camera_params(&(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), &oldCamParams);
   }
   #if defined HW_PLATFORM_RASPBERRY
   if ( g_pCurrentModel->isActiveCameraVeye() )
       g_uTimeToSaveVeyeCameraParams = g_TimeNow + 5000;
   #endif
}


void process_camera_periodic_loop()
{
   // Save lastest camera params to flash (on camera)
   if ( 0 != g_uTimeToSaveVeyeCameraParams )
   if ( g_TimeNow > g_uTimeToSaveVeyeCameraParams )
   {
      #ifdef HW_PLATFORM_RASPBERRY
      if ( g_pCurrentModel->isActiveCameraVeye() )
      {
         log_line("Saving Veye camera parameters to flash memory.");
         int nBus = hardware_get_i2c_device_bus_number(I2C_DEVICE_ADDRESS_CAMERA_VEYE);
         char szComm[256];
         if ( g_pCurrentModel->isActiveCameraVeye307() )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./cs_mipi_i2c.sh -w -f paramsave -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER307, nBus);
         else if ( g_pCurrentModel->isActiveCameraVeye() )
            sprintf(szComm, "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -w -f paramsave -b %d; cd $current_dir", VEYE_COMMANDS_FOLDER, nBus);
         hw_execute_bash_command(szComm, NULL);
      }
      #endif
      g_uTimeToSaveVeyeCameraParams = 0;
   }
}