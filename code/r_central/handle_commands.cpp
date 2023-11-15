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

#include <pthread.h>
#include "../base/launchers.h"
#include "../base/ctrl_settings.h"
#include "../common/string_utils.h"
#include "handle_commands.h"
#include "popup.h"
#include "popup_log.h"
#include "shared_vars.h"
#include "menu.h"
#include "colors.h"
#include "osd_common.h"
#include "osd_plugins.h"
#include "pairing.h"
#include "warnings.h"
#include "pairing.h"
#include "ruby_central.h"
#include "osd.h"
#include "launchers_controller.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "menu_update_vehicle.h"
#include "menu_confirmation.h"
#include "process_router_messages.h"
#include "timers.h"
#include "events.h"


#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <semaphore.h>
#include <math.h>
 
static bool s_bHasCommandInProgress = false;
static bool s_bCommandsCheckForRadioFlagsWorking = false;
static bool s_bCommandsWaitForRadioFlagsChangedConfirmation = false;

static bool s_bHasToSyncCorePluginsInfoFromVehicle = true;
static bool s_bHasReceivedVehicleCorePluginsInfo = false;

#define MAX_COM_SEND_RETRY 5

static u32 s_CommandCounter = 0;
static u32 s_CommandLastProcessedResponseToCommandCounter = MAX_U32;
static u32 s_CommandStartTime = 0;
static u32 s_CommandTimeout = 20;
static u32 s_CommandType = 0;
static u32 s_CommandParam = 0;
static u8  s_CommandResendCounter = 0;
static u8  s_CommandMaxResendCounter = 20;
static u32 s_CommandLastResponseReceivedTime = 0;

static u8  s_CommandBuffer[4096];
static int s_CommandBufferLength = 0;

static u8 s_CommandReplyBuffer[MAX_PACKET_TOTAL_SIZE];
static int s_CommandReplyLength = 0;

static int s_RetryGetSettingsCounter = 0;
static int s_RetryGetCorePluginsCounter = 0;


#define MAX_FILE_SEGMENTS_TO_DOWNLOAD 5000

static u32 s_uFileIdToDownload = 0;
static u8  s_uFileToDownloadState = 0xFF;
static u32 s_uFileToDownloadSegmentSize = 0;
static bool s_bListFileSegmentsToDownload[MAX_FILE_SEGMENTS_TO_DOWNLOAD];
static u16 s_uListFileSegmentsSize[MAX_FILE_SEGMENTS_TO_DOWNLOAD];
static u8* s_pListFileSegments[MAX_FILE_SEGMENTS_TO_DOWNLOAD];
static u32 s_uCountFileSegmentsToDownload = 0;
static u32 s_uCountFileSegmentsDownloaded = 0;
static u32 s_uLastFileSegmentRequestTime = 0;
static u32 s_uLastTimeDownloadProgress = 0;


Menu* s_pMenuVehicleHWInfo = NULL;
Menu* s_pMenuUSBInfoVehicle = NULL;

void update_processes_priorities()
{
   ControllerSettings* pCS = get_ControllerSettings();
   hw_set_proc_priority("ruby_rt_station", pCS->iNiceRouter, pCS->ioNiceRouter, 1);
   hw_set_proc_priority("ruby_tx_rc", g_pCurrentModel->niceRC, DEFAULT_IO_PRIORITY_RC, 1 );
   hw_set_proc_priority("ruby_rx_telemetry", g_pCurrentModel->niceTelemetry, 0, 1 );
   hw_set_proc_priority("ruby_central", pCS->iNiceCentral, 0, 1 );
}

void handle_commands_send_current_command()
{
   if ( NULL == g_pCurrentModel )
      return;

   s_CommandStartTime = g_TimeNow;
   s_bHasCommandInProgress = true;

   t_packet_header PH;
   t_packet_header_command PHC;

   PH.packet_flags = PACKET_COMPONENT_COMMANDS;
   PH.packet_type =  PACKET_TYPE_COMMAND;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_headers_length = sizeof(t_packet_header)+sizeof(t_packet_header_command);
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_command) + s_CommandBufferLength;
   PH.extra_flags = 0;

   PHC.command_type = s_CommandType;
   PHC.command_counter = s_CommandCounter;
   PHC.command_param = s_CommandParam;
   PHC.command_resend_counter = s_CommandResendCounter;
  
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&PHC, sizeof(t_packet_header_command));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_command), s_CommandBuffer, s_CommandBufferLength);
   packet_compute_crc(buffer, PH.total_length);

   send_packet_to_router(buffer, PH.total_length);
 
   log_line_commands("[Commands] [Sent] to vId %u, cmd nb. %d, retry %d, type [%s], param: %u, buff len: %d]", g_pCurrentModel->vehicle_id, s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam, s_CommandBufferLength);
}


bool handle_commands_start_on_pairing()
{
   log_line("[Commands] Start pairing");
   s_RetryGetSettingsCounter = 0;
   s_RetryGetCorePluginsCounter = 0;
   s_CommandLastProcessedResponseToCommandCounter = MAX_U32;
   s_bCommandsCheckForRadioFlagsWorking = false;
   s_bCommandsWaitForRadioFlagsChangedConfirmation = false;

   s_bHasToSyncCorePluginsInfoFromVehicle = true;
   s_bHasReceivedVehicleCorePluginsInfo = false;

   s_uFileIdToDownload = 0;
   s_uFileToDownloadState = 0xFF;
   s_uCountFileSegmentsToDownload = 0;
   s_uCountFileSegmentsDownloaded = 0;
   s_uLastFileSegmentRequestTime = 0;
   s_uLastTimeDownloadProgress = 0;

   for( u32 u=0; u<MAX_FILE_SEGMENTS_TO_DOWNLOAD; u++ )
      s_pListFileSegments[u] = NULL;
   
   return true;
}

bool handle_commands_stop_on_pairing()
{
   s_bCommandsCheckForRadioFlagsWorking = false;
   s_bCommandsWaitForRadioFlagsChangedConfirmation = false;

   if ( 0 < s_uCountFileSegmentsToDownload )
   {
       for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
       {
          if ( NULL != s_pListFileSegments[u] )
             free(s_pListFileSegments[u]);
          s_pListFileSegments[u] = NULL;
       }
   }
   s_uFileIdToDownload = 0;
   s_uFileToDownloadState = 0xFF;
   s_uCountFileSegmentsToDownload = 0;
   s_uCountFileSegmentsDownloaded = 0;
   s_uLastFileSegmentRequestTime = 0;
   s_uLastTimeDownloadProgress = 0;
   return true;
}


u8* handle_commands_get_last_command_response()
{
   return s_CommandReplyBuffer;
}

// Merge from Src to Dest
void merge_osd_params(osd_parameters_t* pParamsVehicleSrc, osd_parameters_t* pParamsCtrlDest)
{
   //printf("\n%d, %d\n", pParamsVehicleSrc->layout, (pParamsVehicleSrc->osd_flags2[pParamsVehicleSrc->layout] & OSD_FLAG_EXT_LAYOUT_ENABLED)?1:0);
   if ( NULL == pParamsVehicleSrc || NULL == pParamsCtrlDest )
      return;
         
   memcpy(pParamsCtrlDest, pParamsVehicleSrc, sizeof(osd_parameters_t));
}


void _handle_download_file_response()
{
   u32 uFileId = s_CommandParam;
   u8* pBuffer = &s_CommandReplyBuffer[0] + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
   t_packet_header_download_file_info* pFileInfo = (t_packet_header_download_file_info*)pBuffer;
   log_line("[Commands]: Received file download request response from vehicle (file id %d, name: [%s]), segments: %u, segment size: %u, file state: %s", pFileInfo->file_id, pFileInfo->szFileName, pFileInfo->segments_count, pFileInfo->segment_size, pFileInfo->isReady?"file ready":"file preprocessing");

   if ( s_uFileToDownloadState == 0xFF )
   {
      warnings_add("Waiting for vehicle to preprocess the file...");
      s_uFileIdToDownload = uFileId;
      s_uFileToDownloadState = pFileInfo->isReady;
   }

   if ( pFileInfo->isReady == 1 )
   {
      s_uLastFileSegmentRequestTime = g_TimeNow;
      s_uFileIdToDownload = uFileId;
      s_uFileToDownloadState = pFileInfo->isReady;
      s_uFileToDownloadSegmentSize = pFileInfo->segment_size;
      s_uCountFileSegmentsToDownload = pFileInfo->segments_count;
      s_uCountFileSegmentsDownloaded = 0;
      if ( s_uCountFileSegmentsToDownload >= MAX_FILE_SEGMENTS_TO_DOWNLOAD )
         s_uCountFileSegmentsToDownload = MAX_FILE_SEGMENTS_TO_DOWNLOAD-1;

      for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
      {
         s_bListFileSegmentsToDownload[u] = true;
         s_uListFileSegmentsSize[u] = 0;
         if ( NULL != s_pListFileSegments[u] )
            free(s_pListFileSegments[u]);
         s_pListFileSegments[u] = (u8*) malloc(s_uFileToDownloadSegmentSize);
      }

      s_uLastTimeDownloadProgress = g_TimeNow;

      if ( pFileInfo->file_id == FILE_ID_VEHICLE_LOGS_ARCHIVE )
      {
         hw_execute_bash_command("rm -rf tmp/vehicle_logs.zip", NULL);
         hw_execute_bash_command("touch tmp/vehicle_logs.zip", NULL);
      }
   }
   else
   {
      s_uFileIdToDownload = uFileId;
      s_uFileToDownloadState = pFileInfo->isReady;
   }

   char szBuff[256];
   szBuff[0] = 0;

   if ( pFileInfo->isReady == 1 )
   {
      if ( pFileInfo->file_id == FILE_ID_VEHICLE_LOGS_ARCHIVE )
         strcpy(szBuff, "Downloading vehicle logs...");  
   }

   if ( 0 != szBuff[0] )
      warnings_add(szBuff);
}


