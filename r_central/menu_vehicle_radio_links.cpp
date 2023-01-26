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
#include "menu_vehicle_radio_links.h"
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

const char* s_szNoSupportedCards2 = "None of the radio interfaces supports this frequency.";
const char* s_szSomeUnsupportedCards2 = "Not all your radio interfaces on the controller support the current vehicle's frequency.";
const char* s_szUnsupportedFreqOnVehicle2 = "The vehicle's radio interface does not support that frequency. Reverted back.";

const char* s_szMenuRadio_SingleCard2 = "Note: You can not change the function and capabilities of the radio interface as there is a single one present on the vehicle.";

int s_iTempNewFrequency2 = 0;
int s_iTempNewFrequencyCard2 = 0;

MenuVehicleRadioLinks::MenuVehicleRadioLinks(void)
:Menu(MENU_ID_VEHICLE_RADIO_INTERFACES_EXPERT, "Vehicle Radio Links Parameters", NULL)
{
   m_Width = 0.3;
   m_xPos = 0.08;
   m_yPos = 0.1;
   char szBuff[256];

   m_IndexTXPower = addMenuItem(new MenuItem("Downlink Power Levels"));
   m_pMenuItems[m_IndexTXPower]->showArrow();

   for( int n=0; n<g_pCurrentModel->radioLinksParams.links_count; n++ )
   {
      char szBands[128];
      char szId[32];
      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(n);

      if ( -1 == iRadioInterfaceId )
      {
         log_softerror_and_alarm("Invalid radio link. No radio interfaces assigned to radio link %d.", n+1);
         m_IndexStartNics[n] = -1;
         continue;
      }

      str_get_supported_bands_string(g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], szBands);
      strcpy(szId, g_pCurrentModel->radioInterfacesParams.interface_szMAC[iRadioInterfaceId]);
      szId[6] = 0;
      sprintf(szBuff, "Radio Link %d", n+1);
      m_IndexStartNics[n] = addMenuItem(new MenuItemSection(szBuff));

      sprintf(szBuff, "Interface %d: Port %s, %s, ID: %s, %s", iRadioInterfaceId+1, g_pCurrentModel->radioInterfacesParams.interface_szPort[iRadioInterfaceId], str_get_radio_type_description(g_pCurrentModel->radioInterfacesParams.interface_type_and_driver[iRadioInterfaceId]), szId, szBands);
      addMenuItem(new MenuItemText(szBuff));

      u8 controllerBands = 0;
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNICInfo = hardware_get_radio_info(i);
         if ( controllerIsCardDisabled(pNICInfo->szMAC) )
            continue;
         controllerBands |= pNICInfo->supportedBands;
      }
      m_SupportedChannelsCount[n] = getSupportedChannels( controllerBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 1, &(m_SupportedChannels[n][0]), MAX_MENU_CHANNELS);

      m_pItemsSelect[12*n+5] = createMenuItemCardModelSelector("Radio Card Model");
      m_IndexCardModel[n] = addMenuItem(m_pItemsSelect[12*n+5]);


      m_pItemsSelect[12*n+6] = new MenuItemSelect("Frequency");

      for( int ch=0; ch<m_SupportedChannelsCount[n]; ch++ )
      {
         if ( m_SupportedChannels[n][ch] == -1 )
         {
            m_pItemsSelect[12*n+6]->addSeparator();
            continue;
         }
         sprintf(szBuff,"%d Mhz", m_SupportedChannels[n][ch]);
         m_pItemsSelect[12*n+6]->addSelection(szBuff);
      }
      m_SupportedChannelsCount[n] = getSupportedChannels( controllerBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 0, &(m_SupportedChannels[n][0]), MAX_MENU_CHANNELS);

      m_pItemsSelect[12*n+6]->setIsEditable();
      m_IndexFrequency[n] = addMenuItem(m_pItemsSelect[12*n+6]);

      m_pItemsSelect[12*n+7] = new MenuItemSelect("Usage", "Selects what type of data gets sent/received on this radio link, or disable it.");
      m_pItemsSelect[12*n+7]->addSelection("Disabled");
      m_pItemsSelect[12*n+7]->addSelection("Video & Data");
      m_pItemsSelect[12*n+7]->addSelection("Video");
      m_pItemsSelect[12*n+7]->addSelection("Data");
      //m_pItemsSelect[12*n+7]->addSelection("Relaying");
      m_pItemsSelect[12*n+7]->setIsEditable();
      m_IndexUsage[n] = addMenuItem(m_pItemsSelect[12*n+7]);

      m_pItemsSelect[12*n+8] = new MenuItemSelect("Capabilities", "Sets the uplink/downlink capabilities of this radio link. If the associated radio interface has attached an external LNA or a unidirectional booster, it can't be used for both uplink and downlink, it must be marked to be used only for uplink or downlink accordingly.");  
      m_pItemsSelect[12*n+8]->addSelection("Uplink & Downlink");
      m_pItemsSelect[12*n+8]->addSelection("Downlink Only");
      m_pItemsSelect[12*n+8]->addSelection("Uplink Only");
      m_pItemsSelect[12*n+8]->setIsEditable();
      m_IndexCapabilities[n] = addMenuItem(m_pItemsSelect[12*n+8]);

      m_pItemsSelect[12*n+9] = new MenuItemSelect("Radio Data Rate", "Sets the physical radio data rate to use on this link. If adaptive radio links is enabled, this can get lower automatically as needed.");
      for( int i=0; i<getDataRatesCount(); i++ )
      {
         sprintf(szBuff, "%d Mbps", getDataRates()[i]);
         m_pItemsSelect[12*n+9]->addSelection(szBuff);
      }
      for( int i=0; i<=MAX_MCS_INDEX; i++ )
      {
         sprintf(szBuff, "MCS-%d (%d Mbps)", i, getMCSRate(i));
         m_pItemsSelect[12*n+9]->addSelection(szBuff);
      }
      m_pItemsSelect[12*n+9]->setIsEditable();
      m_IndexDataRate[n] = addMenuItem(m_pItemsSelect[12*n+9]);


      //m_pItemsSelect[10*n+14] = new MenuItemSelect("Adaptive Datarates", "System will adjust the radio datarates based on realtime conditions of the link");
      //m_pItemsSelect[10*n+14]->addSelection("No");
      //m_pItemsSelect[10*n+14]->addSelection("Yes");
      //m_pItemsSelect[10*n+14]->setUseMultiViewLayout();
      //m_IndexDataRateAdaptive[n] = addMenuItem(m_pItemsSelect[10*n+14]);
      m_IndexDataRateAdaptive[n] = -1;

      m_IndexMCSInfo[n] = addMenuItem(new MenuItemText("MCS Data Rates Flags:"));

      m_pItemsSelect[12*n+10] = new MenuItemSelect("Apply MCS flags to", "When using MCS data rates, apply the MCS flags below to vehicle radio interface, to controller radio interfaces or to both.");
      m_pItemsSelect[12*n+10]->addSelection("Vehicle & Controller");
      m_pItemsSelect[12*n+10]->addSelection("Vehicle Only");
      m_pItemsSelect[12*n+10]->addSelection("Controller Only");
      m_pItemsSelect[12*n+10]->setIsEditable();
      m_IndexFlagsTarget[n] = addMenuItem(m_pItemsSelect[12*n+10]);

      m_pItemsSelect[12*n+11] = new MenuItemSelect("Channel Bandwidth", "Sets the radio channel bandwidth when using MCS data rates.");
      m_pItemsSelect[12*n+11]->addSelection("20 Mhz");
      m_pItemsSelect[12*n+11]->addSelection("40 Mhz");
      m_pItemsSelect[12*n+11]->setUseMultiViewLayout();
      m_IndexHT[n] = addMenuItem(m_pItemsSelect[12*n+11]);

      m_pItemsSelect[12*n+12] = new MenuItemSelect("Enable LDPC", "Enables LDPC (Low Density Parity Check) when using MCS data rates.");
      m_pItemsSelect[12*n+12]->addSelection("No");
      m_pItemsSelect[12*n+12]->addSelection("Yes");
      m_pItemsSelect[12*n+12]->setUseMultiViewLayout();
      m_IndexLDPC[n] = addMenuItem(m_pItemsSelect[12*n+12]);

      m_pItemsSelect[12*n+13] = new MenuItemSelect("Enable SGI", "Enables SGI (Short Guard Interval) when using MCS data rates.");
      m_pItemsSelect[12*n+13]->addSelection("No");
      m_pItemsSelect[12*n+13]->addSelection("Yes");
      m_pItemsSelect[12*n+13]->setUseMultiViewLayout();
      m_IndexSGI[n] = addMenuItem(m_pItemsSelect[12*n+13]);

      m_pItemsSelect[12*n+14] = new MenuItemSelect("Enable STBC", "Enables STBC when using MCS data rates.");
      m_pItemsSelect[12*n+14]->addSelection("No");
      m_pItemsSelect[12*n+14]->addSelection("Yes");
      m_pItemsSelect[12*n+14]->setUseMultiViewLayout();
      m_IndexSTBC[n] = addMenuItem(m_pItemsSelect[12*n+14]);
   }
}

