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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <semaphore.h>
#include "../base/base.h"
#include "../base/shared_mem.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/hw_procs.h"
#include "../base/models.h"
#include "../base/launchers.h"
#include "../base/encr.h"
#include "../base/ruby_ipc.h"
#include "../base/camera_utils.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"

#include "shared_vars.h"
#include "timers.h"

#include "processor_tx_audio.h"
#include "processor_tx_video.h"
#include "process_received_ruby_messages.h"
#include "process_radio_in_packets.h"
#include "launchers_vehicle.h"
#include "utils_vehicle.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/fec.h" 
#include "radio_utils.h"
#include "packets_utils.h"
#include "ruby_rt_vehicle.h"
#include "process_local_packets.h"
#include "video_link_auto_keyframe.h"
#include "video_link_stats_overwrites.h"
#include "video_link_check_bitrate.h"
#include "relay_rx.h"
#include "relay_tx.h"


#define MAX_RECV_UPLINK_HISTORY 12
#define SEND_ALARM_MAX_COUNT 5

static int s_iCountCPULoopOverflows = 0;
static u32 s_uTimeLastCheckForRaspiDebugMessages = 0;

u32 s_MinVideoBlocksGapMilisec = 1;


u8 s_BufferCommandsReply[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferCommandsReply[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferCommandsReplyPos = 0;

u8 s_BufferTelemetryDownlink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferTelemetryDownlink[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferTelemetryDownlinkPos = 0;  

u8 s_BufferRCDownlink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferRCDownlink[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferRCDownlinkPos = 0;  

t_packet_queue s_QueueRadioPacketsOut;

u32 s_LoopCounter = 0;
u32 s_debugFramesCount = 0;
u32 s_debugVideoBlocksInCount = 0;
u32 s_debugVideoBlocksOutCount = 0;

bool s_bRadioReinitialized = false;

long s_lLastLiveLogFileOffset = -1;

u16 s_countTXVideoPacketsOutPerSec[2];
u16 s_countTXDataPacketsOutPerSec[2];
u16 s_countTXCompactedPacketsOutPerSec[2];

u8 s_bufferModelSettings[2048];
int s_bufferModelSettingsLength = 0;

u32 s_uTimeLastSentModelSettings = 0;

static u32 s_uTimeLastLoopOverloadError = 0;
static u32 s_LoopOverloadErrorCount = 0;

extern u32 s_uLastAlarms;
extern u32 s_uLastAlarmsFlags;
extern u32 s_uLastAlarmsRepeatCount;
extern u32 s_uLastAlarmsTime;
extern u32 s_uLastAlarmsCount;

int try_read_video_input(bool bDiscard);

void close_radio_interfaces()
{
   log_line("Closing all radio interfaces (rx/tx).");

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         hardware_radio_sik_close(i);
   }

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( pRadioInfo->openedForWrite )
         radio_close_interface_for_write(i);
   }

   radio_close_interfaces_for_read(); 

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }
   log_line("Closed all radio interfaces (rx/tx).");
}

int open_radio_interfaces()
{
   log_line("=========================================================");
   log_line("Opening RX/TX radio interfaces...");
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId > 0 )
      log_line("Relaying is enabled on radio link %d.", g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId);

   init_radio_in_packets_state();

   int countOpenedForRead = 0;
   int countOpenedForWrite = 0;


   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      g_SM_RadioStats.radio_interfaces[i].assignedRadioLinkId = iRadioLinkId;
      g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
      g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
   }

   // Init RX interfaces

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( pRadioHWInfo->lastFrequencySetFailed )
         continue;
      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      if ( iRadioLinkId < 0 )
      {
         log_softerror_and_alarm("No radio link is assigned to radio interface %d", i+1);
         continue;
      }
      if ( iRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
      {
         log_softerror_and_alarm("Invalid radio link (%d of %d) is assigned to radio interface %d", iRadioLinkId+1, g_pCurrentModel->radioLinksParams.links_count, i+1);
         continue;
      }

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         continue;

      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         continue;

      if ( (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) ||
           (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         {
            if ( hardware_radio_sik_open_for_read_write(i) > 0 )
            {
               g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
               countOpenedForRead++;
            }
         }
         else
         {
            if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
               radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK);
            else
               radio_open_interface_for_read(i, RADIO_PORT_ROUTER_UPLINK);

            g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;      
            countOpenedForRead++;
         }
      }
   }

   if ( countOpenedForRead == 0 )
   {
      log_error_and_alarm("Failed to find or open any RX interface for receiving data.");
      close_radio_interfaces();
      return -1;
   }


   // Init TX interfaces

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( pRadioHWInfo->lastFrequencySetFailed )
         continue;
      int iRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      if ( iRadioLinkId < 0 )
      {
         log_softerror_and_alarm("No radio link is assigned to radio interface %d", i+1);
         continue;
      }
      if ( iRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
      {
         log_softerror_and_alarm("Invalid radio link (%d of %d) is assigned to radio interface %d", iRadioLinkId+1, g_pCurrentModel->radioLinksParams.links_count, i+1);
         continue;
      }

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
         continue;

      if ( (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) ||
           (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      {
         if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
         {
           g_SM_RadioStats.radio_interfaces[i].openedForWrite= 1;
           countOpenedForWrite++;
         }
         else
         {
            radio_open_interface_for_write(i);
            g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
            countOpenedForWrite++;
         }
      }
   }

   if ( 0 == countOpenedForWrite )
   {
      log_error_and_alarm("Can't find any TX interfaces for video/data or failed to init it.");
      close_radio_interfaces();
      return -1;
   }

   log_line("Opening RX/TX radio interfaces complete. %d interfaces for RX, %d interfaces for TX :", countOpenedForRead, countOpenedForWrite);
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      char szFlags[128];
      szFlags[0] = 0;
      str_get_radio_capabilities_description(g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i], szFlags);

      if ( pRadioHWInfo->openedForRead && pRadioHWInfo->openedForWrite )
         log_line(" * Interface %s, %s, %s opened for read/write, flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequency), szFlags );
      else if ( pRadioHWInfo->openedForRead )
         log_line(" * Interface %s, %s, %s opened for read, flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequency), szFlags );
      else if ( pRadioHWInfo->openedForWrite )
         log_line(" * Interface %s, %s, %s opened for write, flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequency), szFlags );
      else
         log_line(" * Interface %s, %s, %s not used. Flags: %s", pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequency), szFlags );
   }
   log_line("=========================================================");

   g_pCurrentModel->logVehicleRadioInfo();

   if ( ! process_data_tx_video_init() )
   {
      log_error_and_alarm("Failed to initialize video data tx processor.");
      close_radio_interfaces();
      return -1;
   }

   send_radio_config_to_controller();
   return 0;
}

void send_radio_config_to_controller()
{
   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("Tried to send current radio config to controller but there is no current model.");
      return;
   }
 
   log_line("Notify controller (send radio message) about current vehicle radio configuration.");

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_RUBY;
   PH.packet_type =  PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = PH.vehicle_id_src;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters);
   PH.extra_flags = 0;
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet + sizeof(t_packet_header), (u8*)&(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
   memcpy(packet + sizeof(t_packet_header) + sizeof(type_relay_parameters), (u8*)&(g_pCurrentModel->radioInterfacesParams), sizeof(type_radio_interfaces_parameters));
   memcpy(packet + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   send_packet_to_radio_interfaces(packet, PH.total_length);
}

void send_radio_reinitialized_message()
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_RUBY;
   PH.packet_type =  PACKET_TYPE_RUBY_RADIO_REINITIALIZED;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = PH.vehicle_id_src;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);
   PH.extra_flags = 0;
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));

   send_packet_to_radio_interfaces(packet, PH.total_length);
}

