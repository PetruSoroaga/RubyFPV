#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_edit.h"
#include "menu_item_slider.h"

class MenuVehicleGeneral: public Menu
{
   public:
      MenuVehicleGeneral();
      virtual ~MenuVehicleGeneral();
      virtual void Render();
      virtual void valuesToUI();
      virtual int onBack();
      virtual void onSelectItem();
      virtual void onReturnFromChild(int returnValue);
            
   private:
      MenuItemEdit* m_pItemEditName;
      MenuItemSelect* m_pItemsSelect[30];
      MenuItemSlider* m_pItemsSlider[10];
      int m_IndexVehicleType, m_IndexEncryption;
      int m_IndexGPS;
      int m_IndexPrioritizeUplink;

      int m_IndexFreq[MAX_RADIO_INTERFACES];

      int m_SupportedChannels[MAX_RADIO_INTERFACES][100];
      int m_SupportedChannelsCount[MAX_RADIO_INTERFACES];

      bool m_bControllerHasKey;

      void populate();
      void addTopDescription();
};