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
        Copyright info and developer info must be preserved as is in the user
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
#include "menu_vehicle_radio_interface.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_txinfo.h"
#include "menu_confirmation.h"
#include "../launchers_controller.h"
#include "menu_txinfo.h"
#include "../link_watch.h"


const char* s_szMenuRadio_SingleCard3 = "Note: You can not change the function and capabilities of the radio interface as there is a single one present on the vehicle.";

MenuVehicleRadioInterface::MenuVehicleRadioInterface(int iRadioInterface)
:Menu(MENU_ID_VEHICLE_RADIO_INTERFACE, "Vehicle Radio Interface Parameters", NULL)
{
   m_Width = 0.3;
   m_xPos = 0.08;
   m_yPos = 0.1;
   m_iRadioInterface = iRadioInterface;

   log_line("Opening menu for radio interface %d parameters...", m_iRadioInterface);

   char szBuff[256];

   sprintf(szBuff, "Vehicle Radio Interface %d Parameters", m_iRadioInterface+1);
   setTitle(szBuff);
   addTopLine("You have a single radio interface on the vehicle side for this radio link. You can't change the radio interface parameters. They are derived automatically from the radio link params.");

   addMenuItem(new MenuItem("Ok"));
}

MenuVehicleRadioInterface::~MenuVehicleRadioInterface()
{
}

void MenuVehicleRadioInterface::onShow()
{
   valuesToUI();
   Menu::onShow();

   invalidate();
}

void MenuVehicleRadioInterface::valuesToUI()
{
}

void MenuVehicleRadioInterface::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleRadioInterface::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
}

void MenuVehicleRadioInterface::sendInterfaceCapabilitiesFlags(int iInterfaceIndex)
{
   
}

void MenuVehicleRadioInterface::sendInterfaceRadioFlags(int iInterfaceIndex)
{
   
}

void MenuVehicleRadioInterface::onSelectItem()
{
   Menu::onSelectItem();
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( 0 == m_SelectedIndex )
   {
      menu_stack_pop(0);
      return;
   }

   if ( link_is_reconfiguring_radiolink() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   
}