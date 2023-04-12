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

#include "../base/shared_mem.h"
#include "../radio/radiolink.h"
#include "../radio/radiopackets2.h"
#include "../base/config.h"
#include "../base/hw_procs.h"
#include "../base/commands.h"
#include "../base/models.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/hardware_radio.h"
#include "../base/hardware_radio_sik.h"
#include "../base/encr.h"
#include "../base/flags.h"
#include "../base/ruby_ipc.h"
#include "../base/core_plugins_settings.h"
#include "../common/string_utils.h"
#include "launchers_vehicle.h"
#include "shared_vars.h"
#include "timers.h"
#include "radio_utils.h"
#include "utils_vehicle.h"
#include "video_link_stats_overwrites.h"
#include "process_upload.h"

#include <time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#define MAX_COMMAND_REPLY_BUFFER 4048

u32 lastRecvCommandType = 0xFFFFFFFF;
u32 lastRecvCommandNumber = 0xFFFFFFFF;
u32 lastRecvCommandResendCounter = 0xFFFFFFFF;
u32 lastRecvCommandTime = 0;
u32 lastRecvSourceControllerId = 0;
u8 lastRecvCommandResponseFlags = 0;
u8 lastRecvCommandReplyBuffer[MAX_COMMAND_REPLY_BUFFER];
int lastRecvCommandReplyBufferLength = 0;
u32 s_CurrentResponseCounter = 0;


int s_fIPCFromRouter = -1;
int s_fIPCToRouter = -1;

u8 s_BufferCommands[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferCommands[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferCommandsPos = 0;

static bool s_bWaitForRadioFlagsChangeConfirmation = false;
static int s_iRadioLinkIdChangeConfirmation = -1;
static type_radio_links_parameters s_LastGoodRadioLinksParams;


static u32 s_ZIPParams_uLastRecvSourceControllerId = 0;
static u32 s_ZIPParams_uLastRecvCommandNumber = 0;
static u32 s_ZIPPAarams_uLastRecvCommandTime = 0;
static u8  s_ZIPParams_Model_Buffer[3048];
static int s_ZIPParams_Model_BufferLength = 0;

static shared_mem_video_link_overwrites s_CurrentVideoLinkOverwrites;


#define MAX_SEGMENTS_FILE_UPLOAD 10000 // About 10 Mbytes of data maximum

typedef struct
{
   u32 uLastFileId;
   char szFileName[128];
   u32 uFileSize;
   u32 uTotalSegments;
   u8* pSegments[MAX_SEGMENTS_FILE_UPLOAD];
   u32 uSegmentsSize[MAX_SEGMENTS_FILE_UPLOAD];
   bool bSegmentsReceived[MAX_SEGMENTS_FILE_UPLOAD];
   u32 uLastCommandIdForThisFile;
} __attribute__((packed)) t_structure_file_upload_info;

t_structure_file_upload_info s_InfoLastFileUploaded;

// Returns true if it was updated

bool _populate_camera_name()
{
   char szCurrentName[1024];

   strcpy(szCurrentName, g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera));

   u8 dataCamera[1024];
   FILE* fd = fopen(FILE_TMP_CAMERA_NAME, "r");
   if ( NULL != fd )
   {
      int nSize = fread(dataCamera, 1, 1023, fd);
      fclose(fd);
      if ( nSize <= 0 )
      {
         g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, "");
         log_softerror_and_alarm("Failed to read camera name from cached camera name file.");
      }
      else
      {
         dataCamera[MAX_CAMERA_NAME_LENGTH-1] = 0;
         for( int i=0; i<MAX_CAMERA_NAME_LENGTH; i++ )
            if ( dataCamera[i] == 10 || dataCamera[i] == 13 )
               dataCamera[i] = 0;
         g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, (char*)&(dataCamera[0]));
         log_line("Read camera name from cached file: [%s]", (char*)dataCamera);
      }
   }
   else
      g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, "");

   if ( 0 != strcmp(szCurrentName, g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera) ) )
   {
      log_line("Vehicle camera name was updated, from [%s] to [%s]", szCurrentName, g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera) );
      return true;
   }
   log_line("Current vehicle camera name: [%s]", g_pCurrentModel->getCameraName(g_pCurrentModel->iCurrentCamera));
   return false;
}

void update_priorities()
{
   hw_set_proc_priority("ruby_rt_vehicle", g_pCurrentModel->niceRouter, g_pCurrentModel->ioNiceRouter, 1);
   if ( g_pCurrentModel->isCameraVeye() )
   {
      hw_set_proc_priority(VIDEO_RECORDER_COMMAND_VEYE, g_pCurrentModel->niceVideo, g_pCurrentModel->ioNiceVideo, 1 );
      hw_set_proc_priority(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME, g_pCurrentModel->niceVideo, g_pCurrentModel->ioNiceVideo, 1 );
   }
   else
      hw_set_proc_priority(VIDEO_RECORDER_COMMAND, g_pCurrentModel->niceVideo, g_pCurrentModel->ioNiceVideo, 1 );
   hw_set_proc_priority("ruby_rx_rc", g_pCurrentModel->niceRC, DEFAULT_IO_PRIORITY_RC, 1 );
   hw_set_proc_priority("ruby_tx_telemetry", g_pCurrentModel->niceTelemetry, 0, 1 );
   hw_set_proc_priority("ruby_rx_commands", g_pCurrentModel->niceOthers, 0, 1 );
}

void signalReinitializeRouterRadioLinks()
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_REINITIALIZE_RADIO_LINKS;
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);

   packet_compute_crc((u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void signalReloadModel(u32 uChangeType)
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED;
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS | (uChangeType<<8);
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);

   packet_compute_crc((u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void signalForceVideoProfile(int iVideoProfile )
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_VEHICLE_FORCE_AUTO_VIDEO_PROFILE;
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.vehicle_id_dest = (u32)iVideoProfile;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);

   packet_compute_crc((u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void signalTXVideoEncodingChanged()
{
   log_line("Signaling router that video encodings changed");
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_SIGNAL_VIDEO_ENCODINGS_CHANGED;
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);

   packet_compute_crc((u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void signalUserSelectedVideoProfileChanged()
{
   log_line("Signaling router that user selected video profile changed");
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_SIGNAL_USER_SELECTED_VIDEO_PROFILE_CHANGED;
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);

   packet_compute_crc((u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}
void signalReboot()
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_REBOOT;
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);

   packet_compute_crc((u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}


void signalCameraParameterChange(u8 param, u8 value)
{
   //log_line("Sending camera param %d, value: %d", param, value);

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_CAMERA_PARAM;
   PH.stream_packet_idx = ((u32)param) | (((u32)value)<<8);
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.vehicle_id_dest = PACKET_COMPONENT_RUBY;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);

   packet_compute_crc((u8*)&PH, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}


void sendControlMessage(u8 packet_type, u32 extraParam)
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type =  packet_type;
   PH.vehicle_id_src = PACKET_COMPONENT_COMMANDS;
   PH.vehicle_id_dest = extraParam;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);
   PH.extra_flags = 0;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   packet_compute_crc(buffer, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void setCommandReplyBuffer(u8* pData, int length)
{
   if ( length <= 0 )
      length = 0;
   if ( length > MAX_COMMAND_REPLY_BUFFER)
      length = MAX_COMMAND_REPLY_BUFFER;

   lastRecvCommandReplyBufferLength = length;

   if ( 0 < length )
      memcpy(lastRecvCommandReplyBuffer, pData, length);
}

void setCommandReplyBufferExtra(u8* pData, int length, u8* pExtra, int extra)
{
   if ( (length+extra) <= 0 )
   {
      length = 0;
      extra = 0;
   }
   if ( (length+extra) > MAX_COMMAND_REPLY_BUFFER)
   {
      length = MAX_COMMAND_REPLY_BUFFER;
      extra = 0;
   }
   lastRecvCommandReplyBufferLength = length+extra;

   if ( 0 < length )
      memcpy(lastRecvCommandReplyBuffer, pData, length);
   if ( 0 < extra )
      memcpy(lastRecvCommandReplyBuffer+length, pExtra, extra);
}

 
void sendCommandReply(u8 responseFlags, int delayMiliSec)
{
   if ( lastRecvCommandType & COMMAND_TYPE_FLAG_NO_RESPONSE_NEEDED )
      return;

   lastRecvCommandResponseFlags = responseFlags;

   t_packet_header PH;
   t_packet_header_command_response PHCR;

   PH.packet_flags = PACKET_COMPONENT_COMMANDS;
   PH.packet_type =  PACKET_TYPE_COMMAND_RESPONSE;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = lastRecvSourceControllerId;
   PH.total_headers_length = sizeof(t_packet_header)+sizeof(t_packet_header_command_response);
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_command_response) + lastRecvCommandReplyBufferLength;
   PH.extra_flags = 0;

   PHCR.origin_command_type = lastRecvCommandType;
   PHCR.origin_command_counter = lastRecvCommandNumber;
   PHCR.origin_command_resend_counter = lastRecvCommandResendCounter;
   PHCR.command_response_flags = lastRecvCommandResponseFlags;
   PHCR.command_response_param = 0;
   PHCR.response_counter = s_CurrentResponseCounter;
   
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&PHCR, sizeof(t_packet_header_command_response));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_command_response), lastRecvCommandReplyBuffer, lastRecvCommandReplyBufferLength);
   packet_compute_crc(buffer, PH.total_length);
   ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);

   char szBuff[32];
   szBuff[0] = 0;
   if ( lastRecvCommandResponseFlags & COMMAND_RESPONSE_FLAGS_OK )
      strcat(szBuff,"[ok]");
   if ( lastRecvCommandResponseFlags & COMMAND_RESPONSE_FLAGS_FAILED )
      strcat(szBuff,"[failed]");
   if ( lastRecvCommandResponseFlags & COMMAND_RESPONSE_FLAGS_UNKNOWN_COMMAND )
      strcat(szBuff,"[unk_comm]");
   log_line_commands("Sent response %s to router for vehicle UID: %u, command nb.%d, command retry counter: %d, command type: %s ", szBuff, g_pCurrentModel->vehicle_id, lastRecvCommandNumber, lastRecvCommandResendCounter, commands_get_description(lastRecvCommandType));
   s_CurrentResponseCounter++;

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

   if ( 0 < delayMiliSec )
      hardware_sleep_ms(delayMiliSec);
}


void save_config_file()
{
   hardware_mount_boot();
   hardware_sleep_ms(200);
   hw_execute_bash_command("cp /boot/config.txt config.txt", NULL);

   config_file_set_value("config.txt", "over_voltage", g_pCurrentModel->iOverVoltage);
   config_file_set_value("config.txt", "over_voltage_sdram", g_pCurrentModel->iOverVoltage);
   config_file_set_value("config.txt", "over_voltage_min", g_pCurrentModel->iOverVoltage);

   config_file_set_value("config.txt", "arm_freq", g_pCurrentModel->iFreqARM);
   config_file_set_value("config.txt", "arm_freq_min", g_pCurrentModel->iFreqARM);

   config_file_set_value("config.txt", "gpu_freq", g_pCurrentModel->iFreqGPU);
   config_file_set_value("config.txt", "gpu_freq_min", g_pCurrentModel->iFreqGPU);
   config_file_set_value("config.txt", "core_freq_min", g_pCurrentModel->iFreqGPU);
   config_file_set_value("config.txt", "h264_freq_min", g_pCurrentModel->iFreqGPU);
   config_file_set_value("config.txt", "isp_freq_min", g_pCurrentModel->iFreqGPU);
   config_file_set_value("config.txt", "v3d_freq_min", g_pCurrentModel->iFreqGPU);

   config_file_set_value("config.txt", "sdram_freq", g_pCurrentModel->iFreqGPU);
   config_file_set_value("config.txt", "sdram_freq_min", g_pCurrentModel->iFreqGPU);

   hw_execute_bash_command("cp config.txt /boot/config.txt", NULL);
}

bool _process_file_download_request( u8* pBuffer, int length)
{
   t_packet_header_command* pPHC = (t_packet_header_command*)(pBuffer + sizeof(t_packet_header));

   t_packet_header_download_file_info PHDFInfo;

   u32 uFileId = pPHC->command_param & 0x00FFFFFF;
   bool bCheckStatus = (pPHC->command_param & 0xFF000000)?true:false;
   PHDFInfo.file_id = uFileId;
   PHDFInfo.szFileName[0] = 0;
   PHDFInfo.segments_count = 0;
   PHDFInfo.segment_size = 0;
   PHDFInfo.isReady = 0;

   if ( bCheckStatus )
      log_line("Received request to check download ready statusl for file id: %d", uFileId);
   else
      log_line("Received request to download file id: %d", uFileId);

   if ( uFileId == FILE_ID_VEHICLE_LOGS_ARCHIVE )
   {
      if ( bCheckStatus )
      {
         if ( hw_process_exists("zip") )
            PHDFInfo.isReady = 0;
         else
         {
            PHDFInfo.isReady = 1;
            strcpy((char*)PHDFInfo.szFileName, "logs.zip");

            FILE* fd = fopen("tmp/logs.zip", "rb");
            if ( NULL != fd )
            {
               fseek(fd, 0, SEEK_END);
               long fSize = ftell(fd);
               fclose(fd);
               PHDFInfo.segment_size = 1117;
               PHDFInfo.segments_count = fSize/PHDFInfo.segment_size;
               if ( fSize % PHDFInfo.segment_size )
                  PHDFInfo.segments_count++;
               log_line("File logs archive: %u bytes, %d segments, %d bytes/segment", fSize, PHDFInfo.segments_count, PHDFInfo.segment_size );
            }
            else
               log_softerror_and_alarm("Failed to create archive with log files.");
         }
      }
      else
      {
         hw_execute_bash_command("rm -rf tmp/logs.zip", NULL);
         hw_execute_bash_command("zip tmp/logs.zip logs/* > /dev/null 2>&1 &", NULL);
      }
   }

   setCommandReplyBuffer((u8*)&PHDFInfo, sizeof(PHDFInfo));
   sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
   return true;
}


bool _process_file_segment_download_request( u8* pBuffer, int length)
{
   t_packet_header_command* pPHC = (t_packet_header_command*)(pBuffer + sizeof(t_packet_header));

   u32 uFileId = (pPHC->command_param & 0xFFFF);
   u32 uSegmentId = (pPHC->command_param >> 16);

   log_line("Received request to download file segment %u for file id: %u", uSegmentId, uFileId);

   u8 buffer[2000];

   u32 flags = pPHC->command_param;
   memcpy(buffer, (u8*)&flags, sizeof(u32));
   if ( uFileId == FILE_ID_VEHICLE_LOGS_ARCHIVE )
   {
      FILE* fd = fopen("tmp/logs.zip", "rb");
      if ( NULL != fd )
      {
          fseek(fd, uSegmentId*1117, SEEK_SET);
          if ( 1117 != fread(&buffer[4], 1, 1117, fd) )
             log_softerror_and_alarm("Failed to read vehicle logs zip file.");
          fclose(fd);
      }
      else
      {
         log_softerror_and_alarm("Failed to open for read vehicle logs zip file.");
         memset(&buffer[4], 0, 1117);
      }
   }

   lastRecvCommandType &= ~COMMAND_TYPE_FLAG_NO_RESPONSE_NEEDED;
   setCommandReplyBuffer(buffer, 1117+sizeof(u32));
   sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
   return true;
}

void _process_received_uploaded_file()
{
   if ( s_InfoLastFileUploaded.uLastFileId == FILE_ID_CORE_PLUGINS_ARCHIVE )
   {
      log_line("Processing received core plugins archive (%u bytes)...", s_InfoLastFileUploaded.uFileSize);
      hw_execute_bash_command("rm -rf tmp/core_plugins.zip 2>&1", NULL);
      
      FILE* fd = fopen("tmp/core_plugins.zip", "wb");
      if ( NULL == fd )
      {
         log_softerror_and_alarm("Failed to create file on disk for uploaded plugins.");
         return;
      }
      for( u32 u=0; u<s_InfoLastFileUploaded.uTotalSegments; u++ )
      {
          int nRes = fwrite(s_InfoLastFileUploaded.pSegments[u], 1, s_InfoLastFileUploaded.uSegmentsSize[u], fd );
          if ( nRes != (int)s_InfoLastFileUploaded.uSegmentsSize[u] )
          {
             log_softerror_and_alarm("Failed to write file on disk for uploaded plugins.");
             return;
          }
      }

      fflush(fd);
      fclose(fd);
      char szOutput[4096];
      hw_execute_bash_command("chmod 777 tmp/core_plugins.zip 2>&1", NULL);
      hardware_sleep_ms(100);
      hw_execute_bash_command("unzip tmp/core_plugins.zip -d . 2>&1", szOutput);
      log_line("Result: [%s]", szOutput);
      hw_execute_bash_command("chmod 777 plugins/core/*", NULL);
      log_line("Finised processing core plugins archive.");
   }
}

bool _process_file_segment_upload_request( u8* pBuffer, int length)
{
   //t_packet_header_command* pPHC = (t_packet_header_command*)(pBuffer + sizeof(t_packet_header));

   // Discard duplicate segments/commands for last file if already completed

   if ( s_InfoLastFileUploaded.uLastCommandIdForThisFile != 0 )
   if ( lastRecvCommandNumber <= s_InfoLastFileUploaded.uLastCommandIdForThisFile )
   {
      setCommandReplyBuffer(NULL, 0);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      return true;
   }

   command_packet_upload_file_segment segmentData;
   memcpy((u8*)&segmentData, pBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command), sizeof(command_packet_upload_file_segment));

   log_line("Received request to upload file segment %u of %u (%d bytes) for file [%s], file id: %u", segmentData.uSegmentIndex+1, segmentData.uTotalSegments, segmentData.uSegmentSize, segmentData.szFileName, segmentData.uFileId);


   if ( (s_InfoLastFileUploaded.uLastFileId != segmentData.uFileId) ||
        (s_InfoLastFileUploaded.uTotalSegments != segmentData.uTotalSegments) ||
        (s_InfoLastFileUploaded.uFileSize != segmentData.uTotalFileSize) )
   {
      for( u32 u=0; u<s_InfoLastFileUploaded.uTotalSegments; u++ )
      {
         if ( NULL != s_InfoLastFileUploaded.pSegments[u] )
            free(s_InfoLastFileUploaded.pSegments[u]);
         s_InfoLastFileUploaded.pSegments[u] = NULL;
      }

      s_InfoLastFileUploaded.uLastFileId = segmentData.uFileId;
      strncpy(s_InfoLastFileUploaded.szFileName, segmentData.szFileName, 128);
      s_InfoLastFileUploaded.uTotalSegments = segmentData.uTotalSegments;
      s_InfoLastFileUploaded.uFileSize = segmentData.uTotalFileSize;

      for( u32 u=0; u<s_InfoLastFileUploaded.uTotalSegments; u++ )
      {
         s_InfoLastFileUploaded.pSegments[u] = (u8*)malloc(MAX_PACKET_TOTAL_SIZE);
         if ( NULL == s_InfoLastFileUploaded.pSegments[u] )
         {
            s_InfoLastFileUploaded.uLastFileId = MAX_U32;
            setCommandReplyBuffer(NULL, 0);
            sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
            return true;
         }

         s_InfoLastFileUploaded.uSegmentsSize[u] = 0;
         s_InfoLastFileUploaded.bSegmentsReceived[u] = false;
      }
   }

   if ( segmentData.uSegmentIndex >= segmentData.uTotalSegments )
   {
      s_InfoLastFileUploaded.uLastFileId = MAX_U32;
      setCommandReplyBuffer(NULL, 0);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
      return true;
   }

   s_InfoLastFileUploaded.uSegmentsSize[segmentData.uSegmentIndex] = segmentData.uSegmentSize;
   memcpy(s_InfoLastFileUploaded.pSegments[segmentData.uSegmentIndex],
       pBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command) + sizeof(command_packet_upload_file_segment),
       segmentData.uSegmentSize);

   s_InfoLastFileUploaded.bSegmentsReceived[segmentData.uSegmentIndex] = true;

   bool bReceivedAll = true;

   for( u32 u=0; u<s_InfoLastFileUploaded.uTotalSegments; u++ )
      if ( ! s_InfoLastFileUploaded.bSegmentsReceived[u] )
      {
         bReceivedAll = false;
         break;
      }

   if ( bReceivedAll )
   {
      log_line("Received entire file [%s], %u bytes", s_InfoLastFileUploaded.szFileName, s_InfoLastFileUploaded.uFileSize);
      _process_received_uploaded_file();

      for( u32 u=0; u<s_InfoLastFileUploaded.uTotalSegments; u++ )
      {
         if ( NULL != s_InfoLastFileUploaded.pSegments[u] )
            free(s_InfoLastFileUploaded.pSegments[u]);
         s_InfoLastFileUploaded.pSegments[u] = NULL;
      }
      s_InfoLastFileUploaded.uLastFileId = MAX_U32;
      s_InfoLastFileUploaded.uTotalSegments = 0;
      s_InfoLastFileUploaded.uLastCommandIdForThisFile = lastRecvCommandNumber;
   }

   setCommandReplyBuffer(NULL, 0);
   sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
   return true;
}


