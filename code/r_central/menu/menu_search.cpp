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

#include "menu_search.h"
#include "menu.h"
#include "../../common/models_connect_frequencies.h"

#include "menu_item_text.h"
#include "menu_search_connect.h"
#include "../process_router_messages.h"
#include "../../base/utils.h"
#include "../link_watch.h"

static int s_LastSearchedFrequency = 0;

MenuSearch::MenuSearch(void)
:Menu(MENU_ID_SEARCH, "Search for vehicles", "")
{
   m_Width = 0.30;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.2;
   m_SpectatorOnlyMode = false;
   addTopLine("Make sure your vehicle is powered on when you start searching.");
   addTopLine("Note: Long press on [Cancel] key or choose 'Stop Search' to cancel a search in progress.");
   addTopLine("");

   m_pPopupSearch = NULL;

   m_SupportedBands = 0;
   m_iCountSupportedBands = 0;

   m_bHasSiKRadio = false;
   u32 uSiKBands = 0;
   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( (NULL == pRadioHWInfo) || (!pRadioHWInfo->isSupported) )
      {
         log_line("Not using radio interface %d for search as it's not supported.", i+1 );
         continue;
      }
      if ( controllerIsCardDisabled(pRadioHWInfo->szMAC) )
      {
         log_line("Not using radio interface %d for search as it's disabled.", i+1 );
         continue;
      }

      if ( hardware_radio_is_sik_radio(pRadioHWInfo) )
      {
         m_bHasSiKRadio = true;
         uSiKBands |= pRadioHWInfo->supportedBands;
      }

      if ( controllerIsCardTXOnly(pRadioHWInfo->szMAC) )
      {
         log_line("Not using radio interface %d for search as it's TX only.", i+1 );
         continue;
      }
      m_SupportedBands = m_SupportedBands | pRadioHWInfo->supportedBands;
   }

   if ( m_bHasSiKRadio )
    m_Width = 0.38;
   
   for( int i=0; i<10; i++ )
      m_pItemsSelect[i] = 0;

   m_IndexSiKInfo = -1;
   m_IndexSiKAirRate = -1;
   m_IndexSiKECC = -1;
   m_IndexSiKLBT = -1;
   m_IndexSiKMCSTR = -1;

   m_IndexBand = 0;
   m_IndexStartSearch = 1;
   m_IndexManualSearch = 2;

   log_line("MenuSearch: Has SiK radios? %s", m_bHasSiKRadio?"yes":"no");

   m_pItemSelectBand = new MenuItemSelect("Band:", "Change the frequency bands to search on.");
   
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_433 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_433;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("433 Mhz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_868 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_868;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("868 Mhz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_915 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_915;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("915 Mhz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_23 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_23;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("2.3 Ghz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_24 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_24;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("2.4 Ghz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_25 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_25;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("2.5 Ghz");
   }
   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_58 )
   {
      m_SupportedBandsList[m_iCountSupportedBands] = RADIO_HW_SUPPORTED_BAND_58;
      m_iCountSupportedBands++;
      m_pItemSelectBand->addSelection("5.8 Ghz");
   }

   for( int i=0; i<m_iCountSupportedBands; i++ )
      log_line("MenuSearch() Supported search bands (%d of %d): %s", i+1, m_iCountSupportedBands, str_getBandName(m_SupportedBandsList[i]));
   m_pItemSelectBand->setIsEditable();
   addMenuItem(m_pItemSelectBand);
   addMenuItem(new MenuItem("Start Search", "Start/Stop searching for vehicles on current band."));

   log_line("MenuSearch: On open, this is the current radio interfaces configuration:");
   hardware_load_radio_info();

   controllerRadioInterfacesLogInfo();

   m_bIsSearchingManual = false;
   m_bIsSearchingAuto = false;
   m_bIsSearchingSiK = false;
   m_bIsSearchPaused = false;
   m_bMustSwitchBack = false;
   m_bDidConnectToAVehicle = false;

   m_pModelOriginal = g_pCurrentModel;
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      radio_hw_info_t* pRadioInterface = hardware_get_radio_info(i);
      if ( NULL == pRadioInterface )
         continue;
      m_FrequencyOriginal[i] = pRadioInterface->uCurrentFrequencyKhz;
   }

   _populate_search_frequencies();

   if ( m_bHasSiKRadio )
   {
      float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
  
      char szBuff[256];
      char szBands[128];
      str_get_supported_bands_string(uSiKBands, szBands);
      sprintf(szBuff, "SiK parameters (for searching on %s)", szBands);
      
      MenuItemText* pItem = new MenuItemText(szBuff);
      pItem->setMargin(0.0);
      m_IndexSiKInfo = addMenuItem(pItem);

      m_pItemsSelect[2] = new MenuItemSelect("SiK Radio Data Rate", "Sets the physical radio air data rate to use while searching.");
      
      sprintf(szBuff, "Default (%d kbps)", DEFAULT_RADIO_DATARATE_SIK_AIR/1000);
      m_pItemsSelect[2]->addSelection(szBuff);

      for( int i=0; i<getSiKAirDataRatesCount(); i++ )
      {
         sprintf(szBuff, "%d kbps", (getSiKAirDataRates()[i])/1000);
         m_pItemsSelect[2]->addSelection(szBuff);
      }
      
      m_pItemsSelect[2]->setIsEditable();
      m_pItemsSelect[2]->setMargin(height_text);
      m_IndexSiKAirRate = addMenuItem(m_pItemsSelect[2]);


      m_pItemsSelect[3] = new MenuItemSelect("SiK Error Correction", "Enables hardware error correction flag while searching.");
      m_pItemsSelect[3]->addSelection("No (Default)");
      m_pItemsSelect[3]->addSelection("Yes");
      m_pItemsSelect[3]->setIsEditable();
      m_pItemsSelect[3]->setMargin(height_text);
      m_IndexSiKECC = addMenuItem(m_pItemsSelect[3]);

      m_pItemsSelect[4] = new MenuItemSelect("SiK Listen Before Talk", "Enables listen before talk flag while searching.");
      m_pItemsSelect[4]->addSelection("No (Default)");
      m_pItemsSelect[4]->addSelection("Yes");
      m_pItemsSelect[4]->setIsEditable();
      m_pItemsSelect[4]->setMargin(height_text);
      m_IndexSiKLBT = addMenuItem(m_pItemsSelect[4]);

      m_pItemsSelect[5] = new MenuItemSelect("SiK Enable Manchester Encoding", "Enables Manchester type of encoding flag while searching.");
      m_pItemsSelect[5]->addSelection("No (Default)");
      m_pItemsSelect[5]->addSelection("Yes");
      m_pItemsSelect[5]->setIsEditable();
      m_pItemsSelect[5]->setMargin(height_text);
      m_IndexSiKMCSTR = addMenuItem(m_pItemsSelect[5]);
   }

   search_finished_with_no_results = false;
   m_nSkippedCount = 0;

   m_ExtraItemsHeight = 3.2*g_pRenderEngine->textHeight(g_idFontMenu);
   computeRenderSizes();
}

