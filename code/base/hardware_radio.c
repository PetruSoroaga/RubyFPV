/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
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

#include <fcntl.h>        // serialport
#include <termios.h>      // serialport
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hardware_radio.h"
#include "hardware_serial.h"
#include "hardware_radio_sik.h"
#include "hw_procs.h"
#include "../common/string_utils.h"

#define MAX_USB_DEVICES_INFO 12

static int s_iEnumeratedUSBRadioInterfaces = 0;

static usb_radio_interface_info_t s_USB_RadioInterfacesInfo[MAX_USB_DEVICES_INFO];
static int s_iFoundUSBRadioInterfaces = 0;

static radio_hw_info_t sRadioInfo[MAX_RADIO_INTERFACES];
static int s_iHwRadiosCount = 0;
static int s_iHwRadiosSupportedCount = 0;
static int s_HardwareRadiosEnumeratedOnce = 0;


radio_hw_info_t* hardware_get_radio_info_array()
{
   return &(sRadioInfo[0]);
}

void hardware_log_radio_info()
{
   char szBuff[1024];

   log_line("");
   for( int i=0; i<s_iHwRadiosCount; i++ )
   {
      log_line("* RadioInterface %d: %s MAC:%s phy#%d, supported: %s, USBPort: %s now at %s;", i+1, sRadioInfo[i].szName, sRadioInfo[i].szMAC, sRadioInfo[i].phy_index, sRadioInfo[i].isSupported?"yes":"no", sRadioInfo[i].szUSBPort, str_format_frequency(sRadioInfo[i].uCurrentFrequencyKhz));
      sprintf( szBuff, "   Type: %s, %s, pid/vid: %s driver: %s, ", str_get_radio_card_model_string(sRadioInfo[i].iCardModel), sRadioInfo[i].szDescription, sRadioInfo[i].szProductId, sRadioInfo[i].szDriver);
      
      char szBands[128];
      str_get_supported_bands_string(sRadioInfo[i].supportedBands, szBands);
      strcat( szBuff, szBands);
      log_line(szBuff);
      log_line("");
   }
   log_line("=================================================================");
}

void hardware_save_radio_info()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_RADIO_HW_CONFIG);
   FILE* fp = fopen(szFile, "w");
   if ( NULL == fp )
   {
      log_softerror_and_alarm("Failed to save radio HW info file.");
      return;
   }
   u8 buffer[9056];
   int pos = 0;
   memcpy(buffer+pos, &s_iHwRadiosCount, sizeof(s_iHwRadiosCount));
   pos += sizeof(s_iHwRadiosCount);
   memcpy(buffer+pos, &s_iHwRadiosSupportedCount, sizeof(s_iHwRadiosSupportedCount));
   pos += sizeof(s_iHwRadiosSupportedCount);
   for( int i=0; i<s_iHwRadiosCount; i++ )
   {
      memcpy(buffer+pos, &(sRadioInfo[i]), sizeof(radio_hw_info_t));
      pos += sizeof(radio_hw_info_t);
   }
   u32 crc = base_compute_crc32(buffer, pos);
   memcpy( buffer+pos, &crc, sizeof(u32));
   pos += sizeof(u32);

   fwrite( buffer, 1, pos, fp );
   fclose(fp);
   log_line("Saved radio HW info file.");
}

int hardware_load_radio_info()
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_RADIO_HW_CONFIG);
   if ( access(szFile, R_OK) == -1 )
   {
      log_line("No radio HW info file.");
      return 0;
   }
   int succeeded = 1;
   u8 bufferIn[9056];
   int posIn = 0;

   FILE* fp = fopen(szFile, "r");
   if ( NULL == fp )
      succeeded = 0;

   if ( succeeded )
   {
      if ( sizeof(s_iHwRadiosCount) != fread(&s_iHwRadiosCount, 1, sizeof(s_iHwRadiosCount), fp) )
         succeeded = 0;
      if ( s_iHwRadiosCount < 0 || s_iHwRadiosCount >= MAX_RADIO_INTERFACES )
         succeeded = 0;
      memcpy(bufferIn+posIn, &s_iHwRadiosCount, sizeof(s_iHwRadiosCount));
      posIn += sizeof(s_iHwRadiosCount);
   }
   if ( succeeded )
   {
      if ( sizeof(s_iHwRadiosSupportedCount) != fread(&s_iHwRadiosSupportedCount, 1, sizeof(s_iHwRadiosSupportedCount), fp) )
         succeeded = 0;
      if ( s_iHwRadiosSupportedCount < 0 || s_iHwRadiosSupportedCount >= MAX_RADIO_INTERFACES )
         succeeded = 0;
      memcpy(bufferIn+posIn, &s_iHwRadiosSupportedCount, sizeof(s_iHwRadiosSupportedCount));
      posIn += sizeof(s_iHwRadiosSupportedCount);
   }
   if ( succeeded )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         if ( sizeof(radio_hw_info_t) != fread(&(sRadioInfo[i]), 1, sizeof(radio_hw_info_t), fp) )
            succeeded = 0;
         memcpy(bufferIn+posIn, &(sRadioInfo[i]), sizeof(radio_hw_info_t));
         posIn += sizeof(radio_hw_info_t);
      }
   }
   if ( succeeded )
   {
      u32 crc = 0;
      if ( sizeof(u32) != fread(&crc, 1, sizeof(u32), fp ) )
         succeeded = 0;
      u32 crc2 = base_compute_crc32(bufferIn, posIn);
      if ( crc != crc2 )
      {
         succeeded = 0;
         log_softerror_and_alarm("HIC HW Info file is corrupted. Wrong CRC.");
      }
   }

   if ( NULL != fp )
      fclose(fp);

   if ( succeeded )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         sRadioInfo[i].openedForRead = 0;
         sRadioInfo[i].openedForWrite = 0;
         sRadioInfo[i].monitor_interface_read.ppcap = NULL;
         sRadioInfo[i].monitor_interface_read.selectable_fd = -1;
         sRadioInfo[i].monitor_interface_read.nPort = 0;
         sRadioInfo[i].monitor_interface_write.ppcap = NULL;
         sRadioInfo[i].monitor_interface_write.selectable_fd = -1;

         if ( (sRadioInfo[i].supportedBands & RADIO_HW_SUPPORTED_BAND_433) ||
              (sRadioInfo[i].supportedBands & RADIO_HW_SUPPORTED_BAND_868) ||
              (sRadioInfo[i].supportedBands & RADIO_HW_SUPPORTED_BAND_915) )
         if ( sRadioInfo[i].isHighCapacityInterface )
         {
            log_line("Hardware: Removed invalid high capacity flag from radio interface %d (%s, %s)",
               i+1, sRadioInfo[i].szName, sRadioInfo[i].szDriver);
            sRadioInfo[i].isHighCapacityInterface = 0;
         }
      }

      log_line("Loaded radio HW info file.");
      log_line("=================================================================");
      log_line("Total: %d Radios read:", s_iHwRadiosCount);
      hardware_log_radio_info();
      return 1;
   }

   log_softerror_and_alarm("Failed to read radio HW info file.");
   return 0;
}

