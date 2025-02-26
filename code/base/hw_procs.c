#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <ctype.h>
#include <pthread.h>

#include "base.h"
#include "config.h"
#include "hw_procs.h"
#include "hardware.h"

int hw_process_exists(const char* szProcName)
{
   char szComm[128];
   char szPids[1024];
   int iPID = 0;
   if ( (NULL == szProcName) || (0 == szProcName[0]) )
      return 0;

   sprintf(szComm, "pidof %s", szProcName);
   hw_execute_bash_command_raw_silent(szComm, szPids);
   removeTrailingNewLines(szPids);
   char* p = removeLeadingWhiteSpace(szPids);
   if ( strlen(p) < 3 )
   {
      sprintf(szComm, "ps -aef | grep %s | grep -v \"grep\"", szProcName);
      hw_execute_bash_command_raw_silent(szComm, szPids);
      removeTrailingNewLines(szPids);
      p = removeLeadingWhiteSpace(szPids);
      if ( ! isdigit(*p) )
         return 0;
      if ( 1 != sscanf(p, "%d", &iPID) )
         return 0;
      if ( iPID < 100 )
         return 0;
      return iPID;
   }

   if ( ! isdigit(*p) )
      return 0;
   if ( 1 != sscanf(p, "%d", &iPID) )
      return 0;
   if ( iPID < 100 )
      return 0;
   return iPID;
}

char* hw_process_get_pid(const char* szProcName)
{
   static char s_szHWProcessPIDs[256];

   s_szHWProcessPIDs[0] = 0;

   if ( (NULL == szProcName) || (0 == szProcName[0]) )
      return s_szHWProcessPIDs;

   char szComm[128];
   sprintf(szComm, "pidof %s", szProcName);
   hw_execute_bash_command_silent(szComm, s_szHWProcessPIDs);
   removeTrailingNewLines(s_szHWProcessPIDs);
   return s_szHWProcessPIDs;
}

void hw_stop_process(const char* szProcName)
{
   char szComm[1024];
   char szPids[1024];

   if ( NULL == szProcName || 0 == szProcName[0] )
      return;

   log_line("Stopping process [%s]...", szProcName);
   
   sprintf(szComm, "pidof %s", szProcName);
   hw_execute_bash_command(szComm, szPids);
   removeTrailingNewLines(szPids);
   if ( strlen(szPids) > 2 )
   {
      log_line("Found PID(s) for process %s: %s", szProcName, szPids);
      sprintf(szComm, "kill $(pidof %s) 2>/dev/null", szProcName);
      hw_execute_bash_command(szComm, NULL);
      int retryCount = 30;
      sprintf(szComm, "pidof %s", szProcName);
      while ( retryCount > 0 )
      {
         hardware_sleep_ms(10);
         szPids[0] = 0;
         hw_execute_bash_command(szComm, szPids);
         removeTrailingNewLines(szPids);
         if ( strlen(szPids) < 2 )
         {
            log_line("Did stopped process %s", szProcName);
            return;
         }
         retryCount--;
      }
      sprintf(szComm, "kill -9 $(pidof %s) 2>/dev/null", szProcName);
      hw_execute_bash_command(szComm, NULL);
      hardware_sleep_ms(20);
   }
}


