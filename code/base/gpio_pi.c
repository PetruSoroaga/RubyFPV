#include "base.h"
#include "gpio.h"
#include "hw_procs.h"
#include "config_hw.h"
#include "config_file_names.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int s_iGPIOButtonsDirectionDetected = 0;
static int s_iGPIOButtonsPullDirection = 1;


int GPIOExport(int pin)
{
   char buffer[6];
   ssize_t bytes_written;
   int fd;

   fd = open("/sys/class/gpio/export", O_WRONLY);
   if (-1 == fd)
   {
      //fprintf(stderr, "Failed to open export for writing!\n");
      return(-1);
   }

   bytes_written = snprintf(buffer, 6, "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   return 0;
}

int GPIOUnexport(int pin)
{
   char buffer[6];
   ssize_t bytes_written;
   int fd;

   fd = open("/sys/class/gpio/unexport", O_WRONLY);
   if (-1 == fd) {
    //fprintf(stderr, "Failed to open unexport for writing!\n");
    return(-1);
   }

   bytes_written = snprintf(buffer, 6, "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   return 0;
}

int GPIODirection(int pin, int dir)
{
   static const char s_directions_str[]  = "in\0out";
   char path[64];
   int fd;

   snprintf(path, 64, "/sys/class/gpio/gpio%d/direction", pin);
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
    //fprintf(stderr, "Failed to open gpio direction for writing!\n");
    return(-1);
   }

   if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
    //fprintf(stderr, "Failed to set direction!\n");
    return(-1);
   }

   close(fd);
   return 0;
}

int GPIORead(int pin)
{
   char path[64];
   char value_str[5];
   int fd;

   snprintf(path, 64, "/sys/class/gpio/gpio%d/value", pin);
   fd = open(path, O_RDONLY);
   if (-1 == fd) {
    //fprintf(stderr, "Failed to open gpio value for reading!\n");
    return(-1);
   }

   int ir = read(fd, value_str, 3);
          if ( ir == -1 ) {
    //fprintf(stderr, "Failed to read value!\n");
    return(-1);
   }
          value_str[ir] = 0;
   close(fd);
   return atoi(value_str);
}

int GPIOWrite(int pin, int value)
{
   static const char s_values_str[] = "01";

   char path[64];
   int fd;

   snprintf(path, 64, "/sys/class/gpio/gpio%d/value", pin);
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
    //fprintf(stderr, "Failed to open gpio value for writing!\n");
    return(-1);
   }

   if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
    //fprintf(stderr, "Failed to write value!\n");
    return(-1);
   }

   close(fd);
   return 0;
}

int GPIOGetButtonsPullDirection()
{
   return s_iGPIOButtonsPullDirection;
}

int _GPIOTryPullUpDown(int iPin, int iPullDirection)
{
   int failed = 0;
   if (-1 == GPIODirection(iPin, OUT))
   {
      log_line("Failed set GPIO pin %d to output", iPin);
      failed = 1;
   }

   if ( iPullDirection == 0 )
   {
      if ( 0 != GPIOWrite(iPin, LOW) )
         log_line("Failed to pull down pin %d", iPin);
   }
   else
   {
      if ( 0 != GPIOWrite(iPin, HIGH) )
         log_line("Failed to pull up pin %d", iPin);
   }

   if (-1 == GPIODirection(iPin, IN))
   {
      log_line("Failed set GPIO pin %d to input.", iPin);
      failed = 1;
   }
   
   char szComm[64];
   sprintf(szComm, "raspi-gpio set %d ip %s 2>&1", iPin, (iPullDirection==0)?"pd":"pu");
   hw_execute_bash_command_silent(szComm, NULL);

   sprintf(szComm, "gpio -g mode %d in", iPin);
   hw_execute_bash_command_silent(szComm, NULL);
   sprintf(szComm, "gpio -g mode %d %s", iPin, (iPullDirection==0)?"down":"up");
   hw_execute_bash_command_silent(szComm, NULL);

   return failed;
}


void _GPIO_PullAllDown()
{
   char szBuff[64];
   _GPIOTryPullUpDown(GPIO_PIN_BACK, 0);
   _GPIOTryPullUpDown(GPIO_PIN_PLUS, 0);
   _GPIOTryPullUpDown(GPIO_PIN_MINUS, 0);
   _GPIOTryPullUpDown(GPIO_PIN_QACTION1, 0);
   _GPIOTryPullUpDown(GPIO_PIN_QACTION2, 0);
   _GPIOTryPullUpDown(GPIO_PIN_QACTION2_2, 0);
   _GPIOTryPullUpDown(GPIO_PIN_QACTION3, 0);
   _GPIOTryPullUpDown(GPIO_PIN_QACTIONPLUS, 0);
   _GPIOTryPullUpDown(GPIO_PIN_QACTIONMINUS, 0);

   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_MENU);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIO_PIN_MENU);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_BACK);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIO_PIN_BACK);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_PLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIO_PIN_PLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_MINUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIO_PIN_MINUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTION1);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIO_PIN_QACTION1);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTION2);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIO_PIN_QACTION2);
   hw_execute_bash_command_silent(szBuff, NULL);

   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTION2_2);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIO_PIN_QACTION2_2);
   hw_execute_bash_command_silent(szBuff, NULL);

   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTION3);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIO_PIN_QACTION3);
   hw_execute_bash_command_silent(szBuff, NULL);

   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTIONPLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIO_PIN_QACTIONPLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTIONMINUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d down", GPIO_PIN_QACTIONMINUS);
   hw_execute_bash_command_silent(szBuff, NULL);

   log_line("[GPIO] Pulled all buttons down");
}

