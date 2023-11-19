#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuVehicleVideo: public Menu
{
   public:
      MenuVehicleVideo();
      virtual ~MenuVehicleVideo();
      virtual void Render();
      virtual void onReturnFromChild(int returnValue);  
      virtual void onSelectItem();
      virtual void valuesToUI();
            
   private:
      int m_IndexRes, m_IndexFPS, m_IndexKeyframe, m_IndexAutoKeyframe, m_IndexMaxKeyFrame;
      int m_IndexFixedBitrate;
      int m_IndexVideoProfile;
      int m_IndexExpert;
      int m_IndexForceCameraMode;
      int m_IndexHDMIOutput;
      MenuItemSlider* m_pItemsSlider[15];
      MenuItemSelect* m_pItemsSelect[15];

      void showFPSWarning(int w, int h, int fps);
      void sendVideoLinkProfiles();
};