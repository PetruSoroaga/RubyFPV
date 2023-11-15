/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include <fcntl.h>        // serialport
#include <termios.h>      // serialport
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <errno.h>
#include <stdlib.h>

#include "base.h"
#include "hardware.h"
#include "gpio.h"
#include "config.h"
#include "hw_procs.h"
#include "hardware_i2c.h"
#include "../common/string_utils.h"


static int s_ihwSwapButtons = 0;
int sLastReadMenu = 0;
int sLastReadBack = 0;
int sLastReadPlus = 0;
int sLastReadMinus = 0;
int sLastReadQA1 = 0;
int sLastReadQA2 = 0;
int sLastReadQA22 = 0;
int sLastReadQA3 = 0;
int sLastReadQAPlus = 0;
int sLastReadQAMinus = 0;

int sKeyMenuPressed = 0;
int sKeyBackPressed = 0;
int sKeyPlusPressed = 0;
int sKeyMinusPressed = 0;
int sKeyQA1Pressed = 0;
int sKeyQA2Pressed = 0;
int sKeyQA22Pressed = 0;
int sKeyQA3Pressed = 0;
int sKeyQAPlusPressed = 0;
int sKeyQAMinusPressed = 0;

u32 keyMenuDownStartTime = 0;
u32 keyBackDownStartTime = 0;
u32 keyPlusDownStartTime = 0;
u32 keyMinusDownStartTime = 0;
u32 keyQA1DownStartTime = 0;
u32 keyQA2DownStartTime = 0;
u32 keyQA22DownStartTime = 0;
u32 keyQA3DownStartTime = 0;
u32 keyQAPlusDownStartTime = 0;
u32 keyQAMinusDownStartTime = 0;

int sInitialReadQA1 = 0;
int sInitialReadQA2 = 0;
int sInitialReadQA22 = 0;
int sInitialReadQA3 = 0;
int sInitialReadQAPlus = 0;
int sInitialReadQAMinus = 0;

static u32 s_long_key_press_delta = 700;
static u32 s_long_press_repeat_time = 150;

int s_bBlockCurrentPressedKeys = 0;

int s_hwWasSetup = 0;
u32 initSequenceTimeStart = 0;

int hasCriticalError = 0;
int hasRecoverableError = 0;
u32 recoverableErrorStopTime = 0;
int flashGreenLed = 0;
u32 flashGreenLedStopTime = 0;

int s_isRecordingLedOn = 0;
int s_isRecordingLedBlinking = 0;
u32 s_TimeLastRecordingLedBlink = 0;

int s_bHarwareHasDetectedSystemType = 0;
int s_iHardwareSystemIsVehicle = 0;
int s_bHardwareHasCamera = 0;
u32 s_uHardwareCameraType = 0;

int s_iHardwareJoystickCount = 0;
hw_joystick_info_t s_HardwareJoystickInfo[MAX_JOYSTICK_INTERFACES];

int s_iUSBMounted = 0;


int init_hardware()
{
   initSequenceTimeStart = get_current_timestamp_ms();
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
   if ( 0 != GPIOWrite(GPIO_PIN_QACTION1, LOW) )
      log_line("Failed to pull down pin QA1");

   if (-1 == GPIODirection(GPIO_PIN_QACTION2, OUT))
   {
      log_line("Failed set GPIO configuration for pin QA2.");
      failed = 1;
   }
   if ( 0 != GPIOWrite(GPIO_PIN_QACTION2, LOW) )
      log_line("Failed to pull down pin QA2");
   
   if (-1 == GPIODirection(GPIO_PIN_QACTION2_2, IN))
   {
      log_line("Failed set GPIO configuration for pin QA2_2.");
      failed = 1;
   }
   if ( 0 != GPIOWrite(GPIO_PIN_QACTION2_2, LOW) )
      log_line("Failed to pull down pin QA2_2");

   if (-1 == GPIODirection(GPIO_PIN_QACTION3, IN))
   {
      log_line("Failed set GPIO configuration for pin QA3.");
      failed = 1;
   }
   if ( 0 != GPIOWrite(GPIO_PIN_QACTION3, LOW) )
      log_line("Failed to pull down pin QA3");

   if (-1 == GPIODirection(GPIO_PIN_QACTIONPLUS, IN))
   {
      log_line("Failed set GPIO configuration for pin QAPLUS.");
      failed = 1;
   }
   if ( 0 != GPIOWrite(GPIO_PIN_QACTIONPLUS, LOW) )
      log_line("Failed to pull down pin QAPLUS");

   if (-1 == GPIODirection(GPIO_PIN_QACTIONMINUS, IN))
   {
      log_line("Failed set GPIO configuration for pin QAMINUS.");
      failed = 1;
   }
   if ( 0 != GPIOWrite(GPIO_PIN_QACTIONMINUS, LOW) )
      log_line("Failed to pull down pin QAMINUS");

   if (-1 == GPIOExport(GPIO_PIN_BUZZER))
   {
      log_line("Failed to get GPIO access to pin for buzzer.");
      failed = 1;
   }

   if (-1 == GPIODirection(GPIO_PIN_BUZZER, OUT))
   {
      log_line("Failed set GPIO configuration for pin buzzer.");
      failed = 1;
   }

   if (-1 == GPIOExport(GPIO_PIN_DETECT_TYPE))
   {
      log_line("Failed to get GPIO access to pin Type.");
      failed = 1;
   }

   if (-1 == GPIODirection(GPIO_PIN_DETECT_TYPE, IN))
   {
      log_line("Failed set GPIO configuration for pin Type.");
      failed = 1;
   }

   if (-1 == GPIOExport(GPIO_PIN_LED_ERROR) ||
       -1 == GPIOExport(GPIO_PIN_RECORDING_LED))
   {
      log_line("Failed to get GPIO access to pin for LEDs.");
      failed = 1;
   }
   if (-1 == GPIODirection(GPIO_PIN_LED_ERROR, OUT) ||
       -1 == GPIODirection(GPIO_PIN_RECORDING_LED, OUT))
   {
      log_line("Failed set GPIO configuration for pin for LEDs.");
      failed = 1;
   }

   
   char szBuff[64];
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_MENU);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_MENU);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_BACK);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_BACK);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_PLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_PLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_MINUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_MINUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_QACTION1);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_QACTION1);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_QACTION2);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_QACTION2);
   hw_execute_bash_command_silent(szBuff, NULL);

   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_QACTION2_2);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_QACTION2_2);
   hw_execute_bash_command_silent(szBuff, NULL);

   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_QACTION3);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_QACTION3);
   hw_execute_bash_command_silent(szBuff, NULL);

   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_QACTIONPLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_QACTIONPLUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_QACTIONMINUS);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_QACTIONMINUS);
   hw_execute_bash_command_silent(szBuff, NULL);

   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_DETECT_TYPE);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_DETECT_TYPE);
   hw_execute_bash_command_silent(szBuff, NULL);

   GPIOWrite(GPIO_PIN_BUZZER, LOW);
   GPIOWrite(GPIO_PIN_LED_ERROR, LOW);
   GPIOWrite(GPIO_PIN_RECORDING_LED, LOW);
   log_line("HW: GPIO setup successfully.");

   sInitialReadQA1 = GPIORead(GPIO_PIN_QACTION1);
   sInitialReadQA2 = GPIORead(GPIO_PIN_QACTION2);
   sInitialReadQA22 = GPIORead(GPIO_PIN_QACTION2_2);
   sInitialReadQA3 = GPIORead(GPIO_PIN_QACTION3);
   sInitialReadQAPlus = GPIORead(GPIO_PIN_QACTIONPLUS);
   sInitialReadQAMinus = GPIORead(GPIO_PIN_QACTIONMINUS);
   log_line("Initial read of Quick Actions buttons: %d %d-%d %d, [%d, %d]", sInitialReadQA1, sInitialReadQA2, sInitialReadQA22, sInitialReadQA3, sInitialReadQAPlus, sInitialReadQAMinus);

   s_iHardwareJoystickCount = 0;
   s_hwWasSetup = 1;
   s_bHarwareHasDetectedSystemType = 0;
   if ( failed )
      return 0;
   return 1;
}

