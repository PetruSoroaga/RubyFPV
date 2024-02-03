#pragma once
#include "menu_objects.h"


class MenuRoot: public Menu
{
   public:
      MenuRoot();
      virtual ~MenuRoot();
      virtual void Render();
      virtual void onShow();
      virtual void onSelectItem();

   private:
      void RenderVehicleInfo();
      void createAboutInfo(Menu* pm);
      void createHWInfo(Menu* pm);
      void show_MenuInfo();
};