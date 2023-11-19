#pragma once
#include "../base/base.h"

void notification_add_armed(u32 uVehicleId);
void notification_add_disarmed(u32 uVehicleId);
void notification_add_flight_mode(u32 uVehicleId, u32 flightMode);

void notification_add_rc_failsafe(u32 uVehicleId);
void notification_add_rc_failsafe_cleared(u32 uVehicleId);

void notification_add_model_deleted();


void notification_add_recording_start();
void notification_add_recording_end();

void notification_add_rc_output_enabled();
void notification_add_rc_output_disabled();

void notification_add_frequency_changed(int nLinkId, u32 uFreq);

void notification_add_start_pairing();
void notification_remove_start_pairing();
