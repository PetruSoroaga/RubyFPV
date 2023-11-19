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
#include "vehicle_settings.h"
#include "hardware.h"
#include "hw_procs.h"
#include "flags.h"

VehicleSettings s_VehicleSettings;
int s_VehicleSettingsLoaded = 0;

void reset_VehicleSettings()
{
   memset(&s_VehicleSettings, 0, sizeof(s_VehicleSettings));
   s_VehicleSettings.iDevRxLoopTimeout = DEFAULT_MAX_RX_LOOP_TIMEOUT_MILISECONDS;
   
   log_line("Reseted vehicle settings.");
}

int save_VehicleSettings()
{
   FILE* fd = fopen(FILE_VEHICLE_SETTINGS, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save vehicle settings to file: %s",FILE_VEHICLE_SETTINGS);
      return 0;
   }
   fprintf(fd, "%s\n", VEHICLE_SETTINGS_STAMP_ID);
   fprintf(fd, "%d\n", s_VehicleSettings.iDevRxLoopTimeout);
   fclose(fd);

   log_line("Saved vehicle settings to file: %s", FILE_VEHICLE_SETTINGS);
   return 1;
}

int load_VehicleSettings()
{
   s_VehicleSettingsLoaded = 1;
   FILE* fd = fopen(FILE_VEHICLE_SETTINGS, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load vehicle settings from file: %s (missing file). Resetted vehicle settings to default.",FILE_VEHICLE_SETTINGS);
      reset_VehicleSettings();
      save_VehicleSettings();
      return 0;
   }

   int failed = 0;
   char szBuff[256];
   szBuff[0] = 0;
   if ( 1 != fscanf(fd, "%s", szBuff) )
      failed = 1;
   if ( 0 != strcmp(szBuff, VEHICLE_SETTINGS_STAMP_ID) )
      failed = 2;

   if ( failed )
   {
      fclose(fd);
      log_softerror_and_alarm("Failed to load vehicle settings from file: %s (invalid config file version)",FILE_VEHICLE_SETTINGS);
      reset_VehicleSettings();
      save_VehicleSettings();
      return 0;
   }

   
   if ( (!failed) && (1 != fscanf(fd, "%d", &s_VehicleSettings.iDevRxLoopTimeout)) )
   {
      s_VehicleSettings.iDevRxLoopTimeout = DEFAULT_MAX_RX_LOOP_TIMEOUT_MILISECONDS;
      failed = 1;
   }

   fclose(fd);

   if ( failed )
   {
      log_line("Incomplete/Invalid settings file %s, error code: %d. Reseted to default.", FILE_VEHICLE_SETTINGS, failed);
      reset_VehicleSettings();
      save_VehicleSettings();
   }
   else
      log_line("Loaded vehicle settings from file: %s", FILE_VEHICLE_SETTINGS);
   return 1;
}

VehicleSettings* get_VehicleSettings()
{
   if ( ! s_VehicleSettingsLoaded )
      load_VehicleSettings();
   return &s_VehicleSettings;
}
