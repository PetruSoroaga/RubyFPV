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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/hardware.h"
#include "../base/hardware_files.h"
#include "../base/hardware_radio.h"
#include "../base/hw_procs.h"
#include "../base/ruby_ipc.h"

#include <pthread.h>
#include "process_calib_file.h"
#include "ruby_rx_commands.h"
#include "shared_vars.h"
#include "timers.h"


extern int s_fIPCToRouter;
int s_iCalibrationFileUploadIndex = -1;
u8* s_pCalibrationFileBuffer = NULL;


void process_calibration_files_init()
{
   log_line("Process camera calibration files init.");
   s_iCalibrationFileUploadIndex = -1;
   s_pCalibrationFileBuffer = NULL;
}

void _cleanup_calibration_temp_data()
{
   log_line("Cleaning up camera calibration temp data.");
   if ( NULL != s_pCalibrationFileBuffer )
      free(s_pCalibrationFileBuffer);
   s_pCalibrationFileBuffer = NULL;
   s_iCalibrationFileUploadIndex = -1;
}

void _send_calib_file_segment_to_router(t_packet_header_command_upload_calib_file* pCalibFileSegment)
{
   if ( NULL == pCalibFileSegment )
      return;

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_CALIBRATION_FILE, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.total_length = sizeof(t_packet_header) + sizeof(t_packet_header_command_upload_calib_file);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(&buffer[sizeof(t_packet_header)], pCalibFileSegment, sizeof(t_packet_header_command_upload_calib_file));
   ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

bool process_calibration_file_segment(u32 command_param, u8* pBuffer, int length)
{
   if ( (NULL == pBuffer) || (length < (int)(sizeof(t_packet_header) + sizeof(t_packet_header_command) + sizeof(t_packet_header_command_upload_calib_file))) )
   {
      log_softerror_and_alarm("Received calibration file packet of invalid minimum size: %d bytes", length);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
      return false;
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = g_TimeNow;

   t_packet_header_command_upload_calib_file* pCalibFileData = (t_packet_header_command_upload_calib_file*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
   u8* pFileData = (u8*)(pBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command) + sizeof(t_packet_header_command_upload_calib_file));
   log_line("Recv calib file number: %d, type: %d, file: [%s], seg index %d, is last:%d, block size: %d bytes, this block size: %d bytes, total size: %d bytes",
      pCalibFileData->iUploadFileIndex,
      pCalibFileData->calibration_file_type,
      pCalibFileData->szFileName,
      pCalibFileData->segment_index, pCalibFileData->is_last_segment,
      pCalibFileData->segment_length,
      length-sizeof(t_packet_header)-sizeof(t_packet_header_command)-sizeof(t_packet_header_command_upload_calib_file),
      pCalibFileData->total_file_size);

   if ( s_iCalibrationFileUploadIndex != pCalibFileData->iUploadFileIndex )
   {
      _cleanup_calibration_temp_data();
      s_iCalibrationFileUploadIndex = pCalibFileData->iUploadFileIndex;
   }

   if ( 0 == pCalibFileData->calibration_file_type )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      _send_calib_file_segment_to_router(pCalibFileData);
      g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraBinProfile = 0;
      g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName[0] = 0;
      return true;
   }

   if ( (0 == pCalibFileData->total_file_size) || (MAX_U32 == pCalibFileData->segment_index) )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
      _cleanup_calibration_temp_data();
      return true;
   }

   if ( NULL == s_pCalibrationFileBuffer )
   {
      s_pCalibrationFileBuffer = (u8*)malloc(256*1000);
      if ( NULL == s_pCalibrationFileBuffer )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return false;
      }
      log_line("Allocated camera calibration temp data.");
   }

   if ( pCalibFileData->segment_offset + pCalibFileData->segment_length > pCalibFileData->total_file_size )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
      _cleanup_calibration_temp_data();
      return false;
   }

   if ( pCalibFileData->segment_offset + pCalibFileData->segment_length > 255*1000 )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
      _cleanup_calibration_temp_data();
      return false;
   }

   if ( 0 == pCalibFileData->szFileName[0] )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
      _cleanup_calibration_temp_data();
      return false;
   }

   memcpy(&s_pCalibrationFileBuffer[pCalibFileData->segment_offset], pFileData, pCalibFileData->segment_length);

   if ( pCalibFileData->is_last_segment )
   {
      char szComm[256];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "mkdir -p %s", FOLDER_RUBY_TEMP);
      hw_execute_bash_command(szComm, NULL);

      char szFileName[MAX_FILE_PATH_SIZE];
      strcpy(szFileName, FOLDER_RUBY_TEMP);
      strcat(szFileName, "c_");
      strcat(szFileName, pCalibFileData->szFileName);
      strcat(szFileName, ".bin");
      FILE* fp = fopen(szFileName, "wb");
      if ( NULL == fp )
      {
         _cleanup_calibration_temp_data();
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return false;
      }
      if ( pCalibFileData->total_file_size != fwrite(s_pCalibrationFileBuffer, 1, pCalibFileData->total_file_size, fp) )
      {
         fclose(fp);
         _cleanup_calibration_temp_data();
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0, 0);
         return false;
      }
      fclose(fp);
      _send_calib_file_segment_to_router(pCalibFileData);
      _cleanup_calibration_temp_data();
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);

      g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraBinProfile = pCalibFileData->calibration_file_type;
      strncpy(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].szCameraBinProfileName, pCalibFileData->szFileName, MAX_CAMERA_BIN_PROFILE_NAME);
      return true;
   }

   sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0, 0);
   return true;
}
