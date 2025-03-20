#pragma once

#include "../base/base.h"

int negociate_radio_process_received_radio_link_messages(u8* pPacketBuffer);
void negociate_radio_periodic_loop();

bool negociate_radio_link_is_in_progress();



