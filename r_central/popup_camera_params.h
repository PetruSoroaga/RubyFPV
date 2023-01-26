#pragma once
#include "popup.h"
#include "menu_item_slider.h"


class PopupCameraParams: public Popup
{
   public:
      PopupCameraParams();
      virtual ~PopupCameraParams();

      virtual void onShow();
      virtual void Render();

      void handleRotaryEvents(bool bCW, bool bCCW, bool bFastCW, bool bFastCCW, bool bSelect, bool bCancel);

   protected:
      bool m_bCanAdjust;
      int m_nParamToAdjust;
      MenuItemSlider* m_pSlider;
};