void _handle_download_file_segment_response()
{
   t_packet_header* pPH = (t_packet_header*)s_CommandReplyBuffer;
   int length = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_command_response);
   u8* pBuffer = &s_CommandReplyBuffer[0] + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
   u32 flags = 0;
   memcpy((u8*)&flags, pBuffer, sizeof(u32));
   u32 uFileId = flags & 0xFFFF;
   u32 uFileSegment = (flags>>16);
   
   length -= sizeof(u32);
   pBuffer += sizeof(u32);
   log_line("[Commands]: Received file segment %d of %d from vehicle (for file id %d), lenght: %d bytes.", uFileSegment, s_uCountFileSegmentsToDownload, uFileId, length);

   if ( s_uFileIdToDownload == 0 )
      return;
   if ( s_uFileToDownloadState != 1 )
      return;

   if ( (uFileSegment != s_uCountFileSegmentsToDownload) && (length != s_uFileToDownloadSegmentSize) )
      log_softerror_and_alarm("Received file segment of unexpected size. Expected %d bytes segment.", s_uFileToDownloadSegmentSize );

   if ( uFileSegment < s_uCountFileSegmentsToDownload )
   {
      if ( s_bListFileSegmentsToDownload[uFileSegment] )
         s_uCountFileSegmentsDownloaded++;
      s_bListFileSegmentsToDownload[uFileSegment] = false;
      s_uListFileSegmentsSize[uFileSegment] = (u16)length;
      if ( NULL != s_pListFileSegments[uFileSegment] )
         memcpy(s_pListFileSegments[uFileSegment], pBuffer, length);
   }

   bool bHasMoreSegmentsToDownload = false;
   u32 uTotalSize = 0;
   for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
   {
      if ( s_bListFileSegmentsToDownload[u] )
      {
         bHasMoreSegmentsToDownload = true;
         break;
      }
      uTotalSize += (u32) s_uListFileSegmentsSize[u];
   }

   if ( g_TimeNow > s_uLastTimeDownloadProgress + 5000 )
   {
      s_uLastTimeDownloadProgress = g_TimeNow;
      char szBuff[128];
      if ( s_uCountFileSegmentsToDownload > 0 )
         snprintf(szBuff, sizeof(szBuff), "Downloading %d%%", s_uCountFileSegmentsDownloaded*100 / s_uCountFileSegmentsToDownload );
      else
         strcpy(szBuff, "Downloading ...");
      warnings_add(szBuff);
   }

   if ( bHasMoreSegmentsToDownload )
      return;


   log_line("Received entire file. File size: %u bytes.", uTotalSize);

   if ( uFileId == FILE_ID_VEHICLE_LOGS_ARCHIVE )
   {
      warnings_add("Received complete vehicles logs.");

      hw_execute_bash_command("rm -rf tmp/vehicle_logs.zip", NULL);
      hw_execute_bash_command("touch tmp/vehicle_logs.zip", NULL);
      FILE* fd = fopen("tmp/vehicle_logs.zip", "wb");
      if ( NULL != fd )
      {
         for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
            if ( s_uFileToDownloadSegmentSize != fwrite(s_pListFileSegments[u], 1, s_uFileToDownloadSegmentSize, fd) )
               log_softerror_and_alarm("Failed to write vehicle logs zip file segment to storage.");
         fclose(fd);
      }
      else
         log_softerror_and_alarm("Failed to write received vehicle logs zip file to storage.");

      char szComm[256];
      char szFolder[256];
      char szBuff[256];

      snprintf(szFolder, sizeof(szFolder), FOLDER_MEDIA_VEHICLE_DATA, g_pCurrentModel->vehicle_id);
      snprintf(szComm, sizeof(szComm), "mkdir -p %s", szFolder);
      hw_execute_bash_command(szComm, NULL);
      snprintf(szBuff, sizeof(szBuff), "logs_%s_%u_%u.zip", g_pCurrentModel->getShortName(), g_pCurrentModel->m_Stats.uTotalFlights, g_pCurrentModel->m_Stats.uTotalOnTime);
      for( int i=0; i<strlen(szBuff); i++ )
         if ( szBuff[i] == ' ' )
            szBuff[i] = '-';
      snprintf(szComm, sizeof(szComm), "mv -f tmp/vehicle_logs.zip %s/%s", szFolder, szBuff);
      hw_execute_bash_command(szComm, NULL);
   }

   if ( 0 < s_uCountFileSegmentsToDownload )
   {
       for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
       {
          if ( NULL != s_pListFileSegments[u] )
             free(s_pListFileSegments[u]);
          s_pListFileSegments[u] = NULL;
       }
   }
   s_uFileIdToDownload = 0;
   s_uFileToDownloadState = 0xFF;
   s_uCountFileSegmentsToDownload = 0;
}


bool _commands_check_for_response_messages_from_router()
{
   s_CommandReplyLength = 0;

   u8* pLastCommandReply = get_last_command_reply_from_router();
   if ( NULL == pLastCommandReply )
      return false;

   t_packet_header* pPH = (t_packet_header*) pLastCommandReply;

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_COMMANDS )
   if ( pPH->packet_type == PACKET_TYPE_COMMAND_RESPONSE )
   {
      s_CommandLastResponseReceivedTime = g_TimeNow;
      t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(pLastCommandReply + sizeof(t_packet_header));
      if ( pPHCR->origin_command_type == COMMAND_ID_DOWNLOAD_FILE_SEGMENT )
      {
         log_line_commands( "[Commands] [Recv] Response from vId %u, cmd resp nb. %d, origin cmd nb. %d, origin retry %d, type [%s], response flags: %s, extra info len. %d]", pPH->vehicle_id_src, pPHCR->response_counter, pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter, commands_get_description(pPHCR->origin_command_type), str_get_command_response_flags_string(pPHCR->command_response_flags), pPH->total_length-sizeof(t_packet_header)-sizeof(t_packet_header_command_response));

         memcpy( s_CommandReplyBuffer, pLastCommandReply, pPH->total_length );
         s_CommandReplyLength = pPH->total_length;
         _handle_download_file_segment_response();
         return false;
      }
      if ( pPHCR->origin_command_counter != s_CommandCounter )
      {
         log_line_commands( "[CommandsTh] [Recv] Ignore out of bound response from vId %u, cmd resp nb. %d, origin cmd nb. %d, origin retry %d, type [%s], flags %d, extra info length: %d]", pPH->vehicle_id_src, pPHCR->response_counter, pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter, commands_get_description(pPHCR->origin_command_type), pPHCR->command_response_flags, pPH->total_length-sizeof(t_packet_header)-sizeof(t_packet_header_command_response));
         return false;
      }

      memcpy( s_CommandReplyBuffer, pLastCommandReply, pPH->total_length );
      s_CommandReplyLength = pPH->total_length;
      return true;
  }
  return false;
}


// Returns true if the command will still be in progress