int init_hardware_only_status_led()
{
   initSequenceTimeStart = get_current_timestamp_ms();
   if (-1 == GPIOExport(GPIO_PIN_DETECT_TYPE))
   {
      log_line("Failed to get GPIO access to Ruby type PIN.");
      return(0);
   }

   if (-1 == GPIODirection(GPIO_PIN_DETECT_TYPE, IN))
   {
      log_line("Failed set GPIO configuration for Ruby type PIN.");
      return(0);
   }

   char szBuff[64];
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d in", GPIO_PIN_DETECT_TYPE);
   hw_execute_bash_command_silent(szBuff, NULL);
   snprintf(szBuff, sizeof(szBuff), "gpio -g mode %d down", GPIO_PIN_DETECT_TYPE);
   hw_execute_bash_command_silent(szBuff, NULL);

   log_line("HW: GPIO setup successfully.");

   s_iHardwareJoystickCount = 0;
   s_hwWasSetup = 1;
   s_bHarwareHasDetectedSystemType = 0;
   return 1;
}

void hardware_release()
{
   GPIOUnexport(GPIO_PIN_MENU);
   GPIOUnexport(GPIO_PIN_BACK);
   GPIOUnexport(GPIO_PIN_MINUS);
   GPIOUnexport(GPIO_PIN_PLUS);
   GPIOUnexport(GPIO_PIN_QACTION1);
   GPIOUnexport(GPIO_PIN_QACTION2);
   GPIOUnexport(GPIO_PIN_QACTION2_2);
   GPIOUnexport(GPIO_PIN_QACTION3);
   GPIOUnexport(GPIO_PIN_QACTIONPLUS);
   GPIOUnexport(GPIO_PIN_QACTIONMINUS);
   GPIOUnexport(GPIO_PIN_BUZZER);
   GPIOUnexport(GPIO_PIN_LED_ERROR);
   GPIOUnexport(GPIO_PIN_RECORDING_LED);
   GPIOUnexport(GPIO_PIN_DETECT_TYPE);
   log_line("Hardware released!");
}

void hardware_swap_buttons(int swap)
{
   s_ihwSwapButtons = swap;
}


int hardware_getBoardType()
{
   static int s_iHardwareLastDetectedBoardType = BOARD_TYPE_NONE;

   if ( s_iHardwareLastDetectedBoardType != BOARD_TYPE_NONE )
      return s_iHardwareLastDetectedBoardType;
     
   char szBuff[256];
   szBuff[0] = 0;
   hw_execute_bash_command("cat /proc/cpuinfo | grep 'Revision' | awk '{print $3}'", szBuff);
   log_line("Detected board Id: %s", szBuff);

   if ( strcmp(szBuff, "a03111") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "a03112") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "a03113") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "a03114") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "a03115") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "a03116") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "b03111") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "b03112") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "b03114") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "b03115") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "b03116") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "c03111") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "c03112") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "c03114") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "c03115") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "c03116") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "d03114") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "d03115") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}
   if ( strcmp(szBuff, "d03116") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI4B;}

   if ( strcmp(szBuff, "29020e0") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI3APLUS;}
   if ( strcmp(szBuff, "2a02082") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBuff, "2a22082") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBuff, "2a32082") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBuff, "2a52082") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI3B;}
   if ( strcmp(szBuff, "2a020d3") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI3BPLUS;}

   if ( strcmp(szBuff, "1900093") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBuff, "19000c1") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PIZEROW;}
   if ( strcmp(szBuff, "2900092") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PIZERO;}

   if ( strcmp(szBuff, "2900093") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBuff, "29000c1") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PIZEROW;}
   if ( strcmp(szBuff, "2902120") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PIZERO2;}

   if ( strcmp(szBuff, "2920092") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PIZERO;}
   if ( strcmp(szBuff, "2920093") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PIZERO;}

   if ( strcmp(szBuff, "2a01040") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI2B;}
   if ( strcmp(szBuff, "2a21041") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI2BV11;}
   if ( strcmp(szBuff, "2a01041") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI2BV11;}
   if ( strcmp(szBuff, "2a22042") == 0 ) { s_iHardwareLastDetectedBoardType = BOARD_TYPE_PI2BV12;}
   
    strlcpy(szBuff, str_get_hardware_board_name(s_iHardwareLastDetectedBoardType), sizeof(szBuff));
   if ( szBuff[0] == 0 )
      strcpy(szBuff, "N/A");
   log_line("Current board type: %s", szBuff);

   return s_iHardwareLastDetectedBoardType;
}


int hardware_getWifiType()
{
   int wifi_type = WIFI_TYPE_NONE;

   char szBuff[256];
   szBuff[0] = 0;
   hw_execute_bash_command("cat /proc/cpuinfo | grep 'Revision' | awk '{print $3}'", szBuff);
   log_line("Detected board Id: %s", szBuff);

   if ( strcmp(szBuff, "a03111") == 0 ) { wifi_type = WIFI_TYPE_AG; }
   if ( strcmp(szBuff, "b03111") == 0 ) { wifi_type = WIFI_TYPE_AG; }
   if ( strcmp(szBuff, "b03112") == 0 ) { wifi_type = WIFI_TYPE_AG; }
   if ( strcmp(szBuff, "b03114") == 0 ) { wifi_type = WIFI_TYPE_AG; }
   if ( strcmp(szBuff, "c03111") == 0 ) { wifi_type = WIFI_TYPE_AG; }
   if ( strcmp(szBuff, "c03112") == 0 ) { wifi_type = WIFI_TYPE_AG; }
   if ( strcmp(szBuff, "c03114") == 0 ) { wifi_type = WIFI_TYPE_AG; }
   if ( strcmp(szBuff, "d03114") == 0 ) { wifi_type = WIFI_TYPE_AG; }

   if ( strcmp(szBuff, "29020e0") == 0 ) { wifi_type = WIFI_TYPE_AG; }
   if ( strcmp(szBuff, "2a02082") == 0 ) { wifi_type = WIFI_TYPE_G; }
   if ( strcmp(szBuff, "2a22082") == 0 ) { wifi_type = WIFI_TYPE_G; }
   if ( strcmp(szBuff, "2a32082") == 0 ) { wifi_type = WIFI_TYPE_G; }
   if ( strcmp(szBuff, "2a52082") == 0 ) { wifi_type = WIFI_TYPE_G; }
   if ( strcmp(szBuff, "2a020d3") == 0 ) { wifi_type = WIFI_TYPE_AG; }

   if ( strcmp(szBuff, "1900093") == 0 ) { wifi_type = WIFI_TYPE_NONE; }
   if ( strcmp(szBuff, "19000c1") == 0 ) { wifi_type = WIFI_TYPE_AG; }
   if ( strcmp(szBuff, "2900092") == 0 ) { wifi_type = WIFI_TYPE_NONE; }

   if ( strcmp(szBuff, "2900093") == 0 ) { wifi_type = WIFI_TYPE_NONE; }
   if ( strcmp(szBuff, "29000c1") == 0 ) { wifi_type = WIFI_TYPE_AG; }
   if ( strcmp(szBuff, "2902120") == 0 ) { wifi_type = WIFI_TYPE_NONE; }

   if ( strcmp(szBuff, "2920092") == 0 ) { wifi_type = WIFI_TYPE_NONE; }
   if ( strcmp(szBuff, "2920093") == 0 ) { wifi_type = WIFI_TYPE_NONE; }

   if ( strcmp(szBuff, "2a01040") == 0 ) { wifi_type = WIFI_TYPE_NONE; }
   if ( strcmp(szBuff, "2a21041") == 0 ) { wifi_type = WIFI_TYPE_NONE; }
   if ( strcmp(szBuff, "2a01041") == 0 ) { wifi_type = WIFI_TYPE_NONE; }
   if ( strcmp(szBuff, "2a22042") == 0 ) { wifi_type = WIFI_TYPE_NONE; }
   
   return wifi_type;
}

