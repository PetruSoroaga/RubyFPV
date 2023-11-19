#pragma once

void video_link_adaptive_init(u32 uVehicleId);
void video_link_adaptive_switch_to_med_level(u32 uVehicleId);
void video_link_adaptive_set_intial_video_adjustment_level(u32 uVehicleId, int iCurrentVideoProfile, u8 uDataPackets, u8 uECPackets);

void video_link_adaptive_periodic_loop();
