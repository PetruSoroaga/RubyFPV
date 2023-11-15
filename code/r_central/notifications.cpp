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

#include "notifications.h"
#include "popup.h"
#include "../base/base.h"
#include "../base/models.h"
#include "../common/string_utils.h"

#include "colors.h"
#include "osd_common.h"
#include "warnings.h"
#include "shared_vars.h"

static bool s_bIsRCFailsafeNotifTriggered = false;

static Popup* s_pPopupNotificationPairing = NULL;

void notification_add_armed()
{
   Popup* p = new Popup(true, "ARMED", 3);
   p->setFont(g_idFontOSD);
   popups_add(p);
}

void notification_add_disarmed()
{
   Popup* p = new Popup(true, "DISARMED", 3);
   p->setFont(g_idFontOSD);
   popups_add(p);
}

void notification_add_flight_mode(u32 flightMode)
{
   char szBuff[128];
   snprintf(szBuff, sizeof(szBuff), "Flight mode changed to: %s", model_getShortFlightMode(flightMode));
   popups_add(new Popup(true, szBuff, 3));
}

void notification_add_rc_failsafe()
{
   if ( s_bIsRCFailsafeNotifTriggered )
      return;
   s_bIsRCFailsafeNotifTriggered = true;
   warnings_add("RC FAILSAFE triggered.", g_idIconJoystick, get_Color_IconError(), 6);
}

void notification_add_rc_failsafe_cleared()
{
   if ( ! s_bIsRCFailsafeNotifTriggered )
      return;
   s_bIsRCFailsafeNotifTriggered = false;
   warnings_add("RC FAILSAFE cleared.", g_idIconJoystick, get_Color_IconSucces(), 6);
}

void notification_add_model_deleted()
{
   popups_add(new Popup(true, "Model deleted!", 3));
}

void notification_add_recording_start()
{
   Popup* p = new Popup(true, "Video Recording Started", 3);
   p->setIconId(g_idIconCamera, get_Color_IconNormal());
   popups_add_topmost(p);
}

void notification_add_recording_end()
{
   Popup* p = new Popup(true, "Video Recording Stopped. Processing video file...", 4);
   p->setIconId(g_idIconCamera, get_Color_IconNormal());
   popups_add_topmost(p);
}

void notification_add_rc_output_enabled()
{
   warnings_add("RC Output to FC is Enabled!", g_idIconJoystick, get_Color_IconSucces(), 4);
}

void notification_add_rc_output_disabled()
{
   warnings_add("RC Output to FC is Disabled!", g_idIconJoystick, get_Color_IconWarning(), 4);
}

void notification_add_frequency_changed(int nLinkId, u32 uFreq)
{
   char szBuff[64];
   szBuff[0] = 0;
   if ( NULL != g_pCurrentModel && 1 == g_pCurrentModel->radioLinksParams.links_count )
      snprintf(szBuff, sizeof(szBuff),"Radio link switched to %s", str_format_frequency(uFreq));
   else
      snprintf(szBuff, sizeof(szBuff),"Radio link %d switched to %s", nLinkId+1, str_format_frequency(uFreq));

   if ( 0 != szBuff[0] )
      warnings_add(szBuff, g_idIconInfo, get_Color_IconSucces(), 4);
}

void notification_add_start_pairing()
{
   if ( NULL != s_pPopupNotificationPairing )
      notification_remove_start_pairing();

   s_pPopupNotificationPairing = new Popup(true, "Configuring radio interfaces and activating model...", 10);
   popups_add_topmost(s_pPopupNotificationPairing);
}

void notification_remove_start_pairing()
{
  if ( NULL != s_pPopupNotificationPairing )
  {
     popups_remove(s_pPopupNotificationPairing);
     s_pPopupNotificationPairing = NULL;
  }
}
