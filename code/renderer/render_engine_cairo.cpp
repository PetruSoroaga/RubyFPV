/*
    Ruby Licence
    Copyright (c) 2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../base/base.h"
#include "../base/config.h"
#include "render_engine_cairo.h"
#include "drm_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <utime.h>
#include <unistd.h>
#include <getopt.h>
#include <endian.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h> 

RenderEngineCairo::RenderEngineCairo()
:RenderEngine()
{
   log_line("RendererCairo: Init started.");

   type_drm_display_attributes* pDisplayInfo = ruby_drm_get_main_display_info();
   m_iRenderWidth = pDisplayInfo->iWidth;
   m_iRenderHeight = pDisplayInfo->iHeight;
   m_fPixelWidth = 1.0/(float)m_iRenderWidth;
   m_fPixelHeight = 1.0/(float)m_iRenderHeight;

   log_line("RendererCairo: Display size is: %d x %d, pixel size: %.4f x %.4f",
    m_iRenderWidth, m_iRenderHeight,
    m_fPixelWidth, m_fPixelHeight);
   type_drm_buffer* pMainDisplayBuffer = ruby_drm_core_get_main_draw_buffer();
   type_drm_buffer* pBackDisplayBuffer = ruby_drm_core_get_back_draw_buffer();
   
   log_line("RendererCairo: Display buffers size: front: %d x %d, back: %d x %d",
      pMainDisplayBuffer->uWidth, pMainDisplayBuffer->uHeight,
      pBackDisplayBuffer->uWidth, pBackDisplayBuffer->uHeight );
   
   m_uRenderDrawSurfacesIds[0] = pMainDisplayBuffer->uBufferId;
   m_uRenderDrawSurfacesIds[1] = pBackDisplayBuffer->uBufferId;

   m_pMainCairoSurface[0] = cairo_image_surface_create_for_data (pMainDisplayBuffer->pData, CAIRO_FORMAT_ARGB32, 
       pMainDisplayBuffer->uWidth, pMainDisplayBuffer->uHeight, pMainDisplayBuffer->uStride);
   m_pMainCairoSurface[1] = cairo_image_surface_create_for_data (pBackDisplayBuffer->pData, CAIRO_FORMAT_ARGB32, 
       pBackDisplayBuffer->uWidth, pBackDisplayBuffer->uHeight, pBackDisplayBuffer->uStride);
   if ( (NULL == m_pMainCairoSurface[0]) || (NULL == m_pMainCairoSurface[1]) )
      log_softerror_and_alarm("RendererCairo: Failed to create main and back cairo surfaces.");
   else
      log_line("RendererCairo: Created main and back cairo surfaces for render buffer ids %u and %u", m_uRenderDrawSurfacesIds[0], m_uRenderDrawSurfacesIds[1]);

   m_pCairoCtx = NULL;
   
   m_fStrokeSize = 1.0;
   
   m_iCountImages = 0;
   m_iCountIcons = 0;
   m_CurrentImageId = 0;
   m_CurrentIconId = 0;
   log_line("RendererCairo: Render init done.");
}


RenderEngineCairo::~RenderEngineCairo()
{
   if ( NULL != m_pCairoCtx )
      cairo_destroy(m_pCairoCtx);
   m_pCairoCtx = NULL; 

   if ( NULL != m_pMainCairoSurface[0] )
      cairo_surface_destroy(m_pMainCairoSurface[0]);
   if ( NULL != m_pMainCairoSurface[1] )
      cairo_surface_destroy(m_pMainCairoSurface[1]);

   m_pMainCairoSurface[0] = NULL;
   m_pMainCairoSurface[1] = NULL;
}

void* RenderEngineCairo::getDrawContext()
{
   return m_pCairoCtx;
}


void RenderEngineCairo::startFrame()
{
   type_drm_buffer* pOutputBufferInfo = ruby_drm_core_get_back_draw_buffer();
   
   memset(pOutputBufferInfo->pData, m_uClearBufferByte, pOutputBufferInfo->uSize);
   
   if ( NULL != m_pCairoCtx )
      cairo_destroy(m_pCairoCtx);
   m_pCairoCtx = NULL; 

   if ( pOutputBufferInfo->uBufferId == m_uRenderDrawSurfacesIds[0] )
   if ( NULL != m_pMainCairoSurface[0] )
      m_pCairoCtx = cairo_create(m_pMainCairoSurface[0]);

   if ( pOutputBufferInfo->uBufferId == m_uRenderDrawSurfacesIds[1] )
   if ( NULL != m_pMainCairoSurface[1] )
      m_pCairoCtx = cairo_create(m_pMainCairoSurface[1]);

   if ( NULL == m_pCairoCtx )
      return;

   cairo_select_font_face(m_pCairoCtx, "Roboto", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
 
}

void RenderEngineCairo::endFrame()
{
   ruby_drm_swap_mainback_buffers();
}


void RenderEngineCairo::setStroke(const double* color, float fStrokeSize)
{
   RenderEngine::setStroke(color, fStrokeSize);
   if ( m_fStrokeSize > 0.5 )
      cairo_set_line_width(m_pCairoCtx, m_fStrokeSize*(m_fPixelWidth+m_fPixelHeight)*0.6);
   else
      cairo_set_line_width(m_pCairoCtx, m_fStrokeSize);
}


void RenderEngineCairo::setStrokeSize(float fStrokeSize)
{
   RenderEngine::setStrokeSize(fStrokeSize);
   if ( m_fStrokeSize > 0.5 )
      cairo_set_line_width(m_pCairoCtx, m_fStrokeSize*(m_fPixelWidth+m_fPixelHeight)*0.6);
   else
      cairo_set_line_width(m_pCairoCtx, m_fStrokeSize);
}

void* RenderEngineCairo::_loadRawFontImageObject(const char* szFileName)
{
   if ( (NULL == szFileName) || (0 == szFileName[0]) )
   {
      log_softerror_and_alarm("[RendererCairo] Failed to load image. Invalid filename.");
      return NULL;
   }
   cairo_surface_t* pImage = cairo_image_surface_create_from_png (szFileName);
   if ( (NULL == pImage) || (cairo_image_surface_get_stride(pImage) <= 0) )
      log_softerror_and_alarm("[RendererCairo] Failed to load image (%s)", szFileName);
   else
      log_line("[RendererCairo] Loaded image (%s), stride: %d, wxh: %dx%d, ptr: %X",
          szFileName, cairo_image_surface_get_stride(pImage),
          cairo_image_surface_get_width(pImage),
          cairo_image_surface_get_width(pImage), pImage );
   return (void*) pImage;
}

void RenderEngineCairo::_freeRawFontImageObject(void* pImageObject)
{
   if ( NULL == pImageObject )
      return;
   cairo_surface_destroy((cairo_surface_t*)pImageObject);
   log_line("[RendererCairo] Destroyed image object %X", pImageObject);
}

void RenderEngineCairo::setFontOutlineColor(u32 idFont, u8 r, u8 g, u8 b, u8 a)
{

}

u32 RenderEngineCairo::loadImage(const char* szFile)
{
   if ( m_iCountImages > MAX_RAW_IMAGES )
      return 0;

   if ( access( szFile, R_OK ) == -1 )
      return 0;

   if ( NULL != strstr(szFile, ".png") )
   {
      m_pImages[m_iCountImages] = cairo_image_surface_create_from_png(szFile);
   }
   else
   {
      return 0;
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

void RenderEngineCairo::freeImage(u32 idImage)
{
   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == idImage )
      {
         indexImage = i;
         break;
      }
   }
   if ( -1 == indexImage )
      return;

   cairo_surface_destroy(m_pImages[indexImage]);

   for( int i=indexImage; i<m_iCountImages-1; i++ )
   {
      m_pImages[i] = m_pImages[i+1];
      m_ImageIds[i] = m_ImageIds[i+1];
   }
   m_iCountImages--;
}

u32 RenderEngineCairo::loadIcon(const char* szFile)
{
   if ( m_iCountIcons > MAX_RAW_ICONS )
      return 0;

   if ( access( szFile, R_OK ) == -1 )
      return 0;

   if ( NULL != strstr(szFile, ".png") )
   {
      m_pIcons[m_iCountIcons] = cairo_image_surface_create_from_png(szFile);
      //m_pIconsMip[m_iCountIcons][0] = cairo_image_surface_create_from_png(szFile);
      //m_pIconsMip[m_iCountIcons][1] = cairo_image_surface_create_from_png(szFile);
      m_pIconsMip[m_iCountIcons][0] = NULL;
      m_pIconsMip[m_iCountIcons][1] = NULL;
   }
   else
   {
      return 0;
   }
   if ( NULL != m_pIcons[m_iCountIcons] )
      log_line("Loaded icon %s, id: %u", szFile, m_CurrentIconId+1);
   else
      log_softerror_and_alarm("Failed to load icon %s", szFile);

   // To fix : build MIp images for icons
   //_buildMipImage(m_pIcons[m_iCountIcons], m_pIconsMip[m_iCountIcons][0]);
   //_buildMipImage(m_pIconsMip[m_iCountIcons][0], m_pIconsMip[m_iCountIcons][1]);

   m_CurrentIconId++;
   m_IconIds[m_iCountIcons] = m_CurrentIconId;
   m_iCountIcons++;
   return m_CurrentIconId;
}

void RenderEngineCairo::freeIcon(u32 idIcon)
{
   int indexIcon = -1;
   for( int i=0; i<m_iCountIcons; i++ )
   {
      if ( m_IconIds[i] == idIcon )
      {
         indexIcon = i;
         break;
      }
   }
   if ( -1 == indexIcon )
      return;

   cairo_surface_destroy(m_pIcons[indexIcon]);
   if ( NULL != m_pIconsMip[indexIcon][0] )
      cairo_surface_destroy(m_pIconsMip[indexIcon][0]);
   if ( NULL != m_pIconsMip[indexIcon][1] )
      cairo_surface_destroy(m_pIconsMip[indexIcon][1]);

   for( int i=indexIcon; i<m_iCountIcons-1; i++ )
   {
      m_pIcons[i] = m_pIcons[i+1];
      m_pIconsMip[i][0] = m_pIconsMip[i+1][0];
      m_pIconsMip[i][1] = m_pIconsMip[i+1][1];
      m_IconIds[i] = m_IconIds[i+1];
   }
   m_iCountIcons--;
}

int RenderEngineCairo::getImageWidth(u32 uImageId)
{
   if ( uImageId < 1 )
      return 0;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( (-1 == indexImage) || (NULL == m_pImages[indexImage]) )
      return 0;
  
   return (int)cairo_image_surface_get_width(m_pImages[indexImage]);
}

int RenderEngineCairo::getImageHeight(u32 uImageId)
{
   if ( uImageId < 1 )
      return 0;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( (-1 == indexImage) || (NULL == m_pImages[indexImage]) )
      return 0;
  
   return (int)cairo_image_surface_get_height(m_pImages[indexImage]);
}

void RenderEngineCairo::changeImageHue(u32 uImageId, u8 r, u8 g, u8 b)
{
   if ( uImageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( (-1 == indexImage) || (NULL == m_pImages[indexImage]) )
      return;


   int iWidth = cairo_image_surface_get_width(m_pImages[indexImage]);
   int iHeight = cairo_image_surface_get_height(m_pImages[indexImage]);
   int iImageStride = cairo_image_surface_get_stride((cairo_surface_t*)m_pImages[indexImage]);
   u8* pImageData = cairo_image_surface_get_data((cairo_surface_t*)m_pImages[indexImage]);

   for( int y=0; y<iHeight; y++ )
   {
      u8* pDestLine = (u8*)(pImageData + y * iImageStride);
      for( int x=0; x<iWidth; x++ )
      {
         if ( *pDestLine > 220 )
         if ( *(pDestLine+1) > 220 )
         if ( *(pDestLine+2) > 220 )
         {
            *pDestLine = ((unsigned int)(*pDestLine) * (unsigned int)b) >> 8;
            *(pDestLine+1) = ((unsigned int)(*(pDestLine+1)) * (unsigned int)g) >> 8;
            *(pDestLine+2) = ((unsigned int)(*(pDestLine+2)) * (unsigned int)r) >> 8;
         }
         pDestLine += 4;
      }
   }
}

void RenderEngineCairo::rotate180()
{
}

void RenderEngineCairo::drawImage(float xPos, float yPos, float fWidth, float fHeight, u32 uImageId)
{
   if ( uImageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( -1 == indexImage )
      return;
   if ( NULL == m_pImages[indexImage] )
      return;
  
   double scaleX = cairo_image_surface_get_width(m_pImages[indexImage]) / (float) m_iRenderWidth;
   double scaleY = cairo_image_surface_get_height(m_pImages[indexImage]) / (float) m_iRenderHeight;
   cairo_scale(m_pCairoCtx, 1.0/scaleX, 1.0/scaleY);
   cairo_set_source_surface(m_pCairoCtx, m_pImages[indexImage], 0,0);
   cairo_pattern_set_filter(cairo_get_source(m_pCairoCtx), CAIRO_FILTER_NEAREST);
   cairo_paint(m_pCairoCtx);
   cairo_scale(m_pCairoCtx, scaleX, scaleY);
}

void RenderEngineCairo::drawImageAlpha(float xPos, float yPos, float fWidth, float fHeight, u32 uImageId, u8 uAlpha)
{
   // uAlpha is 0..255

   if ( uImageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( -1 == indexImage )
      return;
   if ( NULL == m_pImages[indexImage] )
      return;
 
   //if ( uAlpha == 255 )
      drawImage(xPos, yPos, fWidth, fHeight, uImageId);
   /*
   else
   {
      int iSrcWidth = cairo_image_surface_get_width(m_pImages[indexImage]);
      int iSrcHeight = cairo_image_surface_get_height(m_pImages[indexImage]);
      u8 uTmp[4];
      memcpy(uTmp, m_ColorFill, 4*sizeof(u8));
      m_ColorFill[0] = 255;
      m_ColorFill[1] = 255;
      m_ColorFill[2] = 255;
      m_ColorFill[3] = uAlpha;
      bltImage(xPos, yPos, fWidth, fHeight, 0,0, iSrcWidth, iSrcHeight, uImageId);
      memcpy(m_ColorFill, uTmp, 4*sizeof(u8));
   }
   */
}

