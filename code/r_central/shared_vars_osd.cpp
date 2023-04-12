#include "../base/base.h"
#include "shared_vars.h"


int g_iComputedOSDCellCount = 1;
float g_fOSDDbm[MAX_RADIO_INTERFACES];
int g_iCurrentOSDVehicleLayout = 0;
int g_iCurrentOSDVehicleRuntimeInfoIndex = 0;


void shared_vars_osd_reset_before_pairing()
{
   g_iCurrentOSDVehicleLayout = 0;
   g_iCurrentOSDVehicleRuntimeInfoIndex = 0;
   
   g_iComputedOSDCellCount = 1;
   if ( NULL != g_pCurrentModel && g_pCurrentModel->osd_params.battery_cell_count > 0 )
      g_iComputedOSDCellCount = g_pCurrentModel->osd_params.battery_cell_count;
}