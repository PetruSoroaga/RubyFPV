#pragma once
#include "menu_objects.h"


class MenuConfirmationHDMI: public Menu
{
   public:
      MenuConfirmationHDMI(const char* szTitle, const char* szText, int id);
      virtual ~MenuConfirmationHDMI();
      virtual void onShow();
      virtual void onSelectItem();

};