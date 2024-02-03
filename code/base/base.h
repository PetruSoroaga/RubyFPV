#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <resolv.h>
#include <string.h>
#include <utime.h>
#include <unistd.h>
#include <getopt.h>
#include <pcap.h>
#include <endian.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <signal.h>
#include "build.h"

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef u32 __le32;

#define MAX_U32 0xFFFFFFFF
#define MAX_VEHICLE_NAME_LENGTH 16
#define MAX_SERVICE_LOG_ENTRY_LENGTH 300

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le16_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#else
#define le16_to_cpu(x) ((((x)&0xff)<<8)|(((x)&0xff00)>>8))
#define le32_to_cpu(x) \
((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)&0xff0000)>>8)|(((x)&0xff000000)>>24))
#endif
#define unlikely(x) (x)

typedef struct
{
    long type;
    char text[MAX_SERVICE_LOG_ENTRY_LENGTH];
} type_log_message_buffer;


#ifdef __cplusplus
extern "C" {
#endif 

u32 revert_word(u32 input);

u32 base_compute_crc32(u8 *buf, int length);
u8 base_compute_crc8(u8* pBuffer, int iLength);
int base_check_crc32(u8* pBuffer, int iLength);

void init_boot_timestamp(); // should be called only once per boot
u32 get_current_timestamp_micros();
u32 get_current_timestamp_ms();
u32 get_boot_timestamp_ms();
int is_first_boot();

char* removeTrailingZero(char* szBuff);

void log_init_local_only(const char* component_name);
void log_init(const char* component_name);
void log_add_file(const char* szFileName);
void log_disable();
void log_disable_stdout();
void log_enable_stdout();
void log_only_errors();
void log_format_time(u32 miliseconds, char* szOutTime);
void log_line(const char* format, ...);
void log_buffer(const u8* buffer, int size);
void log_buffer1(const u8* buffer, int size, int delim1);
void log_buffer2(const u8* buffer, int size, int delim1, int delim2);
void log_buffer3(const u8* buffer, int size, int delim1, int delim2, int delim3);
void log_buffer4(const u8* buffer, int size, int delim1, int delim2, int delim3, int delim4);
void log_buffer5(const u8* buffer, int size, int delim1, int delim2, int delim3, int delim4, int delim5);
void log_dword(const char* szText, u32 value);
void log_dword_bits(const char* szText, u32 value);

void log_softerror_and_alarm(const char* format, ...);
void log_error_and_alarm(const char* format, ...);
void log_line_watchdog(const char* format, ...);
void log_line_commands(const char* format, ...);

long metersBetweenPlaces(double lat1, double lon1, double lat2, double lon2);
long distance_meters_between(double lat1, double lon1, double lat2, double lon2);

int check_licences();

long get_filesize(const char* szFileName);

#ifdef __cplusplus
}  
#endif 
