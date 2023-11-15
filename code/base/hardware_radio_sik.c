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
#include "../common/string_utils.h"
#include "hardware_radio.h"
#include "hardware_radio_sik.h"
#include "hardware_serial.h"

static radio_hw_info_t s_SiKRadioLastKnownInfo[MAX_RADIO_INTERFACES];
static int s_iSiKRadioLastKnownCount = 0;
static int s_iSiKRadioCount = 0;

int hardware_radio_has_sik_radios()
{
   if ( s_iSiKRadioCount > 0 )
      return s_iSiKRadioCount;
   return 0;
}

int hardware_radio_index_is_sik_radio(int iHWInterfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iHWInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return 0;
     
   return hardware_radio_is_sik_radio(pRadioHWInfo);
}

int hardware_radio_is_sik_radio(radio_hw_info_t* pRadioInfo)
{
   if ( NULL == pRadioInfo )
      return 0;

   u32 uType = pRadioInfo->typeAndDriver & 0xFF;
   if ( uType == RADIO_TYPE_SIK )
      return 1;

   return 0;
}

radio_hw_info_t* hardware_radio_sik_get_from_serial_port(const char* szSerialPort)
{
   if ( (NULL == szSerialPort) || (0 == szSerialPort[0]) )
      return NULL;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
         continue;
      if ( 0 == pRadioInfo->szDriver[0] )
         continue;
      if ( 0 != strcmp(pRadioInfo->szDriver, szSerialPort) )
         continue;
      return pRadioInfo;
   }
   return NULL;
}


int _hardware_radio_sik_enter_command_mode(int iSerialPortFile, shared_mem_process_stats* pProcessStats)
{
   if ( iSerialPortFile <= 0 )
      return 0;

   // First, empty the receiving buffers for any accumulated pending data in it.
   
   u8 bufferResponse[2024];
   int iBufferSize = 2023;
   
   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = 100;

   fd_set readset;   
   FD_ZERO(&readset);
   FD_SET(iSerialPortFile, &readset);
   
   int res = select(iSerialPortFile+1, &readset, NULL, NULL, &to);
   if ( res > 0 )
   if ( FD_ISSET(iSerialPortFile, &readset) )
   {
      int iRead = read(iSerialPortFile, bufferResponse, iBufferSize-1);
      log_line("[HardwareRadio] SiK flushed Rx buffer first. %d bytes flushed.", iRead);
   }

   // Send command

   if ( ! hardware_serial_send_sik_command(iSerialPortFile, "+++") )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to send to SiK radio AT command mode change.");
      return 0;
   }
   
   log_line("[HardwareRadio] Sent SiK command to enter AT command mode...");

   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   // Wait for "OK\n" response
   u32 uTimeStart = get_current_timestamp_ms();
   u32 uTimeEnd = uTimeStart + 3000;
   int iFoundResponse = 0;
   int iBufferPos = 0;
   
   memset(bufferResponse, 0, 2024);

   while ( ! iFoundResponse )
   {
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
      
      to.tv_sec = 0;
      to.tv_usec = (uTimeEnd-uTimeStart)*1000;

      FD_ZERO(&readset);
      FD_SET(iSerialPortFile, &readset);
      
      res = select(iSerialPortFile+1, &readset, NULL, NULL, &to);
      if ( res > 0 )
      if ( FD_ISSET(iSerialPortFile, &readset) && (iBufferPos < iBufferSize) )
      {
         int iRead = read(iSerialPortFile, (u8*)&(bufferResponse[iBufferPos]), iBufferSize-iBufferPos);
         if ( iRead > 0 )
         {
            iBufferPos += iRead;

            for( int i=0;i<iBufferPos-2; i++ )
            {
               if ( bufferResponse[i] == 'O' )
               if ( bufferResponse[i+1] == 'K' )
               if ( (bufferResponse[i+2] == 10) || (bufferResponse[i+2] == 13) )
               {
                  iFoundResponse = 1;
                  break;
               }
            }
         }
      }
      u32 uTimeStart = get_current_timestamp_ms();
      if ( uTimeStart >= uTimeEnd )
         break;
   }

   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   if ( ! iFoundResponse )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to enter SiK radio into command mode (received %d bytes in response to enter AT mode, but not Ok response.", iBufferPos);
      return 0;
   }

   
   log_line("[HardwareRadio] SiK radio entered in AT command mode.");
   return 1;
}