int _hardware_detect_card_model(const char* szProductId)
{
   if ( (NULL == szProductId) || (0 == szProductId[0]) )
      return 0;

   if ( NULL != strstr( szProductId, "cf3:9271" ) )
      return CARD_MODEL_TPLINK722N;

   //if ( NULL != strstr( szProductId, "cf3:9271" ) )
   //   return CARD_MODEL_ALFA_AWUS036NHA;

   if ( NULL != strstr( szProductId, "148f:3070" ) )
      return CARD_MODEL_ALFA_AWUS036NH;

   if ( NULL != strstr( szProductId, "b05:17d2" ) )
      return CARD_MODEL_ASUS_AC56;

   if ( NULL != strstr( szProductId, "846:9052" ) )
      return CARD_MODEL_NETGEAR_A6100;

   if ( NULL != strstr( szProductId, "bda:881a" ) )
      return CARD_MODEL_RTL8812AU_DUAL_ANTENNA;
   //if ( NULL != strstr( szProductId, "bda/881a" ) )
   //   return CARD_MODEL_ZIPRAY;

   if ( NULL != strstr( szProductId, "bda:8812" ) )
      return CARD_MODEL_ALFA_AWUS036ACH;

   if ( NULL != strstr( szProductId, "bda:8811" ) )
      return CARD_MODEL_ALFA_AWUS036ACS;
   if ( NULL != strstr( szProductId, "bda:0811" ) )
      return CARD_MODEL_ALFA_AWUS036ACS;
   if ( NULL != strstr( szProductId, "bda:811" ) )
      return CARD_MODEL_ALFA_AWUS036ACS;

   if ( NULL != strstr( szProductId, "2604:12" ) )
      return CARD_MODEL_TENDA_U12;
   if ( NULL != strstr( szProductId, "2604:0012" ) )
      return CARD_MODEL_TENDA_U12;

   if ( NULL != strstr( szProductId, "2357:120" ) )
      return CARD_MODEL_ARCHER_T2UPLUS;
   if ( NULL != strstr( szProductId, "2357:0120" ) )
      return CARD_MODEL_ARCHER_T2UPLUS;

   if ( NULL != strstr( szProductId, "bda:8813" ) )
      return CARD_MODEL_RTL8814AU;

   if ( NULL != strstr( szProductId, "bda:a81a" ) )
      return CARD_MODEL_BLUE_8812EU;
   if ( NULL != strstr( szProductId, "0bda:a81a" ) )
      return CARD_MODEL_BLUE_8812EU;
   return 0;
}

// Assigns the right USB port for each detected radio interface. If possible.

