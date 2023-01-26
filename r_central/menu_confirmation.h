#pragma once
#include "menu_objects.h"


class MenuConfirmation: public Menu
{
   public:
      MenuConfirmation(const char* szTitle, const char* szText, int id);
      MenuConfirmation(const char* szTitle, const char* szText, int id, bool singleOption);
      virtual ~MenuConfirmation();
      virtual void onSelectItem();

   protected:
      bool m_bSingleOption;
};