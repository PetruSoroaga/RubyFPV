#pragma once

bool pairing_start();
bool pairing_stop();
void pairing_on_router_ready();

bool pairing_isStarted();
bool pairing_isReceiving();
bool pairing_wasReceiving();
bool pairing_hasReceivedVideoStreamData();
u32 pairing_getCurrentVehicleID();
u32 pairing_getStartTime();
bool pairing_is_connected_to_wrong_model();
void pairing_loop();