void _hardware_assign_usb_from_physical_ports()
{
   if ( s_iHwRadiosCount <= 0 )
      return;

   log_line("[HardwareRadio] Finding USB ports for %d radio interfaces...", s_iHwRadiosCount);

   // Set some default values first
   for( int i=0; i<s_iHwRadiosCount; i++ )
   {
      sRadioInfo[i].szUSBPort[0] = 'Y';
      sRadioInfo[i].szUSBPort[1] = '1' + i;
      sRadioInfo[i].szUSBPort[2] = 0;
   }

   DIR *d;
   struct dirent *dir;
   char szComm[256];
   char szOutput[1024];

   d = opendir("/sys/bus/usb/devices/");
   if ( NULL == d )
   {
      log_line("[HardwareRadio] Can't find USB ports for radio interfaces (can't open /sys/bus/usb/devices).");
      return;
   }

   while ((dir = readdir(d)) != NULL)
   {
      int iLen = strlen(dir->d_name);
      if ( iLen < 3 )
         continue;

      log_line("[HardwareRadio] Quering USB device path: [%s]...", dir->d_name);

      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cat /sys/bus/usb/devices/%s/uevent 2>/dev/null | grep DRIVER", dir->d_name);
      szOutput[0] = 0;
      hw_execute_bash_command_silent(szComm, szOutput);
      if ( 0 == szOutput[0] )
      {
         log_line("[HardwareRadio] No info for USB device: [%s]. Skipping it.", dir->d_name);
         continue;
      }
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         szOutput[0] = 0;
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cat /sys/bus/usb/devices/%s/net/%s/uevent 2>/dev/null | grep DEVTYPE=wlan", dir->d_name, sRadioInfo[i].szName);
         hw_execute_bash_command_silent(szComm, szOutput);
         if ( 0 == szOutput[0] )
            continue;

         szOutput[0] = 0;
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cat /sys/bus/usb/devices/%s/net/%s/uevent 2>/dev/null | grep INTERFACE", dir->d_name, sRadioInfo[i].szName);
         hw_execute_bash_command_silent(szComm, szOutput);
         if ( 0 == szOutput[0] )
            continue;

         int iPos = 0;
         iLen = strlen(szOutput);
         while ( iPos < iLen )
         {
            if ( szOutput[iPos] == '=' )
            {
               iPos++;
               break;
            }
            iPos++;
         }

         if ( iPos >= iLen )
            continue;

         int bMathcingInterfaceName = 1;
         for( int k=0; k<strlen(sRadioInfo[i].szName); k++ )
            if ( szOutput[iPos+k] != sRadioInfo[i].szName[k] )
               bMathcingInterfaceName = 0;

         if ( ! bMathcingInterfaceName )
            continue;

         // Found the USB port for radio interface i

         log_line("[HardwareRadio] Found USB port tree position for radio interface %d: [%s]", i+1, dir->d_name);

         // Find the product id / vendor id

         szOutput[0] = 0;
         snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "cat /sys/bus/usb/devices/%s/uevent 2>/dev/null | grep PRODUCT", dir->d_name);
         hw_execute_bash_command_silent(szComm, szOutput);
         if ( 0 != szOutput[0] )
         {
            iLen = strlen(szOutput);
            int iPos = 0;
            while ( iPos < iLen )
            {
               if ( szOutput[iPos] == '=' )
               {
                  iPos++;
                  break;
               }
               szOutput[iPos] = tolower(szOutput[iPos]);
               iPos++;
            }
            if ( iPos < iLen )
            {
               for( int kk=iPos; kk<iLen; kk++ )
                  szOutput[kk] = tolower(szOutput[kk]);

               strncpy( sRadioInfo[i].szProductId, &(szOutput[iPos]), sizeof(sRadioInfo[i].szProductId)/sizeof(sRadioInfo[i].szProductId[0]) );
               sRadioInfo[i].szProductId[(sizeof(sRadioInfo[i].szProductId)/sizeof(sRadioInfo[i].szProductId[0]))-1] = 0;
               for( int kk=0; kk<strlen(sRadioInfo[i].szProductId); kk++ )
               {
                  if ( sRadioInfo[i].szProductId[kk] == '/' )
                     sRadioInfo[i].szProductId[kk] = ':';
               }
               sRadioInfo[i].iCardModel = _hardware_detect_card_model(sRadioInfo[i].szProductId);
               log_line("[HardwareRadio] Found product/vendor id for radio interface %d: [%s], card model: %d: [%s]", i+1, sRadioInfo[i].szProductId, sRadioInfo[i].iCardModel, str_get_radio_card_model_string(sRadioInfo[i].iCardModel) );
            }
         }

         // Done finding the product id / vendor id

         // Parse the USB port path info

         char szPort[128];
         strncpy(szPort, dir->d_name, 127);
         szPort[127] = 0;

         // Find first : in string

         int iPosEnd = 0;
         iLen = strlen(szPort);
         while ( iPosEnd < iLen )
         {
            if ( szPort[iPosEnd] == ':' )
            {
               szPort[iPosEnd] = 0;
               break;
            }
            iPosEnd++;
         }

         // Found the first : or the end of string.
         // USB ports are the digits before x-y.A.B.C...N:
         // Ignore the x-y part

         log_line("[HardwareRadio] Found USB port part for radio interface %d: [%s]", i+1, szPort);
         
         int iPosMinus = iPosEnd-1;
         while ( iPosMinus > 0 )
         {
            if ( szPort[iPosMinus] == '-' )
               break;
            iPosMinus--;
         }

         if ( szPort[iPosMinus+1] == 0 )
            continue;

         // Is it a short version? X-Y, no : after it;
         if ( szPort[iPosMinus+2] == 0 || ((szPort[iPosMinus+2] == ' ') && (szPort[iPosMinus+3] == 0) ))
         {
            iPosMinus++;
            int nb = atoi(&(szPort[iPosMinus]))-1;
            if ( nb < 0 )
               nb = 0;
            sRadioInfo[i].szUSBPort[0] = 'A' + nb;
            sRadioInfo[i].szUSBPort[1] = 0;
            log_line("[HardwareRadio] Assigned short USB port to radio interface %d: [%s]", i+1, sRadioInfo[i].szUSBPort);
            continue;
         }

         // Find first . after -
         while ( iPosMinus < iPosEnd )
         {
            if ( szPort[iPosMinus] == '.' )
            {
               iPosMinus++;
               break;
            }
            iPosMinus++;
         }

         if ( iPosMinus >= iPosEnd || iPosMinus >= iLen || iPosMinus < 0 )
            continue;

         log_line("[HardwareRadio] Found USB port part2 for radio interface %d: [%s]", i+1, &(szPort[iPosMinus]));

         if ( szPort[iPosMinus] < '0' || szPort[iPosMinus] > '9' )
         {
            log_line("[HardwareRadio] Invalid port string [%s]. Skipping.", &(szPort[iPosMinus]));
            continue;
         }
         char szNb[32];
         szNb[0] = szPort[iPosMinus];
         szNb[1] = 0;
         iPosMinus += 2;

         int nb = atoi(szNb)-1;
         if ( nb < 0 )
            nb = 0;
         sRadioInfo[i].szUSBPort[0] = 'A' + nb;
         sRadioInfo[i].szUSBPort[1] = 0;
         if ( iPosMinus < iPosEnd )
            strncpy(&(sRadioInfo[i].szUSBPort[1]), &(szPort[iPosMinus]), 5);
         sRadioInfo[i].szUSBPort[5] = 0;
         log_line("[HardwareRadio] Assigned USB port to radio interface %d: [%s]", i+1, sRadioInfo[i].szUSBPort);
      }
   }
   closedir(d);

   // Assign some USB port to the ones that did not have a match

   for( int i=0; i<s_iHwRadiosCount; i++ )
   {
      if ( sRadioInfo[i].szUSBPort[0] != 0 )
         continue;
      sRadioInfo[i].szUSBPort[0] = 'X';
      sRadioInfo[i].szUSBPort[1] = '1' + i;
      sRadioInfo[i].szUSBPort[2] = 0;
   }

   // Sort the radio interfaces on USB port number
   
   for( int i=0; i<s_iHwRadiosCount-1; i++ )
   for( int j=i+1; j<s_iHwRadiosCount; j++ )
      if ( strncmp( sRadioInfo[i].szUSBPort, sRadioInfo[j].szUSBPort, 6 ) > 0 )
      {
         radio_hw_info_t tmp;
         memcpy(&tmp, &(sRadioInfo[i]), sizeof(radio_hw_info_t));
         memcpy(&(sRadioInfo[i]), &(sRadioInfo[j]), sizeof(radio_hw_info_t));
         memcpy(&(sRadioInfo[j]), &tmp, sizeof(radio_hw_info_t));
      }

   log_line("[HardwareRadio] Done assigning USB ports to radio interfaces:");
   for( int i=0; i<s_iHwRadiosCount; i++ )
      log_line("[HardwareRadio] Radio interface %d USB port: [%s]", i+1, sRadioInfo[i].szUSBPort);
}

int _hardware_radio_get_product_id_open_ipc_wifi_radio_driver_id(const char* szProdId)
{
   if ( (NULL == szProdId) || (0 == szProdId[0]) )
      return 0;

   if ( (0 == strcmp(szProdId, "bda:8812")) ||
        (0 == strcmp(szProdId, "bda:881a")) ||
        (0 == strcmp(szProdId, "b05:17d2")) ||
        (0 == strcmp(szProdId, "2357:0101")) ||
        (0 == strcmp(szProdId, "2604:0012")) )
      return RADIO_HW_DRIVER_REALTEK_RTL88XXAU;

   if ( (0 == strcmp(szProdId, "cf3:9271")) ||
        (0 == strcmp(szProdId, "40d:3801")) )
      return RADIO_HW_DRIVER_ATHEROS;

   if ( (0 == strcmp(szProdId, "0bda:a81a")) ||
        (0 == strcmp(szProdId, "bda:a81a")) )
      return RADIO_HW_DRIVER_REALTEK_8812EU;

   return 0;
}

