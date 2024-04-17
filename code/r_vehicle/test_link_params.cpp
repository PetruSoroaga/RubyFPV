/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
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
#include "../base/models_list.h"
#include "../base/ruby_ipc.h"
#include "../radio/radiopackets2.h"
#include "../radio/radio_rx.h"
#include "../radio/radio_tx.h"
#include "../common/string_utils.h"
#include "shared_vars.h"
#include "timers.h"
#include "radio_links.h"

#define TEST_LINK_STATE_SEND_START_TO_CONTROLLER 1
#define TEST_LINK_STATE_PING 2
#define TEST_LINK_STATE_ENDED 10
#define TEST_LINK_SUCCESS_PING_COUNTS 2

int s_iTestLinkState = 0;
int s_iTestLinkIndex = -1;
int s_iTestLinkCountDifferences = 0;
bool s_bTestLinkParamsInProgress = false;
bool s_bTestLinkParamsSucceded = false;
bool s_bTestLinkOnlyFreqChanged = false;
bool s_bApplyRadioParamsInProgress = false;
bool s_bTestLinkSwitchedToNewParams = false;
bool s_bTestLinkSwitchedToOldParams = false;
u32 s_uTimeStartTestLink = 0;
u32 s_uTimeLastTestPacketSent = 0;
u32 s_uTimeEndCurrentStep = 0;
u32 s_uCountPingSent = 0;
u32 s_uCountPingReceived = 0;
u32 s_uCountPingsSentByController = 0;
u32 s_uCountPingsReceivedByController = 0;

type_radio_links_parameters s_RadioLinksParamsToTest;
type_radio_links_parameters s_RadioLinksParamsOriginal;

u8 s_uPacketStartTestLink[MAX_PACKET_TOTAL_SIZE];

pthread_t s_pThreadTestLinkWorker;
type_radio_links_parameters s_RadioLinkParamsToChangeFrom;
type_radio_links_parameters s_RadioLinkParamsToChangeTo;
bool s_bTestLinkPausedInterfaces = false;
bool s_bMustResumeRadioInterfacesForRead[MAX_RADIO_INTERFACES];
bool s_bMustResumeRadioInterfacesForWrite[MAX_RADIO_INTERFACES];
bool s_bMustReopenTestInterfaces = false;
bool s_bMustResumeTestInterfaces = false;
bool s_bMustReopenRadioInterfacesForRead[MAX_RADIO_INTERFACES];
bool s_bMustReopenRadioInterfacesForWrite[MAX_RADIO_INTERFACES];

u8 s_uTestLinkRunNumber = 0;

bool test_link_is_in_progress()
{
   return (s_bTestLinkParamsInProgress || s_bApplyRadioParamsInProgress || s_bMustReopenTestInterfaces || s_bMustResumeTestInterfaces);
}

void test_link_state_get_state_string(int iState, char* szBuffer)
{
   if ( NULL == szBuffer )
      return;
   strcpy(szBuffer, "N/A");
   if ( iState == TEST_LINK_STATE_SEND_START_TO_CONTROLLER )
      strcpy(szBuffer, "Send_Start_To_Controller");
   if ( iState == TEST_LINK_STATE_PING )
      strcpy(szBuffer, "Send_Ping_To_Controller");
   if ( iState == TEST_LINK_STATE_ENDED )
      strcpy(szBuffer, "Send_End_To_Controller");
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
      log_line("[TestLink-%d] Vehicle sent %u pings, received %u pings from controller, controller sent %u pings, controller received %u pings from vehicle.",
         (int)s_uTestLinkRunNumber, s_uCountPingSent, s_uCountPingReceived, s_uCountPingsSentByController, s_uCountPingsReceivedByController );
   }
}