int hw_kill_process(const char* szProcName, int iSignal)
{
   char szCommStop[256];
   char szComm[256];
   char szPids[256];

   if ( (NULL == szProcName) || (0 == szProcName[0]) )
      return -1;

   sprintf(szCommStop, "kill %d $(pidof %s) 2>/dev/null", iSignal, szProcName);
   hw_execute_bash_command_raw(szCommStop, NULL);
   hardware_sleep_ms(20);

   sprintf(szComm, "pidof %s", szProcName);
   hw_execute_bash_command_raw(szComm, szPids);
   removeTrailingNewLines(szPids);
   if ( strlen(szPids) < 3 )
      return 1;

   log_line("Process still exists, %s pid is: %s", szProcName, szPids);

   int retryCount = 5;
   while ( retryCount > 0 )
   {
      hardware_sleep_ms(10);
      hw_execute_bash_command_raw(szCommStop, NULL);
      szPids[0] = 0;
      hw_execute_bash_command_raw(szComm, szPids);
      removeTrailingNewLines(szPids);
      if ( strlen(szPids) < 3 )
         return 1;
      log_line("Process still exists (%d), %s pid is: %s", retryCount, szProcName, szPids);
      retryCount--;
   }
   return 0;
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
   if ( (NULL == szParam1) && (NULL == szParam2) && (NULL == szParam3) && (NULL == szParam4) )
      sprintf(szBuff, "%s &", szFile);
   else if ( (NULL != szParam1) && (NULL == szParam2) && (NULL == szParam3) && (NULL == szParam4) )
      sprintf(szBuff, "%s %s &", szFile, szParam1);
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
   if ( (NULL != szParam4) && (0 < strlen(szParam4)) )
   {
      argv[4] = (char*)malloc(strlen(szParam4)+1);
      strcpy(argv[4], szParam4);
   }
   if ( (NULL != szParam3) && (0 < strlen(szParam3)) )
   {
      argv[3] = (char*)malloc(strlen(szParam3)+1);
      strcpy(argv[3], szParam3);
   }
   if ( (NULL != szParam2) && (0 < strlen(szParam2)) )
   {
      argv[2] = (char*)malloc(strlen(szParam2)+1);
      strcpy(argv[2], szParam2);
   }
   if ( (NULL != szParam1) && (0 < strlen(szParam1)) )
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
   removeTrailingNewLines(szPids);
   while ( waitForProcess && (strlen(szPids) <= 2) && (count < 100) )
   {
      hardware_sleep_ms(2);
      szPids[0] = 0;
      hw_execute_bash_command_silent(szComm, szPids);
      removeTrailingNewLines(szPids);
      count++;
   }

   if ( strlen(szPids) <= 2 )
      return;
   sprintf(szComm, "renice -n %d -p %s", nice, szPids);
   hw_execute_bash_command(szComm, NULL);

   #ifdef HW_CAPABILITY_IONICE
   if ( ionice > 0 )
   {
      sprintf(szComm, "ionice -c 1 -n %d -p %s", ionice, szPids);
      hw_execute_bash_command(szComm, NULL);
   }
   //else
   //   sprintf(szComm, "ionice -c 2 -n 5 -p %s", szPids);
   #endif
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

   #ifdef HW_CAPABILITY_IONICE
   strcat(szOutput, ", io priority: ");

   sprintf(szComm, "ionice -p %s", szPids);
   hw_execute_bash_command_raw(szComm, szCommOut);
   if ( 0 < strlen(szCommOut) )
      szCommOut[strlen(szCommOut)-1] = 0;
   strcat(szOutput, szCommOut);
   #endif
   strcat(szOutput, ";");
}

void hw_set_proc_affinity(const char* szProgName, int iExceptThreadId, int iCoreStart, int iCoreEnd)
{
   if ( NULL == szProgName || 0 == szProgName[0] )
   {
      log_softerror_and_alarm("Tried to adjus affinity for NULL process");
      return;
   }
   log_line("Adjusting affinity for process [%s] except thread id: %d ...", szProgName, iExceptThreadId);

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

   sprintf(szComm, "ls /proc/%d/task 2>/dev/null", iPID);
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

       if ( iTask == iExceptThreadId )
          log_line("Skip exception thread id: %d", iExceptThreadId);
       else
       {
          // Set affinity
          if ( iCoreStart == iCoreEnd )
          {
             sprintf(szComm, "taskset -cp %d %d 2>/dev/null", iCoreStart-1, iTask);
             hw_execute_bash_command(szComm, NULL);
          }
          else
          {
             sprintf(szComm, "taskset -cp %d-%d %d", iCoreStart-1, iCoreEnd-1, iTask);
             hw_execute_bash_command(szComm, NULL);        
          }
       }
       // Go to next task in string
       while ( (*pTmp) && (*pTmp != ' ') )
          pTmp++;

       while ( (*pTmp) == ' ' )
          pTmp++;
   }
   while(*pTmp);

   log_line("Done adjusting affinity for process [%s].", szProgName);
}


int _hw_execute_bash_command(const char* command, char* outBuffer, int iSilent, u32 uTimeoutMs)
{
   if ( NULL != outBuffer )
      *outBuffer = 0;
   FILE* fp = popen( command, "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to execute command: %s", command);
      return 0;
   }
   if ( uTimeoutMs == 0 )
      uTimeoutMs = 5;
   char szBuff[2048];
   u32 uTimeStart = get_current_timestamp_ms();
   int iCountRead = 0;
   int iRead = 0;
   char* pOut = outBuffer;
   while ( 1 )
   {
      if ( ferror(fp) || feof(fp) )
         break;

      iRead = fread(szBuff, 1, 1022, fp);
      if ( iRead > 0 )
      {
         iCountRead += iRead;
         if ( (NULL != pOut) && (iCountRead < 4094) )
         {
            szBuff[iRead] = 0;
            memcpy(pOut, szBuff, iRead+1);
            pOut += iRead;
         }
      }
      if ( get_current_timestamp_ms() >= uTimeStart + uTimeoutMs )
      {
         log_line("Abandoning reading start process output.");
         break;
      }
   }

   if ( 0 == iSilent )
   {
      if ( (iCountRead < 20) && (NULL != outBuffer) )
      {
         char szTmp[24];
         strncpy(szTmp, outBuffer, 23);
         szTmp[23] = 0;
         removeTrailingNewLines(szTmp);
         log_line("Read process output: %d bytes in %u ms. Content: [%s]", iCountRead, get_current_timestamp_ms() - uTimeStart, szTmp);
      }
      else
         log_line("Read process output: %d bytes in %u ms", iCountRead, get_current_timestamp_ms() - uTimeStart);
   }
   if ( -1 == pclose(fp) )
   {
      log_softerror_and_alarm("Failed to close command: %s", command);
      return 0;
   }
   return 1;
}

