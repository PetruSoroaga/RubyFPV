#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"

class MenuControllerRecording: public Menu
{
   public:
      MenuControllerRecording();
      virtual void onShow();     
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();
      
   private:
      MenuItemSelect* m_pItemsSelect[25];
      int m_IndexRecordIndicator, m_IndexRecordArm, m_IndexRecordDisarm, m_IndexRecordButton, m_IndexRecordLED;
      int m_IndexVideoDestination;
};