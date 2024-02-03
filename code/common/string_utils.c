/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
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
#include "../base/hardware.h"
#include "../base/hardware_radio.h"
#include "../base/flags.h"
#include "../radio/radioflags.h"
#include "../radio/radiopackets2.h"
#include <ctype.h>
#include "string_utils.h"


void str_sanitize_modelname(char* szName)
{
   if ( NULL == szName || 0 == szName[0] )
      return;

   //log_line("Sanitize model name: [%s]", szName);
   int len = strlen(szName);

   // Remove ending invalid chars

   len--;
   while ( len > 0 )
   {
      int isValid = 1;

      if ( szName[len] == ' ' )
         isValid = 0;
      if ( szName[len] < 48 )
         isValid = 0;
      if ( szName[len] >= 58 && szName[len] <= 64 )
         isValid = 0;
      if ( szName[len] >= 91 && szName[len] <= 96 )
         isValid = 0;
      if ( szName[len] > 122 )
         isValid = 0;
      if ( isValid )
         break;
      szName[len] = 0;
      len--;
   }

   // Remove starting invalid chars

   len = strlen(szName);

   int iSkip = 0;
   while ( iSkip < len )
   {
      int isValid = 1;

      if ( szName[iSkip] == ' ' )
         isValid = 0;
      if ( szName[iSkip] < 48 )
         isValid = 0;
      if ( szName[iSkip] >= 58 && szName[iSkip] <= 64 )
         isValid = 0;
      if ( szName[iSkip] >= 91 && szName[iSkip] <= 96 )
         isValid = 0;
      if ( szName[iSkip] > 122 )
         isValid = 0;
      if ( isValid )
         break;
      isValid++;
   }

   if ( iSkip > 0 )
   {
      for( int i=0; i<len-iSkip; i++ )
         szName[i] = szName[i+iSkip];
      szName[len-iSkip] = 0;
      len -= iSkip;
   }

   // Replace middle invalid chars

   len = strlen(szName);

   for( int i=0; i<len; i++ )
   {
      int isValid = 1;

      if ( szName[i] == ' ' )
         isValid = 0;
      if ( szName[i] < 48 )
         isValid = 0;
      if ( szName[i] >= 58 && szName[i] <= 64 )
         isValid = 0;
      if ( szName[i] >= 91 && szName[i] <= 96 )
         isValid = 0;
      if ( szName[i] > 122 )
         isValid = 0;
      if ( ! isValid )
         szName[i] = '_';
   }
   //log_line("Sanitized model name: [%s]", szName);
}

void str_sanitize_filename(char* szFileName)
{
   if ( NULL == szFileName || 0 == szFileName[0] )
      return;

   log_line("Sanitize filename: [%s]", szFileName);
   int len = strlen(szFileName);

   len--;
   while ( len > 0 )
   {
      if ( szFileName[len] != ' ' && szFileName[len] != '.' && szFileName[len] != '/' && szFileName[len] != '\\' && szFileName[len] != 96 && szFileName[len] <= 122 && szFileName[len] >= 48 )
         break;
      szFileName[len] = 0;
      len--;
   }

   len = strlen(szFileName);
   for( int i=0; i<len; i++ )
   {
      if ( szFileName[i] == ' ' || szFileName[i] == '\'' || szFileName[i] == '\"' )
         szFileName[i] = '_';
      if ( szFileName[i] < 32 )
         szFileName[i] = '_';
      else
      {
         if ( szFileName[i] < 48 )
            szFileName[i] = '-';
         else if ( szFileName[i] >= 58 && szFileName[i] <= 64 )
            szFileName[i] = '-';
         else if ( szFileName[i] >= 91 && szFileName[i] <= 96 )
            szFileName[i] = '-';
         else if ( szFileName[i] > 122 )
            szFileName[i] = '-';
      }
   }
   log_line("Sanitized filename: [%s]", szFileName);
}

char* str_capitalize_first_letter(char* szText)
{
   if ( (NULL != szText) && (0 != szText[0]) )
      szText[0] = toupper(szText[0]);
   return szText;
}

char* str_format_time(u32 miliseconds)
{
   static char s_szFormatTime[4][64];
   static int s_iFormatTimeIndex = 0;
   s_iFormatTimeIndex++;
   if ( s_iFormatTimeIndex >= 4 )
      s_iFormatTimeIndex = 0;
   sprintf(s_szFormatTime[s_iFormatTimeIndex],"%d:%02d:%02d.%03d", (int)(miliseconds/1000/60/60), (int)(miliseconds/1000/60)%60, (int)((miliseconds/1000)%60), (int)(miliseconds%1000));
   return s_szFormatTime[s_iFormatTimeIndex];
}


char* str_get_pipe_flags(int iFlags)
{
   static char s_szBufferPipeFlags[256];
   s_szBufferPipeFlags[0] = 0;

   strcpy(s_szBufferPipeFlags, "[");

   if ( iFlags & O_RDONLY )
      strcat(s_szBufferPipeFlags, " O_RDONLY");
   else
      strcat(s_szBufferPipeFlags, " !O_RDONLY");

   if ( iFlags & O_WRONLY )
      strcat(s_szBufferPipeFlags, " O_WRONLY");
   else
      strcat(s_szBufferPipeFlags, " !O_WRONLY");

   if ( iFlags & O_RDWR )
      strcat(s_szBufferPipeFlags, " O_RDWR");
   else
      strcat(s_szBufferPipeFlags, " !O_RDWR");

   if ( iFlags & O_APPEND )
      strcat(s_szBufferPipeFlags, " O_APPEND");
   else
      strcat(s_szBufferPipeFlags, " !O_APPEND");

   if ( iFlags & O_ASYNC )
      strcat(s_szBufferPipeFlags, " O_ASYNC");
   else
      strcat(s_szBufferPipeFlags, " !O_ASYNC");

   if ( iFlags & O_NONBLOCK )
      strcat(s_szBufferPipeFlags, " O_NONBLOCK");
   else
      strcat(s_szBufferPipeFlags, " !O_NONBLOCK");

   if ( iFlags & O_DSYNC )
      strcat(s_szBufferPipeFlags, " O_DSYNC");
   else
      strcat(s_szBufferPipeFlags, " !O_DSYNC");

   if ( iFlags & O_SYNC )
      strcat(s_szBufferPipeFlags, " O_SYNC");
   else
      strcat(s_szBufferPipeFlags, " !O_SYNC");

   if ( iFlags & O_EXCL )
      strcat(s_szBufferPipeFlags, " O_EXCL");
   else
      strcat(s_szBufferPipeFlags, " !O_EXCL");

   strcat(s_szBufferPipeFlags, " ]");
   return s_szBufferPipeFlags;
}

