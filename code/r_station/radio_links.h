#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../radio/radiolink.h"


int radio_links_has_failed_interfaces();

void radio_links_reinit_radio_interfaces();

void radio_links_close_rxtx_radio_interfaces();
void radio_links_open_rxtx_radio_interfaces_for_search( u32 uSearchFreq );
void radio_links_open_rxtx_radio_interfaces();

// Returns positive is there are packets to read from auxiliary radio links
pcap_t* radio_links_get_auxiliary_link(int iIndex);
int radio_links_check_read_auxiliary_links();
int radio_links_is_auxiliary_link_signaled(int iIndex);

bool radio_links_apply_settings(Model* pModel, int iRadioLink, type_radio_links_parameters* pRadioLinkParamsOld, type_radio_links_parameters* pRadioLinkParams);