void MenuSearch::valuesToUI()
{
   if ( m_bHasSiKRadio )
   {
      ControllerSettings* pCS = get_ControllerSettings();
      if ( -1 != m_IndexSiKAirRate )
      {
          m_pItemsSelect[2]->setSelectedIndex(0);
          for( int i=0; i<getSiKAirDataRatesCount(); i++ )
          {
             if ( pCS->iSearchSiKAirRate != DEFAULT_RADIO_DATARATE_SIK_AIR )
             if ( pCS->iSearchSiKAirRate == getSiKAirDataRates()[i] )
             {
                m_pItemsSelect[2]->setSelectedIndex(i+1);
                break;
             }
         }
      }

      if ( -1 != m_IndexSiKECC )
      {
         m_pItemsSelect[3]->setSelectedIndex(0);
         if ( pCS->iSearchSiKECC )
            m_pItemsSelect[3]->setSelectedIndex(1);
      }
      if ( -1 != m_IndexSiKLBT )
      {
         m_pItemsSelect[4]->setSelectedIndex(0);
         if ( pCS->iSearchSiKLBT )
            m_pItemsSelect[4]->setSelectedIndex(1);
      }
      if ( -1 != m_IndexSiKMCSTR )
      {
         m_pItemsSelect[5]->setSelectedIndex(0);
         if ( pCS->iSearchSiKMCSTR )
            m_pItemsSelect[5]->setSelectedIndex(1);
      }
   }
}

