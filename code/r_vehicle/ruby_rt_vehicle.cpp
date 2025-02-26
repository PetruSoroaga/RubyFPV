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
#include <sys/socket.h> 
#include <getopt.h>
#include <semaphore.h>
#include <pthread.h>
#include <ctype.h>
#include <poll.h>

#include "../base/base.h"
#include "../base/shared_mem.h"
#include "../base/config.h"
#include "../base/commands.h"
#include "../base/hw_procs.h"
#include "../base/models.h"
#include "../base/models_list.h"
#include "../base/radio_utils.h"
#include "../base/encr.h"
#include "../base/ruby_ipc.h"
#include "../base/camera_utils.h"
#include "../base/vehicle_settings.h"
#include "../base/vehicle_rt_info.h"
#include "../base/hardware_radio_serial.h"
#include "../common/string_utils.h"
#include "../common/radio_stats.h"
#include "../common/relay_utils.h"

#include "shared_vars.h"
#include "timers.h"

#include "processor_tx_audio.h"
#include "processor_tx_video.h"
#include "process_received_ruby_messages.h"
#include "process_radio_in_packets.h"
#include "launchers_vehicle.h"
#include "../utils/utils_vehicle.h"
#include "periodic_loop.h"

#include "../radio/radiopackets2.h"
#include "../radio/radiolink.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../radio/radio_duplicate_det.h"
#include "../radio/fec.h" 
#include "packets_utils.h"
#include "ruby_rt_vehicle.h"
#include "radio_links.h"
#include "events.h"
#include "process_local_packets.h"
#include "processor_relay.h"
#include "test_link_params.h"
#include "test_majestic.h"
#include "adaptive_video.h"
#include "video_source_csi.h"
#include "video_source_majestic.h"
#include "video_tx_buffers.h"

#define MAX_RECV_UPLINK_HISTORY 12
#define SEND_ALARM_MAX_COUNT 5

static int s_iCountCPULoopOverflows = 0;

u8 s_BufferCommandsReply[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferCommandsReply[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferCommandsReplyPos = 0;

u8 s_BufferTelemetryDownlink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferTelemetryDownlink[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferTelemetryDownlinkPos = 0;  

u8 s_BufferRCDownlink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBufferRCDownlink[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferRCDownlinkPos = 0;  

u16 s_countTXVideoPacketsOutPerSec[2];
u16 s_countTXDataPacketsOutPerSec[2];
u16 s_countTXCompactedPacketsOutPerSec[2];

u32 s_uTimeLastLoopOverloadError = 0;
u32 s_LoopOverloadErrorCount = 0;

extern u32 s_uLastAlarms;
extern u32 s_uLastAlarmsFlags1;
extern u32 s_uLastAlarmsFlags2;
extern u32 s_uLastAlarmsTime;
extern u32 s_uLastAlarmsCount;

u32 s_uTimeLastTryReadIPCMessages = 0;

bool links_set_cards_frequencies_and_params(int iLinkId)
{
   if ( NULL == g_pCurrentModel )
   {
      log_error_and_alarm("Invalid parameters for setting radio interfaces frequencies");
      return false;
   }

   if ( (iLinkId < 0) || (iLinkId >= hardware_get_radio_interfaces_count()) )
      log_line("Setting all cards frequencies and params according to vehicle radio links...");
   else
      log_line("Setting cards frequencies and params only for vehicle radio link %d", iLinkId+1);

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      
      int nRadioLinkId = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];
      if ( nRadioLinkId < 0 || nRadioLinkId >= g_pCurrentModel->radioLinksParams.links_count )
         continue;

      if ( ( iLinkId >= 0 ) && (nRadioLinkId != iLinkId) )
        continue;

      if ( ! pRadioHWInfo->isConfigurable )
      {
         radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, pRadioHWInfo->uCurrentFrequencyKhz);
         log_line("Links: Radio interface %d is not configurable. Skipping it.", i+1);
         continue;
      }

      u32 uRadioLinkFrequency = g_pCurrentModel->radioLinksParams.link_frequency_khz[nRadioLinkId];
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == nRadioLinkId )
      if ( g_pCurrentModel->relay_params.uRelayFrequencyKhz != 0 )
      {
         uRadioLinkFrequency = g_pCurrentModel->relay_params.uRelayFrequencyKhz;
         log_line("Setting relay frequency of %s for radio interface %d.", str_format_frequency(uRadioLinkFrequency), i+1);
      }
      if ( 0 == hardware_radio_supports_frequency(pRadioHWInfo, uRadioLinkFrequency ) )
      {
         log_softerror_and_alarm("Radio interface %d, %s, %s does not support radio link %d frequency %s. Did not changed the radio interface frequency.",
            i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, nRadioLinkId+1, str_format_frequency(uRadioLinkFrequency));
         continue;
      }
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         if ( iLinkId >= 0 )
            flag_update_sik_interface(i);
         else
         {
            u32 uFreqKhz = uRadioLinkFrequency;
            u32 uDataRate = g_pCurrentModel->radioLinksParams.link_datarate_data_bps[nRadioLinkId];
            u32 uTxPower = g_pCurrentModel->radioInterfacesParams.interface_raw_power[i];
            u32 uECC = (g_pCurrentModel->radioLinksParams.link_radio_flags[nRadioLinkId] & RADIO_FLAGS_SIK_ECC)? 1:0;
            u32 uLBT = (g_pCurrentModel->radioLinksParams.link_radio_flags[nRadioLinkId] & RADIO_FLAGS_SIK_LBT)? 1:0;
            u32 uMCSTR = (g_pCurrentModel->radioLinksParams.link_radio_flags[nRadioLinkId] & RADIO_FLAGS_SIK_MCSTR)? 1:0;

            bool bDataRateOk = false;
            for( int k=0; k<getSiKAirDataRatesCount(); k++ )
            {
               if ( (int)uDataRate == getSiKAirDataRates()[k] )
               {
                  bDataRateOk = true;
                  break;
               }
            }

            if ( ! bDataRateOk )
            {
               log_softerror_and_alarm("Invalid radio datarate for SiK radio: %d bps. Revert to %d bps.", uDataRate, DEFAULT_RADIO_DATARATE_SIK_AIR);
               uDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
            }
            
            int iRetry = 0;
            while ( iRetry < 2 )
            {
               int iRes = hardware_radio_sik_set_params(pRadioHWInfo, 
                      uFreqKhz,
                      DEFAULT_RADIO_SIK_FREQ_SPREAD, DEFAULT_RADIO_SIK_CHANNELS,
                      DEFAULT_RADIO_SIK_NETID,
                      uDataRate, uTxPower, 
                      uECC, uLBT, uMCSTR,
                      NULL);
               if ( iRes != 1 )
               {
                  log_softerror_and_alarm("Failed to configure SiK radio interface %d", i+1);
                  iRetry++;
               }
               else
               {
                  log_line("Updated successfully SiK radio interface %d to txpower %d, airrate: %d bps, ECC/LBT/MCSTR: %d/%d/%d",
                     i+1, uTxPower, uDataRate, uECC, uLBT, uMCSTR);
                  radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, uFreqKhz);
                  break;
               }
            }
         }
      }
      else
      {
         if ( radio_utils_set_interface_frequency(g_pCurrentModel, i, nRadioLinkId, uRadioLinkFrequency, g_pProcessStats, 0) )
            radio_stats_set_card_current_frequency(&g_SM_RadioStats, i, uRadioLinkFrequency);
      }
   }

   hardware_save_radio_info();

   return true;
}

void close_and_mark_sik_interfaces_to_reopen()
{
   int iCount = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      if ( pRadioHWInfo->openedForWrite || pRadioHWInfo->openedForRead )
      {
         if ( (g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex == -1 ) ||
              (g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex == i) )
         {
            radio_rx_pause_interface(i, "SiK config start, close interfaces");
            radio_tx_pause_radio_interface(i, "SiK config start, close interfaces");
            g_SiKRadiosState.bInterfacesToReopen[i] = true;
            hardware_radio_sik_close(i);
            iCount++;
         }
      }
   }
   log_line("[Router] Closed SiK radio interfaces (%d closed) and marked them for reopening.", iCount);
}

void reopen_marked_sik_interfaces()
{
   int iCount = 0;
   int iCountSikInterfacesOpened = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      if ( g_SiKRadiosState.bInterfacesToReopen[i] )
      {
         if ( hardware_radio_sik_open_for_read_write(i) <= 0 )
            log_softerror_and_alarm("[Router] Failed to reopen SiK radio interface %d", i+1);
         else
         {
            iCount++;
            iCountSikInterfacesOpened++;
            radio_tx_resume_radio_interface(i);
            radio_rx_resume_interface(i);
         }
      }
      g_SiKRadiosState.bInterfacesToReopen[i] = false;
   }

   log_line("[Router] Reopened SiK radio interfaces (%d reopened).", iCount);
}


