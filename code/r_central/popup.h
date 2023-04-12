#pragma once
#include "../base/base.h"

#define MAX_POPUPS 20
#define MAX_POPUP_LINES 30

#define POPUP_MARGINS 0.02
#define POPUP_ROUND_MARGIN 0.02
#define POPUP_FONT_SIZE 0.021
#define POPUP_TITLE_SCALE 1.2  // percentage of regular line height
extern float POPUP_LINE_SPACING;

class Popup
{
   public:
   Popup(const char* title, float x, float y, float timeoutSec);
   Popup(const char* title, float x, float y, float maxWidth, float timeoutSec);
   Popup(bool bCentered, const char* title, float timeoutSec);
   virtual ~Popup();
   virtual void onShow();
   virtual void Render();

   void setFont(u32 idFont);
   u32 getFontId();
   const char* getTitle();
   void setNoBackground();
   void setBackgroundAlpha(float fAlpha);
   void invalidate();
   int isExpired();
   void resetTimer();
   void setTimeout(float timoutSeconds);
   void setTitle(const char* szTitle);
   virtual void addLine(const char* szLine);
   void removeLastLine();
   void removeAllLines();
   void refresh();
   void setMaxWidth(float width);
   void setFixedWidth(float width);
   void setCentered();
   void setBottomAlign(bool b);
   void setBottomMargin(float dymargin);
   void setCustomPadding(float fPadding);
   void setIconId(u32 idIcon, double* pColor);
   float getRenderHeight();
   void setXPos(float xPos);

   float m_xPos;
   float m_yPos;

   protected:
      bool m_bInvalidated;
      bool m_bTopmost;
      bool m_bCentered;
      bool m_bBottomAlign;
      bool m_bNoBackground;
      u32 m_idFont;
      u32 m_idIcon;
      double* m_pColorIcon;
      float m_fIconSize;
      float m_fBackgroundAlpha;
      float m_fBottomMargin;
      float m_fPadding; // as percent 0 to 1 of font height

      float m_fMaxWidth;
      float m_fFixedWidth;
      float m_RenderXPos;
      float m_RenderYPos;
      float m_RenderWidth;
      float m_RenderHeight;
      char m_szTitle[256];
      char* m_szLines[MAX_POPUP_LINES];
      int m_LinesCount;
      float m_fTimeoutSeconds;
      u32 m_StartTime;

      virtual void computeSize();
};

void popups_add(Popup* p);
void popups_add_topmost(Popup* p);
bool popups_has_popup(Popup* p);

void popups_remove(Popup* p);
void popups_remove_all(Popup* pExceptionPopup = NULL);
void popups_render();
void popups_render_topmost();
void popups_invalidate_all();

int popups_get_count();
int popups_get_topmost_count();
Popup* popups_get_at(int index);
Popup* popups_get_topmost_at(int index);
