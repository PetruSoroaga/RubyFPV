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
#include "../base/commands.h"
#include "../base/hardware.h"
#include "../base/hardware_radio.h"
#include "../base/hw_procs.h"

#include "launchers_vehicle.h"
#include "process_upload.h"
#include "ruby_rx_commands.h"
#include "shared_vars.h"
#include "timers.h"


FILE* s_pFileSoftware = NULL;

u32 s_uLastReceivedSoftwareBlockIndex = 0xFFFFFFFF;
u32 s_uLastReceivedSoftwareTotalSize = 0;
u32 s_uCurrentReceivedSoftwareSize = 0;
int s_iUpdateType = 0;
bool s_bSoftwareUpdateStoppedVideoPipeline = false;
char s_szUpdateArchiveFile[256];

u8** s_pSWPackets = NULL;
u8*  s_pSWPacketsReceived = NULL;
u32* s_pSWPacketsSize = NULL;
u32 s_uSWPacketsCount = 0;

void _sw_update_close_remove_temp_files()
{
   if ( NULL != s_pFileSoftware )
       fclose(s_pFileSoftware);
   s_pFileSoftware = NULL;

   if ( 0 != s_szUpdateArchiveFile[0] )
   {
      char szComm[512];
      sprintf(szComm, "rm -rf %s", s_szUpdateArchiveFile);
      hw_execute_bash_command_silent(szComm, NULL);
      s_szUpdateArchiveFile[0] = 0;
   }

   s_uLastReceivedSoftwareBlockIndex = 0xFFFFFFFF;
   s_uLastReceivedSoftwareTotalSize = 0;
   s_uCurrentReceivedSoftwareSize = 0;

   /*
   if ( NULL != s_pSWPackets )
   {
      for( u32 i=0; i<s_uSWPacketsCount; i++ )
         free ((u8*)s_pSWPackets[i]);
      free ((u8*)s_pSWPackets);
   }

   if ( NULL != s_pSWPacketsReceived )
      free((u8*)s_pSWPacketsReceived);

   if ( NULL != s_pSWPacketsSize )
      free((u8*)s_pSWPacketsSize);
   */

   s_pSWPackets = NULL;
   s_pSWPacketsReceived = NULL;
   s_pSWPacketsSize = NULL;
   s_uSWPacketsCount = 0;

   if ( s_bSoftwareUpdateStoppedVideoPipeline )
   {
      char szComm[256];
      sprintf(szComm, "rm -rf %s", FILE_TMP_UPDATE_IN_PROGRESS);
      hw_execute_bash_command_silent(szComm, NULL);
      sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_RESUME_VIDEO, 0);
      s_bSoftwareUpdateStoppedVideoPipeline = false;
   }
}

void process_sw_upload_init()
{
   s_pFileSoftware = NULL;
   s_szUpdateArchiveFile[0] = 0;
   s_bSoftwareUpdateStoppedVideoPipeline = false;

   s_pSWPackets = NULL;
   s_pSWPacketsReceived = NULL;
   s_pSWPacketsSize = NULL;
   s_uSWPacketsCount = 0;
}