void send_radio_config_to_controller()
{
   if ( NULL == g_pCurrentModel )
   {
      log_softerror_and_alarm("Tried to send current radio config to controller but there is no current model.");
      return;
   }
 
   log_line("Notify controller (send a radio message, from VID %u to VID %u) about current vehicle radio configuration.", g_pCurrentModel->uVehicleId, g_uControllerId);

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_RADIO_CONFIG_UPDATED, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters) + sizeof(type_radio_links_parameters);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet + sizeof(t_packet_header), (u8*)&(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));
   memcpy(packet + sizeof(t_packet_header) + sizeof(type_relay_parameters), (u8*)&(g_pCurrentModel->radioInterfacesParams), sizeof(type_radio_interfaces_parameters));
   memcpy(packet + sizeof(t_packet_header) + sizeof(type_relay_parameters) + sizeof(type_radio_interfaces_parameters), (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   //send_packet_to_radio_interfaces(packet, PH.total_length, -1);
   packets_queue_add_packet(&g_QueueRadioPacketsOut, packet);
}

void send_radio_reinitialized_message()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_RUBY_RADIO_REINITIALIZED, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = PH.vehicle_id_src;
   PH.total_length = sizeof(t_packet_header);

   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));

   send_packet_to_radio_interfaces(packet, PH.total_length, -1);
}


void video_source_capture_mark_needs_update_or_restart(u32 uChangeType)
{
   log_line("Router was flagged to restart video capture (reason: %d (%s))", uChangeType & 0xFF, str_get_model_change_type(uChangeType & 0xFF));
   if ( (NULL == g_pCurrentModel) || (!g_pCurrentModel->hasCamera()) )
   {
      log_line("Vehicle has no camera. Do not flag need for restarting video capture.");
      return;
   }

   u32 uReason = uChangeType & 0xFF;
   if ( (uReason == MODEL_CHANGED_VIDEO_RESOLUTION) ||
        (uReason == MODEL_CHANGED_VIDEO_CODEC) ||
        (uReason == MODEL_CHANGED_USER_SELECTED_VIDEO_PROFILE) )
      g_pVideoTxBuffers->discardBuffer();

   if ( NULL != g_pVideoTxBuffers )
      g_pVideoTxBuffers->updateVideoHeader(g_pCurrentModel);

   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      video_source_majestic_request_update_program(uChangeType);
   else if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      video_source_csi_request_restart_program();
   log_line("Requested video capture restart from video capture module.");
   log_line("Router: done handling of flagged to restart video capture (reason: %d (%s))", uChangeType & 0xFF, str_get_model_change_type(uChangeType & 0xFF));
}

void flag_update_sik_interface(int iInterfaceIndex)
{
   if ( g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex >= 0 )
   {
      log_line("Router was re-flagged to reconfigure SiK radio interface %d.", iInterfaceIndex+1);
      return;
   }
   log_line("Router was flagged to reconfigure SiK radio interface %d.", iInterfaceIndex+1);
   g_SiKRadiosState.uTimeIntervalSiKReinitCheck = 500;
   g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex = iInterfaceIndex;
   g_SiKRadiosState.iThreadRetryCounter = 0;

   close_and_mark_sik_interfaces_to_reopen();
}

void flag_reinit_sik_interface(int iInterfaceIndex)
{
   // Already flagged ?

   if ( g_SiKRadiosState.bMustReinitSiKInterfaces )
      return;

   log_softerror_and_alarm("Router was flagged to reinit SiK radio interfaces.");
   g_SiKRadiosState.uTimeIntervalSiKReinitCheck = 500;
   g_SiKRadiosState.iMustReconfigureSiKInterfaceIndex = -1;

   close_and_mark_sik_interfaces_to_reopen();

   g_SiKRadiosState.bMustReinitSiKInterfaces = true;
   g_SiKRadiosState.uSiKInterfaceIndexThatBrokeDown = (u32) iInterfaceIndex;
   g_SiKRadiosState.iThreadRetryCounter = 0;
}


void reinit_radio_interfaces()
{
   if ( g_bQuit || g_bReinitializeRadioInProgress )
      return;

   g_bReinitializeRadioInProgress = true;

   char szComm[128];
   log_line("Reinit radio interfaces (PID %d)...", getpid());

   sprintf(szComm, "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_ALARM_ON);
   hw_execute_bash_command_silent(szComm, NULL);

   sprintf(szComm, "touch %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_REINIT_RADIO_IN_PROGRESS);
   hw_execute_bash_command_silent(szComm, NULL);

   u32 uTimeStart = get_current_timestamp_ms();

   radio_links_close_rxtx_radio_interfaces();
   if ( g_bQuit )
   {
      g_bReinitializeRadioInProgress = false;
      return;
   }
   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   char szCommRadioParams[64];
   strcpy(szCommRadioParams, "-initradio");
   for ( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
   {
      if ( (g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF) == RADIO_TYPE_ATHEROS )
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] >= 0 )
      if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] < g_pCurrentModel->radioLinksParams.links_count )
      {
         int dataRateMb = g_pCurrentModel->radioLinksParams.link_datarate_video_bps[g_pCurrentModel->radioInterfacesParams.interface_link_id[i]];
         if ( dataRateMb > 0 )
            dataRateMb = dataRateMb / 1000 / 1000;
         if ( dataRateMb > 0 )
         {
            sprintf(szCommRadioParams, "-initradio %d", dataRateMb);
            break;
         }
      }
   }

   hardware_radio_remove_stored_config();
   
   if ( g_pCurrentModel->hasCamera() )
   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      video_source_csi_stop_program();

   // Clean up video pipe data

   #ifdef HW_PLATFORM_RASPBERRY
   if ( g_pCurrentModel->hasCamera() )
      video_source_csi_flush_discard();
   #endif

   while ( true )
   {
      log_line("Try to do recovery action...");
      hardware_sleep_ms(400);
      if ( get_current_timestamp_ms() > uTimeStart + 20000 )
      {
         g_bReinitializeRadioInProgress = false;
         hardware_reboot();
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
      hw_execute_bash_command("systemctl restart networking", NULL);
      hardware_sleep_ms(200);
      hw_execute_bash_command("ip link", NULL);

      hardware_sleep_ms(50);

      hw_execute_bash_command("systemctl stop networking", NULL);
      hardware_sleep_ms(200);
      hw_execute_bash_command("ip link", NULL);

      hardware_sleep_ms(50);

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      char szOutput[4096];
      szOutput[0] = 0;
      hw_execute_bash_command_raw("ip link | grep wlan", szOutput);
      if ( 0 == strlen(szOutput) )
         continue;

      if ( NULL != g_pProcessStats )
      {
         g_TimeNow = get_current_timestamp_ms();
         g_pProcessStats->lastActiveTime = g_TimeNow;
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      }

      log_line("Reinitializing radio interfaces: found interfaces on ip link: [%s]", szOutput);
      hardware_radio_remove_stored_config();
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
   //hw_execute_bash_command("ifconfig wlan0 down", NULL);
   //hw_execute_bash_command("ifconfig wlan1 down", NULL);
   //hw_execute_bash_command("ifconfig wlan2 down", NULL);
   //hw_execute_bash_command("ifconfig wlan3 down", NULL);
   hw_execute_bash_command("ip link set dev wlan0 down", NULL);
   hw_execute_bash_command("ip link set dev wlan1 down", NULL);
   hw_execute_bash_command("ip link set dev wlan2 down", NULL);
   hw_execute_bash_command("ip link set dev wlan3 down", NULL);
   hardware_sleep_ms(200);

   //hw_execute_bash_command("ifconfig wlan0 up", NULL);
   //hw_execute_bash_command("ifconfig wlan1 up", NULL);
   //hw_execute_bash_command("ifconfig wlan2 up", NULL);
   //hw_execute_bash_command("ifconfig wlan3 up", NULL);
   hw_execute_bash_command("ip link set dev wlan0 up", NULL);
   hw_execute_bash_command("ip link set dev wlan1 up", NULL);
   hw_execute_bash_command("ip link set dev wlan2 up", NULL);
   hw_execute_bash_command("ip link set dev wlan3 up", NULL);
   
   hardware_radio_remove_stored_config();
   
   // Remove radio initialize file flag
   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_RADIOS_CONFIGURED);
   hw_execute_bash_command(szComm, NULL);

   hw_execute_ruby_process_wait(NULL, "ruby_start", szCommRadioParams, NULL, 1);
   
   hardware_sleep_ms(100);
   hardware_reset_radio_enumerated_flag();
   hardware_enumerate_radio_interfaces();

   log_line("=================================================================");
   log_line("Detected hardware radio interfaces:");
   hardware_log_radio_info(NULL, 0);

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
      g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i] = g_pCurrentModel->radioLinksParams.link_frequency_khz[g_pCurrentModel->radioInterfacesParams.interface_link_id[i]];
      radio_utils_set_interface_frequency(g_pCurrentModel, i, g_pCurrentModel->radioInterfacesParams.interface_link_id[i], g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i], g_pProcessStats, 0 );
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

   radio_links_open_rxtx_radio_interfaces();

   log_line("Reinit radio interfaces: completed.");

   g_bRadioReinitialized = true;
   g_TimeRadioReinitialized = get_current_timestamp_ms();

   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_REINIT_RADIO_IN_PROGRESS);
   hw_execute_bash_command_silent(szComm, NULL);

   sprintf(szComm, "rm -rf %s%s", FOLDER_RUBY_TEMP, FILE_TEMP_REINIT_RADIO_REQUEST);
   hw_execute_bash_command_silent(szComm, NULL); 

   // wait here so that whatchdog restarts everything

   log_line("Reinit radio interfaces procedure complete. Now waiting for watchdog to restart everything (PID: %d).", getpid());
   int iCount = 0;
   int iSkip = 1;
   while ( !g_bQuit )
   {
      hardware_sleep_ms(50);
      iCount++;
      iSkip--;

      if ( iSkip == 0 )
      {
         send_radio_reinitialized_message();
         iSkip = iCount;
      }
   }
   log_line("Reinit radio interfaces procedure was signaled to quit (PID: %d).", getpid());

   g_bReinitializeRadioInProgress = false;

}

