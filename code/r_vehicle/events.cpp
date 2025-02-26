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
#include "../base/models.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"
#include "events.h"
#include "shared_vars.h"
#include "timers.h"
#include "processor_relay.h"


void onEventBeforeRuntimeCurrentVideoProfileChanged(int iOldVideoProfile, int iNewVideoProfile)
{
   char szProfile1[64];
   char szProfile2[64];
   strcpy(szProfile1, str_get_video_profile_name(iOldVideoProfile));
   strcpy(szProfile2, str_get_video_profile_name(iNewVideoProfile));
   log_line("Video profile changed from %s to %s", szProfile1, szProfile2);

   g_uTimeLastVideoTxOverload = 0;
   if ( iOldVideoProfile != iNewVideoProfile )
      g_TimeLastVideoProfileChanged = g_TimeNow;

// To fix
   //g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iOldVideoProfile] = 0;
   //g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iNewVideoProfile] = 0;
}

void onEventRelayModeChanged(u32 uOldRelayMode, u32 uNewRelayMode, const char* szSource)
{
   char szTmp1[64];
   char szTmp2[64];
   strncpy(szTmp1, str_format_relay_mode(uOldRelayMode), 63);
   strncpy(szTmp2, str_format_relay_mode(uNewRelayMode), 63);

   if ( NULL != szSource )
      log_line("[Event] Relay mode changed on this vehicle from %s to %s (source: %s)", str_format_relay_mode(uOldRelayMode), str_format_relay_mode(uNewRelayMode), szSource);
   else
      log_line("[Event] Relay mode changed on this vehicle from %s to %s", str_format_relay_mode(uOldRelayMode), str_format_relay_mode(uNewRelayMode));

   relay_on_relay_mode_changed(uOldRelayMode, uNewRelayMode);

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_EVENT, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + 2*sizeof(u32);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   u32 uEventType = EVENT_TYPE_RELAY_MODE_CHANGED;
   u32 uEventInfo = uNewRelayMode;
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy((&buffer[0])+sizeof(t_packet_header), (u8*)&uEventType, sizeof(u32));
   memcpy((&buffer[0])+sizeof(t_packet_header) + sizeof(u32), (u8*)&uEventInfo, sizeof(u32));

   if ( ! ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, buffer, PH.total_length) )
      log_softerror_and_alarm("No pipe to telemetry to send an event message (%d)", uEventType);
}