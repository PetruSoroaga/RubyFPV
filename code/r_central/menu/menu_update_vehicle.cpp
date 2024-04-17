/*
    Ruby Licence
    Copyright (c) 2024 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permited.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL Julien Verneuil BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "menu.h"
#include "menu_update_vehicle.h"
#include "menu_confirmation.h"


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
      menu_discard_all();
      return;
   }
   if ( 0 == m_SelectedIndex )
   {
      if ( uploadSoftware() )
      {
         //Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE+3*1000,"Upload Succeeded",NULL);
         //pm->addTopLine("Your vehicle was updated. It will reboot now.");
         Menu* pm = new MenuConfirmation("Upload Succeeded", "Your vehicle was updated. It will reboot now.", MENU_ID_SIMPLE_MESSAGE+3*1000, true);
         pm->m_xPos = 0.4; pm->m_yPos = 0.4;
         pm->m_Width = 0.36;
         pm->m_bDisableStacking = true;
         replace_menu_on_stack(this, pm);
      }
   }
}

