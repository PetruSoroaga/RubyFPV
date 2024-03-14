#pragma once

#include "../base/base.h"

class RenderEngine
{
   public:
     RenderEngine();
     virtual ~RenderEngine();

     int getScreenWidth();
     int getScreenHeight();
     float getAspectRatio();
     float getPixelWidth();
     float getPixelHeight();

     float setGlobalAlfa(float alfa);
     float getGlobalAlfa();

     virtual void highlightFirstWordOfLine(bool bHighlight);
     virtual bool drawBackgroundBoundingBoxes(bool bEnable);

     virtual void setColors(double* color);
     virtual void setColors(double* color, float fAlfaScale);
     virtual void setFill(float r, float g, float b, float a);
     virtual void setStroke(double* color);
     virtual void setStroke(double* color, float fStrokeSize); 
     virtual void setStroke(float r, float g, float b, float a);
     virtual float getStrokeSize();
     virtual void setStrokeSize(float fStrokeSize);
     virtual void setFontColor(u32 fontId, double* color);
     virtual void enableFontScaling(bool bEnable);
     virtual void setFontBackgroundBoundingBoxFillColor(double* color);
     virtual void setFontBackgroundBoundingBoxStrikeColor(double* color);
     virtual void clearFontBackgroundBoundingBoxStrikeColor();
     virtual void setBackgroundBoundingBoxPadding(float fPadding);

     virtual int loadFont(const char* szFontFile);
     virtual void freeFont(u32 idFont);
     virtual u32 loadImage(const char* szFile);
     virtual void freeImage(u32 idImage);
     virtual u32 loadIcon(const char* szFile);
     virtual void freeIcon(u32 idIcon);

     virtual void startFrame();
     virtual void endFrame();

     virtual void rotate180();

     virtual void drawImage(float xPos, float yPos, float fWidth, float fHeight, u32 imageId);
     virtual void drawIcon(float xPos, float yPos, float fWidth, float fHeight, u32 iconId);

     virtual float getFontHeight(u32 fontId);
     virtual float textHeight(u32 fontId);
     virtual float textWidth(u32 fontId, const char* szText);
     virtual float textHeightScaled(u32 fontId, float fScale);
     virtual float textWidthScaled(u32 fontId, float fScale, const char* szText);
     virtual void drawText(float xPos, float yPos, u32 uFontId, const char* szText);
     virtual void drawTextNoOutline(float xPos, float yPos, u32 fontId, const char* szText);
     virtual void drawTextLeft(float xPos, float yPos, u32 fontId, const char* szText);
     virtual void drawTextScaled(float xPos, float yPos, u32 fontId, float fScale, const char* szText);
     virtual void drawTextLeftScaled(float xPos, float yPos, u32 fontId, float fScale, const char* szText);
     virtual float getMessageHeight(const char* text, float line_spacing_percent, float max_width, u32 fontId);
     virtual float drawMessageLines(float xPos, float yPos, const char* text, float line_spacing_percent, float max_width, u32 fontId);
     virtual float drawMessageLines(const char* text, float xPos, float yPos, float line_spacing_percent, float max_width, u32 fontId);

     virtual void drawLine(float x1, float y1, float x2, float y2);
     virtual void drawRect(float xPos, float yPos, float fWidth, float fHeight);
     virtual void drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius);
     virtual void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
     virtual void fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
     virtual void drawPolyLine(float* x, float* y, int count);
     virtual void fillPolygon(float* x, float* y, int count);

     virtual void fillCircle(float x, float y, float r);
     virtual void drawCircle(float x, float y, float r);
     virtual void drawArc(float x, float y, float r, float a1, float a2);

     bool rectIntersect(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2);

   protected:
      int m_iRenderDepth;
      int m_iRenderWidth;
      int m_iRenderHeight;
      float m_fPixelWidth;
      float m_fPixelHeight;

      float m_fGlobalAlfa;
      bool m_bEnableFontScaling;
      bool m_bHighlightFirstWord;
      bool m_bDrawBackgroundBoundingBoxes;
      float m_fBoundingBoxPadding;

      double m_ColorTextBackgroundBoundingBoxStrike[4];
      bool  m_bDrawStrikeOnTextBackgroundBoundingBoxes;
};



RenderEngine* render_init_engine();
RenderEngine* renderer_engine();
bool render_engine_is_raw();

void render_free_engine();

extern "C" {

void render_engine_test();
}  

