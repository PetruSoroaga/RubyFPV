#pragma once
#include "menu_objects.h"


class MenuConfirmation: public Menu
{
   public:
      MenuConfirmation(const char* szTitle, const char* szText, int id);
      MenuConfirmation(const char* szTitle, const char* szText, int id, bool singleOption);
      virtual ~MenuConfirmation();
      virtual void Render();
      virtual void onSelectItem();

      void setIconId(u32 uIconId);

   protected:
      bool m_bSingleOption;
      u32 m_uIconId;
};