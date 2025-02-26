/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "base.h"
#include "models.h"
#include <stdlib.h>
#include <math.h>
#include "config.h"
#include "ctrl_preferences.h"
#include "hardware.h"
#include "hardware_audio.h"
#include "hardware_camera.h"
#include "hw_procs.h"
#include "hardware_i2c.h"
#include "camera_utils.h"
#include "utils.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"

#define MODEL_FILE_STAMP_ID "vVIII.3stamp"

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

Model::Model(void)
{
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      memset((u8*)&(video_link_profiles[i]), 0, sizeof(type_video_link_profile));

   memset((u8*)&video_params, 0, sizeof(video_parameters_t));
   memset((u8*)&camera_params, 0, sizeof(type_camera_parameters));
   for(int i=0; i<MODEL_MAX_CAMERAS; i++ )
   for(int k=0; k<MODEL_CAMERA_PROFILES; k++)
   {
      camera_params[i].profiles[k].analogGain = 0;
      camera_params[i].profiles[k].awbGainB = 0;
      camera_params[i].profiles[k].awbGainR = 0;
      camera_params[i].profiles[k].fovV = 0;
      camera_params[i].profiles[k].fovH = 0;
   }

   memset((u8*)&osd_params, 0, sizeof(osd_parameters_t));
   osd_params.voltage_alarm = 0;
   memset((u8*)&rc_params, 0, sizeof(rc_parameters_t));
   memset((u8*)&telemetry_params, 0, sizeof(telemetry_parameters_t));
   memset((u8*)&audio_params, 0, sizeof(audio_parameters_t));
   memset((u8*)&functions_params, 0, sizeof(type_functions_parameters));

   memset((u8*)&hardwareInterfacesInfo, 0, sizeof(type_vehicle_hardware_interfaces_info));
   memset((u8*)&radioInterfacesParams, 0, sizeof(type_radio_interfaces_parameters));
   memset((u8*)&radioLinksParams, 0, sizeof(type_radio_links_parameters));

   memset((u8*)&m_Stats, 0, sizeof(type_vehicle_stats_info));

   iSaveCount = 0;
   b_mustSyncFromVehicle = false;
   iCameraCount = 0;
   iCurrentCamera = -1;
   vehicle_name[0] = 0;
   vehicle_long_name[0] = 0;
   iLoadedFileVersion = 0;
   radioInterfacesParams.interfaces_count = 0;
   m_iRadioInterfacesGraphRefreshInterval = 3;
   is_spectator = false;
   uVehicleId = 0;
   uControllerId = 0;
   uModelFlags = 0;
   iGPSCount = 0;
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

bool Model::isRunningOnOpenIPCHardware()
{
   if ( hardware_board_is_openipc(hwCapabilities.uBoardType & BOARD_TYPE_MASK) )
      return true;
   return false;
}

bool Model::isRunningOnPiHardware()
{
   if ( hardware_board_is_raspberry(hwCapabilities.uBoardType & BOARD_TYPE_MASK) )
      return true;
   return false;
}

bool Model::isRunningOnRadxaHardware()
{
   if ( hardware_board_is_radxa(hwCapabilities.uBoardType & BOARD_TYPE_MASK) )
      return true;
   return false;
}

bool Model::reloadIfChanged(bool bLoadStats)
{
   char szFile[MAX_FILE_PATH_SIZE];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CURRENT_VEHICLE_MODEL);
   FILE* fd = fopen(szFile, "r");
   if ( NULL == fd )
      return false;

   int iV, iS = 0;
   if ( 1 == fscanf(fd, "%*s %d", &iV) )
   if ( 1 == fscanf(fd, "%*s %d", &iS) )
   if ( iS != iSaveCount )
   {
      log_line("Model: changed. Reload");
      fclose(fd);
      return loadFromFile(szFile, bLoadStats);
   }
   fclose(fd);
   return true;
}

bool Model::loadFromFile(const char* filename, bool bLoadStats)
{
   char szFileNormal[MAX_FILE_PATH_SIZE];
   char szFileBackup[MAX_FILE_PATH_SIZE];

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
            bMainFileLoadedOk = loadVersion8(fd);
         if ( 9 == iVersionMain )
            bMainFileLoadedOk = loadVersion9(fd);
         if ( 10 == iVersionMain )
            bMainFileLoadedOk = loadVersion10(fd);
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
      char szFreq1[64];
      char szFreq2[64];
      char szFreq3[64];
      strcpy(szFreq1, str_format_frequency(radioLinksParams.link_frequency_khz[0]));
      strcpy(szFreq2, str_format_frequency(radioLinksParams.link_frequency_khz[1]));
      strcpy(szFreq3, str_format_frequency(radioLinksParams.link_frequency_khz[2]));

      log_line("Loaded vehicle (%s) successfully from file: %s; name: [%s], VID: %u, %s, software: %d.%d (b%d), on time: %02d:%02d",
         bLoadStats?"with stats":"without stats",
         filename, vehicle_name, uVehicleId, 
         is_spectator?"spectator mode": "control mode",
         (sw_version >> 8) & 0xFF, sw_version & 0xFF, sw_version>>16,
         m_Stats.uCurrentOnTime/60, m_Stats.uCurrentOnTime%60);
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
            bBackupFileLoadedOk = loadVersion8(fd);
         if ( 9 == iVersionBackup )
            bBackupFileLoadedOk = loadVersion9(fd);
         if ( 10 == iVersionBackup )
            bBackupFileLoadedOk = loadVersion10(fd);
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
   log_line("Loaded vehicle successfully (%d ms) from backup file: %s; version %d, save count: %d, vehicle name: [%s], vehicle id: %u, software: %d.%d (b%d), is in control mode: %s", timeStart, filename, iLoadedFileVersion, iSaveCount, vehicle_name, uVehicleId, (sw_version >> 8) & 0xFF, sw_version & 0xFF, sw_version>>16, is_spectator?"no (is spectator)":"yes");

   constructLongName();
   
   fd = fopen(szFileNormal, "w");
   if ( NULL != fd )
   {
      saveVersion10(fd, false);
      fclose(fd);
      log_line("Restored main model file from backup model file.");
   }
   else
      log_softerror_and_alarm("Failed to write main model file from backup model file.");

   return true;
}


