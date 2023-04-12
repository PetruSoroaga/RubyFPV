#pragma once

#include "../base/base.h"

#define IPC_CHANNEL_TYPE_ROUTER_TO_CENTRAL 201
#define IPC_CHANNEL_TYPE_CENTRAL_TO_ROUTER 202
#define IPC_CHANNEL_TYPE_ROUTER_TO_TELEMETRY 203
#define IPC_CHANNEL_TYPE_TELEMETRY_TO_ROUTER 204
#define IPC_CHANNEL_TYPE_ROUTER_TO_RC 205
#define IPC_CHANNEL_TYPE_RC_TO_ROUTER 206
#define IPC_CHANNEL_TYPE_ROUTER_TO_COMMANDS 207
#define IPC_CHANNEL_TYPE_COMMANDS_TO_ROUTER 208

#define ICP_CHANNEL_MAX_MSG_SIZE 1600

#define RUBY_PIPES_EXTRA_FLAGS O_NONBLOCK

#ifdef __cplusplus
extern "C" {
#endif 


int ruby_init_ipc_channels();
void ruby_clear_all_ipc_channels();

int ruby_open_ipc_channel_write_endpoint(int nChannelType);
int ruby_open_ipc_channel_read_endpoint(int nChannelType);

int ruby_close_ipc_channel(int iChannelFd);


int ruby_ipc_channel_send_message(int iChannelFd, u8* pMessage, int iLength);
u8* ruby_ipc_try_read_message(int iChannelFd, int timeoutMicrosec, u8* pTempBuffer, int* pTempBufferPos, u8* pOutputBuffer);


#ifdef __cplusplus
}  
#endif 
