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
#include "menu_search.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "../base/config.h"
#include "../base/models.h"
#include "../base/shared_mem.h"
#include "../base/launchers.h"
#include "../radio/radiopackets2.h"
#include "../base/commands.h"
#include "../base/hardware.h"
#include "../base/ctrl_interfaces.h"
#include "shared_vars.h"
#include "menu.h"
#include "popup.h"
#include "pairing.h"
#include "ruby_central.h"
#include "local_stats.h"
#include "timers.h"
#include "events.h"
#include "handle_commands.h"

#include "menu_search_connect.h"
#include "process_router_messages.h"

static int s_LastSearchedFrequency = 0;

MenuSearch::MenuSearch(void)
:Menu(MENU_ID_SEARCH, "Search for vehicles", "")
{
   m_Width = 0.28;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;
   m_SpectatorOnlyMode = false;
   addTopLine("Make sure your vehicle is powered on when you start searching.");
   addTopLine("Note: Long press on [Cancel] key or choose 'Stop Search' to cancel a search in progress.");
   addTopLine("");
   m_pItemSelectBand = new MenuItemSelect("Band:", "Change the frequency bands to search on.");  
   m_pItemSelectBand->addSelection("2.3 Ghz");
   m_pItemSelectBand->addSelection("2.4 Ghz");
   m_pItemSelectBand->addSelection("2.5 Ghz");
   m_pItemSelectBand->addSelection("5.8 Ghz");
   m_pItemSelectBand->setIsEditable();
   addMenuItem(m_pItemSelectBand);
   addMenuItem(new MenuItem("Start Search"));

   hardware_load_radio_info();
   hardware_log_radio_info();
   controllerRadioInterfacesLogInfo();

   m_bIsSearchingManual = false;
   m_bIsSearchingAuto = false;
   m_bIsSearchPaused = false;
   m_bMustSwitchBack = false;
   m_pModelOriginal = g_pCurrentModel;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radio_hw_info_t* pNIC = hardware_get_radio_info(i);
      if ( NULL == pNIC )
         continue;
      m_FrequencyOriginal[i] = pNIC->currentFrequency;
   }

   int selectedIndex = 0;
   char szBuff[32];
   m_NumChannels = 0;

   m_pItemsSelectFreq = new MenuItemSelect("Manual Search", "Manualy search on a particular frequency");
   
   m_SupportedBands = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* nicInfo = hardware_get_radio_info(i);
      if ( NULL == nicInfo || (!nicInfo->isSupported) )
      {
         log_line("Not using radio interface %d for search as it's not supported.", i+1 );
         continue;
      }
      if ( controllerIsCardDisabled(nicInfo->szMAC) )
      {
         log_line("Not using radio interface %d for search as it's disabled.", i+1 );
         continue;
      }
      if ( controllerIsCardTXOnly(nicInfo->szMAC) )
      {
         log_line("Not using radio interface %d for search as it's TX only.", i+1 );
         continue;
      }
      m_SupportedBands = m_SupportedBands | nicInfo->supportedBands;
   }

   int currentFreq = DEFAULT_FREQUENCY;
   if ( NULL == g_pCurrentModel )
   {
       currentFreq = s_LastSearchedFrequency;
       if ( 0 == s_LastSearchedFrequency )
       {
          currentFreq = DEFAULT_FREQUENCY;
          for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
          {
            radio_hw_info_t* nicInfo = hardware_get_radio_info(i);
            if ( nicInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_58 )
               currentFreq = DEFAULT_FREQUENCY58;
          }
      }
   }
   if ( NULL != g_pCurrentModel )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pNIC = hardware_get_radio_info(i);
         u32 cardFlags = controllerGetCardFlags(pNIC->szMAC);
         if ( ! controllerIsCardDisabled(pNIC->szMAC) )
         if ( ! controllerIsCardTXOnly(pNIC->szMAC) )
         if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
            currentFreq = pNIC->currentFrequency;
      }
      if ( 0 == currentFreq )
         currentFreq = DEFAULT_FREQUENCY;
   }

   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_23 )
   {
      for( int i=0; i<getChannels23Count(); i++ )
      {
      m_Channels[m_NumChannels] = getChannels23()[i];
      sprintf(szBuff,"%d Mhz", getChannels23()[i]);
      if ( currentFreq == getChannels23()[i] )
         selectedIndex = m_NumChannels;
      m_pItemsSelectFreq->addSelection(szBuff);
      m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_24 )
   {
      for( int i=0; i<getChannels24Count(); i++ )
      {
      m_Channels[m_NumChannels] = getChannels24()[i];
      sprintf(szBuff,"%d Mhz", getChannels24()[i]);
      if ( currentFreq == getChannels24()[i] )
         selectedIndex = m_NumChannels;
      m_pItemsSelectFreq->addSelection(szBuff);
      m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_25 )
   {
      for( int i=0; i<getChannels25Count(); i++ )
      {
      m_Channels[m_NumChannels] = getChannels25()[i];
      sprintf(szBuff,"%d Mhz", getChannels25()[i]);
      if ( currentFreq == getChannels25()[i] )
         selectedIndex = m_NumChannels;
      m_pItemsSelectFreq->addSelection(szBuff);
      m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_58 )
   {
      for( int i=0; i<getChannels58Count(); i++ )
      {
      m_Channels[m_NumChannels] = getChannels58()[i];
      sprintf(szBuff,"%d Mhz", getChannels58()[i]);
      if ( currentFreq == getChannels58()[i] )
         selectedIndex = m_NumChannels;
      m_pItemsSelectFreq->addSelection(szBuff);
      m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }
   m_pItemsSelectFreq->setSelection(selectedIndex);
   //m_pItemsSelectFreq->disablePopupSelector();
   m_pItemsSelectFreq->setIsEditable();

   addMenuItem(m_pItemsSelectFreq);

   search_finished_with_no_results = false;
   m_nSkippedCount = 0;

   m_ExtraHeight = 3.2*g_pRenderEngine->textHeight(0.8*menu_getScaleMenus()*MENU_FONT_SIZE_TOPLINE, g_idFontMenu);
   computeRenderSizes();
}

void MenuSearch::setSpectatorOnly()
{
   m_SpectatorOnlyMode = true;

   removeAllTopLines();
   addTopLine("Search for vehicles to connect to as spectator only:");
   addTopLine("Note: Long press on [Cancel] or 'Stop Search' to cancel a search in progress.");
   addTopLine("");
}

void MenuSearch::onShow()
{
   render_search_step = -1;
   m_SearchBand = load_simple_config_fileI(FILE_CURRENT_SEARCH_BAND, 100);

   if ( 100 == m_SearchBand )
   {
      if ( m_SupportedBands &  RADIO_HW_SUPPORTED_BAND_58 )
         m_SearchBand = 3;
      else
         m_SearchBand = 1;
   }
   save_simple_config_fileI(FILE_CURRENT_SEARCH_BAND, m_SearchBand);
   m_pItemSelectBand->setSelection(m_SearchBand);

   m_bIsSearchingAuto = false;
   m_bIsSearchingManual = false;
   m_bIsSearchPaused = false;
   g_bSearchFoundVehicle = false;
   Menu::onShow();
}

void MenuSearch::startSearch()
{
   log_line("MenuSearch::startSearch()");

   bool bMatchingNICFound = false;

   m_pSearchChannels = NULL;
   m_SearchChannelsCount = 0;

   if ( m_SearchBand == 0 )
   {
     m_pSearchChannels = getChannels23();
     m_SearchChannelsCount = getChannels23Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* nicInfo = hardware_get_radio_info(i);
        if ( nicInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_23 )
        {
           bMatchingNICFound = true;
           break;
        }
     }
   }
   if ( m_SearchBand == 1 )
   {
     m_pSearchChannels = getChannels24();
     m_SearchChannelsCount = getChannels24Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* nicInfo = hardware_get_radio_info(i);
        if ( nicInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_24 )
        {
           bMatchingNICFound = true;
           break;
        }
     }
   }
   if ( m_SearchBand == 2 )
   {
     m_pSearchChannels = getChannels25();
     m_SearchChannelsCount = getChannels25Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* nicInfo = hardware_get_radio_info(i);
        if ( nicInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_25 )
        {
           bMatchingNICFound = true;
           break;
        }
     }
   }
   if ( m_SearchBand == 3 )
   {
     m_pSearchChannels = getChannels58();
     m_SearchChannelsCount = getChannels58Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* nicInfo = hardware_get_radio_info(i);
        if ( nicInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_58 )
        {
           bMatchingNICFound = true;
           break;
        }
     }
   }

   if ( ! bMatchingNICFound )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Info",NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine("None of your radio cards support this frequency band. Please choose another frequency band to search on.");
      add_menu_to_stack(pm);
      return;
   }
   
   pairing_stop();

   m_bMustSwitchBack = true;
   g_pCurrentModel = NULL;
   enableMenuItem(0, false);
   m_pItemsSelectFreq->setEnabled(false);
   m_pMenuItems[1]->setTitle("Stop Search");

   g_bSearching = true;
   g_iSearchFrequency = 0;
   g_bSearchFoundVehicle = false;
   g_RouterIsReadyTimestamp = 0;
   m_CurrentSearchFrequency = 0;
   m_bIsSearchingManual = false;
   m_bIsSearchingAuto = true;
   m_bIsSearchPaused = false;
   m_nSkippedCount = 0;
   search_finished_with_no_results = false;
   render_search_step = -1;
}

