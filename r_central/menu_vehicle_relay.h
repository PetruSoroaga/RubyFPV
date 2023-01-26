#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleRelay: public Menu
{
   public:
      MenuVehicleRelay();
      virtual ~MenuVehicleRelay();
      virtual void Render();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void onSelectItem();

   protected:
      MenuItemSelect* m_pItemsSelect[10];
      MenuItemSlider* m_pItemsSlider[10];

      int m_IndexItemEnabled;
      int m_IndexQAButton;
      int m_IndexItemType;

      bool m_bIsConfigurable;
      void disableRelayUI();
};