bool Model::loadVersion8(FILE* fd)
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

   if ( 4 != fscanf(fd, "%*s %u %d %d %u", &sw_version, &uVehicleId, &uControllerId, &hwCapabilities.uBoardType) )
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

   if ( 3 != fscanf(fd, "%d %u %d", &rxtx_sync_type, &camera_rc_channels, &processesPriorities.iNiceTelemetry ) )
      { log_softerror_and_alarm("Load model8: Error on line 3"); return false; }

   if ( 4 != fscanf(fd, "%d %d %u %d", &tmp1, &vt, &m_Stats.uTotalFlightTime, &iGPSCount ) )
      { log_softerror_and_alarm("Load model8: Error on line 3b"); return false; }

   is_spectator = (bool)tmp1;
   vehicle_type = vt;

   //----------------------------------------
   // CPU

   if ( 3 != fscanf(fd, "%*s %d %d %d", &processesPriorities.iNiceVideo, &processesPriorities.iNiceOthers, &processesPriorities.ioNiceVideo) )
      { log_softerror_and_alarm("Load model8: Error on line 4"); return false; }
   if ( 3 != fscanf(fd, "%d %d %d", &processesPriorities.iOverVoltage, &processesPriorities.iFreqARM, &processesPriorities.iFreqGPU) )
      { log_softerror_and_alarm("Load model8: Error on line 5"); }
   if ( 3 != fscanf(fd, "%d %d %d", &processesPriorities.iNiceRouter, &processesPriorities.ioNiceRouter, &processesPriorities.iNiceRC) )
      { log_softerror_and_alarm("Load model8: Error on extra line 2b"); }

   //----------------------------------------
   // Radio

   if ( 1 != fscanf(fd, "%*s %d", &radioInterfacesParams.interfaces_count) )
      { log_softerror_and_alarm("Load model8: Error on line 6"); return false; }

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      char szTmp[256];
      if ( 3 != fscanf(fd, "%d %d %u", &(radioInterfacesParams.interface_card_model[i]), &(radioInterfacesParams.interface_link_id[i]), &(radioInterfacesParams.interface_current_frequency_khz[i])) )
         { log_softerror_and_alarm("Load model8: Error on line 7a"); return false; }

      radioInterfacesParams.interface_current_frequency_khz[i] *= 1000;
      
      if ( 8 != fscanf(fd, "%u %d %u %d %d %d %s %s", &u4, &tmp2, &tmp32, &tmp5, &tmp6, &tmp7, radioInterfacesParams.interface_szMAC[i], szTmp) )
         { log_softerror_and_alarm("Load model8: Error on line 7b"); return false; }
      radioInterfacesParams.interface_capabilities_flags[i] = u4;
      radioInterfacesParams.interface_supported_bands[i] = (u8)tmp2;
      radioInterfacesParams.interface_radiotype_and_driver[i] = tmp32;
      radioInterfacesParams.interface_current_radio_flags[i] = tmp5;
      radioInterfacesParams.interface_raw_power[i] = tmp6;
      
      radioInterfacesParams.interface_szMAC[i][strlen(radioInterfacesParams.interface_szMAC[i])-1] = 0;
      szTmp[strlen(szTmp)-1] = 0;
      szTmp[2] = 0;
      strcpy(radioInterfacesParams.interface_szPort[i], szTmp);      
   }

   if ( 9 != fscanf(fd, "%d %d %d %d %d %d %d %d %d", &tmp1, &radioInterfacesParams.iAutoVehicleTxPower, &radioInterfacesParams.iAutoControllerTxPower, &radioInterfacesParams.iDummyR3, &radioInterfacesParams.iDummyR4, &radioInterfacesParams.iDummyR5, &radioInterfacesParams.iDummyR6,  &radioInterfacesParams.iDummyR7, &radioInterfacesParams.iDummyR8) )
      { log_softerror_and_alarm("Load model8: Error on line 8"); return false; }
   enableDHCP = (bool)tmp1;

   if ( 1 != fscanf(fd, "%*s %d", &radioLinksParams.links_count) )
      { log_softerror_and_alarm("Load model8: Error on line r0"); return false; }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      if ( 5 != fscanf(fd, "%u %u %u %d %d", &(radioLinksParams.link_frequency_khz[i]), &(radioLinksParams.link_capabilities_flags[i]), &(radioLinksParams.link_radio_flags[i]), &(radioLinksParams.link_datarate_video_bps[i]), &(radioLinksParams.link_datarate_data_bps[i])) )
         { log_softerror_and_alarm("Load model8: Error on line r3"); return false; }

      radioLinksParams.link_frequency_khz[i] *= 1000;
      radioLinksParams.link_datarate_video_bps[i] *= 1000 * 1000;
      radioLinksParams.link_datarate_data_bps[i] *= 1000 * 1000;
      if ( 4 != fscanf(fd, "%d %u %d %d", &tmp1, &(radioLinksParams.uDummy2[i]), &(radioLinksParams.uplink_datarate_video_bps[i]), &(radioLinksParams.uplink_datarate_data_bps[i])) )
         { log_softerror_and_alarm("Load model8: Error on line r4"); return false; }
      radioLinksParams.uSerialPacketSize[i] = tmp1;
      if ( (radioLinksParams.uSerialPacketSize[i] < DEFAULT_RADIO_SERIAL_AIR_MIN_PACKET_SIZE) ||
           (radioLinksParams.uSerialPacketSize[i] > DEFAULT_RADIO_SERIAL_AIR_MAX_PACKET_SIZE) )
         radioLinksParams.uSerialPacketSize[i] = DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE;
      radioLinksParams.uplink_datarate_video_bps[i] *= 1000 * 1000;
      radioLinksParams.uplink_datarate_data_bps[i] *= 1000 * 1000;
   }

   for( unsigned int j=0; j<12; j++ )
   {
      if ( 1 != fscanf(fd, "%d", &tmp1) )
         { log_softerror_and_alarm("Load model8: Error on line radio4 dummy data"); return false; }
   }

   if ( 4 != fscanf(fd, "%*s %d %d %u %u", &relay_params.isRelayEnabledOnRadioLinkId, &tmp2, &relay_params.uRelayedVehicleId, &relay_params.uRelayCapabilitiesFlags) )
      { log_softerror_and_alarm("Load model8: Error on line 10"); return false; }
   relay_params.uCurrentRelayMode = tmp2;

   //----------------------------------------
   // Telemetry

   if ( 5 != fscanf(fd, "%*s %d %d %d %d %d", &telemetry_params.fc_telemetry_type, &telemetry_params.controller_telemetry_type, &telemetry_params.update_rate, &tmp1, &tmp2) )
      { log_softerror_and_alarm("Load model8: Error on line 12"); return false; }

   if ( telemetry_params.update_rate > 200 )
      telemetry_params.update_rate = 200;
   telemetry_params.bControllerHasOutputTelemetry = (bool) tmp1;
   telemetry_params.bControllerHasInputTelemetry = (bool) tmp2;

   if ( 4 != fscanf(fd, "%d %u %d %u", &telemetry_params.iVideoBitrateHistoryGraphSampleInterval, &telemetry_params.dummy2, &telemetry_params.dummy3, &telemetry_params.dummy4) )
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
   {
      video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES;
      if ( hardware_board_is_openipc(hardware_getOnlyBoardType()) )
         video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES_OIPC;
   }
   video_params.videoAdjustmentStrength = tmp2;
   if ( video_params.lowestAllowedAdaptiveVideoBitrate < 250000 )
      video_params.lowestAllowedAdaptiveVideoBitrate = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;

   if ( 1 != fscanf(fd, "%u", &video_params.uMaxAutoKeyframeIntervalMs) )
      { log_softerror_and_alarm("Load model8: Error on line 20"); return false; }
   if ( video_params.uMaxAutoKeyframeIntervalMs < 50 || video_params.uMaxAutoKeyframeIntervalMs > DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL )
       video_params.uMaxAutoKeyframeIntervalMs = DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL;

   if ( 1 != fscanf(fd, "%u", &video_params.uVideoExtraFlags) )
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
      { video_link_profiles[i].uProfileFlags = 0; video_link_profiles[i].uProfileEncodingFlags = 0; }

   if ( tmp7 < 0 || tmp7 > MAX_VIDEO_LINK_PROFILES )
      tmp7 = MAX_VIDEO_LINK_PROFILES;

   for( int i=0; i<tmp7; i++ )
   {
      if ( ! bOk )
        { log_softerror_and_alarm("Load model8: Error on video link profiles 0"); bOk = false; }
 
      if ( bOk && (6 != fscanf(fd, "%u %u %u %d %d %u", &(video_link_profiles[i].uProfileFlags), &(video_link_profiles[i].uProfileEncodingFlags), &(video_link_profiles[i].bitrate_fixed_bps), &(video_link_profiles[i].radio_datarate_video_bps), &(video_link_profiles[i].radio_datarate_data_bps), &(video_link_profiles[i].radio_flags))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 1"); bOk = false; }

      video_link_profiles[i].radio_datarate_video_bps *= 1000 * 1000;
      video_link_profiles[i].radio_datarate_data_bps *= 1000 * 1000;

      if ( bOk && (2 != fscanf(fd, "%d %d", &(video_link_profiles[i].width), &(video_link_profiles[i].height))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 2"); bOk = false; }

      if ( bOk && (5 != fscanf(fd, "%d %d %d %d %d", &(video_link_profiles[i].block_packets), &(video_link_profiles[i].block_fecs), &(video_link_profiles[i].video_data_length), &(video_link_profiles[i].fps), &(video_link_profiles[i].keyframe_ms))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 3"); bOk = false; }
      
      if ( bOk && (5 != fscanf(fd, "%d %d %d %d %d", &(video_link_profiles[i].h264profile), &(video_link_profiles[i].h264level), &(video_link_profiles[i].h264refresh), &(video_link_profiles[i].h264quantization), &(video_link_profiles[i].iIPQuantizationDelta))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 4"); bOk = false; }
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
         if ( 1 != fscanf(fd, "%*s %d", &camera_params[k].profiles[i].uFlags) )
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
         else
            camera_params[k].profiles[i].wdr = (u8)tmp1;

         if ( 1 != fscanf(fd, "%d", &tmp1) )
            { log_softerror_and_alarm("Load model8: Error on line 25c, night mode, cam profile %d", i); camera_params[k].profiles[i].dayNightMode = 0; }
         else
            camera_params[k].profiles[i].dayNightMode = (u8)tmp1;
           
         if ( 1 != fscanf(fd, "%d", &tmp1) )
            { log_softerror_and_alarm("Load model8: Error on line 25f, hue, cam profile %d", i); camera_params[k].profiles[i].hue = 0; }
         else
            camera_params[k].profiles[i].hue = (u8)tmp1;
         for( unsigned int j=0; j<(sizeof(camera_params[k].profiles[i].dummyCamP)/sizeof(camera_params[k].profiles[i].dummyCamP[0])); j++ )
         {
            if ( 1 != fscanf(fd, "%d", &tmp1) )
               { log_softerror_and_alarm("Load model8: Error on line 27, cam profile %d", i); return false; }
            camera_params[k].profiles[i].dummyCamP[j] = tmp1;
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
   if ( 4 == fscanf(fd, "%d %d %d %u", &tmp2, &audio_params.volume, &audio_params.quality, &audio_params.uFlags) )
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

   if ( 4 == fscanf(fd, "%*s %d %d %d %d", &hardwareInterfacesInfo.radio_interface_count, &hardwareInterfacesInfo.i2c_bus_count, &hardwareInterfacesInfo.i2c_device_count, &hardwareInterfacesInfo.serial_port_count) )
   {
      if ( hardwareInterfacesInfo.i2c_bus_count < 0 || hardwareInterfacesInfo.i2c_bus_count > MAX_MODEL_I2C_BUSSES )
         { log_softerror_and_alarm("Load model8: Error on hw info1"); return false; }
      if ( hardwareInterfacesInfo.i2c_device_count < 0 || hardwareInterfacesInfo.i2c_device_count > MAX_MODEL_I2C_DEVICES )
         { log_softerror_and_alarm("Load model8: Error on hw info2"); return false; }
      if ( hardwareInterfacesInfo.serial_port_count < 0 || hardwareInterfacesInfo.serial_port_count > MAX_MODEL_SERIAL_PORTS )
         { log_softerror_and_alarm("Load model8: Error on hw info3"); return false; }

      for( int i=0; i<hardwareInterfacesInfo.i2c_bus_count; i++ )
         if ( 1 != fscanf(fd, "%d", &(hardwareInterfacesInfo.i2c_bus_numbers[i])) )
            { log_softerror_and_alarm("Load model8: Error on hw info4"); return false; }

      for( int i=0; i<hardwareInterfacesInfo.i2c_device_count; i++ )
         if ( 2 != fscanf(fd, "%d %d", &(hardwareInterfacesInfo.i2c_devices_bus[i]), &(hardwareInterfacesInfo.i2c_devices_address[i])) )
            { log_softerror_and_alarm("Load model8: Error on hw info5"); return false; }

      for( int i=0; i<hardwareInterfacesInfo.serial_port_count; i++ )
         if ( 2 != fscanf(fd, "%u %s", &(hardwareInterfacesInfo.serial_port_supported_and_usage[i]), &(hardwareInterfacesInfo.serial_port_names[i][0])) )
            { log_softerror_and_alarm("Load model8: Error on hw info6"); return false; }
   }
   else
   {
      hardwareInterfacesInfo.radio_interface_count = 0;
      hardwareInterfacesInfo.i2c_bus_count = 0;
      hardwareInterfacesInfo.i2c_device_count = 0;
      hardwareInterfacesInfo.serial_port_count = 0;
      bOk = false;
   }

   //----------------------------------------
   // OSD

   if ( 5 != fscanf(fd, "%*s %d %d %f %d %d", &osd_params.iCurrentOSDLayout, &tmp1, &osd_params.voltage_alarm, &tmp2, &tmp3) )
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

   if ( 1 != fscanf(fd, "%d", &rc_params.iRCTranslationType ) )
      { log_softerror_and_alarm("Load model8: Error on line 37e"); return false; }

   for( unsigned int i=0; i<(sizeof(rc_params.rcDummy)/sizeof(rc_params.rcDummy[0])); i++ )
      if ( 1 != fscanf(fd, "%u", &(rc_params.rcDummy[i])) )
         { log_softerror_and_alarm("Load model8: Error on line 37"); return false; }

   if ( rc_params.rc_failsafe_timeout_ms < 50 || rc_params.rc_failsafe_timeout_ms > 5000 )
      rc_params.rc_failsafe_timeout_ms = DEFAULT_RC_FAILSAFE_TIME;

   //----------------------------------------
   // Misc

   if ( 2 != fscanf(fd, "%*s %u %u", &uDummyM1, &uDeveloperFlags) )
      { log_softerror_and_alarm("Load model8: Error on line 39"); uDeveloperFlags = (((u32)DEFAULT_DELAY_WIFI_CHANGE)<<8); }

   if ( 1 != fscanf(fd, "%u", &enc_flags) )
   {
      enc_flags = MODEL_ENC_FLAGS_NONE;
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
      for( int i=0; i<hardwareInterfacesInfo.serial_port_count; i++ )
         if ( 1 != fscanf(fd, "%d", &(hardwareInterfacesInfo.serial_port_speed[i])) )
         {
            for( int k=0; k<hardwareInterfacesInfo.serial_port_count; k++ )
               hardwareInterfacesInfo.serial_port_speed[k] = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
            break;
         }
   }

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      if ( 1 != fscanf(fd, "%u", &(osd_params.osd_flags3[i])) )
         { osd_params.osd_flags3[i] = 0; }

   if ( bOk && (1 != fscanf(fd, "%u", &relay_params.uRelayFrequencyKhz)) )
      relay_params.uRelayFrequencyKhz = 0;
   relay_params.uRelayFrequencyKhz *= 1000;

   if ( 1 != fscanf(fd, "%d", &tmp1) )
      alarms_params.uAlarmMotorCurrentThreshold = (1<<7) & 30;
   else
      alarms_params.uAlarmMotorCurrentThreshold = tmp1;

   if ( bOk && (1 != fscanf(fd, "%d", &radioInterfacesParams.iDummyR9)) )
   {
      radioInterfacesParams.iDummyR9 = 0;
   }

   if ( bOk && (1 != fscanf(fd, "%u", &osd_params.uFlags)) )
   {
      log_softerror_and_alarm("Failed to read OSD uFlags from model config file.");
      osd_params.uFlags = 0;
      bOk = false;
   }

   //--------------------------------------------------
   // End reading file;
   //----------------------------------------

   // Validate settings;

   if ( processesPriorities.iNiceRC < -18 || processesPriorities.iNiceRC > 5 )
      processesPriorities.iNiceRC = DEFAULT_PRIORITY_PROCESS_RC;
   if ( processesPriorities.iNiceRouter < -18 || processesPriorities.iNiceRouter > 5 )
      processesPriorities.iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
   if ( processesPriorities.ioNiceRouter < -7 || processesPriorities.ioNiceRouter > 7 )
      processesPriorities.ioNiceRouter = DEFAULT_IO_PRIORITY_ROUTER;

   if ( telemetry_params.vehicle_mavlink_id <= 0 || telemetry_params.vehicle_mavlink_id > 255 )
      telemetry_params.vehicle_mavlink_id = DEFAULT_MAVLINK_SYS_ID_VEHICLE;
   if ( telemetry_params.controller_mavlink_id <= 0 || telemetry_params.controller_mavlink_id > 255 )
      telemetry_params.controller_mavlink_id = DEFAULT_MAVLINK_SYS_ID_CONTROLLER;
   if ( telemetry_params.flags == 0 )
      telemetry_params.flags = TELEMETRY_FLAGS_RXTX | TELEMETRY_FLAGS_REQUEST_DATA_STREAMS | TELEMETRY_FLAGS_SPECTATOR_ENABLE;
   if ( rxtx_sync_type < 0 || rxtx_sync_type >= RXTX_SYNC_TYPE_LAST )
      rxtx_sync_type = RXTX_SYNC_TYPE_BASIC;

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


bool Model::loadVersion9(FILE* fd)
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

   if ( 4 != fscanf(fd, "%*s %u %d %d %u", &sw_version, &uVehicleId, &uControllerId, &hwCapabilities.uBoardType) )
      { log_softerror_and_alarm("Load model8: Error on line 1"); return false; }

   if ( hardware_is_vehicle() )
      sw_version = (SYSTEM_SW_VERSION_MAJOR * 256 + SYSTEM_SW_VERSION_MINOR) | (SYSTEM_SW_BUILD_NUMBER<<16);

   if ( bOk && (1 != fscanf(fd, "%u", &uModelFlags )) )
      { log_softerror_and_alarm("Load model8: Error on line 2a"); uModelFlags = 0; return false; }


   if ( 1 != fscanf(fd, "%s", vehicle_name) )
      { log_softerror_and_alarm("Load model8: Error on line 2"); return false; }
   if ( vehicle_name[0] == '*' && vehicle_name[1] == 0 )
      vehicle_name[0] = 0;

   for( int i=0; i<(int)strlen(vehicle_name); i++ )
      if ( vehicle_name[i] == '_' )
         vehicle_name[i] = ' ';

   str_sanitize_modelname(vehicle_name);

   if ( 3 != fscanf(fd, "%d %u %d", &rxtx_sync_type, &camera_rc_channels, &processesPriorities.iNiceTelemetry ) )
      { log_softerror_and_alarm("Load model8: Error on line 3"); return false; }

   if ( 4 != fscanf(fd, "%d %d %u %d", &tmp1, &vt, &m_Stats.uTotalFlightTime, &iGPSCount ) )
      { log_softerror_and_alarm("Load model8: Error on line 3b"); return false; }

   is_spectator = (bool)tmp1;
   vehicle_type = vt;

   //----------------------------------------
   // CPU

   if ( 3 != fscanf(fd, "%*s %d %d %d", &processesPriorities.iNiceVideo, &processesPriorities.iNiceOthers, &processesPriorities.ioNiceVideo) )
      { log_softerror_and_alarm("Load model8: Error on line 4"); return false; }
   if ( 3 != fscanf(fd, "%d %d %d", &processesPriorities.iOverVoltage, &processesPriorities.iFreqARM, &processesPriorities.iFreqGPU) )
      { log_softerror_and_alarm("Load model8: Error on line 5"); }
   if ( 3 != fscanf(fd, "%d %d %d", &processesPriorities.iNiceRouter, &processesPriorities.ioNiceRouter, &processesPriorities.iNiceRC) )
      { log_softerror_and_alarm("Load model8: Error on extra line 2b"); }

   //----------------------------------------
   // Radio

   if ( 1 != fscanf(fd, "%*s %d", &radioInterfacesParams.interfaces_count) )
      { log_softerror_and_alarm("Load model8: Error on line 6"); return false; }

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      char szTmp[256];
      if ( 3 != fscanf(fd, "%d %d %u", &(radioInterfacesParams.interface_card_model[i]), &(radioInterfacesParams.interface_link_id[i]), &(radioInterfacesParams.interface_current_frequency_khz[i])) )
         { log_softerror_and_alarm("Load model8: Error on line 7a"); return false; }

      if ( 8 != fscanf(fd, "%u %d %u %d %d %d %s %s", &u4, &tmp2, &tmp32, &tmp5, &tmp6, &tmp7, radioInterfacesParams.interface_szMAC[i], szTmp) )
         { log_softerror_and_alarm("Load model8: Error on line 7b"); return false; }
      radioInterfacesParams.interface_capabilities_flags[i] = u4;
      radioInterfacesParams.interface_supported_bands[i] = (u8)tmp2;
      radioInterfacesParams.interface_radiotype_and_driver[i] = tmp32;
      radioInterfacesParams.interface_current_radio_flags[i] = tmp5;
      radioInterfacesParams.interface_raw_power[i] = tmp6;
      radioInterfacesParams.interface_szMAC[i][strlen(radioInterfacesParams.interface_szMAC[i])-1] = 0;
      szTmp[strlen(szTmp)-1] = 0;
      szTmp[2] = 0;
      strcpy(radioInterfacesParams.interface_szPort[i], szTmp);      
   }

   if ( 9 != fscanf(fd, "%d %d %d %d %d %d %d %d %d", &tmp1, &radioInterfacesParams.iAutoVehicleTxPower, &radioInterfacesParams.iAutoControllerTxPower, &radioInterfacesParams.iDummyR3, &radioInterfacesParams.iDummyR4, &radioInterfacesParams.iDummyR5, &radioInterfacesParams.iDummyR6,  &radioInterfacesParams.iDummyR7, &radioInterfacesParams.iDummyR8) )
      { log_softerror_and_alarm("Load model8: Error on line 8"); return false; }
   enableDHCP = (bool)tmp1;

   if ( 1 != fscanf(fd, "%d", &radioInterfacesParams.iDummyR9) )
   {
      radioInterfacesParams.iDummyR9 = 0;
   }

   if ( 1 != fscanf(fd, "%*s %d", &radioLinksParams.links_count) )
      { log_softerror_and_alarm("Load model8: Error on line r0"); return false; }

   for( int i=0; i<radioLinksParams.links_count; i++ )
   {
      if ( 5 != fscanf(fd, "%u %u %u %d %d", &(radioLinksParams.link_frequency_khz[i]), &(radioLinksParams.link_capabilities_flags[i]), &(radioLinksParams.link_radio_flags[i]), &(radioLinksParams.link_datarate_video_bps[i]), &(radioLinksParams.link_datarate_data_bps[i])) )
         { log_softerror_and_alarm("Load model8: Error on line r3"); return false; }

      if ( 4 != fscanf(fd, "%d %u %d %d", &tmp1, &(radioLinksParams.uDummy2[i]), &(radioLinksParams.uplink_datarate_video_bps[i]), &(radioLinksParams.uplink_datarate_data_bps[i])) )
         { log_softerror_and_alarm("Load model8: Error on line r4"); return false; }
      radioLinksParams.uSerialPacketSize[i] = tmp1;
      if ( (radioLinksParams.uSerialPacketSize[i] < DEFAULT_RADIO_SERIAL_AIR_MIN_PACKET_SIZE) ||
           (radioLinksParams.uSerialPacketSize[i] > DEFAULT_RADIO_SERIAL_AIR_MAX_PACKET_SIZE) )
         radioLinksParams.uSerialPacketSize[i] = DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE;
   }

   for( unsigned int j=0; j<12; j++ )
   {
      if ( 1 != fscanf(fd, "%d", &tmp1) )
         { log_softerror_and_alarm("Load model8: Error on line radio4 dummy data"); return false; }
   }

   //-------------------------------
   // Relay params

   if ( 5 != fscanf(fd, "%*s %d %u %d %u %u", &relay_params.isRelayEnabledOnRadioLinkId, &(relay_params.uRelayFrequencyKhz), &tmp2, &relay_params.uRelayedVehicleId, &relay_params.uRelayCapabilitiesFlags) )
      { log_softerror_and_alarm("Load model8: Error on line 10"); return false; }
   relay_params.uCurrentRelayMode = tmp2;

   //----------------------------------------
   // Telemetry

   if ( 5 != fscanf(fd, "%*s %d %d %d %d %d", &telemetry_params.fc_telemetry_type, &telemetry_params.controller_telemetry_type, &telemetry_params.update_rate, &tmp1, &tmp2) )
      { log_softerror_and_alarm("Load model8: Error on line 12"); return false; }

   if ( telemetry_params.update_rate > 200 )
      telemetry_params.update_rate = 200;
   telemetry_params.bControllerHasOutputTelemetry = (bool) tmp1;
   telemetry_params.bControllerHasInputTelemetry = (bool) tmp2;

   if ( 4 != fscanf(fd, "%d %u %d %u", &telemetry_params.iVideoBitrateHistoryGraphSampleInterval, &telemetry_params.dummy2, &telemetry_params.dummy3, &telemetry_params.dummy4) )
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
   {
      video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES;
      if ( hardware_board_is_openipc(hardware_getOnlyBoardType()) )
         video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES_OIPC;
   }
   video_params.videoAdjustmentStrength = tmp2;
   if ( video_params.lowestAllowedAdaptiveVideoBitrate < 250000 )
      video_params.lowestAllowedAdaptiveVideoBitrate = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;

   if ( 1 != fscanf(fd, "%u", &video_params.uMaxAutoKeyframeIntervalMs) )
      { log_softerror_and_alarm("Load model8: Error on line 20"); return false; }
   if ( video_params.uMaxAutoKeyframeIntervalMs < 50 || video_params.uMaxAutoKeyframeIntervalMs > DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL )
       video_params.uMaxAutoKeyframeIntervalMs = DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL;

   if ( 1 != fscanf(fd, "%u", &video_params.uVideoExtraFlags) )
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
      { video_link_profiles[i].uProfileFlags = 0; video_link_profiles[i].uProfileEncodingFlags = 0; }

   if ( tmp7 < 0 || tmp7 > MAX_VIDEO_LINK_PROFILES )
      tmp7 = MAX_VIDEO_LINK_PROFILES;

   for( int i=0; i<tmp7; i++ )
   {
      if ( ! bOk )
        { log_softerror_and_alarm("Load model8: Error on video link profiles 0"); bOk = false; }
 
      if ( bOk && (6 != fscanf(fd, "%u %u %u %d %d %u", &(video_link_profiles[i].uProfileFlags), &(video_link_profiles[i].uProfileEncodingFlags), &(video_link_profiles[i].bitrate_fixed_bps), &(video_link_profiles[i].radio_datarate_video_bps), &(video_link_profiles[i].radio_datarate_data_bps), &(video_link_profiles[i].radio_flags))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 1"); bOk = false; }

      if ( bOk && (2 != fscanf(fd, "%d %d", &(video_link_profiles[i].width), &(video_link_profiles[i].height))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 2"); bOk = false; }

      if ( bOk && (5 != fscanf(fd, "%d %d %d %d %d", &(video_link_profiles[i].block_packets), &(video_link_profiles[i].block_fecs), &(video_link_profiles[i].video_data_length), &(video_link_profiles[i].fps), &(video_link_profiles[i].keyframe_ms))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 3"); bOk = false; }
      
      if ( bOk && (5 != fscanf(fd, "%d %d %d %d %d", &(video_link_profiles[i].h264profile), &(video_link_profiles[i].h264level), &(video_link_profiles[i].h264refresh), &(video_link_profiles[i].h264quantization), &(video_link_profiles[i].iIPQuantizationDelta))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 4"); bOk = false; }
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
         if ( 1 != fscanf(fd, "%*s %d", &camera_params[k].profiles[i].uFlags) )
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
         else
            camera_params[k].profiles[i].wdr = (u8)tmp1;

         if ( 1 != fscanf(fd, "%d", &tmp1) )
            { log_softerror_and_alarm("Load model8: Error on line 25c, night mode, cam profile %d", i); camera_params[k].profiles[i].dayNightMode = 0; }
         else
            camera_params[k].profiles[i].dayNightMode = (u8)tmp1;
           
         if ( 1 != fscanf(fd, "%d", &tmp1) )
            { log_softerror_and_alarm("Load model8: Error on line 25f, hue, cam profile %d", i); camera_params[k].profiles[i].hue = 0; }
         else
            camera_params[k].profiles[i].hue = (u8)tmp1;

         for( unsigned int j=0; j<(sizeof(camera_params[k].profiles[i].dummyCamP)/sizeof(camera_params[k].profiles[i].dummyCamP[0])); j++ )
         {
            if ( 1 != fscanf(fd, "%d", &tmp1) )
               { log_softerror_and_alarm("Load model8: Error on line 27, cam profile %d", i); return false; }
            camera_params[k].profiles[i].dummyCamP[j] = tmp1;
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
   if ( 4 == fscanf(fd, "%d %d %d %u", &tmp2, &audio_params.volume, &audio_params.quality, &audio_params.uFlags) )
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

   if ( 4 == fscanf(fd, "%*s %d %d %d %d", &hardwareInterfacesInfo.radio_interface_count, &hardwareInterfacesInfo.i2c_bus_count, &hardwareInterfacesInfo.i2c_device_count, &hardwareInterfacesInfo.serial_port_count) )
   {
      if ( hardwareInterfacesInfo.i2c_bus_count < 0 || hardwareInterfacesInfo.i2c_bus_count > MAX_MODEL_I2C_BUSSES )
         { log_softerror_and_alarm("Load model8: Error on hw info1"); return false; }
      if ( hardwareInterfacesInfo.i2c_device_count < 0 || hardwareInterfacesInfo.i2c_device_count > MAX_MODEL_I2C_DEVICES )
         { log_softerror_and_alarm("Load model8: Error on hw info2"); return false; }
      if ( hardwareInterfacesInfo.serial_port_count < 0 || hardwareInterfacesInfo.serial_port_count > MAX_MODEL_SERIAL_PORTS )
         { log_softerror_and_alarm("Load model8: Error on hw info3"); return false; }

      for( int i=0; i<hardwareInterfacesInfo.i2c_bus_count; i++ )
         if ( 1 != fscanf(fd, "%d", &(hardwareInterfacesInfo.i2c_bus_numbers[i])) )
            { log_softerror_and_alarm("Load model8: Error on hw info4"); return false; }

      for( int i=0; i<hardwareInterfacesInfo.i2c_device_count; i++ )
         if ( 2 != fscanf(fd, "%d %d", &(hardwareInterfacesInfo.i2c_devices_bus[i]), &(hardwareInterfacesInfo.i2c_devices_address[i])) )
            { log_softerror_and_alarm("Load model8: Error on hw info5"); return false; }

      for( int i=0; i<hardwareInterfacesInfo.serial_port_count; i++ )
         if ( 3 != fscanf(fd, "%d %u %s", &(hardwareInterfacesInfo.serial_port_speed[i]), &(hardwareInterfacesInfo.serial_port_supported_and_usage[i]), &(hardwareInterfacesInfo.serial_port_names[i][0])) )
            { log_softerror_and_alarm("Load model8: Error on hw info6"); return false; }
   }
   else
   {
      hardwareInterfacesInfo.radio_interface_count = 0;
      hardwareInterfacesInfo.i2c_bus_count = 0;
      hardwareInterfacesInfo.i2c_device_count = 0;
      hardwareInterfacesInfo.serial_port_count = 0;
      bOk = false;
   }

   //----------------------------------------
   // OSD

   if ( 5 != fscanf(fd, "%*s %d %d %f %d %d", &osd_params.iCurrentOSDLayout, &tmp1, &osd_params.voltage_alarm, &tmp2, &tmp3) )
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
      if ( 5 != fscanf(fd, "%u %u %u %u %u", &(osd_params.osd_flags[i]), &(osd_params.osd_flags2[i]), &(osd_params.osd_flags3[i]), &(osd_params.instruments_flags[i]), &(osd_params.osd_preferences[i])) )
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

   if ( 1 != fscanf(fd, "%d", &rc_params.iRCTranslationType ) )
      { log_softerror_and_alarm("Load model9: Error on line 37e"); return false; }

   for( unsigned int i=0; i<(sizeof(rc_params.rcDummy)/sizeof(rc_params.rcDummy[0])); i++ )
      if ( 1 != fscanf(fd, "%u", &(rc_params.rcDummy[i])) )
         { log_softerror_and_alarm("Load model9: Error on line 37"); return false; }

   if ( rc_params.rc_failsafe_timeout_ms < 50 || rc_params.rc_failsafe_timeout_ms > 5000 )
      rc_params.rc_failsafe_timeout_ms = DEFAULT_RC_FAILSAFE_TIME;

   //----------------------------------------
   // Misc

   if ( 2 != fscanf(fd, "%*s %u %u", &uDummyM1, &uDeveloperFlags) )
      { log_softerror_and_alarm("Load model8: Error on line 39"); uDeveloperFlags = (((u32)DEFAULT_DELAY_WIFI_CHANGE)<<8); }
   
   if ( 1 != fscanf(fd, "%u", &enc_flags) )
   {
      enc_flags = MODEL_ENC_FLAGS_NONE;
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

   if ( 1 != fscanf(fd, "%d", &tmp1) )
      alarms_params.uAlarmMotorCurrentThreshold = (1<<7) & 30;
   else
      alarms_params.uAlarmMotorCurrentThreshold = tmp1;

   if ( bOk && (1 != fscanf(fd, "%u", &osd_params.uFlags)) )
   {
      log_softerror_and_alarm("Failed to read OSD uFlags from model config file.");
      osd_params.uFlags = 0;
      bOk = false;
   }

   //--------------------------------------------------
   // End reading file;
   //----------------------------------------

   // Validate settings;

   if ( processesPriorities.iNiceRC < -18 || processesPriorities.iNiceRC > 5 )
      processesPriorities.iNiceRC = DEFAULT_PRIORITY_PROCESS_RC;
   if ( processesPriorities.iNiceRouter < -18 || processesPriorities.iNiceRouter > 5 )
      processesPriorities.iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
   if ( processesPriorities.ioNiceRouter < -7 || processesPriorities.ioNiceRouter > 7 )
      processesPriorities.ioNiceRouter = DEFAULT_IO_PRIORITY_ROUTER;

   if ( telemetry_params.vehicle_mavlink_id <= 0 || telemetry_params.vehicle_mavlink_id > 255 )
      telemetry_params.vehicle_mavlink_id = DEFAULT_MAVLINK_SYS_ID_VEHICLE;
   if ( telemetry_params.controller_mavlink_id <= 0 || telemetry_params.controller_mavlink_id > 255 )
      telemetry_params.controller_mavlink_id = DEFAULT_MAVLINK_SYS_ID_CONTROLLER;
   if ( telemetry_params.flags == 0 )
      telemetry_params.flags = TELEMETRY_FLAGS_RXTX | TELEMETRY_FLAGS_REQUEST_DATA_STREAMS | TELEMETRY_FLAGS_SPECTATOR_ENABLE;
   if ( rxtx_sync_type < 0 || rxtx_sync_type >= RXTX_SYNC_TYPE_LAST )
      rxtx_sync_type = RXTX_SYNC_TYPE_BASIC;

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



bool Model::loadVersion10(FILE* fd)
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

   if ( 4 != fscanf(fd, "%*s %u %d %d %u", &sw_version, &uVehicleId, &uControllerId, &hwCapabilities.uBoardType) )
      { log_softerror_and_alarm("Load model8: Error on line 1"); return false; }

   if ( hardware_is_vehicle() )
      sw_version = (SYSTEM_SW_VERSION_MAJOR * 256 + SYSTEM_SW_VERSION_MINOR) | (SYSTEM_SW_BUILD_NUMBER<<16);

   if ( bOk && (1 != fscanf(fd, "%u", &uModelFlags )) )
      { log_softerror_and_alarm("Load model8: Error on line 2a"); uModelFlags = 0; return false; }


   if ( 1 != fscanf(fd, "%s", vehicle_name) )
      { log_softerror_and_alarm("Load model8: Error on line 2"); return false; }
   if ( vehicle_name[0] == '*' && vehicle_name[1] == 0 )
      vehicle_name[0] = 0;

   for( int i=0; i<(int)strlen(vehicle_name); i++ )
      if ( vehicle_name[i] == '_' )
         vehicle_name[i] = ' ';

   str_sanitize_modelname(vehicle_name);

   if ( 3 != fscanf(fd, "%d %u %d", &rxtx_sync_type, &camera_rc_channels, &processesPriorities.iNiceTelemetry ) )
      { log_softerror_and_alarm("Load model8: Error on line 3"); return false; }

   if ( 4 != fscanf(fd, "%d %d %u %d", &tmp1, &vt, &m_Stats.uTotalFlightTime, &iGPSCount ) )
      { log_softerror_and_alarm("Load model8: Error on line 3b"); return false; }

   is_spectator = (bool)tmp1;
   vehicle_type = vt;

   //----------------------------------------
   // CPU

   if ( 3 != fscanf(fd, "%*s %d %d %d", &processesPriorities.iNiceVideo, &processesPriorities.iNiceOthers, &processesPriorities.ioNiceVideo) )
      { log_softerror_and_alarm("Load model8: Error on line 4"); return false; }
   if ( 3 != fscanf(fd, "%d %d %d", &processesPriorities.iOverVoltage, &processesPriorities.iFreqARM, &processesPriorities.iFreqGPU) )
      { log_softerror_and_alarm("Load model8: Error on line 5"); }
   if ( 3 != fscanf(fd, "%d %d %d", &processesPriorities.iNiceRouter, &processesPriorities.ioNiceRouter, &processesPriorities.iNiceRC) )
      { log_softerror_and_alarm("Load model8: Error on extra line 2b"); }

   //----------------------------------------
   // Radio

   if ( 1 != fscanf(fd, "%*s %d", &radioInterfacesParams.interfaces_count) )
      { log_softerror_and_alarm("Load model8: Error on line 6"); return false; }

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      char szTmp[256];
      if ( 3 != fscanf(fd, "%d %d %u", &(radioInterfacesParams.interface_card_model[i]), &(radioInterfacesParams.interface_link_id[i]), &(radioInterfacesParams.interface_current_frequency_khz[i])) )
         { log_softerror_and_alarm("Load model8: Error on line 7a"); return false; }

      if ( 8 != fscanf(fd, "%u %d %u %d %d %d %s %s", &u4, &tmp2, &tmp32, &tmp5, &tmp6, &tmp7, radioInterfacesParams.interface_szMAC[i], szTmp) )
         { log_softerror_and_alarm("Load model8: Error on line 7b"); return false; }
      radioInterfacesParams.interface_capabilities_flags[i] = u4;
      radioInterfacesParams.interface_supported_bands[i] = (u8)tmp2;
      radioInterfacesParams.interface_radiotype_and_driver[i] = tmp32;
      radioInterfacesParams.interface_current_radio_flags[i] = tmp5;
      radioInterfacesParams.interface_raw_power[i] = tmp6;
      radioInterfacesParams.interface_szMAC[i][strlen(radioInterfacesParams.interface_szMAC[i])-1] = 0;
      szTmp[strlen(szTmp)-1] = 0;
      szTmp[2] = 0;
      strcpy(radioInterfacesParams.interface_szPort[i], szTmp);      
   }

   if ( 9 != fscanf(fd, "%d %d %d %d %d %d %d %d %d", &tmp1, &radioInterfacesParams.iAutoVehicleTxPower, &radioInterfacesParams.iAutoControllerTxPower, &radioInterfacesParams.iDummyR3, &radioInterfacesParams.iDummyR4, &radioInterfacesParams.iDummyR5, &radioInterfacesParams.iDummyR6,  &radioInterfacesParams.iDummyR7, &radioInterfacesParams.iDummyR8) )
      { log_softerror_and_alarm("Load model8: Error on line 8"); return false; }
   enableDHCP = (bool)tmp1;

   if ( 1 != fscanf(fd, "%d", &radioInterfacesParams.iDummyR9) )
   {
      radioInterfacesParams.iDummyR9 = 0;
   }

   if ( 1 != fscanf(fd, "%*s %d", &radioLinksParams.links_count) )
      { log_softerror_and_alarm("Load model8: Error on line r0"); return false; }

   for( int i=0; i<radioLinksParams.links_count; i++ )
   {
      if ( 5 != fscanf(fd, "%u %u %u %d %d", &(radioLinksParams.link_frequency_khz[i]), &(radioLinksParams.link_capabilities_flags[i]), &(radioLinksParams.link_radio_flags[i]), &(radioLinksParams.link_datarate_video_bps[i]), &(radioLinksParams.link_datarate_data_bps[i])) )
         { log_softerror_and_alarm("Load model8: Error on line r3"); return false; }

      if ( 4 != fscanf(fd, "%d %u %d %d", &tmp1, &(radioLinksParams.uDummy2[i]), &(radioLinksParams.uplink_datarate_video_bps[i]), &(radioLinksParams.uplink_datarate_data_bps[i])) )
         { log_softerror_and_alarm("Load model8: Error on line r4"); return false; }
      radioLinksParams.uSerialPacketSize[i] = tmp1;
      if ( (radioLinksParams.uSerialPacketSize[i] < DEFAULT_RADIO_SERIAL_AIR_MIN_PACKET_SIZE) ||
           (radioLinksParams.uSerialPacketSize[i] > DEFAULT_RADIO_SERIAL_AIR_MAX_PACKET_SIZE) )
         radioLinksParams.uSerialPacketSize[i] = DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE;

      if ( 2 != fscanf(fd, "%d %d", &tmp1, &tmp2) )
      {
         log_softerror_and_alarm("Load model10: Error on line r5");
         tmp1 = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO;
         tmp2 = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO;
      }
      radioLinksParams.uUplinkDataDataRateType[i] = tmp1;
      radioLinksParams.uDownlinkDataDataRateType[i] = tmp2;
   }

   if ( 2 != fscanf(fd, "%d %u", &radioLinksParams.iSiKPacketSize, &radioLinksParams.uGlobalRadioLinksFlags) )
   {
      log_softerror_and_alarm("Load model10: error on radio 9");
      return false;
   }
   for( unsigned int j=0; j<(sizeof(radioLinksParams.uDummyRadio)/sizeof(radioLinksParams.uDummyRadio[0])); j++ )
   {
      if ( 1 != fscanf(fd, "%d", &tmp1) )
         { log_softerror_and_alarm("Load model10: Error on line radio4 dummy data"); return false; }
      radioLinksParams.uDummyRadio[j] = tmp1;
   }

   //-------------------------------
   // Relay params

   if ( 5 != fscanf(fd, "%*s %d %u %d %u %u", &relay_params.isRelayEnabledOnRadioLinkId, &(relay_params.uRelayFrequencyKhz), &tmp2, &relay_params.uRelayedVehicleId, &relay_params.uRelayCapabilitiesFlags) )
      { log_softerror_and_alarm("Load model8: Error on line 10"); return false; }
   relay_params.uCurrentRelayMode = tmp2;

   //----------------------------------------
   // Telemetry

   if ( 5 != fscanf(fd, "%*s %d %d %d %d %d", &telemetry_params.fc_telemetry_type, &telemetry_params.controller_telemetry_type, &telemetry_params.update_rate, &tmp1, &tmp2) )
      { log_softerror_and_alarm("Load model8: Error on line 12"); return false; }

   if ( telemetry_params.update_rate > 200 )
      telemetry_params.update_rate = 200;
   telemetry_params.bControllerHasOutputTelemetry = (bool) tmp1;
   telemetry_params.bControllerHasInputTelemetry = (bool) tmp2;

   if ( 4 != fscanf(fd, "%d %u %d %u", &telemetry_params.iVideoBitrateHistoryGraphSampleInterval, &telemetry_params.dummy2, &telemetry_params.dummy3, &telemetry_params.dummy4) )
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
   {
      video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES;
      if ( hardware_board_is_openipc(hardware_getOnlyBoardType()) )
         video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES_OIPC;
   }
   video_params.videoAdjustmentStrength = tmp2;
   if ( video_params.lowestAllowedAdaptiveVideoBitrate < 250000 )
      video_params.lowestAllowedAdaptiveVideoBitrate = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;

   if ( 1 != fscanf(fd, "%u", &video_params.uMaxAutoKeyframeIntervalMs) )
      { log_softerror_and_alarm("Load model8: Error on line 20"); return false; }
   if ( video_params.uMaxAutoKeyframeIntervalMs < 50 || video_params.uMaxAutoKeyframeIntervalMs > DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL )
       video_params.uMaxAutoKeyframeIntervalMs = DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL;

   if ( 1 != fscanf(fd, "%u", &video_params.uVideoExtraFlags) )
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
      { video_link_profiles[i].uProfileFlags = 0; video_link_profiles[i].uProfileEncodingFlags = 0; }

   if ( tmp7 < 0 || tmp7 > MAX_VIDEO_LINK_PROFILES )
      tmp7 = MAX_VIDEO_LINK_PROFILES;

   for( int i=0; i<tmp7; i++ )
   {
      if ( ! bOk )
        { log_softerror_and_alarm("Load model8: Error on video link profiles 0"); bOk = false; }
 
      if ( bOk && (6 != fscanf(fd, "%u %u %u %d %d %u", &(video_link_profiles[i].uProfileFlags), &(video_link_profiles[i].uProfileEncodingFlags), &(video_link_profiles[i].bitrate_fixed_bps), &(video_link_profiles[i].radio_datarate_video_bps), &(video_link_profiles[i].radio_datarate_data_bps), &(video_link_profiles[i].radio_flags))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 1"); bOk = false; }

      if ( bOk && (2 != fscanf(fd, "%d %d", &(video_link_profiles[i].width), &(video_link_profiles[i].height))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 2"); bOk = false; }

      if ( bOk && (5 != fscanf(fd, "%d %d %d %d %d", &(video_link_profiles[i].block_packets), &(video_link_profiles[i].block_fecs), &(video_link_profiles[i].video_data_length), &(video_link_profiles[i].fps), &(video_link_profiles[i].keyframe_ms))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 3"); bOk = false; }
      
      if ( bOk && (5 != fscanf(fd, "%d %d %d %d %d", &(video_link_profiles[i].h264profile), &(video_link_profiles[i].h264level), &(video_link_profiles[i].h264refresh), &(video_link_profiles[i].h264quantization), &(video_link_profiles[i].iIPQuantizationDelta))) )
         { log_softerror_and_alarm("Load model8: Error on video link profiles 4"); bOk = false; }
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
         if ( 1 != fscanf(fd, "%*s %d", &camera_params[k].profiles[i].uFlags) )
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
         else
            camera_params[k].profiles[i].wdr = (u8)tmp1;

         if ( 1 != fscanf(fd, "%d", &tmp1) )
            { log_softerror_and_alarm("Load model10: Error on line 25c, night mode, cam profile %d", i); camera_params[k].profiles[i].dayNightMode = 0; }
         else
            camera_params[k].profiles[i].dayNightMode = (u8)tmp1;
           
         if ( 1 != fscanf(fd, "%d", &tmp1) )
            { log_softerror_and_alarm("Load model10: Error on line 25f, hue, cam profile %d", i); camera_params[k].profiles[i].hue = 0; }
         else
            camera_params[k].profiles[i].hue = (u8)tmp1;

         for( unsigned int j=0; j<(sizeof(camera_params[k].profiles[i].dummyCamP)/sizeof(camera_params[k].profiles[i].dummyCamP[0])); j++ )
         {
            if ( 1 != fscanf(fd, "%d", &tmp1) )
               { log_softerror_and_alarm("Load model10: Error on line 27, cam profile %d", i); return false; }
            camera_params[k].profiles[i].dummyCamP[j] = tmp1;
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
   if ( 4 == fscanf(fd, "%d %d %d %u", &tmp2, &audio_params.volume, &audio_params.quality, &audio_params.uFlags) )
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

   if ( 4 == fscanf(fd, "%*s %d %d %d %d", &hardwareInterfacesInfo.radio_interface_count, &hardwareInterfacesInfo.i2c_bus_count, &hardwareInterfacesInfo.i2c_device_count, &hardwareInterfacesInfo.serial_port_count) )
   {
      if ( hardwareInterfacesInfo.i2c_bus_count < 0 || hardwareInterfacesInfo.i2c_bus_count > MAX_MODEL_I2C_BUSSES )
         { log_softerror_and_alarm("Load model8: Error on hw info1"); return false; }
      if ( hardwareInterfacesInfo.i2c_device_count < 0 || hardwareInterfacesInfo.i2c_device_count > MAX_MODEL_I2C_DEVICES )
         { log_softerror_and_alarm("Load model8: Error on hw info2"); return false; }
      if ( hardwareInterfacesInfo.serial_port_count < 0 || hardwareInterfacesInfo.serial_port_count > MAX_MODEL_SERIAL_PORTS )
         { log_softerror_and_alarm("Load model8: Error on hw info3"); return false; }

      for( int i=0; i<hardwareInterfacesInfo.i2c_bus_count; i++ )
         if ( 1 != fscanf(fd, "%d", &(hardwareInterfacesInfo.i2c_bus_numbers[i])) )
            { log_softerror_and_alarm("Load model8: Error on hw info4"); return false; }

      for( int i=0; i<hardwareInterfacesInfo.i2c_device_count; i++ )
         if ( 2 != fscanf(fd, "%d %d", &(hardwareInterfacesInfo.i2c_devices_bus[i]), &(hardwareInterfacesInfo.i2c_devices_address[i])) )
            { log_softerror_and_alarm("Load model8: Error on hw info5"); return false; }

      for( int i=0; i<hardwareInterfacesInfo.serial_port_count; i++ )
         if ( 3 != fscanf(fd, "%d %u %s", &(hardwareInterfacesInfo.serial_port_speed[i]), &(hardwareInterfacesInfo.serial_port_supported_and_usage[i]), &(hardwareInterfacesInfo.serial_port_names[i][0])) )
            { log_softerror_and_alarm("Load model8: Error on hw info6"); return false; }
   }
   else
   {
      hardwareInterfacesInfo.radio_interface_count = 0;
      hardwareInterfacesInfo.i2c_bus_count = 0;
      hardwareInterfacesInfo.i2c_device_count = 0;
      hardwareInterfacesInfo.serial_port_count = 0;
      bOk = false;
   }

   //----------------------------------------
   // OSD

   if ( 5 != fscanf(fd, "%*s %d %d %f %d %d", &osd_params.iCurrentOSDLayout, &tmp1, &osd_params.voltage_alarm, &tmp2, &tmp3) )
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
      if ( 5 != fscanf(fd, "%u %u %u %u %u", &(osd_params.osd_flags[i]), &(osd_params.osd_flags2[i]), &(osd_params.osd_flags3[i]), &(osd_params.instruments_flags[i]), &(osd_params.osd_preferences[i])) )
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
      { log_softerror_and_alarm("Load model10: Error on line 35"); return false; }

   if ( 4 != fscanf(fd, "%d %ld %d %ld", &rc_params.inputSerialPort, &rc_params.inputSerialPortSpeed, &rc_params.outputSerialPort, &rc_params.outputSerialPortSpeed ) )
      { log_softerror_and_alarm("Load model10: Error on line 36"); return false; }
   
   for( int i=0; i<MAX_RC_CHANNELS; i++ )
   {
      if ( 7 != fscanf(fd, "%u %u %u %u %u %u %u", &u1, &u2, &u3, &u4, &u5, &u6, &u7) )
         { log_softerror_and_alarm("Load model10: Error on line 38"); return false; }
      rc_params.rcChAssignment[i] = u1;
      rc_params.rcChMid[i] = u2;
      rc_params.rcChMin[i] = u3;
      rc_params.rcChMax[i] = u4;
      rc_params.rcChFailSafe[i] = u5;
      rc_params.rcChExpo[i] = u6;
      rc_params.rcChFlags[i] = u7;
   }
   
   if ( 3 != fscanf(fd, "%d %u %u", &rc_params.rc_failsafe_timeout_ms, &rc_params.failsafeFlags, &rc_params.channelsCount ) )
      { log_softerror_and_alarm("Load model10: Error on line 37a"); return false; }
   if ( 1 != fscanf(fd, "%u", &rc_params.hid_id ) )
      { log_softerror_and_alarm("Load model10: Error on line 37b"); return false; }

   if ( 1 != fscanf(fd, "%u", &rc_params.flags ) )
      { log_softerror_and_alarm("Load model10: Error on line 37c"); return false; }

   if ( 1 != fscanf(fd, "%u", &rc_params.rcChAssignmentThrotleReverse ) )
      { log_softerror_and_alarm("Load model10: Error on line 37d"); return false; }

   if ( 1 != fscanf(fd, "%d", &rc_params.iRCTranslationType ) )
      { log_softerror_and_alarm("Load model10: Error on line 37e"); return false; }

   for( unsigned int i=0; i<(sizeof(rc_params.rcDummy)/sizeof(rc_params.rcDummy[0])); i++ )
      if ( 1 != fscanf(fd, "%u", &(rc_params.rcDummy[i])) )
         { log_softerror_and_alarm("Load model10: Error on line 37"); return false; }

   if ( rc_params.rc_failsafe_timeout_ms < 50 || rc_params.rc_failsafe_timeout_ms > 5000 )
      rc_params.rc_failsafe_timeout_ms = DEFAULT_RC_FAILSAFE_TIME;

   //----------------------------------------
   // Misc

   if ( 2 != fscanf(fd, "%*s %u %u", &uDummyM1, &uDeveloperFlags) )
      { log_softerror_and_alarm("Load model8: Error on line 39"); uDeveloperFlags = (((u32)DEFAULT_DELAY_WIFI_CHANGE)<<8); }
  
   if ( 1 != fscanf(fd, "%u", &enc_flags) )
   {
      enc_flags = MODEL_ENC_FLAGS_NONE;
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
         { log_softerror_and_alarm("Load model10: Error on line func_ch"); bOk = false; }
   }

   for( unsigned int i=0; i<(sizeof(functions_params.dummy)/sizeof(functions_params.dummy[0])); i++ )
      if ( bOk && (1 != fscanf(fd, "%u", &(functions_params.dummy[i]))) )
         { log_softerror_and_alarm("Load model10: Error on line funct_d"); bOk = false; }

   //----------------------------------------------------
   // Start of extra params, might be zero when loading older versions.

   if ( 1 != fscanf(fd, "%d", &tmp1) )
      alarms_params.uAlarmMotorCurrentThreshold = (1<<7) & 30;
   else
      alarms_params.uAlarmMotorCurrentThreshold = tmp1;

   if ( 1 != fscanf(fd, "%d", &m_iRadioInterfacesGraphRefreshInterval) )
      m_iRadioInterfacesGraphRefreshInterval = 3;

   if ( 3 != fscanf(fd, "%d %d %u", &hwCapabilities.iMaxTxVideoBlocksBuffer, &hwCapabilities.iMaxTxVideoBlockPackets, &hwCapabilities.uFlags) )
      resetHWCapabilities();

   if ( 1 != fscanf(fd, "%u", &hwCapabilities.uRubyBaseVersion) )
      hwCapabilities.uRubyBaseVersion = 0;
   for( unsigned int i=0; i<(sizeof(hwCapabilities.dummyhwc)/sizeof(hwCapabilities.dummyhwc[0])); i++ )
      if ( bOk && (1 != fscanf(fd, "%d", &(hwCapabilities.dummyhwc[i]))) )
         { bOk = false; }

   for( unsigned int i=0; i<(sizeof(hwCapabilities.dummyhwc2)/sizeof(hwCapabilities.dummyhwc2[0])); i++ )
      if ( bOk && (1 != fscanf(fd, "%u", &(hwCapabilities.dummyhwc2[i]))) )
         { bOk = false; }

   if ( ! bOk )
      log_softerror_and_alarm("Load model10: file is not ok. can't load th-prio");
   if ( bOk )
   if ( 3 != fscanf(fd, "%d %d %d", &processesPriorities.iThreadPriorityRadioRx, &processesPriorities.iThreadPriorityRadioTx, &processesPriorities.iThreadPriorityRouter) )
   {
      log_softerror_and_alarm("Load model10: Error on line th-prio");
      processesPriorities.iThreadPriorityRadioRx = DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_RX;
      processesPriorities.iThreadPriorityRadioTx = DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_TX;
      processesPriorities.iThreadPriorityRouter = DEFAULT_PRIORITY_VEHICLE_THREAD_ROUTER;
   }

   if ( bOk && (1 != fscanf(fd, "%u", &osd_params.uFlags)) )
   {
      log_softerror_and_alarm("Failed to read OSD uFlags from model config file.");
      osd_params.uFlags = 0;
      bOk = false;
   }

   if ( bOk && (1 != fscanf(fd, "%u", &processesPriorities.uProcessesFlags)) )
   {
      log_softerror_and_alarm("Failed to read processes flags.");
      processesPriorities.uProcessesFlags = PROCESSES_FLAGS_BALANCE_INT_CORES;
      bOk = false;
   }

   if ( bOk && (3 != fscanf(fd, "%d %d %d", &video_params.iRemovePPSVideoFrames, &video_params.iInsertPPSVideoFrames, &video_params.iInsertSPTVideoFramesTimings)) )
   {
      log_softerror_and_alarm("Failed to read video frames i flags.");
      video_params.iRemovePPSVideoFrames = 0;
      video_params.iInsertPPSVideoFrames = 1;
      video_params.iInsertSPTVideoFramesTimings = 0;
      bOk = false;
   }

   if ( bOk && (3 != fscanf(fd, "%d %d %u", &tmp1, &tmp2, &audio_params.uDummyA1)) )
   {
      log_softerror_and_alarm("Failed to read audio extra params.");
      audio_params.uECScheme = (((u32)DEFAULT_AUDIO_P_DATA) << 4) | ((u32)DEFAULT_AUDIO_P_EC);
      audio_params.uDummyA1 = 0;
      audio_params.uPacketLength = DEFAULT_AUDIO_PACKET_LENGTH;
   }
   audio_params.uECScheme = (u8)tmp1;
   audio_params.uPacketLength = (u16)tmp2;
   //--------------------------------------------------
   // End reading file;
   //----------------------------------------

   // Validate settings;

   if ( processesPriorities.iNiceRC < -18 || processesPriorities.iNiceRC > 5 )
      processesPriorities.iNiceRC = DEFAULT_PRIORITY_PROCESS_RC;
   if ( processesPriorities.iNiceRouter < -18 || processesPriorities.iNiceRouter > 5 )
      processesPriorities.iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
   if ( processesPriorities.ioNiceRouter < -7 || processesPriorities.ioNiceRouter > 7 )
      processesPriorities.ioNiceRouter = DEFAULT_IO_PRIORITY_ROUTER;

   if ( telemetry_params.vehicle_mavlink_id <= 0 || telemetry_params.vehicle_mavlink_id > 255 )
      telemetry_params.vehicle_mavlink_id = DEFAULT_MAVLINK_SYS_ID_VEHICLE;
   if ( telemetry_params.controller_mavlink_id <= 0 || telemetry_params.controller_mavlink_id > 255 )
      telemetry_params.controller_mavlink_id = DEFAULT_MAVLINK_SYS_ID_CONTROLLER;
   if ( telemetry_params.flags == 0 )
      telemetry_params.flags = TELEMETRY_FLAGS_RXTX | TELEMETRY_FLAGS_REQUEST_DATA_STREAMS | TELEMETRY_FLAGS_SPECTATOR_ENABLE;
   if ( rxtx_sync_type < 0 || rxtx_sync_type >= RXTX_SYNC_TYPE_LAST )
      rxtx_sync_type = RXTX_SYNC_TYPE_BASIC;

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

   //u32 timeStart = get_current_timestamp_ms();

   if ( isOnController )
   {
      sprintf(szBuff, FOLDER_VEHICLE_HISTORY, uVehicleId);
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
   saveVersion10(fd, isOnController);
   fflush(fd);
   fclose(fd);

   log_line("Saved vehicle successfully to file: %s; name: [%s], VID: %u, software: %d.%d (b%d), is on controller: %s, %s, on time: %02d:%02d",
         filename, vehicle_name, uVehicleId, (sw_version >> 8) & 0xFF, sw_version & 0xFF, sw_version>>16,
         isOnController?"yes":"no",
         is_spectator?"spectator mode": "control mode",
         m_Stats.uCurrentOnTime/60, m_Stats.uCurrentOnTime%60);
      
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
   saveVersion10(fd, isOnController);
   fflush(fd);
   fclose(fd);

   /*
   timeStart = get_current_timestamp_ms() - timeStart;
   char szLog[512];
   char szFreq1[64];
   char szFreq2[64];
   char szFreq3[64];
   strcpy(szFreq1, str_format_frequency(radioLinksParams.link_frequency_khz[0]));
   strcpy(szFreq2, str_format_frequency(radioLinksParams.link_frequency_khz[1]));
   strcpy(szFreq3, str_format_frequency(radioLinksParams.link_frequency_khz[2]));
   
   sprintf(szLog, "Saved model version 8 (%d ms) to file [%s] and [*.bak], UID: %u, save count: %d: name: [%s], vehicle id: %u, software: %d.%d (b%d), (is on controller side: %s, is in control mode: %s), %d radio links: 1: %s 2: %s 3: %s",
      timeStart, filename, uVehicleId,
      iSaveCount, vehicle_name, uVehicleId, (sw_version >> 8) & 0xFF, sw_version & 0xFF, sw_version >> 16, isOnController?"yes":"no", is_spectator?"no (spectator mode)":"yes",
      radioLinksParams.links_count, szFreq1, szFreq2, szFreq3 );
   log_line(szLog);
   */

   return true;
}

bool Model::saveVersion10(FILE* fd, bool isOnController)
{
   char szSetting[256];
   char szModel[8096];

   szSetting[0] = 0;
   szModel[0] = 0;

   if ( ! isOnController )
      sw_version = (SYSTEM_SW_VERSION_MAJOR * 256 + SYSTEM_SW_VERSION_MINOR) | (SYSTEM_SW_BUILD_NUMBER<<16);


   sprintf(szSetting, "ver: 10\n"); // version number
   strcat(szModel, szSetting);
   sprintf(szSetting, "%s\n",MODEL_FILE_STAMP_ID);
   strcat(szModel, szSetting);
   sprintf(szSetting, "savecounter: %d\n", iSaveCount);
   strcat(szModel, szSetting);
   sprintf(szSetting, "id: %u %u %u %u\n", sw_version, uVehicleId, uControllerId, hwCapabilities.uBoardType); 
   strcat(szModel, szSetting);

   sprintf(szSetting, "%u\n", uModelFlags);
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
 
   sprintf(szSetting, "%d %u %d\n", rxtx_sync_type, camera_rc_channels, processesPriorities.iNiceTelemetry );
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %u %d\n", is_spectator, vehicle_type, m_Stats.uTotalFlightTime, iGPSCount);
   strcat(szModel, szSetting);
   
   //----------------------------------------
   // CPU 

   sprintf(szSetting, "cpu: %d %d %d\n", processesPriorities.iNiceVideo, processesPriorities.iNiceOthers, processesPriorities.ioNiceVideo); 
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d\n", processesPriorities.iOverVoltage, processesPriorities.iFreqARM, processesPriorities.iFreqGPU);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d\n", processesPriorities.iNiceRouter, processesPriorities.ioNiceRouter, processesPriorities.iNiceRC);
   strcat(szModel, szSetting);

   //----------------------------------------
   // Radio

   sprintf(szSetting, "radio_interfaces: %d\n", radioInterfacesParams.interfaces_count); 
   strcat(szModel, szSetting);
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      sprintf(szSetting, "%d %d %u\n", radioInterfacesParams.interface_card_model[i], radioInterfacesParams.interface_link_id[i], radioInterfacesParams.interface_current_frequency_khz[i]);
      strcat(szModel, szSetting);
      sprintf(szSetting, "  %u %d %u %d %d %d %s- %s-\n", radioInterfacesParams.interface_capabilities_flags[i], radioInterfacesParams.interface_supported_bands[i], radioInterfacesParams.interface_radiotype_and_driver[i], radioInterfacesParams.interface_current_radio_flags[i], radioInterfacesParams.interface_raw_power[i], radioInterfacesParams.interface_dummy2[i], radioInterfacesParams.interface_szMAC[i], radioInterfacesParams.interface_szPort[i]);
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, "%d %d %d %d %d %d %d %d %d\n", enableDHCP, radioInterfacesParams.iAutoVehicleTxPower, radioInterfacesParams.iAutoControllerTxPower, radioInterfacesParams.iDummyR3, radioInterfacesParams.iDummyR4, radioInterfacesParams.iDummyR5, radioInterfacesParams.iDummyR6, radioInterfacesParams.iDummyR7, radioInterfacesParams.iDummyR8); 
   strcat(szModel, szSetting);

   sprintf(szSetting, "%d\n", radioInterfacesParams.iDummyR9);
   strcat(szModel, szSetting);

   sprintf(szSetting, "radio_links: %d\n", radioLinksParams.links_count); 
   strcat(szModel, szSetting);

   for( int i=0; i<radioLinksParams.links_count; i++ )
   {
      sprintf( szSetting, "%u %u %u %d %d   ", radioLinksParams.link_frequency_khz[i], radioLinksParams.link_capabilities_flags[i], radioLinksParams.link_radio_flags[i], radioLinksParams.link_datarate_video_bps[i], radioLinksParams.link_datarate_data_bps[i] );
      strcat(szModel, szSetting);
      sprintf( szSetting, "%d %u %d %d\n", (int)radioLinksParams.uSerialPacketSize[i], radioLinksParams.uDummy2[i], radioLinksParams.uplink_datarate_video_bps[i], radioLinksParams.uplink_datarate_data_bps[i] );
      strcat(szModel, szSetting);
      sprintf( szSetting, "%d %d\n", radioLinksParams.uUplinkDataDataRateType[i], radioLinksParams.uDownlinkDataDataRateType[i] );
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, " %d %u\n", radioLinksParams.iSiKPacketSize, radioLinksParams.uGlobalRadioLinksFlags);
   strcat(szModel, szSetting);

   for( unsigned int j=0; j<(sizeof(radioLinksParams.uDummyRadio)/sizeof(radioLinksParams.uDummyRadio[0])); j++ )
   {
      sprintf(szSetting, " %d", radioLinksParams.uDummyRadio[j]); 
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, "\n");      
   strcat(szModel, szSetting);

   //---------------------------------
   // Relay params

   sprintf(szSetting, "relay: %d %u %d %u %u\n", relay_params.isRelayEnabledOnRadioLinkId, relay_params.uRelayFrequencyKhz, relay_params.uCurrentRelayMode, relay_params.uRelayedVehicleId, relay_params.uRelayCapabilitiesFlags); 
   strcat(szModel, szSetting);

   //----------------------------------------
   // Telemetry

   sprintf(szSetting, "telem: %d %d %d %d %d\n", telemetry_params.fc_telemetry_type, telemetry_params.controller_telemetry_type, telemetry_params.update_rate, telemetry_params.bControllerHasOutputTelemetry, telemetry_params.bControllerHasInputTelemetry);
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %u %d %u\n", telemetry_params.iVideoBitrateHistoryGraphSampleInterval, telemetry_params.dummy2, telemetry_params.dummy3, telemetry_params.dummy4);
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
   
   sprintf(szSetting, "%u\n", video_params.uMaxAutoKeyframeIntervalMs);
   strcat(szModel, szSetting);
   
   sprintf(szSetting, "%u\n", video_params.uVideoExtraFlags);
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
      sprintf(szSetting, "%u %u %u %d %d %u   ", video_link_profiles[i].uProfileFlags, video_link_profiles[i].uProfileEncodingFlags, video_link_profiles[i].bitrate_fixed_bps, video_link_profiles[i].radio_datarate_video_bps, video_link_profiles[i].radio_datarate_data_bps, video_link_profiles[i].radio_flags);
      strcat(szModel, szSetting);
      sprintf(szSetting, "%d %d\n", video_link_profiles[i].width, video_link_profiles[i].height);
      strcat(szModel, szSetting);
      sprintf(szSetting, "   %d %d %d %d %d   ", video_link_profiles[i].block_packets, video_link_profiles[i].block_fecs, video_link_profiles[i].video_data_length, video_link_profiles[i].fps, video_link_profiles[i].keyframe_ms);
      strcat(szModel, szSetting);
      sprintf(szSetting, "%d %d %d %d %d\n", video_link_profiles[i].h264profile, video_link_profiles[i].h264level, video_link_profiles[i].h264refresh, video_link_profiles[i].h264quantization, video_link_profiles[i].iIPQuantizationDelta);
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
      sprintf(szSetting, "cam_profile_%d: %d %d\n", i, camera_params[k].profiles[i].uFlags, camera_params[k].profiles[i].flip_image); 
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

      sprintf(szSetting, "%d %d \n", (int)camera_params[k].profiles[i].dayNightMode, (int)camera_params[k].profiles[i].hue);
      strcat(szModel, szSetting);

      for( unsigned int j=0; j<(sizeof(camera_params[k].profiles[i].dummyCamP)/sizeof(camera_params[k].profiles[i].dummyCamP[0])); j++ )
      {
         sprintf(szSetting, " %d", camera_params[k].profiles[i].dummyCamP[j]); 
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
   sprintf(szSetting, "%d %d %d %u\n", (int)audio_params.enabled, audio_params.volume, audio_params.quality, audio_params.uFlags);
   strcat(szModel, szSetting);

   sprintf(szSetting, "alarms: %u\n", alarms);
   strcat(szModel, szSetting);

   //----------------------------------------
   // Hardware Info

   sprintf(szSetting, "hw_info: %d %d %d %d\n", hardwareInterfacesInfo.radio_interface_count, hardwareInterfacesInfo.i2c_bus_count, hardwareInterfacesInfo.i2c_device_count, hardwareInterfacesInfo.serial_port_count);
   strcat(szModel, szSetting);

   for( int i=0; i<hardwareInterfacesInfo.i2c_bus_count; i++ )
   {
      sprintf(szSetting, " %d", hardwareInterfacesInfo.i2c_bus_numbers[i]);
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, "\n");
   strcat(szModel, szSetting);

   for( int i=0; i<hardwareInterfacesInfo.i2c_device_count; i++ )
   {
      sprintf(szSetting, " %d %d\n", hardwareInterfacesInfo.i2c_devices_bus[i], hardwareInterfacesInfo.i2c_devices_address[i]);
      strcat(szModel, szSetting);
   }

   for( int i=0; i<hardwareInterfacesInfo.serial_port_count; i++ )
   {
      sprintf(szSetting, " %d %u %s\n", hardwareInterfacesInfo.serial_port_speed[i], hardwareInterfacesInfo.serial_port_supported_and_usage[i], hardwareInterfacesInfo.serial_port_names[i]);
      strcat(szModel, szSetting);
   }

   //----------------------------------------
   // OSD 

   sprintf(szSetting, "osd: %d %d %f %d %d\n", osd_params.iCurrentOSDLayout, osd_params.voltage_alarm_enabled, osd_params.voltage_alarm, osd_params.altitude_relative, osd_params.show_gps_position); 
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d %d %d\n", osd_params.battery_show_per_cell, osd_params.battery_cell_count, osd_params.battery_capacity_percent_alarm, osd_params.invert_home_arrow, osd_params.home_arrow_rotate); 
   strcat(szModel, szSetting);
   sprintf(szSetting, "%d %d %d %d %d %d %d\n", osd_params.show_overload_alarm, osd_params.show_stats_rx_detailed, osd_params.show_stats_decode, osd_params.show_stats_rc, osd_params.show_full_stats, osd_params.show_instruments, osd_params.ahi_warning_angle);
   strcat(szModel, szSetting);
   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   {
      sprintf(szSetting, "%u %u %u %u %u\n", osd_params.osd_flags[i], osd_params.osd_flags2[i], osd_params.osd_flags3[i], osd_params.instruments_flags[i], osd_params.osd_preferences[i]);
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
   sprintf(szSetting, "%u %d\n", rc_params.rcChAssignmentThrotleReverse, rc_params.iRCTranslationType );
   strcat(szModel, szSetting);
   for( unsigned int i=0; i<(sizeof(rc_params.rcDummy)/sizeof(rc_params.rcDummy[0])); i++ )
   {
      sprintf(szSetting, " %u",rc_params.rcDummy[i]);
      strcat(szModel, szSetting);
   }
   sprintf(szSetting, "\n");
   strcat(szModel, szSetting);

   //----------------------------------------
   // Misc

   sprintf(szSetting, "misc_dev: %u %u\n", uDummyM1, uDeveloperFlags);
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

   sprintf(szSetting, "%d\n", (int)alarms_params.uAlarmMotorCurrentThreshold);
   strcat(szModel, szSetting);
   
   sprintf(szSetting, "%d\n", (int)m_iRadioInterfacesGraphRefreshInterval);
   strcat(szModel, szSetting);
   
   sprintf(szSetting, "%d %d %u\n", hwCapabilities.iMaxTxVideoBlocksBuffer, hwCapabilities.iMaxTxVideoBlockPackets, hwCapabilities.uFlags);
   strcat(szModel, szSetting);

   sprintf(szSetting, "%u\n", hwCapabilities.uRubyBaseVersion);
   strcat(szModel, szSetting);

   for( unsigned int i=0; i<(sizeof(hwCapabilities.dummyhwc)/sizeof(hwCapabilities.dummyhwc[0])); i++ )
   {
      sprintf(szSetting, " %d", hwCapabilities.dummyhwc[i]);
      strcat(szModel, szSetting);
   }
   for( unsigned int i=0; i<(sizeof(hwCapabilities.dummyhwc2)/sizeof(hwCapabilities.dummyhwc2[0])); i++ )
   {
      sprintf(szSetting, " %u", hwCapabilities.dummyhwc2[i]);
      strcat(szModel, szSetting);
   }
   sprintf(szSetting,"\n");
   strcat(szModel, szSetting);

   sprintf(szSetting, "%d %d %d\n", processesPriorities.iThreadPriorityRadioRx, processesPriorities.iThreadPriorityRadioTx, processesPriorities.iThreadPriorityRouter);
   strcat(szModel, szSetting);

   sprintf(szSetting, "%u\n", osd_params.uFlags);
   strcat(szModel, szSetting);

   sprintf(szSetting, "%u\n", processesPriorities.uProcessesFlags);
   strcat(szModel, szSetting);

   sprintf(szSetting, "%d %d %d\n", video_params.iRemovePPSVideoFrames, video_params.iInsertPPSVideoFrames, video_params.iInsertSPTVideoFramesTimings);
   strcat(szModel, szSetting);

   sprintf(szSetting, "%d %d %u", (int)audio_params.uECScheme, (int)audio_params.uPacketLength, audio_params.uDummyA1);
   strcat(szModel, szSetting);

   // End writing values to file
   // ---------------------------------------------------

   // Done
   fprintf(fd, "%s", szModel);
   return true;
}

void Model::resetVideoParamsToDefaults()
{
   memset(&video_params, 0, sizeof(video_params));

   video_params.user_selected_video_link_profile = VIDEO_PROFILE_HIGH_QUALITY;
   video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES;
   if ( hardware_board_is_openipc(hardware_getOnlyBoardType()) )
      video_params.iH264Slices = DEFAULT_VIDEO_H264_SLICES_OIPC;
   video_params.iRemovePPSVideoFrames = 0;
   video_params.iInsertPPSVideoFrames = 1;
   video_params.iInsertSPTVideoFramesTimings = 0;
   video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;
   video_params.lowestAllowedAdaptiveVideoBitrate = DEFAULT_LOWEST_ALLOWED_ADAPTIVE_VIDEO_BITRATE;
   video_params.uMaxAutoKeyframeIntervalMs = DEFAULT_VIDEO_MAX_AUTO_KEYFRAME_INTERVAL;
   video_params.uVideoExtraFlags = 0;
   resetVideoLinkProfiles(-1);
}

void Model::resetVideoLinkProfiles(int iProfile)
{
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( (iProfile != -1) && (iProfile != i) )
         continue;
      video_link_profiles[i].uProfileFlags = VIDEO_PROFILE_FLAGS_NOISE_AUTO; // 3d noise: auto (2)
      video_link_profiles[i].uProfileEncodingFlags = VIDEO_PROFILE_ENCODING_FLAG_ENABLE_RETRANSMISSIONS | VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK | VIDEO_PROFILE_ENCODING_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
      video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_USE_MEDIUM_ADAPTIVE_VIDEO;
      video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
      video_link_profiles[i].uProfileEncodingFlags |= VIDEO_PROFILE_ENCODING_FLAG_EC_SCHEME_SPREAD_FACTOR_HIGHBIT;
      video_link_profiles[i].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
      video_link_profiles[i].radio_datarate_video_bps = 0; // Auto
      video_link_profiles[i].radio_datarate_data_bps = 0; // Auto
      video_link_profiles[i].radio_flags = 0;
      video_link_profiles[i].h264profile = 2; // high
      video_link_profiles[i].h264level = 2; // 4.2
      video_link_profiles[i].h264refresh = 2; // both
      video_link_profiles[i].h264quantization = DEFAULT_VIDEO_H264_QUANTIZATION;
      video_link_profiles[i].iIPQuantizationDelta = DEFAULT_VIDEO_H264_IPQUANTIZATION_DELTA_HP;

      video_link_profiles[i].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
      video_link_profiles[i].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HP;
      video_link_profiles[i].video_data_length = DEFAULT_VIDEO_DATA_LENGTH;

      video_link_profiles[i].fps = DEFAULT_VIDEO_FPS;
      if ( hardware_board_is_openipc(hardware_getOnlyBoardType()) )
         video_link_profiles[i].fps = DEFAULT_VIDEO_FPS_OIPC;
      if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
         video_link_profiles[i].fps = DEFAULT_VIDEO_FPS_OIPC_SIGMASTAR;
        
      video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME;
      if ( hardware_board_is_goke(hardware_getOnlyBoardType()) )
         video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME_OIPC_GOKE;
      if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
         video_link_profiles[i].keyframe_ms = DEFAULT_VIDEO_KEYFRAME_OIPC_SIGMASTAR;

      video_link_profiles[i].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
      if ( (hardware_getOnlyBoardType() == BOARD_TYPE_PIZERO) ||
           (hardware_getOnlyBoardType() == BOARD_TYPE_PIZEROW) ||
           hardware_board_is_goke(hardware_getOnlyBoardType()) )
         video_link_profiles[i].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_PI_ZERO;
      if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
         video_link_profiles[i].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_OPIC_SIGMASTAR;

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
   if ( (iProfile == -1) || (iProfile == VIDEO_PROFILE_BEST_PERF) )
   {
      video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileFlags = 0; // lowest noise
      video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK;
      video_link_profiles[VIDEO_PROFILE_BEST_PERF].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
      video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = DEFAULT_HP_VIDEO_BITRATE;
      if ( (hardware_getOnlyBoardType() == BOARD_TYPE_PIZERO) ||
           (hardware_getOnlyBoardType() == BOARD_TYPE_PIZEROW) ||
           hardware_board_is_goke(hardware_getOnlyBoardType()) )
         video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_PI_ZERO;

      video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
      video_link_profiles[VIDEO_PROFILE_BEST_PERF].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HP;
      video_link_profiles[VIDEO_PROFILE_BEST_PERF].radio_datarate_video_bps = DEFAULT_HP_VIDEO_RADIO_DATARATE;
   }

   // High Quality 
   if ( (iProfile == -1) || (iProfile == VIDEO_PROFILE_HIGH_QUALITY) )
   {
      video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK;
      video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
      video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HQ;
      video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HQ;
      video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].iIPQuantizationDelta = DEFAULT_VIDEO_H264_IPQUANTIZATION_DELTA_HQ;
   }

   // User
   if ( (iProfile == -1) || (iProfile == VIDEO_PROFILE_USER) )
   {
      video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK;
      video_link_profiles[VIDEO_PROFILE_USER].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_HQ<<8);
      video_link_profiles[VIDEO_PROFILE_USER].block_packets = DEFAULT_VIDEO_BLOCK_PACKETS_HP;
      video_link_profiles[VIDEO_PROFILE_USER].block_fecs = DEFAULT_VIDEO_BLOCK_FECS_HP;
      video_link_profiles[VIDEO_PROFILE_USER].iIPQuantizationDelta = DEFAULT_VIDEO_H264_IPQUANTIZATION_DELTA_HQ;
   }

   // MQ
   if ( (iProfile == -1) || (iProfile == VIDEO_PROFILE_MQ) )
   {
      video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK;
      video_link_profiles[VIDEO_PROFILE_MQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_MQ<<8);
      video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_video_bps = 0;
      video_link_profiles[VIDEO_PROFILE_MQ].radio_datarate_data_bps = 0;
      video_link_profiles[VIDEO_PROFILE_MQ].h264profile = 2; // high
      video_link_profiles[VIDEO_PROFILE_MQ].h264level = 2; // 4.2
      video_link_profiles[VIDEO_PROFILE_MQ].h264refresh = 2; // both
      video_link_profiles[VIDEO_PROFILE_MQ].h264quantization = DEFAULT_VIDEO_H264_QUANTIZATION; // auto
      video_link_profiles[VIDEO_PROFILE_MQ].block_packets = DEFAULT_MQ_VIDEO_BLOCK_PACKETS;
      video_link_profiles[VIDEO_PROFILE_MQ].block_fecs = DEFAULT_MQ_VIDEO_BLOCK_FECS;
      video_link_profiles[VIDEO_PROFILE_MQ].bitrate_fixed_bps = DEFAULT_MQ_VIDEO_BITRATE;
   }

   // LQ
   if ( (iProfile == -1) || (iProfile == VIDEO_PROFILE_LQ) )
   {
      video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_MAX_RETRANSMISSION_WINDOW_MASK;
      video_link_profiles[VIDEO_PROFILE_LQ].uProfileEncodingFlags |= (DEFAULT_VIDEO_RETRANS_MS5_LQ<<8);
      video_link_profiles[VIDEO_PROFILE_LQ].radio_datarate_video_bps = 0;
      video_link_profiles[VIDEO_PROFILE_LQ].radio_datarate_data_bps = 0;
      video_link_profiles[VIDEO_PROFILE_LQ].radio_flags = 0;
      video_link_profiles[VIDEO_PROFILE_LQ].h264profile = 2; // high
      video_link_profiles[VIDEO_PROFILE_LQ].h264level = 2; // 4.2
      video_link_profiles[VIDEO_PROFILE_LQ].h264refresh = 2; // both
      video_link_profiles[VIDEO_PROFILE_LQ].h264quantization = DEFAULT_VIDEO_H264_QUANTIZATION; // auto
      video_link_profiles[VIDEO_PROFILE_LQ].block_packets = DEFAULT_LQ_VIDEO_BLOCK_PACKETS;
      video_link_profiles[VIDEO_PROFILE_LQ].block_fecs = DEFAULT_LQ_VIDEO_BLOCK_FECS;
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
   }

   // Adaptive video & keyframe in openIPC goke cameras is not supported. (majestic bitrate and keyframe can't be changed)
   bool bDisableAdaptive = false;
   if ( hardware_board_is_goke(hwCapabilities.uBoardType) )
      bDisableAdaptive = true;

   if ( bDisableAdaptive )
   {
      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         if ( (iProfile != -1) && (iProfile != i) )
            continue;

         video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK;
         video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION;
         video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME;
      }
   }
   
   // Auto H264 quantization is not implemented in openIPC
   bool bDisableQuantization = false;
   if ( hardware_board_is_openipc(hwCapabilities.uBoardType) )
      bDisableQuantization = true;

   if ( bDisableQuantization )
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( (iProfile != -1) && (iProfile != i) )
         continue;

      video_link_profiles[i].uProfileEncodingFlags &= ~VIDEO_PROFILE_ENCODING_FLAG_ENABLE_VIDEO_ADAPTIVE_H264_QUANTIZATION;
   }

   if ( -1 == iProfile )
      setDefaultVideoBitrate();
}

void Model::resetVideoLinkProfilesToDataRates(int iUsableDataRateHQ, int iUsableDatarateHP)
{
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      video_link_profiles[i].radio_datarate_video_bps = 0;

   if ( 0 == iUsableDataRateHQ )
      iUsableDataRateHQ = DEFAULT_RADIO_DATARATE_VIDEO;
   if ( 0 == iUsableDatarateHP )
      iUsableDatarateHP = DEFAULT_RADIO_DATARATE_VIDEO;

   for( int i=0; i<radioLinksParams.links_count; i++ )
      radioLinksParams.link_datarate_video_bps[i] = iUsableDataRateHQ;

   if ( iUsableDataRateHQ == DEFAULT_RADIO_DATARATE_VIDEO )
   {
      for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      {
         if ( (radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) == RADIO_TYPE_ATHEROS )
         if ( radioInterfacesParams.interface_link_id[i] >= 0 )
         if ( radioInterfacesParams.interface_link_id[i] < radioLinksParams.links_count )
         {
            int iRadioLink = radioInterfacesParams.interface_link_id[i];
            radioLinksParams.link_datarate_video_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_VIDEO_ATHEROS;
         }
      }
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
   uVehicleId = rand();
   if ( BROADCAST_VEHICLE_ID == uVehicleId )
      uVehicleId = rand();
   FILE* fd = fopen("/sys/firmware/devicetree/base/serial-number", "r");
   if ( NULL != fd )
   {
      char szBuff[256];
      szBuff[0] = 0;
      if ( 1 == fscanf(fd, "%s", szBuff) && 4 < strlen(szBuff) )
      {
         //log_line("Serial ID of HW: %s", szBuff);
         uVehicleId += szBuff[strlen(szBuff)-1] + 256 * szBuff[strlen(szBuff)-2];
         
         //strcat(vehicle_name, "-");
         //strcat(vehicle_name, szBuff + (strlen(szBuff)-4));
      }
      fclose(fd);
   }
   log_line("Generated unique vehicle ID: %u", uVehicleId);
}

void Model::populateHWInfo()
{
   hardwareInterfacesInfo.radio_interface_count = hardware_get_radio_interfaces_count();
   hardwareInterfacesInfo.i2c_bus_count = 0;
   hardwareInterfacesInfo.i2c_device_count = 0;
   hardwareInterfacesInfo.serial_port_count = 0;

   hardware_enumerate_i2c_busses();

   hardwareInterfacesInfo.i2c_bus_count = hardware_get_i2c_busses_count();

   for( int i=0; i<hardware_get_i2c_busses_count(); i++ )
   {
      hw_i2c_bus_info_t* pBus = hardware_get_i2c_bus_info(i);
      if ( NULL != pBus && i < MAX_MODEL_I2C_BUSSES )
         hardwareInterfacesInfo.i2c_bus_numbers[i] = pBus->nBusNumber;
   }

   hardwareInterfacesInfo.i2c_device_count = 0;
   for( int i=1; i<128; i++ )
   {
      if ( ! hardware_has_i2c_device_id(i) )
         continue;
      hardwareInterfacesInfo.i2c_devices_address[hardwareInterfacesInfo.i2c_device_count] = i;
      hardwareInterfacesInfo.i2c_devices_bus[hardwareInterfacesInfo.i2c_device_count] = hardware_get_i2c_device_bus_number(i);
      hardwareInterfacesInfo.i2c_device_count++;
      if ( hardwareInterfacesInfo.i2c_device_count >= MAX_MODEL_I2C_DEVICES )
         break;
   }

   populateVehicleSerialPorts();
}

bool Model::populateVehicleSerialPorts()
{
   hardwareInterfacesInfo.serial_port_count = hardware_get_serial_ports_count();

   for( int i=0; i<hardwareInterfacesInfo.serial_port_count; i++ )
   {
      hw_serial_port_info_t* pInfo = hardware_get_serial_port_info(i);
      if ( NULL == pInfo )
      {
         hardwareInterfacesInfo.serial_port_count = i;
         break;
      }
      strcpy(hardwareInterfacesInfo.serial_port_names[i], pInfo->szName);
      hardwareInterfacesInfo.serial_port_speed[i] = pInfo->lPortSpeed;
      
      int iIndex = atoi(&(pInfo->szName[strlen(pInfo->szName)-1]));
      u32 uPortFlags = (((u32)iIndex) & 0x0F) << 8;
      if ( pInfo->iSupported )
         uPortFlags |= MODEL_SERIAL_PORT_BIT_SUPPORTED;
      
      if ( (NULL != strstr(pInfo->szName, "USB")) ||
           (NULL != strstr(pInfo->szPortDeviceName, "USB")) )
         uPortFlags |= MODEL_SERIAL_PORT_BIT_EXTRNAL_USB;
      hardwareInterfacesInfo.serial_port_supported_and_usage[i] = (((u32)(pInfo->iPortUsage)) & 0xFF) | uPortFlags;
   }

   log_line("Model: Populated %d serial ports:", hardwareInterfacesInfo.serial_port_count);
   for( int i=0; i<hardwareInterfacesInfo.serial_port_count; i++ )
   {
      log_line("Model: Serial Port: %s, type: %s, supported: %s, usage: %d (%s); speed: %d bps;",
          hardwareInterfacesInfo.serial_port_names[i],
         (hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_EXTRNAL_USB)?"USB":"HW",
         (hardwareInterfacesInfo.serial_port_supported_and_usage[i] & MODEL_SERIAL_PORT_BIT_SUPPORTED)?"yes":"no",
          hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF,
          str_get_serial_port_usage(hardwareInterfacesInfo.serial_port_supported_and_usage[i] & 0xFF),
          hardwareInterfacesInfo.serial_port_speed[i] );
   }

   return true;
}

void Model::resetRadioLinkDataRatesAndFlags(int iRadioLink)
{
   if ( (iRadioLink < 0) || (iRadioLink >= MAX_RADIO_INTERFACES) )
      return;
   radioLinksParams.link_radio_flags[iRadioLink] = DEFAULT_RADIO_FRAMES_FLAGS;
   radioLinksParams.link_capabilities_flags[iRadioLink] = RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   radioLinksParams.link_capabilities_flags[iRadioLink] |= RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   radioLinksParams.link_datarate_video_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_VIDEO;
   radioLinksParams.link_datarate_data_bps[iRadioLink] = DEFAULT_RADIO_DATARATE_DATA;

   radioLinksParams.uplink_datarate_video_bps[iRadioLink] = radioLinksParams.link_datarate_video_bps[iRadioLink];
   radioLinksParams.uplink_datarate_data_bps[iRadioLink] = radioLinksParams.link_datarate_data_bps[iRadioLink];

   radioLinksParams.uUplinkDataDataRateType[iRadioLink] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO;
   radioLinksParams.uDownlinkDataDataRateType[iRadioLink] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO;

   radioLinksParams.uSerialPacketSize[iRadioLink] = DEFAULT_RADIO_SERIAL_AIR_PACKET_SIZE;
   radioLinksParams.uDummy2[iRadioLink] = 0;

   updateRadioInterfacesRadioFlagsFromRadioLinksFlags();
}


void Model::addNewRadioLinkForRadioInterface(int iRadioInterfaceIndex, bool* pbDefault24Used, bool* pbDefault24_2Used, bool* pbDefault58Used, bool* pbDefault58_2Used)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= hardware_get_radio_interfaces_count()) )
      return;
   if ( radioLinksParams.links_count >= MAX_RADIO_INTERFACES )
      return;

   radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(iRadioInterfaceIndex);
   if ( NULL == pRadioHWInfo )
      return;

   resetRadioLinkDataRatesAndFlags(radioLinksParams.links_count);

   radioInterfacesParams.interface_link_id[iRadioInterfaceIndex] = radioLinksParams.links_count;

   if ( hardware_radio_is_wifi_radio(pRadioHWInfo) )
   {
      radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
      radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] &= ~RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
      radioLinksParams.link_capabilities_flags[radioLinksParams.links_count] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      radioLinksParams.link_capabilities_flags[radioLinksParams.links_count] &= ~RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
      
      radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = DEFAULT_FREQUENCY;
      if ( radioInterfacesParams.interface_supported_bands[iRadioInterfaceIndex] & RADIO_HW_SUPPORTED_BAND_58 )
      {
         if ( (NULL == pbDefault58Used) || (! (*pbDefault58Used)) )
         {
            radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = DEFAULT_FREQUENCY58;
            if ( NULL != pbDefault58Used)
               *pbDefault58Used = true;
         }
         else if ( (NULL == pbDefault58_2Used) || (! (*pbDefault58_2Used)) )
         {
            radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = DEFAULT_FREQUENCY58_2;
            if ( NULL != pbDefault58_2Used )
               *pbDefault58_2Used = true;
         }
         else
            radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = DEFAULT_FREQUENCY58_3;
      }
      else
      {
         if ( (NULL == pbDefault24Used) || (! (*pbDefault24Used)) )
         {
            radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = DEFAULT_FREQUENCY;
            if ( NULL != pbDefault24Used )
               *pbDefault24Used = true;
         }
         else if ( (NULL == pbDefault24_2Used) || (! (*pbDefault24_2Used)) )
         {
            radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = DEFAULT_FREQUENCY_2;
            if ( NULL != pbDefault24_2Used )
               *pbDefault24_2Used = true;
         }
         else
            radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = DEFAULT_FREQUENCY_3;
      }
   }
   else if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
   {
      radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] &= ~RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
      radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
      radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_SIK;

      if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_433 )
         radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = DEFAULT_FREQUENCY_433;
      if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_868 )
         radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = DEFAULT_FREQUENCY_868;
      if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_915 )
         radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = DEFAULT_FREQUENCY_915;

      radioLinksParams.link_capabilities_flags[radioLinksParams.links_count] &= ~RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
      radioLinksParams.link_capabilities_flags[radioLinksParams.links_count] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      radioLinksParams.link_capabilities_flags[radioLinksParams.links_count] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
      radioLinksParams.link_capabilities_flags[radioLinksParams.links_count] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_SIK;

      radioLinksParams.link_datarate_video_bps[radioLinksParams.links_count] = DEFAULT_RADIO_DATARATE_SIK_AIR;
      radioLinksParams.link_datarate_data_bps[radioLinksParams.links_count] = DEFAULT_RADIO_DATARATE_SIK_AIR;
      radioLinksParams.uplink_datarate_video_bps[radioLinksParams.links_count] = DEFAULT_RADIO_DATARATE_SIK_AIR;
      radioLinksParams.uplink_datarate_data_bps[radioLinksParams.links_count] = DEFAULT_RADIO_DATARATE_SIK_AIR;
   }
   else if ( pRadioHWInfo->isSerialRadio )
   {
      radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] &= ~RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
      radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
      if ( pRadioHWInfo->iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS )
         radioInterfacesParams.interface_capabilities_flags[iRadioInterfaceIndex] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS;

      radioLinksParams.link_capabilities_flags[radioLinksParams.links_count] &= ~RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
      radioLinksParams.link_capabilities_flags[radioLinksParams.links_count] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      radioLinksParams.link_capabilities_flags[radioLinksParams.links_count] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
      if ( pRadioHWInfo->iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS )
         radioLinksParams.link_capabilities_flags[radioLinksParams.links_count] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS;

      radioLinksParams.link_datarate_video_bps[radioLinksParams.links_count] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;
      radioLinksParams.link_datarate_data_bps[radioLinksParams.links_count] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;
      radioLinksParams.uplink_datarate_video_bps[radioLinksParams.links_count] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;
      radioLinksParams.uplink_datarate_data_bps[radioLinksParams.links_count] = DEFAULT_RADIO_DATARATE_SERIAL_AIR;       
   
      radioLinksParams.link_frequency_khz[radioLinksParams.links_count] = pRadioHWInfo->uCurrentFrequencyKhz;
   }
   else
   {
      log_softerror_and_alarm("Adding a new radio link, can't complete: Unknow radio type on model's radio interface %d (%s).", iRadioInterfaceIndex+1, pRadioHWInfo->szName);
   }
   radioInterfacesParams.interface_current_frequency_khz[iRadioInterfaceIndex] = radioLinksParams.link_frequency_khz[radioLinksParams.links_count];

   radioLinksParams.uplink_datarate_video_bps[radioLinksParams.links_count] = radioLinksParams.link_datarate_video_bps[radioLinksParams.links_count];
   radioLinksParams.uplink_datarate_data_bps[radioLinksParams.links_count] = radioLinksParams.link_datarate_data_bps[radioLinksParams.links_count];
   
   updateRadioInterfacesRadioFlagsFromRadioLinksFlags();

   log_line("Model: Added a new radio link (link %d) on %s for radio interface %d (%s)",
      radioLinksParams.links_count+1, str_format_frequency(radioLinksParams.link_frequency_khz[radioLinksParams.links_count]), iRadioInterfaceIndex+1, pRadioHWInfo->szName);
}

// Model's radio interfaces are populated here and on hw_config_check.cpp

void Model::populateRadioInterfacesInfoFromHardware()
{
   radioInterfacesParams.interfaces_count = hardware_get_radio_interfaces_count();

   log_line("Model: Populate default radio interfaces info from radio hardware (%d hardware radio interfaces).", radioInterfacesParams.interfaces_count );

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radioInterfacesParams.interface_card_model[i] = 0;
      radioInterfacesParams.interface_link_id[i] = i;
      if ( radioInterfacesParams.interface_link_id[i] >= radioLinksParams.links_count )
         radioInterfacesParams.interface_link_id[i] = radioLinksParams.links_count - 1;
      radioInterfacesParams.interface_current_frequency_khz[i] = 0;
      radioInterfacesParams.interface_current_radio_flags[i] = 0;
      radioInterfacesParams.interface_dummy2[i] = 0;

      radioInterfacesParams.interface_radiotype_and_driver[i] = 0;
      radioInterfacesParams.interface_supported_bands[i] = 0;
      radioInterfacesParams.interface_szMAC[i][0] = 0;
      radioInterfacesParams.interface_szPort[i][0] = 0;

      radioInterfacesParams.interface_capabilities_flags[i] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
      radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      
      if ( i >= radioInterfacesParams.interfaces_count )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      char szBands[128];
      szBands[0] = 0;
      str_get_supported_bands_string(pRadioHWInfo->supportedBands, szBands);
      log_line("Add hardware radio interface %d: name: (%s), driver: (%s), model: (%s), high capacity: %s, supported bands: %s",
         i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_get_radio_card_model_string(pRadioHWInfo->iCardModel),
         pRadioHWInfo->isHighCapacityInterface?"yes":"no", szBands);
      if ( pRadioHWInfo->isHighCapacityInterface )
         radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
      else
         radioInterfacesParams.interface_capabilities_flags[i] &= ~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
       
      if ( ! pRadioHWInfo->isSupported )
         radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_DISABLED;
        
      radioInterfacesParams.interface_supported_bands[i] = pRadioHWInfo->supportedBands;
      radioInterfacesParams.interface_radiotype_and_driver[i] = ((pRadioHWInfo->iRadioType) & 0xFF) | ((pRadioHWInfo->iRadioDriver << 8) & 0xFF00);
      if ( pRadioHWInfo->isSupported )
         radioInterfacesParams.interface_radiotype_and_driver[i] |= 0xFF0000;
      strcpy(radioInterfacesParams.interface_szMAC[i], pRadioHWInfo->szMAC );
      strcpy(radioInterfacesParams.interface_szPort[i], pRadioHWInfo->szUSBPort);

      radioInterfacesParams.interface_card_model[i] = pRadioHWInfo->iCardModel;
      
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         radioInterfacesParams.interface_current_frequency_khz[i] = pRadioHWInfo->uCurrentFrequencyKhz;
         radioInterfacesParams.interface_capabilities_flags[i] &= ~RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
         radioInterfacesParams.interface_capabilities_flags[i] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
         radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
         radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_SIK;
      }
      else if ( pRadioHWInfo->isSerialRadio )
      {
         radioInterfacesParams.interface_capabilities_flags[i] &= ~RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
         radioInterfacesParams.interface_capabilities_flags[i] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
         radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK;
         if ( pRadioHWInfo->iCardModel == CARD_MODEL_SERIAL_RADIO_ELRS )
            radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS;
         
         radioInterfacesParams.interface_current_frequency_khz[i] = pRadioHWInfo->uCurrentFrequencyKhz;
      }
   }

   validate_settings();
   populateDefaultRadioLinksInfoFromRadioInterfaces();
   validate_settings();
}

// Radio links are added from here, or on vehicle start from hw_config_check.cpp

void Model::populateDefaultRadioLinksInfoFromRadioInterfaces()
{
   log_line("Model: Populate default radio links info from radio hardware.");
   
   // Reset all first

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      resetRadioLinkDataRatesAndFlags(i);
      radioLinksParams.link_frequency_khz[i] = 0;
   }

   bool bDefault24Used = false;
   bool bDefault58Used = false;
   bool bDefault24_2Used = false;
   bool bDefault58_2Used = false;

   radioLinksParams.links_count = 0;

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      if ( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! pRadioHWInfo->isSupported )
         continue;
      addNewRadioLinkForRadioInterface(i, &bDefault24Used, &bDefault24_2Used, &bDefault58Used, &bDefault58_2Used);
      radioLinksParams.links_count++;
   }

   // Remove links that have the same frequency

   for( int i=0; i<radioLinksParams.links_count-1; i++ )
   for( int j=i+1; j<radioLinksParams.links_count; j++ )
      if ( radioLinksParams.link_frequency_khz[j] == radioLinksParams.link_frequency_khz[i] )
      {
         radioLinksParams.link_frequency_khz[j] = 0;
         for( int k=0; k<radioInterfacesParams.interfaces_count; k++ )
            if ( radioInterfacesParams.interface_link_id[k] == j )
               radioInterfacesParams.interface_link_id[k] = -1;
         for( int k=j; k<radioLinksParams.links_count-1; k++ )
         {
            copy_radio_link_params(k+1, k);
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
   updateRadioInterfacesRadioFlagsFromRadioLinksFlags();

   if ( true )
      logVehicleRadioInfo();

}

void Model::updateRadioInterfacesRadioFlagsFromRadioLinksFlags()
{
   for( int iLink=0; iLink<radioLinksParams.links_count; iLink++ )
   {
      for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      {
         radioInterfacesParams.interface_dummy2[i] = 0;
         if ( radioInterfacesParams.interface_link_id[i] == iLink )
         {
            radioInterfacesParams.interface_current_radio_flags[i] = radioLinksParams.link_radio_flags[iLink];
         }
      }
   }
}

bool Model::check_update_radio_links()
{
   bool bAnyLinkRemoved = false;
   bool bAnyLinkAdded = false;

   bool bDefault24Used = false;
   bool bDefault58Used = false;
   bool bDefault24_2Used = false;
   bool bDefault58_2Used = false;

   // Remove unused radio links

   log_line("Model: Check for unused radio links...");

   for( int iRadioLink=0; iRadioLink<radioLinksParams.links_count; iRadioLink++ )
   {
      bool bLinkOk = true;
      bool bAssignedCard = false;
      for( int k=0; k<radioInterfacesParams.interfaces_count; k++ )
      {
         if ( radioInterfacesParams.interface_link_id[k] != iRadioLink )
            continue;
         bAssignedCard = true;
         if ( ! (radioInterfacesParams.interface_capabilities_flags[k] & RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS) )
         if ( ! isFrequencyInBands(radioLinksParams.link_frequency_khz[iRadioLink], radioInterfacesParams.interface_supported_bands[k]) )
         {
            bLinkOk = false;
            break;
         }
      }
      if ( bAssignedCard && bLinkOk )
         continue;

      // Remove this radio link

      log_line("Model: Remove model's radio link %d as it has no valid model radio interface assigned to it.", iRadioLink+1);

      // Unassign any cards that still use this radio link

      for( int card=0; card<radioInterfacesParams.interfaces_count; card++ )
         if ( radioInterfacesParams.interface_link_id[card] == iRadioLink )
            radioInterfacesParams.interface_link_id[card] = -1;

      for( int k=iRadioLink; k<radioLinksParams.links_count-1; k++ )
      {
         copy_radio_link_params(k+1, k);
         
         for( int card=0; card<radioInterfacesParams.interfaces_count; card++ )
            if ( radioInterfacesParams.interface_link_id[card] == k+1 )
               radioInterfacesParams.interface_link_id[card] = k;
      }
      radioLinksParams.links_count--;
      bAnyLinkRemoved = true;
   }

   if ( bAnyLinkRemoved )
   {
      log_line("Model: Removed invalid radio links. This is the current vehicle configuration after adding all the radio interfaces and removing any invalid radio links:");
      logVehicleRadioInfo();
   }
   else
      log_line("Model: No invalid radio links where removed from model configuration.");

   // Check for unused radio interfaces

   log_line("Model: Check for active unused radio interfaces...");

   for( int iInterface=0; iInterface<radioInterfacesParams.interfaces_count; iInterface++ )
   {
      int iRadioLink = radioInterfacesParams.interface_link_id[iInterface];
      if ( (iRadioLink < 0) || (iRadioLink >= radioLinksParams.links_count) )
         continue;

      u32 uFreq = radioLinksParams.link_frequency_khz[iRadioLink];
      if ( uFreq == DEFAULT_FREQUENCY )
         bDefault24Used = true;
      if ( uFreq == DEFAULT_FREQUENCY_2 )
         bDefault24_2Used = true;
      if ( uFreq == DEFAULT_FREQUENCY58 )
         bDefault58Used = true;
      if ( uFreq == DEFAULT_FREQUENCY58_2 )
         bDefault58_2Used = true;
   }

   for( int iInterface=0; iInterface<radioInterfacesParams.interfaces_count; iInterface++ )
   {
      if ( radioInterfacesParams.interface_link_id[iInterface] >= 0 )
         continue;
      if ( radioInterfacesParams.interface_capabilities_flags[iInterface] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         log_line("Model: Radio interfaces %d is disabled and not used on any radio links.", iInterface+1);
         continue;
      }

      if ( ! (radioInterfacesParams.interface_capabilities_flags[iInterface] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         log_line("Model: Radio interface %d is not capable of data transmission so it's not used on any radio links.", iInterface+1);
         continue;
      }
      
      log_line("Model: Radio interface %d is not used on any radio links. Creating a radio link for it...", iInterface+1);

      // Add a new radio link

      addNewRadioLinkForRadioInterface(iInterface, &bDefault24Used, &bDefault24_2Used, &bDefault58Used, &bDefault58_2Used);

      if ( 0 == radioLinksParams.links_count )
      {
         log_line("Model: Making sure radio link 1 (first one in the model) has all required capabilities flags. Enable them on radio link 1.");
         radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
         if ( (radioInterfacesParams.interface_radiotype_and_driver[iInterface] & 0xFF) != RADIO_TYPE_SIK )
            radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
         radioLinksParams.link_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
         radioLinksParams.link_capabilities_flags[0] &= (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
      }

      radioLinksParams.links_count++;
      bAnyLinkAdded = true;
   }

   return bAnyLinkRemoved | bAnyLinkAdded;
}

void Model::logVehicleRadioInfo()
{
   char szBuff[256];
   log_line("------------------------------------------------------");
   log_line("Vehicle (%s) current radio links (%d links, %d radio interfaces):", getLongName(), radioLinksParams.links_count, radioInterfacesParams.interfaces_count);
   log_line("");
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
         char szPrefix[32];
         char szRelayFreq[32];
         szPrefix[0] = 0;
         szRelayFreq[0] = 0;
         if ( radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         {
            strcpy(szPrefix, "Relay ");
            sprintf(szRelayFreq, " (Relay on %s)", str_format_frequency(relay_params.uRelayFrequencyKhz));
         }
         log_line("* %sRadio Link %d: %s%s, Radio frame flags: %s, used on radio interfaces: [%s]",
                  szPrefix,i+1, str_format_frequency(radioLinksParams.link_frequency_khz[i]), szRelayFreq, szBuff, szBuff2);
         str_get_radio_capabilities_description(radioLinksParams.link_capabilities_flags[i], szBuff);
         log_line("  %sRadio Link %d capabilities flags: %s", szPrefix, i+1, szBuff);
         log_line("  %sRadio Link %d datarates (video/data downlink/data uplink): %d/[%d-%d]/[%d-%d]", szPrefix, i+1, radioLinksParams.link_datarate_video_bps[i], radioLinksParams.uDownlinkDataDataRateType[i], radioLinksParams.link_datarate_data_bps[i], radioLinksParams.uUplinkDataDataRateType[i], radioLinksParams.uplink_datarate_data_bps[i]);
   }

   log_line("------------------------------------------------------");

   log_line("Vehicle current radio interfaces (%d radio interfaces):", radioInterfacesParams.interfaces_count);
   log_line("");
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      szBuff[0] = 0;
      str_get_radio_capabilities_description(radioInterfacesParams.interface_capabilities_flags[i], szBuff);
      char szBuff2[128];
      str_get_radio_frame_flags_description(radioInterfacesParams.interface_current_radio_flags[i], szBuff2);
      
      char szBands[256];
      szBands[0] = 0;
      str_get_supported_bands_string(radioInterfacesParams.interface_supported_bands[i], szBands);

      char szPrefix[32];
      szPrefix[0] = 0;

      if ( hardware_is_vehicle() )
      {
         if ( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
            strcpy(szPrefix, "Relay ");
         radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
         if ( (NULL != pRadioInfo) && (0 == strcmp(pRadioInfo->szMAC, radioInterfacesParams.interface_szMAC[i]) ) )
            log_line("* %sRadio Interface %d: %s, %s, %s on port %s, drv: %s, supported bands: %s, current frequency: %s, assigned to radio link %d, current capabilities: %s, current radio flags: %s, raw_tx_power: %d",
                szPrefix, i+1, pRadioInfo->szName, radioInterfacesParams.interface_szMAC[i], str_get_radio_card_model_string(radioInterfacesParams.interface_card_model[i]), radioInterfacesParams.interface_szPort[i], str_get_radio_driver_description(radioInterfacesParams.interface_radiotype_and_driver[i]), szBands, str_format_frequency(pRadioInfo->uCurrentFrequencyKhz), radioInterfacesParams.interface_link_id[i]+1, szBuff, szBuff2,
                radioInterfacesParams.interface_raw_power[i]);
         else
            log_line("* %sRadio Interface %d: %s on port %s, drv: %s, supported bands: %s, current frequency: %s, assigned to radio link %d, current capabilities: %s, current radio flags: %s, raw_tx_power: %d",
               szPrefix, i+1, str_get_radio_card_model_string(radioInterfacesParams.interface_card_model[i]), radioInterfacesParams.interface_szPort[i], str_get_radio_driver_description(radioInterfacesParams.interface_radiotype_and_driver[i]), szBands, str_format_frequency(radioInterfacesParams.interface_current_frequency_khz[i]), radioInterfacesParams.interface_link_id[i]+1, szBuff, szBuff2,
               radioInterfacesParams.interface_raw_power[i]);
      }
      else
          log_line("* Radio Interface %d: %s, %s on port %s, %s, supported bands: %s, current frequency: %s, assigned to radio link %d, current capabilities: %s, current radio flags: %s, raw_tx_power: %d",
               i+1, radioInterfacesParams.interface_szMAC[i], str_get_radio_card_model_string(radioInterfacesParams.interface_card_model[i]), radioInterfacesParams.interface_szPort[i], str_get_radio_driver_description(radioInterfacesParams.interface_radiotype_and_driver[i]), szBands, str_format_frequency(radioInterfacesParams.interface_current_frequency_khz[i]), radioInterfacesParams.interface_link_id[i]+1, szBuff, szBuff2,
               radioInterfacesParams.interface_raw_power[i]);
   }

   log_line("------------------------------------------------------");
}

// Returns the number of differences
int Model::logVehicleRadioLinkDifferences(type_radio_links_parameters* pData1, type_radio_links_parameters* pData2)
{
   if ( (NULL == pData1) || (NULL == pData2) )
   {
      log_softerror_and_alarm("Invalid params in detecting radio links params differences.");
      return 0;
   }

   int iDifferences = 0;
   char szTmp[64];
   char szBuff1[256];
   char szBuff2[256];

   if ( pData1->links_count != pData2->links_count )
   {
      log_line("Radio Links count changed from %d to %d", pData1->links_count, pData2->links_count);
      iDifferences++;
   }

   for( int i=0; i<pData1->links_count; i++ )
   {
      if ( pData1->link_frequency_khz[i] != pData2->link_frequency_khz[i] )
      {
         strcpy(szTmp, str_format_frequency(pData1->link_frequency_khz[i]));
         log_line("* Radio Link %d freq changed from %s to %s", i+1, szTmp, str_format_frequency(pData2->link_frequency_khz[i]));
         iDifferences++;
      }

      if ( pData1->link_capabilities_flags[i] != pData2->link_capabilities_flags[i] )
      {
         str_get_radio_capabilities_description(pData1->link_capabilities_flags[i], szBuff1);
         str_get_radio_capabilities_description(pData2->link_capabilities_flags[i], szBuff2);
         log_line("* Radio Link %d capabilities changed from %s to %s", i+1, szBuff1, szBuff2);
         iDifferences++;
      }

      if ( pData1->link_radio_flags[i] != pData2->link_radio_flags[i] )
      {
         str_get_radio_frame_flags_description(pData1->link_radio_flags[i], szBuff1);
         str_get_radio_frame_flags_description(pData2->link_radio_flags[i], szBuff2);
         log_line("* Radio Link %d radio flags changed from %s to %s", i+1, szBuff1, szBuff2);
         iDifferences++;
      }

      if ( pData1->link_datarate_video_bps[i] != pData2->link_datarate_video_bps[i] )
      {
         str_format_bitrate(pData1->link_datarate_video_bps[i], szBuff1);
         str_format_bitrate(pData2->link_datarate_video_bps[i], szBuff2);
         log_line("* Radio Link %d video data rate changed from %s to %s", i+1, szBuff1, szBuff2);
         iDifferences++;
      }
      if ( pData1->link_datarate_data_bps[i] != pData2->link_datarate_data_bps[i] )
      {
         str_format_bitrate(pData1->link_datarate_data_bps[i], szBuff1);
         str_format_bitrate(pData2->link_datarate_data_bps[i], szBuff2);
         log_line("* Radio Link %d data data rate changed from %s to %s", i+1, szBuff1, szBuff2);
         iDifferences++;
      }

      if ( pData1->uplink_datarate_video_bps[i] != pData2->uplink_datarate_video_bps[i] )
      {
         str_format_bitrate(pData1->uplink_datarate_video_bps[i], szBuff1);
         str_format_bitrate(pData2->uplink_datarate_video_bps[i], szBuff2);
         log_line("* Radio Link %d uplink video data rate changed from %s to %s", i+1, szBuff1, szBuff2);
         iDifferences++;
      }
      if ( pData1->uplink_datarate_data_bps[i] != pData2->uplink_datarate_data_bps[i] )
      {
         str_format_bitrate(pData1->uplink_datarate_data_bps[i], szBuff1);
         str_format_bitrate(pData2->uplink_datarate_data_bps[i], szBuff2);
         log_line("* Radio Link %d uplink data data rate changed from %s to %s", i+1, szBuff1, szBuff2);
         iDifferences++;
      }

      if ( pData1->uDownlinkDataDataRateType[i] != pData2->uDownlinkDataDataRateType[i] )
      {
         log_line("* Radio Link %d downlink data rate type changed from %d to %d", i+1, pData1->uDownlinkDataDataRateType[i], pData2->uDownlinkDataDataRateType[i]);
         iDifferences++;
      }
      if ( pData1->uUplinkDataDataRateType[i] != pData2->uUplinkDataDataRateType[i] )
      {
         log_line("* Radio Link %d uplink data rate type changed from %d to %d", i+1, pData1->uUplinkDataDataRateType[i], pData2->uUplinkDataDataRateType[i]);
         iDifferences++;
      }

      if ( pData1->uSerialPacketSize[i] != pData2->uSerialPacketSize[i] )
      {
         log_line("* Radio Link %d serial packet size changed from %d to %d", i+1, pData1->uSerialPacketSize[i], pData2->uSerialPacketSize[i]);
         iDifferences++;
      }
   }

   if ( pData1->iSiKPacketSize != pData2->iSiKPacketSize )
   {
      log_line("* Radio Links SiK packet size changed from %d to %d", pData1->iSiKPacketSize, pData2->iSiKPacketSize);
      iDifferences++;
   }

   if ( (pData1->uGlobalRadioLinksFlags & (~MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)) != (pData2->uGlobalRadioLinksFlags & (~MODEL_RADIOLINKS_FLAGS_HAS_NEGOCIATED_LINKS)) )
   {
      log_line("* Radio Links global radio links flags changed from %u to %u", pData1->uGlobalRadioLinksFlags, pData2->uGlobalRadioLinksFlags);
      iDifferences++;
   }

   if ( 0 == iDifferences )
      log_line("* There are no differences in the radio links params.");
   return iDifferences;
}

bool Model::find_and_validate_camera_settings()
{
   if ( hardware_is_station() )
      return false;

   bool bUpdated = false;
   int camera_now = hardware_getCameraType();

   if ( ! hardware_hasCamera() )
   {
      if ( iCurrentCamera >= 0 )
      {
         log_line("Validating camera info: has no camera and current camera was positive. Set it to -1");
         bUpdated = true;
      }
      if ( iCameraCount > 0 )
      {
         log_line("Validating camera info: had one camera before, now has no camera.");
         bUpdated = true;
      }
      iCurrentCamera = -1;
      iCameraCount = 0;
   }
   else
   {
      if ( iCurrentCamera < 0 )
      {
         log_line("Validating camera info: has camera and current camera was negative. Set it to 0");
         bUpdated = true;
         iCurrentCamera = 0;
      }
      if ( iCameraCount <= 0 )
      {
         iCameraCount = 1;
         log_line("Validating camera info: had no camera before, now has a camera.");
         bUpdated = true;
         camera_params[0].iCameraType = CAMERA_TYPE_NONE;
      }
   }

   if ( iCameraCount < 0 )
   {
      iCameraCount = 0;
      bUpdated = true;
   }
   if ( iCameraCount >= MODEL_MAX_CAMERAS )
   {
      iCameraCount = MODEL_MAX_CAMERAS-1;
      bUpdated = true;
   }
   if ( iCurrentCamera >= MODEL_MAX_CAMERAS )
   {
      iCurrentCamera = MODEL_MAX_CAMERAS-1;
      bUpdated = true;
   }

   if ( iCameraCount == 0 )
   {
      log_line("Validating camera info: There is no camera. No more updates. Was updated just now? %s", bUpdated?"yes":"no");
      return bUpdated;
   }
   if ( camera_params[iCurrentCamera].iCameraType != camera_now )
   {
      char szTmp[64];
      char szTmp2[64];
      str_get_hardware_camera_type_string_to_string(camera_params[iCurrentCamera].iCameraType, szTmp);
      str_get_hardware_camera_type_string_to_string(camera_now, szTmp2);
      log_line("Validating camera info: hardware camera_%d has changed from %u, %u to new type: %u, %s", iCurrentCamera, camera_params[iCurrentCamera].iCameraType, szTmp, camera_now, szTmp2);
      if ( (camera_now != CAMERA_TYPE_NONE) && (camera_params[iCurrentCamera].iCameraType < CAMERA_TYPE_IP) )
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
      if ( hardware_isCameraVeye() )
      {
         resetCameraToDefaults(iCurrentCamera);
         camera_params[iCurrentCamera].iCurrentProfile = 0;
         for( int i=0; i<MODEL_CAMERA_PROFILES; i++ )
            resetCameraProfileToDefaults(&(camera_params[iCurrentCamera].profiles[i]));
      }
      bUpdated = true;
   }
   else
   {
      char szTmp[64];
      str_get_hardware_camera_type_string_to_string(camera_now, szTmp);
      log_line("Validating camera info: camera has not changed. Camera type: %d, %s", camera_now, szTmp);
   }
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

// Returns true if changes where made
bool Model::validate_fps_and_exposure_settings(type_video_link_profile* pVideoLinkProfile, camera_profile_parameters_t* pCameraProfile)
{
   if ( isRunningOnOpenIPCHardware() )
   if ( hardware_board_is_sigmastar(hwCapabilities.uBoardType) )
   {
      if ( pVideoLinkProfile->fps > 0 )
      if ( pCameraProfile->shutterspeed >= 1000/pVideoLinkProfile->fps )
      {
         pCameraProfile->shutterspeed = 1000/pVideoLinkProfile->fps - 1;
         return true;
      }
   }
   return false;
}

bool Model::validate_settings()
{
   log_line("Model: Validating model settings...");
   for( unsigned int i=0; i<(sizeof(hwCapabilities.dummyhwc)/sizeof(hwCapabilities.dummyhwc[0])); i++ )
      hwCapabilities.dummyhwc[i] = 0;
   for( unsigned int i=0; i<(sizeof(hwCapabilities.dummyhwc2)/sizeof(hwCapabilities.dummyhwc2[0])); i++ )
      hwCapabilities.dummyhwc2[i] = 0;

   if ( rc_params.channelsCount < 2 || rc_params.channelsCount > MAX_RC_CHANNELS )
      rc_params.channelsCount = 8;

   if ( (telemetry_params.iVideoBitrateHistoryGraphSampleInterval < 10) || (telemetry_params.iVideoBitrateHistoryGraphSampleInterval > 1000) )
      telemetry_params.iVideoBitrateHistoryGraphSampleInterval = 200;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      if ( (radioLinksParams.uUplinkDataDataRateType[i] <= 0) || (radioLinksParams.uUplinkDataDataRateType[i] > 3) )
         radioLinksParams.uUplinkDataDataRateType[i] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO;
      if ( (radioLinksParams.uDownlinkDataDataRateType[i] <= 0) || (radioLinksParams.uDownlinkDataDataRateType[i] > 3) )
         radioLinksParams.uDownlinkDataDataRateType[i] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO;

      if ( (radioInterfacesParams.interface_raw_power[i] < 1) || (radioInterfacesParams.interface_raw_power[i] > 71) )
         radioInterfacesParams.interface_raw_power[i] = DEFAULT_RADIO_TX_POWER;
   }

   if ( (radioLinksParams.iSiKPacketSize < DEFAULT_RADIO_SERIAL_AIR_MIN_PACKET_SIZE) ||
        (radioLinksParams.iSiKPacketSize > DEFAULT_RADIO_SERIAL_AIR_MAX_PACKET_SIZE) )
      radioLinksParams.iSiKPacketSize = DEFAULT_SIK_PACKET_SIZE;

   // Validate high capacity link flags

   if ( hardware_is_vehicle() )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
            continue;
       
         if ( pRadioHWInfo->isHighCapacityInterface )
         if ( ! (radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
         {
            radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
            log_line("Validate settings: Add missing high capacity flag to radio interface: %d", i+1);
         }
         if ( ! pRadioHWInfo->isHighCapacityInterface )
         if ( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
         {
            radioInterfacesParams.interface_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
            log_line("Validate settings: Remove high capacity flag from radio interface: %d", i+1);
         }
         if ( pRadioHWInfo->isHighCapacityInterface )
            radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
         else
            radioInterfacesParams.interface_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
      }
   }

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      if ( !( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
         radioInterfacesParams.interface_capabilities_flags[i] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      int iLinkId = radioInterfacesParams.interface_link_id[i];
      if ( (iLinkId < 0) || (iLinkId >= MAX_RADIO_INTERFACES) )
         continue;

      if ( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY)
      if ( ! (radioLinksParams.link_capabilities_flags[iLinkId] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
      {
         log_line("Validate settings: Add missing high capacity flag to radio link: %d", iLinkId+1);
         radioLinksParams.link_capabilities_flags[iLinkId] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
         radioLinksParams.link_capabilities_flags[iLinkId] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      }
      if ( ! (radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
      if ( radioLinksParams.link_capabilities_flags[iLinkId] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
      {
         log_line("Validate settings: Remove high capacity flag from radio link: %d", iLinkId+1);
         radioLinksParams.link_capabilities_flags[iLinkId] &= ~RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
         radioLinksParams.link_capabilities_flags[iLinkId] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
      }
      if ( !( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY) )
         radioLinksParams.link_capabilities_flags[iLinkId] &= ~RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO;
   }

   validate_radio_flags();

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      if ( 0 == radioInterfacesParams.interface_szMAC[i][0] )
         strcpy(radioInterfacesParams.interface_szMAC[i], "N/A");
      if ( 0 == radioInterfacesParams.interface_szPort[i][0] )
         strcpy(radioInterfacesParams.interface_szPort[i], "X");
   }


   for( int i=0; i<radioLinksParams.links_count; i++ )
   {
      if ( radioLinksParams.link_frequency_khz[i] == 0 )
         radioLinksParams.link_frequency_khz[i] = DEFAULT_FREQUENCY;
   }

   for( int i=0; i<radioLinksParams.links_count; i++ )
   {
      if ( getRealDataRateFromRadioDataRate(radioLinksParams.link_datarate_video_bps[i], 0) < 500000 )
      if ( radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      {
         if ( ! radioLinkIsSiKRadio(i) )
         {
            log_softerror_and_alarm("Invalid radio video data rates (%d). Reseting to default (%d).", radioLinksParams.link_datarate_video_bps[i], DEFAULT_RADIO_DATARATE_VIDEO);
            radioLinksParams.link_datarate_video_bps[i] = DEFAULT_RADIO_DATARATE_VIDEO;
         }
      }
      if ( getRealDataRateFromRadioDataRate(radioLinksParams.link_datarate_data_bps[i], 0) < 500 )
      if ( radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      {
         int iTmp = radioLinksParams.link_datarate_data_bps[i];
         if ( radioLinkIsSiKRadio(i) )
            radioLinksParams.link_datarate_data_bps[i] = DEFAULT_RADIO_DATARATE_SIK_AIR;
         else
            radioLinksParams.link_datarate_data_bps[i] = DEFAULT_RADIO_DATARATE_DATA;
         log_softerror_and_alarm("Invalid radio data data rates (%d). Reseting to default (%d).", iTmp, radioLinksParams.link_datarate_data_bps[i]);
      }
   }

   if ( osd_params.iCurrentOSDLayout < osdLayout1 || osd_params.iCurrentOSDLayout >= osdLayoutLast )
      osd_params.iCurrentOSDLayout = osdLayout1;

   
   int nOSDFlagsIndex = osd_params.iCurrentOSDLayout;
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

   if ( rxtx_sync_type < 0 || rxtx_sync_type >= RXTX_SYNC_TYPE_LAST )
      rxtx_sync_type = RXTX_SYNC_TYPE_BASIC;

   if ( processesPriorities.iNiceRouter < -18 || processesPriorities.iNiceRouter > 5 )
      processesPriorities.iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
   if ( processesPriorities.iNiceVideo < -18 || processesPriorities.iNiceVideo > 5 )
      processesPriorities.iNiceVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_TX;
   if ( processesPriorities.iNiceRC < -16 || processesPriorities.iNiceRC > 0 )
      processesPriorities.iNiceRC = DEFAULT_PRIORITY_PROCESS_RC;
   if ( processesPriorities.iNiceTelemetry < -16 || processesPriorities.iNiceTelemetry > 5 )
      processesPriorities.iNiceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY;

   if ( audio_params.volume < 0 || audio_params.volume > 100 )
      audio_params.volume = 90;
   if ( audio_params.quality < 0 || audio_params.quality > 3 )
      audio_params.quality = 1;
   if ( 0 == (audio_params.uECScheme & 0xF0) )
   {
      audio_params.uECScheme = (((u32)DEFAULT_AUDIO_P_DATA) << 4) | ((u32)DEFAULT_AUDIO_P_EC);
   }
   if ( ((audio_params.uECScheme >> 4) & 0x0F) < (audio_params.uECScheme & 0x0F) )
   {
      audio_params.uECScheme = (((u32)DEFAULT_AUDIO_P_DATA) << 4) | ((u32)DEFAULT_AUDIO_P_EC);
   }
   if ( ((audio_params.uFlags >> 8) & 0xFF) >= (MAX_BUFFERED_AUDIO_PACKETS*2)/3 )
   {
      audio_params.uFlags &= 0xFFFF00FF;
      audio_params.uFlags |= ((u32)(MAX_BUFFERED_AUDIO_PACKETS*2)/3) << 8;
   }

   if ( (audio_params.uPacketLength < 50) || (audio_params.uPacketLength > 1200) )
      audio_params.uPacketLength = DEFAULT_AUDIO_PACKET_LENGTH;
     
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      if ( video_link_profiles[i].video_data_length > MAX_VIDEO_PACKET_DATA_SIZE )
         video_link_profiles[i].video_data_length = MAX_VIDEO_PACKET_DATA_SIZE;

   if ( video_params.videoAdjustmentStrength < 1 || video_params.videoAdjustmentStrength > 10 )
      video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;

   validate_relay_links_flags();

   if ( (hwCapabilities.iMaxTxVideoBlocksBuffer < 2) || (hwCapabilities.iMaxTxVideoBlocksBuffer > MAX_RXTX_BLOCKS_BUFFER) )
      hwCapabilities.iMaxTxVideoBlocksBuffer = MAX_RXTX_BLOCKS_BUFFER;
   if ( (hwCapabilities.iMaxTxVideoBlockPackets < 2) || (hwCapabilities.iMaxTxVideoBlockPackets > MAX_TOTAL_PACKETS_IN_BLOCK) )
      hwCapabilities.iMaxTxVideoBlockPackets = MAX_TOTAL_PACKETS_IN_BLOCK;

   log_line("Model: Validated model settings.");
   return true;
}

bool Model::validate_relay_links_flags()
{
   bool bAnyChange = false;

   if ( relay_params.isRelayEnabledOnRadioLinkId < 0 )
   {
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         if ( radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         {
            log_line("Validate relay params: removed relay flag from radio link %d as relayed is not enabled on any radio links.", i+1);
            radioLinksParams.link_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
            bAnyChange = true;
         }
         if ( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         {
            log_line("Validate relay params: removed relay flag from radio interface %d as relayed is not enabled on any radio links.", i+1);
            radioInterfacesParams.interface_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
            bAnyChange = true;
         }
      }
      if ( bAnyChange )
         log_line("Validated model relay settings: removed invalid relay flags as relaying is not enabled on any link.");
      return bAnyChange;
   }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      if ( (relay_params.isRelayEnabledOnRadioLinkId == i) && (i<radioLinksParams.links_count) )
      {
         if ( !(radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY) )
         {
            log_line("Validate relay params: added relay flag to radio link %d as relayed is enabled on radio link %d.", i+1, relay_params.isRelayEnabledOnRadioLinkId+1);
            radioLinksParams.link_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
            //bAnyChange = true;
         }
      }
      else
      {
         if ( radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
         {
            log_line("Validate relay params: removed relay flag from radio link %d as relayed is enabled on radio link %d.", i+1, relay_params.isRelayEnabledOnRadioLinkId+1);
            radioLinksParams.link_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);       
            bAnyChange = true;
         }
      }
   }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      int iRadioLinkId = radioInterfacesParams.interface_link_id[i];
      
      if ( (iRadioLinkId) < 0 || (iRadioLinkId >= MAX_RADIO_INTERFACES) || (i >= radioInterfacesParams.interfaces_count) )
      if ( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      {
         log_line("Validate relay params: removed radio interface %d relay flag as it's not assigned to any radio link.", i+1);
         radioInterfacesParams.interface_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
         bAnyChange = true;
      }

      if ( (iRadioLinkId >= 0) && (iRadioLinkId < MAX_RADIO_INTERFACES) && (i < radioInterfacesParams.interfaces_count) )
      {
         if ( iRadioLinkId == relay_params.isRelayEnabledOnRadioLinkId )
         {
            if ( !(radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY) )
            {
               log_line("Validate relay params: added radio interface %d relay flag as it's assigned to relay radio link %d.", i+1, relay_params.isRelayEnabledOnRadioLinkId+1);
               radioInterfacesParams.interface_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
               //bAnyChange = true;
            }
         }
         else
         {
            if ( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
            {
               log_line("Validate relay params: removed radio interface %d relay flag as it's not assigned to relay radio link %d.", i+1, relay_params.isRelayEnabledOnRadioLinkId+1);
               radioInterfacesParams.interface_capabilities_flags[i] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);       
               bAnyChange = true;
            }
         }
      }
   }

   if ( 1 == radioLinksParams.links_count )
   if ( radioLinksParams.link_capabilities_flags[0] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      log_line("Validate relay flags: Removed relay flag from radio link 1 as it's the only link present.");
      radioLinksParams.link_capabilities_flags[0] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      radioInterfacesParams.interface_capabilities_flags[0] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      bAnyChange = true;
   }

   if ( 1 == radioLinksParams.links_count )
   if ( radioInterfacesParams.interface_capabilities_flags[0] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      log_line("Validate relay flags: Removed relay flag from radio interface 1 as it's the only interface present.");
      radioLinksParams.link_capabilities_flags[0] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      radioInterfacesParams.interface_capabilities_flags[0] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
      bAnyChange = true;
   }

   if ( bAnyChange )
      log_line("Validated model relay settings: removed invalid relay flags.");
   return bAnyChange;
}

void Model::validate_radio_flags()
{
   for( int i=0; i<radioLinksParams.links_count; i++ )
   {
      // Clear and set datarate type
      u32 uCurrentRadioFlags = radioLinksParams.link_radio_flags[i];
      radioLinksParams.link_radio_flags[i] &= ~(RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_USE_MCS_DATARATES);
      if ( radioLinksParams.link_datarate_video_bps[i] < 0 )
         radioLinksParams.link_radio_flags[i] |= RADIO_FLAGS_USE_MCS_DATARATES;
      else
         radioLinksParams.link_radio_flags[i] |= RADIO_FLAGS_USE_LEGACY_DATARATES;
      if ( uCurrentRadioFlags != radioLinksParams.link_radio_flags[i] )
         log_line("Model: Updated radio link %d radio flags to match data rates.", i+1);
      for( int k=0; k<radioInterfacesParams.interfaces_count; k++ )
      {
         if ( radioInterfacesParams.interface_link_id[k] == i )
         {
            uCurrentRadioFlags = radioInterfacesParams.interface_current_radio_flags[k];
            radioInterfacesParams.interface_current_radio_flags[k] &= ~(RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_USE_MCS_DATARATES);
            radioInterfacesParams.interface_current_radio_flags[k] |= (radioLinksParams.link_radio_flags[i] & (RADIO_FLAGS_USE_LEGACY_DATARATES | RADIO_FLAGS_USE_MCS_DATARATES));
            if ( uCurrentRadioFlags != radioInterfacesParams.interface_current_radio_flags[k] )
               log_line("Model: Updated radio interface %d on radio link %d radio flags to match data rates.", k+1, i+1);
         }
      }
   }
}


void Model::resetToDefaults(bool generateId)
{
   log_line("Reseting vehicle settings to default...");
   strcpy(vehicle_name, DEFAULT_VEHICLE_NAME);
   iLoadedFileVersion = 0;

   struct timeval ste;
   gettimeofday(&ste, NULL); // get current time
   srand((unsigned int)(ste.tv_usec));
   if ( generateId )
      generateUID();
   else
      log_line("Reusing the same unique vehicle ID (%u).", uVehicleId);

   hwCapabilities.uBoardType = 0;
   if ( hardware_is_vehicle() )
      hwCapabilities.uBoardType = hardware_getOnlyBoardType();
   resetHWCapabilities();

   if ( generateId )
   {
      int iMajor = 0;
      int iMinor = 0;
      get_Ruby_BaseVersion(&iMajor, &iMinor);
      hwCapabilities.uRubyBaseVersion = (((u32)iMajor)<<8) | (((u32)iMinor) & 0xFF);
   }
   
   uControllerId = 0;
   sw_version = (SYSTEM_SW_VERSION_MAJOR * 256 + SYSTEM_SW_VERSION_MINOR) | (SYSTEM_SW_BUILD_NUMBER<<16);
   log_line("SW Version: %d.%d (b%d)", (sw_version >> 8) & 0xFF, sw_version & 0xFF, sw_version >> 16);
   vehicle_type = (MODEL_TYPE_DRONE & MODEL_TYPE_MASK) | ((MODEL_FIRMWARE_TYPE_RUBY << 5) & MODEL_FIRMWARE_MASK);
   is_spectator = true;
   constructLongName();

   enc_flags = MODEL_ENC_FLAGS_NONE;
   alarms = 0;

   uModelFlags = MODEL_FLAG_USE_LOGER_SERVICE | MODEL_FLAG_PRIORITIZE_UPLINK;
     
   m_iRadioInterfacesGraphRefreshInterval = 3;

   radioInterfacesParams.interfaces_count = 0;
   resetRadioLinksParams();

   if ( hardware_is_station() )
   {
      hardwareInterfacesInfo.radio_interface_count = 0;
      hardwareInterfacesInfo.i2c_bus_count = 0;
      hardwareInterfacesInfo.i2c_device_count = 0;
      hardwareInterfacesInfo.serial_port_count = 0;

      radioInterfacesParams.interfaces_count = 0;
      radioLinksParams.links_count = 0;
   }
   else
   {
      populateHWInfo();
      populateRadioInterfacesInfoFromHardware();
   }
   iGPSCount = 1;

   enableDHCP = false;
   uDummyM1 = 0;
   
   uDeveloperFlags = (((u32)DEFAULT_DELAY_WIFI_CHANGE)<<8);
   uDeveloperFlags |= DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS;
   uDeveloperFlags |= DEVELOPER_FLAGS_USE_PCAP_RADIO_TX;

   if ( DEFAULT_USE_PPCAP_FOR_TX )
      uDeveloperFlags |= DEVELOPER_FLAGS_USE_PCAP_RADIO_TX;
   else
      uDeveloperFlags &= (~DEVELOPER_FLAGS_USE_PCAP_RADIO_TX);

   resetRelayParamsToDefaults(&relay_params);

   rxtx_sync_type = RXTX_SYNC_TYPE_BASIC;

   processesPriorities.uProcessesFlags = PROCESSES_FLAGS_BALANCE_INT_CORES;
   processesPriorities.iNiceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY;
   if ( isRunningOnOpenIPCHardware() )
      processesPriorities.iNiceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY_OIPC;

   processesPriorities.iNiceRC = DEFAULT_PRIORITY_PROCESS_RC;
   processesPriorities.iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER;
   if ( isRunningOnOpenIPCHardware() )
      processesPriorities.iNiceRouter = DEFAULT_PRIORITY_PROCESS_ROUTER_OPIC;
   processesPriorities.ioNiceRouter = DEFAULT_IO_PRIORITY_ROUTER;
   processesPriorities.iNiceVideo = DEFAULT_PRIORITY_PROCESS_VIDEO_TX;
   if ( hardware_board_is_openipc(hardware_getOnlyBoardType()) )
      processesPriorities.iNiceVideo = 0;
   processesPriorities.iNiceOthers = DEFAULT_PRIORITY_PROCESS_OTHERS;
   processesPriorities.ioNiceVideo = DEFAULT_IO_PRIORITY_VIDEO_TX;
   processesPriorities.iOverVoltage = DEFAULT_OVERVOLTAGE;
   processesPriorities.iFreqARM = DEFAULT_ARM_FREQ;
   processesPriorities.iFreqGPU = DEFAULT_GPU_FREQ;

   if ( isRunningOnOpenIPCHardware() )
   if ( hardware_board_is_sigmastar(hwCapabilities.uBoardType & BOARD_TYPE_MASK) )
   {
      processesPriorities.iFreqARM = DEFAULT_FREQ_OPENIPC_SIGMASTAR;
      processesPriorities.iFreqGPU = 0;
   }

   processesPriorities.iThreadPriorityRadioRx = DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_RX;
   processesPriorities.iThreadPriorityRadioTx = DEFAULT_PRIORITY_VEHICLE_THREAD_RADIO_TX;
   processesPriorities.iThreadPriorityRouter = DEFAULT_PRIORITY_VEHICLE_THREAD_ROUTER;

   resetVideoParamsToDefaults();
   resetCameraToDefaults(-1);

   resetOSDFlags();

   resetAudioParams();

   alarms_params.uAlarmMotorCurrentThreshold = (1<<7) | 30; // Enable alarm, set current theshold to 3 Amps
   
   loggingParams.uLogTypeFlags = LOG_TYPE_FLAGS_ARMED;
   loggingParams.uLogIntervalMilisec = 50;
   loggingParams.uLogParams = LOG_PARAM_RADIO_DBM | LOG_PARAM_RADIO_DATARATE | LOG_PARAMS_VIDEO_PACKETS | LOG_PARAMS_VIDEO_RETRANSMISSIONS;
   loggingParams.uLogParams2 = 0;
   loggingParams.uDummy = 0;


   resetRCParams();
   resetTelemetryParams();

   resetFunctionsParamsToDefaults();
   log_line("Reseting vehicle settings to default: complete.");
}

void Model::resetAudioParams()
{
   log_line("Model: Did reset audio params.");
   audio_params.has_audio_device = false;
   audio_params.uECScheme = (((u32)DEFAULT_AUDIO_P_DATA) << 4) | ((u32)DEFAULT_AUDIO_P_EC);
   audio_params.uPacketLength = DEFAULT_AUDIO_PACKET_LENGTH;
   audio_params.uDummyA1 = 0;
   audio_params.uFlags = ((u32)(DEFAULT_AUDIO_BUFFERING_SIZE)) << 8;
   if ( isRunningOnOpenIPCHardware() )
   {
      audio_params.has_audio_device = true;
      if ( hardware_board_has_audio_builtin(hwCapabilities.uBoardType) )
      {
         audio_params.uFlags &= ~((u32)0x03);
         audio_params.uFlags |= (u32)0x01;
      }
   }
   audio_params.enabled = false;
   audio_params.volume = 90;
   audio_params.quality = 1;
}

void Model::resetHWCapabilities()
{
   memset(&hwCapabilities, 0, sizeof(type_hardware_capabilities));
   hwCapabilities.iMaxTxVideoBlocksBuffer = MAX_RXTX_BLOCKS_BUFFER;
   hwCapabilities.iMaxTxVideoBlockPackets = MAX_TOTAL_PACKETS_IN_BLOCK;
   hwCapabilities.uFlags = 0;

   if ( hardware_board_is_goke( hardware_getOnlyBoardType() ) )
      hwCapabilities.uFlags &= ~MODEL_HW_CAP_FLAG_OTA;
   else
      hwCapabilities.uFlags |= MODEL_HW_CAP_FLAG_OTA;
}

void Model::resetRadioLinksParams()
{
   log_line("Model: Reset radio links params...");
   radioInterfacesParams.iAutoVehicleTxPower = 1;
   radioInterfacesParams.iAutoControllerTxPower = 1;
   radioInterfacesParams.iDummyR3 = 0;
   radioInterfacesParams.iDummyR4 = 0;
   radioInterfacesParams.iDummyR5 = 0;
   radioInterfacesParams.iDummyR6 = 0;
   radioInterfacesParams.iDummyR7 = 0;
   radioInterfacesParams.iDummyR8 = 0;
   radioInterfacesParams.iDummyR9 = 0;
   
   radioLinksParams.iSiKPacketSize = DEFAULT_SIK_PACKET_SIZE;
   radioLinksParams.uGlobalRadioLinksFlags = 0;
   if ( DEFAULT_BYPASS_SOCKET_BUFFERS )
      radioLinksParams.uGlobalRadioLinksFlags |= MODEL_RADIOLINKS_FLAGS_BYPASS_SOCKETS_BUFFERS;
     
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radioInterfacesParams.interface_raw_power[i] = DEFAULT_RADIO_TX_POWER;
      radioInterfacesParams.interface_link_id[i] = -1;
   }

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      resetRadioLinkDataRatesAndFlags(i);
      radioLinksParams.link_frequency_khz[i] = 0;
   }

   for( unsigned int j=0; j<(sizeof(radioLinksParams.uDummyRadio)/sizeof(radioLinksParams.uDummyRadio[0])); j++ )
      radioLinksParams.uDummyRadio[j] = 0;

   log_line("Model: Did reset radio links params.");
}

void Model::resetRelayParamsToDefaults(type_relay_parameters* pRelayParams)
{
   if ( NULL == pRelayParams )
      return;
   pRelayParams->isRelayEnabledOnRadioLinkId = -1;
   pRelayParams->uCurrentRelayMode = 0;
   pRelayParams->uRelayFrequencyKhz = 0;
   pRelayParams->uRelayedVehicleId = 0;
   pRelayParams->uRelayCapabilitiesFlags = 0;
}


void Model::resetOSDFlags()
{
   memset(&osd_params, 0, sizeof(osd_params));

   osd_params.iCurrentOSDLayout = osdLayout1;
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
   
   osd_params.uFlags = OSD_BIT_FLAGS_SHOW_FLIGHT_END_STATS;
   
   osd_params.osd_flags[0] = 0; // horizontal layout for stats panels;
   osd_params.osd_flags[1] = OSD_FLAG_SHOW_DISTANCE | OSD_FLAG_SHOW_ALTITUDE | OSD_FLAG_SHOW_BATTERY;
   osd_params.osd_flags[2] = OSD_FLAG_SHOW_DISTANCE | OSD_FLAG_SHOW_ALTITUDE | OSD_FLAG_SHOW_BATTERY | OSD_FLAG_SHOW_HOME | OSD_FLAG_SHOW_VIDEO_MODE | OSD_FLAG_SHOW_FLIGHT_MODE | OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE | OSD_FLAG_SHOW_RADIO_INTERFACES_INFO;
   osd_params.osd_flags[3] = 0;
   osd_params.osd_flags[4] = 0;
   osd_params.osd_flags2[0] = OSD_FLAG2_LAYOUT_ENABLED | OSD_FLAG2_SHOW_BGBARS | OSD_FLAG2_SHOW_STATS_RADIO_INTERFACES | OSD_FLAG2_SHOW_RC_RSSI;
   osd_params.osd_flags2[1] =                            OSD_FLAG2_SHOW_BGBARS | OSD_FLAG2_RELATIVE_ALTITUDE | OSD_FLAG2_SHOW_RC_RSSI;
   osd_params.osd_flags2[2] =                            OSD_FLAG2_SHOW_BGBARS | OSD_FLAG2_RELATIVE_ALTITUDE | OSD_FLAG2_SHOW_RC_RSSI;
   osd_params.osd_flags2[3] = OSD_FLAG2_LAYOUT_ENABLED | OSD_FLAG2_SHOW_BGBARS | OSD_FLAG2_SHOW_RC_RSSI;
   osd_params.osd_flags2[4] =                            OSD_FLAG2_SHOW_BGBARS | OSD_FLAG2_SHOW_RC_RSSI;


   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
   {
      osd_params.osd_flags[i] |= OSD_FLAG_SHOW_BATTERY | OSD_FLAG_SHOW_DISTANCE | OSD_FLAG_SHOW_ALTITUDE | OSD_FLAG_SHOW_HOME;
      osd_params.osd_flags[i] |= OSD_FLAG_SHOW_GPS_INFO | OSD_FLAG_SHOW_FLIGHT_MODE | OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE;
      if ( i < 3 )
      {
         osd_params.osd_flags[i] |= OSD_FLAG_SHOW_VIDEO_MODE | OSD_FLAG_SHOW_VIDEO_MBPS | OSD_FLAG_SHOW_VIDEO_MODE_EXTENDED;
      }
      osd_params.osd_flags[i] |= OSD_FLAG_SHOW_RADIO_LINKS | OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS;
      osd_params.osd_flags[i] |= OSD_FLAG_SHOW_RADIO_INTERFACES_INFO;
      osd_params.osd_flags2[i] |= OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_NUMBERS | OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_BARS;
      osd_params.osd_flags2[i] |= OSD_FLAG2_SHOW_GROUND_SPEED;
      osd_params.osd_flags2[i] |= OSD_FLAG2_SHOW_MINIMAL_VIDEO_DECODE_STATS | OSD_FLAG2_SHOW_MINIMAL_RADIO_INTERFACES_STATS;
      osd_params.osd_flags2[i] |= OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY;
      osd_params.osd_flags2[i] |= OSD_FLAG2_SHOW_BGBARS;
      //osd_params.osd_flags2[i] |= OSD_FLAG2_FLASH_OSD_ON_TELEMETRY_DATA_LOST;

      //osd_params.osd_flags3[i] = OSD_FLAG3_SHOW_GRID_DIAGONAL | OSD_FLAG3_SHOW_GRID_SQUARES;
      osd_params.osd_flags3[i] = OSD_FLAG3_SHOW_GRID_THIRDS_SMALL;
      osd_params.osd_flags3[i] |= OSD_FLAG3_HIGHLIGHT_CHANGING_ELEMENTS;
      osd_params.osd_flags3[i] |= OSD_FLAG3_RENDER_MSP_OSD;
      osd_params.osd_preferences[i] = ((u32)2) | (((u32)2)<<8) | (((u32)2)<<16) | (((u32)1)<<20) | OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM;
      osd_params.osd_preferences[i] |= OSD_PREFERENCES_BIT_FLAG_ARANGE_STATS_WINDOWS_RIGHT;
   }

   for( int i=0; i<MODEL_MAX_OSD_PROFILES; i++ )
      osd_params.instruments_flags[i] = 0;
   //osd_params.instruments_flags[0] = INSTRUMENTS_FLAG_SHOW_ALL_OSD_PLUGINS_MASK;

   osd_params.osd_flags[1] = OSD_FLAG_SHOW_ALTITUDE | OSD_FLAG_SHOW_BATTERY;
   osd_params.osd_flags[1] |= OSD_FLAG_SHOW_DISTANCE | OSD_FLAG_SHOW_ALTITUDE;
   osd_params.osd_flags[1] |= OSD_FLAG_SHOW_FLIGHT_MODE | OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE;
   osd_params.osd_flags[1] |= OSD_FLAG_SHOW_RADIO_LINKS | OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS;
   osd_params.osd_flags2[1] = OSD_FLAG2_SHOW_BGBARS | OSD_FLAG2_RELATIVE_ALTITUDE;
   osd_params.osd_flags2[1] |= OSD_FLAG2_SHOW_GROUND_SPEED;
   //osd_params.osd_flags2[1] |= OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY;

   osd_params.osd_flags[2] |= OSD_FLAG_SHOW_BATTERY | OSD_FLAG_SHOW_DISTANCE | OSD_FLAG_SHOW_ALTITUDE | OSD_FLAG_SHOW_HOME;
   osd_params.osd_flags[2] |= OSD_FLAG_SHOW_GPS_INFO | OSD_FLAG_SHOW_FLIGHT_MODE | OSD_FLAG_SHOW_FLIGHT_MODE_CHANGE;
   osd_params.osd_flags[2] |= OSD_FLAG_SHOW_VIDEO_MODE | OSD_FLAG_SHOW_VIDEO_MBPS;
   osd_params.osd_flags[2] |= OSD_FLAG_SHOW_RADIO_LINKS | OSD_FLAG_SHOW_VEHICLE_RADIO_LINKS;
   osd_params.osd_flags2[2] = OSD_FLAG2_SHOW_BGBARS | OSD_FLAG2_RELATIVE_ALTITUDE | OSD_FLAG2_SHOW_RC_RSSI;
   osd_params.osd_flags2[2] |= OSD_FLAG2_SHOW_RADIO_LINK_QUALITY_NUMBERS;
   osd_params.osd_flags2[2] |= OSD_FLAG2_SHOW_GROUND_SPEED;
   //osd_params.osd_flags2[2] |= OSD_FLAG2_SHOW_BACKGROUND_ON_TEXTS_ONLY;
}

void Model::resetTelemetryParams()
{
   memset(&telemetry_params, 0, sizeof(telemetry_params));

   telemetry_params.fc_telemetry_type = TELEMETRY_TYPE_MAVLINK;
   
   telemetry_params.iVideoBitrateHistoryGraphSampleInterval = 200;
   telemetry_params.dummy5 = 0;
   telemetry_params.dummy6 = 0;
   telemetry_params.bControllerHasInputTelemetry = false;
   telemetry_params.bControllerHasOutputTelemetry = false;
   telemetry_params.controller_telemetry_type = 0;

   telemetry_params.update_rate = DEFAULT_TELEMETRY_SEND_RATE;
   telemetry_params.vehicle_mavlink_id = DEFAULT_MAVLINK_SYS_ID_VEHICLE;
   telemetry_params.controller_mavlink_id = DEFAULT_MAVLINK_SYS_ID_CONTROLLER;

   telemetry_params.flags = TELEMETRY_FLAGS_RXTX | TELEMETRY_FLAGS_REQUEST_DATA_STREAMS | TELEMETRY_FLAGS_SPECTATOR_ENABLE;
   telemetry_params.flags |= TELEMETRY_FLAGS_ALLOW_ANY_VEHICLE_SYSID;
   
   if ( 0 < hardwareInterfacesInfo.serial_port_count )
   {
      hardwareInterfacesInfo.serial_port_supported_and_usage[0] &= 0xFFFFFF00;
      hardwareInterfacesInfo.serial_port_supported_and_usage[0] |= SERIAL_PORT_USAGE_TELEMETRY_MAVLINK;
      hardwareInterfacesInfo.serial_port_speed[0] = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;

      if ( hardware_is_vehicle() )
      if ( hardware_get_serial_ports_count() > 0 )
      {
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(0);
         if ( NULL != pPortInfo )
         {
            pPortInfo->lPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;
            pPortInfo->iPortUsage = SERIAL_PORT_USAGE_TELEMETRY_MAVLINK;
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
   rc_params.iRCTranslationType = RC_TRANSLATION_TYPE_2000;
   for( unsigned int i=0; i<(sizeof(rc_params.rcDummy)/sizeof(rc_params.rcDummy[0])); i++ )
      rc_params.rcDummy[i] = 0;

   camera_rc_channels = (((u32)0x07)<<24) | (0x03 << 30); // set only relative speed to middle;
}


void Model::resetCameraToDefaults(int iCameraIndex)
{
   for( int k=0; k<MODEL_MAX_CAMERAS; k++ )
   {
      if ( (iCameraIndex != -1) && (iCameraIndex != k) )
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

         if ( hardware_isCameraVeye() && (k == 0) )
         {
            camera_params[k].profiles[i].brightness = 47;
            camera_params[k].profiles[i].contrast = 50;
            camera_params[k].profiles[i].saturation = 80;
            camera_params[k].profiles[i].sharpness = 110; // 100 is zero
            camera_params[k].profiles[i].whitebalance = 1; //auto
            camera_params[k].profiles[i].shutterspeed = 0; //auto
            camera_params[k].profiles[i].hue = 40;
            if ( (hardware_getCameraType() == CAMERA_TYPE_VEYE307) || (hardware_getCameraType() == CAMERA_TYPE_VEYE290) )
               camera_params[k].profiles[i].drc = 0;
            if ( hardware_getCameraType() == CAMERA_TYPE_VEYE327 )
               camera_params[k].profiles[i].drc = 0x0C;
         }
      }
      // HDMI profile defaults:

      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].whitebalance = 0; // off
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].exposure = 7; // off

      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].brightness = 50;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].contrast = 50;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].saturation = 60;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].sharpness = 100;
      if ( hardware_isCameraVeye() )
         camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].hue = 40;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].analogGain = 2.0;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].awbGainB = 1.5;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].awbGainR = 1.4;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].vstab = 0;
      camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].drc = 0;
      if ( hardware_getCameraType() == CAMERA_TYPE_VEYE327 )
         camera_params[k].profiles[MODEL_CAMERA_PROFILES-1].drc = 0x0C;
   }
}

void Model::resetCameraProfileToDefaults(camera_profile_parameters_t* pCamParams)
{
   pCamParams->uFlags = CAMERA_FLAG_OPENIPC_3A_SIGMASTAR;
   pCamParams->flip_image = 0;
   pCamParams->brightness = 47;
   pCamParams->contrast = 50;
   pCamParams->saturation = 80;
   pCamParams->hue = 50;
   pCamParams->sharpness = 110;
   pCamParams->exposure = 3; // sports   2; //backlight
   pCamParams->shutterspeed = 0; // auto
   if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
     pCamParams->shutterspeed = 8; //milisec

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
   pCamParams->dayNightMode = 0; // day mode
   for( int i=0; i<(int)(sizeof(pCamParams->dummyCamP)/sizeof(pCamParams->dummyCamP[0])); i++ )
      pCamParams->dummyCamP[i] = 0;
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


u32 Model::getRadioLinkVideoDataRateBSP(int iLinkId)
{
   if ( (iLinkId < 0) || (iLinkId >= radioLinksParams.links_count) )
      return 0;
   bool bUsesHT40 = false;
   if ( radioLinksParams.link_radio_flags[iLinkId] & RADIO_FLAG_HT40_VEHICLE )
      bUsesHT40 = true;

   return getRealDataRateFromRadioDataRate(radioLinksParams.link_datarate_video_bps[iLinkId], bUsesHT40);
}

int Model::getRadioLinkDownlinkDataRate(int iLinkId)
{
   if ( (iLinkId < 0) || (iLinkId >= radioLinksParams.links_count) )
      return 0;

   bool bIsAtheros = false;
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      if ( radioInterfacesParams.interface_link_id[i] != iLinkId )
         continue;
      if ( hardware_radio_driver_is_atheros_card((radioInterfacesParams.interface_radiotype_and_driver[i] >> 8) & 0xFF) )
      {
         bIsAtheros = true;
         break;
      }
   }

   if ( bIsAtheros )
      return radioLinksParams.link_datarate_video_bps[iLinkId];

   switch ( radioLinksParams.uDownlinkDataDataRateType[iLinkId] )
   {
      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_FIXED:
         return radioLinksParams.link_datarate_data_bps[iLinkId];
         break;

      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO:
         return radioLinksParams.link_datarate_video_bps[iLinkId];
         break;

      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST:
      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO:
      default:
         if ( radioLinksParams.link_datarate_video_bps[iLinkId] > 0 )
            return DEFAULT_RADIO_DATARATE_LOWEST;
         else
            return -1;
         break;
   }
   return 0;
}

int Model::getRadioLinkUplinkDataRate(int iLinkId)
{
   if ( (iLinkId < 0) || (iLinkId >= radioLinksParams.links_count) )
      return 0;

   switch ( radioLinksParams.uUplinkDataDataRateType[iLinkId] )
   {
      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_FIXED:
         return radioLinksParams.uplink_datarate_data_bps[iLinkId];
         break;

      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO:
         return radioLinksParams.link_datarate_video_bps[iLinkId];
         break;

      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST:
      case FLAG_RADIO_LINK_DATARATE_DATA_TYPE_AUTO:
      default:
         if ( radioLinksParams.link_datarate_video_bps[iLinkId] > 0 )
            return DEFAULT_RADIO_DATARATE_LOWEST;
         else
            return -1;
         break;
   }
   return 0;
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


bool Model::canSwapEnabledHighCapacityRadioInterfaces()
{
   bool bCanSwap = false;

   int bInterfacesToSwap[MAX_RADIO_INTERFACES];
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      bInterfacesToSwap[i] = false;

   if ( radioInterfacesParams.interfaces_count > 1 )
   if ( radioLinksParams.links_count > 1 )
   {
      for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      for( int k=i+1; k<radioInterfacesParams.interfaces_count; k++ )
      {
         int iRadioLink1 = radioInterfacesParams.interface_link_id[i];
         int iRadioLink2 = radioInterfacesParams.interface_link_id[k];
         bool bLink1IsHighCapacity = false;
         bool bLink2IsHighCapacity = false;
         if ( (iRadioLink1 >= 0) && (iRadioLink1 < radioLinksParams.links_count) )
         if ( radioLinksParams.link_capabilities_flags[iRadioLink1] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
            bLink1IsHighCapacity = true;
         if ( (iRadioLink2 >= 0) && (iRadioLink2 < radioLinksParams.links_count) )
         if ( radioLinksParams.link_capabilities_flags[iRadioLink2] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
            bLink2IsHighCapacity = true;
         if ( radioInterfacesParams.interface_supported_bands[i] == radioInterfacesParams.interface_supported_bands[k] )
         if ( bLink1IsHighCapacity && bLink2IsHighCapacity )
         {
            bCanSwap = true;
            bInterfacesToSwap[i] = true;
            bInterfacesToSwap[k] = true;
         }
      }
   }

   if ( ! bCanSwap )
   {
      log_line("Model: Can swap high capacity radio interfaces: no.");
      return false;
   }

   if ( hardware_is_vehicle() )
   {
      int iCountInterfacesToSwap = 0;
      for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      {
         if ( ! bInterfacesToSwap[i] )
            continue;
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         if ( NULL == pRadioHWInfo )
         {
            bCanSwap = false;
            break;
         }
         if ( ! pRadioHWInfo->isHighCapacityInterface )
         {
            bCanSwap = false;
            break;
         }
         iCountInterfacesToSwap++;
      }

      if ( iCountInterfacesToSwap < 2 )
         bCanSwap = false;

      if ( ! bCanSwap )
      {
         log_line("Model: Can swap high capacity radio interfaces: no.");
         return false;
      }
      int iInterfaceIndex1 = -1;
      int iInterfaceIndex2 = -1;

      for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      {
         if ( ! bInterfacesToSwap[i] )
            continue;

         if ( -1 == iInterfaceIndex1 )
         {
            iInterfaceIndex1 = i;
            bInterfacesToSwap[i] = false;
         }
         else if ( -1 == iInterfaceIndex2 )
         {
            iInterfaceIndex2 = i;
            bInterfacesToSwap[i] = false;
         }
      }

      if ( (-1 == iInterfaceIndex1) || (-1 == iInterfaceIndex2) )
      {
         log_line("Model: Can swap high capacity radio interfaces: no.");
         return false;
      }
   }

   log_line("Model: Can swap high capacity radio interfaces: %s.", bCanSwap?"yes":"no");
   return bCanSwap;
}

static int sl_iLastSwappedModelRadioInterface1 = -1;
static int sl_iLastSwappedModelRadioInterface2 = -1;

bool Model::swapEnabledHighCapacityRadioInterfaces()
{
   // Check to see if it's possible

   if ( ! canSwapEnabledHighCapacityRadioInterfaces() )
      return false;

   bool bCanSwap = false;
   int bInterfacesToSwap[MAX_RADIO_INTERFACES];
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      bInterfacesToSwap[i] = false;

   if ( radioInterfacesParams.interfaces_count > 1 )
   if ( radioLinksParams.links_count > 1 )
   {
      for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
      for( int k=i+1; k<radioInterfacesParams.interfaces_count; k++ )
      {
         int iRadioLink1 = radioInterfacesParams.interface_link_id[i];
         int iRadioLink2 = radioInterfacesParams.interface_link_id[k];
         bool bLink1IsHighCapacity = false;
         bool bLink2IsHighCapacity = false;
         if ( (iRadioLink1 >= 0) && (iRadioLink1 < radioLinksParams.links_count) )
         if ( radioLinksParams.link_capabilities_flags[iRadioLink1] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
            bLink1IsHighCapacity = true;
         if ( (iRadioLink2 >= 0) && (iRadioLink2 < radioLinksParams.links_count) )
         if ( radioLinksParams.link_capabilities_flags[iRadioLink2] & RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY )
            bLink2IsHighCapacity = true;
         if ( radioInterfacesParams.interface_supported_bands[i] == radioInterfacesParams.interface_supported_bands[k] )
         if ( bLink1IsHighCapacity && bLink2IsHighCapacity )
         {
            bCanSwap = true;
            bInterfacesToSwap[i] = true;
            bInterfacesToSwap[k] = true;
         }
      }
   }

   if ( ! bCanSwap )
   {
      log_softerror_and_alarm("[Model] Swap: Can't do swap as radio interfaces do not match");
      return false;
   }
   int iInterfaceIndex1 = -1;
   int iInterfaceIndex2 = -1;

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      if ( ! bInterfacesToSwap[i] )
         continue;

      if ( -1 == iInterfaceIndex1 )
      {
         iInterfaceIndex1 = i;
         bInterfacesToSwap[i] = false;
      }
      else if ( -1 == iInterfaceIndex2 )
      {
         iInterfaceIndex2 = i;
         bInterfacesToSwap[i] = false;
      }
   }

   if ( (-1 == iInterfaceIndex1) || (-1 == iInterfaceIndex2) )
   {
      log_softerror_and_alarm("[Model] Swap: Can't do swap as radio interfaces not found.");
      return false;
   }

   int iRadioLinkForInterface1 = radioInterfacesParams.interface_link_id[iInterfaceIndex1];
   int iRadioLinkForInterface2 = radioInterfacesParams.interface_link_id[iInterfaceIndex2];
   
   if ( (iRadioLinkForInterface1 < 0) || (iRadioLinkForInterface1 >= radioLinksParams.links_count) )
   {
      log_softerror_and_alarm("[Model] Swap: Can't do swap as radio interface %d has no radio link assigned.", iRadioLinkForInterface1);
      return false;
   }
   if ( (iRadioLinkForInterface2 < 0) || (iRadioLinkForInterface2 >= radioLinksParams.links_count) )
   {
      log_softerror_and_alarm("[Model] Swap: Can't do swap as radio interface %d has no radio link assigned.", iRadioLinkForInterface2);
      return false;
   }

   radio_hw_info_t* pRadioHWInfo1 = NULL;
   radio_hw_info_t* pRadioHWInfo2 = NULL;
   
   if ( hardware_is_vehicle() )
   {
      pRadioHWInfo1 = hardware_get_radio_info(iInterfaceIndex1);
      pRadioHWInfo2 = hardware_get_radio_info(iInterfaceIndex2);
      if ( (NULL == pRadioHWInfo1) || (NULL == pRadioHWInfo2) )
      {
         log_softerror_and_alarm("[Model] Swap: Can't do swap as radio interface hardware info is NULL");
         return false;
      }
   }

   log_line("[Model] Swapping radio interfaces %d (radio link %d) and %d (radio link %d)...",
      iInterfaceIndex1+1, iRadioLinkForInterface1+1, iInterfaceIndex2+1, iRadioLinkForInterface2+1);

   if ( hardware_is_vehicle() )
   if ( ! hardware_radio_supports_frequency(pRadioHWInfo1, radioLinksParams.link_frequency_khz[iRadioLinkForInterface2]) )
   {
      log_softerror_and_alarm("[Model] Swap: Radio interface %d does not support radio link %d frequency: %s",
         iInterfaceIndex1+1, iRadioLinkForInterface2, str_format_frequency(radioLinksParams.link_frequency_khz[iRadioLinkForInterface2]));
      return false;
   }
   if ( hardware_is_vehicle() )
   if ( ! hardware_radio_supports_frequency(pRadioHWInfo2, radioLinksParams.link_frequency_khz[iRadioLinkForInterface1]) )
   {
      log_softerror_and_alarm("[Model] Swap: Radio interface %d does not support radio link %d frequency: %s",
         iInterfaceIndex2+1, iRadioLinkForInterface1, str_format_frequency(radioLinksParams.link_frequency_khz[iRadioLinkForInterface1]));
      return false;
   }

   // Reasign radio links to radio interfaces
   radioInterfacesParams.interface_link_id[iInterfaceIndex1] = iRadioLinkForInterface2;
   radioInterfacesParams.interface_link_id[iInterfaceIndex2] = iRadioLinkForInterface1;

   // Swap relay flag on radio interfaces (not on radio links, radio links do not change)
   u32 uCapabRelay1 = radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex1] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
   u32 uCapabRelay2 = radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex2] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
   
   radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex1] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
   radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex1] |= uCapabRelay2;

   radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex2] &= (~RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY);
   radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex2] |= uCapabRelay1;
   
   // Swap radio interfaces frequency
   u32 uFreq = radioInterfacesParams.interface_current_frequency_khz[iInterfaceIndex1];
   radioInterfacesParams.interface_current_frequency_khz[iInterfaceIndex2] = radioInterfacesParams.interface_current_frequency_khz[iInterfaceIndex1];
   radioInterfacesParams.interface_current_frequency_khz[iInterfaceIndex1] = uFreq;

   sl_iLastSwappedModelRadioInterface1 = iInterfaceIndex1;
   sl_iLastSwappedModelRadioInterface2 = iInterfaceIndex2;
   
   int iTmp = iRadioLinkForInterface1;
   iRadioLinkForInterface1 = iRadioLinkForInterface2;
   iRadioLinkForInterface2 = iTmp;
   log_line("[Model] Swaped radio interface %d (now on radio link %d) and %d (now on radio link %d).",
      iInterfaceIndex1+1, iRadioLinkForInterface1+1, iInterfaceIndex2+1, iRadioLinkForInterface2+1);
   logVehicleRadioInfo();
   return true;
}

int Model::getLastSwappedRadioInterface1()
{
   return sl_iLastSwappedModelRadioInterface1;
}

int Model::getLastSwappedRadioInterface2()
{
   return sl_iLastSwappedModelRadioInterface2;
}

bool Model::rotateRadioLinksOrder()
{
   log_line("Model: Rotating model radio links (%d radio links)...", radioLinksParams.links_count);
   if ( radioLinksParams.links_count < 2 )
   {
      log_line("Model: Nothing to rotate, less than 2 radio links.");
      return false;
   }

   // Rotate assignment of radio interfaces to radio links

   for( int iInterface=0; iInterface<radioInterfacesParams.interfaces_count; iInterface++ )
   {
      if ( radioInterfacesParams.interface_link_id[iInterface] >= 0 )
      {
         radioInterfacesParams.interface_link_id[iInterface]++;
         if ( radioInterfacesParams.interface_link_id[iInterface] >= radioLinksParams.links_count )
            radioInterfacesParams.interface_link_id[iInterface] = 0;
      }
   }

   // Rotate radio links info

   type_radio_links_parameters oldRadioLinksParams;
   memcpy(&oldRadioLinksParams, &radioLinksParams, sizeof(type_radio_links_parameters));

   for( int iLink=0; iLink<radioLinksParams.links_count; iLink++ )
   {
      int iSourceIndex = iLink;
      int iDestIndex = iLink+1;
      if ( iDestIndex >= radioLinksParams.links_count )
         iDestIndex = 0;

      radioLinksParams.link_frequency_khz[iDestIndex] = oldRadioLinksParams.link_frequency_khz[iSourceIndex];
      radioLinksParams.link_capabilities_flags[iDestIndex] = oldRadioLinksParams.link_capabilities_flags[iSourceIndex];
      radioLinksParams.link_radio_flags[iDestIndex] = oldRadioLinksParams.link_radio_flags[iSourceIndex];
      radioLinksParams.link_datarate_video_bps[iDestIndex] = oldRadioLinksParams.link_datarate_video_bps[iSourceIndex];
      radioLinksParams.link_datarate_data_bps[iDestIndex] = oldRadioLinksParams.link_datarate_data_bps[iSourceIndex];

      radioLinksParams.uSerialPacketSize[iDestIndex] = oldRadioLinksParams.uSerialPacketSize[iSourceIndex];
      radioLinksParams.uDummy2[iDestIndex] = oldRadioLinksParams.uDummy2[iSourceIndex];
      radioLinksParams.uplink_datarate_video_bps[iDestIndex] = oldRadioLinksParams.uplink_datarate_video_bps[iSourceIndex];
      radioLinksParams.uplink_datarate_data_bps[iDestIndex] = oldRadioLinksParams.uplink_datarate_data_bps[iSourceIndex];
      radioLinksParams.uUplinkDataDataRateType[iDestIndex] = oldRadioLinksParams.uUplinkDataDataRateType[iSourceIndex];
      radioLinksParams.uDownlinkDataDataRateType[iDestIndex] = oldRadioLinksParams.uDownlinkDataDataRateType[iSourceIndex];
   }

   log_line("Model: Rotated %d radio links.", radioLinksParams.links_count);
   return true;
}

bool Model::radioInterfaceIsWiFiRadio(int iRadioInterfaceIndex)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= radioInterfacesParams.interfaces_count) )
      return false;
   if ( hardware_radio_type_is_ieee(radioInterfacesParams.interface_radiotype_and_driver[iRadioInterfaceIndex] & 0xFF) )
      return true;
   return false;
}

bool Model::radioLinkIsWiFiRadio(int iRadioLinkIndex)
{
   if ( (iRadioLinkIndex < 0) || (iRadioLinkIndex >= radioLinksParams.links_count) )
      return false;

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      if ( radioInterfacesParams.interface_link_id[i] != iRadioLinkIndex )
         continue;

      if ( hardware_radio_type_is_ieee(radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) )
         return true;
      else
         return false;
   }
   return false;
}


bool Model::radioLinkIsSiKRadio(int iRadioLinkIndex)
{
   if ( iRadioLinkIndex < 0 || iRadioLinkIndex >= radioLinksParams.links_count )
      return false;

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      if ( radioInterfacesParams.interface_link_id[i] != iRadioLinkIndex )
         continue;

      if ( (radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) == RADIO_TYPE_SIK )
         return true;
      else
         return false;
   }
   return false;
}

bool Model::radioLinkIsELRSRadio(int iRadioLinkIndex)
{
   if ( iRadioLinkIndex < 0 || iRadioLinkIndex >= radioLinksParams.links_count )
      return false;

   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
      if ( radioInterfacesParams.interface_link_id[i] != iRadioLinkIndex )
         continue;

      if ( ( (radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) == RADIO_TYPE_SERIAL ) &&
           ( radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_SERIAL_LINK_ELRS) )
         return true;
      else
         return false;
   }
   return false;
}

int Model::hasRadioCardsRTL8812AU()
{
   int iCount = 0;
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
       if ( hardware_radio_driver_is_rtl8812au_card((radioInterfacesParams.interface_radiotype_and_driver[i] >> 8) & 0xFF) )
          iCount++;
   }
   return iCount; 
}

int Model::hasRadioCardsRTL8812EU()
{
   int iCount = 0;
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
       if ( hardware_radio_driver_is_rtl8812eu_card((radioInterfacesParams.interface_radiotype_and_driver[i] >> 8) & 0xFF) )
          iCount++;
   }
   return iCount;
}