bool handle_last_command_result()
{
   if ( NULL == g_pCurrentModel )
      return false;

   if ( s_CommandLastProcessedResponseToCommandCounter == s_CommandCounter )
   {
      log_line_commands( "[Commands] [Ignoring duplicate response] of vId %u, cmd nb. %d, retry %d, type [%s], param: %u]", g_pCurrentModel->vehicle_id, s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam);
      return false;
   }
   s_CommandLastProcessedResponseToCommandCounter = s_CommandCounter;
   log_line_commands( "[Commands] [Handling Response] of vId %u, cmd nb. %d, retry %d, type [%s], param: %u]", g_pCurrentModel->vehicle_id, s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam);

   if ( NULL != g_pCurrentModel )
      popup_log_add_entry("Received response from vehicle to command %s.", commands_get_description(s_CommandType));
   else
      popup_log_add_entry("Received response from vehicle to command %s.", commands_get_description(s_CommandType));

   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("HCommands: command succeeded but current vehicle is NULL." );
      return false;
   }

   t_packet_header* pPH = NULL;
   t_packet_header_command_response* pPHCR = NULL;
   pPH = (t_packet_header*)s_CommandReplyBuffer;
   pPHCR = (t_packet_header_command_response*)(s_CommandReplyBuffer + sizeof(t_packet_header));   

   char szBuff[1500];
   char szBuff2[64];
   char szBuff3[64];
   int tmp = 0, tmp1 = 0, tmp2 = 0, tmp3 = 0, tmp4 = 0;
   u32 tmp32 = 0;
   int tmpCard;
   u32 tmpCtrlId = 0;
   u32 *pTmp32 = NULL;
   bool tmpb = false;
   u8* pBuffer = NULL;
   int length = 0;
   FILE* fd = NULL;
   Menu* pMenu = NULL;
   radio_hw_info_t* pNIC = NULL;
   osd_parameters_t osd_params;
   rc_parameters_t rc_params_temp;
   sem_t* pSem = NULL;
   Model modelTemp;

   const char* szTokens = "+";
   char* szWord = NULL;

   ControllerSettings* pcs = get_ControllerSettings();

   switch ( s_CommandType )
   {
      case COMMAND_ID_GET_CORE_PLUGINS_INFO:
         {
          s_RetryGetCorePluginsCounter = 0;
         warnings_add("Got vehicle core plugins info.");
         s_bHasReceivedVehicleCorePluginsInfo = true;
         s_bHasToSyncCorePluginsInfoFromVehicle = false;

         u8* pTmp = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         command_packet_core_plugins_response* pResponse = (command_packet_core_plugins_response*)pTmp;
         
         g_iVehicleCorePluginsCount = pResponse->iCountPlugins;
         if ( g_iVehicleCorePluginsCount < 0 || g_iVehicleCorePluginsCount > MAX_CORE_PLUGINS_COUNT )
            g_iVehicleCorePluginsCount = 0;
         for( int i=0; i<g_iVehicleCorePluginsCount; i++ )
            memcpy((u8*)&(g_listVehicleCorePlugins[i]), (u8*)&(pResponse->listPlugins[i]), sizeof(CorePluginSettings));
         }
         break;

      case COMMAND_ID_UPLOAD_FILE_SEGMENT:
         {
             if ( g_CurrentUploadingFile.uLastSegmentIndexUploaded < g_CurrentUploadingFile.uTotalSegments )
                g_CurrentUploadingFile.bSegmentsUploaded[g_CurrentUploadingFile.uLastSegmentIndexUploaded] = true;

             bool bAllUploaded = false;
             for( u32 u=0; u<g_CurrentUploadingFile.uTotalSegments; u++ )
                if ( ! g_CurrentUploadingFile.bSegmentsUploaded[u] )
                {
                   bAllUploaded = false;
                   break;
                }

             if ( bAllUploaded )
                g_bHasFileUploadInProgress = false;
         }
         break;

      case COMMAND_ID_SET_RELAY_PARAMETERS:
         {
            type_relay_parameters* pParams = (type_relay_parameters*)s_CommandBuffer;
            type_relay_parameters oldRelayParams;
            memcpy(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
            memcpy(&(g_pCurrentModel->relay_params), pParams, sizeof(type_relay_parameters));
            saveAsCurrentModel(g_pCurrentModel);
            
            u8 uOldRelayMode = oldRelayParams.uCurrentRelayMode;
            oldRelayParams.uCurrentRelayMode = g_pCurrentModel->relay_params.uCurrentRelayMode;
         
            // Only relay mode changed?

            if ( 0 == memcmp(&oldRelayParams, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters)) )
            {
               if ( uOldRelayMode != g_pCurrentModel->relay_params.uCurrentRelayMode )
               {
                  onEventRelayModeChanged();
                  log_line("Changed relay mode from %u to %u.", uOldRelayMode, g_pCurrentModel->relay_params.uCurrentRelayMode);
                  send_control_message_to_router(PACKET_TYLE_LOCAL_CONTROL_RELAY_MODE_SWITCHED, g_pCurrentModel->relay_params.uCurrentRelayMode);
               }
               else
                  log_line("No change in relay flags or relay mode detected.");
               break;
            }
            else
            {
               log_line("Relay flags and parameters changed. Notify router about the change.");
               send_model_changed_message_to_router(MODEL_CHANGED_RELAY_PARAMS);
            }
            break;
         }

      case COMMAND_ID_SET_ENCRYPTION_PARAMS:
         g_pCurrentModel->enc_flags = s_CommandBuffer[0];
         saveAsCurrentModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         break;

      case COMMAND_ID_SET_DEVELOPER_FLAGS:
         g_pCurrentModel->uDeveloperFlags = s_CommandParam;
         saveAsCurrentModel(g_pCurrentModel);  
         snprintf(szBuff, sizeof(szBuff), "Vehicle development flags: %d", s_CommandParam);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         break;

      case COMMAND_ID_ENABLE_LIVE_LOG:
         if ( 0 != s_CommandParam )
            g_pCurrentModel->uDeveloperFlags |= DEVELOPER_FLAGS_BIT_LIVE_LOG;
         else
            g_pCurrentModel->uDeveloperFlags &= (~DEVELOPER_FLAGS_BIT_LIVE_LOG);
         saveAsCurrentModel(g_pCurrentModel);  
         snprintf(szBuff, sizeof(szBuff), "Switched vehicle live log stream to: %d", s_CommandParam);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         break;

      case COMMAND_ID_SET_MODEL_FLAGS:
         g_pCurrentModel->uModelFlags = s_CommandParam;
         saveAsCurrentModel(g_pCurrentModel);  
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         break;


      case COMMAND_ID_GET_MODULES_INFO:
         length = pPH->total_length - pPH->total_headers_length;
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
         {
            u8 size = *(((u8*)pPH) + pPH->total_length-1);
            length -= size;
         }
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         s_pMenuVehicleHWInfo = new Menu(0,"Vehicle Modules Info",NULL);
         s_pMenuVehicleHWInfo->m_xPos = 0.18; s_pMenuVehicleHWInfo->m_yPos = 0.16;
         s_pMenuVehicleHWInfo->m_Width = 0.6;
         s_pMenuVehicleHWInfo->addTopLine(" ");         
         add_menu_to_stack(s_pMenuVehicleHWInfo);

         strcpy(szBuff, (const char*)pBuffer);

         szWord = strtok(szBuff, "#");
         while( NULL != szWord )
         {
            int len = strlen(szWord);
            for( int i=0; i<len; i++ )
               if ( szWord[i] == 10 || szWord[i] == 13 )
                  szWord[i] = ' ';
            s_pMenuVehicleHWInfo->addTopLine(szWord);
            szWord = strtok(NULL, "#");
         }
         break;

      case COMMAND_ID_GET_CURRENT_VIDEO_CONFIG:
         length = pPH->total_length - pPH->total_headers_length;
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
         {
            u8 size = *(((u8*)pPH) + pPH->total_length-1);
            length -= size;
         }
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         s_pMenuVehicleHWInfo = new Menu(0,"Vehicle Hardware Info",NULL);
         s_pMenuVehicleHWInfo->m_xPos = 0.32; s_pMenuVehicleHWInfo->m_yPos = 0.17;
         s_pMenuVehicleHWInfo->m_Width = 0.6;
         snprintf(szBuff, sizeof(szBuff), "Board type: %s, software version: %d.%d (b%d)", str_get_hardware_board_name(g_pCurrentModel->board_type), ((g_pCurrentModel->sw_version)>>8) & 0xFF, (g_pCurrentModel->sw_version) & 0xFF, ((g_pCurrentModel->sw_version)>>16));
         s_pMenuVehicleHWInfo->addTopLine(szBuff);
         s_pMenuVehicleHWInfo->addTopLine(" ");
         s_pMenuVehicleHWInfo->addTopLine(" ");
         
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            snprintf(szBuff, sizeof(szBuff), "Radio Interface %d: %s, USB port %s,  %s, driver %s", i+1, g_pCurrentModel->radioInterfacesParams.interface_szMAC[i], g_pCurrentModel->radioInterfacesParams.interface_szPort[i], str_get_radio_type_description(g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i]), str_get_radio_driver_description(g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i]));
            s_pMenuVehicleHWInfo->addTopLine(szBuff);
            snprintf(szBuff, sizeof(szBuff), ". . . currently at %s, supported bands: ", str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i]));
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_23 )
               strlcat(szBuff, "2.3 ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_24 )
               strlcat(szBuff, "2.4 ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_25 )
               strlcat(szBuff, "2.5 ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] & RADIO_HW_SUPPORTED_BAND_58 )
               strlcat(szBuff, "5.8 ", sizeof(szBuff));
            s_pMenuVehicleHWInfo->addTopLine(szBuff);
            snprintf(szBuff, sizeof(szBuff), ". . . flags: ");
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
               strlcat(szBuff, "TX/RX, ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
            if ( !( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
               strlcat(szBuff, "TX Only, ", sizeof(szBuff));
            if ( !(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
               strlcat(szBuff, "RX Only, ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
               strlcat(szBuff, "Video, ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
               strlcat(szBuff, "Data, ", sizeof(szBuff));
            if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
               strlcat(szBuff, "Relay", sizeof(szBuff));
            s_pMenuVehicleHWInfo->addTopLine(szBuff);
         }

         add_menu_to_stack(s_pMenuVehicleHWInfo);
         s_pMenuVehicleHWInfo->addTopLine(" ");
         s_pMenuVehicleHWInfo->addTopLine(" ");
         strcpy(szBuff, (const char*)pBuffer);

            szWord = strtok(szBuff, "#");
            while( NULL != szWord )
            {    
               s_pMenuVehicleHWInfo->addTopLine(szWord);
               szWord = strtok(NULL, "#");
            }

            s_bHasCommandInProgress = false;
            handle_commands_send_to_vehicle(COMMAND_ID_GET_MEMORY_INFO, 0, NULL, 0);
            return true;
            break;

      case COMMAND_ID_GET_MEMORY_INFO:
         length = pPH->total_length - pPH->total_headers_length;
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
         {
            u8 size = *(((u8*)pPH) + pPH->total_length-1);
            length -= size;
         }
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         pTmp32 = (u32*)pBuffer;
         if ( length == 2*sizeof(u32) )
         {
            snprintf(szBuff, sizeof(szBuff), "Memory: %d Mb free out of %d Mb total", *(pTmp32+1), *pTmp32);
            s_pMenuVehicleHWInfo->addTopLine(" ");
            s_pMenuVehicleHWInfo->addTopLine(szBuff);
         }
         s_bHasCommandInProgress = false;
         handle_commands_send_to_vehicle(COMMAND_ID_GET_CPU_INFO, 0, NULL, 0);
         return true;
         break;

      case COMMAND_ID_GET_CPU_INFO:
         length = pPH->total_length - pPH->total_headers_length;
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
         {
            u8 size = *(((u8*)pPH) + pPH->total_length-1);
            length -= size;
         }
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         s_pMenuVehicleHWInfo->addTopLine(" ");
         //s_pMenuVehicleHWInfo->addTopLine("CPU Info:");

         strcpy(szBuff, (const char*)pBuffer);
         szWord = strtok(szBuff, szTokens);
         while( NULL != szWord )
         {
            log_line("Add vehicle info to menu: [%s]", szWord);
            s_pMenuVehicleHWInfo->addTopLine(szWord);
            szWord = strtok(NULL, szTokens);
         }
         break;

      case COMMAND_ID_GET_ALL_PARAMS_ZIP:
      {
         s_RetryGetSettingsCounter = 0;
         length = pPH->total_length - pPH->total_headers_length;
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
         {
            u8 size = *(((u8*)pPH) + pPH->total_length-1);
            length -= size;
         }
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
         log_line("Received all model settings (compressed). Model file size (compressed): %d", length);

         hw_execute_bash_command("rm -rf tmp/model.mdl", NULL);
         fd = fopen("tmp/last_recv_model.tar", "wb");
         if ( NULL != fd )
         {
            fwrite(pBuffer, 1, length, fd);
            fclose(fd);
            fd = NULL;
            hw_execute_bash_command("tar -zxf tmp/last_recv_model.tar", NULL);
         }

         u8 buffer[5048];
         int length = 0;
         fd = fopen("tmp/model.mdl", "rb");
         if ( NULL != fd )
         {
            length = fread(buffer, 1, 5000, fd);
            fclose(fd);
            onEventReceivedModelSettings(buffer, length, false);
            hw_execute_bash_command("rm -rf tmp/model.mdl", NULL);
         }
         else
            log_error_and_alarm("Failed to load received model file from decompressed file: tmp/model.mdl");

         break;
      }

     case COMMAND_ID_GET_USB_INFO:
     {
        s_pMenuUSBInfoVehicle = new Menu(0,"Vehicle USB Radio Interfaces Info",NULL);
        s_pMenuUSBInfoVehicle->m_xPos = 0.18;
        s_pMenuUSBInfoVehicle->m_yPos = 0.15;
        s_pMenuUSBInfoVehicle->m_Width = 0.56;
        add_menu_to_stack(s_pMenuUSBInfoVehicle);

        int iLength = pPH->total_length - pPH->total_headers_length;
        if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
        {
           u8 size = *(((u8*)pPH) + pPH->total_length-1);
           iLength -= size;
        }
        pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
        log_line("Received %d bytes for vehicle USB radio interfaces info.", iLength);
        char szText[3000];
        FILE* fd = fopen("tmp/tmp_usb_info.tar", "wb");
        if ( NULL != fd )
        {
           fwrite(pBuffer, 1, iLength, fd);
           fclose(fd);
           hw_execute_bash_command("tar -zxf tmp/tmp_usb_info.tar", NULL);
           iLength = 0;
           fd = fopen("tmp/tmp_usb_info.txt", "rb");
           if ( NULL != fd )
           {
              iLength = fread(szText,1,3000, fd);
              fclose(fd);
           }
        }
        else
        {
           log_softerror_and_alarm("Failed to write file with vehicle USB radio info. tmp/tmp_usb_info.tar");
           iLength = 0;
        }
        //hw_execute_bash_command("rm -rf tmp/tmp_usb_info.tar 2>/dev/null", NULL);
        //hw_execute_bash_command("rm -rf tmp/tmp_usb_info.txt 2>/dev/null", NULL);

        if ( iLength <= 0 )
           s_pMenuUSBInfoVehicle->addTopLine("Received invalid info.");
        else
           log_line("Received %d uncompressed bytes for vehicle USB radio interfaces info.", iLength);

        int iPos = 0;
        int iStartLine = iPos;
        while (iPos < iLength )
        {
           if ( szText[iPos] == 10 || szText[iPos] == 13 || szText[iPos] == 0 )
           {
              szText[iPos] = 0;
              if ( iPos > iStartLine + 2 )
                 s_pMenuUSBInfoVehicle->addTopLine(szText+iStartLine);
              iStartLine = iPos+1;
           }
           iPos++;
        }
        szText[iPos] = 0;
        if ( iPos > iStartLine + 2 )
          s_pMenuUSBInfoVehicle->addTopLine(szText+iStartLine);

        s_bHasCommandInProgress = false;
        handle_commands_send_to_vehicle(COMMAND_ID_GET_USB_INFO2, 0, NULL, 0);
        return true;
        break;
     }

     case COMMAND_ID_GET_USB_INFO2:
     {
        if ( NULL == s_pMenuUSBInfoVehicle )
           break;
        s_pMenuUSBInfoVehicle->addTopLine("List:");
        int iLength = pPH->total_length - pPH->total_headers_length;
        if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
        {
           u8 size = *(((u8*)pPH) + pPH->total_length-1);
           iLength -= size;
        }
        pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);
        log_line("Received %d bytes for vehicle USB radio interfaces info2.", iLength);
        char szText[3000];
        FILE* fd = fopen("tmp/tmp_usb_info2.tar", "wb");
        if ( NULL != fd )
        {
           fwrite(pBuffer, 1, iLength, fd);
           fclose(fd);
           hw_execute_bash_command("tar -zxf tmp/tmp_usb_info2.tar", NULL);
           iLength = 0;
           fd = fopen("tmp/tmp_usb_info2.txt", "rb");
           if ( NULL != fd )
           {
              iLength = fread(szText,1,3000, fd);
              fclose(fd);
           }
        }
        else
        {
           log_softerror_and_alarm("Failed to write file with vehicle USB radio info2. tmp/tmp_usb_info2.tar");
           iLength = 0;
        }
        //hw_execute_bash_command("rm -rf tmp/tmp_usb_info.tar 2>/dev/null", NULL);
        //hw_execute_bash_command("rm -rf tmp/tmp_usb_info.txt 2>/dev/null", NULL);

        if ( iLength <= 0 )
           s_pMenuUSBInfoVehicle->addTopLine("Received invalid info 2.");
        else
           log_line("Received %d uncompressed bytes for vehicle USB radio interfaces info2.", iLength);

        int iPos = 0;
        int iStartLine = iPos;
        while (iPos < iLength )
        {
           if ( szText[iPos] == 10 || szText[iPos] == 13 || szText[iPos] == 0 )
           {
              szText[iPos] = 0;
              if ( iPos > iStartLine + 2 )
                 s_pMenuUSBInfoVehicle->addTopLine(szText+iStartLine);
              iStartLine = iPos+1;
           }
           iPos++;
        }
        szText[iPos] = 0;
        if ( iPos > iStartLine + 2 )
          s_pMenuUSBInfoVehicle->addTopLine(szText+iStartLine);
        break;
      }

      case COMMAND_ID_SET_VEHICLE_TYPE:
         log_line("HCommands: command set vehicle type succeeded, vehicle type: %d", s_CommandParam );
         g_pCurrentModel->vehicle_type = s_CommandParam;
         g_pCurrentModel->constructLongName();
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_SET_ENABLE_DHCP:
         log_line("HCommands: command enable DHCP (0/1) succeeded, enabled: %d", s_CommandParam );
         g_pCurrentModel->enableDHCP = (bool)s_CommandParam;
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_SET_RC_CAMERA_PARAMS:
         g_pCurrentModel->camera_rc_channels = s_CommandParam;
         saveAsCurrentModel(g_pCurrentModel);         
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         break;

      case COMMAND_ID_SET_OVERCLOCKING_PARAMS:
         {
            command_packet_overclocking_params params;
            memcpy(&params, s_CommandBuffer, sizeof(command_packet_overclocking_params));
            g_pCurrentModel->iFreqARM = params.freq_arm;
            g_pCurrentModel->iFreqGPU = params.freq_gpu;
            g_pCurrentModel->iOverVoltage = params.overvoltage;
            saveAsCurrentModel(g_pCurrentModel);
         }
         break;

      case COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS:
         {
            memcpy(&(g_pCurrentModel->functions_params), s_CommandBuffer, sizeof(type_functions_parameters));
            saveAsCurrentModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         }
         break;

      case COMMAND_ID_SET_CONTROLLER_TELEMETRY_OPTIONS:
         break;
         
      case COMMAND_ID_SWAP_RADIO_INTERFACES:
         {
            log_line("Received response to vehicle swap radio interfaces.");
            g_pCurrentModel->swapRadioInterfaces();
            saveAsCurrentModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
            menu_refresh_all_menus();
         }
         break;

      case COMMAND_ID_SET_CAMERA_PARAMETERS:
         tmp = s_CommandParam;
         if ( tmp < 0 || tmp >= g_pCurrentModel->iCameraCount )
            tmp = 0;
         memcpy(&(g_pCurrentModel->camera_params[tmp]), s_CommandBuffer, sizeof(type_camera_parameters));
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_SET_CAMERA_PROFILE:
         tmp = s_CommandParam;
         if ( tmp < 0 || tmp >= MODEL_CAMERA_PROFILES )
            tmp = 0;
         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile = tmp;
         saveAsCurrentModel(g_pCurrentModel);  
         snprintf(szBuff, sizeof(szBuff), "Switched camera %d to profile %s", g_pCurrentModel->iCurrentCamera, model_getCameraProfileName(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile));
         warnings_add(szBuff);
         break;

      case COMMAND_ID_SET_CURRENT_CAMERA:
         tmp = s_CommandParam;
         if ( tmp < 0 || tmp >= g_pCurrentModel->iCameraCount )
            tmp = 0;
         g_pCurrentModel->iCurrentCamera = tmp;
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_FORCE_CAMERA_TYPE:
         tmp = s_CommandParam;
         g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType = tmp;
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_RESET_VIDEO_LINK_PROFILE:
         g_pCurrentModel->resetVideoLinkProfiles(g_pCurrentModel->video_params.user_selected_video_link_profile);
         saveAsCurrentModel(g_pCurrentModel);         
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         break;

      case COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES:
         for( tmp=0; tmp<MAX_VIDEO_LINK_PROFILES; tmp++ )
            memcpy(&(g_pCurrentModel->video_link_profiles[tmp]), s_CommandBuffer + tmp * sizeof(type_video_link_profile), sizeof(type_video_link_profile));
         saveAsCurrentModel(g_pCurrentModel);         
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         break;

      case COMMAND_ID_SET_VIDEO_H264_QUANTIZATION:
         if ( s_CommandParam > 0 )
            g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization = (int)s_CommandParam;
         else if ( g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization > 0 )
            g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization = - g_pCurrentModel->video_link_profiles[g_pCurrentModel->video_params.user_selected_video_link_profile].h264quantization;
      
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_RESET_ALL_TO_DEFAULTS:
         {
            u32 vid = g_pCurrentModel->vehicle_id;
            u32 ctrlId = g_pCurrentModel->controller_id;
            int boardType = g_pCurrentModel->board_type;
            bool bDev = g_pCurrentModel->bDeveloperMode;
            int cameraType = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType;
            int forcedCameraType = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType;

            type_vehicle_stats_info stats;
            memcpy((u8*)&stats, (u8*)&(g_pCurrentModel->m_Stats), sizeof(type_vehicle_stats_info));
 
            type_radio_links_parameters radio_links;
            type_radio_interfaces_parameters radio_interfaces;

            memcpy(&radio_links, &g_pCurrentModel->radioLinksParams, sizeof(type_radio_links_parameters) );
            memcpy(&radio_interfaces, &g_pCurrentModel->radioInterfacesParams, sizeof(type_radio_interfaces_parameters) );
 
            g_pCurrentModel->resetToDefaults(false);

            memcpy(&g_pCurrentModel->radioLinksParams, &radio_links, sizeof(type_radio_links_parameters) );
            memcpy(&g_pCurrentModel->radioInterfacesParams, &radio_interfaces, sizeof(type_radio_interfaces_parameters) );

            g_pCurrentModel->vehicle_id = vid;
            g_pCurrentModel->controller_id = ctrlId;
            g_pCurrentModel->board_type = boardType;
            g_pCurrentModel->bDeveloperMode = bDev;
            g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType = cameraType;
            g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType = forcedCameraType;

            memcpy((u8*)&(g_pCurrentModel->m_Stats), (u8*)&stats, sizeof(type_vehicle_stats_info));

            g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);

            g_pCurrentModel->is_spectator = false;
            saveAsCurrentModel(g_pCurrentModel);
            g_pCurrentModel->b_mustSyncFromVehicle = true;
            break;
         }

      case COMMAND_ID_SET_ALL_PARAMS:
         {
            u32 vid = g_pCurrentModel->vehicle_id;
            u32 ctrlId = g_pCurrentModel->controller_id;
            int boardType = g_pCurrentModel->board_type;
            bool bDev = g_pCurrentModel->bDeveloperMode;
            int cameraType = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType;
            int forcedCameraType = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType;

            type_vehicle_stats_info stats;
            memcpy((u8*)&stats, (u8*)&(g_pCurrentModel->m_Stats), sizeof(type_vehicle_stats_info));

            type_radio_links_parameters radio_links;
            type_radio_interfaces_parameters radio_interfaces;

            memcpy(&radio_links, &g_pCurrentModel->radioLinksParams, sizeof(type_radio_links_parameters) );
            memcpy(&radio_interfaces, &g_pCurrentModel->radioInterfacesParams, sizeof(type_radio_interfaces_parameters) );
 
            g_pCurrentModel->loadFromFile("tmp/tempVehicleSettings.txt"); 

            memcpy(&g_pCurrentModel->radioLinksParams, &radio_links, sizeof(type_radio_links_parameters) );
            memcpy(&g_pCurrentModel->radioInterfacesParams, &radio_interfaces, sizeof(type_radio_interfaces_parameters) );

            g_pCurrentModel->vehicle_id = vid;
            g_pCurrentModel->controller_id = ctrlId;
            g_pCurrentModel->board_type = boardType;
            g_pCurrentModel->bDeveloperMode = bDev;
            g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCameraType = cameraType;
            g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iForcedCameraType = forcedCameraType;

            memcpy((u8*)&(g_pCurrentModel->m_Stats), (u8*)&stats, sizeof(type_vehicle_stats_info));

            g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);

            g_pCurrentModel->is_spectator = false;
            saveAsCurrentModel(g_pCurrentModel);
            g_pCurrentModel->b_mustSyncFromVehicle = true;
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
            menu_close_all();
            break;
         }

      case COMMAND_ID_SET_VEHICLE_NAME:
         strcpy(g_pCurrentModel->vehicle_name, (const char*)s_CommandBuffer );
         g_pCurrentModel->constructLongName();
         saveAsCurrentModel(g_pCurrentModel);
         break;

      case COMMAND_ID_SET_RADIO_CARD_MODEL:
         tmp1 = (s_CommandParam & 0xFF);
         tmp2 = ((int)((s_CommandParam >> 8) & 0xFF)) - 128;

         if ( tmp1 >= 0 && tmp1 < g_pCurrentModel->radioInterfacesParams.interfaces_count )
         {
            g_pCurrentModel->radioInterfacesParams.interface_card_model[tmp1] = tmp2;
            saveAsCurrentModel(g_pCurrentModel);
         }
         break;

      case COMMAND_ID_SET_RADIO_LINK_FREQUENCY:
       {
         if ( s_CommandParam & 0x80000000 ) // New format
         {
         u32 tmpLink = (s_CommandParam>>24) & 0x7F;
         u32 tmpFreq = (s_CommandParam & 0xFFFFFF);
         log_line("Received response Ok from vehicle to the link frequency change (new format). Vehicle radio link %u new freq: %s", tmpLink+1, str_format_frequency(tmpFreq));
         if ( tmpLink < g_pCurrentModel->radioLinksParams.links_count )
         {
            g_pCurrentModel->radioLinksParams.link_frequency[tmpLink] = tmpFreq;
            for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
               if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == tmpLink )
                  g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i] = tmpFreq;
            
            saveAsCurrentModel(g_pCurrentModel);
            u32 data[2];
            data[0] = tmpLink;
            data[1] = tmpFreq;
            send_control_message_to_router_and_data(PACKET_TYPE_LOCAL_LINK_FREQUENCY_CHANGED, (u8*)(&data[0]), 2*sizeof(u32));
         }
         }
         else
         {
         u32 tmpLink = (s_CommandParam>>16);
         u32 tmpFreq = (s_CommandParam & 0xFFFF);
         log_line("Received response Ok from vehicle to the link frequency change (old format). Vehicle radio link %u new freq: %s", tmpLink+1, str_format_frequency(tmpFreq));
         if ( tmpLink < g_pCurrentModel->radioLinksParams.links_count )
         {
            g_pCurrentModel->radioLinksParams.link_frequency[tmpLink] = tmpFreq;
            for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
               if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == tmpLink )
                  g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i] = tmpFreq;
            
            saveAsCurrentModel(g_pCurrentModel);
            u32 data[2];
            data[0] = tmpLink;
            data[1] = tmpFreq;
            send_control_message_to_router_and_data(PACKET_TYPE_LOCAL_LINK_FREQUENCY_CHANGED, (u8*)(&data[0]), 2*sizeof(u32));
         }
         }
         break;
      }
      case COMMAND_ID_SET_RADIO_LINK_CAPABILITIES:
         {
         int linkIndex = s_CommandParam & 0xFF;
         u32 flags = s_CommandParam >> 8;
         char szBuffC[128];
         str_get_radio_capabilities_description(flags, szBuffC);
         log_line("Set radio link nb.%d capabilities flags to %s", linkIndex+1, szBuffC);
         bool bOnlyVideoDataFlagsChanged = false;
         if ( (flags & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO))) ==
              (g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex] & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO))) )
            bOnlyVideoDataFlagsChanged = true;
         g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex] = flags;
         saveAsCurrentModel(g_pCurrentModel);
         if ( bOnlyVideoDataFlagsChanged )
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         else
         {
            log_line("Radio link capabilities changed. Reinitializing radio links and router...");
            pairing_stop();
            hardware_sleep_ms(100);
            pairing_start();
            log_line("Finished repairing due to radio links capabilities changed.");
            menu_refresh_all_menus();
         }
         break;
         }

      case COMMAND_ID_SET_CLOCK_SYNC_TYPE:
         g_pCurrentModel->clock_sync_type = s_CommandParam;
         saveAsCurrentModel(g_pCurrentModel);  
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         break;

      case COMMAND_ID_SET_RADIO_LINK_FLAGS:
         {
         u32* p = (u32*)&(s_CommandBuffer[0]);
         u32 linkIndex = *p;
         p++;
         u32 linkRadioFlags = *p;
         p++;
         int* pi = (int*)p;
         int datarateVideo = *pi;
         pi++;
         int datarateData = *pi;

         if ( linkIndex < 0 || linkIndex >= g_pCurrentModel->radioLinksParams.links_count )
            break;
         return true;
         }

      case COMMAND_ID_RADIO_LINK_FLAGS_CHANGED_CONFIRMATION:
         {
            s_bCommandsWaitForRadioFlagsChangedConfirmation = false;
            s_bCommandsCheckForRadioFlagsWorking = false;
            g_TimePendingRadioFlagsChanged = 0;
            g_PendingRadioFlagsConfirmationLinkId = -1;

            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
            break;
         }

      case COMMAND_ID_SET_VIDEO_PARAMS:
         {
            video_parameters_t params;
            video_parameters_t oldParams;

            memcpy(&oldParams, &(g_pCurrentModel->video_params) , sizeof(video_parameters_t));
            memcpy(&params, s_CommandBuffer, sizeof(video_parameters_t));
            memcpy(&(g_pCurrentModel->video_params), &params, sizeof(video_parameters_t));

            if ( g_pCurrentModel->video_params.user_selected_video_link_profile != oldParams.user_selected_video_link_profile )
            {
               //if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_BEST_PERF )
               //   g_pCurrentModel->clock_sync_type = CLOCK_SYNC_TYPE_BASIC;
               //if ( g_pCurrentModel->video_params.user_selected_video_link_profile == VIDEO_PROFILE_HIGH_QUALITY )
               //   g_pCurrentModel->clock_sync_type = CLOCK_SYNC_TYPE_ADV;
               
               g_pCurrentModel->clock_sync_type = CLOCK_SYNC_TYPE_NONE;
            }

            saveAsCurrentModel(g_pCurrentModel);   

            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
            break;
         }


      case COMMAND_ID_SET_AUDIO_PARAMS:
         {
            audio_parameters_t params;
            memcpy(&params, s_CommandBuffer, sizeof(audio_parameters_t));
            memcpy(&(g_pCurrentModel->audio_params), &params, sizeof(audio_parameters_t));
            saveAsCurrentModel(g_pCurrentModel);
            pairing_stop();
            hardware_sleep_ms(50);
            pairing_start();
            menu_refresh_all_menus();
            break;
         }

      case COMMAND_ID_SET_GPS_INFO:
         g_pCurrentModel->iGPSCount = s_CommandParam;
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_SET_OSD_PARAMS:
         {
            osd_parameters_t* pParams = (osd_parameters_t*)s_CommandBuffer;
            merge_osd_params(pParams, &(g_pCurrentModel->osd_params));
            saveAsCurrentModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
            break;
         }

      case COMMAND_ID_SET_ALARMS_PARAMS:
         {
            type_alarms_parameters* pParams = (type_alarms_parameters*)s_CommandBuffer;
            memcpy(&(g_pCurrentModel->alarms_params), &pParams, sizeof(type_alarms_parameters));
            saveAsCurrentModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
            break;
         }

      case COMMAND_ID_SET_SERIAL_PORTS_INFO:
         {
            model_hardware_info_t* pNewInfo = (model_hardware_info_t*)s_CommandBuffer;
            g_pCurrentModel->hardware_info.serial_bus_count = pNewInfo->serial_bus_count;
            for( int i=0; i<pNewInfo->serial_bus_count; i++ )
            {
               strcpy(g_pCurrentModel->hardware_info.serial_bus_names[i], pNewInfo->serial_bus_names[i]);
               g_pCurrentModel->hardware_info.serial_bus_speed[i] = pNewInfo->serial_bus_speed[i];
               g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] = pNewInfo->serial_bus_supported_and_usage[i];
            }
            saveAsCurrentModel(g_pCurrentModel);
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         }
         break;

      case COMMAND_ID_SET_TELEMETRY_PARAMETERS:
         memcpy(&g_pCurrentModel->telemetry_params, s_CommandBuffer, sizeof(telemetry_parameters_t));
         saveAsCurrentModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         break;

      case COMMAND_ID_ENABLE_DEBUG:
         g_pCurrentModel->bDeveloperMode = (bool)s_CommandParam;
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_SET_TX_POWERS:
      {
         u8* pData = &s_CommandBuffer[0];
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

         if ( cardPower > 0 )
         if ( cardIndex < g_pCurrentModel->radioInterfacesParams.interfaces_count )
            g_pCurrentModel->radioInterfacesParams.interface_power[cardIndex] = cardPower;
      
         saveAsCurrentModel(g_pCurrentModel);         
         break;
      }
      case COMMAND_ID_SET_RC_PARAMS:
         {
            bool bPrevRC = g_pCurrentModel->rc_params.rc_enabled;
            memcpy(&g_pCurrentModel->rc_params, s_CommandBuffer, sizeof(rc_parameters_t));
            saveAsCurrentModel(g_pCurrentModel);
            if ( bPrevRC != g_pCurrentModel->rc_params.rc_enabled )
            {
               if ( g_pCurrentModel->rc_params.rc_enabled )
               {
                  Popup* p = new Popup(true, "RC link is enabled", 3 );
                  p->setIconId(g_idIconJoystick, get_Color_IconWarning());
                  popups_add_topmost(p);
               }
               else
               {
                  Popup* p = new Popup(true, "RC link is disabled", 3 );
                  p->setIconId(g_idIconJoystick, get_Color_IconWarning());
                  popups_add_topmost(p);
               }
            }
            send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         break;
         }

      case COMMAND_ID_SET_NICE_VALUE_TELEMETRY:
         g_pCurrentModel->niceTelemetry = ((int)(s_CommandParam % 256))-20;
         log_line("Set new nice value for telemetry: %d", g_pCurrentModel->niceTelemetry);
         saveAsCurrentModel(g_pCurrentModel);
         update_processes_priorities();
         break;

      case COMMAND_ID_SET_NICE_VALUES:
         g_pCurrentModel->niceVideo = ((int)(s_CommandParam % 256))-20;
         g_pCurrentModel->niceOthers = ((int)((s_CommandParam>>8) % 256))-20;
         g_pCurrentModel->niceRouter = ((int)((s_CommandParam>>16) % 256))-20;
         g_pCurrentModel->niceRC = ((int)((s_CommandParam>>24) % 256))-20;
         log_line("Set new nice values: video: %d, router: %d, rc: %d, others: %d", g_pCurrentModel->niceVideo, g_pCurrentModel->niceRouter, g_pCurrentModel->niceRC, g_pCurrentModel->niceOthers);
         saveAsCurrentModel(g_pCurrentModel);
         update_processes_priorities();
         break;

      case COMMAND_ID_SET_IONICE_VALUES:
         g_pCurrentModel->ioNiceVideo = ((int)(s_CommandParam % 256))-20;
         g_pCurrentModel->ioNiceRouter = ((int)((s_CommandParam>>8) % 256))-20;
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_DEBUG_GET_TOP:
         length = pPH->total_length - pPH->total_headers_length;
         if ( pPH->packet_flags & PACKET_FLAGS_BIT_EXTRA_DATA )
         {
            u8 size = *(((u8*)pPH) + pPH->total_length-1);
            length -= size;
         }
         pBuffer = s_CommandReplyBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_command_response);

         log_line("Vehicle TOP output (%d chars):\n%s", length, pBuffer);
         break;

      case COMMAND_ID_SET_RADIO_SLOTTIME:
         g_pCurrentModel->radioInterfacesParams.slotTime = s_CommandParam;
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_SET_RADIO_THRESH62:
         g_pCurrentModel->radioInterfacesParams.thresh62 = s_CommandParam;
         saveAsCurrentModel(g_pCurrentModel);         
         break;

      case COMMAND_ID_DOWNLOAD_FILE:
         _handle_download_file_response();
         break;

      case COMMAND_ID_DOWNLOAD_FILE_SEGMENT:
         _handle_download_file_segment_response();
         break;
   }

   menu_invalidate_all();
   return false;
}