int hw_execute_bash_command_nonblock(const char* command, char* outBuffer)
{
   if ( NULL != outBuffer )
      *outBuffer = 0;

   if ( (NULL == command) || (0 == command[0]) )
      return 0;

   log_line("Executing command nonblock: %s", command);
   FILE* fp = popen( command, "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to execute command: [%s]", command);
      return 0;
   }

   if ( NULL != outBuffer )
   {
      char szBuff[256];
      if ( fgets(szBuff, 254, fp) != NULL)
      {
         szBuff[254] = 0;
         strcpy(outBuffer, szBuff);
      }
      else
         log_line("Empty response from executing command.");
   }
   if ( -1 == pclose(fp) )
   {
      log_softerror_and_alarm("Failed to execute command: [%s]", command);
      return 0;
   }

   log_line("Executed command nonblock: [%s]", command);
   return 0;
}

int hw_execute_bash_command(const char* command, char* outBuffer)
{
   log_line("Executing command: %s", command);
   return _hw_execute_bash_command(command, outBuffer, 0, 3000);
}

int hw_execute_bash_command_timeout(const char* command, char* outBuffer, u32 uTimeoutMs)
{
   log_line("Executing command (timeout: %u ms): %s", uTimeoutMs, command);
   return _hw_execute_bash_command(command, outBuffer, 0, uTimeoutMs);
}

int hw_execute_bash_command_silent(const char* command, char* outBuffer)
{
   if ( (NULL == command) || (0 == command[0]) )
      return 0;
   char szCommand[1024];
   int iLen = strlen(command);

   if ( NULL == strstr(command, "2>/dev/null") )
   {
      if ( command[iLen-1] == '&' )
      {
         char szTmp[1024];
         strncpy(szTmp, command, sizeof(szTmp)/sizeof(szTmp[0]));
         szTmp[iLen-1] = 0;
         if ( command[iLen-2] == ' ' )
            szTmp[iLen-2] = 0;
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "%s 2>/dev/null &", szTmp);
      }
      else
         snprintf(szCommand, sizeof(szCommand)/sizeof(szCommand[0]), "%s 2>/dev/null", command);
   }
   else
      strncpy(szCommand, command, sizeof(szCommand)/sizeof(szCommand[0]));

   return _hw_execute_bash_command(szCommand, outBuffer, 1, 3000);
}

int hw_execute_bash_command_raw(const char* command, char* outBuffer)
{
   log_line("Executing command raw: %s", command);
   return _hw_execute_bash_command(command, outBuffer, 0, 3000);
}

int hw_execute_bash_command_raw_timeout(const char* command, char* outBuffer, u32 uTimeoutMs)
{
   log_line("Executing command raw timeout %u ms: %s", uTimeoutMs, command);
   return _hw_execute_bash_command(command, outBuffer, 0, uTimeoutMs); 
}

int hw_execute_bash_command_raw_silent(const char* command, char* outBuffer)
{
   return _hw_execute_bash_command(command, outBuffer, 1, 3000);
}

void hw_execute_ruby_process(const char* szPrefixes, const char* szProcess, const char* szParams, char* szOutput)
{
   hw_execute_ruby_process_wait(szPrefixes, szProcess, szParams, szOutput, 0);
}

