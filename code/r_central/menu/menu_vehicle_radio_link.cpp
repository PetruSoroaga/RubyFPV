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
#include "menu_vehicle_radio_link.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_tx_power.h"
#include "menu_confirmation.h"
#include "../launchers_controller.h"
#include "../link_watch.h"
#include "../../radio/radiolink.h"

const char* s_szMenuRadio_SingleCard2 = "Note: You can not change the usage and capabilities of the radio link as there is a single radio link between vehicle and controller.";

MenuVehicleRadioLink::MenuVehicleRadioLink(int iRadioLink)
:Menu(MENU_ID_VEHICLE_RADIO_LINK, "Vehicle Radio Link Parameters", NULL)
{
   m_Width = 0.36;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.1;
   m_iRadioLink = iRadioLink;

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      log_line("Opening menu for relay radio link %d ...", m_iRadioLink+1);
   else
      log_line("Opening menu for radio link %d ...", m_iRadioLink+1);
    
   char szBuff[256];

   sprintf(szBuff, "Vehicle Radio Link %d Parameters", m_iRadioLink+1);
   setTitle(szBuff);

   for( int i=0; i<20; i++ )
      m_pItemsSelect[i] = NULL;

   char szBands[128];
   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);

   if ( -1 == iRadioInterfaceId )
   {
      log_softerror_and_alarm("Invalid radio link. No radio interfaces assigned to radio link %d.", m_iRadioLink+1);
      m_IndexStart = -1;
      return;
   }

   log_line("Opened menu to configure radio link %d, used by radio interface %d on the model side.", m_iRadioLink+1, iRadioInterfaceId+1);

   m_IndexStart = 0;
   str_get_supported_bands_string(g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], szBands);
   
   u32 uControllerSupportedBands = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         continue;
      uControllerSupportedBands |= pRadioHWInfo->supportedBands;
   }
      
   m_SupportedChannelsCount = getSupportedChannels( uControllerSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 1, &(m_SupportedChannels[0]), MAX_MENU_CHANNELS);

   sprintf(szBuff, "Vehicle radio interface used for this radio link: %s", str_get_radio_card_model_string(g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceId]));
   
   addMenuItem(new MenuItemText(szBuff, false, 0.0));
   addSection("General");

   m_pItemsSelect[00] = new MenuItemSelect("Frequency");

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      sprintf(szBuff,"Relay %s", str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequencyKhz));
      m_pItemsSelect[00]->addSelection(szBuff);
   }
   else
   {
      Preferences* pP = get_Preferences();
      int iCountChCurrentColumn = 0;
      for( int ch=0; ch<m_SupportedChannelsCount; ch++ )
      {
         if ( (pP->iScaleMenus > 0) && (iCountChCurrentColumn > 14 ) )
         {
            m_pItemsSelect[00]->addSeparator();
            iCountChCurrentColumn = 0;
         }
         if ( m_SupportedChannels[ch] == 0 )
         {
            m_pItemsSelect[00]->addSeparator();
            iCountChCurrentColumn = 0;
            continue;
         }
         strcpy(szBuff, str_format_frequency(m_SupportedChannels[ch]));
         m_pItemsSelect[00]->addSelection(szBuff);
         iCountChCurrentColumn++;
      }

      m_SupportedChannelsCount = getSupportedChannels( uControllerSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 0, &(m_SupportedChannels[0]), MAX_MENU_CHANNELS);
      if ( 0 == m_SupportedChannelsCount )
      {
         sprintf(szBuff,"%s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency_khz[m_iRadioLink]));
         m_pItemsSelect[00]->addSelection(szBuff);
         m_pItemsSelect[00]->setSelectedIndex(0);
         m_pItemsSelect[00]->setEnabled(false);
      }
   }

   m_pItemsSelect[00]->setIsEditable();
   m_IndexFrequency = addMenuItem(m_pItemsSelect[00]);

   m_pItemsSelect[01] = new MenuItemSelect("Usage", "Selects what type of data gets sent/received on this radio link, or disable it.");
   m_pItemsSelect[01]->addSelection("Disabled");
   m_pItemsSelect[01]->addSelection("Video & Data");
   m_pItemsSelect[01]->addSelection("Video");
   m_pItemsSelect[01]->addSelection("Data");
   m_pItemsSelect[01]->setIsEditable();
   m_IndexUsage = addMenuItem(m_pItemsSelect[01]);

   m_pItemsSelect[02] = new MenuItemSelect("Capabilities", "Sets the uplink/downlink capabilities of this radio link. If the associated vehicle radio interface has attached an external LNA or a unidirectional booster, it can't be used for both uplink and downlink, it must be marked to be used only for uplink or downlink accordingly.");  
   m_pItemsSelect[02]->addSelection("Uplink & Downlink");
   m_pItemsSelect[02]->addSelection("Downlink Only");
   m_pItemsSelect[02]->addSelection("Uplink Only");
   m_pItemsSelect[02]->setIsEditable();
   m_IndexCapabilities = addMenuItem(m_pItemsSelect[02]);

   if ( 1 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
      addMenuItem( new MenuItemText(s_szMenuRadio_SingleCard2, true));

   addSection("Radio Data Rates");

   m_pItemsSelect[03] = new MenuItemSelect("Radio Data Rate (for Video)", "Sets the physical radio data rate to use on this radio link for video data. If adaptive radio links is enabled, this will get lowered automatically by Ruby as needed.");
   for( int i=0; i<getDataRatesCount(); i++ )
   {
      str_getDataRateDescription(getDataRatesBPS()[i], 0, szBuff);
      m_pItemsSelect[03]->addSelection(szBuff);
   }
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
   {
      str_getDataRateDescription(-1-i, 0, szBuff);
      m_pItemsSelect[03]->addSelection(szBuff);
   }
   m_pItemsSelect[03]->setIsEditable();
   m_IndexDataRateVideo = addMenuItem(m_pItemsSelect[03]);

   m_pItemsSelect[04] = new MenuItemSelect("Radio Data Rate (for data downlink)", "Sets the physical radio downlink data rate for data packets (excluding video packets).");  
   m_pItemsSelect[04]->addSelection("Same as Video Data Rate");
   m_pItemsSelect[04]->addSelection("Fixed");
   m_pItemsSelect[04]->addSelection("Lowest");
   m_pItemsSelect[04]->setIsEditable();
   m_IndexDataRateTypeDownlink = addMenuItem(m_pItemsSelect[04]);

   m_pItemsSelect[05] = new MenuItemSelect("   Fixed Radio Data Rate (data downlink)", "Sets the physical radio downlink data rate for data packets (excluding video packets).");
   for( int i=0; i<getDataRatesCount(); i++ )
   {
      str_getDataRateDescription(getDataRatesBPS()[i], 0, szBuff);
      m_pItemsSelect[05]->addSelection(szBuff);
   }
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
   {
      str_getDataRateDescription(-1-i, 0, szBuff);
      m_pItemsSelect[05]->addSelection(szBuff);
   }
   m_pItemsSelect[05]->setIsEditable();
   m_IndexDataRateDataDownlink = addMenuItem(m_pItemsSelect[05]);

   m_pItemsSelect[06] = new MenuItemSelect("Radio Data Rate (for data uplink)", "Sets the physical radio uplink data rate for data packets.");  
   m_pItemsSelect[06]->addSelection("Same as Video Data Rate");
   m_pItemsSelect[06]->addSelection("Fixed");
   m_pItemsSelect[06]->addSelection("Lowest");
   m_pItemsSelect[06]->setIsEditable();
   m_IndexDataRateTypeUplink = addMenuItem(m_pItemsSelect[06]);

   m_pItemsSelect[07] = new MenuItemSelect("   Fixed Radio Data Rate (data uplink)", "Sets the physical radio uplink data rate for data packets.");
   for( int i=0; i<getDataRatesCount(); i++ )
   {
      str_getDataRateDescription(getDataRatesBPS()[i], 0, szBuff);
      m_pItemsSelect[07]->addSelection(szBuff);
   }
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
   {
      str_getDataRateDescription(-1-i, 0, szBuff);
      m_pItemsSelect[07]->addSelection(szBuff);
   }
   m_pItemsSelect[07]->setIsEditable();
   m_IndexDataRateDataUplink = addMenuItem(m_pItemsSelect[07]);

   addSection("Radio Link Flags");

   m_pItemsSelect[11] = new MenuItemSelect("Channel Bandwidth", "Sets the radio channel bandwidth when using MCS data rates.");
   m_pItemsSelect[11]->addSelection("20 Mhz");
   m_pItemsSelect[11]->addSelection("40 Mhz Downlink");
   m_pItemsSelect[11]->addSelection("40 Mhz Uplink");
   m_pItemsSelect[11]->addSelection("40 Mhz Both Directions");
   m_pItemsSelect[11]->setIsEditable();
   m_IndexHT = addMenuItem(m_pItemsSelect[11]);

   m_pItemsSelect[12] = new MenuItemSelect("Enable LDPC", "Enables LDPC (Low Density Parity Check) when using MCS data rates.");
   m_pItemsSelect[12]->addSelection("No");
   m_pItemsSelect[12]->addSelection("Vehicle Downlink");
   m_pItemsSelect[12]->addSelection("Controller Uplink");
   m_pItemsSelect[12]->addSelection("Both Directions");
   m_pItemsSelect[12]->setIsEditable();
   m_IndexLDPC = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSelect[13] = new MenuItemSelect("Enable SGI", "Enables SGI (Short Guard Interval) when using MCS data rates.");
   m_pItemsSelect[13]->addSelection("No");
   m_pItemsSelect[13]->addSelection("Vehicle Downlink");
   m_pItemsSelect[13]->addSelection("Controller Uplink");
   m_pItemsSelect[13]->addSelection("Both Directions");
   m_pItemsSelect[13]->setIsEditable();
   m_IndexSGI = addMenuItem(m_pItemsSelect[13]);

   m_pItemsSelect[14] = new MenuItemSelect("Enable STBC", "Enables STBC when using MCS data rates.");
   m_pItemsSelect[14]->addSelection("No");
   m_pItemsSelect[14]->addSelection("Vehicle Downlink");
   m_pItemsSelect[14]->addSelection("Controller Uplink");
   m_pItemsSelect[14]->addSelection("Both Directions");
   m_pItemsSelect[14]->setIsEditable();
   m_IndexSTBC = addMenuItem(m_pItemsSelect[14]);


   m_IndexReset = addMenuItem(new MenuItem("Reset To Default", "Resets this radio link parameters to default values."));
   log_line("Created radio link menu for radio link %d", m_iRadioLink+1);
}

MenuVehicleRadioLink::~MenuVehicleRadioLink()
{
}

void MenuVehicleRadioLink::onShow()
{
   valuesToUI();
   Menu::onShow();
   invalidate();

   log_line("Showed radio link menu for radio link %d", m_iRadioLink+1);
}

void MenuVehicleRadioLink::valuesToUI()
{
   bool bDisableAll = false;
   if ( -1 == m_IndexStart )
      bDisableAll = true;
   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);
   if ( -1 == iRadioInterfaceId )
      bDisableAll = true;

   log_line("MenuVehicleRadioLink: Update values for radio link %d", m_iRadioLink+1);

   if ( bDisableAll )
   {
      for( int i=1; i<20; i++ )
         if ( NULL != m_pItemsSelect[i] )
            m_pItemsSelect[i]->setEnabled(false);
      return;
   }

   u32 linkCapabilitiesFlags = g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink];
   u32 linkRadioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[m_iRadioLink];

   // Frequency

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      m_pItemsSelect[00]->setEnabled(false);
   else
      m_pItemsSelect[00]->setEnabled(true);

   int selectedIndex = 0;
   for( int ch=0; ch<m_SupportedChannelsCount; ch++ )
   {
      if ( g_pCurrentModel->radioLinksParams.link_frequency_khz[m_iRadioLink] == m_SupportedChannels[ch] )
         break;
      selectedIndex++;
   }

   m_pItemsSelect[00]->setSelection(selectedIndex);

   // Capabilities

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      m_pItemsSelect[01]->setEnabled(false);
   else
      m_pItemsSelect[01]->setEnabled(true);

   m_pItemsSelect[01]->setSelection(0); // Disabled

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      m_pItemsSelect[01]->setSelection(1);

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   if ( !(linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
      m_pItemsSelect[01]->setSelection(3);

   if ( !(linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      m_pItemsSelect[01]->setSelection(2);

   if ( (1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count) )
      m_pItemsSelect[01]->setSelection(1);

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      m_pItemsSelect[01]->setSelection(0);

   if ( (1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count) )
      m_pItemsSelect[01]->setEnabled(false);

   if ( (g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY) ||
        (linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
   {
      for( int i=2; i<20; i++ )
         if ( NULL != m_pItemsSelect[i] )
            m_pItemsSelect[i]->setEnabled(false);
      m_pItemsSelect[0]->setEnabled(false);
   }
   else
   {
      for( int i=2; i<20; i++ )
         if ( NULL != m_pItemsSelect[i]) 
            m_pItemsSelect[i]->setEnabled(true);
      m_pItemsSelect[0]->setEnabled(true);
   }

   // Usage

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      m_pItemsSelect[02]->setSelection(0);

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
   if ( !(linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      m_pItemsSelect[02]->setSelection(2);

   if ( linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( !(linkCapabilitiesFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      m_pItemsSelect[02]->setSelection(1);

   if ( (1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count) )
   {
      m_pItemsSelect[02]->setSelection(0);
      m_pItemsSelect[02]->setEnabled(false);
   }

   // Data rates

   selectedIndex = 0;
   for( int i=0; i<getDataRatesCount(); i++ )
      if ( g_pCurrentModel->radioLinksParams.link_datarate_video_bps[m_iRadioLink] == getDataRatesBPS()[i] )
         selectedIndex = i;
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
      if ( g_pCurrentModel->radioLinksParams.link_datarate_video_bps[m_iRadioLink] == (-1-i) )
         selectedIndex = i+getDataRatesCount();

   m_pItemsSelect[03]->setSelection(selectedIndex);

   selectedIndex = 0;
   for( int i=0; i<getDataRatesCount(); i++ )
      if ( g_pCurrentModel->radioLinksParams.link_datarate_data_bps[m_iRadioLink] == getDataRatesBPS()[i] )
         selectedIndex = i;
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
      if ( g_pCurrentModel->radioLinksParams.link_datarate_data_bps[m_iRadioLink] == (-1-i) )
         selectedIndex = i+getDataRatesCount();

   m_pItemsSelect[05]->setSelection(selectedIndex);

   selectedIndex = 0;
   for( int i=0; i<getDataRatesCount(); i++ )
      if ( g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iRadioLink] == getDataRatesBPS()[i] )
         selectedIndex = i;
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
      if ( g_pCurrentModel->radioLinksParams.uplink_datarate_data_bps[m_iRadioLink] == (-1-i) )
         selectedIndex = i+getDataRatesCount();

   m_pItemsSelect[07]->setSelection(selectedIndex);

   // Fixed data rates

   m_pItemsSelect[04]->setSelectedIndex(2);
   if ( g_pCurrentModel->radioLinksParams.uDownlinkDataDataRateType[m_iRadioLink] == FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO )
   {
      m_pItemsSelect[04]->setSelectedIndex(0);
      m_pItemsSelect[05]->setEnabled(false);
      m_pItemsSelect[05]->setSelectedIndex(m_pItemsSelect[03]->getSelectedIndex());
   }
   if ( g_pCurrentModel->radioLinksParams.uDownlinkDataDataRateType[m_iRadioLink] == FLAG_RADIO_LINK_DATARATE_DATA_TYPE_FIXED )
   {
      m_pItemsSelect[04]->setSelectedIndex(1);
   }
   if ( g_pCurrentModel->radioLinksParams.uDownlinkDataDataRateType[m_iRadioLink] == FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST )
   {
      m_pItemsSelect[04]->setSelectedIndex(2);
      m_pItemsSelect[05]->setEnabled(false);
      selectedIndex = 0;
      for( int i=0; i<getDataRatesCount(); i++ )
         if ( DEFAULT_RADIO_DATARATE_LOWEST == getDataRatesBPS()[i] )
            selectedIndex = i;

      if ( g_pCurrentModel->radioLinksParams.link_datarate_video_bps[m_iRadioLink] < 0 )
          selectedIndex = getDataRatesCount();
      m_pItemsSelect[05]->setSelectedIndex(selectedIndex);
   }


   m_pItemsSelect[06]->setSelectedIndex(2);
   if ( g_pCurrentModel->radioLinksParams.uUplinkDataDataRateType[m_iRadioLink] == FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO )
   {
      m_pItemsSelect[06]->setSelectedIndex(0);
      m_pItemsSelect[07]->setEnabled(false);
      m_pItemsSelect[07]->setSelectedIndex(m_pItemsSelect[03]->getSelectedIndex());
   }
   if ( g_pCurrentModel->radioLinksParams.uUplinkDataDataRateType[m_iRadioLink] == FLAG_RADIO_LINK_DATARATE_DATA_TYPE_FIXED )
   {
      m_pItemsSelect[06]->setSelectedIndex(1);
   }
   if ( g_pCurrentModel->radioLinksParams.uUplinkDataDataRateType[m_iRadioLink] == FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST )
   {
      m_pItemsSelect[06]->setSelectedIndex(2);
      m_pItemsSelect[07]->setEnabled(false);
      
      selectedIndex = 0;
      for( int i=0; i<getDataRatesCount(); i++ )
         if ( DEFAULT_RADIO_DATARATE_LOWEST == getDataRatesBPS()[i] )
            selectedIndex = i;
      if ( g_pCurrentModel->radioLinksParams.link_datarate_video_bps[m_iRadioLink] < 0 )
          selectedIndex = getDataRatesCount();
      m_pItemsSelect[07]->setSelectedIndex(selectedIndex);
   }

   // MCS flags

   if ( (linkRadioFlags & RADIO_FLAG_HT40_VEHICLE) && (linkRadioFlags & RADIO_FLAG_HT40_CONTROLLER) )
      m_pItemsSelect[11]->setSelectedIndex(3);
   else if ( linkRadioFlags & RADIO_FLAG_HT40_VEHICLE )
      m_pItemsSelect[11]->setSelectedIndex(1);
   else if ( linkRadioFlags & RADIO_FLAG_HT40_CONTROLLER )
      m_pItemsSelect[11]->setSelectedIndex(2);
   else
      m_pItemsSelect[11]->setSelectedIndex(0);

   if ( (linkRadioFlags & RADIO_FLAG_LDPC_VEHICLE) && (linkRadioFlags & RADIO_FLAG_LDPC_CONTROLLER) )
      m_pItemsSelect[12]->setSelectedIndex(3);
   else if ( linkRadioFlags & RADIO_FLAG_LDPC_VEHICLE )
      m_pItemsSelect[12]->setSelectedIndex(1);
   else if ( linkRadioFlags & RADIO_FLAG_LDPC_CONTROLLER )
      m_pItemsSelect[12]->setSelectedIndex(2);
   else
      m_pItemsSelect[12]->setSelectedIndex(0);

   if ( (linkRadioFlags & RADIO_FLAG_SGI_VEHICLE) && (linkRadioFlags & RADIO_FLAG_SGI_CONTROLLER) )
      m_pItemsSelect[13]->setSelectedIndex(3);
   else if ( linkRadioFlags & RADIO_FLAG_SGI_VEHICLE )
      m_pItemsSelect[13]->setSelectedIndex(1);
   else if ( linkRadioFlags & RADIO_FLAG_SGI_CONTROLLER )
      m_pItemsSelect[13]->setSelectedIndex(2);
   else
      m_pItemsSelect[13]->setSelectedIndex(0);

   if ( (linkRadioFlags & RADIO_FLAG_STBC_VEHICLE) && (linkRadioFlags & RADIO_FLAG_STBC_CONTROLLER) )
      m_pItemsSelect[14]->setSelectedIndex(3);
   else if ( linkRadioFlags & RADIO_FLAG_STBC_VEHICLE )
      m_pItemsSelect[14]->setSelectedIndex(1);
   else if ( linkRadioFlags & RADIO_FLAG_STBC_CONTROLLER )
      m_pItemsSelect[14]->setSelectedIndex(2);
   else
      m_pItemsSelect[14]->setSelectedIndex(0);

   char szCapab[256];
   char szFlags[256];
   str_get_radio_capabilities_description(linkCapabilitiesFlags, szCapab);
   str_get_radio_frame_flags_description(linkRadioFlags, szFlags);
   log_line("MenuVehicleRadioLink: Update UI: Current radio link %d capabilities: %s, flags: %s.", m_iRadioLink+1, szCapab, szFlags);

}

void MenuVehicleRadioLink::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
   }
   RenderEnd(yTop);
}

void MenuVehicleRadioLink::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
}

void MenuVehicleRadioLink::sendRadioLinkCapabilities(int iRadioLink)
{
   int usage = m_pItemsSelect[01]->getSelectedIndex();
   int capab = m_pItemsSelect[02]->getSelectedIndex();

   u32 link_capabilities = g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink];

   link_capabilities = link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
   link_capabilities = link_capabilities & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA));

   link_capabilities = link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
   link_capabilities = link_capabilities & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA));
   
   if ( 0 == usage ) link_capabilities = link_capabilities | RADIO_HW_CAPABILITY_FLAG_DISABLED;
   if ( 1 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);
   if ( 2 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
   if ( 3 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);

   if ( 0 == capab ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX);
   if ( 1 == capab ) link_capabilities = (link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_CAN_RX)) | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   if ( 2 == capab ) link_capabilities = (link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_CAN_TX)) | RADIO_HW_CAPABILITY_FLAG_CAN_RX;

   char szBuffC1[128];
   char szBuffC2[128];
   str_get_radio_capabilities_description(g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink], szBuffC1);
   str_get_radio_capabilities_description(link_capabilities, szBuffC2);

   log_line("MenuVehicleRadioLink: On update link capabilities:");
   log_line("MenuVehicleRadioLink: Radio link current capabilities: %s", szBuffC1);
   log_line("MenuVehicleRadioLink: Radio link new requested capabilities: %s", szBuffC2);

   if ( link_capabilities == g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] )
      return;

   log_line("Sending new link capabilities: %d for radio link %d", link_capabilities, iRadioLink+1);
   u32 param = (((u32)iRadioLink) & 0xFF) | (link_capabilities<<8);
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_CAPABILITIES, param, NULL, 0) )
      valuesToUI();
}

