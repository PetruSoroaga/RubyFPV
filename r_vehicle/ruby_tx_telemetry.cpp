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

#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pcap.h>
#include <pthread.h>
#include <stdint.h>
#include "../radio/radiolink.h"
#include <inttypes.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <termios.h>
#include <unistd.h>
#include <math.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/hw_procs.h"
#include "../base/models.h"
#include "../base/commands.h"
#include "../base/launchers.h"
#include "../base/utils.h"
#include "../base/ruby_ipc.h"
#include "../common/string_utils.h"
#include "../mavlink/common/mavlink.h"
#include "parse_fc_telemetry.h"
#include "launchers_vehicle.h"
#include "shared_vars.h"
#include "timers.h"

bool s_bRouterReady = false;

int s_fIPCToRouter = -1;
int s_fIPCFromRouter = -1;

u8 s_BufferTelemetryUplink[MAX_PACKET_TOTAL_SIZE];
u8 s_PipeTmpBuffer[MAX_PACKET_TOTAL_SIZE];
int s_PipeTmpBufferPos = 0;  

int s_iSerialDataLinkHandle = -1;
int s_fSerialToFC = -1;
bool bInputFromSTDIN = false;
bool s_bRetrySetupTelemetry = true;

int s_iCurrentTelemetrySerialPortIndex = -1;
u32 s_uCurrentTelemetrySerialPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;

int s_iCurrentDataLinkSerialPortIndex = -1;
u32 s_uCurrentDataLinkSerialPortSpeed = DEFAULT_FC_TELEMETRY_SERIAL_SPEED;

u8 serialBufferIn[300];
u8 serialBufferOut[300];

u8  telemetryBufferFromFC[RAW_TELEMETRY_MAX_BUFFER];
int telemetryBufferFromFCMaxSize = RAW_TELEMETRY_MIN_SEND_LENGTH;
int telemetryBufferFromFCCount = 0;
u32 telemetryBufferFromFCLastSendTime = 0;

u8  dataLinkSerialBuffer[RAW_TELEMETRY_MAX_BUFFER];
int dataLinkSerialBufferMaxSize = AUXILIARY_DATA_LINK_MIN_SEND_LENGTH;
int dataLinkSerialBufferCount = 0;
u32 dataLinkSerialBufferLastSendTime = 0;

u32 s_uRawTelemetryDownloadTotalReadFromSerial = 0;
u32 s_uRawTelemetryDownloadTotalSend = 0;
u32 s_uRawTelemetryDownloadSegmentIndex = 0;
u32 s_uRawTelemetryLastReceivedUploadedSegmentIndex = MAX_U32;

u32 s_uDataLinkDownlinkSegmentIndex = 0;
u32 s_uDataLinkLastReceivedUploadedSegmentIndex = MAX_U32;

t_packet_header sPH;
t_packet_header_ruby_telemetry_extended sPHRTE;
t_packet_header_fc_telemetry sPHFCT;
t_packet_header_fc_extra sPHFCE;


u32 s_SendIntervalMiliSec_FCTelemetry = 0;
u32 s_SendIntervalMiliSec_RubyTelemetry = 0;
u32 s_LastSendFCTelemetryTime = 0;
u32 s_LastSendRubyTelemetryTime = 0;
u32 s_SentTelemetryCounter = 0;

bool s_bLogNextMAVLinkMessage = true;

#define MAX_MESSAGES_HISTORY 3
u8 s_szLastFCMessages[MAX_MESSAGES_HISTORY][1024];
u32 s_LastReceivedTimeForFCMessages[MAX_MESSAGES_HISTORY];

u32 s_uLastTotalPacketsReceived = 0;
int iFCSerialReadBytes = 0;
int iFCSerialReadBytesPerSecond = 0;

t_packet_header_rc_info_downstream* s_pPHDownstreamInfoRC = NULL; // Info to send back to ground

shared_mem_video_info_stats* s_pSM_VideoInfoStats = NULL;
shared_mem_video_info_stats* s_pSM_VideoInfoStatsRadioOut = NULL;

long int s_lLastPosLat = 0;
long int s_lLastPosLon = 0;
long int home_lat = 0;
long int home_lon = 0;
bool home_set = false;

bool s_bMAVLinkSetupSent = false;
bool s_bSendRCInfoBack = false;
bool s_bSendFullMAVLinkBackToController = false;

u32 s_CountMessagesFromFCPerSecond = 0;
u32 s_CountMessagesFromFCPerSecondTemp = 0;
u32 s_TimeLastMessagesFromFCPerSecondCalculation = 0;


bool s_bFrequencySwitch1IsUp = false;
bool s_bFrequencySwitch1IsDown = false;
bool s_bFrequencySwitch1ReceivedFirstValue = false;

static bool s_bOnArmEventHandled = false;

static u32 s_uTimeLastCheckForRadioReinit = 0;
static bool s_bRadioInterfacesReinitIsInProgress = false;

void open_datalink_serial_port();
void open_telemetry_serial_port();

void _broadcast_vehicle_stats()
{
   static u32 s_u32LastTimeBroadcastVehicleStats = 0;

   if ( g_TimeNow < s_u32LastTimeBroadcastVehicleStats + 3000 )
      return;

   s_u32LastTimeBroadcastVehicleStats = g_TimeNow;

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type =  PACKET_TYPE_LOCAL_CONTROL_BROADCAST_VEHICLE_STATS;
   PH.vehicle_id_src = PACKET_COMPONENT_TELEMETRY;
   PH.vehicle_id_dest = 0;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + sizeof(type_vehicle_stats_info);
   PH.extra_flags = 0;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header),(u8*)&(g_pCurrentModel->m_Stats), sizeof(type_vehicle_stats_info));
   packet_compute_crc(buffer, PH.total_length);

   if ( s_bRouterReady )
      ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);

   if ( NULL != s_pProcessStats )
      s_pProcessStats->lastIPCOutgoingTime = g_TimeNow; 
}

void _add_hardware_telemetry_info( t_packet_header_ruby_telemetry_extended* pPHRTE )
{
   static unsigned long long s_val_cpu[4] = {0,0,0,0};
   static int counter_tx_telemetry_info = 0;
   static u32 s_time_tx_telemetry_cpu = 0;

   counter_tx_telemetry_info++;

   int rate = g_pCurrentModel->telemetry_params.update_rate;
   if ( rate < 2 )
      rate = 2;
   if ( rate >= 4 )
   if ( (counter_tx_telemetry_info % (rate/2)) != 0 )
      return;

   FILE* fd = NULL;
   unsigned long long tmp[4];
   unsigned long long total;
   int temp = 0;

   fd = fopen("/proc/stat", "r");
   fscanf(fd, "%*s %llu %llu %llu %llu", &tmp[0], &tmp[1], &tmp[2], &tmp[3] );
   fclose(fd);

   if ( tmp[0] < s_val_cpu[0] || tmp[1] < s_val_cpu[1] || tmp[2] < s_val_cpu[2] || tmp[3] < s_val_cpu[3] )
      pPHRTE->cpu_load = 0;
   else
   {
      total = (tmp[0] - s_val_cpu[0]) + (tmp[1] - s_val_cpu[1]) + (tmp[2] - s_val_cpu[2]);
      pPHRTE->cpu_load = (total * 100) / (total + (tmp[3] - s_val_cpu[3]));
   }
   s_val_cpu[0] = tmp[0]; s_val_cpu[1] = tmp[1]; s_val_cpu[2] = tmp[2]; s_val_cpu[3] = tmp[3]; 

   if ( g_TimeNow < s_time_tx_telemetry_cpu + 5000 )
      return;

   s_time_tx_telemetry_cpu = g_TimeNow;

   fd = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
   fscanf(fd, "%d", &temp);
   fclose(fd);
   pPHRTE->temperature = temp/1000;

   pPHRTE->throttled = hardware_get_flags();
   pPHRTE->cpu_mhz = (u16) hardware_get_cpu_speed();
}

void _populate_ruby_telemetry_data(t_packet_header_ruby_telemetry_extended* pPHRTE)
{
   u32 vMaj = SYSTEM_SW_VERSION_MAJOR;
   u32 vMin = SYSTEM_SW_VERSION_MINOR/10;
   if ( vMaj > 15 )
      vMaj = 15;
   if ( vMin > 15 )
      vMin = 15;

   pPHRTE->version = ((vMaj<<4) | vMin);

   pPHRTE->downlink_tx_video_bitrate = 0;
   pPHRTE->downlink_tx_video_all_bitrate = 0;
   pPHRTE->downlink_tx_data_bitrate = 0;

   _add_hardware_telemetry_info(pPHRTE);
   g_pCurrentModel->populateVehicleTelemetryData(pPHRTE);

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      pPHRTE->uplink_rssi_dbm[i] = 0;
      pPHRTE->uplink_link_quality[i] = 0;
   }
   if ( NULL != s_pPHDownstreamInfoRC )
      pPHRTE->uplink_rc_rssi = s_pPHDownstreamInfoRC->rc_rssi;
}

void _store_reboot_info_cache()
{
   FILE* fd = fopen(FILE_VEHICLE_REBOOT_CACHE, "w");
   if ( NULL == fd )
      return;

   fprintf(fd, "%u ", sPHFCT.flags);
   fprintf(fd, "%u ", sPHFCT.flight_mode);
   fprintf(fd, "%u ", sPHFCT.arm_time);
   fprintf(fd, "%u ", sPHFCT.distance);
   fprintf(fd, "%u ", sPHFCT.total_distance);
   fprintf(fd, "%u ", sPHFCT.gps_fix_type);
   fprintf(fd, "%u \n", sPHFCT.hdop);
   fprintf(fd, "%d ", sPHFCT.latitude);
   fprintf(fd, "%d ", sPHFCT.longitude);
   fprintf(fd, "%d ", home_set);
   fprintf(fd, "%li ", home_lat);
   fprintf(fd, "%li \n", home_lon);
   fclose(fd);

   log_line("Stored cached info before reboot: distance: %u, total_distance: %u, flags: %d, flight_mode: %d, arm_time: %u, home is set: %d, lat,lon: %li , %li",
      sPHFCT.distance, sPHFCT.total_distance, sPHFCT.flags, sPHFCT.flight_mode, sPHFCT.arm_time, home_set, home_lat, home_lon);
}

void _process_cached_reboot_info()
{
   if ( access( FILE_VEHICLE_REBOOT_CACHE, R_OK ) == -1 )
   {
      log_line("No stored cached telemetry info.");
      return;
   }

   FILE* fd = fopen(FILE_VEHICLE_REBOOT_CACHE, "r");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to open cached telemetry info file for reading.");
      return;
   }
   int tmp1 = 0;
   fscanf(fd, "%d", &tmp1); sPHFCT.flags = (u8)tmp1;
   fscanf(fd, "%d", &tmp1); sPHFCT.flight_mode = (u8)tmp1;
   fscanf(fd, "%u", &sPHFCT.arm_time);
   fscanf(fd, "%u", &sPHFCT.distance);
   fscanf(fd, "%u", &sPHFCT.total_distance);
   fscanf(fd, "%d", &tmp1); sPHFCT.gps_fix_type = (u8)tmp1;
   fscanf(fd, "%u", &tmp1); sPHFCT.hdop = (u8)tmp1;
   fscanf(fd, "%d", &sPHFCT.latitude);
   fscanf(fd, "%d", &sPHFCT.longitude);
   fscanf(fd, "%d", &tmp1); home_set = (bool)tmp1;
   fscanf(fd, "%li", &home_lat);
   fscanf(fd, "%li", &home_lon);

   fclose(fd);
   unlink(FILE_VEHICLE_REBOOT_CACHE);

   log_line("Restored cached info after reboot: distance: %u, total_distance: %u, flags: %d, flight_mode: %d, arm_time: %u, home is set: %d, lat,lon: %li , %li",
      sPHFCT.distance, sPHFCT.total_distance, sPHFCT.flags, sPHFCT.flight_mode, sPHFCT.arm_time, home_set, home_lat, home_lon);

   _broadcast_vehicle_stats();
}

