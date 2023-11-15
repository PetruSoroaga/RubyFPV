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
#include "../radio/radiolink.h"
#include "../base/ctrl_interfaces.h"
#include "../common/string_utils.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "menu.h"
#include "menu_vehicle_radio_link.h"
#include "menu_item_select.h"
#include "menu_item_section.h"
#include "menu_item_text.h"
#include "menu_txinfo.h"
#include "menu_confirmation.h"
#include "launchers_controller.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include "process_router_messages.h"
#include "osd_common.h"
#include "menu_txinfo.h"
#include "timers.h"

const char* s_szMenuRadio_SingleCard2 = "Note: You can not change the function and capabilities of the radio link as there is a single one between vehicle and controller.";

MenuVehicleRadioLink::MenuVehicleRadioLink(int iRadioLink)
:Menu(MENU_ID_VEHICLE_RADIO_LINK, "Vehicle Radio Link Parameters", NULL)
{
   m_Width = 0.3;
   m_xPos = 0.08;
   m_yPos = 0.1;
   m_iRadioLink = iRadioLink;

   m_IndexDatarateUseLowest = -1;
   
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
      log_line("Opening menu for relay radio link %d ...", m_iRadioLink);
   else
      log_line("Opening menu for radio link %d ...", m_iRadioLink);
    
   char szBuff[256];

   snprintf(szBuff, sizeof(szBuff), "Vehicle Radio Link %d Parameters", m_iRadioLink+1);
   setTitle(szBuff);

   char szBands[128];
   char szId[32];
   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);

   if ( -1 == iRadioInterfaceId )
   {
      log_softerror_and_alarm("Invalid radio link. No radio interfaces assigned to radio link %d.", m_iRadioLink+1);
      m_IndexStart = -1;
      return;
   }

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

   m_pItemsSelect[6] = new MenuItemSelect("Frequency");

   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      snprintf(szBuff, sizeof(szBuff),"Relay %s", str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequency));
      m_pItemsSelect[6]->addSelection(szBuff);
   }
   else
   {
      for( int ch=0; ch<m_SupportedChannelsCount; ch++ )
      {
         if ( m_SupportedChannels[ch] == -1 )
         {
            m_pItemsSelect[6]->addSeparator();
            continue;
         }
         strcpy(szBuff, str_format_frequency(m_SupportedChannels[ch]));
         m_pItemsSelect[6]->addSelection(szBuff);
      }
      m_SupportedChannelsCount = getSupportedChannels( uControllerSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 0, &(m_SupportedChannels[0]), MAX_MENU_CHANNELS);
      if ( 0 == m_SupportedChannelsCount )
      {
         snprintf(szBuff, sizeof(szBuff),"%s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[m_iRadioLink]));
         m_pItemsSelect[6]->addSelection(szBuff);
         m_pItemsSelect[6]->setSelectedIndex(0);
         m_pItemsSelect[6]->setEnabled(false);
      }
   }

   m_pItemsSelect[6]->setIsEditable();
   m_IndexFrequency = addMenuItem(m_pItemsSelect[6]);

   m_pItemsSelect[7] = new MenuItemSelect("Usage", "Selects what type of data gets sent/received on this radio link, or disable it.");
   m_pItemsSelect[7]->addSelection("Disabled");
   m_pItemsSelect[7]->addSelection("Video & Data");
   m_pItemsSelect[7]->addSelection("Video");
   m_pItemsSelect[7]->addSelection("Data");
   //m_pItemsSelect[7]->addSelection("Relaying");
   m_pItemsSelect[7]->setIsEditable();
   m_IndexUsage = addMenuItem(m_pItemsSelect[7]);

   m_pItemsSelect[8] = new MenuItemSelect("Capabilities", "Sets the uplink/downlink capabilities of this radio link. If the associated vehicle radio interface has attached an external LNA or a unidirectional booster, it can't be used for both uplink and downlink, it must be marked to be used only for uplink or downlink accordingly.");  
   m_pItemsSelect[8]->addSelection("Uplink & Downlink");
   m_pItemsSelect[8]->addSelection("Downlink Only");
   m_pItemsSelect[8]->addSelection("Uplink Only");
   m_pItemsSelect[8]->setIsEditable();
   m_IndexCapabilities = addMenuItem(m_pItemsSelect[8]);

   m_pItemsSelect[9] = new MenuItemSelect("Radio Data Rate", "Sets the physical radio data rate to use on this radio link. If adaptive radio links is enabled, this can get lowered automatically as needed.");
   for( int i=0; i<getDataRatesCount(); i++ )
   {
      snprintf(szBuff, sizeof(szBuff), "%d Mbps", getDataRates()[i]);
      m_pItemsSelect[9]->addSelection(szBuff);
   }
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
   {
      snprintf(szBuff, sizeof(szBuff), "MCS-%d (%u Mbps)", i, getRealDataRateFromMCSRate(i)/1000/1000);
      m_pItemsSelect[9]->addSelection(szBuff);
   }
   m_pItemsSelect[9]->setIsEditable();
   m_IndexDataRate= addMenuItem(m_pItemsSelect[9]);


   m_pItemsSelect[15] = new MenuItemSelect("Lower Data/Telemetry", "Use a lower radio datarate for sending/receiving telemetry data, commands and user defined data streams.");
   m_pItemsSelect[15]->addSelection("No");
   m_pItemsSelect[15]->addSelection("Yes");
   m_pItemsSelect[15]->setIsEditable();
   m_IndexDatarateUseLowest = addMenuItem(m_pItemsSelect[15]);
   
   //m_pItemsSelect[10*n+14] = new MenuItemSelect("Adaptive Datarates", "System will adjust the radio datarates based on realtime conditions of the link");
   //m_pItemsSelect[10*n+14]->addSelection("No");
   //m_pItemsSelect[10*n+14]->addSelection("Yes");
   //m_pItemsSelect[10*n+14]->setUseMultiViewLayout();
   //m_IndexDataRateAdaptive[n] = addMenuItem(m_pItemsSelect[10*n+14]);
   m_IndexDataRateAdaptive = -1;

   m_IndexMCSInfo = addMenuItem(new MenuItemText("MCS Data Rates Flags:"));

   m_pItemsSelect[10] = new MenuItemSelect("Apply MCS flags to", "When using MCS data rates, apply the MCS flags below to vehicle radio interfaces assigned to this radio link, to controller radio interfaces assigned to this radio link or to both.");
   m_pItemsSelect[10]->addSelection("Vehicle & Controller");
   m_pItemsSelect[10]->addSelection("Vehicle Only");
   m_pItemsSelect[10]->addSelection("Controller Only");
   m_pItemsSelect[10]->setIsEditable();
   m_IndexFlagsTarget = addMenuItem(m_pItemsSelect[10]);

   m_pItemsSelect[11] = new MenuItemSelect("Channel Bandwidth", "Sets the radio channel bandwidth when using MCS data rates.");
   m_pItemsSelect[11]->addSelection("20 Mhz");
   m_pItemsSelect[11]->addSelection("40 Mhz");
   m_pItemsSelect[11]->setUseMultiViewLayout();
   m_IndexHT = addMenuItem(m_pItemsSelect[11]);

   m_pItemsSelect[12] = new MenuItemSelect("Enable LDPC", "Enables LDPC (Low Density Parity Check) when using MCS data rates.");
   m_pItemsSelect[12]->addSelection("No");
   m_pItemsSelect[12]->addSelection("Yes");
   m_pItemsSelect[12]->setUseMultiViewLayout();
   m_IndexLDPC = addMenuItem(m_pItemsSelect[12]);

   m_pItemsSelect[13] = new MenuItemSelect("Enable SGI", "Enables SGI (Short Guard Interval) when using MCS data rates.");
   m_pItemsSelect[13]->addSelection("No");
   m_pItemsSelect[13]->addSelection("Yes");
   m_pItemsSelect[13]->setUseMultiViewLayout();
   m_IndexSGI = addMenuItem(m_pItemsSelect[13]);

   m_pItemsSelect[14] = new MenuItemSelect("Enable STBC", "Enables STBC when using MCS data rates.");
   m_pItemsSelect[14]->addSelection("No");
   m_pItemsSelect[14]->addSelection("Yes");
   m_pItemsSelect[14]->setUseMultiViewLayout();
   m_IndexSTBC = addMenuItem(m_pItemsSelect[14]);
}

