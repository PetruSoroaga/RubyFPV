#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleVideoEncodings: public Menu
{
   public:
      MenuVehicleVideoEncodings();
      virtual void onShow();
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();
      
   private:
      void sendVideoLinkProfile();
      void sendVideoParams();

      int m_IndexPacketSize, m_IndexBlockPackets, m_IndexBlockFECs, m_IndexECSchemeSpread;
      //int m_IndexDataRate;
      int m_IndexH264Profile, m_IndexH264Level, m_IndexH264Refresh;
      int m_IndexRemoveH264PPS, m_IndexInsertH264PPS, m_IndexInsertH264SPSTimings;
      int m_IndexH264Slices;
      int m_IndexIPQuantizationDelta;
      int m_IndexCustomQuant, m_IndexQuantValue;
      int m_IndexResetParams;
      int m_IndexEnableAdaptiveQuantization;
      int m_IndexAdaptiveH264QuantizationStrength;
      int m_IndexHDMIOutput;

      //bool m_ShowBitrateWarning;
      MenuItemSlider* m_pItemsSlider[25];
      MenuItemSelect* m_pItemsSelect[40];
};