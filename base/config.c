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

#include "base.h"
#include "config.h"
#include "hardware.h"
#include "hw_procs.h"
#include "../radio/radiopackets2.h"
#include <ctype.h>

#define PREFERENCES_SETTINGS_STAMP_ID "vIV.3"
Preferences s_Preferences;

static const char* s_szBandName23 = "2.3 Ghz";
static const char* s_szBandName24 = "2.4 Ghz";
static const char* s_szBandName25 = "2.5 Ghz";
static const char* s_szBandName58 = "5.8 Ghz";
static const char* s_szBandNameNone = "";

int channels23[] = { 2312, 2317, 2322, 2327, 2332, 2337, 2342, 2347, 2352, 2357, 2362, 2367, 2372, 2377, 2382, 2387, 2392, 2397, 2402, 2407 };
int channels24[] = { 2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452, 2457, 2462, 2467, 2472, 2484 };
int channels25[] = { 2487, 2489, 2492, 2494, 2497, 2499, 2512, 2532, 2572, 2592, 2612, 2632, 2652, 2672, 2692, 2712 };
int channels58[] = { 5180, 5200, 5220, 5240, 5260, 5280, 5300, 5320, 5500, 5520, 5540, 5560, 5580, 5600, 5620, 5640, 5660, 5680, 5700, 5745, 5765, 5785, 5805, 5825, 5845, 5865, 5885 };

// in 1 Mbs increments
int dataRates[] = {2, 6, 9, 12, 18, 24, 36, 48, 54};

int* getChannels24() { return channels24; }
int getChannels24Count() { return sizeof(channels24)/sizeof(channels24[0]); }
int* getChannels23() { return channels23; }
int getChannels23Count() { return sizeof(channels23)/sizeof(channels23[0]); }
int* getChannels25() { return channels25; }
int getChannels25Count() { return sizeof(channels25)/sizeof(channels25[0]); }
int* getChannels58() { return channels58; }
int getChannels58Count() { return sizeof(channels58)/sizeof(channels58[0]); }

int getBand(int freq)
{
   if ( freq < 2412 )
      return RADIO_HW_SUPPORTED_BAND_23;
   if ( freq < 2487 )
      return RADIO_HW_SUPPORTED_BAND_24;
   if ( freq < 5000 )
      return RADIO_HW_SUPPORTED_BAND_25;
   if ( freq > 5000 )
      return RADIO_HW_SUPPORTED_BAND_58;
   return 0;
}

int getChannelIndexForFrequency(int nBand, int freq)
{
   int nChannel = -1;
   if ( nBand == RADIO_HW_SUPPORTED_BAND_23 )
      for( int i=0; i<getChannels23Count(); i++ )
         if ( freq == getChannels23()[i] )
         {
            nChannel = i;
            break;
         }

   if ( nBand == RADIO_HW_SUPPORTED_BAND_24 )
      for( int i=0; i<getChannels24Count(); i++ )
         if ( freq == getChannels24()[i] )
         {
            nChannel = i;
            break;
         }

   if ( nBand == RADIO_HW_SUPPORTED_BAND_25 )
      for( int i=0; i<getChannels25Count(); i++ )
         if ( freq == getChannels25()[i] )
         {
            nChannel = i;
            break;
         }

   if ( nBand == RADIO_HW_SUPPORTED_BAND_58 )
      for( int i=0; i<getChannels58Count(); i++ )
         if ( freq == getChannels58()[i] )
         {
            nChannel = i;
            break;
         }
   return nChannel;
}

const char* getBandName(u8 band)
{
   if ( band & RADIO_HW_SUPPORTED_BAND_23 )
      return s_szBandName23;
   if ( band & RADIO_HW_SUPPORTED_BAND_24 )
      return s_szBandName24;
   if ( band & RADIO_HW_SUPPORTED_BAND_25 )
      return s_szBandName25;
   if ( band & RADIO_HW_SUPPORTED_BAND_58 )
      return s_szBandName58;
   return s_szBandNameNone;
}