void MenuSearch::stopSearch()
{
   pairing_stop();

   g_bSearching = false;
   g_bSearchFoundVehicle = false;
   g_iSearchFrequency = 0;
   m_CurrentSearchFrequency = 0;
   m_bIsSearchingAuto = false;
   m_bIsSearchingManual = false;
   m_bIsSearchPaused = false;
   render_search_step = -1;

   enableMenuItem(0, true);
   m_pItemsSelectFreq->setEnabled(true);
   m_pMenuItems[1]->setTitle("Search");
   invalidate();
}

void MenuSearch::Render()
{
   RenderPrepare();
   float yTop = Menu::RenderFrameAndTitle();
   float y0 = yTop;
   float y = y0;
   for( int i=0; i<3; i++ )
      y += RenderItem(i,y);

   float height_text = MENU_FONT_SIZE_TOPLINE*menu_getScaleMenus();
   y += height_text*0.4;

   char szBuff[200];
   float width_text = 0;

   if ( search_finished_with_no_results && (! g_bSearchFoundVehicle) )
   {
       g_pRenderEngine->setColors(get_Color_MenuText());
       strcpy(szBuff, "No vehicles found!");
       if ( m_nSkippedCount > 0 )
          strcpy(szBuff, "Vehicle found and skipped.");
       g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus(), y, szBuff, height_text, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   
       y += height_text *(1.0+MENU_ITEM_SPACING);
       
       strcpy(szBuff, "You could search on other bands and channels.");
       if ( m_nSkippedCount > 0 )
          strcpy(szBuff, " ");
       g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus(), y, szBuff, height_text, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
       RenderEnd(yTop);
       return;
   }

   if ( (! m_bIsSearchingManual) && (! m_bIsSearchingAuto) && (!g_bSearchFoundVehicle) )
   {
      RenderEnd(yTop);
      return;
   }

   if ( m_bIsSearchingManual || m_bIsSearchingAuto )
      onSearchStep();

   float x = m_xPos + MENU_MARGINS*menu_getScaleMenus();
       
   g_pRenderEngine->setColors(get_Color_MenuText());

   sprintf(szBuff, "Searching..");
   if ( render_search_step >= 0 )
      for(int k=0; k<(render_search_step%6); k++ )
         strcat(szBuff,".");

   g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus(), y, szBuff, height_text, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   y += height_text *(1.0+MENU_ITEM_SPACING);

   sprintf(szBuff,"Scanning on %d Mhz", m_CurrentSearchFrequency);
   g_pRenderEngine->drawMessageLines(m_xPos+MENU_MARGINS*menu_getScaleMenus(), y, szBuff, height_text, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   y += height_text *(1.0+MENU_ITEM_SPACING);

   if ( g_bSearchFoundVehicle )
   {
   }
   RenderEnd(yTop);
}


void MenuSearch::onSearchStep()
{
   if ( (! m_bIsSearchingManual) && (! m_bIsSearchingAuto) && (!g_bSearchFoundVehicle) )
      return;

   if ( m_bIsSearchPaused)
      return;

   if ( -1 == render_search_step )
   {
      log_line("MenuSearch::onSearchStep() initialized first search step.");
      m_CurrentSearchFrequency = m_pSearchChannels[0];
      g_iSearchFrequency = m_pSearchChannels[0];
      render_search_step = 0;
      return;
   }

   u32 delayMs = DEFAULT_DELAY_WIFI_CHANGE;
   Preferences* pP = get_Preferences();
   if ( NULL != pP )
      delayMs = (u32) pP->iDebugWiFiChangeDelay;
   if ( delayMs<1 || delayMs > 200 )
      delayMs = DEFAULT_DELAY_WIFI_CHANGE;

   int step = (render_search_step/3);
   int substep = (render_search_step%3);
   log_line("MenuSearch::onSearchStep(), step: %d, substep: %d", step, substep);

   // substep 0: update UI and the search frequency value for the next search frequency
   // substep 1: switch to the new frequency
   // substep 2: search on new frequency

   if ( step >= m_SearchChannelsCount )
   {
      log_line("MenuSearch::onSearchStep() reached end step.");
      search_finished_with_no_results = true;
      //printf("\n set sf: %d\n", search_finished_with_no_results);
      stopSearch();
      return;
   }

   if ( substep == 0 )
   {
      m_CurrentSearchFrequency = m_pSearchChannels[step];
      g_iSearchFrequency = m_pSearchChannels[step];
      log_line("Searching - switch UI to frequency for step %d: %d Mhz", step, m_CurrentSearchFrequency);
      render_search_step++;
      return;
   }

   if ( substep == 1 )
   {
      g_RouterIsReadyTimestamp = 0;
      s_LastSearchedFrequency = m_CurrentSearchFrequency;
      if ( step == 0 )
      {
         log_line("MenuSearch::onSearchStep() start searching processes...");
         pairing_start();
         hardware_sleep_ms(delayMs);
      }
      else
      {
         log_line("MenuSearch::onSearchStep() send command to router to change frequency to %d Mhz", m_CurrentSearchFrequency );
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED, m_CurrentSearchFrequency );
      }
      render_search_step++;
      return;
   }

   if ( substep == 2 )
   {
      if ( 0 == g_RouterIsReadyTimestamp )
      {
         log_line("MenuSearch::onSearchStep(): waiting for router to be ready...");
         return;
      }
      if ( g_TimeNow > g_RouterIsReadyTimestamp + 3*1100/DEFAULT_RUBY_TELEMETRY_UPDATE_RATE )
      {
         log_line("MenuSearch::onSearchStep(): Nothing found on %d Mhz after %d ms of waiting.", m_CurrentSearchFrequency, 3*1100/DEFAULT_RUBY_TELEMETRY_UPDATE_RATE);
         render_search_step++;
         return;
      }

      bool bVehicleIsOnCurrentFreq = false;
      if ( (NULL != g_pPHRTE) && (g_pPHRTE->vehicle_id != 0) )
      {
         char szTmpBuff[256];
         szTmpBuff[0] = 0;
         for( int i=0; i<g_pPHRTE->radio_links_count; i++ )
         {
            char szTmp2[64];
            sprintf(szTmp2, "%d Mhz ", g_pPHRTE->radio_frequencies[i]);
            strcat(szTmpBuff, szTmp2);
         }
         log_line("MenuSearch: There is a vehicle on current frequency. Vehicle id: %u, has %d radio links: %s.", g_pPHRTE->vehicle_id, g_pPHRTE->radio_links_count, szTmpBuff);
         for( int i=0; i<g_pPHRTE->radio_links_count; i++ )
            if ( g_pPHRTE->radio_frequencies[i] == m_CurrentSearchFrequency )
               bVehicleIsOnCurrentFreq = true;
      }
      if ( bVehicleIsOnCurrentFreq )
      {
         u8 vMaj = g_pPHRTE->version;
         u8 vMin = g_pPHRTE->version;
         vMaj = vMaj >> 4;
         vMin = vMin & 0x0F;
   
         log_line("MenuSearch::onSearchStep() Found a vehicle while searching on %d Mhz: vehicle ID: %u, version: %d.%d, radio links: %d Mhz, %d Mhz, %d Mhz", m_CurrentSearchFrequency, g_pPHRTE->vehicle_id, vMaj, vMin, g_pPHRTE->radio_links_count, g_pPHRTE->radio_frequencies[0], g_pPHRTE->radio_frequencies[1], g_pPHRTE->radio_frequencies[2] );
         m_bIsSearchPaused = true;
         g_bSearchFoundVehicle = true;
         m_iConfirmationId = 1;
         invalidate();
         setTooltip("");
         MenuSearchConnect* pMenu = new MenuSearchConnect();
         pMenu->setCurrentFrequency(m_CurrentSearchFrequency);
         if ( m_SpectatorOnlyMode )
            pMenu->setSpectatorOnly();
         add_menu_to_stack(pMenu);
         log_line("Added connect menu to stack");
      }
      else if ( g_pPHRTE->vehicle_id != 0 )
         log_softerror_and_alarm("Found a vehicle that emits on %d Mhz on current search frequencty %d Mhz!", g_pPHRTE->radio_frequencies[0], m_CurrentSearchFrequency );
   }
}