int Model::hasRadioCardsAtheros()
{
   int iCount = 0;
   for( int i=0; i<radioInterfacesParams.interfaces_count; i++ )
   {
       if ( hardware_radio_driver_is_atheros_card((radioInterfacesParams.interface_radiotype_and_driver[i] >> 8) & 0xFF) )
          iCount++;
   }
   return iCount;
}


bool Model::hasCamera()
{
   if ( iCameraCount <= 0 )
      return false;

   if ( (iCurrentCamera >= 0) && (iCurrentCamera < iCameraCount) )
   if ( (camera_params[iCurrentCamera].iCameraType == CAMERA_TYPE_NONE) && (camera_params[iCurrentCamera].iForcedCameraType == CAMERA_TYPE_NONE) )
      return false;

   return true;
}

char* Model::getCameraName(int iCameraIndex)
{
   if ( (iCameraIndex < 0) || (iCameraIndex >= iCameraCount) )
      return NULL;
   return camera_params[iCameraIndex].szCameraName;
}

// Returns true if it was updated
bool Model::setCameraName(int iCameraIndex, const char* szCamName)
{
   if ( (iCameraIndex < 0) || (iCameraIndex >= iCameraCount) )
      return false;

   bool bReturn = false;
   if ( (NULL == szCamName) || (0 == szCamName[0]) )
   {
      if ( 0 != camera_params[iCameraIndex].szCameraName[0] )
         bReturn = true;
      camera_params[iCameraIndex].szCameraName[0] = 0;
      return bReturn;
   }
   if ( 0 != strcmp(camera_params[iCameraIndex].szCameraName, szCamName) )
      bReturn = true;
   strncpy(camera_params[iCameraIndex].szCameraName, szCamName, MAX_CAMERA_NAME_LENGTH-1);
   camera_params[iCameraIndex].szCameraName[MAX_CAMERA_NAME_LENGTH-1] = 0;

   for( int i=0; i<(int)strlen(camera_params[iCameraIndex].szCameraName); i++ )
   {
      if (camera_params[iCameraIndex].szCameraName[i] < 32 )
      {
         camera_params[iCameraIndex].szCameraName[i] = 0;
         break;
      }
   }

   log_line("Did set camera index %d name to: [%s]", iCameraIndex, szCamName);
   return bReturn;
}