bool _commands_check_send_get_settings()
{
   //log_line("%d, %d, %d, %d, %u, %u, %d", s_bHasCommandInProgress, g_pCurrentModel->b_mustSyncFromVehicle, g_pCurrentModel->is_spectator, g_bSearching,
   //           g_TimeNow, pairing_getStartTime(), s_RetryGetSettingsCounter);

   //if ( NULL != g_pCurrentModel )
   //   log_line("%d, %d, %u - %u, %d, %d, %d, %d", g_bIsReinit, s_bHasCommandInProgress, pairing_getCurrentVehicleID(), g_pCurrentModel->vehicle_id, (int)g_bSearching, s_RetryGetSettingsCounter, (int) g_bIsFirstConnectionToCurrentVehicle, g_pCurrentModel->is_spectator);

   if ( ! g_bIsReinit )
   if ( ! s_bHasCommandInProgress )
   if ( NULL != g_pCurrentModel && (g_pCurrentModel->b_mustSyncFromVehicle || g_bIsFirstConnectionToCurrentVehicle ) && (!g_pCurrentModel->is_spectator))
   if ( ! g_bSearching )
   if ( pairing_isReceiving() )
   if ( pairing_getCurrentVehicleID() == g_pCurrentModel->vehicle_id )
   if ( g_TimeNow > pairing_getStartTime() + 500  )
   if ( s_RetryGetSettingsCounter < 5 )
   {
      s_RetryGetSettingsCounter++;
      ControllerSettings* pCS = get_ControllerSettings();
      log_line("Current vehicle sw version: %d.%d (b%d)", ((g_pCurrentModel->sw_version >> 8 ) & 0xFF), (g_pCurrentModel->sw_version & 0xFF)/10, (g_pCurrentModel->sw_version >> 16));
      log_line("Current controller telemetry options: input: %d, output: %d", pCS->iTelemetryInputSerialPortIndex, pCS->iTelemetryOutputSerialPortIndex);
      Preferences* pP = get_Preferences();
      u32 flags = 0;
      flags = 0;
      if ( pCS->iDeveloperMode )
         flags |= 0x01;
      if ( (pCS->iTelemetryOutputSerialPortIndex >= 0) || (pCS->iTelemetryForwardUSBType != 0) )
         flags |= (((u32)0x01)<<1);
      if ( pCS->iTelemetryInputSerialPortIndex >= 0 )
         flags |= (((u32)0x01)<<2);
      if ( pP->iDebugShowVehicleVideoStats )
         flags |= (((u32)0x01)<<3);
      if ( pP->iDebugShowVehicleVideoGraphs )
         flags |= (((u32)0x01)<<4);
      if ( g_bOSDPluginsNeedTelemetryStreams )
         flags |= (((u32)0x01)<<5);
        
      flags |= ((pCS->iMAVLinkSysIdController & 0xFF) << 8);
      flags |= ((pP->iDebugWiFiChangeDelay & 0xFF) << 16);
      log_line("Send request to router to request model settings from vehicle.");
      return handle_commands_send_to_vehicle(COMMAND_ID_GET_ALL_PARAMS_ZIP, flags, NULL, 0);
   }

   bool bHasPluginsSupport = false;
   if ( (NULL != g_pCurrentModel) && (((g_pCurrentModel->sw_version>>8) & 0xFF) >= 7) )
      bHasPluginsSupport = true;
   if ( (NULL != g_pCurrentModel) && (((g_pCurrentModel->sw_version>>8) & 0xFF) == 6) &&
        ( ((g_pCurrentModel->sw_version & 0xFF) == 9) || ((g_pCurrentModel->sw_version & 0xFF) >= 90) ) )
      bHasPluginsSupport = true;

   if ( bHasPluginsSupport )
   if ( get_CorePluginsCount() > 0 )
   if ( s_bHasToSyncCorePluginsInfoFromVehicle )
   if ( ! g_bIsReinit )
   if ( ! s_bHasCommandInProgress )
   if ( NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator))
   if ( ! g_pCurrentModel->b_mustSyncFromVehicle )
   if ( ! g_bSearching )
   if ( pairing_isReceiving() )
   if ( pairing_getCurrentVehicleID() == g_pCurrentModel->vehicle_id )
   if ( g_TimeNow > pairing_getStartTime() + 500  )
   if ( s_RetryGetCorePluginsCounter < 10 )
   {
      s_RetryGetCorePluginsCounter++;
      handle_commands_send_to_vehicle(COMMAND_ID_GET_CORE_PLUGINS_INFO, 0, NULL, 0);
   }
   
   return false;
}