void MenuSearch::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   if ( 1 == m_iConfirmationId )
   {
      m_iConfirmationId = 0;

      // Skip
      if ( 0 == returnValue )
      {
         m_nSkippedCount++;
         log_line("Pressed Skip search");
         m_bIsSearchPaused = false;
         g_bSearchFoundVehicle = false;
         invalidate();
         return;
      }

      // Connect Spectator
      if ( 1 == returnValue )
      {
         bool bIsNew = false;
         log_line("Pressed add as spectator");
         if( ! ruby_is_first_pairing_done() )
         {
            deleteAllModels();
            bIsNew = true;
         }
         saveModel(g_pCurrentModel);

         if ( ! hasModel(g_pPHRTE->vehicle_id) )
            bIsNew = true;

         Model* pModel = addModelSpectator(g_pPHRTE->vehicle_id);
         pModel->populateFromVehicleTelemetryData(g_pPHRTE);
         pModel->is_spectator = true;

         stopSearch();
         pModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, true);
         saveModelsSpectator();
         ruby_set_is_first_pairing_done();
         g_pCurrentModel = pModel;
         if ( bIsNew )
            onModelAdded(pModel->vehicle_id);
         onNewVehicleActive();
         popups_remove_all();
         menu_close_all();
         render_all(g_TimeNow);
         log_line("Added vehicle id: %u as spectator.", pModel->vehicle_id);
         pairing_start();
         return;
      }

      // Connect Controll
      if ( 2 == returnValue )
      {
         bool bIsNew = false;
         log_line("Pressed add as controller");
         if( ! ruby_is_first_pairing_done() )
         {
            deleteAllModels();
            bIsNew = true;
         }
         saveModel(g_pCurrentModel);

         if ( ! hasModel(g_pPHRTE->vehicle_id) )
            bIsNew = true;

         log_line("Creating a vehicle model for vehicle uid: %u ...", g_pPHRTE->vehicle_id);
         Model* pModel = findModelWithId(g_pPHRTE->vehicle_id);
         if ( NULL == pModel )
         {
            log_line("Search: Added a new vehicle model as controller");
            pModel = addNewModel();
         }
         else
            log_line("Search: reused existing vehicle model as controller");

         pModel->populateFromVehicleTelemetryData(g_pPHRTE);
         pModel->is_spectator = false;
         stopSearch();
         log_line("Search save model: mode: %s", (pModel->is_spectator)?"spectator":"control");
         pModel->saveToFile(FILE_CURRENT_VEHICLE_MODEL, true);
         saveModels();
         ruby_set_is_first_pairing_done();

         g_bIsFirstConnectionToCurrentVehicle = true;
         g_pCurrentModel = pModel;
         g_pCurrentModel->b_mustSyncFromVehicle = true;
         if ( bIsNew )
            onModelAdded(pModel->vehicle_id);
         onNewVehicleActive();
         popups_remove_all();
         menu_close_all();
         render_all(g_TimeNow);
         log_line("Added vehicle uid: %u as a controller.", pModel->vehicle_id);
         pairing_start(); 
         return;
      }
   }
   m_iConfirmationId = 0;
}