int hardware_detectBoardType()
{
   int wifi_type = WIFI_TYPE_NONE;
   int board_type = BOARD_TYPE_NONE;

   board_type = hardware_getBoardType();
   wifi_type = hardware_getWifiType();

   char szBuff[1024];
    strlcpy(szBuff, str_get_hardware_board_name(board_type), sizeof(szBuff));
   if ( szBuff[0] == 0 )
      strcpy(szBuff, "N/A");

   log_line("");
   log_line("---------------------------------------------------------------");
   log_line("|   System Hardware: Board Type: %d (%s - %s), Built-in WiFi type: %s", board_type, szBuff, str_get_hardware_board_name(board_type), str_get_hardware_wifi_name(wifi_type));
   log_line("---------------------------------------------------------------");
   log_line("");
   hw_execute_bash_command_raw("cat /proc/device-tree/model", szBuff);
   log_line("Board description string: %s", szBuff);
   log_line("");
   FILE* fd = fopen("config/board.txt", "w");
   if ( NULL == fd )
      log_softerror_and_alarm("Failed to save board configuration to file: config/board.txt");
   else
   {
      fprintf(fd, "%d %s\n", board_type, szBuff);
      fclose(fd);
   }

   fd = fopen("config/wifi.txt", "w");
   if ( NULL == fd )
      log_softerror_and_alarm("Failed to save wifi configuration to file: config/wifi.txts");
   else
   {
      fprintf(fd, "%d\n", wifi_type);
      fclose(fd);
   }

   fd = fopen("/boot/ruby_board.txt", "w");
   if ( NULL != fd )
   {
      fprintf(fd, "%d\n", board_type);
      fclose(fd);
   }

   fd = fopen("/boot/ruby_wifi.txt", "w");
   if ( NULL != fd )
   {
      fprintf(fd, "%d\n", wifi_type);
      fclose(fd);
   }

   hw_execute_bash_command("cat /proc/device-tree/model > /boot/ruby_board_desc.txt", NULL);
   hw_execute_bash_command("cat /proc/device-tree/model > config/ruby_board_desc.txt", NULL);

   return board_type;
}

void hardware_enum_joystick_interfaces()
{
   char szDevName[256];

   log_line("------------------------------------------------------------------------");
   log_line("|  HW: Enumerating hardware HID interfaces...                          |");
   s_iHardwareJoystickCount = 0;

   for( int i=0; i<MAX_JOYSTICK_INTERFACES; i++ )
   {
      s_HardwareJoystickInfo[s_iHardwareJoystickCount].deviceIndex = -1;
      s_HardwareJoystickInfo[s_iHardwareJoystickCount].szName[0] = 0;
      s_HardwareJoystickInfo[s_iHardwareJoystickCount].uId = 0;
      s_HardwareJoystickInfo[s_iHardwareJoystickCount].countAxes = 0;
      s_HardwareJoystickInfo[s_iHardwareJoystickCount].countButtons = 0;
      snprintf(szDevName, sizeof(szDevName), "/dev/input/js%d", i);
      if( access( szDevName, R_OK ) == -1 )
         continue;
   
      int fd = open(szDevName, O_NONBLOCK);

      char name[256];
      if (ioctl(fd, JSIOCGNAME(sizeof(name)), name) < 0)
          strlcpy(name, "Unknown", sizeof(name));

      u32 uid = 0;
      if (ioctl(fd, JSIOCGVERSION, &uid) == -1)
         log_softerror_and_alarm("Error to query for joystick UID");

      u8 count_axes = 0;
      if (ioctl(fd, JSIOCGAXES, &count_axes) == -1)
         log_softerror_and_alarm("Error to query for joystick axes");

      u8 count_buttons = 0;
      if (ioctl(fd, JSIOCGBUTTONS, &count_buttons) == -1)
         log_softerror_and_alarm("Error to query for joystick buttons");
   
      close(fd);

      if ( count_axes > MAX_JOYSTICK_AXES )
         count_axes = MAX_JOYSTICK_AXES;
      if ( count_buttons > MAX_JOYSTICK_BUTTONS )
         count_buttons = MAX_JOYSTICK_BUTTONS;

      for( int i=0; i<strlen(name); i++ )
         uid += name[i]*i;
      s_HardwareJoystickInfo[s_iHardwareJoystickCount].deviceIndex = i;
      strcpy(s_HardwareJoystickInfo[s_iHardwareJoystickCount].szName, name);
      s_HardwareJoystickInfo[s_iHardwareJoystickCount].uId = uid;
      s_HardwareJoystickInfo[s_iHardwareJoystickCount].countAxes = count_axes;
      s_HardwareJoystickInfo[s_iHardwareJoystickCount].countButtons = count_buttons;
      for( int k=0; k<MAX_JOYSTICK_AXES; k++ )
         s_HardwareJoystickInfo[s_iHardwareJoystickCount].axesValues[k] = 0;
      for( int k=0; k<MAX_JOYSTICK_BUTTONS; k++ )
         s_HardwareJoystickInfo[s_iHardwareJoystickCount].buttonsValues[k] = 0;
      s_HardwareJoystickInfo[s_iHardwareJoystickCount].fd = -1;
      log_line("|  Found joystick interface %s: Name: %s, UID: %u, has %d axes and %d buttons.", szDevName, name, uid, count_axes, count_buttons);
      s_iHardwareJoystickCount++;
   }
   log_line("|  HW: Enumerating hardware HID interfaces result: found %d interfaces. |", s_iHardwareJoystickCount);
   log_line("------------------------------------------------------------------------");
}

int hardware_get_joystick_interfaces_count()
{
   return s_iHardwareJoystickCount;
}

hw_joystick_info_t* hardware_get_joystick_info(int index)
{
   if ( index < 0 || index >= s_iHardwareJoystickCount )
      return NULL;
   return &s_HardwareJoystickInfo[index];
}