void reinit_radio_interfaces()
{
   if ( g_bQuit )
      return;
   char szComm[128];
   log_line("Reinit radio interfaces...");

   snprintf(szComm, sizeof(szComm), "touch %s", FILE_TMP_ALARM_ON);
   hw_execute_bash_command_silent(szComm, NULL);

   snprintf(szComm, sizeof(szComm), "touch %s", FILE_TMP_REINIT_RADIO_IN_PROGRESS);
   hw_execute_bash_command_silent(szComm, NULL);

   u32 uTimeStart = get_current_timestamp_ms();

   close_radio_interfaces();
   if ( g_bQuit )
      return;
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_CURRENT_RADIO_HW_CONFIG);
   hw_execute_bash_command(szComm, NULL);

   vehicle_stop_video_capture(g_pCurrentModel);

   // Clean up video pipe data
   if ( g_pCurrentModel->hasCamera() )
      for( int i=0; i<50; i++ )
         try_read_video_input(true);

   while ( true )
   {
      log_line("Try to do recovery action...");
      hardware_sleep_ms(600);
      if ( get_current_timestamp_ms() > uTimeStart + 20000 )
      {
         hw_execute_bash_command("sudo reboot -f", NULL);
         sleep(10);
      }

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      hw_execute_bash_command("/etc/init.d/udev restart", NULL);
      hardware_sleep_ms(200);
      hw_execute_bash_command("sudo systemctl restart networking", NULL);
      hardware_sleep_ms(200);
      hw_execute_bash_command("sudo ifconfig -a", NULL);

      hardware_sleep_ms(50);

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      char szOutput[4096];
      szOutput[0] = 0;
      hw_execute_bash_command_raw("sudo ifconfig -a | grep wlan", szOutput);
      if ( 0 == strlen(szOutput) )
         continue;

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      log_line("Reinitializing radio interfaces: found interfaces on ifconfig: [%s]", szOutput);
      snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_CURRENT_RADIO_HW_CONFIG);
      hw_execute_bash_command(szComm, NULL);
      hardware_reset_radio_enumerated_flag();
      hardware_enumerate_radio_interfaces();
      if ( 0 < hardware_get_radio_interfaces_count() )
         break;
   }

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }
   hw_execute_bash_command("ifconfig wlan0 down", NULL);
   hw_execute_bash_command("ifconfig wlan1 down", NULL);
   hw_execute_bash_command("ifconfig wlan2 down", NULL);
   hw_execute_bash_command("ifconfig wlan3 down", NULL);
   hardware_sleep_ms(200);

   hw_execute_bash_command("ifconfig wlan0 up", NULL);
   hw_execute_bash_command("ifconfig wlan1 up", NULL);
   hw_execute_bash_command("ifconfig wlan2 up", NULL);
   hw_execute_bash_command("ifconfig wlan3 up", NULL);
   
   snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_CURRENT_RADIO_HW_CONFIG);
   hw_execute_bash_command(szComm, NULL);
   // Remove radio initialize file flag
   hw_execute_bash_command("rm -rf tmp/ruby/conf_radios", NULL);
   hw_execute_bash_command("./ruby_initradio", NULL);
   
   hardware_sleep_ms(100);
   hardware_reset_radio_enumerated_flag();
   hardware_enumerate_radio_interfaces();

   log_line("=================================================================");
   log_line("Detected hardware radio interfaces:");
   hardware_log_radio_info();

   log_line("Setting all the cards frequencies again...");

   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] < 0 )
      {
         log_softerror_and_alarm("No radio link is assigned to radio interface %d", i+1);
         continue;
      }
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] >= g_pCurrentModel->radioLinksParams.links_count )
      {
         log_softerror_and_alarm("Invalid radio link (%d of max %d) is assigned to radio interface %d", i+1, g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1, g_pCurrentModel->radioLinksParams.links_count);
         continue;
      }
      g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i] = g_pCurrentModel->radioLinksParams.link_frequency[g_pCurrentModel->radioInterfacesParams.interface_link_id[i]];
      launch_set_frequency(g_pCurrentModel, i, g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i], g_pProcessStats );
   }
   log_line("Setting all the cards frequencies again. Done.");
   hardware_save_radio_info();
   hardware_sleep_ms(100);
 
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   open_radio_interfaces();

   log_line("Reinit radio interfaces: completed.");

   s_bRadioReinitialized = true;
   g_TimeRadioReinitialized = get_current_timestamp_ms();

   snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_TMP_REINIT_RADIO_IN_PROGRESS);
   hw_execute_bash_command_silent(szComm, NULL);

   snprintf(szComm, sizeof(szComm), "rm -rf %s", FILE_TMP_REINIT_RADIO_REQUEST);
   hw_execute_bash_command_silent(szComm, NULL); 


   // wait here so that whatchdog restarts everything
   while ( !g_bQuit )
   {
      hardware_sleep_ms(100);
      send_radio_reinitialized_message();
   }
}

void populate_model_settings_buffer()
{
   u8 dataCamera[1024];
   FILE* fd = fopen(FILE_TMP_CAMERA_NAME, "r");
   if ( NULL != fd )
   {
      int nSize = fread(dataCamera, 1, 1023, fd);
      fclose(fd);
      if ( nSize <= 0 )
         g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, "");
      else
      {
         dataCamera[MAX_CAMERA_NAME_LENGTH-1] = 0;
         for( int i=0; i<MAX_CAMERA_NAME_LENGTH; i++ )
            if ( dataCamera[i] == 10 || dataCamera[i] == 13 )
               dataCamera[i] = 0;
         g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, (char*)&(dataCamera[0]));
      }
   }
   else
      g_pCurrentModel->setCameraName(g_pCurrentModel->iCurrentCamera, "");

   g_pCurrentModel->saveToFile("tmp/tmp_download_model.mdl", false);

   char szBuff[128];
   hw_execute_bash_command("rm -rf tmp/model.tar 2>/dev/null", NULL);
   hw_execute_bash_command("rm -rf tmp/model.mdl 2>/dev/null", NULL);
   snprintf(szBuff, sizeof(szBuff), "cp -rf tmp/tmp_download_model.mdl tmp/model.mdl 2>/dev/null");
   hw_execute_bash_command(szBuff, NULL);
   hw_execute_bash_command("tar -czf tmp/model.tar tmp/model.mdl", NULL);

   s_bufferModelSettingsLength = 0;
   fd = fopen("tmp/model.tar", "rb");
   if ( NULL != fd )
   {
      s_bufferModelSettingsLength = fread(s_bufferModelSettings, 1, 2000, fd);
      fclose(fd);
      log_line("Generated buffer with compressed model settings. Compressed size: %d bytes", s_bufferModelSettingsLength);
   }
   else
      log_error_and_alarm("Failed to load vehicle configuration from file: model.tar");

   hw_execute_bash_command("rm -rf tmp/model.tar", NULL);
   hw_execute_bash_command("rm -rf tmp/model.mdl", NULL); 
}

void send_model_settings_to_controller()
{
   if ( 0 == s_bufferModelSettingsLength )
      populate_model_settings_buffer();
   if ( 0 == s_bufferModelSettingsLength || s_bufferModelSettingsLength > MAX_PACKET_PAYLOAD )
   {
      log_softerror_and_alarm("Invalid compressed model file size (%d). Skipping sending it to controller.", s_bufferModelSettingsLength);
      return;
   }

   log_line("Sending to controller all model settings. Compressed size: %d bytes", s_bufferModelSettingsLength); 
   log_line("Sending tar model file to controller. Total on time: %dm:%02ds, total mAh: %d", g_pCurrentModel->m_Stats.uCurrentOnTime/60, g_pCurrentModel->m_Stats.uCurrentOnTime%60, g_pCurrentModel->m_Stats.uCurrentTotalCurrent);

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_RUBY;
   PH.packet_type =  PACKET_TYPE_RUBY_MODEL_SETTINGS;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = PH.vehicle_id_src;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + s_bufferModelSettingsLength;
   PH.extra_flags = 0;
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+PH.total_headers_length, (u8*)&(s_bufferModelSettings[0]), s_bufferModelSettingsLength);

   send_packet_to_radio_interfaces(packet, PH.total_length);
   s_uTimeLastSentModelSettings = g_TimeNow;

   log_line("Send to controller all model settings. Compressed size: %d bytes", s_bufferModelSettingsLength); 
}

void _inject_video_link_dev_stats()
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_TELEMETRY;
   PH.packet_type = PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + sizeof(shared_mem_video_link_stats_and_overwrites);
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &g_SM_VideoLinkStats, sizeof(shared_mem_video_link_stats_and_overwrites));

   // Add it to the start of the queue, so it's extracted (poped) next
   packets_queue_inject_packet_first(&s_QueueRadioPacketsOut, packet);
}

void _inject_video_link_dev_graphs()
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_TELEMETRY;
   PH.packet_type = PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_GRAPHS;
   PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + sizeof(shared_mem_video_link_graphs);
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &g_SM_VideoLinkGraphs, sizeof(shared_mem_video_link_graphs));

   // Add it to the start of the queue, so it's extracted (poped) next
   packets_queue_inject_packet_first(&s_QueueRadioPacketsOut, packet);
}

