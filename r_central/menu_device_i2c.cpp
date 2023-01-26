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
#include "menu_device_i2c.h"
#include "menu_text.h"
#include "menu_txinfo.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"
#include "menu_item_range.h"

#include "../base/config.h"
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../base/launchers.h"
#include "../base/hardware.h"
#include "../base/hardware_i2c.h"

#include "pairing.h"

#include "shared_vars.h"
#include "popup.h"
#include "handle_commands.h"
#include "process_router_messages.h"
#include "ruby_central.h"

MenuDeviceI2C::MenuDeviceI2C(void)
:Menu(MENU_ID_DEVICE_I2C, "Configure I2C Device", NULL)
{
   m_Width = 0.34;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.25;
   m_nDeviceId = 0;
}

MenuDeviceI2C::~MenuDeviceI2C(void)
{
}

void MenuDeviceI2C::setDeviceId(int nDeviceId)
{
   m_nDeviceId = nDeviceId;
}

void MenuDeviceI2C::onShow()
{
    createItems();
    Menu::onShow();
}


void MenuDeviceI2C::createItems()
{
   removeAllItems();
   m_ExtraHeight = 0;

   m_IndexCustomSettings1 = -1;
   m_IndexCustomSettings2 = -1;
   m_IndexCustomSettings3 = -1;
   m_IndexCustomSettings4 = -1;
   m_bDeviceHasCustomSettings = false;
   m_pItemsSelect[2] = NULL;
   m_pItemsSelect[3] = NULL;
   m_pItemsSelect[4] = NULL;
   m_pItemsSelect[5] = NULL;

   m_pItemsSelect[0] = new MenuItemSelect("Device Type", "Sets what type of device this is.");
   m_pItemsSelect[0]->addSelection("Unknown");
   m_pItemsSelect[0]->addSelection(I2C_DEVICE_NAME_INA219);
   m_pItemsSelect[0]->addSelection(I2C_DEVICE_NAME_RC_IN);
   m_pItemsSelect[0]->addSelection(I2C_DEVICE_NAME_PICO_EXTENDER);
   m_pItemsSelect[0]->addSelection(I2C_DEVICE_NAME_RUBY_ADDON);
   m_pItemsSelect[0]->setIsEditable();
   m_IndexDevType = addMenuItem(m_pItemsSelect[0]);
   m_pItemsSelect[0]->setEnabled(false);

   m_pItemsSelect[1] = new MenuItemSelect("Enabled", "Enables or disables this device.");
   m_pItemsSelect[1]->addSelection("No");
   m_pItemsSelect[1]->addSelection("Yes");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexEnable = addMenuItem(m_pItemsSelect[1]);

   t_i2c_device_settings* pInfo = hardware_i2c_get_device_settings((u8)m_nDeviceId);
   if ( NULL == pInfo || pInfo->bConfigurable == false )
   {
      addMenuItem(new MenuItemText("This device is not configurable."));
      return;
   }

   addMenuItem( new MenuItemSection("Device Specific Settings"));
   if ( NULL != pInfo && pInfo->nDeviceType == I2C_DEVICE_TYPE_INA219 )
   {
      m_pItemsSelect[2] = new MenuItemSelect("Reads", "What information should the sensor read from the hardware. That information can then be displayed on the OSD.");
      m_pItemsSelect[2]->addSelection("Voltage");
      m_pItemsSelect[2]->addSelection("Current");
      m_pItemsSelect[2]->addSelection("Voltage & Current");
      m_pItemsSelect[2]->setIsEditable();
      m_IndexCustomSettings1 = addMenuItem(m_pItemsSelect[2]);
      m_bDeviceHasCustomSettings = true;
   }

   if ( NULL != pInfo && pInfo->nDeviceType == I2C_DEVICE_TYPE_PICO_RC_IN )
   {
      m_pItemsSelect[2] = new MenuItemSelect("RC Input Protocol", "What kind of protocol is provided on the input.");
      m_pItemsSelect[2]->addSelection("SBUS");
      m_pItemsSelect[2]->addSelection("IBUS");
      m_pItemsSelect[2]->setIsEditable();
      m_IndexCustomSettings1 = addMenuItem(m_pItemsSelect[2]);

      m_pItemsSelect[3] = new MenuItemSelect("RC Input Protocol Inverted", "The signal is inverted on the input. Used mostly for SBUS protocol and should be set to Yes.");
      m_pItemsSelect[3]->addSelection("No");
      m_pItemsSelect[3]->addSelection("Yes");
      m_pItemsSelect[3]->setIsEditable();
      m_IndexCustomSettings2 = addMenuItem(m_pItemsSelect[3]);

      m_bDeviceHasCustomSettings = true;
   }

   if ( NULL != pInfo && pInfo->nDeviceType == I2C_DEVICE_TYPE_PICO_EXTENDER )
   {
      m_pItemsSelect[2] = new MenuItemSelect("RC Input Protocol", "What kind of protocol is provided on the input.");
      m_pItemsSelect[2]->addSelection("SBUS");
      m_pItemsSelect[2]->addSelection("IBUS");
      m_pItemsSelect[2]->setIsEditable();
      m_IndexCustomSettings1 = addMenuItem(m_pItemsSelect[2]);

      m_pItemsSelect[3] = new MenuItemSelect("RC Input Protocol Inverted", "The signal is inverted on the input. Used mostly for SBUS protocol and should be set to Yes.");
      m_pItemsSelect[3]->addSelection("No");
      m_pItemsSelect[3]->addSelection("Yes");
      m_pItemsSelect[3]->setIsEditable();
      m_IndexCustomSettings2 = addMenuItem(m_pItemsSelect[3]);

      m_pItemsSelect[4] = new MenuItemSelect("Rotary Encoder Function", "Sets the function of the optional rotary encoder. Long press the rotary encoder for 10 seconds to revert to Menu function.");  
      m_pItemsSelect[4]->addSelection("None");
      m_pItemsSelect[4]->addSelection("Menu Navigation");
      m_pItemsSelect[4]->addSelection("Camera Adjustment");
      m_pItemsSelect[4]->setIsEditable();
      m_IndexCustomSettings3 = addMenuItem(m_pItemsSelect[4]);

      m_pItemsSelect[5] = new MenuItemSelect("Rotary Encoder Speed", "Sets the speed of the optional rotary encoder.");  
      m_pItemsSelect[5]->addSelection("Normal");
      m_pItemsSelect[5]->addSelection("Slower");
      m_pItemsSelect[5]->setIsEditable();
      m_IndexCustomSettings4 = addMenuItem(m_pItemsSelect[5]);

      m_bDeviceHasCustomSettings = true;
   }

   if ( NULL != pInfo && pInfo->nDeviceType == I2C_DEVICE_TYPE_RUBY_ADDON )
   {
      if ( pInfo->uCapabilitiesFlags & I2C_CAPABILITY_FLAG_RC_INPUT )
      {
         m_pItemsSelect[2] = new MenuItemSelect("RC Input Protocol", "What kind of protocol is provided on the input.");
         m_pItemsSelect[2]->addSelection("None");
         m_pItemsSelect[2]->addSelection("SBUS");
         m_pItemsSelect[2]->addSelection("IBUS");
         m_pItemsSelect[2]->setIsEditable();
         m_IndexCustomSettings1 = addMenuItem(m_pItemsSelect[2]);

         m_pItemsSelect[3] = new MenuItemSelect("RC Input Protocol Inverted", "The signal is inverted on the input. Used mostly for SBUS protocol and should be set to Yes.");
         m_pItemsSelect[3]->addSelection("No");
         m_pItemsSelect[3]->addSelection("Yes");
         m_pItemsSelect[3]->setIsEditable();
         m_IndexCustomSettings2 = addMenuItem(m_pItemsSelect[3]);

         m_bDeviceHasCustomSettings = true;
      }

      if ( pInfo->uCapabilitiesFlags & I2C_CAPABILITY_FLAG_ROTARY )
      {
         m_pItemsSelect[4] = new MenuItemSelect("Rotary Encoder Function", "Sets the function of the optional rotary encoder. Long press the rotary encoder for 10 seconds to revert to Menu function.");  
         m_pItemsSelect[4]->addSelection("None");
         m_pItemsSelect[4]->addSelection("Menu Navigation");
         m_pItemsSelect[4]->addSelection("Camera Adjustment");
         m_pItemsSelect[4]->setIsEditable();
         m_IndexCustomSettings3 = addMenuItem(m_pItemsSelect[4]);
      
         m_pItemsSelect[5] = new MenuItemSelect("Rotary Encoder Speed", "Sets the speed of the optional rotary encoder.");  
         m_pItemsSelect[5]->addSelection("Normal");
         m_pItemsSelect[5]->addSelection("Slower");
         m_pItemsSelect[5]->setIsEditable();
         m_IndexCustomSettings4 = addMenuItem(m_pItemsSelect[5]);

         m_bDeviceHasCustomSettings = true;
      }
   }

   if ( ! m_bDeviceHasCustomSettings )
      addMenuItem( new MenuItemText("No device specific settings.") );
}

