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

#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <sys/file.h>
#include <time.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

bool g_bQuit = false;

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

   log_enable_stdout();
   log_init_local_only("RubyLogger");
   
   key_t key;
   int iLogMsgQueue = -1;
   type_log_message_buffer logMessage;

   key = ftok("ruby_logger", LOGGER_MESSAGE_QUEUE_ID);
   iLogMsgQueue = msgget(key, 0444 | IPC_CREAT);
   if ( iLogMsgQueue < 0 )
   {
      log_softerror_and_alarm("Failed to create logger message queue");
      iLogMsgQueue = msgget(key, 0444);
      if ( iLogMsgQueue >= 0 )
      {
         log_softerror_and_alarm("Queue already exists. Closing and recreating it.");
         msgctl(iLogMsgQueue,IPC_RMID,NULL);
         iLogMsgQueue = msgget(key, 0444 | IPC_CREAT);
         if ( iLogMsgQueue < 0 )
         {
            log_softerror_and_alarm("Failed to recreate logger message queue");
         }
      }
      log_softerror_and_alarm("Failed to open existing logger message queue");
   }
   else
      log_line("Created logger message queue, key id: %x, msgqueue id: %d", key, iLogMsgQueue);

   if ( iLogMsgQueue < 0 )
   {
      log_softerror_and_alarm("No message queue available. Exit logger.");
      return -1;
   }

   log_line("Started logger main loop");

   while ( !g_bQuit )
   {
      // This is blocking
      int len = msgrcv(iLogMsgQueue, &logMessage, MAX_SERVICE_LOG_ENTRY_LENGTH, 0, 0);
      if ( len <= 0 )
      {
          log_line("Failed to read log message queue. Exiting...");
          g_bQuit = true;
          break;
      }

      logMessage.text[len-1] = 0;
      //log_line("Received message type: %d, length: %d bytes", logMessage.type, len);

      //if ( logMessage.type == 1 )
      if ( 1 )
      {
         FILE* fd = fopen(LOG_FILE_SYSTEM, "a+");
         if ( NULL != fd )
         {
            fprintf(fd, "%s\n", logMessage.text);
            fclose(fd);
         }
      }

      if ( logMessage.type == 2 )
      {
         FILE* fd = fopen(LOG_FILE_ERRORS_SOFT, "a+");
         if ( NULL != fd )
         {
            fprintf(fd, "%s\n", logMessage.text);
            fclose(fd);
         }
      }

      if ( logMessage.type == 3 )
      {
         FILE* fd = fopen(LOG_FILE_ERRORS, "a+");
         if ( NULL != fd )
         {
            fprintf(fd, "%s\n", logMessage.text);
            fclose(fd);
         }
      }

      //hardware_sleep_ms(5);
   }

   if ( iLogMsgQueue >= 0 )
   {
      msgctl(iLogMsgQueue,IPC_RMID,NULL);
      log_line("Closed logger message queue.");
   }

   log_line("Stopped");
   return 0;
} 