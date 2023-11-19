#pragma once

void video_link_keyframe_init(u32 uVehicleId);
void video_link_keyframe_set_intial_received_level(u32 uVehicleId, int iReceivedKeyframeMs);
void video_link_keyframe_set_current_level_to_request(u32 uVehicleId, int iKeyframeMs);
void video_link_keyframe_periodic_loop();
