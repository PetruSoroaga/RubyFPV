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
#include "../osd/osd_common.h"
#include "menu_vehicle_radio.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"
#include "menu_vehicle_radio_link.h"
#include "menu_vehicle_radio_link_sik.h"
#include "menu_radio_config.h"
#include "menu_txinfo.h"

#include "../link_watch.h"
#include "../launchers_controller.h"
#include "../../base/encr.h"

int s_iTempGenNewFrequency = 0;
int s_iTempGenNewFrequencyLink = 0;

MenuVehicleRadioConfig::MenuVehicleRadioConfig(void)
:Menu(MENU_ID_VEHICLE_RADIO_CONFIG, "Vehicle Radio Configuration", NULL)
{
   m_Width = 0.30;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.21;
   m_bControllerHasKey = false;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      m_IndexFreq[i] = -1;
      m_IndexConfigureLinks[i] = -1;
   }
   valuesToUI();
}

MenuVehicleRadioConfig::~MenuVehicleRadioConfig()
{
}

void MenuVehicleRadioConfig::populate()
{
   removeAllItems();
   m_IndexPower = -1;
   char szBuff[128];
   char szTooltip[256];
   char szTitle[128];
   char szInfo[128];
   strcpy(szInfo, " A radio link can be: (C) connected, (D) disconnected or (R) used by vehicle for relaying.");

   int len = 64;
   int res = lpp(szBuff, len);
   if ( res )
      m_bControllerHasKey = true;
   else
      m_bControllerHasKey = false;

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      m_IndexFreq[i] = -1;

   if ( 0 == g_pCurrentModel->radioLinksParams.links_count )
   {
      addMenuItem( new MenuItemText("No radio interfaces detected on this vehicle!"));
      return;
   }

   u32 uControllerAllSupportedBands = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);
      if ( ! (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
         uControllerAllSupportedBands |= pRadioHWInfo->supportedBands;
   }

   str_get_supported_bands_string(uControllerAllSupportedBands, szBuff);
   log_line("MenuVehicleRadio: All supported radio bands by controller: %s", szBuff);

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      m_IndexFreq[iRadioLinkId] = -1;
      
      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(iRadioLinkId);
      if ( -1 == iRadioInterfaceId )
      {
         log_softerror_and_alarm("Invalid radio link. No radio interfaces assigned to radio link %d.", iRadioLinkId+1);
         continue;
      }

      str_get_supported_bands_string(g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], szBuff);
      log_line("MenuVehicleRadio: Vehicle radio interface %d (used on vehicle radio link %d) supported bands: %s", 
         iRadioInterfaceId+1, iRadioLinkId+1, szBuff);

      if ( g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceId] == CARD_MODEL_SERIAL_RADIO_ELRS )
      {
         m_SupportedChannelsCount[iRadioLinkId] = 1;
         m_SupportedChannels[iRadioLinkId][0] = g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId];
      }
      else
         m_SupportedChannelsCount[iRadioLinkId] = getSupportedChannels( uControllerAllSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 1, &(m_SupportedChannels[iRadioLinkId][0]), 100);

      log_line("MenuVehicleRadio: Suported channels count by controller and vehicle radio interface %d: %d",
          iRadioInterfaceId+1, m_SupportedChannelsCount[iRadioLinkId]);

      char szTmp[128];
      strcpy(szTmp, "Radio Link Frequency");
      if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
         sprintf(szTmp, "Radio Link %d Frequency", iRadioLinkId+1 );

      strcpy(szTooltip, "Sets the radio link frequency for this radio link.");
      snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), " Radio type: %s.", str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceId]));
      strcat(szTooltip, szBuff);
      
      int iCountInterfacesAssignedToThisLink = 0;
      for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iRadioLinkId )
            iCountInterfacesAssignedToThisLink++;
      }

      if ( 0 == iCountInterfacesAssignedToThisLink )
      {
         snprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), "(D) %s", szTmp);
         strcat(szTooltip, " This radio link is not connected to the controller.");
      }
      else
      {
         snprintf(szTitle, sizeof(szTitle)/sizeof(szTitle[0]), "(C) %s", szTmp);
         strcat(szTooltip, " This radio link is connected to the controller.");
      }

      strcat(szTooltip, szInfo);

      m_pItemsSelect[20+iRadioLinkId] = new MenuItemSelect(szTitle, szTooltip);

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == iRadioLinkId )
      {
         sprintf(szBuff, "Relay on %s", str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequencyKhz));
         m_pItemsSelect[20+iRadioLinkId]->addSelection(szBuff);
         m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
      }
      else
      {
         Preferences* pP = get_Preferences();
         int iCountChCurrentColumn = 0;
         for( int ch=0; ch<m_SupportedChannelsCount[iRadioLinkId]; ch++ )
         {
            if ( (pP->iScaleMenus > 0) && (iCountChCurrentColumn > 14 ) )
            {
               m_pItemsSelect[20+iRadioLinkId]->addSeparator();
               iCountChCurrentColumn = 0;
            }
            if ( 0 == m_SupportedChannels[iRadioLinkId][ch] )
            {
               m_pItemsSelect[20+iRadioLinkId]->addSeparator();
               iCountChCurrentColumn = 0;
               continue;
            }
            iCountChCurrentColumn++;
            sprintf(szBuff,"%s", str_format_frequency(m_SupportedChannels[iRadioLinkId][ch]));
            m_pItemsSelect[20+iRadioLinkId]->addSelection(szBuff);
         }
         log_line("MenuVehicleRadio: Added %d frequencies to selector for radio link %d", m_SupportedChannelsCount[iRadioLinkId], iRadioLinkId+1);
         m_SupportedChannelsCount[iRadioLinkId] = getSupportedChannels( uControllerAllSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 0, &(m_SupportedChannels[iRadioLinkId][0]), 100);
         log_line("MenuVehicleRadio: %d frequencies supported for radio link %d", m_SupportedChannelsCount[iRadioLinkId], iRadioLinkId+1);
         
         int selectedIndex = 0;
         for( int ch=0; ch<m_SupportedChannelsCount[iRadioLinkId]; ch++ )
         {
            if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId] == m_SupportedChannels[iRadioLinkId][ch] )
               break;
            selectedIndex++;
         }
         m_pItemsSelect[20+iRadioLinkId]->setSelection(selectedIndex);

         if ( 0 == m_SupportedChannelsCount[iRadioLinkId] )
         {
            sprintf(szBuff,"%s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[iRadioLinkId]));
            m_pItemsSelect[20+iRadioLinkId]->addSelection(szBuff);
            m_pItemsSelect[20+iRadioLinkId]->setSelectedIndex(0);
            m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
         }

         m_pItemsSelect[20+iRadioLinkId]->setIsEditable();
         //if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         //   m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
         if ( g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceId] == CARD_MODEL_SERIAL_RADIO_ELRS )
            m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
      }
      m_IndexFreq[iRadioLinkId] = addMenuItem(m_pItemsSelect[20+iRadioLinkId]);
   }

   /*
   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(iRadioLinkId);
      if ( -1 == iRadioInterfaceId )
      {
         log_softerror_and_alarm("Invalid radio link. No radio interfaces assigned to radio link %d.", iRadioLinkId+1);
         continue;
      }

      int iCountInterfacesAssignedToThisLink = 0;
      for( int i=0; i<g_SM_RadioStats.countLocalRadioInterfaces; i++ )
      {
         if ( g_SM_RadioStats.radio_interfaces[i].assignedVehicleRadioLinkId == iRadioLinkId )
            iCountInterfacesAssignedToThisLink++;
      }

      char szTooltip[256];
      m_IndexConfigureLinks[iRadioLinkId] = -1;

      strcpy(szTooltip, "Change the radio parameters for this radio link.");
      sprintf(szBuff, " Radio type: %s;", str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceId]));
      strcat(szTooltip, szBuff);
      
      if ( 0 == iCountInterfacesAssignedToThisLink )
      {
         sprintf(szTitle, "(D) Configure Radio Link %d", iRadioLinkId+1);
         strcat(szTooltip, " This radio link is not connected to the controller.");
      }
      else
      {
         sprintf(szTitle, "(C) Configure Radio Link %d", iRadioLinkId+1);
         strcat(szTooltip, " This radio link is connected to the controller.");
      }
      strcat(szTooltip, szInfo);
      m_IndexConfigureLinks[iRadioLinkId] = addMenuItem(new MenuItem(szTitle, szTooltip));
      m_pMenuItems[m_IndexConfigureLinks[iRadioLinkId]]->showArrow();
   }
   */

   m_IndexPower = addMenuItem(new MenuItem("Tx Power Level", "Change the transmit power levels for the vehicle's radio interfaces."));
   m_pMenuItems[m_IndexPower]->showArrow();

   m_IndexRadioConfig = addMenuItem(new MenuItem("Full Radio Config", "Full radio configuration"));
   m_pMenuItems[m_IndexRadioConfig]->showArrow();

   m_pItemsSelect[4] = new MenuItemSelect("Disable Uplinks", "Disable all uplinks, makes the system a one way system. Except for initial pairing and synching and sending commands to the vehicle. No video retransmissions happen, adaptive video is also disabled.");
   m_pItemsSelect[4]->addSelection("No");
   m_pItemsSelect[4]->addSelection("Yes");
   m_pItemsSelect[4]->setIsEditable();
   m_IndexDisableUplink = addMenuItem(m_pItemsSelect[4]);

   m_pItemsSelect[3] = new MenuItemSelect("Prioritize Uplink", "Prioritize Uplink data over Downlink data. Enable it when uplink data resilience and consistentcy is more important than downlink data.");
   m_pItemsSelect[3]->addSelection("No");
   m_pItemsSelect[3]->addSelection("Yes");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexPrioritizeUplink = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[2] = new MenuItemSelect("Radio Encryption", "Changes the encryption used for the radio links. You can encrypt the video data, or telemetry data, or everything, including the ability to search for and find this vehicle (unless your controller has the right pass phrase).");
   m_pItemsSelect[2]->addSelection("None");
   m_pItemsSelect[2]->addSelection("Video Stream Only");
   m_pItemsSelect[2]->addSelection("Data Streams Only");
   m_pItemsSelect[2]->addSelection("Video and Data Streams");
   m_pItemsSelect[2]->addSelection("All Streams and Data");
   m_pItemsSelect[2]->setIsEditable();
   m_IndexEncryption = addMenuItem(m_pItemsSelect[2]);
}

