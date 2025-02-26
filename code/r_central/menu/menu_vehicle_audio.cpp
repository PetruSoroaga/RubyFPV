/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "menu.h"
#include "menu_vehicle_audio.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_item_legend.h"
#include "../../base/hardware_audio.h"


MenuVehicleAudio::MenuVehicleAudio(void)
:Menu(MENU_ID_VEHICLE_AUDIO, "Audio Settings", NULL)
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.26;
}

MenuVehicleAudio::~MenuVehicleAudio()
{
}

void MenuVehicleAudio::onShow()
{
   int iTmp = getSelectedMenuItemIndex();
   addItems();
   Menu::onShow();
   if ( iTmp >= 0 )
      m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuVehicleAudio::addItems()
{
   int iTmp = getSelectedMenuItemIndex();
   removeAllItems();

   float fSliderWidth = 0.1;
   m_IndexOIPCMic = -1;
   m_IndexVolume = -1;
   m_IndexQuality = -1;

   m_IndexDevBufferingSize = -1;
   m_IndexDevPacketLength = -1;
   m_IndexDevDataPackets = -1;
   m_IndexDevECPackets = -1;

   if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      g_pCurrentModel->audio_params.has_audio_device = true;
      m_pItemsSelect[5] = new MenuItemSelect("Microphone", "Select the type of microphone present on this vehicle, if any.");
      m_pItemsSelect[5]->addSelection("None");
      m_pItemsSelect[5]->addSelection("Builtin");
      m_pItemsSelect[5]->addSelection("External");
      m_pItemsSelect[5]->setIsEditable();
      m_IndexOIPCMic = addMenuItem(m_pItemsSelect[5]);
      m_pItemsSelect[5]->setSelectedIndex(0);
      if ( (g_pCurrentModel->audio_params.uFlags & 0x03) == 1 )
         m_pItemsSelect[5]->setSelectedIndex(1);
      if ( (g_pCurrentModel->audio_params.uFlags & 0x03) == 2 )
         m_pItemsSelect[5]->setSelectedIndex(2);

      if ( hardware_board_has_audio_builtin(g_pCurrentModel->hwCapabilities.uBoardType) )
      {
         addMenuItem(new MenuItemText(L("Your camera has a builtin microphone."), true));
         m_pItemsSelect[5]->setSelectedIndex(1);
         m_pItemsSelect[5]->setEnabled(false);
      }
   }

   m_pItemsSelect[0] = new MenuItemSelect("Audio Stream", "Enables or disables the audio stream.");
   m_pItemsSelect[0]->addSelection("Disabled");
   m_pItemsSelect[0]->addSelection("Enabled");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexEnable = addMenuItem(m_pItemsSelect[0]);
   m_pItemsSelect[0]->setSelectedIndex(0);
   if ( g_pCurrentModel->audio_params.enabled )
      m_pItemsSelect[0]->setSelectedIndex(1);

   if ( ! g_pCurrentModel->audio_params.has_audio_device )
   {
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSelect[0]->setEnabled(false);
      addMenuItem(new MenuItemText("This vehicle doesn't have any audio capture device. Audio can't be used on this vehicle."));
      m_SelectedIndex = 0;
      return;
   }

   m_pItemsSlider[0] = new MenuItemSlider("Volume", "Audio recording volume.", 0,100,5, fSliderWidth);
   m_pItemsSlider[0]->setStep(1);
   m_pItemsSlider[0]->setEnabled(g_pCurrentModel->audio_params.enabled);
   m_IndexVolume = addMenuItem(m_pItemsSlider[0]);
   m_pItemsSlider[0]->setCurrentValue(g_pCurrentModel->audio_params.volume);

   if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      m_pItemsSelect[1] = new MenuItemSelect("Quality", "Sets the quality of the audio stream.");
      m_pItemsSelect[1]->addSelection("8kbps");
      m_pItemsSelect[1]->addSelection("16kbps");
      m_pItemsSelect[1]->addSelection("24kbps");
      m_pItemsSelect[1]->addSelection("48kbps");
      m_pItemsSelect[1]->setIsEditable();
      m_IndexQuality = addMenuItem(m_pItemsSelect[1]);
   }
   else
   {
      m_pItemsSelect[1] = new MenuItemSelect("Quality", "Sets the quality of the audio stream.");
      m_pItemsSelect[1]->addSelection("Low");
      m_pItemsSelect[1]->addSelection("Normal");
      m_pItemsSelect[1]->addSelection("High");
      m_pItemsSelect[1]->addSelection("Highest");
      m_pItemsSelect[1]->setIsEditable();
      m_IndexQuality = addMenuItem(m_pItemsSelect[1]);
   }

   m_pItemsSelect[1]->setEnabled(g_pCurrentModel->audio_params.enabled);
   m_pItemsSelect[1]->setSelectedIndex(g_pCurrentModel->audio_params.quality);

   if ( hardware_board_is_openipc(g_pCurrentModel->hwCapabilities.uBoardType) )
   if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
   {
      m_pItemsSelect[0]->setSelectedIndex(0);
      m_pItemsSelect[0]->setEnabled(false);
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSlider[0]->setEnabled(false);
   }

   ControllerSettings* pCS = get_ControllerSettings();
   if ( (NULL != pCS) && pCS->iDeveloperMode )
   {
      m_pItemsSlider[4] = new MenuItemSlider("Buffering size", "How much to buffer audio. Longer buffering increase the consistency of audio stream but it adds delay.", 0,(MAX_BUFFERED_AUDIO_PACKETS*2)/3,5, fSliderWidth);
      m_pItemsSlider[4]->setStep(1);
      m_pItemsSlider[4]->setEnabled(g_pCurrentModel->audio_params.enabled);
      m_IndexDevBufferingSize = addMenuItem(m_pItemsSlider[4]);
      m_pItemsSlider[4]->setCurrentValue((g_pCurrentModel->audio_params.uFlags >> 8) & 0xFF);


      m_pItemsSlider[1] = new MenuItemSlider("Packet size", "Audio packets size over air", 50,1200,500, fSliderWidth);
      m_pItemsSlider[1]->setStep(10);
      m_pItemsSlider[1]->setEnabled(g_pCurrentModel->audio_params.enabled);
      m_IndexDevPacketLength = addMenuItem(m_pItemsSlider[1]);
      m_pItemsSlider[1]->setCurrentValue(g_pCurrentModel->audio_params.uPacketLength);

      m_pItemsSlider[2] = new MenuItemSlider("Data packets", "How many data packets for each correction data", 1,15,5, fSliderWidth);
      m_pItemsSlider[2]->setStep(1);
      m_pItemsSlider[2]->setEnabled(g_pCurrentModel->audio_params.enabled);
      m_IndexDevDataPackets = addMenuItem(m_pItemsSlider[2]);
      m_pItemsSlider[2]->setCurrentValue((g_pCurrentModel->audio_params.uECScheme >> 4) & 0x0F);

      m_pItemsSlider[3] = new MenuItemSlider("EC packets", "How many ec packets", 1,15,5, fSliderWidth);
      m_pItemsSlider[3]->setStep(1);
      m_pItemsSlider[3]->setEnabled(g_pCurrentModel->audio_params.enabled);
      m_IndexDevECPackets = addMenuItem(m_pItemsSlider[3]);
      m_pItemsSlider[3]->setCurrentValue(g_pCurrentModel->audio_params.uECScheme & 0x0F);

      m_pMenuItems[m_IndexDevBufferingSize]->setTextColor(get_Color_Dev());
      m_pMenuItems[m_IndexDevPacketLength]->setTextColor(get_Color_Dev());
      m_pMenuItems[m_IndexDevDataPackets]->setTextColor(get_Color_Dev());
      m_pMenuItems[m_IndexDevECPackets]->setTextColor(get_Color_Dev());
   }

   if ( iTmp >= 0 )
      m_SelectedIndex = iTmp;
   if ( m_SelectedIndex >= m_ItemsCount )
      m_SelectedIndex = m_ItemsCount-1;
}

