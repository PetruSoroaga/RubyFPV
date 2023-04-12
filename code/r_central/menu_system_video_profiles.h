#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuSystemVideoProfiles: public Menu
{
   public:
      MenuSystemVideoProfiles();
      virtual void onShow(); 
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int returnValue);
      virtual void onSelectItem();

   private:
      void sendVideoLinkProfiles();

      MenuItemSelect* m_pItemsSelect[200];
      MenuItemSlider* m_pItemsSlider[200];

      int m_IndexRetransmitWindow;
      int m_IndexRetransmissionDuplication;
      int m_IndexRetransmitRetryTimeout;
      int m_IndexRetransmitRequestOnTimeout;
      int m_IndexDisableRetransmissionsTimeout;
      int m_IndexDefaultAutoKeyframe;
      int m_IndexVideoAdjustStrength;
      int m_IndexLowestAllowedVideoBitrate;

      int m_IndexVideoProfile_VideoRadioRate[8];
      int m_IndexVideoProfile_DataRadioRate[8];
      int m_IndexVideoProfile_PacketLength[8];
      int m_IndexVideoProfile_BlockPackets[8];
      int m_IndexVideoProfile_BlockFECs[8];
      int m_IndexVideoProfile_H264Profile[8];
      int m_IndexVideoProfile_H264Level[8];
      int m_IndexVideoProfile_H264Refresh[8];
      int m_IndexVideoProfile_Bitrate[8];
      int m_IndexVideoProfile_RetransmissionWindow[8];
      int m_IndexVideoProfileTransmissionDuplication[8];
      int m_IndexVideoProfile_KeyFrame[8];
      int m_IndexVideoProfile_AutoKeyFrame[8];
      int m_IndexVideoProfile_FPS[8];
      int m_IndexVideoProfile_Reset[8];


      int m_IndexSwitchUsingQAButton;
      int m_IndexSwitchToHQVideo;
      int m_IndexSwitchToMQVideo;
      int m_IndexSwitchToLQVideo;
      int m_IndexSwitchToCustomVideoLevel;
};