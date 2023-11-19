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

#include "menu.h"
#include "menu_switch_vehicle.h"


MenuSwitchVehicle::MenuSwitchVehicle(u32 uVehicleId)
:Menu(MENU_ID_SWITCH_VEHICLE, "Switch Vehicle", NULL)
{
   m_Width = 0.4;
   m_xPos = 0.53-m_Width/2; m_yPos = 0.44;

   m_uVehicleId = uVehicleId;

   char szBuff[256];
   char szName[128];

   szBuff[0] = 0;
   szName[0] = 0;
   strcpy(szName, "Unknown");
   Model* pModelTmp = findModelWithId(m_uVehicleId);
   if ( NULL != pModelTmp )
   strcpy(szName, pModelTmp->getLongName());

   if ( NULL != g_pCurrentModel )
   {
      sprintf(szBuff, "Warning: There is a different vehicle (%s) on the same frequency as your current vehicle (%s)!", szName, g_pCurrentModel->getLongName());
      if ( 1 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
         sprintf(szBuff, "Warning: There is a different vehicle (%s) on the same frequency (%s) as your current vehicle (%s)!", szName, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[0]), g_pCurrentModel->getLongName());
      else if ( 2 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
      {
         char szFreq1[64];
         char szFreq2[64];
         strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[1]));
         sprintf(szBuff, "Warning: There is a different vehicle (%s) on the same frequencies (%s/%s) as your current vehicle (%s)!", szName, szFreq1, szFreq2, g_pCurrentModel->getLongName());
      }
      else
      {
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];
         strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[1]));
         strcpy(szFreq3, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[2]));
         sprintf(szBuff, "Warning: There is a different vehicle (%s) on the same frequencies (%s/%s/%s) as your current vehicle (%s)!", szName, szFreq1, szFreq2, szFreq3, g_pCurrentModel->getLongName());
      }
   }
   if ( 0 != szBuff[0] )
   {
      addTopLine(szBuff);
      addTopLine("Switch to this vehicle or power it off as it will impact your current vehicle radio link quality.");
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
      Model *pModel = findModelWithId(m_uVehicleId);

      if ( NULL == pModel )
      {
         log_softerror_and_alarm("NULL model for vehicle to switch to.");
         menu_close_all();
         return;
      }
      if ( NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) && (g_pCurrentModel->vehicle_id == pModel->vehicle_id) )
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
      setControllerCurrentModel(g_pCurrentModel->vehicle_id);
      saveControllerModel(g_pCurrentModel);

      ruby_set_active_model_id(g_pCurrentModel->vehicle_id);

      g_bIsFirstConnectionToCurrentVehicle = true;
      onMainVehicleChanged();
      pairing_start_normal(); 
   }
}

