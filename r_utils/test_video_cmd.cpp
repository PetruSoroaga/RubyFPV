#include <stdio.h>
#include <stdlib.h>

int main (int argc, char *argv[])
{
   if ( argc < 3 )
   {
      printf("Invalid params. Too few. Needs 2: command type and parameter.\n");
      return 0;
   }
   int iCommand = atoi(argv[argc-2]);
   int iParam = atoi(argv[argc-1]);
   printf("Exec command type %d, param %d\n", iCommand, iParam);

   FILE* fd = fopen("tmp.cmd", "wb");
   if ( NULL == fd )
   {
      printf("Failed to write command.\n");
      return 0;
   }
   fprintf(fd, "%d %d\n", iCommand, iParam);
   fclose(fd);
   system("chmod 777 tmp.cmd");
   printf("Done.\n");
   return 0;
}
