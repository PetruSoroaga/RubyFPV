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
#include "models.h"
#include <stdlib.h>
#include "config.h"
#include "hardware.h"
#include "hw_procs.h"
#include "hardware_i2c.h"
#include "utils.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"

#define MODEL_FILE_STAMP_ID "vIII.4stamp"


Model* spModelsSpectator[MAX_MODELS_SPECTATOR];
int sModelsSpectatorCount = 0;

Model* spModels[MAX_MODELS];
int sModelsCount = 0;

static const char* s_szModelFlightModeNONE = "NONE";
static const char* s_szModelFlightModeMAN  = "MAN";
static const char* s_szModelFlightModeSTAB = "STAB";
static const char* s_szModelFlightModeALTH = "ALTH";
static const char* s_szModelFlightModeLOIT = "LOIT";
static const char* s_szModelFlightModeRTL  = "RTL";
static const char* s_szModelFlightModeLAND = "LAND";
static const char* s_szModelFlightModePOSH = "POSH";
static const char* s_szModelFlightModeTUNE = "TUNE";
static const char* s_szModelFlightModeACRO = "ACRO";
static const char* s_szModelFlightModeFBWA = "FBWA";
static const char* s_szModelFlightModeFBWB = "FBWB";
static const char* s_szModelFlightModeCRUS = "CRUS";
static const char* s_szModelFlightModeTKOF = "TKOF";
static const char* s_szModelFlightModeRATE = "RATE";
static const char* s_szModelFlightModeHORZ = "HORZ";
static const char* s_szModelFlightModeCIRC = "CIRC";
static const char* s_szModelFlightModeAUTO = "AUTO";
static const char* s_szModelFlightModeQSTAB = "QSTB";
static const char* s_szModelFlightModeQHOVER = "QHVR";
static const char* s_szModelFlightModeQLOITER = "QLTR";
static const char* s_szModelFlightModeQLAND = "QLND";
static const char* s_szModelFlightModeQRTL = "QRTL";

static const char* s_szModelFlightModeNONE2 = "None";
static const char* s_szModelFlightModeMAN2  = "Manual";
static const char* s_szModelFlightModeSTAB2 = "Stabilize";
static const char* s_szModelFlightModeALTH2 = "Altitude Hold";
static const char* s_szModelFlightModeLOIT2 = "Loiter";
static const char* s_szModelFlightModeRTL2  = "Return to Launch";
static const char* s_szModelFlightModeLAND2 = "Land";
static const char* s_szModelFlightModePOSH2 = "Position Hold";
static const char* s_szModelFlightModeTUNE2 = "Auto Tune";
static const char* s_szModelFlightModeACRO2 = "Acrobatic";
static const char* s_szModelFlightModeFBWA2 = "Fly by Wire A";
static const char* s_szModelFlightModeFBWB2 = "Fly by Wire B";
static const char* s_szModelFlightModeCRUS2 = "Cruise";
static const char* s_szModelFlightModeTKOF2 = "Take Off";
static const char* s_szModelFlightModeRATE2 = "Rate";
static const char* s_szModelFlightModeHORZ2 = "Horizon";
static const char* s_szModelFlightModeCIRC2 = "Circle";
static const char* s_szModelFlightModeAUTO2 = "Auto";
static const char* s_szModelFlightModeQSTAB2 = "Q-Stabilize";
static const char* s_szModelFlightModeQHOVER2 = "Q-Hover";
static const char* s_szModelFlightModeQLOITER2 = "Q-Loiter";
static const char* s_szModelFlightModeQLAND2 = "Q-Land";
static const char* s_szModelFlightModeQRTL2 = "Q-Return To Launch";


static const char* s_szModelCameraProfile = "Normal";
static const char* s_szModelCameraProfile1 = "A";
static const char* s_szModelCameraProfile2 = "B";
static const char* s_szModelCameraProfile3 = "HDMI";

const char* model_getShortFlightMode(u8 mode)
{
   if ( mode == FLIGHT_MODE_MANUAL )  return s_szModelFlightModeMAN;
   if ( mode == FLIGHT_MODE_STAB )    return s_szModelFlightModeSTAB;
   if ( mode == FLIGHT_MODE_ALTH )    return s_szModelFlightModeALTH;
   if ( mode == FLIGHT_MODE_LOITER )  return s_szModelFlightModeLOIT;
   if ( mode == FLIGHT_MODE_RTL )     return s_szModelFlightModeRTL;
   if ( mode == FLIGHT_MODE_LAND )    return s_szModelFlightModeLAND;
   if ( mode == FLIGHT_MODE_POSHOLD ) return s_szModelFlightModePOSH;
   if ( mode == FLIGHT_MODE_AUTOTUNE )return s_szModelFlightModeTUNE;
   if ( mode == FLIGHT_MODE_ACRO )    return s_szModelFlightModeACRO;
   if ( mode == FLIGHT_MODE_FBWA )    return s_szModelFlightModeFBWA;
   if ( mode == FLIGHT_MODE_FBWB )    return s_szModelFlightModeFBWB;
   if ( mode == FLIGHT_MODE_CRUISE )  return s_szModelFlightModeCRUS;
   if ( mode == FLIGHT_MODE_TAKEOFF ) return s_szModelFlightModeTKOF;
   if ( mode == FLIGHT_MODE_RATE )    return s_szModelFlightModeRATE;
   if ( mode == FLIGHT_MODE_HORZ )    return s_szModelFlightModeHORZ;
   if ( mode == FLIGHT_MODE_CIRCLE )  return s_szModelFlightModeCIRC;
   if ( mode == FLIGHT_MODE_AUTO )    return s_szModelFlightModeAUTO;
   if ( mode == FLIGHT_MODE_QSTAB )   return s_szModelFlightModeQSTAB;
   if ( mode == FLIGHT_MODE_QHOVER )  return s_szModelFlightModeQHOVER;
   if ( mode == FLIGHT_MODE_QLOITER ) return s_szModelFlightModeQLOITER;
   if ( mode == FLIGHT_MODE_QLAND )   return s_szModelFlightModeQLAND;
   if ( mode == FLIGHT_MODE_QRTL )    return s_szModelFlightModeQRTL;
   return s_szModelFlightModeNONE;
}

const char* model_getLongFlightMode(u8 mode)
{
   if ( mode == FLIGHT_MODE_MANUAL )  return s_szModelFlightModeMAN2;
   if ( mode == FLIGHT_MODE_STAB )    return s_szModelFlightModeSTAB2;
   if ( mode == FLIGHT_MODE_ALTH )    return s_szModelFlightModeALTH2;
   if ( mode == FLIGHT_MODE_LOITER )  return s_szModelFlightModeLOIT2;
   if ( mode == FLIGHT_MODE_RTL )     return s_szModelFlightModeRTL2;
   if ( mode == FLIGHT_MODE_LAND )    return s_szModelFlightModeLAND2;
   if ( mode == FLIGHT_MODE_POSHOLD ) return s_szModelFlightModePOSH2;
   if ( mode == FLIGHT_MODE_AUTOTUNE )return s_szModelFlightModeTUNE2;
   if ( mode == FLIGHT_MODE_ACRO )    return s_szModelFlightModeACRO2;
   if ( mode == FLIGHT_MODE_FBWA )    return s_szModelFlightModeFBWA2;
   if ( mode == FLIGHT_MODE_FBWB )    return s_szModelFlightModeFBWB2;
   if ( mode == FLIGHT_MODE_CRUISE )  return s_szModelFlightModeCRUS2;
   if ( mode == FLIGHT_MODE_TAKEOFF ) return s_szModelFlightModeTKOF2;
   if ( mode == FLIGHT_MODE_RATE )    return s_szModelFlightModeRATE2;
   if ( mode == FLIGHT_MODE_HORZ )    return s_szModelFlightModeHORZ2;
   if ( mode == FLIGHT_MODE_CIRCLE )  return s_szModelFlightModeCIRC2;
   if ( mode == FLIGHT_MODE_AUTO )    return s_szModelFlightModeAUTO2;
   if ( mode == FLIGHT_MODE_QSTAB )   return s_szModelFlightModeQSTAB2;
   if ( mode == FLIGHT_MODE_QHOVER )  return s_szModelFlightModeQHOVER2;
   if ( mode == FLIGHT_MODE_QLOITER ) return s_szModelFlightModeQLOITER2;
   if ( mode == FLIGHT_MODE_QLAND )   return s_szModelFlightModeQLAND2;
   if ( mode == FLIGHT_MODE_QRTL )    return s_szModelFlightModeQRTL2;
   return s_szModelFlightModeNONE2;
}

const char* model_getCameraProfileName(int profileIndex)
{
   if ( profileIndex == 0 ) return s_szModelCameraProfile1;
   if ( profileIndex == 1 ) return s_szModelCameraProfile2;
   if ( profileIndex == 2 ) return s_szModelCameraProfile3;
   return s_szModelCameraProfile;
}

bool hasModel(u32 uVehicleId)
{
   for( int i=0; i < sModelsSpectatorCount; i++ )
      if ( NULL != spModelsSpectator[i] )
      if ( spModelsSpectator[i]->vehicle_id == uVehicleId )
         return true;

   for( int i=0; i < sModelsCount; i++ )
      if ( NULL != spModels[i] )
      if ( spModels[i]->vehicle_id == uVehicleId )
         return true;

   return false;
}

int getModelsSpectatorCount() { return sModelsSpectatorCount; }

Model* getModelSpectator(int index)
{
   if ( index < 0 || index > sModelsSpectatorCount )
      return NULL;
   return spModelsSpectator[index];
}

Model* addModelSpectator(u32 vehicleId)
{
   int index = 0;
   for( ; index < sModelsSpectatorCount; index++ )
      if ( spModelsSpectator[index]->vehicle_id == vehicleId )
      {
         // move it to top of the list;
         Model* tmp = spModelsSpectator[index];
         for( int i=index-1; i >=0; i-- )
            if (i >=0 )
               spModelsSpectator[i+1] = spModelsSpectator[i];
         spModelsSpectator[0] = tmp;
         return spModelsSpectator[0];
      }

   // New vehicle, add it on top of the list, move the other ones down the list.
   for( int i=sModelsSpectatorCount-1; i >= 0; i-- )
   {
      if ( i < MAX_MODELS_SPECTATOR-1 )
         spModelsSpectator[i+1] = spModelsSpectator[i];
   }
   spModelsSpectator[0] = new Model();
   spModelsSpectator[0]->resetToDefaults(false);
   spModelsSpectator[0]->vehicle_id = vehicleId;

   sModelsSpectatorCount++;
   if ( sModelsSpectatorCount > MAX_MODELS_SPECTATOR )
      sModelsSpectatorCount = MAX_MODELS_SPECTATOR;

   return spModelsSpectator[0];
}

void moveModelSpectatorToTop(int index)
{
   if ( index < 0 || index >= sModelsSpectatorCount )
      return;
        
   Model* tmp = spModelsSpectator[index];
   for( int i=index-1; i >=0; i-- )
      if (i >=0 )
         spModelsSpectator[i+1] = spModelsSpectator[i];
   spModelsSpectator[0] = tmp;
}

bool loadModelsSpectator()
{
   sModelsSpectatorCount = 0;
   while( sModelsSpectatorCount < MAX_MODELS_SPECTATOR )
   {
      spModelsSpectator[sModelsSpectatorCount] = new Model();
      char szBuff[256];
      sprintf(szBuff, FILE_VEHICLE_SPECTATOR, sModelsSpectatorCount);
      if( access( szBuff, R_OK ) != -1 )
      {   
         if ( ! spModelsSpectator[sModelsSpectatorCount]->loadFromFile(szBuff, true) )
            break;
      }
      else
         break;
      sModelsSpectatorCount++;
   }
   log_line("Loaded %d spectator models.", sModelsSpectatorCount);
   return true;
}

bool saveModelsSpectator()
{
   for( int i=0; i<sModelsSpectatorCount; i++ )
   {
      char szBuff[256];
      sprintf(szBuff, FILE_VEHICLE_SPECTATOR, i);
      spModelsSpectator[i]->saveToFile(szBuff, true);
   }
   return true;
}


int getModelsCount() { return sModelsCount; }

Model* getModelAtIndex(int index) 
{
   if ( index < 0 || index >= MAX_MODELS )
      return NULL;
   return spModels[index];
}

void deleteAllModels()
{
   sModelsSpectatorCount = 0;
   sModelsCount = 0;
}

Model* addNewModel()
{
   if ( sModelsCount >= MAX_MODELS-1 )
      return NULL;
   log_line("Adding a new model in the models list...");
   sModelsCount++;
   spModels[sModelsCount-1] = new Model();
   spModels[sModelsCount-1]->resetToDefaults(true);
   log_line("Added a new model in the models list.");
   return spModels[sModelsCount-1];
}

void replaceModel(int index, Model* pModel)
{
   if ( index < 0 || index >= MAX_MODELS-1 )
      return;
   spModels[index] = pModel;
}

Model* findModelWithId(u32 vehicle_id)
{
   for( int i=0; i<sModelsCount; i++ )
      if ( spModels[i]->vehicle_id == vehicle_id )
         return spModels[i];

   return NULL;
}

void deleteModel(Model* pModel)
{
   if ( NULL == pModel )
   {
      log_softerror_and_alarm("Tried to delete a NULL model object.");
      return;
   }
   if ( 0 == sModelsCount )
   {
      log_softerror_and_alarm("Tried to delete a model when there are none in the list.");
      return;
   }
   int pos = 0;
   for( pos=0; pos<sModelsCount; pos++ )
      if ( spModels[pos]->vehicle_id == pModel->vehicle_id )
      {
         for( int i=pos; i<sModelsCount-1; i++ )
            spModels[i] = spModels[i+1];
         sModelsCount--;
         saveModels();
         unlink(FILE_CURRENT_VEHICLE_MODEL);
         unlink(FILE_CURRENT_VEHICLE_MODEL_BACKUP);
         log_line("Deleted current vehicle model file: %s", FILE_CURRENT_VEHICLE_MODEL);
         return;
      }

   log_softerror_and_alarm("Tried to delete a model that is not in the list.");
}

void loadModels()
{
   int count = load_simple_config_fileI(FILE_CURRENT_VEHICLE_COUNT, 0);
   log_line("Loading %d control models...", count);
   sModelsCount = 0;
   for( int i=0; i<count; i++ )
   {
      spModels[i] = new Model();
      char szFile[256];
      sprintf(szFile, FILE_VEHICLE_CONTROLLER, i);
      if ( ! spModels[i]->loadFromFile(szFile, true) )
         break;
      sModelsCount++;
   }
   log_line("Loaded %d controller models.", sModelsCount);
}

void saveModels()
{
   log_line("Saving control models. Count: %d", sModelsCount);
   save_simple_config_fileI(FILE_CURRENT_VEHICLE_COUNT, sModelsCount);
   for( int i=0; i<sModelsCount; i++ )
   {
      char szFile[256];
      sprintf(szFile, FILE_VEHICLE_CONTROLLER, i);
      spModels[i]->saveToFile(szFile, true);
   }
}

void saveModel(Model* pModel)
{
   if ( NULL == pModel )
      return;
   for( int i=0; i<sModelsCount; i++ )
   {
      if ( spModels[i]->vehicle_id == pModel->vehicle_id )
      if ( spModels[i]->is_spectator == pModel->is_spectator )
      {
         log_line("Found matching controller vehicle in list.");
         char szFile[256];
         sprintf(szFile, FILE_VEHICLE_CONTROLLER, i);
         pModel->saveToFile(szFile, true);
         spModels[i]->loadFromFile(szFile);
      }
   }

   for( int i=0; i<sModelsSpectatorCount; i++ )
   {
      if ( spModelsSpectator[i]->vehicle_id == pModel->vehicle_id )
      if ( spModelsSpectator[i]->is_spectator == pModel->is_spectator )
      {
         log_line("Found matching spectator vehicle in list.");
         char szFile[256];
         sprintf(szFile, FILE_VEHICLE_SPECTATOR, i);
         pModel->saveToFile(szFile, true);
         spModelsSpectator[i]->loadFromFile(szFile);
      }
   }
}

void saveAsCurrentModel(Model* pModel)
{
   if ( NULL == pModel )
      return;
   log_line("Saving model id %u as current model", pModel->vehicle_id);
   pModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, true);
   saveModel(pModel);
}

Model::Model(void)
{
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      memset((u8*)&(video_link_profiles[i]), 0, sizeof(type_video_link_profile));

   memset((u8*)&video_params, 0, sizeof(video_parameters_t));
   memset((u8*)&camera_params, 0, sizeof(type_camera_parameters));
   memset((u8*)&osd_params, 0, sizeof(osd_parameters_t));
   memset((u8*)&rc_params, 0, sizeof(rc_parameters_t));
   memset((u8*)&telemetry_params, 0, sizeof(telemetry_parameters_t));
   memset((u8*)&audio_params, 0, sizeof(audio_parameters_t));
   memset((u8*)&functions_params, 0, sizeof(type_functions_parameters));

   memset((u8*)&hardware_info, 0, sizeof(model_hardware_info_t));
   memset((u8*)&radioInterfacesParams, 0, sizeof(type_radio_interfaces_parameters));
   memset((u8*)&radioLinksParams, 0, sizeof(type_radio_links_parameters));

   memset((u8*)&m_Stats, 0, sizeof(type_vehicle_stats_info));

   iSaveCount = 0;
   b_mustSyncFromVehicle = false;
   resetCameraToDefaults(-1);
   vehicle_name[0] = 0;
   vehicle_long_name[0] = 0;
   iLoadedFileVersion = 0;
   radioInterfacesParams.interfaces_count = 0;

   constructLongName();

   m_Stats.uCurrentOnTime = 0; // seconds
   m_Stats.uCurrentFlightTime = 0; // seconds
   m_Stats.uCurrentFlightDistance = 0; // in 1/100 meters (cm)
   m_Stats.uCurrentFlightTotalCurrent = 0; // miliAmps (1/1000 amps);

   m_Stats.uCurrentTotalCurrent = 0; // miliAmps (1/1000 amps);
   m_Stats.uCurrentMaxAltitude = 0; // meters
   m_Stats.uCurrentMaxDistance = 0; // meters
   m_Stats.uCurrentMaxCurrent = 0; // miliAmps (1/1000 amps)
   m_Stats.uCurrentMinVoltage = 100000; // miliVolts (1/1000 volts)

   m_Stats.uTotalFlights = 0; // count
   m_Stats.uTotalOnTime = 0;  // seconds
   m_Stats.uTotalFlightTime = 0; // seconds
   m_Stats.uTotalFlightDistance = 0; // in 1/100 meters (cm)

   m_Stats.uTotalTotalCurrent = 0; // miliAmps (1/1000 amps);
   m_Stats.uTotalMaxAltitude = 0; // meters
   m_Stats.uTotalMaxDistance = 0; // meters
   m_Stats.uTotalMaxCurrent = 0; // miliAmps (1/1000 amps)
   m_Stats.uTotalMinVoltage = 100000; // miliVolts (1/1000 volts)
}

int Model::getLoadedFileVersion()
{
   return iLoadedFileVersion;
}

bool Model::reloadIfChanged(bool bLoadStats)
{
   FILE* fd = fopen(FILE_CURRENT_VEHICLE_MODEL, "r");
   if ( NULL == fd )
      return false;

   int iV, iS = 0;
   if ( 1 == fscanf(fd, "%*s %d", &iV) )
   if ( 1 == fscanf(fd, "%*s %d", &iS) )
   if ( iS != iSaveCount )
   {
      fclose(fd);
      return loadFromFile(FILE_CURRENT_VEHICLE_MODEL, bLoadStats);
   }
   fclose(fd);
   return true;
}