void MenuVehicleRadioConfig::valuesToUI()
{
   populate();
   
   m_pItemsSelect[2]->setSelectedIndex(0);
   m_pItemsSelect[3]->setSelectedIndex(0);

   if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_PRIORITIZE_UPLINK )
      m_pItemsSelect[3]->setSelectedIndex(1); 

   m_pItemsSelect[4]->setSelectedIndex(0);
   if ( g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags & MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY )
      m_pItemsSelect[4]->setSelectedIndex(1);
    
   if ( m_pItemsSelect[4]->getSelectedIndex() == 1 )
      m_pItemsSelect[3]->setEnabled(false);
   else
      m_pItemsSelect[3]->setEnabled(true);

   if ( g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_VIDEO )
      m_pItemsSelect[2]->setSelectedIndex(1); 

   if ( g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA )
      m_pItemsSelect[2]->setSelectedIndex(2); 

   if ( g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_DATA )
   if ( g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_VIDEO )
      m_pItemsSelect[2]->setSelectedIndex(3); 

   if ( g_pCurrentModel->enc_flags & MODEL_ENC_FLAG_ENC_ALL )
      m_pItemsSelect[2]->setSelectedIndex(4); 

   if ( ! m_bControllerHasKey )
   {
      m_pItemsSelect[2]->setSelectedIndex(0);
      m_pItemsSelect[2]->setEnabled(false);
   }

   if ( -1 != m_IndexPower )
   if ( NULL != g_pCurrentModel )
   {
      char szBuff[32];
      strcpy(szBuff, "N/A");
      bool bHas24Cards = false;
      bool bHas58Cards = false;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         if ( ((g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF00) >> 8) == RADIO_HW_DRIVER_ATHEROS )
            bHas24Cards = true;
         if ( ((g_pCurrentModel->radioInterfacesParams.interface_radiotype_and_driver[i] & 0xFF00) >> 8) == RADIO_HW_DRIVER_RALINK )
            bHas24Cards = true;
         else
            bHas58Cards = true;
      }
      if ( bHas24Cards )
         sprintf(szBuff, "%d", g_pCurrentModel->radioInterfacesParams.txPowerAtheros);
      if ( bHas58Cards )
      {
         if ( bHas24Cards )
            sprintf(szBuff, "%d / %d", g_pCurrentModel->radioInterfacesParams.txPowerAtheros, g_pCurrentModel->radioInterfacesParams.txPowerRTL);
         else
            sprintf(szBuff, "%d", g_pCurrentModel->radioInterfacesParams.txPowerRTL);
      }
      m_pMenuItems[m_IndexPower]->setValue(szBuff);
   }
}