char* str_get_packet_type(int iPacketType)
{
   static char s_szPacketType[128];

   switch(iPacketType)
   {
      case PACKET_TYPE_EMBEDED_SHORT_PACKET: strcpy(s_szPacketType, "PACKET_TYPE_EMBEDED_SHORT_PACKET"); break;
      case PACKET_TYPE_EMBEDED_FULL_PACKET: strcpy(s_szPacketType, "PACKET_TYPE_EMBEDED_FULL_PACKET"); break;
      
      case PACKET_TYPE_RUBY_PING_CLOCK:          strcpy(s_szPacketType, "PACKET_TYPE_RUBY_PING_CLOCK"); break;
      case PACKET_TYPE_RUBY_PING_CLOCK_REPLY:    strcpy(s_szPacketType, "PACKET_TYPE_RUBY_PING_CLOCK_REPLY"); break;
      case PACKET_TYPE_RUBY_RADIO_REINITIALIZED: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_RADIO_REINITIALIZED"); break;
      case PACKET_TYPE_RUBY_MODEL_SETTINGS:      strcpy(s_szPacketType, "PACKET_TYPE_RUBY_MODEL_SETTINGS"); break;
      case PACKET_TYPE_RUBY_PAIRING_REQUEST:     strcpy(s_szPacketType, "PACKET_TYPE_RUBY_PAIRING_REQUEST"); break;
      case PACKET_TYPE_RUBY_PAIRING_CONFIRMATION: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_PAIRING_CONFIRMATION"); break;
      case PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED"); break;
      case PACKET_TYPE_RUBY_LOG_FILE_SEGMENT:    strcpy(s_szPacketType, "PACKET_TYPE_RUBY_LOG_FILE_SEGMENT"); break;
      case PACKET_TYPE_RUBY_ALARM:               strcpy(s_szPacketType, "PACKET_TYPE_RUBY_ALARM"); break;
      case PACKET_TYPE_VIDEO_DATA_FULL:          strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_DATA_FULL"); break;
      case PACKET_TYPE_AUDIO_SEGMENT:            strcpy(s_szPacketType, "PACKET_TYPE_AUDIO_SEGMENT"); break;
      case PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS:   strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS"); break;
      case PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2:  strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2"); break;
      case PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL:     strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL"); break;
      case PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK: strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK"); break;
      case PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE:     strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE"); break;
      case PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK: strcpy(s_szPacketType, "PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK"); break;
      case PACKET_TYPE_COMMAND:                  strcpy(s_szPacketType, "PACKET_TYPE_COMMAND"); break;
      case PACKET_TYPE_COMMAND_RESPONSE:         strcpy(s_szPacketType, "PACKET_TYPE_COMMAND_RESPONSE"); break;
      case PACKET_TYPE_SIK_CONFIG:               strcpy(s_szPacketType, "PACKET_TYPE_SIK_CONFIG"); break;
      
      // Telemetry

      case PACKET_TYPE_RUBY_TELEMETRY_SHORT:      strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_SHORT"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_EXTENDED:   strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_EXTENDED"); break;
      case PACKET_TYPE_FC_TELEMETRY:              strcpy(s_szPacketType, "PACKET_TYPE_FC_TELEMETRY"); break;
      case PACKET_TYPE_FC_TELEMETRY_EXTENDED:     strcpy(s_szPacketType, "PACKET_TYPE_FC_TELEMETRY_EXTENDED"); break;
      case PACKET_TYPE_FC_RC_CHANNELS:            strcpy(s_szPacketType, "PACKET_TYPE_FC_RC_CHANNELS"); break;
      case PACKET_TYPE_RC_TELEMETRY:              strcpy(s_szPacketType, "PACKET_TYPE_RC_TELEMETRY"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS:   strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_GRAPHS:  strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_GRAPHS"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY:     strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY"); break;
      case PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD:    strcpy(s_szPacketType, "PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD"); break;
      case PACKET_TYPE_TELEMETRY_RAW_UPLOAD:      strcpy(s_szPacketType, "PACKET_TYPE_TELEMETRY_RAW_UPLOAD"); break;
      case PACKET_TYPE_AUX_DATA_LINK_UPLOAD:      strcpy(s_szPacketType, "PACKET_TYPE_AUX_DATA_LINK_UPLOAD"); break;
      case PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD:    strcpy(s_szPacketType, "PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS:  strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS"); break;
      case PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY: strcpy(s_szPacketType, "PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY"); break;

      case PACKET_TYPE_VEHICLE_RECORDING: strcpy(s_szPacketType, "PACKET_TYPE_VEHICLE_RECORDING"); break;
       
      // Local packets

      case PACKET_TYPE_LOCAL_CONTROL_PAUSE_VIDEO:           strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_PAUSE_VIDEO"); break;
      case PACKET_TYPE_LOCAL_CONTROL_RESUME_VIDEO:          strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_RESUME_VIDEO"); break;
      case PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM"); break;
      case PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED:         strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_SIGNAL_VIDEO_ENCODINGS_CHANGED:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_SIGNAL_VIDEO_ENCODINGS_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED:    strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_START_VIDEO_PROGRAM:   strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_START_VIDEO_PROGRAM"); break;
      case PACKET_TYPE_LOCAL_CONTROL_REBOOT:                strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_REBOOT"); break;
      case PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED:        strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_UPDATE_STARTED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED:         strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_UPDATE_STOPED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED:       strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_UPDATE_FINISHED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_START:     strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_START"); break;
      case PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_STOP:      strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_STOP"); break;
      case PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_PAUSE:     strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_PAUSE"); break;
      case PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_RESUME:    strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_DEBUG_SCOPE_RESUME"); break;
      case PACKET_TYPE_LOCAL_CONTROL_UPDATED_VIDEO_LINK_OVERWRITES:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_UPDATED_VIDEO_LINK_OVERWRITES"); break;
      case PACKET_TYPE_LOCAL_CONTROL_RELAY_MODE_SWITCHED:            strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_RELAY_MODE_SWITCHED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_BROADCAST_RADIO_REINITIALIZED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_RECEIVED_MODEL_SETTING:         strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_RECEIVED_MODEL_SETTING"); break;
      case PACKET_TYPE_LOCAL_CONTROL_REINITIALIZE_RADIO_LINKS:       strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_REINITIALIZE_RADIO_LINKS"); break;
      case PACKET_TYPE_LOCAL_CONTROL_RECEIVED_VEHICLE_LOG_SEGMENT:   strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_RECEIVED_VEHICLE_LOG_SEGMENT"); break;
      case PACKET_TYPE_LOCAL_CONTROL_SIGNAL_USER_SELECTED_VIDEO_PROFILE_CHANGED:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_SIGNAL_USER_SELECTED_VIDEO_PROFILE_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_PASSPHRASE_CHANGED:      strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_PASSPHRASE_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_I2C_DEVICE_CHANGED:      strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_I2C_DEVICE_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY:         strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROLLER_ROUTER_READY"); break;
      case PACKET_TYPE_LOCAL_CONTROLLER_RADIO_INTERFACE_FAILED_TO_INITIALIZE:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROLLER_RADIO_INTERFACE_FAILED_TO_INITIALIZE"); break;
      case PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROLLER_RELOAD_CORE_PLUGINS"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_CAMERA_PARAM: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_CAMERA_PARAM"); break;
      case PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS"); break;
      case PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED:          strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_LINK_FREQUENCY_CHANGED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ1_UP:   strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ1_UP"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ1_DOWN: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ1_DOWN"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ2_UP:   strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ2_UP"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ2_DOWN: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ2_DOWN"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_FORCE_AUTO_VIDEO_PROFILE: strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_FORCE_AUTO_VIDEO_PROFILE"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_VIDEO_PROFILE_SWITCHED:   strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_VIDEO_PROFILE_SWITCHED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_ROUTER_READY:             strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_ROUTER_READY"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_SIK_RADIO_SERIAL_SPEED:  strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_SIK_RADIO_SERIAL_SPEED"); break;
      case PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS:      strcpy(s_szPacketType, "PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS"); break;

      default: sprintf(s_szPacketType, "Unknown %d", iPacketType); break;
   }
   return s_szPacketType;
}

