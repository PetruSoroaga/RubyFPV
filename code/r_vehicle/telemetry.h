#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../radio/radiopackets2.h"

void telemetry_init();

bool telemetry_detect_serial_port_to_use();
int telemetry_open_serial_port();
int telemetry_close_serial_port();
u32 telemetry_last_time_opened();
u32 telemetry_time_last_telemetry_received();
void telemetry_reset_time_last_telemetry_received();
int telemetry_get_serial_port_file();

int telemetry_try_read_serial_port();
void telemetry_periodic_loop();

t_packet_header_fc_telemetry* telemetry_get_fc_telemetry_header();
t_packet_header_fc_extra* telemetry_get_fc_extra_telemetry_header();