MenuVehicleRadioLinks::~MenuVehicleRadioLinks()
{
}

void MenuVehicleRadioLinks::onShow()
{
   valuesToUI();
   Menu::onShow();

   m_ExtraHeight = MENU_ITEM_SPACING * menu_getScaleMenus()*MENU_FONT_SIZE_ITEMS;

   if ( 1 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
      m_ExtraHeight += 1.4*g_pRenderEngine->getMessageHeight(s_szMenuRadio_SingleCard2, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);

   invalidate();
}

void MenuVehicleRadioLinks::valuesToUI()
{   
   for( int n=0; n<g_pCurrentModel->radioLinksParams.links_count; n++ )
   {
      if ( -1 == m_IndexStartNics[n] )
         continue;
 
      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(n);
      if ( -1 == iRadioInterfaceId )
         continue;

      int cardModel = g_pCurrentModel->radioInterfacesParams.interface_card_model[iRadioInterfaceId];
      if ( cardModel < 0 )
      {
         m_pItemsSelect[12*n+5]->setSelectedIndex(-cardModel);
         m_pItemsSelect[12*n+5]->setEnabled(true);
      }
      else
      {
         m_pItemsSelect[12*n+5]->setSelectedIndex(cardModel);
         m_pItemsSelect[12*n+5]->setEnabled(true);
      }
      int selectedIndex = 0;
      for( int ch=0; ch<m_SupportedChannelsCount[n]; ch++ )
      {
         if ( g_pCurrentModel->radioLinksParams.link_frequency[n] == m_SupportedChannels[n][ch])
            break;
         selectedIndex++;
      }

      m_pItemsSelect[12*n+6]->setSelection(selectedIndex);

      u32 linkFlags = g_pCurrentModel->radioLinksParams.link_capabilities_flags[n];
      u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[n];
      m_pItemsSelect[12*n+7]->setSelection(0); // Disabled

      if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         m_pItemsSelect[12*n+7]->setSelection(1);

      if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      if ( !(linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO) )
         m_pItemsSelect[12*n+7]->setSelection(3);

      if ( !(linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA) )
      if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         m_pItemsSelect[12*n+7]->setSelection(2);

      if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
      {
         m_pItemsSelect[12*n+7]->setSelection(0);
         m_pItemsSelect[12*n+8]->setEnabled(false);
         m_pItemsSelect[12*n+9]->setEnabled(false);
         m_pItemsSelect[12*n+10]->setEnabled(false);
         m_pItemsSelect[12*n+11]->setEnabled(false);
         m_pItemsSelect[12*n+12]->setEnabled(false);
         m_pItemsSelect[12*n+13]->setEnabled(false);
         m_pItemsSelect[12*n+14]->setEnabled(false);
         m_pMenuItems[m_IndexMCSInfo[n]]->setEnabled(false);
      }
      else
      {
         m_pItemsSelect[12*n+8]->setEnabled(true);
         m_pItemsSelect[12*n+9]->setEnabled(true);
         m_pItemsSelect[12*n+10]->setEnabled(true);
         m_pItemsSelect[12*n+11]->setEnabled(true);
         m_pItemsSelect[12*n+12]->setEnabled(true);
         m_pItemsSelect[12*n+13]->setEnabled(true);
         m_pItemsSelect[12*n+14]->setEnabled(true);
         m_pMenuItems[m_IndexMCSInfo[n]]->setEnabled(true);
      }

      if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         m_pItemsSelect[10*n+11]->setSelection(0);

      if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         m_pItemsSelect[12*n+8]->setSelection(0);

      if ( !(linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX) )
      if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
         m_pItemsSelect[12*n+8]->setSelection(2);

      if ( linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( !(linkFlags & RADIO_HW_CAPABILITY_FLAG_CAN_RX) )
         m_pItemsSelect[12*n+8]->setSelection(1);

      selectedIndex = 0;
      for( int i=0; i<getDataRatesCount(); i++ )
         if ( g_pCurrentModel->radioLinksParams.link_datarates[n][0] == getDataRates()[i] )
            selectedIndex = i;
      for( int i=0; i<=MAX_MCS_INDEX; i++ )
         if ( g_pCurrentModel->radioLinksParams.link_datarates[n][0] == -1-i )
            selectedIndex = i+getDataRatesCount();

      m_pItemsSelect[12*n+9]->setSelection(selectedIndex);

      m_pItemsSelect[12*n+10]->setSelection(0);
      m_pItemsSelect[12*n+11]->setSelection(0);
      m_pItemsSelect[12*n+12]->setSelection(0);
      m_pItemsSelect[12*n+13]->setSelection(0);
      m_pItemsSelect[12*n+14]->setSelection(0);

      m_pItemsSelect[12*n+10]->setEnabled(false);
      m_pItemsSelect[12*n+11]->setEnabled(false);
      m_pItemsSelect[12*n+12]->setEnabled(false);
      m_pItemsSelect[12*n+13]->setEnabled(false);
      m_pItemsSelect[12*n+14]->setEnabled(false);
      m_pMenuItems[m_IndexMCSInfo[n]]->setEnabled(false);

      if ( ! (radioFlags & RADIO_FLAGS_ENABLE_MCS_FLAGS) )
         continue;

      m_pItemsSelect[12*n+10]->setEnabled(true);
      m_pItemsSelect[12*n+11]->setEnabled(true);
      m_pItemsSelect[12*n+12]->setEnabled(true);
      m_pItemsSelect[12*n+13]->setEnabled(true);
      m_pItemsSelect[12*n+14]->setEnabled(true);
      m_pMenuItems[m_IndexMCSInfo[n]]->setEnabled(true);

      if ( (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE) && (radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER) )
         m_pItemsSelect[12*n+10]->setSelection(0);
      else if ( radioFlags & RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE )
         m_pItemsSelect[12*n+10]->setSelection(1);
      else
         m_pItemsSelect[12*n+10]->setSelection(2);

      if ( radioFlags & RADIO_FLAGS_HT40 )
         m_pItemsSelect[12*n+11]->setSelection(1);
      if ( radioFlags & RADIO_FLAGS_LDPC )
         m_pItemsSelect[12*n+12]->setSelection(1);
      if ( radioFlags & RADIO_FLAGS_SHORT_GI )
         m_pItemsSelect[12*n+13]->setSelection(1);
      if ( radioFlags & RADIO_FLAGS_STBC )
         m_pItemsSelect[12*n+14]->setSelection(1);
   }

   if ( (1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count) )
   {
      m_pItemsSelect[7]->setSelection(1);
      m_pItemsSelect[8]->setSelection(0);
      m_pMenuItems[m_IndexUsage[0]]->setEnabled(false);
      m_pMenuItems[m_IndexCapabilities[0]]->setEnabled(false);
   }
}

void MenuVehicleRadioLinks::Render()
{
   RenderPrepare();
   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();   
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   float maxWidth = m_Width*menu_getScaleMenus() - 2*MENU_MARGINS*menu_getScaleMenus();

   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);

      if ( i == 4 && ((1 == g_pCurrentModel->radioInterfacesParams.interfaces_count) || (1 == g_pCurrentModel->radioLinksParams.links_count)) )
      {
         g_pRenderEngine->setColors(get_Color_MenuText());
         y += 0.3 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING);
         y += g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus() + 0.006*menu_getScaleMenus(), y, s_szMenuRadio_SingleCard2, menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
         y += 0.5 * menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE*(1+MENU_TEXTLINE_SPACING);
      }
   }
   RenderEnd(yTop);
}


