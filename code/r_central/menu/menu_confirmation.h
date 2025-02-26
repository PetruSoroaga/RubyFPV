#pragma once
#include "menu_objects.h"
#include "menu_item_checkbox.h"

class MenuConfirmation: public Menu
{
   public:
      MenuConfirmation(const char* szTitle, const char* szText, int id);
      MenuConfirmation(const char* szTitle, const char* szText, int id, bool singleOption);
      virtual ~MenuConfirmation();

      virtual void onShow();
      virtual void valuesToUI();
      virtual void Render();
      virtual int onBack();
      virtual void onSelectItem();

      void setOkActionText(const char* szText);
      void setIconId(u32 uIconId);
      void setUniqueId(int iUniqueId);
      void enableShowDoNotShowAgain();

   protected:
      void _saveDoNotShowFlag();
      bool m_bSingleOption;
      bool m_bShowDoNotShowAgain;
      u32 m_uIconId;
      int m_iUniqueId;
      char m_szButtonOk[64];

      int m_iIndexMenuOk;
      int m_iIndexMenuCancel;
      int m_iIndexMenuDoNotShow;
      MenuItemCheckbox* m_pCheckBox;
      bool m_bDoNotShowAgain;
};