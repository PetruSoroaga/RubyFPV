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
#include "ui_alarms.h"
#include "colors.h"
#include "osd_common.h"
#include "popup.h"
#include "shared_vars.h"
#include "timers.h"
#include "warnings.h"

u32 g_uPersistentAllAlarmsVehicle = 0;
u32 g_uPersistentAllAlarmsLocal = 0;

u32 s_TimeLastVehicleAlarm = 0;
u16 s_uLastVehicleAlarms = 0;

u32 s_TimeLastLocalAlarm = 0;
u16 s_uLastLocalAlarms = 0;

u32 s_TimeLastCPUOverloadAlarmVehicle = 0;
u32 s_TimeLastCPUOverloadAlarmController = 0;

Popup* s_pPopupAlarm1 = NULL;
Popup* s_pPopupAlarm2 = NULL;
Popup* s_pPopupAlarm3 = NULL;
Popup* s_pPopupAlarm4 = NULL;

void alarms_render()
{
}

Popup* _get_next_available_alarm_popup(const char* szTitle, int timeout)
{
   Popup* pFirstFree = s_pPopupAlarm1;
   if ( popups_has_popup(pFirstFree) )
      pFirstFree = s_pPopupAlarm2;
   if ( popups_has_popup(pFirstFree) )
      pFirstFree = s_pPopupAlarm3;
   if ( popups_has_popup(pFirstFree) )
      pFirstFree = s_pPopupAlarm4;

   Popup* pNew = new Popup(true, szTitle, timeout);
   pNew->setBottomAlign(true);
   pNew->setMaxWidth(0.7);

   if ( pFirstFree == s_pPopupAlarm1 )
   {
      s_pPopupAlarm1 = pNew;
      log_line("Add alarm popup on slot 1");
   }
   else if ( pFirstFree == s_pPopupAlarm2 )
   {
      s_pPopupAlarm2 = pNew;
      pNew->setBottomMargin(0.1);
      log_line("Add alarm popup on slot 2");
   }
   else if ( pFirstFree == s_pPopupAlarm3 )
   {
      s_pPopupAlarm3 = pNew;
      pNew->setBottomMargin(0.2);
      log_line("Add alarm popup on slot 3");
   }
   else if ( pFirstFree == s_pPopupAlarm4 )
   {
      s_pPopupAlarm3 = pNew;
      pNew->setBottomMargin(0.3);
      log_line("Add alarm popup on slot 4");
   }
   return pNew;
}

