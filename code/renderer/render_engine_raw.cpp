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

#include "render_engine_raw.h"
#include "fbg_dispmanx.h"
#include <math.h>

RenderEngineRaw::RenderEngineRaw()
:RenderEngine()
{
   log_line("RendererRAW: Init started.");

   m_pFBG = fbg_dispmanxSetup(0, VC_IMAGE_RGBA32);

   m_iRenderWidth = m_pFBG->width;
   m_iRenderHeight = m_pFBG->height;
   log_line("Initialized graphics to resolution: %d x %d", m_iRenderWidth, m_iRenderHeight);
   m_fPixelWidth = 1.0/(float)m_iRenderWidth;
   m_fPixelHeight = 1.0/(float)m_iRenderHeight;

   m_iCountFonts = 0;
   m_iCountImages = 0;
   m_iCountIcons = 0;
   m_CurrentFontId = 1;
   m_CurrentImageId = 1;
   m_CurrentIconId = 1;

   m_ColorFill[0] = m_ColorFill[1] = m_ColorFill[2] = m_ColorFill[3] = 0;
   m_ColorStroke[0] = m_ColorStroke[1] = m_ColorStroke[2] = m_ColorStroke[3] = 0;
   m_ColorTextBoundingBoxBgFill[0] = m_ColorTextBoundingBoxBgFill[1] = m_ColorTextBoundingBoxBgFill[2] = m_ColorTextBoundingBoxBgFill[3] = 0;
   m_fStrokeSize = 0.0;

   log_line("RendererRAW: Render init done.");
}


RenderEngineRaw::~RenderEngineRaw()
{
   log_line("Free graphics engine resources.");
   if ( NULL != m_pFBG )
   {
      log_line("Free graphics engine instance.");
      fbg_close(m_pFBG);
   }
}


void RenderEngineRaw::setColors(double* color)
{
   setColors(color, 1.0);
}

void RenderEngineRaw::setColors(double* color, float fAlfaScale)
{
   m_ColorFill[0] = color[0];
   m_ColorFill[1] = color[1];
   m_ColorFill[2] = color[2];

   m_ColorStroke[0] = color[0];
   m_ColorStroke[1] = color[1];
   m_ColorStroke[2] = color[2];

   float fAlpha = color[3]*m_fGlobalAlfa*fAlfaScale;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;
   m_ColorFill[3] = fAlpha * 255;
   m_ColorStroke[3] = fAlpha * 255;
}

void RenderEngineRaw::setFill(float r, float g, float b, float a)
{
   m_ColorFill[0] = r;
   m_ColorFill[1] = g;
   m_ColorFill[2] = b;
   float fAlpha = a*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;
   m_ColorFill[3] = fAlpha * 255;
}

void RenderEngineRaw::setStroke(double* color)
{
   setStroke(color, 1.0);
}

void RenderEngineRaw::setStroke(double* color, float fStrokeSize)
{
   m_ColorStroke[0] = color[0];
   m_ColorStroke[1] = color[1];
   m_ColorStroke[2] = color[2];

   float fAlpha = color[3]*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;

   m_ColorStroke[3] = fAlpha * 255;
   m_fStrokeSize = fStrokeSize;

   // Is it in pixel size? convert to pixels;
   if ( fStrokeSize < 0.8 )
      m_fStrokeSize = fStrokeSize / m_fPixelWidth;
}

void RenderEngineRaw::setStroke(float r, float g, float b, float a)
{
   m_ColorStroke[0] = r;
   m_ColorStroke[1] = g;
   m_ColorStroke[2] = b;

   float fAlpha = a*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;

   m_ColorStroke[3] = fAlpha * 255;
}

float RenderEngineRaw::getStrokeSize()
{
   return m_fStrokeSize;
}

void RenderEngineRaw::setStrokeSize(float fStrokeSize)
{
   m_fStrokeSize = fStrokeSize;

   // Is it not in pixel size? convert to pixels;
   if ( fStrokeSize < 0.8 )
      m_fStrokeSize = fStrokeSize / m_fPixelWidth;
}

void RenderEngineRaw::setFontColor(u32 fontId, double* color)
{
}

void RenderEngineRaw::setFontBackgroundBoundingBoxFillColor(double* color)
{
   m_ColorTextBoundingBoxBgFill[0] = color[0];
   m_ColorTextBoundingBoxBgFill[1] = color[1];
   m_ColorTextBoundingBoxBgFill[2] = color[2];

   float fAlpha = color[3]*m_fGlobalAlfa;
   if ( fAlpha > 1.0 )
      fAlpha = 1.0;
   if ( fAlpha < 0.0 )
      fAlpha = 0.0;

   m_ColorTextBoundingBoxBgFill[3] = fAlpha * 255;
}


int RenderEngineRaw::_getFontIndexFromId(u32 fontId)
{
   for( int i=0; i<m_iCountFonts; i++ )
      if ( m_FontIds[i] == fontId )
         return i;
   return -1;
}

RenderEngineRawFont* RenderEngineRaw::_getFontFromId(u32 fontId)
{
   int index = _getFontIndexFromId(fontId);
   if ( index < 0 )
      return NULL;
   return m_pFonts[index];
}

