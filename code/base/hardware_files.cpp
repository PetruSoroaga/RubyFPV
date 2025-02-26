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

#include "base.h"
#include "config.h"
#include "hardware_files.h"
#include "hw_procs.h"
#include <pthread.h>

pthread_t s_pThreadGetFreeSpaceAsync;
static int s_iGetFreeSpaceAsyncResultValueKb = -1;

bool hardware_file_check_and_fix_access(char* szFullFileName)
{
   if ( (NULL == szFullFileName) || (0 == szFullFileName[0]) )
      return false;
   if ( access(szFullFileName, F_OK) == -1 )
      return false;

   bool bUpdate = false;
   if ( access(szFullFileName, R_OK) == -1 )
      bUpdate = true;
   if ( access(szFullFileName, X_OK) == -1 )
      bUpdate = true;

   if ( bUpdate )
   {
      char szComm[MAX_FILE_PATH_SIZE];
      snprintf(szComm, sizeof(szComm)/sizeof(szComm[0]), "chmod 777 %s", szFullFileName);
      hw_execute_bash_command(szComm, NULL);
   }

   if ( access(szFullFileName, R_OK) != -1 )
   if ( access(szFullFileName, X_OK) != -1 )
      return true;
   return false;
}

void hardware_mount_root()
{
   #ifdef HW_PLATFORM_RASPBERRY
   hw_execute_bash_command("sudo mount -o remount,rw /", NULL);
   #endif

   #if defined(HW_PLATFORM_RADXA_ZERO3)
   hw_execute_bash_command("sudo mount -o remount,rw /", NULL);
   #endif
}

void hardware_mount_boot()
{
   #ifdef HW_PLATFORM_RASPBERRY
   hw_execute_bash_command("sudo mount -o remount,rw /boot", NULL);
   #endif
}

int hardware_get_free_space_kb()
{
   char szOutput[2048];
   szOutput[0] = 0;

   #if defined(HW_PLATFORM_RASPBERRY)
   if ( 1 != hw_execute_bash_command_raw("df . | grep root", szOutput) )
      return -1;
   #endif
   #if defined(HW_PLATFORM_RADXA_ZERO3)
   if ( 1 != hw_execute_bash_command_raw("df / | grep dev/", szOutput) )
      return -1;
   #endif
   #if defined(HW_PLATFORM_OPENIPC_CAMERA)
   //if ( 1 != hw_execute_bash_command_raw("df . | grep overlay 2>/dev/null", szOutput) )
   //   return -1;
   //if ( strlen(szOutput) < 10 )
   if ( 1 != hw_execute_bash_command_raw("df /tmp | grep tmpfs 2>/dev/null", szOutput) )
      return -1;
   #endif

   if ( strlen(szOutput) < 10 )
      return -1;
   char szTemp[1024];
   long lb = 0, lu = 0, lf = 0;
   sscanf(szOutput, "%s %ld %ld %ld", szTemp, &lb, &lu, &lf);
   return (int)lf;
}

void* _thread_get_free_space_async(void *argument)
{
   sched_yield();
   s_iGetFreeSpaceAsyncResultValueKb = -1;
   hardware_sleep_ms(500);
   int iFreeSpaceKb = hardware_get_free_space_kb();
   log_line("Done getting free space async");
   s_iGetFreeSpaceAsyncResultValueKb = iFreeSpaceKb;
   return NULL;
}

int hardware_get_free_space_kb_async()
{
   s_iGetFreeSpaceAsyncResultValueKb = -1;
   pthread_attr_t attr;
   struct sched_param params;

   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
   pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
   pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
   params.sched_priority = 0;
   pthread_attr_setschedparam(&attr, &params);
   if ( 0 != pthread_create(&s_pThreadGetFreeSpaceAsync, &attr, &_thread_get_free_space_async, NULL) )
   {
      pthread_attr_destroy(&attr);
      return -1;
   }
   pthread_attr_destroy(&attr);
   return 0;
}

int hardware_has_finished_get_free_space_kb_async()
{
   return s_iGetFreeSpaceAsyncResultValueKb;
}
