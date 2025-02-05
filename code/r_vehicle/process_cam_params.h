#pragma once

#include "../base/base.h"
#include "../base/config.h"

void process_camera_params_changed(u8* pPacketBuffer, int iPacketLength);
void process_camera_periodic_loop();
