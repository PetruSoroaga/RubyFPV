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
#include "../base/ctrl_settings.h"
#include "../base/ctrl_interfaces.h"
#include "../common/string_utils.h"
#include "render_commands.h"
#include "osd.h"
#include "colors.h"
#include "../renderer/render_engine.h"
#include "menu.h"
#include "menu_vehicle.h"
#include "menu_vehicle_general.h"
//#include "menu_vehicle_radio.h"
#include "menu_vehicle_video.h"
#include "menu_vehicle_camera.h"
#include "menu_vehicle_osd.h"
#include "menu_vehicle_expert.h"
#include "menu_confirmation.h"
#include "menu_vehicle_telemetry.h"
#include "menu_vehicle_relay.h"

#include "shared_vars.h"
#include "pairing.h"
#include "ruby_central.h"
#include <ctype.h>
#include "handle_commands.h"
#include "osd_common.h"

MenuVehicleRelay::MenuVehicleRelay(void)
:Menu(MENU_ID_VEHICLE_RELAY, "Relay Settings", NULL)
{
   m_Width = 0.4;
   m_xPos = menu_get_XStartPos(m_Width);
   m_yPos = 0.3;
   m_bIsConfigurable = false;
   m_iCountVehiclesIDs = 0;
   
   hardware_load_radio_info();
}

MenuVehicleRelay::~MenuVehicleRelay()
{
}