void send_model_settings_to_controller()
{
   log_line("Send model settings to controller. Notify rx_commands to do it.");
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_SEND_MODEL_SETTINGS, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header);

   ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);

   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != g_pProcessStats )
      g_pProcessStats->lastActiveTime = get_current_timestamp_ms();
}

void _inject_video_link_dev_stats_packet()
{
 // To fix
 /*
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_STATS, STREAM_ID_TELEMETRY);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + sizeof(shared_mem_video_link_stats_and_overwrites);
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &g_SM_VideoLinkStats, sizeof(shared_mem_video_link_stats_and_overwrites));

   // Add it to the start of the queue, so it's extracted (poped) next
   packets_queue_inject_packet_first(&g_QueueRadioPacketsOut, packet);
*/
}

void _inject_video_link_dev_graphs_packet()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_TELEMETRY, PACKET_TYPE_RUBY_TELEMETRY_VIDEO_LINK_DEV_GRAPHS, STREAM_ID_TELEMETRY);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + sizeof(shared_mem_video_link_graphs);
   u8 packet[MAX_PACKET_TOTAL_SIZE];
   memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
   memcpy(packet+sizeof(t_packet_header), &g_SM_VideoLinkGraphs, sizeof(shared_mem_video_link_graphs));

   // Add it to the start of the queue, so it's extracted (poped) next
   packets_queue_inject_packet_first(&g_QueueRadioPacketsOut, packet);
}

void _read_ipc_pipes(u32 uTimeNow)
{
   s_uTimeLastTryReadIPCMessages = uTimeNow;
   int maxToRead = 20;
   int maxPacketsToRead = maxToRead;

   while ( (maxPacketsToRead > 0) && (NULL != ruby_ipc_try_read_message(s_fIPCRouterFromCommands, s_PipeTmpBufferCommandsReply, &s_PipeTmpBufferCommandsReplyPos, s_BufferCommandsReply)) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferCommandsReply;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferCommandsReply); 
      else
         packets_queue_add_packet(&g_QueueRadioPacketsOut, s_BufferCommandsReply);
   } 
   if ( maxToRead - maxPacketsToRead > 6 )
      log_line("Read %d messages from commands msgqueue.", maxToRead - maxPacketsToRead);

   maxPacketsToRead = maxToRead;
   while ( (maxPacketsToRead > 0) && (NULL != ruby_ipc_try_read_message(s_fIPCRouterFromTelemetry, s_PipeTmpBufferTelemetryDownlink, &s_PipeTmpBufferTelemetryDownlinkPos, s_BufferTelemetryDownlink)) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferTelemetryDownlink;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferTelemetryDownlink); 
      else
         packets_queue_add_packet(&g_QueueRadioPacketsOut, s_BufferTelemetryDownlink); 
   }
   if ( maxToRead - maxPacketsToRead > 6 )
      log_line("Read %d messages from telemetry msgqueue.", maxToRead - maxPacketsToRead);

   maxPacketsToRead = maxToRead;
   while ( (maxPacketsToRead > 0) && (NULL != ruby_ipc_try_read_message(s_fIPCRouterFromRC, s_PipeTmpBufferRCDownlink, &s_PipeTmpBufferRCDownlinkPos, s_BufferRCDownlink)) )
   {
      maxPacketsToRead--;
      t_packet_header* pPH = (t_packet_header*)s_BufferRCDownlink;      
      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
         packets_queue_add_packet(&s_QueueControlPackets, s_BufferRCDownlink); 
      else
         packets_queue_add_packet(&g_QueueRadioPacketsOut, s_BufferRCDownlink); 
   }
   if ( maxToRead - maxPacketsToRead > 3 )
      log_line("Read %d messages from RC msgqueue.", maxToRead - maxPacketsToRead);
}

int _consume_ipc_messages()
{
   int iConsumed = 0;
   int iMaxToConsume = 20;

   while ( packets_queue_has_packets(&s_QueueControlPackets) && (iMaxToConsume > 0) )
   {
      iMaxToConsume--;
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      int length = -1;
      u8* pBuffer = packets_queue_pop_packet(&s_QueueControlPackets, &length);
      if ( NULL == pBuffer || -1 == length )
         break;

      t_packet_header* pPH = (t_packet_header*)pBuffer;
      process_local_control_packet(pPH);
      iConsumed++;
   }
   return iConsumed;
}

int process_and_send_packets()
{
   int iCountSent = 0;
   bool bMustInjectVideoDevStats = false;
   bool bMustInjectVideoDevGraphs = false;
   
   while ( packets_queue_has_packets(&g_QueueRadioPacketsOut) )
   {
      u32 uTime = get_current_timestamp_ms();
      if ( (0 != s_uTimeLastTryReadIPCMessages) && (uTime > s_uTimeLastTryReadIPCMessages + 500) )
      {
         log_softerror_and_alarm("Too much time since last ipc messages read (%u ms) while sending radio packets, read ipc messages.", uTime - s_uTimeLastTryReadIPCMessages);
         _read_ipc_pipes(uTime);
      }

      int iPacketLength = -1;
      u8* pPacketBuffer = packets_queue_pop_packet(&g_QueueRadioPacketsOut, &iPacketLength);
      if ( (NULL == pPacketBuffer) || (-1 == iPacketLength) )
         break;

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCIncomingTime = g_TimeNow;

      bMustInjectVideoDevStats = false;
      bMustInjectVideoDevGraphs = false;

      t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
      pPH->packet_flags &= (~PACKET_FLAGS_BIT_CAN_START_TX);

      if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_TELEMETRY )
      {
         // Update Ruby telemetry info if we are sending Ruby telemetry to controller
         // Update also the telemetry extended extra info: retransmissions info

         if ( pPH->packet_type == PACKET_TYPE_DEBUG_INFO )
         {
            type_u32_couters* pCounters = (type_u32_couters*) (pPacketBuffer + sizeof(t_packet_header));
            memcpy(pCounters, &g_CoutersMainLoop, sizeof(type_u32_couters));

            type_radio_tx_timers* pRadioTxInfo = (type_radio_tx_timers*) (pPacketBuffer + sizeof(t_packet_header) + sizeof(type_u32_couters));
            memcpy(pRadioTxInfo, &g_RadioTxTimers, sizeof(type_radio_tx_timers));
         }

         if ( pPH->packet_type == PACKET_TYPE_RUBY_TELEMETRY_EXTENDED )
         {
            if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS) )
               bMustInjectVideoDevStats = true;
            if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.iCurrentOSDLayout] & OSD_FLAG2_SHOW_ADAPTIVE_VIDEO_GRAPH) )
               bMustInjectVideoDevStats = true;
            if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS) )
               bMustInjectVideoDevGraphs = true;

            t_packet_header_ruby_telemetry_extended_v4* pPHRTE = (t_packet_header_ruby_telemetry_extended_v4*) (pPacketBuffer + sizeof(t_packet_header));
            pPHRTE->downlink_tx_video_bitrate_bps = g_pProcessorTxVideo->getCurrentVideoBitrateAverageLastMs(500);
            pPHRTE->downlink_tx_video_all_bitrate_bps = g_pProcessorTxVideo->getCurrentTotalVideoBitrateAverageLastMs(500);
            if ( NULL != g_pProcessorTxAudio )
               pPHRTE->downlink_tx_data_bitrate_bps += g_pProcessorTxAudio->getAverageAudioInputBps();
            else
               pPHRTE->downlink_tx_data_bitrate_bps = 0;

            pPHRTE->downlink_tx_video_packets_per_sec = s_countTXVideoPacketsOutPerSec[0] + s_countTXVideoPacketsOutPerSec[1];
            pPHRTE->downlink_tx_data_packets_per_sec = s_countTXDataPacketsOutPerSec[0] + s_countTXDataPacketsOutPerSec[1];
            pPHRTE->downlink_tx_compacted_packets_per_sec = s_countTXCompactedPacketsOutPerSec[0] + s_countTXCompactedPacketsOutPerSec[1];

            for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
            {
               // positive: regular datarates, negative: mcs rates;
               pPHRTE->last_sent_datarate_bps[i][0] = get_last_tx_used_datarate_bps_video(i);
               pPHRTE->last_sent_datarate_bps[i][1] = get_last_tx_used_datarate_bps_data(i);

               pPHRTE->last_recv_datarate_bps[i] = g_UplinkInfoRxStats[i].lastReceivedDataRate;

               pPHRTE->uplink_rssi_dbm[i] = g_UplinkInfoRxStats[i].lastReceivedDBM + 200;
               pPHRTE->uplink_noise_dbm[i] = g_UplinkInfoRxStats[i].lastReceivedNoiseDBM;
               pPHRTE->uplink_link_quality[i] = g_SM_RadioStats.radio_interfaces[i].rxQuality;
               if ( g_TimeLastReceivedRadioPacketFromController < g_TimeNow-2000 )
                  pPHRTE->uplink_link_quality[i] = 0;
               else if ( (g_TimeLastReceivedRadioPacketFromController < g_TimeNow-1500) && (pPHRTE->uplink_link_quality[i] > 20) )
                  pPHRTE->uplink_link_quality[i] = 20;
            }

            pPHRTE->txTimePerSec = g_RadioTxTimers.uComputedTotalTxTimeMilisecPerSecondAverage;

            // Update extra info: retransmissions
            
            if ( pPHRTE->extraSize > 0 )
            if ( pPHRTE->extraSize == sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions) )
            if ( pPH->total_length == (sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v4) + sizeof(t_packet_header_ruby_telemetry_extended_extra_info_retransmissions)) )
            {
               memcpy( pPacketBuffer + sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended_v4), (u8*)&g_PHTE_Retransmissions, sizeof(g_PHTE_Retransmissions));
            }
         }
      }
      if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_CONFIRMATION )
         log_line("Sending pairing request confirmation to controller (from VID %u to CID %u)", pPH->vehicle_id_src, pPH->vehicle_id_dest);

      send_packet_to_radio_interfaces(pPacketBuffer, iPacketLength, -1);
      iCountSent++;
      if ( bMustInjectVideoDevStats )
         _inject_video_link_dev_stats_packet();
      if ( bMustInjectVideoDevGraphs )
         _inject_video_link_dev_graphs_packet();
   }
   return iCountSent;
}

