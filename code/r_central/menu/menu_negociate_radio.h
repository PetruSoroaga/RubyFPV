#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"

class MenuNegociateRadio: public Menu
{
   public:
      MenuNegociateRadio();
      virtual ~MenuNegociateRadio();
      virtual void Render();
      virtual void valuesToUI();
      virtual int onBack();
      virtual bool periodicLoop();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);  
      virtual void onSelectItem();
      
      void onReceivedVehicleResponse(u8* pPacketData, int iPacketLength);

   private:
      void _computeQualities();
      void _send_command_to_vehicle();
      void _switch_to_step(int iStep);
      void _onCancel();
      MenuItemSelect* m_pItemsSelect[10];
      int m_MenuIndexCancel;
      char m_szStatusMessage[256];
      int m_iCounter;
      u32 m_uShowTime;
      u32 m_uStepStartTime;
      u32 m_uLastTimeSendCommandToVehicle;
      bool m_bWaitingVehicleConfirmation;
      int m_iStep;
      int m_iDataRateIndex;
      int m_iDataRateTestCount;
      int m_iDataRateToApply;

      int m_iRXPackets[20][MAX_RADIO_INTERFACES][3];
      int m_iRxLostPackets[20][MAX_RADIO_INTERFACES][3];
      float m_fQualities[20];
};