void _preprocess_fc_telemetry(t_packet_header_fc_telemetry* pPHFCT)
{
   if ( iFCSerialReadBytesPerSecond > 60000 )
      iFCSerialReadBytesPerSecond = 60000;
   pPHFCT->fc_kbps = (u16) (iFCSerialReadBytesPerSecond/125); // from bytes to kbits
   pPHFCT->fc_telemetry_type = g_pCurrentModel->telemetry_params.fc_telemetry_type;

   if ( g_TimeLastMessageFromFC < g_TimeNow-1100 )
      pPHFCT->flags |= FC_TELE_FLAGS_NO_FC_TELEMETRY;
   else
      pPHFCT->flags &= ~FC_TELE_FLAGS_NO_FC_TELEMETRY;

   if ( g_bDebug )
   {
      pPHFCT->flags &= ~FC_TELE_FLAGS_NO_FC_TELEMETRY;
      if ( s_SentTelemetryCounter > 70 )
         pPHFCT->flags |= FC_TELE_FLAGS_ARMED;
      pPHFCT->satelites = 15;
      pPHFCT->hdop = 110;
      pPHFCT->gps_fix_type = GPS_FIX_TYPE_3D_FIX;
      pPHFCT->throttle = 25;
      pPHFCT->altitude = 100025;
      pPHFCT->altitude_abs = 100325;
      pPHFCT->heading = 1;
      pPHFCT->hspeed = 100010; // 1 m/sec
      pPHFCT->flight_mode = FLIGHT_MODE_ALTH | FLIGHT_MODE_ARMED;
      pPHFCT->latitude = 47.136136 * 10000000 + 40000 * sin(s_SentTelemetryCounter/10.0);
      pPHFCT->longitude = 27.5777667 * 10000000 + 30000 * cos(s_SentTelemetryCounter/10.0);
      //log_line("lat: %u, lon: %u", DPFCT.latitude, DPFCT.longitude);
      //log_line("pos %d", s_SentTelemetryCounter);
   }

   if ( pPHFCT->gps_fix_type >= GPS_FIX_TYPE_2D_FIX )
      pPHFCT->flags = pPHFCT->flags | FC_TELE_FLAGS_HAS_GPS_FIX;
   else
      pPHFCT->flags = pPHFCT->flags & (~FC_TELE_FLAGS_HAS_GPS_FIX);

   if ( (!has_received_gps_info()) || (! has_received_flight_mode()) || pPHFCT->satelites < 2 || pPHFCT->hdop > 9900 )
   {
      pPHFCT->flags = pPHFCT->flags & (~FC_TELE_FLAGS_POS_CURRENT);
      pPHFCT->flags = pPHFCT->flags & (~FC_TELE_FLAGS_POS_HOME);
      return;
   }

   s_lLastPosLat = pPHFCT->latitude;
   s_lLastPosLon = pPHFCT->longitude;
   if ( pPHFCT->gps_fix_type >= GPS_FIX_TYPE_2D_FIX )
   {
      if ( (! home_set) || (! (pPHFCT->flight_mode & FLIGHT_MODE_ARMED) ) )
      {
         home_set = true;
         home_lat = pPHFCT->latitude;
         home_lon = pPHFCT->longitude;

         if ( g_bDebug )
         {
            home_lat = 47.136136 * 10000000;
            home_lon = 27.5777667 * 10000000;
         }
      }

      // Update distance from home
      if ( home_set )
      {
         //log_line("lat: %u, lon: %u, hlat: %u, hlon: %u", dpfct.latitude, dpfct.longitude, home_lat, home_lon);
         pPHFCT->distance = 100 * distance_meters_between( home_lat/10000000.0, home_lon/10000000.0, pPHFCT->latitude/10000000.0, pPHFCT->longitude/10000000.0 );
         //log_line("home set, distance: %d", dpfct.distance);
      }
   }

   if ( s_SentTelemetryCounter%10 )
   {
      pPHFCT->flags = pPHFCT->flags | FC_TELE_FLAGS_POS_CURRENT;
      pPHFCT->flags = pPHFCT->flags & (~FC_TELE_FLAGS_POS_HOME);
   }
   else if ( home_set )
   {
      // Send home position every 10 frames
      //log_line("Send home pos lat: %u, lon: %u", home_lat, home_lon);
      pPHFCT->flags = pPHFCT->flags & (~FC_TELE_FLAGS_POS_CURRENT);
      pPHFCT->flags = pPHFCT->flags | FC_TELE_FLAGS_POS_HOME;
      pPHFCT->latitude = home_lat;
      pPHFCT->longitude = home_lon;
   }
}

void send_mavlink_setup()
{
   if ( (s_fSerialToFC == -1) || bInputFromSTDIN )
      return;

   if ( s_bMAVLinkSetupSent )
      return;

   g_TimeLastMessageFromFC = g_TimeNow - 1200;

   int componentId = MAV_COMP_ID_MISSIONPLANNER;
   //int componentId = MAV_COMP_ID_SYSTEM_CONTROL;
   //int componentId = 255;

   if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_ALLOW_ANY_VEHICLE_SYSID )
      parse_telemetry_allow_any_sysid(1);
   else
      parse_telemetry_allow_any_sysid(0);

   int len = 0;
   mavlink_message_t msgHeartBeat;
   mavlink_message_t msgDataStreams;
   mavlink_message_t msgMsgInterval;
   mavlink_message_t msgHighLatency;

   
   log_line("Initializing MAVLink with flight controller...");
   if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_RXONLY )
   if ( ! (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_REQUEST_DATA_STREAMS) )
   {
      log_line("Telemetry is set as read only with no requests for data streams. Do nothing.");
      return;
   }

   
   mavlink_msg_heartbeat_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgHeartBeat, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0,0,0);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgHeartBeat);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
      log_softerror_and_alarm("Failed to write to serial port to FC");
   
   int rate = g_pCurrentModel->telemetry_params.update_rate;
   if ( rate < 2 ) rate = 2;
   if ( rate > 10 ) rate = 10;
   mavlink_msg_request_data_stream_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgDataStreams, g_pCurrentModel->telemetry_params.vehicle_mavlink_id, MAV_COMP_ID_AUTOPILOT1, MAV_DATA_STREAM_ALL, rate, 1);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgDataStreams);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }
   log_line("Requested all MAVLink data streams");
   

   char szParam[32];
   strcpy(szParam, "RC_OVERRIDE_TIME");

   if ( g_pCurrentModel->rc_params.rc_enabled )
   {      
      float fTimeout = 1.0/g_pCurrentModel->rc_params.rc_frames_per_second;
      fTimeout *= 2.5;
      log_line("Sending to FC an RC_OVERRIDE timeout of %.2f seconds", fTimeout);
      mavlink_message_t msgSetParam;
      mavlink_msg_param_set_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgSetParam,
                               g_pCurrentModel->telemetry_params.vehicle_mavlink_id, MAV_COMP_ID_ALL, szParam, fTimeout, MAV_PARAM_TYPE_REAL32);

      len = mavlink_msg_to_send_buffer(serialBufferOut, &msgSetParam);
      if ( len != write(s_fSerialToFC, serialBufferOut, len) )
      {
         log_softerror_and_alarm("Failed to write to serial port to FC");
         return;
      }
   }

   mavlink_message_t msgReqParam;
   mavlink_msg_param_request_read_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgReqParam,
                               g_pCurrentModel->telemetry_params.vehicle_mavlink_id, MAV_COMP_ID_ALL, szParam, -1);

   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgReqParam);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }
  
   //mavlink_message_t msgReqParamList;
   //mavlink_msg_param_request_list_pack(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg,
   //                            uint8_t target_system, uint8_t target_component) 

/*
   mavlink_msg_message_interval_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgMsgInterval, MAVLINK_MSG_ID_ATTITUDE, 100000);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgMsgInterval);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }

   mavlink_msg_message_interval_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgMsgInterval, MAVLINK_MSG_ID_SCALED_IMU, 100000);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgMsgInterval);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }

   mavlink_msg_message_interval_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgMsgInterval, MAVLINK_MSG_ID_RAW_IMU, 100000);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgMsgInterval);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }
*/
   mavlink_msg_message_interval_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgMsgInterval, MAVLINK_MSG_ID_HIGH_LATENCY, 1000000);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgMsgInterval);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }

   mavlink_msg_message_interval_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgMsgInterval, MAVLINK_MSG_ID_HIGH_LATENCY2, 1000000);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgMsgInterval);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }
/*
   log_line("Requested MAVLink message interval.");
*/

   mavlink_command_long_t high_latency = {0};
   high_latency.target_system = g_pCurrentModel->telemetry_params.vehicle_mavlink_id;
   high_latency.target_component = MAV_COMP_ID_AUTOPILOT1;
   high_latency.command = MAV_CMD_CONTROL_HIGH_LATENCY;
   high_latency.confirmation = 0;
   high_latency.param1 = 1; 					 	
	
   mavlink_msg_command_long_encode(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msgHighLatency, &high_latency);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msgHighLatency);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }
	
   log_line("Requested MAVLink high latency messages.");

/*
   mavlink_command_long_t cmd_mode = {0};
   cmd_mode.target_system = g_pCurrentModel->telemetry_params.vehicle_mavlink_id;
   cmd_mode.target_component = MAV_COMP_ID_AUTOPILOT1;
   cmd_mode.command = MAV_CMD_DO_SET_MODE;
   cmd_mode.confirmation = 0;
   cmd_mode.param1 = MAV_MODE_MANUAL_DISARMED; 					 	
	
   mavlink_message_t msg;
   mavlink_msg_command_long_encode(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msg, &cmd_mode);
   len = mavlink_msg_to_send_buffer(serialBufferOut, &msg);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
   {
      log_softerror_and_alarm("Failed to write to serial port to FC");
      return;
   }
	
   log_line("Requested stab mode.");
*/

   log_line("Initialized MAVLink with flight controller, controller mavid: %d, vehicle mavid: %d, speed: %u bps.", g_pCurrentModel->telemetry_params.controller_mavlink_id, g_pCurrentModel->telemetry_params.vehicle_mavlink_id, s_uCurrentTelemetrySerialPortSpeed);
   s_bMAVLinkSetupSent = true;
}

