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
#include "hardware_radio.h"
#include "hardware_serial.h"
#include "hardware_radio_sik.h"
#include "hw_procs.h"
#include "../common/string_utils.h"

#define MAX_USB_DEVICES_INFO 16

static usb_radio_interface_info_t s_USB_RadioInterfacesInfo[MAX_USB_DEVICES_INFO];
static int s_iFoundUSBRadioInterfaces = 0;

static radio_hw_info_t sRadioInfo[MAX_RADIO_INTERFACES];
static int s_iHwRadiosCount = 0;
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
   FILE* fp = fopen(FILE_CURRENT_RADIO_HW_CONFIG, "w");
   if ( NULL == fp )
   {
      log_softerror_and_alarm("Failed to save radio HW info file.");
      return;
   }
   u8 buffer[9056];
   int pos = 0;
   memcpy(buffer+pos, &s_iHwRadiosCount, sizeof(s_iHwRadiosCount));
   pos += sizeof(s_iHwRadiosCount);
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
   if ( access( FILE_CURRENT_RADIO_HW_CONFIG, R_OK ) == -1 )
   {
      log_line("No radio HW info file.");
      return 0;
   }
   int succeeded = 1;
   u8 bufferIn[9056];
   int posIn = 0;

   FILE* fp = fopen(FILE_CURRENT_RADIO_HW_CONFIG, "r");
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

void _hardware_detect_card_model(int iIndex)
{
   if ( iIndex < 0 || iIndex > s_iHwRadiosCount )
      return;

   if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "cf3/9271" ) )
       sRadioInfo[iIndex].iCardModel = CARD_MODEL_TPLINK722N;

   //if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "cf3/9271" ) )
   //    sRadioInfo[iIndex].iCardModel = CARD_MODEL_ALFA_AWUS036NHA;

   if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "148f/3070" ) )
       sRadioInfo[iIndex].iCardModel = CARD_MODEL_ALFA_AWUS036NH;

   if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "b05/17d2" ) )
       sRadioInfo[iIndex].iCardModel = CARD_MODEL_ASUS_AC56;

   if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "846/9052" ) )
       sRadioInfo[iIndex].iCardModel = CARD_MODEL_NETGEAR_A6100;

   if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "bda/881a" ) )
       sRadioInfo[iIndex].iCardModel = CARD_MODEL_RTL8812AU_DUAL_ANTENNA;
   //if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "bda/881a" ) )
   //    sRadioInfo[iIndex].iCardModel = CARD_MODEL_ZIPRAY;

   if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "bda/8812" ) )
       sRadioInfo[iIndex].iCardModel = CARD_MODEL_ALFA_AWUS036ACH;

   if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "bda/8811" ) )
       sRadioInfo[iIndex].iCardModel = CARD_MODEL_ALFA_AWUS036ACS;

   if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "2604/12" ) )
       sRadioInfo[iIndex].iCardModel = CARD_MODEL_TENDA_U12;

   if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "2357/120" ) )
       sRadioInfo[iIndex].iCardModel = CARD_MODEL_ARCHER_T2UPLUS;

   if ( NULL != strstr( sRadioInfo[iIndex].szProductId, "bda/8813" ) )
       sRadioInfo[iIndex].iCardModel = CARD_MODEL_RTL8814AU;
}

