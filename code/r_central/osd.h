#pragma once

bool osd_is_debug();
float osd_show_home(float xPos, float yPos, bool showHeading, float fScale);
float osd_get_link_bars_width(float fScale);
float osd_get_link_bars_height(float fScale);
float osd_show_link_bars(float xPos, float yPos, float fQuality, float fScale);
float osd_render_radio_link_tag(float xPos, float yPos, int iRadioLink, bool bVehicle, bool bDraw);

void osd_render_all();
void osd_render_elements();
void osd_render_instruments();
void osd_render_stats();
void osd_render_warnings();

void osd_add_stats_flight_end();
void osd_remove_stats_flight_end();

void osd_add_stats_total_flights();
