#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuControllerExpert: public Menu
{
   public:
      MenuControllerExpert();
      virtual void onShow(); 
      virtual void Render();
      virtual void valuesToUI();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);      
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[20];
      MenuItemSlider* m_pItemsSlider[15];

      int m_iIndexCoresAdjustment;
      int m_iIndexPrioritiesAdjustment;
      int m_IndexNiceRouter, m_IndexIONiceRouter, m_IndexIONiceRouterValue;
      int m_IndexNiceCentral;
      int m_IndexAutoRxVideo, m_IndexNiceRXVideo, m_IndexIONiceRXVideo, m_IndexIONiceValueRXVideo;

      int m_IndexEnableRadioThreadsPriority;
      int m_IndexRadioRxPriority;
      int m_IndexRadioTxPriority;

      int m_IndexCPUEnabled;
      int m_IndexGPUEnabled;
      int m_IndexVoltageEnabled;
      int m_IndexCPUSpeed;
      int m_IndexGPUSpeed;
      int m_IndexVoltage;
      int m_IndexReset;
      int m_IndexVersions;
      int m_IndexReboot;

      int m_iDefaultARMFreq;
      int m_iDefaultGPUFreq;
      int m_iDefaultVoltage;

      void addTopInfo();
      void readConfigFile();
      void writeConfigFile();


};