void MenuDeviceI2C::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   t_i2c_device_settings* pInfo = hardware_i2c_get_device_settings((u8)m_nDeviceId);

   if ( NULL == pInfo || pInfo->bConfigurable == false )
   {
      m_pMenuItems[m_IndexDevType]->setEnabled(false);
      m_pMenuItems[m_IndexEnable]->setEnabled(false);
      if ( NULL != m_pItemsSelect[2] )
         m_pItemsSelect[2]->setEnabled(false);
      if ( NULL != m_pItemsSelect[3] )
         m_pItemsSelect[3]->setEnabled(false);
      if ( NULL != m_pItemsSelect[4] )
         m_pItemsSelect[4]->setEnabled(false);
      if ( NULL != m_pItemsSelect[5] )
         m_pItemsSelect[5]->setEnabled(false);
   }

   m_pItemsSelect[0]->setSelection(0);
   m_pItemsSelect[0]->setEnabled(false);
   m_pItemsSelect[0]->setSelectedIndex(0);

   if ( NULL == pInfo )
      return;

   if ( pInfo->nDeviceType == I2C_DEVICE_TYPE_INA219 )
      m_pItemsSelect[0]->setSelection(1);
   if ( pInfo->nDeviceType == I2C_DEVICE_TYPE_PICO_RC_IN )
      m_pItemsSelect[0]->setSelection(2);
   if ( pInfo->nDeviceType == I2C_DEVICE_TYPE_PICO_EXTENDER )
      m_pItemsSelect[0]->setSelection(3);
   if ( pInfo->nDeviceType == I2C_DEVICE_TYPE_RUBY_ADDON )
      m_pItemsSelect[0]->setSelection(4);

   m_pItemsSelect[1]->setSelectedIndex(pInfo->bEnabled?1:0);

   if ( ! m_bDeviceHasCustomSettings )
      return;

   if ( pInfo->nDeviceType == I2C_DEVICE_TYPE_INA219 )
   {
      m_pItemsSelect[2]->setEnabled(true);
      m_pItemsSelect[2]->setSelection( pInfo->uParams[0] );
   }

   if ( (pInfo->nDeviceType == I2C_DEVICE_TYPE_PICO_RC_IN) || (pInfo->nDeviceType == I2C_DEVICE_TYPE_PICO_EXTENDER) ||
        ((pInfo->nDeviceType == I2C_DEVICE_TYPE_RUBY_ADDON) && (pInfo->uCapabilitiesFlags & I2C_CAPABILITY_FLAG_RC_INPUT)) )
   {
      m_pItemsSelect[2]->setEnabled(true);
      m_pItemsSelect[3]->setEnabled(true);
      m_pItemsSelect[2]->setSelection( pInfo->uParams[0] );
      m_pItemsSelect[3]->setSelection( pInfo->uParams[1] );
      if ( 0 == pInfo->uParams[0] )
        m_pItemsSelect[3]->setEnabled(false);
      else
        m_pItemsSelect[3]->setEnabled(true);
   }

   if ( (pInfo->nDeviceType == I2C_DEVICE_TYPE_PICO_EXTENDER) ||
        ((pInfo->nDeviceType == I2C_DEVICE_TYPE_RUBY_ADDON) && (pInfo->uCapabilitiesFlags & I2C_CAPABILITY_FLAG_ROTARY)) )
   {
      m_pItemsSelect[4]->setEnabled(true);
      m_pItemsSelect[4]->setSelection( pCS->nRotaryEncoderFunction );
      m_pItemsSelect[5]->setEnabled(true);
      m_pItemsSelect[5]->setSelection( pCS->nRotaryEncoderSpeed );
   }

   if ( ! pInfo->bEnabled )
   {
      if ( NULL != m_pItemsSelect[2] )
         m_pItemsSelect[2]->setEnabled(false);
      if ( NULL != m_pItemsSelect[3] )
         m_pItemsSelect[3]->setEnabled(false);
      if ( NULL != m_pItemsSelect[4] )
         m_pItemsSelect[4]->setEnabled(false);
      if ( NULL != m_pItemsSelect[5] )
         m_pItemsSelect[5]->setEnabled(false);
   }
}

