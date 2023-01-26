#pragma once
#include "../base/base.h"

void signalReboot();
void sendControlMessage(u8 packet_type, u32 extraParam);
void sendCommandReply(u8 responseFlags, int delayMiliSec);