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
#include "../base/encr.h"
#include "../common/string_utils.h"

#include "menu.h"
#include "popup.h"
#include "colors.h"
#include "osd_common.h"
#include "menu_vehicle_general.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include "handle_commands.h"
#include "launchers_controller.h"

int s_iTempGenNewFrequency = 0;
int s_iTempGenNewFrequencyLink = 0;

MenuVehicleGeneral::MenuVehicleGeneral(void)
:Menu(MENU_ID_VEHICLE_GENERAL, "General Vehicle Settings", NULL)
{
   m_Width = 0.26;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.21;
   m_bControllerHasKey = false;

   valuesToUI();
}

MenuVehicleGeneral::~MenuVehicleGeneral()
{
}

void MenuVehicleGeneral::addTopDescription()
{
   Preferences* pP = get_Preferences();
   removeAllTopLines();
   char szBuff[256];
   char szType[32];

   strcpy(szType, "runs");
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
      g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
      g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      strcpy(szType, "flights");
   
   sprintf(szBuff, "Total %s: %d", szType, g_pCurrentModel->m_Stats.uTotalFlights);
   addTopLine(szBuff);

   int sec = (g_pCurrentModel->m_Stats.uTotalOnTime)%60;
   int min = (g_pCurrentModel->m_Stats.uTotalOnTime/60)%60;
   int hours = (g_pCurrentModel->m_Stats.uTotalOnTime/3600);

   sprintf(szBuff, "Total ON time: %dh:%02dm:%02ds", hours, min, sec);
   addTopLine(szBuff);

   sec = (g_pCurrentModel->m_Stats.uTotalFlightTime)%60;
   min = (g_pCurrentModel->m_Stats.uTotalFlightTime/60)%60;
   hours = (g_pCurrentModel->m_Stats.uTotalFlightTime/3600);

   strcpy(szType, "run time");
   if ( g_pCurrentModel->vehicle_type == MODEL_TYPE_DRONE ||
      g_pCurrentModel->vehicle_type == MODEL_TYPE_AIRPLANE ||
      g_pCurrentModel->vehicle_type == MODEL_TYPE_HELI )
      strcpy(szType, "flight time");
   sprintf(szBuff, "Total %s: %dh:%02dm:%02ds", szType, hours, min, sec);
   addTopLine(szBuff);

   if ( pP->iUnits == prefUnitsImperial )
      sprintf(szBuff, "Odometer: %.1f Mi", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
   else
      sprintf(szBuff, "Odometer: %.1f Km", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
   addTopLine(szBuff);

   char szBuff2[64];
   getSystemVersionString(szBuff2, g_pCurrentModel->sw_version);

   sprintf(szBuff, "SW Version: %s", szBuff2);
   addTopLine(szBuff);
   
   addTopLine("");

   m_ExtraHeight = 0;
   return;
}

void MenuVehicleGeneral::populate()
{
   removeAllItems();
   addTopDescription();

   char szBuff[128];
   int len = 64;
   int res = lpp(szBuff, len);
   if ( res )
      m_bControllerHasKey = true;
   else
      m_bControllerHasKey = false;

   m_pItemEditName = new MenuItemEdit("Name", g_pCurrentModel->vehicle_name);
   m_pItemEditName->setMaxLength(MAX_VEHICLE_NAME_LENGTH-1);
   addMenuItem(m_pItemEditName);

   m_pItemsSelect[0] = new MenuItemSelect("Vehicle Type", "Changes the vehicle type Ruby is using. Has impact on things like OSD elements, telemetry parsing.");
   m_pItemsSelect[0]->addSelection("Generic");
   m_pItemsSelect[0]->addSelection("Drone");
   m_pItemsSelect[0]->addSelection("Airplane");
   m_pItemsSelect[0]->addSelection("Helicopter");
   m_pItemsSelect[0]->addSelection("Car");
   m_pItemsSelect[0]->addSelection("Boat");
   m_pItemsSelect[0]->addSelection("Robot");
   m_pItemsSelect[0]->setIsEditable();
   m_IndexVehicleType = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Has GPS", "Sets how many GPS units are configured on the vehicle. Ruby uses GPS data for OSD display.");  
   m_pItemsSelect[1]->addSelection("No GPS");
   m_pItemsSelect[1]->addSelection("1 GPS unit");
   m_pItemsSelect[1]->addSelection("2 GPS units");
   m_pItemsSelect[1]->addSelection("3 GPS units");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexGPS = addMenuItem(m_pItemsSelect[1]);

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

   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
      m_IndexFreq[i] = -1;

   if ( 0 == g_pCurrentModel->radioLinksParams.links_count )
   {
      addMenuItem( new MenuItemText("No radio interfaces detected on this vehicle!"));
      return;
   }

   u32 uControllerSupportedBands = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);
      if ( ! (cardFlags & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
         uControllerSupportedBands |= pRadioHWInfo->supportedBands;
   }

   for( int iRadioLinkId=0; iRadioLinkId<g_pCurrentModel->radioLinksParams.links_count; iRadioLinkId++ )
   {
      m_IndexFreq[iRadioLinkId] = -1;
      
      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(iRadioLinkId);
      if ( -1 == iRadioInterfaceId )
      {
         log_softerror_and_alarm("Invalid radio link. No radio interfaces assigned to radio link %d.", iRadioLinkId+1);
         continue;
      }

      m_SupportedChannelsCount[iRadioLinkId] = getSupportedChannels( uControllerSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 1, &(m_SupportedChannels[iRadioLinkId][0]), 100);

      char szBuff[64];
      char szTooltip[256];
      
      strcpy(szBuff, "Radio Link Frequency");
      strcpy(szTooltip, "Sets the radio link frequency used by the vehicle and the controller to communicate with each other.");
      if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
      {
         sprintf(szBuff, "Radio Link %d Frequency", iRadioLinkId+1 );
         sprintf(szTooltip, "Sets the radio link %d frequency used by the vehicle and the controller to communicate with each other.", iRadioLinkId+1);
      }

      m_pItemsSelect[20+iRadioLinkId] = new MenuItemSelect(szBuff, szTooltip);

      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId-1 == iRadioLinkId )
      {
         sprintf(szBuff, "Relay on %s", str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequency));
         m_pItemsSelect[20+iRadioLinkId]->addSelection(szBuff);
         m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
      }
      else
      {
         for( int ch=0; ch<m_SupportedChannelsCount[iRadioLinkId]; ch++ )
         {
            if ( -1 == m_SupportedChannels[iRadioLinkId][ch] )
            {
               m_pItemsSelect[20+iRadioLinkId]->addSeparator();
               continue;
            }
            sprintf(szBuff,"%s", str_format_frequency(m_SupportedChannels[iRadioLinkId][ch]));
            m_pItemsSelect[20+iRadioLinkId]->addSelection(szBuff);
         }

         m_SupportedChannelsCount[iRadioLinkId] = getSupportedChannels( uControllerSupportedBands & g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iRadioInterfaceId], 0, &(m_SupportedChannels[iRadioLinkId][0]), 100);

         int selectedIndex = 0;
         for( int ch=0; ch<m_SupportedChannelsCount[iRadioLinkId]; ch++ )
         {
            if ( getFrequencyInKHz(g_pCurrentModel->radioLinksParams.link_frequency[iRadioLinkId]) == getFrequencyInKHz(m_SupportedChannels[iRadioLinkId][ch]) )
               break;
               selectedIndex++;
         }
         m_pItemsSelect[20+iRadioLinkId]->setSelection(selectedIndex);

         if ( 0 == m_SupportedChannelsCount[iRadioLinkId] )
         {
            sprintf(szBuff,"%s", str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[iRadioLinkId]));
            m_pItemsSelect[20+iRadioLinkId]->addSelection(szBuff);
            m_pItemsSelect[20+iRadioLinkId]->setSelectedIndex(0);
            m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
         }

         m_pItemsSelect[20+iRadioLinkId]->setIsEditable();
         if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLinkId] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
            m_pItemsSelect[20+iRadioLinkId]->setEnabled(false);
      }
      m_IndexFreq[iRadioLinkId] = addMenuItem(m_pItemsSelect[20+iRadioLinkId]);
   }

   if ( g_pCurrentModel->radioLinksParams.links_count > 1 )
   {
      addMenuItem( new MenuItemText("Note: If you want to configure each individual radio link, go to [Radio Links] menu."));
      //addMenuItem( new MenuItemText("Note: If you want to configure the links for relaying other vehicles, go to [Relay] menu."));
   }
}