void MenuVehicleRelay::onShow()
{
   m_fHeightHeader = 0.24/g_pRenderEngine->getAspectRatio();
   m_Height = 0.0;
   m_ExtraHeight = 0.0;
   
   char szBuff[128];
   removeAllItems();
   removeAllTopLines();

   m_IndexItemEnabled = -1;
   m_IndexQAButton = -1;
   m_IndexVehicle = -1;
   m_bIsConfigurable = false;

   int countLinksOkOnVehicle = 0;
   int countCardsOkOnVehicle = 0;
   for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      if ( 0 != g_pCurrentModel->radioInterfacesParams.interface_supported_bands[i] )
      if ( ! (g_pCurrentModel->radioInterfacesParams.interface_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
         countCardsOkOnVehicle++;

   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      if ( ! (g_pCurrentModel->radioLinksParams.link_capabilities_flags[i] & RADIO_HW_CAPABILITY_FLAG_DISABLED) )
         countLinksOkOnVehicle++;

   if ( (countLinksOkOnVehicle < 2) || (countCardsOkOnVehicle < 2) )
   {
      float iconSize = 4 * menu_getScaleMenus()*MENU_FONT_SIZE_TOOLTIPS;
      addTopLine("You can not enalbe relay functionality on this vehicle!", iconSize);
      addTopLine("You need at least two radio interfaces on the vehicle to be able to relay from this vehicle.", iconSize);
      Menu::onShow();
      return;
   }

   m_ExtraHeight = m_fHeightHeader;
   m_bIsConfigurable = true;

   m_pItemsSelect[0] = new MenuItemSelect("Enable Relay Mode", "Enables or disables the relay functionality on this vehicle (this vehicle will relay data/video from another vehicle).");  
   m_pItemsSelect[0]->addSelection("Disabled");
   for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
   {
      snprintf(szBuff, sizeof(szBuff),"Enable on radio link %d", i+1);
      m_pItemsSelect[0]->addSelection(szBuff);
   }
   m_pItemsSelect[0]->setIsEditable();
   m_IndexItemEnabled = addMenuItem(m_pItemsSelect[0]);

   m_pItemsSelect[1] = new MenuItemSelect("Relay Switching", "Selects the QA button to be used to switch between this vehicle and the relayed one.");  
   m_pItemsSelect[1]->addSelection("Disabled");
   m_pItemsSelect[1]->addSelection("QA Button 1");
   m_pItemsSelect[1]->addSelection("QA Button 2");
   m_pItemsSelect[1]->addSelection("QA Button 3");
   m_pItemsSelect[1]->setIsEditable();
   m_IndexQAButton = addMenuItem(m_pItemsSelect[1]);

   m_iCountVehiclesIDs = 0;

   m_pItemsSelect[2] = new MenuItemSelect("Relayed Vehicle", "Selects the vehicle to relay.");  
   m_pItemsSelect[2]->addSelection("None");
   for( int i=0; i<getModelsCount(); i++ )
   {
      Model *pModel = getModelAtIndex(i);
      if ( NULL == pModel )
         continue;
      if ( pModel->vehicle_id == g_pCurrentModel->vehicle_id )
         continue;

      m_uVehiclesID[m_iCountVehiclesIDs] = pModel->vehicle_id;
      m_iCountVehiclesIDs++;

      char szName[128];
      strlcpy(szName, pModel->getLongName(), sizeof(szName));
      szName[0] = toupper(szName[0]);

      snprintf(szBuff, sizeof(szBuff),"%s, %s", szName, str_format_frequency(pModel->radioLinksParams.link_frequency[0]));
      m_pItemsSelect[2]->addSelection(szBuff);
   }

   /*
   for( int i=0; i<getModelsSpectatorCount(); i++ )
   {
      Model *pModel = getModelSpectator(i);
      if ( NULL == pModel )
         continue;
      if ( pModel->vehicle_id == g_pCurrentModel->vehicle_id )
         continue;

      char szName[128];
       strlcpy(szName, pModel->getLongName(), sizeof(szName));
      szName[0] = toupper(szName[0]);

      snprintf(szBuff, sizeof(szBuff),"%s (S), %s", szName, str_format_frequency(pModel->radioLinksParams.link_frequency[0]));
      m_pItemsSelect[2]->addSelection(szBuff);
   }
   */
   m_pItemsSelect[2]->setIsEditable();
   m_IndexVehicle = addMenuItem(m_pItemsSelect[2]);
   
   m_pItemsSelect[3] = new MenuItemSelect("Relay Type", "Selects which radio streams to relay.");
   m_pItemsSelect[3]->addSelection("Video Only");
   m_pItemsSelect[3]->addSelection("Telemetry Only");
   m_pItemsSelect[3]->addSelection("Video & Telemetry");
   m_pItemsSelect[3]->setIsEditable();
   m_IndexRelayType = addMenuItem(m_pItemsSelect[3]);

   m_pItemsSelect[4] = new MenuItemSelect("Show on OSD", "Shows relaying state in the OSD.");
   m_pItemsSelect[4]->addSelection("No");
   m_pItemsSelect[4]->addSelection("Yes");
   m_pItemsSelect[4]->setUseMultiViewLayout();
   m_IndexOSD = addMenuItem(m_pItemsSelect[4]);

   Menu::onShow();

   if ( g_pCurrentModel->rc_params.rc_enabled )
   if ( pairing_isReceiving() || pairing_wasReceiving() )
   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].bGotFCTelemetry )
   if ( g_VehiclesRuntimeInfo[g_iCurrentActiveVehicleRuntimeInfoIndex].headerFCTelemetry.flags & FC_TELE_FLAGS_ARMED )
      addMessage("RC Link is enabled and vehicle is Armed. We do not recomend chaning the relay settings while Armed. It has an impact on your radio links.");
}


