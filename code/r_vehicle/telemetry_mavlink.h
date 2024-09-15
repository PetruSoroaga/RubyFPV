#pragma once
#include "../base/base.h"
#include "../base/config.h"

void telemetry_mavlink_on_open_port(int iSerialPortFile);
void telemetry_mavlink_on_close();
void telemetry_mavlink_periodic_loop();
void telemetry_mavlink_on_second_lapse();

// Returns true if a new message was found
bool telemetry_mavlink_on_new_serial_data(u8* pData, int iDataLength);

void telemetry_mavlink_send_to_controller();
