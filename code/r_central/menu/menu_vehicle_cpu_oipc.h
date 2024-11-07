#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleCPU_OIPC: public Menu
{
   public:
      MenuVehicleCPU_OIPC();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();
      
   private:
      int m_IndexEnableNice;
      int m_IndexNiceRouter;
      int m_IndexEnableVideo;
      int m_IndexNiceVideo;
      int m_IndexEnableRouter;
      int m_IndexRouter;
      int m_IndexEnableRadio;
      int m_IndexRadioRx;
      int m_IndexRadioTx;
      int m_IndexCPUSpeed;
      int m_IndexGPUBoost;
      
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[10];

      void send_threads_values();
};