int _hardware_radio_sik_get_all_params(radio_hw_info_t* pRadioInfo, int iSerialPortFile, shared_mem_process_stats* pProcessStats, int iComputeMAC)
{
   if ( NULL == pRadioInfo || iSerialPortFile <= 0 )
      return 0;

   if ( ! _hardware_radio_sik_enter_command_mode(iSerialPortFile, pProcessStats) )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to enter SiK radio into AT command mode.");
      return 0;
   }
   
   log_line("[HardwareRadio] Getting SiK radio info...");
   
   u8 bufferResponse[1024];

   if ( hardware_radio_sik_send_command(iSerialPortFile, "ATI", bufferResponse, 255) )
   {
      bufferResponse[62] = 0;
      strcpy(pRadioInfo->szDescription, (char*)bufferResponse);
   }
   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   if ( 1 == iComputeMAC )
   {
      char szMAC[256];
      szMAC[0] = 0;

      if ( hardware_radio_sik_send_command(iSerialPortFile, "ATI1", bufferResponse, 255) )
         strlcat(szMAC, (char*)bufferResponse, sizeof(szMAC));
      else
         strlcat(szMAC, "X", sizeof(szMAC));
      strlcat(szMAC, "-", sizeof(szMAC));
      if ( hardware_radio_sik_send_command(iSerialPortFile, "ATI2", bufferResponse, 255) )
         strlcat(szMAC, (char*)bufferResponse, sizeof(szMAC));
      else
         strlcat(szMAC, "X", sizeof(szMAC));
      strlcat(szMAC, "-", sizeof(szMAC));

      if ( hardware_radio_sik_send_command(iSerialPortFile, "ATI3", bufferResponse, 255) )
         strlcat(szMAC, (char*)bufferResponse, sizeof(szMAC));
      else
         strlcat(szMAC, "X", sizeof(szMAC));
      strlcat(szMAC, "-", sizeof(szMAC));

      if ( hardware_radio_sik_send_command(iSerialPortFile, "ATI4", bufferResponse, 255) )
         strlcat(szMAC, (char*)bufferResponse, sizeof(szMAC));
      else
         strlcat(szMAC, "X", sizeof(szMAC));
      strlcat(szMAC, "-", sizeof(szMAC));

      if ( hardware_radio_sik_send_command(iSerialPortFile, "ATS8?", bufferResponse, 255) )
      {
         u32 uFreq = (u32)atoi((char*)bufferResponse);
         if ( uFreq < 500000 )
            strlcat(szMAC, "433", sizeof(szMAC));
         else if ( uFreq < 890000 )
            strlcat(szMAC, "868", sizeof(szMAC));
         else
            strlcat(szMAC, "915", sizeof(szMAC));
      }
      else
         strlcat(szMAC, "NNN", sizeof(szMAC));

      strlcpy(pRadioInfo->szMAC, szMAC, MAX_MAC_LENGTH);
      pRadioInfo->szMAC[MAX_MAC_LENGTH-1] = 0;
      log_line("[HardwareRadio] Computed SiK radio MAC: [%s]", pRadioInfo->szMAC);
   }

   // Get parameters 0 to 15
   
   int iMaxParam = 16;
   if ( iMaxParam > MAX_RADIO_HW_PARAMS )
      iMaxParam = MAX_RADIO_HW_PARAMS;

   pRadioInfo->uHardwareParamsList[0] = MAX_U32;

   for( int i=0; i<iMaxParam; i++ )
   {
      pRadioInfo->uHardwareParamsList[i] = MAX_U32;
      char szComm[32];
      snprintf(szComm, sizeof(szComm), "ATS%d?", i);
      if ( hardware_radio_sik_send_command(iSerialPortFile, szComm, bufferResponse, 255) )
      {
         pRadioInfo->uHardwareParamsList[i] = atoi((char*)bufferResponse);
      }
      else
         pRadioInfo->uHardwareParamsList[i] = MAX_U32;
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }

   // Exit AT command mode
   hardware_radio_sik_send_command(iSerialPortFile, "ATO", bufferResponse, 255);

   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();
   return 1;
}

int hardware_radio_sik_detect_interfaces()
{
   log_line("[HardwareRadio]: Enumerating Sik radios...");
   
   s_iSiKRadioCount = 0;
   s_iSiKRadioLastKnownCount = 0;

   hardware_radio_sik_load_configuration();

   s_iSiKRadioCount = 0;

   radio_hw_info_t* pRadioInfo = hardware_radio_sik_try_detect_on_port("ttyUSB0");
   if ( (NULL != pRadioInfo) && ( hardware_get_radio_interfaces_count() < MAX_RADIO_INTERFACES ) )
   {
      pRadioInfo->phy_index = 0;
      strcpy(pRadioInfo->szUSBPort, "TU-0");
      hardware_add_radio_interface_info(pRadioInfo);
   }

   pRadioInfo = hardware_radio_sik_try_detect_on_port("ttyUSB1");
   if ( (NULL != pRadioInfo) && ( hardware_get_radio_interfaces_count() < MAX_RADIO_INTERFACES ) )
   {
      pRadioInfo->phy_index = 0;
      strcpy(pRadioInfo->szUSBPort, "TU-0");
      hardware_add_radio_interface_info(pRadioInfo);
   }

   pRadioInfo = hardware_radio_sik_try_detect_on_port("ttyUSB2");
   if ( (NULL != pRadioInfo) && ( hardware_get_radio_interfaces_count() < MAX_RADIO_INTERFACES ) )
   {
      pRadioInfo->phy_index = 0;
      strcpy(pRadioInfo->szUSBPort, "TU-0");
      hardware_add_radio_interface_info(pRadioInfo);
   }

   s_iSiKRadioCount = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      pRadioInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioInfo) || ((pRadioInfo->typeAndDriver & 0xFF) != RADIO_TYPE_SIK) )
         continue;
      s_iSiKRadioCount++;
   }

   log_line("[HardwareRadio]: Found %d SiK radios.", s_iSiKRadioCount);

   return hardware_radio_sik_save_configuration();
}