void process_and_send_packets()
{
   int maxLengthAllowedInRadioPacket = MAX_PACKET_PAYLOAD;
   u8 composed_packet[MAX_PACKET_TOTAL_SIZE];
   int composed_packet_length = 0;
   bool bMustInjectVideoDevStats = false;
   bool bMustInjectVideoDevGraphs = false;

   while ( packets_queue_has_packets(&s_QueueRadioPacketsOut) )
   {
      int length = -1;
      u8* pBuffer = packets_queue_pop_packet(&s_QueueRadioPacketsOut, &length);
      if ( NULL == pBuffer || -1 == length )
         break;

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      bMustInjectVideoDevStats = false;
      bMustInjectVideoDevGraphs = false;

      t_packet_header* pPH = (t_packet_header*)pBuffer;
      pPH->packet_flags &= (~PACKET_FLAGS_BIT_CAN_START_TX);

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
      {
         // Update Ruby telemetry info if we are sending Ruby telemetry to controller
         // Update also the telemetry extended extra info: retransmissions info

         if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
         {
            if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS) )
               bMustInjectVideoDevStats = true;
            if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_ADAPTIVE_VIDEO_GRAPH) )
               bMustInjectVideoDevStats = true;
            if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS) )
               bMustInjectVideoDevGraphs = true;

            t_packet_header_ruby_telemetry_extended_v2* pPHRTE = (t_packet_header_ruby_telemetry_extended_v2*) (pBuffer + sizeof(t_packet_header));
            pPHRTE->downlink_tx_video_bitrate = g_pProcessorTxVideo->getCurrentVideoBitrateAverage500Ms() + get_audio_bps();
            pPHRTE->downlink_tx_video_all_bitrate = g_pProcessorTxVideo->getCurrentTotalVideoBitrateAverage500Ms() + get_audio_bps();
            pPHRTE->downlink_tx_data_bitrate = 0;

            pPHRTE->downlink_tx_video_packets_per_sec = s_countTXVideoPacketsOutPerSec[0] + s_countTXVideoPacketsOutPerSec[1];
            pPHRTE->downlink_tx_data_packets_per_sec = s_countTXDataPacketsOutPerSec[0] + s_countTXDataPacketsOutPerSec[1];
            pPHRTE->downlink_tx_compacted_packets_per_sec = s_countTXCompactedPacketsOutPerSec[0] + s_countTXCompactedPacketsOutPerSec[1];

            for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
            {
               // 0...127 regular datarates, 128+: mcs rates;
               if ( get_last_tx_used_datarate(i, 0) >= 0 )
                  pPHRTE->downlink_datarates[i][0] = get_last_tx_used_datarate(i, 0);
               else
                  pPHRTE->downlink_datarates[i][0] = 127 - get_last_tx_used_datarate(i, 0);

               if ( get_last_tx_used_datarate(i, 1) >= 0 )
                  pPHRTE->downlink_datarates[i][0] = get_last_tx_used_datarate(i, 1);
               else
                  pPHRTE->downlink_datarates[i][0] = 127 - get_last_tx_used_datarate(i, 1);

               pPHRTE->uplink_datarate[i] = g_UplinkInfoRxStats[i].lastReceivedDataRate;
               pPHRTE->uplink_rssi_dbm[i] = g_UplinkInfoRxStats[i].lastReceivedDBM + 200;

               pPHRTE->uplink_link_quality[i] = g_SM_RadioStats.radio_interfaces[i].rxQuality;
            }

            pPHRTE->txTimePerSec = 0;
            if ( NULL != g_pCurrentModel )
               for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
                  pPHRTE->txTimePerSec += g_TXMiliSecTimePerSecond[i];

            // Update extra info: retransmissions
            
            if ( pPHRTE->extraSize > 0 )
            if ( pPHRTE->extraSize == sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) )
            if ( pPH->total_length == (sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v2) + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
            {
                memcpy( pBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v2), (u8*)&g_PHTE_Retransmissions, sizeof(g_PHTE_Retransmissions));
            }
         }
      }

      if ( composed_packet_length + length > maxLengthAllowedInRadioPacket )
      {
         send_packet_to_radio_interfaces(composed_packet, composed_packet_length);
         composed_packet_length = 0;
      }
 
      memcpy(&composed_packet[composed_packet_length], pBuffer, length);
      composed_packet_length += length;

      if ( bMustInjectVideoDevStats )
         _inject_video_link_dev_stats();
      if ( bMustInjectVideoDevGraphs )
         _inject_video_link_dev_graphs();
   }

   if ( composed_packet_length > 0 )
   {
      send_packet_to_radio_interfaces(composed_packet, composed_packet_length);
      composed_packet_length = 0;
   } 
}


int try_read_video_input(bool bDiscard)
{
   if ( -1 == s_fInputVideoStream )
      return 0;

   fd_set readset;
   FD_ZERO(&readset);
   FD_SET(s_fInputVideoStream, &readset);

   struct timeval timePipeInput;
   timePipeInput.tv_sec = 0;
   timePipeInput.tv_usec = 100; // 0.1 miliseconds timeout

   int selectResult = select(s_fInputVideoStream+1, &readset, NULL, NULL, &timePipeInput);
   if ( selectResult <= 0 )
      return 0;

   if( 0 == FD_ISSET(s_fInputVideoStream, &readset) )
      return 0;

   int count = read(s_fInputVideoStream, process_data_tx_video_get_current_buffer_to_read_pointer(), process_data_tx_video_get_current_buffer_to_read_size());
   if ( count < 0 )
   {
      log_error_and_alarm("Failed to read from video input fifo: %s, returned code: %d, error: %s", FIFO_RUBY_CAMERA1, count, strerror(errno));
      return -1;
   }

   if ( bDiscard )
      return 0;

   // We have a full video block ?

   if ( process_data_tx_video_on_data_read_complete(count) )
      s_debugVideoBlocksInCount++;
   return count;
}


void try_read_pipes()
{
   int maxPacketsToRead = 10;

   while ( (maxPacketsToRead > 0) && NULL != ruby_ipc_try_read_message(s_fIPCRouterFromCommands, 50, s_PipeTmpBufferCommandsReply, &s_PipeTmpBufferCommandsReplyPos, s_BufferCommandsReply) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferCommandsReply;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferCommandsReply); 
      else
         packets_queue_add_packet(&s_QueueRadioPacketsOut, s_BufferCommandsReply); 
   } 

   maxPacketsToRead = 10;
   while ( (maxPacketsToRead > 0) && NULL != ruby_ipc_try_read_message(s_fIPCRouterFromTelemetry, 50, s_PipeTmpBufferTelemetryDownlink, &s_PipeTmpBufferTelemetryDownlinkPos, s_BufferTelemetryDownlink) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferTelemetryDownlink;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferTelemetryDownlink); 
      else
      {
         packets_queue_add_packet(&s_QueueRadioPacketsOut, s_BufferTelemetryDownlink); 
         /*
         if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
         {
            if ( pPH->packet_type == PACKET_TYPE_TELEMETRY_ALL )
            {
               t_packet_header_fc_telemetry* pH = (t_packet_header_fc_telemetry*)(&s_BufferTelemetryDownlink[0] + sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry));
               log_line("Received from telemetry pipe: %d pitch", pH->pitch/100-180);
            }
            //else
            //   log_line("Received from telemetry pipe: other packet to download, type: %d, size: %d bytes", pPH->packet_type, pPH->total_length);
         }
         */
      }
   }

   maxPacketsToRead = 10;
   while ( (maxPacketsToRead > 0) && NULL != ruby_ipc_try_read_message(s_fIPCRouterFromRC, 50, s_PipeTmpBufferRCDownlink, &s_PipeTmpBufferRCDownlinkPos, s_BufferRCDownlink) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferRCDownlink;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferRCDownlink); 
      else
         packets_queue_add_packet(&s_QueueRadioPacketsOut, s_BufferRCDownlink); 
   }
}


// returns 1 if needs to stop/exit

