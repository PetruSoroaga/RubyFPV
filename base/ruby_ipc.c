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

#include "base.h"
#include "config.h"
#include "ruby_ipc.h"
#include "hardware.h"
#include "hw_procs.h"
#include "../common/string_utils.h"
#include "../radio/radiopackets2.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>

//#define RUBY_USE_FIFO_PIPES 1
#define RUBY_USES_MSGQUEUES 1

#define FIFO_RUBY_ROUTER_TO_CENTRAL "tmp/ruby/fiforoutercentral"
#define FIFO_RUBY_CENTRAL_TO_ROUTER "tmp/ruby/fifocentralrouter"
#define FIFO_RUBY_ROUTER_TO_COMMANDS "tmp/ruby/fiforoutercommands"
#define FIFO_RUBY_COMMANDS_TO_ROUTER "tmp/ruby/fifocommandsrouter"
#define FIFO_RUBY_ROUTER_TO_TELEMETRY "tmp/ruby/fiforoutertelemetry"
#define FIFO_RUBY_TELEMETRY_TO_ROUTER "tmp/ruby/fifotelemetryrouter"
#define FIFO_RUBY_ROUTER_TO_RC "tmp/ruby/fiforouterrc"
#define FIFO_RUBY_RC_TO_ROUTER "tmp/ruby/fiforcrouter"


#define PROFILE_IPC 1
#define PROFILE_IPC_MAX_TIME 5

#define MAX_CHANNELS 32

int s_iRubyIPCChannelsFd[MAX_CHANNELS];
int s_iRubyIPCChannelsType[MAX_CHANNELS];
key_t s_uRubyIPCChannelsKeys[MAX_CHANNELS];

int s_iRubyIPCChannelsCount = 0;

typedef struct
{
    long type;
    char data[ICP_CHANNEL_MAX_MSG_SIZE];
} type_ipc_message_buffer;


char* _ruby_ipc_get_channel_name(int nChannelType)
{
   static char s_szRubyIPCChannelType[128];
   s_szRubyIPCChannelType[0] = 0;
   sprintf(s_szRubyIPCChannelType, "[Unknown Channel Type %d]", nChannelType);

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_CENTRAL )
      strcpy(s_szRubyIPCChannelType, "[ROUTER-TO-CENTRAL]");

   if ( nChannelType == IPC_CHANNEL_TYPE_CENTRAL_TO_ROUTER )
      strcpy(s_szRubyIPCChannelType, "[CENTRAL-TO-ROUTER]");

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY )
      strcpy(s_szRubyIPCChannelType, "[ROUTER-TO-TELEMETRY]");

   if ( nChannelType == IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER )
      strcpy(s_szRubyIPCChannelType, "[TELEMETRY-TO-ROUTER]");

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_RC )
      strcpy(s_szRubyIPCChannelType, "[ROUTER-TO-RC]");

   if ( nChannelType == IPC_CHANNEL_TYPE_RC_TO_ROUTER )
      strcpy(s_szRubyIPCChannelType, "[RC-TO-ROUTER]");

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_COMMANDS )
      strcpy(s_szRubyIPCChannelType, "[ROUTER-TO-COMMANDS]");

   if ( nChannelType == IPC_CHANNEL_TYPE_COMMANDS_TO_ROUTER )
      strcpy(s_szRubyIPCChannelType, "[COMMANDS-TO-ROUTER]");

   return s_szRubyIPCChannelType;
}

