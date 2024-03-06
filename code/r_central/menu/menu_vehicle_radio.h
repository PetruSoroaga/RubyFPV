#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_edit.h"
#include "menu_item_slider.h"

class MenuVehicleRadioConfig: public Menu
{
   public:
      MenuVehicleRadioConfig();
      virtual ~MenuVehicleRadioConfig();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onSelectItem();
            
   private:
      MenuItemSelect* m_pItemsSelect[30];
      MenuItemSlider* m_pItemsSlider[10];
      
      int m_IndexPrioritizeUplink;
      int m_IndexDisableUplink;
      int m_IndexEncryption;
      int m_IndexPower;
      int m_IndexFreq[MAX_RADIO_INTERFACES];
      int m_IndexConfigureLinks[MAX_RADIO_INTERFACES];
      u32 m_SupportedChannels[MAX_RADIO_INTERFACES][100];
      int m_SupportedChannelsCount[MAX_RADIO_INTERFACES];

      bool m_bControllerHasKey;
      void populate();
      void sendNewRadioLinkFrequency(int iVehicleLinkIndex, u32 uNewFreqKhz);

};