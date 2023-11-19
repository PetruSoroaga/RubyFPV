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
      log_line("Created logger message queue");

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