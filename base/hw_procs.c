#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <ctype.h>

#include "base.h"
#include "config.h"
#include "hw_procs.h"
#include "hardware.h"

int hw_process_exists(const char* szProcName)
{
   char szComm[1024];
   char szPids[1024];

   if ( NULL == szProcName || 0 == szProcName[0] )
      return 0;

   sprintf(szComm, "pidof %s", szProcName);
   hw_execute_bash_command_silent(szComm, szPids);
   if ( strlen(szPids) > 2 )
      return 1;
   return 0;
}

void hw_stop_process(const char* szProcName)
{
   char szComm[1024];
   char szPids[1024];

   if ( NULL == szProcName || 0 == szProcName[0] )
      return;

   sprintf(szComm, "pidof %s", szProcName);
   hw_execute_bash_command(szComm, szPids);
   if ( strlen(szPids) > 2 )
   {
      sprintf(szComm, "kill $(pidof %s) 2>/dev/null", szProcName);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(20);
      int retryCount = 20;
      sprintf(szComm, "pidof %s", szProcName);
      while ( retryCount > 0 )
      {
         hardware_sleep_ms(15);
         szPids[0] = 0;
         hw_execute_bash_command(szComm, szPids);
         if ( strlen(szPids) < 2 )
            return;
         retryCount--;
      }
      sprintf(szComm, "kill -9 $(pidof %s) 2>/dev/null", szProcName);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(20);
   }
}


void hw_kill_process(const char* szProcName)
{
   char szComm[1024];
   char szPids[1024];

   if ( NULL == szProcName || 0 == szProcName[0] )
      return;

   sprintf(szComm, "kill -9 $(pidof %s) 2>/dev/null", szProcName);
   hw_execute_bash_command(szComm, NULL);
   hardware_sleep_ms(20);

   sprintf(szComm, "pidof %s", szProcName);
   hw_execute_bash_command(szComm, szPids);
   if ( strlen(szPids) > 2 )
   {
      log_line("Process %s pid is: %s", szProcName, szPids);

      int retryCount = 10;
      sprintf(szComm, "pidof %s", szProcName);
      while ( retryCount > 0 )
      {
         hardware_sleep_ms(10);
         szPids[0] = 0;
         hw_execute_bash_command(szComm, szPids);
         if ( strlen(szPids) < 2 )
            return;
         log_line("Process %s pid is: %s", szProcName, szPids);
         retryCount--;
      }
   }
}


int hw_launch_process(const char *szFile)
{
   return hw_launch_process4(szFile, NULL, NULL, NULL, NULL);
}

int hw_launch_process1(const char *szFile, const char* szParam1)
{
   return hw_launch_process4(szFile, szParam1, NULL, NULL, NULL);
}

int hw_launch_process2(const char *szFile, const char* szParam1, const char* szParam2)
{
   return hw_launch_process4(szFile, szParam1, szParam2, NULL, NULL);
}

int hw_launch_process3(const char *szFile, const char* szParam1, const char* szParam2, const char* szParam3)
{
   return hw_launch_process4(szFile, szParam1, szParam2, szParam3, NULL);
}

int hw_launch_process4(const char *szFile, const char* szParam1, const char* szParam2, const char* szParam3, const char* szParam4)
{
   // execv messes up the timer

   char szBuff[1024];
   if ( NULL == szParam1 && NULL == szParam2 && NULL == szParam3 && NULL == szParam4 )
      sprintf(szBuff, "%s &", szFile);
   else
      sprintf(szBuff, "%s %s %s %s %s &", szFile, ((NULL != szParam1)?szParam1:""), ((NULL != szParam2)?szParam2:""), ((NULL != szParam3)?szParam3:""), ((NULL != szParam4)?szParam4:"") );
   hw_execute_bash_command(szBuff, NULL);
   return 0;

   log_line("--------------------------------------------------------");
   log_line("|   Launching process: %s %s %s %s %s", szFile, ((NULL != szParam1)?szParam1:""), ((NULL != szParam2)?szParam2:""), ((NULL != szParam3)?szParam3:""), ((NULL != szParam4)?szParam4:""));
   log_line("--------------------------------------------------------");

   pid_t   my_pid;
#ifdef WAIT_FOR_COMPLETION
   int status;
   int timeout /* unused ifdef WAIT_FOR_COMPLETION */;
#endif
   char    *argv[6];
   argv[5] = NULL;
   argv[4] = NULL;
   argv[3] = NULL;
   argv[2] = NULL;
   argv[1] = NULL;
   if ( NULL != szParam4 && 0 < strlen(szParam4) )
   {
      argv[4] = (char*)malloc(strlen(szParam4)+1);
      strcpy(argv[4], szParam4);
   }
   if ( NULL != szParam3 && 0 < strlen(szParam3) )
   {
      argv[3] = (char*)malloc(strlen(szParam3)+1);
      strcpy(argv[3], szParam3);
   }
   if ( NULL != szParam2 && 0 < strlen(szParam2) )
   {
      argv[2] = (char*)malloc(strlen(szParam2)+1);
      strcpy(argv[2], szParam2);
   }
   if ( NULL != szParam1 && 0 < strlen(szParam1) )
   {
      argv[1] = (char*)malloc(strlen(szParam1)+1);
      strcpy(argv[1], szParam1);
   }

   argv[0] = (char*)malloc(strlen(szFile)+1);
   strcpy(argv[0], szFile);
   if (0 == (my_pid = fork()))
   {
      if (-1 == execve(argv[0], (char **)argv , NULL))
      {
         perror("child process execve failed [%m]");
         return -1;
      }
   }

#ifdef WAIT_FOR_COMPLETION
    timeout = 1000;

    while (0 == waitpid(my_pid , &status , WNOHANG)) {
            if ( --timeout < 0 ) {
                    perror("timeout");
                    return -1;
            }
            sleep(1);
    }

    printf("%s WEXITSTATUS %d WIFEXITED %d [status %d]\n",
            argv[0], WEXITSTATUS(status), WIFEXITED(status), status);

    if (1 != WIFEXITED(status) || 0 != WEXITSTATUS(status)) {
            perror("%s failed, halt system");
            return -1;
    }

#endif
    return 0;
}

