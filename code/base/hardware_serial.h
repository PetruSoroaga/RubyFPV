#pragma once
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif 

#define MAX_SERIAL_PORT_NAME 64
#define MAX_SERIAL_PORTS 6

#define SERIAL_PORT_USAGE_NONE 0
#define SERIAL_PORT_USAGE_TELEMETRY_MAVLINK 3
#define SERIAL_PORT_USAGE_TELEMETRY_LTM 4
#define SERIAL_PORT_USAGE_MSP_OSD 5
#define SERIAL_PORT_USAGE_DATA_LINK 10
#define SERIAL_PORT_USAGE_SIK_RADIO 11
#define SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_433 12
#define SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_868 13
#define SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_915 14
#define SERIAL_PORT_USAGE_SERIAL_RADIO_ELRS_24 15
#define SERIAL_PORT_USAGE_CORE_PLUGIN 200

typedef struct
{
   char szName[MAX_SERIAL_PORT_NAME];
   char szPortDeviceName[MAX_SERIAL_PORT_NAME];
   long lPortSpeed;
   int iPortUsage;
   int iSupported;
} ALIGN_STRUCT_SPEC_INFO hw_serial_port_info_t;


int* hardware_get_serial_baud_rates();
int hardware_get_serial_baud_rates_count();

int hardware_init_serial_ports();
int hardware_reload_serial_ports_settings();
void hardware_serial_save_configuration();

int hardware_has_unsupported_serial_ports();

int hardware_get_serial_ports_count();
hw_serial_port_info_t* hardware_get_serial_port_info(int iPortIndex);
hw_serial_port_info_t* hardware_get_serial_port_info_from_serial_port_name(char* szPortName);

int hardware_configure_serial(const char* szDevName, long baudRate);
int hardware_open_serial_port(const char* szDevName, long baudRate);

int hardware_serial_is_sik_radio(const char* szDevName);
int hardware_serial_send_sik_command(int iSerialPortFD, const char* szCommand);
int hardware_serial_wait_sik_response(int iSerialPortFD, int iTimeoutMS, int iMinimumLines, u8* pOutputBuffer, int* pInOutputLength);

#ifdef __cplusplus
}  
#endif 