bool _commands_check_download_file_segments()
{
   if ( s_bHasCommandInProgress )
      return false;
   
   if ( s_uFileIdToDownload == 0 )
      return false;

   if ( (s_uFileToDownloadState == 1) && (s_uCountFileSegmentsToDownload == 0) )
      return false;

   if ( s_uFileToDownloadState != 1 )
   if ( s_CommandStartTime < g_TimeNow - 4000 )
   {
      handle_commands_send_to_vehicle(COMMAND_ID_DOWNLOAD_FILE, s_uFileIdToDownload | 0xFF000000, NULL, 0);
      return true;
   }

   if ( g_TimeNow < s_uLastFileSegmentRequestTime + 100 )
      return false;

   for( u32 u=0; u<s_uCountFileSegmentsToDownload; u++ )
   {
      if ( s_bListFileSegmentsToDownload[u] )
      {
         s_uLastFileSegmentRequestTime = g_TimeNow;
         handle_commands_send_single_oneway_command(0, COMMAND_ID_DOWNLOAD_FILE_SEGMENT, (s_uFileIdToDownload & 0xFFFF) | (u<<16), NULL, 0);
         return true;
      }
   }
   return false;
}


bool _commands_check_upload_file_segments()
{
   if ( s_bHasCommandInProgress )
      return false;
   
   if ( ! g_bHasFileUploadInProgress )
      return false;

   if ( g_CurrentUploadingFile.uFileId == 0 || 0 == g_CurrentUploadingFile.szFileName[0] )
   {
      g_bHasFileUploadInProgress = false;
      return false;
   }

   if ( 0 == g_CurrentUploadingFile.uTotalSegments )
   {
      g_bHasFileUploadInProgress = false;
      return false;    
   }

   if ( g_TimeNow < g_CurrentUploadingFile.uTimeLastUploadSegment + 100 )
      return false;

   if ( g_TimeNow < s_CommandLastResponseReceivedTime + 50 )
      return false;

   bool bUploadedAnySegment = false;
   for( u32 u=0; u<g_CurrentUploadingFile.uTotalSegments; u++ )
   {
      if ( g_CurrentUploadingFile.bSegmentsUploaded[u] )
         continue;

      bUploadedAnySegment = true;

      g_CurrentUploadingFile.currentUploadSegment.uSegmentSize = g_CurrentUploadingFile.uSegmentsSize[u];
      g_CurrentUploadingFile.currentUploadSegment.uSegmentIndex = u;

      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, (u8*)&g_CurrentUploadingFile.currentUploadSegment, sizeof(command_packet_upload_file_segment));
      memcpy(buffer + sizeof(command_packet_upload_file_segment), g_CurrentUploadingFile.pSegments[u], g_CurrentUploadingFile.uSegmentsSize[u]);
      handle_commands_send_to_vehicle(COMMAND_ID_UPLOAD_FILE_SEGMENT, 0, buffer, sizeof(command_packet_upload_file_segment) + g_CurrentUploadingFile.uSegmentsSize[u]);
      g_CurrentUploadingFile.uTimeLastUploadSegment = g_TimeNow;
      g_CurrentUploadingFile.uLastSegmentIndexUploaded = u;
      break;
   }

   if ( ! bUploadedAnySegment )
   {
     // No more segments to upload
     warnings_add("Finished uploading core plugins to vehicle.");
     g_bHasFileUploadInProgress = false;
   }
   return false;
}

