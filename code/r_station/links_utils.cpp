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
#include "../base/hardware.h"
#include "../base/hardware_radio_sik.h"
#include "../base/ctrl_interfaces.h"
#include "../base/radio_utils.h"
#include "../base/ruby_ipc.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "../radio/radiolink.h"
#include "../radio/radio_rx.h"

#include "links_utils.h"
#include "shared_vars.h"
#include "timers.h"

u32 s_uAlarmIndexToCentral = 0;

void send_alarm_to_central(u32 uAlarm, u32 uFlags1, u32 uFlags2)
{
   s_uAlarmIndexToCentral++;
  
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_RUBY_ALARM, STREAM_ID_DATA);
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + 4*sizeof(u32);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &s_uAlarmIndexToCentral, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &uAlarm, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+2*sizeof(u32), &uFlags1, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+3*sizeof(u32), &uFlags2, sizeof(u32));

   char szAlarm[256];
   alarms_to_string(uAlarm, uFlags1, uFlags2, szAlarm);

   if ( ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length) )
      log_line("Sent local alarm to central: [%s], alarm index: %u;", szAlarm, s_uAlarmIndexToCentral);
   else
      log_softerror_and_alarm("Can't send alarm to central, no pipe. Alarm: [%s], alarm index: %u", szAlarm, s_uAlarmIndexToCentral);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;   
}
