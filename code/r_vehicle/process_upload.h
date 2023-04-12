#pragma once
#include "../base/base.h"

void process_sw_upload_init();
void process_sw_upload_old(u32 command_param, u8* pBuffer, int length);
void process_sw_upload_new(u32 command_param, u8* pBuffer, int length);