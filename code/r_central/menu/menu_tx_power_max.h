#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuTXPowerMax: public Menu
{
   public:
      MenuTXPowerMax();
      virtual ~MenuTXPowerMax();
      virtual void onShow(); 
      virtual void valuesToUI();
      virtual void Render();
      virtual void onSelectItem();

      bool m_bShowController;
      bool m_bShowVehicle;

   private:
      void sendMaxPowerToVehicle(int txMaxRTL8812AU, int txMaxRTL8812EU, int txMaxAtheros);

      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[10];

      int m_IndexPowerMaxVehicleRTL8812AU;
      int m_IndexPowerMaxVehicleRTL8812EU;
      int m_IndexPowerMaxVehicleAtheros;
      int m_IndexPowerMaxControllerRTL8812AU;
      int m_IndexPowerMaxControllerRTL8812EU;
      int m_IndexPowerMaxControllerAtheros;
};