// returns true if it knows about the command, false if it's an unknown command

bool process_command(u8* pBuffer, int length)
{
   int replyDelay = 0; //milisec
   int dataLength = length - sizeof(t_packet_header) - sizeof(t_packet_header_command);

   char szBuff[1024];
   t_packet_header* pPH = (t_packet_header*)pBuffer;
   t_packet_header_command* pPHC = (t_packet_header_command*)(pBuffer + sizeof(t_packet_header));

   u16 uCommandType = ((pPHC->command_type) & COMMAND_TYPE_MASK);
   u32 uCommandParam = pPHC->command_param;

   if ( uCommandType == COMMAND_ID_SET_ENCRYPTION_PARAMS )
   {
      u8* pData = (pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      u8 flags = *pData;
      pData++;
      u8 len = *pData;
      pData++;
      log_line("Received e flags %d, len: %d", (int)flags, (int)len);
      if ( flags == MODEL_ENC_FLAGS_NONE )
      {
         char szComm[256];
         sprintf(szComm, "rm -rf %s", FILE_ENCRYPTION_PASS);
         hw_execute_bash_command(szComm, NULL);
         rpp(); 
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);

         g_pCurrentModel->enc_flags = flags;
         g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
         signalReloadModel(0);
         return true;
      }

      FILE* fd = fopen(FILE_ENCRYPTION_PASS, "w");
      if ( NULL == fd )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
         return true;
      }
      fwrite(pData, 1, len, fd);
      fclose(fd);

      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);

      g_pCurrentModel->enc_flags = flags;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_DOWNLOAD_FILE )
   {
      return _process_file_download_request( pBuffer, length);
   }

   if ( uCommandType == COMMAND_ID_DOWNLOAD_FILE_SEGMENT )
   {
      return _process_file_segment_download_request( pBuffer, length);
   }

   if ( uCommandType == COMMAND_ID_UPLOAD_FILE_SEGMENT )
   {
      return _process_file_segment_upload_request( pBuffer, length);    
   }

   if ( uCommandType == COMMAND_ID_CLEAR_LOGS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      hw_execute_bash_command("rm -rf logs/*", NULL);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RELAY_PARAMETERS )
   {
      type_relay_parameters* params = (type_relay_parameters*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      memcpy(&(g_pCurrentModel->relay_params), params, sizeof(type_relay_parameters));

      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      for( int i=0; i<5; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 10);
      signalReloadModel(MODEL_CHANGED_RELAY_PARAMS);
      return true;
   }

   if ( uCommandType == COMMAND_ID_GET_CORE_PLUGINS_INFO )
   {
      log_line("Received command to get core plugins info.");
      command_packet_core_plugins_response response;
      
      load_CorePlugins(1);
      load_CorePluginsSettings();
      
      response.iCountPlugins = get_CorePluginsCount();
      for( int i=0; i<response.iCountPlugins; i++ )
      {
         char* szGUID = get_CorePluginGUID(i);
         CorePluginSettings* pSettings = get_CorePluginSettings(szGUID);
         if ( NULL != pSettings )
            memcpy((u8*)&(response.listPlugins[i]), (u8*)pSettings, sizeof(CorePluginSettings));
      }
      log_line("Sending back to controller response to get core plugins info. %d core plugins:", response.iCountPlugins);
      for( int i=0; i<get_CorePluginsCount(); i++ )
      {
         char* szGUID = get_CorePluginGUID(i);
         char* szName = get_CorePluginName(i);
         CorePluginSettings* pSettings = get_CorePluginSettings(szGUID);
         if ( NULL == pSettings )
         {
            log_softerror_and_alarm("Commands: NULL core plugin settings object.");
            continue;
         }
         if ( NULL == szGUID )
         {
            log_softerror_and_alarm("Commands: NULL core plugin GUID.");
            continue;
         }
         if ( NULL == szName )
         {
            log_softerror_and_alarm("Commands: NULL core plugin name.");
            continue;
         }
         if ( NULL == pSettings->szGUID )
         {
            log_softerror_and_alarm("Commands: NULL core plugin settings GUID for plugin [%s]", szName);
            continue;
         }
         if ( NULL == pSettings->szName )
         {
            log_softerror_and_alarm("Commands: NULL core plugin settings name for plugin [%s]", szName);
            continue;
         }
         log_line("Core plugin %d: [%s]-[%s]: [%s]-[%s], enabled: %d", i+1, szName, szGUID, pSettings->szName, pSettings->szGUID, pSettings->iEnabled);
      }
      setCommandReplyBuffer((u8*)&response, sizeof(command_packet_core_plugins_response));
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_OVERCLOCKING_PARAMS )
   {
      command_packet_overclocking_params* params = (command_packet_overclocking_params*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->iFreqARM = params->freq_arm;
      g_pCurrentModel->iFreqGPU = params->freq_gpu;
      g_pCurrentModel->iOverVoltage = params->overvoltage;
      log_line("Received overclocking params: %d arm freq, %d gpu freq, %d overvoltage", g_pCurrentModel->iFreqARM, g_pCurrentModel->iFreqGPU, g_pCurrentModel->iOverVoltage);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      save_config_file();
      return true;
   }

   if ( uCommandType == COMMAND_ID_RESET_CPU_SPEED )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      int board_type = hardware_detectBoardType();
      g_pCurrentModel->iFreqARM = 900;
      if ( board_type == BOARD_TYPE_PIZERO2 )
         g_pCurrentModel->iFreqARM = 1000;
      else if ( board_type == BOARD_TYPE_PI3B )
         g_pCurrentModel->iFreqARM = 1200;
      else if ( board_type == BOARD_TYPE_PI3BPLUS || board_type == BOARD_TYPE_PI4B )
         g_pCurrentModel->iFreqARM = 1400;
      else if ( board_type != BOARD_TYPE_PIZERO && board_type != BOARD_TYPE_PIZEROW && board_type != BOARD_TYPE_NONE 
               && board_type != BOARD_TYPE_PI2B && board_type != BOARD_TYPE_PI2BV11 && board_type != BOARD_TYPE_PI2BV12 )
         g_pCurrentModel->iFreqARM = 1200;

      g_pCurrentModel->iFreqGPU = 400;
      g_pCurrentModel->iOverVoltage = 3;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      save_config_file();
      return true;
   }

   if ( uCommandType == COMMAND_ID_GET_MODULES_INFO )
   {
      char szBuffer[2048];
      char szOutput[1500];
      szBuffer[0] = 0;

      strcpy(szBuffer, "Modules versions: #");

      hw_execute_bash_command_raw_silent("./ruby_start -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "ruby_start: ");
      strcat(szBuffer, szOutput);

      hw_execute_bash_command_raw_silent("./ruby_vehicle -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, ", ruby_vehicle: ");
      strcat(szBuffer, szOutput);

      hw_execute_bash_command_raw_silent("./ruby_rt_vehicle -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, ", ruby_rt_vehicle: ");
      strcat(szBuffer, szOutput);

      hw_execute_bash_command_raw_silent("./ruby_rx_commands -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, ", ruby_rx_commands: ");
      strcat(szBuffer, szOutput);
      
      hw_execute_bash_command_raw_silent("./ruby_tx_telemetry -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, ", ruby_tx_telemetry: ");
      strcat(szBuffer, szOutput);

      hw_execute_bash_command_raw_silent("./ruby_rx_rc -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, ", ruby_rx_rc: ");
      strcat(szBuffer, szOutput);


      hw_execute_bash_command_raw_silent("./ruby_capture_raspi -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, ", ruby_capture_csi: ");
      strcat(szBuffer, szOutput);

      hw_execute_bash_command_raw_silent("./ruby_capture_veye -ver", szOutput);
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      if ( strlen(szOutput)> 0 )
      if ( szOutput[strlen(szOutput)-1] == 10 || szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, ", ruby_capture_veye: ");
      strcat(szBuffer, szOutput);


      strcat(szBuffer, " #");     

      strcat(szBuffer, "USB Devices: #");
      hw_execute_bash_command_raw("lsusb", szOutput);
      strcat(szBuffer, szOutput);
      /*
      strcat(szBuffer, " #Loaded Modules: #");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("lsmod", szOutput);
      int len = strlen(szOutput);
      for( int i=0; i<len; i++ )
      {
         if ( szOutput[i] == 10 )
            szOutput[i] = '#';
         if ( szOutput[i] == 13 )
            szOutput[i] = ' ';
      }
      strcat(szBuffer, szOutput);
      */
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      if ( strlen(szBuffer) > MAX_PACKET_PAYLOAD )
      {
         log_softerror_and_alarm("Modules info string is bigger than max allowed size: %d / %d; truncating it.", strlen(szBuffer), MAX_PACKET_PAYLOAD);
         szBuffer[MAX_PACKET_PAYLOAD-1] = 0;
      }
      log_line("Sending back modules info, %d bytes", strlen(szBuffer)+1);
      setCommandReplyBuffer((u8*)szBuffer, strlen(szBuffer)+1);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, replyDelay);
      return true;

   }

   if ( uCommandType == COMMAND_ID_GET_USB_INFO )
   {
      hw_execute_bash_command("rm -rf tmp/tmp_usb_info.tar 2>/dev/null", NULL);
      hw_execute_bash_command("rm -rf tmp/tmp_usb_info.txt 2>/dev/null", NULL);

      char szBuff[6000];
      char szBuff2[3000];
      szBuff[0] = 0;
      szBuff2[0] = 0;
      hw_execute_bash_command_raw("lsusb", szBuff);
      hw_execute_bash_command_raw("lsusb -t", szBuff2);
      int iLen1 = strlen(szBuff);
      int iLen2 = strlen(szBuff2);
      if ( iLen1 > 3000 )
         iLen1 = 3000;
      strcat(szBuff, "\nTree:\n");
      strcat(szBuff, szBuff2);
      iLen1 = strlen(szBuff);

      FILE* fd = fopen("tmp/tmp_usb_info.txt", "wb");
      if ( NULL == fd )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
          return true;
      }
      fwrite(szBuff, 1, iLen1, fd );
      fclose(fd);

      hw_execute_bash_command("rm -rf tmp/tmp_usb_info.tar 2>/dev/null", NULL);
      hw_execute_bash_command("tar -czf tmp/tmp_usb_info.tar tmp/tmp_usb_info.txt", NULL);

      fd = fopen("tmp/tmp_usb_info.tar", "rb");
      if ( NULL == fd )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
          hw_execute_bash_command("rm -rf tmp/tmp_usb_info.tar 2>/dev/null", NULL);
          hw_execute_bash_command("rm -rf tmp/tmp_usb_info.txt 2>/dev/null", NULL);
          return true;       
      }
      szBuff2[0] = 0;
      iLen2 = fread(szBuff2, 1, 3000, fd);
      fclose(fd);

      if ( iLen2 <= 0 )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
          hw_execute_bash_command("rm -rf tmp/tmp_usb_info.tar 2>/dev/null", NULL);
          hw_execute_bash_command("rm -rf tmp/tmp_usb_info.txt 2>/dev/null", NULL);
          return true;
      }

      log_line("Send reply of %d bytes to get USB info command.", iLen2);
      setCommandReplyBuffer((u8*)szBuff2, iLen2);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      //hw_execute_bash_command("rm -rf tmp/tmp_usb_info.tar 2>/dev/null", NULL);
      //hw_execute_bash_command("rm -rf tmp/tmp_usb_info.txt 2>/dev/null", NULL);
      return true;       
   }

   if ( uCommandType == COMMAND_ID_GET_USB_INFO2 )
   {
      hw_execute_bash_command("rm -rf tmp/tmp_usb_info.tar 2>/dev/null", NULL);
      hw_execute_bash_command("rm -rf tmp/tmp_usb_info.txt 2>/dev/null", NULL);

      char szBuff[3000];
      szBuff[0] = 0;
    
      DIR *d;
      struct dirent *dir;
      char szFile[1024];
      char szComm[1024];
      char szOutput[1024];
      d = opendir("/sys/bus/usb/devices/");
      if (d)
      {
         while ((dir = readdir(d)) != NULL)
         {
            int iLen = strlen(dir->d_name);
            if ( iLen < 3 )
               continue;
            snprintf(szFile, 1023, "/sys/bus/usb/devices/%s", dir->d_name);
            snprintf(szComm, 1023, "cat /sys/bus/usb/devices/%s/uevent | grep DRIVER", dir->d_name);
            szOutput[0] = 0;
            hw_execute_bash_command(szComm, szOutput);
            strcat(szBuff, dir->d_name);
            strcat(szBuff, " :  ");
            strcat(szBuff, szOutput);

            for( int i=0; i<hardware_get_radio_interfaces_count()+1; i++ )
            {
               szOutput[0] = 0;
               sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/wlan%d/uevent 2>/dev/null | grep DEVTYPE=wlan", dir->d_name, i);
               hw_execute_bash_command_silent(szComm, szOutput);
               if ( strlen(szOutput) > 0 )
               {
                  log_line("Accessed %s", szComm);
                  szOutput[0] = 0;
                  sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/wlan%d/uevent 2>/dev/null | grep INTERFACE", dir->d_name, i);
                  hw_execute_bash_command_silent(szComm, szOutput);
                  if ( strlen(szOutput) > 0 )
                  {
                     strcat(szBuff, ", ");
                     strcat(szBuff, szOutput);
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                  }
                  szOutput[0] = 0;
                  sprintf(szComm, "cat /sys/bus/usb/devices/%s/net/wlan%d/uevent 2>/dev/null | grep IFINDEX", dir->d_name, i);
                  hw_execute_bash_command_silent(szComm, szOutput);
                  if ( strlen(szOutput) > 0 )
                  {
                     strcat(szBuff, ", ");
                     strcat(szBuff, szOutput);
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                  }

                  szOutput[0] = 0;
                  sprintf(szComm, "cat /sys/bus/usb/devices/%s/uevent 2>/dev/null | grep PRODUCT", dir->d_name);
                  hw_execute_bash_command_silent(szComm, szOutput);
                  if ( strlen(szOutput) > 0 )
                  {
                     strcat(szBuff, ", ");
                     strcat(szBuff, szOutput);
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                     iLen = strlen(szBuff);
                     if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
                        szBuff[iLen-1] = 0;
                  }
                  break;
               }
            }

            iLen = strlen(szBuff);
            if ( iLen > 0 )
            if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
               szBuff[iLen-1] = 0;

            iLen = strlen(szBuff);
            if ( iLen > 0 )
            if ( szBuff[iLen-1] == 10 || szBuff[iLen-1] == 13 )
               szBuff[iLen-1] = 0;

            strcat(szBuff, "\n");
         }
         closedir(d);
      }

      if ( 0 == strlen(szBuff) )
         strcpy(szBuff, "No info available.");
      int iLen = strlen(szBuff);
      FILE* fd = fopen("tmp/tmp_usb_info2.txt", "wb");
      if ( NULL == fd )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
          return true;
      }
      fwrite(szBuff, 1, iLen, fd );
      fclose(fd);

      hw_execute_bash_command("rm -rf tmp/tmp_usb_info2.tar 2>/dev/null", NULL);
      hw_execute_bash_command("tar -czf tmp/tmp_usb_info2.tar tmp/tmp_usb_info2.txt", NULL);

      fd = fopen("tmp/tmp_usb_info2.tar", "rb");
      if ( NULL == fd )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
          hw_execute_bash_command("rm -rf tmp/tmp_usb_info2.tar 2>/dev/null", NULL);
          hw_execute_bash_command("rm -rf tmp/tmp_usb_info2.txt 2>/dev/null", NULL);
          return true;       
      }
      szBuff[0] = 0;
      iLen = fread(szBuff, 1, 3000, fd);
      fclose(fd);

      if ( iLen <= 0 )
      {
          sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
          hw_execute_bash_command("rm -rf tmp/tmp_usb_info2.tar 2>/dev/null", NULL);
          hw_execute_bash_command("rm -rf tmp/tmp_usb_info2.txt 2>/dev/null", NULL);
          return true;
      }

      log_line("Send reply of %d bytes to get USB info command.", iLen);
      setCommandReplyBuffer((u8*)szBuff, iLen);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      //hw_execute_bash_command("rm -rf tmp/tmp_usb_info.tar 2>/dev/null", NULL);
      //hw_execute_bash_command("rm -rf tmp/tmp_usb_info.txt 2>/dev/null", NULL);
      return true;       
   }

   if ( uCommandType == COMMAND_ID_GET_MEMORY_INFO )
   {
      u32 resp[2];
      resp[0] = 0;
      resp[1] = 0;
      if ( 1 == hw_execute_bash_command_raw("free -m  | grep Mem", szBuff) )
      {
         char szTemp[1024];
         long lt, lu, lf;
         sscanf(szBuff, "%s %ld %ld %ld", szTemp, &lt, &lu, &lf);
         resp[0] = (u32)lt;
         resp[1] = (u32)lf;
         setCommandReplyBuffer((u8*)resp, 2*sizeof(u32));
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, replyDelay);
      }
      else
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
      }
      return true;
   }

   if ( uCommandType == COMMAND_ID_GET_CPU_INFO )
   {
      char szBuffer[2048];
      char szOutput[1500];
      szBuffer[0] = 0;
      
      FILE* fd = fopen(FILE_INFO_VERSION, "r");
      if ( NULL == fd )
         fd = fopen("ruby_ver.txt", "r");
      if ( NULL != fd )
      {
         szOutput[0] = 0;
         if ( 1 == fscanf(fd, "%s", szOutput) )
         {
            strcat(szBuffer, "Ruby base version: ");
            strcat(szBuffer, szOutput);
            strcat(szBuffer, "; ");
         }
         fclose(fd);
      }
      fd = fopen(FILE_INFO_LAST_UPDATE, "r");
      if ( NULL != fd )
      {
         szOutput[0] = 0;
         if ( 1 == fscanf(fd, "%s", szOutput) )
         {
            strcat(szBuffer, "last update: ");
            strcat(szBuffer, szOutput);
            strcat(szBuffer, "+");
         }
         else
         {
            strcat(szBuffer, "Ruby last update: none");
            strcat(szBuffer, "+");
         }
         fclose(fd);
      }
      else
      {
         strcat(szBuffer, "Ruby last update: none");
         strcat(szBuffer, "+");
      }

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_get_proc_priority("ruby_vehicle", szOutput);
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_get_proc_priority("ruby_rt_vehicle", szOutput);
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_get_proc_priority("ruby_tx_telemetry", szOutput);
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_get_proc_priority("ruby_rx_commands", szOutput);
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_get_proc_priority("ruby_rx_rc", szOutput);
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      if ( g_pCurrentModel->isCameraVeye() )
         hw_get_proc_priority(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME, szOutput);
      else
         hw_get_proc_priority(VIDEO_RECORDER_COMMAND, szOutput);
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("nproc --all", szOutput);
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "CPU Cores: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, ", ");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      int speed = hardware_get_cpu_speed();
      sprintf(szOutput, "%d Mhz; ", speed);
      strcat(szBuffer, szOutput);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
      speed = hardware_get_gpu_speed();
      sprintf(szOutput, "GPU: %d Mhz+", speed);
      strcat(szBuffer, szOutput);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_clock h264", szOutput);
      szOutput[0] = 'F';
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "H264: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, " Hz+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_clock isp", szOutput);
      szOutput[0] = 'F';
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "ISP: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, " Hz+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_volts core", szOutput);
      szOutput[strlen(szOutput)-1] = 0;
      if ( szOutput[strlen(szOutput)-1] == 10 || 
         szOutput[strlen(szOutput)-1] == 13 )
         szOutput[strlen(szOutput)-1] = 0;

      strcat(szBuffer, "CPU: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "; ");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_volts sdram_c", szOutput);
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "SDRAM C: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, ", ");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_volts sdram_i", szOutput);
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "I: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, ", ");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      hw_execute_bash_command_raw("vcgencmd measure_volts sdram_p", szOutput);
      szOutput[strlen(szOutput)-1] = 0;
      strcat(szBuffer, "P: ");
      strcat(szBuffer, szOutput);
      strcat(szBuffer, "+");

      sprintf(szOutput, "TxPower Atheros: %d; ", hardware_get_radio_tx_power_atheros());
      strcat(szBuffer, szOutput);
      sprintf(szOutput, "TxPower RTL: %d +", hardware_get_radio_tx_power_rtl());
      strcat(szBuffer, szOutput);

      sprintf(szOutput, "Avg/Max loops (ms): rx_commands: %u/%u;", g_pProcessStats->uAverageLoopTimeMs, g_pProcessStats->uMaxLoopTimeMs);
      strcat(szBuffer, szOutput);
      shared_mem_process_stats* pProcessStats = NULL;
      pProcessStats = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_ROUTER_TX);
      if ( NULL != pProcessStats )
      {
         sprintf(szOutput, " router: %u/%u;", pProcessStats->uAverageLoopTimeMs, pProcessStats->uMaxLoopTimeMs);
         strcat(szBuffer, szOutput);
         shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, pProcessStats);
      }
      else
      {
         strcat(szBuff, " router: N/A;");
      }

      pProcessStats = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_TELEMETRY_TX);
      if ( NULL != pProcessStats )
      {
         sprintf(szOutput, " tx_telemetry: %u/%u;", pProcessStats->uAverageLoopTimeMs, pProcessStats->uMaxLoopTimeMs);
         strcat(szBuffer, szOutput);
         shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, pProcessStats);
      }
      else
      {
         strcat(szBuff, " tx_telemetry: N/A;");
      }

      pProcessStats = shared_mem_process_stats_open_read(SHARED_MEM_WATCHDOG_RC_RX);
      if ( NULL != pProcessStats )
      {
         sprintf(szOutput, " rx_rc: %u/%u;", pProcessStats->uAverageLoopTimeMs, pProcessStats->uMaxLoopTimeMs);
         strcat(szBuffer, szOutput);
         shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, pProcessStats);
      }
      else
      {
         strcat(szBuff, " rx_rc: N/A;");
      }

      strcat(szBuffer, "+");

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      log_line("Sending back CPU info, %d bytes", strlen(szBuffer)+1);
      setCommandReplyBuffer((u8*)szBuffer, strlen(szBuffer)+1);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, replyDelay);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SWAP_RADIO_INTERFACES )
   {
      g_pCurrentModel->swapRadioInterfaces();
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      for( int i=0; i<5; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 10);
      signalReloadModel(MODEL_CHANGED_SWAPED_RADIO_INTERFACES);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_VEHICLE_TYPE )
   {
      g_pCurrentModel->vehicle_type = pPHC->command_param;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_CAMERA_PARAMETERS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      int nCamera = uCommandParam;
      type_camera_parameters* pcpt = (type_camera_parameters*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      if ( nCamera < 0 || nCamera >= g_pCurrentModel->iCameraCount )
         return true;

      u32 oldFlags = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile].flags;
      camera_profile_parameters_t oldParams;
      memcpy(&oldParams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile]), sizeof(camera_profile_parameters_t));

      memcpy(&(g_pCurrentModel->camera_params[nCamera]), pcpt, sizeof(type_camera_parameters));
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(MODEL_CHANGED_CAMERA_PARAMS);

      bool bSentUsingPipe = false;
      camera_profile_parameters_t* pCamParams = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile]);
      if ( g_pCurrentModel->isCameraCSICompatible() )
      {
         if ( oldParams.brightness != pCamParams->brightness )
         {
            bSentUsingPipe = true;
            signalCameraParameterChange(RASPIVID_COMMAND_ID_BRIGHTNESS, pCamParams->brightness);
         }
         if ( oldParams.contrast != pCamParams->contrast )
         {
            bSentUsingPipe = true;
            signalCameraParameterChange(RASPIVID_COMMAND_ID_CONTRAST, pCamParams->contrast);
         }
         if ( oldParams.saturation != pCamParams->saturation )
         {
            bSentUsingPipe = true;
            signalCameraParameterChange(RASPIVID_COMMAND_ID_SATURATION, pCamParams->saturation);
         }
         if ( oldParams.sharpness != pCamParams->sharpness )
         {
            bSentUsingPipe = true;
            signalCameraParameterChange(RASPIVID_COMMAND_ID_SHARPNESS, pCamParams->sharpness);
         }
      }
      if ( bSentUsingPipe )
         return true;

      if ( g_pCurrentModel->isCameraVeye327290() )
         vehicle_update_camera_params(g_pCurrentModel, g_pCurrentModel->iCurrentCamera);
      else if ( g_pCurrentModel->hasCamera() )
      {
         if ( (oldFlags & CAMERA_FLAG_AWB_MODE_OLD) != ((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile].flags) & CAMERA_FLAG_AWB_MODE_OLD) )
            g_pCurrentModel->setAWBMode();

         sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM,0);
      }
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_CAMERA_PROFILE )
   {
      u32 oldFlags = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile].flags;

      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile = pPHC->command_param;
      log_line("Switched camera %d to profile %d.", g_pCurrentModel->iCurrentCamera, g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);

      if ( g_pCurrentModel->hasCamera() )
      {
         if ( (oldFlags & CAMERA_FLAG_AWB_MODE_OLD) != ((g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile].flags) & CAMERA_FLAG_AWB_MODE_OLD) )
            g_pCurrentModel->setAWBMode();

         sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM,0);
      }
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_CURRENT_CAMERA )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      int iCam = (int)pPHC->command_param;
      if ( iCam >= 0 && iCam < g_pCurrentModel->iCameraCount )
      {
         g_pCurrentModel->iCurrentCamera = iCam;
         g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
         signalReloadModel(0);

         if ( g_pCurrentModel->hasCamera() )
            sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM,0);
      }
      else
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_FORCE_CAMERA_TYPE )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      int iCamT = (int)pPHC->command_param;
      if ( g_pCurrentModel->hasCamera() )
         vehicle_stop_video_capture(g_pCurrentModel); 
      
      g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType = iCamT;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);

      signalReloadModel(0);

      if ( g_pCurrentModel->hasCamera() )
         sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_START_VIDEO_PROGRAM,0);

      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RADIO_LINK_FREQUENCY )
   {
      u32 uLinkIndex = ((pPHC->command_param)>>24) & 0x7F;
      u32 uNewFreq = (pPHC->command_param) & 0xFFFFFF;
      log_line("Received command to change frequency on radio link %u to %s", uLinkIndex+1, str_format_frequency(uNewFreq));

      bool freqIsOk = false;
      if ( uLinkIndex < (u32)g_pCurrentModel->radioLinksParams.links_count )
      {
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
            if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkIndex )
            if ( hardware_radioindex_supports_frequency(i, uNewFreq) )
               freqIsOk = true;
      }

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId-1 == (int)uLinkIndex )
      {
         log_softerror_and_alarm("Tried to change link frequency (to %s) for a relay link (radio link %u).", str_format_frequency(uNewFreq), uLinkIndex+1);
         freqIsOk = false;
      }
      
      if ( ! freqIsOk )
      {
         for( int i=0; i<20; i++ )
            sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, i+5);
         log_line("Can't do the change.");
         log_current_full_radio_configuration(g_pCurrentModel);
         return true;
      }

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      for( int i=0; i<20; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, i+5);

      hardware_sleep_ms(40);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      log_line("Switching vehicle radio link %u to %s.", uLinkIndex+1, str_format_frequency(uNewFreq));
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkIndex )
         {
            launch_set_frequency(g_pCurrentModel, i, uNewFreq, g_pProcessStats);
            g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i] = uNewFreq;
         }
      hardware_save_radio_info();

      g_pCurrentModel->radioLinksParams.link_frequency[uLinkIndex] = uNewFreq;

      log_current_full_radio_configuration(g_pCurrentModel);

      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      hardware_sleep_ms(20);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
      
      signalReloadModel(MODEL_CHANGED_FREQUENCY);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RC_CAMERA_PARAMS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->camera_rc_channels = pPHC->command_param;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      return true;
   }


   if ( uCommandType == COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      type_functions_parameters* params = (type_functions_parameters*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      memcpy(&(g_pCurrentModel->functions_params), params, sizeof(type_functions_parameters));

      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      return true;
   }


   if ( uCommandType == COMMAND_ID_SET_RADIO_CARD_MODEL )
   {
      int cardIndex = (pPHC->command_param) & 0xFF;
      int cardType = ((int)(((pPHC->command_param) >> 8) & 0xFF)) - 128;
      if ( cardIndex < 0 || cardIndex >= g_pCurrentModel->radioInterfacesParams.interfaces_count )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
         return true;
      }
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->radioInterfacesParams.interface_card_model[cardIndex] = cardType;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      return true;
   }


   if ( uCommandType == COMMAND_ID_SET_RADIO_LINK_CAPABILITIES )
   {
      int linkIndex = (pPHC->command_param) & 0xFF;
      u32 flags = (pPHC->command_param) >> 8;
      char szBuffC[128];
      str_get_radio_capabilities_description(flags, szBuffC);
      log_line("Set radio link nb.%d capabilities flags to %d: %s", linkIndex+1, flags, szBuffC);
      if ( linkIndex < 0 || linkIndex >= g_pCurrentModel->radioLinksParams.links_count )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
         return true;
      }
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);

      bool bOnlyVideoDataChanged = false;
      if ( (flags & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) ) ==
           (g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex] & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA)) ) )
         bOnlyVideoDataChanged = true;

      g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex] = flags;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);

      // If only video/data flags changed, do not reinitialize radio links
      if ( ! bOnlyVideoDataChanged )
         signalReinitializeRouterRadioLinks();
      signalReloadModel(MODEL_CHANGED_RADIO_LINK_CAPABILITIES);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_MODEL_FLAGS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      bool bHadServiceLog = (g_pCurrentModel->uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE)?true:false;
      g_pCurrentModel->uModelFlags = pPHC->command_param;
      if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_USE_LOGER_SERVICE )
      {
         if ( ! bHadServiceLog )
         {
            char szC[128];
            sprintf(szC, "touch %s", LOG_USE_PROCESS);
            hw_execute_bash_command(szC,NULL);
         }
      }
      else
      {
         if ( bHadServiceLog )
         {
            char szC[128];
            sprintf(szC, "rm -rf %s", LOG_USE_PROCESS);
            hw_execute_bash_command(szC,NULL);
         }
      }

      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_CLOCK_SYNC_TYPE )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->clock_sync_type = pPHC->command_param;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS )
   {
      u32* pData = (u32*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      u32 linkIndex = *pData;
      pData++;
      u32 linkFlags = *pData;
      pData++;
      int* piData = (int*)pData;
      int datarateVideo = *piData;
      piData++;
      int datarateData = *piData;

      if ( linkIndex < 0 || linkIndex >= (u32)g_pCurrentModel->radioLinksParams.links_count )
      {
         log_error_and_alarm("Invalid link index received in command: %d (%d radio links on vehicle)", (int)linkIndex+1, g_pCurrentModel->radioLinksParams.links_count);
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
         return true;
      }

      g_TimeLastRadioLinkFlagsChange = g_TimeNow;
      s_bWaitForRadioFlagsChangeConfirmation = true;
      s_iRadioLinkIdChangeConfirmation = (int)linkIndex;
      memcpy(&s_LastGoodRadioLinksParams, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   
      g_pCurrentModel->radioLinksParams.link_radio_flags[linkIndex] = linkFlags;
      g_pCurrentModel->radioLinksParams.link_datarates[linkIndex][0] = datarateVideo;
      g_pCurrentModel->radioLinksParams.link_datarates[linkIndex][1] = datarateData;

      char szBuffR[128];
      str_get_radio_frame_flags_description(linkFlags, szBuffR); 
      log_line("Received new radio link flags for radio link %d: %s, datarates: %d/%d", (int)linkIndex+1, szBuffR, datarateVideo, datarateData);

      // Populate radio interfaces radio flags and rates from radio links radio flags and rates

      g_pCurrentModel->updateRadioInterfacesRadioFlags();

      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      if ( linkIndex == 0 )
         signalReloadModel(MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK0);
      else if ( linkIndex == 1 )
         signalReloadModel(MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK1);
      else if ( linkIndex == 2 )
         signalReloadModel(MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK2);
      else if ( linkIndex == 3 )
         signalReloadModel(MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK3);

      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 5);
      for( int i=0; i<5; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 10);

      return true;
   }

   if ( uCommandType == COMMAND_ID_RADIO_LINK_FLAGS_CHANGED_CONFIRMATION )
   {
      int linkId = (int)(pPHC->command_param);
      log_line("Received radio link flags change confirmation from controller for link %d.", linkId+1);

      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);

      g_TimeLastRadioLinkFlagsChange = 0;
      s_bWaitForRadioFlagsChangeConfirmation = false;
      s_iRadioLinkIdChangeConfirmation = -1;

      return true;
   }

   if ( uCommandType == COMMAND_ID_RESET_ALL_TO_DEFAULTS )
   {
      for( int i=0; i<20; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 10+i);
      for( int i=0; i<5; i++ )
         hardware_sleep_ms(400);

      hw_execute_bash_command("rm -rf /home/pi/ruby/logs", NULL);
      hw_execute_bash_command("rm -rf /home/pi/ruby/media", NULL);
      hw_execute_bash_command("rm -rf /home/pi/ruby/config", NULL);
      hw_execute_bash_command("rm -rf /home/pi/ruby/tmp", NULL);
      hw_execute_bash_command("mkdir -p config", NULL);
      hw_execute_bash_command("touch /home/pi/ruby/config/firstboot.txt", NULL);
      char szBuff[128];
      sprintf(szBuff, "touch %s", LOG_USE_PROCESS);
      hw_execute_bash_command(szBuff, NULL);

      FILE* fd = fopen("config/reset_info.txt", "wt");
      if ( NULL != fd )
      {
         char szName[128];
         strcpy(szName, g_pCurrentModel->vehicle_name);
         if ( 0 == szName[0] )
            strcpy(szName, "*");
         fprintf(fd, "%u %u %d %d %d %s",
           g_pCurrentModel->vehicle_id, g_pCurrentModel->controller_id,
           g_pCurrentModel->radioLinksParams.link_frequency[0],
           g_pCurrentModel->radioLinksParams.link_frequency[1],
           g_pCurrentModel->radioLinksParams.link_frequency[2],
           szName);
         fclose(fd);
      }
      hw_execute_bash_command("sudo reboot -f", NULL);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_CONTROLLER_TELEMETRY_OPTIONS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->telemetry_params.bControllerHasOutputTelemetry = (pPHC->command_param & 0x01)?true:false;
      g_pCurrentModel->telemetry_params.bControllerHasInputTelemetry = (pPHC->command_param & 0x02)?true:false;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(MODEL_CHANGED_CONTROLLER_TELEMETRY);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_GPS_INFO )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->iGPSCount = (int) pPHC->command_param;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_AUDIO_PARAMS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      audio_parameters_t* params = (audio_parameters_t*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      bool bHasAudioDevice = g_pCurrentModel->audio_params.has_audio_device;
      memcpy(&(g_pCurrentModel->audio_params), params, sizeof(audio_parameters_t));
      g_pCurrentModel->audio_params.has_audio_device = bHasAudioDevice;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);

      vehicle_stop_audio_capture(g_pCurrentModel);
      if ( g_pCurrentModel->audio_params.has_audio_device )
         vehicle_launch_audio_capture(g_pCurrentModel);

      signalReloadModel(0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_VIDEO_PARAMS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);

      video_parameters_t* params = (video_parameters_t*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      video_parameters_t oldParams;
      memcpy(&oldParams, &(g_pCurrentModel->video_params) , sizeof(video_parameters_t));
      memcpy(&(g_pCurrentModel->video_params), params, sizeof(video_parameters_t));

      if ( g_pCurrentModel->video_params.videoAdjustmentStrength != oldParams.videoAdjustmentStrength )
      {
         log_line("Changed video adjustment strength from %d to %d", oldParams.videoAdjustmentStrength, g_pCurrentModel->video_params.videoAdjustmentStrength);
         g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
         signalReloadModel(0);
         return true;
      }

      log_line("Received new video params. User selected video link profile change from %s to %s", str_get_video_profile_name(oldParams.user_selected_video_link_profile), str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile));

      log_line("Video flags for video profile %s: %s, %s, %s",
      str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile),
      (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS)?"Retransmissions=On":"Retransmissions=Off",
      (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?"AdaptiveVideo=On":"AdaptiveVideo=Off",
      (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?"AdaptiveUseControllerInfo=On":"AdaptiveUseControllerInfo=Off"
      );

      bool bMustSignalTXVideo = false;
      bool bVideoResolutionChanged = false;
      bool bMustRestartCapture = false;

      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].width = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].height = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].fps = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].keyframe = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe;
         
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].width = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].height = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].keyframe = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe;

      if ( g_pCurrentModel->video_params.user_selected_video_link_profile != oldParams.user_selected_video_link_profile )
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].fps = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps;
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].keyframe = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe;
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].fps = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps;
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].keyframe = g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe;

         if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_BEST_PERF )
            g_pCurrentModel->clock_sync_type = CLOCK_SYNC_TYPE_BASIC;
         if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_HIGH_QUALITY )
            g_pCurrentModel->clock_sync_type = CLOCK_SYNC_TYPE_ADV;
         g_pCurrentModel->clock_sync_type = CLOCK_SYNC_TYPE_NONE;
      }


      int iProfileToCheck = g_pCurrentModel->video_params.user_selected_video_link_profile;

      // Copy the bidirectional video and adaptive video flags to MQ and LQ profiles too

      int retr = ((g_pCurrentModel->video_link_profiles[iProfileToCheck].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS)?1:0;
      int adaptive = ((g_pCurrentModel->video_link_profiles[iProfileToCheck].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
      int useControllerInfo = ((g_pCurrentModel->video_link_profiles[iProfileToCheck].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?1:0;

      if ( retr == 0 )
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS );
      }
      else
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS );
      }

      if ( adaptive == 0 )
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS );
      }
      else
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS );
      }

      if ( useControllerInfo == 0 )
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO );
      }
      else
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO );
      }

      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);

      
      if ( g_pCurrentModel->video_link_profiles[oldParams.user_selected_video_link_profile].width != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].width ||
           g_pCurrentModel->video_link_profiles[oldParams.user_selected_video_link_profile].height != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].height ||
           g_pCurrentModel->video_link_profiles[oldParams.user_selected_video_link_profile].fps != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].fps ||
           g_pCurrentModel->video_link_profiles[oldParams.user_selected_video_link_profile].keyframe != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].keyframe ||
           g_pCurrentModel->video_link_profiles[oldParams.user_selected_video_link_profile].h264profile != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264profile ||
           g_pCurrentModel->video_link_profiles[oldParams.user_selected_video_link_profile].h264level != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264level ||
           g_pCurrentModel->video_link_profiles[oldParams.user_selected_video_link_profile].h264refresh != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264refresh ||
           g_pCurrentModel->video_link_profiles[oldParams.user_selected_video_link_profile].insertPPS != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].insertPPS ||
           g_pCurrentModel->video_link_profiles[oldParams.user_selected_video_link_profile].h264quantization != g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization
         )
         bVideoResolutionChanged = true;

      if ( g_pCurrentModel->video_params.iH264Slices != oldParams.iH264Slices )
         bMustRestartCapture = true;
      if ( g_pCurrentModel->video_params.uExtraFlags != oldParams.uExtraFlags )
         bMustRestartCapture = true;

      if ( (! bVideoResolutionChanged) && (! bMustRestartCapture) )
      if ( g_pCurrentModel->video_params.uMaxAutoKeyframeInterval != oldParams.uMaxAutoKeyframeInterval )
      {
         signalReloadModel(MODEL_CHANGED_DEFAULT_MAX_ADATIVE_KEYFRAME);
         return true;
      }

      signalReloadModel(0);

      if ( (g_pCurrentModel->hasCamera()) && ( g_pCurrentModel->video_params.user_selected_video_link_profile != oldParams.user_selected_video_link_profile ) )
         signalReloadModel(MODEL_CHANGED_USER_SELECTED_VIDEO_PROFILE);
      else
         signalReloadModel(0);

      if ( bVideoResolutionChanged || bMustRestartCapture )
      {
         if ( g_pCurrentModel->hasCamera() )
            sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM,0);
         signalReloadModel(0);
      }
      if ( bMustSignalTXVideo )
      {
         signalTXVideoEncodingChanged();
         signalUserSelectedVideoProfileChanged();
      }
      return true;
   }

   if ( uCommandType == COMMAND_ID_RESET_VIDEO_LINK_PROFILE )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->resetVideoLinkProfiles(g_pCurrentModel->video_params.user_selected_video_link_profile);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalTXVideoEncodingChanged();
      return true;
   }
   
   if ( uCommandType == COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);

      type_video_link_profile oldVideoProfiles[MAX_VIDEO_LINK_PROFILES];
      memcpy(&(oldVideoProfiles[0]), &(g_pCurrentModel->video_link_profiles[0]), MAX_VIDEO_LINK_PROFILES*sizeof(type_video_link_profile));
      u8* pData = pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command);

      memcpy(&(g_pCurrentModel->video_link_profiles[0]), pData, MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile));
      log_line("Received Video link profiles: %d bytes, Video profile selected by user: %s", 1 + MAX_VIDEO_LINK_PROFILES * sizeof(type_video_link_profile), str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile) );

      log_line("Received video flags for video profile %s: %s, %s, %s",
      str_get_video_profile_name(g_pCurrentModel->video_params.user_selected_video_link_profile),
      (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS)?"Retransmissions=On":"Retransmissions=Off",
      (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?"AdaptiveVideo=On":"AdaptiveVideo=Off",
      (g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?"AdaptiveUseControllerInfo=On":"AdaptiveUseControllerInfo=Off"
      );

      int iProfileToCheck = g_pCurrentModel->video_params.user_selected_video_link_profile;

      // Copy the bidirectional video and adaptive video flags to MQ and LQ profiles too

      int retr = ((g_pCurrentModel->video_link_profiles[iProfileToCheck].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS)?1:0;
      int adaptive = ((g_pCurrentModel->video_link_profiles[iProfileToCheck].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS)?1:0;
      int useControllerInfo = ((g_pCurrentModel->video_link_profiles[iProfileToCheck].encoding_extra_flags) & ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO)?1:0;

      if ( retr == 0 )
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS );
      }
      else
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ENABLE_RETRANSMISSIONS );
      }

      if ( adaptive == 0 )
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS );
      }
      else
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ENABLE_ADAPTIVE_VIDEO_LINK_PARAMS );
      }

      if ( useControllerInfo == 0 )
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags & (~ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO );
      }
      else
      {
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO );
         g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags = g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags | (ENCODING_EXTRA_FLAG_ADAPTIVE_VIDEO_LINK_USE_CONTROLLER_INFO_TOO );
      }
   
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].width = g_pCurrentModel->video_link_profiles[iProfileToCheck].width;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].height = g_pCurrentModel->video_link_profiles[iProfileToCheck].height;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].fps = g_pCurrentModel->video_link_profiles[iProfileToCheck].fps;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].keyframe = g_pCurrentModel->video_link_profiles[iProfileToCheck].keyframe;
      
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].width = g_pCurrentModel->video_link_profiles[iProfileToCheck].width;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].height = g_pCurrentModel->video_link_profiles[iProfileToCheck].height;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].fps = g_pCurrentModel->video_link_profiles[iProfileToCheck].fps;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].keyframe = g_pCurrentModel->video_link_profiles[iProfileToCheck].keyframe;
      
      u32 retransmissionWindow = ((g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].encoding_extra_flags & 0xFF00) >> 8);
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags &= 0xFFFF00FF;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_MQ].encoding_extra_flags |= (retransmissionWindow<<8);
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags &= 0xFFFF00FF;
      g_pCurrentModel->video_link_profiles[VIDEO_PROFILE_LQ].encoding_extra_flags |= (retransmissionWindow<<8);

      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);

      if ( videoLinkProfileIsOnlyVideoKeyframeChanged(&oldVideoProfiles[iProfileToCheck], &g_pCurrentModel->video_link_profiles[iProfileToCheck]) )
      {
         log_line("[RX Commands]: Changed only user selected video profile keyframe interval.");
         signalReloadModel(MODEL_CHANGED_VIDEO_KEYFRAME);
         return true;
      }

      if ( videoLinkProfileIsOnlyAdaptiveVideoChanged(&oldVideoProfiles[iProfileToCheck], &g_pCurrentModel->video_link_profiles[iProfileToCheck]) )
      {
         log_line("[RX Commands]: Changed only user selected video profile adaptive video link flags.");
         signalReloadModel(MODEL_CHANGED_ADAPTIVE_VIDEO_FLAGS);
         return true;
      }

      if ( videoLinkProfileIsOnlyECSchemeChanged(&oldVideoProfiles[iProfileToCheck], &g_pCurrentModel->video_link_profiles[iProfileToCheck]) )
      {
         log_line("[RX Commands]: Changed EC scheme");
         signalReloadModel(MODEL_CHANGED_EC_SCHEME);
         return true;
      }

      if ( videoLinkProfileIsOnlyBitrateChanged(&oldVideoProfiles[iProfileToCheck], &g_pCurrentModel->video_link_profiles[iProfileToCheck]) )
      {
         log_line("[RX Commands]: Changed only bitrate.");
         if ( g_pCurrentModel->isCameraCSICompatible() || g_pCurrentModel->isCameraHDMI() || g_pCurrentModel->isCameraVeye() )
         {
            log_line("[RX Commands]: Signal bitrate change on the fly.");
            signalReloadModel(MODEL_CHANGED_VIDEO_BITRATE);
         }
         else if ( g_pCurrentModel->hasCamera() )
         {          
            log_line("[RX Commands]: Signal bitrate change by capture restart.");
            signalReloadModel(MODEL_CHANGED_VIDEO_BITRATE);
            sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM,0);
         }
         return true;
      }

      signalReloadModel(0);

      if ( true )
      {   
         if ( g_pCurrentModel->hasCamera() )
            sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM,0);
         signalReloadModel(0);
      }
      if ( true )
         signalTXVideoEncodingChanged();

      return true;
   }


   if ( uCommandType == COMMAND_ID_SET_VIDEO_H264_QUANTIZATION )
   {
      if ( pPHC->command_param > 0 )
         g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization = (int)pPHC->command_param;
      else if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization > 0 )
         g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization = - g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization;
      log_line("Received command to set h264 quantization to: %d", g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      signalReloadModel(MODEL_CHANGED_VIDEO_H264_QUANTIZATION);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_OSD_PARAMS )
   {
      osd_parameters_t* params = (osd_parameters_t*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      //log_line("Received osd params %d bytes, expected %d bytes", dataLength, sizeof(osd_parameters_t));
      memcpy(&g_pCurrentModel->osd_params, params, sizeof(osd_parameters_t));
      int nOSDIndex = g_pCurrentModel->osd_params.layout;
      if ( nOSDIndex < 0 || nOSDIndex >= MODEL_MAX_OSD_PROFILES )
         nOSDIndex = 0;

      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      log_line("Saved new OSD params, current layout: %d, enabled: %s", nOSDIndex, (g_pCurrentModel->osd_params.osd_flags2[nOSDIndex] & OSD_FLAG_EXT_LAYOUT_ENABLED)?"yes":"no");
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      signalReloadModel(0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_ALARMS_PARAMS )
   {
      type_alarms_parameters* params = (type_alarms_parameters*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      memcpy(&g_pCurrentModel->alarms_params, params, sizeof(type_alarms_parameters));
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      signalReloadModel(0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_OSD_CURRENT_LAYOUT )
   {
      static u32 s_uLastCommandIdSetOSDLayout = MAX_U32;
      if ( pPHC->command_counter == s_uLastCommandIdSetOSDLayout )
         return true;
      
      s_uLastCommandIdSetOSDLayout = pPHC->command_counter;

      int nOSDIndex = pPHC->command_param;
      if ( nOSDIndex < 0 || nOSDIndex >= MODEL_MAX_OSD_PROFILES )
         nOSDIndex = 0;
      g_pCurrentModel->osd_params.layout = nOSDIndex;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      log_line("Saved received new active OSD layout layout: %d, enabled: %s", nOSDIndex, (g_pCurrentModel->osd_params.osd_flags2[nOSDIndex] & OSD_FLAG_EXT_LAYOUT_ENABLED)?"yes":"no");
      signalReloadModel(0);
      return true;    
   }

   if ( uCommandType == COMMAND_ID_SET_SERIAL_PORTS_INFO )
   {
      log_line("Received new serial ports info");
      model_hardware_info_t* pNewInfo = (model_hardware_info_t*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      g_pCurrentModel->hardware_info.serial_bus_count = pNewInfo->serial_bus_count;
      for( int i=0; i<pNewInfo->serial_bus_count; i++ )
      {
         strcpy(g_pCurrentModel->hardware_info.serial_bus_names[i], pNewInfo->serial_bus_names[i]);
         g_pCurrentModel->hardware_info.serial_bus_speed[i] = pNewInfo->serial_bus_speed[i];
         g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] = pNewInfo->serial_bus_supported_and_usage[i];
      }

      int iCount = g_pCurrentModel->hardware_info.serial_bus_count;
      if ( iCount > hardware_get_serial_ports_count() )
         iCount = hardware_get_serial_ports_count();

      int iSiKPortToUpdate = -1;
      int iSikPortSpeedToUse = -1;   
      for( int i=0; i<iCount; i++ )
      {
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);
         if ( NULL == pPortInfo )
            continue;

         if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_HARDWARE_RADIO )
         if ( pPortInfo->lPortSpeed != g_pCurrentModel->hardware_info.serial_bus_speed[i] )
         if ( hardware_serial_is_sik_radio(pPortInfo->szPortDeviceName) )
         {
            // Changed the serial speed of a SiK radio interface
            log_line("Received command to change the serial speed of a SiK radio interface. Serial port: [%s], old speed: %ld bps, new speed: %d bps",
               pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed,
               g_pCurrentModel->hardware_info.serial_bus_speed[i] );
            iSikPortSpeedToUse = (int)pPortInfo->lPortSpeed;
            iSiKPortToUpdate = i;
         }

         pPortInfo->lPortSpeed = g_pCurrentModel->hardware_info.serial_bus_speed[i];
         pPortInfo->iPortUsage = (int)(g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & 0xFF);
      }
      hardware_serial_save_configuration();

      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      for( int i=0; i<5; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 20);

      signalReloadModel(MODEL_CHANGED_SERIAL_PORTS);
      
      if ( -1 != iSiKPortToUpdate )
      {
         hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(iSiKPortToUpdate);
         if ( NULL != pPortInfo )
         {
            radio_hw_info_t* pSiKRadio = hardware_radio_sik_get_from_serial_port(pPortInfo->szPortDeviceName);
            if ( NULL != pSiKRadio )
            {
               for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
               {
                  radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
                  if ( pRadioInfo == pSiKRadio )
                  {
                     log_line("Notify router that the serial speed of a Sik Radio must be changed.");
                     t_packet_header PH;
                     PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
                     PH.packet_type =  PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SET_SIK_RADIO_SERIAL_SPEED;
                     PH.vehicle_id_src = (u32)i;
                     PH.vehicle_id_dest = (u32)iSikPortSpeedToUse;
                     PH.total_headers_length = sizeof(t_packet_header);
                     PH.total_length = sizeof(t_packet_header);
                     PH.extra_flags = 0;
                     u8 buffer[MAX_PACKET_TOTAL_SIZE];
                     memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
                     packet_compute_crc(buffer, PH.total_length);
                     ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);

                     if ( NULL != g_pProcessStats )
                        g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
                     if ( NULL != g_pProcessStats )
                        g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
                     break;
                  }
               }
            }
         }
      }
      return true;      
   }

   if ( uCommandType == COMMAND_ID_SET_TELEMETRY_PARAMETERS )
   {
      telemetry_parameters_t* params = (telemetry_parameters_t*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      log_line("Received telemetry params %d bytes, expected %d bytes", dataLength, sizeof(telemetry_parameters_t));
      memcpy(&g_pCurrentModel->telemetry_params, params, sizeof(telemetry_parameters_t));
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      //vehicle_stop_tx_telemetry();
      //vehicle_launch_tx_telemetry(g_pCurrentModel);
      signalReloadModel(0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_REBOOT )
   {
      for( int i=0; i<10; i++ )
      {
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
         hardware_sleep_ms(10);
      }
      hardware_sleep_ms(400);
      signalReboot();
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_VEHICLE_NAME )
   {
      char* szName = (char*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      strcpy(g_pCurrentModel->vehicle_name, szName);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      return true;
   }

   // TO FIX
   if ( uCommandType == COMMAND_ID_SET_ALL_PARAMS )
   {
      /*
      int length = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command);
      u8* pData = pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command);
      log_line("Received all model settings. Model file size: %d", length);
      FILE* fd = fopen(FILE_CURRENT_VEHICLE_MODEL, "wb");
      if ( NULL != fd )
      {
          fwrite(pData, 1, length, fd);
          fclose(fd);
          fd = NULL;
      }
      else
      {
         log_error_and_alarm("Failed to save received vehicle configuration to file: %s", FILE_CURRENT_VEHICLE_MODEL);
         sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
         return true;
      }
         
      u32 vid = g_pCurrentModel->vehicle_id;
      u32 ctrlId = g_pCurrentModel->controller_id;
      int boardType = g_pCurrentModel->board_type;
      bool bDev = g_pCurrentModel->bDeveloperMode;
      u8 cameraType = g_pCurrentModel->camera_type;

      bool bHasAudioDevice = g_pCurrentModel->audio_params.has_audio_device;

      int nicCount = g_pCurrentModel->nic_count;
      int iFr[MAX_RADIO_INTERFACES];
      u32 iType[MAX_RADIO_INTERFACES];
      u8 iBands[MAX_RADIO_INTERFACES];
      u8 iFlags[MAX_RADIO_INTERFACES];
      char iszMAC[MAX_RADIO_INTERFACES][MAX_MAC_LENGTH];
      char iszPort[MAX_RADIO_INTERFACES][3];
      for( int i=0; i<g_pCurrentModel->nic_count; i++ )
      {
         iFr[i] = g_pCurrentModel->nic_frequency[i];
         iType[i] = g_pCurrentModel->nic_type_and_driver[i];
         iBands[i] = g_pCurrentModel->nic_supported_bands[i];
         iFlags[i] = g_pCurrentModel->nic_flags[i];
         strcpy(iszMAC[i], g_pCurrentModel->nic_szMAC[i]);
         strcpy(iszPort[i], g_pCurrentModel->nic_szPort[i]);
      }

      g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true);

      g_pCurrentModel->nic_count = nicCount;
      for( int i=0; i<g_pCurrentModel->nic_count; i++ )
      {
         g_pCurrentModel->nic_frequency[i] = iFr[i];
         g_pCurrentModel->nic_type_and_driver[i] = iType[i];
         g_pCurrentModel->nic_supported_bands[i] = iBands[i];
         g_pCurrentModel->nic_flags[i] = iFlags[i];
         strcpy(g_pCurrentModel->nic_szMAC[i], iszMAC[i]);
         strcpy(g_pCurrentModel->nic_szPort[i], iszPort[i]);
      }
      g_pCurrentModel->vehicle_id = vid;
      g_pCurrentModel->controller_id = ctrlId;
      g_pCurrentModel->board_type = boardType;
      g_pCurrentModel->bDeveloperMode = bDev;
      g_pCurrentModel->camera_type = cameraType;

      g_pCurrentModel->audio_params.has_audio_device = bHasAudioDevice;

      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);

      video_overwrites_init( &s_CurrentVideoLinkOverwrites, g_pCurrentModel );

      vehicle_stop_tx_router();
      vehicle_stop_audio_capture(g_pCurrentModel);
      if ( g_pCurrentModel->hasCamera() )
         vehicle_stop_video_capture(g_pCurrentModel); 

      if ( g_pCurrentModel->audio_params.has_audio_device )
         vehicle_launch_audio_capture(g_pCurrentModel);
      if ( g_pCurrentModel->hasCamera() )
      {
         if ( g_pCurrentModel->isCameraHDMI() )
            hardware_sleep_ms(800);
         vehicle_launch_video_capture(g_pCurrentModel, NULL);
      }
      vehicle_launch_tx_router(g_pCurrentModel);
      signalReloadModel(0);
      return true;
      */
   }

   if ( uCommandType == COMMAND_ID_GET_ALL_PARAMS_ZIP )
   {
      bool restartVideo = false;
      bool bTelemetryChanged = false;
      bool bTelemetryOut = false;
      bool bTelemetryIn = false;
      bool bSave = false;
      bool bNotifyChanged = false;
      log_line("Received command to get all params as zip file.");
      bool devMode = (pPHC->command_param & 0x01)? true:false;
      if ( devMode != g_pCurrentModel->bDeveloperMode )
      {
         log_line("Developer Mode value changed to: %s. Updated model.", devMode?"Enabled":"Disabled");
         g_pCurrentModel->bDeveloperMode = devMode;
         restartVideo = true;
         bSave = true;
         bNotifyChanged = true;
      }

      if ( (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS) &&
           (!(pPHC->command_param & (((u32)0x01)<<3))) )
      {
         g_pCurrentModel->uDeveloperFlags &= ~DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS;
         bSave = true;
         bNotifyChanged = true;
         log_line("Disabled vehicle developer video link stats.");
      }
      else if ( (!(g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS)) &&
           (pPHC->command_param & (((u32)0x01)<<3)) )
      {
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS;
         bSave = true;
         bNotifyChanged = true;
         log_line("Enabled vehicle developer video link stats.");
      }

      if ( (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS) &&
           (!(pPHC->command_param & (((u32)0x01)<<4))) )
      {
         g_pCurrentModel->uDeveloperFlags &= ~DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS;
         bSave = true;
         bNotifyChanged = true;
         log_line("Disabled vehicle developer video link graphs.");
      }
      else if ( (!(g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS)) &&
           (pPHC->command_param & (((u32)0x01)<<4)) )
      {
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS;
         bSave = true;
         bNotifyChanged = true;
         log_line("Enabled vehicle developer video link graphs.");
      }

      if ( (pPHC->command_param & (((u32)0x01)<<5)) && (!(g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SEND_FULL_PACKETS_TO_CONTROLLER)) )
      {
         g_pCurrentModel->telemetry_params.flags |= TELEMETRY_FLAGS_SEND_FULL_PACKETS_TO_CONTROLLER;
         bNotifyChanged = true;
         bSave = true;
         log_line("Send full MAVLink/LTM packets to controller was enabnled.");
      }
      else if ( (!(pPHC->command_param & (((u32)0x01)<<5))) && (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SEND_FULL_PACKETS_TO_CONTROLLER) )
      {
         g_pCurrentModel->telemetry_params.flags &= (~TELEMETRY_FLAGS_SEND_FULL_PACKETS_TO_CONTROLLER);
         bNotifyChanged = true;
         bSave = true;
         log_line("Send full MAVLink/LTM packets to controller was disabled (can still be enabled from telemetry menu).");
      }

      if ( pPHC->command_param & (((u32)0x01)<<5) )
         log_line("Send full MAVLink/LTM packets to controller: enabled.");
      else
         log_line("Send full MAVLink/LTM packets to controller: disabled (can still be enabled from telemetry menu).");

      u32 wifiGuardDelay = ((pPHC->command_param>>16) & 0xFF);
      if ( wifiGuardDelay != ((g_pCurrentModel->uDeveloperFlags >> 8) & 0xFF) )
      {
         log_line("Radio Guard Delay value changed (from %d to %d). Updated model.", (int) ((g_pCurrentModel->uDeveloperFlags >> 8) & 0xFF) , (int)wifiGuardDelay);
         g_pCurrentModel->uDeveloperFlags &= 0xFFFF00FF;
         g_pCurrentModel->uDeveloperFlags |= ((wifiGuardDelay & 0xFF)<<8);
         log_line("New guard delay: %d ms", ((g_pCurrentModel->uDeveloperFlags >> 8) & 0xFF) );
         bSave = true;
         bNotifyChanged = true;
      }

      if ( g_pCurrentModel->controller_id != pPH->vehicle_id_src )
      {
         log_line("Controller ID changed. Updating it.");
         bTelemetryChanged = true;
         restartVideo = true;
         bSave = true;
         bNotifyChanged = true;
         g_pCurrentModel->controller_id = pPH->vehicle_id_src;
         FILE* fd = fopen(FILE_CONTROLLER_ID, "w");
         if ( NULL != fd )
         {
            fprintf(fd, "%u\n", g_pCurrentModel->controller_id);
            fclose(fd);
         }
      }

      int controller_mavlink_sys_id = ((pPHC->command_param>>8) & 0xFF);

      if ( controller_mavlink_sys_id <= 0 || controller_mavlink_sys_id > 255 )
         controller_mavlink_sys_id = DEFAULT_MAVLINK_SYS_ID_CONTROLLER;
      if ( controller_mavlink_sys_id != g_pCurrentModel->telemetry_params.controller_mavlink_id )
         bTelemetryChanged = true;
      if ( (pPHC->command_param & ((u32)0x01)<<1) )
         bTelemetryOut = true;
      if ( (pPHC->command_param & ((u32)0x01)<<2) )
         bTelemetryIn = true;
      if ( bTelemetryOut != g_pCurrentModel->telemetry_params.bControllerHasOutputTelemetry )
         bTelemetryChanged = true;
      if ( bTelemetryIn != g_pCurrentModel->telemetry_params.bControllerHasInputTelemetry )
         bTelemetryChanged = true;

      log_line("Current OSD params, current layout: %d, enabled: %s", g_pCurrentModel->osd_params.layout, (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_LAYOUT_ENABLED)?"yes":"no");
      log_line("Current on time: %02d:%02d, current flights: %d", g_pCurrentModel->m_Stats.uCurrentOnTime/60, g_pCurrentModel->m_Stats.uCurrentOnTime%60, g_pCurrentModel->m_Stats.uTotalFlights);

      log_line("Received flags: telemetry in: %d, telemetry out: %d", bTelemetryIn, bTelemetryOut );
      if ( bTelemetryChanged )
      {
         g_pCurrentModel->telemetry_params.controller_mavlink_id = controller_mavlink_sys_id;
         g_pCurrentModel->telemetry_params.bControllerHasOutputTelemetry = bTelemetryOut;
         g_pCurrentModel->telemetry_params.bControllerHasInputTelemetry = bTelemetryIn;
         bSave = true;
      }

      bool bNewZIPCommand = false;

      if ( (lastRecvCommandNumber != s_ZIPParams_uLastRecvCommandNumber) || s_ZIPParams_uLastRecvSourceControllerId == 0 || (s_ZIPParams_uLastRecvSourceControllerId != lastRecvSourceControllerId) || s_ZIPPAarams_uLastRecvCommandTime == 0 || (lastRecvCommandTime > s_ZIPPAarams_uLastRecvCommandTime + 4000) )
      {
         bNewZIPCommand = true;
         s_ZIPParams_uLastRecvSourceControllerId = lastRecvSourceControllerId;
         s_ZIPParams_uLastRecvCommandNumber = lastRecvCommandNumber;
         s_ZIPPAarams_uLastRecvCommandTime = lastRecvCommandTime;
         log_line("New command for getting zip params, regenerate data.");
      }
      else
         log_line("Reuse command for getting zip params, use cached data.");

      if ( bNewZIPCommand )
      if ( _populate_camera_name() )
         bSave = true;

      if ( bSave )
         g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      if ( bTelemetryChanged || bNotifyChanged )
         signalReloadModel(0);

      log_line("Current OSD params, current layout: %d, enabled: %s", g_pCurrentModel->osd_params.layout, (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_LAYOUT_ENABLED)?"yes":"no");
      log_line("Current on time: %02d:%02d, current flights: %d", g_pCurrentModel->m_Stats.uCurrentOnTime/60, g_pCurrentModel->m_Stats.uCurrentOnTime%60, g_pCurrentModel->m_Stats.uTotalFlights);

      if ( bNewZIPCommand )
      {
         hw_execute_bash_command("rm -rf tmp/model.tar 2>/dev/null", NULL);
         hw_execute_bash_command("rm -rf tmp/model.mdl 2>/dev/null", NULL);
         sprintf(szBuff, "cp -rf %s tmp/model.mdl 2>/dev/null", FILE_CURRENT_VEHICLE_MODEL);
         hw_execute_bash_command(szBuff, NULL);
         hw_execute_bash_command("tar -czf tmp/model.tar tmp/model.mdl", NULL);

         s_ZIPParams_Model_BufferLength = 0;
         FILE* fd = fopen("tmp/model.tar", "rb");
         if ( NULL != fd )
         {
            s_ZIPParams_Model_BufferLength = fread(s_ZIPParams_Model_Buffer, 1, 2500, fd);
            fclose(fd);
         }
         else
            log_error_and_alarm("Failed to load current vehicle configuration from file: model.tar");

         hw_execute_bash_command("rm -rf tmp/model.tar", NULL);
         hw_execute_bash_command("rm -rf tmp/model.mdl", NULL);

         if ( 0 == s_ZIPParams_Model_BufferLength || s_ZIPParams_Model_BufferLength > MAX_PACKET_PAYLOAD )
         {
            log_softerror_and_alarm("Invalid compressed model file size (%d). Skipping sending it to controller.", s_ZIPParams_Model_BufferLength); 
            sendCommandReply(COMMAND_RESPONSE_FLAGS_FAILED, 0);
            return true;
         }
      }

      log_line("Sending back to controller all model settings. Compressed size: %d bytes", s_ZIPParams_Model_BufferLength);
      setCommandReplyBuffer(s_ZIPParams_Model_Buffer, s_ZIPParams_Model_BufferLength);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);

      if ( restartVideo )
      if ( g_pCurrentModel->hasCamera() )
         sendControlMessage(PACKET_TYPE_LOCAL_CONTROL_RESTART_VIDEO_PROGRAM,0);

      return true;
   }

   if ( uCommandType == COMMAND_ID_GET_CURRENT_VIDEO_CONFIG )
   {
      u8 buffer[2048];
      int length = 0;
      FILE* fd = fopen(FILE_TMP_CURRENT_VIDEO_PARAMS, "rb");
      if ( NULL != fd )
      {
         length = fread(buffer, 1, 2000, fd);
         fclose(fd);
      }
      else
         log_error_and_alarm("Failed to load current video config from log file: %s", FILE_TMP_CURRENT_VIDEO_PARAMS);
      buffer[length] = 0;
      length++;

      setCommandReplyBuffer(buffer, length);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      return true;
   }


   if ( uCommandType == COMMAND_ID_SET_TX_POWERS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      u8* pData = pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command);
      u8 txPowerGlobal = *pData;
      pData++;
      u8 txPowerAtheros = *pData;
      pData++;
      u8 txPowerRTL = *pData;
      pData++;
      u8 txMaxPower = *pData;
      pData++;
      u8 txMaxPowerAtheros = *pData;
      pData++;
      u8 txMaxPowerRTL = *pData;
      pData++;
      u8 cardIndex = *pData;
      pData++;
      u8 cardPower = *pData;
      pData++;

      log_line("Received new radio power levels: G: %d, Atheros: %d, RTL: %d, MaxG: %d, MaxAth: %d, MaxRTL: %d", txPowerGlobal, txPowerAtheros, txPowerRTL, txMaxPower, txMaxPowerAtheros, txMaxPowerRTL);
      if ( txMaxPower > 0 )
         g_pCurrentModel->radioInterfacesParams.txMaxPower = txMaxPower;
      if ( txMaxPowerAtheros > 0 )
         g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros = txMaxPowerAtheros;
      if ( txMaxPowerRTL > 0 )
         g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL = txMaxPowerRTL;

      if ( (txPowerGlobal > 0) && (txPowerAtheros == 0) && (txPowerRTL == 0) )
      {
         txPowerAtheros = txPowerGlobal;
         txPowerRTL = txPowerGlobal;
      }
      if ( txPowerGlobal > 0 )
         g_pCurrentModel->radioInterfacesParams.txPower = txPowerGlobal;
      if ( txPowerAtheros > 0 )
         g_pCurrentModel->radioInterfacesParams.txPowerAtheros = txPowerAtheros;
      if ( txPowerRTL > 0 )
         g_pCurrentModel->radioInterfacesParams.txPowerRTL = txPowerRTL;

      if ( g_pCurrentModel->radioInterfacesParams.txPower > g_pCurrentModel->radioInterfacesParams.txMaxPower )
         g_pCurrentModel->radioInterfacesParams.txPower = g_pCurrentModel->radioInterfacesParams.txMaxPower;
      if ( g_pCurrentModel->radioInterfacesParams.txPowerAtheros > g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros )
         g_pCurrentModel->radioInterfacesParams.txPowerAtheros = g_pCurrentModel->radioInterfacesParams.txMaxPowerAtheros;
      if ( g_pCurrentModel->radioInterfacesParams.txPowerRTL > g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL )
         g_pCurrentModel->radioInterfacesParams.txPowerRTL = g_pCurrentModel->radioInterfacesParams.txMaxPowerRTL;

      system("sudo mount -o remount,rw /");
      system("sudo mount -o remount,rw /boot");

      if ( txPowerAtheros > 0 )
      {
         hardware_set_radio_tx_power_atheros((int)txPowerAtheros);
      }
      if ( txPowerRTL > 0 )
      {
         hardware_set_radio_tx_power_rtl((int)txPowerRTL);
      }

      if ( cardPower > 0 )
      if ( cardIndex < g_pCurrentModel->radioInterfacesParams.interfaces_count )
         g_pCurrentModel->radioInterfacesParams.interface_power[cardIndex] = cardPower;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(MODEL_CHANGED_RADIO_POWERS);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RADIO_SLOTTIME )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->radioInterfacesParams.slotTime = pPHC->command_param;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      //sprintf(szBuff, "cp /etc/modprobe.d/ath9k_hw.conf tmp/; sed -i 's/slottime=[0-9]*/slottime=%d/g' tmp/ath9k_hw.conf; cp tmp/ath9k_hw.conf /etc/modprobe.d/", pPHC->command_param );
      //hw_execute_bash_command(szBuff, NULL);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RADIO_THRESH62 )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->radioInterfacesParams.thresh62 = pPHC->command_param;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      //sprintf(szBuff, "cp /etc/modprobe.d/ath9k_hw.conf tmp/; sed -i 's/thresh62=[0-9]*/thresh62=%d/g' tmp/ath9k_hw.conf; cp tmp/ath9k_hw.conf /etc/modprobe.d/", pPHC->command_param );
      //hw_execute_bash_command(szBuff, NULL);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_ENABLE_DHCP)
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->enableDHCP = (bool)(pPHC->command_param);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      hardware_mount_boot();
      hardware_sleep_ms(200);
      if ( g_pCurrentModel->enableDHCP )
      {
         log_line("Enabling DHCP");
         hw_execute_bash_command("rm -f /boot/nodhcp", NULL);
      }
      else
      {
         log_line("Disabling DHCP");
         hw_execute_bash_command("echo '1' > /boot/nodhcp", NULL);
      }
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_RC_PARAMS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      rc_parameters_t* params = (rc_parameters_t*)(pBuffer + sizeof(t_packet_header)+sizeof(t_packet_header_command));
      memcpy(&g_pCurrentModel->rc_params, params, sizeof(rc_parameters_t));
      log_dword("Received RC flags: ", g_pCurrentModel->rc_params.flags);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      return true;
   }


   if ( uCommandType == COMMAND_ID_SET_NICE_VALUE_TELEMETRY )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->niceTelemetry = ((int)((pPHC->command_param) % 256))-20;
      if ( g_pCurrentModel->niceTelemetry < -16 )
         g_pCurrentModel->niceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY;
      if ( g_pCurrentModel->niceTelemetry > 0 )
         g_pCurrentModel->niceTelemetry = DEFAULT_PRIORITY_PROCESS_TELEMETRY;
      log_line("Received nice value for telemetry: %d", g_pCurrentModel->niceTelemetry);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      update_priorities();
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_NICE_VALUES )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->niceVideo = ((int)((pPHC->command_param) % 256))-20;
      g_pCurrentModel->niceOthers = ((int)(((pPHC->command_param)>>8) % 256))-20;
      g_pCurrentModel->niceRouter = ((int)(((pPHC->command_param)>>16) % 256))-20;
      g_pCurrentModel->niceRC = ((int)(((pPHC->command_param)>>24) % 256))-20;
      if ( g_pCurrentModel->niceRouter < -18 )
         g_pCurrentModel->niceRouter = -18;
      if ( g_pCurrentModel->niceRC < -18 )
         g_pCurrentModel->niceRC = -18;
      log_line("Received nice values: video: %d, router: %d, rc: %d, others: %d", g_pCurrentModel->niceVideo, g_pCurrentModel->niceRouter, g_pCurrentModel->niceRC, g_pCurrentModel->niceOthers);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      update_priorities();
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_IONICE_VALUES )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->ioNiceVideo = ((int)((pPHC->command_param)%256))-20;
      g_pCurrentModel->ioNiceRouter = ((int)(((pPHC->command_param)>>8)%256))-20;
      log_line("Received io nice values: video: %d, router: %d", g_pCurrentModel->ioNiceVideo, g_pCurrentModel->ioNiceRouter);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      update_priorities();
      return true;
   }

   if ( uCommandType == COMMAND_ID_DEBUG_GET_TOP )
   {
      char szOutput[1024];
      hw_execute_bash_command_raw("top -bn1 | grep -A 1 'CPU\\|ruby\\|raspi'", szOutput);
      log_line("TOP output: %s", szOutput);
      setCommandReplyBuffer((u8*)szOutput, strlen(szOutput)+1);
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_ENABLE_DEBUG )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->bDeveloperMode = (bool)(pPHC->command_param);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      hardware_sleep_ms(50);
      vehicle_stop_tx_telemetry();
      vehicle_launch_tx_telemetry(g_pCurrentModel);
      return true;
   }

   if ( uCommandType == COMMAND_ID_SET_DEVELOPER_FLAGS )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      g_pCurrentModel->uDeveloperFlags = pPHC->command_param;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_ENABLE_LIVE_LOG )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      if ( 0 != pPHC->command_param )
         g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_LIVE_LOG;
      else
         g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_LIVE_LOG);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      signalReloadModel(0);
      return true;
   }

   if ( uCommandType == COMMAND_ID_UPLOAD_SW_TO_VEHICLE )
   {
      process_sw_upload_old(pPHC->command_param, pBuffer, length);
      return true;
   }

   if ( uCommandType == COMMAND_ID_UPLOAD_SW_TO_VEHICLE63 )
   {
      process_sw_upload_new(pPHC->command_param, pBuffer, length);
      return true;
   }

   if ( uCommandType == COMMAND_ID_RESET_ALL_DEVELOPER_FLAGS )
   {
      for( int i=0; i<20; i++ )
         sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 20);

      g_pCurrentModel->uDeveloperFlags = (((u32)DEFAULT_DELAY_WIFI_CHANGE)<<8);

      for( int i=0; i<MAX_VIDEO_LINK_PROFILES; i++ )
      {
         g_pCurrentModel->video_link_profiles[i].encoding_extra_flags |= ENCODING_EXTRA_FLAG_RETRANSMISSIONS_DUPLICATION_PERCENT_AUTO;
 
         g_pCurrentModel->video_link_profiles[i].encoding_extra_flags &= (~0xFF00);
         g_pCurrentModel->video_link_profiles[i].encoding_extra_flags |= (DEFAULT_VIDEO_RETRANS_MS5_HP<<8);
      }

      g_pCurrentModel->resetVideoLinkProfiles(VIDEO_PROFILE_MQ);
      g_pCurrentModel->resetVideoLinkProfiles(VIDEO_PROFILE_LQ);
      g_pCurrentModel->video_params.videoAdjustmentStrength = DEFAULT_VIDEO_PARAMS_ADJUSTMENT_STRENGTH;

      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      hardware_sleep_ms(400);
      hw_execute_bash_command("sudo reboot -f", NULL); 
      return true;
   }

   if ( uCommandType == COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_LOW )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      signalForceVideoProfile(VIDEO_PROFILE_LQ);
      return true;
   }

   if ( uCommandType == COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_MED )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      signalForceVideoProfile(VIDEO_PROFILE_MQ);
      return true;
   }

   if ( uCommandType == COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_NORMAL )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      signalForceVideoProfile(0xFF);
      return true;
   }

   if ( uCommandType == COMMAND_ID_MANUAL_SWITCH_TO_VIDEO_LINK_QUALITY_AUTO )
   {
      sendCommandReply(COMMAND_RESPONSE_FLAGS_OK, 0);
      signalForceVideoProfile(0xFF);
      return true;
   }

   return false;
}