void _send_rc_data_to_FC()
{
   static u16 s_ch_last_values[18];
   static u8 s_is_failsafe = 0;

   if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_RXONLY )
   if ( ! (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_REQUEST_DATA_STREAMS) )
   {
      log_line("Telemetry is set as read only with no requests for data streams. Do not send data to FC.");
      return;
   }
 
   if ( (s_fSerialToFC == -1) || bInputFromSTDIN )
      return;
   if ( NULL == s_pPHDownstreamInfoRC )
      return;
   if ( g_TimeNow < g_TimeStart + 1000 )
      return;
   if ( s_pPHDownstreamInfoRC->recv_packets < 2 )
      return;

   bool bSend = false;

   if ( s_is_failsafe != s_pPHDownstreamInfoRC->is_failsafe )
      bSend = true;
   if ( g_TimeNow >= g_TimeLastRCSentToFC + 1000/g_pCurrentModel->rc_params.rc_frames_per_second )
      bSend = true;

   if ( ! bSend )
      return;

   int componentId = MAV_COMP_ID_MISSIONPLANNER;
   //int componentId = MAV_COMP_ID_AUTOPILOT1;

   g_TimeLastRCSentToFC = g_TimeNow;
   s_is_failsafe = s_pPHDownstreamInfoRC->is_failsafe;
  
   int count = g_pCurrentModel->rc_params.channelsCount;
   if ( count > 18 )
      count = 18;
   for( int i=0; i<count; i++ )
      s_ch_last_values[i] = s_pPHDownstreamInfoRC->rc_channels[i];

   if ( s_pPHDownstreamInfoRC->is_failsafe )
   {
     for( int i=0; i<count; i++ )
         s_ch_last_values[i] = get_rc_channel_failsafe_value(g_pCurrentModel, i, s_pPHDownstreamInfoRC->rc_channels[i]);
   }

   /*
      char szBuff[256];
      sprintf(szBuff, "fs:%d ", s_pPHDownstreamInfoRC->is_failsafe );
      for( int i=0; i<(int)g_pCurrentModel->rc_params.channelsCount; i++ )
      {
         char szTmp[32];
         sprintf(szTmp, "%d: %d  ", i+1, s_ch_last_values[i]);
         strcat(szBuff, szTmp);
      }
      log_line(szBuff);
   */

   if ( s_is_failsafe && ((g_pCurrentModel->rc_params.failsafeFlags & 0xFF) == RC_FAILSAFE_NOOUTPUT ) )
      return;

   if ( ! s_bMAVLinkSetupSent )
      send_mavlink_setup();

   //log_line("send RC, from %d, component %d, to %d, component %d", g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, 1, MAV_COMP_ID_ALL);

   mavlink_message_t msg;
   int len;
   if ( g_TimeNow > g_TimeLastMAVLinkHeartbeatSent + 700 )
   {
      g_TimeLastMAVLinkHeartbeatSent = g_TimeNow;
      //log_line("Sending MAVLink heartbeat...");

      mavlink_msg_heartbeat_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msg, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0,0,0);
      len = mavlink_msg_to_send_buffer(serialBufferOut, &msg);
      if ( len != write(s_fSerialToFC, serialBufferOut, len) )
         log_softerror_and_alarm("Failed to write to serial port to FC");
   }

   mavlink_msg_rc_channels_override_pack(g_pCurrentModel->telemetry_params.controller_mavlink_id, componentId, &msg, g_pCurrentModel->telemetry_params.vehicle_mavlink_id, MAV_COMP_ID_ALL, s_ch_last_values[0], s_ch_last_values[1], s_ch_last_values[2], s_ch_last_values[3],
         s_ch_last_values[4], s_ch_last_values[5], s_ch_last_values[6], s_ch_last_values[7], s_ch_last_values[8], s_ch_last_values[9], s_ch_last_values[10], s_ch_last_values[11], s_ch_last_values[12],
         s_ch_last_values[13], s_ch_last_values[14], s_ch_last_values[15], s_ch_last_values[16], s_ch_last_values[17]);

   len = mavlink_msg_to_send_buffer(serialBufferOut, &msg);
   if ( len != write(s_fSerialToFC, serialBufferOut, len) )
      log_softerror_and_alarm("Failed to write to serial port to FC");
}


void save_model()
{
   log_line("Saving model...");

   if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, false) )
      g_pCurrentModel->resetToDefaults(true);

   g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, false);

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED;
   PH.vehicle_id_src = PACKET_COMPONENT_TELEMETRY | (MODEL_CHANGED_STATS<<8);
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header);

   packet_compute_crc((u8*)&PH, PH.total_length);
   if ( s_bRouterReady )
      ruby_ipc_channel_send_message(s_fIPCToRouter, (u8*)&PH, PH.total_length);

   if ( NULL != s_pProcessStats )
      s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   if ( NULL != s_pProcessStats )
      s_pProcessStats->lastActiveTime = get_current_timestamp_ms(); 
}

void reload_model(u8 changeType)
{
   log_line("Reloading model");

   hardware_reload_serial_ports();

   s_bRetrySetupTelemetry = true;

   u8  last_telemetry_type = g_pCurrentModel->telemetry_params.fc_telemetry_type;
   int last_telemetry_serial_port_index = s_iCurrentTelemetrySerialPortIndex;
   u32 last_telemetry_serial_port_speed = s_uCurrentTelemetrySerialPortSpeed;
   int last_datalink_serial_port_index = s_iCurrentDataLinkSerialPortIndex;
   u32 last_datalink_serial_port_speed = s_uCurrentDataLinkSerialPortSpeed;

   if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, false) )
      g_pCurrentModel->resetToDefaults(true);

   if ( changeType != MODEL_CHANGED_GENERIC )
   if ( changeType != MODEL_CHANGED_CONTROLLER_TELEMETRY ) 
   {
      log_line("Model change does not affect TX telemetry. Done updating local model.");
      return;
   }

   s_SendIntervalMiliSec_FCTelemetry = 1000/g_pCurrentModel->telemetry_params.update_rate;
   s_SendIntervalMiliSec_RubyTelemetry = 1000/DEFAULT_RUBY_TELEMETRY_UPDATE_RATE;
   if ( g_pCurrentModel->telemetry_params.update_rate > DEFAULT_RUBY_TELEMETRY_UPDATE_RATE )
   {
      if ( g_pCurrentModel->telemetry_params.update_rate < 2*DEFAULT_RUBY_TELEMETRY_UPDATE_RATE )
         s_SendIntervalMiliSec_RubyTelemetry = 1000/g_pCurrentModel->telemetry_params.update_rate;
      else
         s_SendIntervalMiliSec_RubyTelemetry = 2*1000/g_pCurrentModel->telemetry_params.update_rate;
   }
   else
   {
      if ( g_pCurrentModel->telemetry_params.update_rate >= DEFAULT_RUBY_TELEMETRY_UPDATE_RATE/2 )
         s_SendIntervalMiliSec_FCTelemetry = 1000/DEFAULT_RUBY_TELEMETRY_UPDATE_RATE;
      else
         s_SendIntervalMiliSec_FCTelemetry = 2*1000/DEFAULT_RUBY_TELEMETRY_UPDATE_RATE;
   }


   log_line("Using FC telemetry rate of %d ms, Ruby telemetry rate of %d ms", s_SendIntervalMiliSec_FCTelemetry, s_SendIntervalMiliSec_RubyTelemetry);

   s_bSendRCInfoBack = false;
   if ( g_pCurrentModel->osd_params.show_stats_rc ||
       (g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_HID_IN_OSD) ||
       (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_STATS_RC)
      )
      s_bSendRCInfoBack = true;
   log_line("Sending RC info back to controller: %s", s_bSendRCInfoBack?"yes":"no");


   s_iCurrentTelemetrySerialPortIndex = -1;
   s_iCurrentDataLinkSerialPortIndex = -1;
   
   for( int i=0; i<hardware_get_serial_ports_count(); i++ )
   {
       hw_serial_port_info_t* pPortInfo = hardware_get_serial_port_info(i);
       if ( NULL == pPortInfo )
          continue;
       if ( ! pPortInfo->iSupported )
          continue;
       if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY )
       {
          s_iCurrentTelemetrySerialPortIndex = i;
          s_uCurrentTelemetrySerialPortSpeed = pPortInfo->lPortSpeed;
       }
       if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_DATA_LINK )
       {
          s_iCurrentDataLinkSerialPortIndex = i;
          s_uCurrentDataLinkSerialPortSpeed = pPortInfo->lPortSpeed;
       }
   }

   log_line("New assigned serial port index for telemetry: %d, at baudrate: %u", s_iCurrentTelemetrySerialPortIndex, s_uCurrentTelemetrySerialPortSpeed);
   log_line("New assigned serial port index for data link: %d, at baudrate: %u", s_iCurrentDataLinkSerialPortIndex, s_uCurrentDataLinkSerialPortSpeed);

   if ( last_telemetry_type != g_pCurrentModel->telemetry_params.fc_telemetry_type ||
        last_telemetry_serial_port_index != s_iCurrentTelemetrySerialPortIndex ||
        last_telemetry_serial_port_speed != s_uCurrentTelemetrySerialPortSpeed )
   {
      open_telemetry_serial_port();
   }

   s_bMAVLinkSetupSent = false;
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != MODEL_TELEMETRY_TYPE_NONE )
      send_mavlink_setup();

   if ( last_datalink_serial_port_index != s_iCurrentDataLinkSerialPortIndex ||
        last_datalink_serial_port_speed != s_uCurrentDataLinkSerialPortSpeed )
   {
      open_datalink_serial_port();
   }
   s_bSendFullMAVLinkBackToController = (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SEND_FULL_PACKETS_TO_CONTROLLER)?true:false;
   if ( s_bSendFullMAVLinkBackToController )
      log_line("Flag to send back full mavlink/tml packet to controller is set.");
   else
      log_line("Flag to send back full mavlink/tml packet to controller is not set.");
}

void onRebootRequest()
{
   log_line("Executing request to reboot.");

   _store_reboot_info_cache();
   save_model();

   vehicle_stop_tx_router();
   if ( g_pCurrentModel->hasCamera() ) 
      vehicle_stop_video_capture(g_pCurrentModel);
   hw_stop_process("ruby_rx_commands");
   hw_stop_process("ruby_rx_rc");
   hardware_sleep_ms(100);
   hw_execute_bash_command("sudo reboot -f", NULL);
}