void hw_execute_ruby_process_wait(const char* szPrefixes, const char* szProcess, const char* szParams, char* szOutput, int iWait)
{
   if ( (NULL == szProcess) || (0 == szProcess[0]) )
      return;
   if ( (NULL != szPrefixes) && (0 != szPrefixes[0]) )
      log_line("Executing Ruby process: [%s], prefixes: [%s], params: [%s], wait: %s", szProcess, szPrefixes, ((NULL != szParams)?szParams:"None"), (iWait?"yes":"no"));
   else
      log_line("Executing Ruby process: [%s], no prefixes, params: [%s], wait: %s", szProcess, ((NULL != szParams)?szParams:"None"), (iWait?"yes":"no"));

   if ( NULL != szOutput )
      szOutput[0] = 0;
   
   char szFullPath[MAX_FILE_PATH_SIZE];
   char szFullPathDebug[MAX_FILE_PATH_SIZE];
   szFullPathDebug[0] = 0;
   strcpy(szFullPath, szProcess);
   if ( access(szFullPath, R_OK) == -1 )
      sprintf(szFullPath, "%s%s", FOLDER_BINARIES, szProcess);

   if ( access("/tmp/debug", R_OK) != -1 )
   {
      sprintf(szFullPathDebug, "/tmp/%s", szProcess);
      if ( access(szFullPathDebug, R_OK) != -1 )
         strcpy(szFullPath, szFullPathDebug);
   }
   if ( access(szFullPath, R_OK) == -1 )
   {
      log_error_and_alarm("Can't execute Ruby process. Not found here: [%s] or here: [%s]", szProcess, szFullPath );
      return;
   }

   char szCommand[256];
   szCommand[0] = 0;
   if ( (NULL != szPrefixes) && (0 != szPrefixes[0]) )
   {
      strcpy(szCommand, szPrefixes);
      strcat(szCommand, " ");
   }
   if ( szFullPath[0] != '/' )
     strcat(szCommand, "./");
   strcat(szCommand, szFullPath);

   if ( (NULL != szParams) && (0 != szParams[0]) )
   {
      strcat(szCommand, " ");
      strcat(szCommand, szParams);
   }

   if ( ! iWait )
      strcat(szCommand, "&");

   FILE* fp = popen( szCommand, "r" );
   if ( NULL == fp )
   {
      log_error_and_alarm("Failed to execute Ruby process: [%s]", szCommand);
      return;
   }
   if ( NULL != szOutput )
   {
      // Ruby processes output very little info (version only)
      char szBuff[256];
      if ( fgets(szBuff, 254, fp) != NULL)
      {
         szBuff[254] = 0;
         strcpy(szOutput, szBuff);
      }
      else
         log_line("Empty response from Ruby process.");
   }
   if ( -1 == pclose(fp) )
      log_softerror_and_alarm("Failed to launch and confirm Ruby process: [%s]", szCommand);
   else
      log_line("Launched Ruby process: [%s]", szCommand);
}

// Returns current priority
int hw_get_current_thread_priority(const char* szLogPrefix)
{
   char szTmp[2];
   szTmp[0] = 0;
   char* szPrefix = szTmp;
   if ( (NULL != szLogPrefix) && (0 != szLogPrefix[0]) )
     szPrefix = (char*)szLogPrefix;

   pthread_t this_thread = pthread_self();
   struct sched_param params;
   int policy = 0;
   int iRetValue = -1;
   int ret = 0;
   ret = pthread_getschedparam(this_thread, &policy, &params);
   if ( ret != 0 )
     log_softerror_and_alarm("%s Failed to get schedule param", szPrefix);
   else
   {
      iRetValue = params.sched_priority;
      log_line("%s Current thread policy/priority: %d/%d", szPrefix, policy, params.sched_priority);
   }
   
   return iRetValue;
}

// Returns previous priority or -1 for error
int hw_increase_current_thread_priority(const char* szLogPrefix, int iNewPriority)
{
   int iRetValue = -1;
   char szTmp[2];
   szTmp[0] = 0;
   char* szPrefix = szTmp;
   if ( (NULL != szLogPrefix) && (0 != szLogPrefix[0]) )
     szPrefix = (char*)szLogPrefix;

   pthread_t this_thread = pthread_self();
   struct sched_param params;
   int policy = 0;
   int ret = 0;
   ret = pthread_getschedparam(this_thread, &policy, &params);
   if ( ret != 0 )
     log_softerror_and_alarm("%s Failed to get schedule param", szPrefix);
   else
   {
      iRetValue = params.sched_priority;
      log_line("%s Current thread policy/priority: %d/%d", szPrefix, policy, params.sched_priority);
   }
   params.sched_priority = iNewPriority;
   ret = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
   if ( ret != 0 )
      log_softerror_and_alarm("%s Failed to set thread schedule class, error: %d, %s", szPrefix, errno, strerror(errno));

   ret = pthread_getschedparam(this_thread, &policy, &params);
   if ( ret != 0 )
     log_softerror_and_alarm("%s Failed to get schedule param", szPrefix);
   log_line("%s Current new thread policy/priority: %d/%d", szPrefix, policy, params.sched_priority);

   return iRetValue;
}