void _commands_check_pending_radio_flags_changes()
{
   if ( ! s_bCommandsCheckForRadioFlagsWorking )
      return;

   if ( s_bCommandsWaitForRadioFlagsChangedConfirmation )
      return;

   if ( 0 == g_TimePendingRadioFlagsChanged )
      return;

   if ( -1 == g_PendingRadioFlagsConfirmationLinkId )
      return;

   if ( g_TimeNow < g_TimePendingRadioFlagsChanged+500 )
      return;

   //log_line("Checking for working new radio flags...");

   bool bHasNewPackets = false;
   for( int k=0; k<MAX_RADIO_STREAMS; k++ )
   {
      if ( g_pSM_RadioStats->radio_streams[k].timeLastRxPacket > g_TimePendingRadioFlagsChanged+500 )
         bHasNewPackets = true;
   }

   if ( (!bHasNewPackets) && (g_TimeNow >= g_TimePendingRadioFlagsChanged + TIMEOUT_RADIO_FRAMES_FLAGS_CHANGE_CONFIRMATION) )
   {
      log_softerror_and_alarm("HCommands: The new radio flags where not confirmed by working downlink from vehicle (timeout)." );

      log_line("Reverting radio links changes to last good ones.");
      memcpy(&(g_pCurrentModel->radioLinksParams), &g_LastGoodRadioLinksParams, sizeof(type_radio_links_parameters));
      g_pCurrentModel->updateRadioInterfacesRadioFlags();
      saveAsCurrentModel(g_pCurrentModel);
      send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);

      g_TimePendingRadioFlagsChanged = 0;
      g_PendingRadioFlagsConfirmationLinkId = -1;
      s_bCommandsCheckForRadioFlagsWorking = false;
      s_CommandType = 0;
      s_bHasCommandInProgress = false;

      MenuConfirmation* pMC = new MenuConfirmation("Unsupported Parameter","Your vehicle radio link does not support this combination of radio params.", -1, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      menu_invalidate_all();
      return;
   }

   if ( ! bHasNewPackets )
      return;

   log_line("New radio link flags working on the downlink. Sending confirmation to vehicle");

   s_bHasCommandInProgress = false;
   handle_commands_send_to_vehicle(COMMAND_ID_RADIO_LINK_FLAGS_CHANGED_CONFIRMATION, g_PendingRadioFlagsConfirmationLinkId, NULL, 0);
   s_bCommandsWaitForRadioFlagsChangedConfirmation = true;
}

