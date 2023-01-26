#pragma once

#define IN  0
#define OUT 1

#define LOW  0
#define HIGH 1


#ifdef __cplusplus
extern "C" {
#endif 

int GPIOExport(int pin);
int GPIOUnexport(int pin);
int GPIODirection(int pin, int dir);
int GPIORead(int pin);
int GPIOWrite(int pin, int value);

#ifdef __cplusplus
}  
#endif 
