#pragma once
#include "menu_items.h"

#define MAX_MENU_ITEM_SELECTIONS 100

class MenuItemSelectBase: public MenuItem
{
   public:
     MenuItemSelectBase(const char* title);
     MenuItemSelectBase(const char* title, const char* tooltip);
     virtual ~MenuItemSelectBase();

     void removeAllSelections();
     void addSelection(const char* szText);
     void addSelection(const char* szText, bool bEnabled);
     void setSelection(int index);
     void setSelectedIndex(int index);
     void setSelectionIndexDisabled(int index);
     void setSelectionIndexEnabled(int index);

     void disableClick();
     int getSelectedIndex();
     int getSelectionsCount();
     void restoreSelectedIndex();
     virtual void beginEdit();
     virtual void endEdit(bool bCanceled);
     virtual void onClick();
     virtual void onKeyUp(bool bIgnoreReversion);
     virtual void onKeyDown(bool bIgnoreReversion);
     virtual void onKeyLeft(bool bIgnoreReversion);
     virtual void onKeyRight(bool bIgnoreReversion);
     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);
     virtual void RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection);

   protected:
      bool m_bDisabledClick;
      int m_SelectionsCount;
      int m_SelectedIndex;
      int m_SelectedIndexBeforeEdit;
      char* m_szSelections[MAX_MENU_ITEM_SELECTIONS];
      bool m_bEnabledItems[MAX_MENU_ITEM_SELECTIONS];
};
