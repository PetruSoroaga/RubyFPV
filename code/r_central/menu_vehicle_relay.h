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
      u32 m_uVehiclesID[MAX_MODELS];
      int m_iCountVehiclesIDs;
      
      int m_IndexItemEnabled;
      int m_IndexVehicle;
      int m_IndexQAButton;
      int m_IndexRelayType;
      int m_IndexOSD;

      bool m_bIsConfigurable;
      float m_fHeightHeader;

      void _drawHeader(float yPos);
      bool _check_enable_relay(int iRadioLinkRelay);
};