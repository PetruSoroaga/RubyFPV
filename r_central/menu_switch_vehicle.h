#pragma once
#include "menu_objects.h"


class MenuSwitchVehicle: public Menu
{
   public:
      MenuSwitchVehicle(int vehicleIndex);
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();
   protected:
      int m_VehicleIndex;
};