void MenuVehicleRelay::valuesToUI()
{
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pP = get_Preferences();
   
   if ( NULL == pCS || NULL == pP || (!m_bIsConfigurable) )
      return;

   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId <= 0 )
   {
      m_pItemsSelect[0]->setSelection(0);
      m_pItemsSelect[1]->setEnabled(false);
      m_pItemsSelect[2]->setEnabled(false);
      m_pItemsSelect[3]->setEnabled(false);
      m_pItemsSelect[4]->setEnabled(false);
      return;
   }

   m_pItemsSelect[0]->setSelection(g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId);
   m_pItemsSelect[1]->setEnabled(true);
   m_pItemsSelect[2]->setEnabled(true);
   m_pItemsSelect[3]->setEnabled(true);
   m_pItemsSelect[4]->setEnabled(true);

   m_pItemsSelect[1]->setSelection(0);
   if ( pCS->iQAButtonRelaySwitching >= 0 )
       m_pItemsSelect[1]->setSelectedIndex(pCS->iQAButtonRelaySwitching+1);

   m_pItemsSelect[2]->setSelection(0);
   if ( g_pCurrentModel->relay_params.uRelayVehicleId != 0 )
   if ( g_pCurrentModel->relay_params.uRelayFrequency != 0 )
   {
      for( int i=0; i<getModelsCount(); i++ )
      {
         Model *pModel = getModelAtIndex(i);
         if ( NULL == pModel )
            continue;
         if ( pModel->vehicle_id == g_pCurrentModel->relay_params.uRelayVehicleId )
         {
            m_pItemsSelect[2]->setSelectedIndex(i+1);
            break;
         }
      }
   }

   m_pItemsSelect[3]->setSelectedIndex(0);
   if ( g_pCurrentModel->relay_params.uRelayFlags == RELAY_FLAGS_NONE )
      m_pItemsSelect[3]->setEnabled(false);
   else
   {
      m_pItemsSelect[3]->setEnabled(true);
      if ( (g_pCurrentModel->relay_params.uRelayFlags & RELAY_FLAGS_VIDEO) &&
           (g_pCurrentModel->relay_params.uRelayFlags & RELAY_FLAGS_TELEMETRY) )
         m_pItemsSelect[3]->setSelectedIndex(2);
      else if ( g_pCurrentModel->relay_params.uRelayFlags & RELAY_FLAGS_VIDEO )
         m_pItemsSelect[3]->setSelectedIndex(0);
      else if ( g_pCurrentModel->relay_params.uRelayFlags & RELAY_FLAGS_TELEMETRY )
         m_pItemsSelect[3]->setSelectedIndex(1);
   }

   m_pItemsSelect[4]->setSelectedIndex(0);
   if ( g_pCurrentModel->relay_params.uRelayFlags & RELAY_FLAGS_SHOW_OSD )
      m_pItemsSelect[4]->setSelectedIndex(1);
}