bool try_read_messages_from_router()
{
   if ( NULL == ruby_ipc_try_read_message(s_fIPCFromRouter, 500, s_PipeTmpBuffer, &s_PipeTmpBufferPos, s_BufferTelemetryUplink) )
      return false;

   if ( NULL != s_pProcessStats )
      s_pProcessStats->lastIPCIncomingTime = g_TimeNow;

   t_packet_header* pPH = (t_packet_header*)&s_BufferTelemetryUplink[0];

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
   if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_VEHICLE_ROUTER_READY )
   {
      log_line("Received notification that router is ready.");
      s_bRouterReady = true;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_RUBY )
   if ( pPH->packet_type == PACKET_TYPE_RUBY_PAIRING_REQUEST )
   {
      log_line("Received pairing request from router. CID: %u, VID: %u. Updating local model.", pPH->vehicle_id_src, pPH->vehicle_id_dest);
      if ( NULL != g_pCurrentModel )
         g_pCurrentModel->controller_id = pPH->vehicle_id_src;
   }
 
   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) == PACKET_COMPONENT_LOCAL_CONTROL )
   {
      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED )
      {
         u8 changeType = (pPH->vehicle_id_src >> 8 ) & 0xFF;      
         log_line("Received request from router to reload model (change type: %d).", (int)changeType);
         reload_model(changeType);
         return true;
      }
      if ( pPH->packet_type == PACKET_TYPE_LOCAL_LINK_FREQUENCY_CHANGED )
      {
         int* pI = (int*)((&s_BufferTelemetryUplink[0])+sizeof(t_packet_header));
         int nLink = *pI;
         pI++;
         int newFreq = *pI;
         log_line("Received new model radio link frequency from router (radio link %d new freq: %d Mhz). Updating local model copy.", nLink+1, newFreq);
         if ( nLink >= 0 && nLink < g_pCurrentModel->radioLinksParams.links_count )
         { 
            g_pCurrentModel->radioLinksParams.link_frequency[nLink] = newFreq;
            for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
            {
               if ( g_pCurrentModel->radioInterfacesParams.interface_link_id[i] == nLink )
                  g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i] = newFreq;
            }
         }
         else
            log_softerror_and_alarm("Invalid parameters for changing radio link frequency. radio link: %d of %d", nLink+1, g_pCurrentModel->radioLinksParams.links_count);

         return true;
      }

      if ( pPH->packet_type == PACKET_TYPE_LOCAL_CONTROL_REBOOT )
         onRebootRequest();
      return true;
   }

   if ( (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE) != PACKET_COMPONENT_TELEMETRY )
      return true;

   t_packet_header_telemetry_raw* pPHTR = (t_packet_header_telemetry_raw*)((&s_BufferTelemetryUplink[0]) + sizeof(t_packet_header));
   if ( ! packet_check_crc(s_BufferTelemetryUplink, pPH->total_length) )
      return true;

   if ( pPH->vehicle_id_dest != g_pCurrentModel->vehicle_id )
   {
      log_softerror_and_alarm("Received telemetry targeted to another vehicle (current vehicle UID: %u, received vehicle UID:%u).", g_pCurrentModel->vehicle_id, pPH->vehicle_id_dest);
      return true;
   }

   if (pPH->packet_type == PACKET_TYPE_TELEMETRY_RAW_UPLOAD )
   {
      int len = pPH->total_length - sizeof(t_packet_header) - sizeof(t_packet_header_telemetry_raw);
      u8* pTelemetryData = (&s_BufferTelemetryUplink[0]) + sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw);

      if ( s_uRawTelemetryLastReceivedUploadedSegmentIndex != MAX_U32 &&
           s_uRawTelemetryLastReceivedUploadedSegmentIndex > 10 && 
           pPHTR->telem_segment_index < s_uRawTelemetryLastReceivedUploadedSegmentIndex-10 )
         s_uRawTelemetryLastReceivedUploadedSegmentIndex = MAX_U32;

      if ( s_uRawTelemetryLastReceivedUploadedSegmentIndex != MAX_U32 )
      if ( pPHTR->telem_segment_index <= s_uRawTelemetryLastReceivedUploadedSegmentIndex )
         return true;

      //log_line("Received uploaded raw telemetry %d bytes", len);

      if ( s_uRawTelemetryLastReceivedUploadedSegmentIndex != MAX_U32 )
      if ( s_uRawTelemetryLastReceivedUploadedSegmentIndex + 1 != pPHTR->telem_segment_index )
      {
      //log_line("-------------------------------");
      //log_line("Missing telemetry upload packet");
      //log_line("-------------------------------");
      }

      s_uRawTelemetryLastReceivedUploadedSegmentIndex = pPHTR->telem_segment_index;
      /*
      log_line("Telemetry payload RX Length: %d", len);
      char szBuff[2000];
      memcpy(szBuff, rxBuffer+sizeof(data_packet_header), len);
      szBuff[len] = 0;
      log_line("Recv data: %s", szBuff);
      */

      if ( len != write(s_fSerialToFC, pTelemetryData, len) )
         log_softerror_and_alarm("Failed to write to serial port for telemetry output to FC.");
   }

   if (pPH->packet_type == PACKET_TYPE_AUX_DATA_LINK_UPLOAD )
   {
      int len = pPH->total_length - sizeof(t_packet_header) - sizeof(u32);
      u8* pDataLinkData = (&s_BufferTelemetryUplink[0]) + sizeof(t_packet_header);
      u32 segment = 0;
      memcpy((u8*)&segment, pDataLinkData, sizeof(u32));
      pDataLinkData += sizeof(u32);
      if ( s_uDataLinkLastReceivedUploadedSegmentIndex != MAX_U32 &&
           s_uDataLinkLastReceivedUploadedSegmentIndex > 10 && 
           segment < s_uDataLinkLastReceivedUploadedSegmentIndex-10 )
         s_uDataLinkLastReceivedUploadedSegmentIndex = MAX_U32;

      if ( s_uDataLinkLastReceivedUploadedSegmentIndex != MAX_U32 )
      if ( segment <= s_uDataLinkLastReceivedUploadedSegmentIndex )
         return true;

      //log_line("Received uploaded raw telemetry: segment index (last/now): %u/%u, len: %d", s_uDataLinkLastReceivedUploadedSegmentIndex, segment,  len);

      if ( s_uDataLinkLastReceivedUploadedSegmentIndex != MAX_U32 )
      if ( s_uDataLinkLastReceivedUploadedSegmentIndex + 1 != segment )
      {
      //log_line("-------------------------------");
      //log_line("Missing telemetry upload packet");
      //log_line("-------------------------------");
      }

      s_uDataLinkLastReceivedUploadedSegmentIndex = segment;
      if ( -1 != s_iSerialDataLinkHandle )
      if ( len != write(s_iSerialDataLinkHandle, pDataLinkData, len) )
         log_softerror_and_alarm("Failed to write to serial port for auxiliary data link output.");
   }
   return true;
}

void send_raw_telemetry_packet_to_controller()
{
   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == MODEL_TELEMETRY_TYPE_NONE )
      return;
   if ( telemetryBufferFromFCCount <= 0 )
      return;

   if ( telemetryBufferFromFCCount > RAW_TELEMETRY_MAX_BUFFER )
   {
      log_softerror_and_alarm("Trying to send more raw telemetry data than expected to downlink: %d bytes, max expected: %d bytes. Sending only max allowed.", telemetryBufferFromFCCount, RAW_TELEMETRY_MAX_BUFFER);
      telemetryBufferFromFCCount = RAW_TELEMETRY_MAX_BUFFER;
   }

   s_uRawTelemetryDownloadTotalSend += telemetryBufferFromFCCount;

   t_packet_header PH;
   t_packet_header_telemetry_raw PHTR;

   PH.packet_flags = PACKET_COMPONENT_TELEMETRY;
   PH.packet_type =  PACKET_TYPE_TELEMETRY_RAW_DOWNLOAD;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_headers_length = sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw);
   PH.total_length = sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw) + telemetryBufferFromFCCount;
   PH.extra_flags = 0;
      
   PHTR.telem_segment_index = s_uRawTelemetryDownloadSegmentIndex;
   PHTR.telem_total_data = s_uRawTelemetryDownloadTotalSend;
   PHTR.telem_total_serial = s_uRawTelemetryDownloadTotalReadFromSerial;

   //log_line("Sending raw telemetry to router, segment index: %u, total serial: %u, total data: %u,  data length: %d, total packet length: %d", PHTR.telem_segment_index, PHTR.telem_total_serial, PHTR.telem_total_data, telemetryBufferFromFCCount, PH.total_length);

   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&PHTR, sizeof(t_packet_header_telemetry_raw));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_telemetry_raw), telemetryBufferFromFC, telemetryBufferFromFCCount);
   packet_compute_crc(buffer, PH.total_length);

   if ( s_bRouterReady && (! s_bRadioInterfacesReinitIsInProgress) )
   {
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
      if ( result != PH.total_length )
         log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
   }

   telemetryBufferFromFCLastSendTime = g_TimeNow;
   telemetryBufferFromFCCount = 0;
   s_uRawTelemetryDownloadSegmentIndex++;

   if ( NULL != s_pProcessStats )
      s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}

void send_datalink_data_packet_to_controller()
{
   if ( dataLinkSerialBufferCount > RAW_TELEMETRY_MAX_BUFFER )
   {
      log_softerror_and_alarm("Trying to send more data link data than expected to downlink: %d bytes, max expected: %d bytes. Sending only max allowed.", dataLinkSerialBufferCount, RAW_TELEMETRY_MAX_BUFFER);
      dataLinkSerialBufferCount = RAW_TELEMETRY_MAX_BUFFER;
   }

   t_packet_header PH;

   PH.packet_flags = PACKET_COMPONENT_TELEMETRY;
   PH.packet_type =  PACKET_TYPE_AUX_DATA_LINK_DOWNLOAD;
   PH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_headers_length = sizeof(t_packet_header)+sizeof(u32);
   PH.total_length = sizeof(t_packet_header)+sizeof(u32) + dataLinkSerialBufferCount;
   PH.extra_flags = 0;
      
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)&s_uDataLinkDownlinkSegmentIndex, sizeof(u32));
   memcpy(buffer+sizeof(t_packet_header)+sizeof(u32), dataLinkSerialBuffer, dataLinkSerialBufferCount);
   packet_compute_crc(buffer, PH.total_length);

   if ( s_bRouterReady && (! s_bRadioInterfacesReinitIsInProgress) )
   {
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);
      if ( result != PH.total_length )
         log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
   }

   dataLinkSerialBufferLastSendTime = g_TimeNow;
   dataLinkSerialBufferCount = 0;
   s_uDataLinkDownlinkSegmentIndex++;

   if ( NULL != s_pProcessStats )
      s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}

void addSerialDataToFCTelemetryBuffer(u8* pData, int dataLength)
{
   bool bMustSendFullTelemetryPackets = false;

   if ( s_bSendFullMAVLinkBackToController )
      bMustSendFullTelemetryPackets = true;

   if ( g_pCurrentModel->telemetry_params.bControllerHasInputTelemetry || g_pCurrentModel->telemetry_params.bControllerHasOutputTelemetry )
      bMustSendFullTelemetryPackets = true;

   if ( ! bMustSendFullTelemetryPackets )
      return;
   /*
   log_line("adding %d bytes to sent to controller as telemetry.", dataLength);

   char szBuff[2000];
   memcpy(szBuff, pData, dataLength);
   szBuff[dataLength] = 0;
   log_line("Seding data: %s", szBuff);
   if ( dataLength != write(s_fSerialToFC, pData, dataLength) )
      log_softerror_and_alarm("Failed to write to serial port for telemetry output to FC.");
   else
      log_line("Sent %d bytes serial echo", dataLength);
   */

   while ( dataLength > 0 )
   {
      if ( telemetryBufferFromFCCount + dataLength < telemetryBufferFromFCMaxSize )
      {
         memcpy(&(telemetryBufferFromFC[telemetryBufferFromFCCount]), pData, dataLength);
         telemetryBufferFromFCCount += dataLength;
         return;
      }
      int chunkSize = telemetryBufferFromFCMaxSize-telemetryBufferFromFCCount;
      memcpy(&(telemetryBufferFromFC[telemetryBufferFromFCCount]), pData, chunkSize);
      telemetryBufferFromFCCount += chunkSize;
      pData += chunkSize;
      dataLength -= chunkSize;
      send_raw_telemetry_packet_to_controller();
   }
}

void try_read_serial_telemetry()
{
   if ( -1 == s_fSerialToFC )
      return;

   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = 2000; // 2 ms

   fd_set readset;   
   FD_ZERO(&readset);
   FD_SET(s_fSerialToFC, &readset);
   
   int res = select(s_fSerialToFC+1, &readset, NULL, NULL, &to);
   if ( res <= 0 )
      return;
   if ( ! FD_ISSET(s_fSerialToFC, &readset) )
      return;

   int length = read(s_fSerialToFC, serialBufferIn, 270);
   if ( length <= 0 )
      return;

   //printf("+%d", length);
   //fflush(stdout);
   s_uRawTelemetryDownloadTotalReadFromSerial += length;
   iFCSerialReadBytes += length;

   addSerialDataToFCTelemetryBuffer(serialBufferIn, length);

   //log_line("Received %d bytes from FC", length);

   if ( parse_telemetry_from_fc(serialBufferIn, length, &sPHFCT, &sPHRTE, g_pCurrentModel->vehicle_type, g_pCurrentModel->telemetry_params.fc_telemetry_type) )
   {
      g_TimeLastMessageFromFC = g_TimeNow;
      s_CountMessagesFromFCPerSecondTemp++;
   }
}

void try_read_serial_datalink()
{
   if ( -1 == s_iSerialDataLinkHandle )
      return;

   struct timeval to;
   to.tv_sec = 0;
   to.tv_usec = 2000; // 2 ms

   fd_set readset;   
   FD_ZERO(&readset);
   FD_SET(s_iSerialDataLinkHandle, &readset);
   
   int res = select(s_iSerialDataLinkHandle+1, &readset, NULL, NULL, &to);
   if ( res <= 0 )
      return;
   if ( ! FD_ISSET(s_iSerialDataLinkHandle, &readset) )
      return;

   int length = read(s_iSerialDataLinkHandle, serialBufferIn, 270);
   if ( length <= 0 )
      return;

   char szBuff[2000];
   memcpy(szBuff, &serialBufferIn[0], length);
   szBuff[length] = 0;

   u8* pData = &serialBufferIn[0];

   while ( length > 0 )
   {
      if ( dataLinkSerialBufferCount + length < dataLinkSerialBufferMaxSize )
      {
         memcpy(&(dataLinkSerialBuffer[dataLinkSerialBufferCount]), pData, length);
         dataLinkSerialBufferCount += length;
         return;
      }
      int chunkSize = dataLinkSerialBufferMaxSize-dataLinkSerialBufferCount;
      memcpy(&(dataLinkSerialBuffer[dataLinkSerialBufferCount]), pData, chunkSize);
      dataLinkSerialBufferCount += chunkSize;
      pData += chunkSize;
      length -= chunkSize;
      send_datalink_data_packet_to_controller();
   }
}

