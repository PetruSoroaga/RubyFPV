#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"

#define MAX_NEGOCIATE_TESTS 100

typedef struct
{
   int iDataRateToTest;
   u32 uFlagsToTest;
   int iRadioInterfacesRXPackets[MAX_RADIO_INTERFACES];
   int iRadioInterfacesRxLostPackets[MAX_RADIO_INTERFACES];
   float fQualityCards[MAX_RADIO_INTERFACES];
   float fComputedQuality;
} type_negociate_radio_step;

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
      float _getComputedQualityForDatarate(int iDatarate, int* pTestIndex);
      float _getMinComputedQualityForDatarate(int iDatarate, int* pTestIndex);
      void _send_command_to_vehicle();
      void _switch_to_step(int iStep);
      void _advance_to_next_step();
      void _compute_datarate_settings_to_apply();
      void _apply_new_settings();
      void _onCancel();
      MenuItemSelect* m_pItemsSelect[10];
      int m_MenuIndexCancel;
      char m_szStatusMessage[256];
      char m_szStatusMessage2[256];
      int m_iLoopCounter;
      u32 m_uShowTime;
      u32 m_uStepStartTime;
      u32 m_uLastTimeSendCommandToVehicle;
      bool m_bWaitingVehicleConfirmation;
      bool m_bWaitingCancelConfirmationFromUser;
      int m_iStep;
      float m_fQualityOfDataRateToApply;
      int m_iIndexTestDataRateToApply;
      int m_iDataRateToApply;
      u32 m_uRadioFlagsToApply;
      int m_iCountSucceededSteps;
      int m_iCountFailedSteps;

      type_negociate_radio_step m_TestStepsInfo[MAX_NEGOCIATE_TESTS];
      int m_iTestsCount;
      int m_iIndexFirstRadioFlagsTest;
      int m_iCurrentTestIndex;
};