int hardware_open_joystick(int joystickIndex)
{
   if (joystickIndex < 0 || joystickIndex >= s_iHardwareJoystickCount )
      return 0;
   if ( s_HardwareJoystickInfo[joystickIndex].deviceIndex < 0 )
      return 0;

   if ( s_HardwareJoystickInfo[joystickIndex].fd > 0 )
   {
      log_softerror_and_alarm("HW: Opening HID interface /dev/input/js%d (it's already opened)", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
      return 1;
   }
   char szDevName[256];
   snprintf(szDevName, sizeof(szDevName), "/dev/input/js%d", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
   if( access( szDevName, R_OK ) == -1 )
   {
      log_softerror_and_alarm("HW: Failed to access HID interface /dev/input/js%d !", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
      return 0;
   }
   s_HardwareJoystickInfo[joystickIndex].fd = open(szDevName, O_RDONLY | O_NONBLOCK);
   if ( s_HardwareJoystickInfo[joystickIndex].fd <= 0 )
   {
      log_softerror_and_alarm("HW: Failed to open HID interface /dev/input/js%d !", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
      s_HardwareJoystickInfo[joystickIndex].fd = -1;
      return 0;
   }
   log_line("HW: Opened HID interface /dev/input/js%d;", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
   return 1;
}

void hardware_close_joystick(int joystickIndex)
{
   if (joystickIndex < 0 || joystickIndex >= s_iHardwareJoystickCount )
      return;
   if ( -1 == s_HardwareJoystickInfo[joystickIndex].fd )
      log_softerror_and_alarm("HW: Closing HID interface /dev/input/js%d (it's already closed)", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
   else
   {   
      close(s_HardwareJoystickInfo[joystickIndex].fd);
      s_HardwareJoystickInfo[joystickIndex].fd = -1;
      log_line("HW: Closed HID interface /dev/input/js%d;", s_HardwareJoystickInfo[joystickIndex].deviceIndex);
   }
}

// Return 1 if joystick file is opened
int hardware_is_joystick_opened(int joystickIndex)
{
   if (joystickIndex < 0 || joystickIndex >= s_iHardwareJoystickCount )
      return 0;
   if ( -1 == s_HardwareJoystickInfo[joystickIndex].fd )
      return 0;
   return 1;
}

// Returns the count of new events
// Return -1 on error

int hardware_read_joystick(int joystickIndex, int miliSec)
{
   if (joystickIndex < 0 || joystickIndex >= s_iHardwareJoystickCount )
      return -1;
   if ( s_HardwareJoystickInfo[joystickIndex].deviceIndex < 0 )
      return -1;
   if ( -1 == s_HardwareJoystickInfo[joystickIndex].fd )
      return -1;

   memcpy( &s_HardwareJoystickInfo[joystickIndex].buttonsValuesPrev, &s_HardwareJoystickInfo[joystickIndex].buttonsValues, MAX_JOYSTICK_BUTTONS*sizeof(int));
   memcpy( &s_HardwareJoystickInfo[joystickIndex].axesValuesPrev, &s_HardwareJoystickInfo[joystickIndex].axesValues, MAX_JOYSTICK_AXES*sizeof(int));

   int countEvents = 0;
   u32 timeStart = get_current_timestamp_micros();
   u32 timeEnd = timeStart + miliSec*1000;
   if ( timeEnd < timeStart )
      timeEnd = timeStart;

   while ( get_current_timestamp_micros() < timeEnd )
   { 
      hardware_sleep_micros(200);
      struct js_event joystickEvent[8];
      int iRead = read(s_HardwareJoystickInfo[joystickIndex].fd, &joystickEvent[0], sizeof(joystickEvent));
      if ( iRead == 0 )
         continue;
      if ( iRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) )
         continue;
      if ( iRead < 0 )
      {
         log_softerror_and_alarm("Error on reading joystick data, joystick index: %d, error: %d", joystickIndex, errno);
         hardware_close_joystick(joystickIndex);
         return -1;
      }
      int count = iRead / sizeof(joystickEvent[0]);
      for( int i=0; i<count; i++ )
      {
         if ( (joystickEvent[i].type & ~JS_EVENT_INIT) == JS_EVENT_BUTTON )
         if ( joystickEvent[i].number >= 0 && joystickEvent[i].number < MAX_JOYSTICK_BUTTONS )
         {
            s_HardwareJoystickInfo[joystickIndex].buttonsValues[joystickEvent[i].number] = joystickEvent[i].value;
            countEvents++;
         }
         if ( (joystickEvent[i].type & ~JS_EVENT_INIT) == JS_EVENT_AXIS )
         if ( joystickEvent[i].number >= 0 && joystickEvent[i].number < MAX_JOYSTICK_AXES )
         {
            s_HardwareJoystickInfo[joystickIndex].axesValues[joystickEvent[i].number] = joystickEvent[i].value;
            countEvents++;
         }
      }
   }
   return countEvents;
}


u16 hardware_get_flags()
{
   u16 retValue = 0xFFFF;
   char szOut[1024];
   hw_execute_bash_command_silent("vcgencmd get_throttled", szOut);
   int len = strlen(szOut)-1;
   while (len > 0 )
   {
      if ( szOut[len] == '=')
         break;
      len--;
   }
   if ( szOut[len] == '=' )
   {
      unsigned long ul = 0;
      if ( 1 == sscanf(szOut+len+1, "0x%lX", &ul) )
      {
         u8 u = (ul & 0xF) | ((ul>>16) << 4);
         retValue = u;
      }
   }
   
   return retValue;
}

void keyboard_loop()
{
   if ( ! s_hwWasSetup )
      return;

   // Check inputs
   int rMenu = GPIORead(GPIO_PIN_MENU);
   int rBack = GPIORead(GPIO_PIN_BACK);
   int rPlus = GPIORead(GPIO_PIN_PLUS);
   int rMinus = GPIORead(GPIO_PIN_MINUS);
   int rQA1 = GPIORead(GPIO_PIN_QACTION1);
   int rQA2 = GPIORead(GPIO_PIN_QACTION2);
   int rQA22 = GPIORead(GPIO_PIN_QACTION2_2);
   int rQA3 = GPIORead(GPIO_PIN_QACTION3);
   int rQAPlus = GPIORead(GPIO_PIN_QACTIONPLUS);
   int rQAMinus = GPIORead(GPIO_PIN_QACTIONMINUS);

   u32 time_now = get_current_timestamp_ms();

   if ( ! rMenu )
   {
      sKeyMenuPressed = 0;
      keyMenuDownStartTime = 0;
   }
   if ( rMenu && (!sLastReadMenu) )
   {
      sKeyMenuPressed = 1;
      keyMenuDownStartTime = time_now;
   }
   else
      sKeyMenuPressed = 0;

   if ( rMenu && (0 != keyMenuDownStartTime) && (time_now > (keyMenuDownStartTime+s_long_key_press_delta)) )
   {
      sKeyMenuPressed = 1;
      keyMenuDownStartTime += s_long_press_repeat_time;
   }

   if ( ! rBack )
   {
      sKeyBackPressed = 0;
      keyBackDownStartTime = 0;
   }
   if ( rBack && (!sLastReadBack) )
   {
      sKeyBackPressed = 1;
      keyBackDownStartTime = time_now;
   }
   else
      sKeyBackPressed = 0;

   if ( rBack && 0 != keyBackDownStartTime && time_now > (keyBackDownStartTime+s_long_key_press_delta) )
   {
      sKeyBackPressed = 1;
      keyBackDownStartTime += s_long_press_repeat_time;
   }
    
   if ( ! rPlus )
   {
      sKeyPlusPressed = 0;
      keyPlusDownStartTime = 0;
   }
   if ( rPlus && (!sLastReadPlus) )
   {
      sKeyPlusPressed = 1;
      keyPlusDownStartTime = time_now;
   }
   else
      sKeyPlusPressed = 0;

   if ( rPlus && 0 != keyPlusDownStartTime && time_now > (keyPlusDownStartTime+s_long_key_press_delta) )
   {
      sKeyPlusPressed = 1;
      keyPlusDownStartTime += s_long_press_repeat_time;
   }
   
   if ( ! rMinus )
   {
      sKeyMinusPressed = 0;
      keyMinusDownStartTime = 0;
   }
   if ( rMinus && (!sLastReadMinus) )
   {
      sKeyMinusPressed = 1;
      keyMinusDownStartTime = time_now;
   }
   else
      sKeyMinusPressed = 0;

   if ( rMinus && 0 != keyMinusDownStartTime && time_now > (keyMinusDownStartTime+s_long_key_press_delta) )
   {
      sKeyMinusPressed = 1;
      keyMinusDownStartTime += s_long_press_repeat_time;
   }

   if ( ! rQA1 )
   {
      sKeyQA1Pressed = 0;
      keyQA1DownStartTime = 0;
   }
   if ( rQA1 && (!sLastReadQA1) )
   {
      sKeyQA1Pressed = 1;
      keyQA1DownStartTime = time_now;
   }
   else
      sKeyQA1Pressed = 0;

   if ( rQA1 && 0 != keyQA1DownStartTime && time_now > (keyQA1DownStartTime+s_long_key_press_delta) )
   {
      sKeyQA1Pressed = 1;
      keyQA1DownStartTime += s_long_press_repeat_time;
   }

   if ( ! rQA2 )
   {
      sKeyQA2Pressed = 0;
      keyQA2DownStartTime = 0;
   }
   if ( rQA2 && (!sLastReadQA2) )
   {
      sKeyQA2Pressed = 1;
      keyQA2DownStartTime = time_now;
   }
   else
      sKeyQA2Pressed = 0;

   if ( rQA2 && 0 != keyQA2DownStartTime && time_now > (keyQA2DownStartTime+s_long_key_press_delta) )
   {
      sKeyQA2Pressed = 1;
      keyQA2DownStartTime += s_long_press_repeat_time;
   }

   if ( ! rQA22 )
   {
      sKeyQA22Pressed = 0;
      keyQA22DownStartTime = 0;
   }
   if ( rQA22 && (!sLastReadQA22) )
   {
      sKeyQA22Pressed = 1;
      keyQA22DownStartTime = time_now;
   }
   else
      sKeyQA22Pressed = 0;

   if ( rQA22 && 0 != keyQA22DownStartTime && time_now > (keyQA22DownStartTime+s_long_key_press_delta) )
   {
      sKeyQA22Pressed = 1;
      keyQA22DownStartTime += s_long_press_repeat_time;
   }

   if ( ! rQA3 )
   {
      sKeyQA3Pressed = 0;
      keyQA3DownStartTime = 0;
   }
   if ( rQA3 && (!sLastReadQA3) )
   {
      sKeyQA3Pressed = 1;
      keyQA3DownStartTime = time_now;
   }
   else
      sKeyQA3Pressed = 0;

   if ( rQA3 && 0 != keyQA3DownStartTime && time_now > (keyQA3DownStartTime+s_long_key_press_delta) )
   {
      sKeyQA3Pressed = 1;
      keyQA3DownStartTime += s_long_press_repeat_time;
   }

   if ( ! rQAPlus )
   {
      sKeyQAPlusPressed = 0;
      keyQAPlusDownStartTime = 0;
   }
   if ( rQAPlus && (!sLastReadQAPlus) )
   {
      sKeyQAPlusPressed = 1;
      keyQAPlusDownStartTime = time_now;
   }
   else
      sKeyQAPlusPressed = 0;

   if ( rQAPlus && 0 != keyQAPlusDownStartTime && time_now > (keyQAPlusDownStartTime+s_long_key_press_delta) )
   {
      sKeyQAPlusPressed = 1;
      keyQAPlusDownStartTime += s_long_press_repeat_time;
   }

   if ( ! rQAMinus )
   {
      sKeyQAMinusPressed = 0;
      keyQAMinusDownStartTime = 0;
   }
   if ( rQAMinus && (!sLastReadQAMinus) )
   {
      sKeyQAMinusPressed = 1;
      keyQAMinusDownStartTime = time_now;
   }
   else
      sKeyQAMinusPressed = 0;

   if ( rQAMinus && 0 != keyQAMinusDownStartTime && time_now > (keyQAMinusDownStartTime+s_long_key_press_delta) )
   {
      sKeyQAMinusPressed = 1;
      keyQAMinusDownStartTime += s_long_press_repeat_time;
   }

   //printf("\n%d %d    %d %d %d %d\n", sKeyQA1Pressed,sKeyQA2Pressed, sKeyMenuPressed, sKeyBackPressed, sKeyPlusPressed, sKeyMinusPressed );

   if ( s_bBlockCurrentPressedKeys )
   {
      sKeyMenuPressed = 0;
      sKeyBackPressed = 0;
      sKeyPlusPressed = 0;
      sKeyMinusPressed = 0;
      sKeyQA1Pressed = 0;
      sKeyQA2Pressed = 0;
      sKeyQA22Pressed = 0;
      sKeyQA3Pressed = 0;
      sKeyQAMinusPressed = 0;
      sKeyQAMinusPressed = 0;

      if ( !rMenu && sLastReadMenu )
         s_bBlockCurrentPressedKeys = 0;
      if ( !rBack && sLastReadBack )
         s_bBlockCurrentPressedKeys = 0;
      if ( !rPlus && sLastReadPlus )
         s_bBlockCurrentPressedKeys = 0;
      if ( !rMinus && sLastReadMinus )
         s_bBlockCurrentPressedKeys = 0;
      if ( !rQA1 && sLastReadQA1 )
         s_bBlockCurrentPressedKeys = 0;
      if ( !rQA2 && sLastReadQA2 )
         s_bBlockCurrentPressedKeys = 0;
      if ( !rQA22 && sLastReadQA22 )
         s_bBlockCurrentPressedKeys = 0;
      if ( !rQA3 && sLastReadQA3 )
         s_bBlockCurrentPressedKeys = 0;
      if ( !rQAMinus && sLastReadQAMinus )
         s_bBlockCurrentPressedKeys = 0;
      if ( !rQAPlus && sLastReadQAPlus )
         s_bBlockCurrentPressedKeys = 0;
   }
   sLastReadMenu = rMenu;
   sLastReadBack = rBack;
   sLastReadPlus = rPlus;
   sLastReadMinus = rMinus;
   sLastReadQA1 = rQA1;
   sLastReadQA2 = rQA2;
   sLastReadQA22 = rQA22;
   sLastReadQA3 = rQA3;
   sLastReadQAPlus = rQAPlus;
   sLastReadQAMinus = rQAMinus;
}

void hardware_loop()
{
   if ( ! s_hwWasSetup )
      return;

   keyboard_loop();

   u32 t = get_current_timestamp_ms();

   if ( t < (initSequenceTimeStart + 1200 ) )
   {
      int delta = (t - initSequenceTimeStart)/200;
      int step = delta % 4;
      switch (step)
      {
          case 0: GPIOWrite(GPIO_PIN_LED_ERROR, HIGH);
                  break;
          case 1: GPIOWrite(GPIO_PIN_LED_ERROR, LOW);
                  break;
          case 2: GPIOWrite(GPIO_PIN_LED_ERROR, HIGH);
                  break;
          case 3: GPIOWrite(GPIO_PIN_LED_ERROR, LOW);
                  break;
      }
   }
   else if ( initSequenceTimeStart > 0 )
   {
      initSequenceTimeStart = 0;
      GPIOWrite(GPIO_PIN_BUZZER, LOW);
      GPIOWrite(GPIO_PIN_LED_ERROR, LOW);
   }

   if ( hasRecoverableError )
   {
      if ( (t/150) % 2 )
         GPIOWrite(GPIO_PIN_LED_ERROR, LOW);
      else
         GPIOWrite(GPIO_PIN_LED_ERROR, HIGH);

      if ( t>recoverableErrorStopTime )
      {
         GPIOWrite(GPIO_PIN_LED_ERROR, LOW);
         hasRecoverableError = 0;
         recoverableErrorStopTime = 0;
      }
   }


   if ( s_isRecordingLedBlinking )
   {
      if ( s_isRecordingLedOn && t > s_TimeLastRecordingLedBlink+500 )
      {
         GPIOWrite(GPIO_PIN_RECORDING_LED, LOW);
         s_isRecordingLedOn = 0;
         s_TimeLastRecordingLedBlink = get_current_timestamp_ms();
      }
      if ( (!s_isRecordingLedOn) && t > s_TimeLastRecordingLedBlink+500 )
      {
         GPIOWrite(GPIO_PIN_RECORDING_LED, HIGH);
         s_isRecordingLedOn = 1;
         s_TimeLastRecordingLedBlink = get_current_timestamp_ms();
      }
   }
}

void hardware_setCriticalErrorFlag()
{
   if ( ! s_hwWasSetup )
      return;
   GPIOWrite(GPIO_PIN_LED_ERROR, HIGH);
   hasCriticalError = 1;
}

void hardware_setRecoverableErrorFlag()
{
   if ( ! s_hwWasSetup )
      return;
   hasRecoverableError = 1;
   recoverableErrorStopTime = get_current_timestamp_ms()+1250;
}

void hardware_flashGreenLed()
{
   flashGreenLed = 1;
   flashGreenLedStopTime = get_current_timestamp_ms()+280;
}

int isKeyMenuPressed() { return sKeyMenuPressed; }
int isKeyBackPressed() { return sKeyBackPressed; }

int isKeyPlusPressed()
{
   if ( s_ihwSwapButtons == 0 )
      return sKeyPlusPressed;
   else
      return sKeyMinusPressed;
}

int isKeyMinusPressed()
{
   if ( s_ihwSwapButtons == 0 )
      return sKeyMinusPressed;
   else
      return sKeyPlusPressed;
}

int isKeyMenuLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( sKeyMenuPressed && (0 != keyMenuDownStartTime) && (time_now > (keyMenuDownStartTime+s_long_key_press_delta)) )
      return 1;
   return 0;
}

int isKeyBackLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( sKeyBackPressed && (0 != keyBackDownStartTime) && (time_now > (keyBackDownStartTime+s_long_key_press_delta)) )
      return 1;
   return 0;
}

int isKeyPlusLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( s_ihwSwapButtons == 0 )
   {
      if ( sKeyPlusPressed && (0 != keyPlusDownStartTime) && (time_now > (keyPlusDownStartTime+s_long_key_press_delta)) )
         return 1;
   }
   else
   {
      if ( sKeyMinusPressed && (0 != keyMinusDownStartTime) && (time_now > (keyMinusDownStartTime+s_long_key_press_delta)) )
         return 1;
   }

   return 0;
}

int isKeyMinusLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( s_ihwSwapButtons == 0 )
   {
      if ( sKeyMinusPressed && (0 != keyMinusDownStartTime) && (time_now > (keyMinusDownStartTime+s_long_key_press_delta)) )
         return 1;
   }
   else
   {
      if ( sKeyPlusPressed && (0 != keyPlusDownStartTime) && (time_now > (keyPlusDownStartTime+s_long_key_press_delta)) )
         return 1;
   }

   return 0;
}

int isKeyPlusLongLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( s_ihwSwapButtons == 0 )
   {
      if ( sKeyPlusPressed && (0 != keyPlusDownStartTime) && (time_now > (keyPlusDownStartTime+3*s_long_key_press_delta)) )
         return 1;
   }
   else
   {
      if ( sKeyMinusPressed && (0 != keyMinusDownStartTime) && (time_now > (keyMinusDownStartTime+3*s_long_key_press_delta)) )
         return 1;
   }

   return 0;
}

