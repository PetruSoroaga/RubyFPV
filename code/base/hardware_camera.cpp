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
#include "hardware_camera.h"
#include "hw_procs.h"

void hardware_camera_apply_all_majestic_camera_settings(camera_profile_parameters_t* pCameraParams)
{
   if ( NULL == pCameraParams )
   {
      log_softerror_and_alarm("Received invalid params to set majestic camera settings.");
      return;
   }
   char szComm[128];

   sprintf(szComm, "cli -s .image.luminance %d", pCameraParams->brightness);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "cli -s .image.contrast %d", pCameraParams->contrast);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "cli -s .image.saturation %d", 50 + (pCameraParams->saturation-100)/2);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "cli -s .image.hue %d", pCameraParams->hue);
   hw_execute_bash_command(szComm, NULL);

   if ( pCameraParams->flip_image )
      hw_execute_bash_command("cli -s .image.flip true", NULL);
   else
      hw_execute_bash_command("cli -s .image.flip false", NULL);

   hw_execute_bash_command("killall -1 majestic", NULL);
}

void hardware_camera_apply_all_majestic_settings(camera_profile_parameters_t* pCameraParams, type_video_link_profile* pVideoParams)
{
   if ( (NULL == pCameraParams) || (NULL == pVideoParams) )
   {
      log_softerror_and_alarm("Received invalid params to set majestic settings.");
      return;
   }
   char szComm[128];

   hw_execute_bash_command("cli -s .system.logLevel none", NULL);
   hw_execute_bash_command("cli -s .video1.enabled false", NULL);
   hw_execute_bash_command("cli -s .video0.enabled true", NULL);
   hw_execute_bash_command("cli -s .video0.codec h264", NULL);
   hw_execute_bash_command("cli -s .video0.rcMode cbr", NULL);

   sprintf(szComm, "cli -s .video0.fps %d", pVideoParams->fps);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "cli -s .video0.bitrate %d", pVideoParams->bitrate_fixed_bps/1000);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "cli -s .video0.size %dx%d", pVideoParams->width, pVideoParams->height);
   hw_execute_bash_command(szComm, NULL);

   float fGOP = 0.5;
   if ( pVideoParams->keyframe_ms > 0 )
      fGOP = (float)pVideoParams->keyframe_ms / 1000.0;
   else
      fGOP = -(float)pVideoParams->keyframe_ms / 1000.0;
   
   sprintf(szComm, "cli -s .video0.gopSize %.1f",fGOP);
   hw_execute_bash_command(szComm, NULL);

   hardware_camera_apply_all_majestic_camera_settings(pCameraParams);
}