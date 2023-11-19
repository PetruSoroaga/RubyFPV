#pragma once
#include "menu_objects.h"


class MenuSwitchVehicle: public Menu
{
   public:
      MenuSwitchVehicle(u32 uVehicleId);
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();
   protected:
      u32 m_uVehicleId;
};