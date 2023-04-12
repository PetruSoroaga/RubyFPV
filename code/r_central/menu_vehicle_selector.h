#pragma once
#include "menu_objects.h"


class MenuVehicleSelector: public Menu
{
   public:
      MenuVehicleSelector();
      virtual ~MenuVehicleSelector();
      virtual void Render();
      virtual void onReturnFromChild(int returnValue);
      virtual void onShow();
      virtual void onSelectItem();
      
      int m_IndexSelectedVehicle;

   protected:
      int m_IndexSelect;
      int m_IndexDelete;

};