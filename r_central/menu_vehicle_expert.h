#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleExpert: public Menu
{
   public:
      MenuVehicleExpert();
      virtual void onShow();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onReturnFromChild(int returnValue);
      virtual void onSelectItem();
      
   private:
      int m_IndexNiceRouter, m_IndexNiceRC, m_IndexIONiceRouter, m_IndexIONiceRouterValue;
      int m_IndexIONice, m_IndexIONiceValue;
      int m_IndexNiceVideo, m_IndexNiceTelemetry, m_IndexNiceOthers;
      int m_IndexReboot;
      int m_IndexDHCP;
      int m_IndexCPUEnabled;
      int m_IndexGPUEnabled;
      int m_IndexVoltageEnabled;
      int m_IndexCPUSpeed;
      int m_IndexGPUSpeed;
      int m_IndexVoltage;
      int m_IndexReset;
      
      MenuItemSlider* m_pItemsSlider[20];
      MenuItemSelect* m_pItemsSelect[20];

      void addTopInfo();

};