#pragma once
#include "popup.h"
#include "menu_item_slider.h"
#include "../../base/models.h"

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
      bool m_bCanSendQuick;
      bool m_bHasPendingCameraChanges;
      bool m_bHasPendingAudioChanges;
      type_camera_parameters m_PendingCameraChanges;
      audio_parameters_t m_PendingAudioChanges;
      bool m_bHasSharpness;
      bool m_bHasHue;
      bool m_bHasAGC;
      bool m_bHasVolume;
      int m_iIndexParamBrightness;
      int m_iIndexParamContrast;
      int m_iIndexParamSaturation;
      int m_iIndexParamSharpness;
      int m_iIndexParamHue;
      int m_iIndexParamAGC;
      int m_iIndexVolume;
      int m_iTotalParams;
      int m_iParamToAdjust;
      u32 m_uTimeLastChange;
      MenuItemSlider* m_pSlider;
};
