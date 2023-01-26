#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

#define MAX_MENU_CHANNELS 100

class MenuVehicleRadioLinks: public Menu
{
   public:
      MenuVehicleRadioLinks();
      virtual ~MenuVehicleRadioLinks();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onShow();
      virtual void onReturnFromChild(int returnValue); 
      virtual void onItemValueChanged(int itemIndex);
      virtual void onSelectItem();
            
   private:
      void sendLinkCapabilitiesFlags(int linkIndex);
      void sendLinkRadioFlags(int linkIndex);

      MenuItemSlider* m_pItemsSlider[120];
      MenuItemSelect* m_pItemsSelect[120];

      int m_SupportedChannels[MAX_RADIO_INTERFACES][MAX_MENU_CHANNELS];
      int m_SupportedChannelsCount[MAX_RADIO_INTERFACES];

      int m_IndexStartNics[MAX_RADIO_INTERFACES];
      int m_IndexCardModel[MAX_RADIO_INTERFACES];
      int m_IndexFrequency[MAX_RADIO_INTERFACES];
      int m_IndexUsage[MAX_RADIO_INTERFACES];
      int m_IndexCapabilities[MAX_RADIO_INTERFACES];
      int m_IndexDataRate[MAX_RADIO_INTERFACES];
      int m_IndexDataRateAdaptive[MAX_RADIO_INTERFACES];
      int m_IndexFlagsTarget[MAX_RADIO_INTERFACES];
      int m_IndexHT[MAX_RADIO_INTERFACES];
      int m_IndexLDPC[MAX_RADIO_INTERFACES];
      int m_IndexSGI[MAX_RADIO_INTERFACES];
      int m_IndexSTBC[MAX_RADIO_INTERFACES];
      int m_IndexMCSInfo[MAX_RADIO_INTERFACES];

      int m_IndexTXPower;
};