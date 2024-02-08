#pragma once

void close_and_mark_sik_interfaces_to_reopen();
void reopen_marked_sik_interfaces();
void flag_need_video_capture_restart();
void flag_update_sik_interface(int iInterfaceIndex);
void flag_reinit_sik_interface(int iInterfaceIndex);
void reinit_radio_interfaces();
void send_radio_config_to_controller();