int isKeyMinusLongLongPressed()
{
   u32 time_now = get_current_timestamp_ms();
   if ( s_ihwSwapButtons == 0 )
   {
      if ( sKeyMinusPressed && (0 != keyMinusDownStartTime) && (time_now > (keyMinusDownStartTime+3*s_long_key_press_delta)) )
         return 1;
   }
   else
   {
      if ( sKeyPlusPressed && (0 != keyPlusDownStartTime) && (time_now > (keyPlusDownStartTime+3*s_long_key_press_delta)) )
         return 1;
   }

   return 0;
}

int isKeyQA1Pressed()
{
   if ( sInitialReadQA1 )
      return 0;
   return sKeyQA1Pressed;
}

int isKeyQA2Pressed()
{
   int pressed = 0;
   if ( ! sInitialReadQA2 )
      pressed = pressed || sKeyQA2Pressed;
   if ( ! sInitialReadQA22 )
      pressed = pressed || sKeyQA22Pressed;
   return pressed;
}

int isKeyQA3Pressed()
{
   if ( sInitialReadQA3 )
      return 0;
   return sKeyQA3Pressed;
}

void hardware_override_keys(int iKeyMenu, int iKeyBack, int iKeyPlus, int iKeyMinus, int iKeyMenuLong, int iKeyBackLong, int iKeyPlusLong, int iKeyMinusLong)
{
   if ( iKeyMenu )
      sKeyMenuPressed = 1;
   if ( iKeyBack )
      sKeyBackPressed = 1;
   if ( iKeyPlus )
      sKeyPlusPressed = 1;
   if ( iKeyMinus )
      sKeyMinusPressed = 1;

   if ( iKeyMenuLong )
   {
      sKeyMenuPressed = 1;
      u32 time_now = get_current_timestamp_ms();
      keyMenuDownStartTime = time_now - 2000;
   }
   if ( iKeyBackLong )
   {
      sKeyBackPressed = 1;
      u32 time_now = get_current_timestamp_ms();
      keyBackDownStartTime = time_now - 2000;
   }
   if ( iKeyPlusLong )
   {
      sKeyPlusPressed = 1;
      u32 time_now = get_current_timestamp_ms();
      keyPlusDownStartTime =  time_now - 2000;
   }
   if ( iKeyMinusLong )
   {
      sKeyMinusPressed = 1;
      u32 time_now = get_current_timestamp_ms();
      keyMinusDownStartTime = time_now - 2000;
   }
}

