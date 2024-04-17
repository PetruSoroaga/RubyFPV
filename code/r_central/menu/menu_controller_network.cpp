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
#include "menu_objects.h"
#include "menu_controller_network.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"

#include "../process_router_messages.h"

#include <time.h>
#include <sys/resource.h>

MenuControllerNetwork::MenuControllerNetwork(void)
:Menu(MENU_ID_CONTROLLER_NETWORK, "Controller Local Network Settings", NULL)
{
   m_Width = 0.29;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;
   
   ControllerSettings* pCS = get_ControllerSettings();
   char szBuff[128];
   char szOutput[1024];
   hw_execute_bash_command_raw("hostname -I", szOutput);
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Current IP: %s", szOutput);
   addMenuItem(new MenuItemText(szBuff));

   m_pItemsSelect[0] = new MenuItemSelect("Enable ETH and DHCP", "Enables the wired network connection.");
   m_pItemsSelect[0]->addSelection("No");
   m_pItemsSelect[0]->addSelection("Yes");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexDHCP = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Use a fixed IP", "Use a fixed IP on the controller, for point to point ETH connections.");
   m_pItemsSelect[1]->addSelection("No");
   m_pItemsSelect[1]->addSelection("Yes");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexFixedIP = addMenuItem(m_pItemsSelect[1]);

   m_pItemsRange[0] = new MenuItemRange("IP", "Sets the fixed IP to assign to the controller.", 0, 255, 20, 1 );
   snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "%d.%d.%d.", pCS->uFixedIP >> 24, (pCS->uFixedIP >> 16 ) & 0xFF, (pCS->uFixedIP >> 8 ) & 0xFF );
   m_pItemsRange[0]->setPrefix(szBuff);
   m_IndexIP = addMenuItem(m_pItemsRange[0]);
   addMenuItem(new MenuItemText("Note: Changing network settings requires a restart."));

   m_IndexSSH = addMenuItem( new MenuItem("Enable SSH", "Enables SSH login to this controller"));
}

void MenuControllerNetwork::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   
   if( access( "/boot/nodhcp", R_OK ) == -1 )
      m_pItemsSelect[0]->setSelection(1);
   else
      m_pItemsSelect[0]->setSelection(0);

   m_pItemsSelect[1]->setSelectedIndex(pCS->nUseFixedIP);

   m_pItemsRange[0]->setCurrentValue(pCS->uFixedIP & 0xFF);
   if ( pCS->nUseFixedIP )
      m_pItemsRange[0]->setEnabled(true);
   else
      m_pItemsRange[0]->setEnabled(false);
}

void MenuControllerNetwork::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuControllerNetwork::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   Preferences* pp = get_Preferences();
   if ( NULL == pp )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }


   if ( m_IndexDHCP == m_SelectedIndex )
   {
      hardware_mount_boot();
      hardware_sleep_ms(50);
      if ( 1 == m_pItemsSelect[0]->getSelectedIndex() )
      {
         log_line("Enabling DHCP");
         hw_execute_bash_command("rm -f /boot/nodhcp", NULL);
         pCS->nUseFixedIP = 0;
         save_ControllerSettings();
      }
      else
      {
         log_line("Disabling DHCP");
         hw_execute_bash_command("echo '1' > /boot/nodhcp", NULL);
      }
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);       
      return;
   }  

   if ( m_IndexFixedIP == m_SelectedIndex )
   {
      pCS->nUseFixedIP = m_pItemsSelect[1]->getSelectedIndex();
      save_ControllerSettings();
      if ( 1 == pCS->nUseFixedIP )
         hw_execute_bash_command("echo '1' > /boot/nodhcp", NULL);
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);       
   }

   if ( m_IndexIP == m_SelectedIndex )
   {
      m_pItemsRange[0]->invalidate();
      pCS->uFixedIP &= ~0xFF;
      pCS->uFixedIP |= ((int)m_pItemsRange[0]->getCurrentValue()) & 0xFF;
      save_ControllerSettings();
      valuesToUI();
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, PACKET_COMPONENT_LOCAL_CONTROL);       
   }

   if ( m_IndexSSH == m_SelectedIndex )
   {
      hw_execute_bash_command("touch /boot/ssh", NULL);
      addMessage("SSH enabled. Controller will reboot now.");
      for( int i=0; i<10; i++ )
         hardware_sleep_ms(400);
      hw_execute_bash_command("sudo reboot -f", NULL);
   }
}
