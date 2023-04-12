#pragma once
#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/ctrl_interfaces.h"
#include "../base/hardware.h"
#include "../base/shared_mem_i2c.h"


float compute_controller_rc_value(Model* pModel, int nChannel, float prevRCValue, t_shared_mem_i2c_controller_rc_in* pRCIn, hw_joystick_info_t* pJoystick, t_ControllerInputInterface* pCtrlInterface, u32 miliSec);
u32 get_rc_channel_failsafe_type(Model* pModel, int nChannel);
int get_rc_channel_failsafe_value(Model* pModel, int nChannel, int prevRCValue);

float compute_output_rc_value(Model* pModel, int nChannel, float prevRCValue, float fNormalizedValue, u32 miliSec);
