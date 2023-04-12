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
#include "../base/launchers.h"
#include "../base/ruby_ipc.h"
#include "../common/radio_stats.h"
#include "../common/string_utils.h"

#include "links_utils.h"
#include "shared_vars.h"
#include "timers.h"

bool links_set_cards_frequencies_for_search( u32 uSearchFreq )
{
   log_line("Links: Set all cards frequencies for search mode to %s", str_format_frequency(uSearchFreq));
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      radio_stats_set_card_current_frequency(&g_Local_RadioStats, i, 0);

      u32 flags = controllerGetCardFlags(pRadioHWInfo->szMAC);
      if ( (flags & RADIO_HW_CAPABILITY_FLAG_DISABLED) || controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;

      if ( 0 == hardware_radio_supports_frequency(pRadioHWInfo, uSearchFreq ) )
         continue;

      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      {
         launch_set_frequency(NULL, i, uSearchFreq, g_pProcessStats); 
         g_Local_RadioStats.radio_interfaces[i].uCurrentFrequency = uSearchFreq;
         radio_stats_set_card_current_frequency(&g_Local_RadioStats, i, uSearchFreq);
      }
   }
   if ( NULL != g_pSM_RadioStats )
      memcpy((u8*)g_pSM_RadioStats, (u8*)&g_Local_RadioStats, sizeof(shared_mem_radio_stats));
   log_line("Links: Set all cards frequencies for search mode to %s. Completed.", str_format_frequency(uSearchFreq));
   return true;
}

void send_alarm_to_central(u32 uAlarm, u32 uFlags, u32 uRepeatCount)
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type =  PACKET_TYPE_RUBY_ALARM;
   PH.vehicle_id_src = 0;
   PH.vehicle_id_dest = 0;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u32);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &uAlarm, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &uFlags, sizeof(u32));
   memcpy(packet+sizeof(t_packet_header)+2*sizeof(u32), &uRepeatCount, sizeof(u32));
   packet_compute_crc(packet, sizeof(t_packet_header)+3*sizeof(u32));

   char szAlarm[256];
   alarms_to_string(uAlarm, szAlarm);

   if ( ruby_ipc_channel_send_message(g_fIPCToCentral, packet, PH.total_length) )
      log_line("Sent alarm to central: [%s]", szAlarm);
   else
      log_softerror_and_alarm("Can't send alarm to central, no pipe. Alarm: [%s]", szAlarm);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;   
}