#pragma once
#include "../base/base.h"
#include "../radio/radiolink.h"

void radio_links_close_and_mark_sik_interfaces_to_reopen();
void radio_links_reopen_marked_sik_interfaces();
void radio_links_flag_update_sik_interface(int iInterfaceIndex);
void radio_links_flag_reinit_sik_interface(int iInterfaceIndex);
int radio_links_check_reinit_sik_interfaces();

