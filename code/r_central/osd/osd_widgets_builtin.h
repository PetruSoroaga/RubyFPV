#pragma once

#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/hardware.h"
#include "../../base/models.h"

#include "osd_widgets.h"

void osd_widget_builtin_altitude_on_new_vehicle(type_osd_widget* pWidgetInfo, u32 uCurrentVehicleId);
void osd_widget_builtin_altitude_on_main_vehicle_changed(type_osd_widget* pWidgetInfo, u32 uCurrentVehicleId);
void osd_widget_builtin_altitude_render(type_osd_widget* pWidgetInfo, type_osd_widget_display_info* pDisplayInfo, u32 uCurrentVehicleId, int iOSDScreen);