int periodic_loop()
{
   s_LoopCounter++;
   s_debugFramesCount++;

   if ( (g_TimeNow > g_TimeStart + 2000) && ( g_TimeNow < g_TimeStart + 4000 ) )
   if ( g_TimeNow > s_uTimeLastSentModelSettings + 300 )
   {
      s_uTimeLastSentModelSettings = g_TimeNow;
      send_model_settings_to_controller();

      static bool s_bRouterCheckedForWriteFileSystem = false;
      static bool s_bRouterWriteFileSystemOk = false;

      if ( ! s_bRouterCheckedForWriteFileSystem )
      {
         log_line("Checking the file system for write access...");
         s_bRouterCheckedForWriteFileSystem = true;
         s_bRouterWriteFileSystemOk = false;

         hw_execute_bash_command("rm -rf tmp/testwrite.txt", NULL);
         FILE* fdTemp = fopen("tmp/testwrite.txt", "wb");
         if ( NULL == fdTemp )
         {
            g_pCurrentModel->alarms |= ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR;
            s_bRouterWriteFileSystemOk = false;
         }
         else
         {
            fprintf(fdTemp, "test1234\n");
            fclose(fdTemp);
            fdTemp = fopen("tmp/testwrite.txt", "rb");
            if ( NULL == fdTemp )
            {
               g_pCurrentModel->alarms |= ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR;
               s_bRouterWriteFileSystemOk = false;
            }
            else
            {
               char szTmp[256];
               if ( 1 != fscanf(fdTemp, "%s", szTmp) )
               {
                  g_pCurrentModel->alarms |= ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR;
                  s_bRouterWriteFileSystemOk = false;
               }
               else if ( 0 != strcmp(szTmp, "test1234") )
               {
                  g_pCurrentModel->alarms |= ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR;
                  s_bRouterWriteFileSystemOk = false;
               }
               else
                  s_bRouterWriteFileSystemOk = true;
               fclose(fdTemp);
               hw_execute_bash_command("rm -rf tmp/testwrite.txt", NULL);
            }
         }
         if ( ! s_bRouterWriteFileSystemOk )
            log_line("Checking the file system for write access: Failed.");
         else
            log_line("Checking the file system for write access: Succeeded.");
      }
      if ( s_bRouterCheckedForWriteFileSystem )
      {
         if ( ! s_bRouterWriteFileSystemOk )
           send_alarm_to_controller(ALARM_ID_VEHICLE_STORAGE_WRITE_ERRROR, 0, 1);
      }      
   }

   if ( radio_stats_periodic_update(&g_SM_RadioStats, g_TimeNow) )
   {
      // Send them to controller if needed
      bool bSend = false;
      if ( g_pCurrentModel )
      if ( g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_VEHICLE_RADIO_INTERFACES_STATS )
          bSend = true;

      if ( bSend )
      {   
         t_packet_header PH;
         PH.packet_flags = PACKET_COMPONENT_TELEMETRY;
         PH.packet_type =  PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_RX_CARDS_STATS;
         PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
         PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
         PH.vehicle_id_dest = g_uControllerId;
         PH.total_headers_length = sizeof(t_packet_header);
         PH.total_length = sizeof(t_packet_header) + sizeof(u8) + g_pCurrentModel->radioInterfacesParams.interfaces_count * sizeof(shared_mem_radio_stats_radio_interface);

         u8 packet[MAX_PACKET_TOTAL_SIZE];
         u8 count = g_pCurrentModel->radioInterfacesParams.interfaces_count;
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet + sizeof(t_packet_header), (u8*)&count, sizeof(u8));
         u8* pData = packet + sizeof(t_packet_header) + sizeof(u8);
         for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
         {
            memcpy(pData, &(g_SM_RadioStats.radio_interfaces[i]), sizeof(shared_mem_radio_stats_radio_interface));
            pData += sizeof(shared_mem_radio_stats_radio_interface);
         }
         packet_compute_crc((u8*)&PH, PH.total_length);
         packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);
      }
   }

   if ( g_bHasSikConfigCommandInProgress && (g_uTimeStartSiKConfigCommand != 0) )
   if ( g_TimeNow >= g_uTimeStartSiKConfigCommand+500 )
   {
      if ( hw_process_exists("ruby_sik_config") )
      {
         g_uTimeStartSiKConfigCommand = g_TimeNow - 400;
      }
      else
      {
         int iResult = -1;
         FILE* fd = fopen(FILE_TMP_SIK_CONFIG_FINISHED, "rb");
         if ( NULL != fd )
         {
            if ( 1 != fscanf(fd, "%d", &iResult) )
               iResult = -2;
            fclose(fd);
         }
         log_line("SiK radio configuration completed. Result: %d.", iResult);
         char szBuff[256];
         snprintf(szBuff, sizeof(szBuff), "rm -rf %s", FILE_TMP_SIK_CONFIG_FINISHED);
         hw_execute_bash_command(szBuff, NULL);
         g_bHasSikConfigCommandInProgress = false;
      }
   }
   if ( (! g_bDidSentRaspividBitrateRefresh) || (0 == g_TimeStartRaspiVid) )
   if ( g_TimeNow > g_TimeLastVideoCaptureProgramCheck + 500 )
   {
      g_TimeLastVideoCaptureProgramCheck = g_TimeNow;

      if ( 0 == g_TimeStartRaspiVid )
      {
         log_line("Detecting running video capture program...");
         bool bHasCompatibleCaptureProgram = false;
         if ( (!bHasCompatibleCaptureProgram) && hw_process_exists(VIDEO_RECORDER_COMMAND) )
            bHasCompatibleCaptureProgram = true;
         if ( (!bHasCompatibleCaptureProgram) && hw_process_exists(VIDEO_RECORDER_COMMAND_VEYE) )
            bHasCompatibleCaptureProgram = true;
         if ( (!bHasCompatibleCaptureProgram) && hw_process_exists(VIDEO_RECORDER_COMMAND_VEYE307) )
            bHasCompatibleCaptureProgram = true;
         if ( (!bHasCompatibleCaptureProgram) && hw_process_exists(VIDEO_RECORDER_COMMAND_VEYE_SHORT_NAME) )
            bHasCompatibleCaptureProgram = true;

         if ( ! bHasCompatibleCaptureProgram )
         {
            log_line("No compatible video capture program running (for sending video bitrate on the fly).");
            g_bDidSentRaspividBitrateRefresh = true;
         }
         else
         {
            log_line("Compatible video capture program is running (for sending video bitrate on the fly).");
            g_TimeStartRaspiVid = g_TimeNow - 500;
         }
      }

      if ( ! g_bDidSentRaspividBitrateRefresh )
      if ( 0 != g_TimeStartRaspiVid )
      if ( g_TimeNow > g_TimeStartRaspiVid + 2000)
      if ( NULL != g_pCurrentModel )
      {
         log_line("Send initial bitrate (%u bps) to video capture program.", g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate);
         send_control_message_to_raspivid( RASPIVID_COMMAND_ID_VIDEO_BITRATE, g_SM_VideoLinkStats.overwrites.currentSetVideoBitrate/100000 );
         g_bDidSentRaspividBitrateRefresh = true;
      }
   }

#ifdef FEATURE_ENABLE_RC_FREQ_SWITCH
   if ( (s_iPendingFrequencyChangeLinkId >= 0) && (s_uPendingFrequencyChangeTo > 100) &&
        (s_uTimeFrequencyChangeRequest != 0) && (g_TimeNow > s_uTimeFrequencyChangeRequest + VEHICLE_SWITCH_FREQUENCY_AFTER_MS) )
   {
      log_line("Processing pending RC trigger to change frequency to: %s on link: %d", str_format_frequency(s_uPendingFrequencyChangeTo), s_iPendingFrequencyChangeLinkId+1 );
      g_pCurrentModel->compute_active_radio_frequencies(true);

      for( int i=0; i<g_pCurrentModel->nic_count; i++ )
      {
         if ( g_pCurrentModel->nic_flags[i] & NIC_FLAG_DISABLED )
            continue;
         if ( i == s_iPendingFrequencyChangeLinkId )
         {
            launch_set_frequency(g_pCurrentModel, i, s_uPendingFrequencyChangeTo, g_pProcessStats); 
            g_pCurrentModel->nic_frequency[i] = s_uPendingFrequencyChangeTo;
         }
      }
      hardware_save_radio_info();
      g_pCurrentModel->compute_active_radio_frequencies(true);
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      log_line("Notifying all other components of the new link frequency.");

      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
      PH.packet_type = PACKET_TYPE_LOCAL_LINK_FREQUENCY_CHANGED;
      PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
      PH.vehicle_id_dest = 0;
      PH.total_headers_length = sizeof(t_packet_header);
      PH.total_length = sizeof(t_packet_header) + 2*sizeof(u32);
      PH.extra_flags = 0;
      u8 buffer[MAX_PACKET_TOTAL_SIZE];
      memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
      u32* pI = (u32*)((&buffer[0])+sizeof(t_packet_header));
      *pI = (u32)s_iPendingFrequencyChangeLinkId;
      pI++;
      *pI = s_uPendingFrequencyChangeTo;
      
      packet_compute_crc(buffer, PH.total_length);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;  

      write(s_fPipeTelemetryUplink, buffer, PH.total_length);
      write(s_fPipeToCommands, buffer, PH.total_length);
      log_line("Done notifying all other components about the frequency change.");
      s_iPendingFrequencyChangeLinkId = -1;
      s_uPendingFrequencyChangeTo = 0;
      s_uTimeFrequencyChangeRequest = 0;
   }
