// Licence: you can copy, edit, change or do whatever you wish with the code in this file

#include "ruby_plugins_common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>


void rotatePoints(RenderEngineUI* pEngine, float *px, float *py, float angle, int points, float center_x, float center_y, float fScale)
{
   if ( NULL == pEngine || NULL == px || NULL == py )
      return;

   double cosAngle = cos(angle * 0.017453292519);
   double sinAngle = sin(angle * 0.017453292519);

    float tmp_x = 0;
    float tmp_y = 0;
    for( int i=0; i<points; i++ )
    {
       px[i] = (px[i] - center_x) * pEngine->getAspectRatio();
       py[i] = (py[i] - center_y);
       tmp_x = center_x + (px[i]*cosAngle - py[i]*sinAngle)*fScale/pEngine->getAspectRatio();
       tmp_y = center_y + (px[i]*sinAngle + py[i]*cosAngle)*fScale;
       px[i] = tmp_x;
       py[i] = tmp_y;
    }
}


void rotate_point(RenderEngineUI* pEngine, float x, float y, float xCenter, float yCenter, float angle, float* px, float* py)
{
   if ( NULL == pEngine || NULL == px || NULL == py )
      return;

   float s = sin(angle*3.14159/180.0);
   float c = cos(angle*3.14159/180.0);

   x = (x-xCenter)*pEngine->getAspectRatio();
   y = (y-yCenter);
   *px = x * c - y * s;
   *py = x * s + y * c;

   *px /=pEngine->getAspectRatio();
   *px += xCenter;
   *py += yCenter;
}


float show_value_centered(RenderEngineUI* pEngine, float x, float y, const char* szValue, u32 fontId)
{
   if ( NULL == pEngine || NULL == szValue || 0 == szValue[0] )
      return 0.0;

   float w = pEngine->textWidth(fontId, szValue);
   pEngine->drawText(x-0.5*w,y, fontId, szValue);
   return w;
}

void draw_shadow(RenderEngineUI* pEngine, float xCenter, float yCenter, float fRadius)
{
   if ( NULL == pEngine )
      return;

   double color[4] = {0,0,0,0.07};

   pEngine->setFill(color[0], color[1], color[2], color[3]);
   pEngine->setStrokeSize(0.0);
   for( float i=1; i<5; i+=1 )
      pEngine->fillCircle(xCenter+i*pEngine->getPixelHeight()*0.3/pEngine->getAspectRatio(), yCenter+i*pEngine->getPixelHeight()*0.7, fRadius+3.0*pEngine->getPixelHeight());

   pEngine->setStroke(250,250,250,0.07);
   pEngine->setStrokeSize(2.0);
   pEngine->drawArc(xCenter, yCenter, fRadius-pEngine->getPixelHeight(), 110, 170);

   pEngine->setStroke(250,250,250,0.01);
   pEngine->setStrokeSize(2.0);
   pEngine->drawArc(xCenter, yCenter, fRadius-pEngine->getPixelHeight(), 115, 165);

   pEngine->setStroke(250,250,250,0.2);
   pEngine->setStrokeSize(2.0);
   pEngine->drawArc(xCenter, yCenter, fRadius-pEngine->getPixelHeight(), 125, 145);

   pEngine->setStroke(250,250,250,0.5);
   pEngine->setStrokeSize(2.0);
   pEngine->drawArc(xCenter, yCenter, fRadius-pEngine->getPixelHeight(), 133, 137);
}
