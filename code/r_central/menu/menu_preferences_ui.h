#pragma once
#include "menu_objects.h"
#include "menu_item_select.h"

class MenuPreferencesUI: public Menu
{
   public:
      MenuPreferencesUI(bool bShowOnlyOSD = false);
      virtual void onShow();     
      virtual void Render();
      virtual void onSelectItem();
      virtual void valuesToUI();
      
   private:
      bool m_bShowOnlyOSD;
      MenuItemSelect* m_pItemsSelect[20];
      int m_IndexScaleMenu, m_IndexMenuStacked;
      int m_IndexOSDSize, m_IndexOSDFlip;
      int m_IndexInvertColors;
      int m_IndexColorPickerOSD;
      int m_IndexColorPickerOSDOutline;
      int m_IndexOSDOutlineThickness;
      int m_IndexColorPickerAHI;
      int m_IndexOSDFont;
      int m_IndexMonitor;
      int m_IndexUnits;
      int m_IndexPersistentMessages;
      int m_IndexLogWindow;
};
