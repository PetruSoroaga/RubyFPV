/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "../base/base.h"
#include "../base/models.h"
#include "../base/hw_procs.h"
#include "../base/config.h"
#include "../common/string_utils.h"
#include "shared_vars.h"
#include "menu.h"
#include "menu_switch_vehicle.h"
#include "pairing.h"
#include "ruby_central.h"
#include "events.h"
#include "popup.h"


MenuSwitchVehicle::MenuSwitchVehicle(int vehicleIndex)
:Menu(MENU_ID_SWITCH_VEHICLE, "Switch Vehicle", NULL)
{
   m_Width = 0.4;
   m_xPos = 0.53-m_Width/2; m_yPos = 0.54;

   m_VehicleIndex = vehicleIndex;

   char szBuff[256];
   char szName[128];

   szBuff[0] = 0;
   szName[0] = 0;
   //snprintf(szName, sizeof(szName),"%s ", Model::getVehicleType(g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_type));
   strcpy(szName, "Unknown");
   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotRubyTelemetryInfo )
   {
      if ( 0 == g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_name[0] )
         strcpy(szName, "No Name");
      else
         strcpy(szName, (char*)g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerRubyTelemetryExtended.vehicle_name);
   }
   if ( NULL != g_pCurrentModel )
   {
      snprintf(szBuff, sizeof(szBuff), "Warning: There is a different vehicle (%s) on the same frequency as your current vehicle (%s)!", szName, g_pCurrentModel->getShortName());
      if ( 1 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
         snprintf(szBuff, sizeof(szBuff), "Warning: There is a different vehicle (%s) on the same frequency (%s) as your current vehicle (%s)!", szName, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[0]), g_pCurrentModel->getShortName());
      else if ( 2 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
      {
         char szFreq1[64];
         char szFreq2[64];
         strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[0]));
         strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[1]));
         snprintf(szBuff, sizeof(szBuff), "Warning: There is a different vehicle (%s) on the same frequencies (%s/%s) as your current vehicle (%s)!", szName, szFreq1, szFreq2, g_pCurrentModel->getShortName());
      }
      else
      {
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];
         strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[0]));
         strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[1]));
         strcpy(szFreq3, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[2]));
         snprintf(szBuff, sizeof(szBuff), "Warning: There is a different vehicle (%s) on the same frequencies (%s/%s/%s) as your current vehicle (%s)!", szName, szFreq1, szFreq2, szFreq3, g_pCurrentModel->getShortName());
      }
   }
   if ( 0 != szBuff[0] )
   {
      addTopLine(szBuff);
      addTopLine("Do you want to switch to this vehicle?");
   }
   addMenuItem(new MenuItem("Yes"));
   addMenuItem(new MenuItem("No"));
}


void MenuSwitchVehicle::onShow()
{
   Menu::onShow();
}

void MenuSwitchVehicle::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuSwitchVehicle::onSelectItem()
{
   if ( 1 == m_SelectedIndex )
   {
      menu_close_all();
      return;
   }
   if ( 0 == m_SelectedIndex )
   {
      Model *pModel = getModelAtIndex(m_VehicleIndex);
      if ( NULL == pModel )
      {
         log_softerror_and_alarm("NULL model for vehicle: %s.", m_pMenuItems[m_SelectedIndex]->getTitle());
         menu_close_all();
         return;
      }
      if ( NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) && g_pCurrentModel->vehicle_id == pModel->vehicle_id )
      {
         menu_close_all();
         return;
      }

      menu_close_all();
      render_all(get_current_timestamp_ms(), true);
      Popup* p = new Popup("Switching vehicles...",0.3,0.64, 0.26, 0.2);
      popups_add_topmost(p);
      render_all(get_current_timestamp_ms(), true);
         
      pairing_stop();
      pModel->is_spectator = false;
      g_pCurrentModel = pModel;
      g_pCurrentModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, true);

      g_bIsFirstConnectionToCurrentVehicle = true;
      onMainVehicleChanged();
      pairing_start(); 
   }
}

