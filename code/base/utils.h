#pragma once
#include <pthread.h>

#include "../base/base.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/ctrl_interfaces.h"
#include "../base/hardware.h"
#include "../base/shared_mem_i2c.h"


typedef struct
{
   // Worker thead info
   bool bConfiguringSiKThreadWorking; // reinitialization worker thread is active
   pthread_t  pThreadSiKReinit;
   int iThreadRetryCounter;
   bool bMustReinitSiKInterfaces; // true if SiK interfaces must be reinitialized (using a worker thread)
   int iMustReconfigureSiKInterfaceIndex; // 0 or positive if SiK interface must be reconfigured and reinitialized (using a worker thread)
   u32  uTimeLastSiKReinitCheck;
   u32  uTimeIntervalSiKReinitCheck;
   u32  uSiKInterfaceIndexThatBrokeDown;

   // Helper configure tool info
   bool bConfiguringToolInProgress; // SiK configuring is in progress using the helper tool
   u32  uTimeStartConfiguring; // start time of the SiK configure helper tool
   
   bool bInterfacesToReopen[MAX_RADIO_INTERFACES]; // SiK interfaces to reopen after the worker thread or helper tool finishes

} __attribute__((packed)) t_sik_radio_state;


bool ruby_is_first_pairing_done();
void ruby_set_is_first_pairing_done();

void reset_sik_state_info(t_sik_radio_state* pState);

float compute_controller_rc_value(Model* pModel, int nChannel, float prevRCValue, t_shared_mem_i2c_controller_rc_in* pRCIn, hw_joystick_info_t* pJoystick, t_ControllerInputInterface* pCtrlInterface, u32 miliSec);
u32 get_rc_channel_failsafe_type(Model* pModel, int nChannel);
int get_rc_channel_failsafe_value(Model* pModel, int nChannel, int prevRCValue);

float compute_output_rc_value(Model* pModel, int nChannel, float prevRCValue, float fNormalizedValue, u32 miliSec);

u32 utils_get_max_allowed_video_bitrate_for_profile(Model* pModel, int iProfile);
u32 utils_get_max_allowed_video_bitrate_for_profile_or_user_video_bitrate(Model* pModel, int iProfile);
u32 utils_get_max_allowed_video_bitrate_for_profile_and_level(Model* pModel, int iProfile, int iLevel);
int utils_get_video_profile_mq_radio_datarate(Model* pModel);
int utils_get_video_profile_lq_radio_datarate(Model* pModel);