void MenuVehicleRadioConfig::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuVehicleRadioConfig::sendNewRadioLinkFrequency(int iVehicleLinkIndex, u32 uNewFreqKhz)
{
   if ( (iVehicleLinkIndex < 0) || (iVehicleLinkIndex >= MAX_RADIO_INTERFACES) )
      return;

   log_line("MenuVehicleRadio: Changing radio link %d frequency to %u khz (%s)", iVehicleLinkIndex+1, uNewFreqKhz, str_format_frequency(uNewFreqKhz));

   type_radio_links_parameters newRadioLinkParams;
   memcpy((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   
   newRadioLinkParams.link_frequency_khz[iVehicleLinkIndex] = uNewFreqKhz;
   
   if ( 0 == memcmp((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters)) )
   {
      log_line("MenuVehicleRadio: No change in radio link frequency. Do not send command.");
      return;
   }

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE + sizeof(type_radio_links_parameters);

   u8 buffer[1024];
   static int s_iMenuRadioTestNumberCount = 0;
   s_iMenuRadioTestNumberCount++;
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));
   buffer[sizeof(t_packet_header)] = PACKET_TYPE_TEST_RADIO_LINK_PROTOCOL_VERSION;
   buffer[sizeof(t_packet_header)+1] = PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE;
   buffer[sizeof(t_packet_header)+2] = (u8)iVehicleLinkIndex;
   buffer[sizeof(t_packet_header)+3] = (u8)s_iMenuRadioTestNumberCount;
   buffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(buffer + sizeof(t_packet_header) + PACKET_TYPE_TEST_RADIO_LINK_HEADER_SIZE, &newRadioLinkParams, sizeof(type_radio_links_parameters));
   send_packet_to_router(buffer, PH.total_length);

   link_set_is_reconfiguring_radiolink(iVehicleLinkIndex);
   warnings_add_configuring_radio_link(iVehicleLinkIndex, "Changing Frequency");
}