#endif

   int iMaxRxQuality = 0;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      if ( g_SM_RadioStats.radio_interfaces[i].rxQuality > iMaxRxQuality )
         iMaxRxQuality = g_SM_RadioStats.radio_interfaces[i].rxQuality;
        
   if ( g_SM_VideoLinkGraphs.vehicleRXQuality[0] == 255 || (iMaxRxQuality < g_SM_VideoLinkGraphs.vehicleRXQuality[0]) )
      g_SM_VideoLinkGraphs.vehicleRXQuality[0] = iMaxRxQuality;


   if ( g_TimeNow >= g_TimeLastPacketsOutPerSecCalculation + 500 )
   {
      g_TimeLastPacketsOutPerSecCalculation = g_TimeNow;
      s_countTXVideoPacketsOutPerSec[1] = s_countTXVideoPacketsOutPerSec[0] = 0;
      s_countTXDataPacketsOutPerSec[1] = s_countTXDataPacketsOutPerSec[0] = 0;
      s_countTXCompactedPacketsOutPerSec[1] = s_countTXCompactedPacketsOutPerSec[0] = 0;

      s_countTXVideoPacketsOutPerSec[0] = s_countTXVideoPacketsOutTemp;
      s_countTXDataPacketsOutPerSec[0] = s_countTXDataPacketsOutTemp;
      s_countTXCompactedPacketsOutPerSec[0] = s_countTXCompactedPacketsOutTemp;

      s_countTXVideoPacketsOutTemp = 0;
      s_countTXDataPacketsOutTemp = 0;
      s_countTXCompactedPacketsOutTemp = 0;
   }

   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_STATS_VIDEO_INFO)
   if ( g_TimeNow >= g_VideoInfoStats.uTimeLastUpdate + 200 )
   {
      update_shared_mem_video_info_stats( &g_VideoInfoStats, g_TimeNow);

      if ( NULL != g_pSM_VideoInfoStats )
         memcpy((u8*)g_pSM_VideoInfoStats, (u8*)&g_VideoInfoStats, sizeof(shared_mem_video_info_stats));
      else
      {
        g_pSM_VideoInfoStats = shared_mem_video_info_stats_open_for_write();
        if ( NULL == g_pSM_VideoInfoStats )
           log_error_and_alarm("Failed to open shared mem video info stats for write!");
        else
           log_line("Opened shared mem video info stats for write.");
      }
   }

   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_STATS_VIDEO_INFO)
   if ( g_TimeNow >= g_VideoInfoStatsRadioOut.uTimeLastUpdate + 200 )
   {
      update_shared_mem_video_info_stats( &g_VideoInfoStatsRadioOut, g_TimeNow);

      if ( NULL != g_pSM_VideoInfoStatsRadioOut )
         memcpy((u8*)g_pSM_VideoInfoStatsRadioOut, (u8*)&g_VideoInfoStatsRadioOut, sizeof(shared_mem_video_info_stats));
      else
      {
        g_pSM_VideoInfoStatsRadioOut = shared_mem_video_info_stats_radio_out_open_for_write();
        if ( NULL == g_pSM_VideoInfoStatsRadioOut )
           log_error_and_alarm("Failed to open shared mem video info stats radio out for write!");
        else
           log_line("Opened shared mem video info stats radio out for write.");
      }
   }


   if ( g_TimeNow >= g_TimeLastDebugFPSComputeTime + 1000 )
   {
         if( access( FILE_TMP_REINIT_RADIO_REQUEST, R_OK ) != -1 )
         {
            log_line("Received signal to reinitialize the radio modules.");
            reinit_radio_interfaces();
            return 1;
         }
   
         if ( NULL != g_pProcessStats )
            g_pProcessStats->lastActiveTime = g_TimeNow;

         //log_line("Loop FPS: %d", s_debugFramesCount);
         g_TimeLastDebugFPSComputeTime = g_TimeNow;
         s_debugFramesCount = 0;

         s_MinVideoBlocksGapMilisec = 500/(1+s_debugVideoBlocksInCount);
         if ( s_debugVideoBlocksInCount >= 500 )
            s_MinVideoBlocksGapMilisec = 0;
         if ( s_MinVideoBlocksGapMilisec > 40 )
            s_MinVideoBlocksGapMilisec = 40;

         s_debugVideoBlocksInCount = 0;
         s_debugVideoBlocksOutCount = 0;
   }

   if ( s_bRadioReinitialized )
   {
      if ( g_TimeNow < 5000 || g_TimeNow < g_TimeRadioReinitialized+5000 )
      {
         if ( (g_TimeNow/100)%2 )
            send_radio_reinitialized_message();
      }
      else
      {
         s_bRadioReinitialized = false;
         g_TimeRadioReinitialized = 0;
      }
   }

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG )
   if ( g_TimeNow > g_TimeLastLiveLogCheck + 100 )
   {
      g_TimeLastLiveLogCheck = g_TimeNow;
      FILE* fd = fopen(LOG_FILE_SYSTEM, "rb");
      if ( NULL != fd )
      {
         fseek(fd, 0, SEEK_END);
         long lSize = ftell(fd);
         if ( -1 == s_lLastLiveLogFileOffset )
            s_lLastLiveLogFileOffset = lSize;

         while ( lSize - s_lLastLiveLogFileOffset >= 100 )
         {
            fseek(fd, s_lLastLiveLogFileOffset, SEEK_SET);
            u8 buffer[1024];
            long lRead = fread(buffer, 1, 1023, fd);
            if ( lRead > 0 )
            {
               s_lLastLiveLogFileOffset += lRead;
               send_packet_vehicle_log(buffer, (int)lRead);
            }
            else
               break;
         }
         fclose(fd);
      }
   }

   if ( g_TimeNow >= g_TimeLastHistoryTxComputation + 50 )
   {
      g_TimeLastHistoryTxComputation = g_TimeNow;
      
      // Compute the averate tx gap

      g_PHVehicleTxStats.historyTxGapAvgMiliseconds[0] = 0xFF;
      if ( g_PHVehicleTxStats.tmp_uAverageTxCount > 1 )
         g_PHVehicleTxStats.historyTxGapAvgMiliseconds[0] = (g_PHVehicleTxStats.tmp_uAverageTxSum - g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0])/(g_PHVehicleTxStats.tmp_uAverageTxCount-1);
      else if ( g_PHVehicleTxStats.tmp_uAverageTxCount == 1 )
         g_PHVehicleTxStats.historyTxGapAvgMiliseconds[0] = g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0];

      // Compute average video packets interval        

      g_PHVehicleTxStats.historyVideoPacketsGapAvg[0] = 0xFF;
      if ( g_PHVehicleTxStats.tmp_uVideoIntervalsCount > 1 )
         g_PHVehicleTxStats.historyVideoPacketsGapAvg[0] = (g_PHVehicleTxStats.tmp_uVideoIntervalsSum - g_PHVehicleTxStats.historyVideoPacketsGapMax[0])/(g_PHVehicleTxStats.tmp_uVideoIntervalsCount-1);
      else if ( g_PHVehicleTxStats.tmp_uVideoIntervalsCount == 1 )
         g_PHVehicleTxStats.historyVideoPacketsGapAvg[0] = g_PHVehicleTxStats.historyVideoPacketsGapMax[0];
        

      if ( ! g_bVideoPaused )
      if ( g_pCurrentModel->bDeveloperMode )
      if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_SEND_BACK_VEHICLE_TX_GAP )
      {
         t_packet_header PH;
         PH.packet_flags = PACKET_COMPONENT_TELEMETRY;
         PH.packet_type =  PACKET_TYPE_RUBY_TELEMETRY_VEHICLE_TX_HISTORY;
         PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
         PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
         PH.vehicle_id_dest = 0;
         PH.total_headers_length = sizeof(t_packet_header) + sizeof(t_packet_header_vehicle_tx_history);
         PH.total_length = sizeof(t_packet_header) + sizeof(t_packet_header_vehicle_tx_history);

         g_PHVehicleTxStats.iSliceInterval = 50;
         g_PHVehicleTxStats.uCountValues = MAX_HISTORY_VEHICLE_TX_STATS_SLICES;
         u8 packet[MAX_PACKET_TOTAL_SIZE];
         memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
         memcpy(packet + sizeof(t_packet_header), (u8*)&g_PHVehicleTxStats, sizeof(t_packet_header_vehicle_tx_history));
         packet_compute_crc((u8*)&PH, PH.total_length);
         packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);
      }

      for( int i=MAX_HISTORY_RADIO_STATS_RECV_SLICES-1; i>0; i-- )
      {
         g_PHVehicleTxStats.historyTxGapMaxMiliseconds[i] = g_PHVehicleTxStats.historyTxGapMaxMiliseconds[i-1];
         g_PHVehicleTxStats.historyTxGapMinMiliseconds[i] = g_PHVehicleTxStats.historyTxGapMinMiliseconds[i-1];
         g_PHVehicleTxStats.historyTxGapAvgMiliseconds[i] = g_PHVehicleTxStats.historyTxGapAvgMiliseconds[i-1];
         g_PHVehicleTxStats.historyTxPackets[i] = g_PHVehicleTxStats.historyTxPackets[i-1];
         g_PHVehicleTxStats.historyVideoPacketsGapMax[i] = g_PHVehicleTxStats.historyVideoPacketsGapMax[i-1];
         g_PHVehicleTxStats.historyVideoPacketsGapAvg[i] = g_PHVehicleTxStats.historyVideoPacketsGapAvg[i-1];
      }
      g_PHVehicleTxStats.historyTxGapMaxMiliseconds[0] = 0xFF;
      g_PHVehicleTxStats.historyTxGapMinMiliseconds[0] = 0xFF;
      g_PHVehicleTxStats.historyTxGapAvgMiliseconds[0] = 0xFF;
      g_PHVehicleTxStats.historyTxPackets[0] = 0;
      g_PHVehicleTxStats.historyVideoPacketsGapMax[0] = 0xFF;
      g_PHVehicleTxStats.historyVideoPacketsGapAvg[0] = 0xFF;

      g_PHVehicleTxStats.tmp_uAverageTxSum = 0;
      g_PHVehicleTxStats.tmp_uAverageTxCount = 0;
      g_PHVehicleTxStats.tmp_uVideoIntervalsSum = 0;
      g_PHVehicleTxStats.tmp_uVideoIntervalsCount = 0;
   }


  
   if ( ! g_bVideoPaused )
   //if ( g_pCurrentModel->bDeveloperMode )
   if ( g_pCurrentModel->osd_params.osd_flags3[g_pCurrentModel->osd_params.layout] & OSD_FLAG3_SHOW_VIDEO_BITRATE_HISTORY )
   if ( g_TimeNow >= g_SM_DevVideoBitrateHistory.uLastComputeTime + g_SM_DevVideoBitrateHistory.uComputeInterval )
   {
      g_SM_DevVideoBitrateHistory.uLastComputeTime = g_TimeNow;
      
      g_SM_DevVideoBitrateHistory.uQuantizationOverflowValue = video_link_get_oveflow_quantization_value();

      for( int i=MAX_INTERVALS_VIDEO_BITRATE_HISTORY-1; i>0; i-- )
      {
         g_SM_DevVideoBitrateHistory.uHistMaxVideoDataRateMbps[i] = g_SM_DevVideoBitrateHistory.uHistMaxVideoDataRateMbps[i-1]; 
         g_SM_DevVideoBitrateHistory.uHistVideoQuantization[i] = g_SM_DevVideoBitrateHistory.uHistVideoQuantization[i-1]; 
         g_SM_DevVideoBitrateHistory.uHistVideoBitrateKb[i] = g_SM_DevVideoBitrateHistory.uHistVideoBitrateKb[i-1]; 
         g_SM_DevVideoBitrateHistory.uHistVideoBitrateAvgKb[i] = g_SM_DevVideoBitrateHistory.uHistVideoBitrateAvgKb[i-1]; 
         g_SM_DevVideoBitrateHistory.uHistTotalVideoBitrateAvgKb[i] = g_SM_DevVideoBitrateHistory.uHistTotalVideoBitrateAvgKb[i-1]; 
      }
      g_SM_DevVideoBitrateHistory.uHistVideoQuantization[0] = g_SM_VideoLinkStats.overwrites.currentH264QUantization;
      g_SM_DevVideoBitrateHistory.uHistMaxVideoDataRateMbps[0] = get_last_tx_video_datarate_mbps();
      g_SM_DevVideoBitrateHistory.uHistVideoBitrateKb[0] = g_pProcessorTxVideo->getCurrentVideoBitrate()/1000;
      g_SM_DevVideoBitrateHistory.uHistVideoBitrateAvgKb[0] = g_pProcessorTxVideo->getCurrentVideoBitrateAverage()/1000;
      g_SM_DevVideoBitrateHistory.uHistTotalVideoBitrateAvgKb[0] = g_pProcessorTxVideo->getCurrentTotalVideoBitrateAverage()/1000;
      
      if ( (0 == g_TimeStartRaspiVid) || (g_TimeNow < g_TimeStartRaspiVid + 3000) )
         g_SM_DevVideoBitrateHistory.uHistVideoQuantization[0] = 0xFF;

      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_TELEMETRY;
      PH.packet_type =  PACKET_TYPE_RUBY_TELEMETRY_DEV_VIDEO_BITRATE_HISTORY;
      PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
      PH.vehicle_id_dest = 0;
      PH.total_headers_length = sizeof(t_packet_header);
      PH.total_length = sizeof(t_packet_header) + sizeof(shared_mem_dev_video_bitrate_history);

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet + sizeof(t_packet_header), (u8*)&g_SM_DevVideoBitrateHistory, sizeof(shared_mem_dev_video_bitrate_history));
      packet_compute_crc((u8*)&PH, PH.total_length);
      packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);
   }

   if ( g_TimeLastNotificationRelayParamsChanged != 0 )
   if ( g_TimeNow >= g_TimeLastNotificationRelayParamsChanged )
      relay_on_relay_params_changed();

   return 0;
}

