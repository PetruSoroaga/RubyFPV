/*
    MIT Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "test_link_params.h"
#include "../base/ruby_ipc.h"
#include "../base/models_list.h"
#include "../radio/radiopackets2.h"
#include "../radio/radiopacketsqueue.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../common/string_utils.h"
#include "timers.h"
#include "shared_vars.h"
#include "radio_links.h"
#include "packets_utils.h"

#define TEST_LINK_STATE_SEND_START_TO_VEHICLE 1
#define TEST_LINK_STATE_PING 2
#define TEST_LINK_STATE_ENDED 10
#define TEST_LINK_SUCCESS_PING_COUNTS 2

extern t_packet_queue s_QueueRadioPackets;

int s_iTestLinkState = 0;
int s_iTestLinkIndex = -1;
int s_iTestLinkCountDifferences = 0;
bool s_bTestLinkParamsInProgress = false;
bool s_bTestLinkParamsSucceded = false;
bool s_bApplyRadioParamsInProgress = false;
bool s_bTestLinkOnlyFreqChanged = false;
u32 s_uTimeStartTestLink = 0;
u32 s_uTimeLastTestPacketSent = 0;
u32 s_uTimeEndCurrentStep = 0;
u32 s_uTestMsgSentCount = 0;
u32 s_uCountPingSent = 0;
u32 s_uCountPingReceived = 0;
u32 s_uCountPingsSentByVehicle = 0;
u32 s_uCountPingsReceivedByVehicle = 0;
type_radio_links_parameters s_RadioLinksParamsToTest;
type_radio_links_parameters s_RadioLinksParamsOriginal;

u8 s_uPacketStartTestLink[MAX_PACKET_TOTAL_SIZE];
u8 s_uPacketToSend[MAX_PACKET_TOTAL_SIZE];

pthread_t s_pThreadTestLinkWorker;
type_radio_links_parameters s_RadioLinkParamsToChangeFrom;
type_radio_links_parameters s_RadioLinkParamsToChangeTo;
bool s_bTestLinkPausedInterfaces = false;
bool s_bMustReopenTestInterfaces = false;
bool s_bMustReopenRadioInterfacesForRead[MAX_RADIO_INTERFACES];
bool s_bMustReopenRadioInterfacesForWrite[MAX_RADIO_INTERFACES];
bool s_bMustResumeTestInterfaces = false;
bool s_bMustResumeRadioInterfacesForRead[MAX_RADIO_INTERFACES];
bool s_bMustResumeRadioInterfacesForWrite[MAX_RADIO_INTERFACES];
bool s_bSentTestPingAfterPingState = false;

u8 s_uTestLinkRunNumber = 0;

bool test_link_is_in_progress()
{
   return (s_bTestLinkParamsInProgress || s_bApplyRadioParamsInProgress || s_bMustReopenTestInterfaces || s_bMustResumeTestInterfaces);
}

int test_link_active()
{
   return s_iTestLinkIndex;
}

void test_link_state_get_state_string(int iState, char* szBuffer)
{
   if ( NULL == szBuffer )
      return;
   strcpy(szBuffer, "N/A");
   if ( iState == TEST_LINK_STATE_SEND_START_TO_VEHICLE )
      strcpy(szBuffer, "Send_Start_To_Vehicle");
   if ( iState == TEST_LINK_STATE_PING )
      strcpy(szBuffer, "Send_Ping_To_Vehicle");
   if ( iState == TEST_LINK_STATE_ENDED )
      strcpy(szBuffer, "Send_End_To_Vehicle");
}

void test_link_switch_to_state(int iNewState, u32 uTimeout)
{
   char szStateOld[64];
   char szStateNew[64];
   test_link_state_get_state_string(s_iTestLinkState, szStateOld);
   test_link_state_get_state_string(iNewState, szStateNew);

   s_iTestLinkState = iNewState;
   s_uTimeEndCurrentStep = g_TimeNow + uTimeout;

   log_line("[TestLink-%d] Switch from state [%s] to state [%s]. Timesout %u ms from now", (int)s_uTestLinkRunNumber, szStateOld, szStateNew, uTimeout);
   if ( s_iTestLinkState == TEST_LINK_STATE_ENDED )
   {
      log_line("[TestLink-%d] Controller sent %u pings, received %u pings from vehicle, vehicle sent %u pings, vehicle received %u pings from controller.",
         (int)s_uTestLinkRunNumber, s_uCountPingSent, s_uCountPingReceived, s_uCountPingsSentByVehicle, s_uCountPingsReceivedByVehicle );
   }
}

void test_link_reset_state()
{
   s_bTestLinkParamsInProgress = false;
   s_bTestLinkParamsSucceded = false;
   s_bTestLinkPausedInterfaces = false;
   s_bTestLinkOnlyFreqChanged = false;
   s_iTestLinkState = 0;
   s_uTimeStartTestLink = 0;
   s_uTimeLastTestPacketSent = 0;
   s_uTimeEndCurrentStep = 0;
   s_uCountPingSent = 0;
   s_uCountPingReceived = 0;
   s_uCountPingsSentByVehicle = 0;
   s_uCountPingsReceivedByVehicle = 0;
   
   s_bSentTestPingAfterPingState = false;
   s_bTestLinkPausedInterfaces = false;
   s_bMustReopenTestInterfaces = false;
   s_bMustResumeTestInterfaces = false;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      s_bMustResumeRadioInterfacesForRead[i] = false;
      s_bMustResumeRadioInterfacesForWrite[i] = false;

      s_bMustReopenRadioInterfacesForRead[i] = false;
      s_bMustReopenRadioInterfacesForWrite[i] = false;
   }
}

static void * _thread_test_link_worker(void *argument)
{
   log_line("[TestLink-%d] Started worker thread to update radio interfaces for vehicle radio link %d.", (int)s_uTestLinkRunNumber, s_iTestLinkIndex+1);
   radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinkParamsToChangeFrom, &s_RadioLinkParamsToChangeTo);
   log_line("[TestLink-%d] Finished worker thread to update radio interfaces for vehicle radio link %d.", (int)s_uTestLinkRunNumber, s_iTestLinkIndex+1);
   s_bApplyRadioParamsInProgress = false;
   return NULL;
}

void test_link_pause_interfaces()
{
   if ( s_bTestLinkPausedInterfaces )
      return;
   
   s_bTestLinkPausedInterfaces = true;

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != s_iTestLinkIndex )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( pRadioHWInfo->openedForWrite )
      {
         if ( ! s_bMustResumeRadioInterfacesForWrite[i] )
            radio_tx_pause_radio_interface(i);
         s_bMustResumeRadioInterfacesForWrite[i] = true;
      }
      if ( pRadioHWInfo->openedForRead )
      {
         if ( ! s_bMustResumeRadioInterfacesForRead[i] )
            radio_rx_pause_interface(i);
         s_bMustResumeRadioInterfacesForRead[i] = true;
      }
   }
}

void test_link_resume_interfaces()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != s_iTestLinkIndex )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      
      if ( s_bMustResumeRadioInterfacesForWrite[i] )
         radio_tx_resume_radio_interface(i);
      s_bMustResumeRadioInterfacesForWrite[i] = false;
      
      if ( s_bMustResumeRadioInterfacesForRead[i] )
         radio_rx_resume_interface(i);
      s_bMustResumeRadioInterfacesForRead[i] = false;
   }
   s_bTestLinkPausedInterfaces = false;
   s_bMustResumeTestInterfaces = false;
}


void test_link_close_interfaces()
{
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      s_bMustReopenRadioInterfacesForRead[i] = false;
      s_bMustReopenRadioInterfacesForWrite[i] = false;
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != s_iTestLinkIndex )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( pRadioHWInfo->openedForWrite )
      {
         // Do not close interface if only frequency changed
         if ( ! s_bTestLinkOnlyFreqChanged )
         {
            radio_close_interface_for_write(i);
            g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
            s_bMustReopenRadioInterfacesForWrite[i] = true;
         }
      }
      if ( pRadioHWInfo->openedForRead )
      {
         // Do not close interface if only frequency changed
         if ( ! s_bTestLinkOnlyFreqChanged )
         {
            radio_close_interface_for_read(i);
            g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
            s_bMustReopenRadioInterfacesForRead[i] = true;
         }
      }

      if ( ((pRadioHWInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS) ||
        ((pRadioHWInfo->typeAndDriver & 0xFF) == RADIO_TYPE_RALINK) )
         s_uTimeEndCurrentStep = g_TimeNow + 2*TIMEOUT_TEST_LINK_STATE_START;
   }
}

void test_link_reopen_interfaces()
{
   log_line("[TestLink-%d] Reopening radio interfaces for vehicle radio link %d...", (int)s_uTestLinkRunNumber, s_iTestLinkIndex+1);
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId != s_iTestLinkIndex )
         continue;

      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;

      if (s_bMustReopenRadioInterfacesForRead[i] )
      {
         s_bMustReopenRadioInterfacesForRead[i] = false;
         if ( radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK) > 0 )
            g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
      }

      if (s_bMustReopenRadioInterfacesForWrite[i] )
      {
         s_bMustReopenRadioInterfacesForWrite[i] = false;
         if ( radio_open_interface_for_write(i) > 0 )
            g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
      }
   }
   s_bMustReopenTestInterfaces = false;
   log_line("[TestLink-%d] Done reopen radio interfaces for vehicle radio link %d.", (int)s_uTestLinkRunNumber, s_iTestLinkIndex+1);
}

u8* test_link_generate_start_packet(u32 uVehicleId, int iLinkId, type_radio_links_parameters* pRadioParamsToTest)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = uVehicleId;
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(type_radio_links_parameters);

   memcpy(s_uPacketStartTestLink, (u8*)&PH, sizeof(t_packet_header));
   s_uPacketStartTestLink[sizeof(t_packet_header)] = (u8)iLinkId;
   s_uPacketStartTestLink[sizeof(t_packet_header)+1] = (u8)s_uTestLinkRunNumber;
   s_uPacketStartTestLink[sizeof(t_packet_header)+2] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(s_uPacketStartTestLink + sizeof(t_packet_header) + 3*sizeof(u8), pRadioParamsToTest, sizeof(type_radio_links_parameters));
   return s_uPacketStartTestLink;
}

void test_link_send_status_message_to_central(const char* szMsg)
{
   if ( (NULL == szMsg) || (0 == szMsg[0]) )
      return;

   log_line("[TestLink-%d] Send status message to central: [%s]", (int)s_uTestLinkRunNumber, szMsg);

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + 4*sizeof(u8) + strlen(szMsg)+1;

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = (u8) s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+1] = (u8) s_uTestLinkRunNumber;
   buffer[sizeof(t_packet_header)+2] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_STATUS;
   buffer[sizeof(t_packet_header)+3] = (u8) strlen(szMsg)+1;
   memcpy( &(buffer[sizeof(t_packet_header)+4]), szMsg, strlen(szMsg)+1);
   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
       log_softerror_and_alarm("No pipe to central to send message to.");
}

void test_link_send_end_message_to_central(bool bSucceeded)
{
   log_line("[TestLink-%d] Send end message to central. Succeeded? %s", (int)s_uTestLinkRunNumber, (bSucceeded?"Yes":"No"));

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + 4*sizeof(u8);

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = (u8) s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+1] = (u8) s_uTestLinkRunNumber;
   buffer[sizeof(t_packet_header)+2] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END;
   buffer[sizeof(t_packet_header)+3] = (u8) (bSucceeded?1:0);
   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
       log_softerror_and_alarm("No pipe to central to send message to.");
}

void _test_link_send_ping_to_vehicle()
{
   s_uCountPingSent++;
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + 2*sizeof(u32);

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = (u8)s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+1] = (u8)s_uTestLinkRunNumber;
   buffer[sizeof(t_packet_header)+2] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING;
   memcpy(buffer + sizeof(t_packet_header) + 3*sizeof(u8), &s_uCountPingSent, sizeof(u32));
   memcpy(buffer + sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32), &s_uCountPingReceived, sizeof(u32));
   packets_queue_add_packet(&s_QueueRadioPackets, buffer);
   log_line("[TestLink-%d] Sent ping %d to vehicle", (int)s_uTestLinkRunNumber, s_uCountPingSent);
}

void _test_link_send_end_message_to_vehicle(bool bSucceeded)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_length = sizeof(t_packet_header) + 4*sizeof(u8);

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = (u8)s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+1] = (u8)s_uTestLinkRunNumber;
   buffer[sizeof(t_packet_header)+2] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END;
   buffer[sizeof(t_packet_header)+3] = (u8) (bSucceeded?1:0);
   packets_queue_add_packet(&s_QueueRadioPackets, buffer);
   log_line("[TestLink-%d] Sent end message (repeat %d) to vehicle", (int)s_uTestLinkRunNumber, s_uTestMsgSentCount);
}

bool test_link_start(u32 uControllerId, u32 uVehicleId, int iLinkId, type_radio_links_parameters* pRadioParamsToTest)
{
   log_line("[TestLink-%d] Start test flow. CID: %u, VID: %u, radio link %d", (int)s_uTestLinkRunNumber, uControllerId, uVehicleId, iLinkId+1);

   if ( (s_iTestLinkState != 0) || test_link_is_in_progress() )
   {
      log_line("[TestLink-%d] is already in progress. Ignore request.", (int)s_uTestLinkRunNumber);
      return false;
   }

   s_iTestLinkIndex = iLinkId;
   s_uTestLinkRunNumber++;
   memcpy(&s_RadioLinksParamsToTest, pRadioParamsToTest, sizeof(type_radio_links_parameters));

   s_iTestLinkCountDifferences = g_pCurrentModel->logVehicleRadioLinkDifferences(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsToTest);

   memcpy(&s_RadioLinksParamsOriginal, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   memcpy(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));
   
   test_link_reset_state();

   if ( (1 == s_iTestLinkCountDifferences) && (s_RadioLinksParamsOriginal.link_frequency_khz[s_iTestLinkIndex] != s_RadioLinksParamsToTest.link_frequency_khz[s_iTestLinkIndex]) )
   {
      log_line("[TestLink-%d] Only frequency must be changed for this test.", (int)s_uTestLinkRunNumber);
      s_bTestLinkOnlyFreqChanged = true;
   }

   s_bTestLinkParamsInProgress = true;
   s_bTestLinkParamsSucceded = false;
   
   test_link_switch_to_state(TEST_LINK_STATE_SEND_START_TO_VEHICLE, TIMEOUT_TEST_LINK_STATE_START );
   
   s_uTimeLastTestPacketSent = g_TimeNow;
   s_uTestMsgSentCount = 0;

   test_link_generate_start_packet(uVehicleId, iLinkId, pRadioParamsToTest);
   
   t_packet_header* pPH = (t_packet_header*)s_uPacketStartTestLink;
   pPH->vehicle_id_src = uControllerId;
   pPH->vehicle_id_dest = uVehicleId;

   // For frequency change only, pause radio interfaces and apply new params after start command confirmation is received from vehicle
   if ( s_bTestLinkOnlyFreqChanged )
      return true;

   test_link_pause_interfaces();
   test_link_close_interfaces();
   s_bMustResumeTestInterfaces = true;
   s_bMustReopenTestInterfaces = true;

   log_line("[TestLink-%d] Apply new parameters for radio link %d", (int)s_uTestLinkRunNumber, s_iTestLinkIndex+1);
   s_bApplyRadioParamsInProgress = true;
   memcpy(&s_RadioLinkParamsToChangeTo, &s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));
   memcpy(&s_RadioLinkParamsToChangeFrom, &s_RadioLinksParamsOriginal, sizeof(type_radio_links_parameters));

   if ( 0 != pthread_create(&s_pThreadTestLinkWorker, NULL, &_thread_test_link_worker, NULL) )
   {
      radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinksParamsOriginal, &s_RadioLinksParamsToTest);
      test_link_reopen_interfaces();
      test_link_resume_interfaces();
      s_bApplyRadioParamsInProgress = false;
      s_bMustReopenTestInterfaces = false;
      s_bMustResumeTestInterfaces = false;
   }

   return true;
}

void test_link_switch_to_end_flow_state(bool bSucceeded)
{
   if ( 0 == s_iTestLinkState )
   {
      log_line("[TestLink-%d] Ended without executing. Succeeded? %s", (int)s_uTestLinkRunNumber, (bSucceeded?"Yes":"No"));
      s_bTestLinkParamsSucceded = bSucceeded;
      s_bTestLinkParamsInProgress = false;
      return;
   }
 
   test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
   
   s_bTestLinkParamsSucceded = bSucceeded;
   s_uTimeLastTestPacketSent = 0;
   s_uTestMsgSentCount = 0;

   log_line("[TestLink-%d] Switched to state end flow. Succeeded? %s", (int)s_uTestLinkRunNumber, (bSucceeded?"Yes":"No"));

   if ( s_bTestLinkParamsSucceded )
   {
      test_link_send_status_message_to_central("New parameters are supported and applied.");
      test_link_send_status_message_to_central("Saving state...");
   }
   else
   {
      test_link_send_status_message_to_central("New parameters are not supported.");
      test_link_send_status_message_to_central("Cleaning up...");
   }
}

void test_link_process_received_message(int iInterfaceIndex, u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
   {
      log_softerror_and_alarm("[TestLink-%d] Process received message from vehicle on radio interface %d but message is null", (int)s_uTestLinkRunNumber, iInterfaceIndex+1);
      return;
   }
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   if ( (pPH->packet_type != PACKET_TYPE_TEST_RADIO_LINK) || (pPH->total_length < sizeof(t_packet_header)+3) )
   {
      log_softerror_and_alarm("[TestLink-%d] Process received message from vehicle on radio interface %d but message is invalid.", (int)s_uTestLinkRunNumber, iInterfaceIndex+1);
      return;
   }
   int iRadioLinkId = pPacketBuffer[sizeof(t_packet_header)];
   //int iTestNb = pPacketBuffer[sizeof(t_packet_header)+1];
   int iMsgType = pPacketBuffer[sizeof(t_packet_header)+2];
   int iMsgLen = pPH->total_length - sizeof(t_packet_header)-3*sizeof(u8);
   
   char szBuff[128];
   test_link_state_get_state_string(s_iTestLinkState, szBuff);

   // Start message

   if ( (iMsgType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START) && (s_iTestLinkState == TEST_LINK_STATE_SEND_START_TO_VEHICLE) )
   {
      log_line("[TestLink-%d] Processing received test link message type %s from VID %u, test link state is: %s, %d data bytes, on radio interface %d for radio link %d",
         (int)s_uTestLinkRunNumber, str_get_packet_test_link_command(iMsgType), pPH->vehicle_id_src,
         szBuff, iMsgLen,
         iInterfaceIndex+1, iRadioLinkId+1);
      if ( pPH->total_length != (int)(sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(type_radio_links_parameters)) )
      {
         log_softerror_and_alarm("[TestLink-%d] Received old format. Ignore it.", (int)s_uTestLinkRunNumber);
         return;
      }
      test_link_send_status_message_to_central("Vehicle updated paramaters. Testing link...");

      // For frequency change only, pause radio interfaces and apply new params now
      if ( s_bTestLinkOnlyFreqChanged )
      if ( ! s_bApplyRadioParamsInProgress )
      {
         test_link_pause_interfaces();
         s_bMustResumeTestInterfaces = true;
         log_line("[TestLink-%d] Apply new parameters (freq) for radio link %d", (int)s_uTestLinkRunNumber, s_iTestLinkIndex+1);
         s_bApplyRadioParamsInProgress = true;
         memcpy(&s_RadioLinkParamsToChangeTo, &s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));
         memcpy(&s_RadioLinkParamsToChangeFrom, &s_RadioLinksParamsOriginal, sizeof(type_radio_links_parameters));

         if ( 0 != pthread_create(&s_pThreadTestLinkWorker, NULL, &_thread_test_link_worker, NULL) )
         {
            radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinksParamsOriginal, &s_RadioLinksParamsToTest);
            test_link_resume_interfaces();
            s_bApplyRadioParamsInProgress = false;
            s_bMustResumeTestInterfaces = false;
         }
      }

      test_link_switch_to_state(TEST_LINK_STATE_PING, TIMEOUT_TEST_LINK_STATE_PING);
      s_uCountPingSent = 0;
      s_uCountPingReceived = 0;
      s_uCountPingsSentByVehicle = 0;
      s_uCountPingsReceivedByVehicle = 0;
      s_uTimeLastTestPacketSent = g_TimeNow;
      return;
   }

   // Ping message

   if ( (iMsgType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING) )
   {
      log_line("[TestLink-%d] Processing received test link message type %s from VID %u, test link state is: %s, %d data bytes, on radio interface %d for radio link %d",
         (int)s_uTestLinkRunNumber, str_get_packet_test_link_command(iMsgType), pPH->vehicle_id_src,
         szBuff, iMsgLen,
         iInterfaceIndex+1, iRadioLinkId+1);

      if ( pPH->total_length != (int)(sizeof(t_packet_header) + 3*sizeof(u8) + 2*sizeof(u32)) )
      {
         log_softerror_and_alarm("[TestLink-%d] Received old format. Ignore it.", (int)s_uTestLinkRunNumber);
         return;
      }

      s_uCountPingReceived++;
      memcpy(&s_uCountPingsSentByVehicle, pPacketBuffer + sizeof(t_packet_header) + 3*sizeof(u8), sizeof(u32));
      memcpy(&s_uCountPingsReceivedByVehicle, pPacketBuffer + sizeof(t_packet_header) + 3*sizeof(u8)+sizeof(u32), sizeof(u32));
      log_line("[TestLink-%d] Received test ping nb %u from vehicle. Vehicle sent %u pings so far, vehicle received %u pings so far, we sent %u pings.",
         (int)s_uTestLinkRunNumber, s_uCountPingReceived, s_uCountPingsSentByVehicle, s_uCountPingsReceivedByVehicle, s_uCountPingSent);

      if ( s_iTestLinkState == TEST_LINK_STATE_ENDED )
      {
         // Send to vehicle one more ping after the ping state ended, so that the vehicle can increase the pings count too
         if ( ! s_bSentTestPingAfterPingState )
         {
            log_line("[TestLink-%d] Send one more ping to vehicle after ping state ended.", (int)s_uTestLinkRunNumber);
            s_bSentTestPingAfterPingState = true;
            _test_link_send_ping_to_vehicle();
         }
      }

      if ( s_iTestLinkState == TEST_LINK_STATE_PING )
      if ( s_uCountPingSent >= TEST_LINK_SUCCESS_PING_COUNTS )
      if ( s_uCountPingReceived >= TEST_LINK_SUCCESS_PING_COUNTS )
      if ( s_uCountPingsSentByVehicle >= TEST_LINK_SUCCESS_PING_COUNTS )
      if ( s_uCountPingsReceivedByVehicle >= TEST_LINK_SUCCESS_PING_COUNTS )
      {
         log_line("[TestLink-%d] Pings where sent and received successfully on both sides. Params are working fine.", (int)s_uTestLinkRunNumber);
         test_link_send_status_message_to_central("Link is active");
         test_link_switch_to_end_flow_state(true);
      }
      return;
   }

   // End message
   
   if ( (iMsgType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END) && (s_iTestLinkState == TEST_LINK_STATE_ENDED) )
   {
      log_line("[TestLink-%d] Processing received test link message type %s from VID %u, test link state is: %s, %d data bytes, on radio interface %d for radio link %d",
        (int)s_uTestLinkRunNumber, str_get_packet_test_link_command(iMsgType), pPH->vehicle_id_src,
        szBuff, iMsgLen,
        iInterfaceIndex+1, iRadioLinkId+1);

      if ( pPH->total_length != (int)(sizeof(t_packet_header) + 4*sizeof(u8)) )
      {
         log_softerror_and_alarm("[TestLink-%d] Received old format. Ignore it.", (int)s_uTestLinkRunNumber);
         return;
      }
      u8 uSuccessOnVehicle = pPacketBuffer[sizeof(t_packet_header) + 3*sizeof(u8)];
      log_line("[TestLink-%d] Test link has ended on both ends. Succeeded locally? %s. Succeeded on vehicle? %s",
         (int)s_uTestLinkRunNumber, 
         (s_bTestLinkParamsSucceded?"Yes":"No"),
         (uSuccessOnVehicle?"Yes":"No"));
      s_bTestLinkParamsInProgress = false;
      s_iTestLinkState = 0;
      saveControllerModel(g_pCurrentModel);
      test_link_send_end_message_to_central(s_bTestLinkParamsSucceded);
      return;
   }

   log_line("[TestLink-%d] Ignored received test link message from VID %u, msg type: %s, test link state is: %s, %d data bytes for radio link %d",
      (int)s_uTestLinkRunNumber, pPH->vehicle_id_src, str_get_packet_test_link_command(iMsgType),
      szBuff, iMsgLen, iRadioLinkId+1);
}

void test_link_loop()
{
   static u32 s_uTimeLastTestLinkLoop = 0;

   if ( g_TimeNow > s_uTimeLastTestLinkLoop + 50 )
   {
      s_uTimeLastTestLinkLoop = g_TimeNow;
      char szBuff[128];
      test_link_state_get_state_string(s_iTestLinkState, szBuff);
      log_line("[TestLink-%d] Loop state: %s, time to end step: %u ms, radio config in progress: %s", (int)s_uTestLinkRunNumber, szBuff, s_uTimeEndCurrentStep - g_TimeNow, (s_bApplyRadioParamsInProgress?"Yes":"No"));
   }

   if ( s_bApplyRadioParamsInProgress )
      return;

   if ( s_bMustReopenTestInterfaces )
   {
      test_link_reopen_interfaces();
      s_bMustReopenTestInterfaces = false;
   }

   if ( ! s_bMustReopenTestInterfaces )
   if ( s_bMustResumeTestInterfaces )
   {
      test_link_resume_interfaces();
      s_bMustResumeTestInterfaces = false;
   }

   if ( ! test_link_is_in_progress() )
      log_line("[TestLink-%d] Finished completly.", (int)s_uTestLinkRunNumber);

   if ( ! s_bTestLinkParamsInProgress )
      return;

   if ( s_iTestLinkState == TEST_LINK_STATE_SEND_START_TO_VEHICLE )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] No start confirmation received from vehicle. Ending flow.", (int)s_uTestLinkRunNumber);
         test_link_switch_to_end_flow_state(false);
         return;
      }
      if ( g_TimeNow > s_uTimeLastTestPacketSent + 90 )
      {
         s_uTestMsgSentCount++;
         s_uTimeLastTestPacketSent = g_TimeNow;
         packets_queue_add_packet(&s_QueueRadioPackets, s_uPacketStartTestLink);
         log_line("[TestLink-%d] Sent start message (repeat %u) to vehicle", (int)s_uTestLinkRunNumber, s_uTestMsgSentCount);
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_PING )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] No ping confirmation received from vehicle. Ending flow.", (int)s_uTestLinkRunNumber);
         test_link_switch_to_end_flow_state(false);
         return;       
      }
      if ( g_TimeNow > s_uTimeLastTestPacketSent + 87 )
      {
         s_uTimeLastTestPacketSent = g_TimeNow;
         _test_link_send_ping_to_vehicle();
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_ENDED )
   {
       if ( g_TimeNow > s_uTimeEndCurrentStep )
       {
          log_line("[TestLink-%d] No end confirmation received from vehicle. Ending flow.", (int)s_uTestLinkRunNumber);

          if ( ! s_bTestLinkParamsSucceded )
          if ( s_uCountPingSent >= TEST_LINK_SUCCESS_PING_COUNTS )
          if ( s_uCountPingReceived >= TEST_LINK_SUCCESS_PING_COUNTS )
          if ( s_uCountPingsReceivedByVehicle >= TEST_LINK_SUCCESS_PING_COUNTS )
          {
             log_line("[TestLink-%d] Ping phase succeded, so mark the test as succeeded.", (int)s_uTestLinkRunNumber);
             s_bTestLinkParamsSucceded = true;
          }
          log_line("[TestLink-%d] Test link has ended with timeout. Succeeded? %s", (int)s_uTestLinkRunNumber, (s_bTestLinkParamsSucceded?"Yes":"No"));
          
          s_iTestLinkState = 0;
          s_bTestLinkParamsInProgress = false;

          if ( s_bTestLinkParamsSucceded )
          {
             saveControllerModel(g_pCurrentModel);
             test_link_send_end_message_to_central(s_bTestLinkParamsSucceded);
             return;
          }

          log_line("[TestLink-%d] Revert radio link %d paramters to original values.", (int)s_uTestLinkRunNumber, s_iTestLinkIndex+1);
          memcpy(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsOriginal, sizeof(type_radio_links_parameters));
          
          s_bMustResumeTestInterfaces = true;
          test_link_pause_interfaces();
          if ( ! s_bTestLinkOnlyFreqChanged )
          {
             s_bMustReopenTestInterfaces = true;
             test_link_close_interfaces();
          }
          s_bApplyRadioParamsInProgress = true;
          memcpy(&s_RadioLinkParamsToChangeFrom, &s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));
          memcpy(&s_RadioLinkParamsToChangeTo, &s_RadioLinksParamsOriginal, sizeof(type_radio_links_parameters));

          if ( 0 != pthread_create(&s_pThreadTestLinkWorker, NULL, &_thread_test_link_worker, NULL) )
          {
             radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinksParamsToTest, &s_RadioLinksParamsOriginal);
             if ( ! s_bTestLinkOnlyFreqChanged )
                test_link_reopen_interfaces();
             test_link_resume_interfaces();
             s_bApplyRadioParamsInProgress = false;
             s_bTestLinkParamsInProgress = false;
             s_bMustReopenTestInterfaces = false;
             s_bMustResumeTestInterfaces = false;
             s_iTestLinkState = 0;
             test_link_send_end_message_to_central(s_bTestLinkParamsSucceded);
          }
          return;
       }
       if ( g_TimeNow > s_uTimeLastTestPacketSent + 91 )
       {
          s_uTestMsgSentCount++;
          s_uTimeLastTestPacketSent = g_TimeNow;
          _test_link_send_end_message_to_vehicle(s_bTestLinkParamsSucceded);
      }
      return;
   }
}
