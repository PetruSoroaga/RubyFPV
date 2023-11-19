#pragma once

bool pairing_start_normal();
bool pairing_start_search_mode(int iFreqKhz);
bool pairing_start_search_sik_mode(int iFreqKhz, int iAirDataRate, int iECC, int iLBT, int iMCSTR);

bool pairing_stop();
void pairing_on_router_ready();

bool pairing_isStarted();
void pairing_loop();