void alarms_add_from_vehicle(u32 uAlarms, u32 uFlags, u32 uAlarmsCount)
{
   g_uPersistentAllAlarmsVehicle |= uAlarms;

   Preferences* p = get_Preferences();
   if ( p->iDebugShowFullRXStats )
      return;

   char szAlarmText[256];
   char szAlarmText2[256];
   char szAlarmText3[256];

   sprintf(szAlarmText, "Received alarm(s) (id(s) %u-%d) from the vehicle", uAlarms, (int)uAlarmsCount);
   szAlarmText2[0] = 0;
   szAlarmText3[0] = 0;

   if ( ! (p->uEnabledAlarms & uAlarms) )
   {
      log_line(szAlarmText);
      log_line("This alarm is disabled. Do not show it in UI.");
      return;
   }

   if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET )
   {
      strcpy(szAlarmText, "Invalid unrecoverable data detected over the radio links! Some radio interfaces are not functioning correctly!");
      strcpy(szAlarmText2, "Try to: swap the radio interfaces, change the USB ports or replace the radio interfaces!");
   }

   if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED )
   {
      strcpy(szAlarmText, "Invalid but recovered data detected over the radio links! Some radio interfaces are not functioning correctly!");
      if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET )
         strcpy(szAlarmText, "Both invalid and recoverable data detected over the radio links! Some radio interfaces are not functioning correctly!");
      strcpy(szAlarmText2, "Try to: swap the radio interfaces, change the USB ports or replace the radio interfaces!");
   }

   if ( uAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD )
   {
      g_bHasVideoDataOverloadAlarm = true;
      g_TimeLastVideoDataOverloadAlarm = g_TimeNow;

      int kbpsSent = (int)(uFlags & 0xFFFF);
      int kbpsMax = (int)(uFlags >> 16);
      if ( NULL != g_pCurrentModel && (! g_pCurrentModel->osd_params.show_overload_alarm) )
         return;
      sprintf(szAlarmText, "Video link is overloaded (sending %.1f Mbps, over the safe %.1f Mbps limit for current radio rate).", ((float)kbpsSent)/1000.0, ((float)kbpsMax)/1000.0);
      strcpy(szAlarmText2, "Video bitrate was decreased temporarly.");
      strcpy(szAlarmText3, "Lower your video bitrate.");
   }

   if ( uAlarms & ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD )
   {
      g_bHasVideoTxOverloadAlarm = true;
      g_TimeLastVideoTxOverloadAlarm = g_TimeNow;

      int ms = (int)(uFlags & 0xFFFF);
      int msMax = (int)(uFlags >> 16);
      if ( NULL != g_pCurrentModel && (! g_pCurrentModel->osd_params.show_overload_alarm) )
         return;
      sprintf(szAlarmText, "Video link transmission is overloaded (transmission took %d ms/sec, max safe limit is %d ms/sec).", ms, msMax);
      strcpy(szAlarmText2, "Video bitrate was decreased temporarly.");
      strcpy(szAlarmText3, "Lower your video bitrate or switch frequencies.");
   }

   if ( uAlarms & ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD )
   if ( !g_bUpdateInProgress )
   {
      if ( g_TimeNow > s_TimeLastCPUOverloadAlarmVehicle + 10000 )
      {
         s_TimeLastCPUOverloadAlarmVehicle = g_TimeNow;
         u32 timeAvg = uFlags & 0xFFFF;
         u32 timeMax = uFlags >> 16;
         if ( timeMax > 0 )
         {
            sprintf(szAlarmText, "Your vehicle CPU had a spike of %u miliseconds in it's loop processing.", timeMax );
            sprintf(szAlarmText2, "If this is recurent, increase your CPU overclocking speed or reduce your data processing load (video bitrate).");
         }
         if ( timeAvg > 0 )
         {
            sprintf(szAlarmText, "Your vehicle CPU had an overload of %u miliseconds in it's loop processing.", timeAvg );
            sprintf(szAlarmText2, "Increase your CPU overclocking speed or reduce your data processing (video bitrate).");
         }
      }
      else
         return;
   }

   if ( uAlarms & ALARM_ID_VEHICLE_RX_TIMEOUT )
   {
      if ( g_bUpdateInProgress )
         return;
      sprintf(szAlarmText, "Vehicle radio interface %d timed out reading a radio packet %d times.", 1+(int)uFlags, (int)uAlarmsCount );
   }

   if ( g_TimeNow > s_TimeLastVehicleAlarm + 8000 || ((s_uLastVehicleAlarms != uAlarms) && (uAlarms != 0)) )
   {
      log_line("Adding UI for alarm from vehicle: [%s]", szAlarmText);
      s_TimeLastVehicleAlarm = g_TimeNow;
      s_uLastVehicleAlarms = uAlarms;

      char szText[512];
      sprintf(szText, "Alarm from vehicle: %s", szAlarmText);
      if ( (uAlarms & ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD) || (uAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD) )
         strcpy(szText, szAlarmText);
   
      Popup* p = _get_next_available_alarm_popup(szText, 7);
      //p->setFont(g_idFontOSD);
      p->setIconId(g_idIconAlarm, get_Color_IconNormal());
      if ( (uAlarms & ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD) || (uAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD) )
         p->setIconId(g_idIconCamera, get_Color_IconNormal());

      if ( 0 != szAlarmText2[0] )
         p->addLine(szAlarmText2);
      if ( 0 != szAlarmText3[0] )
         p->addLine(szAlarmText3);
      popups_add(p);

      if ( (uAlarms & ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD) || (uAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD) )
         g_pPopupVideoOverloadAlarm = p;
   }
}

