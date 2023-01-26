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
#include "menu_vehicle_audio.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include "handle_commands.h"


MenuVehicleAudio::MenuVehicleAudio(void)
:Menu(MENU_ID_VEHICLE_AUDIO, "Audio Settings", NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.26;

   m_pItemsSelect[0] = new MenuItemSelect("Audio", "Enables audio stream.");
   m_pItemsSelect[0]->addSelection("Disabled");
   m_pItemsSelect[0]->addSelection("Enabled");
   m_pItemsSelect[0]->setIsEditable();

   m_IndexEnable = addMenuItem(m_pItemsSelect[0]);

   if ( ! g_pCurrentModel->audio_params.has_audio_device )
   {
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSelect[0]->setEnabled(false);
      addMenuItem(new MenuItemText("This vehicle doesn't have any audio capture device. Audio can't be used on this vehicle."));
      m_IndexVolume = -1;
      m_IndexQuality = -1;
      return;
   }

   m_pItemsSlider[0] = new MenuItemSlider("Volume", "Audio recording volume.", 0,100,5, 0.1);
   m_IndexVolume = addMenuItem(m_pItemsSlider[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Quality", "Sets the recording quality of the audio stream.");
   m_pItemsSelect[1]->addSelection("Low");
   m_pItemsSelect[1]->addSelection("Normal");
   m_pItemsSelect[1]->addSelection("High");
   m_pItemsSelect[1]->addSelection("Highest");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexQuality = addMenuItem(m_pItemsSelect[1]);
}

MenuVehicleAudio::~MenuVehicleAudio()
{
}

void MenuVehicleAudio::valuesToUI()
{
   m_pItemsSelect[0]->setSelectedIndex(0);
   if ( g_pCurrentModel->audio_params.enabled )
   {
      m_pItemsSelect[0]->setSelectedIndex(1);
   }
   if ( ! g_pCurrentModel->audio_params.has_audio_device )
   {
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSelect[0]->setEnabled(false);
      return;
   }
   m_pItemsSlider[0]->setEnabled(false);
   m_pItemsSelect[1]->setEnabled(false);
   if ( g_pCurrentModel->audio_params.enabled )
   {
      m_pItemsSlider[0]->setEnabled(true);
      m_pItemsSelect[1]->setEnabled(true);
   }

   m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->audio_params.volume);
   m_pItemsSelect[1]->setSelectedIndex(g_pCurrentModel->audio_params.quality);
}

void MenuVehicleAudio::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleAudio::sendParams()
{
   audio_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t));

   params.enabled = (bool) m_pItemsSelect[0]->getSelectedIndex();
   params.volume = m_pItemsSlider[0]->getCurrentValue();
   params.quality = m_pItemsSelect[1]->getSelectedIndex();

   if ( params.enabled == g_pCurrentModel->audio_params.enabled )
   if ( params.volume == g_pCurrentModel->audio_params.volume )
   if ( params.quality == g_pCurrentModel->audio_params.quality )
      return;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_AUDIO_PARAMS, 0, (u8*)&params, sizeof(audio_parameters_t)) )
      valuesToUI();
}

void MenuVehicleAudio::onSelectItem()
{
   Menu::onSelectItem();
   if ( -1 == m_SelectedIndex )
      return;
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexEnable == m_SelectedIndex && ! m_pMenuItems[m_SelectedIndex]->isEditing() )
   {
      sendParams();
      return;
   }
   if ( m_IndexVolume == m_SelectedIndex && ! m_pMenuItems[m_SelectedIndex]->isEditing() )
   {
      sendParams();
      return;
   }
   if ( m_IndexQuality == m_SelectedIndex && ! m_pMenuItems[m_SelectedIndex]->isEditing() )
   {
      sendParams();
      return;
   }
}
