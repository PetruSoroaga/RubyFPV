#include "core_plugins_utils.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#ifdef __cplusplus
extern "C" {
#endif 

typedef struct
{
    long type;
    char text[300];
} type_log_message_buffer_core_plugin;

static int s_logServiceMessageQueueCorePlugin = -1;

void core_plugin_util_log_line(const char* szLine)
{
   if ( 0 == szLine || 0 == szLine[0] )
      return;

   if ( -1 == s_logServiceMessageQueueCorePlugin )
   {
      key_t key;
      key = ftok("ruby_logger", 123);
   
      s_logServiceMessageQueueCorePlugin = msgget(key, 0222);
   }

   if ( -1 == s_logServiceMessageQueueCorePlugin )
      return;

   type_log_message_buffer_core_plugin msg;
   msg.type = 1;
   msg.text[0] = 0;


   strcpy(msg.text, "S-CorePlugin");
   int len = strlcat(msg.text, ": ", sizeof(msg.text));

   if ( strlen(szLine) > 300 - 2 - len )
   {
      char szTmp[300];
      strlcpy(szTmp, szLine, sizeof(szLine));
      szTmp[300 - 2 - len] = 0;
      strlcat(msg.text, szTmp, sizeof(msg.text));
   }
   else
      strlcat(msg.text, szLine, sizeof(msg.text));

   msgsnd(s_logServiceMessageQueueCorePlugin, &msg, sizeof(msg), 0);  
}

#ifdef __cplusplus
}  
#endif 

