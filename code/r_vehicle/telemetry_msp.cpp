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

#include "telemetry_msp.h"
#include "telemetry.h"
#include "shared_vars.h"
#include "timers.h"
#include "../base/ruby_ipc.h"
#include "../base/msp.h"

bool isRadioLinksInitInProgress();
extern int s_fIPCToRouter;

u8 s_uMSPRawStream[256]; // Max size is one byte long
int s_iMSPRawStreamFilledBytes = 0;
int s_iMSPState = 0;
int s_iMSPDirection = 0;
int s_iMSPCommandDataSize = 0;
int s_iMSPParsedDataSize = 0;
u8 s_uMSPCommandData[256]; // Max size is one byte long
u8 s_uMSPChecksum = 0;
u8 s_uMSPCommand = 0;
u32 s_uLastMSPCommandReceivedTime = 0;

u8 s_uMSPOutputBuffer[MAX_PACKET_PAYLOAD];
int s_iMSPOutputBufferFilledBytes = 0;
u32 s_uTimeLastSentMSPPacketToRouter = 0;

t_packet_header_telemetry_msp s_PHTMSP;

u32 s_uMSPTimeLastConfigCommandToFC = 0;
bool s_bMSPGotFCInfo = false;

u32 s_uMSPLastRequestBatteryInfoTime = 0;

void _send_msp_to_fc(u8 uCommand, u8* pData, int iDataLength)
{
   if ( iDataLength < 0 )
      iDataLength = 0;
   if ( iDataLength > 250 )
      iDataLength = 250;
   
   u8 uMSPBuffer[256];
   u8 uChecksum;

   uMSPBuffer[0] = '$';
   uMSPBuffer[1] = 'M';
   uMSPBuffer[2] = '<';
   uMSPBuffer[3] = iDataLength;
   uMSPBuffer[4] = uCommand;

   uChecksum = (u8)iDataLength;
   uChecksum ^= uCommand;

   if ( NULL != pData )
   {
      for( int i=0; i<iDataLength; i++ )
      {
         uMSPBuffer[5+i] = pData[i];
         uChecksum ^= pData[i];
      }
   }

   uMSPBuffer[5 + iDataLength] = uChecksum;
   int iTotalSize = iDataLength + 6;

   if ( write(telemetry_get_serial_port_file(), uMSPBuffer, iTotalSize) != iTotalSize )
      log_softerror_and_alarm("[Telem] Failed to write MSD (%d bytes) to serial port to FC", iTotalSize);
}

void telemetry_msp_on_open_port(int iSerialPortFile)
{
   s_iMSPRawStreamFilledBytes = 0;
   s_iMSPOutputBufferFilledBytes = 0;
   s_uTimeLastSentMSPPacketToRouter = g_TimeNow;
   s_iMSPState = MSP_STATE_NONE;
   s_bMSPGotFCInfo = false;
   s_uMSPLastRequestBatteryInfoTime = 0;

   memset(&s_PHTMSP, 0, sizeof(t_packet_header_telemetry_msp));
   s_PHTMSP.uCols = 60;
   s_PHTMSP.uRows = 22;
}

void telemetry_msp_on_close()
{

}

void telemetry_msp_periodic_loop()
{
   if ( ! s_bMSPGotFCInfo )
   if ( g_TimeNow >= s_uMSPTimeLastConfigCommandToFC + 500 )
   {
      s_uMSPTimeLastConfigCommandToFC = g_TimeNow;
      _send_msp_to_fc(MSP_CMD_FC_VARIANT, NULL, 0);
      return;
   }

   if ( g_TimeNow >= s_uMSPLastRequestBatteryInfoTime + 500 )
   {
      s_uMSPLastRequestBatteryInfoTime = g_TimeNow;
      _send_msp_to_fc(MSP_CMD_BATTERY_STATE, NULL, 0); 
   } 
}

u32 telemetry_msp_get_last_command_received_time()
{
   return s_uLastMSPCommandReceivedTime;
}

void telemetry_msp_set_last_command_received_time(u32 uTime)
{
   s_uLastMSPCommandReceivedTime = uTime;
}

