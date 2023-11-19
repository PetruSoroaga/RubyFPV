/*
 * This program reads a single key, and then exits without echoing it.
 */

#include <stdio.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include "../base/base.h"

int main(void)
{
  int            count;
  int            jndex;
  int            result;

  char           in_buffer[80];

  struct termios tp1;
  struct termios tp2;

  log_init("TestKeyboard");
  log_line("\nStarted.\n");

  tcgetattr(0,&tp1);

  tp2=tp1;

  tp2.c_iflag&=~ICRNL;
  tp2.c_lflag&=~ICANON;
  tp2.c_lflag&=~ECHO;
  tp2.c_cc[VMIN ]=1;
  tp2.c_cc[VTIME]=0;
  tp2.c_cc[VINTR]=0xFF;
  tp2.c_cc[VSUSP]=0xFF;
  tp2.c_cc[VQUIT]=0xFF;

  tcsetattr(0,TCSANOW,&tp2);

  log_line("Start.");
  do
  {
    in_buffer[0]=0;

    count=read(0,in_buffer,1);
    log_line("CH");
    //log_line("%d %d", in_buffer[0], in_buffer[1]);

  } while(1);

  tcsetattr(0,TCSANOW,&tp1);

  result=in_buffer[0]&0xFF;

  return result;

} /* main(void) */