void _synchronize_shared_mems()
{
   /*
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDLayout] & OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO)
   if ( g_TimeNow >= g_VideoInfoStatsCameraOutput.uTimeLastUpdate + 200 )
   {
      update_shared_mem_video_frames_stats( &g_VideoInfoStatsCameraOutput, g_TimeNow);

      if ( NULL != g_pSM_VideoInfoStatsCameraOutput )
         memcpy((u8*)g_pSM_VideoInfoStatsCameraOutput, (u8*)&g_VideoInfoStatsCameraOutput, sizeof(shared_mem_video_frames_stats));
      else
      {
        g_pSM_VideoInfoStatsCameraOutput = shared_mem_video_frames_stats_open_for_write();
        if ( NULL == g_pSM_VideoInfoStatsCameraOutput )
           log_error_and_alarm("Failed to open shared mem video info camera stats for write!");
        else
           log_line("Opened shared mem video info stats camera for write.");
      }
   }
   */

   /*
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.iCurrentOSDLayout] & OSD_FLAG_SHOW_STATS_VIDEO_H264_FRAMES_INFO)
   if ( g_TimeNow >= g_VideoInfoStatsRadioOut.uTimeLastUpdate + 200 )
   {
      update_shared_mem_video_frames_stats( &g_VideoInfoStatsRadioOut, g_TimeNow);

      if ( NULL != g_pSM_VideoInfoStatsRadioOut )
         memcpy((u8*)g_pSM_VideoInfoStatsRadioOut, (u8*)&g_VideoInfoStatsRadioOut, sizeof(shared_mem_video_frames_stats));
      else
      {
        g_pSM_VideoInfoStatsRadioOut = shared_mem_video_frames_stats_radio_out_open_for_write();
        if ( NULL == g_pSM_VideoInfoStatsRadioOut )
           log_error_and_alarm("Failed to open shared mem video info stats radio out for write!");
        else
           log_line("Opened shared mem video info stats radio out for write.");
      }
   }
   */

   static u32 s_uTimeLastRxHistorySync = 0;

   if ( g_TimeNow >= s_uTimeLastRxHistorySync + 100 )
   {
      s_uTimeLastRxHistorySync = g_TimeNow;
      memcpy((u8*)g_pSM_HistoryRxStats, (u8*)&g_SM_HistoryRxStats, sizeof(shared_mem_radio_stats_rx_hist));
   }
}

void cleanUp()
{
   radio_links_close_rxtx_radio_interfaces();

   if ( NULL != g_pProcessorTxAudio )
   {
      g_pProcessorTxAudio->stopLocalRecording();
      g_pProcessorTxAudio->closeAudioStream();
   }
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->audio_params.has_audio_device) )
      vehicle_stop_audio_capture(g_pCurrentModel);

   if ( g_pCurrentModel->isActiveCameraOpenIPC() )
   {
      video_source_majestic_close();
      video_source_majestic_cleanup();
   }
   else
      video_source_csi_close();
   
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
   return 0;
}

void _check_loop_consistency(int iStep, u32 uLastTotalTxPackets, u32 uLastTotalTxBytes, u32 tTime0, u32 tTime1, u32 tTime2, u32 tTime3, u32 tTime4, u32 tTime5)
{
   if ( g_TimeNow < g_TimeStart + 10000 )
      return;
   
   if ( (get_video_capture_start_program_time() != 0) && ( g_TimeNow < get_video_capture_start_program_time() + 4000 ) )
      return;

   if ( tTime5 > tTime0 + DEFAULT_MAX_LOOP_TIME_MILISECONDS )
   {
      uLastTotalTxPackets = g_SM_RadioStats.radio_links[0].totalTxPackets - uLastTotalTxPackets;
      uLastTotalTxBytes = g_SM_RadioStats.radio_links[0].totalTxBytes - uLastTotalTxBytes;

      s_iCountCPULoopOverflows++;
      if ( s_iCountCPULoopOverflows > 5 )
      if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandReceived + 5000 )
      {
         log_line("Too many CPU loop overflows. Send alarm to controller (%d overflows, now overflow: %u ms)", s_iCountCPULoopOverflows, tTime5 - tTime0);
         send_alarm_to_controller(ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD,(tTime5-tTime0), 0, 2);
      }
      if ( tTime5 >= tTime0 + 500 )
      if ( g_TimeNow > g_TimeLastSetRadioFlagsCommandReceived + 5000 )
      {
         log_line("CPU loop overflow is too big (%d ms). Send alarm to controller", tTime5 - tTime0);
         send_alarm_to_controller(ALARM_ID_VEHICLE_CPU_LOOP_OVERLOAD,(tTime5-tTime0)<<16, 0, 2);
      }
      if ( g_TimeNow > s_uTimeLastLoopOverloadError + 3000 )
          s_LoopOverloadErrorCount = 0;
      s_uTimeLastLoopOverloadError = g_TimeNow;
      s_LoopOverloadErrorCount++;
      if ( (s_LoopOverloadErrorCount < 10) || ((s_LoopOverloadErrorCount%20)==0) )
         log_softerror_and_alarm("Router loop(%d) took too long to complete (%u milisec (%u + %u + %u + %u + %u ms)), sent %u packets, %u bytes!!!",
            iStep, tTime5 - tTime0, tTime1-tTime0, tTime2-tTime1, tTime3-tTime2, tTime4-tTime3, tTime5-tTime4, uLastTotalTxPackets, uLastTotalTxBytes);
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

void _check_rx_loop_consistency()
{
   int iAnyBrokeInterface = radio_rx_any_interface_broken();
   if ( iAnyBrokeInterface > 0 )
   {
      if ( hardware_radio_index_is_sik_radio(iAnyBrokeInterface-1) )
         flag_reinit_sik_interface(iAnyBrokeInterface-1);
      else
      {
         reinit_radio_interfaces();
         return;
      }
   }

   int iAnyRxTimeouts = radio_rx_any_rx_timeouts();
   if ( iAnyRxTimeouts > 0 )
   {
      static u32 s_uTimeLastAlarmRxTimeout = 0;
      if ( g_TimeNow > s_uTimeLastAlarmRxTimeout + 3000 )
      {
         s_uTimeLastAlarmRxTimeout = g_TimeNow;
         int iCount = radio_rx_get_timeout_count_and_reset(iAnyRxTimeouts-1);
         send_alarm_to_controller(ALARM_ID_VEHICLE_RX_TIMEOUT,(u32)(iAnyRxTimeouts-1), (u32)iCount, 1);
      }
   }

   int iAnyRxErrors = radio_rx_any_interface_bad_packets();
   if ( iAnyRxErrors > 0 )
   {
      int iError = radio_rx_get_interface_bad_packets_error_and_reset(iAnyRxErrors-1);
      send_alarm_to_controller(ALARM_ID_RECEIVED_INVALID_RADIO_PACKET, (u32)iError, 0, 1);
   }

   u32 uMaxRxLoopTime = radio_rx_get_and_reset_max_loop_time();
   if ( (int)uMaxRxLoopTime > get_VehicleSettings()->iDevRxLoopTimeout )
   {
      static u32 s_uTimeLastAlarmRxLoopOverload = 0;
      if ( g_TimeNow > s_uTimeLastAlarmRxLoopOverload + 10000 )
      {
         s_uTimeLastAlarmRxLoopOverload = g_TimeNow;
         u32 uRead = radio_rx_get_and_reset_max_loop_time_read();
         u32 uQueue = radio_rx_get_and_reset_max_loop_time_queue();
         if ( uRead > 0xFFFF ) uRead = 0xFFFF;
         if ( uQueue > 0xFFFF ) uQueue = 0xFFFF;

         send_alarm_to_controller(ALARM_ID_CPU_RX_LOOP_OVERLOAD, uMaxRxLoopTime, uRead | (uQueue<<16), 1);
      }
   }
}

u32 get_video_capture_start_program_time()
{
   if ( (NULL == g_pCurrentModel) || ( ! g_pCurrentModel->hasCamera()) )
      return 0;

   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      return video_source_cs_get_program_start_time();
   return video_source_majestic_get_program_start_time();
}

void _broadcast_router_ready()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_VEHICLE_ROUTER_READY, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = 0;
   PH.total_length = sizeof(t_packet_header);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   
   if ( ! ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, buffer, PH.total_length) )
      log_softerror_and_alarm("No pipe to telemetry to broadcast router ready to.");

   log_line("Broadcasted that router is ready.");
}

