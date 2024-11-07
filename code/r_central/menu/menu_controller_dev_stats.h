#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuControllerDevStatsConfig: public Menu
{
   public:
      MenuControllerDevStatsConfig();
      virtual void onShow(); 
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      void addItems();
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[15];

      int m_IndexQAButton;
      int m_IndexShowRXVideoDataPackets;  
      int m_IndexShowRxH264Frames;
      int m_IndexShowRxDBM;
      int m_IndexShowRxMissingPackets;
      int m_IndexShowRxMissingPacketsMaxGap;
      int m_IndexShowRxConsumedPackets;
      int m_IndexShowMinMaxAckTime;
      int m_IndexShowRxMaxECUsed;
      int m_IndexShowUnrecoverableVideoBlocks;
      int m_IndexShowVideoProfileChanges;
      int m_IndexShowVideoRetransmissions;
};