void MenuSearch::_populate_search_frequencies()
{
   int selectedIndex = 0;
   char szBuff[32];

   m_NumChannels = 0;

   m_pItemsSelectFreq = new MenuItemSelect("Manual Search", "Manualy search on a particular frequency");
   
   u32 currentFreqKhz = DEFAULT_FREQUENCY;

   if ( NULL == g_pCurrentModel )
   {
       currentFreqKhz = s_LastSearchedFrequency;
       if ( 0 == s_LastSearchedFrequency )
       {
          currentFreqKhz = DEFAULT_FREQUENCY;
          for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
          {
            radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
            if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_58 )
               currentFreqKhz = DEFAULT_FREQUENCY58;
          }
      }
   }
   if ( NULL != g_pCurrentModel )
   {
      for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
      {
         radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
         u32 cardFlags = controllerGetCardFlags(pRadioHWInfo->szMAC);
         if ( ! controllerIsCardDisabled(pRadioHWInfo->szMAC) )
         if ( ! controllerIsCardTXOnly(pRadioHWInfo->szMAC) )
         if ( cardFlags & RADIO_HW_CAPABILITY_FLAG_CAN_USE_FOR_DATA )
            currentFreqKhz = pRadioHWInfo->uCurrentFrequencyKhz;
      }
      if ( 0 == currentFreqKhz )
         currentFreqKhz = DEFAULT_FREQUENCY;
   }

   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_433 )
   {
      for( int i=0; i<getChannels433Count(); i++ )
      {
         m_Channels[m_NumChannels] = getChannels433()[i];
         strcpy(szBuff, str_format_frequency(getChannels433()[i]));
         if ( currentFreqKhz == getChannels433()[i] )
            selectedIndex = m_NumChannels;
         m_pItemsSelectFreq->addSelection(szBuff);
         m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }

   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_868 )
   {
      for( int i=0; i<getChannels868Count(); i++ )
      {
         m_Channels[m_NumChannels] = getChannels868()[i];
         strcpy(szBuff, str_format_frequency(getChannels868()[i]));
         if ( currentFreqKhz == getChannels868()[i] )
            selectedIndex = m_NumChannels;
         m_pItemsSelectFreq->addSelection(szBuff);
         m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }

   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_915 )
   {
      for( int i=0; i<getChannels915Count(); i++ )
      {
      m_Channels[m_NumChannels] = getChannels915()[i];
      strcpy(szBuff, str_format_frequency(getChannels915()[i]));
      if ( currentFreqKhz == getChannels915()[i] )
         selectedIndex = m_NumChannels;
      m_pItemsSelectFreq->addSelection(szBuff);
      m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }

   if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_23 )
   {
      for( int i=0; i<getChannels23Count(); i++ )
      {
      m_Channels[m_NumChannels] = getChannels23()[i];
      strcpy(szBuff, str_format_frequency(getChannels23()[i]));
      if ( currentFreqKhz == getChannels23()[i] )
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
      strcpy(szBuff,str_format_frequency(getChannels24()[i]));
      if ( currentFreqKhz == getChannels24()[i] )
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
         strcpy(szBuff, str_format_frequency(getChannels25()[i]));
         if ( currentFreqKhz == getChannels25()[i] )
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
         strcpy(szBuff, str_format_frequency(getChannels58()[i]));
         if ( currentFreqKhz == getChannels58()[i] )
            selectedIndex = m_NumChannels;
         m_pItemsSelectFreq->addSelection(szBuff);
         m_NumChannels++;
      }
      m_pItemsSelectFreq->addSeparator();
   }
   m_pItemsSelectFreq->setSelection(selectedIndex);
   m_pItemsSelectFreq->setIsEditable();

   addMenuItem(m_pItemsSelectFreq);
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
   m_SearchBandIndex = load_simple_config_fileI(FILE_CURRENT_SEARCH_BAND, 100);

   if ( (100 == m_SearchBandIndex) || (m_SearchBandIndex<0) || (m_SearchBandIndex >=m_iCountSupportedBands) )
   {
      if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_58 )
      {
         for( int i=0; i<m_iCountSupportedBands; i++ )
         {
            if ( m_SupportedBandsList[i] == RADIO_HW_SUPPORTED_BAND_58 )
            {
               m_SearchBandIndex = i;
               break;
            }
         }
      }
      else if ( m_SupportedBands & RADIO_HW_SUPPORTED_BAND_24 )
      {
         for( int i=0; i<m_iCountSupportedBands; i++ )
         {
            if ( m_SupportedBandsList[i] == RADIO_HW_SUPPORTED_BAND_24 )
            {
               m_SearchBandIndex = i;
               break;
            }
         }
      }
      else
         m_SearchBandIndex = m_iCountSupportedBands-1;
   }
   if ( m_SearchBandIndex >= m_iCountSupportedBands )
      m_SearchBandIndex = m_iCountSupportedBands-1;
   save_simple_config_fileI(FILE_CURRENT_SEARCH_BAND, m_SearchBandIndex);
   m_pItemSelectBand->setSelection(m_SearchBandIndex);

   m_bIsSearchingAuto = false;
   m_bIsSearchingManual = false;
   m_bIsSearchingSiK = false;
   m_bIsSearchPaused = false;
   g_bSearchFoundVehicle = false;
   Menu::onShow();
}

