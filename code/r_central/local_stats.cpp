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
#include "link_watch.h"

u32 s_stats_u32LastTimeSecondsCheck = 0;

void local_stats_reset_vehicle_index(int iVehicleIndex)
{
   if ( (iVehicleIndex < 0) || (iVehicleIndex >= MAX_CONCURENT_VEHICLES) )
      return;

   g_VehiclesRuntimeInfo[iVehicleIndex].bIsArmed = false;
   g_VehiclesRuntimeInfo[iVehicleIndex].bHomeSet = false;
   g_VehiclesRuntimeInfo[iVehicleIndex].fHomeLat = 0.0;
   g_VehiclesRuntimeInfo[iVehicleIndex].fHomeLon = 0.0;
   g_VehiclesRuntimeInfo[iVehicleIndex].fHomeLastLat = 0.0;
   g_VehiclesRuntimeInfo[iVehicleIndex].fHomeLastLon = 0.0;
   Model* pModel = findModelWithId(g_VehiclesRuntimeInfo[iVehicleIndex].uVehicleId);
   if ( NULL == pModel )
      return;
   
   pModel->m_Stats.uCurrentOnTime = 0;
   pModel->m_Stats.uCurrentFlightTime = 0; // seconds
   pModel->m_Stats.uCurrentFlightDistance = 0; // in 1/100 meters (cm)
   pModel->m_Stats.uCurrentFlightTotalCurrent = 0; // 0.1 miliAmps (1/10000 amps);

   pModel->m_Stats.uCurrentMaxAltitude = 0; // meters
   pModel->m_Stats.uCurrentMaxDistance = 0; // meters
   pModel->m_Stats.uCurrentMaxCurrent = 0; // miliAmps (1/1000 amps)
   pModel->m_Stats.uCurrentMinVoltage = 100000; // miliVolts (1/1000 volts)
}

void local_stats_reset_all()
{
   log_line("Reseted all local stats for all vehicles.");
   
   s_stats_u32LastTimeSecondsCheck = g_TimeNow;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      local_stats_reset_vehicle_index(i);
}

void local_stats_on_arm(u32 uVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
      {
         local_stats_reset_vehicle_index(i);
         g_VehiclesRuntimeInfo[i].bIsArmed = true;
         break;
      }
   }
}

void local_stats_on_disarm(u32 uVehicleId)
{
   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( g_VehiclesRuntimeInfo[i].uVehicleId == uVehicleId )
      {
         if ( g_VehiclesRuntimeInfo[i].bIsArmed )
         if ( NULL != g_VehiclesRuntimeInfo[i].pModel )
            g_VehiclesRuntimeInfo[i].pModel->m_Stats.uTotalFlights++;
         g_VehiclesRuntimeInfo[i].bIsArmed = false;
         break;
      }
   }
}