u32 RenderEngineRaw::loadFont(const char* szFontFile)
{
   if ( m_iCountFonts >= MAX_RAW_FONTS )
      return 0;

   if ( access( szFontFile, R_OK ) == -1 )
      return 0;

   char szFile[256];
   FILE* fd = fopen(szFontFile, "r");
   if ( NULL == fd )
      return 0;

   m_pFonts[m_iCountFonts] = (RenderEngineRawFont*) malloc(sizeof(RenderEngineRawFont));

   int len = snprintf(szFile, sizeof(szFile), "%s", szFontFile);
   szFile[len-3] = 'p';
   szFile[len-2] = 'n';
   szFile[len-1] = 'g';

   log_line("Loading font: %s", szFile);
   m_pFonts[m_iCountFonts]->pImage = fbg_loadPNG(m_pFBG, szFile);
   if ( NULL == m_pFonts[m_iCountFonts]->pImage )
   {
      log_softerror_and_alarm("Failed to load font: %s", szFile);
      fclose(fd);
      return 0;
   }
   if ( 2 != fscanf(fd, "%d %d", &(m_pFonts[m_iCountFonts]->lineHeight), &(m_pFonts[m_iCountFonts]->baseLine) ) )
      return 0;
   if ( 1 != fscanf(fd, "%d", &(m_pFonts[m_iCountFonts]->charCount) ) )
      return 0;
   if ( m_pFonts[m_iCountFonts]->charCount < 0 || m_pFonts[m_iCountFonts]->charCount >= MAX_FONT_CHARS )
      return 0;
   log_line("Loaded font: size: %d/%d, count: %d", m_pFonts[m_iCountFonts]->lineHeight, m_pFonts[m_iCountFonts]->baseLine, m_pFonts[m_iCountFonts]->charCount);
   for( int i=0; i<m_pFonts[m_iCountFonts]->charCount; i++ )
   {
      if ( 5 != fscanf(fd, "%d %d %d %d %d", &(m_pFonts[m_iCountFonts]->chars[i].charId),
                              &(m_pFonts[m_iCountFonts]->chars[i].imgXOffset),
                              &(m_pFonts[m_iCountFonts]->chars[i].imgYOffset),
                              &(m_pFonts[m_iCountFonts]->chars[i].width),
                              &(m_pFonts[m_iCountFonts]->chars[i].height) ) )
      {
         log_softerror_and_alarm("Failed to load font, invalid c-description.");
         return 0;
      }
      if ( 3 != fscanf(fd, "%d %d %d", &(m_pFonts[m_iCountFonts]->chars[i].xOffset),
                              &(m_pFonts[m_iCountFonts]->chars[i].yOffset),
                              &(m_pFonts[m_iCountFonts]->chars[i].xAdvance) ) )
      {
         log_softerror_and_alarm("Failed to load font, invalid cc-description.");
         return 0;
      }
      int page, ch;
      if ( 2 != fscanf(fd, "%d %d", &page, &ch ) )
      {
         log_softerror_and_alarm("Failed to load font, invalid p-description.");
         return 0;
      }
     //printf("loaded char: %d %d %d %d %d\n", m_pFonts[m_iCountFonts]->chars[i].charId, 
     //         m_pFonts[m_iCountFonts]->chars[i].imgXOffset, m_pFonts[m_iCountFonts]->chars[i].imgYOffset, m_pFonts[m_iCountFonts]->chars[i].width, m_pFonts[m_iCountFonts]->chars[i].height );
   }

   /*
   if ( 1 != fscanf(fd, "%d", &(m_pFonts[m_iCountFonts]->keringsCount) ) )
   {
      log_softerror_and_alarm("Failed to load font, invalid k-description.");
      return 0;
   }
   if ( m_pFonts[m_iCountFonts]->keringsCount < 0 || m_pFonts[m_iCountFonts]->keringsCount >= MAX_FONT_KERINGS )
   {
      log_softerror_and_alarm("Failed to load font, invalid k-description.");
      return 0;
   }
   for( int i=0; i<m_pFonts[m_iCountFonts]->keringsCount; i++ )
   {
      if ( 3 != fscanf(fd, "%d %d %d", &(m_pFonts[m_iCountFonts]->kerings[i][0]),
                              &(m_pFonts[m_iCountFonts]->kerings[i][1]),
                              &(m_pFonts[m_iCountFonts]->kerings[i][2]) ) )
      {
         log_softerror_and_alarm("Failed to load font, invalid k-description.");
         return 0;
      }
   }
   */
   fclose(fd);

   m_pFonts[m_iCountFonts]->charIdFirst = m_pFonts[m_iCountFonts]->chars[0].charId;
   m_pFonts[m_iCountFonts]->charIdLast = m_pFonts[m_iCountFonts]->chars[m_pFonts[m_iCountFonts]->charCount-1].charId;
   m_pFonts[m_iCountFonts]->dxLetters = 0.0;
   log_line("Loaded font %s (%d of max %d): chars: %d to %d, count %d, kering %d, id: %u",
       szFile, m_iCountFonts, MAX_RAW_FONTS,
       m_pFonts[m_iCountFonts]->charIdFirst, m_pFonts[m_iCountFonts]->charIdLast, m_pFonts[m_iCountFonts]->charCount, m_pFonts[m_iCountFonts]->keringsCount, m_CurrentFontId+1);

   m_CurrentFontId++;
   m_FontIds[m_iCountFonts] = m_CurrentFontId;
   m_iCountFonts++;
   return m_CurrentFontId;
}

void RenderEngineRaw::freeFont(u32 idFont)
{
   int indexFont = _getFontIndexFromId(idFont);
   if ( -1 == indexFont )
      return;

   fbg_freeImage(m_pFonts[indexFont]->pImage);
   free(m_pFonts[indexFont]);

   for( int i=indexFont; i<m_iCountFonts-1; i++ )
   {
      m_pFonts[i] = m_pFonts[i+1];
      m_FontIds[i] = m_FontIds[i+1];
   }
   m_iCountFonts--;
}


u32 RenderEngineRaw::loadImage(const char* szFile)
{
   if ( m_iCountImages > MAX_RAW_IMAGES )
      return 0;

   if ( access( szFile, R_OK ) == -1 )
      return 0;

   if ( NULL != strstr(szFile, ".png") )
   {
      m_pImages[m_iCountImages] = fbg_loadPNG(m_pFBG, szFile);
   }
   else
   {
      m_pImages[m_iCountImages] = fbg_loadJPEG(m_pFBG, szFile);
   }
   if ( NULL != m_pImages[m_iCountImages] )
      log_line("Loaded image %s, id: %u", szFile, m_CurrentImageId+1);
   else
      log_softerror_and_alarm("Failed to load image %s", szFile);

   m_CurrentImageId++;
   m_ImageIds[m_iCountImages] = m_CurrentImageId;
   m_iCountImages++;
   return m_CurrentImageId;
}

void RenderEngineRaw::freeImage(u32 idImage)
{
   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
      if ( m_ImageIds[i] == idImage )
      {
         indexImage = i;
         break;
      }
   if ( -1 == indexImage )
      return;

   fbg_freeImage(m_pImages[indexImage]);

   for( int i=indexImage; i<m_iCountImages-1; i++ )
   {
      m_pImages[i] = m_pImages[i+1];
      m_ImageIds[i] = m_ImageIds[i+1];
   }
   m_iCountImages--;
}


u32 RenderEngineRaw::loadIcon(const char* szFile)
{
   if ( m_iCountIcons > MAX_RAW_ICONS )
      return 0;

   if ( access( szFile, R_OK ) == -1 )
      return 0;

   if ( NULL != strstr(szFile, ".png") )
   {
      m_pIcons[m_iCountIcons] = fbg_loadPNG(m_pFBG, szFile);
      m_pIconsMip[m_iCountIcons][0] = fbg_loadPNG(m_pFBG, szFile);
      m_pIconsMip[m_iCountIcons][1] = fbg_loadPNG(m_pFBG, szFile);
   }
   else
   {
      m_pIcons[m_iCountIcons] = fbg_loadJPEG(m_pFBG, szFile);
      m_pIconsMip[m_iCountIcons][0] = fbg_loadJPEG(m_pFBG, szFile);
      m_pIconsMip[m_iCountIcons][1] = fbg_loadJPEG(m_pFBG, szFile);
   }
   if ( NULL != m_pIcons[m_iCountIcons] )
      log_line("Loaded icon %s, id: %u", szFile, m_CurrentIconId+1);
   else
      log_softerror_and_alarm("Failed to load icon %s", szFile);

   _buildMipImage(m_pIcons[m_iCountIcons], m_pIconsMip[m_iCountIcons][0]);
   _buildMipImage(m_pIconsMip[m_iCountIcons][0], m_pIconsMip[m_iCountIcons][1]);

   m_CurrentIconId++;
   m_IconIds[m_iCountIcons] = m_CurrentIconId;
   m_iCountIcons++;
   return m_CurrentIconId;
}

void RenderEngineRaw::freeIcon(u32 idIcon)
{
   int indexIcon = -1;
   for( int i=0; i<m_iCountIcons; i++ )
      if ( m_IconIds[i] == idIcon )
      {
         indexIcon = i;
         break;
      }
   if ( -1 == indexIcon )
      return;

   fbg_freeImage(m_pIcons[indexIcon]);
   fbg_freeImage(m_pIconsMip[indexIcon][0]);
   fbg_freeImage(m_pIconsMip[indexIcon][1]);

   for( int i=indexIcon; i<m_iCountIcons-1; i++ )
   {
      m_pIcons[i] = m_pIcons[i+1];
      m_pIconsMip[i][0] = m_pIconsMip[i+1][0];
      m_pIconsMip[i][1] = m_pIconsMip[i+1][1];
      m_IconIds[i] = m_IconIds[i+1];
   }
   m_iCountIcons--;
}