void MenuSearch::startSearch()
{
   log_line("MenuSearch::startSearch() on band %s", str_getBandName(m_SupportedBandsList[m_SearchBandIndex]));

   for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      char szTmp[128];
      str_get_supported_bands_string(pRadioHWInfo->supportedBands, szTmp);
      log_line("MenuSearch: Radio interface %d supported bands: %s", i+1, szTmp);
   }

   if ( NULL != m_pPopupSearch )
   {
      popups_remove(m_pPopupSearch);
      m_pPopupSearch = NULL;
   }

   bool bFoundSupportedRadioInterface = false;

   g_iCurrentActiveVehicleRuntimeInfoIndex = 0;
   
   m_pSearchChannels = NULL;
   m_SearchChannelsCount = 0;
   m_bIsSearchingSiK = false;
   
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_433 )
   {
     m_pSearchChannels = getChannels433();
     m_SearchChannelsCount = getChannels433Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_433 )
        {
           bFoundSupportedRadioInterface = true;
           if ( m_bHasSiKRadio )
              m_bIsSearchingSiK = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_868 )
   {
     m_pSearchChannels = getChannels868();
     m_SearchChannelsCount = getChannels868Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_868 )
        {
           bFoundSupportedRadioInterface = true;
           if ( m_bHasSiKRadio )
              m_bIsSearchingSiK = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_915 )
   {
     m_pSearchChannels = getChannels915();
     m_SearchChannelsCount = getChannels915Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_915 )
        {
           bFoundSupportedRadioInterface = true;
           if ( m_bHasSiKRadio )
              m_bIsSearchingSiK = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_23 )
   {
     m_pSearchChannels = getChannels23();
     m_SearchChannelsCount = getChannels23Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_23 )
        {
           bFoundSupportedRadioInterface = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_24 )
   {
     m_pSearchChannels = getChannels24();
     m_SearchChannelsCount = getChannels24Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_24 )
        {
           bFoundSupportedRadioInterface = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_25 )
   {
     m_pSearchChannels = getChannels25();
     m_SearchChannelsCount = getChannels25Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_25 )
        {
           bFoundSupportedRadioInterface = true;
           break;
        }
     }
   }
   if ( m_SupportedBandsList[m_SearchBandIndex] == RADIO_HW_SUPPORTED_BAND_58 )
   {
     m_pSearchChannels = getChannels58();
     m_SearchChannelsCount = getChannels58Count();
     for( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
     {
        radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
        if ( pRadioHWInfo->supportedBands & RADIO_HW_SUPPORTED_BAND_58 )
        {
           bFoundSupportedRadioInterface = true;
           break;
        }
     }
   }

   if ( ! bFoundSupportedRadioInterface )
   {
      Menu* pm = new Menu(MENU_ID_SIMPLE_MESSAGE,"Info",NULL);
      pm->m_xPos = 0.4; pm->m_yPos = 0.4;
      pm->m_Width = 0.36;
      pm->addTopLine("None of your radio cards support this frequency band. Please choose another frequency band to search on.");
      add_menu_to_stack(pm);
      return;
   }
   
   g_bSearching = true;
   g_iSearchFrequency = 0;
   g_bSearchFoundVehicle = false;
   g_RouterIsReadyTimestamp = 0;
   g_pCurrentModel = NULL;

   link_watch_remove_popups();
   render_all(get_current_timestamp_ms(), true);

   createSearchPopup();

   render_all(get_current_timestamp_ms(), true);
   pairing_stop();
   render_all(get_current_timestamp_ms(), true);

   if ( ! m_bDidConnectToAVehicle )
      m_bMustSwitchBack = true;
   
   if ( m_bHasSiKRadio )
   {
      enableMenuItem(m_IndexSiKInfo, false);
      enableMenuItem(m_IndexSiKAirRate, false);
      enableMenuItem(m_IndexSiKECC, false);
      enableMenuItem(m_IndexSiKLBT, false);
      enableMenuItem(m_IndexSiKMCSTR, false);
   }
   enableMenuItem(m_IndexBand, false);
   m_pItemsSelectFreq->setEnabled(false);
   m_pMenuItems[m_IndexStartSearch]->setTitle("Stop Search");

   handle_commands_abandon_command();
   reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
   
   m_CurrentSearchFrequencyKhz = 0;
   m_bIsSearchingManual = false;
   m_bIsSearchingAuto = true;
   m_bIsSearchPaused = false;
   m_nSkippedCount = 0;

   
   search_finished_with_no_results = false;
   render_search_step = -1;
}

void MenuSearch::stopSearch()
{
   log_line("MenuSearch: Stopping search...");

   if ( m_pPopupSearch )
   {
      popups_remove( m_pPopupSearch );
      m_pPopupSearch = NULL;
   }

   pairing_stop();

   log_line("MenuSearch: Controller radio interfaces configuration after stopping search:");
   hardware_load_radio_info();

   g_bSearching = false;
   g_bSearchFoundVehicle = false;
   g_iSearchFrequency = 0;
   m_CurrentSearchFrequencyKhz = 0;
   m_bIsSearchingAuto = false;
   m_bIsSearchingManual = false;
   m_bIsSearchingSiK = false;
   m_bIsSearchPaused = false;
   render_search_step = -1;

   reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);

   if ( m_bHasSiKRadio )
   {
      enableMenuItem(m_IndexSiKInfo, true);
      enableMenuItem(m_IndexSiKAirRate, true);
      enableMenuItem(m_IndexSiKECC, true);
      enableMenuItem(m_IndexSiKLBT, true);
      enableMenuItem(m_IndexSiKMCSTR, true);
   }

   enableMenuItem(m_IndexBand, true);
   m_pItemsSelectFreq->setEnabled(true);
   m_pMenuItems[m_IndexStartSearch]->setTitle("Start Search");
   invalidate();
   log_line("MenuSearch: Stopped search. Complete.");
}