u32 Model::getActiveCameraType()
{
   if ( iCameraCount <= 0 )
      return CAMERA_TYPE_NONE;

   if ( (iCurrentCamera < 0) || (iCurrentCamera >= iCameraCount) )
      return CAMERA_TYPE_NONE;

   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
      return camera_params[iCurrentCamera].iForcedCameraType;
   return camera_params[iCurrentCamera].iCameraType;
}

bool Model::isActiveCameraHDMI()
{
   if ( iCameraCount <= 0 )
      return false;

   if ( (iCurrentCamera < 0) || (iCurrentCamera >= iCameraCount) )
      return false;
    
   int iCameraType = camera_params[iCurrentCamera].iCameraType;
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
      iCameraType = camera_params[iCurrentCamera].iForcedCameraType;

   if ( iCameraType == CAMERA_TYPE_HDMI )
      return true;
   return false;
}

bool Model::isActiveCameraVeye()
{
   if ( iCameraCount <= 0 )
      return false;

   if ( (iCurrentCamera < 0) || (iCurrentCamera >= iCameraCount) )
      return false;

   int iCameraType = camera_params[iCurrentCamera].iCameraType;
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
      iCameraType = camera_params[iCurrentCamera].iForcedCameraType;

   if ( iCameraType == CAMERA_TYPE_VEYE290 ||
        iCameraType == CAMERA_TYPE_VEYE307 ||
        iCameraType == CAMERA_TYPE_VEYE327 )
      return true;
   return false;
}

