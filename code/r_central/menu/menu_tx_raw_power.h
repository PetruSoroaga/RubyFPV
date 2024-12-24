#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuTXRawPower: public Menu
{
   public:
      MenuTXRawPower();
      virtual ~MenuTXRawPower();
      virtual void onShow();
      virtual void valuesToUI();
      virtual int onBack();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void Render();
      virtual void onSelectItem();

      bool m_bShowVehicleSide;
      
   protected:
      void addItems();
      MenuItemSelect* createItemCard(bool bVehicle, int iCardIndex, int iCardModel, int iDriver, int iRawPower);
      void computeSendPowerToVehicle(int iCardIndex);
      void computeApplyControllerPower(int iCardIndex);

      MenuItemSlider* m_pItemsSlider[10];
      MenuItemSelect* m_pItemsSelect[10];

      MenuItemSelect* m_pItemSelectVehicleCards[MAX_RADIO_INTERFACES];
      MenuItemSelect* m_pItemSelectControllerCards[MAX_RADIO_INTERFACES];

      int m_iIndexVehicleCards[MAX_RADIO_INTERFACES];
      int m_iIndexControllerCards[MAX_RADIO_INTERFACES];
      int m_iIndexAutoControllerPower;
};