char* _ruby_ipc_get_pipe_name(int nChannelType )
{
   static char s_szRubyPipeName[256];
   s_szRubyPipeName[0] = 0;

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_CENTRAL )
      strcpy(s_szRubyPipeName, FIFO_RUBY_ROUTER_TO_CENTRAL);

   if ( nChannelType == IPC_CHANNEL_TYPE_CENTRAL_TO_ROUTER )
      strcpy(s_szRubyPipeName, FIFO_RUBY_CENTRAL_TO_ROUTER);

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY )
      strcpy(s_szRubyPipeName, FIFO_RUBY_ROUTER_TO_TELEMETRY);

   if ( nChannelType == IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER )
      strcpy(s_szRubyPipeName, FIFO_RUBY_TELEMETRY_TO_ROUTER);

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_RC )
      strcpy(s_szRubyPipeName, FIFO_RUBY_ROUTER_TO_RC);

   if ( nChannelType == IPC_CHANNEL_TYPE_RC_TO_ROUTER )
      strcpy(s_szRubyPipeName, FIFO_RUBY_RC_TO_ROUTER);

   if ( nChannelType == IPC_CHANNEL_TYPE_ROUTER_TO_COMMANDS )
      strcpy(s_szRubyPipeName, FIFO_RUBY_ROUTER_TO_COMMANDS);

   if ( nChannelType == IPC_CHANNEL_TYPE_COMMANDS_TO_ROUTER )
      strcpy(s_szRubyPipeName, FIFO_RUBY_COMMANDS_TO_ROUTER);

   return s_szRubyPipeName;
}


void _check_ruby_ipc_consistency()
{
   for( int i=0; i<s_iRubyIPCChannelsCount-1; i++ )
   {
      for ( int k=i+1; k<s_iRubyIPCChannelsCount; k++ )
      {
         #ifdef RUBY_USES_MSGQUEUES
         if ( s_uRubyIPCChannelsKeys[i] == s_uRubyIPCChannelsKeys[k] )
            log_error_and_alarm("[IPC] Duplicate key for IPC channels %d and %d, %s and %s.", i, k, _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[i]), _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[k]));
         #endif
         if ( s_iRubyIPCChannelsFd[i] == s_iRubyIPCChannelsFd[k] )
            log_error_and_alarm("[IPC] Duplicate fd for IPC channels %d and %d, %s and %s.", i, k, _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[i]), _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[k]));
         if ( s_iRubyIPCChannelsType[i] == s_iRubyIPCChannelsType[k] )
            log_error_and_alarm("[IPC] Duplicate channel type for IPC channels %d and %d, %s and %s.", i, k, _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[i]), _ruby_ipc_get_pipe_name(s_iRubyIPCChannelsType[k]));
      }
   }
}


int ruby_init_ipc_channels()
{
   char szBuff[256];

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_CAMERA1 );
   hw_execute_bash_command(szBuff, NULL);
      
   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_AUDIO1 );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_STATION_VIDEO_STREAM );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_STATION_VIDEO_STREAM_ETH );
   hw_execute_bash_command(szBuff, NULL);

   #ifdef RUBY_USE_FIFO_PIPES

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_ROUTER_TO_CENTRAL );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_CENTRAL_TO_ROUTER );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_ROUTER_TO_COMMANDS );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_COMMANDS_TO_ROUTER);
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_ROUTER_TO_TELEMETRY );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_TELEMETRY_TO_ROUTER );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_ROUTER_TO_RC );
   hw_execute_bash_command(szBuff, NULL);

   sprintf(szBuff, "mkfifo %s", FIFO_RUBY_RC_TO_ROUTER );
   hw_execute_bash_command(szBuff, NULL);

   #endif

   return 1;
}

void ruby_clear_all_ipc_channels()
{
   log_line("[IPC] Clearing all IPC channels...");
   
   #ifdef RUBY_USES_MSGQUEUES

   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
      msgctl(s_iRubyIPCChannelsFd[i],IPC_RMID,NULL);
   s_iRubyIPCChannelsCount = 0;

   #endif

   log_line("[IPC] Done clearing all IPC channels.");
}