void _hardware_find_usb_radio_interfaces_info()
{
   s_iEnumeratedUSBRadioInterfaces = 1;
   s_iFoundUSBRadioInterfaces = 0;
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   log_line("[HardwareRadio] Finding USB radio interfaces and devices for OpenIPC platform...");
   #else
   log_line("[HardwareRadio] Finding USB radio interfaces and devices for linux/raspberry platform...");
   #endif

   char szBuff[2048];
   szBuff[0] = 0;
   hw_execute_bash_command_raw("lsusb", szBuff);

   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   log_line("[HardwareRadio] Output of lsusb: <<<%s>>>", szBuff);
   #endif

   int iLen = strlen(szBuff);
   if ( iLen >= (int)(sizeof(szBuff)/sizeof(szBuff[0])) )
   {
      iLen = (int)(sizeof(szBuff)/sizeof(szBuff[0])) - 1;
      szBuff[iLen] = 0;
   }
   int iPos = 0;
   while (iPos <= iLen)
   {
      // Get one line
      int iEndLine = iPos;
      while ( (szBuff[iEndLine] != 10) && (szBuff[iEndLine] != 13) && (szBuff[iEndLine] != 0) && (iEndLine < iLen) )
         iEndLine++;
      szBuff[iEndLine] = 0;

      if ( (szBuff[iEndLine+1] == 10) || (szBuff[iEndLine+1] == 13) )
      {
         iEndLine++;
         szBuff[iEndLine] = 0;
      }

      // Parse the line
      log_line("[HardwareRadio] Parsing USB line: [%s]", &(szBuff[iPos]));
      int bLookForBus = 1;
      int bLookForDev = 1;
      int bLookForId = 1;
      for( int i=iPos; i<iEndLine; i++ )
      {
         if ( bLookForBus )
         {
            if ( szBuff[i] < '0' || szBuff[i] > '9' )
               continue;
            int iStartNb = i;
            while ( szBuff[i] >= '0' && szBuff[i] <= '9' )
               i++;
            szBuff[i] = 0;
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iBusNb = atoi(szBuff+iStartNb);
            bLookForBus = 0;
            continue;
         }
         else if ( bLookForDev )
         {
            if ( szBuff[i] < '0' || szBuff[i] > '9' )
               continue;
            int iStartNb = i;
            while ( szBuff[i] >= '0' && szBuff[i] <= '9' )
               i++;
            szBuff[i] = 0;
            szBuff[i+1] = 0;
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDeviceNb = atoi(szBuff+iStartNb);
            bLookForDev = 0;
            continue;
         }
         else if ( bLookForId )
         {
            if ( (szBuff[i] < '0') || (szBuff[i] > '9') )
               continue;
            int iStartId = i;
            while ( (szBuff[i] >= '0' && szBuff[i] <= '9') || szBuff[i] == ':' || (szBuff[i] >= 'a' && szBuff[i] <= 'f') )
               i++;
            szBuff[i] = 0;
            // Skip leading 0s
            if ( szBuff[iStartId] == '0' )
               iStartId++;
            if ( szBuff[iStartId] == '0' )
               iStartId++;
            strncpy(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId, szBuff+iStartId, sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId)-1 );
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId[sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId)-1] = 0;
            for( int kk=9; kk<strlen(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId); kk++ )
                  s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId[kk] = tolower(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId[kk]);

            bLookForId = 0;

            #ifdef HW_PLATFORM_OPENIPC_CAMERA
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDriver = _hardware_radio_get_product_id_open_ipc_wifi_radio_driver_id(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId);
            strncpy(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName, str_get_radio_card_model_string(_hardware_detect_card_model(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId)), sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName)/sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[0]) );
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName)/sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[0]) - 1] = 0;
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iPortNb = -1;
            log_line("[HardwareRadio] Found USB device: bus %d, device %d: prod id: %s, name: %s",
                s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iBusNb,
                s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDeviceNb,
                s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId,
                s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName);

            if ( s_iFoundUSBRadioInterfaces < MAX_USB_DEVICES_INFO-1 )
               s_iFoundUSBRadioInterfaces++;
            else
               log_softerror_and_alarm("[HardwareRadio] Not enough space in USB devices interfaces info to store one more entry. It has %d entries already.", s_iFoundUSBRadioInterfaces);
            break;
            #endif
         }
         else
         {
            //strncpy(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName, szBuff+i, sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName)-1 );
            //s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDriver = 0;
            //s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName)-1] = 0;
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDriver = _hardware_radio_get_product_id_open_ipc_wifi_radio_driver_id(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId);
            strncpy(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName, str_get_radio_card_model_string(_hardware_detect_card_model(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId)), sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName)/sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[0]) );
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName)/sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[0]) - 1] = 0;

            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iPortNb = -1;
            log_line("[HardwareRadio] Found USB device: bus %d, device %d: prod id: %s, name: %s",
                s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iBusNb,
                s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDeviceNb,
                s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId,
                s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName);

            if ( s_iFoundUSBRadioInterfaces < MAX_USB_DEVICES_INFO-1 )
               s_iFoundUSBRadioInterfaces++;
            else
               log_softerror_and_alarm("[HardwareRadio] Not enough space in USB devices interfaces info to store one more entry. It has %d entries already.", s_iFoundUSBRadioInterfaces);
            break;
         }
      }

      // Go to next line
      iEndLine++;
      iPos = iEndLine;
   }

   log_line("[HardwareRadio] Done finding USB devices. Found %d USB devices.", s_iFoundUSBRadioInterfaces);
}