void MenuVehicleGeneral::valuesToUI()
{
   populate();

   m_pItemsSelect[0]->setSelection(g_pCurrentModel->vehicle_type);
   m_pItemsSelect[1]->setSelection(g_pCurrentModel->iGPSCount);

   m_pItemsSelect[2]->setSelectedIndex(0);

   m_pItemsSelect[3]->setSelectedIndex(0);

   if ( g_pCurrentModel->uModelFlags & MODEL_FLAG_PRIORITIZE_UPLINK )
      m_pItemsSelect[3]->setSelectedIndex(1); 

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
}

void MenuVehicleGeneral::Render()
{
   RenderPrepare();
   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   float height_text = g_pRenderEngine->textHeight(MENU_FONT_SIZE_TOOLTIPS*menu_getScaleMenus(), g_idFontMenu);
   float iconHeight = 4.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   u32 idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );

   g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   g_pRenderEngine->drawIcon(m_RenderXPos+m_RenderWidth - iconWidth -2.0*MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio() , m_RenderYPos + m_RenderHeaderHeight+MENU_MARGINS*menu_getScaleMenus() + iconHeight*0.05, iconWidth, iconHeight, idIcon);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);

   RenderEnd(yTop);
}


void MenuVehicleGeneral::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);
}

int MenuVehicleGeneral::onBack()
{
   if ( 0 == m_SelectedIndex )
   if ( m_pMenuItems[0]->isEditing() )
   {
      m_pMenuItems[0]->endEdit(false);
      char szBuff[MAX_VEHICLE_NAME_LENGTH];
      strcpy(szBuff, m_pItemEditName->getCurrentValue());
      str_sanitize_modelname(szBuff);

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VEHICLE_NAME, 0, (u8*)szBuff, MAX_VEHICLE_NAME_LENGTH) )
         m_pItemEditName->setCurrentValue(g_pCurrentModel->vehicle_name);
    
      return 1;
   }

   return Menu::onBack();
}

