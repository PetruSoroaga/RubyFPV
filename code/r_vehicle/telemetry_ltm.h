#pragma once
#include "../base/base.h"
#include "../base/config.h"

void telemetry_ltm_on_open_port(int iSerialPortFile);
void telemetry_ltm_on_close();
void telemetry_ltm_periodic_loop();