MenuVehicleRadioLink::~MenuVehicleRadioLink()
{
}

void MenuVehicleRadioLink::onShow()
{
   valuesToUI();
   Menu::onShow();

   m_ExtraHeight = MENU_ITEM_SPACING * menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS;

   if ( 1 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
      m_ExtraHeight += 1.4*g_pRenderEngine->getMessageHeight(s_szMenuRadio_SingleCard2, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth()-MENU_MARGINS*menu_getScaleMenus(), g_idFontMenu);

   invalidate();
}

void MenuVehicleRadioLink::valuesToUI()
{
   if ( -1 == m_IndexStart )
      return;
 
   if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink] & RADIO_HW_CAPABILITY_FLAG_USED_FOR_RELAY )
   {
      m_pItemsSelect[6]->setEnabled(false);
      m_pItemsSelect[7]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[6]->setEnabled(true);
      m_pItemsSelect[7]->setEnabled(true);    
   }

   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);
   if ( -1 == iRadioInterfaceId )
      return;

   
   int selectedIndex = 0;
   for( int ch=0; ch<m_SupportedChannelsCount; ch++ )
   {
      if ( getFrequencyInKHz(g_pCurrentModel->radioLinksParams.link_frequency[m_iRadioLink]) == getFrequencyInKHz(m_SupportedChannels[ch]))
         break;
      selectedIndex++;
   }

   m_pItemsSelect[6]->setSelection(selectedIndex);

   u32 linkFlags = g_pCurrentModel->radioLinksParams.link_capabilities_flags[m_iRadioLink];
   u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[m_iRadioLink];

   m_pItemsSelect[7]->setSelection(0); // Disabled

   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      m_pItemsSelect[7]->setSelection(1);

   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
   if ( !(linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
      m_pItemsSelect[7]->setSelection(3);

   if ( !(linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      m_pItemsSelect[7]->setSelection(2);

   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
   {
      m_pItemsSelect[7]->setSelection(0);
      m_pItemsSelect[8]->setEnabled(false);
      m_pItemsSelect[9]->setEnabled(false);
      m_pItemsSelect[10]->setEnabled(false);
      m_pItemsSelect[11]->setEnabled(false);
      m_pItemsSelect[12]->setEnabled(false);
      m_pItemsSelect[13]->setEnabled(false);
      m_pItemsSelect[14]->setEnabled(false);
      m_pItemsSelect[15]->setEnabled(false);
      m_pMenuItems[m_IndexMCSInfo]->setEnabled(false);
   }
   else
   {
      m_pItemsSelect[8]->setEnabled(true);
      m_pItemsSelect[9]->setEnabled(true);
      m_pItemsSelect[10]->setEnabled(true);
      m_pItemsSelect[11]->setEnabled(true);
      m_pItemsSelect[12]->setEnabled(true);
      m_pItemsSelect[13]->setEnabled(true);
      m_pItemsSelect[14]->setEnabled(true);
      m_pItemsSelect[15]->setEnabled(true);
      m_pMenuItems[m_IndexMCSInfo]->setEnabled(true);
   }

   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      m_pItemsSelect[11]->setSelection(0);

   m_pItemsSelect[15]->setSelection(0);
   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA )
      m_pItemsSelect[15]->setSelection(1);

   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      m_pItemsSelect[8]->setSelection(0);

   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
   if ( !(linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      m_pItemsSelect[8]->setSelection(2);

   if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
   if ( !(linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
      m_pItemsSelect[8]->setSelection(1);

   char szCapab[256];
   str_get_radio_capabilities_description(linkFlags, szCapab);
   log_line("MenuVehicleRadioLink: Update UI: Current radio link %d capabilities: %s.", m_iRadioLink+1, szCapab);

   selectedIndex = 0;
   for( int i=0; i<getDataRatesCount(); i++ )
      if ( g_pCurrentModel->radioLinksParams.link_datarates[m_iRadioLink][0] == getDataRates()[i] )
         selectedIndex = i;
   for( int i=0; i<=MAX_MCS_INDEX; i++ )
      if ( g_pCurrentModel->radioLinksParams.link_datarates[m_iRadioLink][0] == -1-i )
         selectedIndex = i+getDataRatesCount();

   m_pItemsSelect[9]->setSelection(selectedIndex);

   m_pItemsSelect[10]->setSelection(0);
   m_pItemsSelect[11]->setSelection(0);
   m_pItemsSelect[12]->setSelection(0);
   m_pItemsSelect[13]->setSelection(0);
   m_pItemsSelect[14]->setSelection(0);
   
   m_pItemsSelect[10]->setEnabled(false);
   m_pItemsSelect[11]->setEnabled(false);
   m_pItemsSelect[12]->setEnabled(false);
   m_pItemsSelect[13]->setEnabled(false);
   m_pItemsSelect[14]->setEnabled(false);
   m_pMenuItems[m_IndexMCSInfo]->setEnabled(false);

   if ( (1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count) )
   {
      m_pItemsSelect[7]->setSelection(1);
      m_pItemsSelect[8]->setSelection(0);
      m_pMenuItems[m_IndexUsage]->setEnabled(false);
      m_pMenuItems[m_IndexCapabilities]->setEnabled(false);
   }

   if ( ! (radioFlags & RADIO_FLAGS_ENABLE_MCS_FLAGS) )
      return;

   m_pItemsSelect[10]->setEnabled(true);
   m_pItemsSelect[11]->setEnabled(true);
   m_pItemsSelect[12]->setEnabled(true);
   m_pItemsSelect[13]->setEnabled(true);
   m_pItemsSelect[14]->setEnabled(true);
   m_pMenuItems[m_IndexMCSInfo]->setEnabled(true);

   if ( (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE) && (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER) )
      m_pItemsSelect[10]->setSelection(0);
   else if ( radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE )
      m_pItemsSelect[10]->setSelection(1);
   else
      m_pItemsSelect[10]->setSelection(2);

   if ( radioFlags & RADIO_FLAGS_HT40 )
      m_pItemsSelect[11]->setSelection(1);
   if ( radioFlags & RADIO_FLAGS_LDPC )
      m_pItemsSelect[12]->setSelection(1);
   if ( radioFlags & RADIO_FLAGS_SHORT_GI )
      m_pItemsSelect[13]->setSelection(1);
   if ( radioFlags & RADIO_FLAGS_STBC )
      m_pItemsSelect[14]->setSelection(1);
}

void MenuVehicleRadioLink::Render()
{
   RenderPrepare();
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();   
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   float maxWidth = m_Width*menu_getScaleMenus() - 2*MENU_MARGINS*menu_getScaleMenus();

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);

      if ( i == 1 && ((1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count)) )
      {
         g_pRenderEngine->setColors(get_Color_MenuText());
         y += 0.3 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING);
         y += g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus(), y, s_szMenuRadio_SingleCard2, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth()-MENU_MARGINS*menu_getScaleMenus(), g_idFontMenu);
         y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING);
      }
   }
   RenderEnd(yTop);
}


void MenuVehicleRadioLink::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

void MenuVehicleRadioLink::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
}

