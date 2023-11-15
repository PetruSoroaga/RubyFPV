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
#include "popup_camera_params.h"
#include "../renderer/render_engine.h"
#include "colors.h"
#include "menu.h"
#include "shared_vars.h"
#include "timers.h"
#include "ruby_central.h"
#include "handle_commands.h"
#include "pairing.h"
#include "osd_common.h"

PopupCameraParams::PopupCameraParams()
:Popup("", 0.2, 0.2, 5)
{
   setBottomAlign(true);
   setMaxWidth(0.3);
   m_fFixedWidth = 0.2;
   m_bCentered = true;
   m_bInvalidated = true;
   m_nParamToAdjust = 0;
   m_bCanAdjust = false;
   setIconId(g_idIconCamera, get_Color_PopupText());
}

PopupCameraParams::~PopupCameraParams()
{
}

void PopupCameraParams::onShow()
{
   removeAllLines();
   if ( NULL == g_pCurrentModel || (!pairing_isStarted()) || (!(pairing_isReceiving() || pairing_wasReceiving())) )
   {
      m_fFixedWidth = 0.3;
      setTitle("Camera Adjustment");
      addLine("You must be connected to a vehicle to be able to adjust camera parameters.");
      Popup::onShow();
      log_line("Popup Camera Params: Show");
      m_bCanAdjust = false;
      return;
   }

   m_fFixedWidth = 0.2;
   m_bCanAdjust = true;
   m_nParamToAdjust = 0;
   Popup::onShow();
   invalidate();
   log_line("Popup Camera Params: Show");
}

void PopupCameraParams::Render()
{
   float height_text = g_pRenderEngine->textHeight(POPUP_FONT_SIZE*menu_getScaleMenus(), g_idFontMenuSmall);
   float x = m_RenderXPos+POPUP_MARGINS*menu_getScaleMenus()/g_pRenderEngine->getAspectRatio();
   float y = m_RenderYPos+0.8*POPUP_MARGINS*menu_getScaleMenus();
   float dx = m_fIconSize/g_pRenderEngine->getAspectRatio() + 0.01*menu_getScaleMenus();

   if ( m_bCanAdjust )
   {
      computeSize();
      m_RenderHeight = 4.0*height_text;
      m_RenderYPos = 0.84 - m_RenderHeight;
   }
   Popup::Render();

   if ( (! m_bCanAdjust) || NULL == g_pCurrentModel )
      return;

   char szBuff[32];
   char szText[64];
   int value = 0;
   int value_min = 0;
   int value_max = 100;

   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   camera_profile_parameters_t* pCamProfile = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile]);

   if ( m_nParamToAdjust == 0 )
   {
      value = pCamProfile->brightness;
      strcpy(szText, "Brightness");
   }
   if ( m_nParamToAdjust == 1 )
   {
      value = pCamProfile->contrast;
      strcpy(szText, "Contrast");
   }
   if ( m_nParamToAdjust == 2 )
   {
      value = pCamProfile->saturation-100;
      value_min = -100;
      value_max = 100;
      strcpy(szText, "Saturation");
   }
   if ( m_nParamToAdjust == 3 )
   {
      value = pCamProfile->sharpness-100;
      value_min = -100;
      value_max = 100;
      strcpy(szText, "Sharpness");
   }
   snprintf(szBuff, sizeof(szBuff), "%d", value);
   strlcat(szText, ":", sizeof(szText));
   //strlcat(szText, szBuff, sizeof(szText));

   g_pRenderEngine->setColors(get_Color_PopupText());   
   g_pRenderEngine->drawText(x+dx, y, height_text*POPUP_TITLE_SCALE, g_idFontMenuSmall, szText);
   float fw = g_pRenderEngine->textWidth(height_text, g_idFontMenuSmall, szText);
   g_pRenderEngine->drawText(x+dx+fw + 0.02, y-0.2*height_text, height_text*POPUP_TITLE_SCALE, g_idFontMenu, szBuff);
   y += height_text*1.4;

   float padding = MENU_SELECTION_PADDING*menu_getScaleMenus();
   float paddingH = padding/g_pRenderEngine->getAspectRatio();

   float sliderHeight = 0.4 * height_text;
   float sliderWidth = m_RenderWidth - m_fIconSize/g_pRenderEngine->getAspectRatio() - 0.005*menu_getScaleMenus() - 2*POPUP_MARGINS*menu_getScaleMenus();
   float valueWidth = 0.03*menu_getScaleMenus();
   float xPosSlider = x + dx;
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(0);

   
   g_pRenderEngine->setColors(get_Color_MenuText(), 0.2);
   g_pRenderEngine->setStrokeSize(1);

   g_pRenderEngine->drawRoundRect(xPosSlider, y+height_text-1.3*sliderHeight, sliderWidth, sliderHeight, 0.002*menu_getScaleMenus());

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(1);

   float yBottom = y + height_text - 0.9*sliderHeight;
   g_pRenderEngine->drawLine(xPosSlider, yBottom, xPosSlider, yBottom-1.6*sliderHeight);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth, yBottom, xPosSlider+sliderWidth, yBottom-1.6*sliderHeight);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth/2, yBottom, xPosSlider+sliderWidth/2, yBottom-1.6*sliderHeight);

   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.1, yBottom, xPosSlider+sliderWidth*0.1, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.2, yBottom, xPosSlider+sliderWidth*0.2, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.3, yBottom, xPosSlider+sliderWidth*0.3, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.4, yBottom, xPosSlider+sliderWidth*0.4, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.6, yBottom, xPosSlider+sliderWidth*0.6, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.7, yBottom, xPosSlider+sliderWidth*0.7, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.8, yBottom, xPosSlider+sliderWidth*0.8, yBottom-sliderHeight*0.8);
   g_pRenderEngine->drawLine(xPosSlider+sliderWidth*0.9, yBottom, xPosSlider+sliderWidth*0.9, yBottom-sliderHeight*0.8);

   float size = sliderHeight*1.8;
   xPosSlider = xPosSlider + (float)(sliderWidth-size)*(value-value_min)/(float)(value_max-value_min);

   g_pRenderEngine->drawRoundRect(xPosSlider, yBottom-size*0.6, size, size, 0.002*menu_getScaleMenus());

   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStroke(get_Color_MenuBorder());
   g_pRenderEngine->setStrokeSize(0);
}


