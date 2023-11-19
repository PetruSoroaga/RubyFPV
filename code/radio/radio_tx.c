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
#include "../base/hardware_radio_sik.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

#include "../common/radio_stats.h"
#include "../common/string_utils.h"
#include "radio_tx.h"
#include "radiolink.h"
#include "radio_duplicate_det.h"

typedef struct
{
    long type; // type will be the local radio interface index to use for sending the message
    char data[MAX_PACKET_TOTAL_SIZE];
} type_ipc_message_tx_packet_buffer;



int s_iRadioTxInitialized = 0;
int s_iRadioTxSingalStop = 0;
int s_iRadioTxMarkedForQuit = 0;
int s_iRadioTxIPCQueue = -1;
int s_iRadioTxSiKPacketSize = DEFAULT_SIK_PACKET_SIZE;

int s_iRadioTxInterfacesPaused[MAX_RADIO_INTERFACES];

pthread_t s_pThreadRadioTx;
pthread_mutex_t s_pThreadRadioTxMutex;

int _radio_tx_create_msg_queue()
{
   key_t key;
   key = ftok("ruby_logger", 123);

   if ( key < 0 )
   {
      log_softerror_and_alarm("[RadioTx] Failed to generate message queue key for ipc channel. Error: %d, %s",
         errno, strerror(errno));
      log_line("%d %d %d", ENOENT, EACCES, ENOTDIR);
      return 0;
   }

   s_iRadioTxIPCQueue = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

   if ( s_iRadioTxIPCQueue < 0 )
   {
      log_softerror_and_alarm("[RadioTx] Failed to create IPC message queue, error %d, %s",
         errno, strerror(errno));
      log_line("%d %d %d", ENOENT, EACCES, ENOTDIR);
      return 0;
   }

   log_line("[RadioTx] Create IPC msg queue, fd: %d", s_iRadioTxIPCQueue);
   return 1;
}

int _radio_tx_send_msg(int iInterfaceIndex, u8* pData, int iLength)
{
   // Split the packet into small SiK packets and send it
   
   t_packet_header_short PHS;
   u8 uBuffer[1000];
   int iTotalBytesSent = 0;

   for( int iPos=0; iPos < iLength; iPos += s_iRadioTxSiKPacketSize )
   {
      radio_packet_short_init(&PHS);
      
      int iShortPacketSize = s_iRadioTxSiKPacketSize;
      
      if ( iPos == 0 )
         PHS.start_header = SHORT_PACKET_START_BYTE_START_PACKET;
      
      if ( iPos + iShortPacketSize >= iLength )
      {
         iShortPacketSize = iLength - iPos;
         PHS.start_header = SHORT_PACKET_START_BYTE_END_PACKET;
      }

      PHS.packet_id = radio_packets_short_get_next_id_for_radio_interface(iInterfaceIndex);
      PHS.data_length = (u8)iShortPacketSize;
      memcpy(uBuffer, (u8*)&PHS, sizeof(t_packet_header_short));
      memcpy(&uBuffer[sizeof(t_packet_header_short)], &pData[iPos], iShortPacketSize);
      iShortPacketSize += sizeof(t_packet_header_short);
      uBuffer[1] = base_compute_crc8(&uBuffer[2], iShortPacketSize - 2);
      
      int iWriteResult = radio_write_sik_packet(iInterfaceIndex, uBuffer, iShortPacketSize, get_current_timestamp_ms());
      if ( iWriteResult != iShortPacketSize )
      {
         log_softerror_and_alarm("[RadioTx] Failed to send message to Sik radio: sent %d bytes, only %d bytes written.",
            iShortPacketSize, iWriteResult);
         if ( iWriteResult > 0 )
            iTotalBytesSent += iWriteResult;
         continue; 
      }
      iTotalBytesSent += iWriteResult;
      hardware_sleep_ms(1);
   }
   return 1;
}

