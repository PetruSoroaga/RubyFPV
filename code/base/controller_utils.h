#pragma once

#include "../base/base.h"
#include "../base/models.h"

u32 controller_utils_getControllerId();

int controller_utils_export_all_to_usb();
int controller_utils_import_all_from_usb(bool bImportAnyFound);
bool controller_utils_usb_import_has_matching_controller_id_file();
bool controller_utils_usb_import_has_any_controller_id_file();

// Returns number of asignable radio interfaces or a negative error code number
int controller_count_asignable_radio_interfaces_to_vehicle_radio_link(Model* pModel, int iVehicleRadioLinkId);