void local_stats_update_loop()
{   
   if ( (NULL == g_pCurrentModel) || (! pairing_isStarted()) )
      return;

   if ( ! g_bIsRouterReady )
      return;
   if ( ! link_has_received_main_vehicle_ruby_telemetry() )
      return;


   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if (! g_VehiclesRuntimeInfo[i].bGotFCTelemetry )
         continue;
      if ( (0 == g_VehiclesRuntimeInfo[i].uVehicleId) || (MAX_U32 == g_VehiclesRuntimeInfo[i].uVehicleId) )
         continue;
      if ( NULL == g_VehiclesRuntimeInfo[i].pModel )
         continue;

      if ( g_VehiclesRuntimeInfo[i].bIsArmed )
         g_VehiclesRuntimeInfo[i].pModel->updateStatsMaxCurrentVoltage(&(g_VehiclesRuntimeInfo[i].headerFCTelemetry));

      // Every second

      if ( g_TimeNow >= s_stats_u32LastTimeSecondsCheck + 1000 )
         g_VehiclesRuntimeInfo[i].pModel->updateStatsEverySecond(&(g_VehiclesRuntimeInfo[i].headerFCTelemetry));
   }
   
   if ( g_TimeNow >= s_stats_u32LastTimeSecondsCheck + 1000 )
      s_stats_u32LastTimeSecondsCheck = g_TimeNow;
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
      char szComm[256];
      sprintf(szComm, "rm -rf %s 2>&1", FILE_TMP_CONTROLLER_LOAD_LOCAL_STATS);
      hw_execute_bash_command(szComm, NULL);  
      return false;
   }

   int failed = 0;
   int tmp1, tmp2;
   u32 uVehicleId = 0;
   u32 u1, u2, u3, u4;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      if ( (!failed) && (3 != fscanf(fd, "%u %d %d", &uVehicleId, &tmp1, &tmp2)) )
         failed = 1;
      if ( !failed )
      {
         g_VehiclesRuntimeInfo[i].bIsArmed = tmp1;
         g_VehiclesRuntimeInfo[i].bHomeSet = tmp2;
      }

      if ( (!failed) && (4 != fscanf(fd, "%f %f %f %f", &g_VehiclesRuntimeInfo[i].fHomeLat, &g_VehiclesRuntimeInfo[i].fHomeLon, &g_VehiclesRuntimeInfo[i].fHomeLastLat, &g_VehiclesRuntimeInfo[i].fHomeLastLon)) )
         failed = 1;

      Model* pModel = findModelWithId(uVehicleId);
      if ( NULL == pModel )
      {
         if ( (!failed) && (2 != fscanf(fd, "%u %u", &u1, &u2)) )
            failed = 1;
         if ( (!failed) && (4 != fscanf(fd, "%u %u %u %u", &u1, &u2, &u3, &u4)) )
            failed = 1;
      }
      else
      {
         if ( (!failed) && (2 != fscanf(fd, "%u %u", &(pModel->m_Stats.uCurrentOnTime), &(pModel->m_Stats.uCurrentFlightTime))) )
            failed = 1;
         if ( (!failed) && (4 != fscanf(fd, "%u %u %u %u", &(pModel->m_Stats.uCurrentMaxDistance), &(pModel->m_Stats.uCurrentMaxAltitude), &(pModel->m_Stats.uCurrentMaxCurrent), &(pModel->m_Stats.uCurrentMinVoltage))) )
            failed = 1;
      }
      
      if ( failed )
      {
         local_stats_reset_all();
         break;
      }
   }

   fclose(fd);

   if ( failed )
      log_line("Incomplete/Invalid temporary local stats file %s", FILE_TMP_CONTROLLER_LOCAL_STATS);
   else
      log_line("Loaded temporary local stats from file: %s", FILE_TMP_CONTROLLER_LOCAL_STATS);

   char szComm[256];
   sprintf(szComm, "rm -rf %s 2>&1", FILE_TMP_CONTROLLER_LOAD_LOCAL_STATS);
   hw_execute_bash_command(szComm, NULL);  

   if ( failed )
      return false;
   return true;             
}

void save_temp_local_stats()
{
   if ( NULL == g_pCurrentModel )
      return;

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
      saveControllerModel(g_VehiclesRuntimeInfo[i].pModel);

   FILE* fd = fopen(FILE_TMP_CONTROLLER_LOCAL_STATS, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save temporary local stats to file: %s",FILE_TMP_CONTROLLER_LOCAL_STATS);
      return;
   }

   for( int i=0; i<MAX_CONCURENT_VEHICLES; i++ )
   {
      Model* pModel = g_VehiclesRuntimeInfo[i].pModel;
      
      fprintf(fd, "%u %d %d\n", g_VehiclesRuntimeInfo[i].uVehicleId, (int)g_VehiclesRuntimeInfo[i].bIsArmed, (int)g_VehiclesRuntimeInfo[i].bHomeSet);
      fprintf(fd, "%f %f %f %f\n", g_VehiclesRuntimeInfo[i].fHomeLat, g_VehiclesRuntimeInfo[i].fHomeLon, g_VehiclesRuntimeInfo[i].fHomeLastLat, g_VehiclesRuntimeInfo[i].fHomeLastLon);
      if ( NULL == pModel )
      {
         fprintf(fd, "%u %u\n", 0, 0);
         fprintf(fd, "%u %u %u %u\n", 0, 0, 0, 0);
      }
      else
      {
         fprintf(fd, "%u %u\n", pModel->m_Stats.uCurrentOnTime, pModel->m_Stats.uCurrentFlightTime);
         fprintf(fd, "%u %u %u %u\n", pModel->m_Stats.uCurrentMaxDistance, pModel->m_Stats.uCurrentMaxAltitude, pModel->m_Stats.uCurrentMaxCurrent, pModel->m_Stats.uCurrentMinVoltage);
      }
   }
   fclose(fd);
   log_line("Saved temp local stats for reboot or watchdog.");
}
