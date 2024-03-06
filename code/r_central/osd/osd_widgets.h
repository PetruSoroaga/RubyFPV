#pragma once

#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/hardware.h"
#include "../../base/models.h"

#define MAX_OSD_WIDGETS 30
#define MAX_OSD_WIDGET_NAME 24
#define MAX_OSD_WIDGET_PARAMS 10
#define OSD_WIDGET_ID_BUILTIN_ALTITUDE 1

typedef struct
{
   u32 uVehicleId;
   float fXPos; // 0...1
   float fYPos; // 0...1
   float fWidth; // 0...1
   float fHeight; // 0..1

   bool bShow;
   int  iParams[MAX_OSD_WIDGET_PARAMS];
   int iParamsCount;
} type_osd_widget_display_info;

typedef struct
{
   u32 uGUID;
   int iVersion;
   char szName[MAX_OSD_WIDGET_NAME];

} __attribute__((packed)) type_osd_widget_info;

typedef struct
{
   type_osd_widget_info info;
   type_osd_widget_display_info display_info[MAX_MODELS][MODEL_MAX_OSD_PROFILES];
} type_osd_widget;

int osd_widgets_get_count();
type_osd_widget* osd_widgets_get(int iIndex);

bool osd_widgets_load();
bool osd_widgets_save();

// Returns the position of the model in the widget models list
int osd_widget_add_to_model(type_osd_widget* pWidget, u32 uVehicleId);
// Returns the position of the model in the widget models list
int osd_widget_get_model_index(type_osd_widget* pWidget, u32 uVehicleId);

void osd_widgets_on_new_vehicle_added_to_controller(u32 uVehicleId);

void osd_widgets_on_main_vehicle_changed(u32 uCurrentVehicleId);
void osd_widgets_render(u32 uCurrentVehicleId, int iOSDScreen);
