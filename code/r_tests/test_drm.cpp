#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "../base/base.h"
#include "../base/hardware.h"

#if defined (HW_PLATFORM_RASPBERRY)
#include "../renderer/render_engine_raw.h"
#endif
#if defined (HW_PLATFORM_RADXA_ZERO3)
#include "../renderer/drm_core.h"
#include "../renderer/render_engine_cairo.h"
#endif

bool g_bQuit = false;
int g_iMode = 0;

void handle_sigint(int sig) 
{ 
   log_line("Caught signal to stop: %d\n", sig);
   g_bQuit = true;
} 
  

int main(int argc, char *argv[])
{
   
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   g_iMode = 0;
   if ( argc > 1 )
   {
      if (0 == strcmp(argv[1], "1" ) )
         g_iMode = 1;
      if (0 == strcmp(argv[1], "2" ) )
         g_iMode = 2;
   }

   if ( 0 == g_iMode )
      log_init("TestRender0"); 
   else if ( 1 == g_iMode )
      log_init("TestRender1"); 
   else if ( 2 == g_iMode )
      log_init("TestRender2");
   else
      log_init("TestRenderX");
   log_enable_stdout();

   log_line("Test Render UI (mode %d)", g_iMode);

   #if defined (HW_PLATFORM_RADXA_ZERO3)
   if ( 0 == g_iMode )
      ruby_drm_core_init(0, DRM_FORMAT_ARGB8888, 1920, 1080, 60);
   else if ( 1 == g_iMode )
      ruby_drm_core_init(0, DRM_FORMAT_ARGB8888, 1920, 1080, 60);
   else
      ruby_drm_core_init(1, DRM_FORMAT_ARGB8888, 1920, 1080, 60);
   #endif

   ruby_drm_core_set_plane_properties_and_buffer(ruby_drm_core_get_main_draw_buffer_id());
   
   RenderEngine* g_pRenderEngine = render_init_engine();

   //double pColorBg[4] = {255,0,0,1.0};
   double pColorFg[4] = {255,255,0,1.0};

   //cairo_surface_t *image = cairo_image_surface_create_from_png ("res/ruby_bg4.png");
   u32 uImg = g_pRenderEngine->loadImage("res/ruby_bg4.png");
   u32 uIcon = g_pRenderEngine->loadIcon("res/icon_v_plane.png");
   int iFont = g_pRenderEngine->loadRawFont("res/font_ariobold_32.dsc");

   u32 uLastTimeFPS = 0;
   u32 uFPS = 0;

   float fTmpX = 0.4;
   float fTmp1 = 0.1;
   while ( ! g_bQuit )
   {
      g_pRenderEngine->startFrame();

      if ( g_iMode == 1 )
      {
         g_pRenderEngine->setColors(pColorFg);
         g_pRenderEngine->drawCircle(fTmp1, fTmp1, 0.05);
         g_pRenderEngine->setFill(50,50,255,0.5);
         g_pRenderEngine->fillCircle(fTmp1, fTmp1, 0.08);

         g_pRenderEngine->setFill(0,0,0,1);
         g_pRenderEngine->fillCircle(fTmp1, fTmp1, 0.03);

         g_pRenderEngine->setColors(pColorFg);
         g_pRenderEngine->drawText(0.05, 0.9, (u32)iFont, "Text2");
      }
      else if ( g_iMode == 2 )
      {
         g_pRenderEngine->setColors(pColorFg);
         g_pRenderEngine->drawCircle(fTmp1, fTmp1, 0.05);
         g_pRenderEngine->setFill(255,50,50,0.5);
         g_pRenderEngine->fillCircle(fTmp1, fTmp1, 0.08);

         g_pRenderEngine->setFill(0,0,0,1);
         g_pRenderEngine->fillCircle(fTmp1, fTmp1, 0.03);

         g_pRenderEngine->setColors(pColorFg);
         g_pRenderEngine->drawText(0.05, 0.9, (u32)iFont, "Text2");
      }
      else
      {
         g_pRenderEngine->drawImage(0,0,1.0,1.0, uImg);

         
         g_pRenderEngine->setColors(pColorFg);
         g_pRenderEngine->drawRoundRect(0.1, 0.1, 0.5, 0.5, 0.1);
         g_pRenderEngine->setStroke(255,0,0,1.0);
         g_pRenderEngine->setFill(0,0,0,0);
         g_pRenderEngine->drawRoundRect(0.2, 0.2, 0.5, 0.5, 0.1);

         g_pRenderEngine->setColors(pColorFg);
         g_pRenderEngine->drawLine(0.4,0.1, 0.5, 0.8);
         g_pRenderEngine->drawTriangle(0.2,0.2, 0.22, 0.3, 0.3, 0.35);     
         g_pRenderEngine->fillTriangle(0.82,0.82, 0.92, 0.53, 0.83, 0.95);     
  
         g_pRenderEngine->setColors(pColorFg);
         g_pRenderEngine->drawText(0.05, 0.8, (u32)iFont, "Text1");

         g_pRenderEngine->drawIcon(0.5, 0.2, 0.2, 0.2, uIcon);
         
         g_pRenderEngine->bltIcon(fTmpX, 0.5, 20, 20, 60, 60, uIcon);
      }

      g_pRenderEngine->endFrame();
      
      //hardware_sleep_ms(900);
      //hardware_sleep_ms(900);
      uFPS++;
      if ( get_current_timestamp_ms() > uLastTimeFPS+1000 )
      {
         log_line("DEBUG FPS: %u", uFPS);
         uLastTimeFPS = get_current_timestamp_ms();
         uFPS = 0;
      }

      fTmpX += 0.01;
      if ( fTmpX > 0.7 )
         fTmpX = 0.4;

      fTmp1 += 0.002;
      if ( fTmp1 > 0.75 )
         fTmp1 = 0.1;
   }

   render_free_engine();

   #if defined (HW_PLATFORM_RADXA_ZERO3)
   ruby_drm_core_uninit();
   #endif


   return (0);
}