void getAllBandsNames(u8 bands, char* szBuff)
{
   szBuff[0] = 0;
   int first = 1;
   if ( bands & RADIO_HW_SUPPORTED_BAND_23 )
   {
      if ( ! first )
         strcat(szBuff, ", ");
      strcat(szBuff, s_szBandName23);
      first = 0;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_24 )
   {
      if ( ! first )
         strcat(szBuff, ", ");
      strcat(szBuff, s_szBandName24);
      first = 0;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_25 )
   {
      if ( ! first )
         strcat(szBuff, ", ");
      strcat(szBuff, s_szBandName25);
      first = 0;
   }
   if ( bands & RADIO_HW_SUPPORTED_BAND_58 )
   {
      if ( ! first )
         strcat(szBuff, ", ");
      strcat(szBuff, s_szBandName58);
      first = 0;
   }
}

int isFrequencyInBands(int freq, u8 bands)
{
   if ( freq < 2412 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_23 )
         return 1;
      return 0;
   }
   if ( freq < 2487 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_24 )
         return 1;
      return 0;
   }
   if ( freq < 5000 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_25 )
         return 1;
      return 0;
   }
   if ( freq > 5000 )
   {
      if ( bands & RADIO_HW_SUPPORTED_BAND_58 )
         return 1;
      return 0;
   }
   return 0;
}

int getSupportedChannels(u8 supportedBands, int includeSeparator, int* pOutChannels, int maxChannels)
{
   if ( NULL == pOutChannels || 0 == maxChannels )
      return 0;

   int countSupported = 0;

   if ( supportedBands & RADIO_HW_SUPPORTED_BAND_23 )
   {
      for( int i=0; i<getChannels23Count(); i++ )
      {
         *pOutChannels = getChannels23()[i];
         pOutChannels++;
         countSupported++;
         if ( countSupported >= maxChannels )
            return countSupported;
      }
      if ( includeSeparator )
      {
         *pOutChannels = -1;
         pOutChannels++;
         countSupported++;
         if ( countSupported >= maxChannels )
            return countSupported;
      }
   }

   if ( supportedBands & RADIO_HW_SUPPORTED_BAND_24 )
   {
      for( int i=0; i<getChannels24Count(); i++ )
      {
         *pOutChannels = getChannels24()[i];
         pOutChannels++;
         countSupported++;
         if ( countSupported >= maxChannels )
            return countSupported;
      }
      if ( includeSeparator )
      {
         *pOutChannels = -1;
         pOutChannels++;
         countSupported++;
         if ( countSupported >= maxChannels )
            return countSupported;
      }
   }

   if ( supportedBands & RADIO_HW_SUPPORTED_BAND_25 )
   {
      for( int i=0; i<getChannels25Count(); i++ )
      {
         *pOutChannels = getChannels25()[i];
         pOutChannels++;
         countSupported++;
         if ( countSupported >= maxChannels )
            return countSupported;
      }
      if ( includeSeparator )
      {
         *pOutChannels = -1;
         pOutChannels++;
         countSupported++;
         if ( countSupported >= maxChannels )
            return countSupported;
      }
   }

   if ( supportedBands & RADIO_HW_SUPPORTED_BAND_58 )
   {
      for( int i=0; i<getChannels58Count(); i++ )
      {
         *pOutChannels = getChannels58()[i];
         pOutChannels++;
         countSupported++;
         if ( countSupported >= maxChannels )
            return countSupported;
      }
      if ( includeSeparator )
      {
         *pOutChannels = -1;
         pOutChannels++;
         countSupported++;
         if ( countSupported >= maxChannels )
            return countSupported;
      }
   }

   return countSupported;
}

int *getDataRates() { return dataRates; }
int getDataRatesCount() { return sizeof(dataRates)/sizeof(dataRates[0]); }

int getMCSRate(int mcsIndex)
{
   if ( 0 == mcsIndex )
      return 6;
   if ( 1 == mcsIndex )
      return 13;
   if ( 2 == mcsIndex )
      return 19;
   if ( 3 == mcsIndex )
      return 26;
   if ( 4 == mcsIndex )
      return 39;
   if ( 5 == mcsIndex )
      return 52;
   if ( 6 == mcsIndex )
      return 58;
   return 58;
}

int getDataRateRealMbRate(int dataRate)
{
   if ( dataRate > 0 )
      return dataRate;
   else if ( dataRate < 0 )
      return getMCSRate(-dataRate-1);
   else
      return 0;
}

