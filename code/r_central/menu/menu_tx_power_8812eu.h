#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_tx_power.h"

class MenuTXPower8812EU: public MenuTXPower
{
   public:
      MenuTXPower8812EU();
      virtual ~MenuTXPower8812EU();
      virtual void onShow(); 
      virtual void valuesToUI();
      virtual int onBack();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void Render();
      virtual void onSelectItem();
};