int _hardware_enumerate_wifi_radios()
{
   #ifdef HW_PLATFORM_OPENIPC_CAMERA
   log_line("[HardwareRadio] Enumerating wifi radios for OpenIPC platform...");
   #else
   log_line("[HardwareRadio] Enumerating wifi radios for linux/raspberry platform...");
   #endif

   char szDriver[128];
   char szComm[256];
   char szBuff[1024];

   _hardware_find_usb_radio_interfaces_info();

   log_line("[HardwareRadio] Finding wireless radio cards...");
   FILE* fp = popen("ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot", "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to enumerate 2.4/5.8 radios.");
      return 0;
   }
   
   int iStartIndex = s_iHwRadiosCount;

   while (fgets(szBuff, 255, fp) != NULL)
   {
      sscanf(szBuff, "%s", sRadioInfo[s_iHwRadiosCount].szName);
      
      if ( 0 == sRadioInfo[s_iHwRadiosCount].szName[0] )
         continue;
      if ( 0 != strstr(sRadioInfo[s_iHwRadiosCount].szName, "wlx") )
         continue;
        
      log_line("[HardwareRadio] Parsing found wireless radio: [%s]", sRadioInfo[s_iHwRadiosCount].szName);
      if ( 0 == strstr(sRadioInfo[s_iHwRadiosCount].szName, "wlan" ) )
      if ( 0 == strstr(sRadioInfo[s_iHwRadiosCount].szName, "wlx" ) )
      {
         log_line("[HardwareRadio] Skipping wireless radio [%s]", sRadioInfo[s_iHwRadiosCount].szName);
         continue;
      }
      
      sRadioInfo[s_iHwRadiosCount].iCardModel = 0;
      sRadioInfo[s_iHwRadiosCount].isSupported = 0;
      sRadioInfo[s_iHwRadiosCount].isSerialRadio = 0;
      sRadioInfo[s_iHwRadiosCount].isConfigurable = 1;
      sRadioInfo[s_iHwRadiosCount].isEnabled = 1;
      sRadioInfo[s_iHwRadiosCount].isTxCapable = 1;
      sRadioInfo[s_iHwRadiosCount].isHighCapacityInterface = 1;
      sRadioInfo[s_iHwRadiosCount].iCurrentDataRateBPS = 0;
      sRadioInfo[s_iHwRadiosCount].uCurrentFrequencyKhz = 0;
      sRadioInfo[s_iHwRadiosCount].lastFrequencySetFailed = 0;
      sRadioInfo[s_iHwRadiosCount].uFailedFrequencyKhz = 0;
      sRadioInfo[s_iHwRadiosCount].phy_index = 0;
      sRadioInfo[s_iHwRadiosCount].uExtraFlags = 0;
      sRadioInfo[s_iHwRadiosCount].supportedBands = 0;
      sRadioInfo[s_iHwRadiosCount].szDescription[0] = 0;
      sRadioInfo[s_iHwRadiosCount].szProductId[0] = 0;
      sRadioInfo[s_iHwRadiosCount].szDriver[0] = 0;
      sRadioInfo[s_iHwRadiosCount].szMAC[0] = 0;
      sRadioInfo[s_iHwRadiosCount].szUSBPort[0] = 'X';
      sRadioInfo[s_iHwRadiosCount].szUSBPort[1] = '0';
      sRadioInfo[s_iHwRadiosCount].iRadioType = RADIO_TYPE_OTHER;
      sRadioInfo[s_iHwRadiosCount].iRadioDriver = 0;
      memset(&(sRadioInfo[s_iHwRadiosCount].uHardwareParamsList[0]), 0, sizeof(u32)*MAX_RADIO_HW_PARAMS);
      sRadioInfo[s_iHwRadiosCount].openedForRead = 0;
      sRadioInfo[s_iHwRadiosCount].openedForWrite = 0;
      sRadioInfo[s_iHwRadiosCount].monitor_interface_read.ppcap = NULL;
      sRadioInfo[s_iHwRadiosCount].monitor_interface_read.selectable_fd = -1;
      sRadioInfo[s_iHwRadiosCount].monitor_interface_read.nPort = 0;
      sRadioInfo[s_iHwRadiosCount].monitor_interface_write.ppcap = NULL;
      sRadioInfo[s_iHwRadiosCount].monitor_interface_write.selectable_fd = -1;
      s_iHwRadiosCount++;
      if ( s_iHwRadiosCount >= MAX_RADIO_INTERFACES )
         break;
   }
   pclose(fp);

   log_line("[HardwareRadio] Found a total of %d wifi cards. Get info about them...", s_iHwRadiosCount);
   s_iHwRadiosSupportedCount = 0;

   for( int i=iStartIndex; i<s_iHwRadiosCount; i++ )
   {
      szDriver[0] = 0;

      #ifdef HW_PLATFORM_OPENIPC_CAMERA
      sprintf(szComm, "ls -Al /sys/class/net/%s/device/ | grep driver", sRadioInfo[i].szName);
      hw_execute_bash_command_raw(szComm, szDriver);
      #else
      sprintf(szComm, "cat /sys/class/net/%s/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'", sRadioInfo[i].szName);
      hw_execute_bash_command(szComm, szDriver);
      #endif

      log_line("[HardwareRadio] Driver for %s: <<<%s>>>", sRadioInfo[i].szName, szDriver);
      log_line("[HardwareRadio] Driver for %s (last part): <<<%s>>>", sRadioInfo[i].szName, &szDriver[strlen(szDriver)/2]);
      char* pszDriver = szDriver;
      if ( strlen(szDriver) > 0 )
      for( int i=strlen(szDriver)-1; i>0; i-- )
      {
         if ( szDriver[i] == '/' )
         {
            pszDriver = &szDriver[i];
            break;
         }
      }
      log_line("[HardwareRadio] Minimised driver string: <<<%s>>>", pszDriver);

      sRadioInfo[i].isSupported = 0;
      if ( (NULL != strstr(pszDriver,"rtl88xxau")) ||
           (NULL != strstr(pszDriver,"8812au")) ||
           (NULL != strstr(pszDriver,"rtl8812au")) ||
           (NULL != strstr(pszDriver,"rtl88XXau")) ||
           (NULL != strstr(pszDriver,"8812eu")) ||
           (NULL != strstr(pszDriver,"88x2eu"))
           )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Realtek");
         if ( (NULL != strstr(pszDriver,"rtl88xxau")) ||
              (NULL != strstr(pszDriver,"rtl88XXau")) )
         {
            sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
            sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_RTL88XXAU;
         }
         if ( NULL != strstr(pszDriver,"8812au") )
         {
            sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
            sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_8812AU;
         }
         if ( NULL != strstr(pszDriver,"rtl8812au") )
         {
            sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
            sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_RTL8812AU;
         }
         if ( (NULL != strstr(pszDriver,"8812eu")) || (NULL != strstr(pszDriver,"88x2eu")) )
         {
            sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
            sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_8812EU;
         }
      }
      // Experimental
      if ( NULL != strstr(pszDriver,"rtl88x2bu") )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         sRadioInfo[i].isTxCapable = 0;
         strcpy(sRadioInfo[i].szDescription, "Realtek");
         sRadioInfo[i].iRadioType = RADIO_TYPE_REALTEK;
         sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_REALTEK_RTL88X2BU;
      }

      if ( NULL != strstr(pszDriver,"rt2800usb") )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Ralink");
         sRadioInfo[i].iRadioType = RADIO_TYPE_RALINK;
         sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_RALINK;
      }
      if ( NULL != strstr(pszDriver, "mt7601u") )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Mediatek");
         sRadioInfo[i].iRadioType = RADIO_TYPE_MEDIATEK;
         sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_MEDIATEK;
      }
      if ( NULL != strstr(pszDriver, "ath9k_htc") )
      {
         s_iHwRadiosSupportedCount++;
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Atheros");
         sRadioInfo[i].iRadioType = RADIO_TYPE_ATHEROS;
         sRadioInfo[i].iRadioDriver = RADIO_HW_DRIVER_ATHEROS;
      }

      int iCopyPos = strlen(szDriver);
      while ( iCopyPos >= 0 )
      {
         if ( (szDriver[iCopyPos] == 10) || (szDriver[iCopyPos] == 13) )
            szDriver[iCopyPos] = 0;
         if ( szDriver[iCopyPos] == '/' )
            break;
         iCopyPos--;
      }
      strcpy(sRadioInfo[i].szDriver, &(szDriver[iCopyPos+1]));

      if ( ! sRadioInfo[i].isSupported )
         log_softerror_and_alarm("Found unsupported radio (%s), driver %s, skipping.", sRadioInfo[i].szName, szDriver);
      else
         log_line("[HardwareRadio] Found supported radio type: %s, driver: %s, driver string short: %s, driver string: [%s]", str_get_radio_type_description(sRadioInfo[i].iRadioType), str_get_radio_driver_description(sRadioInfo[i].iRadioDriver), sRadioInfo[i].szDriver, pszDriver);
      sRadioInfo[i].iCardModel = 0;

      for( int kk=0; kk<(int)(sizeof(sRadioInfo[i].szUSBPort)/sizeof(sRadioInfo[i].szUSBPort[0])); kk++ )
         sRadioInfo[i].szUSBPort[kk] = 0;

      for( int kk=0; kk<(int)(sizeof(sRadioInfo[i].szProductId)/sizeof(sRadioInfo[i].szProductId[0])); kk++ )
         sRadioInfo[i].szProductId[kk] = 0;

      // Find the MAC address
      sprintf(szComm, "iw dev %s info | grep addr", sRadioInfo[i].szName );
      if ( 1 != hw_execute_bash_command_raw(szComm, szBuff) )
      {
         log_softerror_and_alarm("Failed to find MAC address for %s", sRadioInfo[i].szName);
      }
      else if ( 1 != sscanf(szBuff, "%*s %s", szComm) )
      {
         log_softerror_and_alarm("Failed to find MAC address for %s", sRadioInfo[i].szName);
      }
      else
      {
         log_line("Found MAC address %s for %s", szComm, sRadioInfo[i].szName);
         szComm[MAX_MAC_LENGTH-1] = 0;
         int iSt = 0;
         int iEnd = 0;
         while ( iEnd < strlen(szComm) )
         {
            if ( szComm[iEnd] == ':' )
               iEnd++;
            else
            {
               szComm[iSt] = toupper(szComm[iEnd]);
               iSt++;
               iEnd++;
            }
         }
         szComm[iSt] = 0;
         strcpy(sRadioInfo[i].szMAC, szComm);
      }

      // Find physical interface number, in form phy#0

      sprintf(szComm, "iw dev | grep -B 1 %s", sRadioInfo[i].szName );
      if ( 1 != hw_execute_bash_command_raw(szComm, szBuff) )
      {
         sRadioInfo[i].phy_index = i;
         log_softerror_and_alarm("Failed to find physical interface index for %s", sRadioInfo[i].szName);
      }
      else if ( 1 != sscanf(szBuff, "%s", szComm) )
      {
         sRadioInfo[i].phy_index = i;
         log_softerror_and_alarm("Failed to find physical interface index for %s", sRadioInfo[i].szName);
      }
      else
      {
         log_line("phy string: [%s], length: %d", szComm, strlen(szComm));
         sRadioInfo[i].phy_index = szComm[strlen(szComm)-1] - '0';
      }

      // Check supported bands

      sRadioInfo[i].supportedBands = 0;
      sprintf(szComm, "iw phy%d info | grep 2377", sRadioInfo[i].phy_index);
      hw_execute_bash_command_raw(szComm, szBuff);
      if ( 5 < strlen(szBuff) )
        sRadioInfo[i].supportedBands |= RADIO_HW_SUPPORTED_BAND_23;

      sprintf(szComm, "iw phy%d info | grep 2427", sRadioInfo[i].phy_index);
      hw_execute_bash_command_raw(szComm, szBuff);
      if ( 5 < strlen(szBuff) )
        sRadioInfo[i].supportedBands |= RADIO_HW_SUPPORTED_BAND_24;

      sprintf(szComm, "iw phy%d info | grep 2512", sRadioInfo[i].phy_index);
      hw_execute_bash_command_raw(szComm, szBuff);
      if ( 5 < strlen(szBuff) )
        sRadioInfo[i].supportedBands |= RADIO_HW_SUPPORTED_BAND_25;

      sprintf(szComm, "iw phy%d info | grep 5745", sRadioInfo[i].phy_index);
      hw_execute_bash_command_raw(szComm, szBuff);
      if ( 5 < strlen(szBuff) )
        sRadioInfo[i].supportedBands |= RADIO_HW_SUPPORTED_BAND_58;

      char szBands[128];
      str_get_supported_bands_string(sRadioInfo[i].supportedBands, szBands);
      log_line("[HardwareRadio] Interface %s supported bands: [%s]", sRadioInfo[i].szName, szBands);
   }

   _hardware_assign_usb_from_physical_ports();
   return 1;
}

