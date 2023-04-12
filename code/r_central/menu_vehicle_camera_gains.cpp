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
#include "menu_vehicle_camera_gains.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_legend.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"


MenuVehicleCameraGains::MenuVehicleCameraGains(void)
:Menu(MENU_ID_VEHICLE_CAMERA_GAINS, "Camera Analog Gains", NULL)
{
   m_Width = 0.20;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.40;
   float fSliderWidth = 0.10;

   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   camera_profile_parameters_t* pCamProfile = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile]);

   if ( NULL != g_pCurrentModel && pCamProfile->whitebalance != 0 )
      addMenuItem(new MenuItemLegend("Warning", "Analog gains are ignored if AWB is not turned off.", 0) );
   

   m_pItemsRange[0] = new MenuItemRange("Analog Gain:", "Analog gain of the camera sensor when AWB is turned off.", 1.0, 8.0, pCamProfile->analogGain, 0.1 );  
   m_pItemsRange[0]->setSufix("");
   m_IndexGain = addMenuItem(m_pItemsRange[0]);

   m_pItemsRange[1] = new MenuItemRange("Blue Gain:", "Blue gain when AWB is turned off.", 0.0, 5.0, pCamProfile->awbGainB, 0.1 );  
   m_pItemsRange[1]->setSufix("");
   m_IndexGainB = addMenuItem(m_pItemsRange[1]);

   m_pItemsRange[2] = new MenuItemRange("Red Gain:", "Red gain when AWB is turned off.", 0.0, 5.0, pCamProfile->awbGainR, 0.1 );  
   m_pItemsRange[2]->setSufix("");
   m_IndexGainR = addMenuItem(m_pItemsRange[2]);
}

MenuVehicleCameraGains::~MenuVehicleCameraGains()
{
}

void MenuVehicleCameraGains::updateUIValues()
{
   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   camera_profile_parameters_t* pCamProfile = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile]);
   m_pItemsRange[0]->setCurrentValue(pCamProfile->analogGain);
   m_pItemsRange[1]->setCurrentValue(pCamProfile->awbGainB);
   m_pItemsRange[2]->setCurrentValue(pCamProfile->awbGainR);
}

void MenuVehicleCameraGains::valuesToUI()
{
   updateUIValues();
}


void MenuVehicleCameraGains::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleCameraGains::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
}

void MenuVehicleCameraGains::onSelectItem()
{
   Menu::onSelectItem();

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexGain == m_SelectedIndex && ! m_pMenuItems[m_IndexGain]->isEditing() )
   {
      type_camera_parameters cparams;
      memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
      int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   
      cparams.profiles[iProfile].analogGain = m_pItemsRange[0]->getCurrentValue();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters)) )
         valuesToUI();
   }

   if ( m_IndexGainB == m_SelectedIndex && ! m_pMenuItems[m_IndexGainB]->isEditing() )
   {
      type_camera_parameters cparams;
      memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
      int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;

      cparams.profiles[iProfile].awbGainB = m_pItemsRange[1]->getCurrentValue();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters)) )
         valuesToUI();
   }

   if ( m_IndexGainR == m_SelectedIndex && ! m_pMenuItems[m_IndexGainR]->isEditing() )
   {
      type_camera_parameters cparams;
      memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
      int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;

      cparams.profiles[iProfile].awbGainR = m_pItemsRange[2]->getCurrentValue();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters)) )
         valuesToUI();
   }
}