void _read_ipc_pipes()
{
   if ( g_TimeNow < g_TimeLastPipesCheck + 10 )
      return;

   g_TimeLastPipesCheck = g_TimeNow;
   try_read_pipes();

   if ( (s_uLastAlarms != 0) && (s_uLastAlarmsRepeatCount > 0 ) )
   if ( g_TimeNow >= s_uLastAlarmsTime + 50 )
   {
      s_uLastAlarmsRepeatCount--;
      s_uLastAlarmsTime = g_TimeNow;

      char szBuff[256];
      alarms_to_string(s_uLastAlarms, szBuff);
      if ( s_uLastAlarms & ALARM_ID_VEHICLE_VIDEO_TX_OVERLOAD )
         log_line("Sending alarm to controller. Alarms: %u = %s, tx time now: %u, tx time max allowed: %u, repeat count: %u/%d", s_uLastAlarms, szBuff, s_uLastAlarmsFlags & 0xFFFF, (s_uLastAlarmsFlags>>16), s_uLastAlarmsCount, (int)s_uLastAlarmsRepeatCount);
      else if ( s_uLastAlarms & ALARM_ID_VEHICLE_VIDEO_DATA_OVERLOAD )
         log_line("Sending alarm to controller. Alarms: %u = %s, bitrate now: %u kbps, max bitrate allowed: %u kbps, repeat count: %u/%d", s_uLastAlarms, szBuff, s_uLastAlarmsFlags & 0xFFFF, (s_uLastAlarmsFlags>>16), s_uLastAlarmsCount, (int)s_uLastAlarmsRepeatCount);
      else
         log_line("Sending alarm to controller. Alarms: %u = %s, flags: %u, repeat count: %u/%d", s_uLastAlarms, szBuff, s_uLastAlarmsFlags, s_uLastAlarmsCount, (int)s_uLastAlarmsRepeatCount);
      t_packet_header PH;
      PH.packet_flags = PACKET_COMPONENT_RUBY;
      PH.packet_type =  PACKET_TYPE_RUBY_ALARM;
      PH.stream_packet_idx = (STREAM_ID_DATA) << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
      PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
      PH.vehicle_id_dest = 0;
      PH.total_headers_length = sizeof(t_packet_header);
      PH.total_length = sizeof(t_packet_header) + 3*sizeof(u32);

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet+sizeof(t_packet_header), &s_uLastAlarms, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+sizeof(u32), &s_uLastAlarmsFlags, sizeof(u32));
      memcpy(packet+sizeof(t_packet_header)+2*sizeof(u32), &s_uLastAlarmsCount, sizeof(u32));
      packet_compute_crc((u8*)&PH, sizeof(t_packet_header)+3*sizeof(u32));
      packets_queue_add_packet(&s_QueueRadioPacketsOut, packet);
   }
}

void cleanUp()
{
   close_radio_interfaces();

   if ( -1 != s_fInputAudioStream )
      close(s_fInputAudioStream);
   if ( -1 != s_fInputVideoStream )
      close(s_fInputVideoStream);

   s_fInputAudioStream = -1;
   s_fInputVideoStream = -1;

   ruby_close_ipc_channel(s_fIPCRouterToCommands);
   ruby_close_ipc_channel(s_fIPCRouterFromCommands);
   ruby_close_ipc_channel(s_fIPCRouterToTelemetry);
   ruby_close_ipc_channel(s_fIPCRouterFromTelemetry);
   ruby_close_ipc_channel(s_fIPCRouterToRC);
   ruby_close_ipc_channel(s_fIPCRouterFromRC);

   s_fIPCRouterToCommands = -1;
   s_fIPCRouterFromCommands = -1;
   s_fIPCRouterToTelemetry = -1;
   s_fIPCRouterFromTelemetry = -1;
   s_fIPCRouterToRC = -1;
   s_fIPCRouterFromRC = -1;

   if ( NULL != g_pSharedMemRaspiVidComm )
      munmap(g_pSharedMemRaspiVidComm, SIZE_OF_SHARED_MEM_RASPIVID_COMM);
   g_pSharedMemRaspiVidComm = NULL;

   process_data_tx_video_uninit();
}
  
int router_open_pipes()
{
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   s_fIPCRouterFromRC = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_RC_TO_ROUTER);
   if ( s_fIPCRouterFromRC < 0 )
      return -1;

   s_fIPCRouterToRC = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_RC);
   if ( s_fIPCRouterToRC < 0 )
      return -1;

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   s_fIPCRouterToCommands = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_COMMANDS);
   if ( s_fIPCRouterToCommands < 0 )
   {
      cleanUp();
      return -1;
   }
   
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }
 
   s_fIPCRouterFromCommands = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_COMMANDS_TO_ROUTER);
   if ( s_fIPCRouterFromCommands < 0 )
   {
      cleanUp();
      return -1;
   }
   
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   s_fIPCRouterFromTelemetry = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER);
   if ( s_fIPCRouterFromTelemetry < 0 )
   {
      cleanUp();
      return -1;
   }

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   s_fIPCRouterToTelemetry = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY);
   if ( s_fIPCRouterToTelemetry < 0 )
   {
      cleanUp();
      return -1;
   }
   
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms(); 
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   s_fInputAudioStream = -1;
   s_fInputVideoStream = -1;

   if ( g_pCurrentModel->audio_params.has_audio_device && g_pCurrentModel->audio_params.enabled )
   {
      log_line("Opening audio input stream: %s", FIFO_RUBY_AUDIO1);
      s_fInputAudioStream = open(FIFO_RUBY_AUDIO1, O_RDONLY);
      if ( s_fInputAudioStream < 0 )
         log_softerror_and_alarm("Failed to open audio input stream: %s", FIFO_RUBY_AUDIO1);
      else
         log_line("Opened audio input stream: %s", FIFO_RUBY_AUDIO1);
   }
   else
   {
      log_line("Vehicle with no audio capture device or audio is disabled. Do not try to read audio stream.");
      s_fInputAudioStream = -1;
   }

   s_fInputVideoStream = -1;
   if ( g_pCurrentModel->hasCamera() )
   {
      log_line("Opening video input stream: %s", FIFO_RUBY_CAMERA1);
      s_fInputVideoStream = open( FIFO_RUBY_CAMERA1, O_RDONLY | RUBY_PIPES_EXTRA_FLAGS);
      if ( s_fInputVideoStream < 0 )
      {
         log_error_and_alarm("Failed to open video input stream: %s", FIFO_RUBY_CAMERA1);
         cleanUp();
         return -1;
      }
      log_line("Opened video input stream: %s", FIFO_RUBY_CAMERA1);
   }
   else
   {
      log_line("Vehicle with no camera. Do not try to read video stream.");
      s_fInputVideoStream = -1;
   }

   if ( -1 != s_fInputVideoStream )
      log_line("Pipe camera read end flags: %s", str_get_pipe_flags(fcntl(s_fInputVideoStream, F_GETFL)));
   return 0;
}