int hardware_radio_sik_save_configuration()
{
   s_iSiKRadioCount = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioInfo) || ((pRadioInfo->typeAndDriver & 0xFF) != RADIO_TYPE_SIK) )
         continue;
      s_iSiKRadioCount++;
   }

   log_line("[HardwareRadio]: Saving current SiK radios configuration (%d SiK radio interfaces).", s_iSiKRadioCount);
   FILE* fd = fopen(FILE_CONFIG_LAST_SIK_RADIOS_DETECTED, "wb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[HardwareRadio]: Failed to write last good SiK radio configuration to file: [%s]", FILE_CONFIG_LAST_SIK_RADIOS_DETECTED);
      return 0;
   }

   fwrite((u8*)&s_iSiKRadioCount, 1, sizeof(int), fd);

   int iCountWrite = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioInfo) || ((pRadioInfo->typeAndDriver & 0xFF) != RADIO_TYPE_SIK) )
         continue;
       fwrite( (u8*)pRadioInfo, 1, sizeof(radio_hw_info_t), fd );

      char szBuff[256];
      snprintf(szBuff, sizeof(szBuff), "* SiK radio %d of %d on [%s], id: [%s]", iCountWrite+1, s_iSiKRadioCount, pRadioInfo->szDriver, pRadioInfo->szMAC);
      log_line(szBuff);
      iCountWrite++;
   }
   fclose(fd);

   log_line("[HardwareRadio]: Saved current SiK radios configuration to file [%s]:", FILE_CONFIG_LAST_SIK_RADIOS_DETECTED);
   return 1;
}


int hardware_radio_sik_load_configuration()
{
   s_iSiKRadioCount = 0;
   s_iSiKRadioLastKnownCount = 0;

   if ( access( FILE_CONFIG_LAST_SIK_RADIOS_DETECTED, R_OK ) == -1 )
   {
      log_line("[HardwareRadio]: No last known good SiK radio configuration file. Nothing to load.");
      return 0;
   }
   log_line("[HardwareRadio]: Try to load last known good SiK radio configuration from file [%s].", FILE_CONFIG_LAST_SIK_RADIOS_DETECTED);

   FILE* fd = fopen(FILE_CONFIG_LAST_SIK_RADIOS_DETECTED, "rb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("[HardwareRadio]: Failed to load last known good SiK radio configuration from file [%s].", FILE_CONFIG_LAST_SIK_RADIOS_DETECTED);
      return 0;
   }

   if ( sizeof(int) != fread((u8*)&s_iSiKRadioCount, 1, sizeof(int), fd) )
   {
      fclose(fd);
      s_iSiKRadioCount = 0;
      s_iSiKRadioLastKnownCount = 0;
      log_softerror_and_alarm("[HardwareRadio]: Failed to load last known good SiK radio configuration from file [%s]. Invalid file format.", FILE_CONFIG_LAST_SIK_RADIOS_DETECTED);
      return 0;
   }

   if ( s_iSiKRadioCount < 0 || s_iSiKRadioCount >= MAX_RADIO_INTERFACES )
   {
      fclose(fd);
      s_iSiKRadioCount = 0;
      s_iSiKRadioLastKnownCount = 0;
      log_softerror_and_alarm("[HardwareRadio]: Failed to load last known good SiK radio configuration from file [%s]. Invalid file format (2).", FILE_CONFIG_LAST_SIK_RADIOS_DETECTED);
      return 0;
   }

   for( int i=0; i<s_iSiKRadioCount; i++ )
   {
      if ( sizeof(radio_hw_info_t) != fread((u8*)&(s_SiKRadioLastKnownInfo[i]), 1, sizeof(radio_hw_info_t), fd) )
      {
         fclose(fd);
         s_iSiKRadioCount = 0;
         s_iSiKRadioLastKnownCount = 0;
         log_softerror_and_alarm("[HardwareRadio]: Failed to load last known good SiK radio configuration from file [%s]. Invalid file format (3/%d).", FILE_CONFIG_LAST_SIK_RADIOS_DETECTED, i);
         return 0;
      }
   }
   fclose(fd);
   log_line("[HardwareRadio]: Loaded last known good SiK radio configuration from file [%s]. %d SiK radios loaded:", FILE_CONFIG_LAST_SIK_RADIOS_DETECTED, s_iSiKRadioCount);

   for( int i=0; i<s_iSiKRadioCount; i++ )
   {
      char szBuff[256];
      snprintf(szBuff, sizeof(szBuff), "* SiK radio %d of %d on [%s], id: [%s]", i+1, s_iSiKRadioCount, s_SiKRadioLastKnownInfo[i].szDriver, s_SiKRadioLastKnownInfo[i].szMAC);
      log_line(szBuff);
   }
   s_iSiKRadioLastKnownCount = s_iSiKRadioCount;
   return 1;
}


