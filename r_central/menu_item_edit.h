#pragma once
#include "menu_items.h"

#define MAX_EDIT_LENGTH 32

class MenuItemEdit: public MenuItem
{
   public:
     MenuItemEdit(const char* title, const char* szValue);
     MenuItemEdit(const char* title, const char* tooltip, const char* szValue);
     virtual ~MenuItemEdit();
     void setOnlyLetters();
     void setMaxLength(int l);
     void setCurrentValue(const char* szValue);
     const char* getCurrentValue();

     virtual void beginEdit();
     virtual void onKeyUp(bool bIgnoreReversion);
     virtual void onKeyDown(bool bIgnoreReversion);
     virtual void onKeyLeft(bool bIgnoreReversion);
     virtual void onKeyRight(bool bIgnoreReversion);

     virtual float getValueWidth(float maxWidth);
     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);

   protected:
      char m_szValue[MAX_EDIT_LENGTH+2];
      char m_szValueOriginal[MAX_EDIT_LENGTH+2];
      int m_EditPos;
      int m_MaxLength;
      bool m_bOnlyLetters;
};