bool Model::isActiveCameraVeye307()
{
   if ( iCameraCount <= 0 )
      return false;

   if ( (iCurrentCamera < 0) || (iCurrentCamera >= iCameraCount) )
      return false;

   int iCameraType = camera_params[iCurrentCamera].iCameraType;
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
      iCameraType = camera_params[iCurrentCamera].iForcedCameraType;

   if ( iCameraType == CAMERA_TYPE_VEYE307 )
      return true;
   return false;
}

bool Model::isActiveCameraVeye327290()
{
   if ( iCameraCount <= 0 )
      return false;

   if ( (iCurrentCamera < 0) || (iCurrentCamera >= iCameraCount) )
      return false;

   int iCameraType = camera_params[iCurrentCamera].iCameraType;
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
      iCameraType = camera_params[iCurrentCamera].iForcedCameraType;

   if ( iCameraType == CAMERA_TYPE_VEYE290 ||
        iCameraType == CAMERA_TYPE_VEYE327 )
      return true;
   return false;
}

bool Model::isActiveCameraCSICompatible()
{
   if ( iCameraCount <= 0 )
      return false;

   if ( (iCurrentCamera < 0) || (iCurrentCamera >= iCameraCount) )
      return false;

   int iCameraType = camera_params[iCurrentCamera].iCameraType;
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
      iCameraType = camera_params[iCurrentCamera].iForcedCameraType;
   if ( iCameraType == CAMERA_TYPE_CSI ||
        iCameraType == CAMERA_TYPE_HDMI )
      return true;
   return false;
}


