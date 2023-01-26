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

#include "local_stats.h"
#include "../base/base.h"
#include "../base/models.h"
#include "../base/hw_procs.h"
#include "shared_vars.h"
#include "pairing.h"
#include "timers.h"

u32 g_stats_uVehicleId = 0;

bool g_stats_bHomeSet = false;
double g_stats_fHomeLat = 0.0;
double g_stats_fHomeLon = 0.0;
double g_stats_fLastPosLat = 0.0;
double g_stats_fLastPosLon = 0.0;

bool s_stats_isArmed = false;
u32 s_stats_u32LastTimeSecondsCheck = 0;

void local_stats_reset_home_set()
{
   g_stats_bHomeSet = false;
   g_stats_fHomeLat = 0.0;
   g_stats_fHomeLon = 0.0;
}

void local_stats_reset(u32 uVehicleId)
{
   if ( uVehicleId == g_stats_uVehicleId )
      return;

   local_stats_reset_home_set();

   g_stats_uVehicleId = uVehicleId;

   s_stats_u32LastTimeSecondsCheck = g_TimeNow;

   s_stats_isArmed = false;

   if ( NULL != g_pCurrentModel )
   {
      g_pCurrentModel->m_Stats.uCurrentOnTime = 0;
      g_pCurrentModel->m_Stats.uCurrentFlightTime = 0; // seconds
      g_pCurrentModel->m_Stats.uCurrentFlightDistance = 0; // in 1/100 meters (cm)
      g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent = 0; // 0.1 miliAmps (1/10000 amps);

      g_pCurrentModel->m_Stats.uCurrentMaxAltitude = 0; // meters
      g_pCurrentModel->m_Stats.uCurrentMaxDistance = 0; // meters
      g_pCurrentModel->m_Stats.uCurrentMaxCurrent = 0; // miliAmps (1/1000 amps)
      g_pCurrentModel->m_Stats.uCurrentMinVoltage = 100000; // miliVolts (1/1000 volts)
   }
}

void local_stats_on_arm()
{
   if ( s_stats_isArmed )
      return;
   u32 vid = g_stats_uVehicleId;
   local_stats_reset(0);
   local_stats_reset(vid);
   s_stats_isArmed = true;
}

void local_stats_on_disarm()
{
   if ( s_stats_isArmed )
      g_pCurrentModel->m_Stats.uTotalFlights++;
   s_stats_isArmed = false;
}

void local_stats_update_loop()
{   
   if ( (NULL == g_pCurrentModel) || ( ! pairing_isStarted()) || (NULL == g_pdpfct) || (0 == g_stats_uVehicleId))
      return;

   if ( (! pairing_isReceiving()) && (! pairing_wasReceiving()) )
      return;

   if ( pairing_is_connected_to_wrong_model() )
      return;

   g_pCurrentModel->updateStatsMaxCurrentVoltage(g_pdpfct);

   // Every second

   if ( g_TimeNow < s_stats_u32LastTimeSecondsCheck + 1000 )
      return;
   s_stats_u32LastTimeSecondsCheck = g_TimeNow;

   g_pCurrentModel->updateStatsEverySecond(g_pdpfct);
}

bool load_temp_local_stats()
{
   // Load and delete the file

   if( access( FILE_TMP_CONTROLLER_LOAD_LOCAL_STATS, R_OK ) == -1 )
      return false;

   log_line("Loading temp local stats after a crash or reboot.");

   FILE* fd = fopen(FILE_TMP_CONTROLLER_LOCAL_STATS, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load temporary local stats from file: %s (missing file)",FILE_TMP_CONTROLLER_LOCAL_STATS);
      return false;
   }

   int failed = 0;
   int tmp1, tmp2;
   u32 vehicle_id = 0;

   if ( (!failed) && (2 != fscanf(fd, "%u %d", &vehicle_id, &tmp1)) )
      failed = 1;
   if ( NULL == g_pCurrentModel || vehicle_id != g_pCurrentModel->vehicle_id )
   {
      log_line("Temporary local stats file %s for a different vehicle. Ignoring it.", FILE_TMP_CONTROLLER_LOCAL_STATS);
      fclose(fd);
      char szComm[256];
      sprintf(szComm, "rm -rf %s", FILE_TMP_CONTROLLER_LOAD_LOCAL_STATS);
      hw_execute_bash_command(szComm, NULL);               
      return false;
   }
   else
   {
      g_stats_uVehicleId = vehicle_id;
      if ( (!failed) && (4 != fscanf(fd, "%d %d %u %u", &tmp1, &tmp2, &(g_pCurrentModel->m_Stats.uCurrentOnTime), &(g_pCurrentModel->m_Stats.uCurrentFlightTime))) )
         failed = 1;
      g_stats_bHomeSet = (bool)tmp1;
      s_stats_isArmed = (bool)tmp2;
      if ( (!failed) && (4 != fscanf(fd, "%f %f %f %f", &g_stats_fHomeLat, &g_stats_fHomeLon, &g_stats_fLastPosLat, &g_stats_fLastPosLon)) )
         failed = 1;
      if ( (!failed) && (4 != fscanf(fd, "%u %u %u %u", &(g_pCurrentModel->m_Stats.uCurrentMaxDistance), &(g_pCurrentModel->m_Stats.uCurrentMaxAltitude), &(g_pCurrentModel->m_Stats.uCurrentMaxCurrent), &(g_pCurrentModel->m_Stats.uCurrentMinVoltage))) )
         failed = 1;

      if ( failed )
         local_stats_reset(0);
   }

   fclose(fd);

   if ( failed )
      log_line("Incomplete/Invalid temporary local stats file %s", FILE_TMP_CONTROLLER_LOCAL_STATS);
   else
      log_line("Loaded temporary local stats from file: %s", FILE_TMP_CONTROLLER_LOCAL_STATS);

   char szComm[256];
   sprintf(szComm, "rm -rf %s", FILE_TMP_CONTROLLER_LOAD_LOCAL_STATS);
   hw_execute_bash_command(szComm, NULL);  

   if ( failed )
      return false;
   return true;             
}

void save_temp_local_stats()
{
   if ( NULL == g_pCurrentModel )
      return;

   saveAsCurrentModel(g_pCurrentModel);

   FILE* fd = fopen(FILE_TMP_CONTROLLER_LOCAL_STATS, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save temporary local stats to file: %s",FILE_TMP_CONTROLLER_LOCAL_STATS);
      return;
   }

   fprintf(fd, "%u %d\n", g_stats_uVehicleId, (int)0);
   fprintf(fd, "%d %d %u %u\n", (int)g_stats_bHomeSet, (int)s_stats_isArmed, g_pCurrentModel->m_Stats.uCurrentOnTime, g_pCurrentModel->m_Stats.uCurrentFlightTime);
   fprintf(fd, "%f %f %f %f\n", g_stats_fHomeLat, g_stats_fHomeLon, g_stats_fLastPosLat, g_stats_fLastPosLon);
   fprintf(fd, "%u %u %u %u\n", g_pCurrentModel->m_Stats.uCurrentMaxDistance, g_pCurrentModel->m_Stats.uCurrentMaxAltitude, g_pCurrentModel->m_Stats.uCurrentMaxCurrent, g_pCurrentModel->m_Stats.uCurrentMinVoltage);
   fclose(fd);
   log_line("Saved temp local stats for reboot or watchdog.");
}
