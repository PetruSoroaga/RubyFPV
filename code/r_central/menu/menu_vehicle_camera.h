#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

class MenuVehicleCamera: public Menu
{
   public:
      MenuVehicleCamera();
      virtual ~MenuVehicleCamera();
      virtual void Render();
      virtual void onSelectItem();
      virtual void onItemValueChanged(int itemIndex);
      virtual void onItemEndEdit(int itemIndex);
      virtual void valuesToUI();
            
   private:
      void resetIndexes();
      void addItems();
      void updateUIValues(int iCameraProfileIndex);
      bool canSendLiveUpdates(int iItemIndex);
      void sendCameraParams(int itemIndex, bool bQuick);

      MenuItemSlider* m_pItemsSlider[25];
      MenuItemSelect* m_pItemsSelect[25];
      MenuItemRange* m_pItemsRange[25];

      int m_iLastCameraType;
      int m_IndexCamera;
      int m_IndexForceMode;
      int m_IndexProfile;
      int m_IndexBrightness, m_IndexContrast, m_IndexSaturation, m_IndexSharpness;
      int m_IndexHue;
      int m_IndexEV, m_IndexEVValue;
      int m_IndexAGC;
      int m_IndexExposureMode, m_IndexExposureValue, m_IndexWhiteBalance;
      int m_IndexAWBMode, m_IndexAnalogGains;
      int m_IndexMetering, m_IndexDRC;
      int m_IndexISO, m_IndexISOValue;
      int m_IndexShutterMode, m_IndexShutterValue;
      int m_IndexWDR;
      int m_IndexDayNight;
      int m_IndexVideoStab, m_IndexFlip, m_IndexReset;
      int m_IndexIRCut;
      int m_IndexOpenIPCDayNight;
      int m_IndexCalibrateHDMI;

      bool m_bDidAnyLiveUpdates;
};