void _check_loop_consistency(int iStep, u32 uLastTotalTxPackets, u32 uLastTotalTxBytes, u32 tTime0, u32 tTime1, u32 tTime2, u32 tTime3, u32 tTime4, u32 tTime5)
{
   if ( g_TimeNow < g_TimeStart + 10000 )
      return;
   
   if ( g_TimeStartRaspiVid != 0 )
   if ( g_TimeNow < g_TimeStartRaspiVid + 4000 )
      return;

   if ( tTime5 > tTime0 + DEFAULT_MAX_LOOP_TIME_MILISECONDS )
   {
      uLastTotalTxPackets = g_SM_RadioStats.radio_links[0].totalTxPackets - uLastTotalTxPackets;
      uLastTotalTxBytes = g_SM_RadioStats.radio_links[0].totalTxBytes - uLastTotalTxBytes;

      s_iCountCPULoopOverflows++;
      if ( s_iCountCPULoopOverflows > 5 )
      if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandReceived + 5000 )
         send_alarm_to_controller(ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD,(tTime5-tTime0),1);

      if ( tTime5 >= tTime0 + 500 )
      if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandReceived + 5000 )
         send_alarm_to_controller(ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD,(tTime5-tTime0)<<16,1);

      if ( g_TimeNow > s_uTimeLastLoopOverloadError + 3000 )
          s_LoopOverloadErrorCount = 0;
      s_uTimeLastLoopOverloadError = g_TimeNow;
      s_LoopOverloadErrorCount++;
      if ( (s_LoopOverloadErrorCount < 10) || ((s_LoopOverloadErrorCount%20)==0) )
         log_softerror_and_alarm("Router loop(%d) took too long to complete (%u milisec (%u + %u + %u + %u + %u ms)), sent %u packets, %u bytes!!!", iStep, tTime5 - tTime0, tTime1-tTime0, tTime2-tTime1, tTime3-tTime2, tTime4-tTime3, tTime5-tTime4, uLastTotalTxPackets, uLastTotalTxBytes);
   }
   else
      s_iCountCPULoopOverflows = 0;

   if ( NULL != g_pProcessStats )
   if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandReceived + 5000 )
   {
      if ( g_pProcessStats->uMaxLoopTimeMs < tTime5 - tTime0 )
         g_pProcessStats->uMaxLoopTimeMs = tTime5 - tTime0;
      g_pProcessStats->uTotalLoopTime += tTime5 - tTime0;
      if ( 0 != g_pProcessStats->uLoopCounter )
         g_pProcessStats->uAverageLoopTimeMs = g_pProcessStats->uTotalLoopTime / g_pProcessStats->uLoopCounter;
   }
}

void _check_for_debug_raspi_messages()
{
   if ( g_TimeNow < s_uTimeLastCheckForRaspiDebugMessages + 2000 )
      return;
   s_uTimeLastCheckForRaspiDebugMessages = g_TimeNow;

   if( access( "tmp.cmd", R_OK ) == -1 )
      return;

   int iCommand = 0;
   int iParam = 0;

   hardware_sleep_ms(20);

   FILE* fd = fopen("tmp.cmd", "r");
   if ( NULL == fd )
      return;
   if ( 2 != fscanf(fd, "%d %d", &iCommand, &iParam) )
   {
      iCommand = 0;
      iParam = 0;
   } 

   fclose(fd);
   hw_execute_bash_command_silent("rm -rf tmp.cmd", NULL);

   if ( iCommand > 0 )
      send_control_message_to_raspivid((u8)iCommand, (u8)iParam);
}

void _broadcast_router_ready()
{
   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_VEHICLE_ROUTER_READY;
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = 0;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);
   PH.extra_flags = 0;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   packet_compute_crc(buffer, PH.total_length);

   if ( ! ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, buffer, PH.total_length) )
      log_softerror_and_alarm("No pipe to telemetry to broadcast router ready to.");

   log_line("Broadcasted that router is ready.");
} 

void handle_sigint(int sig) 
{ 
   g_bQuit = true;
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
} 

