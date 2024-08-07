#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuTXPower: public Menu
{
   public:
      MenuTXPower();
      virtual ~MenuTXPower();
      virtual void onShow();
      virtual void valuesToUI();
      virtual int onBack();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void Render();
      virtual void onSelectItem();

      bool m_bShowController;
      bool m_bShowVehicle;

   protected:
      void RenderTableLine(int iCardModel, const char* szText, const int* piValues, bool bIsHeader, bool bIsBoosterLine);
      void drawPowerLine(const char* szText, float yPos, int value);
      void sendPowerToVehicle(int txRTL8812AU, int txRTL8812EU, int txAtheros);

      float m_yTemp;
      float m_yTopRender;
      float m_xTable;
      float m_xTableCellWidth;
      int m_iLine;

      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[10];

      bool m_bShowThinLine;

      int m_IndexPowerVehicleRTL8812AU;
      int m_IndexPowerVehicleRTL8812EU;
      int m_IndexPowerVehicleAtheros;
      int m_IndexPowerControllerRTL8812AU;
      int m_IndexPowerControllerRTL8812EU;
      int m_IndexPowerControllerAtheros;
      int m_IndexPowerMax;
      int m_IndexShowAllCards;
      int m_IndexShowBoosters;

      bool m_bValuesChangedVehicle;
      bool m_bValuesChangedController;
};