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

void alarms_to_string(unsigned int alarms, char* szOutput)
{
   if ( 0 == szOutput )
      return;

   szOutput[0] = 0;

   if ( alarms & ALARM_ID_UNSUPORTED_USB_SERIAL )
      strcat(szOutput, " UNSUPORTED_USB_SERIAL");

   if ( alarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET )
      strcat(szOutput, " RECEIVED_INVALID_RADIO_PACKET");

   if ( alarms & ALARM_ID_RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED )
      strcat(szOutput, " RECEIVED_INVALID_RADIO_PACKET_RECONSTRUCTED");

   if ( alarms & ALARM_ID_LINK_TO_CONTROLLER_LOST )
      strcat(szOutput, " LINK_TO_CONTROLLER_LOST");

   if ( alarms & ALARM_ID_LINK_TO_CONTROLLER_RECOVERED )
      strcat(szOutput, " LINK_TO_CONTROLLER_RECOVERED");

   if ( alarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD )
      strcat(szOutput, " VEHICLE_VIDEO_DATA_OVERLOAD");
   if ( alarms & ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD )
      strcat(szOutput, " VEHICLE_VIDEO_TX_OVERLOAD");

   if ( alarms & ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD )
      strcat(szOutput, " VEHICLE_CPU_LOOP_OVERLOAD");
   if ( alarms & ALARM_ID_VEHICLE_RX_TIMEOUT )
      strcat(szOutput, " VEHICLE_RX_TIMEOUT");
   if ( alarms & ALARM_ID_VEHICLE_LOW_STORAGE_SPACE )
      strcat(szOutput, " VEHICLE_LOW_STORAGE_SPACE");
   if ( alarms & ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR )
      strcat(szOutput, " VEHICLE_STORAGE_WRITE_ERRROR");
   if ( alarms & ALARM_ID_VEHICLE_VIDEO_TX_BITRATE_TOO_LOW )
      strcat(szOutput, " VEHICLE_VIDEO_TX_BITRATE_TOO_LOW");

   if ( alarms & ALARM_ID_CONTROLLER_RECEIVED_INVALID_RADIO_PACKET )
      strcat(szOutput, " CONTROLLER_RECEIVED_INVALID_RADIO_PACKET");

   if ( alarms & ALARM_ID_CONTROLLER_NO_INTERFACES_FOR_RADIO_LINK )
      strcat(szOutput, " NO_INTERFACES_FOR_RADIO_LINK");

   if ( alarms & ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD )
      strcat(szOutput, " CONTROLLER_CPU_LOOP_OVERLOAD");
   if ( alarms & ALARM_ID_CONTROLLER_RX_TIMEOUT )
      strcat(szOutput, " CONTROLLER_RX_TIMEOUT");
   if ( alarms & ALARM_ID_CONTROLLER_IO_ERROR )
      strcat(szOutput, " CONTROLLER_IO_ERROR");
   if ( alarms & ALARM_ID_CONTROLLER_LOW_STORAGE_SPACE )
      strcat(szOutput, " ALARM_ID_CONTROLLER_LOW_STORAGE_SPACE");
   if ( alarms & ALARM_ID_CONTROLLER_STORAGE_WRITE_ERRROR )
      strcat(szOutput, " CONTROLLER_STORAGE_WRITE_ERRROR");
   if ( alarms & ALARM_ID_CONTROLLER_PAIRING_COMPLETED )
      strcat(szOutput, " CONTROLLER_PAIRING_COMPLETED");
}