void _hardware_assign_usb_from_physical_ports()
{
   if ( s_iHwRadiosCount <= 0 )
      return;

   log_line("[HardwareRadio] Finding USB ports for radio interfaces...");

   DIR *d;
   struct dirent *dir;
   char szComm[1024];
   char szOutput[1024];

   d = opendir("/sys/bus/usb/devices/");
   if ( NULL == d )
   {
      for( int i=0; i<s_iHwRadiosCount; i++ )
      {
         sRadioInfo[i].szUSBPort[0] = 'Y';
         sRadioInfo[i].szUSBPort[1] = '1' + i;
         sRadioInfo[i].szUSBPort[2] = 0;
      }
      log_line("[HardwareRadio] Can't find USB ports for radio interfaces.");
      return;
   }

   while ((dir = readdir(d)) != NULL)
   {
      int iLen = strlen(dir->d_name);
      if ( iLen < 3 )
         continue;

      log_line("[HardwareRadio] Quering USB device: [%s]...", dir->d_name);

      snprintf(szComm, 1023, "cat /sys/bus/usb/devices/%s/uevent 2>/dev/null | grep DRIVER", dir->d_name);
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
         sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/%s/uevent 2>/dev/null | grep DEVTYPE=wlan", dir->d_name, sRadioInfo[i].szName);
         hw_execute_bash_command_silent(szComm, szOutput);
         if ( 0 == szOutput[0] )
            continue;

         szOutput[0] = 0;
         sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/%s/uevent 2>/dev/null | grep INTERFACE", dir->d_name, sRadioInfo[i].szName);
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

         log_line("[HardwareRadio] Found USB port for radio interface %d: [%s]", i+1, dir->d_name);

         // Find the product id / vendor id

         szOutput[0] = 0;
         sprintf(szComm, "cat /sys/bus/usb/devices/%s/uevent 2>/dev/null | grep PRODUCT", dir->d_name);
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

               strncpy( sRadioInfo[i].szProductId, &(szOutput[iPos]), 12 );
               sRadioInfo[i].szProductId[11] = 0;
               _hardware_detect_card_model(i);
               log_line("[HardwareRadio] Found product/vendor id for radio interface %d: [%s], card model: %d: [%s]", i+1, sRadioInfo[i].szProductId, sRadioInfo[i].iCardModel, str_get_radio_card_model_string(sRadioInfo[i].iCardModel) );
            }
         }

         // Done finding the product id / vendor id

         // Parse the USB port info

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

void _hardware_find_usb_radio_interfaces_info()
{
   s_iFoundUSBRadioInterfaces = 0;
   log_line("[HardwareRadio]: Finding USB radio interfaces and devices...");
   char szBuff[4096];
   szBuff[0] = 0;
   hw_execute_bash_command_raw("lsusb", szBuff);

   int iLen = strlen(szBuff);
   int iPos = 0;
   while (iPos < iLen )
   {
      // Get one line
      int iEndLine = iPos;
      while ( szBuff[iEndLine] != 10 && szBuff[iEndLine] != 13 && iEndLine < iLen )
         iEndLine++;
      szBuff[iEndLine] = 0;

      if ( szBuff[iEndLine+1] == 10 || szBuff[iEndLine+1] == 13 )
      {
         iEndLine++;
         szBuff[iEndLine] = 0;
      }

      // Parse the line

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
         }
         else if ( bLookForDev )
         {
            if ( szBuff[i] < '0' || szBuff[i] > '9' )
               continue;
            int iStartNb = i;
            while ( szBuff[i] >= '0' && szBuff[i] <= '9' )
               i++;
            szBuff[i] = 0;
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].iDeviceNb = atoi(szBuff+iStartNb);
            bLookForDev = 0;
         }
         else if ( bLookForId )
         {
            if ( (szBuff[i] < '0' || szBuff[i] > '9') && szBuff[i] != ':' )
               continue;
            int iStartId = i;
            while ( (szBuff[i] >= '0' && szBuff[i] <= '9') || szBuff[i] == ':' )
               i++;
            szBuff[i] = 0;
            strncpy(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId, szBuff+iStartId, sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId)-1 );
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId[sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szProductId)-1] = 0;
            bLookForId = 0;
         }
         else
         {
            strncpy(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName, szBuff+i, sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName)-1 );
            s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName[sizeof(s_USB_RadioInterfacesInfo[s_iFoundUSBRadioInterfaces].szName)-1] = 0;
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

   log_line("[HardwareRadio]: Done finding USB devices. Found %d USB devices.", s_iFoundUSBRadioInterfaces);
}

