#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/shared_mem.h"
#include "shared_vars.h"

void osd_stats_init();

float osd_render_stats_radio_links_get_height(shared_mem_radio_stats* pStats, float scale);
float osd_render_stats_radio_links_get_width(shared_mem_radio_stats* pStats, float scale);
float osd_render_stats_radio_links( float xPos, float yPos, const char* szTitle, shared_mem_radio_stats* pStats, float scale);

float osd_render_stats_radio_interfaces_get_height(shared_mem_radio_stats* pStats, float scale);
float osd_render_stats_radio_interfaces_get_width(shared_mem_radio_stats* pStats, float scale);
float osd_render_stats_radio_interfaces( float xPos, float yPos, const char* szTitle, shared_mem_radio_stats* pStats, float scale);

float osd_render_stats_video_decode_get_height(float scale);
float osd_render_stats_video_decode(float xPos, float yPos, float scale);

float osd_render_stats_rc_get_height(float scale);
float osd_render_stats_rc(float xPos, float yPos, float scale);

float osd_render_stats_flight_end(float scale);
float osd_render_stats_flights(float scale);

float osd_render_stats_efficiency_get_height(float scale);
float osd_render_stats_efficiency(float xPos, float yPos, float scale);

float osd_render_stats_dev(float xPos, float yPos, float scale);

void osd_render_stats_panels();
