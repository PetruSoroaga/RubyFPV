#pragma once

#ifdef __cplusplus
extern "C" {
#endif 

// Power is 0..71
void hardware_radio_set_txpower_rtl8812au(int iTxPower);
void hardware_radio_set_txpower_rtl8812eu(int iTxPower);
void hardware_radio_set_txpower_atheros(int iTxPower);

#ifdef __cplusplus
}  
#endif
