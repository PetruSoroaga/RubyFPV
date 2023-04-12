#pragma once

void video_link_adaptive_init();
void video_line_adaptive_switch_to_med_level();
void video_link_adaptive_set_intial_video_adjustment_level(int iCurrentVideoProfile, u8 uDataPackets, u8 uECPackets);

void video_link_adaptive_periodic_loop();
