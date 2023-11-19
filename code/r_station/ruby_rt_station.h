#pragma once

void close_and_mark_sik_interfaces_to_reopen();
void reopen_marked_sik_interfaces();
void flag_update_sik_interface(int iInterfaceIndex);
void flag_reinit_sik_interface(int iInterfaceIndex);
void broadcast_router_ready();
bool links_set_cards_frequencies_and_params(int iVehicleLinkId);
bool links_set_cards_frequencies_for_search( u32 iSearchFreq, bool bSiKSearch, int iAirDataRate, int iECC, int iLBT, int iMCSTR );

void reasign_radio_links();
