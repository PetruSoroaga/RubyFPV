#pragma once
#include "../base/base.h"
#include "../base/models.h"

bool test_link_is_in_progress();
void test_link_process_received_message(int iInterfaceIndex, u8* pPacketBuffer);

void test_link_loop();
