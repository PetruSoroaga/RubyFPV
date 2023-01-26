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
#include "menu.h"
#include "menu_objects.h"
#include "menu_controller_network.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"

#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/launchers.h"
#include "../base/hardware.h"

#include "pairing.h"
#include "colors.h"

#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "process_router_messages.h"
#include "ruby_central.h"
#include "timers.h"

#include <time.h>
#include <sys/resource.h>

MenuControllerNetwork::MenuControllerNetwork(void)
:Menu(MENU_ID_CONTROLLER_NETWORK, "Controller Local Network Settings", NULL)
{
   m_Width = 0.29;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;
   
   Preferences* pp = get_Preferences();
   ControllerSettings* pCS = get_ControllerSettings();
   char szBuff[128];
   char szOutput[1024];
   hw_execute_bash_command_raw("hostname -I", szOutput);
   sprintf(szBuff, "Current IP: %s", szOutput);
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
   sprintf(szBuff, "%d.%d.%d.", pCS->uFixedIP >> 24, (pCS->uFixedIP >> 16 ) & 0xFF, (pCS->uFixedIP >> 8 ) & 0xFF );
   m_pItemsRange[0]->setPrefix(szBuff);
   m_IndexIP = addMenuItem(m_pItemsRange[0]);

   addMenuItem(new MenuItemText("Note: Changing network settings requires a restart."));
}

void MenuControllerNetwork::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pp = get_Preferences();
   
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

void MenuControllerNetwork::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   m_iConfirmationId = 0;
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
}
