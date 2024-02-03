#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../radio/radiopackets2.h"

bool test_link_is_in_progress();
int test_link_active();
u8* test_link_generate_start_packet(u32 uVehicleId, int iLinkId, type_radio_links_parameters* pRadioParamsToTest);
void test_link_send_status_message_to_central(const char* szMsg);
void test_link_send_end_message_to_central(bool bSucceeded);

bool test_link_start(u32 uControllerId, u32 uVehicleId, int iLinkId, type_radio_links_parameters* pRadioParamsToTest);
void test_link_end_flow(bool bSucceeded);

void test_link_process_received_message(int iInterfaceIndex, u8* pPacketBuffer);
void test_link_loop();