void _send_msp_telemetry_packet_to_controller()
{
   if ( s_iMSPOutputBufferFilledBytes <= 0 )
      return;
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_TELEMETRY_MSP, STREAM_ID_TELEMETRY);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header) + sizeof(t_packet_header_telemetry_msp) + s_iMSPOutputBufferFilledBytes;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, &PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), &s_PHTMSP, sizeof(t_packet_header_telemetry_msp));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_msp), s_uMSPOutputBuffer, s_iMSPOutputBufferFilledBytes);

   if ( g_bRouterReady && (! isRadioLinksInitInProgress()) )
   {
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
      if ( result != PH.total_length )
         log_softerror_and_alarm("[Telem] Failed to send data to router. Sent result: %d", result );
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   s_uTimeLastSentMSPPacketToRouter = g_TimeNow;
   s_iMSPOutputBufferFilledBytes = 0;
}

void _add_msp_data_to_output(u8* pData, int iDataLength, bool bSendNow)
{
   if ( (NULL == pData) || (iDataLength <= 0) || (iDataLength > 255) )
      return;

   // No more room in the output? Send packet
   if ( s_iMSPOutputBufferFilledBytes + iDataLength >= 1100 )
      _send_msp_telemetry_packet_to_controller();

   memcpy(&s_uMSPOutputBuffer[s_iMSPOutputBufferFilledBytes], pData, iDataLength);
   s_iMSPOutputBufferFilledBytes += iDataLength;

   if ( bSendNow && (g_TimeNow > s_uTimeLastSentMSPPacketToRouter + 50) )
      _send_msp_telemetry_packet_to_controller();
}

void _parse_msp_osd_command()
{
   if ( (s_uMSPCommand != MSP_CMD_DISPLAYPORT) || (s_iMSPCommandDataSize < 1) || (s_iMSPDirection != MSP_DIR_FROM_FC) )
      return;
   
   bool bSendNow = false;

   switch ( s_uMSPCommandData[0] )
   {
      case MSP_DISPLAYPORT_DRAW_STRING:
         {
            int x = s_uMSPCommandData[2];
            int y = s_uMSPCommandData[1];
            if ( x >= s_PHTMSP.uCols )
            {
               if ( x >= 50 )
                  s_PHTMSP.uCols = 60;
               else if ( x >= 30 )
                  s_PHTMSP.uCols = 50;
               else
                  s_PHTMSP.uCols = 30;
            }
            if ( y >= s_PHTMSP.uRows )
            {
               if ( y >= 20 )
                  s_PHTMSP.uRows = 22;
               else if ( y >= 18 )
                  s_PHTMSP.uRows = 20;
               else if ( y >= 16 )
                  s_PHTMSP.uRows = 18;
               else
                  s_PHTMSP.uRows = 16;
            }
         }
         break;

      case MSP_DISPLAYPORT_SET_OPTIONS:
         {
            if ( s_iMSPCommandDataSize >= 3 )
            {
               if ( s_uMSPCommandData[2] == MSP_SD_OPTION_30_16 )
               {
                  s_PHTMSP.uCols = 30;
                  s_PHTMSP.uRows = 16;
               }
               if ( s_uMSPCommandData[2] == MSP_HD_OPTION_50_18 )
               {
                  s_PHTMSP.uCols = 50;
                  s_PHTMSP.uRows = 18;
               }
               if ( s_uMSPCommandData[2] == MSP_HD_OPTION_30_16 )
               {
                  s_PHTMSP.uCols = 30;
                  s_PHTMSP.uRows = 16;
               }
               if ( s_uMSPCommandData[2] == MSP_HD_OPTION_60_22 )
               {
                  s_PHTMSP.uCols = 60;
                  s_PHTMSP.uRows = 22;
               }
            }
         }
         break;
      case MSP_DISPLAYPORT_DRAW_SCREEN:
      case MSP_DISPLAYPORT_DRAW_SYSTEM:
         bSendNow = true;
         break;

      default: break;
   }

   _add_msp_data_to_output(s_uMSPRawStream, s_iMSPRawStreamFilledBytes, bSendNow);
}