int main (int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   if ( strcmp(argv[argc-1], "-ver") == 0 )
   {
      printf("%d.%d (b%d)", SYSTEM_SW_VERSION_MAJOR, SYSTEM_SW_VERSION_MINOR/10, SYSTEM_SW_BUILD_NUMBER);
      return 0;
   }

   log_init("R-Router Vehicle");

   hardware_reload_serial_ports();

   hardware_enumerate_radio_interfaces(); 

   init_radio_in_packets_state();

   g_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_ROUTER_TX);
   if ( NULL == g_pProcessStats )
      log_softerror_and_alarm("Failed to open shared mem for router process watchdog for writing: %s", SHARED_MEM_WATCHDOG_ROUTER_TX);
   else
      log_line("Opened shared mem for router process watchdog for writing.");

   g_pCurrentModel = new Model();
   if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
   {
      log_error_and_alarm("Can't load current model vehicle. Exiting.");
      return -1;
   }

   if ( g_pCurrentModel->enc_flags != MODEL_ENC_FLAGS_NONE )
   {
      if ( ! lpp(NULL, 0) )
      {
         g_pCurrentModel->enc_flags = MODEL_ENC_FLAGS_NONE;
         g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);
      }
   }
  
   log_line("Loaded model. Developer flags: live log: %s, enable radio silence failsafe: %s, log only errors: %s, radio config guard interval: %d ms",
         (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)?"yes":"no",
         (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE)?"yes":"no",
         (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS)?"yes":"no",
          (int)((g_pCurrentModel->uDeveloperFlags >> 8) & 0xFF) );
   log_line("Model has vehicle developer video link stats flag on: %s/%s", (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS)?"yes":"no", (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS)?"yes":"no");


   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
      log_only_errors();

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   if ( -1 == router_open_pipes() )
      log_error_and_alarm("Failed to open some pipes.");
   
   radio_stats_reset(&g_SM_RadioStats, 100);
   
   g_SM_RadioStats.countRadioInterfaces = hardware_get_radio_interfaces_count();
   if ( NULL != g_pCurrentModel )
      g_SM_RadioStats.countRadioLinks = g_pCurrentModel->radioLinksParams.links_count;
   else
      g_SM_RadioStats.countRadioLinks = 1;

   
   if ( NULL != g_pCurrentModel )
      hw_set_priority_current_proc(g_pCurrentModel->niceRouter);

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   video_stats_overwrites_init();
  
   hardware_sleep_ms(50);
   radio_init_link_structures();
   radio_enable_crc_gen(1);
   fec_init();

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }


   if ( open_radio_interfaces() < 0 )
   {
      shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, g_pProcessStats);
      return -1;
   }

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   g_pProcessorTxVideo = new ProcessorTxVideo(0,0);
   
   packets_queue_init(&s_QueueRadioPacketsOut);
   packets_queue_init(&s_QueueControlPackets);
      
   if ( NULL != g_pCurrentModel && g_pCurrentModel->hasCamera() && g_pCurrentModel->isCameraCSICompatible() )
   {
      log_line("Opening video commands pipe write endpoint...");
      g_pSharedMemRaspiVidComm = (u8*)open_shared_mem_for_write(SHARED_MEM_RASPIVIDEO_COMMAND, SIZE_OF_SHARED_MEM_RASPIVID_COMM);
      if ( NULL == g_pSharedMemRaspiVidComm )
         log_error_and_alarm("Failed to open video commands pipe write endpoint!");
      else
         log_line("Opened video commands pipe write endpoint.");
   }

   g_pSM_VideoInfoStats = shared_mem_video_info_stats_open_for_write();
   if ( NULL == g_pSM_VideoInfoStats )
      log_error_and_alarm("Failed to open shared mem video info stats for write!");
   else
      log_line("Opened shared mem video info stats for write.");

   g_pSM_VideoInfoStatsRadioOut = shared_mem_video_info_stats_radio_out_open_for_write();
   if ( NULL == g_pSM_VideoInfoStatsRadioOut )
      log_error_and_alarm("Failed to open shared mem video info stats radio out for write!");
   else
      log_line("Opened shared mem video info stats radio out for write.");

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      memset(&g_UplinkInfoRxStats[i], 0, sizeof(type_uplink_rx_info_stats));
      g_UplinkInfoRxStats[i].lastReceivedDBM = 200;
      g_UplinkInfoRxStats[i].lastReceivedDataRate = 0;
      g_UplinkInfoRxStats[i].timeLastLogWrongRxPacket = 0;
   }
   relay_set_rx_info_stats(&(g_UplinkInfoRxStats[0]));

   s_countTXVideoPacketsOutPerSec[0] = s_countTXVideoPacketsOutPerSec[1] = 0;
   s_countTXDataPacketsOutPerSec[0] = s_countTXDataPacketsOutPerSec[1] = 0;
   s_countTXCompactedPacketsOutPerSec[0] = s_countTXCompactedPacketsOutPerSec[1] = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      g_TXMiliSecTimePerSecond[i] = 0;

   for( int i=0; i<MAX_HISTORY_VEHICLE_TX_STATS_SLICES; i++ )
   {
      g_PHVehicleTxStats.historyTxGapMaxMiliseconds[i] = 0xFF;
      g_PHVehicleTxStats.historyTxGapMinMiliseconds[i] = 0xFF;
      g_PHVehicleTxStats.historyTxGapAvgMiliseconds[i] = 0xFF;
      g_PHVehicleTxStats.historyTxPackets[i] = 0;
      g_PHVehicleTxStats.historyVideoPacketsGapMax[i] = 0xFF;
      g_PHVehicleTxStats.historyVideoPacketsGapAvg[i] = 0xFF;
   }
   g_PHVehicleTxStats.tmp_uAverageTxSum = 0;
   g_PHVehicleTxStats.tmp_uAverageTxCount = 0;
   g_PHVehicleTxStats.tmp_uVideoIntervalsSum = 0;
   g_PHVehicleTxStats.tmp_uVideoIntervalsCount = 0;

   g_TimeLastHistoryTxComputation = get_current_timestamp_ms();
   g_TimeLastTxPacket = get_current_timestamp_ms();

   memset((u8*)&g_SM_DevVideoBitrateHistory, 0, sizeof(shared_mem_dev_video_bitrate_history));
   g_SM_DevVideoBitrateHistory.uComputeInterval = 200;
   g_SM_DevVideoBitrateHistory.uSlices = MAX_INTERVALS_VIDEO_BITRATE_HISTORY;

   log_line("");
   log_line("");
   log_line("----------------------------------------------"); 
   log_line("         Started all ok. Running now."); 
   log_line("----------------------------------------------"); 
   log_line("");
   log_line("");

   g_TimeNow = get_current_timestamp_ms();
   g_TimeStart = get_current_timestamp_ms(); 

   if ( NULL != g_pProcessStats )
   {
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastRadioTxTime = g_TimeNow;
      g_pProcessStats->lastRadioRxTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   u32 uLastTotalTxPackets = 0;
   u32 uLastTotalTxBytes = 0;

   u32 uTimeLastMemoryCheck = 0;
   u32 uCountMemoryChecks = 0;

   _broadcast_router_ready();

   u32 uLoopMicroInterval = 1000;

   while ( !g_bQuit )
   {
      g_TimeNow = get_current_timestamp_ms();
      u32 tTime0 = g_TimeNow;

      if ( NULL != g_pProcessStats )
      {
         g_pProcessStats->uLoopCounter++;
         g_pProcessStats->lastActiveTime = g_TimeNow;
      }
      
      _check_for_debug_raspi_messages();

      if ( (0 == uCountMemoryChecks && (g_TimeNow > g_TimeStart+6000)) || (g_TimeNow > uTimeLastMemoryCheck + 60000) )
      {
         uCountMemoryChecks++;
         uTimeLastMemoryCheck = g_TimeNow;
         char szOutput[2048];
         if ( 1 == hw_execute_bash_command_raw("df -m /home/pi/ruby | grep root", szOutput) )
         {
            char szTemp[1024];
            long lb, lu, lMemoryFreeMb;
            sscanf(szOutput, "%s %ld %ld %ld", szTemp, &lb, &lu, &lMemoryFreeMb);
            if ( lMemoryFreeMb < 100 )
               send_alarm_to_controller(ALARM_ID_VEHICLE_LOW_STORAGE_SPACE, (u32)lMemoryFreeMb, 5);
         }
      }

      uLastTotalTxPackets = g_SM_RadioStats.radio_links[0].totalTxPackets;
      uLastTotalTxBytes = g_SM_RadioStats.radio_links[0].totalTxBytes;

      process_data_tx_video_loop();

      if ( g_bQuit )
         break;

      u32 tTime1 = get_current_timestamp_ms();

      // Try to recv packets for max uEndRecvTimeMicro microsec.
      int readResult = 0;
      u32 uStartRecvTimeMicro = get_current_timestamp_micros();
      u32 uEndRecvTimeMicro = uStartRecvTimeMicro + uLoopMicroInterval;
      while ( uStartRecvTimeMicro < uEndRecvTimeMicro )
      {
         readResult = try_receive_radio_packets(uEndRecvTimeMicro-uStartRecvTimeMicro);
         if ( readResult < 0 || g_bQuit )
            break; 

         if( readResult > 0 )
            process_received_radio_packets();
         u32 t = get_current_timestamp_micros();
         if ( t < uStartRecvTimeMicro )
            break;
         uStartRecvTimeMicro = t;
      }
      if ( readResult < 0 || g_bQuit )
         break;

      u32 tTime2 = get_current_timestamp_ms();

      if ( periodic_loop() )
         break;

      video_stats_overwrites_periodic_loop();

      _read_ipc_pipes();

      process_local_control_packets();

      if ( s_fInputAudioStream > 0 )
         try_read_audio_input(s_fInputAudioStream);

      int iReadCameraBytes = 0;
      int iReadCameraCount = 0;
      do
      {
         iReadCameraCount++;
         if ( g_pCurrentModel->hasCamera() )
            iReadCameraBytes = try_read_video_input(false);
         if ( -1 == iReadCameraBytes )
            log_softerror_and_alarm("Failed to read camera stream.");
         if ( 0 == iReadCameraBytes )
            break;
      } while ( (iReadCameraBytes > 0) && (iReadCameraCount < 5) );

      u32 tTime3 = get_current_timestamp_ms();

      int videoPacketsReadyToSend = process_data_tx_video_has_packets_ready_to_send();
      int hasVideoBlockReadyToSendIndex = process_data_tx_video_has_block_ready_to_send();

      bool bSendPacketsNow = false;

      if ( g_bVideoPaused || (! g_pCurrentModel->hasCamera()) )
         bSendPacketsNow = true;

      if ( 0 != g_uTimeLastCommandSowftwareUpload )
      if ( g_TimeNow < g_uTimeLastCommandSowftwareUpload + 2000 )
         bSendPacketsNow = true;

      if ( g_pCurrentModel->clock_sync_type == CLOCK_SYNC_TYPE_BASIC )
      if ( videoPacketsReadyToSend > 0 )
         bSendPacketsNow = true;

      if ( g_pCurrentModel->clock_sync_type == CLOCK_SYNC_TYPE_ADV )
      if ( -1 != hasVideoBlockReadyToSendIndex )
         bSendPacketsNow = true;

      if ( packets_queue_has_packets(&s_QueueRadioPacketsOut) )
      if ( (s_QueueRadioPacketsOut.timeFirstPacket < g_TimeNow-100) || (g_pCurrentModel->clock_sync_type == CLOCK_SYNC_TYPE_NONE) )
         bSendPacketsNow = true;

      if ( bSendPacketsNow )
         process_and_send_packets();

      send_audio_packets();

      u32 tTime4 = get_current_timestamp_ms();

      if ( g_pCurrentModel->clock_sync_type == CLOCK_SYNC_TYPE_NONE ||
           g_pCurrentModel->clock_sync_type == CLOCK_SYNC_TYPE_BASIC )
      {
         if ( videoPacketsReadyToSend > 0 )
            process_data_tx_video_send_packets_ready_to_send(videoPacketsReadyToSend);

         u32 tTime5 = get_current_timestamp_ms();
         _check_loop_consistency(0, uLastTotalTxPackets, uLastTotalTxBytes, tTime0,tTime1, tTime2, tTime3, tTime4, tTime5);
         continue;
      }

      if ( -1 != hasVideoBlockReadyToSendIndex || g_TimeLastVideoBlockSent <= g_TimeNow - s_MinVideoBlocksGapMilisec )
      {         
         int countBlocksPendingToSend = process_data_tx_video_get_pending_blocks_to_send_count();
         while ( countBlocksPendingToSend > 0 )
         {
            process_data_tx_video_send_first_complete_block(countBlocksPendingToSend==1);
            g_TimeLastVideoBlockSent = get_current_timestamp_ms();
            s_debugVideoBlocksOutCount++;
            countBlocksPendingToSend--;
         }
      }

      u32 tTime5 = get_current_timestamp_ms();
      _check_loop_consistency(1, uLastTotalTxPackets, uLastTotalTxBytes, tTime0,tTime1, tTime2, tTime3, tTime4, tTime5); 
   }

   log_line("Stopping...");
   cleanUp();
   delete g_pProcessorTxVideo;
   shared_mem_video_info_stats_close(g_pSM_VideoInfoStats);
   shared_mem_video_info_stats_radio_out_close(g_pSM_VideoInfoStatsRadioOut);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, g_pProcessStats);
   log_line("Stopped.Exit now.");
   log_line("---------------------\n");
   return 0;
}