void _send_telemetry_to_controller()
{
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   bool bDidSentRubyTelemetry = false;

   s_SentTelemetryCounter++;

   //---------------------------------------
   // Send Ruby Telemetry Extended packet

   if ( (g_TimeNow < s_LastSendRubyTelemetryTime) || (g_TimeNow >= s_LastSendRubyTelemetryTime + s_SendIntervalMiliSec_RubyTelemetry) )
   {
      //log_line("Send Ruby telemetry to controller");
      s_LastSendRubyTelemetryTime = g_TimeNow;
      bDidSentRubyTelemetry = true;
      _populate_ruby_telemetry_data(&sPHRTE);

      sPHRTE.flags &= ~(FLAG_RUBY_TELEMETRY_ENCRYPTED_DATA | FLAG_RUBY_TELEMETRY_ENCRYPTED_VIDEO);
      if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
         sPHRTE.flags |= FLAG_RUBY_TELEMETRY_ENCRYPTED_DATA;
      if ( (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_VIDEO) || (g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL) )
         sPHRTE.flags |= FLAG_RUBY_TELEMETRY_ENCRYPTED_VIDEO;

      if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE )
         sPHRTE.flags |= FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;
      else
         sPHRTE.flags &= ~FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;

      sPHRTE.flags &= ~(FLAG_RUBY_TELEMETRY_RC_FAILSAFE | FLAG_RUBY_TELEMETRY_RC_ALIVE);

      #ifdef FEATURE_ENABLE_RC
      if ( g_pCurrentModel->rc_params.rc_enabled && NULL != s_pPHDownstreamInfoRC )
      {
         if ( s_pPHDownstreamInfoRC->is_failsafe )
            sPHRTE.flags |= FLAG_RUBY_TELEMETRY_RC_FAILSAFE;
         else
            sPHRTE.flags |= FLAG_RUBY_TELEMETRY_RC_ALIVE;
      }
      #endif

      // Gets populated by router
      t_packet_header_ruby_telemetry_extended_extra_info_retransmissions ph_extra_info;
      memset((u8*)&ph_extra_info,0, sizeof(ph_extra_info));
      
      sPHRTE.extraSize = sizeof(ph_extra_info);

      sPH.packet_flags = PACKET_COMPONENT_TELEMETRY;
      sPH.packet_type = PACKET_TYPE_RUBY_TELEMETRY_EXTENDED;
      sPH.total_headers_length = (u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_ruby_telemetry_extended) + sPHRTE.extraSize;
      sPH.total_length = sPH.total_headers_length;
      sPH.extra_flags = 0;

      memcpy(buffer, &sPH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), &sPHRTE, sizeof(t_packet_header_ruby_telemetry_extended));
      // Copy extra info
      memcpy(buffer+sizeof(t_packet_header) + sizeof(t_packet_header_ruby_telemetry_extended), (u8*)&ph_extra_info, sPHRTE.extraSize);
      packet_compute_crc(buffer, sPH.total_length);

      if ( s_bRouterReady && (! s_bRadioInterfacesReinitIsInProgress) )
      {
         int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
         if ( result != sPH.total_length )
            log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
      }

      if ( NULL != s_pProcessStats )
         s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   // ----------------------------------
   // Send FC Telemetry packet

   if ( (g_TimeNow < s_LastSendFCTelemetryTime) || (g_TimeNow >= s_LastSendFCTelemetryTime + s_SendIntervalMiliSec_FCTelemetry) )
   {
      //log_line("Send FC telemetry to controller");
      s_LastSendFCTelemetryTime = g_TimeNow;

      sPH.packet_flags = PACKET_COMPONENT_TELEMETRY;
      sPH.packet_type = PACKET_TYPE_FC_TELEMETRY;
      sPH.extra_flags = 0;

      if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != TELEMETRY_TYPE_NONE )
         _preprocess_fc_telemetry(&sPHFCT);
      else
         sPHFCT.flags = FC_TELE_FLAGS_NO_FC_TELEMETRY;

      sPHFCT.extra_info[5]++;
      sPHFCT.extra_info[6] = (s_CountMessagesFromFCPerSecond>255)?255:s_CountMessagesFromFCPerSecond;

      bool bHasFCMessage = false;
      bool bIsDuplicateFCMessage = false;
      bool bSendFCMessage = false;

      // Do we have  message from FC? Message was reset on the previous check

      if ( (get_last_message_time() > 0) && (0 != get_last_message()[0]) )
         bHasFCMessage = true;

      // Is it a duplicate?

      if ( bHasFCMessage )
      {
         for( int i=0; i<MAX_MESSAGES_HISTORY; i++ )
         {
            if ( s_szLastFCMessages[i][0] != 0 )
            if ( 0 == strcmp((const char*)s_szLastFCMessages[i], get_last_message()) )
            if ( s_LastReceivedTimeForFCMessages[i] > g_TimeNow - 5000 )
            {
               s_LastReceivedTimeForFCMessages[i] = get_last_message_time();
               bIsDuplicateFCMessage = true;
               break;
            }
         }
      }

      // Add the message to the history
      if ( bHasFCMessage && ( ! bIsDuplicateFCMessage ) )
      {
         for( int i=MAX_MESSAGES_HISTORY-1; i>0; i-- )
         {
            strcpy((char*)s_szLastFCMessages[i], (const char*)s_szLastFCMessages[i-1]);
            s_LastReceivedTimeForFCMessages[i] = s_LastReceivedTimeForFCMessages[i-1];
         }
         strcpy((char*)s_szLastFCMessages[0], get_last_message());
         s_LastReceivedTimeForFCMessages[0] = get_last_message_time();
      }

      if ( s_LastReceivedTimeForFCMessages[0] > 0 && 0 != s_szLastFCMessages[0][0] &&
         ( g_TimeNow < s_LastReceivedTimeForFCMessages[0] + FC_MESSAGE_TIMEOUT ) )
      {
         bSendFCMessage = true;
         if ( s_bLogNextMAVLinkMessage )
            log_line("Sending FC message from vehicle to station: [%s]", s_szLastFCMessages[0]);
         s_bLogNextMAVLinkMessage = false;
         sPHFCT.flags = sPHFCT.flags | FC_TELE_FLAGS_HAS_MESSAGE;

         sPHFCE.chunk_index = 0;
         memset(sPHFCE.text, 0, FC_MESSAGE_MAX_LENGTH);
         s_szLastFCMessages[0][FC_MESSAGE_MAX_LENGTH-1] = 0;
         strcpy((char*)(sPHFCE.text), (const char*)s_szLastFCMessages[0]);
      }
      else
      {
         s_bLogNextMAVLinkMessage = true;
         sPHFCT.flags = sPHFCT.flags & (~FC_TELE_FLAGS_HAS_MESSAGE);
      }

      reset_last_message();

      sPHFCT.flags = sPHFCT.flags & (~FC_TELE_FLAGS_RC_FAILSAFE);

      #ifdef FEATURE_ENABLE_RC
      if ( g_pCurrentModel->rc_params.rc_enabled && NULL != s_pPHDownstreamInfoRC )
      if ( s_pPHDownstreamInfoRC->is_failsafe )
         sPHFCT.flags = sPHFCT.flags | FC_TELE_FLAGS_RC_FAILSAFE;
      #endif

      sPH.packet_flags = PACKET_COMPONENT_TELEMETRY;
      sPH.packet_type = PACKET_TYPE_FC_TELEMETRY;
      sPH.total_headers_length = (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry);
      if ( bSendFCMessage )
      {
         sPH.packet_type = PACKET_TYPE_FC_TELEMETRY_EXTENDED;
         sPH.total_headers_length = (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_fc_telemetry) + (u16)sizeof(t_packet_header_fc_extra);
      }

      sPH.total_length = sPH.total_headers_length;
      sPH.extra_flags = 0;

      memcpy(buffer, &sPH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), &sPHFCT, sizeof(t_packet_header_fc_telemetry));
      if ( bSendFCMessage )
         memcpy(buffer+sizeof(t_packet_header)+sizeof(t_packet_header_fc_telemetry), &sPHFCE, sizeof(t_packet_header_fc_extra));

      packet_compute_crc(buffer, sPH.total_length);

      if ( s_bRouterReady && (! s_bRadioInterfacesReinitIsInProgress) )
      {
         int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
         if ( result != sPH.total_length )
            log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
      }

      if ( NULL != s_pProcessStats )
         s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      // Set lat, lon to last values as can have home pos in them if we just sent home pos to controller.

      sPHFCT.latitude = s_lLastPosLat;
      sPHFCT.longitude = s_lLastPosLon;
   }

   if ( ! bDidSentRubyTelemetry )
      return;


   // --------------------------------------
   // Send video info stats

   if ( NULL == s_pSM_VideoInfoStats )
   {
      s_pSM_VideoInfoStats = shared_mem_video_info_stats_open_for_read();
      if ( NULL == s_pSM_VideoInfoStats )
         log_softerror_and_alarm("Failed to open shared mem video info stats for reading.");
      else
         log_line("Opened shared mem video info stats for reading.");
   }
   
   if ( NULL == s_pSM_VideoInfoStatsRadioOut )
   {
      s_pSM_VideoInfoStatsRadioOut = shared_mem_video_info_stats_radio_out_open_for_read();
      if ( NULL == s_pSM_VideoInfoStatsRadioOut )
         log_softerror_and_alarm("Failed to open shared mem video info stats radio out for reading.");
      else
         log_line("Opened shared mem video info stats radio out for reading.");
   }
   
   if ( (NULL != g_pCurrentModel) && (NULL != s_pSM_VideoInfoStats) && (NULL != s_pSM_VideoInfoStatsRadioOut) )
   if ( g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_STATS_VIDEO_INFO)
   {
      sPH.packet_flags = PACKET_COMPONENT_TELEMETRY;
      sPH.packet_type = PACKET_TYPE_RUBY_TELEMETRY_VIDEO_INFO_STATS;
      sPH.total_headers_length = (u16)sizeof(t_packet_header);
      sPH.total_length = sPH.total_headers_length + 2*(u16)sizeof(shared_mem_video_info_stats);
      sPH.extra_flags = 0;

      memcpy(buffer, &sPH, sizeof(t_packet_header));
      memcpy(buffer+sizeof(t_packet_header), (u8*)s_pSM_VideoInfoStats, sizeof(shared_mem_video_info_stats));
      memcpy(buffer+sizeof(t_packet_header) + sizeof(shared_mem_video_info_stats), (u8*)s_pSM_VideoInfoStatsRadioOut, sizeof(shared_mem_video_info_stats));
      packet_compute_crc(buffer, sPH.total_length);

      if ( s_bRouterReady && (! s_bRadioInterfacesReinitIsInProgress) )
      {
         int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
         if ( result != sPH.total_length )
            log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
      }

      if ( NULL != s_pProcessStats )
         s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   // FC RC channels and RC extra packets are sent at the same rate as Ruby telemetry

   // ---------------------------------
   // Send FC RC Channels

   if ( g_pCurrentModel->osd_params.show_stats_rc ||
       (g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_HID_IN_OSD) ||
       (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_STATS_RC)
      )
   {
   sPH.packet_flags = PACKET_COMPONENT_TELEMETRY;
   sPH.packet_type = PACKET_TYPE_FC_RC_CHANNELS;
   sPH.total_headers_length = (u16)sizeof(t_packet_header)+(u16)sizeof(t_packet_header_fc_rc_channels);
   sPH.total_length = sPH.total_headers_length;
   sPH.extra_flags = 0;

   t_packet_header_fc_rc_channels PHFCRCChannels;
   PHFCRCChannels.channels[0] = (u16) s_MAVLinkRCChannels[0];
   PHFCRCChannels.channels[1] = (u16) s_MAVLinkRCChannels[1];
   PHFCRCChannels.channels[2] = (u16) s_MAVLinkRCChannels[2];
   PHFCRCChannels.channels[3] = (u16) s_MAVLinkRCChannels[3];
   PHFCRCChannels.channels[4] = (u16) s_MAVLinkRCChannels[4];
   PHFCRCChannels.channels[5] = (u16) s_MAVLinkRCChannels[5];
   PHFCRCChannels.channels[6] = (u16) s_MAVLinkRCChannels[6];
   PHFCRCChannels.channels[7] = (u16) s_MAVLinkRCChannels[7];
   PHFCRCChannels.channels[8] = (u16) s_MAVLinkRCChannels[8];
   PHFCRCChannels.channels[9] = (u16) s_MAVLinkRCChannels[9];
   PHFCRCChannels.channels[10] = (u16) s_MAVLinkRCChannels[10];
   PHFCRCChannels.channels[11] = (u16) s_MAVLinkRCChannels[11];
   PHFCRCChannels.channels[12] = (u16) s_MAVLinkRCChannels[12];
   PHFCRCChannels.channels[13] = (u16) s_MAVLinkRCChannels[13];

   memcpy(buffer, &sPH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), &PHFCRCChannels, sizeof(t_packet_header_fc_rc_channels));
   packet_compute_crc(buffer, sPH.total_length);

   if ( s_bRouterReady && (! s_bRadioInterfacesReinitIsInProgress) )
   {
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
      if ( result != sPH.total_length )
         log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
   }

   if ( NULL != s_pProcessStats )
      s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   // ----------------------------------
   // Send RC downlink info packet

   bool bAddRCInfo = false;

   #ifdef FEATURE_ENABLE_RC
   if ( g_pCurrentModel->rc_params.rc_enabled && NULL != s_pPHDownstreamInfoRC )
   if ( s_bSendRCInfoBack )
      bAddRCInfo = true;
   #endif

   if ( ! bAddRCInfo )
      return;

   sPH.packet_flags = PACKET_COMPONENT_TELEMETRY;
   sPH.packet_type = PACKET_TYPE_RC_TELEMETRY;
   sPH.total_headers_length = (u16)sizeof(t_packet_header) + (u16)sizeof(t_packet_header_rc_info_downstream);
   sPH.total_length = sPH.total_headers_length;
   sPH.extra_flags = 0;

   memcpy(buffer, &sPH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header), (u8*)s_pPHDownstreamInfoRC, sizeof(t_packet_header_rc_info_downstream));
   packet_compute_crc(buffer, sPH.total_length);

   if ( s_bRouterReady && (! s_bRadioInterfacesReinitIsInProgress) )
   {
      int result = ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, sPH.total_length);
      if ( result != sPH.total_length )
         log_softerror_and_alarm("Failed to send data to router. Sent result: %d", result );
   }

   if ( NULL != s_pProcessStats )
      s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
}