void hardware_reset_radio_enumerated_flag()
{
   s_HardwareRadiosEnumeratedOnce = 0;
}

int hardware_enumerate_radio_interfaces()
{
   return hardware_enumerate_radio_interfaces_step(-1);
}

int hardware_enumerate_radio_interfaces_step(int iStep)
{
   if ( s_HardwareRadiosEnumeratedOnce && (iStep == -1 || iStep == 0) )
      return 1;

   log_line("=================================================================");
   log_line("[HardwareRadio] Enumerating radios (step %d)...", iStep);

   if( iStep == -1 || iStep == 0 )
   {
      char szFile[MAX_FILE_PATH_SIZE];
      strcpy(szFile, FOLDER_CONFIG);
      strcat(szFile, FILE_CONFIG_CURRENT_RADIO_HW_CONFIG);
      if ( access(szFile, R_OK) != -1 )
      {
         log_line("[HardwareRadio] HW Enumerate: has radio HW info file. Using it.");
         if ( hardware_load_radio_info() )
         {
            s_HardwareRadiosEnumeratedOnce = 1;
            hardware_radio_sik_load_configuration();
            return 1;
         }
      }
      else
         log_line("[HardwareRadio] HW Enumerate: no existing radio HW info file. Enumerating HW radios from scratch.");

      s_iHwRadiosCount = 0;
   }

   if( iStep == -1 || iStep == 0 )
   {
      _hardware_enumerate_wifi_radios();
      
      if ( 0 == s_iHwRadiosCount )
         log_error_and_alarm("[HardwareRadio] No 2.4/5.8 radio modules found!");
      else
         log_line("[HardwareRadio] Found %d 2.4/5.8 radio modules.", s_iHwRadiosCount);

      s_HardwareRadiosEnumeratedOnce = 1;
   }

   if( iStep == -1 || iStep == 1 )
      hardware_radio_sik_detect_interfaces();

   log_line("[HardwareRadio] Total: %d radios found", s_iHwRadiosCount);
   log_line("=================================================================");
   
   if( iStep == -1 || iStep == 1 )
   {
      hardware_log_radio_info();
      hardware_save_radio_info();
   }
   s_HardwareRadiosEnumeratedOnce = 1;

   if ( 0 == s_iHwRadiosCount )
      return 0;
   return 1;
}