char* str_get_packet_history_symbol(int iPacketType, int iRepeatCount)
{
   static char s_szOSDRenderRxHistoryPacketSymbol[4];
   s_szOSDRenderRxHistoryPacketSymbol[0] = 'o';
   s_szOSDRenderRxHistoryPacketSymbol[1] = 0;

   if ( iPacketType == 0xFF ) // End of seccond
   {
      s_szOSDRenderRxHistoryPacketSymbol[0] = '*';
      return s_szOSDRenderRxHistoryPacketSymbol;
   }

   if ( iPacketType == 0x00 ) // Missing packets or none
   {
      if ( iRepeatCount > 0 )
      {
         s_szOSDRenderRxHistoryPacketSymbol[0] = 'x';
         return s_szOSDRenderRxHistoryPacketSymbol;
      }
      s_szOSDRenderRxHistoryPacketSymbol[0] = ' ';
      return s_szOSDRenderRxHistoryPacketSymbol;
   }
   if ( iPacketType == PACKET_TYPE_RUBY_PING_CLOCK ||
        iPacketType == PACKET_TYPE_RUBY_PING_CLOCK_REPLY )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'P';

   if ( iPacketType == PACKET_TYPE_COMMAND ||
        iPacketType == PACKET_TYPE_COMMAND_RESPONSE )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'C';

   if ( iPacketType == PACKET_TYPE_AUDIO_SEGMENT )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'A';

   if ( iPacketType == PACKET_TYPE_VIDEO_DATA_FULL ||
        iPacketType == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS ||
        iPacketType == PACKET_TYPE_VIDEO_REQ_MULTIPLE_PACKETS2 )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'V';

   if ( iPacketType == PACKET_TYPE_AUX_DATA_LINK_UPLOAD ||
        iPacketType == PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'D';

   if ( iPacketType == PACKET_TYPE_RUBY_TELEMETRY_SHORT ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'R';

   if ( iPacketType == PACKET_TYPE_RC_TELEMETRY ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_GRAPHS ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS ||
        iPacketType == PACKET_TYPE_RUBY_TELEMETRY_RADIO_RX_HISTORY )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'r';

   if ( iPacketType == PACKET_TYPE_FC_TELEMETRY ||
        iPacketType == PACKET_TYPE_FC_TELEMETRY_EXTENDED )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'T';
   if ( iPacketType == PACKET_TYPE_FC_RC_CHANNELS )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 't';

   if ( iPacketType == PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL ||
        iPacketType == PACKET_TYPE_VIDEO_SWITCH_TO_ADAPTIVE_VIDEO_LEVEL_ACK ||
        iPacketType == PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE ||
        iPacketType == PACKET_TYPE_VIDEO_SWITCH_VIDEO_KEYFRAME_TO_VALUE_ACK )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'S';

   if ( iPacketType == PACKET_TYPE_RUBY_MODEL_SETTINGS )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'M';

   if ( iPacketType == PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'U';

   if ( iPacketType == PACKET_TYPE_RUBY_ALARM )
      s_szOSDRenderRxHistoryPacketSymbol[0] = 'a';

   return s_szOSDRenderRxHistoryPacketSymbol;
}

char* str_get_packet_test_link_command(int iTestCommandId)
{
   static char s_szTestRadioLinkCommandType[32];
   s_szTestRadioLinkCommandType[0] = 0;

   if ( iTestCommandId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_STATUS )
      strcpy(s_szTestRadioLinkCommandType, "[Status]");
   if ( iTestCommandId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START )
      strcpy(s_szTestRadioLinkCommandType, "[Start]");
   if ( iTestCommandId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING )
      strcpy(s_szTestRadioLinkCommandType, "[Ping]");
   if ( iTestCommandId == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END )
      strcpy(s_szTestRadioLinkCommandType, "[End]");

   return s_szTestRadioLinkCommandType;
}

void str_getDataRateDescription(int dataRateBPS, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   szOutput[0] = 0;

   if ( dataRateBPS < 0 )
   {
      int mcsIndex = -dataRateBPS-1;
      if ( mcsIndex <= MAX_MCS_INDEX )
         sprintf(szOutput, "MCS-%d %u Mb", mcsIndex, getRealDataRateFromMCSRate(mcsIndex)/1000/1000 );
      else
         sprintf(szOutput, "MCS-?");
   }
   else if ( 0 == dataRateBPS )
   {
      strcpy(szOutput, "None (0)");
   }
   else if ( dataRateBPS <= 56 )
   {
       sprintf(szOutput, "*%d Mbps", dataRateBPS);
   }
   else
   {
      if ( dataRateBPS >= 1000000 )
      {
         if ( ((dataRateBPS /1000) % 1000) != 0 )
            sprintf(szOutput, "%1.f Mbps", (float)dataRateBPS/1000.0/1000.0);
         else
            sprintf(szOutput, "%d Mbps", dataRateBPS/1000/1000);
      }
      else if ( dataRateBPS >= 10000 )
         sprintf(szOutput, "%d kbps", dataRateBPS/1000);
      else
         sprintf(szOutput, "%d bps", dataRateBPS);
   }
}


void str_getDataRateDescriptionNoSufix(int dataRateBPS, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   szOutput[0] = 0;

   if ( dataRateBPS < 0 )
   {
      int mcsIndex = -dataRateBPS-1;
      if ( mcsIndex <= MAX_MCS_INDEX )
         sprintf(szOutput, "MCS-%d", mcsIndex);
      else
         sprintf(szOutput, "MCS-?");
   }
   else if ( dataRateBPS <= 56 )
   {
       sprintf(szOutput, "*%d", dataRateBPS);
   }
   else
   {
      if ( dataRateBPS >= 1000000 )
      {
         if ( ((dataRateBPS /1000) % 1000) != 0 )
            sprintf(szOutput, "%1.f", (float)dataRateBPS/1000.0/1000.0);
         else
            sprintf(szOutput, "%d", dataRateBPS/1000/1000);
      }
      else if ( dataRateBPS >= 10000 )
         sprintf(szOutput, "%d", dataRateBPS/1000);
      else
         sprintf(szOutput, "%d", dataRateBPS);
   }
}

void str_format_bitrate(int bitrate_bps, char* szBuffer)
{
   if ( NULL == szBuffer )
      return;
   if ( bitrate_bps < 0 )
      sprintf(szBuffer, "MCS-%d", -bitrate_bps-1); // starts from MCS-0 (which is -1)
   else if ( bitrate_bps <= 56 )
   {
      if ( bitrate_bps == 0 )
         strcpy(szBuffer, "0 bps");
      else
         sprintf(szBuffer, "*%d Mbps", bitrate_bps);
   }
   else if ( bitrate_bps < 1000 ) // less than 1kb
      sprintf(szBuffer, "%d bps", bitrate_bps);
   else if ( bitrate_bps < 10000 ) // less than 10 kb
      sprintf(szBuffer, "%.1f kbps", (float)bitrate_bps/1000.0);
   else if ( bitrate_bps < 500000 ) // less than 500 kb
      sprintf(szBuffer, "%d kbps", bitrate_bps/1000);
   else
   {
      if ( ((bitrate_bps/1000)%1000) == 0 )
         sprintf(szBuffer, "%d Mbps", bitrate_bps/1000/1000);
      else
         sprintf(szBuffer, "%.1f Mbps", (float)bitrate_bps/1000.0/1000.0);
   }
}

void str_format_bitrate_no_sufix(int bitrate_bps, char* szBuffer)
{
   if ( NULL == szBuffer )
      return;
   if ( bitrate_bps < 0 )
      sprintf(szBuffer, "MCS-%d", -bitrate_bps-1); // starts from MCS-0 (which is -1)
   else if ( bitrate_bps <= 56 )
      sprintf(szBuffer, "*%d", bitrate_bps*1000*1000);
   else if ( bitrate_bps < 1000 ) // less than 1kb
      sprintf(szBuffer, "%d", bitrate_bps);
   else if ( bitrate_bps < 10000 ) // less than 10 kb
      sprintf(szBuffer, "%.1f", (float)bitrate_bps/1000.0);
   else if ( bitrate_bps < 500000 ) // less than 500 kb
      sprintf(szBuffer, "%d", bitrate_bps/1000);
   else
   {
      if ( ((bitrate_bps/1000)%1000) == 0 )
         sprintf(szBuffer, "%d", bitrate_bps/1000/1000);
      else
         sprintf(szBuffer, "%.1f", (float)bitrate_bps/1000.0/1000.0);
   }
}

const char* str_getBandName(u32 band)
{
   if ( band & RADIO_HW_SUPPORTED_BAND_433 )
      return "433 Mhz";
   if ( band & RADIO_HW_SUPPORTED_BAND_868 )
      return "868 Mhz";
   if ( band & RADIO_HW_SUPPORTED_BAND_915 )
      return "915 Mhz";
   if ( band & RADIO_HW_SUPPORTED_BAND_23 )
      return "2.3 Ghz";
   if ( band & RADIO_HW_SUPPORTED_BAND_24 )
      return "2.4 Ghz";
   if ( band & RADIO_HW_SUPPORTED_BAND_25 )
      return "2.5 Ghz";
   if ( band & RADIO_HW_SUPPORTED_BAND_58 )
      return "5.8 Ghz";
   return "";
}

char* str_format_frequency(u32 uFrequencyKhz)
{
   static char s_szFrequencyFormat[64];

   s_szFrequencyFormat[0] = 0;
   
   if ( uFrequencyKhz < 10000 )
      uFrequencyKhz *= 1000;
     
   if ( (uFrequencyKhz%1000) == 0 )
      sprintf(s_szFrequencyFormat, "%u Mhz", uFrequencyKhz/1000);
   else if ( (uFrequencyKhz%100) == 0 )
      sprintf(s_szFrequencyFormat, "%u.%u Mhz", uFrequencyKhz/1000, (uFrequencyKhz%1000) / 100);
   else if ( (uFrequencyKhz%10) == 0 )
      sprintf(s_szFrequencyFormat, "%u.%u Mhz", uFrequencyKhz/1000, (uFrequencyKhz%1000) / 10);
   else
      sprintf(s_szFrequencyFormat, "%u.%u Mhz", uFrequencyKhz/1000, uFrequencyKhz%1000);

   return s_szFrequencyFormat;
}


char* str_format_frequency_no_sufix(u32 uFrequencyKhz)
{
   static char s_szFrequencyFormatNoSufix[64];

   s_szFrequencyFormatNoSufix[0] = 0;
   
   if ( (uFrequencyKhz%1000) == 0 )
      sprintf(s_szFrequencyFormatNoSufix, "%u", uFrequencyKhz/1000);
   else if ( (uFrequencyKhz%100) == 0 )
      sprintf(s_szFrequencyFormatNoSufix, "%u.%u", uFrequencyKhz/1000, (uFrequencyKhz%1000) / 100);
   else if ( (uFrequencyKhz%10) == 0 )
      sprintf(s_szFrequencyFormatNoSufix, "%u.%u", uFrequencyKhz/1000, (uFrequencyKhz%1000) / 10);
   else
      sprintf(s_szFrequencyFormatNoSufix, "%u.%u", uFrequencyKhz/1000, uFrequencyKhz%1000);

   return s_szFrequencyFormatNoSufix;
}


const char* str_get_hardware_board_name(u32 board_type)
{
   static const char* s_szBoardTypeUnknown = "N/A";
   static const char* s_szBoardTypePi0 = "Raspberry Pi Zero";
   static const char* s_szBoardTypePi0W = "Raspberry Pi Zero W";
   static const char* s_szBoardTypePi02 = "Raspberry Pi Zero 2";
   static const char* s_szBoardTypePi2B = "Raspberry Pi 2B";
   static const char* s_szBoardTypePi2BV1 = "Raspberry Pi 2B v1.1";
   static const char* s_szBoardTypePi2BV12 = "Raspberry Pi 2B v1.2";
   static const char* s_szBoardTypePi3AP = "Raspberry Pi 3A+";
   static const char* s_szBoardTypePi3B = "Raspberry Pi 3B";
   static const char* s_szBoardTypePi3BP = "Raspberry Pi 3B+";
   static const char* s_szBoardTypePi4B = "Raspberry Pi 4B";
   static const char* s_szBoardTypeOpenIPCGoke = "OpenIPC Goke";

   if ( board_type == BOARD_TYPE_PIZERO )
      return s_szBoardTypePi0;
   if ( board_type == BOARD_TYPE_PIZEROW )
      return s_szBoardTypePi0W;
   if ( board_type == BOARD_TYPE_PIZERO2 )
      return s_szBoardTypePi02;
   if ( board_type == BOARD_TYPE_PI2B )
      return s_szBoardTypePi2B;
   if ( board_type == BOARD_TYPE_PI2BV11 )
      return s_szBoardTypePi2BV1;
   if ( board_type == BOARD_TYPE_PI2BV12 )
      return s_szBoardTypePi2BV12;
   if ( board_type == BOARD_TYPE_PI3APLUS )
      return s_szBoardTypePi3AP;
   if ( board_type == BOARD_TYPE_PI3B )
      return s_szBoardTypePi3B;
   if ( board_type == BOARD_TYPE_PI3BPLUS )
      return s_szBoardTypePi3BP;
   if ( board_type == BOARD_TYPE_PI4B )
      return s_szBoardTypePi4B;

   if ( board_type == BOARD_TYPE_OPENIPC_GOKE )
      return s_szBoardTypeOpenIPCGoke;

   return s_szBoardTypeUnknown;
}

const char* str_get_hardware_board_name_short(u32 board_type)
{
   static const char* s_szBoardSTypeUnknown = "N/A";
   static const char* s_szBoardSTypePi0 = "Pi 0";
   static const char* s_szBoardSTypePi0W = "Pi 0W";
   static const char* s_szBoardSTypePi02 = "Pi 02";
   static const char* s_szBoardSTypePi2B = "Pi 2B";
   static const char* s_szBoardSTypePi2BV1 = "Pi 2B-v1.1";
   static const char* s_szBoardSTypePi2BV12 = "Pi 2B-v1.2";
   static const char* s_szBoardSTypePi3AP = "Pi 3A+";
   static const char* s_szBoardSTypePi3B = "Pi 3B";
   static const char* s_szBoardSTypePi3BP = "Pi 3B+";
   static const char* s_szBoardSTypePi4B = "Pi 4B";

   if ( board_type == BOARD_TYPE_PIZERO )
      return s_szBoardSTypePi0;
   if ( board_type == BOARD_TYPE_PIZEROW )
      return s_szBoardSTypePi0W;
   if ( board_type == BOARD_TYPE_PIZERO2 )
      return s_szBoardSTypePi02;
   if ( board_type == BOARD_TYPE_PI2B )
      return s_szBoardSTypePi2B;
   if ( board_type == BOARD_TYPE_PI2BV11 )
      return s_szBoardSTypePi2BV1;
   if ( board_type == BOARD_TYPE_PI2BV12 )
      return s_szBoardSTypePi2BV12;
   if ( board_type == BOARD_TYPE_PI3APLUS )
      return s_szBoardSTypePi3AP;
   if ( board_type == BOARD_TYPE_PI3B )
      return s_szBoardSTypePi3B;
   if ( board_type == BOARD_TYPE_PI3BPLUS )
      return s_szBoardSTypePi3BP;
   if ( board_type == BOARD_TYPE_PI4B )
      return s_szBoardSTypePi4B;

   return s_szBoardSTypeUnknown;
}

const char* str_get_hardware_wifi_name(u32 wifi_type)
{
   static const char* s_szWiFiTypeUnknown = "N/A";
   static const char* s_szWiFiTypeA = "A";
   static const char* s_szWiFiTypeG = "G";
   static const char* s_szWiFiTypeAG = "AG";

   if ( wifi_type == WIFI_TYPE_A )
      return s_szWiFiTypeA;
   if ( wifi_type == WIFI_TYPE_G )
      return s_szWiFiTypeG;
   if ( wifi_type == WIFI_TYPE_AG )
      return s_szWiFiTypeAG;

   return s_szWiFiTypeUnknown;
}

void str_get_hardware_camera_type_string(u32 camType, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   szOutput[0] = 0;
   if ( camType == CAMERA_TYPE_CSI )
      strcat(szOutput, "Standard CSI");
   else if ( camType == CAMERA_TYPE_VEYE290 )
      strcat(szOutput, "VEYE/IMX290");
   else if ( camType == CAMERA_TYPE_VEYE307 )
      strcat(szOutput, "VEYE/IMX307");
   else if ( camType == CAMERA_TYPE_VEYE327 )
      strcat(szOutput, "VEYE/IMX327");
   else if ( camType == CAMERA_TYPE_HDMI )
      strcat(szOutput, "HDMI CSI");
   else if ( camType == CAMERA_TYPE_USB )
      strcat(szOutput, "USB");
   else if ( camType == CAMERA_TYPE_IP )
      strcat(szOutput, "IP");
   else if ( camType == CAMERA_TYPE_OPENIPC_GOKE )
      strcat(szOutput, "Goke OpenIPC");
   else if ( camType == 0 )
      strcat(szOutput, "None");
   else
      strcat(szOutput, "Unknown");
}

void str_get_supported_bands_string(u32 bands, char* szOut)
{
   if ( NULL == szOut )
      return;

   int iCount = 0;
   
   szOut[0] = 0;
   if ( bands & RADIO_HW_SUPPORTED_BAND_433 )
      strcat(szOut, "433");

   if ( bands & RADIO_HW_SUPPORTED_BAND_868 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "868");
      iCount++;
   }

   if ( bands & RADIO_HW_SUPPORTED_BAND_915 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "915");
      iCount++;
   }

   if ( 0 != szOut[0] )
      strcat(szOut, " Mhz");

   int hasGhz = 0;
   if ( bands & RADIO_HW_SUPPORTED_BAND_23 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "2.3");
      hasGhz = 1;
      iCount++;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_24 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "2.4");
      hasGhz = 1;
      iCount++;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_25 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "2.5");
      hasGhz = 1;
      iCount++;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_58 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "5.8");
      hasGhz = 1;
      iCount++;
   }

   if ( hasGhz )
      strcat(szOut, " Ghz");

   if ( iCount > 1 )
      strcat(szOut, " bands");
   else
      strcat(szOut, " band");
}