bool Model::loadFromFile(const char* filename, bool bLoadStats)
{
   char szFileNormal[1024];
   char szFileBackup[1024];

   strcpy(szFileNormal, filename);
   strcpy(szFileBackup, filename);
   szFileBackup[strlen(szFileBackup)-3] = 'b';
   szFileBackup[strlen(szFileBackup)-2] = 'a';
   szFileBackup[strlen(szFileBackup)-1] = 'k';

   bool bMainFileLoadedOk = false;
   bool bBackupFileLoadedOk = false;

   type_vehicle_stats_info stats;
   memcpy((u8*)&stats, (u8*)&m_Stats, sizeof(type_vehicle_stats_info));

   u32 timeStart = get_current_timestamp_ms();

   int iVersionMain = 0;
   int iVersionBackup = 0;
   FILE* fd = fopen(szFileNormal, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%*s %d", &iVersionMain) )
      {
         log_softerror_and_alarm("Load model: Error on version line. Invalid vehicle configuration file: %s", szFileNormal);
         bMainFileLoadedOk = false;
      }
      else
      {
         //log_line("Found model file version: %d.", iVersion);
         if ( 8 == iVersionMain )
            bMainFileLoadedOk = loadVersion8(szFileNormal, fd);
         if ( bMainFileLoadedOk )
         {
            iLoadedFileVersion = iVersionMain;
         }
         else
            log_softerror_and_alarm("Invalid vehicle configuration file: %s",szFileNormal);
      }
      fclose(fd);
   }
   else
      bMainFileLoadedOk = false;

   if ( bMainFileLoadedOk )
   {
      if ( ! bLoadStats ) 
         memcpy((u8*)&m_Stats, (u8*)&stats, sizeof(type_vehicle_stats_info));
      validate_settings();

      timeStart = get_current_timestamp_ms() - timeStart;
      log_line("Loaded vehicle successfully (%u ms) from file: %s; version %d, save count: %d, vehicle name: [%s], vehicle id: %u, software: %d.%d (b%d), is in control mode: %s, is in developer mode: %s, %d radio links, 1st link: %d Mhz, 2nd link: %d Mhz", timeStart, filename, iLoadedFileVersion, iSaveCount, vehicle_name, vehicle_id, (sw_version >> 8) & 0xFF, sw_version & 0xFF, sw_version>>16, is_spectator?"no (is spectator)":"yes", (bDeveloperMode?"yes":"no"), radioLinksParams.links_count, radioLinksParams.link_frequency[0], radioLinksParams.link_frequency[1]);

      constructLongName();
      return true;
   }


   fd = fopen(szFileBackup, "r");
   if ( NULL != fd )
   {
      if ( 1 != fscanf(fd, "%*s %d", &iVersionBackup) )
      {
         log_softerror_and_alarm("Load model: Error on version line. Invalid vehicle configuration file: %s", szFileBackup);
         bBackupFileLoadedOk = false;
      }
      else
      {
         //log_line("Found model file version: %d.", iVersion);
         if ( 8 == iVersionBackup )
            bBackupFileLoadedOk = loadVersion8(szFileBackup, fd);
         if ( bBackupFileLoadedOk )
         {
            iLoadedFileVersion = iVersionBackup;
         }
         else
            log_softerror_and_alarm("Invalid vehicle configuration file: %s",szFileBackup);
      }
      fclose(fd);
   }
   else
      bBackupFileLoadedOk = false;

   if ( !bBackupFileLoadedOk )
   {
      if ( ! bLoadStats ) 
         memcpy((u8*)&m_Stats, (u8*)&stats, sizeof(type_vehicle_stats_info));
      log_softerror_and_alarm("Failed to load vehicle configuration from file: %s (missing file, missing backup)",filename);
      resetToDefaults(true);
      return false;
   }

   if ( ! bLoadStats ) 
      memcpy((u8*)&m_Stats, (u8*)&stats, sizeof(type_vehicle_stats_info));
   validate_settings();

   timeStart = get_current_timestamp_ms() - timeStart;
   log_line("Loaded vehicle successfully (%d ms) from backup file: %s; version %d, save count: %d, vehicle name: [%s], vehicle id: %u, software: %d.%d (b%d), is in control mode: %s", timeStart, filename, iLoadedFileVersion, iSaveCount, vehicle_name, vehicle_id, (sw_version >> 8) & 0xFF, sw_version & 0xFF, sw_version>>16, is_spectator?"no (is spectator)":"yes");

   constructLongName();
   
   fd = fopen(szFileNormal, "w");
   if ( NULL != fd )
   {
      saveVersion8(szFileNormal, fd, false);
      fclose(fd);
      log_line("Restored main model file from backup model file.");
   }
   else
      log_softerror_and_alarm("Failed to write main model file from backup model file.");

   return true;
}


