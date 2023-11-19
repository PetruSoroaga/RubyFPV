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
#include "menu_update_vehicle.h"


MenuUpdateVehiclePopup::MenuUpdateVehiclePopup(int vehicleIndex)
:Menu(MENU_ID_UPDATE_VEHICLE_POPUP, "Update Vehicle Software", NULL)
{
   m_Width = 0.4;
   m_xPos = 0.53-m_Width/2; m_yPos = 0.34;

   m_VehicleIndex = vehicleIndex;

   char szBuff[256];
   char szBuff2[32];
   char szBuff3[32];
   getSystemVersionString(szBuff2, g_pCurrentModel->sw_version);
   getSystemVersionString(szBuff3, (SYSTEM_SW_VERSION_MAJOR<<8) | SYSTEM_SW_VERSION_MINOR);
   sprintf(szBuff, "Your vehicle %s has Ruby version %s (b.%d) and your controller has Ruby version %s (b.%d). You should update your vehicle.", g_pCurrentModel->getLongName(), szBuff2, g_pCurrentModel->sw_version >> 16, szBuff3, SYSTEM_SW_BUILD_NUMBER);

   addTopLine(szBuff);
   addTopLine("Do you want to update the vehicle?");
   addMenuItem(new MenuItem("Yes"));
   addMenuItem(new MenuItem("No"));
}


void MenuUpdateVehiclePopup::onShow()
{
   Menu::onShow();
}

void MenuUpdateVehiclePopup::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuUpdateVehiclePopup::onSelectItem()
{
   if ( 1 == m_SelectedIndex )
   {
      menu_close_all();
      return;
   }
   if ( 0 == m_SelectedIndex )
   {
      if ( uploadSoftware() )
      {
         m_iConfirmationId = 3;
         Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Upload Succeeded",NULL);
         pm->m_xPos = 0.4; pm->m_yPos = 0.4;
         pm->m_Width = 0.36;
         pm->addTopLine("Your vehicle was updated. It will reboot now.");
         replace_menu_on_stack(this, pm);
      }
   }
}