void test_link_reset_state()
{
   s_bTestLinkParamsInProgress = false;
   s_bTestLinkParamsSucceded = false;
   s_bTestLinkSwitchedToNewParams = false;
   s_bTestLinkSwitchedToOldParams = false;
   s_bTestLinkPausedInterfaces = false;
   s_bTestLinkOnlyFreqChanged = false;
   s_iTestLinkState = 0;
   s_uTimeStartTestLink = 0;
   s_uTimeLastTestPacketSent = 0;
   s_uTimeEndCurrentStep = 0;
   s_uCountPingSent = 0;
   s_uCountPingReceived = 0;
   s_uCountPingsSentByController = 0;
   s_uCountPingsReceivedByController = 0;

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
   log_line("[TestLink-%d] Reopen radio interfaces for vehicle radio link %d...", (int)s_uTestLinkRunNumber, s_iTestLinkIndex+1);
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
         int iRes = 0;
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[s_iTestLinkIndex] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
            iRes = radio_open_interface_for_read(i, RADIO_PORT_ROUTER_DOWNLINK);
         else
            iRes = radio_open_interface_for_read(i, RADIO_PORT_ROUTER_UPLINK);

         if ( iRes > 0 )
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

u8* _test_link_generate_start_packet(u32 uControllerId, u32 uVehicleId, int iLinkId, type_radio_links_parameters* pRadioParamsToTest)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = uVehicleId;
   PH.vehicle_id_dest = uControllerId;
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(type_radio_links_parameters);

   memcpy(s_uPacketStartTestLink, (u8*)&PH, sizeof(t_packet_header));
   s_uPacketStartTestLink[sizeof(t_packet_header)] = (u8)iLinkId;
   s_uPacketStartTestLink[sizeof(t_packet_header)+1] = (u8)s_uTestLinkRunNumber;
   s_uPacketStartTestLink[sizeof(t_packet_header)+2] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(s_uPacketStartTestLink + sizeof(t_packet_header) + 3*sizeof(u8), pRadioParamsToTest, sizeof(type_radio_links_parameters));
   return s_uPacketStartTestLink;
}

void _test_link_send_ping_to_controller()
{
   s_uCountPingSent++;
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + 2*sizeof(u32);

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = (u8)s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+1] = (u8)s_uTestLinkRunNumber;
   buffer[sizeof(t_packet_header)+2] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING;
   memcpy(buffer + sizeof(t_packet_header) + 3*sizeof(u8), &s_uCountPingSent, sizeof(u32));
   memcpy(buffer + sizeof(t_packet_header) + 3*sizeof(u8) + sizeof(u32), &s_uCountPingReceived, sizeof(u32));
   packets_queue_add_packet(&g_QueueRadioPacketsOut, buffer);
   log_line("[TestLink-%d] Sent ping %d to controller", (int)s_uTestLinkRunNumber, s_uCountPingSent);
}

void _test_link_send_end_message()
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = g_pCurrentModel->uVehicleId;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + 4*sizeof(u8);

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = (u8)s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+1] = (u8)s_uTestLinkRunNumber;
   buffer[sizeof(t_packet_header)+2] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END;
   buffer[sizeof(t_packet_header)+3] = (u8) (s_bTestLinkParamsSucceded?1:0);
   packets_queue_add_packet(&g_QueueRadioPacketsOut, buffer);
   log_line("[TestLink-%d] Sent end message (succeeded? %s) to controller", (int)s_uTestLinkRunNumber, s_bTestLinkParamsSucceded?"yes":"no");
}

void test_link_switch_to_end_flow(bool bSucceeded)
{
   if ( 0 == s_iTestLinkState )
   {
      log_line("[TestLink-%d] Ended without executing. Succeeded? %s", (int)s_uTestLinkRunNumber, (bSucceeded?"Yes":"No"));
      s_bTestLinkParamsSucceded = bSucceeded;
      s_bTestLinkParamsInProgress = false;
      return;
   }

   test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
   log_line("[TestLink-%d] Switched to state end flow. Succeeded? %s", (int)s_uTestLinkRunNumber, (bSucceeded?"Yes":"No"));

   s_bTestLinkParamsSucceded = bSucceeded;
   s_uTimeLastTestPacketSent = 0;
}

