#pragma once

#define MENU_ITEM_TYPE_SIMPLE 0
#define MENU_ITEM_TYPE_SECTION 1

class Menu;

class MenuItem
{
   public:
     MenuItem(const char* title);
     MenuItem(const char* title, const char* tooltip);
     virtual ~MenuItem();

     void setTitle(const char* title);
     char* getTitle();
     void setTooltip(const char* tooltip);
     char* getTooltip();

     virtual void invalidate();

     bool isEnabled();
     virtual bool isSelectable();
     virtual void setEnabled(bool enabled);
     void showArrow();

     void setCondensedOnly();

     void setTextColor(double* pColor);

     void setIsEditable();
     void setNotEditable();
     bool isEditable();
     bool isEditing();
     virtual void beginEdit();
     virtual void endEdit(bool bCanceled);
     virtual void onClick();

     virtual bool preProcessKeyUp();
     virtual bool preProcessKeyDown();
     virtual bool preProcessKeyLeft();
     virtual bool preProcessKeyRight();

     virtual void onKeyUp(bool bIgnoreReversion) {};   // for editing items
     virtual void onKeyDown(bool bIgnoreReversion) {};
     virtual void onKeyLeft(bool bIgnoreReversion) {};
     virtual void onKeyRight(bool bIgnoreReversion) {};

     virtual float getItemHeight(float maxWidth);
     virtual float getTitleWidth(float maxWidth);
     virtual float getValueWidth(float maxWidth);
     virtual void Render(float xPos, float yPos, bool bSelected, float fWidthSelection);
     virtual void RenderCondensed(float xPos, float yPos, bool bSelected, float fWidthSelection);
     int m_ItemType;
     Menu* m_pMenu;

     void setLastRenderPos(float xPos, float yPos);
     void RenderBaseTitle(float xPos, float yPos, bool bSelected, float fWidthSelection);

     char* m_pszTitle;
     char* m_pszTooltip;
     bool m_bEnabled;
     bool m_bIsEditable;
     bool m_bIsEditing;
     bool m_bShowArrow;
     bool m_bCondensedOnly;

     float m_RenderTitleWidth;
     float m_RenderTitleHeight;
     float m_RenderValueWidth;
     float m_RenderWidth;
     float m_RenderHeight;
     float m_RenderLastY;
     float m_RenderLastX;

     bool  m_bCustomTextColor;
     double m_TextColor[4];
};
