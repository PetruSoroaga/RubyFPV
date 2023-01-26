/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use and only if this copyright terms are preserved.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga
*/

#include "../base/base.h"
#include "../base/hardware.h"
#include "../base/hardware_radio.h"
#include "../radio/radioflags.h"
#include "string_utils.h"


void str_sanitize_modelname(char* szName)
{
   if ( NULL == szName || 0 == szName[0] )
      return;

   log_line("Sanitize model name: [%s]", szName);
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
   log_line("Sanitized model name: [%s]", szName);
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


void str_format_bitrate(u32 bitrate, char* szBuffer)
{
   if ( NULL == szBuffer )
      return;
   if ( bitrate < 1000 ) // less than 1kb
      sprintf(szBuffer, "%u bps", bitrate);
   else if ( bitrate < 10000 ) // less than 10 kb
      sprintf(szBuffer, "%.1f kbps", (float)bitrate/1000.0);
   else if ( bitrate < 500000 ) // less than 500 kb
      sprintf(szBuffer, "%u kbps", bitrate/1000);
   else
      sprintf(szBuffer, "%.1f Mbps", (float)bitrate/1000.0/1000.0);
}

void str_format_bitrate_no_sufix(u32 bitrate, char* szBuffer)
{
   if ( NULL == szBuffer )
      return;
   if ( bitrate < 1000 ) // less than 1kb
      sprintf(szBuffer, "%u", bitrate);
   else if ( bitrate < 10000 ) // less than 10 kb
      sprintf(szBuffer, "%.1f", (float)bitrate/1000.0);
   else if ( bitrate < 500000 ) // less than 500 kb
      sprintf(szBuffer, "%u", bitrate/1000);
   else
      sprintf(szBuffer, "%.1f", (float)bitrate/1000.0/1000.0);
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

   return s_szBoardTypeUnknown;
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
   else if ( camType == 0 )
      strcat(szOutput, "None");
   else
      strcat(szOutput, "Unknown");
}

void str_get_supported_bands_string(int bands, char* szOut)
{
   if ( NULL == szOut )
      return;

   szOut[0] = 0;
   if ( bands & RADIO_HW_SUPPORTED_BAND_433 )
      strcat(szOut, "433");

   if ( bands & RADIO_HW_SUPPORTED_BAND_868 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "868");
   }

   if ( bands & RADIO_HW_SUPPORTED_BAND_915 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "915");
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
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_24 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "2.4");
      hasGhz = 1;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_25 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "2.5");
      hasGhz = 1;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_58 )
   {
      if ( 0 != szOut[0] )
         strcat(szOut, ", ");
      strcat(szOut, "5.8");
      hasGhz = 1;
   }

   if ( hasGhz )
      strcat(szOut, " Ghz");
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
   if ( cardModel == CARD_MODEL_ASUS_AC56 )         strcpy(s_szCardModelDescription, "ASUS AC56");
   if ( cardModel == CARD_MODEL_BLUE_STICK )        strcpy(s_szCardModelDescription, "Blue Stick");
   if ( cardModel == CARD_MODEL_RTL8812AU_DUAL_ANTENNA ) strcpy(s_szCardModelDescription, "RTL8812AU DualAnt");
   if ( cardModel == CARD_MODEL_NETGEAR_A6100 )     strcpy(s_szCardModelDescription, "Netgear A6100");
   if ( cardModel == CARD_MODEL_TENDA_U12 )         strcpy(s_szCardModelDescription, "Tenda U12");
   // 10
   if ( cardModel == CARD_MODEL_RTL8812AU_LOW_POWER ) strcpy(s_szCardModelDescription, "RTL8812AU Low Power");
   if ( cardModel == CARD_MODEL_ZIPRAY )            strcpy(s_szCardModelDescription, "Zipray 1W");

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
   if ( cardModel == CARD_MODEL_ASUS_AC56 )         strcpy(s_szCardModelDescription, "AC56");
   if ( cardModel == CARD_MODEL_BLUE_STICK )        strcpy(s_szCardModelDescription, "BlueStick");
   if ( cardModel == CARD_MODEL_RTL8812AU_DUAL_ANTENNA ) strcpy(s_szCardModelDescription, "DualAnt");
   if ( cardModel == CARD_MODEL_NETGEAR_A6100 )     strcpy(s_szCardModelDescription, "A6100");
   if ( cardModel == CARD_MODEL_TENDA_U12 )         strcpy(s_szCardModelDescription, "T-U12");
   // 10
   if ( cardModel == CARD_MODEL_RTL8812AU_LOW_POWER ) strcpy(s_szCardModelDescription, "LowPow");
   if ( cardModel == CARD_MODEL_ZIPRAY )            strcpy(s_szCardModelDescription, "Zipray-1W");

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
      strcat(szOutput, "[CAN RX]");
   else if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      strcat(szOutput, "[CAN TX]");
   else
      strcat(szOutput, "![CAN'T RX OR TX]!");

   if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      strcat(szOutput, "[DATA]");

   if ( flags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      strcat(szOutput, "[VIDEO]");

   if ( flags & RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA )
      strcat(szOutput, "[LOWEST_DATA]");
}


void str_get_radio_frame_flags_description(u32 frameFlags, char* szOutput)
{
   if ( frameFlags & RADIO_FLAGS_ENABLE_MCS_FLAGS )
      strcpy(szOutput, "[MCS rates and flags]");
   else
      strcpy(szOutput, "[LEGACY rates no flags]");

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

   if ( frameFlags & RADIO_FLAGS_HT20 )
      strcat(szOutput, " [HT20]");
   if ( frameFlags & RADIO_FLAGS_HT40 )
      strcat(szOutput, " [HT40]");

   if ( frameFlags & RADIO_FLAGS_LDPC )
      strcat(szOutput, " [LDPC]");
   if ( frameFlags & RADIO_FLAGS_SHORT_GI )
      strcat(szOutput, " [SGI]");
   if ( frameFlags & RADIO_FLAGS_STBC )
      strcat(szOutput, " [STBC]");

   if ( (frameFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE) && (frameFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER) )
      strcat(szOutput, " [Apply On Both]");
   else if ( frameFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE )
      strcat(szOutput, " [Apply On Vehicle]");
   else
      strcat(szOutput, " [Apply On Controller]");
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

   if ( iStreamId == STREAM_ID_DATA )
      strcpy(s_szStreamName, "Data Stream");
   else if ( iStreamId < STREAM_ID_VIDEO_1 )
      sprintf(s_szStreamName, "Data Stream %d", iStreamId+2);
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
   if ( uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_VIDEO_BITRATE_HISTORY)
      strcat(s_szDeveloperFlagsDesc, " VIDEO_BITRATE_HISTORY");
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