bool Model::loadVersion8(const char* szFile, FILE* fd)
{
   char szBuff[256];
   int tmp1 = 0, tmp2 = 0, tmp3 = 0, tmp4 = 0, tmp5 = 0, tmp6 = 0, tmp7 = 0;
   u32 tmp32 = 0;
   u32 u1 = 0, u2 = 0, u3 = 0, u4 = 0, u5 = 0, u6 = 0, u7 = 0;
   int vt = 0;

   bool bOk = true;

   if ( 1 != fscanf(fd, "%s", szBuff) )
      { log_softerror_and_alarm("Load model8: Error on line stamp"); return false; }

   if ( 1 != fscanf(fd, "%*s %d", &iSaveCount) )
      { log_softerror_and_alarm("Load model8: Error on line save count"); return false; }

   if ( 4 != fscanf(fd, "%*s %u %d %d %d", &sw_version, &vehicle_id, &controller_id, &board_type) )
      { log_softerror_and_alarm("Load model8: Error on line 1"); return false; }

   if ( hardware_is_vehicle() )
      sw_version = (SYSTEM_SW_VERSION_MAJOR * 256 + SYSTEM_SW_VERSION_MINOR) | (SYSTEM_SW_BUILD_NUMBER<<16);

   if ( 1 != fscanf(fd, "%s", vehicle_name) )
      { log_softerror_and_alarm("Load model8: Error on line 2"); return false; }
   if ( vehicle_name[0] == '*' && vehicle_name[1] == 0 )
      vehicle_name[0] = 0;

   for( int i=0; i<(int)strlen(vehicle_name); i++ )
      if ( vehicle_name[i] == '_' )
         vehicle_name[i] = ' ';

   str_sanitize_modelname(vehicle_name);

   if ( 3 != fscanf(fd, "%d %u %d", &clock_sync_type, &camera_rc_channels, &niceTelemetry ) )
      { log_softerror_and_alarm("Load model8: Error on line 3"); return false; }

   if ( 4 != fscanf(fd, "%d %d %u %d", &tmp1, &vt, &m_Stats.uTotalFlightTime, &iGPSCount ) )
      { log_softerror_and_alarm("Load model8: Error on line 3b"); return false; }

   is_spectator = (bool)tmp1;
   vehicle_type = vt;

   //----------------------------------------
   // CPU

   if ( 3 != fscanf(fd, "%*s %d %d %d", &niceVideo, &niceOthers, &ioNiceVideo) )
      { log_softerror_and_alarm("Load model8: Error on line 4"); return false; }
   if ( 3 != fscanf(fd, "%d %d %d", &iOverVoltage, &iFreqARM, &iFreqGPU) )
      { log_softerror_and_alarm("Load model8: Error on line 5"); }
   if ( 3 != fscanf(fd, "%d %d %d", &niceRouter, &ioNiceRouter, &niceRC) )
      { log_softerror_and_alarm("Load model8: Error on extra line 2b"); }

   //----------------------------------------
   // Radio

   if ( 1 != fscanf(fd, "%*s %d", &radioInterfacesParams.interfaces_count) )
      { log_softerror_and_alarm("Load model8: Error on line 6"); return false; }

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      char szTmp[256];
      if ( 3 != fscanf(fd, "%d %d %d", &(radioInterfacesParams.interface_card_model[i]), &(radioInterfacesParams.interface_link_id[i]), &(radioInterfacesParams.interface_current_frequency[i])) )
         { log_softerror_and_alarm("Load model8: Error on line 7a"); return false; }

      if ( 8 != fscanf(fd, "%u %d %u %d %d %d %s %s", &u4, &tmp2, &tmp32, &tmp5, &tmp6, &tmp7, radioInterfacesParams.interface_szMAC[i], szTmp) )
         { log_softerror_and_alarm("Load model8: Error on line 7b"); return false; }
      radioInterfacesParams.interface_capabilities_flags[i] = u4;
      radioInterfacesParams.interface_supported_bands[i] = (u8)tmp2;
      radioInterfacesParams.interface_type_and_driver[i] = tmp32;
      radioInterfacesParams.interface_current_radio_flags[i] = tmp5;
      radioInterfacesParams.interface_datarates[i][0] = tmp6;
      radioInterfacesParams.interface_datarates[i][1] = tmp7;
      if ( radioInterfacesParams.interface_datarates[i][0] < -7 || radioInterfacesParams.interface_datarates[i][0] > 54 )
      {
         log_softerror_and_alarm("Load model8: Invalid radio interfaces data rates. Reseting to default.");
         radioInterfacesParams.interface_datarates[i][0] = 0;
      }
      if ( radioInterfacesParams.interface_datarates[i][1] < -7 || radioInterfacesParams.interface_datarates[i][1] > 54 )
      {
         log_softerror_and_alarm("Load model8: Invalid radio interfaces  data rates. Reseting to default.");
         radioInterfacesParams.interface_datarates[i][1] = 0;
      }
      radioInterfacesParams.interface_szMAC[i][strlen(radioInterfacesParams.interface_szMAC[i])-1] = 0;
      szTmp[strlen(szTmp)-1] = 0;
      szTmp[2] = 0;
      strcpy(radioInterfacesParams.interface_szPort[i], szTmp);      
   }

   if ( 9 != fscanf(fd, "%d %d %d %d %d %d %d %d %d", &tmp1, &radioInterfacesParams.txPower, &radioInterfacesParams.txPowerAtheros, &radioInterfacesParams.txPowerRTL, &radioInterfacesParams.txMaxPower, &radioInterfacesParams.txMaxPowerAtheros, &radioInterfacesParams.txMaxPowerRTL,  &radioInterfacesParams.slotTime, &radioInterfacesParams.thresh62) )
      { log_softerror_and_alarm("Load model8: Error on line 8"); return false; }
   enableDHCP = (bool)tmp1;

   if ( 1 != fscanf(fd, "%*s %d", &radioLinksParams.links_count) )
      { log_softerror_and_alarm("Load model8: Error on line r0"); return false; }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      if ( 5 != fscanf(fd, "%d %u %u %d %d", &(radioLinksParams.link_frequency[i]), &(radioLinksParams.link_capabilities_flags[i]), &(radioLinksParams.link_radio_flags[i]), &(radioLinksParams.link_datarates[i][0]), &(radioLinksParams.link_datarates[i][1])) )
         { log_softerror_and_alarm("Load model8: Error on line r3"); return false; }

      if ( 4 != fscanf(fd, "%d %u %d %d", &tmp1, &(radioLinksParams.uplink_radio_flags[i]), &(radioLinksParams.uplink_datarates[i][0]), &(radioLinksParams.uplink_datarates[i][1])) )
         { log_softerror_and_alarm("Load model8: Error on line r4"); return false; }
      radioLinksParams.bUplinkSameAsDownlink[i] = (tmp1!=0)?1:0;
   }

   for( unsigned int j=0; j<(sizeof(radioLinksParams.dummy)/sizeof(radioLinksParams.dummy[0])); j++ )
   {
      if ( 1 != fscanf(fd, "%d", &tmp1) )
         { log_softerror_and_alarm("Load model8: Error on line radio4 dummy data"); return false; }
      radioLinksParams.dummy[j] = tmp1;
   }

   if ( 4 != fscanf(fd, "%*s %d %d %u %u", &tmp1, &tmp2, &relay_params.relayVehicleId, &relay_params.relayFlags) )
      { log_softerror_and_alarm("Load model8: Error on line 10"); return false; }
   relay_params.is_RelayEnabled = (bool)tmp1;
   relay_params.is_Relaying = (bool)tmp2;

   //----------------------------------------
   // Telemetry

   if ( 5 != fscanf(fd, "%*s %d %d %d %d %d", &telemetry_params.fc_telemetry_type, &telemetry_params.controller_telemetry_type, &telemetry_params.update_rate, &tmp1, &tmp2) )
      { log_softerror_and_alarm("Load model8: Error on line 12"); return false; }

   if ( telemetry_params.update_rate > 200 )
      telemetry_params.update_rate = 200;
   telemetry_params.bControllerHasOutputTelemetry = (bool) tmp1;
   telemetry_params.bControllerHasInputTelemetry = (bool) tmp2;

   if ( 4 != fscanf(fd, "%d %u %d %u", &telemetry_params.dummy1, &telemetry_params.dummy2, &telemetry_params.dummy3, &telemetry_params.dummy4) )
      { log_softerror_and_alarm("Load model8: Error on line 13"); return false; }

   if ( 3 != fscanf(fd, "%d %d %d", &telemetry_params.vehicle_mavlink_id, &telemetry_params.controller_mavlink_id, &telemetry_params.flags) )
      { log_softerror_and_alarm("Load mode8: Error on line 13b"); return false; }

   if ( 2 != fscanf(fd, "%d %u", &telemetry_params.dummy5, &telemetry_params.dummy6) )
      { telemetry_params.dummy5 = -1; }

   if ( 1 != fscanf(fd, "%d", &tmp1) ) // not used
      { log_softerror_and_alarm("Load model8: Error on line 13b"); return false; }

   //----------------------------------------
   // Video

   if ( 4 != fscanf(fd, "%*s %d %d %d %u", &video_params.user_selected_video_link_profile, &video_params.iH264Slices, &tmp2, &video_params.lowestAllowedAdaptiveVideoBitrate) )
      { log_softerror_and_alarm("Load model8: Error on line 19"); return false; }

   if ( video_params.iH264Slices < 1 || video_params.iH264Slices > 16 )
      video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES;

   video_params.videoAdjustmentStrength = tmp2;
   if ( video_params.lowestAllowedAdaptiveVideoBitrate < 250000 )
      video_params.lowestAllowedAdaptiveVideoBitrate = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;

   if ( 1 != fscanf(fd, "%u", &video_params.uMaxAutoKeyframeInterval) )
      { log_softerror_and_alarm("Load model8: Error on line 20"); return false; }
   if ( video_params.uMaxAutoKeyframeInterval < 50 || video_params.uMaxAutoKeyframeInterval > 20000 )
       video_params.uMaxAutoKeyframeInterval = DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL;

   if ( 1 != fscanf(fd, "%u", &video_params.uExtraFlags) )
      { log_softerror_and_alarm("Load model8: Error on line 20c"); return false; }
   
   for( unsigned int j=0; j<(sizeof(video_params.dummy)/sizeof(video_params.dummy[0])); j++ )
   {
      if ( 1 != fscanf(fd, "%d", &tmp1) )
         { log_softerror_and_alarm("Load model8: Error on line 27, video params dummy data"); return false; }
      video_params.dummy[j] = tmp1;
   }

   if ( bOk && (1 != fscanf(fd, "%*s %d", &tmp7)) )
      { log_softerror_and_alarm("Load model8: Error: missing count of video link profiles"); bOk = false; tmp7=0; }

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      { video_link_profiles[i].flags = 0; video_link_profiles[i].encoding_extra_flags = 0; }

   if ( tmp7 < 0 || tmp7 > MAX_VIDEO_LINK_PROFILES )
      tmp7 = MAX_VIDEO_LINK_PROFILES;

   for( int i=0; i<tmp7; i++ )
   {
      if ( ! bOk )
        { log_softerror_and_alarm("Load model8: Error on video link profiles 0"); bOk = false; }
 
      if ( bOk && (6 != fscanf(fd, "%u %u %u %d %d %u", &(video_link_profiles[i].flags), &(video_link_profiles[i].encoding_extra_flags), &(video_link_profiles[i].bitrate_fixed_bps), &(video_link_profiles[i].radio_datarate_video), &(video_link_profiles[i].radio_datarate_data), &(video_link_profiles[i].radio_flags))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 1"); bOk = false; }

      if ( bOk && (2 != fscanf(fd, "%d %d", &(video_link_profiles[i].width), &(video_link_profiles[i].height))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 2"); bOk = false; }

      if ( bOk && (5 != fscanf(fd, "%d %d %d %d %d", &(video_link_profiles[i].block_packets), &(video_link_profiles[i].block_fecs), &(video_link_profiles[i].packet_length), &(video_link_profiles[i].fps), &(video_link_profiles[i].keyframe))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 3"); bOk = false; }
      
      if ( bOk && (5 != fscanf(fd, "%d %d %d %d %d", &(video_link_profiles[i].h264profile), &(video_link_profiles[i].h264level), &(video_link_profiles[i].h264refresh), &(video_link_profiles[i].h264quantization), &tmp1)) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 4"); bOk = false; }
      video_link_profiles[i].insertPPS = (tmp1!=0)?1:0;
   }


   //----------------------------------------
   // Camera params

   if ( 2 != fscanf(fd, "%*s %d %d", &iCameraCount, &iCurrentCamera) )
      { log_softerror_and_alarm("Load model8: Error on line 20"); return false; }

   for( int k=0; k<MODEL_MAX_CAMERAS; k++ )
   {

      // Camera type:
      if ( 3 != fscanf(fd, "%*s %d %d %d", &(camera_params[k].iCameraType), &(camera_params[k].iForcedCameraType), &(camera_params[k].iCurrentProfile)) )
         { log_softerror_and_alarm("Load model8: Error on line camera 2"); return false; }

      //----------------------------------------
      // Camera sensor name

      char szTmp[1024];
      if ( bOk && (1 != fscanf(fd, "%*s %s", szTmp)) )
         { log_softerror_and_alarm("Load model8: Error on line camera name"); camera_params[k].szCameraName[0] = 0; bOk = false; }
      else
      {
         szTmp[MAX_CAMERA_NAME_LENGTH-1] = 0;
         strcpy(camera_params[k].szCameraName, szTmp);
         camera_params[k].szCameraName[MAX_CAMERA_NAME_LENGTH-1] = 0;

         if ( camera_params[k].szCameraName[0] == '*' && camera_params[k].szCameraName[1] == 0 )
            camera_params[k].szCameraName[0] = 0;
         for( int i=0; i<(int)strlen(camera_params[k].szCameraName); i++ )
            if ( camera_params[k].szCameraName[i] == '*' )
               camera_params[k].szCameraName[i] = ' ';
      }

      for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
      {
         if ( 1 != fscanf(fd, "%*s %d", &camera_params[k].profiles[i].flags) )
            { log_softerror_and_alarm("Load model8: Error on line 21, cam profile %d", i); return false; }
         if ( 1 != fscanf(fd, "%d", &tmp1) )
            { log_softerror_and_alarm("Load model8: Error on line 21b, cam profile %d", i); return false; }
         camera_params[k].profiles[i].flip_image = (bool)tmp1;

         if ( 4 != fscanf(fd, "%d %d %d %d", &tmp1, &tmp2, &tmp3, &tmp4) )
            { log_softerror_and_alarm("Load model8: Error on line 22, cam profile %d", i); return false; }
         camera_params[k].profiles[i].brightness = tmp1;
         camera_params[k].profiles[i].contrast = tmp2;
         camera_params[k].profiles[i].saturation = tmp3;
         camera_params[k].profiles[i].sharpness = tmp4;

         if ( 4 != fscanf(fd, "%d %d %d %d", &tmp1, &tmp2, &tmp3, &tmp4) )
            { log_softerror_and_alarm("Load model8: Error on line 23, cam profile %d", i); return false; }
         camera_params[k].profiles[i].exposure = tmp1;
         camera_params[k].profiles[i].whitebalance = tmp2;
         camera_params[k].profiles[i].metering = tmp3;
         camera_params[k].profiles[i].drc = tmp4;

         if ( 3 != fscanf(fd, "%f %f %f", &(camera_params[k].profiles[i].analogGain), &(camera_params[k].profiles[i].awbGainB), &(camera_params[k].profiles[i].awbGainR)) )
            { log_softerror_and_alarm("Load model8: Error on line 24, cam profile %d", i); }

         if ( 2 != fscanf(fd, "%f %f", &(camera_params[k].profiles[i].fovH), &(camera_params[k].profiles[i].fovV)) )
            { log_softerror_and_alarm("Load model8: Error on line 25, cam profile %d", i); }

         if ( 4 != fscanf(fd, "%d %d %d %d", &tmp1, &tmp2, &tmp3, &tmp4) )
            { log_softerror_and_alarm("Load model8: Error on line 26, cam profile %d", i); return false; }
         camera_params[k].profiles[i].vstab = tmp1;
         camera_params[k].profiles[i].ev = tmp2;
         camera_params[k].profiles[i].iso = tmp3;
         camera_params[k].profiles[i].shutterspeed = tmp4;

         if ( 1 != fscanf(fd, "%d", &tmp1) )
            { log_softerror_and_alarm("Load model8: Error on line 25b, cam profile %d", i); }
         camera_params[k].profiles[i].wdr = (u8)tmp1;
         for( unsigned int j=0; j<(sizeof(camera_params[k].profiles[i].dummy)/sizeof(camera_params[k].profiles[i].dummy[0])); j++ )
         {
            if ( 1 != fscanf(fd, "%d", &tmp1) )
               { log_softerror_and_alarm("Load model8: Error on line 27, cam profile %d", i); return false; }
            camera_params[k].profiles[i].dummy[j] = tmp1;
         }
      }

   }

   //----------------------------------------
   // Audio Settings

   audio_params.has_audio_device = false;
   if ( 1 == fscanf(fd, "%*s %d", &tmp1) )
      audio_params.has_audio_device = (bool)tmp1;
   else
      { bOk = false; log_softerror_and_alarm("Load model8: Error on audio line 1"); }
   if ( 4 == fscanf(fd, "%d %d %d %u", &tmp2, &audio_params.volume, &audio_params.quality, &audio_params.flags) )
   {
      audio_params.has_audio_device = tmp1;
      audio_params.enabled = tmp2;
   }
   else
   {
      bOk = false;
      log_softerror_and_alarm("Load model8: Error on audio line 2");
   }
   
   if ( 1 != fscanf(fd, "%*s %u", &alarms) )
      alarms = 0;

   //----------------------------------------
   // Hardware info

   if ( 4 == fscanf(fd, "%*s %d %d %d %d", &hardware_info.radio_interface_count, &hardware_info.i2c_bus_count, &hardware_info.i2c_device_count, &hardware_info.serial_bus_count) )
   {
      if ( hardware_info.i2c_bus_count < 0 || hardware_info.i2c_bus_count > MAX_MODEL_I2C_BUSSES )
         { log_softerror_and_alarm("Load model8: Error on hw info1"); return false; }
      if ( hardware_info.i2c_device_count < 0 || hardware_info.i2c_device_count > MAX_MODEL_I2C_DEVICES )
         { log_softerror_and_alarm("Load model8: Error on hw info2"); return false; }
      if ( hardware_info.serial_bus_count < 0 || hardware_info.serial_bus_count > MAX_MODEL_SERIAL_BUSSES )
         { log_softerror_and_alarm("Load model8: Error on hw info3"); return false; }

      for( int i=0; i<hardware_info.i2c_bus_count; i++ )
         if ( 1 != fscanf(fd, "%d", &(hardware_info.i2c_bus_numbers[i])) )
            { log_softerror_and_alarm("Load model8: Error on hw info4"); return false; }

      for( int i=0; i<hardware_info.i2c_device_count; i++ )
         if ( 2 != fscanf(fd, "%d %d", &(hardware_info.i2c_devices_bus[i]), &(hardware_info.i2c_devices_address[i])) )
            { log_softerror_and_alarm("Load model8: Error on hw info5"); return false; }

      for( int i=0; i<hardware_info.serial_bus_count; i++ )
         if ( 2 != fscanf(fd, "%u %s", &(hardware_info.serial_bus_supported_and_usage[i]), &(hardware_info.serial_bus_names[i][0])) )
            { log_softerror_and_alarm("Load model8: Error on hw info6"); return false; }
   }
   else
   {
      hardware_info.radio_interface_count = 0;
      hardware_info.i2c_bus_count = 0;
      hardware_info.i2c_device_count = 0;
      hardware_info.serial_bus_count = 0;
      bOk = false;
   }

   //----------------------------------------
   // OSD

   if ( 5 != fscanf(fd, "%*s %d %d %f %d %d", &osd_params.layout, &tmp1, &osd_params.voltage_alarm, &tmp2, &tmp3) )
      { log_softerror_and_alarm("Load model8: Error on line 29"); return false; }
   osd_params.voltage_alarm_enabled = (bool)tmp1;
   osd_params.altitude_relative = (bool)tmp2;
   osd_params.show_gps_position = (bool)tmp3;

   if ( 5 != fscanf(fd, "%d %d %d %d %d", &osd_params.battery_show_per_cell,  &osd_params.battery_cell_count, &osd_params.battery_capacity_percent_alarm, &tmp1, &osd_params.home_arrow_rotate) )
      { log_softerror_and_alarm("Load model8: Error on line 30"); return false; }
   osd_params.invert_home_arrow = (bool)tmp1;

   if ( 7 != fscanf(fd, "%d %d %d %d %d %d %d", &tmp1, &tmp2, &tmp3, &tmp4, &tmp5, &tmp6, &osd_params.ahi_warning_angle) )
      { log_softerror_and_alarm("Load model8: Error on line 31"); return false; }

   osd_params.show_overload_alarm = (bool)tmp1;
   osd_params.show_stats_rx_detailed = (bool)tmp2;
   osd_params.show_stats_decode = (bool)tmp3;
   osd_params.show_stats_rc = (bool)tmp4;
   osd_params.show_full_stats = (bool)tmp5;
   osd_params.show_instruments = (bool)tmp6;
   if ( osd_params.ahi_warning_angle < 0 ) osd_params.ahi_warning_angle = 0;
   if ( osd_params.ahi_warning_angle > 80 ) osd_params.ahi_warning_angle = 80;

   for( int i=0; i<5; i++ )
      if ( 4 != fscanf(fd, "%u %u %u %u", &(osd_params.osd_flags[i]), &(osd_params.osd_flags2[i]), &(osd_params.instruments_flags[i]), &(osd_params.osd_preferences[i])) )
         { bOk = false; log_softerror_and_alarm("Load model8: Error on osd params os flags line 2"); }

   //----------------------------------------
   // RC

   if ( 4 != fscanf(fd, "%*s %d %d %d %d", &tmp1, &tmp2, &rc_params.receiver_type, &rc_params.rc_frames_per_second ) )
      { log_softerror_and_alarm("Load model8: Error on line 34"); return false; }
   rc_params.rc_enabled = tmp1;
   rc_params.dummy1 = tmp2;

   if ( rc_params.receiver_type >= RECEIVER_TYPE_LAST || rc_params.receiver_type < 0 )
      rc_params.receiver_type = RECEIVER_TYPE_BUILDIN;
   if ( rc_params.rc_frames_per_second < 2 || rc_params.rc_frames_per_second > 200 )
      rc_params.rc_frames_per_second = DEFAULT_RC_FRAMES_PER_SECOND;

   if ( 1 != fscanf(fd, "%d", &rc_params.inputType) )
      { log_softerror_and_alarm("Load model8: Error on line 35"); return false; }

   if ( 4 != fscanf(fd, "%d %ld %d %ld", &rc_params.inputSerialPort, &rc_params.inputSerialPortSpeed, &rc_params.outputSerialPort, &rc_params.outputSerialPortSpeed ) )
      { log_softerror_and_alarm("Load model8: Error on line 36"); return false; }
   
   for( int i=0; i<MAX_RC_CHANNELS; i++ )
   {
      if ( 7 != fscanf(fd, "%u %u %u %u %u %u %u", &u1, &u2, &u3, &u4, &u5, &u6, &u7) )
         { log_softerror_and_alarm("Load model8: Error on line 38"); return false; }
      rc_params.rcChAssignment[i] = u1;
      rc_params.rcChMid[i] = u2;
      rc_params.rcChMin[i] = u3;
      rc_params.rcChMax[i] = u4;
      rc_params.rcChFailSafe[i] = u5;
      rc_params.rcChExpo[i] = u6;
      rc_params.rcChFlags[i] = u7;
   }
   
   if ( 3 != fscanf(fd, "%d %u %u", &rc_params.rc_failsafe_timeout_ms, &rc_params.failsafeFlags, &rc_params.channelsCount ) )
      { log_softerror_and_alarm("Load model8: Error on line 37a"); return false; }
   if ( 1 != fscanf(fd, "%u", &rc_params.hid_id ) )
      { log_softerror_and_alarm("Load model8: Error on line 37b"); return false; }

   if ( 1 != fscanf(fd, "%u", &rc_params.flags ) )
      { log_softerror_and_alarm("Load model8: Error on line 37c"); return false; }

   if ( 1 != fscanf(fd, "%u", &rc_params.rcChAssignmentThrotleReverse ) )
      { log_softerror_and_alarm("Load model8: Error on line 37d"); return false; }

   for( unsigned int i=0; i<(sizeof(rc_params.dummy)/sizeof(rc_params.dummy[0])); i++ )
      if ( 1 != fscanf(fd, "%u", &(rc_params.dummy[i])) )
         { log_softerror_and_alarm("Load model8: Error on line 37"); return false; }

   if ( rc_params.rc_failsafe_timeout_ms < 50 || rc_params.rc_failsafe_timeout_ms > 5000 )
      rc_params.rc_failsafe_timeout_ms = DEFAULT_RC_FAILSAFE_TIME;

   //----------------------------------------
   // Misc

   if ( 2 != fscanf(fd, "%*s %d %u", &tmp1, &uDeveloperFlags) )
      { log_softerror_and_alarm("Load model8: Error on line 39"); bDeveloperMode = false; uDeveloperFlags = (((u32)DEFAULT_DELAY_WIFI_CHANGE)<<8); }
   else
      bDeveloperMode = (bool)tmp1;

   if ( 1 != fscanf(fd, "%u", &enc_flags) )
   {
      enc_flags = 0;
      log_softerror_and_alarm("Load model8: Error on line extra 3");
   }

   if ( bOk && (1 != fscanf(fd, "%*s %u", &m_Stats.uTotalFlights)) )
   {
      log_softerror_and_alarm("Load model8: error on stats1");
      m_Stats.uTotalFlights = 0;
      bOk = false;
   }
   if ( bOk && (4 != fscanf(fd, "%u %u %u %u", &m_Stats.uCurrentOnTime, &m_Stats.uCurrentFlightTime, &m_Stats.uCurrentFlightDistance, &m_Stats.uCurrentFlightTotalCurrent)) )
   {
      log_softerror_and_alarm("Load model8: missing extra data 1");
      m_Stats.uCurrentOnTime = 0; m_Stats.uCurrentFlightTime = 0; m_Stats.uCurrentFlightDistance = 0;
      bOk = false;
   }
   if ( bOk && (5 != fscanf(fd, "%u %u %u %u %u", &m_Stats.uCurrentTotalCurrent, &m_Stats.uCurrentMaxAltitude, &m_Stats.uCurrentMaxDistance, &m_Stats.uCurrentMaxCurrent, &m_Stats.uCurrentMinVoltage)) )
   {
      log_softerror_and_alarm("Load model8: missing extra data 2");
      m_Stats.uCurrentTotalCurrent = 0;
      m_Stats.uCurrentMaxAltitude = 0;
      m_Stats.uCurrentMaxDistance = 0;
      m_Stats.uCurrentMaxCurrent = 0;
      m_Stats.uCurrentMinVoltage = 100000;
      bOk = false;
   }

   if ( bOk && (3 != fscanf(fd, "%u %u %u", &m_Stats.uTotalOnTime, &m_Stats.uTotalFlightTime, &m_Stats.uTotalFlightDistance)) )
   {
      log_softerror_and_alarm("Load model8: missing extra data 3");
      m_Stats.uTotalOnTime = 0; m_Stats.uTotalFlightTime = 0; m_Stats.uTotalFlightDistance = 0;
      bOk = false;
   }
   if ( bOk && (5 != fscanf(fd, "%u %u %u %u %u", &m_Stats.uTotalTotalCurrent, &m_Stats.uTotalMaxAltitude, &m_Stats.uTotalMaxDistance, &m_Stats.uTotalMaxCurrent, &m_Stats.uTotalMinVoltage)) )
   {
      log_softerror_and_alarm("Load model8: missing extra data 4");
      m_Stats.uTotalTotalCurrent = 0;
      m_Stats.uTotalMaxAltitude = 0;
      m_Stats.uTotalMaxDistance = 0;
      m_Stats.uTotalMaxCurrent = 0;
      m_Stats.uTotalMinVoltage = 100000;
      bOk = false;
   }


   //----------------------------------------
   // Functions & Triggers

   if ( bOk && (3 != fscanf(fd, "%*s %d %d %d", &tmp1, &tmp2, &tmp3 )) )
      { log_softerror_and_alarm("Load model8: Error on line func_1"); bOk = false; tmp1 = 0; tmp2 = 0; tmp3 = 0; }

   functions_params.bEnableRCTriggerFreqSwitchLink1 = (bool)tmp1;
   functions_params.bEnableRCTriggerFreqSwitchLink2 = (bool)tmp2;
   functions_params.bEnableRCTriggerFreqSwitchLink3 = (bool)tmp3;


   if ( bOk && (3 != fscanf(fd, "%d %d %d", &functions_params.iRCTriggerChannelFreqSwitchLink1, &functions_params.iRCTriggerChannelFreqSwitchLink2, &functions_params.iRCTriggerChannelFreqSwitchLink3 )) )
      { log_softerror_and_alarm("Load model8: Error on line func_2"); bOk = false; functions_params.iRCTriggerChannelFreqSwitchLink1 = -1; functions_params.iRCTriggerChannelFreqSwitchLink2 = -1; functions_params.iRCTriggerChannelFreqSwitchLink3 = -1; }

   if ( bOk && (3 != fscanf(fd, "%d %d %d", &tmp1, &tmp2, &tmp3 )) )
      { log_softerror_and_alarm("Load model8: Error on line func_3"); bOk = false; }

   functions_params.bRCTriggerFreqSwitchLink1_is3Position = (bool)tmp1;
   functions_params.bRCTriggerFreqSwitchLink2_is3Position = (bool)tmp2;
   functions_params.bRCTriggerFreqSwitchLink3_is3Position = (bool)tmp3;

   for( int i=0; i<3; i++ )
   {
      if ( bOk && (6 != fscanf(fd, "%u %u %u %u %u %u", &functions_params.uChannels433FreqSwitch[i], &functions_params.uChannels868FreqSwitch[i], &functions_params.uChannels23FreqSwitch[i], &functions_params.uChannels24FreqSwitch[i], &functions_params.uChannels25FreqSwitch[i], &functions_params.uChannels58FreqSwitch[i])) )
         { log_softerror_and_alarm("Load model8: Error on line func_ch"); bOk = false; }
   }

   for( unsigned int i=0; i<(sizeof(functions_params.dummy)/sizeof(functions_params.dummy[0])); i++ )
      if ( bOk && (1 != fscanf(fd, "%u", &(functions_params.dummy[i]))) )
         { log_softerror_and_alarm("Load model8: Error on line funct_d"); bOk = false; }

   //----------------------------------------------------
   // Start of extra params, might be zero when loading older versions.

   if ( bOk && (1 != fscanf(fd, "%u", &uModelFlags )) )
      { log_softerror_and_alarm("Load model8: Error on line extra 1"); uModelFlags = 0; }


   if ( bOk )
   {
      for( int i=0; i<hardware_info.serial_bus_count; i++ )
         if ( 1 != fscanf(fd, "%d", &(hardware_info.serial_bus_speed[i])) )
         {
            for( int k=0; k<hardware_info.serial_bus_count; k++ )
               hardware_info.serial_bus_speed[k] = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
            break;
         }
   }

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      if ( 1 != fscanf(fd, "%u", &(osd_params.osd_flags3[i])) )
         { osd_params.osd_flags3[i] = 0; }


   // End reading file;
   //----------------------------------------

   // Validate settings;

   if ( niceRC < -18 || niceRC > 5 )
      niceRC = DEFAULT_PRIORITY_PROCESS_RC;
   if ( niceRouter < -18 || niceRouter > 5 )
      niceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
   if ( ioNiceRouter < -7 || ioNiceRouter > 7 )
      ioNiceRouter = DEFAULT_IO_PRIORITY_ROUTER;

   if ( telemetry_params.vehicle_mavlink_id <= 0 || telemetry_params.vehicle_mavlink_id > 255 )
      telemetry_params.vehicle_mavlink_id = DEFAULT_MAVLINK_SYS_ID_VEHICLE;
   if ( telemetry_params.controller_mavlink_id <= 0 || telemetry_params.controller_mavlink_id > 255 )
      telemetry_params.controller_mavlink_id = DEFAULT_MAVLINK_SYS_ID_CONTROLLER;
   if ( telemetry_params.flags == 0 )
      telemetry_params.flags = TELEMETRY_FLAGS_RXTX | TELEMETRY_FLAGS_REQUEST_DATA_STREAMS | TELEMETRY_FLAGS_SPECTATOR_ENABLE;
   if ( clock_sync_type < 0 || clock_sync_type >= CLOCK_SYNC_TYPE_LAST )
      clock_sync_type = CLOCK_SYNC_TYPE_NONE;

   validate_settings();

   /*
   log_line("---------------------------------------");
   log_line("Loaded radio links %d:", radioLinksParams.links_count);
   for( int i=0; i<radioLinksParams.links_count; i++ )
   {
      char szBuffR[128];
      str_get_radio_frame_flags_description(radioLinksParams.link_radio_flags[i], szBuffR);
      log_line("Radio link %d frame flags: [%s]", i+1, szBuffR);
   }
   log_line("Loaded radio interfaces %d:", radioInterfacesParams.interfaces_count);
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      char szBuffR[128];
      str_get_radio_frame_flags_description(radioInterfacesParams.interface_current_radio_flags[i], szBuffR);
      log_line("Radio interface %d frame flags: [%s]", i+1, szBuffR);
   }
   */
   return true;
}


bool Model::saveToFile(const char* filename, bool isOnController)
{
   iSaveCount++;
   char szBuff[128];
   char szComm[256];

   u32 timeStart = get_current_timestamp_ms();

   if ( isOnController )
   {
      sprintf(szBuff, FOLDER_VEHICLE_HISTORY, vehicle_id);
      sprintf(szComm, "mkdir -p %s", szBuff);
      hw_execute_bash_command_silent(szComm, NULL);
   }

   for( int i=0; i<(int)strlen(vehicle_name); i++ )
   {
      if ( vehicle_name[i] == '?' || vehicle_name[i] == '%' || vehicle_name[i] == '.' || vehicle_name[i] == '/' || vehicle_name[i] == '\\' )
         vehicle_name[i] = '_';
   }

   FILE* fd = fopen(filename, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save model configuration to file: %s",filename);
      return false;
   }
   saveVersion8(filename, fd, isOnController);
   if ( NULL != fd )
      fclose(fd);

   hardware_sleep_ms(2);

   strcpy(szBuff, filename);
   szBuff[strlen(szBuff)-3] = 'b';
   szBuff[strlen(szBuff)-2] = 'a';
   szBuff[strlen(szBuff)-1] = 'k';

   fd = fopen(szBuff, "w");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to save model configuration to file: %s",szBuff);
      return false;
   }
   saveVersion8(szBuff, fd, isOnController);
   if ( NULL != fd )
   {
      fflush(fd);
      fclose(fd);
   }

   timeStart = get_current_timestamp_ms() - timeStart;
   char szLog[512];
   sprintf(szLog, "Saved model version 8 (%d ms) to file [%s] and [*.bak], save count: %d: name: [%s], vehicle id: %u, software: %d.%d (b%d), (is on controller side: %s, is in control mode: %s)", timeStart, filename, iSaveCount, vehicle_name, vehicle_id, (sw_version >> 8) & 0xFF, sw_version & 0xFF, sw_version >> 16, isOnController?"yes":"no", is_spectator?"no (spectator mode)":"yes" );
   log_line(szLog);

   return true;
}

bool Model::saveVersion8(const char* szFile, FILE* fd, bool isOnController)
{
   char szSetting[256];
   char szModel[8096];

   szSetting[0] = 0;
   szModel[0] = 0;

   if ( ! isOnController )
      sw_version = (SYSTEM_SW_VERSION_MAJOR * 256 + SYSTEM_SW_VERSION_MINOR) | (SYSTEM_SW_BUILD_NUMBER<<16);


   sprintf(szSetting, "ver: 8\n"); // version number
   strcat(szModel, szSetting);
   sprintf(szSetting, "%s\n",MODEL_FILE_STAMP_ID);
   strcat(szModel, szSetting);
   sprintf(szSetting, "savecounter: %d\n", iSaveCount);
   strcat(szModel, szSetting);
   sprintf(szSetting, "id: %u %u %u %d\n", sw_version, vehicle_id, controller_id, board_type); 
   strcat(szModel, szSetting);

   char szVeh[MAX_VEHICLE_NAME_LENGTH+1];
   strncpy(szVeh, vehicle_name, MAX_VEHICLE_NAME_LENGTH);
   szVeh[MAX_VEHICLE_NAME_LENGTH] = 0;
   str_sanitize_modelname(szVeh);
   for( int i=0; i<(int)strlen(szVeh); i++ )
   {
      if ( szVeh[i] == ' ' )
         szVeh[i] = '_';
   }

   if ( 0 == szVeh[0] )
      sprintf(szSetting, "*\n"); 
   else
      sprintf(szSetting, "%s\n", szVeh);
   strcat(szModel, szSetting);
 
   sprintf(szSetting, "%d %u %d\n", clock_sync_type, camera_rc_channels, niceTelemetry );
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %u %d\n", is_spectator, vehicle_type, m_Stats.uTotalFlightTime, iGPSCount);
   strcat(szModel, szSetting);
   
   //----------------------------------------
   // CPU 

   sprintf(szSetting, "cpu: %d %d %d\n", niceVideo, niceOthers, ioNiceVideo); 
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d\n", iOverVoltage, iFreqARM, iFreqGPU);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d\n", niceRouter, ioNiceRouter, niceRC);
   strcat(szModel, szSetting);

   //----------------------------------------
   // Radio

   sprintf(szSetting, "radio_interfaces: %d\n", radioInterfacesParams.interfaces_count); 
   strcat(szModel, szSetting);
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      sprintf(szSetting, "%d %d %d\n", radioInterfacesParams.interface_card_model[i], radioInterfacesParams.interface_link_id[i], radioInterfacesParams.interface_current_frequency[i]);
      strcat(szModel, szSetting);
      sprintf(szSetting, "  %u %d %u %d %d %d %s- %s-\n", radioInterfacesParams.interface_capabilities_flags[i], radioInterfacesParams.interface_supported_bands[i], radioInterfacesParams.interface_type_and_driver[i], radioInterfacesParams.interface_current_radio_flags[i], radioInterfacesParams.interface_datarates[i][0], radioInterfacesParams.interface_datarates[i][1], radioInterfacesParams.interface_szMAC[i], radioInterfacesParams.interface_szPort[i]);
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, "%d %d %d %d %d %d %d %d %d\n", enableDHCP, radioInterfacesParams.txPower, radioInterfacesParams.txPowerAtheros, radioInterfacesParams.txPowerRTL, radioInterfacesParams.txMaxPower, radioInterfacesParams.txMaxPowerAtheros, radioInterfacesParams.txMaxPowerRTL, radioInterfacesParams.slotTime, radioInterfacesParams.thresh62); 
   strcat(szModel, szSetting);

   sprintf(szSetting, "radio_links: %d\n", radioLinksParams.links_count); 
   strcat(szModel, szSetting);

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      sprintf( szSetting, "%d %u %u %d %d   ", radioLinksParams.link_frequency[i], radioLinksParams.link_capabilities_flags[i], radioLinksParams.link_radio_flags[i], radioLinksParams.link_datarates[i][0], radioLinksParams.link_datarates[i][1] );
      strcat(szModel, szSetting);
      sprintf( szSetting, "%d %u %d %d\n", radioLinksParams.bUplinkSameAsDownlink[i], radioLinksParams.uplink_radio_flags[i], radioLinksParams.uplink_datarates[i][0], radioLinksParams.uplink_datarates[i][1] );
      strcat(szModel, szSetting);
   }
   for( unsigned int j=0; j<(sizeof(radioLinksParams.dummy)/sizeof(radioLinksParams.dummy[0])); j++ )
   {
      sprintf(szSetting, " %d", radioLinksParams.dummy[j]); 
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, "\n");      
   strcat(szModel, szSetting);

   sprintf(szSetting, "relay: %d %d %u %u\n", relay_params.is_RelayEnabled, relay_params.is_Relaying, relay_params.relayVehicleId, relay_params.relayFlags); 
   strcat(szModel, szSetting);

   //----------------------------------------
   // Telemetry

   sprintf(szSetting, "telem: %d %d %d %d %d\n", telemetry_params.fc_telemetry_type, telemetry_params.controller_telemetry_type, telemetry_params.update_rate, telemetry_params.bControllerHasOutputTelemetry, telemetry_params.bControllerHasInputTelemetry);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %u %d %u\n", telemetry_params.dummy1, telemetry_params.dummy2, telemetry_params.dummy3, telemetry_params.dummy4);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d\n", telemetry_params.vehicle_mavlink_id, telemetry_params.controller_mavlink_id, telemetry_params.flags);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %u\n", telemetry_params.dummy5, telemetry_params.dummy6);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d\n", 0); // not used
   strcat(szModel, szSetting);
 
   //----------------------------------------
   // Video 

   sprintf(szSetting, "video: %d %d %d %u\n", video_params.user_selected_video_link_profile, video_params.iH264Slices, video_params.videoAdjustmentStrength, video_params.lowestAllowedAdaptiveVideoBitrate);
   strcat(szModel, szSetting);
   
   sprintf(szSetting, "%u\n", video_params.uMaxAutoKeyframeInterval);
   strcat(szModel, szSetting);
   
   sprintf(szSetting, "%u\n", video_params.uExtraFlags);
   strcat(szModel, szSetting);
   
   for( unsigned int i=0; i<(sizeof(video_params.dummy)/sizeof(video_params.dummy[0])); i++ )
   {
      sprintf(szSetting, " %u",video_params.dummy[i]);
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, "\n");
   strcat(szModel, szSetting);
   
   //----------------------------------------
   // Video link profiles

   sprintf(szSetting, "video_link_profiles: %d\n", (int)MAX_VIDEO_LINK_PROFILES);
   strcat(szModel, szSetting);
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      sprintf(szSetting, "%u %u %u %d %d %u   ", video_link_profiles[i].flags, video_link_profiles[i].encoding_extra_flags, video_link_profiles[i].bitrate_fixed_bps, video_link_profiles[i].radio_datarate_video, video_link_profiles[i].radio_datarate_data, video_link_profiles[i].radio_flags);
      strcat(szModel, szSetting);
      sprintf(szSetting, "%d %d\n", video_link_profiles[i].width, video_link_profiles[i].height);
      strcat(szModel, szSetting);
      sprintf(szSetting, "%d %d %d %d %d   ", video_link_profiles[i].block_packets, video_link_profiles[i].block_fecs, video_link_profiles[i].packet_length, video_link_profiles[i].fps, video_link_profiles[i].keyframe);
      strcat(szModel, szSetting);
      sprintf(szSetting, "%d %d %d %d %d\n", video_link_profiles[i].h264profile, video_link_profiles[i].h264level, video_link_profiles[i].h264refresh, video_link_profiles[i].h264quantization, video_link_profiles[i].insertPPS);
      strcat(szModel, szSetting);
   }

   //----------------------------------------
   // Camera params

   sprintf(szSetting, "cameras: %d %d\n", iCameraCount, iCurrentCamera); 
   strcat(szModel, szSetting);
   for( int k=0; k<MODEL_MAX_CAMERAS; k++ )
   {

      sprintf(szSetting, "camera_%d: %d %d %d\n", k, camera_params[k].iCameraType, camera_params[k].iForcedCameraType, camera_params[k].iCurrentProfile); 
      strcat(szModel, szSetting);

      //----------------------------------------
      // Camera sensor name

      char szTmp[MAX_CAMERA_NAME_LENGTH+1];
      strncpy(szTmp, camera_params[k].szCameraName, MAX_CAMERA_NAME_LENGTH);
      szTmp[MAX_CAMERA_NAME_LENGTH] = 0;
      for( int i=0; i<(int)strlen(szTmp); i++ )
      {
         if ( szTmp[i] == ' ' )
            szTmp[i] = '*';
      }

      if ( 0 == szTmp[0] )
         sprintf(szSetting, "cname: *\n"); 
      else
         sprintf(szSetting, "cname: %s\n", szTmp);
      strcat(szModel, szSetting);


      for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
      {
      sprintf(szSetting, "cam_profile_%d: %d %d\n", i, camera_params[k].profiles[i].flags, camera_params[k].profiles[i].flip_image); 
      strcat(szModel, szSetting);
      sprintf(szSetting, "%d %d %d %d\n", camera_params[k].profiles[i].brightness, camera_params[k].profiles[i].contrast, camera_params[k].profiles[i].saturation, camera_params[k].profiles[i].sharpness);
      strcat(szModel, szSetting);
      sprintf(szSetting, "%d %d %d %d\n", camera_params[k].profiles[i].exposure, camera_params[k].profiles[i].whitebalance, camera_params[k].profiles[i].metering, camera_params[k].profiles[i].drc);
      strcat(szModel, szSetting);
      sprintf(szSetting, "%f %f %f\n", camera_params[k].profiles[i].analogGain, camera_params[k].profiles[i].awbGainB, camera_params[k].profiles[i].awbGainR);
      strcat(szModel, szSetting);
      sprintf(szSetting, "%f %f\n", camera_params[k].profiles[i].fovH, camera_params[k].profiles[i].fovV);
      strcat(szModel, szSetting);
      sprintf(szSetting, "%d %d %d %d\n", camera_params[k].profiles[i].vstab, camera_params[k].profiles[i].ev, camera_params[k].profiles[i].iso, camera_params[k].profiles[i].shutterspeed); 
      strcat(szModel, szSetting);

      sprintf(szSetting, "%d\n", (int)camera_params[k].profiles[i].wdr); 
      strcat(szModel, szSetting);

      for( unsigned int j=0; j<(sizeof(camera_params[k].profiles[i].dummy)/sizeof(camera_params[k].profiles[i].dummy[0])); j++ )
      {
         sprintf(szSetting, " %d", camera_params[k].profiles[i].dummy[j]); 
         strcat(szModel, szSetting);
      }
      sprintf(szSetting, "\n");      
      strcat(szModel, szSetting);
      }
   }
   //----------------------------------------
   // Audio

   sprintf(szSetting, "audio: %d\n", (int)audio_params.has_audio_device);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d %u\n", (int)audio_params.enabled, audio_params.volume, audio_params.quality, audio_params.flags);
   strcat(szModel, szSetting);

   sprintf(szSetting, "alarms: %u\n", alarms);
   strcat(szModel, szSetting);

   //----------------------------------------
   // Hardware Info

   sprintf(szSetting, "hw_info: %d %d %d %d\n", hardware_info.radio_interface_count, hardware_info.i2c_bus_count, hardware_info.i2c_device_count, hardware_info.serial_bus_count);
   strcat(szModel, szSetting);

   for( int i=0; i<hardware_info.i2c_bus_count; i++ )
   {
      sprintf(szSetting, " %d", hardware_info.i2c_bus_numbers[i]);
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, "\n");
   strcat(szModel, szSetting);

   for( int i=0; i<hardware_info.i2c_device_count; i++ )
   {
      sprintf(szSetting, " %d %d\n", hardware_info.i2c_devices_bus[i], hardware_info.i2c_devices_address[i]);
      strcat(szModel, szSetting);
   }

   for( int i=0; i<hardware_info.serial_bus_count; i++ )
   {
      sprintf(szSetting, " %u %s\n", hardware_info.serial_bus_supported_and_usage[i], hardware_info.serial_bus_names[i]);
      strcat(szModel, szSetting);
   }

   //----------------------------------------
   // OSD 

   sprintf(szSetting, "osd: %d %d %f %d %d\n", osd_params.layout, osd_params.voltage_alarm_enabled, osd_params.voltage_alarm, osd_params.altitude_relative, osd_params.show_gps_position); 
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d %d %d\n", osd_params.battery_show_per_cell, osd_params.battery_cell_count, osd_params.battery_capacity_percent_alarm, osd_params.invert_home_arrow, osd_params.home_arrow_rotate); 
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d %d %d %d %d\n", osd_params.show_overload_alarm, osd_params.show_stats_rx_detailed, osd_params.show_stats_decode, osd_params.show_stats_rc, osd_params.show_full_stats, osd_params.show_instruments, osd_params.ahi_warning_angle);
   strcat(szModel, szSetting);
   for( int i=0; i<5; i++ )
   {
      sprintf(szSetting, "%u %u %u %u\n", osd_params.osd_flags[i], osd_params.osd_flags2[i], osd_params.instruments_flags[i], osd_params.osd_preferences[i]);
      strcat(szModel, szSetting);
   }

   //----------------------------------------
   // RC 

   sprintf(szSetting, "rc: %d %d %d %d\n", rc_params.rc_enabled, rc_params.dummy1, rc_params.receiver_type, rc_params.rc_frames_per_second);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d\n", rc_params.inputType);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %ld %d %ld\n", rc_params.inputSerialPort, rc_params.inputSerialPortSpeed, rc_params.outputSerialPort, rc_params.outputSerialPortSpeed);
   strcat(szModel, szSetting);
   for( int i=0; i<MAX_RC_CHANNELS; i++ )
   {
      sprintf(szSetting, "%u %u %u %u %u %u %u\n", (u32)rc_params.rcChAssignment[i], (u32)rc_params.rcChMid[i], (u32)rc_params.rcChMin[i], (u32)rc_params.rcChMax[i], (u32)rc_params.rcChFailSafe[i], (u32)rc_params.rcChExpo[i], (u32)rc_params.rcChFlags[i]);
      strcat(szModel, szSetting);
   }

   sprintf(szSetting, "%d %u %u\n", rc_params.rc_failsafe_timeout_ms, rc_params.failsafeFlags, rc_params.channelsCount );
   strcat(szModel, szSetting);
   sprintf(szSetting, "%u\n", rc_params.hid_id );
   strcat(szModel, szSetting);
   sprintf(szSetting, "%u\n", rc_params.flags );
   strcat(szModel, szSetting);
   sprintf(szSetting, "%u\n", rc_params.rcChAssignmentThrotleReverse );
   strcat(szModel, szSetting);
   for( unsigned int i=0; i<(sizeof(rc_params.dummy)/sizeof(rc_params.dummy[0])); i++ )
   {
      sprintf(szSetting, " %u",rc_params.dummy[i]);
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, "\n");
   strcat(szModel, szSetting);

   //----------------------------------------
   // Misc

   sprintf(szSetting, "misc_dev: %d %u\n", bDeveloperMode, uDeveloperFlags);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%u\n", enc_flags);
   strcat(szModel, szSetting);

   sprintf(szSetting, "stats: %u\n", m_Stats.uTotalFlights);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%u %u %u %u\n", m_Stats.uCurrentOnTime, m_Stats.uCurrentFlightTime, m_Stats.uCurrentFlightDistance, m_Stats.uCurrentFlightTotalCurrent);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%u %u %u %u %u\n", m_Stats.uCurrentTotalCurrent, m_Stats.uCurrentMaxAltitude, m_Stats.uCurrentMaxDistance, m_Stats.uCurrentMaxCurrent, m_Stats.uCurrentMinVoltage);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%u %u %u\n", m_Stats.uTotalOnTime, m_Stats.uTotalFlightTime, m_Stats.uTotalFlightDistance);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%u %u %u %u %u\n", m_Stats.uTotalTotalCurrent, m_Stats.uTotalMaxAltitude, m_Stats.uTotalMaxDistance, m_Stats.uTotalMaxCurrent, m_Stats.uTotalMinVoltage);
   strcat(szModel, szSetting);

   //----------------------------------------
   // Functions triggers 

   sprintf(szSetting, "func: %d %d %d\n", functions_params.bEnableRCTriggerFreqSwitchLink1, functions_params.bEnableRCTriggerFreqSwitchLink2, functions_params.bEnableRCTriggerFreqSwitchLink3);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d\n", functions_params.iRCTriggerChannelFreqSwitchLink1, functions_params.iRCTriggerChannelFreqSwitchLink2, functions_params.iRCTriggerChannelFreqSwitchLink3);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d\n", functions_params.bRCTriggerFreqSwitchLink1_is3Position, functions_params.bRCTriggerFreqSwitchLink2_is3Position, functions_params.bRCTriggerFreqSwitchLink3_is3Position);
   strcat(szModel, szSetting);

   for( int i=0; i<3; i++ )
   {
      sprintf(szSetting, "%u %u %u %u %u %u\n", functions_params.uChannels433FreqSwitch[i], functions_params.uChannels868FreqSwitch[i], functions_params.uChannels23FreqSwitch[i], functions_params.uChannels24FreqSwitch[i], functions_params.uChannels25FreqSwitch[i], functions_params.uChannels58FreqSwitch[i]);
      strcat(szModel, szSetting);
   }
   for( unsigned int i=0; i<(sizeof(functions_params.dummy)/sizeof(functions_params.dummy[0])); i++ )
   {
      sprintf(szSetting, " %u",functions_params.dummy[i]);
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, "\n");
   strcat(szModel, szSetting);

   //----------------------------------------
   //-------------------------------------------
   // Starting extra params, might be zero on load

   sprintf(szSetting, "%u\n", uModelFlags);
   strcat(szModel, szSetting);

   for( int i=0; i<hardware_info.serial_bus_count; i++ )
   {
      sprintf(szSetting, " %d\n", hardware_info.serial_bus_speed[i]);
      strcat(szModel, szSetting);
   }

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   {
      sprintf(szSetting, " %u", osd_params.osd_flags3[i]);
      strcat(szModel, szSetting);
   }
   strcat(szModel, "\n");

   // Done
   fprintf(fd, "%s", szModel);
   return true;
}