int _hardware_enumerate_wifi_radios()
{
   char szBuff[1024];
   char szDriver[1024];
   char szComm[1024];

   FILE* fp = popen("ls /sys/class/net/ | nice grep -v eth0 | nice grep -v lo | nice grep -v usb | nice grep -v intwifi | nice grep -v relay | nice grep -v wifihotspot", "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to enumerate 2.4/5.8 radios.");
      return 0;
   }

   _hardware_find_usb_radio_interfaces_info();
   
   int iStartIndex = s_iHwRadiosCount;

   while (fgets(szBuff, 255, fp) != NULL)
   {
      sscanf(szBuff, "%s", sRadioInfo[s_iHwRadiosCount].szName);
      sRadioInfo[s_iHwRadiosCount].iCardModel = 0;
      sRadioInfo[s_iHwRadiosCount].isSupported = 0;
      sRadioInfo[s_iHwRadiosCount].isEnabled = 1;
      sRadioInfo[s_iHwRadiosCount].isTxCapable = 1;
      sRadioInfo[s_iHwRadiosCount].isHighCapacityInterface = 1;
      sRadioInfo[s_iHwRadiosCount].iCurrentDataRate = 0;
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
      sRadioInfo[s_iHwRadiosCount].szUSBPort[0] = '0';
      sRadioInfo[s_iHwRadiosCount].typeAndDriver = RADIO_TYPE_OTHER;
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

   for( int i=iStartIndex; i<s_iHwRadiosCount; i++ )
   {
      sprintf(szBuff, "cat /sys/class/net/%s/device/uevent | nice grep DRIVER | sed 's/DRIVER=//'", sRadioInfo[i].szName);
      hw_execute_bash_command(szBuff, szDriver);
      log_line("Driver for %s: %s", sRadioInfo[i].szName, szDriver);
      sRadioInfo[i].isSupported = 0;
      if ( strcmp(szDriver,"rtl88xxau") == 0 ||
           strcmp(szDriver,"8812au") == 0 ||
           strcmp(szDriver,"rtl8812au") == 0 ||
           strcmp(szDriver,"rtl88XXau") == 0 )
      {
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Realtek");
         if ( strcmp(szDriver,"rtl88xxau") == 0 ||
              strcmp(szDriver,"rtl88XXau") == 0 )
            sRadioInfo[i].typeAndDriver = RADIO_TYPE_REALTEK | (RADIO_HW_DRIVER_REALTEK_RTL88XXAU<<8);
         if ( strcmp(szDriver,"8812au") == 0)
            sRadioInfo[i].typeAndDriver = RADIO_TYPE_REALTEK | (RADIO_HW_DRIVER_REALTEK_8812AU<<8);
         if ( strcmp(szDriver,"rtl8812au") == 0 )
            sRadioInfo[i].typeAndDriver = RADIO_TYPE_REALTEK | (RADIO_HW_DRIVER_REALTEK_RTL8812AU<<8);
      }
      // Experimental
      if ( strcmp(szDriver,"rtl88x2bu") == 0 )
      {
         sRadioInfo[i].isSupported = 1;
         sRadioInfo[i].isTxCapable = 0;
         strcpy(sRadioInfo[i].szDescription, "Realtek");
         sRadioInfo[i].typeAndDriver = RADIO_TYPE_REALTEK | (RADIO_HW_DRIVER_REALTEK_RTL88X2BU<<8);
      }

      if ( strcmp(szDriver,"rt2800usb") == 0 )
      {
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Ralink");
         sRadioInfo[i].typeAndDriver = RADIO_TYPE_RALINK | (RADIO_HW_DRIVER_RALINK<<8);
      }
      if ( strcmp(szDriver, "mt7601u") == 0 )
      {
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Mediatek");
         sRadioInfo[i].typeAndDriver = RADIO_TYPE_MEDIATEK | (RADIO_HW_DRIVER_MEDIATEK<<8);
      }
      if ( strcmp(szDriver, "ath9k_htc") == 0 )
      {
         sRadioInfo[i].isSupported = 1;
         strcpy(sRadioInfo[i].szDescription, "Atheros");
         sRadioInfo[i].typeAndDriver = RADIO_TYPE_ATHEROS | (RADIO_HW_DRIVER_ATHEROS<<8);
      }

      strcpy(sRadioInfo[i].szDriver, szDriver);
      if ( ! sRadioInfo[i].isSupported )
         log_softerror_and_alarm("Found unsupported radio (%s), driver %s, skipping.", sRadioInfo[i].szName, szDriver);
      
      sRadioInfo[i].iCardModel = 0;

      for( int kk=0; kk<6; kk++ )
         sRadioInfo[i].szUSBPort[kk] = 0;

      for( int kk=0; kk<12; kk++ )
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

      // Find physical interface number
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
   log_line("[HardwreRadio]: Enumerating radios (step %d)...", iStep);

   if( iStep == -1 || iStep == 0 )
   {
      if ( access( FILE_CURRENT_RADIO_HW_CONFIG, R_OK ) != -1 )
      {
         log_line("[HardwreRadio]: HW Enumerate: has radio HW info file. Using it.");
         if ( hardware_load_radio_info() )
         {
            s_HardwareRadiosEnumeratedOnce = 1;
            hardware_radio_sik_load_configuration();
            return 1;
         }
      }
      else
         log_line("[HardwreRadio]: HW Enumerate: no existing radio HW info file. Enumerating HW radios from scratch.");

      s_iHwRadiosCount = 0;
   }

   if( iStep == -1 || iStep == 0 )
   {
      _hardware_enumerate_wifi_radios();
      
      if ( 0 == s_iHwRadiosCount )
         log_error_and_alarm("[HardwreRadio]: No 2.4/5.8 radio modules found!");
      else
         log_line("[HardwreRadio]: Found %d 2.4/5.8 radio modules.", s_iHwRadiosCount);

      s_HardwareRadiosEnumeratedOnce = 1;
   }

   if( iStep == -1 || iStep == 1 )
      hardware_radio_sik_detect_interfaces();

   log_line("[HardwreRadio]: Total: %d radios found", s_iHwRadiosCount);
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

int hardware_get_radio_interfaces_count()
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   return s_iHwRadiosCount;
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


int hardware_radio_is_wifi_radio(radio_hw_info_t* pRadioInfo)
{
   if ( NULL == pRadioInfo )
      return 0;

   u32 uType = pRadioInfo->typeAndDriver & 0xFF;
   if ( uType == RADIO_TYPE_RALINK ||
        uType == RADIO_TYPE_ATHEROS ||
        uType == RADIO_TYPE_REALTEK ||
        uType == RADIO_TYPE_MEDIATEK )
      return 1;

   return 0;
}

int hardware_radioindex_supports_frequency(int iRadioIndex, u32 freqKhz)
{
   if ( ! s_HardwareRadiosEnumeratedOnce )
      hardware_enumerate_radio_interfaces();
   if ( iRadioIndex < 0 || iRadioIndex >= s_iHwRadiosCount )
      return 0;
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


int hardware_get_radio_tx_power_atheros()
{
   if ( access( "/etc/modprobe.d/ath9k_hw.conf", R_OK ) == -1 )
   {
      log_softerror_and_alarm("Hardware: There are no Atheros radio interfaces configuration files to read radio config from in /etc/modprobe.d");
      return -1;
   }

   FILE *pF = fopen("/etc/modprobe.d/ath9k_hw.conf", "rb");
   if ( NULL == pF )
   {
      log_softerror_and_alarm("Hardware: There are no Atheros radio interfaces configuration files to read radio config from in /etc/modprobe.d");
      return -1;
   }

   char szBuff[512];
   int iTxPower = 0;

   fseek(pF, 0, SEEK_END);
   long fsize = ftell(pF);
   fseek(pF, 0, SEEK_SET);
   fread(szBuff, 1, fsize, pF);
   fclose(pF);

   if ( fsize < 10 || fsize > 256 )
   {
      log_softerror_and_alarm("Hardware: Invalid Atheros radio interfaces configuration files to read radio config from in /etc/modprobe.d");
      return -1;
   }

   szBuff[fsize] = 0;

   char* szPos = strstr(szBuff, "txpower=");
   int val = 0;
   if ( NULL != szPos )
   {
      if ( 1 != sscanf(szPos+8, "%d", &val) )
         iTxPower = 0;
      else
         iTxPower = val;
   }
   else
   {
      log_softerror_and_alarm("Hardware: Invalid Atheros radio interface configuration file to read radio config from in /etc/modprobe.d");
      return -1;
   }
   log_line("Hardware: Read Atheros radio configuration ok. Atheros radio config tx power: %d", iTxPower);
   return iTxPower;
}

int hardware_get_radio_tx_power_rtl()
{
   if ( access( "/etc/modprobe.d/rtl8812au.conf", R_OK ) == -1 )
   {
      log_softerror_and_alarm("Hardware: There are no RTL radio interfaces configuration files to read radio config from in /etc/modprobe.d");
      return -1;
   }

   FILE *pF = fopen("/etc/modprobe.d/rtl8812au.conf", "rb");
   if ( NULL == pF )
   {
      log_softerror_and_alarm("Hardware: There are no RTL radio interfaces configuration files to read radio config from in /etc/modprobe.d");
      return -1;
   }

   char szBuff[512];
   int iTxPower = 0;

   fseek(pF, 0, SEEK_END);
   long fsize = ftell(pF);
   fseek(pF, 0, SEEK_SET);
   fread(szBuff, 1, fsize, pF);
   fclose(pF);

   if ( fsize < 10 || fsize > 256 )
   {
      log_softerror_and_alarm("Hardware: Invalid RTL radio interfaces configuration files to read radio config from in /etc/modprobe.d");
      return -1;
   }

   szBuff[fsize] = 0;
   char* szPos = strstr(szBuff, "tx_pwr_idx_override=");
   int val = 0;

   if ( NULL != szPos )
   {
      if ( 1 != sscanf(szPos+20, "%d", &val) )
         iTxPower = 0;
      else
         iTxPower = val;
   }
   else
   {
      log_softerror_and_alarm("Hardware: Invalid RTL radio interface configuration file to read radio config from in /etc/modprobe.d");
      return -1;
   }
   log_line("Hardware: Read RTL radio configuration ok. RTL radio config tx power: %d", iTxPower);
   return iTxPower;
}


int hardware_set_radio_tx_power_atheros(int txPower)
{
   log_line("Setting Atheros TX Power to: %d", txPower);
   if ( txPower < 1 || txPower > MAX_TX_POWER )
      txPower = DEFAULT_RADIO_TX_POWER;

   int iHasOrgFile = 0;
   if ( access( "/etc/modprobe.d/ath9k_hw.conf.org", R_OK ) != -1 )
      iHasOrgFile = 1;
   if ( iHasOrgFile )
   {
      FILE *pF = fopen("/etc/modprobe.d/ath9k_hw.conf.org", "rb");
      if ( NULL == pF )
         iHasOrgFile = 0;
      else
      {
         fseek(pF, 0, SEEK_END);
         long fsize = ftell(pF);
         fclose(pF);
         if ( fsize < 20 || fsize > 512 )
            iHasOrgFile = 0;
      }
   }

   char szBuff[256];
   szBuff[0] = 0;
   if ( iHasOrgFile )
      sprintf(szBuff, "cp /etc/modprobe.d/ath9k_hw.conf.org tmp/ath9k_hw.conf; sed -i 's/txpower=[0-9]*/txpower=%d/g' tmp/ath9k_hw.conf; cp tmp/ath9k_hw.conf /etc/modprobe.d/", txPower);
   else if ( access( "/etc/modprobe.d/ath9k_hw.conf", R_OK ) != -1 )
      sprintf(szBuff, "cp /etc/modprobe.d/ath9k_hw.conf tmp/ath9k_hw.conf; sed -i 's/txpower=[0-9]*/txpower=%d/g' tmp/ath9k_hw.conf; cp tmp/ath9k_hw.conf /etc/modprobe.d/", txPower);
   else
      log_softerror_and_alarm("Can't access/find Atheros power config files.");

   if ( 0 != szBuff[0] )
      hw_execute_bash_command(szBuff, NULL);

   // Change power for RALINK cards too. They are 2.4Ghz only cards 
   if ( access( "/etc/modprobe.d/rt2800usb.conf", R_OK ) != -1 )
   {
      txPower = (txPower/10)-2;
      if ( txPower < 0 ) txPower = 0;
      if ( txPower > 5 ) txPower = 5;
      sprintf(szBuff, "cp /etc/modprobe.d/rt2800usb.conf tmp/; sed -i 's/txpower=[0-9]*/txpower=%d/g' tmp/rt2800usb.conf; cp tmp/rt2800usb.conf /etc/modprobe.d/", txPower );
      hw_execute_bash_command(szBuff, NULL);
   }

   int val = hardware_get_radio_tx_power_atheros();
   log_line("Atheros TX Power changed to: %d", val);
   return val;
}

int hardware_set_radio_tx_power_rtl(int txPower)
{
   log_line("Setting RTL TX Power to: %d", txPower);
   if ( txPower < 1 || txPower > MAX_TX_POWER )
      txPower = DEFAULT_RADIO_TX_POWER;

   int iHasOrgFile = 0;
   if ( access( "/etc/modprobe.d/rtl8812au.conf.org", R_OK ) != -1 )
      iHasOrgFile = 1;
   if ( iHasOrgFile )
   {
      FILE *pF = fopen("/etc/modprobe.d/rtl8812au.conf.org", "rb");
      if ( NULL == pF )
         iHasOrgFile = 0;
      else
      {
         fseek(pF, 0, SEEK_END);
         long fsize = ftell(pF);
         fclose(pF);
         if ( fsize < 20 || fsize > 512 )
            iHasOrgFile = 0;
      }
   }

   char szBuff[256];
   szBuff[0] = 0;
   if ( iHasOrgFile )
      sprintf(szBuff, "cp /etc/modprobe.d/rtl8812au.conf.org tmp/rtl8812au.conf; sed -i 's/rtw_tx_pwr_idx_override=[0-9]*/rtw_tx_pwr_idx_override=%d/g' tmp/rtl8812au.conf; cp tmp/rtl8812au.conf /etc/modprobe.d/", txPower);
   else if ( access( "/etc/modprobe.d/rtl8812au.conf", R_OK ) != -1 )
      sprintf(szBuff, "cp /etc/modprobe.d/rtl8812au.conf tmp/rtl8812au.conf; sed -i 's/rtw_tx_pwr_idx_override=[0-9]*/rtw_tx_pwr_idx_override=%d/g' tmp/rtl8812au.conf; cp tmp/rtl8812au.conf /etc/modprobe.d/", txPower);
   else
      log_softerror_and_alarm("Can't access/find RTL power config files.");

   if ( 0 != szBuff[0] )
      hw_execute_bash_command(szBuff, NULL);

   szBuff[0] = 0;
   if ( iHasOrgFile )
      sprintf(szBuff, "cp /etc/modprobe.d/rtl88XXau.conf.org tmp/rtl88XXau.conf; sed -i 's/rtw_tx_pwr_idx_override=[0-9]*/rtw_tx_pwr_idx_override=%d/g' tmp/rtl88XXau.conf; cp tmp/rtl88XXau.conf /etc/modprobe.d/", txPower);
   else if ( access( "/etc/modprobe.d/rtl88XXau.conf", R_OK ) != -1 )
      sprintf(szBuff, "cp /etc/modprobe.d/rtl88XXau.conf tmp/rtl88XXau.conf; sed -i 's/rtw_tx_pwr_idx_override=[0-9]*/rtw_tx_pwr_idx_override=%d/g' tmp/rtl88XXau.conf; cp tmp/rtl88XXau.conf /etc/modprobe.d/", txPower);
   else
      log_softerror_and_alarm("Can't access/find RTL-XX power config files.");

   if ( 0 != szBuff[0] )
      hw_execute_bash_command(szBuff, NULL);

   int val = hardware_get_radio_tx_power_rtl();
   log_line("RTL TX Power changed to: %d", val);
   return val;
}


int hardware_get_basic_radio_wifi_info(radio_info_wifi_t* pdptr)
{
   if ( NULL == pdptr )
   {
      log_softerror_and_alarm("Hardware: invalid output parameter for telemetry radio structure");
      return 0;
   }
   
   pdptr->tx_powerAtheros = hardware_get_radio_tx_power_atheros();
   pdptr->tx_powerRTL = hardware_get_radio_tx_power_rtl();

   if ( pdptr->tx_powerAtheros < 1 || pdptr->tx_powerAtheros > MAX_TX_POWER )
   {
      log_softerror_and_alarm("Invalid TX power for Atheros cards. Setting %d as default value.", DEFAULT_RADIO_TX_POWER);
      pdptr->tx_powerAtheros = DEFAULT_RADIO_TX_POWER;
      hardware_set_radio_tx_power_atheros(pdptr->tx_powerAtheros);
   }

   if ( pdptr->tx_powerRTL < 1 || pdptr->tx_powerRTL > MAX_TX_POWER )
   {
      log_softerror_and_alarm("Invalid TX power for RTL cards. Setting %d as default value.", DEFAULT_RADIO_TX_POWER);
      pdptr->tx_powerRTL = DEFAULT_RADIO_TX_POWER;
      hardware_set_radio_tx_power_rtl(pdptr->tx_powerRTL);
   }

   if ( pdptr->tx_powerRTL > 0 )
      pdptr->tx_power = pdptr->tx_powerRTL;
   else
      pdptr->tx_power = pdptr->tx_powerAtheros;
   log_line("Hardware: parsed radio interfaces config files in /etc/modeprobe.d: tx power: %d,%d,%d", pdptr->tx_power, pdptr->tx_powerAtheros, pdptr->tx_powerRTL);
   return 1;
}
