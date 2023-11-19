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

#include "../base/base.h"
#include "../base/config.h"
#include "../base/hdmi.h"
#include "../base/ctrl_preferences.h"
#include "render_engine.h"
#include "../public/render_engine_ui.h"
#include "../r_central/colors.h"

RenderEngine* s_pRenderEngineUI = NULL;
u32 s_uRenderEngineUIFontIdSmall = 0;
u32 s_uRenderEngineUIFontIdRegular = 0;
u32 s_uRenderEngineUIFontIdBig = 0;
u32 s_uRenderEngineUIFontsListSizes[100];

RenderEngineUI::RenderEngineUI()
{
   for( int i=0; i<100; i++ )
      s_uRenderEngineUIFontsListSizes[i] = 0;

   if ( NULL == s_pRenderEngineUI )
      s_pRenderEngineUI = renderer_engine();
}

RenderEngineUI::~RenderEngineUI()
{
}

int RenderEngineUI::getScreenWidth()
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_pRenderEngineUI->getScreenWidth();
}

int RenderEngineUI::getScreenHeight()
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_pRenderEngineUI->getScreenHeight();
}

float RenderEngineUI::getAspectRatio()
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->getAspectRatio();
}

float RenderEngineUI::getPixelWidth()
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->getPixelWidth();
}

float RenderEngineUI::getPixelHeight()
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->getPixelHeight();
}

unsigned int RenderEngineUI::getFontIdSmall()
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_uRenderEngineUIFontIdSmall;
}

unsigned int RenderEngineUI::getFontIdRegular()
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_uRenderEngineUIFontIdRegular;
}
     
unsigned int RenderEngineUI::getFontIdBig()
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_uRenderEngineUIFontIdBig;
}

unsigned int RenderEngineUI::loadFontSize(float fSize)
{
   fSize *= hdmi_get_current_resolution_height();
   int nSize = fSize/2;
   nSize *= 2;
   if ( nSize < 14 )
      nSize = 14;
   if ( nSize > 56 )
      nSize = 56;

   if ( s_uRenderEngineUIFontsListSizes[nSize] == MAX_U32 )
      return 0;
   if ( s_uRenderEngineUIFontsListSizes[nSize] != 0 )
      return s_uRenderEngineUIFontsListSizes[nSize];

   Preferences* pP = get_Preferences();

   char szFont[32];
   char szFileName[256];

   strcpy(szFont, "raw_bold");
   if ( pP->iOSDFont == 0 )
      strcpy(szFont, "raw_bold");
   if ( pP->iOSDFont == 1 )
      strcpy(szFont, "rawobold");
   if ( pP->iOSDFont == 2 )
      strcpy(szFont, "ariobold");
   if ( pP->iOSDFont == 3 )
      strcpy(szFont, "bt_bold");

   sprintf(szFileName, "res/font_%s_%d.dsc", szFont, nSize );
   if ( access( szFileName, R_OK ) == -1 )
   {
      log_softerror_and_alarm("[RenderEngineOSD] Can't find font: [%s]. Using a default font instead.", szFileName );
      sprintf(szFileName, "res/font_rawobold_%d.dsc", nSize );
      if ( access( szFileName, R_OK ) == -1 )
      {
         log_softerror_and_alarm("[RenderEngineOSD] Can't find font: [%s]. Using a default size instead.", szFileName );
         nSize = 56;
         sprintf(szFileName, "res/font_rawobold_%d.dsc", nSize );
      }
   }
   
   s_uRenderEngineUIFontsListSizes[nSize] = s_pRenderEngineUI->loadFont(szFileName);
   if ( 0 == s_uRenderEngineUIFontsListSizes[nSize] )
   {
      log_softerror_and_alarm("[RenderEngineOSD] Can't load font %s (%s)", szFont, szFileName);
      s_uRenderEngineUIFontsListSizes[nSize] = MAX_U32;
      return 0;
   }
   log_line("[RenderEngineOSD] Loaded font: %s (%s)", szFont, szFileName);
   return s_uRenderEngineUIFontsListSizes[nSize];
}

double* RenderEngineUI::getColorOSDText()
{
   return get_Color_OSDText();
}

double* RenderEngineUI::getColorOSDInstruments()
{
   Preferences* pP = get_Preferences();
   if ( NULL == pP )
      return getColorOSDText();

   static double s_ColorRenderUIInstruments[4];

   s_ColorRenderUIInstruments[0] = pP->iColorAHI[0];
   s_ColorRenderUIInstruments[1] = pP->iColorAHI[1];
   s_ColorRenderUIInstruments[2] = pP->iColorAHI[2];
   s_ColorRenderUIInstruments[3] = pP->iColorAHI[3]/100.0;
   return &s_ColorRenderUIInstruments[0];
}

double* RenderEngineUI::getColorOSDOutline()
{
   static double s_ColorRenderUIOutline[4] = { 0,0,0, 0.4 };
   return &s_ColorRenderUIOutline[0];
}