int ruby_open_ipc_channel_write_endpoint(int nChannelType)
{
   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
      if ( s_iRubyIPCChannelsType[i] == nChannelType )
      if ( s_iRubyIPCChannelsFd[i] > 0 )
         return s_iRubyIPCChannelsFd[i];

   log_line("[IPC] Opening IPC channel %s write endpoint...", _ruby_ipc_get_channel_name(nChannelType));
   if ( s_iRubyIPCChannelsCount >= MAX_CHANNELS )
   {
      log_error_and_alarm("[IPC] Can't open more IPC channels. List full.");
      return 0;
   }

   s_iRubyIPCChannelsType[s_iRubyIPCChannelsCount] = nChannelType;

   #ifdef RUBY_USE_FIFO_PIPES

   char* szPipeName = _ruby_ipc_get_pipe_name(nChannelType);
   if ( NULL == szPipeName || 0 == szPipeName[0] )
   {
      log_error_and_alarm("[IPC] Can't open IPC FIFO Pipe with invalid name (requested type: %d).", nChannelType);
      return 0;
   }

   s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] = open(szPipeName, O_WRONLY | (RUBY_PIPES_EXTRA_FLAGS & (~O_NONBLOCK)));
   if ( s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] < 0 )
   {
      log_error_and_alarm("[IPC] Failed to open IPC channel %s write endpoint.", _ruby_ipc_get_channel_name(nChannelType));
      return 0;
   }

   if ( RUBY_PIPES_EXTRA_FLAGS & O_NONBLOCK )
   if ( 0 != fcntl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], F_SETFL, O_NONBLOCK) )
      log_softerror_and_alarm("[IPC] Failed to set nonblock flag on PIC channel %s write endpoint.", _ruby_ipc_get_channel_name(nChannelType));

   log_line("[IPC] FIFO write endpoint pipe flags: %s", str_get_pipe_flags(fcntl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], F_GETFL)));
   
   #endif

   #ifdef RUBY_USES_MSGQUEUES

   key_t key;

   key = ftok("ruby_logger", nChannelType);
   s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] = msgget(key, 0222 | IPC_CREAT);
   if ( s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] < 0 )
   {
      log_softerror_and_alarm("[IPC] Failed to create IPC message queue write endpoint for channel: ", _ruby_ipc_get_channel_name(nChannelType));
      return -1;
   }
   s_uRubyIPCChannelsKeys[s_iRubyIPCChannelsCount] = key;

   //struct msqid_ds msg_stats;
   //if ( 0 != msgctl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], IPC_STAT, &msg_stats) )
   //   log_softerror_and_alarm("[IPC] Failed to get statistics on ICP message queue %s", _ruby_ipc_get_channel_name(nChannelType) );
   //else
   //   log_line("[IPC] FD: %d, id: %u, Max bytes in the IPC channel %s: %u bytes", s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], key, _ruby_ipc_get_channel_name(nChannelType), (u32)msg_stats.msg_qbytes);

   //struct msginfo msg_info;
   //if ( 0 != msgctl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], IPC_INFO, (struct msqid_ds*)&msg_info) )
   //   log_softerror_and_alarm("[IPC] Failed to get statistics on system message queues." );
   //else
   //   log_line("[IPC] IPC channels pools max: %u bytes, max msg size: %u bytes, max msg queue total size: %u bytes", (u32)msg_info.msgpool, (u32)msg_info.msgmax, (u32)msg_info.msgmnb);

   #endif

   s_iRubyIPCChannelsCount++;
   
   log_line("[IPC] Opened IPC channel %s write endpoint: success, id: %d. (%d channels currently opened).", _ruby_ipc_get_channel_name(nChannelType), s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount-1], s_iRubyIPCChannelsCount);
   _check_ruby_ipc_consistency();
   return s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount-1];
}

