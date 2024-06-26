#pragma once

void close_and_mark_sik_interfaces_to_reopen();
void reopen_marked_sik_interfaces();
void video_source_capture_mark_needs_update_or_restart(u32 uChangeType);
void flag_update_sik_interface(int iInterfaceIndex);
void flag_reinit_sik_interface(int iInterfaceIndex);
void reinit_radio_interfaces();
void send_radio_config_to_controller();
void send_radio_reinitialized_message();

u32 get_video_capture_start_program_time();
