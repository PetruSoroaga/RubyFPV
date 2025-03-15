#include "../base/shared_mem.h"
#include "../public/utils/core_plugins_utils.h"
#include "../base/hw_procs.h"
#include <time.h>
#include <sys/resource.h>

bool bQuit = false;

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   bQuit = true;
} 
  
int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   srand(rand()+get_current_timestamp_micros());
   log_init("TestLOG");
   log_enable_stdout();
   log_line("\nStarted.\n");


   while ( ! bQuit )
   {
      int iPID = hw_process_exists("ruby_central");
      if ( iPID > 0 )
         log_line("Proc exists: %d", iPID);
      else
         log_softerror_and_alarm("Proc does not exists !!!! (result: %d)", iPID);
      hardware_sleep_ms(100);
   }
   int id = rand()%10000;
   int counter = 0;
   core_plugin_util_log_line("TestLog core");
   counter++;
   log_line("%d counter value: %d", id, counter);
   exit(0);
}