void _test_link_on_final_end()
{
   log_line("[TestLink-%d] Test link has ended completly on vehicle. Succeeded? %s", (int)s_uTestLinkRunNumber, (s_bTestLinkParamsSucceded?"Yes":"No"));
   s_bApplyRadioParamsInProgress = false;

   if ( s_bTestLinkParamsSucceded )
   {
      saveCurrentModel();

      t_packet_header PH;
      radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, STREAM_ID_DATA);
      PH.vehicle_id_src = PACKET_COMPONENT_RUBY | (MODEL_CHANGED_RADIO_LINK_PARAMS<<8) | (((u32)s_iTestLinkIndex)<<16);
      PH.total_length = sizeof(t_packet_header);

      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastActiveTime = get_current_timestamp_ms();

      ruby_ipc_channel_send_message(s_fIPCRouterToTelemetry, (u8*)&PH, PH.total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToRC, (u8*)&PH, PH.total_length);
      ruby_ipc_channel_send_message(s_fIPCRouterToCommands, (u8*)&PH, PH.total_length);
      
      if ( NULL != g_pProcessStats )
         g_pProcessStats->lastIPCOutgoingTime = g_TimeNow;

      s_bTestLinkParamsInProgress = false;
      s_iTestLinkState = 0;
      return;
   }


   log_line("[TestLink-%d] Revert radio link %d paramters to original values.", (int)s_uTestLinkRunNumber, s_iTestLinkIndex+1);
   memcpy(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsOriginal, sizeof(type_radio_links_parameters));

   if ( ! s_bTestLinkSwitchedToOldParams )
   {
      s_bTestLinkSwitchedToOldParams = true;
      // Pause the impacted local radio interfaces
      test_link_pause_interfaces();
      test_link_close_interfaces();

      s_bMustReopenTestInterfaces = true;
      s_bMustResumeTestInterfaces = true;
      s_bApplyRadioParamsInProgress = true;
      memcpy(&s_RadioLinkParamsToChangeFrom, &s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));
      memcpy(&s_RadioLinkParamsToChangeTo, &s_RadioLinksParamsOriginal, sizeof(type_radio_links_parameters));

      if ( 0 != pthread_create(&s_pThreadTestLinkWorker, NULL, &_thread_test_link_worker, NULL) )
      {
         radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinksParamsToTest, &s_RadioLinksParamsOriginal);
         test_link_reopen_interfaces();
         test_link_resume_interfaces();
         s_bApplyRadioParamsInProgress = false;
         s_bTestLinkParamsInProgress = false;
         s_bMustReopenTestInterfaces = false;
         s_bMustResumeTestInterfaces = false;
      }
   }
  
   s_bTestLinkParamsInProgress = false;
   s_iTestLinkState = 0;
}

