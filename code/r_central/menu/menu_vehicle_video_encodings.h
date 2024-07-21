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

      int m_IndexPacketSize, m_IndexBlockPackets, m_IndexBlockFECs, m_IndexECSchemeSpread;
      int m_IndexDataRate;
      int m_IndexH264Profile, m_IndexH264Level, m_IndexH264Refresh, m_IndexH264Headers;
      int m_IndexH264SPSTimings;
      int m_IndexH264Slices;
      int m_IndexCustomQuant, m_IndexQuantValue;
      int m_IndexResetParams;
      int m_IndexEnableAdaptiveQuantization;
      int m_IndexAdaptiveH264QuantizationStrength;
      int m_IndexHDMIOutput;

      bool m_ShowBitrateWarning;
      MenuItemSlider* m_pItemsSlider[25];
      MenuItemSelect* m_pItemsSelect[40];
};