int MenuSearch::onBack()
{
   log_line("MenuSearch::onBack()");
   if ( m_pItemSelectBand->isEditing() )
   {
      log_line("MenuSearch::onBack() end edit");
      m_pItemSelectBand->endEdit(true);
      return 1;
   }

   if ( m_pItemsSelectFreq->isEditing() )
   {
      log_line("MenuSearch::onBack() end edit2");
      search_finished_with_no_results = false;
      m_pItemsSelectFreq->endEdit(true);
      return 1;
   }
   search_finished_with_no_results = false;
   if ( m_bIsSearchingManual || m_bIsSearchingAuto )
   {
      log_line("MenuSearch::onBack() stop search");
      hardware_blockCurrentPressedKeys();
      stopSearch();
      return 1;
   }

   // Switch back to the original vehicle when we opened the search menu
   if ( m_bMustSwitchBack )
   {
      log_line("MenuSearch::onBack() switch back");
      pairing_stop();
      g_pCurrentModel = m_pModelOriginal;
      m_bMustSwitchBack = false;
      pairing_start();
   }
   log_line("Default back");
   return 0;
}

void MenuSearch::onSelectItem()
{
   Menu::onSelectItem();
   if ( 0 == m_SelectedIndex )
   {
      m_SearchBand = m_pItemSelectBand->getSelectedIndex();
      save_simple_config_fileI(FILE_CURRENT_SEARCH_BAND, m_SearchBand);
      return;
   }

   // Start/Stop Search
   if ( 1 == m_SelectedIndex )
   {
      log_line("Pressed Star/Stop search button");
      if ( ! m_bIsSearchingAuto )
         startSearch();
      else
      {
         hardware_blockCurrentPressedKeys();
         stopSearch();
      }
      return;
   }

   // Manual search on a frequency
   if ( 2 == m_SelectedIndex && ! m_pItemsSelectFreq->isEditing() )
   {
      pairing_stop();

      log_line("Pressed manual search button");
      m_bMustSwitchBack = true;
      g_pCurrentModel = NULL;
      m_CurrentSearchFrequency = 0;
      g_iSearchFrequency = 0;
      m_pSearchChannels = &(m_Channels[m_pItemsSelectFreq->getSelectedIndex()]);
      m_SearchChannelsCount = 1;

      log_line("Searching only on %d Mhz", m_pSearchChannels[0]);

      m_bIsSearchingManual = true;
      m_bIsSearchingAuto = false;
      m_bIsSearchPaused = false;
      g_bSearching = true;
      g_bSearchFoundVehicle = false;
      g_RouterIsReadyTimestamp = 0;
      render_search_step = -1;
      search_finished_with_no_results = false;
      return;
   }
}
