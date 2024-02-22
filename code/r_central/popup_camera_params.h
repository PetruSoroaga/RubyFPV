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
      bool m_bHasSharpness;
      bool m_bHasHue;
      bool m_bHasAGC;
      int m_iIndexParamBrightness;
      int m_iIndexParamContrast;
      int m_iIndexParamSaturation;
      int m_iIndexParamSharpness;
      int m_iIndexParamHue;
      int m_iIndexParamAGC;
      int m_iTotalParams;
      int m_iParamToAdjust;
      MenuItemSlider* m_pSlider;
};
