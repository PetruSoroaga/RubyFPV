#pragma once
#include "menu_items.h"

class MenuItemText: public MenuItem
{
   public:
     MenuItemText(const char* title);
     MenuItemText(const char* title, float fScale);
     virtual ~MenuItemText();
     
     virtual bool isSelectable();
     virtual float getItemHeight(float maxWidth);
     virtual float getTitleWidth(float maxWidth);

     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);

   protected:
      float m_fMarginX;
      float m_fScale;
};
