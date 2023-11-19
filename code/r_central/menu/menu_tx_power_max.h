#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuTXPowerMax: public Menu
{
   public:
      MenuTXPowerMax();
      virtual ~MenuTXPowerMax();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onSelectItem();

   private:
      void sendMaxPowerToVehicle(int txMax, int txMaxAtheros, int txMaxRTL);

      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[10];

      int m_IndexPowerMaxVehicle;
      int m_IndexPowerMaxVehicleAtheros;
      int m_IndexPowerMaxVehicleRTL;
      int m_IndexPowerMaxController;
      int m_IndexPowerMaxControllerAtheros;
      int m_IndexPowerMaxControllerRTL;

      bool m_bShowBothOnController;
      bool m_bShowBothOnVehicle;
      bool m_bControllerHas24OnlyCards;
      bool m_bControllerHas58Cards;
      bool m_bVehicleHas24OnlyCards;
      bool m_bVehicleHas58Cards;
};