#pragma once

void video_stats_overwrites_init();
void video_stats_overwrites_switch_to_profile_and_level(int iTotalLevelsShift, int iVideoProfile, int iLevelShift);
void video_stats_overwrites_reset_to_highest_level();
void video_stats_overwrites_reset_to_forced_profile();
void video_stats_overwrites_periodic_loop();

bool video_stats_overwrites_increase_videobitrate_overwrite(u32 uCurrentTotalBitrate);
bool video_stats_overwrites_decrease_videobitrate_overwrite();

// Returns in bps or negative for MCS rates
int video_stats_overwrites_get_current_radio_datarate_video(int iRadioLink, int iRadioInterface);
int video_stats_overwrites_get_next_level_down_radio_datarate_video(int iRadioLink, int iRadioInterface);

u32 video_stats_overwrites_get_time_last_shift_down();
