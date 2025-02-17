#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../radio/radiolink.h"


int radio_links_has_failed_interfaces();

void radio_links_reinit_radio_interfaces();

void radio_links_compute_set_tx_powers();
void radio_links_close_rxtx_radio_interfaces();
void radio_links_open_rxtx_radio_interfaces_for_search( u32 uSearchFreq );
void radio_links_open_rxtx_radio_interfaces();

bool radio_links_apply_settings(Model* pModel, int iRadioLink, type_radio_links_parameters* pRadioLinkParamsOld, type_radio_links_parameters* pRadioLinkParamsNew);

void radio_links_set_monitor_mode();
u32 radio_links_get_last_set_monitor_time();
