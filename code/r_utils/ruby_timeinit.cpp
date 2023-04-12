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

#include "../base/base.h"
#include "../base/config.h"

int main(int argc, char *argv[])
{
   init_boot_timestamp();
   log_init("RubyTimeInit");
   long long t = 0;
   FILE* fd = fopen(FILE_BOOT_TIMESTAMP, "r");
   if ( NULL != fd )
   {
      fscanf(fd, "%lld\n", &t);
      fclose(fd);
      log_line("Initial timestamp: %lld", t);
   }
   else
      log_line("Failed to read initial timestamp.");
   log_line("TimeInit done.");
   return (0);
} 