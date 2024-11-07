#pragma once
#include "menu_objects.h"
#include "menu_item_vehicle.h"

class MenuVehicles: public Menu
{
   public:
      MenuVehicles();
      virtual void Render();
      virtual void onShow();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();
      
   protected:
      int m_IndexImport;
      int m_IndexDeleteAll;
      int m_IndexSelectedVehicle;
      int m_iLastSelectedVehicle;
};