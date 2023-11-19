#pragma once

#include "render_engine.h"
#include "fbg_dispmanx.h"

#define MAX_FONT_CHARS 256
#define MAX_FONT_KERINGS 1024
#define MAX_RAW_FONTS 100
#define MAX_RAW_IMAGES 100
#define MAX_RAW_ICONS 100

typedef struct
{
   int charId;
   int imgXOffset;
   int imgYOffset;
   int width;
   int height;
   int xOffset;
   int yOffset;
   int xAdvance;
} RenderEngineRawFontChar;

typedef struct
{
   struct _fbg_img* pImage;
   int lineHeight;
   int baseLine;
   int charCount;
   int charIdFirst;
   int charIdLast;
   RenderEngineRawFontChar chars[MAX_FONT_CHARS];

   int keringsCount;
   int kerings[MAX_FONT_KERINGS][3]; // first char, second char, kering distance

   float dxLetters; // percent -1...0...1 of font height

} RenderEngineRawFont;


class RenderEngineRaw: public RenderEngine
{
   public:
     RenderEngineRaw();
     virtual ~RenderEngineRaw();

     virtual void setColors(double* color);
     virtual void setColors(double* color, float fAlfaScale);
     virtual void setFill(float r, float g, float b, float a);
     virtual void setStroke(double* color);
     virtual void setStroke(double* color, float fStrokeSize); 
     virtual void setStroke(float r, float g, float b, float a);
     virtual float getStrokeSize();
     virtual void setStrokeSize(float fStrokeSize);
     virtual void setFontColor(u32 fontId, double* color);
     virtual void setFontBackgroundBoundingBoxFillColor(double* color);

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
     virtual void drawText(float xPos, float yPos, u32 fontId, const char* szText);
     virtual void drawTextNoOutline(float xPos, float yPos, u32 fontId, const char* szText);
     virtual void drawTextLeft(float xPos, float yPos, u32 fontId, const char* szText);
     virtual void drawTextScaled(float xPos, float yPos, u32 fontId, float fScale, const char* szText);
     virtual void drawTextLeftScaled(float xPos, float yPos, u32 fontId, float fScale, const char* szText);
     virtual float getMessageHeight(const char* text, float line_spacing_percent, float max_width, u32 fontId);
     virtual float drawMessageLines(float xPos, float yPos, const char* text, float line_spacing_percent, float max_width, u32 fontId);

     virtual void drawLine(float x1, float y1, float x2, float y2);
     virtual void drawRect(float xPos, float yPos, float fWidth, float fHeight);
     virtual void drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius);
     virtual void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3);
     virtual void drawPolyLine(float* x, float* y, int count);
     virtual void fillPolygon(float* x, float* y, int count);

     virtual void fillCircle(float x, float y, float r);
     virtual void drawCircle(float x, float y, float r);
     virtual void drawArc(float x, float y, float r, float a1, float a2);

   protected:
      int _getFontIndexFromId(u32 fontId);
      RenderEngineRawFont* _getFontFromId(u32 fontId);
      float _get_space_width(RenderEngineRawFont* pFont);
      float _get_char_width(RenderEngineRawFont* pFont, int ch);
      void _buildMipImage(struct _fbg_img* pSrc, struct _fbg_img* pDest);

      void _drawSimpleText(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos);
      void _drawSimpleTextScaled(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos, float fScale);

      struct _fbg* m_pFBG;

      RenderEngineRawFont* m_pFonts[MAX_RAW_FONTS];
      u32 m_FontIds[MAX_RAW_FONTS];
      u32 m_CurrentFontId;
      int m_iCountFonts;

      struct _fbg_img* m_pImages[MAX_RAW_IMAGES];
      u32 m_ImageIds[MAX_RAW_IMAGES];
      u32 m_CurrentImageId;
      int m_iCountImages;

      struct _fbg_img* m_pIcons[MAX_RAW_ICONS];
      struct _fbg_img* m_pIconsMip[MAX_RAW_ICONS][2];
      u32 m_IconIds[MAX_RAW_ICONS];
      u32 m_CurrentIconId;
      int m_iCountIcons;

      u8 m_ColorFill[4];
      u8 m_ColorStroke[4];
      u8 m_ColorTextBoundingBoxBgFill[4];
      float m_fStrokeSize;

      bool m_bDisableTextOutline;
};