void _process_upload_apply()
{
   vehicle_stop_video_capture(g_pCurrentModel);

   if ( 0 == s_iUpdateType )
   {
      log_line("Apply regular update using zip file...");
      hw_execute_bash_command("./ruby_update_worker &", NULL);
      
      for( int i=0; i<10; i++ )
      {
         g_TimeNow = get_current_timestamp_ms();
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastActiveTime = g_TimeNow;
         hardware_sleep_ms(100);
      }
      do
      {
         g_TimeNow = get_current_timestamp_ms();
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastActiveTime = g_TimeNow;

         hardware_sleep_ms(100);
      }
      while ( hw_process_exists("ruby_update_worker") );

      hw_execute_bash_command("chmod 777 ruby*", NULL);

      _sw_update_close_remove_temp_files();
      log_line("Done appling regular update using zip file.");
   }
   else
   {
      char szComm[512];

      log_line("Apply update using controller mirrored files...");
      sprintf(szComm, "tar -zxf %s", s_szUpdateArchiveFile);
      hw_execute_bash_command(szComm, NULL);
      hw_execute_bash_command("chmod 777 ruby*", NULL);

      if ( access( "ruby_capture_raspi", R_OK ) != -1 )
         hw_execute_bash_command("cp -rf ruby_capture_raspi /opt/vc/bin/raspivid", NULL);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = g_TimeNow;

      if ( access( "ruby_config.txt", R_OK ) != -1 )
      {
         hardware_mount_boot();
         hardware_sleep_ms(200);
         hw_execute_bash_command("mv ruby_config.txt /boot/config.txt", NULL);
      }

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = g_TimeNow;

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = g_TimeNow;

      log_line("Done updating. Cleaning up and reboot");
      _sw_update_close_remove_temp_files();

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = g_TimeNow;

      if( access( "ruby_update_vehicle", R_OK ) != -1 )
         hw_execute_bash_command("./ruby_update_vehicle -pre", NULL);
   }

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = g_TimeNow;

   signalReboot();
}

void process_sw_upload_old(u32 command_param, u8* pBuffer, int length)
{
   command_packet_sw_package* params = (command_packet_sw_package*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
   char szComm[512];

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = g_TimeNow;

   bool bSendAck = (bool) command_param;

   // Check for cancel
   if ( params->file_block_index == MAX_U32 || 0 == params->total_size )
   {
      log_line("Upload canceled");
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      _sw_update_close_remove_temp_files();
      return;
   }

   if ( s_uLastReceivedSoftwareBlockIndex == 0xFFFFFFFF && params->file_block_index != 0 )
   {
      log_line("Received wrong software block index: %d, expected: %d", params->file_block_index, 0);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
      _sw_update_close_remove_temp_files();
      return;
   }

   if ( s_uLastReceivedSoftwareBlockIndex != 0xFFFFFFFF && s_uLastReceivedSoftwareBlockIndex == params->file_block_index )
   {
      log_line("Received duplicate software block index: %d, ignoring it.", params->file_block_index);
      if ( bSendAck )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      return;
   }

   if ( s_uLastReceivedSoftwareBlockIndex != 0xFFFFFFFF && s_uLastReceivedSoftwareBlockIndex != params->file_block_index-1 )
   {
      log_line("Received wrong software block index: %d, expected: %d", params->file_block_index, s_uLastReceivedSoftwareBlockIndex+1);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
      _sw_update_close_remove_temp_files();
      return;
   }

   s_uLastReceivedSoftwareBlockIndex = params->file_block_index;
   s_uLastReceivedSoftwareTotalSize = params->total_size;
   s_uCurrentReceivedSoftwareSize += params->block_length;
   int blockSize = length-sizeof(t_packet_header)-sizeof(t_packet_header_command)-sizeof(command_packet_sw_package);
   log_line("Recv sw pkg seg %d, %d of %d bytes, is last:%d, total recv: %d", params->file_block_index, blockSize, s_uLastReceivedSoftwareTotalSize, params->last_block, s_uCurrentReceivedSoftwareSize);

   if ( 0 == s_uLastReceivedSoftwareBlockIndex )
   {
      sprintf(szComm, "touch %s", FILE_TMP_UPDATE_IN_PROGRESS);
      hw_execute_bash_command_silent(szComm, NULL);
      s_bSoftwareUpdateStoppedVideoPipeline = true;
      sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_PAUSE_VIDEO, 0);

      s_iUpdateType = params->type;
      if ( s_iUpdateType == 0 )
      {
         strcpy(s_szUpdateArchiveFile, "ruby_update.zip");
         log_line("Receiving update zip file.");
      }
      else
      {
         strcpy(s_szUpdateArchiveFile, "ruby_update.tar");
         log_line("Receiving update tar file.");
      }
      s_pFileSoftware = fopen(s_szUpdateArchiveFile, "wb");

      if ( NULL == s_pFileSoftware )
      {
         log_softerror_and_alarm("Failed to create file for the downloaded software package.");
         log_softerror_and_alarm("The download did not completed correctly. Expected size: %d, received size: %d", params->total_size, s_uCurrentReceivedSoftwareSize );
         _sw_update_close_remove_temp_files();
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
         return;         
      }
   }
      
   if ( NULL != s_pFileSoftware )
   if (  blockSize != (int)fwrite(pBuffer+sizeof(t_packet_header)+sizeof(t_packet_header_command)+sizeof(command_packet_sw_package), 1, blockSize, s_pFileSoftware) )
   {
      log_softerror_and_alarm("Failed to write to file for the downloaded software package.");
      _sw_update_close_remove_temp_files();
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
      return;
   }

   if ( params->last_block )
   {
      if ( NULL == s_pFileSoftware || (s_uCurrentReceivedSoftwareSize != params->total_size ) )
      {
         log_softerror_and_alarm("The download did not completed correctly. Expected size: %d, received size: %d", params->total_size, s_uCurrentReceivedSoftwareSize );
         _sw_update_close_remove_temp_files();
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
         return;         
      }
   }

   if ( params->last_block || bSendAck )
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);

   if ( ! params->last_block )
      return;

   // Received successfully the entire update file.

   log_line("Received software package correctly. Update file: [%s]. Applying it.", s_szUpdateArchiveFile);
   _process_upload_apply();
}