void Model::resetVideoParamsToDefaults()
{
   memset(&video_params, 0, sizeof(video_params));

   video_params.user_selected_video_link_profile = VIDEO_PROFILE_HIGH_QUALITY;
   video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES;
   video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;
   video_params.lowestAllowedAdaptiveVideoBitrate = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;
   video_params.uMaxAutoKeyframeInterval = DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL;
   video_params.uExtraFlags = 0; // Fill H264 SPS timings
   resetVideoLinkProfiles();
}

void Model::resetVideoLinkProfiles(int iProfile)
{
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( iProfile != -1 && iProfile != i )
         continue;
      video_link_profiles[i].flags = 0;
      video_link_profiles[i].encoding_extra_flags = ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO | ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
      video_link_profiles[i].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);

      video_link_profiles[i].radio_datarate_video = DEFAULT_DATARATE_VIDEO;
      video_link_profiles[i].radio_datarate_data = DEFAULT_DATARATE_DATA;
      video_link_profiles[i].radio_flags = 0;
      video_link_profiles[i].h264profile = 3; // extended
      video_link_profiles[i].h264level = 2; 
      video_link_profiles[i].h264refresh = 2; // both
      video_link_profiles[i].h264quantization = DEFAULT_VIDEO_H264_QUANTIZATION;
      video_link_profiles[i].insertPPS = 1;

      video_link_profiles[i].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
      video_link_profiles[i].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HP;
      video_link_profiles[i].packet_length = DEFAULT_VIDEO_PACKET_LENGTH_HP;
      video_link_profiles[i].fps = DEFAULT_VIDEO_FPS;
      video_link_profiles[i].keyframe = DEFAULT_VIDEO_KEYFRAME;
      video_link_profiles[i].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;

      if ( hardware_getBoardType() == BOARD_TYPE_PIZERO || hardware_getBoardType() == BOARD_TYPE_PIZEROW )
         video_link_profiles[i].bitrate_fixed_bps = 4000000;

      if ( hardware_isCameraVeye() || hardware_isCameraHDMI() )
      {
         video_link_profiles[i].width = 1920;
         video_link_profiles[i].height = 1080;
      }
      else
      {
         video_link_profiles[i].width = DEFAULT_VIDEO_WIDTH;
         video_link_profiles[i].height = DEFAULT_VIDEO_HEIGHT;
      }
   }


   // Best Perf
   if ( iProfile == -1 || iProfile == VIDEO_PROFILE_BEST_PERF )
   {
   video_link_profiles[VIDEO_PROFILE_BEST_PERF].flags = 0;
   video_link_profiles[VIDEO_PROFILE_BEST_PERF].encoding_extra_flags = ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
   video_link_profiles[VIDEO_PROFILE_BEST_PERF].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   video_link_profiles[VIDEO_PROFILE_BEST_PERF].encoding_extra_flags |= ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
   video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
   video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HP;
   video_link_profiles[VIDEO_PROFILE_BEST_PERF].packet_length = DEFAULT_VIDEO_PACKET_LENGTH_HP;
   }

   // High Quality 
   if ( iProfile == -1 || iProfile == VIDEO_PROFILE_HIGH_QUALITY )
   {
   video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].flags = 0;
   video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].encoding_extra_flags = ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
   video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
   video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].encoding_extra_flags |= ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
   video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
   video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HQ;
   video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].packet_length = DEFAULT_VIDEO_PACKET_LENGTH_HQ;
   }

   // User
   if ( iProfile == -1 || iProfile == VIDEO_PROFILE_USER )
   {
   video_link_profiles[VIDEO_PROFILE_USER].flags = 0;
   video_link_profiles[VIDEO_PROFILE_USER].encoding_extra_flags = ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO;
   video_link_profiles[VIDEO_PROFILE_USER].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
   video_link_profiles[VIDEO_PROFILE_USER].encoding_extra_flags |= ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
   video_link_profiles[VIDEO_PROFILE_USER].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
   video_link_profiles[VIDEO_PROFILE_USER].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HP;
   video_link_profiles[VIDEO_PROFILE_USER].packet_length = DEFAULT_VIDEO_PACKET_LENGTH_HP;
   }

   // MQ
   if ( iProfile == -1 || iProfile == VIDEO_PROFILE_MQ )
   {
   video_link_profiles[VIDEO_PROFILE_MQ].flags = 0;
   video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO | ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
   video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_MQ<<8);
   video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_video = 12;
   video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_data = 12;
   video_link_profiles[VIDEO_PROFILE_MQ].h264profile = 3; // extended
   video_link_profiles[VIDEO_PROFILE_MQ].h264level = 2;
   video_link_profiles[VIDEO_PROFILE_MQ].h264refresh = 2; // both
   video_link_profiles[VIDEO_PROFILE_MQ].h264quantization = DEFAULT_VIDEO_H264_QUANTIZATION; // auto
   video_link_profiles[VIDEO_PROFILE_MQ].block_packets = DEFAULT_MQ_VIDEO_BLOCK_PACKETS;
   video_link_profiles[VIDEO_PROFILE_MQ].block_fecs = DEFAULT_MQ_VIDEO_BLOCK_FECS;
   video_link_profiles[VIDEO_PROFILE_MQ].packet_length = DEFAULT_MQ_VIDEO_PACKET_LENGTH;
   video_link_profiles[VIDEO_PROFILE_MQ].keyframe = DEFAULT_MQ_VIDEO_KEYFRAME;
   video_link_profiles[VIDEO_PROFILE_MQ].fps = DEFAULT_MQ_VIDEO_FPS;
   video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps = DEFAULT_MQ_VIDEO_BITRATE;
   }

   // LQ
   if ( iProfile == -1 || iProfile == VIDEO_PROFILE_LQ )
   {
   video_link_profiles[VIDEO_PROFILE_LQ].flags = 0;
   video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS | ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS | ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO | ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
   video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_LQ<<8);
   video_link_profiles[VIDEO_PROFILE_LQ].radio_datarate_video = 6;
   video_link_profiles[VIDEO_PROFILE_LQ].radio_datarate_data = 6;
   video_link_profiles[VIDEO_PROFILE_LQ].radio_flags = RADIO_FLAGS_HT20;
   video_link_profiles[VIDEO_PROFILE_LQ].h264profile = 3; // extended
   video_link_profiles[VIDEO_PROFILE_LQ].h264level = 2;
   video_link_profiles[VIDEO_PROFILE_LQ].h264refresh = 2; // both
   video_link_profiles[VIDEO_PROFILE_LQ].h264quantization = DEFAULT_VIDEO_H264_QUANTIZATION; // auto
   video_link_profiles[VIDEO_PROFILE_LQ].block_packets = DEFAULT_LQ_VIDEO_BLOCK_PACKETS;
   video_link_profiles[VIDEO_PROFILE_LQ].block_fecs = DEFAULT_LQ_VIDEO_BLOCK_FECS;
   video_link_profiles[VIDEO_PROFILE_LQ].packet_length = DEFAULT_LQ_VIDEO_PACKET_LENGTH;
   video_link_profiles[VIDEO_PROFILE_LQ].keyframe = DEFAULT_LQ_VIDEO_KEYFRAME;
   video_link_profiles[VIDEO_PROFILE_LQ].fps = DEFAULT_LQ_VIDEO_FPS;
   video_link_profiles[VIDEO_PROFILE_LQ].bitrate_fixed_bps = DEFAULT_LQ_VIDEO_BITRATE;
   }

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( i == video_params.user_selected_video_link_profile )
         continue;
      if ( (i != VIDEO_PROFILE_MQ) && (i != VIDEO_PROFILE_LQ) )
         continue;
      video_link_profiles[i].h264profile = video_link_profiles[video_params.user_selected_video_link_profile].h264profile;
      video_link_profiles[i].h264level = video_link_profiles[video_params.user_selected_video_link_profile].h264level; 
      video_link_profiles[i].h264refresh = video_link_profiles[video_params.user_selected_video_link_profile].h264refresh;
      video_link_profiles[i].insertPPS = video_link_profiles[video_params.user_selected_video_link_profile].insertPPS;
   }
}