void MenuVehicleAudio::valuesToUI()
{
   addItems();
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

void MenuVehicleAudio::sendParams(bool bOneWay)
{
   log_line("MenuAudio: Check send audio params.");
   audio_parameters_t params;
   memcpy(&params, &(g_pCurrentModel->audio_params), sizeof(audio_parameters_t));

   params.enabled = (bool) m_pItemsSelect[0]->getSelectedIndex();
   params.volume = m_pItemsSlider[0]->getCurrentValue();
   params.quality = m_pItemsSelect[1]->getSelectedIndex();

   if ( -1 != m_IndexOIPCMic )
   {
      params.uFlags &= ~((u32)0x03);
      params.uFlags |= ((u32)m_pItemsSelect[5]->getSelectedIndex());
      if ( 0 == m_pItemsSelect[5]->getSelectedIndex() )
         params.enabled = false;
   }

   if ( -1 != m_IndexDevBufferingSize )
   {
      params.uFlags &= 0xFFFF00FF;
      params.uFlags = (((u32)m_pItemsSlider[4]->getCurrentValue()) & 0xFF) << 8;
   }
   if ( -1 != m_IndexDevPacketLength )
      params.uPacketLength = m_pItemsSlider[1]->getCurrentValue();

   if ( -1 != m_IndexDevDataPackets )
   {
      params.uECScheme &= ~(0xF0);
      params.uECScheme |= ((m_pItemsSlider[2]->getCurrentValue() & 0x0F) << 4);
   }
   if ( -1 != m_IndexDevECPackets )
   {
      params.uECScheme &= ~(0x0F);
      params.uECScheme |= (m_pItemsSlider[3]->getCurrentValue() & 0x0F);
   }

   if ( params.uFlags == g_pCurrentModel->audio_params.uFlags )
   if ( params.enabled == g_pCurrentModel->audio_params.enabled )
   if ( params.volume == g_pCurrentModel->audio_params.volume )
   if ( params.quality == g_pCurrentModel->audio_params.quality )
   if ( params.uPacketLength == g_pCurrentModel->audio_params.uPacketLength )
   if ( params.uECScheme == g_pCurrentModel->audio_params.uECScheme )
   {
      log_line("MenuAudio: Check send audio params: no change.");
      return;
   }

   if ( bOneWay )
      handle_commands_send_single_oneway_command(2, COMMAND_ID_SET_AUDIO_PARAMS, 0, (u8*)&params, sizeof(audio_parameters_t));
   else if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_AUDIO_PARAMS, 0, (u8*)&params, sizeof(audio_parameters_t)) )
      valuesToUI();
}