void test_link_process_received_message(int iInterfaceIndex, u8* pPacketBuffer)
{
   if ( NULL == pPacketBuffer )
   {
      log_softerror_and_alarm("[TestLink-%d] Process received message from controller on radio interface %d but message is null", (int)s_uTestLinkRunNumber, iInterfaceIndex+1);
      return;
   }

   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   if ( (pPH->packet_type != PACKET_TYPE_TEST_RADIO_LINK) || (pPH->total_length < sizeof(t_packet_header)+2) )
   {
      log_softerror_and_alarm("[TestLink-%d] Process received message from controller on radio interface %d but message is of type test radio link", (int)s_uTestLinkRunNumber, iInterfaceIndex+1);
      return;
   }

   int iRadioLinkId = pPacketBuffer[sizeof(t_packet_header)];
   int iMsgType = pPacketBuffer[sizeof(t_packet_header)+2];
   int iMsgLen = pPH->total_length - sizeof(t_packet_header)-3*sizeof(u8);

   char szBuff[128];
   test_link_state_get_state_string(s_iTestLinkState, szBuff);

   // Start message

   if ( (iMsgType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START) && (s_iTestLinkState == 0) && (! test_link_is_in_progress()) )
   {
      log_line("[TestLink-%d] Processing received test link message from CID %u, msg type: %s, test link state is: %s, %d data bytes on radio interface %d for radio link %d",
         (int)s_uTestLinkRunNumber, pPH->vehicle_id_src, str_get_packet_test_link_command(iMsgType), 
         szBuff, iMsgLen,
         iInterfaceIndex+1, iRadioLinkId+1);
   
      s_uTestLinkRunNumber = pPacketBuffer[sizeof(t_packet_header)+1];

      memcpy(&s_RadioLinksParamsToTest, (pPacketBuffer + sizeof(t_packet_header)+3*sizeof(u8)), sizeof(type_radio_links_parameters));

      log_line("[TestLink-%d] Received message to start testing radio link %d", (int)s_uTestLinkRunNumber, iRadioLinkId+1);
      
      s_iTestLinkCountDifferences = g_pCurrentModel->logVehicleRadioLinkDifferences(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsToTest);
      s_iTestLinkIndex = iRadioLinkId;
      
      memcpy(&s_RadioLinksParamsOriginal, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
      memcpy(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsToTest, sizeof(type_radio_links_parameters));

      _test_link_generate_start_packet(g_uControllerId, g_pCurrentModel->uVehicleId, iRadioLinkId, &s_RadioLinksParamsToTest);

      test_link_reset_state();

      if ( (1 == s_iTestLinkCountDifferences) && (s_RadioLinksParamsOriginal.link_frequency_khz[s_iTestLinkIndex] != s_RadioLinksParamsToTest.link_frequency_khz[s_iTestLinkIndex]) )
      {
         log_line("[TestLink-%d] Only frequency must be changed for this test.", (int)s_uTestLinkRunNumber);
         s_bTestLinkOnlyFreqChanged = true;
      }
      test_link_switch_to_state(TEST_LINK_STATE_SEND_START_TO_CONTROLLER, TIMEOUT_TEST_LINK_STATE_START);
      s_bTestLinkParamsInProgress = true;
      s_uTimeStartTestLink = g_TimeNow;
      s_uTimeLastTestPacketSent = g_TimeNow;

      log_line("[TestLink-%d] Pending radio outgoing messages count before sending response to vehicle: %d", (int)s_uTestLinkRunNumber, packets_queue_has_packets(&g_QueueRadioPacketsOut));
      // For frequency change only, pause radio interfaces and apply new params after start command confirmation is sent to controller or ping is received from controller
      if ( s_bTestLinkOnlyFreqChanged )
      {
         for( int i=0; i<5; i++ )
            packets_queue_add_packet(&g_QueueRadioPacketsOut, s_uPacketStartTestLink);
      }
      else
      {
         s_bTestLinkSwitchedToNewParams = true;
         test_link_pause_interfaces();
         test_link_close_interfaces();
         s_bMustReopenTestInterfaces = true;
         s_bMustResumeTestInterfaces = true;
       
         log_line("[TestLink-%d] Apply new parameters for radio link %d", (int)s_uTestLinkRunNumber, iRadioLinkId+1);

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
      }
      packets_queue_add_packet(&g_QueueRadioPacketsOut, s_uPacketStartTestLink);
      log_line("[TestLink-%d] Sent start message response to controller", (int)s_uTestLinkRunNumber);
      log_line("[TestLink-%d] Pending radio outgoing messages count again before sending response to vehicle: %d", (int)s_uTestLinkRunNumber, packets_queue_has_packets(&g_QueueRadioPacketsOut));
      return;
   }

   // Ping message

   if ( iMsgType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING )
   {
      if ( s_iTestLinkState == TEST_LINK_STATE_SEND_START_TO_CONTROLLER )
      {
         log_line("[TestLink-%d] Processing received test link message from CID %u, msg type: %s, test link state is: %s, %d data bytes on radio interface %d for radio link %d",
            (int)s_uTestLinkRunNumber, pPH->vehicle_id_src, str_get_packet_test_link_command(iMsgType), 
            szBuff, iMsgLen,
            iInterfaceIndex+1, iRadioLinkId+1);

         test_link_switch_to_state(TEST_LINK_STATE_PING, TIMEOUT_TEST_LINK_STATE_PING);
      
         s_uCountPingSent = 0;
         s_uCountPingReceived = 0;
         s_uCountPingsSentByController = 0;
         s_uCountPingsReceivedByController = 0;
         // follow through down as this is the first ping
      }
      if ( s_iTestLinkState == TEST_LINK_STATE_PING )
      {
         log_line("[TestLink-%d] Processing received test link message from CID %u, msg type: %s, test link state is: %s, %d data bytes on radio interface %d for radio link %d",
            (int)s_uTestLinkRunNumber, pPH->vehicle_id_src, str_get_packet_test_link_command(iMsgType), 
            szBuff, iMsgLen,
            iInterfaceIndex+1, iRadioLinkId+1);
         s_uCountPingReceived++;
         memcpy(&s_uCountPingsSentByController, pPacketBuffer + sizeof(t_packet_header) + 3*sizeof(u8), sizeof(u32));
         memcpy(&s_uCountPingsReceivedByController, pPacketBuffer + sizeof(t_packet_header) + 3*sizeof(u8)+sizeof(u32), sizeof(u32));
         log_line("[TestLink-%d] Received test ping nb %u from controller. Controller sent %u pings, controller received %u pings so far, we sent %u pings.",
             (int)s_uTestLinkRunNumber, s_uCountPingReceived, s_uCountPingsSentByController, s_uCountPingsReceivedByController, s_uCountPingSent);
         if ( s_uCountPingSent >= TEST_LINK_SUCCESS_PING_COUNTS )
         if ( s_uCountPingReceived >= TEST_LINK_SUCCESS_PING_COUNTS )
         if ( s_uCountPingsSentByController >= TEST_LINK_SUCCESS_PING_COUNTS )
         if ( s_uCountPingsReceivedByController >= TEST_LINK_SUCCESS_PING_COUNTS )
            s_bTestLinkParamsSucceded = true;
   
         if ( 0 == s_uCountPingSent )
         {
            s_uTimeLastTestPacketSent = g_TimeNow;
            _test_link_send_ping_to_controller();
         }
         return;
      }
   }

   // End message

   if ( iMsgType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END )
   {
       if ( (s_iTestLinkState == TEST_LINK_STATE_PING) || (s_iTestLinkState == TEST_LINK_STATE_SEND_START_TO_CONTROLLER) )
       {
         log_line("[TestLink-%d] Processing received test link message from CID %u, msg type: %s, test link state is: %s, %d data bytes on radio interface %d for radio link %d",
            (int)s_uTestLinkRunNumber, pPH->vehicle_id_src, str_get_packet_test_link_command(iMsgType), 
            szBuff, iMsgLen,
            iInterfaceIndex+1, iRadioLinkId+1);
          bool bSuccedOnController = (pPacketBuffer[sizeof(t_packet_header)+3*sizeof(u8)]?true:false);
          if ( bSuccedOnController )
          if ( s_uCountPingSent >= TEST_LINK_SUCCESS_PING_COUNTS )
          if ( s_uCountPingReceived >= TEST_LINK_SUCCESS_PING_COUNTS )
          if ( s_uCountPingsSentByController >= TEST_LINK_SUCCESS_PING_COUNTS )
          if ( s_uCountPingsReceivedByController >= TEST_LINK_SUCCESS_PING_COUNTS )
             s_bTestLinkParamsSucceded = true;

          // If we received end message from controller, if it succeeded there it succeeded here too even if we got no pings as we got end message, so upling is working.
          if ( bSuccedOnController )
          if ( s_uCountPingSent >= TEST_LINK_SUCCESS_PING_COUNTS )
          if ( g_TimeNow < s_uTimeEndCurrentStep-500 )
          {
             log_line("[TestLink-%d] We got END from controller but no PINGs from controller (only %d). Uplink is still working fine. Mark test as success.", (int)s_uTestLinkRunNumber, s_uCountPingReceived);
             s_bTestLinkParamsSucceded = true;
          }

          log_line("[TestLink-%d] Test link has ended on controller. Succeeded on controller? %s. Succeeded locally? %s",
             (int)s_uTestLinkRunNumber, 
             (bSuccedOnController?"Yes":"No"),
             (s_bTestLinkParamsSucceded?"Yes":"No"));
          log_line("[TestLink-%d] We sent %u pings, we received %u pings from controller, controller sent %u pings, controller received %u pings from us.",
             (int)s_uTestLinkRunNumber, s_uCountPingSent, s_uCountPingReceived, s_uCountPingsSentByController, s_uCountPingsReceivedByController);

          test_link_switch_to_state(TEST_LINK_STATE_ENDED, TIMEOUT_TEST_LINK_STATE_END);
  
          s_uTimeLastTestPacketSent = g_TimeNow;
          _test_link_send_end_message();
          _test_link_on_final_end();
          return;
       }
       log_line("[TestLink-%d] Processing received test link message from CID %u, msg type: %s, test link state is: %s, %d data bytes on radio interface %d for radio link %d",
          (int)s_uTestLinkRunNumber, pPH->vehicle_id_src, str_get_packet_test_link_command(iMsgType), 
          szBuff, iMsgLen,
          iInterfaceIndex+1, iRadioLinkId+1);
       _test_link_send_end_message();
       return;
   }
   log_line("[TestLink-%d] Ignored received test link message from CID %u, msg type: %s, test link state is: %s, %d data bytes for radio link %d",
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

   if ( s_iTestLinkState == TEST_LINK_STATE_SEND_START_TO_CONTROLLER )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] No progress to new state received after start from controller. Ending flow.", (int)s_uTestLinkRunNumber);
         test_link_switch_to_end_flow(false);
         return;
      }

      bool bSendResponse = false;
      if ( g_TimeNow >= s_uTimeLastTestPacketSent + 100 )
         bSendResponse = true;
      if ( s_bTestLinkOnlyFreqChanged )
      if ( g_TimeNow >= s_uTimeLastTestPacketSent + 50 )
         bSendResponse = true;

      if ( bSendResponse )
      {
         s_uTimeLastTestPacketSent = g_TimeNow;
         packets_queue_add_packet(&g_QueueRadioPacketsOut, s_uPacketStartTestLink);
      }

      if ( s_bTestLinkSwitchedToNewParams )
         return;

      bool bUpdateLinkNow = false;
      int iCountPending = packets_queue_has_packets(&g_QueueRadioPacketsOut);
      if ( g_TimeNow > s_uTimeStartTestLink + 200 )
      if ( iCountPending < 3 )
         bUpdateLinkNow = true;
      if ( g_TimeNow > s_uTimeStartTestLink + 700 )
         bUpdateLinkNow = true;

      if ( bUpdateLinkNow )
      {
         log_line("[TestLink-%d] Pending radio outgoing messages count before updating radio link params: %d", (int)s_uTestLinkRunNumber, iCountPending);

         s_bTestLinkSwitchedToNewParams = true;
         // For frequency change only , pause radio interfaces and apply new params after start command confirmation is sent to controller
         if ( s_bTestLinkOnlyFreqChanged )
         {
            test_link_pause_interfaces();
            s_bMustResumeTestInterfaces = true;
                     
            log_line("[TestLink-%d] Apply new parameters for radio link %d", (int)s_uTestLinkRunNumber, s_iTestLinkIndex+1);

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
      }
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_PING )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink-%d] No progress to new state received after ping state timed out. Ending flow.", (int)s_uTestLinkRunNumber);
         if ( s_uCountPingSent >= TEST_LINK_SUCCESS_PING_COUNTS )
         if ( s_uCountPingReceived >= TEST_LINK_SUCCESS_PING_COUNTS )
         if ( s_uCountPingsSentByController >= TEST_LINK_SUCCESS_PING_COUNTS )
         if ( s_uCountPingsReceivedByController >= TEST_LINK_SUCCESS_PING_COUNTS-1 )
            s_bTestLinkParamsSucceded = true;
         test_link_switch_to_end_flow(s_bTestLinkParamsSucceded);
         return;       
      }
      if ( g_TimeNow > s_uTimeLastTestPacketSent + 80 )
      {
         s_uTimeLastTestPacketSent = g_TimeNow;
         _test_link_send_ping_to_controller();
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_ENDED )
   {
       if ( g_TimeNow > s_uTimeEndCurrentStep )
       {
          log_line("[TestLink-%d] No progress to new state received after end state timed out. Ending flow completly.", (int)s_uTestLinkRunNumber);
          _test_link_on_final_end();
          return;
       }
       if ( g_TimeNow > s_uTimeLastTestPacketSent + 100 )
       {
          s_uTimeLastTestPacketSent = g_TimeNow;
          _test_link_send_end_message();
      }
      return;
   }
}
