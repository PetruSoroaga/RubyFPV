#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehiclePeripherals: public Menu
{
   public:
      MenuVehiclePeripherals();
      virtual ~MenuVehiclePeripherals();
      virtual void Render();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void onSelectItem();

   protected:
      MenuItemSelect* m_pItemsSelect[40];
      MenuItemSlider* m_pItemsSlider[40];

      bool m_bWaitingForVehicleInfo;

      int m_IndexSerialPortsUsage[MAX_MODEL_SERIAL_PORTS];
      int m_IndexStartPortUsagePluginsStartIndex[MAX_MODEL_SERIAL_PORTS];
      int m_IndexSerialPortsBaudRate[MAX_MODEL_SERIAL_PORTS];
};