void getDataRateDescription(int dataRate, char* szOutput)
{
   if ( NULL == szOutput )
      return;
   szOutput[0] = 0;
   if ( dataRate > 0 )
      sprintf(szOutput, "%d Mb", dataRate);
   else
   {
      int mcsIndex = -dataRate-1;
      if ( mcsIndex < MAX_MCS_INDEX )
         sprintf(szOutput, "MCS-%d %d Mb", mcsIndex, getMCSRate(mcsIndex) );
      else
         sprintf(szOutput, "? Mb");
   }
}

void getSystemVersionString(char* p, u32 swversion)
{
   if ( NULL == p )
      return;
   u32 val = swversion;
   u32 major = (val >> 8) & 0xFF;
   u32 minor = val & 0xFF;
   sprintf(p, "%u.%u", major, minor);
   int len = strlen(p);

   if ( len > 2 )
   {
      if ( p[len-1] == '0' && p[len-2] != '.' )
         p[len-1] = 0;
   }

   if ( minor > 10 && ((minor % 10) != 0) )
      sprintf(p, "%u.%u.%u", major, minor/10, minor%10);
}

int config_file_get_value(const char* szPropName)
{
   char szComm[1024];
   char szOut[1024];
   char* szTmp = NULL;
   int value = 0;

   sprintf(szComm, "grep '#%s!=' /boot/config.txt", szPropName);
   hw_execute_bash_command_silent(szComm, szOut);
   if ( strlen(szOut) > 5 )
   {
      szTmp = strchr(szOut, '=');
      if ( NULL != szTmp )
         sscanf(szTmp+1, "%d", &value);
      if ( value > 0 )
         value = -value;
   }

   sprintf(szComm, "grep '%s=' /boot/config.txt", szPropName);
   hw_execute_bash_command_silent(szComm, szOut);
   if ( strlen(szOut) > 5 )
   {
      szTmp = strchr(szOut, '=');
      if ( NULL != szTmp )
         sscanf(szTmp+1, "%d", &value);
      if ( value < 0 )
         value = -value;
   }
   return value;
}


void config_file_add_value(const char* szFile, const char* szPropName, int value)
{
   char szComm[1024];
   char szOutput[1024];
   sprintf(szComm, "cat %s | grep %s", szFile, szPropName);
   hw_execute_bash_command(szComm, szOutput);
   if ( strlen(szOutput) >= strlen(szPropName) )
      return;

   u8 data[4096];
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
      return;
   int nSize = fread(data, 1, 4095, fd);
   fclose(fd);
   if ( nSize <= 0 )
      return;

   fd = fopen(szFile, "w");
   if ( NULL == fd )
      return;

   fprintf(fd, "%s=%d\n", szPropName, value);
   fwrite(data, 1, nSize, fd);
   fclose(fd);
}


