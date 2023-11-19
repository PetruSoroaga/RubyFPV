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

   g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iOldVideoProfile] = 0;
   g_SM_VideoLinkStats.overwrites.profilesTopVideoBitrateOverwritesDownward[iNewVideoProfile] = 0;
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