static void * _thread_radio_tx(void *argument)
{
   log_line("[RadioTxThread] Started.");

   pthread_t thId = pthread_self();
   pthread_attr_t thAttr;
   int policy = 0;
   int max_prio_for_policy = 0;

   pthread_attr_init(&thAttr);
   pthread_attr_getschedpolicy(&thAttr, &policy);
   max_prio_for_policy = sched_get_priority_max(policy);

   pthread_setschedprio(thId, max_prio_for_policy);
   pthread_attr_destroy(&thAttr);

   log_line("[RadioTxThread] Initialized State. Waiting for tx messages...");

   int* piQuit = (int*) argument;
   u32 uWaitTime = 1;
   while ( 1 )
   {
      hardware_sleep_ms(uWaitTime);
      if ( uWaitTime < 50 )
         uWaitTime += 10;
      
      if ( (NULL != piQuit) && (*piQuit != 0 ) )
      {
         log_line("[RadioTxThread] Signaled to stop.");
         break;
      }
      if ( s_iRadioTxIPCQueue < 0 )
         continue;

      type_ipc_message_tx_packet_buffer ipcMessage;
      int iIPCLength = msgrcv(s_iRadioTxIPCQueue, &ipcMessage, sizeof(ipcMessage), 0, IPC_NOWAIT);
      if ( iIPCLength <= 2 )
         continue;

      uWaitTime = 1;

      if ( iIPCLength > MAX_PACKET_TOTAL_SIZE )
      {
         log_softerror_and_alarm("[RadioTx] Read IPC message too big (%d bytes), skipping it.", iIPCLength);
         continue;
      }

      if ( (ipcMessage.type < 0) || (ipcMessage.type >= hardware_get_radio_interfaces_count()) )
      {
         log_softerror_and_alarm("[RadioTx] Read IPC message for invalid radio interface %d, skipping it.", ipcMessage.type+1);
         continue;
      }

      if ( s_iRadioTxInterfacesPaused[ipcMessage.type] )
         continue;
        
      if ( ! hardware_radio_index_is_sik_radio(ipcMessage.type) )
      {
         log_softerror_and_alarm("[RadioTx] Read IPC message for radio interface %d which is not a SiK radio, skipping it.", ipcMessage.type+1);
         continue;
      }

      _radio_tx_send_msg(ipcMessage.type, (u8*)ipcMessage.data, iIPCLength);
   }

   log_line("[RadioTxThread] Stopped.");
   return NULL;
}

int radio_tx_start_tx_thread()
{
   if ( s_iRadioTxInitialized )
      return 1;

   s_iRadioTxSingalStop = 0;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      s_iRadioTxInterfacesPaused[i] = 0;

   _radio_tx_create_msg_queue();

   if ( 0 != pthread_mutex_init(&s_pThreadRadioTxMutex, NULL) )
   {
      log_error_and_alarm("[RadioTx] Failed to init mutex.");
      return 0;
   }

   if ( 0 != pthread_create(&s_pThreadRadioTx, NULL, &_thread_radio_tx, (void*)&s_iRadioTxSingalStop) )
   {
      log_error_and_alarm("[RadioTx] Failed to create thread for radio rx.");
      return 0;
   }

   s_iRadioTxInitialized = 1;
   log_line("[RadioTx] Started radio Tx thread.");
   return 1;
}


void radio_tx_stop_tx_thread()
{
   if ( ! s_iRadioTxInitialized )
      return;

   log_line("[RadioTx] Signaled radio Tx thread to stop.");
   s_iRadioTxSingalStop = 1;
   s_iRadioTxInitialized = 0;

   pthread_mutex_lock(&s_pThreadRadioTxMutex);
   pthread_mutex_unlock(&s_pThreadRadioTxMutex);

   pthread_cancel(s_pThreadRadioTx);
   pthread_mutex_destroy(&s_pThreadRadioTxMutex);

   if ( s_iRadioTxIPCQueue >= 0 )
      msgctl(s_iRadioTxIPCQueue, IPC_RMID, NULL);
   s_iRadioTxIPCQueue = -1;
}

void radio_tx_mark_quit()
{
   s_iRadioTxMarkedForQuit = 1;
}