void _on_second_lapse_check()
{
   if (  g_TimeNow < g_TimeLastCheckEverySecond + 1000 )
      return;

   g_TimeLastCheckEverySecond = g_TimeNow;

   if ( g_TimeNow >= s_TimeLastMessagesFromFCPerSecondCalculation + 1000 )
   {
      if ( 0 != s_TimeLastMessagesFromFCPerSecondCalculation )
         s_CountMessagesFromFCPerSecond = s_CountMessagesFromFCPerSecondTemp*1000/(g_TimeNow - s_TimeLastMessagesFromFCPerSecondCalculation);
      s_TimeLastMessagesFromFCPerSecondCalculation = g_TimeNow;
      s_CountMessagesFromFCPerSecondTemp = 0;
   }

   iFCSerialReadBytesPerSecond = iFCSerialReadBytes;
   iFCSerialReadBytes = 0;

   if ( sPHFCT.flight_mode != 0 )
   if ( sPHFCT.flight_mode & FLIGHT_MODE_ARMED )
      sPHFCT.arm_time++;
   
   g_pCurrentModel->updateStatsEverySecond(&sPHFCT);

   if ( sPHFCT.flight_mode != 0 )
   {
      if ( sPHFCT.flight_mode & FLIGHT_MODE_ARMED )
      {
         if ( sPHFCT.arm_time >= 1 && sPHFCT.arm_time < 5)
         {
            if ( ! s_bOnArmEventHandled )
            {
               s_bOnArmEventHandled = true;

               char szBuff[64];
               snprintf(szBuff, 63, "touch %s", FILE_TMP_ARMED);
               hw_execute_bash_command(szBuff, NULL);

               g_pCurrentModel->m_Stats.uTotalFlights++;
               log_line("Armed Event. Saving model, total flights: %d", g_pCurrentModel->m_Stats.uTotalFlights);
               g_pCurrentModel->m_Stats.uCurrentFlightTime = 1; // seconds
               g_pCurrentModel->m_Stats.uCurrentFlightDistance = 0; // in 1/100 meters (cm)
               g_pCurrentModel->m_Stats.uCurrentFlightTotalCurrent = 0; // 0.1 miliAmps (1/10000 amps);

               g_pCurrentModel->m_Stats.uCurrentMaxAltitude = 0; // meters
               g_pCurrentModel->m_Stats.uCurrentMaxDistance = 0; // meters
               g_pCurrentModel->m_Stats.uCurrentMaxCurrent = 0; // miliAmps (1/1000 amps)
               g_pCurrentModel->m_Stats.uCurrentMinVoltage = 100000; // miliVolts (1/1000 volts)
               save_model();
            }
         }

         if ( sPHFCT.arm_time > 1 )
         {
            // speed is in 0.01m/s
            // total distance is in 0.01m
            float speed = sPHFCT.hspeed-100000.0;
            sPHFCT.total_distance += speed;
         }
         else
            sPHFCT.total_distance = 0;
      }
      else
      {
         s_bOnArmEventHandled = false;
         if ( sPHFCT.arm_time != 0 )
         {
            char szBuff[64];
            snprintf(szBuff, 63, "rm -rf %s", FILE_TMP_ARMED);
            hw_execute_bash_command(szBuff, NULL);
         }
         sPHFCT.arm_time = 0;
      }
   }

   _broadcast_vehicle_stats();

   //int sec1 = (g_pCurrentModel->m_Stats.uCurrentOnTime)%60;
   //int min1 = g_pCurrentModel->m_Stats.uCurrentOnTime/60;
   //int sec2 = (g_pCurrentModel->m_Stats.uCurrentFlightTime)%60;
   //int min2 = g_pCurrentModel->m_Stats.uCurrentFlightTime/60;
   //log_line("flights: %d, on: %d:%d, fl: %d:%d, totcur: %d, minv: %u", g_pCurrentModel->m_Stats.uTotalFlights, min1, sec1, min2, sec2, g_pCurrentModel->m_Stats.uCurrentTotalCurrent/10, g_pCurrentModel->m_Stats.uCurrentMinVoltage );
   //log_line("tot current: %u mA, max dist: %u m, max alt: %u m, tot dist: %u m", g_pCurrentModel->m_Stats.uCurrentTotalCurrent/10, g_pCurrentModel->m_Stats.uCurrentMaxDistance, g_pCurrentModel->m_Stats.uCurrentMaxAltitude, g_pCurrentModel->m_Stats.uCurrentFlightDistance/100 );
}

void _send_rc_channel_freq_change(int nLink, int nUp, int nDown)
{
#ifdef FEATURE_ENABLE_RC_FREQ_SWITCH

   if ( nLink != 0 && nLink != 1 )
      return;
   if ( nUp != 1 && nDown != 1 )
      return;

   t_packet_header PH;
   PH.packet_flags = PACKET_COMPONENT_LOCAL_CONTROL;
   PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_DUMMY;
   if ( nLink == 0 )
   {
      if ( nUp )
         PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ1_UP;
      if ( nDown )
         PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ1_DOWN;
   }
   if ( nLink == 1 )
   {
      if ( nUp )
         PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ2_UP;
      if ( nDown )
         PH.packet_type = PACKET_TYPE_LOCAL_CONTROL_VEHICLE_RCCHANGE_FREQ2_DOWN;
   }

   PH.vehicle_id_src = PACKET_COMPONENT_TELEMETRY;
   PH.vehicle_id_dest = 0;
   PH.total_headers_length = sizeof(t_packet_header);
   PH.total_length = sizeof(t_packet_header) + sizeof(type_vehicle_stats_info);
   PH.extra_flags = 0;
   u8 buffer[MAX_PACKET_TOTAL_SIZE];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   memcpy(buffer+sizeof(t_packet_header),(u8*)&(g_pCurrentModel->m_Stats), sizeof(type_vehicle_stats_info));
   packet_compute_crc(buffer, PH.total_length);
   
   if ( s_bRouterReady )
      ruby_ipc_channel_send_message(s_fIPCToRouter, buffer, PH.total_length);

   if ( NULL != s_pProcessStats )
      s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
#endif
}

void _periodic_loop()
{
   if ( g_TimeNow > s_uTimeLastCheckForRadioReinit + 500 )
   {
      s_uTimeLastCheckForRadioReinit = g_TimeNow;   
      if ( access( FILE_TMP_REINIT_RADIO_IN_PROGRESS, R_OK ) != -1 )
         s_bRadioInterfacesReinitIsInProgress = true;
      else
         s_bRadioInterfacesReinitIsInProgress = false;
   }

   if ( g_pCurrentModel->functions_params.bEnableRCTriggerFreqSwitchLink1 )
   if ( g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink1 >= 0 )
   if ( g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink1 < 14 )
   if ( s_MAVLinkRCChannels[g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink1] >= 900 )
   {
      int nRCChannel = g_pCurrentModel->functions_params.iRCTriggerChannelFreqSwitchLink1;

      if ( g_pCurrentModel->functions_params.bRCTriggerFreqSwitchLink1_is3Position )
      {
         if ( s_MAVLinkRCChannels[nRCChannel] < 1200 )
         {
            s_bFrequencySwitch1IsDown = false;
            if ( ! s_bFrequencySwitch1IsUp )
            {
               s_bFrequencySwitch1IsUp = true;
               log_line("Freq Switch 1 Up");
               if ( s_bFrequencySwitch1ReceivedFirstValue )
               {
                  _send_rc_channel_freq_change(0, 1, 0);
               }
            }
         }
         else if ( s_MAVLinkRCChannels[nRCChannel] > 1800 )
         {
            s_bFrequencySwitch1IsUp = false;
            if ( ! s_bFrequencySwitch1IsDown )
            {
               s_bFrequencySwitch1IsDown = true;
               log_line("Freq Switch 1 Down");
               if ( s_bFrequencySwitch1ReceivedFirstValue )
               {
                  _send_rc_channel_freq_change(0, 0, 1);
               }
            }
         }
         else
         {
            if ( s_bFrequencySwitch1IsUp )
            {
               log_line("Freq Switch 1 Center");
               s_bFrequencySwitch1IsUp = false;
            }
            if ( s_bFrequencySwitch1IsDown )
            {
               log_line("Freq Switch 1 Center");
               s_bFrequencySwitch1IsDown = false;
            }
         }
      }
      else
      {
         if ( s_MAVLinkRCChannels[nRCChannel] > 1800 )
         {
            s_bFrequencySwitch1IsUp = false;
            if ( ! s_bFrequencySwitch1IsDown )
            {
               s_bFrequencySwitch1IsDown = true;
               log_line("Freq Switch 1 Down");
               if ( s_bFrequencySwitch1ReceivedFirstValue )
               {
                  _send_rc_channel_freq_change(0, 0, 1);
               }
            }
         }
         else
         {
            if ( s_bFrequencySwitch1IsUp )
            {
               log_line("Freq Switch 1 Center");
               s_bFrequencySwitch1IsUp = false;
            }
            if ( s_bFrequencySwitch1IsDown )
            {
               log_line("Freq Switch 1 Center");
               s_bFrequencySwitch1IsDown = false;
            }
         }
      }
      if ( ! s_bFrequencySwitch1ReceivedFirstValue )
         log_line("Received for the first time the RC channels from FC through MAVLink");
      s_bFrequencySwitch1ReceivedFirstValue = true;
   }
}

