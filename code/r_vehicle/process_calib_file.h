#pragma once
#include "../base/base.h"

void process_calibration_files_init();
bool process_calibration_file_segment(u32 command_param, u8* pBuffer, int length);
