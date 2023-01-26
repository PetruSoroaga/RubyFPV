#pragma once

#ifdef __cplusplus
extern "C" {
#endif 

#define MAX_SERIAL_PORT_NAME 64
#define MAX_SERIAL_PORTS 10

#define SERIAL_PORT_USAGE_NONE 0
#define SERIAL_PORT_USAGE_TELEMETRY 2
#define SERIAL_PORT_USAGE_TELEMETRY_MAVLINK 3
#define SERIAL_PORT_USAGE_TELEMETRY_LTM 4
#define SERIAL_PORT_USAGE_MSP_OSD 5
#define SERIAL_PORT_USAGE_MSP_OSD_PITLAB 6
#define SERIAL_PORT_USAGE_DATA_LINK 10
#define SERIAL_PORT_USAGE_CORE_PLUGIN 20

typedef struct
{
   char szName[MAX_SERIAL_PORT_NAME];
   char szPortDeviceName[MAX_SERIAL_PORT_NAME];
   long lPortSpeed;
   int iPortUsage;
   int iSupported;
} __attribute__((packed)) hw_serial_port_info_t;


int* hardware_get_serial_baud_rates();
int hardware_get_serial_baud_rates_count();

int hardware_init_serial_ports();
void hardware_reload_serial_ports();
void hardware_serial_save_configuration();

int hardware_has_unsupported_serial_ports();

int hardware_get_serial_ports_count();
hw_serial_port_info_t* hardware_get_serial_port_info(int iPortIndex);

int hardware_configure_serial(const char* szDevName, long baudRate);
int hardware_open_serial_port(const char* szDevName, long baudRate);


#ifdef __cplusplus
}  
#endif 