void RenderEngineRaw::_buildMipImage(struct _fbg_img* pSrc, struct _fbg_img* pDest)
{
   pDest->width = pSrc->width/2;
   pDest->height = pSrc->height/2;
   u32 r[4];
   u32 g[4];
   u32 b[4];
   u32 a[4];
   u32 res;
   for( int y=0; y<pDest->height; y++ )
   for( int x=0; x<pDest->width; x++ )
   {
      unsigned char* pixelDest = pDest->data + x*4 + y * pDest->width * 4;
      unsigned char* pixelSrc = pSrc->data + x*2*4 + y*2 * pSrc->width * 4;
      r[0] = *pixelSrc;
      pixelSrc++;
      g[0] = *pixelSrc;
      pixelSrc++;
      b[0] = *pixelSrc;
      pixelSrc++;
      a[0] = *pixelSrc;
      pixelSrc++;

      r[1] = *pixelSrc;
      pixelSrc++;
      g[1] = *pixelSrc;
      pixelSrc++;
      b[1] = *pixelSrc;
      pixelSrc++;
      a[1] = *pixelSrc;
      pixelSrc++;

      pixelSrc = pSrc->data + x*2*4 + (y*2+1) * pSrc->width * 4;
      r[2] = *pixelSrc;
      pixelSrc++;
      g[2] = *pixelSrc;
      pixelSrc++;
      b[2] = *pixelSrc;
      pixelSrc++;
      a[2] = *pixelSrc;
      pixelSrc++;

      r[3] = *pixelSrc;
      pixelSrc++;
      g[3] = *pixelSrc;
      pixelSrc++;
      b[3] = *pixelSrc;
      pixelSrc++;
      a[3] = *pixelSrc;
      pixelSrc++;
    
      res = sqrtl((r[0]*r[0] + r[1]*r[1] + r[2]*r[2] + r[3]*r[3])/4);
      if ( res < 0 ) res = 0;
      if ( res > 255 ) res = 255;
      *pixelDest = res;
      pixelDest++;

      res = sqrtl((g[0]*g[0] + g[1]*g[1] + g[2]*g[2] + g[3]*g[3])/4);
      if ( res < 0 ) res = 0;
      if ( res > 255 ) res = 255;
      *pixelDest = res;
      pixelDest++;

      res = sqrtl((b[0]*b[0] + b[1]*b[1] + b[2]*b[2] + b[3]*b[3])/4);
      if ( res < 0 ) res = 0;
      if ( res > 255 ) res = 255;
      *pixelDest = res;
      pixelDest++;

      res = sqrtl((a[0]*a[0] + a[1]*a[1] + a[2]*a[2] + a[3]*a[3])/4);
      if ( res < 0 ) res = 0;
      if ( res > 255 ) res = 255;
      *pixelDest = res;
      pixelDest++;
   }

}

void RenderEngineRaw::startFrame()
{
   fbg_clear(m_pFBG, 0);
}

void RenderEngineRaw::endFrame()
{
   fbg_draw(m_pFBG);
   fbg_flip(m_pFBG);
}

void RenderEngineRaw::rotate180()
{
   unsigned char pixel[4];

   for( int y=0; y<m_pFBG->height/2; y++ )
   for( int x=0; x<m_pFBG->width; x++ )
   {
       unsigned char *scr_pointer1 = (unsigned char *)(m_pFBG->back_buffer + (y * m_pFBG->line_length + x * m_pFBG->components));
       unsigned char *scr_pointer2 = (unsigned char *)(m_pFBG->back_buffer + ((m_pFBG->height-y-1) * m_pFBG->line_length + (m_pFBG->width-x-1) * m_pFBG->components));
       memcpy(pixel, scr_pointer1, 4);
       memcpy(scr_pointer1, scr_pointer2, 4);
       memcpy(scr_pointer2, pixel, 4);
   }
}

