#pragma once
#include "menu_objects.h"


class MenuConfirmationDeleteLogs: public Menu
{
   public:
      MenuConfirmationDeleteLogs(u32 uFreeSpaceMb, u32 uLogsSizeBytes);
      virtual ~MenuConfirmationDeleteLogs();
      virtual void onShow();
      virtual int onBack();
      virtual void onSelectItem();

   protected:
      u32 m_uFreeSpaceMb;
      u32 m_uLogSizeBytes;
};