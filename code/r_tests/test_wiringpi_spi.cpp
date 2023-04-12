#include <iostream>
#include <errno.h>
#include <wiringPiSPI.h>
#include <unistd.h>
#include <time.h>

using namespace std;

// channel is the wiringPi name for the chip select (or chip enable) pin.
// Set this to 0 or 1, depending on how it's connected.
static const int CHANNEL = 0;

int main()
{
   int fd, result;
   unsigned char buffer[20];

   cout << "Initializing" << endl ;

   // Configure the interface.
   // CHANNEL insicates chip select,
   // 500000 indicates bus speed.
   fd = wiringPiSPISetup(CHANNEL, 500000);

   cout << "Init result: " << fd << endl;

   for( int i=0; i<20; i++ )
      buffer[i] = i;

   int i=0;

   while(1)
   {
      i++;
      buffer[0] = i%256;
      buffer[1] = i/256;
      result = wiringPiSPIDataRW(CHANNEL, buffer, 2);
      printf("\nSent: %d, Recv: %d,%d\n", i, buffer[0], buffer[1]);
      //nanosleep((const struct timespec[]){{0, 500000000L}}, NULL);
      nanosleep((const struct timespec[]){{0, 5000000L}}, NULL);
   }
}