int ruby_open_ipc_channel_read_endpoint(int nChannelType)
{
   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
      if ( s_iRubyIPCChannelsType[i] == nChannelType )
      if ( s_iRubyIPCChannelsFd[i] > 0 )
         return s_iRubyIPCChannelsFd[i];

   log_line("[IPC] Opening IPC channel %s read endpoint...", _ruby_ipc_get_channel_name(nChannelType));
   if ( s_iRubyIPCChannelsCount >= MAX_CHANNELS )
   {
      log_error_and_alarm("[IPC] Can't open more IPC channels. List full.");
      return 0;
   }

   s_iRubyIPCChannelsType[s_iRubyIPCChannelsCount] = nChannelType;

   #ifdef RUBY_USE_FIFO_PIPES

   char* szPipeName = _ruby_ipc_get_pipe_name(nChannelType);
   if ( NULL == szPipeName || 0 == szPipeName[0] )
   {
      log_error_and_alarm("[IPC] Can't open IPC FIFO Pipe with invalid name (requested type: %d).", nChannelType);
      return 0;
   }

   s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] = open(szPipeName, O_RDONLY | RUBY_PIPES_EXTRA_FLAGS);

   if ( s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] < 0 )
   {
      log_error_and_alarm("[IPC] Failed to open IPC channel %s read endpoint.", _ruby_ipc_get_channel_name(nChannelType));
      return 0;
   }

   log_line("[IPC] FIFO read endpoint pipe flags: %s", str_get_pipe_flags(fcntl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], F_GETFL)));

   #endif

   #ifdef RUBY_USES_MSGQUEUES

   key_t key;

   key = ftok("ruby_logger", nChannelType);
   s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] = msgget(key, 0444 | IPC_CREAT);
   if ( s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount] < 0 )
   {
      log_softerror_and_alarm("[IPC] Failed to create IPC message queue read endpoint for channel: ", _ruby_ipc_get_channel_name(nChannelType));
      return -1;
   }
   s_uRubyIPCChannelsKeys[s_iRubyIPCChannelsCount] = key;

   //struct msqid_ds msg_stats;
   //if ( 0 != msgctl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], IPC_STAT, &msg_stats) )
   //   log_softerror_and_alarm("[IPC] Failed to get statistics on ICP message queue %s", _ruby_ipc_get_channel_name(nChannelType) );
   //else
   //   log_line("[IPC] FD: %d, key: %u, Max bytes in the IPC channel %s: %u bytes", s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], key, _ruby_ipc_get_channel_name(nChannelType), (u32)msg_stats.msg_qbytes);

   //struct msginfo msg_info;
   //if ( 0 != msgctl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], IPC_INFO, (struct msqid_ds*)&msg_info) )
   //   log_softerror_and_alarm("[IPC] Failed to get statistics on system message queues." );
   //else
   //   log_line("[IPC] IPC channels pools max: %u bytes, max msg size: %u bytes, max msg queue total size: %u bytes", (u32)msg_info.msgpool, (u32)msg_info.msgmax, (u32)msg_info.msgmnb);
   #endif

   s_iRubyIPCChannelsCount++;
   
   log_line("[IPC] Opened IPC channel %s read endpoint: success, id: %d. (%d channels currently opened).", _ruby_ipc_get_channel_name(nChannelType), s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount-1], s_iRubyIPCChannelsCount);
   _check_ruby_ipc_consistency();
   return s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount-1];
}

int ruby_close_ipc_channel(int iChannelFd)
{
   if ( iChannelFd < 0 )
   {
      log_softerror_and_alarm("[IPC] Tried to close invalid IPC channel.");
      return 0;
   }

   #ifdef RUBY_USE_FIFO_PIPES
   close(iChannelFd);
   #endif

   #ifdef RUBY_USES_MSGQUEUES
   msgctl(iChannelFd,IPC_RMID,NULL);
   #endif

   int iFound = 0;

   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
   {
      if ( s_iRubyIPCChannelsFd[i] == iChannelFd )
      {
         iFound = 1;
         log_line("[IPC] Closed IPC channel %s", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[i]));
         for( int k=i; k<s_iRubyIPCChannelsCount-1; k++ )
         {
            s_iRubyIPCChannelsFd[k] = s_iRubyIPCChannelsFd[k+1];
            s_iRubyIPCChannelsType[k] = s_iRubyIPCChannelsType[k+1];
         }
         s_iRubyIPCChannelsCount--;
         break;
      }
   }
   if ( ! iFound )
      log_softerror_and_alarm("[IPC] Closed unknown IPC channel.");
   return 1;
}