void MenuSearch::Render()
{
   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   
   RenderPrepare();
   float yTop = Menu::RenderFrameAndTitle();
   float y0 = yTop;
   float y = y0;
   for( int i=0; i<m_ItemsCount; i++ )
   {
      y += RenderItem(i,y);
      if ( y == m_IndexManualSearch )
         y += height_text*0.3;
   }
   y += height_text*0.4;

   char szBuff[200];
   float width_text = 0;

   if ( search_finished_with_no_results && (! g_bSearchFoundVehicle) )
   {
       g_pRenderEngine->setColors(get_Color_MenuText());
       strcpy(szBuff, "No vehicles found!");
       if ( m_nSkippedCount > 0 )
          strcpy(szBuff, "Vehicle found and skipped.");
       g_pRenderEngine->drawMessageLines(m_xPos+m_sfMenuPaddingX, y, szBuff, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   
       y += height_text *(1.0+MENU_ITEM_SPACING);
       
       strcpy(szBuff, "You could search on other bands and channels.");
       if ( m_nSkippedCount > 0 )
          strcpy(szBuff, " ");
       g_pRenderEngine->drawMessageLines(m_xPos+m_sfMenuPaddingX, y, szBuff, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
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

   float x = m_xPos + m_sfMenuPaddingX;
       
   g_pRenderEngine->setColors(get_Color_MenuText());

   sprintf(szBuff, "Searching..");
   if ( render_search_step >= 0 )
      for(int k=0; k<(render_search_step%6); k++ )
         strcat(szBuff,".");

   if ( NULL != m_pPopupSearch )
   {
      char szTitle[128];
      strcpy(szTitle, m_pPopupSearch->getTitle());
      int iLen = strlen(szTitle)-1;
      while ( (iLen > 0) && ((szTitle[iLen] == '.') || (szTitle[iLen] == '|')) )
      {
         szTitle[iLen] = 0;
         iLen--;
      }
      int k=0;
      int pos = (render_search_step%8);
      while ( k < pos-1 )
      {
         strcat(szTitle,".");
         k++;
      }
      strcat(szTitle, "|");
      k++;
      while ( k < 8 )
      {
         strcat(szTitle, ".");
         k++;
      }
      m_pPopupSearch->setTitle(szTitle);
   }

   g_pRenderEngine->drawMessageLines(m_xPos+m_sfMenuPaddingX, y, szBuff, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
   y += height_text *(1.0+MENU_ITEM_SPACING);

   sprintf(szBuff,"Scanning on %s", str_format_frequency(m_CurrentSearchFrequencyKhz));
   g_pRenderEngine->drawMessageLines(m_xPos+m_sfMenuPaddingX, y, szBuff, MENU_TEXTLINE_SPACING, getUsableWidth(), g_idFontMenu);
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
      reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
      log_line("MenuSearch::onSearchStep() initialized first search step.");
      m_CurrentSearchFrequencyKhz = m_pSearchChannels[0];
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
      reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
      return;
   }

   // substep 0: update UI and the search frequency value for the next search frequency
   if ( substep == 0 )
   {
      m_CurrentSearchFrequencyKhz = m_pSearchChannels[step];
      g_iSearchFrequency = m_pSearchChannels[step];
      log_line("Searching - switch UI to frequency for step %d: %s", step, str_format_frequency(m_CurrentSearchFrequencyKhz));
      if ( NULL != m_pPopupSearch )
      {
         char szTitle[128];
         sprintf(szTitle, "Searching on %s ...", str_format_frequency(m_CurrentSearchFrequencyKhz));     
         m_pPopupSearch->setTitle(szTitle);
      }
      render_search_step++;
      return;
   }

   // substep 1: switch to the new frequency
   if ( substep == 1 )
   {
      g_RouterIsReadyTimestamp = 0;
      s_LastSearchedFrequency = m_CurrentSearchFrequencyKhz;
      reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);
      if ( step == 0 )
      {
         int iBand = getBand(m_CurrentSearchFrequencyKhz);
         log_line("MenuSearch::onSearchStep() start searching processes (for freq %s and band %s)...",
            str_format_frequency(m_CurrentSearchFrequencyKhz), str_getBandName(iBand));
         if ( m_bHasSiKRadio && m_bIsSearchingSiK &&
             ( iBand == RADIO_HW_SUPPORTED_BAND_433 || iBand == RADIO_HW_SUPPORTED_BAND_868 || iBand == RADIO_HW_SUPPORTED_BAND_915 ) )
         {
             int iAirDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
             if ( m_pItemsSelect[2]->getSelectedIndex() > 0 )
                iAirDataRate = getSiKAirDataRates()[m_pItemsSelect[2]->getSelectedIndex()-1];
             int iECC = m_pItemsSelect[3]->getSelectedIndex();
             int iLBT = m_pItemsSelect[4]->getSelectedIndex();
             int iMCSTR = m_pItemsSelect[5]->getSelectedIndex();
             pairing_start_search_sik_mode(m_CurrentSearchFrequencyKhz, iAirDataRate, iECC, iLBT, iMCSTR);
         }  
         else
            pairing_start_search_mode(m_CurrentSearchFrequencyKhz);

         hardware_sleep_ms(delayMs);
      }
      else
      {
         log_line("MenuSearch::onSearchStep() send command to router to change frequency to %s", str_format_frequency(m_CurrentSearchFrequencyKhz) );
         send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROLLER_SEARCH_FREQ_CHANGED, m_CurrentSearchFrequencyKhz );
      }
      render_search_step++;
      return;
   }

   // substep 2: search on new frequency
   if ( substep == 2 )
   {
      if ( 0 == g_RouterIsReadyTimestamp )
      {
         log_line("MenuSearch::onSearchStep(): waiting for router to be ready...");
         return;
      }
      char szBands[128];
      str_get_supported_bands_string(getBand(m_CurrentSearchFrequencyKhz), szBands);
      log_line("MenuSearch::onSearchStep() Current search frequency: %s, band: %s, have SiK radios: %d", str_format_frequency(m_CurrentSearchFrequencyKhz), szBands, hardware_radio_has_sik_radios());
      
      u32 uWaitTimeMs = 3*1100/DEFAULT_RUBY_TELEMETRY_UPDATE_RATE;
      if ( hardware_radio_has_sik_radios() )
      if ( (getBand(m_CurrentSearchFrequencyKhz) == RADIO_HW_SUPPORTED_BAND_433) ||
           (getBand(m_CurrentSearchFrequencyKhz) == RADIO_HW_SUPPORTED_BAND_868) ||
           (getBand(m_CurrentSearchFrequencyKhz) == RADIO_HW_SUPPORTED_BAND_915) )
      {
         uWaitTimeMs = 1500;
         log_line("MenuSearch::onSearchStep() Searching on 433/868/915 Mhz band and we have Sik radios. Increase search time to %u ms", uWaitTimeMs);
      }

      if ( g_TimeNow > g_RouterIsReadyTimestamp + uWaitTimeMs )
      {
         log_line("MenuSearch::onSearchStep(): Nothing found on %s after %d ms of waiting.", str_format_frequency(m_CurrentSearchFrequencyKhz), 3*1100/DEFAULT_RUBY_TELEMETRY_UPDATE_RATE);
         render_search_step++;
         return;
      }

      bool bVehicleIsOnCurrentFreq = false;
      if ( (g_SearchVehicleRuntimeInfo.bGotRubyTelemetryInfo) && (g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id != 0) )
      {
         char szTmpBuff[256];
         szTmpBuff[0] = 0;
         for( int i=0; i<g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count; i++ )
         {
            char szTmp2[64];
            sprintf(szTmp2, "%s ", str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[i]) );
            strcat(szTmpBuff, szTmp2);
         }
         log_line("MenuSearch: There is a vehicle found on current frequency. Vehicle id: %u, has %d radio links: %s.", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id, g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count, szTmpBuff);
         for( int i=0; i<g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count; i++ )
         {
            if ( g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[i] == m_CurrentSearchFrequencyKhz )
               bVehicleIsOnCurrentFreq = true;
            if ( g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[i] < 10000 )
            if ( g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[i]*1000 == m_CurrentSearchFrequencyKhz )
               bVehicleIsOnCurrentFreq = true;
         }
      }
      if ( bVehicleIsOnCurrentFreq )
      {
         u8 vMaj = g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.version;
         u8 vMin = g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.version;
         vMaj = vMaj >> 4;
         vMin = vMin & 0x0F;
   
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];

         strcpy(szFreq1, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[0]) );
         strcpy(szFreq2, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[1]) );
         strcpy(szFreq3, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[2]) );

         log_line("MenuSearch::onSearchStep() Found a vehicle while searching on %s: vehicle ID: %u, version: %d.%d, radio links (%d): %s, %s, %s",
             str_format_frequency(m_CurrentSearchFrequencyKhz), g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id, vMaj, vMin,
             g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.radio_links_count,
             szFreq1, szFreq2, szFreq3 );
         m_bIsSearchPaused = true;
         g_bSearchFoundVehicle = true;
         m_iConfirmationId = 1;
         invalidate();
         setTooltip("");
         if ( NULL != m_pPopupSearch )
         {
            popups_remove(m_pPopupSearch);
            m_pPopupSearch = NULL;
         }
         MenuSearchConnect* pMenu = new MenuSearchConnect();
         pMenu->setCurrentFrequency(m_CurrentSearchFrequencyKhz);
         if ( m_SpectatorOnlyMode )
            pMenu->setSpectatorOnly();
         add_menu_to_stack(pMenu);
         log_line("Added connect menu to stack");
      }
      else if ( g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id != 0 )
      {
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];
         strcpy(szFreq1, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[0]) );
         strcpy(szFreq2, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[1]) );
         strcpy(szFreq3, str_format_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.uRadioFrequenciesKhz[2]) );
         log_softerror_and_alarm("Found a vehicle that emits on these frequencies %s, %s, %s, but is not on current search frequencty %s!",
            szFreq1, szFreq2, szFreq3, str_format_frequency(m_CurrentSearchFrequencyKhz) );
      }
   }
}

