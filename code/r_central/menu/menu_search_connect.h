#pragma once
#include "menu_objects.h"
#include "../../base/models.h"
#include "menu_item_select.h"

#define MAX_MENU_CHANNELS 100

class MenuSearchConnect: public Menu
{
   public:
      MenuSearchConnect();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();
      void setSpectatorOnly();
      void setCurrentFrequency(u32 freq);
      int m_iSearchModelTypes;
   private:
      bool m_bSpectatorOnlyMode;
      u32 m_CurrentSearchFrequency;

      int m_iIndexController;
      int m_iIndexSpectator;
      int m_iIndexSkip;
};