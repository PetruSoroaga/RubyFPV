#include "../base/shared_mem.h"


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
   log_line("\nStarted.\n");
   int id = rand()%10000;
   int counter = 0;
   while ( ! bQuit )
   {
      counter++;
      log_line("%d counter value: %d", id, counter);
   }
   exit(0);
}
