#pragma once

void adaptive_video_init();

void adaptive_video_set_kf_for_current_video_profile(u16 uKeyframe);
void adaptive_video_set_last_kf_requested_by_controller(u16 uKeyframe);
void adaptive_video_set_last_profile_requested_by_controller(int iVideoProfile);
int adaptive_video_get_current_active_video_profile();
u16 adaptive_video_get_current_kf();

void adaptive_video_set_temporary_bitrate(u32 uBitrateBPS);

bool adaptive_video_is_in_degraded_state();
u32  adaptive_video_get_degraded_ec_packets_count(); 
void adaptive_video_on_uplink_lost();
void adaptive_video_on_uplink_recovered();
void adaptive_video_on_user_video_bitrate_changed(u32 uOldBitrate, u32 uNewBitrate);
void adaptive_video_on_capture_restarted();
void adaptive_video_on_new_camera_read(bool bIsEndOfFrame);
void adaptive_video_periodic_loop();