void MenuDeviceI2C::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuDeviceI2C::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
   m_iConfirmationId = 0;
}

void MenuDeviceI2C::onSelectItem()
{
   ControllerSettings* pCS = get_ControllerSettings();

   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   t_i2c_device_settings* pInfo = hardware_i2c_get_device_settings((u8)m_nDeviceId);
   if ( NULL == pInfo )
      return;

   bool bUpdated = false;

   if ( m_IndexDevType == m_SelectedIndex )
   {
      int index = m_pItemsSelect[0]->getSelectedIndex();
      if ( 0 == index )
         pInfo->nDeviceType = I2C_DEVICE_TYPE_UNKNOWN;
      if ( 1 == index )
         pInfo->nDeviceType = I2C_DEVICE_TYPE_INA219;
      if ( 2 == index )
         pInfo->nDeviceType = I2C_DEVICE_TYPE_PICO_RC_IN;
      if ( 3 == index )
         pInfo->nDeviceType = I2C_DEVICE_TYPE_PICO_EXTENDER;
      if ( 4 == index )
         pInfo->nDeviceType = I2C_DEVICE_TYPE_RUBY_ADDON;
      hardware_i2c_save_device_settings();
      createItems();
      bUpdated = true;
   }

   if ( m_IndexEnable == m_SelectedIndex )
   {
      int index = m_pItemsSelect[1]->getSelectedIndex();
      if ( 0 == index )
         pInfo->bEnabled = false;
      if ( 1 == index )
         pInfo->bEnabled = true;
      hardware_i2c_save_device_settings();
      bUpdated = true;
   }

   if ( m_IndexCustomSettings1 == m_SelectedIndex )
   {
      int index = m_pItemsSelect[2]->getSelectedIndex();
      pInfo->uParams[0] = index;
      hardware_i2c_save_device_settings();
      bUpdated = true;
   }

   if ( m_IndexCustomSettings2 == m_SelectedIndex )
   {
      int index = m_pItemsSelect[3]->getSelectedIndex();
      pInfo->uParams[1] = index;
      hardware_i2c_save_device_settings();
      bUpdated = true;
   }

   if ( m_IndexCustomSettings3 == m_SelectedIndex )
   {
      int index = m_pItemsSelect[4]->getSelectedIndex();
      pInfo->uParams[2] = index;
      if ( (pInfo->nDeviceType == I2C_DEVICE_TYPE_PICO_EXTENDER) ||
           ((pInfo->nDeviceType == I2C_DEVICE_TYPE_RUBY_ADDON) && (pInfo->uCapabilitiesFlags & I2C_CAPABILITY_FLAG_ROTARY)) )
      {
         pCS->nRotaryEncoderFunction = index;
         save_ControllerSettings();
      }
      hardware_i2c_save_device_settings();
      bUpdated = true;
   }

   if ( m_IndexCustomSettings4 == m_SelectedIndex )
   {
      int index = m_pItemsSelect[5]->getSelectedIndex();
      pInfo->uParams[3] = index;
      if ( (pInfo->nDeviceType == I2C_DEVICE_TYPE_PICO_EXTENDER) ||
           ((pInfo->nDeviceType == I2C_DEVICE_TYPE_RUBY_ADDON) && (pInfo->uCapabilitiesFlags & I2C_CAPABILITY_FLAG_ROTARY)) )
      {
         pCS->nRotaryEncoderSpeed = index;
         save_ControllerSettings();
      }
      hardware_i2c_save_device_settings();
      bUpdated = true;
   }

   if ( bUpdated )
   {
      send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_I2C_DEVICE_CHANGED, PACKET_COMPONENT_RUBY);
      char szBuff[128];
      sprintf(szBuff, "touch %s", FILE_TMP_I2C_UPDATED);
      hw_execute_bash_command_silent(szBuff, NULL);
      valuesToUI();
   }   
}
