#pragma once
#include "menu_objects.h"


class MenuConfirmationImport: public Menu
{
   public:
      MenuConfirmationImport(const char* szTitle, const char* szText, int id);
      virtual ~MenuConfirmationImport();
      virtual void onShow();
      virtual void onSelectItem();

};