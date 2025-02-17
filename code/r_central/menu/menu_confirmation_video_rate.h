#pragma once
#include "menu_objects.h"


class MenuConfirmationVideoRate: public Menu
{
   public:
      MenuConfirmationVideoRate(int iNewDataRate);
      virtual ~MenuConfirmationVideoRate();
      virtual void onShow();
      virtual void onSelectItem();

   protected:
      int m_iNewDataRate;
};