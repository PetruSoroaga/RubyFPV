#pragma once
#include "menu_items.h"

class MenuItemText: public MenuItem
{
   public:
     MenuItemText(const char* title);
     MenuItemText(const char* title, bool bUseSmallText);
     MenuItemText(const char* title, float fScale);
     virtual ~MenuItemText();
     
     void setSmallText();
     virtual bool isSelectable();
     virtual float getItemHeight(float maxWidth);
     virtual float getTitleWidth(float maxWidth);

     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);

   protected:
      bool m_bUseSmallText;
      float m_fMarginX;
      float m_fScale;
};