void MenuVehicleRadioLink::sendLinkCapabilitiesFlags(int linkIndex)
{
   int usage = m_pItemsSelect[7]->getSelectedIndex();
   int capab = m_pItemsSelect[8]->getSelectedIndex();
   log_line("MenuVehicleRadioLink: Selected menu item: usage index: %d, capability index: %d", usage, capab);

   u32 link_capabilities = g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex];

   link_capabilities = link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
   link_capabilities = link_capabilities & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA));
   link_capabilities = link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA);

   if ( 0 == usage ) link_capabilities = link_capabilities | RADIO_HW_CAPABILITY_FLAG_DISABLED;
   if ( 1 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);
   if ( 2 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
   if ( 3 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);

   if ( 0 == capab ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX);
   if ( 1 == capab ) link_capabilities = (link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_CAN_RX)) | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   if ( 2 == capab ) link_capabilities = (link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_CAN_TX)) | RADIO_HW_CAPABILITY_FLAG_CAN_RX;

   if ( 1 == m_pItemsSelect[15]->getSelectedIndex() )
      link_capabilities |= RADIO_HW_CAPABILITY_FLAG_USE_LOWEST_DATARATE_FOR_DATA;
     
   char szBuffC1[128];
   char szBuffC2[128];
   str_get_radio_capabilities_description(g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex], szBuffC1);
   str_get_radio_capabilities_description(link_capabilities, szBuffC2);

   log_line("MenuVehicleRadioLink: On update link capabilities:");
   log_line("MenuVehicleRadioLink: Radio link current capabilities: %s", szBuffC1);
   log_line("MenuVehicleRadioLink: Radio link new requested capabilities: %s", szBuffC2);

   if ( link_capabilities == g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex] )
      return;

   log_line("Sending new link capabilities: %d for radio link %d", link_capabilities, linkIndex+1);
   u32 param = (((u32)linkIndex) & 0xFF) | (link_capabilities<<8);
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_CAPABILITIES, param, NULL, 0) )
      valuesToUI();
}