void MenuVehicleRelay::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle(); 

   _drawHeader(yTop - 0.4*MENU_MARGINS*menu_getScaleMenus());
   float y = yTop + m_ExtraHeight;
 
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleRelay::_drawHeader(float yPos)
{
   if ( ! m_bIsConfigurable )
      return;

   char szBuff[128];
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float height_text_large = g_pRenderEngine->textHeight(g_idFontMenuLarge);
   
   float fSpacing = 0.14/g_pRenderEngine->getAspectRatio();
   float fMarginY = 0.02;
   float xPos = m_xPos + MENU_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   float hIconCtrlr = m_fHeightHeader - height_text_large - fMarginY;
   float hIcon = hIconCtrlr*0.7;

   float yMidLine = yPos + 0.5*hIconCtrlr;

   u32 uIdIconVehicle = g_idIconDrone;
   if ( NULL != g_pCurrentModel )
      uIdIconVehicle = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );
   
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   g_pRenderEngine->drawIcon(xPos, yPos, hIconCtrlr/g_pRenderEngine->getAspectRatio(), hIconCtrlr, g_idIconController);
   g_pRenderEngine->drawText(xPos, yPos + hIconCtrlr, g_idFontMenu, "Controller");

   xPos += hIconCtrlr/g_pRenderEngine->getAspectRatio() + 0.02/g_pRenderEngine->getAspectRatio();

   u32 iFreqMain = 0;
   for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
   {
      int iRadioLink = g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId;
      if ( iRadioLink < 0 || iRadioLink >= g_pSM_RadioStats->countRadioLinks )
         continue;
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      if ( ! pRadioInfo->isHighCapacityInterface )
         continue;
      iFreqMain = g_pSM_RadioStats->radio_interfaces[i].uCurrentFrequency;
      break;
   }

   u32 iFreqSec = 0;
   for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
   {
      int iRadioLink = g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId;
      if ( iRadioLink < 0 || iRadioLink >= g_pSM_RadioStats->countRadioLinks )
         continue;
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      if ( ! pRadioInfo->isHighCapacityInterface )
         continue;
      if ( g_pSM_RadioStats->radio_interfaces[i].uCurrentFrequency == iFreqMain )
         continue;
      iFreqSec = g_pSM_RadioStats->radio_interfaces[i].uCurrentFrequency;
      break;
   }

   g_pRenderEngine->drawLine(xPos, yMidLine, xPos+fSpacing, yMidLine);

   strcpy(szBuff, str_format_frequency(iFreqMain) );
   float fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine - height_text*1.1, g_idFontMenu, szBuff);

   if ( iFreqSec > 0 )
   {
      strcpy(szBuff, str_format_frequency(iFreqSec) );
      fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine + 0.1*height_text, g_idFontMenu, szBuff);    
   }
   else if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId > 0 )
   {
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == 1 )
         strcpy(szBuff, "Link 2");
      else
         strcpy(szBuff, "Link 1");

      fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine + 0.1*height_text, g_idFontMenu, szBuff);
   }

   xPos += fSpacing + 0.02/g_pRenderEngine->getAspectRatio();

   // ---------------------------------------------
   // Draw vehicle

   g_pRenderEngine->drawIcon(xPos, yMidLine - 0.5*hIcon, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, uIdIconVehicle);

   if ( NULL == g_pCurrentModel )
      strcpy(szBuff, "No Vehicle Active");
   else
      strcpy(szBuff, g_pCurrentModel->getLongName());

   szBuff[0] = toupper(szBuff[0]);
   fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawText(xPos-fW*0.5 + 0.5*hIcon/g_pRenderEngine->getAspectRatio(), yMidLine + 0.6*hIcon, g_idFontMenu, szBuff);

   // ------------------------------------------------
   // Draw relay

   xPos += hIconCtrlr/g_pRenderEngine->getAspectRatio() + 0.01/g_pRenderEngine->getAspectRatio();

   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId <= 0 )
   {   
      strcpy(szBuff, "No Relaying");
      float fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine - height_text*0.4, g_idFontMenu, szBuff);

      xPos += fSpacing + 0.02/g_pRenderEngine->getAspectRatio();
      g_pRenderEngine->drawIcon(xPos, yMidLine - hIcon*0.5, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, g_idIconX);  
      return;
   }
   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId > 0 )
   if ( (g_pCurrentModel->relay_params.uRelayVehicleId == 0) || (g_pCurrentModel->relay_params.uRelayFrequency == 0) )
   {
      fSpacing *= 1.3;

      strcpy(szBuff, "No Vehicle Selected");
      float fW = g_pRenderEngine->textWidth(g_idFontMenuSmall, szBuff);
      g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine - height_text*1.0, g_idFontMenuSmall, szBuff);

      g_pRenderEngine->drawLine(xPos, yMidLine, xPos+fSpacing, yMidLine);

      xPos += fSpacing + 0.02/g_pRenderEngine->getAspectRatio();
      float fA = g_pRenderEngine->setGlobalAlfa(0.2);
      g_pRenderEngine->drawIcon(xPos, yMidLine - hIcon*0.5, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, g_idIconWarning);  
      g_pRenderEngine->setGlobalAlfa(fA);
      return;
   }

   //------------------------------------
   // Draw relayed vehicle

   Model* pRelayModel = findModelWithId(g_pCurrentModel->relay_params.uRelayVehicleId);
   
   uIdIconVehicle = g_idIconDrone;
   if ( NULL != pRelayModel )
      uIdIconVehicle = osd_getVehicleIcon( pRelayModel->vehicle_type );

   g_pRenderEngine->drawLine(xPos, yMidLine, xPos+fSpacing, yMidLine);

   strcpy(szBuff, str_format_frequency(g_pCurrentModel->relay_params.uRelayFrequency) );
   fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine - height_text*1.1, g_idFontMenu, szBuff);

   if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId > 0 )
   {
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == 1 )
            strcpy(szBuff, "Link 1");
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == 2 )
            strcpy(szBuff, "Link 2");
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == 3 )
            strcpy(szBuff, "Link 3");

      fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine + 0.1*height_text, g_idFontMenu, szBuff);
   }

   u32 iRelayVehicleFreq = 0;
   if ( NULL != pRelayModel )
      iRelayVehicleFreq = pRelayModel->radioLinksParams.link_frequency[0];

   if ( iRelayVehicleFreq != g_pCurrentModel->relay_params.uRelayFrequency )
   {
      snprintf(szBuff, sizeof(szBuff), "Now at %s !", str_format_frequency(iRelayVehicleFreq) );
      fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
      g_pRenderEngine->drawText(xPos + 0.5*fSpacing - 0.5*fW, yMidLine + height_text*0.1, g_idFontMenu, szBuff);
   }
   xPos += fSpacing + 0.02/g_pRenderEngine->getAspectRatio();
   
   g_pRenderEngine->drawIcon(xPos, yMidLine - 0.5*hIcon, hIcon/g_pRenderEngine->getAspectRatio(), hIcon, uIdIconVehicle);

   if ( NULL == pRelayModel )
      strcpy(szBuff, "No Relay Vehicle");
   else
      strcpy(szBuff, pRelayModel->getLongName());

   szBuff[0] = toupper(szBuff[0]);
   fW = g_pRenderEngine->textWidth(g_idFontMenu, szBuff);
   g_pRenderEngine->drawText(xPos-fW*0.5 + 0.5*hIcon/g_pRenderEngine->getAspectRatio(), yMidLine + 0.6*hIcon, g_idFontMenu, szBuff);

}

