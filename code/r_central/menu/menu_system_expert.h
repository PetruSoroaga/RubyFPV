#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuSystemExpert: public Menu
{
   public:
      MenuSystemExpert();
      virtual void onShow(); 
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[15];

      int m_IndexDevStats;
      int m_IndexPacket;
      int m_IndexClockSync;
      int m_IndexPingClockSpeed;
      int m_IndexRadioSilence;
      int m_IndexWiFiChangeDelay;
      int m_IndexVideoProfiles;
      int m_IndexVideoOverload;
      int m_IndexFrameType;
      int m_IndexSlotTime, m_IndexThresh62;
      int m_IndexLogs;
      int m_IndexInjectMinorFaults;
      int m_IndexInjectFaults;
      int m_IndexRenderFSP;
      int m_IndexCPULoad;
      int m_IndexFreezeOSD;
      int m_IndexResetDev;
      int m_IndexDetailedPackets;
      int m_IndexRXScope;
      int m_IndexVersion;
      int m_IndexRxLoopTimeout;
};