// Called only once, from ruby_start process
int hardware_radio_load_radio_modules()
{
   _hardware_find_usb_radio_interfaces_info();

   if ( 0 == s_iFoundUSBRadioInterfaces )
   {
      log_softerror_and_alarm("[HardwareRadio] No USB radio devices found!");
      return 0;
   }

   #if defined(HW_PLATFORM_RASPBERRY)
   log_line("[HardwareRadio] Adding radio modules on Raspberry for detected radio cards...");
   #endif
   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   log_line("[HardwareRadio] Adding radio modules on OpenIPC for detected radio cards...");
   #endif
   #if defined(HW_PLATFORM_RADXA_ZERO3)
   log_line("[HardwareRadio] Adding radio modules on Radxa for detected radio cards...");
   #endif


   char szOutput[1024];
   log_line("[HardwareRadio] Loading radio modules...");

   int iRTL8812AULoaded = 0;
   int iRTL8812EULoaded = 0;
   int iAtherosLoaded = 0;
   int iCountLoaded = 0;

   if ( hardware_radio_has_rtl8812au_cards() )
   {
      #if defined(HW_PLATFORM_RASPBERRY)
      hw_execute_bash_command("sudo modprobe -f 88XXau 2>&1", szOutput);
      #endif

      #if defined(HW_PLATFORM_OPENIPC_CAMERA)
      hw_execute_bash_command("modprobe 88XXau rtw_tx_pwr_idx_override=40", szOutput);
      #endif

      #if defined(HW_PLATFORM_RADXA_ZERO3)
      hw_execute_bash_command("sudo modprobe -f 88XXau_wfb", szOutput);
      #endif

      if ( (0 != szOutput[0]) && (strlen(szOutput) > 10) )
         log_softerror_and_alarm("[HardwareRadio] Error on loading driver: [%s]", szOutput);
      else
      {
         iRTL8812AULoaded = 1;
         iCountLoaded++;
      }
   }

   if ( hardware_radio_has_rtl8812eu_cards() )
   {
      log_line("[HardwareRadio] Found RTL8812EU card. Loading module...");
      #if defined(HW_PLATFORM_RASPBERRY)
      hw_execute_bash_command("sudo modprobe cfg80211", NULL);
      hw_execute_bash_command("sudo insmod /lib/modules/$(uname -r)/kernel/drivers/net/wireless/8812eu_pi.ko rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0", szOutput);
      #endif

      #if defined(HW_PLATFORM_RADXA_ZERO3)
      hw_execute_bash_command("sudo modprobe cfg80211", NULL);
      hw_execute_bash_command("sudo insmod /lib/modules/$(uname -r)/kernel/drivers/net/wireless/8812eu_radxa.ko rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0 2>&1 1>/dev/null", szOutput);
      #endif

      #if defined(HW_PLATFORM_OPENIPC_CAMERA)
      hw_execute_bash_command("modprobe cfg80211", NULL);
      hw_execute_bash_command("insmod /lib/modules/$(uname -r)/extra/8812eu.ko rtw_tx_pwr_by_rate=0 rtw_tx_pwr_lmt_enable=0 2>&1 1>/dev/null", szOutput);
      #endif

      if ( (0 != szOutput[0]) && (strlen(szOutput) > 10) )
         log_softerror_and_alarm("[HardwareRadio] Error on loading driver: [%s]", szOutput);
      else
      {
         iRTL8812EULoaded = 1;
         iCountLoaded++;    
      }
   }

   if ( hardware_radio_has_atheros_cards() )
   {
      hw_execute_bash_command("modprobe ath9k_hw txpower=10", szOutput);
      hw_execute_bash_command("modprobe ath9k_htc", szOutput);
      iAtherosLoaded = 1;
      iCountLoaded++;
   }

   log_line("[HardwareRadio] Added %d modules. Added RTL8812AU? %s. Added RTL8812EU? %s. Added Atheros? %s",
      iCountLoaded, (iRTL8812AULoaded?"yes":"no"), (iRTL8812EULoaded?"yes":"no"), (iAtherosLoaded?"yes":"no"));
   return iCountLoaded;
}

int hardware_get_radio_interfaces_count()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   return s_iHwRadiosCount;
}

int hardware_get_supported_radio_interfaces_count()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   return s_iHwRadiosSupportedCount;
}

int hardware_add_radio_interface_info(radio_hw_info_t* pRadioInfo)
{
   if ( (NULL == pRadioInfo) || (s_iHwRadiosCount >= MAX_RADIO_INTERFACES) )
      return 0;

   memcpy((u8*)&(sRadioInfo[s_iHwRadiosCount]), pRadioInfo, sizeof(radio_hw_info_t));
   s_iHwRadiosCount++;
   return 1;
}

int hardware_get_radio_index_by_name(const char* szName)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( NULL == szName || 0 == szName[0] )
      return -1;
   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( 0 == strcmp(szName, sRadioInfo[i].szName ) )
         return i;
   return -1;
}

int hardware_get_radio_index_from_mac(const char* szMAC)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( NULL == szMAC || 0 == szMAC[0] )
      return -1;
   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( 0 == strcmp(szMAC, sRadioInfo[i].szMAC ) )
         return i;
   return -1;
}