int ruby_ipc_channel_send_message(int iChannelFd, u8* pMessage, int iLength)
{
   if ( iChannelFd < 0 || s_iRubyIPCChannelsCount == 0 )
   {
      log_softerror_and_alarm("[IPC] Tried to write a message to an invalid channel (%d)", iChannelFd );
      return 0;
   }

   int iFound = -1;

   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
      if ( s_iRubyIPCChannelsFd[i] == iChannelFd )
      {
         iFound = i;
         break;
      }

   if ( iFound == -1 )
   {
      log_softerror_and_alarm("[IPC] Tried to write a message to an invalid channel (%d) not in the list (%d channels active now).", iChannelFd, s_iRubyIPCChannelsCount);
      return 0;
   }

   if ( NULL == pMessage || 0 == iLength )
   {
      log_softerror_and_alarm("[IPC] Tried to write an invalid message (null or 0) on channel %s", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]) );
      return 0;
   }

   if ( iLength >= ICP_CHANNEL_MAX_MSG_SIZE-6 )
   {
      log_softerror_and_alarm("[IPC] Tried to write a message too big (%d bytes) on channel %s", iLength, _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]) );
      return 0;
   }

   int res = 0;
   
   #ifdef PROFILE_IPC
   u32 uTimeStart = get_current_timestamp_ms();
   #endif

   #ifdef RUBY_USE_FIFO_PIPES
   res = write(iChannelFd, pMessage, iLength);
   #endif

   #ifdef RUBY_USES_MSGQUEUES
   
   type_ipc_message_buffer msg;

   msg.type = 1;
   msg.data[4] = ((u32)iLength) & 0xFF; 
   msg.data[5] = (((u32)iLength)>>8) & 0xFF;
   memcpy((u8*)&(msg.data[6]), pMessage, iLength); 
   u32 uCRC = base_compute_crc32((u8*)&(msg.data[4]), iLength+2);
   memcpy((u8*)&(msg.data[0]), (u8*)&uCRC, sizeof(u32));
   if ( 0 == msgsnd(iChannelFd, &msg, sizeof(msg), IPC_NOWAIT) )
   {
      res = iLength;
      //log_line("[IPC] Send IPC message on channel %s: %d bytes, crc: %u", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]), iLength, uCRC);
   }
   else
   {
      res = 0;
      if ( errno == EAGAIN )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %s, error code: EAGAIN", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]) );
      if ( errno == EACCES )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %s, error code: EACCESS", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]) );
      if ( errno == EIDRM )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %s, error code: EIDRM", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]) );
      if ( errno == EINVAL )
         log_softerror_and_alarm("[IPC] Failed to write to IPC %s, error code: EINVAL", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]) );


      struct msqid_ds msg_stats;
      if ( 0 != msgctl(s_iRubyIPCChannelsFd[s_iRubyIPCChannelsCount], IPC_STAT, &msg_stats) )
         log_softerror_and_alarm("[IPC] Failed to get statistics on ICP message queue %s", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]) );
      else
         log_line("[IPC] Channel %s info: %u pending messages, %u used bytes, max bytes in the IPC channel: %u bytes", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]), (u32)msg_stats.msg_qnum, (u32)msg_stats.msg_cbytes, (u32)msg_stats.msg_qbytes);
   }
   #endif

   #ifdef PROFILE_IPC
   u32 uTimeTotal = get_current_timestamp_ms() - uTimeStart;
   if ( uTimeTotal > PROFILE_IPC_MAX_TIME )
   {
      t_packet_header* pPH = (t_packet_header*)pMessage;
      log_softerror_and_alarm("[IPC] Write message (%d bytes) on channel %s took too long (%u ms) (Message component: %d, msg type: %d, msg length:%d).", iLength, _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]), uTimeTotal, (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE), pPH->packet_type, pPH->total_length);
   }
   #endif

   if ( res != iLength )
   {
      t_packet_header* pPH = (t_packet_header*)pMessage;
      log_softerror_and_alarm("[IPC] Failed to write a message (%d bytes) on channel %s. Only %d bytes written (Message component: %d, msg type: %d, msg length:%d).", iLength, _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]), res, (pPH->packet_flags & PACKET_FLAGS_MASK_MODULE), pPH->packet_type, pPH->total_length);
      return 0;
   }

   return res;
}


