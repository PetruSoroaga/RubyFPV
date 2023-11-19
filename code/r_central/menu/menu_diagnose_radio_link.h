#pragma once
#include "menu_objects.h"


class MenuDiagnoseRadioLink: public Menu
{
   public:
      MenuDiagnoseRadioLink(int iVehicleRadioLinkIndex);
      virtual ~MenuDiagnoseRadioLink();
      virtual void Render();
      virtual void onSelectItem();

      void onReceivedControllerData(u8* pData, int iDataLength);
      void onReceivedVehicleData(u8* pData, int iDataLength);

   protected:
      int m_iVehicleRadioLinkIndex;
      bool m_bWaitingForData;
      bool m_bFailedToGetInfo;
      u8 m_uDataFromVehicle[1024];
      u8 m_uDataFromController[1024];
};