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
      virtual int onBack();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void Render();
      virtual void onSelectItem();

      bool m_bSelectSecond;
      bool m_bShowController;
      bool m_bShowVehicle;

   private:
      void RenderTableLine(int iCardModel, const char* szText, const int* piValues, bool bIsHeader, bool bIsBoosterLine);
      void drawPowerLine(const char* szText, float yPos, int value);
      void sendPowerToVehicle(int tx, int txAtheros, int txRTL);

      float m_yTemp;
      float m_yTopRender;
      float m_xTable;
      float m_xTableCellWidth;
      int m_iLine;

      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[10];

      bool m_bShowThinLine;
      bool m_bShowBothOnController;
      bool m_bShowBothOnVehicle;
      bool m_bDisplay24Cards;
      bool m_bDisplay58Cards;

      bool m_bControllerHas24Cards;
      bool m_bControllerHas58Cards;
      bool m_bVehicleHas24Cards;
      bool m_bVehicleHas58Cards;

      int m_IndexPowerVehicle;
      int m_IndexPowerVehicleAtheros;
      int m_IndexPowerVehicleRTL;
      int m_IndexPowerController;
      int m_IndexPowerControllerAtheros;
      int m_IndexPowerControllerRTL;
      int m_IndexPowerMax;
      int m_IndexShowAllCards;
      int m_IndexShowBoosters;

      bool m_bValuesChangedVehicle;
      bool m_bValuesChangedController;
};