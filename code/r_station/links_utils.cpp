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
