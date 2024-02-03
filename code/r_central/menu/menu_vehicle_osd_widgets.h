#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"
#include "../../base/config.h"

class MenuVehicleOSDWidgets: public Menu
{
   public:
      MenuVehicleOSDWidgets();
      virtual ~MenuVehicleOSDWidgets();
      virtual void valuesToUI();
      virtual bool periodicLoop();
      virtual void Render();

      virtual void onMoveUp(bool bIgnoreReversion);
      virtual void onMoveDown(bool bIgnoreReversion);
      virtual void onMoveLeft(bool bIgnoreReversion);
      virtual void onMoveRight(bool bIgnoreReversion);
      
      virtual void onSelectItem();
            
      virtual int onBack();

      void stopAction();
      
   private:
      bool m_bIsMovingH;
      bool m_bIsMovingV;
      bool m_bIsResizing;
      
      
};