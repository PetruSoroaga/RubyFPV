#include "gpio.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


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

   bytes_written = snprintf(buffer, sizeof(buffer), "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   return(0);
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

	bytes_written = snprintf(buffer, sizeof(buffer), "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

int GPIODirection(int pin, int dir)
{
   static const char s_directions_str[]  = "in\0out";
   char path[64];
   int fd;

	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
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
	return(0);
}

int GPIORead(int pin)
{
   char path[64];
   char value_str[5];
   int fd;

	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
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
	return(atoi(value_str));
}

int GPIOWrite(int pin, int value)
{
	static const char s_values_str[] = "01";

	char path[64];
	int fd;

	snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
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
	return(0);
}