void _check_compute_send_rt_debug_info()
{
   vehicle_rt_info_check_advance_index(&g_VehicleRuntimeInfo, g_TimeNow);
   if ( g_TimeNow >= g_VehicleRuntimeInfo.uLastTimeSentToController + 100 )
   {
      g_VehicleRuntimeInfo.uLastTimeSentToController = g_TimeNow;

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_DEBUG_VEHICLE_RT_INFO, STREAM_ID_DATA);
      PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
      PH.vehicle_id_dest = PH.vehicle_id_src;
      PH.total_length = sizeof(t_packet_header) + sizeof(vehicle_runtime_info);

      u8 packet[MAX_PACKET_TOTAL_SIZE];
      memcpy(packet, (u8*)&PH, sizeof(t_packet_header));
      memcpy(packet + sizeof(t_packet_header), (u8*)&(g_VehicleRuntimeInfo), sizeof(vehicle_runtime_info));
      send_packet_to_radio_interfaces(packet, PH.total_length, -1);
   }
}


void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
   radio_rx_mark_quit();
} 

void _main_loop();

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

   log_init("Router");
   log_arguments(argc, argv);

   if ( strcmp(argv[argc-1], "test_maj") == 0 )
   {
      test_majestic();
      return 0;
   }

   g_uControllerId = vehicle_utils_getControllerId();
   load_VehicleSettings();

   VehicleSettings* pVS = get_VehicleSettings();
   if ( NULL != pVS )
      radio_rx_set_timeout_interval(pVS->iDevRxLoopTimeout);
   hardware_reload_serial_ports_settings();

   hardware_enumerate_radio_interfaces(); 

   reset_sik_state_info(&g_SiKRadiosState);

   g_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_ROUTER_TX);
   if ( NULL == g_pProcessStats )
      log_softerror_and_alarm("Start sequence: Failed to open shared mem for router process watchdog for writing: %s", SHARED_MEM_WATCHDOG_ROUTER_TX);
   else
      log_line("Start sequence: Opened shared mem for router process watchdog for writing.");

   loadAllModels();
   g_pCurrentModel = getCurrentModel();

   log_line("Current vehicle encryption type: %d", g_pCurrentModel->enc_flags);
   if ( g_pCurrentModel->enc_flags != MODEL_ENC_FLAGS_NONE )
   {
      if ( ! lpp(NULL, 0) )
      {
         g_pCurrentModel->enc_flags = MODEL_ENC_FLAGS_NONE;
         saveCurrentModel();
      }
   }
  
   log_line_forced_to_file("Start sequence: Loaded model. Developer flags: live log: %s, enable radio silence failsafe: %s, log only errors: %s, radio config guard interval: %d ms",
         (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LIVE_LOG)?"yes":"no",
         (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_RADIO_SILENCE_FAILSAFE)?"yes":"no",
         (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS)?"yes":"no",
          (int)((g_pCurrentModel->uDeveloperFlags >> 8) & 0xFF) );
   log_line_forced_to_file("Start sequence: Model has vehicle developer video link stats flag on: %s/%s", (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_STATS)?"yes":"no", (g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_ENABLE_VIDEO_LINK_GRAPHS)?"yes":"no");
   log_line_forced_to_file("Model flags: %u (%s), developer flags: %u (%s)",
      g_pCurrentModel->uModelFlags, str_get_model_flags(g_pCurrentModel->uModelFlags), g_pCurrentModel->uDeveloperFlags, str_get_developer_flags(g_pCurrentModel->uDeveloperFlags));
   log_line_forced_to_file("Start sequence: Vehicle has camera? %s", g_pCurrentModel->hasCamera()?"Yes":"No");
   log_line_forced_to_file("Start sequence: Board type: %s", str_get_hardware_board_name(hardware_getBoardType()));

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
      log_only_errors();

   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   log_line("Setting CPU speed for OpenIPC hardware...");
   hw_execute_bash_command_raw("echo 'performance' | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor", NULL);
   char szComm[256];
   sprintf(szComm, "echo %d | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq", g_pCurrentModel->processesPriorities.iFreqARM*1000);
   hw_execute_bash_command_raw(szComm, NULL);
   hw_execute_bash_command_raw("echo 700000 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq", NULL);
   #endif

   radio_rx_set_custom_thread_priority(g_pCurrentModel->processesPriorities.iThreadPriorityRadioRx);
   radio_tx_set_custom_thread_priority(g_pCurrentModel->processesPriorities.iThreadPriorityRadioTx);

   if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_DISABLE_ALL_LOGS )
   {
      log_line("Log is disabled on vehicle. Disabled logs.");
      log_disable();
   }

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   if ( -1 == router_open_pipes() )
      log_error_and_alarm("Start sequence: Failed to open some pipes.");
   
   if ( g_pCurrentModel->hasCamera() )
   {
      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      if ( video_source_csi_open(FIFO_RUBY_CAMERA1) <= 0 )
      {
         cleanUp();
         return -1;
      }

      if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      if ( video_source_majestic_open(MAJESTIC_UDP_PORT) <= 0 )
      {
         cleanUp();
         return -1;
      }
   }

   u32 uRefreshIntervalMs = 100;
   switch ( g_pCurrentModel->m_iRadioInterfacesGraphRefreshInterval )
   {
      case 0: uRefreshIntervalMs = 10; break;
      case 1: uRefreshIntervalMs = 20; break;
      case 2: uRefreshIntervalMs = 50; break;
      case 3: uRefreshIntervalMs = 100; break;
      case 4: uRefreshIntervalMs = 200; break;
      case 5: uRefreshIntervalMs = 500; break;
   }
   radio_stats_reset(&g_SM_RadioStats, uRefreshIntervalMs);
   vehicle_rt_info_init(&g_VehicleRuntimeInfo);

   g_SM_RadioStats.countLocalRadioInterfaces = hardware_get_radio_interfaces_count();
   if ( NULL != g_pCurrentModel )
      g_SM_RadioStats.countVehicleRadioLinks = g_pCurrentModel->radioLinksParams.links_count;
   else
      g_SM_RadioStats.countVehicleRadioLinks = 1;

   g_SM_RadioStats.countLocalRadioLinks = g_SM_RadioStats.countVehicleRadioLinks;
   
   for( int i=0; i<g_SM_RadioStats.countLocalRadioLinks; i++ )
      g_SM_RadioStats.radio_links[i].matchingVehicleRadioLinkId = i;
     
   //if ( NULL != g_pCurrentModel )
   //   hw_set_priority_current_proc(g_pCurrentModel->processesPriorities.iNiceRouter);

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   //To fix video_stats_overwrites_init();
   //To fix video_link_auto_keyframe_init();
  
   hardware_sleep_ms(50);
   radio_init_link_structures();

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_USE_PCAP_RADIO_TX )
      radio_set_use_pcap_for_tx(1);
   else
      radio_set_use_pcap_for_tx(0);
   
   if ( g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_BYPASS_SOCKETS_BUFFERS )
      radio_set_bypass_socket_buffers(1);
   else
      radio_set_bypass_socket_buffers(0);

   radio_enable_crc_gen(1);

   if ( g_bDeveloperMode )
   {
      radio_tx_set_dev_mode();
      radio_rx_set_dev_mode();
      radio_set_debug_flag();
   }
   packet_utils_init();

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   log_line("Start sequence: Setting radio interfaces frequencies...");
   links_set_cards_frequencies_and_params(-1);
   log_line("Start sequence: Done setting radio interfaces frequencies.");

   if ( radio_links_open_rxtx_radio_interfaces() < 0 )
   {
      shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, g_pProcessStats);
      radio_link_cleanup();
      return -1;
   }

   log_line("Start sequence: Done opening radio interfaces.");

   if ( NULL != g_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->lastIPCIncomingTime = g_TimeNow;
   }

   hardware_sleep_ms(50);
   log_line("Start sequence: Init radio queues...");

   packets_queue_init(&g_QueueRadioPacketsOut);
   packets_queue_init(&s_QueueControlPackets);

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      memset(&g_UplinkInfoRxStats[i], 0, sizeof(type_uplink_rx_info_stats));
      g_UplinkInfoRxStats[i].lastReceivedDBM = 200;
      g_UplinkInfoRxStats[i].lastReceivedDataRate = 0;
      g_UplinkInfoRxStats[i].timeLastLogWrongRxPacket = 0;
   }
   relay_init_and_set_rx_info_stats(&(g_UplinkInfoRxStats[0]));

   log_line("Start sequence: Done setting up radio queues.");

   s_countTXVideoPacketsOutPerSec[0] = s_countTXVideoPacketsOutPerSec[1] = 0;
   s_countTXDataPacketsOutPerSec[0] = s_countTXDataPacketsOutPerSec[1] = 0;
   s_countTXCompactedPacketsOutPerSec[0] = s_countTXCompactedPacketsOutPerSec[1] = 0;

   reset_radio_tx_timers(&g_RadioTxTimers);
   
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
   g_SM_DevVideoBitrateHistory.uGraphSliceInterval = 100;
   g_SM_DevVideoBitrateHistory.uTotalDataPoints = MAX_INTERVALS_VIDEO_BITRATE_HISTORY;
   g_SM_DevVideoBitrateHistory.uCurrentDataPoint = 0;
   g_SM_DevVideoBitrateHistory.uLastTimeSendToController = 0;

   log_line("Start sequence: Done setting up stats structures.");

   g_pSM_HistoryRxStats = shared_mem_radio_stats_rx_hist_open_for_write();

   if ( NULL == g_pSM_HistoryRxStats )
      log_softerror_and_alarm("Start sequence: Failed to open history radio rx stats shared memory for write.");
   else
      shared_mem_radio_stats_rx_hist_reset(&g_SM_HistoryRxStats);
  
   log_line("Start sequence: Done setting up radio stats history.");

   if ( g_pCurrentModel->hasCamera() )
   {
      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
         video_source_csi_start_program();
      if ( g_pCurrentModel->isActiveCameraOpenIPC() )
         video_source_majestic_init_all_params();
   }
   else
      log_line("Vehicle has no camera. Video capture not started.");

   log_line("Start sequence: Done starting camera.");
   
   adaptive_video_init();
   g_pVideoTxBuffers = new VideoTxPacketsBuffer(0,0);
   g_pVideoTxBuffers->init(g_pCurrentModel);
   
   g_pProcessorTxVideo = new ProcessorTxVideo(0,0);
   g_pProcessorTxVideo->init();

   log_line("Start sequence: Done creating video processor.");
   
   /*
   g_pSM_VideoInfoStatsCameraOutput = shared_mem_video_frames_stats_open_for_write();
   if ( NULL == g_pSM_VideoInfoStatsCameraOutput )
      log_error_and_alarm("Start sequence: Failed to open shared mem video camera info stats for write!");
   else
      log_line("Start sequence: Opened shared mem video camera info stats for write.");

   g_pSM_VideoInfoStatsRadioOut = shared_mem_video_frames_stats_radio_out_open_for_write();
   if ( NULL == g_pSM_VideoInfoStatsRadioOut )
      log_error_and_alarm("Start sequence: Failed to open shared mem video info stats radio out for write!");
   else
      log_line("Start sequence: Opened shared mem video info stats radio out for write.");

   memset(&g_VideoInfoStatsCameraOutput, 0, sizeof(shared_mem_video_frames_stats));
   memset(&g_VideoInfoStatsRadioOut, 0, sizeof(shared_mem_video_frames_stats));
   */

   g_pProcessorTxAudio = new ProcessorTxAudio();
   g_pProcessorTxAudio->init(g_pCurrentModel);
   if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->audio_params.has_audio_device) && (g_pCurrentModel->audio_params.enabled) )
   {
      vehicle_launch_audio_capture(g_pCurrentModel);
      g_pProcessorTxAudio->openAudioStream();
      // To remove
      g_pProcessorTxAudio->startLocalRecording();
   }
   else
      log_line("Start sequence: Audio is not enabled (has device: %s, enabled: %s).", g_pCurrentModel->audio_params.has_audio_device?"yes":"no", g_pCurrentModel->audio_params.enabled?"yes":"no");

   log_line("Start sequence: Done creating audio processor.");

   radio_duplicate_detection_init();
   radio_rx_start_rx_thread(&g_SM_RadioStats, 0, g_pCurrentModel->getVehicleFirmwareType());
   
   send_radio_config_to_controller();

   reset_counters(&g_CoutersMainLoop);
   log_line("Running main loop for sync type: %d", g_pCurrentModel->rxtx_sync_type);

   log_line("");
   log_line("");
   log_line("----------------------------------------------"); 
   log_line("         Started all ok. Running now."); 
   log_line("----------------------------------------------"); 
   log_line("");
   log_line("");

   hardware_sleep_ms(20);

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      
      if ( pRadioHWInfo->openedForRead && pRadioHWInfo->openedForWrite )
         log_line(" * Interface %d: %s, %s, %s opened for read/write, radio link %d", i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1 );
      else if ( pRadioHWInfo->openedForRead )
         log_line(" * Interface %d: %s, %s, %s opened for read, radio link %d", i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1 );
      else if ( pRadioHWInfo->openedForWrite )
         log_line(" * Interface %d: %s, %s, %s opened for write, radio link %d", i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1 );
      else
         log_line(" * Interface %d: %s, %s, %s not used, radio link %d", i+1, pRadioHWInfo->szName, pRadioHWInfo->szDriver, str_format_frequency(pRadioHWInfo->uCurrentFrequencyKhz), g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1 );
   }

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
      
   _broadcast_router_ready();

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( pRadioHWInfo->uExtraFlags & RADIO_HW_EXTRA_FLAG_FIRMWARE_OLD )
         send_alarm_to_controller(ALARM_ID_FIRMWARE_OLD, i, 0, 5);
   }

   if ( g_pCurrentModel->processesPriorities.iThreadPriorityRouter > 0 )
      g_iDefaultRouterThreadPriority = hw_increase_current_thread_priority("Main thread", g_pCurrentModel->processesPriorities.iThreadPriorityRouter);
   else
      g_iDefaultRouterThreadPriority = hw_get_current_thread_priority("Main thread");

   if ( g_pCurrentModel->hasCamera() )
   {
      // Clear up video pipes on start
      log_line("Clear video pipes...");
      
      if ( g_pCurrentModel->isActiveCameraOpenIPC() )
         video_source_majestic_clear_input_buffers();
      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
         video_source_csi_flush_discard();
      log_line("Done clear video pipes.");
   }
   g_TimeNow = get_current_timestamp_ms();

   // -----------------------------------------------------------
   // Main loop here
   
   int iLoopTimeErrorsCount = 0;
   u32 uLastLoopTime = g_TimeNow;
   g_pProcessStats->uLoopTimer1 = g_pProcessStats->uLoopTimer2 = g_TimeNow;

   while ( !g_bQuit )
   {
      g_TimeNow = get_current_timestamp_ms();
      g_pProcessStats->lastActiveTime = g_TimeNow;
      g_pProcessStats->uLoopSubStep = 0;
      g_pProcessStats->uLoopCounter++;

      if ( g_TimeNow - uLastLoopTime > 5 )
      {
         iLoopTimeErrorsCount++;
         log_softerror_and_alarm("Main loop took too long: %u ms (%u + %u + %u) cnt2: %u, cnt3: %u, cnt4: %u",
            g_TimeNow - uLastLoopTime,
            g_pProcessStats->uLoopTimer1 - uLastLoopTime,
            g_pProcessStats->uLoopTimer2 - g_pProcessStats->uLoopTimer1,
            g_TimeNow - g_pProcessStats->uLoopTimer2,
            g_pProcessStats->uLoopCounter2, g_pProcessStats->uLoopCounter3, g_pProcessStats->uLoopCounter4);
      }
  
      uLastLoopTime = g_TimeNow;

      _main_loop();
      if ( NULL != g_pProcessStats )
         g_pProcessStats->uLoopSubStep = 0xFF;
      if ( g_bQuit )
         break;
   }
   if ( NULL != g_pProcessStats )
      g_pProcessStats->uLoopSubStep = 0xFFFFFFFF;

   // End main loop
   //------------------------------------------------------------

   log_line("Stopping...");

   radio_rx_stop_rx_thread();
   radio_link_cleanup();

   radio_links_close_rxtx_radio_interfaces();

   cleanUp();

   if ( g_pCurrentModel->hasCamera() )
   if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      video_source_csi_stop_program();
 
   delete g_pProcessorTxVideo;
   delete g_pVideoTxBuffers;
   shared_mem_radio_stats_rx_hist_close(g_pSM_HistoryRxStats);
   //shared_mem_video_frames_stats_close(g_pSM_VideoInfoStatsCameraOutput);
   //shared_mem_video_frames_stats_radio_out_close(g_pSM_VideoInfoStatsRadioOut);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_ROUTER_TX, g_pProcessStats);
   log_line("Stopped.Exit now. (PID %d)", getpid());
   log_line("---------------------\n");
   return 0;
}

