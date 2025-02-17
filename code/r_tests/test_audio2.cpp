#include <stdlib.h>
#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hw_procs.h"


int main(int argc, char *argv[])
{
   printf("\nTesting raw audio stream decode...\n");

   if ( argc < 2 )
   {
      printf("\nSpecify input raw file name.\n");
      return 0;

   }
   FILE* fp = fopen(argv[1], "rb");
   if ( NULL == fp )
   {
      printf("Failed to open input file %s\n", argv[1]);
      return 0;
   }

   char szKey[24];
   strcpy(szKey, "0123456789");
   szKey[10] = 10;
   szKey[11] = 0;
   int iKeyPositionToCheck = 0;

   char szComm[256];
   char szOutput[256];
   int iPipeOutput = -1;
   int iSegment = 0;
   FILE* fpOutput = NULL;
   char szBuff[256];
   sprintf(szBuff, "raw_output_segment%d.wav", iSegment);
   printf("Generated segment %s\n", szBuff);
   fpOutput = fopen(szBuff, "wb");


   //sprintf(szComm, "aplay -c 1 --rate 44100 --format S16_LE %s 2>/dev/null &", FIFO_RUBY_AUDIO1);
   sprintf(szComm, "aplay -c 1 --rate 16000 --format S16_BE %s 2>/dev/null &", FIFO_RUBY_AUDIO1);
   hw_execute_bash_command(szComm, NULL);
   iPipeOutput = open(FIFO_RUBY_AUDIO1, O_WRONLY);
   if ( iPipeOutput < 0 )
      printf("Failed to open output pipe.\n");
   else
      printf("Opened output pipe %d\n", iPipeOutput);

   u8 buffer[2000];
   int iTotalRead = 0;
   while ( true )
   {
      hardware_sleep_ms(10+(50*rand())/RAND_MAX);
      int iRead = fread(buffer, 1, 1024, fp);
      if ( iRead <= 0 )
      {
         printf("End of file. Read %d bytes.\n", iTotalRead);
         break;
      }

      int iBreakPosition = -1;
      for( int i=0; i<iRead; i++ )
      {
         if ( buffer[i] != szKey[iKeyPositionToCheck] )
         {
            iKeyPositionToCheck = 0;
            continue;
         }

         iKeyPositionToCheck++;
         if ( szKey[iKeyPositionToCheck] == 0 )
         {
            printf("Found key end at position %d\n", iTotalRead + iKeyPositionToCheck);
            iKeyPositionToCheck = 0;
            iBreakPosition = i;
            break;
         }
      }

      if ( -1 == iBreakPosition )
      {
         //fwrite(buffer, 1, iRead, fpOutput);
         if ( iPipeOutput > 0 )
            write(iPipeOutput, buffer, iRead);
         iTotalRead += iRead;
         continue;
      }

      // New segment

      if ( iBreakPosition > 0 )
      {
         int iToWrite = iBreakPosition-11;
         //if ( iToWrite > 0 )
         //   fwrite(buffer, 1, iToWrite, fpOutput);
      }

      if ( iPipeOutput > 0 )
      {
         close(iPipeOutput);
         iPipeOutput = -1;

         hardware_sleep_ms(1000);

         char szComm[256];
         
         hw_execute_bash_command("pidof aplay", szOutput);
         printf("aplay pid: [%s]", szOutput);
         if ( strlen(szOutput) > 2 )
         {
            sprintf(szComm, "kill -9 %s 2>/dev/null", szOutput);
            hw_execute_bash_command( szComm, NULL);
         }
         
         sprintf(szComm, "aplay -c 1 --rate 44100 --format S16_LE %s 2>/dev/null &", FIFO_RUBY_AUDIO1);
         hw_execute_bash_command(szComm, NULL);
         iPipeOutput = open(FIFO_RUBY_AUDIO1, O_WRONLY);
         if ( iPipeOutput < 0 )
            printf("Failed to open output pipe.\n");
         else
            printf("Opened output pipe %d\n", iPipeOutput);

      }
      fclose(fpOutput);
      iSegment++; 

      sprintf(szBuff, "raw_output_segment%d.wav", iSegment);
      printf("Generated segment %s\n", szBuff);
      fpOutput = fopen(szBuff, "wb");
      
      int iToWrite = iRead - iBreakPosition-1;
      if ( iToWrite > 0 )
      {
         fwrite(&buffer[iBreakPosition+1], 1, iToWrite, fpOutput);
         if ( iPipeOutput > 0 )
            write(iPipeOutput, &buffer[iBreakPosition+1], iToWrite);
      }
      iTotalRead += iRead;
   }

   fclose(fpOutput);
   fclose(fp);
   
   printf("Done\n");
   return (0);
} 