bool Model::isActiveCameraCSI()
{
   if ( iCameraCount <= 0 )
      return false;

   if ( (iCurrentCamera < 0) || (iCurrentCamera >= iCameraCount) )
      return false;

   int iCameraType = camera_params[iCurrentCamera].iCameraType;
   if ( 0 != camera_params[iCurrentCamera].iForcedCameraType )
      iCameraType = camera_params[iCurrentCamera].iForcedCameraType;
   if ( iCameraType == CAMERA_TYPE_CSI )
      return true;
   return false;
}

bool Model::isActiveCameraOpenIPC()
{
   if ( iCameraCount <= 0 )
      return false;

   if ( (iCurrentCamera < 0) || (iCurrentCamera >= iCameraCount) )
      return false;
   
   if ( isRunningOnOpenIPCHardware() )
      return true;
   return false;
}

void Model::log_camera_profiles_differences(camera_profile_parameters_t* pCamProfile1, camera_profile_parameters_t* pCamProfile2, int iIndex1, int iIndex2)
{
   if ( (NULL == pCamProfile1) || (NULL == pCamProfile2) )
      return;

   log_line("* Camera Profile %d and Camera Profile %d differences:", iIndex1, iIndex2);
   if ( pCamProfile1 == pCamProfile2 )
   {
      log_line("* The two camera profiles are the same object.");
      return;
   }
   if ( iIndex1 == iIndex2 )
      log_line("* Cam profile index 1 is the same as cam profile index 2");

   if ( pCamProfile1->uFlags != pCamProfile2->uFlags )
      log_line(" * Cam flags is different: %u - %u", pCamProfile1->uFlags, pCamProfile2->uFlags);
   if ( pCamProfile1->flip_image != pCamProfile2->flip_image )
      log_line(" * Cam flip image is different: %u - %u", pCamProfile1->flip_image, pCamProfile2->flip_image);
   if ( pCamProfile1->brightness != pCamProfile2->brightness )
      log_line(" * Cam brightness is different: %u - %u", pCamProfile1->brightness, pCamProfile2->brightness);
   if ( pCamProfile1->contrast != pCamProfile2->contrast )
      log_line(" * Cam contrast is different: %u - %u", pCamProfile1->contrast, pCamProfile2->contrast);
   if ( pCamProfile1->saturation != pCamProfile2->saturation )
      log_line(" * Cam saturation is different: %u - %u", pCamProfile1->saturation, pCamProfile2->saturation);
   if ( pCamProfile1->sharpness != pCamProfile2->sharpness )
      log_line(" * Cam sharpness is different: %u - %u", pCamProfile1->sharpness, pCamProfile2->sharpness);
   if ( pCamProfile1->exposure != pCamProfile2->exposure )
      log_line(" * Cam exposure is different: %u - %u", pCamProfile1->exposure, pCamProfile2->exposure);
   if ( pCamProfile1->shutterspeed != pCamProfile2->shutterspeed )
      log_line(" * Cam shutterspeed is different: %u - %u", pCamProfile1->shutterspeed, pCamProfile2->shutterspeed);
   if ( pCamProfile1->whitebalance != pCamProfile2->whitebalance )
      log_line(" * Cam whitebalance is different: %u - %u", pCamProfile1->whitebalance, pCamProfile2->whitebalance);
   if ( pCamProfile1->metering != pCamProfile2->metering )
      log_line(" * Cam metering is different: %u - %u", pCamProfile1->metering, pCamProfile2->metering);
   if ( pCamProfile1->drc != pCamProfile2->drc )
      log_line(" * Cam drc is different: %u - %u", pCamProfile1->drc, pCamProfile2->drc);
   if ( pCamProfile1->vstab != pCamProfile2->vstab )
      log_line(" * Cam vstab is different: %u - %u", pCamProfile1->vstab, pCamProfile2->vstab);
   if ( pCamProfile1->ev != pCamProfile2->ev )
      log_line(" * Cam ev is different: %u - %u", pCamProfile1->ev, pCamProfile2->ev);
   if ( pCamProfile1->iso != pCamProfile2->iso )
      log_line(" * Cam iso is different: %u - %u", pCamProfile1->iso, pCamProfile2->iso);
   if ( pCamProfile1->shutterspeed != pCamProfile2->shutterspeed )
      log_line(" * Cam shutterspeed is different: %u - %u", pCamProfile1->shutterspeed, pCamProfile2->shutterspeed);
   if ( pCamProfile1->wdr != pCamProfile2->wdr )
      log_line(" * Cam wdr is different: %u - %u", pCamProfile1->wdr, pCamProfile2->wdr);
   if ( pCamProfile1->dayNightMode != pCamProfile2->dayNightMode )
      log_line(" * Cam dayNightMode is different: %u - %u", pCamProfile1->dayNightMode, pCamProfile2->dayNightMode);
   if ( pCamProfile1->dummyCamP[0] != pCamProfile2->dummyCamP[0] )
      log_line(" * Cam dummy[0] is different: %u - %u", pCamProfile1->dummyCamP[0], pCamProfile2->dummyCamP[0]);

   if ( fabsf(pCamProfile1->analogGain - pCamProfile2->analogGain) > 0.000001 )
      log_line(" * Cam analogGain is different: %f - %f", pCamProfile1->analogGain, pCamProfile2->analogGain);
   if ( fabsf(pCamProfile1->awbGainB - pCamProfile2->awbGainB) > 0.000001 )
      log_line(" * Cam awbGainB is different: %f - %f", pCamProfile1->awbGainB, pCamProfile2->awbGainB);
   if ( fabsf(pCamProfile1->awbGainR - pCamProfile2->awbGainR) > 0.000001 )
      log_line(" * Cam awbGainR is different: %f - %f", pCamProfile1->awbGainR, pCamProfile2->awbGainR);
   if ( fabsf(pCamProfile1->fovV - pCamProfile2->fovV) > 0.000001 )
      log_line(" * Cam fovV is different: %f - %f", pCamProfile1->fovV, pCamProfile2->fovV);
   if ( fabsf(pCamProfile1->fovH - pCamProfile2->fovH) > 0.000001 )
      log_line(" * Cam fovH is different: %f - %f", pCamProfile1->fovH, pCamProfile2->fovH);
}