void hw_set_priority_current_proc(int nice)
{
   if ( nice != 0 )
      setpriority(PRIO_PROCESS, 0, nice);
}


void hw_set_proc_priority(const char* szProgName, int nice, int ionice, int waitForProcess)
{
   char szPids[128];
   char szComm[256];
   if ( NULL == szProgName || 0 == szProgName[0] )
      return;

   sprintf(szComm, "pidof %s", szProgName);
   szPids[0] = 0;
   int count = 0;
   hw_execute_bash_command_silent(szComm, szPids);
   while ( waitForProcess && (strlen(szPids) <= 2) && (count < 100) )
   {
      hardware_sleep_ms(2);
      szPids[0] = 0;
      hw_execute_bash_command_silent(szComm, szPids);
      count++;
   }

   if ( strlen(szPids) <= 2 )
      return;

   sprintf(szComm, "renice -n %d -p %s", nice, szPids);
   hw_execute_bash_command(szComm, NULL);

   if ( ionice > 0 )
   {
      sprintf(szComm, "ionice -c 1 -n %d -p %s", ionice, szPids);
      hw_execute_bash_command(szComm, NULL);
   }
   //else
   //   sprintf(szComm, "ionice -c 2 -n 5 -p %s", szPids);
}

void hw_get_proc_priority(const char* szProgName, char* szOutput)
{
   char szPids[128];
   char szComm[256];
   char szCommOut[1024];
   if ( NULL == szOutput )
      return;

   szOutput[0] = 0;
   if ( NULL == szProgName || 0 == szProgName[0] )
      return;

   if ( szProgName[0] == 'r' && szProgName[1] == 'u' )
      strcpy(szOutput, szProgName+5);
   else
      strcpy(szOutput, szProgName);
   strcat(szOutput, ": ");

   sprintf(szComm, "pidof %s", szProgName);
   hw_execute_bash_command(szComm, szPids);
   if ( strlen(szPids) <= 2 )
   {
      strcat(szOutput, "Not Running");
      return;
   }
   strcat(szOutput, "Running, ");
   sprintf(szComm, "cat /proc/%s/stat | awk '{print \"priority \" $18 \", nice \" $19}'", szPids);
   hw_execute_bash_command_raw(szComm, szCommOut);
   if ( 0 < strlen(szCommOut) )
      szCommOut[strlen(szCommOut)-1] = 0;
   strcat(szOutput, "pri.");
   strcat(szOutput, szCommOut+8);
   strcat(szOutput, ", io priority: ");

   sprintf(szComm, "ionice -p %s", szPids);
   hw_execute_bash_command_raw(szComm, szCommOut);
   if ( 0 < strlen(szCommOut) )
      szCommOut[strlen(szCommOut)-1] = 0;
   strcat(szOutput, szCommOut);
   strcat(szOutput, ";");
}

