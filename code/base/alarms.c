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

   if ( uAlarms & ALARM_ID_VIDEO_CAPTURE_MALFUNCTION )
      strcat(szOutput, " VIDEO_CAPTURE_MALFUNCTION");

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
   if ( uAlarms & ALARM_ID_UNSUPPORTED_VIDEO_TYPE )
      strcat(szOutput, " CONTROLLER_UNSUPPORTED_VIDEO_TYPE");
   if ( uAlarms & ALARM_ID_FIRMWARE_OLD )
      strcat(szOutput, " FIRMWARE_OLD");
   if ( uAlarms & ALARM_ID_CPU_RX_LOOP_OVERLOAD )
      strcat(szOutput, " ALARM_ID_CPU_RX_LOOP_OVERLOAD");

   if ( uAlarms & ALARM_ID_GENERIC )
      strcat(szOutput, " ALARM_ID_GENERIC");

   if ( uAlarms & ALARM_ID_DEVELOPER_ALARM )
      strcat(szOutput, " ALARM_ID_DEVELOPER");

   if ( 0 == szOutput[0] )
      sprintf(szOutput, "%u", uAlarms);

   char szBuff[128];
   if ( uAlarms & ALARM_ID_CONTROLLER_IO_ERROR )
   {
      char szFlags1[128];
      szFlags1[0] = 0;
      switch ( uFlags1 )
      {
         case ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT: strcpy(szFlags1, "ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT"); break;
         case ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_TRUNCATED: strcpy(szFlags1, "ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_TRUNCATED"); break;
         case ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_WOULD_BLOCK : strcpy(szFlags1, "ALARM_FLAG_IO_ERROR_VIDEO_PLAYER_OUTPUT_WOULD_BLOCK"); break;
         case ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT: strcpy(szFlags1, "ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT"); break;
         case ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_TRUNCATED: strcpy(szFlags1, "ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_TRUNCATED"); break;
         case ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_WOULD_BLOCK: strcpy(szFlags1, "ALARM_FLAG_IO_ERROR_VIDEO_USB_OUTPUT_WOULD_BLOCK"); break;
         case ALARM_FLAG_IO_ERROR_VIDEO_MPP_DECODER_STALLED: strcpy(szFlags1, "MPP_DECODER_STALLED"); break;
         default: sprintf(szFlags1, "Unknown (%u)", uFlags1);

      }
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), ", Flags1: %s, Flags2: %u", szFlags1, uFlags2);
   }
   if ( uAlarms & ALARM_ID_DEVELOPER_ALARM )
   {
      char szFlags1[128];
      szFlags1[0] = 0;
      switch ( uFlags1 )
      {
         case ALARM_FLAG_DEVELOPER_ALARM_RETRANSMISSIONS_OFF: strcpy(szFlags1, "ALARM_FLAG_DEVELOPER_ALARM_RETRANSMISSIONS_OFF"); break;
         case ALARM_FLAG_DEVELOPER_ALARM_UDP_SKIPPED: strcpy(szFlags1, "ALARM_FLAG_DEVELOPER_ALARM_UDP_SKIPPED"); break;
         default: sprintf(szFlags1, "Unknown (%u)", uFlags1);

      }
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), ", Flags1: %s, Flags2: %u", szFlags1, uFlags2);
   }
   else if ( ! (uAlarms & ALARM_ID_GENERIC_STATUS_UPDATE) )
   {
      char szFlags1[128];
      szFlags1[0] = 0;
      switch ( uFlags1 )
      {
         case ALARM_ID_GENERIC_TYPE_UNKNOWN_VIDEO: strcpy(szFlags1, "ALARM_ID_GENERIC_TYPE_UNKNOWN_VIDEO"); break;
         case ALARM_ID_GENERIC_TYPE_RECEIVED_DEPRECATED_MESSAGE: strcpy(szFlags1, "ALARM_ID_GENERIC_TYPE_RECEIVED_DEPRECATED_MESSAGE"); break;
         case ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_NOT_POSSIBLE : strcpy(szFlags1, "ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_NOT_POSSIBLE"); break;
         case ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_FAILED: strcpy(szFlags1, "ALARM_ID_GENERIC_TYPE_SWAP_RADIO_INTERFACES_FAILED"); break;
         case ALARM_ID_GENERIC_TYPE_SWAPPED_RADIO_INTERFACES: strcpy(szFlags1, "ALARM_ID_GENERIC_TYPE_SWAPPED_RADIO_INTERFACES"); break;
         case ALARM_ID_GENERIC_TYPE_RELAYED_TELEMETRY_LOST: strcpy(szFlags1, "ALARM_ID_GENERIC_TYPE_RELAYED_TELEMETRY_LOST"); break;
         case ALARM_ID_GENERIC_TYPE_RELAYED_TELEMETRY_RECOVERED: strcpy(szFlags1, "ALARM_ID_GENERIC_TYPE_RELAYED_TELEMETRY_RECOVERED"); break;
         case ALARM_ID_GENERIC_TYPE_ADAPTIVE_VIDEO_LEVEL_MISSMATCH: strcpy(szFlags1, "ALARM_ID_GENERIC_TYPE_ADAPTIVE_VIDEO_LEVEL_MISSMATCH"); break;
         case ALARM_ID_GENERIC_TYPE_WRONG_OPENIPC_KEY: strcpy(szFlags1, "ALARM_ID_GENERIC_TYPE_WRONG_OPENIPC_KEY"); break;
         case ALARM_ID_GENERIC_TYPE_MISSED_TELEMETRY_DATA: strcpy(szFlags1, "ALARM_ID_GENERIC_TYPE_MISSED_TELEMETRY_DATA"); break;
         default: sprintf(szFlags1, "Unknown (%u)", uFlags1);

      }
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), ", Flags1: %s, Flags2: %u", szFlags1, uFlags2);
   }
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
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), ", Flags1: %s, Flags2: %u", szFlags1, uFlags2);
   }
   strcat(szOutput, szBuff);
}