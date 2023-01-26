#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../radio/radiopackets2.h"

extern bool s_bHasReceivedGPSInfo;
extern bool s_bHasReceivedGPSPos;
extern bool s_bHasReceivedHeartbeat;


void parse_telemetry_init(u32 vehicleMavId);
void parse_telemetry_allow_any_sysid(int iAllow);

bool parse_telemetry_from_fc( u8* buffer, int length, t_packet_header_fc_telemetry* pphfct, t_packet_header_ruby_telemetry_extended* pPHRTE, u8 vehicleType, int telemetry_type );
bool has_received_gps_info();
bool has_received_flight_mode();
void reset_last_message();
u32 get_last_message_time();
char* get_last_message();
