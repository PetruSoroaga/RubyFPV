#pragma once
#include "menu_objects.h"
#include "menu_item_checkbox.h"

class MenuVehicleSimpleSetup: public Menu
{
   public:
      MenuVehicleSimpleSetup();
      virtual ~MenuVehicleSimpleSetup();

      virtual void onAddToStack();
      virtual void onShow();
      virtual void onReturnFromChild(int iChildMenuId, int returnValue);
      virtual void valuesToUI();
      virtual bool periodicLoop();
      virtual void Render();
      virtual int onBack();
      virtual void onSelectItem();

      void setPairingSetup();

   protected:
      void addSearchItems();
      void addRegularItems();
      void addRadioItems();
      void addItems();
      void renderSearch();
      void sendTelemetrySearchToVehicle();
      void sendTelemetryTypeToVehicle();
      void sendTelemetryPortToVehicle();
      void sendOSDToVehicle();
      void sendNewRadioLinkFrequency(int iVehicleLinkIndex, u32 uNewFreqKhz);
      void computeSendPowerToVehicle(int iVehicleLinkIndex);

      MenuItemSelect* m_pItemsSelect[50];
      MenuItemSlider* m_pItemsSlider[10];

      bool m_bPairingSetup;
      bool m_bSearchingTelemetry;
      int  m_iSearchTelemetryType;
      int  m_iSearchTelemetryPort;
      int  m_iSearchTelemetrySpeed;
      u32 m_uTimeStartCurrentTelemetrySearch;
      int m_iIndexMenuOk;
      int m_iIndexMenuCancel;
      int m_iIndexOSDLayout;
      int m_iIndexTelemetryType;
      int m_iIndexTelemetryPort;
      int m_iIndexCamera;
      int m_iIndexVideo;

      int m_iIndexFreq[MAX_RADIO_INTERFACES];
      u32 m_SupportedChannels[MAX_RADIO_INTERFACES][100];
      int m_SupportedChannelsCount[MAX_RADIO_INTERFACES];
      int m_iIndexTxPowers[MAX_RADIO_INTERFACES];

      int m_iIndexFullConfig;
      int m_iCurrentSerialPortIndexUsedForTelemetry;
};