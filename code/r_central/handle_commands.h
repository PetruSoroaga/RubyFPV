#pragma once
#include "../base/base.h"
#include "../base/commands.h"
#include "../radio/radiopackets2.h"

bool handle_commands_start_on_pairing();
bool handle_commands_stop_on_pairing();

int handle_commands_on_full_model_settings_received(u32 uVehicleId, int iResponseParam, u8* pData, int iLength);
u8* handle_commands_get_last_command_response();

void handle_commands_loop();

void handle_commands_on_response_received(u8* pPacketBuffer, int iLength);
u32  handle_commands_get_last_command_id_response_received();
bool handle_commands_last_command_succeeded();

u32 handle_commands_increment_command_counter();
u32 handle_commands_decrement_command_counter();

bool handle_commands_send_to_vehicle(u8 commandType, u32 param, u8* pBuffer, int length);
bool handle_commands_send_command_once_to_vehicle(u8 commandType, u8 resendCounter, u32 param, u8* pBuffer, int length);
bool handle_commands_send_single_oneway_command(u8 resendCounter, u8 commandType, u32 param, u8* pBuffer, int length);
bool handle_commands_send_single_oneway_command(u8 resendCounter, u8 commandType, u32 param, u8* pBuffer, int length, int delayMs);
bool handle_commands_send_single_oneway_command_to_vehicle(u32 uVehicleId, u8 resendCounter, u8 commandType, u32 param, u8* pBuffer, int length, int delayMs);

bool handle_commands_send_ruby_message(t_packet_header* pPH, u8* pBuffer, int length);

void handle_commands_abandon_command();
bool handle_commands_is_command_in_progress();
void handle_commands_show_popup_progress();

void handle_commands_reset_has_received_vehicle_core_plugins_info();
bool handle_commands_has_received_vehicle_core_plugins_info();

void handle_commands_initiate_file_upload(u32 uFileId, const char* szFileName);

bool handle_commands_send_developer_flags(int iEnableDevMode, u32 uDevFlags);