void MenuSearch::onReturnFromChild(int returnValue)
{
   Menu::onReturnFromChild(returnValue);

   if ( NULL != m_pPopupSearch )
   {
      popups_remove(m_pPopupSearch);
      m_pPopupSearch = NULL;
   }

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
         createSearchPopup();
         return;
      }

      // Connect as Spectator
      if ( 1 == returnValue )
      {
         bool bIsNew = false;
         log_line("Pressed add as spectator. Has an active model already: %s. Current total spectator vehicles: %d.", (NULL != g_pCurrentModel)?"Yes":"No", getControllerModelsSpectatorCount());
         if( ! ruby_is_first_pairing_done() )
         {
            deleteAllModels();
            bIsNew = true;
         }
         
         if ( ! controllerHasModelWithId(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id) )
            bIsNew = true;

         Model* pModel = addSpectatorModel(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id);
         pModel->populateFromVehicleTelemetryData_v3(&(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended));
         pModel->is_spectator = true;

         set_model_main_connect_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id, m_CurrentSearchFrequencyKhz);

         stopSearch();
         ruby_set_is_first_pairing_done();
         g_bFirstModelPairingDone = true;
         g_pCurrentModel = pModel;
         setControllerCurrentModel(g_pCurrentModel->vehicle_id);
         saveControllerModel(g_pCurrentModel);

         ruby_set_active_model_id(g_pCurrentModel->vehicle_id);

         if ( bIsNew )
            onModelAdded(pModel->vehicle_id);
         onMainVehicleChanged();
         log_line("Added vehicle id: %u as spectator.", pModel->vehicle_id);
         pairing_start_normal();
         m_bDidConnectToAVehicle = true;
         m_bMustSwitchBack = false;
         log_line("MenuSearch: Close all");
         menu_close_all();
         return;
      }

      // Connect for Control

      if ( 2 == returnValue )
      {
         bool bIsNew = false;
         log_line("Pressed add as controller. Has an active model already: %s. Current total controller vehicles: %d.", (NULL != g_pCurrentModel)?"Yes":"No", getControllerModelsCount());
         if( ! ruby_is_first_pairing_done() )
         {
            log_line("First pairing was not completed before. Deleting all models.");
            deleteAllModels();
            bIsNew = true;
         }
         else
         {
            log_line("First pairing was already completed.");
         
            if ( NULL != g_pCurrentModel )
            {
               log_line("Save current model, current model vehicle id: %u. Received vehicle id while searching: %u",
                  g_pCurrentModel->vehicle_id,
                  g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id );

               saveControllerModel(g_pCurrentModel);
            }
            else
               log_line("No current model, do not save it.");
         }

         if ( ! controllerHasModelWithId(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id) )
            bIsNew = true;
         if ( NULL == findModelWithId(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id) )
            bIsNew = true;
         log_line("This VID (%u) is a new vehicle? %s", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id, bIsNew?"Yes":"No, controller already kowns about it.");

         Model* pModel = NULL;
         if ( bIsNew )
         {
            log_line("Search: Adding a new vehicle model as controller");
            log_line("Creating a vehicle model for vehicle uid: %u ...", g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id);
            pModel = addNewModel();
         }
         else
         {
            pModel = findModelWithId(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id);
            log_line("Search: Found existing controller vehicle model for vehicle id %u.", pModel->vehicle_id);
         }
         pModel->populateFromVehicleTelemetryData_v3(&(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended));
         pModel->is_spectator = false;

         set_model_main_connect_frequency(g_SearchVehicleRuntimeInfo.headerRubyTelemetryExtended.vehicle_id, m_CurrentSearchFrequencyKhz);

         stopSearch();
         log_line("MenuSearch: Save model VID %u, mode: %s", pModel->vehicle_id, (pModel->is_spectator)?"spectator":"control");
         ruby_set_is_first_pairing_done();
         g_bFirstModelPairingDone = true;
         g_pCurrentModel = pModel;
         setControllerCurrentModel(g_pCurrentModel->vehicle_id);
         saveControllerModel(g_pCurrentModel);

         ruby_set_active_model_id(g_pCurrentModel->vehicle_id);
         
         g_pCurrentModel->b_mustSyncFromVehicle = true;
         log_line("MenuSearch: Changed current main vehicle to vehicle id %u", g_pCurrentModel->vehicle_id);
         if ( bIsNew )
            onModelAdded(pModel->vehicle_id);
         onMainVehicleChanged();
         log_line("Added VID %u as a controller model", pModel->vehicle_id);
         pairing_start_normal();
         m_bDidConnectToAVehicle = true;
         m_bMustSwitchBack = false;
         log_line("MenuSearch: Close all");
         menu_close_all();
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

   if ( Menu::onBack() == 1 )
      return 1;

   search_finished_with_no_results = false;

   if ( m_bIsSearchingManual || m_bIsSearchingAuto )
   {
      log_line("MenuSearch::onBack() stop search");
      hardware_blockCurrentPressedKeys();
      stopSearch();
      return 1;
   }

   // Switch back to the original vehicle when we opened the search menu, if any

   if ( m_bMustSwitchBack )
   {
      log_line("MenuSearch::onBack() switch back");
      pairing_stop();
      log_line("MenuSearch: Controller radio interfaces configuration on closing the menu and switching back to original model:");
      hardware_load_radio_info();
      g_pCurrentModel = m_pModelOriginal;
      m_bMustSwitchBack = false;

      if ( ! g_bFirstModelPairingDone )
      {
         log_line("MenuSearch:onBack() Switched back to previous active current model with no first pairing done, vehicle id: %u", g_pCurrentModel->vehicle_id);
         pairing_start_normal();
      }
      else if ( (NULL != g_pCurrentModel) && ((0 != getControllerModelsCount()) || (0 != getControllerModelsSpectatorCount()) ) )
      {
         log_line("MenuSearch:onBack() Switched back to previous active current model, vehicle id: %u", g_pCurrentModel->vehicle_id);
         pairing_start_normal();
      }
      else
         log_line("MenuSearch:onBack() no previous current model to switch back to.");
      return 1;
   }
   log_line("Default back, no particular action.");

   if ( m_pPopupSearch )
   {
      popups_remove( m_pPopupSearch );
      m_pPopupSearch = NULL;
   }

   return 0;
}

