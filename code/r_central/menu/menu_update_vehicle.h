#pragma once
#include "menu_objects.h"


class MenuUpdateVehiclePopup: public Menu
{
   public:
      MenuUpdateVehiclePopup(int vehicleIndex);
      virtual void Render();
      virtual void onShow();
      virtual int onBack();
      virtual void onSelectItem();
   protected:
      int m_VehicleIndex;
};