void _update_main_loop_debug_info()
{
   g_CoutersMainLoop.uCounter++;
   g_CoutersMainLoop.uCounter2++;

   if ( g_CoutersMainLoop.uTime == 0 )
   {
      g_CoutersMainLoop.uTime = g_TimeNow;
      g_CoutersMainLoop.uTime2 = g_TimeNow;
      return;
   }

   if ( g_TimeNow < g_CoutersMainLoop.uTime + 500 )
      return;


   // Compute frames per second values
   u32 dTime = g_TimeNow - g_CoutersMainLoop.uTime;
   g_CoutersMainLoop.uValueNow = (g_CoutersMainLoop.uCounter2 * 1000) / dTime;

   if ( (0 == g_CoutersMainLoop.uValueMinim) || (g_CoutersMainLoop.uValueNow < g_CoutersMainLoop.uValueMinim) )
      g_CoutersMainLoop.uValueMinim = g_CoutersMainLoop.uValueNow;
   if ( (0 == g_CoutersMainLoop.uValueMaxim) || (g_CoutersMainLoop.uValueNow > g_CoutersMainLoop.uValueMaxim) )
      g_CoutersMainLoop.uValueMaxim = g_CoutersMainLoop.uValueNow;
   if ( 0 == g_CoutersMainLoop.uValueAverage )
      g_CoutersMainLoop.uValueAverage = g_CoutersMainLoop.uValueNow;
   else
      g_CoutersMainLoop.uValueAverage = (g_CoutersMainLoop.uValueAverage*9)/10 + g_CoutersMainLoop.uValueNow/10;

   if ( (0 == g_CoutersMainLoop.uValueMinimLocal) || (g_CoutersMainLoop.uValueNow < g_CoutersMainLoop.uValueMinimLocal) )
      g_CoutersMainLoop.uValueMinimLocal = g_CoutersMainLoop.uValueNow;
   if ( (0 == g_CoutersMainLoop.uValueMaximLocal) || (g_CoutersMainLoop.uValueNow > g_CoutersMainLoop.uValueMaximLocal) )
      g_CoutersMainLoop.uValueMaximLocal = g_CoutersMainLoop.uValueNow;
   if ( 0 == g_CoutersMainLoop.uValueAverageLocal )
      g_CoutersMainLoop.uValueAverageLocal = g_CoutersMainLoop.uValueNow;
   else
      g_CoutersMainLoop.uValueAverageLocal = (g_CoutersMainLoop.uValueAverageLocal*9)/10 + g_CoutersMainLoop.uValueNow/10;


   g_CoutersMainLoop.uTime = g_TimeNow;
   g_CoutersMainLoop.uCounter2 = 0;

   if ( g_TimeNow > g_CoutersMainLoop.uTime2 + 20000 )
   {
      g_CoutersMainLoop.uValueAverageLocal = 0;
      g_CoutersMainLoop.uValueMinimLocal = 0;
      g_CoutersMainLoop.uValueMaximLocal = 0;
      g_CoutersMainLoop.uTime2 = g_TimeNow;
   }
}