void MenuSearch::onSelectItem()
{
   Menu::onSelectItem();

   if ( m_IndexBand == m_SelectedIndex )
   {
      m_SearchBandIndex = m_pItemSelectBand->getSelectedIndex();
      save_simple_config_fileI(FILE_CURRENT_SEARCH_BAND, m_SearchBandIndex);
      return;
   }

   // Start/Stop Search
   if ( m_IndexStartSearch == m_SelectedIndex )
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
   if ( (m_IndexManualSearch == m_SelectedIndex) && (! m_pItemsSelectFreq->isEditing()) )
   {
      log_line("Pressed manual search button");

      createSearchPopup();
      g_bSearching = true;
      render_all(get_current_timestamp_ms(), true);
      pairing_stop();
      render_all(get_current_timestamp_ms(), true);

      if ( ! m_bDidConnectToAVehicle )
         m_bMustSwitchBack = true;
      g_pCurrentModel = NULL;
      m_CurrentSearchFrequencyKhz = 0;
      g_iSearchFrequency = 0;
      m_pSearchChannels = &(m_Channels[m_pItemsSelectFreq->getSelectedIndex()]);
      m_SearchChannelsCount = 1;

      log_line("Searching only on %s", str_format_frequency(m_pSearchChannels[0]));

      if ( m_bHasSiKRadio )
      {
         enableMenuItem(m_IndexSiKInfo, false);
         enableMenuItem(m_IndexSiKAirRate, false);
         enableMenuItem(m_IndexSiKECC, false);
         enableMenuItem(m_IndexSiKLBT, false);
         enableMenuItem(m_IndexSiKMCSTR, false);
      }
      enableMenuItem(m_IndexBand, false);
      m_pItemsSelectFreq->setEnabled(false);
      m_pMenuItems[m_IndexStartSearch]->setTitle("Stop Search");

      handle_commands_abandon_command();
      reset_vehicle_runtime_info(&g_SearchVehicleRuntimeInfo);

      m_bIsSearchingManual = true;
      m_bIsSearchingAuto = false;
      m_bIsSearchPaused = false;
      m_bIsSearchingSiK = false;
      int iBand = getBand(m_pSearchChannels[0]);
      if ( m_bHasSiKRadio )
      if ( iBand == RADIO_HW_SUPPORTED_BAND_433 || iBand == RADIO_HW_SUPPORTED_BAND_868 || iBand == RADIO_HW_SUPPORTED_BAND_915 )
         m_bIsSearchingSiK = true;


      if ( NULL != m_pPopupSearch )
      {
         popups_remove(m_pPopupSearch);
         m_pPopupSearch = NULL;
      }
      createSearchPopup();

      render_all(get_current_timestamp_ms(), true);

      g_bSearching = true;
      g_bSearchFoundVehicle = false;
      g_RouterIsReadyTimestamp = 0;
      render_search_step = -1;
      search_finished_with_no_results = false;
      return;
   }

   if ( m_bHasSiKRadio && (m_SelectedIndex > m_IndexManualSearch) )
   {
      ControllerSettings* pCS = get_ControllerSettings();
   
      if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
         return;

      if ( m_IndexSiKAirRate == m_SelectedIndex )
      {
         if ( 0 == m_pItemsSelect[2]->getSelectedIndex() )
            pCS->iSearchSiKAirRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
         else
            pCS->iSearchSiKAirRate = getSiKAirDataRates()[m_pItemsSelect[2]->getSelectedIndex()-1];
         save_ControllerSettings();
      }

      if ( m_IndexSiKECC == m_SelectedIndex )
      {
         pCS->iSearchSiKECC = m_pItemsSelect[3]->getSelectedIndex();
         save_ControllerSettings();
      }
      if ( m_IndexSiKLBT == m_SelectedIndex )
      {
         pCS->iSearchSiKLBT = m_pItemsSelect[4]->getSelectedIndex();
         save_ControllerSettings();
      }
      if ( m_IndexSiKMCSTR == m_SelectedIndex )
      {
         pCS->iSearchSiKMCSTR = m_pItemsSelect[5]->getSelectedIndex();
         save_ControllerSettings();
      }
      return;
   }
}