void process_sw_upload_new(u32 command_param, u8* pBuffer, int length)
{
   command_packet_sw_package* params = (command_packet_sw_package*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
   char szComm[256];

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = g_TimeNow;

   bool bSendAck = (bool) command_param;

   // Check for cancel
   if ( params->file_block_index == MAX_U32 || 0 == params->total_size )
   {
      log_line("Upload canceled");
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      _sw_update_close_remove_temp_files();
      return;
   }

   if ( ! s_bSoftwareUpdateStoppedVideoPipeline )
   {
      sprintf(szComm, "touch %s", FILE_TMP_UPDATE_IN_PROGRESS);
      hw_execute_bash_command_silent(szComm, NULL);
      s_bSoftwareUpdateStoppedVideoPipeline = true;
      sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_PAUSE_VIDEO, 0);
   }

   if ( NULL == s_pSWPackets )
   {
      s_uSWPacketsCount = (params->total_size/params->block_length);
      if ( params->total_size > s_uSWPacketsCount * params->block_length )
         s_uSWPacketsCount++;
      s_pSWPackets = (u8**) malloc(s_uSWPacketsCount*sizeof(u32));
      s_pSWPacketsReceived = (u8*) malloc(s_uSWPacketsCount*sizeof(u8));
      s_pSWPacketsSize = (u32*) malloc(s_uSWPacketsCount*sizeof(u32));

      for( u32 i=0; i<s_uSWPacketsCount; i++ )
      {
         u8* pPacket = (u8*)malloc(params->block_length);
         s_pSWPackets[i] = pPacket;
         s_pSWPacketsReceived[i] = 0;
         s_pSWPacketsSize[i] = 0;
      }
      log_line("SW Upload: allocated buffers for %u packets", s_uSWPacketsCount);

      s_iUpdateType = params->type;
      if ( s_iUpdateType == 0 )
      {
         strcpy(s_szUpdateArchiveFile, "ruby_update.zip");
         log_line("Receiving update zip file.");
      }
      else
      {
         strcpy(s_szUpdateArchiveFile, "ruby_update.tar");
         log_line("Receiving update tar file.");
      }
   }

   if ( params->file_block_index >= s_uSWPacketsCount )
   {
      log_softerror_and_alarm("Received SW Upload packet index %d out of bounds (%u)", params->file_block_index, s_uSWPacketsCount);
      _sw_update_close_remove_temp_files();
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
      return;         
   }

   s_pSWPacketsReceived[params->file_block_index]++;
   s_pSWPacketsSize[params->file_block_index] = length-sizeof(t_packet_header)-sizeof(t_packet_header_command)-sizeof(command_packet_sw_package);
   u8* pPacket = s_pSWPackets[params->file_block_index];
   memcpy(pPacket, pBuffer+sizeof(t_packet_header)+sizeof(t_packet_header_command)+sizeof(command_packet_sw_package), s_pSWPacketsSize[params->file_block_index]);

   if ( ! bSendAck )
   {
      log_line("Recv sw pkg seg %d, is last:%d, %d bytes", params->file_block_index + 1, params->last_block, s_pSWPacketsSize[params->file_block_index]);
      return;
   }

   int iIndexCheck = params->file_block_index;
   int iCount = DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY;

   bool bAllPrevOk = true;

   if ( ! params->last_block )
   {
      while ( iIndexCheck >= 0 && iCount >= 0 )
      {
         if ( 0 == s_pSWPacketsReceived[iIndexCheck] )
         {
            bAllPrevOk = false;
            break;
         }
         iIndexCheck--;
         iCount--;
      }
   }
   else
   {
      for( u32 i=0; i<s_uSWPacketsCount; i++ )
         if ( 0 == s_pSWPacketsReceived[i] )
            bAllPrevOk = false;
   }

   log_line("Recv sw pkg seg %d of %u (is last:%d), %u bytes, all %d prev ok: %d", params->file_block_index + 1, s_uSWPacketsCount, params->last_block, s_pSWPacketsSize[params->file_block_index], DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY, (int)bAllPrevOk);

   int nRepeat = s_pSWPacketsReceived[params->file_block_index];
   if ( params->last_block )
      nRepeat = 10;

   if ( bAllPrevOk )
   {
      for( int i=0; i<nRepeat; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 5);
   }
   else
   {
      for( int i=0; i<nRepeat; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 5);
   }
   if ( ! bAllPrevOk )
   {
      log_softerror_and_alarm("Received invalid SW package. Do nothing.");
      return;
   }
   if ( ! params->last_block )
      return;

   // Received all the sw update packets;

   if ( ! bAllPrevOk )
   {
      if ( params->last_block )
         log_softerror_and_alarm("Received invalid SW package. Do nothing.");
      return;
   }

   log_line("Received entire SW upload.");

   s_pFileSoftware = fopen(s_szUpdateArchiveFile, "wb");
   if ( NULL == s_pFileSoftware )
   {
      log_softerror_and_alarm("Failed to create file for the downloaded software package.");
      log_softerror_and_alarm("The download did not completed correctly. Expected size: %d, received size: %d", params->total_size, s_uCurrentReceivedSoftwareSize );
      _sw_update_close_remove_temp_files();
      return;
   }

   u32 fileSize = 0;
   for( u32 i=0; i<s_uSWPacketsCount; i++ )
   {
      if ( s_pSWPacketsSize[i] != (u32)fwrite(s_pSWPackets[i], 1, s_pSWPacketsSize[i], s_pFileSoftware) )
      {
         log_softerror_and_alarm("Failed to write to file for the downloaded software package.");
         _sw_update_close_remove_temp_files();
         return;
      }
      fileSize += s_pSWPacketsSize[i];
   }
   fclose(s_pFileSoftware);
   s_pFileSoftware = NULL;

   log_line("Write successfully to SW archive file [%s], total segments: %u, total size: %u bytes", s_szUpdateArchiveFile, s_uSWPacketsCount, fileSize);
   if ( fileSize != params->total_size )
      log_softerror_and_alarm("Missmatch between expected file size (%u) and created file size (%u)!", params->total_size, fileSize);

   log_line("Received software package correctly (6.3 method). Update file: [%s]. Applying it.", s_szUpdateArchiveFile);
   _process_upload_apply();
}