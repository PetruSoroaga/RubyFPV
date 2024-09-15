#pragma once
#include "menu_objects.h"


class MenuConfirmationVehicleBoard: public Menu
{
   public:
      MenuConfirmationVehicleBoard();
      virtual ~MenuConfirmationVehicleBoard();
      virtual void onShow();
      virtual void onSelectItem();

};