void MenuSearch::createSearchPopup()
{
   link_watch_remove_popups();
   if ( NULL != m_pPopupSearch )
   {
      popups_remove(m_pPopupSearch);
      m_pPopupSearch = NULL;
   }

   m_pPopupSearch = new Popup(true, "Searching ...", 500);
   m_pPopupSearch->setFixedWidth(0.28);
   m_pPopupSearch->setFont(g_idFontMenuLarge);
   m_pPopupSearch->useSmallLines(true);
   m_pPopupSearch->setCenteredTexts();
   m_pPopupSearch->disableAutoRemove();

   if ( m_bHasSiKRadio && m_bIsSearchingSiK )
   {
      int iAirDataRate = DEFAULT_RADIO_DATARATE_SIK_AIR;
      if ( m_pItemsSelect[2]->getSelectedIndex() > 0 )
         iAirDataRate = getSiKAirDataRates()[m_pItemsSelect[2]->getSelectedIndex()-1];
      int iECC = m_pItemsSelect[3]->getSelectedIndex();
      int iLBT = m_pItemsSelect[4]->getSelectedIndex();
      int iMCSTR = m_pItemsSelect[5]->getSelectedIndex();
      char szBuff[256];
      sprintf(szBuff, "Searching using SiK radios (%d kbps air rate, %s, %s, %s)...",
          iAirDataRate/1000,
          iECC?"ECC":"No ECC",
          iLBT?"LBT":"No LBT",
          iMCSTR?"MCSTR":"No MCSTR");
      m_pPopupSearch->addLine(szBuff);
   }     

   m_pPopupSearch->addLine("Long press on [Back] to stop the search.");
   popups_add_topmost(m_pPopupSearch);
}
