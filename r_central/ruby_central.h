#pragma once
#include "../base/base.h"
#include "../base/hw_procs.h"


#define START_SEQ_DELAY 100

#define START_SEQ_NONE 0
#define START_SEQ_PRE_LOAD_CONFIG 1
#define START_SEQ_LOAD_CONFIG 2
#define START_SEQ_PRE_SEARCH_DEVICES 3
#define START_SEQ_SEARCH_DEVICES 4
#define START_SEQ_PRE_SEARCH_INTERFACES 5
#define START_SEQ_SEARCH_INTERFACES 6
#define START_SEQ_PRE_NICS 10
#define START_SEQ_NICS 11
#define START_SEQ_PRE_SYNC_DATA 15
#define START_SEQ_SYNC_DATA 16
#define START_SEQ_SYNC_DATA_FAILED 17

#define START_SEQ_PRE_LOAD_DATA 30
#define START_SEQ_LOAD_DATA 31
#define START_SEQ_START_PROCESSES 40
#define START_SEQ_SCAN_MEDIA_PRE 50
#define START_SEQ_SCAN_MEDIA 51
#define START_SEQ_PROCESS_VIDEO 60

#define START_SEQ_COMPLETED 200
#define START_SEQ_FAILED 201



void ruby_input_loop(bool bNoKeys);

void render_all(u32 timeNow, bool bForceBackground = false, bool bDoInputLoop = false);
void ruby_start_recording();
void ruby_stop_recording();

bool ruby_is_first_pairing_done();
void ruby_set_is_first_pairing_done();

void ruby_reload_osd_fonts();
void ruby_reload_menu_font();

void ruby_load_models();

void ruby_signal_alive();

void ruby_pause_watchdog();
void ruby_resume_watchdog();