radio_hw_info_t* hardware_get_radio_info(int iRadioIndex)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( iRadioIndex < 0 || iRadioIndex >= s_iHwRadiosCount )
      return NULL;
   return &(sRadioInfo[iRadioIndex]);
}

const char* hardware_get_radio_name(int iRadioIndex)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( iRadioIndex < 0 || iRadioIndex >= s_iHwRadiosCount )
      return NULL;
   return sRadioInfo[iRadioIndex].szName;
}

const char* hardware_get_radio_description(int iRadioIndex)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( iRadioIndex < 0 || iRadioIndex >= s_iHwRadiosCount )
      return NULL;
   return sRadioInfo[iRadioIndex].szDescription;
}

int hardware_radio_has_low_capacity_links()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( ! sRadioInfo[i].isHighCapacityInterface )
         return 1;
   return 0;
}

int hardware_radio_has_rtl8812au_cards()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
   if ( 0 == s_iHwRadiosCount )
   if ( ! s_iEnumeratedUSBRadioInterfaces )
      _hardware_find_usb_radio_interfaces_info();

   int iCount = 0;

   if ( 0 < s_iHwRadiosCount )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         if ( (sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812AU) ||
              (sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL88XXAU) ||
              (sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL8812AU) ||
              (sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_REALTEK_RTL88X2BU) ||
              (sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_MEDIATEK) )
            iCount++;
      }
      return iCount;
   }

   for( int i=0; i<s_iFoundUSBRadioInterfaces; i++ )
   {
       if ( (s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_REALTEK_RTL88XXAU) ||
            (s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_REALTEK_RTL8812AU) ||
            (s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_REALTEK_8812AU) ||
            (s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_REALTEK_RTL88X2BU) ||
            (s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_MEDIATEK) )
          iCount++;
   }
   return iCount;
}

int hardware_radio_has_rtl8812eu_cards()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
   if ( 0 == s_iHwRadiosCount )
   if ( ! s_iEnumeratedUSBRadioInterfaces )
      _hardware_find_usb_radio_interfaces_info();

   int iCount = 0;

   if ( 0 < s_iHwRadiosCount )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         if ( sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_REALTEK_8812EU )
            iCount++;
      }
      return iCount;
   }

   for( int i=0; i<s_iFoundUSBRadioInterfaces; i++ )
   {
       if ( s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_REALTEK_8812EU )
          iCount++;
   }
   return iCount;
}

int hardware_radio_has_atheros_cards()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
   if ( 0 == s_iHwRadiosCount )
   if ( ! s_iEnumeratedUSBRadioInterfaces )
      _hardware_find_usb_radio_interfaces_info();

   int iCount = 0;

   if ( 0 < s_iHwRadiosCount )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         if ( sRadioInfo[i].iRadioDriver == RADIO_HW_DRIVER_ATHEROS )
            iCount++;
      }
      return iCount;
   }

   for( int i=0; i<s_iFoundUSBRadioInterfaces; i++ )
   {
       if ( s_USB_RadioInterfacesInfo[i].iDriver == RADIO_HW_DRIVER_ATHEROS )
          iCount++;
   }
   return iCount;
}


int hardware_radio_is_wifi_radio(radio_hw_info_t* pRadioInfo)
{
   if ( NULL == pRadioInfo )
      return 0;

   if ( pRadioInfo->iRadioType == RADIO_TYPE_RALINK ||
        pRadioInfo->iRadioType == RADIO_TYPE_ATHEROS ||
        pRadioInfo->iRadioType == RADIO_TYPE_REALTEK ||
        pRadioInfo->iRadioType == RADIO_TYPE_MEDIATEK )
      return 1;

   return 0;
}

int hardware_radio_is_serial_radio(radio_hw_info_t* pRadioInfo)
{
   if ( NULL == pRadioInfo )
      return 0;

   if ( pRadioInfo->isSerialRadio )
      return 1;
   return 0;
}

int hardware_radio_is_elrs_radio(radio_hw_info_t* pRadioInfo)
{
   if ( NULL == pRadioInfo )
      return 0;

   if ( ! hardware_radio_is_serial_radio(pRadioInfo) )
      return 0;
   if ( pRadioInfo->iCardModel != CARD_MODEL_SERIAL_RADIO_ELRS )
      return 0;
     
   return 1;
}

int hardware_radio_index_is_serial_radio(int iHWInterfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iHWInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return 0;
     
   return hardware_radio_is_serial_radio(pRadioHWInfo);
}

int hardware_radio_index_is_elrs_radio(int iHWInterfaceIndex)
{
   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iHWInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return 0;
     
   return hardware_radio_is_elrs_radio(pRadioHWInfo);
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

   if ( pRadioInfo->iRadioType == RADIO_TYPE_SIK )
      return 1;

   return 0;
}

int hardware_radioindex_supports_frequency(int iRadioIndex, u32 freqKhz)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( iRadioIndex < 0 || iRadioIndex >= s_iHwRadiosCount )
      return 0;

   if ( sRadioInfo[iRadioIndex].isSerialRadio )
   if ( ! sRadioInfo[iRadioIndex].isConfigurable )
   if ( sRadioInfo[iRadioIndex].uCurrentFrequencyKhz == freqKhz )
      return 1;
   int band = getBand(freqKhz);
   
   if ( band & sRadioInfo[iRadioIndex].supportedBands )
      return 1;
    
   return 0;
}

int hardware_radio_supports_frequency(radio_hw_info_t* pRadioInfo, u32 freqKhz)
{
   if ( 0 == freqKhz )
      return 0;
   if ( NULL == pRadioInfo )
      return 0;
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   int band = getBand(freqKhz);
   
   if ( band & pRadioInfo->supportedBands )
      return 1;
    
   return 0;
}

int hardware_get_radio_count_opened_for_read()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   int count = 0;
   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( sRadioInfo[i].openedForRead )
         count++;
   return count;
}

radio_hw_info_t* hardware_get_radio_info_for_usb_port(const char* szUSBPort)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( NULL == szUSBPort || 0 == szUSBPort[0] )
      return NULL;

   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( strcmp(szUSBPort, sRadioInfo[i].szUSBPort) == 0 )
         return &(sRadioInfo[i]);
   return NULL;
}

radio_hw_info_t* hardware_get_radio_info_from_mac(const char* szMAC)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( NULL == szMAC || 0 == szMAC[0] )
      return NULL;

   for( int i=0; i<s_iHwRadiosCount; i++ )
      if ( 0 == strcmp(szMAC, sRadioInfo[i].szMAC) )
         return &(sRadioInfo[i]);
   return NULL;
}
