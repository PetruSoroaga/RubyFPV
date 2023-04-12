#pragma once
#include "menu_items.h"

class MenuItemRange: public MenuItem
{
   public:
     MenuItemRange(const char* title, float min, float max, float current, float step);
     MenuItemRange(const char* title, const char* tooltip, float min, float max, float current, float step);
     virtual ~MenuItemRange();

     void setSufix(const char* sufix);
     void setPrefix(const char* szPrefix);
     void setCurrentValue(float val);
     float getCurrentValue();

     virtual void onKeyUp(bool bIgnoreReversion);
     virtual void onKeyDown(bool bIgnoreReversion);
     virtual void onKeyLeft(bool bIgnoreReversion);
     virtual void onKeyRight(bool bIgnoreReversion);

     virtual float getValueWidth(float maxWidth);
     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);
     virtual void RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection);

   protected:
      char m_szSufix[32];
      char m_szPrefix[32];
      float m_ValueMin;
      float m_ValueMax;
      float m_ValueCurrent;
      float m_ValueStep;
};