void RenderEngineCairo::bltImage(float xPosDest, float yPosDest, float fWidthDest, float fHeightDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uImageId)
{
   if ( uImageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( -1 == indexImage )
      return;
   if ( NULL == m_pImages[indexImage] )
      return;
  
   int xDest = xPosDest*m_iRenderWidth;
   int yDest = yPosDest*m_iRenderHeight;
   int wDest = fWidthDest*m_iRenderWidth;
   int hDest = fHeightDest*m_iRenderHeight;

   if ( (xDest < 0) || (yDest < 0) || (xDest+wDest > m_iRenderWidth) || (yDest+hDest > m_iRenderHeight) )
      return;

   type_drm_buffer* pOutputBufferInfo = ruby_drm_core_get_back_draw_buffer();
   u8* pSrcImageData = cairo_image_surface_get_data(m_pImages[indexImage]);
   int iSrcImageStride = cairo_image_surface_get_stride(m_pImages[indexImage]);

   // Input, output surface format order is: BGRA
   u8 r = 255, g = 255, b = 255, a = 255;

   //float fImgWidth = cairo_image_surface_get_width(m_pImages[indexImage]);
   //float fImgHeight = cairo_image_surface_get_height(m_pImages[indexImage]);

   float dxIcon = (float)iSrcWidth/(float)wDest;
   float dyIcon = (float)iSrcHeight/(float)hDest;
   float fIconY = iSrcY;

   u8* pDestPixel = (u8*)&(pOutputBufferInfo->pData[yDest*pOutputBufferInfo->uStride + xDest*4]);

   for( int sy=0; sy<hDest; sy++ )
   {
      float fIconX = iSrcX;
      for( int sx=0; sx<wDest; sx++ )
      {
         u8* pSrcPixel = pSrcImageData + ((int)(fIconY)) * iSrcImageStride + ((int)fIconX) * 4;
   
         // Output surface format order is: BGRA
         
         b = *pSrcPixel;
         g = *(pSrcPixel+1);
         r = *(pSrcPixel+2);
         a = *(pSrcPixel+3);

         if ( a > 4 )
         {
            /*
            b = (b*m_ColorFill[2])>>8;
            *pDestPixel++ = b;
            g = (g*m_ColorFill[1])>>8;
            *pDestPixel++ = g;
            r = (r*m_ColorFill[0])>>8;
            *pDestPixel++ = r;
            a = (a*m_ColorFill[3])>>8;
            *pDestPixel++ = a;
            */
            
            b = (((b*m_ColorFill[2])>>8) * (255-m_ColorFill[3]) + ((*pDestPixel)*m_ColorFill[3]))>>8;
            *pDestPixel++ = b;

            g = (((g*m_ColorFill[1])>>8) * (255-m_ColorFill[3]) + ((*pDestPixel)*m_ColorFill[3]))>>8;
            *pDestPixel++ = g;

            r = (((r*m_ColorFill[0])>>8) * (255-m_ColorFill[3]) + ((*pDestPixel)*m_ColorFill[3]))>>8;
            *pDestPixel++ = r;

            a = (((a*m_ColorFill[3])>>8) * (255-m_ColorFill[3]) + ((*pDestPixel)*m_ColorFill[3]))>>8;
            *pDestPixel++ = a;
         }
         else
            pDestPixel+=4;

         fIconX += dxIcon;
      }
      pDestPixel += pOutputBufferInfo->uStride - wDest * 4;
      fIconY += dyIcon;
   }
}

void RenderEngineCairo::bltSprite(float xPosDest, float yPosDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uImageId)
{
   if ( uImageId < 1 )
      return;

   int indexImage = -1;
   for( int i=0; i<m_iCountImages; i++ )
   {
      if ( m_ImageIds[i] == uImageId )
      {
         indexImage = i;
         break;
      }
   }
   if ( -1 == indexImage )
      return;
   if ( NULL == m_pImages[indexImage] )
      return;
  
   int xDest = xPosDest*m_iRenderWidth;
   int yDest = yPosDest*m_iRenderHeight;
   
   if ( (xDest < 0) || (yDest < 0) || (xDest+iSrcWidth >= m_iRenderWidth) || (yDest+iSrcHeight >= m_iRenderHeight) )
      return;

   type_drm_buffer* pOutputBufferInfo = ruby_drm_core_get_back_draw_buffer();
   u8* pSrcImageData = cairo_image_surface_get_data(m_pImages[indexImage]);
   int iSrcImageStride = cairo_image_surface_get_stride(m_pImages[indexImage]);

   // Input, output surface format order is: BGRA
   u8 r = 255, g = 255, b = 255, a = 255;

   u8* pDestPixel = (u8*)&(pOutputBufferInfo->pData[yDest*pOutputBufferInfo->uStride + xDest*4]);
   u8* pSrcPixel = pSrcImageData + iSrcY * iSrcImageStride + iSrcX * 4;

   for( int sy=0; sy<iSrcHeight; sy++ )
   {
      for( int sx=0; sx<iSrcWidth; sx++ )
      {   
         // Output surface format order is: BGRA
         
         b = *pSrcPixel++;
         g = *pSrcPixel++;
         r = *pSrcPixel++;
         a = *pSrcPixel++;

         if ( a > 4 )
         {
            b = (((b*m_ColorFill[2])>>8) * a + ((*pDestPixel)*(255-a)))>>8;
            *pDestPixel++ = b;

            g = (((g*m_ColorFill[1])>>8) * a + ((*pDestPixel)*(255-a)))>>8;
            *pDestPixel++ = g;

            r = (((r*m_ColorFill[0])>>8) * a + ((*pDestPixel)*(255-a)))>>8;
            *pDestPixel++ = r;

            a = (((a*m_ColorFill[3])>>8) * a + ((*pDestPixel)*(255-a)))>>8;
            *pDestPixel++ = a;
         }
         else
            pDestPixel += 4;
      }
      pDestPixel += pOutputBufferInfo->uStride - iSrcWidth * 4;
      pSrcPixel += iSrcImageStride - iSrcWidth * 4;
   } 
}

void RenderEngineCairo::drawIcon(float xPos, float yPos, float fWidth, float fHeight, u32 uIconId)
{
   if ( uIconId < 1 )
      return;

   int indexIcon = -1;
   for( int i=0; i<m_iCountIcons; i++ )
   {
      if ( m_IconIds[i] == uIconId )
      {
         indexIcon = i;
         break;
      }
   }
   if ( -1 == indexIcon )
      return;
   if ( NULL == m_pIcons[indexIcon] )
      return;

   int x = xPos*m_iRenderWidth;
   int y = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   if ( (x < 0) || (y < 0) || (x+w >= m_iRenderWidth) || (y+h >= m_iRenderHeight) )
      return;

   type_drm_buffer* pOutputBufferInfo = ruby_drm_core_get_back_draw_buffer();
   u8* pSrcImageData = cairo_image_surface_get_data(m_pIcons[indexIcon]);
   int iSrcImageStride = cairo_image_surface_get_stride(m_pIcons[indexIcon]);

   u8 r = 255, g = 255, b = 255, a = 255;

   float fIconWidth = cairo_image_surface_get_width(m_pIcons[indexIcon]);
   float fIconHeight = cairo_image_surface_get_height(m_pIcons[indexIcon]);

   float dxIcon = fIconWidth/(float)w;
   float dyIcon = fIconHeight/(float)h;

   float yIcon = 0;
   int iyIcon = 0;
   u8* pDestPixel = (u8*)&(pOutputBufferInfo->pData[y*pOutputBufferInfo->uStride + x*4]);

   for( int sy=0; sy<h; sy++ )
   {
      iyIcon = (int)yIcon;
      if ( iyIcon >= (int) fIconHeight )
         break;
      int yIconOffset = iyIcon * iSrcImageStride;
      float xIcon = 0;
      for( int sx=0; sx<w; sx++ )
      {
         u8* pIconData = pSrcImageData + yIconOffset + ((int)xIcon) * 4;

         // Output surface format order is: BGRA

         b = *pIconData++;
         g = *pIconData++;
         r = *pIconData++;
         a = *pIconData++;

         if ( a > 4 )
         {
            b = (b*m_ColorFill[2])>>8;
            *pDestPixel++ = b;

            g = (g*m_ColorFill[1])>>8;
            *pDestPixel++ = g;

            r = (r*m_ColorFill[0])>>8;
            *pDestPixel++ = r;

            a = (a*m_ColorFill[3])>>8;
            *pDestPixel++ = a;
         }
         else
            pDestPixel+=4;
         xIcon += dxIcon;
      }
      pDestPixel += pOutputBufferInfo->uStride - w * 4;
      yIcon += dyIcon;
   }

   /*
   double scaleX = cairo_image_surface_get_width(m_pIcons[indexIcon]) / (float) w;
   double scaleY = cairo_image_surface_get_height(m_pIcons[indexIcon]) / (float) h;
   cairo_scale(m_pCairoCtx, 1.0/scaleX, 1.0/scaleY);
   cairo_set_source_surface(m_pCairoCtx, m_pIcons[indexIcon], x*scaleX,y*scaleY);
   cairo_pattern_set_filter(cairo_get_source(m_pCairoCtx), CAIRO_FILTER_NEAREST);
   cairo_paint(m_pCairoCtx);
   cairo_scale(m_pCairoCtx, scaleX, scaleY);
   */
}

void RenderEngineCairo::bltIcon(float xPosDest, float yPosDest, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, u32 uIconId)
{
   if ( uIconId < 1 )
      return;

   int indexIcon = -1;
   for( int i=0; i<m_iCountIcons; i++ )
   {
      if ( m_IconIds[i] == uIconId )
      {
         indexIcon = i;
         break;
      }
   }
   if ( -1 == indexIcon )
      return;
   if ( NULL == m_pIcons[indexIcon] )
      return;

   int ixPosDest = xPosDest*m_iRenderWidth;
   int iyPosDest = yPosDest*m_iRenderHeight;
   
   if ( (ixPosDest < 0) || (iyPosDest < 0) || (ixPosDest+iSrcWidth >= m_iRenderWidth) || (iyPosDest+iSrcHeight >= m_iRenderHeight) )
      return;

   type_drm_buffer* pOutputBufferInfo = ruby_drm_core_get_back_draw_buffer();
   u8* pSrcImageData = cairo_image_surface_get_data(m_pIcons[indexIcon]);
   int iSrcImageStride = cairo_image_surface_get_stride(m_pIcons[indexIcon]);

   for( int y=0; y<iSrcHeight; y++ )
   {
      u8* pDestLine = (u8*)&(pOutputBufferInfo->pData[(iyPosDest+y)*pOutputBufferInfo->uStride]);
      pDestLine += 4*ixPosDest;

      u8* pSrcLine = pSrcImageData + ((iSrcY +y)* iSrcImageStride);
      pSrcLine += 4 * iSrcX;

      for( int x=0; x<iSrcWidth; x++ )
      {
          u8 uAlpha = *(pSrcLine+3);
          *pDestLine = ((*(pSrcLine)) * uAlpha + (*(pDestLine)) * (255-uAlpha))/256;
          pDestLine++;
          pSrcLine++;
          *pDestLine = ((*(pSrcLine)) * uAlpha + (*(pDestLine)) * (255-uAlpha))/256;
          pDestLine++;
          pSrcLine++;
          *pDestLine = ((*(pSrcLine)) * uAlpha + (*(pDestLine)) * (255-uAlpha))/256;
          pDestLine++;
          pSrcLine++;
          pDestLine++;
          pSrcLine++;
      }
   }
}


inline void RenderEngineCairo::_blend_pixel(unsigned char* pixel, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
   // Output surface format order is: BGRA
   if ( *(pixel+3) == 0 )
   {
      *pixel = b;
      pixel++;
      *pixel = g;
      pixel++;
      *pixel = r;
      pixel++;
      *pixel = a;
      pixel++;
   }
   else if ( *(pixel+3) == 255 )
   {
      *pixel = ((a * b + (255 - a) * (*pixel)) >> 8);
      pixel++;
      *pixel = ((a * g + (255 - a) * (*pixel)) >> 8);
      pixel++;
      *pixel = ((a * r + (255 - a) * (*pixel)) >> 8);
      pixel++;
      pixel++;
   }
   else
   {
       *pixel = ((a * b + (255 - a) * (*pixel)) >> 8);
       pixel++;
       *pixel = ((a * g + (255 - a) * (*pixel)) >> 8);
       pixel++;
       *pixel = ((a * r + (255 - a) * (*pixel)) >> 8);
       pixel++;
       *pixel = (*pixel) + (((255-(*pixel))*a) >> 8);
       pixel++;
   }
}

void RenderEngineCairo::_draw_hline(int x, int y, int w, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
   type_drm_buffer* pOutputBufferInfo = ruby_drm_core_get_back_draw_buffer();
   u8* pDestLine = (&(pOutputBufferInfo->pData[0])) + y*pOutputBufferInfo->uStride + 4*x;
   for( int i=0; i<w; i++ )
   {
      *pDestLine++ = b;
      *pDestLine++ = g;
      *pDestLine++ = r;
      *pDestLine++ = a;
   }
}

void RenderEngineCairo::_draw_vline(int x, int y, int h, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
   type_drm_buffer* pOutputBufferInfo = ruby_drm_core_get_back_draw_buffer();
   u8* pDestLine = (&(pOutputBufferInfo->pData[0])) + y*pOutputBufferInfo->uStride + 4*x;
   for( int i=0; i<h; i++ )
   {
      *pDestLine++ = b;
      *pDestLine++ = g;
      *pDestLine++ = r;
      *pDestLine++ = a;
      pDestLine += pOutputBufferInfo->uStride-4;
   }
}
      
void RenderEngineCairo::drawLine(float x1, float y1, float x2, float y2)
{
   if ( fabs(y1-y2) < 0.0001 )
   {
      if ( x1 < 0 )
         x1 = 0.0;
      if ( x2 < 0 )
         x2 = 0.0;
      if ( x1 > 1.0 - m_fPixelWidth )
         x1 = 1.0 - m_fPixelWidth;
      if ( x2 > 1.0 - m_fPixelWidth )
         x2 = 1.0 - m_fPixelWidth;
      if ( fabs(x2-x1) < 0.0001 )
         return;

      if ( x1 < x2 )
         _draw_hline(x1*m_iRenderWidth, y1*m_iRenderHeight, (x2-x1)*m_iRenderWidth, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      else if ( x1 > x2 )
         _draw_hline(x2*m_iRenderWidth, y1*m_iRenderHeight, (x1-x2)*m_iRenderWidth, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      return;
   }
   if ( fabs(x1-x2) < 0.0001 )
   {
      if ( y1 < 0 )
         y1 = 0.0;
      if ( y2 < 0 )
         y2 = 0.0;
      if ( y1 > 1.0 - m_fPixelHeight )
         y1 = 1.0 - m_fPixelHeight;
      if ( y2 > 1.0 - m_fPixelHeight )
         y2 = 1.0 - m_fPixelHeight;
      if ( fabs(y2-y1) < 0.0001 )
         return;
        
      float yPos = y1;
      float h = (y2-y1);
      if ( y1 > y2 )
      {
         yPos = y2;
         h = y1 - y2;
      }
      if ( m_fStrokeSize < 1.5 )
         _draw_vline(x1*m_iRenderWidth, yPos*m_iRenderHeight, h*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);
      else
      {
         _draw_vline(x1*m_iRenderWidth-m_fPixelWidth*0.5, yPos*m_iRenderHeight, h*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);       
         _draw_vline(x1*m_iRenderWidth+m_fPixelWidth*0.5, yPos*m_iRenderHeight, h*m_iRenderHeight, m_ColorStroke[0], m_ColorStroke[1], m_ColorStroke[2], m_ColorStroke[3]);       
      }
      return;
   }

   cairo_move_to (m_pCairoCtx, x1 * m_iRenderWidth, y1 * m_iRenderHeight); 
   cairo_line_to (m_pCairoCtx, x2 * m_iRenderWidth, y2 * m_iRenderHeight);
   cairo_set_source_rgba(m_pCairoCtx, m_ColorStroke[0]/255.0, m_ColorStroke[1]/255.0, m_ColorStroke[2]/255.0, m_ColorStroke[3]/255.0);
   cairo_stroke (m_pCairoCtx);
}

void RenderEngineCairo::drawRect(float xPos, float yPos, float fWidth, float fHeight)
{   
   int xSt = xPos*m_iRenderWidth;
   int ySt = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   if ( ((xSt + w) <= 0) || ((ySt + h) <= 0) || (xSt >= m_iRenderWidth) || (ySt >= m_iRenderHeight) )
      return;

   if ( xSt < 0 )
   {
      w += xSt;
      xSt = 0;
   }

   if ( ySt < 0 )
   {
      h += ySt;
      ySt = 0;
   }

   if ( xSt + w > m_iRenderWidth )
      w = m_iRenderWidth - xSt;
   if ( ySt + h > m_iRenderHeight )
      h = m_iRenderHeight - ySt;

   if ( (w <= 0) || (h <= 0) )
      return;

   /*
   if ( m_bEnableRectBlending )
   {
      if ( m_fColorFill[3] > 0.001 )
      {
         cairo_rectangle(m_pCairoCtx, xPos * m_iRenderWidth, yPos * m_iRenderHeight, fWidth * m_iRenderWidth, fHeight * m_iRenderHeight);  
         cairo_set_source_rgba(m_pCairoCtx, m_fColorFill[0], m_fColorFill[1], m_fColorFill[2], m_fColorFill[3]);
         cairo_fill(m_pCairoCtx);
      }
      if ( m_fColorStroke[3] > 0.001 )
      if ( m_fStrokeSize > 0.00001 )
      {
         cairo_rectangle(m_pCairoCtx, xPos * m_iRenderWidth, yPos * m_iRenderHeight, fWidth * m_iRenderWidth, fHeight * m_iRenderHeight);  
         cairo_set_source_rgba(m_pCairoCtx, m_fColorStroke[0], m_fColorStroke[1], m_fColorStroke[2], m_fColorStroke[3]);
         cairo_stroke(m_pCairoCtx);
      }
      return;
   }
   */
   u8 r = m_ColorFill[0];
   u8 g = m_ColorFill[1];
   u8 b = m_ColorFill[2];
   u8 a = m_ColorFill[3];

   // Output surface format order is: BGRA
   if ( m_ColorFill[3] > 2 )
   {
      type_drm_buffer* pOutputBufferInfo = ruby_drm_core_get_back_draw_buffer();
      for( int y=0; y<h; y++ )
      {
         u8* pDestLine = (u8*)&(pOutputBufferInfo->pData[(ySt+y)*pOutputBufferInfo->uStride]);
         pDestLine += 4*xSt;
         for( int x=0; x<w; x++ )
         {
            //_blend_pixel(pDestLine, r,g,b,a);
            //pDestLine += 4;
            *pDestLine++ = b;
            *pDestLine++ = g;
            *pDestLine++ = r;
            *pDestLine++ = a;
         }
      }
   }
   if ( m_ColorStroke[3] > 2 )
   if ( m_fStrokeSize > 0.00001 )
   {
      r = m_ColorStroke[0];
      g = m_ColorStroke[1];
      b = m_ColorStroke[2];
      a = m_ColorStroke[3];

      if ( yPos >= 0 )
         _draw_hline(xSt,ySt, w , r,g,b,a);
      if ( yPos + fHeight < 1.0 )
         _draw_hline(xSt,ySt+h-1, w , r,g,b,a);
      if ( xPos >= 0 )
         _draw_vline(xSt,ySt+1, h-1 , r,g,b,a);
      if ( xPos + fWidth < 1.0 )
         _draw_vline(xSt+w-1,ySt+1,h-1 , r,g,b,a);
   }
   /*
   if ( m_fColorFill[3] > 0.001 )
   {
      cairo_rectangle(m_pCairoCtx, xPos * m_iRenderWidth, yPos * m_iRenderHeight, fWidth * m_iRenderWidth, fHeight * m_iRenderHeight);  
      cairo_set_source_rgba(m_pCairoCtx, m_fColorFill[0], m_fColorFill[1], m_fColorFill[2], m_fColorFill[3]);
      cairo_fill(m_pCairoCtx);
   }
   if ( m_fColorStroke[3] > 0.001 )
   if ( m_fStrokeSize > 0.00001 )
   {
      cairo_rectangle(m_pCairoCtx, xPos * m_iRenderWidth, yPos * m_iRenderHeight, fWidth * m_iRenderWidth, fHeight * m_iRenderHeight);  
      cairo_set_source_rgba(m_pCairoCtx, m_fColorStroke[0], m_fColorStroke[1], m_fColorStroke[2], m_fColorStroke[3]);
      cairo_stroke(m_pCairoCtx);
   }
   */
}

void RenderEngineCairo::drawRoundRect(float xPos, float yPos, float fWidth, float fHeight, float fCornerRadius)
{
   int xSt = xPos*m_iRenderWidth;
   int ySt = yPos*m_iRenderHeight;
   int w = fWidth*m_iRenderWidth;
   int h = fHeight*m_iRenderHeight;

   if ( (xSt >= m_iRenderWidth) || (ySt >= m_iRenderHeight) )
      return;

   if ( (xSt+w <= 0) || (ySt+h <= 0) )
      return;

   if ( xSt < 0 )
   {
      w += xSt;
      xSt = 0;
   }
   if ( ySt < 0 )
   {
      h += ySt;
      ySt = 0;
   }

   if ( xSt+w >= m_iRenderWidth )
      w = m_iRenderWidth-xSt-1;
   if ( ySt+h >= m_iRenderHeight )
      h = m_iRenderHeight-ySt-1;

   if ( (w < 6.0*m_fPixelWidth) || (h < 6.0*m_fPixelHeight) )
      return;

   // Output surface format order is: BGRA
   if ( m_ColorFill[3] > 2 )
   {
      u8 r = m_ColorFill[0];
      u8 g = m_ColorFill[1];
      u8 b = m_ColorFill[2];
      u8 a = m_ColorFill[3];
      type_drm_buffer* pOutputBufferInfo = ruby_drm_core_get_back_draw_buffer();
      for( int y=0; y<h; y++ )
      {
         u8* pDestLine = (u8*)&(pOutputBufferInfo->pData[(ySt+y)*pOutputBufferInfo->uStride]);
         pDestLine += 4*(xSt+3);
         for( int x=0; x<(w-5); x++ )
         {
            //_blend_pixel(pDestLine, r,g,b,a);
            //pDestLine += 4;
            *pDestLine++ = b;
            *pDestLine++ = g;
            *pDestLine++ = r;
            *pDestLine++ = a;
         }
      }
  
      _draw_vline(xSt+2, ySt+1, h-2 , r,g,b,a);
      _draw_vline(xSt+1, ySt+1, h-2 , r,g,b,a);
      _draw_vline(xSt,   ySt+3, h-6 , r,g,b,a);
     
      _draw_vline(xSt+w-2, ySt+1, h-2 , r,g,b,a);
      _draw_vline(xSt+w-1, ySt+1, h-2 , r,g,b,a);
      _draw_vline(xSt+w,   ySt+3, h-6 , r,g,b,a);
   }

   if ( (m_ColorStroke[3] > 2) && (m_fStrokeSize >= 0.9) )
   if ( (m_ColorStroke[0] != m_ColorFill[0]) ||
        (m_ColorStroke[1] != m_ColorFill[1]) ||
        (m_ColorStroke[2] != m_ColorFill[2]) ||
        (m_ColorStroke[3] != m_ColorFill[3]))
   {
      u8 r = m_ColorStroke[0];
      u8 g = m_ColorStroke[1];
      u8 b = m_ColorStroke[2];
      u8 a = m_ColorStroke[3];

      _draw_hline(xSt+3,  ySt,    w-6, r,g,b,a);
      _draw_hline(xSt+1,  ySt+1,  2, r,g,b,a);
      _draw_hline(xSt+w-4, ySt+1, 2, r,g,b,a);

      _draw_hline(xSt+3,  ySt+h,    w-6, r,g,b,a);
      _draw_hline(xSt+1,  ySt+h-1,  2, r,g,b,a);
      _draw_hline(xSt+w-4, ySt+h-1, 2, r,g,b,a);

      _draw_vline(xSt, ySt+3,  h-6 , r,g,b,a);
      _draw_vline(xSt+1, ySt+1,  2 , r,g,b,a);
      _draw_vline(xSt+1, ySt+h-3, 2 , r,g,b,a);

      _draw_vline(xSt+w, ySt+3,  h-6 , r,g,b,a);
      _draw_vline(xSt+w-1, ySt+1,  2 , r,g,b,a);
      _draw_vline(xSt+w-1, ySt+h-3, 2 , r,g,b,a);
   }


   //drawRect(xPos, yPos, fWidth, fHeight);

   /*
   double degrees = M_PI / 180.0;
   fCornerRadius = 10.0;
   cairo_new_sub_path (m_pCairoCtx);
   cairo_arc (m_pCairoCtx, xPos + fWidth - fCornerRadius, yPos + fCornerRadius, fCornerRadius, -90 * degrees, 0 * degrees);
   cairo_arc (m_pCairoCtx, xPos + fWidth - fCornerRadius, yPos + fHeight - fCornerRadius, fCornerRadius, 0 * degrees, 90 * degrees);
   cairo_arc (m_pCairoCtx, xPos + fCornerRadius, yPos + fHeight - fCornerRadius, fCornerRadius, 90 * degrees, 180 * degrees);
   cairo_arc (m_pCairoCtx, xPos + fCornerRadius, yPos + fCornerRadius, fCornerRadius, 180 * degrees, 270 * degrees);
   cairo_close_path (m_pCairoCtx);

   if ( m_fColorFill[3] > 0.001 )
   {
      cairo_set_source_rgba(m_pCairoCtx, m_fColorFill[0], m_fColorFill[1], m_fColorFill[2], m_fColorFill[3]);
      cairo_fill_preserve(m_pCairoCtx);
   }

   cairo_set_source_rgba(m_pCairoCtx, m_fColorStroke[0], m_fColorStroke[1], m_fColorStroke[2], m_fColorStroke[3]);
   cairo_stroke(m_pCairoCtx);
   */
}

void RenderEngineCairo::drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
   cairo_move_to (m_pCairoCtx, x1 * m_iRenderWidth, y1 * m_iRenderHeight); 
   cairo_line_to (m_pCairoCtx, x2 * m_iRenderWidth, y2 * m_iRenderHeight);
   cairo_line_to (m_pCairoCtx, x3 * m_iRenderWidth, y3 * m_iRenderHeight);
   cairo_close_path(m_pCairoCtx);
   cairo_set_source_rgba(m_pCairoCtx, m_ColorStroke[0]/255.0, m_ColorStroke[1]/255.0, m_ColorStroke[2]/255.0, m_ColorStroke[3]/255.0);
   cairo_stroke (m_pCairoCtx);
}

void RenderEngineCairo::fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3)
{
   cairo_move_to (m_pCairoCtx, x1 * m_iRenderWidth, y1 * m_iRenderHeight); 
   cairo_line_to (m_pCairoCtx, x2 * m_iRenderWidth, y2 * m_iRenderHeight);
   cairo_line_to (m_pCairoCtx, x3 * m_iRenderWidth, y3 * m_iRenderHeight);
   cairo_close_path(m_pCairoCtx);

   bool bStroke = false;
   if ( m_ColorStroke[3] > 2 )
   if ( m_fStrokeSize > 0.00001 )
      bStroke = true;

   if ( m_ColorFill[3] > 2 )
   {
      cairo_set_source_rgba(m_pCairoCtx, m_ColorFill[0]/255.0, m_ColorFill[1]/255.0, m_ColorFill[2]/255.0, m_ColorFill[3]/255.0);
      if ( bStroke )
         cairo_fill_preserve(m_pCairoCtx);
      else
         cairo_fill(m_pCairoCtx);
   }

   if ( bStroke )
   {
      cairo_set_source_rgba(m_pCairoCtx, m_ColorStroke[0]/255.0, m_ColorStroke[1]/255.0, m_ColorStroke[2]/255.0, m_ColorStroke[3]/255.0);
      cairo_stroke(m_pCairoCtx);
   }
}


void RenderEngineCairo::drawPolyLine(float* x, float* y, int count)
{
   for( int i=0; i<count-1; i++ )
      drawLine(x[i], y[i], x[i+1], y[i+1]);
   drawLine(x[count-1], y[count-1], x[0], y[0]);
}

void RenderEngineCairo::fillPolygon(float* x, float* y, int count)
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


void RenderEngineCairo::fillCircle(float x, float y, float r)
{
   if ( m_ColorFill[3] > 2 )
   {
      cairo_set_source_rgba(m_pCairoCtx, m_ColorFill[0]/255.0, m_ColorFill[1]/255.0, m_ColorFill[2]/255.0, m_ColorFill[3]/255.0);
      cairo_move_to (m_pCairoCtx, x * m_iRenderWidth + r * m_iRenderHeight, y * m_iRenderHeight);
      cairo_arc (m_pCairoCtx, x * m_iRenderWidth, y * m_iRenderHeight, r * m_iRenderHeight,
        0.0, 2 * M_PI);
      cairo_fill(m_pCairoCtx);
   }

   if ( m_ColorStroke[3] > 2 )
   {
      cairo_set_source_rgba(m_pCairoCtx, m_ColorStroke[0]/255.0, m_ColorStroke[1]/255.0, m_ColorStroke[2]/255.0, m_ColorStroke[3]/255.0);
      cairo_move_to (m_pCairoCtx, x * m_iRenderWidth + r * m_iRenderHeight, y * m_iRenderHeight);
      cairo_arc (m_pCairoCtx, x * m_iRenderWidth, y * m_iRenderHeight, r * m_iRenderHeight,
           0.0, 2 * M_PI);
      cairo_stroke(m_pCairoCtx);
   }
}

void RenderEngineCairo::drawCircle(float x, float y, float r)
{
   if ( m_ColorStroke[3] > 2 )
   {
      cairo_set_source_rgba(m_pCairoCtx, m_ColorStroke[0]/255.0, m_ColorStroke[1]/255.0, m_ColorStroke[2]/255.0, m_ColorStroke[3]/255.0);
      cairo_move_to (m_pCairoCtx, x * m_iRenderWidth + r * m_iRenderHeight, y * m_iRenderHeight);
      cairo_arc (m_pCairoCtx, x * m_iRenderWidth, y * m_iRenderHeight, r * m_iRenderHeight,
           0.0, 2 * M_PI);
      cairo_stroke(m_pCairoCtx);
   }
   /*
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
   */
}

void RenderEngineCairo::drawArc(float x, float y, float r, float a1, float a2)
{
}


void RenderEngineCairo::_drawSimpleText(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos)
{
   _drawSimpleTextScaled(pFont, szText, xPos, yPos, 1.0);
}

void RenderEngineCairo::_drawSimpleTextScaled(RenderEngineRawFont* pFont, const char* szText, float xPos, float yPos, float fScale)
{
   if ( (NULL == pFont) || (NULL == szText) || (0 == szText[0]) )
      return;

   if ( yPos < 0 )
      return;
   if ( xPos >= 1.0 )
      return;
   if ( yPos + pFont->lineHeight * fScale * m_fPixelHeight >= 1.0 )
      return;

   if ( m_bDrawBackgroundBoundingBoxes )
      _drawSimpleTextBoundingBox(pFont, szText, xPos, yPos, 1.0);


   cairo_set_font_size (m_pCairoCtx, pFont->lineHeight*0.8);
   cairo_text_extents_t cte;
   cairo_text_extents(m_pCairoCtx, szText, &cte);

   float fColor[4];
   if ( m_bDrawBackgroundBoundingBoxes && m_bDrawBackgroundBoundingBoxesTextUsesSameStrokeColor )
   {
      fColor[0] = m_ColorTextBackgroundBoundingBoxStrike[0]/255.0;
      fColor[1] = m_ColorTextBackgroundBoundingBoxStrike[1]/255.0;
      fColor[2] = m_ColorTextBackgroundBoundingBoxStrike[2]/255.0;
      fColor[3] = m_ColorTextBackgroundBoundingBoxStrike[3]/255.0;
   }
   else
   {
      fColor[0] = m_ColorFill[0]/255.0;
      fColor[1] = m_ColorFill[1]/255.0;
      fColor[2] = m_ColorFill[2]/255.0;
      fColor[3] = m_ColorFill[3]/255.0;
   }
   if ( (fColor[3] < 0.1) || (fColor[3] >= 1.0) )
      fColor[3] = 1.0;
   cairo_set_source_rgba(m_pCairoCtx, fColor[0], fColor[1], fColor[2], fColor[3]);
   cairo_move_to (m_pCairoCtx, xPos * m_iRenderWidth, yPos * m_iRenderHeight + pFont->baseLine);
   cairo_show_text (m_pCairoCtx, szText);
   
   return;

   while ( *szText )
   {
      float fWidthCh = _get_raw_char_width(pFont, *szText);
      if ( (fWidthCh < 0.0001) || ( (*szText) < pFont->charIdFirst || (*szText) > pFont->charIdLast ) )
      {
         szText++;
         continue;
      }
      if ( xPos < 0 )
      {
         xPos += fWidthCh * fScale;
         szText++;
         continue;
      }
      if ( xPos + fWidthCh * fScale >= 1.0 )
         break;

      int xImg = pFont->chars[(*szText)-pFont->charIdFirst].imgXOffset;
      int yImg = pFont->chars[(*szText)-pFont->charIdFirst].imgYOffset;
      int wImg = pFont->chars[(*szText)-pFont->charIdFirst].width;
      int hImg = pFont->chars[(*szText)-pFont->charIdFirst].height;
      
      //fbg_imageDrawAlpha(m_pFBG, (struct _fbg_img*) pFont->pImageObject, xPos * m_iRenderWidth, yPos * m_iRenderHeight, wImg*fScale, hImg*fScale, xImg, yImg, wImg, hImg);
      _bltFontChar(xPos * m_iRenderWidth, yPos * m_iRenderHeight, xImg, yImg, wImg, hImg, pFont);
      xPos += fWidthCh * fScale;
      szText++;
   }
}

void RenderEngineCairo::_bltFontChar(int iDestX, int iDestY, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, RenderEngineRawFont* pFont)
{
   if ( NULL == pFont )
      return;
   if ( (iDestX < 0) || (iDestY < 0) || (iDestX+iSrcWidth >= m_iRenderWidth) || (iDestY+iSrcHeight >= m_iRenderHeight) )
      return;

   type_drm_buffer* pOutputBufferInfo = ruby_drm_core_get_back_draw_buffer();
   u8* pSrcImageData = cairo_image_surface_get_data((cairo_surface_t*)pFont->pImageObject);
   int iSrcImageStride = cairo_image_surface_get_stride((cairo_surface_t*)pFont->pImageObject);

   for( int y=0; y<iSrcHeight; y++ )
   {
      u8* pDestLine = (u8*)&(pOutputBufferInfo->pData[(iDestY+y)*pOutputBufferInfo->uStride]);
      pDestLine += 4*iDestX;

      u8* pSrcLine = pSrcImageData + ((iSrcY +y)* iSrcImageStride);
      pSrcLine += 4 * iSrcX;

      for( int x=0; x<iSrcWidth; x++ )
      {
          u8 uAlpha = *(pSrcLine+3);
          *pDestLine = ((*(pSrcLine)) * uAlpha + (*(pDestLine)) * (255-uAlpha))/256;
          pDestLine++;
          pSrcLine++;
          *pDestLine = ((*(pSrcLine)) * uAlpha + (*(pDestLine)) * (255-uAlpha))/256;
          pDestLine++;
          pSrcLine++;
          *pDestLine = ((*(pSrcLine)) * uAlpha + (*(pDestLine)) * (255-uAlpha))/256;
          pDestLine++;
          pSrcLine++;
          pDestLine++;
          pSrcLine++;
      }
   }
}
