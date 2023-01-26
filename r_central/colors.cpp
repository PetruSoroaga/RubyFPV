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

#include "colors.h"
#include "../base/base.h"
#include "../base/config.h"

double COLOR_DEV[4] = { 170,220,255,0.9 };

double COLOR_POPUP_BG[4] = { 0,0,0,0.76 };
double COLOR_POPUP_BORDER[4] = { 250,240,220,0.4 };
double COLOR_POPUP_TEXT[4] = { 255,250,230,0.85 };

double COLOR_POPUP_INV_BG[4] = { 180,180,180, 0.7 };
double COLOR_POPUP_INV_BORDER[4] = { 100,100,100,0.7 };
double COLOR_POPUP_INV_TEXT[4] = { 30,30,30,0.8 };

double COLOR_MENU_BG[4] = { 20,20,20, 0.96 };
double COLOR_MENU_BORDER[4] = { 255,244,225,0.4 };
double COLOR_MENU_TEXT[4] = { 255,244,225,0.84 };

double COLOR_MENU_INV_BG[4] = { 185,185,180, 0.94 };
double COLOR_MENU_INV_BORDER[4] = { 60,60,60,0.7 };
double COLOR_MENU_INV_TEXT[4] = { 20,20,20,0.96 };

double COLOR_MENU_BG_SELECTED[4] = { 255,244,225,0.6 };
double COLOR_MENU_TEXT_SELECTED[4] = { 0,0,0,0.9 };
double COLOR_MENU_TEXT_DISABLED[4] = { 180,180,180,0.3 };

double COLOR_MENU_INV_BG_SELECTED[4] = { 80,80,80,0.8 };
double COLOR_MENU_INV_TEXT_SELECTED[4] = { 255,244,225, 0.9 };
double COLOR_MENU_INV_TEXT_DISABLED[4] = { 80,80,80,0.5 };

double COLOR_ICON_NORMAL[4] = { 255,255,255, 0.95 };
double COLOR_ICON_WARNING[4] = { 255,255,0, 0.95 };
double COLOR_ICON_ERROR[4] = { 255,70,90, 0.98 };
double COLOR_ICON_SUCCES[4] = { 100,150,100, 0.95 };

double COLOR_OSD_BACKGROUND[4] = {0,0,0, 0.7};
double COLOR_OSD_TEXT[4] = { 255,250,230,1.0 };
double COLOR_OSD_TEXT_OUTLINE[4] = { 0,0,0,0.9 };
double COLOR_OSD_HIGH_WARNING[4] = {255,0,0,0.9};

double COLOR_OSD_INV_BACKGROUND[4] = {220,220,220, 0.7};
double COLOR_OSD_INV_TEXT[4] = { 3,3,3,0.98 };
double COLOR_OSD_INV_TEXT_OUTLINE[4] = { 55,55,55,0.2 };
double COLOR_OSD_INV_HIGH_WARNING[4] = {255,0,0,0.9};

double* get_Color_Dev()
{
   return COLOR_DEV;
}

double* get_Color_OSDBackground()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_OSD_TEXT;
   else
      return COLOR_OSD_BACKGROUND;
}

double* get_Color_OSDText()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_OSD_INV_TEXT;
   else
      return COLOR_OSD_TEXT;
}

double* get_Color_OSDTextOutline()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_OSD_INV_TEXT_OUTLINE;
   else
      return COLOR_OSD_TEXT_OUTLINE;

}

double* get_Color_OSDWarning()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_OSD_INV_HIGH_WARNING;
   else
      return COLOR_OSD_HIGH_WARNING;
}

double* get_Color_PopupBg()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_POPUP_INV_BG;
   else
      return COLOR_POPUP_BG;
}

double* get_Color_PopupBorder()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_POPUP_INV_BORDER;
   else
      return COLOR_POPUP_BORDER;
}

double* get_Color_PopupText()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_POPUP_INV_TEXT;
   else
      return COLOR_POPUP_TEXT;
}

double* get_Color_MenuBg()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_BG;
   else
      return COLOR_MENU_BG;
}
double* get_Color_MenuBorder()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_BORDER;
   else
      return COLOR_MENU_BORDER;
}
double* get_Color_MenuText()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_TEXT;
   else
      return COLOR_MENU_TEXT;
}

double* get_Color_ItemSelectedBg()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_BG_SELECTED;
   else
      return COLOR_MENU_BG_SELECTED;
}
double* get_Color_ItemSelectedText()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_TEXT_SELECTED;
   else
      return COLOR_MENU_TEXT_SELECTED;
}
double* get_Color_ItemDisabledText()
{
   Preferences* p = get_Preferences();
   if ( p->iInvertColorsOSD )
      return COLOR_MENU_INV_TEXT_DISABLED;
   else
      return COLOR_MENU_TEXT_DISABLED;
}

double* get_Color_IconNormal() { return COLOR_ICON_NORMAL; }
double* get_Color_IconWarning() { return COLOR_ICON_WARNING; }
double* get_Color_IconError() { return COLOR_ICON_ERROR; }
double* get_Color_IconSucces() { return COLOR_ICON_SUCCES; }


void set_Color_OSDText(float r, float g, float b, float a)
{
   COLOR_OSD_TEXT[0] = r;
   COLOR_OSD_TEXT[1] = g;
   COLOR_OSD_TEXT[2] = b;
   COLOR_OSD_TEXT[3] = a;
}

void set_Color_OSDOutline(float r, float g, float b, float a)
{
   COLOR_OSD_TEXT_OUTLINE[0] = r;
   COLOR_OSD_TEXT_OUTLINE[1] = g;
   COLOR_OSD_TEXT_OUTLINE[2] = b;
   COLOR_OSD_TEXT_OUTLINE[3] = a;
}