#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuControllerRadio: public Menu
{
   public:
      MenuControllerRadio();
      virtual void Render();
      virtual void onShow();
      virtual void valuesToUI();
      virtual bool periodicLoop();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);  
      virtual void onSelectItem();

   private:
      void addItems();
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];

      int m_iMaxPowerMw;

      int m_IndexTxPowerMode;
      int m_IndexTxPower;
      int m_IndexRadioConfig;
      
};