radio_hw_info_t* hardware_radio_sik_try_detect_on_port(const char* szSerialPort)
{
   if ( NULL == szSerialPort || 0 == szSerialPort[0] )
      return NULL;

   char szDevName[128];
   strcpy(szDevName, szSerialPort);
   if ( 0 == strstr(szSerialPort, "/dev") )
      snprintf(szDevName, sizeof(szDevName), "/dev/%s", szSerialPort);

   log_line("[HardwareRadio]: Try to find SiK radio on interface [%s]...", szDevName);
   if ( access( szDevName, R_OK ) == -1 )
   {
      log_line("[HardwareRadio]: Device [%s] is not present.", szDevName);
      return NULL;
   }
   
   int iBaudRatesList[64];
   int iBaudRatesCount = 0;

   // Try to find it in the last good known configuration
   for( int i=0; i<s_iSiKRadioLastKnownCount; i++ )
   {
      if ( 0 != strcmp(s_SiKRadioLastKnownInfo[i].szDriver, szDevName) )
         continue;
      iBaudRatesList[0] = hardware_radio_sik_get_real_serial_baudrate(s_SiKRadioLastKnownInfo[i].uHardwareParamsList[1]);
      log_line("[HardwareRadio]: Found existing configuration for SiK radio on [%s] at %u bps. Try it first.", szDevName, iBaudRatesList[0]);
      iBaudRatesCount++;
      break;
   }

   for( int i=hardware_get_serial_baud_rates_count()-1; i>0; i-- )
   {
      if ( hardware_get_serial_baud_rates()[i] < 57000 )
         break;
      iBaudRatesList[iBaudRatesCount] = hardware_get_serial_baud_rates()[i];
      iBaudRatesCount++;
   }

   for( int i=0; i<iBaudRatesCount; i++ )
   {
      int iSpeed = iBaudRatesList[i];
      int iSerialPort = hardware_open_serial_port(szDevName, iSpeed);
      if ( iSerialPort <= 0 )
         continue;

      static radio_hw_info_t s_radioHWInfoSikTemp;
      memset((u8*)&s_radioHWInfoSikTemp, 0, sizeof(radio_hw_info_t));
      
      if ( ! _hardware_radio_sik_get_all_params(&s_radioHWInfoSikTemp, iSerialPort, NULL, 1) )
      {
         close(iSerialPort);
         continue;
      }

      strlcat(s_radioHWInfoSikTemp.szMAC, "-", sizeof(s_radioHWInfoSikTemp.szMAC));
      strlcat(s_radioHWInfoSikTemp.szMAC, szSerialPort + (strlen(szSerialPort)-1), sizeof(s_radioHWInfoSikTemp.szMAC));
      log_line("[HardwareRadio]: Found SiK Radio on port %s, baud: %d, MAC: [%s]", szDevName, iSpeed, s_radioHWInfoSikTemp.szMAC);
      char szTmp[256];
      int len = 0;
      szTmp[0] = 0;
      for( int i=0; i<16; i++ )
      {
         if ( 0 != szTmp[0] )
            len = strlcat(szTmp, ", ", sizeof(szTmp));

         len += snprintf(szTmp+len, sizeof(szTmp)-len, "[%d]=%u", i, s_radioHWInfoSikTemp.uHardwareParamsList[i]);
      }
      log_line("[HardwareRadio]: SiK current parameters: %s", szTmp);

      // Found SiK device that answered to AT commands mode.
      // Populate info about device
      
      close(iSerialPort);

      s_radioHWInfoSikTemp.typeAndDriver = RADIO_TYPE_SIK | (RADIO_HW_DRIVER_SERIAL_SIK<<8);
      s_radioHWInfoSikTemp.iCardModel = CARD_MODEL_SIK_RADIO;
      s_radioHWInfoSikTemp.isSupported = 1;
      s_radioHWInfoSikTemp.isHighCapacityInterface = 0;
      s_radioHWInfoSikTemp.isEnabled = 1;
      s_radioHWInfoSikTemp.isTxCapable = 1;

      strcpy(s_radioHWInfoSikTemp.szName, "SiK Radio");
      strcpy(s_radioHWInfoSikTemp.szUSBPort, "T");
      strcpy(s_radioHWInfoSikTemp.szDriver, szDevName);
      sprintf(s_radioHWInfoSikTemp.szProductId, "%u", s_radioHWInfoSikTemp.uHardwareParamsList[0]);
      
      s_radioHWInfoSikTemp.uCurrentFrequency = s_radioHWInfoSikTemp.uHardwareParamsList[8];
      if ( s_radioHWInfoSikTemp.uHardwareParamsList[8] >= 400000 )
      if ( s_radioHWInfoSikTemp.uHardwareParamsList[9] <= 460000 )
         s_radioHWInfoSikTemp.supportedBands = RADIO_HW_SUPPORTED_BAND_433;

      if ( s_radioHWInfoSikTemp.uHardwareParamsList[8] >= 800000 )
      if ( s_radioHWInfoSikTemp.uHardwareParamsList[9] <= 890000 )
         s_radioHWInfoSikTemp.supportedBands = RADIO_HW_SUPPORTED_BAND_868;

      if ( s_radioHWInfoSikTemp.uHardwareParamsList[8] >= 891000 )
      if ( s_radioHWInfoSikTemp.uHardwareParamsList[9] <= 950000 )
         s_radioHWInfoSikTemp.supportedBands = RADIO_HW_SUPPORTED_BAND_915;


      // Mark the serial port as used for a hardware radio interface

      log_line("[HardwareRadio]: Mark serial device [%s] as used for a radio interface.", szDevName);
      
      for( int k=0; k<hardware_get_serial_ports_count(); k++ )
      {
         hw_serial_port_info_t* pSerial = hardware_get_serial_port_info(k);
         if ( NULL == pSerial )
            continue;
         if ( 0 == strcmp(pSerial->szPortDeviceName, szDevName) )
         {
            pSerial->iPortUsage = SERIAL_PORT_USAGE_HARDWARE_RADIO;
            pSerial->lPortSpeed = iSpeed;
            pSerial->iSupported = 1;
            hardware_serial_save_configuration();
            break;
         }
      }
      return &s_radioHWInfoSikTemp;
   }

   return NULL; 
}

