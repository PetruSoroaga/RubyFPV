#pragma once
#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/shared_mem.h"
#include "../shared_vars.h"

void osd_stats_set_last_tx_overload_time_ms(int iMS);

void osd_render_stats_full_rx_port();

float osd_render_stats_radio_interfaces_get_height(shared_mem_radio_stats* pStats);
float osd_render_stats_radio_interfaces_get_width(shared_mem_radio_stats* pStats);
float osd_render_stats_radio_interfaces( float xPos, float yPos, const char* szTitle, shared_mem_radio_stats* pStats);

float osd_render_stats_local_radio_links_get_height(shared_mem_radio_stats* pStats, float scale);
float osd_render_stats_local_radio_links_get_width(shared_mem_radio_stats* pStats, float scale);
float osd_render_stats_local_radio_links( float xPos, float yPos, const char* szTitle, shared_mem_radio_stats* pStats, float scale);

float osd_render_stats_radio_rx_type_history_get_height(bool bVehicle);
float osd_render_stats_radio_rx_type_history_get_width(bool bVehicle);
float osd_render_stats_radio_rx_type_history( float xPos, float yPos, bool bVehicle);
