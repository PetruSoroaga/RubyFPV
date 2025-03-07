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
#include <gpiod.h>

static int s_iGPIOButtonsDirectionDetected = 1;
static int s_iGPIOButtonsPullDirection = 0;


int GPIOExport(int pin)
{
   //if ( pin <= 0 )
   //   return 0;
   //char buffer[6];
   //ssize_t bytes_written;
   //int fd;

   //fd = open("/sys/class/gpio/export", O_WRONLY);
   //if (-1 == fd)
   //{
   //   //fprintf(stderr, "Failed to open export for writing!\n");
   //   return(-1);
   //}

   //bytes_written = snprintf(buffer, 6, "%d", pin);
   //write(fd, buffer, bytes_written);
   //close(fd);
   return 0;
}

int GPIOUnexport(int pin)
{
   //if ( pin <= 0 )
   //   return 0;
   //char buffer[6];
   //ssize_t bytes_written;
   //int fd;

   //fd = open("/sys/class/gpio/unexport", O_WRONLY);
   //if (-1 == fd) {
   // //fprintf(stderr, "Failed to open unexport for writing!\n");
   // return(-1);
   //}

   //bytes_written = snprintf(buffer, 6, "%d", pin);
   //write(fd, buffer, bytes_written);
   //close(fd);
   return 0;
}

int GPIODirection(int pin, int dir)
{
   //if ( pin <= 0 )
   //   return 0;
   //static const char s_directions_str[]  = "in\0out";
   //char path[64];
   //int fd;

   //snprintf(path, 64, "/sys/class/gpio/gpio%d/direction", pin);
   //fd = open(path, O_WRONLY);
   //if (-1 == fd) {
   // //fprintf(stderr, "Failed to open gpio direction for writing!\n");
   // return(-1);
   //}

   //if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
   // //fprintf(stderr, "Failed to set direction!\n");
   // return(-1);
   //}

   //close(fd);
   return 0;
}

int GPIORead(int pin)
{
   if ( pin <= 0 )
      return -1;

   char chipname[16];
   int linenumber;
   Convert_Pin_To_Chip_Line(pin, &chipname, &linenumber);
   struct gpiod_chip *chip = gpiod_chip_open_by_name(chipname);
   if (!chip) {
      log_error_and_alarm("[GPIO] Failed to open chip %s\n", chipname);
      return -1;
   }
   struct gpiod_line *line;
   line = gpiod_chip_get_line(chip, linenumber);
   if (!line) {
      log_error_and_alarm("[GPIO] Failed to get chip %s line %s\n",chipname, linenumber);
      return 0;
	}
   
   int mode = gpiod_line_request_input(line, "Ruby");
   if(mode < 0) {
      log_error_and_alarm("[GPIO] Failed to set input mode\n");
      return 0;
   }
   int value = gpiod_line_get_value(line);
   if (value < 0) {
      printf("[GPIO] Set chip %s line %d input failed.\n", chipname, linenumber);
      gpiod_chip_close(chip);
      return 0;
   }
   gpiod_chip_close(chip);
   gpiod_line_release(line);
   return value;
}

int GPIOWrite(int pin, int value)
{
   if ( pin <= 0 )
      return 0;

   char chipname[16];
   int linenumber;
   Convert_Pin_To_Chip_Line(pin, &chipname, &linenumber);
   struct gpiod_chip *chip = gpiod_chip_open_by_name(chipname);
   if (!chip) {
      log_error_and_alarm("[GPIO] Failed to open chip %s", chipname);
      return -1;
   }
   struct gpiod_line *line;
   line = gpiod_chip_get_line(chip, linenumber);
   if (!line) {
      log_error_and_alarm("[GPIO] Failed to get chip %s line %s",chipname, linenumber);
      return 0;
	}
   
   int mode = gpiod_line_request_output(line, "Ruby", 0);
   if(mode < 0) {
      log_error_and_alarm("[GPIO] Failed to set output mode");
      return 0;
   }
   int ret = gpiod_line_set_value(line, value);
   if (ret < 0) {
      printf("[GPIO] Set chip %s line %d output failed. value: %u\n", chipname, linenumber,value);
      gpiod_chip_close(chip);
      return 0;
   }
   gpiod_chip_close(chip);
   gpiod_line_release(line);
   return 0;
}