void Model::copy_video_link_profile(int from, int to)
{
   if ( from < 0 || to < 0 )
      return;
   if ( from >= MAX_VIDEO_LINK_PROFILES || to >= MAX_VIDEO_LINK_PROFILES )
      return;
   if ( from == to )
      return;
   if ( to == 0 || to == 1 ) // do not overwrite HQ and HP profiles
      return;

   memcpy(&(video_link_profiles[to]), &(video_link_profiles[from]), sizeof(type_video_link_profile));
}


void Model::generateUID()
{
   log_line("Generating a new unique vehicle ID...");
   vehicle_id = rand();
   FILE* fd = fopen("/sys/firmware/devicetree/base/serial-number", "r");
   if ( NULL != fd )
   {
      char szBuff[256];
      szBuff[0] = 0;
      if ( 1 == fscanf(fd, "%s", szBuff) && 4 < strlen(szBuff) )
      {
         //log_line("Serial ID of HW: %s", szBuff);
         vehicle_id += szBuff[strlen(szBuff)-1] + 256 * szBuff[strlen(szBuff)-2];
         
         //strcat(vehicle_name, "-");
         //strcat(vehicle_name, szBuff + (strlen(szBuff)-4));
      }
      fclose(fd);
   }
   log_line("Generated unique vehicle ID: %u", vehicle_id);
}

void Model::populateHWInfo()
{
   hardware_info.radio_interface_count = hardware_get_radio_interfaces_count();
   hardware_info.i2c_bus_count = 0;
   hardware_info.i2c_device_count = 0;
   hardware_info.serial_bus_count = 0;

   hardware_enumerate_i2c_busses();

   hardware_info.i2c_bus_count = hardware_get_i2c_busses_count();

   for( int i=0; i<hardware_get_i2c_busses_count(); i++ )
   {
      hw_i2c_bus_info_t* pBus = hardware_get_i2c_bus_info(i);
      if ( NULL != pBus && i < MAX_MODEL_I2C_BUSSES )
         hardware_info.i2c_bus_numbers[i] = pBus->nBusNumber;
   }

   hardware_info.i2c_device_count = 0;
   for( int i=1; i<128; i++ )
   {
      if ( ! hardware_has_i2c_device_id(i) )
         continue;
      hardware_info.i2c_devices_address[hardware_info.i2c_device_count] = i;
      hardware_info.i2c_devices_bus[hardware_info.i2c_device_count] = hardware_get_i2c_device_bus_number(i);
      hardware_info.i2c_device_count++;
      if ( hardware_info.i2c_device_count >= MAX_MODEL_I2C_DEVICES )
         break;
   }

   populateVehicleSerialPorts();
}

bool Model::populateVehicleSerialPorts()
{
   hardware_info.serial_bus_count = hardware_get_serial_ports_count();

   for( int i=0; i<hardware_info.serial_bus_count; i++ )
   {
      hw_serial_port_info_t* pInfo = hardware_get_serial_port_info(i);
      if ( NULL == pInfo )
      {
         hardware_info.serial_bus_count = i;
         break;
      }
      strcpy(hardware_info.serial_bus_names[i], pInfo->szName);
      hardware_info.serial_bus_speed[i] = pInfo->lPortSpeed;
      hardware_info.serial_bus_supported_and_usage[i] = 0;

      int iIndex = atoi(&(pInfo->szName[strlen(pInfo->szName)-1]));
      
      hardware_info.serial_bus_supported_and_usage[i] |= ((u8)iIndex) & 0x0F;
      if ( pInfo->iSupported )
         hardware_info.serial_bus_supported_and_usage[i] |= ((1<<5)<<8);
      
      if ( (NULL != strstr(pInfo->szName, "USB")) ||
           (NULL != strstr(pInfo->szPortDeviceName, "USB")) )
         hardware_info.serial_bus_supported_and_usage[i] |= ((1<<4)<<8);
      hardware_info.serial_bus_supported_and_usage[i] |= (u8)(pInfo->iPortUsage);
   }

   log_line("Model: Populated %d serial ports:", hardware_info.serial_bus_count);
   for( int i=0; i<hardware_info.serial_bus_count; i++ )
   {
      log_line("Model: Serial Port: %s, type: %s, supported: %s, index: %d;", hardware_info.serial_bus_names[i],
         (hardware_info.serial_bus_supported_and_usage[i] & (1<<4<<8))?"USB":"HW",
         (hardware_info.serial_bus_supported_and_usage[i] & (1<<5<<8))?"yes":"no",
          hardware_info.serial_bus_supported_and_usage[i] & 0x0F );
   }

   return true;
}

void Model::populateDefaultRadioInterfacesInfo()
{
   log_line("Populate default radio interfaces info.");

   radioInterfacesParams.interfaces_count = hardware_get_radio_interfaces_count();
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radioInterfacesParams.interface_card_model[i] = 0;
      radioInterfacesParams.interface_link_id[i] = i;
      if ( radioInterfacesParams.interface_link_id[i] >= radioLinksParams.links_count )
         radioInterfacesParams.interface_link_id[i] = radioLinksParams.links_count - 1;
      radioInterfacesParams.interface_current_frequency[i] = 0;
      radioInterfacesParams.interface_current_radio_flags[i] = 0;
      radioInterfacesParams.interface_datarates[i][0] = 0;
      radioInterfacesParams.interface_datarates[i][1] = 0;

      radioInterfacesParams.interface_type_and_driver[i] = 0;
      radioInterfacesParams.interface_supported_bands[i] = 0;
      radioInterfacesParams.interface_szMAC[i][0] = 0;
      radioInterfacesParams.interface_szPort[i][0] = 0;

      radioInterfacesParams.interface_capabilities_flags[i] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
      if ( i >= radioInterfacesParams.interfaces_count )
         continue;

      radio_hw_info_t* pNIC = hardware_get_radio_info(i);
      if ( NULL == pNIC )
         continue;
      radioInterfacesParams.interface_supported_bands[i] = pNIC->supportedBands;
      radioInterfacesParams.interface_type_and_driver[i] = pNIC->typeAndDriver;
      if ( pNIC->isSupported )
         radioInterfacesParams.interface_type_and_driver[i] = radioInterfacesParams.interface_type_and_driver[i] | 0xFF0000;
      strcpy(radioInterfacesParams.interface_szMAC[i], pNIC->szMAC );
      strcpy(radioInterfacesParams.interface_szPort[i], pNIC->szUSBPort);
   }

   validate_settings();
   populateDefaultRadioLinksInfo();
   validate_settings();
}

void Model::populateDefaultRadioLinksInfo()
{
   log_line("Populate default radio links info.");
   radioLinksParams.links_count = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radioLinksParams.link_frequency[i] = 0;
      radioLinksParams.link_radio_flags[i] = DEFAULT_RADIO_FRAMES_FLAGS;
      radioLinksParams.link_capabilities_flags[i] = RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_RELAY;
      radioLinksParams.link_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      radioLinksParams.link_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
      //if ( i != 0 )
      //   radioLinksParams.link_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_DISABLED;
   }

   bool bDefault24Used = false;
   bool bDefault58Used = false;

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      if ( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;

      radioInterfacesParams.interface_link_id[i] = radioLinksParams.links_count;
      if ( radioInterfacesParams.interface_current_frequency[i] > 0 )
         radioLinksParams.link_frequency[radioLinksParams.links_count] = radioInterfacesParams.interface_current_frequency[i];
      else
      {      
         radioLinksParams.link_frequency[radioLinksParams.links_count] = DEFAULT_FREQUENCY;
         if ( radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_58 )
         {
            if ( ! bDefault58Used )
            {
               radioLinksParams.link_frequency[radioLinksParams.links_count] = DEFAULT_FREQUENCY58;
               bDefault58Used = true;
            }
            else
               radioLinksParams.link_frequency[radioLinksParams.links_count] = DEFAULT_FREQUENCY58_2;
         }
         else
         {
            if ( ! bDefault24Used )
            {
               radioLinksParams.link_frequency[radioLinksParams.links_count] = DEFAULT_FREQUENCY;
               bDefault24Used = true;
            }
            else
               radioLinksParams.link_frequency[radioLinksParams.links_count] = DEFAULT_FREQUENCY_2;
         }
         radioInterfacesParams.interface_current_frequency[i] = radioLinksParams.link_frequency[radioLinksParams.links_count];
      }

      radioLinksParams.links_count++;
   }

   // Remove links that have the same frequency

   for( int i=0; i<radioLinksParams.links_count-1; i++ )
   for( int j=i+1; j<radioLinksParams.links_count; j++ )
      if ( radioLinksParams.link_frequency[j] == radioLinksParams.link_frequency[i] )
      {
         radioLinksParams.link_frequency[j] = 0;
         for( int k=0; k<radioInterfacesParams.interfaces_count; k++ )
            if ( radioInterfacesParams.interface_link_id[k] == j )
               radioInterfacesParams.interface_link_id[k] = -1;
         for( int k=j; k<radioLinksParams.links_count-1; k++ )
         {
            radioLinksParams.link_frequency[k] = radioLinksParams.link_frequency[k+1];
            radioLinksParams.link_capabilities_flags[k] = radioLinksParams.link_capabilities_flags[k+1];
            radioLinksParams.link_radio_flags[k] = radioLinksParams.link_radio_flags[k+1];
            radioLinksParams.link_datarates[k][0] = radioLinksParams.link_datarates[k+1][0];
            radioLinksParams.link_datarates[k][1] = radioLinksParams.link_datarates[k+1][1];
            radioLinksParams.bUplinkSameAsDownlink[k] = radioLinksParams.bUplinkSameAsDownlink[k+1];
            radioLinksParams.uplink_datarates[k][0] = radioLinksParams.uplink_datarates[k+1][0];
            radioLinksParams.uplink_datarates[k][1] = radioLinksParams.uplink_datarates[k+1][1];
            int interface1 = -1;
            int interface2 = -1;
            for(; interface1<radioInterfacesParams.interfaces_count; interface1++ )
               if ( interface1 >= 0 && radioInterfacesParams.interface_link_id[interface1] == k )
                  break;
            for(; interface2<radioInterfacesParams.interfaces_count; interface2++ )
               if ( interface2 >= 0 && radioInterfacesParams.interface_link_id[interface2] == k )
                  break;

            if ( interface1 >= 0 && interface1 < radioInterfacesParams.interfaces_count )
               radioInterfacesParams.interface_link_id[interface1] = -1;
            if ( interface2 >= 0 && interface2 < radioInterfacesParams.interfaces_count )
               radioInterfacesParams.interface_link_id[interface2] = k;
         }
         radioLinksParams.links_count--;
      }


   // Populate radio interfaces radio flags and rates from radio links radio flags and rates

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
     if ( radioInterfacesParams.interface_link_id[i] >= 0 && radioInterfacesParams.interface_link_id[i] < radioLinksParams.links_count )
     {
         radioInterfacesParams.interface_current_radio_flags[i] = radioLinksParams.link_radio_flags[radioInterfacesParams.interface_link_id[i]];
         radioInterfacesParams.interface_datarates[i][0] = 0;
         radioInterfacesParams.interface_datarates[i][1] = 0;
     }
   }

   if ( true )
      logVehicleRadioInfo();

}