bool MenuVehicleRelay::_check_enable_relay(int iRadioLinkRelay)
{
   char szBuff[256];

   if ( iRadioLinkRelay < 0 )
   {
      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

      params.isRelayEnabledOnRadioLinkId = -1;
      params.uCurrentRelayMode = RELAY_MODE_NONE;
      params.uRelayVehicleId = 0;
      params.uRelayFrequency = 0;
      params.uRelayFlags = 0;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
         valuesToUI();

      return true;
   }
  
   // Compute the current active radio links from controller to vehile

   int iCurrentActiveRadioLinksCount = 0;
   int iCurrentActiveRadioLinksIds[MAX_RADIO_INTERFACES];

   for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
   {
      int iRadioLink = g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId;
      if ( iRadioLink < 0 )
         continue;

      bool bFoundLink = false;
      for( int k=0; k<iCurrentActiveRadioLinksCount; k++ )
         if ( iCurrentActiveRadioLinksIds[k] == iRadioLink )
         {
            bFoundLink = true;
            break;
         }
      if ( bFoundLink )
         continue;
      iCurrentActiveRadioLinksIds[iCurrentActiveRadioLinksCount] = iRadioLink;
      iCurrentActiveRadioLinksCount++;
   }

   if ( iCurrentActiveRadioLinksCount == 1 )
   if ( iCurrentActiveRadioLinksIds[0] == iRadioLinkRelay )
   {
      sprintf( szBuff, "You can't use your main radio link %d between your controller and your vehicle as a relay link. It's the only active link between your controller and current vehicle.", iRadioLinkRelay+1);
      addMessage(szBuff);
      return false;
   }

   // See if remaining links to vehicle are capable of high datarate

   bool bHasHighCapacityRemaining = false;

   for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
   {
      int iRadioLink = g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId;           
      if ( iRadioLink == iRadioLinkRelay )
         continue;
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      if ( controllerIsCardDisabled(pRadioInfo->szMAC) )
         continue;
      if ( pRadioInfo->isHighCapacityInterface )
      {
         bHasHighCapacityRemaining = true;
         break;
      }
   }

   if ( ! bHasHighCapacityRemaining )
   {
      snprintf(szBuff, sizeof(szBuff), "You can't use vehicle radio link %d for relaying. You will not have any remaining high capacity link between your controller and this vehicle.", iRadioLinkRelay+1);
      addMessage(szBuff);
      return false;
   }

   // See if remaining links to vehicle are capable of tx and rx

   bool bHasTxRemaining = false;
   bool bHasRxRemaining = false;

   for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
   {
      int iRadioLink = g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId;           
      if ( iRadioLink == iRadioLinkRelay )
         continue;
      
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( controllerIsCardDisabled(pRadioInfo->szMAC) )
         continue;
      
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_TX )
      if ( ! controllerIsCardRXOnly(pRadioInfo->szMAC) )
         bHasTxRemaining = true;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_RX )
      if ( ! controllerIsCardTXOnly(pRadioInfo->szMAC) )
         bHasRxRemaining = true;
   }

   if ( !bHasRxRemaining )
   {
      snprintf(szBuff, sizeof(szBuff), "You can't use vehicle radio link %d for relaying. You will not have any remaining downlink link between your controller and this vehicle. Check your radio cards uplink/downlink settings.", iRadioLinkRelay+1);
      addMessage(szBuff);
      return false;
   }
   if ( !bHasTxRemaining )
   {
      snprintf(szBuff, sizeof(szBuff), "You can't use vehicle radio link %d for relaying. You will not have any remaining uplink link between your controller and this vehicle. Check your radio cards uplink/downlink settings.", iRadioLinkRelay+1);
      addMessage(szBuff);
      return false;
   }


   // See if remaining links to vehicle are capable of tx and rx

   bool bHasDataRemaining = false;
   bool bHasVideoRemaining = false;

   for( int i=0; i<g_pSM_RadioStats->countRadioInterfaces; i++ )
   {
      int iRadioLink = g_pSM_RadioStats->radio_interfaces[i].assignedRadioLinkId;           
      if ( iRadioLink == iRadioLinkRelay )
         continue;
      
      radio_hw_info_t* pRadioInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioInfo )
         continue;
      
      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_DISABLED )
         continue;
      if ( controllerIsCardDisabled(pRadioInfo->szMAC) )
         continue;
      
      u32 uCardFlags = controllerGetCardFlags(pRadioInfo->szMAC);

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
      if ( uCardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
         bHasDataRemaining = true;

      if ( g_pCurrentModel->radioLinksParams.link_capabilities_flags[iRadioLink] & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
      if ( uCardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_VIDEO )
         bHasVideoRemaining = true;
   }

   if ( !bHasDataRemaining )
   {
      snprintf(szBuff, sizeof(szBuff), "You can't use vehicle radio link %d for relaying. You will not have any remaining data link between your controller and this vehicle. Check your radio cards capabilities (data/video) settings.", iRadioLinkRelay+1);
      addMessage(szBuff);
      return false;
   }
   if ( !bHasVideoRemaining )
   {
      snprintf(szBuff, sizeof(szBuff), "You can't use vehicle radio link %d for relaying. You will not have any remaining video link between your controller and this vehicle. Check your radio cards capabilities (data/video) settings.", iRadioLinkRelay+1);
      addMessage(szBuff);
      return false;
   }

   type_relay_parameters params;
   memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

   params.isRelayEnabledOnRadioLinkId = iRadioLinkRelay+1;
   params.uCurrentRelayMode = RELAY_MODE_NONE;
   params.uRelayFlags = RELAY_FLAGS_VIDEO | RELAY_FLAGS_SHOW_OSD;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
      valuesToUI();

    return true;
}