int hardware_radio_sik_send_command(int iSerialPortFile, const char* szCommand, u8* bufferResponse, int iMaxResponseLength)
{
   if ( iSerialPortFile <= 0 || szCommand == NULL || 0 == szCommand[0] )
   {
      log_softerror_and_alarm("[HardwareRadio] Tried to send SiK command to invalid serial port file handle: %d, command: [%s]", iSerialPortFile, (szCommand == NULL)?"NULL":szCommand);
      return 0;
   }

   if ( NULL == bufferResponse || iMaxResponseLength <= 2 )
   {
      log_softerror_and_alarm("[HardwareRadio] Tried to send command with no response buffer.");
      return 0;
   }

   if ( 0 == hardware_serial_send_sik_command(iSerialPortFile, szCommand) )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to send SiK command to serial port.");
      return 0;
   }
   
   log_line("[HardwareRadio] Sent SiK radio command: [%s]...", szCommand);

   u8 buff[256];
   int iBufferSize = 255;
   int iLines = 2;
   if ( 0 == strcmp(szCommand, "ATO") )
      iLines = 1;
   if ( 0 == strcmp(szCommand, "ATZ") )
      iLines = 1;

   int iLen = hardware_serial_wait_sik_response(iSerialPortFile, 2000, iLines, buff, &iBufferSize);
   
   if ( iLen <= 2 )
   {
      log_softerror_and_alarm("[HardwareRadio] Received invalid SiK radio response, length: %d bytes", iLen);
      return 0;
   }

   // Remove trailing \r\n
   for( int i=0; i<6; i++ )
   {
      if ( (iLen > 0) && (buff[iLen-1] == 10 || buff[iLen-1] == 13) )
      {
         buff[iLen-1] = 0;
         iLen--;
      }
   }

   // Remove prefix \r\n
   for( int i=0; i<6; i++ )
   {
      if ( (buff[0] == 10 || buff[0] == 13) )
      {
         for( int k=0; k<iLen; k++ )
            buff[k] = buff[k+1];
         buff[iLen-1] = 0;
         iLen--;
      }
   }

   // Keep only last line from response
   
   for( int i=iLen-1; i>=0; i-- )
   {
      if ( buff[i] == 10 || buff[i] == 13 )
      {
          for( int k=0; k<iLen-i-1; k++ )
             buff[k] = buff[i+k+1];
          iLen = iLen-i-1;
          buff[iLen] = 0;
          break;
      }
   }
   
   if ( iLen >= iMaxResponseLength )
      iLen = iMaxResponseLength-1;
   memcpy(bufferResponse, buff, iLen);
   bufferResponse[iLen] = 0;

   log_line("[HardwareRadio] Received Sik Command [%s] response: [%s], %d bytes", szCommand, (char*)bufferResponse, iLen);
   return 1;
}

int hardware_radio_sik_get_real_serial_baudrate(int iSiKBaudRate)
{
   if ( iSiKBaudRate == 1 )
      return 1200;
   if ( iSiKBaudRate == 2 )
      return 2400;
   if ( iSiKBaudRate == 4 )
      return 4800;
   if ( iSiKBaudRate == 8 )
      return 8000;
   if ( iSiKBaudRate == 9 )
      return 9600;
   if ( iSiKBaudRate == 16 )
      return 16000;
   if ( iSiKBaudRate == 19 )
      return 19200;
   if ( iSiKBaudRate == 24 )
      return 24000;
   if ( iSiKBaudRate == 32 )
      return 32000;
   if ( iSiKBaudRate == 38 )
      return 38400;
   if ( iSiKBaudRate == 48 )
      return 48000;
   if ( iSiKBaudRate == 57 )
      return 57600;
   if ( iSiKBaudRate == 64 )
      return 64000;
   if ( iSiKBaudRate == 96 )
      return 96000;
   
   if ( iSiKBaudRate == 111 )
      return 111200;
   if ( iSiKBaudRate == 115 )
      return 115200;

   if ( iSiKBaudRate == 128 )
      return 128000;
 
   if ( iSiKBaudRate == 250 )
      return 250000;
   
   return 57600;
}

int hardware_radio_sik_get_encoded_serial_baudrate(int iRealSerialBaudRate)
{
   if ( iRealSerialBaudRate <= 1200 )
      return 1;
   if ( iRealSerialBaudRate <= 2400 )
      return 2;
   if ( iRealSerialBaudRate <= 4800 )
      return 4;
   if ( iRealSerialBaudRate <= 8000 )
      return 8;
   if ( iRealSerialBaudRate <= 9600 )
      return 9;
   if ( iRealSerialBaudRate <= 16000 )
      return 16;
   if ( iRealSerialBaudRate <= 19200 )
      return 19;
   if ( iRealSerialBaudRate <= 24000 )
      return 24;
   if ( iRealSerialBaudRate <= 32000 )
      return 32;
   if ( iRealSerialBaudRate <= 38400 )
      return 38;
   if ( iRealSerialBaudRate <= 48000 )
      return 48;
   if ( iRealSerialBaudRate <= 57600 )
      return 57;
   if ( iRealSerialBaudRate <= 64000 )
      return 64;
   if ( iRealSerialBaudRate <= 96000 )
      return 96;
   if ( iRealSerialBaudRate <= 111200 )
      return 111;
   if ( iRealSerialBaudRate <= 115200 )
      return 115;
   if ( iRealSerialBaudRate <= 128000 )
      return 128;
   if ( iRealSerialBaudRate <= 250000 )
      return 250;

   return 57;
}