void open_datalink_serial_port()
{
   if ( -1 != s_iSerialDataLinkHandle )
      close(s_iSerialDataLinkHandle);
   s_iSerialDataLinkHandle = -1;

   s_iCurrentDataLinkSerialPortIndex = -1;
   hw_serial_port_info_t* pPortInfo = NULL;
       
   for( int i=0; i<hardware_get_serial_ports_count(); i++ )
   {
       pPortInfo = hardware_get_serial_port_info(i);
       if ( NULL == pPortInfo )
          continue;
       if ( ! pPortInfo->iSupported )
          continue;
       if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_DATA_LINK )
       {
          s_iCurrentDataLinkSerialPortIndex = i;
          s_uCurrentDataLinkSerialPortSpeed = pPortInfo->lPortSpeed;
          break;
       }
   }

   if ( s_iCurrentDataLinkSerialPortIndex < 0 )
      return;

   if ( s_iCurrentDataLinkSerialPortIndex == s_iCurrentTelemetrySerialPortIndex )
   {
      log_softerror_and_alarm("Invalid model configuration: both telemetry and auxiliary datalink use the same serial port.");
      return;
   }

   hardware_configure_serial(pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed);
   
   log_line("Opening serial port %s (%s) for auxiliary data link (baud rate: %u) ...", pPortInfo->szName, pPortInfo->szPortDeviceName, (int)pPortInfo->lPortSpeed);
   s_iSerialDataLinkHandle = hardware_open_serial_port(pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed);
   if ( -1 == s_iSerialDataLinkHandle )
      log_softerror_and_alarm("Failed to open serial port %s (%s) for auxiliary datalink.", pPortInfo->szName, pPortInfo->szPortDeviceName );
   else
      log_line("Opened serial port %s (%s) for auxiliary datalink successfully.", pPortInfo->szName, pPortInfo->szPortDeviceName);
}

void open_telemetry_serial_port()
{
   if ( ! bInputFromSTDIN )
   if ( -1 != s_fSerialToFC )
   {
      close(s_fSerialToFC);
      s_fSerialToFC = -1;
   }
   
   if ( bInputFromSTDIN )
      return;

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == MODEL_TELEMETRY_TYPE_NONE )
      return;

   s_iCurrentTelemetrySerialPortIndex = -1;
   hw_serial_port_info_t* pPortInfo = NULL;
   
   for( int i=0; i<hardware_get_serial_ports_count(); i++ )
   {
       pPortInfo = hardware_get_serial_port_info(i);
       if ( NULL == pPortInfo )
          continue;
       if ( ! pPortInfo->iSupported )
          continue;
       
       if ( pPortInfo->iPortUsage == SERIAL_PORT_USAGE_TELEMETRY )
       {
          s_iCurrentTelemetrySerialPortIndex = i;
          s_uCurrentTelemetrySerialPortSpeed = pPortInfo->lPortSpeed;
          break;
       }
   }

   if ( s_iCurrentTelemetrySerialPortIndex < 0 )
   {
      log_softerror_and_alarm("Telemetry is enabled but no serial port is assigned to it.");
      return;
   }

   hardware_configure_serial(pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed); 
         
   log_line("Opening serial port %s (%s) to flight controller (baud rate: %d) ...", pPortInfo->szName, pPortInfo->szPortDeviceName, (int)pPortInfo->lPortSpeed);
   s_fSerialToFC = hardware_open_serial_port(pPortInfo->szPortDeviceName, pPortInfo->lPortSpeed);
   if ( -1 == s_fSerialToFC )
      log_softerror_and_alarm("Failed to open serial port %s (%s) to flight controller.", pPortInfo->szName, pPortInfo->szPortDeviceName);
   else
      log_line("Opened serial port %s (%s) to flight controller successfully at baudrate: %u.", pPortInfo->szName, pPortInfo->szPortDeviceName, (int)pPortInfo->lPortSpeed);
}

void open_shared_mem_objects()
{
   s_pProcessStats = shared_mem_process_stats_open_write(SHARED_MEM_WATCHDOG_TELEMETRY_TX);
   if ( NULL == s_pProcessStats)
      log_softerror_and_alarm("Failed to open shared mem for telemetry tx process watchdog stats for writing: %s", SHARED_MEM_WATCHDOG_TELEMETRY_TX);
   else
      log_line("Opened shared mem for telemetry tx process watchdog stats for writing.");

   s_pSM_VideoInfoStats = shared_mem_video_info_stats_open_for_read();
   if ( NULL == s_pSM_VideoInfoStats )
      log_softerror_and_alarm("Failed to open shared mem video info stats for reading.");
   else
      log_line("Opened shared mem video info stats for reading.");

   s_pSM_VideoInfoStatsRadioOut = shared_mem_video_info_stats_radio_out_open_for_read();
   if ( NULL == s_pSM_VideoInfoStatsRadioOut )
      log_softerror_and_alarm("Failed to open shared mem video info stats radio out for reading.");
   else
      log_line("Opened shared mem video info stats radio out for reading.");
}

void _init_telemetry_structures()
{
   sPH.stream_packet_idx = STREAM_ID_DATA << PACKET_FLAGS_MASK_SHIFT_STREAM_INDEX;
   sPH.vehicle_id_src = g_pCurrentModel->vehicle_id;
   sPH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   _populate_ruby_telemetry_data(&sPHRTE);

   sPHRTE.version = ((SYSTEM_SW_VERSION_MAJOR<<4) | SYSTEM_SW_VERSION_MINOR);
   if ( g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SPECTATOR_ENABLE )
      sPHRTE.flags |= FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;
   else
      sPHRTE.flags &= ~FLAG_RUBY_TELEMETRY_ALLOW_SPECTATOR_TELEMETRY;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      sPHRTE.downlink_datarates[i][0] = 0;
      sPHRTE.downlink_datarates[i][1] = 0;
      sPHRTE.uplink_datarate[i]= 0;
      sPHRTE.uplink_rssi_dbm[i] = 0;
      sPHRTE.uplink_link_quality[i] = 0;
   }

   sPHRTE.uplink_rc_rssi = 255;
   sPHRTE.uplink_mavlink_rc_rssi = 255;
   sPHRTE.uplink_mavlink_rx_rssi = 255;

   sPHFCT.flags = 0;
   sPHFCT.arm_time = 0;
   sPHFCT.flight_mode = 0;
   sPHFCT.gps_fix_type = 0;
   sPHFCT.pitch = 0;
   sPHFCT.roll = 0;
   sPHFCT.vspeed = 100000.0; // it's 1000m offset for negative values (1/100 m)
   sPHFCT.hdop = 2000; // (20)
   sPHFCT.distance = 0;
   sPHFCT.total_distance = 0;
   sPHFCT.fc_kbps = 0;
   sPHFCT.rc_rssi = 0xFF;
   sPHFCT.temperature = 0;
   for( int i=0; i<(int)(sizeof(sPHFCT.extra_info)/sizeof(sPHFCT.extra_info[0])); i++ )
      sPHFCT.extra_info[i] = 0;

   sPHFCT.extra_info[1] = 0xFF; // Second GPS
   sPHFCT.extra_info[2] = 0xFF; // Second GPS
   sPHFCT.extra_info[3] = 0xFF; // Second GPS
   sPHFCT.extra_info[4] = 0xFF; // Second GPS
   sPHFCT.extra_info[5] = 0; // FC telemetry packet index
   sPHFCT.extra_info[6] = 0; // FC telemetry messages per second

   sPHFCE.chunk_index = 0;
   sPHFCE.text[0] = 0;

   for( int i=0; i<MAX_MESSAGES_HISTORY; i++ )
   {
      s_szLastFCMessages[i][0] = 0;
      s_LastReceivedTimeForFCMessages[i] = 0;
   }

   for( int i=0; i<MAX_MAVLINK_RC_CHANNELS; i++ )
      s_MAVLinkRCChannels[i] = 0;

   _process_cached_reboot_info();
}

