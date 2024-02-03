#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuControllerTelemetry: public Menu
{
   public:
      MenuControllerTelemetry();
      virtual ~MenuControllerTelemetry();
      virtual void Render();
      virtual void valuesToUI();
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];
      MenuItemRange* m_pItemsRange[14];

      int m_IndexInOut;
      int m_IndexSerialPort;
      int m_IndexSerialSpeed;

      int m_IndexTelemetryUSBForward;
      int m_IndexTelemetryUSBPort;
      int m_IndexTelemetryUSBPacket;
};