int hardware_radio_sik_get_all_params(radio_hw_info_t* pRadioInfo, shared_mem_process_stats* pProcessStats)
{
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
      return 0;

   hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioInfo->szDriver);
  
   if ( NULL == pSerialPort )
   {
      log_error_and_alarm("[HardwareRadio] Failed to find serial port configuration for SiK radio %s.", pRadioInfo->szDriver);
      return 0;
   }

   int iSerialPort = hardware_open_serial_port(pRadioInfo->szDriver, pSerialPort->lPortSpeed);
   if ( iSerialPort <= 0 )
   {
      log_error_and_alarm("[HardwareRadio] Failed to open serial port for SiK radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      return 0;
   }

   if ( ! _hardware_radio_sik_get_all_params(pRadioInfo, iSerialPort, pProcessStats, 0) )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to get all params for SiK radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      close(iSerialPort);
      return 0;
   }

   char szTmp[256];
   int len = 0;
   szTmp[0] = 0;
   for( int i=0; i<16; i++ )
   {
      if ( 0 != szTmp[0] )
         len = strlcat(szTmp, ", ", sizeof(szTmp));

      len += snprintf(szTmp+len, sizeof(szTmp)-len, "[%d]=%u", i, pRadioInfo->uHardwareParamsList[i]);
   }
   log_line("[HardwareRadio]: SiK interface [%s] current parameters: %s", pRadioInfo->szDriver, szTmp);
   
   close(iSerialPort);
   return 1;
}

int _hardware_radio_sik_save_settings_to_flash( int iSerialPortFile )
{
   if ( iSerialPortFile <= 0 )
      return 0;

   u8 bufferResponse[256];

   if ( hardware_radio_sik_send_command(iSerialPortFile, "AT&W", bufferResponse, 255) )
      log_line("[HardwareRadio]: Wrote SiK parameters to flash, response: [%s]", bufferResponse);
   else
      log_softerror_and_alarm("[HardwareRadio]: Failed to write SiK parameters to flash, response: [%s]", bufferResponse);

   if ( hardware_radio_sik_send_command(iSerialPortFile, "ATZ", bufferResponse, 255) )
      log_line("[HardwareRadio]: Restart SiK radio, response: [%s]", bufferResponse);
   else
      log_softerror_and_alarm("[HardwareRadio]: Failed to restart SiK radio, response: [%s]", bufferResponse);

   return 1;
}

int _hardware_radio_sik_set_parameter( int iSerialPortFile, u32 uParamIndex, u32 uParamValue )
{
   if ( iSerialPortFile <= 0 )
      return 0;
   if ( uParamIndex >= 16 )
      return 0;

   char szComm[32];
   u8 bufferResponse[256];

   snprintf(szComm, sizeof(szComm), "ATS%u=%u", uParamIndex, uParamValue);

   if ( hardware_radio_sik_send_command(iSerialPortFile, szComm, bufferResponse, 255) )
      return 1;
   
   return 0;
}

int hardware_radio_sik_set_serial_speed(radio_hw_info_t* pRadioInfo, int iSerialSpeedToConnect, int iNewSerialSpeed, shared_mem_process_stats* pProcessStats)
{
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
      return 0;

   if ( pRadioInfo->uHardwareParamsList[1] == hardware_radio_sik_get_encoded_serial_baudrate(iNewSerialSpeed) )
   {
      log_line("[HardwareRadio]: SiK Radio serial speed is unchanged. SiK radio interface %d is already at serial speed %d bps.",
         pRadioInfo->phy_index+1, iNewSerialSpeed );
      return 1;
   }

   hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioInfo->szDriver);
   
   if ( NULL == pSerialPort )
   {
      log_error_and_alarm("[HardwareRadio] Failed to find serial port configuration for SiK radio %s.", pRadioInfo->szDriver);
      return 0;
   }

   int iSerialPort = hardware_open_serial_port(pRadioInfo->szDriver, iSerialSpeedToConnect);
   if ( iSerialPort <= 0 )
   {
      log_error_and_alarm("[HardwareRadio] Failed to open serial port for SiK radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      return 0;
   }

   // Enter AT mode
   if ( ! _hardware_radio_sik_enter_command_mode(iSerialPort, pProcessStats) )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to enter SiK radio into AT command mode.");
      close(iSerialPort);
      return 0;
   }
   
   // Send params

   if ( _hardware_radio_sik_set_parameter(iSerialPort, 1, hardware_radio_sik_get_encoded_serial_baudrate(iNewSerialSpeed)) )
      pRadioInfo->uHardwareParamsList[1] = hardware_radio_sik_get_encoded_serial_baudrate(iNewSerialSpeed);
   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   // Exit AT command mode
   
   _hardware_radio_sik_save_settings_to_flash(iSerialPort);

   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   ////////////////////
   close(iSerialPort);

   log_line("[HardwareRadio]: Did set SiK radio interface %d to serial speed %d bps;",
      pRadioInfo->phy_index+1, iNewSerialSpeed );
   return 1;
}