u8* ruby_ipc_try_read_message(int iChannelFd, int timeoutMicrosec, u8* pTempBuffer, int* pTempBufferPos, u8* pOutputBuffer)
{
   if ( iChannelFd < 0 || s_iRubyIPCChannelsCount == 0 )
   {
      log_softerror_and_alarm("[IPC] Tried to read a message from an invalid channel (%d)", iChannelFd );
      return NULL;
   }

   int iFound = -1;

   for( int i=0; i<s_iRubyIPCChannelsCount; i++ )
      if ( s_iRubyIPCChannelsFd[i] == iChannelFd )
      {
         iFound = i;
         break;
      }

   if ( iFound == -1 )
   {
      log_softerror_and_alarm("[IPC] Tried to read a message from an invalid channel (%d) not in the list (%d channels active now).", iChannelFd, s_iRubyIPCChannelsCount);
      return NULL;
   }

   if ( NULL == pTempBuffer || NULL == pTempBufferPos || NULL == pOutputBuffer )
   {
      log_softerror_and_alarm("[IPC] Tried to read a message into a NULL buffer on channel %s", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]) );
      return NULL;
   }

   u8* pReturn = NULL;

   #ifdef PROFILE_IPC
   u32 uTimeStart = get_current_timestamp_ms();
   #endif

   #ifdef RUBY_USE_FIFO_PIPES

   t_packet_header* pPH = (t_packet_header*)pTempBuffer;

   if ( (*pTempBufferPos) >= (int)sizeof(t_packet_header) )
   {
      if ( pPH->total_length > MAX_PACKET_TOTAL_SIZE )
         *pTempBufferPos = 0;
   }

   if ( ((*pTempBufferPos) >= (int)sizeof(t_packet_header)) && (*pTempBufferPos) >= pPH->total_length )
   {
      if ( ! packet_check_crc( pTempBuffer, pPH->total_length ) )
      {
         // invalid data. just reset buffers.
         *pTempBufferPos = 0;
      }
      else
      {
         //log_line("Has data in pipe already");
         //log_buffer(pTempBuffer, 40);
         //log_line(" <<< Read existing packet from pipe: type: %d, component: %d, length: %d", pPH->packet_type, pPH->packet_flags & PACKET_FLAGS_MASK_MODULE, pPH->total_length);
         int len = pPH->total_length;
         memcpy(pOutputBuffer, pTempBuffer, len);
         *pTempBufferPos -= len;
         for( int i=len; i<MAX_PACKET_TOTAL_SIZE; i++ )
            pTempBuffer[i-len] = pTempBuffer[i];
         pReturn = pOutputBuffer;
      }
   }
   else
   {
      fd_set readset; 
      struct timeval timePipeInput;
      
      FD_ZERO(&readset);
      FD_SET(iChannelFd, &readset);

      timePipeInput.tv_sec = 0;
      timePipeInput.tv_usec = timeoutMicrosec;

      int selectResult = select(iChannelFd+1, &readset, NULL, NULL, &timePipeInput);
      if ( selectResult > 0 && (FD_ISSET(iChannelFd, &readset)) )
      {
         int count = read(iChannelFd, pTempBuffer+(*pTempBufferPos), MAX_PACKET_TOTAL_SIZE-(*pTempBufferPos));
         if ( count < 0 )
         {
             log_error_and_alarm("[IPC] Failed to read FIFO pipe %s. Broken pipe.", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]) );
         }
         else
         {
            *pTempBufferPos += count;

            if ( ((*pTempBufferPos) >= (int)sizeof(t_packet_header)) && (*pTempBufferPos) >= pPH->total_length )
            {
               if ( ! packet_check_crc( pTempBuffer, pPH->total_length ) )
               {
                  // invalid data. just reset buffers.
                  *pTempBufferPos = 0;
               }
               else
               {
                  int len = pPH->total_length;
                  memcpy(pOutputBuffer, pTempBuffer, len);
                  *pTempBufferPos -= len;
                  for( int i=len; i<MAX_PACKET_TOTAL_SIZE; i++ )
                     pTempBuffer[i-len] = pTempBuffer[i];
                  pReturn = pOutputBuffer;
               }
            }
         }
      }
   }
   #endif

   #ifdef RUBY_USES_MSGQUEUES

   type_ipc_message_buffer ipcMessage;

   pReturn = NULL;
   
   u32 uTimeStartReadIpc = get_current_timestamp_ms();

   int lenReadIPCMsgQueue = msgrcv(iChannelFd, &ipcMessage, sizeof(ipcMessage), 0, IPC_NOWAIT);
   if ( lenReadIPCMsgQueue > 0 )
   {
      int iMsgLen = ipcMessage.data[4] + 256*(int)ipcMessage.data[5];
      if ( iMsgLen <= 0 || iMsgLen >= ICP_CHANNEL_MAX_MSG_SIZE - 6 )
         log_softerror_and_alarm("[IPC] Received invalid message on channel %s, length: %d", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]), iMsgLen );
      else
      {
         u32 uCRC = base_compute_crc32((u8*)&(ipcMessage.data[4]), iMsgLen+2);
         u32 uTmp = 0;
         memcpy((u8*)&uTmp, (u8*)&(ipcMessage.data[0]), sizeof(u32));
         if ( uCRC != uTmp )
            log_softerror_and_alarm("[IPC] Received invalid CRC on channel %s on message, CRC: %u, msg length: %d", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]), uCRC, iMsgLen );
         else
         {
            memcpy(pOutputBuffer, (u8*)&(ipcMessage.data[6]), iMsgLen);
            pReturn = pOutputBuffer;
            //log_line("[IPC] Received message ok on channel %s, length: %d, CRC: %u", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]), iMsgLen, uCRC );
         }
      }
   }

   #endif

   #ifdef PROFILE_IPC
   u32 uTimeTotal = get_current_timestamp_ms() - uTimeStart;
   if ( (uTimeTotal > PROFILE_IPC_MAX_TIME + timeoutMicrosec/1000) || uTimeTotal >= 50 )
   {
      log_softerror_and_alarm("[IPC] Read message on channel %s took too long (%u ms).", _ruby_ipc_get_channel_name(s_iRubyIPCChannelsType[iFound]), uTimeTotal );
   }
   #endif

   #ifdef RUBY_USES_MSGQUEUES
   if ( lenReadIPCMsgQueue == 0 )
   if ( timeoutMicrosec >= 1000 )
   {
      u32 uTimeDiffReadIPC = get_current_timestamp_ms() - uTimeStartReadIpc;
      if ( (u32)timeoutMicrosec > uTimeDiffReadIPC*1000 + 200 )
         hardware_sleep_micros((u32)timeoutMicrosec - uTimeDiffReadIPC*1000);
   }
   #endif

   return pReturn;
}