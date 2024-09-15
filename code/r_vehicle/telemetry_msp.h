#pragma once
#include "../base/base.h"
#include "../base/config.h"

void telemetry_msp_on_open_port(int iSerialPortFile);
void telemetry_msp_on_close();
void telemetry_msp_periodic_loop();
u32 telemetry_msp_get_last_command_received_time();

// Returns true if a new message was found
bool telemetry_msp_on_new_serial_data(u8* pData, int iDataLength);