void on_received_command(u8* pBuffer, int length)
{
   if ( length < (int)sizeof(t_packet_header) + (int)sizeof(t_packet_header_command) )
   {
      log_line_commands("Received a invalid command (packet to small). Ignoring.");
      log_softerror_and_alarm("Received a invalid command (packet to small). Ignoring.");
      return;
   }

   t_packet_header* pPH = (t_packet_header*)pBuffer;
   t_packet_header_command* pPHC = (t_packet_header_command*)(pBuffer + sizeof(t_packet_header));

   if ( pPH->vehicle_id_dest != g_pCurrentModel->vehicle_id )
      return;

   if ( ! packet_check_crc(pBuffer, pPH->total_length) )
   {
      log_line_commands("Received a invalid command (invalid CRC). Ignoring.");
      return;
   }

   // Ignore commands that are resent multiple times
   // Respond to same command retry.
   // Except for commands that do not requires a response, those just process them. (COMMAND_ID_SET_OSD_CURRENT_LAYOUT)

   if ( (pPHC->command_type & COMMAND_TYPE_MASK) != COMMAND_ID_SET_OSD_CURRENT_LAYOUT )
   if ( pPHC->command_counter == lastRecvCommandNumber )
   if ( (lastRecvCommandTime != 0) && (g_TimeNow >= lastRecvCommandTime) && (g_TimeNow < lastRecvCommandTime+4000) )
   {
      if ( pPHC->command_resend_counter > lastRecvCommandResendCounter )
      {
         log_line_commands("Received command (current vehicle UID: %u) nb.%d, retry count: %d, command type: %d: %s, command param: %u, extra info size: %d", g_pCurrentModel->vehicle_id, pPHC->command_counter, pPHC->command_resend_counter, pPHC->command_type & COMMAND_TYPE_MASK, commands_get_description(((pPHC->command_type) & COMMAND_TYPE_MASK)), pPHC->command_param, length-sizeof(t_packet_header)-sizeof(t_packet_header_command));
         if ( ! (((pPHC->command_type) & COMMAND_TYPE_MASK) & COMMAND_TYPE_FLAG_NO_RESPONSE_NEEDED) )
         {
            log_line("Resending command response, for command id: %d", lastRecvCommandNumber);
            lastRecvCommandResendCounter = pPHC->command_resend_counter;
            sendCommandReply(lastRecvCommandResponseFlags, 0);
         }
      }
      else if (pPHC->command_counter > 1 )
         log_line_commands("Ignoring command (duplicate) (current vehicle UID: %u) nb.%d, retry count: %d, command type: %s, command param: %u, extra info size: %d", g_pCurrentModel->vehicle_id, pPHC->command_counter, lastRecvCommandResendCounter, commands_get_description(((pPHC->command_type) & COMMAND_TYPE_MASK)), pPHC->command_param, length-sizeof(t_packet_header)-sizeof(t_packet_header_command));

      if ( pPHC->command_resend_counter > lastRecvCommandResendCounter || pPHC->command_counter > 1 )
         return;
   }

   log_line_commands("Received command (current vehicle UID: %u) nb.%d, retry count: %d, command type: %d: %s, command param: %u, extra info size: %d", g_pCurrentModel->vehicle_id, pPHC->command_counter, pPHC->command_resend_counter, pPHC->command_type & COMMAND_TYPE_MASK, commands_get_description(((pPHC->command_type) & COMMAND_TYPE_MASK)), pPHC->command_param, length-sizeof(t_packet_header)-sizeof(t_packet_header_command));

   lastRecvSourceControllerId = pPH->vehicle_id_src;
   lastRecvCommandNumber = pPHC->command_counter;
   lastRecvCommandResendCounter = pPHC->command_resend_counter;
   lastRecvCommandType = pPHC->command_type;
   lastRecvCommandTime = g_TimeNow;
   lastRecvCommandResponseFlags = 0;
   lastRecvCommandReplyBufferLength = 0;

   //log_line_commands("Received command (current vehicle UID: %u) nb.%d, command type: %s, command param: %u, extra info size: %d", g_pCurrentModel->vehicle_id, pPHC->command_counter, commands_get_description(((pPHC->command_type) & COMMAND_TYPE_MASK)), pPHC->command_param, length-sizeof(t_packet_header)-sizeof(t_packet_header_command));

   if ( ! process_command(pBuffer, length) )
      sendCommandReply(COMMAND_RESPONSE_FLAGS_UNKNOWN_COMMAND, 0); 
   //log_line("Finished processing command.");   
}