const char* str_get_radio_type_description(int typeAndDriver)
{
   static char sszNICTypeDescription[32];
   strcpy(sszNICTypeDescription, "N/A");
   typeAndDriver = typeAndDriver & 0xFF;
   if ( typeAndDriver == RADIO_TYPE_RALINK )
      strcpy(sszNICTypeDescription, "Ralink");
   if ( typeAndDriver == RADIO_TYPE_ATHEROS )
      strcpy(sszNICTypeDescription, "Atheros");
   if ( typeAndDriver == RADIO_TYPE_REALTEK )
      strcpy(sszNICTypeDescription, "Realtek");
   if ( typeAndDriver == RADIO_TYPE_MEDIATEK )
      strcpy(sszNICTypeDescription, "Mediatek");
   if ( typeAndDriver == RADIO_TYPE_SIK )
      strcpy(sszNICTypeDescription, "SiK-Radio");
   if ( typeAndDriver == RADIO_TYPE_SERIAL )
      strcpy(sszNICTypeDescription, "Serial-Radio");
   return sszNICTypeDescription;
}

const char* str_get_radio_driver_description(int typeAndDriver)
{
   static char sszNICDriverDescription[32];
   strcpy(sszNICDriverDescription, "N/A");
  
   typeAndDriver = (typeAndDriver >> 8) & 0xFF;
   if ( typeAndDriver == RADIO_HW_DRIVER_ATHEROS )
      strcpy(sszNICDriverDescription, "ath9k_htc");
   if ( typeAndDriver == RADIO_HW_DRIVER_RALINK )
      strcpy(sszNICDriverDescription, "rt2800usb");
   if ( typeAndDriver == RADIO_HW_DRIVER_MEDIATEK )
      strcpy(sszNICDriverDescription, "mt7601u");
   if ( typeAndDriver == RADIO_HW_DRIVER_REALTEK_RTL88XXAU )
      strcpy(sszNICDriverDescription, "rtl88xxau");
   if ( typeAndDriver == RADIO_HW_DRIVER_REALTEK_RTL8812AU )
      strcpy(sszNICDriverDescription, "rtl8812au");
   if ( typeAndDriver == RADIO_HW_DRIVER_REALTEK_8812AU )
      strcpy(sszNICDriverDescription, "8812au");
   if ( typeAndDriver == RADIO_HW_DRIVER_SERIAL_SIK )
      strcpy(sszNICDriverDescription, "SiK");
   if ( typeAndDriver == RADIO_HW_DRIVER_SERIAL )
      strcpy(sszNICDriverDescription, "Serial");
   return sszNICDriverDescription;
}

