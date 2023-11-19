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


void MenuVehicleRadioInterface::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
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