void _handle_commands_on_command_timeout()
{
   if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS ||
        s_CommandType == COMMAND_ID_RADIO_LINK_FLAGS_CHANGED_CONFIRMATION )
   {
      if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS )
         log_softerror_and_alarm("HCommands: The new radio flags where not received by vehicle (no command acknolwedge received)." );
      else
         log_softerror_and_alarm("HCommands: The new radio flags where not acknowledged by vehicle (no confirmation received)." );

      log_line("Reverting radio links changes to last good ones.");
      memcpy(&(g_pCurrentModel->radioLinksParams), &g_LastGoodRadioLinksParams, sizeof(type_radio_links_parameters));            
      g_pCurrentModel->updateRadioInterfacesRadioFlags();
      saveAsCurrentModel(g_pCurrentModel);
      send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
         
      s_bCommandsWaitForRadioFlagsChangedConfirmation = false;
      s_bCommandsCheckForRadioFlagsWorking = false;
      g_TimePendingRadioFlagsChanged = 0;
      g_PendingRadioFlagsConfirmationLinkId = -1;
      MenuConfirmation* pMC = new MenuConfirmation("Unsupported Parameter","Your vehicle radio link does not support this combination of radio params.", -1, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      menu_invalidate_all();
   }
   else if ( s_CommandType == COMMAND_ID_GET_ALL_PARAMS_ZIP )
      warnings_add("Could not get vehicle settings.");
   else
   {
      Popup* p = new Popup("No response from vehicle. Command not applied.",0.25,0.4, 0.5, 4);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      menu_invalidate_all();
   }

   s_CommandType = 0;
   s_bHasCommandInProgress = false;
}

void handle_commands_loop()
{
   if ( g_bSearching )
      return;

   if ( _commands_check_send_get_settings() )
      return;

   _commands_check_pending_radio_flags_changes();

   // Check for out of bound responses (not expected responses)

   if ( ! s_bHasCommandInProgress )
   {
      _commands_check_for_response_messages_from_router();
      _commands_check_download_file_segments();
      _commands_check_upload_file_segments();
      return;
   }

   // A command is in progress

   // Did it timedout?

   if ( s_CommandResendCounter >= s_CommandMaxResendCounter && g_TimeNow > s_CommandStartTime + s_CommandTimeout )
   {
      log_softerror_and_alarm("HCommands: last command did not complete (timed out waiting for a response)." );
      popup_log_add_entry("Command timed out (No response from vehicle).");
      _handle_commands_on_command_timeout();
      return;
   }

   if ( g_TimeNow > s_CommandStartTime + s_CommandTimeout )
   {
      if ( s_CommandTimeout < 300 )
         s_CommandTimeout += 10;
      s_CommandResendCounter++;
      for( int i=0; i<=(s_CommandResendCounter/10); i++ )
      {
         if ( i != 0 )
            hardware_sleep_micros(500);
         handle_commands_send_current_command();
      }
   }

   if ( ! _commands_check_for_response_messages_from_router() )
      return;

   t_packet_header* pPH = (t_packet_header*)s_CommandReplyBuffer;
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_COMMANDS ) 
      return;
   t_packet_header_command_response* pPHCR = (t_packet_header_command_response*)(s_CommandReplyBuffer + sizeof(t_packet_header));

   if ( pPHCR->origin_command_counter != s_CommandCounter )
   {
      log_line_commands( "[CommandsTh] [Recv] Ignore response from vId %u, cmd resp nb. %d, origin cmd nb. %d, origin retry %d, type [%s], response flags: %d, extra info len. %d]", pPH->vehicle_id_src, pPHCR->response_counter, pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter, commands_get_description(pPHCR->origin_command_type), str_get_command_response_flags_string(pPHCR->command_response_flags), s_CommandReplyLength-sizeof(t_packet_header)-sizeof(t_packet_header_command_response));
      return;
   }

   log_line_commands( "[Commands] [Recv] Response from vId %u, cmd resp nb. %d, origin cmd nb. %d, origin retry %d, type [%s], response flags: %s, extra info len. %d]", pPH->vehicle_id_src, pPHCR->response_counter, pPHCR->origin_command_counter, pPHCR->origin_command_resend_counter, commands_get_description(pPHCR->origin_command_type), str_get_command_response_flags_string(pPHCR->command_response_flags), pPH->total_length-sizeof(t_packet_header)-sizeof(t_packet_header_command_response));

   s_CommandLastResponseReceivedTime = g_TimeNow;

   if ( pPHCR->command_response_flags & COMMAND_RESPONSE_FLAGS_UNKNOWN_COMMAND )
   {
      if ( pPHCR->origin_command_type == COMMAND_ID_UPLOAD_FILE_SEGMENT )
      {
         g_CurrentUploadingFile.uTotalSegments = 0;
         g_bHasFileUploadInProgress = false;
         s_CommandType = 0;
         s_bHasCommandInProgress = false;
         return;
      }

      popup_log_add_entry("Command not understood by the vehicle.");
      log_softerror_and_alarm("HCommands: last command failed on the vehicle: vehicle does not knows the command." );
      Popup* p = new Popup("Command not understood by the vehicle.",0.25,0.4, 0.5, 4);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      menu_invalidate_all();
      s_CommandType = 0;
      s_bHasCommandInProgress = false;
      return;
   }

   bool bSucceeded = false;
   if ( pPHCR->command_response_flags & COMMAND_RESPONSE_FLAGS_OK )
      bSucceeded = true;
   
   if ( ! bSucceeded )
   {
      popup_log_add_entry("Command failed on the vehicle side.");
      log_softerror_and_alarm("HCommands: last command failed on the vehicle." );
      Popup* p = new Popup("Command failed on vehicle side.",0.25,0.4, 0.5, 4);
      p->setIconId(g_idIconError, get_Color_IconError());
      popups_add_topmost(p);
      menu_invalidate_all();
      s_CommandType = 0;
      s_bHasCommandInProgress = false;
      return;
   }

   if ( ! handle_last_command_result() )
      s_bHasCommandInProgress = false;
}

bool handle_commands_send_to_vehicle(u8 commandType, u32 param, u8* pBuffer, int length)
{
   if ( NULL == g_pCurrentModel )
      return false;

   if ( s_bHasCommandInProgress )
   {
      log_line_commands( "[Commands] [Send] to vId %u, cmd nb. %d, retry: %d, type [%s], param: %u] Tried to send a new command while another one (nb %d) was in progress.", g_pCurrentModel->vehicle_id, s_CommandCounter+1, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam, s_CommandCounter);
      handle_commands_show_popup_progress();
      return false;
   }

   if ( NULL == g_pCurrentModel || g_pCurrentModel->is_spectator )
      return false;
   if ( pairing_getCurrentVehicleID() != g_pCurrentModel->vehicle_id )
      return false;

   s_CommandBufferLength = length;
   if ( NULL != pBuffer )
      memcpy(s_CommandBuffer, pBuffer, s_CommandBufferLength );

   s_CommandCounter++;
   s_CommandType = commandType;
   s_CommandParam = param;
   s_CommandResendCounter = 0;

   s_CommandReplyLength = 0;
   //if ( length > 0 )
   //   log_line_commands( "[Commands] [Sending] to vId %u, cmd nb. %d, retry %d, type [%s], param: %u, extra length: %d", g_pCurrentModel->vehicle_id, s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam, length);
   //else
   //   log_line_commands( "[Commands] [Sending] to vId %u, cmd nb. %d, retry %d, type [%s], param: %u", g_pCurrentModel->vehicle_id, s_CommandCounter, s_CommandResendCounter, commands_get_description(s_CommandType), s_CommandParam);
   popup_log_add_entry("Sending command %s to vehicle...", commands_get_description(s_CommandType));

   s_CommandTimeout = 70;
   s_CommandMaxResendCounter = 20;
   if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FREQUENCY )
      s_CommandTimeout = 150;
   if ( s_CommandType == COMMAND_ID_GET_MEMORY_INFO )
      s_CommandTimeout = 400;
   if ( s_CommandType == COMMAND_ID_GET_CPU_INFO )
      s_CommandTimeout = 400;
   if ( s_CommandType == COMMAND_ID_SET_CAMERA_PARAMETERS )
      s_CommandTimeout = 200;
   if ( s_CommandType == COMMAND_ID_SET_VIDEO_PARAMS )
      s_CommandTimeout = 200;
   if ( s_CommandType == COMMAND_ID_UPDATE_VIDEO_LINK_PROFILES )
      s_CommandTimeout = 200;
   if ( s_CommandType == COMMAND_ID_DOWNLOAD_FILE )
   {
      s_CommandTimeout = 500;
      s_CommandMaxResendCounter = 5;
   }
   if ( s_CommandType == COMMAND_ID_DOWNLOAD_FILE_SEGMENT )
   {
      s_CommandTimeout = 200;
      s_CommandMaxResendCounter = 10;
   }

   if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS )
   {
      s_CommandTimeout = 150;
      s_CommandMaxResendCounter = 12;
      bool bModelHasAtheros = false;
      if ( NULL != g_pCurrentModel )
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
            if ( (g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[i] & 0xFF) == RADIO_TYPE_ATHEROS )
            {
               log_line("Current model has Atheros radio cards. Using longer timeouts to send radio flags.");
               bModelHasAtheros = true;
            }

      if ( bModelHasAtheros )
      {
         s_CommandTimeout = 300;
         s_CommandMaxResendCounter = 20;
      }
   }
   if ( s_CommandType == COMMAND_ID_RADIO_LINK_FLAGS_CHANGED_CONFIRMATION )
   {
      s_CommandTimeout = 150;
      s_CommandMaxResendCounter = 12;
   }

   if ( s_CommandType == COMMAND_ID_SET_RADIO_LINK_FLAGS )
   {
      s_bCommandsCheckForRadioFlagsWorking = true;
      s_bCommandsWaitForRadioFlagsChangedConfirmation = false;
   }

   handle_commands_send_current_command();

   s_bHasCommandInProgress = true;
   return true;
}

