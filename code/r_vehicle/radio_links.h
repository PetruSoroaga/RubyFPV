#pragma once
#include "../base/base.h"
#include "../base/models.h"
#include "../radio/radiopackets2.h"

bool radio_links_apply_settings(Model* pModel, int iRadioLink, type_radio_links_parameters* pRadioLinkParamsOld, type_radio_links_parameters* pRadioLinkParams);
