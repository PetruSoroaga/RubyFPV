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
#include "menu.h"
#include "menu_vehicle_rc.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_confirmation.h"
#include "menu_vehicle_rc_camera.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"


MenuVehicleRCCamera::MenuVehicleRCCamera(void)
:Menu(MENU_ID_VEHICLE_RC_CAMERA, "Camera Remote Control Settings", NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.36;
   float fSliderWidth = 0.12;

   m_pItemsSelect[0] = new MenuItemSelect("Camera Control Pitch Channel", "Sets the RC channel to be used to control the camera pitch angle.");
   m_pItemsSelect[0]->addSelection("None");
   for( int i=0; i<g_pCurrentModel->rc_params.channelsCount; i++ )
   {
      char szBuff[32];
      snprintf(szBuff, 31, "CH %d", i+1);
      m_pItemsSelect[0]->addSelection(szBuff);
   }
   m_pItemsSelect[0]->setIsEditable();
   m_IndexPitch = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Camera Control Roll Channel", "Sets the RC channel to be used to control the camera roll angle.");
   m_pItemsSelect[1]->addSelection("None");
   for( int i=0; i<g_pCurrentModel->rc_params.channelsCount; i++ )
   {
      char szBuff[32];
      snprintf(szBuff, 31, "CH %d", i+1);
      m_pItemsSelect[1]->addSelection(szBuff);
   }
   m_pItemsSelect[1]->setIsEditable();
   m_IndexRoll = addMenuItem(m_pItemsSelect[1]);

   m_pItemsSelect[2] = new MenuItemSelect("Camera Control Yaw Channel", "Sets the RC channel to be used to control the camera yaw angle.");
   m_pItemsSelect[2]->addSelection("None");
   for( int i=0; i<g_pCurrentModel->rc_params.channelsCount; i++ )
   {
      char szBuff[32];
      snprintf(szBuff, 31, "CH %d", i+1);
      m_pItemsSelect[2]->addSelection(szBuff);
   }
   m_pItemsSelect[2]->setIsEditable();
   m_IndexYaw = addMenuItem(m_pItemsSelect[2]);

   m_pItemsSelect[3] = new MenuItemSelect("Move Mode", "Sets move mode for the camera, that is how the camera moves based on the joystick moves.");
   m_pItemsSelect[3]->addSelection("Relative");
   m_pItemsSelect[3]->addSelection("Absolute");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexRelative = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSlider[0] = new MenuItemSlider("Relative Move Speed", 1,30, 20, fSliderWidth);
   m_IndexSpeed = addMenuItem(m_pItemsSlider[0]);
}

MenuVehicleRCCamera::~MenuVehicleRCCamera()
{

}


void MenuVehicleRCCamera::onShow()
{
   Menu::onShow();
}

void MenuVehicleRCCamera::valuesToUI()
{
   //log_dword("-------------cam val update: ", g_pCurrentModel->camera_rc_channels);

   if ( ((g_pCurrentModel->camera_rc_channels>>30) & 0x03) != 0x03 )
      return;

   int moveMode = (int)((((g_pCurrentModel->camera_rc_channels>>24) & 0xFF) >> 5) & 0x01);
   m_pItemsSelect[0]->setSelection((int)((g_pCurrentModel->camera_rc_channels & 0xFF) & 0x1F));
   m_pItemsSelect[1]->setSelection((int)(((g_pCurrentModel->camera_rc_channels>>8) & 0xFF) & 0x1F));
   m_pItemsSelect[2]->setSelection((int)(((g_pCurrentModel->camera_rc_channels>>16) & 0xFF) & 0x1F));

   bool bAnyEnabled = false;
   if ( 0 != m_pItemsSelect[0]->getSelectedIndex() ||
        1 != m_pItemsSelect[0]->getSelectedIndex() ||
        2 != m_pItemsSelect[0]->getSelectedIndex() )
      bAnyEnabled = true;

   m_pItemsSelect[3]->setSelection(moveMode);
   m_pItemsSlider[0]->setCurrentValue((int)((g_pCurrentModel->camera_rc_channels>>24) & 0x1F));
   if ( 0 == moveMode )
      m_pItemsSlider[0]->setEnabled(true);
   else
      m_pItemsSlider[0]->setEnabled(false);

   if ( ! bAnyEnabled )
   {
      m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSlider[0]->setEnabled(false);
   }
}


void MenuVehicleRCCamera::Render()
{
   RenderPrepare();   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuVehicleRCCamera::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
   m_iConfirmationId = 0;
}

void MenuVehicleRCCamera::send_params()
{
   u32 pitch = m_pItemsSelect[0]->getSelectedIndex();
   u32 roll = m_pItemsSelect[1]->getSelectedIndex();
   u32 yaw = m_pItemsSelect[2]->getSelectedIndex();
   u32 move = m_pItemsSelect[3]->getSelectedIndex();
   u32 speed = m_pItemsSlider[0]->getCurrentValue();
   
   u32 val = (pitch & 0x1F);
   val = val | ((roll & 0x1F)<<8);
   val = val | ((yaw & 0x1F)<<16);
   val = val | ((speed & 0x1F)<<24);
   val = val | ((speed & 0x1F)<<24);
   val = val | ((move << 24) <<5);
   val = val | (0x03<<30); // consistency check
   
   //log_dword("-------cam val to send", val);
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_CAMERA_PARAMS, val, NULL, 0) )
      valuesToUI();
}

void MenuVehicleRCCamera::onSelectItem()
{
   Menu::onSelectItem();
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexPitch == m_SelectedIndex && ! m_pMenuItems[m_SelectedIndex]->isEditing())
      send_params();

   if ( m_IndexRoll == m_SelectedIndex && ! m_pMenuItems[m_SelectedIndex]->isEditing())
      send_params();

   if ( m_IndexYaw == m_SelectedIndex && ! m_pMenuItems[m_SelectedIndex]->isEditing())
      send_params();

   if ( m_IndexRelative == m_SelectedIndex && ! m_pMenuItems[m_SelectedIndex]->isEditing())
      send_params();

   if ( m_IndexSpeed == m_SelectedIndex && ! m_pMenuItems[m_SelectedIndex]->isEditing())
      send_params();
}