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

#ifdef HW_CAPABILITY_GPIO

#ifdef HW_PLATFORM_RASPBERRY
#include "gpio_pi.c"
#endif

#ifdef HW_PLATFORM_RADXA_ZERO3
#include "gpio_radxa.c"
#endif

#else

int GPIOExport(int pin)
{
   return 0;
}

int GPIOUnexport(int pin)
{
  	return 0;
}

int GPIODirection(int pin, int dir)
{
  	return 0;
}

int GPIORead(int pin)
{
   return 0;
}

int GPIOWrite(int pin, int value)
{
  	return 0;
}

int GPIOGetButtonsPullDirection()
{
   return 1;
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

int GPIOButtonsResetDetectionFlag()
{
   return 1;
}


// Returns 0 on success, 1 on failure

int GPIOInitButtons()
{
   return 0;
}

int GPIOButtonsTryDetectPullUpDown()
{
   return 1;
}
#endif