void _periodic_loop()
{
   if ( (g_TimeLastRadioLinkFlagsChange != 0) && s_bWaitForRadioFlagsChangeConfirmation && (s_iRadioLinkIdChangeConfirmation != -1) )
   {
      if ( g_TimeNow >= g_TimeLastRadioLinkFlagsChange + TIMEOUT_RADIO_FRAMES_FLAGS_CHANGE_CONFIRMATION )
      {
         log_softerror_and_alarm("No confirmation received from controller about the last radio flags change on radio link %d", s_iRadioLinkIdChangeConfirmation+1);
         g_TimeLastRadioLinkFlagsChange = 0;
         s_bWaitForRadioFlagsChangeConfirmation = false;

         memcpy(&(g_pCurrentModel->radioLinksParams), &s_LastGoodRadioLinksParams, sizeof(type_radio_links_parameters));

         char szBuffR[128];
         str_get_radio_frame_flags_description(g_pCurrentModel->radioLinksParams.link_radio_flags[s_iRadioLinkIdChangeConfirmation], szBuffR); 
         log_line("Revert radio link flags for link %d to: %s, datarates: %d/%d", s_iRadioLinkIdChangeConfirmation+1, szBuffR, g_pCurrentModel->radioLinksParams.link_datarates[s_iRadioLinkIdChangeConfirmation][0], g_pCurrentModel->radioLinksParams.link_datarates[s_iRadioLinkIdChangeConfirmation][1]);

         // Populate radio interfaces radio flags and rates from radio links radio flags and rates

         g_pCurrentModel->updateRadioInterfacesRadioFlags();
   
         g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);

         log_line("Radio link %d datarates now: %d/%d", s_iRadioLinkIdChangeConfirmation+1, g_pCurrentModel->radioLinksParams.link_datarates[s_iRadioLinkIdChangeConfirmation][0], g_pCurrentModel->radioLinksParams.link_datarates[s_iRadioLinkIdChangeConfirmation][1]);
         
         if ( 0 == s_iRadioLinkIdChangeConfirmation )
            signalReloadModel(MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK0);
         else if ( 1 == s_iRadioLinkIdChangeConfirmation )
            signalReloadModel(MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK1);
         else if ( 2 == s_iRadioLinkIdChangeConfirmation )
            signalReloadModel(MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK2);
         else if ( 3 == s_iRadioLinkIdChangeConfirmation )
            signalReloadModel(MODEL_CHANGED_RADIO_FRAMES_FLAGS_LINK3);

         s_iRadioLinkIdChangeConfirmation = -1;
      }
   }
}

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
} 

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   log_init("RX_Commands");

   s_fIPCFromRouter = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_COMMANDS);
   if ( s_fIPCFromRouter < 0 )
      return -1;

   s_fIPCToRouter = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_COMMANDS_TO_ROUTER);
   if ( s_fIPCToRouter < 0 )
      return -1;
 
   hardware_reload_serial_ports();
   hardware_enumerate_radio_interfaces();

   g_pCurrentModel = new Model();
   if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
   {
      log_error_and_alarm("Can't load current model vehicle. Exiting.");
      return -1;
   }

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
      log_only_errors();

   hw_set_priority_current_proc(g_pCurrentModel->niceOthers);

   g_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_COMMANDS_RX);
   if ( NULL == g_pProcessStats )
      log_softerror_and_alarm("Failed to open shared mem for commands Rx process watchdog for writing: %s", SHARED_MEM_WATCHDOG_COMMANDS_RX);
   else
      log_line("Opened shared mem for commands Rx process watchdog for writing.");

   video_overwrites_init( &s_CurrentVideoLinkOverwrites, g_pCurrentModel );

   process_sw_upload_init();

   s_InfoLastFileUploaded.uLastFileId = MAX_U32;
   s_InfoLastFileUploaded.szFileName[0] = 0;
   s_InfoLastFileUploaded.uFileSize = 0;
   s_InfoLastFileUploaded.uTotalSegments = 0;
   s_InfoLastFileUploaded.uLastCommandIdForThisFile = 0;

   g_TimeNow = get_current_timestamp_ms();
   g_TimeStart = get_current_timestamp_ms();

   log_line("Started. Running now.");
   log_line("------------------------------------------");

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   } 
  
   g_TimeLastPeriodicCheck = get_current_timestamp_ms();
 
   while (!g_bQuit) 
   {
      hardware_sleep_ms(5);
      g_TimeNow = get_current_timestamp_ms();
      u32 tTime0 = g_TimeNow;

      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->uLoopCounter++;
         g_pProcessStats->lastActiveTime = g_TimeNow;
      }

      if ( g_TimeNow > g_TimeLastPeriodicCheck + 300 )
      {
         g_TimeLastPeriodicCheck = g_TimeNow;
         _periodic_loop();
      }

      int maxMsgToRead = 5 + DEFAULT_UPLOAD_PACKET_CONFIRMATION_FREQUENCY;
      while ( (maxMsgToRead > 0) && NULL != ruby_ipc_try_read_message(s_fIPCFromRouter, 40000, s_PipeTmpBufferCommands, &s_PipeTmpBufferCommandsPos, s_BufferCommands) )
      {
         maxMsgToRead--;
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
         t_packet_header* pPH = (t_packet_header*) s_BufferCommands;

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
         if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST )
         {
            u32 uResendCount = 0;
            if ( pPH->total_length >= pPH->total_headers_length + sizeof(u32) )
               memcpy(&uResendCount, &(s_BufferCommands[sizeof(t_packet_header)]), sizeof(u32));
            log_line("Received pairing request from router (received retry counter: %u). CID: %u, VID: %u. Updating local model.", uResendCount, pPH->vehicle_id_src, pPH->vehicle_id_dest);
            if ( NULL != g_pCurrentModel )
               g_pCurrentModel->controller_id = pPH->vehicle_id_src;

            s_InfoLastFileUploaded.uLastCommandIdForThisFile = 0;
         }

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         {
            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
            {
               u8 changeType = (pPH->vehicle_id_src >> 8 ) & 0xFF;
               u8 fromComponentId = (pPH->vehicle_id_src & 0xFF);
               log_line("Received request from router to reload model (from component id: %d, change type: %d.", (int)fromComponentId, (int)changeType);
               if ( changeType == MODEL_CHANGED_STATS )
               {
                  log_line("Loading model including stats.");
                  if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
                     log_error_and_alarm("Can't load current model vehicle.");
               }
               else
               {
                  if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, false) )
                     log_error_and_alarm("Can't load current model vehicle.");
               }
            }
            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS )
            {
               memcpy((u8*)(&(g_pCurrentModel->m_Stats)),(u8*)&(s_BufferCommands[sizeof(t_packet_header)]), sizeof(type_vehicle_stats_info));
            }

            if ( pPH->packet_type == PACKET_TYPE_LOCAL_LINK_FREQUENCY_CHANGED )
            {
               u32* pI = (u32*)((&s_BufferCommands[0])+sizeof(t_packet_header));
               u32 uLinkId = *pI;
               pI++;
               u32 uNewFreq = *pI;
               log_line("Received new radio link frequency from router (radio link %u new freq: %s). Updating local model copy.", uLinkId+1, str_format_frequency(uNewFreq));
               if ( uLinkId < (u32)g_pCurrentModel->radioLinksParams.links_count )
               {
                  g_pCurrentModel->radioLinksParams.link_frequency[uLinkId] = uNewFreq;
                  for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
                  {
                     if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == (int)uLinkId )
                        g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i] = uNewFreq;
                  }
                  log_current_full_radio_configuration(g_pCurrentModel);
               }
               else
                  log_softerror_and_alarm("Invalid parameters for changing radio link frequency. radio link: %u of %d", uLinkId+1, g_pCurrentModel->radioLinksParams.links_count);
            }
            
            if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_UPDATED_VIDEO_LINK_OVERWRITES )
            {
               memcpy(&s_CurrentVideoLinkOverwrites, (&s_BufferCommands[0]) + sizeof(t_packet_header), sizeof(shared_mem_video_link_overwrites));
            }
            continue;
         }

         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
            on_received_command(s_BufferCommands, pPH->total_length);
      }

      u32 tNow = get_current_timestamp_ms();
      if ( NULL != g_pProcessStats )
      {
         if ( g_pProcessStats->uMaxLoopTimeMs < tNow - tTime0 )
            g_pProcessStats->uMaxLoopTimeMs = tNow - tTime0;
         g_pProcessStats->uTotalLoopTime += tNow - tTime0;
         if ( 0 != g_pProcessStats->uLoopCounter )
            g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
      }

      u32 dTime = get_current_timestamp_ms() - tTime0;
      if ( dTime > 150 )
         log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
   }

   log_line("Stopping...");
   
   ruby_close_ipc_channel(s_fIPCFromRouter);
   ruby_close_ipc_channel(s_fIPCToRouter);
   s_fIPCFromRouter = -1;
   s_fIPCToRouter = -1;

   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_COMMANDS_RX, g_pProcessStats);

   log_line("Stopped. Exit");
   log_line("--------------------------");
   return 0;
}