u32 handle_commands_increment_command_counter()
{
   s_CommandCounter++;
   return s_CommandCounter;
}

u32 handle_commands_decrement_command_counter()
{
   s_CommandCounter--;
   return s_CommandCounter; 
}

bool handle_commands_send_single_command_to_vehicle(u8 commandType, u8 resendCounter, u32 param, u8* pBuffer, int length)
{
   if ( NULL == g_pCurrentModel )
      return false;

   if ( s_bHasCommandInProgress )
   {
      log_line_commands( "[Commands] [Send] to vId %u, cmd nb. %d, retry: %d, type [%s], param: %u] Tried to send a new command while another one (nb %d) was in progress.", g_pCurrentModel->vehicle_id, s_CommandCounter+1, resendCounter, commands_get_description(commandType), param, s_CommandCounter);
      handle_commands_show_popup_progress();
      return false;
   }

   t_packet_header PH;
   t_packet_header_command PHC;

   PH.packet_flags = PACKET_COMPONENT_COMMANDS;
   PH.packet_type =  PACKET_TYPE_COMMAND;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_headers_length = sizeof(t_packet_header)+sizeof(t_packet_header_command);
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_command) + length;
   PH.extra_flags = 0;

   PHC.command_type = commandType;
   PHC.command_counter = s_CommandCounter;
   PHC.command_param = param;
   PHC.command_resend_counter = resendCounter;
  
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&PHC, sizeof(t_packet_header_command));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_command), pBuffer, length);
   packet_compute_crc(buffer, PH.total_length);
   send_packet_to_router(buffer, PH.total_length);
 
   log_line_commands("[CommandsTh] [Sent] to vId %u, cmd nb. %d, retry %d, type [%s], param: %u]", g_pCurrentModel->vehicle_id, s_CommandCounter, resendCounter, commands_get_description(commandType), param);
   return true;
}

bool handle_commands_send_single_oneway_command(u8 resendCounter, u8 commandType, u32 param, u8* pBuffer, int length)
{
   return handle_commands_send_single_oneway_command(resendCounter, commandType, param, pBuffer, length, 2);
}

bool handle_commands_send_single_oneway_command(u8 resendCounter, u8 commandType, u32 param, u8* pBuffer, int length, int delayMs)
{
   if ( NULL == g_pCurrentModel )
      return false;

   if ( s_bHasCommandInProgress )
   {
      log_line_commands( "[Commands] [Send] to vId %u, cmd nb. %d, retry: %d, type [%s], param: %u] Tried to send a new command while another one (nb %d) was in progress.", g_pCurrentModel->vehicle_id, s_CommandCounter+1, resendCounter, commands_get_description(commandType), param, s_CommandCounter);
      handle_commands_show_popup_progress();
      return false;
   }

   t_packet_header PH;
   t_packet_header_command PHC;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];

   PH.packet_flags = PACKET_COMPONENT_COMMANDS;
   PH.packet_type =  PACKET_TYPE_COMMAND;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_headers_length = sizeof(t_packet_header)+sizeof(t_packet_header_command);
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_command) + length;
   PH.extra_flags = 0;

   handle_commands_increment_command_counter();

   for( int i=0; i<=resendCounter; i++ )
   {
      PHC.command_type = commandType | COMMAND_TYPE_FLAG_NO_RESPONSE_NEEDED;
      PHC.command_counter = s_CommandCounter;
      PHC.command_param = param;
      PHC.command_resend_counter = i;
  
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), (u8*)&PHC, sizeof(t_packet_header_command));
      memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_command), pBuffer, length);
      packet_compute_crc(buffer, PH.total_length);
      send_packet_to_router(buffer, PH.total_length);
      hardware_sleep_ms(delayMs);
   }
   //log_line_commands("[Commands] [Sent] oneway to vId %u, cmd nb. %d, repeat count: %d, type [%s], param: %u]", g_pCurrentModel->vehicle_id, s_CommandCounter, resendCounter, commands_get_description(commandType), param);
   return true;
}

bool handle_commands_send_ruby_message(t_packet_header* pPH, u8* pBuffer, int length)
{
   if ( NULL == pPH )
      return false;

   if ( ! pairing_isStarted() )
      return false;

   pPH->vehicle_id_src = g_pCurrentModel->vehicle_id;
   pPH->vehicle_id_dest = g_pCurrentModel->vehicle_id;
   pPH->extra_flags = 0;
   pPH->total_headers_length = sizeof(t_packet_header)+length;
   pPH->total_length = pPH->total_headers_length;

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)pPH, sizeof(t_packet_header));
   if ( NULL != pBuffer )
      memcpy(buffer+sizeof(t_packet_header), pBuffer, length);
   packet_compute_crc(buffer, pPH->total_length);
   send_packet_to_router(buffer, pPH->total_length);
   return true;
}


void handle_commands_abandon_command()
{
   s_bHasCommandInProgress = false;
}

bool handle_commands_is_command_in_progress()
{
   return s_bHasCommandInProgress;
}

void handle_commands_show_popup_progress()
{
   Popup* p = new Popup("A command is in progress. Please wait for it to finish.",0.25,0.4, 0.5, 3);
   p->setIconId(g_idIconWarning, get_Color_IconWarning());
   popups_add_topmost(p);
}

void handle_commands_reset_has_received_vehicle_core_plugins_info()
{
   s_bHasReceivedVehicleCorePluginsInfo = false;
}

bool handle_commands_has_received_vehicle_core_plugins_info()
{
   return s_bHasReceivedVehicleCorePluginsInfo;
}

void handle_commands_initiate_file_upload(u32 uFileId, const char* szFileName)
{
   for( u32 i=0; i<g_CurrentUploadingFile.uTotalSegments; i++ )
   {
      if ( NULL != g_CurrentUploadingFile.pSegments[i] )
         free(g_CurrentUploadingFile.pSegments[i]);
      g_CurrentUploadingFile.pSegments[i] = NULL;
   }

   g_CurrentUploadingFile.uTotalSegments = 0;
   g_CurrentUploadingFile.uFileId = uFileId;
   g_CurrentUploadingFile.szFileName[0] = 0;
 
   g_bHasFileUploadInProgress = false;
 
   if ( NULL == szFileName || 0 == szFileName[0] )
      return;

    strlcpy(g_CurrentUploadingFile.szFileName, szFileName, sizeof(g_CurrentUploadingFile.szFileName));
 
   FILE* fd = fopen(szFileName, "rb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to open file for uploading to vehicle: [%s].", szFileName);
      g_CurrentUploadingFile.szFileName[0] = 0;
      return;
   }

   fseek(fd, 0, SEEK_END);
   long lSize = ftell(fd);
   fclose(fd);
   long lSegmentSize = 800;

   g_CurrentUploadingFile.uTotalSegments = lSize/lSegmentSize;
   for( u32 u=0; u<g_CurrentUploadingFile.uTotalSegments; u++ )
   {
      g_CurrentUploadingFile.uSegmentsSize[u] = lSegmentSize;
      g_CurrentUploadingFile.bSegmentsUploaded[u] = false;
      g_CurrentUploadingFile.pSegments[u] = (u8*) malloc(lSegmentSize);
      if ( NULL == g_CurrentUploadingFile.pSegments[u] )
      {
         log_softerror_and_alarm("Failed to allocate memory for uploading file segment to vehicle.");
         g_CurrentUploadingFile.szFileName[0] = 0;
         g_CurrentUploadingFile.uTotalSegments = 0;
         return;
      }
   }

   if ( lSize > g_CurrentUploadingFile.uTotalSegments*lSegmentSize )
   {
      g_CurrentUploadingFile.uSegmentsSize[g_CurrentUploadingFile.uTotalSegments] = lSize - g_CurrentUploadingFile.uTotalSegments*lSegmentSize ;
      g_CurrentUploadingFile.bSegmentsUploaded[g_CurrentUploadingFile.uTotalSegments] = false;
      g_CurrentUploadingFile.pSegments[g_CurrentUploadingFile.uTotalSegments] = (u8*) malloc(g_CurrentUploadingFile.uSegmentsSize[g_CurrentUploadingFile.uTotalSegments]);
      if ( NULL == g_CurrentUploadingFile.pSegments[g_CurrentUploadingFile.uTotalSegments] )
      {
         log_softerror_and_alarm("Failed to allocate memory for uploading file segment to vehicle.");
         g_CurrentUploadingFile.szFileName[0] = 0;
         g_CurrentUploadingFile.uTotalSegments = 0;
         return;
      }
      g_CurrentUploadingFile.uTotalSegments++;
   }

   log_line("Allocated %d bytes for %u segments to upload file [%s] to vehicle.", lSize, g_CurrentUploadingFile.uTotalSegments, g_CurrentUploadingFile.szFileName);
   
   fd = fopen(szFileName, "rb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to open file for uploading to vehicle: [%s].", szFileName);
      g_CurrentUploadingFile.szFileName[0] = 0;
      return;
   }

   for( u32 u=0; u<g_CurrentUploadingFile.uTotalSegments; u++ )
   {
      int nRes = fread(g_CurrentUploadingFile.pSegments[u], 1, g_CurrentUploadingFile.uSegmentsSize[u], fd);
      if ( nRes != g_CurrentUploadingFile.uSegmentsSize[u] )
      {
         log_softerror_and_alarm("Failed to read file segment %u for uploading file to vehicle.", u);
         g_CurrentUploadingFile.szFileName[0] = 0;
         g_CurrentUploadingFile.uTotalSegments = 0;
         fclose(fd);
         return;
      }
   }

   fclose(fd);

   log_line("Initiated file upload to vehicle: [%s], %d bytes, %u segments.", g_CurrentUploadingFile.szFileName, lSize, g_CurrentUploadingFile.uTotalSegments);
   g_CurrentUploadingFile.uTimeLastUploadSegment = 0;
   g_CurrentUploadingFile.currentUploadSegment.uFileId = g_CurrentUploadingFile.uFileId;
   g_CurrentUploadingFile.currentUploadSegment.uTotalFileSize = lSize;
   g_CurrentUploadingFile.currentUploadSegment.uTotalSegments = g_CurrentUploadingFile.uTotalSegments;
   g_CurrentUploadingFile.currentUploadSegment.uSegmentSize = lSegmentSize;
    strlcpy(g_CurrentUploadingFile.currentUploadSegment.szFileName, szFileName, sizeof(g_CurrentUploadingFile.currentUploadSegment.szFileName));

   g_CurrentUploadingFile.uLastSegmentIndexUploaded = 0xFFFFFFFF;
   g_bHasFileUploadInProgress = true;
}