void MenuVehicleRadioConfig::onSelectItem()
{
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( m_IndexDisableUplink == m_SelectedIndex )
   {
      u32 uFlags = g_pCurrentModel->radioLinksParams.uGlobalRadioLinksFlags;
      if ( 0 == m_pItemsSelect[4]->getSelectedIndex() )
         uFlags &= ~(MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY);
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         uFlags |= MODEL_RADIOLINKS_FLAGS_DOWNLINK_ONLY;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINKS_FLAGS, uFlags, NULL, 0) )
         valuesToUI();

   }

   if ( m_IndexPrioritizeUplink == m_SelectedIndex )
   {
      u32 uFlags = g_pCurrentModel->uModelFlags;
      uFlags &= (~MODEL_FLAG_PRIORITIZE_UPLINK);
      if ( 1 == m_pItemsSelect[3]->getSelectedIndex() )
         uFlags |= MODEL_FLAG_PRIORITIZE_UPLINK;
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_MODEL_FLAGS, uFlags, NULL, 0) )
         valuesToUI();
   }

   if ( m_IndexEncryption == m_SelectedIndex )
   {
      if ( ! m_bControllerHasKey )
      {
         MenuConfirmation* pMC = new MenuConfirmation("Missing Pass Code", "You have not set a pass code on the controller. You need first to set a pass code on the controller.", -1, true);
         pMC->m_yPos = 0.3;
         add_menu_to_stack(pMC);
         return;
      }
      u8 params[128];
      int len = 0;
      params[0] = MODEL_ENC_FLAGS_NONE; // flags
      params[1] = 0; // pass length

      if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )
         params[0] = MODEL_ENC_FLAGS_NONE;
      if ( 1 == m_pItemsSelect[2]->getSelectedIndex() )
         params[0] = MODEL_ENC_FLAG_ENC_VIDEO;
      if ( 2 == m_pItemsSelect[2]->getSelectedIndex() )
         params[0] = MODEL_ENC_FLAG_ENC_DATA ;
      if ( 3 == m_pItemsSelect[2]->getSelectedIndex() )
         params[0] = MODEL_ENC_FLAG_ENC_VIDEO | MODEL_ENC_FLAG_ENC_DATA;
      if ( 4 == m_pItemsSelect[2]->getSelectedIndex() )
         params[0] = MODEL_ENC_FLAG_ENC_ALL;

      if ( 0 != m_pItemsSelect[2]->getSelectedIndex() )
      {
         char szFile[128];
         strcpy(szFile, FOLDER_CONFIG);
         strcat(szFile, FILE_CONFIG_ENCRYPTION_PASS);
         FILE* fd = fopen(szFile, "r");
         if ( NULL == fd )
            return;
         len = fread(&params[2], 1, 124, fd);
         fclose(fd);
      }
      params[1] = (int)len;
      log_line("Send e type: %d, len: %d", (int)params[0], (int)params[1]);

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_ENCRYPTION_PARAMS, 0, params, len+2) )
         valuesToUI();
      return;
   }

   if ( m_IndexPower == m_SelectedIndex )
   {
      MenuTXInfo* pMenu = new MenuTXInfo();
      pMenu->m_bShowController = false;
      pMenu->m_bShowVehicle = true;
      add_menu_to_stack(pMenu);
      return;
   }

   for( int n=0; n<g_pCurrentModel->radioLinksParams.links_count; n++ )
   if ( m_IndexFreq[n] == m_SelectedIndex )
   {
      int index = m_pItemsSelect[20+n]->getSelectedIndex();
      u32 freq = m_SupportedChannels[n][index];
      int band = getBand(freq);      
      if ( freq == g_pCurrentModel->radioLinksParams.link_frequency_khz[n] )
         return;

      if ( link_is_reconfiguring_radiolink() )
      {
         add_menu_to_stack(new MenuConfirmation("Configuration In Progress","Another radio link configuration change is in progress. Please wait.", 0, true));
         valuesToUI();
         return;
      }

      u32 nicFreq[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         nicFreq[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i];

      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(n);
      nicFreq[iRadioInterfaceId] = freq;

      int nicFlags[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         nicFlags[i] = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i];

      const char* szError = controller_validate_radio_settings( g_pCurrentModel, nicFreq, nicFlags, NULL, NULL, NULL);
      
         if ( NULL != szError && 0 != szError[0] )
         {
            log_line(szError);
            add_menu_to_stack(new MenuConfirmation("Invalid option",szError, 0, true));
            valuesToUI();
            return;
         }

         bool supportedOnController = false;
         bool allSupported = true;
         for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
         {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
            if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
               continue;
            if ( band & pRadioHWInfo->supportedBands )
               supportedOnController = true;
            else
               allSupported = false;
         }
         if ( ! supportedOnController )
         {
            char szBuff[128];
            sprintf(szBuff, "%s frequency is not supported by your controller.", str_format_frequency(freq));
            add_menu_to_stack(new MenuConfirmation("Invalid option",szBuff, 0, true));
            valuesToUI();
            return;
         }
         if ( (! allSupported) && (1 == g_pCurrentModel->radioLinksParams.links_count) )
         {
            char szBuff[256];
            sprintf(szBuff, "Not all radio interfaces on your controller support %s frequency. Some radio interfaces on the controller will not be used to communicate with this vehicle.", str_format_frequency(freq));
            add_menu_to_stack(new MenuConfirmation("Confirmation",szBuff, 0, true));
         }

      if ( (get_sw_version_major(g_pCurrentModel->sw_version) < 9) ||
           ((get_sw_version_major(g_pCurrentModel->sw_version) == 9) && (get_sw_version_minor(g_pCurrentModel->sw_version) <= 20)) )
      {
         addMessageWithTitle(0, "Can't update radio links", "You need to update your vehicle to version 9.2 or newer");
         return;
      }

      sendNewRadioLinkFrequency(n, freq);
   }

   /*
   for( int n=0; n<g_pCurrentModel->radioLinksParams.links_count; n++ )
   if ( m_IndexConfigureLinks[n] == m_SelectedIndex )
   {
      Menu* pMenu = NULL;
      if ( g_pCurrentModel->radioLinkIsSiKRadio(n) )
         pMenu = new MenuVehicleRadioLinkSiK(n);
      else if ( g_pCurrentModel->radioLinkIsELRSRadio(n) )
         pMenu = new MenuVehicleRadioLinkELRS(n);
      else
         pMenu = new MenuVehicleRadioLink(n);

      add_menu_to_stack(pMenu);
   }
   */

   if ( m_IndexRadioConfig == m_SelectedIndex )
     add_menu_to_stack(new MenuRadioConfig());
}