void Model::updateRadioInterfacesRadioFlags()
{
   for( int k=0; k<radioLinksParams.links_count; k++ )
   {
      for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      {
         if ( radioInterfacesParams.interface_link_id[i] == k )
         {
            radioInterfacesParams.interface_current_radio_flags[i] = radioLinksParams.link_radio_flags[k];
         }
      }
   }
}

void Model::logVehicleRadioInfo()
{
   char szBuff[256];
   log_line("------------------------------------------------------");
   log_line("Vehicle current radio links (%d links, %d radio interfaces):", radioLinksParams.links_count, radioInterfacesParams.interfaces_count);

   for( int i=0; i<radioLinksParams.links_count; i++ )
   {
         char szBuff2[256];
         szBuff[0] = 0;
         szBuff2[0] = 0;
         str_get_radio_frame_flags_description(radioLinksParams.link_radio_flags[i], szBuff);
         for( int k=0; k<radioInterfacesParams.interfaces_count; k++ )
            if ( radioInterfacesParams.interface_link_id[k] == i )
            {
               char szInfo[256];
               if ( 0 != szBuff2[0] )
                  sprintf(szInfo, ", %d", k+1);
               else
                  sprintf(szInfo, "%d", k+1);
               strcat(szBuff2, szInfo);
            }
         log_line("* Radio Link %d: %d Mhz, Radio frame flags: %s, used on radio interfaces: [%s]", i+1, radioLinksParams.link_frequency[i], szBuff, szBuff2);
         str_get_radio_capabilities_description(radioLinksParams.link_capabilities_flags[i], szBuff);
         log_line("  Radio Link %d capabilities flags: %s", i+1, szBuff);
         log_line("  Radio Link %d datarates (video/data/same on controller):", i+1, radioLinksParams.link_datarates[i][0], radioLinksParams.link_datarates[i][1], radioLinksParams.bUplinkSameAsDownlink[i]);
   }

   log_line("------------------------------------------------------");

   log_line("Vehicle current radio interfaces (%d radio interfaces):", radioInterfacesParams.interfaces_count);
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      {
         szBuff[0] = 0;
         str_get_radio_capabilities_description(radioInterfacesParams.interface_capabilities_flags[i], szBuff);
         char szBuff2[128];
         str_get_radio_frame_flags_description(radioInterfacesParams.interface_current_radio_flags[i], szBuff2);
         
         if ( hardware_is_vehicle() )
         {
            radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
            if ( NULL != pRadioInfo )
               log_line("* Radio Interface %d: %s, %s on port %s, %s, current frequency: %d Mhz, assigned to radio link %d, current capabilities: %s, current radio flags: %s", i+1, pRadioInfo->szName, str_get_radio_card_model_string(radioInterfacesParams.interface_card_model[i]), radioInterfacesParams.interface_szPort[i], str_get_radio_driver_description(radioInterfacesParams.interface_type_and_driver[i]), radioInterfacesParams.interface_current_frequency[i], radioInterfacesParams.interface_link_id[i]+1, szBuff, szBuff2);
            else
               log_line("* Radio Interface %d: %s on port %s, %s, current frequency: %d Mhz, assigned to radio link %d, current capabilities: %s, current radio flags: %s",
                  i+1, str_get_radio_card_model_string(radioInterfacesParams.interface_card_model[i]), radioInterfacesParams.interface_szPort[i], str_get_radio_driver_description(radioInterfacesParams.interface_type_and_driver[i]), radioInterfacesParams.interface_current_frequency[i], radioInterfacesParams.interface_link_id[i]+1, szBuff, szBuff2);
         }
         else
             log_line("* Radio Interface %d: %s on port %s, %s, current frequency: %d Mhz, assigned to radio link %d, current capabilities: %s, current radio flags: %s",
                  i+1, str_get_radio_card_model_string(radioInterfacesParams.interface_card_model[i]), radioInterfacesParams.interface_szPort[i], str_get_radio_driver_description(radioInterfacesParams.interface_type_and_driver[i]), radioInterfacesParams.interface_current_frequency[i], radioInterfacesParams.interface_link_id[i]+1, szBuff, szBuff2);

         char szDR1[32];
         char szDR2[32];
         if ( radioInterfacesParams.interface_datarates[i][0] != 0 )
            getDataRateDescription(radioInterfacesParams.interface_datarates[i][0], szDR1);
         else
            strcpy(szDR1, "auto");

         if ( radioInterfacesParams.interface_datarates[i][1] != 0 )
            getDataRateDescription(radioInterfacesParams.interface_datarates[i][1], szDR2);
         else
            strcpy(szDR2, "auto");
         
         log_line("  Radio Interface %d datarates (video/data): %s / %s", i+1, szDR1, szDR2);
      }

   log_line("------------------------------------------------------");
}

bool Model::validate_camera_settings()
{
   if ( hardware_is_station() )
      return false;

   int camera_now = hardware_getCameraType();
   bool bUpdated = false;
   if ( camera_params[iCurrentCamera].iCameraType != camera_now )
   {
      log_line("Validating camera info: hardware camera has changed. New camera type: %u", camera_now);
      resetCameraToDefaults(iCurrentCamera);
      camera_params[iCurrentCamera].iCameraType = camera_now;

      if ( camera_now == CAMERA_TYPE_HDMI )
         camera_params[iCurrentCamera].iCurrentProfile = MODEL_CAMERA_PROFILES-1;

      if ( hardware_isCameraVeye() || hardware_isCameraHDMI() )
      {
         for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
         {
            video_link_profiles[i].width = 1920;
            video_link_profiles[i].height = 1080;
            video_link_profiles[i].fps = 30;
         }
      }
      bUpdated = true;
   }
   else
      log_line("Validating camera info: camera has not changed. Camera type: %d", camera_now);

   // Force valid resolution for:
   // * always for Veye cameras
   // * when a HDMI camera is first plugged in

   if ( (bUpdated && hardware_isCameraHDMI()) || hardware_isCameraVeye() )
   {
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
      int fps = video_link_profiles[i].fps;
      int width = video_link_profiles[i].width;
      int height = video_link_profiles[i].height;

      if ( width == 1280 && height == 720 )
      {
         if ( fps > 60 )
            { fps = 60; bUpdated = true; }
      }
      else if ( width == 1920 && height == 1080 )
      {
         if ( fps > 30 )
            { fps = 30; bUpdated = true; }
      }
      else
      {
         width = 1280;
         height = 720;
         fps = 30;
         bUpdated = true;
      }
      video_link_profiles[i].width = width;
      video_link_profiles[i].height = height;
      video_link_profiles[i].fps = fps;
      }
   }
   log_line("Validating camera info: camera was updated: %s", bUpdated?"yes":"no");
   return bUpdated;
}


bool Model::validate_settings()
{
   if ( rc_params.channelsCount < 2 || rc_params.channelsCount > MAX_RC_CHANNELS )
      rc_params.channelsCount = 8;

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      if ( 0 == radioInterfacesParams.interface_szMAC[i][0] )
         strcpy(radioInterfacesParams.interface_szMAC[i], "N/A");
      if ( 0 == radioInterfacesParams.interface_szPort[i][0] )
         strcpy(radioInterfacesParams.interface_szPort[i], "X");
   }
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      if ( radioLinksParams.link_datarates[i][0] < -7 || radioLinksParams.link_datarates[i][0] > 54 )
      {
         log_softerror_and_alarm("Invalid radio video data rates. Reseting to default (%d).", DEFAULT_DATARATE_VIDEO);
         radioLinksParams.link_datarates[i][0] = DEFAULT_DATARATE_VIDEO;
      }
      if ( radioLinksParams.link_datarates[i][1] < -7 || radioLinksParams.link_datarates[i][1] > 54 )
      {
         log_softerror_and_alarm("Invalid radio data data rates. Reseting to default (%d).", DEFAULT_DATARATE_DATA);
         radioLinksParams.link_datarates[i][1] = DEFAULT_DATARATE_DATA;
      }
   }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      if ( radioLinksParams.link_frequency[i] < 0 || radioLinksParams.link_frequency[i] > 5995 )
        radioLinksParams.link_frequency[i] = DEFAULT_FREQUENCY;

   if ( osd_params.layout < osdLayout1 || osd_params.layout >= osdLayoutLast )
      osd_params.layout = osdLayout1;

   
   int nOSDFlagsIndex = osd_params.layout;
   if ( nOSDFlagsIndex < 0 || nOSDFlagsIndex >= MODEL_MAX_OSD_PROFILES )
      nOSDFlagsIndex = 0;
   if ( osd_params.show_stats_rc || (osd_params.osd_flags[nOSDFlagsIndex] & OSD_FLAG_SHOW_HID_IN_OSD) )
      rc_params.dummy1 = true;
   else
      rc_params.dummy1 = false;

   if ( telemetry_params.vehicle_mavlink_id <= 0 || telemetry_params.vehicle_mavlink_id > 255 )
      telemetry_params.vehicle_mavlink_id = DEFAULT_MAVLINK_SYS_ID_VEHICLE;
   if ( telemetry_params.controller_mavlink_id <= 0 || telemetry_params.controller_mavlink_id > 255 )
      telemetry_params.controller_mavlink_id = DEFAULT_MAVLINK_SYS_ID_CONTROLLER;
   if ( telemetry_params.flags == 0 )
      telemetry_params.flags = TELEMETRY_FLAGS_RXTX | TELEMETRY_FLAGS_REQUEST_DATA_STREAMS | TELEMETRY_FLAGS_SPECTATOR_ENABLE;

   if ( clock_sync_type < 0 || clock_sync_type >= CLOCK_SYNC_TYPE_LAST )
      clock_sync_type = CLOCK_SYNC_TYPE_NONE;

   if ( niceRouter < -18 || niceRouter > 0 )
      niceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
   if ( niceVideo < -18 || niceVideo > 0 )
      niceVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_TX;
   if ( niceRC < -16 || niceRC > 0 )
      niceRC = DEFAULT_PRIORITY_PROCESS_RC;
   if ( niceTelemetry < -16 || niceTelemetry > 0 )
      niceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY;

   if ( audio_params.volume < 0 || audio_params.volume > 100 )
      audio_params.volume = 50;
   if ( audio_params.quality < 0 || audio_params.quality > 3 )
      audio_params.quality = 1;
   if ( 0 == (audio_params.flags & 0xFF) || (audio_params.flags & 0xFF) > 16 )
      audio_params.flags = 0x04 | (0x02<<8);
   if ( ((audio_params.flags >> 8) & 0xFF) > (audio_params.flags & 0xFF) )
      audio_params.flags = 0x04 | (0x02<<8);

   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      if ( video_link_profiles[i].packet_length > (int)MAX_PACKET_PAYLOAD - (int)sizeof(t_packet_header)-(int)sizeof(t_packet_header_video_full) )
         video_link_profiles[i].packet_length = (int)MAX_PACKET_PAYLOAD - (int)sizeof(t_packet_header)-(int)sizeof(t_packet_header_video_full);

   if ( video_params.videoAdjustmentStrength < 1 || video_params.videoAdjustmentStrength > 10 )
      video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;
   return true;
}

void Model::resetToDefaults(bool generateId)
{
   log_line("Reseting vehicle settings to default.");
   strcpy(vehicle_name, DEFAULT_VEHICLE_NAME);
   iLoadedFileVersion = 0;

   struct timeval ste;
   gettimeofday(&ste, NULL); // get current time
   srand((unsigned int)(ste.tv_usec));
   if ( generateId )
      generateUID();
   else
      log_line("Reusing the same unique vehicle ID (%u).", vehicle_id);

   board_type = 0;
   controller_id = 0;
   sw_version = (SYSTEM_SW_VERSION_MAJOR * 256 + SYSTEM_SW_VERSION_MINOR) | (SYSTEM_SW_BUILD_NUMBER<<16);
   log_line("SW Version: %d.%d (b%d)", (sw_version >> 8) & 0xFF, sw_version & 0xFF, sw_version >> 16);
   vehicle_type = MODEL_TYPE_DRONE;
   is_spectator = true;
   constructLongName();

   enc_flags = 0;
   alarms = 0;
   uModelFlags = 0;

   radioInterfacesParams.interfaces_count = 0;
   resetRadioLinksParams();

   if ( hardware_is_station() )
   {
      hardware_info.radio_interface_count = 0;
      hardware_info.i2c_bus_count = 0;
      hardware_info.i2c_device_count = 0;
      hardware_info.serial_bus_count = 0;

      radioInterfacesParams.interfaces_count = 0;
      radioLinksParams.links_count = 0;
   }
   else
   {
      populateHWInfo();
      populateDefaultRadioInterfacesInfo();
   }
   iGPSCount = 1;

   enableDHCP = false;
   bDeveloperMode = false;
   uDeveloperFlags = (((u32)DEFAULT_DELAY_WIFI_CHANGE)<<8);

   resetRelayParamsToDefaults();

   clock_sync_type = CLOCK_SYNC_TYPE_NONE;

   niceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY;
   niceRC = DEFAULT_PRIORITY_PROCESS_RC;
   niceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
   ioNiceRouter = DEFAULT_IO_PRIORITY_ROUTER;
   niceVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_TX;
   niceOthers = DEFAULT_PRIORITY_PROCESS_OTHERS;
   ioNiceVideo = DEFAULT_IO_PRIORITY_VIDEO_TX;
   iOverVoltage = DEFAULT_OVERVOLTAGE;
   iFreqARM = DEFAULT_ARM_FREQ;
   iFreqGPU = DEFAULT_GPU_FREQ;

   resetVideoParamsToDefaults();
   resetCameraToDefaults(-1);

   if ( ! hardware_is_station() )
      validate_camera_settings();

   resetOSDFlags();

   audio_params.has_audio_device = 0;
   audio_params.enabled = false;
   audio_params.volume = 50;
   audio_params.quality = 1;
   audio_params.flags = 0x04 | (0x02<<8);

   resetRCParams();
   resetTelemetryParams();

   resetFunctionsParamsToDefaults();
}

void Model::resetRadioLinksParams()
{
   radioInterfacesParams.txPower = DEFAULT_RADIO_TX_POWER;
   radioInterfacesParams.txPowerAtheros = DEFAULT_RADIO_TX_POWER;
   radioInterfacesParams.txPowerRTL = DEFAULT_RADIO_TX_POWER;
   radioInterfacesParams.txMaxPower = MAX_TX_POWER;
   radioInterfacesParams.txMaxPowerAtheros = MAX_TX_POWER;
   radioInterfacesParams.txMaxPowerRTL = MAX_TX_POWER;
   radioInterfacesParams.slotTime = DEFAULT_RADIO_SLOTTIME;
   radioInterfacesParams.thresh62 = DEFAULT_RADIO_THRESH62;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radioInterfacesParams.interface_power[i] = DEFAULT_RADIO_TX_POWER;
      radioInterfacesParams.interface_link_id[i] = -1;
   }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radioLinksParams.link_frequency[i] = 0;
      radioLinksParams.link_capabilities_flags[i] = RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA | RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      radioLinksParams.link_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
      //if ( i > 0 )
      //   radioLinksParams.link_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_DISABLED;
      radioLinksParams.link_radio_flags[i] = DEFAULT_RADIO_FRAMES_FLAGS;
      radioLinksParams.link_datarates[i][0] = DEFAULT_DATARATE_VIDEO;
      radioLinksParams.link_datarates[i][1] = DEFAULT_DATARATE_DATA;
      radioLinksParams.bUplinkSameAsDownlink[i] = 1;
      radioLinksParams.uplink_radio_flags[i] = radioLinksParams.link_radio_flags[i];
      radioLinksParams.uplink_datarates[i][0] = radioLinksParams.link_datarates[i][0];
      radioLinksParams.uplink_datarates[i][1] = radioLinksParams.link_datarates[i][1];
   }

   for( unsigned int j=0; j<(sizeof(radioLinksParams.dummy)/sizeof(radioLinksParams.dummy[0])); j++ )
      radioLinksParams.dummy[j] = 0;
}

void Model::resetRelayParamsToDefaults()
{
   relay_params.is_RelayEnabled = false;
   relay_params.is_Relaying = false;
   relay_params.relayVehicleId = 0;
   relay_params.relayFlags = RELAY_FLAGS_NONE;
}