void MenuVehicleRelay::onSelectItem()
{
   char szBuff[256];
   ControllerSettings* pCS = get_ControllerSettings();
   Preferences* pP = get_Preferences();
   
   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   if ( NULL == pCS || NULL == pP || (!m_bIsConfigurable) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexItemEnabled == m_SelectedIndex )
   {
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId == m_pItemsSelect[0]->getSelectedIndex() )
         return;

      if ( ! _check_enable_relay(m_pItemsSelect[0]->getSelectedIndex()-1) )
         valuesToUI();
      return;
   }

   if ( m_IndexQAButton == m_SelectedIndex )
   {
      int iOldButton = pCS->iQAButtonRelaySwitching;
      pCS->iQAButtonRelaySwitching = m_pItemsSelect[1]->getSelectedIndex()-1;

      if ( iOldButton == pCS->iQAButtonRelaySwitching )
         return;

      if ( iOldButton > 0 )
      {
         if ( iOldButton == 0 )
            pP->iActionQuickButton1 = quickActionVideoRecord;
         if ( iOldButton == 1 )
            pP->iActionQuickButton2 = quickActionCycleOSD;
         if ( iOldButton == 2 )
            pP->iActionQuickButton3 = quickActionTakePicture;
      }

      if ( pCS->iQAButtonRelaySwitching > 0 )
      {
         if ( pCS->iQAButtonRelaySwitching == 0 )
            pP->iActionQuickButton1 = quickActionRelaySwitch;
         if ( pCS->iQAButtonRelaySwitching == 1 )
            pP->iActionQuickButton2 = quickActionRelaySwitch;
         if ( pCS->iQAButtonRelaySwitching == 2 )
            pP->iActionQuickButton3 = quickActionRelaySwitch;
      }
      
      save_ControllerSettings();
      save_Preferences();
      return;
   }

   if ( m_IndexVehicle == m_SelectedIndex )
   {
      if ( g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId <= 0 )
      {
         addMessage("You need to enable realying on one of the vehicle's radio link first.");
         valuesToUI();
         return;
      }

      int iIndex = m_pItemsSelect[2]->getSelectedIndex();

      if ( 0 == iIndex )
      {
         type_relay_parameters params;
         memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

         params.uRelayVehicleId = 0;
         params.uRelayFrequency = 0;
         params.uRelayFlags = 0;

         if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
            valuesToUI();
         return;
      }

      Model *pModel = findModelWithId(m_uVehiclesID[iIndex-1]);
      if ( NULL == pModel )
      {
         addMessage("Can't get model settings.");
         valuesToUI();
         return;
      }

      char szName[128];
       strlcpy(szName, pModel->getLongName(), sizeof(szName));
      szName[0] = toupper(szName[0]);

      log_line("Check configuration for relaying vehicle %s: ", szName);
      for( int i=0; i<pModel->radioLinksParams.links_count; i++ )
      {
         snprintf(szBuff, sizeof(szBuff), "Vehicle %s (VID: %u) radio link %d of %d: %s;",
             szName, pModel->vehicle_id, i+1, pModel->radioLinksParams.links_count, str_format_frequency(pModel->radioLinksParams.link_frequency[i]));
         log_line(szBuff);
      }
      for( int i=0; i<g_pCurrentModel->radioLinksParams.links_count; i++ )
      {
         snprintf(szBuff, sizeof(szBuff), "Current vehicle (VID %u) radio link %d of %d: %s;",
             g_pCurrentModel->vehicle_id, i+1, g_pCurrentModel->radioLinksParams.links_count, str_format_frequency(g_pCurrentModel->radioLinksParams.link_frequency[i]));
         log_line(szBuff);
      }
      
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         snprintf(szBuff, sizeof(szBuff), "Current vehicle (VID %u) radio interface %d of %d: %s, for radio link %d;",
             g_pCurrentModel->vehicle_id, i+1, g_pCurrentModel->radioInterfacesParams.interfaces_count,
             str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency[i]),
             g_pCurrentModel->radioInterfacesParams.interface_link_id[i]+1);
         log_line(szBuff);
      }
      
      u32 iMainFreqRelayedVehicle = pModel->radioLinksParams.link_frequency[0];

      bool bConflictWithActiveRadioLinks = false;
      int iVehicleCardForRelaying = -1;
      for( int i=0; i<g_pCurrentModel->radioInterfacesParams.interfaces_count; i++ )
      {
         int iRadioLink = g_pCurrentModel->radioInterfacesParams.interface_link_id[i];      
         if ( iRadioLink == g_pCurrentModel->relay_params.isRelayEnabledOnRadioLinkId-1 )
            iVehicleCardForRelaying = i;
         else
         {
            if ( iRadioLink >= 0 && iRadioLink < g_pCurrentModel->radioLinksParams.links_count )
            if ( g_pCurrentModel->radioLinksParams.link_frequency[iRadioLink] == iMainFreqRelayedVehicle )
               bConflictWithActiveRadioLinks = true;
         }
      }

      if ( bConflictWithActiveRadioLinks )
      {
         snprintf(szBuff, sizeof(szBuff), "You can't relay this vehicle (%s) as it's main frequency %s would conflict with your remaining active radio link(s) between your controller and current vehicle (it's on the same frequency).",
            szName, str_format_frequency(iMainFreqRelayedVehicle));
         addMessage(szBuff);
         valuesToUI();
         return;
      }

      if ( -1 == iVehicleCardForRelaying )
      {
         addMessage("No vehicle radio interfaces assigned to the relay radio link.");
         valuesToUI();
         return;
      }

      if ( ! isFrequencyInBands(iMainFreqRelayedVehicle, g_pCurrentModel->radioInterfacesParams.interface_supported_bands[iVehicleCardForRelaying]) )
      {
         snprintf(szBuff, sizeof(szBuff), "Your vehicle relay link does not support the relayed vehicle frequency %s.", str_format_frequency(iMainFreqRelayedVehicle));
         addMessage(szBuff);
         valuesToUI();
         return;
      }

      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

      params.uRelayVehicleId = pModel->vehicle_id;
      params.uRelayFrequency = (u32)iMainFreqRelayedVehicle;
      params.uRelayFlags = RELAY_FLAGS_VIDEO | RELAY_FLAGS_SHOW_OSD;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
         valuesToUI();
      return;
   }

   if ( m_IndexRelayType == m_SelectedIndex )
   {
      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

      params.uRelayFlags &= ~(RELAY_FLAGS_VIDEO | RELAY_FLAGS_TELEMETRY);
      
      if ( 0 == m_pItemsSelect[3]->getSelectedIndex() )
         params.uRelayFlags |= RELAY_FLAGS_VIDEO;
      else if ( 1 == m_pItemsSelect[3]->getSelectedIndex() )
         params.uRelayFlags |= RELAY_FLAGS_TELEMETRY;
      else if ( 2 == m_pItemsSelect[3]->getSelectedIndex() )
         params.uRelayFlags |= RELAY_FLAGS_VIDEO | RELAY_FLAGS_TELEMETRY;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
         valuesToUI();
      return;
   }

   if ( m_IndexOSD == m_SelectedIndex )
   {
      type_relay_parameters params;
      memcpy((u8*)&params, &(g_pCurrentModel->relay_params), sizeof(type_relay_parameters));

      params.uRelayFlags &= (~RELAY_FLAGS_SHOW_OSD);
      if ( 1 == m_pItemsSelect[4]->getSelectedIndex() )
         params.uRelayFlags |= RELAY_FLAGS_SHOW_OSD;
      
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RELAY_PARAMETERS, 0, (u8*)&params, sizeof(type_relay_parameters)) )
         valuesToUI();
   }
}
