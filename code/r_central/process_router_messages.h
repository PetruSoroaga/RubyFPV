#pragma once

#include "../base/base.h"

void start_pipes_to_router();
void stop_pipes_to_router();

void send_model_changed_message_to_router(u32 uChangeType);
void send_control_message_to_router(u8 packet_type, u32 extraParam);
void send_control_message_to_router_and_data(u8 packet_type, u8* pData, int nDataLength);
void send_packet_to_router(u8* pPacket, int nLength);

int try_read_messages_from_router(u32 uMaxMiliseconds);

u8* get_last_command_reply_from_router();