void PopupCameraParams::handleRotaryEvents(bool bCW, bool bCCW, bool bFastCW, bool bFastCCW, bool bSelect, bool bCancel)
{

   if ( bCW || bCCW || bFastCW || bFastCCW || bSelect || bCancel )
   {
      m_fTimeoutSeconds = (g_TimeNow - m_StartTime)/1000 + 5;
   }   

   if ( bCancel )
   {
      popups_remove(this);
      return;
   }

   if ( bSelect )
   {
       m_nParamToAdjust++;
       if (  m_nParamToAdjust > 3 )
          m_nParamToAdjust = 0;
   }

   bool bHasMoved = false;
   bool bToSend = false;

   if ( bCW || bCCW || bFastCW || bFastCCW )
      bHasMoved = true;

   if ( ! bHasMoved )
      return;

   type_camera_parameters cparams;
   memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;

   if ( m_nParamToAdjust == 0 )
   {
      if ( (bCCW || bFastCCW) && cparams.profiles[iProfile].brightness > 0 )
         cparams.profiles[iProfile].brightness--;
      if ( bFastCCW && cparams.profiles[iProfile].brightness > 0 )
         cparams.profiles[iProfile].brightness--;

      if ( (bCW || bFastCW) && cparams.profiles[iProfile].brightness < 100 )
         cparams.profiles[iProfile].brightness++;
      if ( bFastCW && cparams.profiles[iProfile].brightness < 100 )
         cparams.profiles[iProfile].brightness++;

      if ( cparams.profiles[iProfile].brightness != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile].brightness )
         bToSend = true;
   }

   if ( m_nParamToAdjust == 1 )
   {
      if ( (bCCW || bFastCCW) && cparams.profiles[iProfile].contrast > 0 )
         cparams.profiles[iProfile].contrast--;
      if ( bFastCCW && cparams.profiles[iProfile].contrast > 0 )
         cparams.profiles[iProfile].contrast--;

      if ( (bCW || bFastCW) && cparams.profiles[iProfile].contrast < 100 )
         cparams.profiles[iProfile].contrast++;
      if ( bFastCW && cparams.profiles[iProfile].contrast < 100 )
         cparams.profiles[iProfile].contrast++;

      if ( cparams.profiles[iProfile].contrast != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile].contrast )
         bToSend = true;
   }

   if ( m_nParamToAdjust == 2 )
   {
      if ( (bCCW || bFastCCW) && cparams.profiles[iProfile].saturation > 0 )
         cparams.profiles[iProfile].saturation--;
      if ( bFastCCW && cparams.profiles[iProfile].saturation > 0 )
         cparams.profiles[iProfile].saturation--;

      if ( (bCW || bFastCW) && cparams.profiles[iProfile].saturation < 200 )
         cparams.profiles[iProfile].saturation++;
      if ( bFastCW && cparams.profiles[iProfile].saturation < 200 )
         cparams.profiles[iProfile].saturation++;

      if ( cparams.profiles[iProfile].saturation != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile].saturation )
         bToSend = true;
   }

   if ( m_nParamToAdjust == 3 )
   {
      if ( (bCCW || bFastCCW) && cparams.profiles[iProfile].sharpness > 0 )
         cparams.profiles[iProfile].sharpness--;
      if ( bFastCCW && cparams.profiles[iProfile].sharpness > 0 )
         cparams.profiles[iProfile].sharpness--;

      if ( (bCW || bFastCW) && cparams.profiles[iProfile].sharpness < 200 )
         cparams.profiles[iProfile].sharpness++;
      if ( bFastCW && cparams.profiles[iProfile].sharpness < 200 )
         cparams.profiles[iProfile].sharpness++;

      if ( cparams.profiles[iProfile].sharpness != g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile].sharpness )
         bToSend = true;
   }


   if ( bToSend )
   {
      memcpy( &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), &cparams, sizeof(type_camera_parameters));
      handle_commands_send_single_oneway_command(2, COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters));
   }
}