void alarms_add_from_local(u32 uAlarms, u32 uFlags, u32 uAlarmsCount)
{
   g_uPersistentAllAlarmsLocal |= uAlarms;

   Preferences* p = get_Preferences();
   if ( p->iDebugShowFullRXStats )
      return;

   if ( (uAlarms & ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD) || (uAlarms & ALARM_ID_CONTROLLER_IO_ERROR) )
      g_nTotalControllerCPUSpikes++;

   char szAlarmText[256];
   char szAlarmText2[256];
   sprintf(szAlarmText, "Triggered alarm(s) (id(s) %u-%d) on the controller", uAlarms, (int)uAlarmsCount);
   szAlarmText2[0] = 0;

   if ( uAlarms != ALARM_ID_CONTROLLER_IO_ERROR )
   if ( ! (p->uEnabledAlarms & uAlarms) )
   {
      log_line(szAlarmText);
      log_line("This alarm is disabled. Do not show it in UI.");
      return;
   }

   if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET )
   {
      strcpy(szAlarmText, "Invalid data detected over the radio links! Some radio interfaces are not functioning correctly on the controller!");
      strcpy(szAlarmText2, "Try to: swap the radio interfaces, change the USB ports or replace the radio interfaces!");
   }

   if ( uAlarms & ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK )
   {
      strcpy(szAlarmText, "No radio interfaces on this controller can handle the vehicle radio link 2.");
   }

   
   if ( uAlarms & ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD )
   {
    if ( g_bUpdateInProgress )
       return;
    if ( g_TimeNow > s_TimeLastCPUOverloadAlarmController + 10000 )
    {
       s_TimeLastCPUOverloadAlarmController = g_TimeNow;
       u32 timeAvg = uFlags & 0xFFFF;
       u32 timeMax = uFlags >> 16;
       if ( timeMax > 0 )
       {
          sprintf(szAlarmText, "Your controller CPU had a spike of %u miliseconds in it's loop processing.", timeMax );
          sprintf(szAlarmText2, "If this is recurent, increase your CPU overclocking speed or reduce your data processing load (video bitrate).");
       }
       if ( timeAvg > 0 )
       {
          sprintf(szAlarmText, "Your controller CPU had an overload of %u miliseconds in it's loop processing.", timeAvg );
          sprintf(szAlarmText2, "Increase your CPU overclocking speed or reduce your data processing load (video bitrate).");
       }
    }
    else
       return;
   }

   if ( uAlarms & ALARM_ID_CONTROLLER_RX_TIMEOUT )
   {
      if ( g_bUpdateInProgress )
         return;
      sprintf(szAlarmText, "Controller radio interface %d timed out reading a radio packet %d times.", 1+(int)uFlags, (int)uAlarmsCount );
   }

   if ( uAlarms & ALARM_ID_CONTROLLER_IO_ERROR )
   {
      if ( g_uLastControllerAlarmIOFlags == uFlags )
         return;

      g_uLastControllerAlarmIOFlags = uFlags;

      if ( 0 == uFlags )
      {
         warnings_add("Controller IO Error Alarm cleared.");
         return;
      }
      strcpy(szAlarmText, "Controller IO Error!");
      szAlarmText2[0] = 0;
      if ( uFlags & ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT )
         strcpy(szAlarmText2, "Video USB output error.");
      if ( uFlags & ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_TRUNCATED )
         strcpy(szAlarmText2, "Video USB output truncated.");
      if ( uFlags & ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_WOULD_BLOCK )
         strcpy(szAlarmText2, "Video USB output full.");
      if ( uFlags & ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT )
         strcpy(szAlarmText2, "Video player output error.");
      if ( uFlags & ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_TRUNCATED )
         strcpy(szAlarmText2, "Video player output truncated.");
      if ( uFlags & ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_WOULD_BLOCK )
         strcpy(szAlarmText2, "Video player output full.");
   }

   if ( g_TimeNow > s_TimeLastLocalAlarm + 10000 || ((s_uLastLocalAlarms != uAlarms) && (uAlarms != 0)) )
   {
      log_line("Adding UI for alarm from controller: [%s]", szAlarmText);
      s_TimeLastLocalAlarm = g_TimeNow;
      s_uLastLocalAlarms = uAlarms;
      Popup* p = _get_next_available_alarm_popup(szAlarmText, 15);
      p->setFont(g_idFontOSD);
      p->setIconId(g_idIconAlarm, get_Color_IconNormal());
      if ( 0 != szAlarmText2[0] )
         p->addLine(szAlarmText2);
      popups_add(p);
   }
}
