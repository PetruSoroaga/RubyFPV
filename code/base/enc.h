#pragma once
#include "config_hw.h"

#ifdef HW_CAPABILITY_WFBOHD

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif 

int load_openipc_keys(const char* szFileName);
uint8_t* get_openipc_key1();
uint8_t* get_openipc_key2();

#ifdef __cplusplus
}
#endif

#endif