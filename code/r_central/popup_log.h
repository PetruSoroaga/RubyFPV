#pragma once
#include "popup.h"

class PopupLog: public Popup
{
   public:
      PopupLog();
      virtual ~PopupLog();

      virtual void addLine(const char* szLine);
      virtual void Render();

      void showLineNumbers(bool bShow);
      void setMaxRenderLines(int count);
      void setIsLiveLog(bool b);

   protected:
      virtual void computeSize();

      int m_MaxRenderLines;
      int m_TotalLinesAdded;
      bool m_bShowLineNumbers;
};

void popup_log_add_entry(const char* szLine);
void popup_log_add_entry(const char* szLine, int param);
void popup_log_add_entry(const char* szLine, const char* szParam);
void popup_log_set_show_flag(int flag);
void popup_log_vehicle_remove();
void popup_log_add_vehicle_entry(const char* szLine);