void RenderEngineRaw::drawImage(float xPos, float yPos, float fWidth, float fHeight, u32 imageId)
{
   if ( imageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
      if ( m_ImageIds[i] == imageId )
      {
         indexImage = i;
         break;
      }
   if ( -1 == indexImage )
      return;
   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   if ( x < 0 || y < 0 || x+w > m_iRenderWidth || y+h > m_iRenderHeight )
      return;

   m_pFBG->mix_color.r = 255;
   m_pFBG->mix_color.g = 255;
   m_pFBG->mix_color.b = 255;
   m_pFBG->mix_color.a = 255;

   fbg_imageDraw(m_pFBG, m_pImages[indexImage], x,y,w,h, 0, 0, m_pImages[indexImage]->width, m_pImages[indexImage]->height);
}

void RenderEngineRaw::drawIcon(float xPos, float yPos, float fWidth, float fHeight, u32 iconId)
{
   if ( iconId < 1 )
      return;

   int indexIcon = -1;
   for( int i=0; i<m_iCountIcons; i++ )
      if ( m_IconIds[i] == iconId )
      {
         indexIcon = i;
         break;
      }
   if ( -1 == indexIcon )
      return;

   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   if ( x < 0 || y < 0 || x+w >= m_iRenderWidth || y+h >= m_iRenderHeight )
      return;

   m_pFBG->mix_color.r = m_ColorFill[0];
   m_pFBG->mix_color.g = m_ColorFill[1];
   m_pFBG->mix_color.b = m_ColorFill[2];
   m_pFBG->mix_color.a = m_ColorFill[3];

   if ( fWidth*m_iRenderWidth <= m_pIcons[indexIcon]->width/4 ||
        fHeight*m_iRenderHeight <= m_pIcons[indexIcon]->height/4 ) 
      fbg_imageDrawAlpha(m_pFBG, m_pIconsMip[indexIcon][1], x,y, fWidth*m_iRenderWidth, fHeight*m_iRenderHeight, 0,0, m_pIconsMip[indexIcon][1]->width, m_pIconsMip[indexIcon][1]->height);
   else if ( fWidth*m_iRenderWidth <= m_pIcons[indexIcon]->width/2 ||
        fHeight*m_iRenderHeight <= m_pIcons[indexIcon]->height/2 ) 
      fbg_imageDrawAlpha(m_pFBG, m_pIconsMip[indexIcon][0], x,y, fWidth*m_iRenderWidth, fHeight*m_iRenderHeight, 0,0, m_pIconsMip[indexIcon][0]->width, m_pIconsMip[indexIcon][0]->height);
   else
      fbg_imageDrawAlpha(m_pFBG, m_pIcons[indexIcon], x,y, fWidth*m_iRenderWidth, fHeight*m_iRenderHeight, 0,0, m_pIcons[indexIcon]->width, m_pIcons[indexIcon]->height);
}

float RenderEngineRaw::getFontHeight(u32 fontId)
{
   RenderEngineRawFont* pFont = _getFontFromId(fontId);
   if ( NULL == pFont )
      return 0.05;
   return ((float)pFont->lineHeight)/(float)getScreenHeight();
}

float RenderEngineRaw::textHeight(u32 fontId)
{
   RenderEngineRawFont* pFont = _getFontFromId(fontId);
   if ( NULL == pFont )
      return 0.05;

   return pFont->lineHeight*m_fPixelHeight;
}

float RenderEngineRaw::textHeight(float fTextHeight, u32 fontId)
{
   RenderEngineRawFont* pFont = _getFontFromId(fontId);
   if ( NULL == pFont )
      return 0.05;

   return pFont->lineHeight*m_fPixelHeight;
}

float RenderEngineRaw::textWidth(float fTextHeight, u32 fontId, const char* szText)
{
   if ( NULL == szText )
      return 0.0;

   fTextHeight = textHeight(fTextHeight, fontId);

   return textWidthScaled(fTextHeight, fontId, 1.0, szText);
}

float RenderEngineRaw::textHeightScaled(float fTextHeight, u32 fontId, float fScale)
{
   RenderEngineRawFont* pFont = _getFontFromId(fontId);
   if ( NULL == pFont )
      return 0.0;

   return pFont->lineHeight*m_fPixelHeight*fScale;
}

float RenderEngineRaw::textWidthScaled(float fTextHeight, u32 fontId, float fScale, const char* szText)
{
   if ( NULL == szText )
      return 0.0;

   RenderEngineRawFont* pFont = _getFontFromId(fontId);
   if ( NULL == pFont )
      return 0.0;

   float fWidth = 0.0;
   char* p = (char*)szText;

   while ( (*p) != 0 )
   {
      if ( (*p) >= pFont->charIdFirst && (*p) <= pFont->charIdLast )
      {
         fWidth += pFont->chars[(*p)-pFont->charIdFirst].xAdvance * m_fPixelWidth * fScale;
         fWidth += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;
      }
      p++;
   }

   return fWidth;
}

void RenderEngineRaw::drawText(float xPos, float yPos, u32 fontId, const char* szText)
{
   drawTextScaled(xPos, yPos, 0.0, fontId, 1.0, szText);
}

void RenderEngineRaw::drawText(float xPos, float yPos, float fTextHeight, u32 fontId, const char* szText)
{
   drawTextScaled(xPos, yPos, fTextHeight, fontId, 1.0, szText);
}

void RenderEngineRaw::drawTextNoOutline(float xPos, float yPos, float fTextHeight, u32 fontId, const char* szText)
{
   bool bTmp = m_bDisableTextOutline;
   m_bDisableTextOutline = true;
   drawTextScaled(xPos, yPos, fTextHeight, fontId, 1.0, szText);
   m_bDisableTextOutline = bTmp;
}

void RenderEngineRaw::drawTextScaled(float xPos, float yPos, float fTextHeight, u32 fontId, float fScale, const char* szText)
{
   if ( NULL == szText )
      return;

   RenderEngineRawFont* pFont = _getFontFromId(fontId);
   if ( NULL == pFont )
      return;

   m_pFBG->mix_color.r = m_ColorFill[0];
   m_pFBG->mix_color.g = m_ColorFill[1];
   m_pFBG->mix_color.b = m_ColorFill[2];
   m_pFBG->mix_color.a = m_ColorFill[3];

   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;

   if ( y < 0 )
      return;

   //y += fScale*(pFont->lineHeight-pFont->baseLine )/3;

   if ( y + fScale*pFont->lineHeight >= m_iRenderHeight )
      return;

   if ( fabs(fScale-1.0) > m_fPixelWidth )
       _drawSimpleTextScaled(pFont, szText, x, y, fScale);
   else
       _drawSimpleText(pFont, szText, x, y);
}

void RenderEngineRaw::drawTextLeft(float xPos, float yPos, u32 fontId, const char* szText)
{
   drawTextLeftScaled(xPos, yPos, 0.0, fontId, 1.0, szText);
}

void RenderEngineRaw::drawTextLeft(float xPos, float yPos, float fTextHeight, u32 fontId, const char* szText)
{
   drawTextLeftScaled(xPos, yPos, fTextHeight, fontId, 1.0, szText);
}

void RenderEngineRaw::drawTextLeftScaled(float xPos, float yPos, float fTextHeight, u32 fontId, float fScale, const char* szText)

{
   if ( NULL == szText )
      return;

   RenderEngineRawFont* pFont = _getFontFromId(fontId);
   if ( NULL == pFont )
      return;

   m_pFBG->mix_color.r = m_ColorFill[0];
   m_pFBG->mix_color.g = m_ColorFill[1];
   m_pFBG->mix_color.b = m_ColorFill[2];
   m_pFBG->mix_color.a = m_ColorFill[3];

   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;

   if ( y < 0 )
      return;

   //y += fScale*(pFont->lineHeight-pFont->baseLine )/3;

   if ( y + fScale*pFont->lineHeight >= m_iRenderHeight )
      return;

   char* p = (char*)szText;
   int wText = 0;

   while ( (*p) != 0 )
   {
      if ( (*p) >= pFont->charIdFirst && (*p) <= pFont->charIdLast )
      {
         wText += pFont->chars[(*p)-pFont->charIdFirst].xAdvance*fScale;
         wText += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;
      }
      p++;
   }

   if ( fabs(fScale-1.0) > m_fPixelWidth )
      _drawSimpleTextScaled(pFont, szText, x-wText, y, fScale);
   else
      _drawSimpleText(pFont, szText, x-wText, y);
}

float RenderEngineRaw::getMessageHeight(const char* text, float line_spacing_percent, float max_width, u32 fontId)
{
   return getMessageHeight(text, textHeight(fontId), line_spacing_percent, max_width, fontId);
}

float RenderEngineRaw::getMessageHeight(const char* text, float fTextHeight, float line_spacing_percent, float max_width, u32 fontId)
{
   if ( NULL == text || 0 == text[0] )
      return 0.0;

   RenderEngineRawFont* pFont = _getFontFromId(fontId);
   if ( NULL == pFont )
      return 0.05;

   if ( 0 == strlen(text) )
      return 0.0;

   float fScale = 1.0;
   if ( ! m_bEnableFontScaling )
      fTextHeight = textHeight(fTextHeight, fontId);
   else
   {
      float ff = textHeight(fTextHeight, fontId);
      if ( ff > 0.0001 )
         fScale = fTextHeight / ff;
      else
         fScale = 1.0;
   }
   float height = 0.0;

   char szText[1024];
   strcpy(szText, text );
   const char* szTokens = " ";
   char* szContext = szText;
   char* szWord = NULL;
   char* szPrevWord = NULL;
   float wWord = 0;
   float line_width = 0;
   float space_width = pFont->lineHeight*0.25*fScale + pFont->lineHeight * pFont->dxLetters * m_fPixelWidth;
   int countWords = 0;
   int countLineWords = 0;

   while ( (szWord = strtok_r(szContext, szTokens, &szContext)) )
   {
      countWords++;
      countLineWords++;
      wWord = 0.0;
      while ( *szWord )
      {
         if ( (*szWord) >= pFont->charIdFirst && (*szWord) <= pFont->charIdLast )
         {
            wWord += pFont->chars[(*szWord)-pFont->charIdFirst].xAdvance*fScale;
            wWord += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;
         }
         szWord++;
      }

      if ( (line_width+wWord)/(float)m_iRenderWidth > max_width-0.0001 || ( NULL != szPrevWord && '\n' == szPrevWord[strlen(szPrevWord)-1]) )
      {
         if ( NULL != szPrevWord && '\n' == szPrevWord[strlen(szPrevWord)-1])
            szPrevWord = NULL;
         height += fTextHeight*(1.0+line_spacing_percent);
         line_width = wWord;
         countLineWords = 1;
      }
      else
      {
         line_width += wWord;
         if ( countLineWords > 1 )
            line_width += space_width;
         szPrevWord = szWord;
      }
   }
   if ( countWords > 0 )
      height += fTextHeight;

   return height;
}

float RenderEngineRaw::drawMessageLines(float xPos, float yPos, const char* text, float fTextHeight, float line_spacing_percent, float max_width, u32 fontId)
{
   if ( NULL == text || 0 == text[0] )
      return 0.0;

   RenderEngineRawFont* pFont = _getFontFromId(fontId);
   if ( NULL == pFont )
      return 0.05;

   if ( 0 == strlen(text) )
      return 0.0;

   float fScale = 1.0;
   if ( ! m_bEnableFontScaling )
      fTextHeight = textHeight(fTextHeight, fontId);
   else
   {
      float ff = textHeight(fTextHeight, fontId);
      if ( ff > 0.0001 )
         fScale = fTextHeight / ff;
      else
         fScale = 1.0;
   }

   m_pFBG->mix_color.r = m_ColorFill[0];
   m_pFBG->mix_color.g = m_ColorFill[1];
   m_pFBG->mix_color.b = m_ColorFill[2];
   m_pFBG->mix_color.a = m_ColorFill[3];
   if ( m_bHighlightFirstWord )
   {
      m_pFBG->mix_color.r = 200;
      m_pFBG->mix_color.g = 200;
      m_pFBG->mix_color.b = 0;
   }

   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;

   if ( y < 0 )
      return 0.0;

   //y += fScale*(pFont->lineHeight-pFont->baseLine )/3;

   if ( y + pFont->lineHeight*fScale >= m_iRenderHeight )
      return 0.0;

   float height = 0.0;
   char szText[1024];
   strcpy(szText, text );
   const char* szTokens = " ";
   char* szContext = szText;
   char* szWord = NULL;
   char* szPrevWord = NULL;
   float wWord = 0;
   float line_width = 0;
   float space_width = pFont->lineHeight*0.25*fScale + pFont->lineHeight * pFont->dxLetters * m_fPixelWidth;
   int countWords = 0;
   int countLineWords = 0;

   while ( (szWord = strtok_r(szContext, szTokens, &szContext)) )
   {
      countWords++;
      countLineWords++;
      wWord = 0.0;
      char* pTmpWord = szWord;
      while ( *szWord )
      {
         if ( (*szWord) >= pFont->charIdFirst && (*szWord) <= pFont->charIdLast )
         {
            wWord += pFont->chars[(*szWord)-pFont->charIdFirst].xAdvance*fScale;
            wWord += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;
         }
         szWord++;
      }

      if ( (line_width+wWord)/(float)m_iRenderWidth > max_width+0.0001 || ( NULL != szPrevWord && '\n' == szPrevWord[strlen(szPrevWord)-1]) )
      {
         height += fTextHeight*(1.0+line_spacing_percent);
         y += pFont->lineHeight*(1.0+line_spacing_percent);
         x = xPos*m_iRenderWidth;
         line_width = wWord;
         countLineWords = 1;
         if ( y + pFont->lineHeight*fScale >= m_iRenderHeight )
            return height;

         if ( m_bEnableFontScaling )
            _drawSimpleTextScaled(pFont, szText, x, y, 0.7);
         else
            _drawSimpleText(pFont, pTmpWord, x, y);
         x += wWord;

         if ( NULL != szPrevWord && '\n' == szPrevWord[strlen(szPrevWord)-1])
            szPrevWord = NULL;
      }
      else
      {
         if ( countLineWords > 1 )
         {
            line_width += space_width;
            x += space_width;
         }
         if ( m_bEnableFontScaling )
            _drawSimpleTextScaled(pFont, szText, x, y, 0.7);
         else
            _drawSimpleText(pFont, pTmpWord, x, y);
         line_width += wWord;
         x += wWord;
         szPrevWord = szWord;
      }
      if ( 1 == countWords && m_bHighlightFirstWord )
      {
         m_pFBG->mix_color.r = m_ColorFill[0];
         m_pFBG->mix_color.g = m_ColorFill[1];
         m_pFBG->mix_color.b = m_ColorFill[2];
      }
   }
   if ( countWords > 0 )
      height += fTextHeight;

   return height;
}

void RenderEngineRaw::_drawSimpleText(RenderEngineRawFont* pFont, const char* szText, int x, int y)
{
   if ( NULL == pFont || NULL == szText || 0 == szText[0] )
      return;

   if ( y < 0 )
      return;
   if ( y + pFont->lineHeight >= m_iRenderHeight )
      return;

   int xBoundingStart = 5000;
   int yBoundingStart = 5000;
   int xBoundingEnd = 0;
   int yBoundingEnd = 0;

   int tmp = m_pFBG->disableFontOutline;
   m_pFBG->disableFontOutline = m_bDisableTextOutline?1:0;

   if ( m_bDrawBackgroundBoundingBoxes )
   {
      const char* pText = szText;
      int xOrg = x;
      while ( *pText )
      {
         if ( *pText < pFont->charIdFirst || *pText > pFont->charIdLast )
         {
            pText++;
            continue;
         }
         if ( x < 0 )
         {
            x += pFont->chars[(*pText)-pFont->charIdFirst].xAdvance;
            x += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;

            pText++;
            continue;
         }
         if ( x + pFont->chars[(*pText)-pFont->charIdFirst].xAdvance + pFont->dxLetters * pFont->lineHeight*m_fPixelWidth >= m_iRenderWidth )
            break;

         int wImg = pFont->chars[(*pText)-pFont->charIdFirst].width;
         int hImg = pFont->chars[(*pText)-pFont->charIdFirst].height;

         if ( x < xBoundingStart )
            xBoundingStart = x;
         if ( x + wImg > xBoundingEnd )
            xBoundingEnd = x + wImg;
         if ( y < yBoundingStart )
            yBoundingStart = y;
         if ( y + hImg > yBoundingEnd )
            yBoundingEnd = y + hImg;
      
         x += pFont->chars[(*pText)-pFont->charIdFirst].xAdvance;
         x += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;
         pText++;
      }
      x = xOrg;
      if ( (xBoundingStart < xBoundingEnd) && (yBoundingStart < yBoundingEnd) )
      {
         xBoundingStart -= 0.7*pFont->chars[' '-pFont->charIdFirst].xAdvance;
         xBoundingEnd += 0.7*pFont->chars[' '-pFont->charIdFirst].xAdvance;
         yBoundingStart += 3;
         yBoundingEnd -= 2;
         yBoundingStart += pFont->lineHeight*0.03;
         yBoundingEnd -= pFont->lineHeight*0.01;

         u8 tmp_ColorFill[4];
         u8 tmp_ColorStroke[4];
         float tmp_fStrokeSize = m_fStrokeSize;

         memcpy(tmp_ColorFill, m_ColorFill, 4*sizeof(u8));
         memcpy(tmp_ColorStroke, m_ColorStroke, 4*sizeof(u8));
         memcpy(m_ColorFill, m_ColorTextBoundingBoxBgFill, 4*sizeof(u8));
         m_ColorStroke[0] = 0; m_ColorStroke[1] = 0; m_ColorStroke[2] = 0; m_ColorStroke[3] = 0;
         m_fStrokeSize = 0.0;
         if ( m_bDrawStrikeOnTextBackgroundBoundingBoxes )
         {
            m_ColorStroke[0] = m_ColorTextBackgroundBoundingBoxStrike[0];
            m_ColorStroke[1] = m_ColorTextBackgroundBoundingBoxStrike[1];
            m_ColorStroke[2] = m_ColorTextBackgroundBoundingBoxStrike[2];
            m_ColorStroke[3] = m_ColorTextBackgroundBoundingBoxStrike[3]*255;
            m_fStrokeSize = 1.0;
         }
         drawRoundRect(xBoundingStart/(float)m_iRenderWidth - m_fBoundingBoxPadding/getAspectRatio(), yBoundingStart/(float)m_iRenderHeight - m_fBoundingBoxPadding, (xBoundingEnd - xBoundingStart)/(float)m_iRenderWidth + 2.0*m_fBoundingBoxPadding/getAspectRatio(), (yBoundingEnd - yBoundingStart)/(float)m_iRenderHeight + 2.0*m_fBoundingBoxPadding, 5.0 );

         memcpy(m_ColorFill, tmp_ColorFill, 4*sizeof(u8));
         memcpy(m_ColorStroke, tmp_ColorStroke, 4*sizeof(u8));
         m_fStrokeSize = tmp_fStrokeSize;
      }
   }

   while ( *szText )
   {
      if ( *szText < pFont->charIdFirst || *szText > pFont->charIdLast )
      {
         szText++;
         continue;
      }
      if ( x < 0 )
      {
         x += pFont->chars[(*szText)-pFont->charIdFirst].xAdvance;
         x += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;

         szText++;
         continue;
      }
      if ( x + pFont->chars[(*szText)-pFont->charIdFirst].xAdvance + pFont->dxLetters * pFont->lineHeight*m_fPixelWidth >= m_iRenderWidth )
         break;

      int xImg = pFont->chars[(*szText)-pFont->charIdFirst].imgXOffset;
      int yImg = pFont->chars[(*szText)-pFont->charIdFirst].imgYOffset;
      int wImg = pFont->chars[(*szText)-pFont->charIdFirst].width;
      int hImg = pFont->chars[(*szText)-pFont->charIdFirst].height;
      unsigned char *img_pointer = (unsigned char *)(pFont->pImage->data + (yImg * pFont->pImage->width * m_pFBG->components + xImg * m_pFBG->components));

      fbg_imageClipAColor(m_pFBG, pFont->pImage, x, y, xImg, yImg, wImg, hImg);

      if ( x < xBoundingStart )
         xBoundingStart = x;
      if ( x + wImg > xBoundingEnd )
         xBoundingEnd = x + wImg;
      if ( y < yBoundingStart )
         yBoundingStart = y;
      if ( y + hImg > yBoundingEnd )
         yBoundingEnd = y + hImg;
      
      x += pFont->chars[(*szText)-pFont->charIdFirst].xAdvance;
      x += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;
      szText++;
   }

   m_pFBG->disableFontOutline = tmp;
}

void RenderEngineRaw::_drawSimpleTextScaled(RenderEngineRawFont* pFont, const char* szText, int x, int y, float fScale)
{
   if ( NULL == pFont || NULL == szText || 0 == szText[0] )
      return;

   if ( y < 0 )
      return;
   if ( y + pFont->lineHeight*fScale >= m_iRenderHeight )
      return;

   int xBoundingStart = 5000;
   int yBoundingStart = 5000;
   int xBoundingEnd = 0;
   int yBoundingEnd = 0;

   while ( *szText )
   {
      if ( *szText < pFont->charIdFirst || *szText > pFont->charIdLast )
      {
         szText++;
         continue;
      }
      if ( x < 0 )
      {
         x += pFont->chars[(*szText)-pFont->charIdFirst].xAdvance*fScale;
         x += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;
         szText++;
         continue;
      }
      if ( x + pFont->chars[(*szText)-pFont->charIdFirst].xAdvance*fScale + pFont->dxLetters * pFont->lineHeight*m_fPixelWidth >= m_iRenderWidth )
         break;

      int xImg = pFont->chars[(*szText)-pFont->charIdFirst].imgXOffset;
      int yImg = pFont->chars[(*szText)-pFont->charIdFirst].imgYOffset;
      int wImg = pFont->chars[(*szText)-pFont->charIdFirst].width;
      int hImg = pFont->chars[(*szText)-pFont->charIdFirst].height;
      unsigned char *img_pointer = (unsigned char *)(pFont->pImage->data + (yImg * pFont->pImage->width * m_pFBG->components + xImg * m_pFBG->components));

      fbg_imageDrawAlpha(m_pFBG, pFont->pImage, x,y, wImg*fScale, hImg*fScale, xImg, yImg, wImg, hImg);

      if ( x < xBoundingStart )
         xBoundingStart = x;
      if ( x + wImg*fScale > xBoundingEnd )
         xBoundingEnd = x + wImg*fScale;
      if ( y < yBoundingStart )
         yBoundingStart = y;
      if ( y + hImg*fScale > yBoundingEnd )
         yBoundingEnd = y + hImg*fScale;

      x += pFont->chars[(*szText)-pFont->charIdFirst].xAdvance*fScale;
      x += pFont->dxLetters * pFont->lineHeight*m_fPixelWidth;
      szText++;
   }
}


void RenderEngineRaw::drawLine(float x1, float y1, float x2, float y2)
{
   if ( x1 < 0 && x2 < 0 )
      return;
   if ( y1 < 0 && y2 < 0 )
      return;
   if ( x1 > 1.0 && x2 > 1.0 )
      return;
   if ( y1 > 1.0 && y2 > 1.0 )
      return;

   if ( x1 > 1.0 - m_fPixelWidth )
      x1 = 1.0 - m_fPixelWidth;
   if ( x2 > 1.0 - m_fPixelWidth )
      x2 = 1.0 - m_fPixelWidth;

   if ( y1 > 1.0 - m_fPixelHeight )
      y1 = 1.0 - m_fPixelHeight;
   if ( y2 > 1.0 - m_fPixelHeight )
      y2 = 1.0 - m_fPixelHeight;

   if ( (x2-x1)*(x2-x1) + (y2-y1)*(y2-y1) < 0.5 * m_fPixelWidth * m_fPixelWidth )
      return;

   u8 alfa = m_ColorStroke[3];

   if ( m_fStrokeSize < 1.5 )
   {
      fbg_line(m_pFBG, x1*m_iRenderWidth, y1*m_iRenderHeight, x2*m_iRenderWidth, y2*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);
      return;
   }

   float xp1 = -(y2-y1);
   float yp1 = (x2-x1);
   float len = sqrtf(xp1*xp1+yp1*yp1);
   if ( fabs(len) > 0.0001 )
   {
   xp1 = xp1 / len;
   yp1 = yp1 / len;
   }
   float xp2 = (y2-y1);
   float yp2 = -(x2-x1);
   len = sqrtf(xp2*xp2+yp2*yp2);
   if ( fabs(len) > 0.0001 )
   {
   xp2 = xp2 / len;
   yp2 = yp2 / len;
   }
   if ( m_fStrokeSize < 2.5 )
   {
      xp1 *= 0.5*m_fPixelWidth;
      yp1 *= 0.5*m_fPixelHeight;

      xp2 *= 0.5*m_fPixelWidth;
      yp2 *= 0.5*m_fPixelHeight;

      if ( x1+xp1 >= 0.0 && x1+xp1 <= 1.0 - m_fPixelWidth )
      if ( x2+xp1 >= 0.0 && x2+xp1 <= 1.0 - m_fPixelWidth )
      if ( y1+yp1 >= 0.0 && y1+yp1 <= 1.0 - m_fPixelHeight )
      if ( y2+yp1 >= 0.0 && y2+yp1 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp1)*m_iRenderWidth, (y1+yp1)*m_iRenderHeight, (x2+xp1)*m_iRenderWidth, (y2+yp1)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      if ( x1+xp2 >= 0.0 && x1+xp2 <= 1.0 - m_fPixelWidth )
      if ( x2+xp2 >= 0.0 && x2+xp2 <= 1.0 - m_fPixelWidth )
      if ( y1+yp2 >= 0.0 && y1+yp2 <= 1.0 - m_fPixelHeight )
      if ( y2+yp2 >= 0.0 && y2+yp2 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp2)*m_iRenderWidth, (y1+yp2)*m_iRenderHeight, (x2+xp2)*m_iRenderWidth, (y2+yp2)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);
   }

   if ( m_fStrokeSize < 3.5 )
   {
      fbg_line(m_pFBG, x1*m_iRenderWidth, y1*m_iRenderHeight, x2*m_iRenderWidth, y2*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      xp1 *= 0.99*m_fPixelWidth;
      yp1 *= 0.99*m_fPixelHeight;

      xp2 *= 0.99*m_fPixelWidth;
      yp2 *= 0.99*m_fPixelHeight;

      if ( x1+xp1 >= 0.0 && x1+xp1 <= 1.0 - m_fPixelWidth )
      if ( x2+xp1 >= 0.0 && x2+xp1 <= 1.0 - m_fPixelWidth )
      if ( y1+yp1 >= 0.0 && y1+yp1 <= 1.0 - m_fPixelHeight )
      if ( y2+yp1 >= 0.0 && y2+yp1 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp1)*m_iRenderWidth, (y1+yp1)*m_iRenderHeight, (x2+xp1)*m_iRenderWidth, (y2+yp1)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      if ( x1+xp2 >= 0.0 && x1+xp2 <= 1.0 - m_fPixelWidth )
      if ( x2+xp2 >= 0.0 && x2+xp2 <= 1.0 - m_fPixelWidth )
      if ( y1+yp2 >= 0.0 && y1+yp2 <= 1.0 - m_fPixelHeight )
      if ( y2+yp2 >= 0.0 && y2+yp2 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp2)*m_iRenderWidth, (y1+yp2)*m_iRenderHeight, (x2+xp2)*m_iRenderWidth, (y2+yp2)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);
   }

   //if ( m_fStrokeSize < 4.5 )
   {
      float xp1b = xp1*1.5*m_fPixelWidth;
      float yp1b = yp1*1.5*m_fPixelHeight;
      float xp2b = xp2*1.5*m_fPixelWidth;
      float yp2b = yp2*1.5*m_fPixelHeight;

      if ( x1+xp1b >= 0.0 && x1+xp1b <= 1.0 - m_fPixelWidth )
      if ( x2+xp1b >= 0.0 && x2+xp1b <= 1.0 - m_fPixelWidth )
      if ( y1+yp1b >= 0.0 && y1+yp1b <= 1.0 - m_fPixelHeight )
      if ( y2+yp1b >= 0.0 && y2+yp1b <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp1b)*m_iRenderWidth, (y1+yp1b)*m_iRenderHeight, (x2+xp1b)*m_iRenderWidth, (y2+yp1b)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      if ( x1+xp2b >= 0.0 && x1+xp2b <= 1.0 - m_fPixelWidth )
      if ( x2+xp2b >= 0.0 && x2+xp2b <= 1.0 - m_fPixelWidth )
      if ( y1+yp2b >= 0.0 && y1+yp2b <= 1.0 - m_fPixelHeight )
      if ( y2+yp2b >= 0.0 && y2+yp2b <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp2b)*m_iRenderWidth, (y1+yp2b)*m_iRenderHeight, (x2+xp2b)*m_iRenderWidth, (y2+yp2b)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      xp1 *= 0.5*m_fPixelWidth;
      yp1 *= 0.5*m_fPixelHeight;
      xp2 *= 0.5*m_fPixelWidth;
      yp2 *= 0.5*m_fPixelHeight;

      if ( x1+xp1 >= 0.0 && x1+xp1 <= 1.0 - m_fPixelWidth )
      if ( x2+xp1 >= 0.0 && x2+xp1 <= 1.0 - m_fPixelWidth )
      if ( y1+yp1 >= 0.0 && y1+yp1 <= 1.0 - m_fPixelHeight )
      if ( y2+yp1 >= 0.0 && y2+yp1 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp1)*m_iRenderWidth, (y1+yp1)*m_iRenderHeight, (x2+xp1)*m_iRenderWidth, (y2+yp1)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);

      if ( x1+xp2 >= 0.0 && x1+xp2 <= 1.0 - m_fPixelWidth )
      if ( x2+xp2 >= 0.0 && x2+xp2 <= 1.0 - m_fPixelWidth )
      if ( y1+yp2 >= 0.0 && y1+yp2 <= 1.0 - m_fPixelHeight )
      if ( y2+yp2 >= 0.0 && y2+yp2 <= 1.0 - m_fPixelHeight )
         fbg_line(m_pFBG, (x1+xp2)*m_iRenderWidth, (y1+yp2)*m_iRenderHeight, (x2+xp2)*m_iRenderWidth, (y2+yp2)*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], alfa);
   }
}