double* RenderEngineUI::getColorOSDWarning()
{
   return get_Color_IconError();
}

unsigned int RenderEngineUI::loadImage(const char* szFile)
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_pRenderEngineUI->loadImage(szFile);
}
 
void RenderEngineUI::freeImage(unsigned int idImage)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->freeImage(idImage);
}

unsigned int RenderEngineUI::loadIcon(const char* szFile)
{
   if ( NULL == s_pRenderEngineUI )
      return 0;
   return s_pRenderEngineUI->loadIcon(szFile);
}

void RenderEngineUI::freeIcon(unsigned int idIcon)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->freeIcon(idIcon);
}


void RenderEngineUI::highlightFirstWordOfLine(bool bHighlight)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->highlightFirstWordOfLine(bHighlight);
}

bool RenderEngineUI::drawBackgroundBoundingBoxes(bool bEnable)
{
   if ( NULL == s_pRenderEngineUI )
      return false;
   return s_pRenderEngineUI->drawBackgroundBoundingBoxes(bEnable);
}

void RenderEngineUI::setColors(double* color)
{
   if ( NULL == s_pRenderEngineUI )
      return ;
   s_pRenderEngineUI->setColors(color);
}

void RenderEngineUI::setColors(double* color, float fAlfaScale)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setColors(color, fAlfaScale);
}

void RenderEngineUI::setFill(float r, float g, float b, float a)
{
   if ( NULL == s_pRenderEngineUI )
      return ;
   s_pRenderEngineUI->setFill(r,g,b,a);
}

void RenderEngineUI::setStroke(double* color)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setStroke(color);
}

void RenderEngineUI::setStroke(double* color, float fStrokeSize)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setStroke(color, fStrokeSize);
}

void RenderEngineUI::setStroke(float r, float g, float b, float a)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setStroke(r,g,b,a);
}

float RenderEngineUI::getStrokeSize()
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->getStrokeSize();
}

void RenderEngineUI::setStrokeSize(float fStrokeSize)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setStrokeSize(fStrokeSize);
}

void RenderEngineUI::setFontColor(unsigned int fontId, double* color)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->setFontColor(fontId, color);
}

void RenderEngineUI::drawImage(float xPos, float yPos, float fWidth, float fHeight, unsigned int imageId)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawImage(xPos, yPos, fWidth, fHeight, imageId);
}

void RenderEngineUI::drawIcon(float xPos, float yPos, float fWidth, float fHeight, unsigned int iconId)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawIcon(xPos, yPos, fWidth, fHeight, iconId);
}

float RenderEngineUI::textHeight(unsigned int fontId)
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->textHeight(fontId);
}

float RenderEngineUI::textWidth(unsigned int fontId, const char* szText)
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->textWidth(fontId, szText);
}

void RenderEngineUI::drawText(float xPos, float yPos, unsigned int fontId, const char* szText)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawText(xPos, yPos, fontId, szText);
}

void RenderEngineUI::drawTextLeft(float xPos, float yPos, unsigned int fontId, const char* szText)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawTextLeft(xPos, yPos, fontId, szText);
}

float RenderEngineUI::getMessageHeight(const char* text, float line_spacing_percent, float max_width, unsigned int fontId)
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->getMessageHeight(text, line_spacing_percent, max_width, fontId);
}

float RenderEngineUI::drawMessageLines(const char* text, float xPos, float yPos, float line_spacing_percent, float max_width, unsigned int fontId)
{
   if ( NULL == s_pRenderEngineUI )
      return 0.0;
   return s_pRenderEngineUI->drawMessageLines(text, xPos, yPos, line_spacing_percent, max_width, fontId);
}

void RenderEngineUI::drawLine(float x1, float y1, float x2, float y2)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawLine(x1,y1,x2,y2);
}

void RenderEngineUI::drawRect(float xPos, float yPos, float fWidth, float fHeight)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawRect(xPos,yPos,fWidth, fHeight);
}

void RenderEngineUI::drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawRoundRect(xPos,yPos,fWidth, fHeight, fCornerRadius);
}
 
void RenderEngineUI::drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawTriangle(x1,y1,x2,y2,x3,y3);
}

void RenderEngineUI::drawPolyLine(float* x, float* y, int count)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawPolyLine(x,y,count);
}

void RenderEngineUI::fillPolygon(float* x, float* y, int count)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->fillPolygon(x,y,count);
}

void RenderEngineUI::fillCircle(float x, float y, float r)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->fillCircle(x,y,r);
}

void RenderEngineUI::drawCircle(float x, float y, float r)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawCircle(x,y,r);
}

void RenderEngineUI::drawArc(float x, float y, float r, float a1, float a2)
{
   if ( NULL == s_pRenderEngineUI )
      return;
   s_pRenderEngineUI->drawArc(x,y,r,a1,a2);
}