const char* str_get_radio_card_model_string(int cardModel)
{
   if ( cardModel < 0 )
      cardModel = -cardModel;
   static char s_szCardModelDescription[64];
   s_szCardModelDescription[0] = 0;
   strcpy(s_szCardModelDescription, "Generic");
   if ( cardModel == CARD_MODEL_TPLINK722N )        strcpy(s_szCardModelDescription, "TPLink 722N");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036NHA )   strcpy(s_szCardModelDescription, "Alfa AWUS036NHA");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036NH )    strcpy(s_szCardModelDescription, "Alfa AWUS036NH");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036ACH )   strcpy(s_szCardModelDescription, "Alfa AWUS036ACH");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036ACS )   strcpy(s_szCardModelDescription, "Alfa AWUS036ACS");
   if ( cardModel == CARD_MODEL_ASUS_AC56 )         strcpy(s_szCardModelDescription, "ASUS AC56");
   if ( cardModel == CARD_MODEL_BLUE_STICK )        strcpy(s_szCardModelDescription, "Blue Stick");
   if ( cardModel == CARD_MODEL_RTL8812AU_DUAL_ANTENNA ) strcpy(s_szCardModelDescription, "RTL8812AU DualAnt");
   if ( cardModel == CARD_MODEL_NETGEAR_A6100 )     strcpy(s_szCardModelDescription, "Netgear A6100");
   if ( cardModel == CARD_MODEL_TENDA_U12 )         strcpy(s_szCardModelDescription, "Tenda U12");
   // 10
   if ( cardModel == CARD_MODEL_RTL8812AU_LOW_POWER ) strcpy(s_szCardModelDescription, "RTL8812AU Low Power");
   if ( cardModel == CARD_MODEL_ZIPRAY )            strcpy(s_szCardModelDescription, "Zipray 1W");
   if ( cardModel == CARD_MODEL_ARCHER_T2UPLUS )    strcpy(s_szCardModelDescription, "Archer T2U Plus");
   if ( cardModel == CARD_MODEL_RTL8814AU )         strcpy(s_szCardModelDescription, "RTL8814AU");
   
   if ( cardModel == CARD_MODEL_SIK_RADIO )         strcpy(s_szCardModelDescription, "SiK-Radio");
   if ( cardModel == CARD_MODEL_SERIAL_RADIO )      strcpy(s_szCardModelDescription, "Serial-Radio");
   if ( cardModel == CARD_MODEL_SERIAL_RADIO_ELRS ) strcpy(s_szCardModelDescription, "ELRS-Radio");

   return s_szCardModelDescription;
}

