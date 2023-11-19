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

#include <string.h>
#include "alarms.h"

void alarms_to_string(u32 uAlarms, u32 uFlags1, u32 uFlags2, char* szOutput)
{
   if ( 0 == szOutput )
      return;
   szOutput[0] = 0;

   if ( uAlarms & ALARM_ID_GENERIC_STATUS_UPDATE )
      strcat(szOutput, " GENERIC_STATUS_UPDATE");

   if ( uAlarms & ALARM_ID_UNSUPORTED_USB_SERIAL )
      strcat(szOutput, " UNSUPORTED_USB_SERIAL");

   if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET )
      strcat(szOutput, " RECEIVED_INVALID_RADIO_PACKET");

   if ( uAlarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED )
      strcat(szOutput, " RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED");

   if ( uAlarms & ALARM_ID_LINK_TO_CONTROLLER_LOST )
      strcat(szOutput, " LINK_TO_CONTROLLER_LOST");

   if ( uAlarms & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
      strcat(szOutput, " LINK_TO_CONTROLLER_RECOVERED");

   if ( uAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD )
      strcat(szOutput, " VEHICLE_VIDEO_DATA_OVERLOAD");
   if ( uAlarms & ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD )
      strcat(szOutput, " VEHICLE_VIDEO_TX_OVERLOAD");

   if ( uAlarms & ALARM_ID_RADIO_LINK_DATA_OVERLOAD )
      strcat(szOutput, " RADIO_LINK_DATA_OVERLOAD");

   if ( uAlarms & ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD )
      strcat(szOutput, " VEHICLE_CPU_LOOP_OVERLOAD");
   if ( uAlarms & ALARM_ID_VEHICLE_RX_TIMEOUT )
      strcat(szOutput, " VEHICLE_RX_TIMEOUT");
   if ( uAlarms & ALARM_ID_VEHICLE_LOW_STORAGE_SPACE )
      strcat(szOutput, " VEHICLE_LOW_STORAGE_SPACE");
   if ( uAlarms & ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR )
      strcat(szOutput, " VEHICLE_STORAGE_WRITE_ERRROR");
   if ( uAlarms & ALARM_ID_VEHICLE_VIDEO_TX_BITRATE_TOO_LOW )
      strcat(szOutput, " VEHICLE_VIDEO_TX_BITRATE_TOO_LOW");
   if ( uAlarms & ALARM_ID_VEHICLE_VIDEO_CAPTURE_RESTARTED )
      strcat(szOutput, " VEHICLE_VIDEO_CAPTURE_RESTARTED");

   if ( uAlarms & ALARM_ID_RADIO_INTERFACE_DOWN )
      strcat(szOutput, " RADIO_INTERFACE_DOWN");
   if ( uAlarms & ALARM_ID_RADIO_INTERFACE_REINITIALIZED )
      strcat(szOutput, " RADIO_INTERFACE_REINITIALIZED");

   if ( uAlarms & ALARM_ID_CONTROLLER_RECEIVED_INVALID_RADIO_PACKET )
      strcat(szOutput, " CONTROLLER_RECEIVED_INVALID_RADIO_PACKET");

   if ( uAlarms & ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK )
      strcat(szOutput, " NO_INTERFACES_FOR_RADIO_LINK");

   if ( uAlarms & ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD )
      strcat(szOutput, " CONTROLLER_CPU_LOOP_OVERLOAD");
   if ( uAlarms & ALARM_ID_CONTROLLER_RX_TIMEOUT )
      strcat(szOutput, " CONTROLLER_RX_TIMEOUT");
   if ( uAlarms & ALARM_ID_CONTROLLER_IO_ERROR )
      strcat(szOutput, " CONTROLLER_IO_ERROR");
   if ( uAlarms & ALARM_ID_CONTROLLER_LOW_STORAGE_SPACE )
      strcat(szOutput, " ALARM_ID_CONTROLLER_LOW_STORAGE_SPACE");
   if ( uAlarms & ALARM_ID_CONTROLLER_STORAGE_WRITE_ERRROR )
      strcat(szOutput, " CONTROLLER_STORAGE_WRITE_ERRROR");
   if ( uAlarms & ALARM_ID_CONTROLLER_PAIRING_COMPLETED )
      strcat(szOutput, " CONTROLLER_PAIRING_COMPLETED");

   if ( uAlarms & ALARM_ID_FIRMWARE_OLD )
      strcat(szOutput, " FIRMWARE_OLD");
   if ( uAlarms & ALARM_ID_CPU_RX_LOOP_OVERLOAD )
      strcat(szOutput, " ALARM_ID_CPU_RX_LOOP_OVERLOAD");
   if ( uAlarms & ALARM_ID_GENERIC )
      strcat(szOutput, " ALARM_ID_GENERIC");

   if ( 0 == szOutput[0] )
   {
      sprintf(szOutput, "%u", uAlarms);
   }
   char szBuff[256];
   if ( ! (uAlarms & ALARM_ID_GENERIC_STATUS_UPDATE) )
      sprintf(szBuff, ", Flags1: %u, Flags2: %u", uFlags1, uFlags2);
   else
   {
      char szFlags1[128];
      szFlags1[0] = 0;
      if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE ) strcpy(szFlags1, "ALARM_FLAG_GENERIC_STATUS_RECONFIGURING_RADIO_INTERFACE");
      else if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE ) strcpy(szFlags1, "ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE");
      else if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE_FAILED ) strcpy(szFlags1, "ALARM_FLAG_GENERIC_STATUS_RECONFIGURED_RADIO_INTERFACE_FAILED");

      else if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_REASIGNING_RADIO_LINKS_START ) strcpy(szFlags1, "ALARM_FLAG_GENERIC_STATUS_REASIGNING_RADIO_LINKS_START");
      else if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_REASIGNING_RADIO_LINKS_END ) strcpy(szFlags1, "ALARM_FLAG_GENERIC_STATUS_REASIGNING_RADIO_LINKS_END");
      else if ( uFlags1 == ALARM_FLAG_GENERIC_STATUS_SENT_PAIRING_REQUEST ) strcpy(szFlags1, "ALARM_FLAG_GENERIC_STATUS_SENT_PAIRING_REQUEST");
      else
         sprintf(szFlags1, "Unknown: %u", uFlags1);
      sprintf(szBuff, ", Flags1: %s, Flags2: %u", szFlags1, uFlags2);
   }
   strcat(szOutput, szBuff);
}