void MenuVehicleRadioLink::sendLinkRadioFlags(int linkIndex)
{
   u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[linkIndex];
   int datarateVideo = 0;
   int datarateData = 0;

   int indexRate = m_pItemsSelect[9]->getSelectedIndex();
   if ( indexRate < getDataRatesCount() )
   {
      radioFlags &= ~RADIO_FLAGS_ENABLE_MCS_FLAGS;
      datarateVideo = getDataRates()[indexRate];
   }
   else
   {
      radioFlags |= RADIO_FLAGS_ENABLE_MCS_FLAGS;
      datarateVideo = -1-(indexRate - getDataRatesCount());
   }
   datarateData = datarateVideo;

   radioFlags &= ~(RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE | RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER);
   if ( 0 == m_pItemsSelect[10]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE | RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER;
   else if ( 1 == m_pItemsSelect[10]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE;
   else
      radioFlags |= RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER;

   radioFlags &= ~(RADIO_FLAGS_HT20 | RADIO_FLAGS_HT40);
   if ( 0 == m_pItemsSelect[11]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_HT20;
   else
      radioFlags |= RADIO_FLAGS_HT40;

   if ( 1 == m_pItemsSelect[12]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_LDPC;
   else
      radioFlags &= ~RADIO_FLAGS_LDPC;

   if ( 1 == m_pItemsSelect[13]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_SHORT_GI;
   else
      radioFlags &= ~RADIO_FLAGS_SHORT_GI;

   if ( 1 == m_pItemsSelect[14]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_STBC;
   else
      radioFlags &= ~RADIO_FLAGS_STBC;
  
   u8 buffer[64];
   u32* p = (u32*)&(buffer[0]);
   *p = linkIndex;
   p++;
   *p = radioFlags;
   p++;
   int* pi = (int*)p;
   *pi = datarateVideo;
   pi++;
   *pi = datarateData;
 
   memcpy(&g_LastGoodRadioLinksParams, &(g_pCurrentModel->radioLinksParams), sizeof(type_radio_links_parameters));
   char szBuff[128];
   str_get_radio_frame_flags_description(radioFlags, szBuff);

   if ( radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER )
   {
      log_line("Radio flags should be applied to controller too. Applying new radio flags on the controller right away.");
      radio_set_frames_flags(radioFlags);
   }

   g_pCurrentModel->radioLinksParams.link_radio_flags[linkIndex] = radioFlags;
   g_pCurrentModel->radioLinksParams.link_datarates[linkIndex][0] = datarateVideo;
   g_pCurrentModel->radioLinksParams.link_datarates[linkIndex][1] = datarateData;
   g_pCurrentModel->updateRadioInterfacesRadioFlags();
   saveAsCurrentModel(g_pCurrentModel);
   send_model_changed_message_to_router(MODEL_CHANGED_GENERIC);
   
   log_line("Sending to vehicle new radio link flags for radio link %d: %s and datarates: %d/%d", linkIndex+1, szBuff, datarateVideo, datarateData);
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_FLAGS, 0, buffer, 2*sizeof(u32) + 2*sizeof(int)) )
   {
      valuesToUI();
      memcpy(&(g_pCurrentModel->radioLinksParams), &g_LastGoodRadioLinksParams, sizeof(type_radio_links_parameters));            
   }
   else
   {
      g_TimePendingRadioFlagsChanged = g_TimeNow;
      g_PendingRadioFlagsConfirmationLinkId = linkIndex;
   }
}

void MenuVehicleRadioLink::onSelectItem()
{
   Menu::onSelectItem();
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( 0 != g_TimePendingRadioFlagsChanged )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   char szBuff[256];

   if ( -1 == m_IndexStart )
      return;

   int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(m_iRadioLink);
   if ( -1 == iRadioInterfaceId )
      return;

      if ( m_IndexFrequency == m_SelectedIndex )
      {
         int index = m_pItemsSelect[6]->getSelectedIndex();
         u32 freq = m_SupportedChannels[index];
         int band = getBand(freq);      
         if ( freq == g_pCurrentModel->radioLinksParams.link_frequency[m_iRadioLink] )
            return;

         u32 nicFreq[MAX_RADIO_INTERFACES];
         for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         {
            nicFreq[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i];
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
            snprintf(szBuff, sizeof(szBuff), "%s frequency is not supported by your controller.", str_format_frequency(freq));
            add_menu_to_stack(new MenuConfirmation("Invalid option",szBuff, 0, true));
            valuesToUI();
            return;
         }
         if ( (! allSupported) && (1 == g_pCurrentModel->radioLinksParams.links_count) )
         {
            char szBuff[256];
            snprintf(szBuff, sizeof(szBuff), "Not all radio interfaces on your controller support %s frequency. Some radio links on the controller will not be used.", str_format_frequency(freq));
            add_menu_to_stack(new MenuConfirmation("Confirmation",szBuff, 0, true));
         }

         u32 param = freq & 0xFFFFFF;
         param = param | (((u32)m_iRadioLink)<<24) | 0x80000000; // Highest bit set to 1 to mark the new format of the param
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_FREQUENCY, param, NULL, 0) )
            valuesToUI();
         return;
      }

      if ( m_IndexUsage == m_SelectedIndex )
      {
         int usage = m_pItemsSelect[7]->getSelectedIndex();

         u32 nicFreq[MAX_RADIO_INTERFACES];
         for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
            nicFreq[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i];

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

         sendLinkCapabilitiesFlags(m_iRadioLink);
         return;
      }
      if ( m_IndexCapabilities == m_SelectedIndex )
      {
         sendLinkCapabilitiesFlags(m_iRadioLink);
         return;
      }

      if ( m_IndexDataRate == m_SelectedIndex )
      {
         sendLinkRadioFlags(m_iRadioLink);
         return;
      }

      if ( m_IndexDatarateUseLowest == m_SelectedIndex )
      {
         sendLinkCapabilitiesFlags(m_iRadioLink);
         return;
      }

      //if ( m_IndexDataRateAdaptive[n] == m_SelectedIndex )
      //{
      //   sendRadioFlags(n);
      //   return;
      //}

      if ( m_IndexFlagsTarget == m_SelectedIndex || m_IndexSGI == m_SelectedIndex || m_IndexLDPC == m_SelectedIndex || m_IndexSTBC == m_SelectedIndex || m_IndexHT == m_SelectedIndex)
      {
         sendLinkRadioFlags(m_iRadioLink);
         return;
      }
}