void Model::resetOSDFlags()
{
   memset(&osd_params, 0, sizeof(osd_params));
   osd_params.layout = osdLayout1;
   osd_params.voltage_alarm_enabled = true;
   osd_params.voltage_alarm = 3.2;
   osd_params.battery_show_per_cell = 1;
   osd_params.battery_cell_count = 0;
   osd_params.battery_capacity_percent_alarm = 0;
   osd_params.altitude_relative = true;
   osd_params.show_overload_alarm = true;
   osd_params.show_stats_rx_detailed = true;
   osd_params.show_stats_decode = true;
   osd_params.show_stats_rc = false;
   osd_params.show_full_stats = false;
   osd_params.invert_home_arrow = false;
   osd_params.home_arrow_rotate = 0;
   osd_params.show_instruments = false;
   osd_params.ahi_warning_angle = 45;
   osd_params.show_gps_position = false;
   
   osd_params.osd_flags[0] = 0; // horizontal layout for stats panels;
   osd_params.osd_flags[1] = OSD_FLAG_SHOW_DISTANCE | OSD_FLAG_SHOW_ALTITUDE | OSD_FLAG_SHOW_BATTERY;
   osd_params.osd_flags[2] = OSD_FLAG_SHOW_DISTANCE | OSD_FLAG_SHOW_ALTITUDE | OSD_FLAG_SHOW_BATTERY | OSD_FLAG_SHOW_HOME | OSD_FLAG_SHOW_VIDEO_MODE | OSD_FLAG_SHOW_CPU_INFO | OSD_FLAG_SHOW_FLIGHT_MODE | OSD_FLAG_SHOW_TIME | OSD_FLAG_SHOW_RADIO_INTERFACES_INFO;
   osd_params.osd_flags[3] = 0;
   osd_params.osd_flags[4] = 0;
   osd_params.osd_flags2[0] = OSD_FLAG_EXT_LAYOUT_ENABLED | OSD_FLAG_EXT_SHOW_BGBARS | OSD_FLAG_EXT_SHOW_STATS_VIDEO | OSD_FLAG_EXT_SHOW_STATS_RADIO_LINKS | OSD_FLAG_EXT_SHOW_STATS_RADIO_INTERFACES | OSD_FLAG_EXT_SHOW_RC_RSSI;
   osd_params.osd_flags2[1] = OSD_FLAG_EXT_LAYOUT_ENABLED |OSD_FLAG_EXT_SHOW_BGBARS | OSD_FLAG_EXT_RELATIVE_ALTITUDE | OSD_FLAG_EXT_SHOW_BATTERY_CELLS | OSD_FLAG_EXT_SHOW_RC_RSSI;
   osd_params.osd_flags2[2] = OSD_FLAG_EXT_LAYOUT_ENABLED |OSD_FLAG_EXT_SHOW_BGBARS | OSD_FLAG_EXT_RELATIVE_ALTITUDE | OSD_FLAG_EXT_SHOW_BATTERY_CELLS | OSD_FLAG_EXT_SHOW_RC_RSSI;
   osd_params.osd_flags2[3] = OSD_FLAG_EXT_LAYOUT_ENABLED |OSD_FLAG_EXT_SHOW_BGBARS | OSD_FLAG_EXT_SHOW_RC_RSSI;
   osd_params.osd_flags2[4] = OSD_FLAG_EXT_LAYOUT_ENABLED |OSD_FLAG_EXT_SHOW_BGBARS | OSD_FLAG_EXT_SHOW_RC_RSSI;


   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   {
      osd_params.osd_flags[i] |= OSD_FLAG_SHOW_BATTERY | OSD_FLAG_SHOW_DISTANCE | OSD_FLAG_SHOW_ALTITUDE | OSD_FLAG_SHOW_HOME;
      osd_params.osd_flags[i] |= OSD_FLAG_SHOW_GPS_INFO | OSD_FLAG_SHOW_FLIGHT_MODE | OSD_FLAG_SHOW_TIME | OSD_FLAG_SHOW_CPU_INFO;
      //osd_params.osd_flags[i] |= OSD_FLAG_SHOW_THROTTLE;
      if ( i < 3 )
         osd_params.osd_flags[i] |= OSD_FLAG_SHOW_VIDEO_MODE | OSD_FLAG_SHOW_VIDEO_MBPS | OSD_FLAG_SHOW_VIDEO_MODE_EXTENDED;
      osd_params.osd_flags[i] |= OSD_FLAG_SHOW_RADIO_LINKS | OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS;
      osd_params.osd_flags[i] |= OSD_FLAG_SHOW_RADIO_INTERFACES_INFO;
      osd_params.osd_flags2[i] |= OSD_FLAG_EXT_SHOW_RADIO_LINK_QUALITY_PERCENTAGE | OSD_FLAG_EXT_SHOW_RADIO_LINK_QUALITY_BARS | OSD_FLAG_EXT_SHOW_RADIO_LINK_INTERFACES_EXTENDED;
      osd_params.osd_flags2[i] |= OSD_FLAG_EXT_SHOW_BATTERY_CELLS;
      osd_params.osd_flags2[i] |= OSD_FLAG_EXT_SHOW_GROUND_SPEED;
      osd_params.osd_flags2[i] |= OSD_FLAG_EXT_SHOW_COMPACT_VIDEO_DECODE_STATS;
      osd_params.osd_flags2[i] |= OSD_FLAG_EXT_SHOW_BACKGROUND_ON_TEXTS_ONLY;

      osd_params.osd_flags3[i] = OSD_FLAG3_SHOW_GRID_DIAGONAL | OSD_FLAG3_SHOW_GRID_SQUARES;

      osd_params.osd_preferences[i] = ((u32)5) | (((u32)2)<<8) | (((u32)3)<<16) | ((((u32)2)<<16)<<4) | OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM;
      osd_params.osd_preferences[i] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT;
   }

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      osd_params.instruments_flags[i] = 0;
   //osd_params.instruments_flags[0] = INSTRUMENTS_FLAG_SHOW_ALL_OSD_PLUGINS_MASK;

   osd_params.osd_flags[1] = OSD_FLAG_SHOW_ALTITUDE | OSD_FLAG_SHOW_BATTERY;
   osd_params.osd_flags[1] |= OSD_FLAG_SHOW_BATTERY | OSD_FLAG_SHOW_DISTANCE | OSD_FLAG_SHOW_ALTITUDE;
   osd_params.osd_flags[1] |= OSD_FLAG_SHOW_FLIGHT_MODE | OSD_FLAG_SHOW_TIME;
   osd_params.osd_flags[1] |= OSD_FLAG_SHOW_RADIO_LINKS | OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS;
   osd_params.osd_flags2[1] = OSD_FLAG_EXT_LAYOUT_ENABLED |OSD_FLAG_EXT_SHOW_BGBARS | OSD_FLAG_EXT_RELATIVE_ALTITUDE | OSD_FLAG_EXT_SHOW_BATTERY_CELLS;
   osd_params.osd_flags2[1] |= OSD_FLAG_EXT_SHOW_BATTERY_CELLS;
   osd_params.osd_flags2[1] |= OSD_FLAG_EXT_SHOW_GROUND_SPEED;
   osd_params.osd_flags2[1] |= OSD_FLAG_EXT_SHOW_BACKGROUND_ON_TEXTS_ONLY;

   osd_params.osd_flags[2] |= OSD_FLAG_SHOW_BATTERY | OSD_FLAG_SHOW_DISTANCE | OSD_FLAG_SHOW_ALTITUDE | OSD_FLAG_SHOW_HOME;
   osd_params.osd_flags[2] |= OSD_FLAG_SHOW_GPS_INFO | OSD_FLAG_SHOW_FLIGHT_MODE | OSD_FLAG_SHOW_TIME;
   osd_params.osd_flags[2] |= OSD_FLAG_SHOW_VIDEO_MODE | OSD_FLAG_SHOW_VIDEO_MBPS;
   osd_params.osd_flags[2] |= OSD_FLAG_SHOW_RADIO_LINKS | OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS;
   osd_params.osd_flags2[2] = OSD_FLAG_EXT_LAYOUT_ENABLED |OSD_FLAG_EXT_SHOW_BGBARS | OSD_FLAG_EXT_RELATIVE_ALTITUDE | OSD_FLAG_EXT_SHOW_BATTERY_CELLS | OSD_FLAG_EXT_SHOW_RC_RSSI;
   osd_params.osd_flags2[2] |= OSD_FLAG_EXT_SHOW_RADIO_LINK_QUALITY_PERCENTAGE | OSD_FLAG_EXT_SHOW_RADIO_LINK_INTERFACES_EXTENDED;
   osd_params.osd_flags2[2] |= OSD_FLAG_EXT_SHOW_BATTERY_CELLS;
   osd_params.osd_flags2[2] |= OSD_FLAG_EXT_SHOW_GROUND_SPEED;
   osd_params.osd_flags2[2] |= OSD_FLAG_EXT_SHOW_BACKGROUND_ON_TEXTS_ONLY;
}

void Model::resetTelemetryParams()
{
   memset(&telemetry_params, 0, sizeof(telemetry_params));

   telemetry_params.fc_telemetry_type = TELEMETRY_TYPE_MAVLINK;
   
   telemetry_params.dummy5 = 0;
   telemetry_params.dummy6 = 0;
   telemetry_params.bControllerHasInputTelemetry = false;
   telemetry_params.bControllerHasOutputTelemetry = false;
   telemetry_params.controller_telemetry_type = 0;

   telemetry_params.update_rate = DEFAULT_FC_TELEMETRY_UPDATE_RATE;
   telemetry_params.vehicle_mavlink_id = DEFAULT_MAVLINK_SYS_ID_VEHICLE;
   telemetry_params.controller_mavlink_id = DEFAULT_MAVLINK_SYS_ID_CONTROLLER;

   telemetry_params.flags = TELEMETRY_FLAGS_RXTX | TELEMETRY_FLAGS_REQUEST_DATA_STREAMS | TELEMETRY_FLAGS_SPECTATOR_ENABLE;

   if ( 0 < hardware_info.serial_bus_count )
   {
      hardware_info.serial_bus_supported_and_usage[0] &= 0xFFFFFF00;
      hardware_info.serial_bus_supported_and_usage[0] |= SERIAL_PORT_USAGE_TELEMETRY;
      hardware_info.serial_bus_speed[0] = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;

      if ( hardware_is_vehicle() )
      if ( hardware_get_serial_ports_count() > 0 )
      {
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(0);
         if ( NULL != pPortInfo )
         {
            pPortInfo->lPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
            pPortInfo->iPortUsage = SERIAL_PORT_USAGE_TELEMETRY;
            hardware_serial_save_configuration();
         }
      }
   }
}

     
void Model::resetRCParams()
{
   memset(&rc_params, 0, sizeof(rc_params));

   rc_params.flags = RC_FLAGS_OUTPUT_ENABLED;
   rc_params.rc_enabled = false;
   rc_params.rc_frames_per_second = DEFAULT_RC_FRAMES_PER_SECOND;
   rc_params.dummy1 = false;
   rc_params.receiver_type = RECEIVER_TYPE_BUILDIN;
   rc_params.inputType = RC_INPUT_TYPE_NONE;

   rc_params.inputSerialPort = 0;
   rc_params.inputSerialPortSpeed = 57600;
   rc_params.outputSerialPort = 0;
   rc_params.outputSerialPortSpeed = 57600;
   rc_params.rc_failsafe_timeout_ms = DEFAULT_RC_FAILSAFE_TIME;
   rc_params.failsafeFlags = DEFAULT_RC_FAILSAFE_TYPE;
   rc_params.channelsCount = 8;
   rc_params.hid_id = 0;
   for( int i=0; i<MAX_RC_CHANNELS; i++ )
   {
      rc_params.rcChAssignment[i] = 0;
      rc_params.rcChMin[i] = DEFAULT_RC_CHANNEL_LOW_VALUE;
      rc_params.rcChMax[i] = DEFAULT_RC_CHANNEL_HIGH_VALUE;
      rc_params.rcChMid[i] = DEFAULT_RC_CHANNEL_MID_VALUE;
      rc_params.rcChExpo[i] = 0;
      rc_params.rcChFailSafe[i] = DEFAULT_RC_CHANNEL_FAILSAFE;
      rc_params.rcChFlags[i] = (DEFAULT_RC_FAILSAFE_TYPE << 1);
   }
   rc_params.rcChAssignmentThrotleReverse = 0;
   for( unsigned int i=0; i<(sizeof(rc_params.dummy)/sizeof(rc_params.dummy[0])); i++ )
      rc_params.dummy[i] = 0;

   camera_rc_channels = (((u32)0x07)<<24) | (0x03 << 30); // set only relative speed to middle;
}


void Model::resetCameraToDefaults(int iCameraIndex)
{
   if ( -1 == iCameraIndex )
   {
      iCameraCount = 1;
      iCurrentCamera = 0;
   }

   for( int k=0; k<MODEL_MAX_CAMERAS; k++ )
   {
      if ( iCameraIndex != -1 && iCameraIndex != k )
         continue;
      if ( iCameraIndex == -1 )
      {
         camera_params[k].iCameraType = 0;
         camera_params[k].iForcedCameraType = 0;
         camera_params[k].szCameraName[0] = 0;
         camera_params[k].iCurrentProfile = 0;
      }

      for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
      {
         resetCameraProfileToDefaults(&(camera_params[k].profiles[i]));

         if ( hardware_isCameraVeye() && k == 0 )
         {
            camera_params[k].profiles[i].brightness = 48;
            camera_params[k].profiles[i].contrast = 55;
            camera_params[k].profiles[i].saturation = 70;
            camera_params[k].profiles[i].sharpness = 110; // 100 is zero
            camera_params[k].profiles[i].whitebalance = 1; //auto
            camera_params[k].profiles[i].shutterspeed = 0; //auto
            if ( hardware_getCameraType() == CAMERA_TYPE_VEYE327 || hardware_getCameraType() == CAMERA_TYPE_VEYE290 )
               camera_params[k].profiles[i].drc = 0;
         }
      }
      // HDMI profile defaults:

      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].whitebalance = 0; // off
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].exposure = 7; // off
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].saturation = 90;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].sharpness = 100;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].analogGain = 2.0;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].awbGainB = 1.5;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].awbGainR = 1.4;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].vstab = 0;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].drc = 0;
   }
}

void Model::resetCameraProfileToDefaults(camera_profile_parameters_t* pCamParams)
{
   pCamParams->flags = 0;
   pCamParams->flip_image = 0;
   pCamParams->brightness = 48;
   pCamParams->contrast = 70;
   pCamParams->saturation = 70;
   pCamParams->sharpness = 110;
   pCamParams->exposure = 3; // sports   2; //backlight
   pCamParams->whitebalance = 1; //auto
   pCamParams->metering = 2; //backlight
   pCamParams->drc = 0; // off
   pCamParams->analogGain = 2.0;
   pCamParams->awbGainB = 1.5;
   pCamParams->awbGainR = 1.4;
   pCamParams->fovH = 72.0;
   pCamParams->fovV = 45.0;
   pCamParams->vstab = 0;
   pCamParams->ev = 0; // not set, auto
   pCamParams->iso = 0; // auto
   pCamParams->shutterspeed = 0; //auto
   for( int i=0; i<(int)(sizeof(pCamParams->dummy)/sizeof(pCamParams->dummy[0])); i++ )
      pCamParams->dummy[i] = 0;
}

void Model::resetFunctionsParamsToDefaults()
{
   functions_params.bEnableRCTriggerFreqSwitchLink1 = false;
   functions_params.bEnableRCTriggerFreqSwitchLink2 = false;
   functions_params.bEnableRCTriggerFreqSwitchLink3 = false;

   functions_params.iRCTriggerChannelFreqSwitchLink1 = -1;
   functions_params.iRCTriggerChannelFreqSwitchLink2 = -1;
   functions_params.iRCTriggerChannelFreqSwitchLink3 = -1;

   functions_params.bRCTriggerFreqSwitchLink1_is3Position = false;
   functions_params.bRCTriggerFreqSwitchLink2_is3Position = false;
   functions_params.bRCTriggerFreqSwitchLink3_is3Position = false;

   for( int i=0; i<3; i++ )
   {
      functions_params.uChannels433FreqSwitch[i] = 0xFFFFFFFF;
      functions_params.uChannels868FreqSwitch[i] = 0xFFFFFFFF;
      functions_params.uChannels23FreqSwitch[i] = 0xFFFFFFFF;
      functions_params.uChannels24FreqSwitch[i] = 0xFFFFFFFF;
      functions_params.uChannels25FreqSwitch[i] = 0xFFFFFFFF;
      functions_params.uChannels58FreqSwitch[i] = 0xFFFFFFFF;
   }
   for( unsigned int j=0; j<(sizeof(functions_params.dummy)/sizeof(functions_params.dummy[0])); j++ )
      functions_params.dummy[j] = 0;
}


int Model::getLinkDataRate(int nLinkId)
{
   int nRet = 0;

   if ( nLinkId >= 0 && nLinkId < radioLinksParams.links_count )
   {
      if ( radioLinksParams.link_datarates[nLinkId][0] > 0 )
         nRet = radioLinksParams.link_datarates[nLinkId][0];
      else
         nRet = getMCSRate((-radioLinksParams.link_datarates[nLinkId][0])-1);
   }
   return nRet;
}

int Model::getRadioInterfaceIndexForRadioLink(int iRadioLink)
{
   if ( iRadioLink < 0 || iRadioLink >= radioLinksParams.links_count )
      return -1;
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      if ( radioInterfacesParams.interface_link_id[i] == iRadioLink )
         return i;
   return -1;
}

bool Model::hasCamera()
{
   if ( camera_params[iCurrentCamera].iCameraType == 0 && camera_params[iCurrentCamera].iForcedCameraType == 0 )
      return false;
   return true;
}

char* Model::getCameraName(int iCameraIndex)
{
   if ( iCameraIndex < 0 || iCameraIndex >= iCameraCount )
      iCameraIndex = 0;
   return camera_params[iCameraIndex].szCameraName;
}

void Model::setCameraName(int iCameraIndex, const char* szCamName)
{
   if ( iCameraIndex < 0 || iCameraIndex >= iCameraCount )
      iCameraIndex = 0;

   if ( NULL == szCamName || 0 == szCamName[0] )
   {
      camera_params[iCameraIndex].szCameraName[0] = 0;
      return;
   }
   strncpy(camera_params[iCameraIndex].szCameraName, szCamName, MAX_CAMERA_NAME_LENGTH-1);
   camera_params[iCameraIndex].szCameraName[MAX_CAMERA_NAME_LENGTH-1] = 0;
}

u32 Model::getCameraType()
{
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
      return camera_params[iCurrentCamera].iForcedCameraType;
   return camera_params[iCurrentCamera].iCameraType;
}

bool Model::isCameraHDMI()
{
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
   {
      if ( camera_params[iCurrentCamera].iForcedCameraType == CAMERA_TYPE_HDMI )
         return true;
      return false;
   }
   if ( camera_params[iCurrentCamera].iCameraType == CAMERA_TYPE_HDMI )
      return true;
   return false;
}

bool Model::isCameraVeye()
{
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
   {
      if ( camera_params[iCurrentCamera].iForcedCameraType == CAMERA_TYPE_VEYE290 ||
        camera_params[iCurrentCamera].iForcedCameraType == CAMERA_TYPE_VEYE307 ||
        camera_params[iCurrentCamera].iForcedCameraType == CAMERA_TYPE_VEYE327 )
         return true;
      return false;
   }
   if ( camera_params[iCurrentCamera].iCameraType == CAMERA_TYPE_VEYE290 ||
        camera_params[iCurrentCamera].iCameraType == CAMERA_TYPE_VEYE307 ||
        camera_params[iCurrentCamera].iCameraType == CAMERA_TYPE_VEYE327 )
      return true;
   return false;
}

bool Model::isCameraVeye307()
{
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
   {
      if ( camera_params[iCurrentCamera].iForcedCameraType == CAMERA_TYPE_VEYE307 )
         return true;
      return false;
   }
   if ( camera_params[iCurrentCamera].iCameraType == CAMERA_TYPE_VEYE307 )
      return true;
   return false;
}

bool Model::isCameraVeye327290()
{
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
   {
      if ( camera_params[iCurrentCamera].iForcedCameraType == CAMERA_TYPE_VEYE290 ||
           camera_params[iCurrentCamera].iForcedCameraType == CAMERA_TYPE_VEYE327 )
         return true;
      return false;
   }
   if ( camera_params[iCurrentCamera].iCameraType == CAMERA_TYPE_VEYE290 ||
        camera_params[iCurrentCamera].iCameraType == CAMERA_TYPE_VEYE327 )
      return true;
   return false;
}

bool Model::isCameraCSICompatible()
{
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
   {
      if ( camera_params[iCurrentCamera].iForcedCameraType == CAMERA_TYPE_CSI ||
           camera_params[iCurrentCamera].iForcedCameraType == CAMERA_TYPE_HDMI )
         return true;
      return false;
   }
   if ( camera_params[iCurrentCamera].iCameraType == CAMERA_TYPE_CSI ||
        camera_params[iCurrentCamera].iCameraType == CAMERA_TYPE_HDMI )
      return true;
   return false;
}

void Model::setAWBMode()
{
   //if ( camera_params[iCurrentCamera].profiles[camera_params[iCurrentCamera].iCurrentProfile].flags & CAMERA_FLAG_AWB_MODE_OLD )
   //   hw_execute_bash_command("vcdbg set awb_mode 0 > /dev/null 2>&1", NULL);
   //else
   //   hw_execute_bash_command("vcdbg set awb_mode 1 > /dev/null 2>&1", NULL);
}

