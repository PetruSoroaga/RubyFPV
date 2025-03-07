#pragma once

void adaptive_video_init();
void adaptive_video_pause(u32 uMilisec);
void adaptive_video_on_new_vehicle(int iRuntimeIndex);

void adaptive_video_switch_to_video_profile(int iVideoProfile, u32 uVehicleId);
void adaptive_video_received_video_profile_switch_confirmation(u32 uRequestId, u8 uVideoProfile, u32 uVehicleId, int iInterfaceIndex);
void adaptive_video_periodic_loop(bool bForceSyncNow);
