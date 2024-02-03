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

extern t_packet_queue s_QueueRadioPackets;

int s_iTestLinkState = 0;
int s_iTestLinkIndex = -1;
bool s_bTestLinkParamsInProgress = false;
bool s_bTestLinkParamsSucceded = false;
bool s_bApplyRadioParamsInProgress = false;
u32 s_uTimeStartTestLink = 0;
u32 s_uTimeLastTestPacketSent = 0;
u32 s_uTimeEndCurrentStep = 0;
u32 s_uTestMsgSentCount = 0;
u32 s_uCountPingSent = 0;
u32 s_uCountPingReceived = 0;
type_radio_links_parameters s_RadioLinkParamsToTest;
type_radio_links_parameters s_RadioLinksParamsOriginal;

u8 s_uPacketStartTestLink[MAX_PACKET_TOTAL_SIZE];
u8 s_uPacketToSend[MAX_PACKET_TOTAL_SIZE];

pthread_t s_pThreadTestLinkWorker;
type_radio_links_parameters s_RadioLinkParamsToChangeFrom;
type_radio_links_parameters s_RadioLinkParamsToChangeTo;
bool s_bMustReopenTestInterfaces = false;
bool s_bMustReopenRadioInterfacesForRead[MAX_RADIO_INTERFACES];
bool s_bMustReopenRadioInterfacesForWrite[MAX_RADIO_INTERFACES];

bool test_link_is_in_progress()
{
   return s_bTestLinkParamsInProgress;
}

int test_link_active()
{
   return s_iTestLinkIndex;
}

static void * _thread_test_link_worker(void *argument)
{
   log_line("[TestLink] Started worker thread to update radio interfaces for vehicle radio link %d.", s_iTestLinkIndex+1);
   radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinkParamsToChangeFrom, &s_RadioLinkParamsToChangeTo);
   log_line("[TestLink] Finished worker thread to update radio interfaces for vehicle radio link %d.", s_iTestLinkIndex+1);
   s_bApplyRadioParamsInProgress = false;
   return NULL;
}


void test_link_reopen_interfaces()
{
   log_line("[TestLink] Reopen radio interfaces for vehicle radio link %d...", s_iTestLinkIndex+1);
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
         {
            g_SM_RadioStats.radio_interfaces[i].openedForRead = 1;
            radio_rx_resume_interface(i);
         }
      }

      if (s_bMustReopenRadioInterfacesForWrite[i] )
      {
         s_bMustReopenRadioInterfacesForWrite[i] = false;
         if ( radio_open_interface_for_write(i) > 0 )
         {
            g_SM_RadioStats.radio_interfaces[i].openedForWrite = 1;
            radio_tx_resume_radio_interface(i);
         }
      }
   }
}

u8* test_link_generate_start_packet(u32 uVehicleId, int iLinkId, type_radio_links_parameters* pRadioParamsToTest)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = uVehicleId;
   PH.total_length = sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(type_radio_links_parameters);

   memcpy(s_uPacketStartTestLink, (u8*)&PH, sizeof(t_packet_header));
   s_uPacketStartTestLink[sizeof(t_packet_header)] = (u8)iLinkId;
   s_uPacketStartTestLink[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(s_uPacketStartTestLink + sizeof(t_packet_header) + 2*sizeof(u8), pRadioParamsToTest, sizeof(type_radio_links_parameters));
   return s_uPacketStartTestLink;
}

void test_link_send_status_message_to_central(const char* szMsg)
{
   if ( (NULL == szMsg) || (0 == szMsg[0]) )
      return;

   log_line("[TestLink] Send status message to central: [%s]", szMsg);

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8) + strlen(szMsg);

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = (u8) s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_STATUS;
   buffer[sizeof(t_packet_header)+2] = (u8) strlen(szMsg)+1;
   memcpy( &(buffer[sizeof(t_packet_header)+3]), szMsg, strlen(szMsg)+1);
   if ( ! ruby_ipc_channel_send_message(g_fIPCToCentral, buffer, PH.total_length) )
       log_softerror_and_alarm("No pipe to central to send message to.");
}

void test_link_send_end_message_to_central(bool bSucceeded)
{
   log_line("[TestLink] Send end message to central. Succeeded? %s", bSucceeded?"Yes":"No");

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_uControllerId;
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8);

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = (u8) s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END;
   buffer[sizeof(t_packet_header)+2] = (u8) (bSucceeded?1:0);
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
   PH.total_length = sizeof(t_packet_header) + 2*sizeof(u8) + 2*sizeof(u32);

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = (u8)s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING;
   memcpy(buffer + sizeof(t_packet_header) + 2*sizeof(u8), &s_uCountPingSent, sizeof(u32));
   memcpy(buffer + sizeof(t_packet_header) + 2*sizeof(u8) + sizeof(u32), &s_uCountPingReceived, sizeof(u32));
   packets_queue_add_packet(&s_QueueRadioPackets, buffer);
   log_line("[TestLink] Sent ping %d to vehicle", s_uCountPingSent);
}