void Model::getCameraFlags(char* szCameraFlags)
{
   camera_profile_parameters_t* pParams = &(camera_params[iCurrentCamera].profiles[camera_params[iCurrentCamera].iCurrentProfile]);
   sprintf(szCameraFlags, "-br %d -co %d -sa %d -sh %d", pParams->brightness, pParams->contrast - 50, pParams->saturation - 100, pParams->sharpness-100);

   if ( ! isCameraVeye() )
   if ( pParams->flip_image != 0 )
      strcat(szCameraFlags, " -vf -hf");

   if ( pParams->vstab != 0 )
      strcat(szCameraFlags, " -vs");

   if ( pParams->ev >= 1 &&pParams->ev <= 21 )
   {
      char szBuff[32];
      sprintf(szBuff, " -ev %d", ((int)pParams->ev)-11);
      strcat(szCameraFlags, szBuff);
   }

   if ( pParams->iso != 0 )
   {
      char szBuff[32];
      sprintf(szBuff, " -ISO %d", (int)pParams->iso);
      strcat(szCameraFlags, szBuff);
   }

   if ( pParams->shutterspeed != 0 )
   {
      char szBuff[32];
      sprintf(szBuff, " -ss %d", (int)(1000000l/(long)(pParams->shutterspeed)));
      strcat(szCameraFlags, szBuff);
   }

   switch ( pParams->drc )
   {
      case 0: strcat(szCameraFlags, " -drc off"); break;
      case 1: strcat(szCameraFlags, " -drc low"); break;
      case 2: strcat(szCameraFlags, " -drc med"); break;
      case 3: strcat(szCameraFlags, " -drc high"); break;
   }

   switch ( pParams->exposure )
   {
      case 0: strcat(szCameraFlags, " -ex auto"); break;
      case 1: strcat(szCameraFlags, " -ex night"); break;
      case 2: strcat(szCameraFlags, " -ex backlight"); break;
      case 3: strcat(szCameraFlags, " -ex sports"); break;
      case 4: strcat(szCameraFlags, " -ex verylong"); break;
      case 5: strcat(szCameraFlags, " -ex fixedfps"); break;
      case 6: strcat(szCameraFlags, " -ex antishake"); break;
      case 7: strcat(szCameraFlags, " -ex off"); break;
   }

   switch ( pParams->whitebalance )
   {
      case 0: strcat(szCameraFlags, " -awb off"); break;
      case 1: strcat(szCameraFlags, " -awb auto"); break;
      case 2: strcat(szCameraFlags, " -awb sun"); break;
      case 3: strcat(szCameraFlags, " -awb cloud"); break;
      case 4: strcat(szCameraFlags, " -awb shade"); break;
      case 5: strcat(szCameraFlags, " -awb horizon"); break;
      case 6: strcat(szCameraFlags, " -awb grayworld"); break;
   }

   switch ( pParams->metering )
   {
      case 0: strcat(szCameraFlags, " -mm average"); break;
      case 1: strcat(szCameraFlags, " -mm spot"); break;
      case 2: strcat(szCameraFlags, " -mm backlit"); break;
      case 3: strcat(szCameraFlags, " -mm matrix"); break;
   }

   if ( 0 == pParams->whitebalance )
   {
      char szBuff[32];
      sprintf(szBuff, " -ag %.1f", pParams->analogGain);
      strcat(szCameraFlags, szBuff);
      sprintf(szBuff, " -awbg %.1f,%.1f", pParams->awbGainR, pParams->awbGainB);
      strcat(szCameraFlags, szBuff);
   }
   //if ( bDeveloperMode )
   //   strcat(szCameraFlags, " -a \"DEV\"");
}

void Model::getVideoFlags(char* szVideoFlags, int iVideoProfile, shared_mem_video_link_overwrites* pVideoOverwrites)
{
   camera_profile_parameters_t* pCamParams = &(camera_params[iCurrentCamera].profiles[camera_params[iCurrentCamera].iCurrentProfile]);

   char szBuff[128];
   u32 uBitrate = DEFAULT_VIDEO_BITRATE;
   if ( NULL == pVideoOverwrites )
      uBitrate = video_link_profiles[iVideoProfile].bitrate_fixed_bps;
   else
      uBitrate = video_link_profiles[pVideoOverwrites->currentVideoLinkProfile].bitrate_fixed_bps;

   if ( uBitrate > 6000000 )
      uBitrate = uBitrate*4/5;

   sprintf(szBuff, "-b %u", uBitrate);

   //if ( isCameraCSICompatible() )
   //{
   //   bool bUseAdaptiveVideo = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?true:false; 
   //   if ( bUseAdaptiveVideo )
   //      sprintf(szBuff, "-b %d", 1500000);
   //}

   char szKeyFrame[64];

   if ( video_link_profiles[iVideoProfile].keyframe > 0 )
      sprintf(szKeyFrame, "%d", video_link_profiles[iVideoProfile].keyframe );
   else
   {
      int keyframe = DEFAULT_VIDEO_AUTO_KEYFRAME_INTERVAL * video_link_profiles[iVideoProfile].fps / 1000;
      sprintf(szKeyFrame, "%d", keyframe );
   }

   if ( isCameraVeye() )
   {
      int fps = video_link_profiles[iVideoProfile].fps;
      int width = video_link_profiles[iVideoProfile].width;
      int height = video_link_profiles[iVideoProfile].height;

      if ( isCameraVeye307() )
      {
         if ( video_link_profiles[iVideoProfile].width > 1280 )
            sprintf(szVideoFlags, "-cd H264 -n -fl -md 0 -fps %d -g %s %s", fps, szKeyFrame, szBuff);
         else
            sprintf(szVideoFlags, "-cd H264 -n -fl -md 1 -fps %d -g %s %s", fps, szKeyFrame, szBuff);

         //sprintf(szVideoFlags, "-cd H264 -n -fl -w %d -h %d -fps %d -g %s %s", video_link_profiles[iVideoProfile].width, video_link_profiles[iVideoProfile].height, video_link_profiles[iVideoProfile].fps, szKeyFrame, szBuff);
      }
      else
      {
         if ( video_link_profiles[iVideoProfile].width > 1280 )
            sprintf(szVideoFlags, "-cd H264 -n -fl -w %d -h %d -fps %d -g %s %s", width, height, fps, szKeyFrame, szBuff);
         else
            sprintf(szVideoFlags, "-cd H264 -n -fl -w %d -h %d -fps %d -g %s %s", width, height, fps, szKeyFrame, szBuff);
      }
   }
   else
   {
      sprintf(szVideoFlags, "-cd H264 -n -fl -w %d -h %d -fps %d -g %s %s", video_link_profiles[iVideoProfile].width, video_link_profiles[iVideoProfile].height, video_link_profiles[iVideoProfile].fps, szKeyFrame, szBuff);
      //sprintf(szVideoFlags, "-cd MJPEG -n -fl -w %d -h %d -fps %d -g %s %s", video_params.width, video_params.height, video_params.fps, szKeyFrame, szBuff);
   }

   if ( video_link_profiles[iVideoProfile].h264quantization > 5 )
   {
      sprintf(szBuff, " -qp %d", video_link_profiles[iVideoProfile].h264quantization);
      strcat(szVideoFlags, szBuff);
   }

   if ( video_link_profiles[iVideoProfile].insertPPS )
      strcat(szVideoFlags, " -ih");

   if ( video_params.iH264Slices > 1 )
   if ( video_link_profiles[iVideoProfile].width <= 1280 )
   if ( video_link_profiles[iVideoProfile].h264quantization <= 720 )
   {
      sprintf(szBuff, " -sl %d", video_params.iH264Slices );
      strcat(szVideoFlags, szBuff);
   }

   if ( video_params.uExtraFlags & 0x01 )
      strcat(szVideoFlags, " -stm");

   switch ( video_link_profiles[iVideoProfile].h264profile )
   {
      case 0: strcat(szVideoFlags, " -pf baseline"); break;
      case 1: strcat(szVideoFlags, " -pf main"); break;
      case 2: strcat(szVideoFlags, " -pf high"); break;
      case 3: strcat(szVideoFlags, " -pf extended"); break;
   }

   switch ( video_link_profiles[iVideoProfile].h264level )
   {
      case 0: strcat(szVideoFlags, " -lev 4"); break;
      case 1: strcat(szVideoFlags, " -lev 4.1"); break;
      case 2: strcat(szVideoFlags, " -lev 4.2"); break;
   }

   switch ( video_link_profiles[iVideoProfile].h264refresh )
   {
      case 0: strcat(szVideoFlags, " -if cyclic"); break;
      case 1: strcat(szVideoFlags, " -if adaptive"); break;
      case 2: strcat(szVideoFlags, " -if both"); break;
      case 3: strcat(szVideoFlags, " -if cyclicrows"); break;
   }

   if ( pCamParams->flags & CAMERA_FLAG_FORCE_MODE_1 )
      strcat(szVideoFlags, " -md 1");
}

void Model::populateVehicleTelemetryData(t_packet_header_ruby_telemetry_extended* pdprte)
{
   if ( NULL == pdprte )
      return;
   if ( 0 == radioLinksParams.links_count || radioLinksParams.link_frequency[0] <= 0 )
      populateDefaultRadioInterfacesInfo();

   pdprte->vehicle_name[0] = 0;
   strncpy( (char*)pdprte->vehicle_name, vehicle_name, MAX_VEHICLE_NAME_LENGTH-1);
   pdprte->vehicle_name[MAX_VEHICLE_NAME_LENGTH-1] = 0;
   pdprte->vehicle_id = vehicle_id;
   pdprte->vehicle_type = vehicle_type;
   pdprte->radio_links_count = radioLinksParams.links_count;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      pdprte->radio_frequencies[i] = radioLinksParams.link_frequency[i];

   if ( telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE )
      pdprte->flags |= FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;
   else
      pdprte->flags &= ~FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;
}

void Model::populateFromVehicleTelemetryData(t_packet_header_ruby_telemetry_extended* pdprte)
{
   vehicle_id = pdprte->vehicle_id;
   strncpy(vehicle_name, (char*)pdprte->vehicle_name, MAX_VEHICLE_NAME_LENGTH-1);
   vehicle_name[MAX_VEHICLE_NAME_LENGTH-1] = 0;
   vehicle_type = pdprte->vehicle_type;

   if ( pdprte->flags & FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY )
      telemetry_params.flags |= TELEMETRY_FLAGS_SPECTATOR_ENABLE;
   else
      telemetry_params.flags &= ~TELEMETRY_FLAGS_SPECTATOR_ENABLE;

   u32 ver = pdprte->version;
   log_line("populateFromVehicleTelemetryData: sw version from telemetry stream: %d.%d", ver>>4, ver & 0x0F);
   if ( ver > 0 )
   {
      sw_version = ((ver>>4)) * 256 + ((ver & 0x0F)*10);
      log_line("populateFromVehicleTelemetryData: set sw version to: %d.%d", (sw_version>>8) & 0xFF, (sw_version & 0xFF)/10);
   }

   resetRadioLinksParams();
   radioLinksParams.links_count = pdprte->radio_links_count;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      radioLinksParams.link_frequency[i] = pdprte->radio_frequencies[i];

   radioInterfacesParams.interfaces_count = 1;
   radioInterfacesParams.interface_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
   radioInterfacesParams.interface_link_id[0] = 0;
   radioInterfacesParams.interface_current_frequency[0] = radioLinksParams.link_frequency[0];
   strcpy(radioInterfacesParams.interface_szMAC[0], "XXXXXX");
   strcpy(radioInterfacesParams.interface_szPort[0], "X");
   radioInterfacesParams.interface_datarates[0][0] = 0;
   radioInterfacesParams.interface_datarates[0][1] = 0;

   if ( (radioLinksParams.links_count > 1) && (radioLinksParams.link_frequency[1] != 0) )
   {
      radioInterfacesParams.interfaces_count = 2;
      radioInterfacesParams.interface_capabilities_flags[1] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      radioInterfacesParams.interface_capabilities_flags[1] |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
      radioInterfacesParams.interface_link_id[1] = 1;
      radioInterfacesParams.interface_current_frequency[1] = radioLinksParams.link_frequency[1];
      strcpy(radioInterfacesParams.interface_szMAC[1], "XXXXXX");
      strcpy(radioInterfacesParams.interface_szPort[1], "X");
      radioInterfacesParams.interface_datarates[1][0] = 0;
      radioInterfacesParams.interface_datarates[1][1] = 0;
   }

   constructLongName();

   log_line("populateFromVehicleTelemetryData: %d radio links: freq1: %d Mhz, freq2: %d Mhz; %d radio interfaces.", radioLinksParams.links_count, radioLinksParams.link_frequency[0], radioLinksParams.link_frequency[1], radioInterfacesParams.interfaces_count);
}

int Model::get_video_link_profile_max_level_shifts(int iProfile)
{
   if ( iProfile < 0 || iProfile >= MAX_VIDEO_LINK_PROFILES )
      return 0;

   int nLevels = video_link_profiles[iProfile].block_packets - video_link_profiles[iProfile].block_fecs;
   //if ( iProfile == VIDEO_PROFILE_LQ )
   //   nLevels += video_link_profiles[iProfile].block_packets/2;
   if ( nLevels < 0 )
      nLevels = 0;
   return nLevels;
}

const char* Model::getShortName()
{
   if ( 0 == vehicle_name[0] )
      return "No Name";
   return vehicle_name;
}

const char* Model::getLongName()
{
   return vehicle_long_name;
}

void Model::constructLongName()
{
   switch ( vehicle_type )
   {
      case MODEL_TYPE_DRONE: strcpy(vehicle_long_name, "drone "); break;
      case MODEL_TYPE_AIRPLANE: strcpy(vehicle_long_name, "airplane "); break;
      case MODEL_TYPE_HELI: strcpy(vehicle_long_name, "helicopter "); break;
      case MODEL_TYPE_CAR: strcpy(vehicle_long_name, "car "); break;
      case MODEL_TYPE_BOAT: strcpy(vehicle_long_name, "boat "); break;
      case MODEL_TYPE_ROBOT: strcpy(vehicle_long_name, "robot "); break;
      default: strcpy(vehicle_long_name, "vehicle "); break;
   }
   strcat(vehicle_long_name, getShortName());
}

const char* Model::getVehicleType(u8 vtype)
{
   switch ( vtype )
   {
      case MODEL_TYPE_DRONE: return "drone";
      case MODEL_TYPE_AIRPLANE: return "airplane";
      case MODEL_TYPE_HELI: return "helicopter";
      case MODEL_TYPE_CAR: return "car";
      case MODEL_TYPE_BOAT: return "boat";
      case MODEL_TYPE_ROBOT: return "robot";
      default: return "vehicle";
   }
}

const char* Model::getVehicleTypeString()
{
   return Model::getVehicleType(vehicle_type);
}

void Model::get_rc_channel_name(int nChannel, char* szOutput)
{
   if ( NULL == szOutput )
      return;

   szOutput[0] = 0;
   if ( nChannel == 0 )
      sprintf(szOutput, "Ch %d [AEL]:", nChannel+1 );
   if ( nChannel == 1 )
      sprintf(szOutput, "Ch %d [ELV]:", nChannel+1 );
   if ( nChannel == 2 )
      sprintf(szOutput, "Ch %d [THR]:", nChannel+1 );
   if ( nChannel == 3 )
      sprintf(szOutput, "Ch %d [RUD]:", nChannel+1 );
   if ( nChannel > 3 )
      sprintf(szOutput, "Ch %d [AUX%d]:", nChannel+1, nChannel-3);

   u32 pitch = camera_rc_channels & 0x1F;
   if ( pitch > 0 && pitch == (u32)nChannel+1 )
      sprintf(szOutput, "Ch %d [CAM P]:", nChannel+1);

   u32 roll = (camera_rc_channels>>8) & 0x1F;
   if ( roll > 0 && roll == (u32)nChannel+1 )
      sprintf(szOutput, "Ch %d [CAM R]:", nChannel+1);

   u32 yaw = (camera_rc_channels>>16) & 0x1F;
   if ( yaw > 0 && yaw == (u32)nChannel+1 )
      sprintf(szOutput, "Ch %d [CAM Y]:", nChannel+1);
}

void Model::updateStatsMaxCurrentVoltage(t_packet_header_fc_telemetry* pPHFCT)
{
   if ( NULL == pPHFCT )
      return;

   if ( pPHFCT->flight_mode & FLIGHT_MODE_ARMED )
   if ( pPHFCT->current > m_Stats.uCurrentMaxCurrent )
      m_Stats.uCurrentMaxCurrent = pPHFCT->current;

   if ( pPHFCT->flight_mode & FLIGHT_MODE_ARMED )
   if ( pPHFCT->voltage > 1000 )
   if ( pPHFCT->voltage < m_Stats.uCurrentMinVoltage )
      m_Stats.uCurrentMinVoltage = pPHFCT->voltage;

   if ( pPHFCT->flight_mode & FLIGHT_MODE_ARMED )
   if ( m_Stats.uTotalMaxCurrent < m_Stats.uCurrentMaxCurrent )
      m_Stats.uTotalMaxCurrent = m_Stats.uCurrentMaxCurrent;

   if ( pPHFCT->flight_mode & FLIGHT_MODE_ARMED )
   if ( m_Stats.uTotalMinVoltage > m_Stats.uCurrentMinVoltage )
      m_Stats.uTotalMinVoltage = m_Stats.uCurrentMinVoltage;
}

void Model::updateStatsEverySecond(t_packet_header_fc_telemetry* pPHFCT)
{
   if ( NULL == pPHFCT )
      return;

   m_Stats.uCurrentOnTime++;
   m_Stats.uTotalOnTime++;

   updateStatsMaxCurrentVoltage(pPHFCT);

   m_Stats.uCurrentTotalCurrent += pPHFCT->current*10/3600;
   m_Stats.uTotalTotalCurrent += pPHFCT->current*10/3600;

   float alt = pPHFCT->altitude_abs/100.0f-1000.0;
   if ( osd_params.altitude_relative )
      alt = pPHFCT->altitude/100.0f-1000.0;
    
   if ( pPHFCT->flight_mode & FLIGHT_MODE_ARMED )
   if ( alt > m_Stats.uCurrentMaxAltitude )
      m_Stats.uCurrentMaxAltitude = alt;

   if ( pPHFCT->flight_mode & FLIGHT_MODE_ARMED )
   if ( pPHFCT->distance/100.0 > m_Stats.uCurrentMaxDistance )
      m_Stats.uCurrentMaxDistance = pPHFCT->distance/100.0;

   if ( pPHFCT->flight_mode & FLIGHT_MODE_ARMED )
   if ( m_Stats.uTotalMaxAltitude < m_Stats.uCurrentMaxAltitude )
      m_Stats.uTotalMaxAltitude = m_Stats.uCurrentMaxAltitude;

   if ( pPHFCT->flight_mode & FLIGHT_MODE_ARMED )
   if ( m_Stats.uTotalMaxDistance < m_Stats.uCurrentMaxDistance )
      m_Stats.uTotalMaxDistance = m_Stats.uCurrentMaxDistance;

   if ( pPHFCT->flight_mode != 0 ) 
   if ( pPHFCT->flight_mode & FLIGHT_MODE_ARMED )
   if ( pPHFCT->arm_time > 2 )
   {
      m_Stats.uCurrentFlightTime++;
      m_Stats.uTotalFlightTime++;
      m_Stats.uCurrentFlightTotalCurrent += pPHFCT->current*10/3600;

      float speedMetersPerSecond = pPHFCT->hspeed/100.0f-1000.0f;

      m_Stats.uCurrentFlightDistance += (speedMetersPerSecond*100.0f);
      m_Stats.uTotalFlightDistance += (speedMetersPerSecond*100.0f);
   }
}


int Model::getSaveCount()
{
   return iSaveCount;
}
