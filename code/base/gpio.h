#pragma once

#define IN  0
#define OUT 1

#define LOW  0
#define HIGH 1

// Buttons are pull down or pulled up. 0 logic is taking them down to 0v. 1 logic is taking them to 3.3v

#define GPIO_PIN_MENU 23
#define GPIO_PIN_BACK 24
#define GPIO_PIN_PLUS 22
#define GPIO_PIN_MINUS 27

#define GPIO_PIN_QACTION1 17
#define GPIO_PIN_QACTION2 4
#define GPIO_PIN_QACTION2_2 25
#define GPIO_PIN_QACTION3 18
#define GPIO_PIN_QACTIONPLUS 16
#define GPIO_PIN_QACTIONMINUS 20


#define GPIO_PIN_DETECT_TYPE_VEHICLE 19
#define GPIO_PIN_DETECT_TYPE_CONTROLLER 26

#define GPIO_PIN_LED_ERROR 5
#define GPIO_PIN_RECORDING_LED 21
#define GPIO_PIN_BUZZER 26

#ifdef __cplusplus
extern "C" {
#endif 

int GPIOExport(int pin);
int GPIOUnexport(int pin);
int GPIODirection(int pin, int dir);
int GPIORead(int pin);
int GPIOWrite(int pin, int value);

int GPIOInitButtons();
int GPIOButtonsResetDetectionFlag();
int GPIOButtonsTryDetectPullUpDown();

int GPIOGetButtonsPullDirection();

#ifdef __cplusplus
}  
#endif 