void MenuVehicleGeneral::onSelectItem()
{
   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( 0 == m_SelectedIndex )
   {
      m_pMenuItems[0]->beginEdit();
      return;
   }

   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( m_IndexVehicleType == m_SelectedIndex )
   {
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VEHICLE_TYPE, (u8)(m_pItemsSelect[0]->getSelectedIndex()), NULL, 0) )
         m_pItemsSelect[0]->setSelection(g_pCurrentModel->vehicle_type);
   }

   if ( m_IndexGPS == m_SelectedIndex )
   {
      int iGPSCount = m_pItemsSelect[1]->getSelectedIndex();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_GPS_INFO, iGPSCount, NULL, 0) )
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
         FILE* fd = fopen(FILE_ENCRYPTION_PASS, "r");
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

   for( int n=0; n<g_pCurrentModel->radioLinksParams.links_count; n++ )
   if ( m_IndexFreq[n] == m_SelectedIndex )
   {
      int index = m_pItemsSelect[20+n]->getSelectedIndex();
      u32 freq = m_SupportedChannels[n][index];
      int band = getBand(freq);      
      if ( freq == g_pCurrentModel->radioLinksParams.link_frequency[n] )
         return;

      u32 nicFreq[MAX_RADIO_INTERFACES];
      for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
         nicFreq[i] = g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i];

      int iRadioInterfaceId = g_pCurrentModel->getRadioInterfaceIndexForRadioLink(n);
      nicFreq[iRadioInterfaceId] = freq;

      const char* szError = controller_validate_radio_settings( g_pCurrentModel, nicFreq, NULL, NULL, NULL, NULL);
      
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
            sprintf(szBuff, "Not all radio interfaces on your controller support %s frequency. Some radio links on the controller will not be used.", str_format_frequency(freq));
            add_menu_to_stack(new MenuConfirmation("Confirmation",szBuff, 0, true));
         }
      u32 param = freq & 0xFFFFFF;
      param = param | (((u32)n)<<24) | 0x80000000; // Highest bit set to 1 to mark the new format of the param
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RADIO_LINK_FREQUENCY, param, NULL, 0) )
         valuesToUI();
   }
}