void MenuVehicleRadioLink::sendRadioLinkConfig(int iRadioLink)
{
   if ( (get_sw_version_major(g_pCurrentModel->sw_version) < 9) ||
        ((get_sw_version_major(g_pCurrentModel->sw_version) == 9) && (get_sw_version_minor(g_pCurrentModel->sw_version) <= 20)) )
   {
      addMessageWithTitle(0, "Can't update radio links", "You need to update your vehicle to version 9.2 or newer");
      return;
   }

   type_radio_links_parameters newRadioLinkParams;
   memcpy((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   
   int usage = m_pItemsSelect[01]->getSelectedIndex();
   int capab = m_pItemsSelect[02]->getSelectedIndex();

   u32 link_capabilities = g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink];

   link_capabilities = link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
   link_capabilities = link_capabilities & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA));
   
   if ( 0 == usage ) link_capabilities = link_capabilities | RADIO_HW_CAPABILITY_FLAG_DISABLED;
   if ( 1 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);
   if ( 2 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
   if ( 3 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);

   if ( 0 == capab ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX);
   if ( 1 == capab ) link_capabilities = (link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_CAN_RX)) | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   if ( 2 == capab ) link_capabilities = (link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_CAN_TX)) | RADIO_HW_CAPABILITY_FLAG_CAN_RX;
     
   newRadioLinkParams.link_capabilities_flags[iRadioLink] = link_capabilities;

   int indexRate = m_pItemsSelect[03]->getSelectedIndex();

   if ( indexRate < getDataRatesCount() )
      newRadioLinkParams.link_datarate_video_bps[iRadioLink] = getDataRatesBPS()[indexRate];
   else
      newRadioLinkParams.link_datarate_video_bps[iRadioLink] = -1-(indexRate - getDataRatesCount());
   
   if ( 0 == m_pItemsSelect[04]->getSelectedIndex() )
      newRadioLinkParams.uDownlinkDataDataRateType[iRadioLink] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO;
   if ( 1 == m_pItemsSelect[04]->getSelectedIndex() )
      newRadioLinkParams.uDownlinkDataDataRateType[iRadioLink] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_FIXED;
   if ( 2 == m_pItemsSelect[04]->getSelectedIndex() )
      newRadioLinkParams.uDownlinkDataDataRateType[iRadioLink] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST;
   
   if ( 0 == m_pItemsSelect[06]->getSelectedIndex() )
      newRadioLinkParams.uUplinkDataDataRateType[iRadioLink] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_SAME_AS_ADAPTIVE_VIDEO;
   if ( 1 == m_pItemsSelect[06]->getSelectedIndex() )
      newRadioLinkParams.uUplinkDataDataRateType[iRadioLink] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_FIXED;
   if ( 2 == m_pItemsSelect[06]->getSelectedIndex() )
      newRadioLinkParams.uUplinkDataDataRateType[iRadioLink] = FLAG_RADIO_LINK_DATARATE_DATA_TYPE_LOWEST;
 
   if ( m_pItemsSelect[05]->isEnabled() )
   {
      indexRate = m_pItemsSelect[05]->getSelectedIndex();
      if ( indexRate < getDataRatesCount() )
         newRadioLinkParams.link_datarate_data_bps[iRadioLink] = getDataRatesBPS()[indexRate];
      else
         newRadioLinkParams.link_datarate_data_bps[iRadioLink] = -1-(indexRate - getDataRatesCount());
   }
   if ( m_pItemsSelect[07]->isEnabled() )
   {
      indexRate = m_pItemsSelect[07]->getSelectedIndex();
      if ( indexRate < getDataRatesCount() )
         newRadioLinkParams.uplink_datarate_data_bps[iRadioLink] = getDataRatesBPS()[indexRate];
      else
         newRadioLinkParams.uplink_datarate_data_bps[iRadioLink] = -1-(indexRate - getDataRatesCount());
   }

   u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[iRadioLink];

   // Clear all MCS flags
   radioFlags &= 0xFF00FFFF;

   if ( 1 == m_pItemsSelect[11]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_HT40_VEHICLE;
   else if ( 2 == m_pItemsSelect[11]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_HT40_CONTROLLER;
   else if ( 3 == m_pItemsSelect[11]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_HT40_VEHICLE | RADIO_FLAG_HT40_CONTROLLER;
  
   if ( 1 == m_pItemsSelect[12]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_LDPC_VEHICLE;
   else if ( 2 == m_pItemsSelect[12]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_LDPC_CONTROLLER;
   else if ( 3 == m_pItemsSelect[12]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_LDPC_VEHICLE | RADIO_FLAG_LDPC_CONTROLLER;
  
   if ( 1 == m_pItemsSelect[13]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_SGI_VEHICLE;
   else if ( 2 == m_pItemsSelect[13]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_SGI_CONTROLLER;
   else if ( 3 == m_pItemsSelect[13]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_SGI_VEHICLE | RADIO_FLAG_SGI_CONTROLLER;
  
   if ( 1 == m_pItemsSelect[14]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_STBC_VEHICLE;
   else if ( 2 == m_pItemsSelect[14]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_STBC_CONTROLLER;
   else if ( 3 == m_pItemsSelect[14]->getSelectedIndex() )
      radioFlags |= RADIO_FLAG_STBC_VEHICLE | RADIO_FLAG_STBC_CONTROLLER;
  
   newRadioLinkParams.link_radio_flags[iRadioLink] = radioFlags;

   if ( 0 == memcmp((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters)) )
   {
      log_line("MenuVehicleRadioLink: No change in radio link config. Do not send command.");
      return;
   }

   char szBuff[256];
   str_get_radio_capabilities_description(newRadioLinkParams.link_capabilities_flags[iRadioLink], szBuff);
 
   log_line("MenuVehicleRadioLink: Radio link new requested capabilities: %s", szBuff);

   log_line("MenuVehicleRadioLink: Sending new radio data rates for link %d: v: %d, d: %d/%d, u: %d/%d",
      iRadioLink, newRadioLinkParams.link_datarate_video_bps[iRadioLink],
      newRadioLinkParams.uDownlinkDataDataRateType[iRadioLink],
      newRadioLinkParams.link_datarate_data_bps[iRadioLink],
      newRadioLinkParams.uUplinkDataDataRateType[iRadioLink],
      newRadioLinkParams.uplink_datarate_data_bps[iRadioLink]);

   t_packet_header PH;
   int iHeaderSize = 5;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + iHeaderSize + sizeof(type_radio_links_parameters);

   u8 buffer[1024];
   static int s_iMenuRadioLinkTestNumberCount = 0;
   s_iMenuRadioLinkTestNumberCount++;
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));

   buffer[sizeof(t_packet_header)] = 1;
   buffer[sizeof(t_packet_header)+1] = iHeaderSize;
   buffer[sizeof(t_packet_header)+2] = (u8)iRadioLink;
   buffer[sizeof(t_packet_header)+3] = (u8)s_iMenuRadioLinkTestNumberCount;
   buffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(buffer + sizeof(t_packet_header) + iHeaderSize, &newRadioLinkParams, sizeof(type_radio_links_parameters));

   send_packet_to_router(buffer, PH.total_length);

   link_set_is_reconfiguring_radiolink(m_iRadioLink);
   warnings_add_configuring_radio_link(m_iRadioLink, "Updating Radio Link");
}

void MenuVehicleRadioLink::sendNewRadioLinkFrequency(int iVehicleLinkIndex, u32 uNewFreqKhz)
{
   if ( (iVehicleLinkIndex < 0) || (iVehicleLinkIndex >= MAX_RADIO_INTERFACES) )
      return;

   log_line("MenuVehicleRadioLink: Changing radio link %d frequency to %u khz (%s)", iVehicleLinkIndex+1, uNewFreqKhz, str_format_frequency(uNewFreqKhz));

   type_radio_links_parameters newRadioLinkParams;
   memcpy((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   
   newRadioLinkParams.link_frequency_khz[iVehicleLinkIndex] = uNewFreqKhz;
   
   if ( 0 == memcmp((u8*)&newRadioLinkParams, (u8*)&(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters)) )
   {
      log_line("MenuVehicleRadioLink: No change in radio link frequency. Do not send command.");
      return;
   }

   t_packet_header PH;
   int iHeaderSize = 5;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_TEST_RADIO_LINK, STREAM_ID_DATA);
   PH.vehicle_id_src = PACKET_COMPONENT_RUBY;
   PH.vehicle_id_dest = g_pCurrentModel->uVehicleId;
   PH.total_length = sizeof(t_packet_header) + iHeaderSize + sizeof(type_radio_links_parameters);

   u8 buffer[1024];
   static int s_iMenuRadioLinkTestNumberCount2 = 0;
   s_iMenuRadioLinkTestNumberCount2++;
   memcpy(buffer, (u8*)&PH, sizeof(t_packet_header));

   buffer[sizeof(t_packet_header)] = 1;
   buffer[sizeof(t_packet_header)+1] = iHeaderSize;
   buffer[sizeof(t_packet_header)+2] = (u8)iVehicleLinkIndex;
   buffer[sizeof(t_packet_header)+3] = (u8)s_iMenuRadioLinkTestNumberCount2;
   buffer[sizeof(t_packet_header)+4] = PACKET_TYPE_TEST_RADIO_LINK_COMMAND_START;
   memcpy(buffer + sizeof(t_packet_header) + iHeaderSize, &newRadioLinkParams, sizeof(type_radio_links_parameters));

   send_packet_to_router(buffer, PH.total_length);

   link_set_is_reconfiguring_radiolink(iVehicleLinkIndex);
   warnings_add_configuring_radio_link(iVehicleLinkIndex, "Changing Frequency");
}

void MenuVehicleRadioLink::onSelectItem()
{
   Menu::onSelectItem();
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( link_is_reconfiguring_radiolink() )
   {
      handle_commands_show_popup_progress();
      valuesToUI();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   char szBuff[256];

   if ( -1 == m_IndexStart )
      return;

   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);
   log_line("MenuVehicleRadioLink: Current radio interface assigned to radio link %d: %d", m_iRadioLink+1, iRadioInterfaceId+1);
   if ( -1 == iRadioInterfaceId )
      return;

   if ( m_IndexFrequency == m_SelectedIndex )
   {
      int index = m_pItemsSelect[00]->getSelectedIndex();
      u32 freq = m_SupportedChannels[index];
      int band = getBand(freq);      
      if ( freq == g_pCurrentModel->radioLinksParams.link_frequency_khz[m_iRadioLink] )
         return;

      u32 nicFreq[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      {
         nicFreq[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i];
         log_line("Current frequency on card %d: %s", i+1, str_format_frequency(nicFreq[i]));
      }

      int nicFlags[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         nicFlags[i] = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i];

      nicFreq[m_iRadioLink] = freq;

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
      sendNewRadioLinkFrequency(m_iRadioLink, freq);
      return;
   }

   if ( m_IndexUsage == m_SelectedIndex )
   {
      int usage = m_pItemsSelect[01]->getSelectedIndex();

      u32 nicFreq[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         nicFreq[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[i];

      int nicFlags[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         nicFlags[i] = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i];

      nicFlags[m_iRadioLink] = nicFlags[m_iRadioLink] & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
      nicFlags[m_iRadioLink] = nicFlags[m_iRadioLink] & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA));
      if ( 0 == usage ) nicFlags[m_iRadioLink] = nicFlags[m_iRadioLink] | RADIO_HW_CAPABILITY_FLAG_DISABLED;
      if ( 1 == usage ) nicFlags[m_iRadioLink] = nicFlags[m_iRadioLink] | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);
      if ( 2 == usage ) nicFlags[m_iRadioLink] = nicFlags[m_iRadioLink] | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
      if ( 3 == usage ) nicFlags[m_iRadioLink] = nicFlags[m_iRadioLink] | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);

      const char* szError = controller_validate_radio_settings( g_pCurrentModel, nicFreq, nicFlags, NULL, NULL, NULL);
   
      if ( NULL != szError && 0 != szError[0] )
      {
         log_line(szError);
         add_menu_to_stack(new MenuConfirmation("Invalid option",szError, 0, true));
         valuesToUI();
         return;
      }

      sendRadioLinkCapabilities(m_iRadioLink);
      return;
   }

   if ( m_IndexCapabilities == m_SelectedIndex )
   {
      sendRadioLinkConfig(m_iRadioLink);
      return;
   }

   if ( m_IndexDataRateVideo == m_SelectedIndex ||
       m_IndexDataRateDataDownlink == m_SelectedIndex || m_IndexDataRateDataUplink == m_SelectedIndex ||
       m_IndexDataRateTypeDownlink == m_SelectedIndex || m_IndexDataRateTypeUplink == m_SelectedIndex )
   {
      sendRadioLinkConfig(m_iRadioLink);
      return;
   }

      
   if ( m_IndexSGI == m_SelectedIndex || m_IndexLDPC == m_SelectedIndex || m_IndexSTBC == m_SelectedIndex || m_IndexHT == m_SelectedIndex)
   {
      sendRadioLinkConfig(m_iRadioLink);
      return;
   }

   if ( m_IndexReset == m_SelectedIndex )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_RESET_RADIO_LINK, m_iRadioLink, NULL, 0) )
         valuesToUI();
   }
}