void radio_tx_pause_radio_interface(int iRadioInterfaceIndex)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;
   s_iRadioTxInterfacesPaused[iRadioInterfaceIndex] = 1;
   log_line("[RadioTx] Pause Tx on radio interface %d", iRadioInterfaceIndex+1);
}

void radio_tx_resume_radio_interface(int iRadioInterfaceIndex)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= MAX_RADIO_INTERFACES) )
      return;
   s_iRadioTxInterfacesPaused[iRadioInterfaceIndex] = 0;
   log_line("[RadioTx] Resumed Tx on radio interface %d", iRadioInterfaceIndex+1);
}

void radio_tx_set_sik_packet_size(int iSiKPacketSize)
{
   s_iRadioTxSiKPacketSize = iSiKPacketSize;
   log_line("[RadioTx] Set SiK packet size to %d bytes", s_iRadioTxSiKPacketSize);
   if ( (s_iRadioTxSiKPacketSize < 10) || (s_iRadioTxSiKPacketSize > 250) )
   {
      s_iRadioTxSiKPacketSize = DEFAULT_SIK_PACKET_SIZE;
      log_line("[RadioTx] Set SiK packet size to %d bytes", s_iRadioTxSiKPacketSize);
   }
}

// Sends a regular radio packet to SiK radios. 
// Returns 1 for success.
int radio_tx_send_sik_packet(int iRadioInterfaceIndex, u8* pData, int iDataLength)
{
   if ( (iRadioInterfaceIndex < 0) || (iRadioInterfaceIndex >= hardware_get_radio_interfaces_count()) )
   {
      log_softerror_and_alarm("[RadioTx] Tried to write SiK packet to invalid radio interface index (%d)", iRadioInterfaceIndex);
      return -1;
   }
   
   if ( (NULL == pData) || (iDataLength < 2) )
   {
      log_softerror_and_alarm("[RadioTx] Tried to write invalid SiK packet (%d)", iDataLength);
      return -1;
   }
   
   if ( s_iRadioTxIPCQueue < 0 )
   {
      log_softerror_and_alarm("[RadioTx] Tried to write SiK packet with no IPC opened.");
      if ( ! _radio_tx_create_msg_queue() )
         return -1;    
   }

   type_ipc_message_tx_packet_buffer msg;

   msg.type = iRadioInterfaceIndex;
   memcpy((u8*)&(msg.data[0]), pData, iDataLength); 
   
   int iRetryCounter = 2;
   int iSucceeded = 0;
   do
   {
      if ( 0 == msgsnd(s_iRadioTxIPCQueue, &msg, iDataLength, IPC_NOWAIT) )
      {
         iSucceeded = 1;
         iRetryCounter = 0;
         break;
      }
   
      if ( errno == EAGAIN )
         log_softerror_and_alarm("[RadioTx] Failed to write to IPC, error code: EAGAIN" );
      if ( errno == EACCES )
         log_softerror_and_alarm("[RadioTx] Failed to write to IPC, error code: EACCESS");
      if ( errno == EIDRM )
         log_softerror_and_alarm("[RadioTx] Failed to write to IPC %s, error code: EIDRM");
      if ( errno == EINVAL )
         log_softerror_and_alarm("[RadioTx] Failed to write to IPC %s, error code: EINVAL");


      struct msqid_ds msg_stats;
      if ( 0 != msgctl(s_iRadioTxIPCQueue, IPC_STAT, &msg_stats) )
         log_softerror_and_alarm("[RadioTx] Failed to get statistics on ICP message queue, fd %d",
            s_iRadioTxIPCQueue );
      else
      {
         log_line("[RadioTx] IPC Info (fd %d) info: %u pending messages, %u used bytes, max bytes in the IPC channel: %u bytes",
            s_iRadioTxIPCQueue, (u32)msg_stats.msg_qnum, (u32)msg_stats.msg_cbytes, (u32)msg_stats.msg_qbytes);
      }

      iRetryCounter--;
      hardware_sleep_ms(5);

   } while (iRetryCounter > 0);

   if ( iSucceeded )
     return 1;
   return -1;
}
