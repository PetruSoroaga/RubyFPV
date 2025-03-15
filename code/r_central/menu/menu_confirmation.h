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
      virtual bool periodicLoop();
      virtual int onBack();
      virtual void onSelectItem();

      void setOkActionText(const char* szText);
      void setIconId(u32 uIconId);
      void setTimeoutMs(u32 uTimeoutMs);
      void setUniqueId(int iUniqueId);
      void enableShowDoNotShowAgain();
      void disablePairingUIActions();

   protected:
      void _saveDoNotShowFlag();
      bool m_bSingleOption;
      bool m_bShowDoNotShowAgain;
      bool m_bDisablePairingUIActions;
      u32 m_uTimeoutMs;
      u32 m_uCloseOnTimeoutTime;
      u32 m_uIconId;
      int m_iUniqueId;
      char m_szButtonOk[64];

      int m_iIndexMenuOk;
      int m_iIndexMenuCancel;
      int m_iIndexMenuDoNotShow;
      MenuItemCheckbox* m_pCheckBox;
      bool m_bDoNotShowAgain;
};