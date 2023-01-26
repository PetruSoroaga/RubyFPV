#pragma once

#include "../base/base.h"
#include "../base/shared_mem.h"
#include "../base/models.h"


bool videoLinkProfileIsOnlyVideoKeyframeChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile);
bool videoLinkProfileIsOnlyBitrateChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile);
bool videoLinkProfileIsOnlyECSchemeChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile);
bool videoLinkProfileIsOnlyAdaptiveVideoChanged(type_video_link_profile* pOldProfile, type_video_link_profile* pNewProfile);

void video_overwrites_init(shared_mem_video_link_overwrites* pSMVLO, Model* pModel);
void send_control_message_to_raspivid(u8 parameter, u8 value);