void handle_sigint(int sig) 
{ 
   log_line("--------------------------");
   log_line("Caught signal to stop: %d", sig);
   log_line("--------------------------");
   g_bQuit = true;
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


   log_init("TX Telemetry");
   //log_add_file("logs/log_tx_telemetry.log"); 

   if ( strcmp(argv[argc-1], "-debug") == 0 )
      g_bDebug = true;
   if ( g_bDebug )
      log_enable_stdout();

   g_pCurrentModel = new Model();
   if ( ! g_pCurrentModel->loadFromFile(FILE_CURRENT_VEHICLE_MODEL, true) )
   {
      log_error_and_alarm("Can't load current model vehicle. Exiting.");
      return -1;
   }
 
   s_fIPCToRouter = ruby_open_ipc_channel_write_endpoint(IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER);
   if ( s_fIPCToRouter < 0 )
      return -1;
 
   s_fIPCFromRouter = ruby_open_ipc_channel_read_endpoint(IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY);
   if ( s_fIPCFromRouter < 0 )
      return -1;
   
   open_shared_mem_objects();

   if ( g_pCurrentModel->uDeveloperFlags & DEVELOPER_FLAGS_BIT_LOG_ONLY_ERRORS )
      log_only_errors();

   hardware_init_serial_ports();

   hw_set_priority_current_proc(g_pCurrentModel->niceTelemetry); 
   parse_telemetry_init(g_pCurrentModel->telemetry_params.vehicle_mavlink_id);

   s_SendIntervalMiliSec_FCTelemetry = 1000/g_pCurrentModel->telemetry_params.update_rate;
   s_SendIntervalMiliSec_RubyTelemetry = 1000/DEFAULT_RUBY_TELEMETRY_UPDATE_RATE;
   if ( g_pCurrentModel->telemetry_params.update_rate > DEFAULT_RUBY_TELEMETRY_UPDATE_RATE )
   {
      if ( g_pCurrentModel->telemetry_params.update_rate < 2*DEFAULT_RUBY_TELEMETRY_UPDATE_RATE )
         s_SendIntervalMiliSec_RubyTelemetry = 1000/g_pCurrentModel->telemetry_params.update_rate;
      else
         s_SendIntervalMiliSec_RubyTelemetry = 2*1000/g_pCurrentModel->telemetry_params.update_rate;
   }
   else
   {
      if ( g_pCurrentModel->telemetry_params.update_rate >= DEFAULT_RUBY_TELEMETRY_UPDATE_RATE/2 )
         s_SendIntervalMiliSec_FCTelemetry = 1000/DEFAULT_RUBY_TELEMETRY_UPDATE_RATE;
      else
         s_SendIntervalMiliSec_FCTelemetry = 2*1000/DEFAULT_RUBY_TELEMETRY_UPDATE_RATE;
   }
   log_line("Using FC telemetry rate of %d ms, Ruby telemetry rate of %d ms", s_SendIntervalMiliSec_FCTelemetry, s_SendIntervalMiliSec_RubyTelemetry);

   s_bSendRCInfoBack = false;
   if ( g_pCurrentModel->osd_params.show_stats_rc ||
       (g_pCurrentModel->osd_params.osd_flags[g_pCurrentModel->osd_params.layout] & OSD_FLAG_SHOW_HID_IN_OSD) ||
       (g_pCurrentModel->osd_params.osd_flags2[g_pCurrentModel->osd_params.layout] & OSD_FLAG_EXT_SHOW_STATS_RC)
      )
      s_bSendRCInfoBack = true;

   log_line("Sending RC info back to controller: %s", s_bSendRCInfoBack?"yes":"no");

   s_bSendFullMAVLinkBackToController = (g_pCurrentModel->telemetry_params.flags & TELEMETRY_FLAGS_SEND_FULL_PACKETS_TO_CONTROLLER)?true:false;
   if ( s_bSendFullMAVLinkBackToController )
      log_line("Flag to send back full mavlink/tml packet to controller is set.");
   else
      log_line("Flag to send back full mavlink/tml packet to controller is not set.");

   if ( NULL != s_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      s_pProcessStats->lastActiveTime = g_TimeNow;
      s_pProcessStats->lastRadioTxTime = g_TimeNow;
      s_pProcessStats->lastRadioRxTime = g_TimeNow;
      s_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }


   s_iCurrentTelemetrySerialPortIndex = -1;
   s_iCurrentDataLinkSerialPortIndex = -1;
   
   for( int i=0; i<g_pCurrentModel->hardware_info.serial_bus_count; i++ )
   {
       if ( g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & ((1<<5)<<8) )
       if ( (g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & 0xFF) == SERIAL_PORT_USAGE_TELEMETRY )
       {
          s_iCurrentTelemetrySerialPortIndex = i;
          s_uCurrentTelemetrySerialPortSpeed = g_pCurrentModel->hardware_info.serial_bus_speed[i];
       }
       if ( g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & ((1<<5)<<8) )
       if ( (g_pCurrentModel->hardware_info.serial_bus_supported_and_usage[i] & 0xFF) == SERIAL_PORT_USAGE_DATA_LINK )
       {
          s_iCurrentDataLinkSerialPortIndex = i;
          s_uCurrentDataLinkSerialPortSpeed = g_pCurrentModel->hardware_info.serial_bus_speed[i];
       }
   }

   log_line("Currently assigned serial port index for telemetry: %d, at baudrate: %u", s_iCurrentTelemetrySerialPortIndex, s_uCurrentTelemetrySerialPortSpeed);
   log_line("Currently assigned serial port index for data link: %d, at baudrate: %u", s_iCurrentDataLinkSerialPortIndex, s_uCurrentDataLinkSerialPortSpeed);

   telemetryBufferFromFCMaxSize = 300;
   if ( telemetryBufferFromFCMaxSize > RAW_TELEMETRY_MAX_BUFFER )
      telemetryBufferFromFCMaxSize = RAW_TELEMETRY_MAX_BUFFER;
   telemetryBufferFromFCCount = 0;
   telemetryBufferFromFCLastSendTime = g_TimeNow;
   log_line("Telemetry from FC chunk size: %d for serial speed: %u bps", telemetryBufferFromFCMaxSize, s_uCurrentTelemetrySerialPortSpeed);

   _init_telemetry_structures();

   _broadcast_vehicle_stats();

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != MODEL_TELEMETRY_TYPE_NONE )
   {
      g_TimeNow = get_current_timestamp_ms();
      do
      {
         for( int i=0; i<4; i++ )
         {
         hardware_sleep_ms(500);
         if ( NULL != s_pProcessStats )
         {
            g_TimeNow = get_current_timestamp_ms();
            s_pProcessStats->lastActiveTime = g_TimeNow;
            s_pProcessStats->lastRadioTxTime = g_TimeNow;
            s_pProcessStats->lastRadioRxTime = g_TimeNow;
            s_pProcessStats->lastIPCIncomingTime = g_TimeNow;
            s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
         }
         }
         g_TimeNow = get_current_timestamp_ms(); 
      }
      while ( g_TimeNow < 5000 );

      if ( bInputFromSTDIN )
      {
         log_line("Reading FC serial data from STDIN");
         s_fSerialToFC = STDIN_FILENO;
      }
      else
         open_telemetry_serial_port();
   }
   else
      log_line("FC Telemetry is disabled. Do not open serial port.");

   open_datalink_serial_port();

   s_bOnArmEventHandled = false;

   log_line("Started TX Telemetry. Running now.");
   log_line("----------------------------------");

   if ( NULL != s_pProcessStats )
   {
      g_TimeNow = get_current_timestamp_ms();
      s_pProcessStats->lastActiveTime = g_TimeNow;
      s_pProcessStats->lastRadioTxTime = g_TimeNow;
      s_pProcessStats->lastRadioRxTime = g_TimeNow;
      s_pProcessStats->lastIPCIncomingTime = g_TimeNow;
      s_pProcessStats->lastIPCOutgoingTime = g_TimeNow;
   }

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == MODEL_TELEMETRY_TYPE_MAVLINK )
      send_mavlink_setup();

   _broadcast_vehicle_stats();

   g_TimeStart = get_current_timestamp_ms();

   int iSleepTime = 4;

   if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == MODEL_TELEMETRY_TYPE_NONE )
      iSleepTime = 10;

   while ( !g_bQuit )
   {
      hardware_sleep_ms(iSleepTime);

      g_TimeNow = get_current_timestamp_ms();
      u32 tTime0 = g_TimeNow;

      if ( NULL != s_pProcessStats )
      {
         s_pProcessStats->uLoopCounter++;
         s_pProcessStats->lastActiveTime = g_TimeNow;
      }

      _periodic_loop();

      if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == MODEL_TELEMETRY_TYPE_NONE )
         iSleepTime = 10;
      else
      {
         iSleepTime = 4;
         if ( g_TimeNow > g_TimeLastMessageFromFC + 4000 )
         if ( s_bRetrySetupTelemetry )
         {
            if ( ! bInputFromSTDIN )
            {
               if ( -1 != s_fSerialToFC )
                  close(s_fSerialToFC);
               s_fSerialToFC = -1;
            }
            log_line("Flight controller telemetry is enabled and no telemetry received from flight controller. Reinitialize serial telemetry...");
            open_telemetry_serial_port();
            s_bMAVLinkSetupSent = false;
            g_TimeLastMessageFromFC = g_TimeNow - 1200;
            if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != MODEL_TELEMETRY_TYPE_NONE )
               send_mavlink_setup();
            if ( s_iCurrentTelemetrySerialPortIndex < 0 )
               s_bRetrySetupTelemetry = false;
         }
      }
      
      if ( g_pCurrentModel->rc_params.rc_enabled )
      if ( NULL == s_pPHDownstreamInfoRC )
      {
         #ifdef FEATURE_ENABLE_RC
         s_pPHDownstreamInfoRC = shared_mem_rc_downstream_info_open_read();
         if ( NULL == s_pPHDownstreamInfoRC )
            log_softerror_and_alarm("Failed to open RC Download info shared memory for read.");
         else
            log_line("Opened RC Download info shared memory for read: success.");
         #endif
      }

      if ( g_pCurrentModel->telemetry_params.fc_telemetry_type != MODEL_TELEMETRY_TYPE_NONE )
         try_read_serial_telemetry();    
  
      try_read_serial_datalink();

      int maxMsgToRead = 5;
      while ( (maxMsgToRead > 0) && try_read_messages_from_router() )
         maxMsgToRead--;

      if ( g_pCurrentModel->telemetry_params.fc_telemetry_type == MODEL_TELEMETRY_TYPE_MAVLINK )
      if ( g_pCurrentModel->rc_params.rc_enabled )
      if ( g_pCurrentModel->rc_params.flags & RC_FLAGS_OUTPUT_ENABLED )
         _send_rc_data_to_FC();


      if ( g_pCurrentModel->telemetry_params.bControllerHasInputTelemetry || g_pCurrentModel->telemetry_params.bControllerHasOutputTelemetry )
      if ( telemetryBufferFromFCCount >= RAW_TELEMETRY_MIN_SEND_LENGTH || 
          (telemetryBufferFromFCCount > 0 && g_TimeNow >= telemetryBufferFromFCLastSendTime + RAW_TELEMETRY_SEND_TIMEOUT ) )
         send_raw_telemetry_packet_to_controller();


      if ( dataLinkSerialBufferCount >= AUXILIARY_DATA_LINK_MIN_SEND_LENGTH || 
          (dataLinkSerialBufferCount > 0 && g_TimeNow >= dataLinkSerialBufferLastSendTime + AUXILIARY_DATA_LINK_SEND_TIMEOUT ) )
         send_datalink_data_packet_to_controller();

      g_pCurrentModel->updateStatsMaxCurrentVoltage(&sPHFCT);
      _on_second_lapse_check();

      if ( g_TimeNow < (s_LastSendFCTelemetryTime + s_SendIntervalMiliSec_FCTelemetry) )
      if ( g_TimeNow < (s_LastSendRubyTelemetryTime + s_SendIntervalMiliSec_RubyTelemetry) )
      {
         u32 tNow = get_current_timestamp_ms();
         if ( NULL != s_pProcessStats )
         {
            if ( s_pProcessStats->uMaxLoopTimeMs < tNow - tTime0 )
               s_pProcessStats->uMaxLoopTimeMs = tNow - tTime0;
            s_pProcessStats->uTotalLoopTime += tNow - tTime0;
            if ( 0 != s_pProcessStats->uLoopCounter )
               s_pProcessStats->uAverageLoopTimeMs = s_pProcessStats->uTotalLoopTime / s_pProcessStats->uLoopCounter;
         }

         u32 dTime = get_current_timestamp_ms() - tTime0;
         if ( dTime > 150 )
            log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
         continue;
      }
      
      _send_telemetry_to_controller();

      u32 tNow = get_current_timestamp_ms();
      if ( NULL != s_pProcessStats )
      {
         if ( s_pProcessStats->uMaxLoopTimeMs < tNow - tTime0 )
            s_pProcessStats->uMaxLoopTimeMs = tNow - tTime0;
         s_pProcessStats->uTotalLoopTime += tNow - tTime0;
         if ( 0 != s_pProcessStats->uLoopCounter )
            s_pProcessStats->uAverageLoopTimeMs = s_pProcessStats->uTotalLoopTime / s_pProcessStats->uLoopCounter;
      }
      u32 dTime = get_current_timestamp_ms() - tTime0;
      if ( dTime > 150 )
         log_softerror_and_alarm("Main processing loop took too long (%u ms).", dTime);
   }

   log_line("Stopping...");
   
   shared_mem_video_info_stats_close(s_pSM_VideoInfoStats);
   shared_mem_video_info_stats_radio_out_close(s_pSM_VideoInfoStatsRadioOut);
   shared_mem_process_stats_close(SHARED_MEM_WATCHDOG_TELEMETRY_TX, s_pProcessStats);
   #ifdef FEATURE_ENABLE_RC
   shared_mem_rc_downstream_info_close(s_pPHDownstreamInfoRC);
   #endif
   
   ruby_close_ipc_channel(s_fIPCToRouter);
   ruby_close_ipc_channel(s_fIPCFromRouter);
   s_fIPCToRouter = -1;
   s_fIPCFromRouter = -1;

   if ( -1 != s_iSerialDataLinkHandle )
      close(s_iSerialDataLinkHandle);
   s_iSerialDataLinkHandle = -1;

   if ( ! bInputFromSTDIN )
   {
      if ( -1 != s_fSerialToFC )
         close(s_fSerialToFC);
      s_fSerialToFC = -1;
   }
   save_model();

   log_line("Stopped.Exit");
   log_line("------------------------");
   return 0;
}