// returns the count of consumed high priority packets
int _main_loop_try_read_camera()
{
   int iReadSize = 0;
   u8* pVideoData = NULL;
   bool bIsEndOfFrame = false;
   u8* pPacket = NULL;
   int iPacketLength = 0;
   int iPacketIsShort = 0;
   int iRadioInterfaceIndex = 0;

   int iCountConsumedHighPrio = 0;

   g_pProcessStats->uLoopCounter2 = 0;
   g_pProcessStats->uLoopCounter3 = 0;
   g_pProcessStats->uLoopCounter4 = 0;

   // Read multiple times for large I-frames
   int iMaxRepeatCount = 4;

   while ( iMaxRepeatCount > 0 )
   {
      g_pProcessStats->uLoopCounter2++;
      g_pProcessStats->uLoopSubStep = 4;

      iMaxRepeatCount--;
      iReadSize = 0;
      pVideoData = NULL;
      bIsEndOfFrame = false;

      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      {
         g_pProcessStats->uLoopSubStep = 98;
         pVideoData = video_source_csi_read(&iReadSize);
         g_pProcessStats->uLoopSubStep = 5;
         if ( iReadSize > 0 )
         {
            int iBuffSize = video_source_csi_get_buffer_size();
            bIsEndOfFrame = (iReadSize < iBuffSize)?true:false;
            // Concatenate SPS,PSP units to the next I/P unit
            if ( iReadSize < 50 )
               bIsEndOfFrame = false;

            g_pVideoTxBuffers->fillVideoPacketsFromCSI(pVideoData, iReadSize, bIsEndOfFrame);
         }
         g_pProcessStats->uLoopSubStep = 6;
      }
      
      if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      {
         g_pProcessStats->uLoopSubStep = 99;
         pVideoData = video_source_majestic_read(&iReadSize, true);
         g_pProcessStats->uLoopSubStep = 5;
         if ( iReadSize > 0 )
         {
            bool bSingle = video_source_majestic_last_read_is_single_nal();
            bool bEnd = video_source_majestic_last_read_is_end_nal();
            u32 uNALType = video_source_majestic_get_last_nal_type();
            bIsEndOfFrame = g_pVideoTxBuffers->fillVideoPacketsFromRTSPPacket(pVideoData, iReadSize, bSingle, bEnd, uNALType);
            g_pProcessStats->uLoopCounter3++;
         }
         g_pProcessStats->uLoopSubStep = 6;
      }

      g_pProcessStats->uLoopSubStep = 7;

      if ( (iReadSize <= 0) || bIsEndOfFrame )
         iMaxRepeatCount = 0;

      if ( (pVideoData != NULL) && (iReadSize > 0) )
         adaptive_video_on_new_camera_read(bIsEndOfFrame);

      g_pProcessStats->uLoopSubStep = 8;

      // Send telemetry/commands/etc before video data
      if ( g_pVideoTxBuffers->hasPendingPacketsToSend() )
      if ( packets_queue_has_packets(&g_QueueRadioPacketsOut) )
      {
         process_and_send_packets();
         g_pProcessStats->uLoopCounter4++;
      }
      g_pProcessStats->uLoopSubStep = 9;

      // Send audio packets if any
      if ( g_pCurrentModel->audio_params.has_audio_device && g_pCurrentModel->audio_params.enabled )
      if ( NULL != g_pProcessorTxAudio )
         g_pProcessorTxAudio->sendAudioPackets();

      // Intermix video packets and try again to see if we got any new high priority packets
      while ( g_pVideoTxBuffers->hasPendingPacketsToSend() )
      {
         g_pProcessStats->uLoopSubStep = 10;
         int iPending = g_pVideoTxBuffers->hasPendingPacketsToSend();
         g_pVideoTxBuffers->sendAvailablePackets(10);
         g_pProcessStats->uLoopCounter4++;
         int iCount2 = 0;
         while ( (iCount2 < 3) && (!g_bQuit) )
         {
            g_pProcessStats->uLoopSubStep = 11;
            pPacket = radio_rx_wait_get_next_received_high_prio_packet(0, &iPacketLength, &iPacketIsShort, &iRadioInterfaceIndex);
            if ( (NULL == pPacket) || g_bQuit )
               break;

            g_pProcessStats->uLoopSubStep = 12;

            iCount2++;
            iCountConsumedHighPrio++;

            process_received_single_radio_packet(iRadioInterfaceIndex, pPacket, iPacketLength);      
            shared_mem_radio_stats_rx_hist_update(&g_SM_HistoryRxStats, iRadioInterfaceIndex, pPacket, g_TimeNow);

            g_pProcessStats->uLoopSubStep = 13;
         }
      }

      g_pProcessStats->uLoopSubStep = 14;
      if ( g_bDeveloperMode )
         _check_compute_send_rt_debug_info();

      g_pProcessStats->uLoopSubStep = 15;
   }

   g_pProcessStats->uLoopTimer2 = get_current_timestamp_ms();

   return iCountConsumedHighPrio;
}

extern u8 s_uLastRadioPingId;
extern u32 s_uLastRadioPingSentTime;