void config_file_set_value(const char* szFile, const char* szPropName, int value)
{
   char szComm[1024];
   if ( value <= 0 )
   {
      sprintf(szComm, "sed -i 's/#%s!=[-0-9]*/#%s!=%d/g' %s", szPropName, szPropName, value, szFile);
      hw_execute_bash_command(szComm, NULL);

      sprintf(szComm, "sed -i 's/%s=[-0-9]*/#%s!=%d/g' %s", szPropName, szPropName, value, szFile);
      hw_execute_bash_command(szComm, NULL);
   }
   else
   {
      sprintf(szComm, "sed -i 's/#%s!=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
      hw_execute_bash_command(szComm, NULL);

      sprintf(szComm, "sed -i 's/%s=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
      hw_execute_bash_command(szComm, NULL);
   }
}

void config_file_force_value(const char* szFile, const char* szPropName, int value)
{
   char szComm[1024];
   sprintf(szComm, "sed -i 's/#%s!=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "sed -i 's/#%s=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "sed -i 's/%s!=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
   hw_execute_bash_command(szComm, NULL);

   sprintf(szComm, "sed -i 's/%s=[-0-9]*/%s=%d/g' %s", szPropName, szPropName, value, szFile);
   hw_execute_bash_command(szComm, NULL);
}


void save_simple_config_fileU(const char* fileName, u32 value)
{
   FILE* fd = fopen(fileName, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to simple configuration to file: %s",fileName);
      return;
   }
   fprintf(fd, "%u\n", value);
   fclose(fd);
   log_line("Saved value %u to file: %s", value, fileName);
}

u32 load_simple_config_fileU(const char* fileName, u32 defaultValue)
{
   u32 returnValue = defaultValue;

   FILE* fd = fopen(fileName, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load simple configuration from file: %s (missing file)",fileName);
      return defaultValue;
   }
   if ( 1 != fscanf(fd, "%u", &returnValue) )
   {
      log_softerror_and_alarm("Failed to load simple configuration from file: %s (invalid config file)",fileName);
      return defaultValue;
   }   
   
   fclose(fd);
   log_line("Loaded value %u from file: %s", returnValue, fileName);
   return returnValue;
}

void save_simple_config_fileI(const char* fileName, int value)
{
   FILE* fd = fopen(fileName, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save simple configuration to file: %s",fileName);
      return;
   }
   fprintf(fd, "%d\n", value);
   fclose(fd);
}

int load_simple_config_fileI(const char* fileName, int defaultValue)
{
   int returnValue = defaultValue;

   FILE* fd = fopen(fileName, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load simple configuration from file: %s (missing file)",fileName);
      return defaultValue;
   }
   if ( 1 != fscanf(fd, "%d", &returnValue) )
   {
      log_softerror_and_alarm("Failed to load simple configuration from file: %s (invalid config file)",fileName);
      log_line("Saving a default value (%d) and file.", defaultValue);
      save_simple_config_fileI(fileName, defaultValue);
      return defaultValue;
   }   
   
   fclose(fd);
   return returnValue;
}

void reset_Preferences()
{
   memset(&s_Preferences, 0, sizeof(s_Preferences));
   s_Preferences.iMenusStacked = 0;
   s_Preferences.iInvertColorsOSD = 0;
   s_Preferences.iInvertColorsMenu = 0;
   s_Preferences.iActionQuickButton1 = quickActionVideoRecord;
   s_Preferences.iActionQuickButton2 = quickActionCycleOSD;
   s_Preferences.iActionQuickButton3 = quickActionTakePicture;
   s_Preferences.iOSDScreenSize = 1;
   s_Preferences.iScaleOSD = 0;
   s_Preferences.iScaleAHI = 0;
   s_Preferences.iScaleMenus = 0;
   s_Preferences.iAddOSDOnScreenshots = 1;
   s_Preferences.iShowLogWindow = 0;
   s_Preferences.iMenuDismissesAlarm = 0;
   s_Preferences.iVideoDestination = prefVideoDestination_Disk;
   s_Preferences.iStartVideoRecOnArm = 0;
   s_Preferences.iStopVideoRecOnDisarm = 0;
   s_Preferences.iShowControllerCPUInfo = 1;
   s_Preferences.iShowBigRecordButton = 0;
   s_Preferences.iSwapUpDownButtons = 1;
   s_Preferences.iSwapUpDownButtonsValues = 0;

   s_Preferences.iAHIToSides = 0;
   s_Preferences.iAHIShowAirSpeed = 0;
   s_Preferences.iAHIStrokeSize = 0;

   s_Preferences.iUnits = prefUnitsMetric;

   s_Preferences.iColorOSD[0] = 255;
   s_Preferences.iColorOSD[1] = 250;
   s_Preferences.iColorOSD[2] = 220;
   s_Preferences.iColorOSD[3] = 100; // 100%
   s_Preferences.iColorOSDOutline[0] = 185;
   s_Preferences.iColorOSDOutline[1] = 185;
   s_Preferences.iColorOSDOutline[2] = 155;
   s_Preferences.iColorOSDOutline[3] = 70; // 70%
   s_Preferences.iColorAHI[0] = 208;
   s_Preferences.iColorAHI[1] = 238;
   s_Preferences.iColorAHI[2] = 214;
   s_Preferences.iColorAHI[3] = 100; // 100%
   s_Preferences.iOSDOutlineThickness = 0;
   s_Preferences.iRecordingLedAction = 2; // blink

   s_Preferences.iDebugMaxPacketSize = MAX_PACKET_PAYLOAD;
   s_Preferences.iDebugSBWS = 2;
   s_Preferences.iDebugRestartOnRadioSilence = 0;
   s_Preferences.iOSDFont = 1;
   s_Preferences.iPersistentMessages = 1;
   s_Preferences.nLogLevel = 0;
   s_Preferences.iDebugShowDevVideoStats = 0;
   s_Preferences.iDebugShowDevRadioStats = 0;
   s_Preferences.iDebugShowFullRXStats = 0;
   s_Preferences.iDebugShowVehicleVideoStats = 0;
   s_Preferences.iDebugShowVehicleVideoGraphs = 0;
   s_Preferences.iDebugShowVideoSnapshotOnDiscard = 0;
   s_Preferences.iDebugWiFiChangeDelay = DEFAULT_DELAY_WIFI_CHANGE;

   s_Preferences.iAutoExportSettings = 1;
   s_Preferences.iAutoExportSettingsWasModified = 0;

   s_Preferences.iShowProcessesMonitor = 0;
   s_Preferences.iShowCPULoad = 0;
   s_Preferences.uEnabledAlarms = 0xFFFFFFFF & (~ALARM_ID_CONTROLLER_CPU_LOOP_OVERLOAD);

}

int save_Preferences()
{
   FILE* fd = fopen(FILE_PREFERENCES, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save preferences to file: %s",FILE_PREFERENCES);
      return 0;
   }

   fprintf(fd, "%s\n", PREFERENCES_SETTINGS_STAMP_ID);
   
   fprintf(fd, "%d %d %d\n", s_Preferences.iMenusStacked, s_Preferences.iInvertColorsOSD, s_Preferences.iInvertColorsMenu); 
   fprintf(fd, "%d %d %d %d\n", s_Preferences.iOSDScreenSize, s_Preferences.iScaleOSD, s_Preferences.iScaleMenus, s_Preferences.iScaleAHI); 
   fprintf(fd, "%d %d %d\n", s_Preferences.iActionQuickButton1, s_Preferences.iActionQuickButton2, s_Preferences.iActionQuickButton3);
   fprintf(fd, "%d %d\n", s_Preferences.iAddOSDOnScreenshots, s_Preferences.iStatsToggledOff);
   fprintf(fd, "%d\n", s_Preferences.iShowLogWindow);

   fprintf(fd, "%d\n", s_Preferences.iOSDFlipVertical);
   fprintf(fd, "%d\n", s_Preferences.iMenuDismissesAlarm);

   fprintf(fd, "%d %d %d\n", s_Preferences.iVideoDestination, s_Preferences.iStartVideoRecOnArm, s_Preferences.iStopVideoRecOnDisarm);

   fprintf(fd, "%d\n", s_Preferences.iShowControllerCPUInfo);
   fprintf(fd, "%d\n", s_Preferences.iShowBigRecordButton);
   fprintf(fd, "%d %d\n", s_Preferences.iSwapUpDownButtons, s_Preferences.iSwapUpDownButtonsValues);
   fprintf(fd, "%d %d %d\n", s_Preferences.iAHIToSides, s_Preferences.iAHIShowAirSpeed, s_Preferences.iAHIStrokeSize);

   fprintf(fd, "%d\n", s_Preferences.iUnits);

   fprintf(fd, "%d %d\n", s_Preferences.iDebugMaxPacketSize, s_Preferences.iDebugSBWS);

   fprintf(fd, "%d %d %d %d\n", s_Preferences.iColorOSD[0], s_Preferences.iColorOSD[1], s_Preferences.iColorOSD[2], s_Preferences.iColorOSD[3]);
   fprintf(fd, "%d %d %d %d\n", s_Preferences.iColorAHI[0], s_Preferences.iColorAHI[1], s_Preferences.iColorAHI[2], s_Preferences.iColorAHI[3]);

   fprintf(fd, "%d\n", s_Preferences.iDebugRestartOnRadioSilence);

   fprintf(fd, "%d %d %d %d\n", s_Preferences.iColorOSDOutline[0], s_Preferences.iColorOSDOutline[1], s_Preferences.iColorOSDOutline[2], s_Preferences.iColorOSDOutline[3]);
   fprintf(fd, "%d\n", s_Preferences.iOSDOutlineThickness);
   fprintf(fd, "%d\n", s_Preferences.iOSDFont);
   fprintf(fd, "%d %d\n", s_Preferences.iRecordingLedAction, s_Preferences.iPersistentMessages);
   fprintf(fd, "%d\n", s_Preferences.nLogLevel);
   fprintf(fd, "%d %d\n", s_Preferences.iDebugShowDevVideoStats, s_Preferences.iDebugShowDevRadioStats);
   fprintf(fd, "%d\n", s_Preferences.iDebugShowFullRXStats);
   fprintf(fd, "%d\n", s_Preferences.iDebugWiFiChangeDelay);

   // Extra values

   fprintf(fd, "%d\n", s_Preferences.iDebugShowVideoSnapshotOnDiscard);
   fprintf(fd, "%d %d\n", s_Preferences.iDebugShowVehicleVideoStats, s_Preferences.iDebugShowVehicleVideoGraphs);

   fprintf(fd, "%d %d %d\n", s_Preferences.iAutoExportSettings, s_Preferences.iAutoExportSettingsWasModified, s_Preferences.iShowProcessesMonitor);

   fprintf(fd, "%u\n", s_Preferences.uEnabledAlarms);

   fprintf(fd, "%d\n", s_Preferences.iShowCPULoad);

   fclose(fd);
   log_line("Saved preferences to file: %s", FILE_PREFERENCES);
   return 1;
}

int load_Preferences()
{
   FILE* fd = fopen(FILE_PREFERENCES, "r");
   if ( NULL == fd )
   {
      reset_Preferences();
      log_softerror_and_alarm("Failed to load preferences from file: %s (missing file)",FILE_PREFERENCES);
      return 0;
   }

   int failed = 0;

   char szBuff[256];
   szBuff[0] = 0;
   if ( 1 != fscanf(fd, "%s", szBuff) )
      failed = 1;
   if ( 0 != strcmp(szBuff, PREFERENCES_SETTINGS_STAMP_ID) )
      failed = 1;

   if ( failed )
   {
      reset_Preferences();
      log_softerror_and_alarm("Failed to load preferences from file: %s (invalid config file version)",FILE_PREFERENCES);
      fclose(fd);
      return 0;
   }

   if ( 3 != fscanf(fd, "%d %d %d", &s_Preferences.iMenusStacked, &s_Preferences.iInvertColorsOSD, &s_Preferences.iInvertColorsMenu) )
      failed = 1;

   if ( 4 != fscanf(fd, "%d %d %d %d", &s_Preferences.iOSDScreenSize, &s_Preferences.iScaleOSD, &s_Preferences.iScaleMenus, &s_Preferences.iScaleAHI) )
      failed = 1;

   if ( 3 != fscanf(fd, "%d %d %d", &s_Preferences.iActionQuickButton1, &s_Preferences.iActionQuickButton2, &s_Preferences.iActionQuickButton3) )
      failed = 1;

   if ( s_Preferences.iActionQuickButton1 < 0 || s_Preferences.iActionQuickButton1 >= quickActionLast )
      s_Preferences.iActionQuickButton1 = quickActionCycleOSD;
   if ( s_Preferences.iActionQuickButton2 < 0 || s_Preferences.iActionQuickButton2 >= quickActionLast )
      s_Preferences.iActionQuickButton2 = quickActionTakePicture;
   if ( s_Preferences.iActionQuickButton3 < 0 || s_Preferences.iActionQuickButton3 >= quickActionLast )
      s_Preferences.iActionQuickButton3 = quickActionToggleAllOff;

   if ( 2 != fscanf(fd, "%d %d", &s_Preferences.iAddOSDOnScreenshots, &s_Preferences.iStatsToggledOff) )
      failed = 1;

   if ( 1 != fscanf(fd, "%d", &s_Preferences.iShowLogWindow) )
      failed = 1;

   if ( 1 != fscanf(fd, "%d", &s_Preferences.iOSDFlipVertical) )
      failed = 1;

   if ( 1 != fscanf(fd, "%d", &s_Preferences.iMenuDismissesAlarm) )
      failed = 1;

   if ( (!failed) && 3 != fscanf(fd, "%d %d %d", &s_Preferences.iVideoDestination, &s_Preferences.iStartVideoRecOnArm, &s_Preferences.iStopVideoRecOnDisarm) )
      failed = 1;

   if ( s_Preferences.iVideoDestination != 0 && s_Preferences.iVideoDestination != 1 )
      s_Preferences.iVideoDestination = 0;

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iShowControllerCPUInfo) )
      failed = 1;
   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iShowBigRecordButton) )
      failed = 1;

   if ( (!failed) && 2 != fscanf(fd, "%d %d", &s_Preferences.iSwapUpDownButtons, &s_Preferences.iSwapUpDownButtonsValues) )
      failed = 1;

   if ( (!failed) && 3 != fscanf(fd, "%d %d %d", &s_Preferences.iAHIToSides, &s_Preferences.iAHIShowAirSpeed, &s_Preferences.iAHIStrokeSize) )
      failed = 1;

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iUnits) )
      failed = 1;

   if ( (!failed) && 2 != fscanf(fd, "%d %d", &s_Preferences.iDebugMaxPacketSize, &s_Preferences.iDebugSBWS) )
      failed = 1;

   if ( (!failed) && 4 != fscanf(fd, "%d %d %d %d", &s_Preferences.iColorOSD[0], &s_Preferences.iColorOSD[1], &s_Preferences.iColorOSD[2], &s_Preferences.iColorOSD[3]) )
      failed = 1;
   if ( (!failed) && 4 != fscanf(fd, "%d %d %d %d", &s_Preferences.iColorAHI[0], &s_Preferences.iColorAHI[1], &s_Preferences.iColorAHI[2], &s_Preferences.iColorAHI[3]) )
      failed = 1;

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iDebugRestartOnRadioSilence) )
      failed = 1;

   if ( (!failed) && 4 != fscanf(fd, "%d %d %d %d", &s_Preferences.iColorOSDOutline[0], &s_Preferences.iColorOSDOutline[1], &s_Preferences.iColorOSDOutline[2], &s_Preferences.iColorOSDOutline[3]) )
      failed = 1;

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iOSDOutlineThickness) )
      failed = 1;

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.iOSDFont) )
   {
      s_Preferences.iOSDFont = 1;
      failed = 1;
   }
   if ( (!failed) && (1 != fscanf(fd, "%d", &s_Preferences.iRecordingLedAction)) )
      { s_Preferences.iRecordingLedAction = 2; failed = 1; }

   if ( (!failed) && (1 != fscanf(fd, "%d", &s_Preferences.iPersistentMessages)) )
      { s_Preferences.iPersistentMessages = 1; failed = 1; }

   if ( (!failed) && 1 != fscanf(fd, "%d", &s_Preferences.nLogLevel) )
      failed = 1;

   int bOk = 1;
   if ( bOk && 2 != fscanf(fd, "%d %d", &s_Preferences.iDebugShowDevVideoStats, &s_Preferences.iDebugShowDevRadioStats) )
   {
      s_Preferences.iDebugShowDevVideoStats = 1;
      s_Preferences.iDebugShowDevRadioStats = 1;
      bOk = 0;
   }

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iDebugShowFullRXStats) )
   {
      s_Preferences.iDebugShowFullRXStats = 0;
      bOk = 0;
   }

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iDebugWiFiChangeDelay) )
   {
      s_Preferences.iDebugShowFullRXStats = 0;
      bOk = 0;
   }

   // Extra fields

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iDebugShowVideoSnapshotOnDiscard) )
   {
      s_Preferences.iDebugShowVideoSnapshotOnDiscard = 0;
   }

   if ( bOk && 2 != fscanf(fd, "%d %d", &s_Preferences.iDebugShowVehicleVideoStats, &s_Preferences.iDebugShowVehicleVideoGraphs) )
   {
      s_Preferences.iDebugShowVehicleVideoStats = 0;
      s_Preferences.iDebugShowVehicleVideoGraphs = 0;
   }

   if ( bOk && 2 != fscanf(fd, "%d %d", &s_Preferences.iAutoExportSettings, &s_Preferences.iAutoExportSettingsWasModified) )
   {
      s_Preferences.iAutoExportSettings = 1;
      s_Preferences.iAutoExportSettingsWasModified = 0;
   }

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iShowProcessesMonitor) )
      s_Preferences.iShowProcessesMonitor = 0;

   if ( bOk && 1 != fscanf(fd, "%u", &s_Preferences.uEnabledAlarms) )
      s_Preferences.uEnabledAlarms = 0xFFFFFFFF;

   if ( bOk && 1 != fscanf(fd, "%d", &s_Preferences.iShowCPULoad) )
      s_Preferences.iShowCPULoad = 0;

   // End reading file;
   // Validate settings

   if ( s_Preferences.iColorOSD[3] == 0 )
   {
      s_Preferences.iColorOSD[0] = 255;
      s_Preferences.iColorOSD[1] = 250;
      s_Preferences.iColorOSD[2] = 220;
      s_Preferences.iColorOSD[3] = 100; // 100%
   }

   if ( s_Preferences.iColorOSDOutline[3] == 0 )
   {
      s_Preferences.iColorOSDOutline[0] = 185;
      s_Preferences.iColorOSDOutline[1] = 185;
      s_Preferences.iColorOSDOutline[2] = 155;
      s_Preferences.iColorOSDOutline[3] = 70; // 70%
   }

   if ( s_Preferences.iColorAHI[3] == 0 )
   {
      s_Preferences.iColorAHI[0] = 208;
      s_Preferences.iColorAHI[1] = 238;
      s_Preferences.iColorAHI[2] = 214;
      s_Preferences.iColorAHI[3] = 100; // 100%
   }

   if ( s_Preferences.iDebugMaxPacketSize < 100 ||
        s_Preferences.iDebugMaxPacketSize > MAX_PACKET_PAYLOAD )
      s_Preferences.iDebugMaxPacketSize = MAX_PACKET_PAYLOAD;

   if ( s_Preferences.iDebugSBWS < 2 || s_Preferences.iDebugSBWS > 16 )
      s_Preferences.iDebugSBWS = 2;

   if ( failed )
   {
      log_softerror_and_alarm("Failed to load preferences from file: %s (invalid config file)",FILE_PREFERENCES);
      fclose(fd);
      return 0;
   }
   fclose(fd);
   log_line("Loaded preferences from file: %s", FILE_PREFERENCES);
   return 1;
}

