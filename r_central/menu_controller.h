#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"

class MenuController: public Menu
{
   public:
      MenuController();
      virtual void Render();
      virtual void valuesToUI();
      virtual bool periodicLoop();
      virtual void onReturnFromChild(int returnValue);  
      virtual void onSelectItem();

   private:
      MenuItemSelect* m_pItemsSelect[15];
      MenuItemSlider* m_pItemsSlider[10];

      int m_IndexCPU, m_IndexInterfaces, m_IndexPorts;
      int m_IndexPreferences, m_IndexPreferencesUI, m_IndexPlugins, m_IndexReboot;
      int m_IndexUpdate;
      //int m_IndexShowCPUInfo, m_IndexShowVoltage;
      int m_IndexNetwork, m_IndexEncryption;
      int m_IndexVideo, m_IndexTelemetry, m_IndexRecording;
      bool m_bShownHDMIChangeNotif;
      int m_iMustStartUpdate;
      bool m_bWaitingForUserFinishUpdateConfirmation;
      void addMessage(const char* szMessage);
      void addMessage2(const char* szMessage, const char* szMsg2);
      void updateSoftware();
};