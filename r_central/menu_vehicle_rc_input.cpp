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
#include "../base/config.h"
#include "../base/launchers.h"
#include "../base/commands.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"
#include "../base/hardware_i2c.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "menu.h"
#include "menu_vehicle_rc_input.h"
#include "menu_confirmation.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"

MenuVehicleRCInput::MenuVehicleRCInput(void)
:Menu(MENU_ID_VEHICLE_RC_INPUT, "RC Input Source", NULL)
{
   m_Width = 0.32;
   m_Height = 0.0;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.26;
   m_ExtraHeight = 0;//2*toScreenX(MENU_MARGINS)*menu_getScaleMenus();

   char szLegend[256];
   
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   m_pItemsRadio[0] = new MenuItemRadio("", "");
   m_pItemsRadio[0]->addSelection("None");

   strcpy(szLegend, "Use this option when you connect a USB joystick, a USB gamepad or a USB RC transmitter to your controller");
   if ( 0 == pCI->inputInterfacesCount )
      strcat(szLegend, ".\nNo USB HID devices detected on the controller");
   m_pItemsRadio[0]->addSelection("USB HID device", szLegend);

   strcpy(szLegend, "Use this option when you connect a SBUS/IBUS input adapter to the I2C bus on the controller");
   if ( (! hardware_has_i2c_device_id(I2C_DEVICE_ADDRESS_PICO_RC_IN)) &&
        (! hardware_has_i2c_device_id(I2C_DEVICE_ADDRESS_PICO_EXTENDER)) &&
        (! hardware_i2c_has_external_extenders_rcin()) )
      strcat(szLegend, ".\nNo RC In device detected on the I2C busses");
   m_pItemsRadio[0]->addSelection("RC In (SBUS/IBUS)", szLegend);

   m_pItemsRadio[0]->setEnabled(true);
   m_IndexInputType = addMenuItem(m_pItemsRadio[0]);
}

MenuVehicleRCInput::~MenuVehicleRCInput()
{
}

void MenuVehicleRCInput::onShow()
{      
   Menu::onShow();
}

void MenuVehicleRCInput::valuesToUI()
{
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   m_pItemsRadio[0]->setSelectedIndex(g_pCurrentModel->rc_params.inputType);
   m_pItemsRadio[0]->setFocusedIndex(g_pCurrentModel->rc_params.inputType);
   m_pItemsRadio[0]->enableSelectionIndex(0, true);

   if ( 0 == pCI->inputInterfacesCount )
      m_pItemsRadio[0]->enableSelectionIndex(1, false);
   else
      m_pItemsRadio[0]->enableSelectionIndex(1, true);

   if ( hardware_has_i2c_device_id(I2C_DEVICE_ADDRESS_PICO_RC_IN) ||
        hardware_has_i2c_device_id(I2C_DEVICE_ADDRESS_PICO_EXTENDER) ||
        hardware_i2c_has_external_extenders_rcin() )
      m_pItemsRadio[0]->enableSelectionIndex(2, true);
   else
      m_pItemsRadio[0]->enableSelectionIndex(2, false);
}


void MenuVehicleRCInput::Render()
{
   RenderPrepare();   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleRCInput::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( m_IndexInputType == m_SelectedIndex )
   {
      log_line("Selected option %d", m_pItemsRadio[0]->getFocusedIndex());
      m_pItemsRadio[0]->setSelectedIndex(m_pItemsRadio[0]->getFocusedIndex());

      rc_parameters_t params;
      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));
      int index = m_pItemsRadio[0]->getSelectedIndex();
      if ( 0 == index )
         params.inputType = RC_INPUT_TYPE_NONE;
      if ( 1 == index )
         params.inputType = RC_INPUT_TYPE_USB;
      if ( 2 == index )
         params.inputType = RC_INPUT_TYPE_RC_IN_SBUS_IBUS;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
         valuesToUI();

      menu_stack_pop(1);
   }
}