bool Model::isVideoLinkFixedOneWay()
{
   if ( video_link_profiles[video_params.user_selected_video_link_profile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ONE_WAY_FIXED_VIDEO )
      return true;

   if ( radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY )
      return true;

   return false;
}

int Model::getInitialKeyframeIntervalMs(int iVideoProfile)
{
   int iKeyframeMs = video_link_profiles[iVideoProfile].keyframe_ms;

   //if ( ! isVideoLinkFixedOneWay() )
   //if ( video_link_profiles[iVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_ENABLE_ADAPTIVE_VIDEO_KEYFRAME )
   //   iKeyframeMs = DEFAULT_VIDEO_AUTO_INITIAL_KEYFRAME_INTERVAL;
   
   if ( hardware_board_is_goke(hardware_getBoardType()) )
       iKeyframeMs = DEFAULT_VIDEO_KEYFRAME_OIPC_GOKE;

   if ( iKeyframeMs < 0 )
      iKeyframeMs = -iKeyframeMs;
   return iKeyframeMs;
}

void Model::setDefaultVideoBitrate()
{
   u32 board_type = hardware_getBoardType();

   video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;
   video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = DEFAULT_HP_VIDEO_BITRATE;
   video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE;

   if ( ((board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO) ||
        ((board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PIZEROW) ||
        hardware_board_is_goke(board_type) )
   {
      video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_PI_ZERO;
      video_link_profiles[VIDEO_PROFILE_BEST_PERF].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_PI_ZERO;
      video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_PI_ZERO;
   }
   if ( hardware_board_is_sigmastar(hardware_getOnlyBoardType()) )
   {
      video_link_profiles[VIDEO_PROFILE_HIGH_QUALITY].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_OPIC_SIGMASTAR;
      video_link_profiles[VIDEO_PROFILE_USER].bitrate_fixed_bps = DEFAULT_VIDEO_BITRATE_OPIC_SIGMASTAR;
   }

   // Lower video bitrate on all video profiles if running on a single core CPU
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( ((board_type & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE200) || ((board_type & BOARD_TYPE_MASK) == BOARD_TYPE_OPENIPC_GOKE210) ||
           ((board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PIZERO) || ((board_type & BOARD_TYPE_MASK) == BOARD_TYPE_PIZEROW) || ((board_type & BOARD_TYPE_MASK) == BOARD_TYPE_NONE) )
      {
         if ( video_link_profiles[i].bitrate_fixed_bps > 4500000 )
            video_link_profiles[i].bitrate_fixed_bps -= 1000000;
         else if ( video_link_profiles[i].bitrate_fixed_bps > 3000000 )
            video_link_profiles[i].bitrate_fixed_bps -= 500000;
         log_line("Model: Lowered video bitrate for video profile %d (single core CPU) to %u", i, video_link_profiles[i].bitrate_fixed_bps);
      }
   }
}


void Model::getCameraFlags(char* szCameraFlags)
{
   if ( NULL == szCameraFlags )
      return;
   szCameraFlags[0] = 0;
   if ( iCameraCount <= 0 )
      return;

   if ( (iCurrentCamera < 0) || (iCurrentCamera >= iCameraCount) )
      return;

   camera_profile_parameters_t* pParams = &(camera_params[iCurrentCamera].profiles[camera_params[iCurrentCamera].iCurrentProfile]);
   sprintf(szCameraFlags, "-br %d -co %d -sa %d -sh %d", pParams->brightness, pParams->contrast - 50, pParams->saturation - 100, pParams->sharpness-100);

   if ( ! isActiveCameraVeye() )
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

   if ( ! isActiveCameraVeye() )
   {
      switch ( pParams->drc )
      {
         case 0: strcat(szCameraFlags, " -drc off"); break;
         case 1: strcat(szCameraFlags, " -drc low"); break;
         case 2: strcat(szCameraFlags, " -drc med"); break;
         case 3: strcat(szCameraFlags, " -drc high"); break;
      }
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
}

// To fix: overwrites checks
void Model::getVideoFlags(char* szVideoFlags, int iVideoProfile)
{
   if ( NULL == szVideoFlags )
      return;

   szVideoFlags[0] = 0;
   
   if ( iCameraCount <= 0 )
      return;

   if ( (iCurrentCamera < 0) || (iCurrentCamera >= iCameraCount) )
      return;

   camera_profile_parameters_t* pCamParams = &(camera_params[iCurrentCamera].profiles[camera_params[iCurrentCamera].iCurrentProfile]);

   char szBuff[128];
   u32 uBitrate = DEFAULT_VIDEO_BITRATE;
   // To fix
   //if ( NULL == pVideoOverwrites )
      uBitrate = video_link_profiles[iVideoProfile].bitrate_fixed_bps;
   //else
   //   uBitrate = video_link_profiles[pVideoOverwrites->currentVideoLinkProfile].bitrate_fixed_bps;

   if ( uBitrate > 6000000 )
      uBitrate = uBitrate*4/5;

   sprintf(szBuff, "-b %u", uBitrate);

   char szKeyFrame[64];

   int iKeyframeMs = getInitialKeyframeIntervalMs(iVideoProfile);
   int iKeyframeFramesCount = (video_link_profiles[iVideoProfile].fps * iKeyframeMs) / 1000;
   sprintf(szKeyFrame, "%d", iKeyframeFramesCount);


   if ( isActiveCameraVeye() )
   {
      int fps = video_link_profiles[iVideoProfile].fps;
      int width = video_link_profiles[iVideoProfile].width;
      int height = video_link_profiles[iVideoProfile].height;

      if ( isActiveCameraVeye307() )
      {
         // To fix: check if for IMX307 this is how to set resolution
         if ( video_link_profiles[iVideoProfile].width > 1280 )
            sprintf(szVideoFlags, "-cd H264 -fl -md 0 -fps %d -g %s %s", fps, szKeyFrame, szBuff);
         else
            sprintf(szVideoFlags, "-cd H264 -fl -md 1 -fps %d -g %s %s", fps, szKeyFrame, szBuff);

         //sprintf(szVideoFlags, "-cd H264 -fl -w %d -h %d -fps %d -g %s %s", video_link_profiles[iVideoProfile].width, video_link_profiles[iVideoProfile].height, video_link_profiles[iVideoProfile].fps, szKeyFrame, szBuff);
      }
      else
      {
         if ( video_link_profiles[iVideoProfile].width > 1280 )
            sprintf(szVideoFlags, "-cd H264 -fl -w %d -h %d -fps %d -g %s %s", width, height, fps, szKeyFrame, szBuff);
         else
            sprintf(szVideoFlags, "-cd H264 -fl -w %d -h %d -fps %d -g %s %s", width, height, fps, szKeyFrame, szBuff);
      }
   }
   else
   {
      sprintf(szVideoFlags, "-cd H264 -fl -w %d -h %d -fps %d -g %s %s", video_link_profiles[iVideoProfile].width, video_link_profiles[iVideoProfile].height, video_link_profiles[iVideoProfile].fps, szKeyFrame, szBuff);
      //sprintf(szVideoFlags, "-cd MJPEG -fl -w %d -h %d -fps %d -g %s %s", video_params.width, video_params.height, video_params.fps, szKeyFrame, szBuff);
   }

   if ( ! (video_params.uVideoExtraFlags & VIDEO_FLAG_ENABLE_LOCAL_HDMI_OUTPUT) )
   {
      strcpy(szBuff, "-n ");
      strcat(szBuff, szVideoFlags);
      strcpy(szVideoFlags, szBuff);
   }
   
   if ( video_link_profiles[iVideoProfile].h264quantization > 5 )
   {
      sprintf(szBuff, " -qp %d", video_link_profiles[iVideoProfile].h264quantization);
      strcat(szVideoFlags, szBuff);
   }

   if ( video_params.iInsertPPSVideoFrames )
      strcat(szVideoFlags, " -ih");

   int iSlices = camera_get_active_camera_h264_slices(this);
   if ( iSlices > 1 )
   {
      sprintf(szBuff, " -sl %d", iSlices );
      strcat(szVideoFlags, szBuff);
   }

   if ( video_params.iInsertSPTVideoFramesTimings )
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

   if ( pCamParams->uFlags & CAMERA_FLAG_FORCE_MODE_1 )
      strcat(szVideoFlags, " -md 1");
}

void Model::populateVehicleTelemetryData_v4(t_packet_header_ruby_telemetry_extended_v4* pPHRTE)
{
   if ( NULL == pPHRTE )
      return;
   if ( (0 == radioLinksParams.links_count) || (radioLinksParams.link_frequency_khz[0] == 0) )
      populateRadioInterfacesInfoFromHardware();

   pPHRTE->vehicle_name[0] = 0;
   strncpy( (char*)pPHRTE->vehicle_name, vehicle_name, MAX_VEHICLE_NAME_LENGTH-1);
   pPHRTE->vehicle_name[MAX_VEHICLE_NAME_LENGTH-1] = 0;
   pPHRTE->uVehicleId = uVehicleId;
   pPHRTE->vehicle_type = vehicle_type;
   pPHRTE->radio_links_count = radioLinksParams.links_count;
   pPHRTE->uRelayLinks = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pPHRTE->uRadioFrequenciesKhz[i] = radioLinksParams.link_frequency_khz[i];
      
      if ( (radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY) ||
           (relay_params.isRelayEnabledOnRadioLinkId == i) )
      {
         if ( relay_params.uRelayFrequencyKhz != 0 )
            pPHRTE->uRadioFrequenciesKhz[i] = relay_params.uRelayFrequencyKhz;
         pPHRTE->uRelayLinks |= (1<<i);
      }
   }
   if ( telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE )
      pPHRTE->flags |= FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;
   else
      pPHRTE->flags &= ~FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;
}

void Model::populateFromVehicleTelemetryData_v3(t_packet_header_ruby_telemetry_extended_v3* pPHRTE)
{
   uVehicleId = pPHRTE->uVehicleId;
   strncpy(vehicle_name, (char*)pPHRTE->vehicle_name, MAX_VEHICLE_NAME_LENGTH-1);
   vehicle_name[MAX_VEHICLE_NAME_LENGTH-1] = 0;
   vehicle_type = pPHRTE->vehicle_type;

   if ( pPHRTE->flags & FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY )
      telemetry_params.flags |= TELEMETRY_FLAGS_SPECTATOR_ENABLE;
   else
      telemetry_params.flags &= ~TELEMETRY_FLAGS_SPECTATOR_ENABLE;

   u32 ver = pPHRTE->rubyVersion;
   log_line("populateFromVehicleTelemetryData (version 3): firmware type: %s, sw version from telemetry stream: %d.%d", str_format_firmware_type(getVehicleFirmwareType()), ver>>4, ver & 0x0F);
   log_line("populateFromVehicleTelemetryData (version 3): radio links: %d", pPHRTE->radio_links_count);
   for( int i=0; i<pPHRTE->radio_links_count; i++ )
   {
      log_line("populateFromVehicleTelemetryData (version 3): radio link %d: %u kHz", i+1, pPHRTE->uRadioFrequenciesKhz[i]);
   }
   if ( ver > 0 )
   {
      u32 uBuild = sw_version >> 16;
      if ( (ver>>4) < 7 )
         uBuild = 50;
      if ( (ver>>4) == 7 )
      if ( (ver & 0x0F) < 7 )
         uBuild = 50;

      sw_version = ((ver>>4)) * 256 + ((ver & 0x0F)*10);
      sw_version |= (uBuild<<16);
      log_line("populateFromVehicleTelemetryData (version 3): set sw version to: %d.%d (b %d)", (sw_version>>8) & 0xFF, (sw_version & 0xFF)/10, sw_version >> 16);
   }

   resetRadioLinksParams();
   radioLinksParams.links_count = pPHRTE->radio_links_count;
   relay_params.isRelayEnabledOnRadioLinkId = -1;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radioLinksParams.link_frequency_khz[i] = pPHRTE->uRadioFrequenciesKhz[i];
      if ( pPHRTE->uRelayLinks & (1<<i) )
      {
         radioLinksParams.link_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
         relay_params.isRelayEnabledOnRadioLinkId = i;
         relay_params.uRelayFrequencyKhz = pPHRTE->uRadioFrequenciesKhz[i];
      } 
   }

   radioInterfacesParams.interfaces_count = 1;
   radioInterfacesParams.interface_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
   if ( radioLinksParams.link_frequency_khz[0] > 1000000 )
      radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
   radioInterfacesParams.interface_link_id[0] = 0;
   radioInterfacesParams.interface_current_frequency_khz[0] = radioLinksParams.link_frequency_khz[0];
   strcpy(radioInterfacesParams.interface_szMAC[0], "XXXXXX");
   strcpy(radioInterfacesParams.interface_szPort[0], "X");
   radioInterfacesParams.interface_raw_power[0] = DEFAULT_RADIO_TX_POWER;
   if ( radioLinksParams.link_frequency_khz[0] < 1000000 )
      radioInterfacesParams.interface_raw_power[0] = DEFAULT_RADIO_SIK_TX_POWER;
   radioInterfacesParams.interface_dummy2[0] = 0;
   radioInterfacesParams.interface_supported_bands[0] = getBand(radioLinksParams.link_frequency_khz[0]);
   radioInterfacesParams.interface_card_model[0] = 0;

   if ( getBand(radioLinksParams.link_frequency_khz[0]) == RADIO_HW_SUPPORTED_BAND_58 )
      radioInterfacesParams.interface_radiotype_and_driver[0] = RADIO_TYPE_REALTEK | (RADIO_HW_DRIVER_REALTEK_8812AU<<8);
   else if ( (getBand(radioLinksParams.link_frequency_khz[0]) == RADIO_HW_SUPPORTED_BAND_23) || 
       (getBand(radioLinksParams.link_frequency_khz[0]) == RADIO_HW_SUPPORTED_BAND_24) ||
       (getBand(radioLinksParams.link_frequency_khz[0]) == RADIO_HW_SUPPORTED_BAND_25) )
      radioInterfacesParams.interface_radiotype_and_driver[0] = RADIO_TYPE_ATHEROS | (RADIO_HW_DRIVER_ATHEROS<<8);
   else
      radioInterfacesParams.interface_radiotype_and_driver[0] = RADIO_TYPE_SIK | (RADIO_HW_DRIVER_SERIAL_SIK<<8);

   // Assign radio interfaces to all radio links

   int iInterfaceIndex = radioInterfacesParams.interfaces_count;
   for( int i=1; i<radioLinksParams.links_count; i++ )
   {
      if ( radioLinksParams.link_frequency_khz[i] == 0 )
         continue;

      radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      if ( radioLinksParams.link_frequency_khz[i] > 1000000 )
         radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
      radioInterfacesParams.interface_link_id[iInterfaceIndex] = i;
      radioInterfacesParams.interface_current_frequency_khz[iInterfaceIndex] = radioLinksParams.link_frequency_khz[i];
      strcpy(radioInterfacesParams.interface_szMAC[iInterfaceIndex], "XXXXXX");
      strcpy(radioInterfacesParams.interface_szPort[iInterfaceIndex], "X");
      radioInterfacesParams.interface_raw_power[iInterfaceIndex] = DEFAULT_RADIO_TX_POWER;
      if ( radioLinksParams.link_frequency_khz[i] < 1000000 )
         radioInterfacesParams.interface_raw_power[iInterfaceIndex] = DEFAULT_RADIO_SIK_TX_POWER;
      radioInterfacesParams.interface_dummy2[iInterfaceIndex] = 0;
      radioInterfacesParams.interface_supported_bands[iInterfaceIndex] = getBand(radioLinksParams.link_frequency_khz[i]);
      if ( getBand(radioLinksParams.link_frequency_khz[i]) == RADIO_HW_SUPPORTED_BAND_58 )
         radioInterfacesParams.interface_radiotype_and_driver[iInterfaceIndex] = RADIO_TYPE_REALTEK | (RADIO_HW_DRIVER_REALTEK_8812AU<<8);
      else if ( (getBand(radioLinksParams.link_frequency_khz[i]) == RADIO_HW_SUPPORTED_BAND_23) || 
              (getBand(radioLinksParams.link_frequency_khz[i]) == RADIO_HW_SUPPORTED_BAND_24) ||
              (getBand(radioLinksParams.link_frequency_khz[i]) == RADIO_HW_SUPPORTED_BAND_25) )
         radioInterfacesParams.interface_radiotype_and_driver[iInterfaceIndex] = RADIO_TYPE_ATHEROS | (RADIO_HW_DRIVER_ATHEROS<<8);
      else
         radioInterfacesParams.interface_radiotype_and_driver[iInterfaceIndex] = RADIO_TYPE_SIK | (RADIO_HW_DRIVER_SERIAL_SIK<<8);
      
      radioInterfacesParams.interface_card_model[iInterfaceIndex] = 0;
      radioInterfacesParams.interfaces_count++;
      iInterfaceIndex++;
   }

   updateRadioInterfacesRadioFlagsFromRadioLinksFlags();

   constructLongName();

   char szFreq1[64];
   char szFreq2[64];
   char szFreq3[64];
   strcpy(szFreq1, str_format_frequency(radioLinksParams.link_frequency_khz[0]));
   strcpy(szFreq2, str_format_frequency(radioLinksParams.link_frequency_khz[1]));
   strcpy(szFreq3, str_format_frequency(radioLinksParams.link_frequency_khz[2]));

   log_line("populateFromVehicleTelemetryData (version 3): %d radio links: freq1: %s, freq2: %s, freq3: %s; %d radio interfaces.",
       radioLinksParams.links_count, szFreq1, szFreq2, szFreq3, radioInterfacesParams.interfaces_count);
   log_line("populateFromVehicleTelemetryData (v3) radio info after update:");
   logVehicleRadioInfo();
}


void Model::populateFromVehicleTelemetryData_v4(t_packet_header_ruby_telemetry_extended_v4* pPHRTE)
{
   uVehicleId = pPHRTE->uVehicleId;
   strncpy(vehicle_name, (char*)pPHRTE->vehicle_name, MAX_VEHICLE_NAME_LENGTH-1);
   vehicle_name[MAX_VEHICLE_NAME_LENGTH-1] = 0;
   vehicle_type = pPHRTE->vehicle_type;

   if ( pPHRTE->flags & FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY )
      telemetry_params.flags |= TELEMETRY_FLAGS_SPECTATOR_ENABLE;
   else
      telemetry_params.flags &= ~TELEMETRY_FLAGS_SPECTATOR_ENABLE;

   u32 ver = pPHRTE->rubyVersion;
   log_line("populateFromVehicleTelemetryData (version 4): firmware type: %s, sw version from telemetry stream: %d.%d", str_format_firmware_type(getVehicleFirmwareType()), ver>>4, ver & 0x0F);
   log_line("populateFromVehicleTelemetryData (version 4): radio links: %d", pPHRTE->radio_links_count);
   for( int i=0; i<pPHRTE->radio_links_count; i++ )
   {
      log_line("populateFromVehicleTelemetryData (version 4): radio link %d: %u kHz", i+1, pPHRTE->uRadioFrequenciesKhz[i]);
   }
   if ( ver > 0 )
   {
      u32 uBuild = sw_version >> 16;
      if ( (ver>>4) < 7 )
         uBuild = 50;
      if ( (ver>>4) == 7 )
      if ( (ver & 0x0F) < 7 )
         uBuild = 50;

      sw_version = ((ver>>4)) * 256 + ((ver & 0x0F)*10);
      sw_version |= (uBuild<<16);
      log_line("populateFromVehicleTelemetryData (version 4): set sw version to: %d.%d (b %d)", (sw_version>>8) & 0xFF, (sw_version & 0xFF)/10, sw_version >> 16);
   }

   resetRadioLinksParams();
   radioLinksParams.links_count = pPHRTE->radio_links_count;
   relay_params.isRelayEnabledOnRadioLinkId = -1;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radioLinksParams.link_frequency_khz[i] = pPHRTE->uRadioFrequenciesKhz[i];
      if ( pPHRTE->uRelayLinks & (1<<i) )
      {
         radioLinksParams.link_capabilities_flags[i] |= RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY;
         relay_params.isRelayEnabledOnRadioLinkId = i;
         relay_params.uRelayFrequencyKhz = pPHRTE->uRadioFrequenciesKhz[i];
      } 
   }

   radioInterfacesParams.interfaces_count = 1;
   radioInterfacesParams.interface_capabilities_flags[0] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
   radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
   if ( radioLinksParams.link_frequency_khz[0] > 1000000 )
      radioInterfacesParams.interface_capabilities_flags[0] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
   radioInterfacesParams.interface_link_id[0] = 0;
   radioInterfacesParams.interface_current_frequency_khz[0] = radioLinksParams.link_frequency_khz[0];
   strcpy(radioInterfacesParams.interface_szMAC[0], "XXXXXX");
   strcpy(radioInterfacesParams.interface_szPort[0], "X");
   radioInterfacesParams.interface_raw_power[0] = DEFAULT_RADIO_TX_POWER;
   if ( radioLinksParams.link_frequency_khz[0] < 1000000 )
      radioInterfacesParams.interface_raw_power[0] = DEFAULT_RADIO_SIK_TX_POWER;
   radioInterfacesParams.interface_dummy2[0] = 0;
   radioInterfacesParams.interface_supported_bands[0] = getBand(radioLinksParams.link_frequency_khz[0]);
   radioInterfacesParams.interface_card_model[0] = 0;

   if ( getBand(radioLinksParams.link_frequency_khz[0]) == RADIO_HW_SUPPORTED_BAND_58 )
      radioInterfacesParams.interface_radiotype_and_driver[0] = RADIO_TYPE_REALTEK | (RADIO_HW_DRIVER_REALTEK_8812AU<<8);
   else if ( (getBand(radioLinksParams.link_frequency_khz[0]) == RADIO_HW_SUPPORTED_BAND_23) || 
       (getBand(radioLinksParams.link_frequency_khz[0]) == RADIO_HW_SUPPORTED_BAND_24) ||
       (getBand(radioLinksParams.link_frequency_khz[0]) == RADIO_HW_SUPPORTED_BAND_25) )
      radioInterfacesParams.interface_radiotype_and_driver[0] = RADIO_TYPE_ATHEROS | (RADIO_HW_DRIVER_ATHEROS<<8);
   else
      radioInterfacesParams.interface_radiotype_and_driver[0] = RADIO_TYPE_SIK | (RADIO_HW_DRIVER_SERIAL_SIK<<8);

   // Assign radio interfaces to all radio links

   int iInterfaceIndex = radioInterfacesParams.interfaces_count;
   for( int i=1; i<radioLinksParams.links_count; i++ )
   {
      if ( radioLinksParams.link_frequency_khz[i] == 0 )
         continue;

      radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex] = RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA;
      if ( radioLinksParams.link_frequency_khz[i] > 1000000 )
         radioInterfacesParams.interface_capabilities_flags[iInterfaceIndex] |= RADIO_HW_CAPABILITY_FLAG_HIGH_CAPACITY;
      radioInterfacesParams.interface_link_id[iInterfaceIndex] = i;
      radioInterfacesParams.interface_current_frequency_khz[iInterfaceIndex] = radioLinksParams.link_frequency_khz[i];
      strcpy(radioInterfacesParams.interface_szMAC[iInterfaceIndex], "XXXXXX");
      strcpy(radioInterfacesParams.interface_szPort[iInterfaceIndex], "X");
      radioInterfacesParams.interface_raw_power[iInterfaceIndex] = DEFAULT_RADIO_TX_POWER;
      if ( radioLinksParams.link_frequency_khz[i] < 1000000 )
         radioInterfacesParams.interface_raw_power[iInterfaceIndex] = DEFAULT_RADIO_SIK_TX_POWER;
      radioInterfacesParams.interface_dummy2[iInterfaceIndex] = 0;
      radioInterfacesParams.interface_supported_bands[iInterfaceIndex] = getBand(radioLinksParams.link_frequency_khz[i]);
      if ( getBand(radioLinksParams.link_frequency_khz[i]) == RADIO_HW_SUPPORTED_BAND_58 )
         radioInterfacesParams.interface_radiotype_and_driver[iInterfaceIndex] = RADIO_TYPE_REALTEK | (RADIO_HW_DRIVER_REALTEK_8812AU<<8);
      else if ( (getBand(radioLinksParams.link_frequency_khz[i]) == RADIO_HW_SUPPORTED_BAND_23) || 
              (getBand(radioLinksParams.link_frequency_khz[i]) == RADIO_HW_SUPPORTED_BAND_24) ||
              (getBand(radioLinksParams.link_frequency_khz[i]) == RADIO_HW_SUPPORTED_BAND_25) )
         radioInterfacesParams.interface_radiotype_and_driver[iInterfaceIndex] = RADIO_TYPE_ATHEROS | (RADIO_HW_DRIVER_ATHEROS<<8);
      else
         radioInterfacesParams.interface_radiotype_and_driver[iInterfaceIndex] = RADIO_TYPE_SIK | (RADIO_HW_DRIVER_SERIAL_SIK<<8);
      
      radioInterfacesParams.interface_card_model[iInterfaceIndex] = 0;
      radioInterfacesParams.interfaces_count++;
      iInterfaceIndex++;
   }

   updateRadioInterfacesRadioFlagsFromRadioLinksFlags();

   constructLongName();

   char szFreq1[64];
   char szFreq2[64];
   char szFreq3[64];
   strcpy(szFreq1, str_format_frequency(radioLinksParams.link_frequency_khz[0]));
   strcpy(szFreq2, str_format_frequency(radioLinksParams.link_frequency_khz[1]));
   strcpy(szFreq3, str_format_frequency(radioLinksParams.link_frequency_khz[2]));

   log_line("populateFromVehicleTelemetryData (version 4): %d radio links: freq1: %s, freq2: %s, freq3: %s; %d radio interfaces.",
       radioLinksParams.links_count, szFreq1, szFreq2, szFreq3, radioInterfacesParams.interfaces_count);
   log_line("populateFromVehicleTelemetryData (v4) radio info after update:");
   logVehicleRadioInfo();
}

int Model::get_video_profile_total_levels(int iProfile)
{
   if ( iProfile < 0 || iProfile >= MAX_VIDEO_LINK_PROFILES )
      return 0;

   int iDataCount = video_link_profiles[iProfile].block_packets;
   int iECCount = video_link_profiles[iProfile].block_fecs;

   if ( video_link_profiles[iProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME )
   if ( iProfile != video_params.user_selected_video_link_profile )
   if ( iDataCount < video_link_profiles[video_params.user_selected_video_link_profile].block_packets )
   {
      iDataCount = video_link_profiles[video_params.user_selected_video_link_profile].block_packets;
      iECCount = iDataCount/4;
   }

   if ( (iProfile == VIDEO_PROFILE_LQ) && (iECCount >= (iDataCount*3)/4 + 1 ) )
      return 1;
   else if ( iECCount >= iDataCount/2 + 1 )
      return 1;

   int iOneLevelShift = iDataCount/4;
   if ( iOneLevelShift < 1 )
      iOneLevelShift = 1;

   int nLevels = ((iDataCount/2+1) - iECCount)/iOneLevelShift;
   if ( iProfile == VIDEO_PROFILE_LQ )
      nLevels = 1 + (((iDataCount*3)/4+1) - iECCount)/iOneLevelShift;

   if ( nLevels == 0 )
      nLevels = 1;
   return nLevels+1;
}

int Model::get_video_profile_from_total_levels_shift(int iLevelShift)
{
   int iLevelsHQ = get_video_profile_total_levels(video_params.user_selected_video_link_profile);
   int iLevelsMQ = get_video_profile_total_levels(VIDEO_PROFILE_MQ);
   if ( iLevelShift < iLevelsHQ )
      return video_params.user_selected_video_link_profile;
   else if ( iLevelShift < iLevelsHQ + iLevelsMQ )
      return VIDEO_PROFILE_MQ;
   else
      return VIDEO_PROFILE_LQ;
}

int Model::get_video_profile_level_shift_from_total_levels_shift(int iTotalLevelsShift)
{
   int iLevelsHQ = get_video_profile_total_levels(video_params.user_selected_video_link_profile);
   int iLevelsMQ = get_video_profile_total_levels(VIDEO_PROFILE_MQ);
   int iLevelsLQ = get_video_profile_total_levels(VIDEO_PROFILE_LQ);
   if ( iTotalLevelsShift < iLevelsHQ )
      return iTotalLevelsShift;
   if ( iTotalLevelsShift < iLevelsHQ + iLevelsMQ )
      return iTotalLevelsShift - iLevelsHQ;
   
   int iLevel = iTotalLevelsShift - iLevelsHQ - iLevelsMQ;
   if ( iLevel > iLevelsLQ )
      iLevel = iLevelsLQ;

   return iLevel;
}

int Model::get_video_profile_ec_scheme(int iVideoProfile, int* piData, int* piEC)
{
   if ( iVideoProfile == video_params.user_selected_video_link_profile )
      return get_level_shift_ec_scheme(0, piData, piEC);
   if ( iVideoProfile == VIDEO_PROFILE_MQ )
      return get_level_shift_ec_scheme(get_video_profile_total_levels(video_params.user_selected_video_link_profile), piData, piEC);
   if ( iVideoProfile == VIDEO_PROFILE_LQ )
      return get_level_shift_ec_scheme(get_video_profile_total_levels(video_params.user_selected_video_link_profile) + get_video_profile_total_levels(VIDEO_PROFILE_MQ), piData, piEC);

   return get_level_shift_ec_scheme(0, piData, piEC);
}

int Model::get_level_shift_ec_scheme(int iTotalLevelsShift, int* piData, int* piEC)
{
   int iVideoProfile = get_video_profile_from_total_levels_shift(iTotalLevelsShift);
   int iProfileLevelShift = get_video_profile_level_shift_from_total_levels_shift(iTotalLevelsShift);

   int iDataCount = video_link_profiles[iVideoProfile].block_packets;
   int iECCount = video_link_profiles[iVideoProfile].block_fecs;

   if ( video_link_profiles[iVideoProfile].uProfileEncodingFlags & VIDEO_PROFILE_ENCODING_FLAG_AUTO_EC_SCHEME )
   if ( iVideoProfile != video_params.user_selected_video_link_profile )
   if ( iDataCount < video_link_profiles[video_params.user_selected_video_link_profile].block_packets )
   {
      iDataCount = video_link_profiles[video_params.user_selected_video_link_profile].block_packets;
      iECCount = iDataCount/4;

      float fECRateUser = (float)video_link_profiles[video_params.user_selected_video_link_profile].block_fecs/(float)video_link_profiles[video_params.user_selected_video_link_profile].block_packets;
      float fECRateNew = (float)iECCount/(float)iDataCount;
      if ( fECRateNew < fECRateUser )
         iECCount = fECRateUser * (float)iDataCount;
   }

   int iOneLevelShift = iDataCount/4;
   if ( iOneLevelShift < 1 )
      iOneLevelShift = 1;

   iECCount += iOneLevelShift * iProfileLevelShift;
   if ( iECCount > iDataCount )
      iECCount = iDataCount;

   if ( NULL != piData )
      *piData = iDataCount;
   if ( NULL != piEC )
      *piEC = iECCount;

   return iVideoProfile;
}

int Model::get_current_max_video_packets_for_all_profiles()
{
   int iMax = 0;
   for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
   {
      if ( video_link_profiles[i].block_packets + video_link_profiles[i].block_fecs > iMax )
         iMax = video_link_profiles[i].block_packets + video_link_profiles[i].block_fecs;
      if ( 2*video_link_profiles[i].block_packets > iMax )
         iMax = 2*video_link_profiles[i].block_packets;
   }
   return iMax;
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

bool Model::isAudioCapableAndEnabled()
{
   if ( ! audio_params.has_audio_device )
      return false;
   if ( isRunningOnOpenIPCHardware() )
   if ( (audio_params.uFlags & 0x03) == 0 )
      return false;

   if ( ! audio_params.enabled )
      return false;
   return true; 
}

void Model::constructLongName()
{
   vehicle_long_name[0] = 0;
   
   if ( getVehicleFirmwareType() == MODEL_FIRMWARE_TYPE_RUBY )
   {
      switch ( vehicle_type & MODEL_TYPE_MASK )
      {
         case MODEL_TYPE_DRONE: strcpy(vehicle_long_name, "drone "); break;
         case MODEL_TYPE_AIRPLANE: strcpy(vehicle_long_name, "airplane "); break;
         case MODEL_TYPE_HELI: strcpy(vehicle_long_name, "helicopter "); break;
         case MODEL_TYPE_CAR: strcpy(vehicle_long_name, "car "); break;
         case MODEL_TYPE_BOAT: strcpy(vehicle_long_name, "boat "); break;
         case MODEL_TYPE_ROBOT: strcpy(vehicle_long_name, "robot "); break;
         default: strcpy(vehicle_long_name, "vehicle "); break;
      }
   }
   strcat(vehicle_long_name, getShortName());
}

u32 Model::getVehicleFirmwareType()
{
   return ((vehicle_type & MODEL_FIRMWARE_MASK) >> 5);
}

const char* Model::getVehicleType(u8 vtype)
{
   switch ( vtype & MODEL_TYPE_MASK )
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


void Model::copy_radio_link_params(int iFrom, int iTo)
{
   if ( iFrom < 0 || iTo < 0 )
      return;
   if ( iFrom >= MAX_RADIO_INTERFACES || iTo >= MAX_RADIO_INTERFACES )
      return;

   radioLinksParams.link_frequency_khz[iTo] = radioLinksParams.link_frequency_khz[iFrom];
   radioLinksParams.link_capabilities_flags[iTo] = radioLinksParams.link_capabilities_flags[iFrom];
   radioLinksParams.link_radio_flags[iTo] = radioLinksParams.link_radio_flags[iFrom];
   radioLinksParams.link_datarate_video_bps[iTo] = radioLinksParams.link_datarate_video_bps[iFrom];
   radioLinksParams.link_datarate_data_bps[iTo] = radioLinksParams.link_datarate_data_bps[iFrom];
   radioLinksParams.uSerialPacketSize[iTo] = radioLinksParams.uSerialPacketSize[iFrom];
   radioLinksParams.uDummy2[iTo] = radioLinksParams.uDummy2[iFrom];
   radioLinksParams.uplink_datarate_video_bps[iTo] = radioLinksParams.uplink_datarate_video_bps[iFrom];
   radioLinksParams.uplink_datarate_data_bps[iTo] = radioLinksParams.uplink_datarate_data_bps[iFrom];
   radioLinksParams.uUplinkDataDataRateType[iTo] = radioLinksParams.uUplinkDataDataRateType[iFrom];
   radioLinksParams.uDownlinkDataDataRateType[iTo] = radioLinksParams.uDownlinkDataDataRateType[iFrom];
}

void Model::copy_radio_interface_params(int iFrom, int iTo)
{
   if ( iFrom < 0 || iTo < 0 )
      return;
   if ( iFrom >= MAX_RADIO_INTERFACES || iTo >= MAX_RADIO_INTERFACES )
      return;

   radioInterfacesParams.interface_card_model[iTo] = radioInterfacesParams.interface_card_model[iFrom];
   radioInterfacesParams.interface_link_id[iTo] = radioInterfacesParams.interface_link_id[iFrom];
   radioInterfacesParams.interface_raw_power[iTo] = radioInterfacesParams.interface_raw_power[iFrom];
   radioInterfacesParams.interface_radiotype_and_driver[iTo] = radioInterfacesParams.interface_radiotype_and_driver[iFrom];
   radioInterfacesParams.interface_supported_bands[iTo] = radioInterfacesParams.interface_supported_bands[iFrom];
   strcpy( radioInterfacesParams.interface_szMAC[iTo], radioInterfacesParams.interface_szMAC[iFrom]);
   strcpy( radioInterfacesParams.interface_szPort[iTo], radioInterfacesParams.interface_szPort[iFrom]);
   radioInterfacesParams.interface_capabilities_flags[iTo] = radioInterfacesParams.interface_capabilities_flags[iFrom];
   radioInterfacesParams.interface_current_frequency_khz[iTo] = radioInterfacesParams.interface_current_frequency_khz[iFrom];
   radioInterfacesParams.interface_current_radio_flags[iTo] = radioInterfacesParams.interface_current_radio_flags[iFrom];
   radioInterfacesParams.interface_dummy2[iTo] = radioInterfacesParams.interface_dummy2[iFrom];
}


bool IsModelRadioConfigChanged(type_radio_links_parameters* pRadioLinks1, type_radio_interfaces_parameters* pRadioInterfaces1, type_radio_links_parameters* pRadioLinks2, type_radio_interfaces_parameters* pRadioInterfaces2)
{
   if ( (NULL == pRadioLinks1) || (NULL == pRadioLinks2) || (NULL == pRadioInterfaces1) || (NULL == pRadioInterfaces2) )
      return false;

   if ( pRadioLinks1->links_count != pRadioLinks2->links_count )
      return true;

   if ( pRadioInterfaces1->interfaces_count != pRadioInterfaces2->interfaces_count )
      return true;

   for( int i=0; i<pRadioLinks1->links_count; i++ )
   {
      if ( pRadioLinks1->link_frequency_khz[i] != pRadioLinks2->link_frequency_khz[i] )
         return true;
      if ( pRadioLinks1->link_capabilities_flags[i] != pRadioLinks2->link_capabilities_flags[i] )
         return true;
      if ( pRadioLinks1->link_radio_flags[i] != pRadioLinks2->link_radio_flags[i] )
         return true;
      if ( pRadioLinks1->link_datarate_data_bps[i] != pRadioLinks2->link_datarate_data_bps[i] )
         return true;
   }
   return false;     
}



u32 get_sw_version_major(Model* pModel)
{
   if ( NULL != pModel )
      return (pModel->sw_version >> 8) & 0xFF;
   return SYSTEM_SW_VERSION_MAJOR;
}

u32 get_sw_version_minor(Model* pModel)
{
   u32 uRes = SYSTEM_SW_VERSION_MINOR;
   if ( NULL != pModel )
      uRes = (pModel->sw_version & 0xFF);
   if ( uRes < 10 )
      uRes *= 10;
   return uRes;
}
u32 get_sw_version_build(Model* pModel)
{
   if ( NULL != pModel )
      return (pModel->sw_version >> 16);
   return SYSTEM_SW_BUILD_NUMBER;
}

int is_sw_version_atleast(Model* pModel, int iMajor, int iMinor)
{
   if ( (int)get_sw_version_major(pModel) > iMajor )
      return 1;
   int iM = (int)get_sw_version_minor(pModel);
   if ( iM > 10 )
      iM /= 10;
   if ( (int)get_sw_version_major(pModel) == iMajor )
   if ( iM >= iMinor )
      return 1;

   return 0;
}