Preferences* get_Preferences()
{
   return &s_Preferences;
}


void get_Ruby_BaseVersion(int* pMajor, int* pMinor)
{
   int iMajor = 0;
   int iMinor = 0;

   char szVersion[32];
   szVersion[0] = 0;

   FILE* fd = fopen(FILE_INFO_VERSION, "r");
   if ( NULL == fd )
      fd = fopen("ruby_ver.txt", "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%s", szVersion) )
         szVersion[0] = 0;
      fclose(fd);
   }

   if ( 0 != szVersion[0] )
   {
      char* p = &szVersion[0];
      while ( *p )
      {
         if ( isdigit(*p) )
           iMajor = iMajor * 10 + ((*p)-'0');
         if ( (*p) == '.' )
           break;
         p++;
      }
      if ( 0 != *p )
      {
         p++;
         while ( *p )
         {
            if ( isdigit(*p) )
               iMinor = iMinor * 10 + ((*p)-'0');
            if ( (*p) == '.' )
              break;
            p++;
         }
      }
      if ( iMinor > 9 )
         iMinor = iMinor/10;
   }
   if ( NULL != pMajor )
      *pMajor = iMajor;
   if ( NULL != pMinor )
      *pMinor = iMinor;
}

void get_Ruby_UpdatedVersion(int* pMajor, int* pMinor)
{
   int iMajor = 0;
   int iMinor = 0;

   char szVersion[32];
   szVersion[0] = 0;

   FILE* fd = fopen(FILE_INFO_LAST_UPDATE, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%s", szVersion) )
         szVersion[0] = 0;
      fclose(fd);
   }

   if ( 0 != szVersion[0] )
   {
      char* p = &szVersion[0];
      while ( *p )
      {
         if ( isdigit(*p) )
           iMajor = iMajor * 10 + ((*p)-'0');
         if ( (*p) == '.' )
           break;
         p++;
      }
      if ( 0 != *p )
      {
         p++;
         while ( *p )
         {
            if ( isdigit(*p) )
               iMinor = iMinor * 10 + ((*p)-'0');
            if ( (*p) == '.' )
              break;
            p++;
         }
      }
      if ( iMinor > 9 )
         iMinor = iMinor/10;
   }
   if ( NULL != pMajor )
      *pMajor = iMajor;
   if ( NULL != pMinor )
      *pMinor = iMinor;
}
