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
      void addItems();
      void RenderVehicleInfo();
      void createHWInfo(Menu* pm);

      int m_iIndexSimpleSetup;
      int m_iIndexVehicle, m_iIndexMyVehicles;
      int m_iIndexSpectator, m_iIndexSearch;
      int m_iIndexController, m_iIndexMedia;
      int m_iIndexSystem;
};