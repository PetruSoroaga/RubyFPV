#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuDeviceI2C: public Menu
{
   public:
      MenuDeviceI2C();
      virtual ~MenuDeviceI2C();
      virtual void onShow();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int returnValue);  
      virtual void onSelectItem();

      void setDeviceId(int nDeviceId);

   private:
      void createItems();
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemRange* m_pItemsRange[14];

      int m_nDeviceId;
      bool m_bDeviceHasCustomSettings;
      int m_IndexDevType;
      int m_IndexEnable;
      int m_IndexCustomSettings1;
      int m_IndexCustomSettings2;
      int m_IndexCustomSettings3;
      int m_IndexCustomSettings4;
};