void _GPIO_PullAllUp()
{
   char szBuff[64];
   _GPIOTryPullUpDown(GPIO_PIN_BACK, 1);
   _GPIOTryPullUpDown(GPIO_PIN_PLUS, 1);
   _GPIOTryPullUpDown(GPIO_PIN_MINUS, 1);
   _GPIOTryPullUpDown(GPIO_PIN_QACTION1, 1);
   _GPIOTryPullUpDown(GPIO_PIN_QACTION2, 1);
   _GPIOTryPullUpDown(GPIO_PIN_QACTION2_2, 1);
   _GPIOTryPullUpDown(GPIO_PIN_QACTION3, 1);
   _GPIOTryPullUpDown(GPIO_PIN_QACTIONPLUS, 1);
   _GPIOTryPullUpDown(GPIO_PIN_QACTIONMINUS, 1);

   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_MENU);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIO_PIN_MENU);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_BACK);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIO_PIN_BACK);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_PLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIO_PIN_PLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_MINUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIO_PIN_MINUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTION1);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIO_PIN_QACTION1);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTION2);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIO_PIN_QACTION2);
   hw_execute_bash_command_silent(szBuff, NULL);

   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTION2_2);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIO_PIN_QACTION2_2);
   hw_execute_bash_command_silent(szBuff, NULL);

   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTION3);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIO_PIN_QACTION3);
   hw_execute_bash_command_silent(szBuff, NULL);

   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTIONPLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIO_PIN_QACTIONPLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d in", GPIO_PIN_QACTIONMINUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   sprintf(szBuff, "gpio -g mode %d up", GPIO_PIN_QACTIONMINUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   log_line("[GPIO] Pulled all buttons up");
}

// Returns 0 on success, 1 on failure

int GPIOInitButtons()
{
   log_line("[GPIO] Export and initialize buttons...");

   s_iGPIOButtonsDirectionDetected = 1;
   s_iGPIOButtonsPullDirection = 0;

   int failed = 0;
   if (-1 == GPIOExport(GPIO_PIN_MENU) || -1 == GPIOExport(GPIO_PIN_BACK))
   {
      log_line("Failed to get GPIO access to pin Menu/Back.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIO_PIN_PLUS) || -1 == GPIOExport(GPIO_PIN_MINUS))
   {
      log_line("Failed to get GPIO access to pin Plus/Minus.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIO_PIN_QACTION1))
   {
      log_line("Failed to get GPIO access to pin QA1.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIO_PIN_QACTION2))
   {
      log_line("Failed to get GPIO access to pin QA2.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIO_PIN_QACTION2_2))
   {
      log_line("Failed to get GPIO access to pin QA2_2.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIO_PIN_QACTION3))
   {
      log_line("Failed to get GPIO access to pin QA3.");
      failed = 1;
   }

   if (-1 == GPIOExport(GPIO_PIN_QACTIONPLUS))
   {
      log_line("Failed to get GPIO access to pin QAPLUS.");
      failed = 1;
   }

   if (-1 == GPIOExport(GPIO_PIN_QACTIONMINUS))
   {
      log_line("Failed to get GPIO access to pin QAMINUS.");
      failed = 1;
   }

   if (-1 == GPIODirection(GPIO_PIN_MENU, IN) || -1 == GPIODirection(GPIO_PIN_BACK, IN))
   {
      log_line("Failed set GPIO configuration for pin Menu/Back.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIO_PIN_PLUS, IN) || -1 == GPIODirection(GPIO_PIN_MINUS, IN))
   {
      log_line("Failed set GPIO configuration for pin Plus/Minus.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIO_PIN_QACTION1, IN))
   {
      log_line("Failed set GPIO configuration for pin QA1.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIO_PIN_QACTION2, IN))
   {
      log_line("Failed set GPIO configuration for pin QA2.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIO_PIN_QACTION2_2, IN))
   {
      log_line("Failed set GPIO configuration for pin QA2_2.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIO_PIN_QACTION3, IN))
   {
      log_line("Failed set GPIO configuration for pin QA3.");
      failed = 1;
   }

   if (-1 == GPIODirection(GPIO_PIN_QACTIONPLUS, IN))
   {
      log_line("Failed set GPIO configuration for pin QAACTIONPLUS.");
      failed = 1;
   }

   if (-1 == GPIODirection(GPIO_PIN_QACTIONMINUS, IN))
   {
      log_line("Failed set GPIO configuration for pin QAACTIONMINUS.");
      failed = 1;
   }

   char szComm[64];
   
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIO_PIN_MENU);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIO_PIN_BACK);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIO_PIN_PLUS);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIO_PIN_MINUS);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIO_PIN_QACTION1);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIO_PIN_QACTION2);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIO_PIN_QACTION2_2);
   hw_execute_bash_command(szComm, NULL);
   sprintf(szComm, "raspi-gpio set %d ip 2>&1", GPIO_PIN_QACTION3);
   hw_execute_bash_command(szComm, NULL);

   _GPIO_PullAllDown();

   return failed;
}