void _test_link_send_end_message_to_vehicle(bool bSucceeded)
{
   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_RUBY, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = g_uControllerId;
   PH.vehicle_id_dest = g_pCurrentModel->vehicle_id;
   PH.total_length = sizeof(t_packet_header) + 3*sizeof(u8);

   u8 buffer[1024];
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = (u8)s_iTestLinkIndex;
   buffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END;
   buffer[sizeof(t_packet_header)+2] = (u8) (bSucceeded?1:0);
   packets_queue_add_packet(&s_QueueRadioPackets, buffer);
   log_line("[TestLink] Sent end message (repeat %d) to vehicle", s_uTestMsgSentCount);
}

bool test_link_start(u32 uControllerId, u32 uVehicleId, int iLinkId, type_radio_links_parameters* pRadioParamsToTest)
{
   log_line("[TestLink] Start test flow. CID: %u, VID: %u, radio link %d", uControllerId, uVehicleId, iLinkId+1);

   s_iTestLinkIndex = iLinkId;
   memcpy(&s_RadioLinkParamsToTest, pRadioParamsToTest, sizeof(type_radio_links_parameters));

   g_pCurrentModel->logVehicleRadioLinkDifferences(&(g_pCurrentModel->radioLinksParams), &s_RadioLinkParamsToTest);

   memcpy(&s_RadioLinksParamsOriginal, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   memcpy(&(g_pCurrentModel->radioLinksParams), &s_RadioLinkParamsToTest, sizeof(type_radio_links_parameters));
   
   s_bTestLinkParamsInProgress = true;
   s_bTestLinkParamsSucceded = false;
   s_iTestLinkState = TEST_LINK_STATE_SEND_START_TO_VEHICLE;

   s_uCountPingSent = 0;
   s_uCountPingReceived = 0;
   s_uTimeEndCurrentStep = g_TimeNow + TIMEOUT_TEST_LINK_STATE_START;
   s_uTimeLastTestPacketSent = g_TimeNow;
   s_uTestMsgSentCount = 0;

   test_link_generate_start_packet(uVehicleId, iLinkId, pRadioParamsToTest);
   
   t_packet_header* pPH = (t_packet_header*)s_uPacketStartTestLink;
   pPH->vehicle_id_src = uControllerId;
   pPH->vehicle_id_dest = uVehicleId;

   // Pause the impacted local radio interfaces

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
         radio_tx_pause_radio_interface(i);
         radio_close_interface_for_write(i);
         g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
         s_bMustReopenRadioInterfacesForWrite[i] = true;
      }
      if ( pRadioHWInfo->openedForRead )
      {
         radio_rx_pause_interface(i);
         radio_close_interface_for_read(i);
         g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
         s_bMustReopenRadioInterfacesForRead[i] = true;
      }

      if ( ((pRadioHWInfo->typeAndDriver & 0xFF) == RADIO_TYPE_ATHEROS) ||
        ((pRadioHWInfo->typeAndDriver & 0xFF) == RADIO_TYPE_RALINK) )
         s_uTimeEndCurrentStep = g_TimeNow + 2*TIMEOUT_TEST_LINK_STATE_START;
   }

   log_line("[TestLink] Apply new parameters for radio link %d", s_iTestLinkIndex+1);
   s_bApplyRadioParamsInProgress = true;
   memcpy(&s_RadioLinkParamsToChangeTo, &s_RadioLinkParamsToTest, sizeof(type_radio_links_parameters));
   memcpy(&s_RadioLinkParamsToChangeFrom, &s_RadioLinksParamsOriginal, sizeof(type_radio_links_parameters));

   if ( 0 != pthread_create(&s_pThreadTestLinkWorker, NULL, &_thread_test_link_worker, NULL) )
   {
      radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinksParamsOriginal, &s_RadioLinkParamsToTest);
      test_link_reopen_interfaces();
      s_bApplyRadioParamsInProgress = false;
      s_bMustReopenTestInterfaces = false;
   }
   else
      s_bMustReopenTestInterfaces = true; 
   //packets_queue_add_packet(&s_QueueRadioPackets, s_uPacketStartTestLink);
   //log_line("[TestLink] Sent start message (repeat %u) to vehicle", s_uTestMsgSentCount);
   return true;
}

