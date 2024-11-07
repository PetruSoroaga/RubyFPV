#pragma once
#include "../base/base.h"
#include "../base/config.h"

bool hardware_file_check_and_fix_access(char* szFullFileName);


#ifdef __cplusplus
extern "C" {
#endif 

void hardware_mount_root();
void hardware_mount_boot();
int hardware_get_free_space_kb();

#ifdef __cplusplus
}  
#endif 