void _parse_msp_command()
{
   if ( s_iMSPDirection != MSP_DIR_FROM_FC )
      return;

   switch ( s_uMSPCommand )
   {
      case MSP_CMD_FC_VARIANT:
         {
            char szBuff[5];
            strncpy(szBuff, (char*)s_uMSPCommandData, 4);
            szBuff[4] = 0;
            log_line("[Telem] Got MSP FC variant: (%s)", szBuff);
            s_PHTMSP.uFlags &= ~MSP_FLAGS_FC_TYPE_MASK;
            if ( strncmp("BTFL", (char*)s_uMSPCommandData, s_iMSPCommandDataSize) == 0 )
               s_PHTMSP.uFlags |= MSP_FLAGS_FC_TYPE_BETAFLIGHT;
            else if ( strncmp("ARDU", (char*)s_uMSPCommandData, s_iMSPCommandDataSize) == 0 )
               s_PHTMSP.uFlags |= MSP_FLAGS_FC_TYPE_ARDUPILOT;
            else // "INAV"
               s_PHTMSP.uFlags |= MSP_FLAGS_FC_TYPE_INAV;

            _send_msp_to_fc(MSP_CMD_API_VERSION, NULL, 0);
         }
         break;

      case MSP_CMD_API_VERSION:
         {
            s_bMSPGotFCInfo = true;
            for( int i=0; i<s_iMSPCommandDataSize; i++ )
               log_line("[Telem] Got MSP API version, byte[%d]=%d", i, s_uMSPCommandData[i]);
            u8 uBuffer[2];
            uBuffer[0] = 60;
            uBuffer[1] = 22;
            _send_msp_to_fc(MSP_CMD_SET_OSD_CANVAS, uBuffer, 2);
         }
         break;
      default: break;
   }
}

bool telemetry_msp_on_new_serial_data(u8* pData, int iDataLength)
{
   if ( (NULL == pData) || (iDataLength <= 0) )
      return false;
   
   for( int i=0; i<iDataLength; i++ )
   {
      s_uMSPRawStream[s_iMSPRawStreamFilledBytes] = *pData;
      s_iMSPRawStreamFilledBytes++;
      if ( s_iMSPRawStreamFilledBytes >= 256 )
         s_iMSPRawStreamFilledBytes = 0;

      switch(s_iMSPState)
      {
         case MSP_STATE_NONE:
         case MSP_STATE_ERROR:
            s_iMSPRawStreamFilledBytes = 0;
            if ( *pData == '$' )
            {
               s_uMSPRawStream[0] = *pData;
               s_iMSPRawStreamFilledBytes = 1;
               s_iMSPState = MSP_STATE_WAIT_HEADER;
            }
            break;

         case MSP_STATE_WAIT_HEADER:
            if ( *pData == 'M' )
               s_iMSPState = MSP_STATE_WAIT_DIR;
            else
            {
               s_iMSPState = MSP_STATE_ERROR;
            }
            break;

         case MSP_STATE_WAIT_DIR:
            if ( *pData == '<' )
            {
               s_iMSPState = MSP_STATE_WAIT_SIZE;
               s_iMSPDirection = MSP_DIR_TO_FC;
            }
            else if ( *pData == '>' )
            {
               s_iMSPState = MSP_STATE_WAIT_SIZE;
               s_iMSPDirection = MSP_DIR_FROM_FC;
            }
            else
            {
               s_iMSPState = MSP_STATE_NONE;
            }
            break;

         case MSP_STATE_WAIT_SIZE:
            s_iMSPCommandDataSize = (int) *pData;
            s_uMSPChecksum = *pData;
            s_iMSPState = MSP_STATE_WAIT_TYPE;
            break;

         case MSP_STATE_WAIT_TYPE:
            s_uMSPCommand = *pData;
            s_uMSPChecksum ^= *pData;
            s_iMSPParsedDataSize = 0;
            if ( s_iMSPCommandDataSize > 0 )
               s_iMSPState = MSP_STATE_PARSE_DATA;
            else
               s_iMSPState = MSP_STATE_WAIT_CHECKSUM;
            break;

         case MSP_STATE_PARSE_DATA:
            s_uMSPCommandData[s_iMSPParsedDataSize] = *pData;
            s_iMSPParsedDataSize++;
            s_uMSPChecksum ^= *pData;
            if ( s_iMSPParsedDataSize >= s_iMSPCommandDataSize )
               s_iMSPState = MSP_STATE_WAIT_CHECKSUM;
            break;

         case MSP_STATE_WAIT_CHECKSUM:
            if ( s_uMSPChecksum == *pData )
            {
               s_uLastMSPCommandReceivedTime = g_TimeNow;
               if ( s_uMSPCommand == MSP_CMD_DISPLAYPORT )
                  _parse_msp_osd_command();
               else
                  _parse_msp_command();
            }
            s_iMSPState = MSP_STATE_NONE;
            break;

         default:
            break;
      }
      pData++;
   }
   return false;
}