const char* str_get_radio_card_model_string_short(int cardModel)
{
   if ( cardModel < 0 )
      cardModel = -cardModel;
   static char s_szCardModelDescription[64];
   s_szCardModelDescription[0] = 0;
   strcpy(s_szCardModelDescription, "Generic");
   if ( cardModel == CARD_MODEL_TPLINK722N )        strcpy(s_szCardModelDescription, "TPL-722N");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036NHA )   strcpy(s_szCardModelDescription, "AWUS036NHA");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036NH )    strcpy(s_szCardModelDescription, "AWUS036NH");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036ACH )   strcpy(s_szCardModelDescription, "AWUS036ACH");
   if ( cardModel == CARD_MODEL_ALFA_AWUS036ACS )   strcpy(s_szCardModelDescription, "AWUS036ACS");
   if ( cardModel == CARD_MODEL_ASUS_AC56 )         strcpy(s_szCardModelDescription, "AC56");
   if ( cardModel == CARD_MODEL_BLUE_STICK )        strcpy(s_szCardModelDescription, "BlueStick");
   if ( cardModel == CARD_MODEL_RTL8812AU_DUAL_ANTENNA ) strcpy(s_szCardModelDescription, "DualAnt");
   if ( cardModel == CARD_MODEL_NETGEAR_A6100 )     strcpy(s_szCardModelDescription, "A6100");
   if ( cardModel == CARD_MODEL_TENDA_U12 )         strcpy(s_szCardModelDescription, "T-U12");
   // 10
   if ( cardModel == CARD_MODEL_RTL8812AU_LOW_POWER ) strcpy(s_szCardModelDescription, "LowPow");
   if ( cardModel == CARD_MODEL_ZIPRAY )            strcpy(s_szCardModelDescription, "Zipray-1W");
   if ( cardModel == CARD_MODEL_ARCHER_T2UPLUS )    strcpy(s_szCardModelDescription, "T2U+");
   if ( cardModel == CARD_MODEL_RTL8814AU )         strcpy(s_szCardModelDescription, "RTL8814AU");

   if ( cardModel == CARD_MODEL_SIK_RADIO )         strcpy(s_szCardModelDescription, "SiK-Radio");
   if ( cardModel == CARD_MODEL_SERIAL_RADIO )      strcpy(s_szCardModelDescription, "Serial-Radio");
   if ( cardModel == CARD_MODEL_SERIAL_RADIO_ELRS ) strcpy(s_szCardModelDescription, "ELRS");

   return s_szCardModelDescription;
}