int hardware_radio_sik_set_frequency(radio_hw_info_t* pRadioInfo, u32 uFrequency, shared_mem_process_stats* pProcessStats)
{
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
      return 0;
   int iRes = hardware_radio_sik_set_params(pRadioInfo, uFrequency, DEFAULT_RADIO_SIK_FREQ_SPREAD, DEFAULT_RADIO_SIK_CHANNELS, DEFAULT_RADIO_SIK_NETID, DEFAULT_RADIO_SIK_AIR_SPEED, pProcessStats);
   return iRes;
}


int hardware_radio_sik_set_params(radio_hw_info_t* pRadioInfo, u32 uFrequency, u32 uFreqSpread, u32 uChannels, u32 uNetId, u32 uAirSpeed, shared_mem_process_stats* pProcessStats)
{
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
      return 0;

   int iAnyChanged = 0;

   if ( pRadioInfo->uHardwareParamsList[2] != hardware_radio_sik_get_encoded_serial_baudrate(uAirSpeed) )
      iAnyChanged = 1;
   if ( pRadioInfo->uHardwareParamsList[3] != uNetId )
      iAnyChanged = 1;
   if ( pRadioInfo->uHardwareParamsList[10] != uChannels )
      iAnyChanged = 1;
   if ( pRadioInfo->uHardwareParamsList[8] != uFrequency )
      iAnyChanged = 1;
   if ( pRadioInfo->uHardwareParamsList[9] != (uFrequency + uFreqSpread) )
      iAnyChanged = 1;

   // Duty Cycle
   if ( pRadioInfo->uHardwareParamsList[11] != 100 )
      iAnyChanged = 1;
   // Max Window
   if ( pRadioInfo->uHardwareParamsList[15] != 50 )
      iAnyChanged = 1;
   
   if ( ! iAnyChanged )
   {
      log_line("[HardwareRadio]: SiK Radio all params are unchanged. Set SiK radio interface %d to frequency %s, channels: %u, freq spread: %.1f Mhz, NetId: %u, AirSpeed: %u bps",
         pRadioInfo->phy_index+1, str_format_frequency(uFrequency),
         uChannels, (float)uFreqSpread/1000.0, uNetId, uAirSpeed );
      return 1;
   }

   //hardware_radio_sik_get_all_params(pRadioInfo, pProcessStats);

   hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioInfo->szDriver);
   
   if ( NULL == pSerialPort )
   {
      log_error_and_alarm("[HardwareRadio] Failed to find serial port configuration for SiK radio %s.", pRadioInfo->szDriver);
      return 0;
   }

   int iSerialPort = hardware_open_serial_port(pRadioInfo->szDriver, pSerialPort->lPortSpeed);
   if ( iSerialPort <= 0 )
   {
      log_error_and_alarm("[HardwareRadio] Failed to open serial port for SiK radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      return 0;
   }

   // Enter AT mode
   if ( ! _hardware_radio_sik_enter_command_mode(iSerialPort, pProcessStats) )
   {
      log_softerror_and_alarm("[HardwareRadio] Failed to enter SiK radio into AT command mode.");
      close(iSerialPort);
      return 0;
   }

   // Send params

   if ( pRadioInfo->uHardwareParamsList[2] != hardware_radio_sik_get_encoded_serial_baudrate(uAirSpeed) )
   {
      if ( _hardware_radio_sik_set_parameter(iSerialPort, 2, hardware_radio_sik_get_encoded_serial_baudrate(uAirSpeed)) )
         pRadioInfo->uHardwareParamsList[2] = hardware_radio_sik_get_encoded_serial_baudrate(uAirSpeed);
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }

   if ( pRadioInfo->uHardwareParamsList[3] != uNetId )
   {
      if ( _hardware_radio_sik_set_parameter(iSerialPort, 3, uNetId) )
         pRadioInfo->uHardwareParamsList[3] = uNetId;
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }

   if ( pRadioInfo->uHardwareParamsList[8] != uFrequency )
   {
      if ( _hardware_radio_sik_set_parameter(iSerialPort, 8, uFrequency) )
         pRadioInfo->uHardwareParamsList[8] = uFrequency;
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }

   if ( pRadioInfo->uHardwareParamsList[9] != uFrequency + uFreqSpread )
   {
      if ( _hardware_radio_sik_set_parameter(iSerialPort, 9, uFrequency + uFreqSpread ) )
         pRadioInfo->uHardwareParamsList[9] = uFrequency + uFreqSpread;
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }

   if ( pRadioInfo->uHardwareParamsList[10] != uChannels )
   {
      if ( _hardware_radio_sik_set_parameter(iSerialPort, 10, uChannels) )
         pRadioInfo->uHardwareParamsList[10] = uChannels;
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }

   // Duty cycle to 100 % ( percentage of time allowed to transmit )

   if ( pRadioInfo->uHardwareParamsList[11] != 100 )
   {
      if ( _hardware_radio_sik_set_parameter(iSerialPort, 11, 100 ) )
         pRadioInfo->uHardwareParamsList[11] = 100;
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }

   // Max Window

   if ( pRadioInfo->uHardwareParamsList[15] != 50 )
   {
      if ( _hardware_radio_sik_set_parameter(iSerialPort, 15, 50 ) )
         pRadioInfo->uHardwareParamsList[15] = 50;
      if ( NULL != pProcessStats )
         pProcessStats->lastActiveTime = get_current_timestamp_ms();
   }

   // Exit AT command mode
   _hardware_radio_sik_save_settings_to_flash(iSerialPort);

   if ( NULL != pProcessStats )
      pProcessStats->lastActiveTime = get_current_timestamp_ms();

   ////////////////////
   close(iSerialPort);

   log_line("[HardwareRadio]: Did set SiK radio interface %d to frequency %s, channels: %u, freq spread: %.1f Mhz, NetId: %u, AirSpeed: %u bps",
      pRadioInfo->phy_index+1, str_format_frequency(uFrequency),
      uChannels, (float)uFreqSpread/1000.0, uNetId, uAirSpeed );

   hardware_radio_sik_save_configuration();
   return 1;
}