void hardware_ResetLongPressStatus()
{
   sLastReadMenu = 1;
   sLastReadBack = 1;
   sLastReadPlus = 1;
   sLastReadMinus = 1;
   sLastReadQA1 = 1;
   sLastReadQA2 = 1;
   sLastReadQA22 = 1;
   sLastReadQA3 = 1;

   sKeyMenuPressed = 0;
   sKeyBackPressed = 0;
   sKeyPlusPressed = 0;
   sKeyMinusPressed = 0;
   sKeyQA1Pressed = 0;
   sKeyQA2Pressed = 0;
   sKeyQA22Pressed = 0;
   sKeyQA3Pressed = 0;

   keyMenuDownStartTime = 0;
   keyBackDownStartTime = 0;
   keyPlusDownStartTime = 0;
   keyMinusDownStartTime = 0;
   keyQA1DownStartTime = 0;
   keyQA2DownStartTime = 0;
   keyQA22DownStartTime = 0;
   keyQA3DownStartTime = 0;
}

void hardware_blockCurrentPressedKeys()
{
   s_bBlockCurrentPressedKeys = 1;

   sKeyMenuPressed = 0;
   sKeyBackPressed = 0;
   sKeyPlusPressed = 0;
   sKeyMinusPressed = 0;
   sKeyQA1Pressed = 0;
   sKeyQA2Pressed = 0;
   sKeyQA22Pressed = 0;
   sKeyQA3Pressed = 0;
}

