#pragma once

#include "render_engine.h"

class RenderEngineRaw: public RenderEngine
{
   public:
     RenderEngineRaw();
     virtual ~RenderEngineRaw();

     virtual void setFontOutlineColor(u32 idFont, u8 r, u8 g, u8 b, u8 a);
     virtual u32 loadImage(const char* szFile);
     virtual void freeImage(u32 idImage);
     virtual u32 loadIcon(const char* szFile);
     virtual void freeIcon(u32 idIcon);
     virtual int getImageWidth(u32 uImageId);
     virtual int getImageHeight(u32 uImageId);
     virtual void changeImageHue(u32 uImageId, u8 r, u8 g, u8 b);

     virtual void startFrame();
     virtual void endFrame();
     virtual void rotate180();

     virtual void drawImage(float xPos, float yPos, float fWidth, float fHeight, u32 imageId);
     virtual void bltImage(float xPosDest, float yPosDest, float fWidthDest, float fHeightDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uImageId);
     virtual void bltSprite(float xPosDest, float yPosDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uImageId);
     virtual void drawIcon(float xPos, float yPos, float fWidth, float fHeight, u32 iconId);
     virtual void bltIcon(float xPosDest, float yPosDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 iconId);

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

   protected:
      virtual void* _loadRawFontImageObject(const char* szFileName);
      virtual void _freeRawFontImageObject(void* pImageObject);
      void _buildMipImage(struct _fbg_img* pSrc, struct _fbg_img* pDest);

      void _drawSimpleText(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos);
      void _drawSimpleTextScaled(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos, float fScale);

      struct _fbg* m_pFBG;

      struct _fbg_img* m_pImages[MAX_RAW_IMAGES];
      u32 m_ImageIds[MAX_RAW_IMAGES];
      u32 m_CurrentImageId;
      int m_iCountImages;

      struct _fbg_img* m_pIcons[MAX_RAW_ICONS];
      struct _fbg_img* m_pIconsMip[MAX_RAW_ICONS][2];
      u32 m_IconIds[MAX_RAW_ICONS];
      u32 m_CurrentIconId;
      int m_iCountIcons;
};