void test_link_end_flow(bool bSucceeded)
{
   if ( 0 == s_iTestLinkState )
   {
      log_line("[TestLink] Ended without executing. Succeeded? %s", bSucceeded?"Yes":"No");
      s_bTestLinkParamsSucceded = bSucceeded;
      s_bTestLinkParamsInProgress = false;
      return;
   }

   log_line("[TestLink] Switched to state end flow. Succeeded? %s", bSucceeded?"Yes":"No");

   s_iTestLinkState = TEST_LINK_STATE_ENDED;
   s_bTestLinkParamsSucceded = bSucceeded;
   s_uTimeLastTestPacketSent = 0;
   s_uTestMsgSentCount = 0;

   s_uTimeEndCurrentStep = g_TimeNow + TIMEOUT_TEST_LINK_STATE_END;
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
      log_softerror_and_alarm("[TestLink] Process received message from vehicle on radio interface %d but message is null", iInterfaceIndex+1);
      return;
   }
   t_packet_header* pPH = (t_packet_header*)pPacketBuffer;
   if ( (pPH->packet_type != PACKET_TYPE_TEST_RADIO_LINK) || (pPH->total_length < sizeof(t_packet_header)+2) )
   {
      log_softerror_and_alarm("[TestLink] Process received message from vehicle on radio interface %d but message is invalid.", iInterfaceIndex+1);
      return;
   }
   int iRadioLinkId = pPacketBuffer[sizeof(t_packet_header)];
   int iMsgType = pPacketBuffer[sizeof(t_packet_header)+1];
   int iMsgLen = pPH->total_length - sizeof(t_packet_header)-2*sizeof(u8);
   log_line("[TestLink] Processing received test link message type %s from VID %u, %d data bytes, on radio interface %d for radio link %d",
      str_get_packet_test_link_command(iMsgType), pPH->vehicle_id_src, iMsgLen,
      iInterfaceIndex+1, iRadioLinkId+1);

   // Start message

   if ( iMsgType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START )
   {
      if ( s_iTestLinkState == TEST_LINK_STATE_SEND_START_TO_VEHICLE )
      {
         test_link_send_status_message_to_central("Vehicle updated paramaters. Testing link...");
         s_iTestLinkState = TEST_LINK_STATE_PING;
         s_uCountPingSent = 0;
         s_uCountPingReceived = 0;
         s_uTimeLastTestPacketSent = g_TimeNow;
         s_uTimeEndCurrentStep = g_TimeNow + TIMEOUT_TEST_LINK_STATE_PING;
         return;
      }
   }

   // Ping message

   if ( iMsgType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_PING )
   {
      if ( s_iTestLinkState == TEST_LINK_STATE_PING )
      {
         s_uCountPingReceived++;
         u32 uCountReceivedByVehicle = 0;
         memcpy(&uCountReceivedByVehicle, pPacketBuffer + sizeof(t_packet_header) + 2*sizeof(u8)+sizeof(u32), sizeof(u32));
         log_line("[TestLink] Received test ping %u from vehicle. Vehicle received %u pings so far, we sent %u pings.", s_uCountPingReceived, uCountReceivedByVehicle, s_uCountPingSent);

         if ( uCountReceivedByVehicle > 3 )
         {
            test_link_send_status_message_to_central("Link is active");
            test_link_end_flow(true);
         }
         return;
      }
   }

   // End message
   
   if ( iMsgType == PACKET_TYPE_TEST_RADIO_LINK_COMMAND_END )
   {
       if ( s_iTestLinkState == TEST_LINK_STATE_ENDED )
       {
          log_line("[TestLink] Test link has ended on both ends. Succeeded? %s", s_bTestLinkParamsSucceded?"Yes":"No");
          s_bTestLinkParamsInProgress = false;
          s_iTestLinkState = 0;
          saveControllerModel(g_pCurrentModel);
          test_link_send_end_message_to_central(s_bTestLinkParamsSucceded);
          return;
       }
   }
   log_line("[TestLink] Ignored received test link message from VID %u, msg type: %s, %d data bytes for radio link %d", pPH->vehicle_id_src, str_get_packet_test_link_command(iMsgType), iMsgLen, iRadioLinkId+1);
}