void str_get_radio_capabilities_description(u32 flags, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   szOutput[0] = 0;
   
   if ( flags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      strcat(szOutput, "[DISABLED] ");
   if ( (flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) && (flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      strcat(szOutput, "[CAN TX/RX] ");
   else if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      strcat(szOutput, "[CAN RX] ");
   else if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      strcat(szOutput, "[CAN TX] ");
   else
      strcat(szOutput, "![CAN'T RX OR TX]! ");

   if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      strcat(szOutput, "[DATA]");

   if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      strcat(szOutput, "[VIDEO]");

   if ( flags & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
      strcat(szOutput, "[HIGH CAPACITY]");
   if ( flags & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      strcat(szOutput, "[RELAY]");
}

char* str_get_radio_frame_flags_description2(u32 frameFlags)
{
   static char s_szRadioFrameFlagsDescription[256];
   s_szRadioFrameFlagsDescription[0] = 0;
   str_get_radio_frame_flags_description(frameFlags, s_szRadioFrameFlagsDescription);   
   return s_szRadioFrameFlagsDescription;
}

void str_get_radio_frame_flags_description(u32 frameFlags, char* szOutput)
{
   if ( frameFlags & 0x00FF0000 )
      strcpy(szOutput, "[MCS rates]");
   else
      strcpy(szOutput, "[LEGACY rates]");

   if ( frameFlags & RADIO_FLAGS_FRAME_TYPE_RTS )
      strcat(szOutput, " [Frames Type: RTS]");
   if ( frameFlags & RADIO_FLAGS_FRAME_TYPE_DATA_SHORT )
      strcat(szOutput, " [Frames Type: DATA_SHORT]");
   if ( frameFlags & RADIO_FLAGS_FRAME_TYPE_DATA )
      strcat(szOutput, " [Frames Type: DATA]");
   if ( !( frameFlags & RADIO_FLAGS_FRAME_TYPE_DATA) )
   if ( !( frameFlags & RADIO_FLAGS_FRAME_TYPE_DATA_SHORT) )
   if ( !( frameFlags & RADIO_FLAGS_FRAME_TYPE_RTS) )      
      strcat(szOutput, " [Frames Type: UNKNOWN]");

   if ( frameFlags & RADIO_FLAGS_SIK_ECC )
      strcat(szOutput, " [SIK_ECC]");
   if ( frameFlags & RADIO_FLAGS_SIK_LBT )
      strcat(szOutput, " [SIK_LBT]");
   if ( frameFlags & RADIO_FLAGS_SIK_MCSTR )
      strcat(szOutput, " [SIK_MCSTR]");

   if ( (frameFlags & RADIO_FLAG_HT40_VEHICLE) && (frameFlags & RADIO_FLAG_HT40_CONTROLLER) )
      strcat(szOutput, " [HT40 V/C]");
   else if ( frameFlags & RADIO_FLAG_HT40_VEHICLE )
      strcat(szOutput, " [HT40 V]");
   else if ( frameFlags & RADIO_FLAG_HT40_CONTROLLER )
      strcat(szOutput, " [HT40 C]");
   else
      strcat(szOutput, " [HT20 V/C]");

   if ( (frameFlags & RADIO_FLAG_SGI_VEHICLE) && (frameFlags & RADIO_FLAG_SGI_CONTROLLER) )
      strcat(szOutput, " [SGI V/C]");
   else if ( frameFlags & RADIO_FLAG_SGI_VEHICLE )
      strcat(szOutput, " [SGI V]");
   else if ( frameFlags & RADIO_FLAG_SGI_CONTROLLER )
      strcat(szOutput, " [SGI C]");

   if ( (frameFlags & RADIO_FLAG_STBC_VEHICLE) && (frameFlags & RADIO_FLAG_STBC_CONTROLLER) )
      strcat(szOutput, " [STBC V/C]");
   else if ( frameFlags & RADIO_FLAG_STBC_VEHICLE )
      strcat(szOutput, " [STBC V]");
   else if ( frameFlags & RADIO_FLAG_STBC_CONTROLLER )
      strcat(szOutput, " [STBC C]");
   
   if ( (frameFlags & RADIO_FLAG_LDPC_VEHICLE) && (frameFlags & RADIO_FLAG_LDPC_CONTROLLER) )
      strcat(szOutput, " [LDPC V/C]");
   else if ( frameFlags & RADIO_FLAG_LDPC_VEHICLE )
      strcat(szOutput, " [LDPC V]");
   else if ( frameFlags & RADIO_FLAG_LDPC_CONTROLLER )
      strcat(szOutput, " [LDPC C]");
}

char* str_format_video_encoding_flags(u32 uVideoEncodingFlags)
{
   static char sl_szVideoEncodingFlagsString[256];
   sl_szVideoEncodingFlagsString[0] = 0;

   if ( uVideoEncodingFlags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS )
      strcat(sl_szVideoEncodingFlagsString, " RETRANSMISSIONS_ENABLED");
   if ( uVideoEncodingFlags & ENCODING_EXTRA_FLAG_STATUS_ON_LOWER_BITRATE )
      strcat(sl_szVideoEncodingFlagsString, " IS_ON_LOWER_BITRATE");
   if ( uVideoEncodingFlags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS )
      strcat(sl_szVideoEncodingFlagsString, " ADAPTIVE_VIDEO");
   if ( uVideoEncodingFlags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO )
      strcat(sl_szVideoEncodingFlagsString, " ADAPTIVE_USE_CONTROLLER_INFO_TOO");
   if ( uVideoEncodingFlags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_GO_LOWER_ON_LINK_LOST )
      strcat(sl_szVideoEncodingFlagsString, " ADAPTIVE_GO_LOWER_ON_LINK_LOST");

   if ( uVideoEncodingFlags & ENCODING_EXTRA_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO )
      strcat(sl_szVideoEncodingFlagsString, " ADAPTIVE_USE_MEDIUM_STRENGTH");
   if ( uVideoEncodingFlags & ENCODING_EXTRA_FLAG_ENABLE_VIDEO_ADAPTIVE_QUANTIZATION )
      strcat(sl_szVideoEncodingFlagsString, " VIDEO_AUTO_QUANTISATION_ENABLED");

   return sl_szVideoEncodingFlagsString;
}

char* str_get_video_profile_name(u32 videoProfileId)
{
   static char s_szOSDSchema[32];

   strcpy(s_szOSDSchema, "NA");
   if ( videoProfileId == VIDEO_PROFILE_BEST_PERF )
      strcpy(s_szOSDSchema,"HP");
   else if ( videoProfileId == VIDEO_PROFILE_HIGH_QUALITY )
      strcpy(s_szOSDSchema,"HQ");
   else if ( videoProfileId == VIDEO_PROFILE_USER )
      strcpy(s_szOSDSchema,"USR");
   else if ( videoProfileId == VIDEO_PROFILE_MQ )
      strcpy(s_szOSDSchema,"MQ");
   else if ( videoProfileId == VIDEO_PROFILE_LQ )
      strcpy(s_szOSDSchema,"LQ");
   else if ( videoProfileId == VIDEO_PROFILE_PIP )
      strcpy(s_szOSDSchema,"PIP");
   else
      strcpy(s_szOSDSchema,"N/A");

   return s_szOSDSchema;
}

char* str_get_radio_stream_name(int iStreamId)
{
   static char s_szStreamName[32];

   strcpy(s_szStreamName, "N/A");
   if ( iStreamId == STREAM_ID_DATA )
      strcpy(s_szStreamName, "Data Stream 1");
   else if ( iStreamId < STREAM_ID_VIDEO_1 )
      sprintf(s_szStreamName, "Data Stream %d", iStreamId+1);
   else
      sprintf(s_szStreamName, "Video Stream %d", iStreamId-STREAM_ID_VIDEO_1+1);

   return s_szStreamName;
}

char* str_get_osd_screen_name(int iOSDId)
{
   static char s_szOSDScreenName[64];
   s_szOSDScreenName[0] = 0;

   if ( 0 == iOSDId )
      strcpy(s_szOSDScreenName, "Screen 1");
   if ( 1 == iOSDId )
      strcpy(s_szOSDScreenName, "Screen 2");
   if ( 2 == iOSDId )
      strcpy(s_szOSDScreenName, "Screen 3");
   if ( 3 == iOSDId )
      strcpy(s_szOSDScreenName, "Screen Lean");
   if ( 4 == iOSDId )
      strcpy(s_szOSDScreenName, "Screen Lean Extended");

   return s_szOSDScreenName;
}

char* str_get_serial_port_usage(int iSerialPortUsage)
{
   static char s_szSerialPortUsage[64];

   strcpy(s_szSerialPortUsage, "None");

   if ( iSerialPortUsage == SERIAL_PORT_USAGE_TELEMETRY )
      strcpy(s_szSerialPortUsage, "Telemetry");
   if ( iSerialPortUsage == SERIAL_PORT_USAGE_TELEMETRY_MAVLINK )
      strcpy(s_szSerialPortUsage, "Telemetry MAVLink");
   if ( iSerialPortUsage == SERIAL_PORT_USAGE_TELEMETRY_LTM )
      strcpy(s_szSerialPortUsage, "Telemetry LTM");
   if ( iSerialPortUsage == SERIAL_PORT_USAGE_MSP_OSD )
      strcpy(s_szSerialPortUsage, "MSP OSD");
   if ( iSerialPortUsage == SERIAL_PORT_USAGE_MSP_OSD_PITLAB )
      strcpy(s_szSerialPortUsage, "MSP OSD PitLab");
   if ( iSerialPortUsage == SERIAL_PORT_USAGE_DATA_LINK )
      strcpy(s_szSerialPortUsage, "Data Link");

   if ( iSerialPortUsage == SERIAL_PORT_USAGE_SIK_RADIO )
      strcpy(s_szSerialPortUsage, "SiK Radio Interface");

   if ( iSerialPortUsage == SERIAL_PORT_USAGE_SERIAL_RADIO )
      strcpy(s_szSerialPortUsage, "Serial Radio Interface");

   if ( iSerialPortUsage >= SERIAL_PORT_USAGE_CORE_PLUGIN )
      strcpy(s_szSerialPortUsage, "Core Plugin");

   return s_szSerialPortUsage;
}

char* str_get_developer_flags(u32 uDeveloperFlags)
{
   static char s_szDeveloperFlagsDesc[1024];

   s_szDeveloperFlagsDesc[0] = 0;

   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)
      strcat(s_szDeveloperFlagsDesc, " LIVE_LOG");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS)
      strcat(s_szDeveloperFlagsDesc, " LOG_ONLY_ERRORS");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE)
      strcat(s_szDeveloperFlagsDesc, " RADIO_SILENCE_FAILSAFE");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS)
      strcat(s_szDeveloperFlagsDesc, " VIDEO_LINK_STATS");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS)
      strcat(s_szDeveloperFlagsDesc, " VIDEO_LINK_GRAPHS");
   //if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_VIDEO_BITRATE_HISTORY)
   //   strcat(s_szDeveloperFlagsDesc, " VIDEO_BITRATE_HISTORY");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_DISABLE_VIDEO_OVERLOAD_CHECK)
      strcat(s_szDeveloperFlagsDesc, " VIDEO_OVERLOAD_CHECK");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP)
      strcat(s_szDeveloperFlagsDesc, " SEND_VEHICLE_TX_GAP");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_VIDEO_FAULTS)
      strcat(s_szDeveloperFlagsDesc, " INJECT_VIDEO_FAULTS");
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_INJECT_RECOVERABLE_VIDEO_FAULTS)
      strcat(s_szDeveloperFlagsDesc, " INJECT_VIDEO_FAULTS2");
   
   return s_szDeveloperFlagsDesc;
}

char* str_get_command_response_flags_string(u32 uResponseFlags)
{
   static char s_szCommandResponseFlagsString[64];

   strcpy(s_szCommandResponseFlagsString, "[Uknown Response Flags]");

   if ( uResponseFlags == COMMAND_RESPONSE_FLAGS_OK )
      strcpy(s_szCommandResponseFlagsString, "[Ok]");
   if ( uResponseFlags == COMMAND_RESPONSE_FLAGS_FAILED )
      strcpy(s_szCommandResponseFlagsString, "[Failed]");
   if ( uResponseFlags == COMMAND_RESPONSE_FLAGS_UNKNOWN_COMMAND )
      strcpy(s_szCommandResponseFlagsString, "[Unknown Command]");
   if ( uResponseFlags == COMMAND_RESPONSE_FLAGS_FAILED_INVALID_PARAMS )
      strcpy(s_szCommandResponseFlagsString, "[Failed, Invalid Params]");
   return s_szCommandResponseFlagsString;
}