void RenderEngineRaw::drawRect(float xPos, float yPos, float fWidth, float fHeight)
{
   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   if ( x >= m_iRenderWidth || y >= m_iRenderHeight)
      return;

   if ( x+w <= 0 || y+h <= 0 )
      return;

   if ( x < 0 )
   {
      w += x;
      x = 0;
   }
   if ( y < 0 )
   {
      h += y;
      y = 0;
   }
   if ( x+w >= m_iRenderWidth )
      w = m_iRenderWidth-x-1;
   if ( y+h >= m_iRenderHeight )
      h = m_iRenderHeight-y-1;

   if ( w <= 0 || h <= 0 )
      return;

   if ( 0 != m_ColorFill[3] )
      fbg_recta(m_pFBG, x,y, w,h, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);

   if ( (m_ColorStroke[0] != m_ColorFill[0]) ||
        (m_ColorStroke[1] != m_ColorFill[1]) ||
        (m_ColorStroke[2] != m_ColorFill[2]) ||
        (m_ColorStroke[3] != m_ColorFill[3]) )
   if ( m_ColorStroke[3] > 0 && m_fStrokeSize >= 0.9 )
   {
      fbg_hline(m_pFBG, x,y,w , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_hline(m_pFBG, x,y+h-1,w , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x,y,h , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x+w-1,y,h , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      if ( m_fStrokeSize >= 1.9 )
      {
         if ( x > 0 && y > 0 && x+w+1 < m_iRenderWidth )
            fbg_hline(m_pFBG, x-1,y-1,w+2 , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
         if ( x > 0 && y+h < m_iRenderHeight && x+w+1 < m_iRenderWidth )
            fbg_hline(m_pFBG, x-1,y+h,w+2 , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
         if ( x > 0 && y > 0 && y+h < m_iRenderHeight )
            fbg_vline(m_pFBG, x-1,y,h , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
         if ( x+w<m_iRenderWidth && y > 0 && y+h < m_iRenderHeight )
            fbg_vline(m_pFBG, x+w,y,h , m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      }
   }
}

void RenderEngineRaw::drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius)
{
   //drawRect(xPos, yPos,fWidth, fHeight);
   //return;

   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   if ( x >= m_iRenderWidth || y >= m_iRenderHeight)
      return;

   if ( x+w <= 0 || y+h <= 0 )
      return;

   if ( x < 0 )
   {
      w += x;
      x = 0;
   }
   if ( y < 0 )
   {
      h += y;
      y = 0;
   }

   if ( x+w >= m_iRenderWidth )
      w = m_iRenderWidth-x-1;
   if ( y+h >= m_iRenderHeight )
      h = m_iRenderHeight-y-1;

   if ( w <= 0 || h <= 0 )
      return;
   if ( w < 6.0*m_fPixelWidth || h < 6.0*m_fPixelHeight )
      return;

   if ( 0 != m_ColorFill[3] )
      fbg_recta(m_pFBG, x+3,y, w-5,h, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);

   fbg_vline(m_pFBG, x+2,y+1, h-2, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
   fbg_vline(m_pFBG, x+1,y+1, h-2 , m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
   fbg_vline(m_pFBG, x,y+3, h-6, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);

   fbg_vline(m_pFBG, x+w-2,y+1, h-2, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
   fbg_vline(m_pFBG, x+w-1,y+1, h-2, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);
   fbg_vline(m_pFBG, x+w,y+3, h-6, m_ColorFill[0], m_ColorFill[1], m_ColorFill[2], m_ColorFill[3]);

   if ( (m_ColorStroke[0] != m_ColorFill[0]) ||
        (m_ColorStroke[1] != m_ColorFill[1]) ||
        (m_ColorStroke[2] != m_ColorFill[2]) ||
        (m_ColorStroke[3] != m_ColorFill[3]))
   if ( m_ColorStroke[3] > 0 && m_fStrokeSize >= 0.9 )
   {
      fbg_hline(m_pFBG, x+3,y,w-6, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_hline(m_pFBG, x+1,y+1, 2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_hline(m_pFBG, x+w-4,y+1,2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);

      fbg_hline(m_pFBG, x+3,y+h,w-6, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_hline(m_pFBG, x+1,y+h-1,2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_hline(m_pFBG, x+w-4,y+h-1,2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);

      fbg_vline(m_pFBG, x,y+3, h-6, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x+1,y+1, 2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x+1,y+h-3, 2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);

      fbg_vline(m_pFBG, x+w,y+3, h-6, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x+w-1,y+1, 2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      fbg_vline(m_pFBG, x+w-1,y+h-3, 2, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
   }
}

void RenderEngineRaw::drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
   drawLine(x1,y1,x2,y2);
   drawLine(x2,y2,x3,y3);
   drawLine(x3,y3,x1,y1);
}

void RenderEngineRaw::drawPolyLine(float* x, float* y, int count)
{
   for( int i=0; i<count-1; i++ )
      drawLine(x[i], y[i], x[i+1], y[i+1]);
   drawLine(x[count-1], y[count-1], x[0], y[0]);
}

void RenderEngineRaw::fillPolygon(float* x, float* y, int count)
{
   if ( count < 3 || count > 120 )
      return;
   float xIntersections[256];
   int countIntersections = 0;
   float yMin, yMax;
   float xMin, xMax;

   xMin = xMax = x[0];
   yMin = yMax = y[0];

   for( int i=1; i<count; i++ )
   {
      if ( x[i] < xMin ) xMin = x[i];
      if ( y[i] < yMin ) yMin = y[i];
      if ( x[i] > xMax ) xMax = x[i];
      if ( y[i] > yMax ) yMax = y[i];
   }

   for( float yLine = yMin; yLine <= yMax; yLine += m_fPixelHeight )
   {
      countIntersections = 0;
      for( int i=0; i<count; i++ )
      {
         int j = (i+1)%count;

         // Horizontal line
         if ( fabs(y[i]-yLine) < 0.3*m_fPixelHeight && fabs(y[j]-yLine) < 0.3*m_fPixelHeight )
            drawLine(x[i], y[i], x[j], y[j]);
         else if ( y[i] <= yLine && y[j] >= yLine )
         {
            float xInt = x[i] + (x[j]-x[i])*(yLine-y[i])/(y[j]-y[i]);
            xIntersections[countIntersections] = xInt;
            countIntersections++;
         }
         else if ( y[i] >= yLine && y[j] <= yLine )
         {
            float xInt = x[j] + (x[i]-x[j])*(yLine-y[j])/(y[i]-y[j]);
            xIntersections[countIntersections] = xInt;
            countIntersections++;
         }
      }

      // Sort intersections;
      for( int i=0; i<countIntersections-1; i++ )
      for( int j=i+1; j<countIntersections; j++ )
      {
         if ( xIntersections[i] > xIntersections[j] )
         {
            float tmp = xIntersections[i];
            xIntersections[i] = xIntersections[j];
            xIntersections[j] = tmp;
         }
      }

      // Remove duplicates if odd count
      if ( countIntersections > 2 )
      if ( countIntersections%2 )
      for( int i=0; i<countIntersections-1; i++ )
      {
         if ( fabs(xIntersections[i]-xIntersections[i+1]) < 0.0001 )
         {
            while ( i<countIntersections-1 )
            {
               xIntersections[i] = xIntersections[i+1];
               i++;
            }
            countIntersections--;
            break;
         }
      }

      // Draw lines
      for( int i=0; i<countIntersections; i+= 2 )
         if ( xIntersections[i] >= xMin && xIntersections[i] <= xMax )
         if ( xIntersections[i+1] >= xMin && xIntersections[i+1] <= xMax )
            drawLine(xIntersections[i], yLine, xIntersections[i+1], yLine);
   }

   for( int i=0; i<count-1; i++ )
      drawLine(x[i], y[i], x[i+1], y[i+1]);
   drawLine(x[count-1], y[count-1], x[0], y[0]);

}


void RenderEngineRaw::fillCircle(float x, float y, float r)
{
   u8 tmpColor[4];

   memcpy(tmpColor, m_ColorStroke, 4*sizeof(u8));
   memcpy(m_ColorStroke, m_ColorFill, 4*sizeof(u8));

   float yTop = (y - r);
   float yBottom = (y + r);

   float yi = yTop;
   while ( yi <= yBottom )
   {
      float xi = sqrt(r*r-(yi-y)*(yi-y));
      drawLine(x-xi/getAspectRatio(), yi, x+xi/getAspectRatio(), yi);
      //drawLine(x-5*m_fPixelWidth, yi, x+5*m_fPixelWidth, yi);
      yi += m_fPixelHeight;
   }

   memcpy(m_ColorStroke, tmpColor, 4*sizeof(u8));
}

void RenderEngineRaw::drawCircle(float x, float y, float r)
{
   float xp[180];
   float yp[180];

   int points = r*6.0/(m_fPixelHeight*12.0);
   if ( points < 12 )
      points = 12;
   if ( points > 180 )
      points = 180;
   float dAngle = 360*0.0174533/(float)points;
   float angle = 0.0;
   for( int i=0; i<points; i++ )
   {
      xp[i] = x + r*cos(angle)/getAspectRatio();
      yp[i] = y + r*sin(angle);
      angle += dAngle;
   }

   drawPolyLine(xp,yp,points);
}

void RenderEngineRaw::drawArc(float x, float y, float r, float a1, float a2)
{
   float xp[180];
   float yp[180];

   int points = ((a2-a1)/360.0)*r*8.0/(m_fPixelHeight*12.0);
   if ( points < 6 )
      points = 6;
   if ( points > 180 )
      points = 180;
   float dAngle = (a2-a1)*0.0174533/((float)points-1);
   float angle = a1*0.0174533;
   for( int i=0; i<points; i++ )
   {
      xp[i] = x + r*cos(angle)/getAspectRatio();
      yp[i] = y - r*sin(angle);
      angle += dAngle;
   }

   for( int i=0; i<points-1; i++ )
      drawLine(xp[i], yp[i], xp[i+1], yp[i+1]);
}