void hw_set_proc_affinity(const char* szProgName, int iCoreStart, int iCoreEnd)
{
   char szComm[128];
   char szOutput[256];
   sprintf(szComm, "pidof %s", szProgName);
   hw_execute_bash_command_silent(szComm, szOutput);
   if ( strlen(szOutput) < 3 )
   {
      log_softerror_and_alarm("Failed to set process affinity for process [%s], no such process.", szProgName);
      return;
   }
   int iPID = atoi(szOutput);

   if ( iPID < 100 )
   {
      log_softerror_and_alarm("Failed to set process affinity for process [%s], invalid pid: %d.", szProgName, iPID);
      return;
   }

   sprintf(szComm, "ls /proc/%d/task", iPID);
   hw_execute_bash_command_raw_silent(szComm, szOutput);
   
   if ( strlen(szOutput) < 3 )
   {
      log_softerror_and_alarm("Failed to set process affinity for process [%s], invalid tasks: [%s].", szProgName, szOutput);
      return;
   }

   for( int i=0; i<strlen(szOutput); i++ )
      if ( szOutput[i] == 10 || szOutput[i] == 13 )
         szOutput[i] = ' ';
   
   char* pTmp = &szOutput[0];
   while ( (*pTmp) == ' ' )
      pTmp++;

   log_line("Child processes to adjust affinity for, for process [%s] %d: [%s]", szProgName, iPID, szOutput);
   do
   {
       int iTask = 0;
       if ( 1 != sscanf(pTmp, "%d", &iTask) )
       {
          log_softerror_and_alarm("Failed to set process affinity for process [%s], invalid parsing tasks from [%s].", szProgName, pTmp);
          return;
       }
       if ( iTask < 100 )
       {
          log_softerror_and_alarm("Failed to set process affinity for process [%s], read invalid parsing tasks (%d) from [%s].", szProgName, iTask, pTmp);
          return;
       }

       // Set affinity
       if ( iCoreStart == iCoreEnd )
       {
          sprintf(szComm, "taskset -cp %d %d", iCoreStart-1, iTask);
          hw_execute_bash_command(szComm, NULL);
       }
       else
       {
          sprintf(szComm, "taskset -cp %d-%d %d", iCoreStart-1, iCoreEnd-1, iTask);
          hw_execute_bash_command(szComm, NULL);        
       }
       // Go to next task in string
       while ( (*pTmp) && (*pTmp != ' ') )
          pTmp++;

       while ( (*pTmp) && (*pTmp == ' ') )
          pTmp++;
   }
   while(*pTmp);
}


int hw_execute_bash_command(const char* command, char* outBuffer)
{
   log_line("Executing command: %s", command);
   if ( NULL != outBuffer )
      outBuffer[0] = 0;
   FILE* fp = popen( command, "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to execute command: %s", command);
      return 0;
   }
   if ( NULL != outBuffer )
   {
      char szBuff[10024];
      if ( fgets(szBuff, 10023, fp) != NULL)
      {
         //printf("\nbuffer:[%s]\n", szBuff);
         szBuff[1023] = 0;
         sscanf(szBuff, "%s", outBuffer);
      }
      else
         log_line("Empty response from command.");
   }
   if ( -1 == pclose(fp) )
      log_softerror_and_alarm("Failed to close command: %s", command);
   return 1;
}

int hw_execute_bash_command_raw(const char* command, char* outBuffer)
{
   log_line("Executing command: %s", command);
   if ( NULL != outBuffer )
      outBuffer[0] = 0;
   FILE* fp = popen( command, "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to execute command: %s", command);
      return 0;
   }
   if ( NULL != outBuffer )
   {
      *outBuffer = 0;
      char szBuff[10024];
      int lines = 0;
      szBuff[0] = 0;
      while ( fgets(szBuff, 10023, fp) != NULL )
      {
         szBuff[1023] = 0;
         if ( strlen(szBuff) + strlen(outBuffer) < 1023 )
            strcat(outBuffer, szBuff);
         lines++;
         szBuff[0] = 0;
      }
      if ( 0 == lines )
         log_line("No response lines. Empty response from command.");
   }
   if ( -1 == pclose(fp) )
      log_softerror_and_alarm("Failed to close command: %s", command);
   return 1;
}

int hw_execute_bash_command_raw_silent(const char* command, char* outBuffer)
{
   if ( NULL != outBuffer )
      outBuffer[0] = 0;
   FILE* fp = popen( command, "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to execute command: %s", command);
      return 0;
   }
   if ( NULL != outBuffer )
   {
      *outBuffer = 0;
      char szBuff[10024];
      int lines = 0;
      szBuff[0] = 0;
      while ( fgets(szBuff, 10023, fp) != NULL )
      {
         szBuff[1023] = 0;
         if ( strlen(szBuff) + strlen(outBuffer) < 1023 )
            strcat(outBuffer, szBuff);
         lines++;
         szBuff[0] = 0;
      }
   }
   if ( -1 == pclose(fp) )
      log_softerror_and_alarm("Failed to close command: %s", command);
   return 1;
}

int hw_execute_bash_command_silent(const char* command, char* outBuffer)
{
   if ( NULL != outBuffer )
      outBuffer[0] = 0;
   char szCommand[1024];
   sprintf(szCommand, "%s 2>/dev/null", command);
   FILE* fp = popen( szCommand, "r" );
   if ( NULL == fp )
      return 0;

   char szBuff[10024];
   if ( fgets(szBuff, 10023, fp) != NULL)
   {
      //printf("\nbuffer:[%s]\n", szBuff);
      szBuff[1023] = 0;
      if ( NULL != outBuffer )
         sscanf(szBuff, "%s", outBuffer);
   }
   pclose(fp);
   return 1;
}