char* str_get_component_id(int iComponentId)
{
   static char s_szComponentIdString[64];
   s_szComponentIdString[0] = 0;

   if ( iComponentId == PACKET_COMPONENT_LOCAL_CONTROL )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_LOCAL_CONTROL");
   else if ( iComponentId == PACKET_COMPONENT_VIDEO )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_VIDEO");
   else if ( iComponentId == PACKET_COMPONENT_TELEMETRY )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_TELEMETRY");
   else if ( iComponentId == PACKET_COMPONENT_COMMANDS )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_COMMANDS");
   else if ( iComponentId == PACKET_COMPONENT_RC )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_RC");
   else if ( iComponentId == PACKET_COMPONENT_RUBY )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_RUBY");
   else if ( iComponentId == PACKET_COMPONENT_AUDIO )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_AUDIO");
   else if ( iComponentId == PACKET_COMPONENT_DATA )
      strcpy(s_szComponentIdString, "PACKET_COMPONENT_DATA");
   else
      strcpy(s_szComponentIdString, "INVALID");

   return s_szComponentIdString;
}

char* str_get_model_change_type(int iModelChangeType)
{
   static char s_szModelChangeTypeString[64];
   s_szModelChangeTypeString[0] = 0;

   if ( iModelChangeType == MODEL_CHANGED_GENERIC )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_GENERIC");
   else if ( iModelChangeType == MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RADIO_LINK_FRAMES_FLAGS");
   else if ( iModelChangeType == MODEL_CHANGED_FREQUENCY )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_FREQUENCY");
   else if ( iModelChangeType == MODEL_CHANGED_RADIO_LINK_CAPABILITIES )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RADIO_LINK_CAPABILITIES");
   else if ( iModelChangeType == MODEL_CHANGED_RADIO_DATARATES )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RADIO_DATARATES");
   else if ( iModelChangeType == MODEL_CHANGED_RADIO_POWERS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RADIO_POWERS");
   else if ( iModelChangeType == MODEL_CHANGED_RADIO_LINK_PARAMS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RADIO_LINK_PARAMS");
   else if ( iModelChangeType == MODEL_CHANGED_ADAPTIVE_VIDEO_FLAGS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_ADAPTIVE_VIDEO_FLAGS");
   else if ( iModelChangeType == MODEL_CHANGED_EC_SCHEME )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_EC_SCHEME");
   else if ( iModelChangeType == MODEL_CHANGED_CAMERA_PARAMS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_CAMERA_PARAMS");
   else if ( iModelChangeType == MODEL_CHANGED_STATS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_STATS");
   else if ( iModelChangeType == MODEL_CHANGED_VIDEO_BITRATE )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_VIDEO_BITRATE");
   else if ( iModelChangeType == MODEL_CHANGED_USER_SELECTED_VIDEO_PROFILE )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_USER_SELECTED_VIDEO_PROFILE");
   else if ( iModelChangeType == MODEL_CHANGED_VIDEO_H264_QUANTIZATION )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_VIDEO_H264_QUANTIZATION");

   else if ( iModelChangeType == MODEL_CHANGED_DEFAULT_MAX_ADATIVE_KEYFRAME )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_DEFAULT_MAX_ADATIVE_KEYFRAME");
   else if ( iModelChangeType == MODEL_CHANGED_VIDEO_KEYFRAME )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_VIDEO_KEYFRAME");
   else if ( iModelChangeType == MODEL_CHANGED_SWAPED_RADIO_INTERFACES )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_SWAPED_RADIO_INTERFACES");
   else if ( iModelChangeType == MODEL_CHANGED_CONTROLLER_TELEMETRY )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_CONTROLLER_TELEMETRY");
   else if ( iModelChangeType == MODEL_CHANGED_RELAY_PARAMS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RELAY_PARAMS");
   else if ( iModelChangeType == MODEL_CHANGED_SERIAL_PORTS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_SERIAL_PORTS");
   else if ( iModelChangeType == MODEL_CHANGED_OSD_PARAMS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_OSD_PARAMS");
   else if ( iModelChangeType == MODEL_CHANGED_SYNCHRONISED_SETTINGS_FROM_VEHICLE )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_SYNCHRONISED_SETTINGS_FROM_VEHICLE");
   else if ( iModelChangeType == MODEL_CHANGED_RC_PARAMS )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RC_PARAMS");
   else if ( iModelChangeType == MODEL_CHANGED_RESET_RADIO_LINK )
      strcpy(s_szModelChangeTypeString, "MODEL_CHANGED_RESET_RADIO_LINK");
   else
      strcpy(s_szModelChangeTypeString, "UNKNOWN");
   return s_szModelChangeTypeString;
}

char* str_format_relay_flags(u32 uRelayCapabilitiesFlags)
{
   static char s_szRelayFlagsDescription[128];
   s_szRelayFlagsDescription[0] = 0;

  
   if ( uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_TELEMETRY )
      strcat(s_szRelayFlagsDescription, " [TELEM]");
   if ( uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_VIDEO )
      strcat(s_szRelayFlagsDescription, " [VIDEO]");
   if ( uRelayCapabilitiesFlags & RELAY_CAPABILITY_TRANSPORT_COMMANDS )
      strcat(s_szRelayFlagsDescription, " [COMM]");
   if ( uRelayCapabilitiesFlags & RELAY_CAPABILITY_SWITCH_OSD )
      strcat(s_szRelayFlagsDescription, " [OSD-SWITCH]");
   if ( uRelayCapabilitiesFlags & RELAY_CAPABILITY_MERGE_OSD )
      strcat(s_szRelayFlagsDescription, " [OSD-MERGE]");

   if ( 0 == s_szRelayFlagsDescription[0] )
      strcpy(s_szRelayFlagsDescription, "None");

   return s_szRelayFlagsDescription;
}

char* str_format_relay_mode(u32 uRelayMode)
{
   static char s_szRelayModeDescription[128];
   s_szRelayModeDescription[0] = 0;
   if ( uRelayMode & RELAY_MODE_MAIN )
      strcat(s_szRelayModeDescription, " Main");
   if ( uRelayMode & RELAY_MODE_REMOTE )
      strcat(s_szRelayModeDescription, " Remote");
   if ( uRelayMode & RELAY_MODE_PIP_MAIN )
      strcat(s_szRelayModeDescription, " PIP-Main");
   if ( uRelayMode & RELAY_MODE_PIP_REMOTE )
      strcat(s_szRelayModeDescription, " PIP-Remote");
   if ( uRelayMode & RELAY_MODE_IS_RELAY_NODE )
      strcat(s_szRelayModeDescription, " Is-Relay-Node");
   if ( uRelayMode & RELAY_MODE_IS_RELAYED_NODE )
      strcat(s_szRelayModeDescription, " Is-Relayed-Node");
   
   if ( 0 == s_szRelayModeDescription[0] )
      strcpy(s_szRelayModeDescription, "None");
   return s_szRelayModeDescription;
}

char* str_format_firmware_type(u32 uFirmwareType)
{
   static char s_szFormatFirmwareType[32];
   s_szFormatFirmwareType[0] = 0;
 
   if ( uFirmwareType == MODEL_FIRMWARE_TYPE_RUBY )
      strcpy(s_szFormatFirmwareType, "Ruby");
   else if ( uFirmwareType == MODEL_FIRMWARE_TYPE_OPENIPC )
      strcpy(s_szFormatFirmwareType, "OpenIPC");
   else
      strcpy(s_szFormatFirmwareType, "Unknown");
   return s_szFormatFirmwareType;  
}