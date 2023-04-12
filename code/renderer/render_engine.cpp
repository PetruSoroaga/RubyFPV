/*
You can use this C/C++ code however you wish (for example, but not limited to:
     as is, or by modifying it, or by adding new code, or by removing parts of the code;
     in public or private projects, in new free or commercial products) 
     only if you get a priori written consent from Petru Soroaga (petrusoroaga@yahoo.com) for your specific use
     and only if this copyright terms are preserved in the code.
     This code is public for learning and academic purposes.
Also, check the licences folder for additional licences terms.
Code written by: Petru Soroaga, 2021-2023
*/

#include "render_engine.h"
//#include "render_engine_ovg.h"
#include "render_engine_raw.h"

#include "../base/base.h"
#include "../base/hardware.h"

static RenderEngine* s_pRenderEngine = NULL;
static bool s_bRenderEngineIsRaw = false;

RenderEngine* render_init_engine()
{
   log_line("Renderer Engine Init...");
   if ( NULL == s_pRenderEngine )
   {
      //int board_type = (hardware_getBoardType() & 0xFF);
      //if ( board_type == BOARD_TYPE_PI4B )
      {
         s_bRenderEngineIsRaw = true;
         s_pRenderEngine = new RenderEngineRaw();
      }
      //else
      //   s_pRenderEngine = new RenderEngineOVG();

   }
   return s_pRenderEngine;
}

bool render_engine_is_raw()
{
   return s_bRenderEngineIsRaw;  
}

void render_free_engine()
{
   if ( NULL != s_pRenderEngine )
      delete s_pRenderEngine;
   s_pRenderEngine = NULL;
}

extern "C" {
void render_engine_test()
{
   printf("\nTest engine\n");
}
}  

RenderEngine* renderer_engine()
{
   if ( NULL == s_pRenderEngine )
      s_pRenderEngine = render_init_engine();
   return s_pRenderEngine;
}

RenderEngine::RenderEngine()
{
   m_iRenderDepth = 0;
   m_iRenderWidth = 0;
   m_iRenderHeight = 0;
   m_fPixelWidth = 0.0;
   m_fPixelHeight = 0.0;

   m_fGlobalAlfa = 1.0;
   m_bEnableFontScaling = false;
   m_bHighlightFirstWord = false;
   m_bDrawBackgroundBoundingBoxes = false;
   m_fBoundingBoxPadding = 0.0;

   m_bDrawStrikeOnTextBackgroundBoundingBoxes = false;
}


RenderEngine::~RenderEngine()
{
}

int RenderEngine::getScreenWidth()
{
   return m_iRenderWidth;
}

int RenderEngine::getScreenHeight()
{
   return m_iRenderHeight;
}

float RenderEngine::getAspectRatio()
{
   return (float)m_iRenderWidth/(float)m_iRenderHeight;
}

float RenderEngine::getPixelWidth()
{
   return m_fPixelWidth;
}

float RenderEngine::getPixelHeight()
{
   return m_fPixelHeight;
}


void RenderEngine::highlightFirstWordOfLine(bool bHighlight)
{
   m_bHighlightFirstWord = bHighlight;
}

bool RenderEngine::drawBackgroundBoundingBoxes(bool bEnable)
{
   bool b = m_bDrawBackgroundBoundingBoxes;
   m_bDrawBackgroundBoundingBoxes = bEnable;
   return b;
}

void RenderEngine::setBackgroundBoundingBoxPadding(float fPadding)
{
   m_fBoundingBoxPadding = fPadding;
}

void RenderEngine::setFontBackgroundBoundingBoxStrikeColor(double* color)
{
   memcpy(m_ColorTextBackgroundBoundingBoxStrike, color, 4*sizeof(double));
   m_bDrawStrikeOnTextBackgroundBoundingBoxes = true;
}

void RenderEngine::clearFontBackgroundBoundingBoxStrikeColor()
{
   m_bDrawStrikeOnTextBackgroundBoundingBoxes = false;
}

float RenderEngine::setGlobalAlfa(float alfa)
{
   float f = m_fGlobalAlfa;
   m_fGlobalAlfa = alfa;
   return f;
}
 
float RenderEngine::getGlobalAlfa()
{
   return m_fGlobalAlfa;
}

void RenderEngine::setColors(double* color)
{
   setColors(color, 1.0);
}

void RenderEngine::setColors(double* color, float fAlfaScale)
{
}

void RenderEngine::setFill(float r, float g, float b, float a)
{
}

void RenderEngine::setStroke(double* color)
{
   setStroke(color, 1.0);
}

void RenderEngine::setStroke(double* color, float fStrokeSize)
{
}