int hardware_radio_sik_open_for_read_write(int iHWRadioInterfaceIndex)
{
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iHWRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("[HardwareRadio] Open: Failed to get SiK radio hardware info for hardware radio interface %d.", iHWRadioInterfaceIndex+1);
      return 0;
   }
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
   {
      log_softerror_and_alarm("[HardwareRadio] Open: Tried to open hardware radio interface %d which is not a Sik radio.", iHWRadioInterfaceIndex+1);
      return 0;
   }

   hw_serial_port_info_t* pSerialPort = hardware_get_serial_port_info_from_serial_port_name(pRadioInfo->szDriver);
  
   if ( NULL == pSerialPort )
   {
      log_error_and_alarm("[HardwareRadio] Open: Failed to find serial port configuration for SiK radio %s.", pRadioInfo->szDriver);
      return 0;
   }

   int iSerialPortFD = hardware_open_serial_port(pRadioInfo->szDriver, pSerialPort->lPortSpeed);
   if ( iSerialPortFD <= 0 )
   {
      log_error_and_alarm("[HardwareRadio] Open: Failed to open serial port for SiK radio %s at %ld bps.", pRadioInfo->szDriver, pSerialPort->lPortSpeed);
      return 0;
   }
   pRadioInfo->openedForWrite = 1;
   pRadioInfo->openedForRead = 1;
   pRadioInfo->monitor_interface_read.selectable_fd = iSerialPortFD;
   pRadioInfo->monitor_interface_write.selectable_fd = iSerialPortFD;

   log_line("[HardwareRadio] Opened SiK radio interface %d for read/write. fd=%d", iHWRadioInterfaceIndex+1, iSerialPortFD);
   return 1;
}

int hardware_radio_sik_close(int iHWRadioInterfaceIndex)
{
   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iHWRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("[HardwareRadio] Open: Failed to get SiK radio hardware info for hardware radio interface %d.", iHWRadioInterfaceIndex+1);
      return 0;
   }
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
   {
      log_softerror_and_alarm("[HardwareRadio] Open: Tried to open hardware radio interface %d which is not a Sik radio.", iHWRadioInterfaceIndex+1);
      return 0;
   }

   if ( pRadioInfo->openedForWrite || pRadioInfo->openedForRead )
   {
      if ( pRadioInfo->monitor_interface_read.selectable_fd > 0 )
      {
         close(pRadioInfo->monitor_interface_read.selectable_fd);
      }
      else if ( pRadioInfo->monitor_interface_write.selectable_fd > 0 )
      {
         close(pRadioInfo->monitor_interface_write.selectable_fd);
      }
      pRadioInfo->monitor_interface_read.selectable_fd = -1;
      pRadioInfo->monitor_interface_write.selectable_fd = -1;
      pRadioInfo->openedForWrite = 0;
      pRadioInfo->openedForRead = 0;
   }
   return 1;
}

int hardware_radio_sik_write_packet(int iHWRadioInterfaceIndex, u8* pData, int iLength)
{
   if ( (NULL == pData) || (iLength <= 0) )
   {
      log_softerror_and_alarm("[HardwareRadio] Write: Invalid parameters: NULL buffer or 0 length.");
      return 0;
   }

   radio_hw_info_t* pRadioInfo = hardware_get_radio_info(iHWRadioInterfaceIndex);
   if ( NULL == pRadioInfo )
   {
      log_softerror_and_alarm("[HardwareRadio] Write: Failed to get SiK radio hardware info for hardware radio interface %d.", iHWRadioInterfaceIndex+1);
      return 0;
   }
   if ( ! hardware_radio_is_sik_radio(pRadioInfo) )
   {
      log_softerror_and_alarm("[HardwareRadio] Write: Tried to open hardware radio interface %d which is not a Sik radio.", iHWRadioInterfaceIndex+1);
      return 0;
   }

   if ( (! pRadioInfo->openedForWrite) || (pRadioInfo->monitor_interface_write.selectable_fd <= 0) )
   {
      log_softerror_and_alarm("[HardwareRadio] Write: Interface is not opened for write.");
      return 0;
   }

   int iRes = write(pRadioInfo->monitor_interface_write.selectable_fd, pData, iLength);
   if ( iRes != iLength )
   {
      log_softerror_and_alarm("[HardwareRadio] Write: Failed to write. Written %d bytes of %d bytes.", iRes, iLength);
      return 0;
   }

   //log_line("[HardwareRadio] Write: wrote %d bytes to SiK radio.", iLength);
   return 1;
}