int hardware_try_mount_usb()
{
   if ( s_iUSBMounted )
      return 1;

   char szBuff[128];
   char szCommand[128];
   char szOutput[2048];
   s_iUSBMounted = 0;
   snprintf(szCommand, sizeof(szCommand), "mkdir -p %s", FOLDER_USB_MOUNT); 
   hw_execute_bash_command(szCommand, NULL);

   for( int i=10; i>0; i-- )
   {
      snprintf(szBuff, sizeof(szBuff), "/dev/sda%d", i);
      snprintf(szCommand, sizeof(szCommand), "mount /dev/sda%d %s 2>&1", i, FOLDER_USB_MOUNT);
      if( access( szBuff, R_OK ) != -1 )
      {
         hw_execute_bash_command(szCommand, szOutput);
         s_iUSBMounted = 1;
         break;
      }

      snprintf(szBuff, sizeof(szBuff), "/dev/sdb%d", i);
      snprintf(szCommand, sizeof(szCommand), "mount /dev/sdb%d %s 2>&1", i, FOLDER_USB_MOUNT);
      if( access( szBuff, R_OK ) != -1 )
      {
         hw_execute_bash_command(szCommand, szOutput);
         s_iUSBMounted = 1;
         break;
      }

      snprintf(szBuff, sizeof(szBuff), "/dev/sdc%d", i);
      snprintf(szCommand, sizeof(szCommand), "mount /dev/sdc%d %s 2>&1", i, FOLDER_USB_MOUNT);
      if( access( szBuff, R_OK ) != -1 )
      {
         hw_execute_bash_command(szCommand, szOutput);
         s_iUSBMounted = 1;
         break;
      }

      snprintf(szBuff, sizeof(szBuff), "/dev/sdd%d", i);
      snprintf(szCommand, sizeof(szCommand), "mount /dev/sdd%d %s 2>&1", i, FOLDER_USB_MOUNT);
      if( access( szBuff, R_OK ) != -1 )
      {
         hw_execute_bash_command(szCommand, szOutput);
         s_iUSBMounted = 1;
         break;
      }
   }
   if ( ! s_iUSBMounted )
   if( access( "/dev/sda", R_OK ) != -1 )
   {
      snprintf(szCommand, sizeof(szCommand), "mount /dev/sda %s 2>&1", FOLDER_USB_MOUNT);
      hw_execute_bash_command(szCommand, szOutput);
      s_iUSBMounted = 1;
   }

   if ( ! s_iUSBMounted )
   if( access( "/dev/sdb", R_OK ) != -1 )
   {
      snprintf(szCommand, sizeof(szCommand), "mount /dev/sdb %s 2>&1", FOLDER_USB_MOUNT);
      hw_execute_bash_command(szCommand, szOutput);
      s_iUSBMounted = 1;
   }

   if ( ! s_iUSBMounted )
   if( access( "/dev/sdc", R_OK ) != -1 )
   {
      snprintf(szCommand, sizeof(szCommand), "mount /dev/sdc %s 2>&1", FOLDER_USB_MOUNT);
      hw_execute_bash_command(szCommand, szOutput);
      s_iUSBMounted = 1;
   }

   if ( ! s_iUSBMounted )
   if( access( "/dev/sdd", R_OK ) != -1 )
   {
      snprintf(szCommand, sizeof(szCommand), "mount /dev/sdd %s 2>&1", FOLDER_USB_MOUNT);
      hw_execute_bash_command(szCommand, szOutput);
      s_iUSBMounted = 1;
   }

   log_line("[Hardware] USB mount result: [%s]", szOutput);
   if ( 0 == s_iUSBMounted )
      log_line("[Hardware] USB memory stick could NOT be mounted!");
   return s_iUSBMounted;
}

void hardware_unmount_usb()
{
   if ( 0 == s_iUSBMounted )
      return;

   char szCommand[128];
   snprintf(szCommand, sizeof(szCommand), "umount %s 2>/dev/null", FOLDER_USB_MOUNT);
   hw_execute_bash_command(szCommand, NULL);

   hardware_sleep_ms(200);

   snprintf(szCommand, sizeof(szCommand), "rm -rf %s", FOLDER_USB_MOUNT); 
   hw_execute_bash_command(szCommand, NULL);

   s_iUSBMounted = 0;
   log_line("[Hardware] USB memory stick has been unmounted!");
}

u32 _hardware_get_camera_type()
{
   char szBuff[1024];
   char szComm[1024];
   char szOutput[2048];

   s_bHardwareHasCamera = 0;
   s_uHardwareCameraType = CAMERA_TYPE_NONE;

   int retryCount = 3;
   while ( retryCount > 0 )
   {
      if ( hardware_has_i2c_device_id(I2C_DEVICE_ADDRESS_CAMERA_HDMI) )
      {
         log_line("Hardware: HDMI Camera detected.");
         s_bHardwareHasCamera = 1;
         s_uHardwareCameraType = CAMERA_TYPE_HDMI;
         break;
      }
      
      if ( hardware_has_i2c_device_id(I2C_DEVICE_ADDRESS_CAMERA_VEYE) )
      {
         log_line("Hardware: Veye Camera detected.");
         s_bHardwareHasCamera = 1;
         s_uHardwareCameraType = CAMERA_TYPE_VEYE307;

         snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./veye_mipi_i2c.sh -r -f devid; cd $current_dir", VEYE_COMMANDS_FOLDER307);
         hw_execute_bash_command_raw(szComm, szOutput);
         char* szDevId = strstr(szOutput, "device id is");
         //log_line("Output: [%s], dev id: [%s]", szOutput, szDevId);
         if ( szDevId != NULL && strlen(szDevId) >= strlen("device id is")+3 )
         {
         int index = strlen(szDevId)-1;
         while ( szDevId[index] == 10 || szDevId[index] == 13 )
            index--;
         szDevId[index+1] = 0;
         while ( isdigit(szDevId[index]) || (toupper(szDevId[index]) >= 'A' && toupper(szDevId[index]) <= 'F') )
            index--;
         index++;
         if ( szDevId[index] != 0 )
         {
            szDevId += index;
            log_line("Found veye camera device id: %s", szDevId);
            if ( strcmp(szDevId, "6") == 0 )
               s_uHardwareCameraType = CAMERA_TYPE_VEYE327;
            else if ( strcmp(szDevId, "33") == 0 )
               s_uHardwareCameraType = CAMERA_TYPE_VEYE307;
            else
               s_uHardwareCameraType = CAMERA_TYPE_VEYE327;
         }
         }
         if ( s_uHardwareCameraType == CAMERA_TYPE_VEYE307 )
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./camera_i2c_config; cd $current_dir", VEYE_COMMANDS_FOLDER307);
         else
            snprintf(szComm, sizeof(szComm), "current_dir=$PWD; cd %s/; ./camera_i2c_config; cd $current_dir", VEYE_COMMANDS_FOLDER);
         hw_execute_bash_command(szComm, NULL);

         break;
      }

      if ( 0 == s_bHardwareHasCamera )
      {
         szBuff[0] = 0;
         hw_execute_bash_command_raw("vcgencmd get_camera", szBuff);
         log_line("Camera detection response string: %s", szBuff);
         if ( NULL != strstr(szBuff, "detected=1") || 
              NULL != strstr(szBuff, "detected=2") )
         {
            log_line("Hardware: CSI Camera detected.");
            s_bHardwareHasCamera = 1;
            s_uHardwareCameraType = CAMERA_TYPE_CSI;
            break;
         }
      }
      hardware_sleep_ms(400);
      retryCount--;
   }
   
   if ( s_bHardwareHasCamera == 0 )
      log_line("Hardware: No camera detected.");
   else
   {
      str_get_hardware_camera_type_string(s_uHardwareCameraType, szBuff);
      log_line("Hardware: Detected camera type %d: %s", (int)s_uHardwareCameraType, szBuff);
   }

   return s_uHardwareCameraType;
}