void RenderEngine::setStroke(float r, float g, float b, float a)
{
}

float RenderEngine::getStrokeSize()
{
   return 0.0;
}

void RenderEngine::setStrokeSize(float fStrokeSize)
{
}

void RenderEngine::setFontColor(u32 fontId, double* color)
{
}

void RenderEngine::setFontBackgroundBoundingBoxFillColor(double* color)
{
}

void RenderEngine::enableFontScaling(bool bEnable)
{
   m_bEnableFontScaling = bEnable;
}

u32 RenderEngine::loadFont(const char* szFontFile)
{
   return 0;
}

void RenderEngine::freeFont(u32 idFont)
{
}

u32 RenderEngine::loadImage(const char* szFile)
{
   return 0;
}

void RenderEngine::freeImage(u32 idImage)
{
}

u32 RenderEngine::loadIcon(const char* szFile)
{
   return 0;
}

void RenderEngine::freeIcon(u32 idIcon)
{
}

void RenderEngine::startFrame()
{
}

void RenderEngine::endFrame()
{
}

void RenderEngine::rotate180()
{
}

void RenderEngine::drawImage(float xPos, float yPos, float fWidth, float fHeight, u32 imageId)
{
}

void RenderEngine::drawIcon(float xPos, float yPos, float fWidth, float fHeight, u32 iconId)
{
}

float RenderEngine::getFontHeight(u32 fontId)
{
   return 0.0;
}

float RenderEngine::textHeight(u32 fontId)
{
   return 0.0;
}

float RenderEngine::textHeight(float fTextHeight, u32 fontId)
{
   return fTextHeight;
}

float RenderEngine::textWidth(u32 fontId, const char* szText)
{
   return textWidth(0.0, fontId, szText);
}
     
float RenderEngine::textWidth(float fTextHeight, u32 fontId, const char* szText)
{
   return 0.0;
}

float RenderEngine::textHeightScaled(float fTextHeight, u32 fontId, float fScale)
{
   return fTextHeight*fScale;
}

float RenderEngine::textWidthScaled(float fTextHeight, u32 fontId, float fScale, const char* szText)
{
   return 0.0;
}

void RenderEngine::drawTextScaled(float xPos, float yPos, float fTextHeight, u32 fontId, float fScale, const char* szText)
{
}

void RenderEngine::drawTextLeftScaled(float xPos, float yPos, float fTextHeight, u32 fontId, float fScale, const char* szText)
{
}

void RenderEngine::drawText(float xPos, float yPos, u32 fontId, const char* szText)
{
}

void RenderEngine::drawText(float xPos, float yPos, float fTextHeight, u32 fontId, const char* szText)
{
}

void RenderEngine::drawTextNoOutline(float xPos, float yPos, float fTextHeight, u32 fontId, const char* szText)
{
}

void RenderEngine::drawTextLeft(float xPos, float yPos, u32 fontId, const char* szText)
{
}

void RenderEngine::drawTextLeft(float xPos, float yPos, float fTextHeight, u32 fontId, const char* szText)
{
}

float RenderEngine::getMessageHeight(const char* text, float line_spacing_percent, float max_width, u32 fontId)
{
   return 0.0;
}

float RenderEngine::getMessageHeight(const char* text, float fTextHeight, float line_spacing_percent, float max_width, u32 fontId)
{
   return 0.0;
}

float RenderEngine::drawMessageLines(float xPos, float yPos, const char* text, float fTextHeight, float line_spacing_percent, float max_width, u32 fontId)
{
   return 0.0;
}

float RenderEngine::drawMessageLines(const char* text, float xPos, float yPos, float fTextHeight, float line_spacing_percent, float max_width, u32 fontId)
{
   return drawMessageLines(xPos,yPos, text, fTextHeight, line_spacing_percent, max_width, fontId);
}

void RenderEngine::drawLine(float x1, float y1, float x2, float y2)
{
}

void RenderEngine::drawRect(float xPos, float yPos, float fWidth, float fHeight)
{
}

void RenderEngine::drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius)
{
}

void RenderEngine::drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
}

void RenderEngine::drawPolyLine(float* x, float* y, int count)
{
}

void RenderEngine::fillPolygon(float* x, float* y, int count)
{
}

void RenderEngine::fillCircle(float x, float y, float r)
{
}

void RenderEngine::drawCircle(float x, float y, float r)
{
}

void RenderEngine::drawArc(float x, float y, float r, float a1, float a2)
{
}

bool RenderEngine::rectIntersect(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2)
{
   if ( (x1 + w1 < x2) || (x2+w2 < x1) )
      return false;

   if ( (y1 + h1 < y2) || (y2+h2 < y1) )
      return false;

   return true;
}
