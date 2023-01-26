#pragma once
#include "menu_objects.h"
#include "../base/models.h"
#include "menu_item_select.h"

#define MAX_MENU_CHANNELS 100

class MenuSearch: public Menu
{
   public:
      MenuSearch();
      virtual void Render();
      virtual void onShow();
      virtual int onBack();
      virtual void onReturnFromChild(int returnValue);  
      virtual void onSelectItem();
      void setSpectatorOnly();

   private:
      int render_search_step;
      bool search_finished_with_no_results;
      int m_SearchBand;
      bool m_bIsSearchPaused;
      bool m_SpectatorOnlyMode;
      bool m_bIsSearchingManual;
      bool m_bIsSearchingAuto;
      int m_nSkippedCount;
      int m_SearchChannelsCount;
      int* m_pSearchChannels;
      bool m_bMustSwitchBack;
      int m_FrequencyOriginal[MAX_RADIO_INTERFACES];
      Model* m_pModelOriginal;
      int m_CurrentSearchFrequency;

      MenuItemSelect* m_pItemSelectBand;
      MenuItemSelect* m_pItemsSelectFreq;

      int m_Channels[MAX_MENU_CHANNELS];
      int m_NumChannels;
      u8 m_SupportedBands;
      
      void stopSearch();
      void startSearch();
      void onSearchStep();
};