int hardware_detectSystemType()
{
   if ( s_bHarwareHasDetectedSystemType )
   {
      log_line("Hardware: System type was already detected. Return it.");
      return s_iHardwareSystemIsVehicle;
   }
   log_line("Hardware: Detecting system type...");

   s_iHardwareSystemIsVehicle = 0;

   int val = GPIORead(GPIO_PIN_DETECT_TYPE);
   if ( val == 1 )
   {
      log_line("Hardware: Detected GPIO signal to start as vehicle or relay.");
      s_iHardwareSystemIsVehicle = 1;
   }
   else if( (access( FILE_FORCE_VEHICLE, R_OK ) != -1) || (access( "/boot/forcevehicle.txt", R_OK ) != -1) )
   {
      log_line("Hardware: Detected file %s to force start as vehicle or relay.", FILE_FORCE_VEHICLE);
      s_iHardwareSystemIsVehicle = 1;
   }

   _hardware_get_camera_type();

   if ( s_bHardwareHasCamera && (s_uHardwareCameraType != CAMERA_TYPE_NONE) )
   {
      log_line("Hardware: Has Camera.");
      s_iHardwareSystemIsVehicle = 1;
   }

   if ( s_iHardwareSystemIsVehicle )
   {
      if ( 0 == s_bHardwareHasCamera )
         log_line("Hardware: Detected system as vehicle/relay without camera.");
      else
         log_line("Hardware: Detected system as vehicle/relay with camera.");
   }
   else
      log_line("Hardware: Detected system as controller.");

   FILE* fd = fopen(FILE_SYSTEM_TYPE, "w");
   if ( NULL != fd )
   {
      fprintf(fd, "%d %d %u\n", s_iHardwareSystemIsVehicle, s_bHardwareHasCamera, s_uHardwareCameraType);
      fclose(fd);
   }

   log_line("Hardware: Detected system Type: %s, has camera: %s, camera type: %u", s_iHardwareSystemIsVehicle?"[vehicle]":"[controller]", s_bHardwareHasCamera?"yes":"no", s_uHardwareCameraType);

   s_bHarwareHasDetectedSystemType = 1;
   return s_iHardwareSystemIsVehicle;
}

void _hardware_load_system_type()
{
   if ( s_bHarwareHasDetectedSystemType )
      return;

   log_line("Hardware: Loading system type config...");
   FILE* fd = fopen(FILE_SYSTEM_TYPE, "r");
   if ( NULL != fd )
   {
      if ( 3 != fscanf(fd, "%d %d %u", &s_iHardwareSystemIsVehicle, &s_bHardwareHasCamera, &s_uHardwareCameraType) )
      {
         log_line("Hardware: Failed to load system type config, invalid config file.");
         hardware_detectSystemType();
      }
      else
      {
         s_bHarwareHasDetectedSystemType = 1;
         log_line("Hardware: Loaded system Type: %s, has camera: %s, camera type: %u", s_iHardwareSystemIsVehicle?"[vehicle]":"[controller]", s_bHardwareHasCamera?"yes":"no", s_uHardwareCameraType);
      }
      fclose(fd);
   }
   else
   {
      log_line("Hardware: Failed to load system type config.");
      hardware_detectSystemType();
   }
   log_line("Hardware: System Type: %s, has camera: %s, camera type: %u", s_iHardwareSystemIsVehicle?"[vehicle]":"[controller]", s_bHardwareHasCamera?"yes":"no", s_uHardwareCameraType);
}

int hardware_is_station()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_load_system_type();
   return (s_iHardwareSystemIsVehicle==1)?0:1;
}

int hardware_is_vehicle()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_load_system_type();
   return s_iHardwareSystemIsVehicle;
}

int hardware_hasCamera()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_load_system_type();
   return s_bHardwareHasCamera;
}

u32 hardware_getCameraType()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_load_system_type();
   return s_uHardwareCameraType;      
}

int hardware_isCameraVeye()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_load_system_type();

   if ( s_uHardwareCameraType == CAMERA_TYPE_VEYE290 ||
        s_uHardwareCameraType == CAMERA_TYPE_VEYE307 ||
        s_uHardwareCameraType == CAMERA_TYPE_VEYE327 )
      return 1;
   return 0;
}

int hardware_isCameraVeye307()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_load_system_type();

   if ( s_uHardwareCameraType == CAMERA_TYPE_VEYE307 )
      return 1;
   return 0;
}


int hardware_isCameraHDMI()
{
   if ( ! s_bHarwareHasDetectedSystemType )
      _hardware_load_system_type();

   if ( s_uHardwareCameraType == CAMERA_TYPE_HDMI )
      return 1;
   return 0;
}

int hardware_sleep_ms(u32 miliSeconds)
{
   return usleep(miliSeconds*1000);

   //struct timespec to_sleep = { 0, miliSeconds*1000*1000 };
   //while ((nanosleep(&to_sleep, &to_sleep) == -1) && (errno == EINTR));
   //return 0;
}

int hardware_sleep_micros(u32 microSeconds)
{
   return usleep(microSeconds);

   //struct timespec to_sleep = { 0, microSeconds*1000 };
   //while ((nanosleep(&to_sleep, &to_sleep) == -1) && (errno == EINTR));
   //return 0;
}

void hardware_recording_led_set_off()
{
   GPIOWrite(GPIO_PIN_RECORDING_LED, LOW);
   s_isRecordingLedOn = 0;
   s_isRecordingLedBlinking = 0;
   s_TimeLastRecordingLedBlink = get_current_timestamp_ms();
}

void hardware_recording_led_set_on()
{
   GPIOWrite(GPIO_PIN_RECORDING_LED, HIGH);
   s_isRecordingLedOn = 1;
   s_isRecordingLedBlinking = 0;
   s_TimeLastRecordingLedBlink = get_current_timestamp_ms();
}

void hardware_recording_led_set_blinking()
{
   GPIOWrite(GPIO_PIN_RECORDING_LED, HIGH);
   s_isRecordingLedOn = 1;
   s_isRecordingLedBlinking = 1;
   s_TimeLastRecordingLedBlink = get_current_timestamp_ms();
}

void hardware_mount_root()
{
   hw_execute_bash_command("sudo mount -o remount,rw /", NULL);
}

void hardware_mount_boot()
{
   hw_execute_bash_command("sudo mount -o remount,rw /boot", NULL);
}

int hardware_has_eth()
{
   int nHasETH = 1;
   char szOutput[1024];
   hw_execute_bash_command_raw("ifconfig eth0 2>&1", szOutput);
   if ( 0 == szOutput[0] || NULL != strstr(szOutput, "not found") )
   {
      nHasETH = 0;
      szOutput[1023] = 0;
      log_line("ETH not found. Response: [%s]", szOutput);
   }
   return nHasETH;
}

int hardware_get_cpu_speed()
{
   char szOutput[64];
   szOutput[0] = 0;
   hw_execute_bash_command_raw_silent("vcgencmd measure_clock arm", szOutput);

   if ( 0 == szOutput[0] )
      return 0;

   char* p = &szOutput[0];
   int pos = 0;
   while ( (*p) && (*p != '=') && pos < 63 )
   {
      p++;
      pos++;
   }

   if ( 0 == (*p) || pos >= 63 )
      return 0;

   p++;
   int len = strlen(p);
   if ( len > 6 )
   {
     if ( p[len-1] == 10 || p[len-1] == 13 )
     {
        p[len-1] = 0;
        len--;
     }
     if ( p[len-1] == 10 || p[len-1] == 13 )
     {
        p[len-1] = 0;
        len--;
     }
     if ( len > 6 )
        p[len-6] = 0;
   }
   return atoi(p);
}

int hardware_get_gpu_speed()
{
   char szOutput[64];
   szOutput[0] = 0;
   hw_execute_bash_command_raw_silent("vcgencmd measure_clock core", szOutput);

   if ( 0 == szOutput[0] )
      return 0;

   char* p = &szOutput[0];
   int pos = 0;
   while ( (*p) && (*p != '=') && pos < 63 )
   {
      p++;
      pos++;
   }

   if ( 0 == (*p) || pos >= 63 )
      return 0;

   p++;
   int len = strlen(p);
   if ( len > 6 )
   {
     if ( p[len-1] == 10 || p[len-1] == 13 )
     {
        p[len-1] = 0;
        len--;
     }
     if ( p[len-1] == 10 || p[len-1] == 13 )
     {
        p[len-1] = 0;
        len--;
     }
     if ( len > 6 )
        p[len-6] = 0;
   }
   return atoi(p);
}