void _main_loop()
{
   // This loop executes at least 1000 times/sec
   // Processing riorities (highest to lowest):
   // 1. Retransmissions requests and pings and other high priority radio messages
   // 2. Read input video/camera streams
   // 3. Send video to radio
   // 4. Process other radio-in or IPC messages
   // 5. Periodic loops (at least 20Hz)
   // 6. Other minor tasks
 
   g_pProcessStats->uLoopSubStep = 1;

   _update_main_loop_debug_info();
   
   g_pProcessStats->uLoopSubStep = 2;
  
   //---------------------------------------------
   // Check and process retransmissions received and pings received and other high priority radio messages
   int iCountConsumedHighPrio = 0;
   
   int iPacketLength = 0;
   int iPacketIsShort = 0;
   int iRadioInterfaceIndex = 0;
   u8* pPacket = NULL;

   while ( (iCountConsumedHighPrio < 10) && (!g_bQuit) )
   {
      pPacket = radio_rx_wait_get_next_received_high_prio_packet(0, &iPacketLength, &iPacketIsShort, &iRadioInterfaceIndex);
      if ( (NULL == pPacket) || g_bQuit )
         break;

      iCountConsumedHighPrio++;

      process_received_single_radio_packet(iRadioInterfaceIndex, pPacket, iPacketLength);      
      shared_mem_radio_stats_rx_hist_update(&g_SM_HistoryRxStats, iRadioInterfaceIndex, pPacket, g_TimeNow);
   }

   g_pProcessStats->uLoopSubStep = 3;

   g_pProcessStats->uLoopTimer1 = get_current_timestamp_ms();
   
   //--------------------------------------------
   // Video/camera read
   if ( g_pCurrentModel->hasCamera() )
      iCountConsumedHighPrio += _main_loop_try_read_camera();

   //------------------------------------------
   // Process all the other radio-in packets
   
   u32 uTimeStart = get_current_timestamp_ms();
   g_pProcessStats->uLoopSubStep = 20;

   int iCountConsumedRegPrio = 0;
   while ( (iCountConsumedRegPrio < 50) && (!g_bQuit) )
   {
      g_pProcessStats->uLoopSubStep = 21;
      pPacket = radio_rx_wait_get_next_received_reg_prio_packet(200, &iPacketLength, &iPacketIsShort, &iRadioInterfaceIndex);
      if ( (NULL == pPacket) || g_bQuit )
         break;

      g_pProcessStats->uLoopSubStep = 22;

      iCountConsumedRegPrio++;

      shared_mem_radio_stats_rx_hist_update(&g_SM_HistoryRxStats, iRadioInterfaceIndex, pPacket, g_TimeNow);
      process_received_single_radio_packet(iRadioInterfaceIndex, pPacket, iPacketLength);
   
      g_pProcessStats->uLoopSubStep = 23;

      u32 uTime = get_current_timestamp_ms();
      if ( uTime > uTimeStart + 500 )
      {
         log_softerror_and_alarm("Consuming radio rx packets takes too long (%u ms), read ipc messages.", uTime - uTimeStart);
         uTimeStart = uTime;
         _read_ipc_pipes(uTime);
      }
      if ( (0 != s_uTimeLastTryReadIPCMessages) && (uTime > s_uTimeLastTryReadIPCMessages + 500) )
      {
         log_softerror_and_alarm("Too much time since last ipc messages read (%u ms) while consuming radio messages, read ipc messages.", uTime - s_uTimeLastTryReadIPCMessages);
         uTimeStart = uTime;
         _read_ipc_pipes(uTime);
      }
      g_pProcessStats->uLoopSubStep = 24;
   }

   g_pProcessStats->uLoopSubStep = 25;

   // Check Radio Rx state
   if ( (0 == iCountConsumedHighPrio) && (0 == iCountConsumedRegPrio) )
   if ( (NULL != g_pProcessStats) && (0 != g_pProcessStats->lastRadioRxTime) && (g_TimeNow > TIMEOUT_LINK_TO_CONTROLLER_LOST) && (g_pProcessStats->lastRadioRxTime + TIMEOUT_LINK_TO_CONTROLLER_LOST < g_TimeNow) )
   {
      if ( g_TimeLastReceivedRadioPacketFromController + TIMEOUT_LINK_TO_CONTROLLER_LOST < g_TimeNow )
      if ( g_bHasLinkToController )
      {
         g_bHasLinkToController = false;
         if ( g_pCurrentModel->osd_params.osd_preferences[g_pCurrentModel->osd_params.iCurrentOSDLayout] & OSD_PREFERENCES_BIT_FLAG_SHOW_CONTROLLER_LINK_LOST_ALARM )
            send_alarm_to_controller(ALARM_ID_LINK_TO_CONTROLLER_LOST, 0, 0, 5);
      }

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId >= 0 )
      if ( g_pCurrentModel->relay_params.uRelayedVehicleId != 0 )
      if ( (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_REMOTE) ||
           (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_MAIN) ||
           (g_pCurrentModel->relay_params.uCurrentRelayMode & RELAY_MODE_PIP_REMOTE) )
      {
         u32 uOldRelayMode = g_pCurrentModel->relay_params.uCurrentRelayMode;
         g_pCurrentModel->relay_params.uCurrentRelayMode = RELAY_MODE_MAIN | RELAY_MODE_IS_RELAY_NODE;
         saveCurrentModel();
         onEventRelayModeChanged(uOldRelayMode, g_pCurrentModel->relay_params.uCurrentRelayMode, "stop");
      }
   }

   g_pProcessStats->uLoopSubStep = 26;

   adaptive_video_periodic_loop();
   
   g_pProcessStats->uLoopSubStep = 27;

   //-------------------------------------------
   // Process IPCs

   static u32 s_uMainLoopIPCCheckLastTime = 0;
   if ( g_TimeNow >= s_uMainLoopIPCCheckLastTime + 10 )
   {
      s_uMainLoopIPCCheckLastTime = g_TimeNow;

      _read_ipc_pipes(g_TimeNow);
      g_pProcessStats->uLoopSubStep = 28;

      _consume_ipc_messages();
      g_pProcessStats->uLoopSubStep = 29;
   }

   // Send radio packets right away if:
   // * there is no camera (video feed) or it is configuring now
   // * too much time since last tx
   if ( packets_queue_has_packets(&g_QueueRadioPacketsOut) )
   {
      bool bNoVideoData = false;
      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
      {
         if ( ! video_source_csi_read_any_data() )
             bNoVideoData = true;
      }
      else if ( video_source_majestic_is_restarting() )
         bNoVideoData = true;

      bool bTxWaiting = false;
      if ( g_SM_RadioStats.timeLastTxPacket < g_TimeNow - 100 )
         bTxWaiting = true;

      if ( (! g_pCurrentModel->hasCamera()) || bNoVideoData || bTxWaiting )
            process_and_send_packets();

      g_pProcessStats->uLoopSubStep = 30;
   }

   g_pProcessStats->uLoopSubStep = 40;

   //------------------------------------------
   // Periodic loops

   if ( g_bDeveloperMode )
      _check_compute_send_rt_debug_info();

   g_pProcessStats->uLoopSubStep = 41;
   static u32 s_uMainLoopPeriodicCheckLastTime = 0;
   if ( g_TimeNow < s_uMainLoopPeriodicCheckLastTime + 10 )
      return;

   s_uMainLoopPeriodicCheckLastTime = g_TimeNow;

   process_data_tx_video_loop();
   g_pProcessStats->uLoopSubStep = 45;

   if ( g_pCurrentModel->hasCamera() )
   {
      if ( g_pCurrentModel->isActiveCameraCSICompatible() || g_pCurrentModel->isActiveCameraVeye() )
         video_source_csi_periodic_checks();
      if ( g_pCurrentModel->isActiveCameraOpenIPC() )
      {
         if ( video_source_majestic_periodic_checks() )
         {
            log_line("Router is marked for restart. Quit it.");
            if ( packets_queue_has_packets(&g_QueueRadioPacketsOut) )
               process_and_send_packets();
            log_line("Done sending pending radio packets. Quit now.");
            g_bQuit = true;
            return;
         }
      }
   }

   g_pProcessStats->uLoopSubStep = 46;
   _check_rx_loop_consistency();
   g_pProcessStats->uLoopSubStep = 47;
   
   if ( periodicLoop() )
   {
      reinit_radio_interfaces();
      return;
   }
   g_pProcessStats->uLoopSubStep = 48;

   //To fix video_stats_overwrites_periodic_loop();
   //To fix video_link_auto_keyframe_periodic_loop();

   _synchronize_shared_mems();
   g_pProcessStats->uLoopSubStep = 49;
   send_pending_alarms_to_controller();
   g_pProcessStats->uLoopSubStep = 50;

   if ( NULL != g_pProcessorTxAudio )
      g_pProcessorTxAudio->tryReadAudioInputStream();
   g_pProcessStats->uLoopSubStep = 51;
   //----------------------------------------------
   // Other stuff

   //if ( packets_queue_has_packets(&g_QueueRadioPacketsOut) )
   //   process_and_send_packets();
}
