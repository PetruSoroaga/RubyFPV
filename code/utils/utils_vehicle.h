#pragma once

#include "../base/base.h"
#include "../base/shared_mem.h"
#include "../base/models.h"

u32 vehicle_utils_getControllerId();

bool videoLinkProfileIsOnlyVideoKeyframeChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile);
bool videoLinkProfileIsOnlyBitrateChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile);
bool videoLinkProfileIsOnlyIPQuantizationDeltaChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile);
bool videoLinkProfileIsOnlyECSchemeChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile);
bool videoLinkProfileIsOnlyAdaptiveVideoChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile);

// To fix
//void video_overwrites_init(shared_mem_video_link_overwrites* pSMVLO, Model* pModel);
bool configure_radio_interfaces_for_current_model(Model* pModel, shared_mem_process_stats* pProcessStats);

void apply_vehicle_tx_power_levels(Model* pModel);
