#pragma once

// Select the platform to build for

//#define HW_PLATFORM_OPENIPC_CAMERA 1
//#define HW_PLATFORM_LINUX_GENERIC 1
#define HW_PLATFORM_RASPBERRY 1


// Configuration customisations for individual platforms 

#ifdef HW_PLATFORM_RASPBERRY

#define HW_CAPABILITY_GPIO 1
#define HW_CAPABILITY_I2C 1
#define HW_CAPABILITY_IONICE 1

#endif