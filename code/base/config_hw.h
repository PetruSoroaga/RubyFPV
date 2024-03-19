#pragma once

//-------------------------------------------------------------
// The platform to build for can be forced by the makefile using the define below:
#ifdef RUBY_BUILD_HW_PLATFORM_OPENIPC
#define HW_PLATFORM_OPENIPC_CAMERA
#else
// Select the default hw platform to build for
//#define HW_PLATFORM_OPENIPC_CAMERA
//#define HW_PLATFORM_LINUX_GENERIC
//#define HW_PLATFORM_RADXA_3W
#define HW_PLATFORM_RASPBERRY
#endif

// Make sure one hardware platform is defined
#ifndef HW_PLATFORM_OPENIPC_CAMERA
#ifndef HW_PLATFORM_LINUX_GENERIC
#ifndef HW_PLATFORM_RASPBERRY
#ifndef HW_PLATFORM_RADXA_3W
#error "NO HARDWARE PLATFORM DEFINED!"
#endif
#endif
#endif
#endif


// ------------------------------------------------------------
// Configuration customisations for individual hardware platforms: 

#ifdef HW_PLATFORM_RASPBERRY

#define HW_CAPABILITY_GPIO
#define HW_CAPABILITY_I2C
#define HW_CAPABILITY_IONICE

#endif