void MenuVehicleRadioLinks::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

void MenuVehicleRadioLinks::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
}

void MenuVehicleRadioLinks::sendLinkCapabilitiesFlags(int linkIndex)
{
   int usage = m_pItemsSelect[12*linkIndex+7]->getSelectedIndex();
   int capab = m_pItemsSelect[12*linkIndex+8]->getSelectedIndex();
   log_line("usage: %d, cap: %d", usage, capab);

   u32 link_capabilities = g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex];

   link_capabilities = link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
   link_capabilities = link_capabilities & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA | RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_RELAY));

   if ( 0 == usage ) link_capabilities = link_capabilities | RADIO_HW_CAPABILITY_FLAG_DISABLED;
   if ( 1 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);
   if ( 2 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
   if ( 3 == usage ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);

   if ( 0 == capab ) link_capabilities = link_capabilities | (RADIO_HW_CAPABILITY_FLAG_CAN_RX | RADIO_HW_CAPABILITY_FLAG_CAN_TX);
   if ( 1 == capab ) link_capabilities = (link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_CAN_RX)) | RADIO_HW_CAPABILITY_FLAG_CAN_TX;
   if ( 1 == capab ) link_capabilities = (link_capabilities & (~RADIO_HW_CAPABILITY_FLAG_CAN_TX)) | RADIO_HW_CAPABILITY_FLAG_CAN_RX;

   char szBuffC1[128];
   char szBuffC2[128];
   str_get_radio_capabilities_description(g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex], szBuffC1);
   str_get_radio_capabilities_description(link_capabilities, szBuffC2);

   log_line("Use changed link capabilities flags: current: %s, target: %s", szBuffC1, szBuffC2);

   if ( link_capabilities == g_pCurrentModel->radioLinksParams.link_capabilities_flags[linkIndex] )
      return;

   log_line("Sending new link capabilities: %d for radio link %d", link_capabilities, linkIndex+1);
   u32 param = (((u32)linkIndex) & 0xFF) | (link_capabilities<<8);
   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_CAPABILITIES, param, NULL, 0) )
      valuesToUI();
}

void MenuVehicleRadioLinks::sendLinkRadioFlags(int linkIndex)
{
   u32 radioFlags = g_pCurrentModel->radioLinksParams.link_radio_flags[linkIndex];
   int datarateVideo = 0;
   int datarateData = 0;

   int indexRate = m_pItemsSelect[12*linkIndex+9]->getSelectedIndex();
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
   if ( 0 == m_pItemsSelect[12*linkIndex+10]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE | RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER;
   else if ( 1 == m_pItemsSelect[12*linkIndex+10]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_APPLY_MCS_FLAGS_ON_VEHICLE;
   else
      radioFlags |= RADIO_FLAGS_APPLY_MCS_FLAGS_ON_CONTROLLER;

   radioFlags &= ~(RADIO_FLAGS_HT20 | RADIO_FLAGS_HT40);
   if ( 0 == m_pItemsSelect[12*linkIndex+11]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_HT20;
   else
      radioFlags |= RADIO_FLAGS_HT40;

   if ( 1 == m_pItemsSelect[12*linkIndex+12]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_LDPC;
   else
      radioFlags &= ~RADIO_FLAGS_LDPC;

   if ( 1 == m_pItemsSelect[12*linkIndex+13]->getSelectedIndex() )
      radioFlags |= RADIO_FLAGS_SHORT_GI;
   else
      radioFlags &= ~RADIO_FLAGS_SHORT_GI;

   if ( 1 == m_pItemsSelect[12*linkIndex+14]->getSelectedIndex() )
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
   send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_MODEL_CHANGED, PACKET_COMPONENT_COMMANDS);

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

void MenuVehicleRadioLinks::onSelectItem()
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

   for( int n=0; n<g_pCurrentModel->radioLinksParams.links_count; n++ )
   {
      if ( -1 == m_IndexStartNics[n] )
         continue;

      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(n);
      if ( -1 == iRadioInterfaceId )
         continue;

      if ( m_IndexCardModel[n] == m_SelectedIndex )
      {
         u32 uParam = ((u32) iRadioInterfaceId) & 0xFF;
         int iCardModel = m_pItemsSelect[12*n+5]->getSelectedIndex();
         if ( iCardModel > 0 )
            iCardModel = -iCardModel;
         uParam = uParam | (((iCardModel+128) & 0xFF) << 8);
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_CARD_MODEL, uParam, NULL, 0) )
            valuesToUI();
         return;
      }

      if ( m_IndexFrequency[n] == m_SelectedIndex )
      {
         int index = m_pItemsSelect[12*n+6]->getSelectedIndex();
         int freq = m_SupportedChannels[n][index];
         int band = getBand(freq);      
         if ( freq == g_pCurrentModel->radioLinksParams.link_frequency[n] )
            return;

         int nicFreq[MAX_RADIO_INTERFACES];
         for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         {
            nicFreq[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i];
            log_line("Current frequency on card %d: %d Mhz", i+1, nicFreq[i]);
         }

         int nicFlags[MAX_RADIO_INTERFACES];
         for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
            nicFlags[i] = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i];

         nicFreq[n] = freq;

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
            radio_hw_info_t* pNIC = hardware_get_radio_info(i);
            if ( controllerIsCardDisabled(pNIC->szMAC) )
               continue;
            if ( band & pNIC->supportedBands )
               supportedOnController = true;
            else
               allSupported = false;
         }
         if ( ! supportedOnController )
         {
            sprintf(szBuff, "%d Mhz frequency is not supported by your controller.", freq);
            add_menu_to_stack(new MenuConfirmation("Invalid option",szBuff, 0, true));
            valuesToUI();
            return;
         }
         if ( ! allSupported )
         {
            char szBuff[256];
            sprintf(szBuff, "Not all radio interfaces on your controller support %d Mhz frequency. Some radio links on the controller will not be used.", freq);
            add_menu_to_stack(new MenuConfirmation("Confirmation",szBuff, 0, true));
         }

         u32 param = freq;
         param = param | (n<<16);
         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_FREQUENCY, param, NULL, 0) )
            valuesToUI();
         return;
      }

      if ( m_IndexUsage[n] == m_SelectedIndex )
      {
         int usage = m_pItemsSelect[12*n+7]->getSelectedIndex();

         int nicFreq[MAX_RADIO_INTERFACES];
         for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
            nicFreq[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i];

         int nicFlags[MAX_RADIO_INTERFACES];
         for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
            nicFlags[i] = g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i];

         nicFlags[n] = nicFlags[n] & (~RADIO_HW_CAPABILITY_FLAG_DISABLED);
         nicFlags[n] = nicFlags[n] & (~(RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_RELAY));
         if ( 0 == usage ) nicFlags[n] = nicFlags[n] | RADIO_HW_CAPABILITY_FLAG_DISABLED;
         if ( 1 == usage ) nicFlags[n] = nicFlags[n] | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO|RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);
         if ( 2 == usage ) nicFlags[n] = nicFlags[n] | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO);
         if ( 3 == usage ) nicFlags[n] = nicFlags[n] | (RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA);

         const char* szError = controller_validate_radio_settings( g_pCurrentModel, nicFreq, nicFlags, NULL, NULL, NULL);
      
         if ( NULL != szError && 0 != szError[0] )
         {
            log_line(szError);
            add_menu_to_stack(new MenuConfirmation("Invalid option",szError, 0, true));
            valuesToUI();
            return;
         }

         sendLinkCapabilitiesFlags(n);
         return;
      }
      if ( m_IndexCapabilities[n] == m_SelectedIndex )
      {
         sendLinkCapabilitiesFlags(n);
         return;
      }

      if ( m_IndexDataRate[n] == m_SelectedIndex )
      {
         sendLinkRadioFlags(n);
         return;
      }

      //if ( m_IndexDataRateAdaptive[n] == m_SelectedIndex )
      //{
      //   sendRadioFlags(n);
      //   return;
      //}

      if ( m_IndexFlagsTarget[n] == m_SelectedIndex || m_IndexSGI[n] == m_SelectedIndex || m_IndexLDPC[n] == m_SelectedIndex || m_IndexSTBC[n] == m_SelectedIndex || m_IndexHT[n] == m_SelectedIndex)
      {
         sendLinkRadioFlags(n);
         return;
      }
   }

   if ( m_IndexTXPower == m_SelectedIndex )
   {
      add_menu_to_stack(new MenuTXInfo());
      return;
   }
}