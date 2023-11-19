#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

#define MAX_MENU_CHANNELS 100

class MenuVehicleRadioLink: public Menu
{
   public:
      MenuVehicleRadioLink(int iRadioLink);
      virtual ~MenuVehicleRadioLink();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onShow();
      virtual void onReturnFromChild(int returnValue); 
      virtual void onItemValueChanged(int itemIndex);
      virtual void onSelectItem();
            
   private:
      void sendRadioLinkCapabilitiesFlags(int linkIndex);
      void sendRadioLinkFlags(int linkIndex);
      void sendRadioLinkInfo(int iRadioLink);

      int m_iRadioLink;

      MenuItemSlider* m_pItemsSlider[20];
      MenuItemSelect* m_pItemsSelect[20];

      u32 m_SupportedChannels[MAX_MENU_CHANNELS];
      int m_SupportedChannelsCount;

      int m_IndexStart;
      int m_IndexFrequency;
      int m_IndexUsage;
      int m_IndexCapabilities;
      int m_IndexDataRateVideo;
      int m_IndexDataRateTypeDownlink;
      int m_IndexDataRateTypeUplink;
      int m_IndexDataRateDataDownlink;
      int m_IndexDataRateDataUplink;
      int m_IndexFlagsTarget;
      int m_IndexHT;
      int m_IndexLDPC;
      int m_IndexSGI;
      int m_IndexSTBC;
      int m_IndexMCSInfo;
};