#include <stdio.h>
#include <wiringPi.h>

int main (void)
{
  wiringPiSetup();
  int pin = 22;

  pinMode (pin, INPUT) ;
  pullUpDnControl(pin, PUD_DOWN);
  //pinMode(pin, OUTPUT);
  for (;;)
  {
    printf("%d\n", digitalRead(pin));
    delay (500) ;
    //digitalWrite (pin, HIGH) ; delay (500) ;
    //digitalWrite (pin,  LOW) ; delay (500) ;
  }
  return 0 ;
}