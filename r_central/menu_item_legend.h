#pragma once
#include "menu_items.h"

class MenuItemLegend: public MenuItem
{
   public:
     MenuItemLegend(const char* szTitle, const char* szDesc, float maxWidth);
     virtual ~MenuItemLegend();
     
     virtual float getItemHeight(float maxWidth);
     virtual float getTitleWidth(float maxWidth);

     virtual float getValueWidth(float maxWidth);

     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);


   protected:
      float m_fMarginX;
      float m_fMaxWidth;
};
