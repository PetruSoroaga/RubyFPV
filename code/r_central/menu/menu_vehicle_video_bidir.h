#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_radio.h"
#include "menu_item_checkbox.h"
#include "menu_item_text.h"
#include "../../base/video_capture_res.h"

class MenuVehicleVideoBidirectional: public Menu
{
   public:
      MenuVehicleVideoBidirectional();
      virtual ~MenuVehicleVideoBidirectional();
      virtual void Render();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);  
      virtual void onSelectItem();
      virtual void valuesToUI();
            
   private:
      int m_IndexAutoKeyframe;
      int m_IndexAdaptiveVideo;
      int m_IndexRetransmissions;
      int m_IndexAdaptiveVideoLevel, m_IndexAdaptiveAlgorithm, m_IndexAdaptiveUseControllerToo;
      int m_IndexVideoLinkLost, m_IndexVideoAdjustStrength;
      int m_IndexRetransmissionsFast;
      int m_IndexMaxKeyFrame;
      int m_IndexRetransmissionsAlgo;
      MenuItemSlider* m_pItemsSlider[15];
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemRadio* m_pItemsRadio[5];

      void sendVideoLinkProfiles();
};