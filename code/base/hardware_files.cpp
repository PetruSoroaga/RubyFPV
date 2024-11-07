/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and/or binary forms, with or without
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

   #if defined(HW_PLATFORM_RASPBERRY)
   if ( 1 != hw_execute_bash_command_raw("df . | grep root", szOutput) )
      return -1;
   #elif defined( HW_PLATFORM_RADXA_ZERO3)
   if ( 1 != hw_execute_bash_command_raw("df / | grep dev/", szOutput) )
      return -1;
   #else
   szOutput[0] = 0;
   if ( 1 != hw_execute_bash_command_raw("df . | grep overlay 2>/dev/null", szOutput) )
      return -1;
   if ( strlen(szOutput) < 10 )
   if ( 1 != hw_execute_bash_command_raw("df . | grep tmpfs 2>/dev/null", szOutput) )
      return -1;
   #endif

   if ( strlen(szOutput) < 10 )
      return -1;
   char szTemp[1024];
   long lb = 0, lu = 0, lf = 0;
   sscanf(szOutput, "%s %ld %ld %ld", szTemp, &lb, &lu, &lf);
   return (int)lf;
}