void MenuVehicleAudio::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
   //if ( ! m_pMenuItems[m_SelectedIndex]->isEditing() )
   //   return;
   //if ( itemIndex == m_IndexVolume )
   //   sendParams(true);
}

void MenuVehicleAudio::onItemEndEdit(int itemIndex)
{
   Menu::onItemEndEdit(itemIndex);
}


void MenuVehicleAudio::onSelectItem()
{
   Menu::onSelectItem();
   if ( -1 == m_SelectedIndex )
      return;

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexEnable == m_SelectedIndex )
   {
      sendParams(false);
      return;
   }
   if ( m_IndexVolume == m_SelectedIndex )
   {
      sendParams(false);
      return;
   }
   if ( m_IndexQuality == m_SelectedIndex )
   {
      sendParams(false);
      return;
   }
   if ( (-1 != m_IndexOIPCMic) && (m_IndexOIPCMic == m_SelectedIndex) )
   {
      sendParams(false);
      return;
   }

   if ( (-1 != m_IndexDevBufferingSize) && (m_IndexDevBufferingSize == m_SelectedIndex) )
   {
      sendParams(false);
      return;
   }

   if ( (-1 != m_IndexDevPacketLength) && (m_IndexDevPacketLength == m_SelectedIndex) )
   {
      sendParams(false);
      return;
   }

   if ( (-1 != m_IndexDevDataPackets) && (m_IndexDevDataPackets == m_SelectedIndex) )
   {
      sendParams(false);
      return;
   }

   if ( (-1 != m_IndexDevECPackets) && (m_IndexDevECPackets == m_SelectedIndex) )
   {
      sendParams(false);
      return;
   }
}