int GPIOGetButtonsPullDirection()
{
   return s_iGPIOButtonsPullDirection;
}

int _GPIOTryPullUpDown(int iPin, int iPullDirection)
{
   return 0;
}


void _GPIO_PullAllDown()
{
}

void _GPIO_PullAllUp()
{
}

// Returns 0 on success, 1 on failure

int GPIOInitButtons()
{
   _gpio_load_custom_mapping();
   log_line("[GPIO] Export and initialize buttons (for Radxa)...");

   s_iGPIOButtonsDirectionDetected = 1;
   s_iGPIOButtonsPullDirection = 0;

   int failed = 0;
   if (-1 == GPIOExport(GPIOGetPinMenu()) || -1 == GPIOExport(GPIOGetPinBack()))
   {
      log_line("Failed to get GPIO access to pin Menu/Back.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIOGetPinPlus()) || -1 == GPIOExport(GPIOGetPinMinus()))
   {
      log_line("Failed to get GPIO access to pin Plus/Minus.");
      failed = 1;
   }

   if (-1 == GPIOExport(GPIOGetPinQA1()))
   {
      log_line("Failed to get GPIO access to pin QA1.");
      failed = 1;
   }
   if (-1 == GPIOExport(GPIOGetPinQA2()))
   {
      log_line("Failed to get GPIO access to pin QA2.");
      failed = 1;
   }
   
   if (-1 == GPIOExport(GPIOGetPinQA3()))
   {
      log_line("Failed to get GPIO access to pin QA3.");
      failed = 1;
   }

   /*
   if (-1 == GPIOExport(GPIOGetPinQAPlus()))
   {
      log_line("Failed to get GPIO access to pin QAPLUS.");
      failed = 1;
   }

   if (-1 == GPIOExport(GPIOGetPinQAMinus()))
   {
      log_line("Failed to get GPIO access to pin QAMINUS.");
      failed = 1;
   }
   */

   if (-1 == GPIODirection(GPIOGetPinMenu(), IN) || -1 == GPIODirection(GPIOGetPinBack(), IN))
   {
      log_line("Failed set GPIO configuration for pin Menu/Back.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIOGetPinPlus(), IN) || -1 == GPIODirection(GPIOGetPinMinus(), IN))
   {
      log_line("Failed set GPIO configuration for pin Plus/Minus.");
      failed = 1;
   }
   
   if (-1 == GPIODirection(GPIOGetPinQA1(), IN))
   {
      log_line("Failed set GPIO configuration for pin QA1.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIOGetPinQA2(), IN))
   {
      log_line("Failed set GPIO configuration for pin QA2.");
      failed = 1;
   }
   
   if (-1 == GPIODirection(GPIOGetPinQA3(), IN))
   {
      log_line("Failed set GPIO configuration for pin QA3.");
      failed = 1;
   }

   /*
   if (-1 == GPIODirection(GPIOGetPinQAPlus(), IN))
   {
      log_line("Failed set GPIO configuration for pin QAACTIONPLUS.");
      failed = 1;
   }

   if (-1 == GPIODirection(GPIOGetPinQAMinus(), IN))
   {
      log_line("Failed set GPIO configuration for pin QAACTIONMINUS.");
      failed = 1;
   }
   */

   return failed;
}

int Convert_Pin_To_Chip_Line(int pin, char *chipname, int *line) {
   int chip = pin / 32;
   snprintf(chipname, 16, "gpiochip%d", chip);
   *line = pin % 32;
   return 0;
}