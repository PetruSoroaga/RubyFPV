#pragma once

void broadcast_router_ready();
bool links_set_cards_frequencies_and_params(int iVehicleLinkId);
bool links_set_cards_frequencies_for_search( u32 iSearchFreq, bool bSiKSearch, int iAirDataRate, int iECC, int iLBT, int iMCSTR );

void reasign_radio_links(bool bSilent);

void video_processors_init();
void video_processors_cleanup();
