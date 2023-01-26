#pragma once

void link_watch_init();
void link_watch_reset();
void link_watch_mark_started_video_processing();

void link_watch_loop();

bool link_has_fc_telemetry();