void test_link_loop()
{
   static u32 s_uTimeLastTestLinkLoop = 0;

   if ( g_TimeNow > s_uTimeLastTestLinkLoop + 50 )
   {
      s_uTimeLastTestLinkLoop = g_TimeNow;
      log_line("[TestLink] Loop: state: %d, time to end step: %u ms", s_iTestLinkState, s_uTimeEndCurrentStep - g_TimeNow);
   }

   if ( (0 == s_iTestLinkState) && (s_bMustReopenTestInterfaces) )
   {
      if ( ! s_bApplyRadioParamsInProgress )
      {
         test_link_reopen_interfaces();
         s_bMustReopenTestInterfaces = false;
      }
   }

   if ( ! s_bTestLinkParamsInProgress )
      return;

   if ( s_iTestLinkState == TEST_LINK_STATE_SEND_START_TO_VEHICLE )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink] No start confirmation received from vehicle. Ending flow.");
         test_link_end_flow(false);
         return;
      }
      if ( g_TimeNow > s_uTimeLastTestPacketSent + 100 )
      if ( ! s_bApplyRadioParamsInProgress )
      {
         if ( s_bMustReopenTestInterfaces )
         {
            test_link_reopen_interfaces();
            s_bMustReopenTestInterfaces = false;
         }
         s_uTestMsgSentCount++;
         s_uTimeLastTestPacketSent = g_TimeNow;
         packets_queue_add_packet(&s_QueueRadioPackets, s_uPacketStartTestLink);
         log_line("[TestLink] Sent start message (repeat %u) to vehicle", s_uTestMsgSentCount);
      }
      return;
   }

   if ( s_iTestLinkState == TEST_LINK_STATE_PING )
   {
      if ( g_TimeNow > s_uTimeEndCurrentStep )
      {
         log_line("[TestLink] No ping confirmation received from vehicle. Ending flow.");
         test_link_end_flow(false);
         return;       
      }
      if ( g_TimeNow > s_uTimeLastTestPacketSent + 100 )
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
          log_line("[TestLink] No end confirmation received from vehicle. Ending flow.");
          log_line("[TestLink] Test link has ended with timeout. Succeeded? %s", s_bTestLinkParamsSucceded?"Yes":"No");
          log_line("[TestLink] Revert radio link %d paramters to original values.", s_iTestLinkIndex+1);
          memcpy(&(g_pCurrentModel->radioLinksParams), &s_RadioLinksParamsOriginal, sizeof(type_radio_links_parameters));
          
          // Pause the impacted local radio interfaces

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
                radio_tx_pause_radio_interface(i);
                radio_close_interface_for_write(i);
                g_SM_RadioStats.radio_interfaces[i].openedForWrite = 0;
                s_bMustReopenRadioInterfacesForWrite[i] = true;
             }
             if ( pRadioHWInfo->openedForRead )
             {
                radio_rx_pause_interface(i);
                radio_close_interface_for_read(i);
                g_SM_RadioStats.radio_interfaces[i].openedForRead = 0;
                s_bMustReopenRadioInterfacesForRead[i] = true;
             }
          }

          s_bApplyRadioParamsInProgress = true;
          memcpy(&s_RadioLinkParamsToChangeFrom, &s_RadioLinkParamsToTest, sizeof(type_radio_links_parameters));
          memcpy(&s_RadioLinkParamsToChangeTo, &s_RadioLinksParamsOriginal, sizeof(type_radio_links_parameters));

          if ( 0 != pthread_create(&s_pThreadTestLinkWorker, NULL, &_thread_test_link_worker, NULL) )
          {
             radio_links_apply_settings(g_pCurrentModel, s_iTestLinkIndex, &s_RadioLinkParamsToTest, &s_RadioLinksParamsOriginal);
             test_link_reopen_interfaces();
             s_bApplyRadioParamsInProgress = false;
             s_bTestLinkParamsInProgress = false;
             s_bMustReopenTestInterfaces = false;
             s_iTestLinkState = 0;
          }
          else
             s_bMustReopenTestInterfaces = true;

          if ( ! s_bApplyRadioParamsInProgress )
          {
             if ( s_bMustReopenTestInterfaces )
             {
                test_link_reopen_interfaces();
                s_bMustReopenTestInterfaces = false;
             }

             s_bTestLinkParamsInProgress = false;
             s_iTestLinkState = 0;
             test_link_send_end_message_to_central(s_bTestLinkParamsSucceded);
          }
          return;
       }
       if ( g_TimeNow > s_uTimeLastTestPacketSent + 100 )
       {
          s_uTestMsgSentCount++;
          s_uTimeLastTestPacketSent = g_TimeNow;
          _test_link_send_end_message_to_vehicle(s_bTestLinkParamsSucceded);
      }
      return;
   }
}
