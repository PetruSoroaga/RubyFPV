#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuTXInfo: public Menu
{
   public:
      MenuTXInfo();
      virtual ~MenuTXInfo();
      virtual void onShow(); 
      virtual void valuesToUI();
      virtual void onReturnFromChild(int returnValue);
      virtual void Render();
      virtual void onSelectItem();

      bool m_bShowController;
      bool m_bShowVehicle;

   private:
      void RenderTableLine(const char* szText, const char** szValues, bool header);
      void drawPowerLine(const char* szText, float yPos, int value);
      void sendPowerToVehicle(int tx, int txAtheros, int txRTL);

      float m_yTemp;
      float m_yTopRender;
      float m_xTable;
      float m_xTableCellWidth;

      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[10];

      bool m_bShowThinLine;
      bool m_bShowBothOnController;
      bool m_bShowBothOnVehicle;

      bool m_bControllerHasAtheros;
      bool m_bControllerHasRTL;
      bool m_bVehicleHasAtheros;
      bool m_bVehicleHasRTL;

      int m_IndexPowerVehicle;
      int m_IndexPowerVehicleAtheros;
      int m_IndexPowerVehicleRTL;
      int m_IndexPowerController;
      int m_IndexPowerControllerAtheros;
      int m_IndexPowerControllerRTL;
      int m_IndexPowerMax;
};