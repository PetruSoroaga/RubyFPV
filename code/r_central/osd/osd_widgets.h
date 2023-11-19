#pragma once

#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/hardware.h"
#include "../../base/models.h"

#define MAX_OSD_WIDGETS 50
#define MAX_OSD_WIDGET_NAME 32

typedef struct
{
   u32 uId;
   char szName[MAX_OSD_WIDGET_NAME];
   float fXPosPercent;
   float fYPosPercent;
   float fWidth;
   float fHeight;
   bool bHorizontal;

   u32 uShowInModels[MAX_MODELS];
   bool bShowInOSDScreen[MAX_MODELS][MODEL_MAX_OSD_PROFILES];
} __attribute__((packed)) osd_widget_t;


int osd_widgets_get_count();
osd_widget_t* osd_widgets_get(int iIndex);

bool osd_widgets_load();
bool osd_widgets_save();

void osd_widgets_render(u32 uCurrentVehicleId);
