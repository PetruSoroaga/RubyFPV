#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "../base/config.h"

class MenuVehicleOSDPlugins: public Menu
{
   public:
      MenuVehicleOSDPlugins();
      virtual ~MenuVehicleOSDPlugins();
      virtual void valuesToUI();
      virtual void Render();
      virtual void onReturnFromChild(int returnValue);
      virtual void onSelectItem();
            
   private:
      void readPlugins();
      void importFromUSB();

      //int m_IndexImport;
};