#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_vehicle_radio_link_sik.h"
#include "menu_vehicle_radio_link_elrs.h"

class MenuVehicleRadioLink: public Menu
{
   public:
      MenuVehicleRadioLink(int iRadioLink);
      virtual ~MenuVehicleRadioLink();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onShow();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);  
      virtual void onSelectItem();
            
   private:
      void sendRadioLinkCapabilities(int iRadioLink);
      void sendRadioLinkConfig(int linkIndex);
      void sendNewRadioLinkFrequency(int iVehicleLinkIndex, u32 uNewFreqKhz);

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
      int m_IndexHT;
      int m_IndexLDPC;
